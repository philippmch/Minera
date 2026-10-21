# Pandas case filters: bounded public/native contract

The dedicated `pandas_case_filter.hpp` companion covers the six public functions
in `algo.filtering.pandas.cases.case_filter`, pinned to PM4Py 2.7.22 commit
`0715c31f3f1d85924107d6aa5329806db69df899`, pandas 3.0.6 and Python 3.11.9.
Its opaque module hash is
`64c874dff6366ec669a688828a84396e23eb848c95452d26c3c891e70a9ee82a`.
The canonical module remains **partial**. Existing convenience filters and
historical between/repetition checkpoints retain their own contracts.

## Captured operations

`filter_pandas_case_size` preserves source rows belonging to cases whose sizes
satisfy the inclusive minimum and optional inclusive maximum. Missing case
values do not form groups. Observed numeric/bool case keys use typed equality;
bytes, tuples, timestamps, large unsigned integers and literal empty/whitespace
strings retain their own identities. Lists are unhashable grouping keys.
The minimum filter runs before the optional maximum filter, including the
observed index representation changes between these two selections.

`filter_pandas_ncases` chooses cases by descending event count, preserving the
observed first-occurrence tie order, then returns their rows in source order.
Negative limits follow the captured prefix slicing behavior; zero keeps no
cases. A limit at least the case count keeps all nonmissing cases. The observed
NaN limit also keeps all nonmissing cases. Exact integer tokens remain valid
beyond machine integer width. Null/string/list and nonintegral slicing limits
produce their captured diagnostics.

`filter_pandas_case_performance` computes each case's last nonmissing timestamp
minus its first nonmissing timestamp in source order. It does not sort events.
Reversed events can therefore produce negative durations. Complete cases retain
all their source rows, including rows with missing timestamps. Cases with no
usable timestamps have a missing duration. Timestamp differences preserve the
captured nanosecond boundary behavior. The public `filter_case_performance`
wrapper accepts canonical string keys and enum-value parameter keys.

Business-hour performance uses the captured weekday 07:00–17:00 default slots
or explicit weekly second intervals. The bank covers weekdays, weekends,
cross-week intervals, empty/all-week/overlapping slots, subsecond endpoints and
ordinary malformed schedules. Empty inputs and all-missing case timestamps
retain the reference's business-hour exceptions. The observed reversed slot
does not trigger a newly invented native range-validation error. The captured
`workcalendar` controls do not alter the result; this does not establish an
arbitrary custom-calendar protocol.

The generic public entry points reject independently of input/parameters:
`apply_pandas_case_filter` raises `NotImplementedError` with
`apply method not available for case filter`; `auto_filter_pandas_cases` raises
`Exception` with `apply_auto_filter method not available for case filter`.

All controls use `EventAttribute` carriers. Canonical role defaults apply when
options are omitted. Explicit empty strings, surrounding spaces, newline,
Unicode and NUL in role names are literal. Bounds retain their declared scalar
types; strings do not silently become numbers. `PandasCaseFilterError` carries
the complete diagnostic type and message, including embedded NUL-safe text.

## Complete table preservation

Returned tables retain source column order, source row order, scalar payloads,
nullable declarations, non-case categories and unused category entries, dataframe
attributes and index labels/names. Source declared dtypes and case categories
also remain intact. RangeIndex survives an arithmetic selection from a
RangeIndex; an already materialized Index remains Index after a later filter.
Duplicate/string/stepped/MultiIndex labels remain associated with their source
rows. Signed zero, nonfinite payloads, exact integer payloads, bytes, nested
attributes and nanosecond timestamps are included in full snapshot comparisons.
Native inputs remain unchanged after both successful results and diagnostics.

## Public capture and native acceptance

Preparation searched 174 retained actual directories and 8604 public JSON
files. The sole matching file was the zero-algorithm public surface capture;
no prior case-filter algorithm receipts were found. The executable repeats the
retained-receipt search and fingerprints every complete instantiated input,
method, positional arguments, keyword arguments and call shape before any
algorithm entry. Its full call plan and immediate call intents are durable.

The first attempt, `filtering-cases-pandas-public-actual-1`, failed during
planning with zero algorithm entries because the observer used the invalid
encoding alias `utf8-sig`. Root retained that attempt and copied its frozen
recipe to `filtering-cases-pandas-public-prepared-2`, correcting only the alias
to `utf-8-sig`. Root executed the new `filtering-cases-pandas-public-actual-2`
once: **259 distinct public calls, 157 complete returned tables, 102 errors,
zero duplicates, zero prior overlaps and zero input/argument mutations**. The
opaque hash remained unchanged. No source, AST, bytecode or private reference
state was inspected. The zero-entry failed attempt adds no algorithm calls.

`test_pandas_case_filter.cpp` embeds all **259 native counterparts**, with
**101 unique complete source/result snapshots**, generated offline from the
retained outcomes. Every diagnostic compares its exact public type and complete
message; every return compares the whole table; every case checks source
preservation. The two unavailable generic entry points are tested across all
their captured shapes and controls.

The named pandas native suite passes all 259 counterparts in the final verbose
`build/parity-closures/native-current/ctest-cases-final.log`. The coordinated
regression run passed 19/19 suites. The earlier five-suite PASS and the initial
build's unrelated root-owned test compilation failure are retained separately;
the latter required no pandas companion changes.

The separate `docs/pandas-case-filter-checks.jsonl.gz` archive retains both
attempts, both frozen scripts, the full public records and pre-entry audit,
the preparation/generation recipes, source/test snapshots and final native
acceptance bindings. Archive members carry byte lengths and SHA-256 hashes.

## Remaining boundary

This finite bank does not establish arbitrary pandas extension dtypes,
nonstring/duplicate column labels, a named column index, every mixed-object
grouping/sort order, arbitrary NumPy scalar wrappers, custom Python classes,
all Timestamp resolution/class/zone identities, or custom workcalendar objects.
Per-call warning emission is not part of native acceptance: the root run emitted
six warnings, and the observer did not bind warnings to their originating calls.
Other dataframe/library runtimes may differ. The module remains partial until
uncovered public behavior receives its own fresh, disjoint evidence and native
acceptance.
