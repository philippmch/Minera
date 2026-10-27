# Full parity continuation — 2026-10-21–24

Resume from a fresh clone of the repository, branch `main`. The
`docs/**/*.jsonl.gz` evidence logs named below were removed from the history
on 2026-10-26 (nothing read them); the capture scripts in `scripts/` can
regenerate a bank when one is needed. The reference
venv and checkout can be rebuilt in a cloud container (see the README).

## Objective and authorization

Reach full pinned-reference parity, prioritizing usable features, then complete
fidelity. Keep all 86 packages and 387 modules in scope. The user explicitly
authorized parallel subagents and clean logical commits/pushes directly to
`main`. Do not ask again for routine implementation, tests or Git checkpoints.

Use only public black-box calls, public signatures/docs/enums and opaque binary
hashes. Never inspect reference implementation source, AST, bytecode or private
state. Do not replay retained calls. Preserve failed attempts and execute new
uncovered banks only in new output directories.

## Parity recheck fixes — 2026-10-27

A recheck on macOS found five problems that Linux CI could not see:

- F1: Apple builds of the vendored date/tz need `date/ios.h`; it and `ptz.h` are now vendored (see `third_party/date/README.minera`).
- F2: the genetic miner's repair step took a `uint64_t` matrix while its callers build `size_t` counts, which differ on macOS.
- F4: the portable time-zone backend now applies the TZif footer's POSIX rule after the last explicit transition (2037), so DST keeps recurring. `test_zone_database` checks 2030–2060 against Python `zoneinfo` for four zones, plus gaps and overlaps after 2037.
- F5: `second_offset` is only set for nonexistent or ambiguous local times.
- F3: the frontend shows precision that was not computed (`precision: null`, `precision_incomplete: true`) as unavailable instead of 0.0%.

For portability, `__int128` was removed (MSVC has none): `exact_divide.hpp` does correctly rounded int64 division, and timestamp interleavings use a seconds-plus-nanoseconds instant. CI gained a `native-cross` job that builds and runs ctest on macOS and Windows (MSVC). `long double` is still used in places; it equals `double` on macOS arm64 and MSVC, so any parity drift there will show up in that job.

## Parity review fixes — 2026-10-26

An independent review of `0cf509b` reported eight findings (R1–R8); all were
confirmed and fixed, each with a regression test:

- R1: named zones go through `src/backend/zone_database.*`. CMake probes
  `std::chrono::locate_zone`; Apple libc++ lacks it, so the vendored
  `third_party/date` (MIT, OS zoneinfo) is built there
  (`MINERA_PORTABLE_TZDB=AUTO|ON|OFF`). Test: `minera_zone_database_tests`.
- R2: standard tree-to-net conversion gives loops a private exit place, so
  `XOR(LOOP(A,B),C)` no longer accepts `C,B,A`. Languages are compared with
  transition-bordered conversion in `minera_petri_net_conversion_tests`.
- R5: dashboard token precision always discards invalid prefixes and keeps
  unknown labels distinct (reference: `A→B` with `B,A,B` is 0;
  `XOR(B,C)` with `X,B`/`Y,C` is 0.25, with `X,B`/`X,C` is 0.5).
- R6: the eventually-enabled silent search has an explicit budget (65,536
  expansions); exhaustion makes precision `null` with `precision_incomplete`
  instead of a low number. A 13-branch optional split now yields all 14
  labels and 1/14 like the reference. Test:
  `minera_precision_prefix_semantics_tests`.
- R7: label lists travel as `<field>_json` arrays from the frontend; the comma
  form remains for CLI callers.
- R3: Apply/Reset carry over only import settings (`source-import-options.ts`).
- R4: playout is offered only for the live mined source and every request
  sends `source_id`; `/api/playout` answers 409 when it no longer matches.
- R8: graph pan/zoom map pointers through the SVG meet fit
  (`graph-viewport.ts`), shared by all four graph views.

## ILP discovery — 2026-10-25

New bank `docs/ilp-discovery-bank.json.gz` (1,169 calls, about 12 minutes to
capture) passes in `minera_ilp_discovery_parity_tests` through the new
`reference_ilp_net` (one LP relaxation per causal pair rounded at 0.5, the
alpha prefix filter, place naming, murata implicit places, simple silent
reduction; model in the parity doc). 353 tie-free calls match exactly; for
the 795 calls whose pair LPs have tied optima, reference and native places
are checked to be optimal roundings. `mine_ilp` and its adapters are
unchanged (follow-up: wire `reference_ilp_net` into JSON/CLI/server).
Package promoted: 76/86 packages, 284/387 modules.

## Genetic discovery — 2026-10-25

New bank `docs/genetic-discovery-bank.json.gz` (5,742 calls, about 6 minutes
to capture plus two hash-seed replays) passes in
`minera_genetic_discovery_parity_tests` through
include/minera/genetic_reference.hpp: src/sink sets, matrix2petrinet,
partition utils and tournament fitness exactly; individuals, crossover,
mutate, repair and apply by invariants (model in the parity doc). Token
replay ties need the reference place names, which the bank records.
`mine_genetic` is unchanged (follow-up: route it through the reference blocks).
Package promoted: 78/86 packages, 306/387 modules (after rebase).

## Interleavings dataframe merge — 2026-10-25

New bank `docs/interleavings-merge-bank.txt.gz` (6,000 calls, about 13
minutes for three hash seeds) passes in `minera_interleavings_merge_parity_tests`
through the new include/minera/interleavings_merge.hpp
(`interleavings_directly_follows_dataframe`, `interleavings_merge_dataframes`;
model in the parity doc). The discovery-gaps thread (ILP, genetic, this util)
is finished; nothing of it stays claimed. Modules: 307/387 complete.

## Process-tree reduction — 2026-10-25

New bank `docs/tree-reduction-bank.txt.gz` (6,000 calls, eight hash seeds,
about a minute) passes in `minera_tree_reduction_parity_tests`.
`reduce_process_tree` (and its JSON/CLI/server adapters) now runs the
reference walk (`reference_reduce_process_tree`), including its structural
deduplication that can return a subtree, and the reference fold keeps the
last xor tau (the old native rule kept the first).

## Organizational-mining util — 2026-10-25

New bank `docs/organizational-util-bank.txt.gz` (4,000 cases, about a
minute) passes in `minera_organizational_util_parity_tests` through the new
include/minera/organizational_util.hpp (model in the parity doc).

## Precision utils — 2026-10-25

New bank `docs/precision-utils-bank.txt.gz` (3,000 calls, about 30 seconds)
passes in `minera_precision_utils_parity_tests` through the new
include/minera/precision_utils.hpp (`precision_log_prefixes`,
`precision_fake_log`, `precision_sync_search`; model in the parity doc).

## Element-usage comparison — 2026-10-25

New bank `docs/element-usage-bank.txt.gz` (2,000 calls on the 40 frozen
conformance nets, about four minutes) passes in
`minera_element_usage_comparison_parity_tests` through the new
`compare_element_usage_token_replay`, which composes the native forward
token replay with PLTR fitness the way the reference does. The CLI/server
comparison still calls `compare_petri_usage`; moving it onto the new
function is a possible follow-up.

## OCPN extensive playout — 2026-10-25

New bank `docs/ocpn-playout-bank.txt.gz` (200 mined logs, 1,418 compared
calls, about 25 minutes to capture, 4.5 minutes to run) passes in
`minera_ocpn_playout_parity_tests` through the new
include/minera/ocpn_reference_playout.hpp (one object per type, double arcs
optional in a binding, every transition bounded; model in the parity doc).
Package `algo.simulation.playout.ocpn` promoted. The older
`ocpn_extensive_playout` (its own object choices, caps and timestamps) still
backs the CLI/server.

## OC causal net extensive playout — 2026-10-25

New bank `docs/oc-causal-net-playout-bank.txt.gz` (800 seeded nets, 1,807
compared calls, about four minutes to capture, seconds to run) passes in
`minera_oc_causal_net_playout_parity_tests` through the new
include/minera/oc_causal_net_reference_playout.hpp (model and the KeyError
rules in the parity doc). Package `algo.simulation.playout.oc_causal_net`
promoted. The older native causal-net playout (its own caps and branching)
still backs the CLI/server.

## Process-tree DP alignments — 2026-10-25

New bank `docs/process-tree-dp-alignment-bank.txt.gz` (3,000 trees, 24,000
traces, seconds to capture and run) passes in
`minera_process_tree_reference_alignment_parity_tests` through the new
`reference_process_tree_dp_alignments` (alignments included; model and the
85 pinned reference cache artefacts in the parity doc). Module
`...process_tree.variants.dynamic_programming` promoted; the package waits
on SEARCH_GRAPH_PT, MILP and APPROXIMATED_ORIGINAL. The older
`align_process_tree` still backs the CLI/server.

## LLM SQL utils — 2026-10-25

New bank `docs/llm-sql-utils-bank.txt.gz` (6,000 mask/restore calls,
seconds) passes in `minera_llm_sql_utils_parity_tests` through the new
include/minera/llm_sql_utils.hpp. Module `algo.querying.llm.utils.sql_utils`
promoted.

## Trace-variant query anonymization — 2026-10-25

New bank `docs/trace-variant-query-bank.txt.gz` (5,720 calls: exp_mech,
BA relations and violations, generate_pm4py_log, 2,400 SaCoFa queries, 20
LAPLACE; under a minute to capture) passes in
`minera_trace_variant_query_parity_tests` through the new
include/minera/trace_variant_query_reference.hpp (model in the parity doc;
Python `random` and legacy numpy draws emulated in
src/backend/python_random.hpp and numpy_legacy_random.hpp). Package
`algo.anonymization.trace_variant_query` and its five partial modules
promoted. LAPLACE raises AttributeError in the pinned release for every
input, so the reference function raises the same. The older
`trace_variant_query.hpp` still backs the CLI/server.

## PRIPEL — 2026-10-25

The pinned requirements do not install diffprivlib, so every module of
`algo.anonymization.pripel` fails to import (ModuleNotFoundError "No module
named 'diffprivlib'"), whatever the input. Judgment call: the new
include/minera/pripel_reference.hpp functions raise the same, bound by
`docs/pripel-reference-bank.txt` in `minera_pripel_reference_parity_tests`;
package and its three partial modules promoted. Minera's working PRIPEL
(minera/pripel.hpp) is unchanged. Revisit if the pin set ever gains
diffprivlib (its mechanisms draw from secrets.SystemRandom, so only
invariants would be checkable).

## Trace-attribute clustering helpers — 2026-10-25

The nine helper modules of `algo.clustering.trace_attribute_driven`
(filter_subsets, act/suc_dist_calc, sim_calc, logslice_dist, leven_dist_calc,
dfg_dist, linkage_avg, merge_log) are ported black-box in
include/minera/trace_attribute_reference.hpp and bound by
`docs/trace-attribute-helpers-bank.txt.gz` (6,608 seeded calls over 47
functions, 0 failures) in `minera_trace_attribute_helpers_parity_tests`.
Reference quirks kept: act_sim/suc_sim weigh by sublog2df's variant-sorted
counts (IndexError past them), act_dist and merge_log always raise TypeError,
linkage_* need a square matrix and IndexError below two logs, clusteredlog
returns one (possibly empty) group per label 1..maxclust. Contracts: floats
within 1e-12; act_sim_med (numpy unstable argsort median) must hit one of its
tie choices; occu_suc may pick other successions within a frequency it takes
only in part. fcluster(maxclust) is emulated for monotonic linkages only.

## Search-graph variant and replay helpers — 2026-10-25

Bank `docs/process-tree-search-graph-bank.txt.gz` (60,000 replay calls,
7,200 `align_variant` runs, 3,072 public `apply` traces and 400 annotated
searches, about two minutes to capture) passes in
`minera_process_tree_search_graph_parity_tests` through
`include/minera/process_tree_search_graph_reference.hpp`. Modules
`...process_tree.variants.search_graph_pt`,
`...util.search_graph_pt_replay_semantics` and
`...util.search_graph_pt_frequency_annotation` promoted; the package is now complete (see the approximated entry). `apply`'s candidate order follows object
addresses with repeated labels, so the bank drives `align_variant` with a
preorder leaf list there and uses `apply` only on trees with unique labels.
The annotation rule was corrected: per alignment, ENABLED counts, or OPEN
counts when there is no ENABLED assignment.

## Process-tree MILP variant — 2026-10-25

Bank `docs/process-tree-milp-bank.txt.gz` (300 trees, 1,500 traces, graphs,
programs, CBC solutions and `apply` results) passes in
`minera_process_tree_milp_parity_tests` (about 14 s) through
`include/minera/process_tree_milp_reference.hpp`. Graph, programs and
solution extraction are exact; the native branch and bound matches costs and
fitness within 1e-6 (68 of 1,460 optima tie to another alignment). Module
`...process_tree.variants.milp` promoted. Infeasible programs (empty trace,
root loop with a tau do) throw natively where the reference reports CBC
leftovers. Remaining in the package: APPROXIMATED_ORIGINAL and its utils.

## Process-tree approximated variant — 2026-10-25

Bank `docs/process-tree-approximated-bank.txt.gz` (400 trees, 2,098 traces,
3,133 PuLP programs, 5,122 base alignments, about two minutes to capture)
passes in `minera_process_tree_approximated_parity_tests` (about 6 s) through
`include/minera/process_tree_approximated_reference.hpp`. Binary trees, sets,
programs, choices and compositions are exact; replaying CBC's solutions and
the base alignments reproduces apply exactly. The three approximated modules
are promoted and `algo.conformance.alignments.process_tree` is complete with
all four selector proofs. CBC's ties (splits) and object-address ties (base
alignments) are not reproduced natively; the fully native run gives the
reference cost for 1,733 of 1,932 traces.

## Decomposed RECOMPOS_MAXIMAL — 2026-10-25

Bank `docs/decomposed-bank.txt.gz` (400 tree nets, 765 components, 2,144
component alignments, 1,402 traces, about a minute to capture) passes in
`minera_decomposed_parity_tests` (about 6 s) through
`include/minera/decomposed_reference.hpp`. Decomposition and merges are
exact; replaying the reference's component alignments reproduces every
align/merge/recompose call and every apply result exactly, and the 82 logs
where the reference loops forever (a link cycle in the first recomposition
queue) raise natively. Module and package `algo.conformance.alignments.decomposed`
promoted. The older `align_decomposed` used by the CLI and server is unchanged.

## Connectors, LLM connectors and embeddings — complete (2026-10-25)

Thread "Close remaining algorithm parity gaps" owns `algo.connectors.*`,
`algo.transformation.to_embeddings.*` and `algo.querying.llm.connectors.*`.
Chrome and Firefox history, Camunda and both SAP connectors are bound to
`docs/connector-bank.txt.gz` (`minera_connector_parity_tests`, 1,200 seeded
databases plus 900 recorded database-connector calls, exact frames, SQL text,
connection close and exception types) through
`include/minera/connector_reference.hpp`. The database connectors are
read_sql of a fixed SQL text plus format_dataframe; the bank found three
native `format_dataframe` gaps (compact dates, conversion before dropping
incomplete rows, date-like canonical identifiers), fixed in
`src/backend/dataframe_format.cpp`.

All of `algo.connectors` is now complete: GitHub (600 stubbed repositories,
every GET and frame exact), Outlook calendar and mail, Windows events and
util.mail (win32com stubbed, 1,800 cases plus 60 connect call sequences).
The package selectors are proven. Native GitHub date parsing accepts the
ISO 8601 forms the API writes rather than dateutil's whole grammar.

The three LLM connectors are complete against `docs/llm-connector-bank.txt.gz`
(`minera_llm_connector_parity_tests`, 2,400 stubbed POSTs and answers).
`algo.transformation.to_embeddings` is complete against
`docs/embedding-bank.txt.gz` (`minera_embedding_parity_tests`, 3,000 calls
with a stub model of dyadic float32 vectors). Claim released.

## In progress — remaining algorithm gaps (claimed 2026-10-25)

Thread "Close remaining discovery parity gaps" now owns
`algo.reduction.process_tree.*`, `algo.organizational_mining.util`,
`algo.evaluation.precision.utils`,
`algo.comparison.petrinet.element_usage_comparison`,
`algo.clustering.trace_attribute_driven.*`,
`algo.simulation.playout.{ocpn,oc_causal_net}`,
`algo.conformance.alignments.{process_tree,decomposed}` and
`algo.anonymization.{pripel,trace_variant_query}`. Other threads: please pick
different packages.

## In progress — filtering gaps (claimed 2026-10-25)

Thread "Close remaining filter and log parity gaps" owns the partial
`support:filtering` modules (`algo.filtering.{common,dfg,log,pandas,polars}.*`).
Other threads: please pick different packages.

## Clustering and features — complete (2026-10-25)

Thread "Continue parity toward 2.7.22" did the OCEL LLM abstractions and
`algo.simulation.montecarlo`. The `algo.transformation.log_to_features`
leftovers were finished by thread "Close remaining algorithm parity gaps".
`variants.temporal_lazy` is bound to `docs/temporal-lazy-bank.txt.gz`
(`minera_temporal_lazy_parity_tests`, 4,575 cases, polars 1.44.1 installed
into the cloud reference venv). The Polars group means are summed in a
thread-dependent order, so the test accepts any order of the bank's case
values. `util.locally_linear_embedding` is bound to `docs/lle-bank.txt.gz`
(`minera_lle_parity_tests`, 2,000 cases). Smoothing and features are
exact, and the embedding replays scikit-learn's tie-broken neighbours. The
verifier now reports all 86 packages and all 387 modules complete.

## LLM log abstractions — 2026-10-25

New bank `docs/llm-abstractions-bank.json.gz` (2,700 calls over columns, DFG,
variants, features, stream and log-skeleton descriptions) passes byte for byte
in `minera_llm_abstractions_parity_tests` through
`include/minera/llm_abstractions.hpp`. The capture builds a fresh log per call
because the public abstractions mutate their input. Native feature inference
now one-hot encodes mixed string/number attributes. Six support modules
promoted: 283/387 with the filtering bank. Left in `algo.querying.llm.abstractions`: ocel_fea_descr and
ocel_ocdfg_descr.

## Monte Carlo simulation — 2026-10-25

New bank `docs/monte-carlo-numpy-bank.json.gz` (240 seeded public
simulations, 427 distinct outcomes) passes in
`minera_monte_carlo_numpy_parity_tests` through the new
`MonteCarloOptions::numpy_legacy_seed` (numpy legacy generator, reference draw
order) and `numpy_choice_order`. Native `total_cases_time` and hidden-firing
timing in map fitting were corrected. One outcome stays a pinned residual
(resource wait, mc-081). Package and both modules promoted. Follow-up: expose
the seeded mode in JSON/CLI/server. Next for this thread: log_to_features
leftovers (temporal_lazy, LLE util).

## OCEL LLM abstractions — 2026-10-25

New bank `docs/llm-ocel-abstractions-bank.json.gz` (1,794 calls over the
OCDFG and object-feature descriptions) passes byte for byte in
`minera_llm_ocel_abstractions_parity_tests`. Native object degree centrality
now normalizes over interacting objects only, as the reference does. The
whole `algo.querying.llm.abstractions` support set is complete. Next for this
thread: `algo.simulation.montecarlo`, then the log_to_features leftovers.

## Profile clustering — 2026-10-25

New bank `docs/profile-clustering-bank.json.gz` (300 logs) passes in
`minera_profile_clustering_parity_tests` through `reference_profile_clustering`
(k-means k=2, seed 0, on default trace features, numpy-compatible MT19937
k-means++). 7 logs hinge on exact distance ties broken by OpenBLAS rounding;
those need only a k-means fixpoint. Package promoted: 75/86.

## Trace-attribute clustering DMM_VEC — 2026-10-25

The open DMM_VEC rule is found: equal variant lists (ordered by descending
frequency, then descending sequence) give a zero succession distance. The
bank's 73-call exemption is gone and all 1,200 calls pass (3 near-tie vector
trees remain tolerated). Package promoted: 74/86.

## Log to features — 2026-10-25

New bank `docs/log-to-features-bank.json.gz` (960 calls, a few seconds)
passes in `minera_log_to_features_parity_tests` for EVENT_BASED, TRACE_BASED
and TEMPORAL (weekly and daily). One native fix: absent-activity index
features are -1. Package promoted (73/86); temporal_lazy and the LLE util
stay partial.

## Common and DFG filtering helpers — 2026-10-25

New bank `docs/filtering-common-bank.json.gz` (4,000 cases, 33,747 public
calls over `algo.filtering.common.{filtering_constants, timestamp_common,
attributes_common}` and `algo.filtering.dfg.dfg_filtering`; about 2.5 minutes
to capture) passes in `minera_filtering_common_bank_parity_tests`. New native
API: `filtering_common.hpp` (constants, `get_dt_from_string`, attribute
thresholds, numeric/date KDE replicating scipy's gaussian_kde and numpy's
grids) and `dfg_filtering.hpp` (the reference-shaped DFG filters, next to the
old `dfg_filter.hpp`). KDE densities use a conditioning-based tolerance
because BLAS covariance sums are not portable; everything else is exact.
Activity names `_S_START_`/`_S_END_` are outside the domain. Details in
`docs/filtering-common-domain-parity.md`. All four modules promoted (290/387 after rebasing on the other threads).
Next in this thread: the pandas filter modules, then Polars.

## Pandas sequence filters — 2026-10-25

