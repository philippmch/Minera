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
adapter, integration, or missing are complete. The current manifest records 46
complete and 40 partial dispatcher rows, plus 199 complete and 188 partial
non-dispatcher module rows. The completed module group covers the implemented
simulation, core transformation, Petri-analysis, organizational, statistical
DFG, causal and correlation discovery, 26-module Inductive discovery, 31-module
POWL discovery, 13-module footprints discovery/conformance, and the 19-module
temporal/behavioral adapter stack, plus discounted anti-/multi-alignment and
classic Declare/log-skeleton discovery and conformance, with distinct
registered tests, plus the replay-duration diagnostic support module and the
backwards token-replay variation and CLASSIC heuristic discovery; 228 coverage
units still require exact proof.
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
| Concept drift | BOSE relation-type feature extraction and permutation-window drift detection | **Complete** — module and dispatcher contracts cover all seven public functions, eight apply parameters, typed labels, signed controls, EventLog/EventTable adapters, diagnostics and RNG continuation; exact proofs and reviewed ordering distinctions are recorded in `docs/bose-parity-contract.md` |
| Conformance: Petri alignments | state-equation A*, Dijkstra no heuristics, Dijkstra less memory, discounted A*, generator Dijkstra no heuristics, generator Dijkstra less memory | **Partial (implemented core; proof pending)** — all six variants are explicitly selectable; less-memory Dijkstra with legacy unit costs is the generic default, with named event activity fields available at replay time |
| Conformance: other alignments | decomposed recomposition, DFG classic, edit distance, process-tree search graph/dynamic programming/MILP/approximated original | **Partial (implemented core; proof pending)** — `RECOMPOS_MAXIMAL`, DFG classic, edit distance (`EDIT_DISTANCE`), and all four process-tree variants are native selectable APIs with JSON, CLI, server, and frontend adapters; strict activity selectors apply to every family and DFG/edit projections additionally expose timestamp selectors |
| Conformance: replay | forward token replay and backwards token replay | **Partial (duration support complete; replay proof pending)** — forward replay remains the default; grouped/stream/eager/lazy replay preserves null activities, typed case identity, selector precedence, adapter-specific case ordering, and event caps; both variations expose per-case counters, ordered silent/visible firings, event-level problems, unknown-event positions, reached markings, and enabled-transition IDs; duration cohorts have independent support proofs and native/JSON/CLI/server/frontend adapters, while exact routing/flood-cleaning and root-cause support remain pending |
| Conformance: relational | footprints log-model/log-extensive/trace-extensive; log skeleton; Declare | **Complete** — all three footprint selectors have distinct production components and registered contracts, including strict/non-strict relations, extensive diagnostics, tree-node decoration, and evaluation utilities; classic log-skeleton and Declare conformance add trace/log/flat adapters, strict activity and case selectors, exact empty/subset family-selection behavior, typed per-case diagnostics, and distinct registered proofs |
| Conformance: temporal | temporal profile log/dataframe adapters, zeta threshold, business-hour slots | **Complete** — log and dataframe contracts preserve one result bucket per case, apply strict configurable selectors and business-hour elapsed time, handle zero-variance profiles deterministically, and expose grouped diagnostics through native JSON plus the cached local conformance endpoint and typed frontend adapter |
| Conformance: advanced | Petri-net anti-alignments and multi-alignments (discounted A*) | **Complete** — both `VERSION_DISCOUNTED_A_STAR` algorithms preserve the asymmetric discounted recurrence, complete model-transition runs, raw ordinary-distance borders, deterministic longer-run ties, exact marking-limit boundaries, objectives, precision/distance diagnostics, arbitrary markings, strict activity selectors, and JSON/CLI/server/frontend adapters, with distinct registered proofs |
| Conformance: object-centric | ETOT, OCDFG and OTG graph comparison | **Partial** — deterministic ETOT, OCDFG, and OTG graph comparisons with strict thresholds, weighted fitness, diagnostics, direct-OCEL and explicit-graph native APIs, graph-vs-graph and OCEL-vs-graph headless CLI/local-server/frontend adapters, strict direct-OCDFG activity/object-type selectors, and cross-family envelope rejection; broader OCEL schema coverage remains |
| Discovery: Alpha | classic and Alpha+ | **Partial (classic, abstraction, and endpoint modules complete)** — grouped/stream/eager/lazy inputs, sparse weighted graphs, explicit and inferred endpoints, typed selectors, table ordering/null contracts, and native/request/CLI/frontend event caps are implemented; Alpha+ exposes raw live/detached markings and reserved-label collision cleanup through native/JSON/CLI/HTTP adapters, with full public/native process captures; its selection law remains partial because native repeated calls vary where the observed public cells stay stable |
| Discovery: Inductive | IM, IMf, IMd | **Partial (feature surfaces complete; differential proof pending)** — `log`, explicit `imf`, and graph-recursive `dfg` selectors have distinct native semantics; EventLog, stream, eager/lazy table, weighted-variant, and direct-DFG inputs are available; specialized fall-throughs, strict-sequence and fall-through toggles, field selectors, regrouping, timestamp ordering, recursive JSON, request/CLI/server/frontend plumbing, and model-specific downstream replay are covered by focused tests |
| Discovery: Heuristics | classic and Heuristics Miner++ | **CLASSIC complete; dispatcher and PLUSPLUS partial** — CLASSIC public EventLog/DFG object and Petri routes, typed returned fields, order, noise pruning and conversion have saved native/public proof; separate dispatcher and PLUSPLUS obligations remain |
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
The acceptance criteria for incidental choice order and complete replay-path
diagnostics are recorded in `docs/token-replay-parity-contract.md`; sampled
support alone does not close the remaining choice/cleanup evidence gap.
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
Conversions validate unique column names (including a single literal empty
name), rectangular rows, non-null case
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
prefer shorter routes, then source place names, then target place names. The
supplied-PNML comparison route preserves original place and transition IDs for
these choices. Pre-event
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

Default pre-event routing fires enabled path prefixes until the target becomes
enabled or a candidate yields no new firing, then recomputes candidates and
explores blocked dependencies. Each recursive invocation freezes its initially
deficient target input places; recomputation uses those same target places with
the updated marking keys. A prefix that supplies an input does not remove that
input from the invocation's later path search. Pre-event paths prefer shorter
routes, then source place names, then target place names. Routing stops before
dependency exploration if the prefix already enables the target. Visited targets
remain visited even if they never fire, and a blocked step does not discard later
steps of its path.
With `exhaustive_invisible_exploration=true`, candidate paths supply a transition
pool in first-occurrence order. Breadth-first exploration tries enabled pool
transitions on temporary markings, using each transition at most once per trial
and excluding transitions already visited by the surrounding search. Only a
sequence that enables the selected target is retained. Successful trial firings
do not themselves mark those transitions visited in the surrounding search.
This prevents a fallback dependency from refiring a visited producer merely to
fill a weighted deficit. The native trial search reports its 4,096-state bound
and 64 MiB retained-node payload budget explicitly; temporary firing scratch
is outside that accounting. If no trial succeeds, dependency
exploration runs without the preliminary
prefix pass. Both modes allow two invocation levels. The default dependency
phase continues across candidate paths, checking target enablement within each path.
This option defaults to false and is available in the native API,
`--compare-petri-exhaustive-invisible-exploration`, strict server parsing, and
the frontend comparison controls.

The exhaustive-path checkpoint retains 20 inputs in
`docs/token-replay-exhaustive-path-reference.json.gz` and a separate element-usage
reference. All 20 usage comparisons and the routing, marking, fitness, and token
counters in all 40 replay observations match. The subsequent enabled-transition
checkpoint also closes the four diagnostic mismatches originally recorded in
`docs/token-replay-exhaustive-path-validation.json`, so all 40 complete replay
observations now match. Reference results were
identical across three fresh processes with hash seeds 0, 1, and 17. The existing
160 duplicate-label and 55 nested-route observations still match.

The deficient-input checkpoint in
`docs/token-replay-deficient-input-reference.json.gz` retains 144 inputs with
288 replay observations, plus 144 separate element-usage comparisons. All match
exactly, including reversed construction order, weighted arcs, token counts,
both exploration modes, and immediate-stop controls. Results agree across three
fresh processes with hash seeds 0, 1, and 17. Its validation record also preserves
the passing exhaustive, duplicate-label, nested-route, and enabled-transition
regressions. These finite checks do not establish full parity.

The visited-trial checkpoint in
`docs/token-replay-visited-trial-reference.json.gz` retains 84 inputs, 168 replay
observations, and a separate reference with 84 element-usage comparisons. All
match exactly and agree across three fresh processes with hash seeds 0, 1, and
17. Public helper probes separately distinguish skipping a visited prefix from
discarding its whole path, and confirm that successful trials leave the visited
set unchanged. The validation record preserves the preceding routing regressions.

Final routing uses the same source-before-target ordering. The checkpoint in
`docs/token-replay-final-order-reference.json.gz` retains 96 inputs with 192
replay observations and a separate reference with 96 element-usage comparisons.
All match exactly, including source-place renaming, reachable and unreachable
final targets, token counts one through four, and disabled-routing controls.
Results agree across hash seeds 0, 1, and 17. The validation record preserves the
passing prior routing checkpoints and all 154 native test suites.

The combined-pool checkpoint in
`docs/token-replay-exhaustive-pool-reference.json.gz` retains 96 inputs with 192
replay observations and a separate reference with 96 element-usage comparisons.
All match exactly across token counts one through four, traces of one through
three repeated events, and immediate-stop controls. Results agree across three
fresh processes with hash seeds 0, 1, and 17. Its helper model artifact preserves
1,400 exact public comparisons over 700 nets, including pre-visited transitions.
The shortest enabling sequence may combine transitions from separate candidate
paths; trying each structural path independently does not reproduce this behavior.

The flood-cleaning repeated-event checkpoint in
`docs/token-replay-flood-multievent-validation.json` retains 148 acyclic inputs:
131 initial observations match exactly and 17 remain strict differences.
Resampling those 17 inputs yields 221 exact matches among 484 observations;
all 263 strict differences are preserved. Each affected input produces an exact
native match in both diagnostic modes within the sampled public outcomes.
This establishes finite sampled support only, not equal outcome probabilities
or correlations. One resampling batch reached its 20-second bound with four
of 30 requested observations persisted; all 148 initial isolated captures
completed within their individual five-second bounds. No production behavior
or coverage classification changes in this evidence checkpoint.

Eventually-enabled visible transition diagnostics use the observed public FIFO
worklist behavior. Silent IDs may occur more than once in the queue, each ID uses
its latest discovered marking, and transition/marking pairs are deduplicated when
popped. Discovery visits transitions in name order. This differs from exact
marking-state reachability; precision retains its separate exact search, and
extended-arc nets retain their existing native diagnostic search.
`docs/token-replay-enabled-transition-reference.json.gz` preserves 1,259 inputs
and 2,518 complete replay observations, including weighted repeated firings,
renamed competing routes, and 129 silent self-loops. All match the strict native
checker. Eighteen focused inputs also agree across hash seeds 0, 1, and 17.
Token-producing cyclic public probes exceeded an eight-second bound; native
diagnostics report explicit resource errors at the search limit rather than
returning partial transition sets. Validation and the bounded reference outcomes
are retained alongside the main fixture artifact.

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

### BOSE semantic completion

The BOSE module and its dispatcher now satisfy the semantic completion criteria.
The fresh requirement audit maps all seven public functions and eight apply
parameters to native implementations and committed behavioral evidence.
Independent scope review confirms that the earlier NA, timestamp, frozen-set,
and mutable-alphabet gaps are resolved. Exact registered module/selector proof
cases are `bose_module_contract` and `bose_dispatch_contract`.

The current core has 20,886 exact matches across 20,899 comparisons, with 13
documented unique-list ordering differences retained as differences. The
member, strict-subset, drift and RNG invariants supporting those alternative
orders are preserved. Two native suites and 14 CLI checks pass. Typed EventLog
evidence directly exercises default dispatch, explicit BOSE and the adapter,
including the eight tuple/NaN cases formerly absent from dispatcher evidence.

`docs/bose-parity-contract.md` defines the reviewed contract;
`docs/bose-parity-completion-audit.json.gz` preserves the requirement and scope
audits; `docs/bose-parity-validation.json` links current-core evidence and
catalogue gates. These two complete records leave 232 incomplete coverage units.
Project-wide full parity is not achieved.

The checkpoint sections below preserve the development history. Their statements
that BOSE remained partial describe those earlier checkpoints and are superseded
by this completion audit and the current coverage records.

### BOSE empty-label relation-feature checkpoint

Automatic activity extraction now retains empty-string labels, including labels
selected from generic event attributes. An empty label followed by `A` produces
the alphabet `["", "A"]` and feature vector `[0, 0, 2, 1, 0, 1]`, matching the
pinned public API. Previously the empty label disappeared from the alphabet and
changed the feature dimensions and relation counts.

`docs/bose-features-reference.json.gz` captures 5,958 public relation-feature
cases: empty logs, one or two traces of up to four events, repeated activities,
empty/space labels, and an additional absent alphabet member. All 5,958 native
comparisons pass after the correction, versus 2,110 before it. The native checks
artifact retains both outcomes, probe source, and a passing XES/CLI custom-key
check. The focused concept-drift suite also passes, covering automatic and
custom-key discovery. `docs/bose-features-validation.json` records the built
code tree and artifact hashes. The BOSE module and dispatcher remain partial:
this finite feature checkpoint does not establish randomized window-test or
complete adapter parity.

### BOSE returned-log content checkpoint

The pinned public `bose.apply` reconstructs each returned segment with case IDs
starting from zero and synthetic event timestamps. Repeated output case IDs do
not imply repeated input cases. Native returned logs retain the original trace
and event metadata as an extension, so behavioral comparisons use ordered
activity sequences within each returned trace and segment.

`docs/bose-returned-logs-reference.json.gz` retains 240 public observations across
raw sub-log sizes 1, 2, and 3; zero to three behavioral segments; unequal segment
lengths; and trailing partial sub-logs. All 240 comparisons match the existing
native implementation by returned activity sequences, event counts, and selected
boundary sets. The fixture also retains generated case IDs, output timestamps,
and drift p-values. `docs/bose-returned-logs-native-checks.json.gz` records the
native outputs and comparison probe; the validation file identifies the unchanged
built code and reused native-suite evidence. Exact randomized p-values and ranked
boundary order are not compared. No production behavior or coverage classification
changes in this checkpoint; window-test and adapter proof remain incomplete.

### BOSE activity-input validation checkpoint

Detection validates the selected activity on every input event before feature
extraction. Missing, null, and non-string scalar or container values are rejected
instead of skipped or converted through a text projection. Standard activity,
resource, lifecycle, and timestamp aliases respect first-class presence and
attribute metadata. An explicitly present empty string remains a valid label;
an absent first-class field is distinct from it. Empty traces are rejected, while
an empty log is accepted. Standalone relation-feature utilities retain their
separate string-vector behavior.

`docs/bose-activity-input-reference.json.gz` retains 244 public EventLog
observations for the standard activity, resource, lifecycle, and a custom key.
The cases include missing and null values, integers, floats, booleans, dates,
lists, tuples, wrapped values, bytes, UUIDs, empty strings, and empty inputs.
Accepted string cases compare alphabets and feature vectors; rejected cases
compare acceptance, retaining the public exception details without equating
Python and C++ exception types. The native checks artifact and validation file
record all 244 matching outcomes (52 matched before the correction), four passing
XES/CLI checks, and 6,198 passing prior feature/segment comparisons. The focused
native concept-drift suite passes. Timestamp and case ordering,
dataframe adapters, and complete BOSE behavior remain outside this checkpoint.

### BOSE timestamp and JSON precision checkpoint

Concept-drift JSON emits finite doubles with round-trip precision and a fixed
decimal locale. Drift timestamps, raw sub-log start timestamps, feature values,
and p-values no longer lose digits at the JSON boundary. For example, epoch
`1577836820` previously appeared as `1577840000` through the CLI.

Timestamp extraction parses integral seconds and nanoseconds, forms the signed
epoch numerator exactly, and rounds its ratio to binary64 once. This also fixes
the earlier parser's digit-by-digit fractional rounding. Native seven-to-nine-digit
fractions remain supported; additional legacy forms, including longer fractions,
retain the existing parser path. The shared timestamp parser is unchanged.

`docs/bose-json-precision-reference.json.gz` retains 120 public EventLog timestamp
cases spanning years 2 through 9998, negative epochs, microseconds, and timezone
offsets, plus 1,040 finite-double serialization checks. All pass, along with 84
independent exact-rational nanosecond cases retained separately. The initial
reference artifact preserves the exact original 84-case public corpus (only
three timestamp cases matched before the correction) and its serialization
checks. Native checks retain the initial, intermediate, and final results.
The focused native suite, 6,442 prior BOSE comparisons, and five CLI checks pass.
These finite checks do not establish complete BOSE parity, pandas nanosecond
timestamp semantics, or validation of manually constructed nonfinite results.

### BOSE grouped-log timestamp and case-selector checkpoint

Detection validates the first selected timestamp of every input trace, including
traces inside a raw sub-log and calls with timestamp output disabled. Missing,
null, or invalid first timestamps are rejected. Later-event timestamps do not
participate in this validation; the first event supplies the case-start time even
when a later event has an earlier timestamp. Custom timestamp keys are honored.
The existing output flag still controls the timestamp arrays in the result.

An already grouped EventLog supplies its own trace boundaries. As in the pinned
public adapter, a custom case-id selector does not require a matching trace or
event attribute and does not regroup those traces. The selector remains in the
result metadata; the separate import/grouping surfaces retain their own rules.

`docs/bose-grouped-adapter-reference.json.gz` retains 288 public calls projected
to 576 native comparisons across both timestamp-output settings. Inputs cover
empty, single-case, and multi-case logs; sub-log sizes 1 and 2; standard and custom
selectors; missing, null, or invalid timestamps at different positions; and
reversed event times. Native checks retain acceptance, returned activity
sequences, raw sub-log starts, and drift timestamps. This grouped-log checkpoint
does not attest for dataframe grouping or arbitrary timestamp-cell type semantics.
All 576 comparisons pass, up from 429 before the change. The focused native
suite, 7,686 prior comparisons, eight CLI checks, and independent review pass.
`docs/bose-grouped-adapter-validation.json` records the verified build and evidence
hashes. No module or dispatcher status is promoted.

### BOSE seeded permutation-stream checkpoint

The native detector uses a 32-bit MT19937 stream with explicit descending
Fisher-Yates shuffling and bit-mask rejection sampling. Scalar seeds through
`UINT32_MAX` reproduce the pinned adapter's NumPy legacy random stream; wider
native seeds remain deterministic through a two-word seed sequence. This changes
the previous native seeded results, which used a different generator and the
standard library's implementation-dependent shuffle. Each permutation starts
from the original concatenated windows, and successive windows share the stream.

Public black-box probes compare shuffle outputs and final random states across
54 seed/length combinations, and permutation-test outputs across 48 calls.
The native detector is also compared against complete public adapter results,
including exact p-values, ordered capped boundaries, and returned trace content.
All 360 comparisons pass, up from 293 before the change. The focused native
suite, 8,262 prior comparisons, and 14 CLI checks pass. Reference observations
and native checks are retained in `docs/bose-seeded-reference.json.gz` and
`docs/bose-seeded-native-checks.json.gz`; `docs/bose-seeded-validation.json`
records the build and evidence hashes. These checks do not promote the BOSE
module or dispatcher to complete parity.

### BOSE explicit feature-alphabet checkpoint

The standalone feature extractor preserves the supplied alphabet's order and
multiplicity. A repeated target repeats its feature triple, and repeated source
entries contribute separately to the relation-type counts. Multiplicity weights
and cached unique-target triples avoid quadratic work for duplicate alphabets.
Event labels absent
from the supplied alphabet are rejected. Automatic alphabet discovery remains
sorted and unique, including the alphabet used by detection.

`docs/bose-alphabet-reference.json.gz` retains 3,159 public observations covering
reversed and duplicated labels, absent labels, empty alphabets/logs/traces, and
empty or whitespace labels. All 3,159 feature-value and acceptance comparisons
pass, up from 220 before the change. The focused native suite, 8,622 prior BOSE
comparisons, and 14 CLI checks also pass. Native results and verification hashes
are retained in `docs/bose-alphabet-native-checks.json.gz` and
`docs/bose-alphabet-validation.json`. This checkpoint does not promote the BOSE
module or dispatcher to complete parity.

### BOSE standalone relation and splitting helpers

`compute_concept_drift_follows_relation` exposes the eventual-follow relation as
a set of activity pairs. The supplied alphabet restricts targets, while sources
may be outside that alphabet. Repeated labels can produce a self-pair, but an
event never follows itself. Duplicate alphabet entries do not duplicate pairs.

`split_concept_drift_sublogs` exposes consecutive trace groups and the
`keep_leftover` control. Negative sizes return no groups, zero is rejected even
for an empty log, and positive sizes retain or drop the last partial group as
requested. Native log, trace, and event metadata is copied with each group.

`docs/bose-helpers-reference.json.gz` retains 1,166 public observations, including
empty and repeated labels, incomplete alphabets, empty traces, both leftover
settings, and signed 64-bit size extremes. All 1,166 comparisons pass, along with
11,781 prior comparisons, 14 CLI checks, the focused native suite, and independent
review. `docs/bose-helpers-native-checks.json.gz` retains the results and
`docs/bose-helpers-validation.json` records build and evidence hashes.
These helper contracts do not establish
standalone permutation-test or DataFrame adapter parity, and do not promote the
BOSE module or dispatcher to complete.

### BOSE nullable-missing scalar contract audit

A pinned public audit adds 68 unique-activity, feature, and follows calls
covering pandas.NA alone, repeated values, explicit/omitted/duplicate alphabets,
mixed scalar families, empty contexts, and tuple members. It records 32 accepted
calls and 36 errors: unique activities accept 5/20, features 13/24, and follows
14/24. Successful homogeneous and tuple contexts rule out blanket rejection;
other contexts expose ambiguous boolean comparisons or missing alphabet keys.
Inputs preserve PNA as a distinct tag and retain exact typed results/errors.

Six sentinel-free controls match the existing native helper probe. The other
62 observations remain reference-only because the native value model lacks
the distinct scalar. The required next work is that representation and its
behavior, including the eight raw and eight EventLog PNA observations already
captured. This audit makes no coverage promotion or native PNA parity claim.
Evidence and reproducible capture source are in
`docs/bose-missing-sentinel-contract-reference.json.gz`,
`docs/bose-missing-sentinel-contract-checks.json.gz`, and
`docs/bose-missing-sentinel-contract-validation.json`.

### BOSE mutable follows-alphabet checkpoint

The standalone typed follows helper now accepts mutable list, set, and
dictionary entries in its alphabet while retaining strict hashability of
trace activities. A mutable set can match a frozen-set target, including
inside tuples. Ordered comparisons preserve tuple-prefix and alphabet
short-circuit behavior and NA ambiguity rules. Returned pairs retain their
original typed trace values. The unique-activity and global-feature helpers
retain their stricter alphabet/activity validation.

The new pinned public corpus improves from 39 to
110 exact matches across 110 calls, with no skipped cases.
Before and after runs use the same frozen reference bytes and wire inputs.
Prior BOSE results are retained: 20,886 exact matches across
20,899 comparisons, with the same 13 documented ordering differences.
Two native suites and 14 CLI checks pass; independent review is clear.

Evidence is in `docs/bose-follows-alphabet-reference.json.gz`,
`docs/bose-follows-alphabet-native-checks.json.gz`, and
`docs/bose-follows-alphabet-validation.json`. This resolves the mutable-alphabet
gap recorded in the preceding immutable-set checkpoint. Both BOSE coverage
rows remain partial pending a fresh completion audit.

### BOSE immutable-set activity checkpoint

BOSE typed activities now accept immutable frozen sets, including nested
sets and tuple members. Activity identity ignores member order and collapses
equal members while preserving numeric aliases, calendar-date distinctions,
NaN identity, and the separate NA singleton. Returned raw sublogs retain the
original native values and child order. Mutable trace activity containers
continue to reject.

Strict subset is a partial order, so it cannot safely serve as a C++ sorting
comparator. The native extension compares unique member counts, then canonical
member identities. Every strict subset precedes its superset. Equal frozen-set
tuple prefixes advance to later fields; incomparable prefixes use the native
extension. Cached keys avoid rebuilding and sorting member identities for
each outer comparison.

The 84-helper corpus improves from eight matching rejection cases to 72 exact
matches and 12 visible unique-list ordering differences. For example, the
pinned reference returns incomparable singleton sets in A,C,B order while the
native result is A,B,C. All 12 retain the same typed members and preserve strict
comparisons. This documented alternative ordering follows the existing semantic
parity criterion and native set-order boundary; it is not counted as an exact
match. All feature and follows comparisons match exactly.

All 80 raw-trace and 16 EventLog comparisons match, including a 64-case raw
cohort with nonuniform traces, several incomparable labels, multiple windows,
two thresholds, and two seeds. These check change points and RNG continuation
as well as acceptance and preserved raw values. The prior 20,609 comparisons
retain their results. Combined validation has 20,776 exact matches from 20,789
comparisons, 13 documented ordering differences, two native suites, and 14 CLI
checks. Independent review is clear.

Evidence is in `docs/bose-frozenset-reference.json.gz`,
`docs/bose-frozenset-native-checks.json.gz`, and
`docs/bose-frozenset-validation.json`. At that checkpoint both BOSE rows
remained partial because the native standalone follows helper still rejected
mutable alphabet entries. The later mutable follows-alphabet checkpoint resolves
that gap; the semantic completion audit above records the current status.

### Native nullable-missing scalar and BOSE checkpoint

The native attribute model now represents pandas.NA as a distinct present
`NativeNA` scalar (`AttributeKind::kNA`). It remains separate from null/None,
floating NaN, and literal strings. Existing variant and kind indices remain
stable. Lossless JSON uses `kind: "na"`, `value: "<NA>"`, and
`typed_value: "NA"`; generic adapters preserve the marker and XES export
rejects it because XES has no equivalent scalar.

BOSE activity handling preserves repeated NA identity, tuple comparisons,
and the reference's ambiguous membership failures. Selected table case and
timestamp grouping handles missing markers in context, preserving standard
case validation and the timing of errors relative to RNG consumption.

The checkpoint replays 68 nullable-missing helper contracts, an 864-call
ordered membership matrix, and 44 table cases. It also brings 19 formerly
reference-only NA cases into the existing native replay. Combined validation
has 20,608 exact matches out of 20,609 comparisons, plus 14 CLI checks and
14 native suites. The one documented allocation-dependent NaN tuple order
difference remains visible and is not counted as an exact match.

Evidence and review are retained in `docs/bose-na-reference.json.gz`,
`docs/bose-na-native-checks.json.gz`, and `docs/bose-na-validation.json`.
This is a verified finite checkpoint. The BOSE module and dispatcher rows
remain partial pending a fresh completion audit; no coverage promotion is
made by this change.

### BOSE change-timestamp microsecond rounding checkpoint

The public table and EventLog adapters round returned change timestamps to six
decimal places after converting the exact instant to a floating-point epoch.
The native shared adapter previously retained nanoseconds. It now uses decimal
formatting and parsing to reproduce that rounding, including negative zero.
Integer microsecond rounding is insufficient: captured 500 ns returns zero,
while 2,500 ns returns 3 microseconds because the binary epoch value matters.
Case/activity equality and ordering retain exact timestamp precision.

The new corpus contains 32 table cases covering nanosecond second/day
boundaries, timezones, a custom selected timestamp column, and first-row NaT,
plus 34 precision controls for each of EventTable and EventLog. All 100 now
match, up from 24 before the fix. The controls check positive and negative
submicrosecond/halfway instants, modern epochs, timestamp zero signs, returned
frames, probabilities, and RNG continuation. All 19,514 prior comparisons
retain their results: 19,513 exact matches and the documented variable NaN
tuple ordering difference. The combined result is 19,613 exact matches out of
19,614 comparisons, with two native suites and 14 CLI checks passing.

The registered `minera_bose_parity_contract_tests` target adds named
`bose_module_contract` and `bose_dispatch_contract` cases that directly
exercise the public native entry points with captured expected outputs.
Independent review is clear for the timestamp fix. Captures, before/after
reports, signed-zero comparisons, and review are in
`docs/bose-timestamp-resolution-reference.json.gz`,
`docs/bose-timestamp-resolution-native-checks.json.gz`, and
`docs/bose-timestamp-resolution-validation.json`.

The completion audit identified a remaining semantic gap: the reference
accepts pandas.NA as a unique activity label, while native null represents
None and cannot reproduce the sentinel's distinct comparison behavior.
Those observations remain reference-only. Neither BOSE coverage row is
promoted; a distinct representation and behavioral verification are required.

### BOSE typed activity helpers and calendar-date checkpoint

Three native helpers now expose typed unique activities, relation-count
features, and eventual-follow pairs. Unique extraction retains original typed
representatives and sorts compatible labels. Explicit feature alphabets retain
order and duplicate entries, allow extras, and reject omitted logged labels.
Follows filters targets only and retains the first generated representative of
each semantic pair, including numeric/boolean aliases. Recursive tuple, NaN
identity, and resource checks apply to these helpers.

BOSE activity identity now distinguishes a canonical `YYYY-MM-DD` calendar
date from a datetime containing a time, even at midnight. Explicit alphabets
keep both labels; sorted extraction rejects their mixture before consuming
RNG state. Case grouping and timestamp interpretation retain their existing
behavior. All 24 captured raw/EventLog/table counterexamples now match the
pinned public runtime; all 24 failed before this fix.

Of 100 public helper observations, 97 have native representations and 96 match
exactly. Three pandas.NA observations remain reference-only. The one exact
mismatch is unique-activity list order for tuples containing distinct NaNs:
all typed labels and their identities are preserved, but the native comparator
orders distinct NaNs by scalar identity. Repeated public calls demonstrate
that the reference order varies with object allocation. This is a documented
ordering difference, not an exact passing comparison; the evidence retains
the original expected and actual lists. General NaN ordering parity remains
unproven.

All 19,393 prior comparisons, 14 CLI checks, and the native suite pass.
This checkpoint therefore records 19,513 exact matches out of 19,514 native
comparisons, with the single documented ordering difference. Independent
review found no other defect. Captures, replay sources, before/after results,
and review are in `docs/bose-typed-helpers-reference.json.gz`,
`docs/bose-typed-helpers-native-checks.json.gz`, and
`docs/bose-typed-helpers-validation.json`. This finite checkpoint makes no
coverage status promotion or full BOSE parity claim.

### BOSE typed raw-trace detector checkpoint

The public raw BOSE detector accepts non-string labels and returns their
original values in raw sublogs. The native `detect_concept_drift_typed_traces`
entry point now accepts ordered traces of `EventAttribute` values and returns
typed sublogs with ranked change points. It shares activity equality and
ordering with the table adapter, runs the existing detector on ordered surrogate
labels, and restores original values, metadata, trace order, and NaN scalar
identities in its results. Numeric/boolean aliases, nulls, bytes, dates, nested
tuples, and shared/distinct NaNs follow that common activity path. The raw API
has no timestamp requirement or output-frame string-join step.

The pinned public corpus contains 108 raw calls; all 100 representable calls
match sublog membership and preserved values, ranked points, signed zeros, and
RNG continuation. The recursive EventLog probe also verifies 112 representable
calls, including the eight tuple/NaN cases left unverified by the preceding
dispatcher checkpoint. Eight pandas.NA calls in each new corpus remain
reference-only because the native value model has no distinct pandas.NA
sentinel; they are not counted as passing native comparisons.

All 19,181 prior comparisons, 14 CLI checks, and the native suite pass after the
shared encoding refactor, for 19,393 native comparisons in this checkpoint.
Independent review is clear. Evidence and reproducible captures/probes are in
`docs/bose-typed-direct-reference.json.gz`,
`docs/bose-typed-direct-native-checks.json.gz`, and
`docs/bose-typed-direct-validation.json`. Native resource limits and explicit
seeding remain extensions. This finite checkpoint does not promote the BOSE
module or dispatcher coverage status.

### BOSE signed public dispatcher checkpoint

The native `apply_concept_drift` dispatcher now accepts EventLog and EventTable
inputs with signed options, default or explicit BOSE selection, and either
seeded or caller-owned RNG state. Each overload validates the variant before
calling the corresponding adapter. EventLog input order and DataFrame case
projection remain distinct, as observed through the pinned public dispatcher.

A 168-call reference audit compares default dispatch, explicit BOSE, direct
variant calls, and explicit `None` across logs, DataFrames, and event streams.
The three BOSE routes agree for each fixture and seed. Explicit `None` rejects
before RNG; tested nonempty event streams raise `KeyError(0)` before RNG, while
empty streams return empty results. The native overloads implement the two
advertised input types, EventLog and DataFrame; they do not convert event streams.

All 72 new advertised-input replays and 1,923 existing adapter cases pass through
the dispatcher. A separate typed-activity audit adds 12 passing scalar EventLog
comparisons, for 2,007 native comparisons in this checkpoint. The native suite
passes and independent review is clear. Eight additional public tuple/NaN cases
are captured but require a richer native probe before agreement can be claimed.

Evidence is retained in `docs/bose-dispatch-reference.json.gz`,
`docs/bose-dispatch-native-checks.json.gz`, and
`docs/bose-dispatch-validation.json`. The strict diagnostic CLI/server contract
is unchanged. Full BOSE module and dispatcher parity remain unproven; coverage
statuses are not promoted by this finite checkpoint.

### BOSE signed EventLog apply adapter checkpoint

`apply_concept_drift_log` exposes the signed EventLog apply contract and returns
regenerated DataFrames, ranked change timestamps, and probabilities. It keeps
input trace order, uses the selected first-event timestamp for each trace,
respects the activity/timestamp selectors, and ignores the grouped-log case-ID
selector. Its seeded and caller-owned MT19937 overloads share the same path.

The adapter reuses the verified table activity and output behavior through
ordinal case keys. Original case identifiers and log/trace/event metadata do
not survive the generated output frames. Missing selected activities and
invalid first timestamps reject before stochastic work; numeric output labels
retain the reference's later frame-construction failure. The existing strict
EventLog diagnostic API keeps its contract.

All 268 public comparisons pass, covering signed options, empty
inputs, varied trace shapes, selectors, invalid fields, output regeneration,
signed zero, and RNG continuation. All 18,841 prior BOSE comparisons, 14 CLI
checks, and the native suite pass; independent review is clear.
Evidence is in `docs/bose-log-apply-reference.json.gz`,
`docs/bose-log-apply-native-checks.json.gz`, and
`docs/bose-log-apply-validation.json`. This finite checkpoint does not promote
the BOSE module or dispatcher to complete.

### BOSE signed EventLog apply contract

A 110-call public EventLog audit captures signed option handling, empty inputs,
returned frame regeneration, and post-call NumPy state. The reference accepts
46 calls; the existing strict native EventLog detector accepts none of this
signed/threshold-1.1 corpus. This identifies a missing apply adapter rather than
establishing equivalence for the strict diagnostic API.

The public apply path regenerates canonical DataFrames with fresh case IDs,
RangeIndex labels, and synthetic timestamps, discarding input log, trace, and
event markers. Negative options retain signed slicing and RNG behavior. The
native diagnostic result instead preserves the source EventLog context.

`docs/bose-log-signed-contract-reference.json.gz` retains public observations,
capture/probe source, native observations, and inputs;
`docs/bose-log-signed-contract-validation.json` identifies the observed gap.
No coverage status is promoted.

### BOSE table activity-kind checkpoint

The table adapter validates activity ordering separately from case grouping.
Homogeneous null values and immutable tuples can reach the direct detector;
non-string output activities reject during frame construction after stochastic
work. Negative sub-log sizes that emit no frames preserve the reference's
accepted empty result. Incompatible mixed activity kinds still reject before
permutation sampling.

Tuple comparisons preserve NaN identity: shared NaN members allow comparison
to proceed to later elements, while distinct NaNs stop that prefix comparison.
The corpus includes both paths and incompatible tail controls.

All 184 captured public activity-kind calls pass, comparing
acceptance, returned frames, timestamps, probabilities, and post-call RNG state.
All 18,657 prior BOSE comparisons, 14 CLI checks, and the native suite pass;
independent review is clear. Evidence is in
`docs/bose-table-activity-reference.json.gz`,
`docs/bose-table-activity-native-checks.json.gz`, and
`docs/bose-table-activity-validation.json`. These finite cases do not establish
all pandas scalar identity or container behavior, and coverage statuses remain
unchanged.

### BOSE table case-ordering checkpoint

Selected-case timestamp lookup now sorts mixed numeric/string case IDs with
numbers first and follows declared categorical order while ignoring unused
categories. Standard activity projection retains its separate scalar ordering.
The table adapter compares date case keys at exact nanosecond precision, so
distinct cases within one microsecond remain separate in both grouping paths.
These changes are local to the BOSE adapter.

All 58 captured public ordering cases pass, including categorical
and mixed-key controls, exact returned frames, timestamps, probabilities, and
post-call RNG state. All 18,599 prior BOSE comparisons, 14 CLI checks, and the
native test suite pass. Independent review reports no actionable findings.

Evidence is in `docs/bose-table-order-reference.json.gz`,
`docs/bose-table-order-native-checks.json.gz`, and
`docs/bose-table-order-validation.json`. This remains a finite checkpoint;
coverage statuses are unchanged.

### BOSE DataFrame adapter checkpoint

`apply_concept_drift_table` accepts native `EventTable` input and returns
regenerated frames, ranked change timestamps, and probabilities. It preserves
the pinned adapter's separate grouping paths: selected case/time columns drive
group-first timestamp lookup, while standard case IDs drive contiguous activity
slices beginning at each sorted case's first occurrence. This deliberately
reproduces the observed behavior on interleaved rows. The shared direct detector
retains signed option behavior and caller-owned MT19937 continuation.

Returned frames contain the three canonical columns with `str`, `str`, and
`datetime64[us, UTC]` dtypes, fresh case IDs and RangeIndex labels, and synthetic
timestamps starting at epoch second 10,000,000 for each frame. Input extras and
metadata are discarded. Numeric activity labels fail during frame construction
after permutation sampling; selected timestamp index failures also preserve
the observed RNG advancement. Singleton null cases and numeric NaN case groups
are covered separately from the general scalar ordering path.

