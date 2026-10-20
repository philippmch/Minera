# OCEL 2.0 graph computation parity contract

`algo.transformation.ocel.graphs.ocel20_computation` is represented by the
public native entry point `compute_ocel20(log, included_graphs)`. This is an
exact companion to the existing, broader `enrich_ocel` workflow. The latter's
lifecycle rewriting and deduplication behavior remains unchanged.

## Input, selection and returned log

The input is a typed `Ocel` with unique declared event/object identities and
relations referencing those identities. Strings are literal identities,
including empty strings. Duplicate relation rows do not multiply a computed
graph. Stored event-table order determines object lifetimes; relation-row
order, timestamps, labels and types do not replace it.

Omitted or null `included_graphs` includes all five documented graph names.
An empty vector selects none. Names are exact strings; unknown names are
ignored, repeated names do not repeat a computation, and selection order does
not change the resulting triple multiplicities. The typed optional vector
represents effective inclusion semantics. The captured Python enum/string
dictionary aliases and permissive string-container membership are normalized
at that language boundary; a Python dictionary API is not claimed for C++.
The retained mixed-key observation gives the enum key precedence.

The function returns an independent copy of the log and appends computed O2O
triples. It preserves the original O2O prefix **including duplicates and
self-relations**. Computed triples are unique within one computation, but are
not deduplicated against that prefix. Applying it to an enriched result
therefore appends another copy of the computed triples.

Only `object_interaction_graph` adds both orientations. Cobirth and codeath
add the single canonical pair from their direct graph results. Descendants
retains directed orientation. An important measured composite behavior is
preserved: triples qualified **`object_inheritance_graph` use descendants
edges**, rather than the separate direct inheritance relation. All sixteen
inheritance-including subset captures confirm this. On the same-birth,
different-death fixture, direct inheritance contains `b->a`, while the
composite contributes no inheritance triple. The direct graph implementation
and its lifetime contract are unchanged.

The public implementation emits newly computed rows in hash-set iteration
order. Native output uses deterministic lexical triple order. Tests compare
the complete triple **multiset**, never a deduplicated edge projection, and
separately require the original prefix in its original order. Dataframe row
indices and hash-set presentation order are not graph-relation invariants.

Every other native stored field is copied unchanged: ordered events and
objects with their attributes, E2O qualifiers, E2E rows, object changes,
globals, declared type catalogs and optional table-column declarations. Native
tests include duplicate attributes, nulls, signed zero, arbitrary bytes,
embedded NUL, WTF-8, unused schemas and independent result mutation. No
round-trip through an import/export projection reconstructs this state.
The public capture separately retains all six complete table snapshots and
public configuration, including additional dataframe columns and nested
public metadata. Those observations establish that this operation leaves
non-O2O data alone; they do not introduce dataframe-only storage members into
the typed native `Ocel` representation.

## Errors and dispatch

Selected lifetime computations reject declared events without relation
groups. The public call raises `KeyError` for the first offending event.
Native code raises `std::out_of_range`, preserving the offending identity and
diagnostic meaning. Dispatch is interaction, descendants, composite
inheritance, cobirth, codeath, independent of selection-vector order.
Descendants, composite inheritance and cobirth inspect forward event order;
codeath inspects reverse order. Consequently all fifteen captured default
failures select the forward offending identity. Unselected computations do
not introduce their errors. An empty or unknown-only selection returns a copy
without running graph computations.

Native duplicate-identity and unknown-endpoint diagnostics remain explicit.
They are not represented as captured public outcomes for malformed references.
Python exception class/repr spelling is not a C++ API compatibility goal.

## Evidence and tests

Root first executed a zero-algorithm-call public signature/docstring/enum
probe, then one finite bank of **134 public calls: 119 returns and 15 expected
errors**. There were zero input/configuration or parameter mutations, and
every successful result was a distinct object. The bank contains all 64
two-event/three-object incidence patterns, all 32 graph subsets on a lifetime
fixture, ten lifetime controls and 26 selection/identity/order/metadata/O2O
controls, including two repeated calls. One repeat reuses the same source;
the other passes the actual returned log to its second call.

Opaque installed-module hashes are checked before and after execution against
PM4Py 2.7.22 commit `0715c31f3f1d85924107d6aa5329806db69df899`.
No reference source, implementation bytecode or private state was inspected.
No successful public call was replayed.

The initial companion used the direct inheritance helper and failed 53 of
the 134 literal outcomes. Those failed receipts are retained. Only the
companion's inheritance dispatch was corrected after comparing the actual
subset and lifetime outputs. The direct graph helper did not change.

`test_ocel20_computation_module.cpp` embeds all 134 literal captured outcomes
and compares complete multiplicity, preserved prefix, input snapshots and
options. It also tests selected error order, full native metadata copying,
independent result storage, repeated enrichment and the explicit divergence
from direct inheritance. The final focused `/UNDEBUG` build compiles the
current `ocel_transform.cpp` object before the frozen library; a link map
binds `compute_ocel20` to that fresh object. The registered target is
`minera_ocel20_computation_module_tests`. Its focused CTest passed, matching
**134/134** retained outcomes plus the native invariants. The older broad
enrichment implementation is tested separately and retains its behavior.

Root's subsequent joint build compiled all current closure implementations and
passed **17/17** affected suites, including this target, all five direct graph
modules and the existing transformation suite. Actual build and CTest logs are
under `build/parity-closures/native-current`. This is a focused integration
check, not a fresh execution of every native suite.

Closure combines the finite observations with the structural relation
invariants and identity-preserving copy/append implementation; it is not a
claim that 134 examples exhaust arbitrary log size. It does not promote OCEL
I/O, arbitrary dataframe constructors or unrelated support modules.

`ocel20-computation-checks.jsonl.gz` (**157,295 bytes**, SHA-256
`5d8a3c2315cd84637198f7c2751de813341e624facc06ea0005d95fb4a4074a5`)
contains **30 byte-verified members**: the public surface probe, full semantic
recipe/cases/pins and all 134 records, execution receipts, the failed first
companion and corrected focused checks, native source/header deltas and final
test source, assertion-enabled compiler commands and fresh-object link proof.
