#include "minera/transition_system.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>

namespace minera {
namespace {

using Payload = std::vector<std::string>;

Payload make_payload(const std::vector<std::string>& activities,
                     std::size_t begin, std::size_t end,
                     TransitionView view) {
  Payload payload(activities.begin() + static_cast<std::ptrdiff_t>(begin),
                  activities.begin() + static_cast<std::ptrdiff_t>(end));
  if (view == TransitionView::kSequence) return payload;
  std::sort(payload.begin(), payload.end());
  if (view == TransitionView::kSet) {
    payload.erase(std::unique(payload.begin(), payload.end()), payload.end());
  }
  return payload;
}

struct EdgeKey {
  Payload source;
  Payload target;
  std::string label;
  friend bool operator<(const EdgeKey& a, const EdgeKey& b) {
    if (a.source != b.source) return a.source < b.source;
    if (a.target != b.target) return a.target < b.target;
    return a.label < b.label;
  }
};

}  // namespace

TransitionSystem discover_transition_system(const EventLog& event_log,
                                             const TransitionSystemOptions& options) {
  if (options.direction != TransitionDirection::kForward &&
      options.direction != TransitionDirection::kBackward) {
    throw std::invalid_argument("transition-system direction is invalid");
  }
  if (options.view != TransitionView::kSequence &&
      options.view != TransitionView::kSet &&
      options.view != TransitionView::kMultiset) {
    throw std::invalid_argument("transition-system view is invalid");
  }

  TransitionSystem result;
  result.options_ = options;

  // Build semantic payload maps first. std::map gives deterministic canonical
  // ordering independent of trace/input order; ids are assigned in that order.
  std::map<Payload, TransitionSystemState> states;
  std::map<EdgeKey, TransitionSystemTransition> edges;

  for (std::size_t trace_index = 0; trace_index < event_log.traces.size(); ++trace_index) {
    const auto& trace = event_log.traces[trace_index];
    const std::size_t n = trace.events.size();
    if (n == 0) continue;  // PM4Py contributes no path for an empty trace.

    std::vector<std::string> activities;
    activities.reserve(n);
    for (const auto& event : trace.events) activities.push_back(event.activity);

    std::vector<Payload> trace_states;
    trace_states.reserve(n + 1);
    for (std::size_t i = 0; i <= n; ++i) {
      std::size_t begin = 0;
      std::size_t end = 0;
      if (options.direction == TransitionDirection::kForward) {
        begin = i;
        const std::size_t remaining = n - i;
        const std::size_t width = std::min(options.window, remaining);
        end = i + width;
      } else {
        end = i;
        begin = (i > options.window) ? i - options.window : 0;
      }
      trace_states.push_back(make_payload(activities, begin, end, options.view));
      auto& state = states[trace_states.back()];
      if (i == 0) ++state.start_count;
      if (i == n) ++state.end_count;
    }

    for (std::size_t i = 0; i < n; ++i) {
      const TransitionOccurrence occurrence{trace_index, i};
      auto& source = states[trace_states[i]];
      auto& target = states[trace_states[i + 1]];
      if (options.include_data) {
        source.outgoing_occurrences.push_back(occurrence);
        target.incoming_occurrences.push_back(occurrence);
      }
      EdgeKey key{trace_states[i], trace_states[i + 1], activities[i]};
      auto& edge = edges[key];
      ++edge.count;
      if (options.include_data) edge.occurrences.push_back(occurrence);
    }
  }

  result.states_.reserve(states.size());
  std::map<Payload, std::size_t> ids;
  for (auto& [payload, state] : states) {
    state.id = result.states_.size();
    ids.emplace(payload, state.id);
    state.payload = payload;
    result.states_.push_back(std::move(state));
  }

  result.transitions_.reserve(edges.size());
  for (auto& [key, edge] : edges) {
    edge.source = ids.at(key.source);
    edge.target = ids.at(key.target);
    edge.label = key.label;
    result.transitions_.push_back(std::move(edge));
  }
  return result;
}

TransitionSystem discover_transition_system(const EventLog& event_log,
                                             TransitionDirection direction,
                                             std::size_t window,
                                             TransitionView view,
                                             bool include_data) {
  TransitionSystemOptions options;
  options.direction = direction;
  options.view = view;
  options.window = window;
  options.include_data = include_data;
  return discover_transition_system(event_log, options);
}

}  // namespace minera
