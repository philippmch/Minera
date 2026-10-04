# Token-replay completion contract

Target: PM4Py 2.7.22, commit
`0715c31f3f1d85924107d6aa5329806db69df899`.
The dispatcher, forward and backwards variants remain partial. This document
records acceptance criteria for the unresolved forward choice/cleanup evidence;
it does not replace the other replay, adapter or diagnostic requirements in
`docs/reference-2.7.22-parity.md`.

## Incidental choice order

The project requires equivalent process-mining behavior and diagnostic meaning,
while permitting documented differences among valid outputs. Token replay's
incidental set iteration is not an advertised random sampler. Matching its
outcome probabilities is therefore not a completion requirement. This does not
permit arbitrary changes to counters, markings, trace fitness or diagnostics.

There are two different comparison cases:

- A validated net automorphism or independent silent-transition permutation can
  rename/reorder equivalent elements while retaining every required output field.
  Existing checkers validate those transformations and report them separately.
- Choosing among enabled duplicate-label transitions can change the subsequent
  replay, including cleanup, token counters and fitness. These are distinct
  replay paths, not naming differences. Each native output must be justified as
  one complete legal path with the corresponding exact diagnostic values.

The repeated-event flood corpus in
`docs/token-replay-flood-multievent-validation.json` retains 17 initial strict
differences. Its 484 resampled calls contain 221 exact matches and 263 strict
differences. Native outputs occurring in sampled public support are useful
observations, but do not establish the complete set of legal outcomes.

Inspection of the 17 retained differing fixtures found exactly one visible
transition (label `A`) in each net, alongside silent transitions. Duplicate
visible-label selection therefore cannot explain those particular differences.
Multiple silent routing alternatives remain; their ordering and cleanup effects
must be isolated before attributing the differing diagnostics to a rule.

## Required next evidence

For minimized acyclic fixtures, isolate visible or silent choices from cleanup
by varying cleaning and place/transition fitness reporting independently.
Enumerate feasible transition orders that remain consistent across repeated
events in a single net; independent per-event choices are not an adequate model.
An independently written oracle must derive each complete path's ordered
firings, reached markings, counters, diagnostic records and fitness.

Compare native output with a whole oracle result, not a mixture of fields from
different paths. Require exact output for deterministic, single-choice fixtures.
Retain public calls that witness competing results and verify their membership
in the same legal set. More samples alone cannot establish completeness. A
native result that no feasible order explains remains a semantic defect.

The oracle's scope and assumptions must be reviewed against the public
observations. This bounded evidence must then be reconciled with the broader
forward controls, backwards behavior, supported input adapters, root-cause
diagnostics and distinct registered catalogue proofs before any completion claim.

## Explicit backwards reference checkpoint

`docs/token-backwards-controls-reference.json.gz` records 13 fixtures through
both the explicit `BACKWARDS` dispatcher selector and the public backwards
module: 26 calls, 24 returned results and two `ZeroDivisionError` outcomes for
the unknown-only fixture. Both routes agree for all 13 pairs. All actual
log/net/marking/parameter snapshots are unchanged after the calls.

The cases cover a silent prefix, unknown and repeated activities, stopping,
weighted silent arcs, duplicate labels, final-marking multiplicity, empty
traces/logs and a supplied hidden-walk-false parameter. That last fixture records
an observed result; it does not prove general parameter behavior. The separately
documented native hidden-walk extension remains distinct from this comparison.

Independent review verified the fixture inputs, complete results/errors and
opaque runtime-module equality with pinned Git blobs. Capture source and review
are retained in `docs/token-backwards-controls-capture-evidence.json.gz`; the
summary is `docs/token-backwards-controls-reference-validation.json`. This is
reference-only evidence. Native comparison and broader backwards completion
requirements remain open; no catalogue row is promoted.

The native baseline is retained in `docs/token-backwards-controls-baseline.json.gz`
and summarized by `docs/token-backwards-controls-baseline-validation.json`.
Thirteen CLI invocations compare all 26 public route observations: 24 match;
both unknown-only observations differ. The reference raises `ZeroDivisionError`
while native replay succeeds with fitness 0.5, produced 1, consumed 0, missing 1
and remaining 1. Every public result field is compared, with native-only
diagnostics retained separately. The hidden-walk-false case uses an explicitly
documented effective-control mapping limited to its paired public fixture.
Independent review verified the input translation and frozen source/binary/raw
bindings. This is a confirmed baseline discrepancy, not a completion claim.

## Backwards undefined-fitness correction

`docs/token-backwards-zero-reference.json.gz` expands the denominator controls
to 13 fixtures through four explicit routes: forward and backwards dispatcher
selectors and their public variant modules. All 52 calls preserve their inputs.
The 26 forward calls return results; backwards returns 12 results and raises
`ZeroDivisionError` in 14 calls across seven fixtures. The errors themselves do
not expose token counters or establish a universal internal formula.

Backwards native fitness now throws `std::domain_error` when either consumed or
produced tokens are zero. Full trace results and aggregate conformance retain
this error; forward fitness and empty-log behavior remain covered separately.
The CLI comparison maps only the exact diagnostic together with a nonzero exit
to the public exception. It does not claim language-level exception identity.

Count-only replay remains available. The native
`replay_token_activation_sequences` helper uses the same validation and replay
path as full trace diagnostics, returning one ordered transition sequence per
source trace without calculating fitness. Stochastic-map fitting consumes these
sequences, preserving activation counts even when backwards fitness is undefined.
This helper is a native API contract, not an additional reference parity claim.

The corrected build matches all 52 denominator observations and all 26 original
backwards controls, using 26 and 13 native invocations respectively. These are
two overlapping cohorts, not 78 unique input fixtures. The frozen denominator
baseline retains its 38 matches and 14 mismatches; the original baseline retains
24 matches and two mismatches. All result fields and the strictly mapped errors
are retained in `docs/token-backwards-zero-native-checks.json.gz`, with the
summary in `docs/token-backwards-zero-native-validation.json`.

Eleven affected Release suites pass, including replay diagnostics, input
adapters, stochastic maps, Monte Carlo and downstream evaluation. Source,
binary, test and comparison bindings accompany the evidence. The dispatcher,
forward and backwards rows remain partial; broader behavior and distinct
catalogue proof requirements remain open.

## Weighted backwards routing checkpoint

`docs/token-backwards-weighted-routing-checks.json.gz` retains eight fixtures
through the explicit backwards dispatcher and public variant module: sixteen
calls, fourteen results and two exceptions, with unchanged input snapshots.
Both routes agree for every fixture. Eight invocations of the already tested
native build match all sixteen observations; no production change was needed.
The summary is `docs/token-backwards-weighted-routing-validation.json`.

In these deterministic nets, exact silent-output/visible-input weights of 1:1
and 2:2 route through the silent transition and fit. Output oversupply at 2:1,
3:1 and 3:2 instead leaves the silent transition unfired and weak-fires the
visible transition with missing tokens. A two-output silent gate with matching
visible requirements routes successfully. These observations support the
existing weighted-bundle subtraction rule; they do not establish arbitrary
weighted-net routing behavior.

Two repeated-activity fixtures distinguish different paths. With one visible
`A` transition, `A,A` returns two visible firings and unfit diagnostics. With
two initially blocked same-label transitions, the captured public calls raise
`ZeroDivisionError`; native replay emits the exact undefined-fitness diagnostic
and exits nonzero. The comparison records that explicit exception mapping.
The initial comparison without the mapping is preserved separately from the
passing run. This checkpoint adds finite routing evidence and does not promote
the dispatcher or either replay variation.

## Backwards module completion gate

The backwards variation can be proved separately from the still-open forward
choice/cleanup behavior. Its declared direct-call input is a grouped EventLog
with a Petri net, initial/final markings and a parameter dictionary. Native
stream/table grouping is an extension, as documented in the main parity contract;
it is not evidence for the direct backwards input domain.

The existing controls, denominator and weighted-routing records cover returned
fields, unknown/repeated activities, stopping, empty traces/logs, final marking
multiplicity, silent routing and the observed undefined-fitness errors. The next
required evidence is the public parameter and input boundary, not another
undirected routing sample:

- Pin custom activity selection through enum and string parameter keys, missing
  selected fields, explicit nulls and non-string activity values inside EventLog.
- Use duplicate traces and case IDs to test case selection, supplied variants,
  variant delimiters and disabled variant caching. Check order and cardinality.
- Contrast return-name and place/transition-fitness settings using complete
  returned values, including their types and diagnostic order.
- Exercise final silent routing, residual tokens and repeated-event token
  accumulation when testing final-routing, remaining-fitness and cleaning flags.
  Check hidden-path caches, timeout and reduction controls separately; classify
  observed no-effects without inferring universal behavior from benign inputs.
- Translate the captured inputs faithfully to native calls, compare every
  result/error under explicit representation mappings, and resolve discrepancies.
- Add a distinct registered backwards acceptance proof tied to the native entry
  point and reviewed behavioral evidence, then pass its affected native checks
  and the structural catalogue verifier before promoting the module row.

The parameter inventory comes from the pinned public enum and signature already
retained in `docs/token-backwards-controls-reference.json.gz`. Default forward
captures do not satisfy these backwards requirements. Arbitrary wrong Python
carrier or Marking object types are not inferred to be promised contracts from
type annotations; any error claim must identify the supported input constraint
and its actual public observation.

## Backwards parameter reference checkpoint

`docs/token-backwards-parameters-reference.json.gz` preserves 31 grouped EventLog
fixtures through both explicit backwards public routes: 62 calls, 60 returned
results and two missing-activity-key errors. All inputs remain unchanged and
each route pair agrees. The fixtures exercise all 15 public parameter members
and 17 paired contrasts, including typed null/integer activities, enum/string
activity selectors, duplicate traces and case IDs, hidden routing, stopping,
fitness controls, supplied variants, caches and timeout. Observed no-effects
are fixture-local evidence, not universal ignored-parameter claims.

`docs/token-backwards-parameters-capture-evidence.json.gz` retains the reviewed
capture sources and typed-input validator; the summary is
`docs/token-backwards-parameters-reference-validation.json`. This is reference
evidence only. Native parameter comparison and correction of discrepancies
remain separate completion gates; no coverage row is promoted here.

The reviewed pre-fix native baseline is retained in
`docs/token-backwards-parameters-baseline-checks.json.gz`, with its summary in
`docs/token-backwards-parameters-baseline-validation.json`. Thirty-one native
invocations match 58 of the 62 public observations. The four discrepancies are
the enum/string activity-key fixtures on both routes: native replay incorrectly
matches integer activity `1` to model label string `"1"`. Public replay activates
only `a` with fitness 0.25; native additionally activates `one` with fitness 1.
The archive preserves typed XES inputs, complete results, comparator sources
and the original tested-build identity before any correction.

The correction and verification are retained in
`docs/token-backwards-parameters-after-checks.json.gz`, summarized by
`docs/token-backwards-parameters-after-validation.json`. The corrected build
matches all 62 public observations through 31 native invocations and passes all
11 affected native suites. Replay resolves only scalar string activities to
model labels; numeric activities remain unknown and non-null. Trace results and
aggregate variants use the same typed selection. Equal scalar strings retain
their shared identity when ancillary child metadata differs, and standard
activity selection honors retained first-class type metadata.

The archive also preserves the first failed test run and its correction: an
overly restrictive scalar-string guard rejected child metadata, which the
existing event-data suite caught. The new `backwards_module_acceptance` case
checks ordered trace results and aggregate diagnostics alongside targeted typed
activity regressions. Independent review cleared the corrected change. Catalogue
registration and module promotion remain a separate gate.

## Backwards module acceptance

The separate catalogue gate is now linked to `replay_token_traces` and the
registered `backwards_module_acceptance` case in
`minera_token_replay_diagnostics_tests`. Independent completion review checked
all six gates above against the preserved controls (26 matched observations),
denominator boundaries (52), weighted routing (16), and post-fix parameter/input
matrix (62). These cohorts overlap and are not a count of unique inputs.

The module `algo.conformance.tokenreplay.variants.backwards` is complete under
the declared direct EventLog contract. Its proof combines the behavioral evidence
with the native acceptance case and passing affected suites; fixture-local
parameter no-effects are not generalized into universal ignored-parameter
claims. The token-replay dispatcher, forward variation and root-cause module
remain partial. Stream/table grouping remains a native extension. The overall
catalogue now has 198 complete and 189 partial modules, with 46 complete and 40
partial dispatcher packages: 229 coverage units remain open.

## Exact signed ordinary-weight forward replay

Forward token replay now uses exact signed token integers for reached markings,
trace/place diagnostics and aggregate token counters. Compact ordinary arcs
contribute their measured input/output weights. Repair records the input deficit
and adds the full input weight, preserving partially available tokens. Only actual
input places are repaired. A signed input marking can remain insufficient after
repair: that attempted transition contributes consumed/produced counts but does
not activate or move tokens until enabled. Zero output keys are retained. Final
missing checks apply to final-marking keys; signed remaining counts are retained.
The observed fitness result is 1 when consumed or produced totals are nonpositive.

TokenInteger owns signed decimal-limb arithmetic, canonical zero, add/subtract/
multiply, hashing/comparison, checked bounded conversions and decimal JSON output.
Independent Python arithmetic checks cover 545 boundary/random signed cases and
match all 3270 fields, including float conversion. The first conversion attempt
had 90 rounding mismatches; its original source, executable and actual results
are retained. The corrected header matches the accepted frozen source exactly.
Exact initial/final replay options take precedence over legacy integer vectors;
the central PNML adapter supplies them and standalone CLI replay uses that adapter.

The retained 58-model public capture has 1044 actual forward returns. Seven new
finite genuinely silent models add seven actual imports and 126 actual direct/
dispatcher returns. Their hidden output weights include negative uint64 magnitude,
negative small weights, zero, unit, larger and uint64-limit weights. Each hidden
transition consumes one source token, bounding silent exploration. All public net,
marking, log and parameter snapshots remain unchanged. The reused public worker
incorrectly declares 58 imports in this seven-model capture: original raw bytes
are preserved and an audited copy changes only that count. Seven actual import
markers and 126 actual call markers prove the corrected counts. Returned objects
and scalar values are unchanged. Opaque before/after module pin proofs match
PM4Py 2.7.22 commit 0715c31f3f1d85924107d6aa5329806db69df899 with recorded
CRLF-to-LF normalization only; implementation and bytecode are not inspected.

A derived binding combines these two actual captures, preserving returned objects
and remapping only case/call indices. It is not a new public producer. The expanded
before probe matches 736 of 1098 supported public trace outcomes. The accepted
native probe makes 65 actual imports, 61 successful imports and 549 actual replay
invocations across nine profiles. Each invocation maps to both independently
captured public routes. All 1098 supported outcomes match all ten trace fields
(10980 fields), including ordered transition/problem lists, enabled sets and exact
reached-marking keys/counts. All 1544 measured PLTR place m/r/c/p fields also match.
Four public weights exceed the native uint64 magnitude carrier, leaving 72 captured
public outcomes behind explicit import gates. No success is inferred for them.

CLI checks make 553 actual process invocations: all 549 supported model/profile
runs match complete native replay JSON and four out-of-range imports reject.
This preserves the native trace carrier; public PLTR tuple/set/dictionary and
RETURN_NAMES formatting parity is not inferred. The 43 label controls still match
all 624 public trace outcomes/6240 fields. The prior 352-model regression preserves
all returned cases, 4224 metric outcomes, 1056 parameter parses and 352 exports.

The frozen build contains 657 source inputs: nine existing changes, two additions
and 646 unchanged parent inputs. Full compilation produces 154 fresh binaries.
Its first CTest run passes 149 of 151 suites: Unicode test fixtures need the MSVC
UTF-8 option and a forward test still expects obsolete int32 overflow. Both actual
failures are retained. The corrected test asserts an exact postmarking and counters
beyond int32; bounded reachability, footprints and alignment guards remain tested.
The final snapshot freshly rebuilds both corrected suites and reuses 152 binaries
whose source is unchanged. Their full generated compile/link projects match after
only path and generated project/reference GUID normalization; actual binary hashes
match the original full compilation. All 151 assertion-enabled suites now pass.
No 154-fresh-output claim is made for this final test-only correction build.

Four unsuccessful build snapshots, actual compiler/CTest logs, all linked artifacts,
partial compiler inventories, the early failure and isolated successful diagnostics,
before/candidate mismatches, zero-call preparation/audit failures and immutable
helper correction chains are retained. Partial compiler objects remain at their
recorded local paths; only linked artifacts are archived. Binary duplicates use
exact aliases. The deterministic gzip archives round-trip every chunk and entire
file; the validation JSON lists every file, alias, process and producer binding.
The pushed parent validation/archives bind the original public oracle and baseline.

