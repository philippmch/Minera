# OCEL object interaction graph parity contract

The module `algo.transformation.ocel.graphs.object_interaction_graph` is complete
under the semantic parity definition in `reference-2.7.22-parity.md`. This is one
relation, not completion of the six-module OCEL graph support family.

The public Minera entry point is `discover_ocel_object_graph(log,
OcelEnrichmentGraph::kInteraction)` in `ocel_transform.hpp`. Its normalized
input is an OCEL with unique declared event/object identities and relations
referencing those identities. Every string value is literal identity data,
including an empty string. Duplicate relation rows and different qualifiers do
not create multiple edges or self-loops. Distinct objects sharing an event
form an unordered pair; the result is the union of those pairs over events.
Names/types, activities, timestamps, input ordering, attributes, O2O/E2E records
and object-change metadata do not affect this relation.

The public result is an unordered set of string pairs. The native edge vector
is its canonical lexical representation, with each undirected pair once. The
native `object_ids` member additionally retains isolated declared objects for
rendering; it does not change the public edge set. Event/object column bindings
are normalized into the typed OCEL fields. Public omitted, null, empty and
irrelevant parameter dictionaries return the same relation; the observed module
exports no `Parameters` enum. No additional native parameter family is needed.

## Actual public observations

Root executed `build/ocel-interaction-focused-prepared-1/capture-public.py` once,
using the retained PM4Py 2.7.22 runtime pinned to
`0715c31f3f1d85924107d6aa5329806db69df899`. The direct public capture is
`build/ocel-interaction-focused-public-actual-1/records.jsonl`, SHA-256
`2a67ad1e41caf687706d19c862450b4c4bd94844acfb0bb8eff3a9a3150fa7d5`.
Its companion `validation.json` records **90 calls, 90 returns, zero errors,
zero input mutations and zero parameter mutations**. Before/after snapshots
include all six public OCEL tables and the known public configuration fields.
Opaque installed-module hashes match the preceding pinned capture before and
after; no reference implementation, bytecode or private state was inspected.

The cases include every two-event/three-object incidence matrix (64 cases),
12 parameter-form controls, duplicate and qualified relations, independent
relation/event/object order controls, reversed timestamps, Unicode/literal IDs,
empty event/object IDs, isolated records, all three empty-table shapes and a
custom-column configuration. These observations retain actual set/tuple/string
values and complete input snapshots, not type-level OTG frequency aggregates.

## Corrected behavior and tests

The original native enrichment index rejected empty IDs before computing an
interaction graph. The new focused test linked against the unchanged Feature99
core reproduced **88/90 matches**, with exactly the empty-object-ID and
empty-event-ID cases rejected. Their public counterparts both returned the
expected three co-event pairs.

`make_enrichment_index` now accepts an `allow_empty_ids` control, default false.
The interaction repair enabled it only for that direct selector. Unique identity
and declared endpoint checks remain. The subsequent cobirth module repair also
enables empty IDs for direct cobirth. Later direct descendants, inheritance and
codeath repairs extend the same literal-ID rule to all five direct relations;
enrichment retains its previous nonempty-ID policy. Existing import/transport identifier
policies are unchanged; this native relation fix is not a claim that every
import adapter now accepts empty IDs.

`test_ocel_interaction_module.cpp` embeds all 90 captured edge sets directly,
without computing expected results from the native formula. It compares the
full native source before/after, supplements the corpus with inactive typed and
byte-string metadata, and checks preserved duplicate/endpoint validation and
other-selector rejection. Native invalid-input diagnostics are not described
as captured Python exceptions for malformed OCELs.

A focused MSVC Release build compiled the changed `ocel_transform.cpp` object
and linked it before the validated Feature99 core archive. The link map binds
the actual graph/index symbols to that fresh object. Assertions were enabled
with `/UNDEBUG`. Both `minera_ocel_interaction_module_tests` and the existing
`minera_ocel_transform_tests` passed; the new suite matched **90/90** public
cases. Logs and the map are retained under
`build/ocel-interaction-focused-native/after`. The failed-before output remains
alongside them. No successful public case was replayed.

The finite corpus alone is not an exhaustive test of arbitrary OCEL size.
The general contract follows the per-event clique/union invariant: deduplicating
object identities within each event produces each distinct co-event pair once,
and set union removes duplicates across events. The implementation preserves
that invariant independently of ordering and inactive metadata. Closure combines
this structural argument, normalized public input/output contracts, actual
parameter observations, explicit native diagnostics and the registered test.
The remaining graph relations and OCEL import/format contracts keep their own
statuses; no package or family promotion follows from this module alone.


## Retained repository evidence

`ocel-interaction-checks.jsonl.gz` retains **20 byte-verified members**: the
capture recipe/cases/pins, all 90 complete public observations and interface,
actual execution/validation records, unchanged native test source, original
88/90 failure output, the exact interaction-only source patch, passing focused
CTest log, assertion-enabled compiler command records and fresh-object link
proof. Each decoded member carries its original path, byte count and SHA-256.
The archive is **39,576 bytes**, SHA-256
`4a1b7e883d79108ffdcce55047e127515b81e56883ba34f944ff1d45d2a28eff`.
Every member was decoded and checked after compression; this added no public
or native calls. The subsequent root integration independently passed the
joint current-source assertion-enabled suite, including this new module test
and the existing OCEL transform test, before application relinking.
