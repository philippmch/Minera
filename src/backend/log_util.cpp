#include "minera/log_util.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>

#include "minera/timestamp.hpp"

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
      const double number = std::stod(value, &consumed);
      if (consumed == value.size() && std::isfinite(number)) return number;
    } catch (const std::exception&) {
    }
    return std::nullopt;
  };
  const auto start = parse(previous_text);
  const auto end = parse(target_text);
  if (!start || !end || *end < *start) {
    throw std::invalid_argument(
        "until_activity: invalid or reversed timestamps in trace " +
        std::to_string(trace_index));
  }
  return *end - *start;
}

}  // namespace

LogTrainTestSplit split_log_train_test(const EventLog& log,
                                       double train_percentage,
                                       std::uint64_t seed) {
  if (!std::isfinite(train_percentage) || train_percentage < 0.0 ||
      train_percentage > 1.0) {
    throw std::invalid_argument(
        "train_percentage must be finite and in the range [0, 1]");
  }

  LogTrainTestSplit result{with_log_attributes(log), with_log_attributes(log)};
  std::vector<std::size_t> indexes(log.traces.size());
  std::iota(indexes.begin(), indexes.end(), 0);
  std::mt19937_64 random(seed);
  std::shuffle(indexes.begin(), indexes.end(), random);
  const auto requested = static_cast<std::size_t>(
      std::floor(static_cast<double>(indexes.size()) * train_percentage));
  // The source utility includes one trace even for a zero percentage whenever
  // the input is non-empty.  Cap the result so a percentage of one remains a
  // valid partition.
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
    Trace cumulative;
    cumulative.case_id = source.case_id;
    cumulative.attributes = source.attributes;
    cumulative.properties = source.properties;
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
