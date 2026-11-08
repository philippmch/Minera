#include "minera/json_export.hpp"
#include "minera/cancellation.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/playout.hpp"
#include "minera/pnml.hpp"
#include "minera/timestamp.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

minera::PetriNet linear_net() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a"});
  net.add_transition(0, {source}, {sink});
  return net;
}

minera::EventLog source_log(std::initializer_list<std::vector<std::string>> rows) {
  minera::EventLog log;
  std::size_t case_number = 0;
  for (const auto& activities : rows) {
    minera::Trace trace;
    trace.case_id = "source_" + std::to_string(++case_number);
    for (std::size_t i = 0; i < activities.size(); ++i) {
      minera::Event event;
      event.case_id = trace.case_id;
      event.activity = activities[i];
      event.input_order = i;
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

minera::PetriNet branching_net(bool with_unseen_transition = false) {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  const auto dead = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a", "b", "never"});
  net.add_transition(0, {source}, {sink});
  net.add_transition(1, {source}, {sink});
  if (with_unseen_transition) net.add_transition(2, {dead}, {sink});
  return net;
}

std::string activity_signature(const minera::EventLog& log) {
  std::string signature;
  for (const auto& trace : log.traces) {
    signature.push_back('|');
    for (const auto& event : trace.events) signature += event.activity + ',';
  }
  return signature;
}

void deterministic_and_timestamped() {
  const auto net = linear_net();
  minera::StochasticPlayoutOptions options;
  options.traces = 2;
  options.seed = 17;
  options.start = 1767225600.0;
  const auto first = minera::stochastic_playout(net, options);
  const auto second = minera::stochastic_playout(net, options);
  assert(first.traces.size() == 2);
  assert(first.traces[0].events.size() == 1);
  assert(first.traces[0].events[0].timestamp ==
         second.traces[0].events[0].timestamp);
  assert(first.traces[0].events[0].timestamp ==
         "2026-01-01T00:00:00.000Z");
}

void stochastic_visited_records_silent_steps_and_preserves_default() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto middle = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"finish"});
  net.add_transition(std::nullopt, {source}, {middle});
  net.add_transition(0, {middle}, {sink});

  minera::StochasticPlayoutOptions options;
  options.traces = 1;
  options.max_steps = 2;
  options.transition_weights = {1.0, 1.0};
  options.return_visited_elements = true;
  const auto ordinary = minera::stochastic_playout(net, options);
  auto ordinary_without_flag = options;
  ordinary_without_flag.return_visited_elements = false;
  assert(activity_signature(ordinary) ==
         activity_signature(minera::stochastic_playout(net, ordinary_without_flag)));
  const auto visited = minera::stochastic_playout_visited(net, options);
  assert(visited.traces.size() == 1 && visited.traces[0].size() == 5);
  assert(visited.traces[0][0].kind == minera::PlayoutVisitedElementKind::MARKING);
  assert(visited.traces[0][0].marking == std::vector<std::uint32_t>({1, 0, 0}));
  assert(visited.traces[0][1].kind == minera::PlayoutVisitedElementKind::TRANSITION &&
         visited.traces[0][1].transition_index == 0);
  assert(visited.traces[0][2].marking == std::vector<std::uint32_t>({0, 1, 0}));
  assert(visited.traces[0][3].transition_index == 1);
  assert(visited.traces[0][4].marking == std::vector<std::uint32_t>({0, 0, 1}));
  assert(minera::serialize_stochastic_playout_visited_json(net, visited).find(
             R"("variant":"STOCHASTIC_PLAYOUT")") != std::string::npos);
}

void weighted_input_multiplicity_and_silent_steps() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto gate = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a"});
  net.add_transition(std::nullopt, {source}, {gate, gate});
  net.add_transition(0, {gate, gate}, {sink});

  minera::StochasticPlayoutOptions options;
  options.traces = 1;
  options.max_steps = 2;
  const auto log = minera::stochastic_playout(net, options);
  assert(log.traces.size() == 1);
  assert(log.traces[0].events.size() == 1);
  assert(log.traces[0].events[0].activity == "a");
}