New bank `docs/pandas-sequence-filter-bank.json.gz` (3,000 frames, 33,820
public calls over pd_filtering_constants, start/end_activities, traces,
starts_with, ends_with, rework, prefixes, suffixes,
consecutive_act_case_grouping, timestamp_case_grouping and activity_split;
about 4 minutes to capture) passes in
`minera_pandas_sequence_filter_bank_parity_tests`. New native API:
`pandas_sequence_filters.hpp` over `EventTable`, with typed Python arguments
and Parameters-member keys. Notable reference quirks reproduced: the two-row
`groupby([k1, k2])` array grouping when a key is not a column, and numpy's
object argsort TypeError on str case ids mixed with nulls. Details and
domain limits are in `docs/pandas-sequence-filter-domain-parity.md`. All
twelve modules are promoted.
Next in this thread: the pandas attributes, between, cases, ltl, paths,
timestamp and variants filters, then Polars.

## Pandas attributes/between/cases/ltl/paths/timestamp/variants — 2026-10-25

New bank `docs/pandas-filter-bank.json.gz` (3,000 frames, 87,971 public calls
over all 28 functions of the seven modules; about 20 minutes to capture)
passes in `minera_pandas_filter_bank_parity_tests`. The pilots with seeds 7
and 9 pass too. The native APIs are the existing per-module headers. New
shared helpers: `pandas_parameter_carrier.hpp` (the `parameters` carrier),
`numpy_object_sort.hpp` and `python_list_sort.hpp` (the comparison orders of
numpy's object argsort and CPython's sort, which decide the operands a
TypeError names). Bank arguments use Python datetimes, which carry no native
date payload. Details are in `docs/pandas-filter-domain-parity.md`. All seven
modules are promoted.
Next in this thread: the Polars filter modules.

## Polars sequence filters — 2026-10-25

New bank `docs/polars-sequence-filter-bank.json.gz` (3,000 frames, 38,601
public calls over polars_filtering_constants, start/end_activities, traces,
starts_with, ends_with, rework, prefixes, suffixes,
consecutive_act_case_grouping, timestamp_case_grouping and activity_split;
about 6 minutes to capture, three runs per hash seed) passes in
`minera_polars_sequence_filter_bank_parity_tests`. New native API:
`polars_sequence_filters.hpp`, reusing the pandas sequence argument types.
Errors carry the full Polars plan text. Polars `group_by` outputs come in a
random order, so such calls are stored as unordered and compared sorted.
Inner joins keep source order unless the source is no taller than the joined
case frame. Details in `docs/polars-sequence-filter-domain-parity.md`. All
twelve modules are promoted.
Next in this thread: the Polars attributes, between, cases, ltl, paths,
timestamp and variants filters.

## Polars attributes/between/cases/ltl/paths/timestamp/variants — 2026-10-25

New bank `docs/polars-filter-bank.json.gz` (3,000 frames, 82,819 public calls
over every function and constant of the seven modules; about 8 minutes to
capture, three runs per hash seed) passes in
`minera_polars_filter_bank_parity_tests`, replayed through the
`LazyEventTable` APIs. The pilots with seeds 7 and 9 pass too. The numeric
attribute filters were rewritten around the Polars plan: Python build errors,
bottom-up plan resolution, then predicate pushdown merging (filters on a null
literal stay in place, predicates combine per column in bytewise order) and
Kleene evaluation. New shared helpers: `polars_column_hint.hpp`,
`polars_list_literal.hpp`, `polars_float_text.hpp`. Two outputs depend on
`group_by` order (`filter_df_keeping_spno_activities` ties, `filter_on_ncases`
on Categorical ids) and are checked by invariants. Details in
`docs/polars-filter-domain-parity.md`. All seven modules are promoted, which
completes `algo.filtering`.

## EventLog filtering modules — 2026-10-25

New bank `docs/log-filter-domain-bank.json.gz` (6,000 XES cases, 240,450
public calls over every function of the seven `algo.filtering.log` modules
attributes/between/cases/ltl/paths/timestamp/variants, 40% with typed edge-case
arguments; about 3 minutes to capture) passes in
`minera_log_filter_bank_parity_tests`. Native changes: max-activities key and
empty-log rules, negative numeric ranges, lazy stream case keys, operand order
of case-size errors, per-position LTL timestamp bounds, protected-trace rules
of the two threshold helpers, list/tuple repetition in the paths threshold
(details in `docs/log-filter-domain-parity.md`). All seven modules promoted
(277/387). The retained per-module suites (`test_log_*_filter.cpp`, not in
CMake) still pass; compile them ad hoc with
`g++ -std=c++20 -O0 -g0 -UNDEBUG -Iinclude -Isrc/backend TEST.cpp build/libminera_core.a`
(about a minute each). Next in this thread: common/dfg filtering helpers, then
pandas and Polars filter modules.

## Decision mining — 2026-10-25

New bank `docs/decision-mining-bank.json.gz` (632 decision points, under a
minute) passes in `minera_decision_mining_parity_tests` through the new
`reference_decision_table` (token replay rows, attributes present in every
trace, one-hot strings then numerics, first-appearance classes). The public
tree raises and the data net has no guards in the pinned environment; native
trees and guards stay extensions. Package promoted: 72/86 packages.
Next: DMM_VEC rule, OC playouts, remaining partial packages.

## Trace-attribute clustering — 2026-10-25

New bank `docs/trace-attribute-clustering-bank.json.gz` (1,200 calls, about
30 s, with the evaluation functions' condensed distances) passes in
`minera_trace_attribute_clustering_parity_tests`. Native changes:
nearest-neighbour-chain average linkage with scipy's tie and relabel rules,
left-to-right leaves, single-group error, DFG selector raising (native DFG
distance behind `dfg_extension`), norm-product cosine. Still open: DMM_VEC
when a variant has no successions (73 calls tolerated; see the parity doc).
Package stays partial. Next: that DMM_VEC rule, decision mining, OC playouts.

## LLM knowledge injection — 2026-10-25

New bank `docs/llm-injection-bank.json.gz` (245 calls, a few seconds) passes
in `minera_llm_injection_parity_tests`. The reference texts are fixed
templates over the parameter strings; new native header
`include/minera/llm_injection.hpp` reproduces them (not yet wired into the
JSON/CLI/server adapters, which still use the older prompt builder). The
three injection packages promoted (71/86, 265/387). Next: process-tree
alignments, OC playouts, decision mining.

## Contextual label splitting — 2026-10-25

New bank `docs/label-splitting-bank.json.gz` (1,600 calls, about 10 s) passes
in `minera_label_splitting_parity_tests`. Native changes: the target column
copies the activity column first, and greedy modularity uses the incremental
CNM gain updates with the library's exact float arithmetic (details in the
parity doc). Package promoted (68/86, 261/387). Next: process-tree
alignments, OC playouts.

## Case-relations merging — 2026-10-25

New bank `docs/case-relations-bank.json.gz` (440 calls, 46 raising, about
20 s) passes in `minera_case_relations_parity_tests` (cells, dtypes, index
labels). Native changes: `str` stays `str` with missing values, absent
columns of empty sides count as missing, `datetime64[s|ms]` units, and the
pandas inner-merge row order when each incoming row yields one pair on
average (rule in the parity doc, also fuzzed on 36,000 direct merges). The
fixture header takes about 8 minutes to compile. Package promoted (67/86,
260/387). Next: label splitting, process-tree alignments, OC playouts.

## OCPN and SAW nets — 2026-10-25

New bank `docs/ocel-nets-bank.json.gz` (1,080 calls, about 50 s) passes in
`minera_ocel_nets_parity_tests`; nets compare up to isomorphism, with replay
counters and per-arc weight distributions in the colours. Native changes: the
reference tree-to-net construction (`process_tree_reference_net`, fuzzed
against the public converter on 6,000 random trees), OCPN mining without
fall-throughs or the strict sequence cut, relation-order event sequences for
IMd, SAW and replay, flattening by canonical type under a type selector, the
unrelated-object-type error, the IMd single-activity IndexError, and per-arc
SAW weights in the merged net. Both packages promoted (66/86, 259/387).
Next: process-tree alignments, decomposed RECOMPOS_MAXIMAL, OC playouts.

## Link analysis and timestamp interleavings — 2026-10-25

New bank `docs/ocel-links-bank.json.gz` (1,000 calls, about 10 s) passes in
`minera_ocel_links_parity_tests`. Native interleavings changes: reference
index numbering, nanosecond-extreme boundaries with exact elapsed seconds and
the start-boundary overflow error, naive timestamps rejected, and rows ordered
by the pairs' first indices. Link target order within a source is a hash-set
order and is compared as a set. Both packages promoted (64/86, 256/387).
Next: OCPN and SAW nets.

## Object-centric OTG, ETOT and OCDFG — 2026-10-25

New bank `docs/ocel-graphs-bank.json.gz` (2,550 calls, about a minute) passes
in `minera_ocel_graphs_parity_tests` with no native change. Six packages
promoted: OTG, ETOT and OCDFG discovery and graph-comparison conformance (62/86
packages, 254/387 modules). Remaining object-centric packages: interleavings,
link analysis, OCPN, SAW nets, and the two OC playouts.

## Heuristics Miner++ and dispatcher — 2026-10-25

New bank `docs/heuristics-plusplus-bank.json.gz` (2,580 calls, about 20 s)
passes in `minera_heuristics_plusplus_parity_tests`; Petri results are bound up
to renaming of places and silent transitions (the reference names them in hash
order). Native fix: the pandas route's two pair-free TypeErrors now follow the
general rule (see the parity doc section "Heuristics Miner++ bank"). Module
`algo.discovery.heuristics.variants.plusplus` and package
`algo.discovery.heuristics` promoted (56/86 packages, 248/387 modules). The
older `docs/heuristic-plusplus-parity-contract.md` gates are superseded by this
bank.

## Token replay dispatcher — 2026-10-25

New bank `docs/token-replay-dispatcher-bank.json.gz` (1,200 calls, about 20 s)
passes in `minera_token_replay_dispatcher_parity_tests`. New native entry
points `token_replay_dataframe_apply` (unvalidated public DataFrame apply,
backwards KeyError rule) and `token_replay_diagnostics_dataframe` (the public
zip of appearance-order case ids with a given output). Package
`algo.conformance.tokenreplay` promoted (55/86).

## Token replay root cause — 2026-10-25

Installed scikit-learn 1.9.0 into the cloud reference venv (the classifier the
existing root-cause fixtures name). New bank `docs/token-replay-root-cause-bank.json.gz`
(240 cases, sixteen hash seeds, about 20 s) passes in
`minera_token_replay_root_cause_parity_tests` after splitting the native
transition and unknown-activity diagnoses into separate calls, as the public
functions are. Module `root_cause_analysis` promoted (247/387).

## Forward token replay PLTR and RETURN_NAMES — 2026-10-25

New bank `docs/token-replay-pltr-bank.json.gz` (400 logs on the frozen
conformance nets, four PLTR/RETURN_NAMES configurations, sixteen hash seeds;
about a minute to capture) bound by `minera_token_replay_pltr_parity_tests`:
1,600/1,600 calls pass with no native change needed. Module
`algo.conformance.tokenreplay.variants.token_replay` promoted (246/387). The
tokenreplay package then remained partial on `root_cause_analysis` (closed
above) and its dispatcher.

## Petri alignment log dispatcher — 2026-10-25

New `align_petri_log` and bank `docs/petri-alignment-dispatcher-bank.json.gz`
(`scripts/capture-petri-alignment-dispatcher-bank.py`, about 12 minutes on 4
cores; fixture by `scripts/generate-petri-alignment-dispatcher-tests.py`):
1,568/1,568 bound calls pass in `minera_petri_alignment_dispatcher_parity_tests`.
Package `algo.conformance.alignments.petri_net` promoted (54/86 packages). The
log path raises on nets with an unreachable final marking (easy-soundness
check) and orders DataFrame cases by case id; see the parity doc section.

## Alignment log enrichment — 2026-10-25

New `include/minera/alignment_log_enrichment.hpp` ports the public
petri_net `utils.log_enrichment`; bank `docs/alignment-log-enrichment-bank.json.gz`
(`scripts/capture-alignment-log-enrichment-bank.py`, fixture by
`scripts/generate-alignment-log-enrichment-tests.py`) passes 3,000/3,000 in
`minera_alignment_log_enrichment_parity_tests`. Module promoted (245/387). All
seven petri_net alignment modules are now complete (the package followed, see
above).

## DFG alignment dispatcher — 2026-10-25

New bank `docs/dfg-alignment-dispatcher-bank.json.gz` (1,500 cases, EventLog
and DataFrame, sixteen processes each) bound by
`minera_dfg_alignment_dispatcher_parity_tests`. Both sides break ties by two
random routing UUIDs per search; the new `DfgAlignmentOptions::routing_names`
fixes them, and the test enumerates all 56 relative orders: scores and errors
exact, every public alignment/counter attainable. Package
`algo.conformance.alignments.dfg` promoted (53/86 packages). Regenerate with
`scripts/capture-dfg-alignment-dispatcher-bank.py` and
`scripts/generate-dfg-alignment-dispatcher-tests.py` (the fixture is JSON
parsed at run time; a brace-initialized fixture of that size did not compile
within 30 minutes).

## Log repetition XES lists — 2026-10-25

Cloud container, pinned runtime. The public default importer stores a
`<list>` as `{'value': None, 'children': ...}` (None without child elements,
pairs after an early `<values>`, later `<values>` ignored) and never stores
`<container>`; native kept plain list/dict structure. The importer now sets
`EventAttribute::xes_public_shape` for the iterparse variations (native
structure and export unchanged) and the log repetition filter compares the
public value. New bank `docs/attribute-repetition-domain-log-lists-bank.json.gz`
(`capture-repetition-domain-bank.py --engine log --log-lists --bank-seed
20261025`): 18,557/18,557 pass, 1,010 fail with the shapes ignored; the
20261024 log bank still passes. `algo.filtering.log.attr_value_repetition.filter`
promoted (244/387 modules). One XES round-trip test assertion was updated:
an exported `<container>` re-imports as a `<list>`, so its shape becomes a
wrapper. Rules: *XES lists and containers* in
`docs/attribute-repetition-domain-parity.md`.

## Token replay silent-path tie order — 2026-10-25

Cloud container, pinned runtime; no retained `build/` bank used. The last
evaluator gap (`seed1_heuristics_noisy`) was a tie between equally short
silent paths: the reference follows the PNML arc order of a place's outgoing
arcs, native followed transition index order. PNML document entry points of
the evaluators now pass `pnml_place_outgoing_transition_order(document)` as
`place_outgoing_transition_order`. The evaluation suite has no listed
divergences left (token fitness 162/162, generalization 162/162, composite
160/160). New disjoint bank `docs/token-replay-arc-order-bank.json.gz`
(1,000 shuffled documents, hash seeds 0-3 x three allocation paddings) is
bound by `minera_token_replay_arc_order_parity_tests`: 3,695/3,695 stable
rows exact, 843 of them differ under the old order, 55 identity-dependent
rows excluded. Regenerate with `scripts/capture-token-replay-arc-order-bank.py`
and `scripts/generate-token-replay-arc-order-tests.py`. Promoted: token
fitness and token generalization modules, packages `algo.evaluation`,
`algo.evaluation.replay_fitness`, `algo.evaluation.generalization`
(52/86 packages, 243/387 modules). Open for token replay itself: in-memory
nets carry no arc order, and identity-dependent ties have no document rule.
See *Token replay silent-path tie order* in the parity doc.

## Model quality metrics equivalence — 2026-10-24

Fresh black-box capture from the cloud container (pip-installed pinned
release), no retained bank reused. `docs/evaluation-reference-fixtures.json`
holds 163 log/model pairs, each evaluated on the net read back from its own
PNML (reference replay order follows in-memory node order) in at least two
fresh processes; `minera_evaluation_reference_equivalence_tests` compares
every evaluator. Complete: all three simplicity variants, token,
Align-ETConformance and automaton-after-align precision, the precision
dispatcher, alignment fitness and the fitness dispatcher. Automaton precision
is proven by validity and optimality across the reference's tie choices
(152/156 attainable, 4 too large to enumerate with Minera's alignment checked
optimal). Evaluator alignment fitness now defaults to state-equation A*.
Align-ETC and automaton precision use the reference's name-ordered silent
search (see the parity doc). Token fitness, generalization and composite now agree too (see the
tie-order checkpoint above). Mined corpus nets depend on the capture
process's hash seed, so a fresh capture can expose new routing ties on other
heuristics nets. Details: *Evaluation black-box equivalence* in
`docs/reference-2.7.22-parity.md`.

## Current continuation: full732 Polars timestamp API/UI acceptance — 2026-10-24

The pandas timestamp checkpoint below is on origin/main. The Polars half of
the table timestamp family is now complete as well. Scope is unchanged:
86 packages / 387 modules, 201 strict open units, 565 unentered queries; no
catalogue row was promoted.

ROOT Polars admission (`polars-timestamp-full732-admission-prepared-1/`) binds
all 26 declared originals before any decode. The first run stopped before any
output because the census assumed a fixed keyword order; that source revision
and its log are preserved (`source-revision-1`, `...-admission-run-1.log`). The
corrected census accepts any order of the required keywords and records it.
Result: 732 distinct receipts, 229 frames / 503 errors (427 apply, 76 collect),
26 eager / 706 lazy, 2 algorithm warnings, zero mutations. The independent
mapper rebuilds the unchanged 11,579,792-byte vector oracle byte-exact (885
frames). Report SHA256
`76e3970ce6e3454c579e56c3a44379862e9de25b276ab8ee886755bee6bc3d9a`.

Wire: `docs/polars-timestamp-original-call-workflow.md`. Descriptor
`minera-polars-timestamp-original-call-v1` holds `source_kind`
(DataFrame/LazyFrame) and ordered `keyword_entries`; any parameter dictionary
uses the ordered binding vector for the four filters, while the helpers flatten
it to one raw value. Lazy schema errors report `collect` with apply returned.
Transport fixture: 3,426,683 bytes, SHA256
`4097e4411063410c6a2513667cefffb2de2fca3ab99fd07ec0e6607f3ccad65e`.

CTest run 1 left the two large Polars targets unbuilt (the new test missed the
workbench JSON header and namespace); both logs are preserved. Run 2
(`polars-timestamp-native-ctest-2.log`) passes all 8 timestamp suites,
including 732 native + 732 direct + 732 owned HTTP Polars calls (53.5 s) and the
pandas 861 bank. The pandas controls test assertion for a Polars-engine
original call was updated because that call now reaches the Polars route.
Server SHA256 `f5ba2af8c265249d7e152052625b0b1d225324b4ab83915eea4eae7384abd7a6`.
Frontend: focused 27, TypeScript, Vite and full 204 files / 9,969 tests pass.
Owned browser: 23 checks, 17 requests, 12 byte-exact downloads, including the
naive-Datetime LazyFrame collect diagnostic and reordered-keyword import; four
screenshots inspected. Report SHA256
`e8229a7b1d2200c61fc93ab510c8b3555841ca8a8bb7f835ddedc0dfb44efff3`.

A post-execution independent source review of both table families now exists
(`docs/table-timestamp-independent-review-1/`, review.json SHA256
`f5bc6bd254167e97d6fe277a9fc44302e7a380c6596dc0923282e23dfce1edb8`). It ran in
the cloud from the sealed checkpoint members only (original inputs absent) and
found no blocking issue: oracle/vector pins, all 861/732 transport-vs-oracle
cases, fresh-namespace mapper loading and the 23-field C++ checks hold. Minor,
still open: M1 plan.py hardcodes five validation flags and drops non-suffix
members silently; M2 the Polars census trusts the producer's input_unchanged
flag instead of comparing an input-after snapshot (so Polars zero-mutation is
producer-asserted, not independently measured); M3 path() skips the symlink
check for nonexistent targets (uses fail safely); M4 pandas admission reads the
plan declarations before their pin and unpinned bank validation.json. Notes
N1-N6 are in review.md. Closing M2 needs the owner's build/ Polars records.
`docs/retained-polars-timestamp-original-call-checkpoint.jsonl.gz` (132,754
bytes, SHA256 `4fa340bd707be63119cc38eb9ae7ca6a756b3ea8d7ee004116b91f35d7b4fd18`)
embeds 44 OWN members and declares 29 external pins; manifest SHA256
`a609914576a9ae35ef6b2c6e1a738c0b5575b11e446025cc95b31b8511e3bf6d`.

Next: continue with the next open requirement below. Fix M1-M4 in any future
admission scripts; rerunning these banks' admission needs the owner's build/.
The build/ evidence directories are gitignored and exist only on the owner's
machine; the sealed checkpoints in docs/ carry their OWN members.

## Previous checkpoint: full861 pandas timestamp API/UI acceptance — 2026-10-24

The LOG timestamp checkpoint below is on origin/main. The
next family is the table timestamp pair; pandas is done, Polars is next. Full
scope remains 86 packages / 387 modules, 201 strict open units and 565
unentered grouping queries. No catalogue row was promoted; offline structural
validation passes unchanged 46/40 packages and 226/161 modules.

Source-only plan: `log-timestamp-next-table-timestamp-metadata-plan-1/` (source
`table-timestamp-next-plan-source-1/plan.py`) reads only OWN bank validation,
acceptance bindings, contracts and headers. It records pandas 861 (302/559)
and Polars 732 (229/503) totals and copies 18 pandas and 26 Polars declared
original whole-file pins without opening them. Validation SHA256
`1a36b18e0af10ccd63219d94ebfb1d4746d5bcb0679b63d6d7ae1123a1005c77`.

