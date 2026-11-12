#include "minera/frame_keys.hpp"

#include "dataframe_cell.hpp"
#include "frame_cells.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/timestamp.hpp"

#include <algorithm>
#include <map>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <unordered_map>
#include <variant>

namespace minera {
namespace {

constexpr std::string_view kCasePrefix = "case:";

// str() of a UTC date as the frame holds it ("2020-01-01 10:00:00+00:00",
// microseconds or nanoseconds when there is a fraction).
std::string python_date_text(const std::string& text) {
  const auto exact = parse_timestamp_exact(text);
  if (!exact) return text;
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
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%04lld-%02lld-%02lld %02lld:%02lld:%02lld",
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
  return value + "+00:00";
}

FrameValue attribute_value(const EventAttribute& attribute) {
  FrameValue value;
  if (attribute.is_null || attribute.kind == AttributeKind::kNA) return value;
  if (const auto* integer = std::get_if<std::int64_t>(&attribute.typed_value)) {
    value.text = std::to_string(*integer);
    value.value_class = FrameValueClass::kNumber;
    return value;
  }
  if (const auto* natural = std::get_if<std::uint64_t>(&attribute.typed_value)) {
    value.text = std::to_string(*natural);
    value.value_class = FrameValueClass::kNumber;
    return value;
  }
  if (const auto* real = std::get_if<double>(&attribute.typed_value)) {
    if (std::isnan(*real)) return value;
    value.text = reference_csv_float_text(*real);
    value.value_class = FrameValueClass::kNumber;
    return value;
  }
  if (const auto* flag = std::get_if<bool>(&attribute.typed_value)) {
    value.text = *flag ? "True" : "False";
    value.value_class = FrameValueClass::kNumber;
    return value;
  }
  switch (attribute.kind) {
    case AttributeKind::kInt:
    case AttributeKind::kFloat:
      if (attribute.value == "nan" || attribute.value == "NaN") return value;
      value.text = attribute.value;
      value.value_class = FrameValueClass::kNumber;
      return value;
    case AttributeKind::kBoolean:
      value.text = attribute.value;
      value.value_class = FrameValueClass::kNumber;
      return value;
    case AttributeKind::kDate:
      if (attribute.value == "NaT") return value;
      value.text = python_date_text(attribute.value);
      value.value_class = FrameValueClass::kDate;
      return value;
    default:
      value.text = attribute.value;
      value.value_class = FrameValueClass::kText;
      return value;
  }
}

const EventAttribute* last_attribute(const std::vector<EventAttribute>& attributes,
                                     const std::string& key) {
  const EventAttribute* found = nullptr;
  for (const auto& attribute : attributes)
    if (attribute.key == key) found = &attribute;
  return found;
}

FrameValue text_value(const std::string& text) {
  return FrameValue{text, FrameValueClass::kText};
}

}  // namespace

std::set<std::string> frame_keys(const EventLog& frame, FrameKeyScope scope) {
  std::set<std::string> keys;
  if (!frame.dataframe_columns) return keys;
  const auto& columns = *frame.dataframe_columns;
  for (const auto& column : columns.columns) keys.insert(column.key);
  keys.insert("concept:name");
  bool timestamps = false;
  for (const auto& trace : frame.traces)
    for (const auto& event : trace.events) {
      if (!event.activity_source_key.empty()) keys.insert(event.activity_source_key);
      if (event.has_timestamp()) timestamps = true;
    }
  if (columns.csv_text) {
    keys.insert("@@index");
    keys.insert("@@case_index");
  } else if (timestamps) {
    keys.insert("time:timestamp");
  }
  if (scope == FrameKeyScope::kDataFrame) {
    keys.insert("case:concept:name");
    for (const auto& column : columns.trace_columns)
      keys.insert(std::string(kCasePrefix) + column.key);
  }
  return keys;
}

void require_frame_key(const std::set<std::string>& keys, const std::string& key) {
  if (!keys.contains(key)) throw std::invalid_argument("KeyError: '" + key + "'");
}

namespace {

// A CSV activity column under its own header, typed as read_csv typed it.
std::optional<EventAttribute> typed_activity_cell(const EventLog& frame, const Event& event,
                                                  const std::string& key) {
  if (!frame.dataframe_columns || !frame.dataframe_columns->csv_text ||
      event.activity_source_key.empty() || key != event.activity_source_key ||
      key == "concept:name")
    return std::nullopt;
  DataFrameColumn column;
  column.key = key;
  column.type = frame.dataframe_columns->activity_type;
  if (!event.has_activity()) {
    EventAttribute missing{key, "", AttributeKind::kNA};
    missing.is_null = true;
    return missing;
  }
  return csv_dataframe_cell(column, event.activity);
}

}  // namespace

FrameValue frame_value(const EventLog& frame, const Trace& trace, const Event& event,
                       const std::string& key, FrameKeyScope scope) {
  if (const auto cell = typed_activity_cell(frame, event, key)) return attribute_value(*cell);
  // format_dataframe's concept:name is the activity as text; an XES activity
  // under another key holds the same values.
  if (key == "concept:name" ||
      (!event.activity_source_key.empty() && key == event.activity_source_key))
    return event.has_activity() ? text_value(event.activity) : FrameValue{};
  if (const auto* attribute = last_attribute(event.attributes, key))
    return attribute_value(*attribute);
  if (key == "time:timestamp" && event.has_timestamp())
    return FrameValue{python_date_text(event.timestamp), FrameValueClass::kDate};
  if (scope == FrameKeyScope::kDataFrame && key.starts_with(kCasePrefix)) {
    if (key == "case:concept:name")
      return trace.has_case_id() ? text_value(trace.case_id) : FrameValue{};
    // A row's own case cell (CSV rows, merged XES cases): pandas keeps one
    // value per row, which may differ within a case (round AZ).
    if (const auto own = dataframe_cell::case_row_value(event, key)) {
      if (!*own) return {};
      const std::string bare = key.substr(kCasePrefix.size());
      const auto* stored = last_attribute(event.case_row_attributes, bare);
      const DataFrameColumn* column = nullptr;
      if (frame.dataframe_columns)
        for (const auto& candidate : frame.dataframe_columns->trace_columns)
          if (candidate.key == bare) column = &candidate;
      if (frame.dataframe_columns && frame.dataframe_columns->csv_text && column)
        return attribute_value(csv_dataframe_cell(*column, **own));
      auto value = stored ? attribute_value(*stored) : text_value(**own);
      if (column && column->type == DataFrameColumnType::kFloat64 && stored &&
          std::holds_alternative<std::int64_t>(stored->typed_value))
        value.text = reference_csv_float_text(static_cast<double>(std::get<std::int64_t>(stored->typed_value)));
      return value;
    }
    if (const auto* attribute = last_attribute(trace.attributes, key.substr(kCasePrefix.size())))
      return attribute_value(*attribute);
  }
  return {};
}

namespace {

// A frame cell as the Python object pandas holds: dates are UTC Timestamps.
std::optional<python::Scalar> cell_scalar(const EventAttribute& attribute) {
  if (attribute_value(attribute).missing()) return std::nullopt;
  auto scalar = python::from_attribute(attribute);
  if (python::is_missing(scalar)) return std::nullopt;
  if (scalar.kind == python::Scalar::Kind::kDate) {
    scalar.date.timezone_aware = true;
    scalar.date.offset_minutes = 0;
  }
  return scalar;
}

}  // namespace

std::optional<python::Scalar> frame_scalar(const EventLog& frame, const Trace& trace,
                                           const Event& event, const std::string& key,
                                           FrameKeyScope scope) {
  if (const auto cell = typed_activity_cell(frame, event, key)) return cell_scalar(*cell);
  if (key == "concept:name" ||
      (!event.activity_source_key.empty() && key == event.activity_source_key))
    return event.has_activity() ? std::optional(python::text(event.activity)) : std::nullopt;
  if (const auto* attribute = last_attribute(event.attributes, key)) return cell_scalar(*attribute);
  if (key == "time:timestamp" && event.has_timestamp()) {
    auto date = python::date_literal(event.timestamp);
    if (!date) return std::nullopt;
    date->date.timezone_aware = true;
    date->date.offset_minutes = 0;
    return date;
  }
  if (scope == FrameKeyScope::kDataFrame && key.starts_with(kCasePrefix)) {
    if (key == "case:concept:name")
      return trace.has_case_id() ? std::optional(python::text(trace.case_id)) : std::nullopt;
    if (const auto* attribute = last_attribute(trace.attributes, key.substr(kCasePrefix.size())))
      return cell_scalar(*attribute);
  }
  return std::nullopt;
}

std::vector<FrameRow> frame_rows(const EventLog& frame) {
  std::vector<FrameRow> rows;
  rows.reserve(frame.event_count());
  bool ordered = true;
  for (const auto& trace : frame.traces)
    for (const auto& event : trace.events) {
      if (!rows.empty() && event.input_order < rows.back().event.input_order) ordered = false;
      rows.push_back({trace, event});
    }
  if (ordered || !frame.dataframe_columns || frame.dataframe_columns->csv_text) return rows;
  std::vector<std::size_t> order(rows.size());
  for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
    return rows[a].event.input_order < rows[b].event.input_order;
  });
  std::vector<FrameRow> sorted;
  sorted.reserve(rows.size());
  for (const auto i : order) sorted.push_back(rows[i]);
  return sorted;
}

