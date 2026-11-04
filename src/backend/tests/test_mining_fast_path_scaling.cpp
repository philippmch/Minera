// Time and memory bounds for the mining stages whose fast paths replaced
// super-linear work: Declare discovery over a large alphabet (dense candidate
// evaluation), POWL discovery on very long traces and on a large alphabet,
// log skeleton discovery and conformance on one very long trace, model
// footprints on a net whose transitions are always enabled, token replay of
// a heuristic net over a large alphabet (hidden-place routing), and, on a
// random log's inductive tau-flower, the soundness search, model
// decoration's end-of-trace silent routing, and the alignment time budget
// and cancel callback. Before the fast paths each case took from about forty
// seconds to several minutes in a release build; the bounds below are well
// under that and loose enough for a slower build. The outputs themselves are
// pinned by test_mining_fast_path_equivalence.cpp.
//
// Run with a case name to run that case alone.
#include "minera/alpha_miner.hpp"
#include "minera/conformance.hpp"
#include "minera/declare.hpp"
#include "minera/heuristic_miner.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/log_skeleton.hpp"
#include "minera/model_decoration.hpp"
#include "minera/petri_net.hpp"
#include "minera/powl_discovery.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 2  // GetProcessMemoryInfo from kernel32, no psapi.lib
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace {

using namespace minera;

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

EventLog build(const std::vector<std::vector<std::size_t>>& traces) {
  EventLog log;
  std::size_t order = 0;
  for (std::size_t c = 0; c < traces.size(); ++c) {
    Trace trace;
    trace.case_id = "case" + std::to_string(c);
    trace.case_id_state = FirstClassAttributeState::kPresent;
    trace.events.reserve(traces[c].size());
    for (const auto a : traces[c]) {
      Event event{trace.case_id, label(a), "", "", order++};
      event.case_id_state = FirstClassAttributeState::kPresent;
      event.activity_state = FirstClassAttributeState::kPresent;
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

// A backbone over `acts` activities with skips, swaps and loop-backs.
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

std::vector<std::size_t> uniform(Rng& rng, std::size_t acts, std::size_t length) {
  std::vector<std::size_t> t(length);
  for (auto& a : t) a = rng.below(acts);
  return t;
}

// About 5k events over 200 activities.
EventLog many_activities_log() {
  Rng rng(11);
  std::vector<std::vector<std::size_t>> traces;
  for (int c = 0; c < 170; ++c) traces.push_back(structured(rng, 200, rng.below(200), 30));
  return build(traces);
}

// Peak resident set size in KiB. ru_maxrss is KiB on Linux but bytes on
// macOS; Windows reports the peak working set in bytes.
long peak_rss_kib() {
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS counters{};
  counters.cb = sizeof counters;
  if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof counters)) return 0;
  return static_cast<long>(counters.PeakWorkingSetSize / 1024);
#else
  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
#if defined(__APPLE__)
  return static_cast<long>(usage.ru_maxrss / 1024);
#else
  return usage.ru_maxrss;
#endif
#endif
}

struct Case {
  const char* name;
  double max_seconds;
  long max_rss_growth_mib;
  std::function<void()> body;
};

void declare_many_activities() {
  const auto log = many_activities_log();
  const auto model = discover_declare(log);
  std::size_t rules = 0;
  for (const auto& [family, list] : model.rules) rules += list.size();
  assert(rules > 0);
  const auto conformance = check_declare_conformance(log, model);
  assert(conformance.size() == log.traces.size() && conformance.front().no_constr_total > 0);
}

void powl_one_long_trace() {
  Rng rng(13);
  const auto log = build({uniform(rng, 3, 16000)});
  const auto model = discover_powl(log);
  (void)model;
}

// A backbone that wraps around `acts` activities for `length` events, with
// skips, swaps and loop-backs: long traces revisiting every activity.
std::vector<std::size_t> wrapping(Rng& rng, std::size_t acts, std::size_t length) {
  std::vector<std::size_t> t;
  std::size_t i = 0;
  while (t.size() < length) {
    const auto r = rng.below(100);
    if (r < 10) {
      i = (i + 2) % acts;
      continue;
    }
    if (r < 20 && i + 1 < acts && t.size() + 1 < length) {
      t.push_back((i + 1) % acts);
      t.push_back(i);
      i = (i + 2) % acts;
      continue;
    }
    if (r < 27 && i > 0) --i;
    t.push_back(i);
    i = (i + 1) % acts;
  }
  return t;
}

void powl_long_traces() {
  Rng rng(14);
  std::vector<std::vector<std::size_t>> traces;
  for (int c = 0; c < 50; ++c) traces.push_back(wrapping(rng, 20, 1000));
  const auto log = build(traces);
  const auto model = discover_powl(log);
  (void)model;
}

void powl_many_activities() {
  const auto log = many_activities_log();
  const auto model = discover_powl(log);
  (void)model;
}

void log_skeleton_one_long_trace() {
  Rng rng(15);
  const auto log = build({uniform(rng, 3, 16000)});
  const auto model = discover_log_skeleton(log);
  const auto conformance = check_log_skeleton_conformance(log, model);
  assert(conformance.size() == 1);
}

void footprints_free_transitions() {
  // Skips, swaps and loop-backs leave the alpha miner without causal places
  // between the activities, so its transitions are always enabled and the
  // reachable marking space is unbounded: the footprint exploration runs to
  // its marking cap.
  Rng rng(16);
  std::vector<std::vector<std::size_t>> traces;
  for (int c = 0; c < 300; ++c) traces.push_back(structured(rng, 60, 0, 60));
  const auto log = build(traces);
  const auto net = discover_petri_net(log);
  const auto result = check_footprints(log, net);
  assert(result.log_relations > 0);
  assert(result.exhausted);
}

void heuristic_replay_many_activities() {
  // About 50k events over 200 activities.
  Rng rng(11);
  std::vector<std::vector<std::size_t>> traces;
  for (int c = 0; c < 1700; ++c) traces.push_back(structured(rng, 200, rng.below(200), 30));
  const auto log = build(traces);
  const auto net = to_petri_net(discover_heuristics_net(log));
  const auto result = check_conformance(log, net);
  assert(result.fitness >= 0.0 && result.fitness <= 1.0);
}

// About 5k events of uniformly random activities over `acts` labels: over 21
// the inductive net is a tau-flower of 86 places and 107 transitions (86
// silent).
EventLog random_log(std::size_t acts = 21) {
  Rng rng(17);
  std::vector<std::vector<std::size_t>> traces;
  for (int c = 0; c < 520; ++c) traces.push_back(uniform(rng, acts, 2 + rng.below(17)));
  return build(traces);
}

void soundness_random_inductive() {
  // The uniform-cost model run exhausts its 200k expansions on the silent
  // flower (66 places, 82 transitions over 16 labels), and the
  // marking-equation A* then solves an LP per new marking.
  const auto net = to_petri_net(discover_process_tree(random_log(16)));
  assert(net.transitions().size() == 82);
  assert(final_marking_reachable(net));
}

void decoration_random_inductive() {
  // End-of-trace silent routing: hundreds of distinct end markings whose
  // breadth-first searches run to their expansion cap.
  const auto log = random_log();
  const auto net = to_petri_net(discover_process_tree(log));
  const auto decoration = decorate_model(log, net);
  assert(decoration.trace_count == log.traces.size());
}

void alignment_budget_and_cancel() {
  // The API default is no time limit; a total budget and a cancel callback
  // must each stop a long alignment promptly with an uncomputed result.
  const auto log = random_log();
  const auto net = to_petri_net(discover_process_tree(log));
  {
    AlignmentOptions options;
    options.max_total_time_seconds = 1.0;
    const auto start = std::chrono::steady_clock::now();
    const auto result = check_alignment(log, net, options);
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::fprintf(stderr, "  1 s budget: %.2f s, computed %d\n", seconds, result.computed);
    assert(seconds < 4.0);
    assert(result.computed || result.exhausted);
  }
  {
    AlignmentOptions options;
    const auto start = std::chrono::steady_clock::now();
    std::size_t polls = 0;
    options.cancelled = [&] {
      ++polls;
      return std::chrono::steady_clock::now() - start > std::chrono::milliseconds(300);
    };
    const auto result = check_alignment(log, net, options);
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::fprintf(stderr, "  cancel at 0.3 s: %.2f s, computed %d, %zu polls\n", seconds,
                 result.computed, polls);
    assert(seconds < 3.0);
    assert(polls > 0);
    assert(!result.computed && result.exhausted);
  }
}

}  // namespace

int main(int argc, char** argv) {
  const std::string only = argc > 1 ? argv[1] : "";
  const Case cases[] = {
      {"declare_many_activities", 60.0, 512, declare_many_activities},
      {"powl_one_long_trace", 30.0, 512, powl_one_long_trace},
      {"powl_long_traces", 90.0, 1024, powl_long_traces},
      {"powl_many_activities", 90.0, 1024, powl_many_activities},
      {"log_skeleton_one_long_trace", 30.0, 512, log_skeleton_one_long_trace},
      {"footprints_free_transitions", 30.0, 2048, footprints_free_transitions},
      {"heuristic_replay_many_activities", 30.0, 1024, heuristic_replay_many_activities},
      {"soundness_random_inductive", 32.0, 1536, soundness_random_inductive},
      {"decoration_random_inductive", 60.0, 512, decoration_random_inductive},
      {"alignment_budget_and_cancel", 20.0, 2048, alignment_budget_and_cancel},
  };
  bool ran = false;
  for (const auto& test : cases) {
    if (!only.empty() && only != test.name) continue;
    ran = true;
    const long before = peak_rss_kib();
    const auto start = std::chrono::steady_clock::now();
    test.body();
    const auto seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const long grown_mib = (peak_rss_kib() - before) / 1024;
    std::fprintf(stderr, "%s: %.2f s, peak RSS growth %ld MiB\n", test.name, seconds, grown_mib);
    assert(seconds < test.max_seconds);
    assert(grown_mib < test.max_rss_growth_mib);
  }
  assert(ran);
  return ran ? 0 : 1;
}
