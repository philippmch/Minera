# Full parity continuation — 2026-10-21

The user is switching chats. Resume in this same checkout:
`C:/Users/pmoch/Documents/Minera`, branch `main`.

## Objective and authorization

Reach full pinned-reference parity, prioritizing usable features, then complete
fidelity. Keep the entire catalogue in scope. The user explicitly authorized
parallel subagents and clean logical commits/pushes directly to `main`.
Do not ask again for routine implementation, tests or those Git checkpoints.
The old chat's goal is currently paused; no agents remain active in its live
team. Start fresh subagents when the user resumes the work in the new chat.

Use only public black-box calls, public signatures/docs/enums, and opaque
binary hashes of the reference. Never inspect reference implementation
source, AST, bytecode or private state. Reference calls already captured below
must not be replayed. Preserve failed attempts and run only new uncovered
inputs in new output directories.

## Pushed checkpoint

Verified HEAD and `origin/main` both equal
`4f2dc141cea4ca04bc45c62362dc12bc7fb59ee9` before this handoff commit.
The latest four functional checkpoints are:

- `ff514160`: supplied case descriptions and original-case route; all 32
  retained byte/error outcomes pass, including output above 2 MiB.
- `ddf45c40`: usable six-operation log filtering workspace, HTTP adapter,
  complete native JSON/XES downloads and activity-key-aware opening.
- `19148d9c`: exact OCEL event membership/timestamp companions, 195 full
  returned logs and 50 native diagnostics.
- `4f2dc141`: usable exact OCEL membership/timestamp/count controls, exact JSON
  transport, literal-name reader, repeated classic relation preservation and
  file reopening through the explicit `minera:literalNames=true` marker.

The complete catalogue remains 86 packages and 387 modules: **46 complete /
40 partial packages, 224 complete / 163 partial modules, 203 open strict
units**, down from 228. Full parity is not complete. Read
`docs/parity-closure-status.md` and the machine-readable coverage ledger.

The final native exact-OCEL/core/filter/event checks passed 4/4:
`build/parity-closures/native-current/ctest-literal-ocel-multiplicity.log`.
The server translation unit was freshly compiled. The complete frontend suite
passed 9,667 tests; later focused metadata/handoff suites passed 220 tests and
the final timezone-hint suites passed 25 tests. TypeScript passed; final Vite
output is `build/parity-closures/frontend-ocel-exact-dist`, with log
`build/parity-closures/ocel-exact-frontend-vite-final.log`.

## Uncommitted work — preserve and finish

These files are local drafts, not verified production changes. Do not present
them as delivered or blindly commit them.

### OCDFG working-time performance

- `include/minera/ocdfg_edge_performance_options.hpp`
- `include/minera/ocdfg_edge_performance.hpp`
- `src/backend/ocdfg_edge_performance.cpp`
- `src/backend/tests/test_ocdfg_edge_performance.cpp`
- `src/backend/tests/ocdfg_edge_performance_public_vectors.inc`
- `src/backend/tests/ocdfg_working_time_discovery_vectors.inc`
- `docs/ocdfg-working-time-performance.md`
- `docs/ocdfg-working-time-public-evidence.json.gz`

Root executed the new 77-call public bank exactly once:
`build/parity-closures/ocdfg-performance-public-actual-1/capture.json`.
It has 59 successes and 18 errors. All source/parameter/aggregation snapshots
are unchanged. Direct timing slice: 48 returns and 12 errors. Draft acceptance
also covers eight complete discovery outcomes/errors. The helper recipe is
`build/parity-closures/capture-ocdfg-performance-public.py`.

Observed working-time arithmetic uses local wall clocks, ignores supplied
calendar None/false/string/dict controls, merges overlapping slots and retains
fractional, reversed and outside-week slots. NaT yields zero without business
hours and errors with them. Do not invent a holiday-calendar callback protocol
or close the whole OCDFG/performance gate from this bounded slice.

Next: register/compile the independent helper and test. Then append an
optional edge-performance options member to `OcdfgDiscoveryOptions` and
connect the captured timing behavior to discovery. An absent optional member
should preserve the existing legacy branch; explicit options activate the new
behavior. At handoff, `ocel.cpp`/`ocel.hpp` contain only the pushed literal
reader changes: discovery integration has NOT been applied. Rebuild all
server/CLI callers after extending an options struct. Expose usable working-
time controls in the existing app once native acceptance passes.

