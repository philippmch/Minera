# Reference 2.7.22 parity contract

Minera targets behavioral feature parity with the **2.7.22 reference release**, tag commit
`0715c31f3f1d85924107d6aa5329806db69df899`. The target is pinned so “all
variations” is finite, reviewable, and testable. Moving the target requires a
separate catalogue diff and must not silently reopen completed rows.

The upstream open-source implementation is AGPL-3.0. Minera therefore uses public
documentation, published papers, independently written implementations, and
canonicalized black-box fixtures. Upstream source may be inventoried for names and
dispatch structure, but code is not copied into Minera.

Run the reproducible catalogue audit against an exact checkout:

```powershell
node scripts/audit-reference-catalog.mjs C:\path\to\reference-2.7.22
```

At the pinned tag the source catalogue contains 18 algorithm domains, 86
`algorithm.py` packages, 182 implementation files under `variants` trees, 779
Python files under `algo`, 348 under `objects`, 156 under `statistics`,
and 142 under `visualization`.

## What parity means

Status values are:

- **Complete**: equivalent supported inputs, output invariants, important
  parameters, diagnostics, edge cases, deterministic fixtures, and a public
  Minera entry point.
- **Partial**: a useful equivalent exists, but at least one named behavioral
  variant, parameter family, diagnostic, or model conversion is absent.
- **Missing**: no public behavioral equivalent exists.
- **Adapter**: The reference implementation has separate EventLog/pandas/polars implementations of the
  same semantics. One native C++ implementation is sufficient; these do not
  count as separate process-mining algorithms.
- **Integration**: backend, renderer, connector, or optional ML/LLM surface.
  It is tracked, but not allowed to inflate the core algorithm count.

Python symbol-for-symbol API compatibility is not a goal. Semantic parity is:
the same process-mining question, equivalent supported data, canonicalized
model/output behavior, the same quality metric or diagnostic meaning, and
documented differences where multiple mathematically valid outputs exist.

Adjacent path filtering is tracked as an integration surface: predecessor and
successor adjacency, optional inclusive elapsed-time bounds, and positive or
negative polarity are implemented natively. Strict request parsing, CLI flags,
local-server fields, and compact frontend controls/serialization apply the
operation after ordinary log filters and segment extraction and before LTL.

## Algorithm-family checklist

This is the human-owned acceptance matrix. The audit script is the
machine-owned list of exact modules; a release may not claim parity unless both
are reconciled.

| Domain | Reference families and behavioral variants | Minera status |
|---|---|---|
| Analysis | marking equation (classic), extended marking equation (classic), workflow-net check, Woflan soundness with reachability/coverability/invariants | **Partial** — native marking/extended-equation/workflow/soundness cores now include deterministic structural pair diagnostics, explicit Woflan selection, short-circuit construction, reachability liveness projection, strong-connectivity diagnostics, single-place marking validation, early unsound-witness return, and strict CLI/server/frontend adapters; full differential edge-contract review remains |
| Anonymization | PRIPEL; trace-variant query Laplace and SaCoFa | **Partial** — bounded PRIPEL-style release plus both trace-variant query variants are exposed through native, JSON, CLI, local-server, and frontend adapters; deterministic seeded mode and opt-in OS-backed secure randomness are available, while differential edge-contract review remains |
| Clustering | `SKLEARN_PROFILES` profile clustering; trace-attribute-driven activity/successor/log-slice/similarity distances | **Partial** — the explicit `SKLEARN_PROFILES` selector and deterministic seeded profile clustering have bounded native/JSON/CLI/server/frontend adapters with activity/succession and explicit trace/event attribute representations; trace-attribute groups with DMM/average Levenshtein, vector, and DFG distances retain first-seen group/variant ordering, independently match DMM activity and succession components, omit missing grouping attributes, and preserve linkage child order across the same adapters; broader differential review remains |
| Comparison | Petri-net element-usage comparison | **Partial** — native deterministic place/transition/arc usage deltas are available through JSON, CLI, local server, and frontend adapters, including explicit initial/final markings and extended arcs; broader differential semantics remain |
| Concept drift | BOSE relation-type feature extraction and permutation-window drift detection | **Partial** — the explicit `BOSE` selector, seeded permutation windows, timestamp diagnostics, strict options, and JSON/CLI/server/frontend adapters are available; differential edge-contract review and alternate data-frame adapters remain |
| Conformance: Petri alignments | state-equation A*, Dijkstra no heuristics, Dijkstra less memory, discounted A*, generator Dijkstra no heuristics, generator Dijkstra less memory | **Complete (core)** — all six variants are explicitly selectable; the backward-compatible default remains the legacy unit-cost state-equation A* behavior |
| Conformance: other alignments | decomposed recomposition, DFG classic, edit distance, process-tree search graph/dynamic programming/MILP/approximated original | **Complete** — `RECOMPOS_MAXIMAL`, DFG classic, edit distance (`EDIT_DISTANCE`), and all four process-tree variants are native selectable APIs with JSON, CLI, server, and frontend adapters |
| Conformance: replay | forward token replay and backwards token replay | **Complete (core)** — forward replay remains the default; the deprecated backwards variation, reverse silent routing, weak firing, duplicate-label behavior, and immediate-stop option are selectable |
| Conformance: relational | footprints log-model/log-extensive/trace-extensive; log skeleton; Declare | **Complete (core)** — all three footprint checks, classic log-skeleton conformance, and classic Declare conformance are available with per-trace diagnostics |
| Conformance: temporal | temporal profile log/dataframe adapters, zeta threshold, business-hour slots | **Complete (core)** |
| Conformance: advanced | Petri-net anti-alignments and multi-alignments (discounted A*) | **Complete** — both pinned `VERSION_DISCOUNTED_A_STAR` algorithms, objectives, precision/distance diagnostics, marking limits, arbitrary markings, and JSON/CLI/server/frontend adapters are implemented |
| Conformance: object-centric | ETOT, OCDFG and OTG graph comparison | **Partial** — deterministic ETOT, OCDFG, and OTG graph comparisons with strict thresholds, weighted fitness, diagnostics, direct-OCEL and explicit-graph native APIs, headless OCEL CLI output, and multipart local-server/frontend API adapters; broader OCEL schema coverage remains |
| Discovery: Alpha | classic and Alpha+ | **Complete** — event-log and direct-DFG classic entry points are available; Alpha+ remains an explicit event-log variant |
| Discovery: Inductive | IM, IMf, IMd | **Complete** — `log`, explicit `imf`, graph-recursive `dfg`, and direct-DFG IMd entry points share deterministic native semantics; request/CLI/frontend selectors cover event-log paths |
| Discovery: Heuristics | classic and Heuristics Miner++ | **Complete (core)** — both variants, exact dependency/AND/loop behavior, DFG pre-cleaning, filtering, performance aggregation, interval concurrency, and classic DFG input are available |
| Discovery: optimization | ILP classic and Genetic classic | **Partial** — bounded deterministic ILP and Genetic classic implementations are available through native, JSON, CLI, local-server, and frontend adapters; ILP now supports one-trace-per-variant normalization, default silent `▶`/`■` boundaries (with explicit opt-out), cumulative alpha sequence encoding, derived one-way causal edges, optional causal overrides, and frequency-based noise filtering, while differential edge-contract review and full solver parity remain |
| Discovery: declarative | Declare classic and log skeleton classic | **Complete (core)** |
| Discovery: graph/statistical | DFG frequency/performance/frequency triples/case attributes/clean; causal alpha/heuristic; correlation classic/classic-split/trace-based; minimum self-distance | **Complete (core)** — all statistical DFG aliases, both causal variants, all three correlation-miner variants, eventually-follows, and minimum self-distance with witnesses are available |
| Discovery: footprints | log entire/trace-by-trace, DFG, Petri reachability, process-tree bottom-up, POWL bottom-up | **Complete (core)** — dataframe and lazyframe forms are adapters over the same native semantics |
| Discovery: structured | transition system view-based; POWL brute-force/dynamic-clustering/frequency/maximal/tree cuts | **Complete (core)** — transition systems support sequence/set/multiset views, forward/backward direction, arbitrary windows including zero, and optional occurrence data; POWL exposes TREE, BRUTE_FORCE, MAXIMAL, DYNAMIC_CLUSTERING, and DYNAMIC_CLUSTERING_PLAIN/frequency behavior with filtering and order-frequency settings |
| Discovery: temporal/behavioral | temporal profile; performance spectrum connected/disconnected plus log/dataframe projections; batches | **Complete (core)** — temporal profiles, all six named performance-spectrum projections, and all five batch classifications are available; columnar/lazyframe paths are adapters |
| Discovery: object-centric | ETOT, timestamp interleavings, link analysis, OCDFG, OCPN annotated/unannotated, OTG, SAW nets | **Partial** — classic ETOT, strict timestamp interleavings over related event logs, deterministic link analysis (including the configurable generated index-column contract), OCDFG metrics (explicit total-object associations plus separate event-couple and total-object performance projections), annotated OCPN per-object-type nets (IM/IMd) with opt-in per-object token-replay diagnostics, OTG discovery, and classic stochastic arc-weight nets are available through native/headless adapters; SAW weight distributions retain per-event transition multiplicity (including final routing) and merged-net arc ownership decorations; broader object-centric schema and visualization coverage remains |
| Evaluation | replay fitness token/alignment; precision token/alignment/automaton/DFG; generalization token; simplicity arc-degree/Cardoso/cyclomatic; composite weighted metrics/F-score; earth-mover distance | **Complete** — dynamic soundness-based fitness/precision dispatch, exact token and prefix-stop alignment precision, aligned-automaton precision (including task identities), escaping-transition DFG precision, transition-frequency generalization, all three simplicity metrics, normalized composite fitness/precision/generalization/simplicity weights with F-score, and exact optimal-transport distance; strict request parsing, JSON, CLI, local-server, and frontend adapters cover every setting and externally supplied comparison logs |
| Label splitting | contextual | **Complete (core)** — deterministic native contextual splitting with token-aware contexts, weighted greedy-modularity clustering, strict edge thresholds, custom classifiers/targets, JSON/CLI/local-server/frontend adapters, and focused regressions for weighted gains, zero-edge graphs, zero-gain merges, and deterministic ties |
| Merging | case relations | **Complete (core)** — the single case-relations variation merges related incoming cases with deterministic timestamp ordering, strict relation validation, and native/JSON/CLI/server/frontend entry points |
| Organizational mining | local diagnostics, network analysis frequency/performance, resource profiles, roles, SNA metrics | **Partial** — deterministic local diagnostics (group-attribute and explicit role/clustering modes), frequency/performance network analysis, role discovery, all eleven resource-profile metrics, and four SNA metrics (handover, working-together, subcontracting, joint-activities) now have native/JSON/CLI/server/frontend adapters; SNA and role discovery accept standard or custom activity/resource selectors; broader differential review remains |
| Reduction | process-tree transition-reduction based; ordinary Petri-net silent-transition reduction | **Partial** — `TREE_TR_BASED` trace reduction, optional-subtree replacement, reference-compatible folding, custom activity keys, JSON/CLI/server/frontend adapters, and deterministic single-entry/single-exit/series-transition reduction for generated process-tree, heuristic, POWL, BPMN, and mixed reset/inhibitor nets are available; conservative series-place fusion, positively marked invariant-place elimination, duplicate silent-transition fusion, inert self-loop elimination, extended-arc preservation, redundant reset cleanup, and opt-in parallel-place fusion and abstraction rules are covered, while broader adapter and differential-coverage work remains |
| Simulation | Petri basic/stochastic/extensive; process-tree basic/extensive/top-bottom; DFG classic/performance; Declare; object-centric Petri/causal nets; Monte Carlo; random tree and tree+log generation | **Partial** — Petri basic/stochastic/extensive, Declare classic, DFG classic/performance, all three native process-tree playout variants, both seeded random-tree strategies (BASIC and bounded PT-and-log generation, including deterministic multi-model batches), and a bounded virtual-time Monte Carlo Petri variant with source-log-fitted activation weights and service means are wired through strict request parsing, CLI, local server, JSON, and frontend controls; bounded OCPN playout plus native causal-net obligation search support independent/shared/synchronized projections, marker groups, cardinality, key constraints, independent activity/binding/transition branching caps, reachability, binding sequences, and per-trace object suffixing through native, CLI, multipart local-server, and frontend adapters; broader simulation/model-family coverage remains |
| Transformation | event/trace/temporal features; next-activity/next-time/remaining-time targets; interval tree; trie; embeddings; OCEL descriptions/features/splitting | **Partial** — event/trace/temporal feature tables are native and exposed through `minera_export --log-to-features` and the local `/api/transform-features` endpoint; event rows infer typed attributes with bounded categorical cardinality, use canonical `event:key@value` names, and are rectangular across traces; trace extraction now defaults to trace-based rows and exposes selectable activity/succession representations, categorical cardinality bounds, case identifiers, duration/timing/path/workload/concurrency extras, and epsilon controls through native, JSON, CLI, server, and typed frontend adapters; temporal rows now provide arrival/finish-rate, service/waiting/sojourn, rework, case/resource, and legacy compact projections with explicit feature schemas; all three target variants, open-path interval extraction, the traditional trie, contextual label splitting, deterministic case/event embeddings with cosine top-k ranking, both OCEL description variants with timestamp controls, all three bounded OCEL event/object/events-object feature tables with shared activity, timestamp, relation, endpoint, independently selectable lifecycle metrics and relation-stream lifecycle paths, deterministic object-specific relation-prefix length/timediff/one-hot activity columns, interaction, and custom attribute option groups, object-type filtering, optional per-object-type interaction counts, and opt-in interaction/descendants/inheritance/cobirth/codeath graph scalar columns for OBJECTS, and both connected-components and ancestors/descendants OCEL splits are native or headless CLI/server/frontend API-wired; numeric transformer model runtimes and broader OCEL feature parameters remain |

Process-tree simulation evidence: `BASIC_PLAYOUT`, `EXTENSIVE`, and
`TOPBOTTOM` are available through strict request parsing, the CLI
(`--playout-miner process_tree`), local-server `/api/playout`, cached-tree XES,
and frontend controls. Omitted process-tree playout settings select
`TOPBOTTOM`; `BASIC_PLAYOUT` and `EXTENSIVE` remain explicit selections. This
closes only the process-tree variants; the broader simulation row remains
partial because other simulation families are pending.

DFG playout evidence: classic best-first variant extraction and seeded
performance sampling accept activity, timestamp, and case-id attribute
selectors. Custom names are emitted as typed event/trace attributes while the
native first-class fields remain populated. Performance sampling uses distinct
internal synthetic start/end nodes with configurable labels, so real activities
that collide with either label (including equal labels) remain separate and
deterministic. The selectors and labels are available through strict request
parsing, standalone CLI flags, the local playout endpoint, and typed frontend
controls.

Extensive process-tree playout also supports an opt-in set-string result. The
native result helper returns the deduplicated activity-word set while the
legacy EventLog API remains unchanged; the export CLI, cached local server,
and typed frontend helper expose the deterministic JSON envelope
`PROCESS_TREE_PLAYOUT` with sorted activity arrays. The option is restricted
to `EXTENSIVE` and rejects malformed or non-extensive requests.