ROOT pandas admission (`pandas-timestamp-full861-admission-prepared-1/`) binds
all 18 declared originals to their pins before any decode, then recomputes
every canonical receipt: 861 distinct, intents/audit/plan aligned, 302 tables /
559 errors, zero mutations and warnings. Functions: apply 7, apply_auto_filter
7, apply_events 131, attribute 156, completing 128, contained/intersecting/
starting 144 each. Parameter states: mapping 779, entries 36, raw 20, null 14,
omitted 12. The independent OWN mapper (never oldGEN) rebuilds the unchanged
1,971,807-byte native oracle byte-exact (117 snapshots). Report SHA256
`557d5179f770a82cc6e13882db94e530e05bbf3bbd52b08db4e3b51dacf54a89` at
`pandas-timestamp-full861-root-admission-1/report.json`. The admission was
reviewed by ROOT only; no separate independent-review agent was used.

Wire is frozen in `pandas-timestamp-original-call-wire-frozen-1/wire.json` and
documented in `docs/pandas-timestamp-original-call-workflow.md`:
`/api/reference-table-timestamp` enters original mode when `call_json` is
present; descriptor `minera-pandas-timestamp-original-call-v1` holds exact
arity args and an optional `parameters` entry. Enum-keyed dictionaries use the
ordered parameter vector; other values the raw arm (helpers always raw). The
23-field response adds call, controls before/after and call projection.
Backend commit `c5ac33b6`; frontend/doc commit `493f3ae5`.

ROOT transport generation re-binds every original, reproduces the census and
whole oracle, then emits the 1,964,359-byte external fixture once (SHA256
`71286e69b3d31c944fabebbf0b5fa13bf583d71a29073aa328ac19e3389b0d01`); report
`pandas-timestamp-transport-generated-1/report.json`. CTest run 1 had the
standalone oracle target unbuilt in `build/` (Not Run) while the other four
passed; it is preserved. Run 2 (`pandas-timestamp-native-ctest-2.log`) passes
all five: oracle 302/559, controls, 861 native + 861 direct + 861 owned HTTP
(46.33 s) and both existing table timestamp workbench/route suites. Production
server builds; SHA256
`f6d00c9c9911436d1a4d0a6127ec7eb616bb461445de66196b2a14f8bc2c76a9`.

Frontend: focused 21, TypeScript, Vite (`pandas-timestamp-frontend-dist-1`) and
full 203 files / 9,963 tests pass. Owned headless browser
(`pandas-timestamp-browser-owned-1.cjs`) passes 22 checks, 17 requests and 12
byte-exact downloads: all six filters with independent row selections, typed
and enum examples, helper and parser diagnostics, malformed control text,
BOM/CRLF/wide import, operation retention, held cancel/edit races, genuine
filtered File reuse, mobile layout and the preserved friendly form. All three
screenshots were inspected; owned server and Chrome stopped. Report 8,880
bytes, SHA256 `20bc40764022af0fcef8605a1e6c802936cb9bdde2d54a0dec1d87a5d86cae37`.

`docs/retained-pandas-timestamp-original-call-checkpoint.jsonl.gz` (123,429
bytes, SHA256 `b8fef0494a8bada19a9a81fd3c60d8cef6a7b8c4fcf83948515f997ab1ac4769`)
embeds 43 OWN members and declares 21 external pins; closed readback passes.
Manifest SHA256
`f04aecbd32df58908f02ef1aa94068c9d2811d3d4b52107b2e9ddeed700f3029`.

Next: separate ROOT admission of the 26 declared Polars originals (three
banks, 732 calls, eager/lazy and apply/collect phases), census and whole-native
fixture mapping, then the same original-call wire/UI/transport/browser
pattern. Do not replay retained banks. Push of the four local pandas commits
awaits owner approval.

## Previous checkpoint: full342 LOG timestamp API/UI acceptance — 2026-10-24

All13 original LOG timestamp functions now have an original-call API and a
rendered workbench. Backend/acceptance commit is `a8dd00b0`; frontend/workflow
commit is `bb0c79c0844be735ffc8752abddc1fd66c06c592`; evidence commit is
`53f9d675`. Full scope remains86 packages /387 modules,201 strict open units
and565 unentered grouping queries. No catalogue row was promoted.

`POST /api/reference-log-timestamp` requires exactly operation, genuine native
source_file and raw call_json, without query controls or duplicate parts. The
descriptor is `minera-log-timestamp-original-call-v1`, with non-source args,
ordered keyword_entries and a nullable source_trace_index. Predicates receive
the actual source.traces reference. The seven filters receive an explicitly
passed caller-owned Options object even when parameters are omitted or raw;
ordered literal/enum timestamp and positive entries assign its actual members.
The two source-only helpers retain their implicit default call syntax, marked
`implicit_default_not_inspected`. Unused descriptor values remain metadata.
The18-tag grammar and15 native attribute tags remain distinct. Kernel, public
native header, original oracle and shared codecs are unchanged.

The exact25-field response records native_returned/result_kind independently
from export. Actual source, operand and Options snapshots are captured before
and after success or native failure. Optional enum selector absence is null;
predicate trace wrappers are inspection artifacts. Full source-after mutations
survive native errors. Internal inspection/export failures use500 and preserve
primary native diagnostics; frontend500 handling has synthetic acceptance,
while actual internal fault injection remains unverified. Value comparisons
do not imply Python object identity. Workflow contract is
`docs/log-timestamp-original-call-workflow.md`.

ROOT fully reviewed the new transport generator/IO and independent source
review, then separately reverified all13 complete original inputs twice before
admission. Admission SHA256 is
`5d6563d91079567b96533d132965e497c7e729282de97864460dfecd62d2be00` at
`log-timestamp-transport-prepared-1/root-admission.json`. Generation `e96d92`
passes complete census identities/placements, complete mapping and the entire
unchanged2,043,334-byte oracle before emitting new342 transport vectors. The
1,583,109-byte external fixture has SHA256
`64527cf7e8d514e380488e5bc87167581bfabe2ee940cd2ff0fabfb9da50e3e4`;
generation report is3,771 bytes, SHA256
`3b297e5164df5d09fd0c0adb42ba34668082a183470727b4e55de9cc45387aaa`
at `log-timestamp-transport-generated-1/report.json`. No old capture producer,
oldGEN or reference implementation was imported/executed; no new reference call.

The generator's first source draft stays in source-revision-1. Independent
review found a filesystem-loader bytecode/current-byte issue and dangling-link
path admission before execution. Final source compiles only pinned OWN mapper
bytes in a fresh namespace, uses lstat checks and rechecks every complete input.
It constructs Options only for the seven filters. Independent review is
`log-timestamp-transport-independent-source-review-1/review.json`.

Native acceptance `c38dcd` passes all four new suites in76.43seconds. The full
public suite passes342 original native cases plus storage controls,342 direct
workbench and342 owned HTTP cases. Independent native invocations supply actual
operand/Options/source controls rather than product-parser mirrors. Every25
field value, full135 returned logs,48 Boolean values,159 complete native errors
and38 source/control mutations agree. All native source/result/cell/control
artifacts reopen; descriptor/projection JSON and successful XES reopen too.
Five rebuilt prior regression suites pass `0de233` in71.73seconds: complete
log-variants public transport, table-timestamp workbench/route and friendly
log-filter workbench/route. Logs are `log-timestamp-native-{ctest,regression}-1.log`.

Production server initially failed compilation because ROOT's new route include
needed its public header before the anonymous namespace. The original failure
is preserved in log-timestamp-native-build-1.log. Final server build `ea742d`
passes with executable SHA256
`fde42651ec0f45eb352fe5535431f272bb5dd19d7041204fccbf609e498885c3`.
No native/oracle/fixture change was needed after full342 acceptance.

Frontend full suite `8486a5` passes202 files /9951 tests. Review then fixes two
small issues: predicate examples now use aware Z/offset0 bounds, and current-call
downloads use nativeStringBytes to match diagnostic submission bytes. Final
focused33 tests, TypeScript and Vite pass `7cba76`; the full9951 run precedes
these two separately covered fixes. Imported BOM/CRLF and wide integer tokens
remain untouched, including when operations change. Only complete successful
genuine returned native log Files can be reused. Bool false and all available
error artifacts remain visible/downloadable. Final build is
`log-timestamp-frontend-dist-2`, with checks in log-timestamp-frontend-*-final-*.

Owned headless browser `8d1187` passes18 checks,13 HTTP requests including two
held responses, and12 byte-exact downloads. Four predicates, explicit false,
source-only diagnostics, full independently expected log/source/error mutation,
BOM/CRLF/uint64 import, operation retention, deferred import, cancel/edit races,
genuine returned File reuse and mobile layout pass. All three screenshots were
inspected. Listener PID12976 and Chrome30516 were verified/owned, stopped and
stdio drained; the existing application was not replaced. Report is7,926 bytes
at `log-timestamp-browser-output-1/report.json`. Browser scenarios supplement
the complete342 native/direct/HTTP bank.

ROOT source equality `9a3bfe` verifies all16 adopted source/vector paths against
their committed bytes, permitting CRLF/LF only. Proof is5,371 bytes, SHA256
`342ea09a92955e56e19d93dabd8b1f9c1756f75d8ac595e4a1adc2480dbdb591`,
at `log-timestamp-original-call-finalization-1/source-equality.json`.
Offline structural catalogue validation `ce8e59` passes unchanged46/40 packages
and226/161 modules. Inherited coverage/journal dirt and caches remain unstaged.

`docs/retained-log-timestamp-original-call-checkpoint.jsonl.gz` is832,341 bytes,
SHA256 `f487e3f6ce271eceb5548456efebaaf10b2c306b430c61a731a23f73c4cdf4a7`.
It retains84 complete OWN source/metadata members and21 inert external
declarations, with the complete plan embedded in its header. Writer `6bec17`
and independent closed readback `ff388e` pass all complete member bindings and
current OWN bytes without following any external target. Sidecar is
`docs/retained-log-timestamp-original-call-checkpoint.manifest.json`, SHA256
`58d2e5b93d0569bd02bebcefdcb22fe03dedd1d444519a1c5a7c95b57c2e5164`.
Original records/oracle/vector/database/archive targets are declarations during
this packaging phase, never reclassified as OWN payload. Python qualified
error modules/args, identities/aliases, warning machinery, unobserved domains
and history/absence/new-call authority remain outside this native checkpoint.

Next: continue a source-only next-family timestamp/table plan using retained
receipt metadata, then ROOT admission before any original payload access.
Keep the complete201/565 scope and the earlier discovery/audit-timing disclosure
below. Do not replay retained banks or rerun historical readbacks against new
source snapshots. Push/readback completion is recorded separately under
log-timestamp-original-call-finalization-1, outside the already sealed capsule.

## Previous checkpoint: full 342-call LOG timestamp index and native basis — 2026-10-24

The sealed Polars checkpoint below is pushed. Actual remote readback `a3467c`
verifies main at `94e0772eaa827e26343559121d2b542ed9728fb6`, including separate
evidence `b5584f01` and handoff `94e0772e` commits. The inherited two coverage
edits remain unstaged. Full scope remains201 strict open units and565 unentered
grouping queries; no catalogue promotion follows from the new timestamp work.

The complete timestamp bank is342 retained calls (326 public +12 converter +4
forwarding), all13 functions. Its previously missing10 whole bindings are now
recovered and current bytes verified. Root first reviewed the entire new
recovery helper/IO and independent source review, then opaquely admitted only
`docs/log-timestamp-filter-public.json.gz` (782,843 bytes; SHA256
`5bf07e1c85251d670fc22f9cd10acc6b1f4d0c3c0a5b2a5226a558fe4d8b3ee7`).
Separate structural recovery `20baf8`, after container admission `66d5e2`, reads
28 member headers and lexically skips their encoded payload string tokens.
It traverses41,666,835 decompressed container bytes, including41,661,359 encoded
payload-token bytes, with no member payload semantic decoding, parsing or hash
verification and no named member target following. The checks archive remains
unopened. Recovered pins are declarations only:
`log-timestamp-public-header-recovery-1/recovered-missing-pins.json`,10,139 bytes,
SHA256 `2b8ead71e8a5d5155903d30229464ea73fc922db48649af24fd5eec4b34e1303`.

The separately reviewed root opaque helper verifies all20 current whole target
bindings twice (`b2de96`): each bank's records/intents/digest audit/cases/producer/
validation plus oldGEN and the monolithic native oracle. This phase decodes no
target bytes, opens no archives and executes no producer, generator or reference
code. Proof is `log-timestamp-full342-opaque-gate-1/opaque-gate.json`,10,011 bytes,
SHA256 `8609dc53a250660cb8ce31d87d8f8aca9367eb0bc8f5b9ceb303831a227ca703`.

Root then separately admits exactly12 records/intents/digest audit/cases files
for the independently reviewed canonical census. Admission is4,288 bytes,
SHA256 `395dd076efea8a03243cb7e89544c359041d1ea21c24bc87e2d63bdd092c955a`, at
`log-timestamp-full342-admission-prepared-1/root-semantic-admission.json`.
Census `19157b` passes all342 distinct canonical IDs, zero case/canonical
duplicates and zero audit/entry alignment gaps. All original whole bytes are
bound before parsing and rechecked afterwards. Its complete report is
`log-timestamp-full342-canonical-census-1/report.json`,14,772 bytes, SHA256
`f24ada2b1515fb5b158041e9c175c8ca166874c51b8c4fa6fa281e080ab1ff26`.
Canonical fields are exactly module,function,args,kwargs,call_shape with
`positional_args_keyword_controls`; sorted IDs joined by LF without trailing LF
have SHA256 `b61e2093aa21d996449e23051695d06061a4212bd2188ab5aed11a61fd482f67`.

Measured outcomes are135 complete logs,48 Boolean values and159 full qualified
diagnostics, with93 predicate calls and zero warnings. All38 input/control
mutations occur in the source embedded in args0; kwargs and positional tails
do not mutate. Complete alias evidence uses sparse flattened identity-match
positions per returned trace, so pair counts can differ from event counts.
The census measures15 actually observed Packed tags from the18-tag producer
grammar,116 distinct complete log snapshots,249 source-derived trace helpers
and819 snapshot placements. Snapshot attributes have zero nonstring top-key
drops; three parameter entries lie outside the oldGEN selector/positive
projection. These measurements do not prove native representability.

The census's initial source incorrectly hardcoded one audit verification flag.
Both reviews found it before execution; the original source/IO/preparation stay
preserved under `source-revision-2`. The corrected source reports actual separate
path/count/overlap booleans. Independent final review is7,178 bytes, SHA256
`9f4a226ba3ac6f7b29c09a675a840a53594e2aff8a15e611fe89adb00431132d`.
The prior metadata discovery/truncation disclosure and326/338 audit-timing
correction remain intact; no complete-history, absence or new-call authority.

Root's separate native semantic admission is5,118 bytes, SHA256
`12f4f9e2cdeb2b82abf2168589d8955deff1a119212b98da19f10e480c195934`.
Whole native mapping `2b9903` passes the entire2,043,334-byte monolithic fixture:
all116 log builders,249 trace builders,342 whole cases, support, dispatcher,
storage controls and main. Reconstructed and current oracle SHA256 both equal
`4578ca764ccf6d1f0e94d1443d47a4ff7b48c1f738e599362c29f017d6821e25`;
only CRLF/LF normalization is permitted and no normalization difference occurs.
Report `log-timestamp-full342-native-mapping-1/report.json` is5,554 bytes, SHA256
`90c0c82a88199c1d2bde28fb4b0644802723a92d6df1d61c3c4b5b9dea094fb9`.
Its hash-only mappings are315,906 bytes, SHA256
`bf523b0a8f9900ad3686dcf2dd2c24918bc189053d0b15c1a2dd202db2edefb4`.
Root preserved the author's source before adding exact independent-review-body/
current-source checks and finite JSON-number validation; independent source
review2 passes at5,853 bytes, SHA256
`7adf4c127e45ff13a719767402a04bbdfb2c3d3ff1124c200ca06dd00e4ab7d6`.

Actual predicates use27 distinct Trace control snapshots in186 before/after
placements for93 calls. Native source mutations split20 successful log results
and18 errors. Native construction projects49 option assignments:28 literal
timestamp selectors,7 enum selectors and14 positive values. Three parameter
entries are ignored and seven non-dict parameter values do not project. Ten
generic stubs use the source-only/default-parameter call form and ignore eight
kwargs objects. Python error args/type modules, object identity/aliases,
datetime fold/offset metadata and full caller controls after invocation remain
explicit original-observer domains outside the old native assertions.

The unchanged native baseline builds (`4ff8c5`) and passes all342 retained cases
plus its storage controls (`ec4d68`,1.07seconds). Owned test binary is2,533,888
bytes, SHA256 `c272a32f3ff912a08fbf16533192172f2c56f83640a7129f6f673551bd66b796`.
No test process/session remains live. Kernel, oracle and capture producers are
unchanged; no old producer or GEN was executed/imported.

The distinct `log-timestamp-packed-v1` receipt profile passes new27 synthetic
integrity tests (`0efa11`) and all173 prior/new integrity tests (`4c6182`).
Root inverse-source and AST proof (`e4ca83`) preserves all nine previous profiles
after removing only the new profile/schema/doc mention and three dispatch gates.
Full342 index acceptance `071413` resolves342 actual outcomes,342 exact light
intents and342 module-free digest-only plans, verifies1,026 complete raw slices
and freshly rehashes all nine sources for nine cache hits. Validation is
`log-timestamp-admitted-index-acceptance-1/validation.json`,6,541 bytes, SHA256
`9ef7b75ce55ccb43d5e090db61bd50b57a61fd91c00bc8528339de73695b7038`.
The15,519,744-byte SQLite database is an external original-payload artifact,
SHA256 `233acff3d826edac5d1ca70be998bf66ec6f6305cd84abef21991580bbbd7b21`.

Index partitions retain each bank's complete original public_surface/runtime
objects. Root separately admits and extracts the three exact validation metadata
files (`999a44`), with no referenced path following. Safe OWN extraction is
`log-timestamp-index-provenance-1/retained-provenance.json`,32,348 bytes, SHA256
`f858db476d429dc917b83cd16469f04b35e67fbb76fdca2efdef5479292ecb8a`.
The earlier incomplete common-signature manifest/runner stays preserved as a
source draft. Tested index code/new tests remain unchanged; the guide's final
edit only records the actual completed runtime checks after acceptance.
Source commit is `63aa4664c3ac4a62b0a045ac10f4db7201db5747`. Root compares all
three adopted source files to their committed bytes modulo CRLF/LF only:
`log-timestamp-index-prepared-1/root-source-commit-proof.json`,1,838 bytes, SHA256
`219f58ccd14cac2ed025e5d5cdcb841742d4a72250fc6077e43a63b449a90fe6`.

Next: freeze the exact native original-call API/UI wire, expose all13 functions
and verify every342 case through native/direct/owned HTTP acceptance plus the
rendered workflow. Backend wire design is source-only work under
`log-timestamp-original-call-design-prepared-1/`; no API/UI implementation or
acceptance is claimed. Full201/565 scope and all authority limits remain intact.

## Previous checkpoint: full 411-call Polars variants API acceptance and rendered UI checks — 2026-10-23

Four focused source commits descend from `20cfd7f9`: `8f4c70f8` adds the closed
Polars receipt index, `eca48a77` adds exact native original-call controls,
`804e112e` adds full411 native/direct/HTTP acceptance, and `66c2336b` adds the
editor, downloads, genuine filtered File reuse and workflow contracts. Root
source proof (`046338`) compares all19 own paths against their committed bytes,
normalizing CRLF/LF only. It is
`polars-variants-original-call-finalization-1/source-equality.json`, SHA256
`eb0968ac6ee1dce75e7b8691b2ef04d2619a0f3610c4b608a3cf30ad4678d03a`.
Source push `5bfb8f` and actual remote readback `be3a8f` verify main at
`66c2336b0b55b86245878a86a991c9796c0511ed`. The inherited two tracked coverage
edits remain unstaged. Original native header, kernel, driver, vectors and
shared codecs remain unchanged.

The entire original bank is **399+12=411**: apply120, from-list104, top-k107 and
auto80;142 complete frames and269 full diagnostics (258apply/11collect), with35
eager and376 lazy inputs. All528 complete frame/category constructors and all411
whole native cases/driver/vector are independently mapped (`1da673`), report
SHA256 `23cba3b2f3c8625744334ed120e1487eca6eeb366af7c4cca0706292f53d2e32`.
Actual invocation arms are274 raw /134 ordered /3 missing-required defaults;
constructed parameter arms are277 raw /134 ordered. All three unused objects
are raw. The original seven fields are module/function/input/
category_observables/kwargs/call_shape/input_kind, with
`source_positional_all_options_keyword`. Sorted canonical IDs joined by LF,
**without a trailing LF**, have SHA256
`1e3b6c30324bdac74bbcf8660e1db4ddc46791c50b3f73de8f9bcbca6c986759`.

The distinct `polars-variants-receipt-v1` index resolves all411 outcomes, all411
full audit plans and all411 light intents:1233 complete raw slices and six
freshly rehashed cache hits (`c8ca4d`). All146 integrity tests pass (`766f92`).
Independent source review retains all eight earlier profiles exactly after
removing only the additive new gates. Incomplete entries, unknown terminal
fields and unresolved intents remain conservative gaps, with no history,
absence, catalogue or new-call authority.

`POST /api/reference-table-variants` now dispatches original mode by the unique
engine. Polars takes exactly engine=polars, membership/from_list/top_k/auto,
genuine native source_file and exact raw call_json. Descriptor schema is
`minera-polars-variants-original-call-v1`, with authoritative DataFrame/LazyFrame
input_kind and ordered keyword_entries. There is no fabricated args array or
header/auxiliary upload. Required operands may be absent and reach the exact
native `(source,std::nullopt)` overload; the constructed parameters remain
inspectable but are not passed. Explicit None, raw values, omission and ordered
enum/string identities remain distinct. All twelve producer Packed types use
the proven Polars carriers: int64/uint64, exact double bits/std::to_string,
Microsecond/null-zone NativeDate and empty sequence child keys.

