# Polars timestamp: bounded native companion

`filter_polars_timestamp_events`, `filter_polars_timestamp_contained`,
`filter_polars_timestamp_intersecting` and `filter_polars_timestamp_attribute`
operate directly on `EventTable` and `LazyEventTable`. They follow retained public
observations of `algo.filtering.polars.timestamp.timestamp_filter` in PM4Py
2.7.22 with Polars 1.44.1. The canonical module remains **partial**; the bounded
row/schema/dictionary projection does not establish the remaining gates below.

## Bounds and filtering behavior

The default timestamp role is `time:timestamp`; the default case role is
`case:concept:name`. Empty, Unicode, quote, newline and NUL-containing role names
are literal names. The native binding carrier preserves public enum-key
precedence over conflicting string parameter keys. Captured `positive` and
`keep_nan_values` parameters are inert, including false polarity.

String bounds use the strict public `%Y-%m-%d %H:%M:%S` parser. Canonical input
is `2039-06-01 08:00:00`. Unpadded numeric components are accepted. ISO `T`,
date-only and month-name strings fail; fractional, `Z` and offset suffixes on
otherwise valid space-format strings produce their complete unconverted-data
errors. Typed datetime bounds preserve microseconds. Their timezone is replaced
with UTC **without converting the wall clock**: typed `10:00+02:00` becomes
`10:00 UTC`, while `08:00+02:00` becomes `08:00 UTC`. Typed date-only bounds and
unsupported bound objects retain their observed immediate errors.

Normalized bounds are UTC. A naive source Datetime compared with those bounds
raises the retained temporal schema error; supplying a space string does not
make that naive source compatible. The usable Polars table example therefore
uses an explicit UTC Datetime dtype. Date columns compare as midnight calendar
instants. Captured numeric timestamp columns compare to physical epoch microseconds. A
fresh year-2301 continuation distinguishes adjacent integral microseconds above
2**53: signed and unsigned integer cells/bounds remain exact, while Float64
comparisons retain the observed floating rounding of both cells and bounds.
The public UInt64 plans explicitly cast the selected timestamp to Int64.
This establishes the captured values within the signed-microsecond range;
unsigned overflow and every nonfinite/fractional floating boundary remain open. Null timestamps do not satisfy event comparisons.

Event filtering keeps source rows whose timestamp lies in the inclusive
interval. It ignores the case role, including invalid supplied case selectors.
Containment computes each case's minimum and maximum nonnull timestamp and
keeps the whole case when both lie within the inclusive interval. The
attribute operation first finds events with an equal selected attribute and an
inclusive matching timestamp, then retains every row of their cases.

Intersection uses the exact public three-clause predicate:
`lower < first < upper OR lower < last < upper OR
(first < lower AND upper < last)`. Its boundaries are strict. A case spanning
exactly `[lower, upper]` is excluded; a case strictly covering the interval is
included. With equal bounds, only the strict covering clause can select a
case. Reversed intervals retain the observed expression semantics rather than
being replaced with a guessed empty result. Event and attribute predicates with
reversed bounds simplify to empty before incompatible temporal schemas are
compared.

All returned rows retain source order, complete columns, cells, dtypes and
category-label dictionaries. Noncontiguous rows of one case are selected
together. Null case keys do not join and are dropped by whole-case operations;
event filtering can retain them. Float NaN case keys group together and signed
zero keys share a group, while original floating cell bits are preserved.
Selected numeric case expressions follow their observed literal projection,
including the appended Int32 `literal` column; empty-list selectors and
unsupported timestamp/attribute selectors preserve their distinct diagnostics.

The public `apply` and `apply_auto_filter` functions exist but immediately raise
`Exception` with their respective unavailable-method messages, independently
of their supplied parameters. Their native companions preserve those outcomes.
The Polars module exposes no trace-starting or trace-completing functions.

## Native phases and storage

`PolarsTimestampFilterError` exposes a class name, `phase` and length-carrying
`message`. Immediate parser/selector/parameter errors remain apply errors.
Lazy schema/dtype errors remain collect errors; captured eager failures remain
apply errors. Diagnostic comparison uses the complete message bytes, including
resolved plans, expression trees, context stacks and literal NUL, rather than
only a C-string prefix or error class. Lazy construction performs no source scan
and the result remains restartable.

Native table attributes, metadata and field provenance survive independently
of filtering. Native controls exercise whole-case versus event selection,
strict intersection boundaries, complete context and signed-zero payloads,
source preservation, lazy result independence, repeat scans and literal
user-controlled diagnostic text. Native tables require declared dtypes and no
dataframe index. String-labelled Categorical/Enum dictionaries preserve their
full observed ordered label vectors, including unused labels and empty results.

## Public evidence and acceptance boundary

The zero-call surface observer pins reference commit
`0715c31f3f1d85924107d6aa5329806db69df899` and opaque module SHA-256
`533f86a96004eee1e7bfb53cd54938a1bea448f09169174f80d4d71982068b1b`.
An initial global exact-module receipt search found catalogue/coverage/status
references but no earlier algorithm-call bank. No reference implementation,
private state, AST or bytecode was inspected.

`build/filtering-timestamp-polars-public-actual-1` retains 360 distinct calls:
2 returned frames, 324 apply errors and 34 collect errors. Most first-bank
failures expose the strict public parser on ISO `T` strings; valid typed bounds
also expose naive-source versus UTC comparison errors. Those complete outcomes
remain historical evidence and are not rewritten to intended success.

