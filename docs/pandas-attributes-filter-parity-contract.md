# Pandas attributes: bounded public/native contract

## Original typed calls in the table workbench

`POST /api/reference-table-attributes` accepts `engine=pandas`, an existing
operation, a complete native EventTable `source_file` File and `call_json`.
The presence of `call_json` selects original mode, including malformed or empty
text. Friendly controls are rejected by presence in that mode. Polars continues
to use friendly controls. Ordinary text and a canonical `call_json.txt` byte
File are accepted; duplicate fields and unrelated File names reject.

The call schema is `minera-pandas-attributes-original-call-v1`. Required `args`
is an ordered array of typed operands excluding the source. This is the binding
shape of the complete retained bank, rather than a general Python call binder.

| Operation | Public function | Args | Optional controls |
| --- | --- | --- | --- |
| `traces` | `apply` | values | parameters |
| `events` | `apply_events` | values | parameters |
| `numeric_traces` | `apply_numeric` | lower, upper | parameters |
| `numeric_events` | `apply_numeric_events` | lower, upper | parameters |
| `relative` | `filter_df_relative_occurrence_event_attribute` | stake | parameters |
| `threshold` | `filter_df_keeping_activ_exc_thresh` | threshold | direct keywords |
| `top_activities` | `filter_df_keeping_spno_activities` | empty | direct keywords |
| `on_values` | `filter_df_on_attribute_values` | values | direct keywords |

Five wrappers accept optional `parameter_value` or `parameter_entries`, exclusive
by presence. Entries are ordered `{key,enum_key,value}` objects. Key is one of
the eleven public underlying parameter strings; enum_key is Boolean. A string
key and its enum key remain distinct and may occur in either order. A duplicate
exact identity rejects. All ten captured collision calls retain the four apply
wrappers' enum precedence and relative occurrence's literal precedence. Omitted
parameters, explicit null, empty list and an empty dictionary stay distinguishable
in the controls artifact even when their native outcomes coincide.

Direct helpers instead accept optional ordered `keyword_entries` objects with
`key` and typed `value`. Threshold allows `act_count0`, `activity_key` and
`most_common_variant`; activity-limit allows `activity_key` and
`max_no_activities`; on-values allows `case_id_glue`, `attribute_key`, `positive`
and `keep_nan_values`. Unknown or duplicate keywords reject. Defaults are used
only for omitted controls. The activity-limit public signature has no positional
maximum in this bank: supply it as `max_no_activities`.

Threshold count dictionaries use
`{"type":"attribute_counts","entries":[[TypedKey,TypedCount],...]}`.
Typed keys, order, empty dictionary presence and entry boundaries are preserved.
The unchanged native normalizer handles Python-equal keys. Null, list and string
count inputs remain raw typed values and retain native diagnostics.

Common typed operands use `null`, `na`, `string`, `integer`, `decimal`, `boolean`,
`bytes`, `date`, `list`, `tuple`, `set`, `frozenset` and `map`. Integer strings and
raw integer tokens avoid a floating intermediary. Maps retain the existing
codec's lexical-order constraint; every captured ordinary operand map meets it.
The private recursive `pandas_timestamp` descriptor has exactly `type`, ISO
`value`, `resolution` (`ns` or `us`) and `timezone` (null or `UTC`). It preserves
the eight captured Timestamp operands' native units and zone, including nested
values and count keys. A microsecond value with finer precision rejects. The
shared `date` codec remains unchanged. Other Timestamp units, arbitrary timezone
objects, nested dataframe controls and arbitrary Python objects are unproven.

The response has 23 fields: the previous eighteen plus `call_mode`, exact
`call_json`, `controls_before_json`, `controls_after_json` and
`controls_unchanged`. Both control artifacts serialize actual parsed native
operands before and after invocation, including on native error. They contain
`args_json`, `parameter_state`, `parameters_json` and ordered `keyword_entries`.
Native cells use complete EventTable serialization; count dictionaries retain
their union tag and separate typed key/value cells. These are actual native
state snapshots, separate from the raw call echo. The source and result artifacts
continue to retain full rows, index, dtypes, categories and attributes. Original
`case_id_key` and `attribute_key` are empty placeholders; inspect typed controls
for selectors. Friendly role fields retain their existing meaning.

