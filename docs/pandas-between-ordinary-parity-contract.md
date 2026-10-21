# Pandas between: ordinary numeric and nested temporal continuation

This checkpoint extends the bounded `filter_pandas_between` companion using
fresh observations of `algo.filtering.pandas.between.between_filter`, pinned to
PM4Py 2.7.22 commit `0715c31f3f1d85924107d6aa5329806db69df899` with pandas
3.0.6/Python 3.11.9. The canonical module remains **partial**. The original
139 native oracles and their historical archive are unchanged.

## Numeric case-array projections

A nonmissing UInt64 case column keeps exact unsigned values through grouping
and label generation, including 9223372036854775809 and 18446744073709551615.
When a nullable UInt64 column contains missing values, the observed public
array projection is binary64. Missing values group as `nan`; large integers
use their projected floating labels. In the captured adjacent-integer case,
2^53 and 2^53+1 become one case, producing consecutive subcase indices from
the first representative. This behavior is specific to the declared dtype
and missing-value projection; ordinary native integer values stay exact.
Nullable Float64 case columns also use the observed floating array and NaN
grouping. Signed zero retains the first representative's sign.

Float32 cases project to binary32 before grouping and use the shortest
binary32 label in fixed notation for zero or magnitudes in [1e-4, 1e6), and
scientific notation otherwise. Comparisons use the actual binary32 value:
the captured decimal input 1e-4 rounds slightly below that boundary and prints
`1e-04`, while 0.00012 prints `0.00012`. The two fresh boundary sweeps also
establish fixed labels at 100000 and scientific labels at 1e6 and above.
Adjacent representable binary32 values stay distinct; source integers that
round to the same float32 case value share its completed-subcase sequence.
NaN, infinities and signed zero retain their observed label forms.

## Nested temporal case labels

Ordinary list/tuple case labels use Python-shaped nested representations.
Captured Timestamp values retain nine fractional digits, UTC or Europe/Berlin
identity and compact numeric offsets inside `Timestamp(...)`. Naive Timestamp
values omit a timezone argument. Captured standard datetime values retain
microseconds and their `datetime.datetime(...)` representation, including
`datetime.timezone.utc` for the observed aware form. Mixed nested strings,
Unicode, newline, NUL and tuple children keep their literal representation.
The native fixture maps the observed nanosecond Timestamp and microsecond
standard-datetime forms to distinct existing date-resolution carriers.

Nested typed NaT values use the captured container singleton equality behavior:
`[NaT]` rows can form a case and receive a complete slice. This does not change
the earlier scalar aware-NaT behavior. Complete source cells, dtypes, index,
unused categorical declarations and dataframe/native metadata remain intact
apart from the previously established generated case-column behavior.

## Fresh public receipts and native acceptance

Root executed the frozen sixteen-call bank once at
`build/filtering-between-pandas-ordinary-public-actual-1`. Before any algorithm
entry, the recipe re-fingerprinted all 140 prior full input/anchor/parameter/
invocation records, verified their original receipt hashes and audited all
sixteen fully instantiated calls. All sixteen returned complete tables, with
no duplicates, overlap or source/anchor/parameter mutation.

A fresh two-call continuation at
`build/filtering-between-pandas-float32-public-actual-1` resolves the float32
format branch using two sixteen-row magnitude/significand sweeps. Its complete
receipts were audited against all 156 prior records before entry; both returned
complete tables, with no duplicates, overlap or mutations. The opaque module
hash remained unchanged in both runs. No prior calls were rerun, and no
reference source, AST, bytecode or private state was inspected.

The new test `test_pandas_between_ordinary.cpp` embeds all **18 complete native
returned tables**, using **36 full source/result snapshots** generated offline
from the retained records. The original `test_pandas_between_filter.cpp`
remains unchanged with **139 native outcomes**. Together they cover **157
native outcomes** from **158 distinct public calls**: 104 complete returns,
54 public errors and 53 native diagnostic counterparts. The single arity
error remains outside the C++ signature.

Both named pandas suites pass in the final fourteen-suite native regression
run at `build/parity-closures/native-current/ctest-between-ordinary.log`:
the original 139 outcomes and all eighteen new full-table returns. The earlier
direct eighteen-case PASS is retained separately. The complete coordinated
run passed 14/14; this contract's semantic acceptance covers the two named
pandas suites.

`docs/pandas-between-ordinary-checks.jsonl.gz` retains the two new banks,
pre-entry full receipt plans/audits, all prior records required for independent
dedup verification, public surface, own preparation/generation recipes, source
and test snapshots, build/CTest receipts and final object/executable/link-map
bindings. The accepted companion and both native suites are bound to their
actual source and binary contents. Every archive member carries a byte length
and SHA-256 hash.

## Remaining boundary

These observations do not close arbitrary extension dtypes, Timestamp units
or datetime class/zone identities beyond the captured forms, arbitrary NumPy
mixed-object sort/comparison orders, other missing-singleton identity graphs,
custom Python objects, nonstring/duplicate column labels, a column-index name,
scalar-wrapper/children-list payloads or nonstring nested dictionary keys.
All observed column-index names were null. Native date-resolution metadata
separates the captured Timestamp-ns and standard-datetime-micro forms; it does
not establish every possible pandas Timestamp resolution/class combination.
Other adapters and the independent log/Polars modules retain their own
contracts. The canonical pandas row stays partial.
