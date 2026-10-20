# OCEL object descendants graph parity contract

`algo.transformation.ocel.graphs.object_descendants_graph` is complete under the
repository's semantic parity definition. Its public native entry point is
`discover_ocel_object_graph(log, OcelEnrichmentGraph::kDescendants)`.
The containing support family and `ocel20_computation` are not promoted by this
module conclusion.

## Relation and input contract

At each event in stored event-table order, every participating object seen
at an earlier event has an edge to each participating object seen for the
first time at this event. New objects do not create edges to one another, and
an old/old meeting adds no edge. This is a direct birth interaction relation,
not transitive reachability: the captured chain has `a->b` and `b->c`, without
`a->c`. Reversing event rows reverses the observed directions; reversing or
interleaving relation rows, changing timestamps or object types does not.

Supported OCEL inputs have unique declared event/object identities and
relations referencing those identities. Every string is literal identity data,
including empty strings. Duplicate relation rows and qualifier variations do
not multiply edges or introduce self-loops. Attributes, activities, O2O/E2E
relations, object changes and declared type/schema metadata are inactive.
Objects without relations remain isolated native node metadata.

The public result is an unordered set of directed string pairs. Native
edges are its canonical lexical vector, retaining directed orientation where
applicable; `object_ids` additionally includes isolated declared objects.
Custom public column bindings normalize into the typed native identity fields.
The public module exports no `Parameters` enum. Omitted, null, empty and
irrelevant parameter dictionaries add no effective control.

A declared event without object relations produces an error. The public
function raises `KeyError` for the first missing group in **forward
event-table order**; native code raises `std::out_of_range` with
`OCEL descendants event has no object relations: <event-id>`.
The failure condition, offending identity and diagnostic meaning are retained;
Python exception classes/repr formatting are not a native API requirement.
Empty and objects-only inputs return empty edge sets; events-only inputs fail.
Native duplicate-ID and unknown-endpoint checks remain explicit diagnostics,
not fabricated public observations for malformed references.

## Captured evidence and actual repair

Root executed one pinned public recipe for the three lifetime relations:
`build/ocel-lifetime-graphs-focused-prepared-1/capture-public.py`.
This module accounts for **119 calls: 95 returns and 24 expected error
outcomes**, with zero six-table/public-configuration or parameter mutations.
Across the three modules the capture contains 357 calls, 285 returns and
72 errors, each retained under its own module identity. Opaque module hashes
match PM4Py 2.7.22 commit `0715c31f3f1d85924107d6aa5329806db69df899`
before/after execution. No reference implementation, bytecode or private state
was inspected, and no successful public case was replayed.

The shared cases cover all two-event/three-object incidence matrices, default
parameter forms, duplicate/qualified rows, empty/isolated sources, Unicode and
empty IDs, custom columns and independently changed event/relation/timestamp
orders. Twenty-two lifetime controls distinguish chains, split/merge
boundaries, ephemeral objects, same birth/different death, different birth/same
death, revisits and cohort replacement. Returned strings, edge directions,
error identities and all six public table snapshots remain in the archive.

The frozen native implementation matched **91/119 outcomes**. Its relation
formula and ordering already agreed; the four empty-ID cases were rejected,
and the 24 missing-event failures were silently skipped. The direct selector
now permits literal empty IDs and enforces the observed missing-event error
order. Private helper defaults preserve existing enrichment behavior and its
validation policy. Import/transport identifier policies were not broadened.

`test_ocel_descendants_module.cpp` embeds all 119 literal captured outcomes,
without deriving expectations from the native formula. It supplements them
with byte-string identity/metadata invariance, complete native source
before/after snapshots and preserved validation. A focused build compiled the
current `ocel_transform.cpp` object ahead of the frozen library; the link map
binds the actual metric/helper symbols to that fresh object. `/UNDEBUG`
enabled assertions. This module matched **119/119**, and all six focused
CTest suites passed: the three new modules, closed interaction/cobirth and the
existing OCEL transform suite.

## Scope of completion

The general invariant is the event-ordered seen partition. Before each
event, the seen set contains exactly the objects observed at earlier events.
The result adds the cross-product of that event's old and new objects, then
marks its new objects seen. No transitive-closure or timestamp computation is
part of this relation.

The finite corpus alone does not exhaust arbitrary OCEL size. Closure combines
these structural invariants, identity-preserving native input/output, the
observed default/ordering/error contracts and a registered module test. No
full-family, OCEL import/export, composite enrichment or performance promotion
follows automatically.

`ocel-lifetime-graphs-checks.jsonl.gz` (**139,369 bytes**, SHA-256
`604a3ee819a00b2dfb23e9f6e9b17554c566a3d74421d022eabd522bb02915fd`) contains **28 byte-verified members**: complete
357-call records and per-module counters/interfaces/pins, the recipe/cases,
failed-before and passed-after logs, exact source delta, five module test
sources, assertion-enabled compiler commands and fresh-object link proof.
Every decoded member retains its original path, byte count and SHA-256 and
was rechecked after compression without additional public/native calls.
