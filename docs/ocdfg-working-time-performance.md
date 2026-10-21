# OCDFG working-time performance increment

Target: PM4Py 2.7.22 at `0715c31f3f1d85924107d6aa5329806db69df899`.
The whole OCDFG module/performance gate remains open. This work delivers the
working-time calculation and typed direct timing seam; it does not replace
the earlier 549-fixture real-OCEL checkpoint or its schema/order obligations.

Root executed one new 77-call bank, retained at
`build/parity-closures/ocdfg-performance-public-actual-1/capture.json` (SHA256
`2530a248aeaffe22dc4fab303951233a8794f4c176d8ae7ab8f2393cd1ab0d90`).
It contains 59 returned values and 18 captured errors, with unchanged input
tables, parameters and supplied aggregation collections in every row. The
prior January 2026 discovery/self-comparison fixture was not replayed. New
February 2027 inputs have nine event pairs spanning daily, overnight and
weekly boundaries, repeated/shared-object associations, reversed and zero
intervals, and subsecond timings.

The new direct performance slice has 60 observed cells: 48 numeric returns
and twelve errors. Both event-couple and total-object projections are tested.
Every returned timing is retained as its binary64 hex value. Eight additional
full discovery outcomes/errors cover elapsed/default/split working time,
custom selectors, a missing timestamp selector, disabled performance and both
NaT branches. The other nine rows retain aggregation and strict
association-selector observations for subsequent review.

Observed controls:

- Elapsed time is signed UTC duration. Missing/NaT pairs yield zero when
  business hours are disabled.
- Working time uses local wall-clock fields: timezone offsets do not shift
  the weekly schedule. Naive timestamps and aware timestamps are supported.
- Omitted slots use Monday-Friday 07:00-17:00; explicit empty slots produce
  zero. Split, overlapping/unsorted, full-week and fractional slots retain
  the captured arithmetic.
- Reversed and outside-week slot endpoints are accepted by the reference.
  Their weekly contribution can be negative. The dedicated OCDFG slot
  normalization preserves that behavior instead of applying the stricter
  schedule validator used by other native algorithms.
- Explicit null slots raise `TypeError`; the captured malformed string slot
  raises `ValueError`. Slots and calendars are inactive when business hours
  are disabled.
- Calendar None, false, string and dictionary arguments all produce the same
  returned values. The native option retains their JSON representation but
  does not invent holiday callbacks or a third-party calendar protocol.
- NaT with business hours raises `ValueError`; literal text datetime cells
  raise `TypeError`. The direct native row carrier distinguishes datetime,
  missing and text cells. Native OCEL's primary timestamp uses datetime
  semantics, while literal custom/empty timestamp selectors are supported.

`OcdfgEdgePerformanceOptions` and `calculate_ocdfg_edge_performance` live in
independent new files. Numeric slot values reuse the existing exact weekly
wall-time arithmetic; elapsed differences are computed before binary64
conversion. The `Ocel` overload validates selected event-ID and timestamp
columns, retaining absent versus explicitly empty keys. Generic strings are
not used as a substitute for typed datetime cells by the direct row API.

`generate-ocdfg-performance-tests.py` constructs all sixty timing vectors
offline from the retained receipt. The assertion-enabled native test compares
each numeric result bitwise and each captured error type/message, and also
exercises literal schema selectors through the native OCEL adapter. A direct
practical case verifies Friday 16:30 to Monday 08:30 as 230,400 elapsed seconds
or 7,200 business seconds, plus explicit empty/inactive schedules.

An independent offline mathematical check matched all 48 successful timing
vectors. The assertion-enabled `minera_ocdfg_edge_performance_tests` now passes
all sixty timing outcomes and eight complete discovery outcomes/errors, with
log `build/parity-closures/native-current/ctest-workingtime-first.log`.
The optional `OcdfgDiscoveryOptions::edge_performance_options` activates this
timing seam for both event-couple and total-object projections. An omitted
member preserves the earlier discovery path; `compute_edges_performance=false`
keeps timing controls inactive. Practical native acceptance checks both cases
and verifies source preservation.

The scoped HTTP query controls are
`ocdfg_business_hours`, `ocdfg_business_hour_slots_json`,
`ocdfg_event_id_attribute`, `ocdfg_timestamp_attribute` and
`ocdfg_workcalendar_json`. The slot carrier is a JSON array of numeric
`[start,end]` pairs, including fractional, reversed and outside-week values;
`[]` explicitly disables all working intervals and `null` preserves the
captured native error when working hours are active. Omission uses the default
weekly schedule. Attribute selectors preserve literal empty names. The
calendar carrier is valid JSON retained as an inert parameter. Supplying any
one of these controls opts discovery into the new timing path.

The assertion-enabled `minera_ocdfg_performance_controls_tests` passed strict
JSON/presence tests and actual requests to its own ephemeral loopback server,
using the same request adapter as the application discovery route. This
acceptance verifies default/full-week/empty schedules, inactive null slots,
literal empty timestamp selection, inert calendar controls and failure
responses. It neither replaces nor accesses the user's running application.
The application server translation unit was also freshly compiled with the
new allowed query names and optional timing adapter.

The final timing and transport tests, together with the original OCEL suite,
pass 3/3 in `build/parity-closures/native-current/ctest-timing-json-roundtrip.log`.
The discovery JSON writer now uses the classic locale and emits negative zero
as a real-number token (`-0.0`). All three performance arrays round-trip
bitwise under a hostile decimal/grouping locale, covering a fractional large
duration, microseconds, adjacent binary64 values, signed zero and extreme
finite values. The existing arrays already used seventeen-digit output;
this increment repairs locale and signed-zero fidelity.

The existing OCEL explorer exposes a "Configure edge performance timing"
control. Enabling it selects elapsed or business time and either the default
schedule or custom slot JSON; leaving it disabled omits the new timing
controls. Its five frontend request-control checks pass. API callers can also
supply the literal event/timestamp selectors and inert calendar JSON. Rendered
application interaction remains unverified; frontend request/build and owned
HTTP checks are not rendered browser evidence.

No additional reference call, source/AST/bytecode read, or calendar-object
protocol inference was used to implement this increment.

Final acceptance is retained in `docs/ocdfg-working-time-acceptance.json.gz`:
source/test hashes and snapshots, assertion-enabled build and eight-suite native
results, frontend checks and six actual production-handler requests on an
independently owned temporary server. The latter server exited after testing;
the existing application was not replaced. Earlier failed native expectations
and HTTP harness attempts remain in their original build directories.
