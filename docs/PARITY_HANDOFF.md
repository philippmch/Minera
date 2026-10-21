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

## Latest adjacent-path checkpoint

Four functional commits are pushed after `010ec3db`:

- `6d9b1a85`: log paths membership/performance and public count/threshold/
  dictionary helpers, with 182 complete outcomes and separate public/checks archives.
- `54f02501`: pandas paths membership/performance, typed parameters, exact
  complete table/index snapshots and 266 outcomes.
- `35aa54ed`: Polars eager/lazy paths helpers, dictionaries and full diagnostics,
  with 238 outcomes and retained EOF-only driver cleanup evidence.
- `849d5487`: usable log/table paths controls, genuine upload, complete
  artifacts, literal roles, XES reopening and actual production acceptance.

Three independent native paths companions and usable log/table workflows now
cover **686 distinct public calls**, with **686 exact projected outcomes**:
182 log (118 returns / 64 errors), 266 pandas (155 tables / 111 errors), and
238 Polars (153 frames / 85 full type/phase/message diagnostics). All three
canonical rows remain partial; only their evidence pointers changed. The full
catalogue remains **46 complete / 40 partial packages**, **226 complete / 161
partial modules**, and **201 strict open units** across all 86 packages and
387 modules. No finite bank is a claim of whole-module closure.

Read `docs/paths-filter-workflow.md` and the three `*-paths-filter-parity-contract.md`
contracts. Ordinary paths retain complete matching cases; performance paths use
inclusive duration bounds. Log preserves original trace order; dataframe
engines sort case/timestamp before adjacency. Pandas uses fractional elapsed
seconds and Polars integral seconds. Comma-key collisions, split source/target
columns, literal selectors, typed diagnostics, metadata/dictionaries/index and
complete downloads have explicit acceptance. Performance ignores the native
target-column parameter; the usable HTTP workflow rejects that inactive control.

New banks are **executed and accepted; never replay them**:

- `build/filtering-paths-log-public-actual-1`: 174 distinct calls;
  `filtering-paths-log-temporal-public-actual-1`: four;
  `filtering-paths-log-helper-edge-public-actual-1`: four. The latter bank
  audited all 178 preceding full receipts before entry. All 182 log outcomes
  have complete warning lists, with zero warnings and mutations.
- `build/filtering-paths-pandas-public-actual-1`: 250 distinct calls after two
  duplicate candidates were removed before entry;
  `filtering-paths-pandas-continuation-public-actual-1`: 16 disjoint calls after
  auditing the preceding 250 full receipts. The first bank's plain parameter
  dictionaries deliberately retain nested tag dictionaries as actual values;
  the continuation recursively decodes real NaN/infinity/Timestamp scalars.
  Preserve both banks as observed. All 266 have zero warnings or mutations.
- `build/filtering-paths-polars-public-actual-1`: 226 distinct calls;
  `filtering-paths-polars-strings-public-actual-1`: twelve disjoint calls after
  auditing the preceding 226 full receipts, with public unoptimized plans.
  One algorithm warning and fifty dictionary-observer warnings remain
  retained; native warning emission and physical plans remain open.

Only public calls/signatures/docs/enums, public plan output and opaque hashes
were used. Root executed all reference recipes; workers implemented offline.
No algorithm call was replayed and no implementation source was inspected.

Final acceptance passes **24 assertion-enabled native suites**, **44 actual
production HTTP checks**, **184 frontend files / 9,726 tests**, TypeScript and
Vite. Native receipts are under `build/parity-closures/native-current/`;
frontend receipts are `paths-frontend-full-final.log`,
`paths-frontend-tsc-final.log`, `paths-frontend-vite-final.log` under
`build/parity-closures/`, with output `paths-frontend-final-dist`.
HTTP acceptance is `build/parity-closures/paths-production-http-3`; server SHA256
`4a2702f822276268827516bfbd46a691dc1f3d98e9f64f81ce2af76f08d3f825`.
Every owned server exited; the existing application was not replaced.
Rendered browser interaction remains unverified. Offline catalogue validation
passes in `build/parity-closures/catalog-paths-final.log`.

