#include "minera/monte_carlo_workbench_json.hpp"
#include "minera/active_event_source.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace minera;

template <class Action>
void rejects(Action action) {
  bool rejected = false;
  try { action(); } catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}

void contains(const std::string& actual, std::string_view expected) {
  assert(actual.find(expected) != std::string::npos);
}

PetriNet one_activity_net() {
  PetriNet net;
  const auto source = net.add_place(), sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A"});
  net.add_transition(0, {source}, {sink});
  return net;
}

PetriNet sequence_net() {
  PetriNet net;
  const auto source = net.add_place(), middle = net.add_place(), sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A", "B"});
  net.add_transition(0, {source}, {middle});
  net.add_transition(1, {middle}, {sink});
  return net;
}

void defaults_and_presence_are_native_values() {
  std::set<std::string> supplied{"stale"};
  const auto omitted = parse_monte_carlo_workbench_options("{}", &supplied);
  assert(supplied.empty());
  assert(omitted.simulations == 100 && omitted.max_steps == 1000 && omitted.seed == 1);
  assert(!omitted.case_arrival_ratio && !omitted.case_arrival_seconds);
  assert(omitted.activity_key == "concept:name" && omitted.timestamp_key == "time:timestamp");
  assert(omitted.initial_marking.empty() && omitted.final_marking.empty());
  assert(omitted.transition_weights.empty() && omitted.transition_durations.empty());
  assert(omitted.resources_per_place.empty() && omitted.default_resources_per_place == 1);
  assert(omitted.enable_diagnostics && omitted.stop_at_final);
  assert(!omitted.force_distribution && !omitted.stochastic_map);
  assert(omitted.token_replay_variant == TokenReplayVariant::kForward);
  assert(omitted.diagnostic_interval == 32 && omitted.small_scale_factor == 864000);
  assert(omitted.max_thread_execution_time == 60);
  assert(omitted.place_names.empty() && omitted.transition_names.empty());
  assert(omitted.initial_marking_keys.empty() && omitted.final_marking_keys.empty());

  const auto explicit_empty = parse_monte_carlo_workbench_options(
      R"({"initial_marking":[],"final_marking":[],"initial_marking_keys":[],"final_marking_keys":[],"case_arrival_ratio":null,"force_distribution":null,"stochastic_map":{"transitions":[]}})",
      &supplied);
  assert(supplied == std::set<std::string>({"initial_marking", "final_marking",
      "initial_marking_keys", "final_marking_keys", "case_arrival_ratio",
      "force_distribution", "stochastic_map"}));
  assert(explicit_empty.initial_marking.empty() && explicit_empty.initial_marking_keys.empty());
  assert(explicit_empty.stochastic_map && explicit_empty.stochastic_map->transitions.empty());
  assert(!parse_monte_carlo_workbench_options(R"({"stochastic_map":null})").stochastic_map);
  const auto full = serialize_monte_carlo_workbench_options_json(omitted);
  (void)parse_monte_carlo_workbench_options(full, &supplied);
  assert(supplied.size() == 27);
  assert(serialize_monte_carlo_workbench_options_json(parse_monte_carlo_workbench_options(full)) == full);
  supplied = {"unchanged"};
  rejects([&] { (void)parse_monte_carlo_workbench_options(R"({"seed":false})", &supplied); });
  assert(supplied == std::set<std::string>{"unchanged"});
}

