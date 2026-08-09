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
| Analysis | marking equation (classic), extended marking equation (classic), workflow-net check, Woflan soundness with reachability/coverability/invariants | **Partial** — native marking/extended-equation/workflow/soundness cores plus strict CLI/server/frontend adapters; full differential edge-contract review remains |
| Anonymization | PRIPEL; trace-variant query Laplace and SaCoFa | **Partial** — bounded PRIPEL-style release plus both trace-variant query variants are exposed through native, JSON, CLI, local-server, and frontend adapters; deterministic seeded mode and opt-in OS-backed secure randomness are available, while differential edge-contract review remains |
| Clustering | profile clustering; trace-attribute-driven activity/successor/log-slice/similarity distances | **Partial** — deterministic profile clustering now has a bounded native API with activity/succession and explicit trace/event attribute representations; trace-attribute groups with DMM/average Levenshtein, vector, and DFG distances retain bounded JSON/server/CLI/frontend adapters; broader differential review remains |
| Comparison | Petri-net element-usage comparison | **Partial** — native deterministic place/transition/arc usage deltas are available through JSON, CLI, local server, and frontend adapters; broader differential semantics remain |
| Concept drift | relation-type feature extraction and permutation-window drift detection | **Partial** — native extraction, seeded permutation windows, timestamp diagnostics, strict options, JSON/CLI/server/frontend adapters are available; differential edge-contract review and alternate data-frame adapters remain |
| Conformance: Petri alignments | state-equation A*, Dijkstra no heuristics, Dijkstra less memory, discounted A*, generator Dijkstra no heuristics, generator Dijkstra less memory | **Complete (core)** — all six variants are explicitly selectable; the backward-compatible default remains the legacy unit-cost state-equation A* behavior |
| Conformance: other alignments | decomposed recomposition, DFG classic, edit distance, process-tree search graph/dynamic programming/MILP/approximated original | **Complete** — `RECOMPOS_MAXIMAL`, DFG classic, edit distance (`EDIT_DISTANCE`), and all four process-tree variants are native selectable APIs with JSON, CLI, server, and frontend adapters |
| Conformance: replay | forward token replay and backwards token replay | **Complete (core)** — forward replay remains the default; the deprecated backwards variation, reverse silent routing, weak firing, duplicate-label behavior, and immediate-stop option are selectable |
| Conformance: relational | footprints log-model/log-extensive/trace-extensive; log skeleton; Declare | **Complete (core)** — all three footprint checks, classic log-skeleton conformance, and classic Declare conformance are available with per-trace diagnostics |
| Conformance: temporal | temporal profile log/dataframe adapters, zeta threshold, business-hour slots | **Complete (core)** |
| Conformance: advanced | Petri-net anti-alignments and multi-alignments (discounted A*) | **Complete** — both pinned `VERSION_DISCOUNTED_A_STAR` algorithms, objectives, precision/distance diagnostics, marking limits, arbitrary markings, and JSON/CLI/server/frontend adapters are implemented |
| Conformance: object-centric | ETOT, OCDFG and OTG graph comparison | **Partial** — deterministic ETOT, OCDFG, and OTG graph comparisons with strict thresholds, weighted fitness, diagnostics, native APIs, headless OCEL CLI output, and multipart local-server/frontend API adapters; broader OCEL schema coverage remains |
| Discovery: Alpha | classic and Alpha+ | **Complete** |
| Discovery: Inductive | IM, IMf, IMd | **Complete** — `log`, explicit `imf`, and graph-recursive `dfg` selectors share the validated native/request/CLI/frontend surface |
| Discovery: Heuristics | classic and Heuristics Miner++ | **Complete (core)** — both variants, exact dependency/AND/loop behavior, DFG pre-cleaning, filtering, performance aggregation, interval concurrency, and classic DFG input are available |
| Discovery: optimization | ILP classic and Genetic classic | **Partial** — bounded deterministic ILP and Genetic classic implementations are available through native, JSON, CLI, local-server, and frontend adapters; differential edge-contract review remains |
| Discovery: declarative | Declare classic and log skeleton classic | **Complete (core)** |
| Discovery: graph/statistical | DFG frequency/performance/frequency triples/case attributes/clean; causal alpha/heuristic; correlation classic/classic-split/trace-based; minimum self-distance | **Complete (core)** — all statistical DFG aliases, both causal variants, all three correlation-miner variants, eventually-follows, and minimum self-distance with witnesses are available |
| Discovery: footprints | log entire/trace-by-trace, DFG, Petri reachability, process-tree bottom-up, POWL bottom-up | **Complete (core)** — dataframe and lazyframe forms are adapters over the same native semantics |
| Discovery: structured | transition system view-based; POWL brute-force/dynamic-clustering/frequency/maximal/tree cuts | **Complete (core)** — transition systems support sequence/set/multiset views, forward/backward direction, arbitrary windows including zero, and optional occurrence data; POWL exposes TREE, BRUTE_FORCE, MAXIMAL, DYNAMIC_CLUSTERING, and DYNAMIC_CLUSTERING_PLAIN/frequency behavior with filtering and order-frequency settings |
| Discovery: temporal/behavioral | temporal profile; performance spectrum connected/disconnected; batches | **Complete (core)** — temporal profiles, both performance-spectrum variants, and all five batch classifications are available; dataframe/lazyframe paths are adapters |
| Discovery: object-centric | ETOT, timestamp interleavings, link analysis, OCDFG, OCPN annotated/unannotated, OTG, SAW nets | **Partial** — classic ETOT, strict timestamp interleavings over related event logs, deterministic link analysis, OCDFG metrics, annotated OCPN per-object-type nets (IM/IMd), OTG discovery, and classic stochastic arc-weight nets are available through native/headless adapters; broader object-centric schema and visualization coverage remains |
| Evaluation | replay fitness token/alignment; precision token/alignment/automaton; generalization token; simplicity arc-degree/Cardoso/cyclomatic; earth-mover distance | **Complete** — dynamic soundness-based fitness/precision dispatch, exact token and prefix-stop alignment precision, aligned-automaton precision (including task identities), transition-frequency generalization, all three simplicity metrics, and exact optimal-transport distance; strict request parsing, JSON, CLI, local-server, and frontend adapters cover every setting and externally supplied comparison logs |
| Label splitting | contextual | **Partial** — deterministic native contextual splitting with strict options, JSON/CLI/local-server/frontend adapters, and focused tests; a differential modularity edge-contract review remains |
| Merging | case relations | **Complete (core)** — the single case-relations variation merges related incoming cases with deterministic timestamp ordering, strict relation validation, and native/JSON/CLI/server/frontend entry points |
| Organizational mining | local diagnostics, network analysis frequency/performance, resource profiles, roles, SNA metrics | **Partial** — deterministic local diagnostics (group-attribute and explicit role/clustering modes), frequency/performance network analysis, role discovery, all eleven resource-profile metrics, and four SNA metrics (handover, working-together, subcontracting, joint-activities) now have native/JSON/CLI/server/frontend adapters; broader differential review remains |
| Reduction | process-tree transition-reduction based | **Complete** — `TREE_TR_BASED` trace reduction, optional-subtree replacement, reference-compatible folding, custom activity keys, and JSON/CLI/server/frontend adapters are available |
| Simulation | Petri basic/stochastic/extensive; process-tree basic/extensive/top-bottom; DFG classic/performance; Declare; object-centric Petri/causal nets; Monte Carlo; random tree and tree+log generation | **Partial** — Petri basic/stochastic/extensive, Declare classic, DFG classic/performance, all three native process-tree playout variants, both seeded random-tree strategies (BASIC and bounded PT-and-log generation), and a bounded virtual-time Monte Carlo Petri variant with source-log-fitted activation weights and service means are wired through strict request parsing, CLI, local server, JSON, and frontend controls; bounded OCPN and causal-net extensive playout projections are now available through native, CLI, multipart local-server, and frontend adapters; richer marker-binding semantics and interval-tree diagnostics remain |
| Transformation | event/trace/temporal features; next-activity/next-time/remaining-time targets; interval tree; trie; embeddings; OCEL descriptions/features/splitting | **Partial** — event/trace/temporal feature tables are native and exposed through `minera_export --log-to-features` and the local `/api/transform-features` endpoint; all three target variants, open-path interval extraction, the traditional trie, contextual label splitting, deterministic case/event embeddings with cosine top-k ranking, deterministic OCEL text/tuple descriptions, all three bounded OCEL event/object/events-object feature tables, and both connected-components and ancestors/descendants OCEL splits are native or headless CLI/server/frontend API-wired; model-backed embedding parity and broader OCEL feature parameters remain |

