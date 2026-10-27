// Regression cases from the 2026-10-27 broad parity audit for dataframe
// filtering, formatting and case statistics. Every expected value comes from a
// public PM4Py 2.7.22 call (pandas 3.0.5, NumPy 2.4.6).
#undef NDEBUG

#include "minera/case_arrival.hpp"
#include "minera/dataframe_format.hpp"
#include "minera/json_export.hpp"
#include "minera/pandas_case_filter.hpp"
#include "minera/pandas_sequence_filters.hpp"
#include "minera/timestamp.hpp"

#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {
using namespace minera;

EventAttribute text(std::string key, std::string value) { return {std::move(key), std::move(value)}; }
EventAttribute integer(std::string key, std::int64_t value) {
  EventAttribute a{std::move(key), std::to_string(value), AttributeKind::kInt};
  a.typed_value = value;
  return a;
}
EventAttribute real(std::string key, double value, std::string lexical) {
  EventAttribute a{std::move(key), std::move(lexical), AttributeKind::kFloat};
  a.typed_value = value;
  return a;
}
EventAttribute boolean(std::string key, bool value) {
  EventAttribute a{std::move(key), value ? "True" : "False", AttributeKind::kBoolean};
  a.typed_value = value;
  return a;
}
EventAttribute date(std::string key, std::string value,
                    NativeDateResolution resolution = NativeDateResolution::kNanosecond) {
  EventAttribute a{std::move(key), value, AttributeKind::kDate};
  a.typed_value = NativeDate{value, resolution, std::string("UTC")};
  return a;
}
EventAttribute list(std::vector<std::int64_t> values, AttributeContainerKind kind = AttributeContainerKind::kList) {
  EventAttribute a{"", ""};
  a.container = kind;
  for (const auto value : values) a.children.push_back(integer("", value));
  return a;
}
EventAttribute none() {
  EventAttribute a{"", ""};
  a.is_null = true;
  return a;
}

// A case/activity/timestamp table with an object case column.
EventTable table(std::vector<EventAttribute> cases, std::vector<std::string> activities,
                 std::vector<std::string> timestamps, std::string unit = "ns") {
  EventTable result;
  result.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  result.dtypes = {"object", "str", "datetime64[" + unit + ", UTC]"};
  const auto resolution = unit == "us" ? NativeDateResolution::kMicrosecond : NativeDateResolution::kNanosecond;
  for (std::size_t i = 0; i < cases.size(); ++i) {
    cases[i].key = "case:concept:name";
    result.rows.push_back({cases[i], text("concept:name", activities[i]),
                           date("time:timestamp", timestamps[i], resolution)});
  }
  return result;
}

std::vector<std::string> column(const EventTable& t, const std::string& name) {
  std::size_t c = 0;
  while (t.columns[c] != name) ++c;
  std::vector<std::string> out;
  for (const auto& row : t.rows) out.push_back(row[c] && !row[c]->is_null ? row[c]->value : "<null>");
  return out;
}

template <class F>
std::string error_of(F&& f) {
  try {
    (void)f();
  } catch (const std::exception& e) {
    return e.what();
  }
  return {};
}

// FS1: list and tuple size bounds compare row by row with the per-row case
// size, the upper bound against every row before label alignment.
void case_size_vector_bounds() {
  const auto t = table({text("", "a"), text("", "a"), text("", "b")}, {"X", "Y", "Z"},
                       {"2026-01-01T00:00:00+00:00", "2026-01-02T00:00:00+00:00", "2026-01-03T00:00:00+00:00"});
  PandasCaseFilterOptions o;
  const auto rows = [&](const EventAttribute& lower, const EventAttribute& upper) {
    return column(filter_pandas_case_size(t, lower, upper, o), "concept:name");
  };
  const std::vector<std::string> first_and_last{"X", "Z"};
  assert(rows(list({1, 3, 1}), none()) == first_and_last);
  assert(rows(list({1, 3, 1}), integer("", 10)) == first_and_last);
  assert(rows(list({1, 3, 1}, AttributeContainerKind::kTuple), none()) == first_and_last);
  assert(rows(list({2}, AttributeContainerKind::kTuple), none()) == (std::vector<std::string>{"X", "Y"}));
  assert(rows(list({3}, AttributeContainerKind::kTuple), none()).empty());
  assert(rows(integer("", 1), list({2, 2, 1})) == (std::vector<std::string>{"X", "Y", "Z"}));
  assert(rows(integer("", 1), list({1, 1, 1})) == std::vector<std::string>{"Z"});
  assert(rows(list({1, 3, 1}), list({5, 5, 5})) == first_and_last);
  assert(error_of([&] { return rows(integer("", 1), list({1, 1})); }) ==
         "('Lengths must match to compare', (3,), (2,))");
  assert(error_of([&] { return rows(list({1, 3, 1}), list({5, 5})); }) ==
         "('Lengths must match to compare', (3,), (2,))");
  assert(error_of([&] { return rows(integer("", 1), list({1, 2}, AttributeContainerKind::kTuple)); }) ==
         "operands could not be broadcast together with shapes (3,) (2,) ");
}

