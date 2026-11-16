#include "minera/log_util.hpp"

#include "minera/random.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <map>
#include <numeric>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>

#include "minera/dataframe_columns.hpp"
#include "minera/frame_keys.hpp"
#include "minera/timestamp.hpp"
#include "frame_cells.hpp"
#include "full_double.hpp"
#include "python_scalar.hpp"

namespace minera {
namespace {

EventLog with_log_attributes(const EventLog& source) {
  EventLog result;
  copy_event_log_context(source, result);
  return result;
}

std::string event_value(const Event& event, std::string_view key) {
  if (key.empty() || key == "activity" || key == "concept:name")
    return event.activity;
  if (key == "timestamp" || key == "time:timestamp") return event.timestamp;
  if (key == "org:resource" || key == "resource") return event.resource;
  if (const auto* value = event.attribute(key)) return *value;
  return {};
}

double parse_elapsed(const Event& previous, const Event& target,
                     std::string_view timestamp_key, std::size_t trace_index) {
  const auto previous_text = event_value(previous, timestamp_key);
  const auto target_text = event_value(target, timestamp_key);
  const auto parse = [](const std::string& value) -> std::optional<double> {
    if (const auto parsed = parse_timestamp(value)) return parsed;
    try {
      std::size_t consumed = 0;
      const double number = ::minera::detail::stod(value, &consumed);
      if (consumed == value.size() && std::isfinite(number)) return number;
    } catch (const std::exception&) {
    }
    return std::nullopt;
  };
  const auto start = parse(previous_text);
  const auto end = parse(target_text);
  if (!start || !end) {
    throw std::invalid_argument(
        "until_activity: invalid timestamps in trace " +
        std::to_string(trace_index));
  }
  // The reference subtracts the two instants as given, so a target recorded
  // before its predecessor yields a negative duration.
  return *end - *start;
}

// pm4py's prefix utilities run on convert_to_event_log of an upload's frame
// (CSV and XES uploads): every key is a column of the frame (the role names
// "activity" and "timestamp" stand for concept:name and time:timestamp), a
// cell is the value pandas typed, an activity is found by Python equality
// with the requested text, and the target's elapsed time is the plain
// difference of the two typed cells (round CX, observed black-box).
struct FrameView {
  EventLog frame;
  std::set<std::string> keys;
};

std::optional<FrameView> frame_view(const EventLog& log) {
  if (!log.dataframe_columns) return std::nullopt;
  FrameView view{dataframe_frame_event_log(log), {}};
  if (view.frame.traces.size() != log.traces.size()) return std::nullopt;
  for (std::size_t t = 0; t < log.traces.size(); ++t)
    if (view.frame.traces[t].events.size() != log.traces[t].events.size()) return std::nullopt;
  view.keys = frame_keys(view.frame, FrameKeyScope::kEventLog);
  return view;
}

std::string role_key(const std::string& key, std::string_view alias, const char* standard) {
  return key.empty() || key == alias ? std::string(standard) : key;
}

std::optional<python::Scalar> frame_cell(const FrameView& view, std::size_t trace, std::size_t event,
                                         const std::string& key) {
  const auto& source = view.frame.traces[trace];
  return frame_scalar(view.frame, source, source.events[event], key, FrameKeyScope::kEventLog);
}

bool frame_activity_is(const FrameView& view, std::size_t trace, std::size_t event,
                       const std::string& key, const std::string& activity) {
  const auto value = frame_cell(view, trace, event, key);
  return value && value->kind == python::Scalar::Kind::kText && value->text == activity;
}

double python_float(const python::Scalar& value) {
  if (value.kind == python::Scalar::Kind::kFloat) return value.real;
  if (value.kind == python::Scalar::Kind::kBool) return value.boolean ? 1.0 : 0.0;
  const double magnitude = std::strtod(value.digits.c_str(), nullptr);
  return value.negative ? -magnitude : magnitude;
}

// target - previous on the frame's typed cells, as a JSON literal: a
// Timedelta's total_seconds() for two dates, an int for two ints (bools
// count as ints), a float otherwise; null for NaN or NaT. Text, or a date
// beside a number, raises Python's TypeError; a key no event holds raises
// KeyError.
std::string frame_elapsed(const FrameView& view, std::size_t trace, std::size_t target,
                          const std::string& key) {
  if (!view.keys.contains(key)) throw std::invalid_argument("KeyError: '" + key + "'");
  const bool date_column = frame_column_type(view.frame, key) == DataFrameColumnType::kDate;
  const auto end = frame_cell(view, trace, target, key);
  const auto start = frame_cell(view, trace, target - 1, key);
  using Kind = python::Scalar::Kind;
  const auto name = [&](const std::optional<python::Scalar>& value) {
    return value ? python::type_name(*value) : std::string(date_column ? "NaTType" : "float");
  };
  const auto kind = [&](const std::optional<python::Scalar>& value) {
    if (!value) return date_column ? 'T' : 'F';  // NaT, NaN
    switch (value->kind) {
      case Kind::kDate: return 'D';
      case Kind::kBool:
      case Kind::kInt: return 'I';
      case Kind::kFloat: return 'F';
      default: return 'X';
    }
  };
  const char a = kind(end), b = kind(start);
  const auto unsupported = [&] {
    throw std::invalid_argument("TypeError: unsupported operand type(s) for -: '" + name(end) +
                                "' and '" + name(start) + "'");
  };
  if (a == 'X' || b == 'X') unsupported();
  if (a == 'D' || b == 'D') {
    if (a == 'T' || b == 'T') return "null";
    if (a != 'D' || b != 'D') unsupported();
    // total_seconds() of the Timedelta: microseconds / 1e6 when the
    // difference is whole microseconds (a microsecond column spans far
    // more than 2**63 ns), nanoseconds / 1e9 otherwise.
    const std::int64_t whole_seconds = end->date.seconds - start->date.seconds;
    const std::int64_t nanos =
        static_cast<std::int64_t>(end->date.nanoseconds) - static_cast<std::int64_t>(start->date.nanoseconds);
    const double seconds = nanos % 1000 == 0
                               ? static_cast<double>(whole_seconds * 1'000'000 + nanos / 1000) / 1e6
                               : static_cast<double>(whole_seconds * 1'000'000'000 + nanos) / 1e9;
    return python::float_repr(seconds);
  }
  if (a == 'T' || b == 'T') return "null";
  if (a == 'I' && b == 'I') {
    const auto digits = [](const python::Scalar& value) {
      return value.kind == Kind::kBool ? std::string(value.boolean ? "1" : "0") : value.digits;
    };
    const auto difference = python::add_integers(end->negative, digits(*end), !start->negative, digits(*start));
    return (difference.negative ? "-" : "") + difference.digits;
  }
  const double left = end ? python_float(*end) : std::numeric_limits<double>::quiet_NaN();
  const double right = start ? python_float(*start) : std::numeric_limits<double>::quiet_NaN();
  const double difference = left - right;
  return std::isnan(difference) ? "null" : python::float_repr(difference);
}

double literal_value(const std::string& literal) {
  if (literal == "null") return std::numeric_limits<double>::quiet_NaN();
  return ::minera::detail::stod(literal);
}

}  // namespace

LogTrainTestSplit split_log_train_test(const EventLog& log,
                                       double train_percentage,
                                       std::uint64_t seed,
                                       SplitTrainTestInput input) {
  if (!std::isfinite(train_percentage) || train_percentage < 0.0 ||
      train_percentage > 1.0) {
    throw std::invalid_argument(
        "train_percentage must be finite and in the range [0, 1]");
  }

  LogTrainTestSplit result{with_log_attributes(log), with_log_attributes(log)};
  rng::PythonRandom random(seed);
  if (input == SplitTrainTestInput::kDataFrame) {
    for (const auto& trace : log.traces) {
      if (trace.events.empty()) continue;  // no rows, no case
      (random.random() <= train_percentage ? result.training : result.testing)
          .traces.push_back(trace);
    }
    return result;
  }
  std::vector<std::size_t> indexes(log.traces.size());
  std::iota(indexes.begin(), indexes.end(), 0);
  random.shuffle(indexes);
  const auto requested = static_cast<std::size_t>(
      std::floor(static_cast<double>(indexes.size()) * train_percentage));
  // The reference includes one trace even for a zero percentage whenever
  // the input is non-empty, capped so a percentage of one stays a partition.
  const auto stop = indexes.empty() ? std::size_t{0}
                                    : std::min(indexes.size(), requested + 1);
  for (std::size_t position = 0; position < indexes.size(); ++position) {
    if (position < stop)
      result.training.traces.push_back(log.traces[indexes[position]]);
    else
      result.testing.traces.push_back(log.traces[indexes[position]]);
  }
  return result;
}

EventLog get_log_prefixes(const EventLog& log, std::size_t length) {
  EventLog result = with_log_attributes(log);
  result.traces.reserve(log.traces.size());
  for (const auto& source : log.traces) {
    Trace prefix = source;
    if (prefix.events.size() > length) prefix.events.resize(length);
    // On an upload's frame pm4py.get_prefixes_from_log keeps each case's
    // first `length` rows, so a case left without rows is gone (round CX).
    if (log.dataframe_columns && prefix.events.empty()) continue;
    result.traces.push_back(std::move(prefix));
  }
  return result;
}

LogPrefixResult get_log_with_prefixes(const EventLog& log) {
  LogPrefixResult result;
  result.log = with_log_attributes(log);
  result.log.traces.reserve(log.event_count() * 2);
  result.change_indexes.reserve(log.traces.size());
  for (const auto& source : log.traces) {
    // As the reference get_log_with_log_prefixes, each prefix is a fresh
    // trace: it carries the events only, without the trace's attributes,
    // properties or case id.
    Trace cumulative;
    std::vector<std::size_t> changes;
    changes.reserve(source.events.size());
    for (const auto& event : source.events) {
      result.log.traces.push_back(cumulative);
      cumulative.events.push_back(event);
      result.log.traces.push_back(cumulative);
    }
    if (!source.events.empty()) {
      const auto final_index = result.log.traces.size() - 1;
      changes.assign(source.events.size(), final_index);
    }
    result.change_indexes.push_back(std::move(changes));
  }
  return result;
}

LogUntilActivityResult get_log_traces_until_activity(
    const EventLog& log, const std::string& activity,
    const LogUtilityOptions& options) {
  if (activity.empty())
    throw std::invalid_argument("until_activity: activity must not be empty");
  if (options.activity_key.empty())
    throw std::invalid_argument("until_activity: activity_key must not be empty");

  LogUntilActivityResult result;
  result.log = with_log_attributes(log);
  if (const auto view = frame_view(log)) {
    const auto activity_key = role_key(options.activity_key, "activity", "concept:name");
    const auto timestamp_key = role_key(options.timestamp_key, "timestamp", "time:timestamp");
    if (log.event_count() > 0 && !view->keys.contains(activity_key))
      throw std::invalid_argument("KeyError: '" + activity_key + "'");
    for (std::size_t t = 0; t < log.traces.size(); ++t) {
      const auto& source = log.traces[t];
      std::size_t target = 0;
      while (target < source.events.size() && !frame_activity_is(*view, t, target, activity_key, activity))
        ++target;
      if (target == source.events.size() || target == 0) continue;
      const auto literal = frame_elapsed(*view, t, target, timestamp_key);
      Trace prefix = source;
      prefix.events.assign(source.events.begin(), source.events.begin() + static_cast<std::ptrdiff_t>(target));
      result.log.traces.push_back(std::move(prefix));
      result.elapsed_literals.push_back(literal);
      result.elapsed_seconds.push_back(literal_value(literal));
    }
    return result;
  }
  for (std::size_t trace_index = 0; trace_index < log.traces.size();
       ++trace_index) {
    const auto& source = log.traces[trace_index];
    const auto target = std::find_if(
        source.events.begin(), source.events.end(), [&](const Event& event) {
          return event_value(event, options.activity_key) == activity;
        });
    if (target == source.events.end() || target == source.events.begin()) continue;
    Trace prefix = source;
    const auto target_index = static_cast<std::size_t>(
        std::distance(source.events.begin(), target));
    prefix.events.assign(source.events.begin(), target);
    result.elapsed_seconds.push_back(parse_elapsed(
        source.events[target_index - 1], *target, options.timestamp_key,
        trace_index));
    result.log.traces.push_back(std::move(prefix));
  }
  return result;
}

LogActivityTargetResult get_log_traces_to_activities(
    const EventLog& log, const std::vector<std::string>& activities,
    const LogUtilityOptions& options) {
  if (activities.empty())
    throw std::invalid_argument("to_activities: activity list must not be empty");
  std::set<std::string> unique(activities.begin(), activities.end());
  if (unique.size() != activities.size() || unique.contains(""))
    throw std::invalid_argument("to_activities: activities must be unique and non-empty");
  LogActivityTargetResult result;
  if (const auto view = frame_view(log)) {
    const auto activity_key = role_key(options.activity_key, "activity", "concept:name");
    const auto timestamp_key = role_key(options.timestamp_key, "timestamp", "time:timestamp");
    // A key no event holds selects nothing (no KeyError, unlike
    // get_log_traces_until_activity).
    if (!view->keys.contains(activity_key)) return result;
    for (const auto& target_activity : activities) {
      EventLog sublog = with_log_attributes(log);
      for (std::size_t t = 0; t < log.traces.size(); ++t) {
        const auto& source = log.traces[t];
        std::size_t target = 0;
        while (target < source.events.size() &&
               !frame_activity_is(*view, t, target, activity_key, target_activity))
          ++target;
        if (target == source.events.size() || target == 0) continue;
        bool other = false;
        for (std::size_t e = 0; e < source.events.size() && !other; ++e)
          for (const auto& activity : unique)
            if (activity != target_activity && frame_activity_is(*view, t, e, activity_key, activity)) {
              other = true;
              break;
            }
        if (other) continue;
        (void)frame_elapsed(*view, t, target, timestamp_key);
        Trace prefix = source;
        prefix.events.assign(source.events.begin(), source.events.begin() + static_cast<std::ptrdiff_t>(target));
        sublog.traces.push_back(std::move(prefix));
      }
      if (!sublog.traces.empty()) {
        result.activities.push_back(target_activity);
        result.logs.push_back(std::move(sublog));
      }
    }
    return result;
  }
  for (const auto& target_activity : activities) {
    std::set<std::string> other_activities = unique;
    other_activities.erase(target_activity);
    EventLog sublog = with_log_attributes(log);
    for (std::size_t trace_index = 0; trace_index < log.traces.size();
         ++trace_index) {
      const auto& source = log.traces[trace_index];
      const auto target = std::find_if(
          source.events.begin(), source.events.end(), [&](const Event& event) {
            return event_value(event, options.activity_key) == target_activity;
          });
      if (target == source.events.end() || target == source.events.begin()) continue;
      const bool has_other = std::any_of(
          source.events.begin(), source.events.end(), [&](const Event& event) {
            return other_activities.contains(event_value(event, options.activity_key));
          });
      if (has_other) continue;
      const auto target_index = static_cast<std::size_t>(
          std::distance(source.events.begin(), target));
      (void)parse_elapsed(source.events[target_index - 1], *target,
                          options.timestamp_key, trace_index);
      Trace prefix = source;
      prefix.events.assign(source.events.begin(), target);
      sublog.traces.push_back(std::move(prefix));
    }
    if (!sublog.traces.empty()) {
      result.activities.push_back(target_activity);
      result.logs.push_back(std::move(sublog));
    }
  }
  return result;
}

LogFeatureSelectionResult select_log_features(
    const EventLog& log, const LogFeatureSelectionOptions& options) {
  if (options.min_different_values > options.max_different_values)
    throw std::invalid_argument(
        "feature selection: minimum different values exceed maximum");
  LogFeatureSelectionResult result;

  struct Summary {
    std::set<std::string> values;
    std::set<std::string> cases;
    std::set<AttributeKind> kinds;
  };
  std::map<std::string, Summary> event_summaries;
  std::map<std::string, Summary> case_summaries;
  const auto add_event = [&](const Trace& trace,
                             const std::string& key, const std::string& value,
                             AttributeKind kind) {
    if (value.empty()) return;
    auto& summary = event_summaries[key];
    summary.values.insert(value);
    summary.cases.insert(trace.case_id);
    summary.kinds.insert(kind);
  };
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      add_event(trace, "activity", event.activity, AttributeKind::kString);
      add_event(trace, "timestamp", event.timestamp, AttributeKind::kDate);
      add_event(trace, "resource", event.resource, AttributeKind::kString);
      add_event(trace, "lifecycle", event.lifecycle, AttributeKind::kString);
      for (const auto& attribute : event.attributes)
        if (!attribute.is_null)
          add_event(trace, attribute.key, attribute.value, attribute.kind);
    }
    for (const auto& attribute : trace.attributes) {
      if (attribute.is_null || attribute.value.empty()) continue;
      auto& summary = case_summaries[attribute.key];
      summary.values.insert(attribute.value);
      summary.cases.insert(trace.case_id);
      summary.kinds.insert(attribute.kind);
    }
  }
  const auto mandatory = [&](const std::string& key) {
    if (std::find(options.mandatory_attributes.begin(),
                  options.mandatory_attributes.end(), key) !=
        options.mandatory_attributes.end()) return true;
    return options.mandatory_attributes.empty() &&
           (key == "activity" || key == "timestamp");
  };
  const auto retain = [&](const Summary& summary, std::size_t case_count,
                          const std::string& key) {
    if (mandatory(key)) return true;
    if (!options.consider_all_attributes && summary.cases.size() != case_count)
      return false;
    if (summary.kinds.contains(AttributeKind::kDate)) return false;
    const bool numeric = !summary.kinds.empty() &&
                         std::all_of(summary.kinds.begin(), summary.kinds.end(),
                                     [](AttributeKind kind) {
                                       return kind == AttributeKind::kInt ||
                                              kind == AttributeKind::kFloat;
                                     });
    if (numeric) return true;
    return summary.values.size() >= options.min_different_values &&
           summary.values.size() <= options.max_different_values;
  };
  for (const auto& [key, summary] : event_summaries) {
    if (retain(summary, log.traces.size(), key))
      result.event_attributes.push_back(key);
  }
  for (const auto& [key, summary] : case_summaries) {
    if (retain(summary, log.traces.size(), key))
      result.case_attributes.push_back(key);
  }
  return result;
}

}  // namespace minera
