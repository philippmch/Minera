# Pandas timestamp: bounded public/native contract

The dedicated `pandas_timestamp_filter.hpp` companion implements eight captured
public functions of `algo.filtering.pandas.timestamp.timestamp_filter`, pinned
to PM4Py 2.7.22 commit `0715c31f3f1d85924107d6aa5329806db69df899`, pandas
3.0.6 and Python 3.11.9. The opaque module SHA-256 is
`8b4f29c47b5b69c00447f9e7ced092d456ac500eff50324d8f0281be02361e7e`.
The canonical module remains **partial**. Existing convenience timestamp filters
retain their separate contracts.

## Time interpretation and selection

String bounds follow the observed strict `%Y-%m-%d %H:%M:%S` grammar and become
UTC bounds. ISO strings with `T`, timezone suffixes and fractional suffixes
retain their captured parser errors. Typed UTC Timestamp and standard-library
datetime bounds retain their observed nanosecond or microsecond precision.
Missing, integer, NaN and list bounds preserve their captured
attribute/parser diagnostics and error precedence. Typed non-UTC date-bound
replacement behavior beyond the captured UTC domain remains unverified.

Event filtering retains individual rows whose timestamps fall inclusively
between the bounds. Case-contained filtering uses the minimum and maximum
nonmissing timestamp of each case, inclusively. Case-intersecting filtering
uses strict overlap: minimum < upper and maximum > lower. Starting and
completing modes compare the minimum and maximum respectively, inclusively;
they do not use the first or last row in source order. Date aggregation keeps
exact seconds and nanoseconds, including adjacent-nanosecond extrema. Reversed times and
noncontiguous cases are captured. All modes preserve source row order.

Attribute filtering selects complete cases containing an event with the
requested attribute value inside the inclusive timeframe. Numeric/Boolean
equivalence, literal bytes, vector comparisons and missing-value cases retain
their observed behavior. Missing case IDs are excluded by grouped modes;
attribute mode preserves the separately observed membership behavior for
matching NaN case IDs.

`positive` affects starting and completing modes; the other captured modes
ignore it. `keep_nan_values` admits events with missing timestamps in event
mode and complete cases without any nonmissing timestamp in case modes.
Partial-missing cases still use their nonmissing minimum/maximum. Attribute
mode also admits wholly untimed cases when this option is true. Ordinary
truthiness is retained.

## Typed controls, tables and errors

The native options carry typed EventAttribute timestamp/case selectors,
positive and keep_nan_values controls. Selectors remain literal, including
empty strings, surrounding spaces, Unicode, newline and NUL. Ordinary mapping
adapters and a parameter-vector adapter preserve enum-key identity: captured
enum keys override equivalent string keys in both insertion orders.
Unsupported selector shapes retain their captured indexing/grouping errors,
including the observed empty-list timestamp masking behavior for event mode.

Full outputs preserve columns and order, every typed cell, dataframe attributes,
index kind/dtype/names/labels, declared dtypes and complete categorical
metadata including unused categories. Coverage includes category, nullable
Boolean/String, float signed zero/NaN, bytes and date precision. Exact
TokenInteger arithmetic determines whether a selected RangeIndex remains
arithmetic, including labels close to both signed-int64 limits. Unsigned
index labels above signed-int64 maximum retain their exact identities.

The public `apply` and `apply_auto_filter` functions always raise their
captured unavailable-method Exceptions, including malformed controls. The
six active functions retain error type and full message separately, including
embedded NUL. Typed datetime declarations, object mixed timestamps, naive
versus aware comparisons, empty schemas, missing columns and parser-before-
column error precedence have dedicated retained outcomes.

## Evidence and acceptance boundary

Root executed each frozen observer once. The first bank contains 484 distinct
calls: 24 returns and 460 errors. Its ISO-string diagnostics remain intact.
The fresh canonical-space continuation contains 377 distinct calls: 278
returns and 99 errors, after auditing all 484 prior receipts. Across both
banks there are 861 distinct complete typed call receipts, 302 full-table
returns and 559 exact diagnostics. All per-call warning lists are retained;
zero warnings and zero source/argument/control mutations were observed.
No reference implementation source, AST, bytecode or private state was read.

The offline generator creates 117 unique complete input/result snapshots and
861 individual noinline native cases. Expected snapshots and diagnostics come
only from the retained observations. The named native target passes all 302 full tables and 559 exact diagnostics
with assertions enabled in `ctest-timestamp-second.log`. The coordinated native
build and all 29 suites pass. The shared production HTTP checkpoint passes
50 requests, including 17 requests with the explicit pandas engine. The final
server SHA-256 is
`a90c0d6f519386992266bdabc8fddd70fdf27b85cad723ec4d70c49e95253b4f`.

The initial direct test and first full CTest failure are preserved with the
initial complete source/test/map snapshots and compiled binary/object hashes.
The initial timestamp aggregation called an established scalar-key comparator
that compares dates in microseconds, collapsing adjacent nanoseconds. The
repair compares date extrema with exact seconds/nanoseconds in this companion;
the retained test source and all public expectations are unchanged. First
CTest passed 27 of 29 suites, and the authoritative repaired CTest passed all
29. The first HTTP harness failed while sending a table-shaped typed value to
the separate raw-log route; its original script and outputs are retained. The
second HTTP run corrected that harness input and passed all 50 requests.
The shared final frontend run passes 185 files and 9,737 tests; TypeScript and
Vite checks also pass. The earlier focused failure and first full-suite timeout
are retained alongside the final passing logs. These shared checks do not add
new public reference algorithm observations.

This finite checkpoint does not establish arbitrary pandas extension/object
classes, duplicate/non-string dataframe column labels, unobserved timezone
replacement behavior, every parser spelling, unsupported MultiIndex variants,
or every warning-producing input. Those remain explicit canonical-module gaps.
