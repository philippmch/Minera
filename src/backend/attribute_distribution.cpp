#include "minera/attribute_distribution.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "minera/dataframe_columns.hpp"
#include "minera/timestamp.hpp"
#include "frame_cells.hpp"
#include "reference_activity_selector.hpp"

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

// The selected timestamp: the key (default "time:timestamp") names a literal
// attribute or the projection read from exactly that field (J1), never
// another field. The empty string is a field name like any other.
const std::string* timestamp_value(const Event& event, std::string_view key) {
  return reference_selected_field(event, key);
}

// Civil date of a day count since 1970-01-01 (proleptic Gregorian).
void civil_from_days(std::int64_t z, int& year, int& month, int& day) {
  z += 719468;
  const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const auto doe = static_cast<std::uint64_t>(z - era * 146097);
  const auto yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const std::int64_t y = static_cast<std::int64_t>(yoe) + era * 400;
  const auto doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const auto mp = (5 * doy + 2) / 153;
  day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  year = static_cast<int>(y + (month <= 2 ? 1 : 0));
}

// Calendar parts of an exact instant at its own offset, from integer seconds
// alone: no epoch double, so a nanosecond before midnight stays in its day.
UtcParts exact_parts(const ParsedExactTimestamp& exact) {
  const std::int64_t wall = exact.seconds + (exact.timezone_aware ? std::int64_t(exact.offset_minutes) * 60 : 0);
  std::int64_t days = wall / 86400;
  std::int64_t rest = wall % 86400;
  if (rest < 0) { rest += 86400; --days; }
  UtcParts parts;
  civil_from_days(days, parts.year, parts.month, parts.day);
  parts.hour = static_cast<int>(rest / 3600);
  parts.weekday = static_cast<int>(((days + 3) % 7 + 7) % 7);  // 1970-01-01 was a Thursday
  constexpr int before_month[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  const bool leap = parts.year % 4 == 0 && (parts.year % 100 != 0 || parts.year % 400 == 0);
  parts.day_of_year = before_month[parts.month - 1] + (parts.month > 2 && leap ? 1 : 0) + parts.day - 1;
  return parts;
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

// pm4py's DataFrame statistics (what view_events_distribution_graph calls
// on a frame) take the column's .dt part: a column that is no datetime
// column has no .dt accessor; a NaT cell makes the part a float column (its
// labels '3.0'), while ISO weeks stay integers; the present values are
// counted, labelled str(v).zfill(2) (weekdays by name, keyed by str(v)),
// the fixed range gains the missing integers, years span min to max (a
// float range raises), and the labels sort as text (round BD, observed
// black-box and modelled against pm4py on 2,400 requests).
AttributeDistributionResult frame_event_distribution(const EventLog& input, const AttributeDistributionOptions& options) {
  const auto frame = dataframe_frame_event_log(input);
  const auto& key = options.timestamp_key;
  const auto type = frame_column_type(frame, key);
  if (!type) throw std::invalid_argument("KeyError: '" + key + "'");
  if (type != DataFrameColumnType::kDate)
    throw std::invalid_argument("AttributeError: Can only use .dt accessor with datetimelike values");
  AttributeDistributionResult result;
  result.options = options;
  const bool weeks = options.distribution == EventDistributionType::kWeeks;
  const bool weekdays = options.distribution == EventDistributionType::kDaysWeek;
  bool nat = false;
  std::map<int, std::uint64_t> counts;
  for (const auto& trace : frame.traces)
    for (const auto& event : trace.events) {
      const auto cell = frame_row_scalar(frame, trace, event, key);
      if (!cell || cell->kind != python::Scalar::Kind::kDate) {
        nat = true;
        continue;
      }
      // A frame's datetime column is UTC (pm4py's read_xes keeps an XES
      // date's wall clock as UTC; the frame cell carries that instant).
      auto instant = cell->date;
      instant.timezone_aware = false;
      instant.offset_minutes = 0;
      const auto parts = exact_parts(instant);
      ++result.timed_events;
      int bucket = 0;
      switch (options.distribution) {
        case EventDistributionType::kDaysMonth: bucket = parts.day; break;
        case EventDistributionType::kMonths: bucket = parts.month; break;
        case EventDistributionType::kYears: bucket = parts.year; break;
        case EventDistributionType::kHours: bucket = parts.hour; break;
        case EventDistributionType::kDaysWeek: bucket = parts.weekday; break;
        case EventDistributionType::kWeeks: bucket = iso_week(parts); break;
      }
      ++counts[bucket];
    }
  const bool floats = nat && !weeks;
  int first = 0, last = 0;
  switch (options.distribution) {
    case EventDistributionType::kDaysMonth: first = 1; last = 31; break;
    case EventDistributionType::kMonths: first = 1; last = 12; break;
    case EventDistributionType::kHours: first = 0; last = 23; break;
    case EventDistributionType::kDaysWeek: first = 0; last = 6; break;
    case EventDistributionType::kWeeks: first = 0; last = 52; break;
    case EventDistributionType::kYears:
      if (counts.empty()) throw std::invalid_argument("ValueError: min() arg is an empty sequence");
      if (floats) throw std::invalid_argument("TypeError: 'float' object cannot be interpreted as an integer");
      first = counts.begin()->first;
      last = counts.rbegin()->first;
      break;
  }
  const auto zfill = [](std::string text) { return text.size() < 2 ? std::string(2 - text.size(), '0') + text : text; };
  std::map<std::string, std::uint64_t> labelled;  // sorted as text
  for (const auto& [bucket, count] : counts) {
    const auto text = floats ? std::to_string(bucket) + ".0" : std::to_string(bucket);
    labelled[weekdays ? text : zfill(text)] = count;
  }
  for (int i = first; i <= last; ++i)
    if (!counts.contains(i)) labelled[weekdays ? std::to_string(i) : zfill(std::to_string(i))] = 0;
  static constexpr const char* kWeekdays[] = {"Monday", "Tuesday", "Wednesday", "Thursday",
                                              "Friday", "Saturday", "Sunday"};
  for (const auto& [label, count] : labelled) {
    if (weekdays) {
      if (label.size() != 1 || label[0] < '0' || label[0] > '6') throw std::invalid_argument("KeyError: '" + label + "'");
      result.labels.emplace_back(kWeekdays[label[0] - '0']);
    } else {
      result.labels.push_back(label);
    }
    result.counts.push_back(count);
  }
  return result;
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
  if (log.dataframe_columns) return frame_event_distribution(log, options);
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
      const auto parts = exact ? exact_parts(*exact) : utc_parts(*parsed);
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
    else
      label = padded(bucket);  // years too: year 7 is "07"
    result.labels.push_back(std::move(label));
    result.counts.push_back(count);
  }
  return result;
}

}  // namespace minera
