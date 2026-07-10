#pragma once

#include <filesystem>
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

// Parses a BPMN 2.0 model into a workflow net, so a process drawn or exported
// in a BPM suite can be checked against a log like a discovered model. The
// first non-empty <process> is read (namespace prefixes are ignored); its
// token semantics map onto the net: every sequence flow becomes a place, a
// task (any task kind, or a call activity) a transition — visible under its
// name, silent when unnamed — an exclusive or event-based gateway routes one
// token from any incoming flow to one outgoing flow, and a parallel gateway
// consumes a token from every incoming flow and produces one on every
// outgoing flow. Per the standard's token rules, a non-gateway node with
// several outgoing flows starts all of them, and one with several incoming
// flows fires on each token separately; intermediate events pass tokens
// through silently. An expanded subprocess converts recursively between its
// own start and end events; a collapsed (empty) one behaves as an atomic
// task. Start events feed from a fresh source place and end events into a
// fresh sink place, giving the single-source single-sink shape conformance
// replays. A task no sequence flow reaches — how an activity that is
// disconnected in the source model comes out of a BPMN export — stays a
// disconnected (dead) transition rather than an error, so conformance can
// report it honestly. Inclusive, complex, and boundary constructs have no
// faithful workflow-net reading and are rejected, as are malformed models
// (dangling flow references, missing start or end events, gateways without
// incoming or outgoing flows) — all via std::runtime_error.
[[nodiscard]] PetriNet parse_bpmn(std::string_view xml);

// Reads and parses a BPMN file.
[[nodiscard]] PetriNet import_bpmn(const std::filesystem::path& path);

// True when the document's root element is a BPMN <definitions> (under any
// namespace prefix) — used to tell an uploaded BPMN model from a PNML net.
[[nodiscard]] bool looks_like_bpmn(std::string_view xml);

}  // namespace minera
