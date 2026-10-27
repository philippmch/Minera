#include "minera/case_arrival.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>

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

double elapsed(double start, double end, std::string_view start_text,
                std::string_view end_text,
                const std::vector<BusinessHourSlot>& slots) {
  if (slots.empty()) return exact_seconds_between(start_text, end_text).value_or(end - start);
  const auto local_start = local_wall_timestamp(start_text);
  const auto local_end = local_wall_timestamp(end_text);
  if (!local_start || !local_end)
    throw std::invalid_argument("case arrival: invalid business-hours timestamp");
  return business_elapsed(*local_start, *local_end, slots);
}

double average_gaps(std::vector<double> values) {
  if (values.empty()) return 0.0;
  return std::accumulate(values.begin(), values.end(), 0.0) /
         static_cast<double>(values.size());
}

}  // namespace

CaseArrivalResult discover_case_arrival(const EventLog& log,
                                        const CaseArrivalOptions& options) {
  const auto slots = options.use_business_hours
                         ? normalized_slots(options)
                         : std::vector<BusinessHourSlot>{};
  std::vector<std::pair<double, std::string>> starts;
  std::vector<std::pair<double, std::string>> ends;
  starts.reserve(log.traces.size());
  ends.reserve(log.traces.size());
  for (const auto& trace : log.traces) {
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
                              starts[i - 1].second, starts[i].second, slots));
  for (std::size_t i = 1; i < ends.size(); ++i)
    dispersion.push_back(elapsed(ends[i - 1].first, ends[i].first,
                                 ends[i - 1].second, ends[i].second, slots));
  return {average_gaps(std::move(arrival)), average_gaps(std::move(dispersion)),
          starts.size(), ends.size()};
}

}  // namespace minera
