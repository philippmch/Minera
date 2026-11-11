#include "minera/dataframe_columns.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/timestamp.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
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

// A missing cell: NaN (a float), or NaT in a date column. NaT is neither a
// string nor a number to the default feature selection (pm4py leaves a date
// column with gaps out), and its category is "NaT".
EventAttribute missing_cell(const DataFrameColumn& column) {
  if (column.type == DataFrameColumnType::kDate)
    return EventAttribute{column.key, "NaT", AttributeKind::kDate};
  return float_cell(column.key, std::numeric_limits<double>::quiet_NaN());
}

}  // namespace

// A CSV timestamp as format_dataframe's to_datetime(utc=True) holds it: the
// UTC instant, printed with "+00:00" (nullopt when the text is no instant).
std::optional<EventAttribute> csv_utc_date_cell(const std::string& key, const std::string& text) {
  const auto exact = parse_timestamp_exact(text);
  if (!exact) return std::nullopt;
  std::int64_t days = exact->seconds / 86400;
  std::int64_t rest = exact->seconds % 86400;
  if (rest < 0) {
    rest += 86400;
    --days;
  }
  // Civil date from days since 1970-01-01 (H. Hinnant's algorithm).
  const std::int64_t z = days + 719468;
  const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const std::int64_t doe = z - era * 146097;
  const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const std::int64_t mp = (5 * doy + 2) / 153;
  const std::int64_t day = doy - (153 * mp + 2) / 5 + 1;
  const std::int64_t month = mp < 10 ? mp + 3 : mp - 9;
  const std::int64_t year = yoe + era * 400 + (month <= 2 ? 1 : 0);
  if (year < 1 || year > 9999) return std::nullopt;
  char buffer[48];
  std::snprintf(buffer, sizeof(buffer), "%04lld-%02lld-%02lldT%02lld:%02lld:%02lld",
                static_cast<long long>(year), static_cast<long long>(month),
                static_cast<long long>(day), static_cast<long long>(rest / 3600),
                static_cast<long long>(rest / 60 % 60), static_cast<long long>(rest % 60));
  std::string value(buffer);
  if (exact->nanoseconds != 0) {
    char fraction[16];
    if (exact->nanoseconds % 1000 == 0)
      std::snprintf(fraction, sizeof(fraction), ".%06u", exact->nanoseconds / 1000);
    else
      std::snprintf(fraction, sizeof(fraction), ".%09u", exact->nanoseconds);
    value += fraction;
  }
  value += "+00:00";
  EventAttribute cell{key, value, AttributeKind::kDate};
  cell.typed_value = NativeDate{value};
  return cell;
}

