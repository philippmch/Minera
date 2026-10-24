# Original Polars timestamp calls

The table timestamp workspace has a Polars call-mode selector. The friendly
form remains the default; "Use an original typed call" exposes all six public
functions of `algo.filtering.polars.timestamp.timestamp_filter`: `apply_events`,
`filter_traces_contained`, `filter_traces_intersecting`,
`filter_traces_attribute_in_timeframe`, `apply` and `apply_auto_filter`. The
module has no trace-starting or trace-completing functions.

## Wire

`POST /api/reference-table-timestamp` enters original mode when a `call_json`
part is present and `engine=polars`. It then accepts exactly `engine`,
`operation`, a genuine multipart `source_file` (native EventTable JSON) and
`call_json`. Friendly controls, duplicates and unknown parts are control
errors. The raw call bytes are captured before validation and echoed.

The descriptor `minera-polars-timestamp-original-call-v1` has exactly
`schema`, `source_kind` and `keyword_entries`. `source_kind` is `DataFrame`
(eager) or `LazyFrame`. The public functions take every operand after the frame
as a keyword, so `keyword_entries` holds `dt1`, `dt2` and, for the attribute
filter, `attribute` and `attribute_value`, plus an optional `parameters` entry.
Each required keyword appears once, in any order; the order is retained in the
call projection. Values use the retained Polars producer tags (str, int tokens
within the signed/unsigned 64-bit carrier, Python hexadecimal floats, bool,
None, date, datetime with fold/offset metadata, list/tuple/set and string-keyed
dictionaries).

Parameter projection reproduces the retained native mapping. For the four
filters, any dictionary becomes the ordered `PolarsTimestampFilterParameter`
vector, preserving enum versus string identity and order; other values use the
raw-value overload. The two helpers flatten enum keys to strings and receive
one raw value; they always raise their captured unavailable-method `Exception`.

A LazyFrame call constructs the native lazy plan first; schema and dtype
failures surface in the `collect` phase with `apply_returned` true. Immediate
parser, selector and parameter failures, and every eager failure, stay in the
`apply` phase. The 23-field `TABLE_TIMESTAMP_FILTER` response otherwise
matches the pandas original mode, including controls before and after and the
native call projection.

## Workspace

The editor imports call files as strict UTF-8 without rewriting BOM, CRLF or
integer tokens; changing the operation keeps imported or edited text. Examples
cover a LazyFrame with canonical string bounds, an eager DataFrame, typed UTC
datetime bounds, a string parameter mapping, ordered enum and string keys and a
raw parameter diagnostic. Every artifact and the complete response download
byte-exact; only a complete filtered table can be reused as the next source.

## Acceptance

ROOT admitted all three retained banks (732 calls) against their declared
whole-file pins before any decode, recomputed every canonical receipt and
rebuilt the unchanged 11.6 MB native vector oracle byte-exact with an
independent mapper. The generated transport fixture drives all 732 calls
natively, through the direct workbench and over an owned HTTP listener,
comparing all 23 fields, expected apply/collect phases, independently
serialized controls, complete frames and diagnostics, and artifact reopening.
An owned headless browser run covers every operation, eager and lazy sources,
typed and enum examples, helper, parser and lazy collect diagnostics, malformed
control text, BOM/CRLF/wide import with reordered keywords, held-response
races, filtered-table reuse, mobile layout and the preserved friendly form. No
reference call was made; the canonical module remains partial.
