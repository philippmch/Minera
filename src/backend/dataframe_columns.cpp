#include "minera/dataframe_columns.hpp"
#include "minera/csv_event_log_importer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace minera {
namespace {

// The Python type a typed (XES) value has in the frame.
enum class CellType { kInt, kFloat, kBool, kString, kDate, kOther };

CellType cell_type(const EventAttribute& value) {
  if (value.is_null || value.container != AttributeContainerKind::kScalar ||
      !value.children.empty() || value.scalar_wrapper || value.scalar_children_list)
    return CellType::kOther;
  if (std::holds_alternative<std::int64_t>(value.typed_value) ||
      std::holds_alternative<std::uint64_t>(value.typed_value))
    return CellType::kInt;
  if (std::holds_alternative<double>(value.typed_value)) return CellType::kFloat;
  if (std::holds_alternative<bool>(value.typed_value)) return CellType::kBool;
  if (std::holds_alternative<NativeDate>(value.typed_value)) return CellType::kDate;
  switch (value.kind) {
    case AttributeKind::kInt: return CellType::kInt;
    case AttributeKind::kFloat: return CellType::kFloat;
    case AttributeKind::kBoolean: return CellType::kBool;
    case AttributeKind::kDate: return CellType::kDate;
    case AttributeKind::kString:
    case AttributeKind::kId: return CellType::kString;
    default: return CellType::kOther;
  }
}

// The first-class fields an XES event keeps out of its attribute list (the
// selected key's original attribute and its state).
struct Selected {
  const std::optional<EventAttribute>* attribute;
  FirstClassAttributeState state;
};

std::array<Selected, 2> optional_fields(const Event& event) {
  return {Selected{&event.resource_attribute, event.resource_state},
          Selected{&event.lifecycle_attribute, event.lifecycle_state}};
}

// The value an event holds for `key` in the frame: its last attribute of
// that key (a dict keeps the last), else a selected first-class field.
std::optional<EventAttribute> xes_value(const Event& event, const std::string& key) {
  const EventAttribute* found = nullptr;
  for (const auto& attribute : event.attributes)
    if (attribute.key == key && attribute.first_class_role != FirstClassAttributeRole::kCaseId)
      found = &attribute;
  std::optional<EventAttribute> result;
  if (found) {
    result = *found;
  } else {
    for (const auto& field : optional_fields(event)) {
      if (!*field.attribute || (*field.attribute)->key != key ||
          field.state == FirstClassAttributeState::kMissing)
        continue;
      result = **field.attribute;
      if (field.state == FirstClassAttributeState::kNull) result->is_null = true;
    }
  }
  if (!result) return std::nullopt;
  if (result->first_class_role != FirstClassAttributeRole::kNone &&
      result->first_class_state == FirstClassAttributeState::kMissing)
    return std::nullopt;
  if (result->first_class_state == FirstClassAttributeState::kNull) result->is_null = true;
  result->key = key;
  result->first_class_role = FirstClassAttributeRole::kNone;
  result->first_class_state = FirstClassAttributeState::kMissing;
  result->first_class_source_key.clear();
  return result;
}

EventAttribute float_cell(const std::string& key, double value) {
  EventAttribute cell{key, reference_csv_float_text(value), AttributeKind::kFloat};
  cell.typed_value = value;
  return cell;
}

// A missing cell: NaN (a float), or NaT in a date column.
EventAttribute missing_cell(const DataFrameColumn& column) {
  if (column.type == DataFrameColumnType::kDate)
    return EventAttribute{column.key, "NaT", AttributeKind::kString};
  return float_cell(column.key, std::numeric_limits<double>::quiet_NaN());
}

std::optional<EventAttribute> csv_value(const Event& event, const DataFrameColumn& column,
                                        std::vector<char>& listed) {
  const std::string* text = nullptr;
  if (column.role == FirstClassAttributeRole::kResource) {
    if (event.resource_state == FirstClassAttributeState::kPresent && !event.resource.empty())
      text = &event.resource;
  } else if (column.role == FirstClassAttributeRole::kLifecycle) {
    if (event.lifecycle_state == FirstClassAttributeState::kPresent && !event.lifecycle.empty())
      text = &event.lifecycle;
  }
  for (std::size_t i = 0; i < event.attributes.size(); ++i) {
    if (listed[i] || event.attributes[i].key != column.key) continue;
    listed[i] = 1;
    if (column.role == FirstClassAttributeRole::kNone && !event.attributes[i].is_null)
      text = &event.attributes[i].value;
  }
  if (!text) return std::nullopt;
  return csv_dataframe_cell(column, *text);
}

}  // namespace

