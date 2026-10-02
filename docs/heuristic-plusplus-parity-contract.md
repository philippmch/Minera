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
frequency counts. A corrected projection requires a separate actual report.

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

## Returned-object acceptance

Presence, null, empty values, numeric types, and actual keys must be retained.
Derived matrix views may share backing data, but their complete values and key
domains still require comparison. Unavailable native representation must be
reported explicitly rather than filled with a presumed default.

| Public fields | Native data to compare | Remaining acceptance |
| --- | --- | --- |
| `activities`, `activities_occurrences`, `start_activities`, `end_activities` | Net metadata and boundary counts | Compare complete key domains through filtering, including an edge incident to a removed activity; retain `[{}]` boundary containers for empty results. |
| `dfg`, `dfg_matrix` | Retained `eventually_follows_frequency` relation statistics | Actual overlapping and non-neighbor interval results disprove literal-adjacency projection in 29 aliases. Compare the corrected complete map and nested matrix domains; current PLUSPLUS raw/cleaned DFG maps are empty in the saved cases. |
| `concurrent_activities`, `dependency_matrix` | Concurrency and dependency statistics | Compare positive concurrency and matrix entries whose dependencies fail the edge threshold. |
| `dfg_window_2`, `freq_triples` | Auxiliary maps and their presence state | Saved public values are `None`, while native maps are empty; the current PLUSPLUS representation does not encode that distinction. Longer and repeated traces remain required. |
| `dfg_window_2_matrix`, `freq_triples_matrix` | Views of the auxiliary maps | Empty dictionaries match the saved cases; the full domain and any positive values remain to be established. |
| `node_type` | Decoration | Compare frequency and performance results, including edge value types. |
| `performance_dfg`, `performance_matrix` | Performance presence, relation statistics, and edge representation | Frequency public DFG is `None`; performance DFG is a dictionary, including an empty dictionary. A frequency matrix entry may be zero even when native wait statistics are positive. Differing waits must distinguish count, mean, and representation. |
| `sojourn_times` | Optional name-to-double map | Twelve saved comparisons pass, including escaped labels, removed activities, and empty input. Retain this verified obligation when broader inputs are added. |
| `nodes` | Retained activities, node statistics, maps, and connections | Compare actual node keys and all fields below; empty AND/loop maps alone do not prove positive-map behavior. |

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
| `AND_MEASURE_THRESH` | `and_threshold` | Positive public measures 1 and 0.5 and strict threshold acceptance are observed, including a connected AND Petri shape. Compare native maps and conversion. |
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
Native comparisons of these broader shapes, self-loop conversion, and the
complete name/property/weight obligations remain open. Identical overlapping
EventLog and datetime-typed DataFrame controls return equal HeuristicsNet
fields but different Petri shapes; their cause is not established by these
observations.

Complete the module only after the full field, route, parameter, input, and
Petri gates above are reconciled against actual pinned public calls and actual
native results, with any representation choices documented. The catalogue
proof must use a distinct tested production symbol and acceptance test, and
pass the structural verifier. Finite samples do not justify dropping public
parameters, carriers, positive maps, or conversion obligations from the scope.