All 1,655 public comparisons pass, including 1,367 accepted calls, row-order,
selector, signed-boundary, dtype/null, numeric-key, and 120 timestamp-precision
cases. Comparisons include frame columns/dtypes/content/index, empty metadata,
ranked timestamps/probabilities, signed zero, and post-call RNG state. All 16,944
prior BOSE comparisons, 14 CLI checks, and the native test suite pass. Independent
review reports no actionable findings.

Evidence is in `docs/bose-table-reference.json.gz`,
`docs/bose-table-native-checks.json.gz`, and `docs/bose-table-validation.json`.
Resource bounds and explicit seeding remain native extensions. This finite
checkpoint does not establish every pandas scalar/container, categorical, null
identity, or timestamp-resolution behavior. No coverage status is promoted.

### BOSE direct activity-trace detector checkpoint

The native `detect_concept_drift_traces` API accepts string activity traces
without timestamps, including empty traces. It returns every raw trace group
and the ranked change points, matching the direct public helper rather than
the higher-level adapter's returned partitions. A separate options type and
signed change-point index preserve zero/negative window behavior, negative
permutation signed zero, Python-style negative cap slicing, and unrestricted
threshold comparisons. Seeded and caller-owned MT19937 overloads share the
existing feature and permutation implementation.

The pinned public corpus contains 3,240 cases, including 2,220 accepted calls.
Native comparisons verify raw groups, ranked indices, exact probabilities,
signed zero, and the next RNG word after successful and rejected calls. All
13,704 prior BOSE comparisons, 14 CLI checks, and the focused native test suite
also pass. Independent review reports no actionable findings.

Evidence is retained in `docs/bose-direct-reference.json.gz`,
`docs/bose-direct-native-checks.json.gz`, and
`docs/bose-direct-validation.json`. Resource bounds and explicit seeding remain
native extensions. This typed surface does not establish Python non-string
label or container-identity behavior, DataFrame adaptation, or complete BOSE
module parity. No coverage status is promoted.

### BOSE standalone permutation-test checkpoint

`concept_drift_permutation_test` exposes permutation probabilities for numeric
feature matrices. One overload accepts a seed; another advances a supplied
MT19937 engine so callers can continue the same random stream across tests.
Shape validation precedes sampling. Ragged matrices and mismatched widths are
rejected, zero permutations are rejected, and negative counts return negative
zero without drawing random values. Empty and zero-column matrices retain the
reference's distinct result and shuffle-state behavior.

The comparison uses Euclidean distances after square-root rounding. Single-column
means use the observed eight-lane, block-128 reduction. Squared distances use
16 fused accumulation lanes, merge offsets 2/4/8/1, and a separately rounded
scalar tail. This matches the observed NumPy 2.4.6 / SciPy OpenBLAS 0.3.31.188.0
Windows x86_64 runtime; different BLAS implementations, dtypes, and array layouts
are not established by this checkpoint. Native work limits bound positive
permutation requests, including zero-column shuffling.

`docs/bose-permutation-reference.json.gz` retains 757 public cases, including
four floating-point distance ties and a cancellation-heavy mean that failed
the initial scalar implementation. All 757 result and RNG-next-word checks
pass, together with 12,947 prior comparisons, 14 CLI checks, and the focused
native suite. Independent reduction models match 882 dot products and 204
single-column means. Results and numeric probe sources are retained in
`docs/bose-permutation-native-checks.json.gz`; build and evidence hashes are in
`docs/bose-permutation-validation.json`. These contracts do not complete the
DataFrame adapter or establish full BOSE module/dispatcher parity.

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
now use the table route described below. CLI merge output supports `-o` with
checked flushing.

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
surface, while mixed/XES adapters now use the table surface described below.
These finite observations do not establish complete dataframe or family parity. Non-string
index names, specialized index-level metadata and range parameters not recoverable
from empty/singleton labels remain outside the current table metadata model.

## Mixed-format case-relation adapters

CLI and HTTP case-relation merging now return typed tables for XES/CSV,
CSV/XES, and XES/XES inputs, alongside the compatibility log projection.
Relation metadata and duplicate relation rows flow through the table merger.
The CSV-only path retains its existing behavior.

When merge selectors are omitted, the CSV side supplies its resolved case and
timestamp headers; XES values are projected onto those aliases. Selected CSV
date cells are parsed to native dates in mixed merges so sorting remains
chronological. XES tables carry basic scalar dtype declarations, allowing
missing numeric values to promote during table merging. CSV relation keys
remain strings, and incompatible numeric XES case keys are rejected.

Explicit case selectors use the corresponding XES trace attribute. An alias
collision preserves the original event field under an `event:` prefix (repeated
until unique); the requested alias carries the selected trace or timestamp
value. Explicit timestamp selectors remain exact. The public table retains
source case attributes; a compatibility log may retain the original trace name
when a different trace attribute supplies the grouping key.

The expanded transport suite checks 25 scenarios, including both mixed
orientations, typed date/numeric values, missing cells, duplicate relations,
extra relation columns, missing selectors, and case/activity/timestamp name
collisions, mixed numeric promotion, and empty CSV inputs with configured
headers. All 154 native suites and 110 script tests pass; review is clear.
Validation is recorded in
`docs/case-relations-mixed-validation.json`. These adapter checks do not prove
exhaustive XES dtype inference, arbitrary CSV type inference, or complete
case-relation parity.

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


## Direct dataframe replay group-key inference evidence

The numeric-key discrepancy recorded after the exact stream grouping checkpoint
is resolved for the captured cases. Public direct-dataframe replay first groups
exact case values, then infers the group index from each case's first scalar
representative. In the observed all-numeric sets, a float representative promotes
the index to floating point unless a boolean representative is present. When
inferred keys collide, the later group replaces the earlier group at its output
position; events are not merged. String representatives also prevent promotion.
Exact stream-to-log conversion retains its separate grouping behavior.

A flower net with distinct activity labels makes the surviving event sequences
observable without inferring case labels from replay results. The new capture
contains 28 public cases, with 25 successful cases checked through both eager
and lazy native tables. All 50 comparisons verify activated transition paths,
fit/fitness values, consumed/produced/missing/remaining tokens, and unchanged
inputs. Reversed source orders, first-representative type changes, signed and
unsigned large integers, infinities, and boolean/string suppression are covered.

Three public high-level empty-case rejections remain named exclusions: the
native lower-level normalization API does not implement that validation layer.
Case-label representation is not established by the public list diagnostics,
and broader dtype/index and replay parity remain unproven. The durable capture
and validation are `docs/token-replay-dataframe-grouping-reference.json.gz` and
`docs/token-replay-dataframe-grouping-validation.json`. The isolated Release
build, all 152 native suites, 92 script tests, four optimized evidence gates,
and the existing formatter CLI/HTTP matrix pass. Structural coverage counts
are unchanged.


## Dataframe column validation evidence

`check_dataframe_columns` now validates the dataframe schema and optional case,
activity, timestamp, and start-timestamp selectors. Declared dtypes determine
column eligibility; global schema checks precede selected-column type and
missing-value checks. Empty strings remain valid values. Checks preserve the
input, including metadata, and reproduce the captured public error messages.

`replay_dataframe_traces` applies that validation before dataframe replay.
All 28 captured replay cases now have a validated native comparison, including
the three empty-case rejections excluded from the lower-level API checkpoint.
Existing lower-level conversion and replay APIs retain their separate behavior.
The validator corpus adds 64 cases: 18 successes and 46 expected rejections.
All 153 native suites, 96 script tests, four optimized evidence gates, and
258 CLI plus 258 HTTP formatter comparisons pass. Independent review found
no actionable issues. The durable evidence is `docs/dataframe-column-validation-reference.json.gz`
and `docs/dataframe-column-validation-evidence.json`.

These finite checks do not establish Python exception-class identity, arbitrary
extension dtypes, duplicate/non-string column labels, or exhaustive pandas
inference. Native tables without declared dtypes use basic scalar-kind inference.
Full parity and structural coverage counts remain unchanged.


## Replay diagnostics dataframe output

`replay_dataframe_diagnostics` validates the required columns and replays exact
case groups in first-occurrence order. This reproduces the observed dataframe
return path, which differs from the list path's inferred group index. Output
case dtype inference can round distinct identifiers without merging their rows.
The returned table contains `case_id`, `is_fit`, `trace_fitness`, `missing`,
`remaining`, `produced`, and `consumed`, with a fresh RangeIndex and empty attrs.
Valid typed empty input returns a zero-column table.

The public corpus contains 61 cases: 56 successes and five expected rejections.
It covers numeric boundaries, first-representative dtype inference, source order,
empty inputs, missing case IDs, non-fitting linear traces, unknown activities,
and custom case/activity/timestamp selectors. Generated native comparisons check
the full typed result, index, attrs, and unchanged input. The capture and
validation are `docs/token-replay-diagnostics-dataframe-reference.json.gz` and
`docs/token-replay-diagnostics-dataframe-evidence.json`.

The isolated Release build, all 154 native suites, 104 script tests, eight
optimized evidence checks, and 516 formatter interface comparisons pass.
Independent review found no code issues.

This finite matrix does not establish arbitrary extension or date/container
case dtypes, Python exception-class identity, all replay parameters, or full
parity. Structural coverage counts remain unchanged.


## Date-valued replay case identifiers

The expanded public dataframe diagnostics capture contains 55 replay cases and
13 direct conversion comparisons. Date observations distinguish naive values
from timezone-aware values, group equal aware instants across written offsets,
and keep distinct nanosecond instants separate. First case representatives
still determine source order and output dtype. Uniform naive, UTC, and fixed
UTC-offset dates infer datetime columns; mixed offsets or naive/aware values
retain object dtype.

The capture records timestamp precision separately from its lexical value.
Nanosecond precision at a whole second and mixed microsecond/nanosecond columns
are therefore observable rather than inferred from nonzero fractional digits.
Generated checks compare precision metadata as well as complete tables,
conversion group membership, and unchanged inputs.

Native grouping now uses an exact seconds/nanoseconds key with a separate
awareness domain. The original typed date supplies the case label even when
its outer lexical projection is stale. All 154 native suites pass, including
the 55 replay and 13 conversion comparisons, as do 103 script tests, seven
optimized evidence checks, and 516 formatter interface comparisons. Independent
review is clear.

Named timezone identity is covered by the subsequent checkpoint below. Arbitrary
date/container or extension types remain unproven. Full parity remains incomplete.


## Named timezone replay metadata

The public diagnostics capture now includes named timezone identity alongside
resolved ISO offsets and date precision. Berlin dates on either side of the
summer clock change retain `Europe/Berlin` as their datetime dtype. Different
zone names with the same offset retain object dtype. Equal instants still form
one case and use its first timezone representative; the repeated autumn hour
contains two distinct instants.

The expanded corpus contains 61 replay cases and 19 direct conversion checks.
All 154 native suites, 104 script tests, eight optimized evidence checks, and
258 CLI plus 258 HTTP comparisons pass. Fourteen transport boundary checks
include nanosecond and timezone metadata preservation in rejected requests.
Independent review is clear.
Native fixture comparisons include date timezone metadata, precision, complete
tables, and unchanged inputs. The implementation retains resolved offsets and
zone identity; it does not infer named zones from offsets or perform timezone
database lookups. Arbitrary timezone-object classes and complete parity remain
unproven.


## Typed-date formatter resolution

The public formatter retains nanosecond resolution for whole-second typed
Timestamp values in object columns. Resolution is observable even when all
fractional digits are zero. The native formatter now uses the supplied date
resolution when inferring the output datetime unit, including mixed-resolution
columns and custom start timestamps.

The separate 21-case capture covers naive, UTC, and Berlin timestamps; object
and declared datetime columns; microseconds and nanoseconds; mixed precision;
custom start columns; and explicit format strings. The checker compares complete
results and post-call inputs, then explicitly checks date resolution and
timezone metadata. Normalized output dates carry UTC timezone identity,
matching their dtype and resolved lexical offset.
All 21 new CLI comparisons, 154 native suites, 107 script tests, three optimized
evidence tests, and 258 CLI plus 258 HTTP formatter regressions pass. Independent
review is clear. All 21 cases also verify output UTC timezone metadata.
The evidence is `docs/formatter-date-resolution-reference.json.gz` and
`docs/formatter-date-resolution-validation.json`. These observations do not
establish arbitrary date-object, timezone, or complete formatter parity.


## Container-valued conversion case identifiers

Public stream conversion compares tuple case identifiers recursively and treats
frozensets as unordered sets. Numeric equality remains exact across integer,
float, and boolean children. Empty tuples, empty frozensets, strings, and nulls
remain distinct domains. Lists, dictionaries, and mutable sets are unhashable,
including when nested in tuple keys. Explicit null cells remain null regardless
of container tags, including inside tuple keys.

The pinned capture contains 15 cases, including five expected errors. Generated
native checks compare group membership, source order, first typed representatives,
error messages, and unchanged inputs. The artifacts are
`docs/container-case-grouping-reference.json.gz` and
`docs/container-case-grouping-validation.json`. All 154 native suites, 110 script
tests, three optimized evidence tests, and 516 CLI/HTTP comparisons pass. Review
found a null-container regression, which was corrected and verified.
Python exception-class identity,
arbitrary Python objects, and exhaustive container/replay behavior remain outside
this finite evidence. Full parity remains incomplete.


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

The backwards undefined-fitness correction is recorded in
`docs/token-replay-parity-contract.md`. Native fitness APIs now raise a domain
error for a zero consumed or produced denominator; the public reference raises
`ZeroDivisionError` on the captured controls. Count-only activation sequences
remain available for stochastic-map fitting through the same replay path.
The bounded reference and native evidence does not complete either replay
variation or promote a catalogue row.

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

Start timestamp selection distinguishes an omitted selector from an explicit
attribute. Classic performance discovery defaults to completion timestamps,
even if events carry `start_timestamp`; an explicitly selected start attribute
is required on each destination event. PLUSPLUS retains its conventional
`start_timestamp` default and lifecycle conversion when that field is absent.
The UI preserves explicit conventional selectors and displays each variant's
automatic default. `docs/heuristic-start-selector-reference.json.gz` preserves
21 public EventLog observations covering both variants, conventional and custom
selectors, completion timestamps, missing attributes, overlapping intervals,
and custom timestamp fields present only where the calculation needs them.
This finite checkpoint does not establish arbitrary dataframe timestamp parity
or Python exception-class and message identity for native rejections.
The first selector fix improved the strict CLI comparison from 16/21 to 20/21;
that historical result remains in `docs/heuristic-start-selector-validation.json`.
The subsequent decoration fix treats missing custom timestamps as untimed,
preserving firing counts while omitting waits without usable endpoints. It
does not substitute canonical timestamps for the selected custom field, and
missing activity fields still raise errors. All 21 selector cases now match,
along with seven additional one- to four-event sparse timestamp cases. The
reference observations and passing 154 native suites are recorded in
`docs/heuristic-decoration-sparse-validation.json`. This validates native
decoration's documented untimed-event behavior; it does not establish parity
with every reference visualization variant.

The C++ `HeuristicMinerOptions::start_timestamp_attribute` field is now
`std::optional<std::string>` to distinguish omission from explicit selection.
String assignments remain supported; callers that read it as a string must
handle the omitted value. The existing explicit-empty native fallback remains
an extension, while the UI treats a blank field as automatic selection.

Classic and PLUSPLUS expose business-hours performance waits through the native
options, CLI, request parser, and UI. PLUSPLUS also applies the schedule to
activity sojourn times, including frequency decoration. Omitted schedules use
Monday-Friday 07:00-17:00; explicit empty schedules contribute zero time. Custom
slots use seconds from Monday and retain the DFG helper's one-second gap merge.
Business durations use local clock fields, while interval ordering and
concurrency retain absolute instants. Supplied DFG performance samples are not
reinterpreted because they contain no timestamps.

The business-hours reference and validation artifacts retain 126 matching
public-API comparisons, plus 21 start-selector and seven sparse-decoration
regressions. All 154 native suites and 624 frontend tests pass, and the frontend
production build passes. Offset-bearing constructed datetimes use ISO-string
XES transport to preserve their clock fields; typed-date import normalization
is a separate contract. These finite checks do not establish full parity.

`docs/heuristic-workcalendar-reference.json` records eight further EventLog
observations: omitted, closed-calendar, opaque-object, and `None` workcalendars
produce identical waits and sojourns in both variants. The supplied calendar's
methods are never called. Native output without a calendar option matches all
eight observations; this does not provide an opaque Python-object API or prove
the behavior of dataframe inputs or arbitrary calendar types.

The PLUSPLUS variation independently implements the pinned interval abstraction.
Lifecycle events are converted through case-insensitive start/complete matching
with FIFO queues keyed by activity and lifecycle instance; unmatched completes
become zero-duration events and unmatched starts are omitted. A log whose first
event already carries the configured start timestamp is treated as interval
input globally. Eventual-follow and concurrency calculations use stable
start-time ordering, while pre-interval performance edges preserve the input
trace order.

Lifecycle conversion distinguishes an omitted transition (a completion) from
an explicit empty transition (ignored). It requires selected activity and
timestamp fields even on ignored events, but parses timestamps only when they
contribute to an interval: unused start timestamps and timestamps on ignored
lifecycle transitions need not be valid dates. A malformed start timestamp
still fails when a matching completion consumes it. Pre-interval input retains
its required start and completion timestamp validation.

The lifecycle reference and validation artifacts retain 72 cases spanning
canonical/custom completion fields and ordinary/business time. Matches improved
from 56 to 72; successful outputs compare frequencies, waits, and sojourns,
while error cases check rejection only. All 154 native suites and 154 earlier
business-hours, start-selector, and sparse-decoration comparisons pass.

Heuristics JSON preserves round-trip double precision for dependencies,
representations, wait statistics, and sojourns. The writer restores its caller's
stream precision on both normal completion and an output exception. This avoids
six-significant-digit rounding such as `0.666667` for a dependency of two thirds.
`docs/heuristic-graph-reference.json.gz` and the precision validation artifacts
retain 480 graph comparisons and 16 performance comparisons: exact matches
improved from 379 to 496. All 154 native suites and 226 earlier timestamp,
business-hours, and lifecycle comparisons pass. The graph checks cover active
nodes and accepted edges, not full object equivalence or structural completion.

Heuristics interval sorting, overlap, and ordinary waits now retain integer
microseconds. Ordinary PLUSPLUS sojourns retain the reference's subtraction of
floating epoch endpoints, including its loss of microseconds at large years.
Epoch endpoints and long wait durations round the exact microsecond ratio once;
this also avoids cancellation immediately before 1970. Performance means use an
exact binary64 accumulator with one nearest-even rounding, while other
aggregation modes retain their existing arithmetic.

Business-time arithmetic keeps exact integer week boundaries and follows the
reference's ordered weekly-total and prefix subtraction. This preserves its
fractional rounding at working-slot and weekend boundaries. The existing DFG
double-based helper is unchanged. `docs/heuristic-subsecond-reference.json.gz`
and its native-checks and validation artifacts retain 672 comparisons spanning
fractional durations, custom schedules, offsets, negative epochs, and years
1–9999. Exact matches improved from 266 to 672; all 672 public observations also
replayed identically on the restored pinned reference runtime. All 154 native
suites and 722 earlier regression comparisons passed. These finite checks do
not establish full object parity or change structural coverage.

The aggregation follow-up retains 640 public Classic observations over 80
duration datasets and eight aggregation modes. Exact matches improved from
568 to 640 after replacing rounded variance intermediates with exact integer
first and second moments and one nearest-even square-root rounding. This
fixes 36 standard-deviation results and their 36 `all` results. Mean, median,
minimum, maximum, sum, and raw values continue to match this corpus.
The reference and validation artifacts are `docs/heuristic-aggregation-*.json*`.
All 1,037 public Python `statistics.stdev` bit goldens pass, including signed
cancellation, very large values, subnormals, and halfway rounding. The Release
build, all 154 native suites, 1,394 earlier regression comparisons, and independent
review passed. Native zero-for-fewer-than-two-samples, nonfinite input, and true
result-overflow behavior remain native conventions; exception equivalence is
not claimed. Raw/all values are verified through relation statistics, while
edge representations still use scalar means at that checkpoint. Structural
coverage remains partial.

The structured-value follow-up preserves `raw_values` arrays and `all` summary
objects in Classic and PLUSPLUS edge and relation JSON representations. The
existing C++ scalar field remains a compatibility view, with an optional owning
payload for structured values. Frequency decorations, scalar aggregations,
PLUSPLUS pairs without direct wait samples, and native `connect_all` fallback
edges retain scalar representations. Rescued Classic loop edges use each
direction's own samples.

`docs/heuristic-structured-reference.json.gz` and its native-checks and validation
artifacts retain 1,384 public comparisons across graph thresholds, all eight
aggregation modes, and reordered, overlapping, and touching intervals. Exact
matches improved from 1,028 to 1,384. The isolated Release build, all 154 native
suites, all 624 frontend tests, the frontend production build, 2,034 earlier
regression comparisons, and independent review passed. These finite EventLog
checks do not establish full object parity, cover the native `connect_all`
extension as a reference behavior, or change structural coverage.

Classic and PLUSPLUS now expose accepted numeric `and_measures_in` and
`and_measures_out` alongside their existing bundles. Each record retains the
center activity, name-ordered neighbor pair, and score. Classic records scores
at its inclusive threshold before length-two loop rescue; PLUSPLUS uses its
strict threshold. The public C++ fields are appended, and JSON uses the existing
activity-ID mapping and full double precision.

`docs/heuristic-and-reference.json.gz` and its native-checks and validation
artifacts retain 620 comparisons across five AND thresholds, graph filters,
loops, and interval ordering/overlap cases. All 620 existing graphs already
matched before this change; the missing score arrays are now present and match
exactly, including the 93 cases with nonempty measures. All 620 bundle outputs
remain unchanged. The isolated Release build, 154 native suites, 624 frontend
tests, frontend production build, 3,418 earlier reference comparisons, and
independent review passed. This verifies finite AND-score behavior, not full
HeuristicsNet object equivalence; structural coverage remains partial.

Heuristics nets now expose `dependency_measures` for retained DFG relations,
including negative and dependency-threshold-rejected values. Classic uses the
cleaned DFG before edge frequency/activity filters; PLUSPLUS records each
retained relation before applying the dependency threshold. Classic also exposes
directional `loop_length_two` counts for pre-rescue nodes. These are cleaned DFG
counts recorded after the loop-ratio check but before later edge filters, so a
count can be zero or refer to an inactive target. PLUSPLUS has no such loop data.

`docs/heuristic-matrix-reference.json.gz` and its native-checks and validation
artifacts retain 1,171 public EventLog comparisons, including 290 cases with
nonempty loop counts. Existing graphs already matched before the new arrays;
all new arrays now match exactly, with all existing bundles and AND measures
unchanged. The isolated Release build, all 154 native suites, 624 frontend tests,
frontend production build, 4,038 earlier reference comparisons, and independent
review passed. This is finite data preservation evidence; structural coverage
and full object equivalence remain incomplete.

Classic DFG inputs now preserve declared activity order and duplicates, raw
occurrence maps, and list-or-map boundary metadata independently of their unique
internal activity IDs. Graph-bearing names can receive IDs without becoming
declared fallback nodes. Positive noise cleaning rejects undeclared DFG
endpoints; disabled cleaning permits them. Missing occurrence entries skip edge
candidates, while fallback node creation validates declared counts strictly.
Inferred boundaries retain DFG traversal order, and length-two rescue requires
an actual forward DFG entry even when the minimum frequency is zero. Direct DFG
IDs are local, and converting an intentionally empty discovered net no longer
creates transitions from its internal label table.

The JSON `metadata` object retains declared activities, occurrence counts, and
boundary list/map shapes. Classic EventLog boundaries remain maps; PLUSPLUS
metadata contains retained activities and counts. The new
`docs/heuristic-dfg-input-reference.json.gz` fixture matches all 1,350 cases,
improving from 239 before this checkpoint. The earlier 52 observations in
`docs/heuristic-dfg-metadata-reference.json.gz` also match, as do all 124
EventLog metadata observations in
`docs/heuristic-eventlog-metadata-reference.json.gz`. Reference rejections are
compared as acceptance versus rejection, without claiming Python exception
class or message identity. These finite cases do not establish full object,
adapter, or structural coverage completion.

`docs/heuristic-dfg-input-native-checks.json.gz` and
`docs/heuristic-dfg-input-validation.json` retain the before/after comparisons,
probe sources, binary hashes, and verified source tree. The isolated Release
build, all 154 native suites, 624 frontend tests, frontend production build,
5,209 earlier reference comparisons, and independent review passed.

Custom activity-selector JSON exports now preserve the names of raw statistics
whose activities were filtered out by PLUSPLUS. The exported local label table
starts with the native activity IDs, then appends missing names referenced by
raw activity/concurrency statistics and name-based DFG/triple fields. Graph
nodes, edges, native activity tables, and declared metadata remain unchanged.
An all-filtered net retains an empty active-node list while its raw statistics
still have valid labels. Canonical document-wide activity IDs are unchanged.

`docs/heuristic-local-labels-reference.json.gz` retains 240 public comparisons
covering canonical/custom labels, partial/complete activity filtering, interval
overlap, both variants and decorations, empty logs, and escaped/Unicode labels.
All 240 now match, compared with 212 before the fix. All 240 graph payloads are
unchanged, and the 212 previously matching JSON payloads remain identical.
This is finite export preservation evidence, not full object or adapter parity.

`docs/heuristic-local-labels-native-checks.json.gz` and
`docs/heuristic-local-labels-validation.json` retain the comparisons, checker,
source tree and binary hashes. The isolated Release build, 154 native suites,
624 frontend tests, frontend production build, 6,559 earlier reference
comparisons, and independent review passed.

Direct Classic DFG inputs now accept an additional `performance_values` map of
already-aggregated values. Scalars, ordered arrays, strings, nulls, booleans,
and nested string-keyed objects retain their payloads rather than entering the
sample aggregator. The legacy `performance_dfg` sample field remains supported;
the new field takes precedence when both are supplied. A supplied value map
selects performance decoration regardless of the decoration option. With
neither map, direct DFG discovery uses frequency representation. Existing
EventLog aggregation continues to use the native sample path.

The new map is retained on the net and exported as `performance_dfg`, including
extra pairs that do not create nodes. An explicit root null stays null in that
map but selects the path frequency for edge/relation representation; nested
nulls remain part of their payload. Every cleaned DFG pair must have a value,
even if later dependency filters reject its edge. A rescued reverse loop absent
from the cleaned DFG gets scalar zero even when an extra reverse value was
supplied. Typed values are authoritative; opaque non-double values use zero in
the legacy scalar representation field. Wrapped root doubles retain their
numeric projection. Discovery preserves nonfinite input values, while JSON
export rejects them because NaN and infinity are not JSON numbers.

`docs/heuristic-dfg-values-reference.json.gz` captures 1,152 public cases,
including missing/empty maps, nested values, large signed/unsigned integers,
filtering, decoration/aggregation options, and directional loop rescue. The
baseline could not express 1,056 cases and exposed 48 no-map decoration
mismatches; 48 cases already matched. This transport supports JSON-compatible
values within native integer ranges, not arbitrary Python objects or non-string
mapping keys. Full object, adapter, and structural coverage remain incomplete.

All 1,152 cases now match. `docs/heuristic-dfg-values-native-checks.json.gz`
and `docs/heuristic-dfg-values-validation.json` retain the comparisons, probe,
checker, source and binary hashes, and four public nonfinite-value observations.
The isolated Release build, 154 native suites, 624 frontend tests, frontend
production build, 6,923 earlier reference comparisons, and independent review
passed. These finite checks do not establish full parity.

Classic length-two loop rescue now checks occurrence-map membership before
applying activity thresholds. Missing counts skip rescue, including at a zero
threshold; explicit zero counts remain eligible at that threshold. Loop
statistics are still recorded before these filters, and fallback nodes retain
their strict required-count validation.

`docs/heuristic-loop-membership-reference.json.gz` captures 384 public cases
across missing, zero, and positive counts for three activities, three activity
thresholds, and two DFG shapes. The baseline matched 370 cases and incorrectly
rejected the remaining 14. The corpus includes 181 expected rejections that
preserve fallback validation; rejection evidence concerns outcomes rather than
Python exception identity.

All 384 cases now match. `docs/heuristic-loop-membership-native-checks.json.gz`
and `docs/heuristic-loop-membership-validation.json` retain the before/after
comparisons, probe and checker, source and binary hashes, and build/test logs.
The isolated Release build, all 154 native suites, and 4,293 earlier DFG,
performance-value, dependency/loop, and AND reference comparisons passed.
Structural coverage remains partial and unchanged.

Classic DFG dependency, AND, and length-two-loop ratios now preserve integer
sums and differences until the final correctly rounded binary64 division.
This avoids erased dependencies between adjacent large counts and wrapping
uint64 sums. Large frequency representations retain exact uint64 payloads in
JSON for both edges and relation statistics; the legacy scalar field remains
a binary64 compatibility projection. Existing performance values and ordinary
small-count payload shapes are preserved.
`docs/heuristic-count-ratios-reference.json.gz` captures 1,828 public cases for
self-loops, bidirectional paths, splits, joins, and length-two loops, including
threshold equality and adjacent binary64 thresholds. The baseline matches 828
cases and emits invalid JSON in 12 cases. The fixture supplies occurrence
counts explicitly; inferred occurrence overflow is a separate remaining gap.

All 1,828 cases now match, and 10,185 independent Python integer-division
golden comparisons pass bit for bit, including rounding ties and ratios above
one. `docs/heuristic-count-ratio-arithmetic-reference.json.gz` records those
arithmetic expectations separately from the public behavioral corpus.

`docs/heuristic-count-ratios-native-checks.json.gz` and
`docs/heuristic-count-ratios-validation.json` retain the comparisons, checkers,
probe sources, source-tree and binary hashes, and review result. The isolated
Release build, all 154 native suites, 8,459 prior reference comparisons, and
independent review passed. These finite checks leave inferred-count overflow,
arbitrary-size inputs, and full object/adapter and structural parity incomplete.

The inferred-occurrence checkpoint addresses that remaining count overflow.
Classic DFG inference accumulates incoming and outgoing counts exactly and
matches the public conversion of a two-sided total through binary64 division
by two and integer truncation. A one-sided total remains an exact integer.
Direction presence, including zero-count entries, determines whether averaging
applies. Explicit occurrence maps retain their supplied values.

`HeuristicActivityStatistics::occurrences` and the mapped values in
`HeuristicNetMetadata::activities_occurrences` now use `HeuristicOccurrenceCount`.
The public type retains integer construction and comparison, provides exact
decimal output, and requires checked `to_uint64()` conversion when narrowing.
Code that names the former `map<string, uint64_t>` metadata type must adopt the
wider mapped type. Input DFG counts, explicit occurrence inputs, and boundary
counts keep their existing uint64 types. JSON emits exact decimal integers in
the existing fields; JavaScript consumers still use their native number model.

For a DFG with N unique pairs, each activity receives at most N+1 contributions
because only its one possible self-loop is counted twice. The unsigned 128-bit
accumulator therefore holds every total obtainable from uint64 DFG entries on
the supported 64-bit platform. The rounded two-sided average also fits.
`docs/heuristic-inferred-counts-reference.json.gz` captures 2,690 public cases;
the baseline matches 1,146 and differs in 1,544. The cases cover zero counts,
rounding boundaries, large seeded counts, fanout, cycles, self-loops, and
activity thresholds. `docs/heuristic-occurrence-arithmetic-reference.json.gz`
separately records 10,061 arithmetic goldens for exact decimal conversion,
rounded averaging, checked addition, and checked narrowing.

All 2,690 public cases and their activity-statistic projections now match;
all 10,061 arithmetic goldens pass. The isolated Release build, 154 native
suites, 10,287 prior reference comparisons, and independent review passed.
`docs/heuristic-inferred-counts-native-checks.json.gz` and
`docs/heuristic-inferred-counts-validation.json` preserve the checks, probe
sources, source-tree and binary hashes, and review evidence. Structural
coverage remains partial; arbitrary-size raw inputs and full object/adapter
parity remain unproven.

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

The current edit-distance acceptance matrix is maintained in
`docs/edit-distance-parity-contract.md`. The pinned public surface is archived
in `docs/edit-distance-public-surface.json.gz`; both catalogue rows are complete.

### Edit-distance carrier acceptance and completion

The registered `edit_distance_native_carrier_acceptance` verifies string
alignment on log, table, stream and lazy carriers, typed alignment on mixed
stream/lazy and table/stream inputs, and fresh projection adapters. It checks
exact eager outputs, custom selectors, anti mode, projection markers and
timestamps, unchanged normalized table views, and counted lazy scans.
Stream/lazy empty-target and missing case/activity/timestamp errors
are checked directly. Flat rows cannot retain empty traces; projection omission
is tested separately.

The focused Release suite passes with assertions enabled. The final source,
executable, unchanged core library and logs are bound in
`docs/edit-distance-carrier-native-checks.json.gz`, with a summary in
`docs/edit-distance-carrier-validation.json`. Independent review closed the
adapter finding and found no remaining semantic issue in the two scoped rows.
The earlier 2,470 differential comparisons and CLI/HTTP fixtures apply to the
unchanged production code. Distinct registered catalogue proofs and the
requirement reconciliation in `docs/edit-distance-parity-contract.md` complete
the dispatcher and variant module rows. Project-wide parity remains incomplete.

The checkpoint sections below record historical states. Their open items were
resolved by subsequent checkpoints and the completion audit above.

### Edit-distance proof registration and completion audit

The dispatcher and variant module now have distinct catalogue proof objects,
binding `edit_distance_dispatcher_acceptance` and
`edit_distance_variant_module_acceptance` to the registered native suite.
Structural validation passes with both rows still partial. The independent
semantic audit reconciled the six public functions and existing cohorts, but
identified missing direct tests of the advertised event-stream and lazy-table
alignment/projector adapters. Those carrier routes must be verified against
the established eager fixtures before completion can be claimed.

### Edit-distance flat-selector native checkpoint

Native tables now accept a unique literal empty column name. Flat conversion
uses the exact selected case key, preserves an empty case source key through
roundtrips, and retains generic empty-key fields when no first-class key was
selected. Already grouped EventLogs ignore the unused case selector.

All 22 captured flat-data calls match, up from seven matches in the frozen
baseline. The comparison checks actual native index, attribute and option state
before and after each call; all inputs remain unchanged. The changed library
also passes 2,448 earlier comparable calls, all five affected native suites,
and the two existing CLI/HTTP parameter regression cases. The latter reuse the
established fixture and do not claim product support for every blank selector.

The capture, implementation and comparison harness passed independent review.
`docs/edit-distance-flat-selectors-native-checks.json.gz` preserves the evidence;
`docs/edit-distance-flat-selectors-validation.json` summarizes its bindings and
results. These finite cases do not establish universal DataFrame metadata
equivalence. At that checkpoint, both coverage rows remained partial pending distinct catalogue proof
bundles and an independent requirement-by-requirement completion audit.

### Edit-distance flat-selector reference checkpoint

The public DataFrame delta capture in
`docs/edit-distance-flat-selectors-reference.json.gz` records 22 calls:
12 successes and 10 errors. Matching literal empty and one-space column names
are accepted for activity selection, case grouping and projection timestamps.
Missing selected columns raise `KeyError` for the exact key, including when
canonical decoy columns exist. Empty target DataFrames still reject through
alignment; zero-row fresh projection returns an empty log with either declared
columns or no columns.

Full DataFrame schema, dtypes, index, attrs and cells are captured before and
after each call, together with independent parameter and projection-variant
snapshots. All checks report unchanged inputs. The corrected capture and its
public-call provenance passed independent review; its summary is in
`docs/edit-distance-flat-selectors-reference-validation.json`. This checkpoint
establishes reference behavior only; the subsequent native checkpoint above
records its implementation and comparison. Neither edit-distance row was promoted
at that reference-only checkpoint.

### Edit-distance explicit-dispatch and named-acceptance checkpoint

Nine paired public EventLog fixtures compare omitted dispatcher selection with
explicit `Variants.EDIT_DISTANCE`. All 18 calls agree within their pairs,
including empty-source behavior, two empty-target errors per route, anti mode,
literal empty activity keys, and effective string-keyed custom controls.
Full input snapshots remain unchanged. An enum-keyed custom-parameter fixture
retains the reference's default-field behavior; it is not evidence that those
enum keys select the custom fields.

The native alignment executable now calls separate named dispatcher and module
acceptance functions directly from `main`. They exercise default and explicit
`apply` calls, selected-field anti behavior, exact alignment metrics, direct
helper aliases and cache identity, and fresh projection markers. The suite
passes against the unchanged verified library. These native acceptance tests
and the public paired calls are complementary evidence, not an 18-call native
differential comparison.

The capture is in `docs/edit-distance-explicit-dispatch-reference.json.gz`;
source, build/test evidence and independent reviews are in
`docs/edit-distance-dispatch-proof-checks.json.gz`, with a summary in
`docs/edit-distance-dispatch-proof-validation.json`. Both coverage rows remained
partial at that checkpoint, pending flat-data selector reconciliation, distinct catalogue proof
bundles and independent completion review.

### Edit-distance literal-selector checkpoint

Grouped-log alignment, direct trace helpers, and fresh variant projection now
accept literal empty and whitespace activity/timestamp keys when present.
Unused timestamp selectors no longer reject whole-log alignment. Exact generic
empty-key attributes take precedence over stored first-class slots, including
null values; absent source-key sentinels do not alias a literal empty key.
Missing selected fields still fail when read, and empty-target validation keeps
its existing order.

The frozen public capture contains 46 calls. Of these, 45 express a single
effective native selector: the prior library matched 22, and the changed
library matches all 45. Seven errors use narrowly checked semantic mappings
with raw diagnostics retained; Python exception-class identity is not claimed.
One projector enum-key case uses different outer and inner activity selectors
in Python and cannot be represented by one native activity option. It remains
an explicit reference-only observation, including its differing native result.

`docs/edit-distance-selector-domain-reference.json.gz` retains the capture;
`docs/edit-distance-selector-domain-native-checks.json.gz` retains baseline and
changed-library evidence, source/binary bindings, reviews, and raw outputs.
`docs/edit-distance-selector-domain-validation.json` summarizes the checkpoint.
All 2,403 earlier comparisons, the native alignment suite, and both rebuilt
CLI/HTTP parameter cases pass against the changed library. Catalogue validation
passes without promoting either edit-distance row. Flat-data selector rules
and requirement-by-requirement completion proofs still required reconciliation
at that checkpoint.

