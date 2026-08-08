#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

enum class AnchorOccurrence {
  first,
  last,
  kFirst = first,
  kLast = last,
};
using AnchorSelection = AnchorOccurrence;
using Occurrence = AnchorOccurrence;

// Options for cutting the beginning of a trace at a matching event.
// In strict mode the matching event is omitted; otherwise it is retained.
struct PrefixFilterOptions {
  std::string activity;
  bool strict = true;
  AnchorOccurrence occurrence = AnchorOccurrence::first;
};
using PrefixOptions = PrefixFilterOptions;

// Options for cutting the end of a trace at its last matching event.  In
// strict mode the matching event is retained; otherwise it is omitted.
struct SuffixFilterOptions {
  std::string activity;
  bool strict = true;
  AnchorOccurrence occurrence = AnchorOccurrence::first;
};
using SuffixOptions = SuffixFilterOptions;

// Extract complete subtraces beginning at a listed start activity and ending
// at the next listed end activity.  A closing event that is also a start may
// open the next segment.  A separator is inserted between an original case id
// and a one-based segment number when a trace yields multiple segments.
struct BetweenFilterOptions {
  std::vector<std::string> start_activities;
  std::vector<std::string> end_activities;
  std::string case_id_separator = "##@@";
  // Compatibility spelling for callers that use "subcase" terminology.  If
  // set, this takes precedence over case_id_separator.
  std::string subcase_separator;
};
using BetweenOptions = BetweenFilterOptions;

// Apply any combination of the three segment operations in prefix, suffix,
// then between order.  Empty optionals leave that operation disabled.
struct SegmentFilterOptions {
  PrefixFilterOptions prefix;
  SuffixFilterOptions suffix;
  BetweenFilterOptions between;
  bool use_prefix = false;
  bool use_suffix = false;
  bool use_between = false;
};

[[nodiscard]] EventLog filter_prefix(const EventLog& event_log,
                                     const PrefixFilterOptions& options);
[[nodiscard]] EventLog filter_prefix(const EventLog& event_log,
                                     std::string_view activity,
                                     bool strict = true);

[[nodiscard]] EventLog filter_suffix(const EventLog& event_log,
                                     const SuffixFilterOptions& options);
[[nodiscard]] EventLog filter_suffix(const EventLog& event_log,
                                     std::string_view activity,
                                     bool strict = true);

[[nodiscard]] EventLog filter_between(const EventLog& event_log,
                                      const BetweenFilterOptions& options);

// Convenience overload for a single start/end activity pair.
[[nodiscard]] EventLog filter_between(const EventLog& event_log,
                                      std::string_view start_activity,
                                      std::string_view end_activity,
                                      std::string_view case_id_separator = "##@@");

[[nodiscard]] EventLog filter_segments(const EventLog& event_log,
                                       const SegmentFilterOptions& options);

// Compatibility aliases retaining the noun-first naming used by some clients.
[[nodiscard]] inline EventLog prefix_filter(const EventLog& log,
                                            const PrefixFilterOptions& o) {
  return filter_prefix(log, o);
}
[[nodiscard]] inline EventLog suffix_filter(const EventLog& log,
                                            const SuffixFilterOptions& o) {
  return filter_suffix(log, o);
}
[[nodiscard]] inline EventLog between_filter(const EventLog& log,
                                             const BetweenFilterOptions& o) {
  return filter_between(log, o);
}

}  // namespace minera