Process-tree simulation evidence: `BASIC_PLAYOUT`, `EXTENSIVE`, and
`TOPBOTTOM` are available through strict request parsing, the CLI
(`--playout-miner process_tree`), local-server `/api/playout`, cached-tree XES,
and frontend controls. This closes only the process-tree variants; the broader
simulation row remains partial because other simulation families are pending.

Random-tree generation evidence: the native generator now exposes both the
legacy `BASIC` strategy and a seeded, bounded `PTANDLOGGENERATOR` strategy.
The latter validates triangular activity-count bounds, supported operator
weights, silent/duplicate probabilities, and its single-tree contract; strict request
parsing, standalone CLI JSON, local `/api/random-tree`, frontend query
serialization, and recursive response guards preserve the selected variant.

Monte Carlo simulation evidence: the native `monte_carlo` core implements the
`PETRI_SEMAPH_FIFO` virtual-time scheduler over workflow nets, seeded transition
selection, deterministic, exponential, constant-zero, immediate, normal, uniform,
lognormal, and gamma service times, bounded case/step
budgets, shared output-place resource pools, completion/deadlock diagnostics,
and deterministic EventLog output. `monte_carlo_*` request fields are strictly
validated, the export CLI can select `--playout-miner monte_carlo`, the local
server exposes both `/api/playout?mode=monte_carlo` (XES) and `/api/monte-carlo`
(JSON), and the frontend offers typed query/result guards and controls. Fitted
activation weights and timestamp-derived deterministic service means are applied
when the source log overload is used.

