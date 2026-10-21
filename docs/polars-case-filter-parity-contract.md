# Polars case filters: bounded native companion

`polars_case_filter.hpp` provides size, count and elapsed-performance filters
over `EventTable` and `LazyEventTable`, following public observations of
`algo.filtering.polars.cases.case_filter` in PM4Py 2.7.22 with Polars 1.44.1.
The canonical module remains **partial**. Native frames retain complete typed
rows, declared schema and representable categorical label dictionaries; the
companion operates on the table directly.

## Observed operations

`filter_polars_case_size` retains whole case groups whose event counts meet
inclusive minimum and optional maximum bounds. `filter_polars_ncases` ranks
groups by descending event count and retains the requested number of groups.
Zero count limits produce an empty frame, None means no count limit, and
negative limits raise the observed immediate error. Boolean count limits use
their integer value. Other captured invalid limit/bound types preserve their
public diagnostic.

`filter_polars_case_performance` uses each selected case group's maximum minus
minimum nonnull timestamp, rather than source first/last event order. It
truncates elapsed duration to integral seconds before applying inclusive bounds.
An all-null timestamp group fails the performance predicate. The default case
and timestamp columns are `case:concept:name` and `time:timestamp`; literal
custom, empty, Unicode and NUL-bearing selectors remain literal names. Size
and count operations do not require a timestamp column. The retained numeric
case-selector expression creates a constant group and adds its `literal`
Int32 column to the returned projection.

Source rows remain in their original order in the observed returned frames.
Null selected-case groups participate in aggregation but disappear during the
inner join; their complete source rows remain unchanged on the source. Numeric,
Boolean, Date, microsecond Datetime, String, Categorical, Enum and Null case
columns retain their dtypes. Float64 NaN forms a case group, and positive and
negative zero belong to the same group while their stored payload bits survive.
Other columns retain complete cells, dtypes and full declared category-label
dictionaries, including unused labels and empty-frame dictionaries.

Truthy `business_hours` raises `NotImplementedError` at apply with the retained
public message. Slots do not make this unsupported public branch succeed.
False/None/empty-list flags follow the elapsed branch. The public parameter
wrapper resolves case/timestamp selectors and business flags; enum keys override
conflicting string keys regardless of insertion order. Captured workcalendar
metadata is inert. Native `PolarsCaseFilterParameter` represents these public
key/value/enum bindings explicitly, separately from table metadata.

The public `apply` and `apply_auto_filter` entry points reject immediately with
their different retained exception classes/messages. The native aliases preserve
those errors before scanning the source. They do not silently choose another
filter operation.

## Count ties and exactness

The disjoint continuation captures the returned LazyFrame's public unoptimized
plan. It exposes an aggregation with `maintain_order: false`, descending sort
on `count`, a slice and an inner join. Fresh unique-count cases establish
frequency rank independently of source encounter order. Equal-count groups at
the slice boundary have an unspecified public order. The observed String and
Int64 controls choose different tied groups, and a null group can consume one
of the requested slots before disappearing in the join.

The native implementation chooses encounter order deterministically among
equal-count groups. It implements the admitted count-ranking semantics without
claiming to reconstruct the public runtime's hash/group tie choice. The test
generator retains every exact captured frame and labels each ambiguous cutoff
separately. Those twelve cases compare complete frames against the finite
admitted selections at the tied cutoff, with exact group counts, complete
whole-case rows, schema/dictionaries and null-join behavior. The other 134
returned frames compare exactly. No input-specific or dtype-specific tie rules
are fitted to the captured outputs.

## Phases, native storage and boundaries

`PolarsCaseFilterError` carries `type`, `phase` and a length-carrying `message`;
the latter preserves complete diagnostic bytes even when names contain NUL.
Selector/construction failures occur at apply. Lazy schema and dtype failures
occur at collection; the corresponding eager observations fail during apply.
The lazy producer is restartable and does not scan during construction.
Independent native controls verify complete attributes, metadata, provenance,
signed-zero bits, source preservation and result independence.

The observed timestamp domain includes Date and naive/UTC microsecond Datetime;
String, numeric, Boolean and Null timestamp controls retain their unsupported
subtraction/duration diagnostics. Tables require declared dtypes and no dataframe
index metadata. General selector expressions, arbitrary dtype combinations,
Duration/Time, nanosecond timestamps, additional timezone identities, arbitrary
optimizer/streaming plans and complete diagnostic suggestion heuristics remain
outside this checkpoint.

Public categorical dictionaries are observed through public APIs and projected
as their complete ordered String labels in `categorical_dtypes`. Runtime context
namespace/identity and physical category representation remain in the raw
receipt but are outside the native carrier; no physical-code equivalence is
claimed. The public calls emit 96 warnings: 89 `count` rename deprecations,
four `GroupBy.count` rename deprecations and three None-comparison warnings.
Their full records are retained. Metadata-observer deprecation warnings are
stored separately with dictionary snapshots. The native frame/error API does
not emit Python warnings, so warning emission remains a parity boundary.

## Retained evidence and acceptance

The zero-call public surface observer pins reference commit
`0715c31f3f1d85924107d6aa5329806db69df899` and opaque module SHA-256
`62cd10220a7ccf9418297882ad9a70323a5dbd27f8f7935156660f036701761a`.
The first bank, `build/filtering-cases-polars-public-actual-1`, contains 213
distinct actual calls: 134 returned frames and 79 errors (52 apply, 27 collect).
The disjoint `build/filtering-cases-polars-ncases-order-public-actual-1`
continuation contains twelve successful fresh calls and their public plans.
It reconstructs all 213 prior full receipts before any new algorithm entry.
Both banks snapshot complete actual inputs, kwargs, invocation kind, rows,
schema and public dictionary observables before entry. They record no duplicate,
overlapping or mutated inputs/kwargs, and the opaque implementation hash remains
unchanged. No reference implementation was inspected and no calls were replayed.

The offline generated native projection covers **225 retained calls: 134 exact
projected returned frames, 79 complete type/phase/message diagnostics and 12
explicitly admitted count-tie projections**. Individual noninline frame/case
functions keep test stack usage bounded on MSVC. The native source contains no
captured-fixture lookup table.

The assertion-enabled `minera_polars_case_filter_tests` target passes all these
checks, including the twelve explicitly admitted tie cases. All **19 coordinated
native/workflow suites** pass. The freshly rebuilt production server passes
**42 actual HTTP handler checks** and the owned server exits afterward.
The first five-suite case run also passed; the broader first build then stopped
on unrelated test code accessing a nonexistent `EventLog.properties` field.
The corrected test uses its metadata field. That failed build and the historical
first CTest receipt remain in the archive alongside the successful final logs.

`docs/polars-case-filter-checks.jsonl.gz` retains both full public banks, complete
warnings and public count plans, invocation/dedup receipts, observer/preparer/
generator scripts, all original expected projections, native source/header/test
snapshots and generated cases. It binds the successful configure/build/CTest
and actual production receipts to source, object, library, map and executable
identities. Each archived member records its original length and SHA-256. No
additional reference algorithm calls were made while implementing or validating
the native companion. The module stays partial with the boundaries above.

Final diff checking removed one redundant blank line at the end of the test
driver. The original driver/archive remain retained, and an offline byte audit
confirms that only terminal whitespace changed. The target was rebuilt in
`build-cases-eof-final.log`; all 19 suites passed again in
`ctest-cases-final2.log`. The final archive binds this rebuilt test executable.