Full forward method/module parity remains open: PLTR tuple case sets, transition/
unknown dictionaries, RETURN_NAMES output formatting, other parameters, broader
silent topologies, arbitrary imported markings and large floating-point ratios need
further evidence. PNML weight magnitude is still uint64 and PNML markings are
signed int64. Dataframe diagnostic integer cells retain explicit int64 range checks;
large JSON numbers do not establish exact frontend presentation. Weighted aggregate
fitness, precision, alignment, composite conformance and specialized transformations
remain unverified, so the existing application computed=false/error gate remains.
This forward matrix does not establish weighted backwards or broader extended-arc
parity. The catalogue is unchanged: 46/86 packages, 199/387 modules and 228 incomplete
units. No module/package promotion or full parity is claimed.

Evidence: [weighted-token-replay-validation.json](weighted-token-replay-validation.json).

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `weighted-token-replay-evidence-01.jsonl.gz` | `7045fd754266ca99dc6c56c7c2c8830c06d0555fa1de9fe453646d5c3a0de149` | 8939263 |
| `weighted-token-replay-evidence-02.jsonl.gz` | `3c5bf7a7e7ecc97a402103c1c41c1548fce96af70a6ff1d1a12518de6aeb833f` | 5885080 |
| `weighted-token-replay-evidence-03.jsonl.gz` | `59bb524925a3db5ca91b801197f62f900dbe2645ef4e89f9f04540fb4c1c6840` | 3816674 |
| `weighted-token-replay-evidence-04.jsonl.gz` | `36637fd6cf12c88f7e3d55c2611146fa23ed82a18f8f0a1b8b97976ebf5ab2cc` | 4971711 |
| `weighted-token-replay-evidence-05.jsonl.gz` | `3de95c596f3648d99cc9f256d24e6705a6bd81f1a75aa97355f45eaaa14e2616` | 7596070 |
| `weighted-token-replay-evidence-06.jsonl.gz` | `de0f2a93a24d5be2458899d50fa880ec1429c93e872deb804d57170c2c95d4d5` | 7117240 |
| `weighted-token-replay-evidence-07.jsonl.gz` | `fd7d440c18b00580a64ea8ff06cc44f5831b1b86f55076c0ad09b887b06fb30e` | 6952163 |
| `weighted-token-replay-evidence-08.jsonl.gz` | `b10ca7fb6a12fab98bc546e49941a72d85f3e9346254223e7f9e86b033f8528f` | 7521413 |
| `weighted-token-replay-evidence-09.jsonl.gz` | `6eaeefc411cc2467669bc3b2002537cf7b265168c31fd5fff67afa119f848397` | 7089170 |
| `weighted-token-replay-evidence-10.jsonl.gz` | `09449d18de3b9640822282e3f47a52f6417bcea0d2feaa82cdd0c85d7e729c93` | 7651108 |
| `weighted-token-replay-evidence-11.jsonl.gz` | `422f5cbc7e1caaabe026a4491d71620531339e6cabca9a56d16ca8674a3834cd` | 17043565 |
| `weighted-token-replay-evidence-12.jsonl.gz` | `03e379affa137915b673ac9c6ba834f35ca0254a70164130a9db08ffea905e0d` | 8022822 |
| `weighted-token-replay-evidence-13.jsonl.gz` | `3eb08e765ad8f4a1780f4c776a1927fe7f36fa244125a6ec0e9e3dde88c1d805` | 7363149 |
| `weighted-token-replay-evidence-14.jsonl.gz` | `def530402023a11f151a61415da830bc974cee05599e95e9471d4c5dfc9cf316` | 7238610 |
| `weighted-token-replay-evidence-15.jsonl.gz` | `08adfa35347ccedcf091b93a4ea3fb1daaf60047706d12f46a880eefd5531aeb` | 6304991 |
| `weighted-token-replay-evidence-16.jsonl.gz` | `88b993e2939e7457fada299085f5f724267e6448b11b2e4a027b47e31b4593e9` | 7857780 |
| `weighted-token-replay-evidence-17.jsonl.gz` | `a9c6dd88595f0411be0f28130fc945f925faec816f122a816a986f4777d11051` | 7167983 |
| `weighted-token-replay-evidence-18.jsonl.gz` | `bd59ab66a788935b1989fcc20b2ee2d5043f40e16426953c72a8c2275dd03ead` | 7968558 |
| `weighted-token-replay-evidence-19.jsonl.gz` | `722d14bf564090d08ceb4bdebffd0b9569fdf435f7506016a9888931ca5ba173` | 13309668 |
| `weighted-token-replay-evidence-20.jsonl.gz` | `f759000af2d6c39799c39d7e9c5906e1755a709552a43fda7c95d33c2f12b5ca` | 22982111 |
| `weighted-token-replay-evidence-21.jsonl.gz` | `60fe66fbf406e1726f283a563b054c98e22c48cc442f4dc5807179fc0bed6b74` | 15820689 |

## Exact ordinary arc magnitudes beyond uint64

PNML integer inscriptions, executable arc metadata, document occurrences and
presentation/export carriers now retain exact nonnegative integer magnitudes
with a separate sign. Large, signed and zero weights keep compact endpoints;
ordinary replay uses the exact token arithmetic introduced in the pushed parent.
Bounded simulation and CYC token carriers retain their checked range limits.

Thirty new actual public models cover ordinary input, ordinary output and finite
silent output weights of both signs at 2^64, 2^64+1, 2^128+1, 2^256+1 and 10^400.
Nine profiles are invoked through both public routes: empty, one, two and three
events, unknown activity, STOP, CLEANING, PLTR and RETURN_NAMES. These make
30 actual imports and 540 actual replay calls: 494 returns and 46 float-overflow
errors. Actual public inputs, markings, logs and parameters remain unchanged.
All seven opaque module pin proofs match before and after the calls, with only
recorded CRLF-to-LF normalization. Upstream implementation and bytecode are not
inspected. The reused outer runner's stale 58-import summary is preserved and
explicitly audited against the worker's actual import/call markers.

The derived binding preserves the original 65 public models and all new actual
returns/errors, remapping only model/call indices. It is not a new API producer.
Before this change, 34 of the combined 95 native imports reject, leaving 612
public outcomes behind import gates. After the change, all 95 imports succeed.
The 855 actual native replay invocations return 832 results and 23 float-overflow
errors. Both independent public routes match all 1664 returned trace outcomes
across ten fields (16640 fields), all 46 measured float-overflow outcomes and
2288 measured PLTR place m/r/c/p fields. Positive 10^400 cases that reach fitness
float conversion fail as measured; negative cases retain their exact integers.
Python/C++ exception class and message identity is not claimed.

All 95 exact native document exports are independently reimported through the
public importer. Place/transition names and labels, ordinary endpoint/weight
multisets and exact initial/final marking keys/counts match. All 1710 complete
packed public replay returns/errors match after only dictionary/Marking/set order
canonicalization; ordered lists retain their order. This is public reimport
evidence, not a native full PLTR tuple/RETURN_NAMES carrier proof or an exporter
byte/format/property equivalence claim. The 95 reimports and 1710 replay calls
are actual fresh public calls, bringing this checkpoint to 125 fresh public
imports and 2250 replay calls (2158 returns and 92 recorded float errors).

The fresh complete build freezes 657 inputs: eight existing edits and 649
unchanged parent inputs. It produces all 154 core/application/test binaries
afresh with zero reuse. All 151 assertion-enabled suites pass. The owned exact
integer header also matches 545 independent arithmetic controls across 3270
fields. All 855 actual CLI runs match 832 complete native trace JSON results and
23 checked float-overflow errors. The label regression preserves all 624 public
trace outcomes/6240 fields; all 352 prior metric cases, 4224 metric outcomes,
1056 parameter parses and 352 document exports remain unchanged.

Deterministic sub-50-MiB gzip archives retain all 160 linked binaries, all frozen
inputs, exact exports, public returns/errors, raw logs, actual process witnesses
and immutable owned helpers. Every chunk and whole file passes exact round-trip
verification. The pushed parent's validation/archive hashes bind the reused
65-model oracle, baseline build and prior unsuccessful attempts. This checkpoint
contains one successful complete build and does not claim a new failed build.

Full native PLTR tuple case sets, transition/unknown dictionaries, RETURN_NAMES
formatting, broader lexical/digit-limit inputs, other parameters, imported
arbitrary markings and general silent topologies remain open. PNML markings and
specialized state carriers remain bounded; dataframe integer cells retain int64
range checks. Large JSON numbers do not prove exact frontend rendering or input
transport. Weighted aggregate/composite conformance, precision, alignment,
specialized transformations and weighted backwards behavior remain unverified;
the application computed=false/error gate remains. The catalogue is unchanged
at 46/86 packages, 199/387 modules and 228 incomplete units. No module/package
promotion or full parity is claimed.

Evidence: [unbounded-ordinary-weight-validation.json](unbounded-ordinary-weight-validation.json).

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `unbounded-ordinary-weight-evidence-01.jsonl.gz` | `4de554ad255b70dcdaf976c4704a3f6b30ae67def2ab0322e3ca717ce1573b3f` | 8902808 |
| `unbounded-ordinary-weight-evidence-02.jsonl.gz` | `aefade6ad16f61f6befb24e8027e082fcdd259c1ef9a30e77cf0698e0332e526` | 7380442 |
| `unbounded-ordinary-weight-evidence-03.jsonl.gz` | `812a4365f9cf57e561a71f261f02bab70629b6a98bcf36626e60b2de0107d61b` | 7336017 |
| `unbounded-ordinary-weight-evidence-04.jsonl.gz` | `84a4043aa3f8512312236867d4c9db523c1b4ba74d4f0cdaeb61233ad618ac22` | 6233267 |
| `unbounded-ordinary-weight-evidence-05.jsonl.gz` | `da53a666d5d10d4bb2a3ad8b1e9074932e769d480d0faf458cf791506dc904ca` | 7915217 |
| `unbounded-ordinary-weight-evidence-06.jsonl.gz` | `47d0a30648fb85461e8802a1af146440569242aa51d1c5feba13897b8e0889f4` | 7222875 |
| `unbounded-ordinary-weight-evidence-07.jsonl.gz` | `c0126c80f9de6c5b7256ae7ca051a0f4866c6108235394915e0625797e2fb281` | 8008834 |
| `unbounded-ordinary-weight-evidence-08.jsonl.gz` | `318335237e1adb9b1fbab498eec5080120b228a916823ce96a0ce536c0734923` | 12607393 |
| `unbounded-ordinary-weight-evidence-09.jsonl.gz` | `6376f75e5065c4a1459e00c4c3db660f1e5746685ab2bb628abcb5b38c26687d` | 23147548 |
| `unbounded-ordinary-weight-evidence-10.jsonl.gz` | `80156e4068449af4b59ea8017bb98aba9fb9091ed0175a1034920ecba22e46cf` | 18515365 |
| `unbounded-ordinary-weight-evidence-11.jsonl.gz` | `b89dce959cd0c30257c0cf1ca9e96c18f0c107766ad7b401a37cfa7ecafe6900` | 4172382 |
| `unbounded-ordinary-weight-evidence-12.jsonl.gz` | `824805794f0ff8dbbd8e74907e89592bfe37006a1e7b6981a42fe46013cb0e1a` | 3531088 |
| `unbounded-ordinary-weight-evidence-13.jsonl.gz` | `4a0a2df59c390272842290c881c29e98a1dd6d7acbe723a2ab0193645457f8e6` | 3498824 |
| `unbounded-ordinary-weight-evidence-14.jsonl.gz` | `f61fc87469bca7ccf6d5854770df3dd3681b15e596acfa6f32e0e81a5782ea92` | 1854065 |

## Full forward diagnostic carrier controls

This checkpoint preserves actual public evidence for the complete forward result
carrier and an independently derived carrier prototype. It does not add a native
full-result API. The pushed exact-weight implementation remains unchanged.

The same 95 actual PNML carriers are exercised through both public routes across
twelve profiles: PLTR with stop/clean/names combinations, explicit variants enabled
and disabled, unknown-activity fitness, and names without PLTR. Nine source traces
cover empty cases, repeated activities/unknowns, different event contexts, repeated
variants and equal traces with equal or different case attributes. Public Trace
value equality is measured explicitly. Source Trace indices and generated Trace
keys retain their actual full attributes/events; generated keys use a null source
index. No source identity is invented for them.

The first ten-trace capture adds a null activity to the string-activity log. Its
95 actual imports and 2280 actual API calls all complete with the measured public
TypeError: '<' not supported between instances of 'str' and 'NoneType'. The current
native APIs instead return traces on 2216 corresponding public rejection outcomes
and encounter checked float overflow on the other 64. No mixed-null rejection
parity is inferred. The exact rejection and native outputs remain retained.

The first nine-trace capture makes another 95 imports/2280 calls and packs 2032
complete returns. It records 64 float errors and 184 KeyErrors from its packer,
which assumes every Trace key belongs to the source log. Those KeyErrors are
serialization failures, not reference API errors. The immutable corrected worker
separates API and serialization exceptions and retains generated Trace keys.
It makes 95 actual imports/2280 calls: 2216 complete returns, 64 float-overflow
errors and zero serialization failures. Across the three captures, all 285 imports
and 6840 attempted public calls are actual; reused/derived values are not counted
as fresh calls. All seven opaque module pin proofs match before and after each
worker. Net, initial/final marking, log and parameter snapshots remain unchanged.
No upstream implementation, private globals or bytecode is inspected.

The fresh owned probe binds the pushed parent's exact core and all 657 frozen
inputs, then makes 95 actual imports, 2280 trace replay calls and 2090 count-only
accounting calls. It compiles one probe afresh. The parent core, 154 production/
test binaries and 151 passing assertion-enabled suites are reused evidence;
there is no fresh full build or fresh 151-suite claim here.

All 19944 returned public trace outcomes match ten exact native trace fields
(199440 fields). Both complete public routes match for all 2280 paired controls;
all 4560 measured native/public Trace equality matrices match. All 64 measured
public float failures match the native checked-conversion behavior, without an
exception class/message identity claim. Of 25264 aggregate place m/r/c/p fields,
25194 match. All 70 differences are missing counters under DISABLE_VARIANTS=false:
the public PLTR result counts event repair missing tokens once per variant while
weighing final-marking missing tokens by frequency. The generic native count-only
API still accounts for each source trace; it is not a full public PLTR carrier.

The independently written prototype derives one whole packed result from each
native path record and the immutable owned inputs. It retains all source traces
or generates activity-only variant traces as observed. Place underfed membership
comes from event repair, excluding final-marking deficits. That membership takes
precedence over the same place's overfed set. Remaining marking keys matter:
stopped signed/zero output controls retain negative or zero-token keys in overfed
cohorts when no input repair occurs. Transition fit/underfed lists retain occurrence
counts and the final merged context of examined known events, including a blocked
event's context. Unknown-activity dictionaries use that context without unknown
events updating it. Equal Trace keys retain one representative and combine their
context occurrence lists. Named traces retain names, labels and marking dictionary
carriers; unnamed results retain their object/set/Marking carriers.

The first prototype records zero complete matches because native JSON loses the
declared double type when integral fitness values print without a decimal point.
The typed correction preserves actual values and matches 1844 complete carriers.
All remaining 372 differences are place cohorts; traces, transition contexts and
unknown maps already match. Cohort precedence/nonzero signed state corrects all
but eight stopped zero-output controls. Marking-key preservation resolves those.
The final prototype matches all 2216 complete returned carriers, including every
one of the 2032 PLTR tuples and 184 named results without PLTR. Each comparison
uses a whole result, not a combination of fields from competing paths. Only
dictionary/Marking/set order and paired enabled name/label order are canonicalized;
activation, problem and context lists remain ordered. This is a derived prototype,
not an implemented native carrier or another public producer.

Two analysis attempts fail on incorrect error-count and stale helper-hash guards;
their executed helpers and correction derivations remain retained. Their outer
PIDs/raw process logs were not recorded and are not reconstructed. All three raw
captures, generated Trace values, actual native logs/process witnesses, all four
prototype stages and every rejected complete candidate are archived. Every chunk
and entire file passes exact archive round-trip verification.