FrameCases frame_cases(const EventLog& frame) {
  FrameCases cases{frame_rows(frame), {}, {}, true, true};
  for (std::size_t i = 0; i < cases.rows.size(); ++i) {
    const auto [it, added] = cases.extent.emplace(&cases.rows[i].trace, std::pair<std::size_t, std::size_t>{i, 0});
    if (!added && it->second.first + it->second.second != i) cases.adjacent = false;
    ++it->second.second;
  }
  for (const auto& trace : frame.traces)
    if (cases.extent.contains(&trace)) cases.traces.push_back(&trace);
  std::vector<const Trace*> by_row = cases.traces;
  std::stable_sort(cases.traces.begin(), cases.traces.end(),
                   [](const Trace* left, const Trace* right) { return left->case_id < right->case_id; });
  std::sort(by_row.begin(), by_row.end(), [&](const Trace* left, const Trace* right) {
    return cases.extent.at(left).first < cases.extent.at(right).first;
  });
  cases.sorted = cases.adjacent && by_row == cases.traces;
  return cases;
}

std::vector<Trace> relabel_frame_rows(const std::vector<FrameRow>& rows,
                                      const std::vector<const std::string*>& labels) {
  std::vector<Trace> traces;
  std::vector<std::vector<const FrameRow*>> members;
  std::unordered_map<std::string, std::size_t> index;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    if (labels[i] == nullptr) continue;
    const auto [it, added] = index.emplace(*labels[i], traces.size());
    if (added) {
      traces.emplace_back();
      copy_trace_context(rows[i].trace, traces.back());
      set_trace_case_id(traces.back(), *labels[i]);
      members.emplace_back();
    }
    members[it->second].push_back(&rows[i]);
  }
  const auto cells = [](const FrameRow& row) {
    return row.event.case_row_attributes.empty()
               ? row.trace.attributes
               : std::vector<EventAttribute>(row.event.case_row_attributes.begin(),
                                             row.event.case_row_attributes.end());
  };
  for (std::size_t t = 0; t < traces.size(); ++t) {
    auto& trace = traces[t];
    const auto& own = members[t];
    bool mixed = false;
    for (const auto* row : own)
      if (&row->trace != &own.front()->trace && cells(*row) != cells(*own.front())) mixed = true;
    std::vector<std::string> keys;
    if (mixed)
      for (const auto* row : own)
        for (const auto& attribute : cells(*row))
          if (std::find(keys.begin(), keys.end(), attribute.key) == keys.end()) keys.push_back(attribute.key);
    trace.events.reserve(own.size());
    for (const auto* row : own) {
      Event event = row->event;
      event.case_id = trace.case_id;
      if (mixed) {
        const auto source = cells(*row);
        std::vector<EventAttribute> row_cells;
        for (const auto& key : keys) {
          const auto found = std::find_if(source.begin(), source.end(),
                                          [&](const EventAttribute& attribute) { return attribute.key == key; });
          if (found != source.end()) {
            row_cells.push_back(*found);
          } else {
            EventAttribute missing{key, ""};
            missing.is_null = true;
            row_cells.push_back(std::move(missing));
          }
        }
        event.case_row_attributes = std::move(row_cells);
      }
      trace.events.push_back(std::move(event));
    }
  }
  return traces;
}

