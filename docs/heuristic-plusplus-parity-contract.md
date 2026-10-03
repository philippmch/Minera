# PLUSPLUS parity contract

The target is the complete public surface of
`algo.discovery.heuristics.variants.plusplus` in PM4Py 2.7.22, pinned to commit
`0715c31f3f1d85924107d6aa5329806db69df899`. The module remains **partial**.
The gates below include returned-object values, public routes and parameters,
input preservation, and Petri conversion. A matching graph projection alone
does not complete this contract.

## Saved checkpoints

`heuristic-plusplus-returned-fields-validation.json` binds twelve actual public
EventLog calls, twelve actual native calls, the raw captures, and four passing
Release suites. The public worker calls `algorithm.apply_heu` with
`variant=algorithm.Variants.PLUSPLUS`; the saved signatures also describe the
direct variant route. Those signatures do not establish direct-route behavior.
The observations cover standard and escaped activity labels, min-activity
counts 1/2/10, frequency/performance decoration, interval overlap, and empty
inputs. All seventeen public net fields were observed, including six fields per
node and three numeric fields per outgoing edge.

The native checkpoint compares the sojourn map, node flags, and selected graph
projections. Its twelve cases match. The sojourn map retains activities removed
by filtering, and empty input yields an engaged empty map. Both public node
flags are false in all twenty-one retained nodes; native boundary occurrence
counts remain separate data.

The compiled source tree is `513be22bc70ad27b4e08bf72c37d96b88768436b`, built
from parent `f25cfa208175222e8db281075534d582c4055490` with the tested source
changes. The build manifest identifies its actual binaries and inputs. Before
commit `cb810e5`, a fresh staged export matched all compiled source bytes and
passed the structural catalogue verifier. Commit identity and compiled-tree
identity are distinct provenance fields.

`heuristic-plusplus-route-object-validation.json` preserves a subsequent 112
actual public calls: 60 HeuristicsNet calls and 52 Petri calls. Direct and
parent EventLog routes match in all 27 paired HeuristicsNet controls and all
23 paired Petri controls. The 109 successful results include sequence, XOR,
positive AND, interval overlap, filtering, custom selectors, and flat carriers.
Three parent DataFrame Petri calls raise `AttributeError` because their captured
timestamp columns have object dtype; the errors remain in the evidence.
Correctly datetime-typed DataFrame Petri controls remain required. None of the
112 inputs changed.

The same checkpoint compares twelve fresh, complete native objects against
twelve reused public objects. All seventeen net fields, six fields per node,
and three fields per outgoing edge produce 348 comparison cells: 248 matched,
60 ambiguous, and 40 unrepresentable. Ambiguous candidates retain their actual
values and are not passing checks. In particular, an unset native
`performance_values` input is not a computed public `performance_dfg` result.
Native auxiliary empty maps do not represent public `None`, and an unset edge
scalar alternative does not establish the integer type of a frequency value.
The full returned-object gate remains open.

Only the new whole-object probe was compiled; the frozen core and its earlier
four Release suites are reused. Its successful execution records fourteen
commands and twelve actual native calls. A separate configure cleanup failure
records zero native calls. The deterministic archive retains 96 exact UTF-8
members, all command logs and errors, and 654 external byte/hash bindings.

`heuristic-petri-inputless-native-validation.json` supplies the separate narrow
PLUSPLUS inputless control: five places, four visible transitions, five arcs,
unwired A/B inputs, private output places, and source/final markings. It does
not settle connected, branching, or cyclic PLUSPLUS Petri structures.

`heuristic-plusplus-supplement-validation.json` preserves seventy further actual
public calls with seventy successful results and no reference or serialization
errors. These include eight correctly datetime-typed DataFrame calls, missing
start timestamps, lifecycle events, repeated traces, reserved-name activities,
and filtering with positive concurrency. Twenty-one paired HeuristicsNet
results and nine paired Petri results match on direct and parent EventLog
routes. Four parent DataFrame Petri calls append a float `@@diff` column; the
before/after snapshots retain that mutation. The captured timestamp dtype is
`datetime64[us, UTC]`, distinct from the preparation's planned nanosecond dtype.

The original worker exits 1 after saving all seventy results because its own
verification omitted the returned `pandas.Timestamp` alias. A subsequent
read-only finalization corrects that alias and accepts the observed dtype,
adding no public or native calls. The archive preserves the original failure,
the complete raw results, and the finalization separately. It contains 64 exact
UTF-8 members and fourteen external byte/hash bindings.

Plain ABC traces return AB and BC, with no AC relation. Overlapping and
non-neighbor interval controls below distinguish those counts from literal
event adjacency. Positive raw concurrency
retains a removed activity under filtering. A dependency threshold immediately
below zero accepts reciprocal ABA edges and produces an actual cyclic Petri
net; the equal and above-zero controls exclude those edges. Self-loop controls
capture HeuristicsNet values only. Reserved names collide across place and
transition kinds, while captured names remain unique within each kind.

PLUSPLUS frequency relation and edge representations now preserve an engaged
integer scalar alternative. A fresh build of canonical source tree
`fdfbd92359fe459ba6f4bf82c6df4ef6d2129815` passes the core, heuristic variants,
Petri conversion, and Petri reduction Release suites with assertions enabled.
The regression also checks threshold exclusion and unchanged floating-point
performance representation. Broader native field and conversion acceptance
remains open.

`heuristic-plusplus-frequency-build-validation.json` binds that fresh build,
its 650 compile inputs, six actual binaries, exact source changes, and four
passing suites. The archive retains three earlier configure failures
separately. A shared-dictionary discrepancy in embedded configure/build
metadata is documented; the SHA-bound command files preserve the actual
before-state. All original bytes remain available.

`heuristic-plusplus-broad-native-validation.json` preserves seventy actual
native discoveries and forty-three conversions against the same tested core.
The 194 reused public aliases deduplicate to 71 exact cases; one unfamiliar
decoration case, covering two aliases, remains unrepresentable and receives
no native call. The successful capture executes 72 commands in seventy probe
processes with no discovery or conversion errors. All input and option
snapshots, complete returned fields, Petri incidences and markings remain raw.

The original comparison covers 119 HeuristicsNet aliases and 4,573 cells:
3,823 match, 234 are ambiguous, 458 are unrepresentable, and 58 mismatch.
All 58 differences concern the original literal-adjacency candidate for
`dfg` and `dfg_matrix`, with 29 aliases in each field. Each affected public
DFG instead equals the captured native `eventually_follows_frequency` map;
its matrix contains the same pairs and counts. For example, overlapping
intervals yield BD=4 and CD=4 where literal adjacency counts each twice.
An interval control also yields AC=1 with zero literal adjacency. The original
diagnostic is preserved; these differences do not establish missing native
frequency counts. The separate actual comparison below applies the corrected
projection without changing those original observations.

The archive retains two earlier probe failures. Compilation failed with
MSVC C1128 before any native call. The next attempt compiled, but its first
uninstrumented process overflowed its stack; discovery and conversion counts
are unknown for that process. Separate non-inlined case factories and the
required `/bigobj` option produce the successful capture. Its flushed call
markers agree with final returned counts. The archive contains 315 exact
UTF-8 members and 660 external byte/hash bindings, roundtripped with the
unchanged reviewed codec. It preserves all 75 Petri aliases for a separate
comparison and keeps names, properties and arc-weight obligations explicit.
The four passing Release suites and 650 compiled inputs are reused; no new
public calls or production source changes occur in this evidence checkpoint.

`heuristic-plusplus-broad-comparison-validation.json` preserves the separate
corrected comparison: 119 HeuristicsNet aliases, 4,573 cells, 3,881 matched,
234 ambiguous, 458 unrepresentable, and zero mismatched. All 117 supported
aliases match their complete interval-frequency DFG and nested matrix; the
two unfamiliar-decoration aliases still have no native counterpart. The
frequency, dependency, sojourn, node-count, and edge-value matches retain
their actual types and complete domains. Nullable auxiliary fields, computed
performance output, positive AND maps, and unresolved projections remain
explicit obligations rather than passing cells.

The separate Petri comparison covers all 75 aliases. Projected topology
matches in 69; six differ. Three are the original object-dtype DataFrame
reference errors against native success. The other three use correctly typed
DataFrames with overlapping intervals: each public result has four places,
four transitions and eight arcs, against native five places, four transitions
and nine arcs. Four public DataFrame input mutations remain recorded; native
inputs are unchanged. Bijections preserve visible labels, silent transitions,
directed multiplicities and initial/final token values. Raw explicit-zero and
omitted marking domains remain distinct. Search exhaustion cannot pass.
The 69 topology matches remain unrepresentable for full Petri acceptance
because native names, properties and arc-object/weight carriers are absent.
The separate 29-member archive roundtrips every exact UTF-8 byte, SHA and
count and binds 935 external artifacts, including the original broad capture.
No additional public or native calls, production edits or suite runs occur.

`heuristic-plusplus-filtered-performance-validation.json` preserves fourteen
additional actual public EventLog HeuristicsNet calls: seven direct/parent
pairs with all nine parameters explicit. Every pair has identical typed DFG,
performance DFG and performance matrix values. Activity and minimum-frequency
filters remove the same retained interval relations from all three maps;
dependency exclusion removes edges while leaving those maps unchanged.
An entirely filtered performance result retains three present empty maps.

The minimum-frequency-five control retains BD with interval count five, while
its mean performance is `1.6666666666666667` from three direct waits, 2/2/1.
The interval control returns AC=1 and BC=1 in its frequency DFG; its performance
DFG contains only BC=1.0, while its matrix also contains AC=0.0. For these
observed inputs, the performance DFG domain is retained interval relations
with positive direct frequency, and the matrix covers every retained interval
relation. These observations distinguish computed output from the optional
aggregated `performance_values` input. Missing or invalid timestamps and
additional native performance aggregations remain outside this checkpoint.

The actual worker exits zero with fourteen results, no reference or
serialization errors, and no input mutations. The enclosing capture exits one
after saving those results because a Matplotlib cache diagnostic violates its
own empty-stderr assertion. A read-only finalization verifies the exact saved
diagnostic and original results without repeating any public call. An earlier
preparation failure, before worker creation or public imports, also remains
separate: frozen compiled-source bytes and current working-source bytes are
different physical contexts, including line endings. Exact guards are retained
independently; no normalized-byte equivalence substitutes for those guards.

The archive preserves 726 members, including immutable snapshots of all 650
working-source files, distinct compiled-source bindings, original logs and
failures, and finalization proof. All working snapshots are UTF-8; the saver
supports binary wrappers when needed. The prior 23 helper checks and subsequent
29 capture, 21 finalizer, and 22 saver checks are retained. Saving adds no public/native calls, production
changes or suite runs, and does not promote the module.

## Returned-object acceptance

