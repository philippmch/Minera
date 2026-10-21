# Full parity continuation — 2026-10-21

Resume in `C:/Users/pmoch/Documents/Minera`, branch `main`.

## Objective and authorization

Reach full pinned-reference parity, prioritizing usable features, then complete
fidelity. Keep all 86 packages and 387 modules in scope. The user explicitly
authorized parallel subagents and clean logical commits/pushes directly to
`main`. Do not ask again for routine implementation, tests or Git checkpoints.

Use only public black-box calls, public signatures/docs/enums and opaque binary
hashes. Never inspect reference implementation source, AST, bytecode or private
state. Do not replay retained calls. Preserve failed attempts and execute new
uncovered banks only in new output directories.

## Pushed checkpoints

Seven functional commits were pushed after `e68e191d`:

- `0d8e4399`: exact OCEL object attributes and shared typed NaT membership.
- `c552c2db`: usable exact object/NaT preparation controls and HTTP dispatch.
- `fa4980cb`: OCDFG working-time helper and optional discovery timing.
- `73d7de01`: working-time controls in the existing explorer and HTTP route.
- `742f24fd`: faithful OCEL activity/type matching companion.
- `4862885e`: usable exact matching, literal transport and receipt correction.
- `eed5ed07`: typed missing timestamps, canonical field access and corrected
  scalar/datetime projection in the native OCDFG adapter.

Coverage is **46 complete / 40 partial packages**, **226 complete / 161 partial
modules**, and **201 open strict coverage units**, down from 203 at the prior
handoff. Full parity is not complete. The whole OCDFG row remains partial.
Read `docs/parity-closure-status.md` and the machine-readable coverage ledger.

## Delivered behavior and evidence

Exact object filtering matches all 195 retained complete native projections
and both native errors. The new event NaT bank matches 12/12 returned logs.
Typed NaT is distinct from null and numeric NaN; scalar/date kinds, literal
names, exact numeric comparisons, unused selected objects and stable six-table
propagation are retained. See `docs/ocel-object-attributes-parity-contract.md`
and `docs/ocel-event-nat-membership.md`, with their compressed archives.

Exact activity/type matching matches all 155 returned rows and eight native
errors from a 138-call bank and a disjoint 30-call continuation. **One duplicate
exists inside the first bank** (`mapping-subset-1` / `mapping-key-domain-7`).
The 168 calls contain 167 distinct explicit receipts; the 155 successful rows
contain 154 distinct successful receipts. Both records and an offline audit are
preserved. Neither bank was rerun. Do not call this 168 unique observations or
an entirely replay-free bank. See the matching contract and evidence archive.

The preparation workspace now exposes five exact selectors, including
`object_attribute_exact` and `activity_type_matching_exact`. Matching uses
`allowed_json`, optional literal role names and omitted versus literal
temporary-column/separator controls. Multiline controls, complete downloads
and ordinary reopening are supported. Final snapshots and receipts are in
`docs/ocel-exact-filter-workflow-checks.json.gz`.

OCDFG acceptance covers 60 retained direct timing outcomes, eight complete
discovery outcomes/errors, default/custom/empty/null schedules, wall-clock
working time and inert calendar JSON. Absent optional options preserve the
legacy branch. Primary timestamps use datetime semantics; generic timing
columns require native event-type datetime declarations. Datetime NaT yields
zero elapsed time and the retained working-time error. Scalar/undeclared
strings, including ISO-looking text and NaT, remain literal. Discovery JSON
is locale-independent and retains negative zero. See
`docs/ocdfg-working-time-performance.md`. Its earlier acceptance archive is
historical; the separate typed-adapter archive binds the corrected final
sources and retained dtype projection. No new timing reference calls occurred.

## Final validation

- **Nine assertion-enabled native suites pass**, including original core,
  filters, events, objects, NaT, exact controls, matching and both timing suites:
  `build/parity-closures/native-current/ctest-matching-typedtime-final2.log`.
- Server and CLI freshly build:
  `build/parity-closures/native-current/build-matching-route-final2.log`.
- Frontend **179 files / 9,677 tests**, focused 29, TypeScript and Vite pass.
  Logs: `build/parity-closures/frontend-matching-exact-*`; final Vite log:
  `frontend-matching-exact-vite-final2.log`; output: `frontend-matching-exact-dist`.
- **Ten actual production-handler requests pass** on an independently owned
  temporary server, including matching/open output, NUL transport and NaT/
  scalar timing: `build/parity-closures/owned-final-ocel-http-2/validation.json`
  and `records.jsonl`. That server exited; the existing app was not replaced.
- Offline declaration/artifact/proof checks pass for 86 packages / 387 modules.
  This is not a fresh reference implementation/source audit.

Earlier failed builds, expectations and HTTP harness attempts remain retained.
The matching HTTP test now percent-encodes bytes itself because the vendored
test client's `httplib::detail::encode_url` truncates NUL. Production
URLSearchParams and server decoding passed actual NUL transport acceptance.
Rendered application interaction remains unverified.

## Next open requirements

- Continue the wider OCDFG gate. Reuse the retained 77-call bank at
  `build/parity-closures/ocdfg-performance-public-actual-1/capture.json`;
  inspect remaining aggregation/strict-selector outcomes before designing
  new disjoint inputs. Do not equate the 60+8 slice with whole-module closure.
- Conformance embedding's `write_ocdfg_content` retains a separate inherited
  locale boundary. The discovery serializer repair does not close it.
- Keep arbitrary relation dataframe cells, dtype/storage domains, non-string
  grouping IDs and actual performance benchmarking explicit. The new timing
  companion supports string identities; do not silently coerce other domains.
- Resolve Alpha+ identity-dependent cleanup selection. Both returned graph
  shapes occur in broader captures; the old single-process 28/64 mismatch
  count does not establish 28 incorrect models.
- All other partial rows, connectors/platforms, solvers, rendered UI and
  performance requirements remain in scope until their own evidence closes.

## Build coordination and safe staging

Python: `C:/Users/pmoch/AppData/Local/Programs/Python/Python311/python.exe`
with `-B -X utf8`; Node: `C:/Program Files/nodejs/node.exe`;
CMake/CTest: `C:/Program Files/CMake/bin/`.

Focused native project: `build/parity-closures/native-current/`, build `out/`.
It recompiles current overrides and affected server/CLI callers, linking the
other validated feature-99 core objects. Add sources/targets here and in root
CMakeLists. Tests require `/UNDEBUG`; coordinate shared MSBuild/project edits.
A timestamp anomaly once left controls code stale: verify source/object
bindings and force a fresh owned-source compile when evidence contradicts code.

No owned functional drafts remain. Preserve inherited dirty comparison metadata
in `docs/reference-2.7.22-coverage.json` and the historical journal change in
`docs/reference-2.7.22-parity.md`; do not stage those whole files. Preserve
untracked `node_modules/` and Python caches. Stage owned coverage rows with
`build/parity-closures/stage-rows.py ROW_JSON`. Structural check:
`node build/parity-closures/verify-declared-catalog.mjs . --project-root . --manifest build/parity-closures/coverage-for-index.json`.
Bind real tracked sources/tests, direct-main proof cases and every named
implementation symbol. Inspect staged scope and run `git diff --cached --check`
before each clean logical commit/push.

The existing application was not replaced. The prior handoff records a policy
rejection of server replacement and a browser permission gate. Do not retry
those rejected actions through another shell or disguise. Owned temporary HTTP
acceptance is separate evidence. Recheck listeners/PIDs before relying on
historical running-app identities.
