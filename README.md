# Minera

Minera is an early C++ process mining core built to explore PM4Py-style
workflows with native performance.

## Current Core

- CSV event-log import with configurable case, activity, and timestamp columns.
- XES event-log import **and export** (IEEE 1849-2016), the standard
  interchange format used by public datasets (e.g. the BPI Challenge logs),
  reading and writing the common `concept:name` / `time:timestamp` /
  `org:resource` attribute subset — so a log can be cleaned and filtered in
  Minera, then handed to any other XES-speaking tool (CLI `--xes`, or the
  frontend's **Download log as XES** button).
- In-memory event log model grouped into traces.
- Directly-follows graph discovery with activity, start/end, edge, and variant
  counts.
- Eventually-follows graph discovery: for every activity pair, how often one
  is followed by the other — directly or transitively — later in the same
  case, surfacing long-range dependencies the directly-follows edges miss.
- Rework statistics: per activity, the number of cases where it executes more
  than once — repeated work that a frequency count alone hides.
- Case arrival rate: summary statistics over the gaps between consecutive
  case starts, alongside the existing case-throughput and edge waiting times.
- Process-model discovery with the **Alpha Miner**, running the **Alpha+
  variant** by default: the log's causality, parallelism, and choice relations
  are assembled into a Petri net, and the classic algorithm's two short-loop
  blind spots are repaired — an activity that directly repeats is mined out and
  re-attached as a self loop on the place between its neighbours, and two
  activities that alternate (`a b a b …`) are recognised as a loop instead of
  being mistaken for concurrency (which would leave both unconnected). On a log
  without short loops the result is identical to the textbook algorithm, which
  remains available via `--alpha-variant classic` (CLI) or the `alpha_variant`
  form field (server).
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
  (logs without a cut fall through to a flower model). A **noise threshold**
  switches on the **infrequent variant (IMf)**: cuts are sought on the
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
  with `--inductive-variant dfg` (CLI) or the `inductive_variant` form field;
  the noise threshold applies to both variants.
- Conformance checking of a log against **any** discovered model — every miner
  is translated to a workflow net (the Inductive Miner's process tree, the Alpha
  Miner's net directly, and the Heuristic Miner's dependency graph with its
  AND/XOR split and join semantics) — or against a **user-supplied model**: a PNML
  importer reads a workflow net discovered or drawn in another tool, so the
  same checks run on it (CLI `--check-pnml`, or upload it in the frontend's
  Conformance tab). **Token replay**
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
  guided by the marking-equation lower bound so even highly concurrent models
  stay alignable) — giving exact, optimal diagnostics of *where* and *how* each
  trace deviates, plus cost-based fitness. **Footprint conformance** compares the
  directly-follows relation of the log against that of the model (found by
  exploring the net's reachable markings): it reports how many of the log's
  successions the model allows (fitness) and how many of the model's the log
  shows (precision), and lists the deviating pairs. Token replay and footprints
  are robust on unsound nets; alignments report that they can't be computed when
  a net (e.g. an Alpha net with concurrency) has an unreachable final marking.
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
  PM4Py — and a PNML workflow net from any of those tools can be read back for
  conformance checking. Every miner reuses the same workflow-net translation
  used for conformance, so the exported net is exactly what Minera checks.
- **BPMN 2.0 export** for every miner: the inductive model converts block by
  block from its process tree, and the Alpha and Heuristic models convert
  from their workflow nets by token semantics — a branching place becomes an
  exclusive gateway, a forking transition a parallel gateway — readable by
  Camunda, bpmn.io, Signavio, and most BPM suites.
- **Model playout**: simulate any workflow net — a discovered model or an
  imported PNML — into a synthetic, timestamped event log (seeded, so runs
  reproduce). On a sound model every generated trace replays with fitness
  1.0, which makes the playout a self-check for discovery and a test-log
  generator (`minera_playout model.pnml -o log.xes`, or the **Simulate log**
  button on any mined model).
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
  activity repeats), between-extraction (cut each case into the subtraces
  running from one activity to another — e.g. every loop iteration), trace
  length, start/end activities, and variant frequency (minimum count or top-N)
  before discovery.
