#include "minera/dfg_variants.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
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

void event_attribute(Event& event, std::string key, std::string value) {
  event.attributes.push_back(
      {std::move(key), std::move(value), AttributeKind::kString});
}

Trace trace(std::string case_id, std::vector<Event> events) {
  Trace result;
  result.case_id = std::move(case_id);
  result.events = std::move(events);
  return result;
}

void trace_attribute(Trace& trace, std::string key, std::string value) {
  trace.attributes.push_back(
      {std::move(key), std::move(value), AttributeKind::kString});
}

EventLog log(std::initializer_list<Trace> traces) {
  return EventLog{std::vector<Trace>(traces)};
}

std::uint64_t frequency(const DfgFrequency& dfg, const std::string& source,
                        const std::string& target) {
  const auto found = dfg.find({source, target});
  return found == dfg.end() ? 0 : found->second;
}

const DfgPerformanceStatistics& performance(
    const DfgPerformance& dfg, const std::string& source,
    const std::string& target) {
  const auto found = dfg.find({source, target});
  assert(found != dfg.end());
  return found->second;
}

void native_windows_keep_once_and_aliases() {
  auto first = trace("1", {event("A"), event("B"), event("A"), event("B")});
  first.events[1].lifecycle = "start";
  first.events[2].lifecycle = "complete";
  const auto source = log({first, trace("2", {}), trace("3", {event("Z")})});

  DfgStatisticalOptions options;
  auto dfg = discover_frequency_dfg(source, options);
  assert(frequency(dfg, "A", "B") == 2);
  assert(frequency(dfg, "B", "A") == 1);

  options.keep_once_per_case = true;
  dfg = discover_frequency_dfg(source, options);
  assert(frequency(dfg, "A", "B") == 1);
  assert(frequency(dfg, "B", "A") == 1);

  options.keep_once_per_case = false;
  options.window = 2;
  dfg = discover_frequency_dfg(source, options);
  assert(frequency(dfg, "A", "A") == 1);
  assert(frequency(dfg, "B", "B") == 1);

  options.window = 0;
  dfg = discover_frequency_dfg(source, options);
  assert(frequency(dfg, "A", "A") == 2);
  assert(frequency(dfg, "B", "B") == 2);
  assert(frequency(dfg, "Z", "Z") == 1);

  options.variant = DfgStatisticalVariant::FREQUENCY_GREEDY;
  const auto alias = discover_dfg_statistics(source, options);
  assert(alias.frequency_dfg == dfg);
}

void triples_are_consecutive_and_never_deduplicated() {
  const auto source = log({
      trace("1", {event("A"), event("B"), event("A"), event("B")}),
      trace("2", {event("A"), event("B")}), trace("3", {})});
  const auto triples = discover_frequency_triples(source);
  assert(triples.at({"A", "B", "A"}) == 1);
  assert(triples.at({"B", "A", "B"}) == 1);
  assert(triples.size() == 2);

  DfgStatisticalOptions options;
  options.variant = DfgStatisticalVariant::FREQ_TRIPLES;
  options.keep_once_per_case = true;
  const auto analysis = discover_dfg_statistics(
      log({trace("x", {event("A"), event("B"), event("A"), event("B"),
                        event("A")})}),
      options);
  assert(analysis.frequency_triples.at({"A", "B", "A"}) == 2);
}

EventLog performance_log() {
  auto b1 = event("B", "2026-01-05T00:00:06Z");
  event_attribute(b1, "started", "2026-01-05T00:00:06Z");
  auto b2 = event("B", "2026-01-06T00:00:16Z");
  event_attribute(b2, "started", "2026-01-06T00:00:16Z");
  return log({trace("1", {event("A", "2026-01-05T00:00:01Z"), b1}),
              trace("2", {event("A", "2026-01-06T00:00:01Z"), b2})});
}

