#pragma once

#include <string>

#include "minera/petri_net.hpp"
#include "minera/trie.hpp"

namespace minera {

// Converts a prefix trie into a workflow net that accepts exactly the
// terminal paths represented by the trie.  Shared prefixes reuse their output
// place; a silent transition closes a prefix that is terminal as well as
// expandable.
[[nodiscard]] PetriNet convert_trie_to_petri_net(const TrieModel& trie);

[[nodiscard]] inline PetriNet to_petri_net(const TrieModel& trie) {
  return convert_trie_to_petri_net(trie);
}

[[nodiscard]] std::string serialize_trie_petri_net_json(const PetriNet& net);

}  // namespace minera