EventLog frame_slice_cases(const EventLog& frame) {
  const auto rows = frame_rows(frame);
  std::map<const Trace*, std::pair<std::size_t, std::size_t>> extent;  // first row, rows
  for (std::size_t i = 0; i < rows.size(); ++i)
    ++extent.emplace(&rows[i].trace, std::pair<std::size_t, std::size_t>{i, 0}).first->second.second;
  EventLog cases;
  cases.attributes = frame.attributes;
  for (const auto& trace : frame.traces) {
    const auto found = extent.find(&trace);
    if (found == extent.end()) continue;
    Trace copy;
    copy_trace_context(trace, copy);
    for (std::size_t i = found->second.first; i < found->second.first + found->second.second; ++i)
      copy.events.push_back(rows[i].event);
    cases.traces.push_back(std::move(copy));
  }
  return cases;
}

std::optional<python::Scalar> frame_row_scalar(const EventLog& frame, const Trace& trace,
                                               const Event& event, const std::string& key) {
  if (key.starts_with("case:") && key != "case:concept:name")
    if (const auto own = dataframe_cell::case_row_value(event, key)) {
      if (!*own) return std::nullopt;
      const std::string bare = key.substr(5);
      const EventAttribute* stored = nullptr;
      for (const auto& attribute : event.case_row_attributes)
        if (attribute.key == bare) stored = &attribute;
      const auto& columns = *frame.dataframe_columns;
      const DataFrameColumn* column = nullptr;
      for (const auto& candidate : columns.trace_columns)
        if (candidate.key == bare) column = &candidate;
      python::Scalar value;
      if (columns.csv_text && column) {
        value = python::from_attribute(csv_dataframe_cell(*column, **own));
      } else {
        value = stored ? python::from_attribute(*stored) : python::text(**own);
        if (column && column->type == DataFrameColumnType::kFloat64 && value.kind == python::Scalar::Kind::kInt)
          value = python::real(std::strtod(python::str(value).c_str(), nullptr));
      }
      if (python::is_missing(value)) return std::nullopt;
      if (value.kind == python::Scalar::Kind::kDate) {
        value.date.timezone_aware = true;
        value.date.offset_minutes = 0;
      }
      return value;
    }
  return frame_scalar(frame, trace, event, key, FrameKeyScope::kDataFrame);
}

