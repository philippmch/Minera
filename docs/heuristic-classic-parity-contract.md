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
| Non-finite returned performance values | `heuristic-classic-order-nonfinite-native-validation.json`; bound supplemental public controls | All five controls match actual native maps and edge/relation values with exact input/options fidelity. JSON rejection remains a separate transport policy. |
| Direct DFG insertion order | `heuristic-classic-order-nonfinite-native-validation.json`; bound supplemental public controls | All four public controls match using optional `dfg_order`. Registered tests additionally cover omitted map order, invalid permutations, empty inputs and explicit boundary overrides. |
| DFG count arithmetic and inference | `heuristic-count-ratios-validation.json`; `heuristic-inferred-counts-validation.json` | Specialized cohorts cover large ratios and inferred occurrence counts. |
| EventLog graph, AND, matrices, aggregation and selectors | `reference-2.7.22-parity.md`; the `heuristic-*-validation.json` artifacts referenced there | Existing finite cohorts cover defaults, thresholds, loops, AND measures, duplicate handling, metadata, performance aggregation, timestamp selection, business hours and arithmetic. |
| Missing selected start, typed NaT and input order | `heuristic-classic-flat-native-validation.json` and its archived checks | All 26 paired/control observations pass exact input, selector and represented-result checks. Missing-key exceptions use the documented native diagnostic mapping. |
| Returned node boundary flags and named sojourn map | `heuristic-classic-object-native-validation.json`; its archived checks and bound public captures | Actual model and serialized fields match 26 flat observations and five direct DFG controls, including fallback nodes and explicit list boundaries with performance data. |
| Omitted versus explicitly empty raw DFG auxiliary maps | `heuristic-classic-auxiliary-native-validation.json`; its archived checks and bound public controls | Three direct controls and 26 flat observations match. Native supplied/effective presence flags produce actual nullable public fields, preserving existing array fields. |
| Petri-returning `classic.apply` / `classic.apply_dfg` | `heuristic-classic-petri-bridge-controls-validation.json`; its archived checks and bound public references | All 18 primary cases and four additive route observations match the current converter. Three inputless CLASSIC/PLUSPLUS controls also match; their existing direct-input serializer explicitly leaves `dfg_order` unobserved. |
| Pure XOR/AND Petri construction | `heuristic-classic-petri-bridge-controls-validation.json`; `heuristic-classic-petri-diamonds-reference.json.gz` | All four additive Petri/HeuristicsNet route checks match the current converter, including the XOR/AND structural distinction and AND measures of 0.8. |
| Small-DFG Petri topology and thresholds | `heuristic-classic-petri-small-dfg-native-validation.json`; its complete native and public captures | All 1,779 Petri cases match. The twelve threshold-zero topology failures are resolved, with zero native errors and exact unchanged input/options checks throughout. |
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

The native node boundary flags remain distinct from internal start/end activity
membership: the public captures return false flags even when boundary counts
are nonzero. CLASSIC now stores and serializes those actual flags and an empty
named sojourn map. The registered `classic_object_fields_acceptance()` case
also preserves nonzero internal boundary counts and checks that PLUSPLUS omits
these new optional fields. Six affected native suites and independent review
pass. The auxiliary-presence checkpoint additionally checks omitted direct maps
as `null`, explicitly empty direct maps as `[]`, and EventLog-computed empty
maps as `[]`. Legacy nonempty map assignments imply presence; PLUSPLUS keeps
its prior JSON shape. These checks do not settle Petri conversion behavior.

For Petri outputs, retain actual place/transition names and properties in the
reference capture. A structural comparison must preserve place/transition
types, transition labels (including silent transitions), weighted directed
arcs, and initial/final token counts. State explicitly which generated names
are excluded from semantic comparison. A HeuristicsNet match alone is not
evidence that its Petri conversion matches.

The initial Petri cohort preserves all 18 inputs and parameter maps unchanged.
Its six DFG cases come from the existing input catalogue, which does not contain
a pure diamond/AND-split fixture. Two additive controls close that public-capture
gap: XOR returns 4 places, 4 transitions and 8 arcs, while the input with
cross-branch evidence returns 6 places, 4 transitions and 10 arcs and AND
measures of 0.8. All four additive route calls preserve their arguments.
Capture and fixture-helper sources are embedded with hash bindings in the
reference archives. The initial native baseline matched both diamond inputs
through both routes and retained one mismatch among the 18 primary cases. Its DFG is
`A->B:1, B->A:2` with defaults and omitted boundary metadata. Both results keep
the visible A/B transitions and disconnected source/sink markings, but native
conversion adds two private input places; the reference leaves both transition
inputs unwired. The baseline preserves this failure and does not establish
complete Petri-route parity.

