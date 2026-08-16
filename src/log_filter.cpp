#include "minera/log_filter.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

std::unordered_set<std::string> to_set(const std::vector<std::string>& values) {
  return {values.begin(), values.end()};
}

// The attribute constraint shared by both attribute-filter modes: the key
// (empty = filter off) and the value keep/remove sets.
struct AttributeMatcher {
  std::string key;
  std::unordered_set<std::string> keep;
  std::unordered_set<std::string> remove;

  [[nodiscard]] bool active() const { return !key.empty(); }

  // Event-mode survival. Events without the key follow the resource filter's
  // rule — never kept by a keep-set, never dropped by a remove-set — except
  // that a bare key (no value sets) keeps only the events carrying it.
  [[nodiscard]] bool keeps_event(const Event& event) const {
    const auto* value = event.attribute(key);
    if (value == nullptr) return keep.empty() && !remove.empty();
    if (!keep.empty() && keep.find(*value) == keep.end()) return false;
    if (!remove.empty() && remove.find(*value) != remove.end()) return false;
    return true;
  }

  // Case-attribute survival, judged on the trace's own attributes: same rule
  // as keeps_event, so a case without the key is never kept by a keep-set,
  // never dropped by a remove-set, and dropped by a bare key.
  [[nodiscard]] bool keeps_case(const Trace& trace) const {
    const auto* value = trace.attribute(key);
    if (value == nullptr) return keep.empty() && !remove.empty();
    if (!keep.empty() && keep.find(*value) == keep.end()) return false;
    if (!remove.empty() && remove.find(*value) != remove.end()) return false;
    return true;
  }

  // Trace-mode survival: a remove-set drops the trace as soon as any event
  // carries a removed value, a keep-set requires some event to carry a kept
  // one, and a bare key requires some event to carry the key at all.
  [[nodiscard]] bool keeps_trace(const Trace& trace) const {
    bool kept_value_seen = false;
    bool key_seen = false;
    for (const auto& event : trace.events) {
      const auto* value = event.attribute(key);
      if (value == nullptr) continue;
      key_seen = true;
      if (!remove.empty() && remove.find(*value) != remove.end()) return false;
      if (!keep.empty() && keep.find(*value) != keep.end()) kept_value_seen = true;
    }
    if (!keep.empty()) return kept_value_seen;
    if (!remove.empty()) return true;
    return key_seen;
  }
};

// Projects a trace's events on the activity, resource, lifecycle, and — when
// `attribute` is non-null — generic-attribute constraints at once, dropping
// events outside a keep-set or inside a remove-set and preserving order. An event with no
// resource is never kept by a resource keep-set and never dropped by a
// resource remove-set, so a resource filter only ever acts on events that
// actually name a performer.
Trace project_events(const Trace& trace,
                     const std::unordered_set<std::string>& keep_activities,
                     const std::unordered_set<std::string>& remove_activities,
                     const std::unordered_set<std::string>& keep_resources,
                     const std::unordered_set<std::string>& remove_resources,
                     const std::unordered_set<std::string>& keep_lifecycles,
                     const std::unordered_set<std::string>& remove_lifecycles,
                     const AttributeMatcher* attribute) {
  Trace result;
  result.case_id = trace.case_id;
  result.attributes = trace.attributes;
  for (const auto& event : trace.events) {
    if (!keep_activities.empty() &&
        keep_activities.find(event.activity) == keep_activities.end()) {
      continue;
    }
    if (!remove_activities.empty() &&
        remove_activities.find(event.activity) != remove_activities.end()) {
      continue;
    }
    if (!keep_resources.empty() &&
        (event.resource.empty() ||
         keep_resources.find(event.resource) == keep_resources.end())) {
      continue;
    }
    if (!remove_resources.empty() && !event.resource.empty() &&
        remove_resources.find(event.resource) != remove_resources.end()) {
      continue;
    }
    if (!keep_lifecycles.empty() &&
        (event.lifecycle.empty() ||
         keep_lifecycles.find(event.lifecycle) == keep_lifecycles.end())) {
      continue;
    }
    if (!remove_lifecycles.empty() && !event.lifecycle.empty() &&
        remove_lifecycles.find(event.lifecycle) != remove_lifecycles.end()) {
      continue;
    }
    if (attribute != nullptr && !attribute->keeps_event(event)) {
      continue;
    }
    result.events.push_back(event);
  }
  return result;
}

// Parses a time-filter bound; an empty string means "unbounded" and yields the
// given infinity, anything unparseable is a caller error and throws.
double parse_time_bound(const std::string& text, double unbounded) {
  if (text.empty()) return unbounded;
  const auto instant = parse_timestamp(text);
  if (!instant) {
    throw std::invalid_argument("time filter: unrecognized timestamp '" + text +
                                "'");
  }
  return *instant;
}

// The smallest and largest parsed instant in a trace, or nullopt when no event
// carries a recognizable timestamp.
std::optional<std::pair<double, double>> trace_time_span(const Trace& trace) {
  std::optional<std::pair<double, double>> span;
  for (const auto& event : trace.events) {
    const auto instant = parse_timestamp(event.timestamp);
    if (!instant) continue;
    if (!span) {
      span = {*instant, *instant};
    } else {
      span->first = std::min(span->first, *instant);
      span->second = std::max(span->second, *instant);
    }
  }
  return span;
}

