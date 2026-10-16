#include "minera/playout_workbench_json.hpp"
#include "minera/active_event_source.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace minera;

template <class Action>
void rejects(Action action) {
  bool rejected = false;
  try { action(); } catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}

void contains(const std::string& actual, std::string_view expected) {
  if (actual.find(expected) == std::string::npos)
    std::cerr << "Missing JSON fragment: " << expected << "\nActual JSON: " << actual << std::endl;
  assert(actual.find(expected) != std::string::npos);
}

template <class Options, class Parse>
void complete_round_trip(const Options& options, Parse parse, std::size_t count) {
  const auto json = serialize_playout_workbench_options_json(options);
  std::set<std::string> supplied{"stale"};
  const auto parsed = parse(json, &supplied);
  assert(supplied.size() == count);
  assert(serialize_playout_workbench_options_json(parsed) == json);
  (void)parse("{}", &supplied);
  assert(supplied.empty());
  supplied = {"untouched"};
  rejects([&] { (void)parse(R"({"seed":false})", &supplied); });
  assert(supplied == std::set<std::string>{"untouched"});
}

PetriNet choice_net() {
  PetriNet net;
  const auto source = net.add_place(), sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A", "B"});
  net.add_transition(0, {source}, {sink});
  net.add_transition(1, {source}, {sink});
  return net;
}

EventLog one_a() {
  EventLog log;
  Trace trace;
  trace.case_id = "original";
  Event event;
  event.case_id = trace.case_id;
  event.activity = "A";
  trace.events.push_back(event);
  log.traces.push_back(trace);
  return log;
}

