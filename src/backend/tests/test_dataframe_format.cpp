#undef NDEBUG

#include "minera/dataframe_format.hpp"
#include "dataframe_format_reference_cases.hpp"

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace minera;

EventAttribute text_cell(std::string key, std::string value) {
  return {std::move(key), std::move(value), AttributeKind::kString};
}

EventAttribute uint_cell(std::string key, std::uint64_t value) {
  EventAttribute result{std::move(key), std::to_string(value),
                        AttributeKind::kInt};
  result.typed_value = value;
  return result;
}

EventAttribute null_cell(std::string key) {
  EventAttribute result{std::move(key), "", AttributeKind::kString, true};
  result.first_class_state = FirstClassAttributeState::kNull;
  return result;
}

EventAttribute nan_cell(std::string key) {
  EventAttribute result{std::move(key), "nan", AttributeKind::kFloat};
  result.typed_value = std::numeric_limits<double>::quiet_NaN();
  return result;
}

EventAttribute native_date_cell(
    std::string key, std::string value, NativeDateResolution resolution,
    std::optional<std::string> timezone = std::nullopt) {
  EventAttribute result{std::move(key), value, AttributeKind::kDate};
  result.typed_value = NativeDate{std::move(value), resolution,
                                  std::move(timezone)};
  return result;
}

EventTable basic_table() {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  table.rows = {{text_cell("case:concept:name", "C"),
                text_cell("concept:name", "A"),
                text_cell("time:timestamp", "2024-01-01")}};
  table.dtypes = {"str", "str", "str"};
  table.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  return table;
}

void exact_unsigned_identifier_projection() {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  table.rows = {{uint_cell("case:concept:name", std::uint64_t{0xFFFFFFFFFFFFFFFFULL}),
                uint_cell("concept:name", std::uint64_t{0x8000000000000000ULL}),
                text_cell("time:timestamp", "2024-01-01")}};
  table.dtypes = {"uint64", "uint64", "str"};
  const auto result = format_dataframe(table);
  assert(result.rows.size() == 1);
  assert(result.rows[0][0]->value == "18446744073709551615");
  assert(result.rows[0][1]->value == "9223372036854775808");
  assert(result.rows[0][0]->kind == AttributeKind::kString);
  assert(result.rows[0][1]->kind == AttributeKind::kString);
}

void null_timestamp_is_dropped_and_start_can_be_disabled() {
  auto table = basic_table();
  table.rows.push_back({text_cell("case:concept:name", "C"),
                        text_cell("concept:name", "B"),
                        null_cell("time:timestamp")});
  DataFrameFormatOptions options;
  options.start_timestamp_key = std::nullopt;
  const auto result = format_dataframe(table, options);
  assert(result.rows.size() == 1);
  assert(result.columns.back() == "@@case_index");
  for (const auto& property : result.metadata.properties)
    assert(property.key != "pm4py:param:start_timestamp_key");
}

void nan_required_values_are_missing() {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  table.rows = {{nan_cell("case:concept:name"),
                text_cell("concept:name", "dropped"),
                text_cell("time:timestamp", "2024-01-01")},
               {text_cell("case:concept:name", "C"),
                text_cell("concept:name", "kept"),
                text_cell("time:timestamp", "2024-01-02")}};
  table.dtypes = {"float64", "str", "str"};
  const auto result = format_dataframe(table);
  assert(result.rows.size() == 1);
  assert(result.rows[0][1]->value == "kept");
}

