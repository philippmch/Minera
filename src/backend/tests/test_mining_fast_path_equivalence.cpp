// Pins the exact output of the mining stages that have fast paths: Declare
// discovery and conformance, POWL discovery (every variant), log skeleton
// discovery and conformance, model footprints, final-marking reachability,
// token replay (hidden-place routing) and model decoration (end-of-trace
// silent routing). The digests were recorded from the implementations before
// the fast paths existed, on deterministic logs of several shapes, so any
// change in the serialized results fails here.
//
// Run with --print to list the current digests, or --dump DIR to write the
// serialized outputs to DIR for comparison.
#include "minera/alpha_miner.hpp"
#include "minera/conformance.hpp"
#include "minera/declare.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/json_export.hpp"
#include "minera/log_skeleton.hpp"
#include "minera/model_conversion_workbench_json.hpp"
#include "minera/model_decoration.hpp"
#include "minera/petri_net.hpp"
#include "minera/powl_conversion.hpp"
#include "minera/powl_discovery.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace minera;

// Small deterministic generator (no std::distribution, whose output differs
// between standard libraries).
struct Rng {
  std::uint64_t state;
  explicit Rng(std::uint64_t seed) : state(seed * 0x9E3779B97F4A7C15ull + 1) {}
  std::uint64_t next() {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
  }
  std::size_t below(std::size_t n) { return static_cast<std::size_t>(next() % n); }
  bool chance(unsigned percent) { return below(100) < percent; }
};

std::string label(std::size_t i) {
  char buffer[16];
  std::snprintf(buffer, sizeof buffer, "A%03zu", i);
  return buffer;
}

std::string stamp(std::uint64_t minutes) {
  const std::uint64_t day = minutes / 1440, hour = minutes / 60 % 24, minute = minutes % 60;
  char buffer[40];
  std::snprintf(buffer, sizeof buffer, "2024-%02llu-%02lluT%02llu:%02llu:00",
                static_cast<unsigned long long>(day / 28 % 12 + 1),
                static_cast<unsigned long long>(day % 28 + 1),
                static_cast<unsigned long long>(hour),
                static_cast<unsigned long long>(minute));
  return buffer;
}