Required next implementation: add a native forward result API and transport that
retain these complete place/transition/unknown and RETURN_NAMES carriers, with the
observed variant counting and case/context rules. Preserve the separate generic
per-trace accounting API. Validate mixed null/string error behavior independently,
then reconcile broader parameters, scalar/foreign keys, variant ordering, silent
topologies, imported markings and the existing choice/cleanup acceptance contract.
Weighted aggregate/composite conformance remains gated. The catalogue remains
46/86 packages, 199/387 modules and 228 incomplete units. No module/package
promotion or full parity is claimed.

Evidence: [forward-pltr-carrier-validation.json](forward-pltr-carrier-validation.json).

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `forward-pltr-carrier-evidence-01.jsonl.gz` | `6f37905c14eb5902b8de5b8b992d23bfea7e6a37d7e1d568c761b0dabf8d9478` | 1462044 |
| `forward-pltr-carrier-evidence-02.jsonl.gz` | `3e9631b50f363c520bc22098b493f4f621383f710a59f7522366f12f6c4ffdc7` | 1760220 |
| `forward-pltr-carrier-evidence-03.jsonl.gz` | `1cac7a5ad90cf639f81522e64fc804cc672c1aa04441f45ec178efc8b95e7370` | 1733078 |
| `forward-pltr-carrier-evidence-04.jsonl.gz` | `c977b648051c87122d9b1e703c2743ac1d875624319997b72fe82c8ee34bb726` | 1772428 |
| `forward-pltr-carrier-evidence-05.jsonl.gz` | `a97ca868385973af0bc2ff1bc49f1a922ea2ef042c6e2ee3dd8121b72013573f` | 1707695 |
| `forward-pltr-carrier-evidence-06.jsonl.gz` | `2e5d23d5561c6974acf39ca26840f1f91fda29413bc63c7c52efe432fcdf15e6` | 1807601 |
| `forward-pltr-carrier-evidence-07.jsonl.gz` | `97c4f38aa2835bd801d60a5059075bb28987e2a8c8b0ddf49154f889d386b8c2` | 1840824 |
| `forward-pltr-carrier-evidence-08.jsonl.gz` | `2e3badb08b38914dcca8fc183d4b6bb3ed6ac2477b63a9561c42d8a53a06c296` | 1724110 |
| `forward-pltr-carrier-evidence-09.jsonl.gz` | `75adf2e66df07f1531c524a74d233b09db88a80b496e75f3df610f1274e76909` | 1684829 |
| `forward-pltr-carrier-evidence-10.jsonl.gz` | `91fc8c1c231110411986a95892526882fc9bad103c119c6f746119e895821c8f` | 1732357 |
| `forward-pltr-carrier-evidence-11.jsonl.gz` | `4dc7164edf829502a8f65b3c28fd0ff0c5e6df48b22df5ff4c57a63228080a54` | 1743060 |
| `forward-pltr-carrier-evidence-12.jsonl.gz` | `68f7a853687500014b054f8731cf59b8a867d09a4e04e4069817444b97829db5` | 1756843 |
| `forward-pltr-carrier-evidence-13.jsonl.gz` | `50e66eb6b4dafee292f8b7a5ef7f42c0ee508ce6afaed32fdf6ae0f7c30cbc31` | 1730447 |
| `forward-pltr-carrier-evidence-14.jsonl.gz` | `48cd1ead84289ba4769d4ea9412eabf972bf4d2fbea12d6b63c74a307c6313ed` | 1789020 |
| `forward-pltr-carrier-evidence-15.jsonl.gz` | `ad3ff8960d0cca8c667585c798b67807a0f8441665a5cf22f275f9e7a4bb789e` | 1836312 |
| `forward-pltr-carrier-evidence-16.jsonl.gz` | `5b44382c41b9fd5ebc3a43d0899066bef2ee7a435d43160a77d6f30f484378f2` | 1559909 |
| `forward-pltr-carrier-evidence-17.jsonl.gz` | `d09bc9367ef9ba1db29b2f1b6e430173675dae368cb41dd21c56464a3a451376` | 1724121 |
| `forward-pltr-carrier-evidence-18.jsonl.gz` | `1a37115ba3dcb99f6d374c442c1113332c1c6f495d7591395f3d98c2a562822a` | 1762773 |
| `forward-pltr-carrier-evidence-19.jsonl.gz` | `819d14217dffc9e42d536af27d28e1e3836bde3be8bbf8efc8e9f42b9397d998` | 1750347 |
| `forward-pltr-carrier-evidence-20.jsonl.gz` | `a089fd44856f7f11cc5af31a2b8a222818a5f3bbe4b4b43e0e99594dab67c7ed` | 1738969 |
| `forward-pltr-carrier-evidence-21.jsonl.gz` | `ae9d62a3ec41e8e1f19a1e504d1ede3e9ad089d9152b4c6b901251f74d6f211b` | 1772142 |
| `forward-pltr-carrier-evidence-22.jsonl.gz` | `3266bb6ad5e1c1204fcfd657a5c5dc50e78f3d07309c1afe3c0796410d5b2f35` | 1792902 |
| `forward-pltr-carrier-evidence-23.jsonl.gz` | `4a773acf83ff4e768bc5038a8530422c3bd00ea6b4b76d6f5b2952134219714e` | 1887569 |
| `forward-pltr-carrier-evidence-24.jsonl.gz` | `57bdb25d0243ab85575cd0e45af0e87eb0f8cc96b974cbe653589162a6d2303a` | 1618379 |
| `forward-pltr-carrier-evidence-25.jsonl.gz` | `63fcd8e4249ee2dfd5b63f31ac6ddc2da98068adf6cc301f33056bc5ce5d1d1e` | 1706697 |
| `forward-pltr-carrier-evidence-26.jsonl.gz` | `8508eac3f8b4cfaba71c29a5b9e9c7eb252104eeae85f7d75e18e611044d3eb3` | 1727451 |
| `forward-pltr-carrier-evidence-27.jsonl.gz` | `f7b582b498cb1cc3a4e62b9d2eb74a7f2cca96e57322b42b7750c561c7025173` | 1755848 |
| `forward-pltr-carrier-evidence-28.jsonl.gz` | `c225d8387ff25dcf359bbd40c5c7ef2e1aa5b8b5ebcdf2f14de69ed7c82fc2f7` | 1704339 |
| `forward-pltr-carrier-evidence-29.jsonl.gz` | `2124c4af0d8116bd63eb52116d270c0694a8f855e58d1cb257897f65a78431d9` | 1776221 |
| `forward-pltr-carrier-evidence-30.jsonl.gz` | `e5b28f8daba9e22f540d9f8e0f37c7f34c81c6fe424a256c61b5d84bf94b984c` | 1789336 |
| `forward-pltr-carrier-evidence-31.jsonl.gz` | `22b85f797e82fdb08464609a362c3bb44b8b8faecaa4f3cdbba0f2ec51474e2c` | 1611726 |
| `forward-pltr-carrier-evidence-32.jsonl.gz` | `868b4171ac14b6a870eaa490d4bc043fbd092581af6f76ba993cb6114a4442d5` | 1850686 |
| `forward-pltr-carrier-evidence-33.jsonl.gz` | `11805b85a8fd00fac11e10c3a6247d6e2766cbe0e828fabc47f31929aa6d316b` | 1728583 |
| `forward-pltr-carrier-evidence-34.jsonl.gz` | `827cf8ca84f144542c1e996ad543d39c0962bb97e5d73d19debc0000de257440` | 2059083 |
| `forward-pltr-carrier-evidence-35.jsonl.gz` | `115b0f48d38a1da7ca1574202c96573263b38574f846d59d71c07dc82eb242f5` | 2136692 |
| `forward-pltr-carrier-evidence-36.jsonl.gz` | `9042601cdb3f53c2fdf430a5629d7b0c001bd8a01872d231c45889c6ea3bba16` | 2073210 |
| `forward-pltr-carrier-evidence-37.jsonl.gz` | `0718ecd230bbe29d30e8f5f29556bbd175ae1e831b52f531d64e37f44c14cc11` | 2051359 |
| `forward-pltr-carrier-evidence-38.jsonl.gz` | `22b99b2d9e0c769d429056eaf114473babf46422ac9170bd39f04374f4c8b58b` | 2091097 |
| `forward-pltr-carrier-evidence-39.jsonl.gz` | `162c97a96799cd6b51e084770482d741550fe3eede38ee7d69ad2351dad1a333` | 2097659 |
| `forward-pltr-carrier-evidence-40.jsonl.gz` | `d2365d280e868bb764f86420da03f1dfc3f2b0fd07159227c76b1068c214d4d4` | 2206122 |
| `forward-pltr-carrier-evidence-41.jsonl.gz` | `df4cc469b8d373330c92c53de761cd120bd1877a9ed0acad34d25143721fc69d` | 2180571 |
| `forward-pltr-carrier-evidence-42.jsonl.gz` | `f4cf769beab358e20529048574d22c59f9e4b7a87d89abd8cc76eb1c154b8207` | 2154535 |
| `forward-pltr-carrier-evidence-43.jsonl.gz` | `8521dcb0d5e8c2b38a1674dac98c50a83045eaaa837d986015ce5d318af8037b` | 2274627 |
| `forward-pltr-carrier-evidence-44.jsonl.gz` | `83e7d29618a0307f2280382e652ce6de3c7af8340c2f656bdd2362fd2a4ba2e9` | 2284698 |
| `forward-pltr-carrier-evidence-45.jsonl.gz` | `9da4eca1cad8d43021a0cc8daaed5f7eef232dc98a3e9fa6b4c69b05b4525092` | 2768565 |
| `forward-pltr-carrier-evidence-46.jsonl.gz` | `4e0e3eda5099fae2c673131bdfce207d842b605e5c691aa4bbee78be264e6c44` | 2852145 |
| `forward-pltr-carrier-evidence-47.jsonl.gz` | `7b98b5a0a044b997a0db1cc1280b796268b9c4e5b8a6e329b1eaec24dab9e785` | 2854683 |
| `forward-pltr-carrier-evidence-48.jsonl.gz` | `9ef374e7d17f695fdc124b099f29723bbd339b077111bc69f1d7bf13107619e2` | 1626467 |
| `forward-pltr-carrier-evidence-49.jsonl.gz` | `ae55aec4714a148b14b6ebaf6653850cbad17135d23a2bafad56ad62fcb75788` | 2064973 |
| `forward-pltr-carrier-evidence-50.jsonl.gz` | `b21db634205676d3184c627a8524557b4b2e142f2137881b048ccc57e1050ddd` | 2114010 |
| `forward-pltr-carrier-evidence-51.jsonl.gz` | `2df2165cd426d1d6e9ef9e347897df384aa30109a7679f428ac578eb9ae1982b` | 2045308 |
| `forward-pltr-carrier-evidence-52.jsonl.gz` | `18f8275c1d095b548bfc40f6e6f4157aa13bde58f5f9c987357b0ad19cdd6a61` | 2070513 |
| `forward-pltr-carrier-evidence-53.jsonl.gz` | `e52227ad84909b0d2f407c1ee47c6439807853e6ae83f0401a0465ee7b6e8453` | 2078274 |
| `forward-pltr-carrier-evidence-54.jsonl.gz` | `bc37470882f5ea121a47814ee464f00a229741aee37c1787948bea6ca52c3cb0` | 2069546 |
| `forward-pltr-carrier-evidence-55.jsonl.gz` | `533ee5ac9ff0493aa000c21bd32c4ea6ed68b12938ab57aefb0216f88b9c1b64` | 2210839 |
| `forward-pltr-carrier-evidence-56.jsonl.gz` | `ba9a8f487d8e50ec39018dc1abca6d75b5ba3c295f0c7f423944758310caf937` | 2137070 |
| `forward-pltr-carrier-evidence-57.jsonl.gz` | `7f913df99af5cc982afaea5cd298de126fe4f7b14cd78a2cfefd04efdbd9c40c` | 2199148 |
| `forward-pltr-carrier-evidence-58.jsonl.gz` | `3e439ee64008f0dcfd6bcc7ebffea7bc7b6b7cc59d3cb004350a6c2243284895` | 1753471 |

## Native full forward result API

The production `replay_token_result` API now returns source-order trace rows and,
when `enable_pltr_fitness` is true, complete place fitness, visible-transition
fitness and unknown-activity context maps. The new API is forward-only. Its
`disable_variants` optional policy defaults to source traces for PLTR and activity
variants otherwise. Explicit false generates activity-only Trace keys without
case attributes or event payloads. Value-equal source Trace keys retain the first
representative and combine their ordered context occurrences.

Grouped PLTR counts event repair missing tokens once per variant. Final-marking
missing tokens and remaining/consumed/produced place counters retain variant
frequency. Place underfed membership comes from event repair; final deficits alone
do not create it. Reached-marking membership retains explicit zero and signed keys
in overfed cohorts, with underfed membership taking precedence for that trace and
place. Visible transitions are precreated even when never examined; silent
transitions are omitted from the transition fitness map. Known examined events
merge the final context used by earlier observations. Unknown events do not update
it. A stopped blocked known event updates that context but adds no transition
underfed observation. Repeated unknown events retain one final context per Trace
key. These rules are implemented in C++, using the same replay path and shared
owned trace equality/context helpers.

`token_replay_result_json` is an explicit tagged object codec. It retains the
list/tuple/set/Marking carrier kinds, Place/Transition keys and source/generated
Trace values, exact signed integer tokens and declared float hexadecimal values.
`return_names` projects trace transitions and reached marking to names and adds
parallel activation/enabled label lists. PLTR dictionary keys remain objects.
The generic `replay_token_traces`, `replay_token_diagnostics` and their existing
JSON route continue to account for each source trace independently.

The standalone CLI selects this transport with `--token-replay-result` alongside
`--token-replay-model`. The boolean `--token-replay-enable-pltr-fitness`,
`--token-replay-disable-variants` and `--token-replay-return-names` options also
select the full result route. Request parameter parsing accepts the corresponding
underscore names. The existing standalone default output remains available.

A fresh build freezes 660 exact source inputs: nine existing edits, three new
inputs and 648 unchanged parent inputs. It builds the core, CLI/server and all
152 native suites (155 production/test outputs), with no binary reuse. Every
Release test project explicitly undefines NDEBUG. The new full-result contract
suite passes an early gate, and the complete 152/152 regression run passes.

The fresh owned probe imports all 95 exact retained weighted models and makes
2280 actual full-result calls across twelve profiles and normal/mixed-null logs.
It also repeats 2280 per-trace calls and 2090 count-only calls. All 2280 complete
legacy native call records remain equal to the prior checkpoint, including trace
rows, accounting results and errors. Document and trace-equality records remain
unchanged. Direct raw C++ tagged carriers match all 2216 complete retained public
returns, including all 2032 PLTR tuples and 184 named results without PLTR. All
2344 retained public error behaviors match: 2280 mixed-null/string rejections and
64 float-overflow outcomes. Native exceptions are not asserted to have Python
exception classes. Mixed null/string rejection occurs before replay and floating
conversion in the new full API; the generic APIs retain their prior contract.

Another 1140 actual CLI invocations import the same nine source traces from XES,
with their original case/event attributes and no invented timestamps. The 1108
successful raw CLI results match both public routes for all 2216 complete returned
carriers. The 32 CLI overflow exits match all 64 corresponding public error
outcomes. No Python carrier synthesis is used. Comparisons canonicalize only
dictionary/Marking/set order and paired enabled name/label set projections;
activation, problem and context lists remain ordered. The public controls and
opaque module pin proofs are reused from the prior retained checkpoint: there
are zero new public calls in this implementation step.

The first CLI harness incorrectly uses `--xes` (an output flag) for its input.
All 1140 invocations exit with usage before replay; they are not successful replay
calls or carrier comparisons. The executed helper and raw process witnesses remain
immutable. A derived correction changes only positional input routing, keeps the
same input bytes and produces the accepted CLI results above. The rejected run
is archived separately.

The fresh frozen sources, all 156 production/test/probe binaries, build/test logs,
raw native/CLI outputs, process witnesses and immutable helpers are archived.
Every chunk and whole file passes exact archive round-trip verification.

Broader forward parameters, typed/foreign activity and Trace keys, nonmixed null
inputs, variant ordering and object alias graphs remain open, as do the prior
silent-choice/cleanup, backwards and adapter acceptance contracts. The tagged
codec has native date/bytes/container extensions; those extensions are not
claimed to match unmeasured public object encodings. Weighted aggregate/composite
conformance remains gated. The catalogue remains 46/86 packages, 199/387 modules
and 228 incomplete units. No module/package promotion or full parity is claimed.

