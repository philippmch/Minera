#include "minera/ilp_miner.hpp"
#include "minera/conformance.hpp"
#include "minera/petri_analysis.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

using namespace minera;

namespace {
EventLog log_of(const std::vector<std::vector<std::string>>& rows) {
  EventLog log;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    Trace trace;
    trace.case_id = std::to_string(i);
    for (std::size_t j = 0; j < rows[i].size(); ++j)
      trace.events.push_back(Event{trace.case_id, rows[i][j], {}, {}, j, {}, {}});
    log.traces.push_back(std::move(trace));
  }
  return log;
}

bool has_arc(const PetriNet& net, std::size_t from, std::size_t to) {
  for (const auto& transition : net.transitions()) {
    if (!transition.activity || *transition.activity != from) continue;
    for (const auto place : transition.outputs)
      if (place >= 2 && std::find(transition.inputs.begin(), transition.inputs.end(), place) ==
                            transition.inputs.end()) {
        for (const auto& target : net.transitions()) {
          if (!target.activity || *target.activity != to) continue;
          if (std::find(target.inputs.begin(), target.inputs.end(), place) !=
              target.inputs.end())
            return true;
        }
      }
  }
  return false;
}
}  // namespace

int main() {
  const auto sequence = mine_ilp(log_of({{"a", "b", "c"}, {"a", "b", "c"}}));
  assert(sequence.diagnostics.computed);
  assert(sequence.markings.initial.size() == sequence.net.place_count());
  assert(sequence.net.activities() == std::vector<std::string>({"a", "b", "c"}));
  assert(has_arc(sequence.net, 0, 1) && has_arc(sequence.net, 1, 2));
  assert(check_workflow_net(sequence.net).valid);

  const auto choice = mine_ilp(log_of({{"a", "b"}, {"a", "c"}}));
  assert(choice.net.place_count() >= 3);
  assert(check_workflow_net(choice.net).valid);

  const auto parallel = mine_ilp(log_of({{"a", "b", "c", "d"},
                                         {"a", "c", "b", "d"}}));
  assert(parallel.net.activities().size() == 4);
  assert(has_arc(parallel.net, 0, 1) && has_arc(parallel.net, 0, 2));
  assert(check_workflow_net(parallel.net).valid);

  // Region constraints must retain repeated activity and self-loop behavior.
  const auto repeated_log = log_of({{"a", "a", "b"}, {"a", "b"}});
  const auto repeated = mine_ilp(repeated_log);
  assert(check_workflow_net(repeated.net).valid);
  const auto repeated_replay = check_conformance(repeated_log, repeated.net);
  assert(repeated_replay.fitting_traces == repeated_replay.trace_count);
  const auto loop_log = log_of({{"a", "a"}, {"a"}});
  const auto loop = mine_ilp(loop_log);
  assert(loop.net.place_count() >= 3);
  assert(check_workflow_net(loop.net).valid);
  const auto loop_replay = check_conformance(loop_log, loop.net);
  assert(loop_replay.fitting_traces == loop_replay.trace_count);

  // Candidate ordering and diagnostics are stable across runs.
  const auto repeat_again = mine_ilp(repeated_log);
  assert(repeated.net.activities() == repeat_again.net.activities());
  assert(repeated.net.place_count() == repeat_again.net.place_count());
  assert(repeated.diagnostics.visited_nodes == repeat_again.diagnostics.visited_nodes);

  ILPMinerOptions bounded;
  bounded.max_nodes = 1;
  const auto limited = mine_ilp(log_of({{"a", "b", "c"}}), bounded);
  assert(limited.diagnostics.exhausted);
  assert(limited.diagnostics.visited_nodes <= 1);
  assert(limited.net.place_count() >= 2);

  bool rejected = false;
  try {
    ILPMinerOptions invalid;
    invalid.noise_threshold = 2.0;
    (void)mine_ilp(log_of({{"a"}}), invalid);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  // Exponential signature growth is rejected before matrix allocation.
  std::vector<std::vector<std::string>> many(1);
  for (int i = 0; i < 9; ++i) many[0].push_back("activity_" + std::to_string(i));
  bool high_cardinality_rejected = false;
  try {
    (void)mine_ilp(log_of(many));
  } catch (const std::invalid_argument&) {
    high_cardinality_rejected = true;
  }
  assert(high_cardinality_rejected);
  return 0;
}
