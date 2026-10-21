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
Null. Declared String/Categorical/Enum case columns use their stored literal value
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
Categorical, Enum, Date and naive microsecond Datetime; case and timestamp controls
also include UTC microsecond Datetime. Timestamp controls additionally cover
lexical strings, exact signed/unsigned integers, floating NaN/infinity, Boolean,
Categorical and all-null values. Metadata columns include UInt32 and arbitrary
stored payload values. Native tables require unique string labels and declared
dtypes, and reject dataframe index metadata.

The fresh categorical continuation observes complete public label dictionaries,
including unused labels and dictionary order, through `cat.get_categories()`;
Enum declarations are also checked through their public `categories` property.
The native `categorical_dtypes` descriptor carries these String labels and their
order. Named and shared Categorical contexts retain their visible dictionaries
through filtering, while Categorical case groups sort lexically. Enum case
groups sort by declared dictionary order. Rewriting the selected case column to
String removes its categorical descriptor, including for an empty result.
Other columns retain their complete dictionaries, including unused categories.

Selected native Enum case/timestamp sort columns require a dictionary descriptor;
the adapter rejects an absent descriptor or a nonnull label outside it. This
validation precedes sorting, so empty inputs cannot hide a missing descriptor
and a single-row input cannot hide an unknown label. Each selected dictionary
is indexed once per materialization; sort comparisons reuse its label ranks.
These adapter guards are native storage controls, rather than additional public
reference observations.

The native descriptor does not carry Categorical namespace/context identity,
physical dtype or physical category codes. Public names, namespaces and physical
dtype metadata are retained in the raw evidence and explicitly excluded from
the native projection. Physical codes were not observed and no equivalence is
claimed. Metadata-observer deprecation warnings remain separate from warnings
emitted by the algorithm itself. Physical lazy execution plans and arbitrary
optimizer/streaming
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

## Complete label-dictionary continuation

`build/filtering-between-polars-categorical-public-actual-1` contains ten fresh
timestamp-complete successful calls. The observer reconstructs and verifies all
333 previous actual-call receipts, checks each new complete source/endpoints/
parameters/invocation receipt against them before entry, and additionally records
an enriched receipt containing the public dictionary observables. Both new
receipt sets are distinct, with no prior overlaps or mutations. The opaque
implementation hash remains unchanged; no calls were replayed and no reference
implementation was inspected.

The ten cases cover global, named, independent and shared Categorical contexts;
Categorical case/activity columns; Enum case/activity/context columns; unused
labels; empty Enum frames; and literal empty, Unicode and NUL-bearing labels.
The generator compares complete representable source/result projections: every
row and declared dtype plus the full ordered String label dictionary of each
categorical column. It preserves all 333 previous fixtures without rewriting
their expected outcomes. The combined retained scope is **343 outcomes: 163
projected returned frames and 180 complete diagnostics**. Ten returned-frame
projections additionally compare all observed category-label dictionaries.

The rebuilt assertion-enabled native target passes all **343 retained outcomes**,
including the ten complete label-dictionary projections, plus the native missing
dictionary, unknown-label, Enum order and lazy-source controls. The separate
shared-carrier suite passes dictionary validation, lazy storage and complete
JSON/workbench transport checks. All **14 coordinated native/workflow suites**
pass, including existing EventData, table and repetition regressions affected by
the shared validation extension. The freshly rebuilt production server passes
**37 actual handler checks**, including dictionary transport. The first HTTP
attempt stopped on a harness comparison of minimal upload fields with canonical
export defaults; its script, records and failure explanation are retained. The
corrected harness compares the result against the parsed canonical source and
requires no production source change.

The historical `docs/polars-between-checks.jsonl.gz` remains the unchanged proof
for the earlier 333 outcomes. `docs/polars-between-categorical-checks.jsonl.gz` binds the
fresh public receipts, explicit projection boundary, generated fixtures and
current native sources to the coordinated acceptance logs, compiled object and
binary identities, and actual production receipts. The ten calls emit no
algorithm warnings; the 38 dictionary snapshots separately retain 38 public
metadata-observer warning observations. Context identity, physical category
representation and warning emission remain explicit unimplemented boundaries.

`docs/polars-between-checks.jsonl.gz` retains all three public banks, complete
warnings, recipes, generated fixtures, final source snapshots and native
build/test bindings. Historical first, second and third coordinated build/test
logs and the pre-quoted-literal archive are preserved alongside the successful
fourth run and runtime-byte audit. Every archived member has
its original byte length and SHA-256; compiled helper/test objects, libraries,
map and executable identities are bound separately. No new reference calls
were made while implementing or checking the native companion.
