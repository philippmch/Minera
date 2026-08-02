#include "minera/network_analysis.hpp"
#include "minera/json_export.hpp"
#include "minera/request_params.hpp"

#include <cassert>
#include <cmath>
#include <map>
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

int main() {
  auto result = discover_network_analysis(sample());
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
  const auto parsed = parse_network_analysis_params(
      [](std::string_view n) { return n == "network_analysis_edge_reference" ? std::string("_in") : std::string{}; });
  assert(parsed.edge_reference == NetworkAnalysisEdgeReference::kIn);
  try {
    (void)parse_network_analysis_params([](std::string_view n) {
      return n == "network_analysis_edge_reference" ? std::string("in") : std::string{};
    });
    assert(false);
  } catch (const std::invalid_argument&) {
  }
}
