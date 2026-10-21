# Polars between companion: bounded parity checkpoint

`filter_polars_between` operates directly on `EventTable` or `LazyEventTable`.
It follows retained public observations of
`algo.filtering.polars.between.between_filter` in PM4Py 2.7.22 with Polars
1.44.1. The canonical module remains **partial**. The companion covers the
observed visible typed frame domain, rather than converting frames into logs.

## Observed frame behavior

The operation requires the canonical `time:timestamp` column, although its
public parameter enum only lists case, activity and subcase separator options.
It sorts by the selected case column and canonical timestamp, then extracts
inclusive intervals from the latest matching start to the first matching end.
A newer start replaces an open start. Repeated ends after a completed interval
do not create another interval. A value matching both endpoints yields a
singleton interval. Incomplete intervals are omitted.

Case groups and timestamps sort ascending. Null timestamps sort first and
Float64 NaN sorts after ordinary numbers; the retained ties preserve source
order. Null case groups do not appear in the returned frame. Each retained
case identifier becomes a String containing its source value, the separator
and a dense one-based subcase number. Numeric, Boolean, Date and Datetime case
values use the observed string casts. A null separator produces null case
cells while retaining the selected rows. Literal empty separators remain
distinct from omitted options.
Subcase numbers rank the textual per-case `start_end` identifiers lexically.
They can therefore differ from chronological ordinals: an interval at positions
`10_11` receives a lower number than one at `2_3`. Output rows still follow the
sorted case/timestamp order. The native companion retains this observed behavior.

Endpoint values can be ordinary scalars or observed list/tuple/set collections.
Dictionary endpoints use their keys. Null endpoint values do not match;
homogeneous typed numeric/date endpoints match their corresponding columns.
Incompatible membership dtypes raise the retained Polars diagnostic, and the
captured mixed string/integer list fails during construction. Empty, Unicode,
newline and NUL activity values are literal values rather than substrings.

The default selectors are `case:concept:name` and `concept:name`; the default
separator is `##@@`. Empty and control-character selector names remain literal.
The captured one-element activity selector list/tuple selects that column;
the corresponding case selector raises its observed immediate error. Enum
parameter keys override conflicting string keys in both retained controls.
The native options carry the resolved values; callers resolve public parameter
bindings before invoking the companion. The captured timestamp-selector-looking
extra parameter is inert: sorting continues to use `time:timestamp`.

## Native carriers, laziness and limits

Every returned projection retains source column order, all selected rows'
other cells and their declared dtypes. The selected case column becomes String
even when the returned frame is empty. The eager overload returns an independent
table. The lazy overload does not scan during construction and provides a
restartable producer; materialization sorts and extracts the intervals.
Native attributes, metadata and field provenance survive independently.
Rewritten native case cells become ordinary String/null scalars: old container,
children and wrapper payloads are cleared. Field roles and literal source-key
provenance remain, with an active role's presence state updated to Present or
Null. Declared String/Categorical case columns use their stored literal value
as the scalar projection, including when native source descriptors accompany it.

`PolarsBetweenError` exposes `type`, `phase` and a length-carrying `message`.
Immediate selector/construction failures remain apply errors. Lazy schema and
membership failures remain collect errors; eager missing-schema errors occur
at apply. Complete error text is compared, including plans with literal NUL
labels. Diagnostic text is generated from schema, role and endpoint values;
the long separator-list diagnostic uses a parameterized public plan template.
Its source uses adjacent quoted lines to retain significant trailing spaces
without physical source-line whitespace. An offline before/after audit proves
that this representation change preserves all 4,838 runtime bytes exactly;
the final native suite also compares the complete resulting diagnostics.

Observed selected columns cover String, Int64, UInt64, Float64, Boolean, Null,
Categorical, Date and naive microsecond Datetime; case and timestamp controls
also include UTC microsecond Datetime. Timestamp controls additionally cover
lexical strings, exact signed/unsigned integers, floating NaN/infinity, Boolean,
Categorical and all-null values. Metadata columns include UInt32 and arbitrary
stored payload values. Native tables require unique string labels and declared
dtypes, and reject dataframe index metadata.