### Edit-distance product parameter checkpoint

One CLI and one local HTTP case combine anti-alignment, a custom activity key,
and native projected-log output using a custom timestamp key. Decoy default
fields distinguish the selected fields from defaults. Both routes select ABCD
rather than AB for source ABC and match ordered moves, cost 10,000, fitness
0.8, best-worst cost 50,000, and projected custom activity/timestamp values.
The inserted D retains the source's last selected timestamp.

`docs/edit-distance-parameter-integration.json.gz` retains the fixture,
checker, exact raw route outputs and build logs, hashes, and independent
review. Both executables were explicitly recompiled and relinked against the
unchanged verified core; source and binary bindings were checked across the
runs, and the owned server was stopped. The summary is in
`docs/edit-distance-parameter-integration-validation.json`. These two cases
verify parameter propagation for this fixture; they do not complete either
edit-distance catalogue row or the remaining selector-domain audit.

### Edit-distance exhaustive short-sequence checkpoint

All 1,922 singleton source/target pairs over the two-symbol alphabet A/B,
with each sequence length from zero through four, match the pinned public
variant through native typed whole-log alignment. Both ordinary and anti
modes are included. The comparison retains ordered moves, cost, fitness,
best-worst cost, source/target variants, and full input/option snapshots.
It compares 1,891 accepted results and the rejection meaning of 31 all-exact
anti cases; it does not claim Python exception-class identity.

The reference archive is `docs/edit-distance-exhaustive-reference.json.gz`.
Native outputs, generator/checker/probe sources, build provenance, and reviews
are in `docs/edit-distance-exhaustive-native-checks.json.gz`, with the summary
in `docs/edit-distance-exhaustive-validation.json`. The capture, harness, and artifact saver passed independent review.

This is exhaustive only for the stated singleton-log domain. Existing cohorts
provide multi-candidate ordering and long-input threshold evidence. Both
catalogue rows remained partial: the selector-domain audit had identified empty
and whitespace field names as a difference, and distinct registered
completion proofs and the final semantic audit remained open.

### Edit-distance fresh-projection activity-type checkpoint

The fresh public variant projector enforces the reference activity-type
contract. Typed whole-log alignment and the metadata-preserving native
projector remain separate APIs; their accepted activity domain does not imply
that the fresh public projector accepts those values.

The new `project_log_on_typed_variant` accepts typed argument values for
validation; successful projection retains the reference string-only activity
contract. The existing string-variant API shares that path. Present source
activities are checked for hashability before variant values, with missing
source keys deferred until after those checks. Target joining precedes
projection, and each nonempty source trace is joined before its timestamp
is read. Selected keys are exact; custom first-class fields do not silently
satisfy the default activity selector.

All 53 frozen public calls match through the typed-argument API, and all 24
representable legacy calls match through the string API. The preserved old
library matched 13 of those 24 legacy calls, with 11 observed differences;
it had no typed-variant API. All 404 earlier comparisons pass against the
same final library, for 481 native comparisons, along with the native
alignment suite and catalogue check.

Evidence is retained in `docs/edit-distance-typed-variant-reference.json.gz`,
`docs/edit-distance-typed-variant-native-checks.json.gz`, and
`docs/edit-distance-typed-variant-validation.json`. It includes frozen public
captures, before/after route outputs, probe/checker sources, input-mutation
checks, and library/input hashes. Independent production, capture, and harness reviews passed.

The comparisons retain ordered fresh-output attributes and distinguish missing
fields, unhashable activity keys, string-join failures, and timestamp reads.
At that checkpoint, both edit-distance catalogue rows remained partial; broader parameter adapters
and semantic completion proofs remained open.


### Edit-distance typed whole-log and long-input checkpoint

Typed whole-log alignment interns semantic activity keys in source-then-target
order, retains the first representative, and delegates target selection and
scoring to the existing whole-log engine. Decoded variants and moves preserve
numeric, Boolean, null, byte, tuple, and frozen-set values. Activity validation
is eager, including when one log is empty; missing fields and unhashable values
are rejected before the empty-input result or target check.

Typed JSON export and the CLI/server edit-distance routes use these results.
Projection through supplied alignments retains typed activity values. This
metadata-preserving native projection remains separate from the fresh public
variant projector. Gap serialization uses the move kind, so empty-string and
null activity values are not mistaken for gaps.

All 72 frozen typed whole-log calls and 19 long direct calls match. The 313
prior comparisons also pass against the same final library: 404 comparisons
in total. Twelve CLI cases and four live HTTP cases pass, including typed
projected values, event counts, and projection markers. The Release build,
native alignment suite, catalogue check, and independent production and
evidence reviews pass. Projection regressions also cover ordered attributes
and custom first-class field identities without mutating inputs.

Evidence is retained in `docs/edit-distance-typed-log-reference.json.gz`,
`docs/edit-distance-typed-log-native-checks.json.gz`, and
`docs/edit-distance-typed-log-validation.json`. The bundle includes frozen
reference inputs and outputs, probe/checker sources, exact result comparisons,
and before/after binary and input hashes. The prior header had no typed
whole-log API, so no native before-match count is invented.

The long direct-call corpus covers 199/200/201-element boundaries, popular
symbol counts around the cutoff, and equal-long-block candidate ordering.
These finite comparisons extend the tested domain; broader raw parameter
adapters, typed fresh-variant projection, and semantic completion proofs
remained open. At that checkpoint, both edit-distance catalogue rows remained partial.


### Edit-distance typed mapping checkpoint

The typed direct-trace interface accepts native event values as mapping keys
and retains those values in decoded moves. Numeric-equivalent keys share an
entry while the first key's type is retained and the latest encoding wins.
Null, tuple, frozen-set, and byte keys use the shared native key-comparison
implementation. Gaps remain separate from present null activity labels.

Cache lookup still uses the encoded source. A later mapping with a different
key type can therefore return the same earlier result object. Missing activity
fields and unhashable activity values are rejected before cache lookup.

All 35 frozen typed-mapping calls match. The 278 prior whole-log, table,
projection, and string-helper comparisons pass against the same library, for
313 comparisons. The Release build, native alignment suite, catalogue check,
and independent production/probe/checker reviews pass.

Evidence is retained in `docs/edit-distance-typed-mapping-reference.json.gz`,
`docs/edit-distance-typed-mapping-native-checks.json.gz`, and
`docs/edit-distance-typed-mapping-validation.json`. It includes raw mapping
constructors, the normalized mappings passed to Python, capture and native
probe/checker sources, exact typed outputs, cache identity observations, and
before/after input and binary hashes. The prior header had no typed direct
API, so this checkpoint does not invent a native before-match count.

This extends the direct-helper surface. Whole-log typed activities, broader
parameter adapters, and larger-input matching-block contracts remained open;
both edit-distance catalogue rows remained partial.

### Edit-distance direct trace and caller-cache checkpoint

The additive `apply_edit_distance_trace`, `apply_trace`, and `align_trace`
interfaces accept an ordered activity mapping, an ordered target list, an
independent exact-match set, and an optional caller-owned cache. Fresh results
contain alignment and cost; cache hits return the identical shared handle,
including opaque caller payloads and cached null values. Cache keys use the
encoded source, and a hit takes precedence over later target or option changes.

Direct ordinary ties retain the first list entry without a length preference.
Anti-alignment prefers maximum distance, then a longer target, then first list
occurrence. Unicode encodings are decoded by code point; reverse-map collisions
use the last mapping entry. Unmapped activities are omitted. For the native
string-key mapping, present numeric, Boolean, and null activities do not match
string labels. Missing selected fields are rejected, and a differently named
first-class activity field cannot satisfy the selector.

The probe retains the reference's distinction between enum and string activity
parameter keys. Native options hold the resulting effective selector. Cache
sequences execute against live shared handles without restoring expected cache
snapshots between calls. Comparisons preserve ordered moves, exact integer
costs, typed inputs, cache insertions, returned-object identity, and unchanged
arguments; expected failures compare mapped diagnostics rather than Python
exception-class identity.

All 66 direct public calls match, including the nine previously reference-only
calls. The 212 prior whole-log, table, and projection comparisons pass against
the same library, for 278 native comparisons. The alignment suite, Release CLI
build, catalogue check, and independent code/probe/checker reviews pass.

Evidence is retained in `docs/edit-distance-direct-reference.json.gz`,
`docs/edit-distance-direct-native-checks.json.gz`, and
`docs/edit-distance-direct-validation.json`, including frozen capture sources,
generated probe/checker sources, exact inputs, and before/after binary hashes.
The prior header did not expose a direct API, so no native before-baseline
match count is invented.

At that checkpoint, both edit-distance catalogue rows remained partial. Typed mapping keys are
extended by the checkpoint above. Invalid parameter shapes, ordering, and
larger input contracts still needed proof; the project then retained 232 incomplete
coverage units.

### Edit-distance table, projection, and target-selection checkpoint

The native EventDataView overloads now accept table and mixed log/table inputs,
including a configurable case-ID selector and first-seen case grouping. Public
`project_log_on_variant` returns fresh log/trace metadata and only the selected
activity, timestamp, and conformance event fields. Empty source traces are
omitted. Timestamp reads follow the captured synchronous-move cursor: skipped
log moves do not advance it. Missing fields at a read point are rejected;
present null and typed NaT timestamps are preserved.

Equal-distance normal alignments prefer shorter targets; anti-alignments prefer
longer targets. Equal-length ties select the last distinct target in
first-occurrence order. Anti-alignment rejects a trace whose target candidates
all have zero distance. Selection distance and reconstructed alignment cost
remain separate, including substitution as two emitted deviation moves.

All 104 new pinned public calls match, compared with 41 before this checkpoint.
The 108 prior whole-log comparisons also pass, for 212 comparisons in this
validation. The native alignment suite, catalogue check, and independent code
and probe reviews pass. Reference inputs and binary/source hashes are retained.
Comparisons preserve trace/event/move order, typed values, exact integer costs,
exact numeric fitness, and mapped diagnostics; attribute-map order alone is
canonicalized. Python exception-class identity is not claimed.

Evidence is in `docs/edit-distance-table-reference.json.gz`,
`docs/edit-distance-table-native-checks.json.gz`, and
`docs/edit-distance-table-validation.json`. The baseline report is retained
unchanged. A separate audit corrects its probe's Boolean wire encoding in a
copy of the saved values and confirms that every original match result, and
the 41/104 count, is unchanged. The final probe exercises the new table adapter
and calls the public projector directly.

At this table checkpoint, direct helper mapping/cache interfaces and broader
typed-activity and parameter contracts remained open. Its nine direct-helper
observations were reference-only; the direct checkpoint above compares them.
At that checkpoint, both edit-distance catalogue rows remained partial, with 232 incomplete project
coverage units. The following checkpoint is historical evidence for the prior
empty-target and equal-length anti-tie fixes.

### Edit-distance empty-target and anti-tie checkpoint

The earlier pinned public corpus covers 36 whole-log scenarios through three reference
entry points: variant `apply_log`, variant `apply`, and dispatcher `apply`.
All 108 native comparisons match, up from 90 before the fixes. The native helper
rejects an empty target even when the source is also empty. Its equal-length
anti-alignment cases select the last distinct target in first-occurrence order;
duplicate targets do not move their first occurrence. At that checkpoint the
ordinary lexical tie rule was unchanged; the later target-selection evidence
above supersedes it. The shortest-target fitness denominator is unchanged,
including negative anti-alignment fitness.

Comparisons retain exact ordered moves, integer costs and `bwc`, numeric fitness,
and the mapped empty-target diagnostic. Empty-string activity labels remain
distinct from gap markers in the probe. Saved baseline values were rechecked
under these exact comparisons after tightening the checker. The native
edit-distance suite passes and independent review is clear.

Evidence is in `docs/edit-distance-core-reference.json.gz`,
`docs/edit-distance-core-native-checks.json.gz`, and
`docs/edit-distance-core-validation.json`. Nine captured direct-helper calls
were explicitly reference-only at that checkpoint and are now compared above.
DataFrame/projection comparisons and direct helper mapping/cache support were
still open at that checkpoint and are extended above; broader typed activity
behavior was still unfinished at that checkpoint.

`EDIT_DISTANCE` computes Levenshtein distance for every source variant and picks
the minimum (or maximum when `perform_anti_alignment` is enabled); duplicate
target variants retain their first occurrence. Equal-distance ties prefer the
shorter target for normal alignment and longer target for anti-alignment, then
the last distinct first occurrence when lengths are equal. Reconstruction follows `difflib.SequenceMatcher` matching-block
semantics, including the default autojunk suppression for popular symbols in
large targets, but uses ordered maps for deterministic clean-room behavior (the
pinned hash-order implementation could choose a different tied path). A
substitution is represented as two moves (one log-only and one model-only), so
deviation cost, fitness, and the reference best/worst-cost denominator (`bwc`) are
explicit and auditable. Empty source logs return no alignments when the target
is nonempty; an empty target log and non-positive deviation cost are rejected.
Anti-alignment also rejects all-zero-distance target candidates.

The native `project_log_on_alignments` extension uses each trace's independently
selected target variant, including heterogeneous selections. It preserves source trace
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


## Backwards token-replay module acceptance

The backwards variation now has a distinct catalogue proof for
`replay_token_traces` and the registered `backwards_module_acceptance` case.
The completion audit and direct EventLog scope are recorded in
`docs/token-replay-parity-contract.md`. Reviewed controls, denominator and
weighted-routing comparisons are supplemented by all 62 passing parameter/input
observations and 11 affected native suites after the typed-activity correction
in `7b6fd7e`. The post-fix evidence is preserved in
`docs/token-backwards-parameters-after-checks.json.gz`.

The structural verifier accepts 198 complete and 189 partial module rows. Only
`algo.conformance.tokenreplay.variants.backwards` is promoted by this checkpoint;
its dispatcher and the forward variation remain partial. Overall, 229 coverage
units remain open.


## CLASSIC flat timestamp reference checkpoint

`docs/heuristic-classic-flat-timestamps-reference.json.gz` preserves twelve paired
EventLog/DataFrame fixtures through explicit public CLASSIC discovery: 24 calls
with unchanged typed input snapshots. The reviewed capture, initial snapshots
and sources are retained in
`docs/heuristic-classic-flat-timestamps-capture-evidence.json.gz`; its summary is
`docs/heuristic-classic-flat-timestamps-reference-validation.json`.

Ten pairs return results, nine of which agree across the captured public graph
fields, including occurrence and boundary counts, DFG structures, matrices,
node metadata, AND measures and performance decoration. Custom case selection
changes DataFrame grouping while grouped EventLog trace boundaries remain
authoritative. A wholly absent selected start field raises the same KeyError
on both routes; a missing EventLog event field raises where the corresponding
DataFrame date cell is NaT and succeeds. The explicit NaT control succeeds on
both routes with mean performance 2.5. Reversed completion timestamps preserve
the supplied A-then-B sequence in both captured routes.

The native checkpoint below implements flat-input support and typed NaT
handling, with a comparison of the represented fields. CLASSIC remains partial
and coverage counts are unchanged.

The direct module entry point is separately captured in
`docs/heuristic-classic-direct-reference.json.gz`, with review and bindings in
`docs/heuristic-classic-direct-reference-validation.json`. Twelve fresh
`classic.apply_heu(EventLog)` calls produce ten results and two errors, exactly
matching the reviewed dispatcher EventLog inputs and outcomes. Inputs and
parameters remain unchanged. This closes the direct-route reference gap without
claiming native completion.

CLASSIC promotion still requires resolution of the public node boundary flags
and sojourn map, a concise reconciliation of its returned public fields with
existing EventLog and direct DFG evidence, direct verification of the
Petri-returning `apply` / `apply_dfg` routes, and its own catalogue
implementation/test proof. `docs/heuristic-classic-parity-contract.md` records
the route-by-route acceptance gates. Parent input adapters and PLUSPLUS
obligations remain separate catalogue gates.

A focused business-hours control is preserved in
`docs/heuristic-classic-nat-business-reference.json.gz`, with runtime bindings and
review in `docs/heuristic-classic-nat-business-reference-validation.json`.
With the same explicit NaT input and `business_hours=true`, both public carriers
raise ValueError (`cannot convert float NaN to integer`) and leave inputs
unchanged. Ordinary elapsed-time acceptance of NaT therefore does not imply
business-hours acceptance. The native checkpoint below verifies both errors,
mapping them to invalid_argument with the same message.

Three existing direct-DFG boundary inputs are recaptured in
`docs/heuristic-classic-dfg-boundaries-reference.json.gz`, with validation in
`docs/heuristic-classic-dfg-boundaries-reference-validation.json`. The inferred
list boundaries and the supplied start/end maps retain their public shapes;
all returned node start/end flags are false and all sojourn maps are empty.
All three calls preserve their inputs, and opaque CLASSIC module proofs match
before and after the calls. This supplies missing public-object observations
for those cases; it does not establish native object parity or change coverage.

## CLASSIC native flat-input and typed-NaT checkpoint

`docs/heuristic-classic-flat-native-checks.json.gz` preserves the actual native
output, comparison scripts, source/binary bindings, build/test logs, and earlier
failed attempts; the summary is
`docs/heuristic-classic-flat-native-validation.json`. All 26 observations pass:
the twelve paired fixtures plus the two business-hours errors. The comparison
checks exact input fields, typed date/NaT payloads, row order, table dtypes/index,
selectors, unchanged input snapshots, and represented result fields. Numeric
values compare exactly; node occurrence counts come from actual native node
statistics. The twelve direct CLASSIC EventLog observations have identical
inputs and outputs to their paired dispatcher cases and add no unique inputs.

The native API now accepts EventDataView, EventStream, EventTable and
LazyEventTable, with a configurable case attribute. Flat grouping follows the
selected key even when another field carries canonical case-role metadata;
existing EventLog trace boundaries remain authoritative. The shared converter
is unchanged. CLASSIC elapsed-time NaT contributes a zero sample, while
business-hours NaT rejects and absent selected destination fields retain their
missing-field diagnostic. Frequency discovery does not validate timestamps.

The registered `classic_flat_timestamp_acceptance()` case also checks tagged
table/stream/lazy grouping and a bounded PLUSPLUS grouping regression. Six
affected native suites pass: core, heuristic_variants, log_conversion,
event_data, dataframe_format and dataframe_format_json. Independent review
cleared the repaired implementation and fixture changes. This checkpoint does
not claim full PLUSPLUS parity or full public-object parity: node start/end
flags and the sojourn map still lack native fields. Python class and exception
identity are outside the native result/error mapping. Coverage remains
198 complete / 189 partial modules and 46 complete / 40 partial packages.

## CLASSIC native object-field checkpoint

`docs/heuristic-classic-object-native-validation.json` and its checks archive
record 31 passing observations: the 26 flat controls and five direct DFG
controls. CLASSIC now stores and serializes actual false node start/end flags
and an empty named sojourn map, independently of internal boundary membership.
At that checkpoint, PLUSPLUS left the new optional fields absent. The registered
`classic_object_fields_acceptance()` checks direct DFG and EventLog behavior,
including fallback nodes, nonzero boundary counts and performance decoration.

All six affected native suites pass, and independent implementation review
found no actionable defects. The archive retains actual native output,
source/binary bindings, comparison scripts, and the initial threshold-type
comparison failure; the repaired check compares the floating threshold exactly
while preserving strict integer count checks. This supersedes the preceding
checkpoint's missing-node-field limitation. Raw auxiliary-map presence and
direct Petri route comparisons remain open; coverage counts are unchanged.

## CLASSIC auxiliary-map presence checkpoint

`docs/heuristic-classic-auxiliary-native-validation.json` and its checks archive
record 29 matching observations: 26 flat observations and three exact direct
DFG controls. Optional public window-two and triple inputs now retain their
presence independently of their concrete maps. Omitted direct inputs serialize
as `null` in `classic_public_fields`; explicitly empty and EventLog-computed
empty maps serialize as `[]`. Existing array fields remain available, nonempty
legacy map assignments imply presence, and PLUSPLUS output is unchanged.

Six affected native suites pass. Independent review found a test-only JSON
substring boundary error; the repaired assertions were reviewed and rerun.
The archive retains the failed first run and the verified inputs, actual
outputs, scripts, source/binary bindings and final logs. This closes the
preceding checkpoint's auxiliary-map presence gap within the supported API.
Direct Petri conversion and the distinct catalogue proof remain open; coverage
counts are unchanged.

## CLASSIC native Petri baseline

`docs/heuristic-classic-petri-native-baseline-validation.json` and its archive
preserve 22 actual native route checks, with 21 matches. The 18 primary cases
match except for `metadata_3_0_0_0`; both XOR/AND controls additionally match
through the Petri and HeuristicsNet routes. The comparison preserves weighted
directed topology, place/transition kinds, visible and silent labels, and
per-place initial/final token counts. Generated names are retained in raw
reference evidence but excluded from structural isomorphism.

This baseline also excludes public property payloads from isomorphism and
proves timestamp/offset values rather than Python timezone identity. Its Berlin
EventLog fixture carries native named-zone metadata while the reference uses
a fixed offset. Review additionally identified missing success guards for
duplicate case keys and executable hash changes; the current keys are unique
and hashes unchanged, but these guards must be added before the comparator is
used as a completion gate.

The unresolved DFG `A->B:1, B->A:2` keeps two visible transitions with no accepted
dependency edges or inferred boundaries. Reference conversion leaves their
inputs unwired and gives each a private output place (four places/two arcs in
total); native conversion adds two private input places (six places/four arcs).
The archived baseline is bound to the earlier verified core/header snapshot,
not the subsequently rebuilt shared checkout. It also retains the initial
comparison that misread two reference KeyErrors as empty nets, followed by the
corrected explicit exception mapping. No completion or coverage promotion is
claimed from this baseline.

## Inputless Petri controls and corrected comparison

`docs/heuristic-petri-inputless-reference.json.gz` and its validation summary
preserve the direct CLASSIC DFG control and the timed `S,A,B,A,E` EventLog control
through CLASSIC and PLUSPLUS public Petri routes. Both EventLog results have
five places, four visible transitions and five arcs; A/B inputs are unwired,
their outputs retain private places, and the source/final markings remain.
The authoritative PLUSPLUS capture explicitly checks that the dispatcher enum
identifies the opaque-pinned variant module, with an optimization-safe guard.

The archive includes provenance metadata for an earlier discarded probe task
that accidentally searched reference source. That agent produced no accepted
probe or files after exposure. A fresh-context agent captured the public calls,
and the primary agent subsequently added and verified the public identity guard.
No reference source contents are embedded. Earlier capture iterations remain
available with their stated provenance limits.

`docs/heuristic-classic-petri-corrected-baseline-validation.json` records the
corrected native comparator: 22 unique cases, stable executable hash, fixed
offsets derived from actual timestamp payloads, and explicit validation of all
18 returned Petri results' available empty property maps. Negative gate checks
reject duplicate keys and executable hash changes. It still reports the same
21/22 matches against the immutable pre-fix core. Generated names and Python
timezone identity are outside the native mapping; nonempty public properties
are rejected as unsupported. Native repair and broader DFG verification remain
open, and coverage counts are unchanged.

## Small-DFG public Petri corpus

`docs/heuristic-classic-petri-small-dfg-reference-validation.json` and its archive
contain 1,779 ordered input cases, each called through direct CLASSIC Petri and
HeuristicsNet routes. All 3,558 calls succeeded and preserved their DFG and
parameter maps. The corpus enumerates the four directed pairs on two activities
(including loops) with frequencies absent/1/2, and the nine pairs on three
activities with frequencies absent/1, each at dependency thresholds 0, 0.5 and
1. Only the DFG and threshold are supplied; declarations, occurrence counts,
boundaries, auxiliary DFG/triple maps and performance maps are omitted.

The archive retains actual object fields, names, properties and markings,
the complete ordered fixture manifest, source/helper bindings and opaque
before/after module proofs. This is a finite reference corpus for detecting
additional topology differences. Native comparison was pending at this capture
checkpoint; its successful public calls alone do not establish full module parity.

Review identified that the first HeuristicsNet serializer pass reported its
wrapper type. The corrected capture records the actual returned public type;
all other observation fields, the fixture manifest and module proofs match
the first run. The rejected run is preserved as a nested archive with its
original scripts and bindings.

## Inputless Petri native repair

`docs/heuristic-petri-inputless-native-validation.json` and its archived checks
record the repaired converter: empty joining alternatives leave transitions
unwired, while empty output alternatives retain their private places. This
resolves the earlier direct-DFG mismatch and matches the CLASSIC and PLUSPLUS
timed EventLog controls, including actual source/final markings.

All three affected native suites pass. The corrected comparison matches all
22 archived route observations, and the additional inputless comparison matches
all three controls with exact input/options fidelity. These 25 observations
include a repeated DFG input. The archive binds the staged code tree, actual
core, test binaries, scripts, observations and private probe headers. Independent
review found no actionable converter, test or comparator defects. The broader
1,779-case native comparison followed as a separate gate; full HeuristicsNet
fields are outside this Petri comparison, and coverage counts remain unchanged.

## Supplemental CLASSIC returned-object gates

`docs/heuristic-classic-object-supplement-reference-validation.json` and its
archive preserve nine pinned public calls with unchanged inputs. Five confirm
that NaN, positive/negative infinity, nested infinity and the literal string
`NaN` retain distinct values and types in returned performance maps and edges.
Native discovery has a corresponding typed representation; the subsequent
object checkpoint below supplies the required direct native comparison. JSON
export rejection is a separate
transport policy and does not prove or disprove object preservation.

Four insertion-order controls expose a further supported-input gap. Reversing
the disconnected DFG `A->B:1, Z->Y:1` reverses the inferred start and end lists;
a fork with singleton boundaries stays unchanged. The prior native map could
not carry this order; the later `dfg_order` checkpoint below repairs it. The
earlier sorted-map cohort therefore could not close
the full boundary-order gate. Both public captures passed independent review,
and the archive retains scripts, exact raw observations and bindings. No
module promotion or coverage-count change is claimed.

## Small-DFG native Petri baseline

`docs/heuristic-classic-petri-small-dfg-native-baseline-validation.json` and its
archive preserve all 1,779 native observations, comparison details, inputs,
options and build/source bindings. The structural comparison matches 1,767
cases and retains twelve topology mismatches, all in three-activity graphs at
dependency threshold zero. Each has one fewer native place, one fewer silent
transition and two fewer arcs than the public result. Native execution and
exact input/options fidelity pass for every case; there are no global failures.

The executed runner, actual core, private source/header copies, probe binary
hashes and full failed graphs remain bound in the archive. Independent review
cleared the comparison and a supplementary proof that all 148 compiled headers
match the immutable source tree. The future runner now requires complete header
inventory/content before capture; guard tests reject missing/extra and changed
headers. Generated names remain outside structural isomorphism, available empty
public properties are checked, and the HeuristicsNet diagnostic covers selected
fields only. This baseline retains the twelve failures subsequently resolved
by the join-bridge checkpoint below.

## CLASSIC DFG order and non-finite object checkpoint

`docs/heuristic-classic-order-nonfinite-native-validation.json` and its checks
archive record the native ordering repair. Optional `dfg_order` carries the
direct DFG's insertion order into inferred boundary lists and must contain
every supplied edge exactly once. Existing map callers keep their established
order; explicit boundary values retain precedence. Registered acceptance tests
reject duplicate, missing and foreign paths, distinguish empty input/order,
and preserve inputs and options.

All four order controls and all five non-finite/string controls match their
pinned public observations. The latter inspect actual native performance maps,
edge representations and relation representations with typed NaN, signed
infinities, nested infinity and a distinct literal string. Valid fixture JSON
encodes those values only for comparison; production export policy is unchanged.
Isolated Debug variant checks and all three affected Release suites pass.
Independent review found no actionable implementation, test or comparator
defects. This closes the two supplemental object gates. The twelve archived
small-DFG Petri failures remained open until the join-bridge checkpoint below;
coverage is unchanged.

## CLASSIC Petri join-bridge checkpoint

`docs/heuristic-classic-petri-small-dfg-native-validation.json` and its checks
archive record all 1,779 matching native Petri outputs, resolving the twelve
failures retained in the baseline. Exact inputs and options remain unchanged,
with zero native errors or global mismatches. The graph comparison includes
place/transition types, visible and silent labels, weighted directed arcs and
actual initial/final markings. Generated names remain outside isomorphism;
captured public properties are required to be available and empty.

The generic reducer retains its existing default behavior. HeuristicsNet
conversion enables a narrow option that preserves a unary silent bridge when
its input has multiple ordinary producers and its output has another ordinary
producer. Registered regressions check default contraction, the preservation
condition, the complete case 126 topology and disconnected boundary markings,
and unchanged neighboring cases 125/127. All four affected Release suites pass.

`docs/heuristic-classic-petri-bridge-controls-validation.json` and its checks
archive additionally record all 22 earlier route observations and all three
inputless controls matching the same build. These 25 observations include a
repeated DFG input. The new 22-route and broad corpus probes serialize actual
`dfg_order`, options arrays/enums and workflow markings; the older three-control
input serializer explicitly leaves `dfg_order` unobserved. All complete private
header inventories and contents, bound native sources, core, executable,
fixtures, runners and logs remain tied to the immutable code tree. Independent
review found no actionable implementation, test or comparison defects.

The original failing baseline stays archived. The 1,779-case HeuristicsNet
diagnostic still compares selected fields only, and these Petri checks do not
claim full object equivalence. CLASSIC remains partial pending the separate
module gate audit and distinct catalogue proof; coverage counts are unchanged.

## CLASSIC full returned-object audit

`docs/heuristic-classic-full-object-gate-validation.json` and its checks archive
record a full field audit of the 1,779 saved direct-DFG observations. Every
observed field matches, including the actual per-node AND maps in 288 cases.
The earlier native serializer did not capture raw `performance_dfg` presence
or values. The audit reports both outer and nested paths as unobserved in every
case, so there are zero full-object passes. Empty-array projections cannot
settle these fields, and this corpus has no nonempty window-two or triple maps.

The archive also retains a fresh public replay of all 1,152 decorated-value
fixtures with exact ordered arguments and map presence. It captures 1,080
complete results and 72 errors matching the historical inventory, error types
and messages. All inputs remain unchanged, with zero serialization failures.
There are 288 nonempty window-two inputs and 288 nonempty triple inputs; 264
successful outputs retain each map and its corresponding matrix. Complete
returned objects and typed raw payloads supply the remaining native comparison
targets. Before/after opaque module proofs match the pinned reference commit;
capture scripts, serializer helpers, fixture construction and ordered call
specifications are hash bound.

This checkpoint preserves the remaining field gaps. Actual native raw
performance-map observations and the nonempty auxiliary returned-object
comparisons are still required before the separate catalogue proof can be
added. CLASSIC and coverage counters remain unchanged.

## CLASSIC actual full returned-object checkpoint

`docs/heuristic-classic-full-object-native-validation.json` and its checks
archive now retain complete actual native captures for both saved cohorts.
All 2,931 outcomes pass: 1,080 decorated-value objects, 72 expected missing-path
rejections and 1,779 small-DFG objects. All inputs and options remain unchanged.
Every returned-object field category matches, with zero unobserved or
unsupported cases. The decorated-value cohort includes 264 successful nonempty
window-two maps/matrices and 264 successful nonempty triple maps/matrices;
the small-DFG cohort includes actual nonempty per-node AND maps in 288 cases.

The captures observe actual optional performance-map presence and typed
payloads, raw maps, node flags, loops, edges and serialized fields. Matrix
comparison preserves retained null, boolean, integer, string, array and object
values independently of edge-display representations. The archive retains the
original incorrect projection and its 132 failing objects/306 diagnostics,
the reviewed correction, both failed probe attempts and the successful command
records, logs and outputs. All 2,906 archive members preserve their exact bytes
and hashes. Reference module proofs remain pinned to 2.7.22.

The compiled core and complete source/header bindings are unchanged. The four
affected native suites and prior 1,779/22+3 Petri checks are reused; this
checkpoint does not report new suite or converter runs. The new small-DFG
captures match the earlier common native models exactly.

The current full-object cohorts have identical raw and cleaned DFG domains.
Supplemental controls must observe an actually pruned path to establish the
performance-matrix domain. CLASSIC remains partial pending that check and its
distinct catalogue proof; coverage counters are unchanged.

## CLASSIC noise-domain gate and module completion

`docs/heuristic-classic-noise-matrix-native-validation.json` and its checks
archive preserve eight exact public/native calls over the ordered DFG
`A->B:1000, B->C:1000, A->C:1`, pairing noise 0 and 0.5 for absent, complete,
retained-null and incomplete performance maps. All seven complete objects and
one expected missing-path rejection match. Inputs and options remain unchanged,
with zero field gaps, unsupported outcomes or mismatches.

Actual pruning establishes that public DFG and matrix domains use cleaned
paths. The supplied raw performance map retains the removed path and its typed
value. Explicit null in a retained matrix cell remains separate from the edge
display fallback. An incomplete map rejects while that path is retained and
succeeds once it is pruned. The independent projector changes only the domain
source; all raw-map and edge/relation checks remain active. Twenty-two synthetic
checks and independent review pass.

The archive binds the public and native captures, all four actual command
records and logs, opaque reference module proofs, exact immutable source/header
inventory, core, probe, fixtures, comparator and successful report. The prior
2,931 full-object outcomes, 1,779/22+3 Petri observations and four affected
native suites are reused with their existing immutable bindings. No core change,
new suite run or converter run is claimed by this evidence checkpoint.

The route/parameter/returned-field audit has no remaining CLASSIC gates within
the documented native representation. The catalogue now supplies the distinct
`discover_heuristics_net` implementation proof and registered
`classic_dfg_order_preserves_inferred_boundary_lists` acceptance case. The
structural verifier passes: **46 complete / 40 partial packages and 199 complete
/ 188 partial modules, with 228 open coverage units**. CLASSIC is complete;
the heuristics dispatcher and PLUSPLUS remain partial.

## Alpha+ process selection evidence and remaining law gap

`docs/alpha-plus-process-law-validation.json` binds three streaming UTF-8
member archives: `alpha-plus-process-law-public-baseline-checks.jsonl.gz`,
`alpha-plus-process-law-order-label-checks.jsonl.gz`, and
`alpha-plus-process-law-native-checks.jsonl.gz`. They retain 79,560 public
baseline calls, 43,200 public order/label controls, and 14,400 actual native
rows. Every member preserves its exact bytes, size and hash; archive headers,
inventories and tails round-trip. Independent review and seven codec checks
pass. The archives contain 198, 132 and 1,197 members, respectively.

The 72,000-call public baseline has constant outcomes within all 3,600
process/case cells of 20 repeated calls. Ten fresh processes for each recorded
hash seed also agree for every case. In contrast, the first three audited
native processes contain 4,320 rows: each process's 36 graph-choice cases
switch between two weighted graph structures, and its 36 error-prone cases
mix graph results with the mapped native rejection. Case 0 changes from a
detached initial token and one live place to a live start place plus a silent
transition and arcs. This is a structural difference within one process.
The complete native capture contains 10,853 graphs and 3,547 errors.

The public controls cross five hash seeds, original/reversed/rotated case
orders, and original versus renamed ordinary labels. The literal
`artificial_start` and `artificial_end` labels remain fixed. All 2,160
process/case cells stay constant across their 20 calls, with no input mutations
or serialization failures. Case-list order changes no observed error counts,
boundary distributions or summary/weighted-arc outputs. The complete controls
contain 34,560 graphs and 8,640 `IndexError` outcomes.

Renaming `a` to `label_0` changes outcomes after applying only the declared
inverse label map to structured comparison fields. It flips error counts in
216 matched seed/order/case groups and changes graph/boundary signatures in
378 further successful pairs. For seed 0, case 30, all original-label calls
have five arcs and a live initial start token; all renamed calls have two
arcs and a detached start token. Both retain a live final token. Composite
name collisions occur in 84 of the changed graph pairs, so these summaries
do not establish general graph isomorphism. Raw typed names and graphs remain
unchanged in the evidence. These observations do not identify the cause of
label sensitivity.

The guarded native resume preserves the first 4,320 rows, executes only the
seven missing processes, and records the actual resume and command contexts
separately from the compiled tree, parent and core. Its captured row count is
14,400; the exact total of actual calls remains unknown because the stopped
attempt may have made unrecorded calls. Source/header inventories, core,
probe, fixtures, commands, logs, inputs/options and public API/runtime/opaque
module proofs stay bound. Existing core build/test evidence is reused.

The older 774-row inventory has 598 successful support objects, all with one
live final token and no detached final marking. Its varying joint marking
pairs differ only in their initial coordinate. Final and joint-choice behavior,
within-log construction order, and the faithful native selection rule remain
open. Alpha+ stays partial; coverage remains 46 complete / 40 partial packages,
199 complete / 188 partial modules and 228 open units. Finite captures do not
prove universal support or probability parity.

### Alpha+ final-marking and trace-order pilot

The finite paired pilot in
`docs/alpha-plus-final-joint-public-checks.jsonl.gz` and
`docs/alpha-plus-final-joint-native-checks.jsonl.gz` preserves 9,600 actual public
calls and 9,600 actual native calls. Twelve EventLog families, each with cleanup
off and on, produce 24 cases. Twenty fresh processes cross five hash seeds,
two case-list orders, and two trace-list orders; each case is called 20 times
with fresh inputs. Only the outer trace list is reversed: event sequences,
trace multiplicities, and the directly-follows multiset stay unchanged.
The native `short_loops=true` counterpart is separate from the actual public
parameter map. `PYTHONHASHSEED` records native process context and does not seed
the native random generator.

The public capture returned 7,520 graphs and 2,080
`IndexError("list index out of range")` failures, with no input mutations or
serialization failures. Every one of its 480 process/case cells stayed constant
across 20 calls. All 7,520 successful final markings contain one live `end`
place with one token; no detached final marking or variation in both initial
and final choices was observed. Changing case-list order changes none of the
240 matched comparisons. Reversing trace-list order changes 28 of 240 matched
comparisons: eight have unique names and 20 involve a name collision, so those
20 do not establish general graph isomorphism. At seed 0, cases 6/7 change
from detached `start` and one arc to live `start` and three arcs. At seed 4,
cases 10/11 change from six to nine arcs while both boundary markings stay
live. Cleanup changes the graph signature in 118 of 240 matched pairs without
changing marking or error outcomes.

