#include "minera/log_to_features.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <unordered_map>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

std::string value_for(const Event& e, const std::string& key) {
  if (key.empty()) return {};
  if (key == "activity" || key == "concept:name") return e.activity;
  if (key == "time:timestamp" || key == "timestamp") return e.timestamp;
  if (key == "org:resource" || key == "resource") return e.resource;
  if (const auto* v = e.attribute(key)) return *v;
  return {};
}

const EventAttribute* attr_for(const Event& e, const std::string& key) {
  for (const auto& a : e.attributes)
    if (a.key == key) return &a;
  return nullptr;
}
const EventAttribute* attr_for(const Trace& t, const std::string& key) {
  for (const auto& a : t.attributes)
    if (a.key == key) return &a;
  return nullptr;
}

bool event_attribute_present(const Event& event, const std::string& key) {
  if (key == "activity" || key == "concept:name") return true;
  if (key == "time:timestamp" || key == "timestamp")
    return !event.timestamp.empty();
  if (key == "org:resource" || key == "resource")
    return !event.resource.empty();
  return attr_for(event, key) != nullptr;
}

std::string trace_value_for(const Trace& trace, const std::string& key) {
  if (key == "case:concept:name" || key == "case_id" ||
      key == "concept:name")
    return trace.case_id;
  if (const auto* value = attr_for(trace, key)) return value->value;
  return {};
}

std::string event_attribute_value(const Event& event, const std::string& key) {
  return value_for(event, key);
}

std::string trace_attribute_value(const Trace& trace, const std::string& key) {
  return trace_value_for(trace, key);
}

std::string categorical_value(std::string value) {
  return value.empty() ? "UNDEFINED" : std::move(value);
}

std::vector<std::string> distinct_event_values(const EventLog& log,
                                               const std::string& key) {
  std::set<std::string> values;
  for (const auto& trace : log.traces) {
    bool present = false;
    for (const auto& event : trace.events) {
      if (event_attribute_present(event, key)) {
        values.insert(categorical_value(event_attribute_value(event, key)));
        present = true;
      }
    }
    if (!present) values.insert("UNDEFINED");
  }
  return {values.begin(), values.end()};
}

std::vector<std::string> event_attribute_values_for_trace(
    const Trace& trace, const std::string& key) {
  std::set<std::string> values;
  for (const auto& event : trace.events) {
    if (event_attribute_present(event, key))
      values.insert(categorical_value(event_attribute_value(event, key)));
  }
  if (values.empty()) values.insert("UNDEFINED");
  return {values.begin(), values.end()};
}

std::vector<std::string> distinct_trace_values(const EventLog& log,
                                               const std::string& key) {
  std::set<std::string> values;
  for (const auto& trace : log.traces)
    values.insert(categorical_value(trace_attribute_value(trace, key)));
  return {values.begin(), values.end()};
}

bool within_cardinality(const std::vector<std::string>& values,
                        const LogToFeaturesOptions& options) {
  return values.size() >= options.min_num_different_string_values &&
         values.size() <= options.max_num_different_string_values;
}

bool is_numeric_kind(AttributeKind k) {
  return k == AttributeKind::kInt || k == AttributeKind::kFloat ||
         k == AttributeKind::kBoolean;
}

double parse_number(const std::string& s, double fallback, bool* ok = nullptr) {
  if (s.empty()) {
    if (ok) *ok = false;
    return fallback;
  }
  double x = 0.0;
  try {
    std::size_t n = 0;
    x = std::stod(s, &n);
    if (n != s.size() || !std::isfinite(x)) throw std::invalid_argument("bad");
  } catch (...) {
    if (ok) *ok = false;
    return fallback;
  }
  if (ok) *ok = true;
  return x;
}

double checked_number(const std::string& s, double fallback) {
  if (s == "true" || s == "TRUE" || s == "True") return 1.0;
  if (s == "false" || s == "FALSE" || s == "False") return 0.0;
  bool ok = false;
  const double value = parse_number(s, fallback, &ok);
  if (!ok) throw std::invalid_argument("log_to_features: invalid or non-finite numeric attribute");
  return value;
}

double numeric_attribute_value(const Event& event, const std::string& key,
                               double fallback) {
  if (const auto* attribute = attr_for(event, key))
    return checked_number(attribute->value, fallback);
  const auto value = event_attribute_value(event, key);
  return value.empty() ? fallback : checked_number(value, fallback);
}

double numeric_trace_value(const Trace& trace, const std::string& key,
                           double fallback) {
  if (const auto* attribute = attr_for(trace, key))
    return checked_number(attribute->value, fallback);
  const auto value = trace_attribute_value(trace, key);
  return value.empty() ? fallback : checked_number(value, fallback);
}

void append_unique(std::vector<std::string>& out,
                   const std::vector<std::string>& in) {
  out.insert(out.end(), in.begin(), in.end());
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
}

void validate_names(const std::vector<std::string>& names, const char* label) {
  std::set<std::string> seen;
  for (const auto& n : names) {
    if (n.empty()) throw std::invalid_argument(std::string("log_to_features: ") + label + " contains empty name");
    if (!seen.insert(n).second) throw std::invalid_argument(std::string("log_to_features: duplicate ") + label);
  }
}

std::vector<std::string> categories_for_events(const EventLog& log,
                                                const std::string& key) {
  std::set<std::string> values;
  for (const auto& t : log.traces)
    for (const auto& e : t.events) values.insert(value_for(e, key));
  return {values.begin(), values.end()};
}

std::vector<std::string> categories_for_attr(const EventLog& log,
                                             const std::string& key,
                                             bool case_level) {
  std::set<std::string> values;
  for (const auto& t : log.traces) {
    if (case_level) {
      if (const auto* a = attr_for(t, key)) values.insert(a->value);
    } else {
      for (const auto& e : t.events)
        if (event_attribute_present(e, key))
          values.insert(event_attribute_value(e, key));
    }
  }
  return {values.begin(), values.end()};
}

