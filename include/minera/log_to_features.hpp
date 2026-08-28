#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

enum class LogToFeaturesVariant {
  EVENT_BASED,
  TRACE_BASED,
  TEMPORAL,
  kEventBased = EVENT_BASED,
  kTraceBased = TRACE_BASED,
  kTemporal = TEMPORAL,
};

// A deterministic, native projection of a traditional event log.  Attribute
// names are interpreted as event/case attributes; *_string_attributes are
// one-hot encoded and *_numeric_attributes are parsed as finite numbers.
struct LogToFeaturesOptions {
  // The trace projection is the useful general-purpose default; callers that
  // need one row per event can still select EVENT_BASED explicitly.
  LogToFeaturesVariant variant = LogToFeaturesVariant::TRACE_BASED;
  std::string activity_key = "activity";
  std::string timestamp_key = "time:timestamp";
  std::string start_timestamp_key;
  std::string resource_key = "org:resource";
  double window_seconds = 604800.0;
  // Calendar frequency for temporal rows (for example "W", "D", "H",
  // "MS", or "Q").  The default is weekly; an empty value keeps the
  // fixed-width window_seconds projection for compatibility.
  std::string grouper_freq = "W";

  std::vector<std::string> event_string_attributes;
  std::vector<std::string> event_numeric_attributes;
  std::vector<std::string> case_string_attributes;
  std::vector<std::string> case_numeric_attributes;
  // Generic lists are accepted as aliases; their type is read from the first
  // occurrence's AttributeKind and falls back to numeric parsing.
  std::vector<std::string> event_attributes;
  std::vector<std::string> case_attributes;
  // Optional categorical/succession representations used by trace-based
  // extraction.  The successor list is an alias-friendly spelling of the
  // reference STR_EVSUCC_ATTRIBUTES setting.
  std::vector<std::string> event_successor_attributes;
  bool enable_activity_def_representation = true;
  bool enable_succ_def_representation = true;

  // Trace-level extra feature families.  enable_all_extra_features enables
  // all of these switches while preserving explicit feature_names ordering.
  bool enable_all_extra_features = false;
  bool enable_case_duration = false;
  bool add_case_identifier_column = false;
  bool enable_times_from_first_occurrence = false;
  bool enable_times_from_last_occurrence = false;
  bool enable_direct_paths_times_last_occ = false;
  bool enable_indirect_paths_times_last_occ = false;
  bool enable_work_in_progress = false;
  bool enable_resource_workload = false;
  bool enable_first_last_activity_index = false;
  bool enable_max_concurrent_events = false;
  bool enable_max_concurrent_events_per_activity = false;
  double epsilon = 1e-6;
  std::size_t min_num_different_string_values = 2;
  std::size_t max_num_different_string_values = 500;

  // An explicit feature schema.  When non-empty it is returned verbatim and
  // rows are sized to it (unknown names are filled with default_not_present).
  std::vector<std::string> feature_names;
  double default_not_present = 0.0;
};

struct TemporalFeatureRow {
  double timestamp = 0.0;
  std::vector<double> values;
};

struct LogToFeaturesResult {
  LogToFeaturesVariant variant = LogToFeaturesVariant::TRACE_BASED;
  std::vector<std::string> feature_names;
  // Case identifiers are carried out-of-band because feature rows are numeric.
  std::vector<std::string> case_ids;
  std::vector<std::vector<std::vector<double>>> event_rows;
  std::vector<std::vector<double>> trace_rows;
  std::vector<TemporalFeatureRow> temporal_rows;
};

[[nodiscard]] LogToFeaturesResult log_to_features(
    const EventLog& log, const LogToFeaturesOptions& options = {});

[[nodiscard]] inline LogToFeaturesResult transform_log_to_features(
    const EventLog& log, const LogToFeaturesOptions& options = {}) {
  return log_to_features(log, options);
}

[[nodiscard]] inline LogToFeaturesResult extract_log_features(
    const EventLog& log, const LogToFeaturesOptions& options = {}) {
  return log_to_features(log, options);
}

using FeatureExtractionVariant = LogToFeaturesVariant;
using FeatureExtractionOptions = LogToFeaturesOptions;
using FeatureExtractionResult = LogToFeaturesResult;

}  // namespace minera