The completed native audit reconciles all 7,591 graphs and 2,009 discovery
failures, with unchanged inputs in all 9,600 calls. Every error is
`std::invalid_argument` with message
`Alpha+: boundary cleanup has no incident transition`. Across all 480
process/case cells, 40 mix graph and error outcomes (cases 8/9), 80 vary initial
and joint marking support (cases 6/7 and 12/13), and 120 vary unique-name
topology and endpoint multiplicity (cases 6/7, 10/11, and 12/13). Final support
varies in none: all 7,591 successful rows end at live `end` with one token, and
none has a detached final marking. There are 423 detached initial `start`
rows. In process 0/case 6, trial 0 has attached `start`, while trial 3 has
detached initial `start`; both end at live `end`. The final audit corrects the
preliminary success/error classification used by the prior checkpoint.
Four hundred rows with duplicate typed place names are excluded from
name-based graph comparison. This within-process variation differs from the
public pilot's stable cells. It does not establish a universal probability
law, a causal effect of ordering, or general weighted graph equivalence.
Native ordinary arc multiplicity is recorded as repeated place IDs; no
independent arc-weight attribute is manufactured.

The actual public execution manifest is
`e3063941fffc36dce1bdd900fb77dc45cc65437d2fac72127a18e11ff492e9a4`;
the actual native manifest is
`72b017f26119566ba8ff4e61c2003275508d2a0e1d88f737089dbe2111deb206`.
Native capture context before and after is `f25cfa2`, distinct from compiled
tree `03522b85b63cc336dcfc642fa3de8e658b6bdb0e`, compiled parent `25b2cb4`, and
core `885b7b5822df6c4cf0b6b56328d338599b3493e2c46f50032912db4949554134`.
Only the new probe was compiled; earlier core tests are reused. The capture
completed without interruption, so its actual call count is exactly 9,600.

`docs/alpha-plus-final-joint-validation.json` records the ready preparation,
source and binary bindings, actual command logs, inputs, options, and outcomes.
The reviewed saver passed 12 focused checks and roundtripped every UTF-8
member byte, SHA, and count using the unchanged streaming codec: 95 public
members and 1,225 native members. Both archives are below the size guard and
use deterministic gzip. Alpha+ remains partial; the 228 open coverage units
are unchanged. Final and joint selection laws still need evidence beyond
this finite pilot.

### PLUSPLUS returned sojourn map and node flags

PLUSPLUS now returns an engaged `sojourn_times` map containing every raw
activity's computed mean, including labels removed by `min_activity_count`.
Empty inputs return an engaged empty map. Its retained node statistics expose
`is_start_activity` and `is_end_activity` as explicit `false` values, preserving
the separate start/end occurrence counts. This changes returned fields without
changing edge selection, bundles, activity filtering, metadata, or CLASSIC
behavior. Existing JSON serialization already supports engaged empty maps and
false optional flags; no exporter change is required.

The public replay made 12 actual pinned PLUSPLUS calls reconstructed from the
existing local-label capture: canonical and custom activity keys, escaped and
Unicode labels, minima 1/2/10, interval overlap, empty inputs, and frequency and
performance decorations. All 12 returned objects expose the captured 17 net
fields, all canonical serializers succeed, and all 21 observed nodes have both
flags present as boolean `false`. Inputs and ancillary fields remain unchanged.
The new registered `plusplus_returned_object_fields_acceptance` test checks
independent literal sojourn values, filtered and entirely filtered maps, empty
maps, flags, boundary counts, and exact JSON escaping.

A fresh build from reviewed preview tree
`513be22bc70ad27b4e08bf72c37d96b88768436b`, parent `f25cfa2`, passed the four
Release suites `core`, `heuristic_variants`, `petri_net_conversion`, and
`petri_net_reduction`. Its core SHA is
`8a1abd9236276c5de53ebc9025089ef2455d1beb6070b21aee05c8a5726ad8ec`;
build manifest SHA is
`a7af0234d86af5b458f64d967ebf178b7e6c56282f10d894c12e29e35312f359`.
The prior compiled core remains immutable. Integration copies the three
reviewed files from the actual tested source export, matching its exact bytes;
the commit's documentation tree is later than the preview build's tree.

Twelve actual native CLI calls match the public returned sojourn maps and
strict boolean node flags, including node sets. The same checks preserve all
seven saved projections: metadata, active nodes, edges, raw occurrences, raw
concurrency, dependency measures, and sojourn values. Public manifest SHA is
`5426b6c8c394e75115683b28d26aa56e230ee67adafaeef333b01a4a2dab1395`;
native manifest SHA is
`b98b18ed119fc733a93cdc5bb116c8e72b2f18cf3bffd05d73f4614be04fad74`.
The independently reviewed checker passed 14 focused checks. No public or
native calls were rerun to save this checkpoint.

`docs/heuristic-plusplus-returned-fields-checks.jsonl.gz` and
`docs/heuristic-plusplus-returned-fields-validation.json` preserve 743 UTF-8
members containing actual outputs, typed public fields, exact inputs/options,
command logs, source/header/test/CMake and preview bytes, helpers, preparations,
and native build evidence. The archive is 2,661,801 bytes, SHA
`959558628bb7d0bf4c42cc4e3fb778e8ba20fbc1e4ae4f576dbfa0279ba4ebb2`.
The reviewed saver passed nine codec checks and verified every member byte,
SHA, and count after deterministic compression. Native binaries and unrelated
binary assets are SHA-bound; our compressed constructor archives use exact
base64 envelopes. The validation SHA is
`5e4bcad55c7575a899e5fe96533e9c35c72e627594ff9da6bdc3a36125c220b8`.

This closes the finite returned-map and node-flag mismatch. It does not claim a
full native comparison of all 17 returned fields or broad PLUSPLUS Petri
conversion parity. PLUSPLUS remains partial and the 228 open coverage units
are unchanged. A separate subsequent field-gate audit accidentally searched
installed upstream source; it was stopped and quarantined. Its recommendations
are excluded from this change and future implementation. The completed source
change, public observations, independent review, build, and 12 native checks
precede that incident. `docs/plusplus-field-audit-cleanroom-incident.json`
records the incident metadata without upstream source content.

### Complete PLUSPLUS acceptance scope

Fresh independent read-only audits use only our explicit source/header paths
and saved public results. `docs/heuristic-plusplus-parity-contract.md` records
the full target: all 17 net fields, six node fields, three edge fields, both
direct variant routes, parent EventLog/EventStream/DataFrame routes, all nine
public parameter keys, input preservation, and Petri conversion. The existing
twelve public calls use the parent `algorithm.apply_heu` PLUSPLUS route;
inspected direct-route signatures are not counted as direct-route calls.

The audits identify representation obligations that the saved graph
projections do not settle: null auxiliary fields versus empty maps, direct
adjacency versus eventually-follows on longer traces, decoration-specific
performance values, relation domains through activity filtering, and positive
AND/loop maps. The inputless Petri control remains reusable, while connected,
branching, cyclic, and filtered shapes remain required. New capture preparation
is prospective and is not counted as executed evidence. The earlier four
Release suites and exact compiled-source verification are reused. No coverage
row is promoted; PLUSPLUS remains partial with 228 open coverage units.

### PLUSPLUS route controls and complete native objects

The subsequent public capture records 112 actual calls: 60 HeuristicsNet and
52 Petri calls, with 100 EventLog, six EventStream, and six DataFrame inputs.
All 27 paired direct/parent HeuristicsNet results and all 23 paired
direct/parent Petri topologies match. There are 109 successful results, no
serialization errors, and no input mutations. Three parent DataFrame Petri
calls raise `AttributeError("Can only use .dt accessor with datetimelike values")`;
their timestamp columns have object dtype. Those failures are retained, and
correctly datetime-typed DataFrame controls remain open.

The public controls discriminate all nine parameter selectors or thresholds.
Positive relation count 2 is retained at minimum 2 and removed at minimum 3,
including the returned DFG and matrix domains. Activity filtering removes
incident relation maps while retaining raw sojourn entries. Dependency and AND
acceptance are strict: a value equal to its threshold is excluded. Positive
AND measures, connected sequence/XOR/AND conversion, a longer overlapping
interval trace, and a performance mean of 5.0 are observed. Frequency
`performance_dfg` is `None`; performance `performance_dfg` is a dictionary.
These are finite public observations, not a completed native acceptance gate.
Missing-start/lifecycle inputs, actual cyclic conversion, collision-focused
names, and broader native field and carrier comparisons remain required.

Twelve new native calls serialize every HeuristicsNet member and the actual
inputs/options before and after each call. Only the new whole-object probe was
compiled against frozen core
`8a1abd9236276c5de53ebc9025089ef2455d1beb6070b21aee05c8a5726ad8ec`,
compiled tree `513be22bc70ad27b4e08bf72c37d96b88768436b`, parent `f25cfa2`.
The actual capture context is `cc6117a`, distinct from the compiled tree.
Fourteen command records cover configure, probe build, and twelve calls;
all calls succeed without mutation. A prior configure cleanup failure is
preserved separately with zero native calls. The reviewed Windows process
helper terminates residual MSBuild children before recording final logs.
The earlier four core Release suites are reused.

The reviewed comparison pairs these twelve complete native snapshots with
twelve reused public snapshots, adding no public calls. Its 348 cells cover
204 net fields, 126 node fields, and 18 edge fields: 248 match, 60 remain
ambiguous, and 40 are unrepresentable. Ambiguous candidates preserve their
actual values and do not count as passing. An unset native input
`performance_values` is not treated as a computed output; auxiliary empty maps
do not stand in for public `None`, and an unset edge scalar alternative does
not establish a frequency integer. The comparison report SHA is
`b8fd8efc40e83c565c5e530265f272ce16e43a663009fecb3e99bdaa02ce99e3`.

`docs/heuristic-plusplus-route-object-checks.jsonl.gz` and
`docs/heuristic-plusplus-route-object-validation.json` retain all 96 exact UTF-8
members, failed and successful command records, raw results and verdicts,
fixtures, helpers, preparations, and 654 external byte/hash bindings. The
reviewed saver passed ten focused checks and roundtripped every member byte,
SHA, and count with the unchanged streaming codec. The deterministic archive
is 1,200,433 bytes, SHA
`3e1d608659805dc810c0062f20bfce9a77c55973e339ed287046dad3668f6cc7`;
validation SHA is
`16a1db8c9c69d8ebbb83bff37b46cf137556617d4eca7b2618df7a319683d48e`.
No production source changes or new suite runs occur in this checkpoint.
PLUSPLUS remains partial, and all 228 open coverage units remain in scope.

### PLUSPLUS integer frequency values and supplemental controls

PLUSPLUS now stores the actual frequency count as an engaged integer scalar
alternative on both retained relations and accepted edges. The numeric double
view remains available. The regression checks the integer type and count,
dependency threshold exclusion, and the unchanged floating-point performance
mean. A fresh isolated build of canonical source tree
`fdfbd92359fe459ba6f4bf82c6df4ef6d2129815`, exact-byte export tree
`b321434e34ce285a4f20567180869441507e7ba0`, from parent `e6f37f2`, passes the
core, heuristic variants, Petri conversion, and Petri reduction Release suites
with assertions enabled. Its actual core SHA is
`dc5abfa0919b07ad305b53444f0ab419a9f032f9e816d9e1cd9b687b5d9a636d`.

`docs/heuristic-plusplus-frequency-build-checks.jsonl.gz` and its validation
preserve 116 exact UTF-8 members and 1,329 external byte/hash bindings,
including all 650 compiled inputs and six actual binaries. The three earlier
configure failures are retained with zero production build, test, or native
capture calls. The successful configure and build entries have an embedded
metadata discrepancy: a shared dictionary later adds test-binary bindings.
Their SHA-bound command files retain the actual before-state and are the
authoritative execution records; all other values agree. Twelve focused saver
checks and complete byte/hash roundtrips pass. Archive SHA is
`51160bd38d2013481ee0076e527d5ca2474fd3781eb1a15bd822e6e3b88b2cba`,
1,990,055 bytes; validation SHA is
`f764f5b5a4f349a6836ff22ba08af5c71621bea9f1e2d647bca15115ff7d3e99`.

The supplemental public capture makes seventy calls with seventy successful
results, no reference errors, and no serialization errors: 62 EventLog and
eight datetime-typed DataFrame inputs. Direct and parent EventLog routes match
in twenty-one paired HeuristicsNet controls and nine paired Petri controls.
Four parent DataFrame Petri calls append a float `@@diff` column. Actual
timestamp columns have `datetime64[us, UTC]` dtype; the planned nanosecond
dtype remains preserved separately. Overlapping DataFrame and EventLog
controls return equal HeuristicsNet fields but different Petri shapes. These
observations do not establish the cause of that carrier difference.

The capture worker exits 1 after saving all results because its own verifier
omits the returned `pandas.Timestamp` alias. Read-only finalization corrects
that alias and accepts the observed dtype without rerunning public calls.
The archive keeps the original failure, complete returned values, mutation
snapshots, and finalization separate. The finalizer's process exit 0 is an
observed ROOT execution result, not a synthesized command record.

The controls establish missing-start and lifecycle behavior, AB and BC
DFG values on plain ABC traces, and raw concurrency retaining a filtered
activity. Reciprocal ABA edges below dependency threshold zero produce an
actual cyclic Petri net. Equal and above-zero thresholds exclude those edges.
Self-loop observations cover HeuristicsNet fields only. Reserved names collide
across place and transition kinds; captured names are unique within each kind.
An unfamiliar decoration string is accepted publicly and returns performance
values; its native option representation remains unresolved.

`docs/heuristic-plusplus-supplement-checks.jsonl.gz` and its validation retain
64 exact UTF-8 members and fourteen external byte/hash bindings. Nine focused
saver checks pass, and every member roundtrips with the unchanged streaming
codec. Archive SHA is
`4876685618c248bdb7c32fdf22286c59e10e36842cc859c23d80d9b7027b7b51`,
1,351,759 bytes; validation SHA is
`af7209349f454752dea06818f65c7a5c8310c5d981caea625cac9df5ac10c2f4`.
Full returned-object, carrier, input mutation, and Petri acceptance remains
open. PLUSPLUS stays partial, with all 228 open coverage units retained.

### PLUSPLUS broad native capture and diagnostic comparison

Seventy actual native discoveries and forty-three conversions reuse the
tested core `dc5abfa0919b07ad305b53444f0ab419a9f032f9e816d9e1cd9b687b5d9a636d`
from canonical tree `fdfbd92359fe459ba6f4bf82c6df4ef6d2129815`. The 194 reused
public aliases deduplicate to 71 exact cases; seventy have a native carrier.
The unfamiliar decoration case covers two aliases and remains explicitly
unrepresentable without a native call. All 72 successful commands, seventy
probe processes, before/after inputs and options, complete native fields,
Petri incidences, zero marking entries, and actual call markers are retained.
There are no native discovery or conversion errors and no new public calls.

The original diagnostic compares 119 HeuristicsNet aliases across 4,573 cells:
3,823 matched, 234 ambiguous, 458 unrepresentable, and 58 mismatched. All
58 mismatches are the original literal-adjacency projection for `dfg` and
`dfg_matrix`, with 29 aliases per field. Each affected public DFG matches
native `eventually_follows_frequency` counts, and each public matrix flattens
to that same DFG. Overlap yields BD=4 and CD=4 against literal-adjacency counts
of two; a separate interval yields AC=1 with no literal adjacency. These are
comparison-projection differences, not evidence of absent native counts.
The original report remains immutable pending a separate corrected report.
Its three public reference errors and seventy-five pending Petri comparisons
remain explicit; a matching cell does not settle full returned-object parity.

Two earlier native probe attempts remain separate. MSVC C1128 stops the first
before any discovery or conversion. Adding `/bigobj` lets the second compile,
but its first process overflows its stack without a returned object or call
journal; its discovery and conversion counts are unknown. The successful
probe uses separate non-inlined case factories while preserving constructor
bytes and serialization. Its flushed call markers agree with actual returned
counts. Failed logs, process records, guards, and helper versions remain raw.

`docs/heuristic-plusplus-broad-native-checks.jsonl.gz` and its validation retain
315 exact UTF-8 members and 660 external byte/hash bindings. Twenty-six
focused saver checks pass, and every member byte, SHA and count roundtrips
with the unchanged reviewed codec. The archive is 18,480,503 bytes, SHA
`67d072a2d6fd1e08be7b08d140e4844436f0d8e4e8fbf84607cd5b0bd8c3db9c`;
validation SHA is
`572b0d1c16588289e04757c6e4124ffe1d41c62477a0b7718a7951d3b1945581`.
The four passing Release suites and 650 compiled inputs are reused without
new production edits or suite runs. PLUSPLUS remains partial, the catalogue
stays at 46/86 packages and 199/387 modules complete, and all 228 open units
remain in scope.

### PLUSPLUS corrected DFG and complete Petri diagnostic reports

The separate interval-frequency comparison preserves 119 HeuristicsNet
aliases and 4,573 cells: 3,881 matched, 234 ambiguous, 458 unrepresentable,
and zero mismatched. Actual `eventually_follows_frequency` statistics produce
the complete DFG and nested matrix for all 117 supported aliases. The two
unfamiliar-decoration aliases retain their missing native counterparts.
The original literal-adjacency report remains immutable in the prior archive.
Corrected report SHA is
`612aabda5840270703ee3cba5c2e7f21653b2fe420e1bf840aff6178b820c2ba`.
Fourteen focused correction checks pass with the prior fifteen checks reused.

The separate Petri report compares all 75 public aliases. There are 69
matching projected topologies and six differences: three original
object-dtype DataFrame reference errors against native success, and three
structural differences for correctly typed overlapping DataFrames. Each of
those public graphs has four places, four transitions and eight arcs; each
native graph has five places, four transitions and nine arcs. Four public
DataFrame input mutations remain explicit, with zero native mutations.
The report SHA is
`ad7aa0a5f058753be705d720256485522e228129d4c7cafb19d8576bb53514aa`.

The reviewed matcher preserves separate place/transition identities, visible
labels, silent transitions, directed multiplicities and initial/final tokens.
It retains raw explicit-zero and omitted marking domains, positive integer
arc weights and distinct arc objects. A projected unit-multiarc bijection
does not prove names, properties or weight-carrier parity. Search-budget or
vertex-limit exhaustion remains unresolved. Twenty-five focused matcher
checks pass. All 69 topology matches remain unrepresentable for full Petri
acceptance; the six differences remain three errors and three mismatches.

`docs/heuristic-plusplus-broad-comparison-checks.jsonl.gz` and its validation
preserve both complete actual reports, preparations, manifests, attempt
records, policies, frozen helpers with their original execution origins, and
static fixtures. The original broad capture/build/public evidence is reused
through exact external bindings. Twenty-two focused saver checks pass. All
29 exact UTF-8 members roundtrip byte, SHA and count with the unchanged codec;
935 external artifacts are bound. The archive is 2,219,138 bytes, SHA
`af7900ab402825ff91723bcc97efe7a8db9e47367adbded19df59ed2efbc83c3`;
validation SHA is
`ee74f776744081c75accf3350a3db7b84706c3e6a18333b6f64ee22df2e74845`.
There are zero new public/native calls, production edits or suite runs in
this archive step. Nullable auxiliary fields, computed performance output,
positive AND maps, carrier behavior and full Petri fields remain open.
PLUSPLUS stays partial; all 228 open coverage units remain in scope.

### PLUSPLUS filtered performance returned-map observations

Fourteen additional actual public EventLog HeuristicsNet calls form seven
direct/parent pairs with all nine parameters explicit. Their typed DFG,
performance DFG and performance matrix values agree in every pair. Minimum
activity and frequency filters remove retained interval relations from all
three maps. Dependency exclusion removes edges while retaining the maps, and
entirely filtered performance results retain present empty dictionaries.

At minimum frequency five, BD remains with interval count five and mean
performance `1.6666666666666667`, computed from three direct waits, 2/2/1.
In a non-neighbor interval control, the frequency DFG contains AC=1 and BC=1,
the performance DFG contains only BC=1.0, and the matrix contains AC=0.0 and
BC=1.0. For these inputs, performance output uses retained interval relations
with direct observations, while the matrix also retains a zero for a relation
without a direct wait. This is separate computed output; an unset aggregated
`performance_values` input cannot stand in for it. Additional native
aggregations and missing or invalid timestamp behavior remain unproven.

The actual worker exits zero with fourteen results, no reference or
serialization errors, and no input mutations. The enclosing capture exits one
after results and process records are saved because its empty-stderr assertion
rejects a Matplotlib cache diagnostic. Read-only finalization verifies the
exact saved bytes without rerunning the calls. An earlier preparation failure
before worker creation or public imports remains separate. Frozen compiled
inputs and current working inputs have independent exact byte guards;
line-ending differences are not normalized into asserted source equivalence.

`docs/heuristic-plusplus-filtered-performance-checks.jsonl.gz` and its validation
preserve 726 members, including immutable UTF-8 snapshots of all 650 working-source
files, distinct compiled-source bindings, raw results,
original logs, failures, helpers and finalization proof. The prior 23 helper
checks and subsequent 29 capture, 21 finalizer, and 22 saver checks are retained.
Every member byte, SHA and count roundtrips with the unchanged reviewed codec.
The archive is 4,900,615 bytes, SHA
`33d4916f883a7a7b01f6f19f7ed5469816daec3971a8ce2c1b51f436faad17cc`;
validation SHA is
`00895181c3574ca8e1f838e27e1b70a66d5882e14fe89b5e2d46c5617494b884`.
Saving adds zero public/native calls, production edits or suite runs. Native
computed output, full returned objects, carrier behavior and complete Petri
acceptance remain open. PLUSPLUS stays partial; all 228 open units remain.

### PLUSPLUS raw concurrency and positive AND projections

The next comparison reuses the same complete native capture and 119 public
HeuristicsNet aliases. Across 4,573 cells it records 4,023 matches, 117
ambiguous cells, 433 unrepresentable cells, and zero mismatches. Raw-name
concurrency matches every one of the 117 supported aliases, including paired
controls that retain BC=4 after activity C is filtered from active nodes.
Retained-ID concurrency remains separately captured and is not substituted
for this raw public map.

Twenty-five positive nested AND maps match actual native rows after preserving
the center activity, input/output side, canonical raw-name neighbor pairs and
floating scores. Observed scores include 1.0, 0.8 and 0.5. Duplicate or malformed
rows and missing backing cannot pass. Threshold equality and positive-map
label collisions remain unproven. The 117 remaining ambiguous cells are
computed performance matrices in the earlier native capture; nullable
auxiliaries and computed performance DFG remain unrepresentable there.
The independent actual 75-alias Petri diagnostic is reused explicitly; pending
Petri entries in this HeuristicsNet report do not count as new comparisons.

`docs/heuristic-plusplus-broad-raw-projection-checks.jsonl.gz` and its validation
preserve fourteen exact UTF-8 members: complete actual report, preparation,
manifest, attempt, frozen helper, and named fixtures/check reports. Twenty-eight
projection checks and seventeen saver checks pass. Exact bytes, SHA and count
roundtrip with the unchanged reviewed codec. Earlier corrected and original
reports, public results and compiled inputs remain immutable external evidence.
Report SHA is
`d3bba5fbf048bda53f867e6ea6c6982fb49c1a19dabc7aa3e41b5ce7018945af`.
The archive is 1,515,060 bytes, SHA
`3a1b026b994de7113c514496d96b443466d09036f57968589051303149cadad4`;
validation SHA is
`e0aa1525ff66fe03873ba04b4e64c416f8a4e2377ac87e1a3da232dd72e7460b`.
The comparison and save add zero public/native calls, production changes or
suite runs. PLUSPLUS remains partial, with all 228 open units retained.

### PLUSPLUS computed output presence and tested native maps

PLUSPLUS discovery now has independent auxiliary-field presence flags and
separate computed mean performance maps. It marks window-two and triple
outputs absent without changing their backing maps or CLASSIC flags.
Frequency output leaves computed performance DFG unset and gives every
retained interval relation a floating zero in the matrix. Performance output
engages its DFG before filtering, including an empty result, retains direct-wait
means for eligible interval pairs, and also records matrix zero for a retained
pair with no direct observation. Dependency edge exclusion retains both maps.
The selected edge aggregation and supplied `performance_values` input remain
separate from these computed means.

The new `plusplus_public_fields` JSON block preserves null versus concrete
empty maps, key/value pair arrays, nested raw-name matrix keys, floating numeric
tokens, signed zero and escaped labels. Existing nonfinite performance-value
validation applies before output. Literal regressions cover mean five,
interval count five against three direct samples with mean five-thirds,
activity/frequency filters, entirely filtered maps, EFG-only relations,
dependency exclusion, median edge aggregation independence, input separation,
legacy/CLASSIC auxiliary presence, and export edge cases.

A fresh build passes the core, heuristic variants, Petri conversion and Petri
reduction Release suites with assertions enabled. Canonical source tree is
`2b270cf8a692e15b870803a14d2e8d139a05b4a1`; exact exported source tree is
`e54fa844adf5875c87a5348080204e5a1c08af1d`. Actual configure, build and test
commands exit zero, finish their logs, leave zero active job processes, and
preserve all source and binary guards. Core SHA is
`66c05425798fe7a57bdf779d07a1dbf040a0802d606eb79ea2da2e2a3accda59`;
actual build-manifest SHA is
`ff6034b3802fd4cfe14d3d9bb355a9e5fbcf02d42a019b40f48084150647c4cb`.
Each command receives an independent snapshot of its bindings, so its embedded
record exactly equals its preserved disk record. No metadata alias exception
is needed for this new build.

`docs/heuristic-plusplus-computed-fields-build-checks.jsonl.gz` and its validation
preserve 56 exact UTF-8 members and bind 1,320 immutable external artifacts,
including all 650 compiled inputs and six actual binaries. Fourteen source,
fifteen builder and twenty saver checks are retained. Exact byte, SHA and count
roundtrip with the unchanged reviewed codec. The archive is 1,240,484 bytes,
SHA `b37af7a58f25b1ef82a70fd0ee78b1bf38b126f9222246bdf19948018cbad10b`;
validation SHA is
`52afe0d8a3004ab90991e92aaa93e49f9eda0070896b41c333bb2c8bbfd91ae0`.
Archiving adds no build/test commands or public/native calls. Fresh broad
returned-object comparisons, carrier behavior and complete Petri fields
remain open. PLUSPLUS stays partial and all 228 open units remain in scope.

### PLUSPLUS escaped-label AND boundaries and public Petri controls

Sixteen additional actual public calls complete successfully with unchanged
inputs: twelve HeuristicsNet calls form six direct/parent pairs and four Petri
calls form two pairs. All seventeen typed HeuristicsNet fields and canonical
snapshots agree within every pair. The six controls preserve the same event
data and non-AND fields while varying only the explicit AND threshold. Labels
include `__proto__`, `constructor`, `toString`, and a label containing a quote,
backslash, newline and Omega.

At the floating predecessor of 1.0, the score-1.0 outgoing AND map is present;
at 1.0 and its successor it is absent. At the predecessor of 0.5, both that
outgoing map and a score-0.5 incoming map are present; at 0.5 and its successor
only the outgoing map remains. These are finite returned-map observations,
including positive maps under the escaped labels. The earlier ordinary-label
series already preserves the corresponding score-1.0 boundary in public rows
76/78, 80/82 and 84/86 and native cases 21/22/23. Its output bundles also change
across the boundary. The earlier gate audit cited the CLASSIC path; PLUSPLUS
uses the separate strict comparison in `plus_bundles`. These observations
supersede the earlier statement that ordinary threshold equality lacked a
controlled observation, without establishing every threshold or carrier.

The renamed below-0.5 Petri pair has six places, four transitions and ten arcs.
The selfloop-input pair has three places, two transitions and four arcs, forming
`source0 -> A -> pre_B -> B -> sink0` without an explicit A-to-A relation.
Both pairs agree in full public fields and sparse markings. All twenty-eight
returned arc objects have unique identities and valid listed endpoints,
integer weight one and present empty properties. Net and node properties are
also present empty dictionaries; names and labels are preserved. Each marking
contains one explicit integer token at source0 or sink0 and no explicit zero
entries. The selfloop inputs exactly reuse supplement HeuristicsNet rows 40/41;
saved native discovery case 47 accepts only A-to-B and previously had zero
conversion calls. This public pair is not a new native conversion comparison.

The worker exits zero, finalizes its logs with 255 stdout bytes and zero stderr
bytes, and leaves zero active job processes. All eleven opaque installed-module
hash proofs match the pinned reference. Original 34 capture checks, eight
private-cache writability repair checks and nineteen saver checks are retained.
`docs/heuristic-plusplus-and-label-checks.jsonl.gz` and its validation preserve
695 exact UTF-8 members and 1,319 immutable external bindings, including 650
independently frozen working-source files and the distinct compiled context.
Every member byte, SHA and count roundtrips with the unchanged reviewed codec.
The archive is 5,237,763 bytes, SHA
`ad3c9badfa7e776496cd9aaa3b1813feed3eb0cec06c6094892f23ba9a8c7acf`;
validation SHA is
`1f7f8e168d59893fcb947fcb7e82f11d12e7d7f42ad1e09d2b12464b113a3dce`.
Saving adds zero public/native calls, comparisons, production changes or suite
runs. The public file adds sixteen aliases and six threshold identities;
combined native eligibility and conversion membership remain unassigned.
Full native returned-object and Petri acceptance remain open. PLUSPLUS stays
partial, with all 228 open units retained.

### PLUSPLUS fresh computed returned-object comparisons

A fresh capture against the tested computed-field build completes 77 actual
discoveries and 43 conversions in 77 probe processes and 79 commands, with zero
public calls and no native discovery or conversion errors. Its 208 aliases map
to 78 cases, with 77 eligible cases and the unfamiliar-decoration case 58 still
unrepresentable. Complete process records, logs, call markers, generated inputs,
raw results and exact byte guards retain the frozen 650-source/six-binary build
context. Native output SHA is
`ded869537a522515d533364f5bbb880660ac152c12cdbb755dc1866c4037b810`;
actual execution-manifest SHA is
`14b9ad0a80d32f545f0588192c81a26df0fdafe479c6f90c05753693ce213775`.

The fresh HeuristicsNet report compares 133 public aliases and records 5,041
matched cells, 82 unrepresentable cells, zero ambiguous cells and zero
mismatches. All supported aliases agree in their captured typed fields,
including computed performance output and independent auxiliary presence.
The 82 remaining cells are 41 fields each for supplement rows 68/69, whose
explicit decoration value is `supplement-unfamiliar`; no C++ call was made for
that case. The finite returned objects normalize it to performance mode, with
floating performance means and edge representation values. Native enum-only
input representation cannot yet express the requested string. Report SHA is
`1828e2a55da2cc1d06c988f06c47dc1a1f635429c819c8a9d1da2b5e5c9fe140`.

The independent fresh Petri report actually compares the same 75 public aliases
and reproduces 69 matching projected topologies and six differences. Its full
diagnostic categories are 69 unrepresentable objects, three reference errors
against native success, and three structural mismatches. Four public input
mutations and zero native mutations remain explicit. Names, properties and
arc-object/weight storage are still absent from the native carrier; matching
projected topology does not satisfy full Petri acceptance. Report SHA is
`bc05dbffe3d76d653f059ee145d8f749f4fbc7da4c9c1b42b796e25bf4f51347`.
Both report steps make zero new public/native calls or suite runs. The separate
sixteen public AND/Petri controls retain unassigned native mapping. PLUSPLUS
remains partial, with all 228 open units retained.

`docs/heuristic-plusplus-computed-object-checks.jsonl.gz` and its validation
preserve 2,375 exact members and 1,329 immutable external bindings, including
the original reviewed Petri matcher underlying the unchanged-definition proof.
All member bytes, SHA values and counts roundtrip with the unchanged codec.
The archive is 57,270,385 bytes, SHA
`0dcab5665180a22adee76ab69d573dbc8e45da3b28cb5e79889ace7c82b9e1b2`;
validation SHA is
`57612ea208a172997e46c4263bbec24adb492e796ebd542a80035595ed69e0ed`.
Archiving adds zero calls, comparisons or suite runs. Existing build, public
capture, static checks and historical failure evidence remain separately
bound with their original outcomes; no catalogue status is promoted.

### PLUSPLUS public carrier and converter route observations

Ten actual public calls complete across five independent branches using the
same typed sixteen-row DataFrame and all nine explicit baseline parameters.
DataFrame parent HeuristicsNet discovery followed by the public HeuristicsNet
converter returns five places, four transitions and nine arcs. DataFrame
parent `apply(PLUSPLUS)` returns four places, four transitions and eight arcs,
exactly matching the saved baseline public result. After public `TO_EVENT_LOG`
conversion, direct HeuristicsNet discovery plus conversion, direct PLUSPLUS
`apply`, and parent `apply(PLUSPLUS)` all return the same full five/four/nine
public Petri object structure. The HeuristicsNet fields agree in all seventeen
captured fields between the DataFrame and EventLog discoveries.

All three converted EventLogs preserve four traces and sixteen events, their
order, activities, both timestamps and ancillary values. The DataFrame case
field becomes the trace concept name; its attrs become EventLog properties.
Only the DataFrame parent `apply` branch mutates a supplied input, adding a
float64 `@@diff` column. Parameters and constructors remain unchanged in every
call. Newly returned HeuristicsNet and EventLog objects are preserved separately
from mutation of pre-existing inputs.

The five/four/nine results preserve place names `pre_B`, `pre_C`, `pre_D`,
`sink0`, `source0`; the four/four/eight route instead includes `intplace_A` and
`pre_D` with source and sink. All returned net/node/arc properties are present
empty dictionaries; transitions retain names and labels A through D, arcs have
explicit integer weight one, and sparse markings contain source0=1 and sink0=1.
The five/four/nine topology and marking projection agree with saved native case
33, whose carrier still lacks full public names, properties and arc fields.
The finite fixture localizes the differing output to DataFrame parent `apply`;
it does not establish an implementation cause or a universal converter rule.

The actual worker exits zero, finalizes 263 stdout bytes and zero stderr bytes,
and leaves zero active job processes. All eleven opaque module proofs match
the pinned reference. The wrapper completion manifest and separate before-call
API signatures/enums snapshot are retained with their exact links and guards.
`docs/heuristic-plusplus-carrier-converter-checks.jsonl.gz` and its validation
preserve 707 exact UTF-8 members and 1,325 immutable external bindings, including
650 independently frozen current sources and the distinct compiled context.
Original 23 capture checks, eleven mutation-repair checks, 23 saver checks and
seventeen saver repair checks remain with their exact origins. All member byte,
SHA and count checks roundtrip with the unchanged reviewed codec. The archive
is 5,272,193 bytes, SHA
`ef459af63bd2c607c1a60d427db90f2bb908f9eb294a245eefede24927a8b94a`;
validation SHA is
`02e0ddca341fb4bc2d0306568a1d3854fc46bb54e3dbb45442ed64ccdbc1caff`.
Saving adds zero public/native calls, comparisons, builds or suite runs.
PLUSPLUS stays partial and all 228 open units remain in scope.

### PLUSPLUS explicit selector and focused native controls

Source checkpoint `8864276` adds an optional string decoration selector at the
end of `HeuristicMinerOptions`. Omission preserves the enum API; explicit
`frequency` selects frequency, and other explicit strings select performance
within PLUSPLUS. CLASSIC ignores this selector. The saved unfamiliar string
returns normalized performance mode while input snapshots preserve its raw
value. All four required Release suites pass with assertions enabled; the
separate decoration build archive preserves that tested source and execution.

A fresh focused capture completes eight actual native discoveries and two
conversions in eight processes and ten command records, with zero public
calls. Eighteen saved public aliases map to eight inputs: supplement unfamiliar
selector rows 68/69 and the sixteen AND boundary, escaped-label and self-loop
controls. All 718 HeuristicsNet cells across fourteen aliases match. This
resolves the two unfamiliar-selector aliases that the earlier enum-only native
carrier could not represent. Net report SHA is
`3bfcdbcc63563c6aff74174e2679e651f5ad12d3f53463402cfb64dff3d0dac7`.

All four Petri aliases match projected topology and markings. The renamed AND
pair has six places, four transitions and ten arcs; the AAB self-loop pair has
three places, two transitions and four arcs. Native full names, properties,
explicit arc weights and sparse marking objects remain unrepresented, so full
Petri parity is false. Petri report SHA is
`f30248e9c3dcddee0927deb71017533fc660009f0c1ebe9346a9e867e00ee5b9`.

The first capture attempt fails configure because a relative preparation path
is resolved from the private child directory. Its actual exit-one record,
finalized empty stdout and 308-byte stderr remain retained; it makes zero
build, discovery or conversion calls. The repaired CLI resolves incoming paths
before dispatch, with eleven focused checks and the frozen original 34-check
proof. Fresh capture and both comparisons preserve independent provenance;
comparisons add zero calls, builds or suite runs. Other selector domains and
full carrier/Petri gates remain open. PLUSPLUS stays partial, retaining all
228 open coverage units.

`docs/heuristic-plusplus-decoration-object-checks.jsonl.gz` and its validation
preserve 817 exact UTF-8 members, 650 compiled inputs, six actual binaries,
original failed-attempt records, helper/check origins and saved public capture
provenance, including the complete AND-label archive. Original 32 saver checks and twelve repair checks
retain their distinct byte origins. All member byte, SHA and count checks
roundtrip with the unchanged reviewed codec. The archive is 8,566,703 bytes,
SHA `e8fefcbfca2f80fbc1ea7e047c1b8178be34b43fd0e72d69c4c02977e3bf4505`;
validation SHA is
`8e2918f9fae87afe09f5e3583811570dce5d7ba07ac748d63d23145e5e62bd6b`.
Saving adds zero comparisons, calls, builds or suite runs. Post-commit guards
bind immutable saved files, preserving validation after later source and HEAD
changes. No catalogue status is promoted.

### PLUSPLUS converter parameter forwarding contrast

Four actual public calls compare two fresh copies of the saved sixteen-row
DataFrame. Each branch performs parent HeuristicsNet discovery with explicit
PLUSPLUS and the same nine parameters. Its public HeuristicsNet conversion
then omits converter parameters in branch A or passes an independent copy of
all nine parameters in branch F. Both retain the converter's default variant.
Fresh DataFrames, returned HeuristicsNets and option copies keep the parameter
contrast separate from prior-call mutations.