void exact_native_defaults_and_all_69_fields() {
  const auto basic = parse_playout_workbench_options("{}");
  assert(basic.traces == 100 && basic.max_steps == 1000 && basic.seed == 1);
  assert(basic.start == 1767225600 && basic.case_interval == 60 && basic.event_interval == 1);
  assert(basic.initial_marking.empty() && basic.final_marking.empty());
  assert(!basic.add_only_if_fm_is_reached && !basic.fm_leq_accepted && !basic.return_visited_elements);
  const auto stochastic = parse_stochastic_playout_workbench_options("{}");
  assert(stochastic.traces == 1000 && stochastic.max_steps == 1000 && stochastic.seed == 1);
  assert(stochastic.stop_at_final && !stochastic.fit_from_log && stochastic.fitting_max_states == 200000);
  assert(stochastic.transition_weights.empty() && !stochastic.return_visited_elements);
  assert(stochastic.start == basic.start && stochastic.case_interval == 60 && stochastic.event_interval == 1);
  assert(stochastic.initial_marking.empty() && stochastic.final_marking.empty());
  const auto extensive = parse_extensive_playout_workbench_options("{}");
  assert(extensive.max_length == 10 && extensive.max_traces == 1000 && extensive.max_states == 200000);
  assert(extensive.start == basic.start && extensive.case_interval == 60 && extensive.event_interval == 1);
  assert(extensive.initial_marking.empty() && extensive.final_marking.empty());
  assert(extensive.branching_factor_transitions == 0 && extensive.seed == 1);
  assert(!extensive.return_elements && !extensive.max_marking_occ);
  const auto dfg = parse_dfg_playout_workbench_options("{}");
  assert(dfg.variant == DfgPlayoutVariant::kClassic && dfg.max_variants == 3000);
  assert(dfg.min_weighted_probability == 1 && dfg.max_occurrences_per_activity == 2);
  assert(!dfg.interrupt_when_dfg_complete && !dfg.add_trace_if_takes_new_edges);
  assert(dfg.max_execution_time_ms == 0 && !dfg.return_variants && !dfg.return_only_if_complete);
  assert(dfg.min_variant_occurrences == 1 && dfg.traces == 1000 && dfg.case_arrival_rate == 1);
  assert(dfg.seed == 1 && dfg.max_events_per_trace == 1000 && dfg.max_steps == 0);
  assert(dfg.start_time == "2026-01-01T00:00:00Z" && dfg.activity_key == "concept:name");
  assert(dfg.timestamp_key == "time:timestamp" && dfg.case_id_key == "case:concept:name");
  assert(dfg.artificial_start_activity == "\xe2\x96\xb6" && dfg.artificial_end_activity == "\xe2\x96\xa0");
  const auto tree = parse_process_tree_playout_workbench_options("{}");
  assert(tree.variant == ProcessTreePlayoutVariant::kTopBottom);
  assert(tree.traces == 1000 && tree.max_steps == 1000 && tree.seed == 1);
  assert(tree.max_length == 1000 && tree.min_length == 1 && tree.max_traces == 100000);
  assert(tree.max_loop_occurrences == 3 && tree.start == basic.start);
  assert(tree.case_interval == 60 && tree.event_interval == 1 && !tree.return_set_strings);

  const auto max_size = std::numeric_limits<std::size_t>::max();
  const auto max_seed = std::numeric_limits<std::uint64_t>::max();
  const auto max_marking = std::numeric_limits<std::uint32_t>::max();
  PlayoutOptions b;
  b.traces = max_size; b.max_steps = max_size - 1; b.seed = max_seed;
  b.start = -0.0; b.case_interval = 0.25; b.event_interval = 0.5;
  b.initial_marking = {0, max_marking}; b.final_marking = {max_marking, 0};
  b.add_only_if_fm_is_reached = true; b.fm_leq_accepted = true; b.return_visited_elements = true;
  complete_round_trip(b, parse_playout_workbench_options, 11);
  StochasticPlayoutOptions s;
  s.traces = max_size; s.max_steps = max_size - 1; s.seed = max_seed;
  s.start = -0.0; s.case_interval = -2.5; s.event_interval = 0.125;
  s.transition_weights = {-1.0, -0.0, std::numeric_limits<double>::denorm_min()};
  s.stop_at_final = false; s.initial_marking = {max_marking, 0}; s.final_marking = {0, max_marking};
  s.fit_from_log = true; s.fitting_max_states = max_size; s.return_visited_elements = true;
  complete_round_trip(s, parse_stochastic_playout_workbench_options, 13);
  ExtensivePlayoutOptions e;
  e.max_length = max_size; e.max_traces = max_size - 1; e.max_states = max_size - 2;
  e.start = -0.0; e.case_interval = -2.5; e.event_interval = 0.125;
  e.initial_marking = {max_marking, 0}; e.final_marking = {0, max_marking};
  e.branching_factor_transitions = 0.75; e.seed = max_seed; e.return_elements = true; e.max_marking_occ = max_size;
  complete_round_trip(e, parse_extensive_playout_workbench_options, 12);
  DfgPlayoutOptions d;
  d.variant = DfgPlayoutVariant::kPerformance; d.max_variants = max_size;
  d.min_weighted_probability = -0.0; d.max_occurrences_per_activity = max_size - 1;
  d.interrupt_when_dfg_complete = true; d.add_trace_if_takes_new_edges = true;
  d.max_execution_time_ms = max_size - 2; d.return_variants = true; d.return_only_if_complete = true;
  d.min_variant_occurrences = max_size - 3; d.traces = max_size - 4; d.case_arrival_rate = 0.125;
  d.seed = max_seed; d.max_events_per_trace = max_size - 5;
  d.start_time = "  literal date  "; d.activity_key = std::string("a\0b", 3);
  d.timestamp_key = " "; d.case_id_key = ""; d.artificial_start_activity = "same";
  d.artificial_end_activity = "same"; d.max_steps = max_size - 6;
  complete_round_trip(d, parse_dfg_playout_workbench_options, 21);
  ProcessTreePlayoutOptions t;
  t.variant = ProcessTreePlayoutVariant::kExtensive; t.traces = max_size;
  t.max_steps = max_size - 1; t.seed = max_seed; t.max_length = max_size - 2;
  t.min_length = max_size - 3; t.max_traces = max_size - 4; t.max_loop_occurrences = max_size - 5;
  t.start = -0.0; t.case_interval = 0.25; t.event_interval = 0.125; t.return_set_strings = true;
  complete_round_trip(t, parse_process_tree_playout_workbench_options, 12);
  contains(serialize_playout_workbench_options_json(b), R"("seed":18446744073709551615)");
  contains(serialize_playout_workbench_options_json(b), R"("initial_marking":[0,4294967295])");
  assert(parse_extensive_playout_workbench_options(R"({"max_marking_occ":0})").max_marking_occ == 0);
  assert(!parse_extensive_playout_workbench_options(R"({"max_marking_occ":null})").max_marking_occ);
  assert(parse_stochastic_playout_workbench_options(R"({"transition_weights":[]})").transition_weights.empty());
}

