#include "minera/dataframe_format_json.hpp"
#include "minera/log_attribute_repetition_controls.hpp"
#include "minera/log_attribute_repetition_filter.hpp"
#include "minera/pandas_attribute_repetition_filter.hpp"
#include "minera/polars_attribute_repetition_filter.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <cassert>
#include <sstream>
#include <string>
#include <vector>

namespace {
using namespace minera;

// Rows: {case, typed attribute cell JSON}; every case is a string.
EventTable table(const std::string& dtype, const std::vector<std::pair<std::string, std::string>>& rows,
                 const std::string& extra = "") {
  // Polars frames declare String case columns; pandas ones object.
  const bool polars = dtype.front() >= 'A' && dtype.front() <= 'Z';
  std::string json = "{\"columns\":[\"case:concept:name\",\"concept:name\"],\"dtypes\":[\"" +
                     std::string(polars ? "String" : "object") + "\",\"" + dtype +
                     "\"]" + extra + ",\"rows\":[";
  for (std::size_t i = 0; i < rows.size(); ++i) {
    if (i) json += ',';
    json += "[{\"key\":\"case:concept:name\",\"value\":\"" + rows[i].first + "\"}," + rows[i].second + "]";
  }
  return parse_event_table_json(json + "]}");
}

std::string int_cell(const std::string& value, const std::string& storage = "int64") {
  return "{\"key\":\"concept:name\",\"kind\":\"int\",\"value\":\"" + value + "\",\"typed_value\":" + value +
         ",\"integer_storage\":\"" + storage + "\"}";
}

std::string float_cell(const std::string& value) {
  return "{\"key\":\"concept:name\",\"kind\":\"float\",\"value\":\"" + value + "\",\"typed_value\":\"" + value + "\"}";
}

std::string date_cell(const std::string& value, const std::string& zone) {
  return "{\"key\":\"concept:name\",\"kind\":\"date\",\"value\":\"" + value + "\",\"typed_value\":\"" + value +
         "\",\"date_resolution\":\"us\"" + (zone.empty() ? "" : ",\"date_timezone\":\"" + zone + "\"") + "}";
}

EventAttribute target(const std::string& json) { return parse_log_attribute_repetition_target_json(json); }

std::vector<std::string> cases(const EventTable& result) {
  std::vector<std::string> out;
  for (const auto& row : result.rows) out.push_back(row[0]->value);
  return out;
}

template <typename Options>
Options at_least_one() {
  Options options;
  options.minimum = TokenInteger(1);
  return options;
}

void pandas_numeric_columns_compare_weak_python_scalars() {
  // float32 casts a Python float to float32; integers meet floats in float64.
  const auto f32 = table("float32", {{"a", float_cell("0.10000000149011612")}, {"b", float_cell("16777216.0")}});
  const auto options = at_least_one<PandasAttributeRepetitionOptions>();
  assert(cases(filter_pandas_attribute_value_repetition(f32, target(R"({"type":"decimal","value":"0.1"})"), options)) ==
         std::vector<std::string>{"a"});
  assert(cases(filter_pandas_attribute_value_repetition(f32, target(R"({"type":"integer","value":"16777217"})"), options)) ==
         std::vector<std::string>{"b"});
  const auto u64 = table("uint64", {{"a", int_cell("18446744073709551615", "uint64")}, {"b", int_cell("1")}});
  assert(cases(filter_pandas_attribute_value_repetition(u64, target(R"({"type":"decimal","value":"1.8446744073709552e+19"})"), options)) ==
         std::vector<std::string>{"a"});
  assert(filter_pandas_attribute_value_repetition(u64, target(R"({"type":"integer","value":"18446744073709551616"})"), options).rows.empty());
}

void pandas_categorical_columns_compare_like_their_categories() {
  const std::string categories =
      ",\"categorical_dtypes\":[null,{\"categories_dtype\":\"int64\",\"ordered\":false,\"categories\":[" +
      int_cell("1") + "," + int_cell("2") + "]}]";
  const auto source = table("category", {{"a", int_cell("1")}, {"b", int_cell("2")}}, categories);
  const auto options = at_least_one<PandasAttributeRepetitionOptions>();
  assert(filter_pandas_attribute_value_repetition(source, target(R"({"type":"boolean","value":true})"), options).rows.empty());
  assert(cases(filter_pandas_attribute_value_repetition(source, target(R"({"type":"decimal","value":"1.0"})"), options)) ==
         std::vector<std::string>{"a"});
}

void pandas_datetime_columns_localize_naive_strings_to_named_zones() {
  const auto source = table("datetime64[us, Europe/Berlin]",
                            {{"a", date_cell("2021-03-28T01:30:00+01:00", "Europe/Berlin")},
                             {"b", date_cell("2021-10-31T02:30:00+01:00", "Europe/Berlin")}});
  const auto options = at_least_one<PandasAttributeRepetitionOptions>();
  assert(cases(filter_pandas_attribute_value_repetition(source, target(R"({"type":"string","value":"2021-03-28 01:30:00"})"), options)) ==
         std::vector<std::string>{"a"});
  // 02:30 on the autumn transition day is ambiguous and matches nothing.
  assert(filter_pandas_attribute_value_repetition(source, target(R"({"type":"string","value":"2021-10-31 02:30:00"})"), options).rows.empty());
}

void pandas_boolean_columns_reject_integers_beyond_c_long() {
  const auto source = table("bool", {{"a", "{\"key\":\"concept:name\",\"kind\":\"boolean\",\"value\":\"true\",\"typed_value\":true}"}});
  try {
    (void)filter_pandas_attribute_value_repetition(source, target(R"({"type":"integer","value":"18446744073709551615"})"));
    assert(false);
  } catch (const PandasAttributeRepetitionError& error) {
    assert(error.type == "OverflowError" && error.message == "Python int too large to convert to C long");
  }
}

template <typename Call>
PolarsAttributeRepetitionError polars_error(Call call) {
  try { call(); } catch (const PolarsAttributeRepetitionError& error) { return error; }
  assert(false);
  return PolarsAttributeRepetitionError("", "", "");
}

void polars_named_zone_literals_compare_only_with_the_same_zone() {
  const auto berlin = table("Datetime(time_unit='ms', time_zone='Europe/Berlin')",
                            {{"a", date_cell("2021-10-31T03:30:00+01:00", "Europe/Berlin")}});
  const auto options = at_least_one<PolarsAttributeRepetitionOptions>();
  const auto named = target(R"({"type":"date","value":"2021-10-31T03:30:00+01:00","timezone":"Europe/Berlin"})");
  assert(cases(filter_polars_attribute_value_repetition(berlin, named, options)) == std::vector<std::string>{"a"});
  auto missing = options;
  missing.case_id_key = std::string("rd-missing");
  const auto plan = polars_error([&] { (void)filter_polars_attribute_value_repetition(berlin, named, missing); });
  assert(plan.type == "ColumnNotFoundError" &&
         plan.message.find("FILTER col(\"concept:name\") == 2021-10-31 03:30:00 CET\n") != std::string::npos);
  // A fixed offset becomes a UTC literal, which a Berlin column cannot compare.
  const auto fixed = polars_error([&] {
    (void)filter_polars_attribute_value_repetition(berlin, target(R"({"type":"date","value":"2021-10-31T03:30:00+01:00"})"), options);
  });
  assert(fixed.type == "SchemaError" && fixed.phase == "collect" &&
         fixed.message.find("Datetime('\xce\xbcs', 'UTC')") != std::string::npos);
}

void polars_contradictory_integer_bounds_fold_before_comparison() {
  const auto source = table("UInt8", {{"a", int_cell("1")}, {"b", int_cell("2")}});
  const auto list = target(R"({"type":"list","value":[{"type":"integer","value":"1"}]})");
  PolarsAttributeRepetitionOptions folded;
  folded.minimum = TokenInteger(2);
  folded.maximum = false;
  assert(filter_polars_attribute_value_repetition(source, list, folded).rows.empty());
  PolarsAttributeRepetitionOptions open;
  open.minimum = TokenInteger(1);
  const auto error = polars_error([&] { (void)filter_polars_attribute_value_repetition(source, list, open); });
  assert(error.type == "SchemaError" &&
         error.message.find("series 'literal' of dtype: List(Int64)") != std::string::npos);
}

void polars_string_bound_diagnostics_render_simplified_count_comparisons() {
  const auto source = table("Int64", {{"a", int_cell("1")}});
  PolarsAttributeRepetitionOptions options;
  options.minimum = TokenInteger(-1);
  options.maximum = std::string("3");
  const auto error = polars_error([&] {
    (void)filter_polars_attribute_value_repetition(source, target(R"({"type":"integer","value":"1"})"), options);
  });
  assert(error.message ==
         "cannot compare string with numeric type (u32)\n\nThis error occurred in the following expression:\n\t"
         "[(col(\"count\")) <= (\"3\")]\nwhile evaluating this larger expression:\n\t"
         "[(.when(col(\"count\").is_not_null()).then(true).otherwise(null)) & ([(col(\"count\")) <= (\"3\")])]\n");
  options.minimum = 1.5;
  const auto real = polars_error([&] {
    (void)filter_polars_attribute_value_repetition(source, target(R"({"type":"integer","value":"1"})"), options);
  });
  assert(real.message.find("[(col(\"count\").cast(Float64)) >= (1.5)]") != std::string::npos);
}

void polars_integers_beyond_int128_fail_when_the_literal_is_built() {
  const auto source = table("Int64", {{"a", int_cell("1")}});
  const auto error = polars_error([&] {
    (void)filter_polars_attribute_value_repetition(
        source, target(R"({"type":"integer","value":"170141183460469231731687303715884105728"})"));
  });
  assert(error.type == "InvalidOperationError" && error.phase == "apply");
}

void polars_single_row_nan_case_joins_nothing() {
  const auto source = parse_event_table_json(
      "{\"columns\":[\"case:concept:name\",\"concept:name\"],\"dtypes\":[\"Float64\",\"Int64\"],\"rows\":[[" +
      std::string("{\"key\":\"case:concept:name\",\"kind\":\"float\",\"value\":\"NaN\",\"typed_value\":\"NaN\"},") +
      int_cell("1") + "]]}");
  assert(filter_polars_attribute_value_repetition(source, target(R"({"type":"integer","value":"1"})"),
                                                  at_least_one<PolarsAttributeRepetitionOptions>()).rows.empty());
}

void log_scalar_wrappers_compare_as_value_children_mappings() {
  std::istringstream xes(R"(<?xml version="1.0" encoding="UTF-8"?>
<log xes.version="1.0">
 <trace><string key="concept:name" value="t0"/>
  <event><string key="concept:name" value="v"><int key="a" value="1"/><string key="a" value="2"/></string></event>
 </trace>
 <trace><string key="concept:name" value="t1"/>
  <event><string key="concept:name" value="v"><values><string key="a" value="1"/></values></string></event>
 </trace>
</log>
)");
  const auto log = import_xes_event_log(xes);
  LogAttributeRepetitionOptions options;
  options.minimum = TokenInteger(1);
  const auto retained = [&](const std::string& json) {
    std::vector<std::string> out;
    for (const auto& trace : filter_log_attribute_value_repetition(log, target(json), options).traces) out.push_back(trace.case_id);
    return out;
  };
  // Later duplicate children replace earlier ones; <values> keeps key/value pairs.
  assert(retained(R"({"type":"map","value":{"value":{"type":"string","value":"v"},"children":{"type":"map","value":{"a":{"type":"string","value":"2"}}}}})") ==
         std::vector<std::string>{"t0"});
  assert(retained(R"({"type":"map","value":{"value":{"type":"string","value":"v"},"children":{"type":"list","value":[{"type":"tuple","value":[{"type":"string","value":"a"},{"type":"string","value":"1"}]}]}}})") ==
         std::vector<std::string>{"t1"});
  assert(retained(R"({"type":"string","value":"v"})").empty());
}

void log_xes_lists_compare_as_public_list_values() {
  std::istringstream xes(R"(<?xml version="1.0" encoding="UTF-8"?>
<log xes.version="1.0">
 <trace><string key="concept:name" value="mapping"/>
  <event><list key="k"><string key="a" value="1"/><container key="c"/><string key="a" value="2"/></list></event>
 </trace>
 <trace><string key="concept:name" value="pairs"/>
  <event><list key="k"><values><string key="a" value="1"/></values><string key="b" value="2"/></list></event>
 </trace>
 <trace><string key="concept:name" value="late"/>
  <event><list key="k"><string key="b" value="2"/><values><string key="a" value="1"/></values></list></event>
 </trace>
 <trace><string key="concept:name" value="empty"/>
  <event><list key="k"/></event><event><list key="k"></list></event>
 </trace>
 <trace><string key="concept:name" value="container"/>
  <event><container key="k"><string key="a" value="2"/></container></event>
 </trace>
</log>
)");
  const auto log = import_xes_event_log(xes);
  LogAttributeRepetitionOptions options;
  options.attribute_key = std::string("k");
  options.minimum = TokenInteger(1);
  const auto retained = [&](const std::string& json) {
    std::vector<std::string> out;
    for (const auto& trace : filter_log_attribute_value_repetition(log, target(json), options).traces) out.push_back(trace.case_id);
    return out;
  };
  const std::string none = R"({"type":"null"})";
  const auto pair = [](const std::string& key, const std::string& value) {
    return R"({"type":"tuple","value":[{"type":"string","value":")" + key + R"("},{"type":"string","value":")" + value + R"("}]})";
  };
  // A <list> is {'value': None, 'children': ...}; containers are never stored,
  // and a <values> block after a stored child is ignored.
  assert(retained(R"({"type":"map","value":{"value":)" + none + R"(,"children":{"type":"map","value":{"a":{"type":"string","value":"2"}}}}})") ==
         std::vector<std::string>{"mapping"});
  assert(retained(R"({"type":"map","value":{"value":)" + none + R"(,"children":{"type":"list","value":[)" + pair("a", "1") + "," + pair("b", "2") + "]}}}") ==
         std::vector<std::string>{"pairs"});
  assert(retained(R"({"type":"map","value":{"value":)" + none + R"(,"children":{"type":"map","value":{"b":{"type":"string","value":"2"}}}}})") ==
         std::vector<std::string>{"late"});
  // A <list> without child elements is None.
  options.minimum = TokenInteger(2);
  assert(retained(none) == std::vector<std::string>{"empty"});
  options.minimum = TokenInteger(1);
  // A plain mapping equal to the container's children matches nothing.
  assert(retained(R"({"type":"map","value":{"a":{"type":"string","value":"2"}}})").empty());
}

}  // namespace

int main() {
  pandas_numeric_columns_compare_weak_python_scalars();
  pandas_categorical_columns_compare_like_their_categories();
  pandas_datetime_columns_localize_naive_strings_to_named_zones();
  pandas_boolean_columns_reject_integers_beyond_c_long();
  polars_named_zone_literals_compare_only_with_the_same_zone();
  polars_contradictory_integer_bounds_fold_before_comparison();
  polars_string_bound_diagnostics_render_simplified_count_comparisons();
  polars_integers_beyond_int128_fail_when_the_literal_is_built();
  polars_single_row_nan_case_joins_nothing();
  log_scalar_wrappers_compare_as_value_children_mappings();
  log_xes_lists_compare_as_public_list_values();
  return 0;
}
