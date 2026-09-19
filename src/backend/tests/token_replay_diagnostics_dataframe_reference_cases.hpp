// Public artifact SHA256: 9911beb8f4a011f7290b095bff6275ebf4355541c88dc27ce314f2631ff38d68
#include "minera/conformance.hpp"
#include "minera/log_conversion.hpp"
#pragma once
#include "case_relations_reference_cases.hpp"
#include <limits>
#include <iomanip>
#include <sstream>
#include <charconv>

namespace case_relations_dtype_reference {
using namespace case_relations_reference;

template<class T> EventAttribute dtype_number(const std::string& key, T value) {
  auto cell = reference_number(key, value);
  if constexpr (std::is_floating_point_v<T>) {
    std::ostringstream text;
    text << std::setprecision(std::numeric_limits<T>::max_digits10) << value;
    cell.value = text.str();
  }
  return cell;
}
template<class T> bool dtype_integer_text(const std::string& text, T expected) {
  T parsed{};
  const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size() && parsed == expected;
}
inline bool dtype_projection_valid(const EventAttribute& cell) {
  if (cell.is_null || cell.container != AttributeContainerKind::kScalar) return true;
  if (const auto* v = std::get_if<std::int64_t>(&cell.typed_value)) return dtype_integer_text(cell.value, *v);
  if (const auto* v = std::get_if<std::uint64_t>(&cell.typed_value)) return dtype_integer_text(cell.value, *v);
  if (const auto* v = std::get_if<bool>(&cell.typed_value))
    return *v ? (cell.value == "true" || cell.value == "True" || cell.value == "1")
              : (cell.value == "false" || cell.value == "False" || cell.value == "0");
  if (const auto* v = std::get_if<double>(&cell.typed_value)) {
    try { std::size_t used{}; const auto parsed = std::stod(cell.value, &used);
      return used == cell.value.size() && parsed == *v;
    } catch (...) { return false; }
  }
  if (const auto* v = std::get_if<NativeDate>(&cell.typed_value)) return cell.value == v->value;
  return true;
}

inline EventAttribute dtype_tuple(const std::string& key,
                                  std::vector<EventAttribute> children) {
  EventAttribute result{key, "", AttributeKind::kString};
  result.container = AttributeContainerKind::kTuple;
  result.children = std::move(children);
  return result;
}
inline bool dtype_attribute_equal(const EventAttribute& actual, const EventAttribute& expected) {
  if (actual.is_null != expected.is_null) return false;
  if (actual.is_null) return true;
  if (!dtype_projection_valid(actual)) return false;
  if (actual.kind != expected.kind || actual.container != expected.container ||
      actual.children.size() != expected.children.size())
    return false;
  bool scalar_equal = actual.typed_value == expected.typed_value;
  if (const auto* a = std::get_if<std::uint64_t>(&actual.typed_value)) {
    if (const auto* e = std::get_if<std::int64_t>(&expected.typed_value))
      scalar_equal = *e >= 0 && *a == static_cast<std::uint64_t>(*e);
  }
  if (const auto* a = std::get_if<std::int64_t>(&actual.typed_value)) {
    if (const auto* e = std::get_if<std::uint64_t>(&expected.typed_value))
      scalar_equal = *a >= 0 && static_cast<std::uint64_t>(*a) == *e;
  }
  if (!scalar_equal) return false;
  if (std::holds_alternative<std::monostate>(expected.typed_value) && actual.value != expected.value)
    return false;
  for (std::size_t i = 0; i < expected.children.size(); ++i)
    if (!dtype_attribute_equal(actual.children[i], expected.children[i])) return false;
  return true;
}
inline void check_dtype_table(const std::string& name, const EventTable& actual,
                              const EventTable& expected) {
  actual.validate();
  if (actual.columns != expected.columns || actual.rows.size() != expected.rows.size())
    throw std::runtime_error(name + ": output shape/columns differ");
  if (actual.dtypes != expected.dtypes) {
    std::string detail;
    for (std::size_t i = 0; i < expected.columns.size(); ++i)
      detail += " " + expected.columns[i] + "=" +
          (i < actual.dtypes.size() ? actual.dtypes[i] : "<absent>") +
          " (expected " + expected.dtypes[i] + ")";
    throw std::runtime_error(name + ": column dtypes differ:" + detail);
  }
  for (std::size_t column = 0; column < expected.columns.size(); ++column) {
    const auto actual_category = actual.categorical_dtypes.empty()
        ? std::optional<EventTableCategoricalDtype>{} : actual.categorical_dtypes[column];
    const auto expected_category = expected.categorical_dtypes.empty()
        ? std::optional<EventTableCategoricalDtype>{} : expected.categorical_dtypes[column];
    if (actual_category != expected_category)
      throw std::runtime_error(name + ": categorical metadata differs for " + expected.columns[column]);
    for (std::size_t row = 0; row < expected.rows.size(); ++row) {
      const auto& observed = actual.rows[row][column];
      const auto& reference = expected.rows[row][column];
      if (observed && observed->key != actual.columns[column])
        throw std::runtime_error(name + ": output cell key differs");
      if (reference_missing(reference) != reference_missing(observed) ||
          (!reference_missing(reference) && !dtype_attribute_equal(*observed, *reference)))
        throw std::runtime_error(name + ": scalar value/kind differs for " + expected.columns[column]);
    }
  }
}

}

