# Exact OCEL filter controls

The existing OCEL preparation workspace now exposes three additional filter
choices: exact event attributes, exact timestamp bounds, and exact minimum
object-type counts. The fifteen existing convenience variants retain their
controls. Results use the same inspection, download and opening workflow.

All three use `POST /api/ocel/filter` with the original OCEL JSON source.
The new query controls carry their literal JSON text without converting
integers through JavaScript numbers or splitting labels on delimiters.

| Variant | Controls |
| --- | --- |
| `event_attribute_exact` | Required literal `attribute_key`, `attribute_values_json` typed scalar array, optional `positive` and `column_kind=auto\|scalar\|timestamp` |
| `timestamp_exact` | Required `from_json`/`to_json` typed bounds and optional literal `timestamp_key` (default `ocel:timestamp`) |
| `object_count_exact` | Required `minimum_counts_json` object with literal type names and signed integer, fractional, boolean or quoted nonfinite thresholds |

Membership entries use `type` and `value`. Types are `string`, `integer`
(signed int64), `decimal`, `boolean`, `null`, and `timestamp`. Null may omit
`value`; decimal strings can represent `NaN`, `INF`, and `-INF`. Timestamp
values retain nanoseconds and are distinct from literal strings. Column kind
selects timestamp semantics explicitly when the source metadata does not
describe its datetime columns.

Timestamp bounds use `type=string` for the reference whole-second date format
`YYYY-MM-DD HH:MM:SS`, or `type=timestamp` for parsed ISO datetime values.
Datetime bounds interpret their local clock as UTC, retaining the fractional
part, rather than converting their timezone offset. Source values still
require timezone awareness. Bounds are inclusive; reversed bounds return an
empty selection. The complete native filter contract remains in
[ocel-event-attributes-parity-contract.md](ocel-event-attributes-parity-contract.md).

Exact counts retain arbitrary signed integer tokens. Required types must be
present even for zero/negative thresholds, and repeated event-object relation
rows each contribute. Empty type names and attribute names remain literal.
The complete count semantics are recorded in
[ocel-objects-ot-count-parity-contract.md](ocel-objects-ot-count-parity-contract.md).

Exact input reading accepts literal empty names, IDs and type labels. Its
classic reader retains repeated `omap` occurrences and matches typed
qualifiers to those occurrences; surplus typed rows remain separate. Exact
outputs include `minera:literalNames=true`, allowing ordinary file opening to
retain that representation. Unmarked input keeps the existing interchange
reader's validation and behavior. This is a scoped opt-in transport, not a
claim that every general OCEL importer/exporter is complete.

Validation: four assertion-enabled native targets pass, including actual
HTTP query/JSON dispatch for all three variants, wide integers, fractional
relation counts, nanosecond bounds, metadata, literal names, repeated
relations and ordinary reopening of marked output. Existing core OCEL,
filtering and captured event-attribute checks pass. The entire frontend suite
passed 9,667 tests before the final literal-name and timezone-hint checks;
the final focused suites and production build are checked separately.
The server translation unit is freshly compiled. Rendered browser
interaction and replacement of the existing running app remain separate.
