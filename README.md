# Minera

Minera is an early C++ process mining core built to explore process-mining
workflows with native performance.

The compatibility target is pinned to the **2.7.22 reference release**. The
[feature/variant parity contract](docs/reference-2.7.22-parity.md) separates true
algorithm semantics from Python dataframe adapters and optional integrations,
and tracks every family as complete, partial, or missing.

## Current Core

- CSV event-log import with configurable case, activity, and timestamp columns.
- XES event-log import **and export** (IEEE 1849-2016), the standard
  interchange format used by public datasets (e.g. the BPI Challenge logs),
  reading and writing the common `concept:name` / `time:timestamp` /
  `org:resource` attribute subset — so a log can be cleaned and filtered in
  Minera, then handed to any other XES-speaking tool (CLI `--xes`, or the
  frontend's **Download log as XES** button). Null attributes are omitted by
  default; `--xes-preserve-nulls true` or the download's **preserve null markers**
  option retains Minera's explicit-null extension. Scalar log-level, case-level, and
  generic event attributes retain their XES types through import/export
  round-trips, including alignment projection metadata.
- In-memory event log model grouped into traces.
- Directly-follows graph discovery with activity, start/end, edge, and variant
  counts.
- Minimum self-distance discovery with deterministic witness activities.
- Eventually-follows graph discovery: for every activity pair, how often one
  is followed by the other — directly or transitively — later in the same
  case, surfacing long-range dependencies the directly-follows edges miss.
- Rework statistics: per activity, the number of cases where it executes more
  than once — repeated work that a frequency count alone hides.
- Case arrival rate: summary statistics over the gaps between consecutive
  case starts, alongside the existing case-throughput and edge waiting times.
- Process-model discovery with the **Alpha Miner**, using the classic variant
  by default. Select `--alpha-variant plus` (CLI) or `alpha_variant=plus`
  (server) for Alpha+ short-loop and trace-boundary processing. Alpha+ preserves
  separate loop places and explicit marking identities; it can therefore
  produce results that are not ordinary workflow nets.
- Raw Alpha+ discovery is available with `minera_export log.xes --alpha-plus-result`
  or `POST /api/discover-alpha-plus`. Its JSON preserves live and detached
  markings, including empty traces and all-loop logs; ordinary workflow discovery
  rejects results whose initial marking refers to a removed place.
- Process-model discovery with the **Heuristic Miner**: a dependency measure
  weighs how often one activity follows another against the reverse, and only
  edges above a dependency and frequency threshold are kept — yielding a
  noise-tolerant dependency graph (heuristics net) where the Alpha Miner would
  be misled by infrequent paths. An **AND measure** then decides, per split and
  join, which neighbours run in parallel and which are exclusive alternatives:
  two successors of the same activity are bundled as concurrent when they
  follow each other often relative to the split itself. These semantics carry
  into the workflow-net translation used for conformance, PNML, and BPMN, so
  parallel behaviour replays as concurrency instead of counting as deviation
  (the price is that a badly unbalanced graph can yield an unsound net, which
  conformance reports honestly).
- Process-model discovery with the **Inductive Miner**: recursive
  exclusive/sequence/parallel/loop cuts on the directly-follows graph yield a
  block-structured process tree that is always sound and perfectly fits the log
  (logs without a cut fall through to a flower model). The **infrequent
  variant (IMf)** accepts a **noise threshold**: cuts are sought on the
  unfiltered relations first, and only a sublog with no cut has its weak
  directly-follows edges (and rare start/end activities, empty traces, and
  repetitions) filtered relative to the dominant behaviour before the cut
  search is retried — so a noisy log yields a structured model instead of
  collapsing to a flower, while structure the full relations already exhibit
  is never filtered away. A threshold of 0 is the classic miner. The
  **directly-follows variant (IMd)** reads the log exactly once to build the
  directly-follows graph and then recurses on *projections of the graph*
  instead of splitting traces — time and memory scale with the number of
  activities rather than events, at some model-quality cost (a projected graph
  can imply orderings no trace exhibits, and skipped sequence steps are
  inferred from jumping edges instead of observed empty subtraces). Select it
  with `--inductive-variant dfg` (CLI) or the `inductive_variant` form field.
  Select IMf explicitly with `--inductive-variant imf`; its noise threshold is
  not applied by the classic log or directly-follows variants.
- Conformance checking of a log against **any** discovered model — every miner
  is translated to a workflow net (the Inductive Miner's process tree, the Alpha
  Miner's net directly, and the Heuristic Miner's dependency graph with its
  AND/XOR split and join semantics) — or against a **user-supplied model**: a PNML
  importer reads a workflow net discovered or drawn in another tool, a
  BPMN importer reads a BPMN 2.0 process from any modeler, and a PTML
  importer reads a process tree, so the
  same checks run on any of them (CLI `--check-pnml` / `--check-bpmn` /
  `--check-ptml`, or upload the file in the frontend's Conformance tab). **Token replay**
  measures **fitness** (how much of the log the model reproduces) and escaping
  edges measure **precision** (how little extra behavior the model allows), per
  variant and in aggregate — with precision measured both over replayed states
  and, more robustly on deviating logs, over the states the **optimal
  alignments** visit — alongside **generalization** (how evenly the
  replay exercises the model's transitions, so a model whose parts hinge on a
  few observations scores low) and **simplicity** (how lean the net itself is,
  by mean arc degree), completing the four standard quality dimensions. **Alignment-based conformance** finds, per trace,
  the cheapest sequence of synchronous, log-only, and model-only moves that
  walks the net from start to end (an A\* search over the synchronous product,
  guided by the marking-equation lower bound — aggregated per remaining
  activity and cached across trace variants, so states that share a marking
  and remaining work never pay for a second solve — which keeps even highly
  concurrent models alignable) — giving exact, optimal diagnostics of *where* and *how* each
  trace deviates, plus cost-based fitness. The moves are also aggregated per
  activity across the whole log — how often each activity aligns
  synchronously, is inserted (in the log with no model step), or is skipped
  (required by the model, missing from the log), weighted by trace counts —
  so the systemic deviations stand out from the per-trace detail. **Footprint conformance** compares the
  directly-follows relation of the log against that of the model (found by
  exploring the net's reachable markings): it reports how many of the log's
  successions the model allows (fitness) and how many of the model's the log
  shows (precision), and lists the deviating pairs. Token replay and footprints
  are robust on unsound nets; alignments report that they can't be computed when
  a net (e.g. an Alpha net with concurrency) has an unreachable final marking.
- **DFG classic and edit-distance alignments**: the Conformance tab's **Other
  alignments** panel can align the current log to its directly-follows graph
  (`CLASSIC`) or compare it with a reference CSV/XES log using
  `EDIT_DISTANCE`. DFG requests expose state/time bounds, per-activity move
  costs, and `none`/`log`/`metrics` projections. Edit-distance requests choose
  closest or anti-alignment mode, a positive deviation cost, and `none`/`log`
  projection; log projection records the independently selected target variant
  on each source trace, so heterogeneous selections are supported. The native
  server routes are `POST /api/align-dfg` (query fields such as
  `dfg_variant`, `dfg_max_states`, `dfg_max_time_ms`, `dfg_*_cost`, and
  `projection`) and multipart `POST /api/align-edit-distance` (file field
  `reference_log`, `edit_variant`, `anti`, `deviation_cost`, and `projection`).
- **Process-tree alignments**: the same **Other alignments** panel aligns the
  mined inductive process tree with four selectable variants: `SEARCH_GRAPH_PT`
  (the default residual-expression Dijkstra search), `DYNAMIC_PROGRAMMING`
  (recursive subtree DP with an exact residual search for loops), `MILP`
  (an exact integer shortest-path residual A*/branch-and-bound search with
  native search diagnostics and no external LP solver; loops must be binary),
  and `APPROXIMATED_ORIGINAL`
  (exact search below configurable trace-length/tree-height thresholds, then
  recursive sequence/XOR/parallel decomposition with an exact loop fallback).
  These are deterministic, collision-free native implementations and
  dependency-free clean-room equivalents of the reference variants; `MILP` does
  not claim to invoke literal PuLP. `POST /api/align-process-tree` runs the
  deferred request. The CLI embeds results with `--align-process-tree` and
  accepts `--pt-alignment-variant`, `--pt-alignment-max-states`,
  `--pt-alignment-max-time-ms`, `--pt-alignment-max-total-time-ms`, DP cache
  controls (`--pt-alignment-use-global-cache`,
  `--pt-alignment-batch-size`, `--pt-alignment-clear-cache`), and approximation
  thresholds/timeouts (`--pt-alignment-max-trace-length`,
  `--pt-alignment-max-process-tree-height`,
  `--pt-alignment-max-trace-time-ms`,
  `--pt-alignment-max-per-trace-time-ms`). State and time values of `0` mean
  unlimited; native validation still rejects malformed, negative, non-finite,
  or unsafe combinations (including non-positive DP batches and non-binary
  MILP loops). Results use the `PROCESS_TREE` JSON family with variant,
  aggregate and per-trace cost/fitness, `sync`/`log`/`model` moves, diagnostics,
  and explicit `computed`, `completed`, `timed_out`, and `exhausted` flags.
  Empty fitness denominators remain `0.0`, including an empty log
  against a silent tree. N-ary loop redo/exit children are normalized to an
  XOR for search/DP/approximation; the MILP variant explicitly requires the
  binary form.
- **Decomposed and advanced native conformance**: the C++ API now includes
  decomposed Petri-net alignment via `RECOMPOS_MAXIMAL`, plus Petri-net
  anti-alignments and multi-alignments via `VERSION_DISCOUNTED_A_STAR`.
  Decomposed alignment uses real place/silent-transition components, projected
  component searches, cached border-agreement repair, all-owner component
  merging, and causal DAG recomposition without substituting a whole-net
  result. Anti-/multi-alignment retain tau transitions, weighted arcs,
  arbitrary markings, the pinned geometric discounted-edit objectives, and
  their ordinary precision/maximum-distance diagnostics. The complete option
  sets and result diagnostics are available through the C++ API, JSON export,
  CLI, local server, and frontend conformance workspace.
- **Trace-based process-tree reduction**: `TREE_TR_BASED` removes optional
  non-root subtrees whose activities do not occur in a trace, then applies the
  pinned folding rules without mutating the source tree. It preserves root
  replacement quirks, XOR tau ordering, mixed loop/tau behavior, and custom
  string activity keys. A selected trace can be reduced through the CLI,
  local server, or frontend, with the reduced recursive tree embedded in JSON.
- **Model annotations (frequency & performance)**: token replay of the
  cleanly fitting cases decorates the discovered model itself — per
  transition, how often it actually fired (so a path the log never takes is
  visibly dead), and per place, how long tokens rest there between production
  and consumption (count, mean, median, min, max) — bringing the process
  map's frequency/performance perspective onto the model's own structure.
  Deviating cases are excluded rather than force-replayed, and the annotation
  says how many cases it describes. Every miner's model is decorated as its
  workflow net — the Inductive Miner's net fits its log by construction, so
  that view always carries full annotations even on noisy real-world logs
  where the Alpha net fits little. Carried in the JSON export as
  `petri_net_decoration` (alpha), `inductive_petri_net(_decoration)`, and
  `heuristic_petri_net(_decoration)`, each aligned index-for-index with its
  net.
- **Transition-system discovery**: reproduce the reference implementation's view-based transition
  system in sequence, set, and multiset variants, with forward/backward
  windows and optional stable event occurrence references (`--transition-view`,
  `--transition-direction`, `--transition-window`, `--transition-include-data`).
  Imported logs are normalized to importer event order; direct `EventLog` API
  callers retain the order they supply.
- **Performance spectrum**: pick two or more activities as a pattern, and
  every case that performs them as an uninterrupted run (other activities in
  between are ignored) contributes one observation — the instants at which
  each step happened. Connected, disconnected, log, dataframe, and the two
  disconnected projection variants are selectable, with custom activity,
  timestamp, and case-id attributes. Plotted as lines between the steps' time
  axes, the observations show how flow times evolve over the log's lifetime,
  where cases batch up, and which ones overtake others — detail a mean waiting
  time averages away. CLI `--spectrum "a,b,c"` embeds it in the JSON export;
  the frontend computes and draws it live on the **Events over Time** tab,
  with lines graded by flow-time tercile.
- **Temporal-profile discovery and conformance**: every ordered activity pair
  in a trace contributes its elapsed-time observation, producing count, mean,
  and sample standard deviation per pair. Conformance flags observations
  outside a configurable number of standard deviations, supports separate
  completion/start timestamp attributes, and can measure time through a
  normalized weekly business-hours calendar (Monday-Friday 07:00-17:00 by
  default).
- **Decision mining** (Rozinat & van der Aalst): where a model offers
  alternatives — a place in the workflow net with several outgoing
  transitions — Minera learns *which attribute values steer a case down which
  branch*. Every trace that replays cleanly contributes its choices, each
  observed together with the attributes in scope at that moment (the case
  attributes plus the latest value of every event attribute and the
  performing resource seen strictly before the event that triggered the
  choice — information that arrives *with* the choice is never used to
  explain it). A small CART-style decision tree (Gini impurity, binary
  categorical-equality and numeric-threshold splits, missing values falling
  to the else side) is grown per decision point and reported as readable
  rules — `amount ≤ 1150 → Approve` — with each tree's accuracy against the
  always-guess-the-majority baseline, so an uninformative rule is visible as
  such. Works on any miner's model: CLI `--decisions <miner>`, or the
  Conformance tab's **Decisions** method, mined on demand like alignments.
- **Performance (time) perspective**: from event timestamps, Minera measures
  the waiting time on every directly-follows edge (count, mean, median, min,
  max, total) and the end-to-end throughput time of each case. An ISO-8601
  timestamp parser normalizes instants (handling `T`/space separators,
  fractional seconds, and `Z`/±HH:MM offsets); untimed events are simply skipped.
  When the log records **lifecycle transitions** (the XES
  `lifecycle:transition` attribute or a CSV `lifecycle` column), Minera also
  measures each activity's **service time** — how long work actually takes
  *inside* the activity, from its `start` event to its `complete` event
  (matched first-in-first-out per case and activity, case-insensitively, so
  BPI-style `START`/`COMPLETE` logs work) — separating working time from the
  waiting time the edges show. A **lifecycle filter** projects the log onto
  chosen transitions, e.g. keep only `complete` to make a start/complete log
  atomic for discovery instead of every activity appearing twice.
- **Organizational (resource) perspective**: with a resource attribute (a CSV
  `resource` column or the XES `org:resource` attribute), Minera builds the
  handover-of-work social network — how often work passes from one resource to
  another within a case — plus per-resource workload counts.
- **PNML export and import**: any discovered model can be written as PNML
  (ISO/IEC 15909-2), the standard Petri-net interchange format read by ProM and
  another process-mining tool — and a PNML workflow net from any of those tools can be read back for
  conformance checking. Every miner reuses the same workflow-net translation
  used for conformance, so the exported net is exactly what Minera checks.
- **Process-tree PTML export and import**: the Inductive Miner's
  block-structured tree writes as PTML — the process-tree interchange format
  other process-mining tools read (one element per node, loops in the
  three-child body/redo/exit shape they expect) — and a PTML tree from any of
  them reads back for conformance checking, playout, or re-export. A loop's
  non-silent exit child converts as "run once after the loop", so the checked
  net means exactly what the tree says.
- **BPMN 2.0 export and import**: every miner exports — the inductive model
  converts block by block from its process tree, and the Alpha and Heuristic
  models convert from their workflow nets by token semantics — a branching
  place becomes an exclusive gateway, a forking transition a parallel gateway
  — readable by Camunda, bpmn.io, Signavio, and most BPM suites. The importer
  reads the same notation back by its token rules — a sequence flow holds a
  token like a place, tasks become transitions, an exclusive gateway routes
  one token and a parallel gateway forks and joins them, and expanded
  subprocesses convert recursively between their own start and end events —
  so a process drawn or exported anywhere can be conformance-checked, played
  out, or re-exported like a discovered model. Block-structured inclusive
  split/join gateways are compiled with selector and completion control places,
  admitting every non-empty branch subset (including exported silent branches).
  Mixed or ambiguous inclusive graphs, complex gateways, and boundary events
  remain explicit errors rather than being checked approximately.
- **Model playout**: simulate any workflow net — a discovered model, or an
  imported PNML net, BPMN process, or PTML tree — into a synthetic,
  timestamped event log (seeded, so runs reproduce). On a sound model every
  generated trace replays with fitness 1.0, which makes the playout a
  self-check for discovery and a test-log generator (`minera_playout
  model.pnml -o log.xes`, `model.bpmn` and `model.ptml` likewise, or the
  **Simulate log** button on any mined model). An **extensive** mode
  (`--extensive`, with `--max-length`) enumerates every distinct trace the
  model allows up to a length bound instead of sampling — shortest first,
  each exactly once — turning the playout into a listing of the model's
  language, e.g. for eyeballing what a permissive model really admits
  (also `GET /api/playout?mode=extensive` on the server).
- **Generic event attributes**: extra CSV columns and non-standard XES event
  attributes (channel, region, amount, …) are imported alongside the
  case/activity/timestamp/resource fields, keep their XES types through an
  export round-trip, and are summarized per key (event count, value
  frequencies) in every analysis.
- **Case attributes**: scalar XES trace attributes beyond the case id (e.g.
  the BPI Challenge 2012's `AMOUNT_REQ`) and `case:`-prefixed CSV columns
  land on the case itself, keep their XES types through an export
  round-trip, are summarized per key (case count, value frequencies), and
  filter the log at case granularity — keep or drop whole cases by attribute
  value.
- Log preprocessing: filter an event log by time range (keep the events inside
  a window, or the cases contained in / intersecting it), activity projection,
  resource (performer) projection, lifecycle projection, attribute values (drop or keep matching
  events, or whole cases with a match), case-attribute values (keep or drop
  whole cases by their own attributes), rework (keep only cases where an
  activity repeats), succession (keep or drop cases where one activity is
  directly — or eventually, anywhere later in the case — followed by another),
  prefix/suffix cuts (reduce each case to its head up to the first occurrence
  of an activity, or its tail from it), between-extraction (cut each case into
  the subtraces running from one activity to another — e.g. every loop
  iteration), trace length, start/end activities, and variant selection
  (minimum count, top-N, or the most frequent variants that together cover a
  share of the cases) before discovery.
- `minera_server`, a local application server: it serves the frontend and
  mines uploaded CSV event logs with the native core (`POST /api/mine`),
  returning the event log, the directly-follows graph, the process tree, and
  conformance results.
- `minera_export` CLI that mines a CSV log and emits the event log, the
  discovered graph, the process tree, and conformance results as JSON, with
  optional log-filtering flags.
- `minera_dfg` CLI that strictly reads and normalizes classic DFG text (or
  writes it to a requested output path); the same parser is available through
  the local server's `POST /api/dfg` and the typed frontend import helper.
- A React + TypeScript frontend (`src/frontend/`) that uploads a CSV event log to
  the backend, shows the imported log first, and puts the directly-follows
  graph and the discovered process model one tab away each.

## Build

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Implementation sources live under `src/backend/`, while the React/Vite
application is under `src/frontend/`. Native tools and tests follow their
owners in `src/backend/tools/` and `src/backend/tests/`; frontend tests live
in `src/frontend/tests/`.

The other top-level directories are intentional repository boundaries:

| Directory | Responsibility |
| --- | --- |
| `include/` | Installed/public C++ headers (`include/minera/`). |
| `third_party/` | Vendored build dependencies kept separate from project code. |
| `scripts/` | Repository automation and local development helpers. |
| `docs/` | Design notes, contracts, and audit documentation. |
| `data/` | Small sample inputs used by examples and smoke checks. |

## Minimal Example

```cpp
#include "minera/csv_event_log_importer.hpp"
#include "minera/directly_follows_graph.hpp"

auto log = minera::import_csv_event_log("event_log.csv");
auto graph = minera::discover_directly_follows_graph(log);

auto count = graph.edge_count("Register request", "Check ticket");
```

By default, Minera expects CSV columns named `case_id`, `activity`, and
optionally `timestamp`, `resource`, and `lifecycle`. If `timestamp` is present,
events inside each trace are sorted lexicographically by timestamp, which fits
ISO-8601 exports, and feeds the performance perspective; if `resource` is
present it feeds the organizational perspective; if `lifecycle` is present its
start/complete pairs feed the activity service times.

## Frontend

Minera runs as a local desktop-style app: one native binary serves the UI and
does the mining. Build everything once, then start the server from the repo
root:

```sh
cd src/frontend && npm install && npm run build && cd ../..
cmake -S . -B build && cmake --build build
./build/minera_server        # → open http://localhost:8484
```

The launch screen takes a CSV event log (`case_id`, `activity`, `timestamp`
columns; a bundled order-to-cash sample is one click away) or an `.xes` file.
The frontend uploads it to `POST /api/mine`, where the native core picks the
importer by extension/content, then imports and mines it in memory — the log
never leaves your machine. An opened log shows the raw
events first; discovery lives in tabs:

- **Event Log** — the imported cases with their event sequences, timestamps,
  and case attributes, filterable by case id, with a **Download log as XES**
  button that exports the log exactly as mined (filters included),
- **Events over Time** — a dotted chart (one row per case ordered by start
  instant, one dot per event colored by activity) exposing arrival patterns,
  batching, and drift at a glance, and the **performance spectrum**: choose
  a sequence of activities and every uninterrupted run a case makes through
  them draws one line between the steps' time axes, colored by flow-time
  tercile — so slow stretches and overtaking stand out,
- **Directly-Follows Graph** — an interactive process map (pan, zoom, and a
  minimum-frequency path filter) with a **frequency / performance** toggle that
  relabels edges with mean waiting times (and shows median/mean case throughput)
  when the log is timed, a variant explorer that highlights paths on the map,
  per-activity frequency, entry, and exit statistics (plus a mean service
  time per activity when the log records lifecycle start/complete pairs), and an
  **eventually-follows matrix** — a heatmap of how often each activity is
  eventually followed by each other, exposing long-range order the map's
  local edges don't show,
- **Mine a Process Model** — pick a discovery algorithm (the Inductive Miner's
  process tree, the Alpha Miner's Petri net, or the Heuristic Miner's dependency
  graph) and explore the resulting model; the tree and dependency-graph
  views flip to the model's **workflow net** — the exact net conformance
  checks and exports use — and every Petri-net view has a
  **plain / frequency / performance** toggle that badges each transition with
  its replay firing count (never-fired paths fade out) and labels each place
  with the mean time tokens wait in it, with a caption saying how many cases
  the replay covers; the Heuristic Miner's dependency and
  frequency thresholds are live sliders that re-mine the net in the browser,
  **Download … as PNML / as BPMN** buttons export any miner's model for
  ProM or a BPM suite (the inductive tree also **as PTML**), and **Simulate log (XES)** plays the model out
  into a synthetic 100-case log,
- **Conformance** — pick any miner (or upload a PNML workflow net, a BPMN
  2.0 process, or a PTML process tree from another tool) and any method: fitness, precision, generalization, and simplicity of
  the log against that model, plus a per-variant breakdown of where traces
  deviate. **Token Replay** shows token counts; **Alignments** renders each
  deviating trace move by move (synchronous, log-only, model-only) with a
  per-activity deviation table — inserted and skipped counts across all
  cases, worst first;
  **Footprints** compares the log's and model's directly-follows relations and
  lists the successions they disagree on; **Decisions** mines, from the
  cleanly replaying cases, which attribute values steer a case down which
  branch of the model — readable rules with their accuracy against the
  majority baseline. Every miner is checked at import;
  when a model isn't a sound workflow net the alignment view says so rather
  than showing misleading numbers (token replay and footprints still apply),
- **Resources** — when the log carries a resource attribute: the workload per
  resource and the handover-of-work network (how often a case passes from one
  resource to the next).

A **Filter log** bar above the tabs re-mines the original upload focused on the
most frequent variants (top-N, a minimum frequency, or a cumulative coverage
share), a time window (keeping
the events inside it, or the cases contained in / touching it), rework (only
cases where a chosen activity repeats), a succession (keeping or dropping the
cases where one activity is directly — or eventually — followed by another), a
head or tail cut (each case reduced to everything up to the first occurrence
of an activity, or from it on), a between cut (each case reduced to its
stretches from one activity to another), a subset of activities or resources,
an event attribute's values (dropping the matching events, or whole cases
with a match), or a case attribute's values (dropping whole cases); every
view — log, graph, model, conformance, and resources — updates together.

For frontend development, run the Vite dev server alongside the backend; it
proxies `/api` to `localhost:8484`:

```sh
./build/minera_server &
cd src/frontend && npm run dev   # → http://localhost:5173
```

JSON files produced by the `minera_export` CLI open the same way as CSVs:

```sh
./build/minera_export my_event_log.csv -o my_export.json
```

`minera_export` reads a `.xes` file as XES and any other file as CSV (the
column/delimiter options apply to CSV only). It accepts options for custom
column names and delimiters, plus log-filtering flags applied before discovery
— for example, focus on the most common behavior:

```sh
./build/minera_export my_event_log.csv --top-variants 5 -o my_export.json
```

To export the raw RUSTXES table before case grouping, use
`minera_export log.xes --xes-import-variant rustxes --xes-return-legacy-log-object false`.
Add `--xes-return-pl-lazyframe true` for the Polars-compatible table schema.
Either return flag selects import-result JSON, including column dtypes and cells;
`-o` writes that result to a file. Legacy mode takes precedence when both flags
are true. The native lazy API scans an in-memory table; JSON output materializes
its rows.

RUSTXES legacy conversion accepts `--xes-conversion-case-id-key <column>` to group
table rows by an event or case column. Use
`--xes-conversion-case-attribute-prefix <prefix>` to select trace attributes.
For example, grouping by `concept:name`
groups rows by activity. Raw table return modes ignore these conversion options.
`--xes-conversion-infer-extensions false` clears extension metadata; repeated
`--xes-conversion-extension <name> <prefix> <uri>` arguments supply explicit
extensions, with the last value winning for duplicate names.

`--check-pnml model.pnml` additionally checks the log against the workflow net
in that PNML file; its results appear in the export under the conformance id
`imported`, next to the discovered models. `--check-bpmn model.bpmn` and
`--check-ptml model.ptml` do the same for a BPMN 2.0 process and a PTML
process tree (the three flags share the `imported` id, so pass only one).
`--ptml <path>` writes the inductive process tree as PTML, next to the
`--pnml` / `--bpmn` model exports; the running server serves the same at
`GET /api/ptml`.

`--no-alignments` skips embedding the optimal alignments in the export
(token replay and footprints stay). Worth reaching for on a very large log
checked against a badly fitting model: optimal alignments need a deep search
per trace variant there, which can dominate the whole run — the CLI prints a
note when that risk applies. The frontend's Alignments view then computes
them on demand against a running server, exactly as it does for server-mined
logs.

Alignment search defaults to less-memory Dijkstra with legacy unit costs, but
can be selected explicitly: `--alignment-variant`
accepts `state_equation_a_star`, `dijkstra_no_heuristics`,
`dijkstra_less_memory`, `discounted_a_star`,
`generator_dijkstra_no_heuristics`, or `generator_dijkstra_less_memory`.
`--alignment-cost-profile legacy|standard` selects cost semantics; state/time
limits (`--alignment-max-states`, `--alignment-max-align-time`,
`--alignment-max-align-time-trace`), generator `--alignment-max-alignments`,
discounted `--alignment-exponent`/`--alignment-synchronous-dijkstra`, and move-cost
overrides are available as separate flags. The Conformance tab's Alignment
settings exposes the same variant, profile, limits, generator cap, discounted
controls, and move costs; Apply sends a deferred alignment request to the
server, while `--no-alignments` remains the export opt-out.

The same other-alignments operations are available in `minera_export`: pass
`--align-dfg` with optional `--dfg-max-states`, `--dfg-max-time-ms`,
`--dfg-sync-cost a=f`, `--dfg-model-move-cost a=f`, `--dfg-log-move-cost a=f`,
`--dfg-internal-log-move-cost a=f`, and `--dfg-projection none|log|metrics`;
pass `--align-edit-distance reference.csv` (or `.xes`) with optional
`--edit-anti`, `--edit-deviation-cost <n>`, and `--edit-projection none|log`.
The frontend exposes these controls in the Conformance tab's **Other
alignments** panel and runs them on demand against the latest mined session.
Frontend and HTTP DFG cost maps accept `activity=value` entries separated by
commas; escape `,`, `:`, `=`, and `\` inside an activity name with a backslash,
and encode whitespace as `\s`, `\t`, `\n`, or `\r`. CLI cost options each carry
one pair, use the rightmost `=` as their separator, and therefore need no such
escaping. Omitted cost maps use defaults; supplied maps must contain every
activity cost the search requests. Use `{}` to supply an explicitly empty map
(a blank frontend field omits it). Raw native trace alignment returns search
results without fitness; whole-log alignment adds fitness and its empty-trace
baseline, and fails if that baseline has no accepting path or the fitness
denominator is zero. Edit-distance `log` projection selects and records the appropriate
reference variant independently for each source trace, including heterogeneous
logs.

Process-tree alignment defaults are `SEARCH_GRAPH_PT`, unlimited state,
per-trace, and whole-log bounds (`0`), DP global cache off, batch size `1000`,
and cache clearing on. Approximation thresholds default to trace length `1`,
tree height `1`, and unlimited approximation timeouts (`0`); those controls
apply only to `APPROXIMATED_ORIGINAL`, while cache/batch controls apply only to
`DYNAMIC_PROGRAMMING`. A bounded or timed-out request is returned as an
explicit incomplete result rather than an invented optimum, and diagnostics
include visited/queued states, traversed arcs, native residual-search counts,
and aligned-trace counts (`lp_solved` remains zero because no external LP
solver is used).

`--decisions <miner>` additionally mines the decision points of one
discovered model — which attribute values steer a case down which branch —
and embeds the rules in the export under `decisions`.

`--xes out.xes` additionally writes the event log — after any filtering — as
standard XES, making the CLI double as a CSV-to-XES converter. `--bpmn
out.bpmn` writes a discovered model as BPMN 2.0, with `--bpmn-miner`
(inductive, alpha, heuristic, or powl) picking which — mirroring `--pnml` /
`--pnml-miner`.

Other filters include `--min-variant-frequency`, `--variant-coverage` (keep
the most frequent variants that together cover a share of cases, e.g. `0.8`),
`--keep-activities`, `--remove-activities`, `--keep-resources`,
`--remove-resources`, `--start-activities`, `--end-activities`,
`--min-length`, and `--max-length`.
Succession filters keep or drop whole cases by an activity pair:
`--keep-directly-follows "a -> b, c -> d"` keeps only cases where some listed
activity is directly followed by its partner, `--remove-directly-follows`
drops them, and the `--keep-eventually-follows` / `--remove-eventually-follows`
variants let the partner occur anywhere later in the same case.
`--prefix-activity <a>` reduces each case to its head up to (and including)
the first `<a>`; `--suffix-activity <a>` keeps the tail from the first `<a>`
on; cases without the activity are dropped.
`--attribute-key` filters on a generic event attribute (an extra CSV column or
non-standard XES key): `--keep-attribute-values` / `--remove-attribute-values`
constrain its values, the bare key keeps only events carrying the attribute,
and `--attribute-mode traces` switches from filtering events to keeping or
dropping whole cases by the same match.
`--rework-activity` (with `--rework-min`, default 2) keeps only cases where an
activity repeats; `--between-start` / `--between-end` cut each case into the
subtraces running from one activity to the next occurrence of the other, both
endpoints included.
A time window is `--time-from` / `--time-to` (ISO-8601,
both inclusive, either side may be open) with `--time-mode` choosing how it
reads a trace: `events` (default) keeps only the events inside the window,
`traces_contained` keeps whole cases that lie fully inside it, and
`traces_intersecting` keeps whole cases that touch it. Run `minera_export`
without arguments to see usage. The same filters are available on
`POST /api/mine` as form fields (`top_variants`, `min_variant_frequency`,
`keep_activities`, `keep_resources`, `remove_resources`, `time_from`,
`time_to`, `time_mode`, …).

Select `--inductive-variant imf` and pass `--noise-threshold <f>` (0–1) to run
the Inductive Miner's infrequent variant; a threshold of `0` retains all
behaviour. The same values are accepted on `POST /api/mine` as
`inductive_variant` and `noise_threshold`. The available variants are
`--inductive-variant log|imf|dfg`. Discovery also exposes strict-sequence and
fall-through switches, optional timestamp ordering, and activity, timestamp,
and case-identifier selectors through the corresponding `--inductive-*` CLI
options and `inductive_*` form fields. The other miner settings travel the same
two routes: `--alpha-variant plus|classic` / `alpha_variant`, and the Heuristic
Miner's `--dependency-threshold` / `dependency_threshold` (default 0.9),
`--and-threshold` / `and_threshold` (default 0.65), and
`--min-edge-frequency` / `min_edge_frequency` (default 1). In the frontend all
of them live in the **Filters & miner settings** panel, where every apply
re-mines the original upload — so a threshold can always be relaxed again.

The JSON export also carries the performance (`performance`) and resource
(`resources`) perspectives. To additionally write a model as PNML, pass
`--pnml <path>` (and `--pnml-miner inductive|alpha|heuristic|powl`, default
`inductive`); the running server exposes the same export at
`GET /api/pnml?miner=…`:

```sh
./build/minera_export my_event_log.csv --pnml model.pnml --pnml-miner alpha
```

The HTTP layer uses the vendored
[cpp-httplib](https://github.com/yhirose/cpp-httplib)
(`third_party/httplib.h`).

### Sample dataset

The bundled sample (`data/sample_event_log.csv`) is produced by the
`minera_generate_sample` tool; its mined export lives at
`src/frontend/public/sample-data.json`:

```sh
./build/minera_generate_sample data/sample_event_log.csv
./build/minera_export data/sample_event_log.csv -o src/frontend/public/sample-data.json
```