### Exact OCEL object attributes and missing datetime queries

Root executed the object-attribute bank exactly once:
`build/ocel-object-attributes-focused-public-actual-1/records.jsonl`, with
`validation.json`: **201 calls, 195 returns, six errors**, no input mutations.
Prepared recipe: `build/ocel-object-attributes-focused-prepared-1/`.
Draft `src/backend/tests/test_ocel_object_attributes_module.cpp` exists.
No exact object companion has been added to production yet.

Implement canonical object ID/type access, literal keys, numeric bool/int/
float equality without 2^53 rounding, datetime/string distinctions, polarity,
unused-object retention and stable six-table propagation. Reuse the exact
event comparison helpers in `ocel_filter.cpp` without changing closed event
behavior accidentally. Add a separate registered proof and actual acceptance.

New object observations expose a missing native query carrier: a typed NaT
query selects datetime NaT cells; None/NaN queries do not. The current event
query variant cannot express typed NaT either. Prepare a NEW small event-only
NaT bank (canonical/generic datetime columns; NaT/None/NaN; both polarities)
and let root execute it once before claiming event behavior. It has NOT been
prepared/executed yet. Add a distinct shared NaT sentinel and JSON `type=nat`
only after those observations. Existing column-kind controls prevent treating
ordinary date-looking strings as datetimes.

After native object acceptance, add `object_attribute_exact` alongside the
three existing exact selectors. Reuse `attribute_key`,
`attribute_values_json`, `column_kind`, `positive` and the current preparation
workspace's complete download/open flow. At handoff no frontend object-exact
selector or NaT JSON tag has been added.

## Validation and safe Git staging

Python: `C:/Users/pmoch/AppData/Local/Programs/Python/Python311/python.exe`,
normally `-B -X utf8`. Node: `C:/Program Files/nodejs/node.exe`.
CMake/CTest: `C:/Program Files/CMake/bin/`.

Focused native project:
`build/parity-closures/native-current/CMakeLists.txt`, build directory `out/`.
It compiles current override sources and freshly compiles the server, linking
the other validated feature-99 core objects. Add new sources/targets here as
well as the real root CMakeLists. Tests use `/UNDEBUG`; do not accept vacuous
Release assertions. The new OCDFG draft source/target is NOT registered yet.
Coordinate shared native compilation across agents to avoid simultaneous
MSBuild/project edits. Freshly validate affected original suites too.

Preserve inherited dirty changes in
`docs/reference-2.7.22-coverage.json` (comparison-module metadata) and
`docs/reference-2.7.22-parity.md` (historical journal line). Do not stage these
whole files. Keep inherited `node_modules/` and Python caches untracked.

Owned coverage-row staging helper:
`build/parity-closures/stage-rows.py ROW_JSON`.
Offline structural check:
`node build/parity-closures/verify-declared-catalog.mjs . --project-root . --manifest build/parity-closures/coverage-for-index.json`.
This uses unchanged pinned declarations, not a new reference source audit.
Artifacts must be tracked implementation/test files, not documentation.
Every named proof case must be called directly from main and exercise all
bound implementation symbols. Use genuine acceptance cases, not fake wrappers.
Check `git diff --cached --check`, inspect the staged scope, then commit/push
each verified logical step. Never promote a draft just to clear the tree.

## Runtime/browser evidence boundary

Last observed running app was the older feature-99 server on 127.0.0.1:8562;
the latest built binaries were not deployed over it. Automatic approval
previously rejected replacing that server with the reason "blocked by
policy". Do not retry the same rejected operation through another shell or
disguise. Browser navigation was also rejected by a saved permission gate;
rendered app interaction remains unverified. Do not present API/unit/build
checks as rendered UI proof. Recheck current state rather than assuming the
old PIDs/listeners still exist. Independent, owned ephemeral HTTP test servers
have been used successfully and do not replace the app.

The Alpha+ identity-dependent cleanup distribution gate and all other partial
rows, connector/platform, solver, rendering and performance requirements remain
open. Both Alpha+ graph shapes occur in the broader public capture, so the
old single-process 28/64 mismatch count is not proof of 28 wrong models.
