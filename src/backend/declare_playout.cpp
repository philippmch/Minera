#include "minera/declare_playout.hpp"

#include "minera/declare_streaming.hpp"

#include <algorithm>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace minera {

EventLog declare_playout(const DeclareModel& model, const DeclarePlayoutOptions& options) {
  // Construction performs the same strict known-template arity validation as
  // the streaming variant, before any output is produced.
  const DeclareStreamingConformance validator(model);
  (void)validator;
  if (options.min_length > options.max_length)
    throw std::invalid_argument("Declare playout min_length must not exceed max_length");
  if (options.n_traces > 100000 || options.max_length > 10000)
    throw std::invalid_argument("Declare playout bounds exceed the native safety limit");
  EventLog log;
  if (options.n_traces == 0) return log;
  std::mt19937_64 rng(options.seed.value_or(std::random_device{}()));
  std::vector<std::string> activities;
  for (const auto& [_, values] : model.rules) {
    for (const auto& c : values) {
      activities.push_back(c.first);
      if (c.binary) activities.push_back(c.second);
    }
  }
  std::sort(activities.begin(), activities.end());
  activities.erase(std::unique(activities.begin(), activities.end()), activities.end());

  for (std::size_t trace_index = 0; trace_index < options.n_traces; ++trace_index) {
    std::size_t length = options.min_length;
    if (options.min_length <= options.max_length) {
      std::uniform_int_distribution<std::size_t> length_dist(options.min_length, options.max_length);
      length = length_dist(rng);
    }
    Trace trace;
    trace.case_id = std::to_string(trace_index);
    if (activities.empty()) { log.traces.push_back(std::move(trace)); continue; }
    std::vector<std::string> prefix;
    prefix.reserve(static_cast<std::size_t>(length));
    for (std::size_t step = 0; step < length; ++step) {
      struct Candidate { std::string activity; };
      std::vector<Candidate> candidates;
      for (const auto& activity : activities) {
        DeclareStreamingConformance monitor(model);
        bool bad = false;
        for (const auto& prior : prefix) {
          Event e; e.case_id = trace.case_id; e.activity = prior;
          if (monitor.receive(e).deviations != 0) { bad = true; break; }
        }
        if (!bad) {
          Event e; e.case_id = trace.case_id; e.activity = activity;
          if (monitor.receive(e).deviations == 0) candidates.push_back({activity});
        }
      }
      if (candidates.empty()) break;
      std::uniform_int_distribution<std::size_t> choice(0, candidates.size() - 1);
      const auto chosen = candidates[choice(rng)].activity;
      prefix.push_back(chosen);
      Event e; e.case_id = trace.case_id; e.activity = chosen; e.input_order = trace.events.size();
      trace.events.push_back(std::move(e));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

}  // namespace minera
