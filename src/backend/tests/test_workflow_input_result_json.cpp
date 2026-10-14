#undef NDEBUG
#include "minera/workflow_input_result_json.hpp"
#include "minera/workflow_discovery_input_json.hpp"
#include "minera/alpha_miner.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

using namespace minera;

namespace {

void contains(const std::string& json, const std::string& fragment) {
  assert(json.find(fragment) != std::string::npos);
}

void petri_empty_and_optional_boundaries() {
  PetriNet empty;
  assert(workflow_input_petri_json(empty) ==
      R"({"activity_names":[],"place_count":0,"has_source":false,"has_sink":false,"source":null,"sink":null,"transitions":[],"discovery_markings":null})");
  empty.add_place();
  empty.set_source(0);
  empty.set_sink(0);  // A snapshot does not impose distinct workflow boundaries.
  empty.set_discovery_markings({});
  const auto json = workflow_input_petri_json(empty);
  contains(json, R"("has_source":true,"has_sink":true,"source":0,"sink":0)");
  contains(json, R"("discovery_markings":{"detached_places":[],"initial":[],"final":[]})");
}

void petri_preserves_duplicate_labels_silent_names_weights_and_markings() {
  PetriNet net;
  for (int index = 0; index < 4; ++index) net.add_place();
  net.set_activities({"duplicate", "duplicate", "", "東京\nquoted\""});
  const auto first = net.add_transition(0, {0, 0}, {1});
  net.add_transition(1, {1}, {2});
  net.add_transition(std::nullopt, {}, {});
  net.add_transition(2, {}, {});
  net.set_transition_names({std::string(""), std::nullopt, std::string("silent 東京"), std::string("display-empty")});
  const TokenInteger huge("1234567890123456789012345678901234567890");
  net.set_ordinary_arc_weights(first, {{huge, true}, {0, false}}, {{1, false}});
  net.add_reset_arc(first, 2, huge, true);
  net.add_inhibitor_arc(first, 3, 0);
  PetriDiscoveryMarkings markings;
  markings.detached_places = {{0, HeuristicStructuredValue{std::string("same")}},
                             {7, HeuristicStructuredValue{std::string("same")}}};
  markings.initial = {{{PetriMarkingPlaceKind::kRoot, 0}, 0},
                      {{PetriMarkingPlaceKind::kDetached, 0}, huge},
                      {{PetriMarkingPlaceKind::kDetached, 7}, -9}};
  markings.final = {{{PetriMarkingPlaceKind::kDetached, 0}, -huge},
                    {{PetriMarkingPlaceKind::kRoot, 1}, 1}};
  net.set_discovery_markings(markings);
  const auto json = workflow_input_petri_json(net);
  contains(json, R"("activity_names":["duplicate","duplicate","","東京\u000aquoted\""])");
  contains(json, R"("index":0,"activity_id":0,"activity_name":"duplicate","activity_is_null":false,"display_name_supplied":true,"display_name":"")");
  contains(json, R"("index":1,"activity_id":1,"activity_name":"duplicate","activity_is_null":false,"display_name_supplied":true,"display_name":null)");
  contains(json, R"("index":2,"activity_id":null,"activity_name":null,"activity_is_null":true,"display_name_supplied":true,"display_name":"silent 東京")");
  contains(json, R"("index":3,"activity_id":2,"activity_name":"","activity_is_null":false)");
  contains(json, R"("inputs":[0,0],"outputs":[1],"reset_inputs":[2],"inhibitor_inputs":[3])");
  contains(json, R"("input_weights":[{"negative":true,"magnitude":1234567890123456789012345678901234567890},{"negative":false,"magnitude":0}])");
  contains(json, R"("output_weights":[{"negative":false,"magnitude":1}])");
  contains(json, R"("reset_weights":[{"negative":true,"magnitude":1234567890123456789012345678901234567890}])");
  contains(json, R"("inhibitor_weights":[{"negative":false,"magnitude":0}])");
  contains(json, R"("inputs":[1],"outputs":[2],"reset_inputs":[],"inhibitor_inputs":[],"input_weights":[],"output_weights":[])");
  contains(json, R"("namespace":"root","place_id":0,"tokens":0)");
  contains(json, R"("namespace":"detached","place_id":0,"tokens":1234567890123456789012345678901234567890)");
  contains(json, R"("namespace":"detached","place_id":7,"tokens":-9)");
  contains(json, R"("namespace":"detached","place_id":0,"tokens":-1234567890123456789012345678901234567890)");
  assert(json.find('"' + huge.decimal() + '"') == std::string::npos);
}

