# Faithful log filter workflow

The original log query workspace now opens a log filtering workspace. It
accepts an actual CSV, XES (including gzip), or native log JSON File and
applies one of seven native filters: trace sequences, prefix, suffix, start activity,
end activity, rework, or exact typed attribute-value repetition. The first six
retain their prior contracts; repetition has its own bounded partial-module
contract in `log-attribute-repetition-parity-contract.md`.

The HTTP endpoint is `POST /api/reference-log-filter`, with multipart
`source_file` and `operation`. The remaining controls are literal values.
The default activity selector is the CSV activity column, or `concept:name`
for XES and native logs. An explicitly supplied empty selector is distinct
from omission. CSV columns, delimiter and trimming are configurable; the
existing CSV importer still requires a nonempty activity. Native logs retain
the full event/trace/log carrier and can represent empty or missing fields.

Trace sequences and admitted labels are JSON arrays, preserving commas and
empty strings. Prefix/suffix expose strict inclusion and first/last
occurrence. Rework accepts exact integer thresholds, fractional values,
nonfinite values and the native lazy type-error controls. All filtering
leaves the parsed source unchanged. Importing a CSV/XES file still follows
the existing importer contract; this workflow does not promote those
importers or promise arbitrary external object representation.

Attribute-value repetition uses `operation=repetition`, required `target_json`
and optional `min_rep` / `max_rep` (defaults 2 / 9223372036854775807). The literal
counted key travels as `activity_key`; omission uses the CSV activity column
or `concept:name` for XES/native sources.
There is no positive/negative polarity. Both bounds are inclusive, missing
cells are skipped, zero counts can match, and reversed ranges return no cases.
The UI transmits exact bound tokens without JavaScript number conversion.
Null/string bounds retain lazy native comparison errors when reached.

Typed targets use `{"type":"string","value":"web"}`;
`integer` accepts a decimal string or exact JSON integer, `decimal` accepts a
number or `NaN` / `INF` / `-INF` string, and `boolean` uses a JSON boolean.
`null` and `na` omit value. `date` takes an ISO datetime with nanosecond
precision. `list`, `tuple`, `set` and `frozenset` take arrays of typed objects;
`map` takes an object of typed values. Sets require immutable members. For
example, `{"type":"tuple","value":[{"type":"integer","value":"9007199254740993"},{"type":"null"}]}`.
NativeNA scalar comparisons report ambiguous-truth errors; nested matching
singleton values preserve the observed container behavior. Literal string
values and keys support empty strings, Unicode, NUL and newlines. The key
editor is a textarea so multiline keys remain expressible.

Only repetition adds response `filter_attribute_key` for the counted key.
Its response `activity_key` remains the original source activity key, so
counting `channel=web` still opens activity labels such as A/B. These keys are
nullable on import/control failures. Other operation envelopes are unchanged.

The response includes source/result counts, source preservation, the complete
filtered native log JSON and an XES export. A failed XES export leaves the
successful native result available and reports `xes_error`. The UI can
download both formats or hand the actual XES File and the selected activity
key to the normal log-opening workflow. CSV exports retain their original
activity attribute name, so the handoff must pass that selector explicitly.
Native-source results label this action as opening an XES projection and
explain that XES converts some native types to text; the native JSON download
retains the complete typed result. Literal controls needing byte preservation
travel as multipart File fields; only the source itself must be an uploaded
log File.

Editing controls or replacing the source invalidates the displayed result
and cancels any active request. Cancellation and component unmount prevent
older responses from replacing the current result. This application feature
preserves the six previously closed native filter modules. The repetition
companion adds usable bounded behavior while its canonical module remains
partial; it does not add another strict module closure. Rendered application interaction remains a separate
verification requirement.

Validation: the assertion-enabled native workflow test and actual ephemeral
HTTP/multipart test passed. HTTP checks exercise all six operations, exact
native JSON, XES reimport/reupload with the original activity key, CRLF Blob
controls, filename presence, typed projection and error envelopes. The full
frontend suite passed 9,651 tests before the final handoff/projection fixes;
the final focused four suites passed 67 tests. TypeScript and the final
production build passed. The server translation unit was freshly compiled
with both new routes; the existing running app was not replaced by this step.


The repetition continuation passed all 109 retained public outcomes/errors,
three feature native suites (module, workbench and real HTTP route), four
existing filter regression suites, and fourteen requests against a freshly
built owned production server. Those production requests cover zero/reversed
and lazy ranges, exact integer neighbors above 2^53, missing cells/empty
traces, native NA errors, duplicate/bad controls, complete returned artifacts,
original opening activity keys, and NUL/newline/Unicode key and value transport.
The full frontend suite passed **9,688 tests in 180 files**; TypeScript and the
production Vite build passed. The owned server exited and the existing app was
not replaced. Request/UI helper tests use HTTP doubles and SSR; rendered
browser events/effects and the running application were not exercised.

Repetition evidence is retained in `log-attribute-repetition-public.json.gz`
and `log-attribute-repetition-workflow-checks.json.gz`, including the frozen
public recipes/receipts, complete native outcomes, source bindings, compiler
and test logs, and actual production HTTP request/response records.


The data-model selector also opens independent pandas and Polars native-table
repetition workspaces. Their complete EventTable input, engine-specific dtype
examples, lazy collection errors and table downloads are documented in
`table-attribute-repetition-workflow.md`. The log envelope and log opening path
retain their existing behavior. Typed repetition targets additionally accept
`{"type":"bytes","value":"ff00"}` hexadecimal data, including empty bytes;
odd-length/nonhex encodings are rejected.
