# Original pandas timestamp calls

The table timestamp workspace has a pandas call-mode selector. The friendly
form remains the default; "Use an original typed call" exposes all eight public
functions of `algo.filtering.pandas.timestamp.timestamp_filter`: `apply_events`,
`filter_traces_contained`, `filter_traces_intersecting`,
`filter_traces_starting_in_timeframe`, `filter_traces_completing_in_timeframe`,
`filter_traces_attribute_in_timeframe`, `apply` and `apply_auto_filter`.
Polars timestamp calls keep the friendly form until their own bank is admitted.

## Wire

`POST /api/reference-table-timestamp` switches to original mode when a
`call_json` part is present. It then accepts exactly `engine=pandas`,
`operation`, a genuine multipart `source_file` (native EventTable JSON) and
`call_json`. Friendly controls (`lower`, `upper`, roles, `positive`,
`keep_nan_values`, `attribute_value_json`), duplicates and unknown parts are
control errors. The raw call bytes are captured before validation and echoed,
including malformed text; a leading BOM is tolerated.

The descriptor `minera-pandas-timestamp-original-call-v1` has exactly
`schema`, `args` and `keyword_entries`. `args` hold the producer-packed
operands after the DataFrame with the exact public arity: none for the two
helpers, `dt1, dt2` for the window filters and `attribute, attribute_value,
dt1, dt2` for the attribute filter. At most one keyword entry, `parameters`,
is accepted. Packed values use the retained producer tags (str, int tokens of
any width, Python hexadecimal floats, bool, None, pandas NA/NaT/Timestamp,
datetime, bytes, list/tuple/set/frozenset and ordered dict items). Enum keys
must name this module's public `Parameters` members with their exact value.

Parameter projection reproduces the retained native mapping: omitted is a
null value; None and raw values are passed as one typed value; a dictionary of
string keys is one container value; a dictionary containing an enum key becomes
the ordered `PandasTimestampFilterParameter` vector, preserving enum versus
string identity and order. The two helpers only accept a raw value and always
raise their captured unavailable-method `Exception`.

The 23-field `TABLE_TIMESTAMP_FILTER` response retains engine, operation,
status, phase (`control`, `source_import`, `apply`, `result_export`,
`complete`), whether apply returned, source/result counts, complete source
before and after, the filtered table, full error type and message, the exact
call, actual controls before and after with their comparison, and the native
call projection. Friendly role fields are null in original mode.

## Workspace

The editor imports call files as strict UTF-8 without rewriting BOM, CRLF or
integer tokens; changing the operation keeps imported or edited text. Examples
cover canonical string bounds, typed UTC Timestamp bounds, a string parameter
mapping, ordered enum and string keys and a raw parameter diagnostic. Every
artifact and the complete response can be downloaded byte-exact. Only a
complete successful filtered table can be reused as the next source. Source,
operation or call edits and unmounting cancel pending requests.

## Acceptance

ROOT admitted both retained banks (861 calls) against their declared whole-file
pins before any decode, recomputed every canonical receipt and rebuilt the
unchanged monolithic native oracle byte-exact with an independent mapper. The
generated transport fixture drives all 861 calls through the native API,
the direct workbench and an owned HTTP listener, comparing all 23 fields,
independently serialized controls, full tables and diagnostics and artifact
reopening. An owned headless browser run covers every operation, typed and
enum examples, helper and parser diagnostics, malformed control text, exact
BOM/CRLF/wide import, held-response cancel/edit races, filtered-table reuse,
mobile layout and the preserved friendly form. No reference call was made;
the canonical module remains partial.
