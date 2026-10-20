# OCEL object cobirth graph parity contract

The module `algo.transformation.ocel.graphs.object_cobirth_graph` is complete
under the repository's semantic parity definition. This closes one relation;
it does not promote the containing OCEL graph support family.

The native entry point is `discover_ocel_object_graph(log,
OcelEnrichmentGraph::kCobirth)` in `ocel_transform.hpp`. Supported OCEL input
uses unique declared event/object identities and relations referencing them.
String identities are literal, including empty strings. Events are processed
in their stored event-table order. At each event, previously unseen objects
form unordered cobirth pairs with one another. Objects seen at an earlier
event do not join those new pairs. Duplicate relation rows/qualifiers do not
multiply edges or create self-loops. Event timestamps and relation-row order do
not determine birth order. Objects without relations are isolated metadata.

A declared event without any object relations is an error, even when it is an
otherwise unused event. The public function raises `KeyError` for the first
such event in event-table order. Native code raises `std::out_of_range` with
`OCEL cobirth event has no object relations: <event-id>`. This preserves the
failure condition, offending identity and diagnostic meaning; it is not a
claim of Python exception class or representation compatibility. Empty input
and objects-only input return an empty edge set; events-only input fails at
the first event.

The public result is an unordered set of string pairs; native edges are its
canonical lexical vector. Native `object_ids` additionally includes isolated
declared objects. Public omitted, null, empty and irrelevant parameter
dictionaries add no effective control; the observed module exports no
`Parameters` enum. Custom OCEL column bindings normalize into typed native
identity fields. Names/types, activities, timestamps, attributes, O2O/E2E and
object-change metadata do not affect this relation.

## Actual public evidence

Root executed `build/ocel-cobirth-focused-prepared-1/capture-public.py` once
against the retained opaque PM4Py 2.7.22 runtime, pinned to
`0715c31f3f1d85924107d6aa5329806db69df899`. All **97 calls** retain exact
results/errors, all six public tables, public configuration and parameter
snapshots. There are **74 returns, 23 errors and zero input/parameter
mutations**. Installed module hashes match the preceding pinned capture before
and after execution. No reference implementation, bytecode or private state
was inspected.

The cases cover every two-event/three-object incidence matrix, parameter
forms, duplicate/qualified rows, empty/isolated inputs, literal Unicode and
empty IDs, and custom columns. Seven independent ordering controls distinguish
event-table order from relation order and timestamps. In particular,
`e0:{a,b}, e1:{b,c}` yields `a-b`; reversing only event rows yields `b-c`,
while reversing/interleaving relation rows or timestamps retains `a-b`.
Constructor snapshots confirm that those relation permutations were retained,
so this is an observed graph contract rather than assumed import sorting.

## Native repair and regression

The original native metric already used the correct event-table ordering.
The frozen Feature99 core matched only **72/97** outcomes: it rejected two
empty-ID inputs that publicly returned edges, and returned graphs for the
23 cases where the public function failed for an unrelated event. The original
failed native run is retained; no successful public case was replayed.

Only the direct cobirth selector now permits empty IDs and requires every
declared event to have relations. The private helper flags default to their
prior behavior, preserving enrichment and unrelated graph routes. Duplicate
identities and unknown relation endpoints still fail. The interaction
regression was adjusted only to stop expecting the newly supported cobirth
selector to reject empty IDs. Existing import/transport identifier policies
were not broadened by this native module repair.

`test_ocel_cobirth_module.cpp` embeds all **97 captured outcomes** directly:
74 exact edge sets and the 23 offending event identities. It also checks full
native source immutability, inactive typed/byte metadata and preserved native
validation. Subsequent direct codeath/descendants/inheritance repairs extend
literal-ID support to all five direct relations; their own event-error checks
do not change this cobirth contract. A focused MSVC Release build compiled the changed
`ocel_transform.cpp` object and linked it before the frozen archive; its link
map identifies the fresh graph/index/cobirth symbols. `/UNDEBUG` enabled
assertions. The new cobirth suite matched **97/97**, and the interaction module
and existing OCEL transform suites also passed (**3/3 CTest suites**).

The finite corpus alone does not cover all OCEL sizes. The general structural
invariant is one event-ordered partition by first occurrence: each object is
assigned to its earliest declared event with a relation; each resulting birth
set contributes its unordered pairs. The implementation deduplicates within
an event and unions pairs, so ordering-independent fields and relation
duplicates cannot alter the result. Closure combines that invariant, the
observed ordering/error/default contract, identity-preserving inputs, native
entry point and registered regression. Other OCEL graph/I/O units retain their
own statuses.

## Repository evidence archive

`ocel-cobirth-checks.jsonl.gz` contains **23 byte-verified members**: complete
public records/interface/execution/validation, the recipe/cases/pins, failed
72/97 native output, exact source delta, test sources, passing CTest log,
assertion-enabled compiler commands and fresh-object link proof. Each member
retains its original path, byte count and SHA-256. The archive is **47,198
bytes**, SHA-256
`e0d180fb127995f5903ebda60ff7c6687e29530add0858e5982ffbe6a30b37e1`.
All members were decoded and hash-checked after compression without additional
public or native calls.
