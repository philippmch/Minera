#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "minera/event_data.hpp"
#include "minera/event_log.hpp"

namespace minera {

// One weekly business-hour interval, expressed as seconds since Monday
// 00:00 (the end is exclusive).  Intervals may be supplied in any order;
// discovery normalizes overlapping and adjacent intervals.
struct BusinessHourSlot {
  double start_seconds = 0.0;
  double end_seconds = 0.0;
};

struct TemporalProfileEntry {
  std::string source;
  std::string target;
  std::uint64_t count = 0;
  double mean_seconds = 0.0;
  // Population-free sample standard deviation. It is zero for one sample.
  double standard_deviation_seconds = 0.0;

  // Convenient short spelling for callers that use the ``stddev`` term.
  [[nodiscard]] double stddev() const noexcept {
    return standard_deviation_seconds;
  }
};

struct TemporalProfile {
  // Entries are sorted lexicographically by source, then target.
  std::vector<TemporalProfileEntry> entries;

  [[nodiscard]] const TemporalProfileEntry* find(const std::string& source,
                                                  const std::string& target) const noexcept;
};

struct TemporalProfileOptions {
  // Activity and case selectors used by flat/table adapters. Event-log
  // discovery uses trace boundaries and therefore ignores case_id_attribute.
  std::string activity_attribute = "concept:name";
  std::string case_id_attribute = "case:concept:name";

  // Empty means Event.timestamp. The aliases allow callers to describe the
  // Source-completion/target-start terminology is explicit. When both a
  // primary field and its alias are set, the primary field takes precedence.
  std::string source_timestamp_attribute;
  std::string target_timestamp_attribute;
  std::string source_completion_timestamp_attribute;
  std::string target_start_timestamp_attribute;

  bool use_business_hours = false;
  // Empty uses the compatible Monday-Friday 07:00-17:00 schedule unless
  // business_hour_slots_explicit is true.
  std::vector<BusinessHourSlot> business_hour_slots;
  // Distinguishes an omitted schedule (use the default) from an explicitly
  // empty schedule (no active business time).
  bool business_hour_slots_explicit = false;

  // The reference adapters fail when a selected activity or timestamp is
  // missing/malformed. Callers that intentionally process sparse data may opt
  // into the older skip behavior.
  bool strict_attributes = true;

  // Used by conformance checking; discovery ignores this value.
  double zeta = 6.0;
};

struct TemporalDeviation {
  std::string case_id;
  std::string source;
  std::string target;
  double elapsed_seconds = 0.0;
  double standard_deviation_distance = 0.0;
  // Conformance of a zero-variance pair (batch or streaming) reports the
  // exact integer sys.maxsize, which a double cannot hold; the distance then
  // carries 2**63.
  bool zero_variance_sentinel = false;

  [[nodiscard]] double distance() const noexcept {
    return standard_deviation_distance;
  }
};

// Discover all ordered (i,j), i<j, activity pairs in every trace. Negative
// durations are ignored. Missing or malformed selected fields raise by
// default; strict_attributes=false restores sparse-input skipping.
[[nodiscard]] TemporalProfile discover_temporal_profile(
    const EventLog& log, const TemporalProfileOptions& options = {});
// The same discovery; `cancelled`, when set, is polled about every million
// event pairs and throws OperationCancelled (minera/cancellation.hpp) once it
// returns true. The pairs of one trace grow with the square of its length,
// so a single very long case can take minutes.
[[nodiscard]] TemporalProfile discover_temporal_profile(
    const EventLog& log, const TemporalProfileOptions& options,
    const std::function<bool()>& cancelled);

// The discovery of a CSV or XES upload, which reaches pm4py as a DataFrame:
// with the default selectors (concept:name, the event timestamp, no business
// hours) and a timestamp on every row it follows pm4py's DataFrame temporal
// profile (each case's rows stably sorted by timestamp, Timedelta durations,
// pandas' compensated mean and Welford's deviation; rounds BH and BI,
// observed black-box). Other options, a log that is not a frame, or a row
// without a timestamp (where pm4py raises) take discover_temporal_profile.
[[nodiscard]] TemporalProfile discover_temporal_profile_as_uploaded(
    const EventLog& log, const TemporalProfileOptions& options,
    const std::function<bool()>& cancelled = {});

// Input-specific components retained as distinct contracts. The table path
// groups by case_id_attribute and stable-sorts each case by completion time.
[[nodiscard]] TemporalProfile discover_temporal_profile_log(
    const EventLog& log, const TemporalProfileOptions& options = {});
[[nodiscard]] TemporalProfile discover_temporal_profile_dataframe(
    const EventTable& table, const TemporalProfileOptions& options = {});
[[nodiscard]] TemporalProfile discover_temporal_profile(
    const EventStream& stream, const TemporalProfileOptions& options = {});
[[nodiscard]] TemporalProfile discover_temporal_profile(
    const EventTable& table, const TemporalProfileOptions& options = {});
[[nodiscard]] TemporalProfile discover_temporal_profile(
    const LazyEventTable& table, const TemporalProfileOptions& options = {});
[[nodiscard]] TemporalProfile discover_temporal_profile(
    const EventDataView& data, const TemporalProfileOptions& options = {});

// Return only observations whose absolute z-score exceeds options.zeta, or
// whose zero-variance profile disagrees with the observed duration.
[[nodiscard]] std::vector<TemporalDeviation> check_temporal_profile_conformance(
    const EventLog& log, const TemporalProfile& profile,
    const TemporalProfileOptions& options = {});

using TemporalConformanceResult = std::vector<std::vector<TemporalDeviation>>;

// Detailed conformance preserves one result bucket per input case, including
// empty buckets. The legacy check function below remains a flattened view.
[[nodiscard]] TemporalConformanceResult conform_temporal_profile_log(
    const EventLog& log, const TemporalProfile& profile,
    const TemporalProfileOptions& options = {});
[[nodiscard]] TemporalConformanceResult conform_temporal_profile_dataframe(
    const EventTable& table, const TemporalProfile& profile,
    const TemporalProfileOptions& options = {});
[[nodiscard]] TemporalConformanceResult conform_temporal_profile(
    const EventDataView& data, const TemporalProfile& profile,
    const TemporalProfileOptions& options = {});

// Convenience overload when only the zeta threshold differs from defaults.
[[nodiscard]] inline std::vector<TemporalDeviation>
check_temporal_profile_conformance(const EventLog& log,
                                   const TemporalProfile& profile,
                                   double zeta) {
  TemporalProfileOptions options;
  options.zeta = zeta;
  return check_temporal_profile_conformance(log, profile, options);
}

// Readable spelling retained as an alias.
[[nodiscard]] inline std::vector<TemporalDeviation> conformance_temporal_profile(
    const EventLog& log, const TemporalProfile& profile,
    const TemporalProfileOptions& options = {}) {
  return check_temporal_profile_conformance(log, profile, options);
}

}  // namespace minera