The separate tested production checkpoint adds independent PLUSPLUS auxiliary
presence flags and computed mean performance outputs. Discovery marks
`dfg_window_2` and `freq_triples` absent. Frequency output leaves computed
performance DFG unset and records floating zero matrix values. Performance
output engages its DFG before filtering, retains an empty result, includes
retained interval relations with direct waits, and records all retained
interval relations in its matrix, including zero for a relation with no direct
wait. Dependency edge exclusion does not remove either map. These mean maps
remain independent of the selected edge aggregation and the supplied
`performance_values` input.

The `plusplus_public_fields` JSON block exports absent values as `null`,
concrete pair maps as key/value arrays, and the performance matrix as nested
raw-name keys. Floating values preserve numeric tokens such as `0.0`, signed
zero, escaping and existing nonfinite-value rejection. Legacy auxiliary-map
defaults and CLASSIC behavior remain independent.

`heuristic-plusplus-computed-fields-build-validation.json` binds a fresh build
of canonical source tree `2b270cf8a692e15b870803a14d2e8d139a05b4a1`, exact
export tree `e54fa844adf5875c87a5348080204e5a1c08af1d`, and core SHA
`66c05425798fe7a57bdf779d07a1dbf040a0802d606eb79ea2da2e2a3accda59`.
Core, heuristic variants, Petri conversion and Petri reduction Release suites
all pass with assertions enabled. Literal regressions cover the new presence,
mean/filter/zero domains, dependency and aggregation independence, and JSON
edge cases. Fourteen source checks, fifteen builder checks and twenty saver
checks are retained. Each embedded command record equals its actual disk
record; independent binding snapshots prevent retrospective metadata changes.
The 56-member archive binds all 650 compile inputs, six binaries and 1,320
immutable external artifacts. Full comparisons of the new fields across the
broad public corpus, carrier differences and full Petri acceptance remain
required; this tested source checkpoint does not promote the module.

`heuristic-plusplus-broad-raw-projection-validation.json` preserves a further
comparison of the same 119 aliases and 4,573 cells: 4,023 matched, 117 ambiguous,
433 unrepresentable, and zero mismatched. The complete raw-name concurrency
map matches all 117 supported aliases, including the two paired controls that
retain BC=4 after C is removed from the active node table. Retained-ID
concurrency remains separate backing data and is not the public projection.

Twenty-five positive nested AND maps now match projections of actual native
rows. The projection preserves center, input/output side, canonical raw-name
neighbor pairs and floating scores; empty maps and missing backing are kept
distinct. Captured positive scores include 1.0, 0.8 and 0.5. These finite
matches do not settle threshold equality or positive-map label collisions.
The remaining 117 ambiguous cells are computed performance matrices in the
earlier native capture. Nullable auxiliaries and computed performance DFG
still lack counterparts there. The separate actual 75-alias Petri diagnostic
is reused; this HeuristicsNet report's pending Petri entries are not new tests.

The 14-member archive retains the complete actual report, preparation,
manifest, attempt, frozen helper and 28 projection checks, with seventeen
saver checks. All exact UTF-8 bytes, hashes and counts roundtrip using the
unchanged reviewed codec. The earlier corrected report and broad capture
remain immutable, externally bound evidence. This report and save add no
public/native calls, production edits, suite runs or catalogue promotion.

Presence, null, empty values, numeric types, and actual keys must be retained.
Derived matrix views may share backing data, but their complete values and key
domains still require comparison. Unavailable native representation must be
reported explicitly rather than filled with a presumed default.

| Public fields | Native data to compare | Remaining acceptance |
| --- | --- | --- |
| `activities`, `activities_occurrences`, `start_activities`, `end_activities` | Net metadata and boundary counts | Compare complete key domains through filtering, including an edge incident to a removed activity; retain `[{}]` boundary containers for empty results. |
| `dfg`, `dfg_matrix` | Retained `eventually_follows_frequency` relation statistics | Corrected complete maps and nested matrices match all 117 supported aliases, including overlapping and non-neighbor intervals. Current PLUSPLUS raw/cleaned DFG maps are empty in the saved cases; unfamiliar decoration has no native counterpart. |
| `concurrent_activities`, `dependency_matrix` | Raw-name concurrency and retained dependency statistics | Complete raw-name concurrency matches all 117 supported aliases, including positive pairs with filtered activities. Dependency matrices also retain excluded edges. Keep both complete domains when extending the input corpus. |
| `dfg_window_2`, `freq_triples` | Auxiliary maps and independent PLUSPLUS presence flags | Saved public values are `None`. The tested new discovery marks both fields absent while preserving independent legacy concrete-map defaults. Fresh broad native counterparts of the flags remain required; earlier captures expose only empty maps. |
| `dfg_window_2_matrix`, `freq_triples_matrix` | Views of the auxiliary maps | Empty dictionaries match the saved cases; the full domain and any positive values remain to be established. |
| `node_type` | Decoration | Compare frequency and performance results, including edge value types. |
| `performance_dfg`, `performance_matrix` | Separate computed performance presence and complete maps | Frequency public DFG is `None`; performance DFG is a dictionary, including a present empty dictionary. Seven performance route pairs establish retained interval filtering, direct-wait means and matrix zero for a relation with no direct wait. The tested new native outputs preserve these domains and dependency independence. Fresh complete typed counterparts across the broad corpus remain required. |
| `sojourn_times` | Optional name-to-double map | All 117 supported aliases match, including escaped labels, removed activities, and empty input. Retain this verified obligation when broader inputs are added. |
| `nodes` | Retained activities, node statistics, maps, and connections | Twenty-five positive AND maps match projections retaining center, input/output side, canonical raw-name neighbors and float scores. Threshold equality and positive-map label collisions remain open; actual keys and all fields below remain required for new inputs. |

Each node requires `node_occ`, `is_start_activity`, `is_end_activity`,
`loop_length_two`, `and_measures_in`, and `and_measures_out`. Each outgoing edge
requires `dependency_value`, `dfg_value`, and `repr_value`. Frequency
representation is an integer in the saved positive case; performance
representation is a float. Keep the native scalar alternative when comparing
these types. Edge multiplicity and connection keys are part of the comparison.

## Routes and parameters

The direct variant routes are `plusplus.apply_heu(EventLog, parameters=None)`
and `plusplus.apply(EventLog, parameters=None)`. The parent dispatcher adds
EventStream and DataFrame carriers. Native flat carriers adapt to EventLog
before PLUSPLUS dispatch; the native direct DFG overload is CLASSIC-only and
cannot satisfy a PLUSPLUS gate.

Pair direct and parent routes on identical fresh EventLogs, and compare both
HeuristicsNet and Petri results. Flat-carrier controls must contain distinct
case IDs and explicitly selected completion/case-ID columns. Preserve full
input and parameter state before and after every actual call.

| Public parameter | Native option | Acceptance status |
| --- | --- | --- |
| `ACTIVITY_KEY` | `activity_attribute` | Standard/custom selectors are covered by the saved twelve cases and the new paired public routes. |
| `START_TIMESTAMP_KEY` | `start_timestamp_attribute` | Custom interval start, missing-start, and lifecycle controls are observed publicly; broader native counterparts remain open. |
| `TIMESTAMP_KEY` | `timestamp_attribute` | Custom completion is observed on public EventLog and flat routes; combine it with performance decoration and native controls. |
| `CASE_ID_KEY` | `case_id_attribute` | Custom grouping and correctly datetime-typed DataFrame Petri calls are observed publicly; carrier-dependent conversion, input mutation, and native counterparts remain open. |
| `DEPENDENCY_THRESH` | `dependency_threshold` | Public dependency 1 is accepted below 1 and excluded at or above 1; its matrix entry remains. Compare the full native results. |
| `AND_MEASURE_THRESH` | `and_threshold` | Positive scores 1.0, 0.8 and 0.5 match the finite native map projections, including distinct input/output sides. Saved next-down/equal/next-up controls for 1.0 and 0.5 now match native fields with escaped labels; complete AND Petri fields and broader domains remain open. |
| `MIN_ACT_COUNT` | `min_activity_count` | Public controls remove incident relation maps and edges while preserving raw sojourn entries; the complete native field domains remain to be compared. |
| `MIN_DFG_OCCURRENCES` | `min_frequency` | Public count 2 remains at minimum 2 and is removed at minimum 3, including the returned DFG and matrices. Compare native domains. |
| `HEU_NET_DECORATION` | `decoration` and `plusplus_decoration_selector` | Saved frequency/performance field controls and the explicit `supplement-unfamiliar` selector match native presence and scalar types. Other unfamiliar strings, explicit null and non-string selectors remain unverified. |

## Petri and completion gates

Add connected sequence, XOR/AND, cycle/self-loop, and filtered structures to the
existing inputless control. Retain actual names, properties, arc weights,
multiplicity, markings, and endpoint identities. A comparison based on unique
names must reject or separately handle collisions rather than silently merging
places or transitions.

The public controls establish connected sequence, XOR, positive AND, filtered
structures, and a reciprocal ABA cycle below dependency threshold zero.
Reserved-name controls preserve place and transition identities separately.
The automatic PLUSPLUS presentation producer now matches all 69 previously
matching projected topologies on complete supported Petri fields, including
names, properties, arc occurrences and weights, endpoint identity, and sparse
markings. Thirteen additional naming, escaped-label AND, and AAB self-loop
controls also match. This finite acceptance is saved in
`heuristic-plusplus-petri-producer-validation.json`; the module remains partial.
Three datetime-typed DataFrame Petri shape differences and three object-dtype
reference errors against native success remain open. Public entry-point controls now match the parent DataFrame Petri result to
`plusplus.apply_pandas` at all three observed AND thresholds. The exported
`apply_heu_pandas` returns different concurrency counts and AND maps from
parent/direct `apply_heu` for the same fixture. Native pandas discovery,
mutation, and error behavior remain open; the public controls are recorded in
`heuristic-pandas-dispatch-validation.json`.

Complete the module only after the full field, route, parameter, input, and
Petri gates above are reconciled against actual pinned public calls and actual
native results, with any representation choices documented. The catalogue
proof must use a distinct tested production symbol and acceptance test, and
pass the structural verifier. Finite samples do not justify dropping public
parameters, carriers, positive maps, or conversion obligations from the scope.

## Explicit string decoration selector checkpoint

`HeuristicMinerOptions::plusplus_decoration_selector` now accepts an optional
string without changing the existing decoration enum. Omission uses that enum;
an explicit `frequency` selects frequency mode, and other explicit strings
select performance mode. Resolution applies within PLUSPLUS and leaves CLASSIC
behavior unchanged. The caller's selector remains intact in input snapshots,
while the returned net and JSON keep the normalized frequency/performance mode.
No raw selector field is added to the returned public net.

