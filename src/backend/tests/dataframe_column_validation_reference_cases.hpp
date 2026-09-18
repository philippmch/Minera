// Validation report SHA256: 2d99fffb2ed3572112323753fa33660c3977f88f26a8b37ea4f2ff125927c9ac
// Replay report SHA256: 974c33f7e4e5cc5fe017118163d88b7ef26026b7d82060ce2df62226e4fa459d
#include "minera/dataframe_validation.hpp"
#include "minera/conformance.hpp"
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
namespace dataframe_validation_reference {
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
inline void check_validation_0() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "") throw std::runtime_error("valid: validation outcome differs: " + error);
  check_format_table("valid: input after", input, expected_input);
}
inline void check_validation_1() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "") throw std::runtime_error("valid_no_selectors: validation outcome differs: " + error);
  check_format_table("valid_no_selectors: input after", input, expected_input);
}
inline void check_validation_2() {
  EventTable input{{}, {
}};
  input.dtypes = {};
  input.categorical_dtypes = {};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  input.metadata.properties = {};
  EventTable expected_input{{}, {
}};
  expected_input.dtypes = {};
  expected_input.categorical_dtypes = {};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column for the case identifier, a column for the activity and a column for the timestamp.") throw std::runtime_error("empty_no_columns: validation outcome differs: " + error);
  check_format_table("empty_no_columns: input after", input, expected_input);
}
inline void check_validation_3() {
  EventTable input{{"case:concept:name", "concept:name"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}}}};
  input.dtypes = {"str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column for the case identifier, a column for the activity and a column for the timestamp.") throw std::runtime_error("two_strings_only: validation outcome differs: " + error);
  check_format_table("two_strings_only: input after", input, expected_input);
}
inline void check_validation_4() {
  EventTable input{{"case:concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column for the case identifier, a column for the activity and a column for the timestamp.") throw std::runtime_error("one_string_date: validation outcome differs: " + error);
  check_format_table("one_string_date: input after", input, expected_input);
}
inline void check_validation_5() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "x", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "x", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "") throw std::runtime_error("case_object: validation outcome differs: " + error);
  check_format_table("case_object: input after", input, expected_input);
}
inline void check_validation_6() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "x", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "x", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "") throw std::runtime_error("case_str: validation outcome differs: " + error);
  check_format_table("case_str: input after", input, expected_input);
}
inline void check_validation_7() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "x", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"string", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "x", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"string", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "") throw std::runtime_error("case_string: validation outcome differs: " + error);
  check_format_table("case_string: input after", input, expected_input);
}
inline void check_validation_8() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"int64", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"int64", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity.") throw std::runtime_error("case_int64: validation outcome differs: " + error);
  check_format_table("case_int64: input after", input, expected_input);
}
inline void check_validation_9() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"float64", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"float64", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity.") throw std::runtime_error("case_float64: validation outcome differs: " + error);
  check_format_table("case_float64: input after", input, expected_input);
}
inline void check_validation_10() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"bool", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"bool", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity.") throw std::runtime_error("case_bool: validation outcome differs: " + error);
  check_format_table("case_bool: input after", input, expected_input);
}
inline void check_validation_11() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "x", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"category", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {EventTableCategoricalDtype{"str", {EventAttribute{"", "x", AttributeKind::kString}}, false}, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "x", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"category", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {EventTableCategoricalDtype{"str", {EventAttribute{"", "x", AttributeKind::kString}}, false}, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity.") throw std::runtime_error("case_category: validation outcome differs: " + error);
  check_format_table("case_category: input after", input, expected_input);
}
inline void check_validation_12() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_null("case:concept:name"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_null("case:concept:name"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the case ID column should not contain any empty value.") throw std::runtime_error("case_none: validation outcome differs: " + error);
  check_format_table("case_none: input after", input, expected_input);
}
inline void check_validation_13() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_null("case:concept:name"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_null("case:concept:name"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the case ID column should not contain any empty value.") throw std::runtime_error("case_nan: validation outcome differs: " + error);
  check_format_table("case_nan: input after", input, expected_input);
}
inline void check_validation_14() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "") throw std::runtime_error("case_empty: validation outcome differs: " + error);
  check_format_table("case_empty: input after", input, expected_input);
}
inline void check_validation_15() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", " ", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", " ", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "") throw std::runtime_error("case_space: validation outcome differs: " + error);
  check_format_table("case_space: input after", input, expected_input);
}
inline void check_validation_16() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "x", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "object", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "x", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "object", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "") throw std::runtime_error("activity_object: validation outcome differs: " + error);
  check_format_table("activity_object: input after", input, expected_input);
}
inline void check_validation_17() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "x", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "x", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "") throw std::runtime_error("activity_str: validation outcome differs: " + error);
  check_format_table("activity_str: input after", input, expected_input);
}
inline void check_validation_18() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "x", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "string", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "x", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "string", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "") throw std::runtime_error("activity_string: validation outcome differs: " + error);
  check_format_table("activity_string: input after", input, expected_input);
}
inline void check_validation_19() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, dtype_number("concept:name", std::int64_t{1}), reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "int64", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, dtype_number("concept:name", std::int64_t{1}), reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "int64", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity.") throw std::runtime_error("activity_int64: validation outcome differs: " + error);
  check_format_table("activity_int64: input after", input, expected_input);
}
inline void check_validation_20() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, dtype_number("concept:name", double{1.0}), reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "float64", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, dtype_number("concept:name", double{1.0}), reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "float64", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity.") throw std::runtime_error("activity_float64: validation outcome differs: " + error);
  check_format_table("activity_float64: input after", input, expected_input);
}
inline void check_validation_21() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, dtype_number("concept:name", true), reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "bool", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, dtype_number("concept:name", true), reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "bool", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity.") throw std::runtime_error("activity_bool: validation outcome differs: " + error);
  check_format_table("activity_bool: input after", input, expected_input);
}
inline void check_validation_22() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "x", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "category", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, EventTableCategoricalDtype{"str", {EventAttribute{"", "x", AttributeKind::kString}}, false}, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "x", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "category", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, EventTableCategoricalDtype{"str", {EventAttribute{"", "x", AttributeKind::kString}}, false}, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type string for the case identifier and a column of type string for the activity.") throw std::runtime_error("activity_category: validation outcome differs: " + error);
  check_format_table("activity_category: input after", input, expected_input);
}
inline void check_validation_23() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, reference_null("concept:name"), reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "object", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, reference_null("concept:name"), reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "object", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the activity column should not contain any empty value.") throw std::runtime_error("activity_none: validation outcome differs: " + error);
  check_format_table("activity_none: input after", input, expected_input);
}
inline void check_validation_24() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, reference_null("concept:name"), reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "object", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, reference_null("concept:name"), reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "object", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the activity column should not contain any empty value.") throw std::runtime_error("activity_nan: validation outcome differs: " + error);
  check_format_table("activity_nan: input after", input, expected_input);
}
inline void check_validation_25() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "object", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "object", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "") throw std::runtime_error("activity_empty: validation outcome differs: " + error);
  check_format_table("activity_empty: input after", input, expected_input);
}
inline void check_validation_26() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", " ", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "object", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", " ", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "object", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "") throw std::runtime_error("activity_space: validation outcome differs: " + error);
  check_format_table("activity_space: input after", input, expected_input);
}
inline void check_validation_27() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "x", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "x", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type date") throw std::runtime_error("timestamp_object: validation outcome differs: " + error);
  check_format_table("timestamp_object: input after", input, expected_input);
}
inline void check_validation_28() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "x", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "x", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type date") throw std::runtime_error("timestamp_str: validation outcome differs: " + error);
  check_format_table("timestamp_str: input after", input, expected_input);
}
inline void check_validation_29() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "x", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "string"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "x", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "string"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type date") throw std::runtime_error("timestamp_string: validation outcome differs: " + error);
  check_format_table("timestamp_string: input after", input, expected_input);
}
inline void check_validation_30() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})}}};
  input.dtypes = {"str", "str", "int64"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1})}}};
  expected_input.dtypes = {"str", "str", "int64"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type date") throw std::runtime_error("timestamp_int64: validation outcome differs: " + error);
  check_format_table("timestamp_int64: input after", input, expected_input);
}
inline void check_validation_31() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})}}};
  input.dtypes = {"str", "str", "float64"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, dtype_number("time:timestamp", double{1.0})}}};
  expected_input.dtypes = {"str", "str", "float64"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type date") throw std::runtime_error("timestamp_float64: validation outcome differs: " + error);
  check_format_table("timestamp_float64: input after", input, expected_input);
}
inline void check_validation_32() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, dtype_number("time:timestamp", true)}}};
  input.dtypes = {"str", "str", "bool"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, dtype_number("time:timestamp", true)}}};
  expected_input.dtypes = {"str", "str", "bool"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type date") throw std::runtime_error("timestamp_bool: validation outcome differs: " + error);
  check_format_table("timestamp_bool: input after", input, expected_input);
}
inline void check_validation_33() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "x", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "category"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, EventTableCategoricalDtype{"str", {EventAttribute{"", "x", AttributeKind::kString}}, false}};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "x", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "category"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, EventTableCategoricalDtype{"str", {EventAttribute{"", "x", AttributeKind::kString}}, false}};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type date") throw std::runtime_error("timestamp_category: validation outcome differs: " + error);
  check_format_table("timestamp_category: input after", input, expected_input);
}
inline void check_validation_34() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_null("time:timestamp")}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_null("time:timestamp")}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type date") throw std::runtime_error("timestamp_none: validation outcome differs: " + error);
  check_format_table("timestamp_none: input after", input, expected_input);
}
inline void check_validation_35() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_null("time:timestamp")}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_null("time:timestamp")}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type date") throw std::runtime_error("timestamp_nan: validation outcome differs: " + error);
  check_format_table("timestamp_nan: input after", input, expected_input);
}
inline void check_validation_36() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type date") throw std::runtime_error("timestamp_empty: validation outcome differs: " + error);
  check_format_table("timestamp_empty: input after", input, expected_input);
}
inline void check_validation_37() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", " ", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "object"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", " ", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "object"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the dataframe should (at least) contain a column of type date") throw std::runtime_error("timestamp_space: validation outcome differs: " + error);
  check_format_table("timestamp_space: input after", input, expected_input);
}
inline void check_validation_38() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "missing";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the specified case ID column is not contained in the dataframe. Available columns: ['case:concept:name', 'concept:name', 'time:timestamp']") throw std::runtime_error("missing_case_id_key: validation outcome differs: " + error);
  check_format_table("missing_case_id_key: input after", input, expected_input);
}
inline void check_validation_39() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "missing";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the specified activity column is not contained in the dataframe. Available columns: ['case:concept:name', 'concept:name', 'time:timestamp']") throw std::runtime_error("missing_activity_key: validation outcome differs: " + error);
  check_format_table("missing_activity_key: input after", input, expected_input);
}
inline void check_validation_40() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "missing";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the specified timestamp column is not contained in the dataframe. Available columns: ['case:concept:name', 'concept:name', 'time:timestamp']") throw std::runtime_error("missing_timestamp_key: validation outcome differs: " + error);
  check_format_table("missing_timestamp_key: input after", input, expected_input);
}
inline void check_validation_41() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  input.dtypes = {"int64", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  expected_input.dtypes = {"int64", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the case ID column should be of type string.") throw std::runtime_error("selected_case_int64: validation outcome differs: " + error);
  check_format_table("selected_case_int64: input after", input, expected_input);
}
inline void check_validation_42() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "x", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  input.dtypes = {"category", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {EventTableCategoricalDtype{"str", {EventAttribute{"", "x", AttributeKind::kString}}, false}, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "x", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  expected_input.dtypes = {"category", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {EventTableCategoricalDtype{"str", {EventAttribute{"", "x", AttributeKind::kString}}, false}, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the case ID column should be of type string.") throw std::runtime_error("selected_case_category: validation outcome differs: " + error);
  check_format_table("selected_case_category: input after", input, expected_input);
}
inline void check_validation_43() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{reference_null("case:concept:name"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{reference_null("case:concept:name"), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the case ID column should not contain any empty value.") throw std::runtime_error("selected_case_object: validation outcome differs: " + error);
  check_format_table("selected_case_object: input after", input, expected_input);
}
inline void check_validation_44() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, dtype_number("concept:name", std::int64_t{1}), reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  input.dtypes = {"str", "int64", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, dtype_number("concept:name", std::int64_t{1}), reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "int64", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the activity column should be of type string.") throw std::runtime_error("selected_activity_int64: validation outcome differs: " + error);
  check_format_table("selected_activity_int64: input after", input, expected_input);
}
inline void check_validation_45() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "x", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  input.dtypes = {"str", "category", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, EventTableCategoricalDtype{"str", {EventAttribute{"", "x", AttributeKind::kString}}, false}, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "x", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "category", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, EventTableCategoricalDtype{"str", {EventAttribute{"", "x", AttributeKind::kString}}, false}, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the activity column should be of type string.") throw std::runtime_error("selected_activity_category: validation outcome differs: " + error);
  check_format_table("selected_activity_category: input after", input, expected_input);
}
inline void check_validation_46() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, reference_null("concept:name"), reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  input.dtypes = {"str", "object", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, reference_null("concept:name"), reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "object", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the activity column should not contain any empty value.") throw std::runtime_error("selected_activity_object: validation outcome differs: " + error);
  check_format_table("selected_activity_object: input after", input, expected_input);
}
inline void check_validation_47() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1}), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "int64", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, dtype_number("time:timestamp", std::int64_t{1}), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "int64", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the timestamp column should be of time datetime. Use the function pandas.to_datetime") throw std::runtime_error("selected_timestamp_int64: validation outcome differs: " + error);
  check_format_table("selected_timestamp_int64: input after", input, expected_input);
}
inline void check_validation_48() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "x", AttributeKind::kString}, EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "category", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, EventTableCategoricalDtype{"str", {EventAttribute{"", "x", AttributeKind::kString}}, false}, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, EventAttribute{"time:timestamp", "x", AttributeKind::kString}, EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "category", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, EventTableCategoricalDtype{"str", {EventAttribute{"", "x", AttributeKind::kString}}, false}, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the timestamp column should be of time datetime. Use the function pandas.to_datetime") throw std::runtime_error("selected_timestamp_category: validation outcome differs: " + error);
  check_format_table("selected_timestamp_category: input after", input, expected_input);
}
inline void check_validation_49() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_null("time:timestamp"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_null("time:timestamp"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the timestamp column should be of time datetime. Use the function pandas.to_datetime") throw std::runtime_error("selected_timestamp_object: validation outcome differs: " + error);
  check_format_table("selected_timestamp_object: input after", input, expected_input);
}
inline void check_validation_50() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_null("time:timestamp"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[s]", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_null("time:timestamp"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[s]", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the timestamp column should not contain any empty value.") throw std::runtime_error("selected_timestamp_nat: validation outcome differs: " + error);
  check_format_table("selected_timestamp_nat: input after", input, expected_input);
}
inline void check_validation_51() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us]", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us]", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "") throw std::runtime_error("selected_timestamp_valid: validation outcome differs: " + error);
  check_format_table("selected_timestamp_valid: input after", input, expected_input);
}
inline void check_validation_52() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date", "start"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00"), reference_date("start", "2024-01-01T00:00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]", "datetime64[us]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date", "start"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00"), reference_date("start", "2024-01-01T00:00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]", "datetime64[us]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  options.start_timestamp_key = "start";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "") throw std::runtime_error("start_valid: validation outcome differs: " + error);
  check_format_table("start_valid: input after", input, expected_input);
}
inline void check_validation_53() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date", "start"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00"), reference_null("start")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]", "datetime64[s]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date", "start"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00"), reference_null("start")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]", "datetime64[s]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  options.start_timestamp_key = "start";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the start timestamp column should not contain any empty value.") throw std::runtime_error("start_null: validation outcome differs: " + error);
  check_format_table("start_null: input after", input, expected_input);
}
inline void check_validation_54() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date", "start"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00"), EventAttribute{"start", "x", AttributeKind::kString}}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]", "str"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date", "start"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00"), EventAttribute{"start", "x", AttributeKind::kString}}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]", "str"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  options.start_timestamp_key = "start";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the start timestamp column should be of time datetime. Use the function pandas.to_datetime") throw std::runtime_error("start_text: validation outcome differs: " + error);
  check_format_table("start_text: input after", input, expected_input);
}
inline void check_validation_55() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  options.start_timestamp_key = "start";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the specified start timestamp column is not contained in the dataframe. Available columns: ['case:concept:name', 'concept:name', 'extra_date', 'extra_string', 'time:timestamp']") throw std::runtime_error("start_absent: validation outcome differs: " + error);
  check_format_table("start_absent: input after", input, expected_input);
}
inline void check_validation_56() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the specified case ID column is not contained in the dataframe. Available columns: ['case:concept:name', 'concept:name', 'extra_date', 'extra_string', 'time:timestamp']") throw std::runtime_error("empty_selector_case_id_key: validation outcome differs: " + error);
  check_format_table("empty_selector_case_id_key: input after", input, expected_input);
}
inline void check_validation_57() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the specified activity column is not contained in the dataframe. Available columns: ['case:concept:name', 'concept:name', 'extra_date', 'extra_string', 'time:timestamp']") throw std::runtime_error("empty_selector_activity_key: validation outcome differs: " + error);
  check_format_table("empty_selector_activity_key: input after", input, expected_input);
}
inline void check_validation_58() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the specified timestamp column is not contained in the dataframe. Available columns: ['case:concept:name', 'concept:name', 'extra_date', 'extra_string', 'time:timestamp']") throw std::runtime_error("empty_selector_timestamp_key: validation outcome differs: " + error);
  check_format_table("empty_selector_timestamp_key: input after", input, expected_input);
}
inline void check_validation_59() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{reference_null("case:concept:name"), reference_null("concept:name"), reference_null("time:timestamp"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  input.dtypes = {"object", "object", "datetime64[s]", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{reference_null("case:concept:name"), reference_null("concept:name"), reference_null("time:timestamp"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "object", "datetime64[s]", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "the case ID column should not contain any empty value.") throw std::runtime_error("all_null: validation outcome differs: " + error);
  check_format_table("all_null: input after", input, expected_input);
}
inline void check_validation_60() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{reference_null("case:concept:name"), reference_null("concept:name"), reference_null("time:timestamp"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  input.dtypes = {"object", "object", "datetime64[s]", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp", "extra_string", "extra_date"}, {
{reference_null("case:concept:name"), reference_null("concept:name"), reference_null("time:timestamp"), EventAttribute{"extra_string", "s", AttributeKind::kString}, reference_date("extra_date", "2024-01-02T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "object", "datetime64[s]", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "") throw std::runtime_error("null_no_selectors: validation outcome differs: " + error);
  check_format_table("null_no_selectors: input after", input, expected_input);
}
inline void check_validation_61() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "") throw std::runtime_error("typed_empty: validation outcome differs: " + error);
  check_format_table("typed_empty: input after", input, expected_input);
}
inline void check_validation_62() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {format_container("nested", AttributeContainerKind::kContainer, {dtype_number("value", true)})};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {format_container("nested", AttributeContainerKind::kContainer, {dtype_number("value", true)})};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "") throw std::runtime_error("attrs_preserved: validation outcome differs: " + error);
  check_format_table("attrs_preserved: input after", input, expected_input);
}
inline void check_validation_63() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "C", AttributeKind::kString}, EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"str", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
  expected_input.metadata.properties = {};
  DataFrameColumnOptions options;
  options.case_id_key = "case:concept:name";
  options.activity_key = "case:concept:name";
  options.timestamp_key = "time:timestamp";
  std::string error; try { check_dataframe_columns(input, options); } catch (const std::exception& caught) { error = caught.what(); }
  if (error != "") throw std::runtime_error("shared_selector: validation outcome differs: " + error);
  check_format_table("shared_selector: input after", input, expected_input);
}
inline void check_validated_replay_0() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 1) throw std::runtime_error("upper: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={0};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("upper: replay diagnostics differ"); }
  check_format_table("upper: input after", input, expected_input);
}
inline void check_validated_replay_1() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 1) throw std::runtime_error("upper_reverse: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={1};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("upper_reverse: replay diagnostics differ"); }
  check_format_table("upper_reverse: input after", input, expected_input);
}
inline void check_validated_replay_2() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 2) throw std::runtime_error("wide: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={1,2};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("wide: replay diagnostics differ"); }
  { const auto& trace=actual[1]; const std::vector<std::size_t> path={0};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("wide: replay diagnostics differ"); }
  check_format_table("wide: input after", input, expected_input);
}
inline void check_validated_replay_3() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 1) throw std::runtime_error("wide_float_first: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={1};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("wide_float_first: replay diagnostics differ"); }
  check_format_table("wide_float_first: input after", input, expected_input);
}
inline void check_validated_replay_4() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 2) throw std::runtime_error("wide_only_int: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={1};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("wide_only_int: replay diagnostics differ"); }
  { const auto& trace=actual[1]; const std::vector<std::size_t> path={0};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("wide_only_int: replay diagnostics differ"); }
  check_format_table("wide_only_int: input after", input, expected_input);
}
inline void check_validated_replay_5() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 2) throw std::runtime_error("wide_float_unrelated: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={2};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("wide_float_unrelated: replay diagnostics differ"); }
  { const auto& trace=actual[1]; const std::vector<std::size_t> path={0};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("wide_float_unrelated: replay diagnostics differ"); }
  check_format_table("wide_float_unrelated: input after", input, expected_input);
}
inline void check_validated_replay_6() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 2) throw std::runtime_error("negative: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={1};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("negative: replay diagnostics differ"); }
  { const auto& trace=actual[1]; const std::vector<std::size_t> path={2};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("negative: replay diagnostics differ"); }
  check_format_table("negative: input after", input, expected_input);
}
inline void check_validated_replay_7() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 2) throw std::runtime_error("uint_only: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={1};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("uint_only: replay diagnostics differ"); }
  { const auto& trace=actual[1]; const std::vector<std::size_t> path={0};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("uint_only: replay diagnostics differ"); }
  check_format_table("uint_only: input after", input, expected_input);
}
inline void check_validated_replay_8() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 2) throw std::runtime_error("uint_float: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={2};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("uint_float: replay diagnostics differ"); }
  { const auto& trace=actual[1]; const std::vector<std::size_t> path={0};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("uint_float: replay diagnostics differ"); }
  check_format_table("uint_float: input after", input, expected_input);
}
inline void check_validated_replay_9() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 4) throw std::runtime_error("bool_suppresses: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={2};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("bool_suppresses: replay diagnostics differ"); }
  { const auto& trace=actual[1]; const std::vector<std::size_t> path={3};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("bool_suppresses: replay diagnostics differ"); }
  { const auto& trace=actual[2]; const std::vector<std::size_t> path={1};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("bool_suppresses: replay diagnostics differ"); }
  { const auto& trace=actual[3]; const std::vector<std::size_t> path={0};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("bool_suppresses: replay diagnostics differ"); }
  check_format_table("bool_suppresses: input after", input, expected_input);
}
inline void check_validated_replay_10() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 4) throw std::runtime_error("string_suppresses: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={2};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("string_suppresses: replay diagnostics differ"); }
  { const auto& trace=actual[1]; const std::vector<std::size_t> path={1};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("string_suppresses: replay diagnostics differ"); }
  { const auto& trace=actual[2]; const std::vector<std::size_t> path={0};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("string_suppresses: replay diagnostics differ"); }
  { const auto& trace=actual[3]; const std::vector<std::size_t> path={3};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("string_suppresses: replay diagnostics differ"); }
  check_format_table("string_suppresses: input after", input, expected_input);
}
inline void check_validated_replay_11() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 1) throw std::runtime_error("upper_repeat: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={0,2};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("upper_repeat: replay diagnostics differ"); }
  check_format_table("upper_repeat: input after", input, expected_input);
}
inline void check_validated_replay_12() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 3) throw std::runtime_error("signed_unsigned: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={0};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("signed_unsigned: replay diagnostics differ"); }
  { const auto& trace=actual[1]; const std::vector<std::size_t> path={2};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("signed_unsigned: replay diagnostics differ"); }
  { const auto& trace=actual[2]; const std::vector<std::size_t> path={1};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("signed_unsigned: replay diagnostics differ"); }
  check_format_table("signed_unsigned: input after", input, expected_input);
}
inline void check_validated_replay_13() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 4) throw std::runtime_error("bool_first: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={4};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("bool_first: replay diagnostics differ"); }
  { const auto& trace=actual[1]; const std::vector<std::size_t> path={0,1};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("bool_first: replay diagnostics differ"); }
  { const auto& trace=actual[2]; const std::vector<std::size_t> path={3};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("bool_first: replay diagnostics differ"); }
  { const auto& trace=actual[3]; const std::vector<std::size_t> path={2};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("bool_first: replay diagnostics differ"); }
  check_format_table("bool_first: input after", input, expected_input);
}
inline void check_validated_replay_14() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 3) throw std::runtime_error("float_before_bool: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={4};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("float_before_bool: replay diagnostics differ"); }
  { const auto& trace=actual[1]; const std::vector<std::size_t> path={0,1};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("float_before_bool: replay diagnostics differ"); }
  { const auto& trace=actual[2]; const std::vector<std::size_t> path={2};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("float_before_bool: replay diagnostics differ"); }
  check_format_table("float_before_bool: input after", input, expected_input);
}
inline void check_validated_replay_15() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 3) throw std::runtime_error("int_before_float: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={0,1};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("int_before_float: replay diagnostics differ"); }
  { const auto& trace=actual[1]; const std::vector<std::size_t> path={3};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("int_before_float: replay diagnostics differ"); }
  { const auto& trace=actual[2]; const std::vector<std::size_t> path={2};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("int_before_float: replay diagnostics differ"); }
  check_format_table("int_before_float: input after", input, expected_input);
}
inline void check_validated_replay_16() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  std::string error; try { (void)replay_dataframe_traces(input,net); } catch (const std::exception& caught) { error=caught.what(); }
  if (error != "the case ID column should not contain any empty value.") throw std::runtime_error("nan_keys: validated replay rejection differs");
  check_format_table("nan_keys: input after", input, expected_input);
}
inline void check_validated_replay_17() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  std::string error; try { (void)replay_dataframe_traces(input,net); } catch (const std::exception& caught) { error=caught.what(); }
  if (error != "the case ID column should not contain any empty value.") throw std::runtime_error("none_keys: validated replay rejection differs");
  check_format_table("none_keys: input after", input, expected_input);
}
inline void check_validated_replay_18() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 3) throw std::runtime_error("infinity: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={1};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("infinity: replay diagnostics differ"); }
  { const auto& trace=actual[1]; const std::vector<std::size_t> path={2};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("infinity: replay diagnostics differ"); }
  { const auto& trace=actual[2]; const std::vector<std::size_t> path={0};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("infinity: replay diagnostics differ"); }
  check_format_table("infinity: input after", input, expected_input);
}
inline void check_validated_replay_19() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 1) throw std::runtime_error("float_first_wide_twice: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={1,3};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("float_first_wide_twice: replay diagnostics differ"); }
  check_format_table("float_first_wide_twice: input after", input, expected_input);
}
inline void check_validated_replay_20() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  std::string error; try { (void)replay_dataframe_traces(input,net); } catch (const std::exception& caught) { error=caught.what(); }
  if (error != "the case ID column should not contain any empty value.") throw std::runtime_error("nan_only: validated replay rejection differs");
  check_format_table("nan_only: input after", input, expected_input);
}
inline void check_validated_replay_21() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 1) throw std::runtime_error("prior_equal: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={0,1};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_equal: replay diagnostics differ"); }
  check_format_table("prior_equal: input after", input, expected_input);
}
inline void check_validated_replay_22() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 1) throw std::runtime_error("prior_wide_equal: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={0,1};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_wide_equal: replay diagnostics differ"); }
  check_format_table("prior_wide_equal: input after", input, expected_input);
}
inline void check_validated_replay_23() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 2) throw std::runtime_error("prior_wide_distinct: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={1};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_wide_distinct: replay diagnostics differ"); }
  { const auto& trace=actual[1]; const std::vector<std::size_t> path={0};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_wide_distinct: replay diagnostics differ"); }
  check_format_table("prior_wide_distinct: input after", input, expected_input);
}
inline void check_validated_replay_24() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 2) throw std::runtime_error("prior_wide_mixed: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={1,2};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_wide_mixed: replay diagnostics differ"); }
  { const auto& trace=actual[1]; const std::vector<std::size_t> path={0};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_wide_mixed: replay diagnostics differ"); }
  check_format_table("prior_wide_mixed: input after", input, expected_input);
}
inline void check_validated_replay_25() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 1) throw std::runtime_error("prior_upper: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={0};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_upper: replay diagnostics differ"); }
  check_format_table("prior_upper: input after", input, expected_input);
}
inline void check_validated_replay_26() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 2) throw std::runtime_error("prior_small_fraction: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={1};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_small_fraction: replay diagnostics differ"); }
  { const auto& trace=actual[1]; const std::vector<std::size_t> path={0};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_small_fraction: replay diagnostics differ"); }
  check_format_table("prior_small_fraction: input after", input, expected_input);
}
inline void check_validated_replay_27() {
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
  PetriNet net; const auto p = net.add_place(); net.set_source(p); net.set_sink(p); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"}); for (std::size_t i=0;i<8;++i) net.add_transition(i,{p},{p});
  const auto actual = replay_dataframe_traces(input,net); if (actual.size() != 2) throw std::runtime_error("prior_string_mixed: replay count differs");
  { const auto& trace=actual[0]; const std::vector<std::size_t> path={0,1};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_string_mixed: replay diagnostics differ"); }
  { const auto& trace=actual[1]; const std::vector<std::size_t> path={2};
    if (trace.activated_transitions != path || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_string_mixed: replay diagnostics differ"); }
  check_format_table("prior_string_mixed: input after", input, expected_input);
}
}
inline void check_public_dataframe_validation() {
  dataframe_validation_reference::check_validation_0();
  dataframe_validation_reference::check_validation_1();
  dataframe_validation_reference::check_validation_2();
  dataframe_validation_reference::check_validation_3();
  dataframe_validation_reference::check_validation_4();
  dataframe_validation_reference::check_validation_5();
  dataframe_validation_reference::check_validation_6();
  dataframe_validation_reference::check_validation_7();
  dataframe_validation_reference::check_validation_8();
  dataframe_validation_reference::check_validation_9();
  dataframe_validation_reference::check_validation_10();
  dataframe_validation_reference::check_validation_11();
  dataframe_validation_reference::check_validation_12();
  dataframe_validation_reference::check_validation_13();
  dataframe_validation_reference::check_validation_14();
  dataframe_validation_reference::check_validation_15();
  dataframe_validation_reference::check_validation_16();
  dataframe_validation_reference::check_validation_17();
  dataframe_validation_reference::check_validation_18();
  dataframe_validation_reference::check_validation_19();
  dataframe_validation_reference::check_validation_20();
  dataframe_validation_reference::check_validation_21();
  dataframe_validation_reference::check_validation_22();
  dataframe_validation_reference::check_validation_23();
  dataframe_validation_reference::check_validation_24();
  dataframe_validation_reference::check_validation_25();
  dataframe_validation_reference::check_validation_26();
  dataframe_validation_reference::check_validation_27();
  dataframe_validation_reference::check_validation_28();
  dataframe_validation_reference::check_validation_29();
  dataframe_validation_reference::check_validation_30();
  dataframe_validation_reference::check_validation_31();
  dataframe_validation_reference::check_validation_32();
  dataframe_validation_reference::check_validation_33();
  dataframe_validation_reference::check_validation_34();
  dataframe_validation_reference::check_validation_35();
  dataframe_validation_reference::check_validation_36();
  dataframe_validation_reference::check_validation_37();
  dataframe_validation_reference::check_validation_38();
  dataframe_validation_reference::check_validation_39();
  dataframe_validation_reference::check_validation_40();
  dataframe_validation_reference::check_validation_41();
  dataframe_validation_reference::check_validation_42();
  dataframe_validation_reference::check_validation_43();
  dataframe_validation_reference::check_validation_44();
  dataframe_validation_reference::check_validation_45();
  dataframe_validation_reference::check_validation_46();
  dataframe_validation_reference::check_validation_47();
  dataframe_validation_reference::check_validation_48();
  dataframe_validation_reference::check_validation_49();
  dataframe_validation_reference::check_validation_50();
  dataframe_validation_reference::check_validation_51();
  dataframe_validation_reference::check_validation_52();
  dataframe_validation_reference::check_validation_53();
  dataframe_validation_reference::check_validation_54();
  dataframe_validation_reference::check_validation_55();
  dataframe_validation_reference::check_validation_56();
  dataframe_validation_reference::check_validation_57();
  dataframe_validation_reference::check_validation_58();
  dataframe_validation_reference::check_validation_59();
  dataframe_validation_reference::check_validation_60();
  dataframe_validation_reference::check_validation_61();
  dataframe_validation_reference::check_validation_62();
  dataframe_validation_reference::check_validation_63();
  dataframe_validation_reference::check_validated_replay_0();
  dataframe_validation_reference::check_validated_replay_1();
  dataframe_validation_reference::check_validated_replay_2();
  dataframe_validation_reference::check_validated_replay_3();
  dataframe_validation_reference::check_validated_replay_4();
  dataframe_validation_reference::check_validated_replay_5();
  dataframe_validation_reference::check_validated_replay_6();
  dataframe_validation_reference::check_validated_replay_7();
  dataframe_validation_reference::check_validated_replay_8();
  dataframe_validation_reference::check_validated_replay_9();
  dataframe_validation_reference::check_validated_replay_10();
  dataframe_validation_reference::check_validated_replay_11();
  dataframe_validation_reference::check_validated_replay_12();
  dataframe_validation_reference::check_validated_replay_13();
  dataframe_validation_reference::check_validated_replay_14();
  dataframe_validation_reference::check_validated_replay_15();
  dataframe_validation_reference::check_validated_replay_16();
  dataframe_validation_reference::check_validated_replay_17();
  dataframe_validation_reference::check_validated_replay_18();
  dataframe_validation_reference::check_validated_replay_19();
  dataframe_validation_reference::check_validated_replay_20();
  dataframe_validation_reference::check_validated_replay_21();
  dataframe_validation_reference::check_validated_replay_22();
  dataframe_validation_reference::check_validated_replay_23();
  dataframe_validation_reference::check_validated_replay_24();
  dataframe_validation_reference::check_validated_replay_25();
  dataframe_validation_reference::check_validated_replay_26();
  dataframe_validation_reference::check_validated_replay_27();
}