EventLog build(const std::vector<std::vector<std::size_t>>& traces, Rng& rng) {
  EventLog log;
  std::size_t order = 0;
  std::uint64_t clock = 0;
  for (std::size_t c = 0; c < traces.size(); ++c) {
    Trace trace;
    trace.case_id = "case" + std::to_string(c);
    trace.case_id_state = FirstClassAttributeState::kPresent;
    clock = c * 37;
    for (const auto a : traces[c]) {
      clock += 1 + rng.below(240);
      Event event{trace.case_id, label(a), stamp(clock), "R" + std::to_string(rng.below(4)),
                  order++};
      event.case_id_state = FirstClassAttributeState::kPresent;
      event.activity_state = FirstClassAttributeState::kPresent;
      event.timestamp_state = FirstClassAttributeState::kPresent;
      event.resource_state = FirstClassAttributeState::kPresent;
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

// A backbone with skips, swaps (concurrency) and loop-backs.
std::vector<std::size_t> structured(Rng& rng, std::size_t acts, std::size_t start,
                                    std::size_t span) {
  std::vector<std::size_t> t;
  std::size_t i = start;
  while (i < start + span && t.size() < 4 * span) {
    if (rng.chance(10)) { i += 2; continue; }
    if (rng.chance(12) && i + 1 < start + span) {
      t.push_back((i + 1) % acts);
      t.push_back(i % acts);
      i += 2;
      continue;
    }
    if (rng.chance(8) && i > start) --i;
    t.push_back(i % acts);
    ++i;
  }
  if (t.empty()) t.push_back(start % acts);
  return t;
}

struct NamedLog {
  std::string name;
  EventLog log;
};

std::vector<NamedLog> logs() {
  std::vector<NamedLog> result;
  {
    Rng rng(1);
    std::vector<std::vector<std::size_t>> t;
    for (int c = 0; c < 300; ++c) t.push_back(structured(rng, 20, 0, 20));
    result.push_back({"structured20", build(t, rng)});
  }
  {
    Rng rng(2);
    std::vector<std::vector<std::size_t>> t;
    for (int c = 0; c < 120; ++c) {
      std::vector<std::size_t> v;
      const std::size_t len = 2 + rng.below(9);
      for (std::size_t k = 0; k < len; ++k) v.push_back(rng.below(7));
      t.push_back(v);
    }
    result.push_back({"random7", build(t, rng)});
  }
  {
    Rng rng(3);
    std::vector<std::vector<std::size_t>> t;
    for (int c = 0; c < 6; ++c) {
      std::vector<std::size_t> v;
      for (int k = 0; k < 250; ++k) v.push_back(rng.chance(60) ? k % 5 : rng.below(5));
      t.push_back(v);
    }
    result.push_back({"long5", build(t, rng)});
  }
  {
    Rng rng(4);
    std::vector<std::vector<std::size_t>> t;
    for (int c = 0; c < 150; ++c) t.push_back(structured(rng, 60, rng.below(60), 12));
    result.push_back({"many60", build(t, rng)});
  }
  {
    Rng rng(5);
    std::vector<std::vector<std::size_t>> t;
    for (int c = 0; c < 80; ++c) {
      std::vector<std::size_t> v;
      const std::size_t len = 1 + rng.below(25);
      for (std::size_t k = 0; k < len; ++k) v.push_back(rng.chance(50) ? 0 : rng.below(4));
      t.push_back(v);
    }
    // Single-event and duplicate cases.
    t.push_back({1});
    t.push_back({1});
    t.push_back({3});
    t.push_back({0, 0, 0});
    result.push_back({"repeats4", build(t, rng)});
  }
  {
    Rng rng(6);
    std::vector<std::vector<std::size_t>> t;
    for (int c = 0; c < 40; ++c) {
      std::vector<std::size_t> v{0};
      // A parallel block of four activities in random order, optional tail.
      std::vector<std::size_t> block{1, 2, 3, 4};
      for (std::size_t k = block.size(); k > 1; --k) std::swap(block[k - 1], block[rng.below(k)]);
      v.insert(v.end(), block.begin(), block.end());
      if (rng.chance(50)) v.push_back(5);
      v.push_back(6);
      t.push_back(v);
    }
    result.push_back({"parallel", build(t, rng)});
  }
  return result;
}

std::string guarded(const std::function<std::string()>& body) {
  try {
    return body();
  } catch (const std::exception& error) {
    return std::string("error: ") + error.what();
  }
}

std::string declare_text(const DeclareModel& model) {
  std::ostringstream out;
  for (const auto& [family, rules] : model.rules) {
    out << family << ':';
    for (const auto& rule : rules)
      out << '[' << rule.first << ',' << rule.second << ',' << rule.binary << ','
          << rule.support << ',' << rule.confidence << ']';
    out << '\n';
  }
  return out.str();
}

std::uint64_t fnv(const std::string& text) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const unsigned char c : text) {
    hash ^= c;
    hash *= 1099511628211ull;
  }
  return hash;
}

// One serialized result per stage and log.
std::map<std::string, std::string> outputs() {
  std::map<std::string, std::string> result;
  for (const auto& [name, log] : logs()) {
    const auto put = [&](const std::string& stage, std::string text) {
      result[name + "/" + stage] = std::move(text);
    };
    const auto graph = discover_directly_follows_graph(log);

    // Declare: defaults, explicit thresholds, restricted templates/activities.
    const auto declare = discover_declare(log);
    put("declare", declare_text(declare));
    {
      DeclareDiscoveryOptions options;
      options.has_min_support_ratio = true;
      options.min_support_ratio = 0.05;
      options.has_min_confidence_ratio = true;
      options.min_confidence_ratio = 0.5;
      put("declare_thresholds", guarded([&] { return declare_text(discover_declare(log, options)); }));
      options.allowed_templates = std::vector<std::string>{
          "response", "precedence", "succession", "noncoexistence", "existence",
          "absence", "chainresponse", "nonchainsuccession"};
      put("declare_templates", guarded([&] { return declare_text(discover_declare(log, options)); }));
      DeclareDiscoveryOptions subset;
      subset.considered_activities = std::vector<std::string>{label(0), label(1), label(2), "ZZZ"};
      put("declare_subset", guarded([&] { return declare_text(discover_declare(log, subset)); }));
    }
    const auto declare_conformance = check_declare_conformance(log, declare);

    // Log skeleton with and without noise.
    const auto skeleton = discover_log_skeleton(log);
    const auto skeleton_conformance = check_log_skeleton_conformance(log, skeleton);
    {
      LogSkeletonOptions noisy;
      noisy.noise_threshold = 0.2;
      const auto model = discover_log_skeleton(log, noisy);
      const auto conformance = check_log_skeleton_conformance(log, model, noisy);
      // Positions 1-21 (tree .. powl_decoration) unset; 22-23 log skeleton.
      put("log_skeleton_noise", analysis_json(
          "x", log, graph, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
          nullptr, nullptr, nullptr, nullptr, nullptr, &model, &conformance));
    }

    // POWL, every variant (brute force only on small alphabets).
    for (const auto variant :
         {PowlDiscoveryVariant::kTree, PowlDiscoveryVariant::kMaximal,
          PowlDiscoveryVariant::kDynamicClustering,
          PowlDiscoveryVariant::kDynamicClusteringPlain, PowlDiscoveryVariant::kBruteForce}) {
      if (variant == PowlDiscoveryVariant::kBruteForce && graph.activities().size() > 7) continue;
      PowlDiscoveryOptions options;
      options.variant = variant;
      put("powl" + std::to_string(static_cast<int>(variant)), guarded([&] {
            return serialize_model_conversion_powl_json(discover_powl(log, options));
          }));
    }

    const auto tree = discover_process_tree(log);
    const auto inductive_net = to_petri_net(tree);
    const auto alpha_net = discover_petri_net(log);
    const auto heuristics = discover_heuristics_net(log);
    const auto heuristic_net = to_petri_net(heuristics);
    const auto powl = discover_powl(log);
    const auto powl_net = to_petri_net(powl);

    std::vector<ModelConformance> conformance;
    const std::pair<const char*, const PetriNet*> nets[] = {
        {"inductive", &inductive_net}, {"alpha", &alpha_net},
        {"heuristic", &heuristic_net}, {"powl", &powl_net}};
    for (const auto& [id, net] : nets) {
      ModelConformance model;
      model.miner = id;
      model.sound = final_marking_reachable(*net);
      model.token_replay = check_conformance(log, *net);
      model.footprints = check_footprints(log, *net);
      conformance.push_back(std::move(model));
      put(std::string("replay_traces_") + id, guarded([&] {
            return token_replay_traces_json(replay_token_traces(log, *net));
          }));
      put(std::string("replay_result_") + id, guarded([&] {
            return token_replay_result_json(replay_token_result(log, *net));
          }));
      put(std::string("replay_backwards_") + id, guarded([&] {
            TokenReplayOptions options;
            options.variant = TokenReplayVariant::kBackwards;
            const auto r = check_conformance(log, *net, options);
            std::ostringstream out;
            out.precision(17);
            out << r.fitness << ' ' << r.precision << ' ' << r.produced << ' ' << r.consumed
                << ' ' << r.missing << ' ' << r.remaining << ' ' << r.fitting_traces;
            return out.str();
          }));
    }
    const auto alpha_decoration = decorate_model(log, alpha_net);
    const auto inductive_decoration = decorate_model(log, inductive_net);
    const auto heuristic_decoration = decorate_model(log, heuristic_net);
    const auto powl_decoration = decorate_model(log, powl_net);

    put("analysis", analysis_json(
        name, log, graph, &tree, &alpha_net, &heuristics, &conformance, nullptr, nullptr,
        nullptr, nullptr, nullptr, &alpha_decoration, nullptr, &inductive_net,
        &inductive_decoration, &heuristic_net, &heuristic_decoration, nullptr, nullptr,
        nullptr, &powl, &powl_net, &powl_decoration, &skeleton, &skeleton_conformance,
        &declare, &declare_conformance));
  }
  return result;
}

struct Pin {
  const char* key;
  std::uint64_t hash;
  std::size_t size;
};

// Recorded from the reference (pre-fast-path) implementations.
const Pin kPins[] = {
#include "mining_fast_path_pins.inc"
    {nullptr, 0, 0}};

}  // namespace