std::vector<std::string> merged(const std::vector<std::string>& a,
                                const std::vector<std::string>& b) {
  std::vector<std::string> x = a;
  append_unique(x, b);
  return x;
}

bool is_numeric_kind(AttributeKind k);

std::vector<std::string> inferred_event_attributes(const EventLog& log,
                                                   bool numeric) {
  std::map<std::string, std::size_t> occurrences;
  const auto event_count = log.event_count();
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      std::set<std::string> seen;
      for (const auto& attribute : event.attributes) {
        if (is_numeric_kind(attribute.kind) != numeric ||
            !seen.insert(attribute.key).second)
          continue;
        ++occurrences[attribute.key];
      }
    }
  }
  std::vector<std::string> result;
  for (const auto& [key, count] : occurrences)
    if (!numeric || count == event_count) result.push_back(key);
  return result;
}

std::vector<std::string> inferred_trace_attributes(const EventLog& log,
                                                   bool numeric) {
  std::map<std::string, std::size_t> occurrences;
  const auto trace_count = log.trace_count();
  for (const auto& trace : log.traces) {
    std::set<std::string> seen;
    for (const auto& attribute : trace.attributes) {
      if (is_numeric_kind(attribute.kind) != numeric ||
          !seen.insert(attribute.key).second)
        continue;
      ++occurrences[attribute.key];
    }
  }
  std::vector<std::string> result;
  for (const auto& [key, count] : occurrences)
    if (count == trace_count) result.push_back(key);
  return result;
}

struct TimedEvent {
  const Event* event = nullptr;
  double finish = 0.0;
  double start = 0.0;
  std::size_t trace = 0;
  std::size_t index = 0;
};

double instant(const Event& e, const std::string& key, bool required) {
  const auto text = value_for(e, key);
  if (text.empty()) {
    if (required) throw std::invalid_argument("log_to_features: missing timestamp");
    return std::numeric_limits<double>::quiet_NaN();
  }
  const auto parsed = parse_timestamp(text);
  if (!parsed || !std::isfinite(*parsed))
    throw std::invalid_argument("log_to_features: invalid or non-finite timestamp");
  return *parsed;
}

enum class TemporalFrequencyUnit { kSecond, kMinute, kHour, kDay, kWeek,
                                  kMonth, kQuarter, kYear };

struct TemporalFrequency {
  TemporalFrequencyUnit unit = TemporalFrequencyUnit::kWeek;
  std::int64_t multiple = 1;
  bool label_start = false;
  int week_end_day = 0;  // std::tm convention: Sunday = 0.
};

std::string uppercase_ascii(std::string value) {
  for (auto& character : value)
    character = static_cast<char>(std::toupper(
        static_cast<unsigned char>(character)));
  return value;
}

std::int64_t floor_division(std::int64_t numerator,
                            std::int64_t denominator) {
  if (denominator <= 0) throw std::invalid_argument("log_to_features: invalid temporal frequency");
  std::int64_t quotient = numerator / denominator;
  const std::int64_t remainder = numerator % denominator;
  if (remainder < 0) --quotient;
  return quotient;
}

TemporalFrequency parse_temporal_frequency(const std::string& raw) {
  if (raw.empty())
    throw std::invalid_argument("log_to_features: temporal frequency must not be empty");
  const auto value = uppercase_ascii(raw);
  std::size_t suffix_begin = 0;
  while (suffix_begin < value.size() &&
         std::isdigit(static_cast<unsigned char>(value[suffix_begin])))
    ++suffix_begin;
  std::int64_t multiple = 1;
  if (suffix_begin != 0) {
    try {
      multiple = std::stoll(value.substr(0, suffix_begin));
    } catch (...) {
      throw std::invalid_argument("log_to_features: invalid temporal frequency");
    }
  }
  if (multiple <= 0 || suffix_begin == value.size())
    throw std::invalid_argument("log_to_features: temporal frequency multiplier must be positive");
  const auto suffix = value.substr(suffix_begin);
  TemporalFrequency result;
  result.multiple = multiple;
  if (suffix == "S" || suffix == "SEC" || suffix == "SECOND" ||
      suffix == "SECONDS") {
    result.unit = TemporalFrequencyUnit::kSecond;
  } else if (suffix == "T" || suffix == "MIN" || suffix == "MINS" ||
             suffix == "MINUTE" || suffix == "MINUTES") {
    result.unit = TemporalFrequencyUnit::kMinute;
  } else if (suffix == "H" || suffix == "HR" || suffix == "HOUR" ||
             suffix == "HOURS") {
    result.unit = TemporalFrequencyUnit::kHour;
  } else if (suffix == "D" || suffix == "DAY" || suffix == "DAYS") {
    result.unit = TemporalFrequencyUnit::kDay;
  } else if (suffix.rfind("W", 0) == 0) {
    result.unit = TemporalFrequencyUnit::kWeek;
    const auto day = suffix.substr(1);
    if (!day.empty()) {
      if (day.front() != '-')
        throw std::invalid_argument("log_to_features: invalid weekly temporal frequency");
      const auto weekday = day.substr(1);
      static const std::map<std::string, int> weekdays = {
          {"SUN", 0}, {"MON", 1}, {"TUE", 2}, {"WED", 3},
          {"THU", 4}, {"FRI", 5}, {"SAT", 6}};
      const auto found = weekdays.find(weekday);
      if (found == weekdays.end())
        throw std::invalid_argument("log_to_features: invalid weekly temporal frequency");
      result.week_end_day = found->second;
    }
  } else if (suffix == "M" || suffix == "ME" || suffix == "MONTH" ||
             suffix == "MONTHS") {
    result.unit = TemporalFrequencyUnit::kMonth;
  } else if (suffix == "MS" || suffix == "MONTHSTART") {
    result.unit = TemporalFrequencyUnit::kMonth;
    result.label_start = true;
  } else if (suffix == "Q" || suffix == "QE" || suffix == "QUARTER" ||
             suffix == "QUARTERS") {
    result.unit = TemporalFrequencyUnit::kQuarter;
  } else if (suffix == "QS" || suffix == "QUARTERSTART") {
    result.unit = TemporalFrequencyUnit::kQuarter;
    result.label_start = true;
  } else if (suffix == "Y" || suffix == "A" || suffix == "YE" ||
             suffix == "YEAR" || suffix == "YEARS") {
    result.unit = TemporalFrequencyUnit::kYear;
  } else if (suffix == "YS" || suffix == "AS" || suffix == "YEARSTART") {
    result.unit = TemporalFrequencyUnit::kYear;
    result.label_start = true;
  } else {
    throw std::invalid_argument("log_to_features: unsupported temporal frequency '" + raw + "'");
  }
  return result;
}

