#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "minera/case_relations.hpp"
#include "minera/event_log.hpp"

namespace minera {

// Settings for the timestamp-crossing discovery over two related logs.
// Timestamps are read from the event's first-class field for the standard
// names, or from a generic event attribute otherwise.
struct TimestampInterleavingOptions {
  std::string timestamp_key = "time:timestamp";
  std::size_t max_interleavings = 1'000'000;
};

enum class TimestampInterleavingDirection { kLeftToRight, kRightToLeft };

struct TimestampInterleaving {
  std::string source_activity;
  std::string target_activity;
  std::string source_timestamp;
  std::string target_timestamp;
  std::size_t left_index = 0;
  std::size_t right_index = 0;
  TimestampInterleavingDirection direction =
      TimestampInterleavingDirection::kLeftToRight;
  double timestamp_diff_seconds = 0.0;
};

struct TimestampInterleavingsResult {
  TimestampInterleavingOptions options;
  std::vector<TimestampInterleaving> interleavings;
};

[[nodiscard]] TimestampInterleavingsResult discover_timestamp_interleavings(
    const EventLog& left, const EventLog& right,
    const std::vector<CaseRelation>& relations,
    const TimestampInterleavingOptions& options = {});

[[nodiscard]] std::string timestamp_interleavings_json(
    const TimestampInterleavingsResult& result);

}  // namespace minera