The successful coordinated build is `build-paths-final2.log`, with all 24
suites in `ctest-paths-final2.log`. Final staged whitespace checking found one
extra EOF newline in the Polars test driver. Its original driver and archive
are retained under `build/parity-closures/polars-paths-filter-acceptance/pre-eof/`;
the byte audit proves only that terminal LF was removed. The repaired target
was rebuilt in `build-paths-eof-final.log`, then all 24 suites passed again in
`ctest-paths-eof-final.log`. The final Polars and workflow archives bind that
driver/object/executable/map. Log and pandas evidence remains bound to their
unchanged final2 sources/binaries. No frontend or production server bytes
changed during this cleanup.

`docs/paths-filter-workflow-checks.jsonl.gz` retains **104 members**, **36 source
bindings** and **57 compiled identities**. It is 1,005,938 bytes, SHA256
`ac809026b42665a05b7aa6089c2019502e569f26af487b33ad3e0d71148b3a93`.
Manifest: `build/parity-closures/paths-workflow-evidence-manifest.json`;
packager: `package-paths-workflows.py`. The offline
`verify-paths-current-bindings.py` verifies all **164 current bindings**, all
104 embedded member hashes and the three committed partial ledger rows.
Dedicated module archive hashes are:

- Log public: `1e642d9d952c77ea333603268df2ed0a467f03ad91678216d14fcf156a81ddc7`;
  log checks: `6598bdfa4fa2db698bc93efa4b68146ebd45a7740912e2c64b65f740c7b83b5f`.
- Pandas checks: `27060361e64d87cf43d0d17360d73db20792a1d35bb38b54909a47b13c827925`.
- Polars checks: `15a6e4e5f3cd41effb55605186669295c3de96222e887aea51f3deb7a9109bb9`.

The first build's pandas private-function ADL collision and the initial three
native failures remain retained. Repairs preserve dictionary-membership
precedence in the log helper, project captured pandas Timestamp operands
independently of precision, and distinguish Polars eager direct predicates
from lazy cached predicates in complete temporal diagnostics. No expected
outcome was rewritten. HTTP attempts one and two retain their original scripts,
records and failures: their reopen calls omitted the exported activity selector.
The third uses that selector explicitly for XES/native/context reopening.
The workbench also discovers actual non-CSV roles, accepts the literal fallback
when no first-class role exists, and has a CSV-to-XES reopening regression.

Prior case, between, repetition and OCDFG evidence remains unchanged and
historical. All their declared gaps remain in scope. Next paths work should
use fresh discriminating banks for nullable/missing case IDs, equal timestamp
ties across typed cases, extreme pandas index labels (use exact integer
arithmetic), mixed temporal wrapper/zone domains, custom object protocols,
extension dtypes, helper-column collisions, warnings and Polars physical
plans/category identity. Do not infer exact log/pandas internal duration-limit
sentinels from acceptance over the supported datetime calendar.

## Previous complete-case checkpoint

Four further functional commits are pushed after `fb1e0e7c`:

- `b6a0cc81`: public log size/count/performance helpers, explicit unavailable
  generic entry points, typed predicate/error behavior and complete selected
  traces with observed empty log-level context; 180 retained outcomes.
- `e1988a54`: pandas case helpers, count-frequency selection, first/last
  duration, business-hour controls and complete table/index/dictionary context;
  259 retained outcomes, including 157 full tables and 102 exact diagnostics.
- `57b6f280`: Polars eager/lazy case helpers, max-minus-min integral-second
  duration, full apply/collect diagnostics and explicit count-tie acceptance;
  225 calls have 134 exact frames, 79 full errors and 12 admitted tie projections.
