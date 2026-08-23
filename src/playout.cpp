#include "minera/playout.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "minera/timestamp.hpp"

namespace minera {

EventLog playout(const PetriNet& net, const PlayoutOptions& options) {
  const auto& transitions = net.transitions();
  std::mt19937_64 rng(options.seed);

  EventLog log;
  log.traces.reserve(options.traces);

  std::vector<std::uint32_t> marking(net.place_count(), 0);
  std::vector<std::size_t> enabled;

  for (std::size_t c = 0; c < options.traces; ++c) {
    Trace trace;
    trace.case_id = "case_" + std::to_string(c + 1);
    double now = options.start + static_cast<double>(c) * options.case_interval;

    std::fill(marking.begin(), marking.end(), 0);
    marking[net.source()] = 1;
    std::uint64_t tokens = 1;

    for (std::size_t step = 0; step < options.max_steps; ++step) {
      // The final marking of a workflow net: the single token rests on the
      // sink. Reaching it completes the case even if transitions could still
      // fire (e.g. a loop's redo path).
      if (tokens == 1 && marking[net.sink()] == 1) {
        break;
      }

      enabled.clear();
      for (std::size_t t = 0; t < transitions.size(); ++t) {
        if (petri_transition_enabled(transitions[t], marking) &&
            (!transitions[t].inputs.empty() ||
             transitions[t].has_extended_arcs())) {
          enabled.push_back(t);
        }
      }
      if (enabled.empty()) {
        break;  // deadlock (only possible on an unsound net)
      }

      const auto pick = std::uniform_int_distribution<std::size_t>(
          0, enabled.size() - 1)(rng);
      const auto& fired = transitions[enabled[pick]];
      fire_petri_transition(fired, marking);
      tokens = 0;
      for (const auto value : marking) tokens += value;

      if (!fired.silent()) {
        Event event;
        event.case_id = trace.case_id;
        event.activity = net.activities()[*fired.activity];
        event.timestamp = format_timestamp(now);
        event.input_order = trace.events.size();
        trace.events.push_back(std::move(event));
        now += options.event_interval;
      }
    }

    log.traces.push_back(std::move(trace));
  }

  return log;
}

EventLog stochastic_playout(const PetriNet& net,
                            const StochasticPlayoutOptions& options) {
  const auto& transitions = net.transitions();
  const auto places = net.place_count();
  if (net.source() >= places || net.sink() >= places) {
    throw std::invalid_argument("Petri net source and sink must be valid places");
  }
  if (!std::isfinite(options.start) ||
      !std::isfinite(options.case_interval) ||
      !std::isfinite(options.event_interval)) {
    throw std::invalid_argument("stochastic playout timestamps must be finite");
  }
  if (options.case_interval < 0.0 || options.event_interval < 0.0) {
    throw std::invalid_argument(
        "stochastic playout intervals must be non-negative");
  }
  if (!options.transition_weights.empty() &&
      options.transition_weights.size() != transitions.size()) {
    throw std::invalid_argument(
        "transition_weights must be empty or match the transition count");
  }
  for (const double weight : options.transition_weights) {
    if (!std::isfinite(weight) || weight < 0.0) {
      throw std::invalid_argument(
          "transition_weights must be finite and non-negative");
    }
  }
  for (const auto& transition : transitions) {
    for (const auto place : transition.inputs) {
      if (place >= places) {
        throw std::invalid_argument("Petri transition input place is invalid");
      }
    }
    for (const auto place : transition.outputs) {
      if (place >= places) {
        throw std::invalid_argument("Petri transition output place is invalid");
      }
    }
    for (const auto place : transition.reset_inputs) {
      if (place >= places) throw std::invalid_argument("Petri transition reset place is invalid");
    }
    for (const auto place : transition.inhibitor_inputs) {
      if (place >= places) throw std::invalid_argument("Petri transition inhibitor place is invalid");
    }
    if (transition.activity && *transition.activity >= net.activities().size()) {
      throw std::invalid_argument("Petri transition activity is invalid");
    }
  }

  std::mt19937_64 rng(options.seed);
  EventLog log;
  log.traces.reserve(options.traces);
  std::vector<std::uint64_t> marking(places, 0);
  std::vector<std::size_t> enabled;
  const auto is_final = [&](const std::vector<std::uint64_t>& candidate) {
    for (std::size_t p = 0; p < candidate.size(); ++p) {
      if (candidate[p] != (p == net.sink() ? 1U : 0U)) return false;
    }
    return true;
  };

  for (std::size_t c = 0; c < options.traces; ++c) {
    Trace trace;
    trace.case_id = "case_" + std::to_string(c + 1);
    double now = options.start + static_cast<double>(c) * options.case_interval;
    std::fill(marking.begin(), marking.end(), 0);
    marking[net.source()] = 1;

    for (std::size_t step = 0; step < options.max_steps; ++step) {
      if (options.stop_at_final && is_final(marking)) break;

      enabled.clear();
      for (std::size_t t = 0; t < transitions.size(); ++t) {
        const auto& transition = transitions[t];
        if ((transition.inputs.empty() && !transition.has_extended_arcs()) ||
            !petri_transition_enabled(transition, marking)) continue;
        enabled.push_back(t);
      }
      if (enabled.empty()) break;

      std::size_t selected = 0;
      if (options.transition_weights.empty()) {
        selected = enabled[std::uniform_int_distribution<std::size_t>(
            0, enabled.size() - 1)(rng)];
      } else {
        double maximum = 0.0;
        for (const auto t : enabled)
          maximum = std::max(maximum, options.transition_weights[t]);
        if (!(maximum > 0.0)) {
          selected = enabled[std::uniform_int_distribution<std::size_t>(
              0, enabled.size() - 1)(rng)];
        } else {
          std::vector<double> weights;
          weights.reserve(enabled.size());
          for (const auto t : enabled)
            weights.push_back(options.transition_weights[t] / maximum);
          const auto index =
              std::discrete_distribution<std::size_t>(weights.begin(), weights.end())(rng);
          selected = enabled[index];
        }
      }

      const auto& fired = transitions[selected];
      fire_petri_transition(fired, marking);
      if (!fired.silent()) {
        Event event;
        event.case_id = trace.case_id;
        event.activity = net.activities()[*fired.activity];
        event.timestamp = format_timestamp(now);
        event.input_order = trace.events.size();
        trace.events.push_back(std::move(event));
        now += options.event_interval;
      }
    }

    log.traces.push_back(std::move(trace));
  }
  return log;
}

namespace {

// One breadth-first search state: a reachable marking and the visible trace
// that led to it. Two silent routings of the same trace share a state.
struct EnumerationState {
  std::vector<std::uint32_t> marking;
  std::vector<std::size_t> events;  // fired visible transitions, in order
};

std::string state_key(const EnumerationState& state) {
  std::string key;
  key.reserve(state.marking.size() * 2 + state.events.size() * 2);
  for (const auto tokens : state.marking) {
    key += std::to_string(tokens);
    key += ',';
  }
  key += '|';
  for (const auto transition : state.events) {
    key += std::to_string(transition);
    key += ',';
  }
  return key;
}

}  // namespace

EventLog extensive_playout(const PetriNet& net,
                           const ExtensivePlayoutOptions& options) {
  const auto& transitions = net.transitions();
  if (net.place_count() == 0 || net.source() >= net.place_count() ||
      net.sink() >= net.place_count()) {
    throw std::invalid_argument(
        "extensive playout requires valid workflow source and sink places");
  }
  const auto resolve_marking = [&](const std::vector<std::uint32_t>& requested,
                                   std::size_t boundary,
                                   const char* label) {
    if (!requested.empty() && requested.size() != net.place_count()) {
      throw std::invalid_argument(std::string("extensive playout ") + label +
                                  " marking must match the place count");
    }
    std::vector<std::uint32_t> marking(net.place_count(), 0);
    if (requested.empty()) {
      marking[boundary] = 1;
    } else {
      marking = requested;
    }
    return marking;
  };
  const auto initial_marking =
      resolve_marking(options.initial_marking, net.source(), "initial");
  const auto final_marking =
      resolve_marking(options.final_marking, net.sink(), "final");

  const auto is_final = [&](const std::vector<std::uint32_t>& marking) {
    return marking == final_marking;
  };

  EventLog log;
  // Distinct traces are distinct label sequences: two transitions sharing a
  // label (possible in an imported net) must not yield the same case twice.
  std::unordered_set<std::string> emitted;
  const auto emit = [&](const std::vector<std::size_t>& events) {
    std::string labels;
    for (const auto transition : events) {
      labels += std::to_string(*transitions[transition].activity);
      labels += ',';
    }
    if (!emitted.insert(std::move(labels)).second) return;
    Trace trace;
    trace.case_id = "case_" + std::to_string(log.traces.size() + 1);
    double now = options.start +
                 static_cast<double>(log.traces.size()) * options.case_interval;
    for (const auto transition : events) {
      Event event;
      event.case_id = trace.case_id;
      event.activity = net.activities()[*transitions[transition].activity];
      event.timestamp = format_timestamp(now);
      event.input_order = trace.events.size();
      trace.events.push_back(std::move(event));
      now += options.event_interval;
    }
    log.traces.push_back(std::move(trace));
  };

  std::deque<EnumerationState> queue;
  std::unordered_set<std::string> visited;

  EnumerationState initial;
  initial.marking = initial_marking;
  visited.insert(state_key(initial));
  if (is_final(initial.marking)) emit(initial.events);  // degenerate net
  queue.push_back(std::move(initial));

  while (!queue.empty() && log.traces.size() < options.max_traces &&
         visited.size() < options.max_states) {
    const auto state = std::move(queue.front());
    queue.pop_front();

    for (std::size_t t = 0; t < transitions.size(); ++t) {
      const auto& transition = transitions[t];
      if (transition.inputs.empty() && !transition.has_extended_arcs()) continue;
      if (!transition.silent() && state.events.size() >= options.max_length) {
        continue;  // would exceed the visible-length bound
      }
      if (!petri_transition_enabled(transition, state.marking)) continue;

      EnumerationState next;
      next.marking = state.marking;
      fire_petri_transition(transition, next.marking);
      next.events = state.events;
      if (!transition.silent()) next.events.push_back(t);

      if (!visited.insert(state_key(next)).second) continue;
      if (is_final(next.marking)) {
        emit(next.events);
        if (log.traces.size() >= options.max_traces) break;
      }
      queue.push_back(std::move(next));
    }
  }

  return log;
}

}  // namespace minera
