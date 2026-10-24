# Attribute-value repetition: disjoint domain banks (2026-10-24)

This checkpoint continues the three repetition contracts
([log](log-attribute-repetition-parity-contract.md),
[pandas](pandas-attribute-repetition-parity-contract.md),
[Polars](polars-attribute-repetition-parity-contract.md)) with seeded,
disjoint public banks over the column/value domains those contracts left open.
It ran in the cloud container on the pinned runtime (Python 3.11, pm4py
2.7.22, pandas 3.0.5, numpy 2.4.6, polars 1.44.1). No retained `build/` bank
was used or replayed; every generated label carries an `rd-` prefix.

## Tooling

* `scripts/repetition_domain.py` generates cases and the native transport:
  complete event-table JSON (exact integer storage, nanosecond datetimes,
  zones, categorical dictionaries) for pandas/Polars, and XES documents for
  logs. Targets travel as typed target JSON; a datetime target from a named
  zone carries that zone (`"timezone"`), which the Polars literal needs.
* `scripts/capture-repetition-domain-bank.py --engine pandas|polars|log`
  runs every call in a fresh interpreter per `PYTHONHASHSEED`, rebuilding the
  input for each call, and writes a deterministic gzip bank.
* `scripts/check-repetition-domain-bank.py BANK --native minera_repetition_batch`
  sends each representable call through the same workbench functions as the
  table repetition route and the log filter route, without the reference
  runtime. Tables compare columns, dtypes, retained source rows in order,
  pandas index class/names, unchanged categorical declarations, source
  mutation and complete errors (Polars also the apply/collect phase). Logs
  compare the retained case names in order, source mutation and the error
  message. Where public outcomes differ between hash seeds, native must equal
  one of them.

## Final banks

Bank seed 20261024, hash seeds 0-4, 6,000 generated cases per engine:

| Engine | Calls checked | Results | Errors | Varying across seeds | Declared rejections | Failures |
| --- | --- | --- | --- | --- | --- | --- |
| Polars | 23,701 | 12,686 | 11,015 | 9 | 0 | 0 |
| pandas | 28,274 | 22,464 | 5,810 | 0 | 326 | 0 |
| log | 18,433 | 16,221 | 2,212 | 0 | 0 | 0 |

Unrepresentable calls (calendar `date` and `NaT` targets, which the target
JSON has no type for) are counted and skipped: 2,074 Polars, 44
pandas. The nine varying Polars calls differ only in join row order. Banks
and hashes: `docs/attribute-repetition-domain-{pandas,polars,log}-bank.json.gz`
and `docs/attribute-repetition-domain-validation.json`. Smaller pilots with
bank seeds 7, 11 and 12 drove the fixes below and are not part of the
evidence.

## Behavior established

### Polars

* Columns: Int8-64, UInt8-64, Float32/64, Boolean, String, Categorical, Enum,
  Null, Date and Datetime in ms/us/ns, naive, UTC or named zones.
* Literal model: Python ints become i32/i64/u64/i128 literals; beyond i128
  `apply` raises `InvalidOperationError` ("integer too large for Polars").
  Integer lists are `List(Int64)`. A fixed-offset datetime is a UTC literal,
  a named-zone datetime keeps its zone, and nanoseconds are floored to
  microseconds.
* Datetime columns compare only with a literal of the same zone (or both
  naive); otherwise `SchemaError`. Plans render the literal as wall time in
  its zone plus the zone abbreviation (`CET`, `IST`, `UTC`), ms columns floor
  the literal and ns columns use `is_between([t, t+999ns])`.
* Bounds: integers (and booleans) outside the u32 count range simplify to
  `when(count.is_not_null()).then(true|false)`, floats compare
  `count.cast(Float64)`, and a `None` minimum appears as
  `null.repeat(count.len())` with the operands swapped. When both bounds are
  in-range integers with minimum above maximum, the optimizer folds the
  counted side to an empty frame: the result is empty and comparison errors
  are never raised (plan and missing-column errors still are).
* A one-row frame whose case key is NaN joins nothing; NaN keys join in
  larger frames.

### pandas

* Typed numeric columns compare like NumPy 2 with a weak Python scalar:
  integers exactly, integer columns against floats in float64, float32
  columns in float32 (so `0.1` and `16777217` match their float32 cells),
  float64 columns against integers in float64. Nullable extension dtypes
  behave the same.
* A row-aligned list becomes a NumPy array first: any float makes every
  element compare in float64; integer arrays stay exact against integer
  columns; lists with other values are object arrays compared per element.
* Categorical columns compare like their categories: numeric categories never
  equal booleans, boolean categories equal only booleans, datetime categories
  parse string targets.
* Datetime columns localize naive ISO strings to the column zone (named or
  fixed); ambiguous or nonexistent wall times match nothing.

### Log

* Sources are XES documents read by the public default importer (native:
  `import_xes_event_log`). A scalar attribute with nested elements is the
  public mapping `{'value': v, 'children': {...}}`, with later duplicate keys
  replacing earlier ones, or `{'value': v, 'children': [(k, v), ...]}` for
  `<values>` children; nested wrappers recurse. Native now compares such
  cells through that view instead of rejecting them.

## Declared limits

* Explicit native rejections (never silent): Polars mixed-type, set or map
  targets, non-64-bit integers inside lists, pandas NA targets and integers
  beyond 128 bits inside lists; pandas tuple/set/map targets on typed columns
  and non-ISO datetime strings (the public side parses free text with
  dateutil, including "now"). The final pandas bank hit them 326 times
  (269 non-list containers on typed columns, 57 free-form datetime strings);
  the public outcomes stay in the bank.
* Table cells hold only the scalar kinds the native table carrier transports.
  Arbitrary Python objects with custom equality, Polars List/Struct/Decimal/
  Duration/Time columns and pandas period/interval/sparse dtypes are not
  representable and are rejected by the carrier or the companion.
* The log row stays partial. A public XES `<list>` without `<values>` is
  `{'value': None, 'children': {...}}`, but natively it is the same
  `kContainer` that ordinary Python logs use for a plain dict, so the two
  public meanings share one native representation and native compares it as
  a plain mapping. The bank does not generate `<list>`/`<container>`
  elements for that reason. Custom-equality objects and nonstring dictionary
  keys also remain outside the log carrier.

## Registered proofs

`minera_attribute_repetition_parity_contracts_tests`
(`src/backend/tests/test_attribute_repetition_parity_contracts.cpp`) pins each
rule above with a focused case. With these banks, `algo.filtering.pandas.
attr_value_repetition.filter` and `algo.filtering.polars.attr_value_repetition.
filter` are promoted; the log module remains partial.