The Polars original response has25 fields, retaining complete source before/
after, actual eight-field caller controls before/after, raw call and successful
filtered native data. Native diagnostic phase and actual invocation-returned
state are separate. Returned metadata is exactly kind,row_count_known,row_count,
table_context_json, captured from the actual eager/lazy object before collection
without an extra scan. The whole unchanged table codec string preserves complete
context and its raw fallback/schema marker, with empty rows explicitly serving
as header context. This artifact is downloadable and is never reusable source
data. Collect errors retain it after a returned lazy object; genuine successful
filtered File bytes alone can be reused. Friendly18 and pandas original24 retain
their existing contracts, including ambiguous-engine early diagnostics.

Root generation (`065d7b`) rechecks the complete528/411 basis before emitting
the separate external transport include
`src/backend/tests/polars_variants_workbench_public_vectors.inc` (3,118,026 bytes;
SHA256 `5dbfb0b6538fc0cb57851193a7e2349f9537198b32b8bfa0c50a385a08a9a475`).
The original generator and capture producers were never executed/imported.
Generation report SHA256 is
`b61b73e6924a8867bea5873e81fb9275aa78a910d8f840847705064fd0975aee` at
`admitted-polars-variants-original-call-transport-1/generation-report.json`.
The corrected full411 target passes (`bac2d2`,67.91seconds): unchanged original
411 cases plus two extra native regressions,411 direct and411 owned HTTP cases,
all25 fields, actual caller objects, returned metadata and complete native
source/result/control/context artifact reopen. Expected controls and returned
flags come from independent actual native invocations, not the product parser.

Nine focused native and prior pandas491/friendly regression suites pass
(`bfd48d`,55.84seconds). Frontend checks pass57 focused tests (`3f55e9`), all201
files /9937 tests (`83aa6b`) and TypeScript/Vite (`f8239b`). The corrected owned
headless browser passes18 bounded checks,16 HTTP requests including two held
responses, and nine byte-exact downloads (`a191bb`). Independently expected whole
native sources/results/header contexts include exact uint64 maximum values.
All four operations, eager/lazy errors, missing versus None, unused/raw/ordered
parameters, BOM/CRLF import, cancellation and genuine filtered File reuse pass.
Both screenshots were inspected (`c72fb6`). Listener PID ownership was checked;
owned server15932 and Chrome39736 stopped with stdio drained. These18 UI checks
supplement the complete411 API gate.

Failures remain preserved. The initial native baseline was NotRun (`173aea`)
because the executable was absent, then built and passed (`bfddd5`). A focused
frontend metadata preflight used the wrong working directory; all57 product
tests passed and all eight own pins were separately verified (`a7f52b`). The
first full411 fixture compile (`937714`) omitted a public codec include in the
test wrapper. Adding only that include fixed it (`006bda`); its original frozen
source and failure receipt remain. The first browser run (`57ab2a`) passed all16
requests and nine downloads before its final pandas navigation missed the reset
table-filter choice. The corrected recipe explicitly reselects variants, with
a fresh output directory. Neither correction changed product behavior, native
kernel or oracle data.

Root final facts (`41833f`) are
`polars-variants-original-call-finalization-1/root-facts.json` (34,265 bytes;
SHA256 `7ad00c55031e48e70066555996df4ef031d6cbf6b2966ccc9822179c09348ad5`),
with19 own source bindings,91 freshly verified external bindings and all actual
terminal counts. The sealed checkpoint is
`docs/retained-public-observation-polars-variants-original-call-checkpoint.jsonl.gz`,
1,103,489 bytes, SHA256
`c8ea289366ee57ff03ec74ca155d24a9f399929e846058ff6a5fe1774f210cb2`.
Root writer `1bae52` succeeds after explicit scope freeze `c4d689` and root scope
review. It retains155 exact OWN members,91 external bindings and12 inert
historical declarations. Its sidecar is471,355 bytes, SHA256
`360ad762c38d30598c38ba0969385fcae6884009a7489e3c0571c3c353054bd9`.
Independent source review is11,205 bytes, SHA256
`890e190151da3cf9a8477271681ce1773d169532e72448dac7bc63504c05a10b`;
its first null-only report serialization is separately preserved and disclosed.
Standalone independent readback `3d261e` passes all155 current member bytes,
whole canonical header/JSONL/sidecar and987 nested declaration dispositions:
`polars-variants-original-call-checkpoint-independent-readback-1/validation.json`,
1,432 bytes, SHA256
`20f0819451109c0a5153c4fc64431464fcb0c946ac5a8f2ff1f5dd64365ee31b`.
No external/historical target was followed or opened during packaging. Exact
scope is72,860 bytes, SHA256
`9c213690bef074859f395e1cbd21dc9f6e00cfee6a06dacb06f56fe5cfa3358b`.
Final handoff and later readback receipt are excluded to avoid hash cycles.
Evidence commit is `b5584f0105173d82e9866ad7cf7ec89b3e2fe2c4`; source and
evidence are separate clean commits. Readback predates this evidence commit,
with no capsule member changed afterwards.

No original archive was opened for this Polars continuation. Root separately
admitted exact flat observations and native oracle parsing before use. The
unchanged integrity regression uses three previous LTL archive containers.
Python qualified errors/args, warnings, identity, public explain plans and
physical categorical context remain external. Separate datetime fold/offset
metadata remains in original receipts, with no fabricated native fields.

Committed catalogue scope remains86 packages (46 complete /40 partial),387
modules (226 complete /161 partial), **201 strict open units**, six grouping
banks and **565 unentered logical queries**. Next candidate is the complete342
LOG timestamp bank, all13 functions including183 returns,159 errors and38 real
source/control mutations. Its exactly five-file metadata-only plan is
`polars-variants-next-log-timestamp-metadata-plan-1/`, report SHA256
`adb5fe04f5f2db3bf597d5626f9e6a3533df3cb86fb0e060b239057cb4cd1b68`.
Root verifies only these five own files (`cde2b9`), with no original reads:
`polars-variants-next-log-timestamp-metadata-root-review-1/report.json`, SHA256
`51d0da627b3d7e86b92da292ee6dfa37e3606266e24bb392fdd78b4e8dcf39d9`.
Ten whole original bindings remain missing (three records/intents/digest audits
and oldGEN). Admission is pending; the saved28-member public archive pin is
only a declaration, requiring separate root IO review before recovery. Preserve
the prior326/338 audit-timing correction and the distinction between source
mutations inside controls and independent kwargs mutations. The worker's
overbroad initial docs search (`0eab6b`) and truncated, unclassified potential
embedded-payload exposure are disclosed in validation; no zero-exposure claim
or authority follows. This next plan is separate from the current Polars capsule.
The following pandas/LOG/attributes checkpoints are historical.

## Previous checkpoint: full 491-call pandas API acceptance and rendered UI checks — 2026-10-22

The four source commits descend from `019946ea`: `674ca179` adds the closed
pandas variants receipt profile; `a5f670dd` implements original backend controls;
`f4f47975` adds complete native/direct/HTTP acceptance; `002e4de5` adds the editor,
downloads, reuse and workflow contracts. Root source comparison (`37954f`, then
the final heading clarification in `93ce63`) confirms 18 own paths match their
committed bytes modulo CRLF/LF. The generated transport include is separately
external (871,258 bytes; SHA256
`21485a7601a24ff64c9ab544e0bb6f9160b659e8af9ac406ad781f3b46578a06`).
Original native header, predicate, oracle and shared codecs are unchanged.
The inherited two tracked coverage edits remain unstaged.

The complete bank is **439 + 52 = 491** original calls: apply154;
filter_variants_by_coverage_percentage113;
filter_variants_by_maximum_coverage_percentage110; filter_variants_top_k114.
All 357 complete frames and 134 complete native type/message diagnostics remain
in scope. Root independently reconstructs all177 native snapshots, all491 whole
native cases and their driver, including445 raw parameter arms,46 ordered-entry
arms and34 actual supplied tables. No observed native projection losses were
found. All original column_names are neutral singleNone; arbitrary pandas column
names have no EventTable carrier and are not claimed.

The exact original six-field receipt is module/method/input/args/kwargs/call_shape,
with `dataframe-positional-plus-explicit-args-and-keywords`; source is separate
from the one positional criterion. Sorted canonical IDs joined by LF with
**no trailing LF** have SHA256
`82df0e73830fdbedc81501b7d59384d16a7c203253bb2320a36c622f0e99efd3`.
The root-admitted closed index resolves all491 outcomes, full plans, digest plans
and intents with1964 complete raw slices and eight rehashed cache hits (`a3b031`).
All121 receipt integrity tests pass (`bebdff`). Two preentry duplicate links are
retained; their constructor equality and complete historical absence remain
unproven. This checkpoint grants no new reference-call authority.

`POST /api/reference-table-variants` delegates by call_json presence. Original
pandas mode takes engine=pandas, membership/top_k/min_coverage/max_coverage,
genuine native source_file, exact raw call_json and optional variants_file.
The descriptor schema is `minera-pandas-variants-original-call-v1`, with one
Packed criterion in args and ordered keyword_entries. Omitted/None/raw/ordered
parameters, enum/string identity and typed admission dictionary ignored values
retain their actual native carriers. All34 supplied frames are direct literal
variants_df values referencing the real uploaded File. Actual caller controls
before/after have six fields, with native_options_present=false; no private
resolved-options mirror is substituted. The complete24-field response retains
source before/after, auxiliary/result artifacts, actual control snapshots and
exact call bytes. Original friendly selectors are null; friendly18 behavior
remains separate. Python aliases, qualified errors/args and warning transport
remain external; transport phases are not original observer phase claims.

Root runtime acceptance passes five focused native/regression suites (`1bed5e`),
all491 unchanged native outcomes plus491 direct and491 owned HTTP cases
(`d84cd3`,44.70 seconds),36 focused frontend tests (`8264f6`), all199 frontend
files /9916 tests (`39c160`) and TypeScript/Vite (`8c3ab2`). Owned headless browser
acceptance (`43471f`) passes17 checks,14 actual HTTP requests including two held
responses, and11 byte-exact downloads. It compares independently expected whole
native subsets, typed wide integers, attributes, dtypes, index labels and actual
auxiliary unions. Exact BOM/CRLF import, held-response and deferred-import
cancellation, and genuine filtered File reuse pass. Both success/error rendered
screenshots were inspected; listener PID ownership was verified and owned
Chrome/server stopped with stdio drained.

The initial build failure (`38f2c4`) was a test-only optional<string> reopen call.
Its original source and terminal remain retained; adding `.value()` after full
artifact equality fixed it, without a product change. Earlier Timestamp/NaT
parser corrections and all prebuild revisions remain preserved. Independent
browser review first requested complete expected subsets instead of row counts;
the corrected recipe compares all fields with lossless numeric tokens. Final
document review keeps full491 API acceptance distinct from bounded17 UI checks.

Root terminal facts (`93ce63`) are
`pandas-variants-original-call-finalization-1/root-facts.json`, SHA256
`22448dbd7eb740fec80c601335e27a7919aff89f13796605992d7c38349462e9`.
Additional seven compiled native artifact bindings (`be1be5`) are
`native-built-artifact-bindings.json`, SHA256
`cb27243745af9806658c502ec832f06d0345ce36cca434b23f35213712c7b09e`.
Evidence commit `f6583d5c` contains the capsule and validation sidecar. Root
package terminal `9ed836` seals
`docs/retained-public-observation-pandas-variants-original-call-checkpoint.jsonl.gz`
(908,358 bytes; SHA256
`1ae7f7571ae9391a66dbfd7dc9d05a1cdf9afe67b5807b02517a85aa72bb2c22`).
The capsule has199 own UTF8 members and66 external bindings. Its explicit scope
is `pandas-variants-original-call-checkpoint-prepared-1/scope.json`, SHA256
`dab3506de48ce18bf547a2981d026150bfe9168ffefd7c5d6511c2d48a77f7ca`.
Root separately prepared196 opaque OWN candidate pins (`8013f3`) and executed
the complete path/fullhash-alias scope freezer (`3ee8b4`) before packaging.
Independent source review SHA256 is
`77ff783ccaae3564d221d1d3370564ea2a967704821c3aa8a9be00e2d701fe37`.
The standalone independent new-own-capsule readback passes (`168d51`), verifying
all199 members, full hashes, strict UTF8, exact header/sidecar and current own
bytes. Its create-once report is
`pandas-variants-original-call-checkpoint-independent-readback-1/validation.json`,
SHA256 `9bf64838d4bcf38b53e6a76084cb493abb6844ae0fb2aa8af488ff1afd5344d8`.
Only the new own capsule was decompressed; no external payload was opened by
packaging or readback. The sidecar SHA256 is
`80a00ea71ce0dc0bbfea785d3020f91ac409132a0a26cc8e496f20ed6b777d8b`.

No original archive was opened for this pandas continuation: root explicitly
verified/admitted18 exact flat/native bindings, then separately admitted native
oracle parsing. The unchanged integrity regression opens three prior LTL
containers. Packaging/readback may open only their new own capsule, with all
original records/plans/intents/audits/cases/oracle/generator/vector, raw browser
artifacts, binaries and SQLite kept external. Earlier LOG archive access remains
documented below; no global archive-zero claim is made. Original producers and
generators were read as own source and never executed/imported. No new reference
calls or reference implementation inspection occurred.

Committed catalogue counts remain86 packages (46 complete /40 partial) and387
modules (226 complete /161 partial): **201 strict open units**, six grouping banks
and **565 unentered logical queries**. Full parity remains the objective.

Next is the complete **411 =399+12 Polars variants** bank, including142 frames
and all269 errors (258 apply /11 collect), all four functions,35 eager inputs,
176 saved algorithm warning observations and528 declared native frame/category
constructors. The exactly five-file plan2 is
`pandas-variants-next-polars-variants-metadata-plan-2/`; original source declarations
SHA256 is `032814ec3fbb419b464a97f36336b72ab61c89d4b114e337706a65636926894d`.
Root own-metadata verification passes (`be1be5`), with no original payload opens
or admission in that step. Plan1 and its earlier pre-document-update verification
(`396904`) remain preserved. Root then reviewed and approved the separate new
helpers (`034413`, `84be4f`, `3995f5`) and verified exactly15 opaque bindings in
two passes (`e5391a`), before separate census admission (`3d8ca9`). Admission
SHA256 is `806605030b89556bd3338fa796cbcd6753574030a4ef45b15a22011843677dab`.
The all411 canonical census passes (`902af1`): every full audit plan, light intent,
complete case recipe and source/result/category/kwargs/error/plan/warning domain
is bound. Report SHA256 is
`e27b1a567ac60b12285387bc04a3d6c28608cbf4d3025924b375cbae705b0d10` at
`polars-variants-canonical-census-1/report.json`. Sorted IDs joined by LF with
no trailing LF have SHA256
`1e3b6c30324bdac74bbcf8660e1db4ddc46791c50b3f73de8f9bcbca6c986759`.
The census derives20 before /20 after /10 result category observations and30
unique projected category-bearing snapshots; the legacy30 counter's original
definition remains explicitly unproven. Native driver/vector/generator were
opaque only in these census steps. Root subsequently admitted native oracle
parsing separately and passed the complete528-constructor/411-case mapping
(`1da673`), including the whole unchanged vector and retained driver. Its report
is `polars-variants-full-native-mapping-actual-1/report.json`, SHA256
`23cba3b2f3c8625744334ed120e1487eca6eeb366af7c4cca0706292f53d2e32`.
Actual invocation arms are274 raw /134 ordered /3 missing-required default-options;
constructed parameter arms are277 raw /134 ordered, with three unused objects.
The unchanged native411 bank and two extra regressions pass (`bfddd5`) after
building the previously absent executable; the initial NotRun (`173aea`) remains
preserved. No Polars API/UI acceptance is claimed yet.

Focused commit `8f4c70f8` adds the distinct closed Polars variants receipt profile,
25 synthetic cases and guide. All146 receipt integrity tests pass (`766f92`).
Full411 index acceptance passes (`c8ca4d`):411 outcomes,411 complete plans and411
intents,1233 exact raw slices, six rehashed cache hits and no gaps. Independent
source review preserves all eight previous profiles exactly after removing only
additive new gates; report SHA256 is
`a43e28981f4a578e01871ec947acfcf3114925270960de2a86596476b0df8d8a` at
`polars-variants-index-independent-review-1/report.json`. Push `10412b` and fresh
remote readback `b032c9` verify main at
`8f4c70f84168970e23e7ab77a605fa57a7a8e3c7`.

These later Polars receipts are separate from the pandas capsule. Next implement
and validate genuine eager/lazy original calls, complete caller snapshots and
returned-table metadata, then full411 direct/HTTP acceptance and rendered UI.
Catalogue and query counts remain201/565. The older next pandas instructions
below are historical and superseded by this checkpoint.

## Previous checkpoint: all 380 original LOG variants calls reach API/UI — 2026-10-22

Root package terminal `c51997` retains
`docs/retained-public-observation-log-variants-original-call-checkpoint.jsonl.gz`
(763,618 bytes; SHA256
`e8ac773a7f5ac9f246688fd7b13d1dc070c6dfeb10a2723b1610dffb2b307b5a`).
Evidence commit `bc41d9e9` contains the capsule and validation sidecar. The
capsule has 163 own UTF8 members and 53 external bindings. Root executed the
independent new-own-capsule verifier (`731361`): the complete member set,
UTF8 round trips, full hashes and current own bytes all match. No external
payload was opened by packaging or this readback.

The final explicit scope is
`log-variants-original-call-checkpoint-prepared-1/scope.json`, SHA256
`217e7a6330f524fc91b699e718d3062b59e845cdad061edc57f165135f2c1596`.
Root terminal facts are
`log-variants-original-call-finalization-1/root-facts.json`, SHA256
`a9821d467faac3ec096320bf350b291ea02c2e2c48efbeb2fac34e03fec27ae4`.
The independent final-scope review remains outside the capsule to avoid a
self-hash cycle:
`log-variants-original-call-packager-independent-review-1/final-scope-review.json`,
SHA256 `efc6b8dffbe2b7f4c02336794e0888548a71ec5836822c14af77a006885495f3`.
Independent readback report SHA256 is
`e25c135cdb1864151564bfdb49c4d85481f68aa37e06bf34e0d297d5c83b21a4`
at `log-variants-original-call-checkpoint-independent-readback-prepared-1/review.json`.

Four focused source commits descend from `fdd2c172`: `75fd31ac` adds the closed
LOG variants receipt profile; `5abf45b6` adds original backend controls;
`737bf1ce` adds the editor, artifacts and workflow contracts; `d5156b5d` adds
complete native/direct/HTTP acceptance. Twenty own source paths match committed
bytes modulo CRLF/LF. The generated transport include is separately external
(1,317,716 bytes; SHA256
`008a0e8ef5c0b44e773d84562cde25858631d15e99c2b5ffcc7cced100423d8f`).
Original native header, predicate, oracle and shared typed codecs are unchanged.
The inherited two tracked coverage edits remain unstaged.

The complete retained bank is **370 + 10 = 380** calls across all seven methods:
apply 60; filter_log_variants_percentage 58;
filter_variants_by_coverage_percentage 56;
filter_variants_by_maximum_coverage_percentage 55; filter_variants_top_k 58;
filter_variants_variants_percentage 47; find_auto_threshold 46. Outcomes retain
226 complete logs, 32 scalar values and 122 complete native type/message errors.
Independent census and mapping cover all 70 complete native snapshots,
365 trace/log constructor functions, 380 entire native cases and their driver.
All 704 supplied Trace references are bound to actual complete source traces.
Warnings and source/control mutations are zero. Python returned-object and trace
alias observations remain external; transport value equality does not prove
Python identity.

The five-field canonical descriptor is module/function/args/kwargs/call_shape,
with `positional_args_keyword_controls` and the source first for all seven
methods, including the scalar helper. Sorted canonical IDs joined by LF with
**no trailing LF** have SHA256
`3d0e1902b74c68d64d81227822ea5dd726f436c91b31d7717a5283ed59bb6fb9`.
The alternate trailing-LF digest is separately recorded, never substituted.
The closed receipt profile resolves all 380 original raw records, digest-only
audit plans and entered intents with zero gaps and six rehashed cache hits
(`4ec2ac`). All 98 receipt integrity tests pass (`b9502f`); unknown or incomplete
terminal/entry evidence remains a blocking gap and grants no new-call authority.

The dedicated `POST /api/reference-log-variants` takes operation, a genuine
native EventLog File and exact raw `call_json` using
`minera-log-variants-original-call-v1`. Its private parser retains all 17 observed
operand wire tags, ordered string/enum parameter entries, typed admission and
supplied variants dictionaries, ignored values, raw/null/omitted controls,
arbitrary-width integers and source-bound Trace handles. All seven methods
expose actual parsed controls before/after in the fixed 21-field envelope;
the raw descriptor echo is not substituted for those structures. Full native
source/result artifacts and secondary XES export errors remain available.
Scalar artifacts are separate from reusable EventLog Files. The editor preserves
raw BOM/CRLF and large-integer bytes, cancellation and actual filtered File reuse.
Python error.args, qualified exception identity, warnings and alias identity are
not newly transported; native phases are transport metadata, not observer phases.

