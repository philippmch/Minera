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
  resource to the next),
- **Search & embeddings** — search complete case activity sequences or individual
  event text, inspect cosine-ranked matches, and download full embedding vectors
  or match results as JSON. Token hashing works immediately; the local transformer
  below enables semantic search. Text attributes, event IDs, vector width and
  item limits are configurable. Upload a CSV or XES log to use this tab; opening
  a saved JSON export does not create the native log session it needs.

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

For real transformer embeddings, install the optional encoder in a Python 3.11+
virtual environment. On Windows PowerShell, from the repository root:

```powershell
py -3.11 -m venv build/transformer-runtime-venv
& build/transformer-runtime-venv/Scripts/python.exe -m pip install -r scripts/requirements-transformers.txt
& build/transformer-runtime-venv/Scripts/python.exe scripts/embedding_runtime.py --cache-folder build/transformer-model-cache
```

The first start downloads the public `sentence-transformers/all-MiniLM-L6-v2`
model anonymously. The encoder prints readiness only after loading and warming
the model, then listens at `http://127.0.0.1:8188`. Document text is encoded
locally. Keep that terminal running and start your built Minera server in another
terminal with `--embedding-runtime-url http://127.0.0.1:8188`. In **Search &
embeddings**, choose **Local transformer**, keep the matching model name, and
use vector dimensions **0** for its full 384-value vectors. Positive widths
truncate the model output; widths above the loaded model's width are rejected.
The service never substitutes hashing or mock vectors when inference fails.

Use `--model` to preload a different SentenceTransformer model or a local model
directory, `--revision` to pin a model revision, `--local-files-only` to use cached
files without downloading, or `--normalize` for normalized vectors. The service
loads safetensors weights and disables remote model code. `/health` reports the
loaded model and its actual vector width. The native CLI uses the same service:

```sh
./build/minera_export my_event_log.csv --embeddings \
  --embedding-provider transformer \
  --embedding-model sentence-transformers/all-MiniLM-L6-v2 \
  --embedding-dimensions 0 \
  --embedding-runtime-url http://127.0.0.1:8188 -o embeddings.json
```

Add `--embedding-variant events` for event vectors, or
`--embedding-target "approve a refund" --embedding-top-k 10` for ranked matches.
The item limit applies to all cases or events being encoded, rather than to a
sample. Very long text remains subject to the selected model's token limit.

CSV uploads using `case_id`, `activity` and `timestamp` do not need duplicate
canonical columns. The bundled token replay follows the imported activity
field when no replay selector is supplied. An explicitly supplied replay
activity field stays exact; a missing name produces a field error. Custom
CSV column names and delimiters use the existing import options.

**Model quality evaluation** also chooses the imported or mined activity field
when its Activity field is left blank. This applies to fitness, precision,
generalization and composite evaluation, including custom miner activity fields.
An entered Activity field remains an exact selector. The CLI's evaluation and
standalone token-fitness commands use the same defaults.

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

### Language-model providers

The **Ask a model** tab previews and edits a prompt built from process context,
a general description or a database schema. OpenAI, Anthropic and Google calls
run through the optional Python runtime. The application sends the displayed
prompt and selected attachments when **Send displayed prompt** is selected; the response and its original
provider payload can be downloaded as JSON.

Set the selected provider's credential in the runtime environment:
`OPENAI_API_KEY`, `ANTHROPIC_API_KEY`, or `GEMINI_API_KEY` (`GOOGLE_API_KEY` is
also accepted). Credentials stay in that process. Start the runtime and native
server in separate terminals:

```sh
python scripts/llm_runtime.py --port 8189
minera_server --llm-runtime-url http://127.0.0.1:8189
```

`OPENAI_BASE_URL`, `ANTHROPIC_BASE_URL` and `GOOGLE_BASE_URL` select compatible
provider endpoints. HTTPS is required for remote endpoints; local HTTP is
supported. `MINERA_OPENAI_TOKEN_FIELD=max_tokens` enables the legacy limit field
for compatible services; the default is `max_completion_tokens`. Model names
are supplied explicitly. Temperature is omitted unless selected, so the model's
default applies. The runtime uses only Python's standard library.

Standalone native execution uses the same prompt builder and provider runtime:

```sh
minera_knowledge --variant process_mining --description "Order workflow" --query "Explain review" --runtime-url http://127.0.0.1:8189 --provider openai --model MODEL --max-tokens 4096
```

Without `--runtime-url`, `minera_knowledge` retains its prompt-only JSON output.
Windows CLI arguments are converted from UTF-16 to UTF-8 so multilingual prompts
match HTTP inputs. The HTTP bridge exposes `GET /api/llm/status` and
`POST /api/llm/complete`; knowledge previews accept text multipart fields as well
as the existing query parameters. Provider calls are restricted to the local
application origins. Runtime limits are 2 MiB for request bodies and 4 MiB for
provider responses, with connection/read timeouts and explicit error results.