The saved public supplement rows 68/69 pass `supplement-unfamiliar` and return
normalized performance mode. Compared with the same-input frequency rows 56/57,
thirteen top-level fields are identical; performance DFG, performance matrix,
node type and node edge representations differ. Their A-to-B and B-to-C means
are floating 1.0, while frequency counts remain integers 3 and 2. This finite
observation motivates the selector representation. Other unfamiliar values,
explicit null and non-string inputs remain unverified public domains.

The registered native regression covers selector precedence over conflicting
enum values, unchanged omitted-enum behavior, the captured unfamiliar selector,
preserved counts/dependencies/boundaries, mean maps and scalar representations,
unchanged options, and CLASSIC ignoring the selector. Mean performance keeps
its existing unset optional representation payloads; frequency retains integer
payloads. The original static assertion mistake is retained as superseded
evidence beside its corrected eleven-check preview proof.

A fresh build passes core, heuristic variants, Petri conversion and Petri
reduction Release suites with assertions enabled. Actual configure, build and
test records exit zero with finalized logs and exact independent snapshots.
Canonical and physical exported source trees both equal
`fd33276fdaea03e862e7081f5262a02c0b97ff78`. Actual build-manifest SHA is
`2706f1e83c67b6e4a3692dbe521b9ee7dae566577f5189b7a37d9b689d1e2fa2`;
core SHA is
`a657bffd3374a0996f7e6cc1ce70a45d5134c6b5ce8bb640a21f8bf187725d44`.

`docs/heuristic-plusplus-decoration-build-checks.jsonl.gz` and its validation
preserve 71 exact UTF-8 members and 1,336 immutable external bindings, including
all 650 compiled inputs and six actual binaries. Original thirteen source
checks, the superseding eleven checks, 23 harness checks and twelve saver checks
remain bound to their exact origins. Every member byte, SHA and count roundtrips
with the unchanged reviewed codec. The archive is 1,328,077 bytes, SHA
`02caea4be7aadb75928837244cf079d7fec7a0b503ae3fe84bc01ca281102942`;
validation SHA is
`c6513ac889af72debfbc5ee740015e06d19bea6e1a1a0a99eaf9fe1e7803d784`.
Saving adds zero build/test commands or public/native discovery calls. The
focused native comparison against the saved public selector and label controls
is a separate step; this checkpoint does not claim its results. Full returned
objects, carrier conversion and Petri completion remain open. PLUSPLUS stays
partial, with all 228 open coverage units retained.

## Focused selector, AND-label and self-loop comparison

The source checkpoint `8864276` is followed by a fresh capture from the same
tested build. Eight actual native discoveries and two conversions complete
in eight supervised processes and ten command records, with zero public
calls. Eighteen saved public aliases map to eight distinct inputs: fourteen
HeuristicsNet aliases and four Petri aliases. The unfamiliar selector controls
are supplement rows 68/69; the remaining sixteen aliases are the saved AND
boundary, escaped-label and self-loop controls.

All 718 compared HeuristicsNet cells match, including both unfamiliar-selector
aliases that the previous enum-only carrier could not represent. The retained
raw selector remains distinct from the normalized returned performance mode.
The comparison preserves existing typed normalizers and does not infer results
for other selector values or input domains. Report SHA is
`3bfcdbcc63563c6aff74174e2679e651f5ad12d3f53463402cfb64dff3d0dac7`.

All four Petri aliases match projected topology and markings: the two renamed
AND controls have six places, four transitions and ten arcs; the two AAB
self-loop controls have three places, two transitions and four arcs. Public
names, properties, explicit arc weights and full sparse marking objects remain
unrepresented by the native carrier. Full Petri parity remains false. Report
SHA is `f30248e9c3dcddee0927deb71017533fc660009f0c1ebe9346a9e867e00ee5b9`.

The first focused capture attempt failed configure because a relative
preparation path was resolved from the private child directory. Its actual
exit-one configure record, finalized empty stdout and 308-byte stderr remain
retained; it made zero build, discovery or conversion calls. The repaired CLI
resolves incoming paths before dispatch and passes eleven focused checks beside
the frozen original 34-check proof. Fresh successful capture and both
comparisons retain independent provenance. Comparing adds zero public/native
calls, builds or suite runs. These finite results leave PLUSPLUS partial and
retain all 228 open coverage units.

`docs/heuristic-plusplus-decoration-object-checks.jsonl.gz` and its validation
preserve 817 exact UTF-8 members, the 650 compiled inputs and six actual
binaries, current helper/proof origins and saved public capture provenance,
including the complete AND-label archive. The original failed attempt, frozen
helper, original 32 saver checks
and twelve provenance-repair checks remain separate from fresh success. All
member bytes, SHA values and counts roundtrip with the unchanged reviewed codec.
The archive is 8,566,703 bytes, SHA
`e8fefcbfca2f80fbc1ea7e047c1b8178be34b43fd0e72d69c4c02977e3bf4505`;
validation SHA is
`8e2918f9fae87afe09f5e3583811570dce5d7ba07ac748d63d23145e5e62bd6b`.
Saving adds zero comparisons, calls, builds or suite runs. Durable validation
uses immutable saved files rather than requiring the worktree or HEAD to remain
at the capture checkpoint.

## Reduction transition identity foundation

`PetriNetReductionResult::old_to_new_transition` now maps every input transition
to its reduced transition index. Unchanged results use identity mappings;
removed transitions use `kRemovedTransition`. Duplicate silent transitions map
to their surviving representative, including aliases transferred through later
fusion. If that representative is subsequently removed, all its original
transition identities map to the removed sentinel. This provides the transition
counterpart to the existing place mapping for a future public Petri metadata
producer.

The new field follows the existing `changed` field, preserving previous
aggregate initializer positions. The C++ result layout and structured-binding
arity change, so downstream users must rebuild and adapt those bindings.
The native generic JSON serializer still exposes its existing topology fields;
full public names, properties, arc objects and sparse markings remain open.

Regression fixtures exercise thirteen scenarios and 46 original transition
entries, including unchanged extended-arc nets, ordinary removals, shifted
indices, three duplicate aliases, later representative removal and abstraction.
The private source proof preserves exact base/preview bytes and the six static
checks; runtime validation comes from the fresh production build. Core,
heuristic variants, Petri conversion and Petri reduction Release suites all
pass with assertions enabled, with zero failed tests across four suites.
Actual configure, build and test records exit zero with finalized logs and
independent immutable command snapshots.

Canonical and physical exported source trees both equal
`5ae437dd47d29534c071e919c970df54f1cd9b6e`. Actual build-manifest SHA is
`56bc8010154ef47b3d4a4fe25c31362bd4af49d13f07f5c2e3011349d9a3384b`;
core SHA is
`43f7b204359291297117772e3d9252fad3c6df35ef07e2e033b750cdef90e549`.
This checkpoint supplies identity mappings; it does not establish a public
naming rule or full Petri acceptance. PLUSPLUS remains partial and all 228 open
coverage units remain in scope.

`docs/petri-transition-identity-build-checks.jsonl.gz` and its validation
preserve 72 exact UTF-8 members and 1,332 immutable external bindings,
including all 650 compiled inputs and six actual binaries. The six static
source checks, 22 harness checks, eighteen saver checks and reused original
harness/guard proofs retain their byte origins. All member bytes, SHA values
and counts roundtrip with the unchanged reviewed codec. The archive is
1,127,310 bytes, SHA
`50197b7f41bc6c5f031770801c7b33ba6b734b7673131321acb6ebd22839c455`;
validation SHA is
`bf991e64e70ece2a50017d329fcd353ff6d7d69a49233713710d22ec4f08df59`.
Saving adds zero build/test commands or public/native calls. Durable validation
uses immutable saved bytes after the source checkpoint is committed.

## Request decoration selector

`parse_mine_params` accepts `heuristic_plusplus_decoration_selector` as a
separate raw string field and assigns it to
`HeuristicMinerOptions::plusplus_decoration_selector`. Supplied strings retain
their bytes, including whitespace, literal escapes and UTF-8. A caller that
provides `FieldPresence` can distinguish an explicitly empty string from an
omitted field. The lookup-only overload treats an empty result as omission.
This establishes parser behavior; it does not establish JSON-null semantics or
prove that every request caller supplies field-presence information.

The existing `heuristic_decoration` field still accepts only `frequency` or
`performance` and rejects other values with its existing error. The raw
selector can coexist with either enum value. Its existing PLUSPLUS precedence
and CLASSIC behavior remain covered by the heuristic suite. Public acceptance
for additional selector domains remains open.

Two registered parser tests cover omission in both overloads, exact string
preservation, explicit emptiness with presence information, independent enum
values and the existing invalid-enum error. The fresh production build passes
all five assertion-enabled Release suites: core, request parameters, heuristic
variants, Petri conversion and Petri reduction. Its three independent configure,
build and test records exit zero with finalized logs; all 650 compiled input
bindings and seven actual binary bindings remain unchanged through validation.

Canonical and exported source trees both equal
`5ab9f0b3ad4f831df54a922c54e61a5038c8a0ee`. Build-manifest SHA is
`543ca290e2c41ba9139a48dec08c63b71a28e58f61e29e4c8329322db0028f27`;
core SHA is
`01d7fb85b2a3660a2d9a9f8fd692b56bbf1dd8e92fab2b8cc68b4c2ef6d180b2`.
The parser change leaves full carrier and Petri object gates open. PLUSPLUS
stays partial, with all 228 open coverage units retained.

`docs/heuristic-plusplus-selector-request-build-checks.jsonl.gz` and its
validation retain 78 exact UTF-8 members and 1,335 immutable external bindings,
including all 650 compiled inputs and seven actual binaries. Source preview,
patch and proof origins, nineteen harness checks, fourteen saver checks and
reused earlier proofs remain distinct. All member bytes, SHA values and counts
roundtrip with the reviewed codec. The archive is 1,738,889 bytes, SHA
`eab6cdab6da24fa2a59ea8b434753f44937a3c71cfa917c7d168ac01e79f6b5e`;
validation SHA is
`b19a1a7eac1cbc0189b93097ce9cb4ea2352e222bde62c093e489de4762785bd`.
Saving adds zero build/test commands or public/native calls. Validation uses
immutable saved bytes after the source checkpoint is committed.

## Petri metadata presentation

`PetriNetPresentation` supplies a separate carrier for explicitly provided net,
place and transition names, optional transition labels, recursive JSON
properties, ordinary arc occurrences and sparse initial/final markings.
Place and transition IDs occupy separate namespaces and remain independent
of names. A named transition may have a null label. Repeated names and arc
endpoint pairs remain separate records; an explicitly supplied zero marking
entry remains present.

`petri_net_presentation_json` serializes that carrier without changing the
existing generic `PetriNet` representation or its JSON output. Its node arrays
retain explicit IDs, arcs retain typed `{kind, id}` endpoints and integer
weights, and marking arrays retain `{place_id, tokens}` entries. Weights use
unsigned 64-bit integers and marking counts use signed 64-bit integers.
Properties reuse `HeuristicStructuredValue::Object`, with string keys and
recursive JSON values. C++ signed/unsigned integer variants share JSON's
integer token family; double values retain floating-point tokens, including
integral-valued doubles and negative zero. String escaping uses the existing
byte-escaping path; callers supply UTF-8 strings.