Evidence: [forward-result-validation.json](forward-result-validation.json).

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `forward-result-evidence-01.jsonl.gz` | `2c106a66dd8c1937136f11badf83b823daaaf9514f28edf8f08192eab2708052` | 5458164 |
| `forward-result-evidence-02.jsonl.gz` | `d54ce93d688b069e4c302ed1ce6cd241e1a04ad351be07d65b9daceb343e98ee` | 6413869 |
| `forward-result-evidence-03.jsonl.gz` | `06bae09865fa94cc05d88e4dab3624832196a5f2f999f3038442d3d19d2797c6` | 7704756 |
| `forward-result-evidence-04.jsonl.gz` | `bae92de487f0176472f241eaeec288011fabdf2461563e8e7626dfb149ae253b` | 7206644 |
| `forward-result-evidence-05.jsonl.gz` | `494ad11ead3037e681d2fcdf73631798e58d18ca9ddc4a8f33989783559c0f99` | 6619790 |
| `forward-result-evidence-06.jsonl.gz` | `fef7dbb898d647bc991de7421007b7e7b155f68fec3011717812d35f1a550ad9` | 7713631 |
| `forward-result-evidence-07.jsonl.gz` | `6f4b87df3d6690563a84c75962ae07fa3ded7710010fb801115e29db27302fad` | 7354395 |
| `forward-result-evidence-08.jsonl.gz` | `93397867f460747bfb07cccb9989476961c50badb73cbabeb60408d090d8ff3e` | 7692421 |
| `forward-result-evidence-09.jsonl.gz` | `5435f2ae042607e69ccd93e858821640c66132aef887d07ae6244a331e0d6259` | 13571376 |
| `forward-result-evidence-10.jsonl.gz` | `298d9fcb6ca3bcbb475c1e774e4e3c460229d05910c78e4543157189e2d822d4` | 22221872 |
| `forward-result-evidence-11.jsonl.gz` | `d9d57eb2023d4cd99592f8a7d79f3f932c602ce0df5f1a8be47b9117a1bbb9dc` | 18040445 |
| `forward-result-evidence-12.jsonl.gz` | `b2ab0bc0225d251fb0b33888362fa6bcd10dd088b8db0cfddef0b3c35f9878fa` | 4423387 |
| `forward-result-evidence-13.jsonl.gz` | `66ca1cee52959147b6b0aa3668ec4baee349bfb98060fd6376f6766a02c174ea` | 7623043 |
| `forward-result-evidence-14.jsonl.gz` | `0c2b72297cfba5f7f263b44262924efe3a847d3b6eb612983190a9de631c4dbf` | 14855150 |
| `forward-result-evidence-15.jsonl.gz` | `8f53fe7f1b70b40f7862de30309d6131b56f5bdab629399d7c5779703ce4107a` | 4638946 |


## Typed forward variants and null error precedence checkpoint

The full forward result API now groups typed activity variants using measured
numeric key equality, retaining the first typed representative. Booleans, integers
and equal integral floats coalesce, including positive and negative zero. Mutable
activity keys are rejected. Variant frequencies are compared before activity
tuples; only tied frequencies need cross-type ordering. The earlier blanket
mixed-null/string guard was too broad. Unequal-frequency mixtures can return,
while incompatible tied keys retain the measured rejection behavior. Bounded
stable merging handles the measured partial ordering of nonfinite numeric keys.

An explicit null activity can select an actual silent transition in the full
API. PLTR does not precreate fitness entries for silent transitions, so an examined
null event produces the measured missing-transition-key rejection before repair,
firing or fitness conversion. This also establishes the observed error precedence
when a silent output exceeds floating-point range. The generic per-trace and
count-only APIs keep their established contracts.

Two actual public captures each import six exact retained weighted PNML models
and execute 3888 forward/dispatcher calls. Together they retain 12 imports,
7776 actual calls, 4758 complete returns and 3018 actual errors, with zero
serialization gaps. The 27 log cohorts include empty/missing/null activity,
numeric equality and integer limits, nonfinite floats, bytes, tuples, mutable
keys, mixed activity types, typed Trace/event attributes and enum/string custom
activity selectors. Thirteen policy profiles and tied/unequal variant-frequency
controls are measured. All input objects remain unchanged. Seven opaque module
pin proofs match before and after; only public signatures, enum members and
actual return values are inspected. The public signature and all 18 Parameters
members are retained, without claiming every parameter is implemented.

The actual parent-core probe exposes 214 complete-return and 1886 error-behavior
differences. The first fresh candidate matches all 4758 complete returns but
retains 106 error-priority differences. Its passing test run is not semantic
acceptance. The corrected C++ API/codec matches all 4758 whole returned carriers,
all 3018 measured error behaviors and all 7776 public Trace equality matrices,
with zero measured differences. Each typed native probe makes 3888 actual calls;
comparisons against two public routes do not double the actual native call count.
Exception behavior is matched as an observed constraint, without asserting that
C++ and Python exception classes are identical. Raw mutable Python list/dict
activity values map to native mutable containers solely for rejection checks;
their raw value codec/equality is not claimed to be identical.

Both candidate and corrected builds freeze 660 inputs (two existing edits,
658 unchanged, no additions), compile 155 fresh production/test outputs and pass
all 152 assertion-enabled Release suites. Each has five actual terminal-zero
process records and verifies NDEBUG removal in every test project. Three probe
preparations bind immutable core-build witness snapshots after the core and early
suite finish. Those snapshots are separately joined here to the complete final
build manifests, with matching source and core hashes. All 310 fresh full-build
outputs and four fresh probe binaries are retained. The baseline probe reuses the
previous accepted core; its parent binaries are not counted as freshly rebuilt.

The corrected 95-model regression makes 95 imports, 2280 full-result calls,
2280 per-trace calls and 2090 count-only calls. It matches all 2216 retained
complete public carriers (2032 PLTR tuples), all 2344 public error behaviors and
all 2280 prior generic native records. Another 1140 positional-input CLI runs
produce 1108 successful returns and 32 overflow exits, matching all 2216 complete
public carriers, 2032 tuples and 64 error outcomes. These 95-model public controls
are reused from the parent evidence and are not counted as new public calls.
Comparisons normalize only dictionary/Marking/set order and paired enabled-name
set projections; activation, problem and context lists remain ordered.

Frozen sources, binaries, projects, complete raw public/native/CLI outputs,
process witnesses, immutable helpers, baseline differences and the first
candidate's differences remain archived. Every chunk and entire file round-trips
exactly. Evidence:
[forward-typed-key-validation.json](forward-typed-key-validation.json).

Unmeasured VARIANTS/cache/time/reduction/case-id/delimiter parameters, broader
ordering domains, date/NA/frozenset/foreign keys and object alias graphs remain
open. The prior forward silent-choice/cleanup, backwards and adapter acceptance
contracts still require reconciliation. Weighted aggregate/composite conformance,
specialized bounded carriers, large frontend JSON precision and general PNML
lexical/marking/export behavior remain gated or unresolved. The catalogue remains
46/86 packages, 199/387 modules and 228 incomplete units. No module/package
promotion or full parity is claimed.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `forward-typed-key-evidence-01.jsonl.gz` | `0256a1d040050ce8f20d4c4af22999fd50b7d9dfac915ac7968958acae332986` | 8912048 |
| `forward-typed-key-evidence-02.jsonl.gz` | `495bc8e872d15915fdca910845c35a00acf4f5e54c7f0475d2460582001fa75f` | 7386962 |
| `forward-typed-key-evidence-03.jsonl.gz` | `b7e651fbfbaaec2dfb6d64df0d210a57e22fc7015c2d046a33203fefca722289` | 7337976 |
| `forward-typed-key-evidence-04.jsonl.gz` | `68c23489c4d836e6254a26f604fedb3241b2cfd0b03d655a69be6a6cdc27c96f` | 6242645 |
| `forward-typed-key-evidence-05.jsonl.gz` | `a8d0a6ff4a5bf14394fe9113610ef83b69c0fdb7becd4303c68098baba16c498` | 7847395 |
| `forward-typed-key-evidence-06.jsonl.gz` | `2f8a12dd9b61fed1f15436e7f48db3f77928058815a4ccb757fc56e9c9ccf530` | 7221985 |
| `forward-typed-key-evidence-07.jsonl.gz` | `8af73a9e544ba98fd58cc6768d6b61bc2d1e3277c3ec962aa6982509f83883f2` | 8302876 |
| `forward-typed-key-evidence-08.jsonl.gz` | `cb51953820af8f390e411a9917d04491d3c72ec90d35780b957290664707db04` | 11984882 |
| `forward-typed-key-evidence-09.jsonl.gz` | `ad56b6159e6745d401a7df77d69ed9a0ce0ac10fd3c3d7e3043eacc369c930b7` | 23181212 |
| `forward-typed-key-evidence-10.jsonl.gz` | `91af28b9db36720029a3dab598e8740e8cd4c573d653f8473a85de4517694665` | 19170829 |
| `forward-typed-key-evidence-11.jsonl.gz` | `33022a58365d26a6dd20471d98501386d4bac8b6a3b568bb149722fc205be89a` | 4436632 |
| `forward-typed-key-evidence-12.jsonl.gz` | `0259c03d25c9f61ebc93cc8460f69361b22a537b5ef7148d4e13d6bb477faaf2` | 7791257 |
| `forward-typed-key-evidence-13.jsonl.gz` | `0c334f38541dff349e14c76498a385479a783478ee56c613abf8ab2156359c20` | 3884613 |
| `forward-typed-key-evidence-14.jsonl.gz` | `09e6c131ff0fdaebfa428c902287b2b677c13ca398d425310065f09a4bedb318` | 7703955 |
| `forward-typed-key-evidence-15.jsonl.gz` | `c65031dbc6916ea53c9ee8ac7e135003b5fb115012df813e0d9f606c13839732` | 7207697 |
| `forward-typed-key-evidence-16.jsonl.gz` | `d7fbf7c5c47a6ef350c16a8ad12d25b8274d07b310e799d0538e05ee5977a527` | 6619164 |
| `forward-typed-key-evidence-17.jsonl.gz` | `15fa2d9d361f1d80708f6a68e404ecf359c14d3108dd4a1ab5623f22c43ce62e` | 7721321 |
| `forward-typed-key-evidence-18.jsonl.gz` | `d53f4b5c51738ce8eac7853af0d86810d3efbf6d9faf24adcc6c0d8d14cdaebb` | 7359047 |
| `forward-typed-key-evidence-19.jsonl.gz` | `c7859f7d6b66fa71164c69a8b43211f441f57ec1a2e3448ac7446914a20f9801` | 7686930 |
| `forward-typed-key-evidence-20.jsonl.gz` | `0bb94141f16fb9c566d5868cbd9acbf9b81c95654f5c4241de42e0ac06685622` | 13627858 |
| `forward-typed-key-evidence-21.jsonl.gz` | `87f3fc7d4656e92381447c99249c926b34ec0a7f682c08677874d8c6ca22d0df` | 22238044 |
| `forward-typed-key-evidence-22.jsonl.gz` | `690f1968312f2d94da065e3c711fd6fdf82954dcad2525381fde4f7493f59f31` | 18065969 |
| `forward-typed-key-evidence-23.jsonl.gz` | `cd1c356dbf2ebbb65e3c263fd8a9b4892502a756163dcfff180df9b97cf449e8` | 4344333 |
| `forward-typed-key-evidence-24.jsonl.gz` | `add3a78ce3c9422f1a5cbd4ebfdb0cbeee6b9ad00f812687c6e79490fef341e6` | 3157910 |
| `forward-typed-key-evidence-25.jsonl.gz` | `389f4821e88fc941a6e9814aa2c42a2604aedef0ae6c717af1fcfa5c3d12b6df` | 3020155 |
| `forward-typed-key-evidence-26.jsonl.gz` | `61004fbaadf8146b3242499c2faf65ccb40ffb3576e144327b12981a51a40196` | 3135199 |
| `forward-typed-key-evidence-27.jsonl.gz` | `7360a9416ea75adce67c38e44e67a33f7d771111493a1e6daec21cd32a4e63d7` | 2863282 |
| `forward-typed-key-evidence-28.jsonl.gz` | `6674d56100bed55cfc1022e94b84a1df2b7d0a427ee5d440f2c01395d4ddfc2d` | 3278349 |
| `forward-typed-key-evidence-29.jsonl.gz` | `c5739d1d3efa0e64bfa466ceb700075eb11c8bd15b74195e610ea72532dd2124` | 3257347 |
| `forward-typed-key-evidence-30.jsonl.gz` | `9df6e06a35f3df0dcae3cae43f4be247901a7537484b63574b3940fdacb00361` | 3370072 |
| `forward-typed-key-evidence-31.jsonl.gz` | `77671f7195dced57186b4d4b8faa875cd306ccd6ccae49c771e6fc1b51fbe57a` | 3438046 |
| `forward-typed-key-evidence-32.jsonl.gz` | `3d834ba5feee41ce13bbac2338a6fd205f4b5c60621064f1fcf7a9d551ceb1c6` | 2920659 |
| `forward-typed-key-evidence-33.jsonl.gz` | `752ac7350e1f166993b3f5dc857ae16bc71c9cc43e30ea04c05b40bb8a6870b5` | 2936881 |
| `forward-typed-key-evidence-34.jsonl.gz` | `8d19efa553c680c278af9358c42b0702a1fb9737fdf230ebfd4c07425f6b9cf2` | 2473434 |


## Forward supplied variants and hidden-cache parameter checkpoint

Two new public workers each import the same six exact weighted PNML models.
Together they execute 13440 actual calls through the direct forward and parent
dispatcher routes, retaining 11348 complete returned carriers and 2092 actual
errors with zero serialization gaps. Every direct/dispatcher pair has an equal
whole outcome. Model/marking/log/parameter snapshots and seven opaque pinned
module proofs remain unchanged before and after. The five EventLog cohorts cover
duplicate source traces and case IDs, equal numeric activity keys, mixed null
activities, enum/string custom activity selectors and an empty log. Four full
result policies include grouped/source PLTR and named carriers.

The supplied-variant worker makes 5280 calls across 18 forms. Omitted/None/empty
maps, inferred and reordered maps, subsets, extra foreign Trace values, zero
frequencies, index/string/integer/None values, tuple/string/bytes/scalar keys,
custom delimiters and a raw list all retain the complete omitted-parameter
outcomes on these inputs: 4488 returns and 792 errors. This is an observed
no-effect for these exact EventLog controls, including contradictory carriers;
it does not establish a universal result for other input adapters or domains.

The other worker makes 8160 calls across 27 controls for CASE_ID_KEY, delimiter,
IS_REDUCTION, THREAD_MAX_EX_TIME, PLACES_SHORTEST_PATH_BY_HIDDEN and progress.
Case selectors, reduction settings, zero/negative/tiny/None/text/long time limits,
delimiters and progress settings preserve the measured return/error outcomes.
Progress console bytes are retained separately and are not claimed to be equal.
Hidden-cache matrices are owned constructions from public Petri-net arc
endpoints, with actual Place and Transition keys/values. An actual single-hidden
path matrix and source-only cache preserve these controls, while empty,
identity-only, reversed and empty-list caches alter 260 complete outcomes.
A non-iterable integer cache adds 174 observed TypeErrors. There are 434
differences from omitted-cache behavior; no input mutations are observed.

One freshly compiled native probe binds the existing accepted core and all 660
unchanged source inputs. It makes six imports and 6720 actual full API/codec
calls. Both public routes compare all 13440 complete outcomes. The probe matches
11088 of 11348 complete returns, 1918 of 2092 error behaviors and every Trace
equality matrix. All 434 differences belong to explicit hidden-cache controls.
The current native option has no supplied-cache carrier; those cache arguments
are explicitly unmapped in this baseline. The other measured no-effect
parameters map to the existing EventLog options. This does not claim support for
unmeasured values or adapters. Native output fields are never synthesized.

The first native preparation fails on one stale capture label before copying
its first fixture or invoking CMake. Its helper and incomplete preparation are
retained, with zero native calls. A derived helper corrects that label, compiles
the probe and completes all three actual processes with exit zero. The public
captures, raw native carriers, process witnesses, helpers, failed preparation
and probe binary are archived, with every chunk and whole file round-tripping.
The earlier accepted production build and test evidence are reused; this step
does not claim a fresh full build or another 152-suite run.

