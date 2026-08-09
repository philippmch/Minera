#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// Whether an anchor activity starts the new subcase itself or whether the
// event following the anchor starts it.
enum class ActivitySplitCutMode {
  kThis,
  kNext,
};

struct ActivitySplitOptions {
  // One or more activities at which each trace is split. Matching any listed
  // activity starts another subcase.
  std::vector<std::string> activities;
  // The separator between the original case id and the zero-based subcase
  // number.
  std::string subcase_separator = "##@@";
  ActivitySplitCutMode cut_mode = ActivitySplitCutMode::kThis;
};

// Splits every non-empty trace into subcases. Even a trace with no matching
// activity receives the zero suffix, matching the tabular case-id contract.
// Event and trace attributes are copied, and each emitted event's case id is
// synchronized with its new trace id.
[[nodiscard]] EventLog split_activity(
    const EventLog& event_log,
    const ActivitySplitOptions& options = {});

[[nodiscard]] EventLog filter_activity_split(
    const EventLog& event_log,
    const ActivitySplitOptions& options = {});

[[nodiscard]] EventLog split_event_log_on_activity(
    const EventLog& event_log,
    const ActivitySplitOptions& options = {});

[[nodiscard]] ActivitySplitOptions make_activity_split_options(
    std::vector<std::string> activities,
    ActivitySplitCutMode cut_mode = ActivitySplitCutMode::kThis,
    std::string_view subcase_separator = "##@@");

}  // namespace minera
