#include "minera/attribute_statistics.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <variant>

#include "dataframe_cell.hpp"

namespace minera {
namespace {

// Statistics over an imported log follow pm4py's DataFrame
// get_event_attribute_values/get_trace_attribute_values: a missing, null or
// NaN cell is not counted. The short aliases `activity`, `timestamp`,
// `resource` and `lifecycle` name the first-class fields as well.
// The case-id column and `case:` columns are DataFrame columns too: every
// row carries them.
const std::string* event_value(const Trace& trace, const Event& event,
                               std::string_view key) {
  if (key == "activity") key = "concept:name";
  else if (key == "timestamp") key = "time:timestamp";
  else if (key == "resource") key = "org:resource";
  else if (key == "lifecycle") key = "lifecycle:transition";
  return dataframe_cell::row_value(trace, event, key);
}

const std::string* trace_value(const Trace& trace, std::string_view key) {
  if (key == "case_id" || key == "concept:name") key = "case:concept:name";
  return dataframe_cell::trace_value(trace, key);
}

// The stored attribute whose text `value` points at, if any.
const EventAttribute* owner(const std::vector<EventAttribute>& attributes,
                            const std::string* value) {
  for (const auto& attribute : attributes)
    if (&attribute.value == value) return &attribute;
  return nullptr;
}

std::string exact_integer(double number) {
  if (number == 0.0) return "0";
  char buffer[400];
  std::snprintf(buffer, sizeof buffer, "%.0f", number);
  return buffer;
}

// Python's repr of a float holding an integer value: 1.0, -3.0, 1e+16.
std::string python_float_repr(double number) {
  char buffer[64];
  const bool fixed = std::fabs(number) < 1e16;
  const auto converted = std::to_chars(
      buffer, buffer + sizeof buffer, number,
      fixed ? std::chars_format::fixed : std::chars_format::scientific);
  std::string text(buffer, converted.ptr);
  if (fixed && text.find('.') == std::string::npos) text += ".0";
  return text;
}

// A key under Python dict-key equality (bool, int and float compare as
// numbers; strings and dates only equal themselves) and the type name of
// the cell.
std::pair<std::string, std::string> python_key(const EventAttribute* attribute,
                                               const std::string& text,
                                               bool date) {
  if (!attribute) return {(date ? "d:" : "s:") + text, date ? "date" : "str"};
  const auto& typed = attribute->typed_value;
  switch (attribute->kind) {
    case AttributeKind::kBoolean: {
      bool flag = text == "true" || text == "True" || text == "TRUE" || text == "1";
      if (const auto* b = std::get_if<bool>(&typed)) flag = *b;
      return {flag ? "n:1" : "n:0", "bool"};
    }
    case AttributeKind::kInt: {
      if (const auto* i = std::get_if<std::int64_t>(&typed)) return {"n:" + std::to_string(*i), "int"};
      if (const auto* u = std::get_if<std::uint64_t>(&typed)) return {"n:" + std::to_string(*u), "int"};
      long long parsed = 0;
      const auto* first = text.data();
      const auto* last = text.data() + text.size();
      if (first != last && *first == '+') ++first;
      const auto [end, error] = std::from_chars(first, last, parsed);
      if (error == std::errc{} && end == last) return {"n:" + std::to_string(parsed), "int"};
      return {"n:" + text, "int"};
    }
    case AttributeKind::kFloat: {
      double number = 0.0;
      if (const auto* d = std::get_if<double>(&typed)) number = *d;
      else number = std::strtod(text.c_str(), nullptr);
      if (std::isfinite(number) && std::trunc(number) == number)
        return {"n:" + exact_integer(number), "float"};
      char buffer[64];
      std::snprintf(buffer, sizeof buffer, "%.17g", number);
      return {std::string("f:") + buffer, "float"};
    }
    case AttributeKind::kDate:
      return {"d:" + text, "date"};
    default:
      return {"s:" + text, "str"};
  }
}

// Where a key sorts in a pandas groupby on [case id, attribute]: by case id
// text, then by value (numbers numerically, other values by text).
struct GroupPosition {
  std::string case_id;
  bool numeric = false;
  double number = 0.0;
  std::string text;
};

bool group_before(const GroupPosition& left, const GroupPosition& right) {
  if (left.case_id != right.case_id) return left.case_id < right.case_id;
  if (left.numeric != right.numeric) return left.numeric;
  if (left.numeric && left.number != right.number) return left.number < right.number;
  return left.text < right.text;
}

GroupPosition group_position(const std::string& case_id,
                             const std::pair<std::string, std::string>& key,
                             const std::string& text) {
  GroupPosition position{case_id, false, 0.0, text};
  if (key.first.starts_with("n:") || key.first.starts_with("f:")) {
    position.numeric = true;
    position.number = std::strtod(key.first.c_str() + 2, nullptr);
  }
  return position;
}

struct Tally {
  std::map<std::string, std::size_t> index;
  AttributeStatisticsResult* result = nullptr;
  // keep_once_per_case: each key's first (case id, value) group.
  std::vector<GroupPosition> first_group;
  std::size_t add(const std::pair<std::string, std::string>& key,
                  const std::string& text, std::uint64_t count) {
    const auto [it, inserted] = index.emplace(key.first, result->entries.size());
    if (inserted) result->entries.push_back({text, key.second, 0});
    result->entries[it->second].count += count;
    return it->second;
  }
  void add_once(const std::string& case_id,
                const std::pair<std::string, std::string>& key,
                const std::string& text) {
    const auto at = add(key, text, 1);
    auto position = group_position(case_id, key, text);
    if (at == first_group.size()) first_group.push_back(std::move(position));
    else if (group_before(position, first_group[at])) first_group[at] = std::move(position);
  }
  // The reference DataFrame counts are value_counts: descending count, equal
  // counts in first appearance (over the rows, or over the sorted
  // [case id, attribute] groups when counting once per case).
  void finish() {
    std::vector<std::size_t> order(result->entries.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    if (first_group.size() == order.size() && !order.empty())
      std::stable_sort(order.begin(), order.end(), [&](std::size_t l, std::size_t r) {
        return group_before(first_group[l], first_group[r]);
      });
    std::stable_sort(order.begin(), order.end(), [&](std::size_t l, std::size_t r) {
      return result->entries[l].count > result->entries[r].count;
    });
    std::vector<AttributeValueCount> sorted;
    sorted.reserve(order.size());
    for (const auto i : order) sorted.push_back(std::move(result->entries[i]));
    result->entries = std::move(sorted);
    for (const auto& entry : result->entries) result->values[entry.value] += entry.count;
  }
};

}  // namespace

std::set<std::string> get_all_event_attributes_from_log(const EventLog& log) {
  std::set<std::string> result;
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      if (!event.activity.empty()) result.insert("concept:name");
      if (!event.timestamp.empty()) result.insert("time:timestamp");
      if (!event.resource.empty()) result.insert("org:resource");
      // Keep the transition field out of this event-attribute inventory.
      for (const auto& attribute : event.attributes)
        if (!attribute.is_null && attribute.key != "lifecycle:transition")
          result.insert(attribute.key);
    }
  }
  return result;
}

