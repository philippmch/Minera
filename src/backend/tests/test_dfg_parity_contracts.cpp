#include "minera/dfg_variants.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace minera;

Event event(std::string activity, std::string timestamp = {},
            std::size_t input_order = 0) {
  Event result;
  result.activity = std::move(activity);
  result.timestamp = std::move(timestamp);
  result.input_order = input_order;
  return result;
}

void event_attribute(Event& target, std::string key, std::string value) {
  target.attributes.push_back(
      {std::move(key), std::move(value), AttributeKind::kString});
}

Trace trace(std::string case_id, std::vector<Event> events) {
  Trace result;
  result.case_id = std::move(case_id);
  result.events = std::move(events);
  return result;
}

EventLog log(std::initializer_list<Trace> traces) {
  return EventLog{std::vector<Trace>(traces)};
}

std::uint64_t frequency(const DfgFrequency& graph,
                        const std::string& source,
                        const std::string& target) {
  const auto found = graph.find({source, target});
  return found == graph.end() ? 0 : found->second;
}

EventLog simple_log() {
  return log({trace("case-1",
                    {event("A", "2026-01-01T00:00:00Z", 0),
                     event("B", "2026-01-01T00:00:05Z", 1),
                     event("C", "2026-01-01T00:00:09Z", 2)}),
              trace("case-2",
                    {event("A", "2026-01-02T00:00:00Z", 0),
                     event("B", "2026-01-02T00:00:03Z", 1)})});
}

EventLog custom_activity_log() {
  auto first = event("raw-a", "2026-01-01T00:00:00Z", 0);
  event_attribute(first, "label", "A");
  auto second = event("raw-b", "2026-01-01T00:00:04Z", 1);
  event_attribute(second, "label", "B");
  return log({trace("custom", {std::move(first), std::move(second)})});
}

void pandas_df_statistics_contract() {
  DfgStatisticalOptions options;
  options.activity_attribute = "label";
  options.timestamp_attribute = "observed_at";
  options.start_timestamp_attribute = "started_at";

  auto first = event("raw-a", "unused", 0);
  event_attribute(first, "label", "A");
  event_attribute(first, "observed_at", "2026-01-01T00:00:05Z");
  auto second = event("raw-b", "unused", 1);
  event_attribute(second, "label", "B");
  event_attribute(second, "observed_at", "2026-01-01T00:00:11Z");
  event_attribute(second, "started_at", "2026-01-01T00:00:09Z");

  const auto source = log({trace("custom", {std::move(first),
                                              std::move(second)})});
  const auto frequency_graph = discover_frequency_dfg(source, options);
  assert(frequency_graph.at({"A", "B"}) == 1);

  const DfgPerformanceStatistics statistics =
      discover_performance_dfg(source, options).at({"A", "B"});
  assert(statistics.summary.count == 1);
  assert(statistics.summary.mean == 4.0);
  assert(std::holds_alternative<double>(statistics.value));
  assert(std::get<double>(statistics.value) == 4.0);
}

void pandas_freq_triples_contract() {
  DfgTripleFrequency triples = discover_frequency_triples(log({
      trace("one", {event("A"), event("B"), event("C"), event("D")}),
      trace("two", {event("A"), event("B"), event("C")}),
  }));
  assert(triples.at({"A", "B", "C"}) == 2);
  assert(triples.at({"B", "C", "D"}) == 1);
  assert(triples.size() == 2);
}

void polars_df_statistics_contract() {
  DfgCleanAnalysis table = discover_clean_polars_dfg(log({
      trace("one", {event("A", "2026-01-01T00:00:00Z", 0),
                     event("B", "2026-01-01T00:00:01Z", 1)}),
      trace("two", {event("A", "2026-01-02T00:00:00Z", 0),
                     event("B", "2026-01-02T00:00:01Z", 1)}),
  }));
  assert(frequency(table.graph, "A", "B") == 2);
  assert(table.end_activities.at("B") == 2);
  assert(table.start_activities.at("A") == 2);
}

void polars_freq_triples_contract() {
  DfgStatisticalOptions options;
  options.variant = DfgStatisticalVariant::FREQ_TRIPLES;
  DfgStatisticalAnalysis analysis =
      discover_dfg_statistics(log({trace("triples", {event("A"), event("B"),
                                                       event("C")})}),
                              options);
  assert(analysis.variant == DfgStatisticalVariant::FREQ_TRIPLES);
  assert(analysis.frequency_triples.at({"A", "B", "C"}) == 1);
  assert(!analysis.frequency_triples.empty());
}

void replacement_values_contract() {
  DfgFrequency target{{{"A", "B"}, 1}, {{"B", "C"}, 2}};
  const DfgFrequency replacements{{{"A", "B"}, 7}, {{"C", "D"}, 9}};
  DfgFrequency* const returned = &replace_dfg_values(target, replacements);
  assert(returned == &target);
  assert(frequency(target, "A", "B") == 7);
  assert(frequency(target, "B", "C") == 2);
  assert(frequency(target, "C", "D") == 0);
}