void exact_integer_domains_and_complete_options() {
  MonteCarloOptions source;
  source.simulations = std::numeric_limits<std::size_t>::max();
  source.case_arrival_ratio = -0.0;
  source.case_arrival_seconds = 0.125;
  source.activity_key = std::string(" a\0b ", 6);
  source.timestamp_key = "date";
  source.initial_marking = {0, std::numeric_limits<std::uint32_t>::max()};
  source.final_marking = {std::numeric_limits<std::uint32_t>::max(), 0};
  source.max_steps = std::numeric_limits<std::size_t>::max();
  source.seed = std::numeric_limits<std::uint64_t>::max();
  source.start = -100.125;
  source.default_duration_seconds = 3.5;
  source.transition_weights = {-2.25, -0.0, 0.125};
  source.transition_durations = {{MonteCarloDurationDistribution::kNormal, -1, 2, -3}};
  source.resources_per_place = {{0, 0}, {std::numeric_limits<std::size_t>::max(), std::numeric_limits<std::size_t>::max()}};
  source.default_resources_per_place = std::numeric_limits<std::size_t>::max();
  source.enable_diagnostics = false;
  source.stop_at_final = false;
  source.force_distribution = MonteCarloDurationDistribution::kExponential;
  source.stochastic_map = StochasticMap{{{std::numeric_limits<std::size_t>::max(),
      StochasticMapTiming::kImmediate, StochasticMapDistribution::kGamma,
      -1.5, 2.5, -3.5, -4.5, std::numeric_limits<int>::min()}}};
  source.token_replay_variant = TokenReplayVariant::kBackwards;
  source.diagnostic_interval = std::numeric_limits<std::size_t>::max();
  source.small_scale_factor = 7.25;
  source.max_thread_execution_time = 8.5;
  source.place_names = {"", "  place\n", std::string("p\0q", 3)};
  source.transition_names = {"dup", "dup"};
  source.initial_marking_keys = {true, false, true};
  source.final_marking_keys = {false, true};
  const auto json = serialize_monte_carlo_workbench_options_json(source);
  const auto parsed = parse_monte_carlo_workbench_options(json);
  contains(json, R"("seed":18446744073709551615)");
  contains(json, R"("initial_marking":[0,4294967295])");
  contains(json, R"("case_arrival_ratio":-0.0)");
  assert(parsed.simulations == source.simulations && parsed.max_steps == source.max_steps);
  assert(parsed.seed == source.seed && parsed.initial_marking == source.initial_marking);
  assert(parsed.final_marking == source.final_marking && parsed.resources_per_place == source.resources_per_place);
  assert(parsed.activity_key == source.activity_key && parsed.place_names == source.place_names);
  assert(parsed.transition_names == source.transition_names && parsed.initial_marking_keys == source.initial_marking_keys);
  assert(parsed.final_marking_keys == source.final_marking_keys);
  assert(parsed.stochastic_map->transitions[0].priority == std::numeric_limits<int>::min());
  assert(parsed.stochastic_map->transitions[0].transition == std::numeric_limits<std::size_t>::max());
  assert(parsed.stochastic_map->transitions[0].secondary_parameter == 2.5);
  assert(parsed.stochastic_map->transitions[0].tertiary_parameter == -3.5);
  assert(parsed.stochastic_map->transitions[0].activation_weight == -4.5);
  assert(std::signbit(*parsed.case_arrival_ratio) && std::signbit(parsed.transition_weights[1]));
  assert(serialize_monte_carlo_workbench_options_json(parsed) == json);
  // These exact values reach native validation; transport must not clamp them
  // to apparently executable budgets, discard signed weights, or truncate IDs.
  rejects([&] { (void)monte_carlo(one_activity_net(), parsed); });
}

void ieee_and_literal_string_storage() {
  const auto source = parse_monte_carlo_workbench_options(
      R"({"activity_key":"\u0000a\ud800 \udc00","timestamp_key":"\ud83d\ude00","place_names":[""," a ","\ud800"],"start":-0.0,"case_arrival_ratio":"NaN","case_arrival_seconds":"INF","transition_weights":["-INF",1.7976931348623157e308,4.9406564584124654e-324,-1e-9999],"small_scale_factor":"NaN","max_thread_execution_time":"-INF"})");
  assert(source.activity_key == std::string("\0a\xed\xa0\x80 \xed\xb0\x80", 9));
  assert(source.timestamp_key == "\xf0\x9f\x98\x80");
  assert(std::signbit(source.start) && std::isnan(*source.case_arrival_ratio));
  assert(std::isinf(*source.case_arrival_seconds) && *source.case_arrival_seconds > 0);
  assert(std::isinf(source.transition_weights[0]) && source.transition_weights[0] < 0);
  assert(source.transition_weights[1] == std::numeric_limits<double>::max());
  assert(source.transition_weights[2] == std::numeric_limits<double>::denorm_min());
  assert(source.transition_weights[3] == 0.0 && std::signbit(source.transition_weights[3]));
  const auto json = serialize_monte_carlo_workbench_options_json(source);
  contains(json, R"("activity_key":"\u0000a\ud800 \udc00")");
  contains(json, R"("start":-0.0)");
  contains(json, R"("case_arrival_ratio":"NaN")");
  assert(serialize_monte_carlo_workbench_options_json(parse_monte_carlo_workbench_options(json)) == json);
}

