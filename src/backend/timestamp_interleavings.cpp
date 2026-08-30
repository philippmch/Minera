#include "minera/timestamp_interleavings.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

constexpr std::string_view kStartActivity = "▶";
constexpr std::string_view kEndActivity = "■";
constexpr std::string_view kStartTimestamp = "0001-01-01T00:00:00Z";
constexpr std::string_view kEndTimestamp = "9999-12-31T23:59:59Z";
constexpr double kBoundaryMagnitude = 1.0e15;

struct BoundaryEvent {
  const Event* event = nullptr;
  std::string activity;
  std::string timestamp;
  double time = 0.0;
  std::size_t index = 0;
  bool artificial = false;
};

using Timeline = std::vector<BoundaryEvent>;

const std::string* timestamp_text(const Event& event,
                                  std::string_view key) {
  if (key == "timestamp" || key == "time:timestamp") return &event.timestamp;
  return event.attribute(key);
}

struct ParsedEvent {
  const Event* event = nullptr;
  double time = 0.0;
  std::size_t sequence = 0;
};

std::map<std::string, Timeline> build_timelines(const EventLog& log,
                                                 std::string_view key) {
  std::map<std::string, std::vector<ParsedEvent>> grouped;
  std::size_t sequence = 0;
  std::size_t maximum_index = 0;
  bool has_event = false;
  for (const auto& trace : log.traces) {
    auto& events = grouped[trace.case_id];
    for (const auto& event : trace.events) {
      const auto* raw_timestamp = timestamp_text(event, key);
      if (raw_timestamp == nullptr || raw_timestamp->empty()) {
        throw std::invalid_argument(
            "timestamp interleavings: missing timestamp '" + std::string(key) +
            "'");
      }
      const auto parsed = parse_timestamp(*raw_timestamp);
      if (!parsed || !std::isfinite(*parsed)) {
        throw std::invalid_argument(
            "timestamp interleavings: invalid timestamp '" +
            *raw_timestamp + "'");
      }
      events.push_back({&event, *parsed, sequence++});
      maximum_index = std::max(maximum_index, event.input_order);
      has_event = true;
    }
  }

  std::map<std::string, Timeline> timelines;
  if (has_event && maximum_index == std::numeric_limits<std::size_t>::max()) {
    throw std::invalid_argument(
        "timestamp interleavings: event index is too large");
  }
  std::size_t next_synthetic_index = has_event ? maximum_index + 1 : 0;
  for (auto& [case_id, events] : grouped) {
    std::stable_sort(events.begin(), events.end(),
                     [](const ParsedEvent& left, const ParsedEvent& right) {
                       if (left.time != right.time) return left.time < right.time;
                       if (left.event->input_order != right.event->input_order)
                         return left.event->input_order < right.event->input_order;
                       return left.sequence < right.sequence;
                     });
    Timeline timeline;
    timeline.reserve(events.size() + 2);
    const std::size_t start_index = next_synthetic_index++;
    const std::size_t end_index = next_synthetic_index++;
    timeline.push_back({nullptr, std::string(kStartActivity),
                        std::string(kStartTimestamp), -kBoundaryMagnitude,
                        start_index, true});
    for (const auto& parsed : events) {
      timeline.push_back({parsed.event, parsed.event->activity,
                          *timestamp_text(*parsed.event, key), parsed.time,
                          parsed.event->input_order, false});
    }
    timeline.push_back({nullptr, std::string(kEndActivity),
                        std::string(kEndTimestamp), kBoundaryMagnitude,
                        end_index, true});
    timelines.emplace(case_id, std::move(timeline));
  }
  return timelines;
}

void write_json_string(std::ostringstream& out, std::string_view value) {
  out << '"';
  for (const unsigned char c : value) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (c < 0x20) {
          out << "\\u00" << "0123456789abcdef"[(c >> 4) & 15]
              << "0123456789abcdef"[c & 15];
        } else {
          out << static_cast<char>(c);
        }
    }
  }
  out << '"';
}

const char* direction_name(TimestampInterleavingDirection direction) {
  return direction == TimestampInterleavingDirection::kLeftToRight ? "LR"
                                                                      : "RL";
}

}  // namespace

