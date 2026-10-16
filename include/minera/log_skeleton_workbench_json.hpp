#pragma once

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "minera/log_skeleton.hpp"

namespace minera {
// Exact native storage. Omitted model fields keep empty native defaults;
// every supplied pair requires source/target. Frequency vectors retain order,
// duplicates, empty vectors and full native size_t values.
[[nodiscard]] LogSkeletonModel parse_log_skeleton_workbench_model(std::string_view input);
[[nodiscard]] std::string serialize_log_skeleton_workbench_model_json(const LogSkeletonModel& model);

// All five native fields are independent, including considered_constraints
// and considered_constraints_present. No noise/selector semantic checks are
// invented; the selected native overload owns validation of used fields.
[[nodiscard]] LogSkeletonOptions parse_log_skeleton_conformance_workbench_options(
    std::string_view input, std::set<std::string>* supplied = nullptr);
[[nodiscard]] LogSkeletonDiagnosticsOptions parse_log_skeleton_diagnostics_workbench_options(
    std::string_view input, std::set<std::string>* supplied = nullptr);
[[nodiscard]] std::string serialize_log_skeleton_workbench_options_json(const LogSkeletonOptions& options);
[[nodiscard]] std::string serialize_log_skeleton_workbench_options_json(const LogSkeletonDiagnosticsOptions& options);

// Variant counts are exact stored size_t metadata; vector order and duplicate
// activity sequences survive input. Actual native map output uses an ordered
// [{activities:[...],value:<full conformance>}] carrier.
[[nodiscard]] LogSkeletonVariantList parse_log_skeleton_workbench_variants_json(std::string_view input);
[[nodiscard]] std::string serialize_log_skeleton_workbench_variants_json(const LogSkeletonVariantList& variants);
[[nodiscard]] std::string serialize_log_skeleton_workbench_conformance_json(const LogSkeletonVariantConformance& result);

// Standalone diagnostics consume complete caller-owned stored conformance
// vectors. All five result fields and every deviation field are required.
// Counts/frequencies are native size_t, fitness is arbitrary IEEE double,
// inactive relation/activity fields and contradictory stored state survive.
[[nodiscard]] std::vector<LogSkeletonConformance> parse_log_skeleton_workbench_conformances_json(std::string_view input);
[[nodiscard]] std::string serialize_log_skeleton_workbench_conformance_json(const LogSkeletonConformance& result);
[[nodiscard]] std::string serialize_log_skeleton_workbench_conformance_json(const std::vector<LogSkeletonConformance>& results);

// Raw Trace/Event constructor input is explicitly reused from Feature61:
// parse_declare_workbench_trace_json / serialize_declare_workbench_trace_json.
// Exact integers, literal Unicode/WTF-8 and IEEE tags NaN/INF/-INF are retained.
// Duplicate decoded JSON keys, unknown fields and wrong types reject. Parsing
// is bounded to 32MiB / one million nodes /64 levels; serializers do not cap.
}  // namespace minera