The pandas editor imports valid UTF8 without normalizing BOM, CRLF, whitespace
or integer tokens. Edits invalidate pending requests and deferred imports. Exact
call, complete controls, full native tables and raw response bytes can be
downloaded, including available evidence after errors. A successful filtered
table can become the next actual source File. Error type/full message are native
wire outcomes; original Python error.args and object-identity metadata remain
external observations.

The bounded offline census reconstructs all 448 canonical receipt IDs from both
complete admitted banks, checks full plan/case/intent correspondence and observes
335 full tables, 113 diagnostics, zero warnings and zero source/control mutations.
Sorted receipt-set SHA256 is
`4c833bf5516f0170a47ade0d1b1959054e2b7574fe9ea9914ee69c867a55cfdf`.
Independent mapping reconstructs all 150 snapshot constructors and all 448 case
bodies in the byte-unchanged native oracle. Five omitted/null invocation pairs
keep distinct receipt and transport identities. This census and mapping establish
offline evidence; current transport runtime acceptance is recorded separately.
The first census validator's incorrect frame-class assumption was corrected to
the observed `pandas.DataFrame`; the failed generation is preserved. No reference
calls were made or replayed for this work, and module/catalogue closure remains
open.

The dedicated `pandas_attributes_filter.hpp` companion implements all eight
captured public functions of `algo.filtering.pandas.attributes.attributes_filter`,
pinned to PM4Py 2.7.22 commit `0715c31f3f1d85924107d6aa5329806db69df899`,
pandas 3.0.6 and Python 3.11.9. The opaque module SHA-256 is
`bd6cfa6c34e3f1fc6ed816fb11dbd118a0819139dca0886151602d0fde223169`.
The canonical module remains **partial**. Existing convenience and repetition
filters retain their own contracts.

## Event and complete-case filters

`filter_pandas_attributes` and `filter_pandas_attributes_on_values` select whole
cases containing a matching attribute value. Cases can be noncontiguous; output
retains source row order. `filter_pandas_attributes_events` selects matching
individual events. Typed values remain literal, including whitespace, empty
strings, bytes, Boolean/numeric equivalence, exact signed and unsigned integers,
nested list/tuple cells and nanosecond Timestamp values. None, pandas NA and NaN
remain distinct membership targets. Lists, tuples, sets, frozen sets and mapping
keys provide outer membership collections; scalar values retain exact `isin`
diagnostics. Explicit None yields no selected cases in the two case functions,
while the event function rejects it.

Positive applies ordinary captured truthiness. For these membership functions,
`keep_nan_values=True` adds all missing attribute classes to the membership mask
before positive inclusion or negative inversion. Consequently a negative filter
with that flag excludes missing values. Whole-case inversion occurs after
collecting matching case identities. Missing case identities and list-valued
case cells retain captured membership behavior rather than being dropped by
an unrelated grouping rule.

`filter_pandas_attributes_numeric` and `_numeric_events` use inclusive bounds.
The case function selects complete cases containing a qualifying event, with
optional first and second stream equality predicates combined by AND. Both
stream column lookups remain active and preserve diagnostic precedence. The
event function ignores stream controls and case selectors. Negative numerical
event filtering tests outside bounds: missing values do not automatically pass
an inverted comparison, but `keep_nan_values=True` adds them. Negative numerical
case filtering complements the cases containing an in-range or admitted missing
value. Both numerical modes have active missing-value controls.

