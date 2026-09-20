#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "minera/temporal_profile.hpp"
#include "minera/timestamp.hpp"

namespace minera::detail {

inline constexpr double kBusinessDaySeconds = 86400.0;
inline constexpr double kBusinessWeekSeconds = 7.0 * kBusinessDaySeconds;

inline std::optional<double> local_wall_timestamp(std::string_view text) {
  if (!parse_timestamp(text)) return std::nullopt;
  std::size_t end = text.size();
  while (end > 0 &&
         std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  const auto timezone = text.substr(0, end).find_first_of("Zz+-", 10);
  if (timezone != std::string_view::npos) end = timezone;
  return parse_timestamp(text.substr(0, end));
}

inline std::vector<BusinessHourSlot> normalize_business_hour_slots(
    const std::vector<BusinessHourSlot>& configured, bool explicit_schedule,
    std::string_view error_prefix) {
  auto slots = configured;
  if (slots.empty() && !explicit_schedule) {
    slots.reserve(5);
    for (int day = 0; day < 5; ++day) {
      const double start = day * kBusinessDaySeconds + 7.0 * 3600.0;
      slots.push_back({start, start + 10.0 * 3600.0});
    }
  }
  for (const auto& slot : slots) {
    if (!std::isfinite(slot.start_seconds) ||
        !std::isfinite(slot.end_seconds) || slot.start_seconds < 0.0 ||
        slot.start_seconds >= slot.end_seconds ||
        slot.end_seconds > kBusinessWeekSeconds) {
      throw std::invalid_argument(
          std::string(error_prefix) +
          ": business-hour slots must satisfy 0 <= start < end <= 604800");
    }
  }
  std::sort(slots.begin(), slots.end(), [](const auto& lhs, const auto& rhs) {
    return std::tie(lhs.start_seconds, lhs.end_seconds) <
           std::tie(rhs.start_seconds, rhs.end_seconds);
  });
  std::vector<BusinessHourSlot> merged;
  for (const auto& slot : slots) {
    // Slots with gaps of at most one second are merged, including fractional
    // gaps, to match the event-log and dataframe DFG variants.
    if (!merged.empty() &&
        slot.start_seconds <= merged.back().end_seconds + 1.0) {
      merged.back().end_seconds =
          std::max(merged.back().end_seconds, slot.end_seconds);
    } else {
      merged.push_back(slot);
    }
  }
  return merged;
}

inline double active_business_segment(
    double start, double end, const std::vector<BusinessHourSlot>& slots) {
  double total = 0.0;
  for (const auto& slot : slots) {
    const double overlap_start = std::max(start, slot.start_seconds);
    const double overlap_end = std::min(end, slot.end_seconds);
    if (overlap_end > overlap_start) total += overlap_end - overlap_start;
  }
  return total;
}

inline double business_elapsed_seconds(
    double start, double end, const std::vector<BusinessHourSlot>& slots) {
  if (end <= start || slots.empty()) return 0.0;
  // 1970-01-01 was Thursday; shift the epoch so weeks begin Monday.
  constexpr double epoch_to_monday = 3.0 * kBusinessDaySeconds;
  const double adjusted_start = start + epoch_to_monday;
  const double adjusted_end = end + epoch_to_monday;
  const auto start_week = static_cast<long long>(
      std::floor(adjusted_start / kBusinessWeekSeconds));
  const auto end_week = static_cast<long long>(
      std::floor(adjusted_end / kBusinessWeekSeconds));
  const double start_offset =
      adjusted_start - static_cast<double>(start_week) * kBusinessWeekSeconds;
  const double end_offset =
      adjusted_end - static_cast<double>(end_week) * kBusinessWeekSeconds;
  if (start_week == end_week)
    return active_business_segment(start_offset, end_offset, slots);
  double weekly_total = 0.0;
  for (const auto& slot : slots)
    weekly_total += slot.end_seconds - slot.start_seconds;
  double total = active_business_segment(start_offset, kBusinessWeekSeconds,
                                         slots) +
                 active_business_segment(0.0, end_offset, slots);
  const auto full_weeks = end_week - start_week - 1;
  if (full_weeks > 0)
    total += static_cast<double>(full_weeks) * weekly_total;
  return total;
}

}  // namespace minera::detail
