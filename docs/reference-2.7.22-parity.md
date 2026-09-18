# Reference 2.7.22 parity contract

Minera targets behavioral feature parity with the **2.7.22 reference release**, tag commit
`0715c31f3f1d85924107d6aa5329806db69df899`. The target is pinned so “all
variations” is finite, reviewable, and testable. Moving the target requires a
separate catalogue diff and must not silently reopen completed rows.

The upstream open-source implementation is AGPL-3.0. Minera therefore uses public
documentation, published papers, independently written implementations, and
canonicalized black-box fixtures. Upstream source may be inventoried for names and
dispatch structure, but code is not copied into Minera.

Run the reproducible catalogue audit against a checkout containing the pinned
Git object database:

```powershell
node scripts/audit-reference-catalog.mjs C:\path\to\reference-2.7.22
```

The audit resolves `HEAD`, verifies the `2.7.22` tag and pinned commit, and
reads the tree and source text with `git ls-tree`/`git show`. It never walks or
reads package files from the checkout working tree, so staged deletions,
untracked replacements, and local edits cannot change the inventory. A
deliberately different test repository may use `--expected-commit` together
with `--expected-tag`; release audits use the defaults above.

The checked-in package/selector mapping is
`docs/reference-2.7.22-coverage.json`. Verify that it still covers the exact
pinned catalogue with:

```powershell
node scripts/verify-reference-parity.mjs C:\path\to\reference-2.7.22
```

The verifier covers every tracked non-`__init__` Python module under `algo`,
classifying each as a dispatcher, implementation variation, or support module.
It rejects target drift, missing or stale packages, selector-list drift,
missing or ambiguous module ownership, invalid statuses, and missing evidence
or artifacts. Each selector has an exact static binding to one or more tracked
target modules, including imported-member and conditional fallback targets.
Every one of the 387 non-dispatcher modules has its own machine-readable
coverage record, owner, classification, selector references, status, and
evidence path. A complete dispatcher carries per-selector proofs tied to its
exact bindings; a complete non-dispatcher row carries a distinct module proof.
Both proof forms name tracked artifacts owned by the row. Implementation proof
symbols must occur as code identifiers outside comments and string literals;
test proofs must name a case built by a registered CMake test executable, and
that case must exercise the implementation symbol. This normal check is a
structural coverage precondition; it is not a claim that rows marked partial,
adapter, integration, or missing are complete. The current manifest records 44
complete and 42 partial dispatcher rows, plus 195 complete and 192 partial
non-dispatcher module rows. The completed module group covers the implemented
simulation, core transformation, Petri-analysis, organizational, statistical
DFG, causal and correlation discovery, 26-module Inductive discovery, 31-module
POWL discovery, 13-module footprints discovery/conformance, and the 19-module
temporal/behavioral adapter stack, plus discounted anti-/multi-alignment and
classic Declare/log-skeleton discovery and conformance, with distinct
registered tests, plus the replay-duration diagnostic support module;
234 coverage units still require exact proof.
A release may
additionally run the strict gate:

```powershell
node scripts/verify-reference-parity.mjs C:\path\to\reference-2.7.22 --require-complete
```

`--require-complete` is an evidence-completeness gate. It fails until every
dispatcher coverage unit and every non-dispatcher module record is marked
`complete`, has tracked regular-file artifacts, and has exact implementation
and registered-test proof objects. A supplemental prefix assigns ownership only;
it cannot attest for the modules beneath it. Module proof identities may not be
reused as a single owner-level substitute for distinct modules. The command
does not execute behavioral proof suites and therefore cannot establish
semantic parity by itself; semantic claims still require the relevant Release
CTest, frontend, and focused behavioral suites to be run separately. Support
modules remain separately owned and audited, but are not counted as additional
public algorithm variations.

At the pinned tag the source catalogue contains 18 algorithm domains, 86
dispatcher (`algorithm.py`) packages, 182 implementation files under
`variants` trees, 205 support modules, 473 non-`__init__` modules overall, 779
Python files under `algo`, 348 under `objects`, 156 under `statistics`, 142
under `visualization`, 54 under `streaming`, and 40 under `util`.

## What parity means

Status values are:

- **Complete**: equivalent supported inputs, output invariants, important
  parameters, diagnostics, edge cases, deterministic fixtures, and a public
  Minera entry point.
- **Partial**: a useful equivalent exists, but at least one named behavioral
  variant, parameter family, diagnostic, or model conversion is absent.
- **Missing**: no public behavioral equivalent exists.
- **Adapter**: The reference implementation has separate EventLog/pandas/polars implementations of the
  same semantics. One native C++ implementation is sufficient; these do not
  count as separate process-mining algorithms.
- **Integration**: backend, renderer, connector, or optional ML/LLM surface.
  It is tracked, but not allowed to inflate the core algorithm count.

Python symbol-for-symbol API compatibility is not a goal. Semantic parity is:
the same process-mining question, equivalent supported data, canonicalized
model/output behavior, the same quality metric or diagnostic meaning, and
documented differences where multiple mathematically valid outputs exist.

Adjacent path filtering is tracked as an integration surface: predecessor and
successor adjacency, optional inclusive elapsed-time bounds, and positive or
negative polarity are implemented natively. Strict request parsing, CLI flags,
local-server fields, and compact frontend controls/serialization apply the
operation after ordinary log filters and segment extraction and before LTL.

## Algorithm-family checklist

This is the human-owned acceptance matrix. The audit script is the
machine-owned list of exact modules; a release may not claim parity unless both
are reconciled.

| Domain | Reference families and behavioral variants | Minera status |
|---|---|---|
| Analysis | marking equation (classic), extended marking equation (classic), workflow-net check, Woflan soundness with reachability/coverability/invariants | **Complete** — native marking/extended-equation/workflow/soundness cores expose the classic continuous state-equation optimum (including fractional solutions) with realizability recovery only for integral vectors, deterministic structural pair diagnostics, explicit Woflan selection, short-circuit construction, reachability liveness projection, strong-connectivity diagnostics, single-place marking validation, early unsound-witness return, and strict CLI/server/frontend adapters; focused contracts independently prove every selector and graph, pair, and invariant support projection |
| Anonymization | PRIPEL; trace-variant query Laplace and SaCoFa | **Partial** — bounded PRIPEL-style release plus both trace-variant query variants are exposed through native, JSON, CLI, local-server, and frontend adapters; deterministic seeded mode and opt-in OS-backed secure randomness are available, while differential edge-contract review remains |
| Clustering | `SKLEARN_PROFILES` profile clustering; trace-attribute-driven activity/successor/log-slice/similarity distances | **Partial** — the explicit `SKLEARN_PROFILES` selector and deterministic seeded profile clustering have bounded native/JSON/CLI/server/frontend adapters with activity/succession and explicit trace/event attribute representations; trace-attribute groups with DMM/average Levenshtein, vector, and DFG distances retain first-seen group/variant ordering, independently match DMM activity and succession components, omit missing grouping attributes, and preserve linkage child order across the same adapters; broader differential review remains |
| Comparison | Petri-net element-usage comparison | **Partial (remaining replay proof)** — native place/transition/arc usage deltas, supplied-PNML CLI input, explicit markings including zero-valued key metadata, selectors, weighted/extended arcs, color helpers, and token-flood cleaning have registered coverage; partial hidden routes and replay limits have public evidence, while resource-unresolved models remain outside the comparison proof |
| Concept drift | BOSE relation-type feature extraction and permutation-window drift detection | **Partial (implemented core; proof pending)** — the explicit `BOSE` selector covers relation-count features, seeded permutation windows, ranked/capped drift boundaries, boundary-partitioned result logs, optional timestamp diagnostics, strict options, and JSON/CLI/server/frontend adapters |
| Conformance: Petri alignments | state-equation A*, Dijkstra no heuristics, Dijkstra less memory, discounted A*, generator Dijkstra no heuristics, generator Dijkstra less memory | **Partial (implemented core; proof pending)** — all six variants are explicitly selectable; less-memory Dijkstra with legacy unit costs is the generic default, with named event activity fields available at replay time |
| Conformance: other alignments | decomposed recomposition, DFG classic, edit distance, process-tree search graph/dynamic programming/MILP/approximated original | **Partial (implemented core; proof pending)** — `RECOMPOS_MAXIMAL`, DFG classic, edit distance (`EDIT_DISTANCE`), and all four process-tree variants are native selectable APIs with JSON, CLI, server, and frontend adapters; strict activity selectors apply to every family and DFG/edit projections additionally expose timestamp selectors |
| Conformance: replay | forward token replay and backwards token replay | **Partial (duration support complete; replay proof pending)** — forward replay remains the default; grouped/stream/eager/lazy replay preserves null activities, typed case identity, selector precedence, adapter-specific case ordering, and event caps; both variations expose per-case counters, ordered silent/visible firings, event-level problems, unknown-event positions, reached markings, and enabled-transition IDs; duration cohorts have independent support proofs and native/JSON/CLI/server/frontend adapters, while exact routing/flood-cleaning and root-cause support remain pending |
| Conformance: relational | footprints log-model/log-extensive/trace-extensive; log skeleton; Declare | **Complete** — all three footprint selectors have distinct production components and registered contracts, including strict/non-strict relations, extensive diagnostics, tree-node decoration, and evaluation utilities; classic log-skeleton and Declare conformance add trace/log/flat adapters, strict activity and case selectors, exact empty/subset family-selection behavior, typed per-case diagnostics, and distinct registered proofs |
| Conformance: temporal | temporal profile log/dataframe adapters, zeta threshold, business-hour slots | **Complete** — log and dataframe contracts preserve one result bucket per case, apply strict configurable selectors and business-hour elapsed time, handle zero-variance profiles deterministically, and expose grouped diagnostics through native JSON plus the cached local conformance endpoint and typed frontend adapter |
| Conformance: advanced | Petri-net anti-alignments and multi-alignments (discounted A*) | **Complete** — both `VERSION_DISCOUNTED_A_STAR` algorithms preserve the asymmetric discounted recurrence, complete model-transition runs, raw ordinary-distance borders, deterministic longer-run ties, exact marking-limit boundaries, objectives, precision/distance diagnostics, arbitrary markings, strict activity selectors, and JSON/CLI/server/frontend adapters, with distinct registered proofs |
| Conformance: object-centric | ETOT, OCDFG and OTG graph comparison | **Partial** — deterministic ETOT, OCDFG, and OTG graph comparisons with strict thresholds, weighted fitness, diagnostics, direct-OCEL and explicit-graph native APIs, graph-vs-graph and OCEL-vs-graph headless CLI/local-server/frontend adapters, strict direct-OCDFG activity/object-type selectors, and cross-family envelope rejection; broader OCEL schema coverage remains |
| Discovery: Alpha | classic and Alpha+ | **Partial (classic, abstraction, and endpoint modules complete)** — grouped/stream/eager/lazy inputs, sparse weighted graphs, explicit and inferred endpoints, typed selectors, table ordering/null contracts, and native/request/CLI/frontend event caps are implemented; Alpha+ exposes raw live/detached markings and reserved-label collision cleanup through native/JSON/CLI/HTTP adapters, with repeated output-support checks; identity-dependent selection probabilities and correlations remain outside its complete claim |
| Discovery: Inductive | IM, IMf, IMd | **Partial (feature surfaces complete; differential proof pending)** — `log`, explicit `imf`, and graph-recursive `dfg` selectors have distinct native semantics; EventLog, stream, eager/lazy table, weighted-variant, and direct-DFG inputs are available; specialized fall-throughs, strict-sequence and fall-through toggles, field selectors, regrouping, timestamp ordering, recursive JSON, request/CLI/server/frontend plumbing, and model-specific downstream replay are covered by focused tests |
| Discovery: Heuristics | classic and Heuristics Miner++ | **Partial (implemented core; proof pending)** — both variants, exact dependency/AND/loop behavior, DFG pre-cleaning, filtering, performance aggregation, interval concurrency, and classic DFG input are available |
| Discovery: optimization | ILP classic and Genetic classic | **Partial** — bounded deterministic ILP and Genetic classic implementations are available through native, JSON, CLI, local-server, and frontend adapters; ILP supports one-trace-per-variant normalization, configurable silent boundaries, cumulative alpha sequence encoding, derived or overridden causal edges, and frequency-based noise filtering; Genetic results expose their normalized causal matrix and use the public matrix-to-workflow-net conversion, while differential edge-contract review and full solver parity remain |
| Discovery: declarative | Declare classic and log skeleton classic | **Complete** — all 18 Declare templates and all six log-skeleton constraint families retain their weighted threshold, vacuity, ordering, and diagnostic contracts; grouped logs, streams, eager/lazy tables, strict custom activity/case selectors, and log-skeleton variant lists are supported by distinct registered proofs |
| Discovery: graph/statistical | DFG frequency/performance/frequency triples/case attributes/clean; causal alpha/heuristic; correlation classic/classic-split/trace-based; minimum self-distance | **Complete** — all eight statistical DFG routes, eager and lazy table adapters, performance behavior, replacement helpers, graph utilities, both causal variants, all three correlation variants, and all three minimum-self-distance adapters with witnesses have focused native contracts plus JSON/CLI/server/frontend integration; eventually-follows is covered as a native statistical extension outside the algorithm-dispatch catalog |
| Discovery: footprints | log entire/trace-by-trace, eager dataframe, lazy dataframe, DFG, Petri reachability, process-tree bottom-up, POWL bottom-up | **Complete** — all eight selectors have distinct registered contracts and production dispatch; eager/lazy ordering differences, activity/case/time/index selectors, explicit Petri markings, and inclusive-choice bottom-up behavior are covered |
| Discovery: structured | transition system view-based; POWL brute-force/dynamic-clustering/frequency/maximal/tree cuts | **Complete** — view-based transition systems cover sequence/set/multiset views, forward/backward direction, arbitrary windows including zero, occurrence data, flat and trace inputs, and explicit activity/case selectors; POWL exposes TREE, BRUTE_FORCE, MAXIMAL, DYNAMIC_CLUSTERING, and the additional DYNAMIC_CLUSTERING_PLAIN/frequency behavior with filtering and order-frequency settings, backed by exact selector and module contracts |
| Discovery: temporal/behavioral | temporal profile; performance spectrum log/dataframe/lazyframe connected and disconnected projections; batches | **Complete** — strict native temporal discovery and permissive bundled analysis, all six canonical spectrum projections with adapter-specific sampling and timestamp behavior, all three batch adapters and five classifications, and every activity/resource/time/case/event selector have registered native plus production integration contracts |
| Discovery: object-centric | ETOT, timestamp interleavings, link analysis, OCDFG, OCPN annotated/unannotated, OTG, SAW nets | **Partial** — classic ETOT, strict timestamp interleavings over related event logs, deterministic link analysis (including the configurable generated index-column contract), OCDFG metrics (explicit total-object associations plus separate event-couple and total-object performance projections), annotated and unannotated OCPN per-object-type nets (IM/IMd) with opt-in per-object token-replay diagnostics and custom scalar activity/object-type selectors, OTG discovery, and classic stochastic arc-weight nets are available through native/headless adapters; SAW weight distributions retain per-event transition multiplicity (including final routing) and merged-net arc ownership decorations; broader object-centric schema and visualization coverage remains |
| Evaluation | replay fitness token/alignment; precision token/alignment/automaton/DFG; generalization token; simplicity arc-degree/Cardoso/cyclomatic; composite weighted metrics/F-score; earth-mover distance | **Partial (independent distance and DFG-precision modules complete)** — the stochastic-language distance selector and standalone DFG precision evaluator now have registered implementation and contract proofs, including their flat/lazy input and option semantics; the shared replay, alignment-precision, generalization, simplicity, and composite packages remain partial while their downstream conformance dependencies are audited variation by variation |
| Label splitting | contextual | **Partial (implemented core; proof pending)** — deterministic native contextual splitting with token-aware contexts, weighted greedy-modularity clustering, strict edge thresholds, custom classifiers/targets, JSON/CLI/local-server/frontend adapters, and focused regressions for weighted gains, zero-edge graphs, zero-gain merges, and deterministic ties |
| Merging | case relations | **Partial (implemented core; proof pending)** — the single case-relations variation merges related incoming cases with deterministic timestamp ordering, strict relation validation, and native/JSON/CLI/server/frontend entry points |
| Organizational mining | local diagnostics, network analysis frequency/performance, resource profiles, roles, SNA metrics | **Complete** — deterministic local diagnostics (group-attribute and explicit role/clustering modes), frequency/performance network analysis, role discovery, all eleven resource-profile metrics, and four SNA metrics (handover, working-together, subcontracting, joint-activities) have native/JSON/CLI/server/frontend adapters; focused contracts prove LOG/PANDAS adapters, custom selectors, diagnostics, common role records, and all eight SNA selectors |
| Reduction | process-tree transition-reduction based; ordinary Petri-net silent-transition reduction | **Partial (implemented core; proof pending)** — `TREE_TR_BASED` trace reduction, optional-subtree replacement, reference-compatible folding, and custom activity keys are exposed through JSON/CLI/server/frontend adapters. Ordinary PNML reduction exposes the complete structural fixed-point rule family through native, standalone CLI, multipart local-server, and typed frontend APIs, including deterministic single-entry/single-exit/series transition and place fusion, positively marked invariant-place elimination, duplicate silent-transition fusion, inert self-loop elimination, extended-arc preservation, redundant reset cleanup, and opt-in parallel-place fusion and abstraction; result diagnostics include remapped markings, place aliases, and reduced PNML. Marking-aware PNML import/export preserves arbitrary nonnegative initial/final vectors through reduction, while legacy conformance import remains strict to single-token workflow nets. |
| Simulation | Petri basic/stochastic/extensive; process-tree basic/extensive/top-bottom; DFG classic/performance; Declare; object-centric Petri/causal nets; Monte Carlo; random tree and tree+log generation | **Partial overall; tree generation plus Petri, process-tree, DFG, and Declare playout complete** — Petri basic/stochastic/extensive, Declare classic, both DFG variants, all three native process-tree playout variants, both seeded random-tree strategies (BASIC and bounded PT-and-log generation, including deterministic multi-model batches), and a bounded virtual-time Monte Carlo Petri variant with source-log-fitted activation weights and service means are wired through strict request parsing, CLI, local server, JSON, and frontend controls; completed catalog packages are backed by manifest-enforced native/request/JSON/CLI/server/frontend/test artifacts; Declare now uses nonnegative bounded native options, a shared strict server/request parser, and a dedicated frontend serializer instead of incorrectly emitting process-tree field names; DFG performance playout counts completed versus truncated/deadlocked traces and treats an invisible end-boundary transition after the visible-event cap as successful termination; bounded OCPN playout plus native causal-net obligation search support independent/shared/synchronized projections, marker groups, cardinality, key constraints, independent activity/binding/transition branching caps, reachability, binding sequences, and per-trace object suffixing through native, CLI, multipart local-server, and frontend adapters; broader simulation/model-family coverage remains |
| Transformation | event/trace/temporal features; next-activity/next-time/remaining-time targets; interval tree; trie; embeddings; OCEL descriptions/features/splitting | **Partial** — event/trace/temporal feature tables are native and exposed through `minera_export --log-to-features` and the local `/api/transform-features` endpoint; event rows infer typed attributes with bounded categorical cardinality, use canonical `event:key@value` names, and are rectangular across traces; trace extraction now defaults to trace-based rows, infers complete typed case attributes, scopes missing categorical values to traces with no observed value, and exposes selectable activity/succession representations, categorical cardinality bounds, case identifiers, duration/timing/path/workload/concurrency extras, and epsilon controls through native, JSON, CLI, server, and typed frontend adapters; trace timing extras enumerate only observed direct/indirect paths (including self-loops), honor the configured start timestamp, and retain the last valid positive occurrence; work-in-progress and resource-workload extras count overlapping cases, while concurrency extras count other events using the configured start timestamp; temporal rows now provide arrival/finish-rate, service/waiting/sojourn, rework, case/resource, and legacy compact projections with explicit feature schemas plus calendar frequencies (`W`, `MS`, `QE`, and related aliases) and fixed-width compatibility; all three target variants, open-path interval extraction, the traditional trie, contextual label splitting, deterministic case/event embeddings with cosine top-k ranking, both OCEL description variants with timestamp controls, all three bounded OCEL event/object/events-object feature tables with shared activity, timestamp, relation, endpoint, independently selectable lifecycle metrics and relation-stream lifecycle paths, deterministic object-specific relation-prefix length/timediff/one-hot activity columns, interaction, and custom attribute option groups, object-type filtering, optional per-object-type interaction counts, and opt-in interaction/descendants/inheritance/cobirth/codeath graph scalar columns for OBJECTS, and both connected-components and ancestors/descendants OCEL splits are native or headless CLI/server/frontend API-wired; numeric transformer model runtimes and broader OCEL feature parameters remain |

Process-tree simulation evidence: `BASIC_PLAYOUT`, `EXTENSIVE`, and
`TOPBOTTOM` are available through strict request parsing, the CLI
(`--playout-miner process_tree`), local-server `/api/playout`, cached-tree XES,
and frontend controls. Omitted process-tree playout settings select
`TOPBOTTOM`; `BASIC_PLAYOUT` and `EXTENSIVE` remain explicit selections. The
bounded extensive result reports `truncated` through native and JSON/frontend
contracts whenever its safety budget cuts off language enumeration, so a
partial language cannot be mistaken for a complete one. This
closes only the process-tree variants; the broader simulation row remains
partial because other simulation families are pending.

DFG playout evidence: classic best-first variant extraction and seeded
performance sampling accept activity, timestamp, and case-id attribute
selectors. Custom names are emitted as typed event/trace attributes while the
native first-class fields remain populated. Performance sampling uses distinct
internal synthetic start/end nodes with configurable labels, so real activities
that collide with either label (including equal labels) remain separate and
deterministic. The selectors and labels are available through strict request
parsing, standalone CLI flags, the local playout endpoint, and typed frontend
controls.

Statistical DFG evidence: frequency, frequency-triple, performance,
case-attribute, clean, and clean-time variants accept independent activity and
completion timestamp selectors. Standard XES names continue to use the native
first-class fields; custom selectors read typed event attributes, participate in
timestamp ordering and business-hour elapsed time, and reject missing fields
deterministically. Strict request parsing, standalone CLI flags, cached-server
mining, and typed frontend controls serialize the selectors consistently.

The table-native DFG surface adds eager and lazy graph adapters with frequency,
performance, combined, and cost measures; independent source and target
activity selectors; case, completion, start, cost, and index selectors; sorting,
window, per-case retention, aggregation, reduction, and business-time controls;
frequency triples; partial-order pairs; and inclusive or strict concurrent-event
pairs. Deterministic JSON, a standalone export path, the cached local endpoint,
and a typed frontend panel expose the same options. An explicit `none` schedule
means zero business time, while omission selects the standard weekday schedule.

Extensive process-tree playout also supports an opt-in set-string result. The
native result helper returns the deduplicated activity-word set while the
legacy EventLog API remains unchanged; the export CLI, cached local server,
and typed frontend helper expose the deterministic JSON envelope
`PROCESS_TREE_PLAYOUT` with sorted activity arrays. The option is restricted
to `EXTENSIVE` and rejects malformed or non-extensive requests.

Stochastic Petri playout keeps its default XES/EventLog output unchanged and
adds an opt-in `stochastic_return_visited_elements` control. The structured
`STOCHASTIC_PLAYOUT` envelope records alternating initial/follow-up markings
and fired transition indices, including silent transitions, with explicit
initial/final markings, seeds, and step bounds respected. The marking reached
after the final allowed firing is included; a case that begins at its final
marking contains only its initial marking. Native, request, standalone CLI,
local-server, and typed frontend helpers expose the same JSON contract.

Token replay diagnostics evidence: `replay_token_traces` returns one row per
source trace, retaining its source index and case ID even when cases share the
same activity variant or duplicate identifiers. The record includes token and
per-place counters, fitting classification and numeric fitness, the ordered
transition IDs fired (including silent routing), problem-transition IDs, unknown
event offsets and labels, examined-event count, early-stop state, and the reached
marking before final-token consumption. Enabled-transition IDs describe visible
transitions reachable through silent routing from that marking; this discovery
uses the existing bounded silent search and is not a completeness guarantee on
unbounded or unusually deep models.

`include_trace_diagnostics` optionally attaches these records to ordinary model
conformance without changing the default compact export or variant aggregation.
The shared JSON writer carries the records for discovered and imported models;
the cached server uses the same parsed policy options. The frontend presents
source-ordered case rows with expandable details and explicit display limits.
The native, request/JSON, and static-render suites cover duplicate cases, empty
logs/traces, weak firing, unknown-event positions, early stopping, explicit
markings, and agreement with existing aggregate/per-place results.

`walk_through_hidden_transitions` controls routing before visible events in both
variations and in their precision-prefix traversal. Forward final routing remains
independent. The three existing forward policies are now exposed end-to-end:
remaining-token fit classification, final hidden routing, and unknown-activity
fit classification. Disabling remaining-token classification does not change the
numeric token-fitness formula. Backwards replay retains its legacy behavior:
unknown labels are ignored, no final hidden routing occurs, and remaining tokens
make a trace unfit. The UI disables these three forward-only controls for that
variation. All Boolean request values are validated, including explicitly empty
values; supplied false values survive frontend serialization. These additions
do not certify either replay variation or its root-cause support module as
complete. Duration diagnostics have separate support-module proofs below.

Pinned black-box checks confirm pre-consumption reached markings, silent-closure
enabled sets even when visible-event routing is disabled, independent forward
final routing, and the negative numeric fitness possible after immediate stop.
Forward unknown events are ignored by default and never change token counters,
numeric fitness, or immediate-stop behavior. The explicit opt-in policy makes
their trace unfit while retaining the same replay. Fitting classification uses
event-input shortages and, optionally, remaining tokens, independently of final
token-demand shortages; a final demand of two tokens reached with one can thus
report fit while retaining one missing token in the counters. Forward per-trace fitness
is one when either token denominator is zero, while aggregate evaluation retains
its separately tested zero-denominator contract. These corrected behaviors are
shared by compact, detailed, per-place, and token-evaluation entry points.
Forward duplicate-label tie selection and replay flood-cleaning/exhaustive-
exploration controls still require reconciliation.
The backwards hidden-walk toggle is a native extension; the pinned legacy
variation ignores that parameter. Remaining root-cause encoding contracts and
trace/event identity-map compatibility remain separate work. Duration support is
covered independently from the still-partial replay algorithms.

Root-cause diagnostics now expose transition-underfeeding and unknown-activity
cohorts through native APIs, CLI/server JSON, and frontend controls/results.
Explicit categorical/numeric event selectors produce feature matrices, source
trace indices, class targets, and a depth-seven Gini tree. Cohorts retain source
log order, first supplied contexts, and problem-over-fit precedence. Optional
balancing repeats the entire smaller cohort and defaults off. Unknown-activity
baselines exclude the union of supplied unknown labels and use the raw final
event; forward replay observations share the final accumulated context of
examined known activities. Blocked unfired transitions have no root-cause
observations. These diagnostics leave replay metrics unchanged.

Standalone `build_root_cause_log` and `build_root_cause_representation` helpers
also expose preprocessing independently of classifier fitting. Without balancing,
they accept empty and one-sided cohorts; zero selectors produce empty rows, and
an empty input retains requested numeric columns. Log rows contain copied single
events with empty trace attributes. Balancing repeats complete
cohorts and rejects either empty cohort. Representation extraction retains
infinities and doubles outside float32 range; classifier fitting separately
rejects those values. Categorical duplicate selectors, rendered-name collisions,
numeric overwrites of colliding categorical columns, and literal missing/null
category markers now have independent full-matrix fixtures.

Independent examples are recorded in `docs/token-replay-root-cause-fixtures.json`
and exercised by the registered classifier, replay-cohort, and transport tests.
Training uses float32 comparisons while exported matrices retain doubles;
NaN values use learned missing routes and serialize as null. Missing-only cuts
have an explicit flag so tree JSON stays finite. Feature visitation is randomized
per node using an independently seeded local generator, permitting different
equally optimal feature choices. Threshold selection uses strict score
improvement, visiting all missing-right cuts before missing-left cuts. Independent
enumeration of all 62 nonconstant labelings of four finite values and two missing
values verifies the root-cut contract. Empty unknown-activity baseline cases are
skipped; explicitly supplied problem contexts can belong to empty cases.
Feature and unknown-activity selectors resolve exact keys before provenance
aliases, reject ambiguity only at the winning priority, and honor retained
first-class missing/null states and custom keys before stale text projections.
Recursive categorical values now preserve ordered scalar children, dictionary
replacement order, list duplicates, typed scalar representations, and nested
string escaping using Unicode 14 data. Date categories preserve microseconds
and explicit timezone offsets, including offset seconds; nested dates retain
their structured datetime representation. Numeric strings normalize Unicode
decimal digits and non-ASCII whitespace, while numeric containers and scalar
values carrying children are rejected. Native XES integration tests cover the
shared imported nested-value contract; importer-specific date normalization and
unsupported elements remain separate from direct helper behavior.
Standard XES trace and event `concept:name` provenance no longer collide in
log-skeleton and DECLARE event selection, allowing nested-XES diagnostics to traverse the
normal analysis pipeline while genuine event-field ambiguity still fails.
Trace-map memberships now expand value-equal source traces while retaining every
source index. Event dictionaries compare ordered event sequences with stringified
values; trace attributes retain typed numeric/date and recursive equality, and
trace properties do not participate. Fingerprint buckets are verified with full
equality and matching groups are cached; repeated contexts expand once per group.
Problem membership overrides the entire equivalent-trace class. Independent
fixtures cover numeric/string differences between event and trace scope, nested
ordering, datetime instants, and source-row expansion.
NaN trace attributes retain scalar identity through native copies and in-memory
stream/table conversions: copied aliases match, independently assigned NaNs do
not. Nested values use the same rule; event values retain stringified equality.
Serialized formats do not encode shared scalar identity.
Scalar replacement uses assignment or `emplace`; mutation through a raw mutable
variant reference cannot renew alias identity and remains outside this contract.
Unknown-activity cohorts now retain typed scalar keys through the optional
`activity_attribute` cell, including JSON output. Numeric/bool equivalents,
aware datetime instants, and shared NaNs use typed membership; string spellings
remain distinct baseline activities. Integrated diagnostics recover typed keys
from the original unknown events. Lists encountered as activities remain
baseline values for scalar keys.
Typed key buckets verify equality after fingerprint lookup, and baseline
eligibility is cached across classifiers. The UI includes scalar type labels
so same-spelling typed keys remain distinguishable.
Tuple values retain ordered children in native cells and JSON. Tuple categories
use recursive tuple syntax, including the singleton comma; typed key equality
ignores positional child names and distinguishes tuples from lists and strings.
Tuples containing mutable list/dictionary values cannot be keys. XES exports
their string spelling, matching the pinned exporter, and reimport yields strings.
The UI identifies tuple keys separately from their string spelling.
A reference-aware helper path uses `RootCauseSharedContext` and
`build_shared_root_cause_log` share entire nested value objects, including parent
scalar fields and child bindings. Top-level rebinding remains local to each
context. `build_shared_root_cause_representation` reads current values;
`materialize_root_cause_log` makes a detached snapshot for existing algorithms
and serializers. Importing a value-owned `Event` into the shared graph is also
a snapshot; callers retain shared contexts to observe mutations. Shared-context
encoding traverses selected values directly: cyclic dictionaries and tuples use
`{...}` and `(...)` markers, unselected cycles are ignored, and numeric nested
values are rejected without recursive materialization. Cyclic graphs still
cannot be flattened into tree-shaped native logs. The shared and ordinary
encoders use the same matrix construction and balancing logic. Integer values
outside float64 range raise overflow; equivalent numeric strings may yield
infinity in the standalone matrix helper.
Byte-valued attributes use `AttributeKind::kBytes` with hexadecimal octets in
their text cell, keeping arbitrary binary payloads safe in JSON. Public make/decode
helpers translate binary strings. Root-cause categories use byte-literal spelling,
typed byte keys remain distinct from strings and numbers, and numeric byte
conversion accepts ASCII syntax without Unicode digit/whitespace normalization.
XES writes byte-literal strings in both exporter variations; native stream/table
copies and JSON retain the binary type. The UI uses readable byte-key spelling.
The hexadecimal payload is authoritative even when a stale native scalar remains;
derived case IDs normalize byte cells to strings. This coverage is scoped to
root-cause encoding and transport. Byte conversion in other numeric feature
extractors still requires auditing.
Set and frozenset values have distinct native container kinds. Their children
carry the supplied member iteration order for categorical spelling; typed equal
members collapse to the first supplied representative. Empty spellings are
`set()` and `frozenset()`. Numeric feature selection rejects both types. Frozen
activity keys compare members without regard to order and match equivalent
ordinary set activities; mutable sets cannot themselves be cohort keys.
Trace attributes use typed unordered membership equality across both kinds,
while event attributes retain representation-sensitive equality. Both XES
exporter variations write categorical strings; native table/JSON copies retain
the distinct kinds and the UI displays their categorical spelling.
The support module remains **partial**: runtime hash-seed-dependent set ordering
is not reconstructed from insertion order, and remaining runtime value boundaries
still require auditing.
These contracts do not certify either replay variation.

Token replay flat-input evidence: `normalize_token_replay_event_log` and the
three replay API families accept `EventDataView`, streams, eager tables, and lazy
tables. Streams retain first-seen cases, including one group for present null
case IDs; tables sort numeric case keys before string keys and drop null cases.
Within-case events always retain source order and no timestamp column is
required. Typed numeric and string case IDs remain distinct. Empty tables still
validate required case/activity columns; empty streams are valid. Missing stream
selectors are rejected, while null activity cells retain their original event
position as unknown labels. Actual empty-string activity labels remain visible
model values. Null-aware variant keys and companion JSON markers prevent null,
empty-string, and embedded-separator labels from collapsing together.

