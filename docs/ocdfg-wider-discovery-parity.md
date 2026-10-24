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

`docs/ocdfg-wider-discovery-evidence.json.gz` holds the full bank, the native
report, the hashes of the three scripts and the build/test receipts.

## Still open

This bank uses string identifiers, one timestamp offset per log and canonical
column names. Mixed offsets in one column, nanosecond-resolution columns,
non-string or missing identifiers, arbitrary relation cell dtypes, holiday
calendars and performance benchmarking remain separate gates, as do the
importer timezone behavior above and rendered UI checks. No coverage row is
promoted.