Runtime acceptance passed: six focused native/regression suites (`17ff13`);
all 380 original native outcomes plus all 380 direct and all 380 owned HTTP cases
(`acdce5`, 86.90 seconds), comparing complete outputs, controls, source-after and
reopened artifacts; 48 focused frontend tests (`cc0422`); all 197 frontend files /
9,898 tests (`065b41`); TypeScript and Vite production build (`685554`). Owned
headless browser acceptance (`c2d3ae`) passed 16 checks, 14 completed POSTs plus one
held actual response, and ten exact downloads. It covers all seven examples,
typed controls, native/control errors, exact imported bytes, both cancellation
paths and real native File reuse. The owned listener PID was verified; browser
and server stopped and stdio drained. Browser inputs, raw responses, artifacts,
PNGs, binaries and SQLite remain external bindings.

Preserve failed attempts and corrections. Static backend review corrected empty
native-error strings and datetime offset validation before the first build. The
first frontend focused run had 47 passes / one assertion failure: the shared
byte-safe multipart helper correctly used a File for raw CRLF, so only the test
assertion changed. Original fixture/browser drafts and reviewer-report failures
remain retained. Final scope review added missing browser recipe provenance and
committed catalogue inputs. The first packaging attempt (`376a0e`) stopped at
source-suffix preflight without member reads or docs writes; the reviewed
correction accepts `.mjs` own source. Prior helper, scope, facts and failure are
preserved; product behavior and expectations were unchanged.

Archive access is explicitly bounded: root separately approved exactly one
known LOG archive for transient UTF8 envelope decoding/reencoding of 20 member
strings to recover full hashes, with no semantic record parse in that step.
Root then separately verified/admitted 14 exact flat/native inputs for census
and mapping. The unchanged receipt integrity regression also opens three prior
LTL containers. **Only packaging/readback have zero original payload opens**;
do not claim global or new-bank archive-zero access. Original producers and
generators were read as own source and never executed/imported. No new reference
calls or reference implementation inspection occurred.

Safe catalogue verification from committed coverage (`fac9ff`) still reports
86 packages (46 complete / 40 partial) and 387 modules (226 complete / 161 partial):
**201 strict open units**, six grouping banks and **565 unentered logical
queries**. This workflow checkpoint grants no catalogue promotion, full-history
closure, absence proof or new public-call authority.

Next is the full **491 = 439 + 52 pandas variants** bank, never just its 357 frame
returns: preserve all 134 errors, four methods and 177 declared full native
snapshots. The exactly five-file metadata plan is
`log-variants-next-pandas-variants-metadata-plan-1/`; report SHA256
`249c1b0ddcca61c991f7f1cbfa2dc12d2813bc4ae9b6f305f9ecd5dab452dd58`.
Saved metadata supplies all requested whole-file pins. Root reviewed and ran its
own-metadata-only `--verify-only` (`170d2d`); no originals were opened and no
admission was granted. Full six-field canonical census, recursive supplied
DataFrame controls and complete 177-snapshot / 491-case mapping remain required.
New root-only admission/census helper preparation is separately frozen and
unexecuted at `pandas-variants-full491-admission-prepared-1/`; preparation SHA256
`c4926cc0f18a6bca8ed04c08ed16227ee33a8a95f3e6e8054c6a08423ae06196`.
Review its source and explicit whole-pin admission before any payload parse. No archive extraction
is needed merely to recover this next bank's saved flat pins.

## Previous continuation: all 726 original Polars attributes calls reach API/UI — 2026-10-22

Root package terminal `822411` retains
`docs/retained-public-observation-polars-attributes-original-call-checkpoint.jsonl.gz`
(1,127,749 bytes; SHA256
`c7c8dfc7d29754a6e9c1054cc18a3e3f611ab912c3959bb661f6e6c921ddf469`).
Its validation sidecar declares 192 own text members and 58 external bindings.
Evidence commit `232bc9ad` retains the capsule and sidecar. Root executed the
independent new-own-capsule verifier (`a1950f`): all 192 members match
current bytes, exact UTF8 round trips and the complete allowlist; no external
payload was opened.
The final explicit scope is
`polars-attributes-original-call-checkpoint-prepared-1/scope.json`, SHA256
`3f3f345a312b0b9409a9540b862e66e17a14b62fbaaca84ce9b7b6604aa35b1b`.
Its independent final-scope review is
`polars-attributes-original-call-packager-independent-review-1/final-scope-review.json`,
SHA256 `e381af45dfbaa130e49087354222a0a9a7b91c3a9c1636805f9961ab5cc0e6a8`.
That review binds the final scope and remains outside the capsule to avoid a self-hash cycle.

Four focused source commits descend from `2dde0eb3`: `3f672dc1` adds the closed
Polars attributes receipt profile; `f7c0992b` adds original backend controls;
`bfb5f302` adds the original-call editor, artifacts and workflow contracts;
`299af794` adds complete native/direct/HTTP acceptance. Fourteen own source paths
match committed source bytes modulo CRLF/LF; the generated transport vector is a
separate external payload binding. The inherited two tracked coverage edits remain
unstaged. Original header, predicate, native oracle and shared codecs are unchanged.

Root reviewed own producers/provenance, freshly verified and explicitly admitted
only the exact three flat banks and immutable own native vector/driver. Original
capture producers and generators were read as own source, never executed. The
complete 700 + 18 + 8 census retains all **726** distinct original receipts across
nine methods, 697 LazyFrame / 29 DataFrame calls and 1,102 complete native frame/
category constructors. Outcomes are 455 exact full frames, nine existing bounded
cutoff frames, 52 unordered typed count maps and 210 complete type/message/phase
errors (126 apply / 84 collect). All full native constructors and original case
bodies were independently mapped. Preserve the nine bounded expectations without
claiming exact tie identities or map iteration order.

The seven-field descriptor is module/function/input/category_observables/kwargs/
call_shape/input_kind, preserving original eager/lazy identity and ordered packed
kwargs. The sorted canonical ID SHA256 is
`128750d0b843f19f8c16ed022ca5d2a0fe86c40a81a190f7c2dd529ec0cbd2bb`
(ASCII IDs joined by LF with **no trailing LF**). The mapper's alternate trailing-LF
digest is separately verified, never substituted for this primary framing. The
complete bank retains 313 parameter dictionaries, 42 raw controls, 371 omissions,
34 enum/string collision calls, original category metadata and 308 outer warning
observations. Python warnings, lazy plans, error.args and alias identity remain
external observations, not newly transported behavior.

The distinct closed receipt profile validates every original raw record slice,
canonical receipt, full audit plan and entered intent. All 726 resolve with zero
gaps and nine rehashed cache hits (`b7a237`). Unknown entry/terminal evidence, mixed
after-call fields, malformed optional plan metadata and unresolved intents remain
blocking gaps; no digest-only or planned-only record grants no-prior authority.
All 80 integrity tests pass (`848c5a`), with the original five profiles preserved.
The initial raw-slice synthetic expectation mismatch and original/corrected adapter
drafts remain retained.

`call_json` selects `minera-polars-attributes-original-call-v1` on
`POST /api/reference-table-attributes`, requiring engine, operation, a complete
native EventTable source File and the raw descriptor. Explicit `input_kind` selects
the existing DataFrame or LazyFrame overload. Ordered keyword entries preserve
missing arguments, raw values, dedicated ordered enum/string parameter entries and
the actual raw union arm of the four observed supplied count dictionaries. The
fixed 23-field envelope retains exact raw call bytes and complete actual native
controls before/after: source kind, keyword order, eight actual operand optionals,
ten native options, parameter state and all count-cache union fields. It does not
substitute the raw echo for parsed controls. Friendly calls retain their defaults;
both table engines now expose the original-call chooser. Imported JSON is forwarded
without parsing/reserializing, and full source/result/control artifacts remain
available on native errors. Count artifacts are separate from reusable table Files.

The first backend compile exposed only an ADL ambiguity, corrected by a private
adapter rename. The first wrapper compile exposed only a renamed-main macro
assumption, corrected without altering the oracle. The first full transport attempt
then exposed an invalid fixture ABI assumption: original Python-int projections
used signed storage while the unchanged native count API returns uint64 storage.
All 52 original maps still pass the unchanged complete semantic comparator. For
each direct and HTTP mode, 52 additional actual-native count checks establish the
native carrier separately, then compare every transport field and reopened artifact
against that carrier. These **104** checks neither change original public expected
maps nor redefine their key/type/value scope. The entire 726-case/driver suffix is
byte-identical through the support correction. All failed attempts and original/
corrected support/vector bindings remain preserved externally where appropriate.

Final complete native terminal `1f5e86` is zero/drained: all 726 original native
cases, all 726 direct calls and all 726 owned HTTP calls pass, with the 104 separate
native count ABI checks (full target 100.66 seconds). Original cases retain their
full native frame/map/error expectations and source state. Direct/HTTP checks add
the complete 23-key envelope, full result/error/source-after, actual controls,
exact raw call echo and artifact reopening. The original renamed main is never
invoked. Initial four native targets also pass (`485ea4`); the prior sandbox HTTP
connection failure and subsequent successful owned-loopback rerun remain distinct.

Final unaffected regression terminal `a564bd` is zero/drained: the same owned
CTest session passes all four complete banks, LOG LTL 297, pandas LTL 429, LOG
attributes 280 and pandas attributes 448, in 753.74 seconds. These are distinct
from the current full 726-case native/direct/HTTP target. Root facts are
`build/parity-closures/polars-attributes-original-call-finalization-1/root-facts.json`
(70,081 bytes; SHA256
`d3efdcd0246f59e3a42508f52a98a480533165035b0f5a1b16b5549db1a3a7d2`).

All 195 frontend files / 9,881 tests pass (`3dcefa`), as do 42 focused tests
(`bfada8`), TypeScript and Vite (`aed181`). Rendered browser terminal `3c1305` is
zero/drained: 23 checks, 20 completed attributes HTTP outcomes plus one held actual
response and eight exact downloads. It covers all nine methods, eager/lazy dispatch,
typed counts/raw count-cache state, enum collisions, apply/collect diagnostics,
exact UTF8/CRLF/large-integer imports, cancellation of an actual response and a
deferred File import, full error artifacts and actual filtered File reuse in a
subsequent native request. Browser examples check rows, type/phase and controls;
complete expected outcomes come from the 726-case fixture. Raw encoded multipart
body bytes are unclaimed. Root verified listener ownership, viewed success/error
screenshots, and stopped both owned processes with stdio drained.

Independent backend, frontend, fixture, index and packager reviews keep their
separate scopes. Packager static review is
`polars-attributes-original-call-packager-independent-review-1/review.json`, SHA256
`6d2dfe08de53a30cb64b02e3d89464b20b5ceefb258626b839f3ee3f349c377f`.
The helper reads only explicitly root-reviewed own text members after complete
external path/hash alias denial; scope/facts bootstrap hashes are mandatory. It
decodes only the new own capsule for exact UTF8/current-byte verification. Original
records, cases, intents, audits, original/native transport vectors and driver,
provenance JSONL, archives, SQLite, binaries and browser raw/download/screenshot
artifacts remain external. External declarations grant no new payload admission.

Archive access must be stated precisely: the new Polars bank pipeline opened no
original archive, and the packager opens no original payload. The existing initial
and final 80-test integrity suites **do reopen three already-admitted LTL archives**
through `docs/retained-public-receipt-ltl-manifest.json`. Do not turn the scoped
new-bank/package boundary into a global whole-turn zero archive claim.

Full scope remains 86 packages / 387 modules, 46 complete packages / 226 complete
modules, **201 strict open units and 565 unentered logical grouping queries across
six banks**. This product checkpoint grants no catalogue promotion, new/replayed
reference calls, implementation inspection, full-history completeness, absence
authority, root-entry certificate or full parity. The broad goal remains active.

Next full candidate is **380 LOG variants calls (370 + 10), all seven functions**.
The bounded metadata-only plan is
`build/parity-closures/polars-attributes-next-log-variants-metadata-plan-1/report.json`
with exact source leads in its `source-declarations.json`. Prior declarations cover
258 returns / 122 errors and 70 native snapshots; they are not a new canonical
census. Five initial duplicate candidates must not be counted as executed calls.
All seven functions take the EventLog source first, including the scalar
`find_auto_threshold` helper; do not copy the source-less LTL-helper protocol.

The next bank is **not admitted**. Bounded plain metadata is missing whole-file
bytes/SHA for both original records, intents and dedup audits; cases currently have
SHA-only declarations. The continuation audit reportedly saves first-bank full
bindings, but that audit remains unadmitted. The known archive has a saved opaque
container binding and own 20-member UTF8 envelope producer; root must review its
producer/provenance and, if needed, explicitly authorize only bounded member-binding
metadata extraction without inspecting or emitting UTF8 member content; any
transient envelope unescaping/byte-hash verification needs explicit bounded admission. Do not open/decompress
the archive, records or oracle from a filename lead alone. Root must freshly verify
and admit exact flat sources/oracle, reconstruct all 380 canonical IDs and complete
native mapping, then freeze the full transport and scalar-helper contract. Typed
tuple-key dictionaries/full trace references and ordinary non-trace diagnostics,
ordered raw parameters, omissions and bounds must retain their native carriers.
Preserve every limitation; do not narrow the bank or replay retained calls.

An additional unexecuted proposal is prepared at
`build/parity-closures/log-variants-archive-member-metadata-prepared-1/proposal.json`
(SHA256 `5fcc3fe986b5fb533d02a2a769959387c6893cb3c8fbe8c01f5a5a1eb934f4bf`).
Its fixed-container extractor is source only, requires a root review certificate,
and admits no archive or flat bank. Root must review the helper, exact provenance,
limits and transient UTF8 byte-hash boundary before any execution. This later
preparation is outside the frozen Polars capsule.

Continue with parallel workers and clean logical commits/pushes while preserving
inherited dirt. Root alone executes generators, targets, tests, builds, browser
acceptance and Git; workers perform bounded assigned source/metadata work. Never
inspect reference implementation/AST/bytecode/private state or run
`scripts/verify-reference-parity.mjs`.

## Previous continuation: all 448 original pandas attributes calls reach API/UI — 2026-10-22

`d3200365` retains the checkpoint in
`docs/retained-public-observation-pandas-attributes-original-call-checkpoint.jsonl.gz`
(718,922 bytes; SHA256
`f58e290565aa5c3d60139b4e30fc9e365fc71153ae4e7be2608250e6921c9665`).
Its adjacent `.validation.json` sidecar declares 167 own text members and 118 external
bindings. Root independently decoded this newly created own capsule, checked every
member against current bytes and verified exact UTF8 round trips (`fa8b61`). Original
records/archives, native oracle, generated vector, scalar before-proof, binaries,
SQLite index, raw browser responses/downloads and screenshots remain external.
Root facts are `build/parity-closures/pandas-attributes-original-call-finalization-1/root-facts.json`
(74,017 bytes; SHA256 `46f0c52f4aefdf1503375f961611600a552057b08dc892f889dc46f20cacd554`).
The packager's exact external declarations are guards, not new payload admission.

Six focused source commits implement this checkpoint: `e25277bc` adds the closed
pandas attributes receipt adapter; `666cfdb1` adds original backend calls and actual
controls; `4bb9e472` fixes independently found unreviewed plan-entry evidence;
`eb810180` adds the original-call editor, exact imports/artifacts and contracts;
`0f234fd9` matches original native decimal carriers; `3c644740` adds complete
448-case native/direct/HTTP acceptance. The six descend from `cc658907`. All fifteen
own source paths match committed bytes modulo CRLF/LF; the generated vector is an
external payload binding. The inherited two tracked coverage edits remain unstaged.

Root reviewed own producers and provenance, freshly verified exact whole-file pins
and explicitly admitted only the two full record banks (429 + 19) and immutable own
native oracle. No producer or original generator was executed; no original archive
was opened. The complete census retains all eight methods: 448 distinct receipts,
335 full returned tables / 113 full type-and-message errors / 150 native snapshots,
zero warnings and zero source/control mutations. Sorted canonical receipt SHA256:
`4c833bf5516f0170a47ade0d1b1959054e2b7574fe9ea9914ee69c867a55cfdf`.
The canonical descriptor preserves original module/method/input_before/args_before/
kwargs_before/call_shape fields. All 150 full native constructors and 448 original
case bodies were independently mapped, with only CRLF/LF normalization; 31,628 typed
nodes were checked. All observed frames are `pandas.DataFrame`, string columns and
unnamed column axes; named/multilevel column fidelity is unclaimed.

The final index (`pandas-attributes-admitted-index-acceptance-3/validation.json`)
resolves all 448 original intents, full plans and digest plans, with zero gaps and
eight rehashed cache hits. Independent review PAI1 found that unknown after-call
plan fields could incorrectly produce a planned-only/no-prior classification.
The family-specific whitelist now blocks that evidence. All 59 integrity tests and
four end-to-end synthetic blocking counterexamples pass; prior four families stay
unchanged. First failed index/path and prior successful generations remain preserved.

`call_json` selects `minera-pandas-attributes-original-call-v1` on
`POST /api/reference-table-attributes`, requiring `engine=pandas`, operation, complete
native EventTable source File and four exclusive request fields. The descriptor keeps
ordered typed args, five wrappers' omitted/raw/ordered enum-or-string parameter
entries and three helpers' ordered direct keyword entries. It retains 321 parameter
dictionaries, fifteen raw controls, five omissions and 107 direct calls. Ten enum/
string collisions preserve method-specific precedence and order; five omitted/null
invocation pairs remain distinct. Threshold dictionaries carry genuine typed keys/
counts. Eight Timestamp controls retain six ns/UTC and two us/UTC values through the
private recursive codec. The fixed 23-field envelope includes exact call bytes and
full actual native controls before/after, including union tags and ordered boundaries.
It does not substitute raw call echo for parsed native controls. Complete source and
result artifacts preserve rows, dtypes, index, categories and attributes. Original
error.args, Python returned-input identity/alias graphs, arbitrary objects/timezones,
unobserved units and nested frame controls remain outside this transport claim.

Native header, predicate, original oracle and shared LOG/LTL codecs remain unchanged.
The oracle is 2,893,157 bytes, SHA256
`cfa46259d2340bbf09af27045a53e8e472834aa7e3c541e503b275538225a582`.
The first fixture compile exposed only a row-storage support-line mismatch; its
correction preserves all 448 case/driver suffix bytes. The first full native attempt
passed six unchanged regressions and all 335/113 native outcomes, then failed on the
new transport's decimal display carrier (`-0.0`/`1.0` versus original constructor
`-0.000000`/`1.000000`). The private parser now uses the same native constructor
formatting, preserving double bits and raw call bytes. New signed-zero, fraction,
recursive parameter/count and whole-control checks pass. The expected vector is
unchanged by this correction (743,481 bytes; SHA256
`3b7e2a9f4202ba7c3c4ad650dd00fd3667150737f702c9cdf4372c6716c1c8da`).

Final native terminal `650484` is zero/drained: all four affected targets pass in
200.47 seconds, including the full public target in 198.24 seconds. All 448 original
native cases verify full native results/diagnostics and source state. Separately,
all 448 direct calls and all 448 owned HTTP calls compare full results, type/message
errors, source-after, actual controls, exact call bytes, reopen behavior and the exact
23-key envelope. The wrapper never invokes its renamed original main.
The earlier seven-target attempt remains exit 1: six unchanged pandas/Polars/LOG/LTL
regressions passed before the private scalar correction; they were not rerun afterward.
Do not describe that mixed attempt as seven passing tests. All 194 frontend files /
9,871 tests pass (`cc5146`), as do 32 focused tests, final TypeScript and Vite build
(`b28f85`). Catalogue checks against a copied committed manifest retain 86/387 and
46/226 (`aed9bc`); never run `scripts/verify-reference-parity.mjs`.

Final rendered browser terminal `2e9dd4` is zero/drained: 19 checks, 16 completed
attributes HTTP outcomes plus one held actual response, and seven byte-exact
downloads. It covers all eight methods, enum collisions, Timestamp native metadata,
typed counts, raw diagnostics, exact UTF8/CRLF/large-integer imports, actual-response
cancellation, deferred File import cancellation, full artifacts even on error and
actual filtered File reuse in a subsequent native request. Browser examples compare
row counts/type/phase and native controls; the complete expected result/error equality
comes from the 448-case fixture. File identity/bytes passed to fetch and next actual
response are checked; raw encoded multipart body inspection is unclaimed. The final
server SHA256 is `c72855d30d32d2769ebe0b11bc56b1f2bca7f244ab226b76c0a1c4b856448276`.
Listener ownership was verified; both new owned processes stopped with stdio drained.
Root viewed success/error screenshots. Browser1's temporal substring assumption failed
on valid raw carriers; browser2 fixed only the assertions; browser3 passed those same
assertions on the final server. All attempts remain preserved.

Independent backend, frontend, receipt index, full fixture mapping, scalar correction
and packager reviews retain their distinct scopes. Packager revision 4 review is
`pandas-attributes-original-call-packager-independent-review-1/revision-4-review.json`,
SHA256 `0f16a4e74935b172217a6686f847ee102767cd938df9d1d9770486043e02a944`.
A static root-facts AST parsing error was corrected before execution; the first draft
is preserved. Other preserved attempts include the initial census frame-kind mismatch,
mapper line endings, metadata serialization, sandbox Vitest EPERM before tests and
the initial receipt-test command typo. Historical EXE/MAP differences are not erased
by matching native source/object pins; current acceptance uses a fresh product build.

Full scope remains 86 packages / 387 modules, 46 complete packages / 226 complete
modules, **201 strict open units and 565 unentered logical grouping queries across
six banks**. This checkpoint grants no catalogue promotion, new/replayed reference
calls, full-history completeness, absence authority, root-entry certificate or full
parity. The broad goal remains active.