// The dtype of a frame column (nullopt for no such column): the CSV and XES
// columns as typed, the case and activity copies text, the CSV activity
// column under its own header as read_csv typed it, a CSV frame's @@index ints, an XES
// time:timestamp dates.
std::optional<DataFrameColumnType> frame_column_type(const EventLog& frame, const std::string& key) {
  const auto& columns = *frame.dataframe_columns;
  // read_csv keeps a True/False column with a missing cell as objects
  // (observed black-box); the importer records it as bool. A case: column
  // too (round BA: case statistics on it raised numpy's bool subtraction).
  const auto typed = [&](const DataFrameColumn& column) {
    if (column.type == DataFrameColumnType::kBool && columns.csv_text)
      for (const auto& trace : frame.traces)
        for (const auto& event : trace.events)
          if (!frame_row_scalar(frame, trace, event, key)) return DataFrameColumnType::kObject;
    return column.type;
  };
  for (const auto& column : columns.columns)
    if (column.key == key) return typed(column);
  if (key.starts_with("case:") && key != "case:concept:name")
    for (const auto& column : columns.trace_columns)
      if ("case:" + column.key == key) return typed(column);
  if (key == "concept:name" || key == "case:concept:name") return DataFrameColumnType::kString;
  // format_dataframe adds the index columns; read_xes's frame has none
  // (round BE, observed black-box: KeyError on an XES upload).
  if ((key == "@@index" || key == "@@case_index") && columns.csv_text) return DataFrameColumnType::kInt64;
  if (key == "time:timestamp") return DataFrameColumnType::kDate;
  for (const auto& trace : frame.traces)
    for (const auto& event : trace.events)
      if (event.activity_source_key == key)
        return columns.csv_text ? columns.activity_type : DataFrameColumnType::kString;
  return std::nullopt;
}