void invalid_weights_are_rejected() {
  const auto net = linear_net();
  minera::StochasticPlayoutOptions options;
  options.transition_weights = {1.0, 2.0};
  bool rejected = false;
  try {
    (void)minera::stochastic_playout(net, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void huge_weights_are_scaled_without_overflow() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a", "b"});
  net.add_transition(0, {source}, {sink});
  net.add_transition(1, {source}, {sink});
  minera::StochasticPlayoutOptions options;
  options.traces = 100;
  options.transition_weights = {std::numeric_limits<double>::max(),
                                 std::numeric_limits<double>::max()};
  const auto log = minera::stochastic_playout(net, options);
  bool saw_a = false;
  bool saw_b = false;
  for (const auto& trace : log.traces) {
    assert(trace.events.size() == 1);
    saw_a = saw_a || trace.events.front().activity == "a";
    saw_b = saw_b || trace.events.front().activity == "b";
  }
  assert(saw_a && saw_b);
}

void uniform_playout_honors_weighted_input_arcs() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto gate = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a"});
  net.add_transition(std::nullopt, {source}, {gate, gate});
  net.add_transition(0, {gate, gate}, {sink});
  minera::PlayoutOptions options;
  options.traces = 1;
  options.max_steps = 2;
  const auto log = minera::playout(net, options);
  assert(log.traces.size() == 1);
  assert(log.traces[0].events.size() == 1);
}

void basic_playout_acceptance_controls() {
  const auto net = linear_net();

  minera::PlayoutOptions exact;
  exact.traces = 2;
  exact.max_steps = 1;
  exact.add_only_if_fm_is_reached = true;
  const auto accepted = minera::playout(net, exact);
  assert(accepted.trace_count() == 2);
  assert(accepted.traces.front().events.size() == 1);

  minera::PetriNet unreachable;
  const auto source = unreachable.add_place();
  const auto sink = unreachable.add_place();
  const auto dead = unreachable.add_place();
  unreachable.set_source(source);
  unreachable.set_sink(sink);
  unreachable.set_activities({"dead"});
  unreachable.add_transition(0, {source}, {dead});
  minera::PlayoutOptions require_final;
  require_final.traces = 2;
  require_final.max_steps = 2;
  require_final.add_only_if_fm_is_reached = true;
  assert(minera::playout(unreachable, require_final).traces.empty());

  minera::PetriNet superset;
  const auto superset_source = superset.add_place();
  const auto superset_sink = superset.add_place();
  const auto extra = superset.add_place();
  superset.set_source(superset_source);
  superset.set_sink(superset_sink);
  superset.set_activities({"finish"});
  superset.add_transition(0, {superset_source}, {superset_sink, extra});

  auto exact_only = require_final;
  exact_only.traces = 1;
  exact_only.max_steps = 1;
  exact_only.add_only_if_fm_is_reached = true;
  assert(minera::playout(superset, exact_only).traces.empty());

  auto superset_accepted = exact_only;
  superset_accepted.fm_leq_accepted = true;
  const auto superset_log = minera::playout(superset, superset_accepted);
  assert(superset_log.trace_count() == 1);
  assert(superset_log.traces.front().events.size() == 1);

  const auto repeat = minera::playout(superset, superset_accepted);
  assert(activity_signature(superset_log) == activity_signature(repeat));
}

void visited_playout_records_markings_and_transitions() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto middle = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a"});
  net.add_transition(std::nullopt, {source}, {middle});
  net.add_transition(0, {middle}, {sink});

  minera::PlayoutOptions options;
  options.traces = 1;
  options.max_steps = 2;
  const auto result = minera::playout_visited(net, options);
  assert(result.traces.size() == 1);
  const auto& visited = result.traces.front();
  assert(visited.size() == 5);
  assert(visited[0].kind == minera::PlayoutVisitedElementKind::MARKING);
  assert(visited[0].marking == std::vector<std::uint32_t>({1, 0, 0}));
  assert(visited[1].kind == minera::PlayoutVisitedElementKind::TRANSITION);
  assert(visited[1].transition_index == 0);
  assert(visited[2].kind == minera::PlayoutVisitedElementKind::MARKING);
  assert(visited[2].marking == std::vector<std::uint32_t>({0, 1, 0}));
  assert(visited[3].kind == minera::PlayoutVisitedElementKind::TRANSITION);
  assert(visited[3].transition_index == 1);
  assert(visited[4].kind == minera::PlayoutVisitedElementKind::MARKING);
  assert(visited[4].marking == std::vector<std::uint32_t>({0, 0, 1}));
  assert(minera::serialize_playout_visited_json(net, result) ==
         "{\"family\":\"PETRI_PLAYOUT_VISITED\",\"variant\":\"BASIC_PLAYOUT\",\"truncated\":false,\"trace_limit_reached\":false,\"state_limit_reached\":false,"
         "\"transition_labels\":[null,\"a\"],\"traces\":[[{\"kind\":\"marking\",\"marking\":[1,0,0]},"
         "{\"kind\":\"transition\",\"transition\":0},"
         "{\"kind\":\"marking\",\"marking\":[0,1,0]},"
         "{\"kind\":\"transition\",\"transition\":1},"
         "{\"kind\":\"marking\",\"marking\":[0,0,1]}]]}");
}

