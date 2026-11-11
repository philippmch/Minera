#include "minera/passed_time.hpp"

#include <cctype>
#include <cmath>
#include <map>
#include <set>
#include <vector>
#include <stdexcept>
#include <string_view>

#include "business_hours.hpp"
#include "frame_times.hpp"
#include "performance_arithmetic.hpp"
#include "python_scalar.hpp"
#include "minera/dataframe_columns.hpp"
#include "minera/frame_keys.hpp"

#include <limits>
#include <numeric>
#include <optional>

namespace minera {
namespace {

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
  throw std::invalid_argument("passed time: event is missing activity attribute '" +
                              std::string(key) + "'");
}

std::string required_timestamp(const Event& event, std::string_view key,
                               std::string_view label) {
  if (builtin_timestamp_key(key)) return event.timestamp;
  if (const auto* value = event.attribute(key)) return *value;
  throw std::invalid_argument("passed time: event is missing " +
                              std::string(label) + " attribute '" +
                              std::string(key) + "'");
}

bool scalar_aggregation(DfgPerformanceAggregation aggregation) {
  return aggregation != DfgPerformanceAggregation::kRawValues &&
         aggregation != DfgPerformanceAggregation::kAll;
}

void append_entry(std::vector<PassedTimeEntry>& destination,
                  double& weighted_total, std::uint64_t& total_frequency,
                  const DfgActivityPair& pair,
                  const DfgPerformanceStatistics& performance,
                  std::uint64_t frequency, bool pre) {
  if (!std::holds_alternative<double>(performance.value)) {
    throw std::invalid_argument(
        "passed time: performance aggregation must be scalar");
  }
  const double value = std::get<double>(performance.value);
  if (!std::isfinite(value))
    throw std::invalid_argument("passed time: performance is not finite");
  destination.push_back({pre ? pair.first : pair.second, value, frequency});
  weighted_total += value * static_cast<double>(frequency);
  total_frequency += frequency;
}


// pm4py's DataFrame passed time (statistics.passed_time.pandas) on an
// upload's frame, observed black-box. Keys are frame columns ("" and
// "activity" are concept:name, "" and "timestamp" time:timestamp); without a
// start key the start is the timestamp column. The rows are sorted by the
// case, start and timestamp columns (each an ordered Categorical: values
// pandas cannot sort raise; missing cells last; ties keep frame order) and
// every two successive rows of one case are a pair. With a start column
// other than the timestamp column, the next row's start is first raised to
// the row's timestamp (a Python `>=` per pair: values Python cannot compare
// raise; a missing start takes the timestamp, a missing timestamp gives
// NaN); the pair's time is the next start less the row's timestamp, as
// Timedelta.total_seconds, or with business hours the business seconds
// between them (a NaT there fails). The pairs are grouped by both
// activities (a missing activity drops the pair), each group's times
// averaged without NaN, and the rows listed in the groupby's order.
using python::Scalar;

[[noreturn]] void fail(const std::string& message) { throw std::invalid_argument(message); }

struct FramePair {
  std::size_t previous = 0;
  std::size_t next = 0;
};

std::optional<DataFrameColumnType> column_type(const frame_times::Frame& frame, const std::string& key) {
  return frame.dtype(key);
}

// A cell as the shifted or unshifted column holds it: a missing cell is NaT
// in a date column and NaN otherwise; shifting makes an int column floats.
Scalar held(const std::optional<Scalar>& cell, std::optional<DataFrameColumnType> type, bool shifted) {
  if (!cell) {
    if (type == DataFrameColumnType::kDate) return python::none();
    return python::real(std::numeric_limits<double>::quiet_NaN());
  }
  if (shifted && type == DataFrameColumnType::kInt64 && cell->kind == Scalar::Kind::kInt)
    return python::real(std::strtod(python::str(*cell).c_str(), nullptr));
  return *cell;
}

std::string held_type(const Scalar& value, std::optional<DataFrameColumnType> type) {
  if (value.kind == Scalar::Kind::kNone && type == DataFrameColumnType::kDate) return "NaTType";
  // An object column (an XES upload's) holds its dates as datetime.datetime.
  if (value.kind == Scalar::Kind::kDate && type != DataFrameColumnType::kDate) return "datetime.datetime";
  return python::type_name(value);
}

// Python's `a >= b` on two held cells; the TypeError's message, if any.
std::optional<std::string> ge_error(const Scalar& a, std::optional<DataFrameColumnType> a_type, const Scalar& b,
                                    std::optional<DataFrameColumnType> b_type) {
  const bool a_nat = a.kind == Scalar::Kind::kNone && a_type == DataFrameColumnType::kDate;
  const bool b_nat = b.kind == Scalar::Kind::kNone && b_type == DataFrameColumnType::kDate;
  const bool a_date = a.kind == Scalar::Kind::kDate || a_nat;
  const bool b_date = b.kind == Scalar::Kind::kDate || b_nat;
  if (a_date && b_date) return std::nullopt;
  if (python::is_number(a) && python::is_number(b)) return std::nullopt;
  if (a.kind == Scalar::Kind::kText && b.kind == Scalar::Kind::kText) return std::nullopt;
  return "'>=' not supported between instances of '" + held_type(a, a_type) + "' and '" + held_type(b, b_type) + "'";
}

// A text pd.to_datetime reads as NaT ("NaT", "nan" or empty, any case).
bool nat_text(const std::string& text) {
  std::string lower;
  for (const char c : text) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return lower.empty() || lower == "nat" || lower == "nan";
}

// A text pandas does not take for a date at all (observed): one of the
// letters a, m, p, t in either case, or a finite number below 1000 that does
// not start with 0.
bool unlikely_date(const std::string& text) {
  if (text.empty() || text.front() == '0') return false;
  if (text.size() == 1 && std::string_view("aAmMpPtT").find(text.front()) != std::string_view::npos) return true;
  const auto begin = text.find_first_not_of(' ');
  if (begin == std::string::npos) return false;
  const auto end = text.find_last_not_of(' ');
  const std::string number = text.substr(begin, end - begin + 1);
  if (number.find_first_not_of("0123456789+-.eE") != std::string::npos) return false;
  char* stop = nullptr;
  const double value = std::strtod(number.c_str(), &stop);
  return stop == number.c_str() + number.size() && std::isfinite(value) && value < 1000.0;
}

// The format pandas infers from an ISO-shaped date text,
// YYYY-MM-DD[( |T)HH:MM[:SS[.digits]]][Z|+HH:MM|+HHMM] (observed); nullopt
// for any other shape.
std::optional<std::string> inferred_date_format(const std::string& text) {
  std::size_t at = 0;
  const auto digits = [&](std::size_t count) {
    for (std::size_t i = 0; i < count; ++i)
      if (at + i >= text.size() || !std::isdigit(static_cast<unsigned char>(text[at + i]))) return false;
    at += count;
    return true;
  };
  const auto literal = [&](char c) {
    if (at < text.size() && text[at] == c) {
      ++at;
      return true;
    }
    return false;
  };
  if (!digits(4) || !literal('-') || !digits(2) || !literal('-') || !digits(2)) return std::nullopt;
  std::string format = "%Y-%m-%d";
  if (at == text.size()) return format;
  const char separator = text[at];
  if (separator != ' ' && separator != 'T') return std::nullopt;
  ++at;
  if (!digits(2) || !literal(':') || !digits(2)) return std::nullopt;
  format += separator;
  format += "%H:%M";
  if (literal(':')) {
    if (!digits(2)) return std::nullopt;
    format += ":%S";
    if (literal('.')) {
      if (!digits(1)) return std::nullopt;
      while (at < text.size() && std::isdigit(static_cast<unsigned char>(text[at]))) ++at;
      format += ".%f";
    }
  }
  if (at == text.size()) return format;
  if (literal('Z')) return at == text.size() ? std::optional(format + "%z") : std::nullopt;
  if (!literal('+') && !literal('-')) return std::nullopt;
  if (!digits(2)) return std::nullopt;
  literal(':');
  if (!digits(2) || at != text.size()) return std::nullopt;
  return format + "%z";
}

// The ns integer pandas makes a number into a Timestamp from.
std::optional<ParsedExactTimestamp> number_instant(const Scalar& value) {
  double ns = 0.0;
  if (value.kind == Scalar::Kind::kInt) ns = std::strtod(python::str(value).c_str(), nullptr);
  else if (value.kind == Scalar::Kind::kFloat) ns = value.real;
  else return std::nullopt;
  if (!std::isfinite(ns)) return std::nullopt;
  const auto whole = static_cast<std::int64_t>(ns);
  ParsedExactTimestamp date;
  date.seconds = whole / 1000000000LL;
  std::int64_t rest = whole % 1000000000LL;
  if (rest < 0) {
    rest += 1000000000LL;
    --date.seconds;
  }
  date.nanoseconds = static_cast<std::uint32_t>(rest);
  date.timezone_aware = true;
  return date;
}

std::int64_t micros_of(const ParsedExactTimestamp& date) {
  return date.seconds * 1000000LL + date.nanoseconds / 1000;
}

PassedTimeResult frame_passed_time(const EventLog& log, const PassedTimeOptions& options) {
  const frame_times::Frame frame(log);
  const std::string case_key = "case:concept:name";
  const auto activity_key = frame_times::activity_column(options.activity_key);
  const auto timestamp_key = frame_times::timestamp_column(options.timestamp_key);
  const bool start_given = !options.start_timestamp_key.empty();
  const auto start_key = start_given ? frame_times::timestamp_column(options.start_timestamp_key) : timestamp_key;
  const bool raised_start = start_given && start_key != timestamp_key;
  if (!start_given) {
    if (!frame.has(timestamp_key)) fail("KeyError: '" + timestamp_key + "'");
    require_frame_columns(frame.keys(), {case_key, activity_key, timestamp_key});
  } else {
    require_frame_columns(frame.keys(), {case_key, start_key, activity_key, timestamp_key});
  }
  const bool business = options.use_business_hours;
  const auto slots = business ? detail::normalize_business_hour_slots(options.business_hour_slots,
                                                                      !options.business_hour_slots.empty(),
                                                                      "passed time")
                              : std::vector<BusinessHourSlot>{};
  struct Row {
    std::optional<Scalar> case_id, activity, timestamp, start;
  };
  std::vector<Row> rows;
  for (const auto& trace : frame.log().traces)
    for (const auto& event : trace.events)
      rows.push_back({frame.cell(trace, event, case_key), frame.cell(trace, event, activity_key),
                      frame.cell(trace, event, timestamp_key), frame.cell(trace, event, start_key)});
  // The sort by case, start and timestamp, each an ordered Categorical.
  const auto codes = [&](auto member) {
    std::vector<std::vector<Scalar>> keys;
    std::vector<std::size_t> keyed;
    for (std::size_t i = 0; i < rows.size(); ++i)
      if (const auto& cell = rows[i].*member) {
        keys.push_back({*cell});
        keyed.push_back(i);
      }
    std::vector<python::Group> groups;
    try {
      groups = python::groupby(keys);
    } catch (const python::TypeError&) {
      fail("TypeError: 'values' is not ordered, please explicitly specify the categories order by passing in a "
           "categories argument.");
    }
    std::vector<std::size_t> code(rows.size(), groups.size());
    for (std::size_t g = 0; g < groups.size(); ++g)
      for (const auto member_row : groups[g].rows) code[keyed[member_row]] = g;
    return code;
  };
  const auto case_codes = codes(&Row::case_id);
  const auto start_codes = codes(&Row::start);
  const auto time_codes = codes(&Row::timestamp);
  std::vector<std::size_t> order(rows.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::stable_sort(order.begin(), order.end(), [&](std::size_t x, std::size_t y) {
    return std::tie(case_codes[x], start_codes[x], time_codes[x]) <
           std::tie(case_codes[y], start_codes[y], time_codes[y]);
  });
  std::vector<FramePair> pairs;
  for (std::size_t k = 1; k < order.size(); ++k) {
    const auto& previous = rows[order[k - 1]];
    const auto& next = rows[order[k]];
    if (previous.case_id && next.case_id && python::identity(*previous.case_id) == python::identity(*next.case_id))
      pairs.push_back({order[k - 1], order[k]});
  }
  const auto timestamp_type = column_type(frame, timestamp_key);
  const auto start_type = column_type(frame, start_key);
  constexpr double nan = std::numeric_limits<double>::quiet_NaN();
  std::vector<double> times(pairs.size(), nan);
  // The next start raised to the row's timestamp: a Python comparison per
  // pair, then its value (a missing start takes the timestamp).
  std::vector<std::optional<Scalar>> starts(pairs.size());
  for (std::size_t i = 0; i < pairs.size(); ++i) {
    const auto& previous = rows[pairs[i].previous];
    const auto& next = rows[pairs[i].next];
    if (!raised_start) {
      starts[i] = next.timestamp;
      continue;
    }
    const auto a = held(next.start, start_type, true);
    const auto b = held(previous.timestamp, timestamp_type, false);
    if (const auto error = ge_error(a, start_type, b, timestamp_type)) fail("TypeError: " + *error);
    // A missing start takes the timestamp, except in a float64 column, and a
    // missing timestamp leaves the start missing unless the start column
    // holds dates (observed with business hours, where the cells are read).
    const bool dated = start_type == DataFrameColumnType::kDate;
    if ((!next.start && start_type == DataFrameColumnType::kFloat64) || (!previous.timestamp && !dated))
      starts[i].reset();
    else if (!next.start) starts[i] = previous.timestamp;
    else if (!previous.timestamp) starts[i] = next.start;
    else starts[i] = python::compare(*next.start, *previous.timestamp) >= 0 ? next.start : previous.timestamp;
  }
  if (raised_start && pairs.empty()) {
    // Without a successive pair (observed): business hours fail on a date or
    // boolean timestamp column and give the empty result otherwise.
    if (business) {
      if (timestamp_type == DataFrameColumnType::kBool)
        fail("TypeError: dtype bool cannot be converted to datetime64[ns]");
      if (timestamp_type == DataFrameColumnType::kDate)
        fail("AttributeError: Can only use .dt accessor with datetimelike values");
    } else {
      if (timestamp_type != DataFrameColumnType::kDate)
        fail("AttributeError: Can only use .dt accessor with datetimelike values");
      fail("TypeError: cannot subtract DatetimeArray from ndarray[object]");
    }
  }
  if (!business) {
    if (raised_start && timestamp_type == DataFrameColumnType::kDate &&
        std::none_of(pairs.begin(), pairs.end(), [&](const FramePair& pair) { return rows[pair.previous].timestamp.has_value(); }))
      fail("TypeError: Cannot subtract tz-naive and tz-aware datetime-like objects.");
    if (timestamp_type != DataFrameColumnType::kDate) {
      // The pairs' starts less their timestamps as pandas subtracts two
      // columns (frame_times::column_subtraction): the starts are the
      // shifted start column (int64 cells become float64, bool ones
      // object), raised in place. A text or object column subtracts the
      // pairs with both cells present cell by cell, which leaves no
      // timedelta for the .dt accessor, except a date column less an object
      // column of one pair (round BA: an XES column of dates whose last row
      // holds the text NaT), which is the whole array's subtraction.
      std::vector<std::optional<Scalar>> ends, begins;
      for (std::size_t i = 0; i < pairs.size(); ++i) {
        ends.push_back(starts[i]);
        begins.push_back(rows[pairs[i].previous].timestamp);
      }
      auto shifted_type = raised_start ? start_type : timestamp_type;
      if (shifted_type == DataFrameColumnType::kInt64) shifted_type = DataFrameColumnType::kFloat64;
      if (shifted_type == DataFrameColumnType::kBool) shifted_type = DataFrameColumnType::kObject;
      const auto difference = frame_times::column_subtraction(shifted_type, ends, timestamp_type, begins);
      if (difference.error) fail("TypeError: " + *difference.error);
      if (!difference.timedelta)
        fail("AttributeError: Can only use .dt accessor with datetimelike values");
    }
    for (std::size_t i = 0; i < pairs.size(); ++i) {
      const auto& previous = rows[pairs[i].previous];
      if (!previous.timestamp || !starts[i]) continue;
      const auto end = starts[i]->date;
      const auto begin = previous.timestamp->date;
      times[i] = frame_times::total_seconds(begin, end);
    }
  } else {
    // Business hours: both columns are made dates, the timestamp column
    // first, then each pair's business seconds from its exact microseconds.
    const auto convertible = [&](std::optional<DataFrameColumnType> type, bool shifted,
                                 const std::vector<const std::optional<Scalar>*>& cells) {
      if (type == DataFrameColumnType::kDate) return;
      if (type == DataFrameColumnType::kBool && !shifted)
        fail("TypeError: dtype bool cannot be converted to datetime64[ns]");
      // pd.to_datetime's text: NaT spellings are missing; when the first
      // other text parses, its inferred format binds the rest (a text that
      // does not parse fails against it; one of another date shape is
      // accepted here, declared); otherwise each text parses on its own.
      bool first = true, inferred = false;
      std::optional<std::string> format;
      for (const auto* cell : cells) {
        if (!*cell) continue;
        if ((*cell)->kind == Scalar::Kind::kBool) fail("TypeError: <class 'bool'> is not convertible to datetime");
        if ((*cell)->kind != Scalar::Kind::kText || nat_text((*cell)->text)) continue;
        const auto& text = (*cell)->text;
        const bool parsed = parse_timestamp_exact(text).has_value();
        if (first) {
          first = false;
          inferred = parsed;
          if (parsed) format = inferred_date_format(text);
        }
        if (parsed) continue;
        if (inferred && format)
          fail("ValueError: time data \"" + text + "\" doesn't match format \"" + *format +
               "\". You might want to try:\n    - passing `format` if your strings have a consistent format;\n    - "
               "passing `format='ISO8601'` if your strings are all ISO8601 but not necessarily in exactly the same "
               "format;\n    - passing `format='mixed'`, and the format will be inferred for each element "
               "individually. You might want to use `dayfirst` alongside this.");
        if (unlikely_date(text)) fail("ValueError: Given date string \"" + text + "\" not likely a datetime");
        fail("DateParseError: Unknown datetime string format, unable to parse: " + text);
      }
    };
    std::vector<const std::optional<Scalar>*> ends, begins;
    for (std::size_t i = 0; i < pairs.size(); ++i) {
      begins.push_back(&rows[pairs[i].previous].timestamp);
      ends.push_back(&starts[i]);
    }
    convertible(timestamp_type, false, begins);
    convertible(raised_start ? start_type : timestamp_type, true, ends);
    const auto instant = [](const std::optional<Scalar>& cell) -> std::optional<ParsedExactTimestamp> {
      if (!cell) return std::nullopt;
      if (cell->kind == Scalar::Kind::kDate) return cell->date;
      if (cell->kind == Scalar::Kind::kText) return parse_timestamp_exact(cell->text);
      return number_instant(*cell);
    };
    for (std::size_t i = 0; i < pairs.size(); ++i) {
      const auto begin = instant(rows[pairs[i].previous].timestamp);
      const auto end = instant(starts[i]);
      if (!begin || !end) fail("ValueError: cannot convert float NaN to integer");
      times[i] = detail::business_elapsed_microseconds(micros_of(*begin), micros_of(*end), slots);
    }
  }
  // The groupby of both activities.
  std::vector<std::vector<Scalar>> keys;
  for (const auto& pair : pairs) {
    const auto& a = rows[pair.previous].activity;
    const auto& b = rows[pair.next].activity;
    keys.push_back({a ? *a : python::none(), b ? *b : python::none()});
  }
  std::vector<python::Group> groups;
  try {
    groups = python::groupby(keys);
  } catch (const python::TypeError& error) {
    fail(std::string("TypeError: ") + error.what());
  }
  PassedTimeResult result;
  result.variant = options.variant;
  result.activity = options.activity;
  const auto wanted = [&](const Scalar& value) {
    return value.kind == Scalar::Kind::kText && value.text == options.activity;
  };
  const auto value_of = [&](const python::Group& group) {
    std::vector<double> values;
    for (const auto row : group.rows)
      if (!std::isnan(times[row])) values.push_back(times[row]);
    if (values.empty())
      return options.performance_aggregation == DfgPerformanceAggregation::kSum ? 0.0 : nan;
    const auto aggregates = detail::aggregate_performance(values, true);
    switch (options.performance_aggregation) {
      case DfgPerformanceAggregation::kMedian: return aggregates.median;
      case DfgPerformanceAggregation::kMinimum: return aggregates.minimum;
      case DfgPerformanceAggregation::kMaximum: return aggregates.maximum;
      case DfgPerformanceAggregation::kStandardDeviation: return values.size() < 2 ? nan : aggregates.stdev;
      case DfgPerformanceAggregation::kSum: return aggregates.sum;
      default: return aggregates.mean;
    }
  };
  const bool pre = options.variant != PassedTimeVariant::kPost;
  const bool post = options.variant != PassedTimeVariant::kPre;
  for (const auto& group : groups) {
    const auto frequency = static_cast<std::uint64_t>(group.rows.size());
    if (pre && wanted(group.labels[1]))
      result.pre.push_back({python::str(group.labels[0]), value_of(group), frequency});
    if (post && wanted(group.labels[0]))
      result.post.push_back({python::str(group.labels[1]), value_of(group), frequency});
  }
  // The weighted averages, summed in row order.
  const auto average = [](const std::vector<PassedTimeEntry>& entries) {
    double total = 0.0;
    std::uint64_t count = 0;
    for (const auto& entry : entries) {
      total += entry.performance * static_cast<double>(entry.frequency);
      count += entry.frequency;
    }
    return count == 0 ? 0.0 : total / static_cast<double>(count);
  };
  result.pre_avg_perf = average(result.pre);
  result.post_avg_perf = average(result.post);
  return result;
}

}  // namespace

const char* passed_time_variant_name(PassedTimeVariant variant) {
  switch (variant) {
    case PassedTimeVariant::kPre:
      return "PRE";
    case PassedTimeVariant::kPost:
      return "POST";
    case PassedTimeVariant::kPrePost:
      return "PREPOST";
  }
  throw std::invalid_argument("passed time: unknown variant");
}

PassedTimeResult discover_passed_time(const EventLog& log,
                                      const PassedTimeOptions& options) {
  if (!scalar_aggregation(options.performance_aggregation)) {
    throw std::invalid_argument(
        "passed time: aggregation must produce one scalar per edge");
  }
  if (log.dataframe_columns) return frame_passed_time(log, options);

  EventLog projected = log;
  for (auto& trace : projected.traces) {
    for (auto& event : trace.events) {
      event.activity = required_activity(event, options.activity_key);
      // The performance calculation only reads timestamps for adjacent
      // pairs. A singleton trace contributes no edge and therefore does not
      // require a timestamp, matching the underlying DFG statistic.
      if (trace.events.size() > 1) {
        event.timestamp = required_timestamp(event, options.timestamp_key,
                                             "timestamp");
      } else if (builtin_timestamp_key(options.timestamp_key)) {
        event.timestamp = event.timestamp;
      } else if (const auto* value = event.attribute(options.timestamp_key)) {
        event.timestamp = *value;
      } else {
        event.timestamp.clear();
      }
    }
  }

  DfgStatisticalOptions dfg_options;
  dfg_options.window = 1;
  dfg_options.performance_aggregation = options.performance_aggregation;
  dfg_options.use_business_hours = options.use_business_hours;
  dfg_options.business_hour_slots = options.business_hour_slots;
  dfg_options.business_hours_utc_clock = options.dataframe;
  if (!options.start_timestamp_key.empty() &&
      !builtin_timestamp_key(options.start_timestamp_key)) {
    dfg_options.start_timestamp_attribute = options.start_timestamp_key;
  } else {
    dfg_options.start_timestamp_attribute.clear();
  }

  const auto frequency = discover_frequency_dfg(projected, dfg_options);
  const auto performance = discover_performance_dfg(projected, dfg_options);

  PassedTimeResult result;
  result.variant = options.variant;
  result.activity = options.activity;
  double pre_total = 0.0;
  double post_total = 0.0;
  std::uint64_t pre_count = 0;
  std::uint64_t post_count = 0;

  // Rows follow the order in which each directly-follows pair first occurs,
  // walking the traces in log order, as the reference's rows do.
  std::vector<DfgActivityPair> order;
  {
    std::set<DfgActivityPair> seen;
    for (const auto& trace : projected.traces) {
      for (std::size_t i = 1; i < trace.events.size(); ++i) {
        DfgActivityPair pair{trace.events[i - 1].activity, trace.events[i].activity};
        if (performance.count(pair) && seen.insert(pair).second)
          order.push_back(std::move(pair));
      }
    }
    for (const auto& entry : performance)
      if (seen.insert(entry.first).second) order.push_back(entry.first);
  }

  for (const auto& pair : order) {
    const auto& statistics = performance.at(pair);
    const auto frequency_it = frequency.find(pair);
    if (frequency_it == frequency.end()) {
      throw std::invalid_argument("passed time: frequency/performance mismatch");
    }
    const auto count = frequency_it->second;
    if (options.variant == PassedTimeVariant::kPre ||
        options.variant == PassedTimeVariant::kPrePost) {
      if (pair.second == options.activity) {
        append_entry(result.pre, pre_total, pre_count, pair, statistics, count,
                     true);
      }
    }
    if (options.variant == PassedTimeVariant::kPost ||
        options.variant == PassedTimeVariant::kPrePost) {
      if (pair.first == options.activity) {
        append_entry(result.post, post_total, post_count, pair, statistics, count,
                     false);
      }
    }
  }

  if (pre_count != 0)
    result.pre_avg_perf = pre_total / static_cast<double>(pre_count);
  if (post_count != 0)
    result.post_avg_perf = post_total / static_cast<double>(post_count);
  return result;
}

}  // namespace minera
