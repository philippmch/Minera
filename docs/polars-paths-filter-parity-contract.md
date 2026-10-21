# Polars paths: bounded native companion

`filter_polars_paths` and `filter_polars_path_performance` operate directly on
`EventTable` or `LazyEventTable`. They follow retained public observations of
`algo.filtering.polars.paths.paths_filter` in PM4Py 2.7.22 with Polars 1.44.1.
The canonical module remains **partial**, with explicit dtype, diagnostic and
runtime-identity boundaries below.

## Observed filtering behavior

Both operations sort by the selected case column and timestamp before detecting
adjacent paths within each case. Plain path filtering therefore requires the
timestamp column too. Noncontiguous source cases are grouped by this sorting;
source row adjacency alone does not determine a path. The result retains every
row of each selected case, in the sorted case/timestamp order, with complete
original columns, cells, dtypes and category-label dictionaries. Float NaN case
values group together; signed zero values share a group while their original
cell bits remain intact. Native sorting retains source order at equal keys.

The defaults are case `case:concept:name`, source attribute `concept:name`,
timestamp `time:timestamp` and positive filtering. Plain apply may select a
different shifted target attribute; omission falls back to the selected source
attribute. Empty and control-character role names are literal names. Performance
filtering uses the source attribute at both ends and ignores
`target_attribute_key`, including invalid supplied values. The captured
`decreasingFactor` and unrelated business-hours parameter are inert.

Positive filtering retains cases containing a qualifying path; false polarity
retains cases without one. The captured Boolean, None, number, string and list
polarity controls follow their Python truth value. A null case group does not
qualify for positive filtering, and its sorted rows survive negative filtering.
The retained empty path collection selects no cases positively.

Public path labels concatenate the source text, a comma and the next target
text. This encoding is retained, including its ambiguity: source pair
`("A,B", "C")` matches requested pair `("A", "B,C")` because both become
`A,B,C`. Numeric and Boolean activity columns are cast to their observed String
forms, including `1`, `true`, `false` and `1.0`. Requested numeric endpoint
objects themselves raise their observed immediate Python concatenation errors;
they are not silently converted to strings.

Apply accepts the observed list/tuple/set/dictionary iterable forms, indexing
the first two values of each contained pair. Performance indexes one supplied
pair. Extra pair members are ignored; missing members, unsupported iterables and
dictionary indexing retain their complete public exception classes/messages.
A string pair such as `"AB"` indexes as `A`, `B`. An outer dictionary is iterated
by its typed keys. `PolarsPathsFilterDictionary` carries tuple keys explicitly
because the shared string-key attribute container cannot represent them; this
does not change the shared native ABI or encode keys into table metadata.

## Performance, phases and native storage

Performance uses the following sorted timestamp minus the current timestamp
for a matching path. Duration is truncated to integral seconds before applying
inclusive bounds. The fresh fractional controls distinguish this from direct
fractional-second comparison. Public diagnostics expose exact defaults of
minimum **0** and maximum **9223372036854775807** seconds. A fresh year-0001 to
year-9999 frame also passes under those defaults. Negative source-time pairs
reorder before adjacency detection; their original order is not used to invent
a negative elapsed path.

The native parameter-binding carrier preserves public enum-key precedence over
conflicting string keys. `PolarsPathsFilterError` exposes `type`, `phase` and a
length-carrying `message`, preserving complete diagnostic text including literal
NUL. Public construction errors occur at apply. Lazy schema and dtype failures
remain collection errors; captured eager temporal failures occur during apply.
The lazy result performs no source scan during construction and is restartable.

Complete native attributes, metadata, field provenance and category-label
dictionaries survive independently of filtering. Independent native controls
check sorted whole rows, signed-zero payloads, source preservation, result
independence, restartable lazy scans, target semantics, fractional bounds and
delimiter collisions. Selected Enum sort columns require their full dictionary;
the adapter validates every nonnull cell against a cached label-rank map before
sorting, including single-row inputs that would not invoke a sort comparison.

## Evidence and remaining boundaries