Exact selected keys outrank retained source-key metadata and canonical aliases,
independent of field order. Nulls shadow lower-priority aliases and missing
transported selectors are rejected. Grouped inputs keep their explicit trace
boundaries; case selection applies only to flat data. Public normalization keeps
user metadata and unrelated event fields intact. A hard materialization cap
applies to every shape and all three APIs, including lazy scans, and is exposed
through request/CLI/frontend settings with explicit zero preserved. Backwards
table replay normalizes through grouped data as a native extension beyond the
pinned low-level backwards dispatcher input domain.

Alpha discovery defaults evidence: omitted `alpha_variant` selects classic
Alpha across native options, strict requests, CLI, and frontend. Named
`discover_alpha_classic` and `discover_alpha_plus` implementations back the
ordinary dispatcher. Classic supports grouped logs, streams, eager/lazy tables,
the internal graph, and sparse weighted maps with inferred or explicit
start/end maps. Explicit empty boundary maps suppress inference. Zero-valued
edge keys retain their labels and endpoint topology, but only positive forward
frequencies with absent/zero reverse frequencies create causal relations.
Reciprocal keys determine the parallel relation, including zero-valued keys.
The independently tested relation abstraction and endpoint helpers are used by
production discovery.

Streams and Alpha+ tables retain first-seen cases and source event order.
Classic tables derive edges from a stable case/start/completion ordering and
derive boundaries independently from the original first/last nonnull activity
per case. Nullable activities interrupt adjacency; null cases contribute no
model labels. Exact selected columns outrank retained source names and aliases,
including missing cells. Completion and explicit start columns are required
even with sorting disabled. An omitted start selector uses an existing
`start_timestamp` column or synthesizes it from completion; a null existing
start sorts last. Native date cells retain their typed lexical fallback.
The eager and lazy contracts share case/activity/time/start selectors, optional
row caps, and native case/time sorting controls. Normalization preserves source
order and is not a substitute for the classic table's separate edge and boundary
observations. Grouped and stream selectors remain strict.

Alpha+ always processes synthetic boundaries, including logs without short
loops. Its unrelated-set rule distinguishes activities with and without
outgoing relation keys. For each removed length-one-loop label, the last
observed non-loop predecessor replaces earlier predecessors; successors are
accumulated. Every maximal pair containing the predecessor/successor set
differences creates a separate, unmarked loop-only place; it does not attach
that loop to the existing ordinary flow place. Empty differences may qualify
multiple places. Leading, trailing, interior, adjacent, two-loop, mixed-empty,
and no-short-loop boundary fixtures are independently checked against an
executable snapshot of the pinned commit. Place membership in markings is
checked by identity, since distinct reference places may share displayed names.

The raw `discover_alpha_plus_result` API preserves explicit live and detached
initial/final markings, including all-loop/empty-only logs and boundary
reductions that leave a marking on a removed place. Detached entries retain
their structural names without identifying a live place with the same name;
the raw net has no fabricated source. The strict `discover_alpha_plus` workflow
adapter still rejects detached markings. A log containing no traces fails in
both the native and pinned public implementations.
A reduced self-loop alone is different: `a,L,L,a` returns two disconnected
boundary places with silent boundary transitions, matching the pinned runtime.
Cleanup removes the disconnected visible transitions. Adding a trailing `b`
retains the boundary-to-`b` connection and an isolated `L` loop place.
Artificial boundary identities now share visible activity labels during loop
removal, including `artificial_start` and `artificial_end`; boundary reduction
can therefore hide an ordinary activity or leave a detached initial marking.
All reduced trace endpoints connect to the source/sink, and hidden boundary
transitions retain reverse arcs. Initial cleanup requires a single-output
successor with an exclusive target producer; final cleanup requires an
exclusive prior-place consumer whose producers have no other outputs.
The enhanced variant and its dispatcher remain partial. Its
optional cleanup removes only completely disconnected visible transitions.

`--alpha-plus-result` and `POST /api/discover-alpha-plus` expose the raw graph
and markings as JSON, with activity selection, optional cleanup, and row caps.
The HTTP endpoint accepts uploaded logs or the cached session log; the typed
frontend client validates place identities, arc references, and markings.
The typed client is available independently; it is not yet connected to the
production UI. The raw result API accepts EventLog inputs; stream and table
adapters retain their ordinary Petri-net projections.
`scripts/capture-alpha-plus-reference.py` records public graph and marking
outputs from the pinned 2.7.22 runtime. The durable
`docs/alpha-plus-reference-fixtures.json` merges five captures with Python hash
seeds 0 through 4 and 20 calls per row. The 784-row corpus contains 774 distinct
cases; 72 cases vary across these seeds. Repeating a fixed seed produced no
additional observed alternatives in this capture.
Boundary cleanup samples incident choices independently and uniformly as an
approximation to reference ordering. Membership in sampled support does not
prove full support, selection probabilities, or correlation equivalence;
these remain open parity requirements.
The integrated audit sampled 7,740 native results across all 774 distinct
cases. Against seed 0 alone, 374 results were outside observed support; all
7,740 were within the merged five-seed support, with no malformed results.
`scripts/check-alpha-plus-sampled-support.py` rejects unsupported or malformed
results and requires pinned reference provenance. The portable
`scripts/check-alpha-plus-model-transports.py` passed all 21 CLI/HTTP checks,
including output files, uploaded and cached logs, selectors, and rejection
contracts. The Release build, all 150 native suites, 613 frontend tests,
TypeScript checks, and production frontend build passed. Six focused offline
tests also verify that the evidence gates fail on invalid output and provenance.
The comparison normalizes structural place names and incident transition
labels, distinguishes live from detached markings, and compares failure
presence rather than Python/C++ exception classes or messages. It does not
establish arbitrary graph identity or exhaust every possible log.

The strict discovery projection is reused for conformance, footprints,
alignment, decoration, comparison, evaluation, decision mining, stochastic
fitting, analysis, and playout. `alpha_max_rows_to_materialize`, CLI
`--alpha-max-rows-to-materialize`, and the frontend event-cap control enforce
the same hard cap, including explicit zero. Registered Alpha contract and
event-data suites, existing core regressions, request tests, and frontend
validation cover this checkpoint.

Random-tree generation evidence: the native generator exposes both the
`BASIC` strategy and the seeded, bounded `PTANDLOGGENERATOR` strategy. The
latter validates triangular activity-count bounds, supported operator weights,
silent/duplicate probabilities, and deterministic zero/one/many model
contracts. A single model retains the legacy `tree` JSON shape; zero or many
models use a `trees` envelope with shared activities, and a single seeded RNG
stream makes the first batch member equal to the corresponding one-model run.
Strict request parsing, standalone CLI JSON, local `/api/random-tree`,
frontend query serialization, recursive response guards, and an explicit
population/activity safety cap preserves the selected variant without silently
clamping large requests.
The request default is `PTANDLOGGENERATOR`; callers that need the recursive
legacy sampler select `BASIC` explicitly.

Monte Carlo simulation evidence: the native `monte_carlo` core implements the
`PETRI_SEMAPH_FIFO` virtual-time scheduler over workflow nets, seeded transition
selection, deterministic, exponential, constant-zero, immediate, normal, uniform,
lognormal, and gamma service times, bounded case/step and live-allocation
budgets, shared place timestamp heaps and signed occupancy counts,
completion/deadlock diagnostics, and deterministic EventLog output. Cases are
primed through their first firing before continuations are ordered by
completion time and generation index. Output resources are reserved before input
timing is updated. A firing completes at the later of its nominal service finish
and the latest reserved resource availability, with one combined wait interval.
Arbitrary multi-token initial/final markings
require final-key membership and componentwise token coverage.
Sparse supplied stochastic maps support every distribution and
define missing transitions as uniform, immediate choices; explicit weight and
duration vectors retain precedence. Source-log fitting honors custom activity
and timestamp selectors, selectable forward/backwards token replay, automatic
or forced normal/uniform/exponential fitting, causal token timing, and
activation/enabling ratios through the shared sparse stochastic-map fitter.
Omitted arrival cadence is inferred from sorted case-start times,
while an explicit ratio overrides inference. Generated XES retains typed aliases
for custom selectors. Documented diagnostic interval, scale-factor, and legacy
thread-limit controls are accepted and validated without introducing sleeping or
threads into the virtual scheduler. `monte_carlo_*` request fields are strictly
validated, the export CLI can select `--playout-miner monte_carlo`, the local
server exposes both `/api/playout?mode=monte_carlo` (XES) and `/api/monte-carlo`
(JSON), and the frontend offers typed query/result guards, interval-tree
validation, markings, selectors, replay, arrival, and compatibility controls.
Dedicated POST adapters keep supplied stochastic maps out of request-size-limited
URLs while the legacy GET routes remain available. The default simulation epoch
is 1,000,000 UTC seconds and generated timestamps retain microseconds. Public
visible-event duration fields remain populated when diagnostics are disabled;
that flag controls only the legacy elapsed-time vector. Zero capacities are
accepted and resolve to one resource. Ordinary transitions without inputs are
enabled; a simulation still requires an initial marking key, which may carry
zero tokens. Zero cases and traces without visible events return errors.

Object-centric Petri-net simulation evidence: the bounded native adapter
enumerates per-object-type workflow-net traces, combines object choices into
deterministic OCEL event/object/relation records, supports explicit object ids,
per-trace object suffixing, variable object-type bindings, explicit per-type
initial and final marker vectors, and strict trace/state/firing/timestamp
bounds. The
standalone `minera_ocel --ocpn-playout` command, multipart local
`POST /api/ocel/playout`, and typed frontend request/response guard expose the
same `OCEL_PLAYOUT` envelope; the endpoint also selects the log/DFG OCPN
discovery path and validates its thresholds. `event_binding=shared_activity`
coalesces same-activity local firings into one event with all selected object
relations; the default `independent` mode preserves one event per object-local
firing. The new `synchronized` mode retains an activity-to-object-type slot map
from discovery, fires enabled transitions for one deterministic binding in a
single global event, supports seeded transition/binding branching caps
(including fractional stochastic rounding), and can return reachability or
binding sequences. Legacy independent/shared modes apply the transition cap
during each per-object language expansion and the binding cap while selecting
per-object traces, using stable seeds derived from the requested seed and
object identity. Variable slots enumerate every enabled non-empty subset of
an object type deterministically before seeded sampling. The selected mode and
optional diagnostics are echoed in the JSON envelope. The CLI and local-server
query use escaped `TYPE:T0,T1,...` marker entries and reject unknown types or
vectors whose lengths do not match the selected net. Traditional exhaustive
Petri-net playout accepts the same marker-vector contract. Object-centric
causal-net playout now has a native obligation search: conjunctive input/output
marker groups, alternative groups, per-marker cardinality ranges, marker-key
object-sharing constraints, synthetic start/end obligations, deterministic
memoized reachability search plus bounded exhaustive trace enumeration, independent seeded activity/binding/transition branching caps, existence checks, binding
sequences, event-binding projection, and per-trace object suffixing. Discovered
models derive deterministic marker groups from per-object event contexts while
retaining the workflow projection for compatibility. The separate
`OC_CAUSAL_NET_PLAYOUT` envelope, `--oc-causal-net` CLI selector, and
`model=causal_net` server/frontend selector expose the same result contract;
explicit model construction is covered by the native JSON serializer.

Traditional Petri playout now applies explicit marker vectors consistently to
the basic sampler and weighted stochastic sampler as well as exhaustive
enumeration. The local `/api/playout` endpoint accepts `initial_marking` and
`final_marking` for basic/extensive mode and the prefixed stochastic forms;
the standalone playout/export CLIs and typed frontend query use the same
comma-separated contract. Basic sampling also exposes deterministic seed and
timing controls, a firing cap, and final-marking acceptance controls: callers
can retain only cases that reach the final marking and optionally accept
markings that contain it. The native API, `--petri-playout-*` export options,
generic `/api/playout` parameters, and typed frontend query serialize these
controls with matching validation.
The stochastic Petri analysis endpoint adds a first-class tangible-state
reachability view at `/api/stochastic-petri` and the corresponding
`--stochastic-petri` export mode. Ordinary arcs are enumerated in deterministic
marking order under a positive `max_states` cap (at most 2,000 states for the
dense generator); finite non-negative
per-transition exponential rates form a CTMC generator, duplicate target
transitions are summed, and immediate-transition metadata removes states with
enabled immediate firings plus incident immediate edges. Unsupported extended
arcs, malformed rates, markings, indices, and caps are rejected explicitly.
The JSON envelope is `STOCHASTIC_PETRI`/`TANGIBLE_CTMC` with deterministic
states, transition edges, generator rows, and exhaustion diagnostics. The same
endpoint and CLI now accept `stochastic_petri_analysis=transient` or
`steady_state`: transient propagation uses a dependency-free uniformization
series with finite non-negative time and unit-mass validation, while
steady-state analysis reports a deterministic distribution for one recurrent
class and an explicit non-unique diagnostic for multiple recurrent classes.
The retained projected initial marking is used when no transient source is
provided; if projection removes it, callers must provide a probability vector.
Dense propagation has a bounded work budget and a one-million-second wire-time
limit so requests fail deterministically instead of exhausting the host.
Probability envelopes use `STOCHASTIC_PETRI_DISTRIBUTION` with deterministic
state ordering, diagnostics, and finite-or-null JSON number handling.
A reusable stochastic-map layer represents deterministic, exponential, constant,
immediate, normal, uniform, lognormal, and gamma transition records. Replayed-log
fitting selects normal, uniform, exponential, or immediate records automatically,
or applies the observed forced-family rules, retaining activation weights and
priority metadata. A performance DFG can be converted to either classic workflow-net construction
with rates derived from edge means or the no-duplicate-activity construction;
silent routing is represented as immediate when its duration is zero. The
native map converts back to the existing tangible CTMC options, and
`minera_export --stochastic-map`, `/api/stochastic-map`, strict request parsing,
and typed frontend helpers share a `STOCHASTIC_MAP` envelope containing the
map, optional converted net, replay diagnostics, and reachability graph. The
CTMC projection eliminates priority-filtered silent routing with normalized
activation weights and exposes the projected initial probability vector.
Basic Petri sampling can also opt into a structured visitation response: each
trace alternates marking snapshots with fired transition indices (including
silent transitions), with transition activity labels where available. The
native serializer, CLI JSON output, generic endpoint, and typed
`fetchBasicPlayoutVisited` adapter share the `PETRI_PLAYOUT_VISITED`/
`BASIC_PLAYOUT` envelope.

Extensive Petri enumeration also has an opt-in `return_elements` control.
When `mode=extensive`, the local server, standalone export CLI, and typed
frontend helper expose an `EXTENSIVE_PLAYOUT` visited envelope with alternating
markings and transition indices, including silent firings. `max_marking_occ`
limits repeated occurrences of a marking on each candidate; omission keeps the
native unlimited behavior, while an explicit zero is rejected. Final-marking
acceptance and the visible trace-length bound remain unchanged, and ordinary
extensive requests still return XES/EventLog output.

Object-centric enrichment evidence: `enrich_ocel` deterministically materializes
the interaction, descendants, inheritance, cobirth, and codeath object graphs as
qualified O2O relations, and can qualify every E2O relation as creation, other,
or termination. Existing relations and qualifiers are preserved when a selected
graph does not add a new edge. The standalone `minera_ocel --enrich` mode,
multipart local `POST /api/ocel/enrich`, and typed frontend adapter expose the
same bounded selection and `OCEL_ENRICHMENT` envelope.

Direct object-graph evidence: `discover_ocel_object_graph` exposes each of the
five relations without synthetic O2O materialization, retaining isolated object
ids and deterministic lexical edge ordering. The `minera_ocel --graph` selector,
multipart local `POST /api/ocel/graph`, and typed frontend adapter share the
`OCEL_OBJECT_GRAPH` envelope and accept the same relation names.

Heterogeneous graph evidence: `discover_ocel_heterogeneous_graph` retains event
and object records as typed nodes, E2O and O2O relations in input order, and
optionally adds relation-order-preserving DF edges and indexed object-change
nodes. The standalone graph selector and dedicated local
`POST /api/ocel/event-object-graph` route expose the `OCEL_GRAPH` envelope with
both optional families enabled by default; disabling either family is available
through the CLI and query parameters. The existing object-only graph route
remains unchanged.

OCEL flattening evidence: `flatten_ocel_to_event_log` projects one non-empty
object type into a conventional `EventLog`. Each selected object becomes a
trace case id; source events drive the left-join order, while matching
event-object rows retain object-table and relation order, including duplicate
rows. The source event id is retained as `ocel:eid`, canonical activity and
timestamp fields can be replaced by scalar event selectors, and scalar event
and object attributes retain their native kinds and original keys. The
selected object type is emitted as `ocel:type`; no `case:` prefix is added to
trace attributes by the XES exporter. Public adapters accept the independent
`event_activity_attribute` and `event_timestamp_attribute` selectors (with
`--flatten-event-activity-attribute` and `--flatten-event-timestamp-attribute`
CLI flags). Unreferenced objects and events are
omitted, while O2O, E2E, and object-change records are ignored. Null scalar
attributes are omitted because the conventional event-log/XES model has no
null attribute kind. Unknown non-empty object types produce an empty XES log;
empty or all-whitespace selectors are rejected; object-type and selector values
are otherwise matched as supplied. The standalone `minera_ocel --flatten-object-type` selector
and multipart local `POST /api/ocel/flatten?object_type=...` route emit the
resulting XES.

OTG conformance threshold evidence: the scalar `otg_theta` remains the fallback
for every relation, while the standalone OCEL CLI, multipart local conformance
route, and typed frontend query expose strict non-negative relation overrides
for interaction, descendants, inheritance, cobirth, and codeath. Identical
relative-frequency deltas can therefore be accepted for one relation and
reported for another without changing the native `compare_otg` contract.

Object-centric graph interchange evidence: ETOT and OTG serializers now have
strict bounded JSON readers, and OCDFG serialized metrics (including event
couples, total-object tuples, performance projections, and the computed flag)
round-trip through a deterministic parser. The readers reject malformed pair or
triple shapes, negative frequencies, non-finite performance values, and invalid
graph envelopes. Direct-OCEL conformance adapters and explicit graph inputs
therefore share one validated native surface. The standalone OCEL conformance
command and local multipart conformance route now accept either an ordinary
OCEL document or the matching ETOT, OTG, or OCDFG graph JSON on either side.
CLI callers select `--real graph` and an optional `--real-graph-kind`; multipart
callers select `real=graph`, provide a `real_graph` part, and may declare
`real_graph_kind`. The default `log` upload continues to use the OCEL path, and
CSV/XML/SQLite normative imports continue to use the OCEL path. Graph readers
reject cross-family envelopes, so an empty graph cannot silently pass as a
different representation.

Annotated object-centric Petri-net evidence: `discover_ocpn` now retains the
deterministic OCDFG activity, start/end, and directly-followed-event metrics
alongside each per-object-type workflow net. The serialized `annotations`
section includes event/object tuples, event-pair aliases, object-type maps,
performance-disabled diagnostics, and double-arc decisions; the existing
`minera_ocel --discover ocpn`, local OCEL discovery route, and frontend result
guard continue to select the same `ocpn` variant. The opt-in
`diagnostics_with_token_based_replay` setting adds deterministic `tbr_results`
for each object type: relation-order flattened traces are replayed against the
discovered net, place counters retain compact `m`/`r`/`c`/`p` missing,
remaining, consumed, and produced token totals, and transition activation
counts include silent routing. The option is
strictly wired through the standalone CLI, local discovery route, and typed
frontend query/response guard; disabled results serialize as an empty map.
The discovery projection also accepts optional event-activity and object-type
attribute selectors. Scalar string, boolean, integer, and finite floating-point
values are rendered deterministically, missing or null selectors are rejected,
and the projected labels flow through annotated and unannotated OCPN models,
causal-net discovery, diagnostics, CLI, local-server, and frontend playout
requests without changing event/object identifiers or relations. OCDFG and SAW
discovery expose the same selector contract: OCDFG applies projected labels to
activity, start/end, flow, and performance metrics, while SAW applies them to
per-type nets, merged-net decorations, and transition-weight projections.
Their existing performance toggle and no-selector overloads remain compatible.
SAW discovery applies a fixed no-specialized-fall-through and normal-sequence
policy to each flattened object-type projection before replaying transition
multiplicities; the policy and sparse projected-activity regression are covered
by the native SAW test suite.

Link-analysis evidence: the classic relation join is available as
`discover_link_analysis` over event logs and supports first-class or generic
event/trace columns, timestamp-aware stable ordering, forward-only filtering,
first-occurrence reduction, deterministic transitive propagation, and a strict
link bound. Its standalone JSON, `minera_export --link-analysis`, cached local
`POST /api/link-analysis`, and frontend API/query guard all expose the same
options and canonical event references. Each endpoint retains the stable
`index` field for existing consumers and also materializes the configured
`index_column` name as an alias; names that collide with the other endpoint
fields are rejected.

OCEL split evidence: `split_ocel` implements the connected-components and
ancestors/descendants variants over the object-interaction graph. Projections
retain only selected objects, their related events and relations, matching
object-to-object links, object changes, and global values; ancestor projections
use deterministic lifecycle ordering and support an object-type selector and
object-count bound. The `minera_ocel --split` CLI, multipart local
`POST /api/ocel/split`, and typed frontend query/upload adapter share the
`OCEL_SPLIT` JSON contract.

OCEL feature evidence: `extract_ocel_features` provides deterministic numeric
tables for event rows, object rows, and exploded event-object rows. Event
features cover first-observed activity indicators, UTC epoch timestamps with
source-local calendar parts, independently selectable overall and per-type
related-object counts, relation-stream lifecycle starts and ends, attribute
counts, custom numeric/string attributes, newly observed interactions, and
opt-in min/max aggregation of the
selected object feature vector over each event's related objects. Object features cover type
indicators, lifecycle length/duration/endpoints, activity/path counts, custom
attributes, opt-in work-in-progress, normalized interaction-graph degree, and optional
per-object-type interaction counts. Per-type event counts deduplicate repeated
object identities while the overall count retains relation multiplicity;
categorical selectors stringify non-null scalar values without merging null and
empty-string categories. Degree centrality is independently
selectable from the interaction feature group, so OBJECTS callers can request
degree alone or interaction-by-type columns without degree. Lifecycle length,
unique-activity, duration/endpoints,
activity, path, and work-in-progress columns can be enabled independently
while the legacy lifecycle switch remains a group-level override. Work-in-progress
uses a deterministic forward window over lifecycle start/end boundaries and
restores the original object row order; objects without relations receive the
zero-boundary count of one, and the feature is disabled by default. Lifecycle
activity and path columns retain occurrence multiplicity, while paths are an
opt-in family rather than part of the broad preset. Optional
OBJECTS-only graph columns cover general
interaction degree, descendants and inheritance incoming/outgoing counts, and
cobirth/codeath degree. The OBJECTS table also supports opt-in related-event
min/max projections, last-related-activity event projections, neighboring-object
min/max projections, and lifecycle metric toggles within the lifecycle feature
group; these derived columns use the same bounded event/object feature schemas
and deterministic relation-stream/interaction ordering as the base tables. Exploded
event-object rows
project the same event and object feature columns pointwise, so the shared
option groups and custom attribute selectors are effective without changing
relation order or duplicate row suffixes. The OBJECTS table can be
filtered after feature computation to one requested object type, preserving
feature columns and original row order; missing types produce an empty table,
while the selector is rejected for event and exploded event-object variants.
The optional `include_all_object_features` control (and matching CLI, server,
and frontend adapters) enables the degree, per-object-type interaction, and
all five general graph scalar families together for OBJECTS only. Complete
nested object projections use a stable preset containing the standard
lifecycle and graph families while suppressing recursive related-event,
related-activity, and neighbor projections; selected numeric/string attributes
remain explicit. Explicit child false values suppress their family, while
explicit false and an omitted preset control retain compact defaults.
Exploded pointwise event and object projections can be disabled independently
with `include_event_pointwise_features` and
`include_object_pointwise_features` (or the matching CLI switches); an unset
control preserves both projections, and explicit controls are rejected for
EVENTS and OBJECTS. Disabled source tables are not computed, so
`max_features` applies only to the selected exploded output families.
Exploded rows also provide object-specific relation prefixes by default:
`@@ev_obj_pref_length`, `@@ev_obj_pref_timediff`, and deterministic one-hot
activity counts named `@@ev_obj_pref_act_<activity>`. Prefixes follow relation
order, retain each earlier event at most once per object, and use zero for
missing timestamps; length, timediff, and activity groups can be disabled
or enabled independently through native, server, CLI, and typed frontend
options that are strictly limited to the exploded variant. An explicit child
selection overrides a disabled prefix-group preset.
The `include_related_objects_features` control and its
`--features-related-objects` CLI counterpart enable this aggregation; they are
strictly limited to EVENTS and remain disabled for the exploded projection,
with duplicate relations deduplicated before aggregation and empty
related-object sets represented by zero columns.
The `include_degree_centrality_features` control and its
`--features-no-degree-centrality` CLI counterpart are strictly limited to
OBJECTS; the server and typed frontend reject this control for event and
exploded variants. When omitted, native callers retain the historical behavior
where degree follows `include_interaction_features`; the default remains
enabled.
The `interaction_graph_source` selector chooses the edge source for normalized
degree, per-object-type interaction counts, and neighboring-object min/max
projections: `event_cooccurrence` uses only objects sharing an event, while
`event_and_o2o` enables the union with explicit O2O links. Event co-occurrence
is the default when the selector is omitted, and degree normalization uses only
active graph nodes; declared isolated objects remain zero. An explicitly supplied selector
is strictly limited to OBJECTS, or to the
related-object EVENTS context, and does not alter the separate general graph
scalar families, which remain event-cooccurrence based.
The `object_neighbor_graph` selector independently chooses interaction,
cobirth, or codeath neighborhoods for neighboring-object min/max projections;
CLI, server, and frontend validation require the neighboring projection itself
to be selected.
Strict feature-count, selector, and relation-id validation is shared by
`minera_ocel --features`, multipart local `POST /api/ocel/features`, and the
typed frontend query/response adapter.
The component audit assigns 29 event, prefix, lifecycle, graph, attribute, and
related-projection modules to distinct declared option members and distinct
called contract cases in `minera_ocel_feature_components_tests`; the parity
verifier checks owner-qualified data-member declarations without accepting
comments, assignments, local variables, or reused cases.
The optional `feature_name_style=compatibility` mode (or
`--features-name-style compatibility`) maps categorical event/object columns
to `@@event_attr_value_<key>_<value>` and
`@@object_attr_value_<key>_<value>`, and maps object interaction-by-type
columns to `@@object_interaction_graph_<type>`. Native names remain the
default; compatibility names are reused consistently in related projections
and are rejected for the ambiguous exploded EVENTS_OBJECTS variant.

OCEL interchange evidence: the native core now parses and writes deterministic
extended-table CSV (with an optional object table), typed OCEL 2 relational
SQLite, and bounded classic/standard XML and JSON. Standard JSON/XML emit and
re-import deterministic object/event type catalogs, including empty types and
scalar attribute declarations without observed values. Classic JSON now
likewise emits and imports the `ocel:eventTypes` and `ocel:objectTypes`
catalogs; observed attributes fill in missing declarations with deterministic
scalar types while explicit catalog types are preserved;
typed values are coerced on import, static object attributes use the epoch
baseline, and every later timed value remains an ordered object change,
including entries that repeat the epoch baseline. CSV import validates
headers, object-list cells, timestamps, ids, and cross-type references; SQLite
round-trips the core relational tables plus typed attributes, event-to-event
links, object changes, and globals, and writes/imports standard type-specific
attribute columns plus exact declared-schema metadata and object-change rows;
XML/JSON import/export preserves
scalar attributes, qualifiers, object-to-object links, object changes,
globals, and deterministic ordering where the selected format carries them.
`minera_ocel`
detects `.csv`, `.sqlite`/`.sqlite3`/`.db`, and `.xml`/`.xmlocel` inputs and can
emit classic or standard XML or a SQLite file. The local server's multipart
`POST /api/ocel/sqlite` imports SQLite files, while the same route accepts
`mode=export` JSON and returns a bounded SQLite download. The bidirectional
`POST /api/ocel/io` route also accepts classic/standard JSON and XML, validates
the selected format, and returns bounded downloads or a canonical JSON import
envelope. The same route accepts event CSV uploads with an optional object-table
part and can download either deterministic event or object CSV. CSV exchange
now carries validated custom column names, object-type prefixes, one-character
delimiters, trimming and timestamp-sort switches, import rejection policy, and
explicit lossy-export opt-in through the CLI, local server, and typed frontend
helpers; non-CSV formats reject those options instead of silently ignoring them.

Embedding evidence: `to_embeddings` exposes both case-level and event-level
variants through deterministic bounded encoder profiles. `hashing-v1` retains
the original token profile, `hashing-v2` adds character n-grams, and other
non-empty model names select a salted token profile instead of being silently
ignored in hashing mode. Transformer mode is an explicit injectable provider:
the callback receives the selected text, model label, and requested dimension,
and its output length and finite values are validated. Transformer callers may
set `embedding_dimensions=0` (or `--embedding-dimensions 0`) as an inference
sentinel; zero is passed through to the callback/runtime, the first non-empty
finite vector establishes a bounded width (at most 4096), and every later or
target vector must match. Hashing providers continue to require a positive
explicit dimension. Applications can either
provide that callback per request or register a thread-safe process-level
default runtime bridge; the latter is used only when a request has no explicit
callback. The explicit
`mock_transformer` model supplies the deterministic headless CLI/server/frontend
integration path; other transformer models require an application callback and
never fall back to hashing. For standalone and cached-server operation, the
optional `--embedding-runtime-url http://host:port` bridge posts
`{"text":"...","model":"...","dimensions":N}` to the fixed `/embed`
path and requires a successful response shaped exactly as
`{"embedding":[finite numbers...]}`; zero-dimensional requests return the
provider's inferred width. Connection,
read, and write calls are bounded to five seconds; malformed URLs, HTTP
failures, extra fields, malformed numbers, empty vectors, wrong dimensions, and
non-finite values are rejected. The CLI enables transformer mode unless an
explicit provider is supplied; the server applies the same default only when
the request omits its provider, so explicit hashing remains unchanged. Provider,
model label, and vector dimension are
echoed by JSON and similarity responses. `minera_export --embeddings`, cached-session
`POST /api/embeddings`, and the typed frontend request/response guards share the
same provider-aware contract. Similarity rows sort by descending cosine score,
then descending identifier; exact duplicate ties retain input order. Event
filtering can retain only selected events or complete cases. Numeric model
weights and their runtime remain an optional application integration concern.

Timestamp-interleavings evidence: the native discovery joins related left/right
event logs through `CaseRelation` rows, adds deterministic extreme trace
boundaries, and emits only strict four-timestamp LR/RL crossings with source /
target activities, original event indices, timestamps, and finite elapsed
seconds. Missing or invalid timestamps, malformed relations, and result-bound
overruns are rejected during discovery. The standalone
`minera_export --timestamp-interleavings` path, multipart local
`POST /api/timestamp-interleavings`, strict request parser, frontend upload
adapter, and focused Release tests share the same canonical JSON contract.

DFG simulation evidence: `CLASSIC` performs deterministic best-first expansion
of a frequency DFG with start/end boundaries, probability ordering, occurrence
limits, graph-completeness interruption, and safety bounds. `PERFORMANCE`
samples frequency-weighted paths and exponential edge delays from the native
performance DFG using a reproducible seed and case-arrival schedule. Both
variants are exposed by `minera_playout --dfg`, `minera_export
--playout-miner dfg`, local-server `/api/playout?miner=dfg`, and frontend
controls. Ordinary requests retain XES output. Classic requests with
`return_variants=true` use a deterministic JSON envelope containing one
activity-sequence and occurrence-count entry per discovered variant, together
with completeness and interruption status; `return_only_if_complete` clears
the entries when coverage is not complete. The native log continues to carry
probability and estimated-occurrence trace attributes for XES consumers.

Petri stochastic simulation evidence: the native `stochastic_playout` API uses
transition-index weights, validates finite non-negative values and weighted
input arcs, samples all-zero enabled sets uniformly, preserves silent routing,
and stops at the exact workflow final marking unless configured otherwise.
The seeded options are available through `--playout-variant stochastic`, the
local server's `mode=stochastic` route, and the frontend controls, with strict
trace/step/event-budget limits at adapter boundaries. The supported native
contract also provides deterministic source-log fitting: reachable enabled
transitions receive activation ratios, stable duplicate-label and silent
routing replay is supported, unknown source labels and partial traces are
tolerated, and never-reached transitions retain neutral weight. Explicit
transition weights take precedence over fitted values in the source-log
overload; the native replay API is wired through strict request parsing, the
export CLI, the cached local server route, and typed frontend controls.

## Data, object, conversion, and integration checklist