Stochastic Petri playout keeps its default XES/EventLog output unchanged and
adds an opt-in `stochastic_return_visited_elements` control. The structured
`STOCHASTIC_PLAYOUT` envelope records alternating initial/follow-up markings
and fired transition indices, including silent transitions, with explicit
initial/final markings, seeds, and step bounds respected. The marking reached
after the final allowed firing is included; a case that begins at its final
marking contains only its initial marking. Native, request, standalone CLI,
local-server, and typed frontend helpers expose the same JSON contract.

Alpha discovery defaults evidence: omitted `alpha_variant` selects classic
Alpha consistently across native options, strict request parsing, CLI, and the
frontend. The short-loop-aware Alpha+ implementation remains available through
an explicit `plus` selection.

Random-tree generation evidence: the native generator exposes both the
`BASIC` strategy and the seeded, bounded `PTANDLOGGENERATOR` strategy. The
latter validates triangular activity-count bounds, supported operator weights,
silent/duplicate probabilities, and deterministic zero/one/many model
contracts. A single model retains the legacy `tree` JSON shape; zero or many
models use a `trees` envelope with shared activities, and a single seeded RNG
stream makes the first batch member equal to the corresponding one-model run.
Strict request parsing, standalone CLI JSON, local `/api/random-tree`,
frontend query serialization, recursive response guards, and an explicit
population/activity safety cap preserves the selected variant without silently
clamping large requests.
The request default is `PTANDLOGGENERATOR`; callers that need the recursive
legacy sampler select `BASIC` explicitly.

Monte Carlo simulation evidence: the native `monte_carlo` core implements the
`PETRI_SEMAPH_FIFO` virtual-time scheduler over workflow nets, seeded transition
selection, deterministic, exponential, constant-zero, immediate, normal, uniform,
lognormal, and gamma service times, bounded case/step
budgets, shared output-place resource pools, completion/deadlock diagnostics,
and deterministic EventLog output. Source-log fitting can force normal,
uniform, or exponential service-time distributions while retaining deterministic
fallbacks for degenerate samples; explicit duration vectors take precedence.
`monte_carlo_*` request fields are strictly
validated, the export CLI can select `--playout-miner monte_carlo`, the local
server exposes both `/api/playout?mode=monte_carlo` (XES) and `/api/monte-carlo`
(JSON), and the frontend offers typed query/result guards and controls. Fitted
activation weights and timestamp-derived service durations are applied
when the source log overload is used.

Object-centric Petri-net simulation evidence: the bounded native adapter
enumerates per-object-type workflow-net traces, combines object choices into
deterministic OCEL event/object/relation records, supports explicit object ids,
per-trace object suffixing, variable object-type bindings, explicit per-type
initial and final marker vectors, and strict trace/state/firing/timestamp
bounds. The
standalone `minera_ocel --ocpn-playout` command, multipart local
`POST /api/ocel/playout`, and typed frontend request/response guard expose the
same `OCEL_PLAYOUT` envelope; the endpoint also selects the log/DFG OCPN
discovery path and validates its thresholds. `event_binding=shared_activity`
coalesces same-activity local firings into one event with all selected object
relations; the default `independent` mode preserves one event per object-local
firing. The new `synchronized` mode retains an activity-to-object-type slot map
from discovery, fires enabled transitions for one deterministic binding in a
single global event, supports seeded transition/binding branching caps
(including fractional stochastic rounding), and can return reachability or
binding sequences. Legacy independent/shared modes apply the transition cap
during each per-object language expansion and the binding cap while selecting
per-object traces, using stable seeds derived from the requested seed and
object identity. Variable slots enumerate every enabled non-empty subset of
an object type deterministically before seeded sampling. The selected mode and
optional diagnostics are echoed in the JSON envelope. The CLI and local-server
query use escaped `TYPE:T0,T1,...` marker entries and reject unknown types or
vectors whose lengths do not match the selected net. Traditional exhaustive
Petri-net playout accepts the same marker-vector contract. Object-centric
causal-net playout now has a native obligation search: conjunctive input/output
marker groups, alternative groups, per-marker cardinality ranges, marker-key
object-sharing constraints, synthetic start/end obligations, deterministic
memoized reachability search plus bounded exhaustive trace enumeration, independent seeded activity/binding/transition branching caps, existence checks, binding
sequences, event-binding projection, and per-trace object suffixing. Discovered
models derive deterministic marker groups from per-object event contexts while
retaining the workflow projection for compatibility. The separate
`OC_CAUSAL_NET_PLAYOUT` envelope, `--oc-causal-net` CLI selector, and
`model=causal_net` server/frontend selector expose the same result contract;
explicit model construction is covered by the native JSON serializer.

Traditional Petri playout now applies explicit marker vectors consistently to
the basic sampler and weighted stochastic sampler as well as exhaustive
enumeration. The local `/api/playout` endpoint accepts `initial_marking` and
`final_marking` for basic/extensive mode and the prefixed stochastic forms;
the standalone playout/export CLIs and typed frontend query use the same
comma-separated contract. Basic sampling also exposes deterministic seed and
timing controls, a firing cap, and final-marking acceptance controls: callers
can retain only cases that reach the final marking and optionally accept
markings that contain it. The native API, `--petri-playout-*` export options,
generic `/api/playout` parameters, and typed frontend query serialize these
controls with matching validation.
The stochastic Petri analysis endpoint adds a first-class tangible-state
reachability view at `/api/stochastic-petri` and the corresponding
`--stochastic-petri` export mode. Ordinary arcs are enumerated in deterministic
marking order under a positive `max_states` cap (at most 2,000 states for the
dense generator); finite non-negative
per-transition exponential rates form a CTMC generator, duplicate target
transitions are summed, and immediate-transition metadata removes states with
enabled immediate firings plus incident immediate edges. Unsupported extended
arcs, malformed rates, markings, indices, and caps are rejected explicitly.
The JSON envelope is `STOCHASTIC_PETRI`/`TANGIBLE_CTMC` with deterministic
states, transition edges, generator rows, and exhaustion diagnostics. The same
endpoint and CLI now accept `stochastic_petri_analysis=transient` or
`steady_state`: transient propagation uses a dependency-free uniformization
series with finite non-negative time and unit-mass validation, while
steady-state analysis reports a deterministic distribution for one recurrent
class and an explicit non-unique diagnostic for multiple recurrent classes.
The retained projected initial marking is used when no transient source is
provided; if projection removes it, callers must provide a probability vector.
Dense propagation has a bounded work budget and a one-million-second wire-time
limit so requests fail deterministically instead of exhausting the host.
Probability envelopes use `STOCHASTIC_PETRI_DISTRIBUTION` with deterministic
state ordering, diagnostics, and finite-or-null JSON number handling.
A reusable stochastic-map layer now fits deterministic, exponential, constant,
immediate, normal, uniform, lognormal, and gamma transition records from a
replayed event log, retaining activation weights and priority metadata. A
performance DFG can be converted to either classic workflow-net construction
with rates derived from edge means or the no-duplicate-activity construction;
silent routing is represented as immediate when its duration is zero. The
native map converts back to the existing tangible CTMC options, and
`minera_export --stochastic-map`, `/api/stochastic-map`, strict request parsing,
and typed frontend helpers share a `STOCHASTIC_MAP` envelope containing the
map, optional converted net, replay diagnostics, and reachability graph. The
CTMC projection eliminates priority-filtered silent routing with normalized
activation weights and exposes the projected initial probability vector.
Basic Petri sampling can also opt into a structured visitation response: each
trace alternates marking snapshots with fired transition indices (including
silent transitions), with transition activity labels where available. The
native serializer, CLI JSON output, generic endpoint, and typed
`fetchBasicPlayoutVisited` adapter share the `PETRI_PLAYOUT_VISITED`/
`BASIC_PLAYOUT` envelope.

Extensive Petri enumeration also has an opt-in `return_elements` control.
When `mode=extensive`, the local server, standalone export CLI, and typed
frontend helper expose an `EXTENSIVE_PLAYOUT` visited envelope with alternating
markings and transition indices, including silent firings. `max_marking_occ`
limits repeated occurrences of a marking on each candidate; omission keeps the
native unlimited behavior, while an explicit zero is rejected. Final-marking
acceptance and the visible trace-length bound remain unchanged, and ordinary
extensive requests still return XES/EventLog output.

Object-centric enrichment evidence: `enrich_ocel` deterministically materializes
the interaction, descendants, inheritance, cobirth, and codeath object graphs as
qualified O2O relations, and can qualify every E2O relation as creation, other,
or termination. Existing relations and qualifiers are preserved when a selected
graph does not add a new edge. The standalone `minera_ocel --enrich` mode,
multipart local `POST /api/ocel/enrich`, and typed frontend adapter expose the
same bounded selection and `OCEL_ENRICHMENT` envelope.

Direct object-graph evidence: `discover_ocel_object_graph` exposes each of the
five relations without synthetic O2O materialization, retaining isolated object
ids and deterministic lexical edge ordering. The `minera_ocel --graph` selector,
multipart local `POST /api/ocel/graph`, and typed frontend adapter share the
`OCEL_OBJECT_GRAPH` envelope and accept the same relation names.

OTG conformance threshold evidence: the scalar `otg_theta` remains the fallback
for every relation, while the standalone OCEL CLI, multipart local conformance
route, and typed frontend query expose strict non-negative relation overrides
for interaction, descendants, inheritance, cobirth, and codeath. Identical
relative-frequency deltas can therefore be accepted for one relation and
reported for another without changing the native `compare_otg` contract.

Object-centric graph interchange evidence: ETOT and OTG serializers now have
strict bounded JSON readers, and OCDFG serialized metrics (including event
couples, total-object tuples, performance projections, and the computed flag)
round-trip through a deterministic parser. The readers reject malformed pair or
triple shapes, negative frequencies, non-finite performance values, and invalid
graph envelopes. Direct-OCEL conformance adapters and explicit graph inputs
therefore share one validated native surface. The standalone OCEL conformance
command and local multipart conformance route now accept either an ordinary
OCEL document or the matching ETOT, OTG, or OCDFG graph JSON as the normative
input; CSV/XML/SQLite normative imports continue to use the OCEL path.

Annotated object-centric Petri-net evidence: `discover_ocpn` now retains the
deterministic OCDFG activity, start/end, and directly-followed-event metrics
alongside each per-object-type workflow net. The serialized `annotations`
section includes event/object tuples, event-pair aliases, object-type maps,
performance-disabled diagnostics, and double-arc decisions; the existing
`minera_ocel --discover ocpn`, local OCEL discovery route, and frontend result
guard continue to select the same `ocpn` variant. The opt-in
`diagnostics_with_token_based_replay` setting adds deterministic `tbr_results`
for each object type: relation-order flattened traces are replayed against the
discovered net, place counters retain compact `m`/`r`/`c`/`p` missing,
remaining, consumed, and produced token totals, and transition activation
counts include silent routing. The option is
strictly wired through the standalone CLI, local discovery route, and typed
frontend query/response guard; disabled results serialize as an empty map.

Link-analysis evidence: the classic relation join is available as
`discover_link_analysis` over event logs and supports first-class or generic
event/trace columns, timestamp-aware stable ordering, forward-only filtering,
first-occurrence reduction, deterministic transitive propagation, and a strict
link bound. Its standalone JSON, `minera_export --link-analysis`, cached local
`POST /api/link-analysis`, and frontend API/query guard all expose the same
options and canonical event references. Each endpoint retains the stable
`index` field for existing consumers and also materializes the configured
`index_column` name as an alias; names that collide with the other endpoint
fields are rejected.

OCEL split evidence: `split_ocel` implements the connected-components and
ancestors/descendants variants over the object-interaction graph. Projections
retain only selected objects, their related events and relations, matching
object-to-object links, object changes, and global values; ancestor projections
use deterministic lifecycle ordering and support an object-type selector and
object-count bound. The `minera_ocel --split` CLI, multipart local
`POST /api/ocel/split`, and typed frontend query/upload adapter share the
`OCEL_SPLIT` JSON contract.

OCEL feature evidence: `extract_ocel_features` provides deterministic numeric
tables for event rows, object rows, and exploded event-object rows. Event
features cover activity indicators, timestamps/calendar parts, independently
selectable overall and per-type related-object counts, independently selectable
lifecycle starts and ends, attribute counts, custom numeric/string attributes,
newly observed interactions, and opt-in min/max aggregation of the
selected object feature vector over each event's related objects. Object features cover type
indicators, lifecycle length/duration/endpoints, activity/path counts, custom
attributes, work-in-progress, normalized interaction-graph degree, and optional
per-object-type interaction counts. Degree centrality is independently
selectable from the interaction feature group, so OBJECTS callers can request
degree alone or interaction-by-type columns without degree. Lifecycle length,
unique-activity, duration/endpoints,
activity, path, and work-in-progress columns can be enabled independently
while the legacy lifecycle switch remains a group-level override. Optional
OBJECTS-only graph columns cover general
interaction degree, descendants and inheritance incoming/outgoing counts, and
cobirth/codeath degree. The OBJECTS table also supports opt-in related-event
min/max projections, last-related-activity event projections, neighboring-object
min/max projections, and lifecycle metric toggles within the lifecycle feature
group; these derived columns use the same bounded event/object feature schemas
and deterministic relation-stream/interaction ordering as the base tables. Exploded
event-object rows
project the same event and object feature columns pointwise, so the shared
option groups and custom attribute selectors are effective without changing
relation order or duplicate row suffixes. The OBJECTS table can be
filtered after feature computation to one requested object type, preserving
feature columns and original row order; missing types produce an empty table,
while the selector is rejected for event and exploded event-object variants.
The optional `include_all_object_features` control (and matching CLI, server,
and frontend adapters) enables the degree, per-object-type interaction, and
all five general graph scalar families together for OBJECTS only. It does not
enable lifecycle, related-event/activity, neighbor, or related-object paths;
explicit child false values suppress their family, while explicit false and an
omitted preset control retain compact defaults.
Exploded pointwise event and object projections can be disabled independently
with `include_event_pointwise_features` and
`include_object_pointwise_features` (or the matching CLI switches); an unset
control preserves both projections, and explicit controls are rejected for
EVENTS and OBJECTS. Disabled source tables are not computed, so
`max_features` applies only to the selected exploded output families.
Exploded rows also provide object-specific relation prefixes by default:
`@@ev_obj_pref_length`, `@@ev_obj_pref_timediff`, and deterministic one-hot
activity counts named `@@ev_obj_pref_act_<activity>`. Prefixes follow relation
order, retain each earlier event at most once per object, and use zero for
missing timestamps; length, timediff, and activity groups can be disabled
independently through native, server, CLI, and typed frontend options that are
strictly limited to the exploded variant.
The `include_related_objects_features` control and its
`--features-related-objects` CLI counterpart enable this aggregation; they are
strictly limited to EVENTS and remain disabled for the exploded projection,
with duplicate relations deduplicated before aggregation and empty
related-object sets represented by zero columns.
The `include_degree_centrality_features` control and its
`--features-no-degree-centrality` CLI counterpart are strictly limited to
OBJECTS; the server and typed frontend reject this control for event and
exploded variants. When omitted, native callers retain the historical behavior
where degree follows `include_interaction_features`; the default remains
enabled.
The `interaction_graph_source` selector chooses the edge source for normalized
degree, per-object-type interaction counts, and neighboring-object min/max
projections: `event_cooccurrence` uses only objects sharing an event, while
`event_and_o2o` preserves the native union with explicit O2O links. The latter
is the default when the selector is omitted. An explicitly supplied selector
is strictly limited to OBJECTS, or to the
related-object EVENTS context, and does not alter the separate general graph
scalar families, which remain event-cooccurrence based.
Strict feature-count, selector, and relation-id validation is shared by
`minera_ocel --features`, multipart local `POST /api/ocel/features`, and the
typed frontend query/response adapter.
The optional `feature_name_style=compatibility` mode (or
`--features-name-style compatibility`) maps categorical event/object columns
to `@@event_attr_value_<key>_<value>` and
`@@object_attr_value_<key>_<value>`, and maps object interaction-by-type
columns to `@@object_interaction_graph_<type>`. Native names remain the
default; compatibility names are reused consistently in related projections
and are rejected for the ambiguous exploded EVENTS_OBJECTS variant.