void all_duration_families_and_native_unused_parameter_semantics() {
  const char* records[] = {
      R"({"distribution":"deterministic","value_seconds":2,"secondary_seconds":-3,"tertiary_seconds":-4})",
      R"({"distribution":"exponential","value_seconds":0,"secondary_seconds":2,"tertiary_seconds":-4})",
      R"({"distribution":"constant0","value_seconds":-2,"secondary_seconds":-3,"tertiary_seconds":-4})",
      R"({"distribution":"immediate","value_seconds":-2,"secondary_seconds":-3,"tertiary_seconds":-4})",
      R"({"distribution":"normal","value_seconds":2,"secondary_seconds":0,"tertiary_seconds":-4})",
      R"({"distribution":"uniform","value_seconds":2,"secondary_seconds":0,"tertiary_seconds":-4})",
      R"({"distribution":"lognormal","value_seconds":1,"secondary_seconds":2,"tertiary_seconds":0})",
      R"({"distribution":"gamma","value_seconds":1,"secondary_seconds":2,"tertiary_seconds":0})"};
  for (int index = 0; index < 8; ++index) {
    const auto options = parse_monte_carlo_workbench_options(
        std::string(R"({"simulations":1,"transition_durations":[)") + records[index] + "]}");
    const auto& duration = options.transition_durations[0];
    assert(static_cast<int>(duration.distribution) == index);
    assert(duration.tertiary_seconds == (index < 6 ? -4 : 0));
    const auto result = monte_carlo(one_activity_net(), options);
    assert(result.log.event_count() == 1 && result.diagnostics.completed_cases == 1);
    assert(result.diagnostics.total_cases_seconds == (index == 2 || index == 3 ? 0 : 2));
    const auto roundtrip = parse_monte_carlo_workbench_options(serialize_monte_carlo_workbench_options_json(options));
    assert(roundtrip.transition_durations[0].tertiary_seconds == duration.tertiary_seconds);
  }
  // Native rejects the used negative deterministic value. The same negative
  // stored value is intentionally accepted for immediate/constant0 above.
  const auto invalid = parse_monte_carlo_workbench_options(
      R"({"simulations":1,"transition_durations":[{"distribution":"deterministic","value_seconds":-2}]})");
  rejects([&] { (void)monte_carlo(one_activity_net(), invalid); });
}

void enum_raw_indexes_and_sparse_map_storage() {
  MonteCarloOptions options;
  options.force_distribution = static_cast<MonteCarloDurationDistribution>(-17);
  options.token_replay_variant = static_cast<TokenReplayVariant>(99);
  options.transition_durations = {{static_cast<MonteCarloDurationDistribution>(99), -0.0, -3, -4}};
  for (int index = 0; index < 8; ++index) {
    StochasticMapTransition record;
    record.transition = static_cast<std::size_t>(index * 3);
    record.timing = index % 2 == 0 ? StochasticMapTiming::kTimed : StochasticMapTiming::kImmediate;
    record.distribution = static_cast<StochasticMapDistribution>(index);
    record.parameter = index + 0.25;
    record.secondary_parameter = -index - 0.5;
    record.tertiary_parameter = index + 0.75;
    record.activation_weight = -index - 1.25;
    record.priority = index - 4;
    if (!options.stochastic_map) options.stochastic_map = StochasticMap{};
    options.stochastic_map->transitions.push_back(record);
  }
  options.stochastic_map->transitions[0].timing = static_cast<StochasticMapTiming>(-23);
  options.stochastic_map->transitions[0].distribution = static_cast<StochasticMapDistribution>(99);
  const auto json = serialize_monte_carlo_workbench_options_json(options);
  contains(json, R"("force_distribution":{"name":"UNKNOWN","index":-17})");
  contains(json, R"("token_replay_variant":{"name":"UNKNOWN","index":99})");
  contains(json, R"("timing":{"name":"UNKNOWN","index":-23})");
  const auto parsed = parse_monte_carlo_workbench_options(json);
  assert(static_cast<int>(*parsed.force_distribution) == -17);
  assert(static_cast<int>(parsed.token_replay_variant) == 99);
  assert(parsed.stochastic_map->transitions.size() == 8);
  assert(parsed.stochastic_map->transitions[7].transition == 21);
  assert(parsed.stochastic_map->transitions[7].secondary_parameter == -7.5);
  assert(serialize_monte_carlo_workbench_options_json(parsed) == json);
}

