#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// Classic PM4Py log-skeleton relation pair.  Relations are stored in
// deterministic lexicographic order by the discovery implementation.
struct LogSkeletonPair {
  std::string source;
  std::string target;
  friend bool operator==(const LogSkeletonPair&, const LogSkeletonPair&) = default;
  friend bool operator<(const LogSkeletonPair& left, const LogSkeletonPair& right) {
    return left.source < right.source ||
           (left.source == right.source && left.target < right.target);
  }
};

struct LogSkeletonOptions {
  double noise_threshold = 0.0;
  // Empty means all six classic families.  Names are exactly:
  // equivalence, always_after, always_before, never_together,
  // directly_follows, activ_freq.
  std::vector<std::string> considered_constraints;
};

struct LogSkeletonModel {
  std::vector<LogSkeletonPair> equivalence;
  std::vector<LogSkeletonPair> always_after;
  std::vector<LogSkeletonPair> always_before;
  std::vector<LogSkeletonPair> never_together;
  std::vector<LogSkeletonPair> directly_follows;
  // Allowed per-trace occurrence counts for each activity, sorted ascending.
  std::map<std::string, std::vector<std::size_t>> activ_freq;
};

[[nodiscard]] LogSkeletonModel discover_log_skeleton(
    const EventLog& log, const LogSkeletonOptions& options = {});

struct LogSkeletonDeviation {
  // A relation violation fills `relation`; an activ_freq violation fills
  // `activity`, `observed_frequency`, and (when known) allowed_frequencies.
  // This compact typed representation stands in for PM4Py's heterogeneous
  // tuple/set diagnostics while retaining deterministic ordering.
  std::string constraint;
  LogSkeletonPair relation;
  std::string activity;
  std::size_t observed_frequency = 0;
  std::vector<std::size_t> allowed_frequencies;
  friend bool operator==(const LogSkeletonDeviation&, const LogSkeletonDeviation&) = default;
};

struct LogSkeletonConformance {
  std::vector<LogSkeletonDeviation> deviations;
  std::size_t no_dev_total = 0;
  std::size_t no_constr_total = 0;
  double dev_fitness = 1.0;
  bool is_fit = true;
};

[[nodiscard]] LogSkeletonConformance check_log_skeleton_conformance(
    const std::vector<std::string>& activities, const LogSkeletonModel& model,
    const LogSkeletonOptions& options = {});

[[nodiscard]] LogSkeletonConformance check_log_skeleton_conformance(
    const Trace& trace, const LogSkeletonModel& model,
    const LogSkeletonOptions& options = {});

[[nodiscard]] std::vector<LogSkeletonConformance> check_log_skeleton_conformance(
    const EventLog& log, const LogSkeletonModel& model,
    const LogSkeletonOptions& options = {});

}  // namespace minera
