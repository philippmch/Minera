#include "minera/case_arrival.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "business_hours.hpp"
#include "exact_mean.hpp"
#include "frame_times.hpp"
#include "numpy_object_sort.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/timestamp.hpp"

namespace minera {
namespace {

constexpr double kDaySeconds = 86400.0;
constexpr double kWeekSeconds = 7.0 * kDaySeconds;

bool builtin_timestamp_key(std::string_view key) {
  return key.empty() || key == "timestamp" || key == "time:timestamp";
}

double required_timestamp(const Event& event, std::string_view key,
                          std::string_view label) {
  std::string value;
  if (builtin_timestamp_key(key)) {
    value = event.timestamp;
  } else if (const auto* attribute = event.attribute(key)) {
    value = *attribute;
  } else {
    throw std::invalid_argument("case arrival: event is missing " +
                                std::string(label) + " timestamp attribute '" +
                                std::string(key) + "'");
  }
  const auto parsed = parse_timestamp(value);
  if (!parsed || !std::isfinite(*parsed))
    throw std::invalid_argument("case arrival: invalid " + std::string(label) +
                                " timestamp");
  return *parsed;
}

std::string timestamp_text(const Event& event, std::string_view key,
                           std::string_view label) {
  if (builtin_timestamp_key(key)) return event.timestamp;
  if (const auto* attribute = event.attribute(key)) return *attribute;
  throw std::invalid_argument("case arrival: event is missing " +
                              std::string(label) + " timestamp attribute '" +
                              std::string(key) + "'");
}

std::vector<BusinessHourSlot> normalized_slots(
    const CaseArrivalOptions& options) {
  auto slots = options.business_hour_slots;
  if (slots.empty()) {
    for (int day = 0; day < 5; ++day) {
      const double start = day * kDaySeconds + 7.0 * 3600.0;
      slots.push_back({start, start + 10.0 * 3600.0});
    }
  }
  for (const auto& slot : slots) {
    if (!std::isfinite(slot.start_seconds) ||
        !std::isfinite(slot.end_seconds) || slot.start_seconds < 0.0 ||
        slot.start_seconds >= slot.end_seconds ||
        slot.end_seconds > kWeekSeconds) {
      throw std::invalid_argument(
          "case arrival: business-hour slots must satisfy 0 <= start < end <= 604800");
    }
  }
  std::sort(slots.begin(), slots.end(), [](const auto& left, const auto& right) {
    return std::tie(left.start_seconds, left.end_seconds) <
           std::tie(right.start_seconds, right.end_seconds);
  });
  std::vector<BusinessHourSlot> merged;
  for (const auto& slot : slots) {
    if (!merged.empty() && slot.start_seconds <= merged.back().end_seconds)
      merged.back().end_seconds =
          std::max(merged.back().end_seconds, slot.end_seconds);
    else
      merged.push_back(slot);
  }
  return merged;
}

std::optional<double> local_wall_timestamp(std::string_view text) {
  if (!parse_timestamp(text)) return std::nullopt;
  std::size_t end = text.size();
  while (end > 0 &&
         std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
    --end;
  const auto timezone = text.substr(0, end).find_first_of("Zz+-", 10);
  if (timezone != std::string_view::npos) end = timezone;
  return parse_timestamp(text.substr(0, end));
}

// pm4py's BusinessHours on microsecond wall clocks
// (detail::business_elapsed_seconds_microseconds); an epoch double holds its
// microsecond exactly enough for rounding to recover it (review AY1).
double business_elapsed(double start, double end,
                        const std::vector<BusinessHourSlot>& slots) {
  return detail::business_elapsed_seconds_microseconds(std::llround(start * 1e6),
                                                       std::llround(end * 1e6), slots);
}

// Seconds between two instants from their exact parsed values, like a
// Python timedelta's total_seconds (whole microseconds / 10**6); rounded epoch
// doubles would lose the gap, e.g. one microsecond in 2026 reads 9.5e-7.
std::optional<double> exact_seconds_between(std::string_view start_text, std::string_view end_text) {
  const auto start = parse_timestamp_exact(start_text);
  const auto end = parse_timestamp_exact(end_text);
  if (!start || !end) return std::nullopt;
  const std::int64_t seconds = end->seconds - start->seconds;
  const std::int64_t nanoseconds = static_cast<std::int64_t>(end->nanoseconds) - start->nanoseconds;
  if (seconds > 9000000000LL || seconds < -9000000000LL) return std::nullopt;
  if (nanoseconds % 1000 == 0)
    return static_cast<double>(seconds * 1000000 + nanoseconds / 1000) / 1e6;
  return static_cast<double>(seconds) + static_cast<double>(nanoseconds) / 1e9;
}

// The seconds of a gap on the reference's EventLog path: the gap floored to
// the microsecond, as an exact quotient (a nanosecond remainder is dropped).
std::optional<double> eventlog_seconds_between(std::string_view start_text, std::string_view end_text) {
  const auto start = parse_timestamp_exact(start_text);
  const auto end = parse_timestamp_exact(end_text);
  if (!start || !end) return std::nullopt;
  const std::int64_t seconds = end->seconds - start->seconds;
  if (seconds > 9000000000LL || seconds < -9000000000LL) return std::nullopt;
  const std::int64_t nanoseconds =
      seconds * 1000000000LL + (static_cast<std::int64_t>(end->nanoseconds) - start->nanoseconds);
  const std::int64_t micro = nanoseconds / 1000 - (nanoseconds % 1000 < 0 ? 1 : 0);
  return static_cast<double>(micro) / 1e6;
}

// The seconds of a gap as pandas' Timedelta.total_seconds gives them on the
// reference's DataFrame path: the whole seconds, plus the microseconds
// times 1e-6, plus the nanoseconds left times 1e-9, which can be an ulp off
// the exact quotient (232001 us reads 0.23200099999999998).
std::optional<double> dataframe_seconds_between(std::string_view start_text,
                                                std::string_view end_text) {
  const auto start = parse_timestamp_exact(start_text);
  const auto end = parse_timestamp_exact(end_text);
  if (!start || !end) return std::nullopt;
  if (end->seconds - start->seconds > 9000000000LL || end->seconds - start->seconds < -9000000000LL)
    return std::nullopt;
  const std::int64_t nanoseconds = (end->seconds - start->seconds) * 1000000000LL +
                                   (static_cast<std::int64_t>(end->nanoseconds) - start->nanoseconds);
  std::int64_t whole = nanoseconds / 1000000000LL;
  std::int64_t rest = nanoseconds % 1000000000LL;
  if (rest < 0) {
    rest += 1000000000LL;
    --whole;
  }
  return static_cast<double>(whole) + static_cast<double>(rest / 1000) * 1e-6 +
         static_cast<double>(rest % 1000) * 1e-9;
}

double elapsed(double start, double end, std::string_view start_text,
                std::string_view end_text,
                const std::vector<BusinessHourSlot>& slots, bool dataframe) {
  if (slots.empty() && dataframe)
    return dataframe_seconds_between(start_text, end_text).value_or(end - start);
  if (slots.empty()) return eventlog_seconds_between(start_text, end_text).value_or(end - start);
  const auto local_start = local_wall_timestamp(start_text);
  const auto local_end = local_wall_timestamp(end_text);
  if (!local_start || !local_end)
    throw std::invalid_argument("case arrival: invalid business-hours timestamp");
  return business_elapsed(*local_start, *local_end, slots);
}

// numpy's pairwise float64 summation of a contiguous array (np.sum and
// np.mean): eight accumulators up to 128 values, recursive halving above.
double numpy_pairwise_sum(const double* values, std::size_t n) {
  if (n < 8) {
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) sum += values[i];
    return sum;
  }
  if (n <= 128) {
    double r[8];
    for (std::size_t j = 0; j < 8; ++j) r[j] = values[j];
    std::size_t i = 8;
    for (; i < n - n % 8; i += 8)
      for (std::size_t j = 0; j < 8; ++j) r[j] += values[i + j];
    double sum = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
    for (; i < n; ++i) sum += values[i];
    return sum;
  }
  std::size_t half = n / 2;
  half -= half % 8;
  return numpy_pairwise_sum(values, half) + numpy_pairwise_sum(values + half, n - half);
}

// The EventLog path averages the gaps with statistics.mean: the exact sum
// divided by the count, rounded once (a running double sum can be an ulp
// off). The DataFrame path takes numpy's mean of the gaps in time order.
double average_gaps(std::vector<double> values, bool dataframe) {
  // pm4py's DataFrame path takes the mean of an empty gap series (one case):
  // NaN. The EventLog path returns 0.0.
  if (values.empty())
    return dataframe ? std::numeric_limits<double>::quiet_NaN() : 0.0;
  if (dataframe)
    return numpy_pairwise_sum(values.data(), values.size()) / static_cast<double>(values.size());
  return detail::exact_mean(values);
}

// Each case's first and last timestamp on an upload's frame, as pm4py's
// DataFrame case arrival and dispersion read them: the key is a column
// (KeyError otherwise), the first and last present cell of each case group
// in frame order (a NaT cell is skipped, a case of NaT cells only has no
// endpoint), and a column of no dates fails as pandas does: text in the
// gaps' subtraction, any other dtype at the .dt accessor (observed
// black-box).
void frame_case_endpoints(const EventLog& log, std::string_view option_key,
                          std::vector<std::pair<double, std::string>>& starts,
                          std::vector<std::pair<double, std::string>>& ends) {
  const frame_times::Frame frame(log);
  const auto key = frame_times::timestamp_column(option_key);
  if (!frame.has(key)) throw std::invalid_argument("KeyError: '" + key + "'");
  const auto dtype = frame.dtype(key);
  if (dtype != DataFrameColumnType::kDate) {
    // The cases' first cells are sorted (an object column with Python's
    // comparisons; an XES object column holds datetime.datetime cells),
    // then each subtracted from the next: text fails there once two cases
    // have a cell, anything else at the .dt accessor. The arrival average
    // fails first, so the dispersion's last cells are never read.
    const auto cells = frame_times::case_cells(frame, key);
    std::vector<const python::Scalar*> values;
    bool text = false;
    for (const auto& cell : cells) {
      values.push_back(&cell.first);
      text = text || cell.first.kind == python::Scalar::Kind::kText;
    }
    if (dtype == DataFrameColumnType::kObject) {
      try {
        (void)detail::numpy_object_quick_argsort(
            values.size(), [&](std::size_t a, std::size_t b) { return python::compare(*values[a], *values[b]); });
      } catch (const python::TypeError& error) {
        std::string message = error.what();
        for (std::size_t at; (at = message.find("'Timestamp'")) != std::string::npos;)
          message.replace(at, 11, "'datetime.datetime'");
        throw std::invalid_argument("TypeError: " + message);
      }
    }
    if (text && values.size() >= 2)
      throw std::invalid_argument("TypeError: unsupported operand type(s) for -: 'str' and 'str'");
    throw std::invalid_argument("AttributeError: Can only use .dt accessor with datetimelike values");
  }
  for (const auto& trace : frame.log().traces) {
    std::optional<ParsedExactTimestamp> first, last;
    for (const auto& event : trace.events)
      if (const auto date = frame.date(trace, event, key)) {
        if (!first) first = date;
        last = date;
      }
    if (!first) continue;
    starts.emplace_back(frame_times::epoch_seconds(*first), frame_times::utc_text(*first));
    ends.emplace_back(frame_times::epoch_seconds(*last), frame_times::utc_text(*last));
  }
}

// No traces: the native loop's range on the frame path.
const std::vector<Trace> kNoTraces;

}  // namespace

CaseArrivalResult discover_case_arrival(const EventLog& log,
                                        const CaseArrivalOptions& options) {
  const bool dataframe = options.dataframe || log.dataframe_columns != nullptr;
  // pm4py's pandas variant takes no business-hours parameters: on a frame
  // the gaps are plain differences whatever is asked (review AY1-4).
  const auto slots = options.use_business_hours && !dataframe
                         ? normalized_slots(options)
                         : std::vector<BusinessHourSlot>{};
  std::vector<std::pair<double, std::string>> starts;
  std::vector<std::pair<double, std::string>> ends;
  starts.reserve(log.traces.size());
  ends.reserve(log.traces.size());
  if (log.dataframe_columns) frame_case_endpoints(log, options.timestamp_key, starts, ends);
  for (const auto& trace : log.dataframe_columns ? kNoTraces : log.traces) {
    if (trace.events.empty()) continue;
    const auto& first = trace.events.front();
    const auto& last = trace.events.back();
    const double start = required_timestamp(first, options.timestamp_key, "start");
    const double end = required_timestamp(last, options.timestamp_key, "end");
    starts.emplace_back(start, timestamp_text(first, options.timestamp_key, "start"));
    ends.emplace_back(end, timestamp_text(last, options.timestamp_key, "end"));
  }
  // Order by the exact instant where both parse, so instants closer than one
  // double ulp keep their true order.
  const auto earlier = [](const std::pair<double, std::string>& a, const std::pair<double, std::string>& b) {
    if (const auto gap = exact_seconds_between(a.second, b.second); gap && *gap != 0.0) return *gap > 0.0;
    return a < b;
  };
  std::sort(starts.begin(), starts.end(), earlier);
  std::sort(ends.begin(), ends.end(), earlier);
  std::vector<double> arrival;
  std::vector<double> dispersion;
  arrival.reserve(starts.size() > 1 ? starts.size() - 1 : 0);
  dispersion.reserve(ends.size() > 1 ? ends.size() - 1 : 0);
  for (std::size_t i = 1; i < starts.size(); ++i)
    arrival.push_back(elapsed(starts[i - 1].first, starts[i].first,
                              starts[i - 1].second, starts[i].second, slots,
                              dataframe));
  for (std::size_t i = 1; i < ends.size(); ++i)
    dispersion.push_back(elapsed(ends[i - 1].first, ends[i].first,
                                 ends[i - 1].second, ends[i].second, slots,
                                 dataframe));
  return {average_gaps(std::move(arrival), dataframe),
          average_gaps(std::move(dispersion), dataframe),
          starts.size(), ends.size()};
}

}  // namespace minera