void dfg_utils_contract() {
  DfgFrequency graph = discover_frequency_dfg(
      log({trace("graph", {event("A"), event("B"), event("A"),
                            event("B")})}));
  assert(frequency(graph, "A", "B") == 2);
  assert(frequency(graph, "B", "A") == 1);
  assert(graph.size() == 2);
}

void case_attributes_variant_contract() {
  auto source = trace("case-1", {event("A"), event("B"), event("C")});
  source.attributes.push_back(
      {"region", "north", AttributeKind::kString});
  DfgStatisticalOptions options;
  options.case_attributes = {"concept:name", "region"};
  options.return_node_attributes = true;
  const DfgCaseAttributeAnalysis result =
      discover_case_attribute_dfg(log({std::move(source)}), options);
  assert(result.edges.at({"A", "B"})
             .at("concept:name")
             .at("case-1") == 1);
  assert(result.edges.at({"B", "C"}).at("region").at("north") == 1);
  assert(result.nodes.at("B").at("region").at("north") == 1);
}

void clean_variant_contract() {
  const DfgCleanAnalysis result = discover_clean_dfg(log({trace(
      "ordered", {event("C", "2026-01-01T00:00:03Z", 2),
                   event("A", "2026-01-01T00:00:01Z", 0),
                   event("B", "2026-01-01T00:00:02Z", 1)})}));
  assert(result.start_activities.at("A") == 1);
  assert(result.end_activities.at("C") == 1);
  assert(frequency(result.graph, "B", "C") == 1);
  assert(frequency(result.graph, "A", "B") == 0);
}

void clean_polars_variant_contract() {
  DfgStatisticalOptions options;
  options.variant = DfgStatisticalVariant::CLEAN_POLARS;
  const auto analysis = discover_dfg_statistics(
      log({trace("one", {event("A", "2026-01-01T00:00:00Z", 0),
                          event("B", "2026-01-01T00:00:01Z", 1)}),
           trace("two", {event("B", "2026-01-02T00:00:01Z", 1),
                          event("A", "2026-01-02T00:00:00Z", 0)})}),
      options);
  assert(options.variant == DfgStatisticalVariant::CLEAN_POLARS);
  assert(analysis.variant == DfgStatisticalVariant::CLEAN_POLARS);
  assert(frequency(analysis.clean.graph, "A", "B") == 2);
  assert(analysis.frequency_dfg == analysis.clean.graph);
}

void clean_time_variant_contract() {
  DfgStatisticalOptions options;
  options.variant = DfgStatisticalVariant::CLEAN_TIME;
  const auto analysis = discover_dfg_statistics(
      log({trace("one", {event("A", "2026-01-01T00:00:00Z", 0),
                          event("B", "2026-01-01T00:00:10Z", 1),
                          event("A", "2026-01-01T00:00:20Z", 2)})}),
      options);
  assert(analysis.variant == DfgStatisticalVariant::CLEAN_TIME);
  assert(analysis.clean_time.at("A") == 10.0);
  assert(analysis.clean_time.at("B") == 10.0);
}

void freq_triples_variant_contract() {
  const DfgTripleFrequency triples = discover_frequency_triples(log({
      trace("ordered", {event("A"), event("B"), event("A"), event("B")}),
  }));
  assert(triples.at({"A", "B", "A"}) == 1);
  assert(triples.at({"B", "A", "B"}) == 1);
}

void native_variant_contract() {
  DfgStatisticalOptions options;
  options.window = 2;
  const DfgFrequency graph = discover_frequency_dfg(
      log({trace("native", {event("A"), event("B"), event("C"),
                             event("D")})}),
      options);
  assert(frequency(graph, "A", "C") == 1);
  assert(frequency(graph, "B", "D") == 1);
  assert(frequency(graph, "A", "B") == 0);
}

void performance_variant_contract() {
  DfgStatisticalOptions options;
  options.performance_aggregation = DfgPerformanceAggregation::MEDIAN;
  const auto result = discover_performance_dfg(
      log({trace("one", {event("A", "2026-01-01T00:00:00Z", 0),
                          event("B", "2026-01-01T00:00:05Z", 1)}),
           trace("two", {event("A", "2026-01-02T00:00:00Z", 0),
                          event("B", "2026-01-02T00:00:15Z", 1)})}),
      options);
  const auto& statistics = result.at({"A", "B"});
  assert(statistics.summary.count == 2);
  assert(statistics.summary.mean == 10.0);
  assert(statistics.summary.median == 10.0);
  assert(std::get<double>(statistics.value) == 10.0);
}

void native_selector_contract() {
  DfgStatisticalOptions options;
  options.variant = DfgStatisticalVariant::NATIVE;
  const auto result = discover_dfg_statistics(
      log({trace("native", {event("A"), event("B")})}), options);
  assert(options.variant == DfgStatisticalVariant::NATIVE);
  assert(result.variant == DfgStatisticalVariant::NATIVE);
  assert(frequency(result.frequency_dfg, "A", "B") == 1);
}

