#include "minera/playout.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <random>
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
        bool ready = true;
        for (const auto place : transitions[t].inputs) {
          if (marking[place] == 0) {
            ready = false;
            break;
          }
        }
        if (ready && !transitions[t].inputs.empty()) {
          enabled.push_back(t);
        }
      }
      if (enabled.empty()) {
        break;  // deadlock (only possible on an unsound net)
      }

      const auto pick = std::uniform_int_distribution<std::size_t>(
          0, enabled.size() - 1)(rng);
      const auto& fired = transitions[enabled[pick]];
      for (const auto place : fired.inputs) {
        --marking[place];
        --tokens;
      }
      for (const auto place : fired.outputs) {
        ++marking[place];
        ++tokens;
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

  const auto is_final = [&](const std::vector<std::uint32_t>& marking) {
    for (std::size_t p = 0; p < marking.size(); ++p) {
      if (marking[p] != (p == net.sink() ? 1U : 0U)) return false;
    }
    return true;
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
  initial.marking.assign(net.place_count(), 0);
  initial.marking[net.source()] = 1;
  visited.insert(state_key(initial));
  if (is_final(initial.marking)) emit(initial.events);  // degenerate net
  queue.push_back(std::move(initial));

  while (!queue.empty() && log.traces.size() < options.max_traces &&
         visited.size() < options.max_states) {
    const auto state = std::move(queue.front());
    queue.pop_front();

    for (std::size_t t = 0; t < transitions.size(); ++t) {
      const auto& transition = transitions[t];
      if (transition.inputs.empty()) continue;
      if (!transition.silent() && state.events.size() >= options.max_length) {
        continue;  // would exceed the visible-length bound
      }
      bool ready = true;
      for (const auto place : transition.inputs) {
        if (state.marking[place] == 0) {
          ready = false;
          break;
        }
      }
      if (!ready) continue;

      EnumerationState next;
      next.marking = state.marking;
      for (const auto place : transition.inputs) --next.marking[place];
      for (const auto place : transition.outputs) ++next.marking[place];
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
