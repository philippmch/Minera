# Pandas paths: bounded public/native contract

The dedicated `pandas_paths_filter.hpp` companion implements the captured
`apply` and `apply_performance` behavior of
`algo.filtering.pandas.paths.paths_filter`, pinned to PM4Py 2.7.22 commit
`0715c31f3f1d85924107d6aa5329806db69df899`, pandas 3.0.6 and Python 3.11.9.
The opaque module SHA-256 is
`aee8c4bc3e31fd7f2227c77cd4aa094f722e706d76e0bfc47378a15fcbaa8c4d`.
The canonical module remains **partial**; existing convenience path filters
and historical filter checkpoints retain their own contracts.

## Ordering, adjacency and complete cases

Both operations first sort the entire dataframe by the selected case column
and timestamp column. Timestamp selection is therefore active for plain path
filtering too. Captured case-category declarations determine category order;
ordinary case values use their observed typed ordering. Equal sort keys retain
the observed source order. Missing sort values appear last. Case equality
retains numeric/bool equivalences, unsigned integer identities, tuples, bytes,
timestamps and literal empty/whitespace strings. Mutable list case identifiers
raise the captured unhashable-key error.

Paths use consecutive rows in that sorted dataframe, with both rows belonging
to the same nonmissing case. Selection keeps or excludes complete cases, then
returns rows in the sorted dataframe order, including all original columns and
their context. This differs materially from filtering only matching events or
retaining source order. Noncontiguous source cases and reversed/unsorted times
are included in complete retained snapshots.

## Path encoding and selectors

`filter_pandas_paths` accepts the captured outer list/tuple/set/frozen-set of
path pairs. Inner lists and tuples both use their first two entries; extra
entries are ignored in the observed forms. Short/scalar/malformed paths retain
their exact indexing/iteration errors. `filter_pandas_path_performance` accepts
one indexable pair, including the captured two-character string form.

The reference encodes a pair by concatenating its first string, a comma and
its second string. The native companion deliberately retains this captured
behavior. For example, `('A,B', 'C')` and `('A', 'B,C')` select both captured
cases because their encoded paths coincide. A comma followed by a space
remains distinct. Unsupported numeric/bytes/container/date endpoint additions
retain their captured diagnostic types/messages; ordinary missing dataframe
activity cells do not create matching paths.

Plain filtering uses `attribute_key` for the source event and the optional
`target_attribute_key` for the next event. An absent target selector follows
the selected source attribute; an explicit empty target selector is literal.
Performance filtering uses the selected source attribute for both endpoints
and ignores `target_attribute_key`, including all captured invalid target
controls. The separate positive target probe confirms this distinction.

Role names remain literal, including empty strings, surrounding spaces,
Unicode, newline and NUL. The native options carry typed `EventAttribute`
controls. Ordinary public parameter mappings use literal keys;
`PandasPathsFilterParameter` also preserves enum-key identity. Captured enum
keys override equivalent string keys in both insertion orders. Irrelevant
`decreasingFactor` and other captured controls do not alter output.

## Performance and bounds

Performance filtering requires at least one matching path occurrence whose
next timestamp minus source timestamp lies in the inclusive supplied range.
It computes durations after sorting and preserves the captured fractional and
nanosecond boundary behavior. Missing times yield missing durations. Numeric,
string and object-datetime timestamp declarations retain their captured
subtraction or datetime-accessor errors. The native source retains microsecond
versus nanosecond date precision, including the long-duration microsecond frame.
Captured date endpoints and date-valued activities are pandas Timestamp scalars
at either precision; their diagnostic class is independent of the precision.
The native path operand projection preserves that Timestamp class name.
Standard-library datetime identity as a path endpoint/activity is unobserved.

Positive controls use the captured ordinary Python truthiness behavior.
Null bounds reject comparisons, NaN rejects all durations, infinities remain
numeric, and strings/dictionaries/list-length mismatches/date bounds retain
their public diagnostics. The first bank passed nested marker dictionaries
inside ordinary parameter mappings literally; their dict errors remain
authoritative. The fresh continuation explicitly decoded genuine NaN,
infinity and Timestamp bounds and retained those separate outcomes.

