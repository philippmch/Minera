// Token-based ETConformance precision scores every distinct proper prefix.
// Logs of long, mostly distinct traces have a number of prefix events that is
// quadratic in the trace length, so the prefixes must not be materialised and
// replayed one by one. These checks pin (1) that the trie walk scores exactly
// what replaying each materialised prefix on its own scores, fit and unfit,
// forward and backwards, and (2) that a long-trace log stays within a bounded
// time and memory budget.
#include "minera/conformance.hpp"
#include "minera/evaluation.hpp"
#include "minera/petri_net.hpp"
#include "minera/process_tree.hpp"
#include "minera/token_integer.hpp"

#include "../evaluation_internal.hpp"
#include "../petri_replay_internal.hpp"

#include <sys/resource.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace {

using minera::ProcessTreeNode;
using minera::ProcessTreeOperator;

ProcessTreeNode node(ProcessTreeOperator op, std::vector<ProcessTreeNode> children) {
  return ProcessTreeNode{op, 0, std::move(children)};
}
ProcessTreeNode leaf(std::size_t activity) { return ProcessTreeNode::leaf(activity); }

// A -> loop(xor(B, C, tau), D) -> (E || F): silent routing, concurrency and a
// loop, so prefixes reach many different markings.
minera::PetriNet mixed_net() {
  return minera::to_petri_net(minera::ProcessTree(
      {"A", "B", "C", "D", "E", "F"},
      node(ProcessTreeOperator::kSequence,
           {leaf(0),
            node(ProcessTreeOperator::kLoop,
                 {node(ProcessTreeOperator::kExclusive,
                       {leaf(1), leaf(2), ProcessTreeNode::tau()}),
                  leaf(3)}),
            node(ProcessTreeOperator::kParallel, {leaf(4), leaf(5)})})));
}

// A flower over ten activities: every trace fits and every prefix is distinct.
minera::PetriNet flower_net() {
  std::vector<std::string> activities;
  std::vector<ProcessTreeNode> choices;
  for (std::size_t i = 0; i < 10; ++i) {
    activities.push_back(std::string(1, static_cast<char>('A' + i)));
    choices.push_back(leaf(i));
  }
  return minera::to_petri_net(minera::ProcessTree(
      activities, node(ProcessTreeOperator::kLoop,
                       {node(ProcessTreeOperator::kExclusive, std::move(choices)),
                        ProcessTreeNode::tau()})));
}