void enums_ieee_and_literal_strings_are_lossless() {
  for (const auto* name : {"classic", "performance"}) {
    const auto options = parse_dfg_playout_workbench_options(std::string("{\"variant\":\"") + name + "\"}");
    const auto encoded = serialize_playout_workbench_options_json(options);
    contains(encoded, std::string("\"name\":\"") + name + "\"");
    assert(serialize_playout_workbench_options_json(parse_dfg_playout_workbench_options(encoded)) == encoded);
  }
  for (const auto* name : {"basic", "extensive", "top_bottom"}) {
    const auto options = parse_process_tree_playout_workbench_options(std::string("{\"variant\":\"") + name + "\"}");
    const auto encoded = serialize_playout_workbench_options_json(options);
    assert(serialize_playout_workbench_options_json(parse_process_tree_playout_workbench_options(encoded)) == encoded);
  }
  const auto raw = parse_dfg_playout_workbench_options(R"({"variant":{"name":"UNKNOWN","index":-2147483648},"activity_key":"\ud800","timestamp_key":"x\u0000\u000a\t ","case_id_key":"\udc00","artificial_start_activity":"\ud83d\ude00","artificial_end_activity":"\u03b1"})");
  assert(static_cast<int>(raw.variant) == std::numeric_limits<int>::min());
  assert(raw.activity_key == std::string("\xed\xa0\x80", 3));
  assert(raw.case_id_key == std::string("\xed\xb0\x80", 3));
  assert(raw.artificial_start_activity == std::string("\xf0\x9f\x98\x80", 4));
  assert(raw.timestamp_key == std::string("x\0\n\t ", 5));
  const auto encoded = serialize_playout_workbench_options_json(raw);
  assert(serialize_playout_workbench_options_json(parse_dfg_playout_workbench_options(encoded)) == encoded);
  const auto unknown_tree = parse_process_tree_playout_workbench_options(R"({"variant":{"name":"UNKNOWN","index":2147483647}})");
  contains(serialize_playout_workbench_options_json(unknown_tree), R"("variant":{"name":"UNKNOWN","index":2147483647})");
  const auto ieee = parse_stochastic_playout_workbench_options(R"({"start":"NaN","case_interval":"INF","event_interval":"-INF","transition_weights":[-0.0,4.9406564584124654e-324,-1e-999]})");
  assert(std::isnan(ieee.start) && std::isinf(ieee.case_interval) && ieee.event_interval < 0);
  assert(std::signbit(ieee.transition_weights[0]) && std::signbit(ieee.transition_weights[2]));
  assert(ieee.transition_weights[1] == std::numeric_limits<double>::denorm_min());
  const auto ieee_json = serialize_playout_workbench_options_json(ieee);
  contains(ieee_json, R"("start":"NaN","case_interval":"INF","event_interval":"-INF")");
  assert(serialize_playout_workbench_options_json(parse_stochastic_playout_workbench_options(ieee_json)) == ieee_json);
}