int main(int argc, char** argv) {
  const auto current = outputs();
  if (argc > 2 && std::string(argv[1]) == "--dump") {
    for (const auto& [key, text] : current) {
      std::string file = std::string(argv[2]) + "/" + key;
      const auto slash = file.find('/', std::string(argv[2]).size() + 1);
      if (slash != std::string::npos) file[slash] = '_';
      if (std::FILE* out = std::fopen(file.c_str(), "wb")) {
        std::fwrite(text.data(), 1, text.size(), out);
        std::fclose(out);
      }
    }
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--print") {
    for (const auto& [key, text] : current)
      std::printf("    {\"%s\", 0x%016llxull, %zu},\n", key.c_str(),
                  static_cast<unsigned long long>(fnv(text)), text.size());
    return 0;
  }
  std::size_t failures = 0;
  std::size_t checked = 0;
  for (const auto& pin : kPins) {
    if (pin.key == nullptr) continue;
    const auto found = current.find(pin.key);
    if (found == current.end()) {
      std::fprintf(stderr, "missing output %s\n", pin.key);
      ++failures;
      continue;
    }
    ++checked;
    const auto hash = fnv(found->second);
    if (hash != pin.hash || found->second.size() != pin.size) {
      std::fprintf(stderr, "%s changed: hash 0x%016llx size %zu, pinned 0x%016llx size %zu\n",
                   pin.key, static_cast<unsigned long long>(hash), found->second.size(),
                   static_cast<unsigned long long>(pin.hash), pin.size);
      ++failures;
    }
  }
  if (checked != current.size()) {
    std::fprintf(stderr, "%zu outputs are not pinned\n", current.size() - checked);
    ++failures;
  }
  std::fprintf(stderr, "%zu outputs checked, %zu failures\n", checked, failures);
  assert(failures == 0);
  return failures == 0 ? 0 : 1;
}