- `ce4898f8`: usable log and table size/count/duration controls, genuine table
  upload, literal roles, complete downloads, correct log reopening and production
  HTTP acceptance. Read `docs/case-filter-workflow.md` and the three case-filter
  contracts. All three canonical case rows remain partial; only their evidence
  pointers changed in the ledger. Counts remain 46/40 packages, 226/161 modules
  and 201 strict open units. All 86 packages and 387 modules remain in scope.

Final acceptance passes **19 assertion-enabled native suites** in
`build/parity-closures/native-current/ctest-cases-final2.log`, including the
689 representable between outcomes and both retained table repetition suites.
The main successful build is `build-cases-final.log`; a trailing blank line in
the Polars test driver was removed after diff checking, then that target was
rebuilt in `build-cases-eof-final.log` and all 19 suites passed again. An offline
byte audit retains both drivers and proves only terminal whitespace changed.
The initial build's wrong `EventLog.properties` test field and its corrected
`metadata.properties` form are documented; the failed build and first five-suite
success remain retained. No product workaround was made for either test issue.

The freshly rebuilt server passes **42 actual production HTTP checks** in
`build/parity-closures/cases-production-http-1`, including all operations,
custom/NUL roles, full integer storage, reopening, business hours and failure
phases. Server SHA256:
`44ad78e0753ef501a80a185dc32a74a0c65c359383f7f567b61e31ac558810b2`.
The independently owned server exited; the existing application was not
replaced. Rendered browser interaction remains unverified. Frontend acceptance
passes **183 files / 9,718 tests**, TypeScript and Vite:
`build/parity-closures/cases-frontend-full-final.log`,
`cases-frontend-tsc-final2.log`, `cases-frontend-vite-final2.log`;
output is `cases-frontend-final-dist`. No frontend sources changed afterward.
Offline declaration checks pass in `catalog-cases-final.log`.

`docs/case-filter-workflow-checks.jsonl.gz` retains **80 members**, **35 current
source bindings** and **47 compiled identities** with all final/historical
workflow receipts. Manifest: `build/parity-closures/case-workflow-evidence-manifest.json`;
packager: `package-case-workflows.py`. The archive is 794,512 bytes, SHA256
`a1207e5ed3e3718054f894662e87d8c6da3a766317b1f734657049bbe754485c`.
Dedicated log, pandas and Polars archives retain full public inputs/results,
dedup audits, generators, diagnostics and native evidence. Earlier between and
repetition archives remain historical snapshots and are unchanged.

The following new banks are **executed and accepted; never replay them**:

- `build/filtering-cases-log-public-actual-1`: 180 distinct calls, 88 returns,
  92 errors. Public `satisfy_perf` can return NativeNA; its native carrier is a
  bool/NativeNA variant. Filtering then evaluates truth and can raise.
- `build/filtering-cases-pandas-public-actual-2`: 259 distinct calls, 157 full
  tables and 102 errors. Actual-1 stopped before any method entry because the
  observer used the invalid encoding name `utf8-sig`. Prepared-2 changes only
  that spelling to `utf-8-sig`; the original recipe and zero-call attempt remain.
  Six emitted warnings lack per-call receipts; do not claim warning parity.
- `build/filtering-cases-polars-public-actual-1`: 213 distinct calls, 134 frames
  and 79 errors. `filtering-cases-polars-ncases-order-public-actual-1` adds twelve
  disjoint successful calls and their public unoptimized plans, after auditing
  all 213 earlier full receipts. Across both banks, 134 native returned frames
  compare exactly and 12 count cutoff ties use explicitly admitted semantics;
  do not claim exact captured tie choices. Ninety-six algorithm warnings and
  separate dictionary-observer warnings remain retained, without native emission.

All 664 new public calls have their declared native acceptance. Complete bank
receipts were audited before algorithm entry; no prior direct case-filter calls
were found and no algorithm calls were replayed. Only public calls/docs/enums,
public plan output and opaque module identity hashes were used.