// The subtraces of `trace` that run from an occurrence of `start` to the next
// occurrence of `end`, both included. The scan resumes after each closed
// segment, and an unclosed segment at the trace's end is discarded rather than
// emitted half-open. When start == end the closing event also opens the next
// segment, so a,x,a,y,a yields every a-to-a stretch.
std::vector<Trace> extract_between(const Trace& trace, const std::string& start,
                                   const std::string& end) {
  std::vector<Trace> segments;
  Trace current;
  bool collecting = false;
  for (const auto& event : trace.events) {
    if (!collecting) {
      if (event.activity != start) continue;
      collecting = true;
      current.events.clear();
    }
    current.events.push_back(event);
    if (event.activity == end && (current.events.size() > 1 || start != end)) {
      segments.push_back(std::move(current));
      current = Trace{};
      collecting = false;
      if (start == end) {
        collecting = true;
        current.events.push_back(event);
      }
    }
  }
  // A lone segment keeps the original case id; several get a #k suffix so each
  // subtrace remains a distinct case. All inherit the case's attributes.
  for (std::size_t i = 0; i < segments.size(); ++i) {
    segments[i].case_id = segments.size() == 1
                              ? trace.case_id
                              : trace.case_id + "#" + std::to_string(i + 1);
    segments[i].attributes = trace.attributes;
  }
  return segments;
}

// Whether some listed succession appears in the trace with the successor as
// the very next event.
bool has_directly_follows(const Trace& trace,
                          const std::vector<ActivityPair>& pairs) {
  for (std::size_t i = 0; i + 1 < trace.events.size(); ++i) {
    for (const auto& pair : pairs) {
      if (trace.events[i].activity == pair.predecessor &&
          trace.events[i + 1].activity == pair.successor) {
        return true;
      }
    }
  }
  return false;
}

// Whether some listed succession appears in the trace with the successor
// anywhere after the predecessor. The predecessor is registered after the
// successor check, so an event never follows itself — a pair (a, a) needs a
// genuine second occurrence.
bool has_eventually_follows(const Trace& trace,
                            const std::vector<ActivityPair>& pairs) {
  for (const auto& pair : pairs) {
    bool seen_predecessor = false;
    for (const auto& event : trace.events) {
      if (seen_predecessor && event.activity == pair.successor) return true;
      if (event.activity == pair.predecessor) seen_predecessor = true;
    }
  }
  return false;
}

std::string variant_key(const Trace& trace) {
  std::string key;
  for (const auto& event : trace.events) {
    key.push_back('\x1f');
    key += event.activity;
  }
  return key;
}

bool trace_matches_pattern(const Trace& trace, const TracePattern& pattern) {
  const std::size_t event_count = trace.events.size();
  std::vector<unsigned char> previous(event_count + 1, 0);
  std::vector<unsigned char> current(event_count + 1, 0);
  previous[0] = 1;
  for (const auto& token : pattern) {
    std::fill(current.begin(), current.end(), static_cast<unsigned char>(0));
    if (token == "...") {
      // A wildcard consumes zero or more events. The left-to-right recurrence
      // keeps the implementation linear in the trace length for this token.
      current[0] = previous[0];
      for (std::size_t j = 1; j <= event_count; ++j) {
        current[j] = static_cast<unsigned char>(
            previous[j] != 0 || current[j - 1] != 0);
      }
    } else {
      for (std::size_t j = 1; j <= event_count; ++j) {
        if (previous[j - 1] != 0 && trace.events[j - 1].activity == token)
          current[j] = 1;
      }
    }
    previous.swap(current);
  }
  return previous[event_count] != 0;
}

bool trace_matches_any_pattern(const Trace& trace,
                               const std::vector<TracePattern>& patterns) {
  for (const auto& pattern : patterns) {
    if (pattern.empty())
      throw std::invalid_argument("trace pattern must not be empty");
    for (const auto& token : pattern) {
      if (token.empty())
        throw std::invalid_argument("trace pattern activity must not be empty");
    }
    if (trace_matches_pattern(trace, pattern)) return true;
  }
  return false;
}

std::optional<double> case_duration_seconds(const Trace& trace) {
  if (trace.events.empty()) return std::nullopt;
  const auto first = parse_timestamp(trace.events.front().timestamp);
  const auto last = parse_timestamp(trace.events.back().timestamp);
  if (!first || !last) return std::nullopt;
  return *last - *first;
}

void validate_case_performance_options(
    const CasePerformanceFilterOptions& options) {
  if (options.min_duration_seconds &&
      !std::isfinite(*options.min_duration_seconds)) {
    throw std::invalid_argument("case duration minimum must be finite");
  }
  if (options.max_duration_seconds &&
      !std::isfinite(*options.max_duration_seconds)) {
    throw std::invalid_argument("case duration maximum must be finite");
  }
  if (options.max_duration_seconds &&
      options.min_duration_seconds.value_or(0.0) >
          *options.max_duration_seconds) {
    throw std::invalid_argument(
        "case duration minimum must not exceed the maximum");
  }
}

bool case_duration_matches(const Trace& trace,
                           const CasePerformanceFilterOptions& options) {
  const auto duration = case_duration_seconds(trace);
  if (!duration) return false;
  if (options.min_duration_seconds &&
      *duration < *options.min_duration_seconds)
    return false;
  if (options.max_duration_seconds &&
      *duration > *options.max_duration_seconds)
    return false;
  return true;
}

void validate_case_size_options(const CaseSizeFilterOptions& options) {
  if (options.max_length && options.min_length > *options.max_length) {
    throw std::invalid_argument(
        "case size minimum must not exceed the maximum");
  }
}

bool case_size_matches(const Trace& trace, const CaseSizeFilterOptions& options) {
  const auto length = trace.events.size();
  if (length < options.min_length) return false;
  if (options.max_length && length > *options.max_length) return false;
  return true;
}

std::optional<double> numeric_attribute_value(const Event& event,
                                              std::string_view key) {
  const auto* text = event.attribute(key);
  if (!text || text->empty()) return std::nullopt;
  char* end = nullptr;
  errno = 0;
  const double value = std::strtod(text->c_str(), &end);
  if (end != text->c_str() + text->size() || errno == ERANGE ||
      !std::isfinite(value)) {
    return std::nullopt;
  }
  return value;
}

