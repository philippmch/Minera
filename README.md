# Minera

Minera is an early C++ process mining core built to explore PM4Py-style
workflows with native performance.

## Current Core

- CSV event-log import with configurable case, activity, and timestamp columns.
- XES event-log import (IEEE 1849-2016) for the standard interchange format used
  by public datasets (e.g. the BPI Challenge logs), reading the common
  `concept:name` / `time:timestamp` attribute subset.
- In-memory event log model grouped into traces.
- Directly-follows graph discovery with activity, start/end, edge, and variant
  counts.
- Process-model discovery with the **Heuristic Miner**: a dependency measure
  weighs how often one activity follows another against the reverse, and only
  edges above a dependency and frequency threshold are kept — yielding a
  noise-tolerant dependency graph (heuristics net) where the Alpha Miner would
  be misled by infrequent paths.
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
  is never filtered away. A threshold of 0 is the classic miner.
- Conformance checking of a log against **any** discovered model — every miner
  is translated to a workflow net (the Inductive Miner's process tree, the Alpha
  Miner's net directly, and the Heuristic Miner's dependency graph as a
  one-token state machine), and both methods run on each. **Token replay**
  measures **fitness** (how much of the log the model reproduces) and escaping
  edges measure **precision** (how little extra behavior the model allows), per
  variant and in aggregate — alongside **generalization** (how evenly the
  replay exercises the model's transitions, so a model whose parts hinge on a
  few observations scores low) and **simplicity** (how lean the net itself is,
  by mean arc degree), completing the four standard quality dimensions. **Alignment-based conformance** finds, per trace,
  the cheapest sequence of synchronous, log-only, and model-only moves that
  walks the net from start to end (a uniform-cost search over the synchronous
  product) — giving exact, optimal diagnostics of *where* and *how* each trace
  deviates, plus cost-based fitness. **Footprint conformance** compares the
  directly-follows relation of the log against that of the model (found by
  exploring the net's reachable markings): it reports how many of the log's
  successions the model allows (fitness) and how many of the model's the log
  shows (precision), and lists the deviating pairs. Token replay and footprints
  are robust on unsound nets; alignments report that they can't be computed when
  a net (e.g. an Alpha net with concurrency) has an unreachable final marking.
- **Performance (time) perspective**: from event timestamps, Minera measures
  the waiting time on every directly-follows edge (count, mean, median, min,
  max, total) and the end-to-end throughput time of each case. An ISO-8601
  timestamp parser normalizes instants (handling `T`/space separators,
  fractional seconds, and `Z`/±HH:MM offsets); untimed events are simply skipped.
- **Organizational (resource) perspective**: with a resource attribute (a CSV
  `resource` column or the XES `org:resource` attribute), Minera builds the
  handover-of-work social network — how often work passes from one resource to
  another within a case — plus per-resource workload counts.
- **PNML export**: any discovered model can be written as PNML (ISO/IEC
  15909-2), the standard Petri-net interchange format read by ProM and PM4Py.
  Every miner reuses the same workflow-net translation used for conformance, so
  the exported net is exactly what Minera checks.
- Log preprocessing: filter an event log by time range (keep the events inside
  a window, or the cases contained in / intersecting it), activity projection,
  resource (performer) projection, trace length, start/end activities, and
  variant frequency (minimum count or top-N) before discovery.
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
optionally `timestamp` and `resource`. If `timestamp` is present, events inside
each trace are sorted lexicographically by timestamp, which fits ISO-8601
exports, and feeds the performance perspective; if `resource` is present it
feeds the organizational perspective.

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

- **Event Log** — the imported cases with their event sequences and
  timestamps, filterable by case id,
- **Directly-Follows Graph** — an interactive process map (pan, zoom, and a
  minimum-frequency path filter) with a **frequency / performance** toggle that
  relabels edges with mean waiting times (and shows median/mean case throughput)
  when the log is timed, a variant explorer that highlights paths on the map,
  and per-activity frequency, entry, and exit statistics,
- **Mine a Process Model** — pick a discovery algorithm (the Inductive Miner's
  process tree, the Alpha Miner's Petri net, or the Heuristic Miner's dependency
  graph) and explore the resulting model; the Heuristic Miner's dependency and
  frequency thresholds are live sliders that re-mine the net in the browser, and
  a **Download … as PNML** button exports the model for ProM/PM4Py,
- **Conformance** — pick any miner and any method: fitness, precision,
  generalization, and simplicity of the log against that miner's model, plus a
  per-variant breakdown of where traces deviate. **Token Replay** shows token
  counts; **Alignments** renders each deviating trace move by move (synchronous,
  log-only, model-only); **Footprints** compares the log's and model's
  directly-follows relations and lists the successions they disagree on. Every
  miner is checked at import; when a model isn't a sound workflow net the
  alignment view says so rather than showing misleading numbers (token replay
  and footprints still apply),
- **Resources** — when the log carries a resource attribute: the workload per
  resource and the handover-of-work network (how often a case passes from one
  resource to the next).

A **Filter log** bar above the tabs re-mines the original upload focused on the
most frequent variants (top-N or a minimum frequency), a time window (keeping
the events inside it, or the cases contained in / touching it), or a subset of
activities; every view — log, graph, model, conformance, and resources —
updates together.

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

Other filters include `--min-variant-frequency`, `--keep-activities`,
`--remove-activities`, `--keep-resources`, `--remove-resources`,
`--start-activities`, `--end-activities`, `--min-length`, and `--max-length`.
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
accepted on `POST /api/mine` as the `noise_threshold` form field.

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