void strict_transport_rejects_unknown_duplicate_and_coerced_values() {
  const std::string documents[] = {
      "", "[]", "null", "{}x", "{,}", R"({"seed":1,})",
      R"({"seed":1,"seed":2})", R"({"seed":1,"\u0073eed":2})",
      R"({"unknown":0})", R"({"seed":-1})", R"({"seed":-0})",
      R"({"seed":1.0})", R"({"seed":1e0})", R"({"seed":"1"})",
      R"({"seed":18446744073709551616})", R"({"seed":01})",
      R"({"initial_marking":[4294967296]})", R"({"initial_marking":[-1]})",
      R"({"final_marking":null})", R"({"simulations":true})",
      R"({"initial_marking_keys":[0]})", R"({"final_marking_keys":[null]})",
      R"({"activity_key":null})", R"({"place_names":[1]})",
      R"({"start":1e9999})", R"({"start":"Infinity"})", R"({"start":true})",
      R"({"force_distribution":"NORMAL"})", R"({"force_distribution":4})",
      R"({"force_distribution":{"name":"normal","index":5}})",
      R"({"force_distribution":{"name":"normal","index":4.0}})",
      R"({"force_distribution":{"name":"normal","index":4,"extra":0}})",
      R"({"force_distribution":{"name":"normal","name":"normal","index":4}})",
      R"({"force_distribution":{"name":"normal"}})",
      R"({"token_replay_variant":{"name":"UNKNOWN","index":0}})",
      R"({"transition_durations":[{"unknown":1}]})",
      R"({"transition_durations":[{"value_seconds":1,"value_seconds":2}]})",
      R"({"transition_durations":[{"secondary_seconds":null}]})",
      R"({"resources_per_place":[]})", R"({"resources_per_place":{"00":1}})",
      R"({"resources_per_place":{"+0":1}})", R"({"resources_per_place":{"-1":1}})",
      R"({"resources_per_place":{"0":1.0}})", R"({"resources_per_place":{"0":1,"0":2}})",
      R"({"resources_per_place":{"18446744073709551616":1}})",
      R"({"stochastic_map":[]})", R"({"stochastic_map":{"unknown":[]}})",
      R"({"stochastic_map":{"transitions":[{"transition":1,"unknown":0}]}})",
      R"({"stochastic_map":{"transitions":[{"priority":2147483648}]}})",
      R"({"stochastic_map":{"transitions":[{"priority":-2147483649}]}})",
      R"({"stochastic_map":{"transitions":[{"priority":1.0}]}})",
      R"({"stochastic_map":{"transitions":[{"timing":"later"}]}})",
      std::string("{\"activity_key\":\"") + "\xed\xa0\x80" + "\"}",
      std::string("{\"activity_key\":\"") + "\xc0\x80" + "\"}"
  };
  for (const auto& document : documents)
    rejects([&] { (void)parse_monte_carlo_workbench_options(document); });
  const std::string too_large(32u * 1024u * 1024u + 1, ' ');
  rejects([&] { (void)parse_monte_carlo_workbench_options(too_large); });
  const std::string nested = std::string(66, '[') + "0" + std::string(66, ']');
  rejects([&] { (void)parse_monte_carlo_workbench_options(nested); });
}

void net_only_and_explicit_empty_log_keep_different_defaults() {
  const auto options = parse_monte_carlo_workbench_options(R"({"simulations":1})");
  const auto net = one_activity_net();
  const auto net_only = monte_carlo(net, options);
  const auto fitted = fit_monte_carlo_options(EventLog{}, net, options);
  assert(!options.stochastic_map && !options.case_arrival_seconds && !options.case_arrival_ratio);
  assert(fitted.stochastic_map && fitted.stochastic_map->transitions.empty());
  assert(fitted.case_arrival_ratio == 0.0 && !fitted.case_arrival_seconds);
  const auto empty_log = monte_carlo(net, fitted);
  assert(net_only.diagnostics.case_arrival_seconds == 60);
  assert(net_only.diagnostics.total_cases_seconds == 1);
  assert(empty_log.diagnostics.case_arrival_seconds == 0);
  assert(empty_log.diagnostics.total_cases_seconds == 0);
  const auto configured_json = serialize_monte_carlo_workbench_options_json(options);
  const auto fitted_json = serialize_monte_carlo_workbench_options_json(fitted);
  contains(configured_json, R"("stochastic_map":null)");
  contains(fitted_json, R"("stochastic_map":{"transitions":[]})");
  assert(serialize_monte_carlo_workbench_options_json(parse_monte_carlo_workbench_options(fitted_json)) == fitted_json);
}