Categorical dictionaries, unused categories and physical identity were not
observed. Physical lazy execution plans and arbitrary optimizer/streaming
behavior are not reproduced. Nested/Object/Decimal columns, Duration/Time,
nanosecond datetimes, other timezone identities, general column-selector
expressions and complete diagnostic suggestion heuristics remain outside this
checkpoint. The finite role/dtype combinations do not establish the entire
cross-product of Polars values or casting behavior.
The second bank also records 70 Python `DeprecationWarning` emissions across
16 calls concerning collection membership ambiguity. Their full text is
retained in the evidence. The native frame/error API has no Python warning
carrier, so warning emission is another explicit parity boundary.

## Retained observations and acceptance

The public three-family surface observer made zero algorithm calls and pinned
the module to reference commit
`0715c31f3f1d85924107d6aa5329806db69df899`. Its opaque SHA-256 is
`2a07abc06a1559bad97398a80cbe416dd27a48a1d8f4252c7c42affeaa11fa38`.

The first bank at `build/filtering-between-polars-public-actual-1` contains
148 distinct calls, one full returned frame and 147 errors. Public diagnostics
expose the missing canonical timestamp prerequisite: 132 errors identify
`time:timestamp`, four identify the case column, and eleven are immediate
selector/endpoint-construction errors. These are retained negative observations,
not evidence of a runtime dependency failure. The successful fixture supplies
the timestamp column and demonstrates actual sorting and interval extraction.

The disjoint continuation at
`build/filtering-between-polars-timestamp-public-actual-1` constructs 179 new
timestamp-complete calls with fresh source markers. It returns 146 complete
frames and 33 errors. Full call receipts are checked against the first bank
before any algorithm entry. A third disjoint bank at
`build/filtering-between-polars-long-segments-public-actual-1` contains six
fresh successful calls covering two-digit per-case indexes, up to twelve
intervals, repeated starts, long intervals and interleaved long cases. It
establishes lexical ranking of segment identifiers, which the shorter examples
could not distinguish from chronological numbering.

The combined banks contain **333 distinct calls, 153 full returned frames and
180 errors**, with no duplicates, overlaps,
source/endpoint/parameter mutations, implementation inspection or replay.
Opaque module hashes stay unchanged. All observers snapshot every public
column, declared dtype, shape and typed row, and preserve apply/collect phases.

The offline generator embeds all 333 native outcomes in individual frame and
case functions to keep MSVC test stack allocation bounded. Tests compare every
returned cell/dtype and the complete diagnostic bytes. Independent native
controls cover sorting, latest-start intervals, signed-zero payloads, field
provenance, source preservation, restartable lazy rows and result independence.
Descriptor controls also verify canonical rewritten scalar cells and null
presence states without mutating the source descriptors.
A separate native formatting control keeps template-like user role, endpoint
and separator text literal through single-pass diagnostic interpolation.

The rebuilt assertion-enabled target `minera_polars_between_filter_tests`
passes all 333 retained outcomes: 153 complete returned frames and 180 full
type/phase/message diagnostics. Its independent native storage, provenance,
lazy collection and literal-diagnostic controls also pass. This includes the
general lexical-numbering correction established by the final six observations.
The final coordinated run passes all seven related native/workflow suites.

`docs/polars-between-checks.jsonl.gz` retains all three public banks, complete
warnings, recipes, generated fixtures, final source snapshots and native
build/test bindings. Historical first, second and third coordinated build/test
logs and the pre-quoted-literal archive are preserved alongside the successful
fourth run and runtime-byte audit. Every archived member has
its original byte length and SHA-256; compiled helper/test objects, libraries,
map and executable identities are bound separately. No new reference calls
were made while implementing or checking the native companion.
