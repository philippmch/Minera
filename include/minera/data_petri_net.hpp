#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "minera/decision_mining.hpp"

namespace minera {

struct DataPetriNetOptions {
  // Keep the original decision-tree diagnostics in the result even when no
  // observed branch can be guarded.
  bool include_decision_diagnostics = true;
};

struct DataPetriNetTransition {
  std::size_t transition_index = 0;
  std::string guard;
  std::vector<std::string> read_variables;
  std::vector<std::string> write_variables;
};

struct DataPetriNet {
  std::string variant = "decision_tree";
  PetriNet net;
  DecisionMiningResult decisions;
  std::vector<DataPetriNetTransition> guarded_transitions;
};

// Mines attribute-dependent branch guards and attaches them to a copy of the
// supplied workflow net. A transition can consume several decision places;
// the resulting guard is the conjunction of the independently learned rules.
[[nodiscard]] DataPetriNet mine_data_petri_net(
    const EventLog& log, const PetriNet& net,
    const DataPetriNetOptions& options = {});

[[nodiscard]] inline DataPetriNet create_data_petri_net_with_decisions(
    const EventLog& log, const PetriNet& net,
    const DataPetriNetOptions& options = {}) {
  return mine_data_petri_net(log, net, options);
}

[[nodiscard]] std::string serialize_data_petri_net_json(
    const DataPetriNet& result, std::string_view miner = {});

[[nodiscard]] inline std::string data_petri_net_json(
    const DataPetriNet& result, std::string_view miner = {}) {
  return serialize_data_petri_net_json(result, miner);
}

}  // namespace minera
