# Streaming retained observation inventory

`scripts/retained_public_observation_inventory.py` inventories explicitly bound
own JSON/JSONL containers without loading whole archives. Discovery reads field
names and declared member identities while leaving payload strings opaque.
Only explicitly listed own public JSON/JSONL members may be decoded, checked
against exact original size/SHA256 bindings and structurally classified.

```powershell
python -B -X utf8 scripts/retained_public_observation_inventory.py `
  manifest.json report.json --events events.jsonl --cache build/owned-cache
```

The manifest schema is
`minera-retained-public-observation-inventory-manifest-v1`. It requires an
absolute `root`, a bounded prose `scope` and `sources`. Every source supplies
relative POSIX `path`, exact `bytes`/`sha256`, `format`
(`json`, `jsonl`, `gzip-json`, `gzip-jsonl`) and `layout`:

- `discover`: metadata only, with `provenance="own-container"`.
- `records`: explicit own public observations, with
  `provenance="own-public-observation"`; an object wrapper requires `records_key`.
- `envelope`: explicit own public container, optionally with `members_key` for
  an object wrapper. Its `members` allowlist supplies each exact original
  path/size/hash, JSON/JSONL format, records layout and public provenance.

Unlisted payloads remain opaque even when their path follows their data.
Implementation, dependency, private and traversing paths cannot be admitted.
Nested archives require a separate reviewed admission. Unknown record schemas,
headers, encodings and possible call evidence remain explicit gaps. Actual
outcomes take precedence over planned fields; entered records stay distinct.
The inventory records exact raw record hashes and saved receipt-hash metadata.
It does **not** reconstruct or validate full canonical call receipts. Use the
[receipt index](retained-public-receipt-index.md) with a validated observer adapter
for that separate step.

The explicit legacy schema `legacy-name-data-base64-v1` maps the reviewed own
producer's `kind/member`, `name`, `sha256` and `data_base64` fields. Its original
size is unavailable, so admission requires an independently reviewed exact size
and verifies both the original envelope SHA and allowlist binding. The reviewed
header requires `metadata_kind="manifest"`, `metadata_schema_version=1`.

The explicit `utf8-name-text-members-v1` schema supports **discovery only**.
It maps the reviewed own producer's exact `kind="member"`, `name`,
`byte_count`, `sha256` and `text` fields. The size and hash describe the full
logical member. Text remains lexically opaque: it is never spooled, unescaped,
decoded or admitted. Header and tail declarations remain separate untrusted
candidates and gaps; metadata validity does not verify member payload hashes.
Both the command entry and direct `Inventory.parse` reject non-discovery
layouts or member admission before constructing the parser.

Two explicit chunk schemas support **discovery only**:

| Schema | Declared identity |
| --- | --- |
| `offset-full-member-base64-chunks-v1` | `member` names the file; `bytes`/`sha256` describe the full member, repeated with each decoded byte `offset`. |
| `indexed-base64-chunks-v1` | `path` names the file; `bytes`/`sha256` describe this chunk, `file_sha256` the full member, and `chunk`/`chunks` the index/count. |

Both chunk profiles reject non-discovery layouts and member admission at the
command entry and direct `Inventory.parse`, before parsing or spooling data.
Chunk data is neither decoded nor reassembled. Every chunk event states
`payload_hash_verified=false` and `whole_member_complete=false`. Duplicate
identities and inconsistent headers are gaps. Identity scope is one container
unless the manifest supplies a reviewed `series_identity`. An ordered complete
series, exact sidecar/producer bindings, all chunk checks and final full member
hash/size verification are still required before public member admission.

Sources are freshly hashed before and after parsing and revalidated before the
report finishes. The parser hash is also rechecked. Cache keys include exact
source content, parser and configuration identity. Cached events require a
verified complete footer, and a hit still requires fresh source hashing.
Timestamps never establish unchanged content. Reports always retain
`admission_complete=false` and `catalogue_audit_complete=false`.

The retained history bindings are separately preserved in
`retained-history-content-bindings.jsonl.gz` with its validation sidecar. Their
507,319 rows cover opaque current-read hashes for selected file suffixes below
`build` and `docs`; they do not establish provenance or historical absence.
Content-equivalent copies retain distinct path provenance. Newly generated
files after enumeration require a later delta inventory.

Before a new reference bank, complete the relevant flat/archive historical
audit, resolve every relevant provenance/schema/reassembly gap, reconstruct and
assert all original complete receipts, then deduplicate the entire candidate
bank on actual inputs, arguments, kwargs and call shape. Neither metadata
discovery nor the bounded LTL acceptance authorizes new-family calls.