const std::string* frequency_attribute_value(const Event& event,
                                             std::string_view key) {
  return key.empty() ? &event.activity : event.attribute(key);
}

void validate_relative_attribute_occurrence_options(
    const RelativeAttributeOccurrenceFilterOptions& options) {
  if (!std::isfinite(options.min_relative_stake) ||
      options.min_relative_stake < 0.0 || options.min_relative_stake > 1.0) {
    throw std::invalid_argument(
        "relative attribute occurrence: stake must be finite and in [0, 1]");
  }
}

void validate_variant_share(const std::optional<double>& value,
                            std::string_view name) {
  if (!value) return;
  if (!std::isfinite(*value) || *value < 0.0 || *value > 1.0) {
    throw std::invalid_argument(std::string(name) +
                                ": expected a finite share in [0, 1]");
  }
}

void validate_variant_share(double value, std::string_view name) {
  validate_variant_share(std::optional<double>(value), name);
}

std::unordered_set<std::string> top_activity_values(
    const EventLog& event_log, const TopActivitiesFilterOptions& options,
    bool* projection_needed) {
  std::unordered_map<std::string, std::size_t> counts;
  for (const auto& trace : event_log.traces) {
    for (const auto& event : trace.events) {
      const auto* value = frequency_attribute_value(event, options.attribute_key);
      if (value != nullptr) ++counts[*value];
    }
  }

  std::vector<std::pair<std::string, std::size_t>> ranked;
  ranked.reserve(counts.size());
  for (const auto& entry : counts) ranked.push_back(entry);
  std::sort(ranked.begin(), ranked.end(),
            [](const auto& left, const auto& right) {
              if (left.second != right.second) return left.second > right.second;
              return left.first < right.first;
            });

  const std::size_t limit = std::min(options.max_activities, ranked.size());
  std::unordered_set<std::string> allowed;
  allowed.reserve(limit);
  for (std::size_t i = 0; i < limit; ++i) allowed.insert(ranked[i].first);
  *projection_needed = limit < ranked.size();
  return allowed;
}

std::unordered_set<std::string> relative_attribute_values(
    const EventLog& event_log,
    const RelativeAttributeOccurrenceFilterOptions& options) {
  std::unordered_map<std::string, std::size_t> counts;
  for (const auto& trace : event_log.traces) {
    std::unordered_set<std::string> seen;
    if (options.keep_once_per_case) seen.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      const auto* value = frequency_attribute_value(event, options.attribute_key);
      if (value == nullptr) continue;
      if (options.keep_once_per_case && !seen.insert(*value).second) continue;
      ++counts[*value];
    }
  }

  const double denominator = options.keep_once_per_case
                                 ? static_cast<double>(event_log.traces.size())
                                 : static_cast<double>(event_log.event_count());
  std::unordered_set<std::string> allowed;
  if (denominator == 0.0) return allowed;
  for (const auto& entry : counts) {
    if (static_cast<double>(entry.second) >=
        options.min_relative_stake * denominator) {
      allowed.insert(entry.first);
    }
  }
  return allowed;
}

std::unordered_set<std::string> threshold_attribute_values(
    const EventLog& event_log,
    const AttributeOccurrenceThresholdFilterOptions& options) {
  std::unordered_map<std::string, std::uint64_t> counts;
  for (const auto& trace : event_log.traces) {
    for (const auto& event : trace.events) {
      const auto* value = frequency_attribute_value(event, options.attribute_key);
      if (value != nullptr) ++counts[*value];
    }
  }
  std::unordered_set<std::string> allowed;
  for (const auto& entry : counts) {
    if (entry.second >= options.min_occurrences) allowed.insert(entry.first);
  }
  return allowed;
}

Trace project_frequency_attributes(
    const Trace& trace, std::string_view top_key,
    const std::unordered_set<std::string>* top_allowed,
    std::string_view relative_key,
    const std::unordered_set<std::string>* relative_allowed,
    std::string_view threshold_key,
    const std::unordered_set<std::string>* threshold_allowed) {
  Trace result;
  result.case_id = trace.case_id;
  result.attributes = trace.attributes;
  for (const auto& event : trace.events) {
    if (top_allowed != nullptr) {
      const auto* value = frequency_attribute_value(event, top_key);
      if (value == nullptr || top_allowed->find(*value) == top_allowed->end())
        continue;
    }
    if (relative_allowed != nullptr) {
      const auto* value = frequency_attribute_value(event, relative_key);
      if (value == nullptr ||
          relative_allowed->find(*value) == relative_allowed->end())
        continue;
    }
    if (threshold_allowed != nullptr) {
      const auto* value = frequency_attribute_value(event, threshold_key);
      if (value == nullptr ||
          threshold_allowed->find(*value) == threshold_allowed->end())
        continue;
    }
    result.events.push_back(event);
  }
  return result;
}

void validate_numeric_attribute_options(
    const NumericAttributeFilterOptions& options) {
  if (options.attribute_key.empty()) {
    throw std::invalid_argument(
        "numeric attribute filter: an attribute key is required");
  }
  if (!options.min_value && !options.max_value) {
    throw std::invalid_argument(
        "numeric attribute filter: at least one bound is required");
  }
  if (options.min_value && !std::isfinite(*options.min_value)) {
    throw std::invalid_argument(
        "numeric attribute filter: minimum must be finite");
  }
  if (options.max_value && !std::isfinite(*options.max_value)) {
    throw std::invalid_argument(
        "numeric attribute filter: maximum must be finite");
  }
  if (options.max_value && options.min_value &&
      *options.min_value > *options.max_value) {
    throw std::invalid_argument(
        "numeric attribute filter: minimum must not exceed the maximum");
  }
}