OCEL interchange evidence: the native core now parses and writes deterministic
extended-table CSV (with an optional object table), typed OCEL 2 relational
SQLite, and bounded classic/standard XML and JSON. Standard JSON/XML emit and
re-import deterministic object/event type catalogs, including empty types and
scalar attribute declarations without observed values. Classic JSON now
likewise emits and imports the `ocel:eventTypes` and `ocel:objectTypes`
catalogs; observed attributes fill in missing declarations with deterministic
scalar types while explicit catalog types are preserved;
typed values are coerced on import, static object attributes use the epoch
baseline, and later timed values remain object changes. CSV import validates
headers, object-list cells, timestamps, ids, and cross-type references; SQLite
round-trips the core relational tables plus typed attributes, event-to-event
links, object changes, and globals, and writes/imports standard type-specific
attribute columns plus exact declared-schema metadata and object-change rows;
XML/JSON import/export preserves
scalar attributes, qualifiers, object-to-object links, object changes,
globals, and deterministic ordering where the selected format carries them.
`minera_ocel`
detects `.csv`, `.sqlite`/`.sqlite3`/`.db`, and `.xml`/`.xmlocel` inputs and can
emit classic or standard XML or a SQLite file. The local server's multipart
`POST /api/ocel/sqlite` imports SQLite files, while the same route accepts
`mode=export` JSON and returns a bounded SQLite download. The bidirectional
`POST /api/ocel/io` route also accepts classic/standard JSON and XML, validates
the selected format, and returns bounded downloads or a canonical JSON import
envelope. The same route accepts event CSV uploads with an optional object-table
part and can download either deterministic event or object CSV. CSV exchange
now carries validated custom column names, object-type prefixes, one-character
delimiters, trimming and timestamp-sort switches, import rejection policy, and
explicit lossy-export opt-in through the CLI, local server, and typed frontend
helpers; non-CSV formats reject those options instead of silently ignoring them.

Embedding evidence: `to_embeddings` exposes both case-level and event-level
variants through deterministic bounded encoder profiles. `hashing-v1` retains
the original token profile, `hashing-v2` adds character n-grams, and other
non-empty model names select a salted token profile instead of being silently
ignored in hashing mode. Transformer mode is an explicit injectable provider:
the callback receives the selected text, model label, and requested dimension,
and its output length and finite values are validated. Transformer callers may
set `embedding_dimensions=0` (or `--embedding-dimensions 0`) as an inference
sentinel; zero is passed through to the callback/runtime, the first non-empty
finite vector establishes a bounded width (at most 4096), and every later or
target vector must match. Hashing providers continue to require a positive
explicit dimension. Applications can either
provide that callback per request or register a thread-safe process-level
default runtime bridge; the latter is used only when a request has no explicit
callback. The explicit
`mock_transformer` model supplies the deterministic headless CLI/server/frontend
integration path; other transformer models require an application callback and
never fall back to hashing. For standalone and cached-server operation, the
optional `--embedding-runtime-url http://host:port` bridge posts
`{"text":"...","model":"...","dimensions":N}` to the fixed `/embed`
path and requires a successful response shaped exactly as
`{"embedding":[finite numbers...]}`; zero-dimensional requests return the
provider's inferred width. Connection,
read, and write calls are bounded to five seconds; malformed URLs, HTTP
failures, extra fields, malformed numbers, empty vectors, wrong dimensions, and
non-finite values are rejected. The CLI enables transformer mode unless an
explicit provider is supplied; the server applies the same default only when
the request omits its provider, so explicit hashing remains unchanged. Provider,
model label, and vector dimension are
echoed by JSON and similarity responses. `minera_export --embeddings`, cached-session
`POST /api/embeddings`, and the typed frontend request/response guards share the
same provider-aware contract. Similarity rows sort by descending cosine score,
then descending identifier; exact duplicate ties retain input order. Event
filtering can retain only selected events or complete cases. Numeric model
weights and their runtime remain an optional application integration concern.

Timestamp-interleavings evidence: the native discovery joins related left/right
event logs through `CaseRelation` rows, adds deterministic extreme trace
boundaries, and emits only strict four-timestamp LR/RL crossings with source /
target activities, original event indices, timestamps, and finite elapsed
seconds. Missing or invalid timestamps, malformed relations, and result-bound
overruns are rejected during discovery. The standalone
`minera_export --timestamp-interleavings` path, multipart local
`POST /api/timestamp-interleavings`, strict request parser, frontend upload
adapter, and focused Release tests share the same canonical JSON contract.

DFG simulation evidence: `CLASSIC` performs deterministic best-first expansion
of a frequency DFG with start/end boundaries, probability ordering, occurrence
limits, graph-completeness interruption, and safety bounds. `PERFORMANCE`
samples frequency-weighted paths and exponential edge delays from the native
performance DFG using a reproducible seed and case-arrival schedule. Both
variants are exposed by `minera_playout --dfg`, `minera_export
--playout-miner dfg`, local-server `/api/playout?miner=dfg`, and frontend
controls. Ordinary requests retain XES output. Classic requests with
`return_variants=true` use a deterministic JSON envelope containing one
activity-sequence and occurrence-count entry per discovered variant, together
with completeness and interruption status; `return_only_if_complete` clears
the entries when coverage is not complete. The native log continues to carry
probability and estimated-occurrence trace attributes for XES consumers.

Petri stochastic simulation evidence: the native `stochastic_playout` API uses
transition-index weights, validates finite non-negative values and weighted
input arcs, samples all-zero enabled sets uniformly, preserves silent routing,
and stops at the exact workflow final marking unless configured otherwise.
The seeded options are available through `--playout-variant stochastic`, the
local server's `mode=stochastic` route, and the frontend controls, with strict
trace/step/event-budget limits at adapter boundaries. The supported native
contract also provides deterministic source-log fitting: reachable enabled
transitions receive activation ratios, stable duplicate-label and silent
routing replay is supported, unknown source labels and partial traces are
tolerated, and never-reached transitions retain neutral weight. Explicit
transition weights take precedence over fitted values in the source-log
overload; the native replay API is wired through strict request parsing, the
export CLI, the cached local server route, and typed frontend controls.

## Data, object, conversion, and integration checklist

| Surface | Required variations | Minera status |
|---|---|---|
| Event data | Event, trace, event stream, typed attributes, lifecycle, dataframe/lazyframe adapters | **Partial** — native EventLog with typed scalar attributes and restartable CSV/XES event/trace readers; columnar adapters remain |
| Object-centric data | OCEL events/objects/relations/o2o/e2e/object changes and OCEL 1/2 schema | **Partial** — bounded events/objects/event-object and event-to-event relations, o2o links, object changes, scalar globals, classic/standard JSON/XML, declared event/object type catalogs with empty-type schemas, and a typed OCEL 2 relational SQLite projection are present; remaining schema coverage remains |
| Models | Petri nets incl reset/inhibitor, markings, process trees, BPMN, DFG, transition systems, heuristics nets, POWL, declarative models, OCPN/OC causal nets, trie, stochastic nets | **Partial** — ordinary and extended reset/inhibitor Petri arcs now round-trip through PNML/JSON and are honored by replay, alignment search, soundness reachability, and Petri playout; reusable stochastic maps now round-trip distribution/rate metadata and derive tangible CTMC options, while object-centric, trie, and broader stochastic model-family coverage remains |
| Conversions | log/stream/table; tree to Petri/BPMN/POWL; BPMN/DFG/heuristics/trie/WF-net conversions; OCEL graph/features | **Partial** — tree/heuristics/POWL/BPMN, deterministic POWL-to-process-tree layering, strict block-structured workflow-net-to-process-tree reconstruction (with non-block rejection), ordinary workflow-net-to-POWL reconstruction for structured fragments, both classic DFG-to-Petri constructions, and trie-to-Petri conversion to workflow nets, tree to POWL, and tree/net to BPMN; remaining object-centric and adapter conversions are absent |
| Traditional I/O | XES parser variants, PNML, PTML, BPMN, DFG read/write | **Partial** — XES, PNML, PTML, deterministic classic DFG text read/write, and DFG input/output through CLI/server/frontend adapters are present; broader interchange variants remain |
| Object-centric I/O | OCEL CSV, JSON, XML, SQLite and OCEL 2 JSON/XML/SQLite | **Partial** — classic and standard JSON/XML now round-trip deterministic type catalogs, including empty types and declared attributes without observed values, typed scalar attributes, epoch-baselined object histories, event/object relations, and event-to-event links; typed OCEL 2 relational SQLite persists the exact declared schemas alongside inferred type-table columns, deterministic extended-table CSV (with an optional object table), and bounded CLI/local-server/frontend adapters for every JSON/XML/CSV/SQLite direction are present; remaining schema-specific adapters remain |
| Filtering | attributes/cases/endpoints/variants/paths/LTL/time/case performance/between/rework/prefix/suffix/segments; trace-pattern filters; DFG filters; OCEL filters | **Partial** — broad traditional-log filters, inclusive numeric, absolute-frequency, and relative-occurrence attribute ranges, deterministic top-activity selection, strict/non-strict first/last prefix and suffix cuts, positive/negative endpoint selection, per-variant minimum/maximum coverage and cumulative percentage selection, exact admitted-variant selection (including empty variants and polarity) plus exact/prefix/suffix/infix trace-pattern matching with `...` spans through native/request/CLI/server/frontend mining surfaces, native/CLI/server/frontend adjacent-path filtering with custom activity/timestamp attribute selectors and inclusive performance bounds, prefix/suffix/between segment variants, all four LTL predicates through native/CLI/server/frontend adapters, connected DFG activity/path/dependency/boundary and noise-threshold filters through native, CLI, server, and frontend export surfaces, and complete native/CLI/server/frontend OCEL filters; OCEL adapter query values now preserve nested delimiters and typed scalar overflow is rejected under a bounded recursive level; broader filter families remain |
| Statistics | endpoints/attributes/variants, calendar attribute distributions, numeric/date KDE, case statistics, passed time, service/cycle/arrival/overlap/rework, concurrent/eventually-follows, process cube, OCEL statistics, chaotic activities | **Partial** — deterministic calendar event distributions across all six time buckets, numeric/date attribute KDE grids, case timing rows with sorting/limits/business-hour durations, passed-time variants, per-activity service-time aggregation, both activity/case rework variants, interval-event/case-envelope/concurrent-activity overlap, entropy-based chaotic-activity ranking, typed numeric/categorical/composite process cubes, and OCEL object-type summaries, activity/object associations, and edge metrics with configurable business-hour durations are available through native/CLI/server/frontend adapters; broader statistical aggregation extensions remain |
| Visualization | Petri/DFG/tree/BPMN/heuristics/footprints/SNA/transition system/POWL/declarative/OCEL, decorations and render backends | **Partial** — interactive native UI includes statistical/causal/correlation DFG results, performance spectra, batch diagnostics, recursive POWL, footprint variants/diagnostics, Declare, log-skeleton, converted workflow nets and decorations; several model families and render backends remain |
| Streaming | CSV/XES event/trace streams, streaming DFG, online Declare/footprints/token/temporal conformance, classic/thread-safe/Redis stores | **Partial** — restartable CSV/XES event and trace readers, live bounded worker-pool streams, incremental DFG, a file-backed restartable streaming DFG store, deterministic classic and mutex-protected dictionaries, a bounded RESP remote dictionary, Declare, footprints, token, and temporal conformance are present; richer distributed lifecycle orchestration remains |
| Decision mining | decision-tree extraction, encoded decision tables, data Petri nets | **Partial** — decision trees/rules with bounded depth/split/leaf/candidate/gain controls and selected context attributes, deterministic encoded numeric/one-hot tables with bounded pre-choice lookback, and guarded data-Petri-net JSON/CLI/server/frontend surfaces are present; clean variants use exact replay while deviating variants now use optimal-alignment model choices with replay/alignment/failure diagnostics; richer classifier backends and columnar training integrations remain |
| Privacy | anonymization algorithms and privacy-preserving query variants | **Partial** — PRIPEL-style release and Laplace/SaCoFa trace-variant queries have native and adapter surfaces, with explicit deterministic or OS-backed secure-random modes; broader privacy contracts remain |
| ML | train/test splits, prefixes, feature tables, prediction targets, OCEL features | **Partial** — deterministic seeded train/test trace splits, fixed and all-prefix projections, until-activity targets, activity-targeted sublogs, automatic event/case feature selection, and typed process-cube tables now have native, standalone CLI, cached-log server, and typed frontend adapters; model training and columnar dataframe integrations remain |
| LLM/querying | abstractions, provider calls, clustering/hypotheses, natural-language log query/filter, database knowledge injection | **Missing / Integration** |
| Connectors | Outlook, Windows events, browsers, GitHub, Camunda, SAP O2C/accounting | **Missing / Integration** |

Overlap evidence: `OverlapOptions` validates finite non-negative epsilon,
supports configurable activity/timestamp attributes, and supports inclusive or strict endpoint semantics. When `overlap=true`, the
native interval-event, case-envelope, and concurrent-activity counters run
after all ordinary, segment, path, and LTL filters; JSON embeds the three
arrays under `overlap`. CLI flags and server/frontend form fields are covered
by `src/backend/tests/test_request_params.cpp` and the focused overlap/native
JSON tests.

Passed-time evidence: `discover_passed_time` exposes `PRE`, `POST`, and
`PREPOST` with deterministic neighbor rows, edge frequencies, weighted
averages, custom activity/timestamp attributes, scalar performance
aggregations, and optional business-hour durations. The standalone CLI,
`POST /api/passed-time`, strict request parser, and typed frontend API share
the `PASSED_TIME` envelope; focused tests cover all variants, custom keys,
aggregation validation, and malformed response values.

Service-time evidence: `discover_service_time` computes each event's
start-to-completion duration grouped by activity, with deterministic
`MEAN`, `MEDIAN`, `MIN`, `MAX`, and `SUM` aggregation plus optional
business-hour elapsed time and custom timestamp/activity keys. The standalone
CLI, `POST /api/service-time`, strict request parser, and typed frontend API
share the `SERVICE_TIME` envelope; focused tests cover custom keys,
aggregation rejection, finite JSON values, and empty/invalid inputs.

Cycle-time evidence: `discover_cycle_time` orders event intervals by their
start instant, merges overlapping production spans, and reports completed
span time per trace instance with configurable completion/start timestamp keys.
The standalone CLI, `POST /api/cycle-time`, strict request parser, and typed
frontend API share the `CYCLE_TIME` envelope; focused tests cover custom keys,
empty/invalid timestamps, interval merging, and bounded diagnostics.