An omitted public upper bound admits the captured year-0100 to year-5000 path;
explicit 1e10 and 1e11 second bounds exclude it. The native omitted upper bound
uses int64-max seconds, above the entire supported native calendar span. These
pandas observations establish admission across that bounded calendar domain;
they do not identify the exact Python default scalar literal. The usable
workflow can supply explicit criteria independently of the omitted API default.

## Frame preservation and diagnostics

Full comparisons preserve column order, source row payloads, declared dtypes,
unused category entries, nullable values, dataframe attributes and original
index labels/names as they move through sorting and filtering. RangeIndex,
materialized Index, duplicate/string/stepped indexes and MultiIndex are covered.
Signed zero, nonfinite payloads, exact large integers, nested attributes, bytes
and high-resolution timestamps remain intact. All source inputs and public
query/control values remained unchanged in the retained run.

`PandasPathsFilterError` exposes the full public diagnostic type and message.
Diagnostic precedence includes query construction before dataframe selection,
sorting-key failures, attribute selection, string concatenation, timestamp
subtraction and bound comparison in the captured combinations.

## Fresh receipts and native acceptance

The global retained JSON/JSONL search found 26 catalogue/surface files and no
earlier explicit algorithm receipts for this exact module. Root alone executed
the frozen first bank once at `filtering-paths-pandas-public-actual-1`:
252 candidates were fully instantiated and deduplicated before entry, removing
two duplicate candidates, then **250 distinct calls returned 140 full tables
and 110 errors**. The duplicates are retained in the pre-entry audit and were
never entered.

Root then executed the fresh sixteen-call continuation once at
`filtering-paths-pandas-continuation-public-actual-1`: **16 distinct calls,
15 full returns and one error**, disjoint from all 250 earlier receipts. The
continuation re-fingerprinted both full prior plans and records, confirming
250 unique earlier call identities. All 266 actual calls have complete typed
input/query/control/invocation receipts, durable pre-entry plans and immediate
call intents. Both opaque hashes remained unchanged; no reference source,
AST, bytecode or private state was inspected. No algorithm calls were replayed.

Both recipes capture warnings around each call using an explicit always filter.
Every retained outcome has its warning list: **zero warnings were emitted across
all 266 calls**, with zero source/query/control mutations and zero prior overlaps.

`test_pandas_paths_filter.cpp` embeds all **266 native counterparts: 155 complete
returns and 111 exact type/message diagnostics**, using **100 unique complete
source/result snapshots** generated offline from retained outcomes. Each case
and each snapshot uses a separate noinline function; every case checks source
preservation. No prior filter test or oracle was changed.

The named pandas suite passes all 266 outcomes in the final verbose
`build/parity-closures/native-current/ctest-paths-final2.log`; the coordinated
regression run passed 24/24 suites. The earlier compiler failure from a private
helper's `std::bind` name collision, first diagnostic-class mismatch, repaired
three-helper PASS and earlier 24-suite PASS are retained. Those repairs changed
no expected table or diagnostic data.

The shared production workflow passed 44/44 requests in
`build/parity-closures/paths-production-http-3`, including fifteen requests with
an explicit pandas engine field. The prior two HTTP harness failures retain
their original scripts, requests and validations. Final native/XES/context
reopening specifies the actually exported activity role. These workflow
receipts are distinct from the 266 pinned-reference algorithm observations.

The separate `docs/pandas-paths-filter-checks.jsonl.gz` archive retains both
frozen recipes, the global search/audits, full call records and warning lists,
preparation and generation scripts, source/test snapshots and actual final
build/CTest/object/executable/link-map bindings. Every member includes a byte
length and SHA-256 hash.

## Remaining boundary

The finite bank does not close arbitrary extension dtypes, every mixed-object
sort order, nonstring/duplicate column labels, a named column index, custom
Python comparison classes, arbitrary vector-bound broadcasting, every nested
missing-scalar identity graph or Timestamp class/resolution/zone combination.
The native date carrier distinguishes precision, without representing every
Python scalar-wrapper identity. Further warning classes or emissions require
fresh observations. Polars/log adapters retain their own public contracts;
this pandas module remains partial.