bool numeric_attribute_in_range(const Event& event,
                                const NumericAttributeFilterOptions& options) {
  const auto value = numeric_attribute_value(event, options.attribute_key);
  if (!value) return false;
  if (options.min_value && *value < *options.min_value) return false;
  if (options.max_value && *value > *options.max_value) return false;
  return true;
}

bool numeric_attribute_event_survives(
    const Event& event, const NumericAttributeFilterOptions& options) {
  if (!numeric_attribute_value(event, options.attribute_key)) return false;
  return numeric_attribute_in_range(event, options) == options.positive;
}

bool numeric_trace_matches(const Trace& trace,
                           const NumericAttributeFilterOptions& options) {
  const bool found = std::any_of(
      trace.events.begin(), trace.events.end(), [&](const Event& event) {
        return numeric_attribute_in_range(event, options);
      });
  return found == options.positive;
}

void validate_prefix_suffix_options(const PrefixSuffixFilterOptions& options) {
  if (options.activity.empty()) {
    throw std::invalid_argument(
        "prefix/suffix filter: an anchor activity is required");
  }
}

std::optional<Trace> cut_trace(const Trace& trace,
                               const PrefixSuffixFilterOptions& options,
                               bool prefix) {
  const auto find_anchor = [&]() -> std::optional<std::size_t> {
    if (options.occurrence == FilterAnchorOccurrence::first) {
      for (std::size_t i = 0; i < trace.events.size(); ++i) {
        if (trace.events[i].activity == options.activity) return i;
      }
    } else {
      for (std::size_t i = trace.events.size(); i-- > 0;) {
        if (trace.events[i].activity == options.activity) return i;
      }
    }
    return std::nullopt;
  };
  const auto anchor = find_anchor();
  if (!anchor) return std::nullopt;

  const std::size_t begin = prefix ? 0 : *anchor + (options.strict ? 0 : 1);
  const std::size_t end = prefix ? *anchor + (options.strict ? 1 : 0)
                                 : trace.events.size();
  if (begin >= end) return std::nullopt;
  Trace result;
  result.case_id = trace.case_id;
  result.attributes = trace.attributes;
  result.events.insert(result.events.end(), trace.events.begin() + begin,
                       trace.events.begin() + end);
  return result;
}

}  // namespace

std::optional<TimeFilterMode> parse_time_filter_mode(std::string_view name) {
  if (name == "events") return TimeFilterMode::events;
  if (name == "traces_contained") return TimeFilterMode::traces_contained;
  if (name == "traces_intersecting") return TimeFilterMode::traces_intersecting;
  return std::nullopt;
}

std::optional<AttributeFilterMode> parse_attribute_filter_mode(
    std::string_view name) {
  if (name == "events") return AttributeFilterMode::events;
  if (name == "traces") return AttributeFilterMode::traces;
  return std::nullopt;
}

std::vector<ActivityPair> parse_activity_pairs(std::string_view text) {
  const auto trim = [](std::string_view value) {
    const auto begin = value.find_first_not_of(' ');
    if (begin == std::string_view::npos) return std::string_view{};
    const auto end = value.find_last_not_of(' ');
    return value.substr(begin, end - begin + 1);
  };

  std::vector<ActivityPair> pairs;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= text.size(); ++i) {
    if (i != text.size() && text[i] != ',') continue;
    const auto entry = trim(text.substr(start, i - start));
    start = i + 1;
    if (entry.empty()) continue;  // stray comma or trailing whitespace
    const auto arrow = entry.find("->");
    if (arrow == std::string_view::npos) {
      throw std::invalid_argument(
          "succession filter: expected 'a -> b', got '" + std::string(entry) +
          "'");
    }
    const auto predecessor = trim(entry.substr(0, arrow));
    const auto successor = trim(entry.substr(arrow + 2));
    if (predecessor.empty() || successor.empty()) {
      throw std::invalid_argument(
          "succession filter: an activity is missing in '" +
          std::string(entry) + "'");
    }
    pairs.push_back({std::string(predecessor), std::string(successor)});
  }
  return pairs;
}

std::vector<TracePattern> parse_trace_patterns(std::string_view text) {
  if (text.empty()) return {};
  const auto trim = [](std::string_view value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) return std::string_view{};
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
  };

  std::vector<TracePattern> patterns;
  std::size_t pattern_start = 0;
  for (std::size_t i = 0; i <= text.size(); ++i) {
    if (i != text.size() && text[i] != ';') continue;
    const auto pattern_text = trim(text.substr(pattern_start, i - pattern_start));
    if (pattern_text.empty())
      throw std::invalid_argument("trace_patterns: patterns must not be empty");
    TracePattern pattern;
    std::size_t activity_start = 0;
    for (std::size_t j = 0; j <= pattern_text.size(); ++j) {
      if (j != pattern_text.size() && pattern_text[j] != ',') continue;
      const auto token = trim(pattern_text.substr(activity_start,
                                                  j - activity_start));
      if (token.empty())
        throw std::invalid_argument(
            "trace_patterns: activities must not be empty");
      pattern.emplace_back(token);
      activity_start = j + 1;
    }
    patterns.push_back(std::move(pattern));
    pattern_start = i + 1;
  }
  return patterns;
}

EventLog filter_trace_patterns(const EventLog& event_log,
                               const TracePatternFilterOptions& options) {
  if (options.patterns.empty()) return event_log;
  EventLog result;
  result.attributes = event_log.attributes;
  for (const auto& trace : event_log.traces) {
    const bool found = trace_matches_any_pattern(trace, options.patterns);
    if (found == options.positive) result.traces.push_back(trace);
  }
  return result;
}

