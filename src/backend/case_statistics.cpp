#include "minera/case_statistics.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>

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
    throw std::invalid_argument("case statistics: event is missing " +
                                std::string(label) + " timestamp attribute '" +
                                std::string(key) + "'");
  }
  const auto parsed = parse_timestamp(value);
  if (!parsed || !std::isfinite(*parsed))
    throw std::invalid_argument("case statistics: invalid " + std::string(label) +
                                " timestamp");
  return *parsed;
}

std::string timestamp_text(const Event& event, std::string_view key,
                           std::string_view label) {
  if (builtin_timestamp_key(key)) return event.timestamp;
  if (const auto* attribute = event.attribute(key)) return *attribute;
  throw std::invalid_argument("case statistics: event is missing " +
                              std::string(label) + " timestamp attribute '" +
                              std::string(key) + "'");
}

std::vector<BusinessHourSlot> normalized_slots(
    const CaseStatisticsOptions& options) {
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
          "case statistics: business-hour slots must satisfy 0 <= start < end <= 604800");
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

std::string case_id(const Trace& trace, std::string_view key,
                    std::size_t index) {
  if (key.empty() || key == "case_id" || key == "concept:name" ||
      key == "case:concept:name") {
    return trace.case_id.empty() ? "EMPTY" + std::to_string(index)
                                 : trace.case_id;
  }
  if (const auto* value = trace.attribute(key)) return *value;
  return "EMPTY" + std::to_string(index);
}

}  // namespace

const char* case_statistics_sort_name(CaseStatisticsSortBy sort_by) {
  switch (sort_by) {
    case CaseStatisticsSortBy::kCaseId: return "CASE_ID";
    case CaseStatisticsSortBy::kStartTime: return "START_TIME";
    case CaseStatisticsSortBy::kEndTime: return "END_TIME";
    case CaseStatisticsSortBy::kDuration: return "DURATION";
  }
  throw std::invalid_argument("case statistics: unknown sort field");
}

CaseStatisticsResult discover_case_statistics(
    const EventLog& log, const CaseStatisticsOptions& options) {
  const auto slots = options.use_business_hours
                         ? normalized_slots(options)
                         : std::vector<BusinessHourSlot>{};
  CaseStatisticsResult result;
  result.options = options;
  result.cases.reserve(log.traces.size());
  for (std::size_t index = 0; index < log.traces.size(); ++index) {
    const auto& trace = log.traces[index];
    if (trace.events.empty()) continue;
    const auto& first = trace.events.front();
    const auto& last = trace.events.back();
    const double start = required_timestamp(first, options.timestamp_key, "start");
    const double end = required_timestamp(last, options.timestamp_key, "end");
    double duration = end - start;
    if (!slots.empty()) {
      const auto local_start = local_wall_timestamp(
          timestamp_text(first, options.timestamp_key, "start"));
      const auto local_end = local_wall_timestamp(
          timestamp_text(last, options.timestamp_key, "end"));
      if (!local_start || !local_end)
        throw std::invalid_argument(
            "case statistics: invalid business-hours timestamp");
      duration = business_elapsed(*local_start, *local_end, slots);
    }
    result.cases.push_back({case_id(trace, options.case_id_key, index), start,
                            end, duration});
  }
  if (options.enable_sort) {
    const auto sort_by = options.sort_by;
    std::stable_sort(result.cases.begin(), result.cases.end(),
                     [sort_by, ascending = options.sort_ascending](
                         const auto& left, const auto& right) {
                       const auto key_less = [&] {
                         switch (sort_by) {
                           case CaseStatisticsSortBy::kCaseId:
                             return left.case_id < right.case_id;
                           case CaseStatisticsSortBy::kStartTime:
                             return left.start_time < right.start_time;
                           case CaseStatisticsSortBy::kEndTime:
                             return left.end_time < right.end_time;
                           case CaseStatisticsSortBy::kDuration:
                             return left.case_duration < right.case_duration;
                         }
                         return false;
                       }();
                       if (ascending) return key_less;
                       const auto reverse_key_less = [&] {
                         switch (sort_by) {
                           case CaseStatisticsSortBy::kCaseId:
                             return right.case_id < left.case_id;
                           case CaseStatisticsSortBy::kStartTime:
                             return right.start_time < left.start_time;
                           case CaseStatisticsSortBy::kEndTime:
                             return right.end_time < left.end_time;
                           case CaseStatisticsSortBy::kDuration:
                             return right.case_duration < left.case_duration;
                         }
                         return false;
                       }();
                       return reverse_key_less;
                     });
  }
  if (options.max_cases != 0 && result.cases.size() > options.max_cases)
    result.cases.resize(options.max_cases);
  return result;
}

}  // namespace minera
