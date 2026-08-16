#include "minera/minimum_self_distance.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <stdexcept>
#include <set>
#include <unordered_map>
#include <utility>

namespace minera {
namespace {

struct Segment {
  std::string activity;
  std::size_t trace = 0;
  std::size_t left = 0;
  std::size_t right = 0;
  std::uint64_t distance = 0;
};

}  // namespace

MinimumSelfDistances discover_minimum_self_distances(
    const EventLog& log, const MinimumSelfDistanceOptions& options) {
  if (options.variant != MinimumSelfDistanceVariant::kLog &&
      options.variant != MinimumSelfDistanceVariant::kPandas &&
      options.variant != MinimumSelfDistanceVariant::kPolars)
    throw std::invalid_argument(
        "minimum_self_distance_variant must be log, pandas, or polars");
  std::vector<Segment> segments;
  std::map<std::string, std::uint64_t> minima;
  segments.reserve(log.event_count());

  for (std::size_t trace_index = 0; trace_index < log.traces.size();
       ++trace_index) {
    const auto& events = log.traces[trace_index].events;
    std::unordered_map<std::string, std::size_t> previous;
    previous.reserve(events.size());
    for (std::size_t index = 0; index < events.size(); ++index) {
      const auto& activity = events[index].activity;
      const auto found = previous.find(activity);
      if (found != previous.end()) {
        const auto gap = index - found->second - 1;
        if (gap > std::numeric_limits<std::uint64_t>::max()) {
          throw std::overflow_error("minimum self-distance exceeds uint64_t");
        }
        const auto distance = static_cast<std::uint64_t>(gap);
        segments.push_back({activity, trace_index, found->second, index, distance});
        const auto min_it = minima.find(activity);
        if (min_it == minima.end() || distance < min_it->second) {
          minima[activity] = distance;
        }
      }
      previous[activity] = index;
    }
  }

  std::map<std::string, std::set<std::string>> witness_sets;
  for (const auto& segment : segments) {
    const auto min_it = minima.find(segment.activity);
    if (min_it == minima.end() || min_it->second != segment.distance) continue;
    const auto& events = log.traces[segment.trace].events;
    auto& witnesses = witness_sets[segment.activity];
    for (std::size_t position = segment.left + 1; position < segment.right;
         ++position) {
      witnesses.insert(events[position].activity);
    }
  }

  MinimumSelfDistances result;
  result.reserve(minima.size());
  for (const auto& [activity, distance] : minima) {
    std::vector<std::string> witnesses(witness_sets[activity].begin(),
                                       witness_sets[activity].end());
    if (distance == 0) witnesses.clear();
    result.push_back({activity, distance, std::move(witnesses)});
  }
  return result;
}

}  // namespace minera