The disjoint continuation
`build/filtering-timestamp-polars-utc-public-actual-1` retains another 360
calls: 215 returned frames, 103 apply errors and 42 collect errors. New input
markers, explicitly UTC ordinary timestamps and canonical space bounds make
successful public filtering observable. Deliberately invalid strings and dtype
controls remain invalid. Its observer reconstructs and verifies all 360 prior
actual receipts before entering any new call. It separately records public
unoptimized plans and plan-observer errors/warnings, then collects each result
once. A public explain failure is not misreported as an algorithm collect.

The small numeric timestamp values in the first two banks could not distinguish
physical numeric coercions. The fresh twelve-call continuation
`build/filtering-timestamp-polars-numeric-public-actual-1` therefore uses all four
filters with signed/unsigned integer and Float64 physical timestamps around
`10448193906000000` microseconds. It verifies all 720 prior receipts before any
entry and returns twelve complete frames with no mutations. Exact adjacent
microseconds at this magnitude expose the original native floating conversion
loss; the general helper now keeps integral seconds/fractions exact and compares
Float64 cells and bounds in their observed floating domain. The complete old
720-call helper/header/driver/vectors/generator/projection snapshot remains
preserved separately before that repair. No original outcome was rewritten.

All three complete banks are deduplicated before any entry against typed full input
rows/schema/category observations, actual callable, complete supplied keyword
and enum/string parameter bindings, eager/lazy input kind and invocation shape.
Immediate pre-entry checks and post-call snapshots show no source/category or
keyword mutation, duplicate calls, prior overlaps or opaque implementation hash
changes. Only the root coordinator executed the frozen observers, once each.

The offline fixture generator retains **732 outcomes: 229 exact complete
projected frames and 503 complete diagnostic types/phases/messages** (427 apply,
76 collect), across all six public functions. Individual noninline frame/case
functions avoid a large combined MSVC stack frame. All 48 input/result public
category snapshots preserve their independent namespace/physical metadata and
metadata-observer warnings in the receipts. Two algorithm warnings concern
None comparisons; the native frame/error API does not emit Python warnings.

The assertion-enabled `minera_polars_timestamp_filter_tests` target passes all
**732 retained outcomes**, including all 229 complete projected frames and 503
full diagnostics, plus independent native controls. All **29 coordinated native
and workflow suites** pass in `ctest-timestamp-second.log`. The freshly rebuilt
production server passes **50 actual HTTP handler checks**, with owned server
exit confirmed, in `timestamp-production-http-2`. Its SHA-256 is
`a90c0d6f519386992266bdabc8fddd70fdf27b85cad723ec4d70c49e95253b4f`.
The full frontend run passes **185 files and 9,737 tests** with two workers
(`timestamp-frontend-full-final2.log`); TypeScript and Vite production builds
pass. The earlier default-pool run, concurrent with native compilation, retains
9,736 passes and one timeout in its historical first log. The successful rerun
changes worker concurrency, with no native helper/header/driver/vector changes.
No rendered browser acceptance is claimed by this checkpoint.

The existing table workbench exposes the four applicable Polars modes, literal
case/timestamp/attribute selectors, canonical space-format bounds and an exact
typed JSON attribute value. Its downloadable Polars example declares UTC
timestamps. The multipart `/api/reference-table-timestamp` route returns source
and filtered native table JSON, counts, apply/collect phase and complete errors;
the workbench downloads complete parsed source, filtered result and raw response.
It hides inactive case controls for event mode and rejects Polars starting,
completing, polarity and keep-missing controls rather than pretending they alter
this module. Stale-request and cancellation tests are workflow checks, separate
from the retained public module outcomes.

`docs/polars-timestamp-filter-checks.jsonl.gz` binds all three frozen banks,
complete actual-call/dedup receipts, public plans and warnings, observer/preparer/
generator scripts, native and workflow source snapshots and compiled identities.
Every member retains original bytes, length and SHA-256. Helper/test objects,
libraries, test/server executables and linker maps are bound to the authoritative
second build/CTest and second production HTTP receipt. The unchanged Polars
first-run success is retained alongside the initial coordinated 27/29 result,
which exposed unrelated log/Pandas helper mismatches. The first production
attempt's original script and failed 44th request are preserved: that harness
supplied native table-style attribute JSON inside a raw native log. The second
harness uses the raw-log schema; no product change is attributed to that harness
repair. The old 720-call native draft and generator remain preserved before the
numeric-boundary repair. A hash manifest preserves the pre-final-frontend
archive identity; final frontend success and the original timeout remain
separate archived receipts.

The observations cover ordinary String, signed/unsigned integer, Float64,
Boolean, Null, named Categorical and Enum context/case/attribute columns;
Date and naive/UTC microsecond Datetime timestamp columns; typed/string and
malformed bounds; literal/missing/empty roles; eager/lazy phases; empty and
unsorted frames. They do not establish every dtype/value/selector cross-product,
all temporal units/timezone identities, arbitrary regex or multi-column
selector expressions, nested extension values, internal-name collisions,
arbitrary diagnostic suggestion heuristics, optimizer/streaming equivalence or
physical execution plans. Public categorical namespaces, physical codes and
identity are recorded but lie outside the native dictionary carrier. Native
warnings, Python object identity and physical category identity are not claimed.
No extra reference calls were made during native implementation. The whole
module remains partial.

## Original-call API and workflow

All 732 retained calls are also reachable through the original-call mode of
`/api/reference-table-timestamp` and the rendered table timestamp workspace;
see `polars-timestamp-original-call-workflow.md`. The transport fixture is
generated only after ROOT re-binds every admitted original and rebuilds the
unchanged native vector oracle. All 229 frames and 503 diagnostics (427 apply,
76 collect) agree natively, through the direct workbench and over owned HTTP.
This adds no reference observations and does not change the partial status.
