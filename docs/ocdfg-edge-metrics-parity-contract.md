# OCDFG edge associations and set aggregation

Target: PM4Py 2.7.22 at `0715c31f3f1d85924107d6aa5329806db69df899`.
This is a bounded helper increment. The full OCDFG module, performance gate,
and complete parity catalogue remain open.

The retained public bank in
`build/parity-closures/ocdfg-performance-public-actual-1/capture.json` has SHA256
`2530a248aeaffe22dc4fab303951233a8794f4c176d8ae7ab8f2393cd1ab0d90`.
The working-time increment already tested sixty timing outcomes and eight
complete discovery outcomes/errors. This companion covers the other nine:

| Public helper | Retained outcomes |
| --- | --- |
| `aggregate_ev_couples` | One full nested set of event-ID pairs |
| `aggregate_total_objects` | One full nested set of event/event/object triples |
| `aggregate_unique_objects` | One full nested set of object IDs |
| `find_associations_per_edge` | Two complete ordered association results and four exact missing-selector errors |

The public interface receipt is retained at
`build/ocdfg-performance-public-interface-1badbdac98264c6b94837769d4312932/public-interface.json`.
Only its recorded public signatures/enums were consulted. The new evidence
subset `docs/ocdfg-edge-metrics-public-evidence.json.gz` preserves the nine
original rows and interface metadata, including input/parameter/collection
snapshots. No reference calls were added or replayed, and no implementation
source, AST, bytecode or private state was inspected.

## Native companion

`find_ocdfg_edge_associations` accepts either the explicitly declared, ordered
relation table or a native `Ocel` log. `OcdfgAssociationOptions` keeps absence
and literal empty selector names distinct. The strict companion reads event
ID, object ID, object type and activity columns without legacy label aliases
or whitespace trimming. Timestamp selection is retained but inactive: the
saved call with a missing timestamp selector succeeds unchanged.

Missing event-ID selection raises `KeyError` with `'<name>'`; missing object
ID, object type and activity selection raises `KeyError` with
`'Column not found: <name>'`, matching all four retained diagnostics. The
observations do not establish precedence when several selectors are missing.

Associations follow relation-row order for each object, retain multiplicity,
and expose intermediate repeated-event self-links. Event-pair, full-triple
and object-ID aggregation return real sets. The native representation and JSON
report sort set members deterministically; the public set's incidental
enumeration order is not an ordered-list contract. Ordered association lists
are compared without sorting. Aggregation preserves explicitly empty type
groups and edge entries as a practical native container contract.

The direct relation-table carrier is bounded to string identity, activity and
object-type cells with a stable type per object. Missing/non-string row cells
and type changes reject explicitly; their full reference behavior is not
claimed. The native `Ocel` adapter derives denormalized relation labels from
the actual stored event/object fields and attributes, retaining declared
relation columns. It requires event/object records for every relation. Direct
relation-table inputs can retain their own captured denormalized cells.

## Validation and application use

The offline generator
`build/parity-closures/generate-ocdfg-edge-metrics-tests.py` constructs complete
native inputs and expected sets/lists/errors from the retained receipt. The
registered test directly calls `public_aggregation_vectors` and
`public_association_vectors`, comparing every returned member or error and
checking original input preservation. Practical checks cover interleaved
object rows, duplicate memberships, self-links, empty maps and literal names.

`serialize_ocdfg_edge_metrics_json` emits separate endpoint fields, ordered
association triples and all three set projections. Separate endpoint fields
preserve labels containing `->`. Literal JSON escaping, source preservation
and the shared request adapter have native and owned ephemeral HTTP
acceptance cases. The query keys are
`ocdfg_edge_metrics_event_id_key`, `ocdfg_edge_metrics_object_id_key`,
`ocdfg_edge_metrics_object_type_key`, `ocdfg_edge_metrics_activity_key` and
`ocdfg_edge_metrics_timestamp_key`. Absent keys use canonical columns;
explicit empty values select literal empty columns.

Both new assertion-enabled native targets pass, together with the original
OCEL, working-time and numeric conformance suites: five of five in
`build/parity-closures/native-current/ctest-edge-metrics.log`, after the
successful build in `build-edge-metrics-second.log`. The initial MSVC
test-literal compilation failure is preserved in `build-edge-metrics.log`;
moving the escaped raw JSON outside the assertion macro fixed that test
syntax without changing its expected data.

The application entry point is the existing OCDFG discovery route with opt-in
`include_edge_metrics=true`; the report does not calculate timing. Eight
requests through the freshly compiled production handler pass in
`build/parity-closures/edge-metrics-production-http-1/validation.json` and
`records.jsonl`. They verify omitted/false options preserve the original
envelope, duplicate and repeated-event associations, all five literal
selectors, exact missing-column errors, incompatible variants, invalid
booleans and empty results. The independently owned temporary server exited
after acceptance; the existing application was not replaced.

The OCEL explorer now exposes optional edge metrics, per-role literal column
selectors and returned projection tables with pagination. Complete JSON
download retains every returned row beyond the visible page. Thirty-six
focused frontend tests, TypeScript and the production Vite build pass. These
request/render-structure checks and production-handler HTTP observations do
not establish rendered browser interaction, which remains unverified.

`docs/ocdfg-edge-metrics-checks.json.gz` binds the implementation, offline
generator, assertion-enabled native objects/build/test logs, frontend sources
and checks, and production HTTP receipt. The retained public bank now has
named native acceptance for all seventy-seven outcomes across working-time
and edge-helper tests; this bounded coverage does not close the whole OCDFG
module or performance gate.
