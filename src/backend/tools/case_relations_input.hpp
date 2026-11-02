#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include "minera/timestamp.hpp"

#include "minera/case_relations.hpp"
#include "minera/log_conversion.hpp"

namespace minera::tools {
inline std::string relation_role_column(const EventTable& table,
                                       FirstClassAttributeRole role,
                                       std::string fallback) {
  for (std::size_t column = 0; column < table.columns.size(); ++column)
    for (const auto& row : table.rows)
      if (row[column] && row[column]->first_class_role == role)
        return table.columns[column];
  const auto present = [&](const std::string& name) {
    return std::find(table.columns.begin(), table.columns.end(), name) != table.columns.end();
  };
  if (present(fallback)) return fallback;
  if (role == FirstClassAttributeRole::kCaseId && present("case:concept:name"))
    return "case:concept:name";
  if (role == FirstClassAttributeRole::kTimestamp && present("time:timestamp"))
    return "time:timestamp";
  return fallback;
}

// True when `key` names the case-id column of a batch-imported CSV table.
// Those ids carry the reference's typed text, so relation keys joined with
// them are typed the same way (type_case_relation_column_like_csv_log).
inline bool csv_case_id_key(const EventTable& table, const std::string& key) {
  for (std::size_t column = 0; column < table.columns.size(); ++column)
    for (const auto& row : table.rows)
      if (row[column] && row[column]->first_class_role == FirstClassAttributeRole::kCaseId)
        return table.columns[column] == key;
  return false;
}

inline void relation_column_alias(EventTable& table, const std::string& source,
                                  const std::string& destination) {
  if (source == destination) return;
  const auto found = std::find(table.columns.begin(), table.columns.end(), source);
  if (found == table.columns.end()) return;
  const auto index = static_cast<std::size_t>(found - table.columns.begin());
  const auto occupied = std::find(table.columns.begin(), table.columns.end(), destination);
  if (occupied != table.columns.end()) {
    const auto occupied_index = static_cast<std::size_t>(occupied - table.columns.begin());
    std::string preserved = "event:" + destination;
    while (std::find(table.columns.begin(), table.columns.end(), preserved) != table.columns.end())
      preserved = "event:" + preserved;
    table.columns[occupied_index] = preserved;
    for (auto& row : table.rows)
      if (row[occupied_index]) row[occupied_index]->key = preserved;
  }
  table.columns.push_back(destination);
  if (!table.dtypes.empty()) table.dtypes.push_back(table.dtypes[index]);
  if (!table.categorical_dtypes.empty())
    table.categorical_dtypes.push_back(table.categorical_dtypes[index]);
  for (auto& row : table.rows) {
    auto cell = row[index];
    if (cell) cell->key = destination;
    row.push_back(std::move(cell));
  }
}

inline EventTable relation_xes_table(const EventLog& log,
                                    const CaseRelationsOptions& options,
                                    bool explicit_case, bool explicit_timestamp) {
  LogConversionOptions conversion;
  auto table = event_log_to_table(log, conversion);
  const bool canonical_case = !explicit_case || options.case_id_key == "case_id" ||
                              options.case_id_key == "case:concept:name" ||
                              options.case_id_key == "concept:name";
  const auto case_source = canonical_case ? std::string("case:concept:name")
      : (options.case_id_key.starts_with("case:") ? options.case_id_key
                                                  : "case:" + options.case_id_key);
  if (std::find(table.columns.begin(), table.columns.end(), case_source) == table.columns.end())
    throw std::invalid_argument("case relations: missing XES trace case attribute " + case_source);
  relation_column_alias(table, case_source, options.case_id_key);
  if (!explicit_timestamp) {
    const auto source = relation_role_column(table, FirstClassAttributeRole::kTimestamp,
                                            "time:timestamp");
    relation_column_alias(table, source, options.timestamp_key);
  }
  // The public log-to-dataframe surface infers scalar columns. These basic
  // dtype declarations allow the merger to promote missing numeric values.
  for (std::size_t column = 0; column < table.columns.size(); ++column) {
    std::string dtype;
    bool missing = false;
    bool negative_integer = false;
    for (const auto& row : table.rows) {
      if (!row[column] || row[column]->is_null) { missing = true; continue; }
      const auto& cell = *row[column];
      if (const auto* number = std::get_if<std::int64_t>(&cell.typed_value))
        negative_integer = negative_integer || *number < 0;
      std::string current = "object";
      if (cell.container == AttributeContainerKind::kScalar) {
        switch (cell.kind) {
          case AttributeKind::kString: current = "str"; break;
          case AttributeKind::kInt:
            current = std::holds_alternative<std::uint64_t>(cell.typed_value) ? "uint64" : "int64"; break;
          case AttributeKind::kFloat: current = "float64"; break;
          case AttributeKind::kBoolean: current = "bool"; break;
          case AttributeKind::kDate:
            current = "datetime64[us, UTC]";
            if (const auto* date = std::get_if<NativeDate>(&cell.typed_value))
              if (date->resolution == NativeDateResolution::kNanosecond)
                current = "datetime64[ns, UTC]";
            break;
          default: break;
        }
      }
      if (dtype.empty()) dtype = current;
      else if (dtype != current) {
        const auto numeric = [](const std::string& name) {
          return name == "int64" || name == "uint64" || name == "float64";
        };
        if (numeric(dtype) && numeric(current)) {
          if (dtype == "float64" || current == "float64") dtype = "float64";
          else dtype = negative_integer ? "object" : "uint64";
        } else if (dtype.starts_with("datetime64[") && current.starts_with("datetime64[")) {
          dtype = "datetime64[ns, UTC]";
        } else dtype = "object";
      }
    }
    if (missing && (dtype == "int64" || dtype == "uint64")) dtype = "float64";
    if (missing && dtype == "bool") dtype = "object";
    table.dtypes.push_back(dtype.empty() ? "object" : dtype);
  }
  return table;
}
inline void relation_csv_dates(EventTable& table, const std::string& timestamp) {
  const auto found = std::find(table.columns.begin(), table.columns.end(), timestamp);
  if (found == table.columns.end()) return;
  const auto column = static_cast<std::size_t>(found - table.columns.begin());
  for (auto& row : table.rows) {
    auto& cell = row[column];
    if (!cell || cell->is_null || cell->kind != AttributeKind::kDate) continue;
    const auto parsed = parse_timestamp_exact(cell->value);
    if (!parsed) throw std::invalid_argument("case relations: invalid CSV timestamp");
    cell->typed_value = NativeDate{cell->value, parsed->nanoseconds % 1000 != 0
        ? NativeDateResolution::kNanosecond : NativeDateResolution::kMicrosecond};
  }
  if (!table.dtypes.empty()) table.dtypes[column] = "object";
}
}  // namespace minera::tools