Next full candidate is the **726-call Polars attributes bank (700 + 18 + 8)**.
Own metadata-only plan:
`build/parity-closures/pandas-attributes-next-polars-metadata-plan-1/report.json`,
SHA256 `05671209ca8b19925b49c11c20f5e5fb055ac304c031c15276ad89a78e491dd6`.
Its five literal own metadata/source members are retained in this capsule. Declarations
cover 464 frames (455 exact + nine existing bounded cutoff predicates), 52 typed count
maps, 210 errors (126 apply / 84 collect), 1,102 native frame constructors, 308 warning
observations and 81 category snapshots. These are prior declarations, not a fresh
canonical census. No candidate original record, oracle or archive was opened/hashed
by the metadata plan, and it grants no admission. Preserve all nine bounded cases
without claiming exact tie identities, and preserve unordered map scope.

Root next reviews own producer/schema/provenance and freshly verifies/admit exact
flat declarations in that plan's `source-declarations.json`, including all three
whole records and immutable native vectors/driver; intents/dedup payloads need their
own explicit admission. Reconstruct all 726 canonical identities and complete native
mapping before freezing transport. This producer uses source-positional/options-
keyword calls, explicit DataFrame versus LazyFrame and original category observables,
unlike the pandas positional descriptor. Current friendly Polars always builds a
LazyFrame; original eager/lazy identity, typed kwargs/omissions, count unions and
apply/collect diagnostics need explicit transport. Do not narrow the bank, rerun own
capture/generators, inspect reference internals or use broad archive scans. Root alone
executes targets/generators/builds/tests/Git; workers perform bounded static source/
metadata work under separate assignments. Continue with parallel workers and clean
logical commits/pushes while retaining inherited dirt.

## Previous checkpoint: all 280 original LOG attributes calls reach API/UI — 2026-10-22

`e581af39` retains the checkpoint in `docs/retained-public-observation-log-attributes-original-call-checkpoint.jsonl.gz`
(321,141 bytes; SHA256 `e95b0c8700f30461fdb040796ec1590e78b18c6fe156e9bcff115692372adf37`). Its validation sidecar is
`docs/retained-public-observation-log-attributes-original-call-checkpoint.validation.json`. Root independently checked all 93 own text members
against current bytes and exact UTF8 round trips. There are 41
external declarations. Original banks/archives, original full native oracle,
generated vector payload, binaries, raw browser responses and screenshots remain
external. The three next-candidate metadata files are embedded by literal names;
they admit no candidate payload. Root facts are embedded from
`build/parity-closures/log-attributes-original-call-finalization-1/root-facts.json`.

Four focused source commits implement this checkpoint: `2f70df9a` adds the distinct
closed receipt adapter and integrity tests; `d5ef3752` adds backend transport and
controls; `8aaed322` adds complete native/direct/HTTP acceptance; `120d311d` adds the
editor, exact imports/downloads and workflow contract. Root verified all fifteen
owned source paths against committed Git bytes, normalizing CRLF/LF only, while
preserving the inherited two tracked coverage edits.

Root reviewed all six own capture producers, reconstructed their original
whole-file declarations from retained metadata and freshly verified all six
record files before explicit bounded public-observation parsing. Both closed
census implementations reconstruct all 280 canonical receipts, sorted SHA256
`ec5330eb47c1a4611d4790d94a02178cde7e914b48d6f5be6c9b93a5fe88a597`.
All eight methods remain intact: 182 full logs / 98 full errors / zero warnings,
162 source changes and 165 aggregate control changes. Sixteen maximum calls change
caller dictionaries, including three without a source change. The final index
resolves all 280 exact intents with zero gaps and twelve rehashed source cache hits.
All 41 integrity tests pass; two return-identity consistency gaps found in independent
review were fixed without changing any original receipt. Earlier parser generations
and review pin-refusal evidence remain preserved.

`call_json` selects `minera-log-attributes-original-call-v1` on the existing
attributes endpoint. Original mode requires a complete native EventLog File and
exclusive ordered typed args. It preserves omitted parameters, raw typed values
and ordered string/enum entries, including actual mutable maximum-activity writes.
Membership/relative take one operand, numeric operations two, maximum zero or one,
and threshold four or five. Threshold dictionaries retain typed count keys/values
and full source/external trace handles; auxiliary File presence is exact. The fixed
30-field envelope adds call mode/exact call bytes and complete actual cache before/
after snapshots, including raw/dictionary union tags, entry boundaries, all traces,
vc and typed selector. It distinguishes 202 actual parameter dictionaries, four raw
NA values and 44 omissions. All 30 full caches and five external-trace calls are checked.
Original error.args and Python identity metadata remain external; wire errors expose
type/full message. Unobserved ordinary cache values and repeated underlying parameter
names reject explicitly, without claiming arbitrary object or alias-graph fidelity.

The native header/predicate/oracle and shared LOG codec remain byte-identical.
The original oracle is 3,481,841 bytes, SHA256
`657b5ab0e92b78de2e85adc685c1f6e949623f12390ddbeca382cc72a61a2e7c`.
All 153 complete snapshot constructors and all 280 original case bodies were
independently reconstructed; the wrapper invokes original cases/storage controls
directly and never calls its renamed unused main. All 280 calls pass direct API and
all 280 pass owned HTTP with complete result/error/source-after/parameter/cache checks.
Four native/control/transport targets pass (terminal 0/drained `65408a`, 53.78 seconds;
full public target 50.26 seconds). All 193 frontend files / 9,865 tests, 35 focused tests,
final TypeScript (`69385f`) and production build pass. Final server SHA256 is
`03e8dc387a26a69fb4390e30b68d7ce7bc0255567cde1f8f2fbe095859e6af08`.

Rendered browser terminal 0/drained `915dba` passes 17 checks, 15 attributes requests
plus one actual mine reopen and 11 byte-exact downloads. It covers all eight operations,
external caches, caller-only mutation on error, raw NA, exact integer/CRLF imports,
held actual HTTP response cancellation and deferred own File import cancellation.
Complete XES and the independent imported activity role pass unchanged to native
fetch, followed by actual mine response/navigation. Raw encoded multipart body
inspection is not claimed. Listener PID ownership was verified and both new owned
processes closed with stdio drained. Root viewed success/error screenshots.

Preserved attempts include sandbox Vitest EPERM before any test, an interrupted
sandbox native-current build after its banner, two browser help-label locator
failures and two 11th-download burst timeouts. An isolated first XES download passed;
spacing complete-harness downloads passed all assertions on the same product server.
No product or expected-result change was needed for those harness corrections.
Historical native source/object hashes match old acceptance, while old EXE/MAP
hashes differ; no historical binary equivalence is claimed. Fresh product builds
provided current runtime acceptance. Independent backend, index, fixture, frontend,
browser and packager reviews are retained with their exact scopes and failures.

Full scope stays 86 packages / 387 modules, 46 complete packages / 226 complete modules,
201 strict open units and 565 unentered logical grouping queries across six banks.
No new/replayed reference calls, catalogue promotion, full-history completeness,
absence proof, root-entry certificate or full parity follows from this checkpoint.

Next bounded metadata plan is
`build/parity-closures/pandas-attributes-next-product-metadata-review-1/report.json`,
SHA256 `7bebc66a6503a995ce7427cf7e43e5d6607498b5efb1871f53add99067c38be1`.
It identifies the **complete 448-call pandas attributes bank / all eight methods**.
335 tables / 113 diagnostics / 150 snapshots are prior accepted declarations, not a
new canonical census. Two record hashes and the native oracle have flat historical
bindings. Root must review producer/schema/provenance, fresh opaque file bindings
and explicitly admit those exact observations/oracle before parsing. Then add its
distinct closed observer adapter, reconstruct all 448 IDs and complete native mapping,
and only then freeze full original-call API/UI transport. Five parameter-based
wrappers and three direct helpers need distinct operand handling. No next-candidate
record, oracle or archive payload was opened or hashed by that plan; do not narrow
it to convenient parameter/cache subsets.

## Previous checkpoint: all297 original LOG calls reach API/UI — 2026-10-22

`fcb9d598` retains the checkpoint in
`docs/retained-public-observation-log-original-call-checkpoint.jsonl.gz`
(652,958 bytes; SHA256
`66c9fd82dfb550a9477980896fed8c0b252aa916928f4ff784d26b169ddb1b3f`).
Its validation sidecar is 160,816 bytes, SHA256
`0b1286f65c1d33c448ef2f4d157793690979d39ac3742e025a5561f35652fd0c`.
Root independently verified **203 own text members** and exact UTF8 round trips.
There are **261 external declarations**. Original banks/archives, complete native
oracles, generated vector includes, raw browser artifacts, screenshots and
binaries remain external. The four own next-candidate metadata files are embedded
by literal names; this does not admit candidate observations or their payloads.
Root final acceptance is 202,277 bytes, SHA256
`6f6666e8f793892d4c8d68a87b6a03470beff9810aa5c6442588bc3770fc9be4`.
All runtime sessions are terminal/drained; root-facts retain the eleven actual
successful terminal receipts and distinguish preserved failed attempts.

Three clean source commits add original typed EventLog calls:
`c2ba379f` adds strict backend transport and controls; `c856d3d2` adds UI/editor,
helper and contract changes; `e06c43c` adds complete retained-call acceptance.
`call_json` accepts `minera-log-ltl-original-call-v1` with ordered typed `args`.
The four filters require one/three/two/one arguments; both timestamp helpers
require two and forbid source and parameters by presence. Explicit null reaches
native semantics; missing/extra arity is a transport control, not an invented
reference binding diagnostic. Original filters require complete native EventLog
JSON, while friendly CSV/XES/import and helper controls remain the default.

Raw parameter values and the six known ordered string/enum option identities
map to existing native carriers. LOG rejects repeated underlying names instead
of inventing an unobserved enum/string collision policy. The LOG-private recursive
codec preserves NaT as null/date/NativeDate NaT at nanosecond resolution and UUID
as kId, including nesting. It leaves the shared pandas/Polars parsers and all
three predicate sources unchanged. The original LOG oracle remains byte-identical:
4,176,860 bytes, SHA256
`7557ccd210fb886ec26f45083a81292dffe3ef980d8261ede13c2a38c1d0e4ec`.
The shared 27-field envelope retains exact call bytes, null input kind and typed
selector roles, complete source/source-after/result/helper/error artifacts and an
independent imported activity role for filtered-log reopening.

All six admitted LOG banks were reconstructed and independently checked:
**297 original canonical IDs**, sorted SHA256
`566c337e8c65cad2a73dc35d9dccfd5bd5d51515660e8dee834e4fbebdb232d3`.
The fixture binds all **153 original snapshots**, **1,283 recursive typed values**,
**six NaT and four UUID originals**, and all source/helper/error oracle links.
New fixture SHA256:
`45f57882b721f726605246af8da88daab6185d8c9d17a2ca6090bb5c00a87662`.
The wrapper includes the unchanged original oracle, invokes its retained/storage
controls directly, and never invokes its renamed unused main.

Root acceptance is `build/parity-closures/ltl-log-original-call-acceptance-1/`.
Terminal0/drained **7afca7** passes original native297 and **all297 direct plus
all297 owned HTTP**: each transport compares **191 complete logs / 36 filter
errors / 23 complete typed helper values / 47 helper errors**. It checks all27
fields, source-before/after, complete raw-log serialize/reopen, helper attributes,
request/control preservation and exact error type/full message. Original
error.args remain in external receipts; this envelope exposes type/message.
XES is independently
checked as an optional own projection; it never replaces the native log oracle.
Eager return/exception observations establish wire complete/true versus
apply/false; no explicit saved phase or lazy collect domain is invented.
Terminal0/drained **fda680** separately passes original LOG297, pandas429 native,
pandas429 direct/HTTP, and Polars675 native/direct/HTTP regressions. All **192
frontend files / 9,858 tests**, focused **49 tests**, TypeScript and production
build pass. Independent backend and frontend/fixture reviews pass at report SHA
prefixes `97d8e19d` and `8fb7cf62`.

Actual rendered browser acceptance passes terminal0/drained **fdd8e9**:
**23 checks / 21 relationship requests plus one actual /api/mine reopen**. It
covers all six operations, source-less helpers, raw parameters, typed options,
NaT/UUID/NA/null separation, complete raw helper true/false artifacts, exact large
integer/CRLF imports and all source/result/helper/error/call/response downloads.
A held actual native HTTP response is discarded after an editor change; a deferred
own File import is discarded after an operation change. Reopening verifies exact
FormData passed unchanged to native fetch, its full XES File and independent
activity-role field, the actual mine response and navigation. Chromium omits
multipart File postData in its network record; raw encoded wire-body inspection
is not claimed. Both newly owned processes closed with stdio drained and listener
PID ownership verified. Root inspected helper and filter success screenshots.
Final server SHA256:
`be85a226e724e347c65017d63c3fa04b845b18d8b20dd2525011338e45f996a8`.

Preserved attempts include an initial private decoder ADL build ambiguity fixed
by renaming only that adapter function. The direct control subtest passed in a
mixed failing run whose sandboxed HTTP connection failed; the same route binary
then passed with authorized owned-loopback access (**060300**). The failed mixed
run is not a successful terminal receipt. Three browser harness failures retain
the exact source-label locator, the mistaken safe-helper expectation and Chromium's
missing multipart body. The existing helper has an empty cell key and therefore
uses the exact raw-table fallback; whole-object expectations were derived from
unchanged own constructors/serializers without weakening equality or changing
native oracles. Static CSV-key alignment, exact reopening-field proof, helper
status-contract corrections and unexecuted helper drafts are also preserved.
The finalizer/packager has a separate passing independent source review; no
original payload, archive member or algorithm was opened/executed by packaging.

Full scope remains **86 packages / 387 modules**, **46 / 226 complete**, **201
strict open units** and **565 unentered logical queries** across six banks.
No new/replayed reference calls, public admission, history completeness, absence
proof, root-entry certificate, catalogue promotion or full parity follows.
Python key/alias identity and temporal/NaN provenance limits remain explicit.

Next bounded plan is
`build/parity-closures/admitted-next-product-after-log-ltl-review-1/report.json`,
SHA256 `b1ac103500235707f35ba165b1291dfc4c7cec1b42c992f6aadfaffb055d9ad2`.
It identifies the **complete280-call LOG attributes bank / all eight methods**
as the next candidate. Basic native/friendly operations already exist; the gap
is original typed/raw/ordered mutable caller parameters and complete supplied
cache shapes. Its counts are prior metadata declarations, not a new canonical
census. Root must first review/admit exactly the six existing observation banks,
add the distinct closed observer adapter, reconstruct all280 original IDs and
bind the unchanged accepted native oracle. No candidate payloads/IDs were opened
or reconstructed by that plan. Continue the full280 scope after admission; do
not replace it with convenient valid-cache or immutable-parameter subsets.

## Previous checkpoint: original typed pandas calls reach API/UI — 2026-10-22

`dacc2899` retains the checkpoint in
`docs/retained-public-observation-pandas-original-call-checkpoint.jsonl.gz`
(601,625 bytes; SHA256
`960e66788c5098f83356a22ae4f6536d62d6ddf3f58ac7d1ea32e67d9cbcae66`).
Its validation sidecar is 123,916 bytes, SHA256
`23d8686577a49bdb43fa5c7c095675e028cb3b5e01dd72bbe197addf7dbc811f`.
Root independently checked all **151 own text members** against source bytes
and exact UTF8 round trips. There are **161 external declarations**; original
banks/archives, full oracle source, generated vector includes, diagnostic-bearing
initial analysis, raw browser artifacts and binaries remain external. Root final
acceptance is 121,159 bytes, SHA256
`e2650512f490c9be86cd6ab1a0ef34b5244cea0ab3a0987f5f41c47ad55b2925`.
All runtime sessions are terminal/drained; root-facts bindings retain their
actual command receipts and separate independent review scopes.

Three clean source commits extend original typed-call reachability to pandas:
`c9185663` adds strict backend transport and control tests; `26cca8a6` adds the
UI/editor/contract changes; `921a53b8` adds a full public transport acceptance
target. `call_json` now accepts the engine-specific
`minera-pandas-ltl-original-call-v1` schema with an ordered typed `args` array.
Exact arities are one/three/two/one for eventual/NEXT/four-eyes/different-persons.
All 429 admitted calls supply every operand. Missing/extra arguments reject as
transport controls; explicit null reaches native semantics. No missing-argument
reference error or new generic native pandas call API was invented.

Omitted parameters, raw non-mapping values and ordered string/enum entries use
existing native overloads. Parameter keys are retained in the pandas carrier's
`value.key`; enum precedence and typed non-Boolean flags remain native behavior.
The shared 27-field response retains exact call text, mode, complete artifacts,
null pandas input kind and null original-mode selector roles. Polars parsing,
dispatch, original vectors and both predicate sources remain byte-identical.
The original pandas oracle source also remains unchanged: 6,677,515 bytes,
SHA256 `7e2250ee98278cb153bb26e723802277ca60bf4f18eb231bbd1f8e5c01d05ad9`.

Root acceptance is `build/parity-closures/ltl-pandas-original-call-acceptance-1/`.
All **192 frontend files / 9,849 tests**, focused **40 tests**, TypeScript and
production build pass. Corrected direct/HTTP control suites pass terminal
**b9f340**. Original pandas **350 full tables / 79 diagnostics** and Polars
**675 native/direct/HTTP comparisons per mode** pass fresh regression terminal
**638b95**. The new pandas target separately passes original native assertions
and **all 429 calls through both direct workbench and owned HTTP**, each with
**350 complete tables / 79 exact diagnostics**: terminal0/drained **8c2b2d**,
289.76 seconds. Source-before/after, request mutation controls, exact call echo,
full table/index/category/context and 27 response fields are compared. The
legacy pandas observer saved eager returns/exceptions, without explicit phase
fields; wire complete/apply boundaries follow those outcomes, not Polars collect.
No fresh full 297-call LOG differential run is claimed here.

All nine admitted pandas banks were independently reconstructed and hash-checked:
**429 original canonical IDs**, sorted SHA256
`09d3110a437f6ff51870b60b7477377be83ea128f6a8b197e12eb40f357bf39c`.
The generator checks all 268 original snapshot IDs and 1,644 recursive typed
value descriptors. New fixture SHA256:
`4b52230f31557f239739cf0ce0060b472636a457bdede07d4c42c73158bfb280`.
The test wrapper includes the unchanged original oracle, calls `retained_cases`
directly, and never invokes its renamed unused main. The original native target
also runs separately. Independent frontend/fixture and backend/control reviews
pass respectively at `1f1ecbe8` and `ec193fef` report SHA prefixes.

Actual rendered browser acceptance passes terminal0/drained **e9e4ad**:
**14 checks / 12 actual requests** cover all four argument shapes, friendly
defaults, raw null/empty-list/string parameters, both enum/string orders, exact
CRLF/large-integer/NA/bytes import text, complete success/error downloads, stale
clearing, arity rejection and engine transitions. NA/bytes browser parameters
are harmless unknown literal controls testing transport; the full native bank
provides their admitted behavioral comparisons. Both owned processes closed
with output drained and listener PID ownership checked; root inspected both
screenshots. Server SHA256:
`dd4952e40cf153b9bc4cf7cc5e2f1462589345dbc5a152a957b674985466ab56`.

Preserved attempts include two own control-test failures caused by expecting
schema validation ahead of the correct unknown-field guard. Both expectations
were corrected and separate schema-only guard cases added; product parsing and
original outcomes were unchanged. The first browser recipe was rejected during
static review because its expected table omitted pandas index metadata. A new
explicitly indexed own upload and complete label subsetting corrected the
recipe without weakening full equality. The unaccepted browser recipe was not executed.
Checkpoint helpers were also corrected before execution to admit own members
before opening them and keep the diagnostic-bearing first analysis external.

Full scope remains **86 packages / 387 modules**, **46 / 226 complete**, **201
strict open units** and **565 unentered logical queries** across six banks.
No new/replayed reference calls, admissions, historical completeness, absence
proof or catalogue promotions follow. Typed temporal provenance, arbitrary
Python keys and other existing carrier limits remain explicit.

Next read-only design is
`build/parity-closures/admitted-log-ltl-next-product-review-1/report.json`,
SHA256 `8a2bbf76d2bf2d9b5784ced3dd3a087e29763d74210534f8a387affab0355afe`.
It verifies all **297 LOG IDs** and retains the full **227 filters + 70 helpers**
as the next acceptance scope. Ten receipts require exact private typed codec
extensions: six NaT and four UUID values. Follow the existing own native
constructors; do not coerce them to ordinary null/string values or reduce the
bank. No LOG product implementation or new reference calls have started.

## Previous checkpoint: original typed Polars calls reach API/UI — 2026-10-22

`ca20fd47` retains this checkpoint in
`docs/retained-public-observation-polars-original-call-checkpoint.jsonl.gz`
(618,416 bytes; SHA256
`b3d8021ac792c4b0e300e79ce32eb32620afba7dc81c6a5059e2b2861ac03d19`).
Its validation sidecar is 84,459 bytes, SHA256
`a572ff65b1927bf81e762f4a8e2304537eaa6eb7403143cce9f46b90a9f04f3d`.
Root independently verified all **120 own text members** against their source
bytes and exact UTF8 round trips. There are **104 external declarations**;
original receipts/archives, both generated public vector includes, browser raw
artifacts and runtime binaries remain external. Final root acceptance is
62,531 bytes, SHA256
`ce95925a5609c8ecb9fe24d2835fa2cc843a83db006cf726dd4406cb954ff09f`.
The packager's initial overly broad browser-directory guard rejected its own
helper during plan-only validation; the corrected bounded guard and failure
metadata are retained. No original payload was opened by packaging.

Three clean source commits extend the previous input-kind checkpoint:
`1e993417` adds strict backend original-call transport; `57676f9d` adds the
frontend editors, inspection/downloads and workflow documentation; `86bd11ea`
adds complete retained-call transport acceptance. The original predicate and
675-case native oracle include remain byte-identical.