EventLog filter_case_performance(
    const EventLog& event_log, const CasePerformanceFilterOptions& options) {
  validate_case_performance_options(options);
  if (!options.min_duration_seconds && !options.max_duration_seconds)
    return event_log;
  EventLog result;
  result.attributes = event_log.attributes;
  for (const auto& trace : event_log.traces) {
    if (case_duration_matches(trace, options)) result.traces.push_back(trace);
  }
  return result;
}

EventLog filter_case_size(const EventLog& event_log,
                          const CaseSizeFilterOptions& options) {
  validate_case_size_options(options);
  EventLog result;
  result.attributes = event_log.attributes;
  for (const auto& trace : event_log.traces) {
    if (case_size_matches(trace, options)) result.traces.push_back(trace);
  }
  return result;
}

EventLog filter_case_size(const EventLog& event_log, std::size_t min_length,
                          std::optional<std::size_t> max_length) {
  return filter_case_size(
      event_log, CaseSizeFilterOptions{min_length, max_length});
}

EventLog filter_numeric_attribute(
    const EventLog& event_log,
    const NumericAttributeFilterOptions& options) {
  validate_numeric_attribute_options(options);
  EventLog result;
  result.attributes = event_log.attributes;
  for (const auto& trace : event_log.traces) {
    if (options.mode == AttributeFilterMode::traces) {
      if (numeric_trace_matches(trace, options) && !trace.events.empty())
        result.traces.push_back(trace);
      continue;
    }
    Trace projected;
    projected.case_id = trace.case_id;
    projected.attributes = trace.attributes;
    for (const auto& event : trace.events) {
      if (numeric_attribute_event_survives(event, options))
        projected.events.push_back(event);
    }
    if (!projected.events.empty()) result.traces.push_back(std::move(projected));
  }
  return result;
}

EventLog filter_top_activities(
    const EventLog& event_log, const TopActivitiesFilterOptions& options) {
  bool projection_needed = false;
  const auto allowed =
      top_activity_values(event_log, options, &projection_needed);
  if (!projection_needed) return event_log;

  EventLog result;
  result.attributes = event_log.attributes;
  for (const auto& trace : event_log.traces) {
    auto projected = project_frequency_attributes(
        trace, options.attribute_key, &allowed, {}, nullptr, {}, nullptr);
    if (!projected.events.empty()) result.traces.push_back(std::move(projected));
  }
  return result;
}

EventLog filter_relative_attribute_occurrence(
    const EventLog& event_log,
    const RelativeAttributeOccurrenceFilterOptions& options) {
  validate_relative_attribute_occurrence_options(options);
  const auto allowed = relative_attribute_values(event_log, options);

  EventLog result;
  result.attributes = event_log.attributes;
  for (const auto& trace : event_log.traces) {
    auto projected = project_frequency_attributes(
        trace, {}, nullptr, options.attribute_key, &allowed, {}, nullptr);
    if (!projected.events.empty()) result.traces.push_back(std::move(projected));
  }
  return result;
}

EventLog filter_attribute_occurrence_threshold(
    const EventLog& event_log,
    const AttributeOccurrenceThresholdFilterOptions& options) {
  const auto allowed = threshold_attribute_values(event_log, options);
  EventLog result;
  result.attributes = event_log.attributes;
  for (const auto& trace : event_log.traces) {
    auto projected = project_frequency_attributes(
        trace, {}, nullptr, {}, nullptr, options.attribute_key, &allowed);
    if (!projected.events.empty()) result.traces.push_back(std::move(projected));
  }
  return result;
}

EventLog filter_start_activities(const EventLog& event_log,
                                 const std::vector<std::string>& activities,
                                 bool positive) {
  if (!positive && activities.empty()) {
    throw std::invalid_argument(
        "start activity polarity requires at least one activity");
  }
  const auto allowed = to_set(activities);
  EventLog result;
  result.attributes = event_log.attributes;
  for (const auto& trace : event_log.traces) {
    if (trace.events.empty()) continue;
    const bool found = allowed.find(trace.events.front().activity) != allowed.end();
    if (found == positive) result.traces.push_back(trace);
  }
  return result;
}

EventLog filter_end_activities(const EventLog& event_log,
                               const std::vector<std::string>& activities,
                               bool positive) {
  if (!positive && activities.empty()) {
    throw std::invalid_argument(
        "end activity polarity requires at least one activity");
  }
  const auto allowed = to_set(activities);
  EventLog result;
  result.attributes = event_log.attributes;
  for (const auto& trace : event_log.traces) {
    if (trace.events.empty()) continue;
    const bool found = allowed.find(trace.events.back().activity) != allowed.end();
    if (found == positive) result.traces.push_back(trace);
  }
  return result;
}

EventLog filter_variants_top_k(const EventLog& event_log, std::size_t k) {
  if (k == 0) {
    EventLog result;
    result.attributes = event_log.attributes;
    return result;
  }
  LogFilterOptions options;
  options.top_variants = k;
  return filter_event_log(event_log, options);
}

EventLog filter_variants_by_coverage_percentage(
    const EventLog& event_log, double min_coverage_percentage) {
  validate_variant_share(min_coverage_percentage,
                         "variant minimum coverage");
  LogFilterOptions options;
  options.variant_min_coverage = min_coverage_percentage;
  return filter_event_log(event_log, options);
}

EventLog filter_variants_by_maximum_coverage_percentage(
    const EventLog& event_log, double max_coverage_percentage) {
  validate_variant_share(max_coverage_percentage,
                         "variant maximum coverage");
  LogFilterOptions options;
  options.variant_max_coverage = max_coverage_percentage;
  return filter_event_log(event_log, options);
}

