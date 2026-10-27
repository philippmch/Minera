#include "minera/network_analysis.hpp"
#include "minera/json_export.hpp"
#include "minera/request_params.hpp"

#include <cassert>
#include <cmath>
#include <map>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>
#include <string>
#include <stdexcept>

using namespace minera;

static Event event(std::string id, std::string activity, std::string resource,
                   std::string timestamp) {
  Event e;
  e.case_id = std::move(id); e.activity = std::move(activity);
  e.resource = std::move(resource); e.timestamp = std::move(timestamp);
  return e;
}

static EventLog sample() {
  EventLog log;
  Trace t; t.case_id = "c1";
  t.events.push_back(event("c1", "A", "alice", "2024-01-01T07:00:00Z"));
  t.events.push_back(event("c1", "B", "bob", "2024-01-01T08:00:00Z"));
  t.events.push_back(event("c1", "C", "carol", "2024-01-01T09:00:00Z"));
  log.traces.push_back(std::move(t));
  return log;
}

// Reference values from PM4Py 2.7.22 network_analysis (public calls).
static Event present(std::string id, std::string activity, std::string resource, int second,
                     std::optional<std::pair<std::string, AttributeKind>> rank = std::nullopt) {
  auto e = event(std::move(id), std::move(activity), std::move(resource),
                 "2026-01-01T00:00:" + std::string(second < 10 ? "0" : "") + std::to_string(second) + "Z");
  e.case_id_state = e.activity_state = e.resource_state = e.timestamp_state = FirstClassAttributeState::kPresent;
  if (rank) e.attributes.push_back({"rank", rank->first, rank->second});
  return e;
}
static std::map<std::tuple<std::string, std::string, std::string>, std::uint64_t> edges_of(
    std::vector<Event> events, NetworkAnalysisOptions options = {}) {
  EventLog log;
  Trace trace; trace.case_id = events.empty() ? "" : events.front().case_id;
  trace.events = std::move(events);
  log.traces.push_back(std::move(trace));
  std::map<std::tuple<std::string, std::string, std::string>, std::uint64_t> out;
  for (const auto& edge : discover_network_analysis(log, options).edges)
    out[{edge.source, edge.target, edge.activity}] = edge.count;
  return out;
}

static void reference_audit_contracts() {
  using Edges = std::map<std::tuple<std::string, std::string, std::string>, std::uint64_t>;
  NetworkAnalysisOptions ranked; ranked.sorting_attribute = "rank";
  // O3: integer ranks 1, 2, 10 sort numerically: r->s (A), s->t (B).
  const auto integer = edges_of({present("c", "A", "r", 0, {{"1", AttributeKind::kInt}}),
                                 present("c", "B", "s", 5, {{"2", AttributeKind::kInt}}),
                                 present("c", "C", "t", 10, {{"10", AttributeKind::kInt}})}, ranked);
  assert((integer == Edges{{{"r", "s", "A"}, 1}, {{"s", "t", "B"}, 1}}));
  const auto floating = edges_of({present("c", "A", "r", 0, {{"1.5", AttributeKind::kFloat}}),
                                  present("c", "B", "s", 5, {{"2.0", AttributeKind::kFloat}}),
                                  present("c", "C", "t", 10, {{"10.25", AttributeKind::kFloat}})}, ranked);
  assert(floating == integer);
  // Tied ranks keep input order: C (0) then A, B (1).
  const auto tied = edges_of({present("c", "A", "r", 0, {{"1", AttributeKind::kInt}}),
                              present("c", "B", "s", 5, {{"1", AttributeKind::kInt}}),
                              present("c", "C", "t", 10, {{"0", AttributeKind::kInt}})}, ranked);
  assert((tied == Edges{{{"r", "s", "A"}, 1}, {{"t", "r", "C"}, 1}}));
  // String ranks stay lexical: "1" < "10" < "2".
  const auto text = edges_of({present("c", "A", "r", 0, {{"1", AttributeKind::kString}}),
                              present("c", "B", "s", 5, {{"2", AttributeKind::kString}}),
                              present("c", "C", "t", 10, {{"10", AttributeKind::kString}})}, ranked);
  assert((text == Edges{{{"r", "t", "A"}, 1}, {{"t", "s", "C"}, 1}}));
  // O4: present empty resources, activities and case ids are values.
  assert((edges_of({present("c", "A", "", 0), present("c", "B", "", 9)}) == Edges{{{"", "", "A"}, 1}}));
  assert((edges_of({present("c", "", "r", 0), present("c", "B", "s", 9)}) == Edges{{{"r", "s", ""}, 1}}));
  assert((edges_of({present("", "A", "r", 0), present("", "B", "s", 9)}) == Edges{{{"r", "s", "A"}, 1}}));
  // A missing resource drops only the edges it would name.
  auto unnamed = present("c", "A", "", 0);
  unnamed.resource_state = FirstClassAttributeState::kMissing;
  assert((edges_of({unnamed, present("c", "B", "s", 9), present("c", "C", "t", 10)}) == Edges{{{"s", "t", "B"}, 1}}));
  // An empty sort value still sorts.
  const auto empty_sort = edges_of({present("c", "A", "r", 0, {{"", AttributeKind::kString}}),
                                    present("c", "B", "s", 9, {{"x", AttributeKind::kString}})}, ranked);
  assert((empty_sort == Edges{{{"r", "s", "A"}, 1}}));
}

