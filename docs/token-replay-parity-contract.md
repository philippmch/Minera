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