Object-centric Petri-net simulation evidence: the bounded native adapter
enumerates per-object-type workflow-net traces, combines object choices into
deterministic OCEL event/object/relation records, supports explicit object ids,
per-trace object suffixing, and strict trace/state/firing/timestamp bounds. The
standalone `minera_ocel --ocpn-playout` command, multipart local
`POST /api/ocel/playout`, and typed frontend request/response guard expose the
same `OCEL_PLAYOUT` envelope; the endpoint also selects the log/DFG OCPN
discovery path and validates its thresholds. Object-centric causal-net playout
uses the same deterministic workflow projection through `OcCausalNet`, with a
separate `OC_CAUSAL_NET_PLAYOUT` envelope, `--oc-causal-net` CLI selector, and
`model=causal_net` server/frontend selector. Richer marker-group binding
semantics and interval-tree diagnostics remain open.

Annotated object-centric Petri-net evidence: `discover_ocpn` now retains the
deterministic OCDFG activity, start/end, and directly-followed-event metrics
alongside each per-object-type workflow net. The serialized `annotations`
section includes event/object tuples, event-pair aliases, object-type maps,
performance-disabled diagnostics, and double-arc decisions; the existing
`minera_ocel --discover ocpn`, local OCEL discovery route, and frontend result
guard continue to select the same `ocpn` variant.

