# Typed table repetition in the existing filter workspace

The original log query workspace opens the filter workspace, whose data-model
selector now offers Event log, pandas dataframe repetitions and Polars lazy
dataframe repetitions. The log operations and their response contracts remain
unchanged. Table repetition uses separate native companions and never converts
an EventTable into an EventLog.

Upload a complete native EventTable JSON File. Both the strict table schema
and the byte-safe `event-table-raw-v1` fallback are accepted by
`parse_embedding_workbench_table_json`. Serialization retains explicit positive
`int64` versus `uint64` storage in cells, attributes, index labels, category
declarations and metadata. Unsafe strings and compatibility snapshots use the
complete raw constructor carrier. This is not a CSV inference/import path.

Each engine has a downloadable example with its own dtype spellings. Both
examples contain four rows, canonical case/activity keys, a signed integer
above 2^53 and uint64 maximum, with explicit storage tags. Default target A and
bounds 2 through 9223372036854775807 retain all three rows of case c1. Examples
use string numeric payloads with explicit integer storage, avoiding JavaScript
number rounding. Complete results can be downloaded and reuploaded to the same
engine.

## Transport and output

`POST /api/reference-table-repetition` requires multipart `source_file`,
`engine=pandas|polars` and `target_json`. Optional literal `case_id_key` and
`attribute_key` default to `case:concept:name` and `concept:name`; omission is
distinct from an explicit empty key. `min_rep` and `max_rep` default to 2 and
9223372036854775807. Their scalar tokens are sent unchanged, including arbitrary
exact decimal integers, fractions, bool, null, strings and nonfinite tags.
Container-valued bounds are outside this transport contract. Unknown or
duplicate controls, query-supplied source files and text placeholders for the
actual source File are rejected.

Targets reuse the typed repetition JSON controls. Examples are
`{"type":"string","value":"A"}`,
`{"type":"integer","value":"9007199254740993"}`,
`{"type":"decimal","value":"NaN"}`, and `{"type":"null"}`.
Bytes use hexadecimal data, for example `{"type":"bytes","value":"ff00"}`;
empty bytes are valid, odd-length/nonhex values are rejected, and bytes remain
distinct from strings. Boolean, datetime, native NA and typed container values
retain the existing target schema. Whether a target comparison is supported
and what it selects depend on the engine and the declared column dtype.

The independent `TABLE_ATTRIBUTE_REPETITION` envelope has engine, HTTP status,
phase, `apply_returned`, source/result row-and-column counts, source preservation,
literal selectors, complete `source_table_json`, complete
`filtered_table_json`, native error type and error message. Parsed-source
artifacts remain available after native apply/collection errors. Diagnostics
use full length-aware native error-message carriers, retaining embedded NUL
and multiline plans rather than truncating at `what()`'s C-string boundary.
Control/import failures are separate from engine calls.

For pandas, native apply failures report `phase=apply` and
`apply_returned=false`. For Polars, the table is wrapped in a restartable
LazyEventTable, the lazy companion is invoked, and its returned producer is
explicitly materialized. Deferred failures report `phase=collect` and
`apply_returned=true`; construction failures preserve the companion's apply
phase. The UI exposes these fields alongside error type/message. It never
presents a deferred error as a successful filtered table.

Downloads contain the complete parsed source table, complete result table and
original response bytes. The source File itself is uploaded unchanged. Changing
a source, selector, target, bound or engine invalidates the result and cancels
the request; leaving or switching the child workspace prevents stale responses
from replacing current state. Table output is not projected through XES or
opened as an activity log.

## Engine boundaries

pandas retains source index labels, categorical declarations, dtypes, table
attributes and metadata. Its matching-case eligibility differs from the log
companion: minimum zero does not admit cases with no matching row. Its native
contract and retained observations are documented in
`pandas-attribute-repetition-parity-contract.md`.

Polars requires declared dtype names such as String, Int64 and Float64 and has
no dataframe index; indexed tables are rejected without dropping the index.
Its primitive/categorical/temporal native domain and deferred error behavior
remain those of its separate companion. Category dictionary internals and
physical plans are outside that contract. The finite table representation does
not expose arbitrary Python objects or lazy external data sources. This usable
workflow does not promote either canonical module or the whole package to full
parity.

Validation uses native complete-carrier comparisons and real owned ephemeral
HTTP/multipart requests, plus frontend request helpers with HTTP doubles and
SSR control/result rendering. These checks do not establish rendered browser
events/effects or replacement of the user's running application.


Final acceptance passes **nine native suites** and **9,702 frontend tests in
181 files**, with TypeScript and the production Vite build. The final owned
production server passes **17 table requests and 17 log requests**, including
full native artifacts and reupload, engine errors, integer storage, literal
keys and real binary byte targets. Both final HTTP receipts bind the same
final server binary. The servers exited and the existing app was not replaced.

The initial default-timeout frontend run retained one unrelated EMD fixture
exceeding its five-second test limit during concurrent compilation. No source
or tests were changed to address it. The final suite passed with explicit
`--testTimeout 30000`; the initial failure, earlier passing suite, native
failure/correction logs and first server captures remain archived separately
from final receipts. Passing unit/SSR/owned HTTP checks does not establish
rendered interaction in the running application.

`table-attribute-repetition-workflow-checks.json.gz` contains **56 members**,
**154,979 bytes**, SHA-256
`936dea2d24364de20192206b16231cc79f1924efa288ce0b4b4ae61b675a422b`.
It retains the acceptance scripts, complete original response records,
compiler/test logs and current native/frontend sources. Each member records
its original byte count and hash; **21 source bindings** match the accepted
contents. The archive is deterministic and all members/bindings were validated
offline without any reference calls.
