# Full parity continuation — 2026-10-21–22

Resume in `C:/Users/pmoch/Documents/Minera`, branch `main`.

## Objective and authorization

Reach full pinned-reference parity, prioritizing usable features, then complete
fidelity. Keep all 86 packages and 387 modules in scope. The user explicitly
authorized parallel subagents and clean logical commits/pushes directly to
`main`. Do not ask again for routine implementation, tests or Git checkpoints.

Use only public black-box calls, public signatures/docs/enums and opaque binary
hashes. Never inspect reference implementation source, AST, bytecode or private
state. Do not replay retained calls. Preserve failed attempts and execute new
uncovered banks only in new output directories.

## Current continuation: streaming history inventory — 2026-10-22

Clean checkpoints: `f9b514c8` adds the streaming inventory and 32 tests;
`eca91620` preserves the zero-call Polars preparation; `56b8785d` retains real
archive acceptance; `7aa63579` preserves full opaque history bindings and
discovery inputs. Read `docs/retained-public-observation-inventory.md`.

All **58 receipt-tool tests** pass, including the original full-receipt archive
integration. The final streaming parser SHA256 is
`db75974c2c3166d0a8d7303997b11654e8d7dc123b0ec30e1ebd59154c2b1d55`.
Root verified all **1,401** actual LTL record identities against the independently
reconstructed original digest sets; a fresh-hash cache replay preserves them.
Metadata discovery of the 2,080,870,160-decoded-byte legacy archive finds all
5,870 members with zero gaps. Five other bounded archives expose 8,991 members
and retain three intentional pending-admission gaps. The initial unknown legacy
format, every frozen parser, and a cache-reporting defect/fix remain preserved.
Evidence: `docs/retained-public-observation-inventory-checks.jsonl.gz`,
77 members / 2,330,097 bytes, SHA256
`99c8906d11ebf7ee434ac8571d5c8f63cfc444b1db6e93227be22c2186239ac5`.
The sidecar distinguishes structural classification from full receipt validation.

The old LTL search was not a complete inventory for the next family. Root has
now enumerated **507,319** candidate files below `build`/`docs` and opaque-hashed
all **79,597,719,131 bytes**, with no read failures or changed/stale metadata
bindings. There are 96,890 distinct content hashes. Exact directory exclusions
and provisional provenance dispositions remain reviewable. The complete binding
stream is `docs/retained-history-content-bindings.jsonl.gz`, 15,670,816 bytes,
SHA256 `06969136b599b3692f910492eab6142425f495c6340ee1b978df448a05dda1c0`.
This is current-read metadata, never provenance, original creation identity or
absence of historical calls. New files require a later delta inventory.

The inventory/mapping capsule is
`docs/retained-history-inventory-preparation.json.gz`, 31 members / 1,085,755
bytes, SHA256 `0ce2f6ca9fdae6c2a1737aab06fef7f4299ba3587ba69456675807817177f06a`.
Its sidecar records the executed hash helper's limited symlink check, exact
exclusions, metadata-only chunk semantics and remaining provenance gap. One own
compressed copy was resolved by exact historical Git-blob equality. The Polars
pre-EOF-repair copy remains unresolved; a shared compressed prefix is insufficient.

**A coordinated metadata-only scan is running. Do not restart it.** Root exec
session **5107** scans the **2,844** distinct own compressed contents using:
`build/parity-closures/grouping-history-archive-discovery-final-2/manifest.json`
(981,981 bytes, SHA256
`05ef77cd8c8fd03bba4202ebf724cc8752b5311b47bf831895df08f1cf2fca20`).
Output `events.jsonl`, `report.json`, cache and `run-first.log` share that folder.
Poll/drain the existing handle; report existence alone does not prove terminal
completion. No payload is admitted. Offset/full-member and indexed/chunk-hash
formats stay distinct, opaque and incomplete. Unknown UTF8/report envelopes and
all possible-entry contexts retain explicit gaps. After terminal completion,
root may run the prepared metadata-only full-member/flat-copy projection helper
in `grouping-history-audit-prepared-1/`; it must not open/admit matched content.
Its corrected helper SHA256 is
`17f6eec37d3123b988844ae508d60e92313926bfdb83ea9c3622397b04bd96a5`;
seven synthetic integrity tests pass. It now requires `--terminal-receipt` with
schema `minera-retained-public-inventory-terminal-binding-v1`, terminal/drained/
finalized flags, exact zero exit code/session ID, and report/events/manifest
size/SHA bindings. Root must issue that receipt only after draining session 5107.
Then review provenance, resolve relevant schemas/chunks, reconstruct complete
original public receipts and deduplicate before any new algorithm bank.

The Polars activity-split bank is now independently verified **prepare-only**:
79 distinct full receipts, zero target/source-collect/result-collect calls,
`prior_barrier_validated=false`, and no entry certificate. Output:
`build/filtering-activity-split-polars-public-preentry-1/`; ordered receipt-list
SHA256 `8ebf001d4371e7ccc5eb003043709d01fef0c6af25e8c124c51370b6f6a7739b`.
Capsule `docs/filtering-grouping-polars-preparation.json.gz`, 27 members /
54,009 bytes, SHA256
`3217251d4f9fb6bdb94fd0574f0866a5b4fb845bae82e9226d92b409318bdc97`.
All original canonical hashes, member bytes and source bindings pass. Default
entry refuses before imports/output. The earlier failed refusal-reader assertion
is preserved. Existing ActivitySplit recipes use assertions: invoke without
Python optimization, and harden/review their gates before any actual certificate.

Pandas case-grouping observer/candidates are under
`build/parity-closures/case-grouping-pandas-prepared-1/` (97 consecutive,
123 timestamp candidates). Root has now run **prepare-only** for both fresh
`build/filtering-case-grouping-pandas-{consecutive,timestamp}-public-preentry-1/`
banks, independently verifying all **220** original full receipt hashes and
both ordered digest sets. Zero algorithms, no intents/outcomes/certificates and
`prior_barrier_validated=false` remain explicit. The corrected observer SHA256
is `84050d49e9dd85db1ede3a6ba3840fffa55dc355ba06773375ea243d53d9d3cf`.
It rejects optimized Python and incomplete nested receipts and recomputes each
full current receipt immediately before an actual intent. All 26 independent
extracted checks pass. Initial findings/drafts and the failed class-spelling
test remain preserved. Capsule:
`docs/filtering-pandas-case-grouping-preparation.json.gz`, 67 members /
155,223 bytes, SHA256
`33f4d710ab4d25b1b78ab60f19a153f480a9b1302d5d40a3b684f502930a401d`.
Its sidecar records the two ordered hashes. Root additionally verified every
capsule member and all 67 current source bindings. A separate Polars grouping
observer is being prepared offline under `case-grouping-polars-prepared-1/`;
do not execute it before root and independent review.

No new reference algorithm was entered, product/frontend code was unchanged,
and no parity status was promoted. The committed structural verifier still
reports **46/86 packages and 226/387 modules complete; 201 strict open units**.
Inherited coverage/parity edits remain untouched (31/1 and 1/1 numstat).

## Earlier checkpoint: retained receipts and next filtering family

The reusable offline receipt index is checkpointed in `1eb692e7`; committed LTL
archive acceptance and evidence are in `5757778e`. Read
`docs/retained-public-receipt-index.md`. The tool and 25 synthetic integrity tests
plus one committed-archive integration test pass. Root independently reconstructed
and asserted all **1,401** original full receipts from the sixteen executed LTL
banks: LOG 297, pandas 429, Polars 675. Flat ingestion, archived copies and a
rehashing cache generation preserve those exact sets. All relevant intents in
the admitted banks resolve against terminal outcomes. No new reference algorithm
was entered and no catalogue status changed: **201 strict open units** remain.

The index supports three closed LTL observer schemas. Unknown schemas require
validated adapters. It preserves immutable history, actual/plan/entered status,
exact receipt and record hashes, reference/runtime partitions and provenance;
corruption, unknown identities, incomplete snapshots, parser changes and source
changes block bounded certificates. Every report remains
`catalogue_audit_complete=false`. The manifest covers explicit sources/members
only; it does **not** replace the complete relevant historical audit required
before a new family can enter. Never relabel its LTL certificate as a complete
ActivitySplit or case-grouping audit.

Evidence: `docs/retained-public-receipt-index-checks.jsonl.gz`, 28 members,
94,095 bytes, SHA256
`1b4d4ee521a2fe622d59b546d34add67b0ceeef323b732f3b75e43e5025fb369`.
The sidecar `docs/retained-public-receipt-index-validation.json` binds the frozen
parser SHA256 `885d16b3935a95d51d90fc09abd085dde3e46bf8732915bb25f520a0ac8ff613`.
Root final acceptance: `build/parity-closures/receipt-index-root-acceptance/`
(`validation.json`, `flat-final-report.json`, `archive-final-report.json`,
`archive-cache-final-report.json`, `cli-final-report.json`,
`unittest-final-first.log`). Earlier manifest/adapter attempts remain retained;
they are not final proof. Postcommit Git/archive bindings pass. Git text
comparison normalizes CRLF/LF only; actual input and archive hashes stay exact.

Next prepared family: the six pandas/Polars `activity_split`,
`consecutive_act_case_grouping` and `timestamp_case_grouping` modules. Root's
public signatures/docs/enums and opaque pin capture is complete at
`build/grouping-public-surface-actual-1/capture.json`, with zero algorithm calls.
Existing convenience implementations are EventLog-only and do not establish
table parity. Offline reviews and a 20-fixture grouping specification are under
`build/parity-closures/{activity-split,case-grouping}-next-review/`.