void frequency_selector_contract() {
  DfgStatisticalOptions options;
  options.variant = DfgStatisticalVariant::FREQUENCY;
  const auto result = discover_dfg_statistics(
      log({trace("frequency", {event("A"), event("B")})}), options);
  assert(options.variant == DfgStatisticalVariant::FREQUENCY);
  assert(result.variant == DfgStatisticalVariant::FREQUENCY);
  assert(frequency(result.frequency_dfg, "A", "B") == 1);
}

void performance_selector_contract() {
  DfgStatisticalOptions options;
  options.variant = DfgStatisticalVariant::PERFORMANCE;
  const auto result = discover_dfg_statistics(
      log({trace("performance", {event("A", "2026-01-01T00:00:00Z"),
                                   event("B", "2026-01-01T00:00:05Z")})}),
      options);
  assert(options.variant == DfgStatisticalVariant::PERFORMANCE);
  assert(result.variant == DfgStatisticalVariant::PERFORMANCE);
  assert(result.performance_dfg.at({"A", "B"}).summary.mean == 5.0);
}

void frequency_greedy_selector_contract() {
  DfgStatisticalOptions options;
  options.variant = DfgStatisticalVariant::FREQUENCY_GREEDY;
  const auto result = discover_dfg_statistics(
      log({trace("greedy", {event("A"), event("B"), event("A")})}),
      options);
  assert(options.variant == DfgStatisticalVariant::FREQUENCY_GREEDY);
  assert(result.variant == DfgStatisticalVariant::FREQUENCY_GREEDY);
  assert(frequency(result.frequency_dfg, "A", "B") == 1);
  assert(frequency(result.frequency_dfg, "B", "A") == 1);
}

void performance_greedy_selector_contract() {
  DfgStatisticalOptions options;
  options.variant = DfgStatisticalVariant::PERFORMANCE_GREEDY;
  const auto result = discover_dfg_statistics(
      log({trace("greedy", {event("A", "2026-01-01T00:00:00Z"),
                             event("B", "2026-01-01T00:00:07Z")})}),
      options);
  assert(options.variant == DfgStatisticalVariant::PERFORMANCE_GREEDY);
  assert(result.variant == DfgStatisticalVariant::PERFORMANCE_GREEDY);
  assert(result.performance_dfg.at({"A", "B"}).summary.mean == 7.0);
}

void freq_triples_selector_contract() {
  DfgStatisticalOptions options;
  options.variant = DfgStatisticalVariant::FREQ_TRIPLES;
  const auto result = discover_dfg_statistics(
      log({trace("triples", {event("A"), event("B"), event("C")})}),
      options);
  assert(options.variant == DfgStatisticalVariant::FREQ_TRIPLES);
  assert(result.variant == DfgStatisticalVariant::FREQ_TRIPLES);
  assert(result.frequency_triples.at({"A", "B", "C"}) == 1);
}

void case_attributes_selector_contract() {
  auto source = trace("case", {event("A"), event("B")});
  source.attributes.push_back(
      {"region", "west", AttributeKind::kString});
  DfgStatisticalOptions options;
  options.variant = DfgStatisticalVariant::CASE_ATTRIBUTES;
  options.case_attributes = {"region"};
  const auto result = discover_dfg_statistics(
      log({std::move(source)}), options);
  assert(options.variant == DfgStatisticalVariant::CASE_ATTRIBUTES);
  assert(result.variant == DfgStatisticalVariant::CASE_ATTRIBUTES);
  assert(result.case_attributes.edges.at({"A", "B"})
             .at("region")
             .at("west") == 1);
}

void clean_selector_contract() {
  DfgStatisticalOptions options;
  options.variant = DfgStatisticalVariant::CLEAN;
  const auto result = discover_dfg_statistics(
      log({trace("clean", {event("A", "2026-01-01T00:00:00Z", 0),
                            event("B", "2026-01-01T00:00:01Z", 1),
                            event("C", "2026-01-01T00:00:02Z", 2)})}),
      options);
  assert(options.variant == DfgStatisticalVariant::CLEAN);
  assert(result.variant == DfgStatisticalVariant::CLEAN);
  assert(frequency(result.frequency_dfg, "B", "C") == 1);
}

}  // namespace

int main() {
  pandas_df_statistics_contract();
  pandas_freq_triples_contract();
  polars_df_statistics_contract();
  polars_freq_triples_contract();
  replacement_values_contract();
  dfg_utils_contract();
  case_attributes_variant_contract();
  clean_variant_contract();
  clean_polars_variant_contract();
  clean_time_variant_contract();
  freq_triples_variant_contract();
  native_variant_contract();
  performance_variant_contract();
  native_selector_contract();
  frequency_selector_contract();
  performance_selector_contract();
  frequency_greedy_selector_contract();
  performance_greedy_selector_contract();
  freq_triples_selector_contract();
  case_attributes_selector_contract();
  clean_selector_contract();
  return 0;
}
