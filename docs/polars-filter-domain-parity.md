# Polars attribute, between, case, ltl, paths, timestamp and variants filters: public-call bank (2026-10-25)

This checkpoint closes the last seven partial Polars filter modules

* `algo.filtering.polars.attributes.attributes_filter`
* `algo.filtering.polars.between.between_filter`
* `algo.filtering.polars.cases.case_filter`
* `algo.filtering.polars.ltl.ltl_checker`
* `algo.filtering.polars.paths.paths_filter`
* `algo.filtering.polars.timestamp.timestamp_filter`
* `algo.filtering.polars.variants.variants_filter`

with one seeded bank of public calls. It ran in the cloud container on the
pinned runtime (Python 3.11, pm4py 2.7.22, polars 1.44.1). Only public
functions, signatures and observed results were used, plus plain Polars
expressions run on their own to learn how a literal prints and how filters
merge. No reference source was read.

## Native API

The per-module headers stay the native API: `polars_attributes_filter.hpp`,
`polars_between_filter.hpp`, `polars_case_filter.hpp`, `polars_ltl_filter.hpp`,
`polars_paths_filter.hpp`, `polars_timestamp_filter.hpp` and
`polars_variants_filter.hpp`, each with `LazyEventTable` entry points.
`polars_between_filter.hpp` gains `resolve_polars_between_parameters` and
`resolve_polars_between_parameter_value` for the `parameters` argument.
Errors carry the exception class and the complete message, with the Polars
"Resolved plan until failure" text and context stack.

New shared helpers in `src/backend`:

* `polars_column_hint.hpp`: the "Did you mean" hint of a missing column (the
  first column at the smallest Levenshtein distance, if that distance is at
  most `max(1, len/3)`). The sequence filters use it too.
* `polars_list_literal.hpp`: the dtype of a Python list turned into a Polars
  Series, and the `TypeError` for mixed items.
* `pandas_parameter_carrier.hpp` (existing) now also checks the Polars
  `parameters` carriers.

## Tooling

* `scripts/polars_filter_domain.py` generates the cases from the pandas
  filter frame generator, as Polars frames: String, Int64 or (8%)
  Categorical case ids, UTC timestamps, `org:resource`, an Int64 or Float64
  `rd:amount`, `rd:kind` and an Int64 `rd` column, custom `cid`/`act`/`ts`
  names in some cases, shuffled rows. Calls cover every public function and
  constant. 8% of calls replace an argument, and 7% of parameter values,
  with an edge value (None, `""`, ints, floats with NaN and inf, booleans,
  empty list and tuple, a mixed list, a naive datetime).
* `scripts/capture-polars-filter-bank.py` runs every call three times on a
  fresh LazyFrame, in fresh interpreters for `PYTHONHASHSEED` 0-3 with
  `TZ=Europe/Berlin`, and writes `docs/polars-filter-bank.json.gz`. A call
  whose outcomes differ only in the order of rows or dict entries is stored
  once as `unordered`; any other difference keeps every observed outcome.
* `minera_polars_filter_bank_parity_tests`
  (`src/backend/tests/test_polars_filter_bank_parity.cpp`) replays each call
  through the `LazyEventTable` API, one function per module. Frames compare
  through the normalized event-table JSON. `get_attribute_values` dicts
  always compare unordered. Ties at the cut of
  `filter_df_keeping_spno_activities` are broken at random by the reference,
  so those calls check that the same number of activities is kept with the
  same counts, each with all of its rows. `filter_on_ncases` on a
  Categorical case column gets the same check: which cases come first in its
  `group_by` follows the process-wide category cache, which earlier frames of
  the capture filled. `MINERA_BANK_DUMP=<path>` writes
  each mismatch as a JSON line.

## Final bank

Bank seed 20261030, 3,000 cases, hash seeds 0-3, three runs per seed:

| Calls | Values | Errors | Unordered | Varying | Failures |
| --- | --- | --- | --- | --- | --- |
| 82,819 | 60,071 | 22,748 | 2,763 | 300 | 0 |

Most functions have 3,000 calls; the dict-argument `apply` and
`apply_auto_filter` functions of cases and timestamp have 213-241, variants
`apply_auto_filter` 900, and each constant one. Unordered calls are mostly
`get_attribute_values` (1,806) and `between.apply` (597). The 300 varying
calls are `filter_df_keeping_spno_activities` (178) and `filter_on_ncases`
(122): both cut a `group_by` result whose order changes between runs.

SHA-256 of the bank: `d935246e68157b51365ecae288245b0642421ac61d7fca99cb36c36935cddff2`.
Pilots with bank seeds 7 (300 cases) and 9 (600 cases) drove most fixes and
pass too; they are not part of the evidence.

## Behaviour established

### Shared

* `parameters` that is not a dict or None is read like a mapping: a str, an
  int or a list raises the same `AttributeError`/`TypeError` as pandas does.
* Python type names: `TypeError`s raised by CPython print
  `datetime.datetime` (or `datetime.date`); the Polars `col()` error prints
  `datetime`.
* An inner join keeps the source row order unless the source is no taller
  than the joined frame; then rows come in the joined frame's order. This
  decides the row order of positive variant filters.
* Python builds list literals before Polars runs: a list mixing types (for
  example `["a", 1]`) raises the Series construction `TypeError` before any
  column check, in every module.
