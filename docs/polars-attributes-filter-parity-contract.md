# Polars attributes: bounded native companion

The native companion covers all nine public functions in
`pm4py.algo.filtering.polars.attributes.attributes_filter` from PM4Py 2.7.22
with Polars 1.44.1. Its `EventTable` and `LazyEventTable` APIs preserve literal
selectors, typed controls and separate apply/collect diagnostics. The canonical
module remains **partial**. The 726-outcome bounded native bank passed the
coordinated 41-suite CTest run. The final production receipt passed 95 HTTP
checks, and the final frontend run passed 9,809 tests across 190 files.

## Selection and controls

`filter_polars_attributes` and `filter_polars_attributes_on_values` select whole
cases containing a matching event. Their event counterpart selects individual
rows. Numeric counterparts use inclusive bounds; the negative event predicate
keeps values outside the interval. Null attribute values do not satisfy ordinary
membership or interval predicates. Null case IDs do not match a positive join;
they survive the negative whole-case selection. Float NaN case IDs group
together, and positive/negative zero case IDs share a group. Returned cells retain
their source floating bits and complete native context.

All output rows preserve source order, including noncontiguous rows of a case.
The captured typed integer case selector is a literal join expression: a
matching event selects the constant case, and output appends the Int32 `literal`
column with value 7. This is distinct from the String column name `"7"`. The
native lazy header projects that new column and dtype before materialization.
An existing conflicting `literal` column remains outside the observed adapter
domain.
Whole-case and numeric comparisons retain exact signed/unsigned integer values
beyond 2**53. Scalar String membership controls name another column rather than
being coerced into a one-element collection. Ordinary lists, tuples, sets and
mapping-key collections retain their observed public membership semantics;
unsupported dtype combinations retain their full diagnostics.

The public trace/event functions use `attribute_key` (default `concept:name`)
and `positive` (default true). Only whole-case operations use `case_id_key`
(default `case:concept:name`). Numeric trace filtering additionally applies the
active stream-filter key/value pairs to its eligible events before selecting
whole cases. A supplied key without a value compares with null and selects no
events. A value without a key is inert. Numeric event filtering ignores all four
stream-filter controls. `activity_key` and `decreasingFactor` parameter bindings
are inert in those ordinary functions.

`filter_polars_attributes_threshold` follows the explicit public activity role
and absolute count threshold. A genuine supplied count mapping bypasses counting;
explicit None recomputes counts. An empty supplied mapping returns the source
unchanged. `most_common_variant` admits keys present in that collection even
below the threshold; it does not introduce absent keys into the count mapping.
If every observed count key survives, this helper returns the complete source,
including null rows. Typed cache entries normalize Python-equal numeric keys:
1, True and 1.0 retain the first key position and the last supplied count.

`filter_polars_attributes_top_activities` keeps the selected number of most
frequent activities, default 25. None means no slice limit; zero selects no
activities. Negative, nonintegral and oversized controls preserve the observed
public diagnostics. Selecting every observed activity returns the complete
source. Ties at the cutoff have no claimed exact identity: native selection uses
first-seen ordering within equal counts, while the public witnesses choose
other tied groups. The bounded acceptance below checks rank, cardinality and
complete selected rows instead of fitting labels or an opaque hash seed.

`get_polars_attribute_values` returns a true typed key/count vector. It includes
a null key where observed, preserves exact integer/date/datetime/float keys, and
uses distinct case/value pairs when `keep_once_per_case` is truthy. Its default
is false. The concrete count result is separate from a filtered table. Public
mapping-entry order varies across equivalent grouping contexts and is excluded
from the native count projection; every typed key and count is compared.

Relative occurrence has a real omission distinction:

| keep_once_per_case | Numerator | Denominator | case_id_key |
|---|---|---|---|
| omitted | event counts | distinct cases, including null | active |
| false or None | event counts | source event count | inert |
| true | distinct case/value counts | distinct cases, including null | active |

`filter_polars_attributes_relative` multiplies its stake by that denominator and
retains matching events; `positive` is active. It always applies a predicate, so
ordinary null attribute rows do not survive merely because all counted keys
qualify. A selected case role equal to the attribute role with explicit true
preserves the captured duplicate-group-name error. The native option is optional
to preserve omission independently of false or None. The relative attribute
selector retains its captured String-key preference over a conflicting enum key;
the ordinary membership helpers retain enum preference.

Threshold, top-activity, relative and count helpers perform public statistics at
apply time. Captured eager DataFrame calls raise their observed missing `collect`
AttributeError after applicable schema checks. The direct eager membership and
numeric helpers work. Lazy statistics executes an apply-time scan and returns a
restartable predicate over the original source; ordinary lazy event/case filters
defer their scan until materialization.