Validation rejects duplicate IDs within a node kind, invalid or missing arc
endpoints, arcs between nodes of the same kind, unknown marking places,
duplicate entries within a marking and non-finite nested property values.
Names, transition labels, parallel arc occurrences and explicit marking
values remain independent of those identity checks.

This API represents metadata supplied by its caller. It does not populate
names from a HeuristicsNet or establish a general public converter naming
rule. Projection through reduction and automatic metadata population remain
open, alongside native carrier routing and full Petri acceptance. PLUSPLUS
stays partial with all 228 open coverage units retained.

Three registered conversion-suite tests exercise the supplied full AND and
self-loop metadata fixtures, respectively six places/four transitions/ten
arcs and three places/two transitions/four arcs. Additional cases cover
escaped and UTF-8 names, equal names and IDs across node kinds, a named silent
transition, repeated arcs, non-unit integer weights, extreme integer values,
nested properties, integral doubles, negative zero and explicit sparse zero
entries. Eighteen invalid mutations exercise identity and non-finite-property
rejection. These tests establish carrier behavior for supplied values; they
do not infer automatic converter names.

Core, request parameters, heuristic variants, Petri conversion and Petri
reduction Release suites all pass with assertions enabled and zero failed
tests. The three independent configure, build and test records exit zero with
finalized logs. All 650 compiled input bindings and seven actual binary
bindings remain unchanged. Canonical and exported source trees both equal
`9a22a8a70ec9bd72da4a8023880342ad4a1f8e03`. Build-manifest SHA is
`e372b73eaa3dd0413dfd2b343a72a488464223b04a260206086dd2bdf85d2aba`;
core SHA is
`5b3f8c02c42aef24ba868fcf6d42f7b362d637f974b479ee640b5d5feea83558`.

`docs/petri-presentation-build-checks.jsonl.gz` and its validation preserve
95 exact UTF-8 members and 1,345 immutable external bindings, including the
650 compiled inputs and seven actual binaries. Source proof and generation
origins, final 39 harness checks, historical 35 checks, nine provenance-repair
checks, fourteen original saver checks, eleven original staged-proof checks
and seven suite-order repair checks retain distinct byte origins. The mocked
collector repair matches the exact suite order of the completed build. The
reviewed pre-release harness and ROOT's single-flag release proof remain
separate from the released execution helper.

All member bytes, SHA values and counts roundtrip with the reviewed codec.
The archive is 2,739,718 bytes, SHA
`483d9bd0d1342cfedf67c1df68f8e284db422847368d864d8f362dca09daaf6a`;
validation SHA is
`e39bd64ae10ce02573b349cad91101d632cdf2674f3f3d7e3ce00a0569d5d855`.
Saving adds zero build/test commands or public/native calls. Durable checks
retain the actual command snapshots and immutable saved files after commit.

## Automatic PLUSPLUS Petri presentation (2026-10-02)

`minera::to_petri_net_presentation(const HeuristicsNet&)` produces Petri names,
nullable transition labels, separate place/transition identities, properties,
unit arc occurrences, and sparse boundary markings for PLUSPLUS inputs. It
rejects other variants and invalid active indices. The existing generic
converter remains unchanged.

Incoming join hubs are created for all activities before outgoing split hubs.
Silent names count transitions removed by reduction; this ordering resolves
the retained `hid_7`/`hid_8` bundle-collision control. Metadata is projected
through the reduction maps. Parking-place names and metadata-survivor choices
outside the saved controls still need public evidence.

Actual replay uses 77 original broad-corpus inputs and thirteen focused
converter inputs. All 69 previously matching broad topologies now pass the
complete-field comparator; the six earlier carrier/error outliers remain
explicit. A separate 77-input replay against the parent production build
confirms unchanged discoveries, generic conversion, and inputs. Seventeen
new regression tests are registered: fifteen literal public graph fixtures,
variant rejection, and active-index rejection. Five Release suites pass with
`NDEBUG` explicitly undefined.

The compiled production inputs stayed byte-identical when the final collision
test fixtures were repaired. That test unit was recompiled against the captured
production library, and all five suites were rerun. The durable archive retains
returned objects, public aliases, witnesses, source bindings, build and probe
logs, the prior failed fixture run, and the final passing run. It is
`heuristic-plusplus-petri-producer-checks.jsonl.gz` (SHA-256
`f4b335d4e0ffdb99636ea228d477a2c5f406bd6d4248a47471f94b6a505f0f55`). Its validation file records the unchanged
comparison bounds and remaining limits. This checkpoint makes no catalogue
promotion: PLUSPLUS and the full target remain partial, with 228 open units.

## Pandas entry points and carrier dispatch (2026-10-02)

Thirty-three actual public calls distinguish CLASSIC/PLUSPLUS, parent/direct,
datetime/object DataFrames, converted EventLogs, and the exported PLUSPLUS
`apply_pandas`/`apply_heu_pandas` functions. All nine supported options are
explicit. Dependency threshold stays 0.5; AND threshold is 0, 0.5, or 1.
Both workers finish successfully with zero remaining child processes. There
are 29 successful returned objects, four actual AttributeErrors, no serializer
errors, and no skipped calls. No native function is invoked in this checkpoint.

For the overlapping datetime fixture, PLUSPLUS parent DataFrame Petri output
matches `apply_pandas` on complete presented fields at all three thresholds.
Their complete input-after snapshots also match, including the added float64
`@@diff` duration column. Direct PLUSPLUS DataFrame Petri output instead matches
the parent/direct EventLog result at AND 0.5. Parent/direct PLUSPLUS HeuristicsNet
results match all seventeen raw fields for both carriers at that threshold.

The pandas HeuristicsNet differs from generic `apply_heu` in three raw fields:
`concurrent_activities`, `dfg`, and `nodes`. Concurrency for B/C is 2 versus 4;
DFG counts are equal but dictionary insertion order differs. At threshold zero,
pandas reports output/input AND scores 0.5/0.25. At threshold 0.5 both maps are
empty; the equal output score is excluded. These are finite observations,
not a rule to halve every native concurrency count.

Object-dtype parent PLUSPLUS Petri and both PLUSPLUS pandas helpers raise
`AttributeError: Can only use .dt accessor with datetimelike values` without
input mutation. Direct PLUSPLUS accepts the same object-dtype DataFrame and
returns the datetime direct-call graph. CLASSIC parent DataFrame Petri accepts
object timestamps, whereas direct CLASSIC Petri raises the same error. These
route-specific obligations must remain in the implementation and completion
audit; CLASSIC observations are not evidence of native route completion.

Saved evidence is `heuristic-pandas-dispatch-checks.jsonl.gz` (SHA-256
`bbc8cbe45fa4765c269a159ef8f04e374370dacdc9738b519fdd2ab0c8184f08`) and `heuristic-pandas-dispatch-validation.json`
(SHA-256 `c1cf86c09f49a16a590155a8bdb5dfca1802c043b186fcf9de3b5b5f276fb65a`). Eight complete public Petri comparisons
match, and two expected route/variant comparisons mismatch. Hash-bound opaque
module proofs, public signatures/enums, inputs, results, errors, mutations,
process records, logs, collectors, and the comparator are retained. The archive
round-trip is verified. Internal dispatch was not inspected; the observations
support entry-point equivalence only for these controls. PLUSPLUS and the full
target stay partial, with 228 open units and no catalogue promotion.

## Native PLUSPLUS pandas entry point (2026-10-02)

`discover_plus_plus_pandas(EventTable&, options)` now implements the separately
captured PLUSPLUS pandas route. It selects PLUSPLUS, honors the conventional
start column when the selector is omitted, copies completion timestamps into a
missing selected start column, and appends or overwrites float64 `@@diff`
durations. Declared timestamp dtypes determine pandas eligibility. The observed
object-dtype input raises `HeuristicPandasError` with the public AttributeError
category and exact message, preserving its input. Generic const-table discovery
continues through its existing route.

Fourteen new public calls resolve concurrency orientation and start-column
controls. Uniform B-outer and C-outer traces both return count 4. Balanced mixed
orientations return 2; mixtures of 3/1 and 1/3 return 1 and 3. The native pandas
abstraction keeps directional counts and assigns them to canonical pairs in
sorted order; it does not sum opposite directions. Generic discovery retains
count 4 on all five controls. At AND threshold zero, the mixed pandas split and
join scores are 0.5 and 0.25; equality at 0.5 excludes the split.

A fresh native build freezes all 650 source inputs and passes the five core,
request-parameter, heuristic-variant, Petri-conversion, and Petri-reduction suites
with assertions enabled. Actual replay makes 111 discoveries, 12 presented-Petri
conversions and 43 generic conversions. Nineteen successful HeuristicsNet aliases
match all seventeen supported field projections, with 995 matched net/node/edge
cells (323/456/216). Twelve Petri aliases match the complete supported names,
labels, properties, arc occurrences, weights and sparse markings. Three actual
public dtype errors match category and message. The 34 route aliases match
supported input-after projections, mutation occurrence and option preservation.
All 77 generic regression cases preserve their previously captured native net,
Petri, input and error results exactly.

The saved evidence retains 47 public calls, including the 33 preceding dispatch
calls and the 14 new controls; no reference calls were made during native replay.
`heuristic-plusplus-pandas-native-checks.jsonl.gz` has SHA-256
`0ca044fc58b2084ae49e77cb38ecf63e5908c3362a865e3973c12812a45df696`, with 1,076 round-trip-verified members, all frozen sources,
collectors, raw input/output snapshots, comparisons, process records, logs and
hash bindings for nine native binaries. Validation is saved in
`heuristic-plusplus-pandas-native-validation.json`.

These finite matches do not establish full pandas or PLUSPLUS parity. Public
unscoped `omni_present` metadata has no proved mapping to native global scopes.
Independent carrier comparisons exclude native scalar allocation counters and
float decimal spelling while retaining actual typed values. Dictionary iteration
order, Python object identity, additional timestamp/null/error domains, and
broader parameter/carrier combinations remain completion obligations. The new
mutable entry point must be explicitly selected by adapters that need pandas
behavior. No catalogue unit is promoted: the full target remains partial at
46/86 packages and 199/387 modules, with 228 open units.

## Pandas timestamp boundaries and measured gaps (2026-10-02)

Twenty additional public calls use ten fresh fixtures and both PLUSPLUS pandas
entry points, with all nine supported options fixed. Eighteen return successfully
and two raise the captured AttributeError. There are no skipped calls or
serializer failures. The worker exits successfully with zero remaining children;
opaque module proofs match the pinned reference before and after execution.

