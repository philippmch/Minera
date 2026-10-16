#pragma once
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "minera/footprints.hpp"
namespace minera {
namespace footprints_conformance {
struct LogModelDiagnostic; struct TraceExtensiveDiagnostic;
struct FootprintProjection; struct TreeNodeDecoration;
}
struct FootprintsWorkbenchDiagnosticsOptions {std::string case_id_key;};
struct FootprintsWorkbenchCompareOptions {bool check_act_always_executed=false;};
struct FootprintsWorkbenchDecorationSets {
  FootprintViolations relations;
  std::set<std::string> start_activities,end_activities,activities_always_happening;
};
// Exact full native storage: all16 members always emit; presence flags are
// independent, optional null differs from engaged empty, native sets insert
// duplicates canonically, and duplicate DFG map edges reject. No graph IDs,
// inference, validation of logical consistency, or field-gated serialization.
[[nodiscard]] Footprint parse_footprints_workbench_footprint_json(std::string_view);
[[nodiscard]] std::vector<Footprint> parse_footprints_workbench_footprints_json(std::string_view);
[[nodiscard]] FootprintsExtensiveResult parse_footprints_workbench_extensive_json(std::string_view);
[[nodiscard]] std::vector<FootprintsExtensiveResult> parse_footprints_workbench_extensives_json(std::string_view);
[[nodiscard]] FootprintViolations parse_footprints_workbench_relations_json(std::string_view);
[[nodiscard]] std::vector<FootprintViolations> parse_footprints_workbench_violations_json(std::string_view);
[[nodiscard]] FootprintsWorkbenchDecorationSets parse_footprints_workbench_decoration_json(std::string_view);
[[nodiscard]] std::string serialize_footprints_workbench_json(const Footprint&);
[[nodiscard]] std::string serialize_footprints_workbench_json(const std::vector<Footprint>&);
[[nodiscard]] std::string serialize_footprints_workbench_json(const FootprintsExtensiveResult&);
[[nodiscard]] std::string serialize_footprints_workbench_json(const std::vector<FootprintsExtensiveResult>&);
[[nodiscard]] std::string serialize_footprints_workbench_json(const FootprintViolations&);
[[nodiscard]] std::string serialize_footprints_workbench_json(const std::vector<FootprintViolations>&);
[[nodiscard]] std::string serialize_footprints_workbench_json(const std::set<std::string>&);
[[nodiscard]] std::string serialize_footprints_workbench_json(const FootprintsWorkbenchDecorationSets&);
[[nodiscard]] std::string serialize_footprints_workbench_json(double);
[[nodiscard]] std::string serialize_footprints_workbench_json(const FootprintsFitnessSummary&);
[[nodiscard]] std::string serialize_footprints_workbench_json(const std::vector<footprints_conformance::LogModelDiagnostic>&);
[[nodiscard]] std::string serialize_footprints_workbench_json(const std::vector<footprints_conformance::TraceExtensiveDiagnostic>&);
[[nodiscard]] std::string serialize_footprints_workbench_json(const footprints_conformance::FootprintProjection&);
[[nodiscard]] std::string serialize_footprints_workbench_json(const std::vector<footprints_conformance::TreeNodeDecoration>&);
// All five groups retain every native field/default. supplied sets change
// transactionally after successful parsing; literal selectors stay untouched.
[[nodiscard]] FootprintsLogModelOptions parse_footprints_workbench_log_model_options(std::string_view,std::set<std::string>* =nullptr);
[[nodiscard]] FootprintsTraceExtensiveOptions parse_footprints_workbench_trace_extensive_options(std::string_view,std::set<std::string>* =nullptr);
[[nodiscard]] FootprintsWorkbenchDiagnosticsOptions parse_footprints_workbench_diagnostics_options(std::string_view,std::set<std::string>* =nullptr);
[[nodiscard]] FootprintsWorkbenchCompareOptions parse_footprints_workbench_compare_options(std::string_view,std::set<std::string>* =nullptr);
[[nodiscard]] FootprintsDiscoveryOptions parse_footprints_workbench_discovery_options(std::string_view,std::set<std::string>* =nullptr);
[[nodiscard]] std::string serialize_footprints_workbench_options_json(const FootprintsLogModelOptions&);
[[nodiscard]] std::string serialize_footprints_workbench_options_json(const FootprintsTraceExtensiveOptions&);
[[nodiscard]] std::string serialize_footprints_workbench_options_json(const FootprintsWorkbenchDiagnosticsOptions&);
[[nodiscard]] std::string serialize_footprints_workbench_options_json(const FootprintsWorkbenchCompareOptions&);
[[nodiscard]] std::string serialize_footprints_workbench_options_json(const FootprintsDiscoveryOptions&);
// Raw EventLog constructor reuses full Feature61 raw Trace/Event/attribute
// input explicitly, and adds every public EventDataMetadata member including
// compatibility snapshots. Omitted constructor fields keep native defaults.
[[nodiscard]] EventLog parse_footprints_workbench_raw_log_json(std::string_view);
[[nodiscard]] std::string serialize_footprints_workbench_raw_log_json(const EventLog&);
[[nodiscard]] std::set<std::string> footprints_workbench_supplied_members(std::string_view);
[[nodiscard]] std::string footprints_workbench_sha256(std::string_view);
// Exact uint64/size_t tokens, IEEE doubles and Unicode/WTF-8; strict duplicate
// keys/unknown fields/types. Parsing transport bounds:32MiB/1e6nodes/64levels;
// serializers have no input transport caps. No reference bodies are used.
}  // namespace minera