void petri_real_alpha_plus_detached_and_native_null_activity() {
  EventLog log;
  Trace trace; trace.case_id = "case";
  trace.events = {{"case", "loop"}, {"case", "loop"}};
  log.traces.push_back(trace);
  const auto net = discover_alpha_plus(log);
  assert(!net.has_source() && net.has_sink() && net.discovery_markings());
  const auto json = workflow_input_petri_json(net);
  contains(json, R"("has_source":false,"has_sink":true,"source":null)");
  contains(json, R"("namespace":"detached")");
  contains(json, serialize_workflow_structured_value_json(net.discovery_markings()->detached_places.front().name));

  log.traces.front().events = {{"case", ""}};
  log.traces.front().events.front().activity_state = FirstClassAttributeState::kNull;
  const auto nullable = discover_alpha_classic(log);
  const auto null_json = workflow_input_petri_json(nullable);
  contains(null_json, R"("activity_id":null,"activity_name":null,"activity_is_null":true)");
}

void petri_preserves_typed_recursive_names_and_wtf8() {
  const auto typed = parse_workflow_structured_value_json(
      R"({"type":"object","value":{"i":{"type":"int64","value":-9223372036854775808},"u":{"type":"uint64","value":18446744073709551615},"f":{"type":"float","value":"NaN"},"b":{"type":"boolean","value":true},"n":{"type":"null","value":null},"s":{"type":"string","value":"\ud800"},"a":{"type":"array","value":[]}}})");
  PetriNet net;
  net.set_activities({std::string("\xed\xa0\x80", 3), std::string("\xed\xb0\x80", 3)});
  net.set_discovery_markings({{{8, typed}}, {}, {}});
  const auto json = workflow_input_petri_json(net);
  contains(json, R"("activity_names":["\ud800","\udc00"])");
  contains(json, serialize_workflow_structured_value_json(typed));
  contains(json, R"("type":"int64","value":-9223372036854775808)");
  contains(json, R"("type":"uint64","value":18446744073709551615)");
  contains(json, R"("type":"float","value":"NaN")");
  assert(json.find(std::string("\xed\xa0\x80", 3)) == std::string::npos);

  HeuristicStructuredValue deep{std::string("leaf")};
  for (int index = 0; index < 70; ++index)
    deep = HeuristicStructuredValue{HeuristicStructuredValue::Array{std::move(deep)}};
  net.set_discovery_markings({{{9, deep}}, {}, {}});
  contains(workflow_input_petri_json(net), "leaf");
}