EventLog filter_log_variants_percentage(const EventLog& event_log,
                                        double percentage) {
  validate_variant_share(percentage, "variant percentage");
  LogFilterOptions options;
  options.variant_percentage = percentage;
  return filter_event_log(event_log, options);
}

EventLog filter_prefix(const EventLog& event_log,
                       const PrefixSuffixFilterOptions& options) {
  validate_prefix_suffix_options(options);
  EventLog result;
  result.attributes = event_log.attributes;
  for (const auto& trace : event_log.traces) {
    const auto cut = cut_trace(trace, options, true);
    if (cut) result.traces.push_back(*cut);
  }
  return result;
}

EventLog filter_suffix(const EventLog& event_log,
                       const PrefixSuffixFilterOptions& options) {
  validate_prefix_suffix_options(options);
  EventLog result;
  result.attributes = event_log.attributes;
  for (const auto& trace : event_log.traces) {
    const auto cut = cut_trace(trace, options, false);
    if (cut) result.traces.push_back(*cut);
  }
  return result;
}

EventLog filter_ncases(const EventLog& event_log, std::size_t max_cases) {
  EventLog result;
  result.attributes = event_log.attributes;
  const auto count = std::min(max_cases, event_log.traces.size());
  result.traces.insert(result.traces.end(), event_log.traces.begin(),
                       event_log.traces.begin() + count);
  return result;
}

