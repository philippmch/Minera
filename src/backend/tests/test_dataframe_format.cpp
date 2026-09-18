#undef NDEBUG

#include "minera/dataframe_format.hpp"
#include "dataframe_format_reference_cases.hpp"

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

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

}  // namespace

int main() try {
  check_public_format_dataframe_reference_cases();
  exact_unsigned_identifier_projection();
  null_timestamp_is_dropped_and_start_can_be_disabled();
  nan_required_values_are_missing();
  missing_selector_does_not_mutate_input();
} catch (const std::exception& error) {
  return (std::fputs(error.what(), stderr), 1);
}
