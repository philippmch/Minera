#pragma once

#include <ostream>
#include <string>
#include <string_view>

#include "minera/petri_net.hpp"
#include "minera/process_tree.hpp"

namespace minera {

// Serializes an inductive-miner process tree as BPMN 2.0 XML — the notation read
// by Camunda, bpmn.io, Signavio, and most BPM suites. The block-structured tree
// maps cleanly onto BPMN: an activity leaf becomes a task, an exclusive choice
// and a parallel block become a matching split/join gateway pair, and a loop
// becomes a gateway cycle. A silent (tau) step emits no task — it collapses into
// a direct sequence flow. Only the semantic process is emitted (no diagram
// interchange); importers lay the graph out on their own.
void write_bpmn(std::ostream& out,
                const ProcessTree& tree,
                std::string_view name = "process");

[[nodiscard]] std::string to_bpmn(const ProcessTree& tree,
                                  std::string_view name = "process");

// Serializes any workflow net as BPMN 2.0 XML, so the Alpha and Heuristic
// miners' models (and imported nets) export like the inductive one. The
// mapping follows the nets' token semantics: a visible transition becomes a
// task, a transition consuming from (producing to) several places gets a
// parallel join (split) gateway, and a place with several producers or
// consumers becomes an exclusive gateway — a token rests in one place and
// leaves through one arc. The source place becomes the start event, the sink
// the end event; trivial places and silent one-in-one-out transitions
// collapse into plain sequence flows.
void write_bpmn(std::ostream& out,
                const PetriNet& net,
                std::string_view name = "process");

[[nodiscard]] std::string to_bpmn(const PetriNet& net,
                                  std::string_view name = "process");

}  // namespace minera
