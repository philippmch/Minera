#include "minera/local_diagnostics.hpp"
#include "minera/network_analysis.hpp"
#include "minera/organizational_mining.hpp"
#include "minera/resource_profiles.hpp"
#include "minera/role_discovery.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

using namespace minera;

namespace {

EventAttribute attribute(std::string key, std::string value,
                         AttributeKind kind = AttributeKind::kString) {
  EventAttribute result;
  result.key = std::move(key);
  result.value = std::move(value);
  result.kind = kind;
  if (kind == AttributeKind::kDate)
    result.typed_value = NativeDate{result.value};
  return result;
}

Event make_event(std::string case_id, std::string activity,
                 std::string timestamp, std::string resource,
                 std::size_t input_order = 0) {
  Event result;
  result.case_id = std::move(case_id);
  result.activity = std::move(activity);
  result.timestamp = std::move(timestamp);
  result.resource = std::move(resource);
  result.input_order = input_order;
  return result;
}

EventLog diagnostic_log() {
  EventLog log;
  Trace trace;
  trace.case_id = "c1";
  auto first = make_event("c1", "A", "2024-01-01T00:00:00Z", "alice", 0);
  first.attributes.push_back(attribute("org:group", "g1"));
  auto second = make_event("c1", "B", "2024-01-01T00:01:00Z", "bob", 1);
  second.attributes.push_back(attribute("org:group", "g1"));
  auto third = make_event("c1", "A", "2024-01-01T00:02:00Z", "carol", 2);
  third.attributes.push_back(attribute("org:group", "g2"));
  trace.events = {std::move(first), std::move(second), std::move(third)};
  log.traces.push_back(std::move(trace));
  return log;
}

EventLog role_log() {
  EventLog log;
  Trace trace;
  trace.case_id = "roles";
  trace.events.push_back(make_event("roles", "A", "", "r1", 0));
  trace.events.push_back(make_event("roles", "A", "", "r1", 1));
  trace.events.push_back(make_event("roles", "B", "", "r1", 2));
  trace.events.push_back(make_event("roles", "B", "", "r1", 3));
  trace.events.push_back(make_event("roles", "B", "", "r2", 4));
  log.traces.push_back(std::move(trace));
  return log;
}

EventLog network_log() {
  EventLog log;
  Trace trace;
  trace.case_id = "network";
  trace.events.push_back(
      make_event("network", "A", "2024-01-01T07:00:00Z", "alice", 0));
  trace.events.push_back(
      make_event("network", "B", "2024-01-01T08:00:00Z", "bob", 1));
  trace.events.push_back(
      make_event("network", "C", "2024-01-01T09:00:00Z", "carol", 2));
  log.traces.push_back(std::move(trace));
  return log;
}

EventLog profile_log() {
  EventLog log;
  Trace first;
  first.case_id = "a";
  first.events.push_back(
      make_event("a", "A", "2024-01-01T00:00:00Z", "alice", 0));
  first.events.push_back(
      make_event("a", "B", "2024-01-01T00:00:10Z", "bob", 1));
  Trace second;
  second.case_id = "b";
  second.events.push_back(
      make_event("b", "A", "2024-01-01T00:00:10Z", "alice", 2));
  second.events.push_back(
      make_event("b", "C", "2024-01-01T00:00:20Z", "bob", 3));
  log.traces = {std::move(first), std::move(second)};
  return log;
}

EventLog custom_attribute_log() {
  EventLog log;
  for (const auto& [case_id, timestamp] :
       {std::pair{"batch-left", "2024-01-01T00:00:10Z"},
        std::pair{"batch-right", "2024-01-01T00:00:20Z"}}) {
    Trace trace;
    // Both source traces intentionally share a projected case id.  The
    // custom case selector must keep their two native case values distinct.
    trace.case_id = "transport-case";
    trace.attributes.push_back(attribute("case:batch", case_id));
    auto event = make_event("transport-case", "ignored", "not-a-date",
                            "ignored", 0);
    event.lifecycle = "complete";
    event.attributes = {
        attribute("team:activity", "A"),
        attribute("team:resource", "r-custom"),
        attribute("observed_at", timestamp, AttributeKind::kDate),
        attribute("started_at", "2024-01-01T00:00:00Z", AttributeKind::kDate),
    };
    trace.events.push_back(std::move(event));
    log.traces.push_back(std::move(trace));
  }
  return log;
}

EventLog fraction_case_log() {
  EventLog log;
  auto add_case = [&](const std::string& case_id,
                      const std::string& resource,
                      std::size_t order) {
    Trace trace;
    trace.case_id = case_id;
    trace.events.push_back(make_event(
        case_id, "A", "2024-01-01T00:00:00Z", resource, order));
    trace.events.push_back(make_event(
        case_id, "B", "2024-01-01T00:00:01Z", resource, order + 1));
    log.traces.push_back(std::move(trace));
  };
  add_case("case-a", "alice", 0);
  add_case("case-b", "bob", 2);
  add_case("case-c", "bob", 4);
  return log;
}

EventLog custom_role_log() {
  EventLog log;
  Trace trace;
  trace.case_id = "custom-roles";
  auto first = make_event("custom-roles", "ignored", "", "", 0);
  first.attributes = {attribute("team:activity", "A"),
                      attribute("team:resource", "r1")};
  auto second = make_event("custom-roles", "ignored", "", "", 1);
  second.attributes = {attribute("team:activity", "B"),
                       attribute("team:resource", "r2")};
  trace.events = {std::move(first), std::move(second)};
  log.traces.push_back(std::move(trace));
  return log;
}

EventLog sna_log() {
  EventLog log;
  Trace first;
  first.case_id = "c1";
  first.events.push_back(make_event("c1", "A", "", "alice", 0));
  first.events.push_back(make_event("c1", "B", "", "bob", 1));
  first.events.push_back(make_event("c1", "C", "", "alice", 2));
  Trace second;
  second.case_id = "c2";
  second.events.push_back(make_event("c2", "A", "", "alice", 0));
  second.events.push_back(make_event("c2", "B", "", "carol", 1));
  Trace third;
  third.case_id = "c3";
  third.events.push_back(make_event("c3", "D", "", "", 0));
  log.traces = {std::move(first), std::move(second), std::move(third)};
  return log;
}

EventLog flattened_sna_log() {
  EventLog log;
  Trace source;
  source.case_id = "flattened-source";
  const auto add = [&](std::string activity, std::string resource,
                       std::string segment, std::size_t order) {
    auto event = make_event("flattened-source", std::move(activity), "",
                            std::move(resource), order);
    event.attributes.push_back(attribute("case:segment", std::move(segment)));
    source.events.push_back(std::move(event));
  };
  add("A", "r1", "left", 0);
  add("B", "r2", "left", 1);
  add("C", "r1", "right", 2);
  add("D", "r3", "right", 3);
  log.traces.push_back(std::move(source));
  return log;
}

EventLog custom_sna_activity_resource_log() {
  EventLog log;
  Trace source;
  source.case_id = "custom-sna";
  const auto add = [&](std::string activity, std::string resource,
                       std::size_t order) {
    auto event = make_event("custom-sna", "ignored", "", "", order);
    event.attributes = {attribute("team:activity", std::move(activity)),
                        attribute("team:resource", std::move(resource))};
    source.events.push_back(std::move(event));
  };
  add("A", "r1", 0);
  add("A", "r1", 1);
  add("B", "r1", 2);
  add("A", "r2", 3);
  add("B", "r2", 4);
  add("B", "r2", 5);
  log.traces.push_back(std::move(source));
  return log;
}

void local_diagnostics_contract() {
  const auto result = discover_local_diagnostics(diagnostic_log());
  assert(result.diagnostics.computed);
  assert(result.diagnostics.group_count == 2);
  assert(result.diagnostics.resource_count == 3);
  assert(result.group_member_contribution.at("g1").at("alice").at("A") ==
         1);
  assert(result.group_relative_focus.at("g1").at("A") == 0.5);
  assert(result.group_coverage.at("g1").at("alice") == 0.5);
}

void roles_common_contract() {
  RoleDiscoveryOptions options;
  options.threshold = 0.49;
  const auto result = discover_roles(role_log(), options);
  assert(result.diagnostics.computed && result.roles.size() == 1);
  const DiscoveredRole& role = result.roles.front();
  assert(role.activities.size() == 2);
  assert(role.resources.size() == 2);
  assert(role.resources.front().name == "r1");
  assert(role.resources.front().count == 2);
}

void network_analysis_dataframe_contract() {
  NetworkAnalysisOptions options;
  options.variant = NetworkAnalysisVariant::DATAFRAME;
  options.include_performance = true;
  const auto result = discover_network_analysis(network_log(), options);
  assert(result.variant == NetworkAnalysisVariant::DATAFRAME);
  assert(result.diagnostics.computed && result.edges.size() == 2);
  assert(result.edges.front().source == "alice");
  assert(result.edges.front().target == "bob");
  assert(result.edges.front().activity == "A");
  assert(result.edges.front().performance.size() == 1);
  assert(result.edges.front().performance.front() == 3600.0);
}

void resource_profiles_log_contract() {
  ResourceProfilesOptions options;
  options.metric = ResourceProfileMetric::DISTINCT_ACTIVITIES;
  const auto result = discover_resource_profiles(profile_log(), options);
  assert(result.metric == ResourceProfileMetric::DISTINCT_ACTIVITIES);
  assert(result.diagnostics.computed && result.rows.size() == 2);
  assert(result.rows.front().resource == "alice");
  assert(result.rows.front().value == 1.0);
  assert(result.rows.back().resource == "bob");
  assert(result.rows.back().value == 2.0);
}

void resource_profiles_pandas_contract() {
  ResourceProfilesOptions options;
  options.metric = ResourceProfileMetric::CASE_COMPLETIONS;
  options.activity_attribute = "team:activity";
  options.resource_attribute = "team:resource";
  options.timestamp_attribute = "observed_at";
  options.start_timestamp_attribute = "started_at";
  options.case_id_attribute = "case:batch";
  options.resource = "r-custom";
  options.activity = "A";
  options.t1 = "2024-01-01T00:00:00Z";
  options.t2 = "2024-01-01T00:01:00Z";
  const auto result = discover_resource_profiles(custom_attribute_log(), options);
  assert(result.diagnostics.computed && result.rows.size() == 1);
  assert(result.rows.front().resource == "r-custom");
  assert(result.rows.front().count == 2);
  assert(result.rows.front().value == 2.0);
}

void fraction_case_completions_contract() {
  ResourceProfilesOptions options;
  options.metric = ResourceProfileMetric::FRACTION_CASE_COMPLETIONS;
  const auto result = discover_resource_profiles(fraction_case_log(), options);
  assert(result.diagnostics.computed && result.rows.size() == 2);
  assert(result.rows.front().resource == "alice");
  assert(result.rows.front().count == 1);
  assert(std::abs(result.rows.front().value - 1.0 / 3.0) < 1e-12);
  assert(result.rows.back().resource == "bob");
  assert(result.rows.back().count == 2);
  assert(std::abs(result.rows.back().value - 2.0 / 3.0) < 1e-12);
}

void roles_log_contract() {
  RoleDiscoveryOptions options;
  options.variant = RoleDiscoveryVariant::LOG;
  options.threshold = 0.49;
  const auto result = discover_roles(role_log(), options);
  assert(result.variant == RoleDiscoveryVariant::LOG);
  assert(result.diagnostics.computed && result.roles.size() == 1);
}

void roles_pandas_contract() {
  RoleDiscoveryOptions options;
  options.variant = RoleDiscoveryVariant::PANDAS;
  options.activity_attribute = "team:activity";
  options.resource_attribute = "team:resource";
  options.threshold = 1.0;
  const auto result = discover_roles(custom_role_log(), options);
  assert(result.variant == RoleDiscoveryVariant::PANDAS);
  assert(result.diagnostics.computed);
  assert(result.diagnostics.activity_count == 2);
  assert(result.diagnostics.resource_count == 2);
  assert(result.roles.size() == 2);
}

void organizational_sna_util_contract() {
  const auto result = discover_organizational_sna(sna_log());
  const OrganizationalSnaDiagnostics& diagnostics = result.diagnostics;
  assert(diagnostics.computed);
  assert(diagnostics.trace_count == 3);
  assert(diagnostics.event_count == 6);
  assert(diagnostics.resource_count == 3);
  assert(diagnostics.edge_count == result.edges.size());
}

void sna_handover_log_contract() {
  OrganizationalSnaOptions options;
  options.variant = OrganizationalSnaVariant::HANDOVER;
  options.beta = 0.0;
  const auto result = discover_organizational_sna(sna_log(), options);
  assert(result.variant == OrganizationalSnaVariant::HANDOVER);
  assert(result.directed && result.edges.size() == 3);
  for (const auto& edge : result.edges)
    assert(std::abs(edge.weight - 1.0 / 3.0) < 1e-12);
}

void sna_working_together_log_contract() {
  OrganizationalSnaOptions options;
  options.variant = OrganizationalSnaVariant::WORKING_TOGETHER;
  const auto result = discover_organizational_sna(sna_log(), options);
  assert(result.variant == OrganizationalSnaVariant::WORKING_TOGETHER);
  assert(!result.directed && result.edges.size() == 4);
  assert(result.edges.front().source < result.edges.front().target);
}

void sna_subcontracting_log_contract() {
  OrganizationalSnaOptions options;
  options.variant = OrganizationalSnaVariant::SUBCONTRACTING;
  options.n = 2;
  const auto result = discover_organizational_sna(sna_log(), options);
  assert(result.variant == OrganizationalSnaVariant::SUBCONTRACTING);
  assert(result.directed && result.edges.size() == 1);
  assert(result.edges.front().source == 0);
  assert(result.edges.front().target == 1);
}

void sna_joint_activities_log_contract() {
  OrganizationalSnaOptions options;
  options.variant = OrganizationalSnaVariant::JOINTACTIVITIES;
  const auto result = discover_organizational_sna(sna_log(), options);
  assert(result.variant == OrganizationalSnaVariant::JOINTACTIVITIES);
  assert(!result.directed && result.edges.size() == 6);
  for (const auto& edge : result.edges) assert(std::isfinite(edge.weight));
}

void sna_handover_pandas_contract() {
  OrganizationalSnaOptions options;
  options.variant = OrganizationalSnaVariant::HANDOVER_PANDAS;
  options.beta = 0.0;
  options.case_id_attribute = "case:segment";
  const auto result = discover_organizational_sna(flattened_sna_log(), options);
  assert(result.variant == OrganizationalSnaVariant::HANDOVER_PANDAS);
  assert(result.directed && result.edges.size() == 2);
  OrganizationalSnaOptions log_options;
  log_options.variant = OrganizationalSnaVariant::HANDOVER;
  log_options.beta = 0.0;
  const auto log_result =
      discover_organizational_sna(flattened_sna_log(), log_options);
  assert(log_result.edges.size() == 3);
}

void sna_working_together_pandas_contract() {
  OrganizationalSnaOptions options;
  options.variant = OrganizationalSnaVariant::WORKING_TOGETHER_PANDAS;
  options.case_id_attribute = "case:segment";
  const auto result = discover_organizational_sna(flattened_sna_log(), options);
  assert(result.variant == OrganizationalSnaVariant::WORKING_TOGETHER_PANDAS);
  assert(!result.directed && result.edges.size() == 4);
  OrganizationalSnaOptions log_options;
  log_options.variant = OrganizationalSnaVariant::WORKING_TOGETHER;
  const auto log_result =
      discover_organizational_sna(flattened_sna_log(), log_options);
  assert(log_result.edges.size() == 6);
}

void sna_subcontracting_pandas_contract() {
  OrganizationalSnaOptions options;
  options.variant = OrganizationalSnaVariant::SUBCONTRACTING_PANDAS;
  options.n = 2;
  options.case_id_attribute = "case:segment";
  const auto result = discover_organizational_sna(flattened_sna_log(), options);
  assert(result.variant == OrganizationalSnaVariant::SUBCONTRACTING_PANDAS);
  assert(result.directed && result.edges.empty());
  OrganizationalSnaOptions log_options;
  log_options.variant = OrganizationalSnaVariant::SUBCONTRACTING;
  log_options.n = 2;
  const auto log_result =
      discover_organizational_sna(flattened_sna_log(), log_options);
  assert(log_result.edges.size() == 1);
}

void sna_joint_activities_pandas_contract() {
  OrganizationalSnaOptions options;
  options.variant = OrganizationalSnaVariant::JOINTACTIVITIES_PANDAS;
  options.activity_attribute = "team:activity";
  options.resource_attribute = "team:resource";
  const auto result = discover_organizational_sna(
      custom_sna_activity_resource_log(), options);
  assert(result.variant == OrganizationalSnaVariant::JOINTACTIVITIES_PANDAS);
  assert(!result.directed && result.resources.size() == 2);
  assert(result.edges.size() == 2);
  assert(std::abs(result.edges.front().weight + 1.0) < 1e-12);
  OrganizationalSnaOptions log_options;
  log_options.variant = OrganizationalSnaVariant::JOINTACTIVITIES;
  const auto log_result = discover_organizational_sna(
      custom_sna_activity_resource_log(), log_options);
  assert(log_result.resources.empty() && log_result.edges.empty());
}

}  // namespace

int main() {
  local_diagnostics_contract();
  roles_common_contract();
  network_analysis_dataframe_contract();
  resource_profiles_log_contract();
  resource_profiles_pandas_contract();
  fraction_case_completions_contract();
  roles_log_contract();
  roles_pandas_contract();
  organizational_sna_util_contract();
  sna_handover_log_contract();
  sna_working_together_log_contract();
  sna_subcontracting_log_contract();
  sna_joint_activities_log_contract();
  sna_handover_pandas_contract();
  sna_working_together_pandas_contract();
  sna_subcontracting_pandas_contract();
  sna_joint_activities_pandas_contract();
}
