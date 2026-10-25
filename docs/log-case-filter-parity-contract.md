# Native public log case filtering

This companion targets `pm4py.algo.filtering.log.cases.case_filter`, pinned to
PM4Py 2.7.22 commit `0715c31f3f1d85924107d6aa5329806db69df899`. Its independently
implemented API is declared in `include/minera/log_case_filter.hpp`; the older
log-filter convenience functions remain unchanged.

## Captured public contract

The zero-algorithm observer at
`build/filtering-cases-public-surface-actual-1/capture.json` retains all seven
public function signatures, docs and the TIMESTAMP_KEY enum. Root executed the
prepared bank exactly once at `build/filtering-cases-log-public-actual-1`:
**180 distinct public calls, 88 returns and 92 errors**, with no input/control
mutations and unchanged module hashes. The pinned opaque module SHA-256 is
`9bd8c820b01edb79f1a7181f1a51ee68f43acb398a210ecb6a6c147f918dd711`.

Each receipt fingerprints the actual module/function, complete passed log or
trace, supplied positional arguments, typed keyword controls and actual call
shape. All candidates are audited before any target-method entry. Searches of
retained public records found no earlier algorithm receipts for this module;
the zero-call observer is separate evidence. The recipe preserves durable call
intents, complete before/after snapshots, returned values, error type/message/
args and public alias relationships. No reference source, AST, bytecode or
private state was inspected, and no captured algorithm call was replayed.

`apply` and `apply_auto_filter` always raise their respective captured
NotImplementedError, including when the input is empty or controls are supplied.
The corresponding native entry points preserve this behavior rather than
inventing a default filter operation.

Size filtering uses inclusive minimum/maximum comparisons in source trace
order. Its defaults are minimum two and no maximum. Maximum None omits the
upper comparison; an empty input performs no bound comparisons. Numeric bool,
integer and floating bounds retain Python comparison behavior, including NaN
and infinities, lazy invalid-upper-bound errors and NativeNA truth errors.

Case-count filtering follows the observed `min`-then-slice behavior: negative
limits omit traces from the end, positive limits clamp to the source count,
and bool limits act as zero/one. A float below the source count raises the
captured slice-index error; a greater float or NaN can select the complete
source because the integral source count becomes the slice endpoint. Invalid
comparisons can still fail on an empty log.

Performance uses the last event's selected timestamp minus the first event's
selected timestamp, without sorting, scanning interior timestamps or computing
minimum/maximum timestamps. Bounds are inclusive and lower comparison
short-circuits the upper comparison. An empty trace returns false before key
lookup. Singleton traces still perform both timestamp lookups. Default key is
`time:timestamp`; custom literal keys and the public enum control select the
same ordinary value. Missing interior cells do not affect the predicate.

Datetime arithmetic keeps exact integral seconds and microseconds before the
final floating `total_seconds` conversion. Offset-aware values compare resolved
instants; mixing naive and aware datetimes raises the captured TypeError.
Ordinary timestamp strings remain strings and fail subtraction. Numeric,
NativeNA, bytes, container and NaT timestamp values retain their observed
subtraction/attribute/comparison outcomes. NaT duration behaves as NaN.

`satisfy_perf` can return NativeNA when its upper comparison does. Therefore
`LogCasePerformancePredicate` explicitly holds bool or NativeNA. The filtering
functions subsequently test the predicate and raise ambiguous-truth TypeError
for NativeNA. A bool-only return type would discard an observed public result.
`filter_case_performance` and `filter_on_case_performance` retain equivalent
filter behavior through separate native entry points.

All four log-returning public functions discard log attributes, extensions,
classifiers, every global scope and log properties. They retain complete
selected trace attributes/properties and event values. Public traces/events
alias the source; the native value carrier gives independent ownership while
preserving values, trace source-key descriptors and event input order. The
source remains unchanged. Native legacy first-class timestamp text retains its
declared datetime role, while explicit ordinary attribute strings keep their
ordinary type.

## Native acceptance and boundary

The offline generator in
`build/filtering-cases-log-public-prepared-1/generate-native-tests.py` reads
retained observations only. It produces 49 complete deduplicated snapshots and
all 180 outcomes in `test_log_case_filter.cpp`. Non-inlined per-trace snapshot
builders and per-case functions avoid the monolithic MSVC stack overflow seen
in earlier unrelated generated tests. Acceptance compares complete returned log
serialization, scalar/NativeNA results, exact error types/messages and unchanged
source snapshots. Independent controls check opaque log-context removal,
trace-context preservation, source keys, event identity/order fields, copy
ownership and first-class timestamp storage fallbacks.

The canonical module remains partial after native/workflow acceptance because
the broader domains below still need their own audit. This bank does not establish arbitrary Python custom
objects, external object alias graphs, named timezone identity/fold behavior,
every mixed timestamp arithmetic combination, or sub-microsecond Timestamp
semantics. Unsupported native encodings are not evidence of full Python object
parity. All public API behavior described above comes from the retained bank;
engineering checks alone do not establish broader reference parity.

Final coordinated acceptance passed **19/19 native suites**, including all
**180/180** retained log outcomes and independent storage controls, in
`build/parity-closures/native-current/ctest-cases-final.log`. The successful
build is `build-cases-final.log` in that same directory. The first build log
and first five-suite acceptance receipt remain separately retained.

The freshly built owned production server passed **42 cross-family HTTP
requests** at `build/parity-closures/cases-production-http-1`, then exited;
the existing application was not replaced. The complete frontend suite passed
**183 files / 9,718 tests**, with final TypeScript and Vite checks passing.
These are shared workflow checks rather than new reference algorithm calls;
rendered browser interaction remains unverified.

`docs/log-case-filter-public.json.gz` preserves the observer, prepared recipe,
all complete public records/intents/audits and the offline generator.
`docs/log-case-filter-checks.json.gz` binds the current header/helper/test,
root and focused CMake context, contract and generator to final/historical
acceptance logs and compiled executable/map/object identities. Embedded
members retain individual byte lengths and SHA-256 hashes; the production
server hash is checked against the actual HTTP receipt.

## Disjoint bank closure (2026-10-25)

The module is promoted on the seeded public-call bank in
[log-filter-domain-parity.md](log-filter-domain-parity.md) (240,450 calls
across the seven EventLog filtering modules, zero failures), which also lists
the native rules that bank added.