Next case-filter work should target discriminating uncovered combinations:
pandas categorical count ties, nullable numeric IDs with missing values,
subsecond duration boundaries and unobserved typed parameter/extension/schema
domains; log typed parameter adapters, mixed temporal class/resolution/timezone
domains and object identities; Polars unstable cutoff choices, warnings,
additional dtypes/selector expressions, category physical identity and execution
plans. Native Polars uses encounter order among equal-frequency groups; the
retained public plan permits other tie choices and a null group can consume a
slot before disappearing in the join. Keep those limits explicit. All earlier
OCDFG, repetition, between, platform/connector, solver and performance gates
below also remain in scope. No owned functional drafts remain.

## Earlier pushed checkpoints

Fourteen functional continuation commits are pushed after `7849925b`:

- `7cfe40d9`: locale-independent OCEL annotation/conformance JSON, exact signed
  zero and int64 boundaries. The embedded writer is
  `write_ocpn_annotation_fields` (the previous next-step note named a nonexistent
  `write_ocdfg_content`). See `docs/ocel-numeric-json-contract.md`.
- `9c40cfa7`: ordered OCDFG edge associations, three set projections, literal
  selectors and usable explorer tables/download. All seventy-seven retained
  outcomes now have native acceptance across timing/discovery/helpers. Five
  native suites, thirty-six focused frontend tests, TypeScript, Vite and eight
  actual production-handler requests pass. The full module remains partial;
  see `docs/ocdfg-edge-metrics-parity-contract.md`.
- `3def003b`: typed attribute repetition in the existing log-filter workspace.
  All 109 distinct retained outcomes, seven native suites, fourteen production
  HTTP checks, 9,688 frontend tests, TypeScript and Vite pass. Full native JSON,
  XES projection and reopening retain the correct activity selector. The new
  companion preserves lazy bounds, zero-match/empty traces, exact integers and
  NativeNA truth errors; its canonical row remains partial for unsupported
  object domains. See `docs/log-attribute-repetition-parity-contract.md`.
- `9a13274d`: pandas repetition companion, complete table/index/dtype/category
  metadata and 139 retained native outcomes.
- `c03443b0`: Polars eager/lazy repetition companion, 105 retained outcomes,
  literal columns and exact apply/collect error type, phase and full message.
- `7a207581`: 86 further log outcomes for ordinary value shapes/bytes and ten
  exact escaped pandas missing-column errors. Complete error strings have a
  length-preserving native carrier.
- `a5e7de37`: usable pandas/Polars table modes and native table downloads,
  separate engine phases and binary byte target controls in log mode.
- `71ce83af`: exact log between extraction, original endpoint equality,
  zero-based case suffixes and a complete result for typed trace keys;
  169 retained outcomes pass.
- `e035671a`: pandas between extraction on complete tables, dtype-specific
  missing cases, nanosecond equality, index/category/context preservation;
  139 representable outcomes from 140 public calls pass.
- `723f063e`: eager/lazy Polars between extraction with canonical timestamp
  sorting, latest-start slices, lexical interval numbering and full diagnostic
  phases/bytes; all 333 outcomes pass.
- `dc1d36dc`: usable between controls in the log and table workspaces, complete
  downloads, literal controls, correct reopening, actual HTTP acceptance and
  final source/binary evidence bindings.
- `ec101e14`: nullable UInt64/Float64 and float32 case projections, float32
  label formatting, nested Timestamp/stdlib datetime labels and nested NaT;
  18 new full pandas tables pass alongside the unchanged 139-outcome suite.
- `156c5eae`: Polars category-label dictionaries and Enum declaration order,
  cached Enum ranks and strict missing-dictionary/unknown-value rejection.
  Existing table metadata now admits Polars dictionaries without changing its
  ABI; complete codec/lazy/workbench transport and shared validation regressions
  pass. Ten fresh complete dictionary-label projections extend Polars to 343.
