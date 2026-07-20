# PM4Py 2.7.22 parity contract

Minera targets behavioral feature parity with PM4Py **2.7.22**, tag commit
`0715c31f3f1d85924107d6aa5329806db69df899`. The target is pinned so “all
variations” is finite, reviewable, and testable. Moving the target requires a
separate catalogue diff and must not silently reopen completed rows.

PM4Py's open-source implementation is AGPL-3.0. Minera therefore uses public
documentation, published papers, independently written implementations, and
canonicalized black-box fixtures. PM4Py source may be inventoried for names and
dispatch structure, but code is not copied into Minera.

Run the reproducible catalogue audit against an exact checkout:

```powershell
node scripts/audit-pm4py-catalog.mjs C:\path\to\pm4py-2.7.22
```

At the pinned tag the source catalogue contains 18 algorithm domains, 86
`algorithm.py` packages, 182 implementation files under `variants` trees, 779
Python files under `pm4py/algo`, 348 under `objects`, 156 under `statistics`,
and 142 under `visualization`.

## What parity means

Status values are:

- **Complete**: equivalent supported inputs, output invariants, important
  parameters, diagnostics, edge cases, deterministic fixtures, and a public
  Minera entry point.
- **Partial**: a useful equivalent exists, but at least one named behavioral
  variant, parameter family, diagnostic, or model conversion is absent.
- **Missing**: no public behavioral equivalent exists.
- **Adapter**: PM4Py has separate EventLog/pandas/polars implementations of the
  same semantics. One native C++ implementation is sufficient; these do not
  count as separate process-mining algorithms.
- **Integration**: backend, renderer, connector, or optional ML/LLM surface.
  It is tracked, but not allowed to inflate the core algorithm count.

Python symbol-for-symbol API compatibility is not a goal. Semantic parity is:
the same process-mining question, equivalent supported data, canonicalized
model/output behavior, the same quality metric or diagnostic meaning, and
documented differences where multiple mathematically valid outputs exist.

## Algorithm-family checklist

This is the human-owned acceptance matrix. The audit script is the
machine-owned list of exact modules; a release may not claim parity unless both
are reconciled.

| Domain | PM4Py families and behavioral variants | Minera status |
|---|---|---|
| Analysis | marking equation (classic), extended marking equation (classic), workflow-net check, Woflan soundness with reachability/coverability/invariants | **Partial** — final-marking reachability only |
| Anonymization | PRIPEL; trace-variant query Laplace and SaCoFa | **Missing** |
| Clustering | profile clustering; trace-attribute-driven activity/successor/log-slice/similarity distances | **Missing** |
| Comparison | Petri-net element-usage comparison | **Missing** |
| Concept drift | Bose feature extraction and drift detection | **Missing** |
| Conformance: Petri alignments | state-equation A*, Dijkstra no heuristics, Dijkstra less memory, discounted A*, generator variants | **Partial** — optimal A* behavior exists; named search/cost variants missing |
| Conformance: other alignments | decomposed recomposition, DFG classic, edit distance, process-tree search graph/dynamic programming/MILP/approximated original | **Missing** |
| Conformance: replay | forward token replay and backwards token replay | **Complete (core)** — forward replay remains the default; the deprecated backwards variation, reverse silent routing, weak firing, duplicate-label behavior, and immediate-stop option are selectable |
| Conformance: relational | footprints log-model/log-extensive/trace-extensive; log skeleton; Declare | **Complete (core)** — all three footprint checks, classic log-skeleton conformance, and classic Declare conformance are available with per-trace diagnostics |
| Conformance: temporal | temporal profile log/dataframe adapters, zeta threshold, business-hour slots | **Complete (core)** |
| Conformance: advanced | anti-alignments and multi-alignments (discounted A*) | **Missing** |
| Conformance: object-centric | ETOT, OCDFG and OTG graph comparison | **Missing** |
| Discovery: Alpha | classic and Alpha+ | **Complete** |
| Discovery: Inductive | IM, IMf, IMd | **Complete** |
| Discovery: Heuristics | classic and Heuristics Miner++ | **Complete (core)** — both variants, exact dependency/AND/loop behavior, DFG pre-cleaning, filtering, performance aggregation, interval concurrency, and classic DFG input are available |
| Discovery: optimization | ILP classic and Genetic classic | **Missing** |
| Discovery: declarative | Declare classic and log skeleton classic | **Complete (core)** |
| Discovery: graph/statistical | DFG frequency/performance/frequency triples/case attributes/clean; causal alpha/heuristic; correlation classic/classic-split/trace-based; minimum self-distance | **Partial** — frequency DFG, performance timing, causal relations, eventually-follows, and minimum self-distance with witnesses exist |
| Discovery: footprints | log entire/trace-by-trace, DFG, Petri reachability, process-tree bottom-up, POWL bottom-up | **Complete (core)** — dataframe and lazyframe forms are adapters over the same native semantics |
| Discovery: structured | transition system view-based; POWL brute-force/dynamic-clustering/frequency/maximal/tree cuts | **Complete (core)** — transition systems support sequence/set/multiset views, forward/backward direction, arbitrary windows including zero, and optional occurrence data; POWL exposes TREE, BRUTE_FORCE, MAXIMAL, DYNAMIC_CLUSTERING, and DYNAMIC_CLUSTERING_PLAIN/frequency behavior with filtering and order-frequency settings |
| Discovery: temporal/behavioral | temporal profile; performance spectrum connected/disconnected; batches | **Partial** — temporal profile and connected performance spectrum |
| Discovery: object-centric | ETOT, timestamp interleavings, link analysis, OCDFG, OCPN annotated/unannotated, OTG, SAW nets | **Missing** |
| Evaluation | replay fitness token/alignment; precision token/alignment/automaton; generalization token; simplicity arc-degree/Cardoso/cyclomatic; earth-mover distance | **Partial** — token/alignment fitness, escaping-edge precision, token generalization, arc-degree simplicity |
| Label splitting | contextual | **Missing** |
| Merging | case relations | **Missing** |
| Organizational mining | local diagnostics, network analysis frequency/performance, resource profiles, roles, SNA metrics | **Partial** — workload and handover network |
| Reduction | process-tree transition-reduction based | **Missing** |
| Simulation | Petri basic/stochastic/extensive; process-tree basic/extensive/top-bottom; DFG classic/performance; Declare; object-centric Petri/causal nets; Monte Carlo; random tree and tree+log generation | **Partial** — workflow-net sampled/extensive playout and Declare classic playout are present |
| Transformation | event/trace/temporal features; next-activity/next-time/remaining-time targets; interval tree; trie; embeddings; OCEL descriptions/features/splitting | **Missing** |

