# Pandas sequence filters: public-call bank (2026-10-25)

This checkpoint closes twelve partial pandas filter modules

* `algo.filtering.pandas.pd_filtering_constants`
* `algo.filtering.pandas.start_activities.start_activities_filter`
* `algo.filtering.pandas.end_activities.end_activities_filter`
* `algo.filtering.pandas.traces.trace_filter`
* `algo.filtering.pandas.starts_with.starts_with_filter`
* `algo.filtering.pandas.ends_with.ends_with_filter`
* `algo.filtering.pandas.rework.rework_filter`
* `algo.filtering.pandas.prefixes.prefix_filter`
* `algo.filtering.pandas.suffixes.suffix_filter`
* `algo.filtering.pandas.consecutive_act_case_grouping.consecutive_act_case_grouping_filter`
* `algo.filtering.pandas.timestamp_case_grouping.timestamp_case_grouping_filter`
* `algo.filtering.pandas.activity_split.activity_split_filter`

with one seeded bank of public calls. It ran in the cloud container on the
pinned runtime (Python 3.11.17, pm4py 2.7.22, pandas 3.0.5, numpy 2.4.6). Only
public functions, signatures and observed results were used. No reference
source was read. The arrays that numpy's sort received were observed only to
fix the comparison order described under activity_split.

## Native API

`include/minera/pandas_sequence_filters.hpp` and
`src/backend/pandas_sequence_filters.cpp` hold the constants and one function
per public callable over the native event table (`EventTable`). Python
arguments are `PandasFilterValue`s and parameter dictionaries are
`PandasFilterParameters`, whose entries are a Parameters member or a plain
key. Public failures raise `PandasSequenceFilterError` with the Python class
name and message.

## Tooling

* `scripts/pandas_sequence_filter_domain.py` generates the cases. Each case is
  one DataFrame:
  * 0-14 rows over 1-4 cases. Case ids are `str` (with an occasional null) or
    `int64`.
  * Activities come from a small pool plus odd names (empty, `ab`, `a,b`, `é`,
    `...`), with occasional nulls.
  * Timestamps are UTC-aware, with repeated and out-of-order hours.
  * There is an optional float column with nulls.
  * The index is a RangeIndex, a shuffled permutation, or random labels with
    duplicates, and the rows are sometimes shuffled.
  * In 15% of cases the columns are named `cid`/`act`/`ts`.

  Calls cover every function with Parameters members, plain keys, ignored
  uppercase keys, missing columns, and non-boolean `positive`, `strict` and
  `min_occurrences` values. They also cover unknown `first_or_last`,
  `filter_type` and `cut_mode` values, count dictionaries,
  `most_common_variant`, and string and list patterns with `...` wildcards.
* `scripts/capture-pandas-sequence-filter-bank.py` runs every call on a fresh
  copy of the frame, in fresh interpreters for `PYTHONHASHSEED` 0-3 with
  `TZ=Europe/Berlin`, and writes `docs/pandas-sequence-filter-bank.json.gz`.
  Each case stores its source frame. A call whose outcome differs between
  hash seeds keeps all the outcomes it showed.
* `minera_pandas_sequence_filter_bank_parity_tests`
  (`src/backend/tests/test_pandas_sequence_filter_bank_parity.cpp`) replays
  each call through the native API, with one function per module. Tables
  compare through the normalized event-table JSON. That covers column order,
  dtypes, the index class and labels, and every cell with its kind.

## Final bank

Bank seed 20261027, 3,000 cases, hash seeds 0-3:

| Calls | Values | Errors | Source frame changed | Varying across seeds | Failures |
| --- | --- | --- | --- | --- | --- |
| 33,820 | 27,594 | 6,226 | 0 | 3 | 0 |

There are 2,369 calls per function, except `activity_split.apply` with 631.
The three varying calls are `rework` KeyErrors. Their list of missing columns
follows set order, and the test accepts any observed outcome.

SHA-256 of the bank: `b2088b0b3b8cd460c39d2cf530ecee594ff154d6fcecc69cafcae1f3604f7cbe`.
Pilots with bank seed 7 drove the fixes and are not part of the evidence.