Link-analysis evidence: the classic relation join is available as
`discover_link_analysis` over event logs and supports first-class or generic
event/trace columns, timestamp-aware stable ordering, forward-only filtering,
first-occurrence reduction, deterministic transitive propagation, and a strict
link bound. Its standalone JSON, `minera_export --link-analysis`, cached local
`POST /api/link-analysis`, and frontend API/query guard all expose the same
options and canonical event references.

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
features cover activity indicators, timestamps/calendar parts, related-object
counts by type, lifecycle endpoints, attribute counts, custom numeric/string
attributes, and newly observed interactions. Object features cover type
indicators, lifecycle length/duration/endpoints, activity/path counts, custom
attributes, work-in-progress, and interaction degree. Strict feature-count and
relation-id validation is shared by `minera_ocel --features`, multipart local
`POST /api/ocel/features`, and the typed frontend query/response adapter.

OCEL interchange evidence: the native core now parses and writes deterministic
extended-table CSV (with an optional object table) and bounded classic/standard
XML. CSV import validates headers, object-list cells, timestamps, ids, and
cross-type references; XML import/export preserves scalar attributes,
qualifiers, object-to-object links, object changes, globals, and deterministic
ordering where the selected format carries them. `minera_ocel` detects `.csv`
Classic XML accepts both attribute-form records and keyed child elements,
including list-based event object maps; standard XML accepts both relationship
container spellings. `minera_ocel` detects `.csv` and `.xml`/`.xmlocel`
inputs and can emit classic or standard XML.

