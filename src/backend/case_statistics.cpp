#include "minera/case_statistics.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>

#include "business_hours.hpp"
#include "frame_cells.hpp"
#include "frame_times.hpp"
#include "performance_arithmetic.hpp"
#include "minera/frame_keys.hpp"
#include "minera/timestamp.hpp"

#include <limits>
#include <map>
#include <type_traits>

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

// pm4py's BusinessHours on microsecond wall clocks
// (detail::business_elapsed_seconds_microseconds); an epoch double holds its
// microsecond exactly enough for rounding to recover it (review AY1).
double business_elapsed(double start, double end,
                        const std::vector<BusinessHourSlot>& slots) {
  return detail::business_elapsed_seconds_microseconds(std::llround(start * 1e6),
                                                       std::llround(end * 1e6), slots);
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

// The column an upload's case statistics group by: the native default
// ("case_id", or no key) is the frame's case column; any other key names a
// frame column as pm4py's case_id_key does (concept:name is the activity).
std::string frame_case_column(std::string_view key) {
  if (key.empty() || key == "case_id") return "case:concept:name";
  return std::string(key);
}

// One group's sort values as the reference's frame holds them: the first and
// last dates as int64 // 10**9 (microseconds, or nanoseconds for a CSV
// column that has a sub-microsecond cell; NaT's integer for a group without
// a date) and the duration (NaN without dates).
struct FrameSortValues {
  std::int64_t start = 0;
  std::int64_t end = 0;
  double duration = 0.0;
};

std::int64_t floor_div(std::int64_t value, std::int64_t divisor) {
  std::int64_t q = value / divisor;
  if ((value % divisor != 0) && ((value < 0) != (divisor < 0))) --q;
  return q;
}

// The case rows on an upload's frame, as pm4py's DataFrame
// get_cases_description reads them (observed black-box): the case and
// timestamp columns are selected (pandas' KeyError), one column in both
// roles fails the groupby (`Grouper for '<key>' not 1-dimensional`), the
// rows are grouped by the case column (a missing key drops the row; values
// equal in Python are one group, named by the first met; groups in sorted
// key order), each group runs from its first to its last present cell in
// frame order (NaT cells skipped; a group of NaT cells only has NaN times),
// and a column of no dates fails as pandas does: bools and text in the
// subtraction, any other dtype at the .dt accessor; with business hours each
// group's first, then last cell is first made naive (.replace(tzinfo=None)),
// which only a date has. The times are seconds since the epoch.
void frame_cases(const EventLog& log, const CaseStatisticsOptions& options,
                 const std::vector<BusinessHourSlot>& slots, CaseStatisticsResult& result,
                 std::vector<FrameSortValues>& sort_values) {
  const frame_times::Frame frame(log);
  const auto key = frame_times::timestamp_column(options.timestamp_key);
  const auto case_key = frame_case_column(options.case_id_key);
  require_frame_columns(frame.keys(), {case_key, key});
  if (case_key == key) throw std::invalid_argument("ValueError: Grouper for '" + key + "' not 1-dimensional");
  // The groupby (python::groupby: pandas' sorted, null-dropping groupby).
  struct Group {
    python::Scalar key;
    std::optional<python::Scalar> first, last;
  };
  std::vector<std::vector<python::Scalar>> keys;
  std::vector<std::optional<python::Scalar>> cells;
  bool nanosecond_unit = false;
  // In the frame's row order (an XES upload's file order: a case column
  // other than the traces' names groups rows of several traces; round BG).
  for (const auto& row : frame_rows(frame.log())) {
    auto id = frame.cell(row.trace, row.event, case_key);
    keys.push_back({id ? std::move(*id) : python::none()});
    auto value = frame.cell(row.trace, row.event, key);
    if (value && value->kind == python::Scalar::Kind::kDate && value->date.nanoseconds % 1000 != 0)
      nanosecond_unit = true;
    cells.push_back(std::move(value));
  }
  std::vector<python::Group> grouped;
  try {
    grouped = python::groupby(keys);
  } catch (const python::TypeError& error) {
    throw std::invalid_argument(std::string("TypeError: ") + error.what());
  }
  std::vector<Group> groups;
  for (auto& entry : grouped) {
    Group group{entry.labels.front(), std::nullopt, std::nullopt};
    for (const auto row : entry.rows)
      if (cells[row]) {
        if (!group.first) group.first = cells[row];
        group.last = cells[row];
      }
    groups.push_back(std::move(group));
  }
  std::vector<std::size_t> order(groups.size());
  for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
  const auto dtype = frame.dtype(key);
  const bool csv_nanoseconds = nanosecond_unit && frame.log().dataframe_columns->csv_text;
  // With no group (every case cell missing) the business hours' empty apply
  // cannot fill the duration column, and the plain subtraction of empty
  // columns only fails for booleans before the .dt accessor (observed).
  if (groups.empty() && !slots.empty())
    throw std::invalid_argument(
        "ValueError: Cannot set a DataFrame with multiple columns to the single column caseDuration");
  if (!slots.empty()) {
    // Each group's first, then last cell made naive, group by group; a
    // numpy column's cells are numpy scalars there.
    const auto numpy_name = [&]() -> std::string {
      if (!dtype) return {};
      switch (*dtype) {
        case DataFrameColumnType::kInt64: return "numpy.int64";
        case DataFrameColumnType::kFloat64: return "numpy.float64";
        case DataFrameColumnType::kBool: return "numpy.bool";
        default: return {};
      }
    }();
    for (const auto index : order)
      for (const auto* cell : {&groups[index].first, &groups[index].last}) {
        if (!numpy_name.empty())
          throw std::invalid_argument("AttributeError: '" + numpy_name + "' object has no attribute 'replace'");
        if (dtype == DataFrameColumnType::kDate) continue;
        if (*cell && (*cell)->kind == python::Scalar::Kind::kDate) continue;
        if (*cell && (*cell)->kind == python::Scalar::Kind::kText)
          throw std::invalid_argument("TypeError: str.replace() takes no keyword arguments");
        // A group without a present cell takes None in an object column,
        // NaN (a float) otherwise.
        const std::string missing = dtype == DataFrameColumnType::kObject ? "NoneType" : "float";
        throw std::invalid_argument("AttributeError: '" + (*cell ? python::type_name(**cell) : missing) +
                                    "' object has no attribute 'replace'");
      }
    // An object column's dates (an XES upload's datetime.datetime cells)
    // stay Python objects, which the business hours cannot take as integers;
    // the first group's last cell fails first (observed).
    if (dtype == DataFrameColumnType::kObject && !order.empty())
      throw std::invalid_argument(
          "TypeError: int() argument must be a string, a bytes-like object or a real number, not "
          "'datetime.datetime'");
  } else {
    // The groups' last cells less their first cells as pandas subtracts the
    // two columns (frame_times::column_subtraction), then the .dt accessor.
    std::vector<std::optional<python::Scalar>> lasts, firsts;
    for (const auto index : order) {
      lasts.push_back(groups[index].last);
      firsts.push_back(groups[index].first);
    }
    const auto difference = frame_times::column_subtraction(dtype, lasts, dtype, firsts);
    if (difference.error) throw std::invalid_argument("TypeError: " + *difference.error);
    if (!difference.timedelta)
      throw std::invalid_argument("AttributeError: Can only use .dt accessor with datetimelike values");
  }
  constexpr double nan = std::numeric_limits<double>::quiet_NaN();
  constexpr std::int64_t nat = std::numeric_limits<std::int64_t>::min();
  const auto ticks = [&](const std::optional<ParsedExactTimestamp>& date) -> std::int64_t {
    if (!date) return floor_div(nat, 1000000000LL);
    const auto micros = date->seconds * 1000000LL + date->nanoseconds / 1000;
    if (!csv_nanoseconds) return floor_div(micros, 1000000000LL);
    return date->seconds + floor_div(date->nanoseconds, 1000000000LL);
  };
  for (const auto index : order) {
    const auto& group = groups[index];
    const auto as_date = [&](const std::optional<python::Scalar>& cell) -> std::optional<ParsedExactTimestamp> {
      if (!cell || cell->kind != python::Scalar::Kind::kDate) return std::nullopt;
      auto date = cell->date;
      date.timezone_aware = true;
      date.offset_minutes = 0;
      return date;
    };
    const auto first = as_date(group.first);
    const auto last = as_date(group.last);
    const auto id = python::str(group.key);
    if (!first || !last) {
      // Business hours take the group's NaT endpoints as integers.
      if (!slots.empty()) throw std::invalid_argument("ValueError: cannot convert float NaN to integer");
      result.cases.push_back({id, nan, nan, nan});
      sort_values.push_back({ticks(std::nullopt), ticks(std::nullopt), nan});
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
    sort_values.push_back({ticks(first), ticks(last), duration});
  }
}

// pandas' sort_values of one column (nargsort, kind="quicksort"): the
// missing values (NaN durations) last; descending sorts the reversed values
// and reverses the result. numpy's quicksort is its generic introsort here;
// builds with SIMD sorts break ties among more values otherwise (declared).
template <typename Value>
std::vector<std::size_t> pandas_nargsort(const std::vector<Value>& values, bool ascending) {
  std::vector<std::size_t> present, missing;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if constexpr (std::is_floating_point_v<Value>) {
      if (std::isnan(values[i])) {
        missing.push_back(i);
        continue;
      }
    }
    present.push_back(i);
  }
  if (!ascending) std::reverse(present.begin(), present.end());
  const auto sorted = detail::numpy_object_quick_argsort(present.size(), [&](std::size_t a, std::size_t b) {
    const auto& x = values[present[a]];
    const auto& y = values[present[b]];
    return x < y ? -1 : (y < x ? 1 : 0);
  });
  std::vector<std::size_t> order;
  order.reserve(values.size());
  for (const auto i : sorted) order.push_back(present[i]);
  if (!ascending) std::reverse(order.begin(), order.end());
  order.insert(order.end(), missing.begin(), missing.end());
  return order;
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
  std::vector<FrameSortValues> frame_sort_values;
  if (log.dataframe_columns) {
    if (!options.sort_by_given) result.options.sort_by = CaseStatisticsSortBy::kStartTime;
    frame_cases(log, options, slots, result, frame_sort_values);
    // The reference sorts its frame of groups with pandas; case ids keep the
    // groupby's sorted order (a native extension: descending reverses it).
    if (result.options.enable_sort) {
      std::vector<std::size_t> order;
      const bool ascending = options.sort_ascending;
      switch (result.options.sort_by) {
        case CaseStatisticsSortBy::kCaseId:
          order.resize(result.cases.size());
          for (std::size_t i = 0; i < order.size(); ++i) order[i] = ascending ? i : order.size() - 1 - i;
          break;
        case CaseStatisticsSortBy::kStartTime:
        case CaseStatisticsSortBy::kEndTime: {
          std::vector<std::int64_t> values;
          for (const auto& v : frame_sort_values)
            values.push_back(result.options.sort_by == CaseStatisticsSortBy::kStartTime ? v.start : v.end);
          order = pandas_nargsort(values, ascending);
          break;
        }
        case CaseStatisticsSortBy::kDuration: {
          std::vector<double> values;
          for (const auto& v : frame_sort_values) values.push_back(v.duration);
          order = pandas_nargsort(values, ascending);
          break;
        }
      }
      std::vector<CaseStatisticsEntry> sorted;
      sorted.reserve(order.size());
      for (const auto i : order) sorted.push_back(std::move(result.cases[i]));
      result.cases = std::move(sorted);
    }
  }
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
  if (options.enable_sort && !log.dataframe_columns) {
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
