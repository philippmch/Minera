#include "minera/attribute_distribution.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

struct UtcParts {
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int weekday = 0;  // Monday = 0, Sunday = 6.
  int day_of_year = 0;
};

UtcParts utc_parts(double seconds) {
  if (!std::isfinite(seconds))
    throw std::invalid_argument("attribute distribution: timestamp is not finite");
  const auto integral = static_cast<std::time_t>(std::floor(seconds));
  std::tm value{};
#if defined(_WIN32)
  if (gmtime_s(&value, &integral) != 0)
    throw std::invalid_argument("attribute distribution: timestamp is out of range");
#else
  if (gmtime_r(&integral, &value) == nullptr)
    throw std::invalid_argument("attribute distribution: timestamp is out of range");
#endif
  return {value.tm_year + 1900, value.tm_mon + 1, value.tm_mday,
          value.tm_hour, (value.tm_wday + 6) % 7, value.tm_yday};
}

const std::string* timestamp_value(const Event& event, std::string_view key) {
  if (key.empty() || key == "timestamp" || key == "time:timestamp")
    return &event.timestamp;
  return event.attribute(key);
}

std::string padded(int value) {
  std::ostringstream out;
  out << std::setw(2) << std::setfill('0') << value;
  return out.str();
}

int iso_week(const UtcParts& value) {
  const auto leap = [](int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  };
  const auto days = [&](int year) { return leap(year) ? 366 : 365; };
  const auto iso_week_count = [&](int year, int january_first) {
    const int december_31 =
        ((january_first - 1 + days(year) - 1) % 7) + 1;
    return (january_first == 4 || december_31 == 4) ? 53 : 52;
  };
  const int day_of_year = value.day_of_year + 1;
  const int iso_weekday = value.weekday + 1;
  const int january_first =
      ((iso_weekday - 1 - value.day_of_year) % 7 + 7) % 7 + 1;
  int week = (day_of_year + 10 - iso_weekday) / 7;
  const int current_week_count = iso_week_count(value.year, january_first);
  if (week < 1) {
    const int previous_year = value.year - 1;
    const int previous_january_first =
        ((january_first - 1 - (days(previous_year) % 7)) % 7 + 7) % 7 + 1;
    return iso_week_count(previous_year, previous_january_first);
  }
  if (week > current_week_count) return 1;
  return week;
}

}  // namespace

const char* event_distribution_name(EventDistributionType type) {
  switch (type) {
    case EventDistributionType::kDaysMonth: return "DAYS_MONTH";
    case EventDistributionType::kMonths: return "MONTHS";
    case EventDistributionType::kYears: return "YEARS";
    case EventDistributionType::kHours: return "HOURS";
    case EventDistributionType::kDaysWeek: return "DAYS_WEEK";
    case EventDistributionType::kWeeks: return "WEEKS";
  }
  throw std::invalid_argument("attribute distribution: unknown distribution");
}

AttributeDistributionResult discover_event_distribution(
    const EventLog& log, const AttributeDistributionOptions& options) {
  (void)event_distribution_name(options.distribution);
  AttributeDistributionResult result;
  result.options = options;
  std::map<int, std::uint64_t> bins;
  int minimum_year = 0;
  int maximum_year = 0;
  bool have_year = false;
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      const auto* text = timestamp_value(event, options.timestamp_key);
      if (text == nullptr || text->empty())
        throw std::invalid_argument(
            "attribute distribution: event is missing timestamp attribute");
      const auto parsed = parse_timestamp(*text);
      if (!parsed || !std::isfinite(*parsed))
        throw std::invalid_argument(
            "attribute distribution: invalid timestamp");
      // The datetime's own calendar: an aware value bins by its represented
      // local wall time (its offset), not by the UTC instant.
      const auto exact = parse_timestamp_exact(*text);
      const double wall = *parsed + (exact && exact->timezone_aware
                                         ? exact->offset_minutes * 60.0
                                         : 0.0);
      const auto parts = utc_parts(wall);
      ++result.timed_events;
      int bucket = 0;
      switch (options.distribution) {
        case EventDistributionType::kDaysMonth: bucket = parts.day; break;
        case EventDistributionType::kMonths: bucket = parts.month; break;
        case EventDistributionType::kYears:
          bucket = parts.year;
          if (!have_year) {
            minimum_year = maximum_year = bucket;
            have_year = true;
          } else {
            minimum_year = std::min(minimum_year, bucket);
            maximum_year = std::max(maximum_year, bucket);
          }
          break;
        case EventDistributionType::kHours: bucket = parts.hour; break;
        case EventDistributionType::kDaysWeek: bucket = parts.weekday; break;
        case EventDistributionType::kWeeks: bucket = iso_week(parts); break;
      }
      ++bins[bucket];
    }
  }

  if (options.distribution == EventDistributionType::kDaysMonth) {
    for (int i = 1; i <= 31; ++i) bins.try_emplace(i, 0);
  } else if (options.distribution == EventDistributionType::kMonths) {
    for (int i = 1; i <= 12; ++i) bins.try_emplace(i, 0);
  } else if (options.distribution == EventDistributionType::kHours) {
    for (int i = 0; i < 24; ++i) bins.try_emplace(i, 0);
  } else if (options.distribution == EventDistributionType::kDaysWeek) {
    for (int i = 0; i < 7; ++i) bins.try_emplace(i, 0);
  } else if (options.distribution == EventDistributionType::kWeeks) {
    for (int i = 0; i <= 52; ++i) bins.try_emplace(i, 0);
  } else if (options.distribution == EventDistributionType::kYears) {
    // The reference spans min..max of the observed years and so raises on a
    // log without timed events.
    if (!have_year)
      throw std::invalid_argument(
          "attribute distribution: min() iterable argument is empty");
    for (int i = minimum_year; i <= maximum_year; ++i) bins.try_emplace(i, 0);
  }

  static constexpr const char* kWeekdays[] = {
      "Monday", "Tuesday", "Wednesday", "Thursday",
      "Friday", "Saturday", "Sunday"};
  for (const auto& [bucket, count] : bins) {
    std::string label;
    if (options.distribution == EventDistributionType::kDaysWeek)
      label = kWeekdays[bucket];
    else if (options.distribution == EventDistributionType::kYears)
      label = std::to_string(bucket);
    else
      label = padded(bucket);
    result.labels.push_back(std::move(label));
    result.counts.push_back(count);
  }
  return result;
}

}  // namespace minera