std::tm utc_calendar(double seconds) {
  const auto integral = static_cast<std::time_t>(std::floor(seconds));
  std::tm calendar{};
#if defined(_WIN32)
  if (gmtime_s(&calendar, &integral) != 0)
    throw std::invalid_argument("log_to_features: timestamp is outside the calendar range");
#else
  if (gmtime_r(&integral, &calendar) == nullptr)
    throw std::invalid_argument("log_to_features: timestamp is outside the calendar range");
#endif
  return calendar;
}

std::chrono::sys_days calendar_day(const std::tm& calendar) {
  using namespace std::chrono;
  return sys_days{year{calendar.tm_year + 1900} /
                  month{static_cast<unsigned>(calendar.tm_mon + 1)} /
                  day{static_cast<unsigned>(calendar.tm_mday)}};
}

std::int64_t epoch_seconds(std::chrono::sys_days day) {
  using namespace std::chrono;
  return duration_cast<seconds>(day.time_since_epoch()).count();
}

std::int64_t temporal_bucket_label(double seconds,
                                   const TemporalFrequency& frequency) {
  using namespace std::chrono;
  const auto calendar = utc_calendar(seconds);
  const auto date = calendar_day(calendar);
  switch (frequency.unit) {
    case TemporalFrequencyUnit::kSecond:
      return floor_division(static_cast<std::int64_t>(std::floor(seconds)),
                            frequency.multiple) * frequency.multiple;
    case TemporalFrequencyUnit::kMinute:
      return floor_division(static_cast<std::int64_t>(std::floor(seconds)),
                            frequency.multiple * 60) * frequency.multiple * 60;
    case TemporalFrequencyUnit::kHour:
      return floor_division(static_cast<std::int64_t>(std::floor(seconds)),
                            frequency.multiple * 3600) * frequency.multiple * 3600;
    case TemporalFrequencyUnit::kDay:
      return floor_division(epoch_seconds(date), frequency.multiple * 86400) *
             frequency.multiple * 86400;
    case TemporalFrequencyUnit::kWeek: {
      const int delta = (frequency.week_end_day - calendar.tm_wday + 7) % 7;
      const auto end_date = date + days{delta};
      const auto anchor_date =
          sys_days{year{1970} / month{1} / day{4}} +
          days{frequency.week_end_day};
      const auto weeks = floor_division(epoch_seconds(end_date) -
                                            epoch_seconds(anchor_date),
                                        7 * 86400);
      const auto bucket = floor_division(weeks, frequency.multiple);
      return epoch_seconds(anchor_date +
                           days{bucket * frequency.multiple * 7});
    }
    case TemporalFrequencyUnit::kMonth:
    case TemporalFrequencyUnit::kQuarter:
    case TemporalFrequencyUnit::kYear: {
      const std::int64_t months_per_period =
          frequency.unit == TemporalFrequencyUnit::kMonth
              ? 1
              : frequency.unit == TemporalFrequencyUnit::kQuarter ? 3 : 12;
      const std::int64_t month_index =
          static_cast<std::int64_t>(calendar.tm_year + 1900) * 12 +
          calendar.tm_mon;
      const auto period = floor_division(
          month_index, months_per_period * frequency.multiple);
      const auto first_month =
          period * months_per_period * frequency.multiple;
      const auto label_month = frequency.label_start
                                   ? first_month
                                   : first_month +
                                         months_per_period * frequency.multiple - 1;
      const auto year = floor_division(label_month, 12);
      const auto month = label_month - year * 12;
      const auto first_day = sys_days{
          std::chrono::year{static_cast<int>(year)} /
          std::chrono::month{static_cast<unsigned>(month + 1)} /
          std::chrono::day{1}};
      if (frequency.label_start) return epoch_seconds(first_day);
      const auto next_month = first_day + days{32};
      const auto next_calendar = utc_calendar(
          static_cast<double>(epoch_seconds(next_month)));
      const auto next_first = sys_days{
          std::chrono::year{next_calendar.tm_year + 1900} /
          std::chrono::month{static_cast<unsigned>(next_calendar.tm_mon + 1)} /
          std::chrono::day{1}};
      return epoch_seconds(next_first - days{1});
    }
  }
  throw std::invalid_argument("log_to_features: invalid temporal frequency");
}

}  // namespace