All four calls succeed, with zero reference/serialization errors and zero
skipped dependents. Both HeuristicsNets expose seventeen equal typed fields,
matching the earlier branch A. Both full public Petri objects also match each
other and the earlier branch A: five places, four transitions and nine arcs,
including names, labels, empty property dictionaries, explicit unit arc
weights and sparse markings. Existing inputs and options remain unchanged;
newly returned HeuristicsNets are recorded separately. This finite contrast
shows the same result with omitted and explicit converter parameters; the
four-place DataFrame parent `apply` result remains unexplained.

The actual worker exits zero, finalizes its logs and leaves zero active job
processes. Six public signatures and eleven unchanged opaque module proofs
are retained before calls. Raw output SHA is
`c5dfc964eb557725fadec4d58e86ea997f08128535a577b0d9994284bd0f749b`;
actual execution-record SHA is
`8b830fae2d038c7f50bde0b850a6b81edaf98dc080455cfaf765e6030b8428eb`.

`docs/heuristic-plusplus-converter-parameter-checks.jsonl.gz` and its validation
preserve 776 exact UTF-8 members, current capture records/objects/API proofs,
original capture proof origins, forty focused capture checks and 22 saver
checks. The full earlier carrier archive and validation remain immutable
origins. All member bytes, SHA values and counts roundtrip with the unchanged
reviewed codec. The archive is 6,642,407 bytes, SHA
`d183a084bdbd20684a6964eb7fe05bebd7c0ff988053cfc1e7c910b81ca16697`;
validation SHA is
`443b4108d8ebf16e9582b28050a4c8491606841f48f784fb22352b4fd8b31d6d`.
The independent compiled context retains 650 inputs, six binaries and its four
successful suites. Saving adds zero calls, builds or suite runs. Immutable
saved-file guards allow later source changes. Full native Petri/carrier gates
remain open; PLUSPLUS stays partial with all 228 open coverage units retained.

### PLUSPLUS DataFrame and EventLog threshold response

Seven actual public calls compare explicit PLUSPLUS parent `apply` using
fresh copies of the saved sixteen-row DataFrame and independent copies of its
public `TO_EVENT_LOG` conversion. Three calls per carrier use AND thresholds
zero, 0.5 and one; the other eight parameters remain fixed. The conversion
accounts for the seventh call. Every call succeeds, with zero errors or skipped
dependents.

| Carrier | AND threshold | Places | Transitions | Arcs |
| --- | --- | --- | --- | --- |
| DataFrame | 0 | 6 | 4 | 10 |
| EventLog | 0 | 6 | 4 | 10 |
| DataFrame | 0.5 | 4 | 4 | 8 |
| EventLog | 0.5 | 5 | 4 | 9 |
| DataFrame | 1 | 4 | 4 | 8 |
| EventLog | 1 | 4 | 4 | 8 |

The full typed public Petri views match between carriers at zero and one.
The DataFrame view at 0.5 matches its view at one, while the EventLog view at
0.5 differs from both. The two 0.5 results reproduce the earlier carrier
baselines, including input fields, options and full returned objects. This
finite comparison localizes the observed contrast to the middle threshold;
it does not establish an implementation cause or a general carrier rule.

Each discovery mutates its supplied DataFrame by adding `@@diff`. The separate
converter DataFrame and three independent EventLog copies remain unchanged,
as do all option dictionaries. The copies preserve the converted trace and
event fields while remaining independent objects. Creation and input mutation
records remain distinct.

The worker exits zero with finalized logs and zero active job processes.
Public signatures and eleven unchanged opaque module proofs precede the calls.
Raw output SHA is
`3c5063e4801f5695e1dda90d146fe94b418bcfa9987c21f32fb0d3be694eb6d9`;
execution-record SHA is
`345f9d9b3821c37d2ad21727cce31cc0d1206c80ea91632734f8e8f2454dbd5c`.
Native carrier routing and full Petri object gates remain open. PLUSPLUS stays
partial, retaining all 228 open coverage units.

`docs/heuristic-plusplus-dataframe-and-route-checks.jsonl.gz` and its validation
preserve 793 exact UTF-8 members, all seven calls, typed returned objects,
copy and mutation records, signatures and opaque proofs. The earlier carrier
and converter archives remain complete immutable origins. Thirty-nine capture
checks, the original 31 saver checks and five repair checks retain distinct
byte origins. The repair corrects the collector's check count and fixture
filename; its mocked full-collector checks cover both original failures and
the repaired success path.

All member bytes, SHA values and counts roundtrip with the reviewed codec.
The archive is 9,198,372 bytes, SHA
`8c9c402f7968eddf8b71ae33436e516070bf341b77c53e0b3ce704d208eb705e`;
validation SHA is
`1f75d75255ec45b85a0b5c395e101fed426e7ee0dd4b3184595e0b77214e4028`.
The independent compiled context retains 650 inputs, six binaries and four
passing assertion-enabled suites. Saving adds zero calls, builds or suite
runs. Immutable saved-file guards permit later source and HEAD changes.

### PLUSPLUS Petri naming controls (2026-10-02)

The pinned 2.7.22 public worker made twelve top-level calls in one process:
nine parent PLUSPLUS Petri discoveries, one parent heuristics-net discovery,
and two public converter calls reusing that returned HN. Both converter kwargs
were omitted. Each discovery used a fresh EventLog and full nine-key options,
with sequential non-overlapping event intervals and explicit case IDs. The
fixtures cover a path, split, join, both-sided branching, repeated split/join,
reordered branches, reserved/escaped labels, HN reuse, and the negative minimum
subnormal dependency threshold on ABA.

All twelve calls succeeded without reference or serialization errors or skips.
The eleven Petri snapshots retain typed names, nullable labels, properties,
endpoint identities, every weighted arc occurrence, and sparse markings. Fresh
log/trace/event and option identities were checked. Existing EventLogs, options,
and the HN reused by both conversions remained unchanged; the newly returned HN
is accounted for as output creation.

The two fresh split/join returns (4/5), the reordered return (4/6), and the two
conversions of HN8 (9/10) have equal complete portable public Petri field views.
The reserved/escaped relabeling (7) returns the corresponding changed fields.
The path returns `pre_B` and `pre_C`; the split returns `intplace_A`, the join
`pre_C`, and the both-sided fixture `pre_B` and `intplace_B`. The split/join
fixture returns `intplace_A` and `pre_D`, alongside `source0` and `sink0`.
The ABA control returns visible A/B and silent names `hid_2` and `hid_4`.
These are finite public observations. The surviving names do not establish a
general counter, creation-order, reduction-survivor, or alias naming policy.
Every observed arc has integer weight one; none repeats an endpoint pair.
Those absences do not close repeated-arc or nonunit-weight domains.

Capture helper SHA is
`5c778d7f50fee046e3708ce3544872dcf0541faaca62cdaa6652c9bd95f90357`;
its 43 focused mocked checks cover the collection body and failure/skip paths.
Preparation SHA is
`014352d38b7a722e7be612595bf0e6f139b2793beffb6eeea85c5518e435997a`;
raw output SHA is
`06ff9c70931e322b6b80fbfbc4d6ce43f2b1a20cb525de32b4d39053b4080c47`.
Execution record SHA is
`63803433e59491524acc762a34ba717ece4df5ace8121481b9c3b3174c40043f`.
PID 31284 exits zero with finalized logs and zero active job processes. Public
signatures and eleven unchanged opaque module proofs precede the calls.

`docs/heuristic-plusplus-petri-naming-checks.jsonl.gz` and its validation retain
827 exact UTF-8 members, the twelve full call/input/output records, signatures,
opaque proofs, logs, and complete earlier helper/report/fixture origins. The
25 saver checks exercise the actual mocked collector, including exact capture
check count and fixture filename, record/plan/input/origin drift rejection.
The earlier route saver's original 31 checks and five repair checks remain
separate whole origins without being rerun.

Every member byte, SHA and count roundtrips with the reviewed codec. The archive
is 8,602,002 bytes, SHA
`472ea0a39ea1d3372f89862c8c6148432f06c30f6840b43c953b5611df2e94db`;
validation SHA is
`2c51cee74d319b1742bf23a304a20bbc52699ab3a39d7567f465442473f8de81`.
The immutable presentation build context retains 650 inputs, seven binaries and
five passing assertion-enabled suites. Saving adds zero public/native calls,
builds or suite runs. Durable guards use immutable BUILD/DOCS paths and permit
later source and HEAD changes. Automatic native names and full returned-object
parity remain open. PLUSPLUS stays partial, retaining all 228 open units.

### Automatic PLUSPLUS Petri presentation (2026-10-02)

Added `minera::to_petri_net_presentation(const HeuristicsNet&)` and seventeen
registered tests: fifteen public graph fixtures, non-PLUSPLUS rejection, and
invalid active-index rejection. The additive producer retains names, nullable
labels, node identity, properties, unit arc occurrences, and sparse markings.
All incoming joins precede outgoing splits; silent numbering includes removed
transitions. This matches the surviving `hid_7`/`hid_8` collision control.

Actual production replay now matches all 69 previously matching broad Petri
topologies on every supported presented field, plus thirteen focused naming,
escaped-label AND, and self-loop controls. Three datetime DataFrame shape
mismatches and three object-dtype public errors against native success remain
open. No comparison is accepted on topology alone or by merging equal names.

A separate replay of all 77 broad inputs against the parent production library
confirms identical discoveries, generic Petri conversion, and input snapshots.
The six differences from the older computed-field capture were already present
at the parent build. The accepted runs contain 167 native probe processes,
154 discovery calls, 86 generic conversions, and 56 automatic presentations;
they reuse pinned public captures and make zero new public calls.

The initial fixture build exposed an ambiguous `Trace` alias. Compact interval
fixtures then failed a transition-count assertion. The final collision tests
use captured converter-relevant native fields, while the broad replay retains
original typed EventLogs and their metadata. Production inputs and compiled
artifacts stayed identical for the final test repair. The changed test unit was
recompiled, and core, request_params, heuristic_variants, petri_net_conversion,
and petri_net_reduction all passed with assertions enabled in Release.

Evidence is saved in `heuristic-plusplus-petri-producer-checks.jsonl.gz`
(SHA-256 `f4b335d4e0ffdb99636ea228d477a2c5f406bd6d4248a47471f94b6a505f0f55`,
4394149 bytes, 389 records), with
`heuristic-plusplus-petri-producer-validation.json` (SHA-256
`9914af8595e9f8a98bd4d955f0997a1bc78fbeb331229fdabd867db116c5410a`).
The archive round-trip was verified. Broader silent-counter, parking-name,
metadata-survivor, carrier, error, and full module gates remain open. No
catalogue entries were promoted: coverage stays 46/86 packages and 199/387
modules, with 228 incomplete units.

### PLUSPLUS pandas entry-point contrasts (2026-10-02)

Executed 25 public carrier/variant contrasts and eight public pandas-helper
controls against pinned PM4Py 2.7.22. Both workers completed with their child
processes finalized. The 33 calls retain 29 successes, four actual
AttributeErrors, no serializer errors, and no skips. There are zero native
calls and no production changes in this checkpoint.

Parent DataFrame PLUSPLUS Petri output and full mutation snapshots match the
exported `apply_pandas` helper at AND thresholds 0/0.5/1. Direct DataFrame
PLUSPLUS Petri output matches the converted EventLog route at 0.5. Parent/direct
PLUSPLUS HeuristicsNet calls match all seventeen raw fields across those
carriers. Eight complete public Petri comparisons match, while two deliberate
route/variant comparisons retain their full-field mismatches.

The pandas helper exposes the missing carrier distinction: B/C concurrency
is 2 rather than 4, and its AND scores are 0.5/0.25 at threshold zero. The 0.5
output score is excluded at threshold equality. DFG counts match, with a
separate raw dictionary insertion-order difference. The pandas route adds
float64 `@@diff` durations and requires datetimelike timestamp columns.
Object-dtype parent PLUSPLUS/pandas errors differ from successful direct
PLUSPLUS calls; CLASSIC parent/direct object-dtype behavior also differs.
These observations define native pandas discovery, mutation, error, and route
work still required. They do not prove internal dispatch or a universal
concurrency-halving rule.

Evidence is `heuristic-pandas-dispatch-checks.jsonl.gz` (SHA-256
`bbc8cbe45fa4765c269a159ef8f04e374370dacdc9738b519fdd2ab0c8184f08`, 645546 bytes,
30 records) and `heuristic-pandas-dispatch-validation.json`
(SHA-256 `c1cf86c09f49a16a590155a8bdb5dfca1802c043b186fcf9de3b5b5f276fb65a`). All archived byte members and the gzip
round-trip were checked. Coverage remains 46/86 packages and 199/387 modules,
with 228 incomplete units. PLUSPLUS and the full target remain partial.

### Native PLUSPLUS pandas route and regression replay (2026-10-02)

Added the mutable `discover_plus_plus_pandas` entry point for the public pandas
concurrency projection, missing start-column fallback, `@@diff` durations and
captured dtype error category/message. Fourteen new public orientation and start
controls distinguish its behavior from generic table discovery. All five relevant
native suites pass with assertions enabled from 650 frozen source inputs.

Actual native replay covers 34 route aliases: nineteen returned HeuristicsNets
match 995 supported field cells across all seventeen net projections and their
nodes/edges; twelve complete supported Petri presentations match; and three dtype
errors match. Supported mutation snapshots and option preservation also match.
The 77 generic regression cases preserve prior native net, Petri, input and error
results exactly. Replay executes 111 discoveries, 12 presented conversions and
43 generic conversions without new reference calls. Full raw snapshots, public
process/proof records, native logs, comparisons, source inputs and nine binary
hash bindings are retained in `heuristic-plusplus-pandas-native-checks.jsonl.gz`
(SHA-256 `0ca044fc58b2084ae49e77cb38ecf63e5908c3362a865e3973c12812a45df696`) and
`heuristic-plusplus-pandas-native-validation.json`; all 1,076 archive members
round-trip successfully.

Pandas timestamp/null/error breadth, object identity, dictionary iteration order,
unscoped metadata and adapter routing remain open. This verified implementation
step does not promote catalogue units or complete PLUSPLUS. Overall coverage stays
46/86 packages and 199/387 modules, with 228 open units.

### PLUSPLUS pandas timestamp boundaries and remaining gaps (2026-10-02)

Twenty actual public calls and twenty actual native discoveries compare touching
endpoints, equal starts, zero durations, NaT, missing object start columns, stale
`@@diff`, reversed rows and microsecond/nanosecond precision. The public worker
returns eighteen objects and two dtype errors, with no skips or serializer
failures and zero remaining child processes. Native replay makes seven Petri
presentations. There are 363 matched field cells, two sojourn mismatches and 106
unavailable cells after native NaT rejection. Seven full supported Petri
presentations match; two are unavailable after those discovery errors.

Measured gaps are NaT acceptance, start-column mutation before an object dtype
error, microsecond sojourn precision, and nanosecond sojourn/duration precision.
The NaT probe's fallback exception category is emitted as a boolean; its actual
exception presence and message are retained without claiming a concrete category.
Raw public/native inputs, outputs, errors, mutations, opaque proofs, collectors,
probe sources, process records, logs and binary/source evidence bindings are saved
in `heuristic-pandas-timestamp-boundary-checks.jsonl.gz` (SHA-256
`41fbe4d8bd85b65089eab10ac1c24352863fff7939c4b3f90dd4a231b98e58be`) and `heuristic-pandas-timestamp-boundary-validation.json`.
All 100 members round-trip. The tested production source remains `63ccbc6`; this
checkpoint changes only evidence and documentation. No catalogue unit is promoted:
46/86 packages and 199/387 modules remain complete, with 228 open units.

### Native PLUSPLUS pandas NaT and timestamp precision (2026-10-02)

Implemented the four measured pandas timestamp gaps: partial NaT acceptance with
nonmissing sojourn means, start-column copying before the object-dtype error,
exact microsecond durations and sojourns, and nanosecond durations and sojourns.
The route also matches the captured all-missing datetime error/mutation order,
scalar performance-wait rounding, unequal occurrence ordering and raw repeated-
activity concurrency. Four new registered regressions cover these behaviors.

Forty-four new completed public calls produce 26 objects and 18 actual errors.
The initial own fixture failure retains sixteen begin markers and no complete
returned-object file; completed and successful call counts stay unknown. The
superseded native build and diagnostic replay retain the failing assertion and
four field mismatches that drove the final fixes. They are not accepted runs.

All five native suites pass with assertions enabled from 650 frozen inputs.
Actual replay covers 98 route aliases: 45 HeuristicsNet results have 2301
matched supported net/node/edge cells, six mismatches and nine extra-edge cells
unavailable. Forty-two HeuristicsNet results, 30 complete supported Petri
presentations and 23 errors match in full, including all ninety preceding aliases.
The three new 1/3/7 ns wait controls expose a concurrency precision gap that changes
dependency scores and accepted edges; their wait values match. All supported mutations and
option preservation match. The 77 generic regression cases preserve previous
net, Petri, input and error results exactly. Replay makes 175 discoveries, 30
presented conversions and 43 generic conversions with no new reference calls.

Evidence is `heuristic-plusplus-pandas-timestamps-checks.jsonl.gz` (SHA-256
`e9fa4fb240c629891540119107a2812de734b954da54884b77e171cfd0235feb`, 24089147 bytes, 1903 verified members) and
`heuristic-plusplus-pandas-timestamps-validation.json`. Full raw outputs, mutation
snapshots, opaque proofs, helpers, sources, comparisons, binary hashes and process
records are retained, including the superseded failures. Every archive member
round-trips. Broader pandas/PLUSPLUS and Alpha+ completion gates remain open;
coverage stays 46/86 packages and 199/387 modules, with 228 incomplete units.

### PLUSPLUS pandas concurrency rounding (2026-10-02)

Resolved the three nanosecond concurrency gaps recorded at `cc8bd1a`. Inclusive
overlap now uses epoch seconds rounded to six decimal places, preserving the
epoch-dependent boundary while exact ordering, eventually-follows, durations and
performance waits retain their existing precision. A registered regression checks
nine captured controls across concurrency, dependency scores, edges and waits.

Forty-nine new public calls return all 49 objects, including three Petri results,
without skips or serializer failures. They cover nanosecond gaps, fractional
completion, half-microsecond boundaries and epochs 1960/1970/2024/2100. The 2024
596/597 ns boundary and Unix-epoch 499/500 ns boundary distinguish the observed
rounding path. Worker logs and unchanged opaque proofs are retained.

All five native suites pass with assertions enabled from 650 frozen inputs.
Actual replay covers all 147 saved route aliases: 91 HeuristicsNets have 4514
matched net/node/edge cells and three sojourn mismatches. Eighty-eight HeuristicsNet
aliases, all 33 Petri presentations and 23 errors match in full; all 98 preceding
aliases now match. Connection domains, mutation occurrence and option preservation
match throughout. Three new cases expose a separate one-ULP duration and input-after
difference: public `1.000001` versus native `1.0000010000000001`. Those gaps remain
open. The 77 generic cases preserve prior
native results exactly. Replay makes 224 discoveries, 33 presented conversions
and 43 generic conversions with zero new reference calls.

Evidence is `heuristic-plusplus-pandas-concurrency-checks.jsonl.gz` (SHA-256
`8a9cde0e3f092b0bcc759e3dc32652a6a924d1fec681b0182a5eba0fab4625c8`, 21245608 bytes, 1587 round-trip-verified members), with
`heuristic-plusplus-pandas-concurrency-validation.json` (SHA-256 `9a8564df9a2ccc0ef1a5e12683d2f7a519ad4edec52016c56aab82fd8d59264a`).
The preceding archive remains hash-bound as parent evidence. Broader pandas,
PLUSPLUS and Alpha+ gates remain open; no catalogue units are promoted. Coverage
stays 46/86 packages and 199/387 modules, with 228 incomplete units.

### PLUSPLUS pandas signed duration components (2026-10-02)

Resolved the three duration-rounding and input-after gaps at `c1a6748`. Array
durations now combine floored seconds, microseconds and nanoseconds, preserving
the captured negative cancellation and millisecond-to-microsecond scaling.
Performance waits retain their nonpositive clamp. A registered regression checks
ten captured values in both sojourn projections and affected duration cells.

Forty-one new public calls return all 41 objects without skips or serializer
failures, including two Petri presentations. They cover positive, negative, zero
and day-spanning values, three declared datetime units, independent nanosecond
durations and millisecond scaling. Full returned values, mutations, API views,
unchanged opaque module proofs and terminal worker records are retained.

All five native suites pass with assertions enabled from 650 frozen inputs.
Actual replay matches all 188 route aliases: 130 HeuristicsNets match 6464
supported field cells and complete connection domains, 35 Petri presentations
match every supported field, and 23 errors match category/message. All supported
input-after projections, mutation occurrence and option preservation match. The
77 generic cases preserve prior native results exactly. Replay makes 265
discoveries, 35 presented conversions and 43 generic conversions with no new
reference calls during replay.

Evidence is `heuristic-plusplus-pandas-duration-checks.jsonl.gz` (SHA-256
`388c8cb1b8e542b446cb0673c726e4362f88725e7094b709729536c11f672dc8`, 26750742 bytes, 1782 verified members), with
`heuristic-plusplus-pandas-duration-validation.json` (SHA-256 `af419a1b1a1199171378359ce1e42278890a4e8e247d0c19bb2888ee8526e7be`).
Every archive member round-trips, and the preceding archive remains hash-bound
as parent evidence. Broader pandas/PLUSPLUS and Alpha+ gates remain open; coverage
stays 46/86 packages and 199/387 modules, with 228 incomplete units and no
catalogue promotion.

### PLUSPLUS pandas filter and threshold controls (2026-10-02)

Captured 66 successful public controls for activity/path count cutoffs, exact
dependency/AND boundaries and frequency/performance decoration. Fourteen Petri
calls include empty and partially filtered graphs. All calls retain complete raw
values and mutation snapshots, unchanged opaque proofs and terminal process logs,
with no skips or serialization errors.

Actual native replay matches all 52 HeuristicsNet aliases on 2,246 supported
net/node/edge cells and connection domains, plus all fourteen supported Petri
presentations. All supported input-after projections, mutation occurrence and
option preservation match. Replay makes 66 discoveries and fourteen presented
conversions with no additional reference calls.

The production source remains `e4693ec`. Its verified core, 650 compilation
inputs, five passing assertion-enabled suites and 77 generic regression results
are reused as parent evidence. Only the new replay probe is compiled; no new
production build, suite run or generic conversion is counted.

Evidence is `heuristic-plusplus-pandas-filter-checks.jsonl.gz` (SHA-256
`397cd79c365240103c42785b7bb22381e1a6320813f6b5c93b9ac106438dc86d`, 7358748 bytes, 322 verified members), with
`heuristic-plusplus-pandas-filter-validation.json`. All member bytes and hashes
round-trip. Broader parameter, carrier, error, metadata, identity and routing
requirements remain open. PLUSPLUS and Alpha+ remain partial; coverage stays
46/86 packages and 199/387 modules, with 228 incomplete units and no promotion.

### PLUSPLUS numeric count threshold implementation (2026-10-02)

Appended PLUSPLUS numeric count overrides and applied them consistently to
activity/path filters, reverse dependency counts and AND numerators. Fractional
cutoffs are not truncated, negative values retain counts and NaN rejects the
corresponding counts. Existing integer filters remain exact; CLASSIC behavior is
preserved. Both decoration modes are covered.

All 88 new successful public controls match: 76 HeuristicsNet results on
3,440 supported net/node/edge cells and complete connection domains,
plus twelve complete supported Petri presentations. The wider replay matches all
342 saved aliases on 12,150 net-field checks,
61 Petri presentations, 23 mapped errors and the supported mutation/option checks.
All 77 generic regression results are unchanged. Actual native execution makes
419 discoveries, 61 presented conversions and 43 generic conversions without
additional reference calls.

All five assertion-enabled suites pass for the accepted 650-input frozen build.
The initial failed test run and its source remain retained; its path-count
expectation was corrected from four to two for cutoffs between two and four.
Its prepared native replay was not executed or counted.

Evidence is `heuristic-plusplus-pandas-numeric-checks.jsonl.gz` (SHA-256
`da8b1d1cdf71df3e6bcd3b6762d0b9612c3094ca9e7da949746400b6317c350f`, 46779784 bytes, 3079 verified members), with
`heuristic-plusplus-pandas-numeric-validation.json`. All member bytes and hashes
round-trip. Broader parameter types, carriers, errors, metadata, identity and
routing requirements remain open. PLUSPLUS and Alpha+ stay partial; coverage
remains 46/86 packages and 199/387 modules, with 228 incomplete units.

### PLUSPLUS pandas literal selector implementation (2026-10-02)

Resolved four pandas selectors literally, including empty names, and matched the
observed case/activity/completion lookup order and distinct KeyError messages.
An absent start is copied after successful lookups; dtype and overlapping-role
errors retain their observed start-only mutation. An internal projection assigns
event roles without interpreting selected column names as generic native aliases.

All 96 new public aliases match: 22 HeuristicsNet results on 1,118
supported cells and complete connection domains, eight Petri presentations and
66 errors, with matching supported input-after, mutation and option checks.
The first sixty pre-change replays and their actual mismatches remain archived.
The wider replay matches all 438 aliases on 13,268
net-field checks, 69 Petri presentations and 89 errors; all 77 generic regressions
remain unchanged. Actual execution makes 515 discoveries, 69 presented conversions
and 43 generic conversions without additional reference calls. All five
assertion-enabled suites pass for the accepted 650-input source snapshot.

Evidence is `heuristic-plusplus-pandas-selector-checks.jsonl.gz` (SHA-256
`29c93c12e26dd093c0667ee811ccfbf0246bc46b4f32f627673deabbf4a146fa`, 55954295 bytes, 3052 verified members), with
`heuristic-plusplus-pandas-selector-validation.json`. Every member round-trips.
Broader parameter, carrier, error, metadata, identity, ordering and routing gates
remain open. PLUSPLUS and Alpha+ stay partial; coverage remains 46/86 packages and
199/387 modules, with 228 incomplete units and no promotion.

### PLUSPLUS pandas duration-column collision implementation (2026-10-03)

Matched errors after @@diff overwrites a selected timestamp, preserved the
observed successful NaT/empty collisions, and distinguished mixed object/datetime
subtraction errors before mutation. Both decorations and us/ns/ms units are
covered. Thirty pre-change errors and their original process records remain
retained as gap evidence.

All 66 new public controls match: twelve HeuristicsNet results on 516 supported
cells, four complete supported Petri presentations and fifty errors. Input-after,
mutation occurrence and option checks match. The wider replay matches all 504
aliases on 13784 net-field checks, 73 Petri presentations and 139 errors;
all 77 generic regressions are unchanged. Actual native execution makes 581
discoveries, 73 presented conversions and 43 generic conversions without new
reference calls. All five assertion-enabled suites pass for 650 exact inputs.

Evidence is `heuristic-plusplus-pandas-duration-selector-checks.jsonl.gz`
(SHA-256 `f3b620c52e16b94307d17174fdc47447847ffbf84d5d21bcce46a1aebf7d0ec5`, 47582491 bytes), with
`heuristic-plusplus-pandas-duration-selector-validation.json`. Every member
round-trips and the preceding selector checkpoint remains hash-bound. Broader
carrier, parameter, error, metadata, identity, ordering and routing gates remain
open. PLUSPLUS and Alpha+ stay partial; coverage remains 46/86 packages and
199/387 modules, with 228 incomplete units and no catalogue promotion.

### PLUSPLUS pandas timestamp tie and Petri counter ordering (2026-10-03)

Ordered equal-start intervals by completion time for eventual follows,
performance and concurrency. Assigned presented silent-transition counters by
target and predecessor relation traversal within the existing join-then-split
phases. Reversed, shuffled, renamed and unequal-orientation controls expose
both former gaps; their complete pre-change runs remain retained. A completed
intermediate replay with eight renamed-label Petri mismatches and the two
earlier failed test builds also remain reconstructible.

All 118 new successful public controls match: 92 HeuristicsNet results on 4738
supported checks and 26 complete supported Petri presentations, with matching
input-after, mutation and options. The wider replay matches all 622 aliases on
18522 net-field checks, 99 Petri presentations and 139 errors. All 77 generic
regressions are unchanged. Actual execution makes 699 discoveries, 99 presented
conversions and 43 generic conversions without additional reference calls.
All five assertion-enabled suites pass for 650 exact compilation inputs.

Evidence is `heuristic-plusplus-pandas-row-order-checks.jsonl.gz` (SHA-256
`a2b79908ded8b7022cf2fafe9b2b330225ed36a1c6d5f01e0843a9beb19711a5`, 54920511 bytes), with
`heuristic-plusplus-pandas-row-order-validation.json`. Every member round-trips;
the `heuristic-plusplus-pandas-row-order-before-checks.jsonl.gz` and
`heuristic-plusplus-pandas-row-order-superseded-checks.jsonl.gz` companions retain
the earlier runs. Validation binds all three segments and 6287
verified members. Both pre-change replays and the preceding checkpoint remain bound. Broader
parameter, carrier, error, metadata, identity, ordering and routing gates remain
open. PLUSPLUS and Alpha+ stay partial; coverage remains 46/86 packages and
199/387 modules, with 228 incomplete units and no promotion.

### PLUSPLUS pandas missing-case implementation (2026-10-03)

Pooled missing case scalars for interval relations while excluding those groups
from boundaries and performance waits. Preserved global activity counts,
sojourn values and original row ordering for frequency ties. Matched the
all-missing aware subtraction error before @@diff mutation and the successful
naive counterpart; copied-start mutation remains observable.

All 72 new public aliases match: 42 HeuristicsNets on 2322 supported checks,
eighteen supported Petri presentations and twelve errors, with matching
input-after, mutation and option checks. Both complete pre-change replays and
their differences remain retained. The wider replay matches 694 aliases on
20844 net-field checks, 117 Petri presentations and 151 errors; all 77 generic
regressions are unchanged. Actual replay execution makes 771 discoveries,
117 presentations and 43 generic conversions without additional reference calls.
Five assertion-enabled suites pass for 650 exact compilation inputs.

Evidence is `heuristic-plusplus-pandas-case-checks.jsonl.gz` (SHA-256
`77a0f23667f5909ad764a10513782423f300736e41b2a912f7fee027c3484f03`, 70230661 bytes), with
`heuristic-plusplus-pandas-case-validation.json`. Every member round-trips.
The earlier ordering checkpoint remains bound across its three archive segments.
The initial FTK1011 configure failure made no native calls and remains retained
alongside the successful retry at a shorter path. Broader type, error, metadata,
identity, ordering and routing gates remain open. PLUSPLUS and Alpha+ stay
partial; coverage remains 46/86 packages and 199/387 modules, with 228 incomplete
units and no promotion.

### PLUSPLUS pandas Unicode selector-error implementation (2026-10-03)

Matched Unicode 14 string escapes and nested lookup messages while preserving
printable labels, quote selection and lone-surrogate scalar identities. The
234-call public matrix reproduces 140 pre-change error mismatches. After the fix,
all 210 selector errors match; ordinary Unicode label and column controls match.
An independent exhaustive sweep matches all 1,114,112 single-code-point native
KeyError messages against Python builtins, making zero additional PM4Py calls.

The first capture's OWN serializer fails on two successful surrogate Petri
results. Its complete raw capture and baseline remain retained; the repaired
OWN serializer produces a fresh 234-call matrix without serialization failures.
The checkpoint counts all 468 new reference calls and retains 1,175 calls.

The full replay resolves 924 of 928 aliases: 436 full supported HN results,
127 Petri presentations and 361 errors. All input, mutation and option checks
match, and all 77 generic regressions remain unchanged. Four lone-surrogate
label aliases remain open, with seventeen actual net-field mismatches and
24 unrepresentable edge-field occurrences. The 21445 matching field checks
do not erase those gaps. Actual replay counts are 1,005 discoveries,
129 presentations and 43 generic conversions; the scalar sweep adds
1,114,112 native discoveries. Five assertion-enabled suites pass on 650 exact
compilation inputs, and all archive members round-trip.

Evidence is `heuristic-plusplus-pandas-unicode-checks.jsonl.gz` (SHA-256
`c02fddee386b7b5055c6102034c56e1cc8a55e92a0dece2d024672ba4c5f872b`, 103247692 bytes), with
`heuristic-plusplus-pandas-unicode-validation.json`. The preceding missing-case
checkpoint remains hash-bound. Surrogate grouping, further type, ordering,
metadata, identity and routing gates remain open. PLUSPLUS and Alpha+ remain
partial; coverage stays 46/86 packages and 199/387 modules, with 228 incomplete
units and no promotion.

### PLUSPLUS pandas surrogate-grouping implementation (2026-10-03)

Matched operation-specific surrogate grouping while retaining distinct caller
labels, activity counts and interval case identities. Raw-row case boundaries,
independent sojourn/EFG/wait/concurrency representatives, mixed case-key types,
category order and exact encoding-error mutation timing follow 152 initial
public controls and 50 recaptures with complete category declarations. Their
before/after inputs, errors and mutation flags agree after excluding only the
added metadata. The total is 202 new PM4Py calls and 384 separately counted
public pandas grouping diagnostics.

All 1,080 retained PLUSPLUS aliases now match: 492 full supported HN results,
183 Petri presentations and 405 errors. The 24972 assessed net/node/edge
fields, connection domains and all input, mutation and option checks match.
The previous four surrogate-label aliases are resolved. All 77 generic
regressions remain unchanged. Actual full replay counts are 1,157 discoveries,
183 presentations and 43 generic conversions; three retained pre-change replays
add 152 discoveries and 74 presentations. Their categorical projections remain
diagnostic; complete category carriers add 50 pre-change discoveries and
20 presentations. The retained initial full replay lacks category metadata
and adds 1,157 discoveries and 197 presentations; it is superseded by the
complete accepted replay. A repeated exhaustive control matches
all 1,114,112 native scalar KeyError messages against Python builtins, with zero
PM4Py calls. Five assertion-enabled suites pass on 650 exact sources after
correcting an OWN fixture's interval order; the failed test build is retained.

Evidence is the five `heuristic-plusplus-pandas-surrogate-*-checks.jsonl.gz`
segments, including `heuristic-plusplus-pandas-surrogate-checks.jsonl.gz`, with
`heuristic-plusplus-pandas-surrogate-validation.json`. All 10797
members round-trip byte for byte. The archive retains 1,377 public calls across
36 directories, and the preceding Unicode checkpoint remains hash-bound.
Broader type, ordering, metadata, identity, timestamp and routing gates remain
open. PLUSPLUS and Alpha+ remain partial; coverage stays 46/86 packages and
199/387 modules, with 228 incomplete units and no promotion.

### PLUSPLUS pandas missing/categorical activity implementation (2026-10-03)

Matched missing-activity filtering in counts, sojourns, case boundaries and
interval relations while retaining every caller row and duration mutation.
Performance adjacency retains missing rows as barriers and omits pairs that
touch them. The first implementation removed those barriers; its failed suite
and 72-discovery diagnostic replay remain retained with exact sources and logs.
A second failed suite exposed explicit-null event roles; the final build tests
that correction too, with all three sets of exact inputs retained.
Unused categorical activities now retain zero counts and appear when the
minimum count admits zero, without adding unobserved sojourn values. Empty
aware/naive inputs and all-missing activity inputs reproduce exact public
errors and their mutation timing. Evidence comprises 72 fresh PM4Py calls
and 144 separately counted public pandas grouping diagnostics on complete
category carriers. The pre-change 72-discovery replay retains the actual
field, Petri and error differences.

Of 1,152 retained PLUSPLUS aliases, 1,148 match: 514 full supported HN results,
201 Petri presentations and 433 expected errors. The 26066 assessed fields,
connection domains, input-after, option and mutation checks match. All 77
generic regressions remain unchanged. Actual accepted calls are 1,229
discoveries, 205 presentations and 43 generic conversions. Five assertion-
enabled suites pass on 650 exact compilation inputs. The unchanged Unicode
decoder retains the preceding exhaustive scalar evidence through the parent.

Four new missing-first-activity Petri aliases still differ in the hidden name
`hid_4` versus `hid_2`. A file-only renamed-copy diagnostic isolates that name
while preserving every original producer artifact; those copied matches give
no parity credit and invoke no producers. All 1,080 preceding controls remain
matched. The remaining name/order behavior stays open for the next step.

The three `heuristic-plusplus-pandas-activity-*-checks.jsonl.gz` segments and
`heuristic-plusplus-pandas-activity-validation.json` retain 6311 members
byte for byte. The preceding surrogate validation and five archive segments
remain hash-bound, with 1,449 public calls in the evidence chain. Broader type,
ordering, metadata, identity, timestamp and routing gates remain open.
PLUSPLUS and Alpha+ remain partial; coverage stays 46/86 packages and 199/387
modules, with 228 incomplete units and no promotion.

### PLUSPLUS pandas Petri join ordering (2026-10-03)

Corrected join traversal to visit a node's own join before its outgoing
targets, retaining the first domain visit. Hidden numbering still counts
every join before every split, including removed transitions. The correction
closes all four naming differences retained at the activity checkpoint.
Controls distinguish activity frequency order, reversed case tie order,
category declaration order, Unicode/surrogate names and missing boundaries.

This step retains 240 real PM4Py calls: 192 pandas discoveries and 48 exported
conversions of the actual returned HN. Direct pandas Petri results, explicit HN
conversions and repeated Petri results match on all 48 contrasts. Conversions
leave HN and parameters unchanged. The 384 public pandas group diagnostics
and 96 returned-net key-order observations are separate from producer calls.
Opaque pinned-module hashes remain equal before and after. The pre-change
96-discovery replay retains 32 Petri differences and 2,556 matched HN fields.
The 48 actual public HN conversions also match the retained native Petri
presentations; that file-only comparison invokes no additional producers.

All 1,248 retained PLUSPLUS aliases match: 562 supported HN results, 253 Petri
presentations and 433 expected errors. All 28,622 assessed fields, connection
domains, input-after, option and mutation checks match; all 77 generic
regressions remain unchanged. Actual accepted native calls are 1,325
discoveries, 253 presentations and 43 generic conversions. The baseline adds
96 discoveries and 48 presentations. Five assertion-enabled suites pass on
650 exact compilation inputs, with 564 shared data factories. The unchanged
Unicode decoder retains the preceding exhaustive evidence through the parent.

The three `heuristic-plusplus-pandas-petri-order-*-checks.jsonl.gz` segments
and `heuristic-plusplus-pandas-petri-order-validation.json` retain 5242
members byte for byte, with exact sources, helpers, process logs and binary
hashes. The preceding activity validation and three archive hashes remain
bound; the evidence chain contains 1,689 real public calls. Broader type,
ordering, metadata, identity, timestamp and routing gates remain open.
PLUSPLUS and Alpha+ stay partial. Coverage remains 46/86 packages and 199/387
modules, with 228 incomplete units and no promotion.