The pandas activity-split observer and 61 candidates are frozen. Root ran
**prepare-only**, materializing 61 distinct complete typed receipts and asserting
their original hashes, with **zero target calls**. The output is
`build/filtering-activity-split-pandas-public-preentry-1/`; its validation
explicitly says `prior_barrier_validated=false`. Its final ordered receipt-list
SHA256 is `43f81e0b1c6c71adf5796477bb9a84d22d4ad70a741d56849d3972fbd8718bba`.
No entry certificate exists. Actual entry requires a root-reviewed complete
relevant flat/archive historical audit, checked underlying bindings, full prior
receipt reconstruction and whole-bank dedup. The gate rejects a bounded index
alone; nine offline gate checks pass and root verified default entry refusal
before output creation/reference imports. Do not execute these planned calls
until that barrier is actually satisfied.

The full preparation capsule is `docs/filtering-grouping-preparation.json.gz`,
37 members / 55,701 bytes, SHA256
`8ae44615941a0eb3c514f00f542da7a28831c65eb91dc9fd20e0f860e9646856`;
its sidecar validation records the zero-call boundary. It retains the original
weaker recipe, the corrected frozen recipe, public surface, materialized plans,
reviews and root checks. Recipe SHA256:
`b41f0cf80301d47dc3628587db8614c3873b5abfe165180694d0e65f75720d33`.
All workers are frozen. No builds, HTTP servers or reference calls remain running.
Product/frontend code was unchanged during this checkpoint; the native,
frontend and HTTP results below remain the earlier LTL acceptance, not new runs.

## Previous continuation: native byte controls and LTL filtering

Commit `13f34f35be95c6a3d054b694ff9a528b400aa867` is pushed. Both attributes
workbenches now accept the frontend's canonical `<field>.txt` byte-preserving
Blob controls. Unrelated File-shaped ordinary controls remain rejected.
All four focused assertion-enabled workbench/owned-HTTP suites pass in
`build/parity-closures/native-current/ctest-attributes-byte-controls-first.log`.
The evidence archive `docs/attributes-native-byte-controls-checks.jsonl.gz`
has 32 members, 107,541 bytes and SHA256
`86299c611832abbe9369dd40fad0e25e9a63fe7449a17f61709c1d754bdc3a5a`.
Its 14 current source and 16 compiled bindings were checked after commit;
Git source comparison normalizes CRLF/LF only, while actual compiled-source
and archive hashes remain exact. Additional production acceptance now passes
**101 actual HTTP checks** in
`build/parity-closures/attributes-byte-controls-production-http-4`. The separate
server was compiled from an unmodified committed server-source copy at
`9b983512` plus 58 unchanged current native objects and the pinned frozen core;
local unfinished LTL endpoint edits were excluded. Its SHA256 is
`22e6635a92e06bee3d97a7678572627093c007aecca0c1110072c78c19a18e1a`.
The new archive `docs/attributes-native-byte-controls-production-checks.jsonl.gz`
has 95 members, 2,167,133 bytes and SHA256
`df468440cb7261ba0c144223eb2e4e7b8de5b182182b0dde0c7cb1af07b3bf7e`.
It binds 59 sources and 62 compiled artifacts, all four HTTP attempts and the
prior focused archive. First two harness attempts misread the valid raw-byte
log wrapper; third passed 101 and final fourth added complete raw-byte log
artifact equality and passed 101. All owned server processes exited. No product
changes were needed for those reader corrections. Rendered browser acceptance
remains unverified. At that checkpoint, LTL's separate rebuild was pending;
its accepted final build is recorded below.

The LTL family is implemented across LOG, pandas and Polars through native
companions and `POST /api/reference-ltl-filter`. The workbench exposes genuine
source Files, literal/typed activities and selectors, active timestamp and
performer controls, source-free typed interval helpers, complete source/result
artifacts, XES export/reopening and cancellation. Existing convenience LTL
behavior remains separate. Read `docs/ltl-filter-workflow.md` and the three
`docs/{log,pandas,polars}-ltl-filter-parity-contract.md` files.

Final validation passes:
- **47 assertion-enabled native suites**, including every retained engine
  outcome and both workbench/owned HTTP suites:
  `build/parity-closures/native-current/ctest-ltl-final2.log`.
- Server and CLI build from current overrides:
  `build/parity-closures/native-current/build-ltl-sixth-final.log`.
  Giant module test drivers alone use `/Od`; production helpers remain Release.
- **192 frontend files / 9,829 tests**, TypeScript and Vite:
  `build/parity-closures/frontend-ltl-full-final.log`,
  `frontend-ltl-tsc-final-second.log`, `frontend-ltl-vite-final.log`;
  retained output `frontend-ltl-dist-final`.
- **73 actual LTL HTTP checks plus 101 attributes regressions** against the same
  final production binary, SHA256
  `be6f9e6b656cb157575554d0d2e4a11fa3984a4d935b04938dba903f3a944445`:
  `build/parity-closures/ltl-production-http-4` and
  `build/parity-closures/ltl-attributes-production-http-2`.
  Both owned server processes exited; the existing application was not replaced.
  Rendered browser acceptance remains unverified.

Both original retained-data audits are parsed complete, with no failures or
unclassified contexts. Do not restart those scans. Flat audit SHA256:
`3e3a284c043f295631a11a3ac09ab9c73d4cedf65b13aef19b1326ac4847763a`;
compressed audit SHA256:
`9da8061363d9ce92c8965e02b48a81f9df12568ef80e939e04439ae950a4245b`.
Root alone ran reference algorithms. All **1,401 distinct outcomes** are retained
and matched: LOG297, pandas429, Polars675. No source/control mutations or
warnings were observed. **Never replay these banks**:

- LOG: `build/filtering-ltl-log-public-actual-1` (253),
  `...log-helper-public-actual-1` (31), `...log-boundaries-public-actual-1` (4),
  `...log-witness-public-actual-1` (4), `...log-multibounds-public-actual-1` (3),
  `...log-indexing-public-actual-1` (2).
- pandas: `build/filtering-ltl-pandas-public-actual-1` (386),
  `...pandas-continuation-public-actual-1` (27),
  `...pandas-time-activation-public-actual-1` (6),
  `...pandas-temporal-witness-public-actual-1` (1),
  `...pandas-case-identity-public-actual-1` (3),
  `...pandas-next-terminal-public-actual-1` (1),
  `...pandas-resource-pairs-public-actual-1` (2),
  `...pandas-resource-identity-public-actual-1` (2),
  `...pandas-resource-union-public-actual-1` (1).
- Polars: `build/filtering-ltl-polars-public-actual-1` (675).

Every continuation reconstructed and asserted all complete prior receipt hashes
before entry, then deduplicated the entire current bank on actual input,
arguments, kwargs and call shape. The failed pandas resource-pair rehearsal
made zero algorithm calls; its original prepared/preentry1 and corrected
prepared2/preentry2 remain archived. Do not infer call absence from filenames.

Preserve the observed engine differences. LOG eventually uses the first forward
activity subsequence, but enabled nonempty timing bounds read consecutive raw
prefix events 0..patternLength-1, independently of matched activity positions.
Its interval helpers compare each difference against inclusive lower/upper
endpoints. pandas supplied nonempty intervals auto-enable time only when the
flag is omitted; explicit false disables it. pandas NEXT uses final B/C
occurrences and original global row adjacency. Four Eyes forms resource sets
per typed case group, then later Python-equal groups replace earlier membership
classifications. Polars eventual/NEXT order by the selected timestamp even
without an enable flag; all difference-bound controls are inert there.

Independent integration review found no actionable transport/UI issue. Evidence
review found two binding gaps; committed archive/manifest anchoring, module
archive hash checks, CLI-object/transitive-recipe bindings and exact tested-unit
sets were added and rechecked. Review receipts are retained in
`build/parity-closures/ltl-independent-reviews.json`.

Retain all failed builds/tests/readers. Initial pandas optional-cell compilation,
NEXT and Four Eyes mixed-case mismatches, LOG witness models and raw-byte helper/
XES reader mistakes remain separate historical evidence. The final expectations
are the original public values; the pandas runner aggregates mismatches and
still fails on any mismatch or count difference. Historical compiled-source
snapshots and map/object identity manifests remain under `build/parity-closures`.

Public evidence archives are immutable:
- LOG: `docs/log-ltl-filter-public.json.gz`, 7,592,723 bytes,
  SHA256 `2660b657d674d7d7bb621d994f1f4cf93d77fddea034384cdd4b9aafd4174173`.
- pandas: `docs/pandas-ltl-filter-public.json.gz`, 8,972,301 bytes,
  SHA256 `9f073d0be9b35661772538d4a81a8de206160c281d9da3c119e8b4a2b7bb77fd`.
- Polars: `docs/polars-ltl-filter-public.json.gz`, 610,562 bytes,
  SHA256 `6270a238dc204c126e80ee816e5790449ad0f5b23f7ccce9fb7f1e9ab7fd417b`.

Current native evidence:
- LOG: `docs/log-ltl-filter-checks.json.gz`, 1,478,607 bytes / 88 members,
  SHA256 `a2bc768ff0d6e45296247ce1cee5e010c6593e0029782691fa769b4776a91f8e`.
- pandas: `docs/pandas-ltl-filter-checks.jsonl.gz`, 1,676,201 bytes / 88 members
  plus one acceptance record,
  SHA256 `4cd3eba8d2507952459225ba8d69adc650b8eb1da2a1c0f2b391b70edd7b72fc`.
