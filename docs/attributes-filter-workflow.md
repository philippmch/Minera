# Attribute filtering workflows

The event-log, pandas and Polars filter workspaces expose the pinned public
attributes functions through real file uploads. Choose **Event and trace
attributes** in the event-log family chooser, or **Attributes** in the table
workspace. Existing filters remain available in their original families.

## Event logs

`POST /api/reference-log-attributes` accepts CSV, XES (including gzip) or a
complete native log JSON File. The eight operations are whole-case/event value
matching, whole-case/event numeric matching, trace attribute matching, maximum
activities, relative occurrence and supplied-cache threshold filtering.

Values can be a literal JSON string array or a repetition-style typed object.
Numeric bounds retain their original text and support the same typed objects.
Attribute names, case selectors, stream selectors and glue remain literal,
including empty strings, whitespace, Unicode and NUL bytes. Only active controls
are submitted. Numerical whole-case filtering supports two ANDed stream
conditions and its separate case lookup glue. Event reconstruction uses full
event case names; numerical cases instead use an unprefixed trace key.

Converter-bearing operations can copy trace attributes into parsed source
events, including before a later error. Maximum activities can also rewrite
the parameter dictionary before an error or an unchanged-source return. Every
response retains complete source and parameter snapshots before and after the
call. CSV converter inputs receive a conventional canonical trace identity
before the first snapshot while retaining their original CSV roles.

Relative occurrence distinguishes omission from explicit true and false.
Omission counts events over all source traces, including empty traces. True
counts cases containing the value over all traces; false counts events over all
events. Maximum-activity ranking retains first-seen order for equal counts.

The threshold helper requires a separate genuine native-log File containing
the actual cached traces. `counts_json` and `variant_traces_json` are typed lists
of typed key/value pairs. Map values are typed lists of zero-based trace offsets
into that auxiliary log. `variant_counts_json` is the supplied typed ordered vc
list. The first actual cached trace supplies activity exemptions; its contents
are independent of the variant key. No cache is inferred from the source or
from the key. Complete auxiliary snapshots and exact supplied JSON remain
downloadable on success and errors.

The strict 26-field response distinguishes the filtered attribute from the
original activity role for reopening. Complete native results, XES, source,
parameter/cache artifacts and raw response bytes can be downloaded. XES export
errors retain the complete native result. Opening uses the actual exported XES
and original activity role. When uploading generic XES with a custom activity
attribute, select that literal reopening role explicitly; XES does not retain
arbitrary native first-class role metadata.

## Tables

`POST /api/reference-table-attributes` accepts a genuine complete native
EventTable JSON File. Both engines expose eight public modes; Polars additionally
exposes typed value counts. The count result is a separate two-column
`value`/`count` table, preserving typed keys without string coercion. It is never
represented as filtered source rows. Full source snapshots, table/count results
and response bytes remain downloadable, including source state on errors. These
artifacts can use the complete raw-byte EventTable carrier; reupload the actual
exported JSON to preserve every cell type and stored field.

Literal string lists, explicit typed collections, exact numeric bounds, two
whole-case stream conditions and supplied count dictionaries are supported.
Threshold controls preserve omitted versus explicit None caches and supplied
most-common variants. Typed key/count pairs support non-string dictionary keys;
a typed map supports literal string keys. Dictionary normalization preserves
first equal-key position and the last count.

Relative occurrence distinguishes an omitted counting mode from explicit true
and false. pandas defaults to once per case. Polars omission uses event counts
over distinct cases; explicit false uses event counts over event count, while
true uses distinct case/value pairs over cases. Case selectors are inactive in
explicit false mode. Counts use the case selector only for explicit true.

pandas' valid integer activity-limit helper returns the original frame in the
captured behavior; the UI describes that behavior directly. Polars top-activity
selection retains all higher-frequency groups and complete source rows. Exact
cutoff tie identity remains an open boundary: nine retained ambiguous results
are admitted only by the documented bounded cutoff predicate. Count-map entry
order is also unclaimed.

## Evidence and remaining scope

Read the three engine contracts for exact semantics and evidence bindings:

- [Log](log-attributes-filter-parity-contract.md): 280 distinct public calls.
- [pandas](pandas-attributes-filter-parity-contract.md): 448 distinct public calls.
- [Polars](polars-attributes-filter-parity-contract.md): 726 distinct public calls.

All 1,454 public calls were executed once. Continuations reconstructed and
asserted complete prior receipt hashes before new entries. Original outcomes,
errors, warnings, source/control mutations and failed native attempts remain
retained. No reference implementation source, AST, bytecode or private state was
inspected. These bounded banks leave all three canonical module rows partial.
The full catalogue remains in scope. Rendered browser interaction is unverified.

The final build passes 41 assertion-enabled native suites, 95 actual production
HTTP checks and 190 frontend files / 9,809 tests, plus 65 focused checks,
TypeScript and Vite. The root workflow archive binds current sources, compiled
artifacts, original failures, all four HTTP attempts and the engine archives.


## Native multipart byte controls

Literal controls with CR/LF or lone UTF-16 surrogates are sent by the frontend
as byte-preserving Blob parts with the exact `<field>.txt` filename. Both
attributes workbenches accept that canonical byte-field representation, as well
as ordinary multipart text. Other File-shaped ordinary controls are rejected.
The source and auxiliary log uploads still require actual Files, including an
explicitly present empty filename. Native and owned HTTP regression checks
exercise the same byte-field representation used by the frontend.


The focused byte-control checkpoint passes all four assertion-enabled attributes
workbench/owned-HTTP suites in
`build/parity-closures/native-current/ctest-attributes-byte-controls-first.log`.
Evidence is packaged in `docs/attributes-native-byte-controls-checks.jsonl.gz`.
This focused archive records the initial native acceptance. Additional evidence
in `docs/attributes-native-byte-controls-production-checks.jsonl.gz` records 101
passing actual production HTTP checks, including complete raw-byte log/table
artifacts and canonical frontend Blob controls with CR/LF, NUL and lone
surrogates. The test server was compiled from an unmodified copy of committed
`src/backend/tools/minera_server.cpp` at `9b983512`, with the 58 unchanged current
native objects and pinned frozen core. It used an independently owned temporary
port and exited. Local unfinished LTL endpoint edits were excluded from this
build. The server SHA256 is
`22e6635a92e06bee3d97a7678572627093c007aecca0c1110072c78c19a18e1a`.

The first two additional HTTP harness attempts stopped after 73 checks because
the result reader assumed the ordinary log shape, then incorrectly expected a
schema marker in the raw-byte log carrier. Both original scripts, responses,
failures and owned-server exit receipts are retained. The third passed 101;
the final fourth adds complete raw-byte filtered-log equality and also passes
101. The product binary stayed unchanged throughout. Rendered browser
interaction remains unverified. Full LTL acceptance is pending separately.
