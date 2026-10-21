# Table between filtering

The existing table filter workspace now offers **Attribute repetition** and
**Between activities**. Choose pandas or Polars, upload an actual native
EventTable JSON file, or download the selected engine's example. Enter the
start and end controls as JSON strings or arrays of strings. Empty strings
and empty arrays remain literal values.

Case and activity column controls default to `case:concept:name` and
`concept:name`. Select a custom control to send its exact text, including an
empty name, whitespace, Unicode, newline or NUL. The default separator is
`##@@`; a custom empty separator is valid. Polars requires the canonical
`time:timestamp` column and sorts by case and timestamp. The two engines have
different extraction and numbering behavior; each uses its own native
companion and retained public acceptance bank. Pandas uses zero-based numbers
and keeps the first open start; Polars keeps the latest start and assigns
one-based numbers by lexical ordering of textual interval indexes. Longer
Polars traces can therefore show segment numbers out of chronological order.

The result shows the selected engine, source/result dimensions and completion
or error phase. Download the complete returned native table, parsed source
table or exact response bytes. Table artifacts preserve typed integer storage,
cell values, column order and the metadata supported by the native codec.
Changing the file, engine or controls invalidates an earlier request; stale
responses cannot replace the current result. Leaving the operation cancels
its request.

`POST /api/reference-table-between` accepts multipart fields `engine`,
`source_file` (a genuine uploaded File), `act1_json`, `act2_json`, and optional
`case_id_key`, `activity_key`, `subcase_concat_str`. Query controls use the same
literal names; duplicates across query and multipart, unknown controls and
invalid endpoint arrays are rejected. Arbitrary typed endpoint/control
negative cases remain accessible through the native helper APIs.

The `TABLE_BETWEEN_FILTER` response retains complete `source_table_json` and
`filtered_table_json` artifacts. A failed operation includes its error type,
complete message, phase and whether apply returned. Pandas errors occur during
apply; lazy Polars schema and membership errors occur during collect. Import
and control errors remain distinct. This route does not convert a table into
an EventLog.

Native helper and HTTP tests cover complete artifact transport, both engines,
literal controls, invalid sources and error phases. Frontend tests cover real
FormData construction, response validation, exact integer/download bytes,
request cancellation and operation selection. Rendered browser interaction
remains unverified. The pandas and Polars canonical modules remain partial;
see their between parity contracts for the wider dtype, object, warning and
physical-plan boundaries.
