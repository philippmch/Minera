#pragma once

#include <any>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "minera/dfg_variants.hpp"
#include "minera/event_data.hpp"

namespace minera {

// The two table adapters intentionally expose the same typed API while
// retaining the observable ordering differences of their source engines.
enum class DfgTableBackend {
  kPandas,
  kPolars,
  PANDAS = kPandas,
  POLARS = kPolars,
};

enum class DfgTableMeasure {
  kFrequency,
  kPerformance,
  kBoth,
  kCost,
  FREQUENCY = kFrequency,
  PERFORMANCE = kPerformance,
  BOTH = kBoth,
  COST = kCost,
};

using DfgTableAggregation =
    std::function<DfgPerformanceValue(const std::vector<double>&)>;

struct DfgTableOptions {
  DfgTableMeasure measure = DfgTableMeasure::kFrequency;
  std::string activity_attribute = "concept:name";
  // Empty means the source activity selector.  A non-empty selector applies
  // only to the target side of each shifted pair.
  std::string target_activity_attribute;
  std::string case_id_attribute = "case:concept:name";
  std::string timestamp_attribute = "time:timestamp";
  // A missing optional start selector uses `start_timestamp` when available,
  // and otherwise the completion timestamp.  The pandas and lazy adapters
  // differ only where the source implementation has a documented quirk.
  std::optional<std::string> start_timestamp_attribute;
  std::optional<std::string> cost_attribute;

  DfgPerformanceAggregation performance_aggregation =
      DfgPerformanceAggregation::kMean;
  // If set, this callback is applied to each edge's observation vector and
  // takes precedence over performance_aggregation. It is the typed native
  // counterpart of an arbitrary aggregation callable.
  DfgTableAggregation custom_aggregation;

  bool sort_caseid_required = true;
  bool sort_timestamp_along_case_id = true;
  std::size_t window = 1;
  bool keep_once_per_case = false;

  // Column reduction controls adapter-specific null filtering. Pair results
  // retain complete typed Event values so callers do not lose source data.
  bool reduce_columns = true;

  bool business_hours = false;
  // nullopt means the default Monday-Friday 07:00-17:00 schedule. An engaged
  // optional containing an empty vector means no business time at all.
  std::optional<std::vector<BusinessHourSlot>> business_hour_slots;
  // Accepted and retained for compatibility, but the reference calculations
  // do not consult a work calendar.
  std::any workcalendar;

  // Partial/concurrent adapter settings.
  std::string event_index_attribute = "@@index";
  std::string max_start_column = "@@max_start_column";
  std::string min_complete_column = "@@min_complete_column";
  std::string overlap_column = "@@diff_maxs_minc";
  bool strict = false;
  bool keep_first_following = true;
};

struct DfgTableResult {
  DfgTableBackend backend = DfgTableBackend::kPandas;
  DfgTableMeasure measure = DfgTableMeasure::kFrequency;
  DfgFrequency frequency;
  DfgPerformance performance;
  // Polars only: the lazy adapter keeps performance edges whose source
  // activity is null (frequency drops them). They are keyed here by target
  // activity, since an activity pair cannot hold a null.
  std::map<std::string, DfgPerformanceStatistics> null_source_performance;
};

// Polars frequency triples keep a null first activity; those triples are
// keyed by their second and third activities in `null_first`.
struct DfgTripleTable {
  DfgTripleFrequency triples;
  std::map<DfgActivityPair, std::uint64_t> null_first;
};

struct DfgPartialOrderPair {
  Event source;
  Event target;
  // Internal deterministic row indices. If a source table supplied a finite
  // numeric index, including a signed or fractional value, it is retained
  // here; non-numeric values remain available through the textual fields.
  double source_index = 0.0;
  double target_index = 0.0;
  std::string source_index_value;
  std::string target_index_value;
  double flow_time_seconds = 0.0;

  [[nodiscard]] const Event& source_event() const noexcept { return source; }
  [[nodiscard]] const Event& target_event() const noexcept { return target; }
  [[nodiscard]] double flow_time() const noexcept { return flow_time_seconds; }
};

struct DfgPartialOrderResult {
  DfgTableBackend backend = DfgTableBackend::kPandas;
  std::vector<DfgPartialOrderPair> pairs;
};

struct DfgConcurrentPair {
  Event left;
  Event right;
  double left_index = 0.0;
  double right_index = 0.0;
  std::string left_index_value;
  std::string right_index_value;
  double max_start_seconds = 0.0;
  double min_complete_seconds = 0.0;
  double overlap_seconds = 0.0;

