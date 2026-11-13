# Polars sequence filters: public-call bank (2026-10-25)

This checkpoint closes twelve partial Polars filter modules

* `algo.filtering.polars.polars_filtering_constants`
* `algo.filtering.polars.start_activities.start_activities_filter`
* `algo.filtering.polars.end_activities.end_activities_filter`
* `algo.filtering.polars.traces.trace_filter`
* `algo.filtering.polars.starts_with.starts_with_filter`
* `algo.filtering.polars.ends_with.ends_with_filter`
* `algo.filtering.polars.rework.rework_filter`
* `algo.filtering.polars.prefixes.prefix_filter`
* `algo.filtering.polars.suffixes.suffix_filter`
* `algo.filtering.polars.consecutive_act_case_grouping.consecutive_act_case_grouping_filter`
* `algo.filtering.polars.timestamp_case_grouping.timestamp_case_grouping_filter`
* `algo.filtering.polars.activity_split.activity_split_filter`

with one seeded bank of public calls. It ran in the cloud container on the
pinned runtime (Python 3.11, pm4py 2.7.22, polars 1.44.1). Only public
functions, signatures and observed results were used, plus plain Polars
joins run on their own to fix the row order of inner joins. No reference
source was read.

## Native API

`include/minera/polars_sequence_filters.hpp` and
`src/backend/polars_sequence_filters.cpp` hold the constants and one function
per public callable over the native event table (`EventTable`, a Polars frame
without index), plus `LazyEventTable` entry points. Arguments and parameter
dictionaries reuse the pandas sequence filter types (`PolarsFilterValue` is
`PandasFilterValue`). Public failures raise `PolarsSequenceFilterError` with
the exception class and its complete message, including the Polars
"Resolved plan until failure" text and context stack.

## Tooling

* `scripts/polars_sequence_filter_domain.py` generates the cases from the
  pandas sequence frame generator: 0-14 rows over 1-4 cases with String
  (occasionally null), Int64 or Categorical case ids, a small activity pool
  plus odd names, occasional null activities, UTC timestamps with repeats and
  out-of-order hours, an optional Float64 column with nulls and NaN, shuffled
  rows, and `cid`/`act`/`ts` column names in 15% of cases. Calls cover every
  public function with Parameters members, plain and uppercase keys, missing
  columns, non-boolean `positive` and `min_occurrences`, unknown filter types
  and cut modes, count dictionaries, `most_common_variant`, sets, and string,
  list and tuple patterns with `...` wildcards.
* `scripts/capture-polars-sequence-filter-bank.py` runs every call three
  times on a fresh LazyFrame, in fresh interpreters for `PYTHONHASHSEED` 0-3
  with `TZ=Europe/Berlin`, and writes `docs/polars-sequence-filter-bank.json.gz`.
  A call whose outcomes differ only in the order of table rows or dict
  entries is stored once as `unordered`; any other difference keeps every
  observed outcome.
* `minera_polars_sequence_filter_bank_parity_tests`
  (`src/backend/tests/test_polars_sequence_filter_bank_parity.cpp`) replays
  each call, one function per module. Frames compare through the normalized
  event-table JSON (columns, dtypes, every cell). Unordered calls compare with
  rows and entries sorted; end-activity count dicts always compare unordered,
  since they come from a `group_by` without `maintain_order`. A set argument
  may reach the filter in any iteration order.

## Final bank

Bank seed 20261029, 3,000 cases, hash seeds 0-3, three runs per seed:

| Calls | Values | Errors | Unordered | Varying | Failures |
| --- | --- | --- | --- | --- | --- |
| 38,601 | 30,986 | 7,615 | 2,448 | 23 | 0 |

There are 2,371 calls per function, except `activity_split.apply` with 629.
Unordered calls are mostly `consecutive_act_case_grouping` (1,336) and the
end-activity counts. The 23 varying calls are `rework.apply_activity_set`
errors whose plan text prints the set in its hash-dependent order.

SHA-256 of the bank: `6b4991b2f4dda286b1d679c7fb39aea5328fcc51d2f389f63a92c0432cbcab73`.
Pilots with bank seeds 7 (300 cases) and 9 (600 cases) drove the fixes and
pass too; they are not part of the evidence.

## Behaviour established

### Joins and nulls

* Filters select cases and join them back: inner join for positive filters,
  anti join for negative ones. A null case id is a group of its own in
  grouping and statistics, but never joins: positive filters drop its rows,
  negative filters keep them.
* An inner join keeps the source row order, except when the source frame is
  no taller than the joined case frame: then the rows come in the case
  frame's order, each case followed by its rows in source order.
* `positive` is read for truthiness. A Parameters member key is read before
  the plain string equal to its value.

### start_activities and end_activities

* The boundary activity is the activity of the case's first or last row in
  row order, null included. Membership never matches a null.
* `get_end_activities` counts the end activities of every case group, the
  null case and null activities included.
