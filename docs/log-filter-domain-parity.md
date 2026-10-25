# EventLog filtering: disjoint public-call bank (2026-10-25)

This checkpoint closes the seven partial EventLog filtering modules

* `algo.filtering.log.attributes.attributes_filter`
* `algo.filtering.log.between.between_filter`
* `algo.filtering.log.cases.case_filter`
* `algo.filtering.log.ltl.ltl_checker`
* `algo.filtering.log.paths.paths_filter`
* `algo.filtering.log.timestamp.timestamp_filter`
* `algo.filtering.log.variants.variants_filter`

with one seeded bank of public calls that is disjoint from the retained banks
of the per-module contracts
([attributes](log-attributes-filter-parity-contract.md),
[between](log-between-parity-contract.md),
[cases](log-case-filter-parity-contract.md),
[LTL](log-ltl-filter-parity-contract.md),
[paths](log-paths-filter-parity-contract.md),
[timestamp](log-timestamp-filter-parity-contract.md),
[variants](log-variants-filter-parity-contract.md)). It ran in the cloud
container on the pinned runtime (Python 3.11, pm4py 2.7.22, pandas 3.0.5,
numpy 2.4.6). Only public functions, signatures and enums were used; no
reference source was read and no retained call was replayed.

## Tooling

* `scripts/log_filter_domain.py` generates the cases. Each case is an XES
  document (0-7 traces, 0-8 events, string/int/float/boolean/date attributes,
  missing labels/timestamps/resources, unsorted timestamps, naive, `Z` and
  `+01:00` dates, trace attributes) and about 40 calls across every public
  function of the seven modules, with their `Parameters` enum members. 40% of
  the cases are "typed": a fifth of the plain arguments and a third of the
  parameter values are replaced by values drawn from the log itself or from
  edge cases (None, NaN, infinities, 2**63, empty and nested sequences,
  datetimes, booleans, unknown keys). Helper arguments that come from other
  modules (variants dictionaries, sorted variant counts, attribute counts,
  path dictionaries) are computed by the public helpers once and stored in the
  call, with traces referenced by index.
* `scripts/capture-log-filter-bank.py` runs every call on a deep copy of the
  imported log in fresh interpreters for `PYTHONHASHSEED` 0-3 with
  `TZ=Europe/Berlin` and writes `docs/log-filter-domain-bank.json.gz`. A call
  whose outcome differs between hash seeds keeps all of them (none did).
* `minera_log_filter_bank_parity_tests`
  (`src/backend/tests/test_log_filter_bank_parity.cpp`) imports each XES
  document with the native importer and dispatches each call to the native
  module API (one function per module). It compares returned logs as trace
  names plus `rd:eid` event ids in order, scalar results as typed JSON (floats
  bit-exact), errors as class and full message, and a mutated source as its
  summary. The ctest registration pins `TZ=Europe/Berlin`.

Typed values travel as `{"s": text}`, `{"i": "decimal"}`, `{"f": "IEEE bits"}`,
`{"b": bool}`, `{"n": null}`, `{"d": "UTC datetime"}`, `{"l": [...]}`,
`{"t": [...]}`, `{"m": [[key, value], ...]}` and `{"x": [trace indices]}`.

## Final bank

Bank seed 20261025, 6,000 cases, hash seeds 0-3:

| Calls | Returned logs | Scalar values | Errors | Varying across seeds | Failures |
| --- | --- | --- | --- | --- | --- |
| 240,450 | 166,454 | 37,276 | 36,720 | 0 | 0 |

SHA-256 of the bank: `3b880804bc7e22e7454d08d8cee4a56396e91eeabd4787df58059f5bc95a8467`.
Pilots with bank seeds 7, 11, 12, 13 and 14 drove the fixes below and are not
part of the evidence. The seven retained per-module suites
(`test_log_{attributes,between,case,ltl,paths,timestamp,variants}_filter.cpp`,
compiled ad hoc against the same core) still pass in full after the fixes.

## Behaviour established (native changes)

### attributes

* `filter_log_on_max_no_activities` reads the activity key only under its
  plain string name `pm4py:param:activity_key`; an `ACTIVITY_KEY` enum member
  is ignored and so is `keep_once_per_case`. The counted key is written into
  the parameters as the string attribute key, so an `ATTRIBUTE_KEY` enum member
  still decides which events the final event filter keeps.
* The same function compares `maximum < number of activities` first, then
  raises the slice-index TypeError for a non-integer maximum, then returns the
  log unchanged when no event has an activity value (even for negative maxima).
* Negative `apply_numeric` / `apply_numeric_events` keep `v < lo or v > hi`
  (so NaN is never kept and the error names `<`/`>` with the event value
  first); positive keeps `lo <= v <= hi`.
* `apply_numeric` builds the stream case key as Python's
  `case_id_glue + case_id_key` at the first matching event (TypeError for
  str + non-str, numeric sums, so glue 5 and key -4 look up event key 1), and
  looks the trace key up on every trace, empty traces included.
* `filter_log_relative_occurrence_event_attribute` multiplies the threshold
  only when some value was counted.
* `filter_log_by_attributes_threshold` takes the protected values from the
  protected trace's events that have the key (others are skipped) and checks
  protection before comparing a count with the threshold.

### cases

* `filter_on_case_size` without a maximum tests `len >= minimum`; with one it
  tests the chained `minimum <= len <= maximum`, which names the operands of a
  failing comparison the other way round.

### ltl

* `eventually_follows` never reads the pattern of a log without traces.
* Timestamp boundaries are checked per position: the k-th difference (from the
  first trace events by index, as before) is compared with `bounds[k][0]` by
  `<` and then with `bounds[k][1]` by `>`; the first failure stops before later
  timestamps or bounds are read (so missing later timestamps and short bound
  lists raise only when reached).

### paths

* `filter_log_by_paths` requires the key on the protected trace's events
  (KeyError) but skips source pairs without it, and checks protection before
  the threshold comparison.
* `get_paths_threshold` multiplies by a list or tuple factor as Python
  sequence repetition, so the error is the following `>` comparison.

### between

* A nonstring `CASE_ID_KEY` is stored by the native typed result; the bank
  reads the string-key projection, whose trace names are unchanged.

The timestamp and variants modules needed no change.

## Contract

The seven modules are complete on this bank plus their retained contracts.
Values the XES importer cannot produce (arbitrary Python objects, pandas
extension scalars inside logs) stay covered only by the retained per-module
evidence and the native typed carriers documented there.