void performance_aggregations_start_fallback_and_validation() {
  DfgStatisticalOptions options;
  options.start_timestamp_attribute = "started";
  const auto source = performance_log();
  const auto expected = std::vector<std::pair<DfgPerformanceAggregation, double>>{
      {DfgPerformanceAggregation::kMean, 10.0},
      {DfgPerformanceAggregation::kMedian, 10.0},
      {DfgPerformanceAggregation::kMinimum, 5.0},
      {DfgPerformanceAggregation::kMaximum, 15.0},
      {DfgPerformanceAggregation::kStandardDeviation, std::sqrt(50.0)},
      {DfgPerformanceAggregation::kSum, 20.0},
  };
  for (const auto& [aggregation, value] : expected) {
    options.performance_aggregation = aggregation;
    const auto dfg = discover_performance_dfg(source, options);
    const auto& selected = performance(dfg, "A", "B").value;
    assert(std::holds_alternative<double>(selected));
    assert(std::abs(std::get<double>(selected) - value) < 1e-9);
  }

  options.performance_aggregation = DfgPerformanceAggregation::kRawValues;
  auto dfg = discover_performance_dfg(source, options);
  assert((std::get<std::vector<double>>(performance(dfg, "A", "B").value) ==
          std::vector<double>{5.0, 15.0}));
  options.performance_aggregation = DfgPerformanceAggregation::kAll;
  dfg = discover_performance_dfg(source, options);
  const auto& all =
      std::get<DfgPerformanceSummary>(performance(dfg, "A", "B").value);
  assert(all.count == 2 && all.mean == 10.0 && all.median == 10.0);
  assert(all.minimum == 5.0 && all.maximum == 15.0 && all.sum == 20.0);
  assert(std::abs(all.standard_deviation - std::sqrt(50.0)) < 1e-9);

  bool missing_start = false;
  try {
    (void)discover_performance_dfg(
        log({trace("c", {event("A", "2026-01-05T00:00:10Z"),
                          event("B", "2026-01-05T00:00:05Z")})}),
        options);
  } catch (const std::invalid_argument&) {
    missing_start = true;
  }
  assert(missing_start);

  // The omitted selector uses completion timestamps, and a target before its
  // predecessor is clamped to zero.
  options.start_timestamp_attribute.clear();
  const auto clamped = discover_performance_dfg(
      log({trace("c", {event("A", "2026-01-05T00:00:10Z"),
                        event("B", "2026-01-05T00:00:05Z")})}), options);
  assert(performance(clamped, "A", "B").summary.mean == 0.0);

  bool invalid = false;
  try {
    (void)discover_performance_dfg(
        log({trace("bad", {event("A", "invalid"),
                            event("B", "2026-01-05T00:00:05Z")})}),
        options);
  } catch (const std::invalid_argument&) {
    invalid = true;
  }
  assert(invalid);
}

void performance_business_hours_use_weekly_slots() {
  DfgStatisticalOptions options;
  options.use_business_hours = true;
  options.business_hour_slots = {{9.0 * 3600.0, 10.0 * 3600.0}};
  const auto dfg = discover_performance_dfg(
      log({trace("monday", {event("A", "2026-01-05T08:00:00Z"),
                             event("B", "2026-01-05T12:00:00Z")})}),
      options);
  assert(performance(dfg, "A", "B").summary.mean == 3600.0);

  options.business_hour_slots = {{10.0, 10.0}};
  bool invalid = false;
  try {
    (void)discover_performance_dfg(performance_log(), options);
  } catch (const std::invalid_argument&) {
    invalid = true;
  }
  assert(invalid);
}