- Polars: `docs/polars-ltl-filter-checks.jsonl.gz`, 19,263,919 bytes / 129 members,
  SHA256 `d96dfa5cd3fb2bdaebbb4217d8ee3239f4a9103bed7d8838c18278d563b3f411`.

The combined workflow capsule `docs/ltl-filter-workflow-checks.jsonl.gz` contains
8,604 verified members, 42,325,468 bytes, SHA256
`0896ec41d54194638991d9ad1c88bd5501b85dbdc02de713050efbfa9164facc`.
Its authoritative manifest is extracted from committed archive bytes. Verification
checks all seven docs archives against HEAD, 8,479 committed source bindings,
217 compiled identities, 124 receipt bindings and both module-evidence arrays.
Git comparisons normalize CRLF/LF only; actual source/artifact hashes remain exact.
The Git blob reader batches queries while retaining full byte comparisons.

Clean functional commits:
- `b4020165`: LOG public LTL and typed interval helpers;
- `d586b082`: pandas LTL with exact case/index behavior;
- `18ab8954`: eager/lazy Polars LTL;
- `365ecaf7`: workbench, production route, complete artifacts and workflow proof.

`docs/ltl-filter-commit-verification.json` records postcommit verification of the
integrated code commit. The following handoff checkpoint changes documentation
only. No owned functional drafts remain. Inherited coverage comparison metadata,
historical journal edit and untracked dependency/cache folders remain untouched.

All three canonical module rows remain partial. Full scope remains **86 packages /
387 modules**, **46 complete / 40 partial packages**, **226 complete / 161 partial
modules**, **201 strict open units**. Do not mark the full-parity goal complete.

## Latest attributes checkpoint

Four functional commits are pushed after `0af4187c`:

- `00e60870`: log attributes companion, all eight public functions;
  280 outcomes, 182 returns / 98 errors, source and parameter mutations.
- `958bb6f7`: pandas attributes companion, all eight public functions;
  448 outcomes, 335 complete tables / 113 diagnostics.
- `5f933276`: Polars eager/lazy attributes companion, all nine public
  functions; 726 outcomes, 455 exact frames, nine bounded cutoff frames,
  52 complete unordered typed maps and 210 full diagnostics.
- `823bdeab`: usable log/table attributes controls, genuine source and
  auxiliary cache Files, complete artifacts and production acceptance.

Read `docs/attributes-filter-workflow.md` and the three engine contracts.
All **1,454 distinct public calls** have matched native outcomes within the
explicit Polars tie/count-order boundaries. The three attributes rows remain
partial; only evidence pointers changed. Full scope remains **86 packages /
387 modules**, **46 complete / 40 partial packages**, **226 complete / 161
partial modules**, **201 strict open units**. Full parity is not complete.

Event-log controls retain source and caller-parameter mutations, including
before errors. Converter results reconstruct cases and can add canonical IDs
from selected custom event case cells. Numerical case selection uses an
unprefixed trace key and separate lookup glue; event reconstruction uses the
full event cell name. Maximum activities rewrites the literal attribute-key
parameter even on unchanged-source returns or before a bad-bound error. Its
equal-frequency ranking is stable first-seen. Relative omission uses event
counts over **all traces**, including empty traces; true uses cases containing
the value over all traces; false uses events over all events. Integer products
use exact TokenInteger arithmetic. The threshold helper consumes supplied
typed counts and actual cached full trace values independent of the variant
key. A separate native-log File plus typed variant-to-offset mapping makes that
cache genuine and downloadable. No cache is inferred from variant labels.

pandas keeps exact integers, dtypes, categories, indexes and context. Both
numeric whole-case stream predicates are active and ANDed. Supplied count
dictionaries normalize equal keys with first position/last count, retaining
nanosecond datetime distinctions. Supplied None differs from omission. An empty
supplied counter can bypass activity-column lookup. The public valid integer
activity-limit helper returns the complete unchanged frame; the UI states that
captured behavior. Negative numeric event filtering retains missing values when
requested; other matching modes add missing values before mask inversion.

Polars counting has a typed value/count result, separate from filtered source
rows. Omitted relative mode counts events over distinct case IDs; false uses
events over events; true uses distinct case/value pairs over cases. Case keys
are inactive for false relative mode and active for true counts. Integer case
selectors form literal join groups and append the corresponding native literal
column. Frequency-cutoff tie identity remains explicitly open: nine retained
ambiguous outputs are accepted only by the bounded rule requiring every
higher-frequency group, exactly k complete groups, cutoff-only remaining groups,
all occurrences and original row order/schema/context. Count-map entry order
is unclaimed. Original reference snapshots remain unchanged.

Fresh public banks are **executed once and accepted; never replay them**:

- LOG: `build/filtering-attributes-log-public-actual-1` (255),
  `...log-boundaries-public-actual-1` (12),
  `...log-parameters-public-actual-1` (4),
  `...log-forwarding-public-actual-1` (4),
  `...log-ranking-public-actual-1` (2), and
  `...log-relative-public-actual-1` (3). Continuations reconstructed and asserted
  all 255, 267, 271, 275 and 277 complete prior receipt hashes before entry.
- pandas: `build/filtering-attributes-pandas-public-actual-1` (429), then
  `...pandas-continuation-public-actual-1` (19), reconstructing all 429 first.
- Polars: `build/filtering-attributes-polars-public-actual-1` (700),
  `...polars-relative-public-actual-1` (18), then
  `...polars-ties-discriminating-public-actual-1` (8), reconstructing all
  700 and then 718 complete receipts before new entry.

The metadata-only Polars eight-call recipe at
`build/filtering-attributes-polars-ties-public-prepared-1` remains **unexecuted**
with its NOT_EXECUTED reason. Its replacement changed actual activity values,
orders, frequencies and limits. Never execute the rejected recipe. Every
executed bank deduplicated all current calls and all full prior receipts before
the first entry. No reference implementation source, AST, bytecode or private
state was inspected. Opaque hashes and public calls/surfaces remain the boundary.

Final acceptance passes **41 assertion-enabled native suites** in
`build/parity-closures/native-current/ctest-attributes-final.log`, with build
`build-attributes-final.log` and configuration
`configure-attributes-workflows-first.log`. pandas passes all 335 complete
results / 113 diagnostics; log all 280 outcomes plus native storage controls;
Polars all 726 bounded outcomes. Failed prebuild/first-module/intermediate
receipts and matching pre-repair source/compiled identities remain retained.
The pandas INT64_MIN fixture repair changed literal representation only; all
448 expected outcomes stayed unchanged. Log reconstruction/ranking/relative
repairs and Polars literal-case/Greek-mu diagnostic repairs changed general
implementation behavior; all earlier captured expectations remained unchanged.
The Polars driver only gained an exception reporter to expose its original
unhandled failure. Root manual missing-case HTTP fixture also required clearing
its nonempty legacy case-ID fallback; original fixture and failure are retained.

Production acceptance passes **95 actual HTTP checks** in
`build/parity-closures/attributes-production-http-4`; its independently owned
temporary server exits. Server SHA256:
`1d6b11e78d1197130a162db0419df62ec62e906de4cbee1db60e26e74c54b1e3`.
Both actual production routes cover genuine uploads, typed controls, complete
source/result/cache artifacts, error mutations, native/XES reopening, strict
multipart/query validation and empty-filename File presence. The existing
application was not replaced. **Rendered browser interaction is unverified.**
The first HTTP harness stopped after 64 checks because its count-result reader
assumed the ordinary EventTable shape; the real count artifact used the valid
complete raw-byte carrier. The repaired harness understands both carriers and
reuploads the actual raw artifact. The original recipe, response, failure and
server exit receipt remain retained. The second stopped after 82 because it
compared threshold output against trace-filter properties, despite the captured
threshold property reset. The third stopped after 85 because it assumed generic
XES retained a custom native primary activity role. Final assertions clear only
the documented threshold trace properties and pass the explicit original
activity role on XES reupload, checking actual literal event attributes. Every
failed recipe/response and owned-server exit remains preserved; no product
changes were needed for these three harness corrections.

Frontend acceptance passes **190 files /
9809 tests** in
`build/parity-closures/frontend-attributes-full-third-final.log`; focused 65,
TypeScript and Vite also pass in the matching `*-third-final.log` receipts.
Earlier full runs (9,808 then 9,809 tests) remain historical; final copy reflects
the newly discriminated log relative default and negative numeric-event missing
behavior. The offline declaration catalogue passes in
`catalog-attributes-final.log`; it reads the retained declared catalogue, never
reference implementation source. Initial missing project-root failure remains.

The root archive `docs/attributes-filter-workflow-checks.jsonl.gz` has
163 members; SHA256 `78a742fc4b00333d4807c50f1b5e69de24511e2c700879ba6637381e8d2f641e`.
It binds 41 sources,
100 compiled artifacts and all current acceptance
receipts plus four engine archives. Run
`build/parity-closures/verify-attributes-current-bindings.py` for independent
post-commit source/compiled/receipt/archive and committed partial-row validation.
Earlier variants proofs remain historical after shared source/server changes.

Keep all 201 open units in scope. Useful next bounded attributes questions
include pandas zero-count unused-category statistics mixed with missing values
and activity limits near observed versus declared category counts. No such new
reference calls have been made. Use genuinely new effective queries and full
prior receipt barriers. Other remaining module families stay in the full
catalogue; this checkpoint does not narrow the objective to filtering.

