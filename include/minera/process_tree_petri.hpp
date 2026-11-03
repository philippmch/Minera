#pragma once

#include <string>
#include <vector>

#include "minera/petri_net.hpp"
#include "minera/pnml.hpp"
#include "minera/process_tree.hpp"

namespace minera {

// Standard process-tree to Petri-net conversion with the reference element
// names. `net` is exactly what to_petri_net(tree) returns: the reference
// construction followed by the reference's two-pass simple reduction, with
// net.transition_names() set to `transition_names`.
//
// Names follow the reference where it is deterministic: places are
// "source", "sink" and "p_<k>"; silent transitions are "<kind>_<k>" (tau,
// skip, tauSplit, tauJoin, init_loop, loop, inclusiveStart, inclusiveLater,
// inclusiveSkip, iTrans, fTrans). The reference names visible transitions
// with random UUID4 strings; here they are deterministic UUID-shaped strings
// "00000000-0000-4000-8000-<12 hex digits>" numbered in creation order, so
// every visible name sorts before every silent name (a reference UUID also
// sorts before every silent kind except possibly fTrans).
struct ProcessTreePetriNetConversion {
  PetriNet net;
  std::vector<std::string> place_names;       // one per net place
  std::vector<std::string> transition_names;  // one per net transition
};

[[nodiscard]] ProcessTreePetriNetConversion convert_process_tree_to_petri_net(
    const ProcessTree& tree);

// PNML document for a converted process tree: the standard variant carries
// the reference place and transition names as PNML IDs; the transition-
// bordered variant (whose reference names are not deterministic) keeps the
// derived pN/tN IDs. Markings hold one token on source and on sink.
[[nodiscard]] PnmlDocument process_tree_pnml_document(
    const ProcessTree& tree,
    ProcessTreePetriNetVariant variant = ProcessTreePetriNetVariant::kStandard);

}  // namespace minera
