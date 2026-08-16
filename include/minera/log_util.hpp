#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// Utility projections used when preparing event logs for prediction and
// supervised-learning workflows.  The projections copy the input records;
// callers can therefore mutate the returned logs without changing the source.
enum class LogUtilityVariant {
  kSplitTrainTest,
  kFixedPrefixes,
  kAllPrefixes,
  kUntilActivity,
  kToActivities,
  kFeatureSelection,

  SPLIT_TRAIN_TEST = kSplitTrainTest,
  FIXED_PREFIXES = kFixedPrefixes,
  ALL_PREFIXES = kAllPrefixes,
  UNTIL_ACTIVITY = kUntilActivity,
  TO_ACTIVITIES = kToActivities,
  FEATURE_SELECTION = kFeatureSelection,
};

struct LogUtilityOptions {
  LogUtilityVariant variant = LogUtilityVariant::kSplitTrainTest;
  // The split uses a seeded Fisher-Yates shuffle.  The reference utility does
  // not expose a seed; making it explicit keeps the native result reproducible.
  double train_percentage = 0.8;
  std::uint64_t seed = 0;
  std::size_t prefix_length = 0;
  std::string activity;
  std::string activity_key = "activity";
  std::string timestamp_key = "timestamp";
};

struct LogTrainTestSplit {
  EventLog training;
  EventLog testing;
};

struct LogPrefixResult {
  EventLog log;
  // For ALL_PREFIXES, one row per input trace.  Each row contains the index
  // of that trace's final prefix in `log`; empty traces have an empty row.
  std::vector<std::vector<std::size_t>> change_indexes;
};

struct LogUntilActivityResult {
  EventLog log;
  // Elapsed seconds from the event immediately before the target to the first
  // target event, aligned with the returned traces.
  std::vector<double> elapsed_seconds;
};

struct LogActivityTargetResult {
  // Only activities that produced at least one non-empty sublog are included;
  // order follows the caller's requested activity list.
  std::vector<std::string> activities;
  std::vector<EventLog> logs;
};

struct LogFeatureSelectionOptions {
  std::size_t min_different_values = 5;
  std::size_t max_different_values = 50;
  bool consider_all_attributes = true;
  std::vector<std::string> mandatory_attributes;
};

struct LogFeatureSelectionResult {
  std::vector<std::string> event_attributes;
  std::vector<std::string> case_attributes;
};

[[nodiscard]] LogTrainTestSplit split_log_train_test(
    const EventLog& log, double train_percentage = 0.8,
    std::uint64_t seed = 0);

[[nodiscard]] EventLog get_log_prefixes(const EventLog& log,
                                         std::size_t length);

[[nodiscard]] LogPrefixResult get_log_with_prefixes(const EventLog& log);

[[nodiscard]] LogUntilActivityResult get_log_traces_until_activity(
    const EventLog& log, const std::string& activity,
    const LogUtilityOptions& options = {});

[[nodiscard]] LogActivityTargetResult get_log_traces_to_activities(
    const EventLog& log, const std::vector<std::string>& activities,
    const LogUtilityOptions& options = {});

[[nodiscard]] LogFeatureSelectionResult select_log_features(
    const EventLog& log, const LogFeatureSelectionOptions& options = {});

[[nodiscard]] inline LogTrainTestSplit split_train_test(
    const EventLog& log, double train_percentage = 0.8,
    std::uint64_t seed = 0) {
  return split_log_train_test(log, train_percentage, seed);
}

[[nodiscard]] inline EventLog get_prefixes_from_log(const EventLog& log,
                                                    std::size_t length) {
  return get_log_prefixes(log, length);
}

}  // namespace minera