The next production requirement is an explicit hidden-path-cache override,
including empty-cache routing and the measured rejection timing. Broader cache
paths, cyclic/choice/cleanup behavior, foreign key carriers, input adapters and
alias graphs remain open. The prior forward/backwards acceptance contracts and
weighted aggregate/composite gates still apply. The catalogue stays at 46/86
packages, 199/387 modules and 228 incomplete units, with no promotions or full
parity claim. Evidence: [forward-controls-validation.json](forward-controls-validation.json).

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `forward-controls-evidence-01.jsonl.gz` | `559933a84a1b29ba6cb928a5f3290f28783eb1f5ef2ba1bfea40a744c8f1e691` | 1339747 |
| `forward-controls-evidence-02.jsonl.gz` | `da3b7e2f6a868797bff56288ba7af9edeb7f8576fc85b65f29ef76417557c8e2` | 2550019 |
| `forward-controls-evidence-03.jsonl.gz` | `953d7c7e6ce9aa84c6f197c381881448c67e91b2772efe1e65372b80a6432c56` | 3330120 |
| `forward-controls-evidence-04.jsonl.gz` | `167de5ef02f1c42f57f37798bbea247ece24209491b2b46b8bdc8b8f4b8c7a49` | 3311192 |
| `forward-controls-evidence-05.jsonl.gz` | `cb80085f25c19552dd878f48f1c5eea88fe1cf093216565f96b67bc20a8fd678` | 3736395 |
| `forward-controls-evidence-06.jsonl.gz` | `b96988c74f3b672768da842084022c8c4dd08bfb004cdda07e707b366a64c6ce` | 3269123 |
| `forward-controls-evidence-07.jsonl.gz` | `a5621b5159d804fafebc9f39aabf3fd4d2bc19c378e588709d4b2cbb1eb79ca4` | 3464470 |
| `forward-controls-evidence-08.jsonl.gz` | `86e057de9cee17ab238ed4806f70447955c447a6834226f11c9e9d31327d2bf5` | 3656749 |
| `forward-controls-evidence-09.jsonl.gz` | `53a40f54fada66d7e61b625206fd649e7d2873d04895d54286ec1e005d89fd95` | 3401747 |
| `forward-controls-evidence-10.jsonl.gz` | `e89be1aeb1fd81c5129bdee273d2462ee4ddbd22487ff2400654cbc5c1b4f679` | 3776580 |
| `forward-controls-evidence-11.jsonl.gz` | `629088f94034a5f41a596a6f961ba2e877e61f32b234c6b2670641d22469fb3e` | 3532494 |
| `forward-controls-evidence-12.jsonl.gz` | `e936df15eea95ad946e29f05a5b1f13ea0e9f0f3322095ed7ffb3cbbf5e80160` | 2419137 |
| `forward-controls-evidence-13.jsonl.gz` | `6c6eb8145c3ec4481d768fc62352debe0c75df90ac2fe23cdbe9b92b6f2702e5` | 3333194 |
| `forward-controls-evidence-14.jsonl.gz` | `98393f09d75388bc0aa773beeb441f50d41a650287d5a05369e72b5f531d5faa` | 3137098 |
| `forward-controls-evidence-15.jsonl.gz` | `b01acbc5364647de5e41cba39e5d454f6549b73be7192a521b9275b24f8ef6fd` | 3084574 |
| `forward-controls-evidence-16.jsonl.gz` | `af065a1380d62c84f5cc60c8afc57afc5991ea490b2b8caae5f0c70a81a91db4` | 3240809 |
| `forward-controls-evidence-17.jsonl.gz` | `67fbcdf3c848d580b73120e70fd73753407b758897e3d29f9545a5ccff899bbb` | 3329049 |
| `forward-controls-evidence-18.jsonl.gz` | `c919f19f443f3e1daf7a7be4b2b8b582224dc5775ed291ed9d3e2a7662f4af61` | 3428755 |
| `forward-controls-evidence-19.jsonl.gz` | `328adc18a2b8598aa63564fe8534ef726d212e908b79e68801eb0787a4831372` | 2776308 |

## Supplied hidden paths and cleaned-place PLTR membership

The forward C++ API now accepts `places_shortest_path_by_hidden` as a
place-index matrix of transition-index paths, or a native value carrier.
Absence and explicit null retain structural path computation. An explicitly
empty matrix suppresses hidden routing before events and when reaching the
final marking. Supplied paths retain their order, including partial paths,
repeated transitions and identity entries, and use the existing replay kernel.
The matrix is examined at path lookup; a non-iterable scalar fails only when
a marking key is actually examined. Empty logs, disabled routing, null PLTR
keys and earlier failures retain their measured precedence. Iterable native
value containers cannot contain model Place objects and have no matching rows.
This is an EventLog/PetriNet C++ API contract; complex supplied-cache parsing
through the CLI, HTTP and other adapters remains unimplemented.

Token-flood cleaning now records the places it clears. Their traces remain in
the PLTR overfed-trace sets after the reached marking loses those place keys.
The change preserves token counts and the full tagged return carrier.

The 13440 public outcomes from the preceding parameter checkpoint are reused
with exact hashes. The accepted native probe matches all 11348 complete returns,
2092 measured error behaviors and 13440 independent Trace equality matrices.
This closes the preceding 260 return and 174 error-behavior differences.

Two fresh public captures add 8424 actual calls through both the direct and
parent APIs, with 8016 complete returns, 408 actual errors and zero serialization
errors. Each imports six owned PNML fixtures and exercises 351 profiles across
three log cohorts, eleven cache forms, nine policies and selected enum/string
parameter keys. The forms include complete, empty, prefix, last-step, selected
branch, identity, repeated-transition, null and integer caches. Every input,
marking, log and parameter snapshot is unchanged after each public call, and
the seven opaque module pin proofs are unchanged before and after each capture.
All 4212 direct/parent outcome pairs agree as complete carriers or actual errors.

The first fixture capture omitted explicit invisibility markers. Public import
snapshots prove that its transitions have visible fallback labels, so it is
retained as visible-routing and cleaning evidence. A derived capture changes
only the ProM invisibility declarations; public snapshots independently confirm
silent-transition counts of 2, 2, 2, 4, 2 and 2. These corrected fixtures cover
multi-step pre-event/final paths, a join, a choice, a self-loop and an explicit
zero-token initial marking key. The accepted core matches all 4008 complete
returns and 204 errors in each fixture capture, with no differences or partial
carrier acceptance.

| Accepted comparison | Whole returns matched | Error behaviors matched |
| --- | ---: | ---: |
| Retained parameter controls | 11348 | 2092 |
| New visible-routing/cleaning controls | 4008 | 204 |
| Corrected invisible-path controls | 4008 | 204 |
| Retained typed-key controls | 4758 | 3018 |
| Retained 95-model full-result controls | 2216 | 2344 |

The 95-model regression also preserves all 2280 legacy native records and 2032
complete tuple carriers, with 2280 per-trace calls and 2090 accounting calls.
The positional CLI completes 1140 actual processes: 1108 results and 32 expected
floating-point overflow failures. Both retained public routes match all 2216
CLI complete carriers, 2032 tuple carriers and 64 error behaviors. No native
output fields are synthesized, and native exception classes are not asserted
to be Python classes.

The initial compile fails on an incorrect native enum name before any replay.
The corrected candidate and final build each freeze 660 inputs, with three
edited and 657 unchanged, produce 155 fresh binaries and pass all 152
assertion-enabled Release suites. Passing tests do not alone establish semantic
acceptance: the first compiled candidate retains 144 whole-result differences
in cleaned-place PLTR sets. The final diagnostic tracking resolves all 144.
Seven actual probe builds retain 131 imports and 25926 full-result calls in
total, including both candidates and regressions. Their immutable early-core
witnesses are separately bound to the final five-command build manifests.
The generated-helper delimiter failure and correction are also retained; that
failure precedes every native process from that helper. The evidence retains
317 fresh full-build/probe binaries, frozen inputs, fixtures, raw returns,
process witnesses, comparisons and helpers. Parent captures and builds are
reused with verified hashes and are not counted as fresh public calls or builds.

Evidence and byte-level archive hashes are in
[forward-hidden-cache-validation.json](forward-hidden-cache-validation.json).
All 5499 retained files round-trip across 42 archives, including every chunk,
whole-file digest and byte count.
Broader foreign/aliased cache objects, invalid model rows, extended-arc cache
matrices and adapter transports remain unmeasured. The prior forward
choice/flood-cleaning and backwards acceptance contracts, weighted
aggregate/composite gates, bounded carriers and general PNML/precision gates
still apply. The catalogue remains 46/86 packages, 199/387 modules and 228
incomplete units, with no promotions or full parity claim.

## Minimized silent-choice oracle and underfed-place precedence

Twelve new acyclic controls isolate the competing `p0 -> t3 -> p3` and
`p0 + p1 -> t6 -> p3` silent paths from retained flood case 62. They rename the
places to source/guard/gate/sink, remove unrelated routing and visible-input
deficits, and independently vary initial source tokens (1 or 3), visible input
weight (1 or 2), one/two/three repeated events, cleaning and PLTR reporting.
Good-only and blocked-only nets provide deterministic controls; the choice
nets preserve both silent alternatives. This isolation does not claim to
preserve every outcome of the original larger fixture.

A fresh public capture imports all twelve exact PNMLs and makes 1152 actual
direct/parent calls across 48 profiles per net. Automatic, good-first,
blocked-first and empty caches keep each supplied matrix fixed throughout a
trace. All 1152 calls return complete carriers, with no errors or serialization
failures. All 576 route pairs agree. The seven opaque module pin proofs and all
actual input, marking, log and parameter snapshots remain unchanged.

The independent oracle reads only the owned inputs. The guard has no producer,
all transitions have one output and a topological ranking certifies acyclicity.
The oracle enumerates the sole global silent order in each of eight deterministic
fixtures and both global orders in each of four choice fixtures. Automatic
routing uses one order consistently across repeated events; explicit caches
select the declared fixed path or suppress routing. The oracle derives ordered
firings, every reached marking and token counter, fitness, full PLTR membership,
Trace/Place/Transition carrier values and ordered context occurrence lists.
It explains every whole public carrier. Negative controls reject both a replay
that switches global order between events and a carrier assembled from one
path's trace fields and another path's PLTR fields.

The unchanged parent core has 30 unique whole native results that no oracle
path explains, corresponding to 60 direct/parent comparisons. Each difference
is in PLTR membership after cleaning a partially available visible input:
the gate has both missing and cleared tokens, but native reporting adds that
trace to both underfed and overfed sets. Actual public reporting gives event
underfed membership precedence. The correction applies that precedence to
cleaned places as well as reached residual places, preserving all counters.
The new native contract test exercises a weighted gate that is repaired and
then cleared; it must appear only in the underfed set.

The corrected core matches all 1152 complete public outcomes exactly, and all
576 unique native outcomes belong to complete oracle paths. A scope review
checks every actual public topology/label/marking snapshot against the oracle's
constraints, verifies that its only import is `itertools`, and confirms that
expected carriers remain identical before and after the production correction.
This is a recorded self-review of the bounded assumptions, not a claim of
independent human or subagent review.

The retained parameter, visible/invisible-path, typed and 95-model regressions
match all 26338 complete carriers and 7862 measured error behaviors. Together
with the new controls, accepted native comparisons match 27490 complete returns
and 30792 independent Trace equality matrices. The 95-model checks preserve
2032 tuple carriers and 2280 legacy native records. One fresh build freezes 660
inputs, with two edited and 658 unchanged, and passes all 152 assertion-enabled
Release suites. Seven actual probe builds retain the baseline and corrected
results; the completed parent production build is reused only for the baseline.
Parent public captures are reused with exact hashes and are not fresh calls.
The 1140 positional CLI processes retain 1108 successful results and 32 expected
overflow failures, matching 2216 complete carriers, 2032 tuple carriers and 64
error behaviors through the retained public routes. All 162 fresh full-build
and probe binaries are retained. Every chunk and all 3699 whole files
round-trip across 18 evidence archives.

Evidence and archive byte hashes are in
[forward-choice-oracle-validation.json](forward-choice-oracle-validation.json).
This oracle establishes complete support only for the declared twelve minimized
fixtures. The original seventeen differing weighted fixtures still require
globally consistent full-path enumeration and reconciliation with every
diagnostic field. Broader forward/backwards, supported adapter, cache/alias and
distinct catalogue acceptance gates remain open. No catalogue row is promoted;
totals remain 46/86 packages, 199/387 modules and 228 incomplete units.

## Original seventeen fixtures: complete carriers and fixed structural caches

The original seventeen weighted acyclic choice/flood fixtures now have fresh
complete direct/parent captures. They preserve the exact retained PNML, original
XES trace attributes/events and each fixture's hidden-walk, exhaustive and
final-routing flags. Cleaning and PLTR are varied independently. Five cache
policies cover automatic routing, the actual public structural matrix, owned
ascending/descending structural orders and an empty matrix. All 680 public
calls return complete carriers and all 340 direct/parent pairs agree. The eight
opaque module pin proofs and all actual net, marking, log and parameter
snapshots remain unchanged. The public shortest-path and cleaning-component
helpers supply 34 additional actual returns.

A fresh probe of the unchanged accepted production core makes 340 full API
calls. All 544 direct/parent comparisons with identical supplied caches match
every field. Automatic-cache comparison retains 48 exact matches and 88 strict
differences. The result of each of the 68 native automatic-cache calls matches one
whole actual public result under the fixed ascending cache through both public
routes, giving 136 complete public witnesses. Activation/problem order,
markings, token counters, fitness, PLTR memberships and context lists are kept
together; no fields are combined across choices.

An independently written pure graph oracle enumerates all 424 simple structural
paths in these owned acyclic nets and retains every minimum-length path. It
resolves ties using one fixed outgoing silent-arc order per place and one fixed
output order per transition, shared by every source search and repeated event.
Complete enumeration of all 1,217,856 such configurations yields 382 distinct
complete shortest-path matrices. Every matrix retains a concrete fixed-order
witness and its configuration multiplicity. The actual public cache is
explained by its captured public arc orders for all seventeen nets; all 51
actual-public/ascending/descending control matrices belong to the enumerated
sets. This structural domain permits fixed local arc orders; it does not claim
to reproduce the distribution of Python object allocation or set iteration.

A recorded self-review checks the oracle's input-only implementation, all 680
actual input snapshots, every matrix witness and multiplicity, and every whole
native/public witness. A negative control combines individually shortest paths
into a matrix that no one fixed order can explain; the complete enumeration
rejects it. The initial component-name serializer failure and an archive
validator's byte-distinct JSON-manifest assertion are retained with their
corrections. Neither is a reference replay failure. One new probe is built;
the completed parent build, all 660 unchanged frozen inputs and its 152 passing
assertion-enabled suites are reused with exact byte bindings. Every chunk and
all 148 evidence files round-trip across two archives.

Evidence and archive byte hashes are in
[forward-choice-original-validation.json](forward-choice-original-validation.json).
This completes structural-cache enumeration and supplies whole public witnesses
for the current native automatic choices. It does not independently derive the
whole replay results for all 382 matrices. Full-path firing, cleanup, counters
and diagnostic derivation remain required before the original-seventeen
legal-set contract is complete. Broader forward/backwards, adapter, cache/alias
and distinct catalogue acceptance gates remain open. Catalogue totals remain
46/86 packages, 199/387 modules and 228 incomplete units, with no promotions or
full parity claim.

## Original seventeen fixtures: complete fixed-order replay paths

The original seventeen weighted acyclic choice/flood fixtures now satisfy their
complete fixed-order acceptance contract. The independent whole-path oracle
uses the exact owned PNML/XES inputs and all 382 complete structural matrices
from the previously exhausted 1,217,856 fixed local arc-order configurations.
Each matrix remains fixed throughout the repeated trace. Cleaning and PLTR vary
independently, giving 1528 whole expected records. The oracle derives ordered
silent/visible firings, weak weighted repairs, cleanup, markings, token counters,
fitness and full diagnostic carriers from inputs alone. Its finite physical
silent-state search has a strictly decreasing potential for these acyclic nets.

All 1528 whole predictions were frozen before the fresh public producer started.
Seventeen fresh public imports yield 3056 complete direct/parent returns, with
zero replay or serialization errors and 1528 equal route pairs. Seventeen native
imports yield 1528 complete full API returns. Every native/public comparison
matches the complete carrier exactly: 3056 comparisons and 3056 Trace equality
matrices. The oracle explains all fresh returns, the retained parent 680 public
and 340 native returns, and all 501 original/resampled historical observations
under their exact legacy serializer. Historical results are matched as whole
paths; fresh tagged carriers separately cover types and identities absent from
that older serializer. Nine opaque public module pin proofs and all actual input
snapshots remain unchanged.