| Surface | Required variations | Minera status |
|---|---|---|
| Event data | Event, trace, event stream, typed attributes, lifecycle, dataframe/lazyframe adapters | **Partial** — native EventLog, value-semantic EventStream/EventTable, restartable bounded LazyEventTable, explicit missing/null/empty cells, full extension/classifier/global/property metadata, and restartable CSV/XES event/trace readers are available; third-party runtime object identity remains adapter-specific |
| Object-centric data | OCEL events/objects/relations/o2o/e2e/object changes and OCEL 1/2 schema | **Partial** — bounded events/objects/event-object and event-to-event relations, o2o links, object changes, scalar globals, classic/standard JSON/XML, declared event/object type catalogs with empty-type schemas, and a typed OCEL 2 relational SQLite projection are present; remaining schema coverage remains |
| Models | Petri nets incl reset/inhibitor, markings, process trees, BPMN, DFG, transition systems, heuristics nets, POWL, declarative models, OCPN/OC causal nets, trie, stochastic nets | **Partial** — ordinary and extended reset/inhibitor Petri arcs now round-trip through PNML/JSON and are honored by replay, alignment search, soundness reachability, and Petri playout; reusable stochastic maps now round-trip distribution/rate metadata and derive tangible CTMC options, while object-centric, trie, and broader stochastic model-family coverage remains |
| Conversions | log/stream/table; tree to Petri/BPMN/POWL; BPMN/DFG/genetic-matrix/heuristics/trie/WF-net conversions; OCEL graph/features | **Partial** — all six traditional log conversion selectors now cover EventLog identity, ordered event streams, rectangular and restartable lazy tables, bounded polymorphic log/stream/table normalization, attributed directed graphs, and one/all-trace variant projections; tree/heuristics/POWL/BPMN, deterministic POWL-to-process-tree layering, strict block-structured workflow-net-to-process-tree reconstruction (with non-block rejection), ordinary workflow-net-to-POWL reconstruction for structured fragments, both classic DFG-to-Petri constructions, genetic causal-matrix to workflow-net conversion, trie-to-Petri conversion, tree to POWL, tree/net to BPMN, structured inclusive BPMN split/join import with every non-empty branch subset, typed heterogeneous OCEL event/object graphs with optional DF and object-change families, and object-type OCEL flattening to typed XES traces are present; third-party columnar identities and broader object-centric conversions remain |
| Traditional I/O | XES parser variants, PNML, PTML, BPMN, DFG read/write | **Partial** — all six XES importer selectors share a standards-compliant native parser with document-order defaults, optional forward/reverse timestamp sorting, trace limits, scalar projection, custom first-class selectors, and strict adapters; extensions, classifiers, trace/event globals, selector properties, typed scalar metadata, and empty-versus-null behavior now survive native import/export cycles; both XES exporter selectors and bounded native gzip interchange are available through native, CLI, server, and frontend surfaces. PNML file/string import and export preserve arbitrary nonnegative markings, weighted ordinary arcs, reset/inhibitor arcs, transition visibility, and optional stochastic distribution metadata; missing marking inference is configurable and a standalone CLI inspection mode exposes the parsed document. Broader interchange variants remain. |
| Object-centric I/O | OCEL CSV, JSON, XML, SQLite and OCEL 2 JSON/XML/SQLite | **Partial** — classic and standard JSON/XML now round-trip deterministic type catalogs, including empty types and declared attributes without observed values, typed scalar attributes, epoch-baselined object histories, event/object relations, and event-to-event links; typed OCEL 2 relational SQLite persists the exact declared schemas alongside inferred type-table columns, deterministic extended-table CSV (with an optional object table), and bounded CLI/local-server/frontend adapters for every JSON/XML/CSV/SQLite direction are present; remaining schema-specific adapters remain |
| Filtering | attributes/cases/endpoints/variants/paths/LTL/time/case performance/between/rework/prefix/suffix/segments; trace-pattern filters; DFG filters; OCEL filters | **Partial** — broad traditional-log filters, inclusive numeric, absolute-frequency, and relative-occurrence attribute ranges, deterministic top-activity selection, strict/non-strict first/last prefix and suffix cuts, positive/negative endpoint selection, per-variant minimum/maximum coverage and cumulative percentage selection, exact admitted-variant selection (including empty variants and polarity) plus exact/prefix/suffix/infix trace-pattern matching with `...` spans through native/request/CLI/server/frontend mining surfaces, native/CLI/server/frontend adjacent-path filtering with custom activity/timestamp attribute selectors and inclusive performance bounds, prefix/suffix/between segment variants, all four LTL predicates through native/CLI/server/frontend adapters, connected DFG activity/path/dependency/boundary and noise-threshold filters through native, CLI, server, and frontend export surfaces, and complete native/CLI/server/frontend OCEL filters; OCEL adapter query values now preserve nested delimiters and typed scalar overflow is rejected under a bounded recursive level; broader filter families remain |
| Statistics | endpoints/attributes/variants, calendar attribute distributions, numeric/date KDE, case statistics, passed time, service/cycle/arrival/overlap/rework, concurrent/eventually-follows, process cube, OCEL statistics, chaotic activities | **Partial** — deterministic calendar event distributions across all six time buckets, numeric/date attribute KDE grids, case timing rows with sorting/limits/business-hour durations, passed-time variants, per-activity service-time aggregation, both activity/case rework variants, interval-event/case-envelope/concurrent-activity overlap, entropy-based chaotic-activity ranking, typed numeric/categorical/composite process cubes, and OCEL object-type summaries, activity/object associations, and edge metrics with configurable business-hour durations are available through native/CLI/server/frontend adapters; broader statistical aggregation extensions remain |
| Visualization | Petri/DFG/tree/BPMN/heuristics/footprints/SNA/transition system/POWL/declarative/OCEL, decorations and render backends | **Partial** — interactive native UI includes statistical/causal/correlation DFG results, performance spectra, batch diagnostics, recursive POWL, footprint variants/diagnostics, Declare, log-skeleton, converted workflow nets and decorations, plus bounded deterministic SVG views for classic object-centric directly-follows, event-to-object-type, and object-type-relation graphs; several model families and render backends remain |
| Streaming | CSV/XES event/trace streams, streaming DFG, online Declare/footprints/token/temporal conformance, classic/thread-safe/Redis stores | **Partial** — restartable CSV/XES event and trace readers, live bounded worker-pool streams, incremental DFG, a file-backed restartable streaming DFG store, deterministic classic and mutex-protected dictionaries, a bounded RESP remote dictionary, Declare, footprints, token, and temporal conformance are present; richer distributed lifecycle orchestration remains |
| Decision mining | decision-tree extraction, encoded decision tables, data Petri nets | **Partial** — decision trees/rules with bounded depth/split/leaf/candidate/gain controls, selected context attributes, and configurable event activity fields, deterministic encoded numeric/one-hot tables with bounded pre-choice lookback, and guarded data-Petri-net JSON/CLI/server/frontend surfaces are present; clean variants use exact replay while deviating variants now use optimal-alignment model choices with replay/alignment/failure diagnostics; richer classifier backends and columnar training integrations remain |
| Privacy | anonymization algorithms and privacy-preserving query variants | **Partial** — PRIPEL-style release and Laplace/SaCoFa trace-variant queries have native and adapter surfaces, with explicit deterministic or OS-backed secure-random modes; broader privacy contracts remain |
| ML | train/test splits, prefixes, feature tables, prediction targets, OCEL features | **Partial** — deterministic seeded train/test trace splits, fixed and all-prefix projections, until-activity targets, activity-targeted sublogs, automatic event/case feature selection, and typed process-cube tables now have native, standalone CLI, cached-log server, and typed frontend adapters; model training and columnar dataframe integrations remain |
| LLM/querying | abstractions, provider calls, clustering/hypotheses, natural-language log query/filter, database knowledge injection | **Partial (implemented core; proof pending)** — bounded provider-neutral prompt construction covers all three catalogued injection packages (generic descriptions, process-mining knowledge, and database-schema knowledge) through native, JSON, strict request, standalone CLI, local-server, and typed frontend APIs without performing network calls; provider execution, clustering/hypotheses, and natural-language log querying remain separate integration work |
| Connectors | Mail/calendar, Windows events, browsers, repository issue history, workflow history, enterprise order-to-cash/accounting | **Partial (implemented core; proof pending)** — every connector family has a native, bounded normalization contract with JSON, CLI, local-server, and typed frontend adapters; mailbox class/direction mapping and calendar lifecycle/reminder synthesis consume explicit CSV exports without desktop automation, while browser and Windows connectors likewise require explicit files; remote repository ingestion confines timeline requests to the configured repository and keeps bearer credentials out of query strings; optional live platform/database drivers remain deployment adapters |

Overlap evidence: `OverlapOptions` validates finite non-negative epsilon,
supports configurable activity/timestamp attributes, and supports inclusive or strict endpoint semantics. When `overlap=true`, the
native interval-event, case-envelope, and concurrent-activity counters run
after all ordinary, segment, path, and LTL filters; JSON embeds the three
arrays under `overlap`. CLI flags and server/frontend form fields are covered
by `src/backend/tests/test_request_params.cpp` and the focused overlap/native
JSON tests.

Passed-time evidence: `discover_passed_time` exposes `PRE`, `POST`, and
`PREPOST` with deterministic neighbor rows, edge frequencies, weighted
averages, custom activity/timestamp attributes, scalar performance
aggregations, and optional business-hour durations. The standalone CLI,
`POST /api/passed-time`, strict request parser, and typed frontend API share
the `PASSED_TIME` envelope; focused tests cover all variants, custom keys,
aggregation validation, and malformed response values.

Service-time evidence: `discover_service_time` computes each event's
start-to-completion duration grouped by activity, with deterministic
`MEAN`, `MEDIAN`, `MIN`, `MAX`, and `SUM` aggregation plus optional
business-hour elapsed time and custom timestamp/activity keys. The standalone
CLI, `POST /api/service-time`, strict request parser, and typed frontend API
share the `SERVICE_TIME` envelope; focused tests cover custom keys,
aggregation rejection, finite JSON values, and empty/invalid inputs.

Cycle-time evidence: `discover_cycle_time` orders event intervals by their
start instant, merges overlapping production spans, and reports completed
span time per trace instance with configurable completion/start timestamp keys.
The standalone CLI, `POST /api/cycle-time`, strict request parser, and typed
frontend API share the `CYCLE_TIME` envelope; focused tests cover custom keys,
empty/invalid timestamps, interval merging, and bounded diagnostics.

Case-arrival evidence: `discover_case_arrival` computes mean gaps between
sorted case starts and sorted case ends, with optional business-hour slots and
zero-safe single-case behavior. The standalone CLI, `POST /api/case-arrival`,
strict request parser, and typed frontend API share the `CASE_ARRIVAL`
envelope; focused tests cover custom timestamps, business-hour validation,
empty gaps, and malformed response values.

Attribute-statistics evidence: `discover_attribute_statistics` counts event or
trace attribute values deterministically, supports first-class activity,
timestamp, resource, lifecycle, and case-id aliases, and optionally keeps one
occurrence per case. The standalone CLI, `POST /api/attribute-statistics`,
strict request parser, and typed frontend API share the
`ATTRIBUTE_STATISTICS` envelope; focused tests cover both scopes, deduplication,
missing values, custom attributes, and count validation.

Attribute-distribution evidence: `discover_event_distribution` implements the
calendar buckets `DAYS_MONTH`, `MONTHS`, `YEARS`, `HOURS`, `DAYS_WEEK`, and
`WEEKS`, fills the complete fixed ranges where applicable, and validates every
configured timestamp. The standalone CLI, `POST /api/attribute-distribution`,
strict request parser, and typed frontend API share the
`ATTRIBUTE_DISTRIBUTION` envelope; focused tests cover custom timestamp keys,
zero-filled bins, ISO week boundaries, and malformed values.

Attribute-KDE evidence: `discover_attribute_kde` provides deterministic
numeric and date Gaussian-density grids, preserving extrema, handling a
single observed value without producing an invalid bandwidth, and bounding
date samples before fitting. The standalone CLI, `POST /api/attribute-kde`,
strict request parser, and typed frontend API share the `ATTRIBUTE_KDE`
envelope; focused tests cover numeric/date parsing, finite non-negative
density validation, deterministic grids, sampling bounds, and malformed
requests.

Boundary-activity evidence: `discover_boundary_activities` exposes START, END,
and BOTH counts for configurable activity-like event attributes, skipping empty
traces and missing boundary values. The standalone CLI,
`POST /api/boundary-activities`, strict request parser, and typed frontend API
share the `BOUNDARY_ACTIVITIES` envelope; focused tests cover all variants,
custom keys, empty traces, and malformed counts.

Variant-statistics evidence: `discover_variant_statistics` groups complete trace
activity sequences, sorts them by descending frequency and deterministic
sequence order, and emits normalized stochastic-language probabilities with an
optional result bound. The standalone CLI, `POST /api/variant-statistics`,
strict request parser, and typed frontend API share the `VARIANT_STATISTICS`
envelope; focused tests cover empty traces, custom keys, limits, and probability
validation.

Case-statistics evidence: `discover_case_statistics` emits one deterministic
timing row per non-empty trace, supports case-id and timestamp attributes,
ascending/descending case-id/start/end/duration sorting, result limits, and
optional business-hour elapsed durations. The standalone CLI,
`POST /api/case-statistics`, strict request parser, and typed frontend API share
the `CASE_STATISTICS` envelope; focused Release tests cover sorting, limits,
business-hour slots, invalid timestamps, and malformed response values.

Rework evidence: `discover_rework` exposes the activity variant (number of
cases containing repeated activity occurrences) and the cases variant (total
events and total repeated-activity count per case), with custom activity and
case-id attributes. The standalone CLI, `POST /api/rework`, strict request
parser, and typed frontend API share the `REWORK` envelope; focused tests
cover duplicate activities, empty cases, custom keys, and count validation.

Chaotic-activity evidence: `discover_chaotic_activities` implements the single
entropy-based ranking variant with configurable activity attribute, smoothing,
and bounded input size. It returns deterministic frequency, entropy,
entropy-gain, and combined-score metrics. The standalone CLI,
`POST /api/chaotic-activities`, strict request parser, and typed frontend API
share the `CHAOTIC_ACTIVITIES` envelope; focused tests cover repeated runs,
custom attributes, empty input, invalid bounds, and malformed response data.

OCEL statistics evidence: `object_type_activities` returns deterministic
lexical object-type keys with deduplicated related activities, while
`objects_ot_count` returns deterministic event/object-type maps and counts
duplicate relation rows independently. Activity associations support none,
start, and end relation prefilters; object-type-dependent associations and
event-edge metrics include event couples, unique objects, event-object triples,
and sorted elapsed-time or configured business-hour samples. The native APIs, `minera_ocel --statistics`,
multipart local `POST /api/ocel/statistics`, and typed frontend
request/response guard share the `OCEL_STATISTICS` JSON envelope. Empty logs,
unreferenced objects, duplicate relations, invalid identifiers, prefilters,
repeated activity labels, and malformed timestamps are covered by focused
Release tests.

Process-cube evidence: `process_cube` validates rectangular typed feature tables,
supports exact numeric and categorical columns, composite categorical dimensions,
prefix indicator columns, mixed numeric/prefix Cartesian cells, explicit or
equal-width numeric edges, and mean, sum, min, max, count, median, first, and last
aggregation. The deterministic
`PROCESS_CUBE` JSON envelope is shared by the native tests, the headless
`minera_process_cube` CLI, local `POST /api/process-cube`, and typed frontend
request/response guards. Explicit bins are sorted and deduplicated, configured
cells retain their case-id sets, composite names accept escaped comma-delimited
CLI/query lists, and malformed JSON, ragged rows, mixed scalar types, non-finite
values, and unsafe division limits are rejected at the boundary.

Activity-split evidence: the native log filter splits every non-empty trace at
one or more anchor activities, preserves event/trace/log attributes, rewrites
case ids with a deterministic separator and zero-based suffix, and supports
both anchor-in-new-case (`this`) and following-event-in-new-case (`next`)
modes. Strict request parsing, CLI flags, local mine-server fields, and the
frontend form/serializer expose the same operation; focused tests cover
multiple anchors, no-match traces, attributes, case-id synchronization, and
invalid options.

Attribute-value repetition evidence: a whole-trace filter counts matching
activity, resource, lifecycle, timestamp, or generic event-attribute values
and keeps traces inside an inclusive repetition range. The native operation
preserves log and trace attributes, validates min/max ordering, and is wired
through strict mine-request fields, CLI flags, local-server form handling, and
frontend serialization/controls. Focused tests cover default and zero ranges,
generic attributes, missing values, aliases, and invalid bounds.

Case-grouping evidence: consecutive activity runs support first/last event
retention, while same-timestamp groups support first, last, and deterministic
lexical activity concatenation. First-class and generic event keys, custom
timestamp keys, and a configurable non-empty concatenation separator are
validated. Native, request, CLI, local-server, and frontend surfaces share
the same grouping options and focused edge-case coverage.

DFG interchange evidence: the strict classic text parser and deterministic
writer now have public input and output adapters. `minera_dfg INPUT.dfg` reads,
validates, and normalizes a document (or writes it to an explicit output path),
and its `--clean-noise` mode applies the native noise-threshold edge filter
with optional most-common-path overrides. The cached `GET /api/dfg` route
accepts the same cleaning query, while local `POST /api/dfg` accepts raw DFG
text and returns the canonical document. The typed frontend `importDfg` and
`fetchDfg` helpers expose the corresponding validation and export paths.

DFG conversion evidence: both classic DFG-to-workflow-net constructions are
available as deterministic native APIs: activity-defines-place emits one
visible transition per start or edge occurrence, while invisibles-no-duplicates
emits one visible transition per activity and silent edge routing. Boundary
activities are inferred when omitted, artificial names are collision-checked,
and the standalone DFG CLI, local conversion endpoint, and typed frontend
adapter share a validated `DFG_CONVERSION` envelope.

Genetic-matrix conversion evidence: the public causal-matrix model retains the
stable activity table plus every alternative/concurrent input and output group.
The `TO_PETRI_NET` construction normalizes set-like groups, builds complete
shared places where both matrix projections agree, and replaces incomplete or
transitively conflicting regions with silent routing. Disconnected and cyclic
boundaries receive deterministic workflow entry/exit routing, and the returned
initial/final markings target the unique source and sink. Classic genetic
discovery exposes its winning normalized matrix and reconstructs its final net
through this public converter. Native JSON, `--genetic-matrix-convert`, cached
`/api/genetic-matrix-convert`, and typed frontend adapters expose the explicit
variant. Focused fixtures cover sequence, parallel split/join, fallback routing,
normalization, malformed references, deterministic transport, replay, and
mined-matrix reconstruction.

Traditional log-conversion evidence: the native conversion API exposes the six
selectors for EventLog identity, flattened event streams, typed rectangular
tables, attributed directed graphs, one trace per control-flow variant, and all
frequency-preserving variant copies. Stream conversion preserves event order,
typed scalar fields, first-event trace attributes, configurable case-field
prefixing, and stable trace-index glue when case attributes are disabled. Graph
conversion supports optional directly-follows edges, selected case/event
attribute nodes, simple directed-edge overwrite semantics, and explicit node
and edge limits. Variant projections use deterministic descending frequency
and sequence ordering, support an optional variant limit, and can return
output-trace indexes. Strict request
parsing, the standalone export CLI, cached local-server route, deterministic
JSON, and typed frontend request/response guards share the same option family;
focused tests cover round trips, sparse table cells, graph bounds and edge
orientation, selector validation, variant multiplicity, and malformed payloads.

Trie conversion evidence: prefix tries now convert to deterministic workflow
nets with shared prefix places, silent terminal-prefix exits, multiple root
branches, and explicit source/sink places. The export CLI's
`--trie-to-petri` mode, cached local `/api/trie-to-petri` route, and typed
frontend request guard share the `TRIE_CONVERSION` envelope.

## Delivery order

1. Stabilize the native semantic contracts and differential fixture format.
2. Finish remaining traditional discovery and conformance variants.
3. Add OCEL 2 data, I/O, discovery, conformance, filtering, and visualization.
4. Add declarative/POWL/transition-system and advanced simulation families.
5. Add streaming, privacy, clustering/drift, transformations, and ML.
6. Add optional renderers, connectors, querying, and LLM integrations.

Every completed row must include focused unit tests. Algorithms with
non-unique models compare canonical behavior (language, footprints, fitness,
or replay) rather than generated node identifiers. Floating metrics use an
explicit tolerance. Performance claims require representative logs and may
not replace semantic tests.

## Petri-analysis parity evidence

The native analysis surface now exposes the classic integer marking equation,
the split-aware extended marking equation, structural workflow-net validation,
and bounded soundness diagnostics, including an explicit `woflan` family
selector over the same reachability, coverability, invariant, and pair
diagnostic contract. Woflan additionally constructs the silent sink-to-source
short circuit, reports its transition id and strong-connectivity result, and
serializes a separate short-circuit reachability projection while retaining
the original graph for completion witnesses. Woflan marking inputs follow its
single-place boundary contract. The soundness family also exposes a strict
`return_asap_when_unsound` control: a structural S-component coverage failure
returns immediately with invariant evidence, while a reachable non-final
deadlock or improper completion marking returns with the partial reachability
graph and its shortest witness. Structural pair diagnostics use a deterministic
node-split unit-capacity flow graph and report both directions whose maximum flow
exceeds one, preserving the duplicated-node coordinates in the JSON contract.
Bounded exploration also returns every shortest path to a marking that cannot
reach the final marking as `locking_scenarios`, alongside the compact first
witness and the `dead_tasks` alias for never-enabled transitions. Woflan's task
projection is derived from the short-circuited coverability graph and reports
visible tasks, while the generic `dead_transitions` field retains the exact
original-net firing result. The native `not_well_handled_pairs` name is
serialized alongside the historical `handled_pairs` compatibility field.
Every pair coordinate also has a stable place/transition input-output label for
renderers.
Woflan now also exposes its `left` decision and a deterministic restricted
coverability-tree projection. The tree preserves one first-seen edge per
transition at each node, accelerates only against ancestors using the omega
marker `-1`, reports explicit state-cap exhaustion, and is kept separate from
the minimal coverability graph. These artifacts are available in the native
result, JSON envelope, and frontend response guard for both bounded and
unbounded workflow diagnostics.
The extended equation keeps trace events in
ordered segments, assigns each event to a synchronous or log move, constrains
segment-boundary markings, and solves the resulting integer state equation with
caller-controlled costs and deterministic node limits. Recovery separately
attempts a concrete firing sequence, so an incidence-feasible result that is
not realizable is reported rather than silently presented as a replay.

Strict request parsing covers family, miner, trace index, markings, transition
cost vectors, split indices, split limits, solver limits, bootstrap, and
diagnostic settings. The CLI emits a focused JSON document for any discovered
or imported Petri model, the local server exposes `POST /api/petri-analysis`,
and the frontend conformance tab provides the same controls with generation-safe
on-demand caching. Native, request, frontend, CLI, and hidden-server smoke
tests cover the current vertical. Karp–Miller-style coverability witnesses now
include a compact omega-marking graph, while bounded exploration exposes its
deterministic reachability markings and transition edges. Positive invariant
diagnostics expose the original, uniform-support, and relative-weight families
separately, including coverage gaps for each family and for S-components. These
projections are serialized by the CLI, local server, and frontend contracts.
Uniform-support vectors are retained only after rechecking the incidence
equation; when a single weighted certificate is not uniform, a bounded binary
invariant search recovers valid combinations of weighted rays. Weighted vectors
remain valid after normalization, and S-components retain the complete support
of each valid state-machine invariant (including a disconnected support when
the invariant itself spans it). The search has a shared caller-configurable
budget and reports exhaustion explicitly; an exhausted search never drives the
Woflan structural branch decision. A focused registered contract suite now
separately exercises the classic and extended equations, workflow validation,
the full Woflan pipeline, all three marking graphs, graph edges, structural
pairs, invariant bases, S-components, and uniform and weighted invariants. Exact
selector and module proofs tie those cases to distinct public declarations.

## XES importer-variant evidence

The native importer explicitly accepts `ITERPARSE`, `LINE_BY_LINE`,
`ITERPARSE_MEM_COMPRESSED`, `ITERPARSE_20`, `CHUNK_REGEX`, and `RUSTXES` (plus
their documented string aliases). These selectors share the native pull parser
with variation-specific date conversion. XML
trace and event order is retained by default. Optional timestamp sorting first
orders every trace and then orders traces by their first event, with a reverse
mode and stable ties. Empty traces are omitted only by the requested timestamp
sort, matching that transformation's result shape. `CHUNK_REGEX` ignores the
timestamp-sort request and retains input order. Sorting compares whole seconds
and fractional digits separately, preserving adjacent microseconds at distant
dates even when their double timestamps coincide. Trace caps apply in XML
order, while a scalar-attribute projection keeps selected generic keys without
discarding the case, activity, timestamp, resource, or lifecycle selectors
needed to form the native log. All selectors, the timestamp requirement,
ordering controls, trace cap, and projection are validated through the native
API, shared request parser, main CLI, cached local-server upload, and typed
frontend request builder. Positive fixtures cover all selectors, both sort
directions, document-order default, aliases, projection, and trace limiting.

Both XES exporter selectors share the same deterministic typed EventLog
contract and are available from the native API, export CLI, cached local-server
route, and frontend download controls. Binary-safe gzip output can be selected
explicitly or inferred from a `.gz` path. Import transparently recognizes gzip
headers and enforces a caller-configurable expansion limit. The native decoder
handles stored, fixed-Huffman, dynamic-Huffman, and concatenated members while
validating member checksums and sizes. Focused fixtures cover both exporter
selectors, typed metadata round trips, independently compressed dynamic-Huffman
input, corrupt trailers, expansion bounds, filename detection, and compressed
file adapters.

## Native event-data interoperability evidence

The traditional event-data layer now has four explicit native forms: trace
logs, ordered flat streams, rectangular eager tables, and restartable lazy
tables. A tagged non-owning view normalizes any form to the other three while
preserving row order, typed scalar kinds, log attributes, and shared metadata.
Lazy scans validate every row, restart from the beginning, and enforce an
optional hard materialization cap without truncation. Compatibility aliases
keep the original stream and table API source-compatible.

Table cells distinguish a missing column value, a present null, and a present
empty string. Optional stream post-processing removes only present nulls.
Conversions validate unique non-empty columns, rectangular rows, non-null case
glue, custom case selectors and prefixes, and stable type-aware case identity.
Extension, classifier, trace-global, event-global, log-property, and
trace-property metadata is carried by the native log model and retained by
log-preserving transformations. Synthetic variant projections deliberately
remain metadata-free.

The XML log reader and writer preserve extension declarations, quoted
classifier keys, typed trace/event globals, and selector properties in
deterministic order. Empty scalar strings remain values; explicit nulls are
omitted because the interchange format has no null scalar. Stored declarations
are retained, while canonical prefixes are generated only when undeclared. The
new stream post-processing, copy/compression compatibility controls, lazy
output selector, and row cap are exposed through strict request parsing, the
headless export CLI, bounded local-server queries, and typed frontend guards.
Focused native fixtures cover metadata cycles, interleaved grouping, typed case
identities, missing/null/empty distinctions, malformed tables, restartable
scans, and cap failures.

## Extended Petri-arc evidence

Petri transitions now retain ordinary, reset, and inhibitor input arcs as
separate deterministic lists. Reset firing clears every token in its place;
inhibitor firing requires an empty place. Forward/backward replay, alignment
search, reachability/soundness exploration, model footprints, and basic,
stochastic, and extensive playout all use these enablement and firing rules.
PNML round-trips annotate extended input arcs with `reset` or `inhibitor`, and
the analysis JSON and frontend model type preserve both lists. Linear marking
equations deliberately return a structured unsupported result for these
nonlinear arcs instead of claiming an unsound solution; focused native tests
cover both arc kinds, transport, replay, playout, and that guard.

## Petri usage comparison evidence

Element-usage comparison replays two logs on a selected or supplied Petri net
and emits deterministic place, transition, and arc records with both log
counts, totals, and signed direction. The CLI supports CSV/XES references and
inductive, Alpha, heuristic, and POWL model selection. The CLI also accepts
`--compare-petri-model model.pnml`, preserving the file's complete initial and
final markings, including zero markings and empty or disconnected general
nets. Explicit CLI markings override the corresponding PNML marking. The
general-net import mode is opt-in; workflow import defaults retain their
existing structural requirements. The local server accepts
the same reference as multipart `reference_log` against its cached session;
both adapters accept comma-separated initial and final place markings, while
the frontend exposes those controls beside the reference upload. Pre-event
hidden routing, post-trace final-marking routing, immediate-stop replay, and
token-flood cleaning
controls retain their established defaults and are selectable through native,
CLI, local-server, and frontend surfaces. An optional
trimmed activity selector applies to both replay logs; resource, timestamp,
and lifecycle aliases resolve to their first-class event fields, while the
CSV/XES import paths accept either short or qualified standard keys even when
the default activity column/key is absent. Custom CSV/XES keys are retained
through import. Replay uses normal, reset, and inhibitor arc semantics and
preserves weighted arcs in the usage records. Ordinary input/output, reset,
and inhibitor arcs are emitted separately with an explicit kind; the legacy
input projection remains true for every place-to-transition arc. Every element carries a
deterministic `(log1/log2/total)` label and a signed-direction color generated
from the public 256-bin blue-to-red palette; arc decorations mirror their owning
transition.
Place counts include consumed tokens and, for each place, the greater of
residual tokens and required final tokens, plus cleaning removals when enabled.
Final demand contributes even to an incomplete trace. Hidden final routing
stops once every required final place has a marking entry, including an explicit
zero-valued entry, even when its requested token count is larger. Immediate-stop mode stops before firing a disabled transition;
unknown activity labels are skipped, while missing selected activity fields
are rejected before replay, including fields after an immediate-stop event.
Present null values are skipped, and first-class projection
precedence applies to nulls and values alike. A forced weighted activation
adds the full input weight to each deficient place before consumption,
preserving a partially available token count. JSON directions retain double
precision. The static color helper exposes the separate five-color public
palette, including its asymmetric neutral interval `(-0.1, 0]`.

The public comparison capture and native PNML differential scripts are
`scripts/capture-petri-element-usage-reference.py` and
`scripts/check-petri-element-usage-native.py`. Element identities are
canonicalized by PNML IDs. A public weighted arc corresponds to native
unit-arc records with repeated endpoints; the checker verifies the exact
multiplicity and identical activation statistics before collapsing them,
without summing their firing counts. Native regression coverage additionally
checks reset/inhibitor arcs, selector aliases, malformed nets, and the
explicit silent-search budget. `comparison_module_contract` directly
exercises both native comparison entry points and JSON serialization.

The expanded public corpus contains 570 observations. All 566 cases mapped to
the native replay controls match, covering weighted deficits, silent routing,
incomplete traces, empty nets, zero/multiple markings, duplicate labels,
parallel joins, custom keys, and missing-key rejection. Four observations of
`disable_variants` and `consider_remaining_in_fitness` are explicitly skipped
by the CLI checker because these parameters have no native option. Registered
tests also check 2,766 dynamic-color and 2,013 static-color observations, plus
typed-null and alias-collision cases. The transport checker
`scripts/check-petri-element-usage-transports.py` verifies 128 server/CLI
combinations (four miners, CSV/XES, and every combination of four replay flags),
32 observable cleaning effects, two custom activity-selector cases, and ten
rejection paths. Comparison
CSV references opt into fallback between `case_id` and `case:concept:name`;
exact matches win and custom selectors remain strict.
The reviewed Release build, all 150 registered native tests, and the normal
catalogue verifier pass for this comparison increment.

The combined cleaning, component, final-routing, pre-event, and marking-key corpus contains
1,374 completed public observations, all of which match native output: 1,371
by element identity and three under an explicitly declared net automorphism.
The latter exchange equivalent silent branches in the six- and eight-stage
parallel fixtures. Fifteen additional bounded public calls, varying insertion
order and branch names, observed both choices; their complete usage records
agree after the branch exchange. The checker first attempts identity matching
and permits a declared exchange only after validating a bijection that preserves
node kinds, visible labels, the weighted arc multiset, and both markings.
Only element keys are renamed; every usage field remains checked. Nine focused
checker tests cover valid exchanges, invalid declarations, and count mismatches.
The checker reports these matches separately, and capture metadata distinguishes
declared equivalences from captured public results. Independent review found no
actionable issues in this normalization.

Token-flood cleaning is selectable through the native option,
`--compare-petri-cleaning-token-flood`, the server's `cleaning_token_flood`
query, and the frontend checkbox. It defaults to false. After a forced visible
activation introduces a new marked place in a structural component, cleaning
removes old places that remain marked in that component and records one
remaining occurrence per removed place, irrespective of its token count.
Increasing an already marked output does not introduce a new place. Enabled
firings, silent firings, and immediate-stop events do not trigger cleaning.
For a `p0-A-p1-B-p2` net and trace `B,A`, cleaning raises both endpoint usage
counts from one to two.

The independent component model matched 196 public observations across 28
nets and depth limits zero through six. The traversal saturates one-output
routes, retains prefixes, and explores each split output independently; its
default depth limit is six. It differs from Woflan invariant supports.
Multiple boundary marking keys disable components; an empty initial marking
otherwise rejects cleaning. Native component generation explicitly reports
its 4,096-expansion or one-million-stored-place resource limits.

Hidden final routing retains enabled prefixes of shortest structural paths,
including progress toward a join that remains disabled. It makes at most six
passes, with at most 51 silent transitions in each candidate path. Competing paths
prefer shorter routes and then place names. The supplied-PNML comparison route
preserves original place and transition IDs for these choices. Pre-event
routing separately targets deficient transition inputs, retains enabled
prefixes, and recursively supplies blocked joins. Candidate paths have at most
51 silent transitions, and a hidden transition is not repeatedly fired to
fill a weighted deficit during one event. The selected visible transition
remains fixed while that routing runs. Disabled duplicate-label candidates
use the greatest transition name; when several candidates are already enabled,
the reference can choose either according to its set iteration. Native replay
uses the least enabled transition name as a deterministic choice.
Native structural searches report their 4,096-place and one-million-stored-step
limits explicitly. Pre-event dependency routing reports its 4,096-call guard
separately from the observed public path and final-pass limits.

Pre-event routing first fires enabled path prefixes until a candidate yields
no new firing, then recomputes candidates and explores blocked dependencies.
Visited targets remain visited even if they never fire, and a blocked step
does not discard later steps of its path. Public helper observations support
two invocation levels by default and one with
`exhaustive_invisible_exploration=true`; the latter can leave more tokens
unrouted. This option defaults to false and is available in the native API,
`--compare-petri-exhaustive-invisible-exploration`, strict server parsing, and
the frontend comparison controls.