void custom_activity_and_timestamp_selectors_cover_all_variants() {
  auto first = event("raw-a", "not-a-time");
  event_attribute(first, "label", "A");
  event_attribute(first, "completed_at", "2026-01-05T00:00:01Z");
  event_attribute(first, "started_at", "2026-01-05T00:00:01Z");
  auto second = event("raw-b", "also-not-a-time");
  event_attribute(second, "label", "B");
  event_attribute(second, "completed_at", "2026-01-05T00:00:06Z");
  event_attribute(second, "started_at", "2026-01-05T00:00:11Z");
  auto third = event("raw-c", "still-not-a-time");
  event_attribute(third, "label", "C");
  event_attribute(third, "completed_at", "2026-01-05T00:00:16Z");
  event_attribute(third, "started_at", "2026-01-05T00:00:21Z");
  const auto source = log({trace("custom", {second, first, third})});

  DfgStatisticalOptions options;
  options.activity_attribute = "label";
  options.timestamp_attribute = "completed_at";
  options.start_timestamp_attribute = "started_at";

  const auto frequency = discover_frequency_dfg(source, options);
  assert(frequency.at({"B", "A"}) == 1);
  assert(frequency.at({"A", "C"}) == 1);

  const auto triples = discover_frequency_triples(source, options);
  assert(triples.at({"B", "A", "C"}) == 1);

  const auto performance = discover_performance_dfg(source, options);
  assert(performance.at({"B", "A"}).summary.mean == 0.0);
  assert(performance.at({"A", "C"}).summary.mean == 20.0);

  auto attributed = source.traces.front();
  trace_attribute(attributed, "region", "north");
  options.variant = DfgStatisticalVariant::CASE_ATTRIBUTES;
  options.case_attributes = {"region"};
  const auto case_attributes = discover_dfg_statistics(
      log({std::move(attributed)}), options);
  assert(case_attributes.case_attributes.edges.at({"B", "A"})
             .at("region")
             .at("north") == 1);

  options.variant = DfgStatisticalVariant::CLEAN;
  const auto clean = discover_dfg_statistics(source, options);
  assert(clean.clean.start_activities.at("A") == 1);
  assert(clean.clean.end_activities.at("C") == 1);
  assert(clean.clean.graph.at({"B", "C"}) == 1);

  options.variant = DfgStatisticalVariant::CLEAN_TIME;
  const auto clean_time = discover_dfg_statistics(source, options);
  assert(clean_time.clean_time.at("A") == 0.0);
  assert(clean_time.clean_time.at("C") == 15.0);

  bool missing_activity = false;
  options.activity_attribute = "unknown_label";
  try {
    (void)discover_frequency_dfg(source, options);
  } catch (const std::invalid_argument&) {
    missing_activity = true;
  }
  assert(missing_activity);

  bool missing_timestamp = false;
  options.activity_attribute = "label";
  options.timestamp_attribute = "unknown_time";
  try {
    (void)discover_clean_time_dfg(source, options);
  } catch (const std::invalid_argument&) {
    missing_timestamp = true;
  }
  assert(missing_timestamp);
}

void case_attributes_cover_ids_custom_missing_and_nodes() {
  auto first = trace("case-1", {event("A"), event("B"), event("B")});
  trace_attribute(first, "region", "north");
  auto second = trace("case-2", {event("A"), event("B")});
  auto third = trace("case-3", {event("A"), event("C")});
  trace_attribute(third, "region", "south");
  DfgStatisticalOptions options;
  options.case_attributes = {"concept:name", "region", "region", "missing"};
  options.return_node_attributes = true;
  const auto result =
      discover_case_attribute_dfg(log({first, second, third}), options);
  assert(result.edges.at({"A", "B"})
             .at("concept:name")
             .at("case-1") == 1);
  assert(result.edges.at({"A", "B"})
             .at("concept:name")
             .at("case-2") == 1);
  assert(result.edges.at({"A", "B"}).at("region").at("north") == 1);
  assert(!result.edges.at({"A", "B"}).contains("missing"));
  assert(result.nodes.at("B").at("region").at("north") == 2);
  assert(result.nodes.at("A").at("concept:name").size() == 3);

  options.case_attributes = {"case_id"};
  const auto alias = discover_case_attribute_dfg(log({first}), options);
  assert(alias.edges.at({"A", "B"}).at("case_id").at("case-1") == 1);
}

void clean_preserves_boundaries_and_middle_owned_edges() {
  const auto source = log({
      trace("one", {event("S", "2026-01-01T00:00:00Z")}),
      trace("two", {event("B", "2026-01-01T00:00:02Z"),
                     event("A", "2026-01-01T00:00:01Z")}),
      trace("three", {event("C", "2026-01-01T00:00:03Z", 2),
                       event("A", "2026-01-01T00:00:01Z", 0),
                       event("B", "2026-01-01T00:00:02Z", 1)}),
  });
  const auto clean = discover_clean_dfg(source);
  assert(clean.start_activities.at("S") == 1);
  assert(clean.start_activities.at("A") == 2);
  assert(clean.end_activities.at("S") == 1);
  assert(clean.end_activities.at("B") == 1);
  assert(clean.end_activities.at("C") == 1);
  assert(clean.graph.size() == 1);
  assert(frequency(clean.graph, "B", "C") == 1);
  assert(frequency(clean.graph, "A", "B") == 0);  // length two has no edge

  DfgStatisticalOptions options;
  options.variant = DfgStatisticalVariant::CLEAN_POLARS;
  const auto adapter = discover_dfg_statistics(source, options);
  // The lazy-frame variant owns every within-case adjacency and derives starts
  // from the row after each case boundary. It is intentionally distinct from
  // CLEAN's middle-owned-edge projection.
  assert(adapter.clean.graph != clean.graph);
  assert(frequency(adapter.clean.graph, "A", "B") == 2);
  assert(frequency(adapter.clean.graph, "B", "C") == 1);
  assert(adapter.clean.start_activities.at("A") == 2);
  assert(!adapter.clean.start_activities.contains("C"));
  assert(adapter.frequency_dfg == adapter.clean.graph);

  bool invalid = false;
  try {
    (void)discover_clean_dfg(
        log({trace("bad", {event("A", "not-a-timestamp")})}));
  } catch (const std::invalid_argument&) {
    invalid = true;
  }
  assert(invalid);
}