The zero-call public observer pins reference commit
`0715c31f3f1d85924107d6aa5329806db69df899`, with opaque module SHA-256
`47e4e7b2278855ca1baaa864548d554c76d7ea82f13d04523288b3386f2229eb`.
Global searches of retained receipts found only the public surface for this
exact module before preparing the new bank; catalogue/coverage references were
not algorithm-call receipts.

`build/filtering-paths-polars-public-actual-1` contains 226 distinct calls: 144
returned frames and 82 errors (55 apply, 27 collect). The disjoint continuation,
`build/filtering-paths-polars-strings-public-actual-1`, adds twelve fresh calls:
nine returned frames and three errors. Its complete pre-entry deduplication
reconstructs all 226 prior actual receipts. It also captures the returned
LazyFrame's public unoptimized plan, exposing sort, shift, comma concatenation
and join behavior. Neither bank replays calls or inspects reference source,
bytecode, AST or private state.

All actual-call receipts include the specific callable, positional path shape
and typed contents, full supplied kwargs/parameter bindings, eager/lazy input
kind, complete rows/schema and public categorical dictionary observables. Entire
banks are deduplicated before any entry, with source/path/kwargs rechecked
immediately before each call. There are no duplicates, prior overlaps or input,
path or keyword mutations, and the opaque implementation hash is unchanged.

The offline generator retains **238 outcomes: 153 exact complete projected
returned frames and 85 complete type/phase/message diagnostics**. Individual
noninline frame/case functions bound MSVC stack allocation. Public dictionary
observations include all ordered labels, unused Enum labels, declared Enum
categories and empty-frame dictionaries. All 50 input/result dictionary
snapshots retain their separate metadata-observer deprecation warnings. One
algorithm warning concerns None comparisons; the native frame/error API does
not emit Python warnings.

Observed case/activity columns include String, signed/unsigned integers,
Float64, Boolean, Null, named Categorical and Enum. Timing covers Date and naive
or UTC microsecond Datetime, plus unsupported String, numeric, Boolean and Null
duration diagnostics. Native tables require declared dtypes and no dataframe
index. The finite observations do not establish all dtype/value/selector
cross-products, nanosecond dates, other timezone identities, nested extension
objects, internal helper-column collisions, arbitrary diagnostic suggestion
heuristics, optimizer/streaming behavior or physical execution plans.
Categorical context namespace/identity and physical category representation are
recorded as public metadata in the evidence but remain outside the native
dictionary carrier. No physical-code or Python warning-emission equivalence is
claimed. The generated native projection contains complete representable
rows/schema/label dictionaries, without silently encoding omitted identity.

The assertion-enabled `minera_polars_paths_filter_tests` target passes all **238
retained outcomes**, including all 153 exact projected frames and 85 full
diagnostics, plus its independent native controls. All **24 coordinated native
and workflow suites** pass. The freshly rebuilt production server passes **44
actual HTTP handler checks** and the owned server exits afterward. The final
production reopen checks explicitly select the activity role retained in the
exported native/XES/context artifact.

`docs/polars-paths-filter-checks.jsonl.gz` retains both complete public banks,
public plans, warnings, full actual-call/dedup receipts, observer/preparer/
generator scripts, original expected frames and diagnostic bytes, current
native and workflow source snapshots, and compiled identities. It binds the
authoritative core/server build, final 24-suite CTest and third production HTTP run
to helper/test objects, libraries, map files and executables. A subsequent
driver-only EOF cleanup removes exactly one terminal blank LF byte. Its offline
byte audit proves an exact prefix match and identical stripped bytes, retaining
both the old driver and original successful archive. The final archive binds the
rebuilt test object/executable/map and repeated coordinated acceptance after
that cleanup. Historical build,
first failed CTest and two failed production harness attempts remain archived
with their original scripts. The initial Polars diagnostic mismatch exposed
eager direct predicates versus lazy cached predicates in temporal larger
expressions; the general phase-aware formatter now matches all six captured
temporal dtype families. No expected outcome was rewritten. Production harness
reopens now use their exported role explicitly, while the workbench also has
regression coverage for actual role detection and custom literal fallback.

Every archive member records its original byte length and SHA-256. No further
reference algorithm calls were made during native implementation or acceptance.
The whole module remains partial with the boundaries above.
