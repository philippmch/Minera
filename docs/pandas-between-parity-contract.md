# Pandas between filtering: bounded public parity contract

The public target is `algo.filtering.pandas.between.between_filter`, pinned to
PM4Py 2.7.22 commit `0715c31f3f1d85924107d6aa5329806db69df899`.
The retained runtime uses pandas 3.0.6 and Python 3.11.9. Its callable is
`apply(df, act1, act2, parameters=None)`. The independent native
companion is `filter_pandas_between` in `include/minera/pandas_between_filter.hpp`.
The existing `segment_filter.cpp` convenience functions retain their contract.
The canonical pandas module remains **partial** for the boundaries below.

## Observed selection and case labels

The pandas observations use each ordered distinct case value's first source
position and total source count as a positional block. They do not gather
all rows carrying that case value. This distinction matters for interleaved
cases and for selectors that share a column: their blocks can overlap.
The native companion preserves the captured behavior, including labels that
are overwritten by a later overlapping block. Timestamps do not reorder rows.

Within a block, the first start anchor opens a slice; additional starts do
not replace that opening. A later end closes the inclusive slice. Generated
case labels are the case value's string form, the literal separator and a
zero-based completed-slice index. The default separator is `##@@`; an explicit
empty separator is valid. Every completed slice gets a generated label,
including a case with only one completed slice.

The closing row starts the next slice only when the two original anchor
queries compare equal. Equal strings, lists, tuples and dictionaries do so;
sets and frozensets can compare equal across those two kinds. Reordered lists,
string-versus-singleton-list queries and dictionaries with different values
can have identical membership behavior while comparing unequal, and therefore
do not reuse that row. A reused row receives the later completed slice's
label; output contains one copy of that source row. Unclosed final slices are
omitted, and returned rows keep source position order.

A string anchor is a single literal label, preserving empty strings,
whitespace, Unicode, commas, newlines and NUL. Other ordinary collections use
membership; dictionary queries use keys. Noniterable scalar queries retain
the captured type errors. Bytes queries retain the captured ordinary Python
membership error when the selected value is None. The finite bank does not
establish every bytes membership combination or arbitrary Python equality.
NativeNA activity values retain the observed ambiguous-truth error when their
comparison is reached.

## Controls, source values and table fidelity

The three controls are literal case and activity column selectors and the
subcase concatenator. Native options carry ordinary typed attributes, so
omitted defaults remain distinct from explicit empty/null/nonstring controls.
The public-parameter overload also accepts null, dictionary and empty-list
carriers and preserves the captured invalid bool/integer/string-carrier
errors. Callers resolve enum bindings and their observed precedence before
transporting dictionary entries.

Missing columns, empty-array indexers and dictionary indexers retain captured
error kinds and complete diagnostic messages. Case lookup occurs before
activity access. Invalid separators raise only when a slice completes; an
empty source does not reach that concatenation. Ordered case values include
strings, numeric scalars, datetimes, raw bytes, lists and tuples in the finite
bank. The captured mixed scalar, null and dictionary case domains raise their
observed ordering errors; they are not silently stringified before ordering.
Case labels use ordinary captured string forms, including bytes/list/tuple
representations and datetime spaces/offsets.

The captured float64 case array groups NaN values and keeps the first signed
zero representative. Nullable Int64 cases with missing values use a floating
array projection, including `0.0`/`nan` case label forms. Native source cells
remain unchanged. Naive `datetime64[ns]` cases use fixed-nanosecond `T` label
forms and group NaT; aware Timestamp cases use spaces and timezone offsets,
and the captured aware NaT rows do not form a complete slice. Adjacent
nanoseconds remain distinct in both case grouping and list-wrapped datetime
anchor comparisons. Timestamp labels retain their nanosecond fractions.


