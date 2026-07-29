#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

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
  // Empty means Event.timestamp. The aliases allow callers to describe the
  // Source-completion/target-start terminology is explicit. When both a
  // primary field and its alias are set, the primary field takes precedence.
  std::string source_timestamp_attribute;
  std::string target_timestamp_attribute;
  std::string source_completion_timestamp_attribute;
  std::string target_start_timestamp_attribute;

  bool use_business_hours = false;
  // Empty means the compatible Monday-Friday 07:00-17:00 schedule.
  std::vector<BusinessHourSlot> business_hour_slots;

  // Used by conformance checking; discovery ignores this value.
  double zeta = 6.0;
};

struct TemporalDeviation {
  std::string case_id;
  std::string source;
  std::string target;
  double elapsed_seconds = 0.0;
  double standard_deviation_distance = 0.0;

  [[nodiscard]] double distance() const noexcept {
    return standard_deviation_distance;
  }
};

// Discover all ordered (i,j), i<j, activity pairs in every trace. Invalid,
// missing, or negative durations are ignored.
[[nodiscard]] TemporalProfile discover_temporal_profile(
    const EventLog& log, const TemporalProfileOptions& options = {});

// Return only observations whose absolute z-score exceeds options.zeta, or
// whose zero-variance profile disagrees with the observed duration.
[[nodiscard]] std::vector<TemporalDeviation> check_temporal_profile_conformance(
    const EventLog& log, const TemporalProfile& profile,
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
