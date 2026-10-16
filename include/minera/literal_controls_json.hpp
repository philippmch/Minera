#pragma once

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "minera/dfg_precision.hpp"
#include "minera/ilp_miner.hpp"
#include "minera/log_to_features.hpp"
#include "minera/log_to_target.hpp"
#include "minera/request_params.hpp"

namespace minera {
// Scoped opt-in transports. Existing legacy parsers remain unchanged.
[[nodiscard]] bool literal_controls_requested(std::string_view group, const FieldPresence&);
[[nodiscard]] std::vector<std::string> parse_literal_string_vector_json(std::string_view);
[[nodiscard]] ILPMinerOptions parse_literal_ilp_options_json(
    std::string_view, std::set<std::string>* supplied = nullptr);
[[nodiscard]] DfgPrecisionOptions parse_literal_dfg_precision_options_json(
    std::string_view, std::set<std::string>* supplied = nullptr);
[[nodiscard]] LogToTargetOptions parse_log_to_target_literal_params(
    const FieldLookup&, const FieldPresence& = {});
[[nodiscard]] LogToFeaturesOptions parse_log_to_features_literal_params(
    const FieldLookup&, const FieldPresence& = {},
    // Append only structurally parsed exact controls; retain a successful
    // prefix if a later control fails. The full options return is transactional.
    std::vector<std::string>* parsed_controls = nullptr);
[[nodiscard]] ILPMinerOptions parse_ilp_literal_params(
    const FieldLookup&, const FieldPresence& = {}, std::set<std::string>* supplied = nullptr);

// Complete native stored fields, including all56 feature options and inactive
// result row families. Strings use existing65 NativeString, integers are exact.
[[nodiscard]] std::string serialize_literal_controls_options_json(const LogToTargetOptions&);
[[nodiscard]] std::string serialize_literal_controls_options_json(const LogToFeaturesOptions&);
[[nodiscard]] std::string serialize_literal_controls_options_json(const ILPMinerOptions&);
[[nodiscard]] std::string serialize_literal_controls_options_json(const DfgPrecisionOptions&);
[[nodiscard]] std::string serialize_literal_controls_result_json(const LogToTargetResult&);
[[nodiscard]] std::string serialize_literal_controls_result_json(const LogToFeaturesResult&);
[[nodiscard]] std::string serialize_literal_controls_result_json(const ILPMinerResult&);
[[nodiscard]] std::string serialize_literal_controls_result_json(const DfgPrecisionResult&);
// null means scalar UTF-8 and a syntactically valid JSON object. The returned
// transport code never claims an actual native exporter threw.
[[nodiscard]] std::optional<std::string> literal_controls_transport_error(std::string_view);
}  // namespace minera
