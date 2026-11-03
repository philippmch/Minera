#pragma once

#include "minera/event_log.hpp"

#include <string>
#include <string_view>

namespace minera {

enum class LogSegmentOccurrence { first, last };

struct LogSegmentFilterOptions {
  std::string activity_key = "concept:name";
  bool strict = true;
  LogSegmentOccurrence occurrence = LogSegmentOccurrence::first;
};

// Slice at the first/last exact string-valued selected activity. Empty labels
// are valid; missing and non-string values do not match. Traces without an
// anchor are omitted. Strict excludes the anchor for BOTH operations.
// segment_filter.hpp's native extension uses the same strictness but rejects
// empty labels.
EventLog filter_log_prefix(const EventLog& source, std::string_view activity,
                          const LogSegmentFilterOptions& options = {});
EventLog filter_log_suffix(const EventLog& source, std::string_view activity,
                          const LogSegmentFilterOptions& options = {});

}  // namespace minera