### PLUSPLUS typed decoration values (2026-10-03)

Added `HeuristicMinerOptions::plusplus_decoration_value` to preserve explicit
null, booleans, signed/unsigned integers, doubles, strings and nested array/
object values. The exact string `frequency` selects frequency; other supplied
values select performance. This typed value takes precedence over the existing
string selector and enum, while omission retains their behavior. CLASSIC
ignores the parameter. The form-field adapter remains a separate routing gate.

Evidence comprises 89 fresh PM4Py calls: one exported log conversion and 88
discoveries over 22 values, on pandas and direct EventLog HN/PN routes. All nine
options are explicit; every call succeeds. Parameters and direct EventLogs
remain unchanged, and the actual pandas `@@diff` mutation is retained. Another
90 group diagnostics and 44 returned-net key-order observations are counted
separately. The pre-change API matches all 28 string cases and cannot represent
60 non-string aliases; those original unsupported inputs remain retained with
zero native calls rather than substituted values.

All 1,336 retained PLUSPLUS aliases match: 606 supported HN results, 297 Petri
presentations and 433 errors. All 30,954 assessed fields, connection domains,
input-after, options and mutations match, and all 77 generic regressions remain
unchanged. Accepted calls are 1,413 discoveries, 297 presentations and 43
generic conversions; the retained baseline adds 28 discoveries and 14
presentations. Five assertion-enabled suites pass on 650 exact sources, with
566 shared data factories. An independent file-only decoder matches actual
typed selectors to the original public parameters before and after all 1,336
calls. It adds no producer calls; native integer-family and dictionary-order
representation limits are explicit in the validation. The Unicode decoder is
unchanged and the earlier exhaustive evidence remains bound through the parent.

The three `heuristic-plusplus-decoration-values-*-checks.jsonl.gz` segments
and `heuristic-plusplus-decoration-values-validation.json` retain 5190
members byte for byte, including exact sources, helpers, logs and binary
hashes. The preceding ordering validation and three archive hashes remain
bound, with 1,778 public calls in the chain. Broader type, metadata, identity,
timestamp and routing gates remain open. PLUSPLUS and Alpha+ stay partial;
coverage remains 46/86 packages and 199/387 modules, with 228 incomplete units
and no promotion.

### PLUSPLUS typed decoration parent routes (2026-10-03)

Verified the 22 typed decoration values through parent PLUSPLUS HN/PN routes
on EventLog, EventStream and DataFrame carriers. Evidence contains 134 fresh
PM4Py calls: two exported carrier conversions and 132 discoveries. All nine
options remain explicit and unchanged. EventLog/Stream inputs and DataFrame
HN inputs stay unchanged; DataFrame Petri inputs retain the `@@diff` mutation.
The 90 pandas grouping diagnostics and 66 returned-net key orders are counted
separately. Installed/pinned opaque module hashes match before and after.

All 132 native aliases match: 66 HN results, 3,498 assessed fields and
connection domains, and 66 Petri presentations, including complete supported
names, properties, labels, arc occurrences/weights and markings. Input-after,
parameters, errors and mutations match. Separate file-only checks match all
132 typed selectors before/after and 132 public parent/direct pairs across six
groups. Those checks execute zero producers and preserve all original values.

This evidence step reuses the unchanged production core, 650 exact compiled
inputs, five assertion-enabled suites and 77 generic regressions. The new
probe executes 132 discoveries and 66 presentations. No new production build,
suite run or generic call is counted. The inherited preparation printout's 77
generic label is corrected by the bound empty generic plan and actual records.

The combined finite corpus has 1,468 matching aliases: 672 HN results,
363 Petri presentations and 433 expected errors, with 34,452 assessed net
fields. The public evidence chain contains 1,912 producer calls. Two archive
segments retain 598 checkpoint members byte for byte and bind the
preceding typed-value validation, exact sources, tested binaries and archives.
`heuristic-plusplus-decoration-parent-validation.json` records the scope.

| Evidence segment | SHA-256 | Bytes |
| --- | --- | ---: |
| `heuristic-plusplus-decoration-parent-controls-checks.jsonl.gz` | `2d56b614cd8b0049b946e3a2fdd10d833d79b1f1dc6250cd5667e2a6866bb7a1` | 8189525 |
| `heuristic-plusplus-decoration-parent-replay-checks.jsonl.gz` | `c00a832d2cad7152b169fa3b35c3962cb7e3662eba66e7331f0a1d452403f919` | 10117468 |

Form-field typed routing and broader carrier/scalar, ordered metadata,
identity and timestamp gates remain open. PLUSPLUS and Alpha+ remain partial;
coverage stays 46/86 packages, 199/387 modules and 228 incomplete units.

### PLUSPLUS typed decoration request transport (2026-10-03)

Added JSON-valued `heuristic_plusplus_decoration_value` to the shared request
parser. It retains null, bool, int64/uint64, double, string and nested array/
string-key object values, including Python JSON nonfinite constants and
escaped lone surrogates. Typed precedence, omission and CLASSIC behavior stay
consistent with the native option. Invalid syntax/UTF-8 reports the field and
byte position. Integer width and a 256-level nesting bound remain explicit.
The existing server mine route forwards the resulting heuristic options.

The actual old-parser baseline ignores all 22 supplied typed values in
22 parameter parses; it executes zero discoveries/presentations. The fresh
accepted build passes five assertion-enabled suites on 650 exact source
inputs, including grammar, Unicode, nesting, integer and double boundaries.

All 1,468 retained PLUSPLUS aliases match through the real request field:
672 HN results, 363 Petri presentations, 433 expected errors and 34,452
assessed net fields. Connection, parameter, input-after and mutation checks
match. All 77 generic regressions remain unchanged. Accepted native probe
calls are 1,545 discoveries, 363 presentations and 43 generic conversions.
The independent typed/wire check matches all values before/after and counts
2,936 actual field parses from the recorded process markers.

The new Unicode decoder matches independent Windows/Python bytes for all
1,114,112 escaped code points and 1,112,064 valid JSON scalar string forms, including
2,048 escaped lone surrogates. Its 2,226,176 parser calls add no mining or
reference producers. Accepted collected parser calls total 2,229,112; the
old baseline adds 22. Suite API calls remain separate test evidence.

Three archives retain 5465 exact members, binding full sources,
actual returns and process logs, original failed/cancelled attempts, tested
binaries and the parent checkpoint. Those unsuccessful attempts execute zero
collected native producers or parsers. No public PM4Py call is repeated; the
bound public evidence chain remains at 1,912 calls.
`heuristic-plusplus-decoration-wire-validation.json` records the scope.

| Evidence segment | SHA-256 | Bytes |
| --- | --- | ---: |
| `heuristic-plusplus-decoration-wire-core-checks.jsonl.gz` | `6b14f8235b8764dd08da27bbfe5e16c8c5110d8400b10ac25b69c663cfeaec0a` | 21012191 |
| `heuristic-plusplus-decoration-wire-controls-checks.jsonl.gz` | `051bab4c6fa2fdb445613a107a431e5714ab79c8b787f7a965e96f14acebd0e5` | 42404900 |
| `heuristic-plusplus-decoration-wire-replay-checks.jsonl.gz` | `16dd2b655aae2564d08ce96a4fa43c1027930e8d8cb6aa60bde4e565084dda5a` | 92490326 |

Broader classes, integer/depth domains, dictionary ordering, metadata,
identity and timestamp gates stay open. PLUSPLUS and Alpha+ remain partial;
coverage stays 46/86 packages, 199/387 modules and 228 incomplete units.

### Full-presentation simplicity metrics (2026-10-03)

Added arc-degree and extended Cardoso evaluation for `PetriNetPresentation`.
Structural arc counts preserve separate occurrences and ignore weights;
Cardoso choices preserve node identity and distinct successor-place sets.
Both native arc-degree carriers and the shared request baseline accept NaN
and infinities. Invalid graph endpoints and malformed numeric fields have
explicit diagnostics. Existing cyclomatic evaluation keeps its executable net.

All 1,510 actual pinned public controls now match through the parsed request
family/variant/options and native full presentation. They cover all 256 two-
place/two-transition incidence graphs and weighted/parallel/empty/choice
controls. The preceding actual native run had 10 scalar differences and 60
unsupported parameter cases. All graph, parameter and input-after checks
match; both native probes execute 1,510 metric calls and 1,510 parser calls.
Three actual CLI processes verify 12 per-model nonfinite results, including
the documented computed NaN-to-JSON-null representation.

Four assertion-enabled Release suites pass on 650 frozen inputs. The archive
retains 776 exact members, including sources, returned values,
compiler/process logs, byte-verified binary chunks and the initial zero-call
reference-check failure. Git LF normalization is separately bound to tested
source bytes. See `simplicity-parity-contract.md` and
`simplicity-structure-validation.json` for the complete scope and accounting.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `simplicity-structure-checks.jsonl.gz` | `bf9a1a0f1031782e21af3e31d2c022250d1aa3a5109cdc642ea8b1147cc288ff` | 63456521 |

PNML/CLI/server retention of parallel arc occurrences, extended cyclomatic
and broader evaluation/conformance gates remain open. No catalogue promotion:
coverage remains 46/86 packages, 199/387 modules and 228 incomplete units.

## Imported PNML simplicity arc occurrences (2026-10-03)

Parent checkpoint: `d0da926e070586622891ae291efcc3f79c517f08`.
Imported PNML documents now retain distinct arc objects, XML order, weights,
types and IDs. The native document simplicity overload and imported CLI/server
routes use these objects for arc-degree and Cardoso. Export/reimport retains
them; stale metadata is checked against executable firing multiplicities/order
and rejected atomically. Cyclomatic stays on executable reachability.

Fresh pinned evidence contains 264 public imports and 1,320 public metric
calls. Each native before/after probe makes 528 imports and 2,640 metric calls.
The baseline has two scalar differences and loses one parallel inventory;
the accepted probe matches all 2,640 scalars exactly and all 264 inventories,
including weights/IDs/order across roundtrip. The parallel K=0 score changes
from baseline 3/7 to public/accepted 1/3. Twelve accepted CLI runs and 14 real
HTTP metric results pass, including nonfinite and exported-model controls.

All five assertion-enabled Release suites pass on the bound 650-input source
bundle. The accepted build reuses seven byte-verified binaries after a single
test-only statement correction; all other 649 inputs are unchanged and all
suites run again. The archive retains 1,686 exact files/1,721 verified chunk
rows, both failed build/test attempts and the corrected helpers. One initial
failed adapter's successful CLI process lacks retained PID/timestamps; exact
argv/output and the limitation remain recorded. Separate native double and
six-significant-digit/null wire comparisons are explicit. See
`simplicity-parity-contract.md` and `simplicity-pnml-validation.json`.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `simplicity-pnml-checks.jsonl.gz` | `bd84b9c898c79133607c330d3efa53aa2834e045f0d3dd569490495310771180` | 69762434 |

Extended cyclomatic reachability/parameters, composite evaluation and broader
evaluation/conformance dependencies remain open. General PNML metadata,
marking keys, malformed/extended-arc and additional carrier domains are outside
these finite comparisons. Public PNML imports omit explicit zero-token keys
that native documents preserve; public exporter byte/ID formatting is not
claimed equivalent. The catalogue remains 46/86 packages, 199/387 modules and
228 incomplete units. Full module/package parity is not established.

## Imported PNML composite structure and weight domains (2026-10-03)

Parent checkpoint: `dbabb667be72f5c60365ed22fc4c0e4b3879ab2a`.
Composite evaluation now accepts retained PNML documents across native carriers
and imported CLI/server routes. Replay metrics keep executable multiplicities;
the simplicity component uses the original arc occurrences. Complete stored
markings are inherited unless explicit vectors override them. Negative,
nonfinite and overflowed coefficient totals follow the observed normalization;
zero totals parse and yield an explicit uncomputed result.

Fresh pinned evidence contains 784 public imports and 784 parent composite
calls: 688 successful results and 96 zero-total exceptions. Each actual native
before/after probe makes 784 imports, 784 parses and 1,960 composite calls.
The baseline parses 400 requests, matches 740 outcomes, leaves 720 valid aliases
uncomputed and has 500 simplicity plus 320 aggregate scalar differences. The
accepted probe parses all 784 requests and matches all 1,960 outcomes, including
17,200 exact scalar comparisons and 240 projected zero-total errors. Public
EventLog and DataFrame empty-trace semantics remain distinct; each lazy
materialization occurs once and input/parameter snapshots remain unchanged.

All 196 accepted CLI runs and 196 real HTTP evaluations match. All 228 recorded
HTTP requests succeed, including empty CSV and nonfinite/null controls. Wire
numbers retain existing six-significant-digit precision, while native doubles
are verified exactly. The server is deliberately stopped after validation.
All five assertion-enabled Release suites pass on the fresh 650-input build.
The archive retains 2,827 exact files/2,864 verified chunk rows,
tested binaries, process records and original failed helper derivations; failed
metadata/probe/comparison attempts add no metric calls. Sixteen opaque reference
module proofs match the pin with recorded newline normalization. See
`simplicity-parity-contract.md` and `composite-pnml-validation.json`.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `composite-pnml-checks.jsonl.gz` | `e2914a4d7a7dc5ba5bfd6faf471c26ee3179250e47af6992b57bc53e4b04711e` | 74739515 |

Extended cyclomatic reachability/parameters and broader evaluation/conformance
dependencies remain open. Additional composite graph/log/marking, typed or
nonnumeric parameter domains and exception-class/stack identity are outside
these finite comparisons. General PNML metadata, marking keys,
malformed/extended-arc and additional carrier domains remain open. Public PNML
imports omit explicit zero-token keys that native documents preserve; public
exporter byte/ID formatting and full application wire precision are not claimed
equivalent. The catalogue remains 46/86 packages, 199/387 modules and
228 incomplete units. Full module/package parity is not established.

## Cyclomatic reachable transition occurrences (2026-10-03)

Parent checkpoint: `aca20ffe344462a28b98c972f9e401645975ef24`.
The executable cyclomatic metric now preserves distinct enabled transition
occurrences between the same marking pair and matches the observed public
cycle scalar. Strongly connected components remain separate graph metadata.
Regressions cover alternatives, cycles with one/two tokens, document forwarding
and isolated loops.

Fresh pinned observations contain 1,684 metric calls, 2,788 independent graph
calls and 324 default-marking discoveries. Each native before/after probe makes
1,426 request parses and 2,852 metric calls on faithfully representable controls.
The baseline has 416 score and 280 edge-count differences; all 2,852 accepted
scalars and graph metadata match, with 17,112 alias/metadata field comparisons.
The 258 supplied-name/distinct-input-arc observations remain outside the
executable projection and preserve explicit remaining gates. All metric calls
succeed; 18 initial graph snapshot adapter errors survive separately. The fresh
adapter retains endpoint aliases, and larger naming experiments reject a
small-matrix pair-count shortcut. No catalogue promotion follows.

All 25 accepted CLI and 25 real HTTP scalar results match, with 51 recorded HTTP
requests returning 200; the baseline CLI matches 8. A terminal MSVC C1001 capture
in unchanged code is retained. The fresh serial production compilation succeeds;
four suites pass and one old parallel-transition expectation fails. Correcting
only that test keeps 649 of 650 inputs byte-identical. The accepted build reuses
seven byte-verified core/test/application binaries, compiles the corrected
evaluation suite and passes all five assertion-enabled suites. The failed
test source/executable/logs and zero-call retry-preparation correction survive.
The archive contains 1,144 exact files/1,183 verified chunk rows,
tested binaries, process logs and original captures. See
`simplicity-parity-contract.md` and `cyclomatic-reachability-validation.json`.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `cyclomatic-reachability-checks.jsonl.gz` | `7f1700b8c466591d4cb885d0c314fb4a5e90ebf7bb3d586e9ce82cad99d0e48c` | 74628422 |

Cyclomatic presentation and marking-name collisions, distinct parallel
input-arc enabling, typed/huge weights and markings, extended arc semantics,
caps and unbounded graphs remain open. Broader evaluation/conformance and
additional composite graph/log/marking, nonnumeric parameter and exception
identity domains are not established. General PNML metadata, explicit marking
keys, malformed/extended-arc carriers, public exporter byte/ID formatting and
full application wire precision remain open. The catalogue stays at
46/86 packages, 199/387 modules and 228 incomplete units.
Full module/package parity is not established.

## Cyclomatic presentation reachability (2026-10-03)

Parent checkpoint: `89ba2d698bff0d39385e6711724d99cd4ae18a16`.
Full presentation and ordinary PNML cyclomatic reachability now preserve
individual weighted input arcs, supplied names and ordered sparse marking
keys. Enabling checks each input arc separately; graph firing removes depleted
or oversubscribed consumed entries before producing tokens. New regressions
cover consume-only and one/two-token source loops. The executable control
route preserves its existing summed-input behavior.

Fresh pinned captures contain 2,832 actual metrics, 2,692 independent graphs,
324 default markings, 52 state-name calls and 20 imports, plus 630 calls each
to enabling, regular firing and weak firing. All producers retain exact inputs,
outputs, process witnesses and opaque pin hashes without implementation
inspection. Each accepted/baseline native bundle makes 2,692 request parses
and 8,076 metric calls. Every target case computes: per route, 2,048/2,062 main
scalars and 2,039/2,062 complete outcomes match. All 3,934 main target outcomes
without name aliases and all 1,260 firing outcomes match every field. All
2,692 executable controls remain unchanged. Fourteen scalar and twenty-three
full-result differences per route remain in name-alias controls; the 120-call
order experiment observes varying public results on twelve of fourteen controls.
These finite comparisons do not promote the catalogue or establish full parity.

All 20 CLI and 20 real HTTP metrics match; all 41 recorded HTTP requests return
200. Baseline CLI matches 13. An initial probe stack overflow occurs before
all call markers and remains a zero-call failed attempt. The intermediate
production build passes five suites but fails 24 non-alias metadata comparisons.
Fresh weak-firing evidence motivates the correction and a second complete
652-input build with zero binary reuse; all five assertion-enabled suites pass.
Both builds, original captures/comparisons, failed initializer and the zero-call
comparison-subtotal correction survive. Unicode classification verifies all
1,114,112 codepoints against public Python operations, with zero PM4Py calls.
The two archives retain 1,197 files/1,272 round-trip-verified rows.
See `simplicity-parity-contract.md` and `cyclomatic-presentation-validation.json`.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `cyclomatic-presentation-checks.jsonl.gz` | `075f57dbdaad05289aca55f9373b66f21f4205ad06f31aa4ec0a8e2f11557a9d` | 82040213 |
| `cyclomatic-presentation-intermediate-checks.jsonl.gz` | `c9e823477c9b6980ad50be9a139070b8e021a076ac039c356bc0696ff519b83c` | 64563737 |

Supplied marking-name collisions and reference set/allocation order remain
open. Additional typed/huge/signed arc-weight and marking domains, extended arcs,
foreign keys and exception identity, caps/time limits and unbounded graphs are
not established. Broader evaluation/conformance, composite log/graph/marking,
PNML metadata, exporter bytes/IDs and full application wire precision remain
open. The catalogue stays at 46/86 packages, 199/387 modules and
228 incomplete units. Full module/package parity is not established.

## PNML arc types and default cyclomatic semantics (2026-10-03)

Parent checkpoint: `331ae157c25c5ecf4ebd20626f8d93fa4ea8eab5`.
PNML import/export now use the pinned public arctype annotation and exact type
values. Reset/inhibitor documents and direct executable nets use classic
weighted enabling and weak firing for the default cyclomatic metric. Decoded
arc types survive; stale document arc metadata still rejects.

Fresh captures retain 1,056 actual public imports, 3,168 metrics, 1,056 independent
graphs, 48 exports and 656 calls each to enabling and weak firing. All 352 original
carriers and all 352 independently measured executable projections are retained,
including 160 aggregated parallel-endpoint controls. Every corrected native
bundle makes 352 imports, 1,056 parses and 4,224 metrics. All 2,112 final target
outcomes and all 1,056 executable-control outcomes match every checked field.
Carrier type/weight matches improve from 106 to 352; document cyclomatic matches
from 346 to 352; executable cyclomatic matches from 219 to 352. All 810 structural
and ordinary controls stay unchanged. Independent public reimports match all
352 native document exports, which remain byte-identical in the final build.

All 80 final CLI and 80 HTTP results match; all 161 recorded requests return 200.
Baseline CLI matches 74. Both fresh 652-input builds pass all five assertion-enabled
Release suites with zero reuse. Five source files differ from the parent; three
inputs change after the wire/document intermediate candidate. Original captures,
the metric-only carrier-check insufficiency, malformed multiline JSON adapter
failure and zero-call derivations remain retained. Missing original failed-probe
process timestamps are explicitly disclosed. No upstream implementation/bytecode
is inspected. The five sub-50-MiB archives retain 5,258 files/5,328
round-trip-verified rows, including both new cores in split chunks.
See `simplicity-parity-contract.md` and `pnml-arctype-validation.json`.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `pnml-arctype-checks.jsonl.gz` | `63f2087610c25a894b65f0273bccc3bae24cd64f534ac5b7159152ce573e3b52` | 42739189 |
| `pnml-arctype-core-1.jsonl.gz` | `c4e630ea6f5f920b0fadd9022da565097e6765a6a825f9518bf3c8a0b091016c` | 23147401 |
| `pnml-arctype-core-2.jsonl.gz` | `c676b0b512f0ddb2d890836f34f1a93d4c7bc59f0edefb1835dc0dd574155ccc` | 26733473 |
| `pnml-arctype-core-3.jsonl.gz` | `a886bc8dd0713543a896243ddcc8059afb7157125c03a60ffa66127b7af18804` | 23158742 |
| `pnml-arctype-core-4.jsonl.gz` | `13eaa7e5c43c8aaaa6c0986d4b47844e4b218210205b3ac7fe736b90ae01f9c3` | 26726709 |

Supplied marking-name collisions and reference set/allocation order remain
open. Additional nonunit, typed, huge or signed extended-arc weights and marking
domains, foreign keys and exception identity are not established. Malformed or
repeated PNML annotations, general metadata, IDs, zero-key/order behavior and
exporter byte formats remain open. Unbounded graphs, time limits/state caps,
broader evaluation/conformance/composite domains and full application numeric
wire precision remain open. The catalogue stays at 46/86 packages,
199/387 modules and 228 incomplete units. Full module/package parity is not
established.

## PNML arc weights and XML annotation text (2026-10-03)

Parent checkpoint: `2dc5821fb1457df638b09f29242e62ea3e2d0407`.
Reset/inhibitor input arcs now retain nonunit, zero and signed weights across
document, extended primitive metadata, presentation and native export routes.
An owned Unicode 14 integer parser handles measured sign/underscore/decimal
lexemes; repeated/empty/CDATA/comment/child/PI/prefix text follows the captured
public behavior. Direct presented cyclomatic weights use checked signed64
marking arithmetic. Other execution consumers retain their arc-type behavior.

The original 262 public imports retain 172 successes and all 90 errors. Final
native evidence makes 248 safe imports and 1,392 actual metric calls; all 150
successful carriers retain weights/types and all 90 public rejections align as
booleans. All 450 document outcomes match six checked fields. Presentations
match 489/492 and primitive nets 449/450, with explicit range/structural gates.
Every exact native export is independently reimported: all 150 carriers, net
subclasses, actual metric triplets and semantic marking checks match. Combined
public totals are 412 imports, 966 metrics and 322 graphs/exports/enabling/weak
calls. All 150 CLI and 150 HTTP results match; all 301 requests return 200. The
old 352-case matrix retains identical snapshots/exports and all 4,224 metrics.

All production/test/application binaries compile afresh from 655 frozen inputs.
Four suites pass before an obsolete ordinary-zero rejection assertion fails.
The accepted correction changes only that assertion, preserves 654 inputs and
recompiles the Petri suite against seven exact hash-guarded compiled binaries.
All five assertion-enabled suites then pass. Original zero-call preparation and
probe compile failures, the failed candidate/test binary and all derivations
remain. Unicode generation checks 1,114,112 standard-library classifications,
660 decimal codepoints and 58 whitespace calls separately from PM4Py counts.
Opaque 11-module pin proofs match; no upstream implementation/bytecode is read.
Three sub-50-MiB archives retain 4,315 files/4,350 round-trip-verified
rows, including the fresh core in two archives and all 655 accepted inputs.
See `simplicity-parity-contract.md` and
`pnml-extended-annotations-validation.json`.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `pnml-extended-annotations-checks.jsonl.gz` | `3f3ba6d32f20d266cde3e91917e5c55786ee72338c12bcb4b51712a4412182a7` | 24426111 |
| `pnml-extended-annotations-core-1.jsonl.gz` | `e48213b907f80854a0ef812603f8056e690a3835ef8b2ae20e4d55b59431b090` | 23224772 |
| `pnml-extended-annotations-core-2.jsonl.gz` | `25e1baf9ccb82fc3fc2a29154b3be94e12fbb81fbec8b11b023dfb5d1fc67ecd` | 26838333 |

The retained matrix still has 14 ordinary positive weights whose native
imports are deliberately unattempted because primitive multiplicity expands
into vector slots; the smallest of these weights is 2,147,483,647. Eight public
successes exceed uint64 weight magnitude. Four signed ordinary PNML cases lack
a primitive execution projection. Three directly presented output weights
exceed signed64 reachable marking range and report uncomputed. One ordinary
zero-input case loses primitive structural adjacency: direct executable Cardoso
is 1, while the actual public, document and presentation results are 2. These
are explicit remaining gates, with their original public rows retained.

Exception class/message equality, additional malformed/repeated/namespace
annotations, combinations of typed inputs and outputs, supplied marking-name
collisions and set/allocation-order dependence remain open. General metadata,
foreign keys, IDs, zero-key/order behavior, public exporter byte formats,
unbounded graphs, time limits/state caps, full application numeric precision
and broader evaluation/conformance/composite domains remain open. The catalogue
stays at 46/86 packages, 199/387 modules and 228 incomplete units. Full
module/package parity is not established.

## Compact ordinary PNML weights (2026-10-03)

Parent checkpoint: `f0934d3da788e4b34e866899ff299446b5127e4e`.
Zero, signed and large ordinary arc weights now use bounded occurrence storage
when a transition needs compact weights. Shared checked enabling/firing,
primitive cyclomatic projection, zero structural adjacency and both PNML export
routes preserve the measured semantics. Specialized weighted conformance is
explicitly unavailable, with computed=false/error in CLI and HTTP and a frontend
diagnostic that hides invalid scores.

The original 262 public observations are reused. Fresh boundary and exact-export
reimports make 176 imports, 528 metrics and 176 graph/export/enabling/weak calls
each. Both native probes attempt 268 imports and 1530 metrics plus 18 enabling
and 18 weak firings. All 170 successful arc carriers match, and 1521 metric
outcomes match six fields (9126 matching fields). All 90 public rejection
booleans align. Every native export reimport matches its carrier, subclass,
actual metric triplet and semantic markings; all 170 final exports bind those
exact captured inputs. Both reruns of the old 352-case matrix retain identical
cases and all 4224 metric outcomes/1056 parameter parses.

The initial candidate passes 17 suites but five automatic conformance imports
fail; its misleading stale-model HTTP metrics are rejected as evidence. After
the diagnostic correction, all 170 CLI runs exit zero and all 511 HTTP requests
return 200. Each route has 167 scalar matches and three explicit range results;
170 HTTP evaluations bind the exact freshly imported model bytes. Eighteen
compact models expose unavailable conformance. Both complete builds are fresh
with 20 binaries each and zero reuse. Accepted inputs total 655, with 12 edits
and 643 unchanged from the parent. All 17 assertion-enabled suites, TypeScript,
Vite and 253 existing frontend tests pass. Rendered QA is not claimed.

Six archives retain 6,354 files/6,424 round-trip-verified chunk rows,
including both fresh cores, all 44 binaries, original failures/helpers, captures
and explicit source aliases. A failed source-count preparer makes zero build
calls and remains retained with a corrected derivative. Public runtime pin
proofs match without inspecting upstream implementation or bytecode. See
`simplicity-parity-contract.md` and `pnml-ordinary-weights-validation.json`.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `pnml-ordinary-weights-checks-1.jsonl.gz` | `786968b97538d125bf913b50a4b3d3af666d74b4edbad3890397e481bc938659` | 29333608 |
| `pnml-ordinary-weights-checks-2.jsonl.gz` | `dad9d61add74d7793a22dd1161275638bdb325802537914e78921f45df9e7d5f` | 18405793 |
| `pnml-ordinary-weights-candidate-core-1.jsonl.gz` | `03458ed238a647b3fc9a00fd4a68c1e79a964e283b14d265830df4a2ee679c88` | 23230248 |
| `pnml-ordinary-weights-candidate-core-2.jsonl.gz` | `2b3e209ef18c971ae2457a0e1c8e907f2c74126024702e07acca4da280373e6d` | 26870038 |
| `pnml-ordinary-weights-final-core-1.jsonl.gz` | `051fc19ec341470af20a41e454c726af6cd0a497612a2d039d53dde45fc2e794` | 23229589 |
| `pnml-ordinary-weights-final-core-2.jsonl.gz` | `275ab2069605a531b884a94d8db2532c988f4e663d0d5f47d6bb6010b5b2e30f` | 26874479 |

Eight successful public integer weight magnitudes still exceed uint64 and
reject explicitly in the native importer. Three large output profiles exceed
signed64 reachable markings on each of three metric routes: nine uncomputed
outcomes. Two independently attempted weak firings exceed signed64. Arbitrary
integer carriers and markings remain incomplete.

Specialized weighted conformance, replay, analysis, transformation and composite
consumers remain unverified. The application exposes unavailable conformance for
the compact-weight models; this checkpoint does not implement those methods.
Weighted frontend graph rendering/labels, full application numeric precision,
unbounded reachability and time/state limits remain open. Exception class/message
identity, broader malformed/repeated/namespace combinations, metadata, foreign
keys/IDs, set/order dependence and marking-name/zero-key behavior remain open.
The catalogue stays at 46/86 packages, 199/387 modules and 228 incomplete units.
Full module/package parity is not established.

## PNML transition labels and invisible flags (2026-10-03)

Parent checkpoint: `35c538e03f80fb72846644e5075eaec57cafd82c`.
Absent/empty usable names now fall back to visible node IDs; the first nonempty
direct name/text initial segment retains whitespace and measured mixed XML
semantics. ProM invisible flags use the measured tool/activity matching and
reject missing required attributes. An explicit empty transition ID stays
visible with empty-string label and survives document export/reimport.

All 43 import booleans, 39 successful labels/names and 624 public trace outcomes
match; ten trace fields yield 6240 matches. The native producer invokes replay
312 times, mapping to both independently called public routes. All 39 exact
native exports are independently reimported; their carrier/marking semantics
and 624 complete public replay returns match. Fresh weighted discovery adds
58 imports/1044 replays and 522 equal route pairs. The ten intended hidden
fixtures actually import visible under label hidden and do not prove weighted
hidden routing. Inventory plus all captures total 140 fresh public imports,
2292 replays and 78 exports, with before/after opaque pinned module proofs.

The 352-case regression retains entire JSON equality, 4224 metric outcomes and
1056 parameter parses. All 43 CLI and 83 HTTP checks align import outcomes;
39 successful conformance bundles per route match seven captured fields and
all 39 HTTP exports bind the exact current imported model. Both full builds
compile 22 binaries afresh from 655 frozen inputs with zero reuse. The first
fails one obsolete silent-label assertion after 18 passing suites; the accepted
build passes all 19. Accepted source changes three inputs, preserving 652.

Six archives retain 2,004 files and 2,076 fully verified chunk rows,
both fresh cores, all 48 binaries, original failures, corrected derivatives,
public observations and explicit source aliases. See
`simplicity-parity-contract.md` and `pnml-transition-labels-validation.json`.
Weighted replay/conformance remains a separate numeric correction; the existing
application gate stays in place. No catalogue promotion or full parity claim.

| Archive | SHA-256 | Bytes |
| --- | --- | ---: |
| `pnml-transition-labels-checks-1.jsonl.gz` | `61a70a2b93391dfe247013fa6c2c1661c71bced65a9e6311112b1d491f22b34b` | 29164468 |
| `pnml-transition-labels-checks-2.jsonl.gz` | `563d529b2a46452d81c9cbda7f334ee511fd6e32d25ae8badf37ffc020e943ee` | 19098859 |
| `pnml-transition-labels-failed-core-1.jsonl.gz` | `0a0601ade81eb3f9612dac5e9f5ec3b1705407147bb08d01381d42dfb7f48011` | 23210238 |
| `pnml-transition-labels-failed-core-2.jsonl.gz` | `ffbbfb9449c4695ab774c2fb21120003860dced69703bce73bb27e5961372ec2` | 26886096 |
| `pnml-transition-labels-accepted-core-1.jsonl.gz` | `8934614bd69c56b2915e88b4d4324bd60088869903251c2a911f5c79836aa890` | 23224891 |
| `pnml-transition-labels-accepted-core-2.jsonl.gz` | `5233c6d7fd0414985c80ecfc2f19285550a156814518098bdb02928ec8b08596` | 26877623 |

Compact weighted replay/conformance is not corrected by this label change.
The existing application computed=false/error gate remains for zero, signed and
large ordinary weights. Public replay observations contain negative consumed or
produced totals and accumulated totals/markings exceeding uint64; native replay
still counts compact ordinary endpoints by one and lacks arbitrary signed integer
counters/markings. Weighted hidden routing, other diagnostic and parameter
variants, specialized analysis/transformation, full UI numeric precision and
unbounded numeric/state behavior remain open.

Exception class/message identity is not matched. General malformed/repeated/
namespace combinations, exporter formatting and metadata, empty place IDs,
foreign IDs/keys, set/order dependence and broader markings remain unverified.
The catalogue stays at 46/86 packages, 199/387 modules and 228 incomplete units.
No package/module promotion or full parity is claimed.

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

## Supplied hidden-path cache and cleaned PLTR trace follow-up

The forward C++ EventLog/PetriNet API now accepts supplied hidden-path matrices
for pre-event and final routing, distinguishes empty from absent/null caches,
and retains measured non-iterable-cache rejection timing. Cleared places now
retain their PLTR overfed-trace membership after token-flood cleaning.

The accepted core matches all 13440 retained parameter outcomes, closing 260
complete-return and 174 error-behavior gaps. Two new actual public captures add
8424 calls, 8016 complete returns and 408 errors; every complete direct/parent
pair agrees. The first PNML capture has visible fallback labels and is retained
as routing/cleaning evidence. A correction adds only explicit invisibility
markers; actual public snapshots verify all intended silent transitions. Each
capture matches 4008 whole native returns and 204 measured error behaviors.

The initial compile failure precedes replay. The first compiled candidate passes
all 152 suites but retains 144 whole PLTR cleanup differences; the final
diagnostic tracking resolves them. Both complete fresh builds freeze 660 inputs
and pass all 152 assertion-enabled suites. The typed regression matches 4758
complete returns and 3018 errors; the 95-model regression preserves 2216 full
carriers, 2032 tuple carriers, 2344 error behaviors and 2280 legacy records.
All 1140 positional CLI processes complete with their expected results/errors.
Frozen inputs, failed/candidate evidence, raw public/native outcomes, process
witnesses, seven probe binaries and both sets of 155 full-build outputs are
retained. Core witness snapshots and final build manifests are verified
separately, and reused parent captures are not counted as new public calls.

See [token-replay-parity-contract.md](token-replay-parity-contract.md) and
[forward-hidden-cache-validation.json](forward-hidden-cache-validation.json).
Supplied-cache CLI/HTTP transports, broader foreign cache/alias domains and
prior forward/backwards acceptance contracts remain open. Weighted aggregate
and composite conformance remain gated. The catalogue remains 46/86 packages,
199/387 modules and 228 incomplete units, with no promotions or full parity claim.

## Minimized silent-choice oracle and cleaned underfed-place correction

Twelve new acyclic controls isolate the enabled and blocked silent alternatives
from retained flood case 62. A fresh capture retains 1152 complete outcomes from
both public routes, with independently varied cleaning/PLTR flags, repeated
events and fixed supplied-cache choices. An independent oracle derives whole
path results from the owned inputs and enumerates every feasible global silent
order in this bounded topology, keeping it consistent throughout each trace.
It explains all public outcomes and rejects per-event order changes and mixtures
of fields from different paths.

The unchanged parent core retains 30 unexplained unique native results, or 60
whole direct/parent comparisons. They expose a cleaned gate that is reported as
both underfed and overfed. The correction restores measured underfed-membership
precedence and adds a native weighted-gate regression. The corrected core matches
all 1152 public results exactly; all 576 unique native carriers belong to whole
oracle paths. A recorded self-review verifies the oracle's actual input scope
and pure input-derived implementation, with identical expected carriers before
and after the fix.

The fresh build freezes 660 inputs (two edits, 658 unchanged) and passes all 152
assertion-enabled Release suites. The retained parameter, visible/invisible-path,
typed and 95-model regressions match every complete carrier and measured error
behavior, preserving all 2280 legacy native records. Raw public outcomes,
oracle paths and rejected controls, baseline/corrected carriers, process
witnesses and fresh build/probe binaries are retained with byte bindings.
The 1140 positional CLI processes preserve every expected result/error; all
3699 evidence files round-trip across 18 archives, retaining 162 fresh binaries.

See [token-replay-parity-contract.md](token-replay-parity-contract.md) and
[forward-choice-oracle-validation.json](forward-choice-oracle-validation.json).
The original seventeen larger weighted choice/flood fixtures still need complete
path enumeration. Broader forward/backwards and adapter acceptance contracts,
cache/alias domains and weighted aggregate/composite gates remain open. Catalogue
totals remain 46/86 packages, 199/387 modules and 228 incomplete units, with no
promotions or full parity claim.

## Original silent-choice fixtures and fixed cache enumeration

The original seventeen weighted choice/flood fixtures now retain 680 complete
public outcomes from both replay routes, preserving their exact PNML, original
trace data and case-specific controls. Cleaning and PLTR vary independently
across automatic, actual-public, ascending, descending and empty cache policies.
All 340 public route pairs agree and actual inputs remain unchanged. A fresh
probe of the accepted core makes 340 full calls; all 544 comparisons with the
same supplied cache match every result field.