void clean_stable_ties_and_clean_time_repeated_activities() {
  const auto same = "2026-01-01T00:00:00Z";
  const auto tied = discover_clean_dfg(log({trace(
      "tie", {event("C", same, 2), event("A", same, 0),
               event("B", same, 1)})}));
  assert(frequency(tied.graph, "B", "C") == 1);

  const auto times = discover_clean_time_dfg(log({
      trace("1", {event("B", "2026-01-01T00:00:20Z"),
                   event("A", "2026-01-01T00:00:10Z"),
                   event("A", "2026-01-01T00:00:00Z")}),
      trace("2", {event("B", "2026-01-02T00:00:10Z"),
                   event("A", "2026-01-02T00:00:00Z")}),
  }));
  assert(times.at("A") == 2.5);
  assert(times.at("B") == 15.0);

  bool invalid = false;
  try {
    (void)discover_clean_time_dfg(
        log({trace("bad", {event("A", "invalid")})}));
  } catch (const std::invalid_argument&) {
    invalid = true;
  }
  assert(invalid);
}

void causal_alpha_contract() {
  const DfgFrequency dfg = {
      {{"A", "B"}, 3}, {{"B", "A"}, 1}, {{"C", "D"}, 2},
      {{"D", "C"}, 0}, {{"E", "F"}, 0}, {{"X", "X"}, 4},
  };
  const auto alpha = discover_dfg_causality(dfg, DfgCausalVariant::ALPHA);
  assert(alpha.relations.size() == 1);
  assert(alpha.relations.at({"C", "D"}) == 1.0);
}

void causal_heuristic_contract() {
  const DfgFrequency dfg = {
      {{"A", "B"}, 3}, {{"B", "A"}, 1}, {{"C", "D"}, 2},
      {{"D", "C"}, 0}, {{"E", "F"}, 0}, {{"X", "X"}, 4},
  };
  const auto heuristic =
      discover_dfg_causality(dfg, DfgCausalVariant::HEURISTIC);
  assert(std::abs(heuristic.relations.at({"A", "B"}) - 0.4) < 1e-9);
  assert(std::abs(heuristic.relations.at({"B", "A"}) + 0.4) < 1e-9);
  assert(std::abs(heuristic.relations.at({"C", "D"}) - 2.0 / 3.0) <
         1e-9);
  assert(std::abs(heuristic.relations.at({"D", "C"}) + 2.0 / 3.0) <
         1e-9);
  assert(heuristic.relations.contains({"F", "E"}));
  assert(heuristic.relations.at({"E", "F"}) == 0.0);
  assert(heuristic.relations.at({"X", "X"}) == 0.0);
  assert(std::signbit(heuristic.relations.at({"X", "X"})));

  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  const DfgFrequency near_max = {
      {{"wide", "other"}, maximum},
      {{"other", "wide"}, maximum - 1},
  };
  const auto wide =
      discover_dfg_causality(near_max, DfgCausalVariant::HEURISTIC);
  assert(wide.relations.at({"wide", "other"}) > 0.0);
  assert(wide.relations.at({"other", "wide"}) < 0.0);
}

void causal_rejects_unknown_variant() {
  bool rejected = false;
  try {
    (void)discover_dfg_causality(
        {{{"A", "B"}, 1}}, static_cast<DfgCausalVariant>(99));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

}  // namespace

int main() {
  native_windows_keep_once_and_aliases();
  triples_are_consecutive_and_never_deduplicated();
  performance_aggregations_start_fallback_and_validation();
  performance_business_hours_use_weekly_slots();
  custom_activity_and_timestamp_selectors_cover_all_variants();
  case_attributes_cover_ids_custom_missing_and_nodes();
  clean_preserves_boundaries_and_middle_owned_edges();
  clean_stable_ties_and_clean_time_repeated_activities();
  causal_alpha_contract();
  causal_heuristic_contract();
  causal_rejects_unknown_variant();
  return 0;
}