Actual native replay invokes twenty discoveries and seven Petri presentations on
the source committed as `63ccbc6`. Touching endpoints, equal starts, zero-duration
intervals, an existing stale `@@diff` column and reversed rows match all supported
HeuristicsNet/node/edge projections and complete Petri presentation fields. The
microsecond and nanosecond Petri outputs also match their supported presentation
fields, while their sojourn or mutation values do not. Across nine successful
public HeuristicsNet results, there are 363 matched, two mismatching and 106
unavailable field cells; the unavailable cells belong to the two NaT controls
that native discovery rejects. Seven Petri results match; two NaT Petri results
are unavailable after those native discovery errors.

The next implementation obligations are concrete:

- A NaT start or completion cell is accepted by pandas, leaves activity B's
  occurrence count intact, and yields its nonmissing sojourn mean
  2.6666666666666665. Native discovery instead rejects the timestamp.
- A missing start column is copied from object completion timestamps before
  pandas raises its dtype error. Native currently raises before adding it.
- The microsecond fixture returns sojourn 0.000002 for B; native returns
  0.0000021457672119140625 even though its `@@diff` column is correct.
- The nanosecond fixture returns sojourn 0.0000000030000000000000004 for B;
  native returns zero and also writes zero into the duration column.

These controls preserve raw inputs, outputs, failures and mutations in
`heuristic-pandas-timestamp-boundary-checks.jsonl.gz` (SHA-256
`41fbe4d8bd85b65089eab10ac1c24352863fff7939c4b3f90dd4a231b98e58be`) and `heuristic-pandas-timestamp-boundary-validation.json`.
All 100 archive members round-trip. The retained source validation binds the
previous 650-source archive and tested library; the new probe binary and its
actual process/log records are hash-bound. Explicit native input adapters map
captured pandas NaT and float NaN into their existing native typed values. Native
exception presence and message are established for the NaT failures; the probe's
fallback category field was emitted as a boolean, so it does not establish their
concrete exception category. The mapped dtype-error category is captured correctly.

No implementation change or completion claim follows from this contrast-only
checkpoint. The four gaps remain open, along with broader pandas/PLUSPLUS and
full-target obligations. Coverage remains 46/86 packages, 199/387 modules, and
228 open units.

## Native pandas NaT, duration precision and performance waits (2026-10-02)

The mutable PLUSPLUS pandas route now closes the four measured timestamp gaps
from the preceding checkpoint. It accepts partial NaT timestamps, retains every
row in activity occurrence counts, and excludes missing durations from sojourn
means. An activity with no valid duration retains a NaN mean. Interval sorting
places missing starts last; eventually-follows and inclusive concurrency require
the relevant known timestamps. Performance adjacency uses sorted nonmissing
starts, preserving rows with missing completion while omitting their wait sample.

Missing selected start columns are copied before timestamp dtype validation, so
the captured object-dtype error retains that added column. Entirely missing start
columns raise the captured TypeError after adding NaN `@@diff` values. Entirely
missing timezone-aware completion columns raise the captured timezone TypeError
before duration mutation. The captured category, message and supported input-after
projection match for all 23 error aliases in the combined replay.

Declared datetime resolution drives exact integer timestamp differences. Array
durations scale the integer unit value; scalar performance waits combine whole
seconds, microseconds and remaining nanoseconds. Eight additional public wait
controls distinguish these rounding paths, including 3/7 ns and waits just below
one/five seconds. The nanosecond duration is `3.0000000000000004e-9`, while its
captured B-to-C wait is `0.999999996`. Activity ordering follows descending
occurrence counts for the captured unequal-count control, retaining first-seen
ties. Its repeated B intervals preserve raw self-concurrency `(B, B): 4`.

Forty-four new completed public calls capture performance/NaT, entire missing
datetime columns and wait scaling: 26 return objects and 18 return actual errors.
Every completed row, raw snapshot, public signature/enum view, opaque pinned
module proof and finalized process log is retained. The initial own fixture
construction failed after sixteen begin markers; no complete returned-object
file survived, so neither completed nor successful call counts are inferred.
Its original collector and terminal failure evidence remain separate.

A fresh 650-input native build passes all five relevant suites with assertions
enabled in Release. Replay executes 175 discoveries, 30 Petri presentations and
43 generic conversions with zero new reference calls. All 98 public route aliases
match supported input mutations, option preservation and errors. Forty-five
HeuristicsNet results compare all seventeen net projections and their supported
node/edge projections, with 2301 matched cells, six mismatches and nine cells
unavailable because native has an extra edge. Forty-two HeuristicsNet results
match every supported field and connection domain; thirty Petri results match all supported
names, labels, properties, arc occurrences, weights and sparse markings. The 77
generic cases preserve their previously captured native results exactly.

All ninety preceding route aliases match in full. Three new nanosecond-gap
controls remain open: at 1, 3 and 7 ns, public A/B concurrency is 4 and dependency
is 0.5, excluding the edge at threshold equality. Native exact timestamp
concurrency is empty, producing dependency 1.0 and an extra A/B edge. All eight
performance wait values themselves match. These results establish a concurrency
comparison precision gap; they do not establish a general timestamp rounding rule.

The first build's failed nanosecond performance assertion and its diagnostic
replay are retained as superseded evidence. That replay found four mismatching
field cells (three wait-related projections and activity ordering), while the
generic results were unchanged. The successful build and replay retain their
own source, binary hashes, preparations, collectors, comparisons and process logs.
`heuristic-plusplus-pandas-timestamps-checks.jsonl.gz` has SHA-256
`e9fa4fb240c629891540119107a2812de734b954da54884b77e171cfd0235feb`, 24089147 bytes and 1903 round-trip-verified members.
Validation is `heuristic-plusplus-pandas-timestamps-validation.json`.

These finite matches do not close broader datetime units/ranges/timezones,
business-hour options, carrier/parameter/error combinations, raw dictionary
iteration order, Python object identity, unscoped metadata or adapter routing.
The independent input comparison still excludes native allocation counters and
decimal float spelling while retaining actual typed values. PLUSPLUS and Alpha+
remain partial. No catalogue unit is promoted: coverage stays 46/86 packages and
199/387 modules, with all 228 open units retained.

## Pandas concurrency timestamp rounding (2026-10-02)

Resolved the three measured nanosecond concurrency gaps from `cc8bd1a`. The pandas
abstraction now caches epoch seconds rounded to six decimal places for inclusive
overlap comparisons. It rounds the actual binary epoch value through fixed
decimal formatting, preserving the observed epoch-dependent boundary. Exact
timestamp ordering, eventually-follows comparisons, durations and scalar waits
retain their separate existing precision paths. Generic discovery is unchanged.

Forty-nine new public calls pin 24 gap/fraction/epoch controls, 19 half-microsecond
boundary controls and three paired HeuristicsNet/Petri controls. All calls return
objects with no skips or serializer failures. Every completed row, full input
mutation snapshot, public signature/enum view, unchanged opaque pinned module
proof and terminal process record is retained. The observed epochs are 1960,
1970, 2024 and 2100. At the 2024 base, 596 ns remains concurrent while 597 ns does
not; at the Unix epoch, 499 ns remains concurrent while 500 ns does not. Fractional
completion controls distinguish the behavior from simply dropping nanoseconds.

The three earlier 1/3/7 ns controls now retain concurrency count 4 and dependency
0.5, excluding the A/B edge at threshold equality while keeping each exact wait.
A new registered native regression exercises nine epoch, fractional and boundary
controls, checking concurrency counts, dependency values, accepted edges and
performance waits together. A fresh build freezes all 650 inputs and passes the
five relevant suites with assertions enabled in Release.

Actual replay makes 224 discoveries, 33 presented-Petri conversions and 43 generic
conversions without additional reference calls. All 147 saved route aliases match
error categories/messages, mutation occurrence, option preservation and supported
connection domains. All 98 preceding aliases now match in full. The 91 returned
HeuristicsNets compare all seventeen net projections and their node/edge
projections, with 4514 matched field cells and three sojourn mismatches.
Eighty-eight HeuristicsNet aliases match every supported field. All 33 Petri
aliases match supported names, nullable labels,
properties, arc occurrences, weights and sparse markings. All 77 generic cases
preserve their previously recorded net, Petri, input and error results exactly.

Three new fractional-completion controls expose a separate array-duration gap.
For A's 1.000001-second duration in nanosecond columns, public `@@diff` and sojourn
are `1.000001`; native writes and returns `1.0000010000000001`. Thus three supported
input-after projections also mismatch. Their concurrency, dependency, accepted
edges and scalar waits match. These raw differences remain explicit completion
obligations; they do not justify changing the precision of exact waits.

`heuristic-plusplus-pandas-concurrency-checks.jsonl.gz` retains 1587 members
(21245608 bytes), SHA-256 `8a9cde0e3f092b0bcc759e3dc32652a6a924d1fec681b0182a5eba0fab4625c8`. The archive round-trip verifies every
member's bytes and hash. Validation is `heuristic-plusplus-pandas-concurrency-validation.json`
(SHA-256 `9a8564df9a2ccc0ef1a5e12683d2f7a519ad4edec52016c56aab82fd8d59264a`). The full raw values, helper derivations, source
inputs, binary bindings, comparisons, preparations and execution logs are saved.
The preceding committed archive is hash-bound as the parent checkpoint and keeps
its earlier failures and three measured gaps intact.

These controls establish the captured precision behavior; broader timestamp
ranges/units/timezones, rounding environments, business hours, carrier/parameter
and error combinations, dictionary iteration order, Python object identity,
unscoped metadata and adapter routing still require completion evidence. The
supported input comparison retains typed values while excluding native allocation
counters and decimal float spelling. PLUSPLUS and Alpha+ remain partial. No
catalogue units are promoted: coverage remains 46/86 packages and 199/387 modules,
with 228 incomplete units.

## Signed pandas duration components (2026-10-02)

Resolved the three measured duration and mutation gaps from `c1a6748`. Pandas
duration and performance calculations now combine floored whole seconds,
microseconds and remaining nanoseconds in that order. Declared datetime units
still determine the exact integer difference. Millisecond remainders are
converted to microseconds before scaling. Signed durations retain their observed
floating cancellation; performance waits still clamp nonpositive differences.
Sorting, eventually-follows and concurrency retain their separate timestamp paths.

Forty-one new public calls capture 26 unit/array contrasts, eleven signed/zero/day
controls and four millisecond scaling contrasts. All calls return objects without
skips or serialization errors. Two calls retain complete Petri presentations.
The 1.000001-second duration in nanosecond columns now writes and returns exactly
`1.000001`. Adding an independent 3 ns duration leaves that value unchanged.
The 7 ms control returns hexadecimal float `0x1.cac083126e978p-8`, distinguishing
microsecond scaling from direct millisecond scaling. The -1 ns control returns
`-0x1.12e0bf070a000p-30`, retaining the observed signed component calculation.