- `2cd4127c`: typed log selector diagnostics for dates, float/signed-zero/NaN,
  frozensets, recursive keys and NaT, plus typed result keys and lazy hashing.
  All 189 log outcomes pass; previous evidence archives remain unchanged.

The earlier seven functional commits after `e68e191d` remain relevant:

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

The OCDFG explorer now exposes the other nine retained edge helper outcomes:
ordered association triples and event-pair, full-triple and object-ID sets.
Literal role overrides, paginated inspection and complete JSON download are
available. All 77 retained OCDFG outcomes have native acceptance, but arbitrary
relation cell/dtype domains and the performance gate remain open.

Repetition now has separate log, pandas and Polars companions rather than a
shared grouping approximation. The public banks contain 195 distinct log calls,
150 pandas calls and 105 Polars calls. The native counterparts cover 195 log,
149 pandas (missing-required-argument call excluded) and 105 Polars outcomes.
Pandas/Polars consider cases observed among matching rows even at minimum zero;
the log module can retain zero-match and empty traces. The companions preserve
typed numeric/date/byte behavior, stored context and full returned rows.
Their three canonical module rows remain partial for the documented remaining
carrier/dtype/object domains. Read the three repetition contracts and
`docs/table-attribute-repetition-workflow.md`.

The filtering workspace offers separate native table modes with engine-specific
examples, complete metadata/integer-storage transport and full result/response
downloads. It does not convert those tables into logs. Log mode accepts typed
bytes as hexadecimal target JSON and retains native JSON/XES/opening behavior.
Earlier evidence archives are historical source snapshots. The repetition
workflow archive bound 21 sources at that checkpoint; the new between workflow
archive bound 34 sources at that checkpoint. The latest ordinary continuation
archive binds 41 current implementation/test/contract/CMake sources and 35
compiled identities; older archives remain historical snapshots.

Between now has separate native log, pandas and Polars companions. Its banks
contain 189, 158 and 343 distinct public calls respectively, with all 689
representable native outcomes passing. The missing-required-argument pandas
call is outside its C++ signature. Log boundary reuse depends on original
endpoint equality: scalar versus singleton array differs, and array order and
duplicates matter. Pandas keeps positional case blocks and the first open
start; Polars sorts by selected case and canonical `time:timestamp`, keeps the
latest start and ranks textual interval indexes lexically. Thus `10_11` can
receive a lower number than `2_3`, while output stays in sorted row order.
Complete output rows/context and the supported typed native carriers survive.

The numeric/temporal continuation covers missing UInt64/Float64 array
projections, float32 shortest labels and magnitude thresholds, nested
nanosecond Timestamp/timezone labels and standard datetime constructor forms.
Polars dictionaries preserve unused category labels and declaration order.
Categorical case sorting is lexical; Enum sorting follows its dictionary order.
Selected Enum sort columns require complete dictionaries, and unknown values
are rejected even for one-row inputs. Rank maps are built once per collection.
Named dtype declarations survive as text, but runtime category context aliasing
and physical codes are not represented. Category metadata API warnings are
retained separately from actual algorithm warnings. The rows remain partial.

Log nonstring result keys use an explicit typed sidecar; EventLog projection
rejects a nonempty sidecar. The workspace exposes literal string selectors and
offers complete result, native JSON, XES and response downloads. Table mode
accepts genuine native EventTable JSON Files, raw string/array endpoints and
literal case/activity/separator controls. It retains engine-specific errors
and full table downloads without converting tables to logs. All three
canonical rows remain partial; read their between contracts and
`docs/table-between-filter-workflow.md`.

## Prior ordinary-between validation