## Data, object, conversion, and integration checklist

| Surface | Required variations | Minera status |
|---|---|---|
| Event data | Event, trace, event stream, typed attributes, lifecycle, dataframe/lazyframe adapters | **Partial** — native EventLog with typed scalar attributes; no stream/columnar adapter |
| Object-centric data | OCEL events/objects/relations/o2o/e2e/object changes and OCEL 1/2 schema | **Missing** |
| Models | Petri nets incl reset/inhibitor, markings, process trees, BPMN, DFG, transition systems, heuristics nets, POWL, declarative models, OCPN/OC causal nets, trie, stochastic nets | **Partial** — ordinary Petri, process tree, BPMN, DFG, heuristics net, transition system, POWL, classic log-skeleton, and Declare objects; reset/inhibitor, object-centric, trie, and stochastic families remain |
| Conversions | log/stream/table; tree to Petri/BPMN/POWL; BPMN/DFG/heuristics/trie/WF-net conversions; OCEL graph/features | **Partial** — tree/heuristics/POWL/BPMN to workflow nets, tree to POWL, and tree/net to BPMN; remaining DFG/trie/object-centric and adapter conversions are absent |
| Traditional I/O | XES parser variants, PNML, PTML, BPMN, DFG read/write | **Partial** — XES, PNML, PTML, BPMN; no DFG format |
| Object-centric I/O | OCEL CSV, JSON, XML, SQLite and OCEL 2 JSON/XML/SQLite | **Missing** |
| Filtering | attributes/cases/endpoints/variants/paths/LTL/time/between/rework/prefix/suffix/segments; DFG filters; OCEL filters | **Partial** — broad traditional-log filters; LTL/segments/DFG/OCEL gaps |
| Statistics | endpoints/attributes/variants, passed time, service/cycle/arrival/overlap/rework, concurrent/eventually-follows, process cube, OCEL statistics | **Partial** — most basic log/performance/resource statistics; overlap/cube/OCEL gaps |
| Visualization | Petri/DFG/tree/BPMN/heuristics/footprints/SNA/transition system/POWL/declarative/OCEL, decorations and render backends | **Partial** — interactive native UI includes recursive POWL, footprint variants/diagnostics, Declare, log-skeleton, converted workflow nets and decorations; several model families and render backends remain |
| Streaming | CSV/XES event/trace streams, streaming DFG, online Declare/footprints/token/temporal conformance, classic/thread-safe/Redis stores | **Partial** — thread-safe Declare AUTOMATA conformance core is present; stream readers, other online algorithms, and persistent stores remain |
| Decision mining | decision-tree extraction, encoded decision tables, data Petri nets | **Partial** — decision trees/rules; no data-Petri-net object |
| Privacy | anonymization algorithms and privacy-preserving query variants | **Missing** |
| ML | train/test splits, prefixes, feature tables, prediction targets, OCEL features | **Missing** |
| LLM/querying | abstractions, provider calls, clustering/hypotheses, natural-language log query/filter, database knowledge injection | **Missing / Integration** |
| Connectors | Outlook, Windows events, browsers, GitHub, Camunda, SAP O2C/accounting | **Missing / Integration** |

## Delivery order

1. Stabilize the native semantic contracts and differential fixture format.
2. Finish traditional discovery/conformance/evaluation variants.
3. Add OCEL 2 data, I/O, discovery, conformance, filtering, and visualization.
4. Add declarative/POWL/transition-system and advanced simulation families.
5. Add streaming, privacy, clustering/drift, transformations, and ML.
6. Add optional renderers, connectors, querying, and LLM integrations.

Every completed row must include focused unit tests. Algorithms with
non-unique models compare canonical behavior (language, footprints, fitness,
or replay) rather than generated node identifiers. Floating metrics use an
explicit tolerance. Performance claims require representative logs and may
not replace semantic tests.

## POWL parity evidence

The POWL review is pinned to commit
`0715c31f3f1d85924107d6aa5329806db69df899`. Minera independently implements
the five selectable discovery behaviors, both filtering policies, filtering
threshold, frequency-order ratio, and a caller-controlled brute-force activity
limit. The common loop cut follows the reference behavior for:

- start/end boundary union and removal of boundary nodes and incident edges;
- weak redo-component discovery;
- start/end reachability repair and completeness-based component merging;
- one merged redo group; and
- multiplicity-preserving, alternating contiguous do/redo projections.

Focused regressions cover ordered multi-activity redo behavior, different
start/end boundary activities, prefix/suffix false positives, incomplete
boundary reachability, and single closed variants. Recursive POWL JSON, request
validation, CLI flags, local-server mining, frontend controls/rendering,
converted-net conformance and decoration, and the PNML/BPMN/decision/playout
selectors are exercised by native tests, frontend tests/build, and headless
CLI/server smoke checks.

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
