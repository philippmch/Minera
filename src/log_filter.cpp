#include "minera/log_filter.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace minera {
namespace {

std::unordered_set<std::string> to_set(const std::vector<std::string>& values) {
  return {values.begin(), values.end()};
}

// Drops events outside the keep-set and inside the remove-set, preserving order.
Trace project_activities(const Trace& trace,
                         const std::unordered_set<std::string>& keep,
                         const std::unordered_set<std::string>& remove) {
  Trace result;
  result.case_id = trace.case_id;
  for (const auto& event : trace.events) {
    if (!keep.empty() && keep.find(event.activity) == keep.end()) continue;
    if (!remove.empty() && remove.find(event.activity) != remove.end()) continue;
    result.events.push_back(event);
  }
  return result;
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

EventLog filter_event_log(const EventLog& log, const LogFilterOptions& options) {
  const auto keep = to_set(options.keep_activities);
  const auto remove = to_set(options.remove_activities);
  const auto starts = to_set(options.start_activities);
  const auto ends = to_set(options.end_activities);

  // Pass 1: project activities and apply per-trace structural filters.
  std::vector<Trace> kept;
  kept.reserve(log.traces.size());
  for (const auto& trace : log.traces) {
    Trace projected = project_activities(trace, keep, remove);
    if (projected.events.empty()) continue;

    const std::size_t length = projected.events.size();
    if (options.min_length != 0 && length < options.min_length) continue;
    if (options.max_length != 0 && length > options.max_length) continue;

    if (!starts.empty() &&
        starts.find(projected.events.front().activity) == starts.end()) {
      continue;
    }
    if (!ends.empty() &&
        ends.find(projected.events.back().activity) == ends.end()) {
      continue;
    }
    kept.push_back(std::move(projected));
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