A registered regression checks ten captured values across positive, negative,
zero and day-spanning durations in nanosecond, microsecond and millisecond
columns. It checks both sojourn projections and every affected `@@diff` cell.
A fresh 650-input build passes all five relevant suites with assertions enabled
in Release. Actual replay makes 265 discoveries, 35 presented-Petri conversions
and 43 generic conversions without new reference calls during replay.

All 188 saved route aliases match supported input-after projections, mutation
occurrence, option preservation and error category/message. All 130 successful
HeuristicsNet aliases match 6464 supported net/node/edge field cells across
all seventeen net projections, including complete connection domains. All 35
Petri aliases match names, nullable labels, properties, arc occurrences, weights
and sparse markings. The 77 generic cases preserve their previously recorded
native net, Petri, input and error results exactly. All three preceding one-ULP
sojourn/input mismatches are resolved in the same complete replay.

`heuristic-plusplus-pandas-duration-checks.jsonl.gz` retains 1782 members
(26750742 bytes), SHA-256 `388c8cb1b8e542b446cb0673c726e4362f88725e7094b709729536c11f672dc8`. Every member byte and hash round-trips.
Validation is `heuristic-plusplus-pandas-duration-validation.json` (SHA-256
`af419a1b1a1199171378359ce1e42278890a4e8e247d0c19bb2888ee8526e7be`). Raw public/native inputs, returns, mutations and errors,
opaque pinned proofs, public API views, helpers and derivations, exact sources,
binary hashes, preparations, comparisons and terminal process logs are retained.
The preceding committed archive remains hash-bound as parent evidence, preserving
its three measured gaps.

These finite controls do not establish broader timestamp ranges/units/timezones,
rounding environments, arbitrary duration aggregation, business hours, other
carrier/parameter/error combinations, dictionary order, Python object identity,
unscoped metadata or adapter routing. The supported input projection still keeps
typed values while excluding native allocation counters and decimal spelling.
PLUSPLUS and Alpha+ remain partial. No catalogue units are promoted: coverage
remains 46/86 packages and 199/387 modules, with 228 incomplete units.

## Pandas filter and threshold controls (2026-10-02)

Executed 66 additional public calls across 26 HeuristicsNet control rows, each
using frequency and performance decoration, plus fourteen selected Petri calls.
The controls vary the five exported count/threshold/decoration parameters while
fixing the four selectors. They cover activity-count cutoffs 0/1/4/5/8/9,
path-count cutoffs 0/1/2/3/4/5/8/9, dependency values immediately below/equal/above
0.5 and at 1, and AND values immediately below/equal/above 0.25 and 0.5.
Fixtures include mixed interval orientation, repeated B activities and touching
B/C intervals. Selected Petri controls retain empty and partially filtered graphs.

All 66 public calls return successfully without skips or serialization errors.
Actual native replay matches every supported field and input-after projection:
52 HeuristicsNet results match 2,246 cells across all seventeen net projections
and their node/edge projections, including complete connection domains; fourteen
Petri results match names, nullable labels, properties, arc occurrences, weights
and sparse markings. Mutation occurrence and option preservation match. Replay
makes 66 discoveries and fourteen presented conversions without new public calls.

Production source is unchanged at `e4693ec`. The verified core, its 650 frozen
inputs and five passing assertion-enabled suites are reused. Only the new replay
probe is compiled. The 77 generic regression results from the preceding accepted
checkpoint are retained as parent evidence; they are not rerun or counted as new
discoveries. The build manifest's pre-implementation parent and the committed
source checkpoint remain distinct provenance fields.

Evidence is `heuristic-plusplus-pandas-filter-checks.jsonl.gz` (SHA-256
`397cd79c365240103c42785b7bb22381e1a6320813f6b5c93b9ac106438dc86d`, 7358748 bytes, 322 verified members) and
`heuristic-plusplus-pandas-filter-validation.json`. Raw calls, typed values,
mutations, public API views, opaque pinned proofs, process records, logs, helpers,
probe sources and comparisons are retained. Every member byte/hash round-trips.
All 650 compilation input hashes and the reused binaries are checked against the
previous checkpoint; that committed archive retains their exact source bytes.

These finite controls expand parameter/filter evidence. Fractional/negative and
nonfinite count parameters, selector/error breadth, dictionary order, object
identity, metadata, timestamp domains and adapter routing remain open. PLUSPLUS
and Alpha+ stay partial; no catalogue unit is promoted. Coverage remains 46/86
packages and 199/387 modules, with 228 incomplete units.

## PLUSPLUS numeric count thresholds (2026-10-02)

Executed 88 additional public pandas calls: 76 HeuristicsNet controls and twelve
Petri controls, with both frequency and performance decoration. Count cutoffs
include negative infinity, -1.5, -1, False, True, signed zero, 0.5, the adjacent
floating values below/above 4, 4.0, 4.5, 8.5, positive infinity and NaN. Dependency
and AND controls also cover negative, infinite and NaN thresholds. Every public
call succeeds, with no skipped calls or serialization errors.

The native API previously exposed unsigned integer count filters. Appended
optional PLUSPLUS numeric overrides now preserve fractional cutoffs and allow
negative/nonfinite values. Fractions compare without truncation: a cutoff just
above four rejects a count of four. NaN rejects every corresponding activity or
path; negative values retain counts. Overrides apply to node/path filtering and
the filtered counts used in dependency and AND measures. Omitted overrides keep
exact integer comparisons. The existing CLASSIC route retains its integer options.
The replay maps booleans to numeric zero/one and negative integers to their exact
double values for these controls; the original public parameter types are retained
in the raw snapshots.

All 88 new aliases match: 76 HeuristicsNet results agree on 3,440
supported net/node/edge cells and complete connection domains, and all twelve
Petri presentations agree on names, nullable labels, properties, arc occurrences,
weights and sparse markings. Supported input-after projections, mutation
occurrence and option preservation match. The wider replay matches 342 saved
aliases, including 258 HeuristicsNet results, 61 Petri presentations and 23 mapped
error controls. Its 12,150 net-field checks match.
All 77 generic regressions remain exact matches. Replay makes 419 discoveries,
61 presented conversions and 43 generic conversions, with no new reference calls.

The corrected frozen build contains 650 exact compilation inputs. All five
assertion-enabled suites pass. The initial build's test incorrectly expected four
retained paths between cutoffs two and four; the reference fixture has AB/AC at
two and BD/CD at four. Its terminal failed run and exact inputs remain archived.
Only the test expectation changes between that build and the accepted build;
the initially prepared replay was not executed or counted as a native run.

Evidence is `heuristic-plusplus-pandas-numeric-checks.jsonl.gz` (SHA-256
`da8b1d1cdf71df3e6bcd3b6762d0b9612c3094ca9e7da949746400b6317c350f`, 46779784 bytes, 3079 verified members), with
`heuristic-plusplus-pandas-numeric-validation.json`. Raw public calls, mutations,
API views, opaque pinned proofs, process records, logs, helpers, both frozen
source snapshots, binary bindings and comparisons round-trip byte for byte.
The preceding filter and compiled-source checkpoints remain hash-bound.

These controls establish the observed numeric cases and their native API mapping.
Other parameter types, selector/error breadth, dictionary order, object identity,
metadata, additional timestamp domains and adapter routing remain open. PLUSPLUS
and Alpha+ remain partial. No catalogue unit is promoted: coverage stays 46/86
packages and 199/387 modules, with 228 incomplete units.

## Pandas literal selectors and error order (2026-10-02)

Executed 96 new public calls across three selector matrices. Thirty return
successfully and 66 raise captured errors; no calls are skipped or fail during
serialization. Controls include four custom selectors, absent and present empty
column names, native-looking alias names with different values, completion/start
role overlap, case/activity role overlap, missing columns in combination, object
timestamp dtypes, absent starts and auxiliary suffix columns. Both decorations
and selected Petri routes are covered.

The committed pre-change core fails these controls: the first sixty native
replays have 59 mismatched and 312 unrepresentable net-field cells, four mismatched
Petri presentations, thirty mismatched errors, 26 mismatched input-after
projections and 24 mismatched mutation occurrences. All original raw values,
process records and comparisons remain retained as before-implementation evidence.

The pandas API now resolves all four selectors as literal column names, including
empty strings. Lookup order is case, activity, then completion. Missing activity
and completion messages preserve their observed distinct forms; the completion
form depends on whether the selected start exists. These lookup errors precede
input mutation. An absent start is then copied from completion before dtype
validation. Equal case/activity selectors raise the observed suffixed KeyError
after datetime validation and any start copy, before adding @@diff. Auxiliary
suffix columns do not avoid that error in the captured controls.

Successful discovery uses an internal five-column projection with explicit event
roles, retaining caller selectors and cells in the mutable table. Alternate and
empty activity/completion names select their actual values rather than falling
back to native event aliases. The returned native selector metadata retains the
literal names. Generic const-table and EventLog routes retain their own semantics.

All 96 new aliases match: 22 HeuristicsNet results agree on 1,118
supported net/node/edge cells and complete connection domains; all eight supported
Petri presentations and 66 error types/messages agree. Supported input-after
projections, mutation occurrence and option preservation match. The wider replay
matches all 438 saved aliases on 13,268 net-field
checks, 69 Petri presentations and 89 mapped errors. All 77 generic regressions
remain exact matches. Execution makes 515 discoveries, 69 presented conversions
and 43 generic conversions, with no new reference calls. All five assertion-enabled
suites pass for the accepted 650-input frozen build.

Evidence is `heuristic-plusplus-pandas-selector-checks.jsonl.gz` (SHA-256
`29c93c12e26dd093c0667ee811ccfbf0246bc46b4f32f627673deabbf4a146fa`, 55954295 bytes, 3052 verified members), with
`heuristic-plusplus-pandas-selector-validation.json`. Calls, mutations, public API
views, opaque pinned proofs, native process records, logs, helpers, exact source
inputs, binary bindings and comparisons round-trip byte for byte. The preceding
compiled-source checkpoint and its 650 inputs remain hash-bound.
The initial 438-case replay compilation reached its time limit before any native
discovery. Its finalized records and inputs remain archived. The accepted replay
shares 242 constructor functions, each creating fresh data;
literal constructor reconstruction is verified and every alias remains executed.

Broader parameter/label types, additional column collisions, Unicode error
representation, dictionary order, object identity, metadata, input ordering,
timestamp domains and adapter routing remain open. PLUSPLUS and Alpha+ stay
partial. Coverage stays 46/86 packages and 199/387 modules, with 228 incomplete
units and no catalogue promotion.

## Pandas duration-column collisions and subtraction errors (2026-10-03)