Preserve inherited dirty comparison metadata in
`docs/reference-2.7.22-coverage.json` (31 insertions / one deletion) and historical
`docs/reference-2.7.22-parity.md` (one insertion / one deletion), plus untracked
root node_modules and Python caches. No app replacement/browser workaround is
authorized by the prior rejection. Root alone runs future reference recipes;
workers prepare offline. User authorization for parallel subagents, clean
logical commits and direct main pushes persists. Leave the full-parity goal
active.

## Latest variants checkpoint

Four functional commits are pushed after `e335a196`:

- `7c2bc9f2`: log variants companion, all seven public functions/helpers;
  380 retained outcomes, 258 returns / 122 errors.
- `374668fa`: pandas variants companion, genuine precomputed indexed
  mappings and typed controls; 491 outcomes, 357 tables / 134 diagnostics.
- `c3b5d97b`: Polars eager/lazy variants companion; 411 outcomes,
  142 complete projected frames / 269 type/phase/message diagnostics.
- `14e5be15`: usable log/table variants controls, genuine uploads,
  complete artifacts, all four pandas mapping modes and production acceptance.

Read `docs/variants-filter-workflow.md` and the three engine contracts. All
**1,282 distinct public calls** have **1,282 matched projected native outcomes**.
The three canonical variants rows remain partial; only evidence pointers changed.
Full scope stays **86 packages / 387 modules**, **46 complete / 40 partial
packages**, **226 complete / 161 partial modules**, **201 strict open units**.

Membership explicitly distinguishes list and tuple sequences. Route omission
keeps lists; UI defaults explicitly request tuples. Lists do not match log/
pandas tuple variants; Polars joins inner sequences with commas and preserves
encoding collisions. Log membership/rank/individual coverage emit first-seen
variant groups; cumulative emits ranked groups including frequency ties and
ignores positive. All trace/log context survives. Original activity roles are
retained separately for XES reopening.

pandas membership groups actual case rows; ranking/coverage statistics instead
take contiguous slices starting at each case's first row, sized by total count.
Timestamp controls are inert in all four functions. Genuine `variants_df`
mapping is active in **all four** modes; the UI and route accept a separate
indexed EventTable File. Statistical selection uses source variants then maps
case identities through that supplied frame. Its complete source and unchanged
check are downloadable.

Polars membership always uses canonical time:timestamp. Top-k uses its selected
timestamp for statistics, then canonical time for final membership. Top-k lazy
construction scans at apply and returns a restartable membership result; errors
retain apply/collect phases. Public from-list/auto helpers fail with unavailable
DEFAULT_NAME_KEY; the UI offers working membership/top-k modes and documents
that limitation. All 176 captured Python deprecation-warning observations and
30 category snapshots remain retained; native Python warning emission is not
implemented. This gap is explicit, not a closure claim.

The following fresh banks are **executed and accepted; never replay them**:

- `build/filtering-variants-log-public-actual-1`: 370 distinct from 375
  candidates, five removals before entry;
  `build/filtering-variants-log-boundaries-public-actual-1` adds 10,
  reconstructing all 370 complete prior receipts first.
- `build/filtering-variants-pandas-public-actual-1`: 439 distinct from 441
  candidates, two removals before entry;
  `build/filtering-variants-pandas-continuation-public-actual-1` adds 52,
  reconstructing all 439 complete receipts first.
- `build/filtering-variants-polars-public-actual-1`: 399 distinct;
  `build/filtering-variants-polars-casts-public-actual-1` adds 12 after reconstructing
  all 399 complete receipts. Public lazy plans and category-observer warnings
  remain separate from actual collection outcomes.

All new bank barriers reconstruct and assert saved full receipt hashes before
entry and deduplicate the complete current bank. No overlaps, replays or source/
control mutations were observed. No reference implementation source, AST,
bytecode or private state was inspected. The earlier timestamp audit-timing
correction below remains authoritative for those historical banks.

Final acceptance passes **34 assertion-enabled native suites** in
`build/parity-closures/native-current/ctest-variants-final.log`, with successful
`build-variants-third.log` and `configure-variants-third.log`. The first build
succeeded but first CTest passed 33/34: Polars top-k statistics alias resolution
needed insertion order while final membership remained enum-preferred. The
second successful build's direct Polars test reached eager 370 and caught eager
missing-column errors carrying lazy plan context. General repairs separate
statistics/member parameter resolution and eager/lazy error formatting. Expected
vectors and generated tests remain byte-identical. Both earlier sources,
compiled identities, build/failure receipts and repair audits are preserved in
the Polars archive's initial-native and statistics-binding-native directories.

Production acceptance passes **50 actual HTTP checks** in
`build/parity-closures/variants-production-http-1`, then the owned server exits.
Server SHA256: `e6037fea1b18de19e983595c1d7bf22281288496d3161d35e87936ea9441df90`.
Tests cover tuple/list transport, complement/rank/coverage, all four real pandas
mapping modes, exact integers, literal NUL/Unicode roles, native/XES reopening,
captured unavailable helpers, error phases and strict multipart controls. The
existing application was not replaced; rendered browser interaction is unverified.

Frontend acceptance passes **186 files / 9,755 tests** with `--maxWorkers 2` in
`variants-frontend-full-final.log`, plus 38 final focused tests, TypeScript and
Vite (`variants-frontend-focused-final.log`, `variants-frontend-tsc-final.log`,
`variants-frontend-vite-final.log`). Sources are frozen after these checks.
The earlier focused failure preserves an old test assumption that admitted_json
only applies to start/end; the operation-specific assertion was extended. Final
catalogue declarations pass in `catalog-variants-final.log`; the first run's
untracked-evidence failure remains retained in `catalog-variants-first.log`.

`docs/variants-filter-workflow-checks.jsonl.gz` retains **108
members**, **35 source bindings**, **79 compiled identities**;
1,312,761 bytes, SHA256 `b73e3ade7c4ae84a885adf01101fe58b42594f0a6c17967755390e3529c846fa`.
Manifest: `build/parity-closures/variants-workflow-evidence-manifest.json`.
`verify-variants-current-bindings.py` verifies every current source, compiled
identity, receipt, engine archive and embedded member, plus three committed
partial evidence rows. Dedicated module archives retain all public observations,
frozen recipes, complete outcomes, current sources and acceptance receipts.

Remaining variants work includes unobserved dtype/value/selector combinations,
mixed comparison/diagnostic orders, extension and custom object domains,
warning emission, Python/category identity, physical lazy plans and performance.
All earlier timestamp, paths, cases, between, repetition, OCDFG, connectors,
platforms, solvers, rendered UI and other catalogue gaps remain in scope. There
are no owned functional drafts. Preserve inherited comparison metadata in
docs/reference-2.7.22-coverage.json and the historical parity-markdown edit, plus
untracked root node_modules and Python caches.

## Latest timestamp checkpoint

Four functional commits are pushed after `b8d32d3b`:

- `9851f4e1`: log timestamp filters and public predicates/stubs, including
  faithful event-converter source mutations; 342 retained outcomes.
- `abdd64e1`: pandas timestamp filters, exact min/max nanoseconds, typed
  parameters and exact large index labels; 861 retained outcomes.
- `83248582`: Polars eager/lazy timestamp filters, strict intersection,
  complete diagnostics/dictionaries and exact integer epoch boundaries;
  732 retained outcomes.
- `fa6baa70`: usable timestamp controls, complete before/after source snapshots,
  genuine table uploads, log/XES reopening and production HTTP acceptance.

Read `docs/timestamp-filter-workflow.md` and the three timestamp contracts.
All **1,935 distinct public calls** have **1,935 exact projected native outcomes**:
342 log (183 returns / 159 errors), 861 pandas (302 tables / 559 errors), and
732 Polars (229 frames / 503 full type/phase/message diagnostics). All three
canonical rows remain partial; only their evidence pointers changed. Counts
remain **46 complete / 40 partial packages**, **226 complete / 161 partial
modules**, and **201 strict open units** across all 86 packages / 387 modules.

String endpoints use the strict public `YYYY-MM-DD HH:mm:ss` parser and UTC
normalization. Typed bounds retain their separately observed behavior. Log
trace windows use first/last source positions and inclusive intersection;
dataframe trace windows use min/max timestamps and preserve row order. pandas
uses strict overlap; Polars has three strict clauses and excludes a trace
exactly spanning the window. Only log/pandas start/completion modes use
`positive`; pandas additionally supports its missing-time control. Polars has
no public start/completion API. Generic `apply`/`apply_auto_filter` entry points
raise the captured unavailable-method exceptions; do not invent implementations.

The log event filter injects trace attributes into source events before some
errors, then reconstructs selected traces. All **38 source/control mutations**
are asserted. Native const overloads explicitly copy; the mutable overload
retains the public effect. CSV workflow preparation separately adds the
conventional canonical trace identity while preserving the original case
column. Native JSON/XES inputs retain their actual stored keys and can raise
missing-canonical-ID errors. The public filter does **not** forward the tested
case-key parameter. Complete parsed source snapshots before and after filtering
are available even on later native errors.

The following new banks are **executed and accepted; never replay them**:

- `build/filtering-timestamp-log-public-actual-1`: 326 calls, 176 returns /
  150 errors, 25 mutations. `filtering-timestamp-log-converter-public-actual-1`
  adds twelve calls (seven returns / five errors, nine mutations).
  `filtering-timestamp-log-forwarding-public-actual-1` adds four errors and four
  mutations. All three banks have zero emitted warnings.
