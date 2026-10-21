# Pandas variants: bounded public/native contract

The dedicated `pandas_variants_filter.hpp` companion implements all four captured
public functions of `algo.filtering.pandas.variants.variants_filter`, pinned to
PM4Py 2.7.22 commit `0715c31f3f1d85924107d6aa5329806db69df899`, pandas 3.0.6 and
Python 3.11.9. The opaque module SHA-256 is
`2c898ea1414b7490c75fb0e95045b7963798a345be28ad87e8109657e637195f`.
The canonical module remains **partial**. Existing convenience variant filters
retain their separate contracts.

## Membership and variant shapes

`filter_pandas_variants` retains or excludes complete cases whose activity
sequence matches an admitted variant. Membership aggregates each case's actual
rows in source order, including noncontiguous case occurrences. Output preserves
source row order and every original column. Missing case IDs do not form normal
membership groups. Numeric/Boolean case equivalence, literal empty/whitespace
IDs, bytes, recursive tuples and nanosecond date IDs retain their captured
behavior.

The documented outer List[List[str]] form produces no matches in the captured
pandas runtime. Inner tuples match the dataframe's tuple sequences; inner lists
remain lists and do not match them. The native companion preserves this
observable distinction. An outer list, tuple, set or frozen set remains
list-like; a scalar string, number or null retains its `isin` diagnostic.
`PandasVariantsAdmittedMapping` preserves genuine typed mapping entries, including
a captured tuple key; dictionary membership iterates keys and leaves values
unchanged. Generic EventAttribute mappings continue to support string keys.

Typed tuple targets match the captured numeric, Boolean-equivalent, bytes,
nested-tuple and date activity sequences. None, NA and NaN activities retain
their observed tuple-membership behavior. Comma-containing activities remain
literal: ('A,B', 'C') and ('A', 'B,C') are distinct variants. Empty activity
strings and surrounding spaces remain literal too.

## Statistics, ranking and coverage

The three statistical filters use a different captured construction from
membership. For each case, they take a contiguous activity slice starting at
that case's first source-row position, with length equal to its total event
count. On contiguous cases this is the ordinary case sequence. On noncontiguous
cases it can include events belonging to other cases. This rule is confirmed
by small four-row frames and supplied-variant-dataframe witnesses. The
statistical filters then pass their chosen tuples to normal membership, which
uses the actual case rows. This explains captured noncontiguous outputs that
would differ from a conventional group-by-activity implementation.

`filter_pandas_variants_top_k` ranks by descending frequency and descending tuple
order for captured ties. It uses the captured Python slice behavior for integer
k, including zero and negative k. Booleans act as integers. A bound larger than
the variant count clamps to that count, including positive infinity; other
floating slice bounds retain their exact diagnostics. Integer criteria and
index arithmetic use exact TokenInteger operations, preserving values beyond
signed-int64 and double's exact-integer range.

Minimum and maximum coverage select variant counts respectively >= and <= the
supplied fraction times the observed case count. The boundaries are inclusive.
NaN admits no variants, while infinities follow the captured comparisons.
String/list/null/dictionary/date criteria retain their captured multiplication
or comparison errors. Captured date criteria and object activities are pandas
Timestamp scalars; standard-library datetime class-specific diagnostics remain
unobserved. Malformed case/activity domains retain exact sorting,
hashing and NA-ambiguity diagnostics. `positive` is active in all four modes;
its captured ordinary truthiness controls inclusion versus complement.

## Supplied dataframes and controls

`variants_df` is optional. When present as a genuine EventTable, it supplies a
`variant` column and case identities in its index. A dataframe with case IDs in
a regular column and a numeric RangeIndex therefore has different membership
semantics from a case-indexed dataframe. Duplicate index labels can select the
same source case through multiple supplied variants. The complete supplied
frame, dtype/category declarations, attributes and index are retained and
mutation-checked in public receipts and native fixtures. Explicit None and
other malformed supplied values raise the captured `.empty` AttributeErrors;
absence remains distinct. Missing literal `variant` columns retain KeyErrors.
Statistical criteria still come from the source dataframe and then use the
supplied mapping during membership.

The options carry typed literal case/activity selectors and positive controls.
The mapping adapter retains enum-key identity and insertion order. In this
module, the last normalized enum/string duplicate key wins; both insertion
orders are captured. `decreasingFactor` is inert in the captured operations.
Timestamp selectors are also inert in all four modes, including an explicit
missing selector, a literal reverse-order alternate timestamp column and an
absent canonical timestamp column. These pandas observations remain separate
from other engines' timestamp behavior.

## Complete frames and evidence

Full outputs preserve every typed cell, column order, dataframe attributes,
index class/dtype/names/labels, declared dtypes and complete categorical
metadata including unused categories. Captures include nullable Boolean/String,
signed zero/NaN, bytes, tuple and date values, duplicate and MultiIndex labels,
and RangeIndex labels near both signed-int64 limits. Exact arithmetic preserves
selected RangeIndex classification without long-double rounding; unsigned
index labels retain their complete identities.

Root executed two frozen banks once each. The first contains 439 distinct calls
from 441 candidates after two duplicates were removed before entry: 305 returns
and 134 errors. The disjoint continuation contains 52 returns and no errors.
Before continuation entry, all prior complete receipts were reconstructed and
every saved hash verified; 878 prior plan/record rows describe the same 439
unique calls. Across both banks there are 491 distinct complete typed receipts,
357 full-table returns and 134 exact type/message diagnostics. All per-call
warning lists are retained; zero warnings and zero source/argument/control
mutations were observed. No reference implementation source, AST, bytecode or
private state was inspected.

The offline generator creates 177 complete source/result/supplied-frame
snapshots and 491 separate noinline native cases. The first direct native run
passes all 357 full tables and 134 exact diagnostics. The authoritative final
coordinated build and CTest pass all 34 suites. The named pandas suite passes
on both initial and final CTest attempts; its sources and fixture expectations
required no repair. The initial whole CTest passed 33 of 34 suites, with its
separate Polars failure retained alongside the final passing logs.

The shared production checkpoint passes 50 HTTP requests, including 20 explicit
pandas-engine requests; the owned server exits after verification. The final
server SHA-256 is
`e6037fea1b18de19e983595c1d7bf22281288496d3161d35e87936ea9441df90`.
The final shared frontend run passes 186 files and 9,755 tests, the focused run
passes 38 tests, and TypeScript/Vite checks pass. Historical focused and native
attempts remain in the evidence alongside the final checks. These shared checks
do not add public reference algorithm calls. The deterministic archive binds
public receipts, preparation/generation scripts, complete native fixtures,
source files, build/test/production/frontend receipts and current compiled
native/frontend artifact hashes.

The bounded checkpoint does not establish arbitrary Python/custom extension
classes, duplicate/non-string dataframe columns, every unobserved mixed-type
sort/error order, Python object-identity aliases, unsupported deeply nested
precomputed MultiIndex labels, or all malformed control combinations. Those
remain explicit gaps in the canonical module rather than implied full parity.
