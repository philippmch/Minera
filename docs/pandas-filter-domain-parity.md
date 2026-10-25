# Pandas filters: public-call bank (2026-10-25)

This checkpoint closes the seven remaining partial pandas filter modules

* `algo.filtering.pandas.attributes.attributes_filter`
* `algo.filtering.pandas.between.between_filter`
* `algo.filtering.pandas.cases.case_filter`
* `algo.filtering.pandas.ltl.ltl_checker`
* `algo.filtering.pandas.paths.paths_filter`
* `algo.filtering.pandas.timestamp.timestamp_filter`
* `algo.filtering.pandas.variants.variants_filter`

with one seeded bank of public calls. It ran in the cloud container on the
pinned runtime (Python 3.11.17, pm4py 2.7.22, pandas 3.0.5, numpy 2.4.6). Only
public functions, signatures and observed results were used. No reference
source was read. The earlier per-module contracts
(`docs/pandas-*-parity-contract.md`) stay as history; their suites still pass.

## Native API

Each module keeps its header (`include/minera/pandas_<module>_filter.hpp`) and
source. The functions take the native event table (`EventTable`), Python
arguments as `EventAttribute`s, and parameter dictionaries as entries keyed by
a Parameters member or a plain key. Failures raise the module's error type
with the Python class name and message. Shared emulation helpers live in
`src/backend/pandas_parameter_carrier.hpp` (the `parameters` argument),
`src/backend/numpy_object_sort.hpp` (numpy's object argsort) and
`src/backend/python_list_sort.hpp` (CPython's `list.sort`).

## Tooling

* `scripts/pandas_filter_domain.py` generates the cases. Each case is one
  DataFrame:
  * 0-14 rows over 1-4 cases. Case ids are `str` or `int64`, with occasional
    nulls.
  * Activities come from a small pool with occasional nulls. Timestamps are
    UTC-aware, with repeated and out-of-order hours and occasional `NaT`.
  * `org:resource` has nulls. `rd:amount` is `int64` or `float64` with `NaN`.
    There are also `rd:kind` and `rd` columns.
  * In 12% of cases the columns are named `cid`/`act`/`ts`.
  * The index is a RangeIndex, a shuffled permutation, or random labels with
    duplicates, and the rows are sometimes shuffled.

  Calls cover all 28 public functions, about 29 per case. They use Parameters
  members, plain keys, ignored uppercase keys and missing columns. Arguments
  include wild values: None, lists, tuples, dicts, sets, strings, Python
  datetimes with and without offsets, and non-boolean flags.
* `scripts/capture-pandas-filter-bank.py` runs every call on a fresh copy of
  the frame, in fresh interpreters for `PYTHONHASHSEED` 0-3 with
  `TZ=Europe/Berlin`, and writes `docs/pandas-filter-bank.json.gz`. A call
  whose outcome differs between hash seeds keeps all the outcomes it showed.
* `minera_pandas_filter_bank_parity_tests`
  (`src/backend/tests/test_pandas_filter_bank_parity.cpp`) replays each call
  through the native API, with one function per module. Tables compare
  through the normalized event-table JSON.

## Final bank

Bank seed 20261028, 3,000 cases, hash seeds 0-3:

| Calls | Values | Errors | Source frame changed | Varying across seeds | Failures |
| --- | --- | --- | --- | --- | --- |
| 87,971 | 69,103 | 18,868 | 0 | 0 | 0 |

By module: attributes 24,000, timestamp 18,477, cases 12,494, ltl 12,000,
variants 12,000, paths 6,000 and between 3,000 calls.

SHA-256 of the bank: `253c1522b240bf9d75f32dede3ccf7d6d0c358e393c8b29f529188be27d40f1b`.
The capture takes about 20 minutes with four jobs.

Pilots with bank seeds 7 and 9 (300 cases each) drove the fixes and are not
part of the evidence. Both still pass.

## Behaviour established

### Parameters and keys

* `parameters` follows the carrier rules of `exec_utils.get_param_value`:
  * None, an empty list, tuple, dict or set, or any dict, gives the defaults.
  * Other lists and tuples index every element. A non-int element raises
    `list/tuple indices must be integers or slices, not T`, and an
    out-of-range int raises `IndexError`.
  * A non-empty set is not subscriptable. `''` raises the `'in <string>'`
    TypeError, and any other str raises `string indices must be integers`.
  * Any other scalar is "not iterable".
* A list key selects columns. When none of them exists, it raises
  `None of [Index([...], dtype=...)] are in the [columns]`. When only some
  exist, it raises `[...] not in index`. A Python datetime key raises a
  KeyError with its `datetime.datetime(...)` repr.
* Python datetime arguments are named `datetime` by `__name__` messages and
  `datetime.datetime` by operator errors. Frame cells are `Timestamp`.

### attributes

* `isin` on a `str` column lets a null cell match a `None` or `NaN` value.
* Numeric bounds:
  * A tuple bound is an array operand. On a `str` column it needs one item
    per row. On other columns one item broadcasts and other lengths must
    match, and a one-row frame reports the assignment length.
  * Negative event filtering keeps `(v < lo) | (v > hi)`.
* A list bound needs one item per row and compares item by item. A str bound
  fails on a non-text column even when the frame is empty.
* Stream filters apply in turn to the rows that remain. A list value needs
  one item per remaining row, and a list key keeps no row.
* An empty-list attribute key masks the whole frame. Event filters return
  it, and case filters keep the cases whose id is null. An empty frame keeps
  its dtypes.
* Relative occurrence:
  * An empty frame only resolves a scalar attribute column.
  * With `keep_once_per_case`, the case column is resolved first, except on a
    two-row frame (the pandas key-list quirk). Null cases are not counted, a
    None key is "not callable", and a list key is one grouper array.
  * The threshold is `stake * count`, so only numbers and sequences pass
    that step. A non-numeric stake fails at the first comparison, which
    never happens when no value is counted.

### between

* The case ids go through numpy's object sort. The first failing comparison
  names the operands.
* The activity column is resolved before that sort, and every event is
  tested against `act2` before `act1`.

### cases

* `filter_on_case_size` masks by `size >= min` and then by `size <= max`.
  The second mask is aligned to the kept rows by label, so a duplicated label
  takes every row that carries it. The mask then applies by position, and the
  frame is unchanged when it is all True. Out-of-range positions raise
  `indices are out-of-bounds`.
* A non-empty frame whose case ids are all null raises
  `cannot do a non-empty take from an empty axes.`.
* `filter_case_performance` compares the case durations in sorted case
  order. The maximum is applied first and the minimum only to the cases that
  remain, so a bad minimum can go unchecked. Lists compare item by item, and
  tuples broadcast like numpy operands.

### ltl

* Eventually-follows timestamp boundaries are checked for each step before
  any row is matched. A short boundary list raises the `IndexError`, and a
  non-numeric bound raises `Invalid comparison between dtype=float64 and T`.
* Column keys that collide raise pandas' duplicate-label errors. The
  eventually-follows pattern is iterated only after the columns are selected.
* Vector targets are compared against every row, including rows dropped
  later for null keys.
* `four_eyes_principle` compares the A-event resources with the resource of
  the case's last B event.
* A tuple target on a `str` column needs one item per row
  (`Lengths of operands do not match`).

### paths

* A tuple performance bound broadcasts against the durations and is then
  assigned to the path occurrences, which reports `Length of values`.
* A Python datetime bound fails only when some path occurs.
* Path occurrences without a duration (NaT) are dropped before the bounds
  apply. A list bound needs one item per remaining occurrence.

### timestamp

* With `keep_nan_values`, `filter_traces_attribute_in_timeframe` also accepts
  the cases of matching rows that have no timestamp.

### variants

* The count filters read the plain `pm4py:param:timestamp_key`. That column
  must exist, and a missing one raises a KeyError. The 2026-10-23 contract
  (`docs/pandas-variants-filter-parity-contract.md`) recorded the timestamp
  selector as inert. That does not reproduce on the pinned runtime: pandas
  3.0.5 and 3.0.6, the site-packages install and the pinned checkout all raise
  the KeyError. Six retained cases in `test_pandas_variants_filter.cpp` (an
  absent `time:timestamp` column and the explicit missing selector, for each
  count filter) now expect it. The retained `top_k` activity-NaN case now
  expects `'float' and 'str'`, which is what the pinned runtime raises on that
  frame.
* `filter_variants_top_k` ranks with `sorted(..., reverse=True)` on
  `(count, variant)` before it reads k. CPython's comparison order decides
  which operands a str/NaN error names.
* With no non-null case, `apply` selects nothing, so a negative filter keeps
  every row.

## Contract and domain limits

All seven modules are complete on this bank. The following are outside the
domain:

* Tuple bounds and targets in `cases`, `paths` and `ltl` whose length equals
  the row count. They raise a native domain error.
* Non-string column labels.
* `grouped_df`/`variants_df` helpers and business-hour case durations.
* Mixed-type object columns longer than 16 values, where the replayed numpy
  sort order is not exact.