Native comparison and PNML documents preserve optional per-place initial and
final marking-key metadata, including explicit zero-valued entries. Omitting
comparison metadata derives keys from positive token counts; supplied metadata
must match the place dimensions and include every positive place. PNML input
preserves explicit entries, while explicit CLI marking overrides replace the
corresponding imported membership with positive-token membership. Firing removes
consumed keys at zero and retains untouched zero keys. Component selection,
cleaning, and final routing use membership independently of token quantity.
For a silent `p0-p1` route with a zero-valued final `p1`, an absent initial `p1`
causes routing, while an explicit zero-valued initial `p1` stops it. Removing a
stale zero key during cleaning contributes one occurrence. Forty targeted public
observations cover these distinctions, and the three formerly excluded component
models now participate in the combined comparison corpus. All completed cases
match native output, and no models remain excluded for marking representation.
Four CLI checks separately verify imported membership and independent numeric
initial/final overrides against public usage records. Independent review found
no actionable issues in the marking-key implementation.

The row remains partial. Source-transition and token-producing-loop reference calls that
exceed their execution budgets are unresolved, rather than expected errors or
passing comparisons: 18 cases remain unresolved in the combined corpus.
Fresh file-backed nested-dependency probes completed 12 separate comparison
calls and 55 direct replay calls without resource failures. The earlier nested
runner polled an undrained output pipe, so its timeouts did not establish
comparison nontermination. All 12 comparisons now match by identity, including
depths two through four, both stop policies, and both exhaustive settings.
`docs/petri-usage-nested-fixtures.json` retains these inputs and full public
results separately from the 1,374-case corpus. The direct replay observations
also match by identity, including prefired splits, shortened and padded paths,
linear tails, repeated calls, and both PLTR modes for the baseline cases.
The resumable capture script
`scripts/capture-petri-usage-cleaning-reference.py` preserves completed
observations and records unresolved cases separately, using one bounded child
per model with a 20-second and 512-MiB resident-memory limit, polled every 50 ms.
Resuming refreshes pending-case accounting without implicitly retrying unresolved
groups, and rejects nonfinite timeouts. The native differential report carries
that unresolved and excluded accounting.

## Concept-drift evidence

The native concept-drift surface explicitly selects the `BOSE` variant and extracts deterministic relation-type count
vectors from ordered trace sub-logs and evaluates adjacent windows with a
seeded permutation test. It supports configurable sub-log/window sizes,
permutation counts, p-value threshold, result cap, reproducible seed, and
optional case-start timestamps. Candidate boundaries are ranked by p-value and
sub-log index before the configured cap is applied. The public result then
partitions the original traces chronologically at those selected boundaries;
the final remainder is always emitted, while raw and cumulative sub-logs remain
available as diagnostics. Trace, event, and log metadata are preserved in the
partitioned logs. Activity, timestamp, and case-id selectors accept the standard
keys or generic event/trace attributes, and the selected keys are echoed in the
JSON response. CSV and XES import paths provide the tabular and event-log input
adapters. Strict variant/request parsing, standalone JSON, CLI, cached-session
server, complete frontend controls and response validation, and positive/no-drift
native regression fixtures are included.

## Trace-variant query anonymization evidence

The native trace-variant query surface supports both Laplace and SaCoFa
variants. It builds a bounded prefix tree with an explicit completion marker,
first-seen activity expansion, independent depth-wise noise, and immediate
pruning, so a removed parent cannot reappear through a later Cartesian
expansion. The completion marker also preserves empty trace variants. Completed
variants are emitted as soon as they survive; prefixes
that reach the configured depth are released only at that boundary. SaCoFa
classifies predecessor/follower relations as Always, Sometimes, or Never and
uses behavioral-appropriateness violations in its bounded exponential
selection before applying the same non-negative integer release noise. The
deterministic seed is an explicit testing/reproducibility extension. Both
privacy surfaces also accept an explicit secure-random switch that draws one
key from the host operating system entropy provider per release call; the
effective key is not returned in diagnostics.

Strict `tvq_*` request parsing, standalone JSON, CLI
(`--trace-variant-query`), cached-session server endpoints, frontend query
serialization, response guards, output caps, secure-random parsing, and focused
Release tests cover
both variants, explicit completion boundaries, intermediate pruning,
behavioral-appropriateness regressions, malformed privacy settings, pruning,
reproducibility, empty-trace completion, open-interval noise boundaries, and
non-negative release counts. SaCoFa diagnostics also report the cumulative
number of behaviorally violating candidates selected for noise, making the
bounded privacy decision observable without exposing the random key. The
PRIPEL-style adapter performs a deterministic one-to-one distance assignment
between query traces and source traces (using an exact bounded assignment
solver and a stable one-to-one fallback for oversized matrices, with
synthesized traces for unmatched queries), removes identity/blocklisted
attributes, preserves monotonic event time, and applies deterministic bounded
categorical/numeric release noise, with the same transformations optionally
keyed by secure OS entropy. Full differential edge-contract review remains
open.

## Target-transformation evidence

The native target transformation emits one numeric row per event, preserving
trace and event order. `NEXT_ACTIVITY` produces sorted or caller-supplied
one-hot classes and a zero row for each trace's final event; `NEXT_TIME`
produces seconds until the next timestamp under the `@@next_time` target
column; and `REMAINING_TIME` produces seconds until the trace's final timestamp
under `@@remaining_time`. Temporal variants require valid
timestamps (including timezone offsets and fractional seconds), while custom
activity/timestamp attributes, escaped comma-containing class names, and
deterministic zero padding are supported.

The three variants are exposed through strict `target_*` request parsing,
standalone CLI JSON (`--transform-target`), local-server
`POST /api/transform-target`, and the frontend query/result adapters. Empty
logs, explicit class lists, unknown classes, invalid timestamps, and padding
bounds have focused native, request, and frontend coverage.

## Log-utility and ML-preparation evidence

The native log-utility layer provides deterministic seeded trace splitting,
fixed-length prefixes, the complete ordered prefix expansion (including the
empty prefix and per-case change indexes), prefixes ending immediately before
the first occurrence of a selected activity with elapsed-time targets, and
activity-targeted sublogs that exclude competing target activities. It also
provides automatic event/case feature selection with mandatory, numeric, and
bounded-cardinality attribute handling. All projections copy log, trace, event,
and typed attribute metadata. Timestamp targets accept ISO timestamps and
finite numeric values, and invalid or reversed intervals are rejected.

`minera_log_util` emits one stable `LOG_UTIL` JSON envelope for CSV/XES input;
the cached-session `POST /api/log-util` route applies all six variants with
bounded output, and the frontend exposes sorted query serialization and strict
recursive response validation. Focused native and frontend tests cover seed
reproducibility, metadata preservation, empty/full prefixes, change-index
alignment, target durations, activity-target exclusion, feature-selection
cardinality rules, option validation, and malformed response guards.

## Trie-transformation evidence

The native trie transformation collapses duplicate activity variants into a
rooted prefix tree with deterministic lexicographic children, depth, terminal
markers, custom activity attributes, and an optional positive path-length cap.
Truncation marks the retained endpoint terminal, matching the language of the
bounded variants. Recursive JSON, strict request fields, standalone CLI JSON
(`--trie`), cached-session `GET /api/trie`, and frontend recursive response
validation are covered by focused native and adapter tests.

## Interval-tree transformation evidence

Open-path interval extraction selects the first later event whose start instant
is at or after the source completion instant, applies the configured epsilon
boundary, and retains deterministic source/target/case identity. It supports
custom activity and timestamp attributes and an exact activity-pair filter;
invalid timestamps, malformed filters, negative epsilon, and timezone offsets
are rejected or normalized explicitly.

The interval result is exposed through recursive JSON, strict request fields,
standalone CLI JSON (`--interval-tree`), cached-session `GET
/api/interval-tree`, and frontend query/response validation. Point queries
return inclusive interval indices, while range queries return every interval
overlapping the inclusive bounds; malformed, non-finite, partial, or reversed
query bounds are rejected consistently by native, CLI, server, and frontend
adapters.

## OCEL description evidence

The object-centric description family exposes both deterministic variants:
`VARIANT1` is the textual event/object representation and `VARIANT2` is the
tuple representation, with optional timestamps and lifecycle durations. The
native API preserves stable event and object ordering, the standalone
`minera_ocel --describe variant1|variant2` selectors expose the same envelopes,
and `POST /api/ocel/description` accepts multipart JSON with strict variant
and timestamp controls. Frontend upload/query helpers validate the textual
and tuple shapes, including timestamp-disabled tuples and malformed duration
fields.

## Object-centric directly-follows visualization evidence

Opening a standard object-centric JSON file now discovers its classic
directly-follows projection through the local endpoint and presents a bounded,
deterministic SVG view. The view aggregates activity and flow metrics across
object types, supports event/object-count and performance projections,
thresholds, rank direction, finite-sample aggregation, annotations, and
cycle-safe return edges. Geometry is pure and capped at 80 activities and 240
flows, so large or malformed metric payloads cannot start an unbounded layout
or produce non-finite SVG coordinates.

The object-centric explorer also switches the uploaded document between ETOT
and OTG projections without a second upload. ETOT renders activity and
object-type nodes joined by typed event-to-object edges; OTG renders object
types and its interaction, descendant, inheritance, birth, and death
relations. Both projections use lexical node identities, frequency thresholds,
cycle-safe return edges, rank-direction controls, and caps of 120 nodes and
360 edges. The graph layouts are pure functions with finite-coordinate
regressions, so changing projections cannot start a force simulation or depend
on browser timing.

## Case-relations merging evidence

The case-relations variation preserves the base log, inserts only incoming
events with a declared right-case relation, rewrites those events to the left
case, preserves duplicate relation rows, supports new left cases, and sorts
each resulting case deterministically by a configured timestamp attribute.
Sorting caches whole-second and nanosecond keys independently, preserving
adjacent nanoseconds, pre-epoch dates, and equivalent offset instants. Exact
ties retain base-before-incoming order and duplicate relation multiplicity.
`scripts/capture-case-relations-time-reference.py` reproduces the pinned public
ordering for modern/pre-epoch dates and default/custom timestamp selectors;
registered native regressions additionally cover offset normalization.
The case identifier can target the canonical trace id or a trace-level
attribute, and relation-column defaults are derived from configurable left and
right suffixes; explicit relation-column selectors still take precedence.
Malformed relation headers/rows, empty identifiers/suffixes, missing selected
case ids, and invalid timestamp text (including explicitly present empty strings)
are rejected. Missing and null
timestamp cells sort after timed events, retaining base-before-incoming order.
Duplicate relations expand each incoming event before advancing to the next
event, including all-missing timestamp ties. Selected custom timestamp fields
and native date payloads share the same exact ordering. The result
is exposed as recursive JSON, strict CLI/server/frontend controls, and a
multipart local-server endpoint with runtime response validation.
The EventTable overload preserves source columns and typed cells, including
relation fields and their collision suffixes. It sorts globally by case identifier,
then by the selected timestamp's actual scalar type, retaining nulls and duplicate
relations. Strings, including empty strings, sort lexically. A timestamp column
absent from one input produces missing cells; absence from both is rejected.
Typed relation tables support null joins and numeric/string identifiers. The
EventLog overload above retains its datetime-oriented compatibility contract.

Both-CSV CLI/server merges use preserved source tables and return an additive
`table` alongside the compatibility `log` projection. An omitted timestamp selector
can use a CSV timestamp alias present in either input; explicit selectors are
exact. CSV relation files retain all columns as string cells, including quoted
extra fields and explicitly selected relation headers. Configured blank CSV
timestamps remain missing; empty custom text fields remain present strings.
The compatibility log uses the selected case and timestamp columns even when
the source CSV contains other first-class columns. XES and mixed-format inputs
retain the EventLog route. CLI merge output
supports `-o` with checked flushing.

`scripts/capture-case-relations-table-reference.py` captures 43 public cases using
PM4Py 2.7.22 and pandas 3.0.5. Algorithm and PANDAS variant module bytes match
commit `0715c31f3f1d85924107d6aa5329806db69df899` after CRLF normalization, without
decoding reference implementation text. Per-cell scalar tags preserve booleans
and dates in mixed object columns. The capture includes and cross-checks the
16 observations from `scripts/capture-case-relations-time-reference.py`.
`docs/case-relations-reference.json.gz` stores complete inputs, outputs, column
dtypes, index labels, public errors and provenance. The generated native fixture
header checks all 43 cases for columns, row order, scalar kinds/values and native
validation rejection. Integral floats and integers compare by exact numeric value.
Offline tests reject truncated observations and inconsistent provenance or nested
timestamp evidence. `scripts/check-case-relations-transport.py` checks CLI and
HTTP behavior after native compilation finishes.
The checkpoint in `docs/case-relations-validation.json` passed all 150 native
suites, 53 script tests, six optimized evidence tests and 12 CLI/HTTP checks.
It records the changed native source hashes and unchanged CLI/server binary
hashes used for validation. The native fixture covers 36 successful tables and
seven validation rejections; quoted CRLF relation uploads have dedicated checks.

The table metadata checkpoint adds optional column dtype declarations and typed,
recursive index labels to EventTable and LazyEventTable. Identity/lazy conversion,
CSV projection and Rust XES import retain applicable metadata; JSON exposes
explicit index class, dtype, names and labels. Case merging carries base labels
and numbers expanded incoming rows before sorting. The 26 pinned public cases in
`docs/case-relations-index-reference.json.gz` cover integer, string, nullable,
float, datetime and tuple labels, empty inputs, cleared names and specialized
index classes, reversed/offset ranges, contiguous range concatenation and
range steps spanning the full signed 64-bit label domain.
Generated native checks compare exact index metadata as well as
output cells.
The validation record in `docs/table-metadata-validation.json` reports 150 passing
native suites, 58 script tests, 11 optimized evidence tests and 13 CLI/HTTP checks.

The dtype checkpoint adds column-aligned categorical descriptors (category dtype,
values and ordering) and exact unsigned 64-bit native scalars. The public capture
in `docs/case-relations-dtype-reference.json.gz` contains 390 observations under
PM4Py 2.7.22 and pandas 3.0.5: a complete ordered 18-by-18 sampled scalar dtype
matrix, typed empty/missing branches, categorical compatibility and category-code
sorting, datetime units, and signed/unsigned boundaries. The generator checks
pinned module hashes, selectors, input immutability and every captured cell tag.
Generated comparisons require exact scalar kind/value, dtype, category metadata,
row order and consistent numeric text. Validation recorded in
`docs/dtype-uint64-validation.json` passed all 150 native suites, 67 script tests,
18 optimized evidence tests and 13 CLI/HTTP checks. They intentionally equate native signed
and unsigned storage for the same nonnegative public integer, and absent/native
null cells for the public capture's normalized missing value. The separate index
fixture retains exact index checks.

`docs/xes-unsigned-reference.json.gz` records 24 public importer boundary calls.
Legacy importers preserve unsigned 64-bit values; Rust returns zero above the
signed 64-bit range. Python's legacy integers beyond UINT64_MAX remain unsupported
by the native scalar model. The public evidence includes that boundary as an
explicit limitation, not a native parity claim.

The family remains partial. These finite dtype observations do not cover every
pandas extension dtype, arbitrary precision integer, category object, timezone or
missing-value identity. Generated rejection checks require native validation
errors rather than exact Python exception class/message equality. CSV adapters
do not yet infer pandas column dtypes or expose the full typed/null identifier
surface, and XES/mixed adapters have not moved to the table surface. These finite
observations do not establish complete dataframe or family parity. Non-string
index names, specialized index-level metadata and range parameters not recoverable
from empty/singleton labels remain outside the current table metadata model.

## Dataframe formatting evidence

The native `format_dataframe(EventTable&, DataFrameFormatOptions)` operation is
based on public calls to `pm4py.format_dataframe`, rather than CSV-reader
inference. `docs/format-dataframe-reference.json.gz` retains 259 observations
under PM4Py 2.7.22 and pandas 3.0.5, including 26 public rejections. The
capture records the full input and post-call state, returned cells and dtypes,
categorical descriptors, index class/names/labels, attrs, and return identity.
The defining public module is checked against the pinned Git blob using opaque
bytes with CRLF normalization; implementation text is not decoded.

Generated native checks cover 258 representable cases, including post-call input
checks for failures. The non-string-column case remains a named exclusion:
EventTable models column labels and selectors as strings. Dataframe attrs map
to `EventTable::metadata.properties`, as confirmed by a separate public
format-and-convert call. Custom selectors validate required columns before
replacing/appending canonical copies in the input; the formatted return value
has its own sorted rows and reset index. Existing generated index columns are
overwritten, and mixed valid/invalid date text remains text when the reference
cannot convert the column. These observations do not establish every pandas
extension dtype, timestamp parser format, non-string label, or Python exception
identity. The generated rejection checks require native rejection, not Python
runtime exception classes. CSV and mixed XES adapters remain separate work.

The checkpoint additionally covers sequential cross-canonical selector aliases,
categorical source aliases, numeric sorting, and mixed microsecond/nanosecond
string columns. Object-typed numeric timestamps within the observed signed
nanosecond range become exact UTC dates; boolean/integer objects and unsigned
values outside that range retain their scalar kinds in the captured cases.
Mixed float/int/UINT64 sorting reproduces the observed categorical-uniqueness
rejection when distinct numeric categories collide after floating conversion.
Exact duplicates retain their first representative, which matters when boolean,
integer, and float values compare equal. Boolean/string mixtures prevent that
numeric category coercion, and numeric cells sort before strings. The check
applies across cases and preserves custom selector mutations before rejection.
The minimum signed nanosecond value converts to NaT and its event is dropped;
conversion failure leaves that integer intact. Infinity values retain their
sign and float type in the public evidence rather than becoming missing cells.

Explicit date formats validate Gregorian month lengths and leap years. Seven
calendar observations cover invalid dates, valid month ends, and the 1900/2000
century boundary; a single invalid value keeps the entire column as text.
A further 64 observations cover explicit `%f` fractional seconds and `%z`
timezone offsets, including nanosecond retention, truncation beyond nine digits,
negative/positive offsets, UTC day rollover, escaped percent directives, and
malformed input. Reference normalization discards offset seconds and fractions;
native formatting preserves that behavior. The observed ISO-shaped formats
accept empty fractions and hour-only offsets, while literal date formats reject
those spellings. This finite matrix does not establish every parsing directive,
locale, timezone name, or directive combination.

Another 68 observations cover default, mixed, ISO8601, and explicit date modes.
Empty text and exact `NaT`/`nat`/`NAT`/`NaN`/`nan`/`NAN` spellings become missing
dates only when the entire column converts successfully. Other capitalization,
whitespace, and invalid strings retain text. All-missing text date columns use
seconds resolution; typed dates ignore incompatible text formats. Named ISO8601
mode handles the captured timezone and nanosecond inputs.

The isolated Release build and all 152 native suites pass, including the 258
mapped public formatter cases. All 80 script tests and 13 optimized formatter
tests pass; independent review has no outstanding findings in this checkpoint.
`docs/format-dataframe-validation.json` records source/artifact hashes, the resolved
mixed numeric exception observation, and validation limits. Structural catalogue
coverage remains 44/86 dispatchers and 195/387 modules complete.

The typed JSON interfaces are `minera_format_dataframe` and
`POST /api/format-dataframe`. They accept declared cells, dtypes, categories,
indexes and recursive metadata, and return both the result and post-call input.
Formatting rejections preserve the changed input; malformed JSON is rejected
before formatting. The existing native log-conversion table serialization is
shared, including ordered globals and exact 64-bit scalar values. The decoder
checks numeric grammar/ranges, lexical/typed agreement, UTF-8, duplicate keys,
unknown fields, table dimensions, and size/depth limits.

All 258 representable observations pass independently through the CLI and HTTP
endpoint, with 12 additional file/error/metadata boundaries. Executables are
hashed before and after checks. `docs/format-dataframe-transport-validation.json`
records the complete case manifest, source/binary hashes and validation;
`docs/dataframe-format-interface.md` describes the request and response schema.


## Dataframe conversion metadata evidence

Public `convert_to_event_log` and `convert_to_event_stream` observations now
cover dataframe metadata mapping in 22 cases: 20 dataframe routes and two
existing-stream controls. The durable capture pins the public conversion and
formatter modules by opaque normalized bytes, records full dataframe inputs
and input-after state, and checks metadata plus output container lengths.
Generated native checks run each dataframe case through eager and lazy routes
for 42 total comparisons, preserving nested properties, custom case keys, and
empty typed schemas.

Dataframe attrs map to native metadata properties. Conversion removes exactly
`pm4py:param:case_id_key`, retaining ordinary `case_id_key` and all other captured
properties. Tables without native log-level attributes receive `origin=csv`.
Native tables can additionally carry log-level attributes, which dataframes do
not represent; those vectors remain intact when nonempty. Existing stream
conversion retains both its origin and reserved case-id property. Input data
and metadata remain unchanged.

The isolated Release build, all 152 native suites, 84 script tests, four
optimized evidence gates, and 258 formatter cases through each CLI/HTTP route
pass. Independent review is clear. The capture and validation are recorded in
`docs/dataframe-conversion-metadata-reference.json.gz` and
`docs/dataframe-conversion-metadata-validation.json`. This evidence is limited
to metadata and container lengths; it does not establish exhaustive conversion
cell, dtype, identity, or error behavior. Structural coverage counts are unchanged.


## Typed case grouping evidence

Seventeen pinned public event-stream conversions establish numeric case-key
equality across booleans, integers, and floating-point values. Native conversion
now groups equal values together, keeps the first typed case representative,
and retains input event order. Exact signed/unsigned keys avoid rounding large
integers through double precision. Positive and negative zero share a case;
strings remain distinct from numbers. Reused NaN scalar identities share a case,
while separately created NaNs remain separate. Native lexical numeric fields
also normalize for grouping without changing their stored labels.

`docs/conversion-case-grouping-reference.json.gz` records typed input keys,
scalar identities, typed output labels and complete event partitions. Generated
checks and native lexical fallback checks pass with all 152 native suites,
88 script tests, four optimized evidence tests and the existing formatter
transport matrix. Validation hashes are recorded in
`docs/conversion-case-grouping-validation.json`.

A separate seven-case capture in `docs/token-replay-case-key-reference.json.gz`
records an open boundary: the public direct-dataframe replay path can produce
fewer diagnostics than conversion followed by replay for large mixed numeric
keys. In particular, int64 max and floating-point 2^63 produce one dataframe
diagnostic but two converted-log diagnostics. This checkpoint corrects exact
stream grouping and its generic native normalization integration; it does not
claim parity for that optimized dataframe replay path. Date/container keys,
all hashability errors, and broader scalar identity behavior remain unproven.


## Basic random-tree generation evidence

The BASIC random-tree generator uses the seeded recursive-depth process-tree
contract: the root sequence has entry and exit activities, recursive operators
are uniformly selected among sequence, loop, XOR, and parallel, operator
child-count ranges are bounded, and loop exits are silent. Leaf probability,
minimum/maximum depth, seed, and deterministic activity labels are validated
and exposed through recursive JSON, standalone CLI output, cached-session-free
server GET, and frontend response validation. The PT-and-log generator also
preserves inclusive-choice nodes when the OR weight is selected; their
non-empty subset language is available through playout, Petri conversion,
POWL/alignment expansion, PTML, BPMN, JSON, and frontend rendering.

## BPMN inclusive-gateway evidence

## PTML atomic-interleaving evidence

The process-tree model includes a first-class atomic-interleaving operator.
Every child executes exactly once and each child subtree remains contiguous,
while the children themselves may occur in any order. PTML reads and writes the
`interleaving` element without approximating it as event-level parallelism.

The standard Petri conversion uses a compact subset-state construction, so the
model has one state per completed-child subset instead of enumerating factorial
permutations. Extensive playout enumerates permutations of complete child words;
seeded playout shuffles whole children. Alignment and POWL conversion use exact
permutation expansion with an explicit eight-child safety bound. BPMN export
automatically uses the exact converted workflow net. JSON and frontend tree
views preserve the operator, and footprints expose atomic interleaving as a
separate relation family from sequence and parallel.

Regressions distinguish the two legal block orders from an illegal event-level
mix, exercise all alignment variants, nested footprint propagation, PTML
round-trip, reduction, exact POWL language, JSON typing, and frontend rendering.

The BPMN importer recognizes block-structured inclusive split/join pairs by
graph structure rather than element identifiers. Each split branch must reach a
distinct incoming flow of one converging join, and every join input must be
accounted for; unmatched, shared, mixed-arity, and ambiguous gateways fail
explicitly. One-in/one-out inclusive gateways remain silent pass-throughs.

The workflow-net translation uses one selector and completion place per branch,
plus a two-state control token. A branch may be skipped or selected, the first
selected branch starts the control token, and the join waits for all branch
completions and a started token. This admits every non-empty subset and all
interleavings of selected branches without enumerating subsets. Direct
split-to-join flows represent silent branches and still require at least one
visible or silent branch selection.

Focused native regressions cover hand-written inclusive models, both branch
orders, subset rejection, silent branches emitted by tree export, nested
structure, malformed mixed-arity gateways, final-marking reachability, and
alignment fitness after a BPMN round trip.

Interrupting boundary events attached to tasks, call activities, and both
collapsed and expanded subprocesses are translated as alternatives to normal
activity completion. Expanded scopes carry a private lifetime token; an
interrupting path consumes it and reset-clears every child-scope place, while
normal completion requires the same token. Multiple attached events therefore
compete with one another and with normal completion. Non-interrupting boundary
events use a one-shot trigger token, fork their boundary path, and preserve the
activity or subprocess lifetime token so normal completion can still occur.
Named boundary events remain observable transitions; unnamed boundary events
route silently. Regressions cover
atomic, collapsed, and partially executed expanded activities, multiple
boundary alternatives, invalid references, stale-completion prevention, and
one-shot non-interrupting paths.

## POWL parity evidence

The POWL review is pinned to commit
`0715c31f3f1d85924107d6aa5329806db69df899`. Minera independently implements
the five selectable discovery behaviors, both filtering policies, filtering
threshold (including strict decreasing-factor boundaries and dynamic filtering
when its threshold is zero), frequency-order ratio, and a caller-controlled
brute-force activity limit. The common loop cut follows the reference behavior
for:

- start/end boundary union and removal of boundary nodes and incident edges;
- weak redo-component discovery;
- start/end reachability repair and completeness-based component merging;
- one merged redo group; and
- multiplicity-preserving, alternating contiguous do/redo projections.

Dynamic-clustering repair rounds use one canonical merge operation for reverse
cycles, choices, and equal-neighborhood groups: both source groups are removed,
their activity order is concatenated, and the result is appended before the
next deterministic sort. This keeps multi-round projected child ordering
stable across all dynamic variants.

Focused regressions cover ordered multi-activity redo behavior, different
start/end boundary activities, prefix/suffix false positives, incomplete
start- and end-side boundary reachability, split return components, single
closed variants, strict transitive sequence grouping with skip repair,
least-cost sequence projection, strict filtering boundaries, and dynamic
zero-threshold filtering, and concurrency grouping with skippable-branch
repair. Seeded black-box checks additionally matched 45 randomized logs for
each pinned selector. A projected one-activity regression proves that filtering
still runs for repeated or multi-variant logs instead of being bypassed by the
singleton base case. The activity-concurrent fall-through performs a cut-only
probe on its projected remainder: the probe runs the same cut validation and
projection checks as discovery, then stops before recursively constructing
child models.
This preserves the boolean fall-through decision while avoiding duplicate
recursive work; tree and dynamic variants exercise the path. Recursive POWL
JSON, request
validation, CLI flags, local-server mining, frontend controls/rendering,
converted-net conformance and decoration, and the PNML/BPMN/decision/playout
selectors are exercised by native tests, frontend tests/build, and headless
CLI/server smoke checks.

The live discovery implementation is separated into explicit base-case, cut,
fall-through, filtering, partial-order, variant, and selector-registry
components. Each of the 31 owned non-dispatcher catalog modules has a distinct
registered Release contract, and the four catalog selectors have separate
tests tied to their exact bindings. The additional plain dynamic-clustering
behavior remains a native extension rather than being added to the pinned
selector inventory. The POWL dispatcher and all 31 owned module rows are
therefore complete in the machine-readable ledger.

The frontend conformance surface also exposes ordinary Petri-net playout for
every discovered workflow-net selector, including POWL, with basic sampling,
extensive enumeration, explicit markings, bounded steps/length, seeds, and
optional visited-element JSON downloads. The basic and extensive POWL request
paths share the strict native response guards and deterministic API tests.

POWL-to-process-tree conversion is available through the native conversion API
and the headless `--powl-ptml` export. Direct native conversion retains its
deterministic approximation for residual partial orders; exported PTML uses the
workflow-net route shared with the server and rejects non-block structures.
Transition frequency bounds map to optional or silent-redo loop blocks;
connected partial-order components are layered by deterministic topological
levels and disconnected components become parallel branches. Ordinary sequence,
choice, inclusive-choice, parallel, and loop models round-trip through the
existing tree-to-POWL conversion; inclusive choices use a bounded XOR of
non-empty concurrent subsets because the POWL object model has no primitive OR
node.

Process-tree Petri conversion also exposes both construction variants. The
standard place-bordered form remains the default export path; the
transition-bordered form adds explicit silent fragment boundaries while
preserving the same workflow language for supported sequence, choice,
parallel, loop, activity, and tau nodes. The selector is validated strictly
and is available in the native API, standalone export CLI, `/api/pnml`,
`/api/bpmn`, `/api/playout`, alignment/evaluation/decision routes, and the
typed frontend API. `/api/process-tree-petri` provides a compact JSON net
envelope for clients that need the selected construction directly.

The reverse workflow-net conversion is also available through the native
`to_powl(const PetriNet&)` API. It validates designated source/sink reachability
and final-marking reachability; when neither boundary is assigned, it infers a
unique structural source and sink without mutating the caller's net. Equivalent
local places are normalized before recursive analysis, silent return boundaries
used by loop fragments are retained, deterministic silent routing wrappers are
stripped, and exclusive branches, do/redo cycles, sequence cuts, and residual
strict partial orders are recognized recursively. Labels and child ordering are
deterministic; extended-arc nets, ambiguous or partial boundary metadata, sink
escapes, malformed boundaries, unsound markings, and unresolved transition
cycles are rejected instead of being silently over-approximated. Focused
round-trip tests cover silent, visible, optional, unbounded, exclusive, loop,
sequence, nested choice, nested loop, concurrent, and diamond partial-order
models, plus inferred and duplicate-place workflow boundaries and invalid
extended, non-workflow, unsound, and sink-escape nets.

Workflow-net-to-process-tree conversion now uses a strict block reducer over
the reverse workflow reconstruction. Exact sequence, XOR, parallel, loop,
silent, and frequency blocks are preserved; residual non-series-parallel
partial orders and extended arcs are rejected. The headless export CLI accepts
`--pnml-to-ptml` and `--bpmn-to-ptml` without an event log, and `/api/ptml`
accepts a miner selector so the frontend can request PTML for any selected
workflow net when the strict conversion succeeds.

Ordinary Petri-net reduction now applies a deterministic fixed point of the
single-entry and single-exit silent-transition rules to generated process-tree,
heuristic, POWL, BPMN, and mixed reset/inhibitor nets. Conservative series-place
fusion handles private inputs with multiple producers, while duplicate silent
transitions, inert silent self-loops, and reset arcs guarded by the same
inhibitor are simplified when their token effects are provably unchanged.
Extended arcs that mention a candidate removed place block that local rewrite;
unrelated extended guards are retained and remapped. Source and sink places and
places carrying supplied initial/final tokens are retained by the ordinary
passes. The optional parallel-place rule fuses complete equivalent place
signatures (including ordinary arc weights and extended guards), preserves
equal supplied markings through representative aliases, and rewrites every
ordinary occurrence. The optional abstraction rule removes a private silent
fan-out bridge with multiple producers after checking target guard equivalence
and direct-route conflicts. Both broad rules are disabled by default because
their search cost is quadratic; callers enable them through
`PetriNetReductionOptions`. Place mappings and markings are returned for
callers that need to preserve workflow metadata, malformed nets are left
unchanged, and focused regressions cover chains, fan-out/fan-in, shared-place
guards, markings, mixed extended arcs, series fusion, parallel transitions and
places, abstraction, self-loops, invalid inputs, and deterministic output.

## Inductive discovery parity evidence

The infrequent Inductive Miner retries cut detection on a filtered graph only
after the unfiltered graph has no cut. Edge retention is strict (`count >
threshold * strongest outgoing count`), start activities use an inclusive
boundary, and every observed end activity remains available to endpoint-aware
cuts while its end count still raises the outgoing noise baseline. Empty-trace
retention compares the empty share with the total trace count and separates the
optional tau branch before recursive cut discovery; graph-recursive discovery
preserves its source skip flag independently of the threshold. The noise
threshold is now active only for the explicit infrequent selector; the standard
and graph-recursive selectors ignore it.

Log recursion now applies the specialized repairs in deterministic order:
activity once per trace, activity concurrent with a discoverable residual,
strict silent loop, general silent loop, and finally flower. Disabling repairs
retains empty-trace handling and the flower fallback, including the exact
silent-body loop shape for a repeated single activity. The native interface also
accepts weighted unique variants and all event-data views. Eager and lazy tables
require selected case, activity, and timestamp fields and are stably grouped and
ordered. EventLog callers can select alternate fields, regroup cases, and opt
into timestamp ordering; absent optional case or timestamp metadata preserves
the trace's existing grouping and order.

The same settings are exposed through strict request parsing, the command-line
exporter, local server, and frontend. Large concurrent-activity fall-through
searches can use a bounded parallel viability scan while retaining deterministic
candidate selection. The exact prepared Inductive log is reused for
conversion-dependent conformance, alignment, footprints, decoration,
evaluation, decision mining, process-tree reduction, stochastic fitting,
comparison, and Monte Carlo consumers. Discovered labels are registered before
recursive JSON serialization, including labels that exist only in a selected
activity field.