void missing_selector_does_not_mutate_input() {
  auto table = basic_table();
  const auto before = table;
  DataFrameFormatOptions options;
  options.activity_key = "missing";
  bool rejected = false;
  try {
    (void)format_dataframe(table, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  assert(table.columns == before.columns);
  assert(table.rows == before.rows);
  assert(table.dtypes == before.dtypes);
}

void literal_empty_and_space_selectors_are_formatted_from_their_columns() {
  EventTable input;
  input.columns = {"", " ", "when", "case:concept:name",
                   "concept:name", "time:timestamp"};
  input.rows = {{text_cell("", "selected-case"), text_cell(" ", "A"),
                 text_cell("when", "2024-01-01"),
                 text_cell("case:concept:name", "decoy-case"),
                 text_cell("concept:name", "decoy-activity"),
                 text_cell("time:timestamp", "2019-01-01")}};
  DataFrameFormatOptions options;
  options.case_id.clear();
  options.activity_key = " ";
  options.timestamp_key = "when";
  options.start_timestamp_key = std::nullopt;

  const auto result = format_dataframe(input, options);
  assert(result.rows.size() == 1);
  assert(result.rows[0][result.column_index("case:concept:name")]->value ==
         "selected-case");
  assert(result.rows[0][result.column_index("concept:name")]->value == "A");
  assert(result.rows[0][result.column_index("time:timestamp")]->value ==
         "2024-01-01T00:00:00+00:00");
  assert(result.columns[result.column_index("")].empty());
  assert(result.columns[result.column_index(" ")] == " ");
  assert(input.rows[0][input.column_index("")]->value == "selected-case");
  assert(input.rows[0][input.column_index(" ")]->value == "A");
  assert(input.rows[0][input.column_index("case:concept:name")]->value ==
         "selected-case");
  assert(input.rows[0][input.column_index("concept:name")]->value == "A");
  assert(input.rows[0][input.column_index("time:timestamp")]->value ==
         "2024-01-01");
}

void native_date_resolution_is_preserved_for_object_columns() {
  struct Case {
    const char* value;
    const char* timezone;
    NativeDateResolution resolution;
    const char* dtype;
    const char* expected_value;
  };
  const Case cases[] = {
      {"2024-01-01T00:00:00", nullptr,
       NativeDateResolution::kMicrosecond, "datetime64[us, UTC]",
       "2024-01-01T00:00:00+00:00"},
      {"2024-01-01T00:00:00", nullptr,
       NativeDateResolution::kNanosecond, "datetime64[ns, UTC]",
       "2024-01-01T00:00:00+00:00"},
      {"2024-01-01T00:00:00+00:00", "UTC",
       NativeDateResolution::kMicrosecond, "datetime64[us, UTC]",
       "2024-01-01T00:00:00+00:00"},
      {"2024-01-01T00:00:00+00:00", "UTC",
       NativeDateResolution::kNanosecond, "datetime64[ns, UTC]",
       "2024-01-01T00:00:00+00:00"},
      {"2024-01-01T00:00:00+01:00", "Europe/Berlin",
       NativeDateResolution::kMicrosecond, "datetime64[us, UTC]",
       "2023-12-31T23:00:00+00:00"},
      {"2024-01-01T00:00:00+01:00", "Europe/Berlin",
       NativeDateResolution::kNanosecond, "datetime64[ns, UTC]",
       "2023-12-31T23:00:00+00:00"},
  };
  for (const auto& test : cases) {
    EventTable table;
    table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
    table.rows = {{text_cell("case:concept:name", "C"),
                  text_cell("concept:name", "A"),
                  native_date_cell("time:timestamp", test.value,
                                   test.resolution,
                                   test.timezone ? std::optional<std::string>(test.timezone)
                                                 : std::nullopt)}};
    table.dtypes = {"str", "str", "object"};

    const auto result = format_dataframe(table);
    assert(result.dtypes[2] == test.dtype);
    const auto* date = std::get_if<NativeDate>(&result.rows[0][2]->typed_value);
    assert(date != nullptr);
    assert(date->resolution == test.resolution);
    assert(date->value == test.expected_value);
    assert(date->timezone == std::optional<std::string>("UTC"));
  }
}

void mixed_native_date_resolution_promotes_to_nanoseconds() {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  table.rows = {{text_cell("case:concept:name", "C"),
                text_cell("concept:name", "A"),
                native_date_cell("time:timestamp", "2024-01-01T00:00:00",
                                 NativeDateResolution::kMicrosecond)},
               {text_cell("case:concept:name", "C"),
                text_cell("concept:name", "A"),
                native_date_cell("time:timestamp", "2024-01-02T00:00:00",
                                 NativeDateResolution::kNanosecond)}};
  table.dtypes = {"str", "str", "object"};

  const auto result = format_dataframe(table);
  assert(result.dtypes[2] == "datetime64[ns, UTC]");
  for (const auto& row : result.rows) {
    const auto* date = std::get_if<NativeDate>(&row[2]->typed_value);
    assert(date != nullptr);
    assert(date->resolution == NativeDateResolution::kNanosecond);
  }
}

void undeclared_native_nanosecond_dtype_is_inferred() {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  table.rows = {{text_cell("case:concept:name", "C"),
                text_cell("concept:name", "A"),
                native_date_cell("time:timestamp", "2024-01-01T00:00:00",
                                 NativeDateResolution::kNanosecond)}};

  const auto result = format_dataframe(table);
  assert(result.dtypes[2] == "datetime64[ns, UTC]");
  const auto* date = std::get_if<NativeDate>(&result.rows[0][2]->typed_value);
  assert(date != nullptr);
  assert(date->resolution == NativeDateResolution::kNanosecond);
}

void native_start_timestamp_resolution_is_preserved() {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp",
                   "start"};
  table.rows = {{text_cell("case:concept:name", "C"),
                text_cell("concept:name", "A"),
                native_date_cell("time:timestamp", "2024-01-01T00:00:00",
                                 NativeDateResolution::kMicrosecond),
                native_date_cell("start", "2024-01-01T00:00:00",
                                 NativeDateResolution::kNanosecond)}};
  table.dtypes = {"str", "str", "datetime64[us]", "object"};
  DataFrameFormatOptions options;
  options.start_timestamp_key = "start";

  const auto result = format_dataframe(table, options);
  const auto timestamp = result.column_index("time:timestamp");
  const auto start = result.column_index("start");
  const auto start_timestamp = result.column_index("start_timestamp");
  assert(result.dtypes[timestamp] == "datetime64[us, UTC]");
  assert(result.dtypes[start] == "datetime64[ns, UTC]");
  assert(result.dtypes[start_timestamp] == "datetime64[ns, UTC]");
  for (const auto column : {start, start_timestamp}) {
    const auto* date =
        std::get_if<NativeDate>(&result.rows[0][column]->typed_value);
    assert(date != nullptr);
    assert(date->resolution == NativeDateResolution::kNanosecond);
  }
}

}  // namespace