Optional `call_json` exclusively selects original Polars mode at
`POST /api/reference-ltl-filter`. The required schema is
`minera-polars-ltl-original-call-v1`. Preserve omitted versus explicit null
operands, omitted/raw parameters, and ordered string/enum mapping entries.
Both DataFrame and LazyFrame use the existing native generic overloads. Strict
JSON grammar, decoded duplicate keys, mixed controls, unsupported enum identities
and unrepresentable operand-map order reject before source import. Recognized
native semantic errors retain their exact apply/collect boundary and message.
The response now has **27 fields**, adding `call_mode` and exact native-string
`call_json`; original-mode selector fields are null instead of invented friendly
defaults. Other engines reject `call_json`.

The friendly form remains the default. Original mode supports operand/parameter
editors plus an authoritative complete-call editor and strict UTF8 file import.
Raw integer tokens and imported CRLF are retained rather than parsed/reformatted.
Edits/import generations cancel stale work; frontend identity checks bind the
exact original call, mode and input type. Complete call/source/result/HTTP-response
downloads remain available at their valid success/error boundaries.

Root acceptance is `build/parity-closures/ltl-polars-original-call-acceptance-1/`.
All **192 frontend files / 9,843 tests**, the focused 34 tests, TypeScript and
production build pass. Native workbench and HTTP control suites pass terminal
**3c9a4b**. All **675 original Polars calls** then pass twice through the new
transport: direct workbench and owned loopback HTTP, each with **516 complete
frames / 159 full diagnostics**, full source before/after, exact errors/phases,
27-field envelope and request mutation checks. The original 675 native
comparisons also pass. This test completed terminal0/drained **698cda** in
104 seconds; native build terminals are **8e375b** and **44a981**.
No fresh full 297-log or 429-pandas native differential run is claimed here.

The new transport fixture reuses 236 original frame functions. Its generator
independently reconstructed all original canonical receipt hashes and the
unchanged sorted set `4b777fc6cfbe75bc7c90cf73022160f54b7da02bb8f3a7b5437f2c12082968b1`.
Final fixture SHA256 is
`78157fdb5359e253f4c8d17a2954adf98ca317b7accb57d1eee9a3cb5089ce50`.
The initial single-function layout is retained; per-case noinline wrappers have
identical 675 assertion calls and provenance bytes. No expected outcome changed.
Independent backend/fixture review passes, SHA256
`92a19d85c640e6cddf1386b3f77a5cfc7161a14a0b681be7653032843a6a3777`.

Actual rendered browser interaction passes terminal0/drained **e46503**: nine
checks / seven actual requests cover friendly default, missing/null/raw parameter
errors, both string/enum conflict orders, eager complete-call import, exact
CRLF/large-integer text and downloads, stale clearing and inactive pandas mode.
Root inspected both success/error screenshots. The owned server and isolated
Chrome closed with output drained and listener PID verified. No existing
process was replaced. Server SHA256:
`ef12532ba4709f1ec5105a3d919316acb972e798fddbd3f84006d6f9a9e939b9`.
The first browser helper failed syntax checking before any app execution;
the failed helper and corrected independently reviewed recipe remain retained.

The 1,203 projected typed values represent the admitted domain, not arbitrary
Python object reconstruction. Nonstring keys, arbitrary ordered operand maps,
custom enums, temporal provenance and other contract gaps remain explicit.
Full scope remains **86 packages / 387 modules**, **46 / 226 complete**, **201
strict open units**, and **565 unentered logical queries** across six banks.
There are zero new/replayed reference calls, new admissions or catalogue
promotions. Historical reconstruction and whole-bank dedup gates still apply.
Continue the analogous already-admitted pandas original-call surface next;
the Polars interface does not close the wider family or catalogue.

## Previous checkpoint: Polars input types reach API/UI — 2026-10-22

`32391002` retains the verified own-evidence checkpoint in
`docs/retained-public-observation-polars-input-kind-checkpoint.jsonl.gz`
(258,912 bytes; SHA256
`e234afc88156aa18693876f8bc2f9e1ab6f4f352be86a49df6041662aa626e4b`).
Its validation sidecar is 180,214 bytes, SHA256
`fa04f74461de121b886eefac5f27a00138d6bbfb91515921e9f75d6feb5d9324`.
Root independently checked all 124 members against their current source bytes
and exact UTF8 round trips. The checkpoint includes 73 frozen acceptance members
and 288 external declarations; raw original receipts, diagnostics, generated
public vectors and original archives remain external. Root final acceptance is
53,196 bytes, SHA256
`bf023c0002ed2a93c61d8b1cf8b07a7bc2b8b3af76eb403fb5412118f6efe90c`.
This is retained engineering/provenance evidence, with zero reference calls and
no new public admission or catalogue promotion.

`d217372a` exposes the existing eager `DataFrame` and lazy `LazyFrame` Polars
LTL overloads through `POST /api/reference-ltl-filter` and the relationship
workspace. Optional `input_kind` accepts those exact public names for Polars
only; omission preserves the lazy default. The fixed response envelope now has
25 fields, including the validated effective input kind. The frontend rejects
cross-kind responses and clears pending/stale results when the choice changes.
Invalid, inactive and duplicate kind controls reject before source import.
See `docs/ltl-filter-workflow.md` for the wire contract and user workflow.

This is product reachability work from already admitted public evidence:
**675 original Polars receipt hashes** and **16 exact eager/lazy pairs** were
independently asserted (659 lazy / 16 eager; eager five returns / eleven apply
errors). No new reference calls or admissions occurred. The ordinary eager
eventual case returns sorted rows while lazy returns source order; eager NEXT
retains its apply-time `AttributeError`, full source artifacts and no result.
The complete native/UI supported domains remain bounded by the existing
contracts; this is not an assertion that every raw-call shape is now exposed.

Root acceptance is `build/parity-closures/ltl-polars-input-kind-acceptance-1/`.
All **192 frontend files / 9,833 tests**, TypeScript and the production build
pass. Final native build direct exec **ce49dc** and final CTest **99da16** are
terminal0/drained. Three assertion-enabled suites pass: all **675 retained
Polars cases** (516 complete frames / 159 full diagnostics), direct workbench
workflows, and an owned HTTP route with strict multipart/control cases. No
fresh full 297-log or 429-pandas native differential run is claimed here.
Independent source/receipt review also passes **208 extracted frontend
transport-condition checks**. Final combined review:
`ltl-polars-input-kind-final-independent-review-1/review.json`, SHA256
`e9cadf6b7fe50aab4d4ad9dec71555ba19d6ecc6cb74b172ab25d5ff5e0b123b`.

The first native run exposed an existing execution-code-page bug: three narrow
`\u03c0` literals became `?` in the captured diagnostic (3,103 actual versus
3,106 expected bytes; first difference at byte 320). `882c6026` replaces only
those literals with explicit UTF8 byte escapes. Predicate code and all original
675 vectors remain byte-identical; no oracle was weakened. Failure/proof and
before/after formatter sources are retained under `ltl-polars-diagnostic-utf8-fix-1/`.
The first HTTP failure was sandbox loopback access; the same owned route passed
outside that sandbox, as did final CTest. The earlier frontend EPERM attempt ran
zero tests and remains preserved. Failed independent/static harness attempts
and subsequent corrections are retained rather than rewritten.

The **final rebuilt executable** passes actual rendered browser interaction,
direct exec **f67f4c**, under `browser-owned-2/`. It covers public navigation,
the lazy default/eager choice, complete six-row ordering and table context,
source preservation, stale-result clearing, exact success downloads, and exact
source/source-after/HTTP-response downloads after eager NEXT failure. Both the
fresh owned listener and isolated Chrome process are confirmed closed with
output drained; listener PID ownership is checked. Existing application
processes were not replaced. Server binary: 16,896,000 bytes, SHA256
`aad4e424088a557e574032848fba9d0a1ddff0b0532c9cd3e15a84cc3c91e141`.
Root visually inspected the rendered workspace; the first and final browser
runs are preserved separately. This supersedes the earlier workflow checkpoint's
unverified rendered-browser status for this bounded flow.

Parallel provenance work remains separate from new-call authority. The G57 DFG
native container now has an exact opaque compressed-to-uncompressed hash link
(one archive opened and gzip bytes fed only to SHA/count; no JSON/member decode).
Four further heuristic singleton own producers were reviewed: aggregation,
business-hours, DFG values and local labels. Their compressed mappings remain
unverified; business-hours has an unresolved producer-pipeline link, and
arithmetic/nonfinite auxiliary scopes remain unresolved. Exact-CRLF evidence
validation failed first, then passed in preserved `corrected-1/` output. Five
of the prior 60 singleton producer definitions are now reviewed; **55 producer
definitions remain unreviewed**. G60 received a bounded review, but its OCDFG
receipts-wrapper producer is unresolved. Its separately identified raw capture
has a prior declared 77-call scope and is only a future schema-review candidate,
not a newly verified row count, complete canonical receipt set or new admission.

The next product slice is proposed read-only in
`admitted-ltl-next-product-slice-review-1/report.json`, SHA256
`1cf0050ea75e8f6e933fceea454a321897c4dcffe1b7da1bba1a770f0d7eb104`.
It asserts all **1,104 admitted pandas/Polars receipt hashes** and identifies
missing-operand, raw parameter-value, enum-key and non-boolean-control behavior
which friendly UI controls cannot currently express. Implement an exclusive
original typed-call mode using the existing native generic carrier, preserving
the friendly default. No implementation or new calls for that next slice have
started. Existing EventLog `OriginalLtlFilter` controls do not supply the table
companions' original public raw-call surface.

Full scope stays **86 packages / 387 modules**, **46 packages / 226 modules
complete**, and **201 strict open units**. The six prepared banks remain
**565 unentered logical queries**. No catalogue row, complete historical audit,
absence proof or new-family entry certificate follows. Continue full relevant
history/receipt reconstruction and whole-bank dedup before new reference calls;
never restart the terminal 2,844-source discovery or replay retained calls.

## Previous checkpoint: files-object discovery and residual provenance — 2026-10-22

`ed4dfdbe` adds two discovery-only files-object profiles for exactly seven
reviewed own producers. Accepted parser SHA256:
`4b96608736aead97860a9ca915e99ef3ae558ef03bc8cfb01604deb244d45106`;
tests SHA256:
`a660b8742b34c6468514472e2a66380ca65cc7482a5669b6f9d772cd4f450bd7`.
All **91 receipt-tool tests**, **90 independent synthetic checks**, and **15
projection-guard tests** pass. The 91 include the original LTL receipt-archive
integration. There was **no fresh current-parser LTL streaming run**; retain
the prior parser's separately bound streaming acceptance below.

Root acceptance is `build/parity-closures/streaming-files-object-root-acceptance-1/`.
The actual metadata run finished terminal0/drained as direct exec **5dddaa**.
It discovers **289 logical members** across seven sources (3/11/6/5/98/108/58),
with **281 declared sizes and eight missing original sizes**. All 289 payload
hashes remain unverified declarations and all source-file identities unavailable.
Observed EOF structure/name/metadata checks pass, but no independent original
file count was saved: each source retains its explicit unknown-count gap.
Together with ten opaque branch gaps, **all 17 gaps remain**. The original wide
scan did not record these nested members; this is their first metadata extraction,
not an equality claim against old member edges.

Raw events: 306,413 bytes, SHA256
`c86307f9f19579bab91e43c508497bc133b9a65de34f35de9b2bd69fa23f0fe1`.
Seven cache hits reproduce all remaining raw event bytes exactly. The fresh
key-wrapper regression retains all 416 declarations and three wrapper gaps,
with event bytes identical to the accepted prior run. Root checks are in
`files-object-root-check.json` and `final-regression-validation.json`.
Independent actual review passes with no findings:
`files-object-actual-metadata-independent-review-1/review.json`, SHA256
`5e44e4de6035760a75c8dcc5901199d05c2926c5afa131193d00a559bd64215d`.

The current guarded projector is
`full-member-projection-scope-guard-1/revision-4/project-declared-full-member-candidates.py`,
SHA256 `261e46800114540b3372357bdbd96f2c548ba8460c0e3d304121e72ac70ecac7`.
Both files-object schemas are refused even with missing/false logical flags.
Prior full/UTF8/chunk/missing-size behavior passes the six-table/view differential
checks. The failed initial synthetic fixture remains preserved. Actual projection
in `files-object-scope-projection-root-1/` retains **289 member-metadata edges
and 289 refusal gaps**, all `identity_id=NULL`, `full_declaration=0`, with zero
full-file identities and zero flat matches. All 17 original source gaps remain.
SQLite: 970,752 bytes, SHA256
`2838b9cabd8625613bb0189b53d38007a9201e2b7689cc515690cef36b215b81`.
Independent event/SQL/cache/scope review passes with no findings:
`files-object-scope-projection-independent-review-1/review.json`, SHA256
`65eea4eb712b01dcb99e04009e8d5f949a58a12e027582cab34902dbbd88cee0`.

The path-gap overlay matches **30,589 of 30,638** original path-absent rows to
exact corrected metadata declarations. The **49 residual rows** comprise 31
tails, eight legacy manifests, three report/cases wrappers, one four-field header
and six token-root rows. Thirty-eight have corrected boundary candidates which
remain gaps; eleven lie outside those selections (five synthetic empty tails
and six token-root rows). The other 176 original gaps and all 64 correction gaps
remain. **225 is original-row accounting, not a current gap union or closure**.
Report: `grouping-history-path-gap-overlay-review-1/report.json`, SHA256
`bbe3d6e74100b6edce2bd40b4d6d1ba01da10431483b6453495210b0557b7b62`.

Own producer review identifies the five mock-tail sources as synthetic empty
fixtures, with no real-reference authority. The six token rows span producer,
cycle and self cases in helper/replay modes: two have recorded result-key
candidates and four may represent entry/resource termination. None is an absence
proof or complete receipt. Original values remain unparsed. Report:
`grouping-history-residual-root-schema-review-1/report.json`, SHA256
`c0a453d45da8deb1cff8d3bc8ae5502ea52af71b18a2992096092729eda9b529`.

Root also verified five exact compressed-to-uncompressed producer SHA mappings
under `grouping-history-opaque-uncompressed-root-1/`, terminal0 direct exec
**a17189**, with zero failures. This operation **opened five original archives
and decompressed gzip bytes opaquely** into SHA/count checks. It performed no
JSON/Unicode/member decoding, extraction, source inspection or payload display.
Unknown original uncompressed sizes remain null; observed byte counts are
separate. These bindings strengthen producer provenance only, with no receipt
completeness, public admission, absence or entry authority.
Independent metadata/static-byteflow review passes:
`grouping-history-opaque-uncompressed-independent-review-1/review.json`, SHA256
`29765d14d8c8ce66106ab84b2d17c0f0f3b2014ddc6c0e08e0f8ae14cec9af0c`.

Preserved in `6a0923a2`:
`docs/retained-public-observation-files-object-checkpoint.jsonl.gz`, **199
members / 8,460,106 bytes**, SHA256
`ba2a58505568f593204aa610e01538835cc15999b137a2e7468aca5f201e3f5b`.
Root checked every member against its exact current UTF8 source bytes; see
`files-object-capsule-root-byte-check-1.json`. The sidecar retains 38 external
artifact bindings. The packager opened no original archives; the earlier five
opaque gzip checks are explicitly separate operations. The capsule preserves
the complete 30,638-row relation overlay, original gap metadata, failed drafts,
scope guards, independent reviews and a frozen review of this section. Its live
capsule sentence was added after that frozen review and the root byte check.

Full scope remains **86 packages / 387 modules**, with **46 packages and 226
modules complete; 201 strict open units**. The six banks remain **565 unentered
logical queries**. No family certificate, history-completeness claim or catalogue
promotion follows. Sixty singleton object-root ownerships remain outside this
checkpoint's completed schema reviews;
comparison/public-result branches also remain opaque gaps. Continue relevant
flat/archive provenance and complete original canonical receipt reconstruction,
hash assertion and whole-bank dedup before new calls. Do not restart the terminal
2,844-source scan or replay retained calls. No product/native/frontend acceptance
was freshly established by this tooling checkpoint.

## Previous checkpoint: key wrappers and remaining object schemas — 2026-10-22

`ff1162f3` adds discovery-only `key-origin-size-base64-members-v1`. The accepted
parser SHA256 is `2dacc29e9cb14cfab370a953d28d0cd1640bf0aa352934db6b0cbace4b9f6061`;
tests SHA256 is `e832a5452c5a93b83c766f19983742e92d1924cab2bdb8465962b2f221c78c4b`.
All **82 receipt-tool tests**, **64 independent synthetic checks**, and **12
corrected projection-guard tests** pass. The source/cache reader is unchanged;
the new profile requires reviewed producer/count/wrapper-sequence controls and
rejects admission before source reads or parser creation.

Root output is `build/parity-closures/streaming-key-wrapper-root-acceptance-1/`.
The real bounded metadata run finished terminal0/drained as direct exec chunk
**4bd7e5**. It recovered **416 logical file aliases** (29 dispatch / 387 Petri
producer), while **three public report/cases wrappers remain opaque gaps**.
Both EOF metadata/count/order/name-uniqueness checks pass. All file sizes and
hashes remain declarations; no payload byte/hash verification or source-file
identity follows. `origin_path` is provenance only and is never followed.
Raw events: 485,784 bytes, SHA256
`0b407279fae0926979552cefdadbcfb23d46869296c55c4daa86a20150827608`.
Two cache hits reproduce every event after removing only the hit rows.

Independent review matches all **419** original source/ordinal/type/event-SHA
relationships and all 416 declared member hashes. The **416 sizes remain
unverified declarations**; the original index had none. Names including
`head-source/` and `final-source/` remain logical aliases. Review:
`key-wrapper-actual-metadata-independent-review-1/review.json`, SHA256
`84d22027cbd517abb17dc9ec99d9631bc097b89334e5a152859dc648df39c738`.

Use only the corrected projector
`full-member-projection-scope-guard-1/revision-3/corrected-1/project-declared-full-member-candidates.py`,
SHA256 `d50727b7cb0d36d5081cff018e3df6115dee0c611ceb8b80ba8da88741005e51`.
It explicitly refuses both known logical schemas even with missing/false flags.
The initial revision3 set-membership compatibility bug on list/dict schema
metadata is preserved and superseded by tuple membership plus regressions;
no real input used that draft. Existing full/UTF8/chunk/missing-size behavior
matches the prior projector in the six-table/view differential tests.

The actual projection is terminal0 under `key-wrapper-scope-projection-root-1/`.
It retains **416 member-metadata edges and 416 refusal gaps**, all with
`identity_id=NULL`, `full_declaration=0`, zero full-file keys and zero matches.
The three original wrapper gaps remain in the bound event stream. SQLite:
1,454,080 bytes, SHA256
`1c1648c379fb1930983e1e6c0e95cc6158a91a8424564df4f912a7ebaabb3bb3`.
Independent hashes/event/SQL/cache review found no issues:
`key-wrapper-scope-projection-independent-review-1/review.json`, SHA256
`98cc2a9ce363271b959e52ec4e72996d5fa85914f3dfa1732b96429d2399c9c3`.

Fresh regressions are terminal0/drained: LTL session **16352** preserves all
**1,401** original receipt identities in three verified sets with zero gaps;
four-field direct exec **7e0996** preserves all 776 declarations and its header
gap. Both event files are byte-identical to accepted parser a968 outputs.
Root validation is `streaming-key-wrapper-root-acceptance-1/final-regression-validation.json`.
No reference algorithms were called by these receipt/metadata regressions.

Preserved in `b6d4f6d2`: `docs/retained-public-observation-key-wrapper-checkpoint.jsonl.gz`,
**155 members / 722,403 bytes**, SHA256
`fe4ddad9906c0fa931178c3f3f86185fab1ce253f0cdfbe8096c367fe02ede20`.
Root checked every member against its original exact UTF8 bytes. The sidecar
retains 39 external artifact bindings; original archive bindings remain prior
declarations and the packager opens none of them. All failed drafts, compatibility
corrections, independent reviews and final regression evidence are preserved.

Four entry-like metadata gaps were traced to own LTL producers/sidecars:
three root-object gaps and one acceptance-header gap. The older 420 checkpoint
declares the same first four bank hashes as current429; those copies are not
additive call totals. Existing429/Polars675 admissions remain scoped evidence.
Polars has zero generic member edges because its 675 outcomes use a direct
records array: zero projected edges never implies zero calls. This review
opens no original payloads and grants no new admission. Report:
`grouping-history-direct-entry-schema-review-1/report.json`, SHA256
`6aed43df87214f5a4a5a4eff03696011a7912a044e35ae21d1bee8e7a1215759`.

The 73 unknown object roots form **64 exact recorded shapes**. Four repeated
groups cover 13 sources whose own producers were reviewed: **nine exact
compressed sidecar links, four uncompressed-only links**, with **60 remaining
ownerships unreviewed**. Nested runtime token types were not captured in the
old metadata and remain unverified. Report:
`grouping-history-object-wrapper-schema-review-1/report.json`, SHA256
`59de627eab160ba29089e07edd8b0048eca88c0204e344d9e60b20bb3ffc5cc9`.
The then-recommended next scope was files-object discovery for exactly seven
producer-backed G02/G03 sources, with separate size-present/absent forms.
Its isolated draft/review under `streaming-files-object-{development,independent-review}-1/`
had not then changed the accepted parser or run against real archives. The current
continuation above supersedes that draft status. All other
scope/summary/binary/comparison branches remain opaque and gapped.