Embedding evidence: `to_embeddings` exposes both case-level and event-level
variants through a deterministic bounded token-hash encoder. The native API
returns fixed-length normalized vectors and deterministic cosine top-k matches;
`minera_export --embeddings`, cached-session `POST /api/embeddings`, and the
typed frontend request/response guards share the same JSON contract. Event
filtering can retain only selected events or complete cases. Model-backed
embedding loading remains outside the native core; the encoder label and
vector dimension are explicit request settings.

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
--playout-miner dfg`, local-server `/api/playout?miner=dfg`, and frontend XES
controls. The native API preserves probability and estimated-occurrence trace
attributes; map-shaped variant return remains represented through those
attributes while the transport remains XES.

Petri stochastic simulation evidence: the native `stochastic_playout` API uses
transition-index weights, validates finite non-negative values and weighted
input arcs, samples all-zero enabled sets uniformly, preserves silent routing,
and stops at the exact workflow final marking unless configured otherwise.
The seeded options are available through `--playout-variant stochastic`, the
local server's `mode=stochastic` route, and the frontend controls, with strict
trace/step/event-budget limits at adapter boundaries. The supported native
contract takes an explicit transition-weight vector; source-log-derived
activation estimates and object-centric stochastic simulators remain outside
this traditional-log vertical.

## Data, object, conversion, and integration checklist

| Surface | Required variations | Minera status |
|---|---|---|
| Event data | Event, trace, event stream, typed attributes, lifecycle, dataframe/lazyframe adapters | **Partial** — native EventLog with typed scalar attributes; no stream/columnar adapter |
| Object-centric data | OCEL events/objects/relations/o2o/e2e/object changes and OCEL 1/2 schema | **Partial** — bounded events/objects/event-object relations, o2o links, object changes, scalar globals, and classic/standard JSON parsing are present; e2e, richer value types, and full schema coverage remain |
| Models | Petri nets incl reset/inhibitor, markings, process trees, BPMN, DFG, transition systems, heuristics nets, POWL, declarative models, OCPN/OC causal nets, trie, stochastic nets | **Partial** — ordinary Petri, process tree, BPMN, DFG, heuristics net, transition system, POWL, classic log-skeleton, and Declare objects; reset/inhibitor, object-centric, trie, and stochastic families remain |
| Conversions | log/stream/table; tree to Petri/BPMN/POWL; BPMN/DFG/heuristics/trie/WF-net conversions; OCEL graph/features | **Partial** — tree/heuristics/POWL/BPMN, both classic DFG-to-Petri constructions, and trie-to-Petri conversion to workflow nets, tree to POWL, and tree/net to BPMN; remaining object-centric and adapter conversions are absent |
| Traditional I/O | XES parser variants, PNML, PTML, BPMN, DFG read/write | **Partial** — XES, PNML, PTML, deterministic classic DFG text read/write, and DFG input/output through CLI/server/frontend adapters are present; broader interchange variants remain |
| Object-centric I/O | OCEL CSV, JSON, XML, SQLite and OCEL 2 JSON/XML/SQLite | **Partial** — classic and standard JSON, deterministic extended-table CSV (with an optional object table), and bounded classic/standard XML round-trip through the native core/CLI; SQLite and the remaining schema-specific adapters remain |
| Filtering | attributes/cases/endpoints/variants/paths/LTL/time/between/rework/prefix/suffix/segments; DFG filters; OCEL filters | **Partial** — broad traditional-log filters, native/CLI/server/frontend adjacent-path performance, prefix/suffix/between segment variants, all four LTL predicates through native/CLI/server/frontend adapters, connected DFG activity/path/dependency/boundary filters, and complete native/CLI/server/frontend OCEL filters; broader filter families remain |
| Statistics | endpoints/attributes/variants, passed time, service/cycle/arrival/overlap/rework, concurrent/eventually-follows, process cube, OCEL statistics | **Partial** — interval-event, case-envelope, and concurrent-activity overlap, typed numeric/categorical/composite process cubes, and OCEL object-type summaries, activity/object associations, and edge metrics with configurable business-hour durations are available through native/CLI/server/frontend adapters; broader statistical aggregation extensions remain |
| Visualization | Petri/DFG/tree/BPMN/heuristics/footprints/SNA/transition system/POWL/declarative/OCEL, decorations and render backends | **Partial** — interactive native UI includes statistical/causal/correlation DFG results, performance spectra, batch diagnostics, recursive POWL, footprint variants/diagnostics, Declare, log-skeleton, converted workflow nets and decorations; several model families and render backends remain |
| Streaming | CSV/XES event/trace streams, streaming DFG, online Declare/footprints/token/temporal conformance, classic/thread-safe/Redis stores | **Partial** — thread-safe Declare AUTOMATA conformance core is present; stream readers, other online algorithms, and persistent stores remain |
| Decision mining | decision-tree extraction, encoded decision tables, data Petri nets | **Partial** — decision trees/rules; no data-Petri-net object |
| Privacy | anonymization algorithms and privacy-preserving query variants | **Partial** — PRIPEL-style release and Laplace/SaCoFa trace-variant queries have native and adapter surfaces, with explicit deterministic or OS-backed secure-random modes; broader privacy contracts remain |
| ML | train/test splits, prefixes, feature tables, prediction targets, OCEL features | **Missing** |
| LLM/querying | abstractions, provider calls, clustering/hypotheses, natural-language log query/filter, database knowledge injection | **Missing / Integration** |
| Connectors | Outlook, Windows events, browsers, GitHub, Camunda, SAP O2C/accounting | **Missing / Integration** |

Overlap evidence: `OverlapOptions` validates finite non-negative epsilon,
supports configurable activity/timestamp attributes, and supports inclusive or strict endpoint semantics. When `overlap=true`, the
native interval-event, case-envelope, and concurrent-activity counters run
after all ordinary, segment, path, and LTL filters; JSON embeds the three
arrays under `overlap`. CLI flags and server/frontend form fields are covered
by `tests/test_request_params.cpp` and the focused overlap/native JSON tests.

Chaotic-activity evidence: the native `discover_chaotic_activities` operation
implements the single entropy-based ranking variant with configurable activity
attribute, smoothing, and bounded input size. It returns deterministic
frequency, entropy, entropy-gain, and combined-score metrics; focused tests
cover repeated runs, custom attributes, empty input, and invalid bounds.

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
local `POST /api/dfg` accepts raw DFG text and returns the canonical document,
and the typed frontend `importDfg` helper exposes the same validation path.

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
and bounded soundness diagnostics. The extended equation keeps trace events in
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
tests cover the current vertical. Karp–Miller-style coverability witnesses,
positive normalized invariant/S-component diagnostics, and their JSON fields are
included; a complete black-box edge matrix remains open before this row can
claim full parity.

## Petri usage comparison evidence

Element-usage comparison replays two logs on a selected discovered Petri net
and emits deterministic place, transition, and arc records with both log
counts, totals, and signed direction. The CLI supports CSV/XES references and
inductive, Alpha, heuristic, and POWL model selection; the local server accepts
the same reference as multipart `reference_log` against its cached session;
the frontend provides a compact upload-and-delta panel. Native replay,
standalone JSON, CLI, server, and frontend response guards are covered; richer
differential semantics remain outside this adapter.

## Concept-drift evidence

The native concept-drift surface extracts deterministic relation-type count
vectors from ordered trace sub-logs and evaluates adjacent windows with a
seeded permutation test. It supports configurable sub-log/window sizes,
permutation counts, p-value threshold, result cap, and optional case-start
timestamps. Strict request parsing, standalone JSON, CLI, cached-session
server, frontend request/response types, and focused native/request tests are
included. The current row remains partial pending differential edge-contract
fixtures and the reference implementation's dataframe/lazyframe adapters.

## Trace-variant query anonymization evidence

The native trace-variant query surface supports both Laplace and SaCoFa
variants. It builds bounded prefix distributions, applies deterministic
seeded non-negative integer noise, prunes by the configured minimum frequency,
and expands released frequencies into timestamp-neutral event traces with
stable case identifiers. SaCoFa additionally filters prefixes by observed
first-event and all-order predecessor/follower relations. The deterministic
seed is an explicit testing/reproducibility extension. Both privacy surfaces
also accept an explicit secure-random switch that draws one key from the host
operating system entropy provider per release call; the effective key is not
returned in diagnostics.

Strict `tvq_*` request parsing, standalone JSON, CLI
(`--trace-variant-query`), cached-session server endpoints, frontend query
serialization, response guards, output caps, secure-random parsing, and focused
Release tests cover
both variants, empty prefix bounds, malformed privacy settings, pruning,
reproducibility, and non-negative release counts. The PRIPEL-style adapter
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
/api/interval-tree`, and frontend query/response validation.

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
server GET, and frontend response validation.

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
closed variants, strict filtering boundaries, and dynamic zero-threshold
filtering. Recursive POWL JSON, request
validation, CLI flags, local-server mining, frontend controls/rendering,
converted-net conformance and decoration, and the PNML/BPMN/decision/playout
selectors are exercised by native tests, frontend tests/build, and headless
CLI/server smoke checks.

