# Minera

Minera is an early C++ process mining core built to explore PM4Py-style
workflows with native performance.

## Current Core

- CSV event-log import with configurable case, activity, and timestamp columns.
- In-memory event log model grouped into traces.
- Directly-follows graph discovery with activity, start/end, edge, and variant
  counts.
- Process-model discovery with the **Inductive Miner**: recursive
  exclusive/sequence/parallel/loop cuts on the directly-follows graph yield a
  block-structured process tree that is always sound and perfectly fits the log
  (logs without a cut fall through to a flower model).
- Conformance checking of a log against the discovered process tree: the tree
  is translated to a workflow net, then token-based replay measures **fitness**
  (how much of the log the model reproduces) and escaping edges measure
  **precision** (how little extra behavior the model allows), per variant and
  in aggregate.
- Log preprocessing: filter an event log by activity projection, trace length,
  start/end activities, and variant frequency (minimum count or top-N) before
  discovery.
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
optionally `timestamp`. If `timestamp` is present, events inside each trace are
sorted lexicographically by timestamp, which fits ISO-8601 exports.

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
columns; a bundled order-to-cash sample is one click away). The frontend
uploads it to `POST /api/mine`, where the native core imports and mines it
in memory — the log never leaves your machine. An opened log shows the raw
events first; discovery lives in tabs:

- **Event Log** — the imported cases with their event sequences and
  timestamps, filterable by case id,
- **Directly-Follows Graph** — an interactive process map (pan, zoom, and a
  minimum-frequency path filter), a variant explorer that highlights paths on
  the map, and per-activity frequency, entry, and exit statistics,
- **Mine a Process Model** — pick a discovery algorithm (currently the
  Inductive Miner) and explore the resulting block-structured process tree,
- **Conformance** — fitness and precision of the log against the discovered
  model, plus a per-variant breakdown of where traces deviate.

A **Filter log** bar above the tabs re-mines the original upload focused on the
most frequent variants (top-N or a minimum frequency) or a subset of activities;
every view — log, graph, model, and conformance — updates together.

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

`minera_export` accepts options for custom column names and delimiters, plus
log-filtering flags applied before discovery — for example, focus on the most
common behavior:

```sh
./build/minera_export my_event_log.csv --top-variants 5 -o my_export.json
```

Other filters include `--min-variant-frequency`, `--keep-activities`,
`--remove-activities`, `--start-activities`, `--end-activities`, `--min-length`,
and `--max-length`. Run `minera_export` without arguments to see usage. The same
filters are available on `POST /api/mine` as form fields (`top_variants`,
`min_variant_frequency`, `keep_activities`, …). The HTTP layer uses the vendored
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