TimestampInterleavingsResult discover_timestamp_interleavings(
    const EventLog& left, const EventLog& right,
    const std::vector<CaseRelation>& relations,
    const TimestampInterleavingOptions& options) {
  if (options.timestamp_key.empty()) {
    throw std::invalid_argument(
        "timestamp interleavings: timestamp key must not be empty");
  }
  if (options.max_interleavings == 0) {
    throw std::invalid_argument(
        "timestamp interleavings: max_interleavings must be positive");
  }

  const auto left_timelines = build_timelines(left, options.timestamp_key);
  const auto right_timelines = build_timelines(right, options.timestamp_key);

  TimestampInterleavingsResult result;
  result.options = options;
  const auto append = [&](const BoundaryEvent& source,
                          const BoundaryEvent& target, std::size_t left_index,
                          std::size_t right_index,
                          TimestampInterleavingDirection direction) {
    if (result.interleavings.size() >= options.max_interleavings) {
      throw std::invalid_argument(
          "timestamp interleavings: max_interleavings exceeded");
    }
    const double difference = target.time - source.time;
    if (!std::isfinite(difference)) {
      throw std::invalid_argument(
          "timestamp interleavings: timestamp difference is not finite");
    }
    result.interleavings.push_back(
        {source.activity, target.activity, source.timestamp, target.timestamp,
         left_index, right_index, direction, difference});
  };

  for (const auto& relation : relations) {
    if (relation.left_case_id.empty() || relation.right_case_id.empty()) {
      throw std::invalid_argument(
          "timestamp interleavings: relation case ids must not be empty");
    }
    const auto left_it = left_timelines.find(relation.left_case_id);
    const auto right_it = right_timelines.find(relation.right_case_id);
    // A relation can name a case that was filtered out of one side.  The
    // dataframe contract is an inner join, so such a relation contributes no
    // rows rather than manufacturing an empty trace.
    if (left_it == left_timelines.end() || right_it == right_timelines.end())
      continue;
    const auto& lhs = left_it->second;
    const auto& rhs = right_it->second;
    for (std::size_t i = 0; i + 1 < lhs.size(); ++i) {
      for (std::size_t j = 0; j + 1 < rhs.size(); ++j) {
        if (lhs[i].time < rhs[j].time && rhs[j].time < lhs[i + 1].time &&
            lhs[i + 1].time < rhs[j + 1].time) {
          append(lhs[i], rhs[j], lhs[i].index, rhs[j].index,
                 TimestampInterleavingDirection::kLeftToRight);
        }
        if (rhs[j].time < lhs[i].time && lhs[i].time < rhs[j + 1].time &&
            rhs[j + 1].time < lhs[i + 1].time) {
          append(rhs[j + 1], lhs[i + 1], lhs[i + 1].index, rhs[j + 1].index,
                 TimestampInterleavingDirection::kRightToLeft);
        }
      }
    }
  }

  std::stable_sort(
      result.interleavings.begin(), result.interleavings.end(),
      [](const TimestampInterleaving& left_row,
         const TimestampInterleaving& right_row) {
        return std::tie(left_row.left_index, left_row.right_index,
                        left_row.direction) <
               std::tie(right_row.left_index, right_row.right_index,
                        right_row.direction);
      });
  return result;
}

std::string timestamp_interleavings_json(
    const TimestampInterleavingsResult& result) {
  if (result.options.timestamp_key.empty() ||
      result.options.max_interleavings == 0 ||
      result.interleavings.size() > result.options.max_interleavings)
    throw std::invalid_argument("timestamp interleavings result has invalid bounds");
  for (const auto& row : result.interleavings) {
    if (row.source_activity.empty() || row.target_activity.empty() ||
        row.source_timestamp.empty() || row.target_timestamp.empty() ||
        !std::isfinite(row.timestamp_diff_seconds) ||
        (row.direction != TimestampInterleavingDirection::kLeftToRight &&
         row.direction != TimestampInterleavingDirection::kRightToLeft))
      throw std::invalid_argument("timestamp interleavings result contains an invalid row");
  }
  std::ostringstream out;
  out << std::setprecision(17);
  out << "{\"family\":\"TIMESTAMP_INTERLEAVINGS\",\"variant\":\"TIMESTAMP\","
         "\"timestamp_key\":";
  write_json_string(out, result.options.timestamp_key);
  out << ",\"max_interleavings\":" << result.options.max_interleavings
      << ",\"interleavings\":[";
  for (std::size_t i = 0; i < result.interleavings.size(); ++i) {
    if (i != 0) out << ',';
    const auto& row = result.interleavings[i];
    out << "{\"source_activity\":";
    write_json_string(out, row.source_activity);
    out << ",\"target_activity\":";
    write_json_string(out, row.target_activity);
    out << ",\"source_timestamp\":";
    write_json_string(out, row.source_timestamp);
    out << ",\"target_timestamp\":";
    write_json_string(out, row.target_timestamp);
    out << ",\"left_index\":" << row.left_index
        << ",\"right_index\":" << row.right_index
        << ",\"direction\":";
    write_json_string(out, direction_name(row.direction));
    out << ",\"timestamp_diff\":" << row.timestamp_diff_seconds << '}';
  }
  out << "]}";
  return out.str();
}

}  // namespace minera
