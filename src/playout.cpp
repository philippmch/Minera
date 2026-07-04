#include "minera/playout.hpp"

#include <algorithm>
#include <random>
#include <string>
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

}  // namespace minera