* nocc keeps the activities counted at least `nocc` times (from the counts or
  `ea_count0`), plus the last activity of a non-empty `most_common_variant`.
  If every counted activity is kept the frame comes back unchanged.
  `return_dict` always returns `(frame, kept counts)`.

### Variants

* Variants follow the rows sorted by case and `time:timestamp` (stable,
  nulls first). The variant string joins the non-null activities with `,`.
  `get_variants_df` frames are ordered by case id; the traces one also has
  `variant_list` (List(String)) and `count` (UInt32).
* `trace_filter` matches `,`-joined patterns, with `...` for any run. An
  empty admitted list admits every case. The admitted ids become a Python
  list joined back: an empty match returns an empty frame (or the frame, for
  negative filters); a list holding only null ids, or str ids against a
  Categorical column, raises the join `SchemaError`.
* `starts_with` and `ends_with` test string prefixes and suffixes.
  `starts_with([])` raises `TypeError invalid predicate for filter: None`;
  `ends_with([])` raises `IndexError` after the variants are built. If the
  first suffix is a str, a later list raises
  `InvalidOperationError cannot cast List type`.
* prefixes and suffixes compare the activity lists element by element. An
  empty admitted list raises the join `SchemaError` against a Null column.

### rework

* A case is kept when some (case, activity) group, null activity included,
  has at least `min_occurrences` (default 2) rows; None keeps nothing.
  `apply_activity_set` first keeps the rows whose activity is in the set,
  and its plan text prints the set (or `false` for an empty one).

### Grouping filters

* consecutive_act_case_grouping merges runs of adjacent rows with equal,
  non-null case and activity into their first or last row. The rows come in
  an unspecified order. Other filter types return the frame after the
  column checks.
* timestamp_case_grouping merges the rows of each (case, timestamp) group,
  null case included, into its first or last row, in first-appearance
  order, with the columns as `[case, timestamp, rest]`. Other filter types
  return the frame before any column check.

### activity_split

* The new case id is `str(case) + SUBCASE_CONCAT_STR + count`, where `count`
  is the per-case running number of rows whose activity is cut. CUT_MODE
  `this` counts the current row; other modes use the previous row's count
  (0 for a case's first row). A null activity makes the count null on its own
  row (`this`) or on the next one (other modes). Null case ids stay null.

## Contract and domain limits

All twelve modules are complete on this bank. Outside the domain:

* Non-string column labels and non-string pattern activities.
* The grouped-dataframe and decreasing-factor parameters.
* The order of Categorical case ids in sorted outputs beyond the generated
  labels.

## Typed-key bank (round CA)

`scripts/polars_sequence_filter_typed_domain.py` extends the domain above:
case columns are Categorical, Enum (categories not in sorted order, unused
categories), `Int64`, `UInt8`, `Float64`, `Boolean` or String, with nulls;
activity columns Categorical, Enum (sorted or reversed categories) or
String. `scripts/capture-polars-sequence-filter-bank.py --domain
polars_sequence_filter_typed_domain.py --count 2000 --bank-seed 2026111302
--hash-seeds 0,1 --repeats 8` writes
`docs/polars-sequence-filter-typed-bank.json.gz` (eight runs per call and
hash seed, since Polars varies some row orders per run), replayed by the
same test (`minera_polars_sequence_filter_typed_bank_parity_tests`):

| Calls | Values | Errors | Varying across seeds | Unordered | Failures |
| --- | --- | --- | --- | --- | --- |
| 25,991 | 17,171 | 8,820 | 55 | 1,763 | 0 |

Review CA1 in `docs/reference-2.7.22-parity.md` lists the fixes (variant
lists on String activities only, the strict Enum cast of `is_in` values,
join key dtypes, Enum casts of prefix and suffix lists, the case sort by
dtype).

## Temporal and integer activity bank (round CB)

`scripts/polars_sequence_filter_cb_domain.py` extends the typed domain with
activity columns of `Int64`, `UInt8`, `Date` and `Datetime` (us naive or
UTC, ns naive or UTC, ms in Europe/Berlin across the 2021 DST change); on
integer activity columns about half of the cases give the activity
arguments as the mapped integers. `scripts/capture-polars-sequence-filter-bank.py
--domain polars_sequence_filter_cb_domain.py --count 1500 --bank-seed
2026111323 --hash-seeds 0,1 --repeats 8` writes
`docs/polars-sequence-filter-cb-bank.json.gz`, replayed by the same test
(`minera_polars_sequence_filter_cb_bank_parity_tests`):

| Calls | Values | Errors | Not stored (no public outcome) | Varying across seeds | Unordered | Failures |
| --- | --- | --- | --- | --- | --- | --- |
| 19,431 | 10,702 | 8,596 | 133 | 83 | 1,220 | 0 |

Review CB1 in `docs/reference-2.7.22-parity.md` lists the fixes (Python
list literals and their strict rebuild, the `is_in` dtype rules, the String
casts of variants, the casts of prefix and suffix lists, datetime lists on
ns and ms columns).