// Categorical columns are never converted to dates (fresh public comparison):
// a categorical timestamp stays categorical in every projection, as does an
// all-null categorical extra column.
void categorical_columns_are_not_dates() {
  EventTable table;
  table.columns = {"case", "act", "time", "extra"};
  table.rows = {{text_cell("case", "C"), text_cell("act", "A"),
                 text_cell("time", "2020-01-02 08:46:00"), null_cell("extra")}};
  table.dtypes = {"str", "str", "category", "category"};
  const EventTableCategoricalDtype times{"str", {text_cell("", "2020-01-02 08:46:00")}, false};
  table.categorical_dtypes = {std::nullopt, std::nullopt, times,
                              EventTableCategoricalDtype{"float64", {}, false}};
  DataFrameFormatOptions options;
  options.case_id = "case";
  options.activity_key = "act";
  options.timestamp_key = "time";
  options.start_timestamp_key = "time";
  const auto result = format_dataframe(table, options);
  assert(result.rows.size() == 1);
  for (const std::string name : {"time", "extra", "time:timestamp", "start_timestamp"}) {
    std::size_t column = 0;
    while (result.columns[column] != name) ++column;
    assert(result.dtypes[column] == "category");
    assert(result.categorical_dtypes[column].has_value());
    if (name != "extra") assert(result.rows[0][column]->kind == AttributeKind::kString);
  }
}

