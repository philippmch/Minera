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
  // As the reference's ENABLE_PADDING and PAD_SIZE (round CU, observed
  // black-box): with padding, each trace shorter than the pad size gets zero
  // rows up to it and longer traces stay as they are; without, pad_size is
  // ignored. The pad size defaults to the longest trace: pad_size 0 means
  // that default unless explicit_pad_size says 0 was given.
  bool enable_padding = false;
  std::size_t pad_size = 0;
  bool explicit_pad_size = false;
  // The classes of NEXT_ACTIVITY (ignored by the time targets). Empty and
  // not explicit_activities: the log's sorted distinct activities. Otherwise
  // the list as given, repeats and blanks included: a next activity takes the
  // column of the first equal class, an activity outside the list a zero row.
  std::vector<std::string> activities;
  bool explicit_activities = false;
};

struct LogToTargetResult {
  LogToTargetVariant variant = LogToTargetVariant::NEXT_ACTIVITY;
  // Outer order is trace order; inner order is event order followed by any
  // requested zero-padding rows.
  std::vector<std::vector<std::vector<double>>> rows;
  std::vector<std::string> classes;
  // Stable dimensions make adapter consumers independent of empty-log and
  // empty-trace corner cases when constructing ML tensors.
  std::size_t trace_count = 0;
  std::size_t target_width = 0;
  bool padded = false;
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