Case-arrival evidence: `discover_case_arrival` computes mean gaps between
sorted case starts and sorted case ends, with optional business-hour slots and
zero-safe single-case behavior. The standalone CLI, `POST /api/case-arrival`,
strict request parser, and typed frontend API share the `CASE_ARRIVAL`
envelope; focused tests cover custom timestamps, business-hour validation,
empty gaps, and malformed response values.

Attribute-statistics evidence: `discover_attribute_statistics` counts event or
trace attribute values deterministically, supports first-class activity,
timestamp, resource, lifecycle, and case-id aliases, and optionally keeps one
occurrence per case. The standalone CLI, `POST /api/attribute-statistics`,
strict request parser, and typed frontend API share the
`ATTRIBUTE_STATISTICS` envelope; focused tests cover both scopes, deduplication,
missing values, custom attributes, and count validation.

Attribute-distribution evidence: `discover_event_distribution` implements the
calendar buckets `DAYS_MONTH`, `MONTHS`, `YEARS`, `HOURS`, `DAYS_WEEK`, and
`WEEKS`, fills the complete fixed ranges where applicable, and validates every
configured timestamp. The standalone CLI, `POST /api/attribute-distribution`,
strict request parser, and typed frontend API share the
`ATTRIBUTE_DISTRIBUTION` envelope; focused tests cover custom timestamp keys,
zero-filled bins, ISO week boundaries, and malformed values.

Attribute-KDE evidence: `discover_attribute_kde` provides deterministic
numeric and date Gaussian-density grids, preserving extrema, handling a
single observed value without producing an invalid bandwidth, and bounding
date samples before fitting. The standalone CLI, `POST /api/attribute-kde`,
strict request parser, and typed frontend API share the `ATTRIBUTE_KDE`
envelope; focused tests cover numeric/date parsing, finite non-negative
density validation, deterministic grids, sampling bounds, and malformed
requests.

Boundary-activity evidence: `discover_boundary_activities` exposes START, END,
and BOTH counts for configurable activity-like event attributes, skipping empty
traces and missing boundary values. The standalone CLI,
`POST /api/boundary-activities`, strict request parser, and typed frontend API
share the `BOUNDARY_ACTIVITIES` envelope; focused tests cover all variants,
custom keys, empty traces, and malformed counts.

Variant-statistics evidence: `discover_variant_statistics` groups complete trace
activity sequences, sorts them by descending frequency and deterministic
sequence order, and emits normalized stochastic-language probabilities with an
optional result bound. The standalone CLI, `POST /api/variant-statistics`,
strict request parser, and typed frontend API share the `VARIANT_STATISTICS`
envelope; focused tests cover empty traces, custom keys, limits, and probability
validation.

Case-statistics evidence: `discover_case_statistics` emits one deterministic
timing row per non-empty trace, supports case-id and timestamp attributes,
ascending/descending case-id/start/end/duration sorting, result limits, and
optional business-hour elapsed durations. The standalone CLI,
`POST /api/case-statistics`, strict request parser, and typed frontend API share
the `CASE_STATISTICS` envelope; focused Release tests cover sorting, limits,
business-hour slots, invalid timestamps, and malformed response values.

Rework evidence: `discover_rework` exposes the activity variant (number of
cases containing repeated activity occurrences) and the cases variant (total
events and total repeated-activity count per case), with custom activity and
case-id attributes. The standalone CLI, `POST /api/rework`, strict request
parser, and typed frontend API share the `REWORK` envelope; focused tests
cover duplicate activities, empty cases, custom keys, and count validation.

Chaotic-activity evidence: `discover_chaotic_activities` implements the single
entropy-based ranking variant with configurable activity attribute, smoothing,
and bounded input size. It returns deterministic frequency, entropy,
entropy-gain, and combined-score metrics. The standalone CLI,
`POST /api/chaotic-activities`, strict request parser, and typed frontend API
share the `CHAOTIC_ACTIVITIES` envelope; focused tests cover repeated runs,
custom attributes, empty input, invalid bounds, and malformed response data.

OCEL statistics evidence: `object_type_activities` returns deterministic
lexical object-type keys with deduplicated related activities, while
`objects_ot_count` returns deterministic event/object-type maps and counts
duplicate relation rows independently. Activity associations support none,
start, and end relation prefilters; object-type-dependent associations and
event-edge metrics include event couples, unique objects, event-object triples,
and sorted elapsed-time or configured business-hour samples. The native APIs, `minera_ocel --statistics`,
multipart local `POST /api/ocel/statistics`, and typed frontend
request/response guard share the `OCEL_STATISTICS` JSON envelope. Empty logs,
unreferenced objects, duplicate relations, invalid identifiers, prefilters,
repeated activity labels, and malformed timestamps are covered by focused
Release tests.

Process-cube evidence: `process_cube` validates rectangular typed feature tables,
supports exact numeric and categorical columns, composite categorical dimensions,
prefix indicator columns, mixed numeric/prefix Cartesian cells, explicit or
equal-width numeric edges, and mean, sum, min, max, count, median, first, and last
aggregation. The deterministic
`PROCESS_CUBE` JSON envelope is shared by the native tests, the headless
`minera_process_cube` CLI, local `POST /api/process-cube`, and typed frontend
request/response guards. Explicit bins are sorted and deduplicated, configured
cells retain their case-id sets, composite names accept escaped comma-delimited
CLI/query lists, and malformed JSON, ragged rows, mixed scalar types, non-finite
values, and unsafe division limits are rejected at the boundary.

Activity-split evidence: the native log filter splits every non-empty trace at
one or more anchor activities, preserves event/trace/log attributes, rewrites
case ids with a deterministic separator and zero-based suffix, and supports
both anchor-in-new-case (`this`) and following-event-in-new-case (`next`)
modes. Strict request parsing, CLI flags, local mine-server fields, and the
frontend form/serializer expose the same operation; focused tests cover
multiple anchors, no-match traces, attributes, case-id synchronization, and
invalid options.

Attribute-value repetition evidence: a whole-trace filter counts matching
activity, resource, lifecycle, timestamp, or generic event-attribute values
and keeps traces inside an inclusive repetition range. The native operation
preserves log and trace attributes, validates min/max ordering, and is wired
through strict mine-request fields, CLI flags, local-server form handling, and
frontend serialization/controls. Focused tests cover default and zero ranges,
generic attributes, missing values, aliases, and invalid bounds.

Case-grouping evidence: consecutive activity runs support first/last event
retention, while same-timestamp groups support first, last, and deterministic
lexical activity concatenation. First-class and generic event keys, custom
timestamp keys, and a configurable non-empty concatenation separator are
validated. Native, request, CLI, local-server, and frontend surfaces share
the same grouping options and focused edge-case coverage.

DFG interchange evidence: the strict classic text parser and deterministic
writer now have public input and output adapters. `minera_dfg INPUT.dfg` reads,
validates, and normalizes a document (or writes it to an explicit output path),
and its `--clean-noise` mode applies the native noise-threshold edge filter
with optional most-common-path overrides. The cached `GET /api/dfg` route
accepts the same cleaning query, while local `POST /api/dfg` accepts raw DFG
text and returns the canonical document. The typed frontend `importDfg` and
`fetchDfg` helpers expose the corresponding validation and export paths.

DFG conversion evidence: both classic DFG-to-workflow-net constructions are
available as deterministic native APIs: activity-defines-place emits one
visible transition per start or edge occurrence, while invisibles-no-duplicates
emits one visible transition per activity and silent edge routing. Boundary
activities are inferred when omitted, artificial names are collision-checked,
and the standalone DFG CLI, local conversion endpoint, and typed frontend
adapter share a validated `DFG_CONVERSION` envelope.

Trie conversion evidence: prefix tries now convert to deterministic workflow
nets with shared prefix places, silent terminal-prefix exits, multiple root
branches, and explicit source/sink places. The export CLI's
`--trie-to-petri` mode, cached local `/api/trie-to-petri` route, and typed
frontend request guard share the `TRIE_CONVERSION` envelope.

## Delivery order

1. Stabilize the native semantic contracts and differential fixture format.
2. Finish remaining traditional discovery and conformance variants.
3. Add OCEL 2 data, I/O, discovery, conformance, filtering, and visualization.
4. Add declarative/POWL/transition-system and advanced simulation families.
5. Add streaming, privacy, clustering/drift, transformations, and ML.
6. Add optional renderers, connectors, querying, and LLM integrations.

Every completed row must include focused unit tests. Algorithms with
non-unique models compare canonical behavior (language, footprints, fitness,
or replay) rather than generated node identifiers. Floating metrics use an
explicit tolerance. Performance claims require representative logs and may
not replace semantic tests.

## Petri-analysis parity evidence

The native analysis surface now exposes the classic integer marking equation,
the split-aware extended marking equation, structural workflow-net validation,
and bounded soundness diagnostics, including an explicit `woflan` family
selector over the same reachability, coverability, invariant, and pair
diagnostic contract. Woflan additionally constructs the silent sink-to-source
short circuit, reports its transition id and strong-connectivity result, and
serializes a separate short-circuit reachability projection while retaining
the original graph for completion witnesses. Woflan marking inputs follow its
single-place boundary contract. The soundness family also exposes a strict
`return_asap_when_unsound` control: a structural S-component coverage failure
returns immediately with invariant evidence, while a reachable non-final
deadlock or improper completion marking returns with the partial reachability
graph and its shortest witness. Structural pair diagnostics use a deterministic
node-split unit-capacity flow graph and report both directions whose maximum flow
exceeds one, preserving the duplicated-node coordinates in the JSON contract.
Bounded exploration also returns every shortest path to a marking that cannot
reach the final marking as `locking_scenarios`, alongside the compact first
witness and the `dead_tasks` alias for never-enabled transitions. Every pair
coordinate also has a stable place/transition input-output label for renderers.
Woflan now also exposes its `left` decision and a deterministic restricted
coverability-tree projection. The tree preserves one first-seen edge per
transition at each node, accelerates only against ancestors using the omega
marker `-1`, reports explicit state-cap exhaustion, and is kept separate from
the minimal coverability graph. These artifacts are available in the native
result, JSON envelope, and frontend response guard for both bounded and
unbounded workflow diagnostics.
The extended equation keeps trace events in
ordered segments, assigns each event to a synchronous or log move, constrains
segment-boundary markings, and solves the resulting integer state equation with
caller-controlled costs and deterministic node limits. Recovery separately
attempts a concrete firing sequence, so an incidence-feasible result that is
not realizable is reported rather than silently presented as a replay.

Strict request parsing covers family, miner, trace index, markings, transition
cost vectors, split indices, split limits, solver limits, bootstrap, and
diagnostic settings. The CLI emits a focused JSON document for any discovered
or imported Petri model, the local server exposes `POST /api/petri-analysis`,
and the frontend conformance tab provides the same controls with generation-safe
on-demand caching. Native, request, frontend, CLI, and hidden-server smoke
tests cover the current vertical. Karp–Miller-style coverability witnesses now
include a compact omega-marking graph, while bounded exploration exposes its
deterministic reachability markings and transition edges. Positive invariant
diagnostics expose the original, uniform-support, and relative-weight families
separately, including coverage gaps for each family and for S-components. These
projections are serialized by the CLI, local server, and frontend contracts.
Uniform-support vectors are retained only after rechecking the incidence
equation; when a single weighted certificate is not uniform, a bounded binary
invariant search recovers valid combinations of weighted rays. Weighted vectors
remain valid after normalization, and S-components retain the complete support
of each valid state-machine invariant (including a disconnected support when
the invariant itself spans it). The search has a shared caller-configurable
budget and reports exhaustion explicitly; an exhausted search never drives the
Woflan structural branch decision. A complete black-box edge matrix remains
open before this row can claim full parity.

## Extended Petri-arc evidence

Petri transitions now retain ordinary, reset, and inhibitor input arcs as
separate deterministic lists. Reset firing clears every token in its place;
inhibitor firing requires an empty place. Forward/backward replay, alignment
search, reachability/soundness exploration, model footprints, and basic,
stochastic, and extensive playout all use these enablement and firing rules.
PNML round-trips annotate extended input arcs with `reset` or `inhibitor`, and
the analysis JSON and frontend model type preserve both lists. Linear marking
equations deliberately return a structured unsupported result for these
nonlinear arcs instead of claiming an unsound solution; focused native tests
cover both arc kinds, transport, replay, playout, and that guard.

## Petri usage comparison evidence

Element-usage comparison replays two logs on a selected discovered Petri net
and emits deterministic place, transition, and arc records with both log
counts, totals, and signed direction. The CLI supports CSV/XES references and
inductive, Alpha, heuristic, and POWL model selection; the local server accepts
the same reference as multipart `reference_log` against its cached session;
both adapters accept comma-separated initial and final place markings, while
the frontend exposes those controls beside the reference upload. Replay uses
normal, reset, and inhibitor arc semantics and preserves weighted arcs in the
usage records. Native replay, standalone JSON, CLI, server, and frontend
request/response guards are covered; richer differential semantics remain
outside this adapter.

## Concept-drift evidence

The native concept-drift surface explicitly selects the `BOSE` variant and extracts deterministic relation-type count
vectors from ordered trace sub-logs and evaluates adjacent windows with a
seeded permutation test. It supports configurable sub-log/window sizes,
permutation counts, p-value threshold, result cap, and optional case-start
timestamps. Activity, timestamp, and case-id selectors accept the standard
keys or generic event/trace attributes, and the selected keys are echoed in
the JSON response. Strict variant/request parsing, standalone JSON, CLI, cached-session
server, frontend request/response validation, and focused native/request tests
are included. The current row remains partial pending differential edge-contract
fixtures and the reference implementation's dataframe/lazyframe adapters.

## Trace-variant query anonymization evidence

The native trace-variant query surface supports both Laplace and SaCoFa
variants. It builds a bounded prefix tree with an explicit completion marker,
first-seen activity expansion, independent depth-wise noise, and immediate
pruning, so a removed parent cannot reappear through a later Cartesian
expansion. The completion marker also preserves empty trace variants. Completed
variants are emitted as soon as they survive; prefixes
that reach the configured depth are released only at that boundary. SaCoFa
classifies predecessor/follower relations as Always, Sometimes, or Never and
uses behavioral-appropriateness violations in its bounded exponential
selection before applying the same non-negative integer release noise. The
deterministic seed is an explicit testing/reproducibility extension. Both
privacy surfaces also accept an explicit secure-random switch that draws one
key from the host operating system entropy provider per release call; the
effective key is not returned in diagnostics.

Strict `tvq_*` request parsing, standalone JSON, CLI
(`--trace-variant-query`), cached-session server endpoints, frontend query
serialization, response guards, output caps, secure-random parsing, and focused
Release tests cover
both variants, explicit completion boundaries, intermediate pruning,
behavioral-appropriateness regressions, malformed privacy settings, pruning,
reproducibility, empty-trace completion, open-interval noise boundaries, and
non-negative release counts. The PRIPEL-style adapter
matches exact and bounded nearest variants, removes identity/blocklisted
attributes, preserves monotonic event time, and applies deterministic bounded
categorical/numeric release noise, with the same transformations optionally
keyed by secure OS entropy. Full differential edge-contract review remains
open.

## Target-transformation evidence