DataFrameColumns xes_dataframe_columns(const EventLog& log) {
  struct Seen {
    std::size_t order = 0;
    std::size_t count = 0;
    bool types[6] = {};
  };
  std::map<std::string, Seen> seen;
  std::size_t events = 0;
  const auto note = [&](const std::string& key, const EventAttribute& value) {
    auto [entry, inserted] = seen.try_emplace(key);
    if (inserted) entry->second.order = seen.size();
    ++entry->second.count;
    entry->second.types[static_cast<int>(cell_type(value))] = true;
  };
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      ++events;
      // One cell per key and event.
      std::map<std::string, EventAttribute> cells;
      for (const auto& attribute : event.attributes) {
        if (attribute.first_class_role == FirstClassAttributeRole::kCaseId) continue;
        if (event.activity_attribute && attribute.key == event.activity_attribute->key) continue;
        if (event.timestamp_attribute && attribute.key == event.timestamp_attribute->key) continue;
        if (auto value = xes_value(event, attribute.key)) cells[attribute.key] = std::move(*value);
      }
      for (const auto& field : optional_fields(event))
        if (*field.attribute)
          if (auto value = xes_value(event, (*field.attribute)->key))
            cells[(*field.attribute)->key] = std::move(*value);
      for (const auto& [key, value] : cells) note(key, value);
    }
  }
  std::vector<std::pair<std::size_t, DataFrameColumn>> ordered;
  for (const auto& [key, entry] : seen) {
    const bool missing = entry.count < events;
    const auto has = [&](CellType type) { return entry.types[static_cast<int>(type)]; };
    int kinds = 0;
    for (const bool present : entry.types) kinds += present ? 1 : 0;
    DataFrameColumn column;
    column.key = key;
    if (kinds == 1 && has(CellType::kInt) && !missing) column.type = DataFrameColumnType::kInt64;
    else if ((has(CellType::kInt) || has(CellType::kFloat)) &&
             kinds == (has(CellType::kInt) ? 1 : 0) + (has(CellType::kFloat) ? 1 : 0))
      column.type = DataFrameColumnType::kFloat64;
    else if (kinds == 1 && has(CellType::kBool) && !missing) column.type = DataFrameColumnType::kBool;
    else if (kinds == 1 && has(CellType::kString)) column.type = DataFrameColumnType::kString;
    else if (kinds == 1 && has(CellType::kDate)) column.type = DataFrameColumnType::kDate;
    else column.type = DataFrameColumnType::kObject;
    ordered.emplace_back(entry.order, std::move(column));
  }
  std::sort(ordered.begin(), ordered.end(),
            [](const auto& left, const auto& right) { return left.first < right.first; });
  DataFrameColumns result;
  result.csv_text = false;
  for (auto& [order, column] : ordered) result.columns.push_back(std::move(column));
  return result;
}

EventLog dataframe_frame_event_log(const EventLog& log) {
  if (!log.dataframe_columns) return log;
  EventLog result = log;
  const auto& frame = *log.dataframe_columns;
  for (auto& trace : result.traces) {
    for (auto& event : trace.events) {
      std::vector<EventAttribute> attributes;
      attributes.reserve(event.attributes.size() + frame.columns.size());
      std::vector<char> listed(event.attributes.size(), 0);
      for (const auto& column : frame.columns) {
        std::optional<EventAttribute> cell;
        if (frame.csv_text) {
          cell = csv_value(event, column, listed);
        } else {
          cell = xes_value(event, column.key);
          for (std::size_t i = 0; i < event.attributes.size(); ++i)
            if (event.attributes[i].key == column.key) listed[i] = 1;
          // A float64 column holds its ints as floats.
          if (cell && column.type == DataFrameColumnType::kFloat64 && !cell->is_null) {
            if (const auto* integer = std::get_if<std::int64_t>(&cell->typed_value))
              cell = float_cell(column.key, static_cast<double>(*integer));
            else if (const auto* natural = std::get_if<std::uint64_t>(&cell->typed_value))
              cell = float_cell(column.key, static_cast<double>(*natural));
          }
        }
        attributes.push_back(cell ? std::move(*cell) : missing_cell(column));
      }
      for (std::size_t i = 0; i < event.attributes.size(); ++i)
        if (!listed[i]) attributes.push_back(std::move(event.attributes[i]));
      event.attributes = std::move(attributes);
    }
  }
  return result;
}

}  // namespace minera