## Genetic discovery parity evidence

The bounded classic genetic miner is implemented as a deterministic causal
matrix search with explicit population, generation, crossover, mutation,
elitism, activity, and candidate-state limits. Seeded evolution repairs
disconnected and inverse matrix projections, preserves partition grouping, and
converts compatible places plus silent routing hubs into a replayable workflow
net with canonical initial/final markings. Fitness combines weighted token
replay and fitting-trace coverage; diagnostics report the search bound and
best candidate without fabricating an optimum after exhaustion.

The native result has a standalone JSON contract and strict request parsing,
CLI flags, cached-session server endpoints, and frontend types/query helpers.
Focused tests cover deterministic output, chain fitness, disconnected repair,
parameter rejection, candidate caps, and malformed frontend responses.

## Organizational SNA parity evidence

The organizational SNA vertical exposes the four log variants as a separate
deterministic contract: normalized directed handover and subcontracting
weights, symmetric per-trace working-together weights, and symmetric Pearson
joint-activities weights. Resource names and edge records are sorted for stable
JSON; blank resources are excluded from the resource alphabet while empty or
resource-less traces remain part of trace-count denominators. Handover beta,
subcontracting distance, optional metric normalization, and output safety caps
are strictly validated.

Native tests cover repeated resources, normalized boundaries, symmetric output,
degenerate correlations, truncation, and parser rejection. Standalone JSON,
CLI flags, cached-session server routes, and frontend query/result guards cover
all four selectable variants. Deterministic role discovery is also exposed via
the `ROLE_DISCOVERY` contract and `--roles`/`POST /api/roles`; local diagnostics
and network analysis are documented below.