namespace {

std::optional<EventAttribute> csv_value(const Event& event, const DataFrameColumn& column,
                                        std::vector<char>& listed) {
  if (column.role == FirstClassAttributeRole::kCaseId ||
      column.role == FirstClassAttributeRole::kTimestamp) {
    // A copied column replaces an attribute column of the same name.
    for (std::size_t i = 0; i < event.attributes.size(); ++i)
      if (event.attributes[i].key == column.key) listed[i] = 1;
    if (column.role == FirstClassAttributeRole::kCaseId)
      return event.has_case_id() && event.case_id_state != FirstClassAttributeState::kNull
                 ? std::optional(csv_dataframe_cell(column, event.case_id))
                 : std::nullopt;
    if (!event.has_timestamp() || event.timestamp_state == FirstClassAttributeState::kNull)
      return std::nullopt;
    return csv_utc_date_cell(column.key, event.timestamp);
  }
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

namespace {

// The cells one frame column holds, for pandas' dtype inference.
struct SeenColumn {
  std::size_t order = 0;
  std::size_t count = 0;
  bool types[6] = {};
  // Ints above INT64_MAX and below zero: pandas keeps such a column of
  // Python ints as objects, with or without gaps or floats (observed).
  bool above_int64 = false;
  bool negative = false;
  bool nan = false;  // a float NaN cell (no kind of its own)
};

void note_cell(std::map<std::string, SeenColumn>& seen, const std::string& key,
               const EventAttribute& value) {
  auto [entry, inserted] = seen.try_emplace(key);
  if (inserted) entry->second.order = seen.size();
  // A float NaN cell (pm4py writes a frame's NaN as <float value="nan">) is
  // a missing value to pandas' inference: text with NaN is a str column,
  // dates with NaN a datetime column (observed on read_xes).
  if (const auto* real = std::get_if<double>(&value.typed_value);
      real && std::isnan(*real) && cell_type(value) == CellType::kFloat) {
    entry->second.nan = true;
    return;
  }
  ++entry->second.count;
  entry->second.types[static_cast<int>(cell_type(value))] = true;
  if (const auto* natural = std::get_if<std::uint64_t>(&value.typed_value))
    if (*natural > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
      entry->second.above_int64 = true;
  if (const auto* integer = std::get_if<std::int64_t>(&value.typed_value))
    if (*integer < 0) entry->second.negative = true;
}

// The columns in first-seen order, each typed as pandas types its cells over
// `rows` rows (a row without the key holds NaN).
std::vector<DataFrameColumn> typed_columns(const std::map<std::string, SeenColumn>& seen,
                                           std::size_t rows) {
  std::vector<std::pair<std::size_t, DataFrameColumn>> ordered;
  for (const auto& [key, entry] : seen) {
    const bool missing = entry.count < rows;
    const auto has = [&](CellType type) { return entry.types[static_cast<int>(type)]; };
    int kinds = 0;
    for (const bool present : entry.types) kinds += present ? 1 : 0;
    DataFrameColumn column;
    column.key = key;
    if (kinds == 0) column.type = DataFrameColumnType::kFloat64;  // NaN only
    else if (entry.above_int64 && entry.negative) column.type = DataFrameColumnType::kObject;
    else if (kinds == 1 && has(CellType::kInt) && !missing) column.type = DataFrameColumnType::kInt64;
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
  std::vector<DataFrameColumn> columns;
  for (auto& [order, column] : ordered) columns.push_back(std::move(column));
  return columns;
}

// Whether a trace attribute is the case id (the frame's case:concept:name).
bool trace_case_id_attribute(const Trace& trace, const EventAttribute& attribute) {
  return attribute.key == "concept:name" ||
         attribute.first_class_role == FirstClassAttributeRole::kCaseId ||
         (!trace.case_id_source_key.empty() && attribute.key == trace.case_id_source_key);
}

// The value a trace holds for `key` in the frame (a dict keeps the last).
std::optional<EventAttribute> xes_trace_value(const Trace& trace, const std::string& key) {
  const EventAttribute* found = nullptr;
  for (const auto& attribute : trace.attributes)
    if (attribute.key == key && !trace_case_id_attribute(trace, attribute)) found = &attribute;
  if (!found) return std::nullopt;
  return *found;
}

// A typed XES cell as its column holds it: a float64 column holds its ints
// as floats.
EventAttribute xes_column_cell(const DataFrameColumn& column, EventAttribute cell) {
  if (column.type == DataFrameColumnType::kFloat64 && !cell.is_null) {
    if (const auto* integer = std::get_if<std::int64_t>(&cell.typed_value))
      return float_cell(column.key, static_cast<double>(*integer));
    if (const auto* natural = std::get_if<std::uint64_t>(&cell.typed_value))
      return float_cell(column.key, static_cast<double>(*natural));
  }
  return cell;
}

}  // namespace

DataFrameColumns xes_dataframe_columns(const EventLog& log) {
  std::map<std::string, SeenColumn> seen;
  std::map<std::string, SeenColumn> trace_seen;
  std::size_t events = 0;
  std::size_t traces = 0;
  for (const auto& trace : log.traces) {
    // read_xes's frame has rows only for events: a trace without events is
    // not in it, and its attributes type nothing.
    if (!trace.events.empty()) {
      ++traces;
      std::map<std::string, EventAttribute> cells;
      for (const auto& attribute : trace.attributes)
        if (!trace_case_id_attribute(trace, attribute))
          if (auto value = xes_trace_value(trace, attribute.key)) cells[attribute.key] = std::move(*value);
      for (const auto& [key, value] : cells) note_cell(trace_seen, key, value);
    }
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
      for (const auto& [key, value] : cells) note_cell(seen, key, value);
    }
  }
  DataFrameColumns result;
  result.csv_text = false;
  result.columns = typed_columns(seen, events);
  result.trace_columns = typed_columns(trace_seen, traces);
  return result;
}

EventLog dataframe_frame_event_log(const EventLog& log) {
  if (!log.dataframe_columns) return log;
  EventLog result = log;
  const auto& frame = *log.dataframe_columns;
  for (auto& trace : result.traces) {
    // A trace holds every case: column too (typed by the column, a missing
    // one NaN or NaT), in place of its own attribute of that key.
    if (!frame.trace_columns.empty() || frame.csv_text) {
      std::vector<EventAttribute> attributes;
      std::vector<char> done(frame.trace_columns.size(), 0);
      for (const auto& attribute : trace.attributes) {
        // A CSV trace's concept:name is the case id (format_dataframe's
        // copy replaces a case:concept:name column beside another case
        // column).
        if (frame.csv_text && attribute.key == "concept:name") continue;
        const auto column = std::find_if(frame.trace_columns.begin(), frame.trace_columns.end(),
                                         [&](const auto& c) { return c.key == attribute.key; });
        if (column != frame.trace_columns.end() && column->role == FirstClassAttributeRole::kCaseId)
          continue;  // the case id under its own header, added below
        if (column == frame.trace_columns.end() || trace_case_id_attribute(trace, attribute)) {
          attributes.push_back(attribute);
          continue;
        }
        auto& emitted = done[column - frame.trace_columns.begin()];
        if (emitted) continue;
        emitted = 1;
        if (!frame.csv_text) {
          attributes.push_back(xes_column_cell(*column, *xes_trace_value(trace, attribute.key)));
          continue;
        }
        const auto last = std::find_if(trace.attributes.rbegin(), trace.attributes.rend(),
                                       [&](const auto& a) { return a.key == attribute.key; });
        attributes.push_back(last->is_null ? missing_cell(*column)
                                           : csv_dataframe_cell(*column, last->value));
      }
      for (std::size_t i = 0; i < frame.trace_columns.size(); ++i) {
        if (done[i]) continue;
        const auto& column = frame.trace_columns[i];
        attributes.push_back(column.role == FirstClassAttributeRole::kCaseId && trace.has_case_id() &&
                                     trace.case_id_state != FirstClassAttributeState::kNull
                                 ? csv_dataframe_cell(column, trace.case_id)
                                 : missing_cell(column));
      }
      trace.attributes = std::move(attributes);
    }
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
          if (cell) cell = xes_column_cell(column, std::move(*cell));
        }
        attributes.push_back(cell ? std::move(*cell) : missing_cell(column));
      }
      if (frame.csv_text) {
        const auto index_cell = [&](const char* key, const std::vector<std::int64_t>& values) {
          if (event.input_order >= values.size()) return;
          EventAttribute cell{key, std::to_string(values[event.input_order]), AttributeKind::kInt};
          cell.typed_value = values[event.input_order];
          attributes.push_back(std::move(cell));
        };
        index_cell("@@index", frame.row_index);
        index_cell("@@case_index", frame.case_index);
        // format_dataframe overwrites a concept:name column with the
        // activity column's copy.
        if (event.activity_source_key != "concept:name")
          for (std::size_t i = 0; i < event.attributes.size(); ++i)
            if (event.attributes[i].key == "concept:name") listed[i] = 1;
        // The resource and lifecycle columns keep their headers: there is no
        // org:resource (or lifecycle:transition) unless that is the header.
        event.resource.clear();
        event.resource_state = FirstClassAttributeState::kMissing;
        event.resource_attribute.reset();
        event.resource_source_key.clear();
        event.lifecycle.clear();
        event.lifecycle_state = FirstClassAttributeState::kMissing;
        event.lifecycle_attribute.reset();
        event.lifecycle_source_key.clear();
      }
      for (std::size_t i = 0; i < event.attributes.size(); ++i)
        if (!listed[i]) attributes.push_back(std::move(event.attributes[i]));
      event.attributes = std::move(attributes);
    }
  }
  return result;
}

}  // namespace minera