The 88 strict automatic-cache differences remain in the evidence. All 68 native
automatic-cache call results match whole actual public ascending-cache results
through both routes, preserving each path's ordered firings and complete
diagnostics together. An independent pure graph oracle exhausts 1,217,856 fixed
local arc-order configurations into 382 distinct complete structural caches.
Every cache retains a fixed-order witness and multiplicity. A recorded
self-review verifies all actual inputs, cache memberships and whole-result
witnesses, and rejects an inconsistent matrix assembled from individually
shortest paths.

The completed parent build and 152 passing assertion-enabled suites are reused
with exact core/manifest bindings and all 660 frozen inputs unchanged. One new
probe binary, raw full carriers, helper failures and corrections, process
witnesses, enumerated caches and the scope review are retained. All 148 files
round-trip across two archives. See
[forward-choice-original-validation.json](forward-choice-original-validation.json)
and [token-replay-parity-contract.md](token-replay-parity-contract.md).

Independent whole replay-path derivation for every structural cache remains
open, along with the broader forward/backwards, adapter and catalogue acceptance
gates. Totals remain 46/86 packages, 199/387 modules and 228 incomplete units,
with no promotions or full parity claim.

## Complete original choice/flood paths

The original seventeen weighted acyclic fixtures now complete their fixed-order
choice/flood acceptance contract. An independent input-derived oracle calculates
whole replay paths for all 382 structural matrices from the previously exhausted
1,217,856 fixed local orders. Each cache stays fixed across repeated events;
cleaning and PLTR policies produce 1528 complete expected records frozen before
the new public capture. Ordered firings, weighted repair/cleanup, markings,
counters, fitness and full diagnostic carriers are derived together.

The fresh capture returns all 3056 direct/parent outcomes, and the accepted core
returns all 1528 native outcomes. Every complete comparison and all 3056 Trace
equality matrices match. The oracle also explains the retained 680 public/340
native parent results and all 501 whole historical observations. Nine opaque
module pin proofs and actual inputs remain unchanged. A recorded self-review
checks 1528 flow ledgers and 3720 event steps and rejects both changing the cache
between events and combining fields from different paths.

All enumerated-cache outcomes agree with physical reachability and the reported
eventually-enabled diagnostic. Two supplemental empty-cache states distinguish
those behaviors; 47 fresh direct helper calls confirm the complete derived
marking set. The step retains 3137 actual public helper calls in total. Cleanup
is proved within these inputs through active sink co-membership; standalone
component-helper carriers remain open. All three oracle versions, the initial
parent diagnostic differences and intermediate provenance-alias failure are
retained; corrections preserve every pre-public expected whole carrier. The
first generated native probe overflows its default stack before any full replay
call. Separating generated factories resolves it with the same 1 MiB stack;
both binaries and actual process witnesses are retained.

No production inputs change. The accepted core, all 660 frozen inputs and its
152 passing assertion-enabled Release suites are reused with byte bindings;
there is no fresh full build or suite run. All 269 evidence files round-trip
across sixteen archives. See
[forward-choice-complete-validation.json](forward-choice-complete-validation.json)
and [token-replay-parity-contract.md](token-replay-parity-contract.md).
Broader forward/backwards, adapter, cache/alias, search-bound and distinct
catalogue acceptance gates remain open, as do supplied-cache CLI/HTTP transports,
weighted aggregate/composite conformance and general PNML/precision. Totals stay
at 46/86 packages, 199/387 modules and 228 incomplete units, with no promotions
or full parity claim.

## Supplied-cache CLI and HTTP transport

The CLI accepts supplied hidden-path caches through
`--token-replay-places-shortest-path-by-hidden <json>`. The shared request field
uses the corresponding underscore name and maps place/transition model IDs to
complete ordered paths. Empty/null caches preserve suppression/automatic routing;
model-name checks remain deferred until a source/target path is examined. A new
standalone `POST /api/token-replay` multipart route accepts CSV/XES `log` and PNML
`model` uploads and emits the same full forward tagged carrier without a mining
session. Native JSON/schema carrier limits remain explicit in the replay contract.

Each actual transport executes 3634 inputs spanning all six retained cache-control
models and all seventeen weighted choice models. Actual cache objects map to
verified model IDs, every captured trace/attribute/event is preserved in XES,
and initial/final markings are checked. One PNML explicitly encodes the reference
input's added zero-token gate key; the other twenty-two remain byte exact.
All 14128 whole comparisons and 408 measured error comparisons against both
retained public routes pass. All 3634 CLI/HTTP pairs agree. No field mixing,
path/name swaps or relaxed float comparisons are used. Six further native
boundary pairs pass for CSV consistency and malformed/used/unused cache behavior.
The 7268 actual public calls and opaque pin proofs are reused with committed
archive bindings, with zero new public calls.

The fresh 660-input snapshot has seven edits and 653 unchanged inputs. All 155
linked production/test outputs are fresh, and all 152 assertion-enabled Release
suites pass. The original zero-replay Python harness failure is retained with
its stopped server process; its correction preserves production and frozen input
bytes. All 1143 evidence members round-trip across fourteen archives, and all
14536 raw process/request/response files separately round-trip in a lossless
nested file bundle. See
[forward-hidden-cache-transport-validation.json](forward-hidden-cache-transport-validation.json)
and [token-replay-parity-contract.md](token-replay-parity-contract.md).
Broader forward/input-adapter/cache-alias/search-bound and catalogue gates remain
open, as do weighted aggregate/composite conformance, general PNML limits and
exact frontend numeric presentation. Totals stay at 46/86 packages, 199/387
modules and 228 incomplete units, with no promotions or full parity claim.

### Exact fitness aggregation from weighted replay results

The new `evaluate_replay_fitness` overloads accept full token replay results or
trace rows directly, retaining exact signed counts and the original rows.
Fitting classification uses the supplied Boolean independently of trace fitness.
Outside-unit and nonfinite double fitness propagate. Empty/nonpositive aggregate
token denominators retain all-zero public scores and the integer-zero
`log_fitness` carrier flag. Exact integer division rounds to binary64 using the
significand and remainder, including subnormals, signed zero and overflow;
converting huge operands to double is unnecessary.

2486 fresh pinned aggregate calls cover 1243 frozen inputs through direct and
parent routes. These include the complete signed small-counter grid, huge
integer totals, nonfinite/outside-unit fitness, mixed/cancelling rows and 1040
byte-bound prior weighted replay observations. Both native carriers execute
2486 aggregate calls: all 4972 whole public-route comparisons match every
metric, carrier type and float hex. All 4972 independent comparisons, 2486
counter checks, 7458 input/retained-row checks and both sets of 1243 route pairs
match. A separate 2315-call native integer-division cohort matches actual Python
division outcomes. No fresh public replay calls are claimed.

The fresh 660-input build changes four inputs, links 155 fresh artifacts and
passes all 152 assertion-enabled Release suites. A failed comparer cannot read
MSVC's NaN display; its retained derived correction leaves every production,
probe and finite observation byte unchanged. The self-review is not an
independent review or a pre-public oracle. All 1026 retained files round-trip
across 12 archives; evidence and hashes are in
[weighted-fitness-aggregate-validation.json](weighted-fitness-aggregate-validation.json).

This accepts the declared C++ exact aggregation domain. Log-based/CLI/HTTP
weighted fitness dispatch and general marking carriers remain open, alongside
legacy bounded precomputed aliases, Python result-carrier/parameter domains,
composite/precision/application conformance and exact frontend presentation.
Catalogue totals remain 46/86 packages, 199/387 modules and 228 incomplete
units. No catalogue promotion or full parity claim is made.

### Exact log-based fitness through C++, CLI and HTTP

Token fitness now replays grouped logs and aggregates exact signed token
counts. Native exact markings preserve sparse zero keys and signed
initial/final counts. The PNML overload supplies document markings and IDs;
the PNML parser retains its separate nonnegative int64 marking limit.
Per-trace and aggregate rounding remain distinct as observed near 2^53.
The standalone CLI and cold multipart HTTP endpoint emit all four public
fields with integer/float carriers and exact float hex. Fitness-only details
leave precision, generalization and simplicity null. The frontend transports
the forward cleaning control.

3264 fresh pinned fitness calls, 90 marked-model full replay calls and 88
imports are retained. Native replay makes 3264 fitness and 90 marked replay
calls. All 4584 whole public comparisons in the declared native domain and
all 86 successful marked trace projections match, alongside the expected
errors. All 1632 native carrier pairs and 1632 public route pairs agree.
1632 actual CLI processes and 1632 HTTP requests make 4460 matching whole
public comparisons in the declared wire domain. Every CLI/HTTP pair and
native wire outcome agrees; 70 PNML carrier-error checks remain separate.

The final 660-source freeze changes 16 inputs and produces 155 linked
production/test artifacts. All 152 assertion-enabled Release suites pass,
as do TypeScript and 87 targeted frontend tests. Failed frontend/native/
diagnostic comparisons and both C1001 compiler failures are preserved with
derived corrections and unchanged-source compiler continuations. This is a
source/build/process/archive self-review, without independent-review or
pre-public-oracle claims. All 11185 files round-trip across 18
archives; [weighted-fitness-apply-validation.json](weighted-fitness-apply-validation.json)
contains the bindings and counts.

The broader original automatic-choice cohort remains open: 312 whole
dictionary differences are retained for each native and wire comparison,
outside the earlier fixed-cache proof. The raw PNML initial-zero-key mismatch
for path model 5 and signed/above-int64 marking transport also remain open.
AUTO, composite/precision/application conformance, precomputed aliases and
broader Python carrier/parameter acceptance are separate gates. Catalogue
totals remain 46/86 packages, 199/387 modules and 228 incomplete units. No
catalogue promotion or full parity claim is made.

### Exact PNML marking import for standalone token fitness

PNML document counts now use exact integers beyond 64 bits. An explicit public
marking import mode follows observed positive-only membership, duplicate and
multiple-final replacement, Unicode integer syntax and missing/empty-final rules.
The standalone fitness CLI and cold multipart HTTP endpoint use that mode;
native key-preserving document replay and bounded consumer range checks remain
explicit. Signed raw counts are dropped as observed, while large positive counts
reach exact fitness, including the observed 401-digit overflow boundary.

Two fresh public captures retain 304 imports,
2080 fitness calls and 1040
replay calls, including repeated observations in the 70-case and 82-case cohorts.
The final 58 successful and 24 rejected models produce
2288 matching whole native/public fitness
comparisons, 572 matching trace projections, and
116 exact marking/structural projections.
410 CLI processes and 410 HTTP requests
produce 1144 matching whole public fitness
comparisons. Every native carrier and wire pair agrees. Exact invalid-integer
error wording remains open for 22 models: 44
native import differences and 110 wire command-pair
differences are retained, alongside the matching NoneType and overflow messages.

The 660-input freeze changes seven sources, links 155 artifacts and passes all
152 assertion-enabled Release suites. The first test's code-page failure,
C4566 warnings and two passing UTF-8 diagnostics are retained; the corrected
fixture uses explicit UTF-8 bytes in a fresh build. All 3727 members
round-trip across 20 archives.
[pnml-exact-markings-validation.json](pnml-exact-markings-validation.json) records
sources, binaries, raw API/process/wire outputs and prior evidence bindings.

Full importer properties/syntax/order and Python exception identity remain
unproven. The previous 312 native and 312 wire automatic-choice differences,
AUTO/composite/precision/application conformance and broader carrier/parameter
gates remain open. This is self-review, with no catalogue promotion or full
parity claim. Totals remain 46/86 packages, 199/387 modules and 228 incomplete units.

### Exact PNML integer errors and text boundaries

Public PNML marking mode now reproduces invalid-literal quoting, Unicode 14
escapes and the first 200 rendered Unicode scalars, including trailing spaces.
It applies the observed default 4300-digit limit after validating the integer
grammar. Physical XML newline normalization, character-reference distinctions,
comment/CDATA joining and child/processing-instruction text boundaries match
the fresh calls. Native empty-text defaults and unbounded counts are retained.

The 334-case public capture and its 350-case extension retain
1368 imports, 8400 fitness
calls and 4200 full replays, including repeated cases.
All 66 Unicode decimal blocks are observed at initial and final marking positions.
The final 210 successful and 140 rejected models yield
280 matching native import errors,
8176 matching complete fitness dictionaries,
2044 matching trace projections and 420 exact marking/
structural projections. Every native carrier pair and expected overflow agrees.
1750 CLI processes and 1750 HTTP requests
produce 4088 matching whole public fitness
comparisons. Every wire/native outcome and CLI/HTTP pair agrees, with exact
truncated-error whitespace. The prior 22-model integer-wording gate is closed
in these observations, with no remaining wording differences in the new cohort.

All 152 assertion-enabled Release suites pass against 660 frozen inputs, with
two edited sources and 155 fresh linked artifacts. All 9457 files
round-trip across 15 archives.
[pnml-integer-errors-validation.json](pnml-integer-errors-validation.json) contains
the source, binary, public/runtime, transport and prior-evidence bindings.
No independent review, pre-public oracle or catalogue promotion is claimed.
Configured limits, Python exception identity and complete PNML syntax/properties/
order remain unproven. The previous automatic-choice, AUTO, composite/precision/
application and broader carrier/parameter gates remain open. Catalogue totals
stay at 46/86 packages, 199/387 modules and 228 incomplete units; full parity
remains unproven.

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


### Nullable minimum self-distance values and witness utility implementation

Minimum self-distance now retains a present null activity as a distinct value from `""`, `"None"` and `"null"`. Native distance records carry explicit null-activity and null-witness flags; their existing text fields remain available for string values. The witness-map key/member carrier is now `std::optional<std::string>`, and a supplied signed map accepts None/string keys with `std::int64_t` distances. The existing string/unsigned supplied-map overload remains available. Changing the witness-map type requires native callers to rebuild; the full production/test outputs are freshly rebuilt here.

Supplied witness derivation includes only observed activities with a supplied strictly positive distance, retains empty sets when no gap matches, and unions intervening activities for matching consecutive-occurrence gaps independently of the discovered global minimum. Zero/negative and unobserved keys are omitted. Supplying the discovered distance records derives minimum witnesses explicitly. The omitted-map utility route retains the observed pinned public `'dict' object has no attribute 'apply'` exception message before examining the log; its native C++ exception class is still distinct. Missing EventLog activity-key messages match the measured public KeyError text. Null stream/table/lazy semantics, broader scalar labels and unmeasured selector/error cases remain open.

The fresh library executes **1,869 actual native calls**, paired with the exact **1,869 existing pinned public calls** on 208 fixtures: direct/LOG-registry discovery, automatic utility calls, five exact supplied-map profiles including None keys and negative values, and 205 exact cached public distance maps. All **1,869 acceptance outcomes**, all **1,640 complete returned typed dict/set value projections**, and all **229 exact exception messages** match. All **229 native exception class identities** remain distinct and recorded. The 205 positive embedded minimum-witness projections also match the actual cached-map public calls; these field checks are not extra native calls. All native log, option and supplied-map inputs are preserved. Dictionary iteration order and Python/native container/object identities remain outside this value projection.

This closes both previously measured sets of **107 nullable discovery acceptance gaps**, the **98 automatic witness acceptance gaps**, the positive witness-map differences caused by extra zero-distance entries, and the None-key/signed supplied-map carrier gaps for the measured cohort. Raw zero-distance distance records remain present; the explicit witness utility omits those keys according to the public observations. The failed 1,064-counterexample hypothesis remains in the preceding committed baseline rather than being rewritten.

CLI/server registration skips the null activity value, and JSON emits `null` for a null selected activity or witness instead of using an empty activity id. An additional actual selected-phase cohort executes **60 CLI processes and 60 HTTP mining requests**. Each transport matches acceptance on all 60 fixtures, all **59 complete distance maps**, all **59 complete positive minimum-witness mappings**, and the one missing-key error message; there are no mapping or entry differences. This explicitly uses the declared native XES null marker and compares the entire measured minimum-self-distance values, not unrelated bundled model/conformance outputs.

The ordinary inductive cohort is also freshly repeated: **534 CLI processes plus 534 HTTP mining requests**. Each transport retains 525 matching acceptance outcomes, 294 complete matching ordered tree graphs and nine entry failures. The **18 existing entry gates** have moved from minimum-self-distance selection to **transition-system selection**, which now reports `transition system event is missing activity attribute 'concept:name'` on the same successful public null-singleton/optional fixtures 14, 90 and 94. These are retained consumer failures, not passing application results. Each transport retains 231 matched rejections, 12 exact matching error messages and 219 differing error messages. The next consumer needs its own pinned public evidence and implementation.

The fresh frozen source has **660 inputs, seven edits and 653 unchanged inputs**. Both core/MSD focused suites and **all 152 assertion-enabled Release suites pass**, with **155 fresh outputs** and exact compiler/source bindings. The initial preparer stage-baseline name collision, old inductive focused-filter failure, and MSVC access-violation crash compiling the unchanged Monte Carlo test are retained. The frozen producer is reused after verifying its inputs; a single-worker Monte Carlo retry and full build/test continuation succeed without a production-source change or erasing earlier logs. The accepted manifest binds the five successful configure/build/focused-test/full-build/full-test commands to that producer and retains all failure/continuation witnesses.

Current regressions also pass: 36 actual CLI processes match 36 ordered trees, 24 complete skeleton models and 48 complete skeleton-conformance values. The supplied-PNML smoke uses 84 actual CLI processes and 112 HTTP requests including 84 cached evaluations and 12 successful uploads; all 214 whole public quality comparisons match, alongside 24 marking and 12 export checks. Earlier wider/full transport cohorts remain historical for their respective sources; no new full cohort or new public calls are implied here.

Lossless evidence and exact source/output links are in `docs/nullable-msd-implementation-validation.json`. Every new member round-trips to its SHA-256, and the preceding three-part MSD baseline is hash-bound. The 18 transition-system application gates, all class/identity/order differences, wider adapters/scalars/selectors, nullable model conversion/replay/export and broader conformance remain open. Coverage remains 46/86 packages, 199/387 modules and 228 incomplete units; no catalogue promotion, independent review or full-parity claim is made.


### Nullable transition-system selection and complete returned graph baseline

The transition-system consumer that still blocks 18 ordinary CLI/HTTP mining calls is now measured with actual pinned public EventLog calls. The parent VIEW_BASED dispatcher and direct view_based variant execute **832 default calls** on the same 208 exact None/string fixtures, with default and include_data=True profiles. A separate capture executes **20,688 controls**: all 208 fixtures for all three views, both directions, windows 0/1/2/8 and both include_data values (19,968 calls), plus signed windows -1/-2/-8 on ten declared fixtures (720 calls). Total new public calls are **21,520**, with 300 observed KeyErrors (200 concept:name and 100 phase) and no log/parameter mutations. Four modules retain matching opaque pinned before/after SHA proofs. No implementation, bytecode, private fields or globals were inspected.

Present None values are successful transition names and state-name members, distinct from empty strings and the strings "None" and "null". The returned state names are lists, sets or Counters according to the view. Complete public graph/state/transition fields, every event-data entry and incidence reference, root membership, source trace/event aliases, and str/repr results are retained. The comparison preserves sequence order, set values, Counter multiplicities and ordered (source trace, event index) data; it does not discard unreachable endpoint objects or merge equal-valued state instances.

The first observer wrongly assumed that every transition endpoint belongs to the graph's root state set. It terminates with a KeyError after 12 complete journal records; a further public API call returned but its graph snapshot failed. The observer, failure logs and partial journal remain unchanged, and are excluded from the 21,520 completed-call count. A separately named observer captures the public endpoint/incident closure and root membership. Across the complete captures, **6,212 public calls contain endpoint-only state objects**, with **6,372 such instances**. This is observed returned-object behavior, including string-only repeat/zero-window cases; no cause is inferred from implementation.

The unchanged accepted native library executes **20,800 actual calls** through direct/view-based and VIEW_BASED registry routes: all 832 default calls and all 19,968 nonnegative controls. It rejects **10,700** successful nullable public cases. Of 9,800 paired successful returns, **7,088 complete graph value projections match and 2,712 differ**. The default cohort has 428 acceptance gaps, 380 matching graphs and 12 graph differences; the nonnegative controls have 10,272 acceptance gaps, 6,708 matching graphs and 2,700 graph differences. All 300 matched rejections retain differing native exception classes and messages. Every native log and option is preserved, and configure/build/run commands exit zero with assertions enabled.

Whole graph value projections compare the graph name, every state/transition name, root membership, endpoint instances, source/target links, incoming/outgoing incidence and all ordered state/transition event data. Native start/end/edge counts are retained raw as native extensions rather than invented upstream fields. Python container/class identities, set/dict iteration observations and str/repr are retained separately; the value comparison does not close language object or container identity. The 720 signed-window calls remain an explicit native unsigned-window API gate and are not counted as native calls. Broader scalar labels, selector aliases, invalid parameters and stream/table/lazy adapters remain open.

This evidence-only checkpoint changes no production source. It verifies all **660 unchanged accepted source inputs** and **155 unchanged outputs**, reusing the preceding **152-suite** result explicitly; no fresh full native suite or CLI/HTTP cohort is claimed. The preceding **18 application entry gates** remain open. Lossless public/native outcomes, endpoint snapshots, original observer failure, exact probe/compiler inputs/output and owned immutable helpers are listed in `docs/nullable-transition-system-baseline-validation.json`; every new archive member round-trips to its SHA-256. The preceding implementation archives and post-push proof are hash-bound. Coverage remains 46/86 packages, 199/387 modules and 228 incomplete units; no catalogue promotion, independent review or full-parity claim is made.


### Nullable transition-system values, endpoint instances and signed windows

Transition-system activity and state-payload values now carry `std::optional<std::string>`. A present null activity is distinct from an absent selected field, an empty string and the strings "None" and "null". The public native window option and positional overloads now use `std::int64_t`; CLI and HTTP parsing accept signed windows. These native type changes require callers to rebuild. The full source/output build is fresh for this checkpoint.

The owned graph retains every root and endpoint state instance, explicit root membership, Counter first-seen key order, transition endpoints and each state's incoming/outgoing edge references. Source and target lookups occur before root registration, allowing equal-valued endpoint-only objects; edge reuse retains its original endpoint objects while occurrence data is attached to the currently selected state instances. Forward negative windows follow the observed relative slice endpoints, and negative backward windows produce empty payloads. Missing EventLog key messages match the measured quoted KeyError text; native exception classes remain distinct. JSON emits a null activity directly, carries root membership/name order/incidence, and validates occurrence labels against the actual selected activity attribute.

There are **21,520 fresh native calls** paired with **21,520 reused pinned public calls**, covering the 208 exact None/string EventLogs, both native direct/registry routes, default profiles, three views, two directions, windows 0/1/2/8, include_data on/off, and 720 signed controls on the ten previously declared fixtures. All **21,520 acceptance outcomes** and all **300 error messages** match. The preceding 10,700 nullable acceptance gaps and unsigned-window carrier gate are addressed within these exact observations; no additional public calls are implied.

A stronger comparison requires a bijection of every returned state and transition object instance and independently checks every typed directed endpoint/incidence link, root membership, complete typed names and ordered source trace/event data. Counter key insertion order is included. Of **21,220 successful pairs**, **20,040 exact object graphs match and 1,180 differ**. All **820 successful default graphs** and all **7,620 sequence graphs** match. Set controls retain **176** graph differences; Counter controls retain **1,004**. The preceding baseline's 7,088/2,712 value counts used a coarser endpoint-key projection and are not presented as a subtraction from these stricter counts. A first owned graph-policy hypothesis failed 408 public observations and remains archived unchanged; the implementation is not claimed to reproduce every Counter/set lookup behavior. Python classes, container identities, raw set iteration, str/repr and native start/end/edge-count extensions remain separate gates.

Ordinary application mining is freshly repeated with **534 CLI processes and 534 HTTP requests**. Each transport matches 525 acceptance outcomes and all 294 successful ordered process trees. The same **18 null-singleton/optional entry gates** on fixtures 14/90/94 now fail in **Alpha mining**, after passing transition-system selection. Each transport retains 231 paired rejections, 18 matching messages and 213 differing messages. Successful transition-system library calls do not close these bundled application gates.

A separate selected-phase transport cohort executes **348 CLI processes and 348 HTTP requests**: all 60 phase fixtures under both default profiles, 48 nonnegative profiles on fixtures 179/189/193/203, and 36 signed profiles on previously captured fixture 207. Actual parameters and complete raw requests/responses are retained. Each transport has 236 matching acceptance outcomes, 234 successful exports whose complete decoded native graph fields match the actual library probe, 224 exact public graph matches, ten Counter graph differences, and two exact matching missing-key messages. Each transport also retains **112 export entry gates**, all reporting an unregistered blank/whitespace selected activity; these newly measured **224** gates remain open. The native explicit-null XES codec is declared and is not upstream importer parity.

All **660 frozen source inputs** are bound, with **nine edits and 651 unchanged inputs**. All **three focused suites**, all **152 assertion-enabled Release suites**, and the frontend typecheck pass; **155 fresh outputs** are bound to their source/compiler projects. The first full build's MSVC C1001 internal compiler error in the unchanged service-time test remains preserved. A single-target retry and full single-worker build/test continuation succeed without changing the frozen source. The initial helper-derivation failure and its separately recorded exit-code bookkeeping correction also remain retained.

Current regressions pass: 36 CLI processes match 36 ordered trees, 24 whole skeleton models and 48 whole skeleton conformance values. The supplied-PNML smoke executes 84 CLI processes and 112 HTTP requests, including 84 cached evaluations and 12 uploads; all 214 complete public quality comparisons match, with 24 marking and 12 export checks. Earlier wider transport runs remain historical.

Lossless evidence, exact source/output bindings and the preserved failure witnesses are listed in `docs/nullable-transition-system-implementation-validation.json`. Every new member round-trips to its recorded SHA-256; the 18 preceding archives bind the reused public calls without relabeling them as fresh. Alpha entry, blank/whitespace activity registration, 1,180 Counter/set object graphs, 300 exception-class differences, broader scalar/selector/adapter semantics, object/container order and identity, nullable model conversion/replay/export and wider conformance remain open. Coverage remains **46/86 packages, 199/387 modules and 228 incomplete units**. No catalogue promotion, independent review or full parity is claimed.


### Transition-system blank and whitespace activity registration

CLI and HTTP mining now register every present label from the discovered transition-system transitions in the shared activity table before export. This includes empty and whitespace strings. A null label remains separate and is emitted directly as JSON null. The preceding selector-registration helper skipped blank/whitespace values, leaving valid returned transition/state names without activity IDs. The two entry points now use actual returned model labels; there are no fixture-specific substitutions, trimmed labels or changes to discovery, public native types, generic selector registration or frontend schema.

The exact preceding selected-phase cohort is freshly repeated with **348 CLI processes and 348 HTTP requests** on byte-identical XES inputs and equal option profiles. Each transport matches all **348 acceptance outcomes**, all **346 successful complete decoded native graph fields**, **336 exact public object graphs**, and both missing-key messages. The same ten Counter object-graph differences per transport remain. Pairing every actual old/new call proves that all **224 blank/whitespace export gates close**, all **468 preceding successful exports remain accepted**, all **four paired missing-key messages remain exact**, and there are no new acceptance regressions. All **692 successful wire graphs** match the actual preceding library probe fields, including typed null versus blank values, root/endpoint membership, Counter key order, incidence and ordered occurrence coordinates. The native explicit-null XES codec remains a declared transport projection rather than upstream importer parity.

The full fresh build freezes **660 inputs**, with **two edited entry-point files and 658 unchanged inputs**, binds **155 fresh outputs**, and passes all **three focused suites** and **152 assertion-enabled Release suites**. All five configure/build/test commands exit zero. Library/header and frontend inputs are unchanged from the preceding implementation. Its 21,520 actual native calls, exact public graph comparisons and frontend typecheck are reused with source/output/archive bindings; they are not counted as new calls or checks here. No new public reference calls or standalone native discovery probe is claimed.

Ordinary mining is also freshly repeated with **534 CLI processes and 534 HTTP requests**. Each transport retains 525 matching acceptance outcomes, 294 whole matching ordered trees, 231 paired rejections, 18 matching error messages and 213 differing messages. The same **18 Alpha null-singleton/optional entry gates** on fixtures 14/90/94 remain. Current regressions pass: 36 CLI processes match 36 whole ordered trees, 24 skeleton models and 48 skeleton conformance values; 84 supplied-PNML CLI processes and 112 HTTP requests, including 84 cached evaluations, match all 214 complete public quality values plus 24 marking and 12 export checks.

Lossless current source/compiler/binary/request/response evidence and immutable helpers are listed in `docs/transition-label-registration-implementation-validation.json`. Every member round-trips to its recorded SHA-256, and the preceding archives, public/native observations and post-push proof are hash-bound. The **1,180 broader Counter/set object-graph differences**, **300 native exception-class differences**, Alpha entry gates, broader typed scalars/selectors/adapters and parameter ranges, Python object/container order and identity, nullable model conversion/replay/export and wider conformance remain open. The original automatic-choice fitness cohort still retains 312 native and 312 wire differences. Coverage remains **46/86 packages, 199/387 modules and 228 incomplete units**; no promotion, independent review or full parity is claimed.


### Complete nullable Alpha public and native baseline

Fresh pinned PM4Py 2.7.22 observations cover the existing 208 exact None/string EventLogs through Classic and Alpha+ parent/direct APIs, plus 32 declared missing/null/blank controls on default and phase selectors: **960 actual public calls**, **840 complete returned nets/markings**, **120 API exceptions**, no input/parameter mutations and no snapshot failures. Five opaque pinned module proofs match before/after each cohort. Every public net/place/transition/arc field, complete endpoint/incidence closure, root membership, exact sparse initial/final marking, detached marking key, alias, public str/repr and raw dynamic net name is retained. Only public APIs/fields/operators and opaque module hashes were observed; no upstream implementation, bytecode, private field or module-global inspection is claimed.

Classic returns for every base fixture, including isolated missing-key singletons. The missing-field controls show that adjacent pairs involving a missing selected key throw KeyError; simply dropping missing events is not supported. Both variants return nullable transitions for present-null values. Alpha+ rejects the empty EventLog with IndexError and missing-key inputs with KeyError. The complete returned observations retain **436 null transition names**, **792 null labels**, **432 blank labels** and **22 detached initial marking entries**. Null, blank and missing remain separate observations.

The unchanged current native library is called on all exact 240 fixtures through Classic parent/direct and Alpha+ parent/rich-result routes: **960 accepted baseline calls**, **385 returns** and **575 rejections**. All three configure/build/run commands exit zero, assertions are enabled by both the MSBuild NDEBUG undefinition and a compile guard, and all inputs/options remain unchanged. Exact typed weighted model graph comparison retains distinct places/transitions/arcs, root/closure membership, every endpoint/incidence link and initial/final token count, including detached rich marking places. Every accepted bijection is independently checked. The cohort has **505 matching acceptance outcomes**, **455 acceptance gaps**, **355 matching whole model projections** and **30 whole model projection gaps** among 385 paired returns; all 120 paired rejections have differing messages and native exception-class carriers. All 196 returned Classic model projections match; the 30 returned-model gaps are Alpha+ results on 15 base fixtures. No allocation-order explanation or graph equivalence erases those failures.

The observed acceptance gates comprise **436 present-null rejections**, **14 missing-field rejections where Classic publicly returned**, and **five Alpha+ parent convenience rejections of detached initial markings**. The rich Alpha+ route retains detached markings. The graph comparison is a declared projection: public symbolic place/transition names, dynamic net names, properties/classes, str/repr, raw object identity, sparse zeros and ordering/distributions remain separate gates on every returned snapshot. Public None-valued transition names cannot be substituted with blank strings. No full public-field graph match is claimed.

Two failed public preparations made zero reference calls. The first native helper failed compilation before making calls. A second native helper completed **960 actual calls** but its runner failed a post-run check that expected the literal /UNDEBUG spelling instead of MSBuild's UndefinePreprocessorDefinitions property; its raw results and missing execution metadata remain retained separately. A fresh corrected helper and directory produced the accepted 960-call baseline. Total fresh native observer calls, including that failed-validator cohort, are **1,920**; only the final 960 contribute to the comparison. No helpers, frozen inputs or failure logs were rewritten.

`docs/nullable-alpha-baseline-validation.json` lists lossless observations, all failures, probes, immutable helpers, exact comparison graphs/bijections and gate diagnosis. Every archive member round-trips to its SHA-256. All **660 application inputs and 155 existing outputs/project hashes are unchanged** from the pushed registration implementation; its **152 assertion-enabled suites, three focused suites and inherited frontend typecheck** are explicitly reused, with no fresh full-suite or typecheck claim. This evidence checkpoint changes no production source and closes no gate. The preceding **18 ordinary Alpha entry gates**, **1,180 Counter/set transition-system graph differences**, **300 transition-system exception-class differences**, wider adapters/scalars/selectors/ranges, nullable conversion/replay/export and conformance remain open. The original automatic-choice fitness cohort still retains 312 native and 312 wire differences. Coverage remains **46/86 packages, 199/387 modules and 228 incomplete units**; no promotion, independent review or full parity is claimed.


### Classic Alpha preserves nullable activities and isolated missing events

Classic grouped-log discovery now interns typed optional activity values before building its footprint. A present null has its own activity identity and yields an unlabeled transition; blank strings remain visible activities with actual string IDs. Missing fields remain absent positions, so an isolated missing event produces no transition and a missing selected key in an adjacent pair still rejects. No artificial string sentinel, trimmed label, missing-event concatenation or fixture-specific branch is used. Materialization limits still apply before mining. The public string-normalization helper, Alpha+ and flat table/stream adapters retain their separate contracts.

The exact preceding Alpha baseline is repeated against the new native library with **960 actual calls** on byte-identical generated probe/fixture files and equal options. The **960 pinned public observations are reused**, with no new reference calls claimed. Before/after pairing closes all **232 measured Classic acceptance gates**, preserves all **196 prior Classic successes** under exact typed weighted/root/endpoint/incidence/marking graph bijections, and retains the **52 paired Classic exception carriers** unchanged. All **428 returned Classic model projections** now match the actual public snapshots on the 208 base fixtures plus 32 missing/null controls through both parent/direct routes. Inputs and options remain unchanged.

Across both variants, the new cohort has **737 matching acceptance outcomes**, **223 remaining acceptance gaps**, **587 matching model projections** and **30 model projection gaps**. Alpha+ is measured separately; prior native/model changes, if any, remain recorded in the before/after journal without an allocation-order or distribution-equivalence claim. All **120 paired public/native rejection messages and native exception-class carriers** remain gates. Exact projected graph matching retains distinct arcs, typed null/string labels, weighted topology, root/closure membership and detached marking places; every mapping is independently checked. Full public symbolic names, dynamic net names, properties/classes, sparse-carrier details, str/repr, runtime identities and ordering/distributions are still independent gates.

The first full build passed the three focused suites but MSVC's CL.exe crashed with exit -1073741819 while compiling the unchanged OCEL statistics test. Its manifest, frozen inputs, logs, projects and existing outputs are retained. A **fresh retry on byte-identical 660 inputs**, with **three edited files and 657 unchanged files**, produces **155 bound outputs** and passes **three focused and all 152 assertion-enabled Release suites**; all five configure/build/test commands exit zero. Frontend inputs are unchanged, so the preceding frontend typecheck is explicitly inherited rather than rerun. No failed helper or frozen input was rewritten.

Fresh ordinary mining makes **534 CLI processes and 534 HTTP requests**. Each transport retains all 525 matching acceptance outcomes, 294 matching whole ordered trees, 231 paired rejections, 18 matching messages and 213 differing messages. The same **18 ordinary Alpha entry gates** remain because CLI/HTTP mining still invokes the strict string normalizer after native discovery. This native correction does not establish nullable decoration, replay, conversion or export semantics.

`docs/nullable-alpha-classic-implementation-validation.json` lists lossless current sources/compiler/binary evidence, all failed-build artifacts, actual native and application observations, exact graph mappings and complete before/after pairs. Every archive member round-trips to its SHA-256. The pushed nullable Alpha baseline archives and post-push proof are hash-bound. Broader Alpha+ acceptance/model gaps, Classic names/classes/carriers and exception behavior, flat adapters, selected scalar/alias/range behavior, nullable downstream operations, **1,180 transition-system Counter/set graph differences**, **300 transition-system exception-class differences** and wider conformance remain open. The original automatic-choice fitness cohort still retains 312 native and 312 wire differences. Coverage stays **46/86 packages, 199/387 modules and 228 incomplete units**; no promotion, independent review or full parity is claimed.


### Alpha+ retains typed null activities and place-name members

Alpha+ grouped-log discovery now interns actual selected activity values as typed optional strings. Present null has its own identity and yields an unlabeled transition; blank and literal "None" strings remain distinct visible activities. Every selected key is required, and materialization limits apply before discovery. Structural maximal-pair names retain actual null members, including detached marking entries. The native `AlphaPlaceName.before`/`after` vectors now contain `optional<string>`, an ABI/source-type change requiring a rebuild; rich JSON emits null members and the frontend type/response guards accept them. String normalization and flat adapters retain separate contracts.

The fresh complete native cohort makes **960 actual calls** on the same 208 base fixtures plus 32 missing/null controls and exact call/options loop. The generated probe differs only by its declared nullable place-name output adapter. **960 pinned public observations are reused**, with no fresh reference calls claimed. Before/after comparison closes **212 measured Alpha+ acceptance gates**, preserves all **617 prior native returns** under complete typed weighted/root/closure/endpoint/incidence/marking graph bijections, and preserves the **97 prior rich Alpha+ returns** under additional structural-name graph bijections. All **428 Classic model projections** remain matched. Of 131 paired native rejections, 125 retain identical exception carriers; six previously null-rejecting convenience calls now reach boundary cleanup and have changed messages. Every change is retained. The initial pairing validator incorrectly required all Alpha+ errors to remain identical; its failed helper and diagnostic are preserved, and a fresh validator records the changes without repeating native or public observations.

Across both variants/routes, the cohort now has **949 matching acceptance outcomes**, **11 remaining acceptance gaps**, **795 matching model projections** and **34 model projection gaps**. All 11 remaining acceptance gaps occur in the Alpha+ convenience parent route when explicit detached initial markings cannot be carried by its workflow-boundary net. Rich direct discovery retains those detached entries. All **120 paired public/native rejection messages/classes** still remain gates. Exact public name string carriers/formatting/order, dynamic net names, properties/classes, str/repr, sparse-carrier details, identities and allocation/order distributions are independent of the graph projections.