## Behaviour established

### Parameters and errors

* A Parameters member key is read before the plain string equal to its value.
  Uppercase strings are ignored.
* A missing case column raises `KeyError 'X'`, and it is checked first. A
  missing activity column in the grouped functions raises
  `KeyError 'Column not found: X'`.
* Row selections keep the source index labels. A RangeIndex stays one only
  while the kept labels form an arithmetic progression, for any nonzero step.

### start_activities and end_activities

* A case's boundary activity is its first or last non-null activity in row
  order. Membership counts null as matching `None`.
* `positive` is evaluated for truthiness. Rows with a null case id are kept
  only by negative filters.
* nocc:
  * Counts follow the first appearance of each case, or come from
    `sa_count0`/`ea_count0`.
  * The filter keeps activities counted at least `nocc` times, plus the last
    activity of `most_common_variant`.
  * If every counted activity is kept, the frame is returned unchanged.
  * `return_dict` returns `(frame, kept counts)`. An empty frame comes back
    alone.

### traces, starts_with and ends_with

* Variants are each case's activities joined by `,`, in first-appearance
  order of the cases. A null activity raises the `str.join` TypeError for the
  first such case.
* Patterns are joined by `,` too, so a string pattern is joined character by
  character.
* `starts_with` and `ends_with` use string prefix and suffix tests.
* `trace_filter` matches segment by segment, where `...` stands for any run
  of activities. An empty admitted list keeps every non-null case, but the
  variants are still built, so their errors are still raised.

### rework, prefixes and suffixes

* rework:
  * Selecting `[activity, case]` comes first, with pandas' "not in index"
    KeyErrors.
  * Then `min_occurrences - 1` is computed, with Python's operand TypeError.
  * A case is kept when some per-case occurrence index of the activity
    reaches `min_occurrences - 1`.
* prefixes and suffixes:
  * Both add `@@index_in_trace` (the position in the case) and `@@temp_column`
    (the first or last occurrence). Each is float64 when nulls appear.
  * They keep rows before or after that occurrence, strictly or not.
  * An unknown `first_or_last` raises
    `AttributeError 'DataFrameGroupBy' object has no attribute 'reset_index'`.

### Grouping filters

* consecutive_act_case_grouping:
  * It merges adjacent rows whose case and activity concatenate to the same
    key. An int case with a str activity raises the `+` TypeError.
  * Each run keeps the first or last non-null value per column, and the index
    is reset. An unknown filter type returns the reset frame.
* timestamp_case_grouping:
  * It merges the rows of a (case, timestamp) group in first-appearance order,
    dropping rows with a null key. The columns come out as
    `[case, timestamp, rest]`.
  * An unknown filter type raises
    `AttributeError 'NoneType' object has no attribute 'reset_index'`.
  * On a two-row frame where a key is not a column, pandas groups by the key
    list itself: row *i* joins the group named by key *i*, so equal keys merge
    both rows. The labels come back as a leading `index` column and nothing
    raises.

### activity_split

* The new case id is `str(case) + SUBCASE_CONCAT_STR + count`. Here `count`
  is the number of cut activities met so far in the case, and CUT_MODE
  `this` also counts the current row.
* A null case id gives `nan` and its own single-row group. The column dtype
  becomes `str`, or float64 for an empty frame.
* A `str` case column that holds both ids and nulls raises Python's `<`
  TypeError from numpy's stable object argsort. The operand order of the
  first id/null comparison decides the message. The native code replays that
  sort's comparison order, which matches numpy for up to 16 values. Longer
  mixed columns are outside the bank.

## Contract and domain limits

All twelve modules are complete on this bank. The following are outside the
domain:

* `activity_split` on frames whose cases interleave. The reference's result
  is not row-aligned there. (The mining filters on XES uploads model that
  result since Review BI1: the cases are labelled in sorted id order, each
  from its slice of the frame's rows, and the labels go to the rows in row
  order. This bank's companion does not.)
* Patterns with two adjacent `...` wildcards.
* Non-string column labels.
* `grouped_df` arguments other than None.
* Float case columns with nulls.
