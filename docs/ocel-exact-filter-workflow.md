# Exact OCEL filter controls

Final source/test snapshots and verification receipts are retained in
`docs/ocel-exact-filter-workflow-checks.json.gz`, with per-member byte counts
and SHA-256 hashes.

The existing OCEL preparation workspace now exposes five additional filter
choices: exact event and object attributes, exact timestamp bounds, exact
minimum object-type counts, and exact activity/type matching. The fifteen existing convenience variants retain their
controls. Results use the same inspection, download and opening workflow.

All five use `POST /api/ocel/filter` with the original OCEL JSON source.
The new query controls carry their literal JSON text without converting
integers through JavaScript numbers or splitting labels on delimiters.

| Variant | Controls |
| --- | --- |
| `event_attribute_exact` | Required literal `attribute_key`, `attribute_values_json` typed scalar array, optional `positive` and `column_kind=auto\|scalar\|timestamp` |
| `object_attribute_exact` | Required literal `attribute_key`, `attribute_values_json` typed scalar array, optional `positive` and `column_kind=auto\|scalar\|timestamp` |
| `activity_type_matching_exact` | Required `allowed_json` object of string arrays; optional literal `activity_key`, `object_type_key`, `temporary_column`, `temporary_separator` |
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

Exact activity/type matching accepts empty mapping keys, empty activity names,
empty allowed lists and `{}`. Correspondence JSON reaches native validation
verbatim. Activity and object-type selectors default to `ocel:activity` and
`ocel:type`; they can also address other stored native relation roles. Extra
relation dataframe cells have no native value storage. Temporary controls use
explicit Native default / Literal choices, preserving an omitted value versus
a supplied empty string. A supplied temporary column is removed from the
returned relation schema; reusing an identity can produce the retained native
missing-ID error, while reusing the qualifier clears selected qualifiers.
Matching retains selected relation order and multiplicity and propagates all
six tables; unused events and objects are removed. The convenience matching
variant retains its previous controls. See
[ocel-activity-type-matching-parity-contract.md](ocel-activity-type-matching-parity-contract.md)
for the retained public evidence and typed native boundary.

The final checkpoint passes all nine relevant assertion-enabled native suites,
including core OCEL, exact JSON/HTTP controls, event/object membership, event
NaT, activity/type matching, existing filtering and working-time checks. The
controls target sends actual HTTP requests for all five exact variants,
including literal empty names, wide integers, repeated relationships,
opening marked outputs and activity/type temporary controls. Its byte-wise
query encoder exercises NUL and &, =, + delimiters. The server translation
unit is freshly compiled. A separate temporary instance of that production
server also passed **ten real HTTP calls**, covering exact matching,
download/open sources, literal NUL controls, and working-time missing/NaT
elapsed-zero and working-time-error behavior. Its receipt is
`build/parity-closures/owned-final-ocel-http-2/validation.json`; the owned
server exited and the existing application was not replaced.

The full frontend suite passes **9,677 tests across 179 files**. The final
29 focused preparation tests, TypeScript check and Vite production build
also pass. Protocol tests retain complete download/open/reupload sources
and exact JSON; SSR verifies control markup. Rendered browser interaction
and replacement of the existing running app remain unverified.
