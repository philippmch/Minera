# Exact log between filtering

The target is `algo.filtering.log.between.between_filter`, pinned to PM4Py
2.7.22 commit `0715c31f3f1d85924107d6aa5329806db69df899`. Public `apply`
accepts an EventLog, two endpoints (`str` or `List[str]`) and optional
parameters. The independently implemented companion is
`filter_log_between_result` in `include/minera/log_between_filter.hpp`.
The older `segment_filter.hpp` convenience operations retain their behavior.

## Extraction behavior

An endpoint string is one literal label, not substring membership. Collections
retain their membership meaning; empty strings and empty arrays are valid.
Missing activity attributes raise at actual event lookup. Generic ordinary
null/numeric/bytes/date/UUID/container values do not become matching strings.
UUID values remain distinct from their textual spelling. NativeNA can raise
ambiguous truth when membership is evaluated, while an empty endpoint list
never evaluates a candidate equality.

A matching start opens a cut. The first subsequent matching end closes it,
retaining both boundaries and all intervening events. A repeated start inside
an open cut does not discard the original start. A closing event opens the
next cut at that same event only when the two original endpoint values are
equal: scalar string versus one-element array is different, and array order
and duplicate labels matter. Overlapping membership alone does not reopen a
cut. An unclosed final cut is omitted. Scanning order remains source
trace/event order.

Every opened cut receives the selected source trace ID, separator and a
zero-based cut number. Even a single emitted cut has its zero suffix. Missing
selected case IDs fall back to the original trace position as a decimal
string. Numbering restarts per source trace. An empty literal separator is
valid. The default separator is `##@@`, activity key `concept:name`, and trace
case-ID key `concept:name`.

Case-key hashing and concatenation occur when a cut opens, so their errors can
occur before any closing endpoint exists. They do not occur in an empty trace
or one with no matching start. Source null and NativeDate NaT produce distinct
concatenation type errors. NativeNA arithmetic follows the retained sentinel
behavior, including its observed null-operand error.

Concatenation preserves the point of failure. Numeric pairs, matching list or
tuple pairs and matching byte pairs can complete the first addition, then fail
when the decimal cut number is appended as a string. Their diagnostic identifies
that second operation. NativeNA can propagate through the complete expression.

The result keeps complete log attributes, extensions, classifiers, every
global scope and properties. It copies trace attributes, resets trace
properties, and retains complete selected event values. The public result
reuses event objects; native value ownership provides independent copies.
Explicit native source-key/role/presence descriptors remain compatible with
the assigned case value. Updating a custom trace attribute does not retarget
an unrelated canonical Trace.case_id or its source key. Native event case IDs,
input order and opaque compatibility snapshots survive copying.

## Complete result and projection

EventLog stores string trace keys, while public CASE_ID_KEY can be a hashable
ordinary nonstring value. The full native `LogBetweenFilterResult` contains
`string_key_log` and an ordered typed trace-attribute sidecar. Each sidecar
entry identifies its result trace and carries an EventAttribute key and value.
The sidecar is logically appended after the original string attributes.
Null, bool, integer, tuple and UUID keys remain typed, with no conversion to a
lookalike string. No existing EventLog carrier is widened or silently coerced.

`project_event_log()` explicitly rejects a nonempty typed-key sidecar.
`filter_log_between` offers this projection for ordinary string-key consumers.
The full serializer emits `log-between-result-v1` with complete byte-safe
`string_key_log` and a native EventTable `typed_trace_attributes` carrier. Its
trace_index/key/value rows preserve typed payloads, including UUID, null and
recursive tuple keys. Native control errors have full length-aware `.type`
and `.message` fields.

## Public evidence and coverage audit

The first prepared bank had 128 candidates. Before algorithm entry, root
normalized actual call shapes and removed `defaults-enum`, which duplicated
`defaults-dict` as an empty dictionary supplied through keyword parameters.
The rejected candidate/audit remain preserved. Root then executed exactly
127 distinct calls: 89 returned logs and 38 errors. A disjoint 21-call lazy
edge continuation returned 12 logs and nine errors. A disjoint seven-call
UUID/NaT continuation returned four logs and three errors. A disjoint eight-call
concatenation continuation returned two logs and six errors. A disjoint six-call
endpoint-equality continuation returned six logs. These **169 distinct
calls** contain **113 full returns and 56 errors**, with zero source, target
or parameter mutations and unchanged pinned module hashes.

Every receipt includes the complete original input, both typed endpoint
values, effective supplied controls and actual call shape. All new receipts
were deduplicated against each other and all retained earlier receipts before
any target-method entry, with durable call intents. Only public signatures,
docs/enums, public return values/errors and opaque hashes were observed. No
reference source/AST/bytecode/private state was inspected, and no retained
algorithm call was replayed.

`docs/log-between-public.json.gz` preserves the zero-algorithm public observer,
all five prepared banks, their complete actual records, pre-entry audits and
durable intents, and the offline native-test generator. Its 39 members retain
individual byte lengths and SHA-256 hashes. The packager performs no reference
imports or algorithm calls. The initial duplicate was removed before method
entry; all 169 executed receipt identities are distinct.