#include "minera/dataframe_format.hpp"
#include <algorithm>
namespace replay_dataframe_reference {
using namespace case_relations_dtype_reference;
inline EventAttribute format_container(const std::string& key,
    AttributeContainerKind kind, std::vector<EventAttribute> children) {
  EventAttribute result{key, "", AttributeKind::kString};
  result.container = kind;
  result.children = std::move(children);
  return result;
}
inline EventTableIndex format_index(const EventTable& table) {
  if (table.index) return *table.index;
  EventTableIndex result;
  result.kind = "RangeIndex";
  for (std::size_t i = 0; i < table.rows.size(); ++i)
    result.labels.push_back({reference_number("", static_cast<std::int64_t>(i))});
  return result;
}
inline void check_format_table(const std::string& name, const EventTable& actual,
                               const EventTable& expected) {
  if (actual.columns != expected.columns || actual.rows.size() != expected.rows.size() ||
      actual.dtypes != expected.dtypes)
    throw std::runtime_error(name + ": columns, rows or dtypes differ");
  if (actual.categorical_dtypes != expected.categorical_dtypes) {
    const auto has_category = [](const auto& values) {
      return std::any_of(values.begin(), values.end(), [](const auto& value) { return value.has_value(); });
    };
    if (has_category(actual.categorical_dtypes) || has_category(expected.categorical_dtypes))
      throw std::runtime_error(name + ": categorical metadata differs");
  }
  for (std::size_t row = 0; row < expected.rows.size(); ++row) {
    if (actual.rows[row].size() != expected.rows[row].size())
      throw std::runtime_error(name + ": row width differs");
    for (std::size_t column = 0; column < expected.columns.size(); ++column) {
      const auto& observed = actual.rows[row][column];
      const auto& reference = expected.rows[row][column];
      if (observed && observed->key != expected.columns[column])
        throw std::runtime_error(name + ": cell key differs");
      if (reference_missing(observed) != reference_missing(reference) ||
          (!reference_missing(reference) && !dtype_attribute_equal(*observed, *reference)))
        throw std::runtime_error(name + ": cell differs at row " + std::to_string(row) + " column " + expected.columns[column]);
    }
  }
  if (format_index(actual) != format_index(expected))
    throw std::runtime_error(name + ": index differs");
  if (actual.metadata.properties != expected.metadata.properties)
    throw std::runtime_error(name + ": dataframe attrs differ");
}
inline void check_replay_table(const std::string& name, const EventTable& actual, const EventTable& expected) {
  check_format_table(name, actual, expected);
  for (std::size_t i=0; i<expected.rows.size(); ++i) for (std::size_t j=0; j<expected.columns.size(); ++j) {
    if (!expected.rows[i][j]) continue;
    const auto* date=std::get_if<NativeDate>(&expected.rows[i][j]->typed_value);
    if (!date) continue;
    const auto* observed=actual.rows[i][j] ? std::get_if<NativeDate>(&actual.rows[i][j]->typed_value) : nullptr;
    if (!observed || observed->resolution != date->resolution) throw std::runtime_error(name + ": date precision differs");
  }
}
inline void check_replay_dataframe_0() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-02T00:00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-01T00:00:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-02T00:00:00"), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-02T00:00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-01T00:00:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-02T00:00:00"), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{reference_date("case_id", "2024-01-02T00:00:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})},
{reference_date("case_id", "2024-01-01T00:00:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"datetime64[us]", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("date_naive", replay_dataframe_diagnostics(input,net,options), expected);
  const auto converted = event_table_to_event_log(input);
  if (converted.traces.size() != 2) throw std::runtime_error("date_naive: conversion group count differs");
  { std::vector<std::string> activities; for (const auto& event : converted.traces[0].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"A","C"}) throw std::runtime_error("date_naive: converted membership differs"); }
  { std::vector<std::string> activities; for (const auto& event : converted.traces[1].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"B"}) throw std::runtime_error("date_naive: converted membership differs"); }
  check_replay_table("date_naive: input after", input, expected_input);
}
inline void check_replay_dataframe_1() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-02T00:00:00+00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-01T00:00:00+00:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-02T00:00:00+00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-01T00:00:00+00:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{reference_date("case_id", "2024-01-02T00:00:00+00:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{reference_date("case_id", "2024-01-01T00:00:00+00:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"datetime64[us, UTC]", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("date_utc", replay_dataframe_diagnostics(input,net,options), expected);
  const auto converted = event_table_to_event_log(input);
  if (converted.traces.size() != 2) throw std::runtime_error("date_utc: conversion group count differs");
  { std::vector<std::string> activities; for (const auto& event : converted.traces[0].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"A"}) throw std::runtime_error("date_utc: converted membership differs"); }
  { std::vector<std::string> activities; for (const auto& event : converted.traces[1].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"B"}) throw std::runtime_error("date_utc: converted membership differs"); }
  check_replay_table("date_utc: input after", input, expected_input);
}
inline void check_replay_dataframe_2() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T00:00:00+00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-01T01:00:00+01:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T00:00:00+00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-01T01:00:00+01:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{reference_date("case_id", "2024-01-01T00:00:00+00:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})}}};
  expected.dtypes = {"datetime64[us, UTC]", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {};
  check_replay_table("date_same_instant_offsets", replay_dataframe_diagnostics(input,net,options), expected);
  const auto converted = event_table_to_event_log(input);
  if (converted.traces.size() != 1) throw std::runtime_error("date_same_instant_offsets: conversion group count differs");
  { std::vector<std::string> activities; for (const auto& event : converted.traces[0].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"A","B"}) throw std::runtime_error("date_same_instant_offsets: converted membership differs"); }
  check_replay_table("date_same_instant_offsets: input after", input, expected_input);
}
inline void check_replay_dataframe_3() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T00:00:00+00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-02T01:00:00+01:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T00:00:00+00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-02T01:00:00+01:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{reference_date("case_id", "2024-01-01T00:00:00+00:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{reference_date("case_id", "2024-01-02T01:00:00+01:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"object", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("date_mixed_offsets", replay_dataframe_diagnostics(input,net,options), expected);
  const auto converted = event_table_to_event_log(input);
  if (converted.traces.size() != 2) throw std::runtime_error("date_mixed_offsets: conversion group count differs");
  { std::vector<std::string> activities; for (const auto& event : converted.traces[0].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"A"}) throw std::runtime_error("date_mixed_offsets: converted membership differs"); }
  { std::vector<std::string> activities; for (const auto& event : converted.traces[1].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"B"}) throw std::runtime_error("date_mixed_offsets: converted membership differs"); }
  check_replay_table("date_mixed_offsets: input after", input, expected_input);
}
inline void check_replay_dataframe_4() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T00:00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-01T00:00:00+00:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T00:00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-01T00:00:00+00:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{reference_date("case_id", "2024-01-01T00:00:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{reference_date("case_id", "2024-01-01T00:00:00+00:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"object", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("date_mixed_naive_aware", replay_dataframe_diagnostics(input,net,options), expected);
  const auto converted = event_table_to_event_log(input);
  if (converted.traces.size() != 2) throw std::runtime_error("date_mixed_naive_aware: conversion group count differs");
  { std::vector<std::string> activities; for (const auto& event : converted.traces[0].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"A"}) throw std::runtime_error("date_mixed_naive_aware: converted membership differs"); }
  { std::vector<std::string> activities; for (const auto& event : converted.traces[1].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"B"}) throw std::runtime_error("date_mixed_naive_aware: converted membership differs"); }
  check_replay_table("date_mixed_naive_aware: input after", input, expected_input);
}
inline void check_replay_dataframe_5() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T00:00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "2024-01-01", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T00:00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "2024-01-01", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{reference_date("case_id", "2024-01-01T00:00:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{EventAttribute{"case_id", "2024-01-01", AttributeKind::kString}, dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"object", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("date_mixed_string", replay_dataframe_diagnostics(input,net,options), expected);
  const auto converted = event_table_to_event_log(input);
  if (converted.traces.size() != 2) throw std::runtime_error("date_mixed_string: conversion group count differs");
  { std::vector<std::string> activities; for (const auto& event : converted.traces[0].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"A"}) throw std::runtime_error("date_mixed_string: converted membership differs"); }
  { std::vector<std::string> activities; for (const auto& event : converted.traces[1].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"B"}) throw std::runtime_error("date_mixed_string: converted membership differs"); }
  check_replay_table("date_mixed_string: input after", input, expected_input);
}
inline void check_replay_dataframe_6() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T00:00:00.000000001"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-01T00:00:00.000000002"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  std::get<NativeDate>(input.rows[0][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  std::get<NativeDate>(input.rows[1][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T00:00:00.000000001"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-01T00:00:00.000000002"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  std::get<NativeDate>(expected_input.rows[0][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  std::get<NativeDate>(expected_input.rows[1][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{reference_date("case_id", "2024-01-01T00:00:00.000000001"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{reference_date("case_id", "2024-01-01T00:00:00.000000002"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"datetime64[ns]", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  std::get<NativeDate>(expected.rows[0][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  std::get<NativeDate>(expected.rows[1][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  check_replay_table("date_nanoseconds", replay_dataframe_diagnostics(input,net,options), expected);
  const auto converted = event_table_to_event_log(input);
  if (converted.traces.size() != 2) throw std::runtime_error("date_nanoseconds: conversion group count differs");
  { std::vector<std::string> activities; for (const auto& event : converted.traces[0].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"A"}) throw std::runtime_error("date_nanoseconds: converted membership differs"); }
  { std::vector<std::string> activities; for (const auto& event : converted.traces[1].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"B"}) throw std::runtime_error("date_nanoseconds: converted membership differs"); }
  check_replay_table("date_nanoseconds: input after", input, expected_input);
}
inline void check_replay_dataframe_7() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T00:00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-02T00:00:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  std::get<NativeDate>(input.rows[1][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T00:00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-02T00:00:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  std::get<NativeDate>(expected_input.rows[1][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{reference_date("case_id", "2024-01-01T00:00:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{reference_date("case_id", "2024-01-02T00:00:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"datetime64[ns]", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  std::get<NativeDate>(expected.rows[0][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  std::get<NativeDate>(expected.rows[1][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  check_replay_table("date_mixed_precision", replay_dataframe_diagnostics(input,net,options), expected);
  const auto converted = event_table_to_event_log(input);
  if (converted.traces.size() != 2) throw std::runtime_error("date_mixed_precision: conversion group count differs");
  { std::vector<std::string> activities; for (const auto& event : converted.traces[0].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"A"}) throw std::runtime_error("date_mixed_precision: converted membership differs"); }
  { std::vector<std::string> activities; for (const auto& event : converted.traces[1].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"B"}) throw std::runtime_error("date_mixed_precision: converted membership differs"); }
  check_replay_table("date_mixed_precision: input after", input, expected_input);
}
inline void check_replay_dataframe_8() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "1969-12-31T23:59:59.999999999"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "1970-01-01T00:00:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  std::get<NativeDate>(input.rows[0][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "1969-12-31T23:59:59.999999999"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "1970-01-01T00:00:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  std::get<NativeDate>(expected_input.rows[0][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{reference_date("case_id", "1969-12-31T23:59:59.999999999"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{reference_date("case_id", "1970-01-01T00:00:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"datetime64[ns]", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  std::get<NativeDate>(expected.rows[0][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  std::get<NativeDate>(expected.rows[1][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  check_replay_table("date_pre_epoch", replay_dataframe_diagnostics(input,net,options), expected);
  const auto converted = event_table_to_event_log(input);
  if (converted.traces.size() != 2) throw std::runtime_error("date_pre_epoch: conversion group count differs");
  { std::vector<std::string> activities; for (const auto& event : converted.traces[0].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"A"}) throw std::runtime_error("date_pre_epoch: converted membership differs"); }
  { std::vector<std::string> activities; for (const auto& event : converted.traces[1].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"B"}) throw std::runtime_error("date_pre_epoch: converted membership differs"); }
  check_replay_table("date_pre_epoch: input after", input, expected_input);
}
inline void check_replay_dataframe_9() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T00:00:00.000000001+00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-01T01:00:00.000000001+01:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  std::get<NativeDate>(input.rows[0][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  std::get<NativeDate>(input.rows[1][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T00:00:00.000000001+00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-01T01:00:00.000000001+01:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  std::get<NativeDate>(expected_input.rows[0][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  std::get<NativeDate>(expected_input.rows[1][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{reference_date("case_id", "2024-01-01T00:00:00.000000001+00:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})}}};
  expected.dtypes = {"datetime64[ns, UTC]", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {};
  std::get<NativeDate>(expected.rows[0][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  check_replay_table("date_offset_nanoseconds_equal", replay_dataframe_diagnostics(input,net,options), expected);
  const auto converted = event_table_to_event_log(input);
  if (converted.traces.size() != 1) throw std::runtime_error("date_offset_nanoseconds_equal: conversion group count differs");
  { std::vector<std::string> activities; for (const auto& event : converted.traces[0].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"A","B"}) throw std::runtime_error("date_offset_nanoseconds_equal: converted membership differs"); }
  check_replay_table("date_offset_nanoseconds_equal: input after", input, expected_input);
}
inline void check_replay_dataframe_10() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T00:00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-01T00:00:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  std::get<NativeDate>(input.rows[1][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T00:00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-01T00:00:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  std::get<NativeDate>(expected_input.rows[1][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{reference_date("case_id", "2024-01-01T00:00:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})}}};
  expected.dtypes = {"datetime64[us]", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {};
  check_replay_table("date_equal_mixed_precision", replay_dataframe_diagnostics(input,net,options), expected);
  const auto converted = event_table_to_event_log(input);
  if (converted.traces.size() != 1) throw std::runtime_error("date_equal_mixed_precision: conversion group count differs");
  { std::vector<std::string> activities; for (const auto& event : converted.traces[0].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"A","B"}) throw std::runtime_error("date_equal_mixed_precision: converted membership differs"); }
  check_replay_table("date_equal_mixed_precision: input after", input, expected_input);
}
inline void check_replay_dataframe_11() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T00:00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-02T00:00:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  std::get<NativeDate>(input.rows[0][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  std::get<NativeDate>(input.rows[1][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T00:00:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-02T00:00:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  std::get<NativeDate>(expected_input.rows[0][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  std::get<NativeDate>(expected_input.rows[1][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{reference_date("case_id", "2024-01-01T00:00:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{reference_date("case_id", "2024-01-02T00:00:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"datetime64[ns]", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  std::get<NativeDate>(expected.rows[0][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  std::get<NativeDate>(expected.rows[1][0]->typed_value).resolution = NativeDateResolution::kNanosecond;
  check_replay_table("date_ns_whole", replay_dataframe_diagnostics(input,net,options), expected);
  const auto converted = event_table_to_event_log(input);
  if (converted.traces.size() != 2) throw std::runtime_error("date_ns_whole: conversion group count differs");
  { std::vector<std::string> activities; for (const auto& event : converted.traces[0].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"A"}) throw std::runtime_error("date_ns_whole: converted membership differs"); }
  { std::vector<std::string> activities; for (const auto& event : converted.traces[1].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"B"}) throw std::runtime_error("date_ns_whole: converted membership differs"); }
  check_replay_table("date_ns_whole: input after", input, expected_input);
}
inline void check_replay_dataframe_12() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T01:00:00+01:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-02T01:00:00+01:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_date("case:concept:name", "2024-01-01T01:00:00+01:00"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-02T01:00:00+01:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{reference_date("case_id", "2024-01-01T01:00:00+01:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{reference_date("case_id", "2024-01-02T01:00:00+01:00"), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"datetime64[us, UTC+01:00]", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("date_fixed_offset", replay_dataframe_diagnostics(input,net,options), expected);
  const auto converted = event_table_to_event_log(input);
  if (converted.traces.size() != 2) throw std::runtime_error("date_fixed_offset: conversion group count differs");
  { std::vector<std::string> activities; for (const auto& event : converted.traces[0].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"A"}) throw std::runtime_error("date_fixed_offset: converted membership differs"); }
  { std::vector<std::string> activities; for (const auto& event : converted.traces[1].events) activities.push_back(event.activity); if (activities != std::vector<std::string>{"B"}) throw std::runtime_error("date_fixed_offset: converted membership differs"); }
  check_replay_table("date_fixed_offset: input after", input, expected_input);
}
inline void check_replay_dataframe_13() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_null("case:concept:name"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-01T00:00:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_null("case:concept:name"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_date("case:concept:name", "2024-01-01T00:00:00"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  std::string error; try { (void)replay_dataframe_diagnostics(input,net,options); } catch (const std::exception& caught) { error=caught.what(); }
  if (error != "the case ID column should not contain any empty value.") throw std::runtime_error("date_nat: rejection differs");
  check_replay_table("date_nat: input after", input, expected_input);
}
inline void check_replay_dataframe_14() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
}};
  input.dtypes = {"object", "float64", "datetime64[s, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
}};
  expected_input.dtypes = {"object", "float64", "datetime64[s, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  std::string error; try { (void)replay_dataframe_diagnostics(input,net,options); } catch (const std::exception& caught) { error=caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity.") throw std::runtime_error("empty: rejection differs");
  check_replay_table("empty: input after", input, expected_input);
}
inline void check_replay_dataframe_15() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
}};
  input.dtypes = {"object", "str", "datetime64[s, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
}};
  expected_input.dtypes = {"object", "str", "datetime64[s, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{}, {
}};
  expected.dtypes = {};
  expected.categorical_dtypes = {};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected.metadata.properties = {};
  check_replay_table("typed_empty", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("typed_empty: input after", input, expected_input);
}
inline void check_replay_dataframe_16() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "z", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "a", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "z", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "b", AttributeKind::kString}, EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {EventAttribute{"example", "not propagated", AttributeKind::kString}};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "z", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "a", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "z", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "b", AttributeKind::kString}, EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {EventAttribute{"example", "not propagated", AttributeKind::kString}};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{EventAttribute{"case_id", "z", AttributeKind::kString}, dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})},
{EventAttribute{"case_id", "a", AttributeKind::kString}, dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{EventAttribute{"case_id", "b", AttributeKind::kString}, dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"str", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {};
  check_replay_table("strings_source_order", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("strings_source_order: input after", input, expected_input);
}
inline void check_replay_dataframe_17() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", false), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", false), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", true), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})},
{dtype_number("case_id", false), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"bool", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("booleans", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("booleans: input after", input, expected_input);
}
inline void check_replay_dataframe_18() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "x", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "x", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{EventAttribute{"case_id", "", AttributeKind::kString}, dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})},
{EventAttribute{"case_id", "x", AttributeKind::kString}, dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"str", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("empty_strings", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("empty_strings: input after", input, expected_input);
}
inline void check_replay_dataframe_19() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", double{9.223372036854776e+18}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", double{9.223372036854776e+18}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"float64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("upper", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("upper: input after", input, expected_input);
}
inline void check_replay_dataframe_20() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", double{9.223372036854776e+18}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", double{9.223372036854776e+18}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"float64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("upper_reverse", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("upper_reverse: input after", input, expected_input);
}
inline void check_replay_dataframe_21() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", std::int64_t{9007199254740993}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", std::int64_t{9007199254740992}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})}}};
  expected.dtypes = {"int64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("wide", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("wide: input after", input, expected_input);
}
inline void check_replay_dataframe_22() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", double{9007199254740992.0}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})},
{dtype_number("case_id", double{9007199254740992.0}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"float64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("wide_float_first", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("wide_float_first: input after", input, expected_input);
}
inline void check_replay_dataframe_23() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", std::int64_t{9007199254740993}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", std::int64_t{9007199254740992}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"int64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("wide_only_int", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("wide_only_int: input after", input, expected_input);
}
inline void check_replay_dataframe_24() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", double{9007199254740992.0}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", double{9007199254740992.0}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", double{1.5}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"float64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {};
  check_replay_table("wide_float_unrelated", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("wide_float_unrelated: input after", input, expected_input);
}
inline void check_replay_dataframe_25() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{-9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{-9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{-1.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{-9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{-9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{-1.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", double{-9007199254740992.0}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", double{-9007199254740992.0}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", double{-1.5}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"float64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {};
  check_replay_table("negative", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("negative: input after", input, expected_input);
}
inline void check_replay_dataframe_26() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_number("case:concept:name", std::uint64_t{18446744073709551615ULL}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551614ULL}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_number("case:concept:name", std::uint64_t{18446744073709551615ULL}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551614ULL}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{reference_number("case_id", std::uint64_t{18446744073709551615ULL}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{reference_number("case_id", std::uint64_t{18446744073709551614ULL}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"uint64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("uint_only", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("uint_only: input after", input, expected_input);
}
inline void check_replay_dataframe_27() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_number("case:concept:name", std::uint64_t{18446744073709551615ULL}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551614ULL}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_number("case:concept:name", std::uint64_t{18446744073709551615ULL}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551614ULL}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", double{1.8446744073709552e+19}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", double{1.8446744073709552e+19}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", double{0.5}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"float64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {};
  check_replay_table("uint_float", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("uint_float: input after", input, expected_input);
}
inline void check_replay_dataframe_28() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", std::int64_t{9007199254740993}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", std::int64_t{9007199254740992}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", double{0.5}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", true), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"object", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected.metadata.properties = {};
  check_replay_table("bool_suppresses", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("bool_suppresses: input after", input, expected_input);
}
inline void check_replay_dataframe_29() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "x", AttributeKind::kString}, EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "x", AttributeKind::kString}, EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", std::int64_t{9007199254740993}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", std::int64_t{9007199254740992}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", double{0.5}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{EventAttribute{"case_id", "x", AttributeKind::kString}, dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"object", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected.metadata.properties = {};
  check_replay_table("string_suppresses", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("string_suppresses: input after", input, expected_input);
}
inline void check_replay_dataframe_30() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", double{9.223372036854776e+18}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})},
{dtype_number("case_id", double{9.223372036854776e+18}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})}}};
  expected.dtypes = {"float64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("upper_repeat", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("upper_repeat: input after", input, expected_input);
}
inline void check_replay_dataframe_31() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{-1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551615ULL}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551614ULL}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{-1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551615ULL}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551614ULL}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", std::int64_t{-1}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{reference_number("case_id", std::uint64_t{18446744073709551615ULL}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{reference_number("case_id", std::uint64_t{18446744073709551614ULL}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"object", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {};
  check_replay_table("signed_unsigned", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("signed_unsigned: input after", input, expected_input);
}
inline void check_replay_dataframe_32() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "E", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "E", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", true), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})},
{dtype_number("case_id", std::int64_t{9007199254740993}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", std::int64_t{9007199254740992}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", double{0.5}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"object", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected.metadata.properties = {};
  check_replay_table("bool_first", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("bool_first: input after", input, expected_input);
}
inline void check_replay_dataframe_33() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "E", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "E", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", double{1.0}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})},
{dtype_number("case_id", double{9007199254740992.0}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", double{9007199254740992.0}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", double{0.5}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"float64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected.metadata.properties = {};
  check_replay_table("float_before_bool", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("float_before_bool: input after", input, expected_input);
}
inline void check_replay_dataframe_34() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", std::int64_t{1}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})},
{dtype_number("case_id", std::int64_t{9007199254740993}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", std::int64_t{9007199254740992}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"int64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {};
  check_replay_table("int_before_float", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("int_before_float: input after", input, expected_input);
}
inline void check_replay_dataframe_35() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_null("case:concept:name"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_null("case:concept:name"), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_null("case:concept:name"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_null("case:concept:name"), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  std::string error; try { (void)replay_dataframe_diagnostics(input,net,options); } catch (const std::exception& caught) { error=caught.what(); }
  if (error != "the case ID column should not contain any empty value.") throw std::runtime_error("nan_keys: rejection differs");
  check_replay_table("nan_keys: input after", input, expected_input);
}
inline void check_replay_dataframe_36() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_null("case:concept:name"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_null("case:concept:name"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_null("case:concept:name"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_null("case:concept:name"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  std::string error; try { (void)replay_dataframe_diagnostics(input,net,options); } catch (const std::exception& caught) { error=caught.what(); }
  if (error != "the case ID column should not contain any empty value.") throw std::runtime_error("none_keys: rejection differs");
  check_replay_table("none_keys: input after", input, expected_input);
}
inline void check_replay_dataframe_37() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::numeric_limits<double>::infinity()), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", -std::numeric_limits<double>::infinity()), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551615ULL}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551614ULL}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::numeric_limits<double>::infinity()), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", -std::numeric_limits<double>::infinity()), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551615ULL}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551614ULL}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", std::numeric_limits<double>::infinity()), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", -std::numeric_limits<double>::infinity()), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", double{1.8446744073709552e+19}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", double{1.8446744073709552e+19}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"float64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected.metadata.properties = {};
  check_replay_table("infinity", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("infinity: input after", input, expected_input);
}
inline void check_replay_dataframe_38() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", double{9007199254740992.0}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})},
{dtype_number("case_id", double{9007199254740992.0}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})}}};
  expected.dtypes = {"float64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("float_first_wide_twice", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("float_first_wide_twice: input after", input, expected_input);
}
inline void check_replay_dataframe_39() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_null("case:concept:name"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_null("case:concept:name"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_null("case:concept:name"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_null("case:concept:name"), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  std::string error; try { (void)replay_dataframe_diagnostics(input,net,options); } catch (const std::exception& caught) { error=caught.what(); }
  if (error != "the case ID column should not contain any empty value.") throw std::runtime_error("nan_only: rejection differs");
  check_replay_table("nan_only: input after", input, expected_input);
}
inline void check_replay_dataframe_40() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", std::int64_t{1}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})}}};
  expected.dtypes = {"int64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {};
  check_replay_table("prior_equal", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("prior_equal: input after", input, expected_input);
}
inline void check_replay_dataframe_41() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", std::int64_t{9007199254740992}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})}}};
  expected.dtypes = {"int64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {};
  check_replay_table("prior_wide_equal", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("prior_wide_equal: input after", input, expected_input);
}
inline void check_replay_dataframe_42() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", std::int64_t{9007199254740993}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", std::int64_t{9007199254740992}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"int64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("prior_wide_distinct", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("prior_wide_distinct: input after", input, expected_input);
}
inline void check_replay_dataframe_43() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", std::int64_t{9007199254740993}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", std::int64_t{9007199254740992}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})}}};
  expected.dtypes = {"int64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("prior_wide_mixed", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("prior_wide_mixed: input after", input, expected_input);
}
inline void check_replay_dataframe_44() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", double{9.223372036854776e+18}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", double{9.223372036854776e+18}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"float64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("prior_upper", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("prior_upper: input after", input, expected_input);
}
inline void check_replay_dataframe_45() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{-1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{-1.5}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{-1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{-1.5}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", double{-1.0}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})},
{dtype_number("case_id", double{-1.5}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"float64", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("prior_small_fraction", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("prior_small_fraction: input after", input, expected_input);
}
inline void check_replay_dataframe_46() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A","B","C","D","E","F","G","H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{dtype_number("case_id", std::int64_t{1}), dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})},
{EventAttribute{"case_id", "1", AttributeKind::kString}, dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"object", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("prior_string_mixed", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("prior_string_mixed: input after", input, expected_input);
}
inline void check_replay_dataframe_47() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(), q=net.add_place(), r=net.add_place(); net.set_source(p); net.set_sink(r); net.set_activities({"A","B"}); net.add_transition(0,{p},{q}); net.add_transition(1,{q},{r});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{EventAttribute{"case_id", "c", AttributeKind::kString}, dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})}}};
  expected.dtypes = {"str", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {};
  check_replay_table("linear_fit", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("linear_fit: input after", input, expected_input);
}
inline void check_replay_dataframe_48() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(), q=net.add_place(), r=net.add_place(); net.set_source(p); net.set_sink(r); net.set_activities({"A","B"}); net.add_transition(0,{p},{q}); net.add_transition(1,{q},{r});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{EventAttribute{"case_id", "c", AttributeKind::kString}, dtype_number("is_fit", false), dtype_number("trace_fitness", double{0.5}), dtype_number("missing", std::int64_t{1}), dtype_number("remaining", std::int64_t{1}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"str", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {};
  check_replay_table("linear_missing", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("linear_missing: input after", input, expected_input);
}
inline void check_replay_dataframe_49() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(), q=net.add_place(), r=net.add_place(); net.set_source(p); net.set_sink(r); net.set_activities({"A","B"}); net.add_transition(0,{p},{q}); net.add_transition(1,{q},{r});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{EventAttribute{"case_id", "c", AttributeKind::kString}, dtype_number("is_fit", false), dtype_number("trace_fitness", double{0.5}), dtype_number("missing", std::int64_t{1}), dtype_number("remaining", std::int64_t{1}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"str", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {};
  check_replay_table("linear_remaining", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("linear_remaining: input after", input, expected_input);
}
inline void check_replay_dataframe_50() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(), q=net.add_place(), r=net.add_place(); net.set_source(p); net.set_sink(r); net.set_activities({"A","B"}); net.add_transition(0,{p},{q}); net.add_transition(1,{q},{r});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{EventAttribute{"case_id", "c", AttributeKind::kString}, dtype_number("is_fit", false), dtype_number("trace_fitness", double{0.6666666666666667}), dtype_number("missing", std::int64_t{1}), dtype_number("remaining", std::int64_t{1}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})}}};
  expected.dtypes = {"str", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {};
  check_replay_table("linear_reverse", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("linear_reverse: input after", input, expected_input);
}
inline void check_replay_dataframe_51() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "X", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "X", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(), q=net.add_place(), r=net.add_place(); net.set_source(p); net.set_sink(r); net.set_activities({"A","B"}); net.add_transition(0,{p},{q}); net.add_transition(1,{q},{r});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{EventAttribute{"case_id", "c", AttributeKind::kString}, dtype_number("is_fit", false), dtype_number("trace_fitness", double{0.0}), dtype_number("missing", std::int64_t{1}), dtype_number("remaining", std::int64_t{1}), dtype_number("produced", std::int64_t{1}), dtype_number("consumed", std::int64_t{1})}}};
  expected.dtypes = {"str", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {};
  check_replay_table("linear_unknown", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("linear_unknown: input after", input, expected_input);
}
inline void check_replay_dataframe_52() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "X", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "X", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "c", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(), q=net.add_place(), r=net.add_place(); net.set_source(p); net.set_sink(r); net.set_activities({"A","B"}); net.add_transition(0,{p},{q}); net.add_transition(1,{q},{r});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{EventAttribute{"case_id", "c", AttributeKind::kString}, dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})}}};
  expected.dtypes = {"str", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {};
  check_replay_table("linear_unknown_between", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("linear_unknown_between: input after", input, expected_input);
}
inline void check_replay_dataframe_53() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "z", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "a", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "z", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "z", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "a", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "z", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(), q=net.add_place(), r=net.add_place(); net.set_source(p); net.set_sink(r); net.set_activities({"A","B"}); net.add_transition(0,{p},{q}); net.add_transition(1,{q},{r});
  TokenReplayOptions options;
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{EventAttribute{"case_id", "z", AttributeKind::kString}, dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})},
{EventAttribute{"case_id", "a", AttributeKind::kString}, dtype_number("is_fit", false), dtype_number("trace_fitness", double{0.5}), dtype_number("missing", std::int64_t{1}), dtype_number("remaining", std::int64_t{1}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"str", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("linear_source_order", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("linear_source_order: input after", input, expected_input);
}
inline void check_replay_dataframe_54() {
  EventTable input{{"ticket", "action", "completed"}, {
{EventAttribute{"ticket", "z", AttributeKind::kString}, EventAttribute{"action", "A", AttributeKind::kString}, reference_date("completed", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"ticket", "a", AttributeKind::kString}, EventAttribute{"action", "B", AttributeKind::kString}, reference_date("completed", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"ticket", "z", AttributeKind::kString}, EventAttribute{"action", "B", AttributeKind::kString}, reference_date("completed", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"ticket", "action", "completed"}, {
{EventAttribute{"ticket", "z", AttributeKind::kString}, EventAttribute{"action", "A", AttributeKind::kString}, reference_date("completed", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"ticket", "a", AttributeKind::kString}, EventAttribute{"action", "B", AttributeKind::kString}, reference_date("completed", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"ticket", "z", AttributeKind::kString}, EventAttribute{"action", "B", AttributeKind::kString}, reference_date("completed", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto p=net.add_place(), q=net.add_place(), r=net.add_place(); net.set_source(p); net.set_sink(r); net.set_activities({"A","B"}); net.add_transition(0,{p},{q}); net.add_transition(1,{q},{r});
  TokenReplayOptions options;
  options.case_id_attribute = "ticket";
  options.activity_attribute = "action";
  options.timestamp_attribute = "completed";
  EventTable expected{{"case_id", "is_fit", "trace_fitness", "missing", "remaining", "produced", "consumed"}, {
{EventAttribute{"case_id", "z", AttributeKind::kString}, dtype_number("is_fit", true), dtype_number("trace_fitness", double{1.0}), dtype_number("missing", std::int64_t{0}), dtype_number("remaining", std::int64_t{0}), dtype_number("produced", std::int64_t{3}), dtype_number("consumed", std::int64_t{3})},
{EventAttribute{"case_id", "a", AttributeKind::kString}, dtype_number("is_fit", false), dtype_number("trace_fitness", double{0.5}), dtype_number("missing", std::int64_t{1}), dtype_number("remaining", std::int64_t{1}), dtype_number("produced", std::int64_t{2}), dtype_number("consumed", std::int64_t{2})}}};
  expected.dtypes = {"str", "bool", "float64", "int64", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {};
  check_replay_table("custom_selectors", replay_dataframe_diagnostics(input,net,options), expected);
  check_replay_table("custom_selectors: input after", input, expected_input);
}
}
inline void check_public_replay_dataframe_diagnostics() {
  replay_dataframe_reference::check_replay_dataframe_0();
  replay_dataframe_reference::check_replay_dataframe_1();
  replay_dataframe_reference::check_replay_dataframe_2();
  replay_dataframe_reference::check_replay_dataframe_3();
  replay_dataframe_reference::check_replay_dataframe_4();
  replay_dataframe_reference::check_replay_dataframe_5();
  replay_dataframe_reference::check_replay_dataframe_6();
  replay_dataframe_reference::check_replay_dataframe_7();
  replay_dataframe_reference::check_replay_dataframe_8();
  replay_dataframe_reference::check_replay_dataframe_9();
  replay_dataframe_reference::check_replay_dataframe_10();
  replay_dataframe_reference::check_replay_dataframe_11();
  replay_dataframe_reference::check_replay_dataframe_12();
  replay_dataframe_reference::check_replay_dataframe_13();
  replay_dataframe_reference::check_replay_dataframe_14();
  replay_dataframe_reference::check_replay_dataframe_15();
  replay_dataframe_reference::check_replay_dataframe_16();
  replay_dataframe_reference::check_replay_dataframe_17();
  replay_dataframe_reference::check_replay_dataframe_18();
  replay_dataframe_reference::check_replay_dataframe_19();
  replay_dataframe_reference::check_replay_dataframe_20();
  replay_dataframe_reference::check_replay_dataframe_21();
  replay_dataframe_reference::check_replay_dataframe_22();
  replay_dataframe_reference::check_replay_dataframe_23();
  replay_dataframe_reference::check_replay_dataframe_24();
  replay_dataframe_reference::check_replay_dataframe_25();
  replay_dataframe_reference::check_replay_dataframe_26();
  replay_dataframe_reference::check_replay_dataframe_27();
  replay_dataframe_reference::check_replay_dataframe_28();
  replay_dataframe_reference::check_replay_dataframe_29();
  replay_dataframe_reference::check_replay_dataframe_30();
  replay_dataframe_reference::check_replay_dataframe_31();
  replay_dataframe_reference::check_replay_dataframe_32();
  replay_dataframe_reference::check_replay_dataframe_33();
  replay_dataframe_reference::check_replay_dataframe_34();
  replay_dataframe_reference::check_replay_dataframe_35();
  replay_dataframe_reference::check_replay_dataframe_36();
  replay_dataframe_reference::check_replay_dataframe_37();
  replay_dataframe_reference::check_replay_dataframe_38();
  replay_dataframe_reference::check_replay_dataframe_39();
  replay_dataframe_reference::check_replay_dataframe_40();
  replay_dataframe_reference::check_replay_dataframe_41();
  replay_dataframe_reference::check_replay_dataframe_42();
  replay_dataframe_reference::check_replay_dataframe_43();
  replay_dataframe_reference::check_replay_dataframe_44();
  replay_dataframe_reference::check_replay_dataframe_45();
  replay_dataframe_reference::check_replay_dataframe_46();
  replay_dataframe_reference::check_replay_dataframe_47();
  replay_dataframe_reference::check_replay_dataframe_48();
  replay_dataframe_reference::check_replay_dataframe_49();
  replay_dataframe_reference::check_replay_dataframe_50();
  replay_dataframe_reference::check_replay_dataframe_51();
  replay_dataframe_reference::check_replay_dataframe_52();
  replay_dataframe_reference::check_replay_dataframe_53();
  replay_dataframe_reference::check_replay_dataframe_54();
}