void require_frame_columns(const std::set<std::string>& keys,
                           const std::vector<std::string>& selection) {
  std::vector<std::string> absent;
  for (const auto& key : selection)
    if (!keys.contains(key)) absent.push_back(key);
  if (absent.empty()) return;
  const auto quoted = [](const std::vector<std::string>& labels) {
    std::string list = "[";
    for (std::size_t i = 0; i < labels.size(); ++i) list += (i ? ", '" : "'") + labels[i] + "'";
    return list + "]";
  };
  if (absent.size() == selection.size())
    throw std::invalid_argument("KeyError: \"None of [Index(" + quoted(absent) +
                                ", dtype='str')] are in the [columns]\"");
  // Some labels present: pandas names each missing label once, in the order
  // of its first appearance (round BA).
  std::vector<std::string> unique;
  for (const auto& key : absent)
    if (std::find(unique.begin(), unique.end(), key) == unique.end()) unique.push_back(key);
  throw std::invalid_argument("KeyError: \"" + quoted(unique) + " not in index\"");
}

python::Scalar frame_number_scalar(const FrameValue& value) {
  if (value.text == "True") return python::boolean(true);
  if (value.text == "False") return python::boolean(false);
  if (auto integer = python::integer_literal(value.text)) return *integer;
  return python::real(std::strtod(value.text.c_str(), nullptr));
}

std::string column_type_error(std::string message, std::optional<DataFrameColumnType> type) {
  if (type == DataFrameColumnType::kObject)
    for (std::size_t at; (at = message.find("'Timestamp'")) != std::string::npos;)
      message.replace(at, 11, "'datetime.datetime'");
  return message;
}

bool pandas_row_label_keys(std::size_t rows, bool all_keys_are_columns) {
  return rows == 2 && !all_keys_are_columns;
}

std::pair<std::string, std::string> label_characters(const std::string& label) {
  std::vector<std::string> characters;
  for (std::size_t i = 0; i < label.size() && characters.size() < 2;) {
    const auto lead = static_cast<unsigned char>(label[i]);
    const std::size_t width = lead < 0x80 ? 1 : lead >= 0xf0 ? 4 : lead >= 0xe0 ? 3 : lead >= 0xc0 ? 2 : 1;
    characters.push_back(label.substr(i, width));
    i += width;
  }
  if (characters.size() < 2) throw std::invalid_argument("IndexError: string index out of range");
  return {characters[0], characters[1]};
}

double frame_number(const FrameValue& value) {
  if (value.text == "True") return 1.0;
  if (value.text == "False") return 0.0;
  return std::strtod(value.text.c_str(), nullptr);
}

std::string FrameNames::name(const FrameValue& value) {
  if (value.value_class != FrameValueClass::kNumber) return value.text;
  const auto [it, inserted] =
      numbers_.try_emplace(python::identity(frame_number_scalar(value)), value.text);
  if (inserted) number_of_.emplace(value.text, value.text);
  return it->second;
}

bool FrameNames::less(const std::string& left, const std::string& right) const {
  const auto l = number_of_.find(left);
  const auto r = number_of_.find(right);
  if (l != number_of_.end() && r != number_of_.end())
    return python::compare(frame_number_scalar(FrameValue{l->second, FrameValueClass::kNumber}),
                           frame_number_scalar(FrameValue{r->second, FrameValueClass::kNumber})) < 0;
  return left < right;
}

void require_sortable_classes(const std::set<FrameValueClass>& classes) {
  const auto python_type = [](FrameValueClass value_class) {
    switch (value_class) {
      case FrameValueClass::kText: return "str";
      case FrameValueClass::kDate: return "Timestamp";
      default: return "float";
    }
  };
  std::set<std::string> types;
  for (const auto value_class : classes) types.insert(python_type(value_class));
  if (types.size() < 2) return;
  const auto first = *types.begin();
  const auto second = *std::next(types.begin());
  throw std::invalid_argument(std::string("TypeError: '<' not supported between instances of '") +
                              first + "' and '" + second + "'");
}

}  // namespace minera
