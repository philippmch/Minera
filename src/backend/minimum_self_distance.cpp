#include "minera/minimum_self_distance.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <stdexcept>
#include <set>
#include <unordered_map>
#include <utility>
#include <string_view>

namespace minera {
namespace {

struct Segment {
  std::string activity;
  std::string case_id;
  std::size_t trace = 0;
  std::size_t left = 0;
  std::size_t right = 0;
  std::uint64_t distance = 0;
};

std::string selected_activity(const Event& event, std::string_view key) {
  if (key.empty() || key == "activity" || key == "concept:name")
    return event.activity;
  if (key == "resource" || key == "org:resource") return event.resource;
  if (const auto* value = event.attribute(key)) return *value;
  throw std::invalid_argument(
      "minimum self-distance event is missing activity attribute '" +
      std::string(key) + "'");
}

std::string selected_case_id(const Trace& trace, const Event& event,
                             std::string_view key) {
  if (key.empty() || key == "case_id" || key == "concept:name" ||
      key == "case:concept:name") {
    return event.case_id.empty() ? trace.case_id : event.case_id;
  }
  if (const auto* value = event.attribute(key)) return *value;
  if (const auto* value = trace.attribute(key)) return *value;
  if (key.rfind("case:", 0) == 0) {
    if (const auto* value = trace.attribute(key.substr(5))) return *value;
  } else if (const auto* value = trace.attribute("case:" + std::string(key))) {
    return *value;
  }
  throw std::invalid_argument(
      "minimum self-distance trace is missing case-id attribute '" +
      std::string(key) + "'");
}

}  // namespace

MinimumSelfDistances discover_minimum_self_distances(
    const EventLog& log, const MinimumSelfDistanceOptions& options) {
  if (options.variant != MinimumSelfDistanceVariant::kLog &&
      options.variant != MinimumSelfDistanceVariant::kPandas &&
      options.variant != MinimumSelfDistanceVariant::kPolars)
    throw std::invalid_argument(
        "minimum_self_distance_variant must be log, pandas, or polars");
  if (options.activity_key.empty() || options.case_id_key.empty()) {
    throw std::invalid_argument(
        "minimum self-distance attribute keys must not be empty");
  }
  std::vector<Segment> segments;
  std::map<std::string, std::uint64_t> minima;
  segments.reserve(log.event_count());

  for (std::size_t trace_index = 0; trace_index < log.traces.size();
       ++trace_index) {
    const auto& events = log.traces[trace_index].events;
    std::map<std::pair<std::string, std::string>,
             std::pair<std::size_t, std::size_t>> previous;
    std::map<std::string, std::size_t> case_positions;
    for (std::size_t index = 0; index < events.size(); ++index) {
      const auto& event = events[index];
      const auto activity = selected_activity(event, options.activity_key);
      const auto case_id = selected_case_id(log.traces[trace_index], event,
                                            options.case_id_key);
      const auto position = case_positions[case_id]++;
      const auto key = std::make_pair(case_id, activity);
      const auto found = previous.find(key);
      if (found != previous.end()) {
        const auto gap = position - found->second.first - 1;
        if (gap > std::numeric_limits<std::uint64_t>::max()) {
          throw std::overflow_error("minimum self-distance exceeds uint64_t");
        }
        const auto distance = static_cast<std::uint64_t>(gap);
        segments.push_back({activity, case_id, trace_index,
                            found->second.second, index, distance});
        const auto min_it = minima.find(activity);
        if (min_it == minima.end() || distance < min_it->second) {
          minima[activity] = distance;
        }
      }
      previous[key] = {position, index};
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
      const auto case_id = selected_case_id(log.traces[segment.trace],
                                            events[position],
                                            options.case_id_key);
      if (case_id == segment.case_id) {
        witnesses.insert(selected_activity(events[position],
                                            options.activity_key));
      }
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