// FS2: durations are Timedelta.total_seconds of the exact unit count.
void case_duration_boundaries() {
  PandasCaseFilterOptions o;
  const auto kept = [&](std::string a, std::string b, double low, double high, std::string unit) {
    const auto t = table({text("", "c"), text("", "c")}, {"X", "Y"}, {std::move(a), std::move(b)}, unit);
    return filter_pandas_case_performance(t, real("", low, ""), real("", high, ""), o).rows.size();
  };
  assert(kept("2026-10-27T11:59:59.999999999+00:00", "2026-10-27T12:00:00+00:00", 1e-9, 1e-9, "ns") == 2);
  assert(kept("2026-10-27T11:59:59.999999+00:00", "2026-10-27T12:00:00+00:00", 1e-6, 1e-6, "us") == 2);
  assert(kept("2026-10-27T11:59:59.700000+00:00", "2026-10-27T12:00:00+00:00", 0.3, 0.3, "ns") == 2);
  assert(kept("2026-10-27T11:59:59.700000+00:00", "2026-10-27T12:00:00+00:00", 0.30000000000000004, 1, "ns") == 0);
  assert(kept("2026-10-27T11:59:59.900000+00:00", "2026-10-27T12:00:01.200000+00:00", 1.3, 1.3, "ns") == 2);
}

// FS3: case ids that do not compare (1 and 'a') sort numbers first.
void mixed_case_ids() {
  PandasCaseFilterOptions o;
  const auto t = table({integer("", 1), text("", "a")}, {"X", "Y"},
                       {"2026-01-01T00:00:00+00:00", "2026-01-02T00:00:00+00:00"});
  assert(filter_pandas_case_performance(t, integer("", 0), integer("", 1), o).rows.size() == 2);
  // Durations follow the case order 1, 2, 'a', 'b'; a -1 bound drops one case.
  const auto four = table({text("", "b"), integer("", 2), text("", "a"), integer("", 1)}, {"X", "Y", "Z", "W"},
                          {"2026-01-01T00:00:00+00:00", "2026-01-02T00:00:00+00:00",
                           "2026-01-03T00:00:00+00:00", "2026-01-04T00:00:00+00:00"});
  EventAttribute bounds{"", ""};
  bounds.container = AttributeContainerKind::kList;
  const std::vector<std::string> dropped{"W", "Y", "Z", "X"};
  for (std::size_t i = 0; i < 4; ++i) {
    bounds.children.clear();
    for (std::size_t j = 0; j < 4; ++j) bounds.children.push_back(integer("", j == i ? -1 : 0));
    const auto rows = column(filter_pandas_case_performance(four, integer("", -5), bounds, o), "concept:name");
    assert(rows.size() == 3);
    for (const auto& row : rows) assert(row != dropped[i]);
  }
}

// FS4: a missing required value drops its row instead of becoming "<NA>".
void format_drops_missing_values() {
  EventTable input = table({make_native_na_attribute(""), text("", "c")}, {"X", "Y"},
                           {"2026-01-01T00:00:00+00:00", "2026-01-02T00:00:00+00:00"});
  DataFrameFormatOptions options;
  options.start_timestamp_key = std::nullopt;
  const auto formatted = format_dataframe(input, options);
  assert(column(formatted, "case:concept:name") == std::vector<std::string>{"c"});
  EventTable missing_activity = table({text("", "c"), text("", "c")}, {"X", "Y"},
                                      {"2026-01-01T00:00:00+00:00", "2026-01-02T00:00:00+00:00"});
  missing_activity.rows[0][1] = make_native_na_attribute("concept:name");
  assert(column(format_dataframe(missing_activity, options), "concept:name") == std::vector<std::string>{"Y"});
}

