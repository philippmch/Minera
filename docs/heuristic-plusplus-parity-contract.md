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
| `AND_MEASURE_THRESH` | `and_threshold` | Positive scores 1.0, 0.8 and 0.5 match the finite native map projections, including distinct input/output sides. Threshold equality, positive-map label collisions and complete AND Petri acceptance remain open. |
| `MIN_ACT_COUNT` | `min_activity_count` | Public controls remove incident relation maps and edges while preserving raw sojourn entries; the complete native field domains remain to be compared. |
| `MIN_DFG_OCCURRENCES` | `min_frequency` | Public count 2 remains at minimum 2 and is removed at minimum 3, including the returned DFG and matrices. Compare native domains. |
| `HEU_NET_DECORATION` | `decoration` | Public differing-wait controls distinguish frequency `None`/integer values from performance dictionary/float values; native output presence and scalar types remain open. |

## Petri and completion gates

Add connected sequence, XOR/AND, cycle/self-loop, and filtered structures to the
existing inputless control. Retain actual names, properties, arc weights,
multiplicity, markings, and endpoint identities. A comparison based on unique
names must reject or separately handle collisions rather than silently merging
places or transitions.

The public controls establish connected sequence, XOR, positive AND, filtered
structures, and a reciprocal ABA cycle below dependency threshold zero.
Reserved-name controls preserve place and transition identities separately.
The 69 matching projected topologies do not complete the name/property/weight
obligations. Self-loop conversion remains open. Identical overlapping
EventLog and datetime-typed DataFrame controls return equal HeuristicsNet
fields but different Petri shapes; their cause is not established by these
observations.

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