- `build/filtering-timestamp-pandas-public-actual-1`: 484 calls, 24 tables /
  460 errors. `filtering-timestamp-pandas-canonical-public-actual-1` adds
  377 disjoint calls, 278 tables / 99 errors, after reconstructing all 484
  previous unique full receipts. The first bank's rejected ISO `T` strings
  remain authoritative; the second changes actual arguments to canonical space
  strings, preserving the original fixtures. Both have zero mutations/warnings.
- `build/filtering-timestamp-polars-public-actual-1`: 360 calls, two frames /
  358 errors, including strict-parser and naive/UTC dtype diagnostics.
  `filtering-timestamp-polars-utc-public-actual-1` adds 360 disjoint calls,
  215 frames / 145 errors, after reconstructing all 360 preceding receipts.
  `filtering-timestamp-polars-numeric-public-actual-1` adds twelve returned
  frames after reconstructing all 720 preceding receipts. It distinguishes
  exact Int64/UInt64 epoch microseconds above 2^53 from Float64 rounding.
  Public unoptimized plans remain separate from physical execution claims.

**Audit timing correction:** the original log recipe preparation note overstates
its full-prior reconstruction. Before entry, it hashed complete new actual
receipts and compared against saved full-call hashes, including all 338 prior
hashes before the four forwarding calls. Root independently reconstructed the
first 326 full receipts before the twelve-call continuation, but reconstructed
those next twelve only **after** the forwarding run. The original recipes and
preparation notes remain preserved with `audit-timing-correction.json` and the
after-entry proof. Independent reconstruction found no duplicate or replay.
Do not claim all 338 were reconstructed before that run. Future continuations
must reconstruct every retained full receipt before any new entry. pandas and
Polars continuations already enforce that barrier.

Final acceptance passes **29 assertion-enabled native suites** in
`build/parity-closures/native-current/ctest-timestamp-second.log`; the matching
successful build is `build-timestamp-second.log`. The first build succeeded,
but its first native run passed 27/29: log intersection fetched the final
timestamp too late, and pandas date extrema collapsed adjacent nanoseconds
through the general microsecond scalar comparator. General repairs eagerly
read both log intersection endpoints and compare exact instants in pandas
date aggregation; containment retains its observed lazy access. The immutable
expected fixtures are unchanged. Original sources, compiled hashes, direct
failures and the first 29-suite failure remain retained in dedicated archives.

Production acceptance passes **50 actual HTTP checks** in
`build/parity-closures/timestamp-production-http-2`, then the owned server exits.
Server SHA256:
`a90c0d6f519386992266bdabc8fddd70fdf27b85cad723ec4d70c49e95253b4f`.
The first HTTP run stopped at request 44: the harness supplied EventTable date
JSON inside a raw native-log attribute. Its original script/records/failure
remain preserved; the second uses the existing raw attribute schema. No product
workaround was made. The existing application was not replaced; rendered
browser interaction remains unverified.

Frontend acceptance passes **185 files / 9,737 tests** in
`build/parity-closures/timestamp-frontend-full-final2.log`, with `--maxWorkers 2`.
The first default-pool full run retains 9,736 passes and one existing 5-second
timeout while native compilation was active. A focused first run caught the
old all-operation test's assumption that every new time mode sends `positive`;
the updated operation-specific assertions pass in the second 60-test focused
run. TypeScript and Vite pass in `timestamp-frontend-tsc-final.log` and
`timestamp-frontend-vite-final.log`; output is `timestamp-frontend-final-dist`.
No frontend sources changed after those checks. Offline declaration checks
pass in `catalog-timestamp-final.log` with unchanged full catalogue counts.

`docs/timestamp-filter-workflow-checks.jsonl.gz` retains **100 members**, **34
source bindings** and **68 compiled identities**. It is 1,475,036 bytes, SHA256
`23994de0c2d3736719c4da8cfbf660665f3f993aee1eb1e32c04786813316c52`.
Manifest: `build/parity-closures/timestamp-workflow-evidence-manifest.json`;
packager: `package-timestamp-workflows.py`. The offline
`verify-timestamp-current-bindings.py` verifies **171 current bindings**, all
100 embedded member hashes and the three committed partial ledger rows.
Dedicated module archive hashes are:

- Log public: `5bf07e1c85251d670fc22f9cd10acc6b1f4d0c3c0a5b2a5226a558fe4d8b3ee7`;
  log checks: `57629131321c6b323a968ef78c1677a34c34cad08a3da341e1d102db71537987`.
- Pandas checks: `6c3247f8a5310ada91d37cc6964a47e620cf7c69e5a71687fdcc7a036988c8d0`.
- Polars checks: `87d84557b34d39d6e3f41e4dea5c79a0f0fd54d6454b50d2215532871f2ef404`.

Next timestamp work should discriminate remaining parser/locale/year domains,
mixed temporal wrapper/zone identities, extension/object dtypes, unobserved
group/cast/internal-column domains, warnings and category/physical lazy plans.
All earlier paths, case, between, repetition, OCDFG, connectors/platforms,
solvers, rendered UI and performance gates remain in scope. No owned functional
drafts remain. Only public calls/docs/enums/plans and opaque hashes were used;
no reference implementation source, AST, bytecode or private state was inspected.

## Previous adjacent-path checkpoint

Four functional commits are pushed after `010ec3db`:

- `6d9b1a85`: log paths membership/performance and public count/threshold/
  dictionary helpers, with 182 complete outcomes and separate public/checks archives.
- `54f02501`: pandas paths membership/performance, typed parameters, exact
  complete table/index snapshots and 266 outcomes.
- `35aa54ed`: Polars eager/lazy paths helpers, dictionaries and full diagnostics,
  with 238 outcomes and retained EOF-only driver cleanup evidence.
- `849d5487`: usable log/table paths controls, genuine upload, complete
  artifacts, literal roles, XES reopening and actual production acceptance.

Three independent native paths companions and usable log/table workflows now
cover **686 distinct public calls**, with **686 exact projected outcomes**:
182 log (118 returns / 64 errors), 266 pandas (155 tables / 111 errors), and
238 Polars (153 frames / 85 full type/phase/message diagnostics). All three
canonical rows remain partial; only their evidence pointers changed. The full
catalogue remains **46 complete / 40 partial packages**, **226 complete / 161
partial modules**, and **201 strict open units** across all 86 packages and
387 modules. No finite bank is a claim of whole-module closure.

Read `docs/paths-filter-workflow.md` and the three `*-paths-filter-parity-contract.md`
contracts. Ordinary paths retain complete matching cases; performance paths use
inclusive duration bounds. Log preserves original trace order; dataframe
engines sort case/timestamp before adjacency. Pandas uses fractional elapsed
seconds and Polars integral seconds. Comma-key collisions, split source/target
columns, literal selectors, typed diagnostics, metadata/dictionaries/index and
complete downloads have explicit acceptance. Performance ignores the native
target-column parameter; the usable HTTP workflow rejects that inactive control.

New banks are **executed and accepted; never replay them**:

- `build/filtering-paths-log-public-actual-1`: 174 distinct calls;
  `filtering-paths-log-temporal-public-actual-1`: four;
  `filtering-paths-log-helper-edge-public-actual-1`: four. The latter bank
  audited all 178 preceding full receipts before entry. All 182 log outcomes
  have complete warning lists, with zero warnings and mutations.
- `build/filtering-paths-pandas-public-actual-1`: 250 distinct calls after two
  duplicate candidates were removed before entry;
  `filtering-paths-pandas-continuation-public-actual-1`: 16 disjoint calls after
  auditing the preceding 250 full receipts. The first bank's plain parameter
  dictionaries deliberately retain nested tag dictionaries as actual values;
  the continuation recursively decodes real NaN/infinity/Timestamp scalars.
  Preserve both banks as observed. All 266 have zero warnings or mutations.
- `build/filtering-paths-polars-public-actual-1`: 226 distinct calls;
  `filtering-paths-polars-strings-public-actual-1`: twelve disjoint calls after
  auditing the preceding 226 full receipts, with public unoptimized plans.
  One algorithm warning and fifty dictionary-observer warnings remain
  retained; native warning emission and physical plans remain open.

Only public calls/signatures/docs/enums, public plan output and opaque hashes
were used. Root executed all reference recipes; workers implemented offline.
No algorithm call was replayed and no implementation source was inspected.

Final acceptance passes **24 assertion-enabled native suites**, **44 actual
production HTTP checks**, **184 frontend files / 9,726 tests**, TypeScript and
Vite. Native receipts are under `build/parity-closures/native-current/`;
frontend receipts are `paths-frontend-full-final.log`,
`paths-frontend-tsc-final.log`, `paths-frontend-vite-final.log` under
`build/parity-closures/`, with output `paths-frontend-final-dist`.
HTTP acceptance is `build/parity-closures/paths-production-http-3`; server SHA256
`4a2702f822276268827516bfbd46a691dc1f3d98e9f64f81ce2af76f08d3f825`.
Every owned server exited; the existing application was not replaced.
Rendered browser interaction remains unverified. Offline catalogue validation
passes in `build/parity-closures/catalog-paths-final.log`.

The successful coordinated build is `build-paths-final2.log`, with all 24
suites in `ctest-paths-final2.log`. Final staged whitespace checking found one
extra EOF newline in the Polars test driver. Its original driver and archive
are retained under `build/parity-closures/polars-paths-filter-acceptance/pre-eof/`;
the byte audit proves only that terminal LF was removed. The repaired target
was rebuilt in `build-paths-eof-final.log`, then all 24 suites passed again in
`ctest-paths-eof-final.log`. The final Polars and workflow archives bind that
driver/object/executable/map. Log and pandas evidence remains bound to their
unchanged final2 sources/binaries. No frontend or production server bytes
changed during this cleanup.

