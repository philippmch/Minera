# Faithful log filter workflow

The original log query workspace now opens a log filtering workspace. It
accepts an actual CSV, XES (including gzip), or native log JSON File and
applies one of the six independently validated native filters: trace
sequences, prefix, suffix, start activity, end activity, or rework.

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
uses the previously closed native filter modules and does not add another
strict module closure. Rendered application interaction remains a separate
verification requirement.

Validation: the assertion-enabled native workflow test and actual ephemeral
HTTP/multipart test passed. HTTP checks exercise all six operations, exact
native JSON, XES reimport/reupload with the original activity key, CRLF Blob
controls, filename presence, typed projection and error envelopes. The full
frontend suite passed 9,651 tests before the final handoff/projection fixes;
the final focused four suites passed 67 tests. TypeScript and the final
production build passed. The server translation unit was freshly compiled
with both new routes; the existing running app was not replaced by this step.