// Fuzzed against pm4py.format_dataframe on str columns: the default (mixed)
// conversion reads dateutil shapes like the CSV importer, a long ISO fraction
// with a date outside 1677-2262 keeps the column text, and an explicit format
// matches anywhere in a cell (to_datetime exact=False) but never a cell that
// ends before the format does.
void str_timestamps_follow_public_parsing() {
  const auto times = [](std::vector<std::string> cells, std::optional<std::string> format) {
    EventTable table;
    table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
    table.dtypes = {"str", "str", "str"};
    for (std::size_t i = 0; i < cells.size(); ++i)
      table.rows.push_back({text_cell("case:concept:name", std::to_string(i + 1)),
                            text_cell("concept:name", "a"),
                            text_cell("time:timestamp", cells[i])});
    DataFrameFormatOptions options;
    options.timestamp_format = std::move(format);
    const auto result = format_dataframe(table, options);
    std::vector<std::string> out;
    for (const auto& row : result.rows) out.push_back(row[2]->value + "|" + row[0]->value);
    return out;
  };
  using V = std::vector<std::string>;
  assert((times({"01/23/2028 3:17:46 AM", "27-Oct-1992", "2024-01-02t10:00:00.123456789",
                 "01/02/2024 10:00 +15:00"}, std::nullopt) ==
          V{"2028-01-23T03:17:46+00:00|1", "1992-10-27T00:00:00+00:00|2",
            "2024-01-02T10:00:00.123456+00:00|3", "2024-01-01T19:00:00+00:00|4"}));
  assert((times({"1676-08-06T22:42:56.412861583", "2000-08-23 07:25:55"}, std::nullopt) ==
          V{"1676-08-06T22:42:56.412861583|1", "2000-08-23 07:25:55|2"}));
  // The nanosecond fraction follows the seconds even after a '.' date.
  assert((times({"2024.10.09 06:17:12.186788426", "1500-01-01"}, std::nullopt) ==
          V{"2024.10.09 06:17:12.186788426|1", "1500-01-01|2"}));
  assert((times({"2024.10.09 06:17:12.186788", "1500-01-01"}, std::nullopt) ==
          V{"2024-10-09T06:17:12.186788+00:00|1", "1500-01-01T00:00:00+00:00|2"}));
  // Lowercase zone names do not parse, so the column stays text.
  assert((times({"2024-07-05 11:46:53 utc", "2024-01-01"}, std::nullopt) ==
          V{"2024-07-05 11:46:53 utc|1", "2024-01-01|2"}));
  // The case ids stay text: "1" no longer reads as a day with "%d %b %Y".
  assert((times({"21 Nov 2005 09:40", "x 21 Mar 2030 12:50 later"}, "%d %b %Y %H:%M") ==
          V{"2005-11-21T09:40:00+00:00|1", "2030-03-21T12:50:00+00:00|2"}));
  assert((times({"2026-09-17 03:01:40", "2013-02-18 18:53:31+02:00"}, "%Y-%m-%d %H:%M:%S") ==
          V{"2026-09-17T03:01:40+00:00|1", "2013-02-18T18:53:31+00:00|2"}));
  assert((times({"2024/01/01 01:02:03.abc"}, "%Y/%m/%d %H:%M:%S.%f") ==
          V{"2024/01/01 01:02:03.abc|1"}));
}

int main() try {
  check_public_format_dataframe_reference_cases();
  exact_unsigned_identifier_projection();
  null_timestamp_is_dropped_and_start_can_be_disabled();
  nan_required_values_are_missing();
  missing_selector_does_not_mutate_input();
  literal_empty_and_space_selectors_are_formatted_from_their_columns();
  native_date_resolution_is_preserved_for_object_columns();
  mixed_native_date_resolution_promotes_to_nanoseconds();
  undeclared_native_nanosecond_dtype_is_inferred();
  native_start_timestamp_resolution_is_preserved();
  categorical_columns_are_not_dates();
  str_timestamps_follow_public_parsing();
} catch (const std::exception& error) {
  return (std::fputs(error.what(), stderr), 1);
}
