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
| `performance_dfg`, `performance_matrix` | Performance presence, relation statistics, and edge representation | Frequency public DFG is `None`; performance DFG is a list, including an empty list. A frequency matrix entry may be zero even when native wait statistics are positive. Differing waits must distinguish count, mean, and representation. |
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
| `ACTIVITY_KEY` | `activity_attribute` | Standard/custom selectors are covered by the saved twelve cases. |
| `START_TIMESTAMP_KEY` | `start_timestamp_attribute` | A selected interval start key is covered; missing-start/lifecycle mode remains open. |
| `TIMESTAMP_KEY` | `timestamp_attribute` | Custom completion key needs an explicit discriminating control. |
| `CASE_ID_KEY` | `case_id_attribute` | Custom case grouping needs a flat-carrier control. |
| `DEPENDENCY_THRESH` | `dependency_threshold` | The inputless dispatcher case covers threshold 1; direct-route below/equal/above edge acceptance remains open. |
| `AND_MEASURE_THRESH` | `and_threshold` | Positive AND values and below/equal/above acceptance remain open. |
| `MIN_ACT_COUNT` | `min_activity_count` | Counts 1/2/10 are covered; relation fields incident to a removed activity remain open. |
| `MIN_DFG_OCCURRENCES` | `min_frequency` | Below/equal/above a concrete positive relation count remains open. |
| `HEU_NET_DECORATION` | `decoration` | Both decorations are covered on the saved short traces; differing-wait and positive branching controls remain open. |

## Petri and completion gates

Add connected sequence, XOR/AND, cycle/self-loop, and filtered structures to the
existing inputless control. Retain actual names, properties, arc weights,
multiplicity, markings, and endpoint identities. A comparison based on unique
names must reject or separately handle collisions rather than silently merging
places or transitions.

Complete the module only after the full field, route, parameter, input, and
Petri gates above are reconciled against actual pinned public calls and actual
native results, with any representation choices documented. The catalogue
proof must use a distinct tested production symbol and acceptance test, and
pass the structural verifier. Finite samples do not justify dropping public
parameters, carriers, positive maps, or conversion obligations from the scope.
