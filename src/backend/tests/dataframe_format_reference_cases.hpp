// Public formatter report SHA256: 1296781252880533e1ceab02cea94c6a05b75b871c7e5dbc29d69b6b0002f03c
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

} // namespace case_relations_dtype_reference

#include "minera/dataframe_format.hpp"
#include <algorithm>
namespace format_dataframe_reference {
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
inline void check_case_0() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "late", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-02T00:00:00Z", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "second", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:01Z", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "first", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:00Z", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "late", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-02T00:00:00Z", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "second", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:01Z", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "first", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:00Z", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "first", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "second", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:01+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "late", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("normal_defaults_unsorted: result", actual, expected);
  check_format_table("normal_defaults_unsorted: input after call", input, expected_input);
}
inline void check_case_1() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "late", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-02T00:00:00Z", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "second", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:01Z", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "first", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:00Z", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "late", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-02T00:00:00Z", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "second", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:01Z", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "first", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:00Z", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.case_id = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  options.start_timestamp_key = "start_timestamp";
  options.timestamp_format = std::nullopt;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "first", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "second", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:01+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "late", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("normal_explicit_default_selectors: result", actual, expected);
  check_format_table("normal_explicit_default_selectors: input after call", input, expected_input);
}
inline void check_case_2() {
  EventTable input{{"cid", "act", "ended", "began"}, {
{EventAttribute{"cid", "L1", AttributeKind::kString}, EventAttribute{"act", "A", AttributeKind::kString}, EventAttribute{"ended", "01/01/2024 00:00", AttributeKind::kString}, EventAttribute{"began", "31/12/2023 23:59", AttributeKind::kString}},
{EventAttribute{"cid", "L1", AttributeKind::kString}, EventAttribute{"act", "B", AttributeKind::kString}, EventAttribute{"ended", "01/01/2024 00:01", AttributeKind::kString}, EventAttribute{"began", "01/01/2024 00:00", AttributeKind::kString}}}};
  input.dtypes = {"category", "str", "str", "str"};
  input.categorical_dtypes = {EventTableCategoricalDtype{"str", {EventAttribute{"", "L1", AttributeKind::kString}}, false}, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"cid", "act", "ended", "began", "case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"cid", "L1", AttributeKind::kString}, EventAttribute{"act", "A", AttributeKind::kString}, EventAttribute{"ended", "01/01/2024 00:00", AttributeKind::kString}, EventAttribute{"began", "31/12/2023 23:59", AttributeKind::kString}, EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "01/01/2024 00:00", AttributeKind::kString}},
{EventAttribute{"cid", "L1", AttributeKind::kString}, EventAttribute{"act", "B", AttributeKind::kString}, EventAttribute{"ended", "01/01/2024 00:01", AttributeKind::kString}, EventAttribute{"began", "01/01/2024 00:00", AttributeKind::kString}, EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, EventAttribute{"time:timestamp", "01/01/2024 00:01", AttributeKind::kString}}}};
  expected_input.dtypes = {"category", "str", "str", "str", "category", "str", "str"};
  expected_input.categorical_dtypes = {EventTableCategoricalDtype{"str", {EventAttribute{"", "L1", AttributeKind::kString}}, false}, std::nullopt, std::nullopt, std::nullopt, EventTableCategoricalDtype{"str", {EventAttribute{"", "L1", AttributeKind::kString}}, false}, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.case_id = "cid";
  options.activity_key = "act";
  options.timestamp_key = "ended";
  options.start_timestamp_key = "began";
  options.timestamp_format = "%d/%m/%Y %H:%M";
  EventTable expected{{"cid", "act", "ended", "began", "case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index", "start_timestamp"}, {
{EventAttribute{"cid", "L1", AttributeKind::kString}, EventAttribute{"act", "A", AttributeKind::kString}, reference_date("ended", "2024-01-01T00:00:00+00:00"), reference_date("began", "2023-12-31T23:59:00+00:00"), EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0}), reference_date("start_timestamp", "2023-12-31T23:59:00+00:00")},
{EventAttribute{"cid", "L1", AttributeKind::kString}, EventAttribute{"act", "B", AttributeKind::kString}, reference_date("ended", "2024-01-01T00:01:00+00:00"), reference_date("began", "2024-01-01T00:00:00+00:00"), EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:01:00+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0}), reference_date("start_timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected.dtypes = {"category", "str", "datetime64[us, UTC]", "datetime64[us, UTC]", "string", "string", "datetime64[us, UTC]", "int64", "int64", "datetime64[us, UTC]"};
  expected.categorical_dtypes = {EventTableCategoricalDtype{"str", {EventAttribute{"", "L1", AttributeKind::kString}}, false}, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:start_timestamp_key", "start_timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("custom_selectors_and_existing_start: result", actual, expected);
  check_format_table("custom_selectors_and_existing_start: input after call", input, expected_input);
}
inline void check_case_3() {
  EventTable input{{"cid", "act", "ended"}, {
{EventAttribute{"cid", "L1", AttributeKind::kString}, EventAttribute{"act", "A", AttributeKind::kString}, EventAttribute{"ended", "2024-01-01T00:00:00Z", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"cid", "act", "ended", "case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"cid", "L1", AttributeKind::kString}, EventAttribute{"act", "A", AttributeKind::kString}, EventAttribute{"ended", "2024-01-01T00:00:00Z", AttributeKind::kString}, EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:00Z", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str", "str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.case_id = "cid";
  options.activity_key = "act";
  options.timestamp_key = "ended";
  options.start_timestamp_key = "began";
  EventTable expected{{"cid", "act", "ended", "case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"cid", "L1", AttributeKind::kString}, EventAttribute{"act", "A", AttributeKind::kString}, reference_date("ended", "2024-01-01T00:00:00+00:00"), EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"str", "str", "datetime64[us, UTC]", "string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("custom_start_missing_is_added: result", actual, expected);
  check_format_table("custom_start_missing_is_added: input after call", input, expected_input);
}
inline void check_case_4() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "start_timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:01:00Z", AttributeKind::kString}, EventAttribute{"start_timestamp", "2024-01-01T00:00:00Z", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "start_timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:01:00Z", AttributeKind::kString}, EventAttribute{"start_timestamp", "2024-01-01T00:00:00Z", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "start_timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:01:00+00:00"), reference_date("start_timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:start_timestamp_key", "start_timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("default_start_existing_values: result", actual, expected);
  check_format_table("default_start_existing_values: input after call", input, expected_input);
}
inline void check_case_5() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "timestamp", "start_time"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:00Z", AttributeKind::kString}, EventAttribute{"timestamp", "not-selected", AttributeKind::kString}, EventAttribute{"start_time", "also-not-selected", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "timestamp", "start_time"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:00Z", AttributeKind::kString}, EventAttribute{"timestamp", "not-selected", AttributeKind::kString}, EventAttribute{"start_time", "also-not-selected", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "timestamp", "start_time", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"timestamp", "not-selected", AttributeKind::kString}, EventAttribute{"start_time", "also-not-selected", AttributeKind::kString}, dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "str", "str", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("extra_timestamp_like_columns: result", actual, expected);
  check_format_table("extra_timestamp_like_columns: input after call", input, expected_input);
}
inline void check_case_6() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {EventAttribute{"source", "probe", AttributeKind::kString}, format_container("nested", AttributeContainerKind::kContainer, {dtype_number("count", std::int64_t{2}), dtype_number("enabled", true)})};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {EventAttribute{"source", "probe", AttributeKind::kString}, format_container("nested", AttributeContainerKind::kContainer, {dtype_number("count", std::int64_t{2}), dtype_number("enabled", true)})};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"source", "probe", AttributeKind::kString}, format_container("nested", AttributeContainerKind::kContainer, {dtype_number("count", std::int64_t{2}), dtype_number("enabled", true)}), EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("attrs_nested_metadata: result", actual, expected);
  check_format_table("attrs_nested_metadata: input after call", input, expected_input);
}
inline void check_case_7() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-02", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"Index", "int64", {"source_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{40})}, EventTableIndexLabel{reference_number("", std::int64_t{10})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-02", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"Index", "int64", {"source_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{40})}, EventTableIndexLabel{reference_number("", std::int64_t{10})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("named_index_labels: result", actual, expected);
  check_format_table("named_index_labels: input after call", input, expected_input);
}
inline void check_case_8() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "second", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "first", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"Index", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{9})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "second", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "first", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"Index", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{9})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "second", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "first", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("tie_order_preserves_input_order: result", actual, expected);
  check_format_table("tie_order_preserves_input_order: input after call", input, expected_input);
}
inline void check_case_9() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{10}), dtype_number("concept:name", std::int64_t{100}), EventAttribute{"time:timestamp", "2024-01-02", AttributeKind::kString}},
{dtype_number("case:concept:name", std::int64_t{2}), dtype_number("concept:name", std::int64_t{20}), EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  input.dtypes = {"int64", "int64", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{10}), dtype_number("concept:name", std::int64_t{100}), EventAttribute{"time:timestamp", "2024-01-02", AttributeKind::kString}},
{dtype_number("case:concept:name", std::int64_t{2}), dtype_number("concept:name", std::int64_t{20}), EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  expected_input.dtypes = {"int64", "int64", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "10", AttributeKind::kString}, EventAttribute{"concept:name", "100", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "20", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_case_and_activity_ids: result", actual, expected);
  check_format_table("numeric_case_and_activity_ids: input after call", input, expected_input);
}
inline void check_case_10() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", true), dtype_number("concept:name", false), EventAttribute{"time:timestamp", "2024-01-02", AttributeKind::kString}},
{dtype_number("case:concept:name", false), dtype_number("concept:name", true), EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  input.dtypes = {"bool", "bool", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", true), dtype_number("concept:name", false), EventAttribute{"time:timestamp", "2024-01-02", AttributeKind::kString}},
{dtype_number("case:concept:name", false), dtype_number("concept:name", true), EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  expected_input.dtypes = {"bool", "bool", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "False", AttributeKind::kString}, EventAttribute{"concept:name", "True", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "True", AttributeKind::kString}, EventAttribute{"concept:name", "False", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("boolean_case_and_activity_ids: result", actual, expected);
  check_format_table("boolean_case_and_activity_ids: input after call", input, expected_input);
}
inline void check_case_11() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{1.5}), dtype_number("concept:name", std::int64_t{1}), EventAttribute{"time:timestamp", "2024-01-02", AttributeKind::kString}},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  input.dtypes = {"float64", "object", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{1.5}), dtype_number("concept:name", std::int64_t{1}), EventAttribute{"time:timestamp", "2024-01-02", AttributeKind::kString}},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  expected_input.dtypes = {"float64", "object", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "1.0", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1.5", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("float_case_ids_and_object_activity: result", actual, expected);
  check_format_table("float_case_ids_and_object_activity: input after call", input, expected_input);
}
inline void check_case_12() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}},
{reference_null("case:concept:name"), reference_null("concept:name"), EventAttribute{"time:timestamp", "2024-01-02", AttributeKind::kString}}}};
  input.dtypes = {"string", "string", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}},
{reference_null("case:concept:name"), reference_null("concept:name"), EventAttribute{"time:timestamp", "2024-01-02", AttributeKind::kString}}}};
  expected_input.dtypes = {"string", "string", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("nullable_case_activity_values: result", actual, expected);
  check_format_table("nullable_case_activity_values: input after call", input, expected_input);
}
inline void check_case_13() {
  EventTable input{{"concept:name", "time:timestamp"}, {
{EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  input.dtypes = {"str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"concept:name", "time:timestamp"}, {
{EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("missing_case_column: expected public rejection");
  check_format_table("missing_case_column: input after call", input, expected_input);
}
inline void check_case_14() {
  EventTable input{{"case:concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  input.dtypes = {"str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("missing_activity_column: expected public rejection");
  check_format_table("missing_activity_column: input after call", input, expected_input);
}
inline void check_case_15() {
  EventTable input{{"case:concept:name", "concept:name"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}}}};
  input.dtypes = {"str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("missing_timestamp_column: expected public rejection");
  check_format_table("missing_timestamp_column: input after call", input, expected_input);
}
inline void check_case_16() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_null("time:timestamp")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_null("time:timestamp")}}};
  input.dtypes = {"str", "str", "datetime64[s]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_null("time:timestamp")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_null("time:timestamp")}}};
  expected_input.dtypes = {"str", "str", "datetime64[s]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
}};
  expected.dtypes = {"string", "string", "datetime64[s, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("null_timestamp_values: result", actual, expected);
  check_format_table("null_timestamp_values: input after call", input, expected_input);
}
inline void check_case_17() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "not-a-date", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "not-a-date", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "not-a-date", AttributeKind::kString}, dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "str", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("invalid_timestamp_text: result", actual, expected);
  check_format_table("invalid_timestamp_text: input after call", input, expected_input);
}
inline void check_case_18() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "31/12/2024 23:59", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "31/12/2024 23:59", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.timestamp_format = "%d/%m/%Y %H:%M";
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-12-31T23:59:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("explicit_format_success: result", actual, expected);
  check_format_table("explicit_format_success: input after call", input, expected_input);
}
inline void check_case_19() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "31/12/2024", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "31/12/2024", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.timestamp_format = "%m-%d-%Y";
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "31/12/2024", AttributeKind::kString}, dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "str", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("explicit_format_failure: result", actual, expected);
  check_format_table("explicit_format_failure: input after call", input, expected_input);
}
inline void check_case_20() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:01Z", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:01Z", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.timestamp_format = "mixed";
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:01+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("mixed_format_keyword: result", actual, expected);
  check_format_table("mixed_format_keyword: input after call", input, expected_input);
}
inline void check_case_21() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "offset", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T01:00:00+01:00", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "utc", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:00Z", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "offset", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T01:00:00+01:00", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "utc", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:00Z", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "offset", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "utc", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("timezone_offsets_as_strings: result", actual, expected);
  check_format_table("timezone_offsets_as_strings: input after call", input, expected_input);
}
inline void check_case_22() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "late", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "early", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:01+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "late", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "early", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:01+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "late", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "early", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:01+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("timezone_aware_typed_series: result", actual, expected);
  check_format_table("timezone_aware_typed_series: input after call", input, expected_input);
}
inline void check_case_23() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "micro", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.123456")}}};
  input.dtypes = {"str", "str", "datetime64[us]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "micro", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.123456")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "micro", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.123456+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("datetime_microsecond_series: result", actual, expected);
  check_format_table("datetime_microsecond_series: input after call", input, expected_input);
}
inline void check_case_24() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "nano", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.123456789")}}};
  input.dtypes = {"str", "str", "datetime64[ns]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "nano", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.123456789")}}};
  expected_input.dtypes = {"str", "str", "datetime64[ns]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "nano", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.123456789+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[ns, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("datetime_nanosecond_series: result", actual, expected);
  check_format_table("datetime_nanosecond_series: input after call", input, expected_input);
}
inline void check_case_25() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "ten", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{10})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "two", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{2})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "zero", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "negative", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{-3})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "one", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})}}};
  input.dtypes = {"str", "str", "int64"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "ten", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{10})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "two", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{2})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "zero", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "negative", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{-3})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "one", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})}}};
  expected_input.dtypes = {"str", "str", "int64"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "negative", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{-3}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "zero", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "one", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "two", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{2}), dtype_number("@@index", std::int64_t{3}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "ten", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{10}), dtype_number("@@index", std::int64_t{4}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "int64", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_timestamp_values: result", actual, expected);
  check_format_table("numeric_timestamp_values: input after call", input, expected_input);
}
inline void check_case_26() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "false", AttributeKind::kString}, dtype_number("time:timestamp", false)},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "true", AttributeKind::kString}, dtype_number("time:timestamp", true)}}};
  input.dtypes = {"str", "str", "bool"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "false", AttributeKind::kString}, dtype_number("time:timestamp", false)},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "true", AttributeKind::kString}, dtype_number("time:timestamp", true)}}};
  expected_input.dtypes = {"str", "str", "bool"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "false", AttributeKind::kString}, dtype_number("time:timestamp", false), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "true", AttributeKind::kString}, dtype_number("time:timestamp", true), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "bool", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("boolean_timestamp_values: result", actual, expected);
  check_format_table("boolean_timestamp_values: input after call", input, expected_input);
}
inline void check_case_27() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
}};
  input.dtypes = {"object", "object", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
}};
  expected_input.dtypes = {"object", "object", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
}};
  expected.dtypes = {"string", "string", "datetime64[s, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("typed_empty_object_schema: result", actual, expected);
  check_format_table("typed_empty_object_schema: input after call", input, expected_input);
}
inline void check_case_28() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "start_timestamp"}, {
}};
  input.dtypes = {"string", "string", "datetime64[us]", "datetime64[us]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "start_timestamp"}, {
}};
  expected_input.dtypes = {"string", "string", "datetime64[us]", "datetime64[us]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "start_timestamp", "@@index", "@@case_index"}, {
}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:start_timestamp_key", "start_timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("typed_empty_extension_schema: result", actual, expected);
  check_format_table("typed_empty_extension_schema: input after call", input, expected_input);
}
inline void check_case_29() {
  EventTable input{{"extra"}, {
}};
  input.dtypes = {"string"};
  input.categorical_dtypes = {std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  input.metadata.properties = {};
  EventTable expected_input{{"extra"}, {
}};
  expected_input.dtypes = {"string"};
  expected_input.categorical_dtypes = {std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("empty_missing_required_columns: expected public rejection");
  check_format_table("empty_missing_required_columns: input after call", input, expected_input);
}
inline void check_case_30() {
  EventTable input{{"cid", "act", "ended"}, {
}};
  input.dtypes = {"string", "string", "string"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  input.metadata.properties = {};
  EventTable expected_input{{"cid", "act", "ended", "case:concept:name", "concept:name", "time:timestamp"}, {
}};
  expected_input.dtypes = {"string", "string", "string", "string", "string", "string"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.case_id = "cid";
  options.activity_key = "act";
  options.timestamp_key = "ended";
  options.start_timestamp_key = "began";
  EventTable expected{{"cid", "act", "ended", "case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
}};
  expected.dtypes = {"datetime64[s, UTC]", "datetime64[s, UTC]", "datetime64[s, UTC]", "string", "string", "datetime64[s, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("empty_custom_selector_schema: result", actual, expected);
  check_format_table("empty_custom_selector_schema: input after call", input, expected_input);
}
inline void check_case_31() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-02", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  input.dtypes = {"category", "category", "str"};
  input.categorical_dtypes = {EventTableCategoricalDtype{"str", {EventAttribute{"", "L1", AttributeKind::kString}, EventAttribute{"", "L2", AttributeKind::kString}}, false}, EventTableCategoricalDtype{"str", {EventAttribute{"", "A", AttributeKind::kString}, EventAttribute{"", "B", AttributeKind::kString}}, true}, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-02", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  expected_input.dtypes = {"category", "category", "str"};
  expected_input.categorical_dtypes = {EventTableCategoricalDtype{"str", {EventAttribute{"", "L1", AttributeKind::kString}, EventAttribute{"", "L2", AttributeKind::kString}}, false}, EventTableCategoricalDtype{"str", {EventAttribute{"", "A", AttributeKind::kString}, EventAttribute{"", "B", AttributeKind::kString}}, true}, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("categorical_case_activity: result", actual, expected);
  check_format_table("categorical_case_activity: input after call", input, expected_input);
}
inline void check_case_32() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "start_timestamp", "case_id"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}, EventAttribute{"start_timestamp", "2023-12-31", AttributeKind::kString}, EventAttribute{"case_id", "unrelated", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "start_timestamp", "case_id"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}, EventAttribute{"start_timestamp", "2023-12-31", AttributeKind::kString}, EventAttribute{"case_id", "unrelated", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "start_timestamp", "case_id", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), reference_date("start_timestamp", "2023-12-31T00:00:00+00:00"), EventAttribute{"case_id", "unrelated", AttributeKind::kString}, dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "datetime64[us, UTC]", "str", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:start_timestamp_key", "start_timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("canonical_column_collision_extra_start: result", actual, expected);
  check_format_table("canonical_column_collision_extra_start: input after call", input, expected_input);
}
inline void check_case_33() {
  EventTable input{{"same", "when"}, {
{EventAttribute{"same", "L1", AttributeKind::kString}, EventAttribute{"when", "2024-01-01", AttributeKind::kString}}}};
  input.dtypes = {"str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"same", "when", "case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"same", "L1", AttributeKind::kString}, EventAttribute{"when", "2024-01-01", AttributeKind::kString}, EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "L1", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.case_id = "same";
  options.activity_key = "same";
  options.timestamp_key = "when";
  EventTable expected{{"same", "when", "case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"same", "L1", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:00+00:00"), EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "L1", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"str", "datetime64[us, UTC]", "string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("custom_selector_same_case_and_activity: result", actual, expected);
  check_format_table("custom_selector_same_case_and_activity: input after call", input, expected_input);
}
inline void check_case_34() {
  EventTable input{{"cid", "act", "when"}, {
{EventAttribute{"cid", "L1", AttributeKind::kString}, EventAttribute{"act", "A", AttributeKind::kString}, EventAttribute{"when", "2024-01-01", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"cid", "act", "when", "case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"cid", "L1", AttributeKind::kString}, EventAttribute{"act", "A", AttributeKind::kString}, EventAttribute{"when", "2024-01-01", AttributeKind::kString}, EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str", "str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.case_id = "cid";
  options.activity_key = "act";
  options.timestamp_key = "when";
  options.start_timestamp_key = "when";
  EventTable expected{{"cid", "act", "when", "case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index", "start_timestamp"}, {
{EventAttribute{"cid", "L1", AttributeKind::kString}, EventAttribute{"act", "A", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:00+00:00"), EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0}), reference_date("start_timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected.dtypes = {"str", "str", "datetime64[us, UTC]", "string", "string", "datetime64[us, UTC]", "int64", "int64", "datetime64[us, UTC]"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:start_timestamp_key", "start_timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("custom_selector_same_timestamp_and_start: result", actual, expected);
  check_format_table("custom_selector_same_timestamp_and_start: input after call", input, expected_input);
}
inline void check_case_35() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "concept:name"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}, EventAttribute{"concept:name", "duplicate", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "concept:name"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}, EventAttribute{"concept:name", "duplicate", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("duplicate_canonical_columns: expected public rejection");
  check_format_table("duplicate_canonical_columns: input after call", input, expected_input);
}
inline void check_case_36() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-02", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"MultiIndex", "object", {"group", "row"}, {EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{"", "x", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}}, EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{"", "x", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-02", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"MultiIndex", "object", {"group", "row"}, {EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{"", "x", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}}, EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{"", "x", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("multiindex_input_labels: result", actual, expected);
  check_format_table("multiindex_input_labels: input after call", input, expected_input);
}
inline void check_case_37() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "text", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "timestamp", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00")}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "text", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "timestamp", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00")}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "text", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "timestamp", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("object_mixed_timestamp_values: result", actual, expected);
  check_format_table("object_mixed_timestamp_values: input after call", input, expected_input);
}
inline void check_case_38() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "start_timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}, reference_null("start_timestamp")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-02", AttributeKind::kString}, EventAttribute{"start_timestamp", "2024-01-01", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "start_timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}, reference_null("start_timestamp")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-02", AttributeKind::kString}, EventAttribute{"start_timestamp", "2024-01-01", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "start_timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), reference_null("start_timestamp"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), reference_date("start_timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:start_timestamp_key", "start_timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("explicit_start_missing_value: result", actual, expected);
  check_format_table("explicit_start_missing_value: input after call", input, expected_input);
}
inline void check_case_39() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "start_timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T01:00:00+01:00", AttributeKind::kString}, EventAttribute{"start_timestamp", "2024-01-01T00:00:00Z", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "start_timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T01:00:00+01:00", AttributeKind::kString}, EventAttribute{"start_timestamp", "2024-01-01T00:00:00Z", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "start_timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), reference_date("start_timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:start_timestamp_key", "start_timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("explicit_start_timezone_offset: result", actual, expected);
  check_format_table("explicit_start_timezone_offset: input after call", input, expected_input);
}
inline void check_case_41() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.start_timestamp_key = std::nullopt;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("start_selector_none: result", actual, expected);
  check_format_table("start_selector_none: input after call", input, expected_input);
}
inline void check_case_42() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "timestamp", "start_time", "other", "numeric_text"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}, EventAttribute{"timestamp", "2024-01-02", AttributeKind::kString}, EventAttribute{"start_time", "2024-01-03", AttributeKind::kString}, EventAttribute{"other", "2024-01-04", AttributeKind::kString}, EventAttribute{"numeric_text", "123", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str", "str", "str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "timestamp", "start_time", "other", "numeric_text"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}, EventAttribute{"timestamp", "2024-01-02", AttributeKind::kString}, EventAttribute{"start_time", "2024-01-03", AttributeKind::kString}, EventAttribute{"other", "2024-01-04", AttributeKind::kString}, EventAttribute{"numeric_text", "123", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str", "str", "str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "timestamp", "start_time", "other", "numeric_text", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), reference_date("timestamp", "2024-01-02T00:00:00+00:00"), reference_date("start_time", "2024-01-03T00:00:00+00:00"), reference_date("other", "2024-01-04T00:00:00+00:00"), EventAttribute{"numeric_text", "123", AttributeKind::kString}, dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "datetime64[us, UTC]", "datetime64[us, UTC]", "datetime64[us, UTC]", "str", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("extra_valid_date_columns: result", actual, expected);
  check_format_table("extra_valid_date_columns: input after call", input, expected_input);
}
inline void check_case_43() {
  EventTable input{{"cid", "act", "ended", "began", "case:concept:name", "concept:name", "time:timestamp", "start_timestamp"}, {
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"act", "A", AttributeKind::kString}, EventAttribute{"ended", "2024-01-01", AttributeKind::kString}, EventAttribute{"began", "2023-12-31", AttributeKind::kString}, EventAttribute{"case:concept:name", "stale-case", AttributeKind::kString}, EventAttribute{"concept:name", "stale-activity", AttributeKind::kString}, EventAttribute{"time:timestamp", "2020-01-01", AttributeKind::kString}, EventAttribute{"start_timestamp", "2020-01-01", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str", "str", "str", "str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"cid", "act", "ended", "began", "start_timestamp", "case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"act", "A", AttributeKind::kString}, EventAttribute{"ended", "2024-01-01", AttributeKind::kString}, EventAttribute{"began", "2023-12-31", AttributeKind::kString}, EventAttribute{"start_timestamp", "2020-01-01", AttributeKind::kString}, EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str", "str", "str", "str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.case_id = "cid";
  options.activity_key = "act";
  options.timestamp_key = "ended";
  options.start_timestamp_key = "began";
  EventTable expected{{"cid", "act", "ended", "began", "start_timestamp", "case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"act", "A", AttributeKind::kString}, reference_date("ended", "2024-01-01T00:00:00+00:00"), reference_date("began", "2023-12-31T00:00:00+00:00"), reference_date("start_timestamp", "2023-12-31T00:00:00+00:00"), EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"str", "str", "datetime64[us, UTC]", "datetime64[us, UTC]", "datetime64[us, UTC]", "string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:start_timestamp_key", "start_timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("custom_selectors_overwrite_canonical: result", actual, expected);
  check_format_table("custom_selectors_overwrite_canonical: input after call", input, expected_input);
}
inline void check_case_44() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "B", AttributeKind::kString}, EventAttribute{"concept:name", "second", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-02", AttributeKind::kString}, dtype_number("@@index", std::int64_t{99}), dtype_number("@@case_index", std::int64_t{77})},
{EventAttribute{"case:concept:name", "A", AttributeKind::kString}, EventAttribute{"concept:name", "first", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}, dtype_number("@@index", std::int64_t{98}), dtype_number("@@case_index", std::int64_t{76})}}};
  input.dtypes = {"str", "str", "str", "int64", "int64"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "B", AttributeKind::kString}, EventAttribute{"concept:name", "second", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-02", AttributeKind::kString}, dtype_number("@@index", std::int64_t{99}), dtype_number("@@case_index", std::int64_t{77})},
{EventAttribute{"case:concept:name", "A", AttributeKind::kString}, EventAttribute{"concept:name", "first", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}, dtype_number("@@index", std::int64_t{98}), dtype_number("@@case_index", std::int64_t{76})}}};
  expected_input.dtypes = {"str", "str", "str", "int64", "int64"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "A", AttributeKind::kString}, EventAttribute{"concept:name", "first", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "B", AttributeKind::kString}, EventAttribute{"concept:name", "second", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("existing_generated_index_columns: result", actual, expected);
  check_format_table("existing_generated_index_columns: input after call", input, expected_input);
}
inline void check_case_45() {
  EventTable input{{"cid", "ended"}, {
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"ended", "2024-01-01", AttributeKind::kString}}}};
  input.dtypes = {"str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"cid", "ended"}, {
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"ended", "2024-01-01", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.case_id = "cid";
  options.activity_key = "missing";
  options.timestamp_key = "ended";
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("custom_missing_activity_partial_mutation: expected public rejection");
  check_format_table("custom_missing_activity_partial_mutation: input after call", input, expected_input);
}
inline void check_case_46() {
  EventTable input{{"cid", "act"}, {
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"act", "A", AttributeKind::kString}}}};
  input.dtypes = {"str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"cid", "act"}, {
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"act", "A", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.case_id = "cid";
  options.activity_key = "act";
  options.timestamp_key = "missing";
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("custom_missing_timestamp_partial_mutation: expected public rejection");
  check_format_table("custom_missing_timestamp_partial_mutation: input after call", input, expected_input);
}
inline void check_case_47() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "valid", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "invalid", AttributeKind::kString}, EventAttribute{"time:timestamp", "!not-a-date", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "valid", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "invalid", AttributeKind::kString}, EventAttribute{"time:timestamp", "!not-a-date", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "invalid", AttributeKind::kString}, EventAttribute{"time:timestamp", "!not-a-date", AttributeKind::kString}, dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "valid", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}, dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "str", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("mixed_valid_invalid_timestamp_text: result", actual, expected);
  check_format_table("mixed_valid_invalid_timestamp_text: input after call", input, expected_input);
}
inline void check_case_48() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "later", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:00.000002000Z", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "earlier", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:00.000001001Z", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "later", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:00.000002000Z", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "earlier", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01T00:00:00.000001001Z", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "earlier", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000001001+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "later", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000002+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[ns, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("nanosecond_timestamp_strings: result", actual, expected);
  check_format_table("nanosecond_timestamp_strings: input after call", input, expected_input);
}
inline void check_case_49() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"time:timestamp", "case:concept:name", "concept:name"}, {
{EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}, EventAttribute{"case:concept:name", "A", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.case_id = "concept:name";
  options.activity_key = "case:concept:name";
  EventTable expected{{"time:timestamp", "case:concept:name", "concept:name", "@@index", "@@case_index"}, {
{reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"case:concept:name", "A", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"datetime64[us, UTC]", "string", "string", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("cross_canonical_selectors: result", actual, expected);
  check_format_table("cross_canonical_selectors: input after call", input, expected_input);
}
inline void check_case_50() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{2})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A2", AttributeKind::kString}, dtype_number("time:timestamp", false)},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A3", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{2})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A2", AttributeKind::kString}, dtype_number("time:timestamp", false)},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A3", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A2", AttributeKind::kString}, dtype_number("time:timestamp", false), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A0", AttributeKind::kString}, dtype_number("time:timestamp", true), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A3", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{2}), dtype_number("@@index", std::int64_t{3}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("object_boolean_integer_timestamps: result", actual, expected);
  check_format_table("object_boolean_integer_timestamps: input after call", input, expected_input);
}
inline void check_case_51() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A1", AttributeKind::kString}, reference_date("time:timestamp", "1970-04-15T05:59:59.254740992+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A0", AttributeKind::kString}, reference_date("time:timestamp", "1970-04-15T05:59:59.254740993+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[ns, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("object_exact_numeric_timestamps: result", actual, expected);
  check_format_table("object_exact_numeric_timestamps: input after call", input, expected_input);
}
inline void check_case_52() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A0", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A0", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A0", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("object_unsigned_timestamps: result", actual, expected);
  check_format_table("object_unsigned_timestamps: input after call", input, expected_input);
}
inline void check_case_53() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{3}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_duplicate_float_late_same: result", actual, expected);
  check_format_table("numeric_boundary_duplicate_float_late_same: input after call", input, expected_input);
}
inline void check_case_54() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{2})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL}), dtype_number("@@index", std::int64_t{3}), dtype_number("@@case_index", std::int64_t{3})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_duplicate_float_late_split: result", actual, expected);
  check_format_table("numeric_boundary_duplicate_float_late_split: input after call", input, expected_input);
}
inline void check_case_55() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_duplicate_float_first_same: expected public rejection");
  check_format_table("numeric_boundary_duplicate_float_first_same: input after call", input, expected_input);
}
inline void check_case_56() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_duplicate_float_first_split: expected public rejection");
  check_format_table("numeric_boundary_duplicate_float_first_split: input after call", input, expected_input);
}
inline void check_case_57() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "4", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "4", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_duplicate_bool_late_same: expected public rejection");
  check_format_table("numeric_boundary_duplicate_bool_late_same: input after call", input, expected_input);
}
inline void check_case_58() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "4", AttributeKind::kString}, EventAttribute{"concept:name", "4", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "4", AttributeKind::kString}, EventAttribute{"concept:name", "4", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_duplicate_bool_late_split: expected public rejection");
  check_format_table("numeric_boundary_duplicate_bool_late_split: input after call", input, expected_input);
}
inline void check_case_59() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "4", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "4", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "4", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL}), dtype_number("@@index", std::int64_t{3}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{4}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_duplicate_bool_first_same: result", actual, expected);
  check_format_table("numeric_boundary_duplicate_bool_first_same: input after call", input, expected_input);
}
inline void check_case_60() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "4", AttributeKind::kString}, EventAttribute{"concept:name", "4", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "4", AttributeKind::kString}, EventAttribute{"concept:name", "4", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{2})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{3}), dtype_number("@@case_index", std::int64_t{3})},
{EventAttribute{"case:concept:name", "4", AttributeKind::kString}, EventAttribute{"concept:name", "4", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL}), dtype_number("@@index", std::int64_t{4}), dtype_number("@@case_index", std::int64_t{4})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_duplicate_bool_first_split: result", actual, expected);
  check_format_table("numeric_boundary_duplicate_bool_first_split: input after call", input, expected_input);
}
inline void check_case_61() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{-0.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{-0.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{-0.0}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{3}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_float_negative_zero_same: result", actual, expected);
  check_format_table("numeric_boundary_float_negative_zero_same: input after call", input, expected_input);
}
inline void check_case_62() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{-0.0})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{-0.0})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{-0.0}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{2})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL}), dtype_number("@@index", std::int64_t{3}), dtype_number("@@case_index", std::int64_t{3})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_float_negative_zero_split: result", actual, expected);
  check_format_table("numeric_boundary_float_negative_zero_split: input after call", input, expected_input);
}
inline void check_case_63() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{3}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_bool_float_wide_same: result", actual, expected);
  check_format_table("numeric_boundary_bool_float_wide_same: input after call", input, expected_input);
}
inline void check_case_64() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{2})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL}), dtype_number("@@index", std::int64_t{3}), dtype_number("@@case_index", std::int64_t{3})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_bool_float_wide_split: result", actual, expected);
  check_format_table("numeric_boundary_bool_float_wide_split: input after call", input, expected_input);
}
inline void check_case_65() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, EventAttribute{"time:timestamp", "z", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, EventAttribute{"time:timestamp", "z", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, EventAttribute{"time:timestamp", "z", AttributeKind::kString}, dtype_number("@@index", std::int64_t{3}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_string_float_wide_same: result", actual, expected);
  check_format_table("numeric_boundary_string_float_wide_same: input after call", input, expected_input);
}
inline void check_case_66() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, EventAttribute{"time:timestamp", "z", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, EventAttribute{"time:timestamp", "z", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, EventAttribute{"time:timestamp", "z", AttributeKind::kString}, dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.5}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{2})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL}), dtype_number("@@index", std::int64_t{3}), dtype_number("@@case_index", std::int64_t{3})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_string_float_wide_split: result", actual, expected);
  check_format_table("numeric_boundary_string_float_wide_split: input after call", input, expected_input);
}
inline void check_case_67() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_float_exact_equal_same: result", actual, expected);
  check_format_table("numeric_boundary_float_exact_equal_same: input after call", input, expected_input);
}
inline void check_case_68() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{2})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_float_exact_equal_split: result", actual, expected);
  check_format_table("numeric_boundary_float_exact_equal_split: input after call", input, expected_input);
}
inline void check_case_69() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_float_rounding_pair_same: expected public rejection");
  check_format_table("numeric_boundary_float_rounding_pair_same: input after call", input, expected_input);
}
inline void check_case_70() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_float_rounding_pair_split: expected public rejection");
  check_format_table("numeric_boundary_float_rounding_pair_split: input after call", input, expected_input);
}
inline void check_case_71() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{3}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_float_bool_rounding_same: result", actual, expected);
  check_format_table("numeric_boundary_float_bool_rounding_same: input after call", input, expected_input);
}
inline void check_case_72() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{2})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{3}), dtype_number("@@case_index", std::int64_t{3})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_float_bool_rounding_split: result", actual, expected);
  check_format_table("numeric_boundary_float_bool_rounding_split: input after call", input, expected_input);
}
inline void check_case_73() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, EventAttribute{"time:timestamp", "!bad", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{2})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, EventAttribute{"time:timestamp", "!bad", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{2})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{2}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, EventAttribute{"time:timestamp", "!bad", AttributeKind::kString}, dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_timestamp_text_before_number_same: result", actual, expected);
  check_format_table("numeric_boundary_timestamp_text_before_number_same: input after call", input, expected_input);
}
inline void check_case_74() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, EventAttribute{"time:timestamp", "!bad", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{2})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, EventAttribute{"time:timestamp", "!bad", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{2})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, EventAttribute{"time:timestamp", "!bad", AttributeKind::kString}, dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{2}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_timestamp_text_before_number_split: result", actual, expected);
  check_format_table("numeric_boundary_timestamp_text_before_number_split: input after call", input, expected_input);
}
inline void check_case_75() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, EventAttribute{"time:timestamp", "!bad", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, EventAttribute{"time:timestamp", "!bad", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", true), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, EventAttribute{"time:timestamp", "!bad", AttributeKind::kString}, dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_timestamp_boolean_text_same: result", actual, expected);
  check_format_table("numeric_boundary_timestamp_boolean_text_same: input after call", input, expected_input);
}
inline void check_case_76() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, EventAttribute{"time:timestamp", "!bad", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, EventAttribute{"time:timestamp", "!bad", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, EventAttribute{"time:timestamp", "!bad", AttributeKind::kString}, dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", true), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{2})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_timestamp_boolean_text_split: result", actual, expected);
  check_format_table("numeric_boundary_timestamp_boolean_text_split: input after call", input, expected_input);
}
inline void check_case_77() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, EventAttribute{"time:timestamp", "bad", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, EventAttribute{"time:timestamp", "bad", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)()), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, EventAttribute{"time:timestamp", "bad", AttributeKind::kString}, dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_nanosecond_nat_with_bad_same: result", actual, expected);
  check_format_table("numeric_boundary_nanosecond_nat_with_bad_same: input after call", input, expected_input);
}
inline void check_case_78() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, EventAttribute{"time:timestamp", "bad", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, EventAttribute{"time:timestamp", "bad", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)()), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, EventAttribute{"time:timestamp", "bad", AttributeKind::kString}, dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_nanosecond_nat_with_bad_split: result", actual, expected);
  check_format_table("numeric_boundary_nanosecond_nat_with_bad_split: input after call", input, expected_input);
}
inline void check_case_79() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_date("time:timestamp", "1970-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[ns, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_numeric_datetime_string_same: result", actual, expected);
  check_format_table("numeric_boundary_numeric_datetime_string_same: input after call", input, expected_input);
}
inline void check_case_80() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024-01-01", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_date("time:timestamp", "1970-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected.dtypes = {"string", "string", "datetime64[ns, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_numeric_datetime_string_split: result", actual, expected);
  check_format_table("numeric_boundary_numeric_datetime_string_split: input after call", input, expected_input);
}
inline void check_case_81() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", true)}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", true)}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)()), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", true), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_numeric_signed_nat_and_bool_same: result", actual, expected);
  check_format_table("numeric_boundary_numeric_signed_nat_and_bool_same: input after call", input, expected_input);
}
inline void check_case_82() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", true)}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", true)}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)()), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", true), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_numeric_signed_nat_and_bool_split: result", actual, expected);
  check_format_table("numeric_boundary_numeric_signed_nat_and_bool_split: input after call", input, expected_input);
}
inline void check_case_83() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_wide_float_collision_same: expected public rejection");
  check_format_table("numeric_boundary_wide_float_collision_same: input after call", input, expected_input);
}
inline void check_case_84() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_wide_float_collision_split: expected public rejection");
  check_format_table("numeric_boundary_wide_float_collision_split: input after call", input, expected_input);
}
inline void check_case_85() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740994})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740994})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740994}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_wide_float_safe_same: result", actual, expected);
  check_format_table("numeric_boundary_wide_float_safe_same: input after call", input, expected_input);
}
inline void check_case_86() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740994})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740994})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740994}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{2})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_wide_float_safe_split: result", actual, expected);
  check_format_table("numeric_boundary_wide_float_safe_split: input after call", input, expected_input);
}
inline void check_case_87() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740992})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740992})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740992}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{3}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_wide_integer_collision_same: result", actual, expected);
  check_format_table("numeric_boundary_wide_integer_collision_same: input after call", input, expected_input);
}
inline void check_case_88() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740992})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740992})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740992}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{2})},
{EventAttribute{"case:concept:name", "3", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL}), dtype_number("@@index", std::int64_t{3}), dtype_number("@@case_index", std::int64_t{3})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_wide_integer_collision_split: result", actual, expected);
  check_format_table("numeric_boundary_wide_integer_collision_split: input after call", input, expected_input);
}
inline void check_case_89() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_wide_only_float_same: expected public rejection");
  check_format_table("numeric_boundary_wide_only_float_same: input after call", input, expected_input);
}
inline void check_case_90() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_wide_only_float_split: expected public rejection");
  check_format_table("numeric_boundary_wide_only_float_split: input after call", input, expected_input);
}
inline void check_case_91() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_wide_one_float_same: result", actual, expected);
  check_format_table("numeric_boundary_wide_one_float_same: input after call", input, expected_input);
}
inline void check_case_92() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_wide_one_float_split: result", actual, expected);
  check_format_table("numeric_boundary_wide_one_float_split: input after call", input, expected_input);
}
inline void check_case_93() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_wide_bool_collision_same: result", actual, expected);
  check_format_table("numeric_boundary_wide_bool_collision_same: input after call", input, expected_input);
}
inline void check_case_94() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{2})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_wide_bool_collision_split: result", actual, expected);
  check_format_table("numeric_boundary_wide_bool_collision_split: input after call", input, expected_input);
}
inline void check_case_95() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740992})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740992})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740992}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_bool_exact_collision_same: result", actual, expected);
  check_format_table("numeric_boundary_bool_exact_collision_same: input after call", input, expected_input);
}
inline void check_case_96() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740992})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true)},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740992})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", true), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740992}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{2})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_bool_exact_collision_split: result", actual, expected);
  check_format_table("numeric_boundary_bool_exact_collision_split: input after call", input, expected_input);
}
inline void check_case_97() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{-1})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{-1})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{-1}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_negative_wide_same: result", actual, expected);
  check_format_table("numeric_boundary_negative_wide_same: input after call", input, expected_input);
}
inline void check_case_98() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{-1})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{-1})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{-1}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL}), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{2})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_negative_wide_split: result", actual, expected);
  check_format_table("numeric_boundary_negative_wide_split: input after call", input, expected_input);
}
inline void check_case_99() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{-1.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{-1.0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_negative_wide_float_same: expected public rejection");
  check_format_table("numeric_boundary_negative_wide_float_same: input after call", input, expected_input);
}
inline void check_case_100() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{-1.0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{-1.0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_negative_wide_float_split: expected public rejection");
  check_format_table("numeric_boundary_negative_wide_float_split: input after call", input, expected_input);
}
inline void check_case_101() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::numeric_limits<double>::infinity())},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740992})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::numeric_limits<double>::infinity())},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740992})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_infinite_collision_same: expected public rejection");
  check_format_table("numeric_boundary_infinite_collision_same: input after call", input, expected_input);
}
inline void check_case_102() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::numeric_limits<double>::infinity())},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740992})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::numeric_limits<double>::infinity())},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740992})},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_infinite_collision_split: expected public rejection");
  check_format_table("numeric_boundary_infinite_collision_split: input after call", input, expected_input);
}
inline void check_case_103() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", -std::numeric_limits<double>::infinity())},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{-9223372036854775807})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", -std::numeric_limits<double>::infinity())},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{-9223372036854775807})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_negative_infinite_same: expected public rejection");
  check_format_table("numeric_boundary_negative_infinite_same: input after call", input, expected_input);
}
inline void check_case_104() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", -std::numeric_limits<double>::infinity())},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{-9223372036854775807})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", -std::numeric_limits<double>::infinity())},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{-9223372036854775807})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_negative_infinite_split: expected public rejection");
  check_format_table("numeric_boundary_negative_infinite_split: input after call", input, expected_input);
}
inline void check_case_105() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, EventAttribute{"time:timestamp", "2", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, EventAttribute{"time:timestamp", "2", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, EventAttribute{"time:timestamp", "2", AttributeKind::kString}, dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_numeric_string_same: result", actual, expected);
  check_format_table("numeric_boundary_numeric_string_same: input after call", input, expected_input);
}
inline void check_case_106() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, EventAttribute{"time:timestamp", "2", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, EventAttribute{"time:timestamp", "2", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, EventAttribute{"time:timestamp", "2", AttributeKind::kString}, dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_numeric_string_split: result", actual, expected);
  check_format_table("numeric_boundary_numeric_string_split: input after call", input, expected_input);
}
inline void check_case_107() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_date("time:timestamp", "1970-04-15T05:59:59.254740992+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_date("time:timestamp", "1970-04-15T05:59:59.254740993+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[ns, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_mixed_object_int_float_same: result", actual, expected);
  check_format_table("numeric_boundary_mixed_object_int_float_same: input after call", input, expected_input);
}
inline void check_case_108() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_date("time:timestamp", "1970-04-15T05:59:59.254740993+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_date("time:timestamp", "1970-04-15T05:59:59.254740992+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected.dtypes = {"string", "string", "datetime64[ns, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_mixed_object_int_float_split: result", actual, expected);
  check_format_table("numeric_boundary_mixed_object_int_float_split: input after call", input, expected_input);
}
inline void check_case_109() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9223372036854775807})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9223372036854775807})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_date("time:timestamp", "2262-04-11T23:47:16.854775807+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[ns, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_object_large_signed_same: result", actual, expected);
  check_format_table("numeric_boundary_object_large_signed_same: input after call", input, expected_input);
}
inline void check_case_110() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9223372036854775807})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9223372036854775807})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_date("time:timestamp", "2262-04-11T23:47:16.854775807+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[ns, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_object_large_signed_split: result", actual, expected);
  check_format_table("numeric_boundary_object_large_signed_split: input after call", input, expected_input);
}
inline void check_case_111() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_date("time:timestamp", "1970-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[ns, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_object_signed_nat_same: result", actual, expected);
  check_format_table("numeric_boundary_object_signed_nat_same: input after call", input, expected_input);
}
inline void check_case_112() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", (std::numeric_limits<std::int64_t>::min)())},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_date("time:timestamp", "1970-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[ns, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_object_signed_nat_split: result", actual, expected);
  check_format_table("numeric_boundary_object_signed_nat_split: input after call", input, expected_input);
}
inline void check_case_113() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{9223372036854775808ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{9223372036854775809ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{9223372036854775808ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{9223372036854775809ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{9223372036854775808ULL}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{9223372036854775809ULL}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_object_unsigned_epoch_same: result", actual, expected);
  check_format_table("numeric_boundary_object_unsigned_epoch_same: input after call", input, expected_input);
}
inline void check_case_114() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{9223372036854775808ULL})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{9223372036854775809ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{9223372036854775808ULL})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{9223372036854775809ULL})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{9223372036854775808ULL}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{9223372036854775809ULL}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_object_unsigned_epoch_split: result", actual, expected);
  check_format_table("numeric_boundary_object_unsigned_epoch_split: input after call", input, expected_input);
}
inline void check_case_115() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_object_unsigned_wrap_same: result", actual, expected);
  check_format_table("numeric_boundary_object_unsigned_wrap_same: input after call", input, expected_input);
}
inline void check_case_116() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL}), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{0}), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected.dtypes = {"string", "string", "object", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("numeric_boundary_object_unsigned_wrap_split: result", actual, expected);
  check_format_table("numeric_boundary_object_unsigned_wrap_split: input after call", input, expected_input);
}
inline void check_case_117() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{9.223372036854776e+18})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9223372036854775807})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{9.223372036854776e+18})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9223372036854775807})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_object_overflow_float_same: expected public rejection");
  check_format_table("numeric_boundary_object_overflow_float_same: input after call", input, expected_input);
}
inline void check_case_118() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{9.223372036854776e+18})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9223372036854775807})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "0", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", double{9.223372036854776e+18})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9223372036854775807})}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_object_overflow_float_split: expected public rejection");
  check_format_table("numeric_boundary_object_overflow_float_split: input after call", input, expected_input);
}
inline void check_case_119() {
  EventTable input{{"cid", "act", "when"}, {
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"act", "0", AttributeKind::kString}, dtype_number("when", std::int64_t{9007199254740993})},
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"act", "1", AttributeKind::kString}, dtype_number("when", double{9007199254740992.0})},
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"act", "2", AttributeKind::kString}, reference_number("when", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"act", "3", AttributeKind::kString}, reference_number("when", std::uint64_t{18446744073709551614ULL})}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"cid", "act", "when", "case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"act", "0", AttributeKind::kString}, dtype_number("when", std::int64_t{9007199254740993}), EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "0", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{9007199254740993})},
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"act", "1", AttributeKind::kString}, dtype_number("when", double{9007199254740992.0}), EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "1", AttributeKind::kString}, dtype_number("time:timestamp", double{9007199254740992.0})},
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"act", "2", AttributeKind::kString}, reference_number("when", std::uint64_t{18446744073709551615ULL}), EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "2", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551615ULL})},
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"act", "3", AttributeKind::kString}, reference_number("when", std::uint64_t{18446744073709551614ULL}), EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "3", AttributeKind::kString}, reference_number("time:timestamp", std::uint64_t{18446744073709551614ULL})}}};
  expected_input.dtypes = {"str", "str", "object", "str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.case_id = "cid";
  options.activity_key = "act";
  options.timestamp_key = "when";
  bool rejected = false;
  try { (void)format_dataframe(input, options); }
  catch (const std::exception&) { rejected = true; }
  if (!rejected) throw std::runtime_error("numeric_boundary_custom_error_mutation: expected public rejection");
  check_format_table("numeric_boundary_custom_error_mutation: input after call", input, expected_input);
}
inline void check_case_120() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024/02/31", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024/03/01", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024/02/31", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024/03/01", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.timestamp_format = "%Y/%m/%d";
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024/02/31", AttributeKind::kString}, dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024/03/01", AttributeKind::kString}, dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "str", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("calendar_invalid_february: result", actual, expected);
  check_format_table("calendar_invalid_february: input after call", input, expected_input);
}
inline void check_case_121() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2023/02/29", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2023/02/29", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.timestamp_format = "%Y/%m/%d";
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2023/02/29", AttributeKind::kString}, dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "str", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("calendar_nonleap_february: result", actual, expected);
  check_format_table("calendar_nonleap_february: input after call", input, expected_input);
}
inline void check_case_122() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024/02/29", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024/02/29", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.timestamp_format = "%Y/%m/%d";
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-02-29T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("calendar_leap_february: result", actual, expected);
  check_format_table("calendar_leap_february: input after call", input, expected_input);
}
inline void check_case_123() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024/04/31", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024/04/31", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.timestamp_format = "%Y/%m/%d";
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024/04/31", AttributeKind::kString}, dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "str", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("calendar_invalid_april: result", actual, expected);
  check_format_table("calendar_invalid_april: input after call", input, expected_input);
}
inline void check_case_124() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024/04/30", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2024/04/30", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.timestamp_format = "%Y/%m/%d";
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-04-30T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("calendar_valid_april: result", actual, expected);
  check_format_table("calendar_valid_april: input after call", input, expected_input);
}
inline void check_case_125() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "1900/02/29", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "1900/02/29", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.timestamp_format = "%Y/%m/%d";
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "1900/02/29", AttributeKind::kString}, dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "str", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("calendar_century_nonleap: result", actual, expected);
  check_format_table("calendar_century_nonleap: input after call", input, expected_input);
}
inline void check_case_126() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2000/02/29", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "2000/02/29", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameFormatOptions options;
  options.timestamp_format = "%Y/%m/%d";
  EventTable expected{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2000-02-29T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})}}};
  expected.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected.metadata.properties = {EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto actual = format_dataframe(input, options);
  actual.validate();
  check_format_table("calendar_century_leap: result", actual, expected);
  check_format_table("calendar_century_leap: input after call", input, expected_input);
}
} // namespace format_dataframe_reference
inline void check_public_format_dataframe_reference_cases() {
  std::string failures;
  try { format_dataframe_reference::check_case_0(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_1(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_2(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_3(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_4(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_5(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_6(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_7(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_8(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_9(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_10(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_11(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_12(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_13(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_14(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_15(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_16(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_17(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_18(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_19(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_20(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_21(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_22(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_23(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_24(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_25(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_26(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_27(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_28(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_29(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_30(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_31(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_32(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_33(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_34(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_35(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_36(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_37(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_38(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_39(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_41(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_42(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_43(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_44(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_45(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_46(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_47(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_48(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_49(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_50(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_51(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_52(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_53(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_54(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_55(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_56(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_57(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_58(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_59(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_60(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_61(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_62(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_63(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_64(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_65(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_66(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_67(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_68(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_69(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_70(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_71(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_72(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_73(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_74(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_75(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_76(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_77(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_78(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_79(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_80(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_81(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_82(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_83(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_84(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_85(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_86(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_87(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_88(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_89(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_90(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_91(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_92(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_93(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_94(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_95(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_96(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_97(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_98(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_99(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_100(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_101(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_102(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_103(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_104(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_105(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_106(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_107(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_108(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_109(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_110(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_111(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_112(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_113(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_114(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_115(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_116(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_117(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_118(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_119(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_120(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_121(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_122(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_123(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_124(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_125(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  try { format_dataframe_reference::check_case_126(); }
  catch (const std::exception& error) { failures += std::string(error.what()) + "\n"; }
  if (!failures.empty()) throw std::runtime_error(failures);
}
