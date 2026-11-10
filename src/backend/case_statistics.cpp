#include "minera/case_statistics.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>

#include "frame_times.hpp"
#include "performance_arithmetic.hpp"
#include "minera/frame_keys.hpp"
#include "minera/timestamp.hpp"

#include <limits>

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

// The case rows on an upload's frame, as pm4py's DataFrame
// get_cases_description reads them: the case and timestamp columns are
// selected (pandas' KeyError), each case group runs from its first to its
// last present cell in frame order (NaT cells skipped; a case of NaT cells
// only has NaN times), and a column of no dates fails as pandas does: bools
// and text in the subtraction, any other dtype at the .dt accessor
// (observed black-box). The times are seconds since the epoch.
void frame_cases(const EventLog& log, const CaseStatisticsOptions& options,
                 const std::vector<BusinessHourSlot>& slots, CaseStatisticsResult& result) {
  const frame_times::Frame frame(log);
  const auto key = frame_times::timestamp_column(options.timestamp_key);
  require_frame_columns(frame.keys(), {"case:concept:name", key});
  const auto dtype = frame.dtype(key);
  if (dtype == DataFrameColumnType::kBool)
    throw std::invalid_argument(
        "TypeError: numpy boolean subtract, the `-` operator, is not supported, use the bitwise_xor, "
        "the `^` operator, or the logical_xor function instead.");
  if (dtype == DataFrameColumnType::kString)
    throw std::invalid_argument("TypeError: unsupported operand type(s) for -: 'str' and 'str'");
  // An object column subtracts each case's first cell from its last, case by
  // case, as Python does (only an XES upload's object column holds dates:
  // datetime.datetime cells).
  // With business hours each case's first, then last cell is first made
  // naive (.replace(tzinfo=None)), which only a date has.
  if (dtype == DataFrameColumnType::kObject)
    for (const auto& cell : frame_times::case_cells(frame, key)) {
      if (!slots.empty())
        for (const auto* value : {&cell.first, &cell.last}) {
          if (value->kind == python::Scalar::Kind::kDate) continue;
          if (value->kind == python::Scalar::Kind::kText)
            throw std::invalid_argument("TypeError: str.replace() takes no keyword arguments");
          throw std::invalid_argument("AttributeError: '" + python::type_name(*value) +
                                      "' object has no attribute 'replace'");
        }
      if (const auto error = frame_times::subtraction_error(cell.last, cell.first, true))
        throw std::invalid_argument("TypeError: " + *error);
    }
  if (dtype != DataFrameColumnType::kDate)
    throw std::invalid_argument("AttributeError: Can only use .dt accessor with datetimelike values");
  constexpr double nan = std::numeric_limits<double>::quiet_NaN();
  const auto& traces = frame.log().traces;
  for (std::size_t index = 0; index < traces.size(); ++index) {
    const auto& trace = traces[index];
    if (trace.events.empty()) continue;
    std::optional<ParsedExactTimestamp> first, last;
    for (const auto& event : trace.events)
      if (const auto date = frame.date(trace, event, key)) {
        if (!first) first = date;
        last = date;
      }
    const auto id = case_id(log.traces.size() == traces.size() ? log.traces[index] : trace,
                            options.case_id_key, index);
    if (!first) {
      // Business hours take the case's NaT endpoints as integers.
      if (!slots.empty()) throw std::invalid_argument("ValueError: cannot convert float NaN to integer");
      result.cases.push_back({id, nan, nan, nan});
      continue;
    }
    double duration = frame_times::total_seconds(*first, *last);
    // Business hours of the UTC wall clock, the week offsets taken from the
    // exact microseconds.
    const auto micros = [](const ParsedExactTimestamp& date) {
      return date.seconds * 1000000LL + date.nanoseconds / 1000;
    };
    if (!slots.empty()) duration = detail::business_elapsed_microseconds(micros(*first), micros(*last), slots);
    result.cases.push_back(
        {id, frame_times::epoch_seconds(*first), frame_times::epoch_seconds(*last), duration});
  }
}

// a < b with NaN (a case without times) before every number.
bool nan_first_less(double a, double b) {
  if (std::isnan(a)) return !std::isnan(b);
  if (std::isnan(b)) return false;
  return a < b;
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
  if (log.dataframe_columns) frame_cases(log, options, slots, result);
  for (std::size_t index = 0; !log.dataframe_columns && index < log.traces.size(); ++index) {
    const auto& trace = log.traces[index];
    if (trace.events.empty()) continue;
    const auto& first = trace.events.front();
    const auto& last = trace.events.back();
    const double start = required_timestamp(first, options.timestamp_key, "start");
    const double end = required_timestamp(last, options.timestamp_key, "end");
    double duration = end - start;
    if (!slots.empty()) {
      const auto clock = [&](std::string_view text) {
        return options.dataframe ? parse_timestamp(text) : local_wall_timestamp(text);
      };
      const auto local_start =
          clock(timestamp_text(first, options.timestamp_key, "start"));
      const auto local_end =
          clock(timestamp_text(last, options.timestamp_key, "end"));
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
                             return nan_first_less(left.start_time, right.start_time);
                           case CaseStatisticsSortBy::kEndTime:
                             return nan_first_less(left.end_time, right.end_time);
                           case CaseStatisticsSortBy::kDuration:
                             return nan_first_less(left.case_duration, right.case_duration);
                         }
                         return false;
                       }();
                       if (ascending) return key_less;
                       const auto reverse_key_less = [&] {
                         switch (sort_by) {
                           case CaseStatisticsSortBy::kCaseId:
                             return right.case_id < left.case_id;
                           case CaseStatisticsSortBy::kStartTime:
                             return nan_first_less(right.start_time, left.start_time);
                           case CaseStatisticsSortBy::kEndTime:
                             return nan_first_less(right.end_time, left.end_time);
                           case CaseStatisticsSortBy::kDuration:
                             return nan_first_less(right.case_duration, left.case_duration);
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
