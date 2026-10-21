# Native public log paths filtering

The target is `pm4py.algo.filtering.log.paths.paths_filter`, pinned to PM4Py
2.7.22 commit `0715c31f3f1d85924107d6aa5329806db69df899`. The independently
implemented companion is declared in `include/minera/log_paths_filter.hpp`.
Existing path-filter convenience operations retain their behavior.

## Public observations

The root-only zero-algorithm surface observer at
`build/filtering-paths-public-surface-actual-1/capture.json` retains all six
public signatures/docs and the public parameter enum. Root executed the main
bank once: **174 distinct calls, 114 returns and 60 errors**, zero warnings and
zero source/control mutations. A disjoint four-call temporal continuation
returned two logs and two errors, again with no mutations or warnings. The
178-outcome checkpoint is followed by a disjoint four-call helper-edge bank:
two returned logs and two errors, with zero warnings or mutations. The combined
bank contains **182 outcomes: 118 returns and 64 errors**. The opaque
module SHA-256 stayed
`2f96b1208c2cf3e2e05e7758108d323446d1a2f284bd7b9baa0aa0891219e25f`.

All complete actual arguments, including passed log/trace values and source
trace references, keyword controls, callable identity and call shape are
fingerprinted before any target method is entered. The entire prepared bank
is audited first, with durable duplicate removals, overlap rejection and call
intents. Searches of retained exact-module receipts found only the zero-call
surface before this bank. Each call retains complete input/result values,
metadata, errors, warnings and before/after controls. No reference source, AST,
bytecode or private state was inspected, and no captured call was replayed.

## Whole-trace filters

`filter_log_paths` implements public `apply`. It compares each adjacent pair
as a tuple against the supplied collection, in source event order, stopping
at the first match. Tuple/list identity matters: a list containing tuple
`("A", "B")` can match, while a list containing list `["A", "B"]` does not.
The workflow's pair-array JSON therefore requires an explicit conversion to
native tuple pairs. Strings remain literal values, including whitespace,
empty strings and control characters.

Empty/singleton traces perform no adjacent-pair lookup. Longer traces require
both selected attribute cells even with empty target paths. Finding a match
can avoid a later missing-cell error. Ordinary nonstring selected values do
not become matching strings; NativeNA equality can raise ambiguous-truth
TypeError. Positive/negative selection uses the observed ordinary Python truth
behavior, including empty controls and NativeNA errors.

`filter_log_paths_performance` implements public `apply_performance`. Its
provided pair also requires tuple shape; a two-element list does not match.
Only matching adjacent pairs access timestamps and bounds. It obtains each
datetime's floating epoch `timestamp()` and subtracts those two doubles, then
applies inclusive bounds in lower-before-upper order. This differs from exact
datetime subtraction used by the separate case-filter companion. A later
matching occurrence can satisfy the interval, and the first successful one
ends scanning. Nonmatching pairs do not require timestamps.

Offset-aware dates resolve their instants, while naive dates follow the host
local-time `timestamp()` convention. Mixing naive and aware values is admitted
by this public operation. Unsupported values retain the observed missing
`timestamp` attribute errors; NaT specifically raises ValueError
`NaTType does not support timestamp`. The default lower bound excludes negative
durations. The full aware Python calendar range, year 1 through year 9999, is
admitted in the positive direction by the default upper bound. Native infinity
represents this effective behavior over the supported datetime carrier; the
unobserved private numeric sentinel is not claimed.

Both whole-trace filters preserve complete log attributes, extensions,
classifiers, all global scopes and properties. Selected traces retain complete
attributes, properties and event values. The public implementation aliases
traces/events; native value ownership gives independent copies with matching
values, source keys and event order.

## Public helper results

`get_log_paths_from_log` preserves the public insertion-ordered path-count
dictionary. It joins ordinary activity values with a comma, retaining those
actual keys rather than substituting tuple keys. Missing selected attributes
skip an edge. Ordinary incompatible concatenations preserve their errors.
NativeNA can become a dictionary key. `LogPathsDictionary` is therefore an
ordered vector of typed EventAttribute key/value pairs; a string-only mapping
would discard an observed result.