The adapters follow the documented text protocols for
[OpenAI Chat Completions](https://developers.openai.com/api/reference/resources/chat/subresources/completions/methods/create),
[Anthropic Messages](https://platform.claude.com/docs/en/api/messages/create), and
[Google generateContent](https://ai.google.dev/api/generate-content).
Automated checks use local HTTP protocol fixtures; external account access and
live model inference are not established by those checks. Provider parameters,
multimodal interfaces and exact reference behavior retain their feature and
parity gates.

### Image queries and additional provider payloads

**Ask a model** accepts up to eight PNG, JPEG, WebP or GIF attachments. The app
detects their media type from their bytes, displays previews, and retains the
complete submitted request in the response download. Native OCEL context and
images can be sent together. Image support also depends on the selected model
and provider; upstream failures remain explicit.

OpenAI offers **Responses** and **Chat Completions** in the app. The Responses
path handles answer text, refusals, incomplete responses and token usage.
Existing requests that omit `openai_api` use the runtime's configured default,
selected with `MINERA_OPENAI_API=responses|chat_completions`; its compatibility
default remains `chat_completions`. Anthropic and Google use their existing
Messages and generateContent paths with image content blocks.

Expand **Extra provider payload** to supply a JSON object using the provider's
field names. Its fields override generated fields, including model or prompt
content when supplied. Google generation controls belong inside
`generationConfig`; that whole object replaces the generated object. Downloads
preserve these overrides. Streaming and background responses need separate
consumers and are rejected by this synchronous runtime. Temperature controls
allow 0–2 for OpenAI/Google and 0–1 for Anthropic; leave the field blank for
model defaults.

```sh
minera_knowledge --variant description --description "Explain this process diagram" --runtime-url http://127.0.0.1:8189 --provider openai --model MODEL --openai-api responses --image process.png
minera_knowledge --variant description --description "Compare these diagrams" --runtime-url http://127.0.0.1:8189 --provider anthropic --model MODEL --image first.png --image second.png --extra-payload-file controls.json
python scripts/llm_runtime.py --complete request.json --image process.png
```

`--image` can be repeated; `--image-detail auto|low|high|original` supplies
OpenAI image detail. `--extra-payload` accepts literal JSON and
`--extra-payload-file` accepts a UTF-8 file. Image and extra-payload arguments
apply to completion commands with `--runtime-url`. The HTTP completion envelope
supports `images` (name, media_type, base64 data and optional detail),
`extra_payload`, and optional `openai_api`. The browser accepts individual
images up to 1.5 MiB; the complete serialized request must fit 2 MiB across
all entry points. Original bytes are retained.

The protocols follow [OpenAI Responses](https://developers.openai.com/api/reference/python/resources/responses/methods/create),
[OpenAI vision](https://developers.openai.com/api/docs/guides/images-vision?api-mode=responses),
[Anthropic vision](https://platform.claude.com/docs/en/build-with-claude/vision), and
[Google content generation](https://ai.google.dev/api/generate-content).
This is usable image transport and payload control. Exact pinned-reference
defaults, field omission, media-type conventions, saver adapters, response/error
behavior and per-call endpoint conventions remain independent parity gates.

### Automatic visualization explanations

Open a diagram, chart or table and select it in **Visualization to explain**.
**Prepare visualization explanation** renders that selected panel into a PNG
and opens it with an editable prompt in **Ask a model**. OCEL views open their
own question section. The image reflects the displayed filters, annotations,
pan/zoom and scroll state; review it before sending.

The renderer waits for fonts and finite entrance animations, retains SVG styles
and legends, and rejects blank output. It fits the complete selected panel
within a 1600-pixel dimension and a 4-million-pixel area, with an image bound of
1.5 MiB. Content already clipped by the selected view remains clipped. Rendering
uses `html2canvas` 1.4.1; exact browser pixels and reference saver layouts remain
separate parity gates. See its [rendering limitations](https://html2canvas.hertzen.com/documentation).

Standalone Python workflows accept a real saver callback. The saver receives
its positional arguments followed by the image path. Additional options are
forwarded, and the generated image is read before the owned temporary directory
is cleaned:

```python
from scripts.llm_visualization import explain_visualization

explanation = explain_visualization(
    save_my_model, model, connector=my_connector,
    description="A process model with activity frequencies.",
    annotated=True,
)
```

The connector receives the prepared prompt, `image_path` and the same options.
`prepare_visualization` returns a prompt and byte-preserving image attachment
without calling a provider. The command-line entry point accepts JSON arguments
and options and supports prompt-only or provider execution:

```sh
python scripts/llm_visualization.py --saver my_plots:save_model --args-file arguments.json --options-file controls.json --prompt-only
python scripts/llm_visualization.py --saver my_plots:save_model --args-file arguments.json --description "Process model" --provider openai --model MODEL --runtime-url http://127.0.0.1:8189
```

Response reports retain the exact submitted image and request. Provider/model
availability, large-view detail, automatic dispatch of every reference saver,
prompt defaults, temporary-file/error conventions and exact rendering fidelity
remain open independently of these usable entry points.

### Native log, model and OCEL contexts

In **Ask a model**, expand **Build log or model context** to build text from the
loaded native data. CSV/XES logs provide one case, directly-follows graphs,
variants, event streams, attribute statistics, case features, DECLARE, log
skeletons, temporal profiles and Petri nets. Original OCEL files provide events
and objects, object features and object-centric directly-follows graphs through
**Ask a model about this object-centric log**.

Inspect the result, select **Use built context**, enter a question and preview
the prompt. Both the native context and the reviewed prompt/response can be
downloaded. Using new context cancels an outstanding model request and clears
its old prompt/response. Native context construction requires no provider or
Python runtime.

Controls include headers, timestamps, case/event attributes, performance,
relative case frequency, two duration aggregations, case index, exact source
attribute names, model selection and object lifecycle paths. Object features
require an exact object type, such as `Order`. The character budget preserves
complete lines; a first line or header may exceed it. The result records the
actual character count and whether content was omitted, with a separate 2 MiB
output limit.

```sh
minera_context my_event_log.xes --variant dfg --relative-frequency true --primary-aggregation median
minera_context my_event_log.csv --variant case --case-index 0 --include-timestamps false
minera_context model.pnml --variant petri_net
minera_context orders.json --variant ocel_features --object-type Order
```

The CLI supports `case`, `dfg`, `variants`, `event_stream`, `log_attributes`,
`log_features`, `declare`, `log_skeleton`, `temporal_profile`, `petri_net`,
`ocel`, `ocel_features` and `ocel_ocdfg`. Petri contexts can use `inductive`,
`alpha`, `heuristic` or `powl` discovery, or an imported PNML document.
`POST /api/llm-context` accepts the same options with underscores in their
names. Traditional log requests require the current cached `source_id` from
`GET /api/log-query/schema`; OCEL requests upload the original JSON in the
multipart `ocel` field. Stale sources and invalid controls return explicit
errors. Saved model-export JSON alone does not contain the original log needed
for native context construction.

All 13 builders have usable native and browser paths. Exact reference wording,
ordering, types and parameter combinations remain parity gates; the current
public-reference byte comparisons cover default DFG and variant text only.

### SQL queries and filtered logs

Open a CSV or XES log and select **Query & filter**. The source schema lists the
columns in the `dataframe` table. Native SQLite executes manual SQL independently
of the language-model runtime. **Generate SQL** prepares an editable query;
**Generate and run** also executes it. Generation sends the displayed schema
and instruction prompt to the selected configured provider.

The query table supports pagination and JSON downloads. To export a filtered
log, return the displayed source event identity column, for example with
`SELECT *`. **Whole cases** retains every event in cases containing a match;
**Events only** retains the matched events. XES and typed table JSON preserve
the original event order and attributes. **Open filtered log** imports the
filtered XES into the app. Display row limits do not truncate the selection used
for export. Source revisions prevent an old query from filtering a newly loaded
log. Saved JSON exports require a CSV/XES upload to provide an active query source.

Standalone native queries use the same SQLite engine:

```sh
minera_query my_event_log.csv --sql "SELECT COUNT(*) AS events FROM dataframe"
minera_query my_event_log.csv --sql-file selection.sql --filter-output filtered.xes --mode cases
```

DuckDB is an optional real SQL runtime. Install its pinned dependencies and run
the runtime and native server in separate terminals:

```sh
python -m pip install -r scripts/requirements-query.txt
python scripts/query_runtime.py --port 8192
minera_server --query-runtime-url http://127.0.0.1:8192 --llm-runtime-url http://127.0.0.1:8189
minera_query my_event_log.csv --engine duckdb --runtime-url http://127.0.0.1:8192 --sql-file selection.sql
```

SQLite retains timestamp cells as source text; DuckDB loads typed timestamps
and supports functions such as `EPOCH`. Result JSON tags exact large integers,
decimals, dates and blobs where needed to avoid loss in the browser. SQL accepts
one read-only result query over the loaded data. The optional runtime bounds
table requests to 32 MiB and result responses to 16 MiB.

The HTTP bridge exposes `GET /api/log-query/schema`, `POST /api/log-query` and
`POST /api/log-query/export`. SQL is a UTF-8 text body; query parameters select
`engine`, display `limit` and `source_id`. Filter export requires `source_id`
and accepts `mode=cases|events` and `format=xes|json`.

### Case clustering and executable hypotheses

In **Ask a model**, choose **Case clustering** or **Hypotheses** from **Model
task**. Both workflows show an editable prompt and accept an existing model
response. Generation can prepare definitions for review or execute them directly.
Parsed names, expressions, descriptions and SQL remain editable. Study reports
include definitions, results/errors and the original provider response.

Case clusters use a fenced JSON list with `name` and `regex` fields. Expressions
search complete activity sequences joined with ` -> ` using
[Python regular expressions](https://docs.python.org/3.11/library/re.html).
Every matching case retains all its original events, and cases can belong to
multiple clusters. Each cluster can be downloaded as XES or typed table JSON,
or opened in the app. Table exports retain source columns for empty clusters and
sparse attributes. Matching uses the query runtime's standard-library worker;
the worker stops after 15 seconds if an expression cannot finish.

Hypotheses pair `<description>...</description>` tags with fenced SQL blocks.
Execute all queries or one edited query through native SQLite or configured
DuckDB. A failed query leaves its error alongside successful results, including
queries following the failure. Display limits are explicit in the result and
downloaded report. Pasted hypotheses can run without a language-model provider.

Standalone workflows use the same native consumers. Supply their executable
paths with `--query-cli` and `--cluster-cli` if they are not on `PATH`:

```sh
python scripts/llm_study.py my_event_log.csv --task clusters --response-file clusters.txt --query-runtime-url http://127.0.0.1:8192 --output-dir clusters
python scripts/llm_study.py my_event_log.csv --task hypotheses --response-file hypotheses.txt --engine sqlite
python scripts/llm_study.py my_event_log.csv --task hypotheses --provider openai --model MODEL --llm-runtime-url http://127.0.0.1:8189
minera_cluster my_event_log.csv --regex "Review" --name "Review cases" --runtime-url http://127.0.0.1:8192 --filter-output review.xes --table-output review.json
```

`--obtain-query false` returns the prompt without a provider call;
`--execute-query false` returns parsed definitions without execution.
The native cluster CLI accepts `--activity-attribute` for an explicit source
field. The cached-log HTTP route is `POST /api/log-cluster`, with a literal
UTF-8 regex body and `source_id`, optional `name`/`activity_attribute`, and
`format=summary|xes|json`. Source revisions reject stale cluster requests.

### ILP and genetic discovery

In **Mine a Process Model**, select **ILP Miner** or **Genetic Miner**, then
**Classic**. Both run the native miner on the active CSV/XES source and render
its actual Petri net with local activity labels and initial/final markings.
ILP exposes activity/boundary labels, alpha, noise filtering, causal relations
and its search bound. Genetic discovery exposes activity/timestamp/case fields,
population, generations, seed, evolutionary rates, tournament size and
activity/state bounds. Zero generations retains the initialized population's
result. Diagnostics show the actual work and returned fitness; bounded search
does not guarantee a perfectly fitting model.

Inspect genetic causal-matrix input/output groups alongside its workflow net.
Members within a group are alternatives; separate groups require independent
tokens. Download the complete discovery report or the exact PNML returned with
the model. Reports retain reviewed parameters and original native JSON text.
**Check this model** opens the existing conformance workflow with that PNML,
the source revision and selected activity field. Valid custom fields follow
through token replay, footprints, default alignments and model quality.

`POST /api/ilp-mine`, `POST /api/genetic-mine` and the existing
`POST /api/genetic-matrix-convert` accept `include_pnml=true` with `source_id`.
The `DISCOVERY_MODEL` envelope includes the ordinary `result` and its `pnml`
(null when discovery did not produce a model). Calls without the option retain
their previous result shape. `POST /api/check-model` accepts `source_id` and
`activity_attribute` for the handoff; stale revisions are rejected. Native CLI
entry points remain available:

```sh
minera_export source.csv --ilp-mine --ilp-alpha 1 --ilp-max-nodes 100000
minera_export source.csv --genetic-mine --genetic-seed 17 --genetic-population-size 32 --genetic-generations 20
minera_export source.csv --genetic-matrix-convert --genetic-conversion-variant to_petri_net
```

These views add usable entry points while both packages retain their partial
parity status. Complete reference optimizer/search/parameter behavior and input
fallback conventions remain independent gates. In particular, a missing named
activity can fall back during native discovery, while a handoff's selected field
must exist; inspect the resulting labels and use a valid field for conformance.

### Anonymization

Open a CSV or XES log and choose **Anonymize**. Release activity variants with
**LAPLACE** or **SACOFA**, inspect the released sequences/cases, and optionally
run **PRIPEL** on that release to add anonymized event attributes. PRIPEL also
accepts a separately released CSV or XES query log. Its attribute blocklist uses
one exact name per line, including names containing commas or backslashes.

Both stages expose epsilon, output limits and OS-entropy sampling or a seeded
preview. Variant queries also expose prefix length, pruning and candidate
limits; PRIPEL exposes matching distance. Empty/capped results are explicit.
Cases, variants and long event lists have paginated inspection. Download the
complete release report or its XES, or open the XES through ordinary mining.
These actions retain the output of the same run, including its sampled values.
Report downloads retain the original JSON text and exact large integer seeds.

The native routes remain `POST /api/trace-variant-query` and `POST /api/pripel`.
Adding `include_xes=true` and the active `source_id` returns an
`ANONYMIZATION_RELEASE` envelope containing the ordinary `result` and
`released_xes`. Stale source revisions are rejected. PRIPEL accepts a multipart
`trace_variant_query` file. Calls without the release option keep their previous
result shape. Standalone JSON execution remains available:

```sh
minera_export source.xes --trace-variant-query --tvq-variant sacofa --tvq-epsilon 1 --tvq-k 4 --tvq-secure-random true
minera_export source.xes --pripel released-query.xes --pripel-epsilon 1 --pripel-blocklist customer,resource --pripel-secure-random true
```

PRIPEL replaces imported typed scalars when their sampled values change and
represents fractional values from integer inputs as floats, so XES export does
not restore original attribute values. Algorithm distribution, parameter and
privacy guarantees remain independent parity gates in the catalogue; this
workspace adds usable entry points without marking those packages complete.

### Native clustering

Open **Clustering** with an active CSV/XES source. **Trace-attribute hierarchy**
groups cases by a trace attribute and exposes all five native distance methods:
`DMM_LEVEN`, `AVG_LEVEN`, `DMM_VEC`, `AVG_VEC` and `DFG`. Merge distance scales
the dendrogram horizontally. Select a graph node or a row, focus its subtree,
and inspect the exact source case positions. Case positions distinguish repeated
case IDs and path IDs distinguish groups whose names collide with merge labels.
The diagram shows up to 120 leaves per subtree; paginated tables and exports
retain complete results. Missing grouping attributes yield an empty hierarchy.

**Case profiles** exposes `sklearn_profiles`, cluster count, iterations, seed,
tolerance, trace/feature bounds, activity/succession presence representations,
and exact trace/event string and numeric selectors. Blank selector lists retain
native automatic selection. Numeric event selectors require a value in every
trace, so an empty trace produces an explicit error when such a field is selected.
Inspect each cluster's feature names and native centroid values.

Download the parameter-bound report, the selected XES or a typed event table, or
open the selected XES for ordinary mining. Selections copy complete cached
traces, including trace attributes and empty traces. Event tables contain event
rows and therefore do not represent empty traces. The ordinary XES import path
currently caches timestamps at microsecond precision; selection export retains
that cached precision. Empty selections export successfully but cannot be opened
for mining. Saved analysis JSON cannot cluster a different cached source.

The existing `POST /api/clustering` and `POST /api/profile-clustering` routes
retain their legacy JSON shapes. Add `source_id` and `include_cases=true` to
receive `CLUSTER_ANALYSIS` with source/analysis revisions, positional case
metadata and native memberships. The optional profile result also includes
native centroids. `POST /api/clustering/export` accepts these two revisions,
`selection=r/0/1` for a hierarchy path or `selection=c/0` for a profile cluster,
and `format=xes|json`. Exports use the cached analysis. A new source or analysis
rejects old selections. Standalone native execution remains available:

```sh
minera_export source.xes --clustering --clustering-trace-attribute segment --clustering-variant AVG_LEVEN
minera_export source.xes --profile-clustering --profile-clustering-cluster-count 2 --profile-clustering-seed 17
```

Both clustering packages retain their partial parity status. Exact distance,
feature extraction, optimization, selector, ordering and scalability behavior
remain separate catalogue gates.

### Feature, target and label transformations

Open **Transform → Features, targets & labels** with an active CSV/XES log.
**Feature matrices** exposes
`TRACE_BASED`, `EVENT_BASED` and `TEMPORAL`, including calendar frequencies and
fixed windows. Configure activity/case/time/resource fields, eight exact
attribute/schema lists, numeric defaults and all ten extra case feature families.
Choose Native default, an explicit JSON list or an explicit empty list. Case
schema selection depends on explicit ordinary attribute selectors; automatic
selection can replace a supplied schema, and extra families can append columns.
Individual Enabled/Disabled overrides take precedence over the all-extra switch.
Resource workload rejects empty traces; disabling that family permits the other
extra features on the exercised fixture.

Inspect exact source case positions, identifiers and native values. Rich case
identifier cells retain large integers and metadata. Event matrices include
native padding to the longest case; padding rows are marked in the table and
CSV, including rows belonging to empty traces. Temporal rows retain exact epoch
nanosecond labels when available and native nonfinite spellings such as Infinity.
The table pages through 25 rows and eight dimensions. Duplicate and empty
feature names keep their column positions; downloads retain the complete result.

**Prediction targets** exposes next activity, next-event time and remaining case
time. Explicit activity classes retain their supplied order. Time targets use
seconds. Optional padding must cover the longest case and is limited to 100,000
rows per case in the workflow; zero rows distinguish terminal/padding values
through their event position and row kind.

**Contextual label splitting** exposes source/output fields, prefix/suffix
lengths, edge threshold, separator and selected activities. The table compares
the input label, previous output field and actual new field. Download the same
run's XES or open it using the selected output activity field. Custom imported
fields remain selected when re-mining with filters, and their names retain
whitespace and Unicode. A newly introduced field may be absent on unchanged
events; its complete XES remains downloadable, while reopening requires a
nonempty output activity field for every event.

**Download transformation data** preserves the original native response JSON;
the complete report adds its requested parameters. CSV is a flat scalar view,
with source positions and padding/bucket metadata. The JSON retains richer
types, exact identifiers and the original native JSON text. Saved analysis JSON
cannot transform another cached source. Changed source revisions are rejected;
failed runs retain the preceding usable result.

Legacy routes remain `POST /api/log-to-features`, `POST /api/transform-target`
and `POST /api/label-splitting`. Add the current `source_id` and
`include_source=true` for a `LOG_TRANSFORMATION` response with case positions
and the ordinary native `result`. Label responses also include `transformed_xes`,
`label_rows`, `activity_attribute` and `complete_activity_field`. Standalone
execution uses the same native algorithms:

```sh
minera_export source.xes --log-to-features --features-variant event_based --features-event-numeric-attributes cost
minera_export source.xes --transform-target --target-variant remaining_time --target-enable-padding true --target-pad-size 20
minera_export source.xes --label-splitting --label-splitting-target-activities Review --label-splitting-prefix-length 1 --label-splitting-suffix-length 0
```

These three standalone modes also accept `-o result.json`. Windows CLI argument
values and filesystem paths are decoded as UTF-8, retaining Unicode classes,
input/output filenames and dataset names in the exercised ordinary XES/JSON paths.

The feature and contextual-label packages retain their partial parity status;
target transformation retains its existing complete catalogue status. App
availability does not promote exact algorithm, selector, schema, numeric or
scalability parity.

### Log preparation

Open **Transform → Prepare logs & event data** with an active CSV/XES log.
**Data conversions** offers event logs, event streams, event tables, attributed
graphs, one trace per variant and all traces grouped by variant. Controls expose
case fields/prefixes, present-null removal, native copy/compression/lazy flags,
variant metadata, graph attribute nodes, directly-follows edges and output bounds.
Copy/compression switches retain native value-copy behavior; they do not gzip
JSON downloads. Graph drawing shows at most 200 nodes; full node/edge tables and
downloads retain the complete native graph, including its identifier collisions.

**Case utilities** offers seeded training/test splits, fixed-length prefixes,
all prefixes, prefixes before one or several activities, and attribute selection.
Training chooses up to `floor(case_count × percentage) + 1` cases, capped at the
total. All-prefix output retains repeated and empty prefixes and native change
indices. Activity prefixes exclude the target; cases with the target first or
absent are excluded. For several targets, cases containing another selected
target are excluded and target logs with no remaining cases are omitted.

Select the actual native training, testing or activity output. Inspect event
rows and cases on independent pages, including empty traces and repeated case
IDs. Positions belong to the selected output; they do not identify original
source positions. JSON/data and parameter-bound reports preserve the original
native response text. CSV retains all scalar rows and columns. Each log output
has a same-run typed XES download and can reopen through its actual activity
field. Empty/incomplete activity outputs remain downloadable; reopening requires
events with a complete nonempty activity field. Saved analysis JSON disables
preparation, source changes reset results, and failures retain the previous run.

Existing `POST /api/log-convert` and `POST /api/log-util` result shapes remain
available. Add query parameters `source_id` and `include_source=true` for a
`LOG_PREPARATION` response containing `result` and ordered `outputs`; each log
output includes its stable ID, counts, selected activity field and exact XES.
Standalone execution uses the same native algorithms:

```sh
minera_export source.xes --log-convert to_data_frame -o events.json
minera_export source.xes --log-convert to_nx --log-conversion-event-attributes-as-nodes cost
minera_log_util source.xes --train-percentage 0.5 --seed 9
minera_log_util source.xes --prefix-length 2
minera_log_util source.xes --to-activities "Approve\, 東京,Archive β"
```

The log-utility CLI now preserves Unicode arguments and UTF-8 filesystem paths
on Windows. Exact reference ordering, defaults, scalar/container types, errors,
selector names and scalability remain parity gates. These preparation workflows
extend feature availability without changing the algorithm catalogue's scope or
parity statuses.

### Trie, net and interval transformations

Open **Transform → Tries, nets & intervals** with an active CSV/XES log.
**Prefix trie** exposes the activity field and optional retained path length.
Inspect exact labels, terminal flags, native child order and path JSON by stable
child-position addresses. Repeated labels on separate branches remain distinct.
Native discovery skips empty paths; input-case counts still include empty traces.
Truncation makes retained endpoints terminal. The diagram supports selection,
zoom and pan, while tables and CSV retain every node beyond the displayed page.

**Trie to Petri net** discovers and converts once, returning that same trie, net
and native PNML. Inspect visible/silent transitions, input/output places and
source/sink markings. Download the exact PNML or use **Check trie model** to
hand it to conformance with the source revision and activity selector. The
primary `activity` alias maps to replay's canonical `concept:name`; literal
custom selectors remain unchanged. Replay shows actual outcomes, including the
empty-trace limitation, without implying perfect fitness.

**Open-path intervals** exposes activity, target-start and source-completion
fields, nonnegative boundary epsilon, an optional activity pair, and simultaneous
point/range queries in epoch seconds. The native algorithm uses the first
non-earlier following start for each source completion, then applies the pair
filter. Matching is inclusive. Select a native interval on the timeline or table,
inspect its original trace/event indices, and reuse exact boundaries as queries.
Point/range views use the returned native match indices. **Download all intervals
CSV** keeps every returned interval regardless of the current display filter.
Numeric boundaries and queries retain double precision; UTC diagram labels
are formatted to milliseconds.

Complete native JSON and parameter-bound reports retain the original response
text. Tables use 25-row pages. Source changes reset results, saved analysis JSON
disables native operations, and failures retain the previous usable result.
The existing GET routes `/api/trie`, `/api/trie-to-petri` and `/api/interval-tree`
accept `source_id` and `include_source=true` query options for a
`STRUCTURAL_TRANSFORMATION` envelope with source cases and the ordinary native
`result`. Trie conversion additionally returns its same-run `trie` and `pnml`.
All three standalone modes now honor `-o`, including Unicode Windows paths:

```sh
minera_export source.xes --trie --trie-max-path-length 2 -o trie.json
minera_export source.xes --trie-to-petri -o net.json
minera_export source.xes --interval-tree --interval-filter-activity-couple "Review,Approve\, 東京" -o intervals.json
```

The new workflows retain existing catalogue parity statuses. Reference
ordering/default/selector/type/error and scalability gates remain independent.

### OCEL analysis

Open an OCEL JSON log and choose **Analyze objects & events** beside the
object-centric process models. The original uploaded file drives all sixteen
analysis variants:

- **Feature matrices** for events, objects and event/object relations. Native
  defaults or explicit Enabled/Disabled controls cover 36 feature switches,
  object presets and related-object context. Set attribute names with JSON lists,
  including literal comma/Unicode names, and choose column limits, name style,
  object type, interaction source or neighbor graph where applicable.
- **Object-centric statistics** for activities by type, object counts per event,
  activity associations, associations by type and edge associations/performance.
  Association filters select lifecycle starts/ends. Edge performance can measure
  business hours with native or explicit weekly active slots.
- **Log descriptions** as native text or event/object tuples, with optional
  timestamps. Object tuples preserve lifecycle booleans and empty orphan bounds.
- **Object relationship graphs** for interaction, descendants, inheritance,
  co-birth and co-death, including isolated objects and empty edge sets.
- **Event/object/change graph** with native E2O, qualified O2O, object-local
  directly-follows and change edges. DF edges and change nodes can be switched off.

Tables retain native positions, identifiers and types, including repeated
relation rows. Row and column pages are independent; **Download complete selected
CSV** retains every entry. Edge statistics export all seven native metric arrays.
Graph nodes support inspection, zoom and pan; the drawing keeps its existing
120-node/360-edge bounds while tables and downloads retain complete native data.

**Download native OCEL data** preserves the response JSON verbatim. Reports bind
the original file's name, byte size and SHA-256 to the request and response.
Inspection and CSV preserve native numeric literals, including integers beyond
JavaScript's safe range. They do not reverse native OCEL source-type conversion.
Absent graph change-value cells display `null`; raw JSON retains field absence.
Source changes reset results, and failed runs retain the preceding usable result.

The same analysis families remain available in `minera_ocel`. Windows filenames
and arguments use UTF-8; feature attribute lists accept escaped delimiters:

```sh
minera_ocel source-東京.json --features events --features-string-events 'region\, 東京'
minera_ocel source.json --statistics activity_associations --statistics-prefilter start
minera_ocel source.json --describe variant2 --no-description-timestamps
minera_ocel source.json --graph heterogeneous --graph-include-df false
```

[Validation and checkpoint boundaries](docs/feature-first-ocel-analysis-validation.json)
track feature delivery separately from full reference parity. OCEL preparation,
comparison, simulation and file exchange have their own workflows below;
remaining model/export controls and exactness/performance gates continue in the
feature ledger.

### OCEL preparation

With an OCEL JSON source open, choose **Prepare object-centric data**:

- **Filter OCEL** exposes all fifteen native filters: event/object attributes,
  object types/IDs, event IDs, timestamp windows, activity/type matching, object
  counts, lifecycle starts/ends, activities connected to a type and components
  selected by object, size, type or activity. Applicable controls include selection
  polarity, neighborhood level and literal JSON lists/maps.
- **Split OCEL** exposes connected components and ancestors/descendants. Choose
  degree centrality for components, or central type and maximum returned central
  objects for ancestors/descendants. Native candidate counts, returned counts,
  central objects and truncation remain visible; projection pages keep positions.
- **Enrich relationships** selects five graph families and lifecycle qualifiers
  independently. Native defaults remain available. Clearing every graph choice
  requests no graph enrichment, with lifecycle qualification still selectable.
- **Flatten to a classic log** selects an object type and optional activity/time
  attributes. Download its exact native XES or open it in the classic workspace.
  Inspection retains emitted typed fields, trace positions and repeated relation
  occurrences. Empty projections remain downloadable.

Attribute filters use typed JSON entries. Integer and decimal text preserves
exact values; count/size fields support the native unsigned integer transport:

```json
[{"type":"string","value":"West"},{"type":"integer","value":"9223372036854775807"},{"type":"decimal","value":"12.5"},{"type":"boolean","value":false},{"type":"null"}]
```

Prepared tables retain complete event, object, relation, object-link, change,
event-link and metadata records. **Download complete prepared CSV** includes
every selected row, regardless of its page. Reports bind the current source's
filename, byte size and SHA-256 to the actual parameters and complete native
response. Failed runs retain the preceding usable result.

**Download selected prepared OCEL** and **Open selected prepared OCEL** use the
same-run native OCEL 2.0 source. This retains repeated event/object relationships
and qualifiers when opening the result; the classic inspection representation
remains in the response and tables. The selected output becomes the new source.

The filter, split and enrichment POST routes accept `include_open_source=true`
for additive `open_log` or position-aligned `open_logs` documents. Ordinary
responses retain their existing shapes when the option is absent. Enrichment
accepts `graphs=none`; an omitted selection uses defaults. The existing CLI also
supports no graph enrichment:

```sh
minera_ocel source.json --enrich none --enrich-no-lifecycle
minera_ocel source.json --split ancestors_descendants --split-object-type Order --split-max-objects 1
minera_ocel source.json --flatten-object-type Item
```

[Validation and checkpoint boundaries](docs/feature-first-ocel-preparation-validation.json)
retain native classic-format duplicate-pair conventions, isolated-component
behavior and remaining exactness/performance gates. Remaining I/O/model controls
continue in the feature ledger.

### OCEL comparison and simulation

Choose **Compare & simulate OCEL** with an object-centric source open.

- **Conformance comparison** supports event-to-object-type (ETOT), object-centric
  directly-follows (OCDFG) and object-type relations (OTG). Compare the current
  original OCEL or an uploaded native graph with a normative OCEL or graph.
  Weights, thresholds and applicable discovery selectors retain native defaults
  when blank. Fitness and missing/additional categories remain separately
  inspectable; a fitness of one can coexist with additional activities or edges.
- **Model playout** discovers an object-centric Petri net or causal net from the
  current original source. Select independent object firings, shared activity or
  synchronized binding. Discovery variants/annotations/selectors, object IDs by
  type, search/branching bounds, full unsigned 64-bit random seeds, time spacing
  and optional binding/fired-transition diagnostics are configurable. Petri-net
  initial/final token vectors use the place positions in the same-run model.

**Download OCEL execution report** binds both actual comparison files, or the
original simulation source, by filename, byte size and SHA-256. Native data,
model records, binding sequences and fired transitions retain their complete
JSON. **Download complete execution CSV** includes every row of the selected
category beyond the 25-row page. Diagram indices belong to the actual discovered
model returned by that run; causal marker groups remain in its model inspection.

**Download simulated OCEL** and **Open simulated OCEL** use the same-run native
OCEL 2.0 source. Opening it makes the generated file the active source in the
existing model and analysis workflows. Empty simulations remain downloadable;
their opening action is disabled. Failed runs retain the preceding result.

Results reflect the supplied search bounds and branching choices. Reachability
mode returns the native existence result and may produce an empty log; it does
not establish unbounded reachability. Explicit per-net markings apply to Petri
playout; causal marker search uses its own groups. Conformance controls accept
signed finite numbers, with nonfinite native conventions retained as a gate.

`POST /api/ocel/playout` accepts optional `include_model=true` and
`include_open_source=true` for additive same-run `model` and `open_log` fields.
Ordinary responses retain their existing shape without these flags. The existing
standalone CLI supports comparisons and simulation:

```sh
minera_ocel source.json --conformance etot --normative normative.json
minera_ocel source.json --ocpn-playout --ocpn-event-binding synchronized --ocpn-max-traces 4 --ocpn-return-traces
minera_ocel source.json --oc-causal-net --ocpn-event-binding shared_activity --ocpn-max-traces 4 --ocpn-return-sequences
```

[Validation and checkpoint boundaries](docs/feature-first-ocel-execution-validation.json)
record all twelve comparison input combinations, six model/binding combinations,
native roundtrips, source handoff, paging and complete exports. This delivers the
workflows while keeping the complete parity catalogue and remaining exactness,
search-exhaustiveness, platform and performance gates open.

### OCEL file exchange

**Import OCEL JSON, XML, CSV or SQLite** is available from the landing page.
**Import OCEL data** is also available from classic and object-centric views;
returning from import preserves the current workspace. With an OCEL source open,
choose **Import & export OCEL** for original-source export.

- Import classic or standard JSON/XML, extended event CSV with an optional
  object CSV, or OCEL 2.0 SQLite. Preview complete positional events, objects,
  relationships, changes, links and metadata. Their CSV download includes every
  selected row beyond the 25-row page.
- Export classic or standard JSON/XML, SQLite, event CSV, object CSV or a paired
  event/object CSV. Downloads retain the exact native bytes. The paired option
  uses the native two-table writer, retaining object attributes and objects with
  no events without opting into loss.
- Configure six literal CSV column/prefix names, comma/semicolon/tab delimiters,
  trimming, timestamp sorting, unsupported-field handling and explicit loss
  allowance. XML controls include unsupported constructs, byte limit,
  declaration and writer allowance. SQLite controls include reference validation
  and readable type-table names. Blank/default selections use native defaults.

Reports bind each original input file and every output by filename, byte size
and SHA-256. Imports additionally retain the complete native response. **Open
imported OCEL** uses its same-run standard source, preserving the native import
result when it becomes the active source for models, analysis and preparation.
Empty imports retain downloads; their opening action is disabled. Native errors
retain the preceding usable result.

CSV requires headers and cannot carry qualifiers, object/event links, object
changes or globals. Event CSV alone also cannot retain object attributes. Loss
allowance is explicit and does not bypass duplicate relation errors. Classic
JSON reimport follows the native duplicate-pair normalization convention;
standard JSON, both XML formats and SQLite retain the full rich controlled
fixture. XML writer allowance does not bypass native JSON-source validation.
Broader type, dialect, ordering, format and performance parity remain open.

`POST /api/ocel/io` imports accept `include_open_source=true` for an additive
`open_log`. CSV export with `format=csv&include_objects=true` returns the two
native streams as `events_csv` and `objects_csv` in an `OCEL_IO_EXPORT/CSV_PAIR`
response. Other export responses retain their original shapes. The SQLite route
accepts the same optional opening source plus `validate_references` and
`readable_type_table_names`. Existing CLI format entry points include:

```sh
minera_ocel source.json --format standard
minera_ocel source.json --format xml_standard
minera_ocel source.json --format csv --ocel-csv-output events.csv --ocel-csv-objects-output objects.csv
minera_ocel events.csv --csv-objects objects.csv --format standard
minera_ocel source.json --sqlite-output export.sqlite
minera_ocel export.sqlite --format standard
```

[Validation and checkpoint boundaries](docs/feature-first-ocel-io-validation.json)
record native file/CLI comparisons, rich and standard-source roundtrips,
source handoff, complete exports and phone controls. File exchange delivery does
not promote any algorithm-family parity status.

### Related logs and events

Choose **Relate logs and events** on the landing page, **Relate logs** in the
top bar, or **Relate logs & events** in the classic transformation workspace.

- **Link events in the active log** selects outgoing/incoming values, sorting,
  the generated event-index field, look-forward behavior, first occurrence and
  propagation. Native event indices drive the diagram and endpoint inspector.
  The request binds the active mined source revision; a changed backend source
  raises an explicit error and retains the preceding result.
- **Timestamp interleavings between logs** uploads both original CSV/plain XES
  files and a case-relation CSV. Configure its timestamp and relation selectors
  and safety bound. Native LR/RL records retain activities, times, indices and
  differences, including artificial boundary rows.
- **Merge by case relations** uploads both original logs and their relation
  table. Select case/timestamp fields and relation columns or suffixes. Explicit
  relation columns can use empty unused suffixes. Repeated relations and extra
  relation columns remain in the complete native table.
- **Format a native event table** accepts native table/result JSON or a complete
  native formatting request. Configure case, activity, timestamp, optional start
  and timestamp-format selectors. **Use uploaded formatting options and exact
  request bytes** preserves a complete request's literal options, including empty
  selectors and native nullable values.

Reports bind all input filenames, byte sizes and SHA-256 values, active-source
metadata and actual formatting payload hashes. Raw native JSON retains complete
tables, dtypes, indices, categories and metadata. Inspection pages through rows
and columns; **Download complete related-log CSV** includes every selected entry.
Readable row views label absent/null cells. Typed cell CSV and raw JSON preserve
the distinct states, scalar/container kinds, lexical values and nested records.

**Download merge formatting table** uses the native canonical serialization of
the same merged table; **Format merged table** preloads it and common selectors
for inspection. The legacy merge-inspection metadata remains separately available.
Native formatting can omit rows missing selected required fields; compare its
formatted and `input_after` tables. Failed formatting retains the previous result
and exposes its native failed input state and report.

**Download merged XES** comes from the same native event-log projection.
**Open merged event log** activates that source with its actual activity field;
opening is disabled for unusable, empty or nonuniform activity projections.
The full merged table remains available when log projection fails. Independent
uploads use their original bytes and are independent of active-view filters.

Native result limits support unsigned 64-bit text and can raise safety errors;
they do not imply silent truncation or exhaustive discovery. Input-format
timestamp normalization, microsecond truncation in plain XES, CSV header trimming,
dtype/selector/index/null conventions and scalability remain parity gates. Timing
differences are displayed as returned by the native algorithm.

The merge route accepts optional `include_open_log=true` and
`include_format_table=true` for additive XES/projection metadata and a canonical
typed table. Link analysis accepts `source_id` and `include_source=true` for a
guarded revision in its result. Ordinary responses retain their existing shapes.
Existing CLI entry points include:

```sh
minera_export left.csv --link-analysis --link-analysis-out-column out --link-analysis-in-column in
minera_export left.csv --timestamp-interleavings right.xes --case-relations relations.csv
minera_export left.csv --merge-case-relations right.xes --case-relations relations.csv
minera_format_dataframe format-request.json --output formatted.json
```

[Validation and checkpoint boundaries](docs/feature-first-related-logs-validation.json)
record all four CSV/XES combinations, eight link-switch combinations, native
CLI comparisons, source guards, handoffs, paging and complete exports. These
workflow annotations preserve every existing catalogue parity status.

### Replay event streams

Choose **Replay event streams** on the landing page, **Event streams** in an
open workspace, or **Replay event streams** under **Transform**. Upload a CSV
event sequence and select streaming DFG discovery, footprint prefix checks,
temporal prefix checks, or forward/backwards token replay with a PNML model.
**Use current original CSV for streaming** selects the original upload.

The workflow defaults to **CSV input order**, preserving parsed arrival order
across cases. **Case-grouped importer order** groups cases and sorts their events
by timestamps when available, matching the existing HTTP/CLI convention. Every
run creates a fresh monitor over the selected file. These runs are independent
of current workspace filters and the preceding streaming result.

Footprint controls accept JSON activity lists and sequence/parallel pairs.
Empty start/end constraint text omits that check. Temporal profiles accept
`[source, target, mean seconds, standard deviation seconds]` tuples with zeta
and start/completion column selectors. Compare parsed and processed counts:
the native temporal monitor can skip invalid or missing timestamps.

Token replay exposes its two variants, stop-unfit option and invisible-search
limit, including exact unsigned 64-bit decimal values. **Finalize all cases
after replay** performs native end/final-marking checks for footprints and
token replay. Prefix cases remain separately inspectable alongside termination
fit, missing/remaining and marking results. Finalized cases leave `open_cases`.

Inspect the DFG diagram and native tables, then download complete native JSON,
a source-bound report or the selected category as CSV. CSV exports include
every row beyond the 25-row page and retain empty table schemas. Raw JSON keeps
the complete native response bytes; object-map tables use JavaScript key order.

The four multipart streaming routes accept `streaming_event_order=input|imported`
and opt-in `streaming_include_diagnostics=true` for parsed/order metadata and
prefix/termination diagnostics. Token replay additionally accepts
`streaming_tbr_terminate_all=true`. Ordinary responses retain their current
shapes. Existing commands use importer order:

```sh
minera_streaming_dfg events.csv
minera_streaming_footprints events.csv --model-activities A,B,C --model-start A --model-end C --model-sequence "A->B,B->C" --terminate-all
minera_streaming_temporal events.csv --profile "A->B:5:1" --zeta 2 --start-timestamp-column start
minera_streaming_tbr model.pnml events.csv --variant backwards --max-invisible 10
```

The current adapters require comma-separated CSV with a header and valid case/
activity identifiers, delimiter-compatible model activities, and a PNML workflow
net with single source/sink markings. Silent search remains bounded. Persistent
stream state, live readers/observers, general PNML documents, unbounded response
counters, cancellation of native computation and performance remain separate
work. [Validation and checkpoint boundaries](docs/feature-first-streaming-validation.json)
record the four workflows, both replay orders, complete exports and phone controls.
Full reference parity remains active.

### Inspect and convert model files

Choose **Inspect and convert model files** on the landing page, **Model files**
in an open workspace, or **Inspect & convert model files** under **Transform**.
PNML, BPMN, PTML and classic DFG uploads work independently of an event log.
Select inspection or a requested PNML/BPMN/PTML/DFG export.

PNML controls expose native initial/final inference, stochastic-map import,
workflow-boundary requirements and public marking semantics. **Preserve general
PNML markings** keeps omitted markings as zero vectors. Inspect full token
vectors and explicit keys, node IDs, arc occurrences, direction, type and exact
signed weights. Native integer values beyond JavaScript precision remain exact
in tables, raw JSON, reports, CSV and PNML downloads.

PTML inspection retains operators, nested hierarchy and child positions, with
standard or transition-bordered net construction. DFG offers activity-defined
places or invisible transitions without duplicate labels. Artificial boundary
identifiers apply to the latter construction for dictionary/collision checks;
their native boundary transitions are silent.

A failed conversion retains imported data and same-run PNML. The current
BPMN/PTML converters require single source/sink token markings and supported
ordinary arcs, and strict reverse-tree conversion requires a supported block
structure. BPMN inspection shows the native Petri projection; **Download
validated BPMN source** preserves the original source bytes separately from
a requested native semantic re-export.

Graphs and paginated tables keep native indices, external IDs and labels
distinct. Download every row and column as CSV, complete native JSON, a report,
the original upload, normalized source or requested converted document.
**Check exchanged model against current log** binds the current native source
revision and passes same-run PNML to Conformance while preserving zero/omitted
markings. This action needs an open CSV/XES source.

`POST /api/model-exchange` accepts multipart `model`, `format` and `output`
(`inspect` by default), plus the displayed native PNML/DFG/tree options.
The existing model-check route accepts optional
`pnml_infer_legacy_boundaries=false`; its historical inference default stays
available. The existing CLI inspector and strict conversion include:

```sh
minera_export --inspect-pnml model.pnml --pnml-import-stochastic-map true
minera_export --inspect-pnml model.pnml --pnml-infer-initial-marking false --pnml-infer-final-marking false --pnml-require-workflow-boundaries false
minera_export --pnml-to-ptml model.pnml -o converted.ptml
```

[Validation and checkpoint boundaries](docs/feature-first-model-exchange-validation.json)

## Process cubes

Choose **Explore process cubes** on the landing page, **Process cubes** in a
classic or OCEL workspace's top bar, or **Process cubes** under **Transform**.
Upload a JSON table with `case_ids` and `columns`; optional
`categorical_columns` contains additional string columns. Each column has one
finite numeric or string value per input row:

```json
{"case_ids":["case-1","case-2"],"columns":{"cost":[10,20],"duration":[2,4],"region":["North","South"]}}
```

Select numeric or categorical axes, a positive one-hot prefix group, or a
composite JSON list of exact dimension names. All eight native aggregations
are available: mean, sum, min, max, count, median, first and last. Division
limits and explicit numeric bin edges use the native controls. The native
engine normalizes edge order; rows outside explicit edges do not enter cells.
First and last follow input row order.

The heatmap uses X columns and Y rows, with ten displayed bins per axis.
Selecting a cell shows its native case-ID set. Repeated case IDs can contribute
multiple input rows but appear once in each cell's membership. Empty numeric
results remain `null`, distinct from zero. Complete cell/membership/bin/edge
and source-row CSV exports retain all data beyond the display pages. Original
uploads, raw native JSON and reports retain source hashes and numeric tokens.

After extracting **One row per case** features in **Transform**, choose
**Explore case features as a process cube**. This transposes the same native
matrix, preserving case order and numeric tokens. The optional case identifier
column remains metadata. Empty/duplicate feature names and non-numeric values
produce an explicit diagnostic instead of dropping or overwriting dimensions.
Generated source metadata records the extraction revision; independent table
membership is not an active-log filtering operation.

`POST /api/process-cube` and `minera_process_cube` share the native engine:

```text
minera_process_cube table.json --x-col cost --y-col region --agg-col duration --aggregation median --max-divisions-x 4
```

The checkpoint verifies 852 frontend tests, one assertion-enabled native suite,
26 controlled HTTP cases, 12 CLI runs, 56 browser checks, 20 complete CSVs and
16 cube reports. The browser handles Unicode and escaped composite names.
Windows narrow command-line argument encoding still mangles Unicode names;
native numeric/default/bin/category/order/performance and complete reference
parity remain separate gates.
[Validation and checkpoint boundaries](docs/feature-first-process-cube-validation.json)

## Persistent streaming sessions

In **Replay event streams**, choose **Persistent server session**. Configure a
DFG, footprint, temporal or token-replay monitor, then create its server session.
Queue a CSV/XES batch while inactive, choose **Start stream**, and append more
batches to the same native monitor. Gzip XES and all six native XES parser
selectors are available. CSV delimiter, whitespace, case-ID aliases and literal
column selectors are retained for every batch. CSV currently requires a header.

**Drain queued events** waits for both queued and executing callbacks without
finishing the stream. **Finish stream** drains and closes arrival; footprint and
token-replay cases can still be finalized afterward. Reset individual cases or
the complete monitor, or detach/reattach its observer. Detached intervals are
consumed by the stream without updating that monitor. The application uses one
ordered worker so case progression follows the selected per-batch order and
the order in which batches were appended.

The session persists while this server process runs. Navigating away leaves it
running; use the saved ID, **Connect or refresh session**, or **List server stream
sessions** to reconnect. A server restart removes sessions. Mutation requests
carry the current revision and unique batch receipt; stale or repeated requests
are rejected. After an interrupted request, refresh to inspect whether it
committed before appending again. **Release server session** returns final data
and frees its native state; download original sources before releasing it.

Inspect native state, queue/observer delivery counts, per-event accepted/skipped
outcomes, case prefixes, temporal deviations and final conformance diagnostics.
Complete CSV/JSON/reports retain every row and column beyond display paging.
Original CSV/XES/gzip/PNML bytes remain downloadable across reconnection. Upload
SHA-256 values are computed by the client, retained as `provided_sha256`, and
checked against actual downloaded bytes; they are not server-computed hashes.

The HTTP routes are `POST /api/live-stream/create`, `POST /api/live-stream/append`,
`POST /api/live-stream/action`, `POST /api/live-stream/release`, and
`GET /api/live-stream/snapshot`, `/api/live-stream/list`, `/api/live-stream/source`.
Creation takes `session_id`, `method` and the existing method-specific streaming
parameters. Token replay also requires multipart `model`. Appends take
`session_id`, `expected_revision`, `batch_id`, `format=csv|xes` and original
multipart `log` or raw bytes. Actions take `action=start|drain|finish|register|
deregister|reset_all|reset_case|terminate_case|terminate_all`, plus `case_id` where
needed. Source download takes `session_id` and `source_id`.

The checkpoint also fixes CRLF record terminators leaking into untrimmed CSV
headers/fields, while preserving carriage returns inside quoted values. It
verifies 869 frontend tests, seven assertion-enabled native suites, 126 HTTP
cases, both token-replay variants, six gzip XES readers, a competing append race,
33 browser checks, 38 full CSVs, six reports and six standalone CLI runs.
Durability across restarts, unbounded/lazy acquisition, trace-session workflows,
general concurrency/performance and complete reference behavior remain open.
[Validation and checkpoint boundaries](docs/feature-first-live-stream-validation.json)
record all four formats, normal export paths, both net constructions, extended
arcs, stochastic data, 501-digit integers, actual conformance handoff and complete
exports. Full format/dialect/schema/diagram preservation, solver/search,
concurrency/cancellation and performance exactness remain active parity work.

### Declare prefix monitoring

Event streams now includes **Check Declare prefixes** for both fresh replay and
persistent server sessions. Upload named-rule JSON or a Minera export containing
`activities` and `declare`, author rules in the 18-template editor, or choose
**Use displayed Declare model** after discovery. CSV, XES and gzip XES events
retain selected input/imported order. Inspect canonical rule identities,
per-case automaton states, per-event violated constraints and the native arrival
chart. Repeated timestamps remain separate observations. Full JSON/report/CSV,
canonical model and original server-held source downloads preserve these data.

Declare checks prefixes: finishing a file or stream leaves pending obligations
open. This monitor has no end-of-case finalization. Duplicate rules retain
separate native indices; unknown template names remain inert native rules.
Exact activity strings and optional signed int32 support/confidence survive JSON
uploads and canonical model roundtrips. Escaped control labels can be entered in
the JSON textarea. The displayed-model bridge resolves native activity IDs.

The standalone entry point is `minera_streaming_declare MODEL.json EVENTS.csv`
(also `.xes`/`.xes.gz`), with `--format csv|xes`, `--order input|imported` and
`--case-id-column`, `--activity-column`, `--timestamp-column` selectors.
`POST /api/streaming-declare` accepts multipart `model` and `log`, `format`,
`streaming_event_order`, `streaming_include_diagnostics`, and
`streaming_declare_case_id_column`, `streaming_declare_activity_column`,
`streaming_declare_timestamp_column`. Live creation uses `method=declare` and
multipart `model` with the existing session lifecycle and batch routes.

This checkpoint passes five assertion-enabled native suites, 880 frontend
tests and the production build, 40 HTTP cases, 25 standalone native replays,
21 browser checks and 81 independent export checks over 31 complete CSVs,
seven reports and three canonical model downloads. The displayed 218-rule
model, 872 live states, 800-state/120-event large replay, Unicode/control labels,
empty models and malformed inputs are covered. Sessions remain process-local;
complete reference behavior and performance remain open.
[Validation and checkpoint boundaries](docs/feature-first-declare-stream-validation.json)
retain source bindings and the corrected validation attempts.

### Native event and trace readers

In **Event streams**, choose **Native event and trace readers** to open the
CSV event, XES event or XES whole-trace reader. Upload the original source,
read a bounded number of items, then start the native dispatcher. Queue,
drain, finish, observer detach/reattach, cursor rewind and session reconnect
retain complete read and delivery histories. Whole traces preserve repeated
case IDs and empty occurrences. All six native XES parsers and gzip are usable;
CSV dialect/column and XES attribute/sort/trace-limit settings are exposed.
Native activity-prefix transforms run before exact case/activity acceptance;
whole-trace filters retain the entire accepted trace.

Download complete JSON/report/CSV, original bytes or selected native log/XES
exports. **Open selected reader log for analysis** retains case, activity,
timestamp, resource and lifecycle selectors through reopening and re-mining.
Native event grouping copies original events, preserving plain timestamp strings
instead of inferring date metadata. Row/column paging leaves exports complete.
Current readers import the source in memory; bounded reads advance a cursor.
Sessions last for the current server process. Lazy/unbounded/live acquisition,
restart durability and complete reference behavior/performance remain open.

`minera_reader_stream INPUT --kind csv_events|xes_events|xes_traces` prints
complete native item and selected-log data. It accepts `--batch-size`,
`--activity-prefix`, `--accept-case`, `--accept-activity`, trace event bounds,
CSV case/activity/timestamp/delimiter selectors, XES parser/case/activity/time
selectors and `--output-xes`. The HTTP family is `/api/stream-reader/`:
POST `create`, `read`, `action`, `release`; GET `snapshot`, `list`, `source`,
`export`. Reads/actions use `expected_revision`; reads also use `batch_id`
and `maximum_items` from 1 through 10000. Export takes `scope=read|delivered`
and `format=json|xes`, with XES `variant`, `gzip` and null/metadata flags.

Six native suites, 893 frontend tests/production build, 145 HTTP cases,
18 standalone native comparisons, 29 browser checks and 103 independent artifact
checks cover 29 complete CSVs, 17 reports, seven native logs, original downloads,
six XES reimports and final served assets. XES reimport normalizes recognized
timestamp spelling and assigns document order; analysis downloads add the
visible dataset name. Exact raw native values remain in JSON and CSV.
[Validation and checkpoint boundaries](docs/feature-first-reader-stream-validation.json)
retain the source bindings and corrected attempts.

### Source acquisition

Open **Acquire source data** from the landing page, classic or object-centric
navigation, or the transformation hub. All nine existing native connectors are
available: Chrome/Firefox history databases, GitHub issues/timelines, Windows
event XML, workflow activity CSV, SAP order-to-cash/accounting CSV, and Outlook
mailbox/calendar CSV. Upload copied source files or specify a server file path;
browser profile directories are discovered only at an explicit path. GitHub
reads the selected API using native HTTPS on Windows. Its optional bearer token
is sent as a header, cleared after success and excluded from saved reports.

Inspect complete native connector and rich event-log results, page through all
cases/events/attributes/metadata, and download full JSON/report/CSV, original
file bytes, native logs or either XES writer with gzip. **Open acquired log for
analysis** uses the native exported log. Reconnect/list/release operate on
immutable snapshots in the current server process. GitHub API and profile
directory acquisitions have no single original file body. Workflow/SAP/Outlook
CSV and Windows XML import exported records. Native Windows channel/EVTX modes
are described below; live Outlook and broader database/driver fidelity, restart
durability and complete reference/performance parity remain open.

`minera_acquire_source CONNECTOR [PATH] --param key=value` prints the complete
native connector and rich log. Optional `--token-env NAME` reads a repository
credential without printing it; `--output-xes PATH` exports the native log.
HTTP endpoints are POST `/api/source-acquisition/acquire` and `release`, plus
GET `snapshot`, `list`, `original`, and `export`. Acquisition takes `source_id`,
`connector`, `mode=upload|path|repository_api`, native family parameters and
explicit `source_path` for path mode; uploads use multipart field `source`.
Export takes `format=json|xes`, XES `variant=line_by_line|etree` and `gzip`.

Eight native suites, 916 frontend tests/production build, 117 HTTP cases,
11 standalone CLI comparisons, 28 browser checks and 69 artifact checks pass.
Browser evidence includes all nine connectors, complete eight-table CSVs, ten
reports, original downloads, both XES writers/gzip, three native reimports and
the final served production assets. Live public GitHub HTTPS also completed.
[Validation and checkpoint boundaries](docs/feature-first-acquisition-validation.json)
retain source bindings and failed verification attempts. No parity unit is promoted.

### Native Windows event acquisition

In **Acquire source data**, choose **Windows event logs**. Read an explicit
**Local Windows event channel**, upload an archived **EVTX binary log**, or
provide its server file path. Exported XML import remains available. Native
channel/EVTX reads use the Windows event APIs with the current account access.
Set a query filter, newest-first selection, event bound or per-read timeout.
The default bound is 1000; a visible flag reports when more records were
available. The native importer orders selected events by timestamp within cases.

Download the exact original EVTX and the complete captured event XML separately,
inspect the capture descriptor alongside all native log fields, or export full
JSON/report/nine-table CSV and XES/gzip. Reconnect returns the retained snapshot;
release removes it from the current server process. **Open acquired log for
analysis** retains the complete selected log. Live channel reads have no original
file body; their event XML capture remains available.

CLI examples:

```text
minera_acquire_source windows_events --param windows_events_source=service --param windows_events_channel=Application --param windows_events_max_events=100 --output-capture events.xml
minera_acquire_source windows_events archive.evtx --param windows_events_source=evtx --param windows_events_reverse=true --output-xes events.xes
```

Additional parameters are `windows_events_query` and
`windows_events_timeout_ms` (1 through 30000). HTTP acquisition uses
`mode=windows_service` for channels, or `mode=upload|path` with
`windows_events_source=evtx` for archived files. GET
`/api/source-acquisition/capture?source_id=...` returns retained UTF-8 event XML.
Native capture requires Windows; continuous subscriptions/bookmarks, remote
hosts, restart durability and full reference/performance fidelity remain open.

Nine native suites, 926 frontend tests/production build, 44 HTTP cases,
three native CLI and three Windows-reader comparisons, 16 browser checks and
44 artifact checks pass. Forward/reverse/query EVTX captures match Windows'
own reader; nine complete CSVs, four reports, original/capture downloads,
three XES reimports and final production assets are independently verified.
[Validation and checkpoint boundaries](docs/feature-first-windows-source-validation.json)
retain public dataset provenance, source bindings and the corrected test attempt.

### Native workflow and SAP database history

In **Acquire source data**, select workflow activity, SAP accounting or SAP
order-to-cash history. **Database history source** selects exported CSV,
an uploaded/explicit SQLite history database, or an ODBC connection.
The native driver reads the actual history schemas: Camunda process/activity
joins, SAP document creation/customer/vendor clearing, or sales/delivery/change/
billing records. SAP schema prefixes, row bounds and database timeouts are exposed.

The default ODBC dialect retains the caller-observed reference queries. Portable
SQLite syntax and Microsoft Access Camunda syntax are explicit options. ODBC
requires Windows and a configured compatible installed driver. Its connection
string is sent as a request header, cleared after success and excluded from
reports; the CLI reads it from a named environment variable. Native connections
have no interactive prompts. Unsupported ODBC query timeout is reported clearly;
the row bound remains active. SQLite opens read-only; ODBC requests the driver's
read-only hint and issues only the fixed history SELECT queries.

Inspect/download complete original query tables, formatted UTC timestamps,
NULL resources, ordering/indexes and rich native logs. Full JSON/report/CSV,
original SQLite bytes and both XES/gzip writers remain usable beyond table paging.
Reconnect uses the immutable captured result. Release includes the full tables
so local inspection and CSV/report exports remain available. **Open acquired log
for analysis** uses the canonical XES trace case key. Choosing the same database
again after changing connectors resets the file input correctly.

```text
minera_acquire_source sap_accounting history.sqlite --param database_driver=sqlite --param database_prefix=main. --output-table query.json --output-formatted formatted.json
minera_acquire_source camunda_workflow --param database_driver=odbc --database-connection-env MINERA_DB_CONNECTION --output-xes history.xes
```

Additional parameters are `database_dialect`, `database_max_rows` (default 1000)
and `database_timeout_seconds` (default 30). HTTP `mode=database` uses ODBC;
SQLite uses upload/path mode with `database_driver=sqlite`. Credentials use
`X-Minera-Database-Connection`. GET `/api/source-acquisition/table` takes
`source_id` and optional `formatted=true`. Query values are text/NULL with
reported SQL types; binary/general dtype behavior, live vendor deployments,
whole-database WAL/backup consistency and full reference/performance fidelity
remain gates. Original explicit-file bytes are the main file, not an online backup.

Ten native suites, 941 frontend tests/production build, 53 HTTP cases, four
native CLI comparisons, three controlled reference formatting comparisons,
17 browser checks and 65 artifact checks pass. All three SQLite schemas and
a real installed ODBC Camunda driver are exercised. Twelve complete CSVs,
six reports, original/query/formatted downloads, three XES reimports and final
production assets are verified. [Reference query/result evidence](docs/database-driver-reference-behavior.json)
and [validation boundaries](docs/feature-first-database-source-validation.json)
retain the public-interface pins and corrected attempts. No parity unit is promoted.

### Sample dataset

The bundled sample (`data/sample_event_log.csv`) is produced by the
`minera_generate_sample` tool; its mined export lives at
`src/frontend/public/sample-data.json`:

```sh
./build/minera_generate_sample data/sample_event_log.csv
./build/minera_export data/sample_event_log.csv -o src/frontend/public/sample-data.json
```


### Native Outlook acquisition on Windows

The native source API and `minera_acquire_source` can read the current Windows
account's `Outlook.Application` COM profile. Mail extraction reads Sent Mail and
Inbox; calendar extraction supports the default calendar or an explicitly
resolved shared account and folder ID. Live item acquisition defaults to 1000
examined items and records total/examined/captured/skipped counts plus an explicit
limit flag. COM property failures and unsupported mail classes appear in the
retained item table. These counts describe a bounded acquisition rather than a
transactional Outlook snapshot.

```powershell
minera_acquire_source outlook_mail_extractor --param outlook_source=com --param communication_max_items=1000 --output-capture mailbox-items.json --output-formatted mailbox-events.json --output-xes mailbox.xes
minera_acquire_source outlook_calendar --param outlook_source=com --param outlook_email_user=owner@example.invalid --param outlook_calendar_id=9 --param communication_include_reminders=false --output-table calendar-items.json --output-formatted calendar-events.json
```

HTTP acquisition uses `POST /api/source-acquisition/acquire` with
`connector=outlook_calendar` or `outlook_mail_extractor`, `mode=outlook_com` and a
new `source_id`. It accepts no upload body or server file path. Use the existing
communication item/event/reminder/current-time parameters and optional
`outlook_email_user`/`outlook_calendar_id` for shared calendars. Source responses
retain `outlook_tables.items` and `outlook_tables.formatted` alongside the full
native log. The `/capture` and `/table?formatted=true` routes download those
tables, and `/export` produces native log JSON or XES/gzip. Reconnection and
release retain the acquired result without rerunning COM extraction.

The Acquire source data workbench supports both Outlook CSV records and the
Outlook desktop profile. Choose mailbox history or calendar history, then the
desktop profile input. Calendar controls select the current/shared account and
folder, reminders and a fixed clock; native limits bound items and events. The
inspector retains all item/formatted/skipped rows, native JSON and full reports,
complete table CSV, and both XES/gzip writers. Reconnect restores the saved native
snapshot, release retains both tables for local inspection, and acquired XES logs
open for analysis with their complete case/event data.

Controlled COM graphs validate the same native extraction and production HTTP
handlers through the application, alongside four public reference outputs.
Real user profiles were not read during validation. Outlook COM activation/profile
behavior, hard call deadlines, recurrence expansion, full date/type/default/error/
performance parity and arbitrary-character XES fidelity remain open. See
`docs/feature-first-outlook-source-validation.json` and
`docs/feature-first-outlook-workspace-validation.json`.


### Decision tables and guarded workflow models

After opening a raw event log, use Conformance → Decision tables and guarded
models. Select a cached discovered/imported workflow and either encoded tables
or a guarded data Petri net. Context controls select the activity field,
attributes, case scope and bounded lookback. Guarded output also exposes native
tree depth/split/leaf/candidate/gain controls and optional diagnostics. Inductive
models can use the standard or transition-bordered process-tree conversion.

The inspector retains every encoded row/target/class and all diagnostics,
guard transitions, variables and recursive decision rules. JSON/report and
complete-table CSV downloads preserve the full native result beyond visible
paging. The guarded model is rendered with native expressions and variables.
The same-run underlying PNML retains workflow structure/markings and can reopen
for structural conformance; data guards remain in the complete decision JSON
and are not enforced by structural replay. Native encoded tables embedded in
a loaded analysis export remain available for offline inspection and downloads.

`POST /api/decision-tables` and `/api/data-petri-net` keep their original native
response shapes. Request `include_workspace=true` with the current query-schema
`source_id` to obtain the result, source/model identity and same-run underlying
PNML in a `DECISION_WORKSPACE` envelope. Guarded requests additionally accept
`include_decision_diagnostics`; mismatched source generations are rejected.
This feature checkpoint does not certify full reference classifier/encoding/
guard behavior. See `docs/feature-first-decision-workspace-validation.json`.


### Complete alignment result inspection

In Conformance → Other alignments, run a DFG, edit-distance, process-tree,
decomposed, Petri anti/multi-alignment, process-tree reduction or stochastic
language comparison. The Complete alignment results inspector below the controls
retains every returned entry, move, native count, diagnostic and projected
DFG/log record. Select a result and table, page through every row and column, or
inspect one entry directly by its zero-based index.

Download complete native JSON to retain the original HTTP response, an alignment
report to retain the submitted settings/reference file metadata, or complete
table CSV for every row and column beyond the visible page. Nested event
attributes remain complete JSON cells. Native errors keep the last successful
result; opening another log clears prior inspections. Earlier summary previews
remain available above the complete inspector. This feature checkpoint does not
promote native algorithm or reference parity status.


### Single-run Monte Carlo simulation

Open a raw CSV/XES log and use Conformance → Monte Carlo Petri-net simulation.
Select a discovered/imported workflow, configure arrivals, markings, replay,
duration fitting and stopping, or expand the indexed overrides for transition
weights, duration-family records, per-place resources and sparse stochastic maps.
Native compatibility controls state their current virtual-time limitations.

Run native simulation once, then inspect every generated case/event, visible and
scheduler duration, diagnostic, model index and resource interval. Row/column
paging and the interval preview retain complete JSON/report/CSV downloads.
Matching simulation XES comes from that same result and can reopen for mining;
the workflow PNML retains structure and run markings and opens for conformance.

`POST /api/monte-carlo` keeps its original JSON shape. Add
`include_workspace=true` and the current query-schema `source_id` to receive a
`SIMULATION_WORKSPACE` with the complete native result/log, matching XES and
model/PNML from one run. The existing playout XES endpoint remains available.
This feature checkpoint does not promote native/reference behavioral parity.


### Variant-specific Alpha and Heuristic discovery

In Mine a Process Model, select Alpha → Classic/Alpha+ or Heuristic → Classic/
Heuristics++. Configure the native grouped-log controls and run discovery.
The rendered models, complete marking/statistic/matrix/bundle tables, JSON/
analysis/request reports and every-table CSV retain that actual selected result.
Selecting a wizard label no longer presents the cached Classic model as Alpha+.

PNML/BPMN and representable PTML export the newly discovered model. Native
conversion errors remain visible. Optional bounded seeded playout produces XES
from that model and reopens for mining. Check this discovered workflow imports
its PNML for conformance with the source ID and selected activity field. Loaded
analysis JSON retains saved models with an explicit unrecorded Alpha variant.

`POST /api/discover-workflow` accepts `family=alpha|heuristic`, the current
query-schema `source_id` and the existing family-specific mining parameters.
It returns `WORKFLOW_DISCOVERY` with the actual variant, complete native
analysis/Alpha+ result and same-model formats. Optional `discovery_playout_*`
controls select trace count, firing cap and seed. Discovery leaves the active
cached model unchanged. Full reference behavioral parity remains open.


### Complete quality and Petri diagnostics

In Conformance, run Model quality evaluation or Petri analysis, then choose
Inspect and save. Complete model diagnostics keeps saved results for the current
workspace and offers every native table, full selected-row JSON and ordered
typed public values where the native result supplies them. Original native
response JSON, request/report JSON and complete CSV retain every returned field
and numeric token. Saved comparisons expose all actual scalar fields across
runs; comparison JSON also retains each complete result and submitted request.

Both workbenches expose Standard/Transition bordered Inductive conversion.
Composite weights accept signed numbers, NaN, inf and -inf; zero-total weights
produce the native uncomputed outcome. Errors, exhausted searches, null values,
empty tables and absent typed public values remain visible. Changing controls
hides the previous current result; saved entries retain their original request.
Opening a new log clears the comparison history. Export comparisons before
navigating away or reloading. Full reference behavioral parity remains open.


### Generate process trees and logs

Choose Generate process trees and logs on the start page, or Generate trees in
an event-log/OCEL toolbar. BASIC offers recursive depth, leaf probability and
custom activity labels. PT-and-log offers model population and triangular
activity counts, plus sequence/choice/parallel/loop/inclusive/silent/duplicate
probabilities. Blank values use native defaults and full 64-bit seeds remain
exact text. An imported source is optional.

Select any generated model to inspect its original tree, native net/markings,
complete hierarchy and same-model PTML/PNML/BPMN. Native conversion errors stay
visible; the original tree remains available. Optional basic/top-bottom sampling
or extensive enumeration generates a log from every actual model, with timing,
seed, case/step/length/loop controls and enumerated words. Original JSON,
request reports, selected-model JSON and every-table CSV preserve all data.
Generated XES opens for ordinary mining. Generated PNML can check an active log
using explicit source/marking semantics. Generation leaves cached models intact.

`POST /api/generate-trees` accepts existing `random_tree_*` fields,
`tree_variant`, `generate_log` and native `playout_*` controls, including start,
case/event intervals. It returns TREE_GENERATION with the original RANDOM_TREE
result and each model's original tree, formats/errors and optional native log.
Population construction retains native bounds; optional logs are bounded to
one million events across models. Reference algorithm/RNG/default/replay/
conversion/numeric/performance parity remains open.


### Discover individual Inductive and POWL models

Choose Mine a Process Model, then an Inductive or POWL variant. Inductive offers
IM/log, IMf and IMd/DFG with native noise, cut/fallthrough, parallel evaluation,
timestamp sorting and Standard/Transition bordered conversion. POWL offers Tree,
Brute force, Maximal and both independent dynamic-clustering variants, with
filtering, order-frequency and search controls. Blank fields use native defaults.

Inspect each run's original process tree/POWL, exact net and complete node,
transition, activity, configuration and source tables. Download original JSON,
the request report, every-table CSV and actual PNML/BPMN/PTML. Unsupported strict
POWL-to-tree conversion retains its native error and original model. Saved
models explicitly show when their discovery variant was not recorded.

Custom activity/case/timestamp fields can regroup and reorder the discovery log.
The original/adapted counts stay visible, and Open adapted discovery source uses
the exported field mapping to preserve labels, case IDs and order. Optional
seeded playout uses the discovered net. Generated XES opens for mining; Check
this discovered workflow imports its PNML for conformance on the current source.
Native diagnostic values and errors remain visible; a handoff does not establish
fitness or full parity.

`POST /api/discover-workflow` requires the current `source_id` and accepts
`family=inductive` with existing `inductive_*`, `noise_threshold` and
`discovery_tree_variant` controls, or `family=powl` with existing `powl_*` fields.
Optional `discovery_playout_*` controls retain bounded native playout. The
WORKFLOW_DISCOVERY response preserves original/adapted source counts, complete
analysis, formats/errors and `source_xes_options` for the actual exported log.


### Discover and check Declare and Log Skeleton constraints

Choose Mine a Process Model, then Declare or Log Skeleton and Classic. Discover
constraints from the active grouped source with native thresholds and activity
selectors. Declare offers all eighteen templates, explicit activity filtering
and support/confidence/automatic ratios. Log Skeleton offers noise and all six
checking families. Selected families and activities can be explicitly empty;
omitted fields retain native defaults. Conformance can use a separate activity
field, and native diagnostics can select the case-ID field.

Inspect the actual discovered model, every case/deviation and typed diagnostic
cell. Download original JSON, the request report, analysis, native diagnostics
and every-table CSV. Saved models retain their identity; fresh Log Skeleton
models show their complete embedded conformance. Table paging retains all rows.
Original JSON keeps native wire order; derived numeric dictionary keys follow
the app's property enumeration.

Optional Declare playout generates XES from this exact model with case/length
controls and a full 64-bit seed. Prefix-safe sampling can leave obligations
pending; the generated analysis includes every completed conformance result.
Mine discovered Declare playout reopens the original log, retaining missing
timestamps. Optional diagnostics/playout errors keep the model available.

`POST /api/discover-constraints` requires the current `source_id` and accepts
`family=declare` with existing `declare_*` discovery fields, or
`family=log_skeleton` with existing `log_skeleton_*` fields. Independent
`conformance_activity_key` and `diagnostics_case_id_key` select checking and
diagnostic fields. `generate_log=true` uses `declare_playout_traces`,
`declare_playout_min_length`, `declare_playout_max_length` and
`declare_playout_seed`. CONSTRAINT_DISCOVERY returns the complete actual model/
conformance analysis, typed diagnostics and optional generated analysis/XES,
with explicit native errors and a one-million configured-event playout budget.


### Discover footprints and transition systems independently

Choose Mine a Process Model, then Footprints or Transition System and Configured
discovery. Footprints offers all eight native discovery adapters and all three
conformance methods. Compare the actual cached process tree, POWL, Inductive,
Alpha or Heuristic model, or the selected source DFG. Choose activity/case/time/
start/index fields, table sorting, strictness, always-executed checks and initial
marking text. The compared model footprint preserves native absent fields.
Alpha input projection and original source counts remain explicit; cached models
are retained rather than rediscovered with new defaults.

Transition systems offer forward/backward direction, sequence/set/multiset views,
the full signed 64-bit window range, activity/case selectors and optional event
traversals. Inspect native state identities, root membership, payloads, adjacency
and every returned occurrence. Cards/diagrams preview the model; complete tables,
row JSON and CSV preserve all data. Original native/request/analysis JSON and the
transition view's JSON download retain exact numeric tokens. A changed source
clears the previous discovery, and saved models identify their recorded origin.

`POST /api/discover-graph` requires `source_id` and `family=footprints` or
`family=transition_system`. Existing `footprints_*` or `transition_*` fields feed
the unchanged native algorithms. GRAPH_DISCOVERY returns original/analysis source
counts, complete native analysis and the compared footprint/model origin when
applicable. Marking/variant incompatibilities and invalid windows retain errors.
The Windows CMake server target enables MSVC's large-object format.


### Discover performance spectra and batches independently

Open a CSV or XES event source and choose Timing discovery. Select performance
spectrum or batch detection, then a native adapter and its controls. Spectrum
supports all log/dataframe/lazyframe connected/disconnected variants and the two
compatibility variants, comma-separated activity patterns, full unsigned 64-bit
sample text, native event sorting and custom activity/timestamp/case fields.
The completed diagram uses native observations; spans retain adapter timestamp
units. A zero sample size preserves the log adapter's endpoint observations.

Batch detection supports log, pandas and polars, merge distance, minimum batch
size, activity/resource/completion/case/start fields and table event identities.
Enable the start-field override with a blank value to use completion times.
Inspect every observation, offset, batch interval/category and event reference
through complete tables and selected-row JSON. Download the original response,
exact request/report, complete analysis and every table as CSV. Source changes
clear previous results and errors; editing controls retains the completed run.
Sources without timestamps keep this workflow available and show native errors.

`POST /api/discover-timing` requires `source_id` and `family=spectrum` or
`family=batches`. Existing `spectrum_*` or `batch_*` parameters feed the native
adapters. TIMING_DISCOVERY returns the completed method/variant/source and full
analysis. Spectrum and batch JSON preserve fractional epoch timestamps with
round-trip double precision. The feature catalogue retains all open parity gates.


### Analyze temporal, repetition and correlation relationships independently

Open a CSV or XES source and choose Behavior analysis. Temporal profile discovery
and conformance support log, stream, dataframe and lazyframe inputs constructed
from the cached log. Select activity/case/completion/start fields, strictness,
business hours and fractional weekly slots. An enabled blank slot override means
no active time. Conformance retains native zeta values, including inf and -inf.

Download native profile JSON or choose Use this profile for temporal conformance.
Profile JSON contains [source, target, mean_seconds, standard_deviation_seconds]
tuples; strings can contain commas, colons, arrows, quotes and Unicode. An empty
array checks an empty profile. Discovery counts remain in complete analysis/CSV.
Every native case bucket and deviation is preserved, including empty buckets;
flat case regrouping can differ from the original source case count.

Minimum self-distance supports log/pandas/polars, activity/case selectors and all
native witnesses. Correlation supports classic/classic_split/trace_based across
all four inputs, activity/case/completion/start fields, exact time matching and
full uint64 sample text. A blank enabled start override uses completion times.
Inspect complete moments, witnesses and matrices; download original native JSON,
request/report, analysis, profile/conformance files and every table as full CSV.
Changed sources clear old output and errors. Untimed sources keep this workbench
available, expose strict temporal errors and permit self-distance analysis.

`POST /api/analyze-behavior` requires `source_id` and `family=temporal_profile`,
`temporal_conformance`, `minimum_self_distance` or `correlation`. `variant` selects
the native algorithm/input; correlation also uses `adapter`. Existing temporal,
minimum-self-distance and correlation parameter names feed unchanged native
algorithms. Conformance uses `temporal_profile_json`. BEHAVIOR_ANALYSIS returns
method/variant/adapter/source, full analysis and native profile/conformance data.
Temporal/correlation JSON preserves round-trip double precision. Original table
metadata/order, every remaining feature and strict reference parity stay in scope.


### Configure statistical and causal DFG discovery independently

Open a CSV or XES log and choose Directly-Follows Graph. Configured DFG discovery
offers ten statistical catalogue selections and alpha/heuristic causal discovery.
Select full unsigned window text (zero includes self-pairs), per-case retention,
activity/completion/start fields, performance aggregation, business hours, weekly
slots, case attributes and node attributes. Blank fields and native switches keep
native defaults. Enabled blank overrides mean completion-time fallback, no active
business slots or no case attributes. Fractional slots use start:end weekly seconds.

Completed results retain their settings while controls are edited. The source map
continues to show the cached analysis; configured statistical and causal results
have their own full native output. Inspect every edge, triple, boundary, time,
performance summary/sample/value, case/node attribute and signed causal strength.
Download native/report/analysis/variants JSON or any full table CSV. Row and column
paging never limits exports, and complete selected-row JSON includes long arrays
and every derived row. Sample indexes locate positions in native sample vectors.
Changed sources clear old results and errors. Untimed sources support frequency
discovery. The separate dataframe DFG workspace retains its own input contract.

POST /api/discover-dfg requires the current source_id and at least one of
dfg_statistical_variants or dfg_causal_variants. Existing dfg_* window, retention,
field, aggregation, business-hour, slot, case-attribute and node-attribute parameters
feed the unchanged native EventLog algorithms. DFG_DISCOVERY returns source/counts,
canonical selections and full analysis containing the configured dfg_variants.
Variant JSON preserves round-trip double precision. Full reference parity and all
remaining catalogue features and strict compatibility gates remain in scope.


### Evaluate already-computed replay results

Choose Conformance > Replay-result fitness. Paste or open JSON token or alignment
replay records, choose the native fitness variant and evaluate. Token input also
accepts traces/trace_diagnostics containers and saved token_replay/replay_fitness
envelopes. Each record represents one native contribution; grouped alignment
variant counts are not expanded. Additional metadata remains in original input.

Token counts accept signed decimal JSON integers or decimal strings. Explicit
*_tokens values, including zero, take precedence over short count aliases; null
uses the alias. Every token record contributes, independently of computed/error
metadata. Native nonpositive denominator rules and trace_is_fit classification
remain intact. trace_fitness accepts "nan", "inf" and "-inf"; typed public metrics
retain their native nonfinite conventions while ordinary summaries use null.

Alignment evaluation skips null/uncomputed records, derives fitting records from
numeric fitness and preserves best_worst_cost/bwc behavior. Incompatible explicit
variants and invalid native metrics remain inspectable uncomputed results. Download
native/report/original-input/evaluation JSON or every full CSV. Paging and long-cell
previews preserve complete selected-row JSON. Edited controls retain the completed
run; changed host logs clear old output/errors. The editor uses full panel width.

POST /api/evaluate-replay-results accepts the original JSON request body with
input_kind=token or alignment and optional fitness_variant. It requires no cached
log or model. The response contains original input, submitted row count, native
evaluation and exact token totals/activations where applicable. The native JSON
decoders are declared in minera/precomputed_replay_json.hpp. Existing log/model
evaluation stays available; full typed input, adapters and reference parity remain
in scope alongside the complete feature catalogue.


### Choose the native input shape for model quality

In Conformance > Model quality evaluation, choose log, event stream, dataframe or
lazyframe for fitness, precision, generalization or composite evaluation. Shapes
are constructed from the selected model's cached log. Activity/case selectors and
row caps feed native evaluators; flat case regrouping can change evaluated trace
counts. The result identifies source, shape and original model-input counts.

Imported PNML fitness keeps supplied exact markings and membership. Simplicity
uses only the model and hides log input controls. Inspect and save evaluation to
download every native field, input context, request, complete CSV and comparison.
Source changes reset stale output and controls. Native cap errors remain visible.

POST /api/evaluate accepts evaluation_input_adapter=log,stream,dataframe,lazyframe
and an optional source_id identity guard. Omission preserves the existing log
path. Additive input_context accompanies the normal single-family result. The
native EventDataView/PnmlDocument fitness overload preserves marked documents.
Original table metadata and every remaining feature/strict parity gate stay open.

### Replay original logs against independent PNML models

Open **Replay a log against a PNML model** from the landing page, or **Token replay**
from either toolbar. Upload CSV, TSV, XES or gzip XES and a PNML model. This workflow
runs independently of the cached discovered model. Import controls cover custom
columns, delimiters and all six existing XES importer selectors.

Choose grouped log, event stream, dataframe or lazyframe inputs. Flat CSV inputs
retain original parser rows, including quoted multiline values. Custom case fields
can regroup those events; already grouped logs retain their trace boundaries.
Declared empty CSV inputs retain their imported activity field and accept a zero cap.

Forward replay exposes the native complete public result, place/transition fitness
maps, variant grouping, returned names/labels and hidden-transition path caches.
Backwards replay exposes its native trace diagnostics. PNML initial/final counts
and explicit zero-key membership remain intact.

Inspect every trace, per-event diagnostic, model ID and marking. Typed public
inspection retains ordered object keys, float hex and exact integer tokens.
Download complete native JSON, a request/input-hash report, every table as full CSV,
the public forward result and the exact original log/PNML files. Results remain
bound to the inputs and settings of their completed run.

This is a feature checkpoint; strict full-reference replay parity remains open.

### Align independent logs against PNML models

Open **Align a log against a PNML model** from landing, or **Petri alignment** from
either toolbar. Upload CSV, TSV, XES or gzip XES and the original PNML model.
Choose any of the six native alignment variants and grouped log, event stream,
dataframe or lazyframe inputs. CSV flat shapes retain parser rows; case selection
can regroup them. Grouped logs preserve their trace boundaries.

Import settings, event/state/time bounds, generator alternatives and discounted
search controls are available. Blank values use native defaults. Cost maps are JSON
objects keyed by exact activity labels; positional log arrays and indexed model/
sync arrays address event positions and PNML transition order. Explicit {} and []
retain native defaults. Marking overrides require one nonnegative native-int count
per PNML place, in the order displayed under model_context/places.

Inspect every trace group, complete move, returned alternative, projection and
marking. IDs distinguish duplicate-label transitions. Original and effective
markings remain separate. Download native JSON, the completed request/hash report,
every complete table CSV and exact original files. Paging does not limit exports.
Uncomputed searches retain diagnostics and do not display placeholder measurements.
Edited controls and failed requests retain the last completed result and its inputs.

POST /api/align-petri-workbench accepts multipart log/model fields and the displayed
alignment controls. It requires no cached source and never changes the host mining
session. Independent numeric output retains native double precision; the existing
/api/align serializer is unchanged. Full reference parity remains open.

### Organizational analysis workbench

Open **Resources** after mining a CSV/XES log. **Role discovery**, **Resource
profiles**, **Local diagnostics** and **Attribute network** keep their forms and
completed results independently. Blank controls use native defaults. Custom fields
work even when the imported log has no default resource attribute.

Roles offer LOG and PANDAS, similarity thresholds and native resource/activity
bounds. **Use completed roles as diagnostics groups** copies every resource label
into source-bound JSON groups and retains the role run's selectors. Explicit group
JSON maps group names to resource arrays and supports overlap, unseen resources
and empty memberships. Native blank-member rules are preserved.

Profiles expose all eleven metrics, selectors, time bounds, resource/activity
queries and row/resource caps. Attribute networks expose independent sorting and
performance timestamps, source/target nodes, edge labels/references, outgoing/
incoming relations, weekly business-hour slots and row/value caps.

Inspect full native results and complete tables. Download native JSON, submitted
requests, source-bound reports, individual table CSV or all tables CSV. Paging
does not trim downloads. Native capped/empty/zero-query results retain their actual
counts and diagnostics. Errors retain the completed run; source/request guards
prevent stale in-process updates. Backend restart identity and lone-surrogate
transport remain explicit compatibility boundaries.

The existing four POST endpoints accept source_id/include_source for the
workbench. Local diagnostics additionally accepts local_diagnostics_groups_json.
Legacy organizational requests retain their existing result bodies.

Native, frontend and captured API checks passed. Browser role/profile exports are
verified; complete diagnostics/network UI checks and visual inspection remain
pending because the browser tool's saved permission rule blocked the local site.
See [validation and boundaries](docs/feature-first-organizational-validation.json).

### Discovery from original workflow inputs

Use **Discover a workflow from original inputs** on the landing page or
**Workflow inputs** in either toolbar. Select Alpha Classic/Alpha+ or Heuristic
Classic/Heuristics++, then an original CSV/TSV/XES/gzip-XES, a typed EventTable
JSON file, or a structured direct graph. Log, stream, dataframe and lazyframe
shapes retain their native behavior. Direct graphs use Classic discovery.

Typed tables retain cells, dtypes, indices, categories and metadata. The separate
mutable Heuristics++ pandas path shows the complete table before and after a run,
including changes made before a native error. Literal selector controls support
empty column labels. Graph counts retain exact uint64 integers and boundary
presence; the complete Heuristic graph carrier also accepts performance values.

Inspect complete native Petri/Heuristics snapshots and Alpha+ live/detached
markings. Download native JSON, original inputs, submitted requests, reports,
every inspection table and supported same-run model formats. A failed diagram
or model conversion retains the complete native result and its export error.
These independent requests preserve the current cached mining session.

The additive POST /api/discover-workflow-input endpoint provides the workbench.
Native, frontend, API and export checks passed. Complete browser/visual checks
and strict reference parity remain pending; see
[validation and boundaries](docs/feature-first-workflow-input-validation.json).

### Align original inputs against independent graph models

Use **Align original traces against a DFG or PNML model** on the landing page or
**Graph alignment** in either toolbar. Choose DFG Classic with DFG JSON/text,
or decomposed RECOMPOS_MAXIMAL with a PNML model. Check an original CSV/TSV/XES/
gzip-XES log, a typed EventTable JSON file, one original trace, or literal trace
words. These requests preserve the current cached mining session.

Configure exact-label cost maps, literal activity/case/timestamp selectors,
native bounds, cache controls and PNML import/marking options. Inspect the full
actual invoked options, original model and effective markings. Explicit empty
DFG cost functions retain their supplied flags. Empty decomposed marking arrays
use native source/sink defaults; dense zero vectors remain explicit.

Inspect every native alignment field, move and diagnostic, including undefined
raw-trace scores, uncomputed or incomplete results and exact transition IDs.
Grouped DFG runs can produce both metric and log projections from the same
alignment. Download complete native JSON, reports, all inspection CSV tables,
original files and supported same-run DFG/PNML/projected-XES artifacts. Native
errors and conversion failures retain their available snapshots separately.

The additive POST /api/align-graph-workbench endpoint provides this workbench.
Native, frontend, API, fixture publication and export checks passed. Complete
browser/visual checks and strict reference parity remain pending; no new browser
attempt is claimed. See [validation and boundaries](docs/feature-first-graph-alignment-validation.json).

### Discover Inductive trees from original inputs

Use **Discover an Inductive workflow from original inputs** on the landing page or the Inductive entry in either
toolbar. Choose IM, IMF or IMD and an original CSV/TSV/XES/gzip-XES, complete
typed EventTable JSON, compact weighted variants or a declared DFG constructor.
Log, stream, dataframe and lazyframe retain their native input behavior. These
requests preserve the current cached mining session.

Weighted records keep their order, repeated rows, empty words, zero counts and
exact uint64 frequencies without expanding cases. DFG input retains its ordered
activity registry, indexed edge vector and start/end counts; discovery always
uses the native graph-recursive IMD path. Literal selectors retain whitespace
and explicit empty values, including native validation errors. Inspect actual
options and unchanged typed tables before/after each run.

Inspect the complete native ProcessTree registry and every stored node field,
native preparation/source snapshots and real graph observations. Choose either
standard or transition_bordered Petri conversion. Download complete native JSON,
reports, all inspection CSV tables, original files and supported same-run PTML,
BPMN, PNML and prepared-source XES. Legacy analysis, preparation and conversion/
export failures retain available native results with separate explicit errors.

The additive POST /api/discover-inductive-input endpoint provides this workbench.
Native, frontend, API, fixture publication and export checks passed. Complete
browser/visual checks and strict reference parity remain pending; no new browser
attempt is claimed. See [validation and boundaries](docs/feature-first-inductive-input-validation.json).

### Align against original process-tree models

Use **Align original traces against a process-tree model** on the landing page or the Tree alignment entry in
either toolbar. Choose SEARCH_GRAPH_PT, DYNAMIC_PROGRAMMING, MILP or
APPROXIMATED_ORIGINAL, an original PTML/full ProcessTree JSON model and original
CSV/TSV/XES/gzip-XES, complete typed EventTable JSON or exact trace words.
Check the grouped log or a selected original trace; explicit words become a
native EventLog with PRESENT cells, including empty activity strings. These
requests preserve the current cached mining session.

Inspect all twelve actual native options, including four time limits in seconds
whose request controls use milliseconds, exact bounds and literal selectors.
The complete original tree retains duplicate/unused labels, latent activity
indices, every operator/raw enum and ordered children. Results preserve exact
costs, full-precision fitness, every move string and diagnostic, including
executed incomplete results. Native errors retain model/input/options context.

Model exports run independently from the same original tree. Choose standard
or transition_bordered Petri conversion and inspect its complete native net.
Download complete JSON, reports, all inspection CSV tables, original files and
available same-run PTML, BPMN, PNML and checked-source XES; conversion/export
errors remain separate. The native MILP variant uses its dependency-free A*
path, with zero external LP solves; strict solver parity remains pending.

The additive POST /api/align-process-tree-workbench endpoint provides this workbench.
Native, frontend, API, fixture publication and export checks passed. Complete
browser/visual checks and strict reference parity remain pending. The current
exact-origin browser retry was rejected by the saved permission/policy gate
while the previous Feature53 deployment was live; no Feature54 UI check completed.
See [validation and boundaries](docs/feature-first-process-tree-alignment-validation.json).
