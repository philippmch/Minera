#include "minera/heuristic_miner.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/json_export.hpp"
#include "minera/log_conversion.hpp"
#include "minera/model_decoration.hpp"
#include "minera/petri_analysis.hpp"
#include "minera/petri_net.hpp"
#include "minera/event_table_json.hpp"
#include "../exact_count_ratio.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iterator>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

using namespace minera;

namespace {

Event event(std::string activity, std::string timestamp,
            std::string lifecycle = {}) {
  Event result;
  result.activity = std::move(activity);
  result.timestamp = std::move(timestamp);
  result.lifecycle = std::move(lifecycle);
  return result;
}

void attr(Event& event, std::string key, std::string value) {
  event.attributes.push_back(
      {std::move(key), std::move(value), AttributeKind::kString});
}

Event interval(std::string activity, std::string start,
               std::string complete, std::string start_key = "start_timestamp") {
  Event result = event(std::move(activity), std::move(complete));
  attr(result, std::move(start_key), std::move(start));
  return result;
}

Trace trace(std::vector<Event> events) {
  Trace result;
  result.events = std::move(events);
  return result;
}

EventLog log(std::vector<Trace> traces) {
  EventLog result;
  result.traces = std::move(traces);
  return result;
}

EventAttribute typed_date(
    std::string key, std::string value,
    std::optional<std::string> timezone = std::string{"UTC"}) {
  EventAttribute result;
  result.key = std::move(key);
  result.value = value;
  result.kind = AttributeKind::kDate;
  NativeDate date{std::move(value)};
  date.timezone = std::move(timezone);
  result.typed_value = std::move(date);
  return result;
}

Event classic_timestamp_event(std::string name, std::string completed,
                              std::optional<std::string> conventional_start,
                              std::optional<std::string> custom_start) {
  auto result = event(std::move(name), std::move(completed));
  if (conventional_start)
    result.attributes.push_back(
        typed_date("start_timestamp", std::move(*conventional_start)));
  if (custom_start)
    result.attributes.push_back(
        typed_date("custom:start", std::move(*custom_start)));
  return result;
}

EventLog classic_timestamp_log(std::vector<std::vector<Event>> cases) {
  EventLog result;
  result.traces.reserve(cases.size());
  for (std::size_t index = 0; index < cases.size(); ++index) {
    Trace current;
    current.case_id = "case-" + std::to_string(index);
    current.case_id_state = FirstClassAttributeState::kPresent;
    current.case_id_source_key = "case:concept:name";
    current.events = std::move(cases[index]);
    for (auto& current_event : current.events) {
      current_event.case_id = current.case_id;
      current_event.case_id_state = FirstClassAttributeState::kPresent;
      current_event.case_id_source_key = "case:concept:name";
      if (!current_event.timestamp_attribute) {
        current_event.timestamp_attribute =
            typed_date("time:timestamp", current_event.timestamp);
        current_event.timestamp_state = FirstClassAttributeState::kPresent;
        current_event.timestamp_source_key = "time:timestamp";
      }
    }
    result.traces.push_back(std::move(current));
  }
  return result;
}

void set_classic_timezone(EventLog& source, std::string timezone) {
  for (auto& current_trace : source.traces) {
    for (auto& current_event : current_trace.events) {
      current_event.timestamp_attribute = typed_date(
          "time:timestamp", current_event.timestamp, timezone);
      current_event.timestamp_state = FirstClassAttributeState::kPresent;
      current_event.timestamp_source_key = "time:timestamp";
      for (auto& attribute : current_event.attributes) {
        if (attribute.kind != AttributeKind::kDate) continue;
        if (auto* date = std::get_if<NativeDate>(&attribute.typed_value))
          date->timezone = timezone;
      }
    }
  }
}

using ClassicFlatRow = std::pair<std::size_t, std::size_t>;

EventTable classic_timestamp_table(
    const EventLog& source,
    const std::vector<std::string>& custom_case_ids = {},
    std::vector<ClassicFlatRow> row_order = {},
    std::vector<std::int64_t> index_labels = {}) {
  EventTable result;
  result.columns = {"case:concept:name", "concept:name"};
  result.dtypes = {"str", "str"};
  if (!custom_case_ids.empty()) {
    result.columns.push_back("case:custom:id");
    result.dtypes.push_back("str");
  }
  result.columns.insert(result.columns.end(),
                        {"time:timestamp", "start_timestamp", "custom:start"});
  result.dtypes.insert(result.dtypes.end(),
                       {"datetime64[us, UTC]", "datetime64[us, UTC]",
                        "datetime64[us, UTC]"});
  if (source.traces.size() > 0 && !source.traces.front().events.empty()) {
    const auto& first = source.traces.front().events.front();
    if (first.timestamp_attribute) {
      if (const auto* date =
              std::get_if<NativeDate>(&first.timestamp_attribute->typed_value);
          date != nullptr && date->timezone.has_value()) {
        const auto dtype = "datetime64[us, " + *date->timezone + "]";
        std::fill(result.dtypes.end() - 3, result.dtypes.end(), dtype);
      }
    }
  }
  if (row_order.empty()) {
    for (std::size_t trace_index = 0; trace_index < source.traces.size();
         ++trace_index) {
      for (std::size_t event_index = 0;
           event_index < source.traces[trace_index].events.size();
           ++event_index)
        row_order.emplace_back(trace_index, event_index);
    }
  }
  std::size_t flat_index = 0;
  for (const auto [trace_index, event_index] : row_order) {
    const auto& current_trace = source.traces.at(trace_index);
    const auto& current_event = current_trace.events.at(event_index);
    EventTable::Row row(result.columns.size());
    for (std::size_t column = 0; column < result.columns.size(); ++column) {
      const auto& key = result.columns[column];
      if (key == "case:concept:name") {
        row[column] = EventAttribute{key, current_trace.case_id,
                                     AttributeKind::kString};
        continue;
      }
      if (key == "concept:name") {
        row[column] = EventAttribute{key, current_event.activity,
                                     AttributeKind::kString};
        continue;
      }
      if (key == "case:custom:id") {
        row[column] = EventAttribute{key, custom_case_ids.at(flat_index),
                                     AttributeKind::kString};
        continue;
      }
      if (key == "time:timestamp") {
        row[column] = current_event.timestamp_attribute
                          ? *current_event.timestamp_attribute
                          : typed_date(key, current_event.timestamp);
        row[column]->key = key;
        continue;
      }
      const auto found = std::find_if(
          current_event.attributes.begin(), current_event.attributes.end(),
          [&](const EventAttribute& attribute) { return attribute.key == key; });
      if (found != current_event.attributes.end()) {
        auto value = *found;
        value.key = key;
        row[column] = std::move(value);
      } else if (key == "custom:start") {
        // The captured DataFrame has this datetime column in its schema;
        // absent EventLog cells therefore enter it as typed pandas NaT.
        row[column] = typed_date(key, "NaT");
      }
    }
    result.rows.push_back(std::move(row));
    ++flat_index;
  }
  const bool has_custom_index = !index_labels.empty();
  if (index_labels.empty()) {
    std::int64_t next = 0;
    for (const auto& _ : result.rows) {
      (void)_;
      index_labels.push_back(next++);
    }
  }
  if (!index_labels.empty()) {
    EventTableIndex index;
    index.kind = has_custom_index ? "Index" : "RangeIndex";
    index.dtype = "int64";
    index.names = {std::nullopt};
    for (const auto label : index_labels) {
      EventAttribute value;
      value.value = std::to_string(label);
      value.kind = AttributeKind::kInt;
      value.typed_value = label;
      index.labels.push_back(EventTableIndexLabel{std::move(value)});
    }
    result.index = std::move(index);
  }
  result.validate();
  return result;
}

HeuristicMinerOptions plus_options() {
  HeuristicMinerOptions options;
  options.variant = HeuristicMinerVariant::kPlusPlus;
  return options;
}

std::size_t activity(const HeuristicsNet& net, const std::string& name) {
  for (std::size_t i = 0; i < net.activities.size(); ++i)
    if (net.activities[i] == name) return i;
  assert(false);
  return 0;
}

const HeuristicDependency* edge(const HeuristicsNet& net,
                                const std::string& source,
                                const std::string& target) {
  const auto a = activity(net, source);
  const auto b = activity(net, target);
  for (const auto& candidate : net.edges)
    if (candidate.source == a && candidate.target == b) return &candidate;
  return nullptr;
}

const HeuristicRelationStatistics& relation(const HeuristicsNet& net,
                                             const std::string& source,
                                             const std::string& target) {
  const auto a = activity(net, source);
  const auto b = activity(net, target);
  for (const auto& candidate : net.relation_statistics)
    if (candidate.source == a && candidate.target == b) return candidate;
  assert(false);
  return net.relation_statistics.front();
}

const HeuristicActivityStatistics& activity_stat(const HeuristicsNet& net,
                                                  const std::string& name) {
  const auto id = activity(net, name);
  for (const auto& candidate : net.activity_statistics)
    if (candidate.activity == id) return candidate;
  assert(false);
  return net.activity_statistics.front();
}

const HeuristicAndMeasure* and_measure(const HeuristicsNet& net, bool outgoing,
                                       const std::string& center,
                                       const std::string& first,
                                       const std::string& second) {
  const auto activity_id = activity(net, center);
  const auto first_id = activity(net, first);
  const auto second_id = activity(net, second);
  const auto& measures =
      outgoing ? net.and_measures_out : net.and_measures_in;
  for (const auto& measure : measures) {
    if (measure.activity == activity_id && measure.first == first_id &&
        measure.second == second_id)
      return &measure;
  }
  return nullptr;
}

const HeuristicDependencyMeasure* dependency_measure(
    const HeuristicsNet& net, const std::string& source,
    const std::string& target) {
  const auto source_id = activity(net, source);
  const auto target_id = activity(net, target);
  for (const auto& measure : net.dependency_measures) {
    if (measure.source == source_id && measure.target == target_id)
      return &measure;
  }
  return nullptr;
}

const HeuristicLoopLengthTwo* loop_length_two(
    const HeuristicsNet& net, const std::string& source,
    const std::string& target) {
  const auto source_id = activity(net, source);
  const auto target_id = activity(net, target);
  for (const auto& loop : net.loop_length_two) {
    if (loop.source == source_id && loop.target == target_id) return &loop;
  }
  return nullptr;
}

EventTable pandas_interval_table(std::size_t b_outer_cases) {
  std::vector<std::vector<Event>> cases;
  for (std::size_t i = 0; i < 4; ++i) {
    const bool b_outer = i < b_outer_cases;
    cases.push_back({
        classic_timestamp_event("A", "2024-01-01T00:00:01Z",
                                "2024-01-01T00:00:00Z", std::nullopt),
        classic_timestamp_event(b_outer ? "B" : "C",
                                "2024-01-01T00:00:06Z",
                                "2024-01-01T00:00:02Z", std::nullopt),
        classic_timestamp_event(b_outer ? "C" : "B",
                                "2024-01-01T00:00:05Z",
                                "2024-01-01T00:00:03Z", std::nullopt),
        classic_timestamp_event("D", "2024-01-01T00:00:08Z",
                                "2024-01-01T00:00:07Z", std::nullopt)});
  }
  return classic_timestamp_table(classic_timestamp_log(std::move(cases)));
}

void plusplus_pandas_directional_concurrency_acceptance() {
  // Actual public controls cover uniform, balanced and unbalanced time orders.
  // The mixed pandas counts differ from generic PLUSPLUS discovery on the same
  // table; these counts determine the accepted AND splits and Petri structure.
  for (const auto [b_outer_cases, expected] :
       std::vector<std::pair<std::size_t, std::uint64_t>>{
           {4, 4}, {0, 4}, {2, 2}, {3, 1}, {1, 3}}) {
    auto table = pandas_interval_table(b_outer_cases);
    const auto before = table;
    auto options = plus_options();
    options.and_threshold = 0.0;
    const auto generic = discover_heuristics_net(table, options);
    assert(generic.raw_concurrency_statistics.size() == 1);
    assert(generic.raw_concurrency_statistics.front().frequency == 4);
    assert(table.rows == before.rows);
    const auto pandas = discover_plus_plus_pandas(table, options);
    assert(pandas.raw_concurrency_statistics.size() == 1);
    const auto& concurrent = pandas.raw_concurrency_statistics.front();
    assert(concurrent.first == "B" && concurrent.second == "C");
    assert(concurrent.frequency == expected);
    assert(options.variant == HeuristicMinerVariant::kPlusPlus);
    assert(options.and_threshold == 0.0);
    assert(table.index == before.index);
    assert(table.attributes == before.attributes);
    assert(table.metadata == before.metadata);
    for (std::size_t row = 0; row < table.rows.size(); ++row)
      assert(std::equal(before.rows[row].begin(), before.rows[row].end(),
                        table.rows[row].begin()));
  }
  auto options = plus_options();
  options.and_threshold = 0.0;
  auto table = pandas_interval_table(2);
  const auto zero = discover_plus_plus_pandas(table, options);
  assert(and_measure(zero, true, "A", "B", "C")->value == 0.5);
  assert(and_measure(zero, false, "D", "B", "C")->value == 0.25);
  options.and_threshold = 0.5;
  const auto at = discover_plus_plus_pandas(table, options);
  assert(!and_measure(at, true, "A", "B", "C"));
  assert(!and_measure(at, false, "D", "B", "C"));
  const auto petri = to_petri_net_presentation(at);
  assert(petri.places.size() == 4);
  assert(petri.transitions.size() == 4);
  options.and_threshold = std::nextafter(0.5, 0.0);
  const auto below = discover_plus_plus_pandas(table, options);
  assert(and_measure(below, true, "A", "B", "C")->value == 0.5);
}

void plusplus_pandas_duration_mutation_and_start_fallback() {
  auto table = pandas_interval_table(2);
  const auto before = table;
  // The public pandas entry point selects PLUSPLUS even with native defaults.
  const HeuristicMinerOptions defaults;
  const auto net = discover_plus_plus_pandas(table, defaults);
  assert(net.variant == HeuristicMinerVariant::kPlusPlus);
  assert(defaults.variant == HeuristicMinerVariant::kClassic);
  assert(!defaults.start_timestamp_attribute);
  assert(net.sojourn_times->at("A") == 1.0);
  assert(net.sojourn_times->at("B") == 3.0);
  assert(net.sojourn_times->at("C") == 3.0);
  assert(net.sojourn_times->at("D") == 1.0);
  assert(table.columns.back() == "@@diff" && table.dtypes.back() == "float64");
  const std::vector<double> expected{1.0, 4.0, 2.0, 1.0};
  for (std::size_t i = 0; i < table.rows.size(); ++i) {
    const auto& value = *table.rows[i].back();
    assert(value.kind == AttributeKind::kFloat);
    assert(std::get<double>(value.typed_value) == expected[i % 4]);
  }
  const auto first = table;
  // Repeated calls overwrite the duration column without duplicating it.
  table.rows.front().back()->typed_value = 99.0;
  table.rows.front().back()->value = "99.0";
  table.dtypes.back() = "object";
  (void)discover_plus_plus_pandas(table);
  assert(table.columns == first.columns && table.rows == first.rows);
  assert(table.dtypes == first.dtypes);

  table = before;
  const auto start = table.column_index("start_timestamp");
  table.columns.erase(table.columns.begin() + start);
  table.dtypes.erase(table.dtypes.begin() + start);
  for (auto& row : table.rows) row.erase(row.begin() + start);
  const auto without_start = table;
  const auto fallback = discover_plus_plus_pandas(table);
  assert(table.columns[table.columns.size() - 2] == "start_timestamp");
  assert(table.columns.back() == "@@diff");
  assert(table.dtypes[table.dtypes.size() - 2] == "datetime64[us, UTC]");
  const auto completed = table.column_index("time:timestamp");
  const auto started = table.column_index("start_timestamp");
  for (const auto& row : table.rows) {
    assert(row[started]->value == row[completed]->value);
    assert(row[started]->typed_value == row[completed]->typed_value);
    assert(std::get<double>(row.back()->typed_value) == 0.0);
  }
  assert(table.index == without_start.index);
  assert(fallback.raw_concurrency_statistics.empty());
  for (const auto& [_, value] : *fallback.sojourn_times) assert(value == 0.0);
  const auto petri = to_petri_net_presentation(fallback);
  assert(petri.places.size() == 6);
  assert(petri.transitions.size() == 4);
  table.validate();
}

void plusplus_pandas_object_dtype_error_preserves_input() {
  auto table = pandas_interval_table(2);
  table.dtypes[table.column_index("time:timestamp")] = "object";
  table.dtypes[table.column_index("start_timestamp")] = "object";
  const auto before = table;
  bool threw = false;
  try {
    (void)discover_plus_plus_pandas(table);
  } catch (const HeuristicPandasError& error) {
    threw = true;
    assert(error.reference_type() == "builtins.AttributeError");
    assert(std::string(error.what()) ==
           "Can only use .dt accessor with datetimelike values");
  }
  assert(threw);
  assert(table.columns == before.columns && table.dtypes == before.dtypes);
  assert(table.rows == before.rows && table.index == before.index);
  assert(table.attributes == before.attributes && table.metadata == before.metadata);
  // Public direct PLUSPLUS accepts the same object-dtype carrier.
  const auto generic = discover_heuristics_net(table, plus_options());
  assert(generic.raw_concurrency_statistics.front().frequency == 4);
}

void plusplus_pandas_missing_start_error_mutation() {
  auto table = pandas_interval_table(2);
  const auto start = table.column_index("start_timestamp");
  table.columns.erase(table.columns.begin() + start);
  table.dtypes.erase(table.dtypes.begin() + start);
  for (auto& row : table.rows) row.erase(row.begin() + start);
  const auto complete = table.column_index("time:timestamp");
  table.dtypes[complete] = "object";
  const auto before = table;
  bool threw = false;
  try {
    (void)discover_plus_plus_pandas(table);
  } catch (const HeuristicPandasError& error) {
    threw = true;
    assert(error.reference_type() == "builtins.AttributeError");
  }
  assert(threw);
  assert(table.columns.size() == before.columns.size() + 1);
  assert(table.columns.back() == "start_timestamp");
  assert(table.dtypes.back() == "object");
  assert(table.index == before.index);
  for (const auto& row : table.rows) {
    assert(row.back()->value == row[complete]->value);
    assert(row.back()->typed_value == row[complete]->typed_value);
  }
  assert(std::find(table.columns.begin(), table.columns.end(), "@@diff") ==
         table.columns.end());
}

void plusplus_pandas_exact_duration_and_performance() {
  for (const bool nanos : {false, true}) {
    auto table = pandas_interval_table(2);
    const auto start = table.column_index("start_timestamp");
    const auto complete = table.column_index("time:timestamp");
    for (auto& row : table.rows) {
      if (row[table.column_index("concept:name")]->value != "B") continue;
      row[start] = typed_date("start_timestamp", nanos
          ? "2024-01-01T00:00:02.000000001Z"
          : "2024-01-01T00:00:02.000001Z");
      row[complete] = typed_date("time:timestamp", nanos
          ? "2024-01-01T00:00:02.000000004Z"
          : "2024-01-01T00:00:02.000003Z");
    }
    if (nanos) {
      table.dtypes[start] = table.dtypes[complete] = "datetime64[ns, UTC]";
      for (auto& row : table.rows)
        for (const auto column : {start, complete})
          std::get<NativeDate>(row[column]->typed_value).resolution =
              NativeDateResolution::kNanosecond;
    }
    auto options = plus_options();
    options.decoration = HeuristicNetDecoration::kPerformance;
    const auto net = discover_plus_plus_pandas(table, options);
    const double expected = nanos ? 3.0 * 1e-9 : 2e-6;
    assert(net.sojourn_times->at("B") == expected);
    assert(activity_stat(net, "B").mean_sojourn_seconds == expected);
    assert(net.plusplus_computed_performance_dfg);
    const auto& waits = *net.plusplus_computed_performance_dfg;
    assert(waits.at({"A", "B"}) == (nanos ? 1.000000001 : 1.000001));
    assert(waits.at({"B", "C"}) == (nanos ? 0.999999996 : 0.9999969999999999));
    assert(waits.at({"B", "D"}) == (nanos ? 4.999999996 : 4.999997));
    for (const auto& row : table.rows)
      if (row[table.column_index("concept:name")]->value == "B")
        assert(std::get<double>(row.back()->typed_value) == expected);
  }
}

void plusplus_pandas_nat_statistics_and_error_order() {
  auto options = plus_options();
  options.decoration = HeuristicNetDecoration::kPerformance;
  for (const bool missing_start : {true, false}) {
    auto table = pandas_interval_table(2);
    const auto column = table.column_index(
        missing_start ? "start_timestamp" : "time:timestamp");
    table.rows[1][column] = typed_date(table.columns[column], "NaT");
    const auto net = discover_plus_plus_pandas(table, options);
    assert(activity_stat(net, "B").occurrences == 4);
    assert(net.sojourn_times->at("B") == 8.0 / 3.0);
    assert(relation(net, "A", "B").eventually_follows_frequency ==
           (missing_start ? 1 : 2));
    assert(relation(net, "B", "D").eventually_follows_frequency == 3);
    assert(std::isnan(std::get<double>(table.rows[1].back()->typed_value)));
    const auto& waits = *net.plusplus_computed_performance_dfg;
    assert(waits.at({"A", "C"}) == (missing_start ? 4.0 / 3.0 : 1.0));

    table = pandas_interval_table(2);
    for (auto& row : table.rows)
      if (row[table.column_index("concept:name")]->value == "B")
        row[column] = typed_date(table.columns[column], "NaT");
    const auto all_b = discover_plus_plus_pandas(table, options);
    assert(activity_stat(all_b, "B").occurrences == 4);
    assert(std::isnan(all_b.sojourn_times->at("B")));

    table = pandas_interval_table(2);
    for (auto& row : table.rows)
      row[column] = typed_date(table.columns[column], "NaT");
    const auto before = table;
    bool threw = false;
    try {
      (void)discover_plus_plus_pandas(table, options);
    } catch (const HeuristicPandasError& error) {
      threw = true;
      assert(error.reference_type() == "builtins.TypeError");
      assert(std::string(error.what()) == (missing_start
          ? "Invalid comparison between dtype=timedelta64[us] and int"
          : "Cannot subtract tz-naive and tz-aware datetime-like objects."));
    }
    assert(threw);
    if (missing_start) {
      assert(table.columns.back() == "@@diff");
      for (const auto& row : table.rows)
        assert(std::isnan(std::get<double>(row.back()->typed_value)));
    } else {
      assert(table.columns == before.columns && table.rows == before.rows);
    }
    assert(table.index == before.index);
  }
}

void plusplus_pandas_occurrence_order_and_self_concurrency() {
  auto table = pandas_interval_table(2);
  const auto name = table.column_index("concept:name");
  for (auto& row : table.rows)
    if (row[name]->value == "C") row[name]->value = "B";
  auto options = plus_options();
  options.decoration = HeuristicNetDecoration::kPerformance;
  const auto net = discover_plus_plus_pandas(table, options);
  assert((net.activities == std::vector<std::string>{"B", "A", "D"}));
  assert(activity_stat(net, "B").occurrences == 8);
  assert(net.sojourn_times->at("B") == 3.0);
  assert(net.raw_concurrency_statistics.size() == 1);
  assert(net.raw_concurrency_statistics.front().first == "B");
  assert(net.raw_concurrency_statistics.front().second == "B");
  assert(net.raw_concurrency_statistics.front().frequency == 4);
}

void plusplus_pandas_concurrency_rounding_keeps_exact_waits() {
  struct Control {
    std::string date;
    std::uint32_t completion_ns;
    std::uint32_t wait_ns;
    bool concurrent;
  };
  const std::vector<Control> controls{
      {"2024-01-01", 0, 1, true},
      {"2024-01-01", 0, 500, true},
      {"2024-01-01", 0, 596, true},
      {"2024-01-01", 0, 597, false},
      {"2024-01-01", 999, 1, true},
      {"1970-01-01", 0, 499, true},
      {"1970-01-01", 0, 500, false},
      {"1960-01-01", 0, 501, true},
      {"2100-01-01", 0, 501, true}};
  for (const auto& control : controls) {
    auto table = pandas_interval_table(2);
    const auto start = table.column_index("start_timestamp");
    const auto complete = table.column_index("time:timestamp");
    table.dtypes[start] = table.dtypes[complete] = "datetime64[ns, UTC]";
    const auto timestamp = [&](int seconds, std::uint32_t nanos) {
      auto fraction = std::to_string(nanos);
      return control.date + "T00:00:" + (seconds < 10 ? "0" : "") +
             std::to_string(seconds) + "." +
             std::string(9 - fraction.size(), '0') + fraction + "Z";
    };
    for (auto& row : table.rows) {
      const auto& name = row[table.column_index("concept:name")]->value;
      int first = name == "A" ? 0 : name == "B" ? 1 : name == "C" ? 10 : 12;
      auto first_ns = name == "B" ? control.completion_ns + control.wait_ns : 0;
      auto last_ns = name == "A" ? control.completion_ns : first_ns;
      row[start] = typed_date("start_timestamp", timestamp(first, first_ns));
      row[complete] = typed_date("time:timestamp", timestamp(first + 1, last_ns));
      for (const auto column : {start, complete})
        std::get<NativeDate>(row[column]->typed_value).resolution =
            NativeDateResolution::kNanosecond;
    }
    auto options = plus_options();
    options.decoration = HeuristicNetDecoration::kPerformance;
    const auto net = discover_plus_plus_pandas(table, options);
    assert(net.raw_concurrency_statistics.size() ==
           (control.concurrent ? 1u : 0u));
    if (control.concurrent) {
      const auto& overlap = net.raw_concurrency_statistics.front();
      assert(overlap.first == "A" && overlap.second == "B");
      assert(overlap.frequency == 4);
    }
    const auto a = activity(net, "A");
    const auto b = activity(net, "B");
    const auto measure = std::find_if(net.dependency_measures.begin(),
        net.dependency_measures.end(), [&](const auto& value) {
          return value.source == a && value.target == b;
        });
    assert(measure != net.dependency_measures.end());
    assert(measure->value == (control.concurrent ? 0.5 : 1.0));
    const auto accepted = std::any_of(net.edges.begin(), net.edges.end(),
        [&](const auto& value) { return value.source == a && value.target == b; });
    assert(accepted == !control.concurrent);
    assert(net.plusplus_computed_performance_dfg->at({"A", "B"}) ==
           static_cast<double>(control.wait_ns) * 1e-9);
  }
}

void plusplus_pandas_signed_array_duration_components() {
  struct Control {
    std::string dtype;
    std::string completed;
    double expected;
  };
  const std::vector<Control> controls{
      {"ns", "2024-01-01T00:00:00.000001Z", 1e-6},
      {"ns", "2024-01-01T00:00:01.000001Z", 1.000001},
      {"us", "2024-01-01T00:00:02.000001Z", 2.000001},
      {"ns", "2023-12-31T23:59:59.999999999Z", -0x1.12e0bf070a000p-30},
      {"us", "2023-12-31T23:59:59.999999Z", -0x1.0c6f7a0b80000p-20},
      {"ms", "2023-12-31T23:59:59.999Z", -0x1.0624dd2f1aa00p-10},
      {"ms", "2024-01-01T00:00:00.007Z", 0x1.cac083126e978p-8},
      {"ns", "2024-01-01T00:00:00Z", 0.0},
      {"ns", "2024-01-02T00:00:00.000000001Z", 0x1.5180000000045p+16},
      {"ns", "2023-12-30T23:59:59.999999999Z", -0x1.5180000000044p+16}};
  for (const auto& control : controls) {
    auto table = pandas_interval_table(2);
    const auto name = table.column_index("concept:name");
    const auto start = table.column_index("start_timestamp");
    const auto complete = table.column_index("time:timestamp");
    table.dtypes[start] = table.dtypes[complete] =
        "datetime64[" + control.dtype + ", UTC]";
    for (auto& row : table.rows) {
      if (row[name]->value == "A")
        row[complete] = typed_date("time:timestamp", control.completed);
      for (const auto column : {start, complete})
        std::get<NativeDate>(row[column]->typed_value).resolution =
            control.dtype == "ns" ? NativeDateResolution::kNanosecond
                                  : NativeDateResolution::kMicrosecond;
    }
    const auto net = discover_plus_plus_pandas(table, plus_options());
    assert(net.sojourn_times->at("A") == control.expected);
    assert(activity_stat(net, "A").mean_sojourn_seconds == control.expected);
    const auto diff = table.column_index("@@diff");
    for (const auto& row : table.rows)
      if (row[name]->value == "A")
        assert(std::get<double>(row[diff]->typed_value) == control.expected);
  }
}

void plusplus_numeric_count_filters_preserve_public_comparisons() {
  // The 88 public numeric controls retain four occurrences per activity,
  // AB/AC paths at count two and BD/CD paths at count four. Fractions are not
  // truncated and NaN rejects.
  const double infinity = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  struct Control { double threshold; bool retains; std::size_t paths; };
  const std::vector<Control> controls{
      {-infinity, true, 4}, {-1.5, true, 4}, {-1.0, true, 4}, {0.0, true, 4},
      {1.0, true, 4}, {-0.0, true, 4}, {0.5, true, 4},
      {std::nextafter(4.0, 0.0), true, 2}, {4.0, true, 2},
      {std::nextafter(4.0, infinity), false, 0}, {4.5, false, 0}, {8.5, false, 0},
      {infinity, false, 0}, {nan, false, 0}, {0x1p64, false, 0}};
  for (const auto decoration : {HeuristicNetDecoration::kFrequency,
                                HeuristicNetDecoration::kPerformance}) {
    for (const auto& control : controls) {
      auto options = plus_options();
      options.decoration = decoration;
      options.min_activity_count = 9;
      options.plusplus_min_activity_count = control.threshold;
      auto table = pandas_interval_table(2);
      const auto filtered_activities = discover_plus_plus_pandas(table, options);
      assert(filtered_activities.activities.size() == (control.retains ? 4 : 0));
      assert(filtered_activities.relation_statistics.size() == (control.retains ? 4 : 0));
      assert(filtered_activities.raw_activity_statistics.size() == 4);
      assert(filtered_activities.sojourn_times->size() == 4);
      assert(options.min_activity_count == 9);

      options.min_activity_count = 1;
      options.plusplus_min_activity_count.reset();
      options.min_frequency = 9;
      options.plusplus_min_frequency = control.threshold;
      table = pandas_interval_table(2);
      const auto filtered_paths = discover_plus_plus_pandas(table, options);
      assert(filtered_paths.activities.size() == 4);
      assert(filtered_paths.relation_statistics.size() == control.paths);
      assert(filtered_paths.raw_activity_statistics.size() == 4);
      assert(filtered_paths.raw_concurrency_statistics.size() == 1);
      assert(options.min_frequency == 9);
      assert(std::isnan(control.threshold)
                 ? std::isnan(*options.plusplus_min_frequency)
                 : *options.plusplus_min_frequency == control.threshold);
    }
  }
  auto table = pandas_interval_table(2);
  auto integer_options = plus_options();
  integer_options.min_activity_count = 9;
  assert(discover_plus_plus_pandas(table, integer_options).activities.empty());
  integer_options.min_activity_count = 1;
  integer_options.min_frequency = std::numeric_limits<std::uint64_t>::max();
  assert(discover_plus_plus_pandas(table, integer_options).relation_statistics.empty());

  const auto classic_table = pandas_interval_table(2);
  HeuristicMinerOptions classic_options;
  const auto classic = discover_heuristics_net(classic_table, classic_options);
  classic_options.plusplus_min_activity_count = nan;
  classic_options.plusplus_min_frequency = nan;
  const auto classic_with_overrides =
      discover_heuristics_net(classic_table, classic_options);
  const EventLog empty;
  const auto graph = discover_directly_follows_graph(empty);
  assert(analysis_json("classic", empty, graph, nullptr, nullptr, &classic) ==
         analysis_json("classic", empty, graph, nullptr, nullptr,
                       &classic_with_overrides));
}

void plusplus_pandas_literal_selectors_and_error_order() {
  struct ErrorControl {
    std::string activity = "concept:name";
    std::string completed = "time:timestamp";
    std::string case_id = "case:concept:name";
    std::string started = "start_timestamp";
    std::string type = "builtins.KeyError";
    std::string message;
    bool copies_start = false;
    bool object_time = false;
  };
  const std::vector<ErrorControl> errors{
      {"missing:activity", "missing:complete", "missing:case", "missing:start",
       "builtins.KeyError", "'missing:case'"},
      {"missing:activity", "missing:complete", "case:concept:name", "missing:start",
       "builtins.KeyError", "'Column not found: missing:activity'"},
      {"activity", "time:timestamp", "case:concept:name", "start_timestamp",
       "builtins.KeyError", "'Column not found: activity'"},
      {"", "time:timestamp", "case:concept:name", "start_timestamp",
       "builtins.KeyError", "'Column not found: '"},
      {"concept:name", "missing:complete", "case:concept:name", "start_timestamp",
       "builtins.KeyError", "\"['missing:complete'] not in index\""},
      {"concept:name", "missing:complete", "case:concept:name", "missing:start",
       "builtins.KeyError", "'missing:complete'"},
      {"concept:name", "", "case:concept:name", "start_timestamp",
       "builtins.KeyError", "\"[''] not in index\""},
      {"concept:name", "time:timestamp", "", "start_timestamp",
       "builtins.KeyError", "''"},
      {"concept:name", "time:timestamp", "concept:name", "start_timestamp",
       "builtins.KeyError", "'concept:name_2'"},
      {"concept:name", "time:timestamp", "concept:name", "missing:start",
       "builtins.KeyError", "'concept:name_2'", true},
      {"concept:name", "time:timestamp", "concept:name", "missing:start",
       "builtins.AttributeError", "Can only use .dt accessor with datetimelike values", true, true},
      {"missing:activity", "time:timestamp", "case:concept:name", "missing:start",
       "builtins.KeyError", "'Column not found: missing:activity'", false, true}};
  for (const auto& control : errors) {
    auto table = pandas_interval_table(2);
    if (control.object_time) {
      table.dtypes[table.column_index("time:timestamp")] = "object";
      table.dtypes[table.column_index("start_timestamp")] = "object";
    }
    const auto before = table;
    auto options = plus_options();
    options.activity_attribute = control.activity;
    options.timestamp_attribute = control.completed;
    options.case_id_attribute = control.case_id;
    options.start_timestamp_attribute = control.started;
    bool matched = false;
    try { (void)discover_plus_plus_pandas(table, options); }
    catch (const HeuristicPandasError& error) {
      matched = error.reference_type() == control.type && error.what() == control.message;
    }
    assert(matched);
    assert(table.columns.size() == before.columns.size() + control.copies_start);
    assert(std::find(table.columns.begin(), table.columns.end(), "@@diff") == table.columns.end());
    for (std::size_t row = 0; row < table.rows.size(); ++row)
      assert(std::equal(before.rows[row].begin(), before.rows[row].end(), table.rows[row].begin()));
    assert(table.index == before.index && table.attributes == before.attributes);
    assert(table.metadata == before.metadata);
  }

  const auto append = [](EventTable& table, const std::string& key, std::size_t source) {
    table.columns.push_back(key);
    table.dtypes.push_back(table.dtypes[source]);
    for (auto& row : table.rows) {
      auto cell = row[source];
      cell->key = key;
      cell->first_class_role = FirstClassAttributeRole::kNone;
      cell->first_class_state = FirstClassAttributeState::kMissing;
      cell->first_class_source_key.clear();
      row.push_back(std::move(cell));
    }
  };
  for (const auto& key : {std::string{"activity"}, std::string{}}) {
    auto table = pandas_interval_table(2);
    append(table, key, table.column_index("concept:name"));
    for (auto& row : table.rows) row.back()->value = "X:" + row.back()->value;
    auto options = plus_options();
    options.activity_attribute = key;
    const auto net = discover_plus_plus_pandas(table, options);
    assert((net.activities == std::vector<std::string>{"X:A", "X:B", "X:C", "X:D"}));
    assert(net.activity_attribute == key && net.activity_ids_are_local);
  }
  for (const auto& key : {std::string{"timestamp"}, std::string{}}) {
    auto table = pandas_interval_table(2);
    append(table, key, table.column_index("time:timestamp"));
    for (auto& row : table.rows) {
      auto& cell = *row.back();
      const auto second = std::stoi(cell.value.substr(17, 2)) + 20;
      cell.value.replace(17, 2, std::to_string(second));
      std::get<NativeDate>(cell.typed_value).value = cell.value;
    }
    auto options = plus_options();
    options.timestamp_attribute = key;
    const auto net = discover_plus_plus_pandas(table, options);
    assert(net.timestamp_attribute == key);
    assert(net.relation_statistics.empty());
    assert(net.sojourn_times->at("A") == 21.0);
  }
  auto table = pandas_interval_table(2);
  append(table, "", table.column_index("case:concept:name"));
  auto options = plus_options();
  options.case_id_attribute = "";
  assert(discover_plus_plus_pandas(table, options).activities.size() == 4);
  table = pandas_interval_table(2);
  options = plus_options();
  options.start_timestamp_attribute = "";
  const auto copied = discover_plus_plus_pandas(table, options);
  assert(std::find(table.columns.begin(), table.columns.end(), "") != table.columns.end());
  for (const auto& [_, value] : *copied.sojourn_times) assert(value == 0.0);
}

void plusplus_pandas_duration_selector_collisions() {
  const auto append_diff = [](EventTable& table, const std::string& source) {
    const auto column = table.column_index(source);
    table.columns.push_back("@@diff");
    table.dtypes.push_back(table.dtypes[column]);
    for (auto& row : table.rows) {
      auto cell = row[column];
      cell->key = "@@diff";
      cell->first_class_role = FirstClassAttributeRole::kNone;
      cell->first_class_state = FirstClassAttributeState::kMissing;
      cell->first_class_source_key.clear();
      row.push_back(std::move(cell));
    }
  };
  for (const auto& unit : {"us", "ns", "ms"}) {
    for (const int selectors : {1, 2, 3, 4}) {
      auto table = pandas_interval_table(2);
      table.dtypes[table.column_index("time:timestamp")] =
          std::string("datetime64[") + unit + ", UTC]";
      table.dtypes[table.column_index("start_timestamp")] =
          std::string("datetime64[") + unit + ", UTC]";
      if (selectors != 4)
        append_diff(table, selectors == 1 ? "start_timestamp" : "time:timestamp");
      const auto before = table;
      auto options = plus_options();
      if (selectors != 2) options.start_timestamp_attribute = "@@diff";
      if (selectors == 2 || selectors == 3) options.timestamp_attribute = "@@diff";
      bool matched = false;
      try { (void)discover_plus_plus_pandas(table, options); }
      catch (const HeuristicPandasError& error) {
        matched = error.reference_type() == "builtins.AttributeError" &&
                  std::string(error.what()) == "'float' object has no attribute 'timestamp'";
      }
      assert(matched);
      const auto difference = table.column_index("@@diff");
      assert(table.dtypes[difference] == "float64");
      for (std::size_t row = 0; row < table.rows.size(); ++row) {
        assert(std::get<double>(table.rows[row][difference]->typed_value) ==
               (selectors >= 3 ? 0.0 : std::vector<double>{1.0, 4.0, 2.0, 1.0}[row % 4]));
        for (std::size_t column = 0; column < before.columns.size(); ++column)
          if (column != difference) assert(table.rows[row][column] == before.rows[row][column]);
      }
      assert(table.index == before.index && table.attributes == before.attributes);
      assert(table.metadata == before.metadata);
    }
  }
  for (const bool both_times : {false, true}) {
    auto table = pandas_interval_table(2);
    append_diff(table, "start_timestamp");
    for (auto& row : table.rows) row.back() = typed_date("@@diff", "NaT");
    auto options = plus_options();
    options.start_timestamp_attribute = "@@diff";
    if (both_times) options.timestamp_attribute = "@@diff";
    const auto net = discover_plus_plus_pandas(table, options);
    assert(net.activities.size() == 4 && net.relation_statistics.empty());
    for (const auto& [_, value] : *net.sojourn_times) assert(std::isnan(value));
    for (const auto& row : table.rows) assert(std::isnan(std::get<double>(row.back()->typed_value)));
  }
  EventTable empty;
  empty.columns = {"case:concept:name", "concept:name", "time:timestamp", "start_timestamp", "@@diff"};
  empty.dtypes = {"str", "str", "datetime64[us]", "datetime64[us]", "datetime64[us]"};
  auto options = plus_options();
  options.start_timestamp_attribute = "@@diff";
  options.timestamp_attribute = "@@diff";
  assert(discover_plus_plus_pandas(empty, options).activities.empty());
  assert(empty.dtypes.back() == "float64");
  auto table = pandas_interval_table(2);
  table.dtypes[table.column_index("start_timestamp")] = "object";
  const auto before = table;
  bool matched = false;
  try { (void)discover_plus_plus_pandas(table, plus_options()); }
  catch (const HeuristicPandasError& error) {
    matched = error.reference_type() == "builtins.TypeError" &&
              std::string(error.what()) == "cannot subtract DatetimeArray from ndarray[object]";
  }
  assert(matched && table.rows == before.rows && table.columns == before.columns);
}

void interval_abstraction_and_strict_threshold() {
  auto a_start = event("A", "2026-01-01T00:00:00Z", "start");
  attr(a_start, "concept:instance", "a1");
  auto b_start = event("B", "2026-01-01T00:02:00Z", "start");
  attr(b_start, "concept:instance", "b1");
  auto a_complete = event("A", "2026-01-01T00:05:00Z", "complete");
  attr(a_complete, "concept:instance", "a1");
  auto b_complete = event("B", "2026-01-01T00:06:00Z", "complete");
  attr(b_complete, "concept:instance", "b1");
  auto c = interval("C", "2026-01-01T00:06:00Z",
                    "2026-01-01T00:06:00Z");

  auto options = plus_options();
  options.decoration = HeuristicNetDecoration::kPerformance;
  const auto net = discover_heuristics_net(
      log({trace({a_start, b_start, a_complete, b_complete, c})}), options);
  assert(net.variant == HeuristicMinerVariant::kPlusPlus);
  assert(net.decoration == HeuristicNetDecoration::kPerformance);
  assert(net.start_activities == std::vector<std::size_t>{activity(net, "A")});
  assert(net.end_activities == std::vector<std::size_t>{activity(net, "C")});
  assert(std::abs(activity_stat(net, "A").mean_sojourn_seconds - 300.0) <
         1e-9);
  assert(std::abs(activity_stat(net, "B").mean_sojourn_seconds - 240.0) <
         1e-9);
  assert(activity_stat(net, "C").mean_sojourn_seconds == 0.0);

  // First non-overlapping eventually-follow: A skips overlapping B and reaches
  // C. B and C touch, so they are both EFG and inclusively concurrent.
  assert(relation(net, "A", "C").eventually_follows_frequency == 1);
  const auto& bc = relation(net, "B", "C");
  assert(bc.eventually_follows_frequency == 1);
  assert(bc.direct_adjacency_frequency == 1);
  assert(bc.mean_wait_seconds == 0.0);
  assert(edge(net, "A", "C") != nullptr);
  assert(edge(net, "B", "C") == nullptr);  // dependency == default 0.5
  bool saw_ab = false;
  bool saw_bc = false;
  for (const auto& concurrent : net.concurrency_statistics) {
    const auto first = net.activities[concurrent.first];
    const auto second = net.activities[concurrent.second];
    saw_ab |= first == "A" && second == "B" && concurrent.frequency == 1;
    saw_bc |= first == "B" && second == "C" && concurrent.frequency == 1;
  }
  assert(saw_ab && saw_bc);

  // An eventually-follows-only relation has no direct wait sample.  Even for
  // raw/all performance selections it keeps the scalar zero projection.
  const auto efg_only = log({trace({
      interval("A", "2026-01-02T00:00:00Z", "2026-01-02T00:00:05Z"),
      interval("B", "2026-01-02T00:00:01Z", "2026-01-02T00:00:10Z"),
      interval("C", "2026-01-02T00:00:06Z", "2026-01-02T00:00:07Z"),
  })});
  options.dependency_threshold = 0.0;
  options.performance_aggregation =
      HeuristicPerformanceAggregation::kRawValues;
  const auto raw_efg = discover_heuristics_net(efg_only, options);
  const auto* raw_efg_edge = edge(raw_efg, "A", "C");
  assert(raw_efg_edge != nullptr);
  assert(raw_efg_edge->representation == 0.0);
  assert(!raw_efg_edge->representation_value.has_value());
  options.performance_aggregation = HeuristicPerformanceAggregation::kAll;
  const auto all_efg = discover_heuristics_net(efg_only, options);
  const auto* all_efg_edge = edge(all_efg, "A", "C");
  assert(all_efg_edge != nullptr);
  assert(all_efg_edge->representation == 0.0);
  assert(!all_efg_edge->representation_value.has_value());

  options.dependency_threshold = 0.49;
  const auto relaxed = discover_heuristics_net(
      log({trace({a_start, b_start, a_complete, b_complete, c})}), options);
  assert(edge(relaxed, "B", "C") != nullptr);
  assert(std::abs(edge(relaxed, "B", "C")->dependency - 0.5) < 1e-9);
}

void plusplus_frequency_representation_preserves_integer_type() {
  // Two independently specified waits (3 and 7 seconds) distinguish the
  // frequency count from the performance mean without changing the topology.
  const auto source = log({
      trace({interval("A", "2026-04-01T00:00:00Z", "2026-04-01T00:00:01Z"),
             interval("B", "2026-04-01T00:00:04Z", "2026-04-01T00:00:05Z")}),
      trace({interval("A", "2026-04-02T00:00:00Z", "2026-04-02T00:00:01Z"),
             interval("B", "2026-04-02T00:00:08Z", "2026-04-02T00:00:09Z")}),
  });
  auto options = plus_options();
  const auto frequency = discover_heuristics_net(source, options);
  assert(frequency.activities == (std::vector<std::string>{"A", "B"}));
  assert(frequency.relation_statistics.size() == 1);
  assert(frequency.edges.size() == 1);
  assert(frequency.concurrency_statistics.empty());
  const auto& frequency_relation = relation(frequency, "A", "B");
  const auto* frequency_edge = edge(frequency, "A", "B");
  assert(frequency_edge != nullptr);
  assert(frequency_edge->dependency == 1.0);
  assert(frequency_relation.representation == 2.0);
  assert(frequency_edge->representation == 2.0);
  for (const auto* value : {&frequency_relation.representation_value,
                            &frequency_edge->representation_value}) {
    assert(value->has_value());
    assert(std::holds_alternative<HeuristicStructuredValue>(**value));
    const auto& scalar = std::get<HeuristicStructuredValue>(**value).value;
    assert(std::holds_alternative<std::uint64_t>(scalar));
    assert(std::get<std::uint64_t>(scalar) == std::uint64_t{2});
  }

  // The strict threshold removes the accepted edge but retains the typed
  // relation value, including when no outgoing edge can carry the count.
  options.dependency_threshold = 1.0;
  const auto excluded = discover_heuristics_net(source, options);
  assert(excluded.edges.empty());
  assert(excluded.relation_statistics.size() == 1);
  const auto& excluded_relation = relation(excluded, "A", "B");
  assert(excluded_relation.representation == 2.0);
  assert(excluded_relation.representation_value.has_value());
  assert(std::holds_alternative<HeuristicStructuredValue>(
      *excluded_relation.representation_value));
  const auto& excluded_scalar = std::get<HeuristicStructuredValue>(
      *excluded_relation.representation_value).value;
  assert(std::holds_alternative<std::uint64_t>(excluded_scalar));
  assert(std::get<std::uint64_t>(excluded_scalar) == std::uint64_t{2});

  options.dependency_threshold = 0.5;
  options.decoration = HeuristicNetDecoration::kPerformance;
  options.performance_aggregation = HeuristicPerformanceAggregation::kMean;
  const auto performance = discover_heuristics_net(source, options);
  assert(performance.activities == frequency.activities);
  assert(performance.edges.size() == 1);
  assert(performance.relation_statistics.size() == 1);
  assert(performance.concurrency_statistics.empty());
  assert(performance.output_bundles == frequency.output_bundles);
  assert(performance.input_bundles == frequency.input_bundles);
  const auto& performance_relation = relation(performance, "A", "B");
  const auto* performance_edge = edge(performance, "A", "B");
  assert(performance_edge != nullptr);
  assert(performance_edge->dependency == 1.0);
  assert((performance_relation.wait_samples == std::vector<double>{3.0, 7.0}));
  static_assert(std::is_same_v<decltype(performance_relation.representation),
                               double>);
  static_assert(std::is_same_v<decltype(performance_edge->representation),
                               double>);
  assert(performance_relation.representation == 5.0);
  assert(performance_edge->representation == 5.0);
  assert(!performance_relation.representation_value.has_value());
  assert(!performance_edge->representation_value.has_value());
}

void lifecycle_fifo_and_configurable_interval_attribute() {
  auto s1 = event("A", "2026-01-01T00:00:00Z", "START");
  auto s2 = event("A", "2026-01-01T00:00:01Z", "start");
  auto c1 = event("A", "2026-01-01T00:00:02Z", "complete");
  auto c2 = event("A", "2026-01-01T00:00:04Z", "COMPLETE");
  for (auto* e : {&s1, &s2, &c1, &c2}) attr(*e, "instance", "same");
  auto atomic = interval("B", "2026-01-01T00:00:10Z",
                         "2026-01-01T00:00:13Z", "begin");
  auto options = plus_options();
  options.lifecycle_instance_attribute = "instance";
  options.start_timestamp_attribute = "begin";
  options.dependency_threshold = -1.0;
  const auto net = discover_heuristics_net(
      log({trace({s1, s2, c1, c2, atomic})}), options);
  // FIFO durations are 2 and 3 seconds.
  assert(std::abs(activity_stat(net, "A").mean_sojourn_seconds - 2.5) <
         1e-9);
  // In lifecycle-conversion mode, an unmatched complete is atomic and any
  // pre-existing start attribute is overwritten by the completion time.
  assert(activity_stat(net, "B").mean_sojourn_seconds == 0.0);

  auto pre_interval = interval("B", "2026-01-01T00:00:10Z",
                               "2026-01-01T00:00:13Z", "begin");
  const auto interval_net = discover_heuristics_net(
      log({trace({pre_interval})}), options);
  assert(std::abs(activity_stat(interval_net, "B").mean_sojourn_seconds -
                  3.0) < 1e-9);

  auto missing = event("C", "2026-01-01T00:00:14Z");
  bool threw = false;
  try {
    (void)discover_heuristics_net(
        log({trace({pre_interval, missing})}), options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void lifecycle_input_shapes_defer_ignored_timestamp_parsing() {
  auto options = plus_options();
  options.decoration = HeuristicNetDecoration::kPerformance;
  options.use_business_hours = true;

  auto unknown_bad_time = event("X", "garbage", "schedule");
  unknown_bad_time.timestamp_state = FirstClassAttributeState::kPresent;
  auto unknown_empty_time = event("Y", "", "schedule");
  unknown_empty_time.timestamp_state = FirstClassAttributeState::kPresent;
  const auto valid = log({trace({
      unknown_bad_time,
      unknown_empty_time,
      event("A", "2026-01-05T10:00:00Z"),
      event("B", "2026-01-05T12:00:00Z"),
  })});
  const auto ignored = discover_heuristics_net(valid, options);
  assert(std::abs(relation(ignored, "A", "B").mean_wait_seconds -
                  7200.0) < 1e-9);

  auto missing_time = event("X", "", "schedule");
  bool missing_time_threw = false;
  try {
    (void)discover_heuristics_net(
        log({trace({missing_time, event("A", "2026-01-05T10:00:00Z")})}),
        options);
  } catch (const std::invalid_argument&) {
    missing_time_threw = true;
  }
  assert(missing_time_threw);

  auto unmatched_bad_start = event("X", "garbage", "start");
  unmatched_bad_start.timestamp_state = FirstClassAttributeState::kPresent;
  const auto unmatched = discover_heuristics_net(
      log({trace({unmatched_bad_start, event("A", "2026-01-05T10:00:00Z"),
                  event("B", "2026-01-05T12:00:00Z")})}),
      options);
  assert(std::abs(relation(unmatched, "A", "B").mean_wait_seconds -
                  7200.0) < 1e-9);

  auto paired_bad_start = event("A", "garbage", "start");
  paired_bad_start.timestamp_state = FirstClassAttributeState::kPresent;
  bool paired_start_threw = false;
  try {
    (void)discover_heuristics_net(
        log({trace({paired_bad_start,
                    event("A", "2026-01-05T10:00:00Z"),
                    event("B", "2026-01-05T12:00:00Z")})}),
        options);
  } catch (const std::invalid_argument&) {
    paired_start_threw = true;
  }
  assert(paired_start_threw);

  auto explicit_empty_lifecycle = event("A", "garbage", "");
  explicit_empty_lifecycle.timestamp_state =
      FirstClassAttributeState::kPresent;
  explicit_empty_lifecycle.lifecycle_state =
      FirstClassAttributeState::kPresent;
  const auto empty_lifecycle = discover_heuristics_net(
      log({trace({explicit_empty_lifecycle,
                  event("B", "2026-01-05T12:00:00Z")})}),
      options);
  assert((empty_lifecycle.activities == std::vector<std::string>{"B"}));
  assert(empty_lifecycle.relation_statistics.empty());

  // A missing lifecycle field is the atomic/completion form and still parses
  // its timestamp, unlike the explicit empty lifecycle above.
  const auto omitted_lifecycle = discover_heuristics_net(
      log({trace({event("A", "2026-01-05T10:00:00Z"),
                  event("B", "2026-01-05T12:00:00Z")})}),
      options);
  assert(std::abs(relation(omitted_lifecycle, "A", "B").mean_wait_seconds -
                  7200.0) < 1e-9);

  auto custom_unknown = event("X", "", "schedule");
  attr(custom_unknown, "occurred", "garbage");
  auto custom_a = event("A", "");
  attr(custom_a, "occurred", "2026-01-05T10:00:00Z");
  auto custom_b = event("B", "");
  attr(custom_b, "occurred", "2026-01-05T12:00:00Z");
  auto custom_options = options;
  custom_options.timestamp_attribute = "occurred";
  const auto custom = discover_heuristics_net(
      log({trace({custom_unknown, custom_a, custom_b})}), custom_options);
  assert(std::abs(relation(custom, "A", "B").mean_wait_seconds -
                  7200.0) < 1e-9);

  const auto custom_missing = event("X", "", "schedule");
  bool custom_missing_threw = false;
  try {
    (void)discover_heuristics_net(
        log({trace({custom_missing, custom_a, custom_b})}), custom_options);
  } catch (const std::invalid_argument&) {
    custom_missing_threw = true;
  }
  assert(custom_missing_threw);
}

void plusplus_returned_object_fields_acceptance() {
  // Saved public local-label rows 0_False_{concept:name,task}_{1,2,10}_
  // PLUSPLUS_frequency retain every raw three-second sojourn. Filtering Z or
  // all nodes does not filter that returned map; shape 4 exposes an empty map.
  // The full public replay also observes false start/end flags on every node,
  // including nodes whose actual start/end occurrence counts are positive.
  const std::string selected_a = "A\"\\";
  const std::string selected_b = "B\n";
  const std::string selected_z = "Z\xce\xa9";
  auto selected_interval = [](std::string canonical, std::string selected,
                              std::string start, std::string complete) {
    auto result = interval(std::move(canonical), std::move(start),
                           std::move(complete));
    attr(result, "task", std::move(selected));
    return result;
  };
  auto a = [&] {
    return selected_interval("canonical_A", selected_a,
                             "2026-01-01T00:00:00Z", "2026-01-01T00:00:03Z");
  };
  auto b = [&] {
    return selected_interval("canonical_B", selected_b,
                             "2026-01-01T00:00:05Z", "2026-01-01T00:00:08Z");
  };
  const auto source = log({
      trace({a(), b()}), trace({a(), b()}),
      trace({selected_interval("canonical_Z", selected_z,
                               "2026-01-01T00:00:00Z",
                               "2026-01-01T00:00:03Z")}),
  });
  const std::map<std::string, double> canonical_expected{
      {"canonical_A", 3.0}, {"canonical_B", 3.0}, {"canonical_Z", 3.0}};
  const std::map<std::string, double> selected_expected{
      {selected_a, 3.0}, {selected_b, 3.0}, {selected_z, 3.0}};
  auto options = plus_options();
  const auto ordinary = discover_heuristics_net(source, options);
  assert(ordinary.sojourn_times.has_value());
  assert(*ordinary.sojourn_times == canonical_expected);
  assert(activity_stat(ordinary, "canonical_A").start_occurrences == 2);
  assert(activity_stat(ordinary, "canonical_B").end_occurrences == 2);
  for (const auto& statistic : ordinary.activity_statistics) {
    assert(statistic.is_start_activity.has_value());
    assert(statistic.is_end_activity.has_value());
    assert(!*statistic.is_start_activity);
    assert(!*statistic.is_end_activity);
  }
  assert((ordinary.activities == std::vector<std::string>{
                                     "canonical_A", "canonical_B", "canonical_Z"}));
  const auto graph = discover_directly_follows_graph(source);
  const auto ordinary_json = analysis_json("plusplus-sojourn-canonical", source,
                                           graph, nullptr, nullptr, &ordinary);
  assert(ordinary_json.find(
             "\"sojourn_times\": {\"canonical_A\": 3, \"canonical_B\": 3, "
             "\"canonical_Z\": 3}") != std::string::npos);

  options.activity_attribute = "task";
  const std::string selected_wire =
      R"json("sojourn_times": {"A\"\\": 3, "B\n": 3, ")json" +
      selected_z + R"json(": 3})json";
  for (const auto decoration : {HeuristicNetDecoration::kFrequency,
                                HeuristicNetDecoration::kPerformance}) {
    options.decoration = decoration;
    for (const std::uint64_t minimum : {1, 2, 10}) {
      options.min_activity_count = minimum;
      const auto net = discover_heuristics_net(source, options);
      assert(net.activity_ids_are_local);
      assert(net.sojourn_times.has_value());
      assert(*net.sojourn_times == selected_expected);
      for (const auto& statistic : net.activity_statistics) {
        assert(statistic.is_start_activity.has_value());
        assert(statistic.is_end_activity.has_value());
        assert(!*statistic.is_start_activity);
        assert(!*statistic.is_end_activity);
      }
      if (minimum == 1) {
        assert((net.activities == std::vector<std::string>{
                                      selected_a, selected_b, selected_z}));
      } else if (minimum == 2) {
        assert((net.activities ==
                std::vector<std::string>{selected_a, selected_b}));
        assert(net.sojourn_times->at(selected_z) == 3.0);
      } else {
        assert(net.activities.empty());
        assert(net.active_activities.empty());
        assert(net.edges.empty());
      }
      const auto wire = analysis_json("plusplus-sojourn-selected", source,
                                      graph, nullptr, nullptr, &net);
      assert(wire.find(selected_wire) != std::string::npos);
      if (minimum != 10) {
        assert(wire.find("\"is_start_activity\": false") != std::string::npos);
        assert(wire.find("\"is_end_activity\": false") != std::string::npos);
      }
    }
  }
  const auto empty_source = log({});
  const auto empty = discover_heuristics_net(empty_source, options);
  assert(empty.sojourn_times.has_value());
  assert(empty.sojourn_times->empty());
  const auto empty_graph = discover_directly_follows_graph(empty_source);
  const auto empty_json = analysis_json("plusplus-sojourn-empty", empty_source,
                                        empty_graph, nullptr, nullptr, &empty);
  assert(empty_json.find("\"sojourn_times\": {}") != std::string::npos);
}

void filtering_and_unfiltered_statistics() {
  const auto source = log({
      trace({interval("A", "2026-01-01T00:00:00Z",
                      "2026-01-01T00:00:00Z"),
             interval("B", "2026-01-01T00:00:01Z",
                      "2026-01-01T00:00:01Z")}),
      trace({interval("A", "2026-01-02T00:00:00Z",
                      "2026-01-02T00:00:00Z"),
             interval("B", "2026-01-02T00:00:01Z",
                      "2026-01-02T00:00:01Z")}),
      trace({interval("A", "2026-01-03T00:00:00Z",
                      "2026-01-03T00:00:00Z"),
             interval("C", "2026-01-03T00:00:01Z",
                      "2026-01-03T00:00:02Z")}),
  });
  auto options = plus_options();
  options.min_frequency = 2;
  const auto path_filtered = discover_heuristics_net(source, options);
  assert(path_filtered.relation_statistics.size() == 1);
  assert(relation(path_filtered, "A", "B").eventually_follows_frequency == 2);
  assert(edge(path_filtered, "A", "B") != nullptr);
  assert(edge(path_filtered, "A", "C") == nullptr);

  options.min_activity_count = 2;
  const auto activity_filtered = discover_heuristics_net(source, options);
  assert(activity_filtered.activities.size() == 2);
  assert(activity_filtered.raw_activity_statistics.size() == 3);
  bool retained_c_sojourn = false;
  for (const auto& stat : activity_filtered.raw_activity_statistics)
    retained_c_sojourn |=
        stat.activity == "C" && stat.mean_sojourn_seconds == 1.0;
  assert(retained_c_sojourn);

  const auto reverse_noise = log({
      trace({interval("A", "2026-02-01T00:00:00Z",
                      "2026-02-01T00:00:00Z"),
             interval("B", "2026-02-01T00:00:01Z",
                      "2026-02-01T00:00:01Z")}),
      trace({interval("A", "2026-02-02T00:00:00Z",
                      "2026-02-02T00:00:00Z"),
             interval("B", "2026-02-02T00:00:01Z",
                      "2026-02-02T00:00:01Z")}),
      trace({interval("B", "2026-02-03T00:00:00Z",
                      "2026-02-03T00:00:00Z"),
             interval("A", "2026-02-03T00:00:01Z",
                      "2026-02-03T00:00:01Z")}),
  });
  options.min_activity_count = 1;
  options.dependency_threshold = 0.9;
  const auto reverse_filtered =
      discover_heuristics_net(reverse_noise, options);
  // The one-count reverse path is removed before dependency calculation.
  assert(std::abs(edge(reverse_filtered, "A", "B")->dependency - 1.0) <
         1e-9);
}

void dependency_concurrency_and_no_connect_all() {
  const auto source = log({
      trace({interval("A", "2026-01-01T00:00:00Z",
                      "2026-01-01T00:00:00Z"),
             interval("B", "2026-01-01T00:00:01Z",
                      "2026-01-01T00:00:01Z")}),
      trace({interval("A", "2026-01-02T00:00:00Z",
                      "2026-01-02T00:00:02Z"),
             interval("B", "2026-01-02T00:00:01Z",
                      "2026-01-02T00:00:03Z")}),
  });
  auto options = plus_options();
  options.dependency_threshold = 0.49;
  auto net = discover_heuristics_net(source, options);
  assert(std::abs(edge(net, "A", "B")->dependency - 0.5) < 1e-9);
  options.dependency_threshold = 0.5;
  options.connect_all = true;
  net = discover_heuristics_net(source, options);
  assert(edge(net, "A", "B") == nullptr);

  const auto self_source =
      log({trace({interval("A", "2026-05-01T00:00:00Z",
                           "2026-05-01T00:00:00Z"),
                  interval("A", "2026-05-01T00:00:01Z",
                           "2026-05-01T00:00:01Z"),
                  interval("B", "2026-05-01T00:00:02Z",
                           "2026-05-01T00:00:02Z")})});
  options.dependency_threshold = -0.1;
  options.and_threshold = 0.4;
  net = discover_heuristics_net(self_source, options);
  const auto a = activity(net, "A");
  const auto b = activity(net, "B");
  assert(edge(net, "A", "A") != nullptr);
  assert((net.output_bundles[a] == std::vector<HeuristicBundle>{{a, b}}));
}

void and_bundles_are_overlapping_maximal_cliques() {
  const auto zero = "2026-01-01T00:00:00Z";
  const auto one = "2026-01-01T00:00:01Z";
  const auto two = "2026-01-01T00:00:02Z";
  const auto source = log({
      trace({interval("A", zero, zero), interval("B", one, one)}),
      trace({interval("A", zero, zero), interval("C", one, one)}),
      trace({interval("A", zero, zero), interval("D", one, one)}),
      trace({interval("B", zero, two), interval("C", one, two)}),
      trace({interval("C", zero, two), interval("D", one, two)}),
  });
  auto options = plus_options();
  options.dependency_threshold = 0.4;
  options.and_threshold = 0.4;
  const auto net = discover_heuristics_net(source, options);
  const auto a = activity(net, "A");
  const auto b = activity(net, "B");
  const auto c = activity(net, "C");
  const auto d = activity(net, "D");
  assert(net.output_bundles[a].size() == 2);
  assert((net.output_bundles[a][0] == HeuristicBundle{b, c}));
  assert((net.output_bundles[a][1] == HeuristicBundle{c, d}));

  options.and_threshold = 0.5;
  const auto strict = discover_heuristics_net(source, options);
  const auto strict_a = activity(strict, "A");
  assert(strict.output_bundles[strict_a].size() == 3);
  for (const auto& bundle : strict.output_bundles[strict_a])
    assert(bundle.size() == 1);
}

void performance_sorting_and_mean() {
  // EFG uses a sorted copy, while a pre-interval log's performance DFG keeps
  // the original order. The reversed second trace therefore contributes to
  // EFG A->B but not to its direct-adjacency performance.
  const auto source = log({
      trace({interval("A", "2026-01-01T00:00:00Z",
                      "2026-01-01T00:00:01Z"),
             interval("B", "2026-01-01T00:00:06Z",
                      "2026-01-01T00:00:06Z")}),
      trace({interval("B", "2026-01-02T00:00:16Z",
                      "2026-01-02T00:00:16Z"),
             interval("A", "2026-01-02T00:00:00Z",
                      "2026-01-02T00:00:01Z")}),
  });
  auto options = plus_options();
  options.decoration = HeuristicNetDecoration::kPerformance;
  const auto net = discover_heuristics_net(source, options);
  const auto& ab = relation(net, "A", "B");
  assert(ab.eventually_follows_frequency == 2);
  assert(ab.direct_adjacency_frequency == 1);
  assert(std::abs(ab.mean_wait_seconds - 5.0) < 1e-9);

  const auto aggregation_source = log({
      trace({interval("A", "2026-04-01T00:00:00Z",
                      "2026-04-01T00:00:01Z"),
             interval("B", "2026-04-01T00:00:06Z",
                      "2026-04-01T00:00:06Z")}),
      trace({interval("A", "2026-04-02T00:00:00Z",
                      "2026-04-02T00:00:01Z"),
             interval("B", "2026-04-02T00:00:16Z",
                      "2026-04-02T00:00:16Z")}),
  });
  options.performance_aggregation =
      HeuristicPerformanceAggregation::kMaximum;
  const auto maximum =
      discover_heuristics_net(aggregation_source, options);
  assert(edge(maximum, "A", "B")->representation == 15.0);
  options.performance_aggregation = HeuristicPerformanceAggregation::kSum;
  const auto sum = discover_heuristics_net(aggregation_source, options);
  assert(edge(sum, "A", "B")->representation == 20.0);
  options.performance_aggregation =
      HeuristicPerformanceAggregation::kRawValues;
  const auto raw = discover_heuristics_net(aggregation_source, options);
  assert((relation(raw, "A", "B").wait_samples ==
          std::vector<double>{5.0, 15.0}));
}

void invalid_timestamps_throw_only_for_plus_plus() {
  const auto source = log({trace({event("A", "not-a-time")})});
  auto options = plus_options();
  bool threw = false;
  try {
    (void)discover_heuristics_net(source, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  // Classic's established untimed behavior is unchanged.
  const auto classic = discover_heuristics_net(source);
  assert(classic.variant == HeuristicMinerVariant::kClassic);
  assert(classic.activities == std::vector<std::string>{"A"});
}

HeuristicClassicDfgInput classic_input(
    std::initializer_list<
        std::pair<const std::pair<std::string, std::string>, std::uint64_t>>
        edges) {
  HeuristicClassicDfgInput input;
  input.dfg = HeuristicFrequencyDfg(edges);
  return input;
}

void and_measure_payload_matches_threshold_boundaries() {
  auto classic_input_value = classic_input(
      {{{"A", "B"}, 2}, {{"A", "C"}, 2}, {{"B", "C"}, 3}});
  classic_input_value.activities = std::vector<std::string>{"A", "B", "C"};
  HeuristicMinerOptions classic_options;
  classic_options.dependency_threshold = 0.0;
  classic_options.dfg_pre_cleaning_noise_threshold = 0.0;
  classic_options.and_threshold = 0.6;
  const auto classic_at_boundary =
      discover_heuristics_net(classic_input_value, classic_options);
  const auto* classic_measure =
      and_measure(classic_at_boundary, true, "A", "B", "C");
  assert(classic_measure != nullptr);
  assert(std::abs(classic_measure->value - 0.6) < 1e-12);

  classic_options.and_threshold = 0.600001;
  const auto classic_above_boundary =
      discover_heuristics_net(classic_input_value, classic_options);
  assert(and_measure(classic_above_boundary, true, "A", "B", "C") ==
         nullptr);

  auto zero_input = classic_input({{{"A", "B"}, 1}, {{"A", "C"}, 1}});
  zero_input.activities = std::vector<std::string>{"A", "B", "C"};
  classic_options.and_threshold = 0.0;
  const auto classic_zero =
      discover_heuristics_net(zero_input, classic_options);
  const auto* zero_measure =
      and_measure(classic_zero, true, "A", "B", "C");
  assert(zero_measure != nullptr);
  assert(zero_measure->value == 0.0);

  // PLUSPLUS retains only the first eventually-following activity, so use
  // two-event traces to make A>B=3, A>C=2, and B>C=3 exactly.
  const auto plus_source = log({
      trace({interval("A", "2026-07-01T00:00:00Z",
                      "2026-07-01T00:00:00Z"),
             interval("B", "2026-07-01T00:00:01Z",
                      "2026-07-01T00:00:01Z")}),
      trace({interval("A", "2026-07-02T00:00:00Z",
                      "2026-07-02T00:00:00Z"),
             interval("B", "2026-07-02T00:00:01Z",
                      "2026-07-02T00:00:01Z")}),
      trace({interval("A", "2026-07-03T00:00:00Z",
                      "2026-07-03T00:00:00Z"),
             interval("B", "2026-07-03T00:00:01Z",
                      "2026-07-03T00:00:01Z")}),
      trace({interval("A", "2026-07-04T00:00:00Z",
                      "2026-07-04T00:00:00Z"),
             interval("C", "2026-07-04T00:00:01Z",
                      "2026-07-04T00:00:01Z")}),
      trace({interval("A", "2026-07-05T00:00:00Z",
                      "2026-07-05T00:00:00Z"),
             interval("C", "2026-07-05T00:00:01Z",
                      "2026-07-05T00:00:01Z")}),
      trace({interval("B", "2026-07-06T00:00:00Z",
                      "2026-07-06T00:00:00Z"),
             interval("C", "2026-07-06T00:00:01Z",
                      "2026-07-06T00:00:01Z")}),
      trace({interval("B", "2026-07-07T00:00:00Z",
                      "2026-07-07T00:00:00Z"),
             interval("C", "2026-07-07T00:00:01Z",
                      "2026-07-07T00:00:01Z")}),
      trace({interval("B", "2026-07-08T00:00:00Z",
                      "2026-07-08T00:00:00Z"),
             interval("C", "2026-07-08T00:00:01Z",
                      "2026-07-08T00:00:01Z")}),
  });
  auto plus_options_at_boundary = plus_options();
  plus_options_at_boundary.dependency_threshold = 0.0;
  plus_options_at_boundary.and_threshold = 0.6;
  const auto plus_at_boundary =
      discover_heuristics_net(plus_source, plus_options_at_boundary);
  assert(and_measure(plus_at_boundary, true, "A", "B", "C") == nullptr);

  plus_options_at_boundary.and_threshold = 0.599999;
  const auto plus_below_boundary =
      discover_heuristics_net(plus_source, plus_options_at_boundary);
  const auto* plus_measure =
      and_measure(plus_below_boundary, true, "A", "B", "C");
  assert(plus_measure != nullptr);
  assert(std::abs(plus_measure->value - 0.6) < 1e-12);
}

void dependency_and_loop_payloads_keep_filtered_values() {
  auto classic = classic_input(
      {{{"A", "B"}, 1}, {{"B", "A"}, 3}});
  classic.activities = std::vector<std::string>{"A", "B"};
  HeuristicMinerOptions classic_options;
  classic_options.dependency_threshold = 0.9;
  classic_options.dfg_pre_cleaning_noise_threshold = 0.0;
  const auto classic_rejected =
      discover_heuristics_net(classic, classic_options);
  assert(classic_rejected.edges.empty());
  assert(std::abs(dependency_measure(classic_rejected, "A", "B")->value +
                  0.4) <
         1e-12);
  assert(std::abs(dependency_measure(classic_rejected, "B", "A")->value -
                  0.4) <
         1e-12);

  auto filtered_loop = classic_input({{{"A", "B"}, 1},
                                      {{"B", "A"}, 1},
                                      {{"A", "C"}, 10}});
  filtered_loop.activities =
      std::vector<std::string>{"A", "B", "C"};
  filtered_loop.activity_counts = std::map<std::string, std::uint64_t>{
      {"A", 12}, {"B", 1}, {"C", 10}};
  filtered_loop.frequency_triples[{"A", "B", "A"}] = 1;
  classic_options.dependency_threshold = 0.0;
  classic_options.min_frequency = 2;
  classic_options.min_activity_count = 3;
  classic_options.loop_length_two_threshold = 0.5;
  const auto filtered =
      discover_heuristics_net(filtered_loop, classic_options);
  assert(loop_length_two(filtered, "A", "B") != nullptr);
  assert(loop_length_two(filtered, "A", "B")->frequency == 1);
  assert(edge(filtered, "A", "B") == nullptr);

  auto zero_loop = filtered_loop;
  zero_loop.dfg[{"B", "D"}] = 10;
  zero_loop.activities =
      std::vector<std::string>{"A", "B", "C", "D"};
  zero_loop.activity_counts = std::map<std::string, std::uint64_t>{
      {"A", 12}, {"B", 11}, {"C", 10}, {"D", 10}};
  classic_options.min_frequency = 1;
  classic_options.min_activity_count = 1;
  classic_options.dfg_pre_cleaning_noise_threshold = 0.9;
  const auto zero = discover_heuristics_net(zero_loop, classic_options);
  assert(loop_length_two(zero, "A", "B") != nullptr);
  assert(loop_length_two(zero, "A", "B")->frequency == 0);
  assert(edge(zero, "A", "B") == nullptr);
  assert(loop_length_two(zero, "B", "A") == nullptr);

  const auto plus_source = log({
      trace({interval("A", "2026-08-01T00:00:00Z",
                      "2026-08-01T00:00:00Z"),
             interval("B", "2026-08-01T00:00:01Z",
                      "2026-08-01T00:00:01Z")}),
      trace({interval("B", "2026-08-02T00:00:00Z",
                      "2026-08-02T00:00:00Z"),
             interval("A", "2026-08-02T00:00:01Z",
                      "2026-08-02T00:00:01Z")}),
      trace({interval("B", "2026-08-03T00:00:00Z",
                      "2026-08-03T00:00:00Z"),
             interval("A", "2026-08-03T00:00:01Z",
                      "2026-08-03T00:00:01Z")}),
      trace({interval("B", "2026-08-04T00:00:00Z",
                      "2026-08-04T00:00:00Z"),
             interval("A", "2026-08-04T00:00:01Z",
                      "2026-08-04T00:00:01Z")}),
  });
  auto plus_options_value = plus_options();
  plus_options_value.dependency_threshold = 0.9;
  const auto plus_rejected =
      discover_heuristics_net(plus_source, plus_options_value);
  assert(plus_rejected.edges.empty());
  assert(std::abs(dependency_measure(plus_rejected, "A", "B")->value +
                  0.5) <
         1e-12);
  assert(std::abs(dependency_measure(plus_rejected, "B", "A")->value -
                  0.5) <
         1e-12);
  assert(plus_rejected.loop_length_two.empty());
}

void classic_dfg_metadata_registry_and_lookup_order() {
  auto input = classic_input({{{"A", "B"}, 1}});
  input.activities = std::vector<std::string>{"A"};
  HeuristicMinerOptions options;
  options.dfg_pre_cleaning_noise_threshold = 0.0;

  // Foreign DFG endpoints are retained in the local id table when noise
  // cleaning is disabled, while metadata keeps only the declared names and
  // inferred boundary lists.
  auto net = discover_heuristics_net(input, options);
  assert(net.activity_ids_are_local);
  assert((net.activities == std::vector<std::string>{"A", "B"}));
  assert(net.metadata.has_value());
  assert((net.metadata->activities == std::vector<std::string>{"A"}));
  assert((net.metadata->activities_occurrences ==
          std::map<std::string, HeuristicOccurrenceCount>{{"A", 1}}));
  assert(std::holds_alternative<std::vector<std::string>>(
      net.metadata->start_activities));
  assert(std::get<std::vector<std::string>>(net.metadata->start_activities) ==
         std::vector<std::string>{"A"});
  assert(std::get<std::vector<std::string>>(net.metadata->end_activities) ==
         std::vector<std::string>{"B"});
  assert(net.active_activities.size() == 1);
  assert(net.activities[net.active_activities.front()] == "A");

  // New boundary variants take precedence and preserve their raw shape,
  // including duplicate list entries and explicit zero map counts.
  input.start_activities = HeuristicBoundaryActivities{
      std::vector<std::string>{"B", "A", "B"}};
  input.end_activities = HeuristicBoundaryActivities{
      std::map<std::string, std::uint64_t>{{"B", 0}, {"A", 2}}};
  net = discover_heuristics_net(input, options);
  assert(std::get<std::vector<std::string>>(net.metadata->start_activities) ==
         std::vector<std::string>({"B", "A", "B"}));
  const std::map<std::string, std::uint64_t> expected_end_counts{
      {"A", 2}, {"B", 0}};
  assert((std::get<std::map<std::string, std::uint64_t>>(
             net.metadata->end_activities) ==
         expected_end_counts));

  // A dependency or frequency rejection skips occurrence lookup. Missing
  // counts simply prevent an edge from being accepted; declared names become
  // strict only when the no-edge fallback validates them.
  auto empty_declared = classic_input({{{"A", "B"}, 1}});
  empty_declared.activities = std::vector<std::string>{};
  empty_declared.activity_counts = std::map<std::string, std::uint64_t>{};
  auto rejected = options;
  rejected.dependency_threshold = 1.0;
  const auto no_nodes = discover_heuristics_net(empty_declared, rejected);
  assert(no_nodes.active_activities.empty());
  const auto no_nodes_petri = to_petri_net(no_nodes);
  assert(no_nodes_petri.transitions().empty());
  rejected.dependency_threshold = 0.5;
  rejected.min_frequency = 2;
  (void)discover_heuristics_net(empty_declared, rejected);
  rejected.min_frequency = 1;
  (void)discover_heuristics_net(empty_declared, rejected);

  auto missing_declared_source = classic_input({{{"A", "B"}, 1}});
  missing_declared_source.activities = std::vector<std::string>{"A"};
  missing_declared_source.activity_counts =
      std::map<std::string, std::uint64_t>{};
  bool threw = false;
  try {
    (void)discover_heuristics_net(missing_declared_source, rejected);
  } catch (const std::exception&) {
    threw = true;
  }
  assert(threw);

  auto missing_fallback = classic_input({{{"A", "B"}, 1}});
  missing_fallback.activities =
      std::vector<std::string>{"A", "B", "Z"};
  missing_fallback.activity_counts =
      std::map<std::string, std::uint64_t>{{"A", 1}, {"B", 1}};
  auto fallback_options = options;
  fallback_options.dependency_threshold = 1.0;
  threw = false;
  try {
    (void)discover_heuristics_net(missing_fallback, fallback_options);
  } catch (const std::exception&) {
    threw = true;
  }
  assert(threw);
}

void classic_defaults_dependency_filters_and_self_loop() {
  auto input = classic_input({{{"A", "B"}, 1}});
  auto net = discover_heuristics_net(input);
  assert(net.variant == HeuristicMinerVariant::kClassic);
  assert(edge(net, "A", "B") != nullptr);
  assert(edge(net, "A", "B")->dependency == 0.5);
  assert(!HeuristicMinerOptions{}.connect_all);

  HeuristicMinerOptions options;
  options.dependency_threshold = 0.500001;
  net = discover_heuristics_net(input, options);
  assert(net.edges.empty());
  assert(net.active_activities.size() == 2);  // no-node fallback

  input = classic_input({{{"A", "A"}, 2}});
  net = discover_heuristics_net(input);
  assert(edge(net, "A", "A") != nullptr);
  assert(std::abs(edge(net, "A", "A")->dependency - 2.0 / 3.0) < 1e-9);

  input = classic_input({{{"A", "B"}, 2}});
  input.activities = std::vector<std::string>{"A", "B", "C"};
  input.activity_counts =
      std::map<std::string, std::uint64_t>{{"A", 2}, {"B", 2}, {"C", 1}};
  options = {};
  options.min_activity_count = 3;
  assert(discover_heuristics_net(input, options).edges.empty());
  options = {};
  options.min_frequency = 3;
  assert(discover_heuristics_net(input, options).edges.empty());
}

void classic_noise_cleaning_and_keep_once() {
  auto make_noise = [](std::uint64_t weak) {
    auto input = classic_input(
        {{{"A", "B"}, weak}, {{"A", "C"}, 100}, {{"D", "B"}, 100}});
    input.activities = std::vector<std::string>{"A", "B", "C", "D"};
    input.activity_counts = std::map<std::string, std::uint64_t>{
        {"A", 200}, {"B", 200}, {"C", 100}, {"D", 100}};
    input.window_two_dfg[{"A", "B"}] = weak;
    return input;
  };
  auto at_boundary = discover_heuristics_net(make_noise(5));
  assert(at_boundary.cleaned_dfg.contains({"A", "B"}));
  auto below = discover_heuristics_net(make_noise(4));
  assert(!below.cleaned_dfg.contains({"A", "B"}));
  assert(below.window_two_dfg.contains({"A", "B"}));  // main DFG only
  HeuristicMinerOptions options;
  options.most_common_paths.insert({"A", "B"});
  assert(discover_heuristics_net(make_noise(4), options)
             .cleaned_dfg.contains({"A", "B"}));
  options = {};
  options.dfg_pre_cleaning_noise_threshold = 0.0;
  assert(discover_heuristics_net(make_noise(4), options)
             .cleaned_dfg.contains({"A", "B"}));

  const auto repeated =
      log({trace({event("A", ""), event("B", ""), event("A", ""),
                  event("B", ""), event("A", ""), event("B", "")})});
  options = {};
  options.keep_once_per_case = true;
  const auto deduplicated = discover_heuristics_net(repeated, options);
  assert(deduplicated.raw_dfg.at({"A", "B"}) == 1);
  assert(deduplicated.raw_dfg.at({"B", "A"}) == 1);
  assert(deduplicated.frequency_triples.at({"A", "B", "A"}) == 2);
  assert(deduplicated.frequency_triples.at({"B", "A", "B"}) == 2);
}

void classic_and_maximal_cliques_at_equality() {
  auto input = classic_input(
      {{{"A", "B"}, 1}, {{"A", "C"}, 1}, {{"A", "D"}, 1},
       {{"B", "C"}, 1}, {{"C", "D"}, 1}});
  input.activity_counts = std::map<std::string, std::uint64_t>{
      {"A", 3}, {"B", 2}, {"C", 3}, {"D", 2}};
  HeuristicMinerOptions options;
  options.and_threshold = 1.0 / 3.0;
  options.dfg_pre_cleaning_noise_threshold = 0.0;
  const auto net = discover_heuristics_net(input, options);
  const auto a = activity(net, "A");
  const auto b = activity(net, "B");
  const auto c = activity(net, "C");
  const auto d = activity(net, "D");
  assert(net.output_bundles[a].size() == 2);
  assert((net.output_bundles[a][0] == HeuristicBundle{b, c}));
  assert((net.output_bundles[a][1] == HeuristicBundle{c, d}));
}

void classic_length_two_rescue_and_reference_quirk() {
  auto loop = classic_input(
      {{{"A", "B"}, 2}, {{"A", "C"}, 10}});
  loop.activities = std::vector<std::string>{"A", "B", "C"};
  loop.activity_counts = std::map<std::string, std::uint64_t>{
      {"A", 12}, {"B", 2}, {"C", 10}};
  loop.frequency_triples[{"A", "B", "A"}] = 1;  // exactly l2 == 0.5
  HeuristicMinerOptions options;
  options.dependency_threshold = 0.9;
  options.dfg_pre_cleaning_noise_threshold = 0.0;
  auto net = discover_heuristics_net(loop, options);
  assert(edge(net, "A", "C") != nullptr);  // creates A before loop pass
  assert(edge(net, "A", "B") != nullptr);
  assert(edge(net, "B", "A") != nullptr);
  assert(edge(net, "A", "B")->length_two_loop);
  assert(edge(net, "A", "B")->frequency == 2);
  assert(edge(net, "B", "A")->frequency == 0);
  assert(edge(net, "B", "A")->dependency == 0.0);
  assert(loop_length_two(net, "A", "B") != nullptr);
  assert(loop_length_two(net, "A", "B")->frequency == 2);
  assert(loop_length_two(net, "B", "A") == nullptr);

  // AND scores are captured before length-two rescue.  Even a permissive
  // threshold must not cause the newly rescued B edge to be folded into a
  // second pass around A.
  auto no_recompute_options = options;
  no_recompute_options.and_threshold = -0.1;
  const auto no_recompute =
      discover_heuristics_net(loop, no_recompute_options);
  assert(no_recompute.and_measures_in.empty());
  assert(no_recompute.and_measures_out.empty());

  auto performance_loop = loop;
  performance_loop.performance_dfg.emplace();
  (*performance_loop.performance_dfg)[{"A", "B"}] = {2.0};
  (*performance_loop.performance_dfg)[{"A", "C"}] = {10.0};
  (*performance_loop.performance_dfg)[{"B", "A"}] = {4.0};
  auto performance_options = options;
  performance_options.decoration = HeuristicNetDecoration::kPerformance;
  performance_options.performance_aggregation =
      HeuristicPerformanceAggregation::kRawValues;
  const auto performance_net =
      discover_heuristics_net(performance_loop, performance_options);
  assert((std::get<std::vector<double>>(
              *edge(performance_net, "A", "B")->representation_value) ==
          std::vector<double>{2.0}));
  assert((std::get<std::vector<double>>(
              *edge(performance_net, "B", "A")->representation_value) ==
          std::vector<double>{4.0}));

  options.loop_length_two_threshold = 0.500001;
  assert(edge(discover_heuristics_net(loop, options), "A", "B") == nullptr);

  auto only_loop = classic_input(
      {{{"A", "B"}, 1}, {{"B", "A"}, 1}});
  only_loop.frequency_triples[{"A", "B", "A"}] = 2;
  only_loop.activity_counts =
      std::map<std::string, std::uint64_t>{{"A", 2}, {"B", 2}};
  options.loop_length_two_threshold = 0.5;
  const auto fallback = discover_heuristics_net(only_loop, options);
  assert(fallback.edges.empty());  // no pre-existing node => no rescue
  assert(fallback.active_activities.size() == 2);
}

void classic_length_two_rescue_requires_occurrence_membership() {
  const auto check_case = [](bool has_b_count, std::uint64_t b_count,
                             std::uint64_t min_activity_count, bool rescued) {
    auto input = classic_input(
        {{{"A", "B"}, 1}, {{"A", "C"}, 10}});
    input.window_two_dfg[{"A", "A"}] = 2;
    input.frequency_triples[{"A", "B", "A"}] = 2;
    input.activity_counts =
        std::map<std::string, std::uint64_t>{{"A", 12}, {"C", 10}};
    if (has_b_count) input.activity_counts->emplace("B", b_count);

    HeuristicMinerOptions options;
    options.dependency_threshold = 0.8;
    options.dfg_pre_cleaning_noise_threshold = 0.0;
    options.min_activity_count = min_activity_count;
    const auto net = discover_heuristics_net(input, options);

    // Loop metadata is emitted before occurrence membership can reject
    // rescue, so it remains visible in every case.
    const auto* metadata = loop_length_two(net, "A", "B");
    assert(metadata != nullptr);
    assert(metadata->frequency == 1);
    assert(edge(net, "A", "C") != nullptr);

    const auto b_id = activity(net, "B");
    const bool b_active =
        std::find(net.active_activities.begin(), net.active_activities.end(),
                  b_id) != net.active_activities.end();
    assert(b_active == rescued);
    if (rescued) {
      assert(edge(net, "A", "B") != nullptr);
      assert(edge(net, "B", "A") != nullptr);
      assert(edge(net, "A", "B")->frequency == 1);
      assert(edge(net, "B", "A")->frequency == 0);
    } else {
      assert(edge(net, "A", "B") == nullptr);
      assert(edge(net, "B", "A") == nullptr);
    }
  };

  // A missing B count skips rescue for every threshold, even at zero.
  check_case(false, 0, 0, false);
  check_case(false, 0, 1, false);
  check_case(false, 0, 2, false);

  // Explicit zero is a real count: it rescues at threshold zero only.
  check_case(true, 0, 0, true);
  check_case(true, 0, 1, false);
  check_case(true, 0, 2, false);

  // A count of one rescues at thresholds zero and one, but not two.
  check_case(true, 1, 0, true);
  check_case(true, 1, 1, true);
  check_case(true, 1, 2, false);
}

void classic_large_count_ratios_and_representations() {
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  using namespace minera::detail;
  assert(exact_ratio(add(maximum, maximum), add_one(add(0, 0))) ==
         std::ldexp(1.0, 65));
  assert(exact_ratio(add(maximum, maximum), add_one(add(maximum, maximum))) == 1.0);
  assert(exact_ratio(add((std::uint64_t{1} << 53) + 1, 0),
                     add_one(add((std::uint64_t{1} << 53) - 1, 0))) == 1.0);
  auto input = classic_input({{{"A", "B"}, (std::uint64_t{1} << 53) + 1},
                              {{"B", "A"}, std::uint64_t{1} << 53}});
  input.activity_counts = std::map<std::string, std::uint64_t>{{"A", 100}, {"B", 100}};
  HeuristicMinerOptions options;
  options.dependency_threshold = 0.0;
  options.dfg_pre_cleaning_noise_threshold = 0.0;
  const auto net = discover_heuristics_net(input, options);
  const auto* forward = edge(net, "A", "B");
  assert(forward && forward->dependency == 5.551115123125782e-17);
  assert(edge(net, "B", "A") == nullptr);
  assert(std::get<std::uint64_t>(std::get<HeuristicStructuredValue>(
      *forward->representation_value).value) == (std::uint64_t{1} << 53) + 1);

  auto loop = classic_input({{{"A", "B"}, 1}, {{"A", "C"}, 10}});
  loop.activity_counts = std::map<std::string, std::uint64_t>{{"A", 100}, {"B", 100}, {"C", 100}};
  loop.frequency_triples[{"A", "B", "A"}] = maximum;
  loop.frequency_triples[{"B", "A", "B"}] = 1;
  options.dependency_threshold = 0.8;
  options.loop_length_two_threshold = 1.0;
  assert(edge(discover_heuristics_net(loop, options), "A", "B") != nullptr);

  auto split = classic_input({{{"A", "B"}, maximum}, {{"A", "C"}, 0},
                              {{"B", "C"}, maximum}, {{"C", "B"}, 0}});
  split.activity_counts = loop.activity_counts;
  options.dependency_threshold = -1.0;
  options.min_frequency = 0;
  const auto split_net = discover_heuristics_net(split, options);
  assert(!split_net.and_measures_out.empty());
  for (const auto& measure : split_net.and_measures_out)
    assert(std::isfinite(measure.value));

  // Inferred reciprocal maxima use the public binary64 average: (2 * M) / 2
  // rounds to 2^64, so the metadata and activity statistics need a wider
  // occurrence representation than the uint64 input edge counts.
  auto inferred_options = options;
  inferred_options.dependency_threshold = 0.0;
  inferred_options.min_frequency = 1;
  inferred_options.min_activity_count = 0;
  const auto reciprocal = discover_heuristics_net(
      classic_input({{{"A", "B"}, maximum},
                     {{"B", "A"}, maximum}}),
      inferred_options);
  assert(reciprocal.metadata->activities_occurrences.at("A").to_string() ==
         "18446744073709551616");
  assert(reciprocal.metadata->activities_occurrences.at("B").to_string() ==
         "18446744073709551616");
  assert(activity_stat(reciprocal, "A").occurrences.to_string() ==
         "18446744073709551616");
  assert(activity_stat(reciprocal, "B").occurrences.to_string() ==
         "18446744073709551616");

  // One-sided fanout sums both maximum uint64 edges exactly.
  const auto fanout = discover_heuristics_net(
      classic_input({{{"A", "B"}, maximum},
                     {{"A", "C"}, maximum}}),
      inferred_options);
  assert(fanout.metadata->activities_occurrences.at("A").to_string() ==
         "36893488147419103230");
  assert(edge(fanout, "A", "B") != nullptr);
  assert(edge(fanout, "A", "C") != nullptr);

  // The average around 2^53 follows binary64 ties-to-even rounding.
  const auto high_average = discover_heuristics_net(
      classic_input({{{"A", "B"}, (std::uint64_t{1} << 53) + 1},
                     {{"B", "A"}, std::uint64_t{1} << 53}}),
      inferred_options);
  assert(high_average.metadata->activities_occurrences.at("A").to_string() ==
         "9007199254740992");
  assert(high_average.metadata->activities_occurrences.at("B").to_string() ==
         "9007199254740992");

  // A widened inferred count still participates in the uint64 threshold.
  auto maximum_threshold = inferred_options;
  maximum_threshold.min_activity_count = maximum;
  const auto threshold_net = discover_heuristics_net(
      classic_input({{{"A", "B"}, maximum}}), maximum_threshold);
  assert(edge(threshold_net, "A", "B") != nullptr);
  assert(threshold_net.active_activities.size() == 2);

  // Explicit occurrence maps remain authoritative and exact.
  auto explicit_input = classic_input(
      {{{"A", "B"}, maximum}, {{"B", "A"}, maximum}});
  explicit_input.activity_counts =
      std::map<std::string, std::uint64_t>{{"A", maximum}, {"B", 0}};
  const auto explicit_net =
      discover_heuristics_net(explicit_input, inferred_options);
  assert(explicit_net.metadata->activities_occurrences.at("A").to_string() ==
         "18446744073709551615");
  assert(explicit_net.metadata->activities_occurrences.at("B").to_string() ==
         "0");
}

void classic_active_nodes_conversion_and_dfg_inference() {
  auto input = classic_input({{{"B", "C"}, 2}, {{"A", "B"}, 4}});
  auto inferred = discover_heuristics_net(input);
  assert((inferred.activities == std::vector<std::string>{"A", "B", "C"}));
  assert(activity_stat(inferred, "A").occurrences == 4);
  assert(activity_stat(inferred, "B").occurrences == 3);
  assert(activity_stat(inferred, "C").occurrences == 2);
  assert(inferred.start_activities ==
         std::vector<std::size_t>{activity(inferred, "A")});
  assert(inferred.end_activities ==
         std::vector<std::size_t>{activity(inferred, "C")});

  input.activities = std::vector<std::string>{"A", "B", "C", "D"};
  input.activity_counts = std::map<std::string, std::uint64_t>{
      {"A", 4}, {"B", 3}, {"C", 2}, {"D", 100}};
  const auto partial = discover_heuristics_net(input);
  assert(partial.active_activities.size() == 3);
  const auto petri = to_petri_net(partial);
  bool has_d = false;
  for (const auto& transition : petri.transitions())
    has_d |= !transition.silent() &&
             petri.activities()[*transition.activity] == "D";
  assert(!has_d);

  HeuristicMinerOptions plus = plus_options();
  bool rejected = false;
  try {
    (void)discover_heuristics_net(input, plus);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void classic_performance_aggregations_and_validation() {
  auto a1 = event("A", "2026-03-01T00:00:01Z");
  auto b1 = event("B", "2026-03-01T00:00:06Z");
  attr(b1, "start_timestamp", "2026-03-01T00:00:06Z");
  auto a2 = event("A", "2026-03-02T00:00:01Z");
  auto b2 = event("B", "2026-03-02T00:00:16Z");
  attr(b2, "start_timestamp", "2026-03-02T00:00:16Z");
  HeuristicMinerOptions options;
  options.decoration = HeuristicNetDecoration::kPerformance;
  options.performance_aggregation = HeuristicPerformanceAggregation::kAll;
  const auto net =
      discover_heuristics_net(log({trace({a1, b1}), trace({a2, b2})}),
                              options);
  const auto& stat = relation(net, "A", "B");
  assert((stat.wait_samples == std::vector<double>{5.0, 15.0}));
  assert(stat.mean_wait_seconds == 10.0);
  assert(stat.median_wait_seconds == 10.0);
  assert(stat.min_wait_seconds == 5.0);
  assert(stat.max_wait_seconds == 15.0);
  assert(stat.sum_wait_seconds == 20.0);
  assert(std::abs(stat.stdev_wait_seconds - std::sqrt(50.0)) < 1e-9);
  assert(edge(net, "A", "B")->representation == 10.0);
  const auto* all_edge = edge(net, "A", "B");
  assert(all_edge->representation_value.has_value());
  const auto& all_edge_summary =
      std::get<HeuristicPerformanceSummary>(*all_edge->representation_value);
  assert(all_edge_summary.mean == 10.0);
  assert(all_edge_summary.median == 10.0);
  assert(all_edge_summary.minimum == 5.0);
  assert(all_edge_summary.maximum == 15.0);
  assert(std::abs(all_edge_summary.stdev - std::sqrt(50.0)) < 1e-9);
  assert(all_edge_summary.sum == 20.0);
  assert(stat.representation_value.has_value());
  assert(std::get<HeuristicPerformanceSummary>(*stat.representation_value).sum ==
         20.0);

  options.performance_aggregation = HeuristicPerformanceAggregation::kRawValues;
  const auto raw = discover_heuristics_net(
      log({trace({a1, b1}), trace({a2, b2})}), options);
  const auto* raw_edge = edge(raw, "A", "B");
  assert(raw_edge->representation == 10.0);  // scalar compatibility value
  assert(raw_edge->representation_value.has_value());
  assert((std::get<std::vector<double>>(*raw_edge->representation_value) ==
          std::vector<double>{5.0, 15.0}));
  assert((std::get<std::vector<double>>(
              *relation(raw, "A", "B").representation_value) ==
          std::vector<double>{5.0, 15.0}));

  // PLUSPLUS uses the same public raw/all value shapes while retaining its
  // established scalar compatibility projection.
  auto plus = plus_options();
  plus.decoration = HeuristicNetDecoration::kPerformance;
  plus.performance_aggregation = HeuristicPerformanceAggregation::kRawValues;
  const auto plus_raw = discover_heuristics_net(
      log({trace({a1, b1}), trace({a2, b2})}), plus);
  const auto* plus_raw_edge = edge(plus_raw, "A", "B");
  assert(plus_raw_edge->representation == 10.0);
  assert((std::get<std::vector<double>>(
              *plus_raw_edge->representation_value) ==
          std::vector<double>{5.0, 15.0}));
  plus.performance_aggregation = HeuristicPerformanceAggregation::kAll;
  const auto plus_all = discover_heuristics_net(
      log({trace({a1, b1}), trace({a2, b2})}), plus);
  const auto& plus_all_summary = std::get<HeuristicPerformanceSummary>(
      *edge(plus_all, "A", "B")->representation_value);
  assert(plus_all_summary.mean == 10.0);
  assert(plus_all_summary.sum == 20.0);

  auto input = classic_input({{{"A", "B"}, 1}});
  input.performance_dfg.emplace();  // explicitly supplied but incomplete
  bool incomplete = false;
  try {
    (void)discover_heuristics_net(input, options);
  } catch (const std::invalid_argument&) {
    incomplete = true;
  }
  assert(incomplete);

  auto invalid_a = event("A", "invalid");
  auto valid_b = event("B", "2026-03-01T00:00:01Z");
  bool invalid = false;
  try {
    (void)discover_heuristics_net(log({trace({invalid_a, valid_b})}),
                                  options);
  } catch (const std::invalid_argument&) {
    invalid = true;
  }
  assert(invalid);
}

void classic_decorated_dfg_values_preserve_typed_payloads() {
  auto input = classic_input({{{"A", "B"}, 2}, {{"A", "C"}, 10}});
  input.activities = std::vector<std::string>{"A", "B", "C"};
  input.activity_counts =
      std::map<std::string, std::uint64_t>{{"A", 12}, {"B", 2},
                                           {"C", 10}};
  input.frequency_triples[{"A", "B", "A"}] = 1;

  HeuristicStructuredValue object_value;
  object_value.value = HeuristicStructuredValue::Object{
      {"kind", HeuristicStructuredValue{std::string("already-aggregated")}}};
  input.performance_values.emplace();
  input.performance_dfg.emplace();  // The new map overrides incomplete samples.
  input.performance_values->emplace(
      std::make_pair(std::string("A"), std::string("B")),
      HeuristicPerformanceValue{object_value});
  input.performance_values->emplace(
      std::make_pair(std::string("A"), std::string("C")),
      HeuristicPerformanceValue{HeuristicStructuredValue{7.25}});
  // Extra values are retained and interned, but do not become fallback nodes.
  input.performance_values->emplace(
      std::make_pair(std::string("extra-source"), std::string("extra-target")),
      HeuristicPerformanceValue{object_value});
  input.performance_values->emplace(
      std::make_pair(std::string("B"), std::string("A")),
      HeuristicPerformanceValue{3.5});

  HeuristicMinerOptions options;
  options.dependency_threshold = 0.9;
  options.dfg_pre_cleaning_noise_threshold = 0.0;
  // The new map selects performance mode even when the option says frequency.
  options.decoration = HeuristicNetDecoration::kFrequency;
  const auto net = discover_heuristics_net(input, options);
  assert(net.decoration == HeuristicNetDecoration::kPerformance);
  assert(net.performance_values.has_value());
  assert(net.performance_values->size() == 4);
  const auto* forward = edge(net, "A", "B");
  assert(forward != nullptr);
  assert(forward->representation == 0.0);
  assert(forward->representation_value.has_value());
  const auto& forward_value = std::get<HeuristicStructuredValue>(
      *forward->representation_value);
  const auto& forward_object =
      std::get<HeuristicStructuredValue::Object>(forward_value.value);
  assert(std::get<std::string>(forward_object.at("kind").value) ==
         "already-aggregated");
  assert(edge(net, "A", "C")->representation == 7.25);
  assert(net.activities[activity(net, "extra-source")] == "extra-source");
  assert(std::find(net.active_activities.begin(), net.active_activities.end(),
                   activity(net, "extra-source")) ==
         net.active_activities.end());

  // A rescued reverse direction has no cleaned DFG value, so an extra reverse
  // entry cannot leak into its scalar or structured representation.
  const auto* rescued_reverse = edge(net, "B", "A");
  assert(rescued_reverse != nullptr && rescued_reverse->length_two_loop);
  assert(rescued_reverse->representation == 0.0);
  assert(!rescued_reverse->representation_value.has_value());
  const auto& reverse_relation = relation(net, "B", "A");
  assert(reverse_relation.representation == 0.0);
  assert(!reverse_relation.representation_value.has_value());

  (*input.performance_values)[{"A", "C"}] = HeuristicStructuredValue{nullptr};
  const auto null_net = discover_heuristics_net(input, options);
  assert(edge(null_net, "A", "C")->representation == 10.0);
  const auto& null_map_value = std::get<HeuristicStructuredValue>(
      null_net.performance_values->at({"A", "C"}));
  assert(std::holds_alternative<std::nullptr_t>(null_map_value.value));

  // With neither performance map, direct DFG discovery keeps its public
  // frequency decoration even if the caller requests performance.
  auto no_values = input;
  no_values.performance_values.reset();
  no_values.performance_dfg.reset();
  options.decoration = HeuristicNetDecoration::kPerformance;
  const auto frequency_net = discover_heuristics_net(no_values, options);
  assert(frequency_net.decoration == HeuristicNetDecoration::kFrequency);
  assert(edge(frequency_net, "A", "C")->representation == 10.0);
}

void start_selector_defaults_and_validation() {
  auto first = interval("A", "2026-07-01T00:00:00Z",
                        "2026-07-01T00:00:00Z");
  auto second = interval("B", "2026-07-01T00:00:03Z",
                         "2026-07-01T00:00:19Z");
  attr(second, "custom_start", "2026-07-01T00:00:04Z");
  const auto source = log({trace({first, second})});

  HeuristicMinerOptions classic;
  classic.decoration = HeuristicNetDecoration::kPerformance;
  const auto completion = discover_heuristics_net(source, classic);
  assert(relation(completion, "A", "B").mean_wait_seconds == 19.0);

  classic.start_timestamp_attribute = "start_timestamp";
  const auto conventional = discover_heuristics_net(source, classic);
  assert(relation(conventional, "A", "B").mean_wait_seconds == 3.0);

  classic.start_timestamp_attribute = "custom_start";
  const auto custom = discover_heuristics_net(source, classic);
  assert(relation(custom, "A", "B").mean_wait_seconds == 4.0);

  auto sparse_first = event("A", "");
  attr(sparse_first, "finished", "2026-07-01T00:00:00Z");
  auto sparse_second = event("B", "");
  attr(sparse_second, "started", "2026-07-01T00:00:03Z");
  classic.timestamp_attribute = "finished";
  classic.start_timestamp_attribute = "started";
  const auto sparse = discover_heuristics_net(
      log({trace({sparse_first, sparse_second})}), classic);
  assert(relation(sparse, "A", "B").mean_wait_seconds == 3.0);

  classic.start_timestamp_attribute = "time:timestamp";
  classic.timestamp_attribute = "time:timestamp";
  const auto explicit_completion = discover_heuristics_net(source, classic);
  assert(relation(explicit_completion, "A", "B").mean_wait_seconds == 19.0);

  classic.start_timestamp_attribute = "missing_start";
  bool missing_threw = false;
  try {
    (void)discover_heuristics_net(source, classic);
  } catch (const std::invalid_argument&) {
    missing_threw = true;
  }
  assert(missing_threw);

  auto negative = event("B", "2026-07-01T00:00:20Z");
  attr(negative, "start_timestamp", "2026-06-30T23:59:58Z");
  classic.start_timestamp_attribute = "start_timestamp";
  const auto clamped = discover_heuristics_net(
      log({trace({interval("A", "2026-07-01T00:00:00Z",
                           "2026-07-01T00:00:10Z"),
                  negative})}),
      classic);
  assert(relation(clamped, "A", "B").mean_wait_seconds == 0.0);

  auto plus = plus_options();
  plus.decoration = HeuristicNetDecoration::kPerformance;
  const auto plus_default = discover_heuristics_net(source, plus);
  assert(relation(plus_default, "A", "B").mean_wait_seconds == 3.0);

  plus.start_timestamp_attribute = "missing_start";
  const auto plus_lifecycle = discover_heuristics_net(source, plus);
  assert(relation(plus_lifecycle, "A", "B").mean_wait_seconds == 19.0);
}

void business_hours_cover_heuristic_waits_and_sojourns() {
  const auto friday_to_monday = log({trace({
      event("A", "2024-01-05T16:00:00Z"),
      event("B", "2024-01-08T10:00:00Z"),
  })});

  HeuristicMinerOptions classic;
  classic.decoration = HeuristicNetDecoration::kPerformance;
  classic.use_business_hours = true;
  const auto default_schedule =
      discover_heuristics_net(friday_to_monday, classic);
  assert(std::abs(relation(default_schedule, "A", "B").mean_wait_seconds -
                  14400.0) < 1e-9);

  // A custom all-week slot exposes the raw 66-hour span, while an explicitly
  // empty schedule measures no elapsed business time.
  classic.business_hour_slots = {{0.0, 604800.0}};
  classic.business_hour_slots_explicit = true;
  const auto all_day = discover_heuristics_net(friday_to_monday, classic);
  assert(std::abs(relation(all_day, "A", "B").mean_wait_seconds -
                  237600.0) < 1e-9);

  classic.business_hour_slots.clear();
  const auto no_business_time =
      discover_heuristics_net(friday_to_monday, classic);
  assert(relation(no_business_time, "A", "B").mean_wait_seconds == 0.0);

  // Business time follows local wall-clock values after parsing UTC offsets;
  // this pair is positive in UTC but reversed in local wall time.
  classic.business_hour_slots_explicit = false;
  const auto mixed_offsets = log({trace({
      event("A", "2024-01-08T12:00:00+05:30"),
      event("B", "2024-01-08T10:00:00+00:00"),
  })});
  const auto mixed_net = discover_heuristics_net(mixed_offsets, classic);
  assert(relation(mixed_net, "A", "B").mean_wait_seconds == 0.0);

  auto plus = plus_options();
  plus.decoration = HeuristicNetDecoration::kFrequency;
  plus.use_business_hours = true;
  const auto intervals = log({trace({
      interval("A", "2024-01-05T15:00:00+05:30",
               "2024-01-05T16:00:00+05:30"),
      interval("B", "2024-01-08T09:00:00+00:00",
               "2024-01-08T10:00:00+00:00"),
  })});
  const auto plus_net = discover_heuristics_net(intervals, plus);
  assert(std::abs(activity_stat(plus_net, "A").mean_sojourn_seconds -
                  3600.0) < 1e-9);
  assert(std::abs(activity_stat(plus_net, "B").mean_sojourn_seconds -
                  3600.0) < 1e-9);
  assert(std::abs(relation(plus_net, "A", "B").mean_wait_seconds -
                  10800.0) < 1e-9);

  const auto negative_interval = log({trace({
      interval("N", "2024-01-08T10:00:00+00:00",
               "2024-01-08T09:00:00+00:00"),
  })});
  const auto negative_net = discover_heuristics_net(negative_interval, plus);
  assert(activity_stat(negative_net, "N").mean_sojourn_seconds == 0.0);

  auto a_start = event("A", "2024-01-05T15:00:00+05:30", "start");
  auto a_complete = event("A", "2024-01-05T16:00:00+05:30", "complete");
  auto b_start = event("B", "2024-01-08T09:00:00+00:00", "start");
  auto b_complete = event("B", "2024-01-08T10:00:00+00:00", "complete");
  for (auto* current : {&a_start, &a_complete})
    attr(*current, "concept:instance", "a1");
  for (auto* current : {&b_start, &b_complete})
    attr(*current, "concept:instance", "b1");
  const auto lifecycle_net = discover_heuristics_net(
      log({trace({a_start, b_start, a_complete, b_complete})}), plus);
  assert(std::abs(activity_stat(lifecycle_net, "A").mean_sojourn_seconds -
                  3600.0) < 1e-9);
  assert(std::abs(activity_stat(lifecycle_net, "B").mean_sojourn_seconds -
                  3600.0) < 1e-9);
  assert(std::abs(relation(lifecycle_net, "A", "B").mean_wait_seconds -
                  10800.0) < 1e-9);
}

void subsecond_timestamps_preserve_exact_order_and_business_prefix() {
  const auto source = log({trace({
      event("A", "2026-01-02T16:59:59.999998Z"),
      event("B", "2026-01-05T07:00:00.000003Z"),
  })});
  auto classic = HeuristicMinerOptions{};
  classic.decoration = HeuristicNetDecoration::kPerformance;
  classic.start_timestamp_attribute = "start_timestamp";
  auto b = source.traces.front().events.back();
  attr(b, "start_timestamp", "2026-01-05T07:00:00.000001Z");
  const auto exact = discover_heuristics_net(
      log({trace({source.traces.front().events.front(), b})}), classic);
  assert(relation(exact, "A", "B").mean_wait_seconds == 223200.000003);

  classic.use_business_hours = true;
  const auto business = discover_heuristics_net(
      log({trace({source.traces.front().events.front(), b})}), classic);
  assert(std::abs(relation(business, "A", "B").mean_wait_seconds -
                  3.00002284348011e-6) < 1e-18);

  // At the upper calendar boundary, binary64 epoch values cannot distinguish
  // adjacent microseconds. Exact interval keys must still retain the EFG edge
  // and reject the false overlap.
  auto plus = plus_options();
  plus.decoration = HeuristicNetDecoration::kPerformance;
  const auto large_year = log({trace({
      interval("A", "9999-01-01T00:00:00.000000Z",
               "9999-01-01T00:00:00.000001Z"),
      interval("B", "9999-01-01T00:00:00.000002Z",
               "9999-01-01T00:00:00.000003Z"),
  })});
  const auto net = discover_heuristics_net(large_year, plus);
  assert(relation(net, "A", "B").eventually_follows_frequency == 1);
  assert(relation(net, "A", "B").mean_wait_seconds == 1e-6);
  assert(net.concurrency_statistics.empty());

  // Sojourns retain the endpoint-float subtraction used by datetime.timestamp
  // even though their ordering and waits use exact microseconds.
  const auto pre_epoch = log({trace({
      interval("A", "1900-01-01T00:00:00.000001Z",
               "1900-01-01T00:00:00.000003Z"),
      interval("B", "1900-01-01T00:00:00.000004Z",
               "1900-01-01T00:00:00.000007Z"),
  })});
  const auto old_net = discover_heuristics_net(pre_epoch, plus);
  assert(activity_stat(old_net, "A").mean_sojourn_seconds ==
         1.9073486328125e-6);

  const auto epoch_boundary = discover_heuristics_net(log({trace({
      interval("A", "1969-12-31T23:59:59.999999Z",
               "1970-01-01T00:00:00.000000Z"),
      interval("B", "1970-01-01T00:00:00.000001Z",
               "1970-01-01T00:00:00.000002Z"),
  })}), plus);
  assert(activity_stat(epoch_boundary, "A").mean_sojourn_seconds == 1e-6);
  assert(relation(epoch_boundary, "A", "B").mean_wait_seconds == 1e-6);

  const auto long_interval = log({trace({
      interval("A", "0001-01-01T00:00:00.000000Z",
               "0001-01-01T00:00:00.000000Z"),
      interval("B", "9999-01-01T00:00:00.000017Z",
               "9999-01-01T00:00:00.000017Z"),
  })});
  // Public timedelta.total_seconds(): round the integer duration only once.
  for (auto options : {classic, plus}) {
    options.use_business_hours = false;
    const auto long_net = discover_heuristics_net(long_interval, options);
    assert(relation(long_net, "A", "B").mean_wait_seconds ==
           0x1.25d6941400000p+38);
  }
}

void configurable_activity_and_timestamp_fields() {
  auto a = event("canonical-a", "not-a-canonical-time");
  auto b = event("canonical-b", "also-not-a-canonical-time");
  attr(a, "phase", "A");
  attr(b, "phase", "B");
  attr(a, "finished_at", "2026-06-01T00:00:01Z");
  attr(b, "finished_at", "2026-06-01T00:05:01Z");

  HeuristicMinerOptions classic;
  classic.activity_attribute = "phase";
  classic.timestamp_attribute = "finished_at";
  classic.decoration = HeuristicNetDecoration::kPerformance;
  const auto net = discover_heuristics_net(log({trace({a, b})}), classic);
  assert((net.activities == std::vector<std::string>{"A", "B"}));
  assert(net.activity_attribute == "phase");
  assert(net.timestamp_attribute == "finished_at");
  assert(net.activity_ids_are_local);
  assert(edge(net, "A", "B") != nullptr);
  assert(std::abs(edge(net, "A", "B")->representation - 300.0) < 1e-9);

  auto interval_a = event("canonical-a", "invalid-completion");
  auto interval_b = event("canonical-b", "invalid-completion");
  attr(interval_a, "phase", "A");
  attr(interval_b, "phase", "B");
  attr(interval_a, "finished_at", "2026-06-02T00:00:05Z");
  attr(interval_b, "finished_at", "2026-06-02T00:00:09Z");
  attr(interval_a, "started_at", "2026-06-02T00:00:00Z");
  attr(interval_b, "started_at", "2026-06-02T00:00:06Z");
  auto plus = plus_options();
  plus.activity_attribute = "phase";
  plus.timestamp_attribute = "finished_at";
  plus.start_timestamp_attribute = "started_at";
  const auto plus_net =
      discover_heuristics_net(log({trace({interval_a, interval_b})}), plus);
  assert((plus_net.activities == std::vector<std::string>{"A", "B"}));
  assert(plus_net.activity_ids_are_local);
  assert(std::abs(activity_stat(plus_net, "A").mean_sojourn_seconds - 5.0) <
         1e-9);
  assert(std::abs(activity_stat(plus_net, "B").mean_sojourn_seconds - 3.0) <
         1e-9);
  const auto decorated = decorate_model(
      log({trace({interval_a, interval_b})}), to_petri_net(plus_net), "phase",
      "finished_at");
  assert(decorated.trace_count == 1);
  assert(decorated.decorated_traces == 1);

  auto missing = event("canonical", "2026-06-02T00:00:10Z");
  bool threw = false;
  try {
    (void)discover_heuristics_net(log({trace({missing, b})}), classic);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

struct ClassicTimestampFixture {
  std::string name;
  EventLog source;
  HeuristicMinerOptions options;
  std::vector<std::string> custom_case_ids;
  std::vector<ClassicFlatRow> row_order;
  std::vector<std::int64_t> index_labels;
  HeuristicFrequencyDfg event_log_dfg;
  HeuristicFrequencyDfg flat_dfg;
  std::vector<double> event_log_waits;
  std::vector<double> flat_waits;
  std::optional<std::string> event_log_error;
  std::optional<std::string> flat_error;
};

ClassicTimestampFixture classic_fixture(
    std::string name, EventLog source, HeuristicMinerOptions options,
    HeuristicFrequencyDfg event_log_dfg,
    HeuristicFrequencyDfg flat_dfg,
    std::vector<double> event_log_waits,
    std::vector<double> flat_waits,
    std::optional<std::string> event_log_error = std::nullopt,
    std::optional<std::string> flat_error = std::nullopt,
    std::vector<std::string> custom_case_ids = {},
    std::vector<ClassicFlatRow> row_order = {},
    std::vector<std::int64_t> index_labels = {}) {
  ClassicTimestampFixture result;
  result.name = std::move(name);
  result.source = std::move(source);
  result.options = std::move(options);
  result.custom_case_ids = std::move(custom_case_ids);
  result.row_order = std::move(row_order);
  result.index_labels = std::move(index_labels);
  result.event_log_dfg = std::move(event_log_dfg);
  result.flat_dfg = std::move(flat_dfg);
  result.event_log_waits = std::move(event_log_waits);
  result.flat_waits = std::move(flat_waits);
  result.event_log_error = std::move(event_log_error);
  result.flat_error = std::move(flat_error);
  if (!result.custom_case_ids.empty()) {
    assert(result.custom_case_ids.size() == result.row_order.size());
    for (std::size_t row = 0; row < result.row_order.size(); ++row) {
      const auto [trace_index, event_index] = result.row_order[row];
      result.source.traces[trace_index].events[event_index].attributes.push_back(
          EventAttribute{"case:custom:id", result.custom_case_ids[row],
                         AttributeKind::kString});
    }
  }
  return result;
}

HeuristicMinerOptions classic_performance_options() {
  HeuristicMinerOptions options;
  options.variant = HeuristicMinerVariant::kClassic;
  options.decoration = HeuristicNetDecoration::kPerformance;
  return options;
}

HeuristicFrequencyDfg classic_ab_frequency(std::uint64_t count = 2) {
  return {{{"A", "B"}, count}};
}

HeuristicFrequencyDfg classic_custom_case_frequency() {
  return {{{"A", "A"}, 1}, {{"B", "B"}, 1}};
}

std::string json_string(std::string_view value) {
  constexpr char hex[] = "0123456789abcdef";
  std::string result = "\"";
  for (const unsigned char byte : value) {
    switch (byte) {
      case '\\': result += "\\\\"; break;
      case '"': result += "\\\""; break;
      case '\b': result += "\\b"; break;
      case '\f': result += "\\f"; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      default:
        if (byte < 0x20) {
          result += "\\u00";
          result += hex[byte >> 4];
          result += hex[byte & 0x0f];
        } else {
          result += static_cast<char>(byte);
        }
    }
  }
  result += '"';
  return result;
}

std::string classic_options_json(const HeuristicMinerOptions& options) {
  std::string result = "{\"variant\":\"CLASSIC\",\"decoration\":\"";
  result += options.decoration == HeuristicNetDecoration::kPerformance
                ? "PERFORMANCE"
                : "FREQUENCY";
  result += "\",\"activity_attribute\":" +
            json_string(options.activity_attribute) +
            ",\"timestamp_attribute\":" +
            json_string(options.timestamp_attribute) +
            ",\"start_timestamp_attribute\":";
  result += options.start_timestamp_attribute
                ? json_string(*options.start_timestamp_attribute)
                : "null";
  result += ",\"case_id_attribute\":" +
            json_string(options.case_id_attribute) +
            ",\"use_business_hours\":" +
            (options.use_business_hours ? "true" : "false") + "}";
  return result;
}

std::string activity_mapping_json(const DirectlyFollowsGraph& graph) {
  std::string result = "{";
  for (std::size_t id = 0; id < graph.activities().size(); ++id) {
    if (id != 0) result += ',';
    result += json_string(std::to_string(id)) + ":" +
              json_string(graph.activities()[id]);
  }
  return result + "}";
}

void assert_classic_performance_result(
    const HeuristicsNet& net, const HeuristicFrequencyDfg& expected_dfg,
    const std::vector<double>& expected_waits) {
  assert(net.variant == HeuristicMinerVariant::kClassic);
  assert(net.decoration == HeuristicNetDecoration::kPerformance);
  assert(net.performance_aggregation == HeuristicPerformanceAggregation::kMean);
  assert(net.activity_attribute == "concept:name");
  assert(net.timestamp_attribute == "time:timestamp");
  assert(!net.activity_ids_are_local);
  assert(net.raw_dfg == expected_dfg);
  assert(net.cleaned_dfg == expected_dfg);
  assert(net.window_two_dfg.empty());
  assert(net.frequency_triples.empty());
  assert(net.edges.size() == expected_dfg.size());
  assert(net.relation_statistics.size() == expected_dfg.size());
  for (const auto& [pair, frequency] : expected_dfg) {
    const auto& stats = relation(net, pair.first, pair.second);
    assert(stats.direct_adjacency_frequency == frequency);
    assert(stats.raw_frequency == frequency);
    assert(stats.cleaned_frequency == frequency);
    assert(stats.wait_samples == expected_waits);
    const double expected_mean = expected_waits.empty()
                                     ? 0.0
                                     : std::accumulate(expected_waits.begin(),
                                                       expected_waits.end(), 0.0) /
                                           expected_waits.size();
    assert(std::abs(stats.mean_wait_seconds - expected_mean) < 1e-9);
    assert(std::abs(stats.representation - expected_mean) < 1e-9);
    const auto* result_edge = edge(net, pair.first, pair.second);
    assert(result_edge != nullptr);
    assert(result_edge->frequency == frequency);
    assert(std::abs(result_edge->representation - expected_mean) < 1e-9);
  }
  assert(net.metadata.has_value());
  assert(net.metadata->activities == net.activities);
}

void classic_flat_timestamp_acceptance(
    const std::optional<std::string>& evidence_path = std::nullopt) {
  const auto normal = classic_timestamp_log({
      {classic_timestamp_event("A", "2026-01-01T00:00:01+00:00",
                               "2026-01-01T00:00:00+00:00",
                               "2026-01-01T00:00:00+00:00"),
       classic_timestamp_event("B", "2026-01-01T00:00:20+00:00",
                               "2026-01-01T00:00:03+00:00",
                               "2026-01-01T00:00:04+00:00")},
      {classic_timestamp_event("A", "2026-01-01T00:00:01+00:00",
                               "2026-01-01T00:00:00+00:00",
                               "2026-01-01T00:00:00+00:00"),
       classic_timestamp_event("B", "2026-01-01T00:00:20+00:00",
                               "2026-01-01T00:00:05+00:00",
                               "2026-01-01T00:00:06+00:00")},
  });
  auto conventional = classic_performance_options();
  conventional.start_timestamp_attribute = "start_timestamp";
  auto custom = classic_performance_options();
  custom.start_timestamp_attribute = "custom:start";
  auto explicit_completion = classic_performance_options();
  explicit_completion.start_timestamp_attribute = "time:timestamp";

  const auto negative = classic_timestamp_log({
      {classic_timestamp_event("A", "2026-01-01T00:00:01+00:00",
                               "2026-01-01T00:00:00+00:00",
                               "2026-01-01T00:00:00+00:00"),
       classic_timestamp_event("B", "2026-01-01T00:00:20+00:00",
                               "2025-12-31T23:59:58+00:00",
                               "2026-01-01T00:00:04+00:00")},
      {classic_timestamp_event("A", "2026-01-01T00:00:01+00:00",
                               "2026-01-01T00:00:00+00:00",
                               "2026-01-01T00:00:00+00:00"),
       classic_timestamp_event("B", "2026-01-01T00:00:20+00:00",
                               "2026-01-01T00:00:05+00:00",
                               "2026-01-01T00:00:06+00:00")},
  });
  auto sparse_destination = classic_timestamp_log({
      {classic_timestamp_event("A", "2026-01-01T00:00:01+00:00",
                               "2026-01-01T00:00:00+00:00",
                               "2026-01-01T00:00:00+00:00"),
       classic_timestamp_event("B", "2026-01-01T00:00:20+00:00",
                               "2026-01-01T00:00:03+00:00", std::nullopt)},
      {classic_timestamp_event("A", "2026-01-01T00:00:01+00:00",
                               "2026-01-01T00:00:00+00:00",
                               "2026-01-01T00:00:00+00:00"),
       classic_timestamp_event("B", "2026-01-01T00:00:20+00:00",
                               "2026-01-01T00:00:05+00:00",
                               "2026-01-01T00:00:06+00:00")},
  });
  auto sparse_source = classic_timestamp_log({
      {classic_timestamp_event("A", "2026-01-01T00:00:01+00:00",
                               "2026-01-01T00:00:00+00:00", std::nullopt),
       classic_timestamp_event("B", "2026-01-01T00:00:20+00:00",
                               "2026-01-01T00:00:03+00:00",
                               "2026-01-01T00:00:04+00:00")},
      {classic_timestamp_event("A", "2026-01-01T00:00:01+00:00",
                               "2026-01-01T00:00:00+00:00", std::nullopt),
       classic_timestamp_event("B", "2026-01-01T00:00:20+00:00",
                               "2026-01-01T00:00:05+00:00",
                               "2026-01-01T00:00:06+00:00")},
  });
  auto typed_nat = classic_timestamp_log({
      {classic_timestamp_event("A", "2026-01-01T00:00:01+00:00",
                               "2026-01-01T00:00:00+00:00",
                               "2026-01-01T00:00:00+00:00"),
       classic_timestamp_event("B", "2026-01-01T00:00:20+00:00",
                               "2026-01-01T00:00:03+00:00", "NaT")},
      {classic_timestamp_event("A", "2026-01-01T00:00:01+00:00",
                               "2026-01-01T00:00:00+00:00",
                               "2026-01-01T00:00:00+00:00"),
       classic_timestamp_event("B", "2026-01-01T00:00:20+00:00",
                               "2026-01-01T00:00:05+00:00",
                               "2026-01-01T00:00:06+00:00")},
  });
  auto reverse_completion = classic_timestamp_log({
      {classic_timestamp_event("A", "2026-01-01T00:00:20+00:00",
                               "2026-01-01T00:00:10+00:00",
                               "2026-01-01T00:00:10+00:00"),
       classic_timestamp_event("B", "2026-01-01T00:00:01+00:00",
                               "2026-01-01T00:00:00+00:00",
                               "2026-01-01T00:00:00+00:00")},
      {classic_timestamp_event("A", "2026-01-01T00:00:30+00:00",
                               "2026-01-01T00:00:15+00:00",
                               "2026-01-01T00:00:15+00:00"),
       classic_timestamp_event("B", "2026-01-01T00:00:02+00:00",
                               "2026-01-01T00:00:01+00:00",
                               "2026-01-01T00:00:01+00:00")},
  });

  const std::vector<ClassicFlatRow> interleaved{{0, 0}, {1, 0}, {0, 1}, {1, 1}};
  auto timezone_interleaved = classic_timestamp_log({
      {classic_timestamp_event("A", "2024-02-01T00:00:01+01:00",
                               "2024-02-01T00:00:00+01:00",
                               "2024-02-01T00:00:00+01:00"),
       classic_timestamp_event("B", "2024-02-01T00:00:20+01:00",
                               "2024-02-01T00:00:03+01:00",
                               "2024-02-01T00:00:04+01:00")},
      {classic_timestamp_event("A", "2024-02-01T00:00:01+01:00",
                               "2024-02-01T00:00:00+01:00",
                               "2024-02-01T00:00:00+01:00"),
       classic_timestamp_event("B", "2024-02-01T00:00:20+01:00",
                               "2024-02-01T00:00:05+01:00",
                               "2024-02-01T00:00:06+01:00")},
  });
  timezone_interleaved.traces[0].case_id = "z-case";
  timezone_interleaved.traces[1].case_id = "a-case";
  for (auto& current_trace : timezone_interleaved.traces)
    for (auto& current_event : current_trace.events)
      current_event.case_id = current_trace.case_id;
  set_classic_timezone(timezone_interleaved, "Europe/Berlin");

  auto custom_case_options = custom;
  custom_case_options.case_id_attribute = "case:custom:id";

  auto absent = classic_performance_options();
  absent.start_timestamp_attribute = "not:present";
  const std::string configured_missing =
      "Heuristics Miner CLASSIC is missing the configured start timestamp attribute";

  const std::vector<ClassicTimestampFixture> fixtures{
      classic_fixture("omitted-start-selector-present", normal,
                      classic_performance_options(), classic_ab_frequency(),
                      classic_ab_frequency(), {19.0, 19.0}, {19.0, 19.0}),
      classic_fixture("explicit-conventional-start", normal, conventional,
                      classic_ab_frequency(), classic_ab_frequency(),
                      {2.0, 4.0}, {2.0, 4.0}),
      classic_fixture("explicit-custom-start", normal, custom,
                      classic_ab_frequency(), classic_ab_frequency(),
                      {3.0, 5.0}, {3.0, 5.0}),
      classic_fixture("explicit-completion-as-start", normal,
                      explicit_completion, classic_ab_frequency(),
                      classic_ab_frequency(), {19.0, 19.0}, {19.0, 19.0}),
      classic_fixture("negative-conventional-start-wait", negative,
                      conventional, classic_ab_frequency(),
                      classic_ab_frequency(), {0.0, 4.0}, {0.0, 4.0}),
      classic_fixture("sparse-custom-start-destination", sparse_destination,
                      custom, {}, classic_ab_frequency(), {}, {0.0, 5.0},
                      configured_missing),
      classic_fixture("sparse-custom-start-source", sparse_source, custom,
                      classic_ab_frequency(), classic_ab_frequency(),
                      {3.0, 5.0}, {3.0, 5.0}),
      classic_fixture("selected-start-column-absent", normal, absent, {}, {},
                      {}, {}, configured_missing, configured_missing),
      classic_fixture("typed-europe-berlin-case-order", timezone_interleaved,
                      custom, classic_ab_frequency(), classic_ab_frequency(),
                      {3.0, 5.0}, {3.0, 5.0}, std::nullopt, std::nullopt, {},
                      interleaved, {9, 3, 7, 1}),
      classic_fixture("explicit-custom-case-selector-interleaved", normal,
                      custom_case_options, classic_ab_frequency(),
                      classic_custom_case_frequency(), {3.0, 5.0}, {0.0},
                      std::nullopt, std::nullopt,
                      {"activity-A", "activity-A", "activity-B", "activity-B"},
                      interleaved, {8, 2, 6, 4}),
      classic_fixture("explicit-selected-start-pandas-nat", typed_nat, custom,
                      classic_ab_frequency(), classic_ab_frequency(),
                      {0.0, 5.0}, {0.0, 5.0}),
      classic_fixture("interleaved-reverse-completion-order",
                      reverse_completion, custom, classic_ab_frequency(),
                      classic_ab_frequency(), {0.0, 0.0}, {0.0, 0.0},
                      std::nullopt, std::nullopt, {}, interleaved,
                      {10, 8, 6, 4}),
  };

  std::vector<std::string> evidence_records;
  const auto append_evidence = [&](const ClassicTimestampFixture& fixture,
                                  std::string_view route,
                                  const HeuristicMinerOptions& options,
                                  const EventLog& graph_log,
                                  std::string_view input_snapshot_before,
                                  std::string_view input_snapshot_after,
                                  const HeuristicsNet* result_net,
                                  std::string_view error) {
    const auto graph = discover_directly_follows_graph(graph_log);
    std::string record = "{\"fixture\":" + json_string(fixture.name) +
                         ",\"route\":" + json_string(route) +
                         ",\"outcome\":" +
                         json_string(result_net ? "result" : "error") +
                         ",\"options\":" + classic_options_json(options) +
                         ",\"input_snapshot_before\":" +
                         std::string(input_snapshot_before) +
                         ",\"input_snapshot_after\":" +
                         std::string(input_snapshot_after) +
                         ",\"activity_id_to_name\":" +
                         activity_mapping_json(graph) + ",\"analysis\":";
    if (result_net == nullptr) {
      record += "null,\"error\":" + json_string(error) + "}";
    } else {
      record += analysis_json(fixture.name + "-" + std::string(route),
                              graph_log, graph, nullptr, nullptr, result_net) +
                ",\"error\":null}";
    }
    evidence_records.push_back(std::move(record));
  };

  const auto exercise = [&](const ClassicTimestampFixture& fixture,
                            bool flat_route) {
    const auto table = classic_timestamp_table(
        fixture.source, fixture.custom_case_ids, fixture.row_order,
        fixture.index_labels);
    const auto snapshot = flat_route
                              ? serialize_event_table_json(table)
                              : serialize_event_table_json(
                                    event_log_to_table(fixture.source));
    EventLog graph_log = fixture.source;
    if (flat_route) {
      LogConversionOptions conversion;
      conversion.case_id_key = fixture.options.case_id_attribute;
      graph_log = event_table_to_event_log(table, conversion);
    }
    const auto& expected_dfg =
        flat_route ? fixture.flat_dfg : fixture.event_log_dfg;
    const auto& expected_waits =
        flat_route ? fixture.flat_waits : fixture.event_log_waits;
    const auto& expected_error =
        flat_route ? fixture.flat_error : fixture.event_log_error;
    const std::string route = flat_route ? "event_table" : "event_log";
    try {
      const auto result_net = flat_route
                                  ? discover_heuristics_net(table,
                                                            fixture.options)
                                  : discover_heuristics_net(fixture.source,
                                                            fixture.options);
      assert(!expected_error.has_value());
      assert_classic_performance_result(result_net, expected_dfg,
                                        expected_waits);
      const auto snapshot_after =
          flat_route ? serialize_event_table_json(table)
                     : serialize_event_table_json(
                           event_log_to_table(fixture.source));
      assert(snapshot == snapshot_after);
      append_evidence(fixture, route, fixture.options, graph_log, snapshot,
                      snapshot_after, &result_net, {});
    } catch (const std::invalid_argument& error) {
      assert(expected_error.has_value());
      assert(error.what() == *expected_error);
      const auto snapshot_after =
          flat_route ? serialize_event_table_json(table)
                     : serialize_event_table_json(
                           event_log_to_table(fixture.source));
      assert(snapshot == snapshot_after);
      append_evidence(fixture, route, fixture.options, graph_log, snapshot,
                      snapshot_after, nullptr, error.what());
    }
  };

  for (const auto& fixture : fixtures) {
    exercise(fixture, false);
    exercise(fixture, true);
  }

  // EventDataView accepts an EventLog while keeping its existing trace
  // boundaries authoritative even when its flat case selector names no field.
  auto grouped_view_options = custom;
  grouped_view_options.case_id_attribute = "case:does:not:exist";
  const auto grouped_view_net = discover_heuristics_net(
      EventDataView(normal), grouped_view_options);
  assert_classic_performance_result(grouped_view_net, classic_ab_frequency(),
                                    {3.0, 5.0});

  // A converted table/stream tags the canonical case cell with a first-class
  // role. The selected flat case column must override that carrier metadata.
  auto role_marked_table = event_log_to_table(normal);
  const auto activity_column = std::find(role_marked_table.columns.begin(),
                                         role_marked_table.columns.end(),
                                         "concept:name");
  assert(activity_column != role_marked_table.columns.end());
  const auto activity_index = static_cast<std::size_t>(
      activity_column - role_marked_table.columns.begin());
  role_marked_table.columns.insert(role_marked_table.columns.begin() + 2,
                                   "case:custom:id");
  if (!role_marked_table.dtypes.empty())
    role_marked_table.dtypes.insert(role_marked_table.dtypes.begin() + 2,
                                    "str");
  for (auto& row : role_marked_table.rows) {
    assert(activity_index < row.size() && row[activity_index]);
    const auto& activity_value = *row[activity_index];
    row.insert(row.begin() + 2,
               EventAttribute{"case:custom:id",
                              activity_value.value == "A" ? "activity-A"
                                                           : "activity-B",
                              AttributeKind::kString});
  }
  role_marked_table.validate();
  const auto role_marked_table_net = discover_heuristics_net(
      role_marked_table, custom_case_options);
  assert_classic_performance_result(role_marked_table_net,
                                    classic_custom_case_frequency(), {0.0});
  const auto role_marked_stream =
      event_table_to_event_stream(role_marked_table);
  const auto stream_net = discover_heuristics_net(
      role_marked_stream, custom_case_options);
  assert_classic_performance_result(stream_net,
                                    classic_custom_case_frequency(), {0.0});
  const auto lazy_table = LazyEventTable::from_table(role_marked_table);
  const auto lazy_net =
      discover_heuristics_net(lazy_table, custom_case_options);
  assert_classic_performance_result(lazy_net,
                                    classic_custom_case_frequency(), {0.0});

  // PLUSPLUS uses the same configured case key for flat carriers while the
  // EventLog overload above remains governed by its trace boundaries.
  auto plus_custom_case_options = plus_options();
  plus_custom_case_options.case_id_attribute = "case:custom:id";
  const auto plus_custom_case_net =
      discover_heuristics_net(role_marked_table, plus_custom_case_options);
  assert(plus_custom_case_net.variant == HeuristicMinerVariant::kPlusPlus);
  assert(activity_stat(plus_custom_case_net, "A").start_occurrences == 1);
  assert(activity_stat(plus_custom_case_net, "A").end_occurrences == 1);
  assert(activity_stat(plus_custom_case_net, "B").start_occurrences == 1);
  assert(activity_stat(plus_custom_case_net, "B").end_occurrences == 1);
  const auto plus_event_log_net =
      discover_heuristics_net(normal, plus_custom_case_options);
  assert(activity_stat(plus_event_log_net, "A").start_occurrences == 2);
  assert(activity_stat(plus_event_log_net, "A").end_occurrences == 0);
  assert(activity_stat(plus_event_log_net, "B").start_occurrences == 0);
  assert(activity_stat(plus_event_log_net, "B").end_occurrences == 2);

  // The value domains stay separate: a present null date is a missing
  // selected value, while an ordinary string "NaT" proceeds to date parsing.
  auto null_date = event("B", "2026-01-01T00:00:20Z");
  EventAttribute null_start;
  null_start.key = "custom:start";
  null_start.kind = AttributeKind::kDate;
  null_start.is_null = true;
  null_date.attributes.push_back(null_start);
  auto null_options = custom;
  bool null_rejected_as_missing = false;
  try {
    (void)discover_heuristics_net(
        classic_timestamp_log({
            {classic_timestamp_event("A", "2026-01-01T00:00:01Z",
                                     std::nullopt,
                                     "2026-01-01T00:00:00Z"),
             null_date}}),
        null_options);
  } catch (const std::invalid_argument& error) {
    null_rejected_as_missing = error.what() ==
                               std::string("Heuristics Miner CLASSIC is missing the configured start timestamp attribute");
  }
  assert(null_rejected_as_missing);

  auto string_nat = event("B", "2026-01-01T00:00:20Z");
  attr(string_nat, "custom:start", "NaT");
  bool string_nat_rejected_by_parser = false;
  try {
    (void)discover_heuristics_net(
        classic_timestamp_log({
            {classic_timestamp_event("A", "2026-01-01T00:00:01Z",
                                     std::nullopt,
                                     "2026-01-01T00:00:00Z"),
             string_nat}}),
        custom);
  } catch (const std::invalid_argument& error) {
    string_nat_rejected_by_parser =
        error.what() != configured_missing;
  }
  assert(string_nat_rejected_by_parser);

  // Public explicit-NaT evidence: elapsed performance contributes a zero
  // sample, while business-hours conversion rejects NaN on both routes.
  auto business_nat = custom;
  business_nat.use_business_hours = true;
  for (const bool flat_route : {false, true}) {
    const auto table = classic_timestamp_table(typed_nat);
    const auto snapshot = flat_route ? serialize_event_table_json(table)
                                     : serialize_event_table_json(
                                           event_log_to_table(typed_nat));
    EventLog graph_log = typed_nat;
    if (flat_route) {
      LogConversionOptions conversion;
      conversion.case_id_key = business_nat.case_id_attribute;
      graph_log = event_table_to_event_log(table, conversion);
    }
    const auto snapshot_before = snapshot;
    bool rejected_nan = false;
    std::string message;
    try {
      if (flat_route)
        (void)discover_heuristics_net(table, business_nat);
      else
        (void)discover_heuristics_net(typed_nat, business_nat);
    } catch (const std::invalid_argument& error) {
      message = error.what();
      rejected_nan = message == "cannot convert float NaN to integer";
    }
    assert(rejected_nan);
    const auto snapshot_after =
        flat_route ? serialize_event_table_json(table)
                   : serialize_event_table_json(event_log_to_table(typed_nat));
    assert(snapshot_before == snapshot_after);
    ClassicTimestampFixture business_fixture{
        "explicit_typed_nat_business_hours", typed_nat, business_nat};
    const auto graph = discover_directly_follows_graph(graph_log);
    std::string record = "{\"fixture\":" +
                         json_string(business_fixture.name) +
                         ",\"route\":" +
                         json_string(flat_route ? "event_table" : "event_log") +
                         ",\"outcome\":\"error\",\"options\":" +
                         classic_options_json(business_nat) +
                         ",\"input_snapshot_before\":" + snapshot_before +
                         ",\"input_snapshot_after\":" + snapshot_after +
                         ",\"activity_id_to_name\":" +
                         activity_mapping_json(graph) +
                         ",\"analysis\":null,\"error\":" +
                         json_string(message) + "}";
    evidence_records.push_back(std::move(record));
  }

  if (evidence_path) {
    std::ofstream output(*evidence_path, std::ios::binary);
    if (!output)
      throw std::runtime_error("unable to open CLASSIC evidence output");
    output << '[';
    for (std::size_t index = 0; index < evidence_records.size(); ++index) {
      if (index != 0) output << ',';
      output << evidence_records[index];
    }
    output << "]\n";
    if (!output)
      throw std::runtime_error("unable to write CLASSIC evidence output");
  }
}

std::string classic_frequency_dfg_json(const HeuristicFrequencyDfg& dfg) {
  std::string result = "[";
  std::size_t index = 0;
  for (const auto& [pair, count] : dfg) {
    if (index++ != 0) result += ',';
    result += "{\"source\":" + json_string(pair.first) +
              ",\"target\":" + json_string(pair.second) +
              ",\"count\":" + std::to_string(count) + "}";
  }
  return result + ']';
}

std::string classic_triples_json(const HeuristicTripleFrequencies& triples) {
  std::string result = "[";
  std::size_t index = 0;
  for (const auto& [triple, count] : triples) {
    const auto& [first, middle, last] = triple;
    if (index++ != 0) result += ',';
    result += "{\"first\":" + json_string(first) +
              ",\"middle\":" + json_string(middle) +
              ",\"last\":" + json_string(last) +
              ",\"count\":" + std::to_string(count) + "}";
  }
  return result + ']';
}

std::string classic_counts_json(
    const std::map<std::string, std::uint64_t>& counts) {
  std::string result = "{";
  std::size_t index = 0;
  for (const auto& [name, count] : counts) {
    if (index++ != 0) result += ',';
    result += json_string(name) + ':' + std::to_string(count);
  }
  return result + '}';
}

std::string classic_optional_counts_json(
    const std::optional<std::map<std::string, std::uint64_t>>& counts) {
  return counts ? classic_counts_json(*counts) : "null";
}

std::string classic_optional_activities_json(
    const std::optional<std::vector<std::string>>& activities) {
  if (!activities) return "null";
  std::string result = "[";
  for (std::size_t index = 0; index < activities->size(); ++index) {
    if (index != 0) result += ',';
    result += json_string((*activities)[index]);
  }
  return result + ']';
}

std::string classic_optional_boundaries_json(
    const std::optional<HeuristicBoundaryActivities>& boundaries) {
  if (!boundaries) return "null";
  if (const auto* activities =
          std::get_if<std::vector<std::string>>(&*boundaries)) {
    std::string result = "[";
    for (std::size_t index = 0; index < activities->size(); ++index) {
      if (index != 0) result += ',';
      result += json_string((*activities)[index]);
    }
    return result + ']';
  }
  return classic_counts_json(
      std::get<std::map<std::string, std::uint64_t>>(*boundaries));
}

std::string classic_optional_performance_json(
    const std::optional<HeuristicDecoratedDfg>& values) {
  if (!values) return "null";
  std::string result = "[";
  std::size_t index = 0;
  for (const auto& [pair, value] : *values) {
    const auto* scalar = std::get_if<double>(&value);
    assert(scalar != nullptr);
    if (index++ != 0) result += ',';
    result += "{\"source\":" + json_string(pair.first) +
              ",\"target\":" + json_string(pair.second) +
              ",\"value\":" + std::to_string(*scalar) + "}";
  }
  return result + ']';
}

struct ClassicObjectEvidenceFixture {
  std::string name;
  HeuristicClassicDfgInput input;
  HeuristicMinerOptions options;
  EventLog graph_log;
  std::map<std::string, std::pair<std::uint64_t, std::uint64_t>>
      expected_boundary_counts;
};

std::string classic_object_input_json(
    const ClassicObjectEvidenceFixture& fixture) {
  const auto& input = fixture.input;
  std::string result = "{\"dfg\":" + classic_frequency_dfg_json(input.dfg) +
                       ",\"activities\":" +
                       classic_optional_activities_json(input.activities) +
                       ",\"activity_counts\":" +
                       classic_optional_counts_json(input.activity_counts) +
                       ",\"start_counts\":" +
                       classic_optional_counts_json(input.start_counts) +
                       ",\"end_counts\":" +
                       classic_optional_counts_json(input.end_counts) +
                       ",\"window_two_dfg\":" +
                       classic_frequency_dfg_json(input.window_two_dfg) +
                       ",\"frequency_triples\":" +
                       classic_triples_json(input.frequency_triples) +
                       ",\"dfg_window_2_supplied\":" +
                       (input.dfg_window_2_supplied ? "true" : "false") +
                       ",\"freq_triples_supplied\":" +
                       (input.freq_triples_supplied ? "true" : "false") +
                       ",\"effective_dfg_window_2_present\":" +
                       ((input.dfg_window_2_supplied ||
                         !input.window_two_dfg.empty())
                            ? "true"
                            : "false") +
                       ",\"effective_freq_triples_present\":" +
                       ((input.freq_triples_supplied ||
                         !input.frequency_triples.empty())
                            ? "true"
                            : "false") +
                       ",\"performance_dfg\":" +
                       classic_optional_performance_json(
                           input.performance_values) +
                       ",\"start_activities\":" +
                       classic_optional_boundaries_json(
                           input.start_activities) +
                       ",\"end_activities\":" +
                       classic_optional_boundaries_json(input.end_activities);
  // The public DFG API has optional window-two and triple arguments, while
  // native input stores concrete maps. Keep caller-supplied flags and
  // effective presence together so omitted and explicit-empty stay distinct.
  result += ",\"public_optional_input_presence\":{\"activities\":" +
            std::string(input.activities ? "true" : "false") +
            ",\"activities_occurrences\":" +
            (input.activity_counts ? "true" : "false") +
            ",\"start_activities\":" +
            (input.start_activities ? "true" : "false") +
            ",\"end_activities\":" +
            (input.end_activities ? "true" : "false") +
            ",\"dfg_window_2\":" +
            ((input.dfg_window_2_supplied || !input.window_two_dfg.empty())
                 ? "true"
                 : "false") +
            ",\"freq_triples\":" +
            ((input.freq_triples_supplied || !input.frequency_triples.empty())
                 ? "true"
                 : "false") +
            ",\"performance_dfg\":" +
            (input.performance_values ? "true" : "false") + "}}";
  return result;
}

std::string classic_object_options_json(
    const HeuristicMinerOptions& options) {
  auto result = classic_options_json(options);
  assert(!result.empty() && result.back() == '}');
  result.pop_back();
  result += ",\"dfg_pre_cleaning_noise_threshold\":" +
            std::to_string(options.dfg_pre_cleaning_noise_threshold) + "}";
  return result;
}

std::string classic_nonfinite_typed_double_json(double value) {
  if (std::isnan(value))
    return "{\"type\":\"builtins.float\",\"nonfinite\":\"NaN\"}";
  if (std::isinf(value))
    return std::signbit(value)
               ? "{\"type\":\"builtins.float\",\"nonfinite\":\"-INF\"}"
               : "{\"type\":\"builtins.float\",\"nonfinite\":\"INF\"}";
  return "{\"type\":\"builtins.float\",\"value\":" +
         std::to_string(value) + "}";
}

std::string classic_nonfinite_structured_json(
    const HeuristicStructuredValue& value) {
  return std::visit(
      [](const auto& item) -> std::string {
        using Item = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<Item, std::nullptr_t>) {
          return "{\"type\":\"builtins.NoneType\",\"value\":null}";
        } else if constexpr (std::is_same_v<Item, bool>) {
          return std::string("{\"type\":\"builtins.bool\",\"value\":") +
                 (item ? "true}" : "false}");
        } else if constexpr (std::is_same_v<Item, std::int64_t> ||
                             std::is_same_v<Item, std::uint64_t>) {
          return "{\"type\":\"builtins.int\",\"value\":" +
                 std::to_string(item) + "}";
        } else if constexpr (std::is_same_v<Item, double>) {
          return classic_nonfinite_typed_double_json(item);
        } else if constexpr (std::is_same_v<Item, std::string>) {
          return "{\"type\":\"builtins.str\",\"value\":" +
                 json_string(item) + "}";
        } else if constexpr (std::is_same_v<
                                 Item, HeuristicStructuredValue::Array>) {
          std::string result = "{\"type\":\"builtins.list\",\"items\":[";
          for (std::size_t index = 0; index < item.size(); ++index) {
            if (index != 0) result += ',';
            result += classic_nonfinite_structured_json(item[index]);
          }
          return result + "]}";
        } else {
          std::string result = "{\"type\":\"builtins.dict\",\"entries\":[";
          std::size_t index = 0;
          for (const auto& [key, mapped] : item) {
            if (index++ != 0) result += ',';
            result += "{\"key\":{\"type\":\"builtins.str\",\"value\":" +
                      json_string(key) + "},\"value\":" +
                      classic_nonfinite_structured_json(mapped) + "}";
          }
          return result + "]}";
        }
      },
      value.value);
}

std::string classic_nonfinite_performance_value_json(
    const HeuristicPerformanceValue& value) {
  return std::visit(
      [](const auto& item) -> std::string {
        using Item = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<Item, double>) {
          return classic_nonfinite_typed_double_json(item);
        } else if constexpr (std::is_same_v<Item, std::vector<double>>) {
          std::string result = "{\"type\":\"builtins.list\",\"items\":[";
          for (std::size_t index = 0; index < item.size(); ++index) {
            if (index != 0) result += ',';
            result += classic_nonfinite_typed_double_json(item[index]);
          }
          return result + "]}";
        } else if constexpr (
            std::is_same_v<Item, HeuristicPerformanceSummary>) {
          return "{\"type\":\"unsupported-performance-summary\"}";
        } else {
          return classic_nonfinite_structured_json(item);
        }
      },
      value);
}

std::string classic_nonfinite_performance_map_json(
    const HeuristicDecoratedDfg& values) {
  std::string result = "{\"type\":\"builtins.dict\",\"entries\":[";
  std::size_t index = 0;
  for (const auto& [pair, value] : values) {
    if (index++ != 0) result += ',';
    result += "{\"key\":{\"type\":\"builtins.tuple\",\"items\":["
              "{\"type\":\"builtins.str\",\"value\":" +
              json_string(pair.first) +
              "},{\"type\":\"builtins.str\",\"value\":" +
              json_string(pair.second) + "}]},\"value\":" +
              classic_nonfinite_performance_value_json(value) + "}";
  }
  return result + "]}";
}

std::string classic_dfg_order_json(
    const std::optional<std::vector<std::pair<std::string, std::string>>>& order) {
  if (!order) return "null";
  std::string result = "[";
  for (std::size_t index = 0; index < order->size(); ++index) {
    if (index != 0) result += ',';
    const auto& [source, target] = (*order)[index];
    result += '[' + json_string(source) + ',' + json_string(target) + ']';
  }
  return result + ']';
}

std::string classic_nonfinite_input_snapshot(
    const HeuristicClassicDfgInput& input) {
  return "{\"dfg\":" + classic_frequency_dfg_json(input.dfg) +
         ",\"activities\":" +
         classic_optional_activities_json(input.activities) +
         ",\"activity_counts\":" +
         classic_optional_counts_json(input.activity_counts) +
         ",\"start_counts\":" +
         classic_optional_counts_json(input.start_counts) +
         ",\"end_counts\":" +
         classic_optional_counts_json(input.end_counts) +
         ",\"window_two_dfg\":" +
         classic_frequency_dfg_json(input.window_two_dfg) +
         ",\"frequency_triples\":" +
         classic_triples_json(input.frequency_triples) +
         ",\"performance_dfg_present\":" +
         (input.performance_dfg ? "true" : "false") +
         ",\"start_activities\":" +
         classic_optional_boundaries_json(input.start_activities) +
         ",\"end_activities\":" +
         classic_optional_boundaries_json(input.end_activities) +
         ",\"performance_values\":" +
         (input.performance_values
              ? classic_nonfinite_performance_map_json(
                    *input.performance_values)
              : "null") +
         ",\"dfg_window_2_supplied\":" +
         (input.dfg_window_2_supplied ? "true" : "false") +
         ",\"freq_triples_supplied\":" +
         (input.freq_triples_supplied ? "true" : "false") +
         ",\"dfg_order\":" + classic_dfg_order_json(input.dfg_order) + "}";
}

void assert_classic_nonfinite_default_options(
    const HeuristicMinerOptions& options) {
  assert(options.variant == HeuristicMinerVariant::kClassic);
  assert(options.activity_attribute == "concept:name");
  assert(options.timestamp_attribute == "time:timestamp");
  assert(!options.dependency_threshold.has_value());
  assert(options.min_frequency == 1);
  assert(!options.connect_all);
  assert(options.and_threshold == 0.65);
  assert(options.min_activity_count == 1);
  assert(options.decoration == HeuristicNetDecoration::kFrequency);
  assert(options.performance_aggregation ==
         HeuristicPerformanceAggregation::kMean);
  assert(options.dfg_pre_cleaning_noise_threshold == 0.05);
  assert(options.loop_length_two_threshold == 0.5);
  assert(!options.keep_once_per_case);
  assert(options.most_common_paths.empty());
  assert(!options.start_timestamp_attribute.has_value());
  assert(options.lifecycle_instance_attribute == "concept:instance");
  assert(!options.use_business_hours);
  assert(options.business_hour_slots.empty());
  assert(!options.business_hour_slots_explicit);
  assert(options.case_id_attribute == "case:concept:name");
}

std::string heuristic_options_snapshot_json(
    const HeuristicMinerOptions& options);

template <typename Relation>
std::string classic_nonfinite_relation_record_json(
    const HeuristicsNet& net, const Relation& value) {
  return "{\"source\":" + json_string(net.activities[value.source]) +
         ",\"target\":" + json_string(net.activities[value.target]) +
         ",\"representation\":" +
         classic_nonfinite_typed_double_json(value.representation) +
         ",\"representation_value\":" +
         (value.representation_value
              ? classic_nonfinite_performance_value_json(
                    *value.representation_value)
              : "null") + "}";
}

void classic_nonfinite_object_payloads_preserve_typed_values(
    const std::optional<std::string>& evidence_path = std::nullopt) {
  struct Fixture {
    std::string name;
    HeuristicPerformanceValue input_value;
    std::string expected_value_json;
  };

  HeuristicStructuredValue::Array nested_items{
      HeuristicStructuredValue{std::numeric_limits<double>::infinity()}};
  HeuristicStructuredValue nested_value{
      HeuristicStructuredValue::Object{
          {"x", HeuristicStructuredValue{std::move(nested_items)}}}};
  const std::vector<Fixture> fixtures{
      {"nan", std::numeric_limits<double>::quiet_NaN(),
       "{\"type\":\"builtins.float\",\"nonfinite\":\"NaN\"}"},
      {"positive_infinity", std::numeric_limits<double>::infinity(),
       "{\"type\":\"builtins.float\",\"nonfinite\":\"INF\"}"},
      {"negative_infinity", -std::numeric_limits<double>::infinity(),
       "{\"type\":\"builtins.float\",\"nonfinite\":\"-INF\"}"},
      {"nested", std::move(nested_value),
       "{\"type\":\"builtins.dict\",\"entries\":[{\"key\":{\"type\":\"builtins.str\",\"value\":\"x\"},\"value\":{\"type\":\"builtins.list\",\"items\":[{\"type\":\"builtins.float\",\"nonfinite\":\"INF\"}]}}]}"},
      {"literal_nan", HeuristicStructuredValue{std::string("NaN")},
       "{\"type\":\"builtins.str\",\"value\":\"NaN\"}"},
  };

  std::vector<std::string> evidence_records;
  for (const auto& fixture : fixtures) {
    auto input = classic_input({{{"A", "B"}, 1}});
    input.performance_values.emplace();
    input.performance_values->emplace(
        std::make_pair(std::string("A"), std::string("B")),
        fixture.input_value);
    HeuristicMinerOptions options;

    // The input uses the captured A>B=1 DFG, default options, omitted
    // auxiliary metadata, and only the supplied decorated map.
    assert(!input.activities.has_value());
    assert(!input.activity_counts.has_value());
    assert(!input.start_counts.has_value());
    assert(!input.end_counts.has_value());
    assert(input.window_two_dfg.empty());
    assert(input.frequency_triples.empty());
    assert(!input.performance_dfg.has_value());
    assert(!input.start_activities.has_value());
    assert(!input.end_activities.has_value());
    assert(!input.dfg_window_2_supplied);
    assert(!input.freq_triples_supplied);
    assert(!input.dfg_order.has_value());
    assert_classic_nonfinite_default_options(options);

    const auto input_before = classic_nonfinite_input_snapshot(input);
    const auto options_before = heuristic_options_snapshot_json(options);
    const auto net = discover_heuristics_net(input, options);
    const auto input_after = classic_nonfinite_input_snapshot(input);
    const auto options_after = heuristic_options_snapshot_json(options);
    assert(input_before == input_after);
    assert(!input.dfg_order.has_value());
    assert(options_before == options_after);
    assert_classic_nonfinite_default_options(options);
    assert(net.variant == HeuristicMinerVariant::kClassic);
    assert(net.decoration == HeuristicNetDecoration::kPerformance);
    assert(net.performance_values.has_value());
    assert(net.performance_values->size() == 1);
    const auto map_value = net.performance_values->find({"A", "B"});
    assert(map_value != net.performance_values->end());
    assert(classic_nonfinite_performance_value_json(map_value->second) ==
           fixture.expected_value_json);
    assert(net.edges.size() == 1);
    assert(net.relation_statistics.size() == 1);
    const auto* result_edge = edge(net, "A", "B");
    assert(result_edge != nullptr);
    const auto& result_relation = relation(net, "A", "B");
    const auto expected_scalar = std::get_if<double>(&fixture.input_value)
                                     ? fixture.expected_value_json
                                     : classic_nonfinite_typed_double_json(0.0);
    assert(classic_nonfinite_typed_double_json(result_edge->representation) ==
           expected_scalar);
    assert(classic_nonfinite_typed_double_json(result_relation.representation) ==
           expected_scalar);
    assert(result_edge->representation_value.has_value());
    assert(result_relation.representation_value.has_value());
    assert(classic_nonfinite_performance_value_json(
               *result_edge->representation_value) == fixture.expected_value_json);
    assert(classic_nonfinite_performance_value_json(
               *result_relation.representation_value) ==
           fixture.expected_value_json);

    const auto map_json =
        classic_nonfinite_performance_map_json(*net.performance_values);
    const auto expected_map_json =
        "{\"type\":\"builtins.dict\",\"entries\":[{\"key\":{\"type\":\"builtins.tuple\",\"items\":["
        "{\"type\":\"builtins.str\",\"value\":\"A\"},{\"type\":\"builtins.str\",\"value\":\"B\"}]},\"value\":" +
        fixture.expected_value_json + "}]}";
    assert(map_json == expected_map_json);

    std::string record = "{\"name\":" + json_string(fixture.name) +
                         ",\"route\":\"direct_classic_dfg\",\"input_before\":" +
                         input_before + ",\"input_after\":" + input_after +
                         ",\"options_before\":" + options_before +
                         ",\"options_after\":" + options_after +
                         ",\"performance_values\":" + map_json +
                         ",\"edges\":[" +
                         classic_nonfinite_relation_record_json(
                             net, *result_edge) +
                         "],\"relations\":[" +
                         classic_nonfinite_relation_record_json(
                             net, result_relation) +
                         "],\"inputs_unchanged\":" +
                         (input_before == input_after ? "true" : "false") +
                         ",\"options_unchanged\":" +
                         (options_before == options_after ? "true" : "false") +
                         "}";
    evidence_records.push_back(std::move(record));
  }

  if (evidence_path) {
    std::ofstream output(*evidence_path, std::ios::binary);
    if (!output)
      throw std::runtime_error(
          "unable to open CLASSIC nonfinite object evidence output");
    output << '[';
    for (std::size_t index = 0; index < evidence_records.size(); ++index) {
      if (index != 0) output << ',';
      output << evidence_records[index];
    }
    output << "]\n";
    if (!output)
      throw std::runtime_error(
          "unable to write CLASSIC nonfinite object evidence output");
  }
}

void classic_dfg_order_preserves_inferred_boundary_lists(
    const std::optional<std::string>& evidence_path = std::nullopt) {
  using Paths = std::vector<std::pair<std::string, std::string>>;
  using Names = std::vector<std::string>;
  struct Fixture {
    std::string name;
    HeuristicFrequencyDfg dfg;
    Paths order;
    Names activities;
    Names starts;
    Names ends;
  };
  // Expected lists come only from the four immutable public observations.
  const HeuristicFrequencyDfg disconnected{{{"A", "B"}, 1},
                                            {{"Z", "Y"}, 1}};
  const HeuristicFrequencyDfg forked{{{"A", "B"}, 2}, {{"A", "C"}, 2},
                                    {{"B", "D"}, 2}, {{"C", "D"}, 2}};
  const std::vector<Fixture> fixtures{
      {"disconnected_forward", disconnected, {{"A", "B"}, {"Z", "Y"}},
       {"A", "B", "Y", "Z"}, {"A", "Z"}, {"B", "Y"}},
      {"disconnected_reversed", disconnected, {{"Z", "Y"}, {"A", "B"}},
       {"A", "B", "Y", "Z"}, {"Z", "A"}, {"Y", "B"}},
      {"forked_forward", forked,
       {{"A", "B"}, {"A", "C"}, {"B", "D"}, {"C", "D"}},
       {"A", "B", "C", "D"}, {"A"}, {"D"}},
      {"forked_reversed", forked,
       {{"C", "D"}, {"B", "D"}, {"A", "C"}, {"A", "B"}},
       {"A", "B", "C", "D"}, {"A"}, {"D"}},
  };
  const auto assert_boundaries = [](const HeuristicsNet& net,
                                    const Names& starts, const Names& ends) {
    assert(net.metadata.has_value());
    assert(std::get<Names>(net.metadata->start_activities) == starts);
    assert(std::get<Names>(net.metadata->end_activities) == ends);
  };
  std::vector<std::string> records;
  for (const auto& fixture : fixtures) {
    HeuristicClassicDfgInput input;
    input.dfg = fixture.dfg;
    input.dfg_order = fixture.order;
    HeuristicMinerOptions options;
    assert_classic_nonfinite_default_options(options);
    assert(!input.activities && !input.activity_counts && !input.start_counts &&
           !input.end_counts && !input.start_activities && !input.end_activities &&
           !input.performance_dfg && !input.performance_values);
    assert(input.window_two_dfg.empty() && input.frequency_triples.empty());
    assert(!input.dfg_window_2_supplied && !input.freq_triples_supplied);
    const auto before = classic_nonfinite_input_snapshot(input);
    const auto dfg_before = classic_frequency_dfg_json(input.dfg);
    const auto order_before = classic_dfg_order_json(input.dfg_order);
    const auto options_before = heuristic_options_snapshot_json(options);
    const auto net = discover_heuristics_net(input, options);
    const auto after = classic_nonfinite_input_snapshot(input);
    const auto options_after = heuristic_options_snapshot_json(options);
    assert(before == after);
    assert(input.dfg == fixture.dfg);
    assert(input.dfg_order == fixture.order);
    assert(options_before == options_after);
    assert_classic_nonfinite_default_options(options);
    assert(net.activities == fixture.activities);
    assert(net.metadata.has_value());
    assert(net.metadata->activities == fixture.activities);
    assert_boundaries(net, fixture.starts, fixture.ends);
    records.push_back(
        "{\"name\":" + json_string(fixture.name) +
        ",\"route\":\"direct_classic_dfg\",\"input_before\":" + before +
        ",\"input_after\":" + after +
        ",\"dfg_before\":" + dfg_before +
        ",\"dfg_after\":" + classic_frequency_dfg_json(input.dfg) +
        ",\"dfg_order_before\":" + order_before +
        ",\"dfg_order_after\":" + classic_dfg_order_json(input.dfg_order) +
        ",\"options_before\":" + options_before +
        ",\"options_after\":" + options_after +
        ",\"activities\":" + classic_optional_activities_json(net.activities) +
        ",\"metadata_activities\":" +
        classic_optional_activities_json(net.metadata->activities) +
        ",\"start_activities\":" +
        classic_optional_boundaries_json(net.metadata->start_activities) +
        ",\"end_activities\":" +
        classic_optional_boundaries_json(net.metadata->end_activities) +
        ",\"inputs_unchanged\":" + (before == after ? "true" : "false") +
        ",\"options_unchanged\":" +
        (options_before == options_after ? "true" : "false") + "}");
  }

  HeuristicClassicDfgInput legacy;
  legacy.dfg = disconnected;
  assert(!legacy.dfg_order);
  assert_boundaries(discover_heuristics_net(legacy), {"A", "Z"}, {"B", "Y"});
  assert(!legacy.dfg_order);
  HeuristicClassicDfgInput empty;
  empty.dfg_order = Paths{};
  assert_boundaries(discover_heuristics_net(empty), {}, {});
  const auto reject_order = [&](const HeuristicFrequencyDfg& dfg,
                                const Paths& order) {
    HeuristicClassicDfgInput invalid;
    invalid.dfg = dfg;
    invalid.dfg_order = order;
    const auto before = classic_nonfinite_input_snapshot(invalid);
    bool rejected = false;
    try {
      (void)discover_heuristics_net(invalid);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
    assert(before == classic_nonfinite_input_snapshot(invalid));
  };
  reject_order(disconnected, {});
  reject_order(disconnected, {{"A", "B"}, {"A", "B"}});
  reject_order(disconnected, {{"A", "B"}});
  reject_order(disconnected, {{"A", "B"}, {"C", "D"}});
  reject_order({}, {{"A", "B"}});
  auto explicit_boundaries = legacy;
  explicit_boundaries.dfg_order = Paths{{"Z", "Y"}, {"A", "B"}};
  explicit_boundaries.start_activities = Names{"B", "A", "B"};
  explicit_boundaries.end_activities = Names{"Y", "B"};
  assert_boundaries(discover_heuristics_net(explicit_boundaries),
                    {"B", "A", "B"}, {"Y", "B"});
  explicit_boundaries.start_activities = Names{};
  explicit_boundaries.end_activities = Names{};
  assert_boundaries(discover_heuristics_net(explicit_boundaries), {}, {});

  if (evidence_path) {
    std::ofstream output(*evidence_path, std::ios::binary);
    if (!output)
      throw std::runtime_error("unable to open CLASSIC DFG order evidence output");
    output << '[';
    for (std::size_t index = 0; index < records.size(); ++index) {
      if (index != 0) output << ',';
      output << records[index];
    }
    output << "]\n";
    if (!output)
      throw std::runtime_error("unable to write CLASSIC DFG order evidence output");
  }
}

void assert_classic_object_fields(const HeuristicsNet& net,
                                  const DirectlyFollowsGraph& graph,
                                  std::string_view analysis) {
  assert(net.variant == HeuristicMinerVariant::kClassic);
  assert(net.sojourn_times.has_value());
  assert(net.sojourn_times->empty());
  assert(analysis.find("\"sojourn_times\": {}") != std::string_view::npos);
  const auto statistics_begin = analysis.find("\"activity_statistics\": [");
  assert(statistics_begin != std::string_view::npos);
  for (const auto activity_id : net.active_activities) {
    const auto statistic = std::find_if(
        net.activity_statistics.begin(), net.activity_statistics.end(),
        [&](const auto& candidate) { return candidate.activity == activity_id; });
    assert(statistic != net.activity_statistics.end());
    assert(statistic->is_start_activity.has_value());
    assert(statistic->is_end_activity.has_value());
    assert(!*statistic->is_start_activity);
    assert(!*statistic->is_end_activity);
    const auto graph_id = graph.activity_id(net.activities[activity_id]);
    assert(graph_id.has_value());
    const std::string row_start =
        "{\"activity\": " + std::to_string(*graph_id) + ",";
    const auto row = analysis.find(row_start, statistics_begin);
    assert(row != std::string_view::npos);
    const auto row_end = analysis.find('}', row);
    assert(row_end != std::string_view::npos);
    const auto row_json = analysis.substr(row, row_end - row);
    assert(row_json.find("\"is_start_activity\": false") !=
           std::string_view::npos);
    assert(row_json.find("\"is_end_activity\": false") !=
           std::string_view::npos);
  }
}

void classic_object_fields_acceptance(
    const std::optional<std::string>& evidence_path = std::nullopt,
    const std::optional<std::string>& auxiliary_evidence_path = std::nullopt) {
  auto edge_log = log({trace({event("A", "2026-01-01T00:00:00Z"),
                              event("B", "2026-01-01T00:00:01Z")})});
  auto repeated_edge_log = log({
      trace({event("A", "2026-01-01T00:00:00Z"),
             event("B", "2026-01-01T00:00:01Z")}),
      trace({event("A", "2026-01-02T00:00:00Z"),
             event("B", "2026-01-02T00:00:01Z")}),
  });
  auto singleton_log =
      log({trace({event("A", "2026-01-01T00:00:00Z")})});

  auto inferred = classic_input({{{"A", "B"}, 1}});
  HeuristicMinerOptions inferred_options;
  inferred_options.dfg_pre_cleaning_noise_threshold = 0.0;
  auto end_mapping = inferred;
  end_mapping.end_activities = HeuristicBoundaryActivities{
      std::map<std::string, std::uint64_t>{{"Z", 0}, {"A", 4}}};
  auto start_mapping = inferred;
  start_mapping.start_activities = HeuristicBoundaryActivities{
      std::map<std::string, std::uint64_t>{{"Z", 0}, {"A", 4}}};

  HeuristicClassicDfgInput empty_fallback;
  empty_fallback.activities = std::vector<std::string>{"A"};
  empty_fallback.activity_counts =
      std::map<std::string, std::uint64_t>{{"A", 1}};
  empty_fallback.start_activities =
      HeuristicBoundaryActivities{std::vector<std::string>{"A"}};
  empty_fallback.end_activities =
      HeuristicBoundaryActivities{std::vector<std::string>{"A"}};

  auto explicit_performance = classic_input({{{"A", "B"}, 2}});
  explicit_performance.activities = std::vector<std::string>{"A", "B"};
  explicit_performance.activity_counts =
      std::map<std::string, std::uint64_t>{{"A", 2}, {"B", 2}};
  explicit_performance.start_activities =
      HeuristicBoundaryActivities{std::vector<std::string>{"A"}};
  explicit_performance.end_activities =
      HeuristicBoundaryActivities{std::vector<std::string>{"B"}};
  explicit_performance.performance_values.emplace();
  explicit_performance.performance_values->emplace(
      std::make_pair(std::string("A"), std::string("B")),
      HeuristicPerformanceValue{1.5});

  auto explicit_empty_auxiliary = classic_input({{{"A", "B"}, 1}});
  explicit_empty_auxiliary.activities = std::vector<std::string>{"A", "B"};
  explicit_empty_auxiliary.activity_counts =
      std::map<std::string, std::uint64_t>{{"A", 1}, {"B", 1}};
  explicit_empty_auxiliary.start_activities =
      HeuristicBoundaryActivities{std::vector<std::string>{"A"}};
  explicit_empty_auxiliary.end_activities =
      HeuristicBoundaryActivities{std::vector<std::string>{"B"}};
  explicit_empty_auxiliary.dfg_window_2_supplied = true;
  explicit_empty_auxiliary.freq_triples_supplied = true;

  const std::vector<ClassicObjectEvidenceFixture> fixtures{
      {"boundary_0_0", inferred, inferred_options, edge_log,
       {{"A", {1, 0}}, {"B", {0, 1}}}},
      {"boundary_0_5", end_mapping, inferred_options, edge_log,
       {{"A", {1, 4}}, {"B", {0, 0}}}},
      {"boundary_5_0", start_mapping, inferred_options, edge_log,
       {{"A", {4, 0}}, {"B", {0, 1}}}},
      {"empty-dfg-declared-fallback-boundary", empty_fallback,
       HeuristicMinerOptions{}, singleton_log,
       {{"A", {1, 1}}}},
      {"explicit-list-boundaries-performance-dfg", explicit_performance,
       HeuristicMinerOptions{}, repeated_edge_log,
       {{"A", {1, 0}}, {"B", {0, 1}}}},
  };

  std::vector<std::string> evidence_records;
  std::map<std::string, std::string> records_by_fixture;
  const auto make_evidence_record = [&](const auto& fixture) {
    const auto input_before = classic_object_input_json(fixture);
    const auto graph = discover_directly_follows_graph(fixture.graph_log);
    const auto result_net =
        discover_heuristics_net(fixture.input, fixture.options);
    const bool window_two_present = fixture.input.dfg_window_2_supplied ||
                                    !fixture.input.window_two_dfg.empty();
    const bool triples_present = fixture.input.freq_triples_supplied ||
                                 !fixture.input.frequency_triples.empty();
    assert(result_net.classic_public_dfg_window_2_present ==
           window_two_present);
    assert(result_net.classic_public_freq_triples_present == triples_present);
    for (const auto& [name, expected] : fixture.expected_boundary_counts) {
      const auto& statistic = activity_stat(result_net, name);
      assert(statistic.start_occurrences == expected.first);
      assert(statistic.end_occurrences == expected.second);
    }
    const auto analysis = analysis_json(fixture.name, fixture.graph_log,
                                        graph, nullptr, nullptr, &result_net);
    assert_classic_object_fields(result_net, graph, analysis);
    if (fixture.name == "empty-dfg-declared-fallback-boundary") {
      assert(result_net.edges.empty());
      assert(result_net.active_activities.size() == 1);
      assert(result_net.activities[result_net.active_activities.front()] ==
             "A");
    }
    if (fixture.name == "explicit-list-boundaries-performance-dfg") {
      assert(result_net.decoration == HeuristicNetDecoration::kPerformance);
      assert(result_net.performance_values.has_value());
      assert(std::get<double>(result_net.performance_values->at({"A", "B"})) ==
             1.5);
      assert(analysis.find("\"value\": 1.5") != std::string::npos);
    }
    if (fixture.name == "explicit-empty-window-triples") {
      assert(fixture.input.dfg_window_2_supplied);
      assert(fixture.input.freq_triples_supplied);
      const auto start = analysis.find("\"classic_public_fields\": {");
      assert(start != std::string::npos);
      const auto end = analysis.find('}', start);
      assert(end != std::string::npos);
      assert(analysis.substr(start, end - start + 1) ==
             "\"classic_public_fields\": {\"dfg_window_2\": [], "
             "\"freq_triples\": []}");
    }
    if (fixture.name == "boundary_0_0" ||
        fixture.name == "boundary_0_5" ||
        fixture.name == "boundary_5_0" ||
        fixture.name == "empty-dfg-declared-fallback-boundary" ||
        fixture.name == "explicit-list-boundaries-performance-dfg") {
      const auto start = analysis.find("\"classic_public_fields\": {");
      assert(start != std::string::npos);
      const auto end = analysis.find('}', start);
      assert(end != std::string::npos);
      assert(analysis.substr(start, end - start + 1) ==
             "\"classic_public_fields\": {\"dfg_window_2\": null, "
             "\"freq_triples\": null}");
    }
    assert(classic_object_input_json(fixture) == input_before);
    std::string record = "{\"fixture\":" + json_string(fixture.name) +
                         ",\"route\":\"direct_dfg\",\"outcome\":\"result\"" +
                         ",\"options\":" +
                         classic_object_options_json(fixture.options) +
                         ",\"input_snapshot_before\":" + input_before +
                         ",\"input_snapshot_after\":" +
                         classic_object_input_json(fixture) +
                         ",\"activity_id_to_name\":" +
                         activity_mapping_json(graph) +
                         ",\"analysis\":" + analysis +
                         ",\"error\":null}";
    return record;
  };
  for (const auto& fixture : fixtures) {
    auto record = make_evidence_record(fixture);
    records_by_fixture.emplace(fixture.name, record);
    evidence_records.push_back(std::move(record));
  }
  const ClassicObjectEvidenceFixture explicit_empty_fixture{
      "explicit-empty-window-triples", explicit_empty_auxiliary,
      HeuristicMinerOptions{}, edge_log, {{"A", {1, 0}}, {"B", {0, 1}}}};
  records_by_fixture.emplace(explicit_empty_fixture.name,
                             make_evidence_record(explicit_empty_fixture));

  // EventLog discovery shares the direct-DFG implementation path and returns
  // the same fields on its own model and serialized analysis object.
  const auto classic_log_graph = discover_directly_follows_graph(edge_log);
  const auto classic_log_net = discover_heuristics_net(edge_log);
  const auto classic_log_analysis =
      analysis_json("classic-object-event-log", edge_log, classic_log_graph,
                    nullptr, nullptr, &classic_log_net);
  assert_classic_object_fields(classic_log_net, classic_log_graph,
                               classic_log_analysis);
  assert(classic_log_net.window_two_dfg.empty());
  assert(classic_log_net.frequency_triples.empty());
  assert(classic_log_net.classic_public_dfg_window_2_present);
  assert(classic_log_net.classic_public_freq_triples_present);
  assert(classic_log_analysis.find(
             "\"classic_public_fields\": {\"dfg_window_2\": [], "
             "\"freq_triples\": []}") != std::string::npos);

  // Nonempty legacy collections make their public fields present even when
  // callers do not set the newly added supplied-input flags.
  auto legacy_nonempty = classic_input({{{"A", "B"}, 1}});
  legacy_nonempty.window_two_dfg[{"A", "C"}] = 2;
  legacy_nonempty.frequency_triples[{"A", "B", "C"}] = 3;
  assert(!legacy_nonempty.dfg_window_2_supplied);
  assert(!legacy_nonempty.freq_triples_supplied);
  const auto legacy_graph_log =
      log({trace({event("A", "2026-01-03T00:00:00Z"),
                  event("B", "2026-01-03T00:00:01Z")})});
  const auto legacy_graph = discover_directly_follows_graph(legacy_graph_log);
  const auto legacy_net = discover_heuristics_net(legacy_nonempty);
  assert(legacy_net.classic_public_dfg_window_2_present);
  assert(legacy_net.classic_public_freq_triples_present);
  const auto legacy_analysis = analysis_json(
      "classic-object-legacy-nonempty-auxiliary", legacy_graph_log,
      legacy_graph, nullptr, nullptr, &legacy_net);
  assert(legacy_analysis.find(
             "\"classic_public_fields\": {\"dfg_window_2\": [{\"key\": "
             "[\"A\", \"C\"], \"value\": 2}], \"freq_triples\": "
             "[{\"key\": [\"A\", \"B\", \"C\"], \"value\": 3}]}") !=
         std::string::npos);

  // PLUSPLUS keeps actual boundary occurrence counts and exposes false public
  // node flags. The CLASSIC auxiliary return-object fields remain absent.
  const auto plus_log = log({trace({event("A", "2026-01-01T00:00:00Z"),
                                   event("B", "2026-01-01T00:00:01Z")})});
  const auto plus_graph = discover_directly_follows_graph(plus_log);
  const auto plus_net = discover_heuristics_net(plus_log, plus_options());
  assert(plus_net.sojourn_times.has_value());
  assert(((*plus_net.sojourn_times) ==
          std::map<std::string, double>{{"A", 0.0}, {"B", 0.0}}));
  assert(activity_stat(plus_net, "A").start_occurrences == 1);
  assert(activity_stat(plus_net, "B").end_occurrences == 1);
  for (const auto& statistic : plus_net.activity_statistics) {
    assert(statistic.is_start_activity.has_value());
    assert(statistic.is_end_activity.has_value());
    assert(!*statistic.is_start_activity);
    assert(!*statistic.is_end_activity);
  }
  const auto plus_analysis = analysis_json("classic-plus-object-fields",
                                           plus_log, plus_graph, nullptr,
                                           nullptr, &plus_net);
  assert(plus_analysis.find("\"sojourn_times\": {\"A\": 0, \"B\": 0}") !=
         std::string::npos);
  assert(plus_analysis.find("\"is_start_activity\": false") != std::string::npos);
  assert(plus_analysis.find("\"is_end_activity\": false") != std::string::npos);
  assert(plus_analysis.find("\"classic_public_fields\"") ==
         std::string::npos);

  if (evidence_path) {
    std::ofstream output(*evidence_path, std::ios::binary);
    if (!output)
      throw std::runtime_error("unable to open CLASSIC object evidence output");
    output << '[';
    for (std::size_t index = 0; index < evidence_records.size(); ++index) {
      if (index != 0) output << ',';
      output << evidence_records[index];
    }
    output << "]\n";
    if (!output)
      throw std::runtime_error("unable to write CLASSIC object evidence output");
  }
  if (auxiliary_evidence_path) {
    const std::vector<std::string> auxiliary_records{
        records_by_fixture.at("empty-dfg-declared-fallback-boundary"),
        records_by_fixture.at("explicit-list-boundaries-performance-dfg"),
        records_by_fixture.at("explicit-empty-window-triples")};
    std::ofstream output(*auxiliary_evidence_path, std::ios::binary);
    if (!output)
      throw std::runtime_error(
          "unable to open CLASSIC auxiliary evidence output");
    output << '[';
    for (std::size_t index = 0; index < auxiliary_records.size(); ++index) {
      if (index != 0) output << ',';
      output << auxiliary_records[index];
    }
    output << "]\n";
    if (!output)
      throw std::runtime_error(
          "unable to write CLASSIC auxiliary evidence output");
  }
}

std::string heuristic_variant_json_name(HeuristicMinerVariant variant) {
  switch (variant) {
    case HeuristicMinerVariant::kClassic: return "classic";
    case HeuristicMinerVariant::kPlusPlus: return "plusplus";
  }
  return "UNKNOWN";
}

std::string heuristic_aggregation_json_name(
    HeuristicPerformanceAggregation aggregation) {
  switch (aggregation) {
    case HeuristicPerformanceAggregation::kMean: return "mean";
    case HeuristicPerformanceAggregation::kMedian: return "median";
    case HeuristicPerformanceAggregation::kMinimum: return "minimum";
    case HeuristicPerformanceAggregation::kMaximum: return "maximum";
    case HeuristicPerformanceAggregation::kStandardDeviation:
      return "standard_deviation";
    case HeuristicPerformanceAggregation::kSum: return "sum";
    case HeuristicPerformanceAggregation::kRawValues: return "raw_values";
    case HeuristicPerformanceAggregation::kAll: return "all";
  }
  return "UNKNOWN";
}

std::string heuristic_options_snapshot_json(
    const HeuristicMinerOptions& options) {
  std::string result = "{\"variant\":" +
                      json_string(heuristic_variant_json_name(options.variant)) +
                      ",\"activity_attribute\":" +
                      json_string(options.activity_attribute) +
                      ",\"timestamp_attribute\":" +
                      json_string(options.timestamp_attribute) +
            ",\"dependency_threshold\":";
  result += options.dependency_threshold
                ? std::to_string(*options.dependency_threshold)
                : "null";
  result += ",\"min_frequency\":" +
            std::to_string(options.min_frequency) +
            ",\"connect_all\":" +
            (options.connect_all ? "true" : "false") +
            ",\"and_threshold\":" +
            std::to_string(options.and_threshold) +
            ",\"min_activity_count\":" +
            std::to_string(options.min_activity_count) +
            ",\"decoration\":" +
            json_string(options.decoration == HeuristicNetDecoration::kPerformance
                            ? "performance"
                            : "frequency") +
            ",\"performance_aggregation\":" +
            json_string(heuristic_aggregation_json_name(
                options.performance_aggregation)) +
            ",\"dfg_pre_cleaning_noise_threshold\":" +
            std::to_string(options.dfg_pre_cleaning_noise_threshold) +
            ",\"loop_length_two_threshold\":" +
            std::to_string(options.loop_length_two_threshold) +
            ",\"keep_once_per_case\":" +
            (options.keep_once_per_case ? "true" : "false") +
            ",\"start_timestamp_attribute\":";
  result += options.start_timestamp_attribute
                ? json_string(*options.start_timestamp_attribute)
                : "null";
  result += ",\"lifecycle_instance_attribute\":" +
            json_string(options.lifecycle_instance_attribute) +
            ",\"use_business_hours\":" +
            (options.use_business_hours ? "true" : "false") +
            ",\"business_hour_slots_explicit\":" +
            (options.business_hour_slots_explicit ? "true" : "false") +
            ",\"business_hour_slots_count\":" +
            std::to_string(options.business_hour_slots.size()) +
            ",\"case_id_attribute\":" +
            json_string(options.case_id_attribute);
  result += ",\"plusplus_decoration_selector\":";
  result += options.plusplus_decoration_selector
                ? json_string(*options.plusplus_decoration_selector)
                : "null";
  result += ",\"most_common_paths\":[";
  std::size_t path_index = 0;
  for (const auto& [source, target] : options.most_common_paths) {
    if (path_index++ != 0) result += ',';
    result += '[' + json_string(source) + ',' + json_string(target) + ']';
  }
  result += "],\"business_hour_slots\":[";
  for (std::size_t index = 0; index < options.business_hour_slots.size(); ++index) {
    if (index != 0) result += ',';
    const auto& slot = options.business_hour_slots[index];
    result += "{\"start_seconds\":" + std::to_string(slot.start_seconds) +
              ",\"end_seconds\":" + std::to_string(slot.end_seconds) + "}";
  }
  return result + "]}";
}

std::string petri_transition_labels_json(const PetriNet& net) {
  std::string result = "[";
  for (std::size_t index = 0; index < net.transitions().size(); ++index) {
    if (index != 0) result += ',';
    const auto& transition = net.transitions()[index];
    if (transition.silent()) {
      result += "null";
      continue;
    }
    assert(*transition.activity < net.activities().size());
    result += json_string(net.activities()[*transition.activity]);
  }
  return result + ']';
}

std::string native_marking_json(const PetriNet& net, bool initial) {
  if (net.place_count() == 0 ||
      (initial ? !net.has_source() : !net.has_sink()))
    return "[]";
  const auto markings = default_workflow_markings(net);
  const auto& marking = initial ? markings.initial : markings.final;
  std::string result = "[";
  bool first = true;
  for (std::size_t place = 0; place < marking.size(); ++place) {
    if (marking[place] == 0) continue;
    if (!first) result += ',';
    first = false;
    result += "{\"place\":" + std::to_string(place) +
              ",\"tokens\":" + std::to_string(marking[place]) + "}";
  }
  return result + ']';
}

std::set<std::size_t> petri_place_set(
    const std::vector<std::size_t>& places) {
  const std::set<std::size_t> result(places.begin(), places.end());
  assert(result.size() == places.size());
  return result;
}

void assert_inputless_petri_shape(
    const HeuristicsNet& heuristics, const PetriNet& petri,
    const std::set<std::string>& expected_labels,
    const std::set<std::string>& expected_parked_labels,
    const std::optional<std::string>& expected_start_label,
    const std::optional<std::string>& expected_end_label,
    std::size_t expected_places, std::size_t expected_arcs) {
  assert(heuristics.edges.empty());
  assert(petri.has_source() && petri.has_sink());
  assert(petri.source() != petri.sink());
  assert(petri.place_count() == expected_places);
  assert(petri.transitions().size() == expected_labels.size());

  const std::set<std::string> registered_labels(petri.activities().begin(),
                                                petri.activities().end());
  assert(registered_labels == expected_labels);
  std::set<std::string> active_labels;
  for (const std::size_t activity_id : heuristics.active_activities) {
    assert(activity_id < heuristics.activities.size());
    active_labels.insert(heuristics.activities[activity_id]);
  }
  assert(active_labels == expected_labels);

  std::set<std::string> transition_labels;
  std::size_t arc_count = 0;
  std::set<std::size_t> parking_places;
  for (const auto& transition : petri.transitions()) {
    assert(!transition.silent());
    assert(*transition.activity < petri.activities().size());
    const auto& label = petri.activities()[*transition.activity];
    assert(transition_labels.insert(label).second);
    assert(transition.reset_inputs.empty());
    assert(transition.inhibitor_inputs.empty());
    arc_count += transition.inputs.size() + transition.outputs.size();

    const std::set<std::size_t> expected_inputs =
        expected_start_label && label == *expected_start_label
            ? std::set<std::size_t>{petri.source()}
            : std::set<std::size_t>{};
    assert(petri_place_set(transition.inputs) == expected_inputs);
    if (expected_parked_labels.contains(label)) {
      assert(transition.outputs.size() == 1);
      const auto parked = transition.outputs.front();
      assert(parked != petri.source() && parked != petri.sink());
      assert(parking_places.insert(parked).second);
    } else {
      assert(expected_end_label && label == *expected_end_label);
      assert(petri_place_set(transition.outputs) ==
             std::set<std::size_t>{petri.sink()});
    }
  }
  assert(transition_labels == expected_labels);
  assert(parking_places.size() == expected_parked_labels.size());

  // Parking places keep sinkless output tokens visible and have no consumers.
  for (const std::size_t place : parking_places) {
    std::size_t producers = 0;
    std::size_t consumers = 0;
    for (const auto& transition : petri.transitions()) {
      producers += static_cast<std::size_t>(
          std::count(transition.outputs.begin(), transition.outputs.end(), place));
      consumers += static_cast<std::size_t>(
          std::count(transition.inputs.begin(), transition.inputs.end(), place));
      assert(std::find(transition.reset_inputs.begin(),
                       transition.reset_inputs.end(), place) ==
             transition.reset_inputs.end());
      assert(std::find(transition.inhibitor_inputs.begin(),
                       transition.inhibitor_inputs.end(), place) ==
             transition.inhibitor_inputs.end());
    }
    assert(producers == 1);
    assert(consumers == 0);
  }
  assert(arc_count == expected_arcs);

  const auto markings = default_workflow_markings(petri);
  assert(markings.initial.size() == petri.place_count());
  assert(markings.final.size() == petri.place_count());
  for (std::size_t place = 0; place < petri.place_count(); ++place) {
    assert(markings.initial[place] == (place == petri.source() ? 1 : 0));
    assert(markings.final[place] == (place == petri.sink() ? 1 : 0));
  }
}

EventLog inputless_sabae_event_log() {
  const auto dated_event = [](std::string label, std::string timestamp) {
    return classic_timestamp_event(std::move(label), timestamp, timestamp,
                                   std::nullopt);
  };
  EventLog result = classic_timestamp_log({
      {dated_event("S", "2026-01-01T00:00:00+00:00"),
       dated_event("A", "2026-01-01T00:00:10+00:00"),
       dated_event("B", "2026-01-01T00:00:20+00:00"),
       dated_event("A", "2026-01-01T00:00:30+00:00"),
       dated_event("E", "2026-01-01T00:00:40+00:00")},
  });
  auto& only_trace = result.traces.front();
  only_trace.case_id = "inputless-probe";
  only_trace.case_id_state = FirstClassAttributeState::kPresent;
  only_trace.case_id_source_key = "case:concept:name";
  for (auto& current_event : only_trace.events) {
    current_event.case_id = only_trace.case_id;
    current_event.case_id_state = FirstClassAttributeState::kPresent;
    current_event.case_id_source_key = "case:concept:name";
  }
  set_classic_timezone(result, "UTC");
  return result;
}

void assert_inputless_sabae_dates(const EventLog& source) {
  static constexpr std::string_view labels[]{"S", "A", "B", "A", "E"};
  static constexpr std::string_view timestamps[]{
      "2026-01-01T00:00:00+00:00", "2026-01-01T00:00:10+00:00",
      "2026-01-01T00:00:20+00:00", "2026-01-01T00:00:30+00:00",
      "2026-01-01T00:00:40+00:00"};
  assert(source.traces.size() == 1);
  assert(source.traces.front().case_id == "inputless-probe");
  assert(source.traces.front().events.size() == std::size(labels));
  for (std::size_t index = 0; index < std::size(labels); ++index) {
    const auto& current_event = source.traces.front().events[index];
    assert(current_event.activity == labels[index]);
    assert(current_event.timestamp == timestamps[index]);
    assert(current_event.timestamp_attribute.has_value());
    assert(current_event.timestamp_attribute->kind == AttributeKind::kDate);
    const auto* completion =
        std::get_if<NativeDate>(&current_event.timestamp_attribute->typed_value);
    assert(completion != nullptr);
    assert(completion->value == timestamps[index]);
    assert(completion->timezone == std::optional<std::string>{"UTC"});
    const auto start = std::find_if(
        current_event.attributes.begin(), current_event.attributes.end(),
        [](const EventAttribute& attribute) {
          return attribute.key == "start_timestamp";
        });
    assert(start != current_event.attributes.end());
    assert(start->kind == AttributeKind::kDate);
    const auto* start_date = std::get_if<NativeDate>(&start->typed_value);
    assert(start_date != nullptr);
    assert(start_date->value == timestamps[index]);
    assert(start_date->timezone == std::optional<std::string>{"UTC"});
  }
}

void heuristic_inputless_petri_acceptance(
    const std::optional<std::string>& evidence_path = std::nullopt) {
  constexpr std::string_view direct_name =
      "metadata_3_0_0_0_direct_classic_inputless";
  constexpr std::string_view eventlog_name =
      "one-trace-S-A-B-A-E-dependency-threshold-1";

  auto direct_input = classic_input(
      {{{"A", "B"}, 1}, {{"B", "A"}, 2}});
  HeuristicMinerOptions direct_options;
  const auto direct_input_snapshot = [&] {
    ClassicObjectEvidenceFixture fixture{
        std::string(direct_name), direct_input, direct_options, {}, {}};
    return classic_object_input_json(fixture);
  }();
  const auto direct_options_snapshot =
      heuristic_options_snapshot_json(direct_options);
  const auto direct_graph_log = log({
      trace({event("A", ""), event("B", "")}),
      trace({event("B", ""), event("A", "")}),
      trace({event("B", ""), event("A", "")}),
  });
  const auto direct_graph = discover_directly_follows_graph(direct_graph_log);
  assert(direct_graph.edge_count("A", "B") == 1);
  assert(direct_graph.edge_count("B", "A") == 2);
  const auto direct_heuristics =
      discover_heuristics_net(direct_input, direct_options);
  const auto direct_petri = to_petri_net(direct_heuristics);
  assert_inputless_petri_shape(
      direct_heuristics, direct_petri, {"A", "B"}, {"A", "B"},
      std::nullopt, std::nullopt, 4, 2);
  const auto direct_input_after = [&] {
    ClassicObjectEvidenceFixture fixture{
        std::string(direct_name), direct_input, direct_options, {}, {}};
    return classic_object_input_json(fixture);
  }();
  assert(direct_input_snapshot == direct_input_after);
  assert(direct_options_snapshot ==
         heuristic_options_snapshot_json(direct_options));

  const auto direct_analysis = analysis_json(
      direct_name, direct_graph_log, direct_graph, nullptr, &direct_petri,
      &direct_heuristics);
  assert(direct_analysis.find("\"petri_net\"") != std::string::npos);
  assert(direct_analysis.find("\"heuristics_net\"") != std::string::npos);
  std::vector<std::string> records;
  records.push_back(
      "{\"name\":" + json_string(direct_name) +
      ",\"fixture\":" + json_string(direct_name) +
      ",\"route\":\"direct_dfg\",\"variant\":\"classic\""
      ",\"status\":\"result\",\"reference_parameters\":{}"
      ",\"options\":" + direct_options_snapshot +
      ",\"options_snapshot_before\":" + direct_options_snapshot +
      ",\"options_snapshot_after\":" +
      heuristic_options_snapshot_json(direct_options) +
      ",\"input_snapshot_before\":" + direct_input_snapshot +
      ",\"input_snapshot_after\":" + direct_input_after +
      ",\"activity_id_to_name\":" + activity_mapping_json(direct_graph) +
      ",\"analysis_json\":" + direct_analysis +
      ",\"petri_transition_labels\":" +
      petri_transition_labels_json(direct_petri) +
      ",\"native_initial_marking\":" +
      native_marking_json(direct_petri, true) +
      ",\"native_final_marking\":" +
      native_marking_json(direct_petri, false) + "}");

  const auto source = inputless_sabae_event_log();
  assert_inputless_sabae_dates(source);
  const auto event_input_snapshot =
      serialize_event_table_json(event_log_to_table(source));
  const std::string event_reference_parameters =
      "{\"dependency_thresh\":1}";
  const std::set<std::string> event_labels{"S", "A", "B", "E"};
  const std::set<std::string> event_parked_labels{"S", "A", "B"};
  for (const HeuristicMinerVariant variant :
       {HeuristicMinerVariant::kClassic, HeuristicMinerVariant::kPlusPlus}) {
    HeuristicMinerOptions options;
    options.variant = variant;
    options.dependency_threshold = 1.0;
    const auto options_before = heuristic_options_snapshot_json(options);
    const auto heuristics = discover_heuristics_net(source, options);
    const auto petri = to_petri_net(heuristics);
    assert_inputless_petri_shape(heuristics, petri, event_labels,
                                 event_parked_labels, std::string("S"),
                                 std::string("E"), 5, 5);
    const auto options_after = heuristic_options_snapshot_json(options);
    const auto event_input_after =
        serialize_event_table_json(event_log_to_table(source));
    assert(options_before == options_after);
    assert(event_input_snapshot == event_input_after);
    assert_inputless_sabae_dates(source);

    const auto graph = discover_directly_follows_graph(source);
    const auto analysis =
        analysis_json(eventlog_name, source, graph, nullptr, &petri, &heuristics);
    assert(analysis.find("\"petri_net\"") != std::string::npos);
    assert(analysis.find("\"heuristics_net\"") != std::string::npos);
    const auto variant_name = heuristic_variant_json_name(variant);
    const std::string record_name =
        std::string(eventlog_name) +
        (variant == HeuristicMinerVariant::kClassic ? "-CLASSIC" : "-PLUSPLUS");
    records.push_back(
        "{\"name\":" + json_string(record_name) +
        ",\"fixture\":" + json_string(eventlog_name) +
        ",\"route\":\"EventLog\",\"variant\":" +
        json_string(variant_name) +
        ",\"status\":\"result\",\"reference_parameters\":" +
        event_reference_parameters + ",\"options\":" + options_before +
        ",\"options_snapshot_before\":" + options_before +
        ",\"options_snapshot_after\":" + options_after +
        ",\"input_snapshot_before\":" + event_input_snapshot +
        ",\"input_snapshot_after\":" + event_input_after +
        ",\"activity_id_to_name\":" + activity_mapping_json(graph) +
        ",\"analysis_json\":" + analysis +
        ",\"petri_transition_labels\":" +
        petri_transition_labels_json(petri) +
        ",\"native_initial_marking\":" + native_marking_json(petri, true) +
        ",\"native_final_marking\":" + native_marking_json(petri, false) +
        "}");
  }

  if (evidence_path) {
    std::ofstream output(*evidence_path, std::ios::binary);
    if (!output)
      throw std::runtime_error("unable to open inputless Petri evidence output");
    output << '[';
    for (std::size_t index = 0; index < records.size(); ++index) {
      if (index != 0) output << ',';
      output << records[index];
    }
    output << "]\n";
    if (!output)
      throw std::runtime_error("unable to write inputless Petri evidence output");
  }
}

void plusplus_raw_decoration_selector_acceptance() {
  const auto abc = trace({
      interval("A", "2024-01-01T00:00:00Z", "2024-01-01T00:00:01Z"),
      interval("B", "2024-01-01T00:00:02Z", "2024-01-01T00:00:03Z"),
      interval("C", "2024-01-01T00:00:04Z", "2024-01-01T00:00:05Z")});
  const auto ab = trace({abc.events[0], abc.events[1]});
  const auto source = log({abc, abc, ab});
  auto options = plus_options();
  options.dependency_threshold = 0.5;
  options.and_threshold = 0.5;
  const auto baseline = discover_heuristics_net(source, options);
  struct Control {
    HeuristicNetDecoration configured;
    std::string selector;
    HeuristicNetDecoration effective;
  };
  // The unfamiliar literal is the captured supplement control. Other raw
  // strings follow the native API's documented selector rule.
  const std::vector<Control> controls{
      {HeuristicNetDecoration::kPerformance, "frequency",
       HeuristicNetDecoration::kFrequency},
      {HeuristicNetDecoration::kFrequency, "performance",
       HeuristicNetDecoration::kPerformance},
      {HeuristicNetDecoration::kFrequency, "supplement-unfamiliar",
       HeuristicNetDecoration::kPerformance}};
  const HeuristicMeanPerformanceDfg means{
      {{"A", "B"}, 1.0}, {{"B", "C"}, 1.0}};
  for (const auto& control : controls) {
    options.decoration = control.configured;
    options.plusplus_decoration_selector = control.selector;
    const auto before = heuristic_options_snapshot_json(options);
    const auto net = discover_heuristics_net(source, options);
    assert(heuristic_options_snapshot_json(options) == before);
    assert(options.decoration == control.configured);
    assert(options.plusplus_decoration_selector == control.selector);
    assert(net.decoration == control.effective);
    assert(net.activities == baseline.activities);
    assert(net.active_activities == baseline.active_activities);
    assert(net.start_activities == baseline.start_activities);
    assert(net.end_activities == baseline.end_activities);
    assert(net.output_bundles == baseline.output_bundles);
    assert(net.input_bundles == baseline.input_bundles);
    assert(net.edges.size() == 2);
    assert(net.relation_statistics.size() == 2);
    assert(net.concurrency_statistics.empty());
    assert(net.loop_length_two.empty());
    assert(net.and_measures_in.empty() && net.and_measures_out.empty());
    assert(activity_stat(net, "A").occurrences == 3);
    assert(activity_stat(net, "B").occurrences == 3);
    assert(activity_stat(net, "C").occurrences == 2);
    for (const auto& stats : net.activity_statistics) {
      assert(stats.is_start_activity == false);
      assert(stats.is_end_activity == false);
    }
    for (const auto& names : std::vector<std::pair<std::string, std::string>>{
             {"A", "B"}, {"B", "C"}}) {
      const auto expected_count = names.first == "A" ? 3U : 2U;
      const auto& stats = relation(net, names.first, names.second);
      assert(stats.eventually_follows_frequency == expected_count);
      assert(stats.direct_adjacency_frequency == expected_count);
      assert(stats.raw_frequency == expected_count);
      assert(stats.cleaned_frequency == expected_count);
      assert(stats.mean_wait_seconds == 1.0);
      const auto* accepted = edge(net, names.first, names.second);
      assert(accepted != nullptr);
      assert(accepted->frequency == expected_count);
      assert(accepted->dependency == 1.0);
      const auto* measure = dependency_measure(net, names.first, names.second);
      assert(measure != nullptr);
      assert(measure->value == 1.0);
      const double expected_representation =
          control.effective == HeuristicNetDecoration::kFrequency
              ? static_cast<double>(expected_count) : 1.0;
      assert(stats.representation == expected_representation);
      assert(accepted->representation == expected_representation);
      if (control.effective == HeuristicNetDecoration::kPerformance) {
        assert(!stats.representation_value);
        assert(!accepted->representation_value);
      } else {
        assert(stats.representation_value && accepted->representation_value);
        assert(std::get<std::uint64_t>(std::get<HeuristicStructuredValue>(
                   *stats.representation_value).value) == expected_count);
        assert(std::get<std::uint64_t>(std::get<HeuristicStructuredValue>(
                   *accepted->representation_value).value) == expected_count);
      }
    }
    if (control.effective == HeuristicNetDecoration::kPerformance) {
      assert(net.plusplus_computed_performance_dfg);
      assert(*net.plusplus_computed_performance_dfg == means);
      assert(net.plusplus_computed_performance_matrix == means);
    } else {
      assert(!net.plusplus_computed_performance_dfg);
      assert((net.plusplus_computed_performance_matrix ==
              HeuristicMeanPerformanceDfg{{{"A", "B"}, 0.0},
                                          {{"B", "C"}, 0.0}}));
    }
  }
  // CLASSIC retains its enum behavior even with a conflicting raw selector.
  options.variant = HeuristicMinerVariant::kClassic;
  const auto graph = discover_directly_follows_graph(source);
  for (const auto decoration : {HeuristicNetDecoration::kFrequency,
                                HeuristicNetDecoration::kPerformance}) {
    options.decoration = decoration;
    options.plusplus_decoration_selector.reset();
    const auto omitted = discover_heuristics_net(source, options);
    const auto omitted_json = analysis_json("classic-selector", source, graph,
                                           nullptr, nullptr, &omitted);
    options.plusplus_decoration_selector =
        decoration == HeuristicNetDecoration::kFrequency
            ? "performance" : "frequency";
    const auto before = heuristic_options_snapshot_json(options);
    const auto explicit_selector = discover_heuristics_net(source, options);
    assert(heuristic_options_snapshot_json(options) == before);
    assert(explicit_selector.decoration == decoration);
    assert(analysis_json("classic-selector", source, graph,
                         nullptr, nullptr, &explicit_selector) == omitted_json);
  }
}

void plusplus_computed_public_fields_acceptance() {
  const auto public_fields = [](const EventLog& source,
                                const HeuristicsNet& net) {
    const auto graph = discover_directly_follows_graph(source);
    const auto json = analysis_json("plusplus-computed-fields", source, graph,
                                    nullptr, nullptr, &net);
    const auto begin = json.find("\"plusplus_public_fields\": {");
    assert(begin != std::string::npos);
    const auto end = json.find(", \"and_measures_in\":", begin);
    assert(end != std::string::npos);
    return json.substr(begin, end - begin);
  };
  // Captured positive waits 3 and 7 distinguish frequency decoration's absent
  // performance DFG and zero matrix from the actual mean wait of five.
  const auto waits = log({
      trace({interval("A", "2026-04-01T00:00:00Z", "2026-04-01T00:00:01Z"),
             interval("B", "2026-04-01T00:00:04Z", "2026-04-01T00:00:05Z")}),
      trace({interval("A", "2026-04-02T00:00:00Z", "2026-04-02T00:00:01Z"),
             interval("B", "2026-04-02T00:00:08Z", "2026-04-02T00:00:09Z")}),
  });
  auto options = plus_options();
  options.and_threshold = 0.5;
  const auto frequency = discover_heuristics_net(waits, options);
  assert(!frequency.plusplus_computed_performance_dfg.has_value());
  assert((frequency.plusplus_computed_performance_matrix ==
          HeuristicMeanPerformanceDfg{{{"A", "B"}, 0.0}}));
  assert(relation(frequency, "A", "B").mean_wait_seconds == 5.0);
  assert(!frequency.plusplus_public_dfg_window_2_present);
  assert(!frequency.plusplus_public_freq_triples_present);
  assert(frequency.classic_public_dfg_window_2_present);
  assert(frequency.classic_public_freq_triples_present);
  assert(!frequency.performance_values.has_value());
  assert(public_fields(waits, frequency) ==
         "\"plusplus_public_fields\": {\"dfg_window_2\": null, "
         "\"freq_triples\": null, \"performance_dfg\": null, "
         "\"performance_matrix\": {\"A\": {\"B\": 0.0}}}");
  options.decoration = HeuristicNetDecoration::kPerformance;
  const auto performance = discover_heuristics_net(waits, options);
  assert(performance.plusplus_computed_performance_dfg.has_value());
  assert(((*performance.plusplus_computed_performance_dfg) ==
          HeuristicMeanPerformanceDfg{{{"A", "B"}, 5.0}}));
  assert(performance.plusplus_computed_performance_matrix ==
         *performance.plusplus_computed_performance_dfg);
  assert(!performance.performance_values.has_value());
  assert(public_fields(waits, performance).find(
             "\"performance_dfg\": [{\"key\": [\"A\", \"B\"], "
             "\"value\": 5.0}]") != std::string::npos);

  // Literal five-trace filter_overlap control from the fourteen captured
  // performance rows: four overlapping AB/CD permutations, then atomic ABD.
  const auto first = trace({
      interval("A", "2024-01-01T00:00:00Z", "2024-01-01T00:00:01Z"),
      interval("B", "2024-01-01T00:00:02Z", "2024-01-01T00:00:06Z"),
      interval("C", "2024-01-01T00:00:03Z", "2024-01-01T00:00:05Z"),
      interval("D", "2024-01-01T00:00:07Z", "2024-01-01T00:00:08Z")});
  const auto second = trace({
      interval("A", "2024-01-01T00:00:00Z", "2024-01-01T00:00:01Z"),
      interval("C", "2024-01-01T00:00:02Z", "2024-01-01T00:00:06Z"),
      interval("B", "2024-01-01T00:00:03Z", "2024-01-01T00:00:05Z"),
      interval("D", "2024-01-01T00:00:07Z", "2024-01-01T00:00:08Z")});
  const auto filtered_source = log({first, second, first, second, trace({
      interval("A", "2024-01-01T00:00:00Z", "2024-01-01T00:00:01Z"),
      interval("B", "2024-01-01T00:00:02Z", "2024-01-01T00:00:03Z"),
      interval("D", "2024-01-01T00:00:04Z", "2024-01-01T00:00:05Z")})});
  const HeuristicMeanPerformanceDfg baseline{
      {{"A", "B"}, 1.0}, {{"A", "C"}, 1.0},
      {{"B", "D"}, 5.0 / 3.0}, {{"C", "D"}, 2.0}};
  struct FilterControl {
    std::uint64_t min_activity;
    std::uint64_t min_frequency;
    HeuristicMeanPerformanceDfg expected;
  };
  const std::vector<FilterControl> controls{
      {1, 1, baseline},
      {5, 1, {{{"A", "B"}, 1.0}, {{"B", "D"}, 5.0 / 3.0}}},
      {1, 5, {{{"B", "D"}, 5.0 / 3.0}}},
      {5, 5, {{{"B", "D"}, 5.0 / 3.0}}},
      {1000, 1, {}}};
  for (const auto& control : controls) {
    options.min_activity_count = control.min_activity;
    options.min_frequency = control.min_frequency;
    const auto net = discover_heuristics_net(filtered_source, options);
    assert(net.plusplus_computed_performance_dfg.has_value());
    assert(*net.plusplus_computed_performance_dfg == control.expected);
    assert(net.plusplus_computed_performance_matrix == control.expected);
    if (!control.expected.empty()) {
      const auto& bd = relation(net, "B", "D");
      assert(bd.eventually_follows_frequency == 5);
      assert(bd.direct_adjacency_frequency == 3);
      assert((bd.wait_samples == std::vector<double>{2.0, 2.0, 1.0}));
    } else {
      assert(net.activities.empty());
      assert(public_fields(filtered_source, net).find(
                 "\"performance_dfg\": [], \"performance_matrix\": {}") !=
             std::string::npos);
    }
    auto frequency_options = options;
    frequency_options.decoration = HeuristicNetDecoration::kFrequency;
    const auto frequency_net =
        discover_heuristics_net(filtered_source, frequency_options);
    assert(!frequency_net.plusplus_computed_performance_dfg);
    auto expected_zero_matrix = control.expected;
    for (auto& [_, value] : expected_zero_matrix) value = 0.0;
    assert(frequency_net.plusplus_computed_performance_matrix ==
           expected_zero_matrix);
  }
  options.min_activity_count = options.min_frequency = 1;
  options.dependency_threshold = 1.0;
  const auto no_edges = discover_heuristics_net(filtered_source, options);
  assert(no_edges.edges.empty());
  assert(*no_edges.plusplus_computed_performance_dfg == baseline);
  assert(no_edges.plusplus_computed_performance_matrix == baseline);
  options.dependency_threshold = 0.5;
  options.performance_aggregation = HeuristicPerformanceAggregation::kMedian;
  const auto median = discover_heuristics_net(filtered_source, options);
  assert(relation(median, "B", "D").representation == 2.0);
  assert(*median.plusplus_computed_performance_dfg == baseline);
  assert(median.plusplus_computed_performance_matrix == baseline);

  // Actual interval control AC1 is EFG-only; BC1 has its direct one-second wait.
  const auto interval_source = log({trace({
      interval("A", "2024-01-01T00:00:00Z", "2024-01-01T00:00:05Z"),
      interval("B", "2024-01-01T00:00:02Z", "2024-01-01T00:00:06Z"),
      interval("C", "2024-01-01T00:00:07Z", "2024-01-01T00:00:08Z")})});
  options.performance_aggregation = HeuristicPerformanceAggregation::kMean;
  const auto interval_net = discover_heuristics_net(interval_source, options);
  assert(relation(interval_net, "A", "C").eventually_follows_frequency == 1);
  assert(relation(interval_net, "A", "C").direct_adjacency_frequency == 0);
  assert(((*interval_net.plusplus_computed_performance_dfg) ==
          HeuristicMeanPerformanceDfg{{{"B", "C"}, 1.0}}));
  assert((interval_net.plusplus_computed_performance_matrix ==
          HeuristicMeanPerformanceDfg{{{"A", "C"}, 0.0}, {{"B", "C"}, 1.0}}));
  assert(public_fields(interval_source, interval_net).find(
             "\"performance_matrix\": {\"A\": {\"C\": 0.0}, "
             "\"B\": {\"C\": 1.0}}") != std::string::npos);
  const EventLog empty;
  const auto empty_net = discover_heuristics_net(empty, options);
  assert(empty_net.plusplus_computed_performance_dfg.has_value());
  assert(empty_net.plusplus_computed_performance_dfg->empty());
  assert(empty_net.plusplus_computed_performance_matrix.empty());
  assert(public_fields(empty, empty_net).find(
             "\"performance_dfg\": [], \"performance_matrix\": {}") !=
         std::string::npos);

  // Appended PLUSPLUS flags preserve concrete legacy maps independently of
  // CLASSIC presence, and supplied performance_values remain an input carrier.
  HeuristicsNet legacy;
  legacy.variant = HeuristicMinerVariant::kPlusPlus;
  legacy.activities = {"A", "B", "C", "D"};
  assert(legacy.plusplus_public_dfg_window_2_present);
  assert(legacy.plusplus_public_freq_triples_present);
  assert(public_fields(filtered_source, legacy).find(
             "\"dfg_window_2\": [], \"freq_triples\": []") !=
         std::string::npos);
  legacy.window_two_dfg[{"A", "B"}] = 2;
  legacy.frequency_triples[{"A", "B", "C"}] = 3;
  assert(public_fields(filtered_source, legacy).find(
             "\"dfg_window_2\": [{\"key\": [\"A\", \"B\"], "
             "\"value\": 2}], \"freq_triples\": [{\"key\": "
             "[\"A\", \"B\", \"C\"], \"value\": 3}]") !=
         std::string::npos);
  const auto classic = discover_heuristics_net(waits, HeuristicMinerOptions{});
  assert(classic.plusplus_public_dfg_window_2_present);
  assert(classic.plusplus_public_freq_triples_present);
  assert(!classic.plusplus_computed_performance_dfg);
  assert(classic.plusplus_computed_performance_matrix.empty());
  const auto classic_graph = discover_directly_follows_graph(waits);
  const auto classic_json = analysis_json("classic-unmodified", waits,
      classic_graph, nullptr, nullptr, &classic);
  assert(classic_json.find("\"classic_public_fields\"") != std::string::npos);
  assert(classic_json.find("\"plusplus_public_fields\"") == std::string::npos);
  HeuristicsNet escaped;
  escaped.variant = HeuristicMinerVariant::kPlusPlus;
  const std::string source_name = "A\"\\\n";
  const std::string target_name = "B\\\"";
  escaped.plusplus_computed_performance_dfg =
      HeuristicMeanPerformanceDfg{{{source_name, target_name}, 1.0}};
  escaped.plusplus_computed_performance_matrix =
      HeuristicMeanPerformanceDfg{{{source_name, target_name}, -0.0}};
  escaped.performance_values.emplace();
  escaped.performance_values->emplace(
      std::pair{source_name, target_name}, HeuristicPerformanceValue{77.0});
  const auto escaped_json = public_fields(empty, escaped);
  assert(escaped_json.find("\"key\": [" + json_string(source_name) + ", " +
                           json_string(target_name) + "], \"value\": 1.0") !=
         std::string::npos);
  assert(escaped_json.find(json_string(source_name) + ": {" +
                           json_string(target_name) + ": -0.0}") !=
         std::string::npos);
  assert(std::get<double>(escaped.performance_values->begin()->second) == 77.0);
  for (const bool dfg : {false, true}) {
    auto invalid = escaped;
    if (dfg)
      invalid.plusplus_computed_performance_dfg->begin()->second =
          std::numeric_limits<double>::quiet_NaN();
    else
      invalid.plusplus_computed_performance_matrix.begin()->second =
          std::numeric_limits<double>::infinity();
    bool rejected = false;
    try { (void)public_fields(empty, invalid); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  }
}

}  // namespace

int main(int argc, char** argv) {
  std::optional<std::string> classic_flat_evidence;
  std::optional<std::string> classic_object_evidence;
  std::optional<std::string> classic_auxiliary_evidence;
  std::optional<std::string> classic_nonfinite_evidence;
  std::optional<std::string> classic_dfg_order_evidence;
  std::optional<std::string> heuristic_inputless_evidence;
  if (argc != 1) {
    if (argc == 3 && std::string_view(argv[1]) == "--classic-flat-evidence") {
      classic_flat_evidence = argv[2];
    } else if (argc == 3 &&
               std::string_view(argv[1]) == "--classic-object-evidence") {
      classic_object_evidence = argv[2];
    } else if (argc == 3 &&
               std::string_view(argv[1]) == "--classic-auxiliary-evidence") {
      classic_auxiliary_evidence = argv[2];
    } else if (argc == 3 &&
               std::string_view(argv[1]) == "--classic-nonfinite-evidence") {
      classic_nonfinite_evidence = argv[2];
    } else if (argc == 3 &&
               std::string_view(argv[1]) == "--classic-dfg-order-evidence") {
      classic_dfg_order_evidence = argv[2];
    } else if (argc == 3 &&
               std::string_view(argv[1]) == "--heuristic-inputless-evidence") {
      heuristic_inputless_evidence = argv[2];
    } else {
      throw std::invalid_argument(
          "usage: test_heuristic_variants "
          "[--classic-flat-evidence PATH | --classic-object-evidence PATH | "
          "--classic-nonfinite-evidence PATH | "
          "--classic-dfg-order-evidence PATH | "
          "--classic-auxiliary-evidence PATH | "
          "--heuristic-inputless-evidence PATH]");
    }
  }
  interval_abstraction_and_strict_threshold();
  plusplus_frequency_representation_preserves_integer_type();
  plusplus_computed_public_fields_acceptance();
  plusplus_raw_decoration_selector_acceptance();
  plusplus_pandas_directional_concurrency_acceptance();
  plusplus_pandas_duration_mutation_and_start_fallback();
  plusplus_pandas_object_dtype_error_preserves_input();
  plusplus_pandas_missing_start_error_mutation();
  plusplus_pandas_exact_duration_and_performance();
  plusplus_pandas_nat_statistics_and_error_order();
  plusplus_pandas_occurrence_order_and_self_concurrency();
  plusplus_pandas_concurrency_rounding_keeps_exact_waits();
  plusplus_pandas_signed_array_duration_components();
  plusplus_numeric_count_filters_preserve_public_comparisons();
  plusplus_pandas_literal_selectors_and_error_order();
  plusplus_pandas_duration_selector_collisions();
  lifecycle_fifo_and_configurable_interval_attribute();
  lifecycle_input_shapes_defer_ignored_timestamp_parsing();
  plusplus_returned_object_fields_acceptance();
  filtering_and_unfiltered_statistics();
  dependency_concurrency_and_no_connect_all();
  and_bundles_are_overlapping_maximal_cliques();
  and_measure_payload_matches_threshold_boundaries();
  dependency_and_loop_payloads_keep_filtered_values();
  classic_dfg_metadata_registry_and_lookup_order();
  performance_sorting_and_mean();
  invalid_timestamps_throw_only_for_plus_plus();
  classic_defaults_dependency_filters_and_self_loop();
  classic_noise_cleaning_and_keep_once();
  classic_and_maximal_cliques_at_equality();
  classic_length_two_rescue_and_reference_quirk();
  classic_length_two_rescue_requires_occurrence_membership();
  classic_large_count_ratios_and_representations();
  classic_active_nodes_conversion_and_dfg_inference();
  classic_performance_aggregations_and_validation();
  classic_decorated_dfg_values_preserve_typed_payloads();
  classic_nonfinite_object_payloads_preserve_typed_values(
      classic_nonfinite_evidence);
  classic_dfg_order_preserves_inferred_boundary_lists(classic_dfg_order_evidence);
  start_selector_defaults_and_validation();
  business_hours_cover_heuristic_waits_and_sojourns();
  subsecond_timestamps_preserve_exact_order_and_business_prefix();
  configurable_activity_and_timestamp_fields();
  classic_flat_timestamp_acceptance(classic_flat_evidence);
  classic_object_fields_acceptance(classic_object_evidence,
                                   classic_auxiliary_evidence);
  heuristic_inputless_petri_acceptance(heuristic_inputless_evidence);
  std::cout << "heuristic variant tests passed\n";
}