EventLog filter_event_log(const EventLog& log, const LogFilterOptions& options) {
  const auto keep = to_set(options.keep_activities);
  const auto remove = to_set(options.remove_activities);
  const auto keep_res = to_set(options.keep_resources);
  const auto remove_res = to_set(options.remove_resources);
  const auto keep_life = to_set(options.keep_lifecycles);
  const auto remove_life = to_set(options.remove_lifecycles);
  const auto starts = to_set(options.start_activities);
  const auto ends = to_set(options.end_activities);

  const AttributeMatcher attribute{options.attribute_key,
                                   to_set(options.keep_attribute_values),
                                   to_set(options.remove_attribute_values)};
  if (!attribute.active() &&
      (!attribute.keep.empty() || !attribute.remove.empty())) {
    throw std::invalid_argument(
        "attribute filter: value lists require an attribute key");
  }
  const AttributeMatcher* event_attribute =
      attribute.active() && options.attribute_mode == AttributeFilterMode::events
          ? &attribute
          : nullptr;

  if (options.numeric_attribute)
    validate_numeric_attribute_options(*options.numeric_attribute);
  if (options.relative_attribute_occurrence)
    validate_relative_attribute_occurrence_options(
        *options.relative_attribute_occurrence);

  bool top_projection_needed = false;
  std::unordered_set<std::string> top_allowed;
  if (options.top_activities) {
    top_allowed = top_activity_values(log, *options.top_activities,
                                      &top_projection_needed);
  }
  std::unordered_set<std::string> relative_allowed;
  if (options.relative_attribute_occurrence) {
    relative_allowed = relative_attribute_values(
        log, *options.relative_attribute_occurrence);
  }
  std::unordered_set<std::string> threshold_allowed;
  if (options.attribute_occurrence_threshold) {
    threshold_allowed = threshold_attribute_values(
        log, *options.attribute_occurrence_threshold);
  }

  if (options.prefix_activity.empty() &&
      (!options.prefix_strict ||
       options.prefix_occurrence != FilterAnchorOccurrence::first)) {
    throw std::invalid_argument(
        "prefix filter: options require prefix_activity");
  }
  if (options.suffix_activity.empty() &&
      (!options.suffix_strict ||
       options.suffix_occurrence != FilterAnchorOccurrence::first)) {
    throw std::invalid_argument(
        "suffix filter: options require suffix_activity");
  }

  const AttributeMatcher case_attribute{
      options.case_attribute_key,
      to_set(options.keep_case_attribute_values),
      to_set(options.remove_case_attribute_values)};
  if (!case_attribute.active() &&
      (!case_attribute.keep.empty() || !case_attribute.remove.empty())) {
    throw std::invalid_argument(
        "case-attribute filter: value lists require an attribute key");
  }

  const bool by_between =
      !options.between_start.empty() || !options.between_end.empty();
  if (by_between &&
      (options.between_start.empty() || options.between_end.empty())) {
    throw std::invalid_argument(
        "between filter: both a start and an end activity are required");
  }

  if (options.variant_coverage < 0.0 || options.variant_coverage > 1.0) {
    throw std::invalid_argument(
        "variant coverage: expected a share in (0, 1]");
  }
  validate_variant_share(options.variant_min_coverage,
                         "variant minimum coverage");
  validate_variant_share(options.variant_max_coverage,
                         "variant maximum coverage");
  if (options.variant_min_coverage && options.variant_max_coverage &&
      *options.variant_min_coverage > *options.variant_max_coverage) {
    throw std::invalid_argument(
        "variant minimum coverage must not exceed the maximum");
  }
  if (!options.start_activities_positive && options.start_activities.empty()) {
    throw std::invalid_argument(
        "start activity polarity requires at least one activity");
  }
  if (!options.end_activities_positive && options.end_activities.empty()) {
    throw std::invalid_argument(
        "end activity polarity requires at least one activity");
  }

  const bool by_time = !options.time_from.empty() || !options.time_to.empty();
  const double time_from = parse_time_bound(
      options.time_from, -std::numeric_limits<double>::infinity());
  const double time_to = parse_time_bound(
      options.time_to, std::numeric_limits<double>::infinity());
  const CasePerformanceFilterOptions case_performance{
      options.min_case_duration_seconds, options.max_case_duration_seconds};
  validate_case_performance_options(case_performance);
  const bool by_case_performance =
      case_performance.min_duration_seconds.has_value() ||
      case_performance.max_duration_seconds.has_value();

  if (options.max_length != 0 && options.min_length > options.max_length) {
    throw std::invalid_argument(
        "min_length must not exceed max_length");
  }

  if (!options.activity_split_activities.empty() &&
      options.activity_split_separator.empty()) {
    throw std::invalid_argument(
        "activity split: subcase separator must not be empty");
  }

  // Pass 1: constrain to the time range, then project activities and apply
  // per-trace structural filters.
  std::vector<Trace> kept;
  kept.reserve(log.traces.size());
  for (const auto& trace : log.traces) {
    if (case_attribute.active() && !case_attribute.keeps_case(trace)) {
      continue;
    }
    const Trace* source = &trace;
    Trace timed;  // only populated in events mode, which rewrites the trace
    if (by_time) {
      switch (options.time_mode) {
        case TimeFilterMode::events: {
          timed.case_id = trace.case_id;
          timed.attributes = trace.attributes;
          for (const auto& event : trace.events) {
            const auto instant = parse_timestamp(event.timestamp);
            if (instant && *instant >= time_from && *instant <= time_to) {
              timed.events.push_back(event);
            }
          }
          if (timed.events.empty()) continue;
          source = &timed;
          break;
        }
        case TimeFilterMode::traces_contained: {
          const auto span = trace_time_span(trace);
          if (!span || span->first < time_from || span->second > time_to) {
            continue;
          }
          break;
        }
        case TimeFilterMode::traces_intersecting: {
          const auto span = trace_time_span(trace);
          if (!span || span->second < time_from || span->first > time_to) {
            continue;
          }
          break;
        }
      }
    }

    if (by_case_performance &&
        !case_duration_matches(trace, case_performance)) {
      continue;
    }

    Trace numeric_projected;
    if (options.numeric_attribute) {
      const auto& numeric = *options.numeric_attribute;
      if (numeric.mode == AttributeFilterMode::traces) {
        if (!numeric_trace_matches(*source, numeric)) continue;
      } else {
        numeric_projected.case_id = source->case_id;
        numeric_projected.attributes = source->attributes;
        for (const auto& event : source->events) {
          if (numeric_attribute_event_survives(event, numeric))
            numeric_projected.events.push_back(event);
        }
        if (numeric_projected.events.empty()) continue;
        source = &numeric_projected;
      }
    }

    Trace frequency_projected;
    const auto* top_filter =
        options.top_activities && top_projection_needed ? &top_allowed : nullptr;
    const auto* relative_filter = options.relative_attribute_occurrence
                                      ? &relative_allowed
                                      : nullptr;
    const auto* threshold_filter = options.attribute_occurrence_threshold
                                       ? &threshold_allowed
                                       : nullptr;
    if (top_filter != nullptr || relative_filter != nullptr ||
        threshold_filter != nullptr) {
      frequency_projected = project_frequency_attributes(
          *source,
          options.top_activities ? options.top_activities->attribute_key : "",
          top_filter,
          options.relative_attribute_occurrence
              ? options.relative_attribute_occurrence->attribute_key
              : "",
          relative_filter,
          options.attribute_occurrence_threshold
              ? options.attribute_occurrence_threshold->attribute_key
              : "",
          threshold_filter);
      if (frequency_projected.events.empty()) continue;
      source = &frequency_projected;
    }

    Trace projected = project_events(*source, keep, remove, keep_res,
                                     remove_res, keep_life, remove_life,
                                     event_attribute);
    if (projected.events.empty()) continue;

    if (!options.trace_patterns.empty()) {
      const bool found = trace_matches_any_pattern(projected,
                                                   options.trace_patterns);
      if (found != options.trace_pattern_positive) continue;
    }

    if (attribute.active() &&
        options.attribute_mode == AttributeFilterMode::traces &&
        !attribute.keeps_trace(projected)) {
      continue;
    }

    if (!options.rework_activity.empty()) {
      std::uint64_t occurrences = 0;
      for (const auto& event : projected.events) {
        if (event.activity == options.rework_activity) ++occurrences;
      }
      if (occurrences < options.rework_min_occurrences) continue;
    }

    if (!options.keep_directly_follows.empty() &&
        !has_directly_follows(projected, options.keep_directly_follows)) {
      continue;
    }
    if (!options.remove_directly_follows.empty() &&
        has_directly_follows(projected, options.remove_directly_follows)) {
      continue;
    }
    if (!options.keep_eventually_follows.empty() &&
        !has_eventually_follows(projected, options.keep_eventually_follows)) {
      continue;
    }
    if (!options.remove_eventually_follows.empty() &&
        has_eventually_follows(projected, options.remove_eventually_follows)) {
      continue;
    }

    // Prefix/suffix cuts anchor on the first occurrence; a case without the
    // anchor is dropped rather than passed through whole.
    if (!options.prefix_activity.empty()) {
      const auto cut = cut_trace(
          projected,
          PrefixSuffixFilterOptions{options.prefix_activity,
                                     options.prefix_strict,
                                     options.prefix_occurrence},
          true);
      if (!cut) continue;
      projected = *cut;
    }
    if (!options.suffix_activity.empty()) {
      const auto cut = cut_trace(
          projected,
          PrefixSuffixFilterOptions{options.suffix_activity,
                                     options.suffix_strict,
                                     options.suffix_occurrence},
          false);
      if (!cut) continue;
      projected = *cut;
    }

    std::vector<Trace> pieces;
    if (by_between) {
      pieces = extract_between(projected, options.between_start,
                               options.between_end);
    } else {
      pieces.push_back(std::move(projected));
    }

    for (auto& piece : pieces) {
      const std::size_t length = piece.events.size();
      if (options.min_length != 0 && length < options.min_length) continue;
      if (options.max_length != 0 && length > options.max_length) continue;

      if (!starts.empty()) {
        const bool found = starts.find(piece.events.front().activity) != starts.end();
        if (found != options.start_activities_positive) continue;
      }
      if (!ends.empty()) {
        const bool found = ends.find(piece.events.back().activity) != ends.end();
        if (found != options.end_activities_positive) continue;
      }
      kept.push_back(std::move(piece));
    }
  }

  if (!options.activity_split_activities.empty()) {
    ActivitySplitOptions split_options;
    split_options.activities = options.activity_split_activities;
    split_options.subcase_separator = options.activity_split_separator;
    split_options.cut_mode = options.activity_split_cut_mode;
    EventLog split_log;
    split_log.traces = std::move(kept);
    split_log.attributes = log.attributes;
    split_log = split_activity(split_log, split_options);
    kept = std::move(split_log.traces);
  }

  if (options.attribute_value_repetition) {
    EventLog repetition_log;
    repetition_log.traces = std::move(kept);
    repetition_log.attributes = log.attributes;
    repetition_log = filter_attribute_value_repetition(
        repetition_log, *options.attribute_value_repetition);
    kept = std::move(repetition_log.traces);
  }

  if (options.consecutive_activity_grouping) {
    EventLog grouped;
    grouped.traces = std::move(kept);
    grouped.attributes = log.attributes;
    grouped = group_consecutive_activities(
        grouped, *options.consecutive_activity_grouping);
    kept = std::move(grouped.traces);
  }
  if (options.timestamp_case_grouping) {
    EventLog grouped;
    grouped.traces = std::move(kept);
    grouped.attributes = log.attributes;
    grouped = group_same_timestamp_events(grouped,
                                          *options.timestamp_case_grouping);
    kept = std::move(grouped.traces);
  }

  const bool by_frequency = options.min_variant_frequency > 1;
  const bool by_rank = options.top_variants != 0;
  const bool by_coverage = options.variant_coverage > 0.0;
  const bool by_min_coverage = options.variant_min_coverage.has_value();
  const bool by_max_coverage = options.variant_max_coverage.has_value();
  const bool by_percentage = options.variant_percentage.has_value();
  if (!by_frequency && !by_rank && !by_coverage && !by_min_coverage &&
      !by_max_coverage && !by_percentage) {
    EventLog result;
    result.traces = std::move(kept);
    result.attributes = log.attributes;
    if (options.max_cases && result.traces.size() > *options.max_cases)
      result.traces.resize(*options.max_cases);
    return result;
  }

  // Pass 2: count variants on the surviving traces, then decide which variants
  // are allowed. A variant must satisfy every active constraint: frequency,
  // rank, and membership in the cumulative-coverage prefix.
  std::unordered_map<std::string, std::uint64_t> counts;
  std::vector<std::string> order;  // first-seen order, for deterministic ranking
  for (const auto& trace : kept) {
    auto key = variant_key(trace);
    if (counts.emplace(key, 0).second) order.push_back(key);
    ++counts[key];
  }

  if (by_rank || by_coverage || by_percentage) {
    std::stable_sort(order.begin(), order.end(),
                     [&](const std::string& a, const std::string& b) {
                       return counts[a] > counts[b];
                     });
  }

  // Coverage prefix: the most frequent variants until together they reach the
  // requested share of the surviving cases (so it always holds at least the
  // single most frequent variant).
  std::unordered_set<std::string> covered;
  if (by_coverage) {
    const double target =
        options.variant_coverage * static_cast<double>(kept.size());
    double cumulative = 0.0;
    for (const auto& key : order) {
      covered.insert(key);
      cumulative += static_cast<double>(counts[key]);
      if (cumulative >= target) break;
    }
  }

  std::unordered_set<std::string> percentage_allowed;
  if (by_percentage && !order.empty()) {
    const double target =
        *options.variant_percentage * static_cast<double>(kept.size());
    double cumulative = 0.0;
    std::size_t boundary = 0;
    for (std::size_t i = 0; i < order.size(); ++i) {
      cumulative += static_cast<double>(counts[order[i]]);
      boundary = i;
      if (cumulative >= target) break;
    }
    const auto boundary_count = counts[order[boundary]];
    for (std::size_t i = 0; i <= boundary; ++i)
      percentage_allowed.insert(order[i]);
    for (std::size_t i = boundary + 1; i < order.size() &&
         counts[order[i]] == boundary_count; ++i) {
      percentage_allowed.insert(order[i]);
    }
  }

  std::unordered_set<std::string> allowed;
  const double denominator = static_cast<double>(kept.size());
  for (std::size_t i = 0; i < order.size(); ++i) {
    if (by_rank && i >= options.top_variants) break;
    if (counts[order[i]] < options.min_variant_frequency) continue;
    if (by_coverage && covered.find(order[i]) == covered.end()) continue;
    if (by_percentage && percentage_allowed.find(order[i]) ==
                             percentage_allowed.end())
      continue;
    const double share = denominator == 0.0
                             ? 0.0
                             : static_cast<double>(counts[order[i]]) /
                                   denominator;
    if (by_min_coverage && share < *options.variant_min_coverage) continue;
    if (by_max_coverage && share > *options.variant_max_coverage) continue;
    allowed.insert(order[i]);
  }

  EventLog result;
  result.attributes = log.attributes;
  for (auto& trace : kept) {
    if (allowed.find(variant_key(trace)) != allowed.end()) {
      result.traces.push_back(std::move(trace));
    }
  }
  if (options.max_cases && result.traces.size() > *options.max_cases)
    result.traces.resize(*options.max_cases);
  return result;
}

}  // namespace minera
