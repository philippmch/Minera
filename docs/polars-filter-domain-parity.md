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

## Typed keys (rounds CC and CD)

`scripts/polars_filter_cc_domain.py` rebuilds the frames of this domain
with typed activity columns (Categorical, Enum, `Int8` to `UInt64`,
`Float32`, `Float64`, `Date`, `Datetime` in us, ns and ms, naive, UTC,
Europe/Berlin and Asia/Kolkata) and typed case columns (Categorical, Enum,
the narrow integers and `Float32` with nulls, `Datetime`, `Date`); on
non-String activity columns about half of the cases pass the activity
values as values of the column's type, sometimes mixed with strings.
`scripts/capture-polars-filter-bank.py --domain polars_filter_cc_domain.py
--count 1200 --bank-seed 2026111335 --hash-seeds 0,1 --repeats 4` captures
`docs/polars-filter-cc-bank.json.gz` (33,137 calls), replayed by
`minera_polars_filter_cc_bank_parity_tests`. Round CC left 3,293 calls
open (Review CC1); round CD fixed them (Review CD1).

The table filters share one model of Polars literals with the sequence
filters (`src/backend/polars_lit_model.hpp`): lists as `pl.lit` builds
them, the `is_in` checks, and, new in round CD, `col(x) OP v` for a
column dtype and a Python scalar.

* **Comparison typing.** A str meets a temporal side, or a temporal value
  a String column, while the plan resolves (`cannot compare
  'date/datetime/time' to a string value`, with the resolved plan). A
  number on a String column, any non-str on a Categorical or Enum column,
  a datetime on a small integer column, a datetime of another zone (or
  awareness) on a `Datetime` column, and a bool on a temporal column fail
  when the filter runs (`cannot compare string with numeric type (i32)`,
  `cannot compare categorical with i32`, `could not evaluate comparison
  between series 'x' of dtype: Datetime('ms', 'Europe/Berlin') and series
  'literal' of dtype: Datetime('μs', 'UTC')`), with the failing
  expression and the larger one. A list value fails the same way on any
  column but a String one.
* **Coerced plans.** Once a comparison types, the plan prints it coerced:
  `col("x") == 1` on any integer column the value fits,
  `col("x").cast(Float64) == 1.5` for a float on integers, a constant
  `when(col("x").is_not_null()).then(false).otherwise(null)` for an int
  outside the column's range, `null.repeat([col("x").len()])` for None,
  `col("x").is_between([lo, lo + 999 ns])` for `==` on a nanosecond
  column, and the literal in the column's unit and zone otherwise. Inside
  an evaluated predicate a folded form prints bare, a `when` with a
  leading dot.
* **Matching.** NaN equals NaN and sorts above every number; a `Float32`
  column compares in single precision; a microsecond literal on a
  millisecond column is truncated; Enum order follows the category list,
  and an unknown str in an ordering raises the Enum conversion error. The
  value filters look a datetime up on a `Datetime` column as `is_in`
  does, by its microseconds whatever the zones.
* **Merged bounds.** Several `>=`, `<=` and `==` filters on one column
  merge in filter order as Polars' predicate pushdown merges them: a
  tighter bound replaces the current one, an equality bounds both sides,
  a literal without order against the current bound (another type, NaN)
  is kept, dynamic literals and cast columns stay apart, an empty interval
  empties the frame and a point becomes `==`; the merge applies only when
  it yields fewer predicates.
* **LTL.** The whole plan resolves before any filter runs (resolution
  errors first), left inputs first: in `four_eyes_principle` activity 0
  is the left input of pair 0 and a later activity first appears as the
  right input of pair (0, at); `eventually_follows` evaluates index 0,
  then n-1 down to 1. In the Python-side matching of `A_next_B_next_C` a
  `datetime.date` never equals a `datetime.datetime`.
