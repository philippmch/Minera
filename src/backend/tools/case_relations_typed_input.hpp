#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include "minera/case_relations.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/event_table_json.hpp"
#include "minera/json_export.hpp"
#include "minera/literal_controls_json.hpp"
#include "minera/quality_workbench_json.hpp"
#include "minera/xes_export.hpp"

namespace minera::tools {
// Scoped adapter only: the legacy CSV/XES route keeps its existing parser.
inline CaseRelationsOptions typed_case_relations_options(
    const FieldLookup& lookup, const FieldPresence& present) {
  CaseRelationsOptions options;
  if (present("merge_variant")) {
    const auto variant = lookup("merge_variant");
    if (variant != "pandas" && variant != "PANDAS")
      throw std::invalid_argument("merge_variant: expected pandas");
  }
  const auto string = [&](std::string_view name, std::string& target) {
    const auto json_name = std::string(name) + "_json";
    if (present(name) && present(json_name))
      throw std::invalid_argument(std::string(name) + ": conflicting plain and JSON selectors");
    if (present(json_name)) {
      const auto values = parse_literal_string_vector_json("[" + lookup(json_name) + "]");
      if (values.size() != 1)
        throw std::invalid_argument(json_name + ": expected one NativeString");
      target = values.front();
    } else if (present(name)) {
      target = lookup(name);
    }
  };
  string("merge_timestamp_key", options.timestamp_key);
  string("merge_case_id_key", options.case_id_key);
  string("merge_left_suffix", options.left_suffix);
  string("merge_right_suffix", options.right_suffix);
  string("merge_left_column", options.relation_left_column);
  string("merge_right_column", options.relation_right_column);
  return options;
}

inline std::string typed_case_relations_options_json(const CaseRelationsOptions& options) {
  const auto q = serialize_embedding_workbench_string_json;
  return "{\"variant\":\"pandas\",\"timestamp_key\":" + q(options.timestamp_key) +
      ",\"case_id_key\":" + q(options.case_id_key) +
      ",\"left_suffix\":" + q(options.left_suffix) +
      ",\"right_suffix\":" + q(options.right_suffix) +
      ",\"relation_left_column\":" + q(options.relation_left_column) +
      ",\"relation_right_column\":" + q(options.relation_right_column) + '}';
}

inline std::string typed_case_relations_snapshot(const EventTable& base,
    const EventTable& incoming, const EventTable& relations) {
  return "{\"base\":" + serialize_quality_workbench_table_json(base) +
      ",\"incoming\":" + serialize_quality_workbench_table_json(incoming) +
      ",\"relations\":" + serialize_quality_workbench_table_json(relations) + '}';
}

inline std::string execute_typed_case_relations_json(std::string_view base_json,
    std::string_view incoming_json, std::string_view relations_json,
    const CaseRelationsOptions& options, bool include_format_table = false,
    bool include_open_log = false) {
  const auto q = serialize_embedding_workbench_string_json;
  const auto base = parse_embedding_workbench_table_json(base_json);
  const auto incoming = parse_embedding_workbench_table_json(incoming_json);
  const auto relations = parse_embedding_workbench_table_json(relations_json);
  const auto before = typed_case_relations_snapshot(base, incoming, relations);
  // No CSV adaptation, guessed dtype, inferred selector, formatting or log conversion.
  const auto merged = merge_case_relations(base, incoming, relations, options);
  std::string payload = "{\"family\":\"MERGE_CASE_RELATIONS\",\"variant\":\"pandas\","
      "\"input_format\":\"event_table_json\",\"options\":" + typed_case_relations_options_json(options) +
      ",\"input_before\":" + before + ",\"input_after\":" +
      typed_case_relations_snapshot(base, incoming, relations) +
      ",\"table\":" + serialize_quality_workbench_table_json(merged);
  std::optional<EventLog> projected;
  std::string projection_error;
  try {
    projected = case_relations_projected_log(merged, options);
    payload += ",\"projected_log\":" + serialize_embedding_workbench_raw_log_json(*projected);
  } catch (const std::exception& error) {
    projection_error = error.what();
    payload += ",\"projected_log\":null,\"log_projection_error\":" + q(projection_error);
  }
  if (include_format_table) {
    try {
      const auto exported = serialize_event_table_json(merged);
      if (const auto error = literal_controls_transport_error(exported))
        payload += ",\"format_table\":null,\"format_table_error\":" + q(*error);
      else payload += ",\"format_table\":" + exported;
    } catch (const std::exception& error) {
      payload += ",\"format_table\":null,\"format_table_error\":" + q(error.what());
    }
  }
  if (include_open_log) {
    if (!projected) {
      payload += ",\"open_log\":null,\"open_log_error\":" + q(projection_error);
    } else try {
      std::string activity = "concept:name";
      bool complete = true, first = true;
      for (const auto& trace : projected->traces) for (const auto& event : trace.events) {
        const auto key = event.activity_source_key.empty() ? "concept:name" : event.activity_source_key;
        if (first) { activity = key; first = false; }
        complete = complete && !event.activity.empty() && key == activity;
      }
      XesExportOptions export_options;
      export_options.preserve_nulls = true;
      const auto xes = serialize_xes(*projected, export_options);
      payload += ",\"open_log\":{\"xes\":" + q(xes) +
          ",\"case_count\":" + std::to_string(projected->trace_count()) +
          ",\"event_count\":" + std::to_string(projected->event_count()) +
          ",\"activity_attribute\":" + q(activity) +
          ",\"complete_activity_field\":" + (complete ? "true" : "false") + '}';
    } catch (const std::exception& error) {
      payload += ",\"open_log\":null,\"open_log_error\":" + q(error.what());
    }
  }
  return payload + '}';
}
}  // namespace minera::tools
