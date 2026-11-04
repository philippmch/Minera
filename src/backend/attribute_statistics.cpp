#include "minera/attribute_statistics.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
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

struct Tally {
  std::map<std::string, std::size_t> index;
  AttributeStatisticsResult* result = nullptr;
  void add(const std::pair<std::string, std::string>& key,
           const std::string& text, std::uint64_t count) {
    const auto [it, inserted] = index.emplace(key.first, result->entries.size());
    if (inserted) result->entries.push_back({text, key.second, 0});
    result->entries[it->second].count += count;
  }
  void finish() {
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
  Tally tally;
  tally.result = &result;
  if (options.scope == AttributeStatisticsScope::kEvents) {
    for (const auto& trace : log.traces) {
      std::set<std::string> seen;
      for (const auto& event : trace.events) {
        const auto* value = event_value(trace, event, options.attribute_key);
        if (!value) continue;
        const EventAttribute* attribute = owner(event.attributes, value);
        if (!attribute) attribute = owner(event.case_row_attributes, value);
        if (!attribute) attribute = owner(trace.attributes, value);
        const auto key = python_key(attribute, *value, value == &event.timestamp);
        if (options.keep_once_per_case && !seen.insert(key.first).second) continue;
        tally.add(key, *value, 1);
      }
    }
    tally.finish();
    return result;
  }
  if (options.scope == AttributeStatisticsScope::kTraces) {
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
            tally.add(python_key(owner(event.case_row_attributes, *cell), **cell, false),
                      **cell, 1);
        }
        continue;
      }
      const auto* value = trace_value(trace, options.attribute_key);
      if (value && !trace.events.empty())
        tally.add(python_key(owner(trace.attributes, value), *value, false), *value,
                  trace.events.size());
    }
    tally.finish();
    return result;
  }
  throw std::invalid_argument("attribute statistics: unknown scope");
}

}  // namespace minera