* **between.** A str anchor is wrapped in a list, a list (a dict: its
  keys) is a list literal, and any other scalar is a `pl.lit` Polars
  implodes. The second anchor resolves first and its failure is reported
  first, the type check before the Enum conversion; an imploded scalar
  names its type `Unknown(Int(1))` or `Unknown(Float)`, prints
  `[x.implode()]` while it fails and `[x]` before it resolves, and once
  resolved casts to the column's list type (`[[1].cast(List(Int16))]`, a
  number that does not fit to its supertype, a float on integers with
  `strict_cast(Float64)`). A float anchor on a `Float32` column is cast to
  single precision before the lookup (a float list is not). Temporal case
  ids cast to String with their unit's fractional digits.
* **variants.** A null case whose variant is in the list counts toward the
  join's list height, though its key never joins.

Declared orders: the between result joins its segments back without an
order (14 of 180 sampled calls gave another case order over six runs;
each case's rows stay in event order), `filter_on_ncases` keeps the first
cases of a group_by without `maintain_order` on every case dtype, and
`filter_df_relative_occurrence_event_attribute` and
`filter_df_keeping_activ_exc_thresh` print the `is_in` list of a group_by
in its unspecified order. The bank test compares these up to that order.

Round CE fixed the six calls round CD left open (Review CE1): Polars
moves the bounds it merges after the column's unmerged predicates and
nests the chain to the left, a None bound's null column joins the
predicates above it, the stream filters of several columns come in the
order pushdown leaves them, LTL compares a list on temporal, Boolean,
Categorical and Enum columns, and the bank test checks
`filter_df_keeping_spno_activities` ties as a tie set.

## Edge values (round CE)

`scripts/polars_filter_ce_domain.py` extends the typed domain with
Boolean and all-null (`Null` dtype) activity columns, an edge argument
mode (ints past 2**53, at and past the Int64 range up to 2**64 - 1, ints
that do not fit the narrow integer columns, floats past 1e16, NaN, the
infinities, -0.0, datetimes before 1970, bools and unknown strs, alone or
in lists and tuples), and numeric filters (`apply_numeric`,
`apply_numeric_events`) on the activity column with typed and edge
bounds and as stream filter values.
`scripts/capture-polars-filter-bank.py --domain polars_filter_ce_domain.py
--count 800 --bank-seed 2026111436 --hash-seeds 0,1 --repeats 3` captures
`docs/polars-filter-ce-bank.json.gz` (23,212 calls), replayed by
`minera_polars_filter_ce_bank_parity_tests`.

* **Ints past the Int64 range.** A Python int above 2**63 - 1 is an
  Int128 value and makes a list of non-negative ints and bools UInt64
  (any other such list builds strictly from its first item, so a bool
  after it fails). `col(x) OP v` compares it exactly on `UInt64`, casts a
  Boolean column to `UInt64`, a signed column to `Float64`, folds it to a
  constant on a narrower unsigned column, and leaves a `dyn int` that
  matches nothing on temporal and Null columns; String and Categorical
  columns fail naming `u64`. In `is_in` a UInt64 list meets signed
  columns in `Int128` and narrower unsigned columns in `UInt64`, an
  Int128 list meets every integer column in `Int128`, and float columns
  read the values as floats.
* **Null columns.** A comparison casts the column to the literal's type
  (str, bool, `Datetime('μs'[, zone])`), a dynamic int or float stays as
  written; `is_in` casts it to the list's element type, and `==` with a
  list to `List(T)`; a list of lists fails.
* **Boolean columns.** A comparison with an int casts the column to
  `Int32` (`Int64` past its range).
* **Display.** Floats in list literals and build errors print as a frame
  prints them (`1e6`, `9.99999e5`, `1.0000e17`, `9.0072e15`, `0.3`).
* **Before 1970.** A microsecond literal reaches a millisecond column by
  floor division.
* **Merges.** Only comparisons on the bare column merge (a cast operand
  never does); a str literal on an Enum column merges as text whether or
  not it is a category; literals of different classes are unordered and
  keep the first bound; an `==` on a nanosecond column
  (`is_between([v, v + 999 ns])`) merges as the range it is.
* **between.** The separator error casts a case column of any other dtype
  than String (`col("case:concept:name").strict_cast(String)`) and prints
  the anchors resolved, as the plan below the failing select is.
* **Resolution errors.** A failing node prints the leaves before the
  failing one coerced (a None leaf as `null.repeat`) and the leaves after
  it as written (`col > null`, `dyn int: 2`). A scalar `is_in` that
  resolves prints the scalar imploded and cast to the supertype of column
  and value (300 on an `Int8` column:
  `col("x").cast(Int16).is_in([[300].cast(List(Int16))])`; a float on
  integers with `strict_cast(Float64)`).
* **paths.** A list or tuple activity refuses the separator str with
  Python's own message (`can only concatenate list (not "str") to
  list`); in `apply_performance` a None bound after the failing one drops
  from the larger expression, and before it leaves
  `null.repeat([col("@@duration").len()])` in place of the path test.

Declared orders: `filter_traces_intersecting` and
`filter_on_case_performance` keep the cases of a group_by in an
unspecified order (fresh seed 2026111437 case 518 and seed 4247 case 333:
two cases came in one order in all six captured runs, while reruns over
several hash seeds gave both orders).

Outside the domain: Duration, Time, Decimal and Int128 columns (the table
transport has no cell for their values) and ints above the UInt64 range
(the bank's typed JSON carries at most UInt64).

## Scalar values, bounds and argument shapes (round CF)

`scripts/polars_filter_cf_domain.py` extends the CE domain with scalar
values in the attribute filters (None, typed and edge values, a missing
case key), numeric filters on any column with odd and typed bounds and up
to two stream filters, typed, edge, None, empty, nested and tuple anchors
in `between.apply`, edge bounds and typed, list, tuple, dict and None
values in the timestamp filters, and odd path shapes and bounds in the
paths filters. `scripts/capture-polars-filter-bank.py --domain
polars_filter_cf_domain.py --count 800 --bank-seed 2026111460
--hash-seeds 0,1 --repeats 3` captures `docs/polars-filter-cf-bank.json.gz`
(30,328 calls), replayed by `minera_polars_filter_cf_bank_parity_tests`.

* **Float display.** A `dyn float` prints as Rust's Display (no
  exponent); a Float64 literal or cell as Polars' `fmt_float` (`1e7`,
  `9.99999e5`, `1.0000e9`, `0.123457`, `2.5000e-7`).
* **Null columns.** Ordering a list raises `cannot perform '>='
  comparison` with physical list names (`list[i64]` for datetimes,
  `list[i32]` for dates); a timeframe list value casts the column to the
  list's type and keeps no row; a resolved between anchor casts the
  column to the anchor's type (`Int32` for a dynamic int).
* **Merges.** List literals merge only when equal (into `==`); unequal
  lists are unordered and never empty the frame. Equal str or list
  bounds of `paths.apply_performance` merge into one `==`.
* **Wide ints as bounds.** An int past the Int64 range meets the Int64
  duration of the paths and case performance filters at Float64.
* **Timeframe attribute values.** Categorical and Enum columns compare a
  list while collecting (an empty window skips the error); the timestamp
  column as attribute is filtered apart from the window; a dict on a
  String column compares with the struct's String cast (`"{"z","x"}"`,
  `"}"`, `null`).
* **UInt64 columns.** A negative int or a datetime before 1970 meets them
  in Int64, so cells past the Int64 range match nothing.

Declared: Polars panics on a Null activity column with Int64 between
anchors (not compared); datetime arguments outside the nanosecond range
are refused; dict values of the timeframe filter other than str, int and
bool fields on String columns are refused; `filter_on_ncases` may keep k
or k - 1 cases when a null case id is among the k it picks.
