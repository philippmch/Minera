#include "minera/batches.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <tuple>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

struct IndexedEvent {
  BatchEvent event;
  std::string activity;
  std::string resource;
};

BatchType classify(const std::vector<BatchEvent>& events) {
  if (events.empty()) return BatchType::kConcurrent;
  const double first_start = events.front().start;
  const double first_complete = events.front().complete;
  bool equal_starts = true;
  bool equal_ends = true;
  bool simultaneous = true;
  for (const auto& event : events) {
    equal_starts = equal_starts && event.start == first_start;
    equal_ends = equal_ends && event.complete == first_complete;
    simultaneous = simultaneous && event.start == first_start &&
                   event.complete == first_complete;
  }
  if (simultaneous) return BatchType::kSimultaneous;
  if (equal_starts) return BatchType::kBatchingOnStart;
  if (equal_ends) return BatchType::kBatchingOnEnd;

  bool sequential = true;
  for (std::size_t i = 1; i < events.size(); ++i) {
    if (events[i - 1].complete != events[i].start) {
      sequential = false;
      break;
    }
  }
  return sequential ? BatchType::kSequential : BatchType::kConcurrent;
}

}  // namespace

BatchDiscovery discover_batches(const EventLog& log,
                                 BatchDiscoveryOptions options) {
  if (!std::isfinite(options.merge_distance_seconds) ||
      options.merge_distance_seconds < 0.0) {
    throw std::invalid_argument("batches: merge distance must be finite and non-negative");
  }
  if (options.min_batch_size < 2) {
    throw std::invalid_argument("batches: min_batch_size must be at least two");
  }

  std::map<std::pair<std::string, std::string>, std::vector<IndexedEvent>> grouped;
  for (std::size_t ti = 0; ti < log.traces.size(); ++ti) {
    const auto& trace = log.traces[ti];
    for (std::size_t ei = 0; ei < trace.events.size(); ++ei) {
      const auto& event = trace.events[ei];
      const auto complete = parse_timestamp(event.timestamp);
      if (!complete || !std::isfinite(*complete)) {
        throw std::invalid_argument("batches: event has an invalid completion timestamp");
      }
      double start = *complete;
      if (!options.start_timestamp_attribute.empty()) {
        const auto* value = event.attribute(options.start_timestamp_attribute);
        if (value == nullptr || value->empty()) {
          throw std::invalid_argument("batches: configured start timestamp is missing");
        }
        const auto parsed = parse_timestamp(*value);
        if (!parsed || !std::isfinite(*parsed)) {
          throw std::invalid_argument("batches: configured start timestamp is invalid");
        }
        start = *parsed;
      }
      if (!std::isfinite(start)) {
        throw std::invalid_argument("batches: non-finite start timestamp");
      }
      if (start > *complete) {
        throw std::invalid_argument("batches: start timestamp is after completion");
      }
      BatchEvent batch_event{start, *complete, trace.case_id, ti, ei};
      grouped[{event.activity, event.resource}].push_back(
          IndexedEvent{std::move(batch_event), event.activity, event.resource});
    }
  }

  std::vector<BatchGroup> result;
  for (auto& [key, values] : grouped) {
    std::sort(values.begin(), values.end(), [](const IndexedEvent& a,
                                               const IndexedEvent& b) {
      return std::tie(a.event.start, a.event.complete, a.event.trace_index,
                      a.event.event_index) <
             std::tie(b.event.start, b.event.complete, b.event.trace_index,
                      b.event.event_index);
    });
    std::vector<Batch> batches;
    std::size_t cursor = 0;
    while (cursor < values.size()) {
      std::vector<BatchEvent> component;
      component.push_back(values[cursor].event);
      double envelope_end = values[cursor].event.complete;
      ++cursor;
      while (cursor < values.size()) {
        const auto& candidate = values[cursor].event;
        const double gap = candidate.start - envelope_end;
        // Strict overlap is transitive through the evolving envelope; a
        // touching or near interval is merged by the configured distance.
        if (candidate.start < envelope_end || gap <= options.merge_distance_seconds) {
          component.push_back(candidate);
          envelope_end = std::max(envelope_end, candidate.complete);
          ++cursor;
        } else {
          break;
        }
      }
      if (component.size() < options.min_batch_size) continue;
      Batch batch;
      batch.start = component.front().start;
      batch.complete = component.front().complete;
      for (const auto& event : component) {
        batch.start = std::min(batch.start, event.start);
        batch.complete = std::max(batch.complete, event.complete);
      }
      batch.events = std::move(component);
      batch.type = classify(batch.events);
      batches.push_back(std::move(batch));
    }
    if (batches.empty()) continue;
    BatchGroup group;
    group.activity = key.first;
    group.resource = key.second;
    group.batch_count = batches.size();
    for (const auto& batch : batches) group.event_count += batch.events.size();
    group.batches = std::move(batches);
    result.push_back(std::move(group));
  }
  std::sort(result.begin(), result.end(), [](const BatchGroup& a,
                                             const BatchGroup& b) {
    if (a.batch_count != b.batch_count) return a.batch_count > b.batch_count;
    return std::tie(a.activity, a.resource) > std::tie(b.activity, b.resource);
  });
  return BatchDiscovery{std::move(options), std::move(result)};
}

}  // namespace minera
