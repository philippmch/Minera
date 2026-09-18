#include "minera/log_conversion.hpp"
// Metadata public report SHA256: d034b1f08122a256ba69bd2e8905fc0327ace088a088c7406bce984f148387b8
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
namespace dataframe_conversion_metadata_reference {
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
inline void check_metadata_case_0() {
  LogConversionOptions options; options.case_id_key = "case:concept:name";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 2) throw std::runtime_error("raw_log: metadata/length differs"); };
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  const auto eager = event_table_to_event_log(input, options); check(eager, eager.traces.size());
  const auto lazy = lazy_event_table_to_event_log(LazyEventTable::from_table(input), options); check(lazy, lazy.traces.size());
  check_format_table("raw_log: input after", input, expected_input);
}
inline void check_metadata_case_1() {
  LogConversionOptions options; options.case_id_key = "case:concept:name";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 3) throw std::runtime_error("raw_stream: metadata/length differs"); };
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  const auto eager = event_table_to_event_stream(input, options); check(eager, eager.events.size());
  const auto lazy = lazy_event_table_to_event_stream(LazyEventTable::from_table(input), options); check(lazy, lazy.events.size());
  check_format_table("raw_stream: input after", input, expected_input);
}
inline void check_metadata_case_2() {
  LogConversionOptions options; options.case_id_key = "case:concept:name";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {EventAttribute{"case_id_key", "ordinary", AttributeKind::kString}};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 2) throw std::runtime_error("case_property_log: metadata/length differs"); };
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {EventAttribute{"case_id_key", "ordinary", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "alternate", AttributeKind::kString}};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {EventAttribute{"case_id_key", "ordinary", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "alternate", AttributeKind::kString}};
  const auto eager = event_table_to_event_log(input, options); check(eager, eager.traces.size());
  const auto lazy = lazy_event_table_to_event_log(LazyEventTable::from_table(input), options); check(lazy, lazy.traces.size());
  check_format_table("case_property_log: input after", input, expected_input);
}
inline void check_metadata_case_3() {
  LogConversionOptions options; options.case_id_key = "case:concept:name";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {EventAttribute{"case_id_key", "ordinary", AttributeKind::kString}};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 3) throw std::runtime_error("case_property_stream: metadata/length differs"); };
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {EventAttribute{"case_id_key", "ordinary", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "alternate", AttributeKind::kString}};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {EventAttribute{"case_id_key", "ordinary", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "alternate", AttributeKind::kString}};
  const auto eager = event_table_to_event_stream(input, options); check(eager, eager.events.size());
  const auto lazy = lazy_event_table_to_event_stream(LazyEventTable::from_table(input), options); check(lazy, lazy.events.size());
  check_format_table("case_property_stream: input after", input, expected_input);
}
inline void check_metadata_case_4() {
  LogConversionOptions options; options.case_id_key = "case:concept:name";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {format_container("nested", AttributeContainerKind::kContainer, {dtype_number("enabled", true), format_container("values", AttributeContainerKind::kList, {dtype_number("", std::int64_t{1}), EventAttribute{"", "two", AttributeKind::kString}, reference_null("")})}), EventAttribute{"source", "probe", AttributeKind::kString}};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 2) throw std::runtime_error("nested_attrs_log: metadata/length differs"); };
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {format_container("nested", AttributeContainerKind::kContainer, {dtype_number("enabled", true), format_container("values", AttributeContainerKind::kList, {dtype_number("", std::int64_t{1}), EventAttribute{"", "two", AttributeKind::kString}, reference_null("")})}), EventAttribute{"source", "probe", AttributeKind::kString}};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {format_container("nested", AttributeContainerKind::kContainer, {dtype_number("enabled", true), format_container("values", AttributeContainerKind::kList, {dtype_number("", std::int64_t{1}), EventAttribute{"", "two", AttributeKind::kString}, reference_null("")})}), EventAttribute{"source", "probe", AttributeKind::kString}};
  const auto eager = event_table_to_event_log(input, options); check(eager, eager.traces.size());
  const auto lazy = lazy_event_table_to_event_log(LazyEventTable::from_table(input), options); check(lazy, lazy.traces.size());
  check_format_table("nested_attrs_log: input after", input, expected_input);
}
inline void check_metadata_case_5() {
  LogConversionOptions options; options.case_id_key = "case:concept:name";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {format_container("nested", AttributeContainerKind::kContainer, {dtype_number("enabled", true), format_container("values", AttributeContainerKind::kList, {dtype_number("", std::int64_t{1}), EventAttribute{"", "two", AttributeKind::kString}, reference_null("")})}), EventAttribute{"source", "probe", AttributeKind::kString}};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 3) throw std::runtime_error("nested_attrs_stream: metadata/length differs"); };
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {format_container("nested", AttributeContainerKind::kContainer, {dtype_number("enabled", true), format_container("values", AttributeContainerKind::kList, {dtype_number("", std::int64_t{1}), EventAttribute{"", "two", AttributeKind::kString}, reference_null("")})}), EventAttribute{"source", "probe", AttributeKind::kString}};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {format_container("nested", AttributeContainerKind::kContainer, {dtype_number("enabled", true), format_container("values", AttributeContainerKind::kList, {dtype_number("", std::int64_t{1}), EventAttribute{"", "two", AttributeKind::kString}, reference_null("")})}), EventAttribute{"source", "probe", AttributeKind::kString}};
  const auto eager = event_table_to_event_stream(input, options); check(eager, eager.events.size());
  const auto lazy = lazy_event_table_to_event_stream(LazyEventTable::from_table(input), options); check(lazy, lazy.events.size());
  check_format_table("nested_attrs_stream: input after", input, expected_input);
}
inline void check_metadata_case_6() {
  LogConversionOptions options; options.case_id_key = "case:concept:name";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {EventAttribute{"origin", "custom", AttributeKind::kString}};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 2) throw std::runtime_error("origin_property_log: metadata/length differs"); };
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {EventAttribute{"origin", "custom", AttributeKind::kString}, reference_null("pm4py:param:case_id_key")};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {EventAttribute{"origin", "custom", AttributeKind::kString}, reference_null("pm4py:param:case_id_key")};
  const auto eager = event_table_to_event_log(input, options); check(eager, eager.traces.size());
  const auto lazy = lazy_event_table_to_event_log(LazyEventTable::from_table(input), options); check(lazy, lazy.traces.size());
  check_format_table("origin_property_log: input after", input, expected_input);
}
inline void check_metadata_case_7() {
  LogConversionOptions options; options.case_id_key = "case:concept:name";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {EventAttribute{"origin", "custom", AttributeKind::kString}};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 3) throw std::runtime_error("origin_property_stream: metadata/length differs"); };
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {EventAttribute{"origin", "custom", AttributeKind::kString}, reference_null("pm4py:param:case_id_key")};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {EventAttribute{"origin", "custom", AttributeKind::kString}, reference_null("pm4py:param:case_id_key")};
  const auto eager = event_table_to_event_stream(input, options); check(eager, eager.events.size());
  const auto lazy = lazy_event_table_to_event_stream(LazyEventTable::from_table(input), options); check(lazy, lazy.events.size());
  check_format_table("origin_property_stream: input after", input, expected_input);
}
inline void check_metadata_case_8() {
  LogConversionOptions options; options.case_id_key = "case:concept:name";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {EventAttribute{"source", "formatted", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 2) throw std::runtime_error("formatted_log: metadata/length differs"); };
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{1})}}};
  input.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {EventAttribute{"source", "formatted", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected_input.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {EventAttribute{"source", "formatted", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto eager = event_table_to_event_log(input, options); check(eager, eager.traces.size());
  const auto lazy = lazy_event_table_to_event_log(LazyEventTable::from_table(input), options); check(lazy, lazy.traces.size());
  check_format_table("formatted_log: input after", input, expected_input);
}
inline void check_metadata_case_9() {
  LogConversionOptions options; options.case_id_key = "case:concept:name";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {EventAttribute{"source", "formatted", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 3) throw std::runtime_error("formatted_stream: metadata/length differs"); };
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{1})}}};
  input.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {EventAttribute{"source", "formatted", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00"), dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"case:concept:name", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected_input.dtypes = {"string", "string", "datetime64[us, UTC]", "int64", "int64"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {EventAttribute{"source", "formatted", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto eager = event_table_to_event_stream(input, options); check(eager, eager.events.size());
  const auto lazy = lazy_event_table_to_event_stream(LazyEventTable::from_table(input), options); check(lazy, lazy.events.size());
  check_format_table("formatted_stream: input after", input, expected_input);
}
inline void check_metadata_case_10() {
  LogConversionOptions options; options.case_id_key = "cid";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {EventAttribute{"case_id_key", "cid", AttributeKind::kString}};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 2) throw std::runtime_error("custom_case_log: metadata/length differs"); };
  EventTable input{{"cid", "concept:name", "time:timestamp"}, {
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"cid", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {EventAttribute{"case_id_key", "cid", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "cid", AttributeKind::kString}};
  EventTable expected_input{{"cid", "concept:name", "time:timestamp"}, {
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"cid", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {EventAttribute{"case_id_key", "cid", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "cid", AttributeKind::kString}};
  const auto eager = event_table_to_event_log(input, options); check(eager, eager.traces.size());
  const auto lazy = lazy_event_table_to_event_log(LazyEventTable::from_table(input), options); check(lazy, lazy.traces.size());
  check_format_table("custom_case_log: input after", input, expected_input);
}
inline void check_metadata_case_11() {
  LogConversionOptions options; options.case_id_key = "cid";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {EventAttribute{"case_id_key", "cid", AttributeKind::kString}};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 3) throw std::runtime_error("custom_case_stream: metadata/length differs"); };
  EventTable input{{"cid", "concept:name", "time:timestamp"}, {
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"cid", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {EventAttribute{"case_id_key", "cid", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "cid", AttributeKind::kString}};
  EventTable expected_input{{"cid", "concept:name", "time:timestamp"}, {
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"cid", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {EventAttribute{"case_id_key", "cid", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "cid", AttributeKind::kString}};
  const auto eager = event_table_to_event_stream(input, options); check(eager, eager.events.size());
  const auto lazy = lazy_event_table_to_event_stream(LazyEventTable::from_table(input), options); check(lazy, lazy.events.size());
  check_format_table("custom_case_stream: input after", input, expected_input);
}
inline void check_metadata_case_12() {
  LogConversionOptions options; options.case_id_key = "cid";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {EventAttribute{"source", "custom", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 2) throw std::runtime_error("custom_formatted_log: metadata/length differs"); };
  EventTable input{{"cid", "concept:name", "time:timestamp", "case:concept:name", "@@index", "@@case_index"}, {
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"case:concept:name", "C", AttributeKind::kString}, dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00"), EventAttribute{"case:concept:name", "C", AttributeKind::kString}, dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"cid", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), EventAttribute{"case:concept:name", "D", AttributeKind::kString}, dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{1})}}};
  input.dtypes = {"str", "string", "datetime64[us, UTC]", "string", "int64", "int64"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {EventAttribute{"source", "custom", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  EventTable expected_input{{"cid", "concept:name", "time:timestamp", "case:concept:name", "@@index", "@@case_index"}, {
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"case:concept:name", "C", AttributeKind::kString}, dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00"), EventAttribute{"case:concept:name", "C", AttributeKind::kString}, dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"cid", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), EventAttribute{"case:concept:name", "D", AttributeKind::kString}, dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected_input.dtypes = {"str", "string", "datetime64[us, UTC]", "string", "int64", "int64"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {EventAttribute{"source", "custom", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto eager = event_table_to_event_log(input, options); check(eager, eager.traces.size());
  const auto lazy = lazy_event_table_to_event_log(LazyEventTable::from_table(input), options); check(lazy, lazy.traces.size());
  check_format_table("custom_formatted_log: input after", input, expected_input);
}
inline void check_metadata_case_13() {
  LogConversionOptions options; options.case_id_key = "cid";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {EventAttribute{"source", "custom", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 3) throw std::runtime_error("custom_formatted_stream: metadata/length differs"); };
  EventTable input{{"cid", "concept:name", "time:timestamp", "case:concept:name", "@@index", "@@case_index"}, {
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"case:concept:name", "C", AttributeKind::kString}, dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00"), EventAttribute{"case:concept:name", "C", AttributeKind::kString}, dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"cid", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), EventAttribute{"case:concept:name", "D", AttributeKind::kString}, dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{1})}}};
  input.dtypes = {"str", "string", "datetime64[us, UTC]", "string", "int64", "int64"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {EventAttribute{"source", "custom", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  EventTable expected_input{{"cid", "concept:name", "time:timestamp", "case:concept:name", "@@index", "@@case_index"}, {
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"case:concept:name", "C", AttributeKind::kString}, dtype_number("@@index", std::int64_t{0}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"cid", "C", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-03T00:00:00+00:00"), EventAttribute{"case:concept:name", "C", AttributeKind::kString}, dtype_number("@@index", std::int64_t{1}), dtype_number("@@case_index", std::int64_t{0})},
{EventAttribute{"cid", "D", AttributeKind::kString}, EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-02T00:00:00+00:00"), EventAttribute{"case:concept:name", "D", AttributeKind::kString}, dtype_number("@@index", std::int64_t{2}), dtype_number("@@case_index", std::int64_t{1})}}};
  expected_input.dtypes = {"str", "string", "datetime64[us, UTC]", "string", "int64", "int64"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {EventAttribute{"source", "custom", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto eager = event_table_to_event_stream(input, options); check(eager, eager.events.size());
  const auto lazy = lazy_event_table_to_event_stream(LazyEventTable::from_table(input), options); check(lazy, lazy.events.size());
  check_format_table("custom_formatted_stream: input after", input, expected_input);
}
inline void check_metadata_case_14() {
  LogConversionOptions options; options.case_id_key = "case:concept:name";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 0) throw std::runtime_error("empty_log: metadata/length differs"); };
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
}};
  input.dtypes = {"str", "str", "datetime64[s, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
}};
  expected_input.dtypes = {"str", "str", "datetime64[s, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected_input.metadata.properties = {};
  const auto eager = event_table_to_event_log(input, options); check(eager, eager.traces.size());
  const auto lazy = lazy_event_table_to_event_log(LazyEventTable::from_table(input), options); check(lazy, lazy.traces.size());
  check_format_table("empty_log: input after", input, expected_input);
}
inline void check_metadata_case_15() {
  LogConversionOptions options; options.case_id_key = "case:concept:name";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 0) throw std::runtime_error("empty_stream: metadata/length differs"); };
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
}};
  input.dtypes = {"str", "str", "datetime64[s, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
}};
  expected_input.dtypes = {"str", "str", "datetime64[s, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected_input.metadata.properties = {};
  const auto eager = event_table_to_event_stream(input, options); check(eager, eager.events.size());
  const auto lazy = lazy_event_table_to_event_stream(LazyEventTable::from_table(input), options); check(lazy, lazy.events.size());
  check_format_table("empty_stream: input after", input, expected_input);
}
inline void check_metadata_case_16() {
  LogConversionOptions options; options.case_id_key = "case:concept:name";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {dtype_number("retained", false)};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 0) throw std::runtime_error("empty_attrs_log: metadata/length differs"); };
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
}};
  input.dtypes = {"str", "str", "datetime64[s, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  input.metadata.properties = {EventAttribute{"pm4py:param:case_id_key", "x", AttributeKind::kString}, dtype_number("retained", false)};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
}};
  expected_input.dtypes = {"str", "str", "datetime64[s, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected_input.metadata.properties = {EventAttribute{"pm4py:param:case_id_key", "x", AttributeKind::kString}, dtype_number("retained", false)};
  const auto eager = event_table_to_event_log(input, options); check(eager, eager.traces.size());
  const auto lazy = lazy_event_table_to_event_log(LazyEventTable::from_table(input), options); check(lazy, lazy.traces.size());
  check_format_table("empty_attrs_log: input after", input, expected_input);
}
inline void check_metadata_case_17() {
  LogConversionOptions options; options.case_id_key = "case:concept:name";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {dtype_number("retained", false)};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 0) throw std::runtime_error("empty_attrs_stream: metadata/length differs"); };
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
}};
  input.dtypes = {"str", "str", "datetime64[s, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  input.metadata.properties = {EventAttribute{"pm4py:param:case_id_key", "x", AttributeKind::kString}, dtype_number("retained", false)};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
}};
  expected_input.dtypes = {"str", "str", "datetime64[s, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected_input.metadata.properties = {EventAttribute{"pm4py:param:case_id_key", "x", AttributeKind::kString}, dtype_number("retained", false)};
  const auto eager = event_table_to_event_stream(input, options); check(eager, eager.events.size());
  const auto lazy = lazy_event_table_to_event_stream(LazyEventTable::from_table(input), options); check(lazy, lazy.events.size());
  check_format_table("empty_attrs_stream: input after", input, expected_input);
}
inline void check_metadata_case_18() {
  LogConversionOptions options; options.case_id_key = "case:concept:name";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {EventAttribute{"source", "empty", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 0) throw std::runtime_error("empty_formatted_log: metadata/length differs"); };
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
}};
  input.dtypes = {"string", "string", "datetime64[s, UTC]", "int64", "int64"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  input.metadata.properties = {EventAttribute{"source", "empty", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
}};
  expected_input.dtypes = {"string", "string", "datetime64[s, UTC]", "int64", "int64"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected_input.metadata.properties = {EventAttribute{"source", "empty", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto eager = event_table_to_event_log(input, options); check(eager, eager.traces.size());
  const auto lazy = lazy_event_table_to_event_log(LazyEventTable::from_table(input), options); check(lazy, lazy.traces.size());
  check_format_table("empty_formatted_log: input after", input, expected_input);
}
inline void check_metadata_case_19() {
  LogConversionOptions options; options.case_id_key = "case:concept:name";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {EventAttribute{"source", "empty", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}};
  expected_metadata.extensions = {{"Concept", "concept", "http://www.xes-standard.org/concept.xesext"}, {"Time", "time", "http://www.xes-standard.org/time.xesext"}};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "csv", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 0) throw std::runtime_error("empty_formatted_stream: metadata/length differs"); };
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
}};
  input.dtypes = {"string", "string", "datetime64[s, UTC]", "int64", "int64"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  input.metadata.properties = {EventAttribute{"source", "empty", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "@@index", "@@case_index"}, {
}};
  expected_input.dtypes = {"string", "string", "datetime64[s, UTC]", "int64", "int64"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected_input.metadata.properties = {EventAttribute{"source", "empty", AttributeKind::kString}, EventAttribute{"pm4py:param:activity_key", "concept:name", AttributeKind::kString}, EventAttribute{"pm4py:param:timestamp_key", "time:timestamp", AttributeKind::kString}, EventAttribute{"pm4py:param:group_key", "org:group", AttributeKind::kString}, EventAttribute{"pm4py:param:transition_key", "lifecycle:transition", AttributeKind::kString}, EventAttribute{"pm4py:param:resource_key", "org:resource", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "case:concept:name", AttributeKind::kString}};
  const auto eager = event_table_to_event_stream(input, options); check(eager, eager.events.size());
  const auto lazy = lazy_event_table_to_event_stream(LazyEventTable::from_table(input), options); check(lazy, lazy.events.size());
  check_format_table("empty_formatted_stream: input after", input, expected_input);
}
inline void check_metadata_case_20() {
  LogConversionOptions options; options.case_id_key = "case:concept:name";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {EventAttribute{"case_id_key", "ordinary", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "preserved", AttributeKind::kString}, format_container("nested", AttributeContainerKind::kContainer, {dtype_number("enabled", true)})};
  expected_metadata.extensions = {};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "custom", AttributeKind::kString}, EventAttribute{"source", "stream", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 1) throw std::runtime_error("existing_stream_log: metadata/length differs"); };
  EventStream input; input.attributes = {EventAttribute{"origin", "custom", AttributeKind::kString}, EventAttribute{"source", "stream", AttributeKind::kString}};
  EventDataMetadata input_metadata;
  input_metadata.properties = {EventAttribute{"case_id_key", "ordinary", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "preserved", AttributeKind::kString}, format_container("nested", AttributeContainerKind::kContainer, {dtype_number("enabled", true)})};
  input_metadata.extensions = {};
  input.metadata = input_metadata; input.events = {{{EventAttribute{"case:concept:name", "C"}, EventAttribute{"concept:name", "A"}}}};
  const auto actual = event_data_to_event_log(EventDataView(input), options); check(actual, actual.traces.size());
  if (input.metadata != input_metadata || input.attributes != expected_attributes) throw std::runtime_error("stream input changed");
}
inline void check_metadata_case_21() {
  LogConversionOptions options; options.case_id_key = "case:concept:name";
  EventDataMetadata expected_metadata;
  expected_metadata.properties = {EventAttribute{"case_id_key", "ordinary", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "preserved", AttributeKind::kString}, format_container("nested", AttributeContainerKind::kContainer, {dtype_number("enabled", true)})};
  expected_metadata.extensions = {};
  const std::vector<EventAttribute> expected_attributes = {EventAttribute{"origin", "custom", AttributeKind::kString}, EventAttribute{"source", "stream", AttributeKind::kString}};
  const auto check = [&](const auto& actual, std::size_t length) { if (actual.metadata != expected_metadata || actual.attributes != expected_attributes || length != 1) throw std::runtime_error("existing_stream_stream: metadata/length differs"); };
  EventStream input; input.attributes = {EventAttribute{"origin", "custom", AttributeKind::kString}, EventAttribute{"source", "stream", AttributeKind::kString}};
  EventDataMetadata input_metadata;
  input_metadata.properties = {EventAttribute{"case_id_key", "ordinary", AttributeKind::kString}, EventAttribute{"pm4py:param:case_id_key", "preserved", AttributeKind::kString}, format_container("nested", AttributeContainerKind::kContainer, {dtype_number("enabled", true)})};
  input_metadata.extensions = {};
  input.metadata = input_metadata; input.events = {{{EventAttribute{"case:concept:name", "C"}, EventAttribute{"concept:name", "A"}}}};
  const auto actual = event_data_to_event_stream(EventDataView(input), options); check(actual, actual.events.size());
  if (input.metadata != input_metadata || input.attributes != expected_attributes) throw std::runtime_error("stream input changed");
}
}
inline void check_public_dataframe_conversion_metadata() {
  dataframe_conversion_metadata_reference::check_metadata_case_0();
  dataframe_conversion_metadata_reference::check_metadata_case_1();
  dataframe_conversion_metadata_reference::check_metadata_case_2();
  dataframe_conversion_metadata_reference::check_metadata_case_3();
  dataframe_conversion_metadata_reference::check_metadata_case_4();
  dataframe_conversion_metadata_reference::check_metadata_case_5();
  dataframe_conversion_metadata_reference::check_metadata_case_6();
  dataframe_conversion_metadata_reference::check_metadata_case_7();
  dataframe_conversion_metadata_reference::check_metadata_case_8();
  dataframe_conversion_metadata_reference::check_metadata_case_9();
  dataframe_conversion_metadata_reference::check_metadata_case_10();
  dataframe_conversion_metadata_reference::check_metadata_case_11();
  dataframe_conversion_metadata_reference::check_metadata_case_12();
  dataframe_conversion_metadata_reference::check_metadata_case_13();
  dataframe_conversion_metadata_reference::check_metadata_case_14();
  dataframe_conversion_metadata_reference::check_metadata_case_15();
  dataframe_conversion_metadata_reference::check_metadata_case_16();
  dataframe_conversion_metadata_reference::check_metadata_case_17();
  dataframe_conversion_metadata_reference::check_metadata_case_18();
  dataframe_conversion_metadata_reference::check_metadata_case_19();
  dataframe_conversion_metadata_reference::check_metadata_case_20();
  dataframe_conversion_metadata_reference::check_metadata_case_21();
}
