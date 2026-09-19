# Typed dataframe formatting

The native formatter is available through `minera_format_dataframe` and
`POST /api/format-dataframe`. Both accept the same UTF-8 JSON request and use
the verified `format_dataframe` library operation.

```json
{
  "table": {
    "columns": ["case:concept:name", "concept:name", "time:timestamp"],
    "dtypes": ["str", "str", "str"],
    "rows": [
      [{"value": "C1"}, {"value": "A"}, {"value": "2024-01-01"}]
    ]
  },
  "options": {
    "start_timestamp_key": null
  }
}
```

Run a saved request:

```text
minera_format_dataframe request.json --output formatted.json
```

Omit `--output` to write JSON to standard output. Use `-` as the request path
to read standard input. `--help` shows the command syntax.

Send the same request to a running server:

```text
curl -H "Content-Type: application/json" --data-binary @request.json http://localhost:8484/api/format-dataframe
```

Options belong in the request body; query parameters are rejected. Supported
options use the public names `case_id`, `activity_key`, `timestamp_key`,
`start_timestamp_key`, and `timest_format`. Omitting them uses the library's
public defaults. `start_timestamp_key: null` disables start-column projection;
`timest_format: null` uses default date parsing. Unknown fields are rejected.

## Table and cell representation

The table shape matches the `table` object emitted by native log conversion.
`columns` and `rows` are required. Each row has exactly one cell per column.
Cells are attribute objects; JSON `null` represents an absent cell. A cell's
optional `key` must match its column. Its default `kind` is `string` and its
default `value` is an empty string.

For numeric, boolean, and date scalars, use `typed_value` with the matching
`kind`. `value` is the lexical projection; if provided alongside `typed_value`,
the two must agree. It can be omitted when `typed_value` is supplied.

```json
{"kind": "int", "typed_value": "18446744073709551615"}
```

Integer `typed_value` accepts either a JSON integer or a decimal string, within
signed/unsigned 64-bit bounds. Decimal strings preserve exact integers in
clients whose JSON number implementation uses floating point. Float
`typed_value` accepts finite double-precision numbers or the string tags `NaN`, `INF`, and
`-INF`. Boolean values use JSON booleans. Date values use strings. Explicit
null attribute objects use `"null": true` and cannot carry a non-null typed
value.

Typed date cells may also carry `date_resolution` (`"us"` or `"ns"`) and
`date_timezone` (a nonempty timezone identity such as `"Europe/Berlin"`).
Omitted resolution means microseconds; omitted timezone leaves the identity
unspecified. These fields require a non-null date `typed_value`. The timezone
name accompanies the resolved offset in the date string; it does not request
localization or timezone database lookup. Serialization emits nanosecond
resolution and present timezone identities while leaving ordinary date JSON
unchanged.

```json
{"kind":"date","typed_value":"2024-01-01T00:00:00+01:00","date_resolution":"ns","date_timezone":"Europe/Berlin"}
```

Optional table fields preserve declared `dtypes`, `categorical_dtypes`,
`index`, `attributes`, and `metadata`. Dtypes and categorical descriptors align
with columns. A categorical descriptor contains `categories_dtype`, `ordered`,
and an ordered `categories` array of attribute objects. Index metadata contains
`class`, `dtype`, `names`, and `labels`; names are strings or null. A scalar
label is `{"kind":"scalar","value":<attribute>}` and a tuple label is
`{"kind":"tuple","items":[<labels>]}`.

Dataframe attrs are carried in `metadata.properties` as ordered attribute
objects. Metadata also preserves native extensions, classifiers, trace/event
globals, and ordered global declarations. Attribute `container` values include
`scalar`, `list`, `container`, `tuple`, `set`, and `frozenset`; recursive values
use `children`. The native scalar-wrapper flags are retained. First-class
state/source fields require a non-`none` first-class role.

## Results and limits

A successful response has `family: "FORMAT_DATAFRAME"`, the formatted `table`,
and `input_after`. The latter exposes canonical-column mutations performed on
the input before the returned table is formed. HTTP returns 200 and the CLI
returns exit status 0.

A formatting rejection has `family`, an `error` string, and `input_after`;
HTTP returns 400 and the CLI returns exit status 1. Invalid JSON or schema data
is rejected before formatting: HTTP returns an error object, while the CLI
writes the error to standard error and exits 1. File read/write failures also
return a nonzero CLI status.

Requests are limited to 32 MiB, 64 nested levels, and one million JSON values.
The decoder rejects duplicate object keys, invalid UTF-8, malformed numbers,
and out-of-range typed integers. Non-string column labels remain outside the
native table model. These interfaces do not infer pandas dtypes from CSV or
provide mixed XES/CSV formatting. Parser and extension-dtype parity remain
bounded by the public evidence documented in `reference-2.7.22-parity.md`.

`scripts/check-format-dataframe-transports.py` validates the pinned evidence
manifest, compares all 258 representable public cases through both interfaces,
checks transport failures, and hashes both executables before and after replay.
