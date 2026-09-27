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
