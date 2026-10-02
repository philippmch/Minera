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

## Returned-object acceptance

Presence, null, empty values, numeric types, and actual keys must be retained.
Derived matrix views may share backing data, but their complete values and key
domains still require comparison. Unavailable native representation must be
reported explicitly rather than filled with a presumed default.

| Public fields | Native data to compare | Remaining acceptance |
| --- | --- | --- |
| `activities`, `activities_occurrences`, `start_activities`, `end_activities` | Net metadata and boundary counts | Compare complete key domains through filtering, including an edge incident to a removed activity; retain `[{}]` boundary containers for empty results. |
| `dfg`, `dfg_matrix` | Direct-adjacency relation statistics and any stored DFG maps | Longer interval traces must distinguish direct adjacency from eventually-follows. Current PLUSPLUS raw/cleaned DFG maps are empty in the saved cases. |
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
| `START_TIMESTAMP_KEY` | `start_timestamp_attribute` | A custom interval start key is observed on both public routes; missing-start/lifecycle mode remains open. |
| `TIMESTAMP_KEY` | `timestamp_attribute` | Custom completion is observed on public EventLog and flat routes; combine it with performance decoration and native controls. |
| `CASE_ID_KEY` | `case_id_attribute` | Custom grouping is observed on public flat carriers; correctly datetime-typed DataFrame Petri and native counterparts remain open. |
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

The new public controls establish connected sequence, XOR, positive AND, and
filtered structures. Their observed names are unique within each returned
net. The default repeated-trace control excludes zero-dependency cyclic edges,
so it does not establish cyclic Petri behavior. Actual cycles, collision-focused
names, and native comparisons of the broader Petri shapes remain required.

Complete the module only after the full field, route, parameter, input, and
Petri gates above are reconciled against actual pinned public calls and actual
native results, with any representation choices documented. The catalogue
proof must use a distinct tested production symbol and acceptance test, and
pass the structural verifier. Finite samples do not justify dropping public
parameters, carriers, positive maps, or conversion obligations from the scope.