Executed 66 new public calls in three matrices. Controls select @@diff as start,
completion or both, copy a missing @@diff start, or retain an unselected datetime
@@diff column. Microsecond, nanosecond and millisecond timestamp units and both
decorations are covered. Additional controls distinguish all-NaT timestamp
columns, an empty naive table, date-valued object columns and copied object starts.
Sixteen calls return and fifty raise captured errors. No calls are skipped or
fail during serialization.

The committed pre-change core makes 38 actual replays of the first matrix.
All thirty error controls differ: Minera raises a generic typed-timestamp error
where the reference raises AttributeError, "'float' object has no attribute
'timestamp'". The six supported HeuristicsNet results match 318 cells and the two
Petri results match. Supported input-after, mutation occurrence and options
already match; original records and differences remain retained.

The pandas route now maps a numeric timestamp left by overwriting @@diff to the
observed AttributeError. The overwrite remains visible before this failure.
NaN duration values are missing timestamps, allowing the observed all-NaT
collision results. Selecting one column for both timestamps preserves the
successful all-NaT subtraction, and an empty selected @@diff column returns an
empty net after its float overwrite. A date-valued object start with datetime
completion raises the observed subtraction TypeError before @@diff mutation.
Object completion and both-object controls retain their AttributeError; an
absent selected start is still copied before the relevant error.

All 66 new aliases match: twelve HeuristicsNet results agree on 516 supported
net/node/edge cells and complete connection domains, four Petri presentations
match, and all fifty error types/messages agree. Supported input-after, mutation
occurrence and option preservation match. The complete replay matches 504 saved
aliases on 13784 net-field checks, 73 Petri presentations and 139 errors.
All 77 generic regression results remain exact matches. Actual native execution
makes 581 discoveries, 73 presented conversions and 43 generic conversions,
without additional reference calls. All five assertion-enabled suites pass for
the frozen build containing 650 exact source inputs.

Evidence is `heuristic-plusplus-pandas-duration-selector-checks.jsonl.gz`
(SHA-256 `f3b620c52e16b94307d17174fdc47447847ffbf84d5d21bcce46a1aebf7d0ec5`, 47582491 bytes,
3268 verified members), with
`heuristic-plusplus-pandas-duration-selector-validation.json`. Raw calls,
mutations, public API views, opaque pinned proofs, process records, logs, helpers,
probe sources, compiled inputs and binary bindings round-trip byte for byte.
The preceding selector checkpoint remains hash-bound; its committed source
checkpoint and original build-parent identity remain separate provenance fields.
The shared replay uses 262 constructor functions, each creating fresh data;
literal reconstruction is verified and all aliases are actually executed.

Broader parameter/label types, further collisions, Unicode error representation,
dictionary order, identity, metadata, input ordering, timestamp domains and
adapter routing remain open. PLUSPLUS and Alpha+ remain partial. Coverage stays
46/86 packages and 199/387 modules, with 228 incomplete units and no promotion.

## Pandas row ordering, timestamp ties and Petri counters (2026-10-03)

Executed 118 new successful public calls across two matrices, with both
decorations. The first covers all 24 permutations within four-event cases,
interleaved cases, a fixed shuffle, reversed rows/case blocks, case-specific
labels, and equal start/completion/zero-duration timestamps in both row
directions. The second varies shuffled activity names, including reserved
names, and unequal interval orientations with equal starts. Twenty-six calls
return Petri presentations; 92 return HeuristicsNets. No calls are skipped or
fail during serialization.

The committed pre-change core executes both complete matrices. The first replay
has 3,741 matching, nine mismatched and twelve unrepresentable net-field checks;
ten Petri presentations match and two differ. The second has 874 matching,
66 mismatched and 42 unrepresentable field checks, and all fourteen Petri
presentations differ. The unrepresentable cells are actual public edge values
without corresponding native edges. Supported input-after, option preservation
and mutation occurrence already match. Raw pre-change runs remain retained.

Pandas interval ordering now compares completion time when starts are equal,
preserving missing-value placement and stable complete ties. The same interval
order feeds eventual follows, performance adjacency and directional concurrency.
Reversing equal-start rows no longer drops the observed A-to-B/C relations;
unequal orientation controls distinguish their counts and concurrency values.
Boundary counts and frequency-ordered activity metadata retain their observed
row-order behavior.

Petri presentation now creates joins in the order their target domains first
appear while traversing active nodes and name-ordered outgoing relations.
Splits follow predecessor domains encountered through that join order. All
joins still precede all splits, and the counter includes transitions later
removed by reduction. A shuffled frequency registry A/D/C/B now retains the
observed hid_2 source-to-pre_C and hid_5 source-to-pre_D paths, plus their split
counter names. Renamed and reserved-label controls distinguish this traversal
from global activity-name sorting. The HeuristicsNet registry remains intact.

All 118 new aliases match: 92 HeuristicsNet results agree on 4738 supported
net/node/edge checks and complete connection domains, and all 26 Petri results
match supported names, nullable labels, properties, arc occurrences, weights
and sparse markings. Input-after, mutation and option checks match. The wider
replay matches 622 saved aliases on 18522 net-field checks, 99 Petri
presentations and 139 errors; all 77 generic regressions remain exact matches.
Actual execution makes 699 discoveries, 99 presented conversions and 43 generic
conversions without new reference calls. All five assertion-enabled suites pass
for the frozen build containing 650 exact compilation inputs.

Evidence is split into accepted, before/failed and superseded archives:

`heuristic-plusplus-pandas-row-order-checks.jsonl.gz` (SHA-256 `a2b79908ded8b7022cf2fafe9b2b330225ed36a1c6d5f01e0843a9beb19711a5`, 54920511 bytes)

`heuristic-plusplus-pandas-row-order-before-checks.jsonl.gz` (SHA-256 `26f5c8a68513555ae7dcf98af350fc2fc5db674bcc34ffdeddf30b4fd7c369bc`, 34890832 bytes)

`heuristic-plusplus-pandas-row-order-superseded-checks.jsonl.gz` (SHA-256 `29c2b063e8b04572a0e9f085fd563a1be5f494efd15788769a35cc890cf83300`, 43115013 bytes)

Their 6287 members round-trip across all three archives;
`heuristic-plusplus-pandas-row-order-validation.json` binds every segment.
Raw calls, mutations, public API views, opaque pinned proofs, process records,
logs, helpers, probe sources, compiled inputs and binary bindings round-trip
byte for byte. Both pre-change replays and the preceding duration-selector
checkpoint remain hash-bound. The replay shares 350 constructor functions,
each creating fresh data; literal reconstruction is checked and every alias
is actually executed.

The first build failed while compiling the new test fixture, which assigned
a string to the numeric/date scalar carrier. The next fixture used contiguous
case orientations instead of the captured alternating orientations and failed
its Petri assertion; the other four suites passed. Those two corrections affect
only the test, with identical production changes across the first three
snapshots. The third snapshot passed all five suites but its full replay still
had eight mismatched renamed-label Petri presentations: all 18,522 net-field
checks matched, along with 91 other Petri results and the 77 generic regressions.
It executed a separate 699 discoveries, 99 presentations and 43 generic
conversions. That completed diagnostic replay is retained independently of the
accepted replay. The final producer changes relation traversal and adds a
renamed-label regression.

Failed command records and all 650 inputs from each earlier snapshot remain
exactly reconstructible: 648 reuse identical accepted members; Petri producer
and test deltas retain their original bytes. Neither failed build's prepared
replay was executed. Unit-test native call counts were not instrumented and
are not replay counts.

These finite controls expand ordering evidence. Broader parameter/label types,
further collisions, Unicode errors, dictionary order, identity, metadata,
additional ordering/timestamp domains and adapter routing remain open. PLUSPLUS
and Alpha+ remain partial. Coverage stays 46/86 packages and 199/387 modules,
with 228 incomplete units and no catalogue promotion.

## Pandas missing case groups and case carriers (2026-10-03)

Executed 72 new public calls across two matrices, with both decorations and all
nine options explicit. They cover empty strings, None, NaN, integers, floats,
booleans, mixed numeric/string keys, categorical keys with unused/reversed
categories, partial and complete missing case columns, interleaved rows,
missing first events, new labels on missing cases, naive/aware timestamps,
second resolution and copied missing starts. Forty-two calls return
HeuristicsNets, eighteen return Petri presentations and twelve raise captured
errors. No calls are skipped or fail during serialization.

The committed pre-change core replays all 72 calls. The first matrix has
1,506 matching and 54 mismatched field checks, eight matching and two mismatched
Petri presentations, and four mismatched errors/input-after/mutation checks.
The second has 683 matching, 61 mismatched and eighteen unrepresentable field
checks; all eight Petri presentations differ. Eight error/input-after checks
and six mutation occurrences differ. The unrepresentable fields contain actual
public edge values absent from the native edge domains. Original raw runs and
comparisons remain retained.

The pandas interval projection now pools missing case scalar kinds into one
internal case without changing caller cells. Missing cases contribute interval
follows and concurrency, and all rows contribute activity counts and sojourn
means. Boundaries and performance waits exclude missing case groups. Activity
frequency ties follow the original column row order, independently of grouped
case traversal. Empty strings remain valid case values, and the existing
numeric equality rules continue to distinguish string keys from numbers while
grouping equal integer/float/bool keys.

An entirely missing case column with aware completion timestamps raises the
observed subtraction TypeError before @@diff mutation. A missing start column
is still copied before that failure. The corresponding naive controls succeed:
their boundary dictionaries and performance DFG are empty, while their interval
relations, global counts and sojourn values remain present. These controls
distinguish missing case handling from dropping rows or creating extra boundaries.

All 72 aliases match on their supported full result/error and input checks.
The 42 HeuristicsNets match 2322 net/node/edge checks and complete connection
domains; all eighteen Petri presentations match names, nullable labels,
properties, directed arc occurrences, weights and sparse markings. The complete
replay matches 694 saved aliases on 20844 net-field checks, 117 Petri
presentations and 151 errors. All 77 generic regressions remain unchanged.
The replay executes 771 discoveries, 117 presentations and 43 generic conversions
without additional reference calls. All five assertion-enabled suites pass for
the frozen build containing 650 exact compilation inputs.

Evidence is `heuristic-plusplus-pandas-case-checks.jsonl.gz` (SHA-256
`77a0f23667f5909ad764a10513782423f300736e41b2a912f7fee027c3484f03`, 70230661 bytes, 4216
verified members), with `heuristic-plusplus-pandas-case-validation.json`.
Actual public calls, mutations, API views, opaque pinned-module proofs, native
process records, logs, helpers, probe code, compilation inputs and binary bindings
round-trip byte for byte. Both pre-change replays and all three segments of the
preceding ordering checkpoint remain hash-bound. The replay shares 396 data
constructors, each creating fresh data; literal reconstruction is verified and
every alias is executed.

