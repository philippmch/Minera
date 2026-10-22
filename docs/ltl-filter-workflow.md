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
and enable controls. Friendly controls omit them; original typed-call mode can
preserve their supplied typed values without enabling a difference constraint.

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

Every response has the same 27-field envelope. It records the selected engine
and operation, HTTP status, stage, whether apply returned, source format,
complete source/result counts, actual selectors, full parsed source before and
after processing, complete filtered result, XES projection/error, helper value
and complete native error type/message. A public Polars apply return is distinct
from subsequent collection success.
`input_kind` records the validated effective Polars type, or null for other
engines and errors before a type was validated. The frontend checks it against
the requested type, including source/apply/collect failures. For a Polars request,
a null type is accepted only for an early HTTP 400 control error.

## Polars original typed calls

Polars offers **Friendly controls** by default and an exclusive **Original typed
call** mode. Original mode keeps the same source File, relationship operation
and DataFrame/LazyFrame choice. Its operand-presence selector distinguishes an
omitted argument from a supplied typed null. Parameters can be omitted, supplied
as a raw typed non-mapping value, or supplied as ordered mapping entries. Empty
entries mean a present empty mapping. Non-Boolean positive values and typed
selectors reach the generic native call without Boolean/string conversion.

The operand/parameter editors retain their JSON fragments as entered. The
complete-call editor and strict UTF-8 File importer preserve the whole call text,
including numeric tokens, whitespace and CR/LF. Invalid UTF-8 is rejected without
replacement decoding. Editing a textarea uses the browser's normal newline
editing behavior; importing and submitting without editing retains imported
CR/LF bytes. JSON numbers are not parsed and rewritten in the frontend. Prefer
typed integer values as decimal strings to preserve exact text in other tools.

For the original mode, multipart `call_json` has exactly this versioned shape:

```json
{
  "schema": "minera-polars-ltl-original-call-v1",
  "values": {"type":"list","value":[{"type":"string","value":"A"},{"type":"string","value":"B"}]},
  "parameter_entries": [
    {"key":"positive","enum_key":false,"value":{"type":"integer","value":"0"}},
    {"key":"positive","enum_key":true,"value":{"type":"integer","value":"1"}}
  ]
}
```

`values` is optional. Both parameter fields absent means omitted parameters;
optional `parameter_value` accepts a raw typed non-map value and is mutually
exclusive with `parameter_entries` by presence. Entries retain their order and
have exactly string `key`, Boolean `enum_key` and typed `value`. The string key
and enum key with the same underlying name are distinct, and either insertion
order is retained. Enum keys are limited to `pm4py:param:case_id_key`,
`pm4py:param:attribute_key`, `pm4py:param:timestamp_key`,
`pm4py:param:resource_key`, `positive`, `enable_timestamp` and
`timestamp_diff_boundaries`. Native enum precedence remains unchanged. Unknown
string keys remain literal; nonstring keys are rejected rather than stringified
or silently omitted. Duplicate identities and unknown enum identities are
control errors.

Original requests contain only `engine=polars`, the existing `operation`,
optional `input_kind`, actual `source_file` and `call_json`. Friendly operands,
selectors, flags and unrelated source controls are forbidden alongside
`call_json`; other engines reject it. Shape/JSON/control failures occur before
source import. Missing operands and valid typed values retain native semantic
diagnostics and apply/collect timing.

The envelope adds `call_mode` and exact native-string `call_json`. Friendly mode
has a null call echo; rejection before field capture can leave both null. After
capture, original-mode control errors retain the exact malformed call text.
The frontend checks mode and call bytes against its request, including native
failures. Inspection and **Download exact original call** preserve that echo on
errors as well as success. Original-mode string selector fields are null because
typed selectors cannot be truthfully reported as friendly default strings.

The 675 retained calls are the acceptance target for this interface; adding the
editor is not itself an execution result. Existing native carriers still limit
fidelity: parameter/map keys are strings, and typed operand map keys use lexical
order. Arbitrary Python keys or ordered operand dictionaries need a separate
carrier extension. Sequence child keys, float display spelling and date source
precision can differ internally between the existing typed parser and retained
fixture constructors; original receipts and expected outputs are not rewritten.
Python fold/timezone provenance, NaN payloads, arbitrary objects, warning lists
and physical plans remain outside this bounded transport. The three module rows
and all 201 strict open units remain partial/open.

Native strings can use the existing raw-byte carrier. Downloads preserve their
complete bytes. The complete response is downloadable with original numeric
tokens. Source artifacts remain available after an algorithm error. XES export
failure leaves the native filtered result available. Reopening XES carries the
actual activity selector into the existing log loader.

Changing a file or criterion cancels an outstanding request and invalidates its
identity. Late responses cannot replace the result for a different file or
form. There is no automatic request on workspace render.

## Original typed-call acceptance

All **675 admitted Polars calls** pass through the original typed-call mode,
first through the direct workbench and then through an independently owned HTTP
listener. Each pass checks **516 complete frames / 159 full diagnostics**, exact
apply/collect boundaries, the fixed 27-field envelope, complete source before
and after processing, and unchanged request bytes. Expected frames and errors
come from the original admitted observations; their original native vectors and
predicate implementation remain unchanged. The original 675 native comparisons
also pass. The workbench and route control suites pass separately.

The frontend passes **192 files / 9,843 tests**, TypeScript and the production
build. Actual browser interaction against the rebuilt server covers the friendly
default, original missing/null operands, raw non-mapping parameters, both
string/enum conflict orders, eager complete-call import, exact CRLF and large
integer text, complete downloads and stale-result clearing. Both owned browser
and server processes close with output drained. The browser uses its own sample;
the full 675-call comparison runs through native direct and HTTP tests.

Evidence is retained under
`build/parity-closures/ltl-polars-original-call-acceptance-1/`. The generated
transport fixture is separately bound to all original receipt hashes. Its
initial layout and later per-case wrappers contain identical assertion calls;
the first browser recipe's syntax failure is preserved before its corrected
recipe. No new reference calls or catalogue promotions occur.

## Earlier Polars input-type acceptance

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