The organizational network-analysis vertical emits deterministic flat rows
(`NETWORK_ANALYSIS`) by linking each event to the earliest later event with
the same relation key. Defaults use case boundaries, resource nodes,
activity labels, and timestamp ordering; `_out`/`_in` edge references,
custom selectors, frequency/performance output, weekly business-hour slots,
and strict row/value bounds are available through the native API, CLI,
cached-session server route, and typed frontend helper. Broader differential
review of additional organizational variants remains partial.

The resource-profile vertical exposes the eleven scalar metrics through the
`RESOURCE_PROFILES` JSON family, `--resource-profiles` CLI mode, cached
`POST /api/resource-profiles`, and typed frontend query/response helpers.
Metrics use deterministic resource/activity rows, half-open timestamp slots,
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

## Evaluation parity evidence

Model evaluation covers token- and alignment-based replay fitness; token,
alignment escaping-edge, and aligned-automaton precision; token-frequency
generalization; arc-degree, extended Cardoso, and extended cyclomatic
simplicity; and stochastic-language earth-mover distance. Automatic fitness
and precision selection uses workflow-net soundness, while every concrete
variation remains directly selectable. Precision exposes task-identity and
search-state controls; simplicity exposes its arc-degree target, reachability
bound, and explicit initial/final markings. Distance comparison accepts an
external CSV or XES log and independently controls mass normalization and
strict equal-mass validation.

The shared request parser rejects empty, malformed, non-finite, negative, and
out-of-range settings before evaluation. Results have standalone and embedded
JSON forms, active-variation replay details, explicit completion/exhaustion
state, and finite-or-null metrics. The CLI evaluates every built-in Petri model
and an optional imported model; the local server evaluates any cached model and
accepts multipart reference logs. The frontend exposes the complete option set,
discovers models from actual nets as well as embedded results, and guards
on-demand caches/results across analysis changes. Native evaluator, request,
JSON, CLI, hidden-server, frontend-test, and production-build validation cover
the full vertical.

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

## Connected DFG filtering evidence

The native DFG filtering API now exposes deterministic activity-percentage,
path-percentage, dependency-threshold, to-activity, from-activity, and
contain-activity variations. Each filter works on explicit activity, boundary,
and edge frequencies, removes only the requested low-support nodes or paths,
and repairs the graph so retained activities remain reachable from a start and
can reach an end. Focused tests cover boundary preservation, branch pruning,
dependency ties, unknown activities, invalid thresholds, and deterministic
output ordering.

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

## Classic DFG text I/O evidence

The deterministic DFG interchange surface reads and writes the compact classic
text layout: a sorted activity registry, `idxcount` boundary rows, and
`source>targetxcount` edge rows. Counts are positive 64-bit integers, duplicate
or out-of-range rows are rejected, unsafe multi-line/trimmed labels are
rejected, and UTF-8 labels plus stream/path aliases round-trip through the
native `DfgCleanAnalysis` payload. Trace variants and activity totals are not
invented into this format; CLI/server/frontend output adapters serialize the
cached post-filter graph, while input adapters remain open.

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
empty maps, and malformed bounds.

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

Performance spectra expose CONNECTED and DISCONNECTED as distinct native
variants. Both require at least two requested activities, strictly validate
selected timestamps, optionally preserve input order, and otherwise use stable
timestamp ordering. Connected discovery projects each case onto the requested
activities and emits every overlapping full-pattern window. Disconnected
discovery emits labeled contiguous subpatterns longest-first, removing matched
events except the endpoint before considering shorter subpatterns so a full
match is not rediscovered as all of its subsets.

