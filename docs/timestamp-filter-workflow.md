# Timestamp filtering workflow

The existing log-filter workspace includes timestamp windows for event logs,
pandas dataframes and Polars lazy dataframes. Log mode accepts CSV, XES or
complete native log JSON. Table modes accept complete native EventTable JSON,
including column types, index/category declarations and metadata.

Log mode exposes seven operations: keep events in a window, retain complete
contained or intersecting traces, select by first/last event time, select traces
with a matching event attribute in the window, or select by a trace timestamp
attribute. pandas exposes the corresponding six dataframe operations except
trace attributes. Polars exposes events, contained/intersecting cases and
attribute-in-window selection; its public module has no start/completion API.

String endpoints use the public parser's `YYYY-MM-DD HH:mm:ss` format and UTC
normalization. The workspace sends endpoint text unchanged; a `T` separator,
fractional suffix or timezone suffix can raise a native parser diagnostic.
The downloadable table examples declare UTC timestamps. Uploaded naive columns
are retained and receive the engine's observed timezone diagnostics; the
workflow does not silently convert their dtype. Native companion APIs also
retain typed bounds and their finer temporal domains as documented separately.

Events, contained cases and start/completion tests include their endpoints.
Dataframe trace filters aggregate minimum/maximum timestamps and preserve source
row order. pandas intersection requires strict overlap. Polars uses its three
strict intersection clauses, so a trace exactly spanning both endpoints is
excluded. Log trace windows use original event positions; their public helper
predicates and context behavior have separate native acceptance.

Only start/completion exposes keep-matching or keep-outside selection. pandas
also exposes its missing-temporal-value control: events retain missing cells,
and case filters can retain cases with no temporal values. The matched event
attribute accepts the established exact typed JSON control, including integer
tokens beyond JavaScript's exact range. Literal role selectors preserve empty
keys, whitespace, Unicode, newlines and NUL bytes.

Event-window filtering in log mode has an observable conversion side effect:
trace attributes are copied into parsed source events before filtering, even
before some later errors. The native mutable API preserves it. The workspace
reports the actual source-preservation status and offers complete parsed source
snapshots before and after filtering. Selected traces are reconstructed using
the converter's observed grouping and attribute behavior. Other log timestamp
filters retain their separately observed context. No result is limited to a
preview: native JSON, XES, source snapshots and raw responses can be downloaded.
Reopening carries the original activity role alongside the exported log.

For CSV event-window input, preparation adds the conventional `concept:name`
trace identity required by the public event-to-log converter. The original
case-column role and its exact value remain present. The before-filter source
snapshot includes that explicit preparation. The native log companion itself
retains missing-canonical-ID errors and does not invent case-key parameter
forwarding; native JSON and XES inputs receive their actual stored trace keys.

`POST /api/reference-table-timestamp` takes `engine`, `operation`, genuine
multipart `source_file`, `lower`, `upper`, and optional literal `timestamp_key`.
Case modes accept `case_id_key`; attribute mode accepts `attribute_key` and
requires `attribute_value_json`. pandas accepts `keep_nan_values`, and its
start/completion modes accept `positive`. Unknown, duplicate and inactive
controls are rejected. The `TABLE_TIMESTAMP_FILTER` response retains complete
source/result artifacts, counts, roles, source status, full type/message
diagnostics, phase and whether apply returned. Polars apply and collect errors
remain distinct.

Log mode uses `POST /api/reference-log-filter` with `operation=time_events`,
`time_contained`, `time_intersecting`, `time_starting`, `time_completing`,
`time_attribute` or `time_trace_attribute`, plus `lower`, `upper` and optional
`timestamp_key`. Attribute mode additionally requires `time_target_json` and
accepts `time_attribute_key`. Only start/completion accepts `positive`.
Existing CSV/source-format controls remain available; `activity_key` selects
the role used for reopening. Timestamp responses additionally retain complete
`source_log_json` and `source_after_log_json` snapshots, `timestamp_key` and
`time_attribute_key`.

Source, mode, engine or criteria changes invalidate pending requests. Unmounting
cancels the request. Rendered browser interaction remains unverified. The three
canonical timestamp modules remain partial for the object, dtype, warning,
runtime identity and execution-plan boundaries documented in their contracts.

Final acceptance covers 1,935 distinct retained public calls, all 1,935 exact
projected outcomes, 29 assertion-enabled native suites, 50 actual production
HTTP requests and 185 frontend files with 9,737 tests. TypeScript and Vite pass.
`timestamp-filter-workflow-checks.jsonl.gz` binds current sources, compiled
identities, module archives and validation receipts. Earlier native failures,
the first frontend timeout and the HTTP harness's incorrect raw-log date schema
remain retained. Native expected outcomes were unchanged during repairs.

The log recipe audit has an explicit timing correction: pre-entry checks
compared hashes of complete new call receipts against saved prior receipt
hashes; the four-call forwarding recipe did not reconstruct every prior receipt
before entry, despite its preparation note. Root reconstructed the initial 326
receipts before the converter continuation and the next twelve after the
forwarding run. Independent reconstruction found no duplicate or replay. The
original recipes/notes remain preserved alongside that correction. pandas and
Polars continuations reconstruct their full prior receipts before entry.