## Carrier and diagnostic boundary

`PolarsAttributesFilterError` carries the class name, phase and length-carrying
complete message, including expression trees, resolved plans, context stacks,
literal NUL and native eager/lazy distinctions. `PolarsAttributesFilterCall`
also represents omitted required arguments and malformed parameter controls.
Production helpers operate on table cells and controls; retained fixtures are
compiled only into their dedicated test target.

The observed dtype domain is String, Int64, UInt64, Float64, Boolean, Null, Date,
Datetime(us, UTC), Categorical and Enum with complete ordinary String category
label dictionaries. Output columns, rows, dtypes, unused dictionary labels and
their public order are projected completely. Native attributes, metadata, field
provenance and source/control preservation are independently checked. Runtime
category namespace/identity, physical storage, dataframe object identity/index,
Python NaN-key object identity, exact lazy plans and public warning emission are
not supplied by the native table ABI. Those remain open boundaries.

## Original typed calls in the table workbench

`POST /api/reference-table-attributes` accepts original Polars calls through the
same genuine native EventTable File used by friendly mode. The four exclusive
fields are `engine=polars`, `operation`, `source_file` and exact `call_json`.
Presence of call text selects original mode even if empty or malformed; the
backend controls validation and preserves the captured text. The editor never
parses and reserializes an imported call.

The descriptor is `minera-polars-attributes-original-call-v1` with required
`input_kind` (`DataFrame` or `LazyFrame`) and ordered `keyword_entries`, an array
of `{key,value}` entries. It has no positional args array: the original source
alone was positional. Examples cover all nine methods and use the actual public
keyword names (`values`, `int1`/`int2`, `thresh`, `max_no_activities`,
`min_relative_stake`, or `attribute_key` as appropriate). Required keyword
omission remains available for the native public binding diagnostics.

The optional `parameters` keyword accepts an ordinary raw typed value or
`{type:"parameter_entries",entries:[{key,enum_key,value},...]}`. Ordered entries
retain literal versus enum identity and inactive supplied values. Omitted,
explicit null and empty entries are separate states. In `act_count0`, an
ordinary string-key typed map stays the original raw native value union; the
bank establishes no typed-count-cache arm input. Returned attribute counts are
complete typed key/count data with unordered mapping scope.

The private codec retains the immutable Polars oracle's scalar and container
constructors: integer precision, decimal double bits and native display string,
microsecond date carrier, empty sequence child keys and ordered literal map
keys. It establishes no UUID, NaT, pandas Timestamp, frozenset or arbitrary
object domain. The complete source File retains its own original native field
and temporal/category context; it is not rewritten to fit the call editor.

The unchanged 23-field envelope records exact raw call text and full actual
native call state before/after, including input kind, keyword order, every
native argument optional, options/defaults, parameter state and count union.
An omitted optional differs from a present null-valued cell artifact. Apply
versus collect errors follow the actual eager/lazy overload. Complete source,
source-after, parsed controls, filtered tables or true counts and response bytes
remain downloadable after applicable errors. Filtered result Files can be used
as the next source; count tables are not offered as filtered-source reuse.

The full transport acceptance scope is all 726 unchanged original calls,
including 455 exact frames, all nine existing bounded cutoff predicates,
52 unordered typed maps and 210 full class/phase/message diagnostics. Native,
direct and owned HTTP acceptance remain separate from browser examples.
Implementation preparation adds no public queries or module promotion; current
execution results belong to root's checkpoint evidence. Warning emission,
public plans, error.args, Python aliases and runtime category namespace identity
remain outside this transport claim.

## Evidence and acceptance

The zero-call public surface pins reference commit
`0715c31f3f1d85924107d6aa5329806db69df899` and opaque module SHA-256
`ba363f34cde8b6967bc34d053de388a108fb0c6c2f4660f89d659668dcb8ea26`.
The earlier surface import failure (missing Polars) is preserved separately and
contains no algorithm calls. The retained exact-module search found only
catalogue/status/surface references before the new banks. No implementation,
source, AST, bytecode or private reference state was inspected.

Root executed these frozen observers once each:

- `filtering-attributes-polars-public-actual-1`: 700 distinct calls, 445 frames,
  52 typed count maps and 203 diagnostics.
- `filtering-attributes-polars-relative-public-actual-1`: 18 distinct calls,
  11 frames and seven diagnostics; all 700 prior full receipts reconstructed
  and their saved hashes verified before any new entry.
