#include "minera/genetic_miner.hpp"

#include <cassert>
#include <stdexcept>

namespace {

minera::EventLog chain_log() {
  minera::EventLog log;
  log.traces.push_back({"c1", {{"c1", "A", "2020-01-01T00:00:00Z", "", 0, {}, ""},
                               {"c1", "B", "2020-01-01T00:01:00Z", "", 1, {}, ""}}});
  log.traces.push_back({"c2", {{"c2", "A", "2020-01-02T00:00:00Z", "", 0, {}, ""},
                               {"c2", "B", "2020-01-02T00:01:00Z", "", 1, {}, ""}}});
  return log;
}

minera::EventLog disconnected_log() {
  minera::EventLog log;
  log.traces.push_back({"x", {{"x", "A", "2020-01-01T00:00:00Z", "", 0, {}, ""}}});
  log.traces.push_back({"y", {{"y", "B", "2020-01-02T00:00:00Z", "", 0, {}, ""}}});
  return log;
}

}  // namespace

int main() {
  minera::GeneticMinerOptions options;
  options.seed = 17;
  options.population_size = 8;
  options.generations = 4;
  options.max_states = 128;
  const auto first = minera::mine_genetic(chain_log(), options);
  const auto second = minera::mine_genetic(chain_log(), options);
  assert(first.diagnostics.computed);
  assert(first.net.activities() == second.net.activities());
  assert(first.net.place_count() == second.net.place_count());
  assert(first.net.transitions().size() == second.net.transitions().size());
  assert(first.diagnostics.best_fitness == second.diagnostics.best_fitness);
  assert(first.diagnostics.best_fitness > 0.99);
  assert(first.net.activities().size() == 2);
  assert(first.net.source() < first.net.place_count());
  assert(first.net.sink() < first.net.place_count());
  bool saw_a = false, saw_b = false;
  for (const auto& transition : first.net.transitions()) {
    if (transition.activity && *transition.activity == 0) saw_a = true;
    if (transition.activity && *transition.activity == 1) saw_b = true;
  }
  assert(saw_a && saw_b);

  options.population_size = 6;
  options.generations = 2;
  const auto repaired = minera::mine_genetic(disconnected_log(), options);
  assert(repaired.diagnostics.computed);
  assert(repaired.net.activities().size() == 2);
  for (const auto& transition : repaired.net.transitions()) {
    if (transition.activity) {
      assert(!transition.inputs.empty());
      assert(!transition.outputs.empty());
    }
  }

  options.max_states = 1;
  const auto capped = minera::mine_genetic(chain_log(), options);
  assert(capped.diagnostics.exhausted);
  assert(capped.diagnostics.candidates_evaluated == 1);

  bool threw = false;
  options.max_states = 128;
  options.population_size = 1;
  try { (void)minera::mine_genetic(chain_log(), options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try { (void)minera::mine_genetic({}, options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  return 0;
}
