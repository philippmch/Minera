#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "minera/event_log.hpp"
#include "minera/petri_analysis.hpp"
#include "minera/petri_net.hpp"

namespace minera {

// Deterministic, bounded controls for the classic causal-matrix genetic
// discovery algorithm.  `seed` is an explicit reproducibility extension.
struct GeneticMinerOptions {
  std::string activity_key;
  // Optional event timestamp and case-id selectors. Standard keys (timestamp,
  // time:timestamp, case_id, case:concept:name) and generic attributes are
  // resolved strictly when supplied.
  std::string timestamp_key;
  std::string case_id_key;
  std::uint64_t seed = 0;
  std::size_t population_size = 32;
  std::size_t generations = 20;
  double elitism_rate = 0.10;
  double crossover_rate = 0.80;
  double mutation_rate = 0.10;
  std::size_t elitism_min_sample = 3;
  std::size_t max_activities = 12;
  std::size_t max_states = 10000;
};

struct GeneticMinerDiagnostics {
  bool computed = false;
  bool exhausted = false;
  std::size_t generations_completed = 0;
  std::size_t candidates_evaluated = 0;
  std::size_t population_size = 0;
  std::size_t activity_count = 0;
  double best_fitness = 0.0;
  std::uint64_t seed = 0;
  std::string message;
};

struct GeneticMinerResult {
  PetriNet net;
  WorkflowMarkings markings;
  GeneticMinerDiagnostics diagnostics;

  [[nodiscard]] const PetriNet& petri_net() const noexcept { return net; }
  [[nodiscard]] const WorkflowMarkings& workflow_markings() const noexcept {
    return markings;
  }
};

[[nodiscard]] GeneticMinerResult mine_genetic(
    const EventLog& event_log, const GeneticMinerOptions& options = {});

inline GeneticMinerResult discover_genetic_miner(
    const EventLog& event_log, const GeneticMinerOptions& options = {}) {
  return mine_genetic(event_log, options);
}

inline GeneticMinerResult discover_genetic(
    const EventLog& event_log, const GeneticMinerOptions& options = {}) {
  return mine_genetic(event_log, options);
}

using GeneticOptions = GeneticMinerOptions;
using GeneticDiagnostics = GeneticMinerDiagnostics;
using GeneticResult = GeneticMinerResult;

}  // namespace minera