`get_sorted_log_paths_list` returns an EventAttribute list of two-element
lists, preserving actual key/count types. Descending count sorting is stable
for equal counts. NaN comparison behavior and incompatible-count errors are
retained. A generic typed dictionary overload admits tuple keys; an ordinary
EventAttribute mapping overload also retains the observed empty-list and null
control outcomes.

`get_log_paths_threshold` starts with the first listed count. It adopts each
next count only while it is strictly greater than the current threshold times
the decreasing factor, stopping at the first failure. It retains the selected
count's actual type, boundary behavior, empty/malformed indexing errors and
ordinary multiplication/comparison errors.

`filter_log_by_paths` takes typed count entries, variant keys/trace lists, the
variant-count list, threshold and selected attribute. It protects paths of the
first listed variant. It drops empty traces, preserves trace attributes and
resets trace properties. It retains the first event, scans edges beginning at
event index one, appends both endpoints of each retained edge, and appends the
last event. These rules intentionally reproduce repeated event positions and
the omitted first-edge scan observed in complete public returns; they are not
replaced by a conventional event/path subset. A protected path still must
exist in the supplied count dictionary; protection bypasses the threshold,
not dictionary membership. Empty or differently typed path-key dictionaries
therefore retain only boundary events. The helper-edge bank additionally
establishes that protected first-variant paths use the selected attribute role,
and that NativeNA followed by null retains its distinct concatenation error.
Log context is preserved.

## Native acceptance and remaining boundary

The offline generator reads retained observations only. It produces **60
complete deduplicated snapshots and all 182 outcomes** in
`test_log_paths_filter.cpp`, using non-inlined per-case/per-trace functions.
Complete serialized logs, ordered helper dictionary/list/scalar values, exact
error type/message and unchanged sources are compared. Independent controls
inspect log/trace context, opaque snapshots, source keys, event identity/order
fields, independent result ownership and legacy first-class timestamp storage.

The canonical module remains partial. This bank does not establish arbitrary
Python custom objects, every mutable equality/hash interaction, all malformed
helper argument shapes, alias graphs, named-zone/fold identities, every native
sub-microsecond timestamp encoding or all host-local calendar limitations.
The effective default upper behavior is bounded to ordinary valid datetime
values, not arbitrary custom timestamp protocols. Native helper variant and
dictionary carriers preserve observed ordinary shapes without claiming every
possible Python mapping. Final native/workflow checks and their source bindings
are separate engineering acceptance, not broader reference parity.

The initial 178-outcome helper run exposed an overly broad protection condition
for absent dictionary keys. Complete retained empty/tuple-key dictionary returns
isolated the condition; the correction requires dictionary membership before
the threshold/protection alternatives. The original source/test/archive state
is preserved under `build/parity-closures/log-paths-first178-snapshot`, and the
first acceptance receipt remains retained alongside final checks.

Final coordinated acceptance passed **24/24 native suites**, including all
**182/182** retained log outcomes and independent native storage controls, in
`build/parity-closures/native-current/ctest-paths-final2.log`. The matching
successful build receipt is `build-paths-final2.log` in that directory.
Earlier build/helper acceptance logs and the first 178-outcome source snapshot
remain retained separately.

The freshly built owned production server passed **44 cross-family HTTP
requests** in `build/parity-closures/paths-production-http-3`, then exited;
the existing application was not replaced. Earlier HTTP harness attempts used
incorrect reopen selectors and remain retained with their original scripts,
records and validation failures. The final harness explicitly selects the
exported activity role when reopening native/XES context. The full frontend
suite passed **184 files / 9,726 tests**; final TypeScript and Vite checks also
passed. Rendered browser interaction remains unverified.

`docs/log-paths-filter-public.json.gz` preserves all three public banks,
recipes, complete observations/warnings, pre-entry audits/intents and the
offline generator. `docs/log-paths-filter-checks.json.gz` binds current
header/helper/test, CMake context, contract and generator to final/historical
acceptance logs, compiled executable/map/object identities and the actual
production server hash. Every embedded member retains its byte length and
SHA-256. These bounded checks do not promote the canonical module to complete.