std::set<std::string> get_all_trace_attributes_from_log(const EventLog& log) {
  std::set<std::string> result;
  for (const auto& trace : log.traces) {
    for (const auto& attribute : trace.attributes) {
      if (attribute.is_null) continue;
      if (attribute.key != "concept:name" &&
          attribute.key != "case:concept:name")
        result.insert(attribute.key);
    }
  }
  return result;
}

const char* attribute_statistics_scope_name(AttributeStatisticsScope scope) {
  switch (scope) {
    case AttributeStatisticsScope::kEvents: return "EVENTS";
    case AttributeStatisticsScope::kTraces: return "TRACES";
  }
  throw std::invalid_argument("attribute statistics: unknown scope");
}

AttributeStatisticsResult discover_attribute_statistics(
    const EventLog& log, const AttributeStatisticsOptions& options) {
  if (options.attribute_key.empty())
    throw std::invalid_argument("attribute statistics: attribute key is required");
  AttributeStatisticsResult result;
  result.scope = options.scope;
  result.attribute_key = options.attribute_key;
  result.keep_once_per_case = options.keep_once_per_case;
  // One DataFrame row of the counted column: its case, key and cell text, or
  // no key when the cell is missing.
  struct Row {
    const std::string* case_id = nullptr;
    std::optional<std::pair<std::string, std::string>> key;
    const std::string* text = nullptr;
    std::uint64_t weight = 1;
  };
  std::vector<Row> rows;
  if (options.scope == AttributeStatisticsScope::kEvents) {
    for (const auto& trace : log.traces) {
      for (const auto& event : trace.events) {
        const auto* value = event_value(trace, event, options.attribute_key);
        if (!value) {
          rows.push_back({&trace.case_id, std::nullopt, nullptr, 1});
          continue;
        }
        const EventAttribute* attribute = owner(event.attributes, value);
        if (!attribute) attribute = owner(event.case_row_attributes, value);
        if (!attribute) attribute = owner(trace.attributes, value);
        rows.push_back({&trace.case_id,
                        python_key(attribute, *value, value == &event.timestamp),
                        value, 1});
      }
    }
  } else if (options.scope == AttributeStatisticsScope::kTraces) {
    // The reference counts the case column's rows. A CSV keeps each row's own
    // cell (blank rows are not counted); otherwise a DataFrame repeats the
    // case attribute on every event row, so a case contributes one count per
    // event.
    for (const auto& trace : log.traces) {
      if (dataframe_cell::has_case_rows(trace, options.attribute_key)) {
        for (const auto& event : trace.events) {
          const auto cell =
              dataframe_cell::case_row_value(event, options.attribute_key);
          if (cell && *cell)
            rows.push_back({&trace.case_id,
                            python_key(owner(event.case_row_attributes, *cell), **cell, false),
                            *cell, 1});
          else
            rows.push_back({&trace.case_id, std::nullopt, nullptr, 1});
        }
        continue;
      }
      if (trace.events.empty()) continue;
      const auto* value = trace_value(trace, options.attribute_key);
      if (value)
        rows.push_back({&trace.case_id,
                        python_key(owner(trace.attributes, value), *value, false),
                        value, trace.events.size()});
      else
        rows.push_back({&trace.case_id, std::nullopt, nullptr, trace.events.size()});
    }
  } else {
    throw std::invalid_argument("attribute statistics: unknown scope");
  }

  // The DataFrame column dtype: an int column with a missing cell, or ints
  // mixed with floats, is float64, so its ints count as floats (1.0). A bool
  // or any other value keeps the column object-typed and every cell its type.
  bool missing = false, ints = false, floats = false, others = false;
  for (const auto& row : rows) {
    if (!row.key) missing = true;
    else if (row.key->second == "int") ints = true;
    else if (row.key->second == "float") floats = true;
    else others = true;
  }
  const bool ints_as_floats =
      options.dataframe_column_types && ints && !others && (missing || floats);
  std::vector<std::string> float_texts;
  float_texts.reserve(rows.size());

  Tally tally;
  tally.result = &result;
  std::set<std::string> seen;
  const std::string* seen_case = nullptr;
  for (auto& row : rows) {
    if (!row.key) continue;
    const std::string* text = row.text;
    if (ints_as_floats && row.key->second == "int") {
      row.key->second = "float";
      float_texts.push_back(python_float_repr(
          std::strtod(row.key->first.c_str() + 2, nullptr)));
      text = &float_texts.back();
    }
    if (options.scope == AttributeStatisticsScope::kEvents &&
        options.keep_once_per_case) {
      if (seen_case != row.case_id) {
        seen.clear();
        seen_case = row.case_id;
      }
      if (seen.insert(row.key->first).second)
        tally.add_once(*row.case_id, *row.key, *text);
      continue;
    }
    tally.add(*row.key, *text, row.weight);
  }
  // Counting once per case groups by [case id, value]: the value level is
  // built from the distinct keys (each its first-seen cell), so ints beside
  // a float there are floats too, even in an object column whose bool was
  // merged into an int key (True after 1).
  if (options.dataframe_column_types && options.keep_once_per_case &&
      options.scope == AttributeStatisticsScope::kEvents) {
    bool level_ints = false, level_floats = false, level_others = false;
    for (const auto& entry : result.entries) {
      if (entry.type == "int") level_ints = true;
      else if (entry.type == "float") level_floats = true;
      else level_others = true;
    }
    if (level_ints && level_floats && !level_others) {
      for (auto& entry : result.entries) {
        if (entry.type != "int") continue;
        entry.type = "float";
        entry.value = python_float_repr(std::strtod(entry.value.c_str(), nullptr));
      }
    }
  }
  tally.finish();
  return result;
}

}  // namespace minera
