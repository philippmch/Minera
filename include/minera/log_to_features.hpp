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
  LogToFeaturesVariant variant = LogToFeaturesVariant::EVENT_BASED;
  std::string activity_key = "activity";
  std::string timestamp_key = "time:timestamp";
  std::string start_timestamp_key;
  std::string resource_key = "org:resource";
  double window_seconds = 604800.0;

  std::vector<std::string> event_string_attributes;
  std::vector<std::string> event_numeric_attributes;
  std::vector<std::string> case_string_attributes;
  std::vector<std::string> case_numeric_attributes;
  // Generic lists are accepted as aliases; their type is read from the first
  // occurrence's AttributeKind and falls back to numeric parsing.
  std::vector<std::string> event_attributes;
  std::vector<std::string> case_attributes;

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
  LogToFeaturesVariant variant = LogToFeaturesVariant::EVENT_BASED;
  std::vector<std::string> feature_names;
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
