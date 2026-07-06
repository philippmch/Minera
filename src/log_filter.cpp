#include "minera/log_filter.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

// Projects a trace's events on the activity, resource, and — when `attribute`
// is non-null — generic-attribute constraints at once, dropping events outside
// a keep-set or inside a remove-set and preserving order. An event with no
// resource is never kept by a resource keep-set and never dropped by a
// resource remove-set, so a resource filter only ever acts on events that
// actually name a performer.
Trace project_events(const Trace& trace,
                     const std::unordered_set<std::string>& keep_activities,
                     const std::unordered_set<std::string>& remove_activities,
                     const std::unordered_set<std::string>& keep_resources,
                     const std::unordered_set<std::string>& remove_resources,
                     const AttributeMatcher* attribute) {
  Trace result;
  result.case_id = trace.case_id;
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
  // subtrace remains a distinct case.
  for (std::size_t i = 0; i < segments.size(); ++i) {
    segments[i].case_id = segments.size() == 1
                              ? trace.case_id
                              : trace.case_id + "#" + std::to_string(i + 1);
  }
  return segments;
}

std::string variant_key(const Trace& trace) {
  std::string key;
  for (const auto& event : trace.events) {
    key.push_back('\x1f');
    key += event.activity;
  }
  return key;
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

EventLog filter_event_log(const EventLog& log, const LogFilterOptions& options) {
  const auto keep = to_set(options.keep_activities);
  const auto remove = to_set(options.remove_activities);
  const auto keep_res = to_set(options.keep_resources);
  const auto remove_res = to_set(options.remove_resources);
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

  const bool by_between =
      !options.between_start.empty() || !options.between_end.empty();
  if (by_between &&
      (options.between_start.empty() || options.between_end.empty())) {
    throw std::invalid_argument(
        "between filter: both a start and an end activity are required");
  }

  const bool by_time = !options.time_from.empty() || !options.time_to.empty();
  const double time_from = parse_time_bound(
      options.time_from, -std::numeric_limits<double>::infinity());
  const double time_to = parse_time_bound(
      options.time_to, std::numeric_limits<double>::infinity());

  // Pass 1: constrain to the time range, then project activities and apply
  // per-trace structural filters.
  std::vector<Trace> kept;
  kept.reserve(log.traces.size());
  for (const auto& trace : log.traces) {
    const Trace* source = &trace;
    Trace timed;  // only populated in events mode, which rewrites the trace
    if (by_time) {
      switch (options.time_mode) {
        case TimeFilterMode::events: {
          timed.case_id = trace.case_id;
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

    Trace projected = project_events(*source, keep, remove, keep_res,
                                     remove_res, event_attribute);
    if (projected.events.empty()) continue;

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

      if (!starts.empty() &&
          starts.find(piece.events.front().activity) == starts.end()) {
        continue;
      }
      if (!ends.empty() &&
          ends.find(piece.events.back().activity) == ends.end()) {
        continue;
      }
      kept.push_back(std::move(piece));
    }
  }

  const bool by_frequency = options.min_variant_frequency > 1;
  const bool by_rank = options.top_variants != 0;
  if (!by_frequency && !by_rank) {
    EventLog result;
    result.traces = std::move(kept);
    return result;
  }

  // Pass 2: count variants on the surviving traces, then decide which variants
  // are allowed by frequency and rank.
  std::unordered_map<std::string, std::uint64_t> counts;
  std::vector<std::string> order;  // first-seen order, for deterministic ranking
  for (const auto& trace : kept) {
    auto key = variant_key(trace);
    if (counts.emplace(key, 0).second) order.push_back(key);
    ++counts[key];
  }

  std::unordered_set<std::string> allowed;
  if (by_rank) {
    std::stable_sort(order.begin(), order.end(),
                     [&](const std::string& a, const std::string& b) {
                       return counts[a] > counts[b];
                     });
    for (std::size_t i = 0; i < order.size() && i < options.top_variants; ++i) {
      if (counts[order[i]] >= options.min_variant_frequency) {
        allowed.insert(order[i]);
      }
    }
  } else {
    for (const auto& [key, count] : counts) {
      if (count >= options.min_variant_frequency) allowed.insert(key);
    }
  }

  EventLog result;
  for (auto& trace : kept) {
    if (allowed.find(variant_key(trace)) != allowed.end()) {
      result.traces.push_back(std::move(trace));
    }
  }
  return result;
}

}  // namespace minera
