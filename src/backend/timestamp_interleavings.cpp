#include "minera/timestamp_interleavings.hpp"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>

#include "minera/timestamp.hpp"
#include "nanosecond_instant.hpp"

namespace minera {
namespace {

using nanosecond_instant::elapsed_nanoseconds;
using nanosecond_instant::Instant;
using nanosecond_instant::kEndTime;
using nanosecond_instant::kNanosPerSecond;

constexpr std::string_view kStartActivity = "▶";
constexpr std::string_view kEndActivity = "■";
// The artificial boundaries sit at the extreme representable nanosecond
// timestamps, so elapsed times against them are finite and exact.
constexpr std::string_view kStartTimestamp = "1677-09-21T00:12:43.145224193Z";
constexpr std::string_view kEndTimestamp = "2262-04-11T23:47:16.854775807Z";
// The start boundary is INT64_MIN + 1 nanoseconds (INT64_MIN itself is
// pandas' NaT), so a row from it to the epoch is exactly INT64_MAX long.
constexpr Instant kStartBoundary{-9'223'372'037, 145'224'193};

// The reference's elapsed seconds: whole seconds, plus whole microseconds
// times 1e-6, plus the remaining nanoseconds times 1e-9, each step rounded
// to double (not one correctly rounded division).
double elapsed_seconds(std::int64_t nanoseconds) {
  std::int64_t seconds = nanoseconds / kNanosPerSecond;
  std::int64_t fraction = nanoseconds % kNanosPerSecond;
  if (fraction < 0) { fraction += kNanosPerSecond; --seconds; }
  const double whole = static_cast<double>(seconds);
  const double micros = static_cast<double>(fraction / 1000) * 1e-6;
  const double nanos = static_cast<double>(fraction % 1000) * 1e-9;
  return (whole + micros) + nanos;
}
struct BoundaryEvent {
  const Event* event = nullptr;
  std::string activity;
  std::string timestamp;
  Instant time;
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
  Instant time;
  std::size_t sequence = 0;
};

// Each case becomes start boundary, events by (timestamp, input order), end
// boundary. Indices number the whole log: the start boundaries of the cases
// in case-id order, then every case's events in that order, then the end
// boundaries.
std::map<std::string, Timeline> build_timelines(const EventLog& log,
                                                 std::string_view key) {
  std::map<std::string, std::vector<ParsedEvent>> grouped;
  std::size_t sequence = 0;
  for (const auto& trace : log.traces) {
    auto& events = grouped[trace.case_id];
    for (const auto& event : trace.events) {
      const auto* raw_timestamp = timestamp_text(event, key);
      if (raw_timestamp == nullptr || raw_timestamp->empty()) {
        throw std::invalid_argument(
            "timestamp interleavings: missing timestamp '" + std::string(key) +
            "'");
      }
      const auto parsed = parse_timestamp_exact(*raw_timestamp);
      if (!parsed) {
        throw std::invalid_argument(
            "timestamp interleavings: invalid timestamp '" +
            *raw_timestamp + "'");
      }
      // Timezone-naive columns cannot be ordered against the boundaries.
      if (!parsed->timezone_aware) {
        throw std::invalid_argument(
            "timestamp interleavings: timezone-naive timestamp '" +
            *raw_timestamp + "'");
      }
      events.push_back({&event,
                        Instant{parsed->seconds, static_cast<std::int64_t>(parsed->nanoseconds)},
                        sequence++});
    }
  }
  std::erase_if(grouped, [](const auto& entry) { return entry.second.empty(); });

  std::map<std::string, Timeline> timelines;
  std::size_t total = 0;
  for (const auto& [_, events] : grouped) total += events.size();
  const std::size_t cases = grouped.size();
  std::size_t rank = 0, position = cases;
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
    timeline.push_back({nullptr, std::string(kStartActivity),
                        std::string(kStartTimestamp), kStartBoundary, rank, true});
    for (const auto& parsed : events) {
      timeline.push_back({parsed.event, parsed.event->activity,
                          *timestamp_text(*parsed.event, key), parsed.time,
                          position++, false});
    }
    timeline.push_back({nullptr, std::string(kEndActivity),
                        std::string(kEndTimestamp), kEndTime,
                        cases + total + rank, true});
    timelines.emplace(case_id, std::move(timeline));
    ++rank;
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
  // Rows are ordered by the indices of the two pairs' first events.
  std::vector<std::pair<std::size_t, std::size_t>> order;
  const auto append = [&](const BoundaryEvent& source,
                          const BoundaryEvent& target, std::size_t left_index,
                          std::size_t right_index,
                          TimestampInterleavingDirection direction,
                          std::size_t left_pair, std::size_t right_pair) {
    if (result.interleavings.size() >= options.max_interleavings) {
      throw std::invalid_argument(
          "timestamp interleavings: max_interleavings exceeded");
    }
    // Elapsed times are nanosecond counts; one from the start boundary to a
    // real event does not fit and fails the whole discovery.
    const auto elapsed = elapsed_nanoseconds(source.time, target.time);
    if (!elapsed)
      throw std::overflow_error(
          "timestamp interleavings: elapsed time overflows nanoseconds");
    const double difference = elapsed_seconds(*elapsed);
    result.interleavings.push_back(
        {source.activity, target.activity, source.timestamp, target.timestamp,
         left_index, right_index, direction, difference});
    order.emplace_back(left_pair, right_pair);
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
                 TimestampInterleavingDirection::kLeftToRight, lhs[i].index,
                 rhs[j].index);
        }
        if (rhs[j].time < lhs[i].time && lhs[i].time < rhs[j + 1].time &&
            rhs[j + 1].time < lhs[i + 1].time) {
          append(rhs[j + 1], lhs[i + 1], lhs[i + 1].index, rhs[j + 1].index,
                 TimestampInterleavingDirection::kRightToLeft, lhs[i].index,
                 rhs[j].index);
        }
      }
    }
  }

  std::vector<std::size_t> rows(result.interleavings.size());
  for (std::size_t i = 0; i < rows.size(); ++i) rows[i] = i;
  std::stable_sort(rows.begin(), rows.end(), [&](std::size_t lhs, std::size_t rhs) {
    return order[lhs] < order[rhs];
  });
  std::vector<TimestampInterleaving> sorted;
  sorted.reserve(rows.size());
  for (const auto row : rows) sorted.push_back(std::move(result.interleavings[row]));
  result.interleavings = std::move(sorted);
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
