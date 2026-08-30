#include "minera/log_to_target.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

std::string event_activity(const Event& event, const std::string& key) {
  if (key == "activity" || key == "concept:name") return event.activity;
  const auto* value = event.attribute(key);
  return value == nullptr ? std::string{} : *value;
}

const std::string* event_timestamp(const Event& event, const std::string& key) {
  if (key == "time:timestamp" || key == "timestamp") return &event.timestamp;
  return event.attribute(key);
}

bool is_known_variant(LogToTargetVariant variant) {
  return variant == LogToTargetVariant::NEXT_ACTIVITY ||
         variant == LogToTargetVariant::NEXT_TIME ||
         variant == LogToTargetVariant::REMAINING_TIME;
}

}  // namespace

LogToTargetResult log_to_target(const EventLog& log,
                                const LogToTargetOptions& options) {
  if (!is_known_variant(options.variant)) {
    throw std::invalid_argument("log_to_target: unknown variant");
  }
  if (options.variant == LogToTargetVariant::NEXT_ACTIVITY &&
      options.activity_key.empty()) {
    throw std::invalid_argument("log_to_target: activity_key must not be empty");
  }
  if (options.variant != LogToTargetVariant::NEXT_ACTIVITY &&
      options.timestamp_key.empty()) {
    throw std::invalid_argument("log_to_target: timestamp_key must not be empty");
  }
  if (!options.enable_padding && options.pad_size != 0) {
    throw std::invalid_argument(
        "log_to_target: pad_size requires enable_padding");
  }
  for (const auto& activity : options.activities) {
    if (activity.empty()) {
      throw std::invalid_argument(
          "log_to_target: explicit activities must not be empty");
    }
  }
  if (std::set<std::string>(options.activities.begin(), options.activities.end())
          .size() != options.activities.size()) {
    throw std::invalid_argument(
        "log_to_target: explicit activities must be unique");
  }
  if (options.variant != LogToTargetVariant::NEXT_ACTIVITY &&
      !options.activities.empty()) {
    throw std::invalid_argument(
        "log_to_target: activities apply only to NEXT_ACTIVITY");
  }

  LogToTargetResult result;
  result.variant = options.variant;
  result.trace_count = log.traces.size();
  result.padded = options.enable_padding;
  if (options.variant == LogToTargetVariant::NEXT_ACTIVITY) {
    if (!options.activities.empty()) {
      result.classes = options.activities;
    } else {
      for (const auto& trace : log.traces) {
        for (const auto& event : trace.events) {
          result.classes.push_back(event_activity(event, options.activity_key));
        }
      }
      std::sort(result.classes.begin(), result.classes.end());
      result.classes.erase(
          std::unique(result.classes.begin(), result.classes.end()),
          result.classes.end());
    }
  }
  result.target_width = options.variant == LogToTargetVariant::NEXT_ACTIVITY
                            ? result.classes.size()
                            : 1;
  if (log.traces.empty()) return result;

  std::unordered_map<std::string, std::size_t> class_index;
  class_index.reserve(result.classes.size());
  for (std::size_t i = 0; i < result.classes.size(); ++i) {
    class_index.emplace(result.classes[i], i);
  }

  std::size_t max_trace_length = 0;
  for (const auto& trace : log.traces) {
    max_trace_length = std::max(max_trace_length, trace.events.size());
  }
  if (options.enable_padding && options.pad_size < max_trace_length) {
    throw std::invalid_argument(
        "log_to_target: pad_size must cover the longest trace");
  }

  result.rows.reserve(log.traces.size());
  for (const auto& trace : log.traces) {
    const auto& events = trace.events;
    std::vector<std::vector<double>> rows;
    rows.reserve(options.enable_padding ? options.pad_size : events.size());

    std::vector<double> parsed_times;
    if (options.variant != LogToTargetVariant::NEXT_ACTIVITY) {
      parsed_times.reserve(events.size());
      for (const auto& event : events) {
        const auto* text = event_timestamp(event, options.timestamp_key);
        const auto instant = text == nullptr ? std::optional<double>{}
                                             : parse_timestamp(*text);
        if (!instant || !std::isfinite(*instant)) {
          throw std::invalid_argument(
              "log_to_target: temporal variant requires valid timestamps");
        }
        parsed_times.push_back(*instant);
      }
    }

    for (std::size_t i = 0; i < events.size(); ++i) {
      if (options.variant == LogToTargetVariant::NEXT_ACTIVITY) {
        std::vector<double> row(result.classes.size(), 0.0);
        if (i + 1 < events.size()) {
          const auto next = class_index.find(
              event_activity(events[i + 1], options.activity_key));
          if (next != class_index.end()) row[next->second] = 1.0;
        }
        rows.push_back(std::move(row));
        continue;
      }

      double value = 0.0;
      if (i + 1 < events.size()) {
        if (options.variant == LogToTargetVariant::NEXT_TIME) {
          value = parsed_times[i + 1] - parsed_times[i];
        } else {
          value = parsed_times.back() - parsed_times[i];
        }
      }
      if (!std::isfinite(value) || value < 0.0) {
        throw std::invalid_argument(
            "log_to_target: temporal target must be finite and nonnegative");
      }
      rows.push_back({value});
    }
    if (options.enable_padding) {
      while (rows.size() < options.pad_size) {
        rows.push_back(options.variant == LogToTargetVariant::NEXT_ACTIVITY
                           ? std::vector<double>(result.classes.size(), 0.0)
                           : std::vector<double>{0.0});
      }
    }
    result.rows.push_back(std::move(rows));
  }
  return result;
}

}  // namespace minera
