# Adjacent paths in the filtering workspace

The existing workspace now offers **Match adjacent activity pairs** and
**Filter by adjacent-pair duration** for logs. Table modes offer **Match adjacent
paths**, with ordinary and duration criteria. Upload CSV, XES/XES.GZ or complete
native log JSON in log mode, or a genuine native EventTable JSON File for pandas
and Polars. Each table engine has a downloadable typed example.

Ordinary criteria use an array of two-string arrays, for example
`[["A", "B"], ["X", "Y"]]`. An empty outer array is valid. Duration criteria use
one two-string array such as `["A", "B"]`. The workspace explicitly gives each
pair tuple semantics in the native helper. It preserves literal empty labels,
commas, whitespace, Unicode, newline and NUL; it does not split CSV-like text.
Malformed shapes and nonstring members are rejected before filtering.

Log filters inspect the original trace order, retain complete selected traces
and preserve all log/trace/event context. Their public performance operation
subtracts floating timestamp values; it has a distinct temporal contract from
the case-duration helper. Table filters sort by selected case and timestamp
before checking adjacency, so both ordinary and duration operations require
a timestamp column. Results retain whole selected cases in that sorted order.
Pandas uses fractional elapsed seconds; Polars uses integral elapsed seconds.
Their two companions preserve their own missing-value, dtype and diagnostic
behavior rather than routing through the legacy convenience filter.

The table algorithms match comma-concatenated label keys. Literal comma labels
can therefore create public collisions: different pairs may produce the same
encoded key. The native companions retain this observed behavior. Log matching
uses tuple-shaped pairs instead; its count/threshold helpers have their own
comma-joined key contract, documented separately.

Both table engines support a separate target activity column for ordinary path
filtering. Its omitted value follows the selected source activity column; an
explicit empty column name remains literal. Duration filtering uses the same
selected activity column for both ends and omits the inactive target control.
Case, activity and timestamp selectors retain literal values. Log CSV defaults
follow configured activity/timestamp columns; XES/native log defaults use
`concept:name` and `time:timestamp`. Reopening filtered logs carries the actual
source activity, even when the filter uses a different event attribute.

An exported XES projection may retain nonstandard CSV keys such as `activity`.
When filtering that file again, supply its actual activity key. If the imported
projection has no declared first-class activity role, an explicit path attribute
also supplies the opening role. The workspace's original open action carries
that selector alongside the exported file.

Both modes expose keep-matching and keep-outside selection. Duration bounds are
inclusive and remain exact scalar tokens until native evaluation. The workspace
starts with zero and `9223372036854775807`; supplied integers retain precision.
Native APIs additionally preserve the wider typed parameter/query/error domains
described by their module contracts. No rows are reduced to a preview when
downloading: table results, parsed sources and exact responses are complete;
log results retain native JSON, XES and reopening controls.

`POST /api/reference-table-paths` accepts `engine`, `operation` (`paths` or
`performance`), genuine multipart `source_file`, boolean `positive`, and either
`paths_json` or `path_json`. Optional literal selectors are `case_id_key`,
`activity_key` and `timestamp_key`. Ordinary mode accepts optional
`target_attribute_key`; duration accepts `minimum` and `maximum`. Unknown,
duplicate and inactive controls are rejected. Its `TABLE_PATHS_FILTER` response
includes selected roles, source/result counts, complete source/result native
artifacts, source-preservation status, exact error type/message, phase and
whether apply returned. Polars apply and collect errors remain distinct.

Log operations use the existing `POST /api/reference-log-filter` route with
`operation=paths` or `operation=path_performance`, the corresponding pair field,
`positive` and optional literal `activity_key`. Duration additionally accepts
`minimum`, `maximum` and `timestamp_key`. Existing source-format and CSV controls
remain available. Changing source, operation, engine or criteria invalidates
stale requests; unmounting cancels the active request.

Native helper and production-handler checks cover full artifacts and literal
transport. Frontend tests cover real multipart construction, strict envelope
validation, complete download bytes, request invalidation and available controls.
Rendered browser interaction remains unverified. The three canonical modules
remain partial; their contracts retain the wider object, warning, dtype,
temporal/runtime-identity and execution-plan boundaries.

The retained acceptance checkpoint covers 686 distinct public calls, all 686
exact projected native outcomes, 24 native suites, 44 production HTTP checks,
and 184 frontend test files with 9,726 tests. TypeScript and the production
frontend build pass. `paths-filter-workflow-checks.jsonl.gz` binds the current
sources, native binaries, dedicated module archives and validation receipts.
Earlier compiler, native-diagnostic and HTTP harness failures remain retained.
The HTTP reruns explicitly select the activity key retained by exported CSV
logs when reopening native JSON or XES; a native regression also covers XES
role discovery. Each HTTP server exited after its owned run.
