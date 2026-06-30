#pragma once

#include <ostream>
#include <string>
#include <string_view>

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

}  // namespace minera
