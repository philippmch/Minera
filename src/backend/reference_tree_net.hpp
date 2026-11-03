#pragma once

#include "minera/petri_net.hpp"
#include "minera/process_tree.hpp"

namespace minera {

// The workflow net the object-centric Petri-net discovery builds from each
// per-type process tree: the standard process-tree conversion
// (convert_process_tree_to_petri_net, the reference construction followed by
// the reference's two-pass simple reduction), without the reference element
// names, so the OCPN nets keep their unnamed pN/tN form.
[[nodiscard]] PetriNet process_tree_reference_net(const ProcessTree& tree);

}  // namespace minera
