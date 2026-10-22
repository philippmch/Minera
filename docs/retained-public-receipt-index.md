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
