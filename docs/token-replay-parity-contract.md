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