Rich Alpha+ transport observations are fresh both before and after: **240 CLI processes and 240 HTTP `/api/discover-alpha-plus` requests per phase**. All input XES bytes and CLI options match across phases; HTTP multipart requests match after replacing only their random boundary. Each transport closes **109 acceptance gates**, preserves all **97 previous returned weighted/structural-name models** and all **34 paired native error messages**, and now matches public acceptance on **all 240 fixtures**. Its **206 returned rich models** match the actual native named model graphs, including **135 actual null place-name members per transport**; **189** also match the pinned public model and structural-name projections, while **17** differ. The 17 consist of 15 retained previous model gaps and two newly accepted null fixtures, base 171/172, whose returned models are now comparable. No prior successful model is changed. Full public string-name identity/carrier/distribution behavior is still open. The explicit-null native XES marker is a declared codec, not a PM4Py importer parity claim.

A fresh full rebuild binds **660 inputs, seven edits, 653 unchanged files and 155 outputs**, and all five configure/build/test commands exit zero. **Four focused and all 152 assertion-enabled Release suites pass**. Fresh TypeScript compilation and **164 tests in two selected frontend files** pass against bound frontend inputs; no Vite production bundle or rendered UI inspection is claimed. Fresh ordinary mining makes **534 CLI processes plus 534 HTTP requests** and preserves 588 matching whole ordered tree projections. The same **18 ordinary Alpha entry gaps** remain at strict string normalization, with the complete rejection/message observations retained.

`docs/nullable-alpha-plus-implementation-validation.json` binds the complete source/build/binary observations, before/after graph mappings, typed name projections, frontend checks, ordinary mining, failed pairing validator, and the pushed Classic baseline archives/post-push proof. Every current archive member round-trips to its SHA-256. Nullable decoration, conversion, replay/export, flat table/stream and scalar/selector/alias/range behavior, Alpha+ model/exception/full-field gates, **1,180 transition-system Counter/set graph gaps**, **300 transition-system exception-class differences** and wider conformance remain open. The original automatic-choice fitness cohort still has 312 native and 312 wire differences. Coverage remains **46/86 packages, 199/387 modules and 228 incomplete units**; no catalogue promotion, independent review or full parity is claimed.


### Alpha+ expands original causal seeds and contained branch pairs

Alpha+ now expands each original causal singleton seed against the later candidates accumulated before that seed's turn. A containment on either side permits merging both sides when their activity sets remain admissible; the merged pair does not require every cross edge to be causal. Newly grown pairs do not become outer seeds, and dominated pairs are removed after expansion. For example, traces `[A,x]`, `[A,y]`, `[B,x]` now produce the combined `{A,B} -> {x,y}` place. Classic retains its complete cross-causality closure. Typed null, blank and literal string identities and nullable structural-name members are retained. New native tests cover contained branches, overlapping pairs, disjoint branches and typed null/blank values. The seed policy currently uses first-seen native relation order; reference hash-set iteration remains an explicit gate.

Fresh pinned public discovery observes every nonempty 3-by-3 bipartite relation across three disjoint typed label profiles, including custom selectors and nulls on either side: **1,533 fixtures and 6,132 actual Classic/Alpha+ parent/direct apply calls**, all returning complete snapshots with no API exceptions, input mutations or serialization failures. The existing 208 base fixtures and 32 missing/null controls contribute **960 reused public calls**. Auxiliary public preprocessing/get_relations calls total **3,091**: one completed preprocessing call whose serializer failed, 24 complete retry observations and 3,066 complete observations retaining the actual returned dict/set iteration. Public signature inspection performs zero discovery calls. Thus this step makes **9,223 fresh public API calls and retains 9,222 complete observations**, plus the one serialization failure. The failed helper assumed Event dictionaries where preprocessing returned primitive activity cells; its unchanged helper, process logs and prepared inputs are preserved, with no upstream rejection inferred. A separate stale native pointer attempt fails its helper hash before any configure/compiler/native command; that zero-call plan is also preserved. Fresh helpers recover both observations without editing executed helpers or frozen inputs. All five opaque pinned module proofs remain equal before/after. No implementation/source/bytecode/private-state inspection occurs.

Analytical alternatives and their counterexamples remain retained as analytical records, with zero additional public/native calls. The original-seed rule driven by **actual public returned relation iteration** matches all **3,066 public Alpha+ structural pair projections** in this cohort. That result is distinct from native graph comparison and does not establish a general rule for reference hash order, larger/cyclic logs, allocation or selection distributions.

Fresh native observations make **7,092 calls before and 7,092 calls after**, on byte-identical probe/fixture files and exact selected options. Complete typed weighted/root/closure/endpoint/incidence/marking comparisons close **2212 previously measured Alpha+ model gates**. All **4,623 prior public model matches**, including **3,494 Classic models**, remain matched and unchanged. All **6,961 prior native returns** are preserved; **4623** model projections are unchanged and **2338** change on previously failing Alpha+ cases. All **131 native rejection carriers** remain identical. Across the after cohort there are **7081 matching acceptance outcomes**, **11 convenience-route acceptance gaps**, **6835 matching returned models** and **126 model gaps**. All **120 paired public/native exception message/class gates** remain. Reference relation iteration and broader Alpha+ behavior remain open; public full names/properties/classes/str/repr/identity/order and sparse carrier details are separately retained.

Fresh rich transport captures make **1,773 CLI processes and 1,773 HTTP `/api/discover-alpha-plus` requests per phase**. Exact XES bytes and CLI options are paired; multipart bytes differ only in the declared random boundary. The combined transports close **2212 prior weighted/structural-name model gates** and preserve all **1,140 prior public named-model matches**, all **3,478 prior rich returns** and all **68 native error messages**. Each after transport matches native/public acceptance on all 1,773 inputs, returns 1,739 complete native named models and rejects 34 inputs; **1676 returned models also match the pinned public graph and typed structural-name projections**, with **63 retained gaps**. Actual typed-null structural-name members total **1527 per transport**. Public string formatting/order/carriers and distributions remain independent gates. The explicit-null XES marker is a declared native codec and does not establish upstream importer parity.

The retained native model gaps comprise 124 branching route observations (four string-profile and 120 custom-selector/null-left observations) plus both routes of base fixture 51, which contains nested paths and loops. The complete actual public/native named graphs for base 51 and all remaining fixture records are retained. This attribution does not by itself prove the cause of every gap or a general hash-order law.

A fresh rebuild binds **660 inputs, two source edits, 658 unchanged files and 155 outputs/project hashes**. All five configure/build/test commands exit zero; **four focused and all 152 assertion-enabled Release suites pass**. Every frontend input is byte-identical to the pushed parent: its TypeScript compile and **164 tests in two selected files are inherited**, with zero fresh frontend typechecks/tests, production bundles or rendered UI inspections. Fresh ordinary mining makes **534 CLI processes plus 534 HTTP requests**, retains **588 matching whole ordered tree projections** and the same **18 strict-normalization Alpha entry gaps**, with complete rejection/message observations preserved.

`docs/alpha-pair-merge-implementation-validation.json` binds all fresh/reused observations, graph bijections, actual public relation iteration, failed analytical hypotheses, both failed attempts, immutable helpers, sources/projects/binaries/logs, inherited frontend checks and the pushed parent archives/post-push proof. Every current archive member round-trips to its byte count and SHA-256. The remaining relation-order/convenience/model/exception/full-carrier gates, nullable decoration/replay/conversion/export, flat adapters, scalar/selector/alias/range behavior and wider conformance remain open. Earlier transition-system 1,180 Counter/set graph gaps, 300 exception-class differences and automatic-choice fitness 312 native/312 wire differences remain separately scoped. Coverage stays **46/86 packages, 199/387 modules and 228 incomplete units**. No catalogue promotion, independent review or full parity is claimed.


### Alpha+ canonical string relation-set ordering

Alpha+ now uses an independently written canonical seed-zero SipHash13 string hash and open-addressed set ordering for its target relations. Unicode labels use their narrowest code-point width with explicit little-endian bytes. Follows sources retain their first-seen order. Each follows target set is constructed and iterated, filtered for causal membership, then reconstructed as a separate causal target set before original-seed pair expansion. Both directions of a length-two loop are causal only when both alternating patterns occur. Classic closure remains unchanged. For traces `[A,z]`, `[B,x]`, `[B,z]`, `[C,x]`, `[C,y]`, the Alpha+ interior pairs now match `{A,B,C} -> {x,z}` and `{B,C} -> {x,y,z}`. Native regression tests check this overlapping case and a separate case that collapses to one combined pair.

The canonical string policy is limited to seed zero. Rows containing an actual null label or invalid UTF-8 retain their first-seen native order. A null reference hash depends on runtime identity, so its ordering is not claimed portable. Nonzero hash seeds, wider set sizes and reference allocation/selection distributions remain explicit gates. The independent analytical hash/table hypotheses match **1,676 string hashes**, **1,828 constructed set profiles** and all add-prefix observations in those profiles across public builtin seeds 0, 1, 42 and 4294967295. These builtin operations are separate from PM4Py discovery counts. A fresh assertion-enabled probe compiles the actual frozen production functions and matches **419 seed-zero string hashes**, **446 string-only set profiles** and **18734 individual add-prefix iterations**. It also verifies five declared invalid-UTF-8 hash rejections and one declared null-row fallback. An initial probe preparation omitted labels present only in set profiles and failed before writing a probe or running any compiler/native command; the fresh helper includes the union of observed labels while checking individual hashes only where an actual hash was captured. The unchanged failed helper, empty preparation directory and failure record remain retained. Those native policy checks do not establish reference behavior for unsupported input domains.

Fresh pinned PM4Py observations total **4,755 API calls**: **3,527 preprocessing/get_relations calls** across 1,780 fixtures, **24 helper calls and eight Alpha+ apply calls** on processing controls, and **1,196 Classic/Alpha+ parent/direct apply calls** on 299 wider fixtures. The wider fixtures include larger sets up to eight target labels, forward/reversed edge order, BMP/astral Unicode, custom activity selectors, null/blank/literal identities and cyclic controls. All 1,196 wider discovery calls return complete snapshots with no exceptions, input changes or serialization failures. The relation helper capture retains **33 actual API exceptions and 1,779 preprocessing mutation records**, including partial changes on failed preprocessing; these are distinct from whole-apply behavior. All five opaque pinned module proofs match before/after. Public-source/bytecode/private-state inspection is absent. A first capture omitted the pinned runtime path and failed before any API call; its helper, inputs and logs remain preserved. A wire run issued before preparation completed also made zero CLI/HTTP calls; its completed preparation and failure record remain preserved. Fresh immutable helpers recover both attempts.

Analytical alternatives and their failed membership/ordering hypotheses remain retained. The refined two-pass relation model matches all **1,747 actual returned follows containers and 1,747 causal containers**, and the original-seed expansion matches all **eight sampled public processing pair sets** when supplied actual returned relation iteration. The acyclic branching policy analysis closes 98 pair-name differences with zero prior pair-name regressions. These are analytical comparisons, not native final-model or universal ordering/distribution proofs. The main native/transport comparisons bind **7,092 reused public apply calls plus the 1,196 fresh wider calls**; the separate processing/helper/builtin counts are not added to their source cohort.

There are **8,288 fresh native calls before and 8,288 after** with byte-identical generated probes/fixtures and exact options. Complete typed weighted/root/closure/endpoint/incidence/marking graph bijections close **200 prior Alpha+ model gates**. All **7,947 prior public model matches**, including **4,092 Classic models**, remain matched and unchanged. All **8,157 prior native returns** and all **131 native rejection carriers** are preserved. After the change there are **8277 matching acceptance outcomes**, **11 convenience acceptance gaps**, **8147 matching returned model projections** and **10 returned model gaps**. The 120 paired public exception class/message carrier gates remain. Full public fields, classes/properties/names/repr/identity/order, sparse carriers and selection distributions stay independently scoped.

Fresh rich transports make **2,072 CLI processes and 2,072 HTTP `/api/discover-alpha-plus` requests per phase**, pairing exact XES/options and multipart payloads except declared boundaries. Combined transports close **200 prior weighted/structural-name model gates**, preserve **3,866 prior public named models**, all **4,076 prior returns** and **68 native error messages**. Each after transport matches all native/public acceptance outcomes, all 2,038 native named returns and **2033 pinned public named-model projections**, with **5 retained public model/name gaps**. Full public string-name formatting/order and object carriers remain separate; the native explicit-null XES codec does not imply upstream importer parity.

All **660 frozen inputs (two edits/658 unchanged), 155 outputs/projects, four focused and all 152 assertion-enabled native suites** are bound. Fresh ordinary application checks make 534 CLI and 534 HTTP calls and retain 588 whole ordered tree matches with the same 18 Alpha normalization entry gates. Every frontend input is unchanged, so the pushed parent TypeScript compilation and 164 tests are inherited explicitly; fresh frontend checks are zero. No production bundle or rendered UI inspection is claimed. Lossless public/native/wire/builtin/primitive records, failed attempts, analysis hypotheses and parent proofs are retained in `docs/alpha-relation-order-implementation-validation.json` and its verified archives. Wider nullable/downstream/transition-system/conformance, hash identity/seed and distribution gates remain. Catalogue totals remain **46/86 packages, 199/387 modules and 228 incomplete units** with no promotion or full parity claim.


### Alpha+ sparse discovery markings and removed-place identities

Alpha+ convenience discovery now returns nets whose initial marking refers to a place removed from the net's root set. `PetriNet::discovery_markings()` carries explicit sparse initial/final entries, each identifying either a live root place or a separate detached place. Detached identities are stored with opaque structured names; equal names or numeric IDs across the two namespaces do not merge places. Alpha+ embeds its actual result markings in the returned net and retains the existing rich DTO fields. A detached initial entry leaves `has_source()` false and does not create a phantom live source or mark a live same-name loop place. An empty trace now returns the single live end place plus a detached initial marking; all-loop traces retain their separate unmarked loop places. An event log containing no traces still raises the measured boundary-cleanup error. The default Classic path and the rich CLI/HTTP schemas retain their previous behavior.

The generic carrier accepts exact `TokenInteger` counts, including signed arbitrary-width values and explicit sparse zero keys. Both roles can refer to one detached identity. It owns its data across copy/move, validates root IDs, detached IDs and per-role duplicate keys, and preserves its previous value when validation fails. Assertion-enabled component tests verify those native API rules, equal-name distinct identities, overlapping numeric namespaces, a 40-digit token magnitude, negative values and zero keys. These component cases establish native carrier behavior; reference Alpha discovery behavior for unobserved token domains is not inferred.

Fresh pinned PM4Py observations make **2,432 actual Classic/Alpha+ parent/direct calls on 608 fixtures**, returning **2,128 complete results and 304 actual API exceptions** with zero input/parameter mutations or serialization failures. The fixtures repeat the 208 base and 32 missing/null controls under explicit cleanup false/true, then add 32 empty/boundary/loop/null/blank/Unicode controls under default/custom selected keys and both cleanup settings. Actual returned initial markings contain **204 nonroot keys**, including **112 whose names equal a live root place**. All 204 outside places have empty incidence sets. This cohort contains no detached final keys, shared detached initial/final keys or explicit zero marking keys; those broader reference domains remain unobserved. All five opaque pinned module proofs match before/after. Public implementation/source/bytecode/private-state inspection is absent.

The main comparison binds those **2,432 fresh public apply calls plus 8,288 reused public apply calls**, with **10,720 fresh native observations before and 10,720 after** on 2,680 fixtures. Exact selected fields and cleanup booleans, byte-identical generated probes/fixtures and a common 16 MiB test-harness linker stack are bound. The first expanded native harness exhausted the Windows default stack before producing any observations; its helper, inputs, successful compiler outputs, failed execution and empty record stream remain retained. A fresh helper uses the existing explicit token conversion and a larger harness stack. An earlier owned factory-template derivation also failed before creating inputs or running compiler/native commands and remains retained. Only terminal-successful calls contribute to the comparison.

The carrier closes **113 prior Alpha+ convenience acceptance gaps**, including the earlier 11 gaps and 102 expanded cleanup/selector controls. All **10173 prior public model matches**, including **5,204 Classic models**, remain unchanged and matched. All **10183 prior native returns** and all **424 continuing native exception carriers** are preserved. After the change, all **10720 acceptance outcomes** match, with **10286 matching complete typed weighted/root/closure/endpoint/incidence/marking graph projections** and **10 retained model gaps** on the existing nullable target-order cases. The 424 paired exception class/message carriers still differ. The actual returned carrier verifier checks **5,092 Alpha+ carriers**, **226 detached initial entries** and **2,546 independent direct/rich initial plus 2,546 final DTO correspondences**, without inventing a live source. These checks are separate from the public graph bijections and add no API calls.

Fresh rich transport phases make **2,680 CLI processes and 2,680 HTTP `/api/discover-alpha-plus` requests each**. Exact XES/selected keys/cleanup fields and multipart bodies are paired except declared boundaries. Every transport matches all native/public acceptance outcomes and all **2,546 native named returns**; each retains **2,541 pinned public model/name matches and five known nullable model/name gaps**. Combined phases preserve **5082 prior public named models**, all **5092 returned native named models** and **268 native error messages**. The rich endpoint structure remains unchanged. Full Python string-name formatting, classes/properties/repr/identity/order and allocation distributions are separate gates; the explicit-null native XES codec does not imply upstream importer parity.

All **660 frozen inputs (six edits/654 unchanged), 155 outputs/projects, five focused and all 152 assertion-enabled native suites** are bound. Fresh ordinary application checks make 534 CLI and 534 HTTP calls and retain 588 ordered tree matches and the same 18 Alpha normalization entry gates. Frontend inputs are unchanged, so the pushed parent TypeScript compilation and 164 tests are inherited explicitly; fresh frontend checks are zero. Lossless public/native/carrier/transport records, failed attempts, inputs, binaries/projects and parent proofs are retained in `docs/alpha-marking-carrier-implementation-validation.json` and its verified archives. Downstream replay/conformance/conversion/export propagation of detached marking keys, wider sparse/closure carriers, nullable ordering and allocation/selection distributions remain open. Catalogue totals remain **46/86 packages, 199/387 modules and 228 incomplete units**, with no promotion or full parity claim.


### Replay of actual Alpha+ discovery markings: public capture and native baseline

The next downstream gate is replay consuming `PetriNet::discovery_markings()` instead of deriving tokens from workflow source/sink indices. This checkpoint records the complete public behavior before changing replay. It makes no production change and closes no catalogue unit. Detached place identity, sparse membership and every observed failure remain separate requirements.

The pinned PM4Py 2.7.22 runtime makes **19,456 fresh Alpha+ discovery calls and 16,256 actual conditional replay calls**, for **35,712 fresh public API calls**. The exact 608 marking fixtures run under 32 explicit profiles: forward/backwards, parent/direct, token cleaning false/true, place-level diagnostics false/true and name-return false/true. Every profile obtains a fresh actual public net and markings before replay, preventing cross-profile mutation. Discovery produces **16,256 returns and 3,200 exceptions**; replay produces **8,704 returns and 7,552 exceptions**. All seven opaque pinned module proofs match before/after. Complete root/closure models, sparse marking references, public properties/classes/names/representations, returned fields, ordered carriers, set membership and iteration, exception classes/messages/public arguments, log and parameter inputs, signatures and enum values are retained. Serialization failures are zero; reference implementation/source/bytecode/private-state/global inspection is absent.

Replay leaves actual nonroot initial keys detached and counts their leftover tokens. Among outside-initial profiles, forward replay without place-level diagnostics returns on **752 of 816 calls**; forward replay with place-level diagnostics returns on **zero of 816 calls**. Error precedence includes mixed nullable/string transition-name sorting, blank or nullable transition keys and actual outside-place keys. Backwards replay returns on **544 of 1,632 outside-initial calls**, with **1,088 actual zero-division exceptions**. All returned outside-initial profiles have positive remaining-token counts. Cleaning and name-return controls remain individually bound; no universal outside-place error shortcut is inferred from these observations.

Backwards replay also mutates its supplied public transition objects: **7,680 model mutations**, on **5,280 returning calls and 2,400 failing calls**, add **29,040 `in_marking` and 29,040 `out_marking` Counter fields**. Logs and parameters show zero mutations. Backwards dictionaries omit `reached_marking` and retain their own public field set. Forward name-return dictionaries retain the `reached_marking` field while converting its keys to names and adding transition-label lists. An initial analysis helper incorrectly assumed every backwards row contained the forward marking field; its terminal failure and unchanged helper are retained. A fresh classifier preserves actual key presence and completes without adding API calls.

A fresh asserted C++ probe makes **16,256 actual native replay calls** against the unchanged accepted build. **381 frozen model descriptions** map each actual public root topology and sparse initial/final markings to native APIs. Detached entries retain their separate identity and never become root places. Both public routes remain individually identified; their corresponding native entrypoints are forward full-result and backwards trace replay. Public nullable transition names remain nullable in frozen descriptions; the native string-only option uses explicitly declared placeholders, leaving name-type/sorting/name-return/exception equivalence open. Full public models remain retained separately from the owned graph descriptions.

The native baseline returns **11,592 times and raises 4,664 exceptions**. It matches the reference accept/reject outcome on **13,368 of 16,256 calls**, compared trace projections on **7,061 of 8,704 reference returns**, and fitting/fitness/four-counter projections on **7,300 returns**. The trace projection preserves exact fitness values, counters, ordered firing/problem lists, enabled-set membership (name-return lists remain ordered), sparse reached-marking membership, labels and the backwards interrupted flag where returned. It does not establish Python classes, object aliasing, dictionary/marking insertion order, representations/properties, complete PLTR collections or full carrier parity. **2,758 exception semantic projections** match two explicitly translated native error shapes; actual Python exception classes and public argument objects remain gates. Complete native trace diagnostics, forward typed wire carriers and all failures remain retained, so numerical matches cannot conceal acceptance, carrier or mutation differences.

All **660 unchanged source bindings and 155 inherited output/project bindings** match the pushed parent. The parent's **five focused and 152 assertion-enabled native suites, TypeScript compilation and 164 frontend tests** are inherited explicitly; this checkpoint runs zero fresh suites or frontend checks. Lossless fresh public/native records, generated probe/inputs, compiler outputs, executable/project hashes, failed analysis, helper code and inherited proof bindings are stored in `docs/alpha-discovery-replay-baseline-validation.json` and its verified archives. The sparse default-marking consumer, detached token accounting, PLTR exception identity/precedence, nullable sorting, backwards public mutations, full runtime carriers, wider downstream conversion/export/conformance/quality and allocation distributions remain open. Catalogue totals remain **46/86 packages, 199/387 modules and 228 incomplete units**, without promotion or a full parity claim.


### Token replay consumes actual discovery markings

Token replay now uses `PetriNet::discovery_markings()` for default initial/final markings. Explicit root vectors still override each role; empty exact vectors select defaults. Default sparse entries preserve zero keys and signed, arbitrary-width token counts. Detached places occupy a separate owned marking domain after root slots; equal display names and overlapping numeric IDs cannot merge their identity. Replay counts their tokens without adding places/arcs to the root topology. Root arc endpoints are validated before extending this domain, preventing an invalid root endpoint from aliasing a detached slot.

Forward routing, token cleaning, reached-marking diagnostics and aggregate counts use the resolved domain. Discovery sparse insertion order is retained for returned markings, and name-return dictionaries collapse equal names by the actual insertion order. Typed forward wire output exposes detached namespace/ID/name references. Backwards replay uses sparse final membership even for zero or negative final counts and allows signed discovery defaults while retaining the existing validation of explicit backwards vectors. Typed place/blank-transition PLTR errors expose their model keys; runtime Python exception carrier equivalence remains a separate gate.

The pushed replay baseline is reused: **19,456 public Alpha+ discoveries and16,256 conditional public replays**, with the complete public models, sparse markings, returns, exception arguments and mutations retained in its verified archives. This step adds **5,632 fresh public replay calls** over constructed public nets: **3,584 detached-marking controls** and **2,048 zero/signed-final-key controls**. Their public constructors, sparse key/value identities, empty/rootless nets, duplicate names, custom/default selectors, cleaning, PLTR and name-return controls remain explicit. There are zero new discovery calls, serialization failures, log mutations or parameter mutations. Five opaque pinned module proofs per fresh cohort agree before/after, without reading reference implementation/source/bytecode/private state/globals.

The native comparison uses **21,888 calls per phase**. Before-change Alpha+ replay's **16,256 calls** are inherited from the pushed baseline; the two control cohorts add **5,632 fresh before calls**. All **21,888 after calls** run against the fresh implementation. Within each cohort, generated probe code, model descriptions and call tables are byte-identical between phases. Measurements use the original reduced trace and numerical projections:

- `alpha-discovery-replay`: **16,256 calls per phase**; acceptance **13,368 -> 15,504**, trace projection **7,061 -> 8,085**, fitting/fitness/four-counter projection **7,300 -> 8,500**. The paired comparison retains **7,061** previous trace matches and records **0** lost matches explicitly.
- `detached-replay-controls`: **3,584 calls per phase**; acceptance **2,272 -> 3,584**, trace projection **256 -> 2,112**, fitting/fitness/four-counter projection **352 -> 2,144**. The paired comparison retains **256** previous trace matches and records **0** lost matches explicitly.
- `detached-replay-final-keys`: **2,048 calls per phase**; acceptance **1,216 -> 2,048**, trace projection **0 -> 1,600**, fitting/fitness/four-counter projection **0 -> 1,600**. The paired comparison retains **0** previous trace matches and records **0** lost matches explicitly.

Complete raw native diagnostics, actual forward typed wire carriers, every native/public exception and public before/after mutation remain losslessly retained. The original raw reached-marking projection does not model equal-name dictionary collapse, leaving32 projected control-call differences. A separate verifier reads the actual emitted wire carriers: all **1,600 successful forward control calls /3,200 trace rows** match its declared trace projection, including sparse key order and actual equal-name collapse. It verifies **1,640 detached wire keys** across the three cohorts against their actual namespace/ID/name descriptions. Every earlier wire projection match is retained. Alpha+ still has **752 acceptance gaps and619 original trace projection gaps**, and its actual wire projection has872 call differences; all remain separately recorded.

Numerical and trace projection matches do not establish Python returned classes, object aliases, complete PLTR collections, dictionary field order, generated properties/representations, nullable-name sorting, exception classes/messages/public arguments or backwards transition `in_marking`/`out_marking` mutations. Sparse order measurements apply only to the compared successful forward wire traces. Those gates remain recorded even when native/public accept/reject outcomes agree.

Fresh validation freezes **660 inputs**, binds **155 native outputs/projects**, passes **five focused and152 assertion-enabled native suites**, and exercises **534 full CLI exports plus534 ordinary `/api/mine` requests**. Ordered public process-tree comparisons and the existing bundled-Alpha acceptance gates are retained. The earlier focused-test failure was diagnosed as an unset blank-event input; the reporting wrapper's compiler failure, a fresh-build pointer registration collision and an evidence metadata-key assumption are retained with the original helpers, frozen inputs and terminal outcomes. The metadata failure created no archives or new observations. Recovery used fresh helpers/directories. Frontend source is unchanged; TypeScript compilation and164 frontend tests are inherited, with zero fresh frontend checks.

`docs/discovery-marking-replay-validation.json` binds all inputs, outputs, paired comparisons, full application witnesses and verified lossless evidence archives. This is a committed implementation step, not full parity. Wider marking conversion/export/alignment/quality propagation, full runtime carriers, backwards public mutations and allocation distributions remain open. Catalogue totals remain **46/86 packages,199/387 modules and228 incomplete units**, without promotion.


### Replay retains actual nullable transition names

`PetriNet::transition_names()` now owns one optional string per transition, independently of activity labels. A null name and a blank string remain distinct. Copy/move preserves this metadata; setting an invalid dimension leaves the old metadata intact, and adding a transition after metadata registration extends it with the generated native name. Alpha+ discovery registers its actual nullable names, including actual reserved boundary names. Explicit nullable replay options take precedence over legacy nonempty string names; legacy names override net metadata, and otherwise generated names remain available.

Forward replay rejects incomparable transition names before selected-activity lookup or variant sorting, including zero-trace logs. It retains the observed first comparison's exact `NoneType`/`str` message direction, and two null names raise the observed `NoneType`/`NoneType` comparison failure. Backwards replay validates dimensions without comparing names and keeps the supplied transition order. Successful full forward output carries authoritative optional names and emits actual typed null names in transition objects and name-return lists. Legacy string views remain available but do not replace authoritative null values in the codec.

The cleanroom capture is fresh: **19,456 Alpha+ discoveries and31,616 PM4Py replay calls**, or **51,072 actual PM4Py calls**. The three cohorts contain608 discovery fixtures,288 parallel-net name controls and192 error-precedence controls, each with32 profiles. Their **31,616 additional actual builtin name-sort observations** are counted separately from PM4Py calls. Public root place/transition/arc set enumeration and every node's actual input/output arc enumeration are retained before/after, alongside complete names, labels, sparse markings, returned carriers, real exception classes/messages/arguments and mutations. Public APIs/signatures/enums/builtins and public fields are used without reading reference implementation/source/bytecode/private state/globals. Seven opaque pinned module proofs for discovery and five per control cohort agree before/after; serialization failures and log/parameter mutations are zero. Backwards public transition marking mutations remain captured as a gate.

The native before/after comparison makes **31,616 actual replay calls per phase** over3,164 owned model descriptions. Probe source, models, log fixtures and call tables are byte-identical; only the bound implementation inputs and the explicit after-phase nullable-name compile definition differ. Both phases construct transitions and ordinary arcs in the same actual public root/adjacency order. This differs from the earlier checkpoint's canonically sorted adapter, so adapter-only movement is not counted as an implementation gain. The unused initial10,679-description preparation is retained, with every constructor body verified unchanged by removal of its unused enumeration field.

Across the paired cohort, acceptance improves **27,872 -> 31,616**, gaining **3,744** matches. Every prior acceptance, original trace, numerical, scalar exception and actual forward wire match is retained; lost matches are explicitly enumerated and zero. Cohort measurements use the same declared reduced projections:

- `alpha`: acceptance **15,504 -> 16,256**, original trace projection **8,055 -> 8,055**, fitting/fitness/four-counter projection **8,500 -> 8,500**, and declared scalar exception projection **3,426 -> 5,680**.
- `controls`: acceptance **6,752 -> 9,216**, original trace projection **3,168 -> 3,168**, fitting/fitness/four-counter projection **3,344 -> 3,344**, and declared scalar exception projection **3,072 -> 5,760**.
- `precedence`: acceptance **5,616 -> 6,144**, original trace projection **848 -> 848**, fitting/fitness/four-counter projection **912 -> 912**, and declared scalar exception projection **2,496 -> 3,840**.

All **6,976 observed forward transition-name sort failures** now reproduce the independent builtin comparison message. Actual nullable net/option metadata is verified on all **31,616 after calls**, including failures. The raw trace comparison projects descriptor names in both phases; it does not conceal the before phase's placeholder wire names. A separate actual-wire verifier checks emitted transition name/label fields, exact scalar types, ordered transition/name lists, enabled-set membership, ordered sparse marking entries and equal-name dictionary collapse. Its matching forward calls improve **4,701 -> 5,085**, gaining **384** and losing zero. It verifies **1,224 actual null transition-object names**, **1,048 actual null name-list values**, and **552 detached namespace/ID/name keys**.

Residual measurements remain explicit:

- `alpha` retains **0 acceptance**, **649 original trace**, **204 numerical**, **435 actual forward wire-call**, and **1,872 untranslated exception** gaps.
- `controls` retains **0 acceptance**, **176 original trace**, **0 numerical**, **176 actual forward wire-call**, and **112 untranslated exception** gaps.
- `precedence` retains **0 acceptance**, **64 original trace**, **0 numerical**, **64 actual forward wire-call**, and **1,392 untranslated exception** gaps.

Matching outcomes and reduced projections do not establish runtime Python returned classes, object aliases, generated properties/representations, dictionary field order, complete PLTR tuples, returned set iteration, actual Python exception classes/argument objects, or backwards `in_marking`/`out_marking` mutations. Public transition-name types beyond the observed optional string domain remain unproven. The two declared C++ exception shapes map only to semantic categories and single-string argument projections. Wider conversion/export/alignment/quality propagation and allocation-dependent ordering/distributions remain open.

Fresh validation freezes **660 inputs with seven source edits**, binds **155 native outputs/projects**, passes **five focused and152 assertion-enabled native suites**, and runs **534 complete CLI exports plus534 ordinary `/api/mine` requests**. Ordered public process-tree comparisons and existing bundled-miner acceptance/error gates are retained. A preparation syntax failure created no build or observations; a subsequent compiler failure used the wrong local name table identifier and created no replay observations. Both original helper/source bytes and terminal outcomes remain preserved. Recovery uses fresh helpers and fresh frozen build directories. Frontend source remains unchanged, with TypeScript compilation and164 frontend tests inherited and zero fresh frontend checks.

`docs/nullable-replay-names-validation.json` binds all source/output/capture/probe/application measurements and **91 lossless evidence archives /4,328 round-tripped members**. This is a committed implementation checkpoint; full parity is still false. Catalogue totals remain **46/86 packages,199/387 modules and228 incomplete units**, without promotion.


### Forward replay selects enabled matches in actual transition order

Forward replay now scans matching transitions in native creation order and fires the first enabled match. The cleanroom native adapter constructs that order from the reference net's actual root transition-set enumeration. The disabled fallback retains the last transition in stable name-ascending order, with the last creation index breaking equal-name ties. For two enabled transitions with names `B,A` and the same activity, replay therefore fires the first constructed transition `B`; with neither enabled it uses the name-based fallback. Tests cover both enabled, only the second enabled by a join guard, neither enabled with reversed names, and equal-name enabled/fallback choices. This changes two source files and does not establish allocation-independent correspondence between separately created discovery nets.

The new public capture makes **32,768 actual PM4Py replay calls** over **1,024 fixtures and32 profiles each**: four name patterns, four label assignments, four shared/split/join guards, four sparse initial markings, two activity values, and default/custom selectors. Logs contain an empty trace, repeated single-event traces, and a double-event trace. Both replay variants, parent/direct routes, cleaning, PLTR, and name/identity output are captured. There are **12,544 returns and20,224 errors**, **16,384 backwards model-mutation profiles**, zero serialization failures, and unchanged log/parameters and root/adjacency enumeration. All **1,792 first-visible-event observations** support root-first enabled choice followed by the name-based disabled fallback. Five opaque pinned module proofs agree before/after. Public APIs, signatures, enums, builtins and returned public fields are observed without reference implementation/source/bytecode/private-state/global inspection.

Independent **32,768 builtin name-sort observations**, **32,768 transition equality observations** and **65,536 transition hashes** are counted separately from PM4Py calls. Every constructed transition pair remains unequal and distinct even when names and labels agree. A separate name/label analysis makes9,808 builtin pair-hash observations and confirms pair membership in25,088 named rows, while equal-name/different-label order defeats three simple sorted-pair recipes. The wider Alpha+ cohort also contradicts universal name sorting. Pair-set iteration, public-representation-string-set iteration and name-set iteration fail fixed-point checks on actual emitted outputs. These rejected hypotheses and complete witnesses remain retained; enabled-name deduplication/order is still a gate.

The new controls make **32,768 native replay calls before and32,768 after**, using byte-identical probe source, descriptors, fixture logs and call tables with real nullable metadata enabled in both phases. The previous checkpoint's **31,616 current-source native measurements are reused as the regression baseline** and compared with **31,616 fresh after calls** on identical construction inputs. This is **64,384 fresh native after calls**. Acceptance matches every observed reference call in both groups. Every prior acceptance, raw trace, counter, declared scalar error and actual forward wire match is retained; lost-match index lists are zero. Raw trace projections retain their declared membership limits; a separate actual-wire verifier checks exact scalar types, ordered activated/problem/name lists, enabled identity membership, ordered sparse markings and equal-name dictionary collapse.

| Cohort | Raw trace matches before -> after | Counter matches before -> after | Forward wire matches before -> after |
| --- | ---: | ---: | ---: |
| selection | 11,663 -> 11,823 | 12,448 -> 12,544 | 11,663 -> 11,823 |
| alpha | 8,055 -> 8,055 | 8,500 -> 8,500 | 2,989 -> 2,989 |
| controls | 3,168 -> 3,248 | 3,344 -> 3,344 | 1,632 -> 1,712 |
| precedence | 848 -> 880 | 912 -> 912 | 464 -> 496 |

Remaining measured gaps are preserved, including errors whose accept/reject outcome agrees while the declared semantic/message projection differs:

| Cohort | Acceptance gaps | Raw trace gaps | Numeric gaps | Forward wire gaps | Untranslated error projections |
| --- | ---: | ---: | ---: | ---: | ---: |
| alpha | 0 | 649 | 204 | 435 | 1,872 |
| controls | 0 | 96 | 0 | 96 | 112 |
| precedence | 0 | 32 | 0 | 32 | 1,392 |
| selection | 0 | 721 | 0 | 721 | 3,840 |

Full returned Python classes/aliases/properties/representations and dictionary field order, complete PLTR carriers, actual Python exception classes/argument objects, backwards marking mutations, returned set iteration, remaining hidden routing, non-optional-string names, wider conversion/export/alignment/quality propagation and allocation distributions remain open. The native constructor maps this finite public enumeration; no full-runtime or whole-family promotion follows from these comparisons.

Validation freezes **660 inputs with two edits**, binds **155 fresh native outputs/projects**, and passes **five focused plus152 assertion-enabled native suites**. **534 fresh CLI exports and534 fresh ordinary `/api/mine` requests** retain the previous complete ordered-tree and bundled-miner gate measurements. Frontend sources are unchanged; TypeScript compilation and164 tests are inherited with zero fresh frontend checks. A failed public-analysis schema lookup and failed pure-text build-helper derivation produced no additional calls or builds, remain unchanged, and recover through fresh helpers. An inherited verifier scope sentence incorrectly describes placeholder names; the bound correction sidecar and actual source/preparations/records establish that both phases use real nullable metadata.

`docs/replay-selection-validation.json` binds every source/output/capture/probe/application measurement and **87 lossless evidence archives /3,463 round-tripped members**, plus the reused parent evidence. This is an implementation checkpoint. Full parity remains false; the catalogue remains **46/86 packages,199/387 modules and228 incomplete units**, without promotion.
