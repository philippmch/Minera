#include <cassert>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <utility>

#include "minera/dfg_alignment.hpp"

using namespace minera;

namespace {
const DfgFrequency path_dfg{{{"a", "b"}, 5}};
const std::map<std::string, std::uint64_t> path_starts{{"a", 2}}, path_ends{{"b", 3}};

EventLog word_log(const std::vector<std::string>& word) {
  Trace trace;
  trace.case_id = "discard";
  trace.attributes.push_back({"extra", "discard", AttributeKind::kString});
  for (std::size_t i = 0; i < word.size(); ++i) {
    Event event{"discard", word[i], "time-" + std::to_string(i)};
    event.resource = "discard";
    event.attributes.push_back({"extra", "discard", AttributeKind::kString});
    trace.events.push_back(std::move(event));
  }
  EventLog log;
  log.attributes.push_back({"extra", "discard", AttributeKind::kString});
  log.traces.push_back(std::move(trace));
  return log;
}

void test_classic_public_reconstruction_and_diagnostics() {
  struct Example {
    std::vector<std::string> word;
    std::vector<DfgAlignmentMove> moves;
    double cost, internal;
    std::size_t visited, closed;
  };
  const std::vector<Example> examples{
      {{}, {}, 20000, 20000, 3, 3},
      {{"a", "b"}, {{"a", "a"}, {"b", "b"}}, 0, 0, 3, 3},
      {{"a"}, {{"a", "a"}, {">>", "b"}}, 10000, 10000, 4, 4},
      {{"b"}, {{"b", "b"}}, 10000, 10000, 4, 4},
      {{"x"}, {{"x", ">>"}, {">>", "a"}, {">>", "b"}}, 30000, 20000, 4, 4},
      {{"a", "x", "b"}, {{"a", "a"}, {"x", ">>"}, {"b", "b"}}, 10000, 0, 4, 4},
  };
  for (const auto& example : examples) {
    const auto result = align_trace_on_dfg(example.word, path_dfg, path_starts, path_ends);
    assert(result.computed && result.cost == example.cost && result.internal_cost == example.internal);
    assert(result.visited_states == example.visited && result.closed == example.closed);
    assert(result.closed_states == result.closed && result.alignment.size() == example.moves.size());
    for (std::size_t i = 0; i < example.moves.size(); ++i)
      assert(result.alignment[i].log == example.moves[i].log &&
             result.alignment[i].model == example.moves[i].model);
  }
  const auto tail = align_trace_on_dfg({"a", "b", "a"}, path_dfg, path_starts, path_ends);
  assert(tail.alignment.size() == 3 && tail.cost == 10000);
  assert(tail.alignment.back().log == "b" && tail.alignment.back().model == "@@dfg_sink");
  const auto collision = align_trace_on_dfg({"a", "b", "a"},
      {{{"a", "b"}, 1}, {{"@@dfg_sink", "@@dfg_sink"}, 1}}, path_starts, path_ends);
  assert(collision.alignment.back().model == "@@dfg_sink_");
}

void test_classic_public_no_accepting_path() {
  for (const auto& word : {std::vector<std::string>{}, std::vector<std::string>{"a", "b"}}) {
    const auto empty = align_trace_on_dfg(word, {}, {}, {});
    assert(!empty.computed && empty.alignment.empty() && std::isnan(empty.fitness));
    const auto unreachable = align_trace_on_dfg(word, path_dfg, path_starts, {{"c", 1}});
    assert(!unreachable.computed && std::isnan(unreachable.fitness));
    const auto disconnected = align_trace_on_dfg(word,
        {{{"a", "b"}, 1}, {{"c", "d"}, 1}}, path_starts, path_ends);
    assert(disconnected.computed);
  }
  const auto singleton = align_trace_on_dfg({"a"}, {}, {{"a", 0}}, {{"a", 0}});
  assert(singleton.computed && singleton.cost == 0);
  const auto log = word_log({"a"});
  bool projection_failed = false, metrics_failed = false;
  try { (void)project_log_on_dfg(log, {}, {}, {}); }
  catch (const std::invalid_argument&) { projection_failed = true; }
  try { (void)project_alignments_on_dfg(log, {}, {}, {}); }
  catch (const std::invalid_argument&) { metrics_failed = true; }
  assert(projection_failed && metrics_failed);
  for (const auto& input : {EventLog{}, log}) {
    bool log_failed = false;
    try { (void)align_log_on_dfg(input, path_dfg, path_starts, {{"c",1}}); }
    catch (const std::invalid_argument&) { log_failed = true; }
    assert(log_failed);
  }
}

void test_classic_public_projection_context_and_metrics() {
  const auto log = word_log({"a", "x", "b"});
  const auto projected = project_log_on_dfg(log, path_dfg, path_starts, path_ends);
  assert(projected.attributes.size() == 1 &&
      *projected.attribute("@@aligned_dfg") == "[{('a', 'b'): 5}, {'a': 2}, {'b': 3}]");
  const auto& trace = projected.traces.front();
  assert(trace.case_id.empty() && trace.attributes.size() == 1 &&
         *trace.attribute("@@original_trace") == "a,x,b");
  assert(trace.events.size() == 2 && trace.events[1].activity == "b" &&
         trace.events[1].timestamp == "time-1");
  for (const auto& event : trace.events)
    assert(event.resource.empty() && event.case_id.empty() && !event.attribute("extra"));
  const auto counts = project_alignments_on_dfg(log, path_dfg, path_starts, path_ends);
  assert(counts.activities.at("a").sync == 1 && counts.activities.at("b").sync == 0);
  assert(counts.dfg.at({"a", "b"}).sync == 0 && counts.dfg.at({"a", "b"}).mm == 0);
  const auto model = project_alignments_on_dfg(word_log({"a"}), path_dfg, path_starts, path_ends);
  assert(model.activities.at("a").sync == 1 && model.activities.at("b").mm == 0 &&
         model.dfg.at({"a", "b"}).mm == 1);
  const auto singleton = project_alignments_on_dfg(word_log({"a"}), {}, {{"a", 1}}, {{"a", 1}});
  assert(singleton.activities.empty() && singleton.dfg.empty());
  const auto tail = project_log_on_dfg(word_log({"a", "b", "a"}), path_dfg, path_starts, path_ends);
  assert(tail.traces.front().events.size() == 2);
}

void test_classic_projection_uses_supplied_alignment_routes() {
  // Both paths consume the same source word and reach the same DFG endpoint,
  // but they timestamp the projected b differently and count the a->b edge
  // differently because a log move interrupts it.  Supplying these fixed paths keeps this regression
  // independent of randomized routing UUID order.
  const auto log = word_log({"a", "b"});
  // Keep the legacy fifth-argument options call source-compatible with the
  // result-consuming overloads below.
  const auto legacy = project_log_on_dfg(
      log, path_dfg, path_starts, path_ends, {});
  assert(legacy.traces.size() == 1);
  DfgAlignmentResult exact;
  exact.alignment = {{"a", "a"}, {"b", "b"}};
  DfgAlignmentResult model_move;
  model_move.alignment = {{"a", "a"}, {"b", ">>"}, {">>", "b"}};
  model_move.cost = model_move.internal_cost = 20000.0;

  DfgAlignmentLogResult supplied;
  supplied.alignments = {exact, model_move};
  supplied.unique_variants = 1;
  EventLog two_traces = log;
  two_traces.traces.push_back(log.traces.front());
  const DfgAlignmentOptions options;
  const auto projected = project_log_on_dfg(
      two_traces, path_dfg, path_starts, path_ends, supplied, options);
  assert(projected.traces.size() == 2);
  assert(projected.traces[0].events.size() == 2 &&
         projected.traces[0].events[0].activity == "a" &&
         projected.traces[0].events[0].timestamp == "time-0" &&
         projected.traces[0].events[1].activity == "b" &&
         projected.traces[0].events[1].timestamp == "time-1");
  assert(projected.traces[1].events.size() == 2 &&
         projected.traces[1].events[0].activity == "a" &&
         projected.traces[1].events[0].timestamp == "time-0" &&
         projected.traces[1].events[1].activity == "b" &&
         projected.traces[1].events[1].timestamp == "time-0");

  const auto metrics = project_alignments_on_dfg(
      two_traces, path_dfg, path_starts, path_ends, supplied, options);
  assert(metrics.activities.at("a").sync == 2 &&
         metrics.activities.at("b").sync == 0);
  assert(metrics.dfg.at({"a", "b"}).sync == 1 &&
         metrics.dfg.at({"a", "b"}).mm == 0);
  assert(metrics.activities.at("b").lm == 1);

  DfgAlignmentLogResult count_mismatch;
  count_mismatch.alignments = {exact};
  bool rejected = false;
  try {
    (void)project_log_on_dfg(two_traces, path_dfg, path_starts, path_ends,
                             count_mismatch, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  rejected = false;
  try {
    (void)project_alignments_on_dfg(two_traces, path_dfg, path_starts,
                                    path_ends, count_mismatch, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  DfgAlignmentLogResult uncomputed;
  uncomputed.alignments = {exact, model_move};
  uncomputed.alignments.back().computed = false;
  rejected = false;
  try {
    (void)project_log_on_dfg(two_traces, path_dfg, path_starts, path_ends,
                             uncomputed, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  rejected = false;
  try {
    (void)project_alignments_on_dfg(two_traces, path_dfg, path_starts,
                                    path_ends, uncomputed, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void test_classic_public_metrics_count_generated_sink_log_labels() {
  const auto repeated_a = project_alignments_on_dfg(word_log({"a","a","a"}),
      path_dfg, path_starts, path_ends);
  assert(repeated_a.activities.at("a").sync == 2 && repeated_a.activities.at("a").lm == 0);
  assert(repeated_a.activities.at("b").mm == 1 && repeated_a.dfg.at({"a","b"}).mm == 1);
  assert(repeated_a.activities.size() == 2); // No generated sink activity is exposed.
  for (const auto& word : {std::vector<std::string>{"b","b","a"},
                           std::vector<std::string>{"b","b","b"}}) {
    const auto result = project_alignments_on_dfg(word_log(word), path_dfg, path_starts, path_ends);
    assert(result.activities.at("b").sync == 2 && result.activities.at("b").lm == 0);
    assert(result.activities.at("a").sync == 0 && result.dfg.at({"a","b"}).sync == 0 &&
           result.dfg.at({"a","b"}).mm == 0);
  }
  const auto longer = project_alignments_on_dfg(word_log({"a","a","a","a"}),
      path_dfg, path_starts, path_ends);
  assert(longer.activities.at("a").sync == 2 && longer.activities.at("a").lm == 1);
  assert(longer.dfg.at({"a","b"}).mm == 1 && longer.dfg.at({"a","b"}).sync == 0);
}

void test_classic_public_custom_costs_and_loops() {
  DfgAlignmentOptions costs;
  costs.sync_cost = {{"a", 2}, {"b", 3}};
  auto result = align_trace_on_dfg({"a", "x", "b"}, path_dfg, path_starts, path_ends, costs);
  assert(result.cost == 10005 && result.internal_cost == 5 && result.visited_states == 4);
  costs = {};
  costs.model_move_cost = {{"a", 2}, {"b", 3}};
  result = align_trace_on_dfg({"b"}, path_dfg, path_starts, path_ends, costs);
  assert(result.cost == 2 && result.internal_cost == 2 && result.visited_states == 3);
  costs = {};
  costs.log_move_cost = {{"a", 2}, {"b", 3}, {"x", 4}};
  result = align_trace_on_dfg({"a", "a", "b"}, path_dfg, path_starts, path_ends, costs);
  assert(result.cost == 2 && result.internal_cost == 2 && std::isnan(result.best_worst_cost));
  const auto enriched = align_log_on_dfg(word_log({"a", "a", "b"}),
      path_dfg, path_starts, path_ends, costs);
  assert(enriched.alignments.front().best_worst_cost == 20007);
  const auto loop = align_trace_on_dfg({"a", "b", "a"},
      {{{"a", "b"}, 1}, {{"b", "a"}, 1}}, {{"a", 1}}, {{"a", 1}});
  assert(loop.cost == 0 && loop.alignment.size() == 3);
}

void test_classic_public_search_frontier_and_queue_diagnostics() {
  struct Expected { double cost, internal; std::size_t visited, closed; };
  struct Example { std::string word; std::array<Expected, 5> costs; };
  // Public CLASSIC observations, ordered default/sync/model/log/internal.
  const std::vector<Example> examples{
      {"aba", {{{10000,10000,4,5}, {10005,10005,4,7}, {10000,10000,4,5}, {2,2,4,5}, {10000,2,4,5}}}},
      {"ba", {{{20000,20000,6,5}, {20002,20002,8,8}, {10002,10002,4,7}, {10003,10003,6,5}, {20000,10003,6,5}}}},
      {"aa", {{{20000,20000,6,8}, {20002,20002,7,8}, {10003,10003,4,6}, {10002,10002,7,8}, {20000,10002,7,8}}}},
      {"bb", {{{20000,20000,5,7}, {20003,20003,5,8}, {10002,10002,4,6}, {10003,10003,6,7}, {20000,10003,6,7}}}},
      {"aab", {{{10000,10000,4,4}, {10005,10005,8,7}, {10000,10000,6,6}, {2,2,4,4}, {10000,2,4,4}}}},
      {"bab", {{{10000,10000,4,4}, {10005,10005,7,7}, {10000,10000,7,7}, {3,3,4,4}, {10000,3,4,4}}}},
      {"aaaa", {{{40000,40000,15,16}, {40002,40002,18,16}, {30003,30003,11,14}, {10006,10006,16,16}, {40000,10006,16,16}}}},
      {"bbaa", {{{40000,40000,16,15}, {40002,40002,17,16}, {30002,30002,13,15}, {10008,10008,11,10}, {40000,10008,11,10}}}},
      {"aaax", {{{40000,30000,13,12}, {40002,30002,17,14}, {30003,20003,12,12}, {10009,10004,15,14}, {40000,10009,15,15}}}},
  };
  std::array<DfgAlignmentOptions, 5> options;
  options[1].sync_cost = options[2].model_move_cost = options[3].log_move_cost =
      options[4].internal_log_move_cost = {{"a",2},{"b",3},{"c",4},{"x",5}};
  for (const auto& example : examples) {
    std::vector<std::string> word;
    for (const char label : example.word) word.emplace_back(1, label);
    for (std::size_t i = 0; i < options.size(); ++i) {
      const auto actual = align_trace_on_dfg(word, path_dfg, path_starts, path_ends, options[i]);
      const auto& expected = example.costs[i];
      assert(actual.cost == expected.cost && actual.internal_cost == expected.internal);
      assert(actual.visited_states == expected.visited && actual.closed == expected.closed);
      assert(actual.closed_states == expected.closed);
    }
  }
  const auto leading = align_trace_on_dfg({"b","a"}, path_dfg, path_starts, path_ends);
  assert(leading.alignment.size() == 3 && leading.alignment[0].log == "b" &&
         leading.alignment[0].model == ">>" && leading.alignment[1].model == "a" &&
         leading.alignment[2].log == ">>" && leading.alignment[2].model == "b");
  // A closed farther position also suppresses newly queued earlier states on
  // a different branch; retaining pending duplicates alone is insufficient.
  const DfgFrequency fork{{{"a","b"},1},{{"a","c"},1}};
  const auto branch = align_trace_on_dfg({"b","a","c","b"}, fork,
      {{"a",1}}, {{"b",1},{"c",1}}, options[1]);
  assert(branch.cost == 20005 && branch.visited_states == 13 && branch.closed == 12);
  DfgAlignmentOptions bounded;
  bounded.max_visited_states = 5;
  bool bounded_out = false;
  try { (void)align_trace_on_dfg({"a","b","a"}, path_dfg, path_starts, path_ends, bounded); }
  catch (const std::runtime_error&) { bounded_out = true; }
  assert(bounded_out); // The actual pop count exceeds the returned snapshot (4).
}

void test_classic_public_random_routing_tie_support() {
  DfgAlignmentOptions zero;
  zero.sync_cost = zero.model_move_cost = zero.log_move_cost = zero.internal_log_move_cost =
      {{"a",0},{"b",0},{"0",0},{"g",0},{"z",0},{"x",0}};
  // These labels lie outside UUIDv4's lexical range. They exercise both
  // sibling tie orders deterministically without introducing a seed API.
  const auto first = align_trace_on_dfg({"0","z"}, {{{"0","z"},1}},
      {{"0",1}}, {{"z",1}}, zero);
  assert(first.alignment.size() == 2 && first.alignment[0].model == "0" &&
         first.visited_states == 4 && first.closed == 4);
  const auto last = align_trace_on_dfg({"g","z"}, {{{"g","z"},1}},
      {{"g",1}}, {{"z",1}}, zero);
  assert(last.alignment.size() == 4 && last.alignment[0].model == ">>" &&
         last.alignment[1].model == ">>" && last.alignment[2].model == "g" &&
         last.alignment[3].model == "z" && last.visited_states == 5 && last.closed == 5);
  for (int repeat = 0; repeat < 64; ++repeat) {
    const auto result = align_trace_on_dfg({"a","b"}, path_dfg, path_starts, path_ends, zero);
    assert(result.cost == 0 && result.internal_cost == 0);
    if (result.alignment.size() == 2) {
      assert(result.alignment[0].log == "a" && result.alignment[0].model == "a" &&
             result.alignment[1].log == "b" && result.alignment[1].model == "b" &&
             result.visited_states == 4 && result.closed == 4);
    } else {
      assert(result.alignment.size() == 4 && result.alignment[0].model == ">>" &&
             result.alignment[1].model == ">>" && result.alignment[2].log == ">>" &&
             result.alignment[2].model == "a" && result.alignment[3].model == "b" &&
             result.visited_states == 5 && result.closed == 5);
    }
  }
  auto unequal_report = zero;
  unequal_report.log_move_cost = {{"a",2},{"b",3},{"x",5}};
  const auto preferred = align_trace_on_dfg({"a","b"}, path_dfg, path_starts, path_ends, unequal_report);
  assert(preferred.alignment.size() == 2 && preferred.cost == 0 &&
         preferred.visited_states == 3 && preferred.closed == 4);
  // Positive equal costs also expose routing ties, while duplicate log
  // variants retain their one cached result despite independent searches.
  DfgAlignmentOptions equal;
  equal.sync_cost = equal.model_move_cost = equal.log_move_cost = equal.internal_log_move_cost =
      {{"a",1},{"b",1},{"x",1}};
  auto log = word_log({"a"});
  log.traces.push_back(word_log({"a","b"}).traces.front());
  log.traces.push_back(log.traces.front());
  for (int repeat = 0; repeat < 32; ++repeat) {
    const auto result = align_log_on_dfg(log, path_dfg, path_starts, path_ends, equal);
    assert(result.unique_variants == 2 && result.alignments.size() == 3);
    assert(result.alignments[0].visited_states == result.alignments[2].visited_states &&
           result.alignments[0].closed == result.alignments[2].closed);
    assert((result.alignments[0].visited_states == 4 || result.alignments[0].visited_states == 5) &&
           result.alignments[0].closed == 4);
    assert((result.alignments[1].visited_states == 5 || result.alignments[1].visited_states == 6) &&
           result.alignments[1].closed == 6);
  }
}

void test_classic_public_cost_function_presence() {
  const auto fails = [](auto&& call, const char* expected) {
    bool threw = false;
    try { call(); }
    catch (const std::invalid_argument& error) {
      threw = true;
      assert(std::string(error.what()).find(expected) != std::string::npos);
    }
    assert(threw);
  };
  DfgAlignmentOptions options;
  options.model_move_cost = {{"a", 2}};
  // Raw trace alignment never asks for the unused model move b. Log alignment
  // does ask for it when aligning the empty trace for its fitness baseline.
  const auto raw = align_trace_on_dfg({"a", "b"}, path_dfg, path_starts, path_ends, options);
  assert(raw.computed && raw.cost == 0 && std::isnan(raw.fitness) &&
         std::isnan(raw.best_worst_cost));
  const auto log = word_log({"a", "b"});
  assert(align_trace_on_dfg(log.traces.front(), path_dfg, path_starts, path_ends, options).cost == 0);
  fails([&] { (void)align_log_on_dfg(log, path_dfg, path_starts, path_ends, options); }, "model move cost for activity b");
  fails([&] { (void)project_log_on_dfg(log, path_dfg, path_starts, path_ends, options); }, "model move cost for activity b");
  fails([&] { (void)project_alignments_on_dfg(log, path_dfg, path_starts, path_ends, options); }, "model move cost for activity b");
  options.model_move_cost.clear();
  options.model_move_cost_supplied = true;
  assert(align_trace_on_dfg({"a", "b"}, path_dfg, path_starts, path_ends, options).cost == 0);
  fails([&] { (void)align_log_on_dfg(EventLog{}, path_dfg, path_starts, path_ends, options); }, "model move cost for activity a");
  options = {};
  options.sync_cost_supplied = true;
  assert(align_trace_on_dfg({"x"}, path_dfg, path_starts, path_ends, options).cost == 30000);
  fails([&] { (void)align_trace_on_dfg({"a", "b"}, path_dfg, path_starts, path_ends, options); }, "synchronous cost for activity a");
  options.sync_cost = {{"a", 2}};
  fails([&] { (void)align_trace_on_dfg({"a", "b"}, path_dfg, path_starts, path_ends, options); }, "synchronous cost for activity b");
  options = {};
  options.log_move_cost_supplied = true;
  fails([&] { (void)align_trace_on_dfg({"a", "b"}, path_dfg, path_starts, path_ends, options); }, "log move cost for activity a");
  options.log_move_cost = {{"x", 4}};
  assert(align_trace_on_dfg({"x"}, path_dfg, path_starts, path_ends, options).cost == 20004);
  options = {};
  options.internal_log_move_cost_supplied = true;
  fails([&] { (void)align_trace_on_dfg({"x"}, path_dfg, path_starts, path_ends, options); }, "internal log move cost for activity x");
  options.internal_log_move_cost = {{"x", 4}};
  const auto internal = align_trace_on_dfg({"x"}, path_dfg, path_starts, path_ends, options);
  assert(internal.cost == 30000 && internal.internal_cost == 20004);
  options = {};
  options.model_move_cost = {{"a", 0}, {"b", 0}};
  options.log_move_cost = {{"a", 0}, {"b", 0}};
  assert(align_trace_on_dfg({"a", "b"}, path_dfg, path_starts, path_ends, options).cost == 0);
  fails([&] { (void)align_log_on_dfg(log, path_dfg, path_starts, path_ends, options); }, "fitness denominator is zero");
  fails([&] { (void)project_log_on_dfg(log, path_dfg, path_starts, path_ends, options); }, "fitness denominator is zero");
  fails([&] { (void)project_alignments_on_dfg(log, path_dfg, path_starts, path_ends, options); }, "fitness denominator is zero");
  assert(align_log_on_dfg(EventLog{}, path_dfg, path_starts, path_ends, options).alignments.empty());
}

void test_classic_public_model_metadata_python_escaping() {
  const std::vector<std::pair<std::string, std::string>> examples{
      {"\b", "'\\x08'"}, {std::string(1, '\0'), "'\\x00'"},
      {"\xe2\x80\x8b", "'\\u200b'"},
      {"\xc2\xa0\xf3\xa0\x80\x81", "'\\xa0\\U000e0001'"},
      {"\t\n\r\\\x7f", "'\\t\\n\\r\\\\\\x7f'"},
      {"a'b", "\"a'b\""}, {"a'\"b", "'a\\'\"b'"},
      {"\xc3\xa9\xf0\x9f\x98\x80", "'\xc3\xa9\xf0\x9f\x98\x80'"},
  };
  for (const auto& [label, quoted] : examples) {
    const auto projected = project_log_on_dfg(word_log({label, "b"}),
        {{{label, "b"}, 5}}, {{label, 2}}, {{"b", 3}});
    assert(*projected.attribute("@@aligned_dfg") ==
           "[{(" + quoted + ", 'b'): 5}, {" + quoted + ": 2}, {'b': 3}]");
  }
  for (const auto& label : {std::string("\xc0\xaf"), std::string("\xed\xa0\x80"),
                            std::string("\xf4\x90\x80\x80"), std::string("\xe2\x80")}) {
    bool rejected = false;
    try { (void)project_log_on_dfg({}, {{{label, "b"}, 1}}, {{label, 1}}, {{"b", 1}}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  }
}
}

int main() {
  test_classic_public_reconstruction_and_diagnostics();
  test_classic_public_no_accepting_path();
  test_classic_public_projection_context_and_metrics();
  test_classic_projection_uses_supplied_alignment_routes();
  test_classic_public_metrics_count_generated_sink_log_labels();
  test_classic_public_custom_costs_and_loops();
  test_classic_public_search_frontier_and_queue_diagnostics();
  test_classic_public_random_routing_tie_support();
  test_classic_public_cost_function_presence();
  test_classic_public_model_metadata_python_escaping();
  DfgFrequency d{{{"a", "b"}, 1}};
  std::map<std::string, std::uint64_t> s{{"a", 1}}, e{{"b", 1}};
  auto exact = align_trace_on_dfg(std::vector<std::string>{"a", "b"}, d, s, e);
  assert(exact.cost == 0 && exact.alignment.size() == 2);
  assert(exact.alignment[0].log == "a" && exact.alignment[0].model == "a");

  auto unknown = align_trace_on_dfg(std::vector<std::string>{"x"}, d, s, e);
  assert(unknown.alignment.size() >= 3 && unknown.cost == 30000);
  auto empty = align_trace_on_dfg(std::vector<std::string>{}, d, s, e);
  assert(empty.cost == 20000 && empty.alignment.empty());

  EventLog log;
  Trace t1; t1.case_id = "1"; t1.events.push_back(Event{"1", "a", "t1", {}, 0});
  t1.events.push_back(Event{"1", "b", "t2", {}, 1});
  Trace t2=t1; t2.case_id="2"; log.traces={t2,t1};
  auto all=align_log_on_dfg(log,d,s,e); assert(all.alignments.size()==2 && all.unique_variants==1);
  auto projected=project_log_on_dfg(log,d,s,e); assert(projected.traces.size()==2);
  assert(projected.traces[0].events[0].attribute("@@is_conforming") != nullptr);
  assert(projected.attribute("@@aligned_dfg") != nullptr);
  auto agg=project_alignments_on_dfg(log,d,s,e); assert(agg.activities["a"].sync==2);
  assert(agg.activities["b"].sync==0 && agg.dfg.at({"a", "b"}).sync==2);

  // Projection metadata replaces collisions and source context is discarded.
  // The final move is excluded from activity counts.
  EventLog colliding = log;
  colliding.attributes.push_back({"@@aligned_dfg", "stale", AttributeKind::kString});
  colliding.traces[0].attributes.push_back({"@@original_trace", "stale", AttributeKind::kString});
  colliding.traces[0].events[0].attributes.push_back({"@@is_conforming", "stale", AttributeKind::kString});
  colliding.traces[0].events[0].lifecycle = "complete";
  colliding.traces[0].events.push_back(Event{"2", "a", "t3", {}, 2});
  auto pc = project_log_on_dfg(colliding, d, s, e);
  assert(pc.attribute("@@aligned_dfg") && *pc.attribute("@@aligned_dfg") != "stale");
  assert(pc.traces[0].attribute("@@original_trace") && *pc.traces[0].attribute("@@original_trace") != "stale");
  assert(pc.traces[0].events[0].attribute("@@is_conforming") && *pc.traces[0].events[0].attribute("@@is_conforming") == "true");
  assert(pc.traces[0].events[0].lifecycle.empty());
  auto agc = project_alignments_on_dfg(colliding, d, s, e);
  assert(agc.activities["a"].lm == 0);

  // Model moves inherit custom first-class selectors from the source event,
  // while an incompatible typed activity value is represented as a string.
  EventLog custom_log;
  Trace custom_trace;
  custom_trace.case_id = "42";
  custom_trace.case_id_state = FirstClassAttributeState::kPresent;
  custom_trace.case_id_source_key = "case.number";
  EventAttribute custom_case{"case.number", "42", AttributeKind::kInt};
  custom_case.typed_value = std::int64_t{42};
  custom_trace.case_id_attribute = custom_case;
  Event custom_event;
  custom_event.case_id = "42";
  custom_event.case_id_state = FirstClassAttributeState::kPresent;
  custom_event.case_id_source_key = "case.number";
  custom_event.activity = "7";
  custom_event.activity_state = FirstClassAttributeState::kPresent;
  custom_event.activity_source_key = "activity.code";
  EventAttribute custom_activity{"activity.code", "7", AttributeKind::kInt};
  custom_activity.typed_value = std::int64_t{7};
  custom_event.activity_attribute = custom_activity;
  custom_event.timestamp = "2026-01-01T00:00:00Z";
  custom_event.timestamp_state = FirstClassAttributeState::kPresent;
  custom_event.timestamp_source_key = "when.value";
  EventAttribute custom_timestamp{
      "when.value", custom_event.timestamp, AttributeKind::kDate};
  custom_timestamp.typed_value =
      NativeDate{"2026-01-01T00:00:00.000Z"};
  custom_event.timestamp_attribute = custom_timestamp;
  custom_trace.events.push_back(std::move(custom_event));
  custom_log.traces.push_back(std::move(custom_trace));
  const auto custom_projection = project_log_on_dfg(custom_log, d, s, e);
  assert(!custom_projection.traces.front().case_id_attribute);
  assert(custom_projection.traces.front().events.size() == 2);
  const auto& model_event = custom_projection.traces.front().events.front();
  assert(model_event.activity == "a" &&
         model_event.activity_source_key == "activity.code" &&
         model_event.activity_attribute &&
         model_event.activity_attribute->kind == AttributeKind::kString &&
         model_event.activity_attribute->value == "a");
  assert(model_event.timestamp_source_key == "when.value" &&
         model_event.timestamp_attribute &&
         model_event.timestamp_attribute->kind == AttributeKind::kDate);

  DfgAlignmentOptions bounded; bounded.max_visited_states = 1; bool limit = false;
  try { (void)align_trace_on_dfg(std::vector<std::string>{"a", "b"}, d, s, e, bounded); } catch (const std::runtime_error&) { limit = true; }
  assert(limit);
  DfgFrequency cyc{{{"a", "b"}, 1}, {{"b", "a"}, 1}};
  auto cycle = align_trace_on_dfg(std::vector<std::string>{"a", "b", "a"}, cyc, s, s);
  assert(cycle.cost == 0 && cycle.alignment.size() == 3);

  // Revisiting a graph node at a later trace position must remain a distinct
  // search state.  The exact cyclic run is free, while the repeated log-only
  // event in the second trace has one deviation and then rejoins the cycle.
  DfgFrequency revisiting{{{"a", "b"}, 1},
                          {{"b", "a"}, 1},
                          {{"a", "c"}, 1}};
  std::map<std::string, std::uint64_t> revisit_start{{"a", 1}};
  std::map<std::string, std::uint64_t> revisit_end{{"c", 1}};
  const auto exact_revisit = align_trace_on_dfg(
      std::vector<std::string>{"a", "b", "a", "c"}, revisiting,
      revisit_start, revisit_end);
  assert(exact_revisit.cost == 0.0 && exact_revisit.alignment.size() == 4);
  const auto deviating_revisit = align_trace_on_dfg(
      std::vector<std::string>{"a", "b", "b", "a", "c"}, revisiting,
      revisit_start, revisit_end);
  assert(deviating_revisit.cost == 10000.0);
  assert(deviating_revisit.alignment.size() == 5);

  DfgAlignmentOptions custom; custom.log_move_cost["x"]=2; custom.internal_log_move_cost["x"]=0.5;
  auto c=align_trace_on_dfg(std::vector<std::string>{"x"},d,s,e,custom); assert(c.cost==20002 && c.internal_cost< c.cost);
  bool threw=false; try { custom.log_move_cost["x"]=-1; (void)align_trace_on_dfg(std::vector<std::string>{"x"},d,s,e,custom); } catch(const std::invalid_argument&) { threw=true; } assert(threw);

  DfgFrequency bad{{{"a", "b"},1},{{"c", "d"},1}};
  assert(align_trace_on_dfg({},bad,s,e).computed);

  // Activity and timestamp selectors are applied to both alignment words and
  // projected model events.  Generic typed cells remain selected metadata,
  // while a newly materialized label is represented as text.
  EventLog selected_log;
  Trace selected_trace;
  Event selected_event{"1", "ignored", "canonical-time"};
  selected_event.attributes.push_back({"phase", "a", AttributeKind::kInt});
  selected_event.attributes.back().typed_value = std::int64_t{1};
  selected_event.attributes.push_back(
      {"when", "2026-01-01T00:00:00Z", AttributeKind::kDate});
  selected_event.attributes.back().typed_value =
      NativeDate{"2026-01-01T00:00:00.000Z"};
  selected_event.attributes.push_back({"discard", "extra", AttributeKind::kString});
  selected_trace.events.push_back(std::move(selected_event));
  selected_log.traces.push_back(std::move(selected_trace));
  DfgAlignmentOptions selected_options;
  selected_options.activity_attribute = "phase";
  selected_options.timestamp_attribute = "when";
  const auto selected_alignment = align_log_on_dfg(
      selected_log, d, s, e, selected_options);
  assert(selected_alignment.alignments.size() == 1 &&
         selected_alignment.alignments.front().cost == 10000.0);
  const auto selected_projection = project_log_on_dfg(
      selected_log, d, s, e, selected_options);
  assert(selected_projection.traces.front().events.size() == 2);
  const auto& selected_sync = selected_projection.traces.front().events[0];
  const auto& selected_model = selected_projection.traces.front().events[1];
  assert(selected_sync.attribute("phase") &&
         *selected_sync.attribute("phase") == "a" &&
         selected_sync.attributes.size() == 3 &&
         !selected_sync.attribute("discard") &&
         selected_sync.attributes[0].key == "phase" &&
         selected_sync.attributes[0].kind == AttributeKind::kInt &&
         std::get<std::int64_t>(selected_sync.attributes[0].typed_value) == 1 &&
         selected_sync.attributes[1].key == "when" &&
         selected_sync.attributes[1].kind == AttributeKind::kDate &&
         std::get<NativeDate>(selected_sync.attributes[1].typed_value).value ==
             "2026-01-01T00:00:00.000Z");
  assert(selected_model.activity == "b" &&
         selected_model.attribute("phase") &&
         *selected_model.attribute("phase") == "b");
  assert(selected_model.activity_source_key == "phase" &&
         selected_model.activity_attribute &&
         selected_model.activity_attribute->kind == AttributeKind::kString);
  assert(selected_model.attribute("when") &&
         *selected_model.attribute("when") == "2026-01-01T00:00:00Z");
  assert(selected_model.timestamp_source_key == "when" &&
         selected_model.timestamp_attribute &&
         selected_model.timestamp_attribute->kind == AttributeKind::kDate);
  // A nonempty projection requires the selected timestamp even for the
  // conventional field name.
  EventLog timestamp_less;
  Trace timestamp_less_trace;
  timestamp_less_trace.events.push_back(Event{"c", "a"});
  timestamp_less.traces.push_back(std::move(timestamp_less_trace));
  threw = false;
  try { (void)project_log_on_dfg(timestamp_less, d, s, e); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  DfgAlignmentOptions missing_timestamp = selected_options;
  missing_timestamp.timestamp_attribute = "missing-when";
  threw = false;
  try {
    (void)project_log_on_dfg(timestamp_less, d, s, e, missing_timestamp);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  DfgAlignmentOptions missing_selector = selected_options;
  missing_selector.activity_attribute = "does-not-exist";
  threw = false;
  try { (void)align_log_on_dfg(selected_log, d, s, e, missing_selector); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  EventLog null_selected = selected_log;
  null_selected.traces.front().events.front().attributes.front().is_null = true;
  threw = false;
  try { (void)align_log_on_dfg(null_selected, d, s, e, selected_options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  selected_options.activity_attribute = "  ";
  threw = false;
  try { (void)align_log_on_dfg(selected_log, d, s, e, selected_options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  return 0;
}