`docs/paths-filter-workflow-checks.jsonl.gz` retains **104 members**, **36 source
bindings** and **57 compiled identities**. It is 1,005,938 bytes, SHA256
`ac809026b42665a05b7aa6089c2019502e569f26af487b33ad3e0d71148b3a93`.
Manifest: `build/parity-closures/paths-workflow-evidence-manifest.json`;
packager: `package-paths-workflows.py`. The offline
`verify-paths-current-bindings.py` verifies all **164 current bindings**, all
104 embedded member hashes and the three committed partial ledger rows.
Dedicated module archive hashes are:

- Log public: `1e642d9d952c77ea333603268df2ed0a467f03ad91678216d14fcf156a81ddc7`;
  log checks: `6598bdfa4fa2db698bc93efa4b68146ebd45a7740912e2c64b65f740c7b83b5f`.
- Pandas checks: `27060361e64d87cf43d0d17360d73db20792a1d35bb38b54909a47b13c827925`.
- Polars checks: `15a6e4e5f3cd41effb55605186669295c3de96222e887aea51f3deb7a9109bb9`.

The first build's pandas private-function ADL collision and the initial three
native failures remain retained. Repairs preserve dictionary-membership
precedence in the log helper, project captured pandas Timestamp operands
independently of precision, and distinguish Polars eager direct predicates
from lazy cached predicates in complete temporal diagnostics. No expected
outcome was rewritten. HTTP attempts one and two retain their original scripts,
records and failures: their reopen calls omitted the exported activity selector.
The third uses that selector explicitly for XES/native/context reopening.
The workbench also discovers actual non-CSV roles, accepts the literal fallback
when no first-class role exists, and has a CSV-to-XES reopening regression.

Prior case, between, repetition and OCDFG evidence remains unchanged and
historical. All their declared gaps remain in scope. Next paths work should
use fresh discriminating banks for nullable/missing case IDs, equal timestamp
ties across typed cases, extreme pandas index labels (use exact integer
arithmetic), mixed temporal wrapper/zone domains, custom object protocols,
extension dtypes, helper-column collisions, warnings and Polars physical
plans/category identity. Do not infer exact log/pandas internal duration-limit
sentinels from acceptance over the supported datetime calendar.

## Previous complete-case checkpoint

Four further functional commits are pushed after `fb1e0e7c`:

- `b6a0cc81`: public log size/count/performance helpers, explicit unavailable
  generic entry points, typed predicate/error behavior and complete selected
  traces with observed empty log-level context; 180 retained outcomes.
- `e1988a54`: pandas case helpers, count-frequency selection, first/last
  duration, business-hour controls and complete table/index/dictionary context;
  259 retained outcomes, including 157 full tables and 102 exact diagnostics.
- `57b6f280`: Polars eager/lazy case helpers, max-minus-min integral-second
  duration, full apply/collect diagnostics and explicit count-tie acceptance;
  225 calls have 134 exact frames, 79 full errors and 12 admitted tie projections.
- `ce4898f8`: usable log and table size/count/duration controls, genuine table
  upload, literal roles, complete downloads, correct log reopening and production
  HTTP acceptance. Read `docs/case-filter-workflow.md` and the three case-filter
  contracts. All three canonical case rows remain partial; only their evidence
  pointers changed in the ledger. Counts remain 46/40 packages, 226/161 modules
  and 201 strict open units. All 86 packages and 387 modules remain in scope.

Final acceptance passes **19 assertion-enabled native suites** in
`build/parity-closures/native-current/ctest-cases-final2.log`, including the
689 representable between outcomes and both retained table repetition suites.
The main successful build is `build-cases-final.log`; a trailing blank line in
the Polars test driver was removed after diff checking, then that target was
rebuilt in `build-cases-eof-final.log` and all 19 suites passed again. An offline
byte audit retains both drivers and proves only terminal whitespace changed.
The initial build's wrong `EventLog.properties` test field and its corrected
`metadata.properties` form are documented; the failed build and first five-suite
success remain retained. No product workaround was made for either test issue.

The freshly rebuilt server passes **42 actual production HTTP checks** in
`build/parity-closures/cases-production-http-1`, including all operations,
custom/NUL roles, full integer storage, reopening, business hours and failure
phases. Server SHA256:
`44ad78e0753ef501a80a185dc32a74a0c65c359383f7f567b61e31ac558810b2`.
The independently owned server exited; the existing application was not
replaced. Rendered browser interaction remains unverified. Frontend acceptance
passes **183 files / 9,718 tests**, TypeScript and Vite:
`build/parity-closures/cases-frontend-full-final.log`,
`cases-frontend-tsc-final2.log`, `cases-frontend-vite-final2.log`;
output is `cases-frontend-final-dist`. No frontend sources changed afterward.
Offline declaration checks pass in `catalog-cases-final.log`.

`docs/case-filter-workflow-checks.jsonl.gz` retains **80 members**, **35 current
source bindings** and **47 compiled identities** with all final/historical
workflow receipts. Manifest: `build/parity-closures/case-workflow-evidence-manifest.json`;
packager: `package-case-workflows.py`. The archive is 794,512 bytes, SHA256
`a1207e5ed3e3718054f894662e87d8c6da3a766317b1f734657049bbe754485c`.
Dedicated log, pandas and Polars archives retain full public inputs/results,
dedup audits, generators, diagnostics and native evidence. Earlier between and
repetition archives remain historical snapshots and are unchanged.

The following new banks are **executed and accepted; never replay them**:

- `build/filtering-cases-log-public-actual-1`: 180 distinct calls, 88 returns,
  92 errors. Public `satisfy_perf` can return NativeNA; its native carrier is a
  bool/NativeNA variant. Filtering then evaluates truth and can raise.
- `build/filtering-cases-pandas-public-actual-2`: 259 distinct calls, 157 full
  tables and 102 errors. Actual-1 stopped before any method entry because the
  observer used the invalid encoding name `utf8-sig`. Prepared-2 changes only
  that spelling to `utf-8-sig`; the original recipe and zero-call attempt remain.
  Six emitted warnings lack per-call receipts; do not claim warning parity.
- `build/filtering-cases-polars-public-actual-1`: 213 distinct calls, 134 frames
  and 79 errors. `filtering-cases-polars-ncases-order-public-actual-1` adds twelve
  disjoint successful calls and their public unoptimized plans, after auditing
  all 213 earlier full receipts. Across both banks, 134 native returned frames
  compare exactly and 12 count cutoff ties use explicitly admitted semantics;
  do not claim exact captured tie choices. Ninety-six algorithm warnings and
  separate dictionary-observer warnings remain retained, without native emission.

All 664 new public calls have their declared native acceptance. Complete bank
receipts were audited before algorithm entry; no prior direct case-filter calls
were found and no algorithm calls were replayed. Only public calls/docs/enums,
public plan output and opaque module identity hashes were used.

Next case-filter work should target discriminating uncovered combinations:
pandas categorical count ties, nullable numeric IDs with missing values,
subsecond duration boundaries and unobserved typed parameter/extension/schema
domains; log typed parameter adapters, mixed temporal class/resolution/timezone
domains and object identities; Polars unstable cutoff choices, warnings,
additional dtypes/selector expressions, category physical identity and execution
plans. Native Polars uses encounter order among equal-frequency groups; the
retained public plan permits other tie choices and a null group can consume a
slot before disappearing in the join. Keep those limits explicit. All earlier
OCDFG, repetition, between, platform/connector, solver and performance gates
below also remain in scope. No owned functional drafts remain.

## Earlier pushed checkpoints

Fourteen functional continuation commits are pushed after `7849925b`:

- `7cfe40d9`: locale-independent OCEL annotation/conformance JSON, exact signed
  zero and int64 boundaries. The embedded writer is
  `write_ocpn_annotation_fields` (the previous next-step note named a nonexistent
  `write_ocdfg_content`). See `docs/ocel-numeric-json-contract.md`.
- `9c40cfa7`: ordered OCDFG edge associations, three set projections, literal
  selectors and usable explorer tables/download. All seventy-seven retained
  outcomes now have native acceptance across timing/discovery/helpers. Five
  native suites, thirty-six focused frontend tests, TypeScript, Vite and eight
  actual production-handler requests pass. The full module remains partial;
  see `docs/ocdfg-edge-metrics-parity-contract.md`.
- `3def003b`: typed attribute repetition in the existing log-filter workspace.
  All 109 distinct retained outcomes, seven native suites, fourteen production
  HTTP checks, 9,688 frontend tests, TypeScript and Vite pass. Full native JSON,
  XES projection and reopening retain the correct activity selector. The new
  companion preserves lazy bounds, zero-match/empty traces, exact integers and
  NativeNA truth errors; its canonical row remains partial for unsupported
  object domains. See `docs/log-attribute-repetition-parity-contract.md`.
- `9a13274d`: pandas repetition companion, complete table/index/dtype/category
  metadata and 139 retained native outcomes.
- `c03443b0`: Polars eager/lazy repetition companion, 105 retained outcomes,
  literal columns and exact apply/collect error type, phase and full message.
- `7a207581`: 86 further log outcomes for ordinary value shapes/bytes and ten
  exact escaped pandas missing-column errors. Complete error strings have a
  length-preserving native carrier.
- `a5e7de37`: usable pandas/Polars table modes and native table downloads,
  separate engine phases and binary byte target controls in log mode.
- `71ce83af`: exact log between extraction, original endpoint equality,
  zero-based case suffixes and a complete result for typed trace keys;
  169 retained outcomes pass.
