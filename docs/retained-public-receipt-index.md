# Retained public-call receipt index

`scripts/retained_public_receipt_index.py` builds an offline SQLite index from
explicitly admitted, hash-bound public observations. It imports no reference
package and enters no reference algorithm. This supplies reusable receipt
validation and history for future parity work; it does not establish full
catalogue coverage or authorize a new reference call.

Run the committed LTL archive inventory from the repository root:

```powershell
python -B -X utf8 scripts/retained_public_receipt_index.py `
  docs/retained-public-receipt-ltl-manifest.json `
  build/retained-ltl-receipts.sqlite build/retained-ltl-receipts-report.json
python -B -X utf8 -m unittest scripts.tests.test_retained_public_receipt_index `
  scripts.tests.test_retained_public_receipt_ltl_archives -v
```

Create the output parent directory first when using a fresh checkout. Keep the
inputs frozen while indexing and before relying on a report. A completed report
binds only its explicit manifest, parser identity and observed source hashes;
later writes require another inventory and validation generation.

## Supported evidence

Version 1 admits three exact original LTL observer schemas: LOG positional
arguments and keyword controls, pandas source/argument/keyword snapshots, and
Polars source/category/kind/keyword snapshots. Their original canonical receipt
bytes and saved SHA-256 are reconstructed without changing operands or call
shape. Unknown schemas and incomplete source snapshots become blocking gaps.
Adding another observer requires an independently validated adapter and tests.

The separate `log-attributes-packed-v1` adapter requires receipt schema
`minera-log-attributes-observer-receipt-v1`. It is closed to the exact LOG
attributes module, its eight reviewed functions and observed positional
arities. Sharing `positional_args_keyword_controls` with LOG LTL does not select
this adapter. Complete ordered typed arguments, keyword entries, enum identities,
log snapshots and cache trace snapshots remain in the original canonical bytes.
Each non-null `source_trace_index` must match its corresponding before/after
source trace. Unknown descriptors, incomplete snapshots, conflicting enum values
and malformed outcome or mutation metadata are gaps.

Terminal records require complete source/control before and after, full returned
logs or errors, warnings and captured result alias metadata. Aggregate control
changes can include aliases to source mutations; they do not prove independent
cache changes. Full plans and lightweight intents retain the index's separate
historical semantics. Possible terminal evidence cannot become a plan, and
after-state evidence remains reserved as an entry. Matching intents resolve only
against exact validated receipts in the same schema and reference/runtime
partition. This adapter grants no new observation provenance or reference entry.

The distinct `pandas-attributes-before-v1` adapter requires schema
`minera-pandas-attributes-observer-receipt-v1`. It reconstructs the original
six-field receipt from module, method, input-before, arguments-before,
keywords-before and call shape. The complete frame is separate from positional
operands. Sharing a call-shape string with pandas LTL grants no schema admission.
The closed pandas attributes module has five parameter wrappers and three
direct helpers, with separate method/arity/keyword domains and exact public
enum identities. Full frames use the observed class tag `pandas.DataFrame`;
the initial incorrect core-module class assumption and failed census are
preserved. Typed rows, index labels/names, dtypes, categorical metadata, attrs,
nanosecond Timestamp strings/resolution/timezone and ordered map entries stay
unchanged in canonical bytes. Unknown nested forms remain gaps.

Complete terminal records validate source and arguments before and after, actual
mutation flags, returned-input identity, full errors including typed arguments,
and warnings including filename and line number. A terminal row under an intent
or plan role remains an actual observation. Conflicting schema/module identities
block bounded certificates, including when the legacy row has no embedded
receipt. Full original plans, digest-only plans and entered intents retain
separate provenance; possible entry/outcome fields cannot be hidden as plans.

The separate `polars-attributes-receipt-v1` adapter requires schema
`minera-polars-attributes-observer-receipt-v1`. Its original receipt has exactly
seven fields: module, function, input, category observables, ordered keywords,
call shape and eager/lazy input kind. It is closed to the nine reviewed Polars
attributes functions. Sharing the Polars LTL call-shape spelling does not select
this schema. Required keyword omission and raw invalid controls remain original
error recipes; the index neither inserts defaults nor rewrites values.

Full snapshots retain tuple rows, dimensions, dtypes, date/datetime ISO,
datetime fold/offset, float hex and typed dictionary order. Category snapshots
retain unused Enum values and named local Categorical context, physical dtype,
ordering and observation warnings. Unknown category contexts or packed types
are gaps. Complete frame returns and typed count-map `result_value` outcomes are
validated before plan/intent roles, alongside exact apply/collect errors, public
pre-collection plans or plan errors, warning arrays and all four source/control
mutation flags. Returned-input identity requires matching source-after frames
and category snapshots. Typed NaN descriptors do not establish Python key object
identity, and an index observation does not establish stable cutoff tie identity.

The producer's module-free intents require exact index/function/case/hash fields;
module identity is resolved only through a validated original receipt in the
same pinned schema/runtime/reference partition. Full audit plans require an
embedded receipt and a closed metadata field set. Optional indices must be
positive integers and plan case names must be nonempty strings; case dictionaries
are accepted only as complete captured actual or entered recipe metadata. Unknown entry evidence,
before-only records and mixed terminals become blocking gaps; known entry
evidence remains reserved. This family does not expand any archive manifest.

JSON, JSONL, gzip JSON/JSONL and explicitly allowlisted archive members are
supported. Recognized members carry either base64 or retained UTF-8 text plus
exact size and SHA-256. Unlisted members are recorded without decoding and are
outside the admitted scope. Reference installations, source trees, dependency
paths, private paths and path traversal are rejected before opening them.

Outcomes are classified before plans: records containing both a receipt and a
result, error or scalar return count as actual calls. Entered calls without a
terminal outcome stay reserved. Lightweight intents resolve only against exact
validated terminal receipts in the same original schema and reference/runtime
partition. Digest-only plans never supply missing operands or resolve entries.

The database keeps historical observations and provenance when files move,
change or disappear. Full source hashes are checked on cache hits and again
before publishing a generation. Parser changes invalidate cached classification;
older-parser history blocks a certificate until it is reclassified. Saved
canonical receipt bytes are also revalidated. Unknown formats, corruption,
identity conflicts, concurrent input changes and unresolved entries block the
affected bounded completeness claims.

Reference and runtime pins are explicit manifest assertions. The index does not
independently authenticate those pins or discover every receipt on disk. The
`catalogue_audit_complete` field remains false. A bounded report cannot replace
the complete relevant historical inventory, receipt reconstruction and whole-bank
dedup required before the first entry of a new reference bank.

## Accepted retained slice

The committed manifest admits three existing public archives and only their
listed observation/plan/intent members. Independent reconstruction from all
sixteen original executed banks produced exactly **1,401 distinct actual calls**:
297 LOG, 429 pandas and 675 Polars. Each saved receipt hash was asserted before
indexing; the expected sorted digest-set hashes are committed separately.
Archive indexing, flat-bank indexing and incremental archive copies preserve
those exact sets. Successful helper scalar returns are included. Plans and
intents retain separate provenance rather than inflating the actual-call count.

Root separately admitted six complete flat LOG attributes banks for bounded
offline parsing. Two standard-library census implementations reconstructed all
280 original receipt hashes across eight functions: 182 returned logs, 98 full
errors, zero warnings, 162 source changes and 165 aggregate control changes.
Their sorted original receipt-set SHA-256 is
`ec5330eb47c1a4611d4790d94a02178cde7e914b48d6f5be6c9b93a5fe88a597`.
The second implementation confirms 16 keyword-control changes in maximum-activity
calls, three without source changes, and complete before/after source trace
membership metadata. It is a separate code path by the first helper's author;
it does not substitute for an independent reviewer. These flat-bank admissions
do not extend the committed three-archive LTL manifest. The new synthetic test
module is `scripts.tests.test_retained_public_receipt_log_attributes`. Root ran
all 41 tests (25 existing integrity tests, the committed-archive integration
test and 15 attributes tests). A fresh flat-bank index reconstructs all 280
canonical receipts, resolves all 280 exact intents and reports zero gaps. A
second generation rehashes all twelve sources and reports twelve cache hits.
This acceptance covers only those explicitly admitted flat files.

Root also admitted two complete flat pandas attributes banks. The corrected
offline census reconstructed all 448 distinct original hashes: 335 full returned
frames, 113 errors, zero source/argument mutations and zero warnings. Its frame
snapshot count is 150 in the original generator's scope; that count alone does
not prove native constructors or oracle mappings. The sorted receipt-set SHA-256
is `4c833bf5516f0170a47ade0d1b1959054e2b7574fe9ea9914ee69c867a55cfdf`.
Root's final 59-test integrity run and final hardened index accepted all 448
canonical receipts, resolved all 448 exact intents, reported zero gaps and then
rehashed eight sources for eight cache hits. The test module is
`scripts.tests.test_retained_public_receipt_pandas_attributes`. Initial frame-kind,
source-path join and unknown-full-plan-field failures remain preserved.

Root has separately admitted three complete flat Polars attributes banks.
Independent bounded census metadata reconstructs all 726 original receipts:
697 LazyFrame and 29 DataFrame calls, 464 frame returns, 52 typed count-map
returns, 126 apply errors, 84 collect errors and 308 warnings. Its sorted digest
set uses UTF-8 newline-joined hashes without a trailing newline and has SHA-256
`128750d0b843f19f8c16ed022ca5d2a0fe86c40a81a190f7c2dd529ec0cbd2bb`.
The new test module is
`scripts.tests.test_retained_public_receipt_polars_attributes`; synthetic integrity
and the proposed full-bank index/cache runner require separate root acceptance.
Nine bounded cutoff frames and unordered count-map keys retain their existing
limitations. These admissions extend neither the committed LTL archive manifest
nor historical completeness, catalogue status or reference-call authority.

Twenty-five synthetic integrity tests and one committed-archive integration test
pass. The integration test checks all original digest sets, canonical bytes,
bounded certificates, admitted/unlisted member provenance and rehashed cache
reuse. Independent review fixes cover outcome misclassification, excluded root
ancestry, incomplete schemas, overflow JSON numbers, input changes during a
generation and stale parser caches. No new reference algorithm was called.

This checkpoint changes no package or module status. Full parity remains open:
86 packages / 387 modules in scope and 201 strict open units. Activity splitting
and consecutive/timestamp case grouping are the next prepared family; their
zero-call public surface and offline recipes do not constitute runtime parity.