The corrected baseline (`heuristic-classic-petri-corrected-baseline-validation.json`)
retains that same 21/22 result while enforcing an exact unique case inventory,
stable executable hashes, fixed-offset timestamp metadata derived from the
captured ISO payloads, and available empty reference property maps. Generated
names remain outside structural comparison; unsupported nonempty properties
are reported rather than ignored.

`heuristic-petri-inputless-reference.json.gz` adds direct CLASSIC EventLog and
PLUSPLUS EventLog controls using the UTC trace `S,A,B,A,E` at dependency threshold
1. Both public Petri results leave A/B inputs unwired, retain private output
places, and preserve source/final markings. The authoritative PLUSPLUS capture
checks the public enum's identity against the pinned variant module. This
establishes the intended behavior for the converter repair. The native repair
now leaves empty joining alternatives unwired while retaining private output
places. `heuristic-petri-inputless-native-validation.json` records three passing
native suites, all 22 archived route observations matching, and all three
inputless controls matching exact inputs, options, topology and markings.
These are 25 observations, not 25 distinct inputs: the direct DFG control repeats
an earlier input. Independent review found no actionable patch or comparator
defects. This checkpoint preceded the broader small-DFG comparison below; the
Petri comparison does not claim full HeuristicsNet field parity.

The broader native baseline preserves every observation and all twelve topology
failures. Each failing three-activity, threshold-zero result lacks one place,
one silent transition and two arcs relative to its public result. All 1,779
input/options checks pass, with zero native errors and no global comparison
failures. Complete include inventories and contents match the immutable build
tree; supplemental verification closes the executed runner's originally limited
header gate, and the future runner enforces that gate directly. These failures
established the remaining Petri topology obligation; the selected HeuristicsNet
diagnostic matching all cases does not establish full object parity.

`heuristic-classic-petri-small-dfg-native-validation.json` records the bridge
repair and all 1,779 matching Petri outputs. The series-place reducer now has a
default-false `preserve_join_bridges` option. HeuristicsNet conversion enables it
to retain a unary silent bridge whose input has multiple ordinary producers and
whose output also has an external ordinary producer. The generic reducer keeps
its existing defaults. Registered tests prove both default contraction and the
narrow preservation condition, exact case 126 wiring and disconnected markings,
and unchanged neighboring cases 125/127. All four affected Release suites pass.

The current build also matches all 22 prior route observations and all three
inputless controls. Complete private include inventories and contents, source,
core, executable, fixture, runner and log hashes stay bound throughout capture.
The 22-route and 1,779-case probes observe actual `dfg_order`; its omission in the
older three-control input serializer remains explicit. Independent review found
no actionable implementation, regression or comparison defects. Generated names
remain outside structural isomorphism and public properties must be available
and empty. The complete module gate still requires the distinct catalogue audit.

## Scope boundaries and completion

Parent dispatcher input adaptation and PLUSPLUS discovery are separate
catalogue obligations. Their partial status is not erased by completing
CLASSIC. Conversely, the CLASSIC module cannot be promoted merely because
the parent flat-input checkpoint passes.

Python object/class identity, exact Python exception classes, opaque Python
workcalendar objects, arbitrary Python object labels and unsupported non-string
DFG keys are outside the native representation. These limits must remain
explicit; they do not excuse differences within supported native inputs.

The older direct-DFG checkpoint's sorted-map input limitation does not settle
insertion-order parity. Supplemental public controls show that reversing
`A->B:1, Z->Y:1` reverses inferred starts from `[A,Z]` to `[Z,A]` and ends
from `[B,Y]` to `[Y,B]`. The native input now carries that order through optional
`dfg_order`, requiring an exact permutation of the supplied DFG keys. Existing
map callers retain their established default behavior and explicit boundaries
retain precedence. The native checkpoint matches these four controls and all
five non-finite/string controls, with direct returned-object comparison even
though the JSON exporter separately rejects non-finite numeric payloads. Three
affected Release suites, the isolated Debug variant checks and independent
review pass. The broader Petri comparison subsequently resolves all twelve
topology failures without changing these object results.

Before promotion, reconcile every route and returned-object gate above with
saved public observations, actual native output, source/binary bindings and
appropriate registered tests. Run the structural catalogue verifier after
adding the distinct proof. Do not substitute coverage counters or a passing
partial comparator for this audit.