Every retained row keeps its complete stored cells and all unaffected column
dtypes, categorical declarations, unused category order and ordered flags.
The generated case column has dtype `str` when output is nonempty and
`float64` when empty; its former categorical descriptor is cleared. Index
labels and names survive, including duplicates, strings, stepped ranges and
MultiIndex tuples. A filtered RangeIndex remains a RangeIndex when the selected
labels form an arithmetic progression; otherwise it becomes an Index.
Dataframe attributes and native metadata remain intact, including raw bytes,
NUL, nested metadata, exact UInt64/nullable integer payloads, signed zero and
nanosecond timestamps. The source is unchanged and native output storage is
independent.

## Retained observations and native acceptance

Root executed a zero-algorithm public signature/doc/enum and opaque-identity
observer at `build/filtering-between-public-surface-actual-1`. A search of prior
algorithm receipts found none for this pandas entrypoint; catalogue/status
metadata and the older log-only segment evidence do not establish pandas
algorithm behavior.

The first fresh bank at `build/filtering-between-pandas-public-actual-1`
removed two duplicate candidates before any algorithm entry. Its **117
distinct explicit calls** returned **66 complete tables** and **51 errors**.
A fresh eleven-call continuation at
`build/filtering-between-pandas-collection-public-actual-1` resolves the raw
anchor-equality branch with **11 complete returns**, no errors and no overlap
with the prior bank. A third twelve-call bank at
`build/filtering-between-pandas-dtype-public-actual-1` adds **9 complete
returns** and **3 errors**, with no overlap. Across all three banks, **140
distinct calls** returned **86 complete tables** and **54 errors**. No source,
anchor or parameter mutation
was observed, and opaque module identities remained unchanged. No retained
calls were rerun; reference implementation source, AST, bytecode and private
state were not inspected.

All three recipes instantiate and fingerprint all complete actual source/anchor/
parameter/invocation receipts before entry, remove duplicates/overlaps first,
and retain a durable full call plan plus immediate per-entry intent. Their
offline native generator imports no reference package and embeds **110 full
native snapshots**, **86 complete returns** and **53 representable diagnostics**
in `test_pandas_between_filter.cpp`. The missing-required-act2 error is outside
the C++ signature. An independent own reconstruction from the retained
observations checks returned positions and generated labels for the first two
banks' 77 tables, plus three original exact ordering diagnostics, without new
reference calls. The third
bank adds direct full-table tests for dtype-specific missing values and
adjacent-nanosecond case/anchor values.

`docs/pandas-between-checks.jsonl.gz` retains all three public banks, recipes,
pre-entry audits/plans, public surface, native source/tests and final native
acceptance/build/object/binary bindings. Individual members retain byte lengths
and SHA-256 hashes. Native target `minera_pandas_between_filter_tests` is the
assertion-enabled acceptance target for all **139** representable outcomes and
independent metadata/ownership checks. The named pandas target passes all
139 outcomes. The first combined native run caught a list case-label formatter
mismatch, repaired locally without changing expected tables. The second run
passes the pandas target while a separate log target still fails; neither
combined run is represented as an overall pass. Their receipts are retained
with the final accepted source/object bindings. The third coordinated run
passes all **7/7** named native/workbench/route suites; this final receipt
binds the accepted pandas source, test, object and executable contents.

## Remaining boundary

Native tables have unique string column labels and no dataframe column-index
name; all observed column-index names were null. This bank does not establish
arbitrary pandas extension dtypes, nonstring/duplicate column labels,
custom Python objects, all timezone identities, arbitrary NumPy mixed-object
sort/comparison orders, scalar-wrapper/children-list payloads, nonstring
nested dictionary keys or all mutable selected-cell equality combinations.
Nullable UInt64/Float64 and float32 case projections, and typed datetime
values nested in container-valued case labels, remain uncovered ordinary
domains. Undeclared dtypes and other native-only input encodings remain separate adapter
boundaries. No Polars or log between-filter behavior is inferred from these
pandas observations. These limits keep the canonical module partial.