The reported eventually-enabled diagnostic is distinct from physical silent
reachability. All 382 supplied-cache outcomes agree with both derivations. Two
supplemental empty-cache states have physically reachable visible transitions
but an empty actual public diagnostic. Forty-seven fresh direct helper calls
cover every distinct derived marking and both controls. Together with 3056
post-replay diagnostic helper calls and 34 structural helper calls, this step
retains 3137 actual helper calls. Helper evaluation leaves the marking, net and
original replay result unchanged. The domain-specific cleanup derivation uses
the active sink co-membership: only the first forced visible event can introduce
that newly surviving key. This proves the replay cleanup effect here; standalone
structural-component helper carrier parity remains open.

A recorded self-review checks all 1528 weighted-flow ledgers, 3720 event steps
and 382 fixed matrices. Negative controls reject a replay that changes global
order between events and a carrier mixing trace fields from one path with PLTR
fields from another. Three immutable oracle versions are retained. The first
version's eight public/four native parent diagnostic differences and a later
intermediate-marking alias failure remain visible; their corrections leave all
1528 pre-public whole-return predictions unchanged. The first generated native
probe terminates with stack overflow before its first full replay call. The
corrected probe separates generated cache/profile factories and succeeds with
the same default 1 MiB PE stack reserve; both binaries and actual process records
are retained. No production inputs change. All 660 frozen inputs and the accepted
core/build manifest remain byte-bound to the reused 152 passing assertion-enabled
Release suites. This step does not claim a fresh full build or test-suite run.

Evidence and archive byte hashes are in
[forward-choice-complete-validation.json](forward-choice-complete-validation.json).
All 269 retained files, chunks, byte counts and whole-file digests round-trip
across sixteen archives. This closes the original-seventeen fixed-order
choice/flood contract. Broader forward/backwards controls, supported adapters,
cache/alias domains, standalone component carriers, search bounds and distinct
catalogue acceptance remain open. Supplied-cache CLI/HTTP transports, weighted
aggregate/composite conformance and general PNML/precision gates also remain
open. Catalogue totals stay at 46/86 packages, 199/387 modules and 228 incomplete
units, with no promotions or full parity claim.

## Supplied hidden-path cache transport

Supplied caches now reach forward replay through the CLI and standalone HTTP
route as complete ordered model-ID paths. The C++ cache carrier also accepts
`TokenReplayNamedHiddenPathMatrix`; the existing index matrix remains available.
Named rows resolve against PNML IDs or configured replay names only when replay
examines that source/target pair. Unused foreign rows remain unused, while a used
invalid or ambiguous model ID raises the explicit native diagnostic. Empty and
null caches retain their existing suppression/automatic-routing meanings.

The CLI accepts `--token-replay-places-shortest-path-by-hidden <json>` alongside
`--token-replay-model` and `--token-replay-result`. The corresponding request field
is `token_replay_places_shortest_path_by_hidden`. Its object grammar is
`{"source":{"gate":["s0","s1"]}}`: place IDs form both dictionary levels and
transition IDs form the ordered path. Empty rows, partial paths, identity entries
and repeated transitions are retained. `{}` suppresses routing and `null` selects
automatic paths. Other JSON value carriers retain deferred iterable checks;
integers use the shared parser's int64/uint64 representation and nesting limit.
Malformed JSON or matrix shapes fail at the transport boundary. This wire grammar
does not imply support for arbitrary Python cache-object or alias graphs.

`POST /api/token-replay` accepts multipart `log` (CSV or XES) and `model` (PNML)
uploads and returns the same full forward tagged carrier as the CLI, without a
mining session. Replay/import controls may be multipart fields or query
parameters. The route preserves supplied PNML markings and uses the existing
log importers. Runtime and parse failures return HTTP 400 with the exact native
diagnostic. The full result API's forward-variant contract still applies.

Actual CLI and HTTP checks each execute 3634 faithful model/profile inputs.
They cover all 351 profiles on the six retained chain/join/choice/self-loop/
zero-key controls and all 1528 policies on the original seventeen weighted
fixtures, including every one of their 382 complete fixed caches. Actual
Place/Transition parameter objects become their verified model IDs; source
trace boundaries, typed attributes/events and log attributes are retained in XES.
One PNML adds an explicit zero-token gate marking to reproduce the actual public
marking modified after import. The other twenty-two PNML files remain byte exact.
Every represented initial/final marking is checked against the captured input.

Both transports return 3532 complete carriers and 102 expected failures each.
All 14128 complete comparisons against both retained public routes match every
field; all 408 error comparisons match the measured iterable-cache rejection.
All 3634 CLI/HTTP whole-outcome pairs agree. Float hexadecimal values, exact
integer counts, carrier types and ordered activation/problem/context lists stay
strict. Only unordered dictionary/Marking/set slots and paired enabled-name/label
projections are canonicalized. The 7268 public calls and opaque module pin proofs
are reused through their already committed archive bindings; no new public calls
are claimed. Six additional CLI/HTTP boundary pairs pass, covering native CSV
consistency, malformed JSON/rows and used/unused invalid transition names. Those
CSV checks establish native transport consistency, with broader Python input
adapter parity still a separate gate.

One fresh build freezes 660 inputs (seven edits, 653 unchanged), produces 155
fresh linked production/test artifacts and passes all 152 assertion-enabled
Release suites. The first Python HTTP harness shadows its imported module and
fails before any CLI process or HTTP replay request. Its stopped server process,
original helper and frozen preparation are retained; a derived correction uses
identical production bytes and request inputs. A recorded self-review checks the
source/build/process bindings, actual input translation and every comparison.
The review does not claim an independent human or subagent review.

Evidence and byte-level hashes are in
[forward-hidden-cache-transport-validation.json](forward-hidden-cache-transport-validation.json).
All 1143 archive members round-trip across fourteen archives. An additional
lossless JSONL bundle retains and independently round-trips all 14536 original
CLI stdout/stderr and HTTP request/response files, with per-file names, byte
counts and SHA-256 digests. Broader forward controls, supported input adapters,
cache/variant aliases, standalone component carriers, search bounds and distinct
catalogue acceptance remain open. Weighted aggregate/composite conformance,
general PNML carrier limits and exact frontend numeric presentation also remain
open. Catalogue totals stay at 46/86 packages, 199/387 modules and 228 incomplete
units, with no promotions or full parity claim.

## Exact aggregation of weighted replay results

`evaluate_replay_fitness` now accepts `std::vector<TokenReplayTraceDiagnostics>`
and `TokenReplayResult` directly. These aggregate routes retain exact signed
`TokenInteger` counters and the input rows, so callers can evaluate the full
weighted replay result without converting its counts to unsigned integers.
`trace_is_fit` determines fitting classification independently of the supplied
trace fitness. Fitness outside [0, 1], NaN and infinities propagate through
sequential double summation as observed on the pinned public evaluator.

For nonempty input with positive aggregate consumed and produced totals, the
public log fitness uses exact integer ratios and the average uses all input
trace fitness values. Otherwise all four public aggregate scores are zero,
including the fitting percentage. `log_fitness_is_integer_zero` distinguishes
the public integer-zero carrier in that branch from a calculated floating zero.
Native trace/fitting counts and signed token totals remain available in either
branch. The four-field public projection retains `perc_fit_traces`,
`average_trace_fitness`, `log_fitness` and `percentage_of_fitting_traces`, including
their observed types and exact double hexadecimal values. This contract concerns
the new C++ aggregate overloads; existing application serializers remain a
separate transport/presentation gate.

`token_integer_ratio` rounds an exact integer quotient to binary64 using
round-to-nearest, ties-to-even. It finds the binary exponent without converting
either operand to double, extracts the significand using exact arithmetic and
rounds from the exact remainder. The subnormal branch rounds in units of
2^-1074, preserving signed zero. Overflow and a zero divisor retain explicit
errors. This handles finite ratios even when both integer operands exceed the
floating-point range. Existing trace replay arithmetic is unchanged.

The accepted cohort makes 2486 fresh pinned public aggregate calls on 1243
frozen inputs through both direct and parent routes. It includes all 162 signed
small-counter/classification combinations, outside-unit/nonfinite fitness,
large comparable integers through 401 decimal digits, mixed/cancelling totals,
1040 retained weighted replay observations and four aggregate prefixes. The
reused weighted observations are byte-bound to their committed evidence; no new
public replay calls are claimed. Both exact native carriers make 2486 actual
aggregate calls. Every one of the 4972 whole public-route comparisons matches,
as do 4972 comparisons against independent aggregate calculations, 2486
exact-counter checks and
7458 input/retained-row checks. All 1243 native carrier pairs and all 1243 public
route pairs agree. Separately, 2315 native exact integer-division calls match
actual Python division observations, including rounding ties, signed zero,
subnormal underflow, finite ratios of huge operands and expected errors.

One fresh build freezes 660 inputs (four edits, 656 unchanged), produces 155
linked production/test artifacts and passes all 152 assertion-enabled Release
suites. The first comparison helper cannot parse MSVC's indeterminate-NaN
display. Its terminal failure is retained; a derived helper recognizes that
display as the public float.hex NaN class. Production inputs, native probe,
observations and finite/signed-zero comparisons remain unchanged. The recorded
source/build/process/archive self-review claims neither an independent reviewer
nor a pre-public oracle.

Evidence is in
[weighted-fitness-aggregate-validation.json](weighted-fitness-aggregate-validation.json).
All 1026 files round-trip across 12 lossless archives. Log-based and
CLI/HTTP weighted fitness dispatch, full PNML marking carriers, composite/
precision/application conformance and exact frontend presentation remain open.
The legacy bounded `PrecomputedTokenReplay` validation/alias API remains
separate; arbitrary Python result dictionaries, parameter/alias behavior and
broader evaluator module acceptance are also open. Catalogue totals stay at
46/86 packages, 199/387 modules and 228 incomplete units, with no promotions or
full parity claim.

## Exact log-based token fitness and standalone transport

Explicit token-based `evaluate_replay_fitness(EventLog, PetriNet, ...)` now
replays the grouped log and feeds its source-order trace rows into the exact
aggregate evaluator. Fitness evaluation no longer runs precision,
generalization or simplicity as side effects. The compatibility detail
serializer reports those unevaluated scores as null. Trace diagnostics,
exact signed counters, transition frequencies and per-trace variant entries
remain available. The frontend accepts that detail shape and transports the
forward cleaning control.

`ReplayFitnessOptions` accepts exact initial/final `TokenMarking` values,
sparse marking-key membership and model element names. Exact values override
legacy integer vectors. Forward replay retains signed input counts; backwards
exact markings keep their existing nonnegative gate. A signed final entry
adjusts a place only when that place is present in the reached marking;
positive absent final entries still contribute missing tokens. The new
`PnmlDocument` overload supplies exact document counts, sparse keys and IDs
unless the caller provides overrides. The PNML document parser's nonnegative
int64 marking carrier remains separate from unbounded native exact markings.

Trace fitness preserves the public separate-double-conversion calculation,
while aggregate log fitness uses exact integer division. With a source/final
count of 2^53 and one visible firing, the observed average trace fitness is
2^-53 and log fitness is 2^-52; both distinct values survive. Empty/nonpositive
aggregate denominators retain the public integer-zero log-fitness carrier.
Nonempty 401-digit marking cases raise `int too large to convert to float`;
empty logs return the observed zeros. Empty-trace backwards fitness raises
`float division by zero`. Cleaning defaults to false and can be enabled for
forward fitness independently of application conformance.

`token_replay_fitness_result_json` emits the complete public four-field tagged
dictionary: `perc_fit_traces`, `average_trace_fitness`, `log_fitness` and
`percentage_of_fitting_traces`. Integer/float types and exact float.hex values
remain explicit. The standalone CLI uses `--replay-fitness-model <file>` on a
CSV/XES log. `POST /api/replay-fitness` accepts multipart `log` and `model`
fields without a mining session. Both use explicit token fitness and accept
the existing evaluation selector/replay controls plus
`fitness_cleaning_token_flood`. Explicit non-token fitness selection is
rejected. The existing model-quality bundle and other evaluator families
retain their separate contracts.

Fresh pinned observations make 3264 direct/parent fitness apply calls, 90
full-replay calls for marked models and 88 PNML imports. They cover 65 weighted
PNML models, six path-control models, all 17 original choice models and 18
exact marking models. Grouped inputs include empty logs/traces, repeated and
unknown events, cleaning true/false, custom activity selection and backwards
replay. Additional raw token control parameters are observed at the public
fitness-wrapper boundary; this cohort does not establish general alias or
parameter parity. Every public log/model/marking snapshot check is retained.

The native run makes 3264 actual fitness calls, 90 marked-model replay calls
and 88 imports. All 4584 whole public comparisons in the accepted domain
(65 weighted models, five path controls and 18 exact marked models) match
every score, scalar carrier type and float hex. All 86 successful six-field
marked trace projections match, including signed initial/final counts and
the observed overflow outcomes. Both native carriers agree on all 1632
input pairs, as do both pinned public routes. The compatibility detail always
leaves the three unevaluated quality scores null.

1632 actual CLI processes and 1632 cold-server HTTP requests exercise the same
profiles, alternating multipart and query options. All 4460 whole public
comparisons in the accepted wire domain match. Every CLI/HTTP pair and native
wire comparison matches, including observed backwards errors. Signed and
above-int64 PNML markings retain 70 explicit carrier-error checks and remain
unsupported on the wire; they are not counted as matched public fitness.

Two important gaps remain explicit. The 17 original automatic-choice models
produce 312 different whole dictionaries in native comparisons and another
312 in wire comparisons. The wider one/empty/mixed/unknown cohort is outside
the earlier complete fixed-cache proof; these outcomes are retained without
accepting a score range or stitching fields. Path-control model 5 retains an
initial zero key in the native PNML document that the reference importer
omits. Its four scores agree, but its input-carrier parity remains open.

The final 660-input build changes 16 inputs, links all 155 production/test
artifacts and passes all 152 assertion-enabled Release suites. TypeScript
and all 87 targeted frontend tests pass on matching frozen frontend sources.
Retained failures include the missing frontend query field, signed-input
validation, the negative-final sparse-key distinction, an inconsistent unit
fixture's key metadata, a diagnostic compile failure and two MSVC C1001
failures. Compiler continuations use unchanged frozen sources and fresh
terminal witnesses. The self-review is not an independent reviewer or a
pre-public oracle.

Evidence is in [weighted-fitness-apply-validation.json](weighted-fitness-apply-validation.json).
All 11185 files round-trip across 18 lossless archives.
AUTO dispatch, composite/precision/application conformance, legacy bounded
precomputed aliases, arbitrary Python result carriers and broader parameter
domains remain open. Catalogue totals stay at 46/86 packages, 199/387 modules
and 228 incomplete units, with no promotion or full parity claim.

### Exact PNML marking import for log-based fitness

`PnmlDocument` carries initial and final counts as exact `TokenMarking`
integers. The document and explicit marking exporters preserve nonnegative
counts beyond 64 bits. Bounded alignment, reduction, comparison and interactive
adapters validate their integer ranges before conversion. Legacy document
mode continues to preserve zero keys, the first final alternative and additive
duplicate final entries.

`PnmlImportOptions::public_marking_semantics` selects the observed pinned
PM4Py marking import behavior: positive counts only, no initial inference,
and later positive final entries replacing earlier ones across all alternatives.
Later zero, negative or missing-text final entries leave an earlier positive
entry present. The last parsed initial count determines its place's membership;
an initialMarking without a text element does not replace a parsed count.
When finalmarkings is absent, every structural sink receives one token,
including isolated places. An explicitly empty finalmarkings element preserves
an empty marking. Empty text elements raise the observed NoneType message.
Decimal counts accept signs, leading zeros, Unicode decimal digits and whitespace,
and single digit-separated underscores. Invalid integer literals are rejected;
their exact Python error wording remains open.

The standalone `--replay-fitness-model` CLI and cold `POST /api/replay-fitness`
route use the public marking mode. Signed raw counts follow the observed dropping
semantics; large positive counts reach exact token fitness. This closes the
previous standalone marking-width and zero-key input gaps in the declared
cohort. The native full-replay document route retains its existing key-preserving
contract. Direct exact C++ markings can still carry signed counts independently
of the public PNML import rule. At 401 digits, empty logs return the observed
integer/float zeros; nonempty logs raise `int too large to convert to float`.