The production implementation now has explicit base-case, cut, data-structure,
fall-through, framework, selector-instance, and IM/IMf/IMd variant components.
Each of the 26 non-dispatcher catalog modules has a distinct registered Release
contract and each dispatcher selector has an exact binding proof. Focused
regressions cover selector dispatch, every repair family and its disabled policy,
strict-sequence behavior, input adapters, optional EventLog metadata, invalid
values, deterministic ordering, recursive JSON validation, transport
serialization, and parallel-search equivalence. A deterministic external
differential run covered 160 variant/option combinations without a mismatch;
a broader campaign found only reference-side hash-order variation and a
reference-side exception, with no stable invariant-level mismatch. The package
and all 26 owned module rows are therefore complete in the machine-readable
ledger.

## ILP discovery parity evidence

The bounded ILP discovery adapter now normalizes event attributes through the
selected activity key, folds duplicate variants while retaining their support,
and inserts the default `▶`/`■` synthetic boundaries as silent transitions.
Native callers can explicitly provide empty boundary labels to retain direct
source/sink wiring. The alpha sequence-encoding threshold uses cumulative
occurrence vectors, so continuations are compared after their complete prefix
rather than only by the immediately preceding activity; rejected continuations
cut the remaining suffix of that variant. The complete variants and activity
alphabet remain intact while weak sequence-encoding candidates are excluded.
Trace-bearing logs with no events retain their empty trace as a boundary-only
variant; a log with zero traces is still rejected. The standalone JSON and
frontend guard represent visible transition activities as indexes into the
activity table and reject indexes outside that table, while `null` remains the
silent-transition marker.
When no causal relation is supplied, the one-way portion of the normalized
directly-follows graph is derived automatically; callers may override it with
deterministic `A=>B` comma-separated entries. Frequency-based noise filtering
can remove low-support variants and their activities before region search. The
dependency-free binary region solver uses the same cumulative constraints and
objective as the classic formulation. It reports causal-pair solves,
branch-and-bound nodes, and exhaustion instead of claiming an unbounded
optimum. Supplied causal pairs are required producer/consumer constraints, not
an exhaustive arc list: the integer solve may add compatible producer or
consumer arcs when a composite region is needed to satisfy all retained trace
markings. Each supplied pair is optimized independently, with exact duplicate
regions removed after selection; explicit pair places use zero initial/final
markings so they survive workflow-net assembly. Arbitrary-width bit vectors
keep large activity alphabets usable, while `max_nodes` bounds the number of
causal-pair solves and the internal branch search has a separate deterministic
budget.

Region feasibility now follows the cumulative sequence-row formulation: the
producer mask is applied to the negative prefix before an event, the consumer
mask to the prefix including that event, and non-final rows require a
non-positive pre-fire slack while each fully retained final row is an equality.
The selected region minimizes the cumulative objective (`producer - consumer`)
over one row per retained variant and resolves equal scores by sparse
deterministic ordering (objective, arc count, then masks). Variant multiplicity
still drives support filtering, while the optimization rows remain one per
variant. This keeps final-row behavior independent across variants and lets a
composite place remain valid when alpha filtering removes a rare suffix from
its rows.

Constraint construction now canonicalizes repeated encoding rows while leaving
the objective contribution of every retained variant row intact. Thus repeated
loops and competing continuations cannot accidentally increase feasibility
constraints or change the objective merely because the same prefix was observed
more than once. Support accumulation is saturating at the unsigned counter
limit, and the inclusive alpha comparison is covered at and just below an
exact support boundary.

Each requested causal pair receives one deterministic binary integer solve. The
objective is optimized before sparse arc-count tie breaking; if a solve or the
pair-count bound is exhausted, any incumbent region is retained and the
diagnostics explicitly report the resulting partial optimization.

Strict request parsing, standalone CLI flags, the cached-session local-server
endpoint, and typed frontend query serialization cover activity/boundary keys,
alpha, causal edges, noise threshold, and safety bounds. Focused tests cover
default and custom boundaries, duplicate-variant support, cumulative-prefix
alpha filtering, causal priority, activity removal, deterministic bounds, and
malformed settings. Full numerical solver differential review remains open, so
this row stays partial.

## Genetic discovery parity evidence

The bounded classic genetic miner is implemented as a deterministic causal
matrix search with explicit population, generation, crossover, mutation,
elitism, activity, and candidate-state limits. Seeded evolution repairs
disconnected and inverse matrix projections, preserves partition grouping, and
converts compatible matrix places with exact reciprocal checks, removes covered
fallback edges, and adds deterministic visible-to-silent-to-visible routes for
non-simple matrices. Source and sink activity sets follow the matrix graph;
strongly connected candidates receive explicit entry/exit alternatives so their
cycle can still complete at the workflow boundary. Each mating step retains
reciprocal offspring, removes duplicate partition members, and updates both
matrix projections. Fitness is explicitly 0.4 token-replay fitness plus 0.6
fitting-trace coverage over the normalized log; diagnostics report the search
bound and best candidate without fabricating an optimum after exhaustion.
Optional strict timestamp and case-id selectors
resolve standard fields plus generic event/trace attributes, including stable
duplicate-timestamp ordering and deterministic regrouping by selected case.

The native result has a standalone JSON contract and strict request parsing,
CLI flags, cached-session server endpoints, and frontend types/query helpers.
Focused tests cover deterministic output, chain fitness, disconnected repair,
cyclic boundary completion, silent fallback routing, custom event/trace
selectors, duplicate timestamp ordering, parameter rejection, candidate caps,
and malformed frontend responses. The bounded adapter intentionally does not
expose a separate fitness-log artifact in the standalone JSON contract;
diagnostics remain scalar and the normalized log is used internally for scoring.

## Organizational SNA parity evidence

The organizational SNA vertical exposes the four log variants as a separate
deterministic contract: normalized directed handover and subcontracting
weights, symmetric per-trace working-together weights, and symmetric Pearson
joint-activities weights. Each variant is available through both the native
log path and explicit `_pandas` adapter selectors, with the same validated
semantics and deterministic output. Resource names and edge records are sorted
for stable JSON; blank resources are excluded from the resource alphabet while
empty or resource-less traces remain part of trace-count denominators. Handover
beta, subcontracting distance, optional metric normalization, custom
`sna_activity_attribute`/`sna_resource_attribute` selectors, and output safety
caps are strictly validated.

Native tests cover repeated resources, normalized boundaries, symmetric output,
degenerate correlations, truncation, and parser rejection across the four
`_pandas` selectors as well as their log counterparts. Standalone JSON, CLI
flags, cached-session server routes, and frontend query/result guards cover all
eight selectable variant names. Deterministic role discovery is also exposed via
the `ROLE_DISCOVERY` contract and `--roles`/`POST /api/roles`; both verticals
accept `*_activity_attribute` and `*_resource_attribute` selectors, resolving
standard keys from first-class event fields and custom keys from typed event
attributes; the handover, working-together, and subcontracting pandas-style SNA
variants also accept a case selector that stably
regroups events from trace or event attributes, while the log variants retain
their existing trace boundaries. Local diagnostics and network analysis are
documented below.

The organizational network-analysis vertical emits deterministic flat rows
(`NETWORK_ANALYSIS`) by linking each event to the earliest later event with
the same relation key. Defaults use case boundaries, resource nodes,
activity labels, and timestamp ordering; `_out`/`_in` edge references,
custom selectors, frequency/performance output, weekly business-hour slots,
and strict row/value bounds are available through the native API, CLI,
cached-session server route, and typed frontend helper. The catalog's
`DATAFRAME` selector is explicit in request/query metadata and is validated at
every adapter boundary. Role discovery likewise exposes both LOG and PANDAS
selectors with deterministic role ordering and a tagged `ROLE_DISCOVERY`
envelope. Focused registered contracts separately prove the dataframe path,
both role selectors, common role resources, both resource-profile projections,
the SNA utility result, and every log and pandas-style SNA selector.

The resource-profile vertical exposes the eleven scalar metrics through the
`RESOURCE_PROFILES` JSON family, `--resource-profiles` CLI mode, cached
`POST /api/resource-profiles`, and typed frontend query/response helpers.
Metrics use deterministic resource/activity rows, half-open timestamp slots,
configurable activity/resource/timestamp/start-timestamp/case-id selectors
(including typed custom event and trace attributes),
FIFO lifecycle pairing with atomic-event fallback, and explicit zero-safe
diagnostics for empty denominators. Workload and multitasking preserve the
reference interval policies; safety limits reject unbounded resource or row
output.

Local diagnostics now exposes the four group maps (`group_relative_focus`,
`group_relative_stake`, `group_coverage`, and `group_member_contribution`) for
both `org:group` event attributes and explicit role/clustering memberships.
The `LOCAL_DIAGNOSTICS` contract is available through native, JSON, CLI,
cached-server, and typed frontend query/guard adapters with deterministic
ordering, finite zero-denominator ratios, and bounded nested output.

Profile clustering now explicitly selects the `SKLEARN_PROFILES` variant and
has its own bounded `PROFILE_CLUSTERING` contract in
addition to trace-attribute clustering. Deterministic seeded k-means operates
over activity/succession one-hot profiles, explicitly selected trace/event
string or numeric attributes, or a caller-supplied feature list. Positive
iteration, trace, feature, and cluster bounds plus finite tolerance and typed
representation flags are validated by the shared request parser. The
seed controls deterministic weighted k-means++ center sampling, so repeated
runs replay exactly while changing the seed can select a different valid
initialization before Lloyd convergence.
`--profile-clustering` export mode, cached `POST /api/profile-clustering`, and
frontend query/result guard expose labels, clusters, feature names, inertia,
and convergence diagnostics without opening a GUI. Both clustering contracts
also accept `clustering_activity_attribute` / `profile_activity_attribute`;
standard activity, timestamp,
resource, and lifecycle fields and generic event attributes are projected
consistently into sequence values and profile feature names, with deterministic
errors for missing custom fields.

Trace-attribute clustering keeps the input order of attribute groups and of
equal-frequency variants before applying the five named distance families.
DMM vector matching computes activity and succession similarities separately
before applying `alpha`; empty succession vectors have maximal distance, and
traces without the selected grouping attribute are excluded. The DFG distance
uses complete group sublogs, so its result is invariant to the shared
`percent` control. Linkage children retain merge order while labels remain
deterministic. Focused Release coverage exercises these numerical and ordering
rules plus repeated serialization; broader differential fixtures remain.

## Evaluation parity evidence

Model evaluation covers token- and alignment-based replay fitness; token,
alignment escaping-edge, and aligned-automaton precision; token-frequency
generalization; arc-degree, extended Cardoso, and extended cyclomatic
simplicity; the composite evaluator with normalized fitness, precision,
generalization, and simplicity weights plus F-score; and stochastic-language
earth-mover distance. Automatic fitness
and precision selection uses workflow-net soundness, while every concrete
variation remains directly selectable. Precision exposes task-identity and
search-state controls; simplicity exposes its arc-degree target, reachability
bound, and explicit initial/final markings. Distance comparison accepts an
external CSV or XES log and independently controls mass normalization and
strict equal-mass validation.

Replay fitness, all three Petri precision variations, token generalization, and
composite evaluation accept common nonnegative initial/final marking vectors.
Those markings drive token replay, every ordinary alignment search variation,
automatic soundness selection, decomposed recomposition, aligned-prefix states,
and the composite submetrics. Empty vectors retain the workflow source/sink
defaults. Dimensions and token signs are validated before evaluation, including
for empty logs, and native callers receive explicit unsupported-variation
results instead of silently falling through to another evaluator.

The same evaluators also accept grouped logs, event streams, eager tables, lazy
tables, and the tagged event-data view. Activity and case selectors use exact
field, retained source, then canonical-alias precedence; an explicitly requested
case field replaces any stale first-class grouping role. Lazy and grouped inputs
honor the same optional materialization cap. Token- and alignment-replay summary
functions can aggregate already-computed trace records without rerunning the
model: absent records are excluded, token fit counts follow the explicit fit
flag, alignment fit counts require unit fitness, and alignment log fitness uses
the summed cost and best-worst denominator (including the nonempty zero-
denominator result). Checked aggregation rejects invalid values and counter
overflow without publishing a partially updated result.

Replay fitness and precision share a selectable activity field and forward or
backwards token replay. Forward token fitness can choose whether unknown activities
affect fitness. Alignment fitness defaults to less-memory Dijkstra with
standard evaluator costs and exposes all six ordinary search variations plus
decomposed recomposition, state/time bounds, dependent generator and discounted
controls, move costs, and decomposed cache and merge settings. Token precision
uses its metric-specific prefix-stop and final-marking policies internally;
alignment precision intentionally does not advertise an independent alignment
engine selector. Prefix-stop alignment consumes only synchronous visible moves
and silent model routing; non-replayable prefixes are omitted rather than
repaired with log or visible model-only moves.

The shared request parser rejects empty, malformed, non-finite, and out-of-range
settings, plus negative values for fields whose domain is nonnegative, before
evaluation. Arc-degree `k` deliberately accepts every finite baseline, including
negative values. Its structural arc count includes ordinary, reset, inhibitor,
and output arcs; extended Cardoso likewise treats all three input-arc kinds as
structural outgoing choices. Results have standalone and embedded
JSON forms, active-variation replay details, explicit completion/exhaustion
state, and finite-or-null metrics. Replay summaries include both stable native
fields and the established token/alignment spellings. Composite results place
the complete token replay-fitness dictionary under `fitness`, retain an additive
`fitness_score`, expose both weighted-average spellings, and include the other
three component scores, normalized weights, and F-score. The CLI evaluates every built-in Petri model
and an optional imported model; the local server evaluates any cached model and
accepts multipart reference logs. The frontend exposes the complete option set,
discovers models from actual nets as well as embedded results, and guards
on-demand caches/results across analysis changes. Native evaluator, request,
JSON, CLI, hidden-server, frontend-test, and production-build validation cover
the full vertical. A separately registered Release-safe contract executable
exercises every evaluator variation, automatic dispatch, activity selectors,
prefix rules, simplicity structure, composite aggregation, and both distance
execution modes.

Earth-mover evaluation defaults to `PYEMD`; the historical native `EXACT`
selector remains an explicit compatibility extension. Both languages share a
sorted union alphabet with one Unicode code point per activity, so encoded
distance callbacks remain collision-free beyond 256 activities. The default
cost is normalized Levenshtein distance over those encoded trace strings, and
the deterministic transportation solver reproduces the canonical distribution
fixtures while retaining arbitrarily small positive mass. Duplicate trace keys
are consolidated; invalid weights or costs and one-sided zero mass are rejected
deterministically. The accelerated setting normalizes unequal positive totals,
whereas strict or fallback execution retains the equal-total precondition;
normalization is reported in the result. The selected variant and execution
mode are carried through native, JSON, CLI, server, and typed frontend contracts.

## PNML document export evidence

`write_pnml` and `to_pnml` accept a complete `PnmlDocument`, retaining numeric
markings, explicit zero-token marking keys, original node IDs, weighted and
extended arcs, and optional stochastic metadata. Document export preserves
first-occurrence endpoint order when combining arc weights, so serialization
does not change the hidden-routing tie choices observed on the saved inputs.
The legacy net overload retains its sorted endpoint order. Empty nets export
safely. Metadata dimensions, positive-token membership, node IDs, and arc
indices are checked before output; generated IDs avoid node-ID collisions.
XML attribute whitespace is escaped numerically, keeping newline, tab, carriage
return, and literal-space IDs distinct. Stochastic values retain double precision.

`--inspect-pnml input.pnml --pnml output.pnml` writes the document and emits
inspection JSON with optional ID and marking-key arrays. The inspection option
`--pnml-require-workflow-boundaries false`, together with both marking-inference
options set to false, admits general and empty nets with their original markings.
`scripts/check-pnml-document-native.py` verifies nine saved public exporter
fixtures in `docs/pnml-document-export-fixtures.json`, including explicit zeros,
weighted multi-place markings, escaped IDs, whitespace IDs, and empty nets.
All 203 saved baseline/nested replay inputs retain their native trace results
after document export and re-import. Four focused native suites pass, including
stochastic metadata, reduction, collision rejection, XML whitespace, and routing
regressions. Independent review reports no remaining actionable findings.

The public exporter preserves explicit zero marking entries, while the public
importer drops them. These separately captured observations do not establish
importer parity: Minera's document parser retains the XML membership metadata.
Display layout and arbitrary PNML extensions remain outside this export
checkpoint. Supplied-model conformance propagation is described below.
No catalogue row is promoted.

## Supplied PNML model context

`--check-pnml` and `/api/check-model` retain the imported document's numeric
markings, explicit zero marking keys, original node IDs, and stochastic map.
Replay and element-usage comparison receive the numeric and identity metadata;
reachability, footprints, alignment, evaluation, analysis, and playout receive
the marking vectors supported by their APIs. Subsequent imported-model PNML
exports serialize the retained document. Replacing the cached model with BPMN
or PTML clears its PNML metadata, and failed imports do not replace the cache.

The supplied-model adapters infer a token only when the corresponding marking
has no keys and there is exactly one structural boundary. Explicit zero keys
suppress inference; ambiguous and empty nets retain empty markings. This is
an adapter compatibility policy, not a claim of public PNML importer parity.
The standalone replay/comparison routes keep their existing raw-document
import policy. Explicit marking overrides take precedence independently for
each vector; comparison overrides derive that vector's keys from positive
entries. Firing beyond the native token range raises an error before overflow.

`scripts/check-pnml-model-transports.py` covers weighted, multiple-place,
nonboundary, zero, omitted, ambiguous, empty, and named-duplicate inputs across
CLI and cached HTTP consumers. It checks document roundtrips, independent
marking overrides, BPMN/PTML replacement, discovered-model defaults, and token
range errors. Registered native regressions cover explicit footprints and
reachability, adapter inference and key membership, and firing at token limits.
The isolated checkpoint passes all 149 registered native suites and 26
transport checks. The catalogue audit passes; independent review's overflow
finding was fixed and reviewed again with no remaining actionable findings.

Evaluation APIs currently accept numeric vectors but not all identity/key
metadata. Stochastic-map log fitting now preserves replay context. Monte Carlo
automatic weight fitting now retains its supplied context as described below.
Decision mining, streaming replay, and BPMN/PTML conversion require separate context work.
These limitations remain open; this checkpoint does not promote catalogue rows.

## Monte Carlo fitting context and reference observations

`fit_monte_carlo_options` carries numeric initial/final markings, original
place/transition names, and explicit zero-token key membership into automatic
weight and duration fitting through the shared sparse stochastic-map fitter.
The context fields are appended to `MonteCarloOptions`. Signed conversion and
replay validation occur only when fitting is required; an explicit stochastic
map or both explicit vectors bypass fitting. A single explicit vector overrides
its fitted counterpart, and simulation-only unsigned markings retain their
existing range contract.
Imported CLI and cached HTTP simulation routes pass the complete context.
An explicit marking override clears only that vector's inherited key membership.

Registered Monte Carlo regressions compare fitting with direct forward/backwards
replay on duplicate and hidden branches, verify changed-marking effects, and
exercise invalid metadata, token ranges, and explicit map/vector precedence.
The transport harness verifies that original transition IDs change the fitted
branch, matching explicit-weight controls through CLI/HTTP JSON and XES playout;
one-sided marking overrides remain effective.
The isolated checkpoint passes four focused native suites, all 29 transport
checks, and the catalogue audit. Independent review reports no actionable findings.

## Stochastic-map fitting statistics

The native fitter now uses shared token replay with supplied initial/final
markings, original place/transition IDs, zero-token key membership, and replay
selectors. Unfit replay paths contribute statistics. Each firing counts the
currently enabled transitions and the activated transition; fitted weights use
activations divided by enablings. The returned map contains only transitions
encountered in those statistics, retaining their original native indices.

Timestamp queues follow causal token arrival, with FIFO consumption and one
timestamp per distinct input/output place even when numeric arcs have weights.
Initial tokens receive the first event time; hidden outputs have no timestamp.
Unknown events retain the observed source-cursor behavior, and negative samples
remain in the statistics. Forced EXPONENTIAL fitting maps empty/nonpositive
means to immediate transitions with priority one and positive means to rates.
Automatic and forced fitting follow the distribution evidence below; explicit
native fitting flags retain their documented policies.

Imported CLI and cached HTTP fitting preserve document context. An explicit
initial marking applies to fitting and CTMC analysis and clears inherited
initial key membership. The net-aware CTMC converter supplies defaults only
inside its dense option vectors; JSON and the frontend preserve sparse records.
The existing complete-map validator and PNML writer contract remain intact.

`scripts/capture-stochastic-fit-reference.py` and
`docs/stochastic-fit-reference-fixtures.json` retain 32 model/log cases from
public `get_map_from_log_and_net` and `single_element_statistics` calls against
the pinned reference. One identical-label/arc duplicate case also records the
alternative choice observed when public transition-set insertion order changes.
The checker accepts a complete observed outcome, never arbitrary mixed fields.
The native probe verifies map records and statistics; CLI/HTTP comparisons
cover nonempty logs, sparse output, hidden/weighted/multiple-token nets, negative
durations, unknown events, and an observable initial-marking override.

## Automatic distribution and Monte Carlo fitting

`fit_stochastic_distribution` and default log-map fitting now select among
normal, uniform, exponential, and immediate candidates by the observed public
likelihood rules. Empty or single-sample automatic inputs yield immediate
records. For two or more samples, automatic fitting rejects negative samples;
the immediate candidate requires every absolute sample to be strictly below
0.0001 seconds. NORMAL and UNIFORM forcing retain their fitted and immediate
candidates. EXPONENTIAL forcing uses a positive mean's reciprocal and otherwise
returns immediate. The observed remaining forced families return immediate;
their map representation and sampling support remain available separately.
Normal uses population standard deviation. Native uniform records retain lower
and upper bounds, with the public lower/width parameterization converted by the
reference checker. Nonfinite raw samples are rejected by the native API.

Causal replay queues now use integer microseconds, preserving the exact
0.0001-second boundary at contemporary epoch timestamps. Timestamp fractions
are padded or truncated to six digits; timezone offsets and negative epochs
retain their meaning. The existing double timestamp API remains available.

Signed normal means and ordered signed uniform bounds are valid map metadata.
When their mean cannot form a CTMC, CLI/HTTP output retains the fitted map and
returns an exhausted, empty reachability result with a CTMC diagnostic. Monte
Carlo conditions normal and uniform service laws on nonnegative values,
including signed normal means with positive deviations and uniform ranges that
cross zero. Laws without nonnegative support remain rejected by the native
bounded-execution guard. A complete explicit duration vector overrides unused
map durations; metadata and explicit vector validation still apply.

`fit_monte_carlo_options` now reuses the shared map fitter for causal performance
samples, activation/enabling weights, distribution selection, and complete replay
context. It returns a sparse fitted map; explicit maps and vectors retain their
precedence. CLI, HTTP, and frontend fitting options expose `automatic` for log
sources. Performance-DFG fitting keeps its explicit exponential default.

`scripts/capture-random-variable-fit-reference.py` and
`docs/random-variable-fit-reference-fixtures.json` retain 253 public raw fitting
outcomes. `scripts/capture-stochastic-distribution-map-reference.py` and
`docs/stochastic-distribution-map-reference-fixtures.json` retain 50 public
map outcomes, including two automatic negative-sample errors. These captures
record the pinned reference commit and NumPy 2.2.6/SciPy 1.14.1 runtime versions.
The checkers compare distributions, parameters, priorities, weights, statistics,
Monte Carlo fitted maps, and CLI/HTTP results where applicable. The existing
32-case forced-exponential matrix also checks Monte Carlo fitted maps.

The isolated checkpoint passes four focused native suites, 253 raw fitting
comparisons, 178 distribution-map comparisons, 124 existing exponential-map
comparisons, 29 supplied-PNML consumer checks, and 191 frontend tests. The PNML
branch check asserts generated AB versus overridden AC event paths even when
immediate durations make aggregate timing summaries identical. Independent
review reports no actionable findings; TypeScript and the catalogue audit pass.

Exact likelihood ties, extreme finite inputs, and broader parameter behavior
still require proof. The scheduler evidence below covers deterministic cases;
randomized choices and wider net/marking behavior remain open. Both catalogue
modules remain partial; these checkpoints do not promote coverage rows or
establish full parity.

## Monte Carlo scheduler and resource observations

`scripts/capture-monte-carlo-scheduler-reference.py` and
`docs/monte-carlo-scheduler-reference-fixtures.json` retain 70 calls to the pinned
public Monte Carlo API. The matrix covers single and multiple cases, simultaneous
and spaced arrivals, zero/one/two resource capacities, object and string resource
keys, hidden prefix/middle/suffix transitions, microsecond service times,
multiple initial tokens, weighted arcs, a deterministic parallel join, diagnostic
controls, and empty-output errors. Captures require `TZ=UTC` and record that
runtime setting so local timestamp conversion is reproducible.

The independently observed scheduler primes every case through its first firing
in generation order, then schedules continuations by completion time and case
index. Shared timestamp heaps and signed occupancy counters are separate from
each case's numeric marking. Initial arrival timestamps append before heap
operations; the captured positive initial markings initialize one timestamp on
the first initial place, irrespective of token multiplicity. Input consumption
and output capacity reservation update timing once per distinct place. Completion
releases occupancy for remaining marked places without removing timestamps.
Resources are reserved before input timing updates. One wait interval spans
the latest required resource availability, and completion is the later of that
availability and nominal service finish. The topology matrix below distinguishes
this rule from the earlier chain examples, where waits did not exceed service
finish. Finite intervals with nonpositive length are omitted. These rules also explain
the otherwise surprising intervals seen when cases arrive far apart.

Native simulation, CLI, and HTTP now follow those observations. Numeric weighted
arcs and per-case markings remain authoritative for firing; shared timing does
not transfer numeric tokens between cases. The bounded reset/inhibitor extensions
retain their marking behavior. Runtime limits account for shared timestamp and
interval growth, as well as dense per-case marking allocation independently of
resource capacity. Explicit stochastic maps and vectors retain their precedence.

`format_timestamp_microseconds` preserves simulation event precision, including
fractional rounding, negative epochs, and second carry. The existing millisecond
formatter remains available to other callers. Visible-event duration fields are
always computed, and the legacy elapsed-time vector still honors the diagnostic
flag. Zero resource capacities pass native, request, server, and frontend guards;
upper bounds and malformed-value rejection remain enforced.

The isolated checkpoint passes all four focused native suites, 140 CLI/HTTP
scheduler comparisons, 29 existing supplied-PNML transport checks, 196 frontend
tests, TypeScript, and the catalogue audit. Independent review reports no
actionable findings. Successful scheduler comparisons check every public result
field and generated activity/timestamp sequence. Four public error cases are
checked for failure on each transport; their exception classes/messages are not
claimed equivalent across languages.

Randomized branch selection, wider cyclic nets beyond the finite cases below,
platform-dependent local timestamp behavior, and wider parameter/error contracts
still require proof.
The native envelope also retains its explicit case identifiers and bounded
diagnostics. Monte Carlo remains partial, and the catalogue still has 234
incomplete coverage units.

## Monte Carlo marking-key membership

`scripts/capture-monte-carlo-marking-reference.py` and
`docs/monte-carlo-marking-reference-fixtures.json` add 68 public simulation calls
and 20 direct public Marking comparisons. The simulation matrix combines one or
three cases with reordered positive/zero initial keys, multiple marked places,
zero final keys, and empty final markings. PNML place order matches the captured
initial-key order. Empty final markings use an explicit all-zero native override
so the supplied-model adapter's absent-boundary inference does not alter the
comparison input.

The first initial key receives the arrival timing record even when its token
count is zero. Each case now retains key membership separately from token
counts: consuming a place's last ordinary token removes its key; producing a
token introduces its key. Completion requires every final key to exist, including
zero-token keys, alongside componentwise token coverage. Thus an initial marking
with source and middle tokens and a final zero source key can produce ABB where
count-only completion previously stopped at AB. Completion releases occupancy
for every remaining key, including a retained zero entry.

Existing PNML key vectors now govern simulation as well as source-log fitting,
including calls with explicit maps or duration/weight vectors. Their dimensions
and positive-token membership are validated before scheduling. Empty key metadata
derives keys from positive counts; explicit marking overrides clear the inherited
membership on only that side. Four captured override cases verify clearing the
changed side while preserving the other side's zero keys and timing behavior.
Original model IDs remain available to fitting.

The isolated checkpoint passes four focused native suites, all 136 marking
CLI/HTTP comparisons, the existing 140 scheduler comparisons, 29 supplied-PNML
transport checks, and the catalogue audit. Independent review reports no
actionable findings. Successful comparisons include every public result field
and each generated activity/timestamp sequence. Error cases establish failure
behavior only. These observations close the captured marking-key gaps without
promoting the broader Monte Carlo module.

## Monte Carlo source transitions and resource-constrained cycles

`scripts/capture-monte-carlo-topology-reference.py` and
`docs/monte-carlo-topology-reference-fixtures.json` retain 260 public simulation
outcomes. The matrix covers finite fuel-consuming cycles, inputless generators,
transitions without outputs, initial/final overlap, and repeated firings with
overlapping input/output places. One or three cases vary arrival spacing,
zero/one/two capacities, and zero or positive service times. Seventy-two cases
vary capacities across two output places and reverse output-arc declaration order
to check reservation aggregation. The capture uses the same pinned runtime and
UTC environment as the preceding scheduler evidence.

Ordinary transitions without inputs now participate in enabled selection. A
nonempty initial key set is required even when inputless transitions could fire;
a zero-valued initial key satisfies that requirement. Empty initial markings use
an explicit all-zero adapter override to avoid absent-PNML-boundary inference.
As before, reaching a final marking before producing a visible event fails when
the public result attempts to compute visible-event duration diagnostics.

Both default and per-place zero capacities resolve to one resource. A firing
reserves all output places before updating input timing. It produces one combined
wait interval ending at the latest reserved availability, even when multiple
outputs require waits. Completion is `max(nominal service finish, latest resource
availability)`; waiting does not add another service duration. All produced
timestamps use that completion before input-place timing is consumed and updated.
Numeric firing and marking-key changes remain case-local, and the existing
allocation and step bounds remain enforced.

The previous native checkpoint matched 97 of these 260 cases. The corrected
independent model matches all 398 saved scheduler, marking, and topology outcomes.
The isolated native checkpoint passes four focused suites, all 520 topology
CLI/HTTP comparisons, the existing 140 scheduler and 136 marking comparisons,
29 supplied-PNML checks, and the catalogue audit. Independent review reports no
actionable findings. Successful transport comparisons check every public result
field and generated activity/timestamp sequence; eight public error outcomes
establish failure behavior on both routes without claiming equivalent exception
classes or messages. The 825 transport checks close these observed topology and
resource-timing gaps; Monte Carlo and the 234 incomplete catalogue units remain
open for their broader contracts.

## Monte Carlo conditional normal and uniform service laws

`scripts/capture-monte-carlo-duration-reference.py` and
`docs/monte-carlo-duration-reference-fixtures.json` retain 14 laws with 2,048 raw
public random-variable draws and 2,048 public simulation timestamps each: 57,344
sample observations. Raw `get_value()` calls can return negative values;
simulation samples follow the same underlying distribution conditioned on
nonnegative service times. The matrix includes signed/zero/positive normal means,
uniform intervals crossing zero, positive intervals, and zero-width fixed laws.
Simulations use arrivals 100 seconds apart and verify that every observed service
time is below that spacing, preventing resource waits from changing the samples.
Captures pin both Python/NumPy seeds and the UTC local-time environment.