The native target transformation emits one numeric row per event, preserving
trace and event order. `NEXT_ACTIVITY` produces sorted or caller-supplied
one-hot classes and a zero row for each trace's final event; `NEXT_TIME`
produces seconds until the next timestamp; and `REMAINING_TIME` produces
seconds until the trace's final timestamp. Temporal variants require valid
timestamps (including timezone offsets and fractional seconds), while custom
activity/timestamp attributes, escaped comma-containing class names, and
deterministic zero padding are supported.

The three variants are exposed through strict `target_*` request parsing,
standalone CLI JSON (`--transform-target`), local-server
`POST /api/transform-target`, and the frontend query/result adapters. Empty
logs, explicit class lists, unknown classes, invalid timestamps, and padding
bounds have focused native, request, and frontend coverage.

## Log-utility and ML-preparation evidence

The native log-utility layer provides deterministic seeded trace splitting,
fixed-length prefixes, the complete ordered prefix expansion (including the
empty prefix and per-case change indexes), prefixes ending immediately before
the first occurrence of a selected activity with elapsed-time targets, and
activity-targeted sublogs that exclude competing target activities. It also
provides automatic event/case feature selection with mandatory, numeric, and
bounded-cardinality attribute handling. All projections copy log, trace, event,
and typed attribute metadata. Timestamp targets accept ISO timestamps and
finite numeric values, and invalid or reversed intervals are rejected.

`minera_log_util` emits one stable `LOG_UTIL` JSON envelope for CSV/XES input;
the cached-session `POST /api/log-util` route applies all six variants with
bounded output, and the frontend exposes sorted query serialization and strict
recursive response validation. Focused native and frontend tests cover seed
reproducibility, metadata preservation, empty/full prefixes, change-index
alignment, target durations, activity-target exclusion, feature-selection
cardinality rules, option validation, and malformed response guards.

## Trie-transformation evidence

The native trie transformation collapses duplicate activity variants into a
rooted prefix tree with deterministic lexicographic children, depth, terminal
markers, custom activity attributes, and an optional positive path-length cap.
Truncation marks the retained endpoint terminal, matching the language of the
bounded variants. Recursive JSON, strict request fields, standalone CLI JSON
(`--trie`), cached-session `GET /api/trie`, and frontend recursive response
validation are covered by focused native and adapter tests.

## Interval-tree transformation evidence

Open-path interval extraction selects the first later event whose start instant
is at or after the source completion instant, applies the configured epsilon
boundary, and retains deterministic source/target/case identity. It supports
custom activity and timestamp attributes and an exact activity-pair filter;
invalid timestamps, malformed filters, negative epsilon, and timezone offsets
are rejected or normalized explicitly.

The interval result is exposed through recursive JSON, strict request fields,
standalone CLI JSON (`--interval-tree`), cached-session `GET
/api/interval-tree`, and frontend query/response validation. Point queries
return inclusive interval indices, while range queries return every interval
overlapping the inclusive bounds; malformed, non-finite, partial, or reversed
query bounds are rejected consistently by native, CLI, server, and frontend
adapters.

## OCEL description evidence

The object-centric description family exposes both deterministic variants:
`VARIANT1` is the textual event/object representation and `VARIANT2` is the
tuple representation, with optional timestamps and lifecycle durations. The
native API preserves stable event and object ordering, the standalone
`minera_ocel --describe variant1|variant2` selectors expose the same envelopes,
and `POST /api/ocel/description` accepts multipart JSON with strict variant
and timestamp controls. Frontend upload/query helpers validate the textual
and tuple shapes, including timestamp-disabled tuples and malformed duration
fields.

## Case-relations merging evidence

The case-relations variation preserves the base log, inserts only incoming
events with a declared right-case relation, rewrites those events to the left
case, preserves duplicate relation rows, supports new left cases, and sorts
each resulting case deterministically by a configured timestamp attribute.
Malformed relation headers/rows, empty identifiers, missing timestamps, and
invalid timestamps are rejected. The result is exposed as recursive JSON,
strict CLI flags, a multipart local-server endpoint, and a frontend upload
adapter with runtime response validation.

## Basic random-tree generation evidence

The BASIC random-tree generator uses the seeded recursive-depth process-tree
contract: the root sequence has entry and exit activities, recursive operators
are uniformly selected among sequence, loop, XOR, and parallel, operator
child-count ranges are bounded, and loop exits are silent. Leaf probability,
minimum/maximum depth, seed, and deterministic activity labels are validated
and exposed through recursive JSON, standalone CLI output, cached-session-free
server GET, and frontend response validation. The PT-and-log generator also
preserves inclusive-choice nodes when the OR weight is selected; their
non-empty subset language is available through playout, Petri conversion,
POWL/alignment expansion, PTML, BPMN, JSON, and frontend rendering.

## POWL parity evidence

The POWL review is pinned to commit
`0715c31f3f1d85924107d6aa5329806db69df899`. Minera independently implements
the five selectable discovery behaviors, both filtering policies, filtering
threshold (including strict decreasing-factor boundaries and dynamic filtering
when its threshold is zero), frequency-order ratio, and a caller-controlled
brute-force activity limit. The common loop cut follows the reference behavior
for:

- start/end boundary union and removal of boundary nodes and incident edges;
- weak redo-component discovery;
- start/end reachability repair and completeness-based component merging;
- one merged redo group; and
- multiplicity-preserving, alternating contiguous do/redo projections.

Focused regressions cover ordered multi-activity redo behavior, different
start/end boundary activities, prefix/suffix false positives, incomplete
start- and end-side boundary reachability, split return components, single
closed variants, strict transitive sequence grouping with skip repair,
least-cost sequence projection, strict filtering boundaries, and dynamic
zero-threshold filtering, and concurrency grouping with skippable-branch
repair. Recursive POWL JSON, request
validation, CLI flags, local-server mining, frontend controls/rendering,
converted-net conformance and decoration, and the PNML/BPMN/decision/playout
selectors are exercised by native tests, frontend tests/build, and headless
CLI/server smoke checks.

The frontend conformance surface also exposes ordinary Petri-net playout for
every discovered workflow-net selector, including POWL, with basic sampling,
extensive enumeration, explicit markings, bounded steps/length, seeds, and
optional visited-element JSON downloads. The basic and extensive POWL request
paths share the strict native response guards and deterministic API tests.

POWL-to-process-tree conversion is available through the native conversion API
and the headless `--powl-ptml` export. Direct native conversion retains its
deterministic approximation for residual partial orders; exported PTML uses the
workflow-net route shared with the server and rejects non-block structures.
Transition frequency bounds map to optional or silent-redo loop blocks;
connected partial-order components are layered by deterministic topological
levels and disconnected components become parallel branches. Ordinary sequence,
choice, inclusive-choice, parallel, and loop models round-trip through the
existing tree-to-POWL conversion; inclusive choices use a bounded XOR of
non-empty concurrent subsets because the POWL object model has no primitive OR
node.

The reverse workflow-net conversion is also available through the native
`to_powl(const PetriNet&)` API. It validates designated source/sink reachability
and final-marking reachability, keeps the silent return boundary used by loop
fragments, strips only deterministic silent routing wrappers, and recursively
recognizes exclusive branches, do/redo cycles, sequence cuts, and residual
strict partial orders. Labels and child ordering are deterministic;
extended-arc nets, sink escapes, malformed boundaries, unsound markings, and
unresolved transition cycles are rejected instead of being silently
over-approximated. Focused round-trip tests cover silent, visible, optional,
unbounded, exclusive, loop, sequence, nested choice, nested loop, concurrent,
and diamond partial-order models, plus invalid extended, non-workflow, unsound,
and sink-escape nets.

Workflow-net-to-process-tree conversion now uses a strict block reducer over
the reverse workflow reconstruction. Exact sequence, XOR, parallel, loop,
silent, and frequency blocks are preserved; residual non-series-parallel
partial orders and extended arcs are rejected. The headless export CLI accepts
`--pnml-to-ptml` and `--bpmn-to-ptml` without an event log, and `/api/ptml`
accepts a miner selector so the frontend can request PTML for any selected
workflow net when the strict conversion succeeds.

Ordinary Petri-net reduction now applies a deterministic fixed point of the
single-entry and single-exit silent-transition rules to generated process-tree,
heuristic, POWL, BPMN, and mixed reset/inhibitor nets. Conservative series-place
fusion handles private inputs with multiple producers, while duplicate silent
transitions, inert silent self-loops, and reset arcs guarded by the same
inhibitor are simplified when their token effects are provably unchanged.
Extended arcs that mention a candidate removed place block that local rewrite;
unrelated extended guards are retained and remapped. Source and sink places and
places carrying supplied initial/final tokens are retained by the ordinary
passes. The optional parallel-place rule fuses complete equivalent place
signatures (including ordinary arc weights and extended guards), preserves
equal supplied markings through representative aliases, and rewrites every
ordinary occurrence. The optional abstraction rule removes a private silent
fan-out bridge with multiple producers after checking target guard equivalence
and direct-route conflicts. Both broad rules are disabled by default because
their search cost is quadratic; callers enable them through
`PetriNetReductionOptions`. Place mappings and markings are returned for
callers that need to preserve workflow metadata, malformed nets are left
unchanged, and focused regressions cover chains, fan-out/fan-in, shared-place
guards, markings, mixed extended arcs, series fusion, parallel transitions and
places, abstraction, self-loops, invalid inputs, and deterministic output.

## ILP discovery parity evidence

The bounded ILP discovery adapter now normalizes event attributes through the
selected activity key, folds duplicate variants while retaining their support,
and inserts the default `▶`/`■` synthetic boundaries as silent transitions.
Native callers can explicitly provide empty boundary labels to retain direct
source/sink wiring. The alpha sequence-encoding threshold uses cumulative
occurrence vectors, so continuations are compared after their complete prefix
rather than only by the immediately preceding activity; rejected continuations
cut the remaining suffix of that variant. The complete variants and activity
alphabet remain intact while weak sequence-encoding candidates are excluded.
When no causal relation is supplied, the one-way portion of the normalized
directly-follows graph is derived automatically; callers may override it with
deterministic `A=>B` comma-separated entries. Frequency-based noise filtering
can remove low-support variants and their activities before region search. The
dependency-free integer-region search reports visited candidates, LP-feasibility
attempts, branch counts, and exhaustion instead of claiming an unbounded
optimum. Supplied causal pairs are required producer/consumer constraints, not
an exhaustive arc list: the bounded search may add compatible producer or
consumer arcs when a composite region is needed to satisfy all retained trace
markings. Each supplied pair is optimized independently, with exact duplicate
regions removed after selection; explicit pair places use zero initial/final
markings so they survive workflow-net assembly. Region signatures use
arbitrary-width bit vectors and are generated lazily in deterministic depth-
first order, so nine-or-more activity alphabets no longer fail solely because
a fixed-width signature table would be too large; `max_nodes` bounds candidate
work and diagnostics before a large candidate table is allocated.

Region feasibility now follows the cumulative sequence-row formulation: the
producer mask is applied to the negative prefix before an event, the consumer
mask to the prefix including that event, and non-final rows require a
non-positive pre-fire slack while each fully retained final row is an equality.
The selected region minimizes the cumulative objective (`producer - consumer`)
over one row per retained variant and resolves equal scores by sparse
deterministic ordering (objective, arc count, then masks). Variant multiplicity
still drives support filtering, while the optimization rows remain one per
variant. This keeps final-row behavior independent across variants and lets a
composite place remain valid when alpha filtering removes a rare suffix from
its rows.

Constraint construction now canonicalizes repeated encoding rows while leaving
the objective contribution of every retained variant row intact. Thus repeated
loops and competing continuations cannot accidentally increase feasibility
constraints or change the objective merely because the same prefix was observed
more than once. Support accumulation is saturating at the unsigned counter
limit, and the inclusive alpha comparison is covered at and just below an
exact support boundary.

Each requested causal pair is seeded with a direct feasibility solve before
bounded refinement is shared across the remaining pairs. If the safety bound
is reached while refining one pair, already-seeded later pairs are retained;
the diagnostics explicitly report the resulting partial optimization.

Strict request parsing, standalone CLI flags, the cached-session local-server
endpoint, and typed frontend query serialization cover activity/boundary keys,
alpha, causal edges, noise threshold, and safety bounds. Focused tests cover
default and custom boundaries, duplicate-variant support, cumulative-prefix
alpha filtering, causal priority, activity removal, deterministic bounds, and
malformed settings. Full numerical solver differential review remains open, so
this row stays partial.

## Genetic discovery parity evidence

The bounded classic genetic miner is implemented as a deterministic causal
matrix search with explicit population, generation, crossover, mutation,
elitism, activity, and candidate-state limits. Seeded evolution repairs
disconnected and inverse matrix projections, preserves partition grouping, and
converts compatible places plus silent routing hubs into a replayable workflow
net with canonical initial/final markings. Each mating step retains reciprocal
offspring with deterministic inverse repair. Fitness is explicitly 0.4 token-
replay fitness plus 0.6 fitting-trace coverage over the normalized log;
diagnostics report the search bound and best candidate without fabricating an
optimum after exhaustion. Optional strict timestamp and case-id selectors
resolve standard fields plus generic event/trace attributes, including stable
duplicate-timestamp ordering and deterministic regrouping by selected case.

The native result has a standalone JSON contract and strict request parsing,
CLI flags, cached-session server endpoints, and frontend types/query helpers.
Focused tests cover deterministic output, chain fitness, disconnected repair,
custom event/trace selectors, duplicate timestamp ordering, parameter
rejection, candidate caps, and malformed frontend responses. The bounded
adapter intentionally does not expose a separate fitness-log artifact in the
standalone JSON contract; diagnostics remain scalar and the normalized log is
used internally for scoring.

## Organizational SNA parity evidence

The organizational SNA vertical exposes the four log variants as a separate
deterministic contract: normalized directed handover and subcontracting
weights, symmetric per-trace working-together weights, and symmetric Pearson
joint-activities weights. Each variant is available through both the native
log path and explicit `_pandas` adapter selectors, with the same validated
semantics and deterministic output. Resource names and edge records are sorted
for stable JSON; blank resources are excluded from the resource alphabet while
empty or resource-less traces remain part of trace-count denominators. Handover
beta, subcontracting distance, optional metric normalization, custom
`sna_activity_attribute`/`sna_resource_attribute` selectors, and output safety
caps are strictly validated.

Native tests cover repeated resources, normalized boundaries, symmetric output,
degenerate correlations, truncation, and parser rejection across the four
`_pandas` selectors as well as their log counterparts. Standalone JSON, CLI
flags, cached-session server routes, and frontend query/result guards cover all
eight selectable variant names. Deterministic role discovery is also exposed via
the `ROLE_DISCOVERY` contract and `--roles`/`POST /api/roles`; both verticals
accept `*_activity_attribute` and `*_resource_attribute` selectors, resolving
standard keys from first-class event fields and custom keys from typed event
attributes; the handover, working-together, and subcontracting pandas-style SNA
variants also accept a case selector that stably
regroups events from trace or event attributes, while the log variants retain
their existing trace boundaries. Local diagnostics and network analysis are
documented below.

