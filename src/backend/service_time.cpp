#include "minera/service_time.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>

#include "frame_times.hpp"
#include "performance_arithmetic.hpp"
#include "minera/timestamp.hpp"

#include <limits>
#include <map>

namespace minera {
namespace {

constexpr double kDaySeconds = 86400.0;
constexpr double kWeekSeconds = 7.0 * kDaySeconds;

bool builtin_activity_key(std::string_view key) {
  return key.empty() || key == "activity" || key == "concept:name";
}

bool builtin_timestamp_key(std::string_view key) {
  return key.empty() || key == "timestamp" || key == "time:timestamp";
}

std::string required_activity(const Event& event, std::string_view key) {
  if (builtin_activity_key(key)) return event.activity;
  if (key == "resource") return event.resource;
  if (const auto* value = event.attribute(key)) return *value;
  throw std::invalid_argument("service time: event is missing activity attribute '" +
                              std::string(key) + "'");
}

double required_timestamp(const Event& event, std::string_view key,
                          std::string_view label) {
  std::string value;
  if (builtin_timestamp_key(key)) {
    value = event.timestamp;
  } else if (const auto* attribute = event.attribute(key)) {
    value = *attribute;
  } else {
    throw std::invalid_argument("service time: event is missing " +
                                std::string(label) + " attribute '" +
                                std::string(key) + "'");
  }
  const auto parsed = parse_timestamp(value);
  if (!parsed || !std::isfinite(*parsed))
    throw std::invalid_argument("service time: invalid " + std::string(label) +
                                " timestamp");
  return *parsed;
}

std::vector<BusinessHourSlot> normalized_slots(
    const ServiceTimeOptions& options) {
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
          "service time: business-hour slots must satisfy 0 <= start < end <= 604800");
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
  while (end > 0 && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
    --end;
  const auto timezone = text.substr(0, end).find_first_of("Zz+-", 10);
  if (timezone != std::string_view::npos) end = timezone;
  return parse_timestamp(text.substr(0, end));
}

double active_segment(double start, double end,
                      const std::vector<BusinessHourSlot>& slots) {
  double total = 0.0;
  for (const auto& slot : slots) {
    const double lo = std::max(start, slot.start_seconds);
    const double hi = std::min(end, slot.end_seconds);
    if (hi > lo) total += hi - lo;
  }
  return total;
}

double business_elapsed(double start, double end,
                        const std::vector<BusinessHourSlot>& slots) {
  if (end <= start || slots.empty()) return 0.0;
  constexpr double epoch_to_monday = 3.0 * kDaySeconds;
  const double adjusted_start = start + epoch_to_monday;
  const double adjusted_end = end + epoch_to_monday;
  const auto start_week = static_cast<long long>(
      std::floor(adjusted_start / kWeekSeconds));
  const auto end_week =
      static_cast<long long>(std::floor(adjusted_end / kWeekSeconds));
  const double start_offset =
      adjusted_start - static_cast<double>(start_week) * kWeekSeconds;
  const double end_offset =
      adjusted_end - static_cast<double>(end_week) * kWeekSeconds;
  if (start_week == end_week)
    return active_segment(start_offset, end_offset, slots);
  double weekly_total = 0.0;
  for (const auto& slot : slots)
    weekly_total += slot.end_seconds - slot.start_seconds;
  double total = active_segment(start_offset, kWeekSeconds, slots) +
                 active_segment(0.0, end_offset, slots);
  const auto full_weeks = end_week - start_week - 1;
  if (full_weeks > 0)
    total += static_cast<double>(full_weeks) * weekly_total;
  return total;
}

bool scalar_aggregation(DfgPerformanceAggregation aggregation) {
  return aggregation == DfgPerformanceAggregation::kMean ||
         aggregation == DfgPerformanceAggregation::kMedian ||
         aggregation == DfgPerformanceAggregation::kMinimum ||
         aggregation == DfgPerformanceAggregation::kMaximum ||
         aggregation == DfgPerformanceAggregation::kSum;
}

// pandas' groupby sum and mean: a Kahan-compensated sum in row order.
double kahan_sum(const std::vector<double>& values) {
  double sum = 0.0, compensation = 0.0;
  for (const double value : values) {
    const double y = value - compensation;
    const double t = sum + y;
    compensation = t - sum - y;
    sum = t;
  }
  return sum;
}

double aggregate(std::vector<double> values,
                 DfgPerformanceAggregation aggregation, bool frame = false) {
  if (frame) {
    // pandas skips NaN durations; a group of NaN only sums to 0 and has no
    // other aggregate.
    std::erase_if(values, [](double value) { return std::isnan(value); });
    if (values.empty())
      return aggregation == DfgPerformanceAggregation::kSum ? 0.0 : std::numeric_limits<double>::quiet_NaN();
  }
  if (values.empty()) return 0.0;
  // The DataFrame path aggregates with pandas' groupby (observed black-box).
  if (frame && aggregation == DfgPerformanceAggregation::kSum) return kahan_sum(values);
  if (frame && aggregation == DfgPerformanceAggregation::kMean)
    return kahan_sum(values) / static_cast<double>(values.size());
  if (aggregation == DfgPerformanceAggregation::kSum)
    return std::accumulate(values.begin(), values.end(), 0.0);
  if (aggregation == DfgPerformanceAggregation::kMinimum)
    return *std::min_element(values.begin(), values.end());
  if (aggregation == DfgPerformanceAggregation::kMaximum)
    return *std::max_element(values.begin(), values.end());
  std::sort(values.begin(), values.end());
  if (aggregation == DfgPerformanceAggregation::kMedian) {
    const auto middle = values.size() / 2;
    return values.size() % 2 == 0
               ? (values[middle - 1] + values[middle]) / 2.0
               : values[middle];
  }
  return std::accumulate(values.begin(), values.end(), 0.0) /
         static_cast<double>(values.size());
}

// pm4py's DataFrame service time (service_time.pandas.get.apply) on an
// upload's frame, observed black-box: the timestamp and the start column are
// read (KeyError), each row's duration is the timestamp less the start as
// pandas subtracts the two columns (its TypeError for columns that are no
// dates, the .dt accessor's AttributeError for numbers) or, with business
// hours, row by row (each start's and timestamp's replace(), then the
// business seconds; a NaT cell raises ValueError there), then the rows are
// grouped by the activity column (KeyError; a missing activity drops the
// row; values equal in Python are one key, named by the first one met). A
// NaT cell elsewhere gives the row a NaN duration, which the aggregation
// skips.
std::map<std::string, std::vector<double>> frame_service_durations(
    const EventLog& log, const ServiceTimeOptions& options,
    const std::vector<BusinessHourSlot>& slots) {
  const frame_times::Frame frame(log);
  const auto activity_key = frame_times::activity_column(options.activity_key);
  const auto timestamp_key = frame_times::timestamp_column(options.timestamp_key);
  const auto start_key = frame_times::timestamp_column(options.start_timestamp_key);
  const auto fail = [](const std::string& message) { throw std::invalid_argument(message); };
  for (const auto* key : {&timestamp_key, &start_key})
    if (!frame.has(*key)) fail("KeyError: '" + *key + "'");
  const auto end_type = frame.dtype(timestamp_key);
  const auto start_type = frame.dtype(start_key);
  const auto is = [](std::optional<DataFrameColumnType> type, DataFrameColumnType wanted) { return type == wanted; };
  const auto no_accessor = [&] { fail("AttributeError: Can only use .dt accessor with datetimelike values"); };
  const auto& traces = frame.log().traces;
  if (slots.empty()) {
    // The column difference (frame_times::column_subtraction), then its .dt.
    std::vector<std::optional<python::Scalar>> ends, starts;
    for (const auto& trace : traces)
      for (const auto& event : trace.events) {
        ends.push_back(frame.cell(trace, event, timestamp_key));
        starts.push_back(frame.cell(trace, event, start_key));
      }
    const auto difference = frame_times::column_subtraction(end_type, ends, start_type, starts);
    if (difference.error) fail("TypeError: " + *difference.error);
    if (!difference.timedelta) no_accessor();
  }
  struct Row {
    const Trace* trace;
    const Event* event;
    double duration;
  };
  std::vector<Row> rows;
  for (const auto& trace : traces)
    for (const auto& event : trace.events) {
      if (slots.empty()) {
        const auto start = frame.date(trace, event, start_key);
        const auto finish = frame.date(trace, event, timestamp_key);
        rows.push_back({&trace, &event,
                        start && finish ? frame_times::total_seconds(*start, *finish)
                                        : std::numeric_limits<double>::quiet_NaN()});
        continue;
      }
      std::optional<ParsedExactTimestamp> ends[2];
      int index = 0;
      for (const auto* key : {&start_key, &timestamp_key}) {
        const auto cell = frame.cell(trace, event, *key);
        if (cell && cell->kind == python::Scalar::Kind::kText) fail("TypeError: str.replace() takes no keyword arguments");
        if (cell ? cell->kind != python::Scalar::Kind::kDate : !is(frame.dtype(*key), DataFrameColumnType::kDate))
          fail("AttributeError: '" + frame.cell_type(trace, event, *key) + "' object has no attribute 'replace'");
        ends[index++] = frame.date(trace, event, *key);
      }
      if (!ends[0] || !ends[1]) fail("ValueError: cannot convert float NaN to integer");
      // The UTC wall clock's business seconds, the week offsets taken from
      // the exact microseconds (epoch-second doubles lose sub-microsecond
      // precision, observed on XES uploads).
      const auto micros = [](const ParsedExactTimestamp& date) {
        return date.seconds * 1000000LL + static_cast<std::int64_t>(date.nanoseconds / 1000);
      };
      rows.push_back(
          {&trace, &event, detail::business_elapsed_microseconds(micros(*ends[0]), micros(*ends[1]), slots)});
    }
  if (!frame.has(activity_key)) fail("KeyError: '" + activity_key + "'");
  std::map<std::string, std::string> names;
  std::map<std::string, std::vector<double>> durations;
  for (const auto& row : rows) {
    const auto activity = frame.cell(*row.trace, *row.event, activity_key);
    if (!activity) continue;
    const auto& name = names.emplace(python::identity(*activity), python::str(*activity)).first->second;
    durations[name].push_back(row.duration);
  }
  return durations;
}

}  // namespace

const char* service_time_aggregation_name(
    DfgPerformanceAggregation aggregation) {
  switch (aggregation) {
    case DfgPerformanceAggregation::kMean: return "MEAN";
    case DfgPerformanceAggregation::kMedian: return "MEDIAN";
    case DfgPerformanceAggregation::kMinimum: return "MIN";
    case DfgPerformanceAggregation::kMaximum: return "MAX";
    case DfgPerformanceAggregation::kSum: return "SUM";
    default: break;
  }
  throw std::invalid_argument("service time: unsupported aggregation");
}

ServiceTimeResult discover_service_time(const EventLog& log,
                                        const ServiceTimeOptions& options) {
  if (!scalar_aggregation(options.aggregation))
    throw std::invalid_argument(
        "service time: aggregation must be mean, median, min, max, or sum");
  const auto slots = options.use_business_hours
                         ? normalized_slots(options)
                         : std::vector<BusinessHourSlot>{};
  std::map<std::string, std::vector<double>> durations;
  if (log.dataframe_columns) {
    durations = frame_service_durations(log, options, slots);
  } else {
    for (const auto& trace : log.traces) {
      for (const auto& event : trace.events) {
        const auto activity = required_activity(event, options.activity_key);
        const double start =
            required_timestamp(event, options.start_timestamp_key, "start");
        const double finish =
            required_timestamp(event, options.timestamp_key, "completion");
        double duration = finish - start;
        if (!slots.empty()) {
          const auto start_value = builtin_timestamp_key(options.start_timestamp_key)
                                       ? event.timestamp
                                       : *event.attribute(options.start_timestamp_key);
          const auto finish_value = builtin_timestamp_key(options.timestamp_key)
                                        ? event.timestamp
                                        : *event.attribute(options.timestamp_key);
          const auto clock = [&](std::string_view text) {
            return options.dataframe ? parse_timestamp(text) : local_wall_timestamp(text);
          };
          const auto local_start = clock(start_value);
          const auto local_finish = clock(finish_value);
          if (!local_start || !local_finish)
            throw std::invalid_argument("service time: invalid business-hours timestamp");
          duration = business_elapsed(*local_start, *local_finish, slots);
        }
        durations[activity].push_back(duration);
      }
    }
  }
  ServiceTimeResult result;
  result.aggregation = options.aggregation;
  for (auto& [activity, values] : durations)
    result.values.emplace(activity, aggregate(std::move(values), options.aggregation,
                                              log.dataframe_columns != nullptr));
  return result;
}

}  // namespace minera