Native direct durations, request parsing, frontend serialization, and supplied
stochastic maps now accept these signed conditional domains. A negative normal
mean requires positive deviation. A negative uniform lower bound requires a
positive upper bound, and uniform widths and upper bounds must remain finite.
Normal sampling uses an independently implemented exponential proposal from
[Robert's truncated-normal method](https://arxiv.org/pdf/0907.4010), with stable
reciprocal and product arithmetic for extreme tails and a bounded rejection loop.
Signed uniforms sample directly from their nonnegative interval. Uniform maps
preserve their original positive upper endpoint before conversion, avoiding
cancellation when the negative lower endpoint is much larger in magnitude.
Explicit duration overrides continue to bypass unused map-duration conversion.

`scripts/check-monte-carlo-duration-reference.py` checks raw draws against their
untruncated CDF and simulation samples against the conditional CDF. Its thresholds
were selected before native execution: one-sample Kolmogorov-Smirnov distance at
most 0.065 and public/native two-sample distance at most 0.09 for 2,048 samples.
Fixed laws require exact timestamps. Identical native seeds must produce equal
durations across CLI/HTTP and direct/map modes. This evidence compares sampled
laws, not identical random streams across C++ and NumPy.

All 98 duration checks pass: 28 public distribution checks, 56 native transport
and parameter-mode checks, and 14 native seed-equivalence checks. The isolated
checkpoint also passes all 825 existing scheduler/marking/topology/PNML transport
comparisons, four native suites, 197 frontend tests, TypeScript, and the catalogue
audit. Independent review's uniform-bound cancellation finding is fixed and
covered by regression tests; no actionable findings remain. The relevant runtime
module bytes match the pinned Git objects after newline normalization, without
inspecting implementation text.

Entirely negative fixed or uniform laws can fail to terminate in the public
runtime; their native rejection is a bounded-execution policy, not an assertion
of identical reference exceptions. Extreme samples still obey finite-duration
and timestamp-range guards, and values below representable precision can round
to zero. The following checkpoint extends signed locations and zero-scale
contracts to gamma and lognormal laws. Wider distribution contracts, randomized
branching, and the previously named Monte Carlo gaps remain open. The catalogue
remains at 234 incomplete coverage units.

## Monte Carlo shifted gamma and lognormal service laws

`scripts/capture-monte-carlo-shifted-reference.py` and
`docs/monte-carlo-shifted-reference-fixtures.json` retain 20 gamma/lognormal laws
with 2,048 raw public draws and 2,048 public simulation timestamps each: 81,920
sample observations. The matrix includes signed, zero, and positive locations,
gamma shapes below one, and four zero-scale fixed laws. Eight invalid parameter
sets retain 16 public `ValueError` outcomes from raw draws and simulation calls.
Another 276 saved public SciPy CDF/survival points check the independent numerical
CDF implementation. Arrivals are 100,000 seconds apart; every observed service
duration is smaller than this spacing, excluding resource waits from the samples.

Direct gamma and lognormal durations now accept negative locations with positive
scale. Shape must remain positive. Zero scale returns the nonnegative location
exactly; negative scale and nonpositive shape are rejected. Request parsing and
frontend serialization use the same domains. Monte Carlo gamma maps additionally
accept zero scale with positive shape. Existing map formats have no location
field, and lognormal maps store `log(scale)`, so their finite representation cannot
encode zero scale. Transport evidence therefore checks shifted laws through direct
vectors and checks the seven representable zero-location laws through maps too.
Explicit duration vectors continue to override unused map-duration conversion.

Native negative-location lognormal sampling transforms a truncated normal draw,
using logarithmic excess calculations to avoid cancellation and intermediate
underflow. Conditional gamma sampling uses ordinary rejection near the mean, a
power-law/body and exponential-tail mixture for small shapes below one, and an
exponential tangent envelope farther into the tail. Rejection loops are bounded
at 10,000 attempts. Native regressions exercise conditional CDFs, small shapes,
extreme tails, zero-scale behavior, map equivalence, and override precedence.

`scripts/check-monte-carlo-shifted-reference.py` checks raw draws against the
untruncated laws and simulation samples against the laws conditioned on
nonnegative duration. The gamma CDF calculation uses the
[lower incomplete-gamma series](https://dlmf.nist.gov/8.11#E4) and
[upper incomplete-gamma continued fraction](https://dlmf.nist.gov/8.9#E2).
Preselected one-sample and public/native two-sample KS limits are 0.065 and 0.09.
Simulation one-sample checks account for the saved one-microsecond timestamp
rounding interval; fixed laws require exact durations and timestamps. Same-seed
native durations must agree across routes and available map/direct modes.
Four deliberately incorrect clamped or untruncated simulation-law controls are
rejected. This validates the stated sampled laws, not cross-runtime RNG identity.

All 154 shifted-law checks pass: 64 public-evidence checks, 54 valid native
transport/parameter-mode checks, 16 invalid native transport checks, and 20 native
seed-equivalence checks. The isolated checkpoint also passes the previous 98
duration-law checks, all 825 scheduler/marking/topology/PNML transport comparisons,
four native suites, 198 frontend tests, TypeScript, and the catalogue audit.
Independent review reports no actionable findings. Opaque runtime module bytes
match the pinned Git objects after newline normalization, without inspecting
reference implementation text.

Negative-location zero-scale laws remain rejected by the native bounded-execution
policy because public simulation calls may not terminate. Extremely large gamma
shapes near their mean inherit the standard-library sampler's floating-point
precision. Overflow, timestamp range, and rejection-budget failures remain
explicit errors; floating-point extremes are outside this sampled-law evidence.
Wider distribution/error contracts, randomized branching, general cyclic nets,
platform-dependent timestamps, and the previously named Monte Carlo gaps remain
open. The catalogue remains at 234 incomplete coverage units.

## Monte Carlo shifted exponential service laws

`scripts/capture-monte-carlo-exponential-reference.py` and
`docs/monte-carlo-exponential-reference-fixtures.json` retain 12 exponential laws
with 2,048 raw public draws and 2,048 simulation timestamps each: 49,152 sample
observations. Locations cover negative, zero, and positive values; scales range
from 0.001 to 10, plus exact zero-scale laws at locations zero and three. Three
negative-scale cases retain six public `ValueError` outcomes. Arrivals are
100,000 seconds apart, exceeding every captured service duration. Captures pin
the Python/NumPy seeds, UTC timezone, and PM4Py 2.7.22 reference revision.

Native direct duration vectors and the CLI/HTTP syntax now accept
`exponential:scale;location`. The existing `exponential:scale` syntax keeps its
zero location, so existing callers retain their service law. In C++ and the typed
frontend, `value_seconds` remains the scale and `secondary_seconds` carries the
optional location. The public `Exponential` constructor takes `(loc=1, scale=1)`;
those defaults are represented explicitly by `exponential:1;1`, without changing
the established native shorthand. Zero scale returns a nonnegative location
exactly. Negative scale and negative-location fixed laws are rejected.

For positive scale, the native law is the shifted exponential conditioned on
nonnegative service time. From the
[documented exponential density and location/scale transform](https://docs.scipy.org/doc/scipy/reference/generated/scipy.stats.expon.html),
conditioning a negatively shifted exponential yields an ordinary zero-location
exponential with the same scale. The implementation samples this law directly,
avoiding rare-event rejection and cancellation of a large negative location.
Positive locations are added to the sampled duration. A scaled unit-exponential
fallback avoids an overflowing reciprocal rate for subnormal scales. Native
regressions exercise conditional CDFs, exact same-seed equivalence for negative
locations including extreme cutoffs, zero-scale laws, and subnormal durations.

`scripts/check-monte-carlo-exponential-reference.py` checks raw draws against
their untruncated CDF and simulation draws against the conditional CDF. It uses
the preselected one-sample and public/native two-sample KS limits of 0.065 and
0.09, accounts for one-microsecond timestamp rounding, and checks fixed laws
exactly. Three deliberately incorrect all-zero, untruncated, or incorrectly shifted
simulation controls fail. Exponential maps store a finite positive rate and have
no location field, so map checks cover the three positive-scale zero-location
laws. Same native seeds must give identical duration vectors across transports
and representable map/direct modes; cross-runtime RNG identity is not claimed.

The previous checkpoint failed all 24 direct transport cases using the optional
location syntax. All 75 checks now pass: 27 public-evidence checks, 30 valid native
transport/mode checks, six invalid native transport checks, and 12 native seed
equivalence checks. The isolated checkpoint also passes the previous 154 shifted
gamma/lognormal and 98 normal/uniform duration checks, all 825 existing
scheduler/marking/topology/PNML transport comparisons, four native suites, 199
frontend tests, TypeScript, and the catalogue audit. Independent review reports
no actionable findings. Opaque reference-runtime bytes match the pinned Git
objects after newline normalization, without inspecting implementation text.

Negative fixed-law rejection remains a bounded native policy because a public
simulation can fail to terminate. Finite-duration and timestamp-range guards
still apply; sub-microsecond event times can round to zero. Wider error contracts,
randomized branching, general cyclic nets, platform-dependent timestamp behavior,
and the previously named Monte Carlo gaps remain open. The catalogue remains at
234 incomplete coverage units.

## Monte Carlo signed activation weights and branching

`scripts/capture-monte-carlo-branch-reference.py` and
`docs/monte-carlo-branch-reference-fixtures.json` retain 26 valid public cases,
each with 2,048 trace paths: 53,248 raw path observations. The matrix covers
unequal two- and three-way weights, all-zero weights, all-negative weights,
negative/zero forced choices, exact signed cancellation, subnormal weights,
priority permutations, sparse maps, a second seed, and sequential branching.
Sequential cases include opposite weight signs in different enabled subsets,
zero-total cancellation, forced paths, and three-way choices. Six invalid cases
retain public `ValueError` outcomes for negative normalized probabilities,
positive/negative sum overflow, NaN, and infinity.

Monte Carlo now accepts finite signed activation weights in explicit vectors
and supplied maps. Request parsing and the frontend defer probability validity
to simulation, because only transitions enabled together contribute to a choice.
For each enabled subset, the native simulator sums weights in double precision.
A zero total selects uniformly, including exact cancellation between opposite
signs. Otherwise, each probability is its weight divided by the total. Negative
or nonfinite normalized probabilities and a nonfinite total are rejected.
Thus all-negative weights select proportionally to their magnitudes, and a
negative weight with zero competitors selects that branch. The reference also
accepts extreme mixed signs when the tiny opposite-sign probability rounds to
negative zero; both captured examples select the dominant branch exactly.

Missing map records retain weight one, and complete explicit weight vectors
override map weights. Supplied priority values remain metadata and do not alter
selection in the tested `PETRI_SEMAPH_FIFO` cases. Generic stochastic-map fitting
and validation outside Monte Carlo retain their existing contracts. Native tests
cover signed/subnormal proportions, two-/three-way cancellation, sparse defaults,
overrides, finite metadata, and errors deferred until an invalid subset is enabled.

`scripts/check-monte-carlo-branch-reference.py` verifies both each stage's marginal
probabilities and the joint trace-path distribution. It rejects impossible paths
exactly, and forced paths therefore require exact counts. Preselected absolute
probability limits are 0.065 against the analytical law and 0.09 between public
and native path frequencies. Five incorrect-law controls fail: uniform selection
for unequal weights, priority filtering, negative-weight clamping, forcing the
positive branch after cancellation, and correlated sequential choices.

All 250 branch checks pass: 32 public-evidence checks, 156 valid native transport
and parameter-mode checks, 36 invalid transport/mode checks, and 26 native seed
equivalence checks. Native modes cover explicit weights, supplied maps, and
supplied maps with explicit weight overrides through both CLI and HTTP. Identical
native seeds must yield equal complete path vectors across these modes. Fixed
zero duration vectors isolate native branch selection; this evidence does not
assert public/native timestamp equality or identical RNG streams.

The isolated checkpoint also passes all 327 previous duration-law checks, all
825 scheduler/marking/topology/PNML transport comparisons, four native suites,
201 frontend tests, TypeScript, and the catalogue audit. Independent review
reports no actionable findings. Six relevant runtime modules match the pinned
Git objects after opaque byte comparison and newline normalization, without
inspecting reference implementation text.

Global finite-metadata validation remains a native policy, including unused map
records. Extreme cancellation can depend on floating-point addition order, and
native transition indices do not reproduce arbitrary public object-set ordering.
More complex branching, general cyclic nets, wider error contracts,
platform-dependent timestamps, and the previously named Monte Carlo gaps remain
open. The catalogue remains at 234 incomplete coverage units.

## Token-replay parity evidence

Forward replay now shares the independently observed structural routing,
token-flood cleaning, and marking-key rules with Petri usage comparison.
Forced weighted firing records the token deficit while adding the full input
weight before consumption. Immediate stop retains hidden firings already made
toward a blocked visible transition. Final routing retains partial paths,
limits candidate paths to 51 silent transitions and makes at most six passes.
Explicit zero-valued marking keys affect cleaning and final routing separately
from token counts. Cleaning configuration is initialized even for a zero-trace
log: an empty initial key set rejects cleaning, while a single zero-valued key
is accepted. Extended reset/inhibitor nets retain enabled-state silent routing,
including completion without an ordinary marked input.

`--token-replay-model model.pnml` emits standalone `TOKEN_REPLAY` trace JSON
after the normal CSV/XES import and log filters. It imports general nets with
complete markings, original element names, and zero-key membership, then calls
the replay API directly. The JSON retains ordered firings, event diagnostics,
full-precision fitness, and optional reached-marking key metadata. Cleaning is
available through `TokenReplayOptions.cleaning_token_flood`, the CLI's
`--token-replay-cleaning-token-flood`, strict request parsing, and the frontend
replay controls. It defaults to false and does not change backwards replay.
The exhaustive hidden-exploration policy follows the observed pre-event
dependency limits described above. It is exposed through
`TokenReplayOptions.exhaustive_invisible_exploration`,
`--token-replay-exhaustive-invisible-exploration`, strict request parsing, and
the frontend. It also defaults to false and is ignored in backwards replay.

`scripts/capture-token-replay-reference.py` captured 320 completed public calls
over 160 distinct inputs, separately using both `enable_pltr_fitness` settings
in fresh bounded children. All successful pairs returned identical complete
trace dictionaries. `scripts/check-token-replay-native.py` matches all 320
observations: 304 by identity, ten after adjacent swaps of independent silent
transitions, and six under declared net automorphisms. Swaps require disjoint
incident places and never move a visible transition. Automorphisms must preserve
node kinds, labels, weighted arcs, and both markings; invalid declarations are
rejected even when identity matches. All trace fields and aggregate place
consumed/produced/missing/remaining counters remain checked. Twelve focused
checker tests cover positive and negative equivalences. Twelve additional
zero-trace public calls establish cleaning initialization behavior.

Twelve exact-PNML calls, retained in
`docs/token-replay-forward-downstream-fixtures.json`, additionally confirm inclusive-choice silent skips,
interleaving duplicate choices, and activation counts used by Monte Carlo
fitting. Native results match all trace fields and PLTR place counters by
identity. The conversion tests now use exact alignment to verify accepted
languages independently of greedy replay; separate replay regressions retain
the observed deficits and firing choices. The full 150-suite native run passes.

`docs/token-replay-forward-nested-fixtures.json` retains 55 additional public
observations. All match by identity after the shared hidden-routing fix,
including the partial markings at depths three and four. Eighteen direct
public-helper observations and 79 replay observations match the independent
routing prototype used to distinguish visitation, recursion, and path passes.
Thirteen native regressions retain exact activation order, markings, token
counters, fitness, and stopping behavior; nine also check independently captured
comparison place and transition usage.

The checkpoint passes all 150 native suites in the integrated worktree and
seven focused suites built from the isolated staged source. The isolated build
also matches all 387 completed replay observations and 1,386 completed
comparison observations, retaining the 18 unresolved comparison calls
separately. Its frontend passes 309 focused tests and the production build.
The expanded comparison transport matrix passes 256 server/CLI comparisons,
two custom-selector checks, and 14 rejection checks; standalone replay checks
cover CSV/XES equivalence, filtered/custom activities, file output, empty-log
initialization, strict exhaustive booleans, and retained backwards behavior.
Independent review reports no actionable findings, and the catalogue verifier
passes without coverage promotion.

Downstream precision/evaluation proof remains partial. Supplied-model marking
propagation and its remaining consumer limitations are recorded above.
This checkpoint does not promote any catalogue row.

The backwards token-replay variation independently follows the pinned legacy
behavior: unknown labels are ignored, same-label transitions are evaluated
sequentially against the evolving marking, and a blocked unique transition
searches backwards through silent transitions before weak firing. Reverse
routing uses the reference's global transition-visit rule and weighted-marking
difference semantics. Exact final-marking deficits and surplus tokens feed the
legacy fitness accounting, including its actual-token consumption denominator
at final-marked places. Both forward and backwards replay accept arbitrary
nonnegative initial/final vectors; forward replay consumes the requested final
token multiplicities, while the legacy variation preserves its distinct final
accounting.

The variation and immediate-stop setting are exposed through the native API,
strict request parsing, CLI, local server, and frontend controls. Focused
regressions cover perfect replay, multi-step silent routing, forced missing
tokens, immediate stop, unknown labels, duplicate labels, weighted arcs, and
empty logs.

Per-case replay diagnostics now retain one event record for every examined
source event. Each record carries its zero-based event position, ordered
silent/visible firings, and problem transitions. Unknown events retain empty
records; unexamined events after stopping have no record. Forward immediate
stop records the blocked transition without claiming it fired, while backwards
weak firing retains both the firing and its problem. Backwards duplicate-label
firings share the source event that triggered them. Final silent routing stays
in the trace-level firing list because it has no source event.

Native contracts check that concatenated event firings form a prefix of the
trace firing list and that event problems reproduce the complete trace problem
list, including repeated deviations. JSON exports and the frontend expose the
same records, preserve older exports without them, and disclose the 100-event
display cap. The September 6 checkpoint passed all 146 Release CTest suites,
563 frontend tests, the production frontend build, and the pinned structural
audit with its verifier self-test. These records prepare the duration and
root-cause support work; they do not complete those modules or establish exact
forward routing/flood-cleaning parity. The 235 incomplete coverage units remain
unchanged at that checkpoint.

## Replay-duration support evidence

The duration support module now has independent native implementations for case
duration, median case duration, transition cohorts, and unknown-activity cohorts.
The observed contracts are recorded in `docs/token-replay-duration-fixtures.json`
and exercised by the registered `minera_token_replay_duration_tests` executable.
Public API descriptions identify these four operations and their timestamp
parameter; pinned 2.7.22 black-box calls establish the edge behavior independently
of implementation source ([API documentation](https://pm4py-source.readthedocs.io/en/latest/pm4py.algo.conformance.tokenreplay.diagnostics.html)).

Case duration is the last event's timestamp minus the first event's timestamp in
source order, with fractional seconds and negative values retained. Interior
timestamps do not affect it. Empty cases, missing/null/invalid endpoint timestamps,
and empty median inputs fail explicitly. Medians use the ordinary middle value
or arithmetic mean of the two middle values. The standalone median helper
preserves repeated samples; cohort membership deduplicates repeated trace indices
while keeping distinct log occurrences, including duplicate case identifiers.

Transition comparison baselines contain every case with the selected transition
label except cases in that transition's supplied problem cohort. They do not
depend on recorded fitting firings or a supplied fit list. Supplied problem
membership remains authoritative even when the case lacks the transition label.
Unknown-activity baselines contain cases with none of the supplied diagnostic
labels, including labels whose own problem cohort is empty; each row additionally
excludes its own problem cases. Other rows' problem membership alone does not
exclude a case. A row is omitted when either cohort is empty. Both helpers return
problem median divided by fit median only when the fit median is positive, and
zero otherwise. Missing activity values do not match explicit null labels.

The generic filter-attribute parameter and timestamp parameter have native
equivalents in `TokenReplayOptions.activity_attribute` and `timestamp_attribute`.
Standalone cohort APIs accept explicit source-index memberships, so the support
contract can be tested independently of the replay routing algorithms. The
integrated adapter builds memberships from examined replay events and measures
the complete source case even when replay stops early. Its source-index lists,
counts, medians, and ratio retain distinct null and empty-string activity labels.

`include_duration_diagnostics` is off by default. The native result, shared JSON
writer, CLI flags, cached local-server conformance, and typed frontend controls
carry the same optional result. Duration and trace-detail opt-ins are independent;
enabling either does not change replay metrics. The frontend reports comparison
counts and medians, explains the zero-ratio rule, accepts older exports, and
discloses its table display caps. This closes one support-module record and leaves
234 incomplete coverage units. It does not complete either replay variation,
root-cause analysis, or downstream evaluation packages that still depend on exact
replay behavior.

This checkpoint passed all 147 Release CTest suites and 570 frontend tests,
the production frontend build, and headless CLI/local-server checks for both
replay directions, imported models, a custom timestamp selector, opt-in payloads,
and rejected malformed options. Windows error dialogs were suppressed and builds
used no more than two workers.

## Decision-mining evidence

Decision mining now has a guarded data-Petri-net projection in addition to the
decision-tree diagnostics. Each observed branch path becomes a deterministic
guard and read-variable set; transitions consuming multiple decision places
combine those rules conjunctively. The native API, standalone
`minera_data_petri`, cached local `POST /api/data-petri-net`, and typed frontend
request/response guard expose the same `DATA_PETRI_NET` envelope. The native
`mine_decision_tables` API and `minera_decision_table` command emit deterministic
numeric and one-hot columns from both clean replay and alignment-backed model
choices. Deviating variants use their optimal alignment; model-only moves do not
consume a log event, while synchronous and log-only moves advance the context.
The alignment path retains hidden transition ids for this extraction even
though ordinary conformance display continues to omit silent moves.
The response reports replayed, aligned, and alignment-failed trace counts, and
never fabricates rows when the net is unsound or a bounded alignment cannot be
computed. The standard
export accepts `--decision-tables`, the cached server exposes
`POST /api/decision-tables`, and the typed frontend request validates the same
encoded-table envelope. Decision depth, split/leaf minimums, candidate caps,
gain threshold, trace-attribute inclusion, selected context attributes, and the
event activity field used for replay are validated and shared by the native,
CLI, server, and frontend decision paths.
The same controls flow into guarded data-Petri projections and their nested
tables. Encoded tables retain a deterministic rolling window of the last
visible-event context snapshots before each choice (bounded to 64; the
default k=1 behavior remains the latest snapshot). The window advances only
after visible events, and an empty feature map still occupies its event slot;
each snapshot is captured before the choice is consumed so post-choice
attributes cannot leak into its row. `--decision-lookback`, `decision_lookback` on the table and
data-Petri endpoints, and the typed frontend table options share the same
strict 1..64 validation; the ordinary decision endpoint intentionally does
not send or accept this table-only parameter. Richer classifier backends and
columnar training integrations remain open.

## Contextual label-splitting evidence

Contextual splitting now builds one segment node for each distinct
`(activity, prefix+suffix context)` pair while retaining the first-seen order
of those nodes. Context similarity is measured over normalized activity tokens
rather than the characters in their labels, so multi-character names cannot
accidentally collapse into one context. Weighted edges use the configured
strict minimum, and one global greedy-modularity graph accumulates
cross-community edge weight across all selected activities. Isolated vertices
remain singleton communities, zero-gain merges are accepted, and clustering
stops at the first negative gain. Community numbering follows occurrence
support with the graph's deterministic merge order as its stable tie break.
Custom activity and target columns, target-activity selection, context bounds,
suffixes, strict edge thresholds, and malformed numeric settings continue to
share the native/request/CLI/server/frontend contract. Focused Release
coverage includes token-boundary distance, weighted competing gains,
zero-edge graphs, zero-gain paths, deterministic ties, cross-activity global
normalization without cross-label edges, custom-column preservation, and
threshold boundaries.

## Live-stream transport evidence

Native live event and trace streams expose inactive, active, and finished
states, accept items before activation, dispatch through a bounded worker pool,
and drain accepted items before stop returns. Observer registration and
deregistration are synchronized, observer exceptions are isolated, and a
finished stream cannot be restarted or appended to. Focused Release tests cover
queueing, drain-on-stop, observer failures, invalid pool sizes, and lifecycle
transitions; the headless `minera_live_stream` command exercises CSV event
delivery end to end.

`CsvEventStreamReader`, `XesEventStreamReader`, and `XesTraceStreamReader` are
restartable readers with transformation and acceptance callbacks. They expose
single-item reads and direct delivery into the live event or trace stream;
focused tests cover filtering, reset behavior, XES trace grouping, and complete
delivery counts. The headless `minera_stream_import` command exercises both
event and trace modes without a GUI.

`PersistentStreamingDfg` adds a process-restartable, append-only CSV state file
for incremental directly-follows discovery. Reopening the same file rebuilds
activity, boundary, edge, and open-case state; `reset()` truncates it back to a
validated header. The headless `minera_streaming_dfg` command accepts
`--state STATE.csv` for this mode. Focused Release coverage includes
interleaved cases, quoted identifiers, multiline activities, restart replay,
reset, and invalid-state rejection. The file is a local restart checkpoint;
the standalone `minera_streaming_store` command now exercises deterministic
classic, mutex-protected, and remote dictionary variants from a line-oriented
script. Local stores preserve sorted key/value views; the remote store uses
bounded connection and response limits, optional database selection, and a
prefix-isolated `clear()` operation, while explicit database/all flush commands
remain available for administrative use. Focused tests cover CRUD ordering,
parallel writers, validation, and unavailable remote endpoints without opening
any interactive process.

## Log-skeleton parity evidence

Classic log-skeleton discovery exposes all six pinned constraint families:
equivalence, always-after, always-before, never-together, directly-follows,
and activity frequency. It preserves weighted trace variants, the reference's
event-occurrence denominators, positive-only counter subtraction, first-seen
frequency tie ordering, repeated adjacent-edge multiplicity, and the
event-count noise threshold. Discovery always computes all six families;
considered constraints apply only to conformance. Direct native callers retain
the pinned raw threshold arithmetic; CLI and server boundaries
strictly validate the conventional finite `[0,1]` range.

Classic conformance supports activity-list, trace, and whole-log entry points,
selected constraint families, source-absent vacuity, unknown and required
activity-frequency diagnostics, per-trace deviation totals, fitness, and fit
status. Omitted constraints mean all families, while an explicitly empty list
performs no checks and is distinct from a non-empty subset. Discovery and
conformance accept the same strict activity selector contract as Declare.
Stream, eager-table, lazy-table, and generic event-data adapters use an
independent case selector for grouping. Exact fields outrank imported source
keys and canonical aliases, equal-priority collisions reject, and an exact null
never falls back to a lower-priority field. Grouped logs ignore this flat-only
case selector.

The explicit variant-list discovery contract materializes one trace per list
entry: the attached count is metadata, duplicates remain repeated entries, and
empty variants remain representable. Variant conformance returns one result per
distinct activity sequence with later duplicate keys replacing earlier ones.
Typed diagnostics preserve the selected case cell's scalar kind and native
value, validate result cardinality, and reject totals outside the signed 64-bit
table range. Focused tests cover repetition-sensitive relations, noisy weighted
variants, threshold and tie quirks, family selection, duplicate and empty
variants, selector collisions/nulls, typed case IDs, unknown/missing activities,
non-finite direct-call behavior, and empty inputs. The model and trace
diagnostics are serialized with prevalidated activity IDs, available through
CLI/server requests, and rendered in the frontend with all settings exposed.

## DFG statistical and causal parity evidence

The statistical DFG surface exposes NATIVE, FREQUENCY, and FREQUENCY_GREEDY
as individually named aliases of the same windowed counter. A zero window
counts one self-pair per event, positive windows connect exactly the two
positions at that distance, and optional per-case deduplication is applied
independently inside each trace. FREQ_TRIPLES counts overlapping contiguous
triples and intentionally ignores the window and deduplication settings.

PERFORMANCE and PERFORMANCE_GREEDY are named aliases over adjacent waiting
times. Omitting the start-timestamp selector uses the target's completion
timestamp; an explicitly named missing selector is rejected. Negative waits
clamp to zero. Mean, median, minimum,
maximum, sum, sample standard deviation, raw observations, and the complete
summary are selectable. Timestamp parsing is strict. Weekly business-hour
slots support the pinned Monday-Friday default, caller-defined ranges,
timezone-bearing timestamps, explicit empty schedules, one-second-gap merging,
and multiweek intervals. The opaque work-calendar parameter is accepted and
preserved but intentionally has no effect because the pinned calculation does
not consult it.

CASE_ATTRIBUTES counts every requested trace attribute on directly-following
edges, maps `concept:name` to the native case id, skips missing attributes, and
optionally returns per-event node annotations. CLEAN preserves the pinned
columnar algorithm's unusual boundary rule: start/end counts include every
non-empty case, while only a non-boundary current row owns an edge to its next
row. CLEAN_POLARS retains every within-case adjacency and derives boundary
starts from the globally sorted row stream, including the columnar first-row
compensation. CLEAN_TIME computes each activity's mean offset per case before
averaging those case means.

Causal ALPHA emits a relation only when its positive reverse is absent.
Causal HEURISTIC emits both directions with
`(forward-reverse)/(forward+reverse+1)`, including synthesized zero-reverse
entries and the pinned self-relation assignment behavior. Integer differences
and sums are formed before floating conversion, preserving near-maximum-count
signals; invalid native selector values are rejected. Focused tests cover
all aliases, windows including zero, deduplication, triples, every performance
aggregation, timestamp fallback and rejection, custom business hours, missing
case attributes, node counts, CLEAN boundary/tie behavior, CLEAN_TIME repeated
activities, zero-frequency reverses, self loops, maximum-count arithmetic,
custom projected labels, and lossless causal JSON values and metadata checks.

Every variant remains individually selectable through the native API, strict
request fields, CLI, and local server. Deterministic JSON resolves all
activities against the shared registry and preserves alias names; the frontend
exposes all settings and renders frequency, triple, performance,
case-attribute, clean-time, and causal artifacts. Native statistical counts
remain unsigned 64-bit values. The JSON/frontend boundary rejects counts above
9,007,199,254,740,991 so numeric values remain exact after JavaScript parsing.

The statistical dispatcher routes and thirteen non-dispatcher rows now carry
machine-checked implementation and registered-test proofs. Focused contracts
separately exercise selector routing, frequency-triple and clean adapters,
clean and clean-time boundary behavior, the in-place edge-value replacement
contract, and the complete native graph-utility surface. The utility API covers
weighted incoming/outgoing maps, inferred and parent-aware boundaries,
outside-boundary projections, endpoint and incident totals, activity
projection, reciprocal removal, direction ordering, self-loops, connected
components, deterministic strongest-component assignment, direct neighbors,
dense matrices, distinct and weighted variant summaries, directed graphs, and
cyclic transitive predecessor/successor closure. Canonical native maps replace
the source runtime's multiple container shapes; missing maxima use
`std::optional`, while invalid adjacency rows and empty component assignment
remain explicit errors.

The dataframe statistics proofs cover global-shift and within-case-shift graph
behavior, target-side selectors, cost nulls, custom aggregation, sorting and
window zero, explicit and default schedules, partial-order first-following
selection, and the adapters' intentionally different concurrency boundaries,
null handling, and index rules. Native pair vectors preserve complete typed
events and expose configured helper-column names as result metadata instead of
returning a language-runtime joined dataframe. Lazy table inputs currently
materialize into the value-semantic table engine before discovery; this affects
memory usage, not results. With those native representation differences stated,
the DFG dispatcher, causal dispatcher, and their tracked statistical and causal
modules are complete with distinct machine-checked proofs.

DFG precision evidence: `evaluate_dfg_precision` builds weighted observed
prefix observations, admits only prefixes reachable through the supplied start
and directly-follows maps, and computes escaping versus activated transitions
with deterministic counters. Its default activity selector is `concept:name`;
grouped logs, event streams, eager tables, lazy tables, and tagged event-data
views share exact/source/alias selector precedence, requested-case authority,
and a checked row-materialization cap. The standalone `minera_dfg_precision` command,
cached-log `POST /api/dfg-precision`, and typed frontend request/response guard
share the `DFG_PRECISION` envelope; custom activity attributes and malformed
model/log inputs are validated by focused tests.

## Connected DFG filtering evidence

The native DFG filtering API now exposes deterministic activity-percentage,
path-percentage, dependency-threshold, noise-threshold, to-activity,
from-activity, and contain-activity variations. Each filter works on explicit
activity, boundary, and edge frequencies, removes only the requested
low-support nodes or paths, and repairs the graph so retained activities remain
reachable from a start and can reach an end. Noise cleaning compares each edge
with the smaller of the strongest ordinary edge counts at its endpoints and
honors an explicit most-common-path keep set without changing activity or
boundary tables. The CLI, local server, and typed frontend export controls
accept the same strict selector names and apply them in a deterministic order,
including the optional keep-all-activities behavior for path and connected-edge
pruning. Focused tests cover boundary preservation, branch pruning, dependency
ties, noise overrides, unknown activities, invalid thresholds, transport
validation, and deterministic output ordering.

## LTL trace-filter evidence

The native LTL filter exposes eventually-follows, contiguous A-next-B-next-C,
four-eyes resource separation, and different-person predicates. It supports
generic event/resource attribute keys, positive and negative selection,
strict pattern arities, and optional inclusive timestamp-difference bounds for
eventually-follows. Request parsing, the export CLI, the local mine endpoint,
and the typed frontend controls all apply the predicate after ordinary log
filters and preserve event/trace attributes. Focused regressions cover
timestamp validation, generic attributes, polarity, missing four-eyes sides,
and malformed request fields.

## Trace-pattern filtering evidence

The native log filter accepts one or more admitted activity patterns. Literal
patterns match complete variants, while a `...` token matches zero or more
activities and therefore expresses prefix, suffix, and infix constraints.
Patterns are separated by semicolons at request boundaries, retain trace and
log attributes, and support positive (keep matches) or negative (keep
non-matches) polarity. The request parser, CLI, local mine endpoint, and
frontend form serialize the same pattern contract. Focused regressions cover
exact and wildcard matching, multiple admitted patterns, negative polarity,
malformed empty tokens, and integration after ordinary event projection.

Exact admitted-variant filtering is available alongside the pattern filter
without changing its wildcard contract. The `admitted_variants` wire field
uses semicolon-separated variants and comma-separated activities; `[]` denotes
an empty variant, duplicate entries remain deterministic, and `...` remains a
literal activity rather than a wildcard. Positive polarity keeps only exact
matches, while negative polarity removes them. Native, request, CLI, local-server,
and typed frontend paths
preserve trace metadata and input order, including empty variants.

## Adjacent-path filtering evidence

Adjacent-path filtering compares neighboring events with inclusive elapsed-time
bounds when requested. The default activity and timestamp fields retain the
canonical event values; callers can select aliases such as `concept:name` or
`time:timestamp`, or a generic event attribute. An explicitly missing custom
attribute does not fall back to the canonical field, and malformed timestamps
are rejected only after a pair matches the selected activity values. Positive
and negative polarity, request/CLI/local-server/frontend serialization, and
ordinary and performance path entry points share this contract.

## Case-performance filtering evidence

Complete-case duration filtering measures the input-order difference between the
first and last event timestamps and applies inclusive lower/upper bounds. Either
bound may be omitted; an untimed or malformed trace is excluded while the
filter is active, and reversed or non-finite bounds are rejected. The native
standalone filter and integrated log options are exposed through request parsing,
the CLI, the local mine endpoint, and typed frontend controls. Focused tests
cover exact boundary inclusion, one-sided maxima, malformed timestamps, and
invalid bound ordering.

Case-selection filtering now also exposes the classic count and size variants.
`filter_ncases` keeps the first requested number of surviving cases in input
order (including the intentional zero-case result), while `filter_case_size`
uses inclusive event-count bounds and defaults to a two-event minimum. Integrated
min/max length bounds are strictly parsed and reject reversed ranges; request,
CLI, local mine, and frontend controls share the same non-negative integer
contract. Focused tests cover default and one-sided size behavior, count
truncation, zero limits, ordering validation, and adapter serialization.

Variant and endpoint filtering now covers the remaining frequency and polarity
forms in one integrated pass. Positive or negative start/end activity lists are
validated as paired options and applied after length/segment extraction. Native
variant helpers support top-k (including an intentional zero result), inclusive
minimum/maximum per-variant case shares, and cumulative percentage selection
with tied boundary variants retained. Request fields, CLI flags, local mining,
and typed frontend controls use fractions on the wire and percentages in the
UI; focused regressions cover zero targets, ties, reversed bounds, subordinate
endpoint settings, and combined constraints.

Prefix/suffix evidence now covers the full anchor variation: strict mode
includes the selected activity, non-strict mode cuts immediately before/after
it, and first/last occurrence selection is deterministic. Missing anchors drop
the trace; subordinate options without an activity are rejected. Standalone
prefix/suffix APIs, integrated request fields, CLI flags, local mining, and
frontend controls share this contract, with focused regressions for repeated
anchors, empty cuts, sequential cuts, and malformed options.

## Numeric attribute filtering evidence

Numeric attribute filtering parses one generic event attribute as a finite
number and applies inclusive lower/upper bounds. Event mode projects matching
events (or only out-of-range numeric events under negative polarity); trace mode
keeps or drops complete cases when any event is in range. Missing and malformed
values never match an event projection, while negative trace polarity retains
cases with no matching numeric value. Open bounds, reversed/non-finite values,
missing keys, and missing bounds are validated consistently by the native API,
request parser, CLI, local mine endpoint, and frontend controls. Focused tests
cover both modes, polarity, malformed values, one-sided ranges, and strict
adapter serialization.

## Frequency attribute filtering evidence

Top-activity filtering ranks activity names, or a selected generic event
attribute, by global occurrence count and projects every trace onto the
requested number of values. Ties are resolved lexicographically and a zero
limit intentionally removes all events. Relative-occurrence filtering keeps
values whose inclusive count reaches the configured fraction of cases or
events; case mode counts a value once per trace, while event mode counts every
occurrence. Missing generic attributes never match. Both standalone filters
and integrated mining options preserve log/trace metadata and are available
through native, request, CLI, local-server, and frontend surfaces. Focused
tests cover zero and one-sided limits, tie ordering, duplicate values,
case-vs-event denominators, missing attributes, malformed stakes, and adapter
round-trips.

Absolute occurrence thresholding complements the relative form with an
inclusive integer count over the original input log. It supports activity or
generic-attribute values, projects matching events while dropping missing
generic values, and composes with top and relative frequency projections in a
deterministic order. The classic pluralized variant-percentage compatibility
entry point shares the cumulative top-variant and boundary-tie semantics.
Request aliases, CLI flags, frontend validation, and focused native/adapter
tests cover zero thresholds, missing keys, and malformed counts.

## Classic DFG text I/O evidence

The deterministic DFG interchange surface reads and writes the compact classic
text layout: a sorted activity registry, `idxcount` boundary rows, and
`source>targetxcount` edge rows. Counts are positive 64-bit integers, duplicate
or out-of-range rows are rejected, unsafe multi-line/trimmed labels are
rejected, and UTF-8 labels plus stream/path aliases round-trip through the
native `DfgCleanAnalysis` payload. Trace variants and activity totals are not
invented into this format; CLI/server/frontend output adapters serialize the
cached post-filter graph, while input adapters remain open.

## Streaming DFG evidence

`StreamingDfgDiscovery` consumes case/activity events in arrival order, keeps
the latest activity for every open case, increments starts, activity totals,
and directly-following edges atomically, and derives deterministic current end
counts from the open-case table. Empty identifiers are skipped, reset clears
all state, and snapshots include open cases for downstream consumers. The
standalone `minera_streaming_dfg` CSV command, multipart `POST
/api/streaming-dfg`, and typed frontend request/response guard expose the same
`STREAMING_DFG` JSON contract; focused tests cover interleaved cases, empty
records, reset, deterministic serialization, and malformed frontend payloads.

## Streaming footprints evidence

`StreamingFootprintsConformance` keeps a mutex-serialized last activity for each
open case and checks model activities, first-activity boundaries, allowed
sequence/parallel pairs, and final end activities on explicit termination.
Snapshots expose deterministic per-case prefix diagnostics and aggregate event
and deviation counts. The standalone `minera_streaming_footprints` command,
multipart `POST /api/streaming-footprints`, and typed frontend request/response
guard expose the same `STREAMING_FOOTPRINTS` contract; focused tests cover
interleaved cases, unknown activities, boundary and pair deviations, reset,
termination, deterministic serialization, and malformed frontend payloads.

## Streaming temporal evidence

`StreamingTemporalConformance` validates each arriving event against every
eligible earlier event in the same case, using profile means, standard
deviations, and a positive zeta threshold. It supports canonical or generic
activity/timestamp attributes, skips incomplete or unparsable records, keeps
per-case histories isolated, and emits deterministic deviation diagnostics.
The standalone `minera_streaming_temporal` command, multipart
`POST /api/streaming-temporal`, and typed frontend request/response guard expose
the same `STREAMING_TEMPORAL` contract; focused tests cover prior-event pairing,
zero-variance profiles, custom attributes, invalid timestamps, reset, and
malformed frontend payloads.

## Streaming token-replay evidence

`StreamingTokenReplay` keeps one marking per open case and supports forward and
backwards replay variants, bounded silent-transition routing, weak firing with
missing-token accounting, duplicate-label handling, immediate-stop behavior,
final-marking checks, reset, and deterministic JSON snapshots. The standalone
`minera_streaming_tbr` PNML/CSV command, multipart `POST /api/streaming-tbr`,
and typed frontend request/response guard expose the same `STREAMING_TBR`
contract. Focused Release tests cover linear
replay, missing tokens, silent prefixes, backwards selection, reset, and
termination diagnostics.

## Segment-filter evidence

The native segment surface exposes prefix and suffix cuts with first/last
anchor selection and strict/non-strict boundary behavior, plus between
extraction over activity sets. Complete subtraces retain event and trace
attributes, reuse a shared boundary for repeated equal endpoints, and receive
deterministic subcase identifiers with a configurable separator. Empty cuts
are preserved when a strict boundary is at the edge; incomplete between
segments are discarded. Focused tests cover missing anchors, first/last
selection, strict boundaries, repeated endpoints, activity lists, attributes,
custom identifiers, malformed options, strict request parsing, CLI flags,
local-server application, and frontend controls/serialization.

## OCEL filtering evidence

The native OCEL filtering surface now supports positive and negative event
attribute, object attribute, object-type, object-id, and event-id selection,
inclusive timestamp windows, activity-to-object-type matching, per-event
object-type count thresholds, first/last lifecycle events per object type,
activity-connected relations, recursive object expansion, and connected-
component object/length/type/activity filters. Event, object, and relation
selections propagate deterministically to the related tables, retaining only
valid links, object changes, and object-to-object relations; empty selections
clear the dependent tables according to the selected propagation direction.
The standalone `minera_ocel` command and multipart `POST /api/ocel/filter`
expose every OCEL filtering variant with strict bounded arguments. The typed
frontend `requestOcelFilter` adapter serializes the same options and validates
the recursive OCEL envelope. Focused tests cover typed scalar attributes,
duplicate relation counts, orphan removal, inclusive boundaries, lifecycle
endpoints, recursive expansion, singleton components, component predicates,
empty maps, malformed bounds, nested delimiter round-trips, escaped
backslashes, and integer overflow rejection. The shared native query codec is
also used by the object-binding playout parser, so object types and ids may
contain the grammar delimiters without being split at the wrong layer.

## Correlation-mining parity evidence

Correlation discovery exposes CLASSIC, CLASSIC_SPLIT, and TRACE_BASED as
independent variants. CLASSIC discards case boundaries, orders interval events
by start and completion time, computes strict precede/succeed probabilities,
and supports both the pinned bidirectional greedy duration estimate and exact
minimum-weight full bipartite matching. The exact matcher has no artificial
population cap. Equal completion/start boundaries are eligible for exact
duration matching but remain excluded from the strict precedence probability.

CLASSIC_SPLIT partitions the native flattened input order before sorting each
chunk. It averages precedence matrices across every chunk, including zero
entries for absent activity pairs, and takes the elementwise maximum duration.
TRACE_BASED instead preserves case boundaries, uses only traces containing both
activities in each probability denominator, and aggregates the two greedy
matching directions over trace-local samples. The trace variant intentionally
ignores the exact-matching option, as does the pinned variation.

All variants solve the same balanced activity-occurrence transportation
problem. Minera uses an integral minimum-cost solver and a deterministic
lexicographic tie normalization; ties may therefore choose a different but
equally optimal DFG from an LP backend while preserving every activity's
incoming and outgoing marginals and the same objective. Frequency edges retain
their raw correlation duration, including zero-duration forced reverse and
self edges.

Activity, completion timestamp, interval start, and case-id selectors are
available through the native API, strict request parser, CLI, local server,
deterministic JSON, and frontend, together with the exact-match toggle and
split sample size. Canonical aliases, imported first-class source spellings,
and ordinary attributes resolve consistently. Exact generic activity columns
win over projection aliases with the same spelling. An omitted or empty start
selector uses the selected completion field. Required missing, null, blank, or
malformed activity and timestamp values fail explicitly; present blank case
values remain valid grouping keys.

Event logs and flat event streams preserve their flattened source order before
CLASSIC_SPLIT creates chunks. Eager and lazy tables first order rows by
completion and start, then retain stable source order for ties. TRACE_BASED
keeps native trace boundaries and groups flat inputs by the selected case id;
each case is ordered independently. All four input forms are also available
through the generic event-data dispatcher. The native TRACE_BASED path honors
every selector rather than dropping configured trace parameters. CLI and local
server CSV requests retain the source header, source row order, and
present-empty cells in a flat table. After filters, surviving events are
projected back to those rows by their stable input order and filter-side field
changes are overlaid, so selected case columns can regroup rows independently
of importer-created native traces without losing filter effects. XES requests
retain their native trace boundaries. Changed first-class cells use matching
transformed attribute metadata, including selected case metadata from the
enclosing trace. Without matching metadata, derived projections become scalar
strings and discard stale native values and children; unchanged cells retain
their original type. This prevents transformed byte or nested cells from
retaining an incompatible payload while preserving source columns and roles.

Focused regressions cover canonical classic/trace/split fixtures, raw and
table-ordered split chunks, all eager/lazy/stream/log adapters, custom
activity/completion/start/case selectors, first-class source metadata, missing
and null cells, blank case groups, exact-key/projection collisions, forced
reverse and self edges, one-sided greedy matching, equal interval boundaries,
a rectangular fixture that distinguishes exact matching from both greedy
orders, exact matching above 25 events, strict timestamp errors, empty and
single-activity logs, complete diagnostic matrices, projected-label JSON ids
and counts, nonblank and blank custom-case CLI regrouping, source-order split
ties, deterministic transportation ties, and balanced transportation
marginals. Native activity and case identities use their stable text projection
because Minera's graph model is string-labelled; typed scalar storage remains
available on the input cells. The dispatcher, three variant modules, and
correlation support module carry distinct machine-checked proofs; the support
proof exercises probability/duration matrices and balanced transport rather
than JSON label registration.

## Performance-spectrum and batch parity evidence

Performance-spectrum discovery exposes the six canonical selectors
`DATAFRAME`, `LOG`, `DATAFRAME_DISCONNECTED`, `LOG_DISCONNECTED`, `LAZYFRAME`,
and `LAZYFRAME_DISCONNECTED`. `CONNECTED` and `DISCONNECTED` remain compatibility
aliases and do not replace a catalog selector. All paths require at least two
requested activities, validate selected timestamps, and return observations in
deterministic first-instant order. Connected discovery projects each case onto
the requested sequence and emits overlapping full-pattern windows.

The adapters retain their observable input and sampling contracts. LOG uses
trace boundaries, epoch-second instants, and midpoint sampling with forced
extremes. Eager tables use their column projection and timestamp scaling;
lazy tables retain prefix-oriented materialization. A sample size of zero is
accepted and retains each adapter's boundary-sampling behavior: eager/lazy
connected results are empty, while selectors that force boundary observations
retain those observations. LOG_DISCONNECTED retains the pair-only compatibility
behavior, while eager and lazy disconnected variants emit longest contiguous
subpatterns first and suppress rediscovery of consumed interiors. Repeated
labels retain explicit requested-pattern offsets in JSON and the UI.

Batch discovery has distinct LOG, PANDAS, and POLARS components. Each groups
events by selected activity and resource and uses completion timestamps as the
default starts unless an interval-start field is configured. LOG selectors can
resolve a case id per event; table variants can additionally use an event-id
column to keep otherwise identical rows distinct. Without an event id, exact
table-row identity tuples are intentionally deduplicated. Intervals merge by
strict transitive overlap and then by the configured inclusive gap. Undersized
groups are removed, and classification priority is simultaneous, batching on
start, batching on end, sequential, then concurrent.

Minimum self-distance has separate LOG, PANDAS, and POLARS components. LOG
preserves real trace boundaries; flat inputs group interleaved rows by their
selected case column while keeping source order. Every result includes the
minimum distance and the distinct activities witnessed between repeated
occurrences. Imported first-class fields, their recorded source keys, generic
event values, and trace values are resolved consistently.

Temporal-profile discovery has distinct log and dataframe components, as does
conformance. Discovery covers all ordered within-case event pairs, independent
source-completion and target-start selectors, strict timestamp validation,
custom or explicit-empty weekly business-hour schedules, and deterministic
sample statistics. The direct adapters are strict by default; the always-on
profile included with general mining stays permissive unless strictness is
requested, so timestamp-less logs remain valid. Conformance preserves one
bucket per case, applies the configured zeta threshold, and represents a
zero-variance mismatch with the finite maximum-distance sentinel. It is exposed
through grouped JSON, a cached local endpoint for LOG and DATAFRAME, and a typed
frontend adapter.

Transition-system discovery accepts EventLog, stream, eager table, lazy table,
and generic event-data views. Sequence, set, and multiset states combine with
forward/backward traversal, any nonnegative window including zero, optional
occurrence evidence, and configurable activity and case selectors. Flat inputs
retain original row ordering within regrouped cases and occurrence indices.

All selectors and settings above flow through the native API, strict request
parser, standalone CLI, local server, deterministic prevalidated JSON, and
frontend controls. Batch JSON carries variant and selector metadata, optional
event ids, all five categories, group/event counts, resources, ranges, and case
evidence. The coverage manifest records six completed packages and nineteen
completed owned modules with independent implementation identities and Release
contracts.

Focused regressions cover adapter dispatch, imported source keys, stable and
unsorted ordering, connected overlaps, disconnected full/partial matches,
loops, longest-first suppression, every sampling boundary, all five batch
types, per-event cases, row deduplication/event identity, custom fields,
overlap/touch/gap/transitive merging, strict and permissive timestamps,
one-sided temporal selectors, business calendars, zero variance, grouped
conformance, all transition views/directions/windows, malformed requests, JSON
rejection before output, and frontend serialization/rendering.

## Footprints parity evidence

Footprint discovery covers the entire event log, trace-by-trace log behavior,
eager dataframes, restartable lazy dataframes, raw DFGs, Petri reachability
graphs, process trees, and POWL. Eager unsorted discovery retains physical-row
adjacency, while lazy unsorted discovery retains each case's row subsequence;
their sorted paths preserve the completion-time and optional start-time rules
of their respective adapters. Activity, case, completion-time, optional
start-time, index, and sorting parameters are explicit native options. The
compatibility index selector is accepted but intentionally does not affect
ordering. The Petri variant accepts an exact initial marking, retains transition
identity until relation classification, follows silent routing when determining
eventually enabled activities, and rejects provably unbounded reachability
instead of returning a truncated model. Process-tree and POWL bottom-up
discovery preserve skippability, start/end activities, always-happening
activities, minimum length, the process-tree maximum length without loops,
optional-path bridging, and partial-order concurrency. Inclusive choice uses
the pinned footprint algebra's mandatory concurrent-child summary, including
silent and nested children.

Conformance exposes LOG_MODEL in strict and non-strict modes, LOG_EXTENSIVE,
and TRACE_EXTENSIVE with the always-executed-activity toggle. Precision,
log fitness, percentage-fit, footprint/conformance flattening, and sparse blue
process-tree boundary decoration use the same footprint artifacts. Explicit
case selectors are retained in trace diagnostics and fail clearly when a
requested attribute is unavailable.
Focused regressions cover empty and repeated traces, eager/lazy interleaving,
timestamp ties and invalid values, zero-weight DFG edges, singleton DFGs,
sequence/XOR/inclusive/parallel/loop models, silent prefixes, duplicate Petri
labels, custom and concurrent initial markings, optional POWL boundaries,
transitive partial orders, weighted fitness, missing optional model fields,
unbounded visible and silent growth, tree-node decorations, and duplicate case
identifiers.

All eight discovery selectors, all three conformance selectors, model
selection, strictness, the always-executed toggle, table selectors and sorting,
and explicit markings are available through the public native API and the
applicable strict request, CLI, local-server, deterministic prevalidated JSON,
and frontend surfaces. The dispatcher selectors and all thirteen owned modules
have distinct implementation identities and registered Release contracts.

## Declare parity evidence

Classic Declare discovery supports all 18 pinned templates, considered
activities, allowed-template prerequisite behavior, explicit or automatic
support/confidence thresholds, and the auto-selection multiplier. It preserves
directional activation, vacuity, identical-trace weighting, zero-filled
inactive rules, compound and negative constraints, tuple-key identity, and
the reference's support/confidence counts. Classic conformance provides the
pinned deviation order, totals, fitness, fit status, unknown-family behavior,
and one result per input trace.

Discovery and trace/log conformance share a strict configurable activity
selector. Canonical aliases, imported first-class source keys, and generic
scalar attributes resolve with exact-field, source-key, then alias precedence;
equal-priority collisions reject. An explicitly blank, missing, or null
selection fails instead of falling back to the canonical label. Flat streams,
eager tables, lazy tables, and generic event-data views additionally expose a
strict case selector for grouping, while already-grouped logs ignore it. The
activity selector is exposed through request, CLI, local-server, and frontend
mining contracts so the discovered model and its immediate conformance
diagnostics use the same projected alphabet.

The batch model and conformance diagnostics are exposed through the native API,
strict request fields, CLI, local server, deterministic numeric-ID JSON, and
the frontend. Per-case diagnostic tables retain numeric, Boolean, date, ID, and
string case-cell metadata instead of reducing identifiers to text. Tests cover
every template family, positive and violating traces, flat-adapter equivalence,
selector precedence/ambiguity/null handling, typed case IDs, empty and unusual
activity labels, duplicate cases, explicit-empty filters, threshold edge
values, malformed output validation, and duplicate rule keys.

The separate streaming AUTOMATA implementation is stateful, mutex-serialized,
and intentionally prefix-only: pending obligations are not finalized at the
end of a case. It implements all 18 immediate automata, sticky first-violation
counting, undefined-case and timestamp fallbacks, inert unknown templates, and
lossless per-constraint snapshots. Declare classic playout independently
generates random prefix-safe traces with the pinned trace-count and length
parameters; a seed is provided as a deterministic Minera extension. Declare
playout is selectable from the CLI and local server, and its XES download is
available from the Declare frontend view. Tests cover multi-event automata
behavior, concurrent snapshots, malformed arity, candidate-state isolation,
early stopping, pending obligations, empty models, length bounds, and seeded
reproducibility.

## Heuristics Miner parity evidence

Classic discovery uses the reference `0.5` dependency, `0.65` AND, `0.05` DFG
pre-cleaning, and `0.5` length-two-loop defaults. It preserves raw trace order,
raw lifecycle events, activity/start/end counts, direct and window-two DFGs,
frequency triples, optional per-case DFG deduplication, protected most-common
paths, and all scalar/raw/all performance aggregations. Pre-cleaning uses the
maximum incident-path rule and strict removal boundary; dependency, activity,
path, AND, and loop-two thresholds use the pinned inclusive comparisons.

Length-two-loop rescue retains the pinned pre-existing-node quirk, adds both
directions with dependency zero, and does not recompute AND bindings afterward.
Classic and PLUSPLUS both use deterministic overlapping maximal AND cliques.
Classic node retention distinguishes isolated activities from the all-filtered
fallback, and the converted workflow net contains exactly the heuristic nodes.
A public classic DFG-input entry point supports explicit or inferred
activities, occurrences, boundaries, window-two paths, triples, and performance
samples; PLUSPLUS rejects DFG-only input as in the reference.

The PLUSPLUS variation independently implements the pinned interval abstraction.
Lifecycle events are converted through case-insensitive start/complete matching
with FIFO queues keyed by activity and lifecycle instance; unmatched completes
become zero-duration events and unmatched starts are omitted. A log whose first
event already carries the configured start timestamp is treated as interval
input globally. Eventual-follow and concurrency calculations use stable
start-time ordering, while pre-interval performance edges preserve the input
trace order.

Discovery uses the first non-overlapping following interval, inclusive overlap
counts (so touching intervals can be both eventual-follow and concurrent),
activity/path occurrence filtering, direct-adjacency mean waits, mean sojourn
times, and the exact Miner++ dependency and split/join formulas. Dependency and
AND thresholds are strict and the default dependency is `0.5`. AND bindings are
deterministic overlapping maximal cliques rather than transitive components.
Frequency and performance decorations, filtered and raw abstraction statistics,
variant selection, minimum activity/path counts, configurable activity and
completion-time selectors, and configurable start-time and lifecycle-instance
attributes are exposed in the native model, JSON, request parser, CLI, local
server, and frontend. Generic activity selectors carry a local label table in
JSON, so custom labels remain renderable and replayable without colliding with
the canonical event graph.

Focused Release tests cover classic defaults and equality boundaries, exact
noise removal/exemptions, filters, self-loops, per-case deduplication,
length-two rescue and false cases, lifecycle and pre-interval modes, custom
activity/time selectors, invalid
and missing timestamps, first-following and boundary-touch concurrency, the
concurrency denominator, all performance summaries, raw statistics, absence
of default connect-all repair, DFG inference/overrides, and overlapping maximal
cliques. Full native tests, frontend tests/build, and headless CLI JSON smokes
validate the integrated path.

## Petri-net alignment parity evidence

The six explicit Petri-net alignment variants are implemented as clean-room
searches over one compact, isomorphic synchronous-product state:
`(marking, trace position)`. State-equation A* adds a full remaining-trace LP
lower bound (place-balance rows plus one row per remaining event, with model,
log, and synchronous columns); its solve count is reported in diagnostics.
Dijkstra without heuristics traverses that product uniformly. The less-memory
variants apply Pareto dominance over marking, trace position, and cost while
preserving the optimum; focused tests compare their costs with the full
frontier under fractional custom costs.

Cost semantics are selectable rather than silently changed: the legacy profile
keeps unit visible log/model deviations and free silent routing, while the
reference profile charges 10,000 for visible log/model moves, 1 for silent moves,
and 0 for synchronous moves.
Per-activity, per-position, and per-transition overrides are supported, as are
API callbacks for trace, model, and synchronous moves. Callback values are
resolved once for each concrete trace/model pair, reject non-finite or negative
costs, and remain stable even for repeated labels; tests cover overrides,
fractional costs, callback statefulness, and invalid values.

Discounted A* honors both the `exponent` and request-layer `theta` spelling
(greater than one). Its synchronous-Dijkstra scheduling toggle is an adapter
over the same compact product representation: only successor scheduling differs
from the explicit order, so both schedules retain equal optima. The native API,
request parser, CLI, and frontend expose these controls and preserve the
less-memory Dijkstra/unit-cost defaults when no explicit options are supplied.
Like the pinned 2.7.22 reference implementation, discounted search closes a state
by `(marking, trace position)` rather than treating the same product state at
every path depth as distinct. Its optimum is therefore over simple
product-state paths. This avoids an ill-defined cyclic objective in which
repeating a positive-cost loop can push a later move toward an unattained
discounted infimum; it also makes zero- and positive-cost cycles terminate
deterministically within the configured limits.

Generator Dijkstra variants retain deduplicated equal-cost optimal paths, with
`max_alignments` as an explicit cap. No-heuristics and less-memory generators
agree on the optimum, tied alternatives are retained up to the cap, and cyclic
models cannot produce more than the requested collection. State, whole-log,
and per-trace time limits are enforced; a reachable search that hits a limit
reports `computed=false, exhausted=true` rather than a fabricated optimum.
Diagnostics include visited/queued states, traversed arcs, closed-set size,
visited markings, and LP solves.

Discounted anti-alignment and multi-alignment use the complete fired-transition
run, including null and other silent moves, for their ordinary precision and
maximum-distance diagnostics. The discounted recurrence retains its asymmetric
model-prefix rules, while the ordinary insertion/deletion metric initializes
its first row and column with raw edit costs and makes silent deletion free only
in interior cells. Exact objective ties prefer the longer transition run before
the deterministic transition order. Per-marking limits permit the configured
number of revisits and report exhaustion only when the next expansion would
cross that boundary. Focused contracts cover unequal lengths, null/tau/n*/skip
labels, silent-only runs, epsilon normalization, longer-loop ties, unreachable
final markings, and the zero/one marking-limit boundary for both algorithms.

Silent (`tau`) model moves remain explicit routing moves in diagnostics and
alignments, while weighted input/output arc multiplicity is preserved when
testing enabledness and firing. A genuinely unreachable final marking is
reported as unsound (`exhausted=false`), distinct from a bounded or timed-out
search. Focused `src/backend/tests/test_alignment_variants.cpp` regressions
cover all six
variants, reference/legacy cost profiles, custom costs and callbacks, full-product
LP solving, less-memory dominance, discounted exponent and scheduling parity,
generator optima and caps, timeout/state diagnostics, tau moves, weighted arcs,
and invalid-cost handling; request, CLI, and frontend tests cover the exposed
adapters.

Both token replay and Petri-net alignment also accept a named event activity
field in addition to the native activity column. Request and CLI adapters trim
and validate the selector, missing event fields fail deterministically, and the
selected field is retained in JSON provenance so custom labels remain
addressable by the frontend.

## Process-tree alignment parity evidence

The four selectable process-tree variants are implemented in the native
`include/minera/process_tree_alignment.hpp` / `src/backend/process_tree_alignment.cpp`
pair and exposed by the strict request parser, `minera_export`, the local
server, and the frontend's **Other alignments** panel. They are deterministic,
collision-free clean-room implementations with dependency-free exact residual
fallbacks. This preserves the reference implementation's behavioral questions without copying its
Python implementation; `MILP` does not claim to invoke literal PuLP.

* `SEARCH_GRAPH_PT` is the default residual-expression Dijkstra search, using
  canonical interned residuals for stable, collision-free state identity.
* `DYNAMIC_PROGRAMMING` recursively aligns subtrees with memoized subtree/trace
  pairs; loops use the exact residual fallback. Optional global caching has a
  positive `batch_size` and `clear_cache` controls periodic eviction.
* `MILP` is an exact integer shortest-path residual A*/branch-and-bound
  equivalent to the pinned unit-cost MILP objective. It is dependency-free and
  performs no external LP solves (`lp_solved` therefore remains zero); native
  residual-search counters are reported instead. Its formulation validates
  loops as binary; non-binary loops are rejected.
* `APPROXIMATED_ORIGINAL` uses exact residual alignment below configurable
  trace-length/tree-height thresholds, then recursively decomposes sequence,
  XOR, and parallel nodes. Loops always use the exact residual fallback, whose
  count is reported in the diagnostic message.

`ProcessTreeAlignmentOptions` defaults to `SEARCH_GRAPH_PT`, `max_states = 0`,
per-trace and whole-log time limits of `0`, `use_global_cache = false`,
`batch_size = 1000`, `clear_cache = true`, approximation thresholds
`max_trace_length = 1` and `max_process_tree_height = 1`, and approximation
trace/per-trace time limits of `0`. A zero state or time bound means unlimited;
native validation still rejects negative/non-finite values, a zero DP batch,
and unsupported cache settings on non-recursive variants. Request and CLI time
fields are milliseconds (`pt_alignment_max_time_ms`,
`pt_alignment_max_total_time_ms`, `pt_alignment_max_trace_time_ms`, and
`pt_alignment_max_per_trace_time_ms`). CLI selection starts with
`--align-process-tree` and `--pt-alignment-variant` plus the corresponding
`--pt-alignment-*` bounds, cache, and threshold flags. The deferred HTTP entry
point is `POST /api/align-process-tree`.

All four variants construct their trace words from the same strict activity
selector. Canonical aliases, imported source keys, and generic scalar fields
are accepted, while blank, missing, and null selections fail before search;
the request, CLI, server, and frontend adapters expose that selector without
changing the canonical default.

The JSON result has `family: "PROCESS_TREE"`, the selected `variant`, aggregate
`total_cost`/`fitness`, per-trace `trace`, `cost`, `fitness`, `moves` (`sync`,
`log`, or `model` with log/model/activity labels), and diagnostics for visited,
queued, and traversed states, native residual-search counters, aligned traces,
and a message (`lp_solved` is zero because no external LP solver is used).
`computed`, `completed`, `timed_out`, and `exhausted` are explicit at
both result and trace level: a state/time bound yields an incomplete result and
never a fabricated optimum. As in the reference implementation, an empty fitness denominator reports
`0.0` (including an empty trace/log against a silent tree). Native n-ary loop
redo/exit children are normalized to an XOR for search/DP/approximation; MILP's
binary validation is deliberate and tested. The recursive variants also use
deterministic duplicate-label event assignment and retain model moves so the
rendered explanation sums to the reported cost.

Focused validation is in `src/backend/tests/test_process_tree_alignment.cpp`,
with request mapping/rejection coverage in `src/backend/tests/test_request_params.cpp`,
JSON checks in `src/backend/tests/test_core.cpp`, and frontend query/result
checks in `src/frontend/tests/app-logic.test.ts`. Tests cover all four variants, exact
sequence/XOR/parallel/loop behavior, repeated traces and DP cache batches,
approximation thresholds and loop fallback diagnostics, timeout/state
completion flags, empty-denominator fitness, n-ary normalization, and MILP's
binary-loop rejection.

## DFG classic and edit-distance alignment parity evidence

DFG alignment implements the pinned `CLASSIC` variant over a complete
start/DFG/end frequency model. Start and end activity maps are included even
when a supplied frequency is zero; edge frequencies are informational rather
than hidden enablement flags. Disconnected nodes are accepted; a graph without
an accepting path returns `computed: false` from raw trace alignment, without
fabricated moves or scores. Whole-log alignment and projections fail when that
path is unavailable for their fitness baseline. Default alignment costs are zero for synchronous moves
and 10,000 for visible model/log moves. Per-activity synchronous, model, log,
and internal-log costs are configurable: the reported cost can differ from the
search's internal cost (for example, unknown labels may have a free internal
move), and both are serialized. Supplied cost maps are strict: a missing
requested activity raises an error. Explicitly empty maps are represented by
native presence flags and `{}` in HTTP, CLI, and frontend controls; omitted
maps retain defaults.

Trace alignment uses a strict configurable activity selector, while log
projection also accepts a strict timestamp selector. Projected original-trace
metadata and synchronous/model events are written under the selected keys and
retain compatible typed metadata. The selected activity is also used to build
the server-side DFG boundaries and edges, preventing model/log alphabet drift.

Whole-log alignment caches exact activity variants in an ordered map, reports
`unique_variants`, and restores results to the original trace order. Raw trace
results contain cost and state diagnostics; fitness and best/worst cost remain
undefined (NaN). Whole-log calls add those scores using an empty-trace baseline,
including its cost-map errors even for empty logs. A zero fitness denominator
raises an error instead of producing an invented fitness value. This preserves the public
distinction where an exact trace can accept a partial model cost map but a log
call fails while calculating its baseline. In both APIs,
visited-state and elapsed-time bounds are enforced and report a bounded-search
failure instead of fabricating an optimum. Log projection requires selected
timestamps, drops unrelated event/trace/log attributes, records the supplied DFG
in `@@aligned_dfg`, and marks projected events with `@@is_conforming`.
Metrics follow the public final-move exclusion and do not bridge intervening
log moves when counting edges; activity counters initialize from DFG edges.

A pinned public audit captured 723 observations using
`scripts/capture-dfg-alignment-reference.py`. Native reconstruction now returns
an empty move list for empty traces while retaining model-path cost, omits
leading model moves as observed publicly, and represents exposed synthetic sink
moves with a collision-safe deterministic name instead of the reference's random
UUID. Focused regressions cover these cases, unavailable alignments, projection
context and timestamps, metrics, and ordinary/custom costs.

The search prioritizes farther trace positions at equal cost, retains pending
duplicates, and prunes positions dominated by an already closed position at the
same node. `visited_states` records the pop counter when the winning goal was
queued; `closed` counts accepted expansions. This reproduces the audited `aba`
4/5 and `ba` 6/5 diagnostics, rather than forcing both counters to be equal.
`scripts/capture-dfg-search-reference.py` compares a clean-room search model with
public calls over 5,730 graph/cost/trace cases, matching both costs and counters.
Forty-five named native cases plus branching, reconstruction, and state-limit
regressions test the implementation directly. Metric activity accounting uses
the log label of exposed sink moves, preserving the public repeated-activity
counts; model labels still determine edges.

`scripts/capture-dfg-native-reference.py` runs the compiled
`minera_dfg_alignment_probe` against public APIs. Its base matrix matches all
5,730 cases for each of raw traces, enriched log results, projected logs, and
metrics (22,920 comparisons). The extended matrix adds
empty/unreachable/disconnected models, self-loops, multiple starts, zero
frequencies, and empty/partial/zero/equal cost functions. It records
single-call differences, including randomly selected tied routes; the repeated
audit below separately checks sampled outcome support. Error class/message
equality is not asserted across languages.

Tied searches use independent UUIDv4 source/sink routing names for each raw
search, including the baseline and each uncached log variant. The queue orders
internal cost, descending trace position, parent pop counter, reported cost,
node name, and insertion sequence. This reproduces the observed variation
under zero and equal costs, while duplicate variants retain their cached result.
The exposed sink label remains canonicalized independently of routing order.

`scripts/capture-dfg-zero-tie-reference.py` compares repeated native/public
outcomes and their frequencies. Across 648 model/cost/trace cases with 200 calls
per side, sampled outcome sets matched, and no frequency difference exceeded
five pooled standard errors. A broader clean-room candidate audit covered
4,275 cases and 137,824 public calls. Native regressions also force both UUID
ordering boundaries using activity labels outside the hexadecimal range,
without relying on chance to exercise each branch.

CLI and HTTP responses derive projections from the same computed alignment
result that they serialize. Result-consuming native overloads preserve this
relationship for callers; standalone projection APIs retain their own search.
Deterministic regression cases supply alternative routes and verify projected
timestamps, activities, and metrics without relying on random tie selection.

The integrated checkpoint repeated all 22,920 base native/public comparisons
and the 648-case, 200-call tie audit. Both passed. The extended matrix made
39,797 calls for each of the four operations and retained 1,512 single-call
differences. Repeating each differing operation 200 times produced 302,400
observations per implementation with matching sampled outcome sets and no frequency
difference above five pooled standard errors. Numeric JSON spellings such as
`1` and `1.0` share an outcome key; the saved draws were rechecked after fixing
that distinction, without resampling. The original counts remain preserved.
The evidence gates fail on mismatches, malformed results, and flagged frequency
differences; they record binary/module hashes and retain paired error details.
Error comparisons establish rejection presence, not identical exception causes.

Durable compressed JSON evidence is stored in
`docs/dfg-classic-reference-fixtures.json.gz` (boundary/base/tie captures, opaque
module verification, and original extended samples),
`docs/dfg-classic-extended-reference.json.gz` (the extended single-call report,
including both errors for matched rejections), and
`docs/dfg-classic-extended-support.json.gz` (the rechecked repeated outcomes).
`scripts/check-dfg-extended-support.py` accepts the compressed extended report
and reproduces the bounded repeated audit. These files retain the configured
reference pin separately from the verified newline-normalized module bytes and
tested executable hashes. The Release build, 150 native suites, 613 frontend tests, TypeScript,
production frontend build, and CLI/server smoke checks passed. Another 32 tied
HTTP projection responses matched their serialized alignment routes.

Full `CLASSIC` parity remains unproven: finite samples establish neither
exhaustive outcome support nor exact probabilities. Arbitrary graphs, labels,
and cost functions remain beyond the bounded matrices; cross-language error
class/message equivalence is not asserted. The dispatcher and implementation
module remain partial.

`EDIT_DISTANCE` computes Levenshtein distance for every source variant and picks
the minimum (or maximum when `perform_anti_alignment` is enabled); duplicate
target variants retain their first occurrence, with deterministic lexical
tie-breaking. Reconstruction follows `difflib.SequenceMatcher` matching-block
semantics, including the default autojunk suppression for popular symbols in
large targets, but uses ordered maps for deterministic clean-room behavior (the
pinned hash-order implementation could choose a different tied path). A
substitution is represented as two moves (one log-only and one model-only), so
deviation cost, fitness, and the reference best/worst-cost denominator (`bwc`) are
explicit and auditable. Empty source logs return no alignments; an empty target
log and non-positive deviation cost are rejected.

Edit-distance log projection uses each trace's independently selected target
variant, including heterogeneous selections. It preserves source trace
metadata, records `@@original_trace` and `@@aligned_variant`, keeps timestamps
from synchronous source events, timestamps inserted model events at the current
source time, omits source-only moves, and marks each projected event's
`@@is_conforming` value. Scalar EventLog log attributes (including these
projection markers) and their XES scalar types round-trip through the XES
importer/exporter.

Edit-distance source and target variants use the same configurable activity
selector, and projection uses that activity selector plus a configurable
timestamp selector. Existing conventional-key overloads remain compatible;
strict request, CLI, server, and frontend adapters reject blank or missing
selected fields rather than silently mixing alphabets.

Concept-drift adapters expose deterministic `CONCEPT_DRIFT` JSON with the
selected `bose` variant from the CLI
(`--concept-drift`, `--drift-*`) and cached-log server endpoint
`POST /api/concept-drift`. Strict controls cover sub-log/window sizes,
permutation count, threshold, change-point cap, seed, and optional timestamps;
the result includes activity features, raw, boundary-partitioned, and cumulative
sub-log summaries, change points, and timestamp arrays. The frontend form
exposes the complete control set and reports both raw sub-log and returned
segment counts.

## Organizational role discovery parity

The bounded role endpoint (`--roles` and `POST /api/roles`) starts with one
role per activity and counts only events having both a non-empty activity and
resource. Role similarity is the overlap of normalized resource-count vectors
(sum of component-wise minima divided by sum of maxima). After a merge, each
source role contributes one presence count per resource rather than carrying
raw event totals. The highest-scoring
pair is merged only when its score is strictly greater than
`roles_threshold` (default `0.65`); `roles_activity_attribute` and
`roles_resource_attribute` select standard or custom event fields; ties and
final role ordering are lexical
and deterministic. Final roles are sorted by descending activity/resource
cardinality before the activity-name tie break. `roles_max_resources` and
`roles_max_activities` are strict safety bounds. Empty activities and
blank-resource events are excluded, while
their traces remain represented by aggregate diagnostics. The standalone JSON
family is `ROLE_DISCOVERY` with `roles[].activities` and
`roles[].resources[{name,count}]`.

## Object-centric graph mining parity

The native object-centric suite covers classic event-to-object-type, type,
directly-follows, process-net, and stochastic-arc discovery. Process-net
discovery exposes both annotated and unannotated forms, log- and graph-based
inductive variants, variable and double arcs, label selectors, noise controls,
and optional token-replay diagnostics. Graph comparison supports direct model
inputs and deterministic discovery from object-centric logs, including
weighted structural and measure differences with strict finite thresholds.

Timestamp interleavings and event-link discovery preserve deterministic source
indices, configurable fields and direction, propagation and first-occurrence
behavior, and strict output bounds. Their serializers reject inconsistent row
indices, empty endpoints, invalid directions, non-finite time differences, and
results that exceed the requested limits.

Extensive process-net and causal-net playout support independent, shared, and
synchronized bindings; explicit object sets and markings; bounded seeded
branching; reachability-only queries; and optional sequence and transition
witnesses. Result serialization validates binding variants and rejects witness
payloads that conflict with their request flags or contain empty activities or
object identities. Causal marker groups retain conjunctive and alternative
bindings, cardinalities, support counts, and object-exclusion keys.

## Feature transformation and clustering parity

Feature extraction supports event, trace, and temporal projections with
categorical and numeric selectors, explicit schemas, activity and succession
representations, trace timing and concurrency families, calendar or fixed
windows, workload features, and strict cardinality and numeric controls.
Event-based schemas infer ordinary typed event attributes, including retained
activity/resource fields under their source keys, without injecting activity,
timestamp, or case columns. Categorical selection collects only string values;
missing/null/non-string rows remain zero. Cardinality bounds apply to inferred
and explicitly selected categorical groups. Omitted categorical/numeric lists
infer their group, whereas explicitly empty lists suppress it. Native supplied
flags and presence-aware CLI/server parsing preserve that distinction. Event and
trace feature-name request lists preserve whitespace and use `\e` for an empty name;
the existing escaped comma/backslash syntax still represents literal characters.
The frontend emits this encoding, distinguishing an empty schema from a schema
containing an empty-name column.
An explicit event feature schema replaces inference and directly encodes each
event's actual numeric/string values, independent of selection lists and
cardinality. Schema order, duplicate names, and empty/unknown names are retained;
only the first duplicate column is populated. Numeric/categorical name collisions
retain duplicate generated columns with the numeric value in the first column.
Rows and padding use zero, independent of the default-not-present option. An
empty log raises; a log containing an empty trace retains its zero-event tensor.
Numeric event columns require a present native numeric scalar in every event;
Boolean values qualify, while strings, bytes, nulls, missing values, and
containers exclude the column. Event numeric cells use float32 precision.
Trace schemas emit trace categorical, event categorical, trace numeric, event
numeric, and succession groups in that order. Explicit attribute lists preserve
order and duplicates, with sorted categories within each attribute. Supplying
any ordinary selection list, including an empty list, disables ordinary default
inference and its forced activity/succession additions. A succession list alone
does not select explicit mode. Explicit succession selection ignores the default
succession flag. Typed categories preserve empty strings and stringify nulls,
bytes, and containers; absent trace values and traces with no selected event
value use `UNDEFINED`. Successions require adjacent events with both values
present; missing events interrupt a pair, and traces without a pair use
`UNDEFINED`.
In explicit selection mode, trace schemas retain duplicate, empty, whitespace,
and unknown names;
only the last duplicate column is populated. They encode only selected groups,
with zero for unassigned core cells regardless of default-not-present. An empty
schema retains one empty row per trace. With all ordinary selectors omitted,
the supplied feature schema is ignored and the inferred core schema is generated.
Legacy activity-count, duration, and WIP
columns are no longer injected; timing and workload extras remain opt-in.
Core extraction does not parse unused timestamps.
Enabled trace extras append their columns after the core schema, including an
explicitly empty schema. A custom core name that matches an extra remains a
separate zero-valued column. The enable-all option supplies defaults for numeric
families; explicit false overrides suppress individual families, and case
identifiers require their independent option. Generated resource workload
columns precede activity indexes and concurrency columns.
Case duration uses the last event completion minus the first event start, with
zero for an empty trace. Occurrence features follow event order: the start-side
value is the selected occurrence start minus the first event completion; the
end-side value is the last event start minus the selected occurrence completion.
Negative values are retained. When no start key is configured, completion is
also used as start. Only the endpoints and selected occurrences needed by these
features require timestamps; unused middle events do not trigger conversion.
Missing activities retain the configured default-not-present value.
Work-in-progress and resource workload expand the first-start/last-completion
interval by epsilon at both ends and count strict interval overlaps. Identical
intervals belonging to different traces remain separate observations. Required
boundary timestamps must be present, and the expanded start must precede its
end; unused middle timestamps are ignored. An empty trace keeps the configured
WIP default but is invalid for resource workload. Workload accepts present string
resources, including empty strings, omits missing resources, and rejects other
types. Each resource participates once per trace, and absent-resource columns
retain the configured default.
Indirect-path timing considers every later event with at least one intervening
event, retains the last positive gap per activity pair in input order, and
preserves defaults when no positive gap is observed.
Concurrency counts subsequent events in input order, stopping at the first
start after the anchor completion; epsilon does not relax that comparison.
General and per-activity columns use zero for empty traces and absent activities.
Only accessed anchor completions and subsequent starts require timestamps.
TRACE timing reads preserve authoritative native date values, projection source
keys, and null/missing state instead of relying on stale lexical projections.
Trace numeric selectors whose columns occur in the schema convert every present
value before retaining the last event value, so an earlier invalid value is
still an error. Columns excluded by an explicit schema skip numeric conversion.
Numeric byte values are decoded before conversion, Unicode numeric strings and separators are
accepted, and null/container values are rejected. A selected event numeric
attribute must occur in every trace, and selected trace numeric attributes must
be present; an empty log still yields an empty matrix.
Result serialization enforces one row family per variation, rectangular
matrices, aligned case identifiers, and finite temporal coordinates. Temporal
schemas retain unique non-empty name validation; event and trace schemas preserve
their respective explicit-name behavior above. Finite numeric cells use
round-trip precision. Native NaN and infinities survive extraction; JSON encodes them as the exact strings
`NaN`, `Infinity`, and `-Infinity`, accepted by the typed frontend result guard.
Homogeneous default trace inference requires an attribute somewhere in every
trace (event attributes) or on every trace (trace attributes), excludes the
canonical trace identifier, and omits boolean-only attributes. Ordinary inferred
string attributes have a fixed limit of 12 distinct values; explicit selection
and the forced activity representation bypass that limit.
Default trace inference still needs broader proof: mixed-type selection and
ordinary attribute ordering vary across hash seeds in the pinned runtime.
Default inference validates hashability before completeness and type filtering:
mutable lists, mappings, sets, and immutable containers containing mutable
children raise. Canonical trace identifiers are excluded from this validation;
event activity values are included. Null values remain hashable. Explicit
ordinary selectors bypass this default validation and retain string conversion
for selected categorical containers.

Case identifier extraction inserts `@@case_id_column` before the core and extra
schemas. The configured trace key defaults to `concept:name`; missing values
raise, while empty strings and nulls remain distinct values. Native trace rows
retain the original typed attribute for this cell. JSON emits scalar strings,
safe integers, floats, booleans, and nulls directly. Larger integers retain exact
canonical text in attribute details; bytes and nested containers use that same
representation to preserve their type and children.
Only the leading identifier column permits these typed cells; other feature
columns retain numeric validation. Duplicate custom identifier names remain
separate zero-valued core columns. Supplemental `case_ids` remain available.
Calendar temporal projections materialize every bucket between the first and
last observed bucket, with zero metrics for empty buckets. Multi-period ticks
anchor to the earliest event-start day; weekly and month/quarter/year periods
anchor to the first natural boundary and advance by their configured multiple.
Calendar steps preserve leap days and month ends. Fiscal quarter/year frequencies
support `QE`, `QS`, `YE`, and `YS` with all twelve month suffixes and positive
multipliers. Period
starts and ends follow the selected fiscal month; multiple periods advance from
the first natural label for the input, including fixed-offset and pre-epoch
dates. Permanent public oracles cover all month anchors and one-, two-, and
three-period groupings. Weekday-based business frequencies cover `B`, `BME`,
`BMS`, `BQE`, `BQS`, `BYE`, and `BYS`, including fiscal quarter/year anchors and
positive multipliers. Business-day bins roll weekends back to Friday. Business
period ends use the last weekday and send subsequent weekend events to the next
period; starts use the first weekday and retain earlier weekend events in the
previous period. Empty grids advance through business labels from the first
observed bucket, preserving local fixed offsets and pre-epoch dates.
Semi-month frequencies use `SME` (configured day and month end) or `SMS` (month
start and configured day), defaulting to day 15. Explicit days range from 1 to 27
for `SME` and 2 to 27 for `SMS`. Week-of-month frequencies select the first through
fourth occurrence of any weekday with `WOM-1MON` through `WOM-4SUN`. Both families
assign an event on a label day to the bucket beginning at that label, start at
the preceding natural label when necessary, and advance multiples from the
input's first label. Sparse
grids retain empty buckets, local fixed offsets, and pre-epoch boundaries.
Fixed ticks now support millisecond, microsecond, and nanosecond bins, including
observed fractional and composite frequencies. Native dates record microsecond
or nanosecond source resolution; microsecond columns reject tick widths that
cannot be expressed in whole microseconds. Exact integer arithmetic preserves
bucket membership and labels across pre-epoch dates and adjacent nanoseconds.
Temporal JSON rows carry an optional canonical decimal `timestamp_ns` string
alongside the approximate numeric timestamp; frontend guards preserve this field.
Public fixtures cover source-resolution promotion, fractional truncation, sparse
grids, fixed offsets, case-rate ties, and case-sensitive frequency aliases.
Numeric case metrics retain observed floating-point timestamp behavior.
XES import preserves up to six fractional digits, truncating finer fractions;
both XES exporters preserve those microseconds in first-class timestamps and
log, trace, event, and nested date attributes, including compressed output.
Explicit nanosecond native dates retain up to nine digits on export; re-import
still follows the microsecond source contract. Native export preserves source
clock fields and timezone offsets without converting fractions through floating
point; UTC emits `+00:00` and naive dates remain without an offset. Zero fractions
are omitted, microsecond values use six digits, and genuine nanosecond values
use nine. Import offset handling follows the selected variation as described
below, so ordinary iterparse re-import retains clock fields while Rust import
preserves absolute instants.
Fractional tick coefficients now follow the observed double conversion and
truncation behavior. Each compound component normalizes independently through
direct conversions to finer units (days begin at hours), accepting an integer
when its residual is at most `1e-8`. Tiny components may become zero, while
some mathematically integral nanosecond widths reject because of their floating
residual. A zero total width rejects. Public observations across 7,707 positive
frequency inputs support this rule; 180 permanent boundary scenarios verify
acceptance, exact labels, compound syntax, and all thirteen output metrics.
Custom business calendars and exhaustive frequency-alias and adapter coverage
remain open. These fixtures do not establish full temporal-feature parity.
Fixed-width windows remain a separate native projection. Calendar metrics use completion-time case spans:
configured start timestamps control grouping only, default service time is zero,
and waiting time equals sojourn time. Means use distinct cases rather than event
weights. Calendar output always uses the canonical thirteen columns, ignoring
custom feature schemas and ordinary attribute selectors. Fixed-width projections
retain their explicit schema and interval metrics. Calendar activity and resource
categories preserve typed equality, including numeric/bool collisions, distinct
strings, bytes, and ordinary immutable containers. Empty strings count; null, NaN,
and missing values are excluded from category sets. Within valid cases, rework
counts events with absent activities; resource means include cases without resources.
Missing
entire selected columns and eventless calendar inputs reject. Mutable categories
reject instead of being converted to strings. Nested container factorization,
frozen-set and NaN-containing container ordering, explicit dataframe dtype handling,
metric-column collisions, and named-zone calendar behavior still need
further behavioral alignment.
Calendar cases group by typed selected identifiers across native trace boundaries.
The omitted key selects projected trace identifiers (`case:concept:name`), while
an explicit `concept:name` selects event activities. Trace attributes project with
a `case:` prefix and override conflicting event cells; unprefixed custom keys
select event attributes. Missing, null, and NaN IDs keep their bucket events but
do not contribute to case aggregates. A populated bucket without a valid case
has an empty-string rework cell and zero case means; missing entire ID columns
and logs without any valid ID reject. Trace feature identifier defaults remain
`concept:name`, independently of the calendar default. Scalar case IDs break
equal arrival/finish-time ties using native numeric, string, byte, or date order.
Integer ordering preserves values beyond float64 precision. Incompatible scalar
tie comparisons reject, while mixed strings and other scalar types remain valid
when their metric endpoints differ. Incompatible non-string scalar case groups
reject during group ordering, before temporal aggregation. Scalar resource groups
apply the same compatibility check within each populated bucket; incompatible
types in separate buckets remain valid, as do mixed strings and numeric resources.
Comparable tuple case identifiers use recursive lexicographic ordering for tied
arrival and finish endpoints, including nested tuples, exact numeric members,
equal null prefixes, and differing lengths. Group compatibility is checked
separately from endpoint tie comparisons: strings and null members can coexist
with otherwise comparable tuple groups, while incompatible numeric, date, byte,
or nested sequence positions reject at the observed grouping stage. Resource
compatibility remains bucket-local; activity grouping does not impose ordering.
Temporal category lookup merges distinct NaN objects along tuple-only nesting,
including nested tuples used as case identifiers. NaN remains distinct from null.
Frozen-set boundaries retain identity-sensitive NaN equality, whether the frozen
set contains a tuple or appears inside one. Separate lookup keys preserve the
original category representatives and native input attributes; shared key
equality outside temporal extraction is unchanged. Equal nested null tuples
continue to merge.
Homogeneous frozen-set case identifiers require two ordering stages: object-key
group ordering followed by whole-list endpoint/key comparisons. Proper subset
comparisons do not define a total order, so encounter order, endpoint-only ranks,
and independently sorted tie groups are insufficient. Public fixtures cover
subset chains, incomparable sets, mixed endpoints, and larger collections up to
1,000 cases; frozen-set and NaN-containing mixed domains remain partial.
Nested tuple comparisons stop at the first unequal member, including frozen-set
members. A differing earlier member prevents inspection of incompatible later
members. Frozen-set/scalar grouping rejects numeric, byte, and date comparisons;
strings and missing tuple members can group but reject when an endpoint tie
requires their direct comparison with a frozen set. Resource checks retain their
bucket-local scope. Tuple NaN ordering uses the original member identities:
shared NaNs permit comparison of later members, while distinct NaNs stop numeric
tuple comparison without ordering either value before the other. These ordering
rules do not change the separate tuple-NaN category lookup normalization.
Calendar EventLog projections infer numeric promotion independently for selected
activity, resource, and case-ID columns across the entire log. Float or missing
cells promote eligible integer columns to float64 before category grouping;
integer-only columns preserve exact values. Booleans, strings, and other nonnumeric
values block promotion. Integers outside the combined signed/unsigned 64-bit range,
or unsigned-range integers mixed with negative integers, also preserve object values.
Promotion can merge large adjacent IDs and changes case
metrics, distinct category counts, and rework consistently without changing the
native input attributes.
Calendar case, activity, resource, completion, and start selectors share the
projected trace-column view. A trace attribute `team` appears as `case:team`,
including null values that mask conflicting event cells; absent trace attributes
preserve event fallback. Already-prefixed trace attributes receive another prefix.
Native date values override stale lexical metadata for timing, while actual string
completion cells reject. Missing completion values are ignored when finding case
time bounds; each valid case must retain at least one completion. Null or missing
starts exclude events from calendar buckets but preserve their contribution to
whole-case timing. An entirely missing start column rejects.
Calendar grouping preserves consistent fixed offsets in the selected start
column: local day and period boundaries become absolute output instants, including
empty grid rows. Completion timestamps may use a different consistent offset;
case durations still use absolute elapsed time. Mixed offsets or mixed naive and
aware dates within either selected column reject. Native date payloads remain
authoritative over stale lexical metadata. Named timezone transitions are not
represented by this fixed-offset contract.
Public XES importer probes establish that `ITERPARSE`, `LINE_BY_LINE`,
`ITERPARSE_MEM_COMPRESSED`, `ITERPARSE_20`, and `CHUNK_REGEX` attach UTC while
retaining input clock fields, including nonzero-offset inputs. Native import
now follows this behavior for date attributes and selected timestamps.
`RUSTXES` retains absolute-instant normalization instead, matching its observed
file-import backend. Six-digit fractions survive and finer fractions truncate.
Nested scalar and list imports now follow variation-specific shapes, with 128
permanent public-category fixtures across event and trace attributes.
`LINE_BY_LINE` flattens recursive children into the containing attribute map;
later duplicate keys replace earlier values, including selected first-class
keys. `CHUNK_REGEX` drops scalar children and retains list children as a mapping.
`ITERPARSE` and `ITERPARSE_MEM_COMPRESSED` preserve scalar children as mappings,
wrapped list entries as ordered pairs, and direct list children as mappings.
`ITERPARSE_20` retains direct list entries as ordered pairs but ignores entries
inside a `values` wrapper. Native Rust scalar imports discard child metadata
and retain the first duplicate attribute. Rust lists and containers now import
as debug-formatted strings, retaining ordered duplicate entries, nested type
names, and scalar child attributes. Broader unknown-tag, table-shape, and
empty/malformed shape edges remain open. Public exporter probes cover 288 combinations of nested shapes,
event/trace/log/global scopes, both export variations, and plain/gzip transport.
Native dictionary-shaped list exports now emit `list`/`values`: `ETREE` writes
typed mapping entries, while `LINE_BY_LINE` uses the first two Unicode code
points of each mapping key as a string key/value pair and rejects shorter keys.
Ordered pair lists retain duplicates; mapping children use last-value replacement
at the first insertion position. Recursive writers and first-class projections
carry the selected variation. Permanent tests cover these rules at all four
scopes and both first-class name projections, with plain and gzip output.
The scalar-wrapper checkpoint below adds default null omission and bounded
exporter-specific boolean/date lexical behavior. The explicit-null extension
remains opt-in, and the exporter coverage units are still partial.
The public Rust wrapper lacks string deserialization; complete file/string
adapter and result-type equivalence remains unproven.
Calendar metric destination options expose arrival, finish, service, waiting,
sojourn, and per-event difference scratch columns, including empty names. Ordered
virtual writes let selected category columns and shared metric destinations
observe overwritten values while preserving native input attributes. Canonical
output names remain unchanged. Destination collisions with case and date columns
follow the validated grouping and timing stages; exhaustive collision coverage
remains open.
Per-case metric representatives select each destination's first nonmissing value
in configured-start order within its bucket. Missing values do not poison case
means, and all-missing metric buckets yield zero, including after difference
writes merge events that originally lacked case identifiers.
A permanent 128-case oracle matrix covers every single default scratch-column
alias, every pair of shared metric destinations, all six sharing a destination,
and activity/resource overwrites across daily and thirty-second buckets. The
fixture varies case lengths, durations, activities, resources, and event order;
it does not establish exhaustive coverage for arbitrary multi-column collisions.
The feature variations remain partial, including inference and extra-feature
contracts that have not received complete behavioral coverage.

Case- and event-level embeddings support deterministic local encoders and an
injectable transformer runtime with inferred or explicit dimensions. Model,
provider, attribute, identifier, result-size, and vector-shape contracts are
validated consistently for transformation, similarity ranking, filtering, and
JSON output. Similarity results reject duplicate identifiers, non-finite or
out-of-range cosine scores, and rows exceeding the requested result bound.

Trace-attribute clustering covers all five distance/profile variations,
including edit, vector, and directly-follows representations, deterministic
hierarchical merging, bounded output, and label selectors. Profile clustering
provides deterministic seeded centroid initialization, categorical, numeric,
activity, and succession features, explicit schemas, convergence diagnostics,
and stable partitions. Serializers verify tree leaves and node diagnostics as
well as complete, non-overlapping cluster membership and finite centroids.

## Privacy, optimization discovery, and simulation parity

Privacy transformations include bounded trace-to-source assignment with
attribute suppression and synthesis, plus Laplace and behavior-aware
trace-variant release. Both deterministic seeds and operating-system entropy
are supported. Result contracts verify positive finite privacy parameters,
released-log and frequency totals, retained-variant counts, matching and
synthesis partitions, and non-empty activity labels.

Classic genetic discovery exposes bounded population, generation, elitism,
crossover, mutation, state, selector, and seed controls. Its causal matrix,
workflow conversion, markings, and diagnostics are serialized together and
validated for dimensions, activity identity, member indices, and finite
fitness. Classic integer-programming discovery covers sequence encoding,
artificial boundaries, support and noise filtering, causal-region restriction,
bounded branch-and-bound search, workflow markings, and exhaustion
diagnostics; output validation rejects malformed arcs, markings, and search
counters.

The virtual-time FIFO simulation supports all declared duration families,
fitted or explicit stochastic maps, token-replay variants, custom markings,
resource pools, seeded transition selection, case arrivals, bounded execution,
and optional diagnostics. Serialized results require a complete case-outcome
partition, finite non-negative timing metrics, valid execution samples, and
contiguous finite place and transition interval trees.

## Browser-history connector evidence

The native connector reads Chrome `History` and Firefox `places.sqlite`
databases in read-only mode, either from an explicit database file or from
profile directories immediately below an explicit root. It never searches a
user profile implicitly and never launches or controls a browser. Both
variants normalize profile names to cases, navigation targets to activities,
microsecond timestamps to UTC, and preserve browser-specific transition,
referrer, duration, title, frequency, and identifier attributes. Profile and
event ordering is deterministic, and a strict event limit bounds ingestion.

The standalone export CLI accepts `--browser-history chrome_history` or
`firefox_history` with the input path and an optional event bound. The local
`POST /api/browser-history` endpoint and typed frontend query/result guards use
the same explicit-path contract and `BROWSER_HISTORY` JSON envelope. Synthetic
SQLite fixtures cover both schemas, timestamp epochs, URL normalization,
transition labels, referrers, directory and single-file input, and invalid
bounds without accessing installed browser data.

Exported Windows event XML uses computer names as cases, provider plus event id
as activities, creation time as timestamps, and security identifiers as
resources. System metadata, rendered messages, and named or unnamed event-data
fields remain event attributes. Namespaced documents, entity decoding,
deterministic computer/timestamp ordering, strict timestamps, and bounded
result sizes share the native, CLI (`--windows-events`), local
`POST /api/windows-events`, and typed frontend contracts. The connector reads
only an explicitly supplied XML file; it does not query the live event service,
request elevation, start a management process, or create subscriptions.


## XES scalar-wrapper checkpoint

`EventAttribute` retains scalar-wrapper identity and ordered scalar children
through native conversion, projections, and typed JSON. Ordered children preserve
duplicate keys; mapping children retain last-value replacement. Public null
scalars are omitted by default, while wrapped nulls become list structures.
`preserve_nulls`, CLI `--xes-preserve-nulls`, HTTP `xes_preserve_nulls`, and the
frontend download control retain the native explicit-null extension. Its ETREE
ordered children also preserve duplicate keys.

Boolean/date export spelling depends on the exporter and wrapper shape. Dates
retain source clock fields and offsets, with six-digit microseconds and native
nine-digit submicrosecond precision. LINE_BY_LINE import now follows the public
lexical line behavior, including packed tags and entity text, rather than
assuming the ordinary XML parser's interpretation.

`docs/xes-foundation-reference-fixtures.json.gz` retains six reproducible public
captures: 128 null export observations; 96 boolean exports and 120 imports;
276 date exports and one construction rejection; 60 empty-wrapper imports;
45 scalar-child imports and 24 exports; and 124 plain/gzip line-import cases.
Native comparisons match all 45 scalar-child and 124 line-import cases. Error
comparisons establish rejection only, not exception-class/message equivalence.
The other captures supply public observations for native regression fixtures,
not automatic exhaustive cross-implementation comparisons.

The artifact records runtime settings, native executable hashes where used,
and opaque normalized byte verification for 22 available XES/log/date modules
against PM4Py 2.7.22 commit `0715c31f3f1d85924107d6aa5329806db69df899`.
The optional `cs8601.py` module is absent from this runtime and is not verified.
Coverage remains partial: these finite corpora do not establish arbitrary XML,
timezone/date-range, Rust raw-table, or complete importer/exporter equivalence.

Validation passed: all 150 Release CTest suites, 617 frontend tests, TypeScript
checking, production frontend build, and 23 evidence-script tests (including
five focused tests repeated under Python optimization). CLI/HTTP checks cover
null defaults, opt-in preservation, gzip, and invalid options. Independent
review findings were corrected and reviewed again. The structural catalogue
remains 44/86 dispatchers and 195/387 non-dispatcher modules complete.


## Rust XES table and conversion checkpoint

The additive `import_xes_result` API supports legacy EventLog, eager DataFrame,
and LazyFrame results. Legacy mode takes precedence; the EventLog convenience
API always returns a log. Raw modes preserve rows before case grouping and
include column-aligned dtype names. The native lazy scanner owns an eagerly
parsed in-memory table, and JSON output materializes rows.

Rust scalar conversion retains missing/NaN placeholders through first-duplicate
selection and schema inference. Nullable numeric columns, mixed-column display,
UTC date instants, invalid scalar fallback, and generated empty column names
follow the observed backend. Lists/containers become debug strings that retain
nested types, ordered duplicates, and Unicode escaping. Empty traces contribute
to schema before being omitted from legacy output.

Legacy reconstruction groups rows by the selected case column, preserving
first-seen group and event order. It normalizes trace prefixes, projects selected
first-class fields, and infers standard extensions. Explicit extension lists
replace inference and use last-value replacement for duplicate names. CLI
return/conversion flags expose these results; raw modes ignore conversion flags.

These behaviors target PM4Py 2.7.22 with r4pm 0.6.2, pandas 3.0.5, and Polars
1.44.1. Column ordering and collision outcomes can vary; finite sampled support
is not evidence of exact distribution equivalence. Python-only parameter object
kinds, arbitrary malformed XML, runtime object identity, true deferred I/O, and
complete adapter equivalence remain unproven. Rust importer coverage stays partial.


The Rust implementation checkpoint is `822afe2`. Its isolated Release build and
all 150 native suites passed. The reproducible corpus covers 77 raw-return,
41 conversion, seven conversion-schema, 26 extension, and 42 empty-boundary
observations. Native checks exercise each mapped observation four times in
plain and gzip form: 616, 312, 56, 160, and 336 comparisons respectively.
Two conversion and six extension observations use Python-only parameter objects
that the native CLI cannot represent; they are retained as explicit exclusions.

Additional comparisons cover 140 metadata cases (including two expected
rejections), 384 container strings, 513 scalar cases, 62 UTC-date cases, and
1,111,934 valid non-ASCII XML code points. Grouping uses 64 samples per side for
108 cases and reaches all 106 observed variable outcomes. These are finite
behavioral checks; rejection comparisons do not establish exception-class or
message identity, and sampled support does not prove equal probabilities.

Public gzip replay retains exact generated column names. Strict differences
caused by variable names remain in the reports; optional paired 64-sample
plain/gzip support comparisons require both the historical expected result and
the triggering gzip result to occur in the matching observed supports. No
name normalization or probability-equivalence claim is used for that check.


`docs/xes-rust-reference-fixtures.json.gz` stores 44 reports as exact UTF-8
`json_text` strings with SHA-256 digests, preserving the bytes needed to verify
cross-report input hashes. The `xes-rust-reviewed-*` reports exercise the final
manifest and provenance gates; earlier `xes-rust-final-*` reports retain the
broader scalar/grouping/Unicode runs and their original inputs. The bundle also
records 33 opaque reference-module comparisons, dependency versions/backend
binary hash, and the native executable hash tied to implementation `822afe2`.
To restore a report for a checker, UTF-8 encode its `json_text` value unchanged.

All 1,480 mapped native comparisons passed the repaired gates. Public gzip
replay checked 193 observations: 189 strict matches and four retained strict
differences resolved by equal observed support over 64 samples per input form.
No gzip observations were skipped. The gates reject incomplete/duplicate
manifests, reported capture errors, invalid provenance, and zero comparisons;
native comparisons hash their executable before and after running. The complete
script suite passes 47 tests, with the 24 focused Rust gate tests also passing
under Python optimization. Structural coverage remains partial and unchanged.