void petri_rejects_only_invalid_native_indices() {
  for (int kind = 0; kind < 4; ++kind) {
    PetriNet net; net.add_place(); net.set_activities({"A"});
    if (kind == 0) net.add_transition(1, {}, {});
    if (kind == 1) net.add_transition(0, {1}, {});
    if (kind == 2) net.set_source(1);
    if (kind == 3) net.set_sink(1);
    bool rejected = false;
    try { (void)workflow_input_petri_json(net); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  }
}

void heuristic_complete_state_preserves_original_values_and_presence() {
  HeuristicsNet net;
  net.variant = HeuristicMinerVariant::kPlusPlus;
  net.decoration = HeuristicNetDecoration::kPerformance;
  net.performance_aggregation = HeuristicPerformanceAggregation::kAll;
  net.activity_attribute = ""; net.timestamp_attribute = "";
  net.activities = {"duplicate", "duplicate", "", "東京"};
  HeuristicDependency edge{3, 1, 1.0 / 3.0, 18446744073709551615ULL, -0.0, true};
  edge.representation_value = std::vector<double>{0.125, std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()};
  net.edges = {edge}; net.start_activities = {3, 3}; net.end_activities = {1}; net.active_activities = {3, 1};
  net.output_bundles = {{{1, 1}, {}}}; net.input_bundles = {{{3}}};
  const HeuristicOccurrenceCount wide(1, 0);
  net.activity_statistics = {{3, wide, 2, 3, 0.5, std::nullopt, false}};
  net.sojourn_times = std::map<std::string, double>{{"filtered", std::numeric_limits<double>::infinity()}};
  HeuristicRelationStatistics relation;
  relation.source = 3; relation.target = 1;
  relation.representation = 9.0;
  relation.wait_samples = {0.125, -0.0};
  relation.representation_value = HeuristicPerformanceSummary{1, 2, 3, 4, 5, 6};
  net.relation_statistics = {relation}; net.concurrency_statistics = {{3, 1, 5}};
  net.raw_activity_statistics = {{"filtered", 7, 0.25}};
  net.raw_concurrency_statistics = {{"filtered", "東京", 8}};
  net.raw_dfg = {{{"東京", "filtered"}, 9}}; net.cleaned_dfg = net.raw_dfg;
  net.window_two_dfg = {{{"filtered", "東京"}, 0}};
  net.frequency_triples = {{{"東京", "filtered", "東京"}, 10}};
  net.and_measures_in = {{3, 1, 1, -0.0}}; net.and_measures_out = {{1, 3, 3, 0.5}};
  net.dependency_measures = {{3, 1, std::numeric_limits<double>::quiet_NaN()}};
  net.loop_length_two = {{3, 1, 11}};
  net.metadata = HeuristicNetMetadata{{"duplicate", "duplicate", "filtered"}, {{"filtered", wide}},
      std::vector<std::string>{"duplicate", "duplicate"}, std::map<std::string, std::uint64_t>{{"filtered", 0}}};
  const auto structured = parse_workflow_structured_value_json(
      R"({"type":"object","value":{"signed":{"type":"int64","value":1},"unsigned":{"type":"uint64","value":1},"float":{"type":"float","value":1.0}}})");
  net.performance_values = HeuristicDecoratedDfg{{{"東京", "filtered"}, structured}};
  net.classic_public_dfg_window_2_present = false;
  net.classic_public_freq_triples_present = true;
  net.plusplus_public_dfg_window_2_present = true;
  net.plusplus_public_freq_triples_present = false;
  net.plusplus_computed_performance_dfg = HeuristicMeanPerformanceDfg{{{"東京", "filtered"}, std::numeric_limits<double>::quiet_NaN()}};
  net.plusplus_computed_performance_matrix = {{{"東京", "filtered"}, -std::numeric_limits<double>::infinity()}};
  const auto json = workflow_input_heuristics_json(net);
  for (const auto* key : {"variant", "decoration", "performance_aggregation", "activity_attribute", "timestamp_attribute", "activity_ids_are_local", "activities", "edges", "start_activities", "end_activities", "active_activities", "output_bundles", "input_bundles", "activity_statistics", "sojourn_times", "relation_statistics", "concurrency_statistics", "raw_activity_statistics", "raw_concurrency_statistics", "raw_dfg", "cleaned_dfg", "window_two_dfg", "frequency_triples", "and_measures_in", "and_measures_out", "dependency_measures", "loop_length_two", "metadata", "performance_values", "classic_public_dfg_window_2_present", "classic_public_freq_triples_present", "plusplus_public_dfg_window_2_present", "plusplus_public_freq_triples_present", "plusplus_computed_performance_dfg", "plusplus_computed_performance_matrix"})
    contains(json, '"' + std::string(key) + "\":");
  contains(json, R"("activity_attribute":"","timestamp_attribute":"","activity_ids_are_local":false,"activities":["duplicate","duplicate","","東京"])");
  contains(json, R"("source":3,"target":1,"dependency":0.33333333333333331,"frequency":18446744073709551615,"representation":-0.0)");
  contains(json, serialize_workflow_performance_value_json(*edge.representation_value));
  contains(json, serialize_workflow_performance_value_json(*relation.representation_value));
  contains(json, serialize_workflow_performance_value_json(HeuristicPerformanceValue{structured}));
  contains(json, R"("occurrences":18446744073709551616)");
  contains(json, R"("is_start_activity":null,"is_end_activity":false)");
  contains(json, R"("raw_activity_statistics":[{"activity":"filtered","occurrences":7,"mean_sojourn_seconds":0.25}])");
  contains(json, R"("start_activities":["duplicate","duplicate"],"end_activities":{"filtered":0})");
  contains(json, R"("classic_public_dfg_window_2_present":false,"classic_public_freq_triples_present":true,"plusplus_public_dfg_window_2_present":true,"plusplus_public_freq_triples_present":false)");
  contains(json, R"("plusplus_computed_performance_dfg":[{"source":"東京","target":"filtered","value":"NaN"}])");
  contains(json, R"("plusplus_computed_performance_matrix":[{"source":"東京","target":"filtered","value":"-INF"}])");
  const auto empty = workflow_input_heuristics_json(HeuristicsNet{});
  contains(empty, R"("sojourn_times":null)"); contains(empty, R"("metadata":null)");
  contains(empty, R"("performance_values":null)"); contains(empty, R"("plusplus_computed_performance_dfg":null)");
  net.sojourn_times = std::map<std::string, double>{}; net.metadata = HeuristicNetMetadata{};
  net.performance_values = HeuristicDecoratedDfg{}; net.plusplus_computed_performance_dfg = HeuristicMeanPerformanceDfg{};
  const auto engaged = workflow_input_heuristics_json(net);
  contains(engaged, R"("sojourn_times":{})"); contains(engaged, R"("performance_values":[])");
  contains(engaged, R"("plusplus_computed_performance_dfg":[])");
}

void heuristic_actual_discovery_and_wtf8() {
  EventLog log;
  Trace trace; trace.case_id = "case"; trace.events = {{"case", "A"}, {"case", "B"}}; log.traces.push_back(trace);
  const auto net = discover_heuristics_net(log);
  const auto json = workflow_input_heuristics_json(net);
  contains(json, R"("variant":"classic")"); contains(json, R"("raw_dfg":[{"source":"A","target":"B","count":1}])");
  HeuristicsNet explicit_names;
  explicit_names.activities = {std::string("\xed\xa0\x80", 3), "東京"};
  contains(workflow_input_heuristics_json(explicit_names), R"("activities":["\ud800","東京"])");
}

}  // namespace

int main() {
  petri_empty_and_optional_boundaries();
  petri_preserves_duplicate_labels_silent_names_weights_and_markings();
  petri_real_alpha_plus_detached_and_native_null_activity();
  petri_preserves_typed_recursive_names_and_wtf8();
  petri_rejects_only_invalid_native_indices();
  heuristic_complete_state_preserves_original_values_and_presence();
  heuristic_actual_discovery_and_wtf8();
}
