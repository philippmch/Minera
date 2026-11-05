#include "minera/playout.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "minera/random.hpp"
#include "minera/timestamp.hpp"

namespace minera {

namespace {

// The reference envelope of algo.simulation.playout.petri_net BASIC_PLAYOUT:
// accepted cases are "0", "1", ... and every event is one second after the
// previous one, counting on across cases from epoch 10,000,000.
constexpr double kReferencePlayoutEpoch = 10000000.0;

struct BasicPlayoutSetup {
  std::vector<std::uint32_t> initial_marking;
  std::vector<std::uint32_t> final_marking;
};

BasicPlayoutSetup basic_playout_setup(const PetriNet& net,
                                      const PlayoutOptions& options) {
  if (net.place_count() == 0 || net.source() >= net.place_count() ||
      net.sink() >= net.place_count()) {
    throw std::invalid_argument(
        "playout requires valid workflow source and sink places");
  }
  if (!std::isfinite(options.start) ||
      !std::isfinite(options.case_interval) ||
      !std::isfinite(options.event_interval)) {
    throw std::invalid_argument("playout timestamps must be finite");
  }
  if (options.case_interval < 0.0 || options.event_interval < 0.0) {
    throw std::invalid_argument("playout intervals must be non-negative");
  }
  const auto resolve = [&](const std::vector<std::uint32_t>& requested,
                           std::size_t boundary, const char* label) {
    if (!requested.empty() && requested.size() != net.place_count()) {
      throw std::invalid_argument(std::string("playout ") + label +
                                  " marking must match the place count");
    }
    std::vector<std::uint32_t> marking(net.place_count(), 0);
    if (requested.empty()) marking[boundary] = 1;
    else marking = requested;
    return marking;
  };
  return {resolve(options.initial_marking, net.source(), "initial"),
          resolve(options.final_marking, net.sink(), "final")};
}

// The sampling process the reference follows (validated by distribution, not
// draw for draw: the reference picks from a set of transition objects, whose
// order follows object identity):
//  - a case stops once it holds max_steps visible events;
//  - otherwise every enabled transition, a transition without input places
//    included, is equally likely, and in an accepted final marking (exact,
//    or covering it with fm_leq_accepted) "stop" is one more equally likely
//    option, so a case at the final marking with n enabled transitions stops
//    with probability 1/(n+1);
//  - a case with nothing enabled stops (deadlock).
// A case is accepted when it ends in an accepted final marking. Without
// add_only_if_fm_is_reached every case is kept; with it, rejected cases are
// discarded and sampling goes on until `traces` cases are accepted, unless
// none of the first `traces` attempts was accepted, in which case the log is
// empty (both as the reference does). Draws come from rng::PythonRandom.
template <class Emit>
void run_basic_playout(const PetriNet& net, const PlayoutOptions& options,
                       Emit&& emit) {
  const auto& transitions = net.transitions();
  const auto setup = basic_playout_setup(net, options);
  const auto& final_marking = setup.final_marking;
  const auto final_reached = [&](const std::vector<std::uint32_t>& marking) {
    if (marking == final_marking) return true;
    if (!options.fm_leq_accepted) return false;
    for (std::size_t place = 0; place < marking.size(); ++place)
      if (final_marking[place] > marking[place]) return false;
    return true;
  };
  // Safety bounds the reference does not have: it would spin forever on a
  // silent livelock or on an add_only request whose final marking is never
  // reached after a first success. Both raise instead of returning less.
  const std::size_t max_firings =
      options.max_firings_per_trace != 0
          ? options.max_firings_per_trace
          : std::max<std::size_t>(1000000, 1000 * (options.max_steps + 1));
  const std::size_t max_attempts =
      options.max_attempts != 0 ? options.max_attempts
                                : 10000 * options.traces + 10000;

  rng::PythonRandom random(options.seed);
  std::vector<std::uint32_t> marking;
  std::vector<std::size_t> enabled;
  std::vector<std::size_t> fired;
  std::size_t accepted = 0;
  std::size_t attempts = 0;
  while (accepted < options.traces) {
    if (options.add_only_if_fm_is_reached && accepted == 0 &&
        attempts >= options.traces)
      break;
    if (attempts >= max_attempts)
      throw std::runtime_error(
          "playout: " + std::to_string(attempts) + " cases generated, " +
          std::to_string(accepted) +
          " reached the final marking; raise max_attempts or check the "
          "final marking");
    ++attempts;
    marking = setup.initial_marking;
    fired.clear();
    std::size_t visible = 0;
    while (visible < options.max_steps) {
      enabled.clear();
      for (std::size_t t = 0; t < transitions.size(); ++t)
        if (petri_transition_enabled(transitions[t], marking))
          enabled.push_back(t);
      const bool can_stop = final_reached(marking);
      const std::size_t options_count = enabled.size() + (can_stop ? 1 : 0);
      if (options_count == 0) break;  // deadlock
      const auto pick = static_cast<std::size_t>(random.randbelow(options_count));
      if (pick == enabled.size()) break;  // stop in the final marking
      const auto& transition = transitions[enabled[pick]];
      fire_petri_transition(transition, marking);
      fired.push_back(enabled[pick]);
      if (!transition.silent()) ++visible;
      if (fired.size() >= max_firings)
        throw std::runtime_error(
            "playout: a case fired " + std::to_string(fired.size()) +
            " transitions with " + std::to_string(visible) +
            " visible events (silent livelock); raise max_firings_per_trace");
    }
    if (options.add_only_if_fm_is_reached && !final_reached(marking)) continue;
    emit(setup.initial_marking, fired);
    ++accepted;
  }
}

}  // namespace

EventLog playout(const PetriNet& net, const PlayoutOptions& options) {
  const auto& transitions = net.transitions();
  EventLog log;
  log.traces.reserve(options.traces);
  double clock = kReferencePlayoutEpoch;
  run_basic_playout(net, options, [&](const std::vector<std::uint32_t>&,
                                      const std::vector<std::size_t>& fired) {
    const auto index = log.traces.size();
    Trace trace;
    double now = clock;
    if (options.add_envelope) {
      trace.case_id = "case_" + std::to_string(index + 1);
      now = options.start + static_cast<double>(index) * options.case_interval;
    } else {
      trace.case_id = std::to_string(index);
    }
    for (const auto t : fired) {
      const auto& transition = transitions[t];
      if (transition.silent()) continue;
      Event event;
      event.case_id = trace.case_id;
      event.activity = net.activities()[*transition.activity];
      event.timestamp = format_timestamp(now);
      event.input_order = trace.events.size();
      trace.events.push_back(std::move(event));
      now += options.add_envelope ? options.event_interval : 1.0;
    }
    if (!options.add_envelope) clock = now;
    log.traces.push_back(std::move(trace));
  });
  return log;
}

PlayoutVisitedResult playout_visited(const PetriNet& net,
                                     const PlayoutOptions& options) {
  const auto& transitions = net.transitions();
  PlayoutVisitedResult result;
  result.traces.reserve(options.traces);
  run_basic_playout(net, options, [&](const std::vector<std::uint32_t>& initial,
                                      const std::vector<std::size_t>& fired) {
    PlayoutVisitedTrace visited;
    visited.reserve(fired.size() * 2 + 1);
    auto marking = initial;
    visited.push_back({PlayoutVisitedElementKind::MARKING, marking, 0});
    for (const auto t : fired) {
      fire_petri_transition(transitions[t], marking);
      visited.push_back({PlayoutVisitedElementKind::TRANSITION, {}, t});
      visited.push_back({PlayoutVisitedElementKind::MARKING, marking, 0});
    }
    result.traces.push_back(std::move(visited));
  });
  return result;
}

namespace {

void validate_stochastic_net(const PetriNet& net) {
  const auto places = net.place_count();
  if (places == 0 || net.source() >= places || net.sink() >= places)
    throw std::invalid_argument("stochastic playout net has invalid workflow places");
  for (const auto& transition : net.transitions()) {
    for (const auto place : transition.inputs)
      if (place >= places) throw std::invalid_argument("stochastic playout input place is invalid");
    for (const auto place : transition.outputs)
      if (place >= places) throw std::invalid_argument("stochastic playout output place is invalid");
    for (const auto place : transition.reset_inputs)
      if (place >= places) throw std::invalid_argument("stochastic playout reset place is invalid");
    for (const auto place : transition.inhibitor_inputs)
      if (place >= places) throw std::invalid_argument("stochastic playout inhibitor place is invalid");
    if (transition.activity && *transition.activity >= net.activities().size())
      throw std::invalid_argument("stochastic playout transition activity is invalid");
  }
}

std::vector<std::uint32_t> stochastic_marking(
    const PetriNet& net, const std::vector<std::uint32_t>& requested,
    std::size_t boundary, const char* label) {
  if (!requested.empty() && requested.size() != net.place_count())
    throw std::invalid_argument(std::string("stochastic playout ") + label +
                                " marking must match the place count");
  std::vector<std::uint32_t> marking(net.place_count(), 0);
  if (requested.empty()) marking[boundary] = 1;
  else marking = requested;
  return marking;
}

struct ReplayState {
  std::vector<std::uint32_t> marking;
  std::size_t event_index = 0;
  std::vector<std::size_t> path;
};

struct ReplayResult {
  std::vector<std::size_t> path;
  std::vector<std::size_t> touched;
  bool complete = false;
};

std::string replay_state_key(const ReplayState& state) {
  std::string key = std::to_string(state.event_index) + '|';
  for (const auto token : state.marking) {
    key += std::to_string(token);
    key += ',';
  }
  return key;
}

ReplayResult replay_trace(
    const PetriNet& net, const Trace& trace,
    const std::vector<std::uint32_t>& initial,
    const std::vector<std::uint32_t>& final, std::size_t max_states) {
  std::deque<ReplayState> queue;
  std::unordered_set<std::string> visited;
  std::vector<std::size_t> selected_path;
  bool complete = false;
  queue.push_back({initial, 0, {}});
  visited.insert(replay_state_key(queue.front()));
  ReplayState best = queue.front();
  while (!queue.empty() && visited.size() <= max_states) {
    auto state = std::move(queue.front());
    queue.pop_front();
    if (state.event_index > best.event_index ||
        (state.event_index == best.event_index &&
         state.path.size() > best.path.size())) {
      best = state;
    }
    if (state.event_index == trace.events.size() && state.marking == final) {
      selected_path = std::move(state.path);
      complete = true;
      break;
    }
    for (std::size_t transition_index = 0;
         transition_index < net.transitions().size(); ++transition_index) {
      const auto& transition = net.transitions()[transition_index];
      if (transition.inputs.empty() && !transition.has_extended_arcs()) continue;
      if (!petri_transition_enabled(transition, state.marking)) continue;
      ReplayState next = state;
      if (!transition.silent()) {
        if (state.event_index >= trace.events.size()) continue;
        const auto activity = *transition.activity;
        if (activity >= net.activities().size() ||
            net.activities()[activity] != trace.events[state.event_index].activity)
          continue;
        ++next.event_index;
      }
      fire_petri_transition(transition, next.marking);
      next.path.push_back(transition_index);
      if (visited.insert(replay_state_key(next)).second)
        queue.push_back(std::move(next));
    }
  }

  // Re-run only the selected deterministic path. This deliberately excludes
  // transitions enabled in discarded BFS branches from the fitted rates.
  ReplayResult result;
  result.path = complete ? std::move(selected_path) : std::move(best.path);
  result.complete = complete;
  std::vector<std::uint32_t> marking = initial;
  for (const auto transition_index : result.path) {
    for (std::size_t i = 0; i < net.transitions().size(); ++i) {
      const auto& candidate = net.transitions()[i];
      if (!(candidate.inputs.empty() && !candidate.has_extended_arcs()) &&
          petri_transition_enabled(candidate, marking)) {
        result.touched.push_back(i);
      }
    }
    const auto& transition = net.transitions()[transition_index];
    if (!petri_transition_enabled(transition, marking)) break;
    fire_petri_transition(transition, marking);
  }
  return result;
}

}  // namespace

namespace {

EventLog stochastic_playout_impl(const PetriNet& net,
                                 const StochasticPlayoutOptions& options,
                                 PlayoutVisitedResult* visited_result) {
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

  std::mt19937_64 engine(options.seed);
  EventLog log;
  log.traces.reserve(options.traces);
  if (visited_result) visited_result->traces.reserve(options.traces);
  std::vector<std::uint64_t> marking(places, 0);
  const auto resolve_marking = [&](const std::vector<std::uint32_t>& requested,
                                   std::size_t boundary,
                                   const char* label) {
    if (!requested.empty() && requested.size() != places) {
      throw std::invalid_argument(std::string("stochastic playout ") + label +
                                  " marking must match the place count");
    }
    std::vector<std::uint64_t> result(places, 0);
    if (requested.empty()) {
      result[boundary] = 1;
    } else {
      std::copy(requested.begin(), requested.end(), result.begin());
    }
    return result;
  };
  const auto initial_marking =
      resolve_marking(options.initial_marking, net.source(), "initial");
  const auto final_marking =
      resolve_marking(options.final_marking, net.sink(), "final");
  std::vector<std::size_t> enabled;
  const auto is_final = [&](const std::vector<std::uint64_t>& candidate) {
    return candidate == final_marking;
  };

  for (std::size_t c = 0; c < options.traces; ++c) {
    Trace trace;
    trace.case_id = "case_" + std::to_string(c + 1);
    PlayoutVisitedTrace visited;
    if (visited_result) {
      visited.reserve(options.max_steps * 2 + 1);
      visited.push_back({PlayoutVisitedElementKind::MARKING, {}, 0});
    }
    double now = options.start + static_cast<double>(c) * options.case_interval;
    marking = initial_marking;
    if (visited_result) {
      auto& initial = visited.front().marking;
      initial.reserve(marking.size());
      for (const auto value : marking) {
        if (value > std::numeric_limits<std::uint32_t>::max())
          throw std::invalid_argument(
              "stochastic visited marking exceeds uint32 range");
        initial.push_back(static_cast<std::uint32_t>(value));
      }
    }

    for (std::size_t step = 0; step < options.max_steps; ++step) {
      if (options.stop_at_final && is_final(marking)) break;

      enabled.clear();
      for (std::size_t t = 0; t < transitions.size(); ++t) {
        const auto& transition = transitions[t];
        // Input-less transitions are always enabled, as in the reference.
        if (!petri_transition_enabled(transition, marking)) continue;
        enabled.push_back(t);
      }
      if (enabled.empty()) break;

      std::size_t selected = 0;
      if (options.transition_weights.empty()) {
        selected = enabled[rng::below(engine, enabled.size())];
      } else {
        double maximum = 0.0;
        for (const auto t : enabled)
          maximum = std::max(maximum, options.transition_weights[t]);
        if (!(maximum > 0.0)) {
          selected = enabled[rng::below(engine, enabled.size())];
        } else {
          std::vector<double> weights;
          weights.reserve(enabled.size());
          for (const auto t : enabled)
            weights.push_back(options.transition_weights[t] / maximum);
          const auto index = rng::weighted_index(engine, weights);
          selected = enabled[index];
        }
      }

      const auto& fired = transitions[selected];
      fire_petri_transition(fired, marking);
      if (visited_result) {
        visited.push_back(
            {PlayoutVisitedElementKind::TRANSITION, {}, selected});
        PlayoutVisitedElement element;
        element.kind = PlayoutVisitedElementKind::MARKING;
        element.marking.reserve(marking.size());
        for (const auto value : marking) {
          if (value > std::numeric_limits<std::uint32_t>::max())
            throw std::invalid_argument(
                "stochastic visited marking exceeds uint32 range");
          element.marking.push_back(static_cast<std::uint32_t>(value));
        }
        visited.push_back(std::move(element));
      }
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
    if (visited_result) visited_result->traces.push_back(std::move(visited));
  }
  return log;
}

}  // namespace

EventLog stochastic_playout(const PetriNet& net,
                            const StochasticPlayoutOptions& options) {
  return stochastic_playout_impl(net, options, nullptr);
}

PlayoutVisitedResult stochastic_playout_visited(
    const PetriNet& net, const StochasticPlayoutOptions& options) {
  PlayoutVisitedResult result;
  (void)stochastic_playout_impl(net, options, &result);
  return result;
}

PlayoutVisitedResult stochastic_playout_visited(
    const PetriNet& net, const EventLog& source_log,
    const StochasticPlayoutOptions& options) {
  auto fitted = options;
  if (fitted.transition_weights.empty())
    fitted.transition_weights =
        fit_stochastic_transition_weights(net, source_log, options);
  return stochastic_playout_visited(net, fitted);
}

std::vector<double> fit_stochastic_transition_weights(
    const PetriNet& net, const EventLog& source_log,
    const StochasticPlayoutOptions& options) {
  validate_stochastic_net(net);
  if (source_log.traces.empty())
    return std::vector<double>(net.transitions().size(), 1.0);
  if (options.fitting_max_states == 0)
    throw std::invalid_argument(
        "source-log fitting state bound must be positive");

  const auto initial = stochastic_marking(
      net, options.initial_marking, net.source(), "initial");
  const auto final = stochastic_marking(
      net, options.final_marking, net.sink(), "final");
  std::vector<std::size_t> enabled_count(net.transitions().size(), 0);
  std::vector<std::size_t> activated_count(net.transitions().size(), 0);

  for (const auto& source_trace : source_log.traces) {
    // Logs can contain labels from a larger model. They are ignored for the
    // native fit, while valid labels retain their original ordering.
    Trace trace;
    trace.case_id = source_trace.case_id;
    trace.events.reserve(source_trace.events.size());
    for (const auto& event : source_trace.events) {
      if (std::find(net.activities().begin(), net.activities().end(),
                    event.activity) != net.activities().end()) {
        trace.events.push_back(event);
      }
    }

    const auto replay = replay_trace(net, trace, initial, final,
                                     options.fitting_max_states);
    for (const auto transition : replay.touched)
      ++enabled_count[transition];
    for (const auto transition : replay.path)
      ++activated_count[transition];
  }

  std::vector<double> weights(net.transitions().size(), 1.0);
  for (std::size_t i = 0; i < weights.size(); ++i) {
    if (enabled_count[i] != 0) {
      weights[i] = static_cast<double>(activated_count[i]) /
                   static_cast<double>(enabled_count[i]);
    }
  }
  return weights;
}

std::vector<double> fit_stochastic_transition_weights(
    const PetriNet& net, const EventLog& source_log) {
  return fit_stochastic_transition_weights(net, source_log,
                                            StochasticPlayoutOptions{});
}

EventLog stochastic_playout_fitted(
    const PetriNet& net, const EventLog& source_log,
    const StochasticPlayoutOptions& options) {
  auto fitted = options;
  if (fitted.transition_weights.empty()) {
    fitted.transition_weights =
        fit_stochastic_transition_weights(net, source_log, options);
  }
  return stochastic_playout(net, fitted);
}

EventLog stochastic_playout(
    const PetriNet& net, const EventLog& source_log,
    const StochasticPlayoutOptions& options) {
  return stochastic_playout_fitted(net, source_log, options);
}

namespace {

// One breadth-first search state: a reachable marking and the visible trace
// that led to it. Two silent routings of the same trace share a state.
struct EnumerationState {
  std::vector<std::uint32_t> marking;
  std::vector<std::size_t> events;  // fired visible transitions, in order
  std::vector<std::size_t> fired;   // all fired transitions for element output
  PlayoutVisitedTrace elements;
  std::unordered_map<std::string, std::size_t> marking_occurrences;
};

std::string marking_key(const std::vector<std::uint32_t>& marking) {
  std::string key;
  key.reserve(marking.size() * 2);
  for (const auto tokens : marking) {
    key += std::to_string(tokens);
    key += ',';
  }
  return key;
}

std::string state_key(const EnumerationState& state, bool include_path) {
  std::string key;
  key.reserve(state.marking.size() * 2 + state.events.size() * 2 +
              (include_path ? state.fired.size() * 2 : 0));
  key += marking_key(state.marking);
  key += '|';
  for (const auto transition : state.events) {
    key += std::to_string(transition);
    key += ',';
  }
  if (include_path) {
    key += '|';
    for (const auto transition : state.fired) {
      key += std::to_string(transition);
      key += ',';
    }
  }
  return key;
}

std::size_t rounded_branching_limit(double cap, std::mt19937_64& engine) {
  if (cap <= 0.0) return 0;
  const auto integral = std::floor(cap);
  auto limit = integral >=
                       static_cast<double>(std::numeric_limits<std::size_t>::max())
                   ? std::numeric_limits<std::size_t>::max()
                   : static_cast<std::size_t>(integral);
  if (cap - integral > 0.0 &&
      rng::unit(engine) < cap - integral &&
      limit != std::numeric_limits<std::size_t>::max())
    ++limit;
  return limit;
}

void sample_enabled(std::vector<std::size_t>& enabled, double cap,
                    std::mt19937_64& engine) {
  if (cap <= 0.0 || enabled.empty()) return;
  const auto limit = rounded_branching_limit(cap, engine);
  if (limit >= enabled.size()) return;
  rng::shuffle(enabled.begin(), enabled.end(), engine);
  enabled.resize(limit);
}

}  // namespace

EventLog extensive_playout_impl(const PetriNet& net,
                                const ExtensivePlayoutOptions& options,
                                PlayoutVisitedResult* visited_result) {
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
  if (!std::isfinite(options.branching_factor_transitions) ||
      options.branching_factor_transitions < 0.0) {
    throw std::invalid_argument(
        "extensive playout branching factor must be finite and non-negative");
  }
  if (options.max_marking_occ && *options.max_marking_occ == 0) {
    throw std::invalid_argument(
        "extensive playout max_marking_occ must be positive when supplied");
  }
  std::mt19937_64 engine(options.seed);
  const bool include_elements = visited_result != nullptr;
  const bool track_marking_occurrences = options.max_marking_occ.has_value();
  const bool track_path = include_elements || track_marking_occurrences;

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
  if (include_elements) {
    initial.elements.push_back(
        {PlayoutVisitedElementKind::MARKING, initial.marking, 0});
  }
  if (track_marking_occurrences) {
    initial.marking_occurrences.emplace(marking_key(initial.marking), 1);
  }
  visited.insert(state_key(initial, track_path));
  if (is_final(initial.marking)) {
    emit(initial.events);  // degenerate net
    if (include_elements) visited_result->traces.push_back(initial.elements);
  }
  queue.push_back(std::move(initial));

  while (!queue.empty() && log.traces.size() < options.max_traces &&
         visited.size() < options.max_states) {
    const auto state = std::move(queue.front());
    queue.pop_front();

    std::vector<std::size_t> enabled;
    for (std::size_t t = 0; t < transitions.size(); ++t) {
      const auto& transition = transitions[t];
      // A transition without input places is always enabled, as in the
      // reference's semantics (alpha nets have such transitions).
      if (!transition.silent() && state.events.size() >= options.max_length) {
        continue;  // would exceed the visible-length bound
      }
      if (!petri_transition_enabled(transition, state.marking)) continue;
      enabled.push_back(t);
    }
    sample_enabled(enabled, options.branching_factor_transitions, engine);
    for (const auto t : enabled) {
      const auto& transition = transitions[t];

      EnumerationState next;
      next.marking = state.marking;
      fire_petri_transition(transition, next.marking);
      next.events = state.events;
      if (!transition.silent()) next.events.push_back(t);
      if (track_path) {
        next.fired = state.fired;
        next.fired.push_back(t);
      }
      if (include_elements) {
        next.elements = state.elements;
        next.elements.push_back(
            {PlayoutVisitedElementKind::TRANSITION, {}, t});
        next.elements.push_back(
            {PlayoutVisitedElementKind::MARKING, next.marking, 0});
      }
      if (track_marking_occurrences) {
        next.marking_occurrences = state.marking_occurrences;
        const auto key = marking_key(next.marking);
        const auto occurrence = ++next.marking_occurrences[key];
        if (options.max_marking_occ &&
            occurrence > *options.max_marking_occ) {
          continue;
        }
      }

      if (!visited.insert(state_key(next, track_path)).second) continue;
      if (is_final(next.marking)) {
        emit(next.events);
        if (include_elements) visited_result->traces.push_back(next.elements);
        if (log.traces.size() >= options.max_traces) break;
      }
      queue.push_back(std::move(next));
    }
  }

  if (include_elements && !queue.empty()) {
    visited_result->trace_limit_reached =
        log.traces.size() >= options.max_traces;
    visited_result->state_limit_reached =
        visited.size() >= options.max_states;
    visited_result->truncated = visited_result->trace_limit_reached ||
                                visited_result->state_limit_reached;
  }

  return log;
}

EventLog extensive_playout(const PetriNet& net,
                           const ExtensivePlayoutOptions& options) {
  return extensive_playout_impl(net, options, nullptr);
}

PlayoutVisitedResult extensive_playout_visited(
    const PetriNet& net, const ExtensivePlayoutOptions& options) {
  PlayoutVisitedResult result;
  (void)extensive_playout_impl(net, options, &result);
  return result;
}

}  // namespace minera