- `minera_server`, a local application server: it serves the frontend and
  mines uploaded CSV event logs with the native core (`POST /api/mine`),
  returning the event log, the directly-follows graph, the process tree, and
  conformance results.
- `minera_export` CLI that mines a CSV log and emits the event log, the
  discovered graph, the process tree, and conformance results as JSON, with
  optional log-filtering flags.
- A React + TypeScript frontend (`frontend/`) that uploads a CSV event log to
  the backend, shows the imported log first, and puts the directly-follows
  graph and the discovered process model one tab away each.

## Build

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

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
cd frontend && npm install && npm run build && cd ..
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
  graph) and explore the resulting model; the Heuristic Miner's dependency and
  frequency thresholds are live sliders that re-mine the net in the browser,
  **Download … as PNML / as BPMN** buttons export any miner's model for
  ProM/PM4Py or a BPM suite, and **Simulate log (XES)** plays the model out
  into a synthetic 100-case log,
- **Conformance** — pick any miner (or upload a PNML workflow net from another
  tool) and any method: fitness, precision, generalization, and simplicity of
  the log against that model, plus a per-variant breakdown of where traces
  deviate. **Token Replay** shows token counts; **Alignments** renders each
  deviating trace move by move (synchronous, log-only, model-only);
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
most frequent variants (top-N or a minimum frequency), a time window (keeping
the events inside it, or the cases contained in / touching it), rework (only
cases where a chosen activity repeats), a between cut (each case reduced to its
stretches from one activity to another), a subset of activities or resources,
an event attribute's values (dropping the matching events, or whole cases
with a match), or a case attribute's values (dropping whole cases); every
view — log, graph, model, conformance, and resources — updates together.

For frontend development, run the Vite dev server alongside the backend; it
proxies `/api` to `localhost:8484`:

```sh
./build/minera_server &
cd frontend && npm run dev   # → http://localhost:5173
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

`--check-pnml model.pnml` additionally checks the log against the workflow net
in that PNML file; its results appear in the export under the conformance id
`imported`, next to the discovered models.

`--decisions <miner>` additionally mines the decision points of one
discovered model — which attribute values steer a case down which branch —
and embeds the rules in the export under `decisions`.

`--xes out.xes` additionally writes the event log — after any filtering — as
standard XES, making the CLI double as a CSV-to-XES converter. `--bpmn
out.bpmn` writes a discovered model as BPMN 2.0, with `--bpmn-miner`
(inductive, alpha, or heuristic) picking which — mirroring `--pnml` /
`--pnml-miner`.

Other filters include `--min-variant-frequency`, `--keep-activities`,
`--remove-activities`, `--keep-resources`, `--remove-resources`,
`--start-activities`, `--end-activities`, `--min-length`, and `--max-length`.
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

Pass `--noise-threshold <f>` (0–1) to run the Inductive Miner's infrequent
variant (IMf) during discovery; `0` is the classic miner. The same value is
accepted on `POST /api/mine` as the `noise_threshold` form field. The other
miner settings travel the same two routes: `--inductive-variant log|dfg` /
`inductive_variant`, `--alpha-variant plus|classic` / `alpha_variant`, and the
Heuristic Miner's `--dependency-threshold` / `dependency_threshold` (default
0.9), `--and-threshold` / `and_threshold` (default 0.65), and
`--min-edge-frequency` / `min_edge_frequency` (default 1). In the frontend all
of them live in the **Filters & miner settings** panel, where every apply
re-mines the original upload — so a threshold can always be relaxed again.

The JSON export also carries the performance (`performance`) and resource
(`resources`) perspectives. To additionally write a model as PNML, pass
`--pnml <path>` (and `--pnml-miner inductive|alpha|heuristic`, default
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
`frontend/public/sample-data.json`:

```sh
./build/minera_generate_sample data/sample_event_log.csv
./build/minera_export data/sample_event_log.csv -o frontend/public/sample-data.json
```
