#pragma once

#include "minera/petri_net.hpp"
#include "minera/process_tree.hpp"

namespace minera {

// The workflow net the object-centric Petri-net discovery builds from each
// per-type process tree, in the reference construction: sequences chain their
// children through fresh places, choices share their entry and exit places,
// parallel blocks add a silent split and join with a fresh place pair per
// child, and loops add a silent entry into the body, the redo children into a
// fresh place with a silent transition back to the body's entry, and a silent
// exit. A silent transition from the source or into the sink is added for a
// choice, parallel or loop root, and for a sequence root whose first child
// starts with a loop (or begins with a choice or parallel block and ends with
// a loop) or whose last child ends with a loop. Finally a silent transition
// with one input place is folded into the transition that feeds that place
// when the place has no other arcs, and a silent transition with one output
// place is folded into the transition that place feeds.
[[nodiscard]] PetriNet process_tree_reference_net(const ProcessTree& tree);

}  // namespace minera
