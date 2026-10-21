# Exact OCEL filter controls

The existing OCEL preparation workspace now exposes four additional filter
choices: exact event and object attributes, exact timestamp bounds, and exact
minimum object-type counts. The fifteen existing convenience variants retain their
controls. Results use the same inspection, download and opening workflow.

All four use `POST /api/ocel/filter` with the original OCEL JSON source.
The new query controls carry their literal JSON text without converting
integers through JavaScript numbers or splitting labels on delimiters.

| Variant | Controls |
| --- | --- |
| `event_attribute_exact` | Required literal `attribute_key`, `attribute_values_json` typed scalar array, optional `positive` and `column_kind=auto\|scalar\|timestamp` |
| `object_attribute_exact` | Required literal `attribute_key`, `attribute_values_json` typed scalar array, optional `positive` and `column_kind=auto\|scalar\|timestamp` |
| `timestamp_exact` | Required `from_json`/`to_json` typed bounds and optional literal `timestamp_key` (default `ocel:timestamp`) |
| `object_count_exact` | Required `minimum_counts_json` object with literal type names and signed integer, fractional, boolean or quoted nonfinite thresholds |

Membership entries use `type` and `value`. Types are `string`, `integer`
(signed int64), `decimal`, `boolean`, `null`, `timestamp`, and `nat`. Null and NaT may omit
`value`; decimal strings can represent `NaN`, `INF`, and `-INF`. Timestamp
values retain nanoseconds and are distinct from literal strings. Column kind
selects timestamp semantics explicitly when the source metadata does not
describe its datetime columns. A `{"type":"nat"}` query selects missing datetime
cells; null and decimal NaN queries do not select those cells. NaT may also use
`"value":null`, and rejects other supplied values.

Object membership accepts canonical `ocel:oid`/`ocel:type` selectors and literal
attribute names, including empty names. Matching unused objects remain in the
selection; event, relation, object-link, event-link and change tables follow the
selected objects. Boolean/integer/decimal comparisons preserve values above
2^53. The native object contract and retained 201-call evidence remain separate
from the new twelve-call event NaT extension.

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
HTTP query/JSON dispatch for all four variants, wide integers, fractional
relation counts, nanosecond bounds, metadata, literal names, repeated
relations and ordinary reopening of marked output. Existing core OCEL,
filtering and captured event-attribute checks pass. The entire frontend suite
passed 9,667 tests before the final literal-name and timezone-hint checks;
the final focused suites and production build are checked separately.
The server translation unit is freshly compiled. Rendered browser
interaction and replacement of the existing running app remain separate.

Current extension validation is recorded with the implementation checkpoint: the
controls target additionally exercises exact object HTTP selection, polarity,
unused objects, repeated relationships and ordinary opening, plus typed NaT
parsing and both membership APIs. Frontend protocol and SSR tests cover all four
selectors; SSR and HTTP doubles do not establish rendered browser interaction.