  [[nodiscard]] const Event& left_event() const noexcept { return left; }
  [[nodiscard]] const Event& right_event() const noexcept { return right; }
  [[nodiscard]] double overlap() const noexcept { return overlap_seconds; }
};

struct DfgConcurrentResult {
  DfgTableBackend backend = DfgTableBackend::kPandas;
  std::string max_start_column = "@@max_start_column";
  std::string min_complete_column = "@@min_complete_column";
  std::string overlap_column = "@@diff_maxs_minc";
  std::vector<DfgConcurrentPair> pairs;
};

[[nodiscard]] DfgTableResult discover_pandas_dfg(
    const EventTable& table, const DfgTableOptions& options = {});
// The same on a CSV upload's frame table: `csv_frame` is the upload's log
// with its frame (dataframe_columns), and the activities are named as pandas
// names them, str() of the frame's typed cells (round BT;
// csv_frame_cell_label). A null frame keeps the cells' text.
[[nodiscard]] DfgTableResult discover_pandas_dfg(
    const EventTable& table, const DfgTableOptions& options, const EventLog* csv_frame);
[[nodiscard]] DfgTableResult discover_polars_dfg(
    const LazyEventTable& table, const DfgTableOptions& options = {});

[[nodiscard]] inline DfgTableResult discover_dataframe_dfg(
    const EventTable& table, const DfgTableOptions& options = {},
    DfgTableBackend backend = DfgTableBackend::kPandas) {
  return backend == DfgTableBackend::kPandas
             ? discover_pandas_dfg(table, options)
             : discover_polars_dfg(LazyEventTable::from_table(table), options);
}

[[nodiscard]] inline DfgTableResult discover_dataframe_dfg(
    const LazyEventTable& table, const DfgTableOptions& options = {}) {
  return discover_polars_dfg(table, options);
}

[[nodiscard]] DfgPartialOrderResult discover_pandas_partial_order(
    const EventTable& table, const DfgTableOptions& options = {});
[[nodiscard]] DfgPartialOrderResult discover_polars_partial_order(
    const LazyEventTable& table, const DfgTableOptions& options = {});

[[nodiscard]] inline DfgPartialOrderResult discover_partial_order(
    const EventTable& table, const DfgTableOptions& options = {},
    DfgTableBackend backend = DfgTableBackend::kPandas) {
  return backend == DfgTableBackend::kPandas
             ? discover_pandas_partial_order(table, options)
             : discover_polars_partial_order(LazyEventTable::from_table(table),
                                             options);
}

[[nodiscard]] inline DfgPartialOrderResult discover_partial_order(
    const LazyEventTable& table, const DfgTableOptions& options = {}) {
  return discover_polars_partial_order(table, options);
}

[[nodiscard]] DfgConcurrentResult discover_pandas_concurrent_events(
    const EventTable& table, const DfgTableOptions& options = {});
[[nodiscard]] DfgConcurrentResult discover_polars_concurrent_events(
    const LazyEventTable& table, const DfgTableOptions& options = {});

[[nodiscard]] inline DfgConcurrentResult discover_concurrent_events(
    const EventTable& table, const DfgTableOptions& options = {},
    DfgTableBackend backend = DfgTableBackend::kPandas) {
  return backend == DfgTableBackend::kPandas
             ? discover_pandas_concurrent_events(table, options)
             : discover_polars_concurrent_events(LazyEventTable::from_table(table),
                                                 options);
}

[[nodiscard]] inline DfgConcurrentResult discover_concurrent_events(
    const LazyEventTable& table, const DfgTableOptions& options = {}) {
  return discover_polars_concurrent_events(table, options);
}

// Adapter-shaped names make migration from the two source modules explicit.
[[nodiscard]] inline DfgTableResult get_dfg_graph(
    const EventTable& table, const DfgTableOptions& options = {}) {
  return discover_pandas_dfg(table, options);
}
[[nodiscard]] inline DfgTableResult get_dfg_graph(
    const LazyEventTable& table, const DfgTableOptions& options = {}) {
  return discover_polars_dfg(table, options);
}

[[nodiscard]] DfgTripleFrequency discover_pandas_frequency_triples(
    const EventTable& table, const DfgTableOptions& options = {});
[[nodiscard]] DfgTripleFrequency discover_polars_frequency_triples(
    const LazyEventTable& table, const DfgTableOptions& options = {});
[[nodiscard]] DfgTripleTable discover_polars_frequency_triple_table(
    const LazyEventTable& table, const DfgTableOptions& options = {});

[[nodiscard]] inline DfgTripleFrequency get_freq_triples(
    const EventTable& table, const DfgTableOptions& options = {}) {
  return discover_pandas_frequency_triples(table, options);
}
[[nodiscard]] inline DfgTripleFrequency get_freq_triples(
    const LazyEventTable& table, const DfgTableOptions& options = {}) {
  return discover_polars_frequency_triples(table, options);
}
[[nodiscard]] inline DfgPartialOrderResult get_partial_order_dataframe(
    const EventTable& table, const DfgTableOptions& options = {}) {
  return discover_pandas_partial_order(table, options);
}
[[nodiscard]] inline DfgPartialOrderResult get_partial_order_dataframe(
    const LazyEventTable& table, const DfgTableOptions& options = {}) {
  return discover_polars_partial_order(table, options);
}
[[nodiscard]] inline DfgConcurrentResult get_concurrent_events_dataframe(
    const EventTable& table, const DfgTableOptions& options = {}) {
  return discover_pandas_concurrent_events(table, options);
}
[[nodiscard]] inline DfgConcurrentResult get_concurrent_events_dataframe(
    const LazyEventTable& table, const DfgTableOptions& options = {}) {
  return discover_polars_concurrent_events(table, options);
}

}  // namespace minera