- `filtering-attributes-polars-ties-discriminating-public-actual-1`: eight
  distinct calls and eight frames; all 718 prior full receipts reconstructed
  before any entry. These vary actual activity ordering, frequency patterns and
  active k controls, rather than only changing irrelevant payload metadata.

Every bank deduplicates the entire complete actual-call receipt set before its
first algorithm entry and checks immediate pre-entry input/control drift.
All 726 calls have zero source/control mutations and unchanged opaque pins.
Public result-plan observations and their own warnings/errors are retained
separately before the one ordinary result collection. The records retain 234
count-renaming warnings, 14 deprecated pl.count warnings, 12 GroupBy count
warnings, 10 collection-is_in warnings and five comparisons-with-None warnings
in the first bank. Native warning emission is not claimed.

An initial proposed metadata-only eight-case tie recipe was **not executed**:
root rejected those irrelevant context differences as contrary to no-replay
intent. Its frozen files and `NOT_EXECUTED.json` are preserved. The replacement
eight-case bank changes actual selected data/order/frequencies or active k.

The generated 726-outcome native bank retains every original oracle snapshot:
455 complete frames require exact rows/schema/dictionaries; nine top-k cutoff
frames use the bounded relation below; 52 count maps require every typed key and
count (entry order excluded); 210 diagnostics require exact class/phase/full
message. Each generated case and frame is a small noinline function to avoid
MSVC test-stack overflow. The real target requires `/bigobj`.

For each ambiguous cutoff, all groups above the kth frequency must survive,
exactly k groups must be selected, and remaining selected groups must come from
the cutoff tie. All occurrences of each selected group must appear in original
source order, with exact full rows/columns/dtypes/context/dictionaries. The
original captured expected frame and actual native output are independently
validated against this relation. It cannot accept lower-frequency groups,
missing occurrences, changed cells/context or a different group count. The
pre-admission 718-vector projection is preserved and its reconstructed bytes
were verified against its previously recorded SHA-256
`dd1b54e727d2d1319109ca0892c99cb491d04ed68e7aa393ddfe83d554a5b3b6`.
The helper body is unchanged by this test admission; no captured expected rows
were rewritten.

The first module CTest passed pandas but terminated the uncaught Polars driver
with 0xc0000409. A catch-only driver confirmed that numeric case selector 7 was
incorrectly rejected as an invalid column selector. The general literal-case
projection repair changes source grouping and output schema together; the
original helper, driver, expected vectors and object/executable/map identities
are preserved before repair. The complete expected vectors remain byte-identical
(`f699f1152ad129aeb847dcd466f9fbd9e4bd2aab496f79638e2965c2691ec4d2`).
The repaired run passed those literal projections and exposed one full-message
byte mismatch: the public Datetime diagnostic uses Greek mu U+03BC, whereas the
formatter used micro sign U+00B5. The general datatype formatter was corrected;
its prior source and compiled identities and failed receipt are preserved.
Root passed the 25 focused frontend tests for active controls, tri-state defaults,
complete artifacts, genuine uploads, request identity and cancellation.
Root passed the final **41/41 native suites** in
`build/parity-closures/native-current/ctest-attributes-final.log`, with successful
`build-attributes-final.log` and `configure-attributes-workflows-first.log` in
the same folder. The Polars suite accepts all 726 outcomes (455 exact frames,
nine bounded cutoff frames, 52 complete unordered typed maps and 210 full
diagnostics). The earlier targeted two-module pass remains historical.
The authoritative production receipt is
`build/parity-closures/attributes-production-http-4/validation.json`: all 95
checks passed against server SHA-256
`1d6b11e78d1197130a162db0419df62ec62e906de4cbee1db60e26e74c54b1e3`.
The dedicated owned server exited, and the existing application was untouched.
Three prior harness attempts remain preserved with their original scripts and
raw records: attempt one failed to unwrap a valid raw-byte count EventTable
after 64 checks; attempt two expected incorrect threshold trace properties;
attempt three needed the separate exported role for generic XES matching.
These harness corrections did not change the product.

The final frontend receipts under `build/parity-closures` are
`frontend-attributes-full-third-final.log` (190 files, 9,809 tests),
`frontend-attributes-focused-third-final.log` (65 focused tests),
`frontend-attributes-tsc-third-final.log` (successful empty TypeScript output)
and `frontend-attributes-vite-third-final.log` (successful production build).
The dedicated evidence archive binds these receipts, the authoritative native
build and CTest logs, all three failed HTTP attempts, immutable public vectors,
pre-repair sources and compiled identities, and the final source/binary hashes.
The whole module remains partial regardless of the bounded companion result;
cutoff identity, warning/plan/object fidelity and unobserved dtype/object domains
are not promoted by these projections.