- `e035671a`: pandas between extraction on complete tables, dtype-specific
  missing cases, nanosecond equality, index/category/context preservation;
  139 representable outcomes from 140 public calls pass.
- `723f063e`: eager/lazy Polars between extraction with canonical timestamp
  sorting, latest-start slices, lexical interval numbering and full diagnostic
  phases/bytes; all 333 outcomes pass.
- `dc1d36dc`: usable between controls in the log and table workspaces, complete
  downloads, literal controls, correct reopening, actual HTTP acceptance and
  final source/binary evidence bindings.
- `ec101e14`: nullable UInt64/Float64 and float32 case projections, float32
  label formatting, nested Timestamp/stdlib datetime labels and nested NaT;
  18 new full pandas tables pass alongside the unchanged 139-outcome suite.
- `156c5eae`: Polars category-label dictionaries and Enum declaration order,
  cached Enum ranks and strict missing-dictionary/unknown-value rejection.
  Existing table metadata now admits Polars dictionaries without changing its
  ABI; complete codec/lazy/workbench transport and shared validation regressions
  pass. Ten fresh complete dictionary-label projections extend Polars to 343.
- `2cd4127c`: typed log selector diagnostics for dates, float/signed-zero/NaN,
  frozensets, recursive keys and NaT, plus typed result keys and lazy hashing.
  All 189 log outcomes pass; previous evidence archives remain unchanged.

The earlier seven functional commits after `e68e191d` remain relevant:

- `0d8e4399`: exact OCEL object attributes and shared typed NaT membership.
- `c552c2db`: usable exact object/NaT preparation controls and HTTP dispatch.
- `fa4980cb`: OCDFG working-time helper and optional discovery timing.
- `73d7de01`: working-time controls in the existing explorer and HTTP route.
- `742f24fd`: faithful OCEL activity/type matching companion.
- `4862885e`: usable exact matching, literal transport and receipt correction.
- `eed5ed07`: typed missing timestamps, canonical field access and corrected
  scalar/datetime projection in the native OCDFG adapter.

Coverage is **46 complete / 40 partial packages**, **226 complete / 161 partial
modules**, and **201 open strict coverage units**, down from 203 at the prior
handoff. Full parity is not complete. The whole OCDFG row remains partial.
Read `docs/parity-closure-status.md` and the machine-readable coverage ledger.

## Delivered behavior and evidence

Exact object filtering matches all 195 retained complete native projections
and both native errors. The new event NaT bank matches 12/12 returned logs.
Typed NaT is distinct from null and numeric NaN; scalar/date kinds, literal
names, exact numeric comparisons, unused selected objects and stable six-table
propagation are retained. See `docs/ocel-object-attributes-parity-contract.md`
and `docs/ocel-event-nat-membership.md`, with their compressed archives.

Exact activity/type matching matches all 155 returned rows and eight native
errors from a 138-call bank and a disjoint 30-call continuation. **One duplicate
exists inside the first bank** (`mapping-subset-1` / `mapping-key-domain-7`).
The 168 calls contain 167 distinct explicit receipts; the 155 successful rows
contain 154 distinct successful receipts. Both records and an offline audit are
preserved. Neither bank was rerun. Do not call this 168 unique observations or
an entirely replay-free bank. See the matching contract and evidence archive.

The preparation workspace now exposes five exact selectors, including
`object_attribute_exact` and `activity_type_matching_exact`. Matching uses
`allowed_json`, optional literal role names and omitted versus literal
temporary-column/separator controls. Multiline controls, complete downloads
and ordinary reopening are supported. Final snapshots and receipts are in
`docs/ocel-exact-filter-workflow-checks.json.gz`.

OCDFG acceptance covers 60 retained direct timing outcomes, eight complete
discovery outcomes/errors, default/custom/empty/null schedules, wall-clock
working time and inert calendar JSON. Absent optional options preserve the
legacy branch. Primary timestamps use datetime semantics; generic timing
columns require native event-type datetime declarations. Datetime NaT yields
zero elapsed time and the retained working-time error. Scalar/undeclared
strings, including ISO-looking text and NaT, remain literal. Discovery JSON
is locale-independent and retains negative zero. See
`docs/ocdfg-working-time-performance.md`. Its earlier acceptance archive is
historical; the separate typed-adapter archive binds the corrected final
sources and retained dtype projection. No new timing reference calls occurred.

The OCDFG explorer now exposes the other nine retained edge helper outcomes:
ordered association triples and event-pair, full-triple and object-ID sets.
Literal role overrides, paginated inspection and complete JSON download are
available. All 77 retained OCDFG outcomes have native acceptance, but arbitrary
relation cell/dtype domains and the performance gate remain open.

Repetition now has separate log, pandas and Polars companions rather than a
shared grouping approximation. The public banks contain 195 distinct log calls,
150 pandas calls and 105 Polars calls. The native counterparts cover 195 log,
149 pandas (missing-required-argument call excluded) and 105 Polars outcomes.
Pandas/Polars consider cases observed among matching rows even at minimum zero;
the log module can retain zero-match and empty traces. The companions preserve
typed numeric/date/byte behavior, stored context and full returned rows.
Their three canonical module rows remain partial for the documented remaining
carrier/dtype/object domains. Read the three repetition contracts and
`docs/table-attribute-repetition-workflow.md`.

The filtering workspace offers separate native table modes with engine-specific
examples, complete metadata/integer-storage transport and full result/response
downloads. It does not convert those tables into logs. Log mode accepts typed
bytes as hexadecimal target JSON and retains native JSON/XES/opening behavior.
Earlier evidence archives are historical source snapshots. The repetition
workflow archive bound 21 sources at that checkpoint; the new between workflow
archive bound 34 sources at that checkpoint. The latest ordinary continuation
archive binds 41 current implementation/test/contract/CMake sources and 35
compiled identities; older archives remain historical snapshots.

Between now has separate native log, pandas and Polars companions. Its banks
contain 189, 158 and 343 distinct public calls respectively, with all 689
representable native outcomes passing. The missing-required-argument pandas
call is outside its C++ signature. Log boundary reuse depends on original
endpoint equality: scalar versus singleton array differs, and array order and
duplicates matter. Pandas keeps positional case blocks and the first open
start; Polars sorts by selected case and canonical `time:timestamp`, keeps the
latest start and ranks textual interval indexes lexically. Thus `10_11` can
receive a lower number than `2_3`, while output stays in sorted row order.
Complete output rows/context and the supported typed native carriers survive.

The numeric/temporal continuation covers missing UInt64/Float64 array
projections, float32 shortest labels and magnitude thresholds, nested
nanosecond Timestamp/timezone labels and standard datetime constructor forms.
Polars dictionaries preserve unused category labels and declaration order.
Categorical case sorting is lexical; Enum sorting follows its dictionary order.
Selected Enum sort columns require complete dictionaries, and unknown values
are rejected even for one-row inputs. Rank maps are built once per collection.
Named dtype declarations survive as text, but runtime category context aliasing
and physical codes are not represented. Category metadata API warnings are
retained separately from actual algorithm warnings. The rows remain partial.

Log nonstring result keys use an explicit typed sidecar; EventLog projection
rejects a nonempty sidecar. The workspace exposes literal string selectors and
offers complete result, native JSON, XES and response downloads. Table mode
accepts genuine native EventTable JSON Files, raw string/array endpoints and
literal case/activity/separator controls. It retains engine-specific errors
and full table downloads without converting tables to logs. All three
canonical rows remain partial; read their between contracts and
`docs/table-between-filter-workflow.md`.

## Prior ordinary-between validation

- **Fourteen assertion-enabled native suites pass**: all between targets,
  new pandas ordinary cases, original EventData validation, category codec/lazy/
  workbench transport, log/table routes and both table repetition regressions.
  Logs: `build/parity-closures/native-current/ctest-between-ordinary.log` and
  `build-between-ordinary.log`; configure: `configure-between-ordinary.log`.
- All **689 representable between outcomes** from 690 public calls pass,
  including complete returned values and retained diagnostic bytes/phases.
  The one excluded pandas call omits a required C++ argument. The latest 48
  public calls were disjoint from every earlier retained receipt; none replayed.
- **37 final production requests pass** on an independently owned server:
  `build/parity-closures/between-ordinary-production-http-2`. It exited and did
  not replace the existing application. Server SHA256 is
  `858abef8eca626a36754b884b02c53a018197d6d22cbd39db2d958c6bda8a4f5`.
  The first attempt's harness incorrectly compared minimal upload fields with
  canonical exports; its script/records/failure context remain retained. The
  corrected check compares the exported dictionary with the parsed source.
  No product change was needed for that harness failure.
- Frontend source/tests remain unchanged from the passing **182-file /
  9,711-test**, TypeScript and Vite checkpoint. Eight current file hashes are
  verified in `build/parity-closures/between-ordinary-frontend-unchanged.json`;
  no redundant frontend run was used for this C++/metadata-only continuation.
- Offline structural catalogue validation passes:
  `build/parity-closures/catalog-between-ordinary.log`. Counts remain 46/40
  packages, 226/161 modules and 201 strict open units; no row was promoted.
- `docs/between-ordinary-workflow-checks.jsonl.gz` retains 84 members, 41 current
  sources, 35 compiled bindings and all final acceptance receipts. Manifest:
  `build/parity-closures/between-ordinary-evidence-manifest.json`. Dedicated
  selector, pandas-ordinary and Polars-categorical archives retain the new
  public observations, pre-entry audits, generators and native proof.
  Original 169-log, 139-pandas and 333-Polars evidence remains historical and
  unchanged. Rendered browser interaction remains unverified.