The organizational network-analysis vertical emits deterministic flat rows
(`NETWORK_ANALYSIS`) by linking each event to the earliest later event with
the same relation key. Defaults use case boundaries, resource nodes,
activity labels, and timestamp ordering; `_out`/`_in` edge references,
custom selectors, frequency/performance output, weekly business-hour slots,
and strict row/value bounds are available through the native API, CLI,
cached-session server route, and typed frontend helper. The catalog's
`DATAFRAME` selector is explicit in request/query metadata and is validated at
every adapter boundary. Role discovery likewise exposes both LOG and PANDAS
selectors with deterministic role ordering and a tagged `ROLE_DISCOVERY`
envelope. Broader differential review of additional organizational variants
remains partial.

The resource-profile vertical exposes the eleven scalar metrics through the
`RESOURCE_PROFILES` JSON family, `--resource-profiles` CLI mode, cached
`POST /api/resource-profiles`, and typed frontend query/response helpers.
Metrics use deterministic resource/activity rows, half-open timestamp slots,
configurable activity/resource/timestamp/start-timestamp/case-id selectors
(including typed custom event and trace attributes),
FIFO lifecycle pairing with atomic-event fallback, and explicit zero-safe
diagnostics for empty denominators. Workload and multitasking preserve the
reference interval policies; safety limits reject unbounded resource or row
output.

Local diagnostics now exposes the four group maps (`group_relative_focus`,
`group_relative_stake`, `group_coverage`, and `group_member_contribution`) for
both `org:group` event attributes and explicit role/clustering memberships.
The `LOCAL_DIAGNOSTICS` contract is available through native, JSON, CLI,
cached-server, and typed frontend query/guard adapters with deterministic
ordering, finite zero-denominator ratios, and bounded nested output.

Profile clustering now explicitly selects the `SKLEARN_PROFILES` variant and
has its own bounded `PROFILE_CLUSTERING` contract in
addition to trace-attribute clustering. Deterministic seeded k-means operates
over activity/succession one-hot profiles, explicitly selected trace/event
string or numeric attributes, or a caller-supplied feature list. Positive
iteration, trace, feature, and cluster bounds plus finite tolerance and typed
representation flags are validated by the shared request parser. The
`--profile-clustering` export mode, cached `POST /api/profile-clustering`, and
frontend query/result guard expose labels, clusters, feature names, inertia,
and convergence diagnostics without opening a GUI.

Trace-attribute clustering keeps the input order of attribute groups and of
equal-frequency variants before applying the five named distance families.
DMM vector matching computes activity and succession similarities separately
before applying `alpha`; empty succession vectors have maximal distance, and
traces without the selected grouping attribute are excluded. The DFG distance
uses complete group sublogs, so its result is invariant to the shared
`percent` control. Linkage children retain merge order while labels remain
deterministic. Focused Release coverage exercises these numerical and ordering
rules plus repeated serialization; broader differential fixtures remain.

## Evaluation parity evidence

Model evaluation covers token- and alignment-based replay fitness; token,
alignment escaping-edge, and aligned-automaton precision; token-frequency
generalization; arc-degree, extended Cardoso, and extended cyclomatic
simplicity; the composite evaluator with normalized fitness, precision,
generalization, and simplicity weights plus F-score; and stochastic-language
earth-mover distance. Automatic fitness
and precision selection uses workflow-net soundness, while every concrete
variation remains directly selectable. Precision exposes task-identity and
search-state controls; simplicity exposes its arc-degree target, reachability
bound, and explicit initial/final markings. Distance comparison accepts an
external CSV or XES log and independently controls mass normalization and
strict equal-mass validation.

The shared request parser rejects empty, malformed, non-finite, negative, and
out-of-range settings before evaluation. Results have standalone and embedded
JSON forms, active-variation replay details, explicit completion/exhaustion
state, and finite-or-null metrics. Composite results include the four component
scores, normalized weights, weighted average, and F-score. The CLI evaluates every built-in Petri model
and an optional imported model; the local server evaluates any cached model and
accepts multipart reference logs. The frontend exposes the complete option set,
discovers models from actual nets as well as embedded results, and guards
on-demand caches/results across analysis changes. Native evaluator, request,
JSON, CLI, hidden-server, frontend-test, and production-build validation cover
the full vertical.

Earth-mover evaluation retains the historical `EXACT` selector as the
backwards-compatible default and also exposes `PYEMD`. The explicit variant
uses normalized Levenshtein trace costs with the same dependency-free exact
transport solver. Duplicate trace keys are consolidated in stable order;
invalid weights, non-finite costs, empty positive-mass inputs, and unequal
masses under strict mode are rejected deterministically. Normalized unequal
masses are reported through the result's `normalized` flag, and the selected
variant is retained in standalone and embedded JSON.

## Token-replay parity evidence

The backwards token-replay variation independently follows the pinned legacy
behavior: unknown labels are ignored, same-label transitions are evaluated
sequentially against the evolving marking, and a blocked unique transition
searches backwards through silent transitions before weak firing. Reverse
routing uses the reference's global transition-visit rule and weighted-marking
difference semantics. Exact final-marking deficits and surplus tokens feed the
legacy fitness accounting.

The variation and immediate-stop setting are exposed through the native API,
strict request parsing, CLI, local server, and frontend controls. Focused
regressions cover perfect replay, multi-step silent routing, forced missing
tokens, immediate stop, unknown labels, duplicate labels, weighted arcs, and
empty logs.

## Decision-mining evidence

Decision mining now has a guarded data-Petri-net projection in addition to the
decision-tree diagnostics. Each observed branch path becomes a deterministic
guard and read-variable set; transitions consuming multiple decision places
combine those rules conjunctively. The native API, standalone
`minera_data_petri`, cached local `POST /api/data-petri-net`, and typed frontend
request/response guard expose the same `DATA_PETRI_NET` envelope. The native
`mine_decision_tables` API and `minera_decision_table` command emit deterministic
numeric and one-hot columns from both clean replay and alignment-backed model
choices. Deviating variants use their optimal alignment; model-only moves do not
consume a log event, while synchronous and log-only moves advance the context.
The alignment path retains hidden transition ids for this extraction even
though ordinary conformance display continues to omit silent moves.
The response reports replayed, aligned, and alignment-failed trace counts, and
never fabricates rows when the net is unsound or a bounded alignment cannot be
computed. The standard
export accepts `--decision-tables`, the cached server exposes
`POST /api/decision-tables`, and the typed frontend request validates the same
encoded-table envelope. Decision depth, split/leaf minimums, candidate caps,
gain threshold, trace-attribute inclusion, and selected context attributes are
validated and shared by the native, CLI, server, and frontend decision paths.
The same controls flow into guarded data-Petri projections and their nested
tables. Encoded tables retain a deterministic rolling window of the last
visible-event context snapshots before each choice (bounded to 64; the
default k=1 behavior remains the latest snapshot). The window advances only
after visible events, and an empty feature map still occupies its event slot;
each snapshot is captured before the choice is consumed so post-choice
attributes cannot leak into its row. `--decision-lookback`, `decision_lookback` on the table and
data-Petri endpoints, and the typed frontend table options share the same
strict 1..64 validation; the ordinary decision endpoint intentionally does
not send or accept this table-only parameter. Richer classifier backends and
columnar training integrations remain open.

## Contextual label-splitting evidence

Contextual splitting now builds one segment node for each distinct
`(activity, prefix+suffix context)` pair while retaining the first-seen order
of those nodes. Context similarity is measured over normalized activity tokens
rather than the characters in their labels, so multi-character names cannot
accidentally collapse into one context. Weighted edges use the configured
strict minimum, and one global greedy-modularity graph accumulates
cross-community edge weight across all selected activities. Isolated vertices
remain singleton communities, zero-gain merges are accepted, and clustering
stops at the first negative gain. Community numbering follows occurrence
support with the graph's deterministic merge order as its stable tie break.
Custom activity and target columns, target-activity selection, context bounds,
suffixes, strict edge thresholds, and malformed numeric settings continue to
share the native/request/CLI/server/frontend contract. Focused Release
coverage includes token-boundary distance, weighted competing gains,
zero-edge graphs, zero-gain paths, deterministic ties, cross-activity global
normalization without cross-label edges, custom-column preservation, and
threshold boundaries.

## Live-stream transport evidence

Native live event and trace streams expose inactive, active, and finished
states, accept items before activation, dispatch through a bounded worker pool,
and drain accepted items before stop returns. Observer registration and
deregistration are synchronized, observer exceptions are isolated, and a
finished stream cannot be restarted or appended to. Focused Release tests cover
queueing, drain-on-stop, observer failures, invalid pool sizes, and lifecycle
transitions; the headless `minera_live_stream` command exercises CSV event
delivery end to end.

`CsvEventStreamReader`, `XesEventStreamReader`, and `XesTraceStreamReader` are
restartable readers with transformation and acceptance callbacks. They expose
single-item reads and direct delivery into the live event or trace stream;
focused tests cover filtering, reset behavior, XES trace grouping, and complete
delivery counts. The headless `minera_stream_import` command exercises both
event and trace modes without a GUI.

`PersistentStreamingDfg` adds a process-restartable, append-only CSV state file
for incremental directly-follows discovery. Reopening the same file rebuilds
activity, boundary, edge, and open-case state; `reset()` truncates it back to a
validated header. The headless `minera_streaming_dfg` command accepts
`--state STATE.csv` for this mode. Focused Release coverage includes
interleaved cases, quoted identifiers, multiline activities, restart replay,
reset, and invalid-state rejection. The file is a local restart checkpoint;
the standalone `minera_streaming_store` command now exercises deterministic
classic, mutex-protected, and remote dictionary variants from a line-oriented
script. Local stores preserve sorted key/value views; the remote store uses
bounded connection and response limits, optional database selection, and a
prefix-isolated `clear()` operation, while explicit database/all flush commands
remain available for administrative use. Focused tests cover CRUD ordering,
parallel writers, validation, and unavailable remote endpoints without opening
any interactive process.

## Log-skeleton parity evidence

Classic log-skeleton discovery exposes all six pinned constraint families:
equivalence, always-after, always-before, never-together, directly-follows,
and activity frequency. It preserves weighted trace variants, the reference's
event-occurrence denominators, positive-only counter subtraction, first-seen
frequency tie ordering, and the event-count noise threshold. Direct native
callers retain the pinned raw threshold arithmetic; CLI and server boundaries
strictly validate the conventional finite `[0,1]` range.

Classic conformance supports activity-list, trace, and whole-log entry points,
selected constraint families, source-absent vacuity, unknown and required
activity-frequency diagnostics, per-trace deviation totals, fitness, and fit
status. Focused tests cover repetition-sensitive relations, noisy weighted
variants, threshold and tie quirks, family selection, duplicate variants,
unknown/missing activities, non-finite direct-call behavior, and empty inputs.
The model and trace diagnostics are serialized with prevalidated activity IDs,
available through CLI/server requests, and rendered in the frontend with all
settings exposed.

## DFG statistical and causal parity evidence

The statistical DFG surface exposes NATIVE, FREQUENCY, and FREQUENCY_GREEDY
as individually named aliases of the same windowed counter. A zero window
counts one self-pair per event, positive windows connect exactly the two
positions at that distance, and optional per-case deduplication is applied
independently inside each trace. FREQ_TRIPLES counts overlapping contiguous
triples and intentionally ignores the window and deduplication settings.

PERFORMANCE and PERFORMANCE_GREEDY are named aliases over adjacent waiting
times. A target's configured start-timestamp attribute falls back to its
complete timestamp; negative waits clamp to zero. Mean, median, minimum,
maximum, sum, sample standard deviation, raw observations, and the complete
summary are selectable. Timestamp parsing is strict. Weekly business-hour
slots support the pinned Monday-Friday default, caller-defined ranges,
timezone-bearing timestamps, and multiweek intervals.

CASE_ATTRIBUTES counts every requested trace attribute on directly-following
edges, maps `concept:name` to the native case id, skips missing attributes, and
optionally returns per-event node annotations. CLEAN preserves the pinned
columnar algorithm's unusual boundary rule: start/end counts include every
non-empty case, while only a non-boundary current row owns an edge to its next
row. CLEAN_POLARS is an adapter alias. CLEAN_TIME computes each activity's
mean offset per case before averaging those case means.

Causal ALPHA emits a relation only when its positive reverse is absent.
Causal HEURISTIC emits both directions with
`(forward-reverse)/(forward+reverse+1)`, including synthesized zero-reverse
entries and the pinned self-relation assignment behavior. Focused tests cover
all aliases, windows including zero, deduplication, triples, every performance
aggregation, timestamp fallback and rejection, custom business hours, missing
case attributes, node counts, CLEAN boundary/tie behavior, CLEAN_TIME repeated
activities, zero-frequency reverses, and self loops.

Every variant remains individually selectable through the native API, strict
request fields, CLI, and local server. Deterministic JSON resolves all
activities against the shared registry and preserves alias names; the frontend
exposes all settings and renders frequency, triple, performance,
case-attribute, clean-time, and causal artifacts.

DFG precision evidence: `evaluate_dfg_precision` builds weighted observed
prefix observations, admits only prefixes reachable through the supplied start
and directly-follows maps, and computes escaping versus activated transitions
with deterministic counters. The standalone `minera_dfg_precision` command,
cached-log `POST /api/dfg-precision`, and typed frontend request/response guard
share the `DFG_PRECISION` envelope; custom activity attributes and malformed
model/log inputs are validated by focused tests.

## Connected DFG filtering evidence

The native DFG filtering API now exposes deterministic activity-percentage,
path-percentage, dependency-threshold, noise-threshold, to-activity,
from-activity, and contain-activity variations. Each filter works on explicit
activity, boundary, and edge frequencies, removes only the requested
low-support nodes or paths, and repairs the graph so retained activities remain
reachable from a start and can reach an end. Noise cleaning compares each edge
with the smaller of the strongest ordinary edge counts at its endpoints and
honors an explicit most-common-path keep set without changing activity or
boundary tables. The CLI, local server, and typed frontend export controls
accept the same strict selector names and apply them in a deterministic order,
including the optional keep-all-activities behavior for path and connected-edge
pruning. Focused tests cover boundary preservation, branch pruning, dependency
ties, noise overrides, unknown activities, invalid thresholds, transport
validation, and deterministic output ordering.

## LTL trace-filter evidence

The native LTL filter exposes eventually-follows, contiguous A-next-B-next-C,
four-eyes resource separation, and different-person predicates. It supports
generic event/resource attribute keys, positive and negative selection,
strict pattern arities, and optional inclusive timestamp-difference bounds for
eventually-follows. Request parsing, the export CLI, the local mine endpoint,
and the typed frontend controls all apply the predicate after ordinary log
filters and preserve event/trace attributes. Focused regressions cover
timestamp validation, generic attributes, polarity, missing four-eyes sides,
and malformed request fields.

## Trace-pattern filtering evidence

The native log filter accepts one or more admitted activity patterns. Literal
patterns match complete variants, while a `...` token matches zero or more
activities and therefore expresses prefix, suffix, and infix constraints.
Patterns are separated by semicolons at request boundaries, retain trace and
log attributes, and support positive (keep matches) or negative (keep
non-matches) polarity. The request parser, CLI, local mine endpoint, and
frontend form serialize the same pattern contract. Focused regressions cover
exact and wildcard matching, multiple admitted patterns, negative polarity,
malformed empty tokens, and integration after ordinary event projection.