Two fresh pinned captures make 304 actual imports,
2080 fitness calls and 1040
full-replay calls. These counts include the original 70-case capture and its
82-case boundary extension. The final cohort contains 58 successfully imported
models and 24 rejected models, each observed through file and deserialize routes.
Five faithful grouped logs cover empty logs/traces, one/two firings and repeated,
unknown-event mixed traces. All 116 native marking
and named structural projections match. All 2288
whole typed four-field fitness comparisons and 572
six-field trace projections match, as do 24 expected
overflow checks and all 580 document/net pairs.

410 actual CLI processes and 410 HTTP
requests make 1144 matching whole public
fitness comparisons. Every CLI/HTTP pair and all 820 native
wire checks agree. The 22 invalid-integer models retain 44
native import wording differences and 110 CLI/HTTP
command-pair wording differences; these are recorded and excluded from an exact
error-parity claim.

All 152 assertion-enabled Release suites
pass against 660 frozen inputs (seven edits, 653 unchanged), with 155 fresh linked
production/test artifacts. The first focused test failure is preserved: its
Unicode fixture was compiled in code page 1252, with C4566 replacement warnings.
Both UTF-8 diagnostic clones passed, and a fresh source freeze encoding those
fixture bytes explicitly passed the full build. Executed helpers, original inputs
and failure witnesses remain immutable. Source/build/process/archive self-review
is not an independent review or a pre-public oracle.

Evidence: [pnml-exact-markings-validation.json](pnml-exact-markings-validation.json).
All 3727 archive members round-trip across 20 lossless
archives, linked to the prior log-fitness evidence. Arbitrary PNML syntax and
properties, marking insertion order, Python exception identities and complete
importer/exporter parameter domains remain unproven. The original automatic-choice
cohort retains its previous 312 native and 312 wire whole-dictionary differences.
AUTO/composite/precision/application conformance, bounded consumers, aliases and
broader Python carrier/parameter domains remain open. Catalogue totals stay at
46/86 packages, 199/387 modules and 228 incomplete units; no promotion or full
parity claim is made.

### Exact PNML integer lexical errors and XML text

The integer-wording gaps recorded in the preceding checkpoint are closed for
the observed cohort. Public marking mode renders invalid integer literals with
the observed quote selection, slash/quote escaping and Unicode 14 printability.
The complete rendered literal is truncated to 200 Unicode scalars, preserving
supplementary characters, partial escape sequences and trailing spaces.

Valid integer grammar is checked before the pinned Python runtime's observed
default limit of 4300 decimal digits. The count includes leading zeros and
Unicode decimal digits, and excludes signs, underscores and outer whitespace.
4301-digit values, including signed zero, raise the complete observed limit
message; invalid strings retain their literal error. Native document mode keeps
its unbounded integer carrier and generic invalid-count message.

Public-mode PNML normalizes physical CR/CRLF before entity decoding; numeric
references to CR retain their value and distinct error representation. Marking
text joins comments and CDATA, while a child element or processing instruction
ends the first segment. Missing initial/final text and empty text retain their
observed separate behavior. Native empty text defaults are preserved: an initial
zero key and a final entry's default one token.

Two fresh public captures contain the original 82 fixtures, initial/final cases
for all 66 Unicode decimal blocks, invalid printable/nonprintable literals,
long literal truncation, 4299/4300/4301-digit boundaries, and XML text fragments.
The first 334-case capture and 350-case whitespace extension make
1368 imports, 8400 fitness
calls and 4200 full-replay calls; counts include the
repeated regression observations in both captures. The final 350 raw models
contain 210 successful and 140 rejected models, each through file/deserialize.

All 280 native import-error messages match.
All 420 successful marking/structural projections,
8176 complete typed four-field fitness
comparisons, 2044 six-field trace projections and
168 expected fitness/replay overflow checks match.
All 2100 native document/net pairs and corresponding
public direct/parent pairs agree. These are complete carrier comparisons within
the declared cohort, with no tolerated score differences or field stitching.

1750 CLI processes and 1750 cold-server
HTTP requests make 4088 matching whole public
fitness comparisons. All 1750 CLI/HTTP pairs and
3500 wire/native checks agree, including every error message.
The comparer removes only the process-ending newline, preserving trailing spaces
in truncated errors. No invalid-integer wording differences remain in this cohort.

All 152 assertion-enabled Release suites pass on 660 frozen inputs: two edits
and 658 unchanged. All 155 production/test artifacts are freshly linked.
[pnml-integer-errors-validation.json](pnml-integer-errors-validation.json) records
the bindings, complete raw calls/process/wire data and prior evidence.
All 9457 members round-trip across 15 lossless archives.
This is source/build/process/archive self-review; no independent review or
pre-public oracle is claimed.

Configured Python integer digit limits, exception class identities, arbitrary
PNML syntax/encoding/properties and marking insertion order remain unproven.
The original automatic-choice fitness cohort retains its prior 312 native and
312 wire whole-dictionary differences. AUTO/composite/precision/application
conformance, bounded consumers, aliases and broader carrier/parameter domains
remain open. Catalogue totals remain 46/86 packages, 199/387 modules and 228
incomplete units. No whole-module promotion or full parity claim is made.

### Exact precomputed token fitness through the compatibility carrier

`PrecomputedTokenReplay` and its `TokenReplayEvaluationResult` alias now feed the
same exact aggregate evaluator as full token results and trace diagnostics.
Descriptive counters are optional exact signed `TokenInteger` values; absent
values select the short native aliases. An explicit zero or negative descriptive
counter retains precedence. This changes the old zero-as-unspecified sentinel
and removes uint64 aggregate overflow and separate-double counter division.

Every supplied record contributes to the calculation. `computed=false` and error
metadata no longer remove a row, matching the public evaluator's treatment of
extra fields. Trace fitness outside [0,1], NaN, infinity and overflowing double
sums propagate through the observed arithmetic. Empty/nonpositive aggregate
denominators retain the public integer-zero log-fitness type. The result retains
the six-field trace diagnostics and marks its compatibility details as fitness
only, leaving unevaluated quality scores null.

Transition activations and exhausted flags remain native metadata. Representable
activation totals retain their uint64 detail vector; an unrepresentable optional
activation vector is omitted without preventing exact fitness. This bounded
metadata projection is separate from the public four-field dictionary.

Fresh pinned observations make 2614 actual direct/parent aggregate calls on 1307
inputs, with no fresh replay calls. The cohort reobserves the preceding signed,
large, zero-denominator and nonfinite fixtures and their retained weighted replay
projections, plus 64 cases with extra status/alias fields. The 5228 native calls
exercise diagnostics, full results, descriptive precomputed fields and explicit
native alias translations. All 10456 complete public comparisons match every
field, scalar type and float hex; all 7842 post-observation calculations agree.
All 5228 exact-counter, native input, input-translation and retained six-field
checks match. All 3921 native carrier pairs and 1307 public route pairs agree.
The aliases are a native translation extension, not a claim that Python accepts
short keys when descriptive keys are absent.

All 152 assertion-enabled Release suites pass on 660 frozen inputs: three edits
and 657 unchanged. All 155 production/test outputs are freshly linked. The first
comparer expected an untagged metric map; the production codec emits a tagged
dictionary. Its observed terminal failure and witnessed reproduction are retained
with the original helper. A derived comparer reads the actual tagged dictionary;
production sources, frozen inputs, probe and public results remain unchanged.

Evidence is in [precomputed-fitness-validation.json](precomputed-fitness-validation.json).
All 1027 members round-trip across 12 lossless
archives, with bindings to prior PNML and original weighted replay evidence.
This is source/build/process/archive self-review; no independent review or
pre-public oracle is claimed. CLI/HTTP transports do not expose the precomputed
API, so no fresh transport calls are claimed for this change.

Python dictionary omissions, non-Boolean fit flags, non-double fitness carriers,
arbitrary result objects and parameter/alias graphs remain unproven. The original
automatic-choice fitness cohort retains its prior 312 native and 312 wire whole
dictionary differences. AUTO/composite/precision/application conformance and
other bounded consumers remain open. Catalogue totals remain 46/86 packages,
199/387 modules and 228 incomplete units; no promotion or full parity claim is made.

### Exact markings for token precision, generalization and composite evaluation

Token precision, token generalization and complete composite evaluation now
accept exact signed marking options and sparse key/name metadata. Their PNML
document overloads inherit exact markings directly, removing the int-range gate
from composite evaluation. Legacy integer options retain their validation and
explicit exact options take precedence. Public-mode PNML import continues to
retain only positive imported marking entries; the default native document mode
is separate. AUTO/alignment precision and interactive bounded PNML adapters are
outside this change.

Token precision replays only proper prefixes. Initial enabled choices are
queried through hidden transitions without computing full-trace fitness. This
preserves the observed distinction for a 401-digit marking: empty-log and
one-event precision succeed, while nonempty generalization/composite evaluation
and precision that needs a nonempty proper prefix raise the observed float
conversion overflow. Precision compatibility JSON omits full-log token details
that it has not evaluated.

Actual public prefix-utility calls establish comma-joined keys, repeated splitting
for replay, merged colliding keys and preserved empty fields. The evaluator now
uses these keys in first-occurrence order and marks generated empty-string
activities as present. Initial choices are scored separately from a proper prefix
whose key happens to be empty. Distinct unknown labels remain distinct keys.

Generalization counts activations from the full forward token replay with
cleaning disabled, including activations in unfit traces. Composite evaluation
propagates exact markings to all component evaluators and preserves the complete
six-field public dictionary, including the whole four-field fitness dictionary.
Production codecs retain scalar types and float hex for precision, generalization
and the complete composite return, including integer-zero empty-log fitness.

Three complete fresh public captures make 445 imports, 17430 quality calls and
4980 separately observed full replay calls. The largest cohort contains all 65
preceding weighted models, all 82 PNML marking boundary inputs and two graphs
with no transitions. It covers eight log profiles, an alternate activity key,
distinct unknown prefixes, comma/empty labels, colliding prefix keys and three
composite weight sets. Sixteen actual public prefix-utility calls retain their
returned dictionaries, counts and generated logs. Whole composite observations
are captured directly, never reconstructed from separately observed components.

The final 149 native imports reproduce 125 named marking/structure projections
and all 24 rejection messages. All 10000 native document/net quality calls retain
their inputs. All 34646 complete public return comparisons and 214 expected
exception comparisons match every field, scalar type and float hex without
tolerance or field stitching. All 5000 native carrier pairs and 4980 public
scalar route pairs agree. All 1986 post-observation generalization activation
frequency checks agree with separately observed full replay. This does not claim
whole PNML object identity, property or insertion-order parity.

All 152 assertion-enabled Release suites pass on 660 frozen inputs: nine edits
and 651 unchanged. All 155 production/test outputs are freshly linked during this
checkpoint. The full build initially passes 151/152; the remaining legacy test
indexes full-log precision counts that are now deliberately omitted. Correcting
that assertion changes only one test file: all production and other 659 source
bindings remain identical. Its freshly compiled replacement is registered with
the same other 151 fresh binaries, and the complete 152-suite run passes. A separate
preflight recompiles the evaluator/contracts against a retained library whose
other source bindings agree; its 10000 calls and whole comparisons also pass.
The final comparison repeats those calls against the fresh complete build.

Three failed frozen build/test runs are retained: an old int-range rejection assertion,
then a generated empty-string event that lacked activity-presence metadata, and
the legacy precision-count indexing crash in the full run. A
named contract diagnostic retains the second exception. Two comparer failures
assumed empty compatibility metadata arrays were always emitted; derived readers
accept omitted arrays only at zero node count. The original native candidate's
350 delimiter-related whole-result differences and witnessed reproduction are
retained. One public observer failed to encode returned sets; its derived observer
reruns the full capture with an explicit set codec. Executed helpers and frozen
inputs remain unchanged.

Evidence is in [exact-model-quality-validation.json](exact-model-quality-validation.json).
All 4218 members round-trip across 78 lossless
archives, with bindings to the preceding precomputed checkpoint, original weighted
replay and PNML marking boundaries. This is source/build/process/archive
self-review; no independent review or pre-public oracle is claimed. No new
CLI/HTTP quality route or fresh transport parity is claimed.

The original automatic-choice fitness cohort retains its prior 312 native and
312 wire whole-dictionary differences. Wider choice/cache/model/log/property and
parameter graphs, Python scalar/exception identities, AUTO/alignment precision,
backward precision and application conformance remain unproven. Interactive
supplied-PNML adapters and other bounded consumers still have separate range
gates. Catalogue totals remain 46/86 packages, 199/387 modules and 228 incomplete
units; no promotion or full parity claim is made.


### Exact PNML evaluation through existing CLI and cached HTTP adapters

The interactive supplied-PNML importer retains arbitrary integer markings. The
existing CLI `--check-pnml` path accepts `--pnml-public-markings`; cached HTTP
`/api/check-model` accepts `pnml_public_markings` in query or multipart fields.
Public mode preserves the pinned importer's positive-only marking semantics,
including explicit empty final markings. Omitted/false mode retains native zero
keys and legacy unique-boundary inference. Invalid/empty boolean values fail
deterministically. Model-quality dispatch inherits markings only for the selected
family. Explicit nonempty caller vectors, including all-zero markings, win
independently for initial and final markings. Token fitness, explicit ET token
precision, token generalization and composite evaluation use the exact document
overloads; structural simplicity uses the document presentation. Bounded precision
and fitness variants keep their separate range checks.

The native diagnostic envelopes now also expose `public_result` for a single
computed supported family: the complete typed four-field token fitness dictionary,
token precision/generalization or arc-degree scalar, or complete six-field
composite dictionary with nested fitness. Float hex and integer zero retain their
distinct types and exact bits. This projection is not field stitching. Uncomputed
or unsupported variants have no public projection. Unsupported legacy conformance
bundles retain the model but publish null soundness, token replay and footprints;
they no longer publish placeholder scores as observations.

Two fresh pinned public captures make 151 actual imports, 11110 quality calls
and 2020 separate token replays. The main capture includes all 149 preceding
weighted/marking/empty-graph inputs, eight log profiles, four direct/parent
family pairs and three complete composite weight sets. The second captures
independent initial/final overrides on two large-marking documents. Logs, parameters,
markings and graphs are checked after calls; opaque public module proofs remain
identical before and after. Public calls return actual whole values and exceptions.

The accepted actual transport run uses the existing full CLI export and cached
HTTP `/api/mine` -> `/api/check-model` -> `/api/evaluate` flow, rather than
substituting the standalone fitness endpoint. It executes 5464 CLI processes,
6995 HTTP requests including 5320 cached evaluations, on all 149 PNML inputs
through six default mining profiles plus the five override profiles. All 16596
whole public-value comparisons, 412 error-message/status comparisons, 5277 whole
CLI/HTTP pairs, 3040 direct/parent public pairs, 1520 cached marking checks and
2149 unavailable-bundle checks pass with zero differences and no tolerance.
760 successful uploads retain their exact markings in cached PNML exports.
Thirty additional checks across 32 HTTP requests verify public/native modes,
strict flags, usable exact precision/simplicity after a 401-digit marking upload,
and the separate AUTO/alignment consumer gates. HTTP transports do not preserve
Python exception class identity, and no such identity parity is claimed.

Both empty-activity profiles remain outside the accepted default full-mining
transport cohort. An actual 63-process smoke run retains 35 differences: default
inductive mining rejects an empty activity name before cached evaluation. A
separate 63-process run with an explicit discovery selector retains 26 differences
and exposes a log-skeleton activity registration gate. These failures are not
reclassified as successful imported-model evaluations. The underlying exact
native token evaluators' prior eight-profile evidence remains separate.

All 152 assertion-enabled Release suites pass on 660 frozen inputs: 12 edits and
648 unchanged. All 155 accepted production/test outputs were freshly linked
during this checkpoint. The full producer passes 151/152; its remaining old
assertion expects import-time rejection above native int range. The correction
instead checks retained exact markings, truthful unavailable conformance and
bounded alignment rejection. Only that test differs from the producer's other
659 frozen inputs; its fresh replacement is run with the same other 151 fresh
binaries against the same fresh core, and the complete inventory passes. The
frontend build and 200 focused frontend tests pass with matching final frontend
source bytes.

The failed CLI parser build (MSVC nesting limit), obsolete range assertion,
initial frontend nullability diagnosis, local HTTP observer name collision and
prearchive schema error are retained under immutable helpers and frozen inputs.
Corrected observers/builds use fresh filenames and directories. Evidence is in
[pnml-cached-evaluation-validation.json](pnml-cached-evaluation-validation.json):
all 29502 members round-trip across 35 lossless
archives, with hashes binding all 78 preceding exact-model-quality archives.
This is source/build/process/archive self-review; no independent review is claimed.

