# Complete case filtering

The existing filtering workspace offers case event-count, case-limit and
duration operations for event logs and complete pandas/Polars native tables.
Upload CSV, XES/XES.GZ or native log JSON in log mode; table modes accept a
genuine native EventTable JSON File with its schema, index and dictionaries.
Engine-specific example files are available alongside the upload controls.

Bounds stay lexical until the native engine receives them. Integers retain
their precision; decimal, boolean, null, NaN and infinity scalar tokens are
also available for public diagnostic behavior. Size bounds are inclusive,
with null meaning no upper bound. Count filters use their engine's public
limit behavior, including differences for negative and nonintegral limits.
Column and timestamp selectors preserve literal empty strings, whitespace,
Unicode, newline and NUL.

Log filters preserve complete selected traces and events but clear all
log-level context, matching the observed public module. Duration uses the
first and last trace event. CSV defaults follow its configured timestamp
column; XES/native logs default to `time:timestamp`. Explicit ordinary strings
remain strings. Log results retain native JSON and XES downloads and can be
reopened with their original activity selector. Native-to-XES projection
limitations remain visible in the existing result inspection.

Pandas duration uses first/last observed timestamps per case. Polars uses
minimum/maximum timestamps and integral elapsed seconds. Case limits rank
groups by frequency; Polars boundary ties have unspecified public selection
order and can include a null group that consumes a slot before disappearing
in the join. The native helper chooses encounter order among equal counts.
This deterministic native choice is documented as an admitted tie result,
not proof of exact selected-case ordering. Both table engines retain complete
selected source rows and their supported metadata. Pandas business hours use
the default weekday schedule; Polars reports its observed unsupported error
when that option is enabled.

`POST /api/reference-table-cases` accepts multipart `engine` (`pandas` or
`polars`), `operation` (`size`, `count`, `performance`) and `source_file`.
Size/performance use `minimum` and `maximum`; count uses `max_cases`.
Optional `case_id_key` is literal. Performance additionally accepts literal
`timestamp_key` and boolean `business_hours`. Unknown, duplicate and
inapplicable fields are rejected. Custom business schedules remain available
through the native helpers, outside these initial workspace controls.

The `TABLE_CASE_FILTER` response includes source/result counts, complete
`source_table_json` and `filtered_table_json`, input-preservation status,
selected roles and operation, full error type/message, phase and whether
apply returned. Polars apply and collect failures remain separate. Download
the complete result table, parsed source or exact response bytes; changing
any active control, source, operation or engine invalidates stale requests.

Log operations use the existing `POST /api/reference-log-filter` route with
`case_size`, `case_count` or `case_performance` as `operation`, the matching
bound fields above and an optional `timestamp_key` for duration. Existing
source-format/CSV controls still apply. Activity selection and positive-mode
controls are inapplicable to these complete-case filters.

The three module contracts describe retained black-box observations and
native acceptance separately. Arbitrary Python objects and alias graphs,
unobserved dataframe schemas/dtypes, warnings and physical execution plans
remain in full scope. Browser interaction remains unverified under the
previously recorded permission boundary.

The coordinated checkpoint passes 19 assertion-enabled native suites and 42
requests against the freshly rebuilt production server on an independently
owned temporary listener. That server exited without replacing the existing
application. Frontend acceptance passes 183 files / 9,718 tests, TypeScript and
Vite. The source/binary/receipt bindings and complete HTTP records are retained
in `docs/case-filter-workflow-checks.jsonl.gz`; earlier failed attempts remain
separate evidence. Rendered browser interaction is not claimed by these checks.