int main() {
  reference_audit_contracts();
  auto result = discover_network_analysis(sample());
  assert(result.variant == NetworkAnalysisVariant::kDataframe);
  assert(result.edges.size() == 2);
  assert(result.edges[0].source == "alice" && result.edges[0].target == "bob");
  assert(result.edges[0].activity == "A" && result.edges[0].count == 1);
  NetworkAnalysisOptions perf; perf.include_performance = true;
  auto p = discover_network_analysis(sample(), perf);
  assert(p.edges[0].performance.size() == 1 && p.edges[0].performance[0] == 3600.0);
  NetworkAnalysisOptions in; in.edge_reference = NetworkAnalysisEdgeReference::kIn;
  auto i = discover_network_analysis(sample(), in);
  assert(i.edges[0].activity == "B");
  NetworkAnalysisOptions custom; custom.max_rows = 1;
  auto bounded = discover_network_analysis(sample(), custom);
  assert(bounded.edges.size() == 1 && bounded.diagnostics.output_truncated);
  custom.include_performance = true;
  const auto bounded_performance = discover_network_analysis(sample(), custom);
  assert(bounded_performance.diagnostics.link_count == 2 &&
         bounded_performance.diagnostics.performance_value_count == 2 &&
         bounded_performance.edges.front().performance.size() == 1);
  const auto bounded_json = network_analysis_json(bounded_performance);
  assert(bounded_json.find("\"link_count\":2") != std::string::npos &&
         bounded_json.find("\"performance_value_count\":2") != std::string::npos);
  auto bad_bounded = bounded_performance;
  bad_bounded.diagnostics.link_count = 0;
  try { (void)network_analysis_json(bad_bounded); assert(false); }
  catch (const std::invalid_argument&) {}
  EventLog missing = sample(); missing.traces[0].events[1].resource.clear();
  for (const auto& edge : discover_network_analysis(missing).edges) assert(!edge.source.empty() && !edge.target.empty());
  EventLog case_attribute_log;
  Trace case_attribute_trace; case_attribute_trace.case_id = "case-attr";
  case_attribute_trace.attributes.push_back({"region", "north", AttributeKind::kString});
  case_attribute_trace.events.push_back(event("case-attr", "A", "alice", "2024-01-01T00:00:00Z"));
  case_attribute_trace.events.push_back(event("case-attr", "B", "bob", "2024-01-01T00:01:00Z"));
  case_attribute_log.traces.push_back(std::move(case_attribute_trace));
  NetworkAnalysisOptions case_attribute_options;
  case_attribute_options.out_relation_attribute = "case:region";
  case_attribute_options.in_relation_attribute = "case:region";
  assert(discover_network_analysis(case_attribute_log, case_attribute_options).edges.size() == 1);
  NetworkAnalysisOptions values; values.include_performance = true; values.max_values = 1;
  auto limited = discover_network_analysis(sample(), values);
  assert(limited.diagnostics.performance_value_count == 1 && limited.diagnostics.output_truncated);
  assert(!network_analysis_json(limited).empty());
  values.max_rows = 1;
  assert(!network_analysis_json(discover_network_analysis(sample(), values)).empty());
  NetworkAnalysisOptions slots; slots.include_performance = true; slots.business_hours = true;
  auto bh = discover_network_analysis(sample(), slots);
  assert(bh.edges[0].performance[0] == 3600.0);
  try { NetworkAnalysisOptions bad; bad.edge_reference = static_cast<NetworkAnalysisEdgeReference>(99); (void)discover_network_analysis(sample(), bad); assert(false); } catch (...) {}
  try { NetworkAnalysisOptions bad; bad.business_hours = true; bad.business_hour_slots = {{10.0, 10.0}}; (void)discover_network_analysis(sample(), bad); assert(false); } catch (...) {}
  EventLog missing_target;
  Trace missing_trace; missing_trace.case_id = "missing";
  missing_trace.events.push_back(event("missing", "A", "alice", "2024-01-01T00:00:00Z"));
  missing_trace.events.push_back(event("missing", "B", "", "2024-01-01T00:01:00Z"));
  missing_trace.events.push_back(event("missing", "C", "carol", "2024-01-01T00:02:00Z"));
  missing_target.traces.push_back(std::move(missing_trace));
  assert(discover_network_analysis(missing_target).edges.empty());
  EventLog missing_sort = sample();
  missing_sort.traces[0].events[0].timestamp.clear();
  assert(discover_network_analysis(missing_sort).edges.size() <= 1);
  const auto json = network_analysis_json(p);
  assert(json.find("NETWORK_ANALYSIS") != std::string::npos);
  assert(json.find("\"variant\":\"dataframe\"") != std::string::npos);
  auto malformed_result = p;
  ++malformed_result.diagnostics.row_count;
  try { (void)network_analysis_json(malformed_result); assert(false); }
  catch (const std::invalid_argument&) {}
  const auto parsed = parse_network_analysis_params(
      [](std::string_view n) {
        if (n == "network_analysis_variant") return std::string("dataframe");
        return n == "network_analysis_edge_reference" ? std::string("_in") : std::string{};
      });
  assert(parsed.variant == NetworkAnalysisVariant::kDataframe);
  assert(parsed.edge_reference == NetworkAnalysisEdgeReference::kIn);
  try {
    (void)parse_network_analysis_params([](std::string_view n) {
      return n == "network_analysis_edge_reference" ? std::string("in") : std::string{};
    });
    assert(false);
  } catch (const std::invalid_argument&) {
  }
}
