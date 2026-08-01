#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// The target to emit for each event in a trace.
enum class LogToTargetVariant {
  NEXT_ACTIVITY,
  NEXT_TIME,
  REMAINING_TIME,

  // Style aliases used by the other native APIs.
  kNextActivity = NEXT_ACTIVITY,
  kNextTime = NEXT_TIME,
  kRemainingTime = REMAINING_TIME,
};

struct LogToTargetOptions {
  LogToTargetVariant variant = LogToTargetVariant::NEXT_ACTIVITY;
  std::string activity_key = "activity";
  std::string timestamp_key = "time:timestamp";
  bool enable_padding = false;
  std::size_t pad_size = 0;
  // An empty list derives sorted unique classes from the log.  Otherwise the
  // supplied order is retained and activities not present in it are encoded
  // as an all-zero row.
  std::vector<std::string> activities;
};

struct LogToTargetResult {
  LogToTargetVariant variant = LogToTargetVariant::NEXT_ACTIVITY;
  // Outer order is trace order; inner order is event order followed by any
  // requested zero-padding rows.
  std::vector<std::vector<std::vector<double>>> rows;
  std::vector<std::string> classes;
};

[[nodiscard]] LogToTargetResult log_to_target(
    const EventLog& log, const LogToTargetOptions& options = {});

// Readable aliases for callers that use "transform" terminology.
[[nodiscard]] inline LogToTargetResult transform_log_to_target(
    const EventLog& log, const LogToTargetOptions& options = {}) {
  return log_to_target(log, options);
}

using TargetTransformationVariant = LogToTargetVariant;
using TargetTransformationOptions = LogToTargetOptions;
using TargetTransformationResult = LogToTargetResult;
using TargetVariant = LogToTargetVariant;
using TargetOptions = LogToTargetOptions;
using TargetResult = LogToTargetResult;

}  // namespace minera