void strict_transport_and_transactional_presence() {
  for (const auto* invalid : {"[]", "null", "true", "{}x", "{", "{\"seed\":01}",
       R"({"seed":1,"seed":2})", R"({"seed":1,"\u0073eed":2})",
       R"({"seed":18446744073709551616})", R"({"seed":1e3})", R"({"seed":1.0})",
       R"({"seed":-1})", R"({"seed":-0})", R"({"seed":"1"})", R"({"seed":null})",
       R"({"traces":true})", R"({"start":1e999})", R"({"start":"infinity"})",
       R"({"initial_marking":[4294967296]})", R"({"initial_marking":[-1]})",
       R"({"initial_marking":null})", R"({"initial_marking":[1.0]})",
       R"({"return_visited_elements":1})", R"({"unexpected":0})"}) {
    std::set<std::string> supplied{"unchanged"};
    rejects([&] { (void)parse_playout_workbench_options(invalid, &supplied); });
    assert(supplied == std::set<std::string>{"unchanged"});
  }
  for (const auto* invalid : {R"({"variant":"CLASSIC"})", R"({"variant":0})",
       R"({"variant":{"name":"classic","index":1}})", R"({"variant":{"name":"UNKNOWN","index":0}})",
       R"({"variant":{"name":"classic"}})", R"({"variant":{"name":"classic","index":0,"extra":1}})",
       R"({"variant":{"name":"UNKNOWN","index":2147483648}})", R"({"variant":{"name":"classic","index":0.0}})",
       R"({"activity_key":null})", R"({"artificial_start_activity":false})", R"({"activity_key":"\uZZZZ"})"}) {
    rejects([&] { (void)parse_dfg_playout_workbench_options(invalid); });
  }
  rejects([] { (void)parse_stochastic_playout_workbench_options(R"({"transition_weights":[null]})"); });
  rejects([] { (void)parse_extensive_playout_workbench_options(R"({"max_marking_occ":false})"); });
  rejects([] { (void)parse_process_tree_playout_workbench_options(R"({"variant":"topbottom"})"); });
  rejects([] { (void)parse_dfg_playout_workbench_options(std::string("{\"activity_key\":\"") + char(0xff) + "\"}"); });
  rejects([] { (void)parse_dfg_playout_workbench_options(std::string("{\"activity_key\":\"") + char(1) + "\"}"); });
  rejects([] { (void)parse_dfg_playout_workbench_options(std::string("{\"activity_key\":\"") + std::string("\xed\xa0\x80", 3) + "\"}"); });
  std::string deep = R"({"initial_marking":)";
  for (int i = 0; i < 66; ++i) deep += '[';
  deep += '0';
  for (int i = 0; i < 66; ++i) deep += ']';
  deep += '}';
  rejects([&] { (void)parse_playout_workbench_options(deep); });
}

void actual_petri_calls_keep_separate_log_and_visited_semantics() {
  const auto net = choice_net();
  const auto basic = parse_playout_workbench_options(R"({"traces":3,"seed":18446744073709551615,"initial_marking":[1,0],"final_marking":[0,1],"return_visited_elements":true})");
  const auto log = playout(net, basic);
  const auto visited = playout_visited(net, basic);
  assert(log.traces.size() == 3 && visited.traces.size() == 3);
  assert(visited.traces[0].size() == 3 && visited.traces[0][0].marking == basic.initial_marking);
  assert(visited.traces[0][2].marking == basic.final_marking);
  contains(serialize_playout_workbench_result_json(visited), R"("kind":{"name":"transition","index":1})");
  auto invalid_time = parse_playout_workbench_options(R"({"traces":1,"case_interval":-1})");
  rejects([&] { (void)playout(net, invalid_time); });
  const auto fit = parse_stochastic_playout_workbench_options(R"({"traces":3,"fit_from_log":false,"fitting_max_states":100})");
  assert(fit_stochastic_transition_weights(net, one_a(), fit) == std::vector<double>({1, 0}));
  const auto fitted = stochastic_playout(net, one_a(), fit);
  for (const auto& trace : fitted.traces) assert(trace.events[0].activity == "A");
  const auto fitted_visited = stochastic_playout_visited(net, one_a(), fit);
  for (const auto& trace : fitted_visited.traces) assert(trace[1].transition_index == 0);
  const auto override_fit = parse_stochastic_playout_workbench_options(R"({"traces":2,"transition_weights":[0,1],"fit_from_log":true,"fitting_max_states":0})");
  for (const auto& trace : stochastic_playout(net, one_a(), override_fit).traces)
    assert(trace.events[0].activity == "B");
  const auto empty_fit = parse_stochastic_playout_workbench_options(R"({"fitting_max_states":0})");
  assert(fit_stochastic_transition_weights(net, EventLog{}, empty_fit) == std::vector<double>({1, 1}));
  rejects([&] { (void)fit_stochastic_transition_weights(net, one_a(), empty_fit); });

  // Two distinct firings have the same visible word and final marking.
  // The true visited API retains paths that log-only enumeration collapses.
  PetriNet duplicate;
  const auto source = duplicate.add_place(), sink = duplicate.add_place();
  duplicate.set_source(source); duplicate.set_sink(sink); duplicate.set_activities({"A"});
  duplicate.add_transition(0, {source}, {sink}); duplicate.add_transition(0, {source}, {sink});
  const auto extensive = parse_extensive_playout_workbench_options(R"({"max_length":1,"max_traces":5,"max_states":64,"max_marking_occ":null,"return_elements":true})");
  assert(extensive_playout(duplicate, extensive).traces.size() == 1);
  const auto paths = extensive_playout_visited(duplicate, extensive);
  assert(paths.traces.size() == 2 && !paths.truncated);
  assert(paths.traces[0][1].transition_index != paths.traces[1][1].transition_index);
  const auto zero_occ = parse_extensive_playout_workbench_options(R"({"max_marking_occ":0})");
  rejects([&] { (void)extensive_playout(duplicate, zero_occ); });
}