- **Fourteen assertion-enabled native suites pass**: all between targets,
  new pandas ordinary cases, original EventData validation, category codec/lazy/
  workbench transport, log/table routes and both table repetition regressions.
  Logs: `build/parity-closures/native-current/ctest-between-ordinary.log` and
  `build-between-ordinary.log`; configure: `configure-between-ordinary.log`.
- All **689 representable between outcomes** from 690 public calls pass,
  including complete returned values and retained diagnostic bytes/phases.
  The one excluded pandas call omits a required C++ argument. The latest 48
  public calls were disjoint from every earlier retained receipt; none replayed.
- **37 final production requests pass** on an independently owned server:
  `build/parity-closures/between-ordinary-production-http-2`. It exited and did
  not replace the existing application. Server SHA256 is
  `858abef8eca626a36754b884b02c53a018197d6d22cbd39db2d958c6bda8a4f5`.
  The first attempt's harness incorrectly compared minimal upload fields with
  canonical exports; its script/records/failure context remain retained. The
  corrected check compares the exported dictionary with the parsed source.
  No product change was needed for that harness failure.
- Frontend source/tests remain unchanged from the passing **182-file /
  9,711-test**, TypeScript and Vite checkpoint. Eight current file hashes are
  verified in `build/parity-closures/between-ordinary-frontend-unchanged.json`;
  no redundant frontend run was used for this C++/metadata-only continuation.
- Offline structural catalogue validation passes:
  `build/parity-closures/catalog-between-ordinary.log`. Counts remain 46/40
  packages, 226/161 modules and 201 strict open units; no row was promoted.
- `docs/between-ordinary-workflow-checks.jsonl.gz` retains 84 members, 41 current
  sources, 35 compiled bindings and all final acceptance receipts. Manifest:
  `build/parity-closures/between-ordinary-evidence-manifest.json`. Dedicated
  selector, pandas-ordinary and Polars-categorical archives retain the new
  public observations, pre-entry audits, generators and native proof.
  Original 169-log, 139-pandas and 333-Polars evidence remains historical and
  unchanged. Rendered browser interaction remains unverified.

## Earlier between foundation validation

- **Seven assertion-enabled native suites pass**, including the three between
  companions, log workbench/route and new table workbench/route:
  `build/parity-closures/ctest-between-workflows-4.log`.
- Final native/server build:
  `build/parity-closures/build-between-workflows-4.log`.
- Frontend **182 files / 9,711 tests**, TypeScript and Vite pass:
  `build/parity-closures/frontend-between-final-{full,tsc,vite}.log`;
  output: `frontend-between-final-dist`. Full tests used two workers and
  `--testTimeout 30000`. An accidentally duplicated runner was stopped with
  its own abort receipt; its output is not used as acceptance.
- **33 final production requests pass** on an independently owned server:
  `build/parity-closures/between-production-http-3`. It exited, and the existing
  application was not replaced. Server SHA256 is
  `3277e18249ed4129441023219f6d3ffc1608afab4a857bdc1e93fcdcf321eccc`.
- Offline structural catalogue validation passes:
  `build/parity-closures/catalog-between-final.log`. Counts remain 46/40
  packages, 226/161 modules and 201 open strict units. No between row was
  promoted from a finite bank.
- `docs/between-filter-workflow-checks.jsonl.gz` retains 75 members, current
  34-source bindings and six compiled bindings, verified by
  `build/parity-closures/between-evidence-manifest.json`. Dedicated public/native
  archives retain all three families' recipes, pre-entry dedup, complete
  returned outputs and diagnostics. Older archives remain historical snapshots.

The initial log compile needed an explicit Unicode helper include. The first
combined native run found generated-log-test stack overflow and pandas list
case-label formatting; the next isolated the log overlap/equality mismatch.
Small non-inlined snapshot/case functions repaired the stack footprint without
changing expected data. A fresh disjoint six-call public bank established the
original endpoint-equality rule. All failures and successful later runs remain
retained; no public calls were replayed. The Polars diagnostic's significant
trailing spaces now live inside quoted source lines: an offline audit and
native diagnostic tests establish identical 4,838 runtime bytes. All three
large generated test targets use `/bigobj` in root CMake as in the validated
focused project. Rendered browser interaction remains unverified.

