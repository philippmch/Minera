#pragma once

// pm4py's discover_temporal_profile on a DataFrame: the reference path of a
// CSV or XES upload (round BH, observed black-box). Each case's rows (the
// cases by sorted id, each case's rows stably sorted by timestamp in the
// frame's row order) pair every row with each later row of its case. A
// duration is the Timedelta's days * 86400 + seconds + microseconds * 1e-6 +
// nanoseconds * 1e-9; a pair's mean is Kahan's sum over the count (pandas'
// groupby mean) and its deviation Welford's sample deviation (groupby std, 0
// for a single pair). A missing timestamp raises pm4py's error.

#include <functional>
#include <optional>
#include <string>

#include "minera/event_log.hpp"
#include "minera/temporal_profile.hpp"

namespace minera {

[[nodiscard]] TemporalProfile discover_temporal_profile_frame(
    const EventLog& log, const std::function<bool()>& cancelled = {});

// pm4py's discover_temporal_profile(frame, activity_key, timestamp_key) on
// other columns of the frame, the timestamp column as both completion and
// start (round BV); nullopt where the frame's cells do not allow it (a
// missing or non-date timestamp cell, activities that do not sort).
[[nodiscard]] std::optional<TemporalProfile> discover_temporal_profile_frame(
    const EventLog& log, const std::string& activity_key, const std::string& timestamp_key,
    const std::function<bool()>& cancelled = {});

}  // namespace minera
