# Original LOG timestamp calls

Open **Filter a CSV, XES, or native log**, choose **Event log**, then
**Original timestamp calls**. Upload a complete native EventLog JSON file.
The example download includes stored native dates and trace properties.

Choose an operation and load its example, or import an original-call descriptor.
Imported and edited text stays unchanged when the operation changes. Loading
an example explicitly replaces the editor. File import preserves UTF-8 BOM,
line endings and exact integer tokens; the current-call download and submission
use the same bytes.

| Operation | Non-source positional operands | Return |
| --- | --- | --- |
| `apply`, `apply_auto_filter` | none | native helper diagnostic |
| `apply_events` | dt1, dt2 | log |
| `filter_traces_contained`, `filter_traces_intersecting` | dt1, dt2 | log |
| `filter_traces_starting_in_timeframe`, `filter_traces_completing_in_timeframe` | dt1, dt2 | log |
| `filter_on_trace_attribute` | dt1, dt2 | log |
| `filter_traces_attribute_in_timeframe` | attribute, value, dt1, dt2 | log |
| `is_contained`, `is_intersecting` | dt1, dt2, timestamp_key | Boolean |
| `trace_attr_is_contained` | dt1, dt2, trace_attribute | Boolean |
| `has_attribute_in_timeframe` | attribute, value, dt1, dt2, timestamp_key | Boolean |

Predicates require `source_trace_index` and use that actual trace from the
uploaded log. Their example bounds are UTC-aware Packed datetimes. Time-filter
examples use string bounds. A Boolean false is displayed explicitly and has
its own downloadable typed native cell; it creates no filtered log.

## Request contract

`POST /api/reference-log-timestamp` takes exactly three unique multipart parts:
`operation`, genuine native `source_file`, and raw `call_json`. Query controls,
unknown parts and duplicates are rejected. Text controls may also be canonical
byte Files named `operation.txt` and `call_json.txt`.

The descriptor has exactly `schema`, `args`, `keyword_entries` and
`source_trace_index`. Its schema is `minera-log-timestamp-original-call-v1`.
Args exclude the separately uploaded source. Keyword entries have ordered
`key`/`value` fields. Trace index is null for log operations. Duplicate JSON
object keys, duplicate keyword names and wrong positional arities are rejected.
Descriptor bytes require strict UTF-8; one leading BOM is accepted without
changing the retained raw text. The shared NativeString transport preserves
native diagnostic bytes, including decoded escaped-string fallback domains.

The 18-tag Packed grammar retains unused metadata. Native attribute projection
supports the 15 scalar/container tags; enum/log/trace values cannot become
positional native operands or an invocation source. The seven filters receive
an actual caller-owned `LogTimestampFilterOptions`, including when parameters
are omitted or non-dictionary. Ordered timestamp and positive entries assign
its members. Enum timestamp and literal timestamp selectors remain separate.
Unused entries remain descriptor metadata. The two source-only helpers invoke
their native default-argument form; the compiler's default temporary is marked
`implicit_default_not_inspected`. Predicates receive explicit operands and no
Options object.

## Results and downloads

Every response contains the same 25 keys:

```text
family operation http_status phase source_format source result source_unchanged
call_mode call_json controls_before_json controls_after_json controls_unchanged
source_log_json source_after_log_json filtered_log_json filtered_xes xes_error
value_json error_type error native_returned result_kind call_projection_json
inspection_error
```

The family is `REFERENCE_LOG_TIMESTAMP`. Phases are `control`, `source_import`,
`native_call`, `result_export` and `complete`. `native_returned` changes only
after a normal native return. `result_kind` is then `log` or `bool`. Native
errors preserve their complete type and message with HTTP 400. Validation and
import failures use `control_error`. Internal capture/export failures use 500;
an inspection failure is recorded separately and preserves a primary native
diagnostic. Unavailable before/after comparison is null.

Actual controls use `minera-log-timestamp-native-controls-v1`, with exactly
`schema`, `source_argument`, `arguments` and `options`. Snapshots serialize the
same actual source argument, stable operands and explicit Options passed to
the native function. Optional enum selector absence is null. A predicate's
one-trace raw-log wrapper is solely an inspection artifact. The independent
source download always contains the full backing log. Comparisons describe
serialized native values, without implying Python object identity.

The native projection artifact records the function, argument origins, selected
source and projected/unprojected entries. It has 14 fields and schema
`minera-log-timestamp-call-projection-v1`. Its scope is
`retained_native_mapper_construction`.

Every available artifact remains downloadable on errors, including mutations
that precede a native exception and partial artifacts on internal failures.
Only an HTTP 200, complete, normally returned log with no error or inspection
failure offers **Use returned native log as source**. This reuses the genuine
native File bytes. XES is secondary and may project stored values; a separate
XES failure preserves the native log result.

The retained 342-call bank covers 13 functions: 135 logs, 48 Boolean returns,
159 native diagnostics and 38 source/control mutations. Its native mapping and
transport acceptance concern the bounded native construction. Python qualified
error modules/args, identities and aliases, warning machinery, unobserved
domains, complete history and catalogue closure remain separate requirements.