The initial edge probe configure failed with MSBuild FTK1011 at its long tracking
path. Its 28-case preparation and finalized command records remain retained;
it made zero native discoveries. A shorter path produced the complete pre-change
replay against the same hash-bound committed core. Its exact frozen inputs
remain separate from the working edits and the later accepted build.

These controls expand case-carrier evidence. Broader carrier/parameter/label
types, remaining collisions, Unicode errors, dictionary order, object identity,
complete metadata, further timestamp/ordering domains and adapter routing remain
open. PLUSPLUS and Alpha+ stay partial. Coverage remains 46/86 packages and
199/387 modules, with 228 incomplete units and no catalogue promotion.

## Pandas Unicode selector errors (2026-10-03)

Executed a 234-call public matrix with both decorations and all nine options
explicit. Twenty-one strings cover ASCII quotes and controls, printable Greek,
combining and supplementary characters, nonbreaking space, C1 controls, format
characters, unassigned/private/noncharacter code points, and lone surrogates.
Each string selects a missing case, activity or completion column, with and
without the start column, or overlapping case/activity selectors. The remaining
24 calls return HeuristicsNets or Petri presentations for Unicode activity labels
and literal column names, including embedded controls and lone surrogates.

The first public capture succeeds at all 234 API calls but the OWN Petri
serializer fails to encode two surrogate-label results while checking duplicate
names. Those are actual serialization failures, not reference discovery errors.
The original capture and all native baseline calls remain retained. An OWN-only
ASCII JSON byte encoder repairs that serializer, and a fresh 234-call capture
returns 24 serialized results and 210 reference errors without skips. Both
captures count as 468 actual new public calls; the archive retains 1,175 public
calls across 29 capture directories, including the preceding checkpoint's 707.

The committed baseline has 140 mismatched selector errors. The formatter now
decodes UTF-8 code points and consults the existing Unicode 14 printability table.
Nonprintable values use Python's lowercase hexadecimal x/u/U escapes at widths
two, four or eight. Printable text, combining characters and quote selection
remain literal. Nested completion-column errors retain both layers of escaping.
WTF-8 input preserves lone surrogate code points long enough to escape them;
native probe output decoding uses surrogatepass without replacement. This is
direct native API evidence and does not establish frontend JSON adapter parity.
The original Petri comparison's two string-domain rejections remain retained.
An OWN lossless string extension compares those code points, reporting two
actual mismatches while leaving the other 127 Petri comparisons unchanged.

An independent exhaustive control invokes native pandas discovery once for each
code point from zero through 0x10ffff, including surrogates, with a missing case
selector. All 1,114,112 error messages match Python 3.11.9 builtin KeyError bytes
exactly under Unicode 14.0.0. The length-prefixed outputs contain 16,860,921 bytes
and share SHA-256 `a8ef0a9a1b13cc42df5de4374e841126018be12f36230d9a9bc3742b7bdf1f41`. This control makes 1,114,112 Python
builtin calls and zero PM4Py reference calls. Both complete output streams,
the generating helper, compiled probe, process records and binary pins remain
retained. No arbitrary multi-code-point or label-grouping completion claim follows
from the scalar sweep.

The complete replay executes 928 saved aliases and 77 generic regressions:
1,005 native discoveries, 129 Petri presentations and 43 generic conversions.
All 361 expected errors, input-after comparisons, option checks and mutation
checks match. All generic regressions remain unchanged. The replay resolves 924
aliases: 436 full supported HeuristicsNet results, 127 Petri presentations and
361 errors. Its 21445 matching net/node/edge checks retain complete domains.

Four aliases remain different: two HeuristicsNets and two Petri presentations
with lone-surrogate activity labels. The public grouping of those labels differs
from native exact string grouping. The net comparison retains seventeen actual
mismatches and 24 unrepresentable edge-field occurrences where the actual public
and native connection domains differ. These are open result obligations, not
successful controls. Ordinary Unicode labels, control-containing labels and
Unicode selector columns match. The original baseline also retains its two
unserialized public Petri controls explicitly.

All five assertion-enabled suites pass on 650 exact frozen compilation inputs.
The accepted full replay and scalar sweep account for 1,115,117 native discovery
calls; including the retained 234-call baseline gives 1,115,351. Test-suite calls
are separate from these instrumented evidence counts. The shared replay uses
429 data factories, each creating fresh data, with literal reconstruction checked.

Evidence is `heuristic-plusplus-pandas-unicode-checks.jsonl.gz` (SHA-256
`c02fddee386b7b5055c6102034c56e1cc8a55e92a0dece2d024672ba4c5f872b`, 103247692 bytes,
5870 verified members), with
`heuristic-plusplus-pandas-unicode-validation.json`. Actual returns, errors,
mutations, public API metadata, opaque pinned-module proofs, helpers, probes,
process logs, frozen source inputs and binary bindings round-trip byte for byte.
The preceding missing-case checkpoint remains hash-bound.

Unicode surrogate grouping, broader label/carrier/parameter types, dictionary
ordering, metadata completeness, object identity, further timestamp and routing
domains remain open. PLUSPLUS and Alpha+ stay partial. Coverage remains 46/86
packages and 199/387 modules, with 228 incomplete units and no promotion.

## Pandas surrogate grouping (2026-10-03)

Executed 152 fresh public PLUSPLUS calls with all nine options explicit, both
decorations and HN/PN routes. The controls vary lone-surrogate labels and case
keys, printable and empty strings, numeric and missing case values, category
dtypes, timezone-aware and naive timestamps, copied starts, input ordering,
distinct case boundaries and interval durations. A further 304 separately
counted public pandas groupby calls inspect returned grouping indices after
the result snapshots. Those diagnostics leave the visible inputs unchanged and
are excluded from the 152 initial PM4Py calls. The retained three initial
pre-change native replays execute 152 discoveries and 74 presentations. Their
original categorical carrier projections omit category declarations and remain
explicitly diagnostic; the noncategorical witnesses remain complete.

For these all-string object columns, strings containing surrogate code points
share a first encountered grouping representative. Activity occurrence counts
and caller labels stay distinct. Sojourn means use raw row order; EFG, waits and
directional concurrency independently select source and target representatives
from their own relation sequences. Concurrency retains the existing canonical
pair overwrite behavior. Surrogate case boundaries pool raw input first/last
rows while interval case identities remain separate. Performance waits exclude
those case keys only for the observed pure-string grouping domain. Mixed
numeric or missing case columns retain distinct surrogate cases.

All-surrogate cases with aware completion timestamps raise the captured TypeError
before `@@diff` is added, after a required start-column copy. Naive counterparts
succeed with pooled boundaries and empty performance DFGs. Observed categorical
activity keys remain distinct and frequency ties follow category declaration
order. Categorical case keys raise the exact UnicodeEncodeError, including
unused categories, first contiguous surrogate ranges and code-point positions;
datetime dtype validation and copied-start mutation occur first. These controls
do not establish arbitrary categorical domains or frontend JSON adapter parity.

The initial snapshots retain the dtype name but omit category declarations.
An initial full replay therefore reproduces four ordering differences and
28 error/mutation differences with incomplete category inputs. That replay's
1,157 discoveries, 197 presentations and 43 generic conversions remain retained.
The OWN snapshot and input adapters now capture and project actual public
category values, dtype and ordering. Fifty fresh public calls complete all
categorical fixtures, including ten older controls. Before/after inputs, errors
and mutation flags are exactly equal to their original captures after removing
only the newly added category metadata. These file-only equality checks make
no producer calls. The complete category carriers reproduce four ordering and
28 error differences against the preceding core in a further 50 discoveries
and 20 presentations. New capture counts total 202 PM4Py calls and 384 public
pandas grouping diagnostics; original incomplete snapshots are preserved.

The complete replay now matches all 1,080 retained PLUSPLUS aliases: 492 full
supported HN results, 183 Petri presentations and 405 expected errors. All
24972 assessed net/node/edge fields and connection domains match. All
input-after, option and mutation checks match, and all 77 generic regressions
remain unchanged. This resolves the preceding checkpoint's two HN and two PN
surrogate-label differences. The OWN Petri string codec preserves surrogate
code points losslessly while retaining the existing graph, property, marking
and arc comparison rules. Actual replay counts are 1,157 discoveries,
183 presentations and 43 generic conversions, using 498 shared fresh-data
factories with exact literal reconstruction.

All five assertion-enabled suites pass on 650 frozen compilation inputs. The
first build's heuristic suite failed because an OWN regression fixture expected
alternating interval order but used a helper with consecutive outer intervals.
The corrected fixture is tested in a fresh build; the failed build's exact
650 inputs, logs and binary pins remain retained. An independent repeated
scalar sweep confirms all 1,114,112 native KeyError messages match Python 3.11.9
builtin bytes, with SHA-256 `a8ef0a9a1b13cc42df5de4374e841126018be12f36230d9a9bc3742b7bdf1f41` and zero PM4Py calls.
The accepted replay and scalar sweep account for 1,115,269 instrumented native
discoveries; including all retained replay diagnostics gives 1,116,628. Unit-suite
calls are separate from these evidence counts.

The archive retains 1,377 actual public calls across 36 capture directories,
including the preceding checkpoint's original serializer failures and dispatch
context. Five hash-bound segments preserve all 10797 members byte
for byte, each below 100 MiB, with
`heuristic-plusplus-pandas-surrogate-validation.json`:

| Evidence segment | SHA-256 | Bytes |
| --- | --- | ---: |
| `heuristic-plusplus-pandas-surrogate-checks.jsonl.gz` | `61dc40fbac61e35169e6ff2e13cd4fd88121b90fdcbfbf4c971823f9b9e56d10` | 14366579 |
| `heuristic-plusplus-pandas-surrogate-before-checks.jsonl.gz` | `f8154de3087bad3b737a95b78a2b0e56b272a742b65403e465cc3e890409b1b6` | 28298246 |
| `heuristic-plusplus-pandas-surrogate-reference-checks.jsonl.gz` | `bb75a08a134d58e18b6ce382bd1a3161c7cf401c21518fd68a7a1ba2a1c03dbe` | 15020269 |
| `heuristic-plusplus-pandas-surrogate-replay-checks.jsonl.gz` | `5ee39ffc347def03d3a51c48508cc04414897f7fa19b08c9d514cf3ed618733b` | 66793857 |
| `heuristic-plusplus-pandas-surrogate-projection-checks.jsonl.gz` | `99e9ccdbd25699f0f3298fb19e539259feff6292dba1f607f72fea333963c80f` | 66502887 |

Public API metadata, opaque pinned-module proofs, actual returns and mutations,
diagnostics, helpers, probes, process records, exact sources and binary bindings
remain retained. The preceding Unicode checkpoint remains hash-bound.

Broader carrier and parameter types, categorical and dictionary ordering,
metadata completeness, identity, timestamp and routing domains remain open.
PLUSPLUS and Alpha+ stay partial. Coverage remains 46/86 packages and 199/387
modules, with 228 incomplete units and no promotion.