Observations are globally stable-sorted by their first instant. The default
sample size is 10,000 and uses the pinned deterministic midpoint selector,
including forced first and last observations; the result can therefore contain
one or two more observations than the requested sample size. The pinned
EventLog disconnected adapter has a generator-versus-tuple membership defect
that limits it to pairs. Minera implements the intended dataframe/lazyframe
longest-subpattern semantics and documents that repair rather than reproducing
the wrapper defect. Columnar and lazyframe implementations otherwise remain
input adapters to the native behavior.

Batch discovery groups events by activity and resource, using completion time
as the default start or a configured interval-start attribute. It first merges
strictly overlapping intervals transitively, then merges non-overlapping
intervals whose gap is at most the configured distance. Groups smaller than
the minimum batch size are removed. Classification priority is simultaneous,
batching on start, batching on end, sequential, then concurrent. The reference
group count is the number of detected batches; Minera additionally reports the
number of member events and retains trace/event identities so duplicate
timestamp/case tuples are not collapsed.

Spectrum variant, sampling, sorting, batch enablement, merge distance, minimum
size, and start attribute are available through strict request parsing, CLI,
local server, deterministic prevalidated JSON, and frontend controls.
Disconnected observations retain their per-point labels and explicit requested
pattern offset in the JSON/UI, including when activity labels repeat. Batch
JSON/UI exposes all five categories, group batch/event counts, resources,
ranges, and case evidence.

Focused regressions cover stable sorting and its disable switch, overlapping
connected windows, full and partial disconnected matches, loops, non-adjacent
rejection, longest-first suppression, exact midpoint/extreme sampling, all five
batch types, strict-overlap/touching/gap/transitive merging, custom starts,
minimum size, empty resources, duplicate event identities, malformed
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
search. Focused `tests/test_alignment_variants.cpp` regressions cover all six
variants, reference/legacy cost profiles, custom costs and callbacks, full-product
LP solving, less-memory dominance, discounted exponent and scheduling parity,
generator optima and caps, timeout/state diagnostics, tau moves, weighted arcs,
and invalid-cost handling; request, CLI, and frontend tests cover the exposed
adapters.

## Process-tree alignment parity evidence

The four selectable process-tree variants are implemented in the native
`include/minera/process_tree_alignment.hpp` / `src/process_tree_alignment.cpp`
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

Focused validation is in `tests/test_process_tree_alignment.cpp`, with request
mapping/rejection coverage in `tests/test_request_params.cpp`, JSON checks in
`tests/test_core.cpp`, and frontend query/result checks in
`frontend/src/app-logic.test.ts`. Tests cover all four variants, exact
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

Concept-drift adapters expose deterministic `CONCEPT_DRIFT` JSON from the CLI
(`--concept-drift`, `--drift-*`) and cached-log server endpoint
`POST /api/concept-drift`. Strict controls cover sub-log/window sizes,
permutation count, threshold, change-point cap, seed, and optional timestamps;
the result includes activity features, raw and cumulative sub-log summaries,
change points, and timestamp arrays.

## Organizational role discovery parity

The bounded role endpoint (`--roles` and `POST /api/roles`) starts with one
role per activity and counts only events having both a non-empty activity and
resource. Role similarity is the overlap of normalized resource-count vectors
(sum of component-wise minima divided by sum of maxima). The highest-scoring
pair is merged only when its score is strictly greater than
`roles_threshold` (default `0.65`); ties and final role ordering are lexical
and deterministic. Final roles are sorted by descending activity/resource
cardinality before the activity-name tie break. `roles_max_resources` and
`roles_max_activities` are strict safety bounds. Empty activities and
blank-resource events are excluded, while
their traces remain represented by aggregate diagnostics. The standalone JSON
family is `ROLE_DISCOVERY` with `roles[].activities` and
`roles[].resources[{name,count}]`.