void declared_dfg_and_original_tree_strategies_reach_native_calls() {
  const DfgFrequency edges{{{"A", "B"}, 1}};
  const DfgActivityCounts starts{{"A", 1}}, ends{{"B", 1}};
  const auto classic = parse_dfg_playout_workbench_options(R"({"variant":"classic","max_steps":1,"max_variants":3,"max_execution_time_ms":0})");
  assert(classic.max_steps == 1 && classic.max_events_per_trace == 1000);
  const auto c = dfg_playout_result(edges, starts, ends, classic);
  assert(c.log.traces.size() == 1 && c.log.traces[0].events.size() == 2);
  assert(c.counts.size() == 1 && c.counts.begin()->first == std::vector<std::string>({"A", "B"}));
  const auto performance = parse_dfg_playout_workbench_options(R"({"variant":"performance","traces":2,"seed":18446744073709551615,"max_steps":1,"case_arrival_rate":3,"activity_key":"visible","timestamp_key":"when","case_id_key":"case"})");
  rejects([&] { (void)dfg_playout_result(edges, starts, ends, performance); });
  DfgPerformance timings;
  timings[{"A", "B"}].value = 2.0;
  const auto p = dfg_playout_result(edges, starts, ends, performance, &timings);
  assert(p.variant == DfgPlayoutVariant::kPerformance && p.log.traces.size() == 2);
  assert(p.log.traces[0].events.size() == 2);
  const auto p_json = serialize_playout_workbench_result_json(p);
  contains(p_json, "\"log\":" + serialize_active_source_log_json(p.log));
  contains(p_json, R"("variant":{"name":"performance","index":1})");
  contains(p_json, R"("completed_traces":)");
  contains(p_json, R"("truncated_traces":)");

  const ProcessTree tree({"A", "B"}, {ProcessTreeOperator::kSequence, 0,
      {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}});
  for (const auto* variant : {"basic", "extensive", "top_bottom"}) {
    const auto options = parse_process_tree_playout_workbench_options(
        std::string("{\"variant\":\"") + variant + "\",\"traces\":2,\"max_length\":2,\"min_length\":2,\"max_traces\":5,\"return_set_strings\":" +
        (std::string_view(variant) == "extensive" ? "true}" : "false}"));
    if (options.variant != ProcessTreePlayoutVariant::kExtensive) {
      auto invalid_set_output = options;
      invalid_set_output.return_set_strings = true;
      rejects([&] { (void)process_tree_playout_result(tree, invalid_set_output); });
    }
    const auto result = process_tree_playout_result(tree, options);
    assert(result.log.traces.size() == (options.variant == ProcessTreePlayoutVariant::kExtensive ? 1u : 2u));
    assert(result.log.traces[0].events.size() == 2);
    assert(result.log.traces[0].events[0].activity == "A" && result.log.traces[0].events[1].activity == "B");
    if (options.variant == ProcessTreePlayoutVariant::kExtensive)
      assert(result.trace_strings == std::vector<std::vector<std::string>>({{"A", "B"}}));
    else assert(result.trace_strings.empty());
    contains(serialize_playout_workbench_result_json(result), "\"log\":" + serialize_active_source_log_json(result.log));
  }
}