Legacy conformance still gates compact zero/signed/large weights and markings
outside native int range. Empty-name default mining, wider application conformance,
AUTO/alignment precision, backward precision, Python scalar/exception identities,
choice/cache/model/log/property graphs and insertion order remain unproven. The
original automatic-choice fitness cohort still retains 312 native and 312 wire
whole-dictionary differences. Catalogue totals remain 46/86 packages, 199/387
modules and 228 incomplete units; no promotion or full parity claim is made.


### Present empty activities through default mining and cached evaluation

The inductive EventLog selector accepts a present empty activity value as a
visible activity. It retains the missing-field guard and uses the same projection
for the default and custom activity keys. IM, IMf and IMd keep the empty leaf
distinct from tau, including a visible middle leaf in a three-activity sequence.
Skeleton JSON validation checks graph registration for both endpoints and activity
frequency keys/deviations; an already registered empty label is valid. Unknown
activities and invalid conformance shapes retain their native validation.

A fresh pinned public capture makes 131 actual calls across 15 frozen fixtures:
empty logs/traces, single/repeated/optional empty labels, boundary positions,
delimiter/prefix collisions, spaces/Unicode, a custom phase selector, and separate
missing/None observations. It records the IM/IMf/IMd public tree fields and whole
direct/parent classic skeleton discovery and conformance values. All log, parameter
and model inputs are checked after calls; opaque pinned module proofs remain equal
before and after. The five actual missing-key exceptions and successful None-label
observations are retained, without claiming native Python exception or None-label
identity parity. No upstream implementation or private fields are inspected.

Thirty-six actual full CLI mining exports use 12 faithful default-key public logs
across the three inductive variants. All 24 whole typed skeleton-model and 48 whole
self-conformance projections match the direct/parent public values without
tolerance. Native relation/frequency vectors are projected to the public sets;
native double fitness is retained as exact float hex. This self-conformance cohort
contains no deviations. Native regressions separately exercise empty frequency and
both empty relation-endpoint deviation serialization, plus unregistered rejection.

Ordered tree field graphs match 27/36. The nine failures on delimiter/prefix/Unicode
fixtures remain explicit for all three variants. Their recursive graphs differ
only in XOR/parallel child order. An unordered-shape diagnosis does not erase the
ordered-field failures or establish Python object/class/property/insertion-order
parity. The actual failed observer that assumed activity objects carried an id is
retained; a new comparer uses their documented array positions against the same
frozen CLI output. The earlier public observer's wrong direct conformance callable
is likewise retained, with a corrected capture under a fresh helper and directory.

The restored existing-flow transport cohort uses all 149 supplied PNML models and
all eight default mining profiles, including both formerly rejected empty-name
profiles, plus five independent marking-override profiles on two large-marking
models. It executes 7262 full CLI processes and
9295 HTTP requests, including 7070
cached `/api/evaluate` calls after ordinary `/api/mine` and public-mode
`/api/check-model`. All 22060 whole typed
public-value comparisons pass with zero transport differences and no tolerance.
All 7015 complete CLI/HTTP pairs,
4040 public direct/parent pairs,
544 error-message/status
comparisons and 2020 cached marking checks pass.
All 1010 successful uploads retain their exact exported
initial/final markings. The preceding 151 public imports, 11110 quality calls and
2020 replays are reused through exact returned/input hashes and the 35 committed
parent archives; they are not described as fresh calls in this step.

All 152 assertion-enabled Release suites and both focused suites pass on 660 fresh
frozen inputs: four backend edits and 656 unchanged. All 155 production/test outputs
are freshly linked and bound with their compiler projects and process logs. The
frontend is unchanged; its preceding build/test evidence remains historical.
[empty-activity-mining-validation.json](empty-activity-mining-validation.json) binds
all 34967 members, round-tripped across
27 lossless archives, and the hashes of all 35 preceding
adapter archives. This is source/build/process/archive self-review; no independent
review is claimed.

The empty-name default-mining entry gates are closed for this cohort. Ordered
process-tree graphs, None/class/property identities, custom-selector transport
graphs, empty visible-label behavior in mined Petri-net conversion/replay/export,
and wider discovery/application conformance remain open. Legacy conformance and
AUTO/alignment consumers retain their separate native/compact marking limits.
The original automatic-choice fitness cohort still retains 312 native and 312
wire whole-dictionary differences. Catalogue totals remain 46/86 packages,
199/387 modules and 228 incomplete units; no promotion or full parity is claimed.


### Ordered inductive branches through native discovery and full export

Inductive XOR and parallel branches follow the stable ascending sum of all
descendant UTF-8 MD5 label digest integers, with tau contributing zero. Sequence
and loop child order retain their existing semantics. The key uses exact integers
for digest and subtree sums, including sums above 128 bits. Existing folding is
retained. This rule is inferred from public constructor, `tree_sort` and mining
return values; no upstream implementation, private fields or bytecode are read.
The portable digest follows [RFC 1321](https://www.rfc-editor.org/rfc/rfc1321).

Five fresh pinned public captures make 6701 actual calls. The
final capture contains 1678 calls: 90 mining fixtures across
IM/IMf/IMd and 1408 explicit sorting controls. Controls
cover nested operators, equal keys, duplicate labels, tau, empty labels, spaces,
delimiters, Unicode and UTF-8 padding boundaries. All 1408
complete before/after control graphs and 797
actual mining branch lists agree with the inferred key. Sequence/loop controls
retain their order and existing public parents/object IDs. Mining log/parameter
inputs remain equal after calls and opaque module proofs remain pinned before and
after. Public IDs and str/repr/hash values are diagnostics, not native Python
identity claims.

Actual native-library probes make 525 calls. The final
270 calls faithfully construct all 90 EventLog fixtures,
including custom activity selectors and exact legacy core inputs with caller noise
thresholds. Native log and parameters remain unchanged. Whole operator/typed
label/ordered children/parent graphs match 264/267
successful public returns. The three retained differences are actual public None
labels versus native empty strings. Three missing-key calls reject on both sides;
public KeyError identity/message differs from native invalid_argument. Those gaps
remain explicit. The original generated observer fails with Windows
STATUS_STACK_OVERFLOW before output; a fresh helper scopes each fixture factory
separately and retains that failed process, source and binary.

The preceding nine ordered-tree failures are closed for all three variants in
both direct native calls and full CLI exports. All 36 ordered CLI field graphs,
24 whole typed skeleton models and 48 whole self-conformance projections match
without tolerance. Native relation/frequency vectors are projected to public sets
and floating fitness retains exact hex. The supplied-model CLI/cached HTTP cohort
again executes 7262 CLI processes, 9295 HTTP requests
and 7070 cached evaluations across all 149 PNML models,
all eight default mining profiles and five marking-override profiles on two large
models. All 22060 whole public comparisons,
7015 CLI/HTTP pairs, 4040 public
direct/parent pairs, 544
error-message/status comparisons and 2020 cached marking
checks pass. All 1010 uploads preserve exported exact markings.
The prior 151 public imports, 11110 quality calls and 2020 replays are reused through
exact hashes bound by the preceding committed archives; they are not fresh public
quality calls in this step.

All 152 assertion-enabled Release suites and both focused suites pass on 660 frozen
inputs: three backend/source-test edits and 657 unchanged. Production core,
CLI/server and 151 suites are freshly built in the retained producer. Its original
focused/full runs stop at an obsolete core ordering assertion; nine expectations
are corrected only after capturing their exact public input results. The corrected
core test is freshly compiled against the unchanged production core in a new
frozen build, then all 152 suites pass together. The failed runs and all 155 accepted
outputs are bound with their compiler projects, sources and process logs.
[inductive-ordering-validation.json](inductive-ordering-validation.json) binds all
35165 members, round-tripped across 32 lossless
archives, plus hashes of the 27 preceding mining archives. This is source/build/
process/archive self-review; no independent review is claimed.

None labels, Python exception/class/property/identity semantics, arbitrary
constructed-tree parity, custom-selector application transport graphs, empty
visible-label behavior in mined Petri-net conversion/replay/export and wider
discovery/application conformance remain open. AUTO/alignment/backward consumers
retain their independent native/compact marking limits. The original automatic
fitness cohort retains 312 native and 312 wire whole-dictionary differences.
Catalogue totals remain 46/86 packages, 199/387 modules and 228 incomplete units;
no promotion or full parity is claimed.


### Nullable activities through native inductive selection and ordinary entry points

The EventLog activity selector retains a present null as kNull and copies the
actual selected attribute metadata for both default and custom keys. Missing and
empty-string fields remain distinct. Selecting a generic null no longer falls
through as a missing field, or overwrites that presence state with a present empty
string. None-only singleton events, optionally mixed with empty traces, return a
silent root with no fabricated visible label. Mixed None/string alphabets and
repeated None events reject, as observed through the pinned public miner.

A fresh public capture makes 534 actual IM/IMf/IMd calls on 178 frozen fixtures:
the preceding 90 ordered-tree/core fixtures, 54 controlled default/custom-key
nullable cases, 32 seeded logs and two additional missing-key cases. All 303
successful tree returns and 231 exceptions are retained with full public fields,
typed labels, ordered children and parents. All log/parameter inputs remain equal
after calls and opaque module proofs remain pinned before and after. The cohort
contains 288 string/empty-log successes, 15 None-only singleton successes, nine
missing-key errors, 12 repeated-None errors and 210 mixed None/string errors.
The first-observed-type hypothesis for exact mixed error-message operand order
has 63 actual counterexamples; those remain preserved. The revised acceptance
rule records mixed message orientation as unpredicted, without normalizing away
the earlier failures or inspecting upstream source, private fields or bytecode.

Actual fresh native-library probes make 534 calls before and 534 after the change.
The baseline contains 117 acceptance differences and nine ordered tree-field
differences. The corrected source matches all 534 accept/reject decisions and all
303 whole successful operator/typed label/ordered children/parent graphs, including
the prior three None-label failures. Native log and parameters remain unchanged.
Native regressions cover selected kNull/attribute metadata, independent default
and custom fields, optional null singleton traces, repeated/mixed rejection,
empty visible labels and EventDataView(EventLog). The 231 native exception classes
remain distinct from their Python classes, and 219 exact messages remain different;
the 12 repeated-None messages match. No Python identity parity is inferred.

Ordinary application observations execute 534 full CLI exports and 534 HTTP
`/api/mine` requests on the same faithful raw inputs and selected key/noise/variant
options. The native XES codec's explicit `null="true"` marker carries the None
values; this is a native transport representation, not a public XES importer
claim. Each transport matches 525/534 acceptance decisions and all 294 returned
ordered tree graphs. Nine successful public calls per transport remain blocked on
default None singleton/optional fixtures 14, 90 and 94: minimum-self-distance
analysis reports a missing activity before inductive discovery. All 18 failures
remain actual entry gates, rather than being omitted or counted as successful
mining. Custom-key None silent trees export successfully. Of the 231 rejections
per transport, six repeated-None messages match and 225 messages differ; all 450
application message differences remain retained. No unrelated bundled model-family
values are equated with their public counterparts.

Established string-log regressions still match all 36 ordered CLI tree graphs,
24 whole typed skeleton models and 48 whole self-conformance projections. Fresh
supplied-model smoke executes 84 CLI processes and 112 HTTP requests, including
84 cached evaluations on three models and four default mining profiles. All 214
whole public values, 67 CLI/HTTP pairs, 48 public direct/parent pairs, 50 expected
error-message/status checks, 24 exact cached marking checks and 12 exported
markings match. The preceding public PNML imports/quality/replays are reused
through exact hashes in the committed parent archives. The parent's 22060 full
transport comparisons remain historical for that source; they are not described
as fresh coverage of this change.

All 152 assertion-enabled Release suites and both focused suites pass on 660
frozen inputs: two backend/source-test edits and 658 unchanged. All 155 production
and test outputs are freshly built and bound with compiler projects and process
logs. [nullable-inductive-validation.json](nullable-inductive-validation.json)
binds all 4034 members, round-tripped across 19
lossless archives, plus hashes of the 32 preceding ordering archives. This is
source/build/process/archive self-review; no independent review is claimed.

Default None application entry, exact exception identities/messages, nullable
stream/table/lazy inputs, generic scalar labels, native alias/trace-fallback
selectors, Python classes/properties/hash/repr/identities and insertion order,
None/empty visible-label Petri-net conversion/replay/export and wider conformance
remain open. AUTO/alignment/backward consumers retain independent native/compact
marking limits. The original automatic-choice fitness cohort retains 312 native
and 312 wire whole-dictionary differences. Catalogue totals remain 46/86 packages,
199/387 modules and 228 incomplete units; no promotion or full parity is claimed.


### Nullable minimum self-distance and supplied witness baseline

The ordinary mining gate from the nullable inductive checkpoint is now measured against actual pinned minimum-self-distance APIs, without changing production source. Fresh public calls cover all preceding 178 EventLog fixtures plus 30 controlled default/custom-selector logs, for 208 fixtures, including 107 with selected `None` values. The direct LOG variant and parent LOG dispatcher execute 416 calls, automatic witness derivation executes 208 calls, and five explicit supplied-map profiles execute 1,040 calls. A separate capture executes 205 witness calls with the exact successful preceding public distance maps; the three missing-key distance failures have no computed map. Total fresh public calls are **1,869**. Four modules have matching opaque pinned before/after SHA proofs; all log, parameter and supplied-map snapshots are unchanged.

Public distances distinguish `None`, `""`, `"None"` and `"null"`. Null singleton/optional logs return an empty distance map; repeated null labels can be real keys, and intervening null values can be real witness members. The native direct and LOG-registry routes each execute all 208 fixtures: each matches 98 complete successful string-only distance maps, retains three missing-key class/message differences, and incorrectly rejects all 107 nullable fixtures. These are measured acceptance gaps; a null label is not treated as an absent field in the public calls.

Public automatic `derive_msd_witnesses` raises `builtins.AttributeError`, with message `'dict' object has no attribute 'apply'`, on **all 208 tested logs**, including empty and missing-key logs. The 208 actual native automatic calls instead return on 98 fixtures and reject on 110 with different classes/messages. All outcomes remain evidence. No implementation, private state or bytecode was inspected to infer the cause of the public exception.

The native distance result embeds witnesses. Pairing those fields from the 208 actual direct discovery calls with 205 separate actual cached-map public utility calls yields 98 successful whole typed mapping comparisons: **63 matches and 35 differences**, plus 107 nullable rejection gaps. The 35 differences retain extra native zero-distance witness entries that the public utility omits. These field comparisons are not counted as additional native calls. Whole dict/set value comparisons retain every typed key, distance and witness member; dictionary insertion order, Python object/container identity versus the native vector/map representation and exception class identity are separate gates.

A first public witness hypothesis initialized every observed activity and failed **1,064** of the 1,869 complete observations. It is preserved unchanged. A separately named hypothesis restricts output keys to observed activities with a supplied strictly positive distance, including empty sets when no segment matches, and unions the intervening values of matching consecutive-occurrence gaps. Together with the observed automatic exception and minimum-distance rule, it matches all 1,869 observed outcomes. This is a finite inferred policy, not a production change or a universal parity claim. The first baseline also incorrectly grouped the empty supplied-map profile with the None-key profiles; the corrected record retains that original bookkeeping and distinguishes **832** public calls with a None-key native carrier gap from **208** empty-map calls that have a faithful carrier but have not yet been measured natively.

The native probe executes **624 actual calls** across direct discovery, LOG registry and automatic witnesses; all three configure/build/run commands exit zero and assert input/option preservation. It links the exact accepted nullable-inductive core, checks all **660 unchanged production/test inputs**, and binds the same **155 outputs**. The preceding **152-suite** result is reused for this unchanged source, not presented as a new full build. No fresh CLI/HTTP mining cohort was run here: the preceding **18 application entry gates** remain open. Table/stream/lazy inputs, broader typed scalars, selector aliases, supplied signed/nullable-map carriers, Python ordering/identities, nullable model conversion/replay/export and wider application conformance remain open. Coverage remains 46/86 packages, 199/387 modules and 228 incomplete units; no promotion or independent review is claimed.

Lossless evidence is listed in `docs/nullable-msd-baseline-validation.json`: public signatures/calls/maps and snapshots, complete native outcomes, both hypotheses, probe/compiler inputs and output, accepted parent source/output bindings and post-push proof, and owned immutable helpers. Every new archive member round-trips to its recorded SHA-256. This checkpoint records a verified baseline and does not close the consumer gates.