The offline generator reads only retained observations and produces
`test_log_between_filter.cpp`, with 121 deduplicated full snapshots and all
169 outcomes/errors. Returned logs compare complete raw native serialization;
nonstring attributes compare complete typed sidecars, and projection rejection
is checked separately. Native controls additionally inspect source-role
provenance, canonical/custom case-ID alignment, event order, copy ownership,
opaque snapshots and dictionary-wrapper semantics.

The valid public String/ListString endpoint domain is implemented over the
ordinary native value carrier, with finite typed-control error acceptance.
This does not claim arbitrary Python custom objects, external alias graphs,
unrepresentable nested dictionary-key carriers or every out-of-annotation
endpoint/control combination. Row completion requires the final native suite,
source binding, usable-workflow acceptance and a review of remaining ordinary
native-domain gaps; the observation count alone does not establish closure.

## Typed-selector continuation

After the 169-outcome checkpoint, a separate 20-call bank examined naive and
offset datetimes, finite and nonfinite float selectors, signed zero, empty and
singleton frozensets, recursive tuple/frozenset selectors, nested mutable-key
errors, NaT, typed result keys and ordinary separator errors. Root executed
each complete receipt once after auditing all five earlier banks. Four calls
returned complete logs and sixteen raised errors, with no mutations or module
hash changes. The combined acceptance now contains **189 outcomes: 117 returns
and 72 errors**, represented by 127 deduplicated complete native snapshots.

Missing-key rendering retains datetime constructor notation and fixed offsets,
shortest float notation including `-0.0`, NaN/inf, `NaT`, and recursive immutable
key notation. The implementation reuses the existing native formatter for
numeric values and nested datetime representation without changing that helper.
Typed date/frozenset/NaN result keys remain explicit sidecar entries, and nested
unhashable selectors still fail only when the corresponding lookup occurs.

`docs/log-between-selector-public.json.gz` preserves the new prepared recipe,
cases, full observations, complete pre-entry audit, durable intents and updated
offline native-test generator. The earlier 169-outcome public archive remains
unchanged so the previous checkpoint's source and evidence bindings stay intact.

The canonical module remains partial. Although the complete annotated endpoint
domain and the literal-string workspace controls are implemented, the public
parameter dictionary admits other selector/separator shapes. The continuation
establishes the observed ordinary date/float/frozenset diagnostics; it does not
establish every timezone identity, runtime hash-order display of multi-element
frozensets or every separator arithmetic combination. Nonstring nested
dictionary keys also remain a carrier boundary. These are separate from the
retained typed result keys, which are represented explicitly rather than
discarded.

The first native acceptance run hit Windows stack overflow in the generated
monolithic test function. The generator now uses separate non-inlined case and
trace-snapshot functions, retaining all complete comparisons while bounding
each stack frame. The original compiler failure, failed CTest receipt and
direct crash receipt remain retained alongside final acceptance.

The first stack-safe run exposed an overly broad closing-event reuse
condition. Complete retained overlap snapshots isolated the branch, and the
six fresh endpoint-equality observations established that original value
equality governs reuse. The correction retains scalar/array distinctions,
array order and duplicates; it does not convert endpoints to sets.

The 169-outcome checkpoint passed all seven native suites in
`build/parity-closures/ctest-between-workflows-3.log`, including all 169 log
outcomes and the independent storage controls. The fresh production server
passed 33 HTTP requests in `build/parity-closures/between-production-http-2`,
including scalar/array equality distinctions and unequal overlap. TypeScript
and Vite checks passed; the complete frontend suite and its source bindings
are recorded in the separate workflow acceptance archive. These checks do
not establish rendered browser interaction or close the remaining parameter
domains described above.

The typed-selector continuation passed all **189** log outcomes and native
storage controls in `native-current/log-between-selector-direct.log`. Final
coordinated acceptance passed **14/14 native suites** in
`build/parity-closures/native-current/ctest-between-ordinary.log`. The freshly
built production server passed **37 cross-family HTTP checks** in
`build/parity-closures/between-ordinary-production-http-2`; its executable hash
is bound to those receipts, and the owned server exited afterward. The existing
application was not replaced. The frontend's eight source/test hashes remain
unchanged from the passing 9,711-test checkpoint, as recorded separately by the
root acceptance audit; this continuation required no frontend edits or rerun.

`docs/log-between-selector-checks.json.gz` binds the current helper/header,
generated test, contract, generator and public continuation archive to these
logs, the compiled helper/test objects, test executable and production server.
Each embedded member retains its byte length and SHA-256. These are bounded
acceptance results; the canonical module remains partial and rendered browser
interaction remains unverified.

## Disjoint bank closure (2026-10-25)

The module is promoted on the seeded public-call bank in
[log-filter-domain-parity.md](log-filter-domain-parity.md) (240,450 calls
across the seven EventLog filtering modules, zero failures), which also lists
the native rules that bank added.
