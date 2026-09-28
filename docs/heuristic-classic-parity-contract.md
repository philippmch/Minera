# CLASSIC heuristic discovery parity contract

Target: PM4Py 2.7.22, pinned commit
`0715c31f3f1d85924107d6aa5329806db69df899`.
The module `algo.discovery.heuristics.variants.classic` remains partial.
This contract records the module's remaining acceptance gates without reducing
its scope to the already verified HeuristicsNet routes.

## Routes and evidence

| Requirement | Authoritative evidence | Current conclusion |
| --- | --- | --- |
| Direct `classic.apply_heu(EventLog)` | `heuristic-classic-direct-reference.json.gz`; `heuristic-classic-flat-native-checks.json.gz` | Twelve direct public observations match the paired dispatcher inputs and outputs; the native comparison checks their represented fields without counting them as additional unique inputs. |
| Direct `classic.apply_heu_dfg` discovery | `heuristic-dfg-input-reference.json.gz`; `heuristic-dfg-input-validation.json` | 1,350 finite cases cover declared/inferred activities, occurrences, boundaries, thresholds, noise, windows/triples and accepted graph behavior. |
| DFG decorated values and loop membership | `heuristic-dfg-values-validation.json`; `heuristic-loop-membership-validation.json` | Specialized cohorts cover supplied performance maps and loop-membership behavior. |
| DFG count arithmetic and inference | `heuristic-count-ratios-validation.json`; `heuristic-inferred-counts-validation.json` | Specialized cohorts cover large ratios and inferred occurrence counts. |
| EventLog graph, AND, matrices, aggregation and selectors | `reference-2.7.22-parity.md`; the `heuristic-*-validation.json` artifacts referenced there | Existing finite cohorts cover defaults, thresholds, loops, AND measures, duplicate handling, metadata, performance aggregation, timestamp selection, business hours and arithmetic. |
| Missing selected start, typed NaT and input order | `heuristic-classic-flat-native-validation.json` and its archived checks | All 26 paired/control observations pass exact input, selector and represented-result checks. Missing-key exceptions use the documented native diagnostic mapping. |
| Returned node boundary flags and named sojourn map | `heuristic-classic-direct-reference.json.gz`; `heuristic-classic-dfg-boundaries-reference.json.gz` | Public observations are available; the native model/serializer and explicit comparisons remain incomplete. |
| Petri-returning `classic.apply` / `classic.apply_dfg` | No completed direct-route native/reference comparison yet | Verify the pinned public callables, preserve actual returned nets and markings, then compare native output. Native-only conversion tests do not close this gate. |
| Distinct catalogue proof | `reference-2.7.22-coverage.json` CLASSIC module row | Add an exact implementation and registered acceptance-test proof only after all module gates are verified. |

## Required returned-object checks

The represented-field comparator checks activity identity, active nodes,
occurrence and boundary metadata, raw DFGs, dependency/performance matrices,
window-two/triple structures, accepted edges, node occurrence statistics,
loop-two and AND fields, and concurrency values for its bounded fixtures.
It reads actual native node statistics instead of synthesizing them from
metadata, compares numeric values exactly, and validates typed timestamp input
payloads separately from lexical values. Empty structures are checked against
actual native collections rather than assumed to be empty.

The still-missing node boundary flags must remain distinct from internal
start/end activity membership: the public captures return false flags even
when boundary counts are nonzero. Likewise, a named public sojourn map is not
equivalent to a per-activity scalar initialized to zero. Add real model fields
and serialize them before claiming those output fields match.

For Petri outputs, retain actual place/transition names and properties in the
reference capture. A structural comparison must preserve place/transition
types, transition labels (including silent transitions), weighted directed
arcs, and initial/final token counts. State explicitly which generated names
are excluded from semantic comparison. A HeuristicsNet match alone is not
evidence that its Petri conversion matches.

## Scope boundaries and completion

Parent dispatcher input adaptation and PLUSPLUS discovery are separate
catalogue obligations. Their partial status is not erased by completing
CLASSIC. Conversely, the CLASSIC module cannot be promoted merely because
the parent flat-input checkpoint passes.

Python object/class identity, exact Python exception classes, opaque Python
workcalendar objects, arbitrary Python object labels and unsupported non-string
DFG keys are outside the native representation. These limits must remain
explicit; they do not excuse differences within supported native inputs.

Before promotion, reconcile every route and returned-object gate above with
saved public observations, actual native output, source/binary bindings and
appropriate registered tests. Run the structural catalogue verifier after
adding the distinct proof. Do not substitute coverage counters or a passing
partial comparator for this audit.