void visited_playout_honors_filtering_and_seed() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  const auto dead = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"finish", "dead"});
  net.add_transition(0, {source}, {sink});
  net.add_transition(1, {source}, {dead});

  minera::PlayoutOptions options;
  options.traces = 20;
  options.max_steps = 1;
  options.seed = 7;
  options.add_only_if_fm_is_reached = true;
  const auto first = minera::playout_visited(net, options);
  const auto second = minera::playout_visited(net, options);
  assert(first.traces == second.traces);
  assert(!first.traces.empty());
  for (const auto& visited : first.traces) {
    assert(!visited.empty());
    assert(visited.back().kind == minera::PlayoutVisitedElementKind::MARKING);
    assert(visited.back().marking == std::vector<std::uint32_t>({0, 1, 0}));
  }

  options.add_only_if_fm_is_reached = false;
  const auto unfiltered = minera::playout_visited(net, options);
  assert(unfiltered.traces.size() == options.traces);
}

void basic_playout_timing_is_validated() {
  const auto net = linear_net();
  for (const auto invalid : {std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::infinity()}) {
    minera::PlayoutOptions options;
    options.start = invalid;
    bool rejected = false;
    try {
      (void)minera::playout(net, options);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
    rejected = false;
    try {
      (void)minera::playout_visited(net, options);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }
  minera::PlayoutOptions negative;
  negative.case_interval = -1.0;
  bool rejected = false;
  try {
    (void)minera::playout(net, negative);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  rejected = false;
  try {
    (void)minera::playout_visited(net, negative);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void extensive_playout_accepts_explicit_markers() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto initial = net.add_place();
  const auto final = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a"});
  net.add_transition(0, {initial}, {final});

  minera::ExtensivePlayoutOptions options;
  options.max_traces = 4;
  options.initial_marking = {0, 1, 0, 0};
  options.final_marking = {0, 0, 1, 0};
  const auto generated = minera::extensive_playout(net, options);
  assert(generated.trace_count() == 1);
  assert(generated.traces.front().events.size() == 1);
  assert(generated.traces.front().events.front().activity == "a");

  options.final_marking = {0, 0, 1};
  bool rejected = false;
  try {
    (void)minera::extensive_playout(net, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void extensive_visited_preserves_silent_steps_and_occurrence_bound() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto middle = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"finish"});
  net.add_transition(std::nullopt, {source}, {middle});
  net.add_transition(0, {middle}, {sink});

  minera::ExtensivePlayoutOptions options;
  options.max_traces = 1;
  options.max_length = 1;
  options.return_elements = true;
  const auto result = minera::extensive_playout_visited(net, options);
  assert(result.traces.size() == 1);
  assert(!result.truncated);
  assert(!result.trace_limit_reached);
  assert(!result.state_limit_reached);
  const auto& trace = result.traces.front();
  assert(trace.size() == 5);
  assert(trace[0].kind == minera::PlayoutVisitedElementKind::MARKING);
  assert(trace[0].marking == std::vector<std::uint32_t>({1, 0, 0}));
  assert(trace[1].kind == minera::PlayoutVisitedElementKind::TRANSITION);
  assert(trace[1].transition_index == 0);
  assert(trace[2].marking == std::vector<std::uint32_t>({0, 1, 0}));
  assert(trace[3].transition_index == 1);
  assert(trace[4].marking == std::vector<std::uint32_t>({0, 0, 1}));
  assert(minera::serialize_extensive_playout_visited_json(net, result)
             .find("\"variant\":\"EXTENSIVE_PLAYOUT\"") !=
         std::string::npos);
  assert(minera::serialize_extensive_playout_visited_json(net, result)
             .find("\"truncated\":false") != std::string::npos);

  minera::PetriNet loop;
  const auto loop_source = loop.add_place();
  const auto loop_sink = loop.add_place();
  loop.set_source(loop_source);
  loop.set_sink(loop_sink);
  loop.set_activities({"loop", "finish"});
  loop.add_transition(0, {loop_source}, {loop_source});
  loop.add_transition(1, {loop_source}, {loop_sink});
  options.max_traces = 1;
  options.max_length = 2;
  options.max_marking_occ.reset();
  const auto trace_limited = minera::extensive_playout_visited(loop, options);
  assert(trace_limited.traces.size() == 1);
  assert(trace_limited.truncated);
  assert(trace_limited.trace_limit_reached);
  assert(!trace_limited.state_limit_reached);

  options.max_traces = 2;
  options.max_length = 1;
  options.max_marking_occ.reset();
  const auto unbounded = minera::extensive_playout(loop, options);
  assert(unbounded.traces.size() == 1);
  options.max_length = 2;
  const auto unbounded_with_loop = minera::extensive_playout(loop, options);
  assert(unbounded_with_loop.traces.size() == 2);
  options.max_marking_occ = 1;
  const auto bounded = minera::extensive_playout_visited(loop, options);
  assert(bounded.traces.size() == 1);
  const auto bounded_log = minera::extensive_playout(loop, options);
  assert(bounded_log.traces.size() == 1);
  options.max_marking_occ = 0;
  bool rejected = false;
  try {
    (void)minera::extensive_playout_visited(loop, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void sampled_variants_accept_explicit_markers() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto initial = net.add_place();
  const auto final = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a"});
  net.add_transition(0, {initial}, {final});

  minera::PlayoutOptions basic;
  basic.traces = 1;
  basic.max_steps = 1;
  basic.initial_marking = {0, 1, 0, 0};
  basic.final_marking = {0, 0, 1, 0};
  const auto basic_log = minera::playout(net, basic);
  assert(basic_log.trace_count() == 1);
  assert(basic_log.traces.front().events.size() == 1);

  minera::StochasticPlayoutOptions stochastic;
  stochastic.traces = 1;
  stochastic.max_steps = 1;
  stochastic.initial_marking = {0, 1, 0, 0};
  stochastic.final_marking = {0, 0, 1, 0};
  const auto stochastic_log = minera::stochastic_playout(net, stochastic);
  assert(stochastic_log.trace_count() == 1);
  assert(stochastic_log.traces.front().events.size() == 1);

  basic.final_marking = {0, 0, 1};
  bool rejected = false;
  try {
    (void)minera::playout(net, basic);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void fitting_tracks_reachable_rates_and_silent_routing() {
  const auto net = branching_net(true);
  const auto fitted = minera::fit_stochastic_transition_weights(
      net, source_log({{"a"}}));
  assert(fitted.size() == 3);
  assert(fitted[0] == 1.0);
  assert(fitted[1] == 0.0);  // reachable, enabled, but never selected
  assert(fitted[2] == 1.0);  // never enabled: retain neutral weight

  minera::PetriNet routed;
  const auto source = routed.add_place();
  const auto middle = routed.add_place();
  const auto sink = routed.add_place();
  routed.set_source(source);
  routed.set_sink(sink);
  routed.set_activities({"a"});
  routed.add_transition(std::nullopt, {source}, {middle});
  routed.add_transition(0, {middle}, {sink});
  const auto routed_weights = minera::fit_stochastic_transition_weights(
      routed, source_log({{"a"}}));
  assert(routed_weights.size() == 2);
  assert(routed_weights[0] == 1.0 && routed_weights[1] == 1.0);
}

void fitting_handles_duplicate_labels_unknowns_and_empty_logs() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto middle = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"a"});
  net.add_transition(std::nullopt, {source}, {middle});
  net.add_transition(0, {middle}, {sink});
  net.add_transition(0, {middle}, {sink});

  const auto weights = minera::fit_stochastic_transition_weights(
      net, source_log({{"unknown", "a"}}));
  // The token replay fires one of the two equal "a" transitions; the
  // reference's choice follows its transition-set order (observed: t2).
  assert(weights[0] == 1.0);
  assert((weights[1] == 1.0 && weights[2] == 0.0) ||
         (weights[1] == 0.0 && weights[2] == 1.0));

  const auto empty = minera::fit_stochastic_transition_weights(
      net, minera::EventLog{});
  assert(empty.size() == 3);
  for (const auto weight : empty) assert(weight == 1.0);

  auto malformed = net;
  malformed.set_source(99);
  bool rejected = false;
  try {
    (void)minera::fit_stochastic_transition_weights(
        malformed, source_log({{"a"}}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void fitting_uses_activation_ratios_and_seeded_precedence() {
  const auto net = branching_net();
  const auto fitted = minera::fit_stochastic_transition_weights(
      net, source_log({{"a"}, {"b"}}));
  assert(fitted[0] == 0.5 && fitted[1] == 0.5);

  minera::StochasticPlayoutOptions options;
  options.traces = 64;
  options.seed = 73;
  const auto first = minera::stochastic_playout(net, source_log({{"a"}, {"b"}}),
                                                options);
  const auto second = minera::stochastic_playout(net, source_log({{"a"}, {"b"}}),
                                                 options);
  assert(activity_signature(first) == activity_signature(second));

  bool varied = false;
  const auto baseline = activity_signature(first);
  for (std::uint64_t seed = 1; seed < 9; ++seed) {
    options.seed = seed;
    if (activity_signature(minera::stochastic_playout(
            net, source_log({{"a"}, {"b"}}), options)) != baseline) {
      varied = true;
      break;
    }
  }
  assert(varied);

  options.seed = 1;
  options.fit_from_log = true;
  options.transition_weights = {0.0, 1.0};
  const auto explicit_log = minera::stochastic_playout(
      net, source_log({{"a"}}), options);
  for (const auto& trace : explicit_log.traces) {
    assert(trace.events.size() == 1);
    assert(trace.events.front().activity == "b");
  }
}

void fitting_replay_budget_is_validated() {
  const auto net = linear_net();
  minera::StochasticPlayoutOptions options;
  options.fitting_max_states = 0;
  bool rejected = false;
  try {
    (void)minera::fit_stochastic_transition_weights(
        net, source_log({{"a"}}), options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

// Regression: the map fitter read only concept:name, so a CSV source (its
// "activity" column) or a custom activity selector failed with KeyError
// 'concept:name'. The reference get_map_from_log_and_net with the source's
// activity key gives these weights (black-box, pm4py 2.7.22).
void fitting_reads_the_source_log_activity_key() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A", "B"});
  net.add_transition(0, {source}, {sink});
  net.add_transition(1, {source}, {sink});

  std::istringstream csv(
      "case_id,activity,timestamp\n"
      "a,A,2026-01-01T00:00:00Z\n"
      "b,A,2026-01-01T00:00:01Z\n"
      "c,A,2026-01-01T00:00:02Z\n"
      "d,B,2026-01-01T00:00:03Z\n");
  const auto csv_log = minera::import_csv_event_log(csv);
  assert(csv_log.traces.front().events.front().activity_source_key == "activity");
  const auto csv_weights = minera::fit_stochastic_transition_weights(net, csv_log);
  assert(csv_weights.size() == 2);
  assert(csv_weights[0] == 0.75 && csv_weights[1] == 0.25);
  minera::StochasticPlayoutOptions options;
  options.traces = 2;
  options.max_steps = 8;
  assert(minera::stochastic_playout(net, csv_log, options).trace_count() == 2);
  assert(minera::stochastic_playout_visited(net, csv_log, options).traces.size() == 2);

  // A custom selector kept as the selected attribute (the table adapter).
  const std::string key(" task \xed\xa0\x80 \0 ", 12);
  minera::EventLog custom;
  for (int number = 0; number < 3; ++number) {
    minera::Trace trace;
    trace.case_id = "case_" + std::to_string(number);
    for (const char* label : {"A", "B"}) {
      minera::Event event;
      event.case_id = trace.case_id;
      event.activity = label;
      event.activity_state = minera::FirstClassAttributeState::kPresent;
      event.activity_source_key = key;
      minera::EventAttribute selected;
      selected.key = key;
      selected.value = label;
      event.activity_attribute = selected;
      trace.events.push_back(std::move(event));
    }
    custom.traces.push_back(std::move(trace));
  }
  const auto custom_weights = minera::fit_stochastic_transition_weights(net, custom);
  assert(custom_weights[0] == 1.0 && custom_weights[1] == 0.5);
}

// The reference BASIC_PLAYOUT process and envelope (invariant-only; the
// reference draws from a set of transition objects).
void basic_playout_reference_semantics() {
  // Envelope: cases "0".., epoch 10,000,000, one second per event across
  // cases; explicit timing is the declared add_envelope extension.
  minera::PetriNet two;
  {
    const auto source = two.add_place();
    const auto middle = two.add_place();
    const auto sink = two.add_place();
    two.set_source(source);
    two.set_sink(sink);
    two.set_activities({"a", "b"});
    two.add_transition(0, {source}, {middle});
    two.add_transition(1, {middle}, {sink});
  }
  minera::PlayoutOptions envelope;
  envelope.traces = 3;
  const auto reference = minera::playout(two, envelope);
  assert(reference.trace_count() == 3);
  for (std::size_t i = 0; i < 3; ++i) {
    assert(reference.traces[i].case_id == std::to_string(i));
    assert(reference.traces[i].events.size() == 2);
    for (std::size_t j = 0; j < 2; ++j)
      assert(reference.traces[i].events[j].timestamp ==
             minera::format_timestamp(10000000.0 + static_cast<double>(2 * i + j)));
  }
  envelope.add_envelope = true;
  const auto explicit_timing = minera::playout(two, envelope);
  assert(explicit_timing.traces[1].case_id == "case_2");
  assert(explicit_timing.traces[1].events[1].timestamp ==
         minera::format_timestamp(envelope.start + envelope.case_interval + envelope.event_interval));

  // In the final marking "stop" is one more option: with one enabled
  // self-loop there, a case stops right after "a" with probability 1/2.
  minera::PetriNet looping;
  const auto lsource = looping.add_place();
  const auto lsink = looping.add_place();
  looping.set_source(lsource);
  looping.set_sink(lsink);
  looping.set_activities({"a", "c", "s"});
  looping.add_transition(0, {lsource}, {lsink});
  looping.add_transition(1, {lsink}, {lsink});
  minera::PlayoutOptions stop;
  stop.traces = 20000;
  stop.seed = 11;
  std::size_t only_a = 0;
  for (const auto& trace : minera::playout(looping, stop).traces) {
    assert(trace.events.front().activity == "a");
    if (trace.events.size() == 1) ++only_a;
  }
  assert(only_a > 9700 && only_a < 10300);

  // Transitions without input places are always enabled.
  minera::PetriNet generator;
  const auto gq0 = generator.add_place();
  const auto gq1 = generator.add_place();
  generator.set_source(gq0);
  generator.set_sink(gq1);
  generator.set_activities({"g", "h"});
  generator.add_transition(0, {}, {gq0});
  generator.add_transition(1, {gq0}, {gq1});
  minera::PlayoutOptions free;
  free.traces = 200;
  free.max_steps = 5;
  free.initial_marking = {0, 0};
  bool repeated_generator = false;
  for (const auto& trace : minera::playout(generator, free).traces) {
    assert(trace.events.front().activity == "g");
    assert(trace.events.size() <= 5);
    if (trace.events.size() > 1 && trace.events[1].activity == "g") repeated_generator = true;
  }
  assert(repeated_generator);

  // max_steps counts visible events; silent firings do not count, and a
  // case cut at the bound is kept without add_only_if_fm_is_reached.
  minera::PetriNet capped;
  const auto cp0 = capped.add_place();
  const auto cp1 = capped.add_place();
  capped.set_source(cp0);
  capped.set_sink(cp1);
  capped.set_activities({"a", "b"});
  capped.add_transition(0, {cp0}, {cp0});
  capped.add_transition(std::nullopt, {cp0}, {cp0});
  capped.add_transition(1, {cp0}, {cp1});
  minera::PlayoutOptions bounded;
  bounded.traces = 3000;
  bounded.max_steps = 4;
  bool cut_kept = false;
  std::size_t longest = 0;
  for (const auto& trace : minera::playout(capped, bounded).traces) {
    longest = std::max(longest, trace.events.size());
    if (trace.events.size() == 4 && trace.events.back().activity == "a") cut_kept = true;
  }
  assert(longest == 4 && cut_kept);

  // add_only_if_fm_is_reached samples on until `traces` are accepted ...
  bounded.add_only_if_fm_is_reached = true;
  const auto accepted = minera::playout(capped, bounded);
  assert(accepted.trace_count() == 3000);
  for (std::size_t i = 0; i < accepted.traces.size(); ++i) {
    assert(accepted.traces[i].case_id == std::to_string(i));
    assert(accepted.traces[i].events.back().activity == "b");
  }
  // ... and raises past its attempt bound instead of returning fewer.
  bounded.max_attempts = 3000;
  bool raised = false;
  try {
    (void)minera::playout(capped, bounded);
  } catch (const std::runtime_error&) {
    raised = true;
  }
  assert(raised);

  // A silent livelock raises at the firing bound.
  minera::PetriNet livelock;
  const auto vp0 = livelock.add_place();
  const auto vp1 = livelock.add_place();
  livelock.set_source(vp0);
  livelock.set_sink(vp1);
  livelock.add_transition(std::nullopt, {vp0}, {vp0});
  minera::PlayoutOptions spin;
  spin.traces = 1;
  spin.max_firings_per_trace = 100;
  raised = false;
  try {
    (void)minera::playout(livelock, spin);
  } catch (const std::runtime_error&) {
    raised = true;
  }
  assert(raised);
}

}  // namespace

// Reference capture (pm4py 2.7.22): c consumes start, f only produces end.
// A transition without input places is always enabled, so EXTENSIVE yields
// {(c,f),(f,c)} at max_trace_length 2, 3 and 4, and STOCHASTIC with equal
// weights fires f before c in some cases and stops on the exact final marking.
void inputless_transitions_are_always_enabled() {
  minera::PetriNet net;
  const auto start = net.add_place();
  const auto end = net.add_place();
  net.set_source(start);
  net.set_sink(end);
  net.set_activities({"c", "f"});
  net.add_transition(0, {start}, {});
  net.add_transition(1, {}, {end});
  for (const std::size_t length : {2, 3, 4}) {
    minera::ExtensivePlayoutOptions options;
    options.max_length = length;
    options.initial_marking = {1, 0};
    options.final_marking = {0, 1};
    std::set<std::string> words;
    for (const auto& trace : minera::extensive_playout(net, options).traces) {
      std::string word;
      for (const auto& event : trace.events) word += event.activity;
      words.insert(word);
    }
    assert((words == std::set<std::string>{"cf", "fc"}));
  }
  minera::StochasticPlayoutOptions options;
  options.traces = 200;
  options.max_steps = 50;
  options.seed = 3;
  const auto log = minera::stochastic_playout(net, options);
  bool f_first = false, exact_cf = false;
  for (const auto& trace : log.traces) {
    assert(!trace.events.empty());
    f_first = f_first || trace.events.front().activity == "f";
    exact_cf = exact_cf || (trace.events.size() == 2 && trace.events[0].activity == "c" &&
                            trace.events[1].activity == "f");
    assert(!(trace.events.size() == 1 && trace.events[0].activity == "c"));
  }
  assert(f_first && exact_cf);
  // The source-log fit is get_map_from_log_and_net's: activations over
  // enablings along the token-replay path, where a firing that leaves the
  // marking empty ends the trace's statistics. In "cf", c empties the
  // marking, so f is counted enabled once and never activated; in "fc" both
  // firings count. Reference: c 2/3, f 1/3 (native gave f 0.5 before).
  minera::EventLog source;
  for (const auto* words : {"cf", "fc"}) {
    minera::Trace trace;
    trace.case_id = words;
    for (const char* c = words; *c; ++c) {
      minera::Event event;
      event.case_id = trace.case_id;
      event.activity = std::string(1, *c);
      trace.events.push_back(event);
    }
    source.traces.push_back(trace);
  }
  const auto weights = minera::fit_stochastic_transition_weights(net, source);
  assert(std::abs(weights[0] - 2.0 / 3.0) < 1e-12 &&
         std::abs(weights[1] - 1.0 / 3.0) < 1e-12);
}

// Reference stochastic playout (pm4py 2.7.22, observed by distribution): in
// the final marking "stop" is an option of weight 1.0 beside the enabled
// transitions, and maxTraceLength counts visible events only.
void stochastic_final_marking_stop_and_visible_cap() {
  minera::PetriNet net;
  const auto start = net.add_place();
  const auto end = net.add_place();
  net.set_source(start);
  net.set_sink(end);
  net.set_activities({"c", "f"});
  net.add_transition(0, {start}, {end});
  net.add_transition(1, {}, {});
  minera::StochasticPlayoutOptions options;
  options.traces = 20000;
  options.max_steps = 1;
  options.initial_marking = {0, 1};
  options.final_marking = {0, 1};
  // f weight 0.25 against stop 1.0: a case stops at once with probability 0.8.
  options.transition_weights = {1.0, 0.25};
  std::size_t empty = 0;
  for (const auto& trace : minera::stochastic_playout(net, options).traces)
    if (trace.events.empty()) ++empty;
  assert(empty > 15700 && empty < 16300);
  options.transition_weights = {1.0, 0.0};
  for (const auto& trace : minera::stochastic_playout(net, options).traces)
    assert(trace.events.empty());

  // A silent self-loop does not use up the visible-event cap.
  minera::PetriNet looped;
  const auto lp0 = looped.add_place();
  const auto lp1 = looped.add_place();
  looped.set_source(lp0);
  looped.set_sink(lp1);
  looped.set_activities({"c"});
  looped.add_transition(std::nullopt, {lp0}, {lp0});
  looped.add_transition(0, {lp0}, {lp1});
  minera::StochasticPlayoutOptions capped;
  capped.traces = 500;
  capped.max_steps = 1;
  capped.transition_weights = {3.0, 1.0};
  for (const auto& trace : minera::stochastic_playout(looped, capped).traces)
    assert(trace.events.size() == 1 && trace.events.front().activity == "c");
}

// loop.pnml: a silent self-loop beside a visible one. With weights 1, 500,
// 1, 0 a case of max_steps visible events fires about 500 times per visible
// event (silent firings do not count towards max_steps), so a run that the
// visible-event budget allows can be huge. The run-wide firing bound counts
// every firing, the visited bound counts marking entries, and the
// cancellation callback stops the loop; all are unset by default.
void stochastic_run_bounds_count_silent_firings() {
  const auto net = minera::parse_pnml(R"(<?xml version="1.0" encoding="UTF-8"?>
<pnml><net id="n" type="http://www.pnml.org/version-2009/grammar/pnmlcoremodel"><page id="pg">
<place id="source"><name><text>source</text></name><initialMarking><text>1</text></initialMarking></place>
<place id="p1"><name><text>p1</text></name></place>
<place id="sink"><name><text>sink</text></name></place>
<transition id="ta"><name><text>A</text></name></transition>
<transition id="tt"><name><text>tau</text></name><toolspecific tool="ProM" version="6.4" activity="$invisible$" localNodeID="x"/></transition>
<transition id="tb"><name><text>B</text></name></transition>
<transition id="te"><name><text>E</text></name></transition>
<arc id="a1" source="source" target="ta"/><arc id="a2" source="ta" target="p1"/>
<arc id="a3" source="p1" target="tt"/><arc id="a4" source="tt" target="p1"/>
<arc id="a5" source="p1" target="tb"/><arc id="a6" source="tb" target="p1"/>
<arc id="a7" source="p1" target="te"/><arc id="a8" source="te" target="sink"/>
</page><finalmarkings><marking><place idref="sink"><text>1</text></place></marking></finalmarkings></net></pnml>)");
  assert(net.transitions().size() == 4 && net.transitions()[1].silent());
  minera::StochasticPlayoutOptions options;
  options.traces = 2;
  options.max_steps = 100;
  options.transition_weights = {1.0, 500.0, 1.0, 0.0};
  // Unbounded by default (reference-like): visible events stop the case.
  const auto unbounded = minera::stochastic_playout(net, options);
  for (const auto& trace : unbounded.traces) assert(trace.events.size() == 100);
  const auto visited = minera::stochastic_playout_visited(net, options);
  assert(visited.traces.front().size() > 10000);  // silent steps recorded

  // The run-wide firing bound counts silent firings.
  options.max_total_firings = 5000;
  bool refused = false;
  try {
    (void)minera::stochastic_playout(net, options);
  } catch (const std::length_error& error) {
    refused = std::string(error.what()).find("more than 5000 transitions") != std::string::npos;
  }
  assert(refused);
  // A bound the run stays within changes nothing (same seed, same log).
  options.max_total_firings = 10000000;
  const auto within = minera::stochastic_playout(net, options);
  assert(within.traces.size() == unbounded.traces.size());
  for (std::size_t i = 0; i < within.traces.size(); ++i)
    assert(within.traces[i].events.size() == unbounded.traces[i].events.size());

  // Visited marking entries are bounded (three places per marking).
  options.max_total_firings = 0;
  options.max_visited_marking_entries = 3000;
  refused = false;
  try {
    (void)minera::stochastic_playout_visited(net, options);
  } catch (const std::length_error&) {
    refused = true;
  }
  assert(refused);
  options.max_visited_marking_entries = 0;

  // The visited bytes bound sees every buffer before it is allocated (a
  // growing buffer counts its old and new allocation), so a run never holds
  // more, and a bound the run stays within changes nothing (same elements).
  const auto held = minera::playout_visited_bytes(visited);
  assert(held > visited.traces.front().size() * sizeof(minera::PlayoutVisitedElement));
  options.max_visited_bytes = held * 2;
  const auto exact = minera::stochastic_playout_visited(net, options);
  assert(exact.traces == visited.traces && minera::playout_visited_bytes(exact) <= held);
  options.max_visited_bytes = held / 2;
  refused = false;
  try {
    (void)minera::stochastic_playout_visited(net, options);
  } catch (const std::length_error& error) {
    refused = std::string(error.what()).find("bytes") != std::string::npos;
  }
  assert(refused);
  options.max_visited_bytes = 0;
  // The visited serialization is sized exactly.
  const auto document = minera::serialize_stochastic_playout_visited_json(net, visited);
  assert(document.size() == minera::stochastic_playout_visited_json_bytes(net, visited));
  assert(document.capacity() == document.size());

  // A cancellation callback stops the run (polled every 256 checks).
  std::size_t polls = 0;
  options.cancelled = [&polls] { return ++polls > 3; };
  bool cancelled = false;
  try {
    (void)minera::stochastic_playout(net, options);
  } catch (const minera::OperationCancelled&) {
    cancelled = true;
  }
  assert(cancelled && polls == 4);
}

int main() {
  stochastic_run_bounds_count_silent_firings();
  deterministic_and_timestamped();
  stochastic_visited_records_silent_steps_and_preserves_default();
  weighted_input_multiplicity_and_silent_steps();
  invalid_weights_are_rejected();
  huge_weights_are_scaled_without_overflow();
  uniform_playout_honors_weighted_input_arcs();
  basic_playout_acceptance_controls();
  visited_playout_records_markings_and_transitions();
  visited_playout_honors_filtering_and_seed();
  basic_playout_timing_is_validated();
  basic_playout_reference_semantics();
  extensive_playout_accepts_explicit_markers();
  extensive_visited_preserves_silent_steps_and_occurrence_bound();
  inputless_transitions_are_always_enabled();
  stochastic_final_marking_stop_and_visible_cap();
  sampled_variants_accept_explicit_markers();
  fitting_tracks_reachable_rates_and_silent_routing();
  fitting_handles_duplicate_labels_unknowns_and_empty_logs();
  fitting_uses_activation_ratios_and_seeded_precedence();
  fitting_replay_budget_is_validated();
  fitting_reads_the_source_log_activity_key();
}
