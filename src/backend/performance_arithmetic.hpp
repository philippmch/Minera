#pragma once

// Gap and aggregation arithmetic of the reference performance DFG
// (pm4py.discover_performance_dfg), on its two paths:
//
// - DataFrame (CSV and XES imports): a gap is pandas' Timedelta.total_seconds
//   (whole seconds + microseconds * 1e-6 + nanoseconds * 1e-9); the groupby
//   sum is Kahan-compensated, the mean is that sum over the count, the
//   standard deviation is Welford's running variance.
// - EventLog (native logs): a gap is datetime's timedelta.total_seconds (the
//   gap floored to the microsecond, divided once by 10**6); the sum is
//   Python's left-to-right sum(), the mean statistics.mean (exact, rounded
//   once), the deviation statistics.stdev (exact, rounded once).
//
// Both take the median of the sorted values (the two middle values averaged
// as (a + b) / 2). Rounding a whole epoch instant to a double first would
// lose the gap: one microsecond in 2026 reads 9.5e-7.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "exact_mean.hpp"
#include "exact_stdev.hpp"
#include "minera/temporal_profile.hpp"
#include "minera/timestamp.hpp"

namespace minera::detail {

// Exact signed nanoseconds from `start_text` to `end_text`, nullopt when
// either does not parse or the gap is beyond int64 nanoseconds.
inline std::optional<std::int64_t> exact_gap_nanoseconds(
    std::string_view start_text, std::string_view end_text) {
  const auto start = parse_timestamp_exact(start_text);
  const auto end = parse_timestamp_exact(end_text);
  if (!start || !end) return std::nullopt;
  const std::int64_t seconds = end->seconds - start->seconds;
  if (seconds > 9000000000LL || seconds < -9000000000LL) return std::nullopt;
  return seconds * 1000000000LL +
         (static_cast<std::int64_t>(end->nanoseconds) - start->nanoseconds);
}

// timedelta.total_seconds(): the gap floored to whole microseconds, / 10**6.
inline double eventlog_gap_seconds(std::int64_t nanoseconds) {
  const std::int64_t micro =
      nanoseconds / 1000 - (nanoseconds % 1000 < 0 ? 1 : 0);
  return static_cast<double>(micro) / 1e6;
}

// pandas Timedelta.total_seconds(): seconds + us * 1e-6 + ns * 1e-9, which can
// be an ulp off the exact quotient (100000 us reads 0.09999999999999999).
inline double dataframe_gap_seconds(std::int64_t nanoseconds) {
  std::int64_t whole = nanoseconds / 1000000000LL;
  std::int64_t rest = nanoseconds % 1000000000LL;
  if (rest < 0) {
    rest += 1000000000LL;
    --whole;
  }
  return static_cast<double>(whole) +
         static_cast<double>(rest / 1000) * 1e-6 +
         static_cast<double>(rest % 1000) * 1e-9;
}

inline double gap_seconds(std::int64_t nanoseconds, bool dataframe) {
  return dataframe ? dataframe_gap_seconds(nanoseconds)
                   : eventlog_gap_seconds(nanoseconds);
}

// pandas' grouped sum: Kahan summation in input order.
inline double kahan_sum(const std::vector<double>& values) {
  double sum = 0.0;
  double compensation = 0.0;
  for (const double value : values) {
    const double y = value - compensation;
    const double t = sum + y;
    compensation = (t - sum) - y;
    sum = t;
  }
  return sum;
}

// pandas' grouped std (ddof=1): Welford's running mean and squared deviations
// in input order. Fewer than two values give 0 (pandas gives NaN; declared).
inline double welford_stdev(const std::vector<double>& values) {
  if (values.size() < 2) return 0.0;
  double mean = 0.0;
  double m2 = 0.0;
  double count = 0.0;
  for (const double value : values) {
    count += 1.0;
    const double old_mean = mean;
    mean += (value - old_mean) / count;
    m2 += (value - mean) * (value - old_mean);
  }
  return std::sqrt(m2 / (count - 1.0));
}

// The traces in the order their gaps reach the aggregates. A DataFrame is
// grouped with its rows sorted by case id (a CSV import is already in that
// order; an XES file need not be), an EventLog keeps its trace order.
template <typename Log>
std::vector<const typename std::decay_t<decltype(std::declval<Log>().traces)>::value_type*>
traces_in_aggregation_order(const Log& log, bool dataframe) {
  std::vector<const typename std::decay_t<decltype(log.traces)>::value_type*> order;
  order.reserve(log.traces.size());
  for (const auto& trace : log.traces) order.push_back(&trace);
  if (dataframe)
    std::stable_sort(order.begin(), order.end(), [](const auto* lhs, const auto* rhs) {
      return lhs->case_id < rhs->case_id;
    });
  return order;
}

struct PerformanceAggregates {
  double sum = 0.0;
  double mean = 0.0;
  double median = 0.0;
  double minimum = 0.0;
  double maximum = 0.0;
  double stdev = 0.0;
};

// The reference aggregates of one edge's gaps, in the order observed.
inline PerformanceAggregates aggregate_performance(
    const std::vector<double>& values, bool dataframe) {
  PerformanceAggregates result;
  if (values.empty()) return result;
  if (dataframe) {
    result.sum = kahan_sum(values);
    result.mean = result.sum / static_cast<double>(values.size());
    result.stdev = welford_stdev(values);
  } else {
    for (const double value : values) result.sum += value;
    result.mean = exact_mean(values);
    result.stdev = exact_stdev(values);
  }
  auto ordered = values;
  std::sort(ordered.begin(), ordered.end());
  result.minimum = ordered.front();
  result.maximum = ordered.back();
  const std::size_t middle = ordered.size() / 2;
  result.median = ordered.size() % 2 == 0
                      ? (ordered[middle - 1] + ordered[middle]) / 2.0
                      : ordered[middle];
  return result;
}

// Business seconds between two wall-clock instants given in exact
// microseconds, against weekly slots (seconds from Monday 00:00, sorted and
// merged). Week positions are taken from the integers, so only the small
// within-week offsets are doubles and the epoch magnitude costs no precision.
inline double business_elapsed_microseconds(
    std::int64_t start, std::int64_t end,
    const std::vector<BusinessHourSlot>& slots) {
  if (end <= start || slots.empty()) return 0.0;
  constexpr std::int64_t kWeek = 7LL * 86400LL * 1000000LL;
  // 1970-01-01 was a Thursday; shift so that weeks begin on Monday.
  constexpr std::int64_t kEpochToMonday = 3LL * 86400LL * 1000000LL;
  const auto floor_div = [](std::int64_t value, std::int64_t divisor) {
    std::int64_t quotient = value / divisor;
    if (value % divisor < 0) --quotient;
    return quotient;
  };
  const std::int64_t adjusted_start = start + kEpochToMonday;
  const std::int64_t adjusted_end = end + kEpochToMonday;
  const std::int64_t start_week = floor_div(adjusted_start, kWeek);
  const std::int64_t end_week = floor_div(adjusted_end, kWeek);
  const double start_offset =
      static_cast<double>(adjusted_start - start_week * kWeek) / 1e6;
  const double end_offset =
      static_cast<double>(adjusted_end - end_week * kWeek) / 1e6;
  const auto segment = [&](double from, double to) {
    double total = 0.0;
    for (const auto& slot : slots) {
      const double low = std::max(from, slot.start_seconds);
      const double high = std::min(to, slot.end_seconds);
      if (high > low) total += high - low;
    }
    return total;
  };
  if (start_week == end_week) return segment(start_offset, end_offset);
  double weekly_total = 0.0;
  for (const auto& slot : slots)
    weekly_total += slot.end_seconds - slot.start_seconds;
  double total = segment(start_offset, 7.0 * 86400.0) + segment(0.0, end_offset);
  const std::int64_t full_weeks = end_week - start_week - 1;
  if (full_weeks > 0) total += static_cast<double>(full_weeks) * weekly_total;
  return total;
}

}  // namespace minera::detail