Exact admitted-variant filtering is available alongside the pattern filter
without changing its wildcard contract. The `admitted_variants` wire field
uses semicolon-separated variants and comma-separated activities; `[]` denotes
an empty variant, duplicate entries remain deterministic, and `...` remains a
literal activity rather than a wildcard. Positive polarity keeps only exact
matches, while negative polarity removes them. Native, request, CLI, local-server,
and typed frontend paths
preserve trace metadata and input order, including empty variants.

## Adjacent-path filtering evidence

Adjacent-path filtering compares neighboring events with inclusive elapsed-time
bounds when requested. The default activity and timestamp fields retain the
canonical event values; callers can select aliases such as `concept:name` or
`time:timestamp`, or a generic event attribute. An explicitly missing custom
attribute does not fall back to the canonical field, and malformed timestamps
are rejected only after a pair matches the selected activity values. Positive
and negative polarity, request/CLI/local-server/frontend serialization, and
ordinary and performance path entry points share this contract.

## Case-performance filtering evidence

Complete-case duration filtering measures the input-order difference between the
first and last event timestamps and applies inclusive lower/upper bounds. Either
bound may be omitted; an untimed or malformed trace is excluded while the
filter is active, and reversed or non-finite bounds are rejected. The native
standalone filter and integrated log options are exposed through request parsing,
the CLI, the local mine endpoint, and typed frontend controls. Focused tests
cover exact boundary inclusion, one-sided maxima, malformed timestamps, and
invalid bound ordering.

Case-selection filtering now also exposes the classic count and size variants.
`filter_ncases` keeps the first requested number of surviving cases in input
order (including the intentional zero-case result), while `filter_case_size`
uses inclusive event-count bounds and defaults to a two-event minimum. Integrated
min/max length bounds are strictly parsed and reject reversed ranges; request,
CLI, local mine, and frontend controls share the same non-negative integer
contract. Focused tests cover default and one-sided size behavior, count
truncation, zero limits, ordering validation, and adapter serialization.

Variant and endpoint filtering now covers the remaining frequency and polarity
forms in one integrated pass. Positive or negative start/end activity lists are
validated as paired options and applied after length/segment extraction. Native
variant helpers support top-k (including an intentional zero result), inclusive
minimum/maximum per-variant case shares, and cumulative percentage selection
with tied boundary variants retained. Request fields, CLI flags, local mining,
and typed frontend controls use fractions on the wire and percentages in the
UI; focused regressions cover zero targets, ties, reversed bounds, subordinate
endpoint settings, and combined constraints.

Prefix/suffix evidence now covers the full anchor variation: strict mode
includes the selected activity, non-strict mode cuts immediately before/after
it, and first/last occurrence selection is deterministic. Missing anchors drop
the trace; subordinate options without an activity are rejected. Standalone
prefix/suffix APIs, integrated request fields, CLI flags, local mining, and
frontend controls share this contract, with focused regressions for repeated
anchors, empty cuts, sequential cuts, and malformed options.

## Numeric attribute filtering evidence

Numeric attribute filtering parses one generic event attribute as a finite
number and applies inclusive lower/upper bounds. Event mode projects matching
events (or only out-of-range numeric events under negative polarity); trace mode
keeps or drops complete cases when any event is in range. Missing and malformed
values never match an event projection, while negative trace polarity retains
cases with no matching numeric value. Open bounds, reversed/non-finite values,
missing keys, and missing bounds are validated consistently by the native API,
request parser, CLI, local mine endpoint, and frontend controls. Focused tests
cover both modes, polarity, malformed values, one-sided ranges, and strict
adapter serialization.

## Frequency attribute filtering evidence

Top-activity filtering ranks activity names, or a selected generic event
attribute, by global occurrence count and projects every trace onto the
requested number of values. Ties are resolved lexicographically and a zero
limit intentionally removes all events. Relative-occurrence filtering keeps
values whose inclusive count reaches the configured fraction of cases or
events; case mode counts a value once per trace, while event mode counts every
occurrence. Missing generic attributes never match. Both standalone filters
and integrated mining options preserve log/trace metadata and are available
through native, request, CLI, local-server, and frontend surfaces. Focused
tests cover zero and one-sided limits, tie ordering, duplicate values,
case-vs-event denominators, missing attributes, malformed stakes, and adapter
round-trips.

Absolute occurrence thresholding complements the relative form with an
inclusive integer count over the original input log. It supports activity or
generic-attribute values, projects matching events while dropping missing
generic values, and composes with top and relative frequency projections in a
deterministic order. The classic pluralized variant-percentage compatibility
entry point shares the cumulative top-variant and boundary-tie semantics.
Request aliases, CLI flags, frontend validation, and focused native/adapter
tests cover zero thresholds, missing keys, and malformed counts.

## Classic DFG text I/O evidence

The deterministic DFG interchange surface reads and writes the compact classic
text layout: a sorted activity registry, `idxcount` boundary rows, and
`source>targetxcount` edge rows. Counts are positive 64-bit integers, duplicate
or out-of-range rows are rejected, unsafe multi-line/trimmed labels are
rejected, and UTF-8 labels plus stream/path aliases round-trip through the
native `DfgCleanAnalysis` payload. Trace variants and activity totals are not
invented into this format; CLI/server/frontend output adapters serialize the
cached post-filter graph, while input adapters remain open.

## Streaming DFG evidence

`StreamingDfgDiscovery` consumes case/activity events in arrival order, keeps
the latest activity for every open case, increments starts, activity totals,
and directly-following edges atomically, and derives deterministic current end
counts from the open-case table. Empty identifiers are skipped, reset clears
all state, and snapshots include open cases for downstream consumers. The
standalone `minera_streaming_dfg` CSV command, multipart `POST
/api/streaming-dfg`, and typed frontend request/response guard expose the same
`STREAMING_DFG` JSON contract; focused tests cover interleaved cases, empty
records, reset, deterministic serialization, and malformed frontend payloads.

## Streaming footprints evidence

`StreamingFootprintsConformance` keeps a mutex-serialized last activity for each
open case and checks model activities, first-activity boundaries, allowed
sequence/parallel pairs, and final end activities on explicit termination.
Snapshots expose deterministic per-case prefix diagnostics and aggregate event
and deviation counts. The standalone `minera_streaming_footprints` command,
multipart `POST /api/streaming-footprints`, and typed frontend request/response
guard expose the same `STREAMING_FOOTPRINTS` contract; focused tests cover
interleaved cases, unknown activities, boundary and pair deviations, reset,
termination, deterministic serialization, and malformed frontend payloads.

## Streaming temporal evidence

`StreamingTemporalConformance` validates each arriving event against every
eligible earlier event in the same case, using profile means, standard
deviations, and a positive zeta threshold. It supports canonical or generic
activity/timestamp attributes, skips incomplete or unparsable records, keeps
per-case histories isolated, and emits deterministic deviation diagnostics.
The standalone `minera_streaming_temporal` command, multipart
`POST /api/streaming-temporal`, and typed frontend request/response guard expose
the same `STREAMING_TEMPORAL` contract; focused tests cover prior-event pairing,
zero-variance profiles, custom attributes, invalid timestamps, reset, and
malformed frontend payloads.

## Streaming token-replay evidence

`StreamingTokenReplay` keeps one marking per open case and supports forward and
backwards replay variants, bounded silent-transition routing, weak firing with
missing-token accounting, duplicate-label handling, immediate-stop behavior,
final-marking checks, reset, and deterministic JSON snapshots. The standalone
`minera_streaming_tbr` PNML/CSV command, multipart `POST /api/streaming-tbr`,
and typed frontend request/response guard expose the same `STREAMING_TBR`
contract. Focused Release tests cover linear
replay, missing tokens, silent prefixes, backwards selection, reset, and
termination diagnostics.

## Segment-filter evidence

The native segment surface exposes prefix and suffix cuts with first/last
anchor selection and strict/non-strict boundary behavior, plus between
extraction over activity sets. Complete subtraces retain event and trace
attributes, reuse a shared boundary for repeated equal endpoints, and receive
deterministic subcase identifiers with a configurable separator. Empty cuts
are preserved when a strict boundary is at the edge; incomplete between
segments are discarded. Focused tests cover missing anchors, first/last
selection, strict boundaries, repeated endpoints, activity lists, attributes,
custom identifiers, malformed options, strict request parsing, CLI flags,
local-server application, and frontend controls/serialization.

## OCEL filtering evidence

The native OCEL filtering surface now supports positive and negative event
attribute, object attribute, object-type, object-id, and event-id selection,
inclusive timestamp windows, activity-to-object-type matching, per-event
object-type count thresholds, first/last lifecycle events per object type,
activity-connected relations, recursive object expansion, and connected-
component object/length/type/activity filters. Event, object, and relation
selections propagate deterministically to the related tables, retaining only
valid links, object changes, and object-to-object relations; empty selections
clear the dependent tables according to the selected propagation direction.
The standalone `minera_ocel` command and multipart `POST /api/ocel/filter`
expose every OCEL filtering variant with strict bounded arguments. The typed
frontend `requestOcelFilter` adapter serializes the same options and validates
the recursive OCEL envelope. Focused tests cover typed scalar attributes,
duplicate relation counts, orphan removal, inclusive boundaries, lifecycle
endpoints, recursive expansion, singleton components, component predicates,
empty maps, malformed bounds, nested delimiter round-trips, escaped
backslashes, and integer overflow rejection. The shared native query codec is
also used by the object-binding playout parser, so object types and ids may
contain the grammar delimiters without being split at the wrong layer.

## Correlation-mining parity evidence

Correlation discovery exposes CLASSIC, CLASSIC_SPLIT, and TRACE_BASED as
independent variants. CLASSIC discards case boundaries, orders interval events
by start and completion time, computes strict precede/succeed probabilities,
and supports both the pinned bidirectional greedy duration estimate and exact
minimum-weight full bipartite matching. The exact matcher has no artificial
population cap. Equal completion/start boundaries are eligible for exact
duration matching but remain excluded from the strict precedence probability.

CLASSIC_SPLIT partitions the native flattened input order before sorting each
chunk. It averages precedence matrices across every chunk, including zero
entries for absent activity pairs, and takes the elementwise maximum duration.
TRACE_BASED instead preserves case boundaries, uses only traces containing both
activities in each probability denominator, and aggregates the two greedy
matching directions over trace-local samples. The trace variant intentionally
ignores the exact-matching option, as does the pinned variation.

All variants solve the same balanced activity-occurrence transportation
problem. Minera uses an integral minimum-cost solver and a deterministic
lexicographic tie normalization; ties may therefore choose a different but
equally optimal DFG from an LP backend while preserving every activity's
incoming and outgoing marginals and the same objective. Frequency edges retain
their raw correlation duration, including zero-duration forced reverse and
self edges.

The configurable interval-start attribute, exact-match toggle, and split
sample size are available through the native API, strict request parser, CLI,
local server, deterministic JSON, and frontend. Missing or malformed configured
starts and malformed completion timestamps fail explicitly. The native
TRACE_BASED path honors a configured start key; this fixes the pinned EventLog
wrapper's dropped-parameter defect while retaining the intended algorithm.
Dataframe and Polars paths are adapters to these same semantics.

Focused regressions cover canonical classic/trace/split fixtures, raw split
chunk order, forced reverse and self edges, one-sided greedy matching, equal
interval boundaries, exact matching above 25 events, custom interval keys,
strict timestamp errors, empty and single-activity logs, complete diagnostic
matrices, deterministic ties, and balanced transportation marginals.

## Performance-spectrum and batch parity evidence

Performance spectra expose CONNECTED, DISCONNECTED, DATAFRAME, LOG,
DATAFRAME_DISCONNECTED, and LOG_DISCONNECTED as distinct selectable variants.
The log/dataframe names select the corresponding native projection while
sharing the deterministic event-log implementation. All variants require at
least two requested activities, strictly validate selected timestamps,
optionally preserve input order, and otherwise use stable timestamp ordering.
Connected discovery projects each case onto the requested activities and emits
every overlapping full-pattern window. Disconnected discovery emits labeled
contiguous subpatterns longest-first, removing matched events except the
endpoint before considering shorter subpatterns so a full match is not
rediscovered as all of its subsets.

Activity, timestamp, and case-id attributes are configurable. Built-in event
fields and generic event/trace attributes are resolved consistently by the
native API, request parser, CLI, and frontend controls; the selected attribute
names are retained in JSON metadata.

Observations are globally stable-sorted by their first instant. The default
sample size is 10,000 and uses the pinned deterministic midpoint selector,
including forced first and last observations; the result can therefore contain
one or two more observations than the requested sample size. The pinned
EventLog disconnected adapter has a generator-versus-tuple membership defect
that limits it to pairs. Minera implements the intended dataframe/lazyframe
longest-subpattern semantics and documents that repair rather than reproducing
the wrapper defect. Columnar and lazyframe implementations otherwise remain
input adapters to the native behavior.

Batch discovery exposes LOG, PANDAS, and POLARS selectors over the same
deterministic interval core. Each variant groups events by activity and
resource, using completion time
as the default start or a configured interval-start attribute. It first merges
strictly overlapping intervals transitively, then merges non-overlapping
intervals whose gap is at most the configured distance. Groups smaller than
the minimum batch size are removed. Classification priority is simultaneous,
batching on start, batching on end, sequential, then concurrent. The reference
group count is the number of detected batches; Minera additionally reports the
number of member events and retains trace/event identities so duplicate
timestamp/case tuples are not collapsed.

Minimum self-distance discovery exposes LOG, PANDAS, and POLARS selectors
through the same deterministic witness calculation. Configurable activity and
case-id columns are resolved from native fields or event/trace attributes;
interleaved case values are grouped by the selected case column, while witness
events remain limited to that case. The selector and columns are carried
through strict request parsing, CLI, local server, JSON metadata, and the
frontend controls; all three adapters preserve the activity, distance, and
witness contract.

Spectrum variant, sampling, sorting, selected activity/timestamp/case-id
attributes, batch variant/enablement, merge distance, minimum size, and start
attribute are available through strict request parsing, CLI, local server,
deterministic prevalidated JSON, and frontend controls. Batch JSON retains the
selected variant name, while footprint JSON keeps separate dataframe and
lazyframe discovery keys for adapter callers.
Disconnected observations retain their per-point labels and explicit requested
pattern offset in the JSON/UI, including when activity labels repeat. Batch
JSON/UI exposes all five categories, group batch/event counts, resources,
ranges, and case evidence.

Focused regressions cover stable sorting and its disable switch, overlapping
connected windows, full and partial disconnected matches, loops, non-adjacent
rejection, longest-first suppression, exact midpoint/extreme sampling, all five
batch types and all three batch selectors, strict-overlap/touching/gap/transitive
merging, custom starts, minimum size, empty resources, duplicate event identities, malformed
timestamps/options, JSON rejection before output, request boundaries, and
frontend rendering.

## Footprints parity evidence

Footprint discovery covers the entire event log, trace-by-trace log behavior,
raw DFGs, Petri reachability graphs, process trees, and POWL. The Petri variant
retains transition identity until relation classification, follows silent
routing when determining eventually enabled activities, and rejects provably
unbounded reachability instead of returning a truncated model. Process-tree
and POWL bottom-up discovery preserve skippability, start/end activities,
always-happening activities, minimum length, the process-tree maximum length
without loops, optional-path bridging, and partial-order concurrency.