Full scope remains **86 packages / 387 modules**, with **46 packages and 226
modules complete; 201 strict open units**. The six banks remain **565 unentered
logical queries**. No public member admission, new-family certificate, complete
historical audit, absence proof or catalogue promotion follows from these
metadata checkpoints. Continue relevant flat/archive provenance and complete
original receipt reconstruction/hash assertion/whole-bank dedup before entry.
Do not restart the terminal 2,844-source scan or replay retained algorithm calls.

## Previous checkpoint: logical-member metadata and scope guards — 2026-10-22

`64cd994e` adds the discovery-only
`name-data-base64-four-field-members-v1` profile. The accepted parser SHA256 is
`a9685b98c47dfcaf5728cece2b450f09cc7abfbdffaac17cd5e5c2247336bf03`;
tests SHA256 is `b7101f0d397f55167a7ba362798d7b46b6374776b8f6abd9b5eb33b708b24d73`.
All **73 receipt-tool tests** pass, plus **44 independent synthetic checks**.
The earlier private nested-key materialization failure and its frozen draft
remain preserved. The new lexical skip applies only to this explicit profile.

Root output: `build/parity-closures/streaming-four-field-root-acceptance-1/`.
The bounded actual metadata run finished terminal0/drained as direct exec chunk
**41130a** (no ongoing session handle). Exact source/producer/sidecar/review
bindings were checked before preparation and after completion. It recovers all
**776 unique names and logical size declarations**, comprising **733 ordinary
logical members and 43 binary slices**. Header/count/name uniqueness checks
pass, but **one header/summary/binary-mapping gap remains unadmitted**. No
payload was decoded. Raw events: 674,530 bytes, SHA256
`0582bc65315f3c98d230d7bdf90f3dec7f7a3c3651b0d39ea23af77d12b54b29`.
The one-hit cache replay matches every event after removing only `cache_hit`.

Independent review matches all 776 original source/ordinal/declared-SHA edges,
650 source-name SHA mappings, and all 43 binary-slice names/counts/sizes against
eight sidecar series. The **733 ordinary member sizes remain declarations**;
the sidecar has no complete ordinary-size inventory. No member byte/hash or
whole-file mapping was freshly verified. Review:
`four-field-actual-metadata-independent-review-1/review.json`, SHA256
`4d7fd164b9266a775ede3ff96e31aa58f3e8cd054d40e0a752a083041b24d0c5`.

Both fresh regression runs are terminal0/drained: LTL session **90351** preserves
all **1,401** original receipt identities in three verified sets with zero gaps;
UTF8 session **67744** preserves 12,313 members and all 52 boundary gaps. Both event files
are byte-identical to accepted parser196 outputs. Read
`streaming-four-field-root-acceptance-1/final-regression-validation.json`.

**Never run the old projector17f6 on logical-member events.** The new reviewed
helper is `full-member-projection-scope-guard-1/revision-2/project-declared-full-member-candidates.py`,
SHA256 `0031b63eca56737ff8468ac5a3d44875b7bc9483efeb933aafb523efb7988e8e`.
It refuses the known four-field schema even with missing/false logical flags,
and also refuses explicit logical/slice/unavailable-source identities. All
eight tests pass, including six synthetic CLI projections and exact old/new
six-table/view comparison for prior unscoped declarations. The first guard's
schema bypass and its preserved counterexamples remain evidence, not acceptance.

Actual scope projection is terminal0 in `four-field-scope-projection-root-1/`.
It retains all **776 original member-metadata edges and 776 refusal gaps**, with
`identity_id=NULL`, `full_declaration=0`, zero full-file keys and zero flat
matches. The wide header gap also remains. SQLite: 2,150,400 bytes, SHA256
`e86dbd21d70e099098a7438674d7922c519f380ab85722182ec731c7f987fb97`.
Independent input/output/event/SQL review found no issues:
`four-field-scope-projection-independent-review-1/review.json`, SHA256
`c3cc42dc4b2827f3c867573aeaf681e26d370a5b1f19b6e703bd52434e737306`.

Preserved in `303193c6`: `docs/retained-public-observation-four-field-checkpoint.jsonl.gz`,
**108 members / 1,490,413 bytes**, SHA256
`dbf900c80daa06d8f44a782124d965bd45f92014acc682bcb549106082d0ceb9`.
Root checked every member against its original exact UTF8 bytes. Eleven external
metadata bindings retain caches/databases/baselines/full sidecars without
embedding them; original archives are only declaration-bound and unopened by
the packager. The capsule includes all failed drafts and independent reviews.

The next key-wrapper review identifies two exact sources with **416 file rows
and three public report/cases wrappers**. Some keys are logical aliases such as
`final-source/` or `head-source/`; `origin_path` is provenance only and must
never be followed. The wrappers preserve partial historical observations,
including saved error/graph aliases, not complete canonical receipts. Keep
values opaque and gapped; producer counts do not establish absence. Review:
`grouping-history-key-wrapper-schema-review-1/report.json`, SHA256
`7df2a2270cbc8740749bea3b4044d86a195cb2d17dd9115342674757776a3b02`.
An isolated adapter/review is in progress under
`streaming-key-wrapper-{development,independent-review}-1/`; it has not changed
the accepted parser or run against real archives. Future projection must also
guard the new schema explicitly before use.

All **86 packages / 387 modules** remain in scope, with **46 packages and 226
modules complete; 201 strict open units**. This checkpoint calls no reference
algorithms, admits no public members, and issues no new-family entry certificate.
The six banks remain 565 unentered logical queries. Continue complete relevant
flat/archive provenance, original full-receipt reconstruction and whole-bank
dedup before entering new calls. No catalogue row is promoted by metadata alone.
Do not restart the completed 2,844-source discovery or replay retained calls.

## Previous checkpoint: UTF8 history metadata and hardened gates — 2026-10-22

`ec8a792f` adds discovery-only `utf8-name-text-members-v1` and prevents direct
chunk admission before parser construction. `f94eb500` preserves hardened
ActivitySplit recipes. All **65 receipt-tool tests** pass. The frozen parser is
SHA256 `19623eb61014c866bc6a241b532adefbe985aa64ae00f2bda04a7988d4b64837`.
No reference algorithms were called in this checkpoint.

Root acceptance is under `build/parity-closures/streaming-envelope-root-acceptance-3/`.
The actual LTL run (session **6440**, terminal0/drained) preserves all **1,401**
original receipt identities, with zero gaps. Its verified cache replay has three
hits. The legacy scale regression still finds 5,870 members with zero gaps.
The direct-parse chunk bug, frozen predecessors, failed repro and independent
fix review remain preserved rather than overwriting the earlier evidence.

The UTF8 metadata correction (session **93699**, terminal0/drained) recovered
**12,313 member names and full logical size declarations from 26 distinct
nonempty contents**, including one explicitly mocked member. Twenty-five
nonmock sources account for 12,312 members. Text stays lexically opaque;
**52 header/tail candidates remain 52 gaps**. All 26 ordered declared inventory
hashes/counts/totals independently match their tails, but payload hashes remain
unverified and no members are admitted. Cache replay has 26 hits. The final raw
event stream is 7,454,999 bytes, SHA256
`f124a1ec5e65565e4110a15ab9baf658e0ba5cb73ee308e2a7c084177e4e3fb6`.
The first selection incorrectly equated 31 boundary candidates with 31 nonempty
sources; that failed assertion and the corrected 26-source selection are retained.
Preserved in `20da383e`: `docs/retained-public-observation-utf8-checkpoint.jsonl.gz`,
83 members / 4,144,576 bytes, SHA256
`9c20cedd8d72ec893cdd1777f501315e8760877ca39f3668286dcf777a7cf224`.
Root verified every saved member against its original exact bytes. Fresh/cache
event sequences are exactly equal after removing the 26/3 `cache_hit` rows;
no event fields were normalized. Pre-guard/final UTF8 and LTL event files also
match byte-for-byte. Cache/SQLite files remain externally bound, not embedded.

The bounded UTF8 flat-copy metadata projection is terminal0 under
`grouping-utf8-member-projection-root-2/`. It preserves all 12,313 provenance
edges, 3,023 declared identity keys, and **940 candidate JSON/JSONL hashes /
13,731 paths**. Its 41,267,200-byte regenerable SQLite hash is
`713ca5aa3979dbcd61a207244c66349983c765190d08657ac585dbb6525df873`.
No matched payload was opened; all 52 gaps remain. The required root terminal
receipt was issued only after session 93699 finished. This projection adds
candidate locations, not public provenance or complete historical receipts.

Own-producer schema review resolved the meaning of **776** four-field
`name/sha256/byte_count/data_base64` declarations: sizes/hashes cover logical
members. `binary/*` members can be slices of a larger file; do not treat a slice
hash as the full binary hash. The adapter is still to implement. All **38**
unsafe paths have scoped Windows-relative producer/sidecar dispositions; retain
raw paths and do not globally normalize or admit them. Two duplicate path events
are repeated native log declarations with equal bytes/hash, not target calls.

The filename-priority catalogue under `grouping-history-target-provenance-2/`
contains 423 candidate identities / 113,761 member/chunk provenance edges,
257,455 alternate flat edges, 4,723 ranked flat paths and 1,860 unmatched ranked
paths. Eleven own producers and three validation sidecars were classified.
Filename ranking never establishes absence or completeness; the remaining
baseline universe stays explicitly unknown. Large derived edge dumps and SQLite
files remain regenerable, with exact bindings, rather than duplicated in Git.
Preserved in `4c442074`: `docs/retained-history-metadata-provenance-checkpoint.jsonl.gz`,
38 members / 332,513 bytes, SHA256
`5b04b36c6b26effc5defd71de87df46160f7c33f9264fbfdd760093b9f94cb6a`.
Its sidecar binds the two omitted large exports and two baseline databases.
Root verified every embedded member against its current exact source bytes.

Both hardened ActivitySplit recipes reject optimized Python before imports or
output, reject incomplete saved receipts, and recompute exact receipt equality
before an actual intent. Independent extracted checks: **54 passed**. Fresh root
prepare-only outputs are `build/filtering-activity-split-{pandas,polars}-public-preentry-2/`.
Their entire 61/79 plan files are byte-identical to the originals. Capsule:
`docs/filtering-activity-split-hardened-preparation.json.gz`, 88 members / 175,145
bytes, SHA256 `9d4d12021408623c02767b8a1f45cbda571ef62347767d9a6be916c5f1e80d9f`.
Root checked every member against its original bytes. The six prepared modules
still total **565** logical queries, all unentered, `prior_barrier_validated=false`,
and no root entry certificate. The older assertion-based ActivitySplit recipes
below are retained historical artifacts; use the hardened replacements for any
future reviewed entry.

The independent final projection/capsule review found no issues and rechecked
all input/output hashes, nine SQL invariants, all 88 capsule members and 140
strict canonical receipt identities:
`grouping-utf8-projection-and-hardened-capsule-review-1/review.json`, SHA256
`530c3826051047fba3ccc8479852e194389e6f4d43cdce18f22743fa69e112d1`.

The full catalogue remains **46/86 packages and 226/387 modules complete; 201
strict open units**. No status changed. Continue the relevant flat/archive audit:
implement the explicitly scoped four-field adapter; review key/report and direct
record schemas, unresolved headers and flat provenance; then reconstruct and
hash-assert complete original receipts and deduplicate the whole candidate bank.
Metadata inventories, source filename classifications and these zero-call plans
cannot authorize algorithm entry. Preserve every prior failed attempt and
create new output directories. **Do not restart the completed 2,844-source scan.**
An isolated four-field adapter draft/review is now in progress under
`streaming-four-field-development-1/`; it has not changed the accepted parser,
run against a real archive, or granted any admission. Review and freeze it before
any future root metadata run.

## Previous checkpoint: streaming history inventory — 2026-10-22

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

**The coordinated metadata-only scan is terminal0/drained. Do not restart it.**
Root exec session **5107** finished all **2,844** selected own compressed contents:
`build/parity-closures/grouping-history-archive-discovery-final-2/manifest.json`
(981,981 bytes, SHA256
`05ef77cd8c8fd03bba4202ebf724cc8752b5311b47bf831895df08f1cf2fca20`).
Its finalized **645,283** metadata events include **123,763 members** and
**490,419 chunks**, with **30,814 explicit gaps**. No payload was decoded/admitted.
The original 370,502,511-byte event stream SHA256 is
`4fea0b765f5a83825d86b1c26bd38ce7c844199b0bbcfabda832ccd7cfa4b42e`.
It is preserved exactly in `docs/retained-history-archive-discovery-events.jsonl.gz`,
24,135,968 bytes, SHA256
`0e3e637eda6fc29d22232c556034f2cf53a86a4b990b330f91b6a4f274e9dbb9`.
Root issued `terminal-receipt.json` only after the actual handle finished.

The full-member/flat-copy metadata projection also finished terminal0/drained
(root session **32464**), under `grouping-member-projection-root-1/`.
It preserves 614,182 member/chunk provenance edges, 172,697 declared identity
keys and **34,997 candidate JSON/JSONL content hashes / 223,533 flat paths**.
Every alternative remains queryable; no matched payload was opened or admitted.
All eleven independent SQL invariants and seven synthetic integrity tests pass.
The 879,497,216-byte regenerable SQLite output SHA256 is
`54872a06b90ed21541ddb9773f02edb3355c1847b62410dfce18586cb087638c`.
Its helper in `grouping-history-audit-prepared-1/` has SHA256
`17f6eec37d3123b988844ae508d60e92313926bfdb83ea9c3622397b04bd96a5`;
it requires `--terminal-receipt` with
schema `minera-retained-public-inventory-terminal-binding-v1`, terminal/drained/
finalized flags, exact zero exit code/session ID, and report/events/manifest
size/SHA bindings.

A separate bounded eight-source correction finished terminal0/drained
(root session **15285**). Existing legacy member mapping recovered **17,084**
names, independently checked against the original source/ordinal/full-SHA edges;
all original sizes remain unknown and all payloads opaque. Eight version-2
manifest headers remain gaps. The first wide report was not overwritten.
Output: `grouping-history-legacy-discovery-correction-1/`.

Completed scan/projection/correction proof:
`docs/retained-history-archive-discovery-checks.json.gz`, 37 exact-byte members /
2,776,071 bytes, SHA256
`b223f318d3e3bd072ba82e616aad73e39480e09918c45c968e7b51c5e9313b76`.
The sidecar retains all incomplete/admission flags. **Historical audit is still
incomplete.** Next resolve the 31 UTF8 name/text envelopes (12,313 unrecognized
member names in the baseline), byte_count/base64 variants, key/report wrappers,
untrusted headers, unsafe/duplicate paths and direct public-record schemas.
Own producer reports and the exact metadata gap summary are retained. Establish
public provenance and freshly verify complete member bytes before any flat copy
replaces reassembly. Finish the relevant flat audit too: the projection does not
cover unmatched flat files. Only after complete original receipt reconstruction
and whole-bank dedup may root issue a new-family entry certificate.

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
capsule member and all 67 current source bindings.

The two Polars grouping banks are likewise root-materialized **prepare-only**:
92 consecutive and 113 timestamp full receipts under
`build/filtering-case-grouping-polars-{consecutive,timestamp}-public-preentry-1/`.
All **205** original hashes, strict schemas, ordered sets and source bindings
pass independent and root checks. Source/result/preentry collect counters and
target calls are zero; no certificate exists. Observer SHA256:
`2d495dc82a135cde7d2c5c5b1c8694aa0ec6714a084062be1777b7346a1d9d21`.
Thirty extracted guard checks plus four synthetic apply/collect/plan phase
checks pass. Failed own verifier/harness attempts remain preserved.
Capsule `docs/filtering-polars-case-grouping-preparation.json.gz`, 80 members /
119,294 bytes, SHA256
`36b41c9f5c1e5c33f3c858466d3d4b405dd9fb8c6bd77026931ee3f04f426511`.
Root verified every member and all 80 current source bindings. All six table
modules now have **565 prepared receipts** total (61/79 activity-split and
220/205 grouping), with every historical barrier still false. All workers,
reference algorithms and owned tools are idle; no live scan handles remain.

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

## Alpha+ cleanup selection checkpoint — 2026-10-24

Resolved in the cloud container (Python 3.11.17, pinned pip dependencies,
Linux Release build); no retained `build/` bank was used or replayed.
Boundary cleanup is now deterministic: the first tied transition in
first-seen label order. `enumerate_alpha_plus_cleanup_alternatives` and CLI
`--alpha-plus-alternatives` return its exact support. Two new disjoint banks,
`docs/alpha-plus-cleanup-bank-{a,b}.json.gz` (2,800 cells, 105,600 public
calls, 8 and 16 hash seeds, both trace orders), and the older merged fixture
all audit with zero failures through `scripts/check-alpha-plus-cleanup-bank.py`.
Every public outcome is an enumerated alternative and every alternative was
observed. Public cells vary exactly where the enumeration has several
outcomes; all others are bound exactly. The audit also found that logs with a
trace made only of loop labels (both boundary labels looping) must fail, and
native now rejects them. `algo.discovery.alpha.variants.plus` and package
`algo.discovery.alpha` are promoted: 47/86 packages, 227/387 modules, 199
strict open units. Contract and limits: "Alpha+ cleanup selection contract"
in the parity doc; hashes in `docs/alpha-plus-cleanup-validation.json`.

## Attribute repetition domain checkpoint — 2026-10-24

Cloud container, pinned runtime (polars 1.44.1 added to the reference venv);
no retained `build/` bank was used or replayed. Disjoint seeded banks, bank
seed 20261024, hash seeds 0-4, 6,000 cases per engine, all at zero failures:
Polars 23,701 calls, pandas 28,274 (326 declared rejections), log 18,433 (XES sources). Fixes: the
Polars companion was rewritten from a probed column x literal matrix (all
integer widths, Float32, Enum, ns/ms/zoned Datetime, literal classes, zone
abbreviations, bound simplification and contradictory-bound folding,
did-you-mean, NaN single-row join); pandas compares like NumPy weak scalars and
arrays, categoricals like their categories, and localizes naive strings to
named zones; log compares XES value/children wrappers as public mappings.
Tools: `scripts/{repetition_domain,capture-repetition-domain-bank,
check-repetition-domain-bank}.py` and `minera_repetition_batch`. pandas and
Polars rows are promoted (229/387 modules, 197 strict open units); the log row
stays partial. See `docs/attribute-repetition-domain-parity.md`.

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
  acceptance. A disjoint 53-log, five-profile discovery bank (265 public
  outcomes, stable across hash seeds 0-3) now matches all 318 native outcomes
  after the default elapsed path stopped losing microseconds; the bank is in
  `docs/ocdfg-wider-discovery-evidence.json.gz` and the scripts rebuild it in
  the cloud. Do not replay it. See `docs/ocdfg-wider-discovery-parity.md`.
  A second disjoint 28-log bank (140 outcomes) covers per-value offsets in
  one column, nanosecond columns and integer IDs; all 168 native outcomes
  match after elapsed time adopted the public whole-seconds-plus-fraction
  rounding. Discovery takes no holiday calendar. The public OCEL 2.0 JSON
  reader drops offsets (recorded, not copied; it is outside the `algo`
  catalogue). Still open: mixed int/str IDs, missing IDs, arbitrary relation
  dtypes and benchmarking. No slice establishes whole-module closure.
- Keep arbitrary relation dataframe cells, dtype/storage domains, non-string
  grouping IDs and actual performance benchmarking explicit. The new timing
  companion supports string identities; do not silently coerce other domains.
- The log repetition row is closed (see the XES list checkpoint). Other
  public algorithms that read XES list/container values through the default
  importer can reuse `EventAttribute::xes_public_shape` the same way.
- All other partial rows, connectors/platforms, solvers, rendered UI and
  performance requirements remain in scope until their own evidence closes.

## Build coordination and safe staging

The paths below describe the original Windows workstation and are kept as
history. Current work runs in Linux cloud containers: the reference venv uses
Python 3.11 with pm4py 2.7.22 and its pinned dependencies, and the build uses
CMake with Ninja.

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

## Conformance parity banks (2026-10-24, cloud container)

Petri alignments (all six search variants), DFG classic and forward/backwards
token replay are bound to three black-box banks by
`minera_conformance_parity_contracts_tests`; see "Conformance parity bank
evidence" in the parity doc for the contract, the reference semantics found
and the open items. Regenerate the fixture with
`python scripts/generate-conformance-parity-fixture.py <the three docs/*-bank.json.gz> --output src/backend/tests/conformance_parity_reference_cases.inc`.
Process-tree alignments are bound by
`minera_process_tree_alignment_parity_contracts_tests` (pinned per-variant
tallies over 4140 rows; table in the parity doc). Regenerate with
`python scripts/generate-process-tree-alignment-parity-fixture.py docs/reference-2.7.22-conformance-parity-bank.json.gz docs/reference-2.7.22-process-tree-alignment-bank.json.gz --output src/backend/tests/process_tree_alignment_parity_cases.inc`.
The four process-tree modules stay partial: each remaining row is a place where
the reference is not cost-optimal (search-graph pruning, DP loop cache, MILP
tau-body root loops, approximated activity-set splits).
Decomposed RECOMPOS_MAXIMAL is now bound in the conformance contract: bwc
exact on every row, cost and fitness pinned at 770 equal / 39 differing of 809
(the reference composition duplicates or drops moves and has unexplained
costs). Forward replay with `consider_activities_not_in_model_in_fitness` now
reproduces the reference's log-wide flag (variants by descending frequency,
then descending activity tuple; after the first variant with an unknown
activity everything is unfit) and all its rows are bound.
`minera_petri_analysis_tests` crashed on main too: `positive_invariant_diagnostics`
iterated the transitions of a temporary net; fixed by binding it first.
