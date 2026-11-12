#include "minera/dfg_dataframe.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace minera;

EventAttribute cell(std::string key, std::string value,
                    bool is_null = false) {
  EventAttribute result;
  result.key = std::move(key);
  result.value = std::move(value);
  result.is_null = is_null;
  return result;
}

EventTable make_table(std::vector<std::string> columns,
                      std::vector<EventTable::Row> rows) {
  EventTable result;
  result.columns = std::move(columns);
  result.rows = std::move(rows);
  return result;
}

EventTable::Row row(std::initializer_list<std::optional<EventAttribute>> cells) {
  return EventTable::Row(cells);
}

const std::optional<EventAttribute> value(std::string key, std::string text) {
  return cell(std::move(key), std::move(text));
}

const std::optional<EventAttribute> null_value(std::string key) {
  return cell(std::move(key), {}, true);
}

const std::optional<EventAttribute> integer_value(std::string key,
                                                  std::int64_t number) {
  auto result = cell(std::move(key), std::to_string(number));
  result.kind = AttributeKind::kInt;
  result.typed_value = number;
  return result;
}

const std::optional<EventAttribute> uint64_value(std::string key,
                                                  std::uint64_t number) {
  auto result = cell(std::move(key), std::to_string(number));
  result.kind = AttributeKind::kInt;
  result.typed_value = number;
  return result;
}

const std::optional<EventAttribute> float_value(std::string key, double number) {
  auto result = cell(std::move(key), std::to_string(number));
  result.kind = AttributeKind::kFloat;
  result.typed_value = number;
  return result;
}

EventTable graph_table() {
  return make_table(
      {"case:concept:name", "concept:name", "resource", "start_timestamp",
       "time:timestamp", "cost", "event_id"},
      {row({value("case:concept:name", "c1"), value("concept:name", "A"),
            value("resource", "r1"), value("start_timestamp", "2026-01-05T07:00:00Z"),
            value("time:timestamp", "2026-01-05T07:00:10Z"), value("cost", "2"),
            value("event_id", "10")}),
       row({value("case:concept:name", "c2"), value("concept:name", "X"),
            value("resource", "r9"), value("start_timestamp", "2026-01-05T07:00:01Z"),
            value("time:timestamp", "2026-01-05T07:00:02Z"), value("cost", "3"),
            value("event_id", "20")}),
       row({value("case:concept:name", "c1"), value("concept:name", "B"),
            value("resource", "r2"), value("start_timestamp", "2026-01-05T07:00:11Z"),
            value("time:timestamp", "2026-01-05T07:00:20Z"), value("cost", "4"),
            value("event_id", "11")}),
       row({value("case:concept:name", "c1"), value("concept:name", "B"),
            value("resource", "r2"), value("start_timestamp", "2026-01-05T07:00:21Z"),
            value("time:timestamp", "2026-01-05T07:00:30Z"), null_value("cost"),
            value("event_id", "12")})});
}

std::uint64_t count(const DfgFrequency& graph, std::string source,
                    std::string target) {
  const auto found = graph.find({std::move(source), std::move(target)});
  return found == graph.end() ? 0 : found->second;
}

void graph_measure_selectors_and_ordering() {
  const auto table = graph_table();
  DfgTableOptions options;
  options.sort_caseid_required = false;
  options.sort_timestamp_along_case_id = false;
  options.target_activity_attribute = "resource";
  auto pandas = discover_pandas_dfg(table, options);
  assert(pandas.backend == DfgTableBackend::kPandas);
  // Eager adapter uses a global shift and then filters by case. Interleaving
  // therefore removes the c1 adjacency while the lazy adapter keeps it.
  assert(count(pandas.frequency, "B", "r2") == 1);
  assert(pandas.frequency.size() == 1);
  auto lazy = discover_polars_dfg(LazyEventTable::from_table(table), options);
  assert(count(lazy.frequency, "A", "r2") == 1);
  assert(count(lazy.frequency, "B", "r2") == 1);

  options.target_activity_attribute.clear();
  options.window = 0;
  options.keep_once_per_case = true;
  pandas = discover_pandas_dfg(table, options);
  assert(count(pandas.frequency, "A", "A") == 1);
  assert(count(pandas.frequency, "B", "B") == 1);
  assert(count(pandas.frequency, "X", "X") == 1);
}