minera::EventLog random_log(std::size_t traces, std::size_t min_length, std::size_t max_length,
                            const std::vector<std::string>& alphabet, std::uint32_t seed) {
  std::mt19937 random(seed);
  minera::EventLog log;
  for (std::size_t t = 0; t < traces; ++t) {
    minera::Trace trace;
    trace.case_id = "c" + std::to_string(t);
    const auto length = min_length + random() % (max_length - min_length + 1);
    for (std::size_t e = 0; e < length; ++e) {
      minera::Event event;
      event.case_id = trace.case_id;
      event.activity = alphabet[random() % alphabet.size()];
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

minera::TokenReplayOptions precision_options(minera::TokenReplayVariant variant) {
  minera::TokenReplayOptions options;
  options.variant = variant;
  options.stop_immediately_unfit = true;
  options.consider_remaining_in_fitness = false;
  options.try_to_reach_final_marking_through_hidden = false;
  options.consider_activities_not_in_model_in_fitness = false;
  options.activity_attribute = "concept:name";
  return options;
}

// The definition the walk must reproduce: every distinct proper prefix is
// materialised as its own trace and replayed alone.
double materialised_precision(const minera::EventLog& log, const minera::PetriNet& net,
                              const minera::TokenReplayOptions& options) {
  struct Prefix {
    std::uint64_t frequency = 0;
    std::set<std::string> observed;
  };
  std::map<std::vector<std::string>, std::size_t> index;
  std::vector<Prefix> prefixes;
  minera::EventLog prefix_log;
  Prefix initial;
  for (const auto& trace : log.traces) {
    initial.frequency += 1;
    if (!trace.events.empty()) initial.observed.insert(trace.events.front().activity);
    std::vector<std::string> key;
    for (std::size_t i = 0; i + 1 < trace.events.size(); ++i) {
      key.push_back(trace.events[i].activity);
      const auto [it, inserted] = index.emplace(key, prefixes.size());
      if (inserted) {
        prefixes.emplace_back();
        minera::Trace sample;
        sample.case_id = "prefix-" + std::to_string(it->second);
        for (std::size_t j = 0; j <= i; ++j) {
          auto event = trace.events[j];
          event.case_id = sample.case_id;
          event.input_order = j;
          sample.events.push_back(std::move(event));
        }
        prefix_log.traces.push_back(std::move(sample));
      }
      prefixes[it->second].frequency += 1;
      prefixes[it->second].observed.insert(trace.events[i + 1].activity);
    }
  }
  const auto rows = options.variant == minera::TokenReplayVariant::kForward
      ? minera::replay_token_result(prefix_log, net, options).traces
      : minera::replay_token_traces(prefix_log, net, options);
  assert(rows.size() == prefixes.size());
  minera::TokenInteger enabled_total = 0, escaping_total = 0;
  const auto score = [&](const Prefix& prefix, const std::set<std::string>& labels) {
    enabled_total += minera::TokenInteger(prefix.frequency) * labels.size();
    for (const auto& label : labels)
      if (!prefix.observed.contains(label)) escaping_total += prefix.frequency;
  };
  for (std::size_t i = 0; i < rows.size(); ++i) {
    if (!rows[i].trace_is_fit) continue;
    std::set<std::string> labels;
    for (const auto transition : rows[i].enabled_transitions)
      if (const auto& activity = net.transitions().at(transition).activity)
        labels.insert(net.activities().at(*activity));
    score(prefixes[i], labels);
  }
  std::set<std::string> labels;
  for (const auto activity : minera::detail::initial_enabled_token_activities(net, options))
    labels.insert(net.activities().at(activity));
  score(initial, labels);
  return enabled_total == 0 ? 1.0 : 1.0 - minera::token_integer_ratio(escaping_total, enabled_total);
}

void walk_matches_materialised_prefixes() {
  const auto net = mixed_net();
  // "X" is absent from the model; the other letters often arrive out of
  // order, so many prefixes stop early and share their stopped state.
  const std::vector<std::string> alphabet{"A", "A", "B", "C", "D", "E", "F", "X"};
  std::size_t checked = 0, scored = 0;
  for (std::uint32_t seed = 1; seed <= 40; ++seed) {
    auto log = random_log(30, 1, 12, alphabet, seed);
    // Fitting traces too, so most prefixes replay fully.
    for (std::size_t t = 0; t < log.traces.size(); t += 3) {
      auto& events = log.traces[t].events;
      const std::vector<std::string> fit{"A", "B", "D", "C", "D", "F", "E"};
      events.resize(1 + seed % fit.size());
      for (std::size_t e = 0; e < events.size(); ++e) events[e].activity = fit[e];
    }
    for (const auto variant :
         {minera::TokenReplayVariant::kForward, minera::TokenReplayVariant::kBackwards}) {
      const auto options = precision_options(variant);
      // Backwards replay throws for a prefix it cannot score; both paths
      // must then throw the same error.
      const auto outcome = [&](auto&& compute) -> std::string {
        try {
          char text[32];
          std::snprintf(text, sizeof text, "%.17g", compute());
          return text;
        } catch (const std::exception& error) {
          return std::string("error: ") + error.what();
        }
      };
      const auto expected = outcome([&] { return materialised_precision(log, net, options); });
      const auto actual = outcome(
          [&] { return minera::detail::token_etconformance_precision(log, net, options); });
      if (actual != expected) {
        std::fprintf(stderr, "seed %u variant %d: walk %s, materialised %s\n", seed,
                     static_cast<int>(variant), actual.c_str(), expected.c_str());
        assert(false);
      }
      if (expected.rfind("error: ", 0) != 0) ++scored;
      ++checked;
    }
  }
  assert(checked == 80);
  // Enough prefix events (about 216k) that backwards replay runs in several
  // materialised chunks.
  {
    const auto flower = flower_net();
    std::vector<std::string> letters;
    for (std::size_t i = 0; i < 10; ++i) letters.push_back(std::string(1, static_cast<char>('A' + i)));
    const auto log = random_log(30, 120, 120, letters, 3);
    for (const auto variant :
         {minera::TokenReplayVariant::kForward, minera::TokenReplayVariant::kBackwards}) {
      const auto options = precision_options(variant);
      const double expected = materialised_precision(log, flower, options);
      const double actual = minera::detail::token_etconformance_precision(log, flower, options);
      if (actual != expected) {
        std::fprintf(stderr, "flower variant %d: walk %.17g, materialised %.17g\n",
                     static_cast<int>(variant), actual, expected);
        assert(false);
      }
    }
  }
  // Most cases must produce a value, not only matching errors.
  std::fprintf(stderr, "materialised comparison: %zu of %zu cases scored\n", scored, checked);
  assert(scored >= 40);
}

long peak_rss_kib() {
  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
  return usage.ru_maxrss;
}

// 250 traces of 400 events over a flower: about 100k distinct prefixes holding
// about 20M prefix events. Materialising them takes several gigabytes; the
// walk needs a few megabytes and a few seconds even in a debug build.
void long_traces_stay_bounded() {
  const auto net = flower_net();
  std::vector<std::string> alphabet;
  for (std::size_t i = 0; i < 10; ++i) alphabet.push_back(std::string(1, static_cast<char>('A' + i)));
  const auto log = random_log(250, 400, 400, alphabet, 7);
  const long before = peak_rss_kib();
  const auto start = std::chrono::steady_clock::now();
  const auto result =
      minera::evaluate_precision(log, net, minera::PrecisionVariant::ETCONFORMANCE_TOKEN);
  const auto seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  const long grown_mib = (peak_rss_kib() - before) / 1024;
  std::fprintf(stderr, "long-trace token precision: %.2f s, peak RSS growth %ld MiB\n", seconds,
               grown_mib);
  assert(result.computed);
  // Every label is enabled after every fitting prefix, and the random log
  // leaves most of them unobserved.
  assert(result.precision > 0.0 && result.precision < 0.5);
  assert(grown_mib < 256);
  assert(seconds < 60.0);
}

}  // namespace

int main() {
  // First, so the materialised comparisons do not raise the peak RSS it reads.
  long_traces_stay_bounded();
  walk_matches_materialised_prefixes();
  return 0;
}
