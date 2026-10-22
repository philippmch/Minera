# Activity relationship filtering

This work extends the pinned LTL filtering family across EventLog, pandas and
Polars. The three catalogue rows remain partial. The existing native LTL
workflow is preserved separately.

## Workspaces and operands

Choose **Activity relationships and performers** in the event-log family or
table-filter selector. Each engine offers sequence, consecutive-activity,
performer-intersection and different-performers rules.

| Public function | EventLog / pandas operands | Polars operands |
| --- | --- | --- |
| `eventually_follows` | Activity collection | Activity collection |
| `A_next_B_next_C` | Separate A, B and C | Activity collection |
| `four_eyes_principle` | Separate A and B | Activity collection |
| `attr_value_different_persons` | Single A | Single activity value |

EventLog also exposes the two public timestamp-list comparison helpers with
typed differences and a typed list of lower/upper intervals, with no source upload.
The helpers check each difference against its corresponding inclusive lower or
upper endpoint; an empty difference list returns true. Their complete typed return value is
exported as a one-cell native EventTable projection.

Activity collections can be JSON arrays of literal strings or explicit typed
collections. Separate operands use typed JSON. Exact integer text, null values,
empty labels, commas, whitespace and escaped characters are passed unchanged.
Literal selectors preserve empty attribute names. The role used when reopening
a filtered log is independent of the attribute being matched.

## Timestamp behavior

Polars sequence and consecutive rules order by the chosen timestamp column.
The performer rules do not use timestamps. Polars ignores difference-bound
and enable controls, so those controls are not offered.

Log eventual filtering checks difference bounds only when explicitly enabled
and the supplied intervals are nonempty. Its time deltas read consecutive raw
events at the beginning of each trace, using the requested pattern length,
independently of the matched activity positions. Pandas eventual filtering enables time
when nonempty intervals are supplied and the flag is omitted; explicit false
disables it. Empty intervals do not activate time. Explicit null differs from
omission and can produce an engine diagnostic. Only eventual rules use these
controls. Pandas consecutive matching uses the last B and C occurrences in each case
and checks their original row positions against an A directly before B.
Interleaved cases interrupt a match.

## Source and artifacts

Event logs accept actual CSV, XES, compressed XES or native log JSON Files.
CSV import exposes case, activity, timestamp and performer columns, delimiter
and explicit whitespace trimming. Tables accept actual native EventTable JSON
Files with declared types and supported index/category/context metadata.
Downloadable examples contain both ordered activities and performer values.

Polars relationship filters offer a **Polars input type** selector. `LazyFrame`
is the default; `DataFrame` invokes the native eager overload. The choice can
change result order and error timing. For example, retained eager eventual
filtering returns timestamp/case-sorted rows while lazy filtering restores source
row order; ordinary eager consecutive filtering returns the retained apply-time
`AttributeError` for missing `collect`. The result inspection shows the effective
input type on success and native errors.

`POST /api/reference-ltl-filter` accepts multipart controls and an actual
`source_file` for filtering. Timestamp helpers accept only engine, operation and
their two typed operands. Unknown, duplicate, inactive and unrelated File-shaped
controls are rejected. Native byte fields use the exact `<field>.txt` filename
sent by the frontend to retain newlines and lone surrogate bytes. Query and
multipart fields share one duplicate check.
An explicitly present empty filename remains a File.
For Polars, optional `input_kind` accepts exactly `LazyFrame` or `DataFrame`;
omission selects `LazyFrame`. Other engines reject this control. Invalid or
duplicate values reject before source import.

Every response has the same 25-field envelope. It records the selected engine
and operation, HTTP status, stage, whether apply returned, source format,
complete source/result counts, actual selectors, full parsed source before and
after processing, complete filtered result, XES projection/error, helper value
and complete native error type/message. A public Polars apply return is distinct
from subsequent collection success.
`input_kind` records the validated effective Polars type, or null for other
engines and errors before a type was validated. The frontend checks it against
the requested type, including source/apply/collect failures. For a Polars request,
a null type is accepted only for an early HTTP 400 control error.

Native strings can use the existing raw-byte carrier. Downloads preserve their
complete bytes. The complete response is downloadable with original numeric
tokens. Source artifacts remain available after an algorithm error. XES export
failure leaves the native filtered result available. Reopening XES carries the
actual activity selector into the existing log loader.

Changing a file or criterion cancels an outstanding request and invalidates its
identity. Late responses cannot replace the result for a different file or
form. There is no automatic request on workspace render.

## Polars input-type acceptance

The input-type checkpoint passes three assertion-enabled native suites: all
**675 retained Polars outcomes** (516 complete frames and 159 full diagnostics),
the relationship workbench, and its owned HTTP route. The frontend passes
**192 files / 9,833 tests**, TypeScript and the production build. Independent
review verifies all 675 original receipt hashes and 16 eager/lazy pairs, plus
208 transport-condition checks. The original oracle remains unchanged.

Rendered browser checks use the final rebuilt server on a fresh owned loopback
port. They cover public navigation, both input types, complete six-row ordering
and table context, source preservation, stale-result clearing, exact result and
HTTP-response downloads, and the eager consecutive-rule error with complete
source/source-after/response downloads. Both the owned server and isolated
browser close with their output drained. Existing application processes are
not replaced. This bounded product check does not close a catalogue unit or
authorize new reference calls.

## Earlier integrated acceptance boundary

The integrated checkpoint passes **47 assertion-enabled native suites**,
including all **1,401 distinct retained public outcomes**: 297 LOG, 429 pandas
and 675 Polars. The frontend passes **192 files / 9,829 tests**, TypeScript and
Vite. The three module contracts describe the exact observed domains and keep
the canonical module rows partial.

The final production server passes **73 relationship requests and 101 attributes
regression requests** on independently owned temporary servers. They share
binary SHA256 `be6f9e6b656cb157575554d0d2e4a11fa3984a4d935b04938dba903f3a944445`.
Both processes exited. The existing application was not replaced. Rendered
browser interaction was unverified at that earlier checkpoint.

`docs/ltl-filter-workflow-checks.jsonl.gz` binds current sources, exact native
units, compiled artifacts, the frontend output and complete actual HTTP
receipts. The three engine public/checks archives retain original observations,
failed attempts and current native identities. Initial pandas optional-cell and
mixed-case mismatches, log timestamp-selection mismatches and local raw-carrier
reader mistakes remain in the retained history. Oracle expectations were not
weakened or rewritten.

Full parity remains open across **86 packages / 387 modules**, with **201 strict
open units**. This checkpoint adds usable controls and native companions while
preserving the wider catalogue and each contract's unverified domains.

Reference observation uses public calls, signatures/docs/enums and opaque
module identities only. Prior complete receipts and every current candidate
are checked before algorithm entry. Retained calls are never replayed.
