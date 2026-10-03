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