void full_generated_log_and_every_stored_result_field() {
  const auto options = parse_monte_carlo_workbench_options(
      R"({"simulations":2,"case_arrival_seconds":1,"activity_key":"visible","timestamp_key":"when","transition_durations":[{"value_seconds":2},{"value_seconds":3}]})");
  const auto result = monte_carlo(sequence_net(), options);
  assert(result.log.trace_count() == 2 && result.log.event_count() == 4);
  const auto json = serialize_monte_carlo_workbench_result_json(result);
  contains(json, "\"log\":" + serialize_active_source_log_json(result.log));
  contains(json, R"("activity":"A")");
  contains(json, R"("activity":"B")");
  contains(json, R"("activity_source_key":)");
  contains(json, R"("input_order":)");
  contains(json, R"("native_state":)");
  contains(json, R"("diagnostics":{"simulated_cases":2,"completed_cases":2,"deadlocked_cases":0,"step_limited_cases":0,"case_arrival_seconds":1)");
  contains(json, R"("case_execution_seconds":)");
  contains(json, R"("case_arrival_ratio":1)");
  contains(json, R"("total_cases_time":)");
  contains(json, R"("median_cases_ex_time":)");
  contains(json, R"("cases_ex_time":)");
  contains(json, R"("output_places_interval_trees":[{"place_index":0,"intervals":)");
  contains(json, R"("output_transitions_interval_trees":[{"transition_index":0,"intervals":)");
  assert(json.find("\"family\":\"MONTE_CARLO\"") == std::string::npos);
  assert(result.log.traces[0].events[0].activity == "A");
  assert(result.log.traces[0].events[1].activity == "B");

  // Serialization retains stored diagnostics and ordered tree data even when
  // a caller constructs a result outside the scheduler's success invariants.
  // Exact unknown indexes, empty trees, nonfinite values and signed zero are
  // data to inspect, not a reason to erase the otherwise complete result.
  MonteCarloResult raw;
  raw.diagnostics.simulated_cases = std::numeric_limits<std::size_t>::max();
  raw.diagnostics.completed_cases = 2;
  raw.diagnostics.deadlocked_cases = 3;
  raw.diagnostics.step_limited_cases = 4;
  raw.diagnostics.case_arrival_seconds = -0.0;
  raw.diagnostics.total_cases_seconds = std::numeric_limits<double>::quiet_NaN();
  raw.diagnostics.median_case_execution_seconds = std::numeric_limits<double>::infinity();
  raw.diagnostics.case_execution_seconds = {-0.0, -std::numeric_limits<double>::infinity()};
  raw.diagnostics.case_arrival_ratio = 0.125;
  raw.diagnostics.total_cases_time = 0.25;
  raw.diagnostics.median_cases_ex_time = 0.5;
  raw.diagnostics.cases_ex_time = {0.75, 1.25};
  raw.output_places_interval_trees = {{std::numeric_limits<std::size_t>::max(), {}},
      {3, {{-0.0, std::numeric_limits<double>::infinity()}}}};
  raw.output_transitions_interval_trees = {{99, {{2, 1}}}, {0, {}}};
  const auto raw_json = serialize_monte_carlo_workbench_result_json(raw);
  contains(raw_json, "\"simulated_cases\":" + std::to_string(std::numeric_limits<std::size_t>::max()));
  contains(raw_json, R"("case_arrival_seconds":-0.0,"total_cases_seconds":"NaN","median_case_execution_seconds":"INF","case_execution_seconds":[-0.0,"-INF"])");
  contains(raw_json, R"("case_arrival_ratio":0.125,"total_cases_time":0.25,"median_cases_ex_time":0.5,"cases_ex_time":[0.75,1.25])");
  contains(raw_json, R"({"start":-0.0,"end":"INF"})");
  contains(raw_json, R"("output_transitions_interval_trees":[{"transition_index":99,"intervals":[{"start":2,"end":1}]},{"transition_index":0,"intervals":[]}])");
}

}  // namespace

int main() {
  defaults_and_presence_are_native_values();
  exact_integer_domains_and_complete_options();
  ieee_and_literal_string_storage();
  all_duration_families_and_native_unused_parameter_semantics();
  enum_raw_indexes_and_sparse_map_storage();
  strict_transport_rejects_unknown_duplicate_and_coerced_values();
  net_only_and_explicit_empty_log_keep_different_defaults();
  full_generated_log_and_every_stored_result_field();
}