## Earlier between foundation validation

- **Seven assertion-enabled native suites pass**, including the three between
  companions, log workbench/route and new table workbench/route:
  `build/parity-closures/ctest-between-workflows-4.log`.
- Final native/server build:
  `build/parity-closures/build-between-workflows-4.log`.
- Frontend **182 files / 9,711 tests**, TypeScript and Vite pass:
  `build/parity-closures/frontend-between-final-{full,tsc,vite}.log`;
  output: `frontend-between-final-dist`. Full tests used two workers and
  `--testTimeout 30000`. An accidentally duplicated runner was stopped with
  its own abort receipt; its output is not used as acceptance.
- **33 final production requests pass** on an independently owned server:
  `build/parity-closures/between-production-http-3`. It exited, and the existing
  application was not replaced. Server SHA256 is
  `3277e18249ed4129441023219f6d3ffc1608afab4a857bdc1e93fcdcf321eccc`.
- Offline structural catalogue validation passes:
  `build/parity-closures/catalog-between-final.log`. Counts remain 46/40
  packages, 226/161 modules and 201 open strict units. No between row was
  promoted from a finite bank.
- `docs/between-filter-workflow-checks.jsonl.gz` retains 75 members, current
  34-source bindings and six compiled bindings, verified by
  `build/parity-closures/between-evidence-manifest.json`. Dedicated public/native
  archives retain all three families' recipes, pre-entry dedup, complete
  returned outputs and diagnostics. Older archives remain historical snapshots.

The initial log compile needed an explicit Unicode helper include. The first
combined native run found generated-log-test stack overflow and pandas list
case-label formatting; the next isolated the log overlap/equality mismatch.
Small non-inlined snapshot/case functions repaired the stack footprint without
changing expected data. A fresh disjoint six-call public bank established the
original endpoint-equality rule. All failures and successful later runs remain
retained; no public calls were replayed. The Polars diagnostic's significant
trailing spaces now live inside quoted source lines: an offline audit and
native diagnostic tests establish identical 4,838 runtime bytes. All three
large generated test targets use `/bigobj` in root CMake as in the validated
focused project. Rendered browser interaction remains unverified.

## Earlier repetition checkpoint validation

- **Nine assertion-enabled native suites pass**, including all three repetition
  companions, the ordinary/literal continuations, old log workbench/route and
  both new table suites:
  `build/parity-closures/native-current/ctest-ordinary-repetition-final.log`.
- Final native/server build:
  `build/parity-closures/native-current/build-ordinary-repetition-final.log`.
- Frontend **181 files / 9,702 tests**, TypeScript and Vite pass:
  `build/parity-closures/table-repetition-frontend-{full,tsc,vite}.log`.
  One unrelated existing fixture exceeded its default five-second timeout
  during concurrent compilation; that failed run is retained. The final full
  run used `--testTimeout 30000`, with no test or production edit for the timeout.
- **34 final production requests pass** on independently owned servers:
  `build/parity-closures/table-repetition-production-http-2` (17) and
  `build/parity-closures/repetition-production-http-3` (17). Their receipts bind
  server SHA256 `1ba9fe6080c3f1cee06855e669b122fc26250485e9a2ca328b31c1b5d6c9ac7e`.
  Both servers exited. Earlier owned HTTP attempts remain retained.
- Offline structural catalogue validation passes:
  `build/parity-closures/catalog-repetition-final.log`. Counts remain 46/40
  packages, 226/161 modules and 201 open strict units. No repetition row was
  promoted from a finite bounded bank.

The first Polars test executable overflowed the Windows stack. Splitting
generated frame/case functions repaired the test harness without raising the
stack limit or changing expected data. The next run detected an omitted
`failed` marker in a temporal diagnostic; the general formatter was corrected.
Both failures and final success are retained in the Polars and workflow
archives. No reference calls in this continuation were replayed. Rendered
browser interaction remains unverified.

## Earlier OCEL checkpoint validation

- **Nine assertion-enabled native suites pass**, including original core,
  filters, events, objects, NaT, exact controls, matching and both timing suites:
  `build/parity-closures/native-current/ctest-matching-typedtime-final2.log`.
- Server and CLI freshly build:
  `build/parity-closures/native-current/build-matching-route-final2.log`.
- Frontend **179 files / 9,677 tests**, focused 29, TypeScript and Vite pass.
  Logs: `build/parity-closures/frontend-matching-exact-*`; final Vite log:
  `frontend-matching-exact-vite-final2.log`; output: `frontend-matching-exact-dist`.
- **Ten actual production-handler requests pass** on an independently owned
  temporary server, including matching/open output, NUL transport and NaT/
  scalar timing: `build/parity-closures/owned-final-ocel-http-2/validation.json`
  and `records.jsonl`. That server exited; the existing app was not replaced.
- Offline declaration/artifact/proof checks pass for 86 packages / 387 modules.
  This is not a fresh reference implementation/source audit.

Earlier failed builds, expectations and HTTP harness attempts remain retained.
The matching HTTP test now percent-encodes bytes itself because the vendored
test client's `httplib::detail::encode_url` truncates NUL. Production
URLSearchParams and server decoding passed actual NUL transport acceptance.
Rendered application interaction remains unverified.

## Next open requirements

- Keep all LTL declared gaps open. Reuse the 16 executed banks above; never replay
  them. Arbitrary Python objects/identity graphs, unobserved dtypes and selector
  combinations, physical lazy plans/category context, warnings and group-order
  diagnostics remain separate gates. The offline catalogue-wide receipt-index
  plan at `build/parity-closures/retained-public-receipt-index-plan.json` is
  archived with Polars checks. Its reusable indexing tooling now has bounded LTL
  acceptance as recorded above; catalogue-wide and next-family historical audits
  remain open.

- Continue the between modules' declared boundaries. The 20-case log-selector
  bank is now **executed and accepted**, with the equality bank explicitly
  included in its prior-receipt audit; do not replay it. Its retained output is
  `build/filtering-between-log-selectors-public-actual-1`. The pandas 16-case
  ordinary bank and two-case float32 sweep are also executed and accepted;
  retained outputs are `filtering-between-pandas-ordinary-public-actual-1`
  and `filtering-between-pandas-float32-public-actual-1` under `build/`.
  The ten categorical calls are at
  `build/filtering-between-polars-categorical-public-actual-1`, with 38 complete
  dictionary snapshots and separate observer warnings. Native acceptance
  covers label dictionaries, not physical codes/context identity. Keep log
  typed-source/nested nonstring-key carriers, unobserved selector forms, pandas
  extension dtypes/temporal resolutions/identity graphs, Polars warning
  emission, unobserved dtype/cast/diagnostic domains and physical plans open.
- Continue the wider OCDFG gate. Reuse the retained 77-call bank at
  `build/parity-closures/ocdfg-performance-public-actual-1/capture.json`;
  all retained timing/discovery/aggregation/strict-selector outcomes now have
  acceptance. Design only new disjoint inputs for remaining domains. This
  seventy-seven-outcome slice does not establish whole-module closure.
- Keep arbitrary relation dataframe cells, dtype/storage domains, non-string
  grouping IDs and actual performance benchmarking explicit. The new timing
  companion supports string identities; do not silently coerce other domains.
- Continue the three repetition modules' declared gaps. Ordinary log values,
  byte targets, container/date/byte bounds, unhashable selectors and literal
  pandas missing-key diagnostics now have disjoint acceptance. Custom equality
  objects, wrapper storage/identity graphs, arbitrary table schemas/dtypes,
  categorical dictionaries and Polars physical plans remain open as documented.
- Resolve Alpha+ identity-dependent cleanup selection. Both returned graph
  shapes occur in broader captures; the old single-process 28/64 mismatch
  count does not establish 28 incorrect models.
- All other partial rows, connectors/platforms, solvers, rendered UI and
  performance requirements remain in scope until their own evidence closes.

## Build coordination and safe staging

Python: `C:/Users/pmoch/AppData/Local/Programs/Python/Python311/python.exe`
with `-B -X utf8`; Node: `C:/Program Files/nodejs/node.exe`;
CMake/CTest: `C:/Program Files/CMake/bin/`.

Focused native project: `build/parity-closures/native-current/`, build `out/`.
It recompiles current overrides and affected server/CLI callers, linking the
other validated feature-99 core objects. Add sources/targets here and in root
CMakeLists. Tests require `/UNDEBUG`; coordinate shared MSBuild/project edits.
A timestamp anomaly once left controls code stale: verify source/object
bindings and force a fresh owned-source compile when evidence contradicts code.

No owned functional drafts remain. Preserve inherited dirty comparison metadata
in `docs/reference-2.7.22-coverage.json` and the historical journal change in
`docs/reference-2.7.22-parity.md`; do not stage those whole files. Preserve
untracked `node_modules/` and Python caches. Stage owned coverage rows with
`build/parity-closures/stage-rows.py ROW_JSON`. Structural check:
`node build/parity-closures/verify-declared-catalog.mjs . --project-root . --manifest build/parity-closures/coverage-for-index.json`.
Bind real tracked sources/tests, direct-main proof cases and every named
implementation symbol. Inspect staged scope and run `git diff --cached --check`
before each clean logical commit/push.

The existing application was not replaced. The prior handoff records a policy
rejection of server replacement and a browser permission gate. Do not retry
those rejected actions through another shell or disguise. Owned temporary HTTP
acceptance is separate evidence. Recheck listeners/PIDs before relying on
historical running-app identities.