Integer comparisons preserve signed/unsigned identities beyond double's exact
range, including adjacent 2^53 values and UInt64 maximum. Float32 vectors compare
against scalars narrowed to float32, as the captured runtime does. Timestamp
bounds compare exact nanoseconds. NaN/NA/null bounds, infinities, inversion and
malformed type/length comparisons retain their captured outcomes. Empty-list
attribute selectors preserve the observable dataframe-mask behavior: case
filters return no selected cases; event filters produce the complete frame
with all cells missing and the captured dtype promotions.

## Counts, thresholds and relative occurrence

`filter_pandas_attributes_threshold` counts nonmissing activity values unless a
genuine supplied count dictionary is present. `PandasAttributesCountMap` carries
typed ordered key/count entries, including numeric, bytes, tuple and Timestamp
keys. Every call normalizes duplicate Python-equal dictionary keys: the first
key and position remain, while the latest count wins. The public normalization
helper preserves exact nanoseconds recursively in date/tuple keys and uses
established native numeric/Boolean coalescing and floating NaN object identity. An independent native test verifies
this transport behavior and mutable-key rejection. Absent and explicit None
counts recompute statistics; an empty supplied dictionary remains genuinely
empty. Invalid nonmapping supplied values retain their `.items` errors.

Thresholds are inclusive. `most_common_variant` admits matching dictionary keys
in addition to keys meeting the threshold; it cannot invent a key absent from
the supplied dictionary. String controls use literal substring membership,
including empty-string membership. Threshold comparison short-circuits the
most-common test. If every supplied/computed dictionary key survives, the
captured function returns the entire source unchanged, including values absent
from that dictionary. This also holds for an empty dictionary and can bypass
an otherwise missing activity column.

`filter_pandas_attributes_top_activities` faithfully retains the captured helper:
all tested valid integer limits, including zero and negative limits, return the
entire original frame. It still performs activity counting and rejects captured
invalid slice/count types. This checkpoint does not present that observed helper
as a conventional top-activity projection.

`filter_pandas_attributes_relative_occurrence` selects individual events whose
value occurrence meets an inclusive relative threshold. By default
`keep_once_per_case=True`: numerator counts distinct nonmissing case identities
for each nonmissing value; denominator counts all distinct nonmissing cases.
False or None uses event counts divided by total rows and ignores the case
selector entirely, even when that column is missing or the selector is invalid.
Positive and keep_nan remain active in the final membership operation. List
attribute keys/cells and malformed stakes retain exact grouping, hashing,
multiplication and comparison diagnostics.

## Parameter and complete-frame fidelity

The five parameter-based functions use ATTRIBUTE_KEY, not ACTIVITY_KEY. The four
ordinary apply wrappers give enum ATTRIBUTE_KEY priority over its string alias
in either insertion order. Relative occurrence instead gives the explicit string
ATTRIBUTE_KEY priority in both captured orders. The typed parameter adapter
preserves this distinction. The three low-level helpers accept direct literal
selectors. Controls inactive for an operation remain inert in that operation.

Full results preserve every typed source column, column order, dataframe attrs,
index class/dtype/names/labels, declared dtypes and complete category metadata,
including unused categories. Captures include nullable cells, signed zero/NaN,
bytes and recursive containers, duplicate and MultiIndex labels, uint64 labels,
and RangeIndex labels near both signed-int64 limits. RangeIndex classification
uses exact TokenInteger differences. No selected index label is rounded through
a floating representation.

## Retained observations and native acceptance

Root executed two frozen banks once each: B1 contains 429 distinct calls with
318 full-table returns and 111 errors; the disjoint continuation contains 19
calls with 17 returns and two errors. Across both, all 448 complete typed
receipts are distinct. Before continuation entry, all prior complete receipts
were reconstructed and saved hashes verified; the prior plan and record files
contain 858 rows describing the same 429 calls. Neither bank removed duplicate
candidates or prior overlaps. Each call retains source, actual typed positional
and keyword controls, complete result or exception, per-call warning list and
before/after mutation snapshots. Zero warnings or mutations were observed. No
reference implementation source, AST, bytecode or private state was inspected.

