# OCDFG wider discovery and timing gate

Target: PM4Py 2.7.22 at `0715c31f3f1d85924107d6aa5329806db69df899`.
This increment widens the OCDFG discovery gate beyond the retained 549-fixture
incidence checkpoint and the 77-call timing bank. It uses only new, disjoint
inputs. The whole OCDFG module stays partial.

## Bank

`scripts/ocdfg_wider_fixtures.py` builds 53 OCELs from fixed seeds: 4,285
events, 863 objects and 8,384 event/object relations. Every identifier has a
`w` prefix and every timestamp lies in 2029-2032, so no input overlaps the
January 2026 incidence matrices or the February 2027 timing bank.

- 24 UTC logs with 20-150 events, two to four object types, lifetimes over
  about four months, 18% tied timestamps, microsecond fractions, duplicate
  relations with a second qualifier, events without objects and shuffled
  table rows in two thirds of the logs.
- Eight logs each at a fixed `+05:30` offset, a fixed `-08:00` offset and
  naive timestamps, half of them with shuffled rows.
- Four long-lifetime logs with 200-320 events on two to four objects over
  about two and a half years, crossing the 2032 leap day.
- One hand-built log: Friday-to-Monday working-time boundaries, tied events
  of one activity, an event without objects, unused objects, a single-event
  object type, a repeated relation that creates a self-flow, Unicode labels
  and an activity name containing `->`.

Each log runs under five public timing profiles: elapsed time, the default
working week, split/fractional slots including a weekend slot,
overlapping/unsorted slots and an explicitly empty slot list.

`scripts/capture-ocdfg-wider-reference.py` builds each OCEL directly from
event, object and relation tables through the public constructor and calls
only `pm4py.discover_ocdfg`. Building from tables keeps the importer out of
this gate: the public OCEL 2.0 JSON reader replaces fixed offsets with UTC
while keeping wall-clock fields, which is importer behavior, not OCDFG
behavior. The capture runs in four processes with `PYTHONHASHSEED` 0-3. All
265 outcomes are identical across seeds, including the order of every
performance list. No call raised an error. Every one of the 13,330
multi-sample performance lists is in ascending order, which matches the
native sort.

## Native result

`scripts/check-ocdfg-wider-native.py` rebuilds the same fixtures, checks the
fixture digest bound in the bank, writes standard OCEL 2.0 JSON and runs the
compiled `minera_ocel --discover ocdfg` for every profile. It compares
activities, object types, independent/per-type/start/end activity metrics
(event and unique-object sets, ordered total-object lists with multiplicity)
and every edge's event couples, unique objects and total-object triples. Each
of the 72,100 performance samples is compared bitwise and in order.

The CLI now accepts the HTTP route's timing controls:
`--ocdfg-business-hours`, `--ocdfg-business-hour-slots-json`,
`--ocdfg-event-id-attribute`, `--ocdfg-timestamp-attribute` and
`--ocdfg-workcalendar-json`. Both entry points use
`parse_ocdfg_performance_controls`, so absence and literal empty values keep
the same meaning. Each option may be given once and only applies with
`--discover ocdfg`.

The first comparison matched all 265 typed-timing outcomes. The 53 elapsed
outcomes on the default path, with no timing controls, did not match: every
mismatch was a performance sample off by less than one millisecond. That path
subtracted two binary64 epoch values, which cannot hold microseconds at
present-day magnitudes. It now subtracts exact parsed seconds and
nanoseconds, flooring a negative submicrosecond remainder as the typed path
does. Timestamp shapes the exact parser rejects keep the old double
projection, and missing timestamps are still skipped. After the repair, all
318 native outcomes match: 265 typed-timing outcomes and 53 default-path
outcomes.

`docs/ocdfg-wider-discovery-evidence.json.gz` holds both banks, both native
reports, the reader observations, the script and source hashes and the
OCEL/OCDFG test log.

## Second bank: offsets, nanoseconds and integer IDs

`fixtures_round2()` adds 28 more disjoint logs (1,822 events, `x` prefixes or
integers above 800,000, timestamps in 2033-2034) under the same five
profiles, for 140 public outcomes. All were identical across hash seeds 0-3
and none raised an error.

- Ten logs keep one timestamp column with per-value offsets (`Z`, `+02:00`,
  `-05:00`, `+05:30`, `+12:45`, `-09:30`), so the column has object dtype.
  Elapsed time uses the real instant, and working time uses each value's own
  local wall clock. Native already matched both.
- Ten logs use nanosecond columns with remainders of 1, 999, 1,000, 1,001 ns
  and random values, including ties that differ only below a microsecond.
  Differences floor to whole microseconds.
- Eight logs use integer event and object IDs. Native IDs are text, so the
  check compares the reference IDs as decimal text and re-sorts sets.
  Ordered association lists keep their reference order. A column that mixes
  `1` and `"1"` would collide in the text carrier and is outside this
  contract.

Four nanosecond outcomes first failed by one ulp. The public elapsed value is
not the correctly rounded quotient of microseconds by 10^6. It is the sum of
whole seconds and the microsecond fraction, after flooring to microseconds:
-0.519789 s comes out as `-1 + 0.480211`, which is -0.5197890000000001. A
black-box sweep of 200,000 random intervals in each of the ns, us, ms and s
units agreed with that sum every time. The first bank never hit a case where
the two roundings differ. `ocdfg_elapsed_seconds` now does this sum, and both
the typed-timing path and the default path use it. All 168 native outcomes of
the second bank and all 318 of the first match, the 60 retained timing cells
still pass, and a new regression case pins the -0.519789 s and -1 us values.

## Calendars and the OCEL 2.0 JSON reader

`pm4py.discover_ocdfg` takes only `business_hours` and
`business_hour_slots`, and the discovery variant exposes only the activity,
object-type and compute-performance parameters. No holiday calendar reaches
discovery. The earlier timing bank already showed that calendar arguments to
the edge-performance helper are inert, and native keeps that.

`scripts/capture-ocel2-json-offset-reference.py` records what the public OCEL
2.0 JSON reader does with offsets. It drops the offset, keeps the wall-clock
fields, truncates to microseconds and labels the result UTC. Mixed-offset
input therefore loses real elapsed time: `16:30+02:00` to `08:30-05:00` three
days later reads as 230,400 s, not 255,600 s. Minera's reader keeps the real
instant, which every other algorithm relies on. The reader is outside the
`algo` catalogue this contract covers, so the difference is recorded, not
copied.

## Still open

Mixed `int`/`str` ID columns, missing identifiers, arbitrary relation cell
dtypes, the reader difference above, performance benchmarking and rendered
UI checks remain open. No coverage row is promoted.
