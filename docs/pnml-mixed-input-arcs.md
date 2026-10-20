# PNML mixed and parallel input arcs

PNML import must preserve distinct ordinary, reset and inhibitor arc objects,
including multiple types or parallel extended arcs sharing a place/transition
endpoint. These graphs are already representable by the full native Petri
constructor and document occurrence inventory. The incremental editing API has
a stricter policy; importing a complete graph must not accidentally apply that
editing policy.

Previously, `parse_pnml_document` parsed and retained the correct typed vectors,
then assembled the net through `add_reset_arc` and `add_inhibitor_arc`. Those
mutators rejected mixed or duplicate typed endpoints. This also meant a valid
full native document could be exported to PNML but not imported again.

The parser now installs its already validated endpoint and weight vectors through
`PetriNet::from_literal_storage`. Existing XML parsing, endpoint/type checks,
exact signed weight parsing, marking and workflow-boundary inference, stochastic
metadata and arc occurrence/ID retention remain in place. The incremental
`PetriNet` mutators keep their previous restrictions. No firing algorithm changes.

`pnml_mixed_input_arc_occurrences` in the existing extended-arcs suite checks
ordinary/reset, ordinary/inhibitor, reset/inhibitor and triple combinations;
parallel reset and inhibitor objects; and zero, negative and arbitrary-width
weights. It checks exact occurrence types/endpoints/weights/IDs and full typed
document equality after export/reimport. Invalid occurrence endpoints, changed
weights and omitted arcs must still be rejected atomically by export/projection,
without mutating the supplied document. A separate assertion retains the stricter
incremental editing policy.

Root's fresh focused native build passed **5/5** assertion-enabled CTest suites:
extended Petri arcs, original-model contexts, core, evaluation contracts and
numeric tests. The joint build compiled the current PNML, context and JSON
objects against the validated core; actual logs remain under
`build/parity-closures/pnml-mixed`, including `ctest.log`. The previous context
diagnosis retained the actual old importer rejection for a mixed
ordinary/reset/inhibitor fixture; that failed history is not erased. This is a
focused integration check, not a fresh execution of every native suite.

This is a concrete importer correction, not completion of every PNML import,
export or Petri catalogue unit. No reference import calls, provider calls or
browser checks have been made for this step.