LogToFeaturesResult log_to_features(const EventLog& log,
                                    const LogToFeaturesOptions& options) {
  if (options.activity_key.empty() || options.timestamp_key.empty() ||
      options.resource_key.empty())
    throw std::invalid_argument("log_to_features: keys must not be empty");
  if (!std::isfinite(options.default_not_present))
    throw std::invalid_argument("log_to_features: default_not_present must be finite");
  if (options.grouper_freq.empty() &&
      (!std::isfinite(options.window_seconds) || options.window_seconds <= 0.0))
    throw std::invalid_argument("log_to_features: window_seconds must be finite and positive");
  if (!std::isfinite(options.epsilon) || options.epsilon < 0.0)
    throw std::invalid_argument("log_to_features: epsilon must be finite and non-negative");
  if (options.min_num_different_string_values > options.max_num_different_string_values)
    throw std::invalid_argument("log_to_features: string cardinality bounds are inverted");
  validate_names(options.event_string_attributes, "event_string_attributes");
  validate_names(options.event_numeric_attributes, "event_numeric_attributes");
  validate_names(options.case_string_attributes, "case_string_attributes");
  validate_names(options.case_numeric_attributes, "case_numeric_attributes");
  validate_names(options.event_attributes, "event_attributes");
  validate_names(options.case_attributes, "case_attributes");

  LogToFeaturesResult out;
  out.variant = options.variant;
  if (!options.feature_names.empty()) out.feature_names = options.feature_names;

  std::vector<std::string> event_strings = merged(options.event_string_attributes, {});
  std::vector<std::string> event_numbers = merged(options.event_numeric_attributes, {});
  std::vector<std::string> case_strings = merged(options.case_string_attributes, {});
  std::vector<std::string> case_numbers = merged(options.case_numeric_attributes, {});
  // Generic lists are deterministic and infer their representation from data.
  for (const auto& n : options.event_attributes) {
    bool numeric = false;
    for (const auto& t : log.traces) for (const auto& e : t.events)
      if (const auto* a = attr_for(e, n)) { numeric = is_numeric_kind(a->kind); break; }
    (numeric ? event_numbers : event_strings).push_back(n);
  }
  for (const auto& n : options.case_attributes) {
    bool numeric = false;
    for (const auto& t : log.traces)
      if (const auto* a = attr_for(t, n)) { numeric = is_numeric_kind(a->kind); break; }
    (numeric ? case_numbers : case_strings).push_back(n);
  }
  append_unique(event_strings, {}); append_unique(event_numbers, {});
  append_unique(case_strings, {}); append_unique(case_numbers, {});
  if (options.event_string_attributes.empty() && options.event_attributes.empty())
    event_strings = inferred_event_attributes(log, false);
  if (options.event_numeric_attributes.empty() && options.event_attributes.empty())
    event_numbers = inferred_event_attributes(log, true);
  if (options.variant == LogToFeaturesVariant::TRACE_BASED) {
    if (options.case_string_attributes.empty() && options.case_attributes.empty())
      case_strings = inferred_trace_attributes(log, false);
    if (options.case_numeric_attributes.empty() && options.case_attributes.empty())
      case_numbers = inferred_trace_attributes(log, true);
  }

  if (options.variant == LogToFeaturesVariant::EVENT_BASED) {
    std::vector<std::string> names;
    const auto activities = categories_for_events(log, options.activity_key);
    for (const auto& x : activities) names.push_back("activity:" + x);
    for (const auto& n : event_strings) {
      const auto values = categories_for_attr(log, n, false);
      if (!within_cardinality(values, options)) continue;
      for (const auto& x : values) names.push_back("event:" + n + "@" + x);
    }
    for (const auto& n : case_strings) {
      const auto values = categories_for_attr(log, n, true);
      if (!within_cardinality(values, options)) continue;
      for (const auto& x : values) names.push_back("case:" + n + "=" + x);
    }
    for (const auto& n : event_numbers) names.push_back("event:" + n);
    for (const auto& n : case_numbers) names.push_back("case:" + n);
    names.push_back("timestamp");
    std::sort(names.begin(), names.end());
    if (options.feature_names.empty()) out.feature_names = names;
    else validate_names(out.feature_names, "feature_names");
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < out.feature_names.size(); ++i) index[out.feature_names[i]] = i;
    std::size_t max_events = 0;
    for (const auto& trace : log.traces) max_events = std::max(max_events, trace.events.size());
    for (const auto& t : log.traces) {
      std::vector<std::vector<double>> rows;
      for (const auto& e : t.events) {
        std::vector<double> row(out.feature_names.size(), options.default_not_present);
        auto set = [&](const std::string& n, double v) { if (auto i = index.find(n); i != index.end()) row[i->second] = v; };
        set("activity:" + value_for(e, options.activity_key), 1.0);
        for (const auto& n : event_strings) {
          const auto values = categories_for_attr(log, n, false);
          if (within_cardinality(values, options))
            if (const auto* attribute = attr_for(e, n))
              set("event:" + n + "@" + attribute->value, 1.0);
        }
        for (const auto& n : case_strings) {
          const auto values = categories_for_attr(log, n, true);
          if (within_cardinality(values, options))
            if (const auto* attribute = attr_for(t, n))
              set("case:" + n + "=" + attribute->value, 1.0);
        }
        for (const auto& n : event_numbers)
          if (event_attribute_present(e, n))
            set("event:" + n, numeric_attribute_value(e, n, options.default_not_present));
        for (const auto& n : case_numbers) if (const auto* a = attr_for(t, n)) set("case:" + n, checked_number(a->value, options.default_not_present));
        if (const auto p = parse_timestamp(value_for(e, options.timestamp_key)); p && std::isfinite(*p)) set("timestamp", *p);
        else if (!value_for(e, options.timestamp_key).empty()) throw std::invalid_argument("log_to_features: invalid timestamp");
        rows.push_back(std::move(row));
      }
      // Event-based consumers receive a rectangular trace/event tensor.  The
      // trailing rows are neutral zero vectors and never represent events.
      while (rows.size() < max_events)
        rows.emplace_back(out.feature_names.size(), 0.0);
      out.event_rows.push_back(std::move(rows));
    }
    return out;
  }

  if (options.variant == LogToFeaturesVariant::TRACE_BASED) {
    const auto activities = categories_for_events(log, options.activity_key);
    std::set<std::pair<std::string, std::string>> observed_direct_paths;
    std::set<std::pair<std::string, std::string>> observed_indirect_paths;
    for (const auto& trace : log.traces) {
      for (std::size_t i = 1; i < trace.events.size(); ++i) {
        observed_direct_paths.emplace(
            value_for(trace.events[i - 1], options.activity_key),
            value_for(trace.events[i], options.activity_key));
      }
      for (std::size_t i = 0; i + 2 < trace.events.size(); ++i) {
        observed_indirect_paths.emplace(
            value_for(trace.events[i], options.activity_key),
            value_for(trace.events[i + 2], options.activity_key));
      }
    }
    std::vector<std::string> names;
    auto add_name = [&](std::string name) {
      if (std::find(names.begin(), names.end(), name) == names.end())
        names.push_back(std::move(name));
    };
    for (const auto& a : activities) {
      add_name("activity_count:" + a);
      add_name("first_activity:" + a);
      add_name("last_activity:" + a);
      add_name("activity_index:" + a);
      if (options.enable_activity_def_representation)
        add_name("activity:" + a);
    }
    names.insert(names.end(), {"duration", "wip"});
    for (const auto& n : event_numbers) names.push_back("event:" + n);
    for (const auto& n : case_numbers) names.push_back("case:" + n);
    for (const auto& n : event_strings) {
      const auto values = distinct_event_values(log, n);
      if (!within_cardinality(values, options)) continue;
      for (const auto& value : values) add_name("event:" + n + "@" + value);
    }
    for (const auto& n : case_strings) {
      const auto values = distinct_trace_values(log, n);
      if (!within_cardinality(values, options)) continue;
      for (const auto& value : values) add_name("trace:" + n + "@" + value);
    }
    std::vector<std::string> successor_attributes = options.event_successor_attributes;
    if (successor_attributes.empty() && options.enable_succ_def_representation)
      successor_attributes.push_back(options.activity_key);
    append_unique(successor_attributes, {});
    if (options.enable_succ_def_representation) {
      for (const auto& key : successor_attributes) {
        std::set<std::pair<std::string, std::string>> pairs;
        for (const auto& trace : log.traces)
          for (std::size_t i = 1; i < trace.events.size(); ++i)
            pairs.emplace(categorical_value(event_attribute_value(trace.events[i - 1], key)),
                          categorical_value(event_attribute_value(trace.events[i], key)));
        for (const auto& pair : pairs)
          add_name("succession:" + key + "@" + pair.first + "#" + pair.second);
      }
    }
    const bool all_extra = options.enable_all_extra_features;
    const bool case_duration = all_extra || options.enable_case_duration;
    const bool case_id = all_extra || options.add_case_identifier_column;
    const bool first_times = all_extra || options.enable_times_from_first_occurrence;
    const bool last_times = all_extra || options.enable_times_from_last_occurrence;
    const bool direct_paths = all_extra || options.enable_direct_paths_times_last_occ;
    const bool indirect_paths = all_extra || options.enable_indirect_paths_times_last_occ;
    const bool wip = all_extra || options.enable_work_in_progress;
    const bool workload = all_extra || options.enable_resource_workload;
    const bool indexes = all_extra || options.enable_first_last_activity_index;
    const bool max_concurrent = all_extra || options.enable_max_concurrent_events;
    const bool max_per_activity = all_extra || options.enable_max_concurrent_events_per_activity;
    if (case_duration) add_name("@@caseDuration");
    if (case_id) add_name("@@case_id_column");
    if (first_times) for (const auto& a : activities) {
      add_name("startToFirstOcc@@" + a); add_name("firstOccToEnd@@" + a);
    }
    if (last_times) for (const auto& a : activities) {
      add_name("startToLastOcc@@" + a); add_name("lastOccToEnd@@" + a);
    }
    if (direct_paths)
      for (const auto& [from, to] : observed_direct_paths)
        add_name("directPathPerformanceLastOcc@@" + from + "##" + to);
    if (indirect_paths)
      for (const auto& [from, to] : observed_indirect_paths)
        add_name("indirectPathPerformanceLastOcc@@" + from + "##" + to);
    if (wip) add_name("@@work_in_progress");
    if (indexes) for (const auto& a : activities) {
      add_name("firstIndexAct@@" + a); add_name("lastIndexAct@@" + a);
    }
    if (max_concurrent) add_name("@@max_concurrent_activities_general");
    if (max_per_activity) for (const auto& a : activities)
      add_name("@@max_concurrent_activities_like_" + a);
    std::set<std::string> resources;
    for (const auto& t : log.traces) for (const auto& e : t.events) resources.insert(value_for(e, options.resource_key));
    for (const auto& r : resources) if (!r.empty()) {
      add_name("resource_workload:" + r);
      if (workload) add_name("resource_workload@@" + r);
    }
    if (options.feature_names.empty()) out.feature_names = names; else { validate_names(options.feature_names, "feature_names"); }
    std::unordered_map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < out.feature_names.size(); ++i) index[out.feature_names[i]] = i;
    const auto path_start = [&](const Event& event) {
      return instant(event,
                     options.start_timestamp_key.empty()
                         ? options.timestamp_key
                         : options.start_timestamp_key,
                     false);
    };
    struct CaseInterval {
      double start = std::numeric_limits<double>::quiet_NaN();
      double finish = std::numeric_limits<double>::quiet_NaN();
      bool valid = false;
      std::set<std::string> resources;
    };
    std::vector<CaseInterval> case_intervals(log.traces.size());
    std::map<std::string, std::vector<std::pair<double, double>>>
        resource_intervals;
    for (std::size_t ti = 0; ti < log.traces.size(); ++ti) {
      const auto& trace = log.traces[ti];
      if (trace.events.empty()) continue;
      auto& interval = case_intervals[ti];
      interval.start = path_start(trace.events.front());
      interval.finish = instant(trace.events.back(), options.timestamp_key, false);
      interval.valid = std::isfinite(interval.start) &&
                       std::isfinite(interval.finish) &&
                       interval.start <= interval.finish;
      for (const auto& event : trace.events) {
        const auto resource = value_for(event, options.resource_key);
        if (!resource.empty()) interval.resources.insert(resource);
      }
      if (!interval.valid) continue;
      for (const auto& resource : interval.resources)
        resource_intervals[resource].emplace_back(interval.start,
                                                   interval.finish);
    }
    std::size_t trace_index = 0;
    for (const auto& t : log.traces) {
      std::vector<double> row(out.feature_names.size(), options.default_not_present);
      auto set = [&](const std::string& n, double v) { if (auto i = index.find(n); i != index.end()) row[i->second] = v; };
      std::map<std::string, int> count; std::map<std::string, std::size_t> first, last;
      std::set<std::string> seen; double lo = 0, hi = 0; bool have = false;
      double case_start = std::numeric_limits<double>::infinity();
      double case_end = -std::numeric_limits<double>::infinity();
      std::map<std::string, double> first_time, last_time;
      std::vector<TimedEvent> timed;
      std::map<std::string, int> resource_count;
      for (std::size_t i = 0; i < t.events.size(); ++i) {
        const auto& e = t.events[i]; const auto a = value_for(e, options.activity_key); ++count[a]; if (!seen.count(a)) first[a] = i; last[a] = i; seen.insert(a);
        const auto text = value_for(e, options.timestamp_key); if (!text.empty()) { auto p = parse_timestamp(text); if (!p || !std::isfinite(*p)) throw std::invalid_argument("log_to_features: invalid timestamp"); if (!have) lo = hi = *p; else { lo = std::min(lo,*p); hi = std::max(hi,*p); } have = true; }
        const auto start_text = options.start_timestamp_key.empty() ? text : value_for(e, options.start_timestamp_key);
        const auto start = start_text.empty() ? std::numeric_limits<double>::quiet_NaN() : instant(e, options.start_timestamp_key.empty() ? options.timestamp_key : options.start_timestamp_key, false);
        const auto finish = text.empty() ? std::numeric_limits<double>::quiet_NaN() : instant(e, options.timestamp_key, false);
        if (std::isfinite(start) && std::isfinite(finish)) {
          timed.push_back({&e, finish, start, 0, i});
          case_start = std::min(case_start, start);
          case_end = std::max(case_end, finish);
        }
        if (std::isfinite(finish)) {
          if (!first_time.contains(a)) first_time[a] = finish;
          last_time[a] = finish;
        }
        const auto r = value_for(e, options.resource_key);
        if (!r.empty()) {
          ++resource_count[r];
          const auto it = index.find("resource_workload:" + r);
          if (it != index.end()) set(it->first, row[it->second] + 1.0);
        }
      }
      for (const auto& [a,c] : count) { set("activity_count:"+a,c); set("first_activity:"+a, first[a] == 0 ? 1.0 : 0.0); set("last_activity:"+a, !t.events.empty() && value_for(t.events.back(), options.activity_key)==a ? 1.0 : 0.0); set("activity_index:"+a, static_cast<double>(first[a])); set("activity:"+a, 1.0); if (indexes) { set("firstIndexAct@@"+a, static_cast<double>(first[a])); set("lastIndexAct@@"+a, static_cast<double>(last[a])); } }
      for (const auto& n : event_strings) {
        const auto values = distinct_event_values(log, n);
        if (!within_cardinality(values, options)) continue;
        for (const auto& value : event_attribute_values_for_trace(t, n))
          set("event:" + n + "@" + value, 1.0);
      }
      for (const auto& n : case_strings) { const auto values = distinct_trace_values(log, n); if (within_cardinality(values, options)) set("trace:"+n+"@"+categorical_value(trace_attribute_value(t,n)), 1.0); }
      for (const auto& n : successor_attributes) if (options.enable_succ_def_representation) for (std::size_t i=1;i<t.events.size();++i) set("succession:"+n+"@"+categorical_value(event_attribute_value(t.events[i-1],n))+"#"+categorical_value(event_attribute_value(t.events[i],n)), 1.0);
      if (have) {
        set("duration", hi-lo);
        if (case_duration && std::isfinite(case_start) && std::isfinite(case_end))
          set("@@caseDuration", case_end-case_start);
      }
      set("wip", static_cast<double>(t.events.size()));
      for (const auto& n : event_numbers) { double value = options.default_not_present; bool found = false; for (const auto& e:t.events) if (event_attribute_present(e,n)) { value = numeric_attribute_value(e,n,options.default_not_present); found = true; } if (found) set("event:"+n,value); }
      for (const auto& n : case_numbers) if (attr_for(t,n) || !trace_attribute_value(t,n).empty()) set("case:"+n,numeric_trace_value(t,n,options.default_not_present));
      for (const auto& [r, c] : resource_count) if (workload) set("resource_workload@@" + r, static_cast<double>(c));
      for (const auto& [a, first_value] : first_time) {
        if (first_times && std::isfinite(case_start) && std::isfinite(case_end)) { set("startToFirstOcc@@"+a, first_value-case_start); set("firstOccToEnd@@"+a, case_end-first_value); }
        if (last_times && std::isfinite(case_start) && std::isfinite(case_end)) { const auto last_value = last_time[a]; set("startToLastOcc@@"+a, last_value-case_start); set("lastOccToEnd@@"+a, case_end-last_value); }
      }
      if (direct_paths) for (std::size_t i = 1; i < t.events.size(); ++i) {
        const auto from = value_for(t.events[i - 1], options.activity_key);
        const auto to = value_for(t.events[i], options.activity_key);
        const auto finish = instant(t.events[i - 1], options.timestamp_key, false);
        const auto start = path_start(t.events[i]);
        if (std::isfinite(finish) && std::isfinite(start) && start > finish)
          set("directPathPerformanceLastOcc@@" + from + "##" + to,
              start - finish);
      }
      if (indirect_paths)
        for (std::size_t i = 0; i + 2 < t.events.size(); ++i) {
          const auto from = value_for(t.events[i], options.activity_key);
          const auto to = value_for(t.events[i + 2], options.activity_key);
          const auto finish = instant(t.events[i], options.timestamp_key, false);
          const auto start = path_start(t.events[i + 2]);
          if (std::isfinite(finish) && std::isfinite(start) && start > finish)
            set("indirectPathPerformanceLastOcc@@" + from + "##" + to,
                start - finish);
        }
      if (wip) {
        const auto& interval = case_intervals[trace_index];
        if (interval.valid) {
          std::size_t overlapping = 0;
          for (const auto& other : case_intervals) {
            if (other.valid && other.start <= interval.finish + options.epsilon &&
                other.finish + options.epsilon >= interval.start)
              ++overlapping;
          }
          set("@@work_in_progress", static_cast<double>(overlapping));
        }
      }
      if (workload) {
        const auto& interval = case_intervals[trace_index];
        if (interval.valid) {
          for (const auto& resource : interval.resources) {
            const auto found = resource_intervals.find(resource);
            if (found == resource_intervals.end()) continue;
            std::size_t overlapping = 0;
            for (const auto& other : found->second) {
              if (other.first <= interval.finish + options.epsilon &&
                  other.second + options.epsilon >= interval.start)
                ++overlapping;
            }
            set("resource_workload@@" + resource,
                static_cast<double>(overlapping));
          }
        }
      }
      if (max_concurrent || max_per_activity) {
        int general = 0;
        std::map<std::string, int> per_activity;
        for (std::size_t i = 0; i + 1 < t.events.size(); ++i) {
          const auto finish = instant(t.events[i], options.timestamp_key, false);
          if (!std::isfinite(finish)) continue;
          const auto activity = value_for(t.events[i], options.activity_key);
          int active = 0;
          int same_activity = 0;
          for (std::size_t j = i + 1; j < t.events.size(); ++j) {
            const auto start = path_start(t.events[j]);
            if (!std::isfinite(start) || start > finish + options.epsilon)
              break;
            ++active;
            if (value_for(t.events[j], options.activity_key) == activity)
              ++same_activity;
          }
          general = std::max(general, active);
          per_activity[activity] =
              std::max(per_activity[activity], same_activity);
        }
        if (max_concurrent)
          set("@@max_concurrent_activities_general",
              static_cast<double>(general));
        if (max_per_activity)
          for (const auto& [activity, overlap_count] : per_activity)
            set("@@max_concurrent_activities_like_" + activity,
                static_cast<double>(overlap_count));
      }
      if (case_id) out.case_ids.push_back(t.case_id);
      out.trace_rows.push_back(std::move(row));
      ++trace_index;
    }
    return out;
  }

  if (options.variant != LogToFeaturesVariant::TEMPORAL) throw std::invalid_argument("log_to_features: unknown variant");
  const std::optional<TemporalFrequency> temporal_frequency =
      options.grouper_freq.empty()
          ? std::nullopt
          : std::optional<TemporalFrequency>(
                parse_temporal_frequency(options.grouper_freq));
  std::vector<TimedEvent> events;
  double origin = std::numeric_limits<double>::infinity();
  struct CaseTemporal {
    double start = std::numeric_limits<double>::infinity();
    double finish = -std::numeric_limits<double>::infinity();
    double service = 0.0;
    std::size_t event_count = 0;
    std::set<std::string> activities;
    std::set<std::string> resources;
  };
  std::vector<CaseTemporal> cases(log.traces.size());
  for (std::size_t ti = 0; ti < log.traces.size(); ++ti) {
    auto& metrics = cases[ti];
    for (std::size_t i = 0; i < log.traces[ti].events.size(); ++i) {
      const auto& event = log.traces[ti].events[i];
      const double finish = instant(event, options.timestamp_key, true);
      const double start = options.start_timestamp_key.empty()
                               ? finish
                               : instant(event, options.start_timestamp_key, true);
      if (start > finish)
        throw std::invalid_argument("log_to_features: start timestamp after finish");
      origin = std::min(origin, start);
      events.push_back({&event, finish, start, ti, i});
      metrics.start = std::min(metrics.start, start);
      metrics.finish = std::max(metrics.finish, finish);
      metrics.service += finish - start;
      ++metrics.event_count;
      metrics.activities.insert(value_for(event, options.activity_key));
      const auto resource = value_for(event, options.resource_key);
      if (!resource.empty()) metrics.resources.insert(resource);
    }
  }
  out.feature_names = options.feature_names;
  if (out.feature_names.empty()) {
    out.feature_names = {
        "unique_resources", "unique_activities", "num_events",
        "average_arrival_rate", "average_finish_rate", "average_waiting_time",
        "average_sojourn_time", "average_service_time",
        "total_number_of_reworked_activities", "avg_cases_per_resource",
        "avg_events_per_case", "number_of_cases", "avg_resources_per_case"};
  } else {
    validate_names(out.feature_names, "feature_names");
  }
  if (events.empty()) return out;
  std::vector<std::size_t> arrival_order(log.traces.size());
  std::vector<std::size_t> finish_order(log.traces.size());
  for (std::size_t i = 0; i < log.traces.size(); ++i) {
    arrival_order[i] = i;
    finish_order[i] = i;
  }
  auto case_less = [&](std::size_t lhs, std::size_t rhs, bool by_finish) {
    const double a = by_finish ? cases[lhs].finish : cases[lhs].start;
    const double b = by_finish ? cases[rhs].finish : cases[rhs].start;
    if (a != b) return a < b;
    if (log.traces[lhs].case_id != log.traces[rhs].case_id)
      return log.traces[lhs].case_id < log.traces[rhs].case_id;
    return lhs < rhs;
  };
  std::sort(arrival_order.begin(), arrival_order.end(),
            [&](std::size_t lhs, std::size_t rhs) { return case_less(lhs, rhs, false); });
  std::sort(finish_order.begin(), finish_order.end(),
            [&](std::size_t lhs, std::size_t rhs) { return case_less(lhs, rhs, true); });
  std::vector<double> arrival_rate(log.traces.size(), 0.0);
  std::vector<double> finish_rate(log.traces.size(), 0.0);
  for (std::size_t i = 1; i < arrival_order.size(); ++i)
    arrival_rate[arrival_order[i]] = cases[arrival_order[i]].start - cases[arrival_order[i - 1]].start;
  for (std::size_t i = 1; i < finish_order.size(); ++i)
    finish_rate[finish_order[i]] = cases[finish_order[i]].finish - cases[finish_order[i - 1]].finish;
  std::map<long long, std::vector<const TimedEvent*>> buckets;
  std::map<long long, double> bucket_timestamps;
  for (const auto& event : events) {
    if (temporal_frequency) {
      const auto label = temporal_bucket_label(event.start, *temporal_frequency);
      buckets[label].push_back(&event);
      bucket_timestamps[label] = static_cast<double>(label);
    } else {
      const auto bucket = static_cast<long long>(
          std::floor((event.start - origin) / options.window_seconds));
      buckets[bucket].push_back(&event);
      bucket_timestamps[bucket] =
          origin + static_cast<double>(bucket) * options.window_seconds;
    }
  }
  std::unordered_map<std::string,std::size_t> index; for (std::size_t i=0;i<out.feature_names.size();++i) index[out.feature_names[i]]=i;
  for (const auto& [bucket, bucket_events] : buckets) {
    TemporalFeatureRow row;
    row.timestamp = bucket_timestamps.at(bucket);
    row.values.assign(out.feature_names.size(), options.default_not_present);
    auto add = [&](const std::string& name, double value) {
      if (const auto it = index.find(name); it != index.end()) row.values[it->second] = value;
    };
    std::set<std::size_t> trace_ids;
    std::set<std::string> resources;
    std::set<std::string> activities;
    std::map<std::size_t, std::vector<const TimedEvent*>> by_trace;
    std::map<std::string, std::set<std::size_t>> cases_by_resource;
    std::map<std::size_t, std::set<std::string>> resources_by_trace;
    for (const auto* event : bucket_events) {
      trace_ids.insert(event->trace);
      by_trace[event->trace].push_back(event);
      activities.insert(value_for(*event->event, options.activity_key));
      const auto resource = value_for(*event->event, options.resource_key);
      if (!resource.empty()) {
        resources.insert(resource);
        cases_by_resource[resource].insert(event->trace);
        resources_by_trace[event->trace].insert(resource);
      }
    }
    double average_arrival = 0.0, average_finish = 0.0;
    double average_waiting = 0.0, average_sojourn = 0.0, average_service = 0.0;
    for (const auto trace_id : trace_ids) {
      average_arrival += arrival_rate[trace_id];
      average_finish += finish_rate[trace_id];
      const auto& metrics = cases[trace_id];
      const double sojourn = metrics.finish - metrics.start;
      average_waiting += sojourn - metrics.service;
      average_sojourn += sojourn;
      average_service += metrics.service;
    }
    const double case_count = static_cast<double>(trace_ids.size());
    if (case_count > 0.0) {
      average_arrival /= case_count;
      average_finish /= case_count;
      average_waiting /= case_count;
      average_sojourn /= case_count;
      average_service /= case_count;
    }
    double reworked = 0.0;
    double events_per_case = 0.0;
    double resources_per_case = 0.0;
    for (const auto& [trace_id, trace_events] : by_trace) {
      std::set<std::string> trace_activities;
      for (const auto* event : trace_events)
        trace_activities.insert(value_for(*event->event, options.activity_key));
      reworked += static_cast<double>(trace_events.size() - trace_activities.size());
      events_per_case += static_cast<double>(trace_events.size());
      resources_per_case += static_cast<double>(resources_by_trace[trace_id].size());
    }
    if (case_count > 0.0) {
      events_per_case /= case_count;
      resources_per_case /= case_count;
    }
    double cases_per_resource = 0.0;
    for (const auto& [resource, resource_cases] : cases_by_resource)
      cases_per_resource += static_cast<double>(resource_cases.size());
    if (!cases_by_resource.empty()) cases_per_resource /= static_cast<double>(cases_by_resource.size());
    add("unique_resources", static_cast<double>(resources.size()));
    add("unique_activities", static_cast<double>(activities.size()));
    add("num_events", static_cast<double>(bucket_events.size()));
    add("average_arrival_rate", average_arrival);
    add("average_finish_rate", average_finish);
    add("average_waiting_time", average_waiting);
    add("average_sojourn_time", average_sojourn);
    add("average_service_time", average_service);
    add("total_number_of_reworked_activities", reworked);
    add("avg_cases_per_resource", cases_per_resource);
    add("avg_events_per_case", events_per_case);
    add("number_of_cases", case_count);
    add("avg_resources_per_case", resources_per_case);
    // Keep the former compact schema as an explicit compatibility projection.
    add("arrivals", static_cast<double>(bucket_events.size()));
    add("finishes", static_cast<double>(bucket_events.size()));
    add("service_seconds", average_service * case_count);
    add("waiting_seconds", average_waiting * case_count);
    add("sojourn_seconds", average_sojourn * case_count);
    add("cases", case_count);
    add("resource_workload", static_cast<double>(bucket_events.size()));
    add("rework", reworked);
    out.temporal_rows.push_back(std::move(row));
  }
  return out;
}

}  // namespace minera