void dataframe_triples_contract() {
  const auto table = make_table(
      {"case:concept:name", "concept:name", "time:timestamp"},
      {row({value("case:concept:name", "c"), value("concept:name", "A"),
            value("time:timestamp", "2026-01-01T00:00:00Z")}),
       row({value("case:concept:name", "x"), value("concept:name", "X"),
            value("time:timestamp", "2026-01-01T00:00:00Z")}),
       row({value("case:concept:name", "c"), value("concept:name", "B"),
            value("time:timestamp", "2026-01-01T00:00:01Z")}),
       row({value("case:concept:name", "c"), value("concept:name", "C"),
            value("time:timestamp", "2026-01-01T00:00:02Z")})});
  DfgTableOptions options;
  options.sort_caseid_required = false;
  const auto eager = get_freq_triples(table, options);
  assert(eager.empty());
  const auto lazy = get_freq_triples(LazyEventTable::from_table(table), options);
  assert(lazy.at({"A", "B", "C"}) == 1);
}

void graph_performance_cost_and_aggregation() {
  const auto table = graph_table();
  DfgTableOptions options;
  options.measure = DfgTableMeasure::kBoth;
  options.start_timestamp_attribute = "start_timestamp";
  options.performance_aggregation = DfgPerformanceAggregation::kAll;
  auto result = discover_pandas_dfg(table, options);
  assert(count(result.frequency, "A", "B") == 1);
  const auto& stats = result.performance.at({"A", "B"});
  assert(stats.summary.count == 1);
  assert(stats.summary.mean == 1.0);
  assert(stats.summary.minimum == 1.0 && stats.summary.maximum == 1.0);
  assert(stats.summary.sum == 1.0);

  options.measure = DfgTableMeasure::kCost;
  options.cost_attribute = "cost";
  options.performance_aggregation = DfgPerformanceAggregation::kMean;
  result = discover_pandas_dfg(table, options);
  assert(result.performance.at({"A", "B"}).summary.mean == 4.0);
  // Null target cost is interpreted as zero.
  assert(result.performance.at({"B", "B"}).summary.mean == 0.0);

  options.custom_aggregation = [](const std::vector<double>& values) {
    double product = 1.0;
    for (double value : values) product *= value;
    return DfgPerformanceValue(product);
  };
  result = discover_pandas_dfg(table, options);
  assert(std::get<double>(result.performance.at({"A", "B"}).value) == 4.0);
}

void negative_duration_and_business_slots() {
  const auto table = make_table(
      {"case:concept:name", "concept:name", "time:timestamp"},
      {row({value("case:concept:name", "c"), value("concept:name", "A"),
            value("time:timestamp", "2026-01-05T12:00:10Z")}),
       row({value("case:concept:name", "c"), value("concept:name", "B"),
            value("time:timestamp", "2026-01-05T12:00:00Z")})});
  DfgTableOptions options;
  options.measure = DfgTableMeasure::kPerformance;
  options.start_timestamp_attribute = "time:timestamp";
  options.sort_caseid_required = false;
  options.sort_timestamp_along_case_id = false;
  const auto negative = discover_pandas_dfg(table, options);
  assert(negative.performance.at({"A", "B"}).summary.mean == -10.0);

  const auto timed = make_table(
      {"case:concept:name", "concept:name", "time:timestamp"},
      {row({value("case:concept:name", "c"), value("concept:name", "A"),
            value("time:timestamp", "2026-01-05T08:59:59Z")}),
       row({value("case:concept:name", "c"), value("concept:name", "B"),
            value("time:timestamp", "2026-01-05T10:00:00Z")})});
  options.business_hours = true;
  options.business_hour_slots = std::vector<BusinessHourSlot>{
      {9.0 * 3600.0, 9.0 * 3600.0 + 1800.0},
      {9.0 * 3600.0 + 1801.0, 10.0 * 3600.0}};
  // The one-second gap is merged. The local-wall-time duration is one hour.
  const auto merged = discover_pandas_dfg(timed, options);
  assert(merged.performance.at({"A", "B"}).summary.mean == 3600.0);
  options.business_hour_slots = std::vector<BusinessHourSlot>{};
  const auto empty = discover_pandas_dfg(timed, options);
  assert(empty.performance.at({"A", "B"}).summary.mean == 0.0);
  options.workcalendar = std::string("ignored");
  const auto calendar = discover_pandas_dfg(timed, options);
  assert(calendar.performance.at({"A", "B"}).summary.mean == 0.0);

  const auto existing_start = make_table(
      {"case:concept:name", "concept:name", "start_timestamp",
       "time:timestamp"},
      {row({value("case:concept:name", "c"), value("concept:name", "A"),
            value("start_timestamp", "2026-01-05T00:00:00Z"),
            value("time:timestamp", "2026-01-05T00:00:10Z")}),
       row({value("case:concept:name", "c"), value("concept:name", "B"),
            value("start_timestamp", "2026-01-05T00:00:05Z"),
            value("time:timestamp", "2026-01-05T00:00:20Z")})});
  options.business_hours = false;
  options.business_hour_slots.reset();
  options.start_timestamp_attribute.reset();
  const auto eager_existing_start = discover_pandas_dfg(
      existing_start, options);
  const auto lazy_existing_start = discover_polars_dfg(
      LazyEventTable::from_table(existing_start), options);
  assert(eager_existing_start.performance.at({"A", "B"}).summary.mean == -5.0);
  assert(lazy_existing_start.performance.at({"A", "B"}).summary.mean == 0.0);
}

