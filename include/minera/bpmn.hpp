#pragma once

#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>

#include "minera/petri_net.hpp"
#include "minera/process_tree.hpp"

namespace minera {

// Serializes an inductive-miner process tree as BPMN 2.0 XML. An activity
// leaf becomes a task; an exclusive, inclusive, or parallel block becomes a
// split gateway (gatewayDirection="Diverging") and a join gateway
// ("Converging"); a loop becomes an exclusive cycle with a converging head and
// a diverging tail. A silent (tau) step emits no task. Start and end events
// are named "start" and "end". Every node lists its <incoming>/<outgoing>
// flows, and a BPMNDiagram with a deterministic layered layout (columns by
// longest-path depth, back edges ignored) is included so BPMN editors open the
// file directly. Trees containing atomic interleaving are first converted to
// their exact workflow net so BPMN export does not weaken them into
// event-level parallelism.
void write_bpmn(std::ostream& out,
                const ProcessTree& tree,
                std::string_view name = "process");

[[nodiscard]] std::string to_bpmn(const ProcessTree& tree,
                                  std::string_view name = "process");

// Serializes any accepting Petri net as BPMN 2.0 XML with the reference
// (pm4py 2.7.22) Petri-net-to-BPMN structure: every place becomes an
// exclusive gateway; the source place is reached from the start event and the
// sink place leads to the end event; every visible transition becomes its own
// task (transitions sharing a label are not merged), preceded by a converging
// parallel gateway when the transition has
// several input places and followed by a diverging one when it has several
// output places (and by an exclusive gateway when it has no input or no output
// place); a silent transition becomes one parallel gateway when it has several
// input or output places (Converging when several inputs, else Diverging) and
// an exclusive gateway otherwise. Exclusive gateways with exactly one incoming
// and one outgoing flow are then collapsed into a direct flow, so dangling
// places keep their gateways. The document carries gatewayDirection,
// <incoming>/<outgoing> children and a layered BPMNDiagram.
void write_bpmn(std::ostream& out,
                const PetriNet& net,
                std::string_view name = "process");

[[nodiscard]] std::string to_bpmn(const PetriNet& net,
                                  std::string_view name = "process");

// The same conversion for a net with explicit markings (one entry per place),
// as the reference `convert_to_bpmn(net, im, fm)` reads them: the start and
// end events are always present, the start event feeds every place holding
// initial tokens and every place holding final tokens feeds the end event
// (an empty marking leaves its event without flows). Throws
// std::invalid_argument when a marking's size differs from the place count.
void write_bpmn(std::ostream& out, const PetriNet& net,
                const TokenMarking& initial_marking,
                const TokenMarking& final_marking,
                std::string_view name = "process");

[[nodiscard]] std::string to_bpmn(const PetriNet& net,
                                  const TokenMarking& initial_marking,
                                  const TokenMarking& final_marking,
                                  std::string_view name = "process");

// Parses a BPMN 2.0 model into a workflow net. The first non-empty <process>
// is read (namespace prefixes are ignored; the reference merges every pool
// instead).
//
// When the process only uses start and end events, tasks of any task kind
// (not call activities), exclusive and parallel gateways — the constructs the
// reference converter supports — the net is built with the reference
// construction and names: places "source" and "sink", "ent_<node id>" and
// "exi_<node id>" per node, one place per sequence flow named by the flow id,
// silent connector transitions between them, a forking (joining) silent
// transition for a parallel gateway with several outgoing (incoming) flows,
// silent start/end connectors from source and into sink, and one transition
// per task (visible under a non-empty name, silent otherwise). Silent
// connectors with a single private input or output place are then merged
// into their neighbour exactly as the reference's simple reduction does.
// Dangling gateways, isolated tasks and processes without start or end events
// are accepted like the reference. Connector transitions get deterministic
// UUID-shaped names (00000000-0000-4000-8000-<counter>) where the reference
// draws random UUIDs. Place names are carried in PetriNet::place_names().
//
// Other processes (intermediate events, event-based, inclusive or complex
// gateways, subprocesses, boundary events, call activities) keep Minera's
// token-semantics mapping: every sequence flow becomes a place, a task or
// call activity a transition, an exclusive or event-based gateway routes one
// token, a parallel gateway synchronizes, a block-structured inclusive
// split/join pair enumerates non-empty branch subsets, a non-gateway node with
// several outgoing flows starts all of them, intermediate events pass tokens
// silently, expanded subprocesses convert recursively, and boundary events
// compete with normal completion (interrupting) or fork a one-shot path
// (non-interrupting). Complex, mixed-arity inclusive and ambiguous inclusive
// gateways are rejected, and malformed models (dangling flow references,
// missing start or end events, gateways without incoming or outgoing flows)
// fail via std::runtime_error on this path.
[[nodiscard]] PetriNet parse_bpmn(std::string_view xml);

// Reads and parses a BPMN file.
[[nodiscard]] PetriNet import_bpmn(const std::filesystem::path& path);

// True when the document's root element is a BPMN <definitions> (under any
// namespace prefix) — used to tell an uploaded BPMN model from a PNML net.
[[nodiscard]] bool looks_like_bpmn(std::string_view xml);

}  // namespace minera