Conformance exposes LOG_MODEL in strict and non-strict modes, LOG_EXTENSIVE,
and TRACE_EXTENSIVE with the always-executed-activity toggle. Precision,
log fitness, and percentage-fit metrics use the same footprint artifacts.
Focused regressions cover empty and repeated traces, zero-weight DFG edges,
singleton DFGs, sequence/XOR/parallel/loop models, silent prefixes, duplicate
Petri labels, optional POWL boundaries, transitive partial orders, weighted
fitness, missing optional model fields, unbounded visible and silent growth,
and duplicate case identifiers.

All six discovery variants, all three conformance variants, model selection,
strictness, and the always-executed toggle are available through strict request
parsing, CLI, local server, deterministic prevalidated JSON, and the frontend.
The dataframe and Polars lazyframe variants are classified as adapters over
these native semantics.

## Declare parity evidence

Classic Declare discovery supports all 18 pinned templates, considered
activities, allowed-template prerequisite behavior, explicit or automatic
support/confidence thresholds, and the auto-selection multiplier. It preserves
directional activation, vacuity, identical-trace weighting, zero-filled
inactive rules, compound and negative constraints, tuple-key identity, and
the reference's support/confidence counts. Classic conformance provides the
pinned deviation order, totals, fitness, fit status, unknown-family behavior,
and one result per input trace.

The batch model and conformance diagnostics are exposed through the native API,
strict request fields, CLI, local server, deterministic numeric-ID JSON, and
the frontend. Tests cover every template family, positive and violating traces,
empty and unusual activity labels, duplicate cases, explicit-empty filters,
threshold edge values, malformed output validation, and duplicate rule keys.

The separate streaming AUTOMATA implementation is stateful, mutex-serialized,
and intentionally prefix-only: pending obligations are not finalized at the
end of a case. It implements all 18 immediate automata, sticky first-violation
counting, undefined-case and timestamp fallbacks, inert unknown templates, and
lossless per-constraint snapshots. Declare classic playout independently
generates random prefix-safe traces with the pinned trace-count and length
parameters; a seed is provided as a deterministic Minera extension. Declare
playout is selectable from the CLI and local server, and its XES download is
available from the Declare frontend view. Tests cover multi-event automata
behavior, concurrent snapshots, malformed arity, candidate-state isolation,
early stopping, pending obligations, empty models, length bounds, and seeded
reproducibility.

## Heuristics Miner parity evidence

Classic discovery uses PM's `0.5` dependency, `0.65` AND, `0.05` DFG
pre-cleaning, and `0.5` length-two-loop defaults. It preserves raw trace order,
raw lifecycle events, activity/start/end counts, direct and window-two DFGs,
frequency triples, optional per-case DFG deduplication, protected most-common
paths, and all scalar/raw/all performance aggregations. Pre-cleaning uses the
maximum incident-path rule and strict removal boundary; dependency, activity,
path, AND, and loop-two thresholds use the pinned inclusive comparisons.

Length-two-loop rescue retains the pinned pre-existing-node quirk, adds both
directions with dependency zero, and does not recompute AND bindings afterward.
Classic and PLUSPLUS both use deterministic overlapping maximal AND cliques.
Classic node retention distinguishes isolated activities from the all-filtered
fallback, and the converted workflow net contains exactly the heuristic nodes.
A public classic DFG-input entry point supports explicit or inferred
activities, occurrences, boundaries, window-two paths, triples, and performance
samples; PLUSPLUS rejects DFG-only input as in the reference.

The PLUSPLUS variation independently implements the pinned interval abstraction.
Lifecycle events are converted through case-insensitive start/complete matching
with FIFO queues keyed by activity and lifecycle instance; unmatched completes
become zero-duration events and unmatched starts are omitted. A log whose first
event already carries the configured start timestamp is treated as interval
input globally. Eventual-follow and concurrency calculations use stable
start-time ordering, while pre-interval performance edges preserve the input
trace order.

Discovery uses the first non-overlapping following interval, inclusive overlap
counts (so touching intervals can be both eventual-follow and concurrent),
activity/path occurrence filtering, direct-adjacency mean waits, mean sojourn
times, and the exact Miner++ dependency and split/join formulas. Dependency and
AND thresholds are strict and the default dependency is `0.5`. AND bindings are
deterministic overlapping maximal cliques rather than transitive components.
Frequency and performance decorations, filtered and raw abstraction statistics,
variant selection, minimum activity/path counts, and configurable start-time
and lifecycle-instance attributes are exposed in the native model, JSON,
request parser, CLI, local server, and frontend.

Focused Release tests cover classic defaults and equality boundaries, exact
noise removal/exemptions, filters, self-loops, per-case deduplication,
length-two rescue and false cases, lifecycle and pre-interval modes, invalid
and missing timestamps, first-following and boundary-touch concurrency, the
concurrency denominator, all performance summaries, raw statistics, absence
of default connect-all repair, DFG inference/overrides, and overlapping maximal
cliques. Full native tests, frontend tests/build, and headless CLI JSON smokes
validate the integrated path.

## Petri-net alignment parity evidence

The six explicit Petri-net alignment variants are implemented as clean-room
searches over one compact, isomorphic synchronous-product state:
`(marking, trace position)`. State-equation A* adds a full remaining-trace LP
lower bound (place-balance rows plus one row per remaining event, with model,
log, and synchronous columns); its solve count is reported in diagnostics.
Dijkstra without heuristics traverses that product uniformly. The less-memory
variants apply Pareto dominance over marking, trace position, and cost while
preserving the optimum; focused tests compare their costs with the full
frontier under fractional custom costs.

Cost semantics are selectable rather than silently changed: the legacy profile
keeps unit visible log/model deviations and free silent routing, while the
The reference profile charges 10,000 for visible log/model moves, 1 for silent moves,
and 0 for synchronous moves.
Per-activity, per-position, and per-transition overrides are supported, as are
API callbacks for trace, model, and synchronous moves. Callback values are
resolved once for each concrete trace/model pair, reject non-finite or negative
costs, and remain stable even for repeated labels; tests cover overrides,
fractional costs, callback statefulness, and invalid values.

Discounted A* honors both the `exponent` and request-layer `theta` spelling
(greater than one). Its synchronous-Dijkstra scheduling toggle is an adapter
over the same compact product representation: only successor scheduling differs
from the explicit order, so both schedules retain equal optima. The native API,
request parser, CLI, and frontend expose these controls and preserve the
legacy state-equation/unit-cost defaults when no explicit options are supplied.
Like the pinned 2.7.22 reference implementation, discounted search closes a state
by `(marking, trace position)` rather than treating the same product state at
every path depth as distinct. Its optimum is therefore over simple
product-state paths. This avoids an ill-defined cyclic objective in which
repeating a positive-cost loop can push a later move toward an unattained
discounted infimum; it also makes zero- and positive-cost cycles terminate
deterministically within the configured limits.

Generator Dijkstra variants retain deduplicated equal-cost optimal paths, with
`max_alignments` as an explicit cap. No-heuristics and less-memory generators
agree on the optimum, tied alternatives are retained up to the cap, and cyclic
models cannot produce more than the requested collection. State, whole-log,
and per-trace time limits are enforced; a reachable search that hits a limit
reports `computed=false, exhausted=true` rather than a fabricated optimum.
Diagnostics include visited/queued states, traversed arcs, closed-set size,
visited markings, and LP solves.

Silent (`tau`) model moves remain explicit routing moves in diagnostics and
alignments, while weighted input/output arc multiplicity is preserved when
testing enabledness and firing. A genuinely unreachable final marking is
reported as unsound (`exhausted=false`), distinct from a bounded or timed-out
search. Focused `src/backend/tests/test_alignment_variants.cpp` regressions
cover all six
variants, reference/legacy cost profiles, custom costs and callbacks, full-product
LP solving, less-memory dominance, discounted exponent and scheduling parity,
generator optima and caps, timeout/state diagnostics, tau moves, weighted arcs,
and invalid-cost handling; request, CLI, and frontend tests cover the exposed
adapters.

## Process-tree alignment parity evidence

The four selectable process-tree variants are implemented in the native
`include/minera/process_tree_alignment.hpp` / `src/backend/process_tree_alignment.cpp`
pair and exposed by the strict request parser, `minera_export`, the local
server, and the frontend's **Other alignments** panel. They are deterministic,
collision-free clean-room implementations with dependency-free exact residual
fallbacks. This preserves the reference implementation's behavioral questions without copying its
Python implementation; `MILP` does not claim to invoke literal PuLP.

* `SEARCH_GRAPH_PT` is the default residual-expression Dijkstra search, using
  canonical interned residuals for stable, collision-free state identity.
* `DYNAMIC_PROGRAMMING` recursively aligns subtrees with memoized subtree/trace
  pairs; loops use the exact residual fallback. Optional global caching has a
  positive `batch_size` and `clear_cache` controls periodic eviction.
* `MILP` is an exact integer shortest-path residual A*/branch-and-bound
  equivalent to the pinned unit-cost MILP objective. It is dependency-free and
  performs no external LP solves (`lp_solved` therefore remains zero); native
  residual-search counters are reported instead. Its formulation validates
  loops as binary; non-binary loops are rejected.
* `APPROXIMATED_ORIGINAL` uses exact residual alignment below configurable
  trace-length/tree-height thresholds, then recursively decomposes sequence,
  XOR, and parallel nodes. Loops always use the exact residual fallback, whose
  count is reported in the diagnostic message.

`ProcessTreeAlignmentOptions` defaults to `SEARCH_GRAPH_PT`, `max_states = 0`,
per-trace and whole-log time limits of `0`, `use_global_cache = false`,
`batch_size = 1000`, `clear_cache = true`, approximation thresholds
`max_trace_length = 1` and `max_process_tree_height = 1`, and approximation
trace/per-trace time limits of `0`. A zero state or time bound means unlimited;
native validation still rejects negative/non-finite values, a zero DP batch,
and unsupported cache settings on non-recursive variants. Request and CLI time
fields are milliseconds (`pt_alignment_max_time_ms`,
`pt_alignment_max_total_time_ms`, `pt_alignment_max_trace_time_ms`, and
`pt_alignment_max_per_trace_time_ms`). CLI selection starts with
`--align-process-tree` and `--pt-alignment-variant` plus the corresponding
`--pt-alignment-*` bounds, cache, and threshold flags. The deferred HTTP entry
point is `POST /api/align-process-tree`.

The JSON result has `family: "PROCESS_TREE"`, the selected `variant`, aggregate
`total_cost`/`fitness`, per-trace `trace`, `cost`, `fitness`, `moves` (`sync`,
`log`, or `model` with log/model/activity labels), and diagnostics for visited,
queued, and traversed states, native residual-search counters, aligned traces,
and a message (`lp_solved` is zero because no external LP solver is used).
`computed`, `completed`, `timed_out`, and `exhausted` are explicit at
both result and trace level: a state/time bound yields an incomplete result and
never a fabricated optimum. As in the reference implementation, an empty fitness denominator reports
`0.0` (including an empty trace/log against a silent tree). Native n-ary loop
redo/exit children are normalized to an XOR for search/DP/approximation; MILP's
binary validation is deliberate and tested. The recursive variants also use
deterministic duplicate-label event assignment and retain model moves so the
rendered explanation sums to the reported cost.

Focused validation is in `src/backend/tests/test_process_tree_alignment.cpp`,
with request mapping/rejection coverage in `src/backend/tests/test_request_params.cpp`,
JSON checks in `src/backend/tests/test_core.cpp`, and frontend query/result
checks in `src/frontend/tests/app-logic.test.ts`. Tests cover all four variants, exact
sequence/XOR/parallel/loop behavior, repeated traces and DP cache batches,
approximation thresholds and loop fallback diagnostics, timeout/state
completion flags, empty-denominator fitness, n-ary normalization, and MILP's
binary-loop rejection.

## DFG classic and edit-distance alignment parity evidence

DFG alignment implements the pinned `CLASSIC` variant over a complete
start/DFG/end frequency model. Start and end activity maps are included even
when a supplied frequency is zero; edge frequencies are informational rather
than hidden enablement flags. The model is validated as connected from its
starts to its ends, and disconnected graphs or graphs without a start-to-end
path fail explicitly. Default alignment costs are zero for synchronous moves
and 10,000 for visible model/log moves. Per-activity synchronous, model, log,
and internal-log costs are configurable: the reported cost can differ from the
search's internal cost (for example, unknown labels may have a free internal
move), and both are serialized.

Whole-log alignment caches exact activity variants in an ordered map, reports
`unique_variants`, and restores results to the original trace order. Trace
results include fitness, best/worst cost, visited and closed-state diagnostics;
visited-state and elapsed-time bounds are enforced and report a bounded-search
failure instead of fabricating an optimum. The log projection preserves event,
trace, and scalar log attributes, replaces reserved projection metadata, keeps
synchronous timestamps/resources/lifecycle values, and marks
`@@is_conforming`. The metrics projection counts synchronous/model/log activity
moves and DFG edge moves, including a final log-only move (the corrected
last-move accounting).

`EDIT_DISTANCE` computes Levenshtein distance for every source variant and picks
the minimum (or maximum when `perform_anti_alignment` is enabled); duplicate
target variants retain their first occurrence, with deterministic lexical
tie-breaking. Reconstruction follows `difflib.SequenceMatcher` matching-block
semantics, including the default autojunk suppression for popular symbols in
large targets, but uses ordered maps for deterministic clean-room behavior (the
pinned hash-order implementation could choose a different tied path). A
substitution is represented as two moves (one log-only and one model-only), so
deviation cost, fitness, and the reference best/worst-cost denominator (`bwc`) are
explicit and auditable. Empty source logs return no alignments; an empty target
log and non-positive deviation cost are rejected.

Edit-distance log projection uses each trace's independently selected target
variant, including heterogeneous selections. It preserves source trace
metadata, records `@@original_trace` and `@@aligned_variant`, keeps timestamps
from synchronous source events, timestamps inserted model events at the current
source time, omits source-only moves, and marks each projected event's
`@@is_conforming` value. Scalar EventLog log attributes (including these
projection markers) and their XES scalar types round-trip through the XES
importer/exporter.

Concept-drift adapters expose deterministic `CONCEPT_DRIFT` JSON with the
selected `bose` variant from the CLI
(`--concept-drift`, `--drift-*`) and cached-log server endpoint
`POST /api/concept-drift`. Strict controls cover sub-log/window sizes,
permutation count, threshold, change-point cap, seed, and optional timestamps;
the result includes activity features, raw and cumulative sub-log summaries,
change points, and timestamp arrays.

## Organizational role discovery parity

The bounded role endpoint (`--roles` and `POST /api/roles`) starts with one
role per activity and counts only events having both a non-empty activity and
resource. Role similarity is the overlap of normalized resource-count vectors
(sum of component-wise minima divided by sum of maxima). After a merge, each
source role contributes one presence count per resource rather than carrying
raw event totals. The highest-scoring
pair is merged only when its score is strictly greater than
`roles_threshold` (default `0.65`); `roles_activity_attribute` and
`roles_resource_attribute` select standard or custom event fields; ties and
final role ordering are lexical
and deterministic. Final roles are sorted by descending activity/resource
cardinality before the activity-name tie break. `roles_max_resources` and
`roles_max_activities` are strict safety bounds. Empty activities and
blank-resource events are excluded, while
their traces remain represented by aggregate diagnostics. The standalone JSON
family is `ROLE_DISCOVERY` with `roles[].activities` and
`roles[].resources[{name,count}]`.