void invalid_selectors_are_rejected() {
  const auto table = graph_table();
  DfgTableOptions options;
  options.activity_attribute = "does_not_exist";
  bool rejected = false;
  try {
    (void)discover_pandas_dfg(table, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  options.activity_attribute = "concept:name";
  options.timestamp_attribute = "bad_timestamp";
  options.measure = DfgTableMeasure::kPerformance;
  rejected = false;
  try {
    (void)discover_pandas_dfg(table, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  options.timestamp_attribute = "time:timestamp";
  options.start_timestamp_attribute = "missing_start";
  rejected = false;
  try {
    (void)discover_pandas_dfg(table, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  const auto lazy_fallback = discover_polars_dfg(
      LazyEventTable::from_table(table), options);
  assert(lazy_fallback.performance.at({"A", "B"}).summary.mean == 10.0);

  const auto reverse_time = make_table(
      {"case:concept:name", "concept:name", "time:timestamp"},
      {row({value("case:concept:name", "c"), value("concept:name", "A"),
            value("time:timestamp", "2026-01-05T00:00:10Z")}),
       row({value("case:concept:name", "c"), value("concept:name", "B"),
            value("time:timestamp", "2026-01-05T00:00:05Z")})});
  options.sort_caseid_required = false;
  options.sort_timestamp_along_case_id = false;
  const auto lazy_negative = discover_polars_dfg(
      LazyEventTable::from_table(reverse_time), options);
  assert(lazy_negative.performance.at({"A", "B"}).summary.mean == -5.0);

  options.measure = DfgTableMeasure::kCost;
  options.start_timestamp_attribute.reset();
  options.cost_attribute.reset();
  rejected = false;
  try {
    (void)discover_pandas_dfg(table, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  options.measure = DfgTableMeasure::kFrequency;
  options.cost_attribute = "does_not_exist";
  options.start_timestamp_attribute.reset();
  options.sort_caseid_required = true;
  options.sort_timestamp_along_case_id = true;
  const auto ignored_cost = discover_pandas_dfg(table, options);
  assert(count(ignored_cost.frequency, "A", "B") == 1);

  const auto invalid_cost = make_table(
      {"case:concept:name", "concept:name", "time:timestamp", "cost"},
      {row({value("case:concept:name", "c"), value("concept:name", "A"),
            value("time:timestamp", "2026-01-05T00:00:00Z"),
            value("cost", "1")}),
       row({value("case:concept:name", "c"), value("concept:name", "B"),
            value("time:timestamp", "2026-01-05T00:00:01Z"),
            value("cost", "invalid")})});
  options.measure = DfgTableMeasure::kCost;
  options.cost_attribute = "cost";
  rejected = false;
  try {
    (void)discover_pandas_dfg(invalid_cost, options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void timestamp_free_frequency_contracts() {
  const auto table = make_table(
      {"case:concept:name", "concept:name"},
      {row({value("case:concept:name", "c"),
            value("concept:name", "A")}),
       row({value("case:concept:name", "c"),
            value("concept:name", "B")}),
       row({value("case:concept:name", "c"),
            value("concept:name", "C")})});
  DfgTableOptions options;
  bool eager_rejected = false;
  try {
    (void)discover_pandas_dfg(table, options);
  } catch (const std::invalid_argument&) {
    eager_rejected = true;
  }
  assert(eager_rejected);
  const auto lazy = discover_polars_dfg(LazyEventTable::from_table(table), options);
  assert(count(lazy.frequency, "A", "B") == 1);
  assert(count(lazy.frequency, "B", "C") == 1);

  // The eager adapter still selects the timestamp column when timestamp
  // sorting is enabled, even if case sorting itself is disabled.
  options.sort_caseid_required = false;
  eager_rejected = false;
  try {
    (void)discover_pandas_dfg(table, options);
  } catch (const std::invalid_argument&) {
    eager_rejected = true;
  }
  assert(eager_rejected);
  options.sort_timestamp_along_case_id = false;
  const auto graph = discover_pandas_dfg(table, options);
  assert(count(graph.frequency, "A", "B") == 1);
  assert(count(graph.frequency, "B", "C") == 1);
  const auto triples = discover_pandas_frequency_triples(table, options);
  assert(triples.at({"A", "B", "C"}) == 1);
}

void partial_order_contract() {
  const auto table = graph_table();
  DfgTableOptions options;
  options.sort_caseid_required = false;
  options.sort_timestamp_along_case_id = false;
  options.start_timestamp_attribute = "start_timestamp";
  options.event_index_attribute = "event_id";
  options.keep_first_following = false;
  const auto result = discover_pandas_partial_order(table, options);
  assert(result.pairs.size() == 3);
  assert(result.pairs[0].source_index_value == "10");
  assert(result.pairs[0].target_index_value == "11");
  assert(result.pairs[0].flow_time_seconds == 1.0);
  assert(result.pairs[0].source.attributes.size() == table.columns.size());

  options.keep_first_following = true;
  bool eager_index_bug = false;
  try {
    (void)discover_pandas_partial_order(table, options);
  } catch (const std::out_of_range&) {
    eager_index_bug = true;
  }
  assert(eager_index_bug);
  // The lazy adapter honors the caller's explicit index for first-following.
  const auto first = discover_polars_partial_order(
      LazyEventTable::from_table(table), options);
  assert(first.pairs.size() == 2);
  assert(first.pairs[0].source_index_value == "10");
  assert(first.pairs[0].target_index_value == "11");

  options.event_index_attribute = "missing_index";
  const auto generated = discover_polars_partial_order(
      LazyEventTable::from_table(table), options);
  assert(!generated.pairs.empty());
  assert(generated.pairs.front().source_index_value == "0");

  const auto signed_fractional_index = make_table(
      {"case:concept:name", "concept:name", "start_timestamp",
       "time:timestamp", "event_id"},
      {row({value("case:concept:name", "c"), value("concept:name", "A"),
            value("start_timestamp", "2026-01-05T09:00:00Z"),
            value("time:timestamp", "2026-01-05T10:00:00Z"),
            integer_value("event_id", -2)}),
       row({value("case:concept:name", "c"), value("concept:name", "B"),
            value("start_timestamp", "2026-01-05T10:00:00Z"),
            value("time:timestamp", "2026-01-05T11:00:00Z"),
            float_value("event_id", 1.5)})});
  options.event_index_attribute = "event_id";
  options.keep_first_following = false;
  const auto numeric = discover_polars_partial_order(
      LazyEventTable::from_table(signed_fractional_index), options);
  assert(numeric.pairs.size() == 1);
  assert(numeric.pairs.front().source_index == -2.0);
  assert(numeric.pairs.front().target_index == 1.5);
  assert(numeric.pairs.front().source_index_value == "-2");
  assert(numeric.pairs.front().target_index_value == "1.500000");

  const auto high = std::numeric_limits<std::uint64_t>::max();
  const auto adjacent_high_indices = make_table(
      {"case:concept:name", "concept:name", "start_timestamp",
       "time:timestamp", "@@index"},
      {row({value("case:concept:name", "c"), value("concept:name", "A"),
            value("start_timestamp", "2026-01-05T09:00:02Z"),
            value("time:timestamp", "2026-01-05T09:00:03Z"),
            uint64_value("@@index", high)}),
       row({value("case:concept:name", "c"), value("concept:name", "B"),
            value("start_timestamp", "2026-01-05T09:00:00Z"),
            value("time:timestamp", "2026-01-05T09:00:01Z"),
            uint64_value("@@index", high - 1)})});
  options.event_index_attribute = "@@index";
  options.keep_first_following = false;
  options.sort_caseid_required = false;
  options.sort_timestamp_along_case_id = false;
  const auto adjacent_high = discover_polars_partial_order(
      LazyEventTable::from_table(adjacent_high_indices), options);
  assert(adjacent_high.pairs.size() == 1);
  assert(adjacent_high.pairs.front().source.activity == "B");
  assert(adjacent_high.pairs.front().target.activity == "A");
  assert(adjacent_high.pairs.front().source_index_value ==
         std::to_string(high - 1));
  assert(adjacent_high.pairs.front().target_index_value ==
         std::to_string(high));

  const auto unrelated_null = make_table(
      {"case:concept:name", "concept:name", "start_timestamp",
       "time:timestamp", "event_id", "optional_note"},
      {row({value("case:concept:name", "c"), value("concept:name", "A"),
            value("start_timestamp", "2026-01-05T09:00:00Z"),
            value("time:timestamp", "2026-01-05T10:00:00Z"),
            integer_value("event_id", 1), null_value("optional_note")}),
       row({value("case:concept:name", "c"), value("concept:name", "B"),
            value("start_timestamp", "2026-01-05T10:00:00Z"),
            value("time:timestamp", "2026-01-05T11:00:00Z"),
            integer_value("event_id", 2), value("optional_note", "kept")})});
  options.reduce_columns = false;
  const auto with_null = discover_pandas_partial_order(unrelated_null, options);
  assert(with_null.pairs.size() == 1);
  assert(with_null.pairs.front().source.activity == "A");
  assert(with_null.pairs.front().target.activity == "B");
}

void concurrency_contracts() {
  const auto table = make_table(
      {"case:concept:name", "concept:name", "start_timestamp",
       "time:timestamp"},
      {row({value("case:concept:name", "c"), value("concept:name", "A"),
            value("start_timestamp", "2026-01-05T09:00:00Z"),
            value("time:timestamp", "2026-01-05T10:00:00Z")}),
       row({value("case:concept:name", "c"), value("concept:name", "B"),
            value("start_timestamp", "2026-01-05T10:00:00Z"),
            value("time:timestamp", "2026-01-05T11:00:00Z")})});
  DfgTableOptions options;
  // Omitting the start selector overwrites the conventional interval starts
  // with completion instants in the eager adapter, so these intervals no
  // longer touch.
  assert(discover_pandas_concurrent_events(table, options).pairs.empty());
  options.start_timestamp_attribute = "start_timestamp";
  const auto eager = discover_pandas_concurrent_events(table, options);
  assert(eager.pairs.size() == 1);
  assert(eager.pairs.front().overlap_seconds == 0.0);
  options.strict = true;
  assert(discover_pandas_concurrent_events(table, options).pairs.empty());

  options.strict = false;
  const auto lazy = discover_polars_concurrent_events(
      LazyEventTable::from_table(table), options);
  assert(lazy.pairs.size() == 1);
  assert(lazy.pairs.front().overlap_seconds == 0.0);
  options.strict = true;
  assert(discover_polars_concurrent_events(LazyEventTable::from_table(table),
                                           options)
             .pairs.empty());

  const auto overlapping = make_table(
      {"case:concept:name", "concept:name", "start_timestamp",
       "time:timestamp"},
      {row({value("case:concept:name", "c"), value("concept:name", "A"),
            value("start_timestamp", "2026-01-05T09:00:00Z"),
            value("time:timestamp", "2026-01-05T11:00:00Z")}),
       row({value("case:concept:name", "c"), value("concept:name", "B"),
            value("start_timestamp", "2026-01-05T10:00:00Z"),
            value("time:timestamp", "2026-01-05T12:00:00Z")})});
  options.strict = true;
  const auto eager_overlap = discover_pandas_concurrent_events(
      overlapping, options);
  assert(eager_overlap.pairs.size() == 1);
  assert(eager_overlap.pairs.front().overlap_seconds == 3600.0);
  assert(discover_polars_concurrent_events(
             LazyEventTable::from_table(overlapping), options)
             .pairs.empty());

  const auto null_activity = make_table(
      {"case:concept:name", "concept:name", "start_timestamp",
       "time:timestamp"},
      {row({value("case:concept:name", "c"), null_value("concept:name"),
            value("start_timestamp", "2026-01-05T09:00:00Z"),
            value("time:timestamp", "2026-01-05T11:00:00Z")}),
       row({value("case:concept:name", "c"), value("concept:name", "B"),
            value("start_timestamp", "2026-01-05T10:00:00Z"),
            value("time:timestamp", "2026-01-05T12:00:00Z")})});
  // A row without an activity never pairs; with one row left, pandas' empty
  // self-join compares a timedelta column with 0 and raises (round BM,
  // observed black-box).
  bool raised = false;
  try {
    (void)discover_pandas_concurrent_events(null_activity, options);
  } catch (const std::invalid_argument& error) {
    raised = std::string(error.what()) == "Invalid comparison between dtype=timedelta64[us] and int";
  }
  assert(raised);

  const auto reverse_index = make_table(
      {"case:concept:name", "concept:name", "start_timestamp",
       "time:timestamp", "@@index"},
      {row({value("case:concept:name", "c"), value("concept:name", "A"),
            value("start_timestamp", "2026-01-05T10:00:00Z"),
            value("time:timestamp", "2026-01-05T11:00:00Z"),
            value("@@index", "2")}),
       row({value("case:concept:name", "c"), value("concept:name", "B"),
            value("start_timestamp", "2026-01-05T09:00:00Z"),
            value("time:timestamp", "2026-01-05T10:00:00Z"),
            value("@@index", "1")})});
  options.strict = false;
  options.sort_caseid_required = false;
  options.sort_timestamp_along_case_id = false;
  const auto lazy_reverse = discover_polars_concurrent_events(
      LazyEventTable::from_table(reverse_index), options);
  assert(lazy_reverse.pairs.size() == 1);
  assert(lazy_reverse.pairs.front().left.activity == "B");
  assert(lazy_reverse.pairs.front().right.activity == "A");
  assert(lazy_reverse.pairs.front().left_index_value == "1");
  assert(lazy_reverse.pairs.front().right_index_value == "2");

  const auto duplicate_index = make_table(
      {"case:concept:name", "concept:name", "start_timestamp",
       "time:timestamp", "@@index"},
      {row({value("case:concept:name", "c"), value("concept:name", "A"),
            value("start_timestamp", "2026-01-05T09:00:00Z"),
            value("time:timestamp", "2026-01-05T10:00:00Z"),
            integer_value("@@index", 1)}),
       row({value("case:concept:name", "c"), value("concept:name", "B"),
            value("start_timestamp", "2026-01-05T10:00:00Z"),
            value("time:timestamp", "2026-01-05T11:00:00Z"),
            integer_value("@@index", 1)})});
  assert(discover_polars_concurrent_events(
             LazyEventTable::from_table(duplicate_index), options)
             .pairs.empty());

  auto null_index = duplicate_index;
  null_index.rows[1][4] = null_value("@@index");
  assert(discover_polars_concurrent_events(
             LazyEventTable::from_table(null_index), options)
             .pairs.empty());

  const auto signed_fractional_index = make_table(
      {"case:concept:name", "concept:name", "start_timestamp",
       "time:timestamp", "@@index"},
      {row({value("case:concept:name", "c"), value("concept:name", "A"),
            value("start_timestamp", "2026-01-05T10:00:00Z"),
            value("time:timestamp", "2026-01-05T11:00:00Z"),
            float_value("@@index", 1.5)}),
       row({value("case:concept:name", "c"), value("concept:name", "B"),
            value("start_timestamp", "2026-01-05T09:00:00Z"),
            value("time:timestamp", "2026-01-05T10:00:00Z"),
            integer_value("@@index", -2)})});
  const auto signed_fractional = discover_polars_concurrent_events(
      LazyEventTable::from_table(signed_fractional_index), options);
  assert(signed_fractional.pairs.size() == 1);
  assert(signed_fractional.pairs.front().left.activity == "B");
  assert(signed_fractional.pairs.front().right.activity == "A");
  assert(signed_fractional.pairs.front().left_index == -2.0);
  assert(signed_fractional.pairs.front().right_index == 1.5);
  assert(signed_fractional.pairs.front().left_index_value == "-2");
  assert(signed_fractional.pairs.front().right_index_value == "1.500000");
}

void first_class_selector_contract() {
  const auto table = make_table(
      {"case:concept:name", "concept:name", "org:resource",
       "lifecycle:transition", "time:timestamp"},
      {row({value("case:concept:name", "c"), value("concept:name", "A"),
            value("org:resource", "r1"), value("lifecycle:transition", "start"),
            value("time:timestamp", "2026-01-05T00:00:00Z")}),
       row({value("case:concept:name", "c"), value("concept:name", "B"),
            value("org:resource", "r2"), value("lifecycle:transition", "complete"),
            value("time:timestamp", "2026-01-05T00:00:01Z")})});
  DfgTableOptions options;
  options.activity_attribute = "org:resource";
  options.target_activity_attribute = "lifecycle:transition";
  const auto result = discover_pandas_dfg(table, options);
  assert(count(result.frequency, "r1", "complete") == 1);
}

void event_log_projection_selector_contract() {
  Event first;
  first.case_id = "c";
  first.activity = "raw-a";
  first.timestamp = "2026-01-05T00:00:00Z";
  first.resource = "r1";
  first.lifecycle = "start";
  first.activity_source_key = "label";
  first.resource_source_key = "worker";
  first.lifecycle_source_key = "phase";
  Event second = first;
  second.activity = "raw-b";
  second.timestamp = "2026-01-05T00:00:01Z";
  second.resource = "r2";
  second.lifecycle = "complete";
  assert(*dfg_event_field_value(first, "concept:name") == "raw-a");
  assert(*dfg_event_field_value(first, "worker") == "r1");
  assert(*dfg_event_field_value(first, "org:resource") == "r1");
  assert(*dfg_event_field_value(first, "phase") == "start");
  assert(*dfg_event_field_value(first, "case:concept:name") == "c");
  assert(*dfg_event_field_value(first, "time:timestamp") ==
         "2026-01-05T00:00:00Z");
  EventLog source;
  Trace trace;
  trace.case_id = "c";
  trace.events = {first, second};
  source.traces.push_back(std::move(trace));
  DfgStatisticalOptions options;
  options.activity_attribute = "org:resource";
  const auto graph = discover_frequency_dfg(source, options);
  assert(graph.at({"r1", "r2"}) == 1);

  // Event-log performance defaults to completion-to-completion even when a
  // generic interval-start attribute happens to be present.
  Event first_timed = first;
  first_timed.timestamp = "2026-01-05T08:00:00Z";
  first_timed.attributes.push_back(
      cell("start_timestamp", "2026-01-05T07:00:00Z"));
  Event second_timed = second;
  second_timed.timestamp = "2026-01-05T10:00:00Z";
  second_timed.attributes.push_back(
      cell("start_timestamp", "2026-01-05T09:00:00Z"));
  Trace timed_trace;
  timed_trace.case_id = "c";
  timed_trace.events = {first_timed, second_timed};
  DfgStatisticalOptions performance_options;
  performance_options.activity_attribute = "concept:name";
  const auto completion_default = discover_performance_dfg(
      EventLog{{std::move(timed_trace)}}, performance_options);
  assert(completion_default.at({"raw-a", "raw-b"}).summary.mean == 7200.0);

  performance_options.use_business_hours = true;
  performance_options.business_hour_slots = {
      {9.0 * 3600.0, 9.0 * 3600.0 + 1800.0},
      {9.0 * 3600.0 + 1801.0, 10.0 * 3600.0}};
  EventLog timed_log;
  Trace timed_trace_copy;
  timed_trace_copy.case_id = "c";
  timed_trace_copy.events = {first_timed, second_timed};
  timed_log.traces.push_back(timed_trace_copy);
  const auto merged_schedule = discover_performance_dfg(
      timed_log, performance_options);
  assert(merged_schedule.at({"raw-a", "raw-b"}).summary.mean == 3600.0);
  performance_options.business_hour_slots.clear();
  performance_options.business_hour_slots_explicit = true;
  performance_options.workcalendar = std::string("ignored");
  const auto empty_schedule = discover_performance_dfg(
      timed_log, performance_options);
  assert(empty_schedule.at({"raw-a", "raw-b"}).summary.mean == 0.0);
}

}  // namespace

// Polars null handling and aggregation keys (fresh public comparison).
void polars_null_and_aggregation_contract() {
  const auto event = [](const char* activity, const char* time) {
    return row({value("case:concept:name", "c"),
                activity ? value("concept:name", activity) : null_value("concept:name"),
                time ? value("time:timestamp", time) : null_value("time:timestamp")});
  };
  // [null, a, b]: frequency drops the null source, performance keeps it.
  auto table = make_table({"case:concept:name", "concept:name", "time:timestamp"},
                          {event(nullptr, "2020-01-01T00:00:00Z"),
                           event("a", "2020-01-01T00:01:00Z"),
                           event("b", "2020-01-01T00:02:00Z")});
  DfgTableOptions options;
  options.measure = DfgTableMeasure::kBoth;
  auto lazy = discover_polars_dfg(LazyEventTable::from_table(table), options);
  assert(lazy.frequency.size() == 1 && count(lazy.frequency, "a", "b") == 1);
  assert(lazy.performance.size() == 1 && lazy.null_source_performance.size() == 1);
  assert(std::get<double>(lazy.null_source_performance.at("a").value) == 60.0);
  auto triples = discover_polars_frequency_triple_table(LazyEventTable::from_table(table), options);
  assert(triples.triples.empty() && triples.null_first.at({"a", "b"}) == 1);
  const auto eager = discover_pandas_dfg(table, options);
  assert(eager.performance.size() == 1 && eager.null_source_performance.empty());

  // A null timestamp sorts first and leaves its edge with a null duration.
  table = make_table({"case:concept:name", "concept:name", "time:timestamp"},
                     {event("a", "2020-01-01T00:00:00Z"), event("b", nullptr),
                      event("c", "2020-01-02T00:00:00Z")});
  lazy = discover_polars_dfg(LazyEventTable::from_table(table), options);
  assert(count(lazy.frequency, "b", "a") == 1 && count(lazy.frequency, "a", "c") == 1);
  assert(std::isnan(std::get<double>(lazy.performance.at({"b", "a"}).value)));
  assert(std::get<double>(lazy.performance.at({"a", "c"}).value) == 86400.0);
  // A Polars sum over only null durations is 0, not null.
  options.performance_aggregation = DfgPerformanceAggregation::kSum;
  lazy = discover_polars_dfg(LazyEventTable::from_table(table), options);
  assert(std::get<double>(lazy.performance.at({"b", "a"}).value) == 0.0);

  // The lazy adapter has no stdev or raw_values aggregation.
  for (const auto aggregation : {DfgPerformanceAggregation::kStandardDeviation,
                                 DfgPerformanceAggregation::kRawValues}) {
    options.performance_aggregation = aggregation;
    bool rejected = false;
    try { (void)discover_polars_dfg(LazyEventTable::from_table(table), options); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    // pandas has raw_values but no SeriesGroupBy.stdev, as in the reference.
    bool pandas_rejected = false;
    try { (void)discover_pandas_dfg(table, options); }
    catch (const std::invalid_argument&) { pandas_rejected = true; }
    assert(pandas_rejected == (aggregation == DfgPerformanceAggregation::kStandardDeviation));
  }
  options.measure = DfgTableMeasure::kFrequency;
  (void)discover_polars_dfg(LazyEventTable::from_table(table), options);
}

// pandas get_dfg_graph(measure="performance", perf_aggregation_key="all")
// in pm4py 2.7.22: Timedelta gaps, Kahan sum, sum / count mean, Welford
// deviation, NaN deviation for one observation, UTC business clock.
void pandas_performance_arithmetic_contract() {
  std::vector<EventTable::Row> rows;
  for (int i = 0; i < 10; ++i) {
    const auto id = "c" + std::to_string(i);
    rows.push_back(row({value("case:concept:name", id), value("concept:name", "A"),
                        value("time:timestamp", "2026-01-01T00:00:00+00:00")}));
    rows.push_back(row({value("case:concept:name", id), value("concept:name", "B"),
                        value("time:timestamp", i == 0 ? "2026-01-12T13:46:40.100000+00:00"
                                                       : "2026-01-01T00:00:00.100000+00:00")}));
  }
  rows.push_back(row({value("case:concept:name", "m"), value("concept:name", "A"),
                      value("time:timestamp", "2026-01-01T00:00:00.000001+00:00")}));
  rows.push_back(row({value("case:concept:name", "m"), value("concept:name", "C"),
                      value("time:timestamp", "2026-01-01T00:00:00.000002+00:00")}));
  const auto table = make_table({"case:concept:name", "concept:name", "time:timestamp"}, rows);
  DfgTableOptions options;
  options.measure = DfgTableMeasure::kPerformance;
  options.performance_aggregation = DfgPerformanceAggregation::kAll;
  const auto result = discover_pandas_dfg(table, options);
  const auto& gaps = result.performance.at({"A", "B"}).summary;
  assert(gaps.mean == 100000.1 && gaps.sum == 1000001.0);
  assert(gaps.median == 0.09999999999999999 && gaps.maximum == 1000000.1);
  assert(gaps.standard_deviation == 316227.76601683797);
  const auto& micro = result.performance.at({"A", "C"}).summary;
  assert(micro.mean == 1e-06 && std::isnan(micro.standard_deviation));

  const auto offset = make_table(
      {"case:concept:name", "concept:name", "time:timestamp"},
      {row({value("case:concept:name", "c"), value("concept:name", "A"),
            value("time:timestamp", "2024-01-01T06:00:00+05:00")}),
       row({value("case:concept:name", "c"), value("concept:name", "B"),
            value("time:timestamp", "2024-01-01T09:00:00+05:00")})});
  options.business_hours = true;
  assert(discover_pandas_dfg(offset, options).performance.at({"A", "B"}).summary.mean == 0.0);
}

// Regression: event_from_row once read the selected activity and timestamp
// cells through a pointer into a destroyed temporary row (heap-use-after-free
// under ASan). Heap-allocated (non-SSO) cell strings make the read observable.
void selected_cells_outlive_row_lookup() {
  const std::string a(64, 'a'), b(64, 'b');
  const auto table = make_table(
      {"case:concept:name", "concept:name", "time:timestamp"},
      {row({value("case:concept:name", "case-with-a-long-identifier-0001"),
            value("concept:name", a), value("time:timestamp", "2026-01-01T00:00:00Z")}),
       row({value("case:concept:name", "case-with-a-long-identifier-0001"),
            value("concept:name", b), value("time:timestamp", "2026-01-02T00:00:00Z")})});
  DfgTableOptions options;
  const auto result = discover_pandas_dfg(table, options);
  assert(result.frequency.size() == 1);
  assert(count(result.frequency, a, b) == 1);
  options.measure = DfgTableMeasure::kPerformance;
  const auto performance = discover_pandas_dfg(table, options);
  assert(performance.performance.size() == 1);
}

int main() {
  graph_measure_selectors_and_ordering();
  dataframe_triples_contract();
  graph_performance_cost_and_aggregation();
  negative_duration_and_business_slots();
  invalid_selectors_are_rejected();
  timestamp_free_frequency_contracts();
  partial_order_contract();
  concurrency_contracts();
  first_class_selector_contract();
  event_log_projection_selector_contract();
  polars_null_and_aggregation_contract();
  pandas_performance_arithmetic_contract();
  selected_cells_outlive_row_lookup();
  return 0;
}
