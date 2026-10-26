# Minera

Minera is a C++20 process-mining core with command-line tools, a local HTTP
server and a React frontend. It reads CSV, XES and OCEL logs, discovers process
models, checks conformance, simulates models and analyses performance, all in
one native process on your machine.

## Status

The behavioural target is the pinned **2.7.22 reference release**. The
[parity contract](docs/reference-2.7.22-parity.md) maps every reference package
and module to a Minera implementation, and
[`docs/reference-2.7.22-coverage.json`](docs/reference-2.7.22-coverage.json)
records the evidence for each. All 86 packages and all 387 modules are marked
complete, and CI runs the verifier with `--require-complete`.

Where the reference is deterministic, Minera matches it exactly. Where the
reference depends on hash seeds or solver tie-breaking (some alignment ties,
search counters and decomposed recomposition), the tests check invariants
instead: the result is valid, optimal and cost-consistent.

The server and the CLI run the reference ports for the ILP miner, the genetic
miner, process-tree alignments, decomposed alignments, SaCoFa trace-variant
queries, OCPN and object-centric causal-net playout, and Petri usage
comparison. Native options that the reference does not have (state and time
limits, custom costs), and inputs the reference cannot handle (for example a
process tree with an operator a variant does not cover), still run the earlier
native implementations. The [handoff log](docs/PARITY_HANDOFF.md) has
the per-package history.

## Build

You need CMake 3.20 or newer, a C++20 compiler (GCC 12+, Clang 15+ or MSVC
2022) and, for the frontend, Node 22.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build -j4 --output-on-failure
```

Test sources compile at `-O0` by default (`MINERA_TEST_OPTIMIZATION`) because
they embed large generated fixtures; the library keeps the build type's
optimisation. A clean build with tests takes about 35 minutes on 4 cores, and
`ccache` is used automatically when it is installed. Configure with
`-DMINERA_BUILD_TESTS=OFF` to build only the library and the tools.

## Run

Build the frontend once, then start the server from the repository root:

```sh
(cd src/frontend && npm ci && npm run build)
./build/minera_server        # then open http://localhost:8484
```

The launch screen takes a CSV or XES log; `data/sample_event_log.csv` is a
small order-to-cash sample. The server answers only browsers on its own
origin and on the Vite dev and preview ports (5173 and 4173). Add other
origins with `--allow-origin https://example.org`, which you can repeat.

The CLI tools in `build/` (`minera_export`, `minera_ocel`, `minera_playout`,
`minera_dfg` and about 35 more) cover the same algorithms without the server.
Run a tool without arguments to print its usage.

The [feature guide](docs/feature-guide.md) describes every algorithm, CLI flag,
API route and frontend workspace in detail.

## Library example

```cpp
#include "minera/csv_event_log_importer.hpp"
#include "minera/directly_follows_graph.hpp"

auto log = minera::import_csv_event_log("event_log.csv");
auto graph = minera::discover_directly_follows_graph(log);
auto count = graph.edge_count("Register request", "Check ticket");
```

By default the CSV importer expects `case_id`, `activity` and optionally
`timestamp`, `resource` and `lifecycle` columns.

## Tests and CI

`.github/workflows/ci.yml` runs four jobs on every push and pull request:

- **native**: Release build and the full `ctest` suite.
- **python**: the script tests (`python -m unittest discover -s scripts/tests`).
- **frontend**: `tsc --noEmit` and `vitest run` in `src/frontend`.
- **parity**: the parity verifier against the pinned reference commit, with
  `--require-complete`.

To run the parity verifier locally, check out the reference at commit
`0715c31f3f1d85924107d6aa5329806db69df899`, tag it `2.7.22`, and run:

```sh
node scripts/verify-reference-parity.mjs /path/to/reference-2.7.22 --require-complete
```

## Layout

| Directory | Contents |
| --- | --- |
| `include/minera/` | Public C++ headers. The reference ports are the `*_reference*.hpp` headers. |
| `src/backend/` | Library sources; `tools/` holds the CLI and server, `tests/` the CTest suites. |
| `src/frontend/` | The React/Vite application and its tests. |
| `scripts/` | Reference capture scripts, the parity verifier and helpers. |
| `docs/` | The parity contract, coverage manifest, handoff log, feature guide and evidence. |
| `data/` | The sample event log. |
| `third_party/` | Vendored build dependencies. |

## License

See [LICENSE](LICENSE).