// FS8: a custom object case column of 1 converts to a date before the
// canonical case id is printed, as with the default selector.
void format_custom_numeric_selectors() {
  EventTable input;
  input.columns = {"trace key", "step \xCE\xA9", "clock"};
  input.dtypes = {"object", "object", "object"};
  input.rows.push_back({integer("trace key", 1), integer("step \xCE\xA9", 2),
                        text("clock", "2026-01-01T00:00:00Z")});
  DataFrameFormatOptions options;
  options.case_id = "trace key";
  options.activity_key = "step \xCE\xA9";
  options.timestamp_key = "clock";
  options.start_timestamp_key = std::nullopt;
  const auto formatted = format_dataframe(input, options);
  assert(column(formatted, "case:concept:name") ==
         std::vector<std::string>{"1970-01-01 00:00:00.000000001+00:00"});
  assert(column(formatted, "concept:name") == std::vector<std::string>{"1970-01-01 00:00:00.000000002+00:00"});
}

// FS5: 1, True and 1.0 are one case; the string '1' is another.
void equal_numeric_case_keys() {
  const std::vector<std::string> same_time{"2026-01-01T00:00:00+00:00", "2026-01-01T00:00:00+00:00"};
  const PandasFilterParameters strict{{{"STRICT", true, PandasFilterValue::of(true)}}};
  for (const auto& second : {boolean("", true), real("", 1.0, "1.0")}) {
    const auto t = table({integer("", 1), second}, {"X", "Y"}, same_time);
    const auto grouped = pandas_timestamp_case_grouping_apply(t);
    assert(grouped.rows.size() == 1 && grouped.dtypes[0] == "int64");
    assert(column(pandas_prefix_filter_apply(t, PandasFilterValue::of("Y"), strict), "concept:name") ==
           std::vector<std::string>{"X"});
    assert(column(pandas_suffix_filter_apply(t, PandasFilterValue::of("X"), strict), "concept:name") ==
           std::vector<std::string>{"Y"});
  }
  const auto distinct = table({integer("", 1), text("", "1")}, {"X", "Y"}, same_time);
  const auto grouped = pandas_timestamp_case_grouping_apply(distinct);
  assert(grouped.rows.size() == 2 && grouped.dtypes[0] == "object");
  assert(pandas_prefix_filter_apply(distinct, PandasFilterValue::of("Y"), strict).rows.empty());
  // String keys give a str column.
  const auto strings = table({text("", "a"), text("", "a")}, {"X", "Y"}, same_time);
  assert(pandas_timestamp_case_grouping_apply(strings).dtypes[0] == "str");
}

// FS metadata: a missing case id comes back from consecutive grouping as None.
void consecutive_grouping_missing_case() {
  const auto t = table({make_native_na_attribute("")}, {"X"}, {"2026-01-01T00:00:00+00:00"});
  const auto grouped = pandas_consecutive_act_case_grouping_apply(t);
  assert(grouped.rows.size() == 1 && grouped.rows[0][0]->is_null && grouped.dtypes[0] == "object");
}

// FS6: arrival gaps come from exact instants (one microsecond, not 9.5e-7)
// and are written with full precision.
void case_arrival_precision() {
  for (const std::string year : {"2026", "9999"}) {
    EventLog log;
    for (const std::string fraction : {"000000", "000001"}) {
      Trace trace;
      trace.case_id = "c" + fraction;
      Event event;
      event.case_id = trace.case_id;
      event.activity = "A";
      event.timestamp = year + "-01-01T00:00:00." + fraction + "Z";
      trace.events.push_back(event);
      log.traces.push_back(trace);
    }
    const auto result = discover_case_arrival(log);
    assert(result.arrival_average == 1e-6 && result.dispersion_average == 1e-6);
    assert(case_arrival_json(result).find("\"arrival_average\":1e-06") != std::string::npos);
  }
}

// FS7: a decimal fraction is parsed exactly and rounded once, as Python's
// datetime.timestamp() does.
void decimal_timestamps() {
  const auto start = parse_timestamp("2200-01-01T00:00:00.000000Z");
  const auto end = parse_timestamp("2200-01-01T00:00:00.999999Z");
  assert(start && end && *end - *start == 0.9999990463256836);
  assert(*parse_timestamp("2026-01-01T00:00:00.5Z") == 1767225600.5);
}

}  // namespace

int main() {
  case_size_vector_bounds();
  case_duration_boundaries();
  mixed_case_ids();
  format_drops_missing_values();
  format_custom_numeric_selectors();
  equal_numeric_case_keys();
  consecutive_grouping_missing_case();
  case_arrival_precision();
  decimal_timestamps();
}
