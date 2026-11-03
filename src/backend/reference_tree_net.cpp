#include "reference_tree_net.hpp"

#include "minera/process_tree_petri.hpp"

namespace minera {

// The object-centric discovery uses the standard process-tree conversion (the
// reference construction and its two-pass simple reduction), so both share
// one implementation. The OCPN nets keep their unnamed tN/pN form: the
// conversion's reference names are dropped by copying its places and
// transitions into a fresh net.
PetriNet process_tree_reference_net(const ProcessTree& tree) {
  const auto converted = convert_process_tree_to_petri_net(tree).net;
  PetriNet net;
  net.set_activities(converted.activities());
  for (std::size_t place = 0; place < converted.place_count(); ++place) (void)net.add_place();
  if (converted.has_source()) net.set_source(converted.source());
  if (converted.has_sink()) net.set_sink(converted.sink());
  for (const auto& transition : converted.transitions())
    net.add_transition(transition.activity, transition.inputs, transition.outputs);
  return net;
}

}  // namespace minera
