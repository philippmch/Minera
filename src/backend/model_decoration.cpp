#include "minera/model_decoration.hpp"

#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "minera/conformance.hpp"
#include "minera/timestamp.hpp"

namespace minera {

namespace {

bool standard_activity_attribute(std::string_view key) {
  return key.empty() || key == "concept:name" || key == "activity";
}

const std::string& selected_activity(const Event& event,
                                     std::string_view activity_attribute) {
  if (standard_activity_attribute(activity_attribute)) return event.activity;
  if (const auto* value = event.attribute(activity_attribute)) return *value;
  throw std::invalid_argument(
      "model decoration: event is missing activity attribute '" +
      std::string(activity_attribute) + "'");
}

bool standard_timestamp_attribute(std::string_view key) {
  return key.empty() || key == "time:timestamp" || key == "timestamp";
}

const std::string& selected_timestamp(const Event& event,
                                      std::string_view timestamp_attribute) {
  if (standard_timestamp_attribute(timestamp_attribute)) return event.timestamp;
  if (const auto* value = event.attribute(timestamp_attribute)) return *value;
  throw std::invalid_argument(
      "model decoration: event is missing timestamp attribute '" +
      std::string(timestamp_attribute) + "'");
}

}  // namespace

ModelDecoration decorate_model(const EventLog& log, const PetriNet& net,
                               std::string_view activity_attribute,
                               std::string_view timestamp_attribute) {
  ModelDecoration result;
  result.transition_counts.assign(net.transitions().size(), 0);
  result.place_counts.assign(net.place_count(), 0);
  result.trace_count = log.traces.size();

  // Replay each distinct variant once; every trace of the variant fires the
  // same transitions, only its instants differ.
  std::vector<std::vector<std::string>> variants;
  std::unordered_map<std::string, std::size_t> variant_ids;
  std::vector<std::size_t> variant_of(log.traces.size());
  for (std::size_t i = 0; i < log.traces.size(); ++i) {
    std::string key;
    for (const auto& event : log.traces[i].events) {
      key.push_back('\x1f');
      key += selected_activity(event, activity_attribute);
    }
    const auto [it, inserted] =
        variant_ids.emplace(std::move(key), variants.size());
    if (inserted) {
      std::vector<std::string> activities;
      activities.reserve(log.traces[i].events.size());
      for (const auto& event : log.traces[i].events) {
        activities.push_back(selected_activity(event, activity_attribute));
      }
      variants.push_back(std::move(activities));
    }
    variant_of[i] = it->second;
  }
  const auto firing = replay_firing_sequences(net, variants);

  // Wait samples per place, reduced to stats at the end.
  std::vector<std::vector<double>> waits(net.place_count());

  // Token instants resting in each place, oldest first. nullopt marks a token
  // produced by an untimed event; it moves like any other but yields no wait.
  std::vector<std::deque<std::optional<double>>> tokens(net.place_count());

  for (std::size_t i = 0; i < log.traces.size(); ++i) {
    const auto& sequence = firing[variant_of[i]];
    if (!sequence) continue;  // deviating trace: not a real path through the net
    ++result.decorated_traces;
    const auto& trace = log.traces[i];

    for (auto& queue : tokens) queue.clear();

    // The initial token carries the case's first instant, so its wait in the
    // source measures delay before the process starts moving, not the span
    // back to some epoch.
    std::optional<double> first_instant;
    for (const auto& event : trace.events) {
      first_instant = parse_timestamp(
          selected_timestamp(event, timestamp_attribute));
      if (first_instant) break;
    }
    std::optional<double> last_instant;
    for (auto it = trace.events.rbegin(); it != trace.events.rend(); ++it) {
      last_instant = parse_timestamp(
          selected_timestamp(*it, timestamp_attribute));
      if (last_instant) break;
    }
    tokens[net.source()].push_back(first_instant);

    // One firing list per consumed event — fired at that event's instant, or
    // untimed (yielding no wait samples) when the event has none — then one
    // for the routing to the sink, at the case's last instant.
    for (std::size_t step = 0; step < sequence->size(); ++step) {
      const auto now = step < trace.events.size()
                           ? parse_timestamp(selected_timestamp(
                                 trace.events[step], timestamp_attribute))
                           : last_instant;
      for (const auto transition_id : (*sequence)[step]) {
        ++result.transition_counts[transition_id];
        const auto& transition = net.transitions()[transition_id];
        for (const auto place : transition.inputs) {
          ++result.place_counts[place];
          // A clean replay never fires without its tokens; the guard is
          // belt-and-braces against an inconsistent sequence.
          if (tokens[place].empty()) continue;
          const auto produced = tokens[place].front();
          tokens[place].pop_front();
          if (produced && now && *now >= *produced) {
            waits[place].push_back(*now - *produced);
          }
        }
        for (const auto place : transition.outputs) {
          tokens[place].push_back(now);
        }
      }
    }
  }

  result.place_waits.reserve(net.place_count());
  for (auto& samples : waits) {
    result.place_waits.push_back(summarize_durations(std::move(samples)));
  }
  return result;
}

}  // namespace minera