void every_stored_result_field_survives_snapshot() {
  PlayoutVisitedResult visited;
  visited.traces = {{
      {PlayoutVisitedElementKind::MARKING, {0, 4294967295u}, std::numeric_limits<std::size_t>::max()},
      {PlayoutVisitedElementKind::TRANSITION, {7, 8}, 17},
      {static_cast<PlayoutVisitedElementKind>(-9), {9}, 19}}, {}};
  visited.truncated = true; visited.trace_limit_reached = true; visited.state_limit_reached = true;
  const auto json = serialize_playout_workbench_result_json(visited);
  contains(json, "\"marking\":[0,4294967295],\"transition_index\":" + std::to_string(std::numeric_limits<std::size_t>::max()));
  contains(json, R"({"kind":{"name":"transition","index":1},"marking":[7,8],"transition_index":17})");
  contains(json, R"({"kind":{"name":"UNKNOWN","index":-9},"marking":[9],"transition_index":19})");
  contains(json, R"("truncated":true,"trace_limit_reached":true,"state_limit_reached":true)");
  EventLog log = one_a();
  log.traces.push_back(Trace{});
  log.attributes.push_back(EventAttribute{"raw", std::string("x\0y", 3)});
  log.traces[0].events[0].input_order = std::numeric_limits<std::size_t>::max();
  log.traces[0].events[0].resource_state = FirstClassAttributeState::kNull;
  log.traces[0].events[0].activity_source_key = " original activity ";
  DfgPlayoutResult dfg;
  dfg.variant = static_cast<DfgPlayoutVariant>(-11); dfg.log = log;
  dfg.counts = {{{}, 0}, {{"A", std::string("\xed\xa0\x80", 3), "A"}, std::numeric_limits<std::uint64_t>::max()}};
  dfg.complete = true; dfg.interrupted = true;
  dfg.completed_traces = std::numeric_limits<std::size_t>::max(); dfg.truncated_traces = 17;
  const auto dfg_json = serialize_playout_workbench_result_json(dfg);
  contains(dfg_json, R"("variant":{"name":"UNKNOWN","index":-11})");
  contains(dfg_json, "\"log\":" + serialize_active_source_log_json(log));
  contains(dfg_json, R"({"activities":[],"count":0})");
  contains(dfg_json, R"("count":18446744073709551615)");
  contains(dfg_json, R"("complete":true,"interrupted":true)");
  contains(dfg_json, "\"completed_traces\":" + std::to_string(std::numeric_limits<std::size_t>::max()));
  contains(dfg_json, R"("truncated_traces":17)");
  ProcessTreePlayoutResult tree;
  tree.log = log; tree.trace_strings = {{}, {"", " A ", std::string("a\0b", 3)}}; tree.truncated = true;
  const auto tree_json = serialize_playout_workbench_result_json(tree);
  contains(tree_json, "\"log\":" + serialize_active_source_log_json(log));
  contains(tree_json, R"("trace_strings":[[],[""," A ","a\u0000b"]],"truncated":true)");
  contains(tree_json, R"("native_state":)");
}

}  // namespace

int main() {
  const auto run = [](const char* name, auto action) {
    std::cerr << "Checking " << name << std::endl;
    try { action(); }
    catch (const std::exception& error) {
      std::cerr << name << " raised: " << error.what() << std::endl;
      throw;
    }
  };
  run("native defaults and all 69 option fields", exact_native_defaults_and_all_69_fields);
  run("enums, IEEE values and literal strings", enums_ieee_and_literal_strings_are_lossless);
  run("strict transport and transactional presence", strict_transport_and_transactional_presence);
  run("separate native Petri log and visited calls", actual_petri_calls_keep_separate_log_and_visited_semantics);
  run("declared DFG and original tree strategies", declared_dfg_and_original_tree_strategies_reach_native_calls);
  run("complete stored result snapshots", every_stored_result_field_survives_snapshot);
}