* A list literal with more than three items prints its first two items,
  `…` and its last item. A naive datetime literal has dtype `Datetime('μs')`
  and prints as `2021-03-01 08:00:00`.
* Resolution type errors (a list or a datetime compared with a String
  column) come before the missing-column errors of later plan nodes, and
  before the optimizer drops a filter; runtime comparison errors come after
  all of them.

### attributes

* `apply_numeric` builds `col >= lo` and `col <= hi`, then for each stream
  filter whose key is not None `col(key) == value` (the value defaults to
  None). `apply_numeric_events` builds the two bound filters when `positive`
  is truthy, else one filter `(col < lo) | (col > hi)`; it ignores stream
  filters.
* Errors come in three phases. First Python: `col()` of a non-str key
  (`invalid input for col`), a list key holding a non-str
  (`'int' object is not an instance of 'str'`), and mixed list literals, in
  call order. Then plan resolution, filter by filter from the bottom: a
  missing column, an empty list key (zero expressions), and on a String
  column a list literal (`cannot cast List type`) or a datetime literal.
  The bound filters count as one context level and each stream filter as
  one more. For traces the case column is checked after all filters. Last,
  evaluation: Polars merges the filters, and the leftmost failing comparison
  of the lowest merged filter raises (`cannot compare string with numeric
  type`, or a `SchemaError` for a list literal on a numeric column).
* Filter merging follows predicate pushdown: walking down from the top,
  filters collect; a filter on a null literal (`col == None` prints as
  `null.repeat([col("k").len()])`) stays in place with the collected ones
  above it. Collected predicates combine by column name in bytewise order,
  the lower filter to the left, and the groups are joined left to right.
* Literals print after coercion to the column: on String columns ints and
  floats stay dynamic (`dyn int: 3`, `dyn float: 2`), booleans become
  `"true"`/`"false"` and compare as strings; on Int64 columns floats cast the
  column (`col("k").cast(Float64)`), datetimes become epoch microseconds
  (naive read as UTC); on Float64 columns every number prints as a float,
  with Polars' float display (`100.0`, `0.00001`, `1.6146e15`).
* The optimizer replaces a merged filter holding `col >= a` and `col <= b`
  on the same operand with a > b (both literals of the column's own type)
  by an empty frame, so that filter raises no comparison error.
* Comparisons are Kleene: a null cell or a None bound gives null, which a
  filter drops; in the negative events filter the other side can still keep
  the row. Floats use a total order where NaN is the largest value and equal
  to itself.
* `get_attribute_values` with a non-str key groups by that literal: on an
  empty frame it still returns `{literal: 0}`. A key list groups by each
  item (`ColumnNotFoundError` for a missing name).
* `filter_df_relative_occurrence_event_attribute` computes the counts, then
  the number of cases, then the threshold; only the final filter calls
  `col()` on the attribute, so a non-str key fails there. A None or datetime
  stake fails in the multiplication. Its counts check the case column before
  the attribute column. Membership in an empty list folds to `false`.
* The resolution plan of a failing trace membership filter has no `.not()`:
  negative trace filters drop cases in the join instead.

### between

* The `parameters` argument resolves plain keys first and Parameters
  members over them; non-dict carriers fail like pandas.
* When both anchors are invalid for the activity column, the second one is
  reported, and the plan prints the first one already resolved (without
  `.implode()`).
* The cast error of a list separator names the list's inner dtype.

### cases

* Datetime bounds of case performance compare as epoch microseconds and print
  as microseconds. The date check comes before the number check.
* The case duration is Int64: a float bound prints `col("case_duration").cast(Float64)`
  and a boolean bound prints as 0/1.
* The business-hours `NotImplementedError` comes before mixed-list bounds.
* `filter_on_case_size` adds a second `'filter'` context when the maximum is
  not None.

### ltl

* The context stacks of `eventually_follows` and `four_eyes_principle`
  failures grow with the number of activities: for `eventually_follows`, one
  `join left join` per remaining step after the failing one and one per step
  for the right side; for `four_eyes_principle`, one per extra activity pair.
* A list of activities with mixed types fails while building the Series.
  The cast error names the list's inner dtype.
* `eventually_follows` without activities builds no attribute filter: a
  positive call keeps every row with a case id, a negative one the rows
  without.
* `attr_value_different_persons` resolves a list or datetime value before
  the case and resource columns, but raises numeric comparison errors after
  them.

### paths

* An empty path list with a falsy `positive` returns the frame unchanged.
  Mixed lists in the performance bounds fail while building the Series.
* The path duration is Int64: float bounds cast it to Float64 in the error
  expression, datetime bounds compare as epoch microseconds.

### timestamp

* `filter_traces_attribute_in_timeframe` checks the attribute and timestamp
  columns of its filter first, then rejects list and datetime values on a
  String column, before the simplification of an empty time range.

### variants

* Positive filters follow the join row order rule above.

## Contract and domain limits

All seven modules are complete on this bank. Outside the domain:

* Non-string column labels, and lists of several column names.
* Attribute columns other than String, Int64 and Float64 in the numeric
  filters; datetimes with a non-UTC offset as bounds.
* Floats in plans outside the generated values, where Polars' float display
  switches format at thresholds that were only sampled.
