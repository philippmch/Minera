#include "minera/service_time.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>

#include "minera/timestamp.hpp"

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

double aggregate(std::vector<double> values,
                 DfgPerformanceAggregation aggregation) {
  if (values.empty()) return 0.0;
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
  ServiceTimeResult result;
  result.aggregation = options.aggregation;
  for (auto& [activity, values] : durations)
    result.values.emplace(activity, aggregate(std::move(values), options.aggregation));
  return result;
}

}  // namespace minera
