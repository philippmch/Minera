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
| Conformance: replay | forward token replay and backwards token replay | **Partial** — forward token replay only |
| Conformance: relational | footprints log-model/log-extensive/trace-extensive; log skeleton; Declare | **Partial** — log-model footprints only |
| Conformance: temporal | temporal profile log/dataframe adapters, zeta threshold, business-hour slots | **Complete (core)** |
| Conformance: advanced | anti-alignments and multi-alignments (discounted A*) | **Missing** |
| Conformance: object-centric | ETOT, OCDFG and OTG graph comparison | **Missing** |
| Discovery: Alpha | classic and Alpha+ | **Complete** |
| Discovery: Inductive | IM, IMf, IMd | **Complete** |
| Discovery: Heuristics | classic and Heuristics Miner++ | **Partial** — classic-style dependency/AND semantics; ++ missing |
| Discovery: optimization | ILP classic and Genetic classic | **Missing** |
| Discovery: declarative | Declare classic and log skeleton classic | **Missing** |
| Discovery: graph/statistical | DFG frequency/performance/frequency triples/case attributes/clean; causal alpha/heuristic; correlation classic/classic-split/trace-based; minimum self-distance | **Partial** — frequency DFG, performance timing, causal relations, eventually-follows, and minimum self-distance with witnesses exist |
| Discovery: footprints | log entire/trace-by-trace, DFG, Petri reachability, process-tree bottom-up, POWL bottom-up | **Partial** — log/model relation sets exist inside conformance |
| Discovery: structured | transition system view-based; POWL brute-force/dynamic-clustering/frequency/maximal/tree cuts | **Partial** — transition-system view-based (sequence/set/multiset, forward/backward) complete; POWL variants missing |
| Discovery: temporal/behavioral | temporal profile; performance spectrum connected/disconnected; batches | **Partial** — temporal profile and connected performance spectrum |
| Discovery: object-centric | ETOT, timestamp interleavings, link analysis, OCDFG, OCPN annotated/unannotated, OTG, SAW nets | **Missing** |
| Evaluation | replay fitness token/alignment; precision token/alignment/automaton; generalization token; simplicity arc-degree/Cardoso/cyclomatic; earth-mover distance | **Partial** — token/alignment fitness, escaping-edge precision, token generalization, arc-degree simplicity |
| Label splitting | contextual | **Missing** |
| Merging | case relations | **Missing** |
| Organizational mining | local diagnostics, network analysis frequency/performance, resource profiles, roles, SNA metrics | **Partial** — workload and handover network |
| Reduction | process-tree transition-reduction based | **Missing** |
| Simulation | Petri basic/stochastic/extensive; process-tree basic/extensive/top-bottom; DFG classic/performance; Declare; object-centric Petri/causal nets; Monte Carlo; random tree and tree+log generation | **Partial** — workflow-net sampled and extensive playout |
| Transformation | event/trace/temporal features; next-activity/next-time/remaining-time targets; interval tree; trie; embeddings; OCEL descriptions/features/splitting | **Missing** |

## Data, object, conversion, and integration checklist

| Surface | Required variations | Minera status |
|---|---|---|
| Event data | Event, trace, event stream, typed attributes, lifecycle, dataframe/lazyframe adapters | **Partial** — native EventLog with typed scalar attributes; no stream/columnar adapter |
| Object-centric data | OCEL events/objects/relations/o2o/e2e/object changes and OCEL 1/2 schema | **Missing** |
| Models | Petri nets incl reset/inhibitor, markings, process trees, BPMN, DFG, transition systems, heuristics nets, POWL, OCPN/OC causal nets, trie, stochastic nets | **Partial** — ordinary Petri, process tree, BPMN, DFG, heuristics net, and transition systems |
| Conversions | log/stream/table; tree to Petri/BPMN/POWL; BPMN/DFG/heuristics/trie/WF-net conversions; OCEL graph/features | **Partial** — tree/heuristics/BPMN to workflow net and tree/net to BPMN |
| Traditional I/O | XES parser variants, PNML, PTML, BPMN, DFG read/write | **Partial** — XES, PNML, PTML, BPMN; no DFG format |
| Object-centric I/O | OCEL CSV, JSON, XML, SQLite and OCEL 2 JSON/XML/SQLite | **Missing** |
| Filtering | attributes/cases/endpoints/variants/paths/LTL/time/between/rework/prefix/suffix/segments; DFG filters; OCEL filters | **Partial** — broad traditional-log filters; LTL/segments/DFG/OCEL gaps |
| Statistics | endpoints/attributes/variants, passed time, service/cycle/arrival/overlap/rework, concurrent/eventually-follows, process cube, OCEL statistics | **Partial** — most basic log/performance/resource statistics; overlap/cube/OCEL gaps |
| Visualization | Petri/DFG/tree/BPMN/heuristics/footprints/SNA/transition system/POWL/OCEL, decorations and render backends | **Partial** — interactive native UI for existing models; several model families absent |
| Streaming | CSV/XES event/trace streams, streaming DFG, online Declare/footprints/token/temporal conformance, classic/thread-safe/Redis stores | **Missing** |
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