## Earlier repetition checkpoint validation

- **Nine assertion-enabled native suites pass**, including all three repetition
  companions, the ordinary/literal continuations, old log workbench/route and
  both new table suites:
  `build/parity-closures/native-current/ctest-ordinary-repetition-final.log`.
- Final native/server build:
  `build/parity-closures/native-current/build-ordinary-repetition-final.log`.
- Frontend **181 files / 9,702 tests**, TypeScript and Vite pass:
  `build/parity-closures/table-repetition-frontend-{full,tsc,vite}.log`.
  One unrelated existing fixture exceeded its default five-second timeout
  during concurrent compilation; that failed run is retained. The final full
  run used `--testTimeout 30000`, with no test or production edit for the timeout.
- **34 final production requests pass** on independently owned servers:
  `build/parity-closures/table-repetition-production-http-2` (17) and
  `build/parity-closures/repetition-production-http-3` (17). Their receipts bind
  server SHA256 `1ba9fe6080c3f1cee06855e669b122fc26250485e9a2ca328b31c1b5d6c9ac7e`.
  Both servers exited. Earlier owned HTTP attempts remain retained.
- Offline structural catalogue validation passes:
  `build/parity-closures/catalog-repetition-final.log`. Counts remain 46/40
  packages, 226/161 modules and 201 open strict units. No repetition row was
  promoted from a finite bounded bank.

The first Polars test executable overflowed the Windows stack. Splitting
generated frame/case functions repaired the test harness without raising the
stack limit or changing expected data. The next run detected an omitted
`failed` marker in a temporal diagnostic; the general formatter was corrected.
Both failures and final success are retained in the Polars and workflow
archives. No reference calls in this continuation were replayed. Rendered
browser interaction remains unverified.

## Earlier OCEL checkpoint validation

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

- Continue the between modules' declared boundaries. The 20-case log-selector
  bank is now **executed and accepted**, with the equality bank explicitly
  included in its prior-receipt audit; do not replay it. Its retained output is
  `build/filtering-between-log-selectors-public-actual-1`. The pandas 16-case
  ordinary bank and two-case float32 sweep are also executed and accepted;
  retained outputs are `filtering-between-pandas-ordinary-public-actual-1`
  and `filtering-between-pandas-float32-public-actual-1` under `build/`.
  The ten categorical calls are at
  `build/filtering-between-polars-categorical-public-actual-1`, with 38 complete
  dictionary snapshots and separate observer warnings. Native acceptance
  covers label dictionaries, not physical codes/context identity. Keep log
  typed-source/nested nonstring-key carriers, unobserved selector forms, pandas
  extension dtypes/temporal resolutions/identity graphs, Polars warning
  emission, unobserved dtype/cast/diagnostic domains and physical plans open.
- Continue the wider OCDFG gate. Reuse the retained 77-call bank at
  `build/parity-closures/ocdfg-performance-public-actual-1/capture.json`;
  all retained timing/discovery/aggregation/strict-selector outcomes now have
  acceptance. Design only new disjoint inputs for remaining domains. This
  seventy-seven-outcome slice does not establish whole-module closure.
- Keep arbitrary relation dataframe cells, dtype/storage domains, non-string
  grouping IDs and actual performance benchmarking explicit. The new timing
  companion supports string identities; do not silently coerce other domains.
- Continue the three repetition modules' declared gaps. Ordinary log values,
  byte targets, container/date/byte bounds, unhashable selectors and literal
  pandas missing-key diagnostics now have disjoint acceptance. Custom equality
  objects, wrapper storage/identity graphs, arbitrary table schemas/dtypes,
  categorical dictionaries and Polars physical plans remain open as documented.
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