The offline generator creates 150 complete source/result snapshots and 448
separate noinline native cases: 335 full-table outcomes and 113 exact exception
type/message outcomes, plus independent count-dictionary normalization checks.
The initial compile attempt failed before creating the test executable because
MSVC parses the decimal INT64_MIN literal as an unsigned unary-minus expression.
The generator now spells that same value with `numeric_limits<int64_t>::min()`.
An offline whole-file equality assertion confirms that this literal spelling is
the only fixture change; no captured oracle changed. Initial source snapshots,
helper-object hashes and the failed build receipt are retained. Native review
also corrected supplied dictionary key equality to preserve exact nanoseconds
recursively before the first native execution.

The first executed pandas suite passes all 335 full tables, 113 exact
diagnostics and dictionary-normalization support checks in
`ctest-attributes-modules-first.log`. That shared run passes one of three
suites; its separate log/Polars failures are retained, and no overall success
is inferred. No pandas source or oracle changes followed that native run.
The two new table workbench/owned-HTTP targets also pass their first run,
including every engine mode, exact integer bounds, typed count controls,
relative tri-state behavior, literal selectors, complete source/result artifacts
and explicit empty-filename File uploads. That shared four-suite run passes
two table suites and retains two separate log workflow failures. Initial table
sources and compiled hashes are preserved; the final build also includes the
explicit relative-default-token rejection assertion.

The authoritative coordinated build and CTest pass all 41 suites. The named
pandas suite passes 335 full tables and 113 exact diagnostics on both its first
execution and final acceptance, without subsequent algorithm/oracle changes.
The shared final frontend acceptance passes 190 files and 9,809 tests; the
focused acceptance passes 65 tests, and TypeScript and Vite checks pass.

The final owned production checkpoint passes all 95 HTTP acceptance requests:
35 pandas table, 33 Polars table, 23 log and four strict multipart checks.
Readiness/version probes are separate. These cover all usable engine modes,
actual auxiliary trace contents independent of variant-map keys, exact signed
and unsigned criteria, custom NUL selectors, count projections and complete
artifact reopen. The final server SHA-256 is
`1d6b11e78d1197130a162db0419df62ec62e906de4cbee1db60e26e74c54b1e3`.
The owned server exits after verification; no existing application is replaced.

Three earlier production attempts remain preserved with their exact executed
scripts, full request/response records, failure context and server-exit receipt.
The first retains 64 responses before a harness assumption rejected a valid
raw native count-table carrier. The second retains 82 responses before a
cross-helper assertion assumed the log threshold helper preserved trace
properties; its captured contract omits them. The third retains 85 responses
before the XES reopen harness omitted the exported opening activity role and
assumed generic XES attributes populated native primary activity fields.
The repaired harness reads ordinary and raw table carriers without stripping
wrapper/type/index metadata, uploads actual exported count JSON, compares the
threshold result after only its documented trace-properties omission, and
reopens XES with both the literal filter selector and exported opening role.
XES verification checks exact work attributes A/D/A; native-log reopen retains
an independent exact full-JSON comparison. These harness repairs change no
native implementation or public oracle.

The deterministic proof archive binds complete public receipts, per-call
warnings and mutation snapshots, frozen observers and generators, initial
failed compilation sources, native/workbench/HTTP fixtures, current source and
compiled object/library/executable/map hashes, final and historical build/test
logs, all production attempts and frontend acceptance/build artifact hashes.
Packaging performs zero reference calls and reconstructs every saved complete
receipt hash again before archiving.

The bounded checkpoint does not establish arbitrary custom Python objects,
third-party extension arrays, arbitrary duplicate/multilevel column labels,
arbitrary warning-producing inputs, exotic business-specific objects or every
possible mixed-type diagnostic precedence. Combinations of unused category
levels with missing activity values or floating activity limits have not yet
been separately discriminated. Nested mutable dictionary keys are
rejected rather than silently represented as a multimap. Captured public
behavior remains authoritative; broader module coverage stays partial.
