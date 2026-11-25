# Full parity continuation — 2026-10-21–24

Resume from a fresh clone of the repository, branch `main`. The
`docs/**/*.jsonl.gz` evidence logs named below were removed from the history
on 2026-10-26 (nothing read them); the capture scripts in `scripts/` can
regenerate a bank when one is needed. The reference
venv and checkout can be rebuilt in a cloud container (see the README).

On 2026-11-10 the history of `main` was rewritten once more, to give every
commit the same author identity and the AGPL licence. Commit IDs quoted below
from before that date, including those in recheck test, fixture and script
names, refer to the earlier history.

## Objective and authorization

Reach full pinned-reference parity, prioritizing usable features, then complete
fidelity. Keep all 86 packages and 387 modules in scope. The user explicitly
authorized parallel subagents and clean logical commits/pushes directly to
`main`. Do not ask again for routine implementation, tests or Git checkpoints.

Use only public black-box calls, public signatures/docs/enums and opaque binary
hashes. Never inspect reference implementation source, AST, bytecode or private
state. Do not replay retained calls. Preserve failed attempts and execute new
uncovered banks only in new output directories.

## Broad audit of 962196a — 2026-10-27

The broad black-box audit (755 primary and 21 minimized comparisons) found
wrong results in discovery (DC1-DC6), organizational mining (O1-O5, C2),
OCEL associations, streaming (S1, S2) and pandas filters (FS1-FS8). All are
fixed with reference-valued regression tests; the table, the declared
exceptions (DC4 weighted marking equation, C3, streaming Partial, Windows not
executed) and the list of areas still without fresh comparisons are in
`docs/reference-2.7.22-parity.md`, section "Broad audit fixes and exception
list". Probe the reference through public calls only; resource-profile
semantics (complete-only intervals, closed-window case intersection, supplied
order) were found that way and are pinned in `test_resource_profiles.cpp`.

## Parity recheck of 4473f13 — 2026-10-27

- G1: TZif footer rules may switch at a signed hour of up to 167 (RFC 8536). America/Nuuk, America/Godthab and America/Scoresbysund use `M3.5.0/-1`. The vendored `ptz.h` now parses these. A footer that still fails to parse is an error rather than a silently frozen offset. Both backends match Python `zoneinfo` for every installed zone (597 zones, about 40,000 probes from 2030 to 2075). `test_zone_database` covers UTC lookups, gaps, overlaps and unique times around every transition from 2038 to 2060 for seven zones.
- G2: `nanosecond_instant.hpp` builds elapsed nanosecond counts without overflow at either end of the int64 range. Tests check exactly INT64_MIN and INT64_MAX, their neighbours, and one past each, plus a public reproduction of a row that is exactly INT64_MAX long. They pass under UBSan.
- G3: the reference rounds each multiply and add separately, so CMake builds with `-ffp-contract=off` on GCC and Clang. Clang contracts by default, and so does GCC on FMA targets such as every ARM64 Mac. MSVC's /fp:precise does not contract. With x86 FMA objects and contraction on, the trace-attribute, heuristic-variants, filtering-bank and LLE tests fail; with contraction off they pass. LLE's frozen contract is NumPy 2.4.6 with scipy-openblas 0.3.31 and unfused rounding. Newer NumPy builds that fuse may differ in the last bits, and the bank does not try to match them.

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
include/minera/random.hpp, the shared generator module). Package
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

## Recheck of 61ebeee0 (2026-10-28, cloud container)

J1-J34 and K1-K4 from the reviewer's recheck are fixed and K5 plus two older
representations are declared; see "Recheck of 61ebeee0" in the parity doc.
The reviewer's saved reference outputs are bound by
`minera_recheck_conformance_fixture_tests` (fixtures in
`src/backend/tests/fixtures/recheck_61ebeee0/`). Regenerate its table with
`python scripts/generate-recheck-61ebeee0-fixture.py <evidence>/discovery-conformance <evidence>/ocel-organizational src/backend/tests/recheck_61ebeee0_cases.inc`.
Delta-minimal cases are in `minera_recheck_regression_tests`. The new
`resource_is_null` (batches) and `zero_variance_sentinel` (temporal) members
of the stored workbench JSON are written only when true, so the recorded UI
fixture bundles stay byte-exact; absent reads as false.

## Recheck of 65572296 (2026-10-28, cloud container)

L1-L9 from the reviewer's recheck are fixed; absent-column set order and
TypeError operand order are declared nondeterministic in the reference and
K5 is expanded to classic OCEL JSON. See "Recheck of 65572296" in the parity
doc. The reviewer's cases are bound by `minera_recheck_65572296_tests`
(fixtures in `src/backend/tests/fixtures/recheck_65572296/`); regenerate the
typed batch/network table with
`python scripts/generate-recheck-65572296-fixture.py <evidence>/ocel-organizational/delta-comparison.json src/backend/tests/recheck_65572296_cases.inc`.
The Python scalar model shared by batches, network analysis and the sequence
filters lives in `src/backend/python_scalar.hpp`; when a pandas grouping or
ordering rule is in doubt, fuzz a Python model of the public call against the
reference first (the approach that pinned these rules) and then port it.

## Recheck of c8ee22d (2026-10-28, cloud container)

M1-M10 and the three smaller differences from the reviewer's recheck are
fixed; set-order batch messages, NaN object identity, the None event-id label
and the representative of equal mixed-type case ids are declared. See
"Recheck of c8ee22d" in the parity doc. The reviewer's cases, fuzz cases and
numpy comparison logs are bound by `minera_recheck_c8ee22d_tests` (fixtures
in `src/backend/tests/fixtures/recheck_c8ee22d/`). Regenerate them with the
pinned reference environment:
`python scripts/capture-recheck-c8ee22d-reference.py fuzz-filters|fuzz-filter-keys|fuzz-batches|fuzz-batch-keys ...`
then `filters`/`batches` for the public results, `sort-logs` for the numpy
logs, and `python scripts/generate-recheck-c8ee22d-fixture.py <evidence> src/backend/tests/fixtures/recheck_c8ee22d --filters CASES RESULTS ... --batches CASES RESULTS ...`.
Lessons: fuzz with more than 16 distinct keys (numpy changes sort strategy
there), parse numbers with `src/backend/full_double.hpp` rather than
`std::stod` (glibc reports subnormals as ERANGE), and check a message
difference against several `PYTHONHASHSEED` values before treating it as a
bug.

## Recheck of d5b33ef (2026-10-28, cloud container)

N1-N6 from the reviewer's recheck are fixed: no 128-bit integer type in core
code (guarded by `minera_portable_integers`), network ties ordered by numpy's
object quicksort, exact and numpy-typed run-key addition with float64 run
comparison, NaN batch-tuple identity with pandas' shared "str" NaN and the
reference's merge order (equal mixed-type representatives are no longer
declared), Python float()/int() grammar for PNML stochastic properties, and
empty native labels in the organizational workbench. Declared: the weight and
priority range policy, seed-dependent set-order outcomes (plus two fuzz
tables where CPython's set layout fixes a different failing pair), NaN
object identity in object columns, and None labels. See "Recheck of d5b33ef" in the parity
doc. The reviewer's cases and the fuzz cases are bound by
`minera_recheck_d5b33ef_tests` (fixtures in
`src/backend/tests/fixtures/recheck_d5b33ef/`). Regenerate them with the
pinned reference environment:
`python scripts/capture-recheck-d5b33ef-reference.py fuzz-run-keys|fuzz-org|fuzz-representatives ...`,
then `filters` and `org` (40 hash seeds) for the public results and `pnml`
for the property cases, and
`python scripts/generate-recheck-d5b33ef-fixture.py <evidence> src/backend/tests/fixtures/recheck_d5b33ef --filters CASES RESULTS --org CASES RESULTS ... --pnml RESULTS`
(pass the reviewer's org cases captured with `org` too, so their
seed-dependent alternatives are listed).
Lessons: compare a numeric run key as float64 (pandas shifts it to float64),
model batch merging as remove-pair, append, sort and restart, treat a "str"
column's missing cells as one `np.nan` object, and capture any result that
may depend on set order over many hash seeds (40 here) before calling a
difference a bug: two seeds missed several.

## Recheck of 7270a1f (2026-10-28, cloud container)

O1-O4 from the reviewer's recheck are fixed. The reviewer's handoff was on a
machine that was offline during this round, so each target was found from its
title by fuzzing public PM4Py 2.7.22 calls. The O1 fix covers numeric dtype run
keys in consecutive activity/case grouping, following numpy and pandas
promotion (wrapping narrow integers, float16/float32 rounding, masked
Int/UInt/Float/boolean columns with pd.NA and their stored values under the
mask). The O2 fix covers a NaN, None, pd.NA or NaT next to an int too large
for a float, which raises OverflowError unless a TypeError comes first. The O3
fix reads PNML transition toolspecifics as PM4Py does (any tag ending in
"toolspecific", tool substrings "ProM" and "StochasticPetriNet", one record
per stochastic toolspecific, other tools ignored with their properties). The
O4 fix converts and validates distribution parameters per type with PM4Py's
messages. The same dtype rules were then carried to the other pandas sequence
and case filters: float16 grouping and first/last aggregation errors, numpy
unique order and str labels in the activity split, int64 case ids that wrap as
a RangeIndex, float16/float32 width comparisons, masked NA matching, variant
and pattern join errors, and NaN boundary counting.

Declared: native maps hold only finite numbers, finite non-negative weights
and int priorities (`POLICY` in the fixture). See "Recheck of 7270a1f" in the
parity doc. `minera_recheck_7270a1f_tests` binds 13,824 filter and 4,000 PNML
reference cases (fixtures in `src/backend/tests/fixtures/recheck_7270a1f/`).
The reviewer's handoff was read on 2026-10-31. Their 100 grouping tables and
75 eligible PNML captures were bound with their saved public results, and all
of them match (four are declared). The eight captures they excluded were
captured again, matched, and were bound too.
Regenerate them with the pinned reference environment:
`python scripts/capture-recheck-7270a1f-reference.py fuzz-pnml pnmlS.json S 2000`
and `pnml` (seeds 1, 2), `fuzz-run-keys rkS.json S 600` and
`fuzz-dtypes dtS.json S 1000` with `filters` (seeds 1, 2),
`fuzz-typed ty1.json 21 3000 case`, `ty2.json 22 3000 activity`,
`ty3.json 23 4000 mixed` and `typed-probes typ.json` with `typed`, then
`excluded-pnml EVIDENCE excluded.json` with `pnml` for the reviewer's
excluded models, then
`python scripts/generate-recheck-7270a1f-fixture.py src/backend/tests/fixtures/recheck_7270a1f --filters CASES RESULTS ... --pnml CASES RESULTS ... --reviewer EVIDENCE`
(EVIDENCE is the reviewer's `minera-recheck-7270a1f/` folder).
Lessons: a numeric run key follows numpy's result type, not Python's; pandas
retries an object addition only after a TypeError, so missing cells still meet
large ints first; a masked column takes part in object arithmetic with the
value stored under its mask; set_index on an evenly stepping int64 column
builds a range whose step test wraps but whose length does not; numpy names
uint64 scalars `uint64` or `ulonglong` depending on what the process converted
earlier, so capture those results in a fresh process.

## Review-fix loop, rounds T–HL (2026-11-05 to 2026-11-25, cloud container)

Each round runs two read-only reviewers in parallel (one re-reviews the
previous round's fixes, one sweeps an area against the pinned reference with
random public inputs), then the same reviewers fix what they found in the
working tree; the main session runs the full suite and pushes one commit per
round to `main`. Each round's findings, fixes and declared differences are in
`docs/reference-2.7.22-parity.md` under "Review <round><n>". Rounds AA–AH
covered token replay and footprint speed, soundness reductions, `/api/mine`
on large logs, the DataFrame path for XES inputs (one shared import, case
merging, case-id order of per-case lists, typed case ids), cancellable log
loading and `cancel-mine`, and strict BPMN/PNML/PTML reading and writing.
Round AI took the BPMN parallel arcs of AH through evaluation and every
alignment variant, and fixed the OCEL 2.0 type declarations that pm4py
reads back. Round AJ swept concept drift (thresholds outside [0, 1]),
feature extraction on CSV logs (pm4py's copied case, activity and timestamp
columns) and `/api/align` on a 50,000-case log (variant searches on four
threads, four malloc arenas in the server: 918 s to 215 s). Round AK
re-reviewed AJ and made `/api/align` with a supplied model about three
times faster without changing results (each A* bound solved once, one
empty-trace search per log, the dispatcher pass on four threads). Round AL
re-reviewed AK and sped up the remaining large-log alignment routes without
changing results: A* bounds solved when a state is popped, the bundle's
search reused by the dispatcher pass with standard costs, and one malloc
arena per server thread (default replay fitness on 3,000 cases 225 s to
119 s, imported-model alignment on 1,000 cases 180 s to 110 s). Round AM
re-reviewed AL without findings and swept OCEL discovery at the route level
against pm4py on random OCEL 1.0 and 2.0 JSON files (etot, otg, OC-DFG,
OCPN, SAW nets, edge associations): files whose relations name an unlisted
object are now read as pm4py reads them (the row is dropped, not the log
refused) by the JSON, OCEL 2.0 XML and SQLite readers; OC-DFG edge keys no
longer collide when activity names hold "->"; edge-association durations
print in full. Round AN re-reviewed AM with degenerate files per reader:
events and objects with an empty id, activity or type are now dropped as
pm4py drops them (OCEL 1.0/2.0 JSON, both XML readers, SQLite), repeated
type and attribute declarations and repeated OCEL 2.0 JSON event ids read
as their last listing, OC-DFG JSON reads its endpoint fields even when
empty, and the merged SAW net of an empty OCEL has no place. Round AO
re-reviewed AN with random files per format (events, objects and times in
order): the JSON readers matched; the SQLite reader now builds its events
and objects from the type tables as pm4py does (equal times by the row's
place in its table, then type name; objects by type name, then table order;
nothing without a row of its own, the base-row rule across all object
tables, NaT last, NULL types and missing tables raise), the OCEL 1.0 XML
reader relates an event to an object once, and both XML readers raise on a
file without relations. OCPN token-replay diagnostics matched pm4py on 245
larger logs (up to 2,472 events, IM and IMd). Round AP re-reviewed AO with a
mutating SQLite fuzzer: ids listed twice in the event or object table now
take their last listing's type (all listed types' tables are read), change
rows take their object's type, change rows without a value column or with an
empty field are kept, and a file left without events or objects keeps
neither. It also fixed root cause on the DataFrame path: CSV and XES logs
record their frame columns (`EventLog::dataframe_columns`), and the
root-cause diagnostics read every event with every column, a missing cell as
NaN, ints of a float64 column as floats (920 XES and 1,448 CSV trees equal
to pm4py). Round AQ re-reviewed AP: SQLite files without base rows take each
id's first row over the object tables (type-name order) as the object, an
XES column of ints past INT64_MAX beside negative ints is objects, and the
CSV frame now has the case column under its own header, UTC timestamp
columns, `@@index` and `@@case_index`, and the resource only under its
header. Trace- and event-based features (`log_to_features`) read the frame
view too, with trace attributes from the frame (`trace_columns`: a missing
one NaN, ints with a gap floats), NaT date cells out of the default
selection, and `lifecycle:transition` never taken by default (1,330 feature
comparisons and 1,053 root-cause trees on CSV and XES uploads equal to
pm4py, apart from the declared mixed-type keys below). Round AR re-reviewed
AQ and fixed the CSV frame's case and activity copies: a case column with
another `case:` header (say `case:id`) is a trace attribute (`id`, typed
like the column), an unprefixed case column is no trace attribute, a
`concept:name` or `case:concept:name` column beside custom case and
activity columns is replaced by pm4py's copy; the calendar TEMPORAL
features (`grouper_freq`) read the frame too, so with a CSV resource column
under another header the default `org:resource` raises as in pm4py
(features, temporal features on CSV and XES, profile clustering and root
cause equal to pm4py apart from declared ties and mixed-type keys). Round AS
re-reviewed AR without findings and settled the organizational routes'
contract on CSV and XES uploads: SNA (`/api/sna`, all eight variants) and
role discovery (`/api/roles`) now read keys as pm4py reads the frame (a
key is a column name with no `org:resource`/`resource` alias, a missing key
raises `KeyError`, cells are typed, sorting text beside NaN or numbers
raises `TypeError`, NaN cells are left out where pm4py keys each one apart),
and the frontend's social-network and roles resource fields start at the
session's resource key (`active_source.source_selectors.resource_key`).
Native table sources keep the alias rules. The decision is recorded in
`docs/reference-2.7.22-parity.md` (Review AS1) for Philipp to overrule.
Round AT re-reviewed AS (fixed: the role tie-break compares activity lists
as Python lists, ints past 2**53 keep their own names, a CSV activity
column under its own header is typed) and brought the other organizational
routes onto the same contract on CSV and XES uploads: local diagnostics
(pm4py's pandas `apply_from_group_attribute`), network analysis (frame
columns, typed nodes, pandas merge dtype and timestamp rules) and resource
profiles (per-metric column selections and `KeyError` order, text
arguments against typed cells, `==` versus `isin` matching, timestamp and
case-column errors, `average_case_duration` grouping the resource's cases
by the case column). CSV text columns whose cells all parse as dates are
now date columns of the frame, as `format_dataframe` makes them. The
frontend's resource fields of all four organizational tools start at the
session's resource key. Reference sweeps of network analysis must run with
`NPY_DISABLE_CPU_FEATURES="X86_V3 X86_V4 AVX512_ICL AVX512_SPR"` (numpy's
SIMD argsort breaks numeric ties in another order). Details and the
declarations: `docs/reference-2.7.22-parity.md`, Review AT1.
Round AU re-reviewed AT (fixed: a column selected twice in resource
profiles fails before the timestamp comparison; local diagnostics sort the
(resource, activity) keys before reading the group; network analysis
subtracts text and object timestamp columns cell by cell where both cells
are present) and took the AT1-4 date columns through the CSV routes that
read typed frame columns: `/api/attribute-statistics` on CSV and XES
uploads now counts the frame's typed cells (UTC dates, float64 ints with a
gap, `KeyError` for a missing key). The resource-profile frame branch now
models `average_duration_activity`'s subtraction by the start column's
dtype and the object start of single-event cases, `interaction_two_resources`
(cases filtered on `case:concept:name`, groups of the case column, each
group's last timestamp), the derived-start metrics' sort by case and
timestamp (an ordered Categorical: unorderable values raise), float64
timestamp columns comparing NaN, and CSV `@@index` network nodes take the
analysis's own sorted numbering. Details: Review AU1.
Round AV re-reviewed AU with the AT1-4 date shapes on the timestamp and
start columns (fixed: `multitasking` sums each distinct interval once,
`average_case_duration` on uploads keeps a case only when it starts before
t2 and ends after t1, both strictly, and compares a float64 column's NaN
cells; `distinct_activities` and `activity_frequency` compare the
resource's rows before a duplicated timestamp column realigns; an empty
role keeps its keys in local diagnostics) and swept the CSV routes AU had
not: features and root cause had no differences, TEMPORAL features failed
on a date key with blanks (`stoi`, fixed), and the LLM context described
the native table on uploads. `/api/llm-context` now reads the frame there
(event stream cells and column order, dtype-driven attribute
descriptions, `format_dataframe`'s keys for CSV features), variants fail on
a one-case stdev only when printed, and the truncation check survives that
failure. Details: Review AV1.

Round AW re-reviewed AV (fixed: network durations on uploads are the
dates' exact `Timedelta.total_seconds`; `average_workload`, `multitasking`
and `average_duration_activity` follow pandas' duplicate-column rules,
checked on a 13,500-request key matrix; a reversed interval raises `Start
must be less than end`), found no differences in the attribute filters and
OCEL-from-CSV with the AT1-4 date columns, and took the four time
statistics routes to the upload frame: service time (module-level pandas
apply, NaN durations skipped, Kahan sums), cycle time (per-row
`.timestamp()`, negative values), case arrival (first cells sorted with
Python's comparisons, text failing once two cases have a cell) and case
statistics (NaT cases null, exact business-hour microseconds, object
columns case by case). The round was paused for the 2026-11-10 history
rewrite and finished on the rewritten main. Details: Review AW1.

Round AX re-reviewed AW with typed extra columns, custom case keys and
business hours on CSV uploads and their XES round trip, comparing error
messages too (fixed: a True/False CSV column with a blank is an object
column; case statistics group by the case key's column, sort by start time
by default and raise pandas' grouper, replace and empty-group errors; the
column subtraction of service time, case statistics and passed time is
pandas' (`frame_times::column_subtraction`: text columns subtract only
rows with both cells, object columns retry on them, date less object cell
by cell); XES cells written as `<float value="nan">` are missing values to
the dtype inference; service time's business hours count exact
microseconds) and took passed time to the upload frame (sorted pairs, the
raised start, NaN means as null, business hours through pandas' datetime
conversion). Details: Review AX1. The first full CI run on the public
repository also gave findings (Review AX2): the Windows build's compile
errors (a `small` macro of the Windows SDK, a local constant in a local
class, a wide path, a parenthesized aggregate), macOS-only address-space
checks, a use after free in a DFG test, last-bit libm differences of the
classic DFG playout (declared) and a cancellation test that timed `-O0`
copies.

Round AY fixed the ten Windows test failures of AX's CI run (Review AY2:
the process time zone behind `local_time.{hpp,cpp}` with an IANA `TZ`
read through the zone database on Windows, 8 MiB stacks and 256 MiB for
tests, `cmd /c` quoting, `ntpath` separators, MSVC's 32-bit
`std::chrono::minutes`; ICU zone abbreviations declared), and re-reviewed
network analysis on uploads (AX's open XES seed 59: the sort fails before
the merge checks; a NaT in a joined row is a NaN kept in the list, JSON
`null`). The new area was business hours: network analysis runs pm4py's
row-wise `BusinessHours` apply with its errors, every time statistic now
shares one `BusinessHours.get_seconds` model (the old segment sum differed
on 752 of 30,000 pairs), and case arrival on frames ignores business hours
as pm4py's pandas variant does. Details: Review AY1. mingw-w64 with Wine
reproduced the Windows failures that are not MSVC-specific. The second
Windows run left two (Review AY3): the evaluation regression test removed
an open file, and the future offsets of Greenland's and Chile's zones
follow Windows' ICU tz release (declared; the test prints them). With
those, the CI run of the AY3 commit passed on every platform. AY also ran
the suite under AddressSanitizer and UBSan (a separate tree, `-O1`, core
as a shared library so the test binaries fit the disk): no memory error
or undefined behaviour in 449 tests. The server contract test's
address-space limits cannot run under ASan (its shadow memory needs the
address space), the POWL and fast-path timing bounds do not hold at ASan's
speed, the cancellation test's one-second bound missed once, and seven
large test files (conformance contracts, the pandas LTL and Polars
workbench banks) run the container out of memory when compiled with
ASan; build those one at a time on a larger machine if needed.

Round AZ re-reviewed AY and found nothing further in its code: network
analysis with business hours, NaN lists and case columns that vary within
a case on 350 CSV and 350 XES upload seeds plus 700 two-row CSV seeds
(`azsweep.py`, which also runs resource profiles and local diagnostics),
OCDFG edge performance with business hours against
`pm4py.read_ocel2_json` on 400 random files, SNA and roles on 350 seeds
with varying case columns and 300 two-row seeds (`orgsweep.py`), root
cause on 150 seeds with varying case columns (`rccsv4.py`, only the
declared hash-seed selection differs). It fixed the two upload errors AY
left open and what the sweeps found (Review AZ1): on a two-row frame
pandas reads a list of two key names as the rows' labels when one is no
column, so local diagnostics and the pandas roles on two-row uploads
without `org:resource` work on the names' first two characters
(`pandas_row_label_keys` and `label_characters` in `frame_cells.hpp`); the
duration check of resource profiles runs per resource; `case:` columns
are read per row in the DataFrame scope (`frame_value`,
`frame_row_scalar`), which changed local diagnostics, resource profiles,
network analysis and the pandas SNA and roles on uploads whose case
columns vary; CPython's `year N is out of range` in the naive timestamps
of the three filters; the OCEL 2.0 JSON and XML and OCEL 1.0 XML readers
keep microseconds (OCDFG durations were a microsecond off; the OCEL 1.0
JSON transport keeps nanoseconds, declared); the
trace-attribute helpers are 64-bit on Windows. The device check for new
reviewer handoffs on the Mac was skipped (no device access from the
round's thread).

Round BA re-reviewed AZ without a finding in its code (the AZ sweeps on
fresh seeds, the two-row and varying-case-column sweeps of the
organizational routes, root cause, features and the LLM context) and took
AX's open targets. It fixed (Review BA1): the OCEL 1.0 JSON transport now
marks itself (`"minera:nanoseconds": true`), so any other classic JSON
file is read at microseconds as `read_ocel_json` reads it (the AZ
declaration is gone); resource profiles with one column selected several
times (`activity_frequency` and `distinct_activities` mask instead of
realigning; the case metrics raise the `Grouper` error on the case column
on the timestamp column; `average_case_duration` reads its start key after
the comparison and the groupby); passed time on an XES object timestamp
column of one pair now subtracts like pandas (`column_subtraction` with the
shifted start dtype); a missing column named twice is named once in
`not in index`; a `case:` bool column with a blank is object dtype; and
the export CLI reads XES files as the frame, as the server does. Open: the
trace-based selection of a bool column with gaps in the LLM context's
`log_features` (seed 17, below). The device check on the Mac was skipped
again (no device access from the round's thread).

Round BC re-reviewed BB and took its open targets (Review BC1). It fixed:
SNA on a datetime resource column with gaps (one resource `NaT`, as
pm4py's frame keeps `pd.NaT`) and SNA's error texts (the case-grouped
pandas variants' `Column not found` and ordered-Categorical errors,
`datetime.datetime` in an object column, scipy's pearsonr message);
resource profiles on XES object columns (the case metrics read each
case's last timestamp, `average_case_duration` takes each case's minimum
and windows an unsorted case by its least and greatest timestamp,
`average_duration_activity` subtracts two object columns cell by cell);
`datetime.datetime` in network analysis' sort and groupby errors; and the
trace-based default selection of a column of numbers, bools and gaps,
which pm4py types by the first element of the Python set of its cells
(`python::number_set_order` emulates CPython's set; BA's open seed 17 of
`llmsweep.py` is closed). The device check on the Mac was skipped again
(the round's thread had no device linked).

Round BD re-reviewed BC and took its open targets (Review BD1). It fixed:
the trace-based default selection of an XES trace attribute (a `case:`
column) of numbers, bools and gaps, which follows the same Python set
order as event columns (profile clustering gets the same features); the
workload metrics without a start column on an XES object timestamp column
(the per-case shift is float64 when every shifted cell is a number, and
the intervals are converted in the order of a second sort by start and
timestamp: BC's open `objtime.py` seed 279); the case-grouped SNA pandas
variants leave a row without a case out of the resource sort; and the
event distribution on uploads now follows pm4py's DataFrame statistics
(NaT makes the parts floats, `'3.0'` labels, the frame's datetimes are
UTC, pandas' errors). The device check on the Mac was skipped again (the
round's thread had no device linked).

Round BE re-reviewed BD and took its open targets (Review BE1). It fixed:
the attribute KDE on uploads, which now follows pm4py's DataFrame
statistics (the column's present cells sorted by Python's `sorted()`, one
distinct value read through `float()`, several numbers as floats so a
bool column has a KDE, pandas' and numpy's own errors, and the date
estimator's `replace(tzinfo=None)` on the first sorted cell); `@@index` and
`@@case_index` exist on CSV frames only (an XES upload gives `KeyError`);
a date KDE whose grid leaves datetime64[ns] raises pandas' `OverflowError`;
and pandas' mixed date parsing of all-numeric text in CSV columns (`2020`,
`2020-01`, `20200101`, `03.24-2020`, `007`, `250699`, `202001011230`), so
such a timestamp column is read as pandas reads it and a column of dates
and `2020` becomes a date column (BD's `snamixcsv.py` seed 152). The
device check on the Mac was skipped again (the round's thread had no
device linked).

Round BF re-reviewed BE and took its open target (Review BF1). It fixed:
`python::sort` for 64 items and more, which now runs CPython 3.11's
timsort with powersort merges and galloping (comparisons, order and the
operands of a failing comparison as in CPython; BE's declaration closed);
an XES upload's frame rows in `read_xes`'s file order when a trace name
repeats (`frame_rows` in the attribute KDE, network analysis, resource
profiles and the SNA pandas variants' sequence sort, whose cases meet
numpy's sort in the order of their first rows); pandas' reading of a
six-digit date before a clock (`200113 10:30`) and of a clock before the
date (`10:30 2020-01-05`, `7:05 PM, 05/06/2020`, zones and offsets kept
last); and the joint-activities pandas variant, which sorts each column
after dropping its own NaN. The device check on the Mac was skipped again
(the round's thread had no device linked).

Round BG re-reviewed BF (Review BG1) and swept the remaining frame routes
with interleaved XES uploads. It fixed: attribute statistics, case
statistics grouped by another column, passed, cycle and service time,
local diagnostics, the roles and SNA pandas variants, and the LLM event
stream and column descriptions, which now read an XES upload's rows in
`read_xes`'s file order (`value_counts` ties and the first of equal values,
a group's first and last rows, the frame's column order); pandas' reading
of `.` fractions longer than six digits off the ISO path (nanoseconds only
when digits four to six are `000` after a two-digit `H:MM:SS` clock), of
comma fractions (after a basic `YYYYMMDD` date; none after one-digit
seconds), and of a clock before a month-name date or with a zone between
the clock and the date. The device check on the Mac was skipped again (the
round's thread had no device linked).

Round BH re-reviewed BG (Review BH1) and swept the frame loops BG left
with interleaved, plain XES and CSV uploads. It fixed the LLM descriptions
of pm4py's DataFrame discoveries on uploads: the log skeleton's activity
occurrences (the first case by sorted id, then `value_counts` order; each
count set sorted as text), the temporal profile (each case's rows stably
sorted by timestamp, durations from pandas' `Timedelta` parts, a
compensated mean and Welford's deviation) and the declare model (each case
read as the frame's rows from its first row on, as many as it has); and
the top-k and minimum/maximum coverage variant filters, which count and
rank variants over those same slices and keep the cases whose own variant
was chosen. The device check on the Mac was skipped again (the round's
thread had no device linked).

Round BI re-reviewed BH (Review BI1) and took its targets with
interleaved, plain XES and CSV uploads. It fixed: Declare discovery on
uploads in `/api/mine` and discover-constraints, which now reads each case
as the frame's rows from its first row on, as many as it has (BH1-3's
slices); the EventLog temporal profile's mean and deviation, now exact and
rounded once as `statistics.mean` and `statistics.stdev`; `/api/mine`'s
temporal profile on uploads and the behavior route's DataFrame variant,
which use pandas' arithmetic (BH1-2); and three row-changing filters on XES
frames: the activity split (cases labelled in sorted id order from their
slices, labels handed to the rows in row order), `filter_between` (slices
marked in sorted id order, later marks replacing earlier ones) and the
consecutive activity grouping (only frame-adjacent rows merge). The device
check on the Mac was skipped again (the round's thread had no device
linked).

Round BJ re-reviewed BI (Review BJ1) and took its targets. It fixed: the
Declare conformance list on uploads in `/api/mine` and discover-constraints,
which reads each case as its slice of the frame's rows while the
diagnostics table keeps the merged cases; the DataFrame temporal
conformance, which lists the cases in the order of their first rows and
takes its durations as pandas `Timedelta`s; and the segment filter's
between on interleaved XES uploads, which marks the slices as BI1-5 does.
One older contract test (`minera_temporal_profile_parity_contracts_tests`)
had pinned the DataFrame conformance's sorted case order and now follows
pm4py's. The device check on the Mac was skipped again (the round's thread
had no device linked).

Round BK re-reviewed BJ (Review BK1) and took its targets. It fixed: XES
booleans spelled other than `true`/`false` and floats with a leading plus,
now kept as the value pm4py reads (the table transport had refused them);
Declare discovery and the conformance list on the frame's case slices in
the Declare workbench and in `/api/mine` with `mining_input_shape`; the
DataFrame temporal profile and conformance with a separate start column
(rows sorted by start, then completion; a pair when the target starts at
or after the source completes); the temporal profile of an event stream,
which pm4py reads through its EventLog; `/api/mine`'s temporal profile with
other keys on uploads (pm4py's DataFrame discovery on the named columns,
the resource and lifecycle found as keys); and the variant statistics,
which count the frame's slices. The device check on the Mac was skipped
again (the round's thread had no device linked).

Round BL re-reviewed BK (Review BL1) and took its targets. It fixed pm4py's
DataFrame temporal profile and conformance on uploads with missing cells
(a row without an activity leaves the groups, a NaT start sorts last and
pairs only as a source, a NaT completion pairs only as a target, every case
is listed), on CSV uploads with a start column (pairs follow
`format_dataframe`'s `@@index`: each case by the upload timestamp, then file
order; `TemporalProfileOptions::frame_index_rows`), with traces without a
name (their rows form one more case, as pandas' self-merge on the case
column matches NaN with NaN) and without any start column (the completion
column serves); and `/api/mine`'s keyed temporal profile with missing cells
and with number, boolean or date activity columns (named by `str()` of
pm4py's key). pm4py's log skeleton, Declare and inductive tree raise on an
`EventStream` (declared). The device check on the Mac was skipped again
(the round's thread had no device linked).

Round BM re-reviewed BL (Review BM1) and took its targets. It fixed pm4py's
DataFrame DFG adapters on uploads (`/api/dfg-dataframe` reads a CSV upload
as `format_dataframe`'s frame with its `@@index`; a blank start is NaT; a
NaN flow is 0; the pandas errors on frames without any start or without
pairs; the case-only sort as numpy's introsort; nanosecond comparisons and
`Timestamp.timestamp()` overlaps; rows without a case meet in the
self-merge), the temporal profile on a CSV's own `@@index` column (the
frame overwrites it), an `/api/mine` crash on interleaved XES uploads with
unnamed traces in the DataFrame shapes (the Declare frame slices dropped
the caseless rows), and `/api/mine` on CSV uploads in the DataFrame shapes,
whose families read the upload's file order instead of the sorted frame
(Declare, log skeleton, alpha, transition system). pm4py's high-level
DataFrame functions refuse frames with an empty case id (declared). The
device check on the Mac was skipped again (the round's thread had no device
linked).

Round BN re-reviewed BM (Review BN1) and took its targets. It fixed the
session routes and workbenches that handed a CSV upload's table to pm4py's
DataFrame functions in file order (minimum self-distances, batches, the
performance spectrum, the constraint, graph, workflow and conversion
routes, the original Declare and log skeleton routes, token replay,
alignments and replay durations, on the DataFrame, LazyFrame and stream
shapes: they now read `format_dataframe`'s frame, built once per session);
the frame's date columns (declared `datetime64[us, UTC]` or `[ns, UTC]` as
`format_dataframe` types them before it drops rows, which the pandas
batches' unit reads); the batches' default event id (the frame's
`@@index`), an event that starts after it completes and a blank start
(pandas: the smallest datetime), and a blank resource (NaN); the
heuristics miner and model decoration keyed by a CSV's own activity column
on the table shapes (the heuristic family failed on every CSV upload mined
as a DataFrame); and the DFG adapters on a frame emptied by the dropped
rows (the dtypes are kept) and without column reduction (a NaN cell drops
its row). The overlap family's one-error-for-three-statistics, the trimmed
whitespace-only cell and the batches' log variant with a blank resource
are declared. A stray `pyarrow` install in the reference venv made pandas 3
store `string` columns in Arrow and sort them stably, which looked like a
regression of BM's introsort model: keep `pyarrow` out of `/tmp/refvenv`
(the harness converts frames to Polars without it, `plconv.py`). The
device check on the Mac was skipped again (the round's thread had no
device linked).

Round BO re-reviewed BN (Review BO1) and took its targets. It fixed the
standard keys on CSV uploads with their own headers (`concept:name` and the
other copies `format_dataframe` adds now name the selected columns on the
Declare, log skeleton and footprints workbenches, the constraint route and
workflow discovery: `csv_frame_key`); a CSV's own `concept:name`,
`time:timestamp` or `case:concept:name` column beside other selected
columns (the frame replaces it by the copy; the import kept it and it
shadowed the selection on every route); the date unit of rows the frame
drops (BN read it from the upload's table, which lacks them; the import
records it now, `DataFrameColumn::nanoseconds`); the batches on the frame's
typed cells (numeric resources `1.0`, NaN resources as pandas' `np.nan`
with the log variant's dict groups and its TypeError, number and bool
activity keys, the Polars activity type check; `type_csv_frame_cells`,
`append_frame_log_events`); the analysis output's batch check, which still
refused BN's inverted events (`/api/mine` and the CLI failed the whole
response); and the CLI's pandas and Polars batches on a CSV input, which
now read the frame as the server does (`csv_frame_table.hpp`, shared).
Declared: `/api/log-convert` on a CSV keeps Minera's representation (rows,
order and roles are pm4py's), the pandas batches run without
`pm4py.discover_batches`' column check, Minera's role names for batch keys
on CSV uploads, the selected activity and case columns as text. Two
active-source fixtures of a CSV with both its own and the standard headers
were recaptured (the copies now replace the CSV's columns, as pm4py's
frame). The device check on the Mac was skipped again (no device linked).

Round BP re-reviewed BO (Review BP1; BO's batches, standard keys, Declare
and log skeleton workbenches and CLI batches held on fresh seeds) and took
its targets. It fixed pm4py's DataFrame performance spectrum instants (the
datetime column's int64 units over 1e9, exactly divided: Minera multiplied
by 0.001, an ulp off in about one answer of ten, and read a nanosecond column,
which pandas gives as seconds, a thousandfold too small) and the LazyFrame
ones on nanosecond cells (epoch microseconds over 1e6, truncated); the
CLI's DataFrame spectrum and DFG adapters on CSV inputs, which now read
the CSV's frame as the server does (the partial order with a start column
followed the log's rows, not the frame's `@@index`); and seven log families
that refused a CSV's own column header as their key (rework, chaotic
activities, variant statistics, boundary activities, clustering, decision
replay and interval overlap: `event is missing activity attribute 'act'`;
they now read the selected column through its source key, in the CLI and
on the server); and BO's Windows build (MSVC refused the CLI's frame
helper, which took the input path as a string). One active-source fixture (a typed table declared
`datetime64[ns, UTC]`) was recaptured: its DataFrame spectrum is in
seconds, as pandas gives it. Declared: pm4py's log variants read a CSV
frame's nanosecond cells through `Timestamp.timestamp()`, which rounds to
microseconds; the DataFrame routes keyed by a number column read its cells
as text. The device check on the Mac was skipped again (no device linked).

Round BQ re-reviewed BP (Review BQ1; BP's DataFrame spectrum, CLI frame
modes and header-keyed log families held on fresh seeds) and took its
targets. It fixed the log families on CSV uploads with nanosecond cells,
where pm4py reads `Timestamp.timestamp()` (rounded to the microsecond) and
sorts by the exact `datetime64`: the spectrum's log variants (instants
rounded, ties tens of nanoseconds apart sorted by the exact instant, which
withdraws BP's declaration), the temporal profile and its conformance on
logs and streams, and the correlation miner's durations on every adapter;
the CLI's temporal profile, which ran the EventLog discovery where the
server reads an upload as pm4py's DataFrame; and two lookups that refused a
CSV's own header as their key, the process-tree reduction and the Petri
usage comparison on the Inductive model (the cached Inductive log projects
the miner's key over the header: `inductive_log_for_key` prepares it under
the explicit key). Declared: pm4py's cvxopt correlation LP sometimes
returns a frequency plan that is not optimal under its own cost matrix
(Minera returns the optimum); case statistics on a nanosecond column give
whole seconds in pm4py. No fixture changed. The device check on the Mac
was skipped again (no device linked).

Round BR re-reviewed BQ (Review BR1; BQ's CI run and BP's were green on
all six jobs, BQ's spectrum, temporal profile, correlation and CLI sweeps
held on fresh seeds) and took its targets. It fixed the routes on the
cached model logs keyed by a CSV's own header (alignments, the
process-tree alignment, evaluation, decomposed, anti- and
multi-alignments, decisions, Monte Carlo: the Inductive log refused the
header, the alpha and heuristics logs refused `concept:name`; a key naming
the selected column under either spelling now reads the model's own log);
the Petri comparison with a CSV reference log under its own headers (read
with the session's selectors); the log families on nanosecond CSV cells
that BQ left (the batches' log variant and the pandas `datetime64[ns]`
batches, the trace-based and temporal features, the interval tree, the
stochastic map's `Timedelta.total_seconds()` durations; the temporal
features also divided a nanosecond instant through a double, so 600 ns past
a second read as 0 where `Timestamp.timestamp()` gives 0.000001); and DFG
precision on a CSV upload, which gave every event an empty activity under
the default key (BQ's check of it sent the key as a multipart field the
route ignores). Declared: transition labels of a date column as the
activity key (pm4py's `str(Timestamp)`), the stochastic-map route failing as
a whole on the reachability analysis Minera adds. No fixture changed. The
device check on the Mac was skipped again (no device linked).

Round BS re-reviewed BR (Review BS1; BR's CI run 43335386677 was green on
all six jobs, BR's stochastic-map sweep held on fresh seeds) and took its
targets. It fixed typed columns as activity keys on the workflow miners
(alpha and alpha plus label `str()` of the frame's typed cell, a date
column's UTC `Timestamp` included; heuristics, Inductive and POWL raise
pm4py's TypeErrors on keys that are no `str`; the heuristics miner keyed by
another text column now reads that column), POWL in the Petri comparison on
a CSV upload's own headers (`KeyError: 'concept:name'` on every such
upload), the stochastic-map route's reachability failure (the map is
returned with the reachability marked unavailable), the LLM abstractions on
nanosecond cells (nine-digit `Timestamp` reprs in the event stream, exact
durations, numpy's int64 quantile interpolation for date columns) and the
multipart fields of 30 query-only routes (a part other than the uploads is
rejected instead of dropped). Declared: blank float or text cells as
activity keys (pm4py's distinct NaN objects), alpha plus and POWL on date
keys with missing cells, the XES wording of some TypeErrors. No fixture
changed. The device check on the Mac was skipped again (no device linked).

Round BT re-reviewed BS (Review BT1; BS's CI run 43347116082 was green,
BS's miner, comparison and nanosecond LLM sweeps held on fresh seeds with
only BS's declared differences) and took its targets. It fixed typed
columns as the activity key of the DFG variants (native, triples and causal
variants label `str()` of the typed cell; CLEAN splits a case at a missing
cell and shifts an int64 target to floats; CLEAN_TIME takes pandas' integer
groupby means and raises pandas' errors; the performance variant runs
pm4py's column check), the DataFrame DFG on typed keys (`str()` labels,
float targets of an int64 key), the LLM abstractions keyed by a column
(`concept:name` and every CSV column with a blank raised; the DFG is the
DataFrame DFG, the variants raise pm4py's join or sort TypeError, the
stream and attributes ignore the key, the features read it), the features
descriptions' budget (one line too many at exactly `max_len`), the
start/end activities and variant statistics keyed by a column (pm4py's
column check, the upload's case column), unknown fields on the replay,
alignment and fitness upload routes, check-model and compare-petri
(compare-petri also dropped the marking form fields its client sends), and
replay fitness on a CSV upload's own headers (`KeyError: 'concept:name'`).
Declared: blank float cells as distinct NaN objects on the DFG variants
too, AQ's hash-seed feature errors. No fixture changed. The device check on
the Mac was skipped again (no device linked).

Round BU re-reviewed BT (Review BU1; BT's CI run 43358535741 was green on
the Python, verifier, frontend and macOS jobs when BU pushed, Linux and
Windows ctest still running; BT's DFG, DataFrame DFG and statistics sweeps
held on fresh seeds) and took its targets. It fixed the LLM model
descriptions keyed by a column (log skeleton, Declare and temporal profile
run pm4py's DataFrame activity column check and key by the frame's column,
the case id column too; the temporal profile keyed by `case:concept:name`
raises pandas' merge KeyError), the OCEL features description's lifecycle
paths (on by default in `abstract_ocel_features`), the OCEL events and
objects text on nanosecond timestamps (pandas' `str(Timestamp)` of the UTC
microseconds, events ordered by timestamp, activity and id, float
durations, no final newline) and unknown parameters on the ten session
routes that read their controls from the query string (`/api/align`,
evaluate, evaluate-emd, playout, petri-analysis, reduce-process-tree and
the GET exports, through `audited_query_route`). Declared: `clean_polars`
is unreachable in pm4py on polars 1.44.1; the LLM `case` variant stays
Minera's own projection. No fixture changed. The device check on the Mac
was skipped again (no device linked).

Round BV re-reviewed BU (Review BV1; BU's CI run 43367732932 was green on
all six jobs; BU's query audit held against the frontend's
parameter combinations) and took its targets. It fixed the LLM context
with a timestamp attribute on a CSV upload (the frame's own timestamp
gives the plain text, the model descriptions run pm4py's DataFrame
timestamp column check after the activity check and the temporal profile
times its pairs by the column, the DFG and variants order and time each
case by a date column with pandas' errors for missing and number columns,
the event stream sorts the rows by the column) and the error texts of the
trace filter (every value read before the join: `KeyError`, then
`sequence item i: expected str instance, T found`) and the start and end
filters (`KeyError`). The log filter bank and the attribute filters on
typed XES attributes and the OCEL edge associations on nanosecond SQLite
timestamps showed no other difference. Declared: the LLM DFG and variants
timed by a date column with gaps or a text column. No fixture changed. The
device check on the Mac was skipped again (no device linked).

Round BW re-reviewed BV (Review BW1; BV's CI run 43375436443 was green on
all six jobs; BV's LLM timestamp context, stream ranks, temporal profile
overload and trace filter types held) and took its targets. It fixed the
native Polars table filters on `Datetime` columns of ns and ms units
(refused before; now the rules of `polars_datetime.hpp`: cells at their
unit, a ms column floors the microsecond bound, a ns column floors the
cell for `<=` and `==` and compares exact nanoseconds for `>` and `!=`,
naive-column errors name the unit, durations count whole seconds of the
unit, numeric bounds meet the physical ticks), the pandas paths filters'
order on nanosecond timestamps (exact instants), the OCEL SQLite export
(event and change times floored to microseconds as `write_ocel2_sqlite`
writes them) and the OCEL features on nanosecond timestamps
(`Timestamp.timestamp()`, rounded to microseconds). The pandas table
filters, the OCEL time filter and the CSV and XML exports showed no other
difference. Declared: Polars' unstable sort of tied timestamps (4 of 1,440
answers). No fixture changed. The device check on the Mac was skipped
again (no device linked).

Round BX re-reviewed BW (Review BX1; BW's CI run 43384361792 was green on
all six jobs; BW's Datetime units, pandas paths order, OCEL features and
SQLite export held) and took its targets. It fixed the order of tied
timestamps on the native Polars paths, path performance, variants,
between, LTL and sequence filters (`polars_sort.hpp`, a port of rayon's
parallel pdqsort that Polars 1.44.1 runs for multi-column sorts; it
matched Polars on 3,200 random frames), Datetime and Date activity columns
on the Polars paths and variants filters (the String cast prints the
column unit's fraction and the zone offset), top_k with the activity key
on the case column (`DuplicateError` resolving the group_by), the Polars
timestamp attribute filter on Boolean columns (`cast(String)` match),
temporal columns with a string value (`InvalidOperationError`) and aware
nanosecond time columns (the `<=` bound printed 999 ns on, equal bounds
not merged), and the OCEL description on nanosecond timestamps
(`str(Timestamp)` texts, durations from `Timestamp.timestamp()`). The
Polars Date time column, pandas `datetime64[s]`/`[ms]` columns and the
OCEL graphs and associations on nanosecond SQLite timestamps showed no
difference. Declared: Polars join and group_by orders with non-default
keys vary per run in Polars itself. No fixture changed. The device check
on the Mac was skipped again (no device linked).

Round BY re-reviewed BX (Review BY1; BX's CI run 43393668614 was green;
the pdqsort port, the Datetime casts, top_k's `DuplicateError`, the
nanosecond bounds and the OCEL description held, and the Polars default-key
sweeps stayed clean) and took its targets. It fixed the pandas table
filters with non-default keys: the vectorised `col + ","` errors of number,
boolean and datetime activity columns on the paths and path performance
filters (also on empty frames), path performance with the case key on the
activity or timestamp column and case performance with the case key on the
timestamp column (pandas' `ValueError`s), datetime attributes compared with
a string on the timestamp attribute and repetition filters (the text parsed
as a Timestamp, NaT texts matching nothing), NaT, NaN and None case values
and activities on top_k, coverage and between (numpy's unique runs: typed
arrays collapse missing values into one last run, object arrays compare
with Python and raise on None; coverage over the non-missing case ids;
numpy labels of naive case dates at their unit). On the OCEL routes with
nanosecond SQLite timestamps it fixed the connected-component split (pm4py
drops O2O rows and object changes there) and the SQLite row order: pm4py
keeps `event_object` rows, and object type table change rows, in their own
order sorted by time, which the lifecycle qualifiers and the event/object
graph (DF edges, change node numbering) follow; the JSON transport now
carries `minera:relationOrder` and `minera:objectChangeOrder` when the log
order differs from the nesting. Conformance (etot, otg, ocdfg), statistics
and the ancestors/descendants split showed no difference. Declared: pandas
membership of naive `datetime64[ns]` activity tuples with sub-microsecond
values depends on `PYTHONHASHSEED` in pandas itself (6 of 12,000 answers),
non-ISO texts against a datetime column, and the connected components'
order in pm4py (set iteration). No fixture changed. The device check on
the Mac was skipped again (no device linked).

Round BZ re-reviewed BY (Review BZ1; BY's CI run 43409678034 and BX's
43393668614 were green; the datetime targets and BY's typed-key sweep held)
and took its targets. On OCEL SQLite files it fixed repeated
`event_object` rows (kept by `read_ocel2_sqlite`; both JSON transport forms
now carry `minera:repeatedRelations`), the O2O row order across source
objects (`minera:o2oOrder`), the OCDFG start and end activities of tied
logs (first and last relation row, edges in event order), the OCPN and SAW
per-type traces (relation rows with repeats in event order, sorted by time
only for the classic IM; IMd edges from those traces, its start and end
activities from the relation rows; `/api/ocel/discover` takes `ocpn_variant` and the OCEL
explorer has an "Inductive miner" selector) and the lifecycle qualifiers
of repeated rows. On the pandas table filters it fixed nullable and
categorical keys: pandas 3 hands an `Int64` column with a gap to NumPy as
floats with NaN (variants statistics and between), a categorical gap as a
NaN object, and `isin` on a masked numeric case column never matches
`<NA>` (attributes and timestamp attribute filters); the paths filter
raises the vectorised errors of interval and Sparse columns. The pandas
LTL filters with non-default keys now fail on duplicated role keys in
pandas' order. Declared: the O2O enrichment's hash-seed link order in
pm4py, the merged SAW net weights per arc versus per transition, no
timedelta cells in the table transport, and the pandas sequence filters
(no route; typed keys moved to CA). No fixture changed. The device check
on the Mac was skipped again (no device linked).

Round CA re-reviewed BZ (Review CA1; BZ's CI run 43425776838 was green;
the pandas table filters and LTL filters with typed keys held on 150 seeds
and 1,500 answers) and took its targets. Two new banks replay the sequence
filters on typed keys: `docs/pandas-sequence-filter-typed-bank.json.gz`
(categorical, nullable, `boolean` and `string` cases and activities) and
`docs/polars-sequence-filter-typed-bank.json.gz` (Categorical and Enum
cases and activities, `UInt8`, `Boolean` and `Float64` cases), captured with
the existing capture scripts and their new `--domain` option (the Polars one
with `--repeats 8`). The pandas fixes cover categorical arithmetic in the
case grouping, `<NA>` runs of `string` keys, categorical prefixes and
suffixes, NumPy views in the activity split and NA end activities; the
Polars fixes cover the variant list join (String activities only), the
strict Enum cast of `is_in` values, the join key dtypes, Enum casts of
prefix and suffix lists, and the case sort by dtype. The Polars table
filters now take `UInt8` and the other narrow integer keys. On OCEL SQLite
logs with repeated relations it fixed the edge metrics (repeats and
self-edges) and the description (`o1, o1`); the reference OCPN playout no
longer stops at 100 traces when `max_traces` is not given. Conformance
(etot, otg, ocdfg), the other statistics, the event/object graph, splits
and object graphs were equal on the same logs. Declared: the pandas rework
KeyError column order (Python set order), Polars per-run row orders on a
few frames, and the playout's size on logs with very many sequences. No
fixture changed. The device check on the Mac was skipped again (no device
linked).

Round CB re-reviewed CA (Review CB1; CA's CI run 43473813818 was green on
all six jobs; fresh seeds of both typed sequence filter domains, 13,807 and
13,096 calls, and the OCEL association statistics with repeated relations
held) and took its targets. The reference OCPN playout now skips states
from which no complete sequence is reachable, visits sequences without
keeping the list, and `/api/ocel/playout` converts each trace as it is
found and stops at `max_traces` (seed 35 of the round-CA sweep: 647 s to
5.6 s, pm4py 81 s; a log with 5,187,076 sequences now finishes in 24 s).
Two new banks replay the sequence filters on temporal keys:
`docs/pandas-sequence-filter-cb-bank.json.gz` (`datetime64` cases and
activities in us, ns, UTC and Berlin ms with NaT, `int64`/`Int64`
activities; 22,745 calls) and `docs/polars-sequence-filter-cb-bank.json.gz`
(`Int64`, `UInt8`, `Date` and `Datetime` activities in us, ns and Berlin
ms; 19,431 calls, eight runs per seed). The pandas fixes cover the case
grouping's addition errors with a `datetime64` side, the activity split's
NumPy and object sorts of datetime cases, integer ids of naive ns keys in
rework and `nocc`, and NaN keys of masked numeric end activities; the
Polars fixes cover Python list literals (inference and the strict
rebuild), the `is_in` dtype rules and plan casts, String casts of
variants, casts of prefix and suffix lists, the item checks of the
pattern filters, and datetime lists in microseconds (ns columns compare
raw numbers, ms columns refuse them; plans print zone abbreviations and
shorten lists of more than three values). Declared: the pandas rework
KeyError order, Polars per-run row orders, the 3.14 spelling of the
membership error, and the playout explosion with two bindings. No fixture
changed. The device check on the Mac was skipped again (no device linked).

Round CC re-reviewed CB (Review CC1; CB's CI run 43495896217 was green
except on Windows, where seven calls of the Polars CB bank printed ICU's
`GMT+1` for `CET`: the declared MSVC abbreviations, now masked by the bank
test on MSVC only; a fresh seed of the pandas CB domain found the float
keys of masked integer end activities with a missing value, the Polars
one held) and took its targets. Two new banks replay the sequence filters
on more temporal keys: `docs/pandas-sequence-filter-cc-bank.json.gz`
(`datetime64` in s, ms, us around the epoch, Kolkata us and New York ns
across DST, object Timestamps naive, mixed or in two zones, Python
datetime arguments; 17,110 calls, 71 mismatches before) and
`docs/polars-sequence-filter-cc-bank.json.gz` (`Datetime` in ms, Kolkata,
New York ns and Berlin across both DST changes, around the epoch, `Date`,
`Int8` to `UInt64` and `Float32`; Python datetime, date, int, float, bool
and nested list arguments; 15,621 calls, 830 mismatches before). The
pandas fixes cover the `datetime.datetime` type names, the inferred zone
of object Timestamps, `isin` of datetime lists on `datetime64` columns
(converted only when every value fits the column; DST folds) and datetime
arguments of the activity split; the Polars fixes rebuild list literals as
`pl.lit` builds them (first-item constructors, the supertype fold and its
errors), check prefix and suffix lists by supertype (plan-time casts, the
collect-time comparison error), cast datetimes to numeric and temporal
columns, and print cast values in the column's dtype. A new Polars table
filter domain (`scripts/polars_filter_cc_domain.py`, `--domain` and
`--repeats` in `scripts/capture-polars-filter-bank.py`) differs in 3,293
of 33,137 calls; it is round CD's main target and not in the suite yet.
Declared: MSVC's abbreviations in the Polars plans, Python set orders in
`rework`. No fixture changed. The device check on the Mac was skipped
again (no device linked).

Round CD re-reviewed CC (Review CD1; CC's CI run 43510000808 was green on
all six jobs, Windows included; fresh seeds of the CC sequence domains
were equal apart from one declared set order) and took CC's main target.
The Polars literal rules of CC1-4 now live in
`src/backend/polars_lit_model.hpp`, shared by the sequence and table
filters, together with a new comparison model (`check_scalar`,
`compare_scalar`, `coerced_operands`, `comparison_plan`: how Polars 1.44
types, folds, prints and evaluates `col(x) OP v` per column dtype and
Python scalar). The table filters use it throughout: `Float32` keys, the
numeric filters' typing and their same-column interval merge, the LTL
plans in resolution order, between's anchors (imploded scalars, list
literals, the second anchor reported first, resolved casts, `Float32`
lookups, temporal case ids), `filter_traces_attribute_in_timeframe` on
typed attributes, and the variants join height with a matching null
case. Fresh seeds of the domain (4243, 4244) and of the base domain (4245)
found three more (folded forms printed bare in evaluated predicates, None
bounds of `filter_case_performance`, datetime lookups by microseconds in
the value filters) and left six calls open (below). The first full ctest
failed six tests (CD1-9: `__int128` in core code, `is_in` on Null
columns, UInt64 literals above the Int64 range, between's empty
categorical entries); all six were fixed before the commit.
`docs/polars-filter-cc-bank.json.gz` (33,137 calls; 3,293
mismatches before) is in the suite as
`minera_polars_filter_cc_bank_parity_tests`. Declared: the between case
order per run, rarely the case order of `attr_value_different_persons`,
`filter_on_ncases` selections on every case dtype, and the `is_in` list
order of the group_by filters (the bank test compares these up to that
order). No fixture changed. The device check on the Mac was skipped again
(no device linked).

Round CE re-reviewed CD (Review CE1). CD's CI run 43532993607 was green
except for the Windows ctest: 45 calls of the Polars CC bank printed
ICU's `GMT+1` for `CET` in the kept values of the group_by filters, so
the table filter bank test now masks zone abbreviations on MSVC only, as
the sequence bank test does. The six calls CD left open are fixed: a
predicate pushdown model (fitted on 12,000 random chains) places merged
bounds after the column's unmerged predicates, left-nested, with a None
bound's null column where pushdown leaves it and stream filters of
several columns in its order; LTL compares lists on non-String columns as
lists; the bank test checks spno ties as tie sets. A new edge-value
domain (`scripts/polars_filter_ce_domain.py`: Boolean and `Null`
activity columns, ints past 2**53 and the Int64 range, NaN, infinities,
-0.0, datetimes before 1970, numeric filters on the activity column)
differed in 159 of 23,212 calls; the fixes give the shared literal model
UInt64 and Int128 literals (supertypes, `is_in` casts, comparisons in
every table filter), `Null` and Boolean column casts, frame float
display, ms floor division before 1970, list literals through the model
in every table filter, merges only on the bare column (Enum strs as
text), resolution errors with the leaves after the failing one raw,
resolved scalar `is_in` plans, and paths' list TypeError and None
bounds. Fresh seeds 2026111437, 4246 and 4247 found eleven more (among
them `==` on nanosecond columns merging as a range and between's
separator error on typed case and activity columns), all fixed but two
declared reference case orders (`filter_traces_intersecting`,
`filter_on_case_performance`).
`docs/polars-filter-ce-bank.json.gz` (23,212 calls) is in the suite as
`minera_polars_filter_ce_bank_parity_tests`. No fixture changed. The
device check on the Mac was skipped again (no device linked).

Round CF re-reviewed CE (Review CF1). CE's CI run 43543514400 was green
on all six jobs (Windows 488/488 with the abbreviation mask). Fresh CE
seeds 2026111450, 4250 and 4251 found a list value on a Categorical
timeframe column and a declared case order of
`filter_on_case_performance`. A new domain
(`scripts/polars_filter_cf_domain.py`: scalar, None and edge values in
the attribute filters, odd numeric bounds, typed and nested between
anchors, typed, tuple and dict timeframe values, odd path shapes and
bounds) differed in 85 of 30,328 calls; the fixes give plans and frames
one float display (Rust's Display for `dyn float`, Polars' `fmt_float`
for Float64: `1e7`, not `1.0000e7`), fold a None membership to `false`,
null UInt64 cells past Int64 against negative ints, raise on ordering a
list on a Null column, merge list literals only when equal, cast Null
activity columns to resolved between anchors, compare timeframe lists on
Categorical, Enum and Null columns while collecting, filter the timestamp
column as attribute apart from the window, model dicts on String columns
as the struct's String cast, and print paths list and wide-int bounds as
Polars does (equal bounds merge into `==`). `filter_on_ncases` may keep
k - 1 cases when a null id is among the k it picks; the bank test accepts
any such selection. Declared: a Polars panic on a Null column with Int64
between anchors, datetimes outside the nanosecond range, other dict
values of the timeframe filter. Fresh CF seeds 2026111461, 4261 and 4262 and CE seed 4263 found four
more (equal list bounds in paths and in `filter_case_performance`, NaN
bounds that Polars' merge orders totally, a declared case order of
`filter_case_performance`), all fixed.
`docs/polars-filter-cf-bank.json.gz` (30,328 calls) is in the suite as
`minera_polars_filter_cf_bank_parity_tests`. No fixture changed. The
device check on the Mac was skipped again (no device linked).

Round CG re-reviewed CF (Review CG1). CF's CI run 43553607248 was green
on all six jobs (the CF bank on Windows for the first time). Fresh CF
seeds 2026111470 and 4270 and CE seeds 2026111471 and 4271 found eight
calls: timeframe list values printed as transported text in the failed
plan, a failing case-performance bound with a None upper bound naming a
larger expression, inverted str bounds of the paths filter raising
instead of folding, the one-item path `[65536]` raising an IndexError
instead of pm4py's TypeError, and tied activities of
`filter_df_keeping_spno_activities` (any k of them, null included; the
bank test accepts any valid top-k listing). The timeframe filter now
models dict (struct) values on every column (probed on 19 dtypes: String
casts, one-field casts of integer and float columns to the fields'
supertypes, "must specify one field in the struct" only on a column with
a valid cell, SchemaErrors on Boolean, temporal and uncastable columns,
ComputeErrors on Categorical and Enum, a None field matching null cells
only when the column has a valid one) and the order of the pushed-down
predicate when the attribute column sorts after the timestamp column
(and only when it merges, on an equal window). The case filters name
list bounds by their `pl.lit` type, coerce the other bound as Polars
prints it, merge equal bounds and report larger expressions in
`filter_on_case_size` too, and fold inverted str bounds to an empty
interval (the paths filter too). A new domain
(`scripts/polars_filter_cg_domain.py`) found 3 more calls in its first
bank (predicate order) and fresh CG seeds 4280, 2026111481 and 4281
found 2 (None fields on all-null columns), all fixed.
`docs/polars-filter-cg-bank.json.gz` (19,011 calls) is in the suite as
`minera_polars_filter_cg_bank_parity_tests`. No fixture changed. The
device check on the Mac was skipped again (no device linked).

Round CH re-reviewed CG (Review CH1). CG's CI run 43562859480 was green
on all six jobs (the CG bank on Windows for the first time). Fresh CG
seeds 2026111490 and 4290, CF seed 2026111491 and CE seed 2026111492
found 2 calls, both reference-random case orders (`filter_on_case_size`,
`filter_df_on_attribute_values`; the bank test accepts them). Polars'
cast of a float to String (`1e-5` as `0.00001`, `1e-7` as `1e-7`,
`1e16` as `1e+16`, Float32 digits for Float32 cells) is now modelled by
`polars_cast_float` in `src/backend/polars_float_text.hpp` (fitted on
805,046 values) and used by `string_cast`, the struct String cast and the
between, paths and variants filters; a dynamic float literal casts the
same way (`ends_with` suffixes). The timeframe filter models nested dict
fields (nested structs cast recursively, one nested field failing the
cast on a column with a present cell, list fields never casting, unbuildable
lists raising pm4py's TypeError while the comparison is built). Two new
domains of odd float activities (`scripts/polars_filter_ch_domain.py`,
`scripts/polars_sequence_filter_ch_domain.py`) found 174 calls in the
first table bank (Float32 lookups, `is_in` against a column named by a
str value, list merges of -0.0 and NaN, the String casts, nested dicts,
and 61 transport artifacts the domain now avoids) and 23 in the first
sequence bank (`ends_with` float suffixes, `activity_split` scalars on
Float32, `consecutive_act_case_grouping` rows in random order). Ints now
round to Float32 straight from their digits (a probe domain of wide ints:
90 calls), NaN variants stay distinct in `filter_variants_top_k`, and an
empty key tuple of `filter_df_relative_occurrence_event_attribute`
resolves the case key first (fresh CH seed 4295), and a String column read
against an Enum column fails on strs outside the categories while
collecting (fresh CH seed 2026111498). All fixed:
`docs/polars-filter-ch-bank.json.gz` (31,894 calls) and
`docs/polars-sequence-filter-ch-bank.json.gz` (19,761 calls) are in the
suite as `minera_polars_filter_ch_bank_parity_tests` and
`minera_polars_sequence_filter_ch_bank_parity_tests`. No fixture changed.
The device check on the Mac was skipped again (no device linked).

Round CI re-reviewed CH (Review CI1). CH's CI run 43572000391 was green
on all six jobs (the CH banks on Windows for the first time). Fresh CH
table seeds 2026111401 and 4301, CH sequence seed 2026111402, CG seed
2026111403, CF seed 2026111404 and CE seed 2026111405 found 1 call, a
reference-random case order of `eventually_follows` (the bank test accepts
it). A new domain (`scripts/polars_filter_ci_domain.py`) passes dicts,
lists of dicts and lists Polars cannot build to the attribute, LTL, paths
and timeframe filters, on missing columns too; its first bank found 641
calls. The shared literal model (`src/backend/polars_lit_model.hpp`) now
builds struct items of `pl.lit` lists (field types, struct supertypes
with Polars' field order, strict builds with field context stacks, casts
in nested lists, display), `is_in` fails on struct items on every column,
the LTL filters compare dict activities with the timeframe filter's
struct model (moved into the shared model), `eventually_follows` runs its
filters first, last, then the middle ones in order, `A_next_B_next_C` on
an empty frame with a missing attribute column fails the join, and the
timeframe filter builds lists of dicts instead of refusing them. The
remaining `real_text` uses were checked (fallbacks the reference never
shows; dynamic floats now in Rust's display everywhere). All fixed: seed
2026111412 found 1 more call (a struct field failing its cast inside a
nested list fails the build), and seed 4402 found 0 on its first capture
and is in the suite as
`docs/polars-filter-ci-bank.json.gz`
(`minera_polars_filter_ci_bank_parity_tests`, 22,843 calls). No fixture
changed. The device check on the Mac was skipped again (no device
linked).

Round CJ re-reviewed CI (Review CJ1). CI's CI run 43580317951 was green
on all six jobs. Fresh CI seed 2026111420, CH table seed 2026111421, CH
sequence seed 2026111422, CG seed 2026111423, CF seed 2026111424 and CE
seed 2026111425 found 1 call: `between.apply` on a Null activity column
prints the resolved `is_in` of a dynamic number as
`.cast(Unknown(Float))` / `.cast(Unknown(Int(1)))` (round CF's
`Float64`/`Int32` fit never met a bank case). Two new domains:
- `scripts/polars_filter_cj_domain.py` passes dict bounds and anchors to
  the numeric, relative occurrence, between, case and paths performance
  filters, dict path items on typed columns, and lists that mix plain
  lists, lists of dicts and scalars. Its banks found 607 and 7 calls.
- `scripts/polars_sequence_filter_cj_domain.py` puts dicts into the
  activity values of the sequence filters. Its banks found 39 and 1
  calls.

What changed:
- Dicts go through `pl.lit` strictly: a list field that fails gives the
  `cannot create expression literal for value of type dict` TypeError.
  The struct-to-String cast gives `}` for an empty struct and null for a
  None field.
- Dict bounds of the case and paths performance filters are struct
  literals. The column is cast to the struct and fails on a present cell,
  or else the struct comparison fails. Partitioning in the reference
  races between those two errors; this is declared, and the bank test
  accepts both.
- Dicts in the numeric filters merge only with equal dicts on an uncast
  column. A null-cast dict compares with null under AND and folds under
  OR.
- A dict, set or frozenset stake of the relative occurrence filter raises
  Python's `*` TypeError.
- `[[1], [{"a": 1}], 5]` fails in the list builder as Polars does.
- Dict activities work in every sequence filter: keys in
  `activity_split`, struct patterns cast field by field on numeric
  columns, the String cast in `ends_with`.
- The prefix and suffix cast failures now carry Polars' resolved plan.

All fixed:
- `docs/polars-filter-cj-bank.json.gz` (seed 2026111432, 27,050 calls) is
  in the suite as `minera_polars_filter_cj_bank_parity_tests`.
- `docs/polars-sequence-filter-cj-bank.json.gz` (seed 2026111433, 12,976
  calls) is in the suite as
  `minera_polars_sequence_filter_cj_bank_parity_tests`.

No fixture changed. The device check on the Mac was skipped again (no
device linked).

Round CK re-reviewed CJ (Review CK1). CJ's CI run 43591565043 was green
on all six jobs, so the struct partition race tolerance held on the
Windows and macOS runners. Fresh seeds 2026111510 to 2026111520 of the CJ
table and sequence, CI, CH table and sequence, CG, CF, CE, CC table and
sequence and CB sequence domains (189,119 calls) found 3 calls, carried
to CL. Four new domains:
- `scripts/pandas_filter_ck_domain.py` puts dicts, lists of dicts and
  dicts next to other values into the column keys, values and
  parameters of the pandas table filters. Its banks found 812 and 15
  calls; all but one are fixed.
- `scripts/pandas_sequence_filter_ck_domain.py` does the same for the
  pandas sequence filters. Its first bank found 1,037 calls (carried).
- `scripts/polars_sequence_filter_ck_domain.py` gives 55% of the frames a
  Categorical or Enum activity column, adds prefix/suffix patterns that
  mix lists, dicts, floats, bools and None, dict items in
  `rework.apply_activity_set`/`traces.apply`, and dict parameter values
  (case-grouping `FILTER_TYPE`, key parameters). Its first bank found 315
  calls (carried).
- `scripts/polars_filter_ck_domain.py` puts dicts where a column name goes
  (key parameters and `case_id_glue`/`attribute_key`/... keyword
  arguments) and `[{"a": 1}, [-1], 5]`-style lists into the attribute
  value calls. Its first bank found 771 calls (carried).

What changed (pandas table filters):
- KeyErrors print Python's repr of list and dict keys and values.
- Unhashable keys fail where pandas checks them: `key in df.columns`,
  `df[key]` (`Passing a dict as an indexer ...`), `df.set_index(key)`
  (helpers in `src/backend/pandas_parameter_carrier.hpp`).
- A dict compares with a str or datetime64 column as one 0-d object of
  its keys, elsewhere as a scalar; per-row case sizes compare with an
  object bound row by row.
- `StringArray.isin` keeps only str and null values; `isna` of a list
  needs exactly one element; kept lists must stay one-dimensional.
- List bounds with None: a float array when the rest are numbers, else
  an object array whose None fails; one-item tuples holding nested lists.
- `paths.apply_performance` applies the maximum only to what the minimum
  kept.
- Dict case keys are groupby mappings: no case forms (variants keep all
  rows when negative, the relative occurrence filter per case keeps
  none), the timeframe filters read the timestamp column first; list case
  keys holding a dict are grouper arrays in the relative occurrence
  filter.
- NaN cells of float columns compare with dict and list bounds; tuple
  timeframe values on str columns need one item per row; dict LTL
  boundaries fail on `[0]`; list LTL targets compare item by item; list
  keys keep nested lists unless they form a regular 2-D array.
- The LTL filters evaluate `path[0]` before `path[1]` (C++ left the
  order open).
- The pandas sequence bank test turns a native exception into a mismatch.

All fixed on the pandas table side:
- `docs/pandas-filter-ck-bank.json.gz` (seed 2026111530, 11,694 calls)
  is in the suite as `minera_pandas_filter_ck_bank_parity_tests`.

No fixture changed. The device check on the Mac was skipped again (no
device linked).

Round CL re-reviewed CK (Review CL1). CK's CI run 43657977486 was green
on all six jobs. CL worked the three banks CK carried:
- The pandas sequence dict bank (seed 2026111531) went from 1,037 calls
  to 0: `Series == value` by dtype family for dict, list and tuple
  values, dict and list keys in each filter's order of groupby,
  selection, `df[...]`, hashing and `set_index`, the start/end `isin`
  and nocc checks, non-dict parameter carriers, trace parts, a non-str
  `SUBCASE_CONCAT_STR`, dict keys of `timestamp_case_grouping`. A fresh
  seed (2026111541, 6,759 calls) matched.
- The Polars sequence bank (seed 2026111532) went from 315 calls to 17:
  construction-time key errors (`Cannot pass a dictionary ...`,
  `invalid input for col`) in each filter's order, `pl.col(dict)` as a
  by-name selection (`"k" not found`, first missing name), `{}` keys
  (missing alias, `_group_id`, zero expressions), the activity literal
  of `rework.apply`, str/list/dict `MIN_OCCURRENCES` (the struct
  comparison model of the table filters, now also built from a
  `PolarsFilterValue`), repeated parameters keeping the last value. A
  fresh seed (2026111542, 4,366 calls) found 6 calls in the carried
  classes.
- The Polars table bank (seed 2026111533) went from 771 calls to 497:
  one-field struct Series take one-item list or tuple rows, `pl.col`
  lists name a non-str first item as the iterable's type, a dict case
  key of `between.apply` is refused as a positional argument.

All three are in the suite:
- `docs/pandas-sequence-filter-ck-bank.json.gz` (9,376 calls) as
  `minera_pandas_sequence_filter_ck_bank_parity_tests`;
- `docs/polars-sequence-filter-ck-bank.json.gz` (11,587 calls, the CK
  seed without the 17 carried calls) as
  `minera_polars_sequence_filter_ck_bank_parity_tests`;
- `docs/polars-filter-ck-bank.json.gz` (21,406 calls, the CK seed
  without the 497 carried calls) as
  `minera_polars_filter_ck_bank_parity_tests`.
The full banks with the carried calls are in the project files under
`review-harness/round-cl/banks/` (`tools/dropcalls.py` and
`tools/dropdump.py` drop a replay's mismatches from a bank).

No fixture changed. The device check on the Mac was skipped again (no
device linked).

Round CM re-reviewed CL (Review CM1). CL's follow-up commit 7b12783 ran
green on all six CI jobs (run 43670069978). CM worked the Polars table
dict keys and fresh seeds of every recent domain:
- The Polars table bank (seed 2026111533) went from 497 calls to 65.
  `src/backend/polars_key_selection.hpp` holds the shared model:
  `pl.col(dict)` reads the dict's keys as `pl.col(list)` does (one
  existing name is that column, the first missing name fails `"k" not
  found` at the node where a missing str column fails, `{}` is the empty
  selection), and `group_by`/`sort`/`unique`/`over` refuse a dict while
  the plan is built (`Cannot pass a dictionary as a single positional
  argument.`). Each filter checks in its own probed order (timestamp:
  the bounds, then the case dict, then the selectors, the timeframe
  filter after its attribute selector and value literal; LTL:
  `eventually_follows`/`A_next_B_next_C` the case dict first,
  `four_eyes_principle` and `attr_value_different_persons` after the
  attribute and literals; paths: after the attribute column; variants:
  first; cases: after the bound literals). The attributes stats read
  lists holding dicts in the top-activities select, `[name, dict]` case
  keys fail Python's `>=` on present rows; `between.apply` misses
  `_is_act1` behind its plan when the activity key selects nothing.
- Pandas table filters (fresh CK seed 2026111552, 3 calls → 0): an enum
  `POSITIVE` wins over the `"positive"` str key in `variants.apply`, the
  relative occurrence filter's stake errors need a counted value, list
  bounds against an empty-list attribute fail `Unable to coerce to
  Series, length must be 0: given N`.
- Polars sequence filters (fresh CJ seed 2026111555, 2 calls → 0; fresh
  CK seed 2026111551, 21 → 17): an empty-dict trace item reads as None
  and a trace holding None joins to null (matches nothing; the model
  used to match it as ""); a struct pattern item whose one field casts
  to null on a numeric column equals a null activity only when some
  case's slice holds a present activity; `ends_with` with an activity
  key selecting nothing builds its items, then misses `variant` (no
  plan); `activity_split` casts a non-str `SUBCASE_CONCAT_STR` to String
  (int digits, `true`, Polars float text, ISO dates, struct display or
  null for a dict, null for None).
- Fresh seeds that matched: pandas sequence CK 2026111553, CJ table
  2026111554, CI table 2026111556, CH table 2026111557, CH sequence
  2026111558. The fresh Polars table CK seed 2026111550 differs in 85
  calls, nearly all in the carried classes below.

In the suite:
- `docs/polars-filter-ck-bank.json.gz` (seed 2026111533 without the 65
  carried calls, 21,838 calls) as `minera_polars_filter_ck_bank_parity_tests`;
- `docs/polars-sequence-filter-cm-bank.json.gz` (CK seed 2026111551
  without its 17 carried calls, then CJ seed 2026111555; 18,908 calls)
  as `minera_polars_sequence_filter_cm_bank_parity_tests`;
- `docs/pandas-filter-cm-bank.json.gz` (pandas table CK seed 2026111552,
  11,730 calls) as `minera_pandas_filter_cm_bank_parity_tests`.
The full banks and replay logs are in the project files under
`review-harness/round-cm/`.

No fixture changed. The device check on the Mac was skipped again (no
device linked).

Round CN re-reviewed CM (Review CN1). CM's commit 5ab3d71 ran green on
all six CI jobs (run 43677891938). CN worked literal keys in the Polars
filters, list time bounds in pandas and fresh seeds of every recent
domain:
- List and tuple case keys are built as `pl.lit` builds them (strictly)
  wherever the reference lists them among sort, `over` or join keys,
  before anything else fails: `between.apply` then reads them with
  `pl.col`, the LTL rules check them in their probed order, `paths`
  right after parsing the paths, `variants` first (a list of present
  names that builds fails the sort, `Series length: 1`). A None item in
  a `pl.col` list fails PyO3's `'None' is not an instance of 'str'` at
  every site.
- Dict and list timestamp keys sort as struct or list literals: the plan
  prints the literal, resolution errors come first, the sort then fails
  `ShapeError` unless the frame has one row (`paths`: no admitted path
  prunes the sort; variants statistics; `eventually_follows` with two or
  more activities, after its activity filters resolve).
- `timestamp_case_grouping` with list/tuple/dict keys: Polars groups by
  a `literal` column (two literal keys fail the duplicate `literal`
  name), pandas checks a list key's length, fails `reset_index` for
  other aggregations, then `unhashable type: 'dict'`.
- Pandas `eventually_follows` with list time bounds (CK case 501 and a
  new domain `scripts/pandas_filter_cn_ltl_domain.py`, 2,400 calls): all
  chains first, each step's lower bound filters before its upper one
  compares, list bounds compare as pandas compares a float Series with a
  list. Smaller pandas items: dict paths, str `>=` in the attribute
  filter, `activity_split` items as numpy scalars.
- The Polars table banks: seed 2026111533 65 → 40 calls, fresh seed
  2026111560 64 → 35; the Polars sequence banks: seed 2026111532 17 → 9,
  seed 2026111551 17 → 6, fresh seed 2026111561 5. Every other fresh
  seed matched (pandas table and sequence CK, CJ table and sequence, CI
  table, CH table and sequence).

In the suite:
- `docs/polars-filter-ck-bank.json.gz` (seed 2026111533 without the 40
  carried calls, 21,863 calls), the new `docs/polars-filter-cn-ck-bank.json.gz`
  (seed 2026111560 without its 35, 22,048 calls) and
  `docs/polars-filter-cn-bank.json.gz` (117 explicit calls,
  `scripts/polars_filter_cn_domain.py`);
- `docs/polars-sequence-filter-ck-bank.json.gz` (seed 2026111532 without
  the 9, 11,595 calls) and the new `docs/polars-sequence-filter-cn-bank.json.gz`
  (seed 2026111561 without its 5, 8,611 calls);
- `docs/pandas-filter-cn-bank.json.gz` (list time bounds, 2,400 calls).
The full banks and replay logs are in the project files under
`review-harness/round-cn/`.

No fixture changed. The device check on the Mac was skipped again (no
device linked).

Round CO re-reviewed CN (Review CO1). CN's commit 282c568 ran green on
all six CI jobs (run 43687579084). CO worked literal case keys in the
Polars join filters and LTL rules and timestamp columns of other dtypes:
- A shared group-key model (`src/backend/polars_group_keys.hpp`): each
  item of a list case key is a `group_by`/join key (a str a column, any
  other value a `pl.lit` literal named `literal`), resolved in order
  (missing column, duplicate output name); literals only select one row
  whatever passed (inner join keeps all, anti join none), a Null literal
  or null cell never joins, an inner join on literal keys appends
  `<name>_right`/`literal`. Wired into the cases, attributes and
  timestamp filters; struct/list output cells count as unrepresentable
  in the bank test.
- LTL rules with a literal case key (a non-str scalar or a one-item list
  of a non-str): data-independent results with the `literal` column,
  `ShapeError` for two `eventually_follows` activities, the
  `literal_right` DuplicateError (with the third activity's plan) for
  three or more.
- Timestamp columns of any dtype (a dict timestamp key selects its
  names): Int32/UInt32/Float32 compare as physical microseconds with a
  cast in the plan, narrow ints fail the comparison, Categorical/Enum
  fail `cannot compare categorical with datetime[μs, UTC]` when the plan
  runs, a String column fails while resolving even with an empty window;
  `filter_case_performance` on these dtypes.
- Evaluation-order fixes (dict case key in the case filters, list
  values and list timestamp keys, between anchors, the first
  `eventually_follows` activity below a literal sort), paths concat casts
  and list sort keys, lazy LTL diagnostic placeholders.
- The Polars table banks: seed 2026111533 40 → 0, seed 2026111560
  35 → 2, fresh seed 2026111580 42 → 0; fresh CH, CI, CJ table seeds
  and the CH sequence seed matched. The Polars sequence calls were not
  worked.

In the suite: `docs/polars-filter-ck-bank.json.gz` is the whole seed
2026111533 bank again (21,903 calls), `docs/polars-filter-cn-ck-bank.json.gz`
seed 2026111560 without 2 calls (22,081); new
`docs/polars-filter-co-bank.json.gz` (fresh CK seed 2026111580, 22,074
calls) and `docs/polars-filter-co-probe-bank.json.gz` (1,388 explicit
probe calls). The full banks, probe specs and replay logs are in the
project files under `review-harness/round-co/`.

No fixture changed. The device check on the Mac was skipped again (no
device linked).

CO's CI run 43698178880 failed only on Windows, in one tied-listing call
of the CO bank: the bank test compared MSVC's zone-masked listing with
unmasked count keys. A follow-up commit masks the keys alike (test only).

Round CP re-reviewed CO (Review CP1). CO's follow-up commit 02f8f96 ran
green on all six CI jobs (run 43702198127). CP worked list and tuple
case keys in the Polars sequence filters, the Polars table leftovers and
the pandas seeds CO captured:
- List case keys in the Polars sequence filters (new domain
  `scripts/polars_sequence_filter_cp_domain.py`, seed 2026111600 121 → 0;
  probes k1–k10): `sort([case, timestamp])` and `group_by([case,
  activity])` take the list as one list literal (built while Python
  builds the plan, `ShapeError` when the sort runs on a frame of another
  height), `group_by`/`select`/`join` take the items as keys
  (`polars_group_keys.hpp` with Python values); literal-only keys plan as
  a `SELECT`; an empty list joins without keys; filters with only empty
  patterns prune the sort; `traces`, `rework`, `ends_with`,
  `activity_split` (list separators) and the consecutive grouping's error
  order. The carried sequence calls (9 + 6 + 5 + 2 + 1) all pass; one
  rework join of the fresh CH seed 2026111599 (case 542) has no fixed row
  order and is compared unordered.
- Polars table leftovers: `A_next_B_next_C` with a dict attribute key on
  empty frames, LTL attribute selectors expanding to nothing,
  `four_eyes_principle` list case key literals, `head(n)` checks in
  `filter_df_keeping_spno_activities`, list keys as Series literals in
  the count schema and the variants statistics, str struct fields cast to
  Float64. Carried tables 17 → 3 (one `apply_numeric` call each in CM
  2026111550, CK 2026111590, CJ 2026111595: struct-cast error order).
- pandas (6 → 0): the relative occurrence filter's three key reads
  (per-case grouping last-given, case count and event filter enum first)
  and empty-frame list/tuple attribute keys; `variants.apply` groupers
  listed with a dict; nested list case keys in `filter_on_case_size`;
  pd.NA against a dict in `activity_split`; masked NA activities added
  to a pandas string case column.

In the suite: the CK, CM and CN Polars sequence banks and the CN-CK
Polars table bank hold their whole seeds again; new
`docs/polars-sequence-filter-cp-bank.json.gz` (8,324 calls),
`docs/polars-sequence-filter-cp-fresh-bank.json.gz` (CK 2026111581,
2026111591, CH 2026111599; 25,300 calls), `docs/polars-filter-cp-bank.json.gz`
(CK 2026111590 without its carried call; 22,006), `docs/pandas-filter-cp-bank.json.gz`
(CK 2026111582, 2026111592; 23,430) and `docs/pandas-sequence-filter-cp-bank.json.gz`
(CK 2026111583, 2026111593; 13,658). The fresh banks, probe specs,
pandas case probes and replay logs are in the project files under
`review-harness/round-cp/`.

No fixture changed. The device check on the Mac was skipped again (no
device linked).

Round CQ re-reviewed CP (Review CQ1). CP's commit a299246 ran green on
all six CI jobs (run 43714424998). CQ took the calls CP carried and fresh
seeds:
- Struct-cast error order (`polars_attributes_filter.cpp`): Polars
  evaluates an elementwise filter predicate in four row partitions (the
  capture host's thread count; three of n/4 rows, the last the rest) when
  the frame is at least four columns wide, and the first failing partition
  decides. A struct cast fails only on a partition with a present cell, a
  comparison on any partition; trace filters read the projection of the
  case and filter columns, null bounds run whole. The three carried
  `apply_numeric` calls pass; CK 2026111550 case 320 and the CJ docs
  bank's case 71 race in the reference and compare against both observed
  outcomes.
- Activity keys naming several columns in the Polars sequence filters
  (k6 18 → 0, probes q1–q3): repeated aggregation aliases, the expanded
  `is_in` predicate of `rework.apply_activity_set`, the duplicate
  `_is_split_activity`, and the grouping's selector counts.
- LTL list case keys of several or str items (probes q4, q5, 514 → 0):
  one List literal in the select and group_by, the items in the joins and
  the window; four_eyes' first-join stack by pair count and sign (the empty
  list key's stack too); four or more four_eyes activities with a literal
  key fail at the third pair, (0, 3); `eventually_follows` with at most
  one activity joins on the named columns.
- Fresh seeds (all 0): Polars table CK 2026111611 (4 calls fixed), CN
  2026111612, CJ 2026111613, CI 2026111614, CH 2026111615 (case 580's
  four_eyes join unordered); Polars sequence CP 2026111610 (1 call: a
  positional struct row), CK 2026111616, CJ 2026111617, CH 2026111618;
  pandas CK table 2026111619 (2 calls: business-hours ints, lexicographic
  list comparisons), CK sequence 2026111620, CN LTL 2026111621.
- CK1-1: two calls pass since earlier rounds; CB sequence case 431 is a
  join with no fixed row order in the reference.

In the suite: new `docs/polars-filter-cq-bank.json.gz` (22,231 calls),
`docs/polars-filter-cq-probe-bank.json.gz` (558),
`docs/polars-sequence-filter-cq-probe-bank.json.gz` (1,420) and
`docs/pandas-filter-cq-bank.json.gz` (11,712); the CJ docs bank's case 71
is observed. The fresh banks, probe specs and replay logs are in the
project files under `review-harness/round-cq/`.

No fixture changed. The device check on the Mac was skipped again (no
device linked).

Round CR re-reviewed CQ (Review CR1). CQ's commit 4910224 ran green on
all six CI jobs (run 43726703592). CR replayed fresh seeds and swept four
upload routes that earlier rounds had not swept on a typed frame:
- Label splitting (`label_splitting.cpp`) reads the frame's typed cells:
  context tokens keyed as a Python dict keys them (one NaT, a fresh NaN per
  float64 cell), the modularity queue's tie TypeError, relabelling in
  global community order, and `label + suffix` raising for a label that
  is no str; an empty suffix is accepted.
- The prefix tree (`trie.cpp`) checks the key column with pm4py's
  messages and reads typed keys; `max_path_length` 0 gives a bare root.
- The interval tree (`interval_tree.cpp`) sorts intervals with their
  events as the last key, so equal intervals of unequal XES events raise
  TypeError; NaT and non-date timestamps raise pm4py's errors.
- Link analysis (`link_analysis.cpp`) merges the frame on typed keys under
  pandas' merge checks (int64 with str fails, NaN matches NaN), numbers the
  rows in numpy's quicksort order of the sorting column (equal cells out of
  row order from 17 rows; the reference sweep runs numpy's portable sort)
  and reads XES rows in file order. Sweeps: 0 differing calls.
- Fresh seeds: Polars table CK 2026111631 (6 calls), CN 2026111632, CJ
  2026111633, CI 2026111634, CH 2026111635; Polars sequence CP 2026111630,
  CK 2026111636 (1), CJ 2026111637, CH 2026111638; pandas CK table
  2026111639 (1), CK sequence 2026111640, CN LTL 2026111641. A second batch
  after the fixes (Polars table CK 2026111651 and CN 2026111654, sequence
  CK 2026111652, pandas CK 2026111653) found 5 more calls. All 0 after.

In the suite: new `docs/polars-filter-cr-bank.json.gz`,
`docs/polars-sequence-filter-cr-bank.json.gz` and
`docs/pandas-filter-cr-bank.json.gz` (the CK seeds with the second batch's
fixed cases) and `minera_review_cr1_tests` (link analysis and label
splitting on a CSV upload, pm4py values). The fresh banks, sweeps and
probes are in the project files under `review-harness/round-cr/`.

No fixture changed. The device check on the Mac was skipped again (no
device linked).

Round CS re-reviewed CR (Review CS1). CR's commit 251318f ran green on
all six CI jobs (run 43740955829). CS modelled the one-row
`A_next_B_next_C` keys CR had declared, replayed two batches of fresh
seeds and swept trace-attribute clustering on uploads:
- One-row `A_next_B_next_C` (`polars_ltl_filter.cpp`) with list, tuple,
  mask and None case keys: the literal window and sort, then Python's
  indexing of the collected one-row frame (pairs, masks, rows, names,
  repeated names) and the result's failures (probes n1, n2, ln2 to ln6;
  2,383 calls bound).
- Literal list case keys in the Polars variants filters
  (`polars_variants_filter.cpp`): one literal row whatever the frame holds,
  keep-all-or-nothing, unhashable dict/list items on one row, DuplicateError
  while resolving the group_by or join. `eventually_follows` on literal
  keys resolves the first three activity filters before the third join's
  duplicate; two activities on one row keep one outcome.
- Trace-attribute clustering (`clustering.cpp`) on CSV and XES frames: each
  trace's typed `case:` values from the case's first row, Python dict keys
  as groups (a fresh NaN per float64 cell, one NaN elsewhere, one NaT),
  repeated leaf names, export by leaf index. Sweeps of 60 seeds each: 0
  differing calls.
- Fresh seeds, first batch: Polars table CK 2026111671 (5 calls), CN
  2026111672, CJ 2026111673 (1: a struct-cast race, observed), CI
  2026111674, CH 2026111675; Polars sequence CP 2026111670, CK 2026111676
  (1: an equal-size join, observed), CJ 2026111677, CH 2026111678; pandas
  CK table 2026111679 (5), CK sequence 2026111680 (1), CN LTL 2026111681.
  Second batch after the fixes: Polars table CK 2026111691 (3: folded int
  and datetime case ranges, narrow path dtypes, list timestamp keys in the
  variants sort) and CN 2026111694, sequence CK 2026111692 and CP
  2026111696, pandas CK table 2026111693 (2: list-like values against
  `df[[]]`, list timestamp keys hashed item by item) and CK sequence
  2026111695. All 0 after.

In the suite: new `docs/polars-filter-cs-bank.json.gz`,
`docs/polars-filter-cs-probe-bank.json.gz`,
`docs/polars-sequence-filter-cs-bank.json.gz`,
`docs/pandas-filter-cs-bank.json.gz` and
`docs/pandas-sequence-filter-cs-bank.json.gz`, and
`minera_review_cs1_tests` (trace-attribute clustering on a CSV upload,
pm4py values). The fresh banks, sweeps and probes are in the project
files under `review-harness/round-cs/`.

No fixture changed. The device check on the Mac was skipped again (no
device linked).

Round CT re-reviewed CS (Review CT1). CS's commit 6514e53 ran green on
all six CI jobs (run 43754352187). CT probed range folding across the
Polars filters, rebuilt the process cube's binning on pm4py's, swept
SaCoFa on uploads and replayed two batches of fresh seeds:
- Range folding (probes f1, f2; 2,824 calls): the Polars range filters
  already folded reversed and equal bounds as the reference does. Two
  timestamp filter details were fixed (`polars_timestamp_filter.cpp`): a
  Float64 timestamp group's min and max skip NaN unless every cell is NaN,
  and `filter_traces_attribute_in_timeframe` on the timestamp column drops
  "while evaluating this larger expression" only on Int64 and Float64
  columns.
- The process cube (`process_cube.cpp`, `/api/process-cube`,
  `minera_process_cube`) now bins numeric and prefix dimensions as
  `statistics.process_cube` does: `pd.cut(include_lowest=True)` labels and
  edges, a constant column as one bin, sorted unique explicit edges,
  prefix columns in the table's column order, empty cubes that never read
  the aggregation column, Kahan sums, 0 to 10,000 divisions. Sweep of
  1,800 calls: 0 differing. Categorical and composite dimensions stay
  Minera extensions (declared).
- SaCoFa on CSV and XES uploads (560 calls) matched. PRIPEL and the
  embeddings route need packages the pinned reference lacks (declared).
- pandas `apply_numeric` with a list stream key masks the rows
  (`DataFrame.where`): list-like values align by position or key, length
  and set errors, NaN case ids matching every missing case id
  (`pandas_attributes_filter.cpp`, probe p1, 1,917 calls).
- Polars sequence: selector counts in the consecutive grouping (probe s1,
  1,176 calls); `rework.apply` on one column as both keys raises
  DuplicateError.
- Literal case keys in `eventually_follows` and the variants filters
  (`polars_ltl_filter.cpp`, `polars_variants_filter.cpp`; probes l1, l2,
  870 calls): literal sorts and windows, None keys as one literal group in
  the statistics, plan resolution order before the literal ShapeError.
- One-row NaN joins (`polars_group_keys.hpp` and the case, attribute,
  timestamp, variants and sequence filters; probes j1, s2, 578 calls): a
  one-row frame joined with a one-row right side on a single float key
  never matches a NaN key.
- Fresh seeds, first batch: Polars table CK 2026111701 (2 calls), CN
  2026111702, CJ 2026111703, CI 2026111704, CH 2026111705; Polars
  sequence CP 2026111700 (1), CK 2026111706, CJ 2026111707, CH 2026111708;
  pandas CK table 2026111709 (1), CK sequence 2026111710, CN LTL
  2026111711. Second batch after the fixes: sequence CP 2026111712 (1:
  one-row NaN joins), table CK 2026111713 (1: variants join resolution
  order), CN 2026111714, CJ 2026111715, CI 2026111716, CH 2026111717;
  sequence CK 2026111718, CJ 2026111719, CH 2026111720; pandas CK table
  2026111721 (1: a dict bound against an empty attribute key), CK sequence
  2026111722, CN LTL 2026111723. All 0 after.

In the suite: new `docs/polars-filter-ct-bank.json.gz`,
`docs/polars-filter-ct-probe-bank.json.gz`,
`docs/polars-sequence-filter-ct-bank.json.gz` and
`docs/pandas-filter-ct-bank.json.gz`, and `minera_process_cube_tests`
rewritten on pm4py's values. The fresh banks, sweeps and probes are in the
project files under `review-harness/round-ct/`.

No fixture changed. The device check on the Mac was skipped again (no device
linked).

Round CU re-reviewed CT (Review CU1). CT's commit ae025ec ran green on
all six CI jobs (run 43770672377). CU swept the transform-target route on
uploads, probed the one-row join shapes and list keys CT left open and
replayed two batches of fresh seeds:
- Transform targets on uploads (`log_to_target.cpp`, `request_params.cpp`,
  `literal_controls_json.cpp`, `json_export.cpp`, the frontend's result
  check; 708 sweep calls, 0 differing): activities and timestamps come from
  the frame's typed cells, derived classes are `sorted(set(...))` with
  Python's ordering and errors, an explicit list still reads every event,
  the first equal class takes the one-hot, padding pads only shorter cases
  (a pad size of 0 means the longest case unless given), an empty log
  raises `max()`, and the JSON carries 17 digits.
- List case keys in the Polars paths filters (`polars_paths_filter.cpp`;
  probes j2, u2, 1,376 calls): the list literal sorts first (ShapeError on
  frames of more than one row), then the names resolve after the sort, two
  distinct names alias `case_next` twice. A repeated existing name stays
  refused (declared).
- `filter_variants_top_k` with several key names on a one-row frame
  (`polars_variants_filter.cpp`): the second key value is the variant, a
  str iterating to characters, any other value raising "not iterable".
- The rework build order with dict keys (`polars_sequence_filters.cpp`,
  fresh CK 2026111806, probe u3): the dict case and activity literals, then
  `MIN_OCCURRENCES`, then the select's "Cannot pass a dictionary".
- `filter_df_on_end_activities` with a list or tuple `case_id_glue`
  (probe u3, 720 calls with the rework calls): grouped and joined as the
  `CASE_ID_KEY` parameter of `end_activities.apply`.
- The positive variants joins with an int or one-item literal list case
  key (`polars_variants_filter.cpp`, fresh CK 2026111813 case 490, probe
  u4): the inner join keeps the literal as a `literal` column.
- List case keys of mixed items in the grouped pandas timestamp filters
  (`pandas_timestamp_filter.cpp`, fresh pandas CK 2026111821 case 690,
  probe u5): groupby reads the items in order (a dict maps, a missing label
  raises `KeyError`, a list of another length the grouper length error,
  None "'NoneType' object is not callable").
- One-row NaN joins in LTL, paths, between and `ncases`, two float keys and
  an Int/Float pair (probe j2) already matched.
- Fresh seeds, first batch: Polars table CK 2026111801, CN 2026111802, CJ
  2026111803, CI 2026111804 (1: an equal-size timeframe join in random
  order, marked unordered), CH 2026111805; Polars sequence CP 2026111800,
  CK 2026111806 (2: the rework build order), CJ 2026111807, CH 2026111808;
  pandas CK table 2026111809, CK sequence 2026111810, CN LTL 2026111811.
  Second batch after the fixes: sequence CP 2026111812 (1: an equal-size
  `starts_with` join, marked unordered), table CK 2026111813 (1: the
  variants literal column), CN 2026111814, CJ 2026111815, CI 2026111816,
  CH 2026111817; sequence CK 2026111818, CJ 2026111819, CH 2026111820;
  pandas CK table 2026111821 (1: mixed list keys), CK sequence 2026111822,
  CN LTL 2026111823. All 0 after.

In the suite: new `docs/polars-filter-cu-bank.json.gz`,
`docs/polars-filter-cu-probe-bank.json.gz`,
`docs/polars-sequence-filter-cu-bank.json.gz` and
`docs/pandas-filter-cu-bank.json.gz`, upload-frame cases in
`minera_log_to_target_tests` and partial padding in the transformation
workspace test. The fresh banks, sweeps and probes are in the
project files under `review-harness/round-cu/`.

No fixture changed. The device check on the Mac was skipped again (no device
linked).

Round CV re-reviewed CU (Review CV1). CU's commit a614862 ran green on
all six CI jobs (run 43784803653). CV moved the concept-drift route onto
pm4py's DataFrame adapter for uploads, followed the fresh seeds into three
filters and replayed two batches of fresh seeds:
- Concept drift on uploads (`concept_drift_table.cpp`
  `detect_concept_drift_table`, `csv_frame_table.hpp`
  `formatted_csv_frame_table`/`formatted_xes_frame_table`,
  `request_params.cpp`; 1,200 sweep calls, 0 differing): BOSE runs on the
  upload's frame (typed columns, text copies of the case and activity,
  NaN gaps), cases sorted by the frame's case text, change times from the
  selected case groups, pm4py's errors (`KeyError` order, `.timestamp()`,
  `ZeroDivisionError` for 0 permutations, `str.join` of non-str
  activities). The frame path polls the client's disconnect. The
  reference sweeps need `OPENBLAS_CORETYPE=Haswell` (permutation ties
  follow the OpenBLAS kernel).
- Relative occurrence (`pandas_attributes_filter.cpp`, fresh pandas CK
  2026111909, probe v1): the per-case count reads the keep-once parameter
  given last, the denominator the enum first; a list case key is a grouper
  array (length must match), a tuple a label that must hash, a list
  denominator key a Series without a truth value.
- `A_next_B_next_C` dict-key message with a datetime
  (`polars_ltl_filter.cpp`, fresh CK 2026111901 case 561).
- Case-performance key pairs through numpy (`pandas_case_filter.cpp`,
  fresh pandas CK 2026111921 case 460).
- Repeated keys of a generated dict argument are `dict(pairs)` in the
  Polars table bank replay (fresh CK 2026111913 case 509).
- Fresh seeds: first batch 2026111900-11 (4 calls, 0 after), second batch
  2026111912-23 (5 calls; 3 carried: CK 2026111913 cases 117, 410, 650).

In the suite: new `docs/polars-filter-cv-bank.json.gz`,
`docs/pandas-filter-cv-bank.json.gz` and
`minera_server_concept_drift_upload_tests`. The fresh banks, sweeps and
probes are in the project files under `review-harness/round-cv/`.

No fixture changed. The device check on the Mac was skipped again (no device
linked).

Round CW re-reviewed CV (Review CW1). CV's commit c3b64c0 ran green on
all six CI jobs (run 43805250373). CW fixed the three calls CV carried,
made every bank decoder build repeated dict keys as Python does, moved
four upload routes onto the frame and replayed a batch of fresh seeds:
- `eventually_follows` cached plan (`polars_ltl_filter.cpp`,
  `polars_ltl_cse_model.hpp`; fresh CK 2026111913 case 117, probes w1-w4):
  the activity filters and sorts run in the order of the reference's
  common-subplan caches (filled first, depth first); a filter sits below
  its own sort when the attribute is the case column; equal literals
  share a plan as Polars coerced them (aware datetimes as one UTC
  literal); a None activity keeps the whole frame's sort.
- Timeframe windows folded into the equality
  (`polars_timestamp_filter.cpp`; cases 410 and 650, probes w5 and w6):
  an attribute equality on the timestamp column itself takes the window
  as bounds on its literal (inside: the equality alone; outside: empty;
  not comparable: dropped); struct casts, dynamic literals, None and cast
  time columns (Date included) keep the predicate; aware non-UTC time
  columns fail as naive ones.
- `dict(pairs)` in all four bank decoders (`tests/bank_dict_pairs.hpp`);
  every docs bank replays at 0.
- Rework, boundary activities and cycle time on uploads (`rework.cpp`
  `discover_rework_as_uploaded`, `boundary_activities.cpp`,
  `cycle_time.cpp`, server and CLI): pm4py's DataFrame checks and errors,
  the case column's groups, the start key defaulting to the timestamp key
  and the set of the two columns. Case arrival and passed time were
  already equal. Sweeps: 1,640 calls in the final runs on CSV and XES
  uploads, all equal (one passed-time message in hash-seed order,
  declared).
- Fresh seeds 2026112000-11: 0 calls differing.
- Chaotic activities on uploads (`discover_chaotic_activities_as_uploaded`,
  server and CLI; 320 sweep calls equal after up to ties): the frame's
  typed column (`KeyError` when missing, Python's `<` order and its
  `TypeError`, each NaN cell its own activity with gain 0). The entropy's
  last bits and near-tie order follow the hash order of the reference's
  activity set (168 of 400 string logs differ between hash seeds 0 and
  1): declared.

In the suite: new `docs/polars-filter-cw-probe-bank.json.gz` and
`minera_server_upload_frame_routes_tests` (rework, boundary, cycle time and
chaotic activities on uploads). The fresh banks, sweeps, probes and the
chaotic-activity fits are in the project files under
`review-harness/round-cw/`.

No fixture changed. The device check on the Mac was skipped again (no device
linked).

Round CX re-reviewed CW (Review CX1). CW's commit ae4ae5bc ran green on all six CI jobs
(run 43821403493). CX replayed a batch of fresh seeds and swept three
upload routes against pm4py on the upload's frame:
- Log util on uploads (`log_util.cpp`, `log_util_json.cpp`, server and
  CLI): until-activity and to-activities read the frame's typed cells
  (Python equality for the activity; the elapsed value is the difference
  of the typed cells: seconds for dates, an int for ints, null for NaN or
  NaT, `TypeError` for text, `KeyError` for a missing column), fixed
  prefixes drop the cases left empty; `elapsed_seconds` may carry null
  and negative values (frontend validator fixed). Sweep: 145 of 960 calls
  differed before, 0 of 960 after.
- Timestamp interleavings on uploads (`timestamp_interleavings.cpp`
  `naive_as_utc`, `case_relations.cpp` `type_case_relations_like_logs`,
  server and CLI): relation ids join as str on both sides (numeric
  relation ids raise, as pandas' merge does) and naive CSV timestamps
  count as UTC, as `format_dataframe` makes them. Sweep: 31 of 200 before,
  0 after. Two BB CLI ctests now expect the error.
- Organizational SNA on uploads: 640 calls, 4 differing, all the
  subcontracting pandas variant with NaN resource cells, whose variant
  order in pm4py is no total order. Minera now sorts NaN above every
  value (the closest order found: 1,342 of 1,500 random frames); declared.
- Fresh seeds 2026112100-11 (12 banks): three calls; the `pl.lit`
  Date/Datetime + UInt64 supertype (Int64) and the trace filter's
  not-iterable scalar trace fixed, four_eyes' equal-size join order marked
  unordered (it varies between runs of the reference).

In the suite: new `docs/polars-filter-cx-bank.json.gz` and
`docs/pandas-sequence-filter-cx-bank.json.gz` with their ctests, and new
cases in the log util, timestamp interleavings, core and organizational
mining tests. The fresh banks, sweeps and the subcontracting study are in
the project files under `review-harness/round-cx/`.

No fixture changed. The device check on the Mac was skipped again (no device
linked).

Round CY re-reviewed CX (Review CY1). CX's commit 26aaabb ran CI run
43835939800 (green on all six jobs). CY replayed a batch of fresh seeds and
swept the case-relations merge on uploads against pm4py:
- CSV uploads (`pandas_read_csv.hpp` `read_csv_reference_table`, server
  and CLI): the three files are read as `pandas.read_csv` reads them
  (every row, `read_csv` dtypes, timestamps as text, blank lines skipped,
  short rows NaN-padded) and merged by the typed-table merge, so dtypes,
  promotions (`7.0` for an int case beside a float LEFT column), merge
  errors, index and order follow pandas. Sweep: 54 of 60 calls differed
  before, 0 of 900 after.
- XES uploads: the relation file is a `read_csv` frame (numeric ids raise
  against XES's str ids, as pandas' merge does), an XES log without
  events is left out of the concat, and a timestamp column suffixed away
  raises a clear error. Sweep: 27 of 60 before, 0 of 300 after.
- A CSV log beside an XES log keeps Minera's alias adapter (pm4py
  concatenates frames sharing no column names); declared.
- Fresh seeds 2026112200-11 (12 banks): two pandas CK calls, fixed
  (relative occurrence per event on an all-NaN attribute never reads the
  case key, a `None` case key fails per case as `groupby(None)`; a list
  activity key in the variants filter hashes its items and names the
  missing columns); probe y1 equal but for two hash-seed `KeyError`
  orders (declared).

In the suite: new `docs/pandas-filter-cy-bank.json.gz` with its ctest and
new `read_csv` table and merge cases in `minera_case_relations_tests`. The
fresh banks, the merge sweep and probe y1 are in the project files under
`review-harness/round-cy/`.

No fixture changed. The device check on the Mac was skipped again (no device
linked).

Round CZ re-reviewed CY (Review CZ1). CY's commit 5d0a5dc2 ran CI run
43849161728 (green on all six jobs). CZ replayed two batches of fresh
seeds, took the timestamp interleavings route (the last upload route whose
pm4py call reads a file with `read_csv`) and carried Python sets through
the pandas bank codec:
- Timestamp interleavings on uploads (`upload_case_relations.hpp`, server
  and CLI): the relation file is a `read_csv` frame joined to the logs'
  str-keyed frames by pandas' two merges, so a str or object column meets
  cases by its str cells, a number column raises the merge `ValueError`
  (skipped when exactly one merged frame is empty), and overlapping,
  repeated or suffix-colliding names raise pandas' errors. Sweep: 26 of
  150 calls differed before, 0 of 1,050 after.
- Sets and frozensets in the pandas table filters (probes z1-z3, 4,404
  calls): a str or datetime64 column compares a set target like a dict
  (pandas' length and 0-d errors), a two-column selection cannot coerce
  one, and a set case key in `variants.apply` is called by `groupby`.
- Fresh seeds 2026112300-11 equal; 2026112312-23 found four calls, fixed:
  a null number field cast to a one-field struct is `{null}` (Polars CJ),
  repeated predicates are dropped by value, not display (Polars CI),
  `activity_split`'s resolved scalar `is_in` (Polars sequence CP), and a
  kept `None` alone matches no NaT on a datetime64 column (pandas
  sequence CK).
- Declared: sets in the sequence filters' codecs, groupby lists mixing a
  set with other keys, and again the two CU plan cases (paths repeated
  list key names, `filter_variants_top_k` one-row second columns; pm4py's
  `DuplicateError` carries the whole plan).

In the suite: new `docs/pandas-filter-cz-bank.json.gz`,
`docs/polars-filter-cz-bank.json.gz`,
`docs/polars-sequence-filter-cz-bank.json.gz` and
`docs/pandas-sequence-filter-cz-bank.json.gz` with their ctests, new
interleavings cases in `minera_case_relations_tests`, and the two
interleavings CLI ctests now expect pandas' merge `ValueError`. The fresh
banks, the interleavings sweep and probes z1-z3 are in the project files
under `review-harness/round-cz/`.

No fixture changed. The device check on the Mac was skipped again (no device
linked).

Round DA re-reviewed CZ (Review DA1). CZ's commit d64f45e ran CI run
43864008901 (green on all six jobs). DA replayed two batches of fresh
seeds, gave the sequence filters' value model a set kind and took the
paths list case keys repeating a name:
- Sets and frozensets in the pandas sequence filters
  (`PandasFilterValue::Kind::kSet`, `S` decoded as a set in both sequence
  drivers): `isin` lists a set, `in` hashes it, columns compare it like a
  dict, `groupby` calls it on the labels, indexers and hashes refuse it,
  a set carrier cannot be indexed. Probes set-pdseq-1/-2: 2,288 of 13,544
  calls differed before, 0 after. Also: `starts_with`, `ends_with` and
  `traces` read the plain case and activity keys before the enum member.
- Sets in the Polars filters, first pass: set keys through `pl.col` like
  lists, refused where pm4py hands them to `pl.lit`; set carriers; `nocc`
  against non-numbers; set comparison operands and `head(n)` bounds.
  Probes: Polars sequence 649 -> 346 of 6,603 (126 of them the keyword
  helpers, which take str keys natively), Polars table 2,041 -> 1,996 of
  16,185; the matching calls are bound.
- Paths filter list case keys repeating a name: pm4py's `DuplicateError`
  with the whole filter plan, modelled (`repeated_case_name`); probes d1/d2
  3,164 calls, 0 differing.
- Fresh seeds 2026112400-11 found three calls (Polars CK timeframe window
  before the case key, pandas CK `set_index` array checks on the whole
  frame then the filtered rows, pandas sequence CK `pd.NA == None` False),
  2026112412-23 one (Polars CK two literal group keys named `literal`);
  all fixed.
- Declared: lists holding a set in `pl.lit`/`pl.Series`, set comparison
  bounds and timestamp keys in the Polars filters (the rest of the two
  Polars set probes), the keyword helpers' set keys, and
  `filter_variants_top_k` one-row second columns.

In the suite: new `docs/pandas-sequence-filter-da-bank.json.gz`,
`docs/pandas-filter-da-bank.json.gz`, `docs/polars-filter-da-bank.json.gz`,
`docs/polars-filter-da-set-bank.json.gz` and
`docs/polars-sequence-filter-da-bank.json.gz` with their ctests. The
probes, set domains and fresh banks are in the project files under
`review-harness/round-da/`.

No fixture changed. The device check on the Mac was skipped again (no device
linked).

Round DB re-reviewed DA (Review DB1). DA's commit e717be3 ran CI run
43878323668 (green on all six jobs). DB replayed two batches of fresh
seeds, finished the Polars set probes and ran fresh set probes on four
other domains:
- Sets as Polars literals (`LitType::K::kObject`): a set item of a list
  `pl.lit`/`pl.Series` builds is an Object value (`Nested object types`,
  `nested objects are not allowed`, the supertype pairs with `object`,
  the String builder's `found value of type Object: {...}`); a whole set
  is refused by `pl.lit` at every literal site (bounds, stream and
  timeframe values, separators, rework/four-eyes/different-persons keys,
  set items of group keys, per-case `group_by([case, set])`, sort keys).
  A datetime literal column holds the typed instant.
- Set keys as Polars column selectors: `pl.col`, `group_by` and `sort`
  iterate a set like a list (one name a column, a missing one by name,
  the item checks); the attribute filters model set selectors and refuse
  several names after the literals; `get_attribute_values` and the
  relative occurrence filter group by the items in order (a one-tuple
  item `unhashable type: 'list'`; several items pair the first key with
  the second against the stake, then `pl.col(set)`); per case on an empty
  frame a literal key still forms its group; `df[set]` in spno/A_next;
  `get_variants_df` overloads for set keys. Probes: Polars table 1,996 ->
  10 of 16,185, Polars sequence 346 -> 65 of 6,603 (62 of them the
  keyword helpers).
- Fresh set probes: Polars CK table 19 -> 10 of 16,419, Polars CP
  sequence 36 -> 27 of 4,432 (24 keyword-helper calls), pandas CK table
  92 -> 0 of 8,785 (a set against a pandas `string` column compares as a
  dict, `groupby` calls a set case key in the relative, timeframe and
  variants filters, a set coverage share cannot multiply), pandas CN LTL
  0 of 1,800.
- Fresh seeds 2026112500-11 and 2026112512-23 found one call each, both
  declared (Polars CJ 2026112503 case 677: two stream predicates printing
  alike rebuilt in another order; pandas CK 2026112521 case 435: a dict
  plain attribute key with an empty-tuple member on an empty frame).
- Declared: a set's iteration order where it shows (the reference
  rebuilds the set in its worker; 17 probe calls), several-name set keys,
  the keyword helpers' set keys, and four one-call shapes (Review DB1).

In the suite: new `docs/polars-filter-db-set-bank.json.gz`,
`docs/polars-filter-db-set-ck-bank.json.gz`,
`docs/polars-sequence-filter-db-set-bank.json.gz`,
`docs/polars-sequence-filter-db-set-cp-bank.json.gz`,
`docs/pandas-filter-db-set-ck-bank.json.gz` and
`docs/pandas-filter-db-set-ltl-bank.json.gz` with their ctests. The
probes, set domains and fresh banks are in the project files under
`review-harness/round-db/`.

No fixture changed. The device check on the Mac was skipped again (no device
linked).

Round DC re-reviewed DB (Review DC1). DB's commit ed683a4 ran CI run
43898017238 (green on all six jobs). DC ran set probes on the six domains
DB had not probed with sets, removed the set-order artefact in the
harness and replayed two batches of fresh seeds:
- Sets listed in the order they iterate (harness `dom/setcommon.py`):
  the main process lists each set in an achievable iteration order and
  every worker copy of the set codec inserts the items so the set
  iterates as listed. DB's four Polars set probes recaptured: 2, 1, 0
  and 1 differing calls (all declared), and the four DB banks replaced
  by the recaptures.
- Set, list and tuple keys in `get_end_activities` and
  `filter_df_on_end_activities_nocc` (`PolarsFilterValue` overloads);
  several plain keys in `get_attribute_values` (the dict of first to
  second key values; a first value with several seconds is refused).
- Predicates over several columns: a set or list naming several
  existing columns in `apply`, `apply_events`, `apply_numeric` and
  `apply_numeric_events` fails `expanded to multiple expressions` with
  each predicate as written; spno set keys repeat `literal` in order;
  a paths target naming several columns aliases `next_activity` twice.
- `eventually_follows` builds every activity's literal before indexing
  the values; on a lazy frame one None activity with the anti join keeps
  the sorted order. `A_next_B_next_C` on an empty frame with a set key
  fails the join; `get_variants_df` with an empty set key selects no
  column.
- Fresh set probes: Polars CN table 0 of 936, CI table 13 -> 3 of 13,701,
  CH table 8 -> 3 of 12,014, CK sequence 36 -> 0 of 4,326, CJ sequence
  21 -> 0 of 3,861, pandas CK sequence 0 of 3,430.
- Fresh seeds 2026112600-11: four calls, three fixed (the timestamp
  filters' by-name selectors before zero expansion; pandas empty-list
  attribute keys with stream filters and null cases), one declared (a
  rework join of equal-size frames in random order).
  Fresh seeds 2026112612-23: 0 differing calls.
- Declared: sets holding NaN (identity hash order), the random group a
  several-key `get_attribute_values` keeps, one-item set case keys
  holding a tuple (List cells), dicts holding a set (struct with an
  Object field), `filter_variants_top_k` on one-row frames with List,
  Struct, Binary, Decimal, Duration or Time second columns, equal-size
  rework joins, several attribute names beside a single paths target.

In the suite: new `docs/polars-filter-dc-set-bank.json.gz`,
`docs/polars-sequence-filter-dc-set-bank.json.gz` and
`docs/pandas-sequence-filter-dc-set-bank.json.gz` with their ctests;
DB's four Polars set banks replaced by their recaptures. The probes, set
domains and fresh banks are in the project files under
`review-harness/round-dc/`.

No fixture changed. The device check on the Mac was skipped again (no device
linked).

Round DD re-reviewed DC (Review DD1). DC's commit da99db9 ran CI run
43913617519 (green on all six jobs). DD ran set probes on the five
domains DC had not probed with sets, modelled the several-column shapes
DC left refused and most of the LTL list-key cases carried since CQ, and
replayed two batches of fresh seeds:
- Several columns: `filter_df_on_attribute_values` and the stream keys
  select a list, tuple or set of names as `pl.col` does (one name that
  column, a repeated one once, a missing one by name, several distinct
  existing names expand the predicate); the refusal lists every
  predicate with its literal as Python wrote it, any bound type.
- Several attribute names beside a single paths target: the label
  concatenates the String casts of the distinct names (null when any is
  null), then `,` and the next row's target.
- Dicts holding a set (a struct with an Object field): the plan prints
  the struct uncast and collect panics (`nested objects are not
  allowed`), so earlier plan errors fail as the reference does (probe
  set-ddsc-5: 2,527 -> 0 of 4,166 calls).
- The timestamp attribute filter checks its join keys after the case
  selection resolved; the threshold filter on an empty frame with a key
  list holding a dict returns the frame.
- LTL list case keys: the first name the join or group_by input lacks
  is the missing one, wherever it stands; a four_eyes key of only the
  resource misses `<resource>_2`; different_persons keys of only the
  resource group by its values, `count` repeats the count column, a
  repeated resource a group_by key; None beside str items joins on the
  str items, None beside a present name never joins (empty frame with
  `literal` and `<name>_right`); eventually_follows over the timestamp
  alone fails `ShapeError`; timestamp keys that are no str sort as a
  literal (two or more positive activities repeat `literal`). Probes
  lt1 256 -> 40 and lt2 600 -> 440 (captured after the lt1 model), the
  rest native refusals.
- Fresh set probes (seed 4): Polars CJ table 3 -> 1 of 16,205, CF table
  2 of 11,405 (both declared), CG table 0 of 1,911, CH sequence 0 of
  3,906, pandas LTL 0 of 4,800.
- Fresh seeds 2026112700-11: two calls, one fixed (the threshold filter
  above), one declared (rework's equal-size join order).
  Fresh seeds 2026112712-23: two calls, both declared (equal-size joins
  in rework and four_eyes_principle).
- Declared: as in DC, plus `eventually_follows` without activities over
  a composite key unique per row (the equal-size join order).

In the suite: new `docs/polars-filter-dd-set-bank.json.gz`,
`docs/polars-sequence-filter-dd-set-bank.json.gz`,
`docs/pandas-filter-dd-set-ltl-bank.json.gz` and
`docs/polars-filter-dd-ltl-bank.json.gz` with their ctests. The probes,
set domains and fresh banks are in the project files under
`review-harness/round-dd/`.

No fixture changed. The device check on the Mac was skipped again (no device
linked).

Round DE re-reviewed DD (Review DE1). DD's commit 92fcffb ran CI run
43929965710 (green on all six jobs). DE took the LTL list-key shapes DD
left refused, ran second-seed set probes on the CJ, CI and CH table
domains, modelled what they found and replayed two batches of fresh
seeds:
- LTL list case keys: one None beside several present names in
  `eventually_follows` (no join up to one activity, a repeated
  `<first name>_right` from two), literal keys and timestamp keys that
  are no str repeating `literal`, None as the timestamp key dropping out
  of the sort; `four_eyes_principle` with a repeated resource (`already
  joined on R and R`) and None beside the resource from three
  activities; `attr_value_different_persons` items scanned in order.
  Probes lt1 40 -> 16, lt2 440 -> 6, lt3 3,752 -> 21, lt4 828 -> 11,
  lt5 965 -> 311 (306 refusals of a str item named `literal`, one key
  with two Nones beside the resource; the rest the declared join order).
- Timestamp filters with keys naming several columns: the events and
  attribute filters fail with the expanded predicates as Python wrote
  them (pairs of several-column keys, or `cannot combine selectors`),
  the trace filters repeat `first_timestamp`; an empty case key list
  resolves the aggregation and filter before `expected join keys`; an
  empty timestamp selector names the case keys' output columns. Probe
  ts6 705 -> 4 of 2,000 (declared order). Case performance: the same
  for `end_time` with set case keys, `start_time` repeated for several
  timestamp names.
- Smaller: PyO3's short type names for date and datetime items in
  `pl.col` lists, four_eyes refusing a set resource key at the first
  activity's select, `get_attribute_values` with a list or tuple first
  key (`unhashable type: 'list'` once a group exists).
- Set probes (seed 6): CJ table 4 -> 1 of 16,150, CI 6 -> 2 of 13,687,
  CH 4 -> 4 of 11,993, all seven declared (several-key value pairs, NaN
  set order, List cells).
- Fresh seeds 2026112800-11 and 2026112812-23: 0 differing calls.

In the suite: new `docs/polars-filter-de-ltl-bank.json.gz` and
`docs/polars-filter-de-set-bank.json.gz` with their ctests. The probes,
set domains and fresh banks are in the project files under
`review-harness/round-de/`.

No fixture changed. The device check on the Mac was skipped again (no device
linked).

Round DF re-reviewed DE (Review DF1). DE's commit 5ba9a67 ran CI run
43943728731 (green on all six jobs). DF took the LTL list-key shapes DE left
refused, probed the four_eyes check order and Python ints past the Int64
range, ran second-seed set probes on the CK and CN table and the CP, CK
and CJ sequence domains and replayed two batches of fresh seeds:
- LTL list case keys with a str item named `literal` (the key's own List
  literal column): four_eyes fails the final join back to the frame for
  `literal` alone, misses `R_2` beside the resource and fails `already
  joined on X and X` for any repeated output name (two Nones included);
  attr_value_different_persons repeats `count` or fails its final join.
  lt5 311 -> 4 of 3,600 (declared order), new lt6 0 of 2,800. Still
  refused: a `literal` item when the frame has a `literal` column, and
  eager calls.
- four_eyes check order: the first pair's join runs after activity 1's
  literal and before activity 2's (None or dict case key, then the
  resource suffix: `invalid input for col`, tuple/list concatenation,
  `unsupported operand type(s) for +: 'dict' and 'str'`); with list case
  keys an activity filter that fails to resolve comes first when the
  failing join resolved it (first two activities, first four, or all for
  the final join). Probe fe2 66 -> 0 of 900.
- Python ints outside [-2**127, 2**127) fail `integer too large for
  Polars` in the numeric attribute, case performance and path
  performance bounds; Int128-range scalars and `is_in` lists past UInt64
  are refused natively (probe i1 534 -> 409, all refusals).
- Set probes (seed 7): CK table 2 -> 1 of 16,512 (declared several-key
  value pairs), CN table, CP, CK and CJ sequence 0.
- Fresh seeds 2026112900-11: one call (four_eyes with a dict case key),
  fixed; 2026112912-23: three calls the reference itself answers
  differently from run to run (two equal-size join orders, one struct
  cast whose row partitions race: the cast error 25 of 30 runs), declared.

In the suite: new `docs/polars-filter-df-ltl-bank.json.gz`,
`docs/polars-filter-df-set-bank.json.gz` and
`docs/polars-sequence-filter-df-set-bank.json.gz` with their ctests. The
probes, set domains and fresh banks are in the project files under
`review-harness/round-df/`.

No fixture changed. The device check on the Mac was skipped again (no device
linked).

Round DG re-reviewed DF (Review DG1). DF's commit 10ebbb7 ran CI run
43955891315 (green on all six jobs). DG modelled the Python ints DF still
refused, widened them to five table domains, ran third-seed set probes on
the CJ, CI and CH table domains and replayed two batches of fresh seeds:
- Ints in Polars' 128-bit ranges (`polars_lit_model.hpp`: HugeInt,
  huge_value, lit types Int128/UInt128): scalars past Int64 are Int128
  literals (UInt64 in [2**63, 2**64)), list items Int128 or UInt128
  (OverflowError past u128), with their supertypes, strict builders,
  casts, comparisons and `is_in` casts; `head(n)` refuses n past Int64
  and past u32 with Python's and Polars' messages. Probe i1 409 -> 0 of
  1,000.
- A struct beside an Int128 value takes the ordinary struct/number
  supertype (DF's "struct keeps its fields" rule was wrong); float cells
  cast to String inside nested literals as Polars casts a Float64.
- An Int128 `literal` result column has no native cell: the capture
  refuses it (`no native cell for 128-bit int`, scripts/repetition_domain.py)
  and the bank replay checks native frames the same way.
- New big-int probes on the CJ, CI, CH, CK and CN table domains (29,447
  calls): 902 left after the fixes (parameter lists indexed by huge ints,
  int keys past Int128 in the counts and case keys, list bounds, dict
  bounds, UInt64 literal cells, the paths timestamp sort key). Of those,
  831 are native refusals in the LTL, between, timestamp and variants
  filters, 65 are those filters' check order with an int past Int128, 6
  attribute counts over unmodelled key lists: carried to DH.
- pandas LTL filters compare a str column with a tuple of one item per
  row element-wise, as numpy holds the tuple (str array, object array or
  number array).
- Set probes (seed 8): CJ 4, CI 1, CH 1 of 42,194, all declared kinds.
- Fresh seeds 2026113000-11: eight calls, all fixed; 2026113012-23: 0.

In the suite: new `docs/polars-filter-dg-int-bank.json.gz`,
`docs/polars-filter-dg-set-bank.json.gz` and
`docs/pandas-filter-dg-bank.json.gz` with their ctests. The probes, big-int
and set domains and fresh banks are in the project files under
`review-harness/round-dg/`.

No fixture changed. The device check on the Mac was skipped again (no device
linked).

Round DH re-reviewed DG (Review DH1). DG's commit 51851f2 ran CI run
43976413859 (green on all six jobs). DH modelled the big-int calls DG left
to native refusals, probed the Polars sequence filters with ints past
Int64 for the first time and replayed two batches of fresh seeds:
- Every Polars table filter takes ints past Int64 (`value_of` no longer
  refuses them; the `huge` flag is inert). The LTL, between, timestamp
  and variants filters check them in the reference's order: `pl.lit` of a
  scalar past Int128 fails `integer too large` before any column or join;
  a list key builds as one strict Series literal first, then item by item
  where it is spread (`over`, `group_by`, `join`), then anchors or
  activities, then `pl.col` (`argument_literal_fault`, `scalar_range`,
  `spread_key`). A_next_B_next_C row-indexes a list key with an int first
  item (`Python int too large to convert to C long`). DG's 902 -> 6.
- Set items of the attribute counts get their typed literal faults in set
  order (probe a2: 21 left, several-key counts, declared).
- The sequence bank test reads ints past Int64; new big-int probes on the
  CP, CK, CJ and CH sequence domains (8,538 calls): Polars parameters
  carriers read as pandas' (round CL) with `cannot fit 'int' into an
  index-sized integer`, nocc `ea_count0` None/non-dict, nocc counts as
  Python ints, `return_dict` truthiness. 255 -> 161, carried.
- four_eyes with a literal case key and three or more activities resolves
  the third pair's inputs (activities 0-2, or 0-3 with more) before the
  `literal_right` duplicate.
- Fresh seeds 2026120100-11: one call, a reference-nondeterministic rework
  row order (declared); 2026120112-23: one call, fixed (the four_eyes
  order above).

In the suite: new `docs/polars-filter-dh-bank.json.gz` and
`docs/polars-sequence-filter-dh-big-bank.json.gz` with their ctests. The
probes, big-int sequence domains and fresh banks are in the project files
under `review-harness/round-dh/`.

No fixture changed. The device check on the Mac was skipped again (no device
linked).

Round DI re-reviewed DH (Review DI1). DH's commit 9ad1ecb ran CI run
43989318482 (green on all six jobs). DI modelled the sequence calls DH carried
and the attribute counts over list and tuple keys, probed the sequence
filters with scalar keys of every kind and replayed two batches of fresh
seeds:
- An int, float or bool key reaches the sequence filters'
  `PolarsFilterValue` overloads (the bank test routes it there): an
  activity or timestamp key fails `pl.col`, a case key is a literal named
  `literal` in pm4py's sort, group_by and join, and an int past Int128
  fails its `pl.lit` in values, `min_occurrences` and group-key items.
  DH's 161 -> 0.
- New scalar-key probes on the CP, CK, CJ and CH sequence domains (8,490
  calls): NaN sort literals print `NaN`, prefixes and suffixes refuse a
  scalar case key as a Series name, the trace variants fail the list
  join on a non-String activity column before the literal sort. 159 ->
  113, carried (datetime keys, None or NaN case keys, NaN nocc glue).
- Attribute counts over list and tuple keys (`list_key_stats`): the
  group_by spread, the `{row[0]: row[1]}` dict, the threshold and relative
  comparisons and `pl.col(list)` selection (one name, or the ambiguous
  multi-expression filter), per-case counts over the List literal and the
  first row of each case, and `df[list]` on the top-activities counts
  frame. a1 636 -> 14, new a3 1,280 calls -> 87; all left follow the
  reference's random group order (declared).
- Fresh seeds 2026120200-11: one call, a reference-nondeterministic rework
  row order (declared); 2026120212-23: one call, the DF1-7 struct-cast
  partition race (declared).

In the suite: new `docs/polars-filter-di-bank.json.gz` and
`docs/polars-sequence-filter-di-bank.json.gz` with their ctests. The
probes, scalar-key sequence domains and fresh banks are in the project
files under `review-harness/round-di/`.

No fixture changed. The device check on the Mac was skipped again (no device
linked).

Round DJ re-reviewed DI (Review DJ1). DI's commit 1e9dd4e ran CI run
44001970825 (green on all six jobs; the DI sequence bank test that failed in
DI's full run only because its bank was rebound mid-run passes there). DJ
modelled the sequence calls DI carried, probed None, NaN and datetime case
keys, took two new areas and replayed two batches of fresh seeds:
- Datetime and None case keys reach the sequence filters'
  `PolarsFilterValue` overloads: a datetime is a literal printed in UTC in
  the plans, a None key drops out of the sort, is one Null group, and fails
  the join as it is built (`must specify `on` OR `left_on` and
  `right_on``); a lone NaN key joins nothing; non-finite literal cells print
  `NaN`/`INF`/`-INF`; `get_variants_df` with a scalar literal key builds
  its one-row frame. DI's 113 -> 0; new None/NaN/datetime probes on the CP,
  CK, CJ and CH sequence domains (8,522 calls) 36 -> 0.
- Attribute counts over key lists on frames with a `literal` column or no
  rows (per case the frame's own `literal` column is left out of the
  aggregation, an empty frame has no groups) and key lists naming a missing
  column (`pl.col(names)` fails `"name" not found` first). Probes lit1 and
  lit2: all left (79) follow the random group order declared in DI.
- LTL filters on frames with their own `literal` column (probe lt7, 2,000
  calls): a resource named `literal` beside a list case key fails the
  duplicate output name in four_eyes and attr_value_different_persons;
  eventually_follows with a scalar literal key on such frames is refused
  instead of answering differently. 1,104 -> 917, all refused (carried).
- Fresh seeds 2026120300-11: two calls, both varying in the reference itself
  (the DF1-7 partition race and a rework row order; declared).
  2026120312-23: 0.

In the suite: new `docs/polars-sequence-filter-dj-bank.json.gz`,
`docs/polars-filter-dj-bank.json.gz` and
`docs/polars-filter-dj-ltl-bank.json.gz` with their ctests. The probes,
None/NaN/datetime sequence domains and fresh banks are in the project files
under `review-harness/round-dj/`.

No fixture changed. The device check on the Mac was skipped again (no device
linked).

Round DK re-reviewed DJ (Review DK1). DJ's commit 7af4161 ran CI run
44014854240 (green on all six jobs). DK
modelled the LTL calls DJ left refused, probed `literal`-column frames across
the other filters, took one new area and replayed two batches of fresh seeds:
- LTL filters on frames with their own `literal` column: a list key's
  List(String) literal against the frame's scalar `literal` column fails the
  pair join's dtype check (`SchemaError`), a scalar literal key's first case
  join names the right literal `literal_right` (the second join repeats it;
  a returned frame appends `literal_right`), and a `literal` timestamp key
  or `literal` case column beside a non-str timestamp repeats `literal` in a
  select. DJ's 917 -> 0, new probe lt8 44 -> 0.
- A Polars four_eyes or attr_value_different_persons case key naming the
  resource column now fails the duplicate output name as pm4py does (it
  answered a frame before, on any frame).
- `literal`-column frames in the other filters: table filters 16,558 calls
  0; sequence filters 8,812 calls, 15 timestamp_case_grouping calls fixed (a
  literal key's `literal` replaces the frame's own column; a str key naming
  it beside a literal key fails the duplicate name).
- New area: the LTL filters on eager DataFrames (the workbench's DataFrame
  input). The bank driver now replays cases marked `"eager"` through the
  `EventTable` overload. Eager steps run at once: no resolved plan (except
  attr_value_different_persons' group_bys, which show their eager input),
  eventually_follows and A_next_B_next_C sort first (missing names, then the
  literal-key ShapeError), four_eyes and attr_value_different_persons check
  the [case, resource] step on the first activity's rows before the joins.
  Probe le1 641 -> 1; probe le2 2,075 clean, 325 carried.
- Fresh seeds 2026120400-23: four calls, all varying in the reference itself
  (equal-size join and group_by row orders, the DF1-7 partition race;
  declared).

In the suite: new `docs/polars-filter-dk-ltl-bank.json.gz`,
`docs/polars-sequence-filter-dk-bank.json.gz`,
`docs/pandas-filter-dk-bank.json.gz` and
`docs/polars-filter-dk-eager-ltl-bank.json.gz` with their ctests (589 tests).
The probes, the eager and `literal`-frame domains, the le2 dump of the 325
carried calls and the fresh banks are in the project files under
`review-harness/round-dk/`.

No fixture changed. The device check on the Mac was skipped again (no device
linked).

Round DL re-reviewed DK (Review DL1). DK's commit 4358d93 ran CI run
44024654681 (green on all six jobs). DL modelled the eager LTL calls DK
left, took the eager area on to the other Polars table filters and replayed
two batches of fresh seeds:
- The eager LTL calls DK left: each activity's filter runs as it is built,
  before the next activity's literal and the [case, resource] step; a key
  that is no str is a literal named `literal` in that step (two such keys,
  or one beside a `literal` column, fail as duplicates); four_eyes' first
  pair join runs as it is built (a list key's first item naming no column
  fails there, the resource suffix TypeErrors after it). le2's 325 and
  le1's one refused call -> 0.
- The other Polars table filters on eager DataFrames (attributes, cases,
  timestamp, paths, between, variants; the bank driver now calls every
  module's `EventTable` overload for an eager case). Each statement runs at
  once, so an earlier step's error comes first and nothing is pushed down
  across filter calls: predicates keep pm4py's written order and a None
  bound stays in place as a null column (one filter call still merges equal
  bounds and empties an empty interval); `filter_on_ncases` follows
  `DataFrame.head(n)` (negative n, float, non-number and too-large n); the
  between, paths and variants filters sort by [case, timestamp] first (a
  literal key fails the sort's ShapeError unless the frame has one row);
  the case size runs each bound as its own filter; an eager failure shows
  no resolved plan except a group_by's over its eager input. Seven probes
  over the base, CK, CN, CJ, CH and CI table domains (22,259 calls): e1 282
  -> 0, all seven 0; one equal-size join order varying in the reference is
  marked unordered.
- Fresh seeds 2026120500-11 and 2026120512-23: 0.

In the suite: new `docs/polars-filter-dl-eager-bank.json.gz` with its
ctest; `docs/polars-filter-dk-eager-ltl-bank.json.gz` now holds the 326
calls DK left out. The probes, the eager table domain and the fresh banks
are in the project files under `review-harness/round-dl/`.

No fixture changed. The device check on the Mac was skipped (the Mac was
offline).

Round DM re-reviewed DL (Review DM1). DL's commit 032458e6 ran CI run
44072270011 (green on all six jobs). DM took the eager area on to the
Polars sequence filters, ran the earlier set, big-int, `literal`-column and
empty-frame table probes on eager frames and replayed two batches of fresh
seeds:
- The Polars sequence filters on eager DataFrames (the sequence bank
  driver now calls each module's `EventTable` overload for an eager case
  and new `LazyEventTable` overloads otherwise). The helpers that end in
  `.collect()` fail there; the variant filters sort by [case, timestamp]
  before the group_by builds pl.col(activity) (a literal case key fails the
  sort's ShapeError unless the frame has one row) and build the group_by
  keys' literals when agg runs; an empty prefix or suffix list joins a
  LazyFrame onto the DataFrame; joins on a None case key and an empty key
  list fail after the comparisons; `activity_split` runs its columns step
  by step (list case keys: missing names, a repeated `_row_within_case`,
  no name over rows, then the separator and the alias); rework runs the
  activity filter, the group_by and the count filter in turn. Two lazy
  fixes came with it: a scalar int activity past the Int128 range in
  `activity_split`, and a list case key beside an `apply_activity_set`
  dict or list activity key repeating `literal`. Probes q1b-q8 (81,030
  calls) 4,853 -> 0; seventeen probes over the set, scalar, big-int and
  None/NaN/datetime sequence domains 393 -> 2 (NaN sets, declared); eight
  fresh-seed probes 0; seven equal-size rework joins marked unordered.
- Eager table probes over the set, big-int, `literal`-column and
  empty-frame domains: the top activities filter follows
  `DataFrame.head(n)` before the missing collect() (73 calls -> 0).
  Eighteen probes leave 138 calls carried to DN (attributes 41, cases 35,
  variants 30, between 16, LTL 14, paths and timestamp one each).
- Fresh seeds 2026120600-11 and 2026120612-23: one `attributes.apply`
  call in each CJ table bank comes back in either row order in the
  reference itself (declared); the rest 0.

In the suite: new `docs/polars-sequence-filter-dm-eager-bank.json.gz`
(16,439 calls) with its ctest; `test_review_ca1.cpp` calls the lazy
overloads for its two LazyFrame messages. The probes, the eager sequence
domain, the dumps of the carried table calls and the fresh banks are in
the project files under `review-harness/round-dm/`.

No fixture changed. The device check on the Mac was skipped (the Mac was
offline).

Round DN re-reviewed DM (Review DN1). DM's commit 478ee7f7 ran CI run
44088769580 (green on all six jobs). DN worked down DM's carried eager
table calls, probed the shapes behind them eagerly and lazily, took the
eager sequence filters on to `literal`-column and empty frames, modelled
the partition race by its majority outcome and replayed two batches of
fresh seeds:
- DM's 138 carried eager table calls -> 5 declared: eager group_bys of
  the attribute counts and relative occurrence run before the missing
  collect() (dict, list, None and scalar keys), lazily a one-name list key
  reads that column and several items holding a dict resolve in order;
  eager case filters over no key run the group_by and filters before the
  join finds no key (lazily the right side's timestamp first); UInt128
  dict fields meet signed columns as a null Int128; the eager between
  separator after `is_in`; LTL keys selecting no column, big-int keys and
  list resource keys; paths list timestamps of any length and items past
  UInt128; the variants sort before the group_by with literal timestamps.
  Probes a1, c2, l1, p1, p2 (eager and lazy) 0 differing but six
  random-order a1 calls.
- Literal case keys in the variants filters (probe v1, 3,150 calls each
  way): empty activity lists (the join misses `variant` over a `SELECT`
  or an empty `AGGREGATE`), activities naming a key item, a None case key
  beside a literal or None timestamp, several key items on one row with a
  None second item; 126 native refusals -> 0.
- Eager sequence probes over `literal`-column frames and frames of no row
  or one row, eagerly and lazily (dn/seqwrap.sh, twelve probes over the
  CK, CP, CJ and CH sequence domains, 49,888 calls): 0 differing but two
  equal-size rework joins (declared).
- The struct-cast partition race (DF1-7): the cast error wins when at
  least two of the four row partitions fail the cast or one holds four
  present cells of the cast column (the majority outcome of 617 probed
  layouts); the native filter now picks the error that way.
- Fresh seeds 2026120700-11 and 2026120712-23: the race (fixed), one
  `prefixes.apply` and one `rework.apply` row order the reference varies
  itself (declared); the rest 0.

In the suite: new `docs/polars-filter-dn-bank.json.gz` (26,582 calls) and
`docs/polars-sequence-filter-dn-bank.json.gz` (9,561 calls) with their
ctests. The probes, the wrappers and the fresh banks are in the project
files under `review-harness/round-dn/`.

No fixture changed. The device check on the Mac was skipped (the Mac was
offline).

Round DO re-reviewed DN (Review DO1). DN's commit a0fee4a ran CI run
44105722499 (green on all six jobs). DO asked whether the case, paths and
timestamp filters race in row partitions as the numeric attribute filters
do, probed iterated arguments and DN's v1 shapes in the Polars sequence
variant filters, took the eager sequence probes on to the set, big-int and
scalar domains on `literal`-column frames and replayed two batches of
fresh seeds:
- The case size and performance filters filter the group_by's frame with
  the struct bound and race in its row partitions: the cast error wins
  with at least four cases of which two hold a present cast cell, on
  frames of four columns or more (the native filters give that majority
  outcome; probe c3, 962 calls each way, 0 differing). The paths
  performance filter on eager frames of fewer than four rows gives the
  comparison error (modelled); on larger eager frames the reference's
  error depends on the process (declared, the bank test accepts either
  error of the same comparison). The timestamp filters refuse a dict
  bound in Python before any plan, so they do not race.
- Arguments Python iterates (probe sv2, 2,112 calls each way): the
  variant filters and `rework.apply_activity_set` take a str's characters
  and a dict's keys; the start and end activity filters pass a str to
  `is_in` as a column name (the boundary alias, the case column, or a
  failing join). 600 lazy and 472 eager calls -> 0.
- DN's v1 shapes (probe sv1, about 7,080 calls each way): dict case keys in
  `get_variants_df` (the sort's struct literal, refused by the group_by at
  once on a LazyFrame, after the sort and pl.col(activity) on a
  DataFrame), dict activity keys as the list of their keys, list case keys
  on one row, and the eager group_by building its keys after
  pl.col(activity). 30 lazy and 61 eager calls -> 0.
- Eager probes over the set, big-int and scalar sequence domains on
  `literal`-column frames (12 probes, 49,629 calls): `activity_split`
  shows an imploded scalar int or float as `Unknown(Int(n))` or
  `Unknown(Float)` in its `is_in` errors (also lazily; probe as1, 420 ->
  0) and builds pl.col(activity) after the eager cum_count; five
  equal-size rework joins and one NaN set order are reference-random
  (declared).
- The pandas numeric attribute filters with datetime bounds (fresh pandas
  CK 2026120821 case 166, probe n1, 2,520 calls): a NaN float cell fails
  Python's comparison with a datetime, and a datetime64 column refuses
  None, numbers and a datetime of the other time-zone kind as a whole
  (`Invalid comparison between dtype=... and ...`). 252 -> 0.
- Fresh seeds 2026120800-11 (clean) and 2026120812-23: the pandas
  datetime bound (fixed) and one equal-size `attributes.apply` join order
  the reference varies itself (declared).

In the suite: new `docs/polars-filter-do-bank.json.gz` (3,240 calls),
`docs/polars-sequence-filter-do-bank.json.gz` (17,853 calls) and
`docs/pandas-filter-do-bank.json.gz` (2,520 calls) with their ctests; the table bank test's struct-cast leniency works both ways. The
probes, the dumps and the fresh banks are in the project files under
`review-harness/round-do/`.

No fixture changed. The device check on the Mac was skipped (the Mac was
offline).

Round DP re-reviewed DO (Review DP1). DO's commit ed4809e ran CI run
44123697378 (green on all six jobs). DP probed iterated arguments in the Polars table
filters, dict keys in the Polars sequence filters and the attribute counts,
and date strings and datetime bounds in the pandas filters, and replayed
two batches of fresh seeds:
- Iterated arguments in the Polars table filters (probes tp1, 2,796 calls
  each way, and tp2, 1,602): the native filters already matched but for
  twelve `paths.apply` calls with a tuple-keyed dict (outside the native
  dict carrier, declared) and the dict attribute keys below.
- Dict keys in the sequence filters beyond the variant filters (probe sv3,
  3,694 calls each way): the start and end activity filters' group_by
  refuses a dict case key (a DataFrame's after pl.col(activity)) and reads
  a dict activity key as its keys; eager rework fails with a DuplicateError
  when a dict case key's struct literal meets a literal activity key. 562
  lazy and 1,192 eager calls -> 0.
- Dict attribute keys (probe tp3, 1,400 calls each way):
  `get_attribute_values` refuses them at once; the relative occurrence
  filter per case checks its case key first (a set refused, on an eager
  frame a missing column or a second `literal`); the attribute filters
  read pl.col(dict) as its names, so several names expand the predicate.
  515 lazy and 645 eager calls -> 0.
- Date strings against pandas datetime64 columns (probes n2, 9,394 calls,
  and n3, 4,200 random strings): an ISO-8601 text compares as its
  Timestamp (a naive one localized to an aware column, an aware one
  refused by a naive column), the NaT texts match nothing, short numbers
  are refused; the LTL `timestamp_diff_boundaries` compares a datetime
  bound row by row; the case size filter names float64 sizes. n2 1,754 ->
  288, all native refusals of texts only pandas' dateutil fallback reads
  (declared), as are n3's 3,042.
- Fresh seeds 2026120900-11: three reference-random calls (two struct-cast
  races the bank test accepts, one equal-size rework join order);
  2026120912-23: four such calls (three struct-cast races, one two-case
  `suffixes.apply` join order). Nothing to fix.

In the suite: new `docs/polars-filter-dp-bank.json.gz` (11,572 calls),
`docs/polars-sequence-filter-dp-bank.json.gz` (7,388 calls) and
`docs/pandas-filter-dp-bank.json.gz` (10,264 calls) with their ctests. The
probes, the dumps and the fresh banks are in the project files under
`review-harness/round-dp/`.

No fixture changed. The device check on the Mac was skipped (the Mac was
not linked).

Round DQ re-reviewed DP (Review DQ1). DP's commit bb51ec2 ran CI run
44139462790: five jobs green, the macOS job cancelled after 15 minutes
without a runner (no step ran). DQ probed dicts with keys that are no str
in the Polars filters, date strings against pandas datetime64 columns of
other units and zones, and the time zone database, and replayed two
batches of fresh seeds:
- Dicts with keys that are no str (probes tq1, 3,276 calls eagerly and
  lazily in the table filters, tq2, 3,096 calls each way in the sequence
  filters, tq3, 270 each way): the bank transport keeps a dict key's type
  now (`EventAttribute::typed_key`); pl.col(dict) is pl.col of its keys'
  list, and where a filter builds the key's literal with the expression, a
  dict with such a key fails pl.lit before group_by, over or join refuse
  the dict; dict literals keep Python-equal keys once (tuple keys no
  longer collapse); `paths.apply` iterates a dict of paths by its typed
  keys. tq1 2,126/2,096 -> 0, tq2 1,800/2,170 -> 0.
- Date strings against datetime64 columns of other units and zones (n4,
  11,556 calls; n5, 3,600 calls with nanosecond and range-edge texts):
  nanosecond texts parse at nanosecond resolution and must fit pandas'
  int64 range (after localizing into an aware column, or as the local
  wall time after converting into it); zones that keep DST in 9999 raise
  NotImplementedError past year 9999. n4 66 -> 0, n5 102 -> 72, all in the
  declared one-day window at the range ends (pandas is erratic there in
  290 of 599 zones).
- The time zone database: libstdc++ 13's tzdata.zi disagreed with the OS
  zoneinfo (what Python reads) in 217 of 599 zones, so POSIX builds use
  the vendored date/tz over the OS zoneinfo (Windows keeps MSVC's), with
  backward links the OS lacks resolved through the standard library;
  `minera_zone_database_tests` pins ten historic offsets.
- dateutil-only date strings: not modelled (kept declared).
- Fresh seeds 2026121000-11: one call, the keep-once relative occurrence
  count with two keys that are no str (DuplicateError `literal`; probes
  tq4 and tq5); 2026121012-23: one call, `eventually_follows` with a list
  time bound where pandas' inner merge reorders the chains (label
  grouping, the equal-length reordering, numeric keys labelled from the
  right, missing keys and timestamps joining the merge; fuzzed over every
  dtype, probes n6 and n7). Both fixed; everything else replays clean.

In the suite: new `docs/polars-filter-dq-bank.json.gz` (8,018 calls),
`docs/polars-sequence-filter-dq-bank.json.gz` (6,192 calls) and
`docs/pandas-filter-dq-bank.json.gz` (16,852 calls) with their ctests. The
probes, the fuzzers (`mo/`), the dumps and the fresh banks are in the
project files under `review-harness/round-dq/`.

Full suite 602 passed; no fixture changed (2,673 responses). The device
check on the Mac was skipped (the Mac was not linked).

Round DR re-reviewed DQ (Review DR1). DQ's commit 6e35984 ran CI run
44169814298: Python, verifier, frontend and Linux green (the Linux job had
finished before DR pushed, contrary to the next list), and the macOS job
green for the first time since DO. DR followed pandas' merge order into the
other modules and replayed a batch of fresh seeds:
- The merge's plain order (probes r1, 451 calls, and r2, 947 calls of
  `eventually_follows` list bounds): when both key arrays are
  non-decreasing and one side holds no key twice, pandas merges by left
  row, then right row. The model moved to `pandas_merge_order.hpp`
  (shared by the LTL filter and link analysis); a datetime NaT key is an
  ordinary label. r1 6 -> 0, r2 -> 0.
- Link analysis on CSV uploads (dense doc/ref collisions, every flag
  combination; 3,200 calls): the self-merge's pairs in merge order,
  keep-first keeping the first target in that order, each source's
  targets in a CPython set's order, the links from a second merge. 84 ->
  0; 1,600 calls of seeds 0-199 agree in exact order.
- List keys in the pandas attribute filters (fresh pandas CK 2026121109
  case 255; probes p1, 2,499 calls, p2, 9,800 calls): the relative
  occurrence count on two-row frames takes a key list as one grouper
  array (pandas' index replacement), list attribute keys count tuples,
  and the values filters mask `df[list].isin(values)` (DataFrame.where).
  p1 293 -> 0, p2 1,306 -> 0.
- Fresh seeds 2026121100-11: three calls, the CK one above, a struct cast
  race (CJ 2026121103 case 795) and an equal-size rework join (sequence
  CH 2026121108 case 899), both declared.

In the suite: new `docs/pandas-filter-dr-bank.json.gz` (13,697 calls)
with `minera_pandas_filter_dr_bank_parity_tests`, `minera_review_dr2_tests`
(208 link analysis calls), and `minera_ocel_links_parity_tests` now binds
the exact link order except under PROPAGATE (declared: the order of a set
grown by `set.union`). The probes, the fuzzers (`la/`), the dumps and the
fresh banks are in the project files under `review-harness/round-dr/`.

Full suite 604 passed; no fixture changed (2,673 responses). The device
check on the Mac was skipped (the Mac was not linked).

Round DS re-reviewed DR (Review DS1). DR's commit c56ad7b ran CI run
44186475483: Python, verifier, frontend, Linux and macOS green, Windows
still building when DS pushed; DQ's Windows job finished green, so all six
DQ jobs passed. DS took DR's carried merges and the propagated link order:
- Temporal profile conformance on a frame (3,000 server calls with
  partial profiles): the per-case deviation order already agreed; an empty
  profile makes pandas raise `KeyError: '<activity key>'` and Polars a
  column-not-found error (native listed the cases). Fixed;
  `minera_review_ds1_tests` binds 209 calls.
- Link analysis under PROPAGATE: Gauss-Seidel passes over the sources in
  the in/out merge's first-appearance order, each source's targets united
  in ascending order from the set as its turn began, a union kept only
  when it grows; CPython's `set.union` table (copy, then set_merge with
  its presizing and equal-size slot copy) modelled as `RowSet`. 1,600 of
  1,600 small-domain and 1,197 of 1,200 larger-domain calls in exact order
  (3 declared, equal as sets). `minera_review_dr2_tests` and the OCEL
  links bank now bind exact order (the bank's three declared calls sorted).
- OCEL flattening: row order agrees (3,000 random OCELs); an OCEL without
  relations, objects or events now raises pm4py's reader/flattening
  errors instead of flattening to an empty log.
- Tuple bounds of the pandas case performance and case size filters
  (fresh pandas CK 2026121221 case 125; probes c1-c4, 2,220 calls): the
  tuple becomes a numpy array first (ragged nesting raises the
  inhomogeneous-shape error, str leaves have no comparison loop), then
  broadcasts with the cases, compares leaf by leaf and must come out one
  value per case. c1 154 -> 0, c2 96 -> 0, c3/c4 0.
- Fresh seeds 2026121200-11: five calls, the declared struct cast race;
  2026121212-23: three calls, the pandas CK one above and two equal-size
  join orders (sequence CJ 2026121219 case 301 `suffixes`, CK 2026121218
  case 510 `rework`; 24 reruns each gave both orders, declared).

In the suite: new `docs/pandas-filter-ds-bank.json.gz` (2,220 calls) with
`minera_pandas_filter_ds_bank_parity_tests`, `minera_review_ds1_tests`, and
relation-free flattening in `minera_ocel_transform_tests`. The probes, the
sweeps, the dumps and the fresh banks are in the project files under
`review-harness/round-ds/`.

Full suite 605 passed before the flattening and tuple-bound fixes; after
them the 90 OCEL and pandas tests (606 tests in all with the new bank)
passed again; no fixture changed (2,673 responses). The device check on
the Mac was skipped (the Mac was not linked).

Round DT re-reviewed DS (Review DT1). DS's commit 225ebc7 ran CI run
44201772589: all six jobs green; DR's Windows job finished green, so all
six DR jobs passed. DT took DS's carried tuple bounds and degenerate OCELs:
- Tuple bounds: numpy's array rules moved to `pandas_numpy_operand.hpp`
  and now serve paths performance, the numeric attribute filters (numpy
  dtypes, the str StringArray model, datetime64 columns) and LTL
  time-difference bounds. Probes t1 1,648 -> 0, t2 393 -> 0, t3 7 -> 0,
  t4 504 -> 0.
- Polars between filter: the separator diagnostic's plan folds a FILTER on
  an empty anchor list to `false.alias(...)` (fresh CK 2026121301 case
  311; u1 180 -> 0).
- Degenerate OCELs: an event-free JSON file fails at read
  (`KeyError: 'ocel:timestamp'`); an object-free one reads as an objects
  frame without columns and raises pm4py's KeyErrors in 20 route variants;
  a relation-free one has float64 relations (flattening and lifecycle
  enrichment fail), carried by the transport as
  `"minera:untypedRelations"` so empty filter results still chain like
  in-memory logs; the SQLite reader fails on tables without rows; the
  objects filter's level expansion raises KeyError on an unknown id.
- Fresh seeds 2026121300-11: three calls, the between plan above and two
  struct cast races (declared).

In the suite: new `minera_review_dt1_tests`,
`docs/pandas-filter-dt-bank.json.gz` (12,854 calls) with
`minera_pandas_filter_dt_bank_parity_tests` and
`docs/polars-filter-dt-bank.json.gz` (504 calls) with
`minera_polars_filter_dt_bank_parity_tests`. The probes, the sweeps and
the fresh banks are in the project files under `review-harness/round-dt/`.

Full suite 608 of 609 passed; the object attributes module test made the
object-frame check narrower (only a frame declared without columns), after
which the 32 OCEL tests passed; no fixture changed (2,673 responses). The
device check on the Mac was skipped (the Mac was not linked).

Round DU re-reviewed DT (Review DU1). DT's CI runs 44216750418 and
44216771591: verifier, Python, frontend and the Linux native job green;
macOS and Windows still running when DU pushed. DU took DT's carried
pandas operands and the frame types of OCELs that routes build:
- Masked columns (Int, UInt, Float, boolean) in the numeric attribute
  filters: a tuple bound is a numpy array that never broadcasts (1-d, same
  length, no str loop, then item by item; NaN items read False); list
  bounds follow pandas' list inference (numbers, string array, datetime
  array, object array) on numpy and masked columns; float32 columns
  compare array items at float64 (`masked_tuple_mask`,
  `list_operand_mask`). Probes v1 12,456 -> 0, v1b 376 -> 0; the
  timestamp and variants filters (v2, v3) already matched.
- Naive OCEL frames: a CSV import without offsets (datetime64[us], ns
  past six fraction digits, s without rows) and both reference playouts
  (datetime64[s]) are naive in pm4py, and stay so through filters,
  splits and enrichment. `Ocel::naive_event_time_unit` holds it, the
  transport carries it as `"minera:naiveEventTimes"` with the texts as
  written, the timestamp filters raise pm4py's TypeError, and JSON
  downloads spell naive times with `Z`. Before, the transport stamped
  them UTC, so every route after the import printed `+00:00`.
- Frame shape: split sublogs keep the source's column order, declared
  columns, float relations and naive unit (they were rebuilt without
  them); the reference playouts list events activity first.
- Fresh seeds 2026121400-11: two calls, both the declared struct cast
  race.

In the suite: new `minera_review_du1_tests` and
`docs/pandas-filter-du-bank.json.gz` (21,524 calls) with
`minera_pandas_filter_du_bank_parity_tests`. The probes, the sweeps and
the fresh banks are in the project files under `review-harness/round-du/`.

Full suite 611 of 611 passed on the first run; no fixture changed (2,673
responses). Python script and frontend tests not rerun (scripts and
frontend unchanged). The device check on the Mac was skipped (the Mac was
not linked).

Round DV re-reviewed DU (Review DV1). DU's CI run 44228428180: green on
all six jobs.
DV took DU's two found items and the PROPAGATE calls declared since DS:
- Relation-free OCEL CSV imports and empty playouts: pm4py's relations
  frame holds only `ocel:qualifier` (an empty playout's events and objects
  frames have no columns), so nearly every function raises KeyError on the
  first column it reads. The CSV reader and both reference playouts
  declare those frames (`mark_ocel_relation_free_frames`), and each
  function checks its first column (`require_ocel_relation_column`,
  `require_ocel_event_column`); a filter whose selection is already empty
  still returns an empty log. Sweep 319 of 352 -> 0, variants 1 -> 0.
- The CSV reader's timestamps: pandas infers one format from the first
  text and refuses later texts of another shape (`doesn't match format`,
  `unconverted data remains`) and mixed offsets (`Mixed timezones
  detected`); modelled for the ISO family (`pandas_iso` in
  `ocel_csv.cpp`), other shapes declared. Model 0 of 23,000, native 0 of
  4,300.
- PROPAGATE: pm4py walks the linked pairs in the order of the swapped
  self-merge, pair by pair (DS walked each source's targets together).
  Server sweeps 1,600 of 1,600 and 3,600 of 3,600 in exact order; the
  three declared calls and the links bank's three are exact now.
- Fresh seeds 2026121500-11: one call (pandas CK `variants.apply` with
  list items in the case key list), found and carried to DW.

In the suite: new `minera_review_dv1_tests` with
`docs/ocel-csv-timestamp-dv-bank.tsv.gz` (4,000 columns);
`minera_ocel_links_parity_tests` binds every propagated call in order.
The probes, the sweeps and the fresh banks are in the project files under
`review-harness/round-dv/`.

Full suite 606 of 612 on the first run; the other six (OCEL CSV re-reads
of inputs pm4py refuses, five object graph module tests declaring a
relations frame without its core columns) were updated and pass. The
workbench fixture replay gave 2,673 responses unchanged.
Python script and frontend tests not rerun (scripts and frontend
unchanged). The device check on the Mac was skipped (the Mac was not
linked).

Round DW re-reviewed DV (Review DW1). DV's CI run 44239415021: green on
the Linux, macOS, frontend, Python and verifier jobs (Windows ctest still
running at the handoff).
DW took DV's found item, list case keys across the pandas filters, with a
shared model of how pandas reads them (`src/backend/pandas_list_key.hpp`):
- `df[key]` (`pandas_getitem_list`): np.asarray first, so a regular 2-D
  key's rows are tuples, a deeper key fails (`Index data must be
  1-dimensional` for numbers, the buffer error otherwise), a ragged key
  keeps unhashable lists, bools mask rows; the KeyError prints the
  Index's dtype. Used by the timestamp, attribute, between and LTL
  filters (LTL role lists) and the relative occurrence denominator.
- `groupby(key)` (`pandas_list_groupers`, timestamp and variants
  filters): the whole key as one array of labels, or per item a mapping,
  a callable, an array (length check, 2-D rows as tuples, deeper ones
  fail when the groups factorize) or a column label.
- `set_index(key)` in the attribute filters: one array alone compares its
  items by equality (unhashable ones too) unless all are tuples.
- between walks the key's frame flattened row by row: `index R is out of
  bounds`, then `Columns must be same length as key` (`fillna is not
  defined for MultiIndex` at k = N = 1); an object array holds
  Timestamps.
- The relative occurrence stake against the `nunique()` Series: per
  stake type (numbers, None/dict/set/datetime, str, list and tuple
  arrays); counted per case the case key's array factorizes first.
- Probes l1 217 of 1,863 -> 0, l2 1,775 of 10,935 -> 0, l3/l3b/l3c 82 of
  3,876 and 56 of 69,958 -> 0, lt1 6 of 24 -> 0.
- Fresh seeds 2026121600-11: three Polars calls, all races (two struct
  cast, one `rework.apply` row order on an Enum case column that varies
  between runs).
- Found, not taken: list case keys in the pandas sequence filters (native
  `KeyError: ''` for all of them; 6,029 of 7,230 probe calls differ).

In the suite: new `minera_review_dw1_tests` and
`docs/pandas-filter-dw-bank.json.gz` (34,520 calls) with
`minera_pandas_filter_dw_bank_parity_tests`. The probes and the fresh
banks are in the project files under `review-harness/round-dw/`.

Full suite 614 of 614 passed on the first run; every earlier pandas filter
bank replays unchanged and the workbench fixture replay gave 2,673
responses unchanged. Python script and frontend tests not rerun (scripts
and frontend unchanged). The device check on the Mac was skipped (the Mac
was not linked).

Round DX re-reviewed DW (Review DX1). DW's CI run 44249031454: green on
all six jobs.
DX took DW's found item, list case keys in the pandas sequence filters,
with the shared model of DW (`src/backend/pandas_list_key.hpp`):
- start/end activities, starts/ends with, traces, rework: `groupby(key)`
  groupers (empty key `No group keys passed!`, a short key's first unknown
  label), then `set_index(key)` in the trace and variant filters (a lone
  2-D item keeps its lists, so no row matches a tuple group);
- prefixes and suffixes: `reset_index` name collisions (`cannot insert X,
  already exists`), else `unhashable type: 'list'`;
- the timestamp grouping: the whole key is one item of `groupby([key,
  timestamp])`, levels first, an empty or all-NaN array level float64;
- the activity split: `df[key]` and between's flattened walk (NaT kept);
  the consecutive grouping: `df[key]`, duplicate columns refused, the
  addition `df[key] + df[act]` declared (NotImplementedError);
- the KeyError's `Index(...)` repr as pandas wraps and truncates it (80
  columns, right-justified non-str items, `...` and `length=N` past 100
  items; fuzzed on 12,000 random Indexes);
- a set item no longer breaks the plain-key rule (unhashable 'set').
- Polars: a list literal holding a dict whose key is no str fails as
  Polars converts the list (`'int' object is not an instance of 'str'`),
  before any type is built (`check_struct_keys` in polars_lit_model.hpp).
- The Polars filter bank test reads list cells as the capture writes them
  (other columns first, then List columns of scalar items).
- Probes s1 6,029 of 7,230 -> 70 (declared), s2 80 of 5,472 and s3 8 of
  1,488 (declared), p1 (Polars sequence) 132 of 3,295 -> 0, p2 (Polars
  table) 996 of 15,462 -> 205 (found, not taken).
- Fresh seeds 2026121700-11: one Polars call, the inner join order race
  of `attributes.apply` (24 direct reruns: 9 and 15).

In the suite: new `minera_review_dx1_tests`,
`docs/pandas-sequence-filter-dx-bank.json.gz` (13,986 calls) with
`minera_pandas_sequence_filter_dx_bank_parity_tests` and
`docs/polars-sequence-filter-dx-bank.json.gz` (3,456 calls) with
`minera_polars_sequence_filter_dx_bank_parity_tests`. The probes and the
fresh banks are in the project files under `review-harness/round-dx/`.

Full suite 617 of 617 passed on the first run; after the bank test's
list-cell change the 52 Polars filter bank tests passed again. Every
earlier pandas filter and sequence bank replays unchanged and the workbench
fixture replay gave 2,673 responses unchanged. Python script and frontend
tests not rerun (scripts and frontend unchanged). The device check on the
Mac was skipped (the Mac was not linked).

Round DY re-reviewed DX (Review DY1). DX's CI run 44261093997: green on
all six jobs.
DY took DX's found item, list and dict case keys in the Polars table
filters:
- set and dict items fail as `pl.lit` while pm4py builds `select(case)`,
  `group_by(case)` or `unique(case)` (attribute, case performance and
  timestamp filters), before any plan resolves; the shared group-key
  model builds the literals first;
- the relative occurrence denominator `select(pl.col(names).n_unique())[0,
  0]` over a list of names or a dict of str keys: distinct names in order,
  the first missing one fails by name, else the first name counts (models
  the declared DQ1-5);
- the LTL filters: a list item of scalars (or an empty one) is a list
  literal named `literal`, so two items repeat the name; a key of names
  windows over its items and sorts as one List literal; the sort length,
  the first pair's join and the `_right` collision come after the first
  pair's activity filters;
- eager variants: a missing listed name fails at the frame's `group_by`
  (after the sort's `ShapeError` unless one row).
- Probes p2 205 of 15,462 -> 0, p3 112 of 4,840 -> 0, p3e 10 of 4,840 ->
  0, p4 3,864 of 14,885 -> 0, p5 and p6e 32 of 4,172 -> 0.
- Fresh seeds 2026121800-11: two Polars calls, both the struct cast race
  (cast failure against comparison failure of the same `<=`).

In the suite: `docs/polars-filter-dy-bank.json.gz` (150 cases, 44,577
calls) with `minera_polars_filter_dy_bank_parity_tests`. The probes and the
fresh banks are in the project files under `review-harness/round-dy/`.

Full suite 618 of 618 passed on the first run; every earlier Polars table
bank replays unchanged and the workbench fixture replay gave 2,673
responses unchanged. Python script and frontend tests not rerun (scripts
and frontend unchanged). The device check on the Mac was skipped (the Mac
was not linked).

Round DZ re-reviewed DY (Review DZ1). DY's CI run 44276414877: green on
all six jobs.
DZ took two declared items:
- the consecutive grouping's `df[key] + df[activity]` with a list key
  (DX1): the union of the frame's columns and the Series' index (names
  sorted unless strictly increasing, the index sorted unless
  non-decreasing, both monotonic: names then labels, else sorted labels
  then sorted names; a repeated label under both monotonic raises the
  Length of values error), the reindexed activity dtype (int to float64
  and bool to object once a name is listed, object inferred again), the
  column-wise (extension arrays) or block-wise (object, numpy, naive
  datetime64) add with their own errors, numpy's quicksort argsort for an
  unsorted empty-label index (reference run with NPY_DISABLE_CPU_FEATURES),
  then the multiple/no columns errors or one run per row; Python model
  0 of 23,400 fuzzed keys, C++ probes c1-c3 0 of 10,752 and DX's 158
  declared calls 0 (`consecutive_list_key` in
  `pandas_sequence_filters.cpp`);
- the OCEL CSV timestamp shapes outside the ISO family (DV1-2): pandas'
  format guess (dateutil: m/d/Y, d/m/Y past 12, `-` `/` `.`, one-digit
  fields, literal t, z and GMT, `%Z` for UTC, `+HH`, ` %z`), the ISO parser
  first for an ISO format and the strptime rules for the rest (whose errors
  are raised; seconds 60 and 61 roll over; day past the month), one zone,
  nanoseconds past six digits; the reader keeps such times as ISO texts of
  pandas' instants (`pandas_guess` in `ocel_csv.cpp`, ahead of DV's
  `pandas_iso` check). Python model 0 of 83,647 fuzzed columns; server
  imports 51,487 of 51,796 covered columns, the rest offsets past 14 hours
  (declared).
- Fresh seeds 2026121900-11: two calls, both declared races (the Polars
  sequence equal-size join order, CK 2026121906; the struct cast race,
  Polars table CJ 2026121903).

In the suite: `docs/pandas-sequence-filter-dz-bank.json.gz` (697 cases,
10,910 calls) with `minera_pandas_sequence_filter_dz_bank_parity_tests`,
and `minera_review_dz1_tests` with `docs/ocel-csv-timestamp-dz-bank.tsv.gz`
(5,000 columns). The probes, models and fresh banks are in the project
files under `review-harness/round-dz/`.

Full suite 619 of 620 passed on the first run; the one failure was
`minera_review_dx1_tests`, whose declared NotImplementedError for the
consecutive addition is now pandas' UFuncTypeError (test updated, then the
45 OCEL, CSV and review tests passed again after the last rebuild). The
workbench fixture replay gave 2,673 responses unchanged. Python script and
frontend tests not rerun (scripts and frontend unchanged). The device
check on the Mac was skipped (the Mac was not linked).

Round EA re-reviewed DZ (Review EA1). DZ's CI run 44289921628: green on
all six jobs.
EA took the CSV timestamp columns DZ1-2 had left declared:
- the format guess through dateutil and pandas' token matching (month and
  weekday names, AM/PM with `%I`, a comma fraction, the compact forms) and,
  when no format is guessed, every text on its own (pandas' ISO parser, its
  MM/DD/YYYY and year or month-year readers, then dateutil with pandas'
  checks and errors: DateParseError, the weekday-without-day and
  un-recognized timezone ValueErrors, OutOfBoundsDatetime past int32;
  two-digit years around the clock year, GMT+2 as two hours west,
  nanoseconds from a long H:MM:SS fraction) (EA1-1). `pandas_to_datetime`
  (`src/backend/pandas_to_datetime.cpp`) replaces DZ's `pandas_guess`
  parser; the reader still keeps each event time as the ISO text of
  pandas' instant. Python model 0 of 37,249 fuzzed columns plus DZ's
  86,729; C++ 0 against the model on all 130,000 and 0 of 14,910 fresh
  covered columns against pandas; server imports 6,002 of 6,002;
- read_csv's NA spellings and to_datetime's NaT spellings (NaT, nat, NAT,
  nan, NaN, NAN) are missing values, for the guess too; a dropped row with
  such a time no longer refuses the file (EA1-2).
- Fresh seeds 2026122000-11: four calls, all the declared struct cast race
  (Polars table CK 2026122001 cases 35, 250, 538; CJ 2026122003 case 588).

In the suite: `minera_review_ea1_tests` with
`docs/ocel-csv-timestamp-ea-bank.tsv.gz` (5,000 columns whose reading does
not move with the clock year until 2050); `minera_review_dz1_tests` and
`minera_ocel_csv_tests` now expect pandas' readings and errors for the
shapes DZ left declared. The models, the C++ comparison driver, the
corpora and the fresh banks are in the project files under
`review-harness/round-ea/`.

Full suite 621 of 621 passed on the first run. The workbench fixture
replay gave 2,673 responses unchanged. Python script and frontend tests
not rerun (scripts and frontend unchanged). The device check on the Mac
was skipped (the Mac was not linked).

Round EB re-reviewed EA (Reviews EB1 and EB2). EA's CI run 44304386291:
green on all six jobs (Windows included).
EB took the other OCEL readers' timestamp columns from EA's list:
- OCEL 2.0 XML: fromisoformat, then fuzzy `dateutil.parser.parse` (today's
  date for missing fields, a weekday without a day moved on, two-digit
  years around today's year, GMT+3 three hours west), whose errors are
  raised in document order; an empty date element raises pandas'
  TypeError; naive next to aware event times raise the sort's error
  (EB1-1). EA's binary differed on 174 of 200 fuzzed files; now 0 of about
  1,700 XML files;
- OCEL 2.0 JSON: date attributes and change values through non-fuzzy
  dateutil (text kept on any error), a naive change value stamped UTC only
  when its field's change column is all naive datetimes (EB1-2); 0 of
  about 1,900 JSON files;
- `Invalid isoformat string` quotes the text as Python's repr (EB1-3);
- SQLite: the event time column as read_sql types each table and
  `pd.to_datetime(utc=True, format="mixed")` reads an object column
  (numbers as nanoseconds, texts element by element, today's date for a
  time without a date); a column of numbers, or one that raises, keeps its
  values and sorts by them (numbers first, then texts as Python sorts
  them, NULL last) (EB2-1). The first fuzz differed on 89 of 136 files;
  now 1,093 files read alike besides texts beyond ASCII;
- Fresh seeds 2026122100-11: one call, the declared struct cast race
  (Polars table CJ 2026122103 case 199).

In the suite: `minera_review_eb1_tests` with
`docs/ocel-dateutil-eb-bank.tsv.gz` (11,977 texts) and
`docs/ocel2-dateutil-eb-bank.tsv.gz` (1,400 documents), and
`minera_review_eb2_tests` with `docs/ocel2-sqlite-time-eb-bank.tsv.gz`
(1,179 files). The dateutil model, the file fuzzers, the SQLite probes and
bank generator and the fresh banks are in the project files under
`review-harness/round-eb/`.

Full suite 622 of 623 passed on the first run; the one failure was
`minera_ocel_xml_tests`, which held an object attribute time of "0" (pm4py
keeps it as text) and a date value with newlines (pm4py reads it), both
fixed in the XML reader and the dateutil model; after the rebuild the 49
OCEL, OCDFG and review tests passed. The workbench fixture replay (run
before that fix) gave 2,673 responses unchanged. Python script and
frontend tests not rerun (scripts and frontend unchanged). The device
check on the Mac was skipped (the Mac was not linked).

Round EC re-reviewed EB (Review EC1). EB's CI run 44325631910: green on
five jobs; the Windows build took 30 min (EA's 62 min did not recur) and
its ctest failed `minera_review_eb2_tests` on 9 bank files whose kept
column holds a REAL: winsqlite3 spells it with 17 digits where SQLite 3.45
spells 15. The reader now formats REAL cells itself (EC1-5).
EC took the CSV leftovers and the classic OCEL readers from EB's list:
- CSV timestamps: time-first texts on today's date, "not likely a
  datetime" numbers below 1000 and lone letters, quarters, years 1 to 9999
  in microseconds with the nanosecond passes, offsets of a day or more
  (EC1-1). Model 0 of 21,000 fresh and 149,000 corpus columns, C++ 0;
- SQLite: EB's own branch for `format="mixed"` raised on `2024.5` and read
  time-first texts with unknown zones, weekdays or invalid days that
  pandas refuses (6 of 132 files); it now shares the CSV element reader
  (EC1-2); EB's bank then caught signed numbers that are no ISO text and
  integers past 18 digits, which the first port had put outside the model
  (both raise in pandas); now 907 plus 554 fuzzed files read alike besides
  negative years (declared) and the harness's ISO spelling of kept
  nanosecond texts;
- fresh seeds 2026122200-11: two calls, the declared struct cast race
  (Polars table CK 2026122201 case 267, CJ 2026122203 case 424) (EC1-3);
- OCEL 1.0 XML: every child keyed `omap`, whatever its tag, read by its
  elements' value attributes, one relation per object with its last
  listing's qualifier (EC1-4); EB's binary differed on 17 of 40 fuzzed
  files, now 0 of 1,000. Classic JSON date values matched on the probes.

In the suite: `minera_review_ec1_tests` with
`docs/ocel-csv-timestamp-ec-bank.tsv.gz` (6,000 columns), and
`minera_review_eb2_tests` now also replays
`docs/ocel2-sqlite-time-ec-bank.tsv.gz` (1,181 files). The model, the
C++ driver, the SQLite fuzzer and bank generator, the omap fuzzer and the
classic XML probes are in the project files under `review-harness/round-ec/`.

Full suite 624 of 624 passed on the first run (before EC1-5 and the
signed-text cases; after them the EB2 and EC1 tests and the OCEL, SQLite
and review E tests passed again). The workbench fixture replay gave 2,673
responses unchanged. Python script and frontend tests not rerun (scripts
and frontend unchanged). The device check on the Mac was skipped (the Mac
was not linked).

Round ED re-reviewed EC (Review ED1). EC's CI run 44345934164: green on
five jobs; the Windows build took 58 min and its ctest failed
`minera_review_eb2_tests` again on 13 bank files: winsqlite3 spells a REAL
with 17 digits through `sqlite3_snprintf("%!.15g")` too, so EC1-5 did not
hold there. The SQLite reader now formats REAL cells itself
(`sqlite_real_text.hpp`, 15 digits rounded half up from the exact value,
SQLite 3.45's shape; ED1-7). ED took the OCEL 1.0 XML shapes EC declared and probed the classic JSON
reader:
- OCEL 1.0 XML, events and objects whose id is a keyed child: every child
  of `<events>`/`<objects>` is an event/object whatever its tag, children
  count by their exact key whatever theirs, the last of a repeated key
  wins (ED1-1); values read by tag (`float` in the lowercase tag is
  `float()`, else `date` is `fromisoformat`, else the text or `"None"`;
  `<int>`, `<boolean>` and `<string>` times stay texts) (ED1-2); the time
  column sorts as pandas sorts an object column, dates or floats first,
  texts by code point, missing last, dates next to floats raise (ED1-3,
  `sort_ocel_xml_events`); vmap lists merge last-wins, an object keeps an
  ovmap key's first value and a repeat is an object change at its
  `timestamp` attribute (ED1-4); values raise in document order (ED1-5).
  EC's binary differed on 79 of 153 probes and 151 of 600 fuzzed files;
  now 12 probes (declared: keyless values, vmap `ocel:*` overrides, a
  processing instruction in a vmap) and 0 of 4,500 fuzzed files. The fuzz
  caught the first port refusing an `ocel:type` change of an object the
  filtering drops; the refusal now waits for the filtering;
- fresh seeds 2026122300-11: two calls, the declared struct cast race
  (Polars table CJ 2026122303 case 380, CK 2026122301 case 219) (ED1-6);
- classic JSON (kept for EE; native's transport format, so a change there
  needs care): `str()` of non-string activities, null activities dropping
  the event, non-string or string omaps, `KeyError` on a missing
  `ocel:vmap`/`ocel:ovmap`, ovmap `ocel:type` and vmap `ocel:activity`
  overrides, list/dict/big-int/NaN values, a time `"x"` raising.

In the suite: `minera_review_ed1_tests` with
`docs/ocel-xml-reference-ed-bank.tsv.gz` (1,336 files) and the ED1-7
REAL texts. The probes, the fuzzer and comparer, the bank generator, the
JSON probes, the REAL text check against SQLite and the fresh banks are in
the project files under `review-harness/round-ed/`.

Full suite 624 of 625 passed on the first run; the one failure was
`minera_ocel_xml_tests`, which held an `<int>` value as an integer (pm4py
keeps the text); after the fix the 40 OCEL, OCDFG, OCPN and review E tests
and 17 recheck, server and import tests passed, and after ED1-7 the 66
OCEL, SQLite, review E, recheck, server, import, connector and history
tests. The workbench fixture
replay gave 2,673 responses unchanged. Python script and frontend tests
not rerun (scripts and frontend unchanged). The device check on the Mac
was skipped (the Mac was not linked).

Round EE re-reviewed ED (Review EE1). ED's CI run 44367108210: the
verifier, Python, frontend and Linux jobs green, the macOS job cancelled,
and the Windows ctest failed `minera_review_eb2_tests` again on 13 bank
files (9 + 4). Those were columns of TEXT affinity: the test bound a
double, so SQLite itself turned it into text at insert, and winsqlite3
spells it with 17 digits. The reader was right (it reads the stored text
as it is); the test now stores the text SQLite 3.45 makes of such a REAL
(EE1-8). EE took the classic JSON reader (`read_ocel_json`), which is also Minera's
own transport between routes:
- the walk: keys and values looked up as Python looks them up, so a
  missing `ocel:vmap`, `ocel:ovmap`, link qualifier or global scope raises
  its `KeyError` and a wrong type its `TypeError`/`AttributeError`; event
  times read by `fromisoformat` while walking (EE1-1);
- record frames: dtypes over all rows (int next to float or missing is
  float, uint64-sized ints too), activities and types as `str()` of the
  cell, non-str omap entries name nothing, a str omap is read by
  character (EE1-2);
- vmap/ovmap `ocel:*` overrides take the frame cell, the relations frame
  keeps the own value; the time column's sort rules (EE1-3);
- the reference filtering: per-frame drop, events/objects kept by the
  remaining relations, then the relations; none left empties all (EE1-4);
- object changes as a record frame (first entry a dict, `dict(entry)`),
  ids as str, the object's type, column-wide raw times, value dtypes over
  all change rows (EE1-5); the scopes required (EE1-6);
- the transport: a log holding an event time read_ocel_json cannot read
  travels with `"minera:unreadableEventTimes": true`; other transports read
  as files do (int columns with a float or missing cell come back as
  floats) (EE1-7).
Main differed on 244 of 456 probes and 301 of 800 fuzzed files; now 0 of
456 and 0 of 4,000 (seeds 1-10) besides declared refusals of rows native
cannot hold (lists, dicts, big ints, override cells, change rows without a
str id/time/field) and two messages.

In the suite: `minera_review_ee1_tests` with
`docs/ocel-json-reference-ee-bank.tsv.gz` (2,056 files). Handwritten
classic documents in four tests and the CLI fixture `ocel_split.json` now
hold what read_ocel_json needs. The probes, the fuzzer, the comparer, the
bank generator and the dump driver are in the project files under
`review-harness/round-ee/`.

Round EF re-reviewed EE (Review EF1). EE's CI run 44383563142 on
f255ebca: the verifier, Python, frontend, Linux and macOS jobs green; the
Windows build passed and its ctest (`minera_review_eb2_tests` with EE1-8)
was still running when EF pushed, so check it with EF's own run. EF fed
the logs EE now reads differently through 38 OCEL routes (`ef/rt/rfz.py` classic JSON fuzzer, `ef/rt/rsw.py` route
sweep reading the file afresh for every pm4py call, since pm4py's
functions change the OCEL in place). Main differed on 115 of 7,800 calls
(descriptions, flattenings) and the probes found more:
- both descriptions raise `KeyError` on the first event without relations
  in (time, activity, id) order and, with timestamps, on the first object
  without events by id (EF1-1); one instant spelled `Z` and `+00:00` is
  one time there (EF1-2);
- the flattening leaves NaN out and keeps infinities as floats (native
  refused the log) (EF1-3);
- the compatibility event and object features and `ocel_o2o_enrichment`
  raise `KeyError` on events and objects without relations (first or last
  as each graph walks them); native skipped them (old OC6 declaration)
  (EF1-4);
- `read_ocel_csv` without an object table lists the objects type by type
  in type-column order (set iteration within a type); native listed them
  by first appearance, which reordered the per-type feature columns
  (EF1-5).
After: 0 of 11,400 (seed 1) and 0 of 7,600 (seed 2) calls differ; CSV
transports 8 of 3,040 (80 files), all the flattening's trace order within
a type, which follows the hash seed in pm4py (declared). SQLite transports
with a blanked event time: 89 of 1,406 calls differ, all on NaT times (kept
for EG, see the first item below).

In the suite: `minera_review_ef1_tests` (hand cases); no other test
changed. The full ctest passed 627 of 627 on the first run, the fixture
replay gave 2,673 responses unchanged. The fuzzer, the sweep, the comparer and the probes are in the project files
under `review-harness/round-ef/`.

Round EG re-reviewed EF (Review EG1). EF's CI run 44395233990 on
35655a0e: the verifier, Python, frontend and macOS jobs green, the Linux
and Windows ctest steps still running when EG pushed (check them with EG's
own run); EE's Windows ctest (run 44383563142) passed. EG took the event
times pm4py holds as NaT (OCEL 2.0 SQLite `''` or NULL times, which the
server's import sends on as transports with
`"minera:unreadableEventTimes"`) through the 38 routes (`eg/sq/sgen2.py`
writes 187 SQLite files in seven NaT shapes; `eg/rt/rsw.py` with
`READER=sqlite` and `STRICT=1`, which now compares messages too):
- both descriptions and the LLM OCEL context sort NaT last and print
  `NaT`, leave it out of the lifecycles, and raise `ValueError: NaTType
  does not support timestamp` on an object whose events all have one
  (EG1-1);
- the compatibility event features raise it on any NaT time, the object
  features on such an object (EG1-2);
- the OC-DFG measures a pair with a NaT end as `0` (EG1-3);
- the ancestors/descendants split gives no direction to a pair where
  either object starts at NaT (EG1-4);
- the writers: classic XML and JSON leave the time out, OCEL 2.0 XML writes
  `NaT`, OCEL 2.0 JSON raises `KeyError: 'ocel:timestamp'`, OCEL 2.0 SQLite
  raises `KeyError: 'ocel_time'` for a type whose events all have one, else
  stores NULL (EG1-5).
It also read the OCEL 1.0 XML vmap `ocel:eid`/`ocel:activity`/
`ocel:timestamp` entries as pm4py does where a native OCEL can hold the
result (equal values; empty, missing or unrelated ids and empty
activities drop the row before the filtering), refusing the rest (EG1-6).
Main differed on 89 of 1,406 calls on EF's SQLite files; now 0 of 1,406,
and 1 of 7,106 on the 187 new files (a declared writer coercion). Relations
naming missing entities in OCEL 2.0 JSON, XML and SQLite files: 0 of 7,448
calls differ (no change needed).

In the suite: `minera_review_eg1_tests` (hand cases);
`minera_review_ee1_tests` now expects a download to leave an empty time
out (EG1-5) and one line of the round-ED XML bank changed (EG1-6). The
full ctest passed 627 of 628 on the first run (the EE1 case, fixed), the
fixture replay gave 2,673 responses unchanged. The generators, the sweep, the
comparers and the probes are in the project files under
`review-harness/round-eg/`.

Round EH re-reviewed EG (Review EH1). EG's CI run 44405676226 on
86d06261: the verifier, Python, frontend and macOS jobs green, the Linux
and Windows ctest steps still running when EH pushed (check them with EH's
own run); EF's run 44395233990 finished green (Linux and Windows ctest
passed). EH took NaT event times from the OCEL CSV reader (`eh/csv/
cgen3.py`, 240 files in seven shapes with naive, UTC and offset times) and
the OCEL 1.0 XML reader (`eh/xg/xgen.py`, 71 files with missing timestamp
elements) through EG's routes (`eh/rt/rsw.py`, now with the CSV download)
and the time-based routes rsw leaves out (`eh/rt/xsw.py`):
- `read_ocel_csv` reads empty cells, NA spellings and NaT spellings as
  NaT, sorted last; native refused them (EH1-1); it sorts the relations by
  time too (EH1-2);
- the edge association performance measures a NaT pair as `0` (EH1-3);
  OCPN puts NaT events last in its traces (EH1-4);
- an OCEL 1.0 XML file without any timestamp element gives pm4py a time
  column of None, with other errors than NaT in the time routes; native
  marks such a log (`"minera:noneEventTimes"`) and raises them (EH1-5);
- every pm4py writer leaves out events and objects without relations,
  with their O2O links and changes; the native file writers now do too,
  the transport keeps them (EH1-6);
- a NaT time keeps the column's shared offset in the downloads, the
  descriptions print a CSV import's times in its offset, and the CSV
  download writes a NaT time as an empty cell (EH1-7).
Fresh OCEL 2.0 SQLite NaT files (seed 7): 0 of 3,621 calls differ. The
counts for the CSV and XML files are in Review EH1. OCEL 1.0 XML keyless
values and the vmap entries whose row stays stay declared.

In the suite: `minera_review_eh1_tests` (hand cases);
`minera_ocel_csv_tests` and `minera_review_ea1_tests` now read an event
without a time as NaT (EH1-1). The generators, the sweeps, the comparers
and the probes are in the project files under `review-harness/round-eh/`.

Round EI re-reviewed EH (Review EI1). EH's CI run 44422565629 on
b63e6875: the verifier, Python, frontend and macOS jobs green, the Linux
and Windows ctest steps still running when EI pushed (check them with EI's
own run); EG's run 44405676226 finished green. EI took NaT and None event
times through the routes rsw and xsw leave out (`ei/rt/esw.py`: the LLM
contexts, OCEL conformance, the feature families one at a time, OCPN
playout) on fresh seeds of EH's banks and on CSV files mixing offsets
(`ei/csv/cgen4.py`), and spoiled the times of OCEL 2.0 JSON and XML files
(`ei/o2/ogen.py`, `sfz.py`, `sfz2.py`):
- the LLM OCEL context is now the description route, so it prints a CSV
  import's times in its offset and raises on events without relations
  (EI1-1); the LLM OCEL features raise in the features' order with
  pm4py's `KeyError` (EI1-2);
- the OC-DFG description (also without header) and OC-DFG conformance
  raise the performance's `TypeError` on a time column of None (EI1-3);
  OTG conformance's `KeyError` keeps its type (EI1-4);
- the WIP, related-event, related-activity, neighbor and related-object
  feature families raise as the families they take do (EI1-5);
- the OCEL 2.0 JSON reader walks the file in pm4py's order, requires the
  qualifier and raises pm4py's `KeyError`/`AttributeError` (EI1-6); the
  OCEL 2.0 XML reader raises dateutil's message on an event without a time
  (EI1-7).
Imports of the 420 mixed-offset and NaT CSV files, 71 XML and 71 SQLite
files: 0 differ. After the fixes the new sweep differs only on the
declared hash-seed WIP order (14 calls); the counts are in Review EI1.

In the suite: `minera_review_ei1_tests` (hand cases);
`minera_ocel_tests` now gives its OCEL 2.0 JSON relationships a qualifier
(EI1-6). The full ctest passed 629 of 630 on the first run (that test,
fixed), the fixture replay gave 2,673 responses unchanged. The generators,
the sweeps, the comparers and the probes are in the project files under
`review-harness/round-ei/`.

Round EJ re-reviewed EI (Review EJ1). EI's CI run 44436185156 on 070218f:
all six jobs green. EJ wrote OCEL 2.0 JSON logs with declared attribute
types, object changes and O2O links (`ej/g2/o2gen.py`, seed 51) and had
pm4py write them as XML and SQLite, spoiled values and change times in
300 more (seed 52), swept the routes with `ej/rt/rsw2.py` (EH's sweep plus
O2O links and changes) and EH's/EI's xsw and esw, and fuzzed the XML and
JSON files (`ej/g2/xfz.py`, also reordering and splitting sections, and
`jfz.py`):
- the OCEL 2.0 XML reader walks the sections in document order and reads
  each value by the type declared so far (only `float` and `date`
  convert), an object value before its time, an event time before its
  values, with pm4py's messages (EJ1-1); a relation to an object not read
  yet is dropped, and a date declared after its values is text (EJ1-2);
- flattening keeps declared dates as `<date>` (EJ1-3); the OCEL 2.0 JSON
  reader's integer columns with a missing cell (dropped rows included)
  read as floats (EJ1-4), and so do the SQLite reader's when a change row
  leaves them NULL (EJ1-6);
- both OCEL 2.0 writers list an object's changes in log order (EJ1-5);
- OCEL 2.0 JSON: a static value's time is not read, a change without a
  time is NaT, kept between routes, and the writers treat it as pm4py's
  do (EJ1-7).
After the fixes the clean banks differ only in the declared date
spellings of the writers (rsw2 198, 200 and 0 of 3,900 calls for JSON,
XML and SQLite; xsw and esw 0); the XML fuzz only on nameless values and
mixed date columns (declared). The spoiled bank's other route differences
are open for EK (below).

In the suite: `minera_review_ej1_tests` (hand cases);
`minera_ocel_xml_tests` and `minera_ocel_tests` follow EJ1-1 and EJ1-4.
The full ctest passed 629 of 631 on the first run (those two, fixed), the
fixture replay gave 2,673 responses unchanged. The generators, the sweeps, the comparers
and the probes are in the project files under `review-harness/round-ej/`.

Round EK re-reviewed EJ (Review EK1). EJ's CI run 44451181228 on
5a11fbdf: all six jobs green. EK fuzzed the writers on native's and
pm4py's frames (`ek/w/wgen.py`, seed 81), swept the spoiled JSON bank
through every route with its xsw and esw, and spoiled the clean SQLite
files (`ek/s/sgen.py 53 300 b53`):
- the writers type and spell each column by its pandas dtype (datetime64
  `...Z`, an object column's dates as isoformat, declared types per owner,
  SQLite TIMESTAMP/TEXT/REAL), leave null values out, and a float-declared
  `"nan"` reads as a missing cell (EK1-1);
- the OCEL 2.0 SQLite reader raises pm4py's structure errors in pm4py's
  order: missing tables (pandas.read_sql's message), NULL types (the sort's
  TypeError in CPython's operand order), unmapped types (KeyError, the
  empty type too), NULL map entries, later map rows winning, the relation
  tables required (EK1-2);
- write_ocel2_xml raises AttributeError on a change time pm4py kept as
  text (or a missing one before it), pandas' NaT strings are missing
  times, and write_ocel2_sqlite writes them as NULL (EK1-3);
- the object graphs' KeyError, the OCEL conformance ZeroDivisionError and
  the LLM text of a log without objects (EK1-4).
EJ's "filters raise tz_convert on +25:00 change times" was a harness bug;
object changes reach no route beyond the writers and the filters'
propagation. After the fixes: b51 imports 0, b51 routes JSON 159 (the
declared fraction), SQLite 0 of 3,900; the spoiled JSON bank 349 of
10,062 (EJ: 866), xsw 0 of 3,354, esw 0 of 4,128; the spoiled SQLite
bank's imports 0 of 300, its routes open for EL (below).

In the suite: `minera_review_ek1_tests` (hand cases, its SQLite cases
checked against pm4py); `minera_ocel_sqlite_tests`, `minera_ocel_tests`
and the four object graph module tests plus
`minera_ocel20_computation_module_tests` follow EK1-2 and EK1-4.
The full ctest passed 632 of 632 on the first run, the fixture replay
gave 2,673 responses unchanged. The generators, the sweeps, the
comparers and the probes are in the project files under
`review-harness/round-ek/`.

Round EL re-reviewed EK (Review EL1). EK's CI run 44464940337 on
ae1eb473: all six jobs green. EL swept the spoiled SQLite bank
through every route (`ek/run1.sh` with `O=el4`):
- an event or change time column pandas cannot convert keeps its Python
  values (`OcelRawTime`: str, int, float, NaN; the transport's
  `minera:rawTime`), the change frame's column is read and sorted like the
  event column (EL1-1);
- the routes raise pm4py's errors on such a column: the timestamp filter,
  descriptions and abstract_ocel, the feature families, OC-DFG performance
  and its LLM text and conformance, measured edge associations, flatten
  (EL1-2), and the writers write or refuse its values as pm4py's do
  (EL1-3);
- write_ocel2_sqlite converts each type table's times with errors="coerce"
  (the first text's format, numbers first raise on an aware text), a type
  whose times are all missing raises `KeyError: 'ocel_time'` (changes
  too), and a change of the empty field is written (EL1-4);
- write_ocel2_xml raises KeyError on a change field that is no column
  (EL1-5).
After the fixes (`ek/el4`): imports 0 of 300; rsw3 257 of 10,101 (EK:
586), 250 of them in the 21 files with duplicate rows (declared), then
four flattened nanosecond times and the raw split (declared) and one
file's change value typed as a number in the OCEL 2.0 XML and SQLite
downloads (s53_0172, open below); esw 72 of 4,144 (EK: 136) and xsw 71
of 3,367 (EK: 92), all in duplicate-row files.

In the suite: `minera_review_el1_tests` (hand cases of EL1-1 to EL1-5).
The full ctest passed 633 of 633 on the first run (24 min at -j2), the
fixture replay gave 2,673 responses unchanged.
The generators, the sweeps, the comparers and the probes are in the
project files under `review-harness/round-el/`.

Round EM re-reviewed EL (Review EM1). EL's CI run 44479148273 on
b964b005: all six jobs green, the Linux, macOS and Windows ctest included. EM swept EJ's JSON bank b52, EK's SQLite banks b51
and b53 and a new spoiled SQLite bank b54 (`em/s/sgen2.py 54 300`: whole
time columns of numbers, NULL first, text first, raw change times, change
cells in other columns) through rsw3, esw and xsw (`ek/runE1.sh`,
`O=em2`):
- the writers type and order a frame's columns over every row the frame
  was built from, rows dropped later included (`OcelFrameHint` per events,
  objects and changes frame; the transport's `minera:frameHints`), and
  the filters keep those types (EM1-1);
- write_ocel2_xml prints a naive change time with "+00:00" (EM1-2);
- a change row's cells in other fields' columns type those columns (the
  changes hint), also through the transport (EM1-3);
- a numeric change-time column (every object table int64, or int64 and
  float64, by read_sql over all its rows) keeps its numbers, sorted by
  value (EM1-4);
- the flatten route prints nanosecond times (EM1-5), and the description
  of a raw column takes the first object's last event with a time (EM1-6).
After the fixes (`ek/em2`): imports 0 of 100 (b51), 0 of 300 (b53, b54),
42 of 300 (b52, the strict JSON refusals as before); rsw3 b52 331 of
10,062 (EL's server: 404; the rest EK's declared datetime fractions and
float-declared "inf" texts), b53 255 of 10,101 (EL: 260; 253 in the
duplicate-row files, the raw split, s53_0172's SQLite download), b51 0
of 3,900, b54 21 of 10,491 (EM before its b54 fixes: 130; a NULL changed
field that duplicates an object, the raw split, the OC-DFG pair on a raw
column); esw b53 76 of 4,144 and xsw 71 of 3,367 (unchanged, duplicate-row
files), b54 esw 8 of 4,304 and xsw 5 of 3,497 (the same two files).

In the suite: `minera_review_em1_tests` (hand cases of EM1-1 to EM1-4 and
EM1-6, the SQLite ones checked against pm4py); `minera_review_el1_tests`
follows EM1-6. The full ctest passed 633 of 634 on the first run (the EL
test's description error; fixed, then passed), the fixture replay gave
2,673 responses unchanged.
The generators, the sweeps, the comparers and the probes are in the
project files under `review-harness/round-em/`.

Round EN re-reviewed EM (Review EN1). EM's CI run 44495021150 on
ab04399f: all six jobs green, the Linux, macOS and Windows ctest included.
EN swept EJ's JSON bank b52, the SQLite banks b51, b53 and b54 through
rsw3, esw and xsw (`ek/runE1.sh`, `O=en2`), a new spoiled OCEL 2.0 XML
bank b55 (`en/x/x2gen.py`: o2gen logs written as OCEL 2.0 XML keeping
unrelated events and objects, with value texts of other kinds;
`en/x/runx.sh`, `O=x2`) and a column-order sweep of every download
(`en/o/osw.py`, `en/o/runo.sh` on b51 as SQLite and as OCEL 2.0 JSON):
- read_ocel2_sqlite keeps a change row's other cells and a `@@cumcount`
  column, which every writer prints or declares (EN1-1);
- write_ocel2_sqlite writes each type's table from its object rows and
  change rows, concatenated, typed per table, only for the types with
  rows (EN1-2; s53_0172's download);
- the frames take their column order from the files (SQLite type tables
  with rows in type order, the OCEL 2.0 JSON file order before the sort),
  and the downloads declare only the used types (EN1-3);
- a NULL time of an object column reads as None (`OcelRawTime::kNull`,
  rawTime `"none"`), which OC-DFG names `NoneType` (EN1-4);
- read_ocel2_xml keeps epoch change texts as str, read_ocel2_json only
  when an unreadable text keeps the column object; write_ocel2_sqlite's
  coercion of such a column raises on a naive text (EN1-5).
After the fixes: imports 0 of 100 (b51), 0 of 300 (b53, b54), 0 of 400
(b55), 42 of 300 (b52, the strict JSON refusals as before); rsw3 b52 331
of 10,062 (unchanged), b53 251 of 10,101 (EM's server: 252), b51 0 of
3,900, b54 21 of 10,491 (unchanged; s54_0163's OC-DFG pair follows the
hash seed in pm4py), b55 272 of 10,530 (EM's server: 447; the declared
fractions, "inf" texts and +25:00 offsets, and 4 open lines); esw and xsw
unchanged on b53 and b54, 0 on b55; column order 0 of 600 (SQLite-read;
EM's server: 41 in 30 files) and 10 of 600 (OCEL 2.0 JSON-read: the CSV
type columns, hash-seed dependent in pm4py, declared).

In the suite: `minera_review_en1_tests` (hand cases of EN1-1 to EN1-5, the
SQLite ones checked against pm4py); `minera_review_el1_tests` follows
EN1-4. The full ctest passed 634 of 635 on the first run (the EL test's
NaN for a mixed column's NULL; updated, then passed), the fixture replay
gave 2,673 responses unchanged.
The generators, the sweeps, the comparers and the probes are in the
project files under `review-harness/round-en/`.

Round EO re-reviewed EN (Review EO1). EN's CI run 44517323874 on
5af1980c: all six jobs green, the Linux, macOS and Windows ctest included.
EO swept the banks b51 to b55 through rsw3, esw and xsw (`ek/runE1.sh`,
`O=eo3`; `en/x/runx.sh`, `O=x3`), a new spoiled OCEL 1.0 bank b56
(`eo/c1/c1gen.py 56 200 b56n`: 200 logs written by hand as OCEL 1.0 JSON
and XML, with unrelated rows, keys only dropped rows hold and mixed value
kinds; `eo/c1/runc.sh`, `O=c4`: rsw3 and osw through both readers), a new
download-content sweep of every bank (`eo/d/dsw.py`, `eo/d/rund.sh`,
`O=d3`: all six downloads against pm4py's writers, compared whole) and a
new playout sweep (`eo/ps/psw.py`, `eo/ps/runp.sh`, `O=p2`: OCPN
extensive playout, OC causal-net playout, OCPN discovery on b53, b54 and
b55):
- an OCEL 2.0 XML date column of aware and naive datetimes is declared
  `date` (EO1-1);
- read_ocel_json's frames keep the dropped rows' columns (CSV columns,
  `attribute-names`, the computed `ocel:global-log`), write_ocel_csv
  keeps all-null columns, read_ocel_xml fails without relations as
  read_ocel2_xml (EO1-2);
- O2O frames without links are float64, so the SQLite `object_object`
  (and an empty log's map tables) are `REAL`; write_ocel_json leaves out
  empty catalogs and `ocel:objectChanges` (EO1-3);
- infer_objects prints ints as floats in columns that are object only
  through dropped rows (`Columns::printed_dtype`, EO1-4);
- read_ocel2_json's changes frame order and naive epoch change times
  (EO1-5); read_ocel2_sqlite's union of the type tables types the changes
  frame, `@@cumcount` NaN for id-less rows, all-NULL columns object
  (`kOcelHintNone`), id-less listings keep their tables (EO1-6);
- change times the OCEL 2.0 readers cannot read stay strs (SQLite NULL,
  write_ocel2_xml's None/NaN message first) (EO1-7); CSV quoting as
  Python's csv module (EO1-8).
After the fixes: imports 0 of 100 (b51), 0 of 300 (b53, b54), 0 of 401
(b55), 0 of 400 (b56), 42 of 300 (b52, the strict JSON refusals as
before); rsw3 b52 328 of 10,062 (EN: 331), b53 252 of 10,101, b51 0 of
3,900, b54 21 of 10,491, b55 265 of 10,530 (EN: 272), b56 0 of 7,800
(JSON) and 49 of 7,566 (XML, the declared c56 files); esw/xsw unchanged on
b53, b54 xsw 3 (EN: 5), 0 on b55; osw 0 on b56; dsw 0 (b51), 18 (b52,
"inf" texts), 38 (b53, duplicate-row files), 5 (b54, s54_0118), 47 (b55,
+25:00 and "inf"), 0 and 12 (b56 JSON and XML, declared); psw b53 19
(duplicate rows and the documented playout limit), b54 3 (the limit), b55
0.

In the suite: `minera_review_eo1_tests` (hand cases of EO1-1 to EO1-8,
the SQLite ones checked against pm4py); `minera_ocel_csv_tests` and
`minera_review_bz1_tests` follow EO1-2 and EO1-3. The full ctest passed
634 of 636 on the first run (28 min at -j2; those two expected a download
without the computed log scope and a transport without
`minera:untypedO2o`; updated, then passed), the fixture replay gave 2,673
responses unchanged.
The generators, the sweeps, the comparers and the probes are in the
project files under `review-harness/round-eo/`.


Round EP re-reviewed EO (Review EP1). EO's CI run 44562956275 on d6411df5:
five jobs green, the Linux and Windows ctest included; the macOS job got
no runner and was cancelled after 15 minutes without running a step. EP
ran b56 through esw and xsw, the banks b51 to b55 through rsw3, esw, xsw
and dsw again (`ek/ep1`, `en/x/x4`, `eo/c1/c5`, `ep/d4`), a structural
fuzz of b56's OCEL 1.0 XML files (`ep/x1/x1fz.py`), a new hand-written
OCEL CSV bank with its objects files (`ep/csv/q1gen.py 57 200 b57`;
`FEAT=` picks the mutations: one bank per feature through
`ep/csv/runf.sh`, and `b58` without the declared ones through
`ep/runq.sh`: imports, rsw3, esw, xsw, osw, dsw), the SQLite banks on the
final server (`ep/runv.sh`) and a new filter-variant sweep of every bank
(`ep/rt/fsw.py`, `ep/runfs.sh`):
- read_ocel_xml raises the object map's KeyError on the first relation to
  an unknown object (EP1-1);
- read_ocel_csv's frames keep the files' columns (all-empty ones, ones only
  dropped rows fill) and the dropped rows (EP1-2);
- the frames' own columns stand where read_ocel_csv and read_ocel2_sqlite
  put them (`OcelFrameHint::reserved`, transport `"reserved"`), which
  write_ocel_csv and the SQLite writer's type tables follow (EP1-3);
- the JSON writers print a shared non-zero offset's wall clock with `Z`
  (EP1-4, formerly declared);
- objects listed but not related: work in progress from lifecycle 0, the
  endpoint KeyError by sorted type, OCPN's KeyError (EP1-5);
- relation-free CSV logs: each feature family raises where pm4py's does,
  the event/object graph raises (EP1-6);
- the object filters at a deeper level keep unrelated listed objects and
  raise KeyError on an unlisted object that shares an event (EP1-7);
- the activity-type matching raises numpy's UFuncTypeError on a
  relation-free JSON read and reads the activity key first (EP1-8).
After the fixes: imports 0 of 100 (b51), 0 of 300 (b53, b54), 0 of 400
(b55), 0 of 400 (b56), 0 of 200 (b58), 0 of 40 per feature bank, 42 of 300
(b52, as before); rsw3 b52 327 of 10,062 (EO: 328), b56 0 and 49 (XML,
c56), b58 12 of 7,800; esw b53 72 (EO: 76), b56 0, b58 13 of 3,200; xsw
b54 5 (s54_0163's raw pair), b56 0, b58 0; osw b58 0; dsw b58 210 of 1,400;
fsw b51 0, b52 36 ("inf" texts), b53 168 (the 21 duplicate-row files), b54
9 (s54_0118), b55 102 (+25:00 and "inf"), b56 0 (JSON) and 38 (XML, c56),
b58 0 of 2,470. Every remaining b58 difference is in a log without an
objects file (the declared hash-seed object order); the feature banks
differ only in the declared features; the XML fuzz only in keyless values.

In the suite: `minera_review_ep1_tests` (hand cases of EP1-1 to EP1-8);
`minera_review_ed1_tests` (and its ED bank, recomputed with pm4py) follows
EP1-1, `minera_ocel_csv_tests` EP1-3 and EP1-4. The full ctest passed 635
of 637 on the first run (24 min at -j2; those two expected the old XML
message and the old JSON download; updated, then passed), the fixture
replay gave 2,673 responses unchanged.
The generators, the sweeps, the comparers and the probes are in the
project files under `review-harness/round-ep/`.

Round EQ re-reviewed EP (Review EQ1). EP's CI run 44587308518 on 652ae8e2:
six jobs green, the Linux, macOS and Windows ctest included. EQ ran two fresh OCEL CSV banks (`eq/csv/q2gen.py 60 200 b60`, every
feature, and `b61` without the declared ones; `TF=1` puts the time column
and the objects' type first more often) through imports, rsw3, esw, xsw,
osw, dsw, fsw and a new sweep of the object filters at levels 2 to 4
(`eq/rt/fsw2.py`: kept and negated, one and several ids and types), one
fresh bank per CSV feature (`eq/csv/f5`, EP's 17 and `unrnat`: objects
listed but not related next to NaT times) through imports, esw, fsw and
fsw2, a fresh OCEL 1.0 bank with objects no relation names (`eo/c1/c1gen.py
59 200`, b59, JSON and XML) through every sweep, SQLite logs written from
time-first CSV logs by pm4py and by native (`eq/sq/mk.py`, b62), native
SQLite downloads of b51, b59 and b61 read back (`eq/sq/mkn.py`,
`eq/runN.sh`), psw on the NaT CSV banks (b58, b60, b61) and fsw2 on b56
and b59:
- an import reads a native SQLite file through its type tables, as
  read_ocel2_sqlite does: values, changes and frame order from the type
  tables, native's own tables (attribute values, declared types, changes,
  globals, event links) only with `OcelSqliteOptions::read_native_tables`
  (EQ1-1);
- filter_ocel_objects at a level above 1 names the smallest of several
  unlisted ids in its KeyError, whatever the standard library (EQ1-2);
- the reference-binding OCPN playout stops past 2,000,000 events instead
  of exhausting the server's memory (EQ1-3, declared: pm4py has no cap).
After the fixes the native SQLite downloads read back give 0 of 600 to
7,410 per sweep (b51, b59, b61, b62: osw, dsw, rsw3; EQ's server gave up to
552 of 700). The other sweeps differ only in the declared classes: b61 0
but for xsw 6 of 2,600 (EP1-6's name order), b60 and the feature banks only
in logs with a declared feature, b59 only in the c59 XML files, b62's pm4py
files 0, fsw2 only in the c56/c59 files and in EQ1-2; psw b58 1 and b61 5 of 800 (the causal-net cap, the declared OCPN type), b60 only in logs with a declared feature.

In the suite: `minera_review_eq1_tests` (hand cases of EQ1-1 and EQ1-2);
`minera_ocel_sqlite_tests` and `minera_ocel_xml_tests` ask for the lossless
read where they check the native round trip, `minera_recheck_regression_tests`
expects the type table's spelling of a change time.
The full ctest passed 637 of 638 on the first run (38 min at -j2; that one
expected the native table's spelling of a change time; updated, then
passed), the fixture replay gave 2,673 responses unchanged.
The generators, the sweeps, the comparers and the probes are in the
project files under `review-harness/round-eq/`.

Round ER re-reviewed EQ (Review ER1). EQ's CI run 44610157456 on 22ead16f:
six jobs green, the Linux, macOS and Windows ctest included. ER read native
SQLite downloads of b52, b55 and b56 back through every sweep (`er/runR.sh`:
0 differences but the comparer's spelling), ran one fresh CSV bank per
feature (`er/csv/fr`) through imports, esw, fsw and fsw2, 774 hand-built
SQLite files (`er/sq/hb1.py` to `hb6.py`, the fuzz banks hb71 and hb72)
through imports, osw, dsw, rsw3 and fsw (`er/runH.sh`), the blow-up OCPN
playouts against `max_traces` (`er/ps/ptime.sh`) and the objects named but
not listed over two hash seeds (`er/nl`):
- read_ocel2_sqlite's tables: views and WITHOUT ROWID tables, `SELECT *`
  with columns by exact name, `ocel:eid`/`ocel:oid`/`ocel:timestamp` in a
  type table, BOOLEAN cells as stored (ER1-1), and its fault order and
  KeyErrors on missing tables and columns (ER1-2);
- an event table's `ocel_changed_field`, `@@cumcount` and `ocel:` columns
  and an object table's `ocel:activity` are attributes, `ocel:type` is not,
  and write_ocel2_sqlite writes them back (ER1-3);
- change rows of an object table without a time column keep a NaT time
  (the writers then raise), or raise KeyError 'ocel:timestamp' when no
  object table has a time column (ER1-4);
- the events frame's activity-first order follows the type tables' column
  union (ER1-5);
- the classic JSON download writes an event's `ocel:` attribute as a key
  of the event entry and leaves an object's out, the classic XML download
  writes the id first and the other fields in the frame's order (ER1-6,
  declared in CU);
- a SQLite export statement error says OperationalError (ER1-7);
- the reference-binding OCPN playout also stops after 120 s (ER1-8;
  `max_traces` and `exists_trace` stop the blow-up log at once);
- a listed type's type table without rows gives the frames its columns as
  object columns, NaN where it lacks one, and an object time column
  (ER1-9; EN1-3's empty tables were unlisted types).
The objects named but not listed stay declared: deterministic in 194 of
205 esw differences over two seeds, but spread over 26+ routes and needing
an unlisted-object notion in the transport. After the fixes the native
SQLite downloads read back differ only in the comparer's spelling, the
CSV feature banks' downloads in none, and the hand-built files only in
the declared classes (duplicate labels, extra or reordered object_object
columns, EK1's duplicate rows and tables without a changed field).

In the suite: `minera_review_er1_tests` (hand cases of ER1-1 to ER1-4,
ER1-6, ER1-8 and ER1-9); `minera_ocel_sqlite_tests` reads a BOOLEAN cell as
stored. The full ctest passed 639 of 639 on the first run (30 min at -j2),
the fixture replay gave 2,673 responses unchanged.
The generators, the sweeps, the comparers and the probes are in the
project files under `review-harness/round-er/`.

Round ES re-reviewed ER (Review ES1). ER's CI run 44636939110 on f7bd7aa4:
six jobs green, the Linux, macOS and Windows ctest included. ES ran a fresh
OCEL 2.0 bank with attributes named like the frames' and tables' own
columns (`es/g/o2rx.py`, r63: JSON and pm4py's SQLite files) and a CSV bank
with such columns (`es/csv/rxgen.py`, rx901) through every sweep
(`es/runS.sh`), compared write_ocel2_sqlite with the native download table
by table on 133 hand-built logs (`es/tools/sqcmp.py`), read 121 hand-built
SQLite files with repeated labels and faulty relation tables (`es/hc/x8`,
`x11`), and ran a fresh structural SQLite fuzz with ES's mutations
(`es/sq/hb3.py`, f85 and f86) through every sweep:
- the classic XML download writes an event's `ocel:` cell as it is (a
  datetime a `<date>`, a str a `<string>`, a number or boolean raises
  lxml's TypeError; ES1-1), the OCEL 2.0 downloads write the changes of an
  `ocel:` field whose static value they leave out (ES1-2);
- the five reference readers but read_ocel_csv lose an event attribute
  `@@index`, read_ocel_csv raises on one (ES1-3), and checks the time
  column, then the id and activity columns, and an objects file's id;
  an objects file's `ocel:qualifier`/`ocel:source`/`ocel:target`/
  `ocel:timestamp` columns are attributes (ES1-4);
- write_ocel2_sqlite writes the change rows of every field, shares the
  changed-field and time columns with object attributes of those names,
  raises pandas' errors on the other repeated labels (`ocel_time`,
  `ocel_id`) in its order, and takes attributes named `ocel:field` and
  `ocel:timestamp` as the changes frame's own columns (ES1-5);
- read_ocel2_sqlite's repeated labels (id or time under both names,
  `ocel:field` next to a changed field) raise pandas' errors in its order
  (ES1-6, declared in ER), and the O2O qualifier column is looked up after
  the change rows (ES1-7).
After the fixes the hand-built logs and files match but for declared
overrides, r63 and the fuzz banks differ only in declared classes (EE1-3's
overrides, EK1's download fractions, the CSV type columns' set order,
numeric event times, extra object_object columns, tables without a changed
field) and harness artifacts (stale transports, types named like times),
rx901 in none but the comparer's spelling, and the regression banks (b63,
u81/u82/u87) as before. Declared, new: a JSON change of `ocel:timestamp`
(pm4py keeps its time), objects CSV files without an id column that pm4py
reads. ER's declared repeated labels are modelled now (ES1-6).

In the suite: `minera_review_es1_tests` (hand cases of ES1-1 to ES1-7).
The full ctest passed 640 of 640 on the first run (35 min at -j2), the
fixture replay gave 2,673 responses unchanged.
The generators, the sweeps, the comparers and the probes are in the
project files under `review-harness/round-es/`.

Round ET re-reviewed ES (Review ET1). ES's CI run 44659629135 on 95a3c65a:
six jobs green, the Linux, macOS and Windows ctest included. ET ran a fresh
OCEL 2.0 bank with object attributes renamed to hard or reserved-looking
names next to change rows of every kind (`et/g/o2rh.py`, h65: JSON and
pm4py's XML and SQLite files), the OCEL 2.0 XML copies of a bank renamed
like the frames' columns (r64), CSV banks with columns dropped, repeated,
mangled, added and renamed in both files (`et/csv/cxgen.py`, cx902, cx903)
and with reserved columns (rx902, rx903), and a fresh structural SQLite
fuzz with repeated labels and broken relation tables next to the other
faults (`et/sq/hb4.py`, f87 and f88), all through every sweep:
- read_ocel2_sqlite renames `ocel_changed_field` to `ocel:field` after
  concatenating the object tables, so a literal `ocel:field` column is
  their changed field when no object table has `ocel_changed_field`
  (ET1-1);
- write_ocel2_json leaves out changes without a value before its NaT check
  (ET1-2);
- write_ocel_xml lists an object related twice to an event twice (ET1-3).
The hard names (ES1-5), the repeated labels (ES1-6), the O2O qualifier
(ES1-7) and the CSV checks (ES1-3, ES1-4) matched; OCEL 2.0 JSON times
with offsets of a day or more read as pm4py reads them (the carried
day-offset item, closed for JSON). After the fixes every bank differs only
in declared classes and the comparer's spelling. Declared, new: an object
attribute named `ocel:field` with changes in an OCEL 2.0 JSON file
(pm4py's change field becomes the value). Harness fix: rsw3 and fsw built
an event's relations as the cross product of `ocel:omap` and
`ocel:typedOmap`, counting an object related twice four times; they read
`ocel:typedOmap` now. rsw3, esw, fsw and fsw2 stopped at the first
reference frame their specs could not read (cx902 lost most of its rsw3,
esw and fsw in the first pass); such a file is one `SPECS` line now.
Pick bank files by a strict glob (`h65/h6403_????.json`): the sweeps leave
`FILE.t.json.norm.json` files that `h65/h*.json` picks up.

In the suite: `minera_review_et1_tests` (hand cases of ET1-1 to ET1-3).
The full ctest passed 641 of 641 on the first run (40 min at -j2, next to
the reruns), the fixture replay gave 2,673 responses unchanged.
The generators, the sweeps, the comparers and the probes are in the
project files under `review-harness/round-et/`.

Round EU re-reviewed ET (Review EU1). ET's CI run 44672028446 on 6ceb8cdd:
six jobs green, the Linux, macOS and Windows ctest included. EU ran a fresh
structural SQLite fuzz with three new mutations next to ET's (`ocel:field`
in place of or next to `ocel_changed_field` across several object tables,
change rows without value, field or time, repeated relation rows:
`eu/sq/hbu.py`, f89 and f90), OCEL 2.0 JSON logs with repeated event and
O2O relations and pm4py's XML and SQLite copies (`eu/g/o2dup.py`, d65), a
CSV bank with ids repeated in relation cells (`eu/csv/dupgen.py`, d904
next to base904) and OCEL 1.0 JSON and XML logs with repeated omap ids
(`eu/c1/c1dup.py`, k91 next to base91), all through every sweep and fsw2,
and the XML day-offset probes (`eu/hc/x1` to `x3`):
- read_ocel2_xml's events frame (every event read, also those dropped
  later) raises the datetime module's offset error when its time column
  mixes an offset of a day or more, which dateutil keeps, with another zone
  or naive times; one shared offset reads (EU1-1);
- find_associations_per_edge keeps a triple once per pair of consecutive
  rows of an object, so an object listed three times by an event gives two
  self-edge triples; the aggregated sets and performances keep one (EU1-2).
ET1-1 to ET1-3 matched on the new banks, the OCEL 1.0 XML, CSV and SQLite
day offsets matched already. After the fixes every bank differs only in
declared classes and the comparer's spelling. Declared, new: the day-offset
values pm4py keeps (a uniform event-time column, date attributes, change
times), whose writers raise or print them by their own rules. Probe pitfall:
`str()` of such a value raises the same error, so a probe that prints frames
fakes a read error (`x1/xsafe.py` reads in a fresh process and prints
fields). `tools/pairdiff.py` diffs a mutated bank against its base bank
(pass the file prefixes with the underscore: `d904_ q904_`; `SWS=fsw2` for
the fsw2 outputs).

In the suite: `minera_review_eu1_tests` (hand cases of EU1-1 and EU1-2).
The full ctest passed 642 of 642 (577 before a container restart, the
other 65 rerun with `-I`), the fixture replay gave 2,673 responses
unchanged.
The generators, the sweeps, the comparers and the probes are in the
project files under `review-harness/round-eu/`.

Round EV re-reviewed EU (Review EV1). EV ran OCEL 2.0 XML files with
event times at offsets of a day or more in random reader positions
(`ev/x1/xgen.py`, x71 to x73, compared by `ev/x1/xcmp.py` without printing
such values), their OCEL 2.0 JSON counterpart (`ev/x1/jgen.py`, j74, j75),
logs at offsets between 14 and 24 hours (`ev/h1/hoff.py`, h65), a fresh
repeated-relation bank (d66 from b64), a fresh OCEL CSV pair (base905,
d905) and a fresh structural SQLite fuzz (f91), all through every sweep
and fsw2. EU1-1 and EU1-2 matched everywhere (420 XML and 270 JSON files,
d66 in three formats: only declared classes). Declared, new:
- routes on a log whose event times share an offset of a day or more
  (read_ocel2_xml keeps it and pandas computes on the instants; native's
  route parsers refuse an offset past 14 hours, so durations, features,
  OC-DFG with business hours and the LLM abstractions differ: EV1-1);
- number qualifiers in SQLite relation tables with a numeric affinity
  (pm4py keeps ints and floats, its JSON writers write numbers, its OCEL 2.0
  XML writer raises for such an O2O qualifier, its event-object graph
  raises on repeated O2O rows mixing them; native keeps text: EV1-2).
pm4py's KeyErrors on CSV objects named but not listed share one model
(the unlisted object whose second relation row comes first, in OC-DFG,
edge associations and OCPN), but native's CSV reader lists such objects
(declared since EP1). Offsets between 14 and 24 hours match in OCEL, XES
and CSV event logs.

EU's CI run 44685379415 on 1f2909c9: the Linux and macOS ctest, the
frontend, Python and verifier jobs green; the Windows ctest still ran when
EV pushed. No source change in EV. The generators, the sweeps, the comparers
and the probes are in the project files under `review-harness/round-ev/`.

Round EW fixed EV1-2 (Review EW1): number qualifiers in OCEL 2.0 SQLite
relation tables. The OCEL model holds a qualifier kind per relation and
O2O row (str, int, float); the SQLite reader types each relation table as
read_sql does (int64, float64 with NULLs as '', object), the classic
transport carries the numbers with `"minera:typedQualifiers": true`, the
JSON writers and the event-object graph print numbers, write_ocel2_xml
raises lxml's TypeError on an O2O number qualifier, and the SQLite writer
declares INTEGER, REAL or TEXT as to_sql does (a float in a TEXT column is
SQLite 3.45's REAL text on every platform). Checked on hand logs
(`ew/q/wq.py`), EV's probe and two fuzz banks (`ew/q/sgen.py`: s101 of
150 files from b65, s102 of 120 from b64) with `ew/q/wcmp.py` (four
writers: 0 differences) and s101 through every sweep (0 differences). EV1-2's
graph TypeError was the sweep's own sort (xsw.py sorts the edges by repr
now; rsw3, fsw and fsw2 read native number qualifiers with str). Declared:
an emptied relation table's column declaration, infinite REAL and BLOB
qualifiers, and JSON inputs with number, boolean or null qualifiers (native
still refuses them; next round).

In the suite: `minera_review_ew1_tests`. The full ctest passed 643 of 643,
the fixture replay gave 2,673 responses unchanged. EU's CI run 44685379415 finished
green in all six jobs; EV's run 44690434272 had four jobs green and both
ctest jobs running at EW's push. The generators, the comparers and the
probes are in the project files under `review-harness/round-ew/`.

Round EX fixed EW1's declared JSON item (Review EX1): number, bool and
null qualifiers in OCEL 2.0 and classic JSON inputs. Both JSON readers type
the relation and O2O qualifiers as pandas types read_ocel2_json's and
read_ocel_json's record frames (int64, float64 with missing qualifiers as
'', bool, object), on the rows those frames are built from (dropped events
and objects, dangling links and unrelated objects' links count; replaced
relations, relations to unlisted objects and typedOmap entries outside the
omap do not). A bool kind joins int and float: the JSON writers and the
event-object graph print bools, write_ocel2_xml raises lxml's TypeError on
an O2O bool, the SQLite writer declares a bool-only column INTEGER. EW's
gaps: the OCEL 2.0 transport now carries `"minera:typedQualifiers": true`
(the routes refused an SQLite import with number qualifiers), and the
frontend's graph validator accepts number and bool qualifiers. The
activity-type matching raises pm4py's UFuncTypeError or TypeError with a
typed qualifier column as a key. Checked on hand files (`ex/p/`), three
fuzz banks (`ex/q/jqgen.py`: j1 of 150 OCEL 2.0 JSON files from b65, k1 of
120 classic from b64, k2 of 150 classic from b65 with dates declared as
strings) with `ex/q/wcmpj.py` (four writers: 0 differences) and through
every sweep (only declared classes). Declared: list, dict, big int and
infinite qualifiers (native refuses them), an object column whose kept rows
share a kind in the matching.

In the suite: `minera_review_ex1_tests`; EE1's bank holds pm4py's reading
of its two classic files with a number qualifier (the old refusal before).
The full ctest passed 644 of 644 (643 on the first run, that bank), the
fixture replay gave 2,673 responses unchanged. EW's CI run 44697743625
finished green in all six jobs. The
generators, the comparers and the probes are in the project files under
`review-harness/round-ex/`.

Round EY re-reviewed EX (Review EY1). EX's fixes matched on fresh JSON
qualifier banks (`ex/q/jqgen.py`: j2, j3 OCEL 2.0 JSON, k3, k4 classic;
wcmpj 0 differences, only declared classes in the sweeps). Fixed: EY1-1,
the dtype of an emptied qualifier column (EW1, declared): the OCEL model
holds a dtype per relation and O2O table (`relation_qualifier_dtype`,
`o2o_qualifier_dtype`), fixed by the readers from every row a frame is
built from (the JSON readers fill a missing qualifier with '' between the
drops of empty ids or types and of dangling or unrelated links;
read_ocel2_sqlite makes a NULL next to numbers object), kept by filters and
splits, set by the enrichments (lifecycle: str, float64 without rows; O2O:
str when it computes links), carried as `"minera:qualifierDtypes"` where
the rows do not show it. write_ocel2_sqlite declares an emptied int64 or
bool column INTEGER and a float64 one REAL, and the activity-type matching
raises numpy's UFuncTypeError on a numpy column with or without rows and
Python's TypeError on an object column of ints. EY1-2: read_ocel_json
ignores a classic file's catalogs, so declared date or time attributes
stay texts (`string`, TEXT, the texts in CSV); a classic transport of a
log with datetimes says `"minera:declaredDates": true`. Checked with
`ey/q/esq.py` (emptied tables: j2 0 of 480, 217 with EX's build; k4 0 of
600; s104 0 of 600) and every sweep on j2, k4, s104 and c57. Declared:
EY1-3, a date-like `<string>` next to a `<date>` of the same key and type
in OCEL 1.0 XML (pm4py keeps the text per element, native types dates per
type and key; it predates EY and needs a per-cell text mark), EV1-1 (no
cheap model).

In the suite: `minera_review_ey1_tests`; `minera_ocel_tests` expects a
classic date declaration as `string`. The full ctest passed 645
of 645 and the fixture replay gave 2,673 responses unchanged. EX's CI run 44709922071
finished green (ctest on Linux, macOS and Windows). The generators, the
comparers and the probes are in the project files under
`review-harness/round-ey/`.

Round EZ re-reviewed EY (Review EZ1). EY's fixes matched on fresh banks
(`ex/q/jqgen.py`: j5, j6 OCEL 2.0 JSON, k5, k6 classic; `ew/q/sgen.py`:
s105 SQLite; `eo/c1/c1gen.py`: c58 OCEL 1.0): the emptied-table comparer
after every filter family (`ez/q/esq2.py`) gave 0 of 7,200 (j5) and 0 of
5,760 (s105), the sweeps only declared classes on j5, k5 and s105. Fixed:
EY1-3, a date-like `<string>` next to a `<date>` of the same key and type
in OCEL 1.0 XML: an attribute carries a per-cell text mark
(`OcelAttribute::text`) set by the OCEL 1.0 XML reader, honoured by the
frame typing, the six writers, the attribute filters, the flattening and
the OC-DFG edge performance, and carried by both transports as
`"minera:textValues"` (c58 dsw 24, fsw 69, rsw3 94 before, 0 after).
EZ1-1, found by the new filter and split download sweeps
(`ez/q/fdsw.py`, `ez/q/ssw.py`) and the split declaration comparer
(`ez/q/esq3.py`): pm4py's filter and split results select rows of their
source's frames, so they keep every column, the column order and the
dtypes; a connected component is a new OCEL built from those rows (a
float64 O2O frame, so object_object REAL, a new changes frame, empty
globals). Filter and split results now record the source rows they lack
in their frame hints (`ocel_frame::hint_subset_rows`, one pass for all
sublogs), and a component's O2O frame, changes frame and globals are a new
OCEL's. EV1-1 stays declared.

In the suite: `minera_review_ez1_tests`, `minera_review_ez2_tests`;
`minera_ocel_exact_filter_controls_tests` expects the filter results' frame
hints, `minera_ocel_transform_tests` compares the split's rows. The full
ctest passed 644 of 647 on the first run (those two and a transport date
declaration in `minera_ocel_tests`, fixed in the transport), then the 191
OCEL, review, OC, server and JSON tests passed; the fixture replay gave
2,673 responses unchanged. EY's CI run 44725615359 finished green in all
six jobs. The generators, the comparers and the probes are in the project
files under `review-harness/round-ez/`.

Round FA re-reviewed EZ (Review FA1). EZ's fixes matched on fresh banks
(`ex/q/jqgen.py`: j7 OCEL 2.0 JSON, k7 classic; `ew/q/sgen.py`: s107
SQLite; `eo/c1/c1gen.py`: c59 OCEL 1.0; `eq/csv/q2gen.py`: q23 CSV; b64x
pm4py-written OCEL 2.0 XML; n59 native OCEL 2.0 XML exports of c59; the
new `fa/c2/c2gen.py` for EY1-3's mark: c61, c62): fdsw, fdsw2 and ssw 0 on
j7, k7, s107 and c59x. The new chain sweep (`fa/q/csw.py`: single filters,
chains of two steps through the transport, capped splits, enrichments,
then the seven downloads) found six classes. Fixed: FA1-1, the exact
filters' literal reading dropped a SQLite change row's other cells and
`"@@cumcount"`; FA1-2, the exact attribute filters raised KeyError on a
column only removed rows held (they consult the frame hints now); FA1-3,
they compared a plain text or a number under a date-declared key as a
datetime (now only a timestamp text, a missing cell or `NaT`); FA1-4,
read_ocel_xml's changes frame holds the time column before the field
column (a frame hint of the classic XML reader, so write_ocel2_sqlite's
type tables match); FA1-5, a `<string>` change under a key a `<date>`
declares stays its text (`OcelObjectChange::text`, set by the OCEL 1.0
XML reader, honoured by the frame typing, the writers and the date
conversion, carried as a change's `"minera:text"` with
`"minera:textChanges"` in both transports); FA1-6, write_ocel2_xml printed
the ints of a float64 column an exact filter's input kept. n59's
differences are the declared typed values of native OCEL 2.0 XML (OC8).
Declared: FA1-7, a `<date>` change under a key no `<date>` of its type
declares is a datetime in pm4py and its isoformat text natively (the
downloads agree; only the rsw3 models of filter results differ). EV1-1
stays declared.

In the suite: `minera_review_fa1_tests`. The full ctest passed 648 of 648 on the first run (37 minutes, -j2); the
fixture replay gave 2,673 responses unchanged. EZ's CI run
44744424886 finished green in all six jobs. The generators, the comparers and the probes are in
the project files under `review-harness/round-fa/`.

Round FB re-reviewed FA (Review FB1). FA's fixes matched on fresh banks
(the new `fb/c3/c3gen.py` for FA1-5's mark on `ocel:`-prefixed keys: c63,
c65, and pm4py's OCEL 2.0 SQLite and XML of c65: w65s, w65x; the new
`fb/o2/o2tgen.py` for FA1-3's rule on OCEL 2.0 inputs, time attributes of
mixed values as JSON, XML and SQLite: o71, o72; CSV q24, q25): imports,
dsw, esw, xsw and rsw3 gave only declared classes. The new `fb/q/xf.py`
(exact attribute filters on every column and value, the exact window on
every event column) and the chain sweep with three-step chains
(`fb/q/csw.py` SET=chain3, `fb/q/crsw.py`) found five classes. Fixed:
FB1-1, the exact filters' literal reading dropped "ocel:"-prefixed vmap
and ovmap keys that read_ocel_json keeps as columns (only the frame's own
columns stay out now); FB1-2, the exact window with timestamp_key compared
timestamp texts where pandas compares the frame's column (int, float and
bool columns raise, a naive datetime64 one raises for its dtype, an object
column raises at its first text, number, boolean or naive datetime, a
null-only column is float64 when a row lacks the key); FB1-3,
write_ocel_csv prints a column of naive datetimes alone as datetime64
(dates alone, or one fraction width); FB1-4, the OCEL 2.0 writers and
write_ocel_json declare a type's static attributes first and the
change-only ones after (after a filter, a column only removed rows fill
no longer places a change-only field first); FB1-5, write_ocel2_sqlite
types a naive date column of the objects frame next to aware changes TEXT
(pandas' concat of naive and aware datetime64 is an object column).
Sampling routes: none in Minera (outside algo.*). EV1-1 stays declared.

In the suite: `minera_review_fb1_tests`. The full ctest passed 648 of 649 on the first run (40 minutes, -j2;
`minera_ocel_event_attributes_module_tests`' typed window contract, so
FB1-2's frame check is the route's, `reference_frame`), then the 26
affected tests passed; the fixture replay gave 2,673 responses unchanged.
FA's CI run 44765194634 finished green in all six jobs. The generators, the
comparers and the probes are in the project files under
`review-harness/round-fb/`.

Round FC re-reviewed FB (Review FC1). FB's fixes matched on fresh banks
(the new `fc/o3/o3gen.py` for FB1-1 to FB1-5 on OCEL 2.0 inputs, "ocel:"-
prefixed names, several object types and change-only fields, as JSON, XML
and SQLite: o81, o82; FB's c66, c67, o73; CSV q26): imports, dsw, xf,
csw3, the key chains (PFX=1) and the event-object graph gave only declared
classes. The new sweeps of the routes that read input with literal names
(`fc/q/em.py` for the OC-DFG edge metrics, `fc/q/atm.py` for
activity_type_matching_exact) and the key chains on CSV and dropped rows
found four classes. Fixed: FC1-1, activity_type_matching_exact with
`ocel:timestamp` as the activity key raises pandas' DatetimeArray + str
TypeError, rows or not, and as the type key per cell (one row: the array,
more: the Timestamp) and not without rows; FC1-2, the edge metrics read
pm4py's frames (event id and activity from the events frame, the object
id from the six-column relations frame, the type from the objects frame,
walking the events frame's ids), so a type key an event attribute shares,
a repeated event id key and an attribute as the object id key follow
pm4py, and a datetime next to texts in an object column rejects with the
other non-string labels; FC1-3, read_ocel_csv's columns are str
(`OcelFrameHint::str_columns`, transport `"strColumns"`), so the exact
window on a column of empty cells keeps no event instead of raising
'float'; FC1-4, the exact window on a naive datetime64 column raises on a
frame without rows too (a filter's result keeps the dtype). Declared: the
edge metrics' typed and missing labels (the string report's bounded
carrier). Carried: the temporary column naming a relations column drops it
from pm4py's result (an empty log's ocel:eid raises nothing then).

In the suite: `minera_review_fc1_tests`; `minera_ocdfg_edge_metrics_controls_tests`
follows pm4py's reading. The full ctest passed 650 of 650 on the first run
(29 minutes, -j2); the fixture replay gave 2,673 responses unchanged. FB's CI run 44787379225 finished green
in all six jobs. The generators, the comparers and the probes are in the
project files under `review-harness/round-fc/`.

Round FD re-reviewed FC (Review FD1). FC's fixes matched on fresh banks
(o81 from `fc/o3/o3gen.py` as JSON and SQLite, CSV q28, the typed-qualifier
banks j92 and s91): type matching with typed keys and separators (atm2)
gave only declared classes, and the statistics route's edge associations
take no selectors. The new sweeps of the OC-DFG discovery's own selectors
(`q/ocd.py`), the exact filters with None, NaT and NaN on full logs and
after filters that keep nothing (`q/xe.py`), and the temporary column on
each relations column through chains, writers and routes (`q/csw.py`
SET=tmp, `q/crsw.py`) found three classes, and atm2 on s91 a fourth. Fixed: FD1-1, the OC-DFG
discovery reads pm4py's frames for its selectors (the relations frame's
six columns, a missing time as the activity a null label that the
associations drop and the start and end activities skip, the edges label
NaT, the activities list None or NaT), and with an events attribute
column as the time column subtracts the frame's Python cells (two numbers
0, other mixes raise with the type names, business hours raise on each
side's replace()), the first pair measured coming from the type and then
the edge whose first couple comes first in the traversal; FD1-2, the
exact filters match None, NaT and NaN as isin does by the column's dtype
(str: all three on missing cells; object: None on None, NaN on missing;
float64: NaN; datetime64: NaT); FD1-3, activity_type_matching drops its
temporary column from the result's relations frame, so the writers, the
per-type filters, the statistics, the features, the split and a second
type matching raise KeyError on it (the object type activities of a
two-row frame give {'c': {'o'}}), and a log pm4py builds anew has its
qualifier column again; FD1-4, read_ocel2_sqlite drops the event-object
rows naming unlisted ids before it fills a NULL qualifier, so such a NULL
leaves a REAL column float64. Declared: an edge's couples as a set (the first
failing pair's hash-seed order), atm2's "nocol" reading of empty SQLite
results, the exact filters after the CSV window that keeps nothing.
Carried: SQLite NULL attribute cells are None in pm4py's object columns
(native leaves the attribute out).

In the suite: `minera_review_fd1_tests`. The full ctest ran 645 of 651 green on the first
run (31 minutes, -j2); the six others held expectations from before FD1-1 to FD1-3 (a "NaT"
text as NaT, the kept temporary column, the joined selectors) or met a degenerate relations
frame the dropped-column check misread, and pass after the updates and the fix, with the 67
OCEL, OC-DFG and review tests that touch the changed routes; the fixture replay gave 2,673
responses unchanged. FC's CI
run 44803654768 finished green in all six jobs. The generators, the comparers and the probes are
in the project files under `review-harness/round-fd/`.

Round FE re-reviewed FD (Review FE1). FD's fixes matched on its banks and
fresh ones (q29 from `eq/csv/q2gen.py 2802 60`, o83 from `fc/o3/o3gen.py
83 60`): the non-exact attribute filters with None (`q/xn.py`), the edge
metrics with selectors, the OC-DFG of chain results (`q/cocd.py`) and the
temporary column on names the relations frame does not hold (`q/csw.py`
SET=tmp2, `q/crsw.py`) gave only declared classes and the carried SQLite
item. The new flattening sweep (`q/fl.py`, both selectors on every events
column, the canonical and `case:` columns) found two classes. Fixed:
FE1-1, the flattening's selectors rename the merged frame's columns as
pm4py's dict literal does (the selected cells as they are under
concept:name and time:timestamp, ocel:activity and ocel:timestamp kept, a
`case:` column moved onto the events, ocel:timestamp as the activity alone
collapsing into the time rename, `case:concept:name` raising the XES
writer's column check); FE1-2 (the carried item), the SQLite reader keeps a
NULL cell as None where read_sql makes the column object (texts next to
numbers, BLOBs, NULLs alone) and leaves it out where the column is str or
float64; FE1-3, the activity of ocel:timestamp after a CSV read whose
times share an offset prints that offset. Declared, unchanged: the CSV
objects-file rows without a type or an id, the hash-seed object order
without an objects file, the first failing pair's hash-seed order.

In the suite: `minera_review_fe1_tests`; `minera_ocel_transform_tests`
follows the rename. The full ctest passed 652 of 652 on the first run (28
minutes, -j2); the fixture replay gave 2,673 responses unchanged. FD's CI
run 44827214734 finished green in all six jobs. The generators,
the comparers and the probes are in the project files under
`review-harness/round-fe/`.

Round FF re-reviewed FE (Review FF1). FE's fixes matched on fresh banks
(o84 from `ff/o4/o4gen.py 84 60`, round FC's OCEL 2.0 generator with any
value null at 25 %, as JSON, SQLite and XML; c85 from round EO's
`c1gen.py 85 60`, OCEL 1.0 JSON and XML with nulls; q30 from
`eq/csv/q2gen.py 2803 60` and q31 from the same generator without the
declared CSV features): the CLI's flattening (`q/cfl.py`) equals pm4py's
and the server route byte for byte, the flattening of chain results
(`q/chfl.py`) gives 0 outside the declared CSV features, the other readers'
nulls agree in the exact and non-exact filters, routes, downloads and
graph sweeps, and q31's chains give 0. The OC-DFG timing and the edge
metrics with the time labels as raw text (`q/emr.py`) and with a duplicate
check under the UTC normaliser (`q/emg.py`) found three classes. Fixed:
FF1-1, a JSON null in a column with no value of its own is NaN where a row
lacks the attribute (pandas makes the column float64; nulls alone stay
object None), so the OC-DFG timed by it measures 0 instead of raising on
None; FF1-2, the event time as the OC-DFG's activity or event id and as the
edge metrics' activity, event id or (datetime64) type is one label per
instant however the file spells it (the first spelling in event order);
FF1-3, a missing event time as the edge metrics' activity is the label
NaT. Declared, unchanged: the edge metrics' typed and missing labels, the
first failing pair's hash-seed order, the CSV writer's type column order,
the download date fractions, the CSV features; the time labels print as the
log spells them (pm4py holds Timestamps and prints none).

In the suite: `minera_review_ff1_tests`. FE's CI run 44840037435: five
jobs green, macOS stopped in `actions/checkout` on a network timeout before
building. The generators, the comparers and the probes are in the project
files under `review-harness/round-ff/`.

Round FG re-reviewed FF (Review FG1). FF's fixes matched on fresh banks
(o86 from `fg/o5/o5gen.py 86 60`, round FF's OCEL 2.0 generator with
null-only attributes and instants respelled, as JSON, SQLite and XML; c87
and c88 from `fg/c6/c6gen.py`, round EO's OCEL 1.0 generator with the same
knobs; q33 from `eq/csv/q2gen.py 2805 60`): the flattening, the exact
filters, the routes and the edge metrics agree outside the declared items
on columns whose nulls share them with missing cells only, the time keys
keep one group per instant on hand-made logs, and q33's NaT times agree in
the OC-DFG's start and end activities and the flattening. A new sweep of
the OCEL features with attribute lists (`q/fat.py`) and probes found three
classes. Fixed: FG1-1, the four attribute lists under pm4py's feature
names follow pm4py's families (float() of each cell with its ValueError and
TypeError, one column per str() of a cell, the objects' TypeError for a
value that is no str in Python set order, the `{id: value}` dicts, the
families' place between the relation counts and the endpoints); FG1-2, a
column of explicit nulls whose events without relations the reader
dropped stays object None when the dropped rows hold nulls (the frame hint
now records them, the transport as a fourth entry); FG1-3, a missing cell
of a datetime64 activity or type column is the edge metrics' label NaT.
Declared, unchanged: the edge metrics' typed and missing labels, the first
failing pair's hash-seed order (every OC-DFG difference one of pm4py's
outcomes under seeds 0 to 39), the download date fractions, the string
family's column order (pm4py's set order).

In the suite: `minera_review_fg1_tests`. The full ctest passed 654 of 654
on the first run (29 minutes, -j2); the fixture replay gave 2,673
responses unchanged. FF's CI run 44850626080 finished green in all six
jobs, macOS included. The generators, the comparers and
the probes are in the project files under `review-harness/round-fg/`.

Round FH re-reviewed FG (Review FH1). FG's fixes matched on fresh banks
(o89 from `fg/o5/o5gen.py 89 60` as JSON, SQLite and XML; c90 and c91 from
`fg/c6/c6gen.py`; q34 from `eq/csv/q2gen.py 2806 60`): the attribute lists
on the related events and related objects and under
`ocel_features_filter_per_type` agree (new sweep `q/fat2.py`), the
dropped rows' nulls agree after filter and split chains (new sweep
`q/onchain.py`, pm4py's chain result pickled against the native result's
transport) and on the OCEL 1.0 JSON reader, and no objects' string family
outcome followed the hash seed in 169 columns. Four classes were fixed:
FH1-1, a missing event time as the edge metrics' event id key is NaT,
which pm4py's groupby drops (the event joins no edge); FH1-2, the exact
time window's `timestamp_key` reads the dropped rows' nulls as FG1-2's
consumers do (a column object None compares nothing); FH1-3, an aware
datetime takes its seed-free slot in the objects' string family set
(CPython's tuple hash of its UTC days, seconds and microseconds), so its
TypeError comes before a float's; FH1-4, the objects' related events and
related activities check the events' attribute families before the
endpoints' KeyError. Declared, unchanged: the edge metrics' typed and
missing labels, the OC-DFG's first failing pair (each difference one of
pm4py's outcomes under seeds 0 to 9 or 0 to 19, the chains' too), the CSV declared
features, the download date fractions, the string family's column order.

In the suite: `minera_review_fh1_tests`. The full ctest passed 655 of 655
on the first run with FH1-1 to FH1-3 (33 minutes, -j2); after FH1-4 the
110 OCEL, feature and review tests passed again and the fixture replay
gave 2,673 responses unchanged. FG's CI run 44860924885 finished green in
all six jobs. The generators, the comparers
and the probes are in the project files under `review-harness/round-fh/`.

Round FI re-reviewed FH (Review FI1). FH's fixes matched on fresh banks
(o92 from `fg/o5/o5gen.py 92 60` as JSON, SQLite and XML; c93 from
`fg/c6/c6gen.py`; c94 from the new `c7/c7gen.py`, c6gen with aware times
carrying nanoseconds and booleans next to 0 and 1; q35 and q36 from
`eq/csv/q2gen.py 2807 60` and `2808 60`): the exact filters, the chains'
filters and window after splits, the features after chains, the attribute
lists (aware datetimes with nanoseconds, OCEL 1.0 XML dates) and the
related objects' families agree outside the declared items. Two new
sweeps, `q/ocd2.py` (the OC-DFG's own event id and time attributes,
`ocel:timestamp` included) and `q/xe2.py` (the exact filters' value lists
mixing None, NaT and NaN with values), and the type matching sweep on the
CSV banks found four classes, all fixed: FI1-1, a missing event time as
the OC-DFG's own event id is NaT (listed among the activities, no edge,
the start and end events from an object's first and last rows with a
time); FI1-2, `ocel:timestamp` as the type matching's type key names
pm4py's operand by the rows with a time (array, scalar, NaTType, or no
error); FI1-3, a CSV log without object references raises numpy's
UFuncTypeError for the float64 qualifier as the activity key; FI1-4,
pandas' isin turns None and NaN into NaT next to datetimes of the
column's kind, so the exact filters match the NaT rows. Declared,
unchanged: the edge metrics' typed and missing labels, the OC-DFG's first
failing pair (each chain difference one of pm4py's outcomes under seeds 0
to 9), the CSV declared features, the type matching's dropped temporary
column ('nocol' in the harness), the string family's set order.

In the suite: `minera_review_fi1_tests`. The full ctest passed 656 of 656
on the first run (25 minutes, -j2); the fixture replay gave 2,673
responses unchanged. FH's CI run 44869187203 finished green in all six
jobs. The Mac was not reachable this round (no
folder access), so no newer reviewer handoff was read. The generators,
the comparers and the probes are in the project files under
`review-harness/round-fi/`.

Round FJ re-reviewed FI (Review FJ1). FI's fixes matched on fresh banks
(o95 from `fg/o5/o5gen.py 95 60` as JSON, SQLite and XML; c96 and c97
from `fg/c6/c6gen.py 96 60` and `fi/c7/c7gen.py 97 60`; q38 from
`eq/csv/q2gen.py 2810 60` with the declared CSV features, and the new q39,
`q2gen.py 2811 60` with `FEAT` leaving those features out, so its CSV
differences need no per-file classing): the OC-DFG with the activity and
event id on a NaT time, its timing and every events column as the event
id agree, and the type matching agrees with every pair of relations
columns as its keys and temporary columns named like relations columns
(hand-made logs with NaT, with events without objects, a degenerate
relations frame and OCEL 1.0 logs without relations) outside the declared
'nocol' reading. Four new sweeps: `q/xe3.py` (the exact filters' value
lists with a cell's datetime at another offset or of the other kind next
to the nulls), `q/xe4.py` (a cell's datetime plus or minus nanoseconds),
`q/xn2.py` (the non-exact filters on datetime columns) and `q/atm3.py`
(the type matching's other columns). They found three classes, all fixed:
FJ1-1, pandas' converted value array of the other awareness matches
nothing at all, NaT included; FJ1-2, the non-exact filters' None matches
by the column's dtype and a datetime cell equals no text; FJ1-3, the
converted array holds its datetimes floored to the column's unit
(microseconds unless a CSV time has more than six fraction digits).
Declared, unchanged: the type matching's dropped relations column
('nocol'), the CSV declared features (q38's remaining differences keep
the same events and differ by the objects a log relates but does not
list), the edge metrics' typed and missing labels, the first failing
pair's hash-seed order, the string family's set order.

In the suite: `minera_review_fj1_tests`. The full ctest passed 657 of 657
on the first run (39 minutes, -j2, next to two sweeps); the fixture
replay gave 2,673 responses unchanged. FI's CI run 44876007645 finished
green in all six jobs. The Mac was not reachable this round (no folder
access), so no newer reviewer handoff was read. The generators, the
comparers and the probes are in the project files under
`review-harness/round-fj/`.

Round FK re-reviewed FJ (Review FK1). FJ's fixes matched on fresh banks
(o98 from `fg/o5/o5gen.py 98 60` as JSON, SQLite and XML; c99 and c100
from `fg/c6/c6gen.py 99 60` and `fi/c7/c7gen.py 100 60`; q43 from
`eq/csv/q2gen.py 2813 60` with q39's features) and four new ones: o102m
(`fk/o6/o6gen.py`, OCEL 2.0 JSON files whose frame has an object column
mixing naive and aware datetimes), o103 (`fk/gen/o7gen.py`, nanosecond
event and attribute times in all three OCEL 2.0 formats), c104
(`fk/gen/c8gen.py`, OCEL 1.0 event times with nanoseconds) and q44
(`fk/gen/q3gen.py`, CSV logs with date-only times, nanoseconds on every
event or one, microseconds and mixed fraction lengths). The chains'
exact filters with xe3's and xe4's lists (`q/onchain2.py`), the mixed
object columns, the non-exact filters after the pre steps, on SQLite's
all-NULL columns and on OCEL 1.0 XML dates, a naive CSV column without
any time and the writers' nanosecond texts (`w/wtx.py`) agree. Two
classes, both fixed: FK1-1, an aware nanosecond time column (CSV with an
offset, SQLite) keeps its unit after the rows that made it so are gone
(dropped by the reader, filtered, split away), so the exact filters'
converted value array keeps its nanoseconds (`Ocel::nanosecond_event_times`,
transport marker `"minera:nanosecondEventTimes"`); FK1-2, found by
running every OCEL route on the nanosecond banks (rsw, xsw, esw, dsw: 0
elsewhere), the edge metrics with business hours read each time at its
microsecond, floored, as pm4py's BusinessHours does (native rounded).
Declared, unchanged: the JSON and OCEL 1.0 XML writers' time spellings
and dropped fractions, 'nocol', the CSV declared features, the edge
metrics' typed and missing labels, the first failing pair's hash-seed
order, the string family's set order.

In the suite: `minera_review_fk1_tests`. The full ctest passed 658 of 658
on the first run (35 minutes, -j2); the fixture replay gave 2,673
responses unchanged. FJ's CI run
44883332593 finished green in all six jobs. The Mac was not reachable
this round (no folder access), so no newer reviewer handoff was read. The
generators, the comparers and the probes are in the project files under
`review-harness/round-fk/`.

Round FL re-reviewed FK (Review FL1). FK's fixes matched on fresh banks:
o105 (`fl/gen/o8gen.py 105 60` with `DUPID=0`: nanosecond OCEL 2.0 logs
whose objects form two connected components, the nanosecond times on one
component's events only) and q46 (`fk/gen/q3gen.py 2815 60`, CSV logs with
nanoseconds and offsets whose objects files list the ids out of order),
through `q/onchain3.py` (onchain2 with the enrichments, the type matching
and every part of the connected components and ancestors splits as chain
steps, then xe4 and the route sweeps), SQLite logs whose nanosecond time
sits on a repeated id or an unlisted row (`t3/mk2.py`) and SQLite files
written by Minera. Four classes, all fixed, three of them found on the
traditional routes with business hours: FL2-1, business hours at the
floored microsecond in the temporal profile, its conformance and the
EventLog statistics (case statistics, service time, case arrival); FL2-2,
an upload's temporal profile with business hours on pm4py's DataFrame
arithmetic, counted on the frame's UTC clock for a CSV upload
(format_dataframe converts an offset), in both behavior route variants and
the conformance too; FL2-3, the heuristics miner's performance with
business hours on that clock (classic and plusplus); FL2-4, found by the
capped ancestors split on q46, the ancestors/descendants split walks the
objects table's order (native took ids in sorted order, so `max_objects`
kept other objects). Declared, unchanged: BK's business-hours DataFrame
profile without pairs (pm4py's ValueError, Minera's empty profile), AP's
repeated SQLite ids, the connected components' hash-seed order, the JSON
and OCEL 1.0 XML writers' time spellings and dropped fractions, 'nocol',
the CSV declared features, the edge metrics' typed and missing labels,
the first failing pair's hash-seed order, the string family's set order.

In the suite: `minera_review_fl1_tests`. The full ctest passed 659 of 659
on the first run (31 minutes, -j2, next to a sweep); the Python script
tests 406 of 406; the fixture replay gave 2,673 responses unchanged. FK's
CI run 44891057199 finished green in all six jobs. The Mac was not
reachable this round (no folder access), so no newer reviewer handoff was
read. The generators, the comparers and the probes are in the project
files under `review-harness/round-fl/`.

Round FM re-reviewed FL (Review FM1). FL's fixes matched and no new class
turned up. FL2-1 and FL2-2 on the other business-hours consumers of an
upload's times: the organizational workbench's network analysis on CSV
uploads with one offset per log or per row and on XES (720 uploads, equal
outside 3 rejected by both), the time analysis routes on 450 XES uploads
whose dates carry their own offsets (equal wherever pm4py returns), the
pandas case filter's performance filter (2,715 cases equal), `/api/mine`'s
DFG performance (250 uploads equal) and the temporal conformance route
(132 logs equal to both variants); pm4py's streaming temporal conformance
and the token replay duration diagnostics count no business hours. FL2-4
on fresh banks whose objects are listed out of id order (o106: OCEL 2.0
JSON, XML and SQLite; c107: OCEL 1.0 JSON and XML; q48: CSV): the
imports, the route sweeps rsw, esw, xsw, dsw and osw and onchain3's
chains show only the declared writers' dropped fractions and
write_ocel_csv's hash-seed type columns. Declared, unchanged: the JSON
and OCEL 1.0 XML writers' time spellings and dropped fractions,
write_ocel_csv's type columns, the CSV declared features, BK, AP, the
connected components' hash-seed order, 'nocol', the edge metrics' typed
and missing labels, the first failing pair's hash-seed order, the string
family's set order.

No code changed, so FL's suite results stand. FL's CI run 44897815468
had five of six jobs green at handoff (the Windows ctest still running:
check it first in FN). The Mac was not reachable this round (no folder access), so no
newer reviewer handoff was read. The generators, the comparers and the
probes are in the project files under `review-harness/round-fm/`.

Round FN reviewed new areas (Review FN1): the raw-log workbenches against
pm4py's EventLog variants on CSV and XES logs with offsets and nanosecond
fractions, non-default business-hour slots, and the Polars DFG with
business hours. Fixed: FN1-1 and FN1-6, the EventLog case statistics,
service time and cycle time read `Timestamp.timestamp()` (nanoseconds
rounded to the microsecond); FN1-2, passed time with the start key
naming the log's own time while the timestamp key names another
attribute; FN1-3, case arrival skips traces whose first event lacks the
key; FN1-4, service time answers stdev, raw values and all with the mean
(`statistics.mean` on the EventLog path); FN1-5, business-hour slots
within a second of each other are one slot, with pm4py's exact merge
expression in all ten consumers; FN1-7, the Polars DFG counts business
time on the UTC clock, raises pm4py's SchemaError where `BusinessHours`
returns an int (whole-number slot bounds count as ints), and takes
Polars' Welford std and linear median on every Polars DFG. Declared:
FN1-8, the raw transport cannot tell a CSV read's pandas Timestamps from
an XES read's datetimes (Minera's EventLog gaps are datetime's; pandas'
scalar total_seconds is an ulp off for about 0.5% of gaps; business
hours read each text's own clock). After the fixes the time analysis,
temporal and timing workbenches agree on every CSV and XES raw-log bank
(exact) and the Polars DFG agrees or raises alike on 140 uploads.

In the suite: the unit tests of service time, case statistics, case
arrival, passed time, cycle time and the DFG tables carry FN1's
reference values. The full ctest passed 659 of 659 on the first run (35
minutes, -j2); the Python script tests 406 of 406 (the 13 query runtime
tests in a venv with duckdb); the fixture replay gave 2,673 responses
unchanged. FL's CI run 44897815468 and FM's run
44901772965 finished green in all six jobs. The Mac was not reachable this round (no
folder access), so no newer reviewer handoff was read. The generators,
the comparers and the probes are in the project files under
`review-harness/round-fn/`.

Round FO re-reviewed FN on fresh raw-log banks (the time analysis,
temporal and timing workbenches, CSV and XES) and on fresh Polars DFG
uploads (`dfg_measure=both`, keep once per case, windows above one), and
swept new areas (Review FO1): the heuristics miner and the OC-DFG with
random non-default business-hour slots, the Polars, pandas and EventLog
batches and minimum self-distance on uploads with offsets (all equal),
and the organizational workbench on raw logs against pm4py's EventLog
variants. Fixed: FO1-1, passed time on an EventLog defaults the start
key to `time:timestamp` and needs no timestamp key on a trace's last
event when the start key is another field; FO1-2, network analysis on an
EventLog follows the frame pm4py converts it to (exact durations; a
missing time is NaT, so business hours raise; zones that differ make an
object column with no `.dt` accessor, or NaN floats without `.replace`);
FO1-3, resource profiles on an EventLog follow the log variant (exact
window comparisons, social position's closed window, wall clocks in
social position and average case duration, TypeError on naive
Timestamps worded by their nanoseconds, KeyError where a metric reads a
missing key, `Timestamp.timestamp()` durations and `statistics.mean`,
the start-key interval rule of the first sorted event), and t1 at or
after t2 is an empty window on both paths. Declared: FO1-4, pm4py's
Polars keep-once dedup order varies between runs (std last ulp); a raw
log's CSV extra date column sorts as text in network analysis; lifecycle
pairing in the log variant's interval metrics (carried). Found and
carried: the frame path's `average_duration_activity` with nanosecond
times (doubles subtracted) and an ulp in some `average_case_duration`
means. After the fixes the organizational workbench agrees on every CSV
and XES raw-log bank where pm4py answers, with pm4py's error texts.

In the suite: the passed time, network analysis and resource profiles
unit tests carry FO1's reference values. The full ctest passed 659 of 659
on the first run (43 minutes, -j2, beside the fixture replay); the Python
script tests 406 of 406 (the 13 query runtime tests in a venv with
duckdb); the fixture replay gave 2,673 responses unchanged. FN's CI run 44909410735
finished green in all six jobs. The Mac was not reachable this round
(no folder access), so no newer reviewer handoff was read. The
generators, the comparers and the probes are in the project files under
`review-harness/round-fo/`.

Round FP re-reviewed FO on fresh raw-log banks and swept new areas
(Review FP1). FO's fixes held (network analysis, passed time, resource
profiles on CSV and XES raw logs, also with lifecycle columns, apart
from the items fixed below). Fixed: FP1-1, resource profiles on uploads
with nanosecond times (the frame path compares its datetime64 cells with
the window exactly, keeps the nanoseconds in its view, takes pandas'
pairwise mean of the exact `Timedelta` seconds in
`average_duration_activity` and `statistics.mean` of the exact durations
in `average_case_duration`; 135 of 5,400 frame results were an ulp to a
microsecond off); FP1-2, the log variant's interval metrics on logs with
lifecycle transitions (pm4py converts the log to intervals when the
first sorted event lacks the start key: FIFO start/complete pairing per
activity, other transitions dropped, intervals sorted by start, and
`IndexError: list index out of range` when, without a start key, a case
converts to nothing); FP1-3, root cause features of a CSV log's dates
keep `str(Timestamp)`'s nanoseconds (XES logs keep microseconds, as
before); FP1-4, the empty averages name pm4py's `StatisticsError`. New
areas: SNA (four log variants) and roles on raw logs with offsets,
nanoseconds and lifecycles are equal apart from the declared joint
activities ulps and networks without connections; root cause on
nanosecond XES logs was already equal; the decision workbench has no
pm4py route counterpart. After the fixes every resource-profile,
root cause, passed time and SNA sweep agrees where pm4py answers, with
pm4py's error texts, on fresh banks too.

In the suite: the resource profiles, token replay root cause and BC1
review tests carry FP1's reference values (four older lifecycle
contracts on EventLogs moved to the log variant's values). The full
ctest passed 659 of 659 on the first run (40 minutes, -j2); the Python
script tests 406 of 406 (the 13 query runtime tests in a venv with
duckdb); the fixture replay gave 2,673 responses unchanged. FO's CI run
44919992483 finished green in all six jobs. The Mac was not reachable
this round (no folder access), so no newer reviewer handoff was read.
The generators, the comparers and the probes are in the project files
under `review-harness/round-fp/`.

Round FQ re-reviewed FP on fresh banks and swept new areas (Review FQ1).
FP's fixes held (resource profiles on uploads with nanosecond times near
the window, CSV and XES; the log variant with lifecycle columns; passed
time and the other timing routes on lifecycle raw logs; root cause on
nanosecond CSV logs with the multiplier, numeric date columns and the
unknown-activity form). FP's CI run 44930288030 finished green in all
six jobs. Fixed: FQ1-1, the one-hot feature names of a CSV log's dates
take `str(Timestamp)` with nine fraction digits when the time has
nanoseconds (`/api/log-to-features`, `/api/features`,
`/api/transform-features`; 61 of 180 comparisons differed); FQ1-2, the
LLM log description marks a CSV log's nanosecond date columns (the
timestamp, other event columns and `case:` columns, which the CSV
importer now flags), so their quantiles print pandas' float64
nanoseconds; FQ1-3, the LLM variants description takes a variant's
durations as differences of `Timestamp.timestamp()` floats, so ties and
their order follow pm4py. New areas without findings: attribute value
lists of nanosecond CSV dates; local diagnostics on raw logs with a
group column (CSV and XES); the SNA pandas variants and roles on CSV
uploads with offsets and nanoseconds; timestamp interleavings and link
analysis on CSV uploads with nanosecond times and offsets; pm4py's
temporal profile, cycle and service time, concurrent activities,
eventually-follows graph, performance DFG and interval features do not
convert lifecycles to intervals, so nothing changes there; the trace
explorer has no pm4py counterpart.

In the suite: `minera_log_to_features_tests` and
`minera_llm_abstractions_parity_tests` carry FQ1's reference values (the
LLM bank of 2,700 calls replays unchanged). The full ctest passed 659 of
659 on the first run (29 minutes, -j2); the Python script tests 406 of
406 (the 13 query runtime tests in a venv with duckdb); the fixture
replay gave 2,673 responses unchanged. The Mac was not reachable this
round (no folder access), so no newer reviewer handoff was read. The
generators, the comparers and the probes are in the project files under
`review-harness/round-fq/`.

Round FR re-reviewed FQ on fresh banks and swept new areas (Review FR1).
FQ's fixes held (one-hot names of nanosecond CSV dates, also with case
date columns only and on XES uploads; the LLM descriptions on CSV and on
XES uploads with nine fraction digits). FQ's CI run 44937354307 finished
green in all six jobs (the Windows ctest, still running at FR's push,
finished green too; checked in FS). Fixed: FR1-1, the CSV frame table types
a `case:` column like an event column (read_csv's int64, float64, bool
or object; UTC dates, datetime64[ns, UTC] with a cell below the
microsecond), so the pandas and Polars batches read a `case:` start,
resource or activity key as pm4py does; FR1-2, the batches log variant
reads `case:X` as the trace attribute X (pm4py's event stream), a NaN
case value one float per trace (its events share a group, other traces'
NaNs do not), where Minera raised `KeyError`; FR1-3, the heuristics
miners on a CSV upload with nanoseconds floor each wait's exact
difference to the microsecond (classic and plusplus), and the timestamp
column named as the start key reads the completion dates; FR1-4, the
Polars DFG with a Datetime(ns) timestamp and a Datetime(us) start
truncates the nanosecond side to microseconds before whole seconds;
FR1-5, on an upload's frame (CSV and XES) a heuristics wait is the whole
seconds plus the microseconds / 1e6 (4.9999970000000005 for 4.999997 s),
and the plusplus sojourn times of a CSV upload subtract
`Timestamp.timestamp()` values. `plusplus.apply_heu_pandas` (the
library's `discover_plus_plus_pandas`) keeps exact nanoseconds and is
unchanged. New areas without findings: profile clustering with date
string features on nanosecond CSV logs (13 declared label ties);
interleavings with odd relation files and nanosecond times (`ODD=1`);
link analysis; the pandas DFG with `case:` and event start keys.

In the suite: `minera_batches_parity_contracts_tests`
(`review_fr_csv_case_columns`), `minera_heuristic_variants_tests`
(`review_fr_nanosecond_waits`) and `minera_dfg_dataframe_tests`
(`review_fr_polars_mixed_datetime_units`) carry FR1's reference values.
The full ctest passed 659 of 659 on the first run (34 minutes, -j2); the
Python script tests passed 406 of 406; the fixture replay
gave 2,673 responses unchanged. The Mac was not reachable this round (no
folder access), so no newer reviewer handoff was read. The generators,
the comparers and the probes are in the project files under
`review-harness/round-fr/`.

Round FS re-reviewed FR on fresh banks and swept new areas (Review FS1).
FR's CI run 44945360743 finished green in all six jobs. FR's fixes held
(the batches with `case:` keys on CSV and XES uploads, `case:st` starts,
the plusplus sojourn times, the DFG with start keys, the heuristics waits
on XES uploads with and without business hours). Fixed: FS1-1, the
plusplus heuristics on a CSV upload order, follow and overlap intervals
at their exact instants (nanoseconds), not at microseconds; FS1-2, the
trace-based correlation reads pm4py's projected log (concept:name and
time:timestamp only): with two or more activities another completion key
or a start key raises `KeyError` (a start key of concept:name a
`TypeError`), which the routes model through
`CorrelationMinerOptions::reference_timestamp_key`; FS1-3, the classic
precede/succeed probability counts the pairs in pm4py's single pass over
the start-sorted lists, which differs from the full count for nested
intervals with a start key; FS1-4, `/api/mine`'s flat footprints read the
upload's frame (a CSV's formatted frame with each row's `case:` cells, an
XES upload's read_xes frame), so the Polars variant sorts by a `case:st`
that varies within a case; FS1-5, the Polars partial order measures whole
seconds (microseconds when the units differ, also in its pair
condition) and pairs by a CSV frame's own `@@index`, and the Polars
concurrency truncates the overlap to whole seconds before comparing it
with 0. Declared: the classic frequency DFGs that still differ are the
LP ties of round BQ (every one at the optimum of pm4py's cost matrix);
corrst.py's stream reference should be the frame's event stream (equal
there). New areas without findings: the pandas partial order and
concurrency; the footprints entire_dataframe, entire_event_log and
trace_by_trace variants with start keys; decision mining with date
attributes (pm4py skips them, so do Minera's tables); no OCEL target in
`/api/log-convert`.

In the suite: `minera_heuristic_variants_tests`
(`review_fs_nanosecond_ordering`), `minera_correlation_miner_tests`
(`review_fs_trace_based_reference_keys`,
`review_fs_nested_interval_precedence`), `minera_dfg_dataframe_tests`
(`review_fs_polars_partial_order_and_overlap`) and the new
`minera_server_review_fs_tests` (the three routes on CSV uploads) carry
FS1's reference values. The full ctest passed 658 of 660 on the first
run (24 minutes, -j2): two older expectations in
`minera_correlation_miner_tests` still held the pairwise precede-succeed
count (pm4py's `get_PS_dur_matrix` gives 0 for A(0, 100), A(1, 2),
B(3, 3) and 2/3, not 5/6, for the rectangular utility log; both updated),
and the new route test read the activities array with the wrong spacing;
both reran green. The Python script tests passed 406 of 406, the fixture
replay gave 2,673 responses unchanged. The Mac was not reachable this
round (no folder access), so no newer reviewer handoff was read. The
generators, the comparers and the probes are in the project files under
`review-harness/round-fs/`.

Round FT re-reviewed FS on fresh banks and swept new areas (Review FT1).
FS's CI run 44954362892 finished green. FS's fixes held
(the plusplus intervals with dense nanoseconds, on XES uploads and with
lifecycle columns; trace-based correlation keys on XES uploads and with
one activity; the footprints with start keys on XES uploads; the Polars
partial order with `dfg_keep_first_following` and `dfg_strict`). Fixed:
FT1-1, the interval eventually-follows on a CSV upload sorts and compares
starts and completions at nanoseconds, and a completion key naming the
CSV's own timestamp header reads the event timestamp; FT1-2, correlation
exact time matching is networkx's minimum weight full matching, i.e.
SciPy's `linear_sum_assignment` with `sys.maxsize` for invalid pairs and
the targets in CPython set order, whose absorption of small costs Minera
now follows (`src/backend/scipy_lsap.hpp`); FT1-3, the classic variants
start each event at time:timestamp when another completion key is given
without a start key; FT1-4, the Polars graph DFG and concurrency sort
with the unstable pdqsort model of round BX (`polars_sort.hpp`), which
shows on frames over 20 rows such as an XES upload's; FT1-5, classic_split
cuts its samples after sorting the exact instants (nanoseconds); FT1-6,
the Polars partial order and concurrency sort a missing start first and
the concurrency skips a missing start or completion in its horizontal
max/min; FT1-7, the plusplus `pandas_mutable` workflow shape on CSV rows
(kept as text) raises pm4py's subtraction or comparison `TypeError`
before the `.dt` error when a case has a completion and a next start.
Declared: the classic frequency DFGs that still differ are the LP ties of
round BQ; the Polars keep-once standard deviation varies in its last
digits in pm4py itself from run to run. New areas without findings: the
CSV dataframe DFG, partial order and concurrency on larger frames;
streaming correlation and footprints take no timestamp keys.

In the suite: `minera_correlation_miner_tests`
(`review_ft_exact_time_matching_assignment`,
`review_ft_classic_default_start_key`,
`review_ft_split_samples_sort_at_nanoseconds`), `minera_overlap_tests`
(`eventually_follows_csv_nanoseconds_reference`),
`minera_dfg_dataframe_tests` (`review_ft_polars_graph_tie_order`) and
`minera_heuristic_variants_tests` (`review_ft_plusplus_pandas_text_columns`)
carry FT1's reference values. The full ctest passed 660 of 660 on the
first run (39 minutes, -j2), the Python script tests passed 406 of 406,
the fixture replay gave 2,673 responses unchanged. The Mac was not
reachable this round (no device served, the folder not approved), so no
newer reviewer handoff was read. The generators, the comparers and the
probes are in the project files under `review-harness/round-ft/`.

Round FU re-reviewed FT on fresh banks and swept new areas (Review FU1).
FT's CI run 44964667724 finished green. FT's fixes held (the interval
eventually-follows with dense nanoseconds, correlation exact time matching
with supplied keys, the dataframe DFG, partial order and concurrency on XES
and CSV uploads). Fixed: FU1-1, the performance spectrum sorts the
disconnected variants by their earliest instant, gathers the frame
variants' disconnected observations longest first in frame row order, lists
the Polars connected observations in frame row order and groups a case's
rows across the whole Polars frame; FU1-2, Alpha on a frame sorts text
timestamp columns as text and a Polars frame with the unstable pdqsort, a
missing value first (the workflow route's `dataframe`/`lazyframe` shapes,
`/api/mine` lazyframe); FU1-3, the plusplus `pandas_mutable` shape on two
text or boolean columns raises the subtraction `TypeError` only when the
partial order keeps a pair (an earlier row's completion at most a later
start, as text, rows apart included); FU1-4, the Inductive miner on a
Polars frame reads each case after the unstable sort by case and
timestamp; FU1-5, Declare on a Polars frame reads each case sorted by
`time:timestamp` (always that column, a missing value first) and the row,
and lists the cases in case-id order, in discovery and conformance
(`/api/mine`, `/api/declare-workbench`). Not a finding: the `/api/mine`
heuristic family follows the dispatcher `apply_heu` (frame order), not
`pm4py.discover_heuristics_net`'s sorting `apply_heu_pandas`. New areas
without findings: the temporal profile on XES ties, the log skeleton and
the transition system on lazy XES uploads, the workflow route's classic
heuristics on text CSV columns; no other route reaches SciPy's
`linear_sum_assignment`.

In the suite: `minera_performance_spectrum_parity_contracts_tests`
(`performance_spectrum_frame_order_contract`),
`minera_alpha_event_data_tests` (`frame_sort_contracts`, and two lazy
expectations that now sort a missing value first),
`minera_heuristic_variants_tests` (`review_ft_plusplus_pandas_text_columns`),
`minera_inductive_miner_tests` (`polars_frame_reads_ties_as_polars_sorts`)
and `minera_declare_tests` (`polars_frame_reads_cases_in_time_order`)
carry FU1's reference values. The full ctest passed 660 of 660 on the
first run (29 minutes, -j2), the Python script tests passed 406 of 406,
the fixture replay gave 2,671 responses unchanged and 2 changed as
intended (two Declare workbench conformances on lazy frames now list the
cases in Polars' case-id order; both read uint64 case ids, which pm4py
refuses, and were refreshed). The Mac was not
reachable this round (no device served, the folder not approved), so no
newer reviewer handoff was read. The generators, the comparers and the
probes are in the project files under `review-harness/round-fu/`.

Round FV re-reviewed FU on fresh banks and swept the other Polars inputs
that group cases after a sort (Review FV1). FU's CI run 44972994921
finished green and FU's fixes held (the spectrum, Alpha on text and Polars
frames, the plusplus text pairs, the lazy families, the Declare workbench
on lazy uploads). Fixed: FV1-1, POWL on the `dataframe` and `lazyframe`
shapes (`/api/mine`, `/api/discover-workflow`) reads the frame sorted by
case and timestamp (pandas stable, Polars' pdqsort) instead of the
prepared log; FV1-2, `/api/variant-statistics` on a Polars session groups
the cases after Polars' sort; FV1-3, Declare on a Polars frame with its own
`@@index` sorts by it (unstably) instead of a row count; FV1-4, Declare on
a Polars frame runs pm4py's frame checks with the case and activity keys
(three columns, two text, one date, then the case and activity columns);
FV1-5, the temporal profile on a Polars frame (`/api/mine`,
`/api/analyze-behavior` discovery and conformance) pairs rows in Polars'
partial order (`temporal_profile.cpp`, `polars_partial_order_table`): the
unstable sort by case, start and completion with the row index after it
(a frame's own `@@index`, and a CSV upload's format_dataframe order, kept
unsorted), no pair for a row without a case, whole-second durations, the
moments and each case's deviations in the pair frame's order (by the
later row, then the earlier, or by the profile's entry first when the pair
frame has no more rows than the profile), and an empty bucket for the
missing case. Declared: the Polars conformance's buckets follow
`unique()`, which has no fixed order; the Declare `dataframe` shape still
answers where pm4py's frame check refuses (round BM's declaration). New
areas without findings: the minimum self-distance and footprints on lazy
XES uploads.

FV's suite: the full ctest 660 of 660 (28 minutes, -j2) before the last
frame-check change, then the 40 Declare, frame-check, POWL, variant and
temporal profile tests and the 66 server and workbench tests; Python
script tests 406 of 406; `tsc` clean; vitest 10,732 of 10,732; the 16
replayable manifests with only the intended changes (POWL's
`native_input_kind` on frame shapes, POWL failing on an absent timestamp
column, unsigned case ids refused by the Polars Declare check), refreshed.
Found at the end and left for FW: pm4py's Declare wrappers also check the
selected timestamp column on a Polars frame (present, no empty value),
the algorithm does not; Minera's `lazyframe` Declare answers there as the
algorithm does (`round-fv/probe/dcwrap.py`).
Harness and probes: `review-harness/round-fv/`.

Round FW re-reviewed FV on fresh banks, settled FV's Declare timestamp
question and swept the temporal profile with business hours on Polars
frames and the Declare `dataframe` shape (Review FW1). FV's CI run
44982394370 passed on Linux and Windows (its macOS job was cancelled
during the build, warnings only in the log), and FV's fixes held (POWL and the variants
on both frame shapes 70 of 70, Declare on Polars frames 62 equal and 58
refused alike, the temporal profile on large, CSV and start-key frames
exact and in pm4py's order). Decided: the Declare routes mirror the
algorithms, so the selected-timestamp check of pm4py's Declare wrappers is
declared. Fixed: FW1-1, the temporal profile with business hours on a
Polars frame (`/api/mine`, `/api/analyze-behavior` discovery and
conformance) raises Polars' `SchemaError` as pm4py does when
`BusinessHours` returns an int for some pair (no end inside a slot; the
message names the first such pair in the pair frame's order and the start
column's `_2` copy, `detail::polars_business_schema_error`, shared with the
Polars DFG); FW1-2, Declare on the `dataframe` shape of an XES upload runs
the checks of pm4py's pandas Declare algorithm (the frame shape without
keys, `KeyError` for the case, activity and `time:timestamp` columns,
`np.unique`'s `TypeError` on a missing case id with numpy's stable object
argsort deciding the operands, discovery's `TypeError` on a missing
activity, conformance reading a missing activity as an event no rule
names), and on an XES upload a column without any value counts as absent
there and in FV1-4's Polars checks (`/api/mine` and the workbench
conformance; a CSV frame keeps every header column; round BM's
declaration withdrawn). Declared: the
hash-seed operand order of the missing-activity `TypeError` and the
thread-dependent int in the `SchemaError`. New areas without findings: the
transition system and the heuristics on tie-heavy uploads (pandas frame,
Polars copy and log agree).

FW's suite: the full ctest 659 of 660 (38 minutes, -j2; round BM's
`minera_review_bm1_tests` expected the withdrawn declaration and now
expects pm4py's `TypeError`, then passed); Python script tests 406 of 406;
the two vitest files that read the captures 298 of 298; the 16 manifests
with one intended change (`xes-custom-clock-native-flat-dataframe`,
Declare on the `dataframe` shape of an XES upload timed by `clock` raises
`KeyError: 'time:timestamp'`), refreshed. The first replay caught the
column rule refusing a header-only CSV on `lazyframe`, so the rule holds
for XES uploads only.
Harness and probes: `review-harness/round-fw/`.

Round FX re-reviewed FW on fresh banks and swept the footprints workbench's
frame variants and concept drift on XES uploads with gaps (Review FX1).
FW's CI run 44992448995 passed on Linux and macOS (Windows still in ctest
at the time), and FW's fixes held (Declare on both frame shapes 30 of 30
and 30 of 30 on new banks, refusals alike, the business-hours temporal
profile exact or refused alike). FW's header-only CSV lead is no gap
(pm4py answers an empty model with the default keys). Fixed: FX1-1, the
footprints frame variants follow pm4py's pandas and Polars variants on
gaps (rows without a case id leave the pandas cases but count their
activities, Polars keeps them as one group, the pandas start and end skip
unnamed events), missing columns (pandas `KeyError` in selection order,
Polars names them sorted, the XES column rule of FW1-2 now
`xes_frame_columns.hpp`), frames without rows (pandas NaN `ValueError`,
Polars `int()` `TypeError`) and the Polars plan errors (a shifted column
the plan dropped, e.g. a start key equal to the timestamp key, or two
shifts of one column from colliding keys, with the resolved plan: its sets
in seed-0 hash order via the new shared `python_str_set.hpp`, its `DF`
line in read_xes's column order, no sort nodes without `sort_required`),
and the pandas variant's duplicate-label `ValueError`s; FX1-2, concept
drift on an XES upload mixing named and unnamed traces raises np.unique's
`TypeError` with numpy's stable object argsort deciding the operands.
Declared: the plan's hash-seed set order (seed 0 modelled). New area found
and left for FY: the `/api/mine` families on frame shapes of XES uploads
with gaps (below).

FX's suite: the full ctest 661 of 661 (26 minutes, -j2, first run), Python script tests 406 of
406, the footprints capture vitest files 336 of 336, and the 16 manifests
with one intended change (`native-discovery-engaged-start-timestamp`, the
Polars variant with the start key equal to the timestamp key on the log
shape raises pm4py's `ColumnNotFoundError` with the unsorted plan),
refreshed, its two stale result captures removed.
Harness and probes: `review-harness/round-fx/`.

Round FY re-reviewed FX on fresh banks and swept the `/api/mine`
transition system, Inductive, POWL and log skeleton families on the frame
shapes of XES uploads with gaps (Review FY1). FX's CI run 44999089784
passed on Linux and macOS, the frontend, the Python tests and the
verifier (Windows still building at the time); FW's Windows job in run
44992448995 passed. FX's fixes held: the footprints frame variants on new
seeds of the random key banks (rK, 1,920 of 1,920 equal, refusals alike),
the higher-gap banks fH (240 of 240, also with `sort_required=false`) and
the start key equal to the timestamp key (fD, 240 of 240); concept drift on
unnamed traces at the start, middle and end (dY, 60 of 60 on each shape).
Fixed: FY1-1, the four families follow pm4py's algorithm applies on
read_xes's frame and its Polars copy: missing columns (pandas `KeyError`s,
a list selection naming the missing labels for Inductive and POWL; the
Polars plans' `ColumnNotFoundError` with the resolved plan and the "Did you
mean" hint, via the new `polars_missing_column.hpp`), rows without a case
id (pandas groups drop them, Polars keeps one null group; Inductive and
POWL on pandas raise np.unique's `TypeError` with numpy's stable argsort
deciding the operands), rows without an activity (pandas: one transition
per unnamed step, `KeyError: nan` in the log skeleton; Inductive and POWL
the alphabet's `TypeError`), and rows without a timestamp (Inductive and
POWL now answer, NaT last); the log skeleton on pandas counts the rows
without a case id where pm4py counts every row (equivalence, never
together, directly follows, the frequency threshold), while always after
and always before count the traces only. Declared: the alphabet
`TypeError`'s operand order (a set of str and NaN or None, hash- and
address-dependent). Left for FZ: the log skeleton with a null activity on
Polars (pm4py answers with `None` as an activity; Minera's model has no
null label), and the alpha, heuristics and temporal profile families on the
same frames (below).

FY's suite: the full ctest 662 of 662 (28 minutes, -j2, first run; the new
`minera_server_mine_xes_frames_tests` fails 42 of its 49 checks on FX's
binary), Python script tests 406 of 406, and the 16 manifests replayed
2,673 responses unchanged (no fixture refresh; frontend untouched).
Harness and probes: `review-harness/round-fy/`.

Round FZ re-reviewed FY on fresh banks and aligned the `/api/mine` Alpha,
heuristics and temporal profile families on the frame shapes of XES
uploads with gaps, and the log skeleton's missing activity on the Polars
copy (Review FZ1). FY's CI run 45006486972 passed on Linux and macOS, the
frontend, the Python tests, the verifier and Windows; FX's
Windows job in run 44999089784 passed. FY's fixes held: the transition
system, Inductive, POWL and the log skeleton on six new gap banks (zM,
zH, zL, zA, zC, zT; 260 uploads per shape) and on the older banks with
two new ones (zX larger logs with tied timestamps, zS tiny logs; 502
uploads per shape), apart from the declared operand order of Inductive's
and POWL's alphabet `TypeError`. Fixed: FZ1-1, Alpha reads the timestamp
first and then selects case, activity and timestamp (pandas `KeyError`s,
Polars `with_columns` and `group_by` plans with `SORT BY` when the case
column is there); pandas rows without an activity leave the pairs and the
boundary activities unless their case has none (then NaN starts and ends
it), Polars rows without an activity or a case id are boundary activities
of their own; FZ1-2, the heuristics convert the frame to an EventLog
(missing case `KeyError`, missing activity `KeyError` only at a pair,
null activities as `nan`/`None`, "not subscriptable" after one, the AND
measure's sort `TypeError` via `python_list_sort.hpp`, typed errors);
FZ1-3, the temporal profile's missing-column errors (Polars `sort` plan,
the pair frame's `KeyError`); FZ1-4, the Polars log skeleton keeps `None`
as an activity (`kLogSkeletonNullActivity`, `null` in relations,
`null_activ_freq`). Declared: the graph presentation leaves out null
skeleton pairs; the heuristics net's Petri conversion labels the missing
activity with text.

FZ's suite: the full ctest 663 of 663 (33 minutes, -j2, first run; the new
`minera_server_mine_xes_frames_fz_tests` fails 55 of its checks on FY's
binary), Python script tests 406 of 406, and the 16 manifests replayed
2,673 responses unchanged (no fixture refresh; frontend untouched).
Harness and probes: `review-harness/round-fz/`.

Round GA re-reviewed FZ on fresh banks and aligned Alpha+, the log
skeleton's conformance and the heuristics' AND-measure node order on the
frame shapes of XES uploads (Review GA1). FZ's CI run 45016289288 passed
on Linux and macOS, the frontend, the Python tests and the verifier
(Windows still in ctest at GA's push). FZ's fixes held:
Alpha, the temporal profile and the log skeleton on six new gap banks
(gaM, gaH, gaL, gaA, gaT, gaS; 280 uploads per shape) on both shapes; the
heuristics' thresholds (dependency, AND, length-two loop, noise, minimum
counts) on the same banks; footprints on CSV uploads (csA, csB, csG; 180
uploads, both variants and shapes, plus the plan's `DF` line). Fixed:
GA1-1, the heuristics' AND measure meets the nodes in the order the
dependency pass creates them (rows by first lead of a cleaned DFG pair,
accepted edges adding source then target), so the first node mixing the
missing label with a named successor decides the `TypeError` operands;
GA1-2, Alpha+ on frames converts to an event log (trace per case value in
first-row order, null case one trace, row order, NaN or None labels;
`IndexError` without rows, `KeyError` for missing case then activity),
pandas' NaN one directly-follows key but never equal to itself (no
length-one loop, never closes "a b a"; internal `alpha_python_nan.hpp`),
Polars' None a transition without a label (`null_label`); GA1-3, the log
skeleton's conformance lists the groupby's cases (pandas sorted ids
without the null case, Polars first-row order with the null case and
merged names), keeps Polars' None and raises the deviation sort's
`TypeError` via `python_list_sort.hpp`. Declared: Alpha+'s place growth
follows Python set order, and the missing label's hash there is an object
address (one upload per shape differs; a Python emulation of Minera's
construction on the frame's own objects reproduces pm4py on all 230).
Sweeps must set `PYTHONHASHSEED=0`: without it Alpha+ differs on uploads
whose set order matters (the ref picks a random seed).

GA's suite: the full ctest 664 of 664 (26 minutes, -j2, first run; the new
`minera_server_mine_xes_frames_ga_tests` fails 36 of its checks on FZ's
binary), Python script tests 406 of 406, and the 16 manifests replayed
2,673 responses unchanged (no fixture refresh; frontend untouched). Harness and probes: `review-harness/round-ga/`.

Round GB re-reviewed GA on fresh banks and aligned Heuristics++, the
classic performance view, Alpha's and the temporal profile's explicit
start keys and the workbench routes that re-discover on an XES session,
on the frame shapes of XES uploads with gaps (Review GB1). GA's CI run
45024372369 and FZ's Windows job passed. GA's fixes held on six new banks
from `gen/gengb.py` (genfz.py plus repeats of the previous label and
length-two loops: gbM, gbR, gbA, gbL, gbH, gbS; 280 uploads per shape):
Alpha, the heuristics, the temporal profile, the log skeleton, Alpha+
(also `remove_unconnected`) and the conformance (also with noise 0.3).
Fixed: GB1-1, Heuristics++ on frames converts to an event log, raises
`KeyError` for case, activity, timestamp in that order, then on any
missing timestamp (pandas `ValueError: NaTType does not support
timestamp`, Polars `TypeError` subtracting None), then where Python
compares a missing label with a named one (interval overlaps per trace,
then the eventually-follows pairs past the minimum counts; pandas when
exactly one side is NaN, Polars when either side is None); GB1-2, the
classic performance view waits 0 over a pandas NaT and raises Polars'
subtraction `TypeError` at the first pair with a None; GB1-3, an explicit
start key the frame holds is read as given (pandas selects case,
activity, timestamp, start with the Index repr wrapped at width 80;
Alpha sorts each key by its own values, a text start as text; Polars
without the start alias, the duplicate `ComputeError` when the start is
the timestamp; the temporal profile's text-start `TypeError` or Polars
`InvalidOperationError`; the text test reads the values since a
session's frame table has no dtypes; `discover_temporal_profile_xes_frame`
takes `explicit_start_key`); GB1-4, `/api/discover-constraints`
(log skeleton and its conformance), `/api/discover-workflow`,
`/api/discover-alpha-plus` without an upload and `/api/analyze-behavior`'s
temporal profile read an XES session's frame as `/api/mine` does.
Declared: Alpha+ through the routes keeps GA1's set-order upload; the
top-cases and top-activities selections among tied counts change from run
to run in pm4py itself (two of the `std::stable_sort` sites). Sweeps set
`PYTHONHASHSEED=0` and use one fresh server per sweep.

GB's suite: the full ctest 665 of 665 (26 minutes, -j2, first run; the new
`minera_server_mine_xes_frames_gb_tests` fails 32 of its checks on GA's
binary), Python script tests 406 of 406, and the 16 manifests replayed
2,673 responses unchanged (no fixture refresh; frontend untouched).
Harness and probes: `review-harness/round-gb/`.

Round GC re-reviewed GB on fresh banks and aligned the conversion of a
heuristics net with a missing label, Heuristics++'s interval input,
Alpha's and the temporal profile's start keys of numbers, booleans and
mixed values, the implicit start column and token replay's missing
columns, on the frame shapes of XES uploads (Review GC1). GB's CI run
45033653968 passed on Linux, macOS and the Python, verifier and frontend
jobs (Windows still running at GC's start). GB's fixes held on new banks
from `gen/gengc.py` (gcI, gcF, gcB, gcX, gcD, gcT, gcP, gcQ, gcN, gcW: 450
uploads per shape with lifecycle gaps, start timestamp columns, integer,
float, boolean and mixed start columns; hS, hG, hL, hK, hM, hA: 240
Heuristics++ uploads): `optval.py` `hpp`, `hpp_perf`, `hpp_min` and the
seven start-key sets, `cm/conv.py` (482 conversions per shape plus 708
where both raise), `wb/cskel.py` with noise 0.2 and `wb/wfopt.py`
(heuristics thresholds, Alpha+ `remove_unconnected`, Heuristics++, the
performance view, temporal conformance with zeta 0.5 and 1.5; 670 uploads
per shape) all agree. Fixed: GC1-1, the heuristics net's conversion names
a node's join place "pre_" + its name, so a missing label with an
incoming dependency raises `TypeError: can only concatenate str (not
"float")` (pandas) or `(not "NoneType")` (Polars), and otherwise Polars'
None is a silent transition (`/api/mine`'s conversion and
`/api/discover-workflow`, which returns the converted net, so its error is
pm4py's); GC1-2, Heuristics++ with a start_timestamp column keeps the log
as it is and reads each start as the frame's Python object (pandas
Timestamp or NaT, an object column's datetime, str or NaN, Polars
datetime, str or None), raising at the first failing step: CPython's
sort of each trace by its starts, completion against the later starts,
the performance subtraction, then `timestamp()` on each start and
completion (internal `heuristic_plus_frame_intervals.hpp`); without one, a
missing lifecycle transition raises `AttributeError: 'float'|'NoneType'
object has no attribute 'lower'`; GC1-3, a start key of numbers, booleans
or mixed values follows read_xes' dtype (internal `frame_key_column.hpp`:
dates datetime64, integers int64 or float64 beside a gap, booleans bool or
object beside a gap, text str, other mixes object): Alpha sorts it by
pandas' Categorical order (numbers before text, gaps last; a date beside
a number raises `'values' is not ordered`) or by Polars' order (nulls
first, String by bytes), and the temporal profile raises pandas' `Invalid
comparison between dtype=int64|float64|bool and ndarray`, an object
column's value-wise `'<=' not supported between instances of 'Timestamp'
and ...` or `cannot subtract DatetimeArray from ndarray[object]`, and
Polars' `- not allowed on i64|f64|bool and datetime[μs, UTC]` plan
error; GC1-4, without a start key Alpha, the temporal profile and its
conformance (`/api/analyze-behavior`) read the frame's start_timestamp
column, as pm4py's directly-follows and partial-order frames do
(`conform_temporal_profile_xes_frame`); GC1-5, GB1-3's text-start check
raises the comparison error only when a case's later present start fails
to parse as a date, else the subtraction error; GC1-6, token replay of
`/api/mine` on a frame without the case or activity column raises
pandas' `KeyError` for the case, then `KeyError: 'Column not found:
...'`, or Polars' `ColumnNotFoundError` at `group_by`. Probed and kept:
Polars 1.44.1 sorts by one column stably on ties (1,800 frames up to
6,000 rows and 24 up to 150,000, Datetime, Int64 and String with nulls,
lazy and eager), and by two columns with a Null-dtype key stably too (800
frames), so the remaining `std::stable_sort` sites (the variants and
sequence filters with a None case key, the LTL fallback, the temporal
lazy arrival and finish order, `sort_by_keys` with a Null key) match.
Declared: temporal conformance on Polars lists the cases through
`select(unique())`, whose order changes from run to run in pm4py itself
(8 orders in 8 runs); sweeps compare its deviations as a multiset.

GC's suite: the full ctest 666 of 666 (39 minutes, -j2, first run; the new
`minera_server_mine_xes_frames_gc_tests` fails 24 of its checks on GB's
binary), Python script tests 406 of 406, and the 16 manifests replayed
2,673 responses unchanged (no fixture refresh; frontend untouched).
Harness and probes: `review-harness/round-gc/`.

Round GD re-reviewed GC on fresh banks and aligned `/api/mine`'s token
replay of the session models and `/api/discover-graph`'s footprints of the
cached heuristics model on the frame shapes of XES uploads (Review GD1).
GC's CI run 45046061452 passed on Linux, macOS and the Python, verifier and
frontend jobs (Windows still building at GD's start). No device was
reachable at GD's start, so no newer reviewer handoff was read from the
Mac. GC's fixes held on new banks from `gen/gengc.py` (gdI, gdB, gdX, gdD,
gdT, gdF: 240 uploads with integer, boolean, mixed, date, text and float
start columns, timestamp gaps, lifecycle columns, missing cases; hdS, hdL,
hdM, hdG: 160 Heuristics++ uploads): `cm/conv.py` over 1,590 uploads per
shape (654 nets, 51 with a missing label, 22 errors equal, 863 where both
raise), `optval.py`'s seven start-key sets on gd* and `hpp`, `hpp_perf`,
`hpp_min` on hd* all agree, except two Polars Heuristics++ uploads (open,
below). Fixed: GD1-1, pm4py's `token_replay.apply` on a frame replays the
frame's variants, each case's activities in row order (no timestamp sort),
pandas' cases in its sorted groupby without the missing case, Polars' in
first-row order with the null case as one more, and sorts the variants by
(count, tuple) descending, a `TypeError` where a missing label meets a
name; Minera replayed each model's prepared log (sorted, the null case
counted, Alpha's refused when a missing case kept the Alpha prepared log
from existing). Every model's replay on an XES session now reads the frame
(internal `xes_frame_token_replay.hpp`). GD1-2, pm4py first sorts the
net's transitions by name: the heuristics net's missing label (NaN, None)
raises `'str' and 'float'|'NoneType'` when it leads the net's activities,
else the reverse; Alpha's raises `'float'|'NoneType' and 'str'` (operand
order declared). GD1-3, a pandas NaN matches no heuristics transition but
Alpha's NaN transition; Polars' None fires a silent transition (the
single enabled one, else the greatest by name, force-fired;
`check_conformance_null_silent`). GD1-4, the heuristics net's silent
routing ties go by pm4py's place and transition names (source0, sink0,
pre_X, intplace_X, splace_in_T_S, hid_N; internal
`heuristics_petri_names.hpp`). GD1-5, the footprints of the cached
heuristics model no longer need a POWL model or a process tree
(`analyze_footprints_models`; a requested variant whose model is missing
raises). Before and after per shape (1,190 uploads): heuristics replay
115 / 54 totals differed and 34 errors differed, after 2 (hash-seed ties
in pm4py itself); its footprints 175 equal, after 398; Alpha replay 106
refused plus 723 / 723 missing-label uploads differed, after every total
equal and only the declared operand order left; Inductive replay 58 / 82
differed, after none; POWL replay 2 / 43 differed, after 2 / 7 (open). Declared: the operands of
Alpha's transition name sort follow a Python set's order, which changes
with the hash seed and from run to run (object identities); Polars' None
against several enabled silent transitions follows hash order; gcQ/f5714
and f5716's totals vary with the hash seed in pm4py; the footprints
route's log part raises on a missing label as pm4py's entire_event_log
variant does (`KeyError: 'concept:name'`, a different message).

GD's suite: the full ctest 667 of 667 (25 minutes, -j2, first run; the new
`minera_server_mine_xes_frames_gd_tests` fails 23 of its checks on GC's
binary), Python script tests 406 of 406, and the 16 manifests replayed
2,671 responses unchanged and 2 refreshed
(`xes-custom-clock-native-flat-dataframe` and `-lazyframe`: the Alpha,
Inductive and POWL replays of a trace against its clock misfit, as
pm4py's frame replay does).
Harness and probes: `review-harness/round-gd/`.

Round GE re-reviewed GD on fresh banks and aligned the session POWL
model's token replay and Heuristics++'s intervals on XES frames with
pm4py (Review GE1). GD's CI run 45056656265 (and 45056680276 for the
handoff commit) passed on Linux, macOS and the Python, verifier and
frontend jobs (Windows still building at GE's start); GC's Windows job
in 45046061452 passed. No device was reachable at GE's start, so no newer
reviewer handoff was read from the Mac. GD1 held on new banks
(`gen/gengc.py`, seeds from 7500: geA, geC, geL, geN, geO, geR, geT, 280
uploads with more missing-label-only cases, several silent transitions
under Polars' None and routing ties; heL, heM, heN, 120 Heuristics++
uploads): `cm/cmroutes2.py` with each model on both shapes gave equal
replay totals for the heuristics, Alpha and Inductive models, the
declared Alpha operand order (5 pandas, 61 Polars messages) and the
declared footprints log-part errors (13 per shape) only, and one Polars
POWL total (open from GD). Fixed: GE1-1, pm4py's POWL net names its
places `p_N` from 3 (source and sink take 1 and 2) and its silent
transitions `<kind>_N` on their own counter (tau, skip, init_loop, loop,
tauSplit, tauJoin, init_par, final_par), in the order its converter
builds and then folds them (a Python model matched pm4py's names on
1,500 random POWL models); its token replay breaks routing ties by those
names. Minera replayed with its own index names: the 9 open totals of GD
(pandas gaL/f2306, gcB/f5213; Polars gaL/f2310, gaM/f2136, gaS/f2606,
gaT/f2531, gbQ/f4708, gbQ/f4712, gcQ/f5716) now match. GE1-2, pm4py's
shortest silent paths follow its arc sets, in practice the order the
converter made the arcs with a folded arc's replacement in its place;
the names alone moved gaL/f2321's pandas total off pm4py's, the arc
order brings it back (internal `powl_petri_names.hpp`: the named
construction matched onto Minera's net, whose indices stay, and
`powl_replay_net`, which orders each transition's outputs as pm4py's).
Both apply to the frame shapes and to the default log shape of
`/api/mine`. GE1-3, Heuristics++ on a frame pairs lifecycle events into
intervals case by case (Polars' cases in first-row order with the null
case, pandas' after a missing lifecycle check over the whole frame): a
complete takes the first open start of its activity and instance, an
open start or another transition forms no interval and its missing time
is never read, the first pair with a missing time raises Polars'
`TypeError: unsupported operand type(s) for -: '<complete>' and
'<start>'` (NoneType or datetime.datetime) or, after the scan, pandas'
`ValueError: NaTType does not support timestamp`; a Polars missing
lifecycle raises its `AttributeError` when the scan reaches it. Minera
raised on any missing time or lifecycle up front: 5 probes per shape
raised where pm4py succeeds, 16 Polars probes gave the other error
(hdL/f7365 and f7375 among them). Before and after: POWL replay on
1,710 uploads 1 pandas total and 0 Polars differed after the names (9
before), 0 after the arc order; on the log shape the names fix gaL/f2330
and change no other total; the 33 Heuristics++ probes 5 / 21 differed,
after 0; `hpp`, `hpp_perf` and `hpp_min` on 520 bank uploads (he*, hd*
and older) all equal on both shapes. Declared: pm4py's own silent-path ties follow its
arc sets' memory order, which can differ between processes (the arc
order model matched every pair in one run and missed 24 of 9,731 in
another); Minera follows the creation order.

GE's suite: the full ctest 668 of 668 (25 minutes, -j2, first run; the new
`minera_server_mine_xes_frames_ge_tests` fails 12 of its checks on GD's
binary), Python script tests 406 of 406, and the 16 manifests replayed
2,673 responses unchanged.
Harness and probes: `review-harness/round-ge/`.

Round GF re-reviewed GE on fresh banks, found the default log shape's
reference and aligned the routes that replay a cached POWL or heuristics
net with pm4py (Review GF1). GE's CI run 45064260404 passed on Linux,
macOS and the Python, verifier and frontend jobs (Windows still building
at GF's start). No device was reachable, so no newer reviewer handoff was
read from the Mac. New banks (`gen/gengf.py`, seeds from 8000: gfA to gfE
with loops, partial orders and silent routing, gfP to gfR with routing
ties, hfI to hfL Heuristics++ uploads with typed `concept:instance`)
held GE1. The default log shape follows AE2-1/AF1 (the legacy log without
empty traces, shared non-null case ids merged, unnamed traces apart): the
177 trace-count differences of GE are that model, and the POWL, Alpha and
Inductive totals on that shape equal pm4py's. Fixed: GF1-1, the log shape
replays the heuristics net with pm4py's names (GD named it on frames only;
27 uploads' totals differed, after 2: gcQ/f5714 and f5716, declared in
GD1 as hash-seed dependent in pm4py); GF1-2, the session keeps a named replay copy
of each cached POWL and heuristics net (pm4py's names, POWL's arc order;
the cached net, its JSON and PNML stay), which `/api/evaluate`
(token-based fitness, precision, generalization, composite),
`/api/compare-petri`, the stochastic map and the decorations replay
(gaL/f2330's POWL fitness 0.99184 against pm4py's 0.98297 before, equal
after; the stochastic map's weights and distribution types now equal
pm4py's on 242 of 244 comparisons); GF1-3, Heuristics++ pairs lifecycle
events by the instance's Python value (int 2 with float 2.0, never with
'2'; NaN never on a log, NaN with NaN on pandas, text on Polars; hfJ/f8279
gave pandas' TypeError operands swapped). The harness footprints check
(`cmroutes2.py`, `cmroutes4.py`) asked for the heuristics model whatever
`MODEL` was, so GE's and GF's footprints lines for the other models
compared the wrong net; fixed, the POWL model's footprints on gfA to gfE
equal pm4py's (32 of 32). Declared: an upload without a non-empty trace
reports fitness 1 in the analysis (pm4py's evaluation and `/api/evaluate`
say 0); the log shape's whole-mine error wording for a missing label or
time; pm4py's element usage of four POWL pairs changes with the hash seed.
Open: the stochastic map on events out of time order (gfR/f8540, f8564),
the element usage of gfR/f8532 against f8533 (log2 out of time order),
the playout and Monte Carlo fits from a cached net.

GF's suite: the full ctest 669 of 669 (40 minutes, -j2, first run; the new
`minera_server_cached_net_names_gf_tests` fails 13 of its checks on GE's
binary), Python script tests 406 of 406, and the 16 manifests replayed
2,673 responses unchanged.
Harness and probes: `review-harness/round-gf/`.

Round GG re-reviewed GF on fresh banks and aligned the stochastic map's
service times, the stochastic fits of the Monte Carlo and playout routes
on cached nets and Heuristics++'s NaN and missing instances with pm4py
(Review GG1). GF's CI run 45074453620 passed on Linux, macOS and the
Python, verifier and frontend jobs (Windows ctest still running at GG's
start); GE's Windows job in 45064260404 passed. No device was reachable,
so no newer reviewer handoff was read from the Mac. New banks
(`gen/gengg.py`, seeds from 9000: ggA to ggD with loops, partial orders,
silent routing, events out of time order and gaps; hgI to hgL Heuristics++
uploads with bool, date, float and NaN instances) and 20 Heuristics++
probes. Fixed: GG1-1, pm4py times a firing from the input token of the
latest event in replay order (not the latest time; first-event tokens
leave a silent firing untimed, which passes its arrival token on);
GG1-2, `/api/monte-carlo` and the stochastic playout fit the cached
net's named replay copy, so a seeded run fitted from the log equals the
run on `/api/stochastic-map`'s map (8 POWL and 27 heuristics runs
differed); GG1-3, a token from an event without a timestamp stays that
event's token and a sample with a missing time on either side is 0 (nine
POWL and ten heuristics maps on gfA to gfE took another fit, gfC/f8087
fitted where pm4py's exponential fit raises); GG1-4, Heuristics++ keeps a
NaN instance apart on an XES upload's log shape, and on pandas frames
pairs by the instance column's dtype (float64: NaN and gaps never pair;
str: both pair; object: gaps pair, parsed NaN floats never), on Polars a
NaN is null. After: the stochastic map equals pm4py's on 372 of 374 POWL
and 370 of 370 heuristics sessions (560 uploads per model, the rest fail
on both sides or in the mine), the two left vary with pm4py's hash seed
(gfR/f8564 4 of 8 seeds, gfD/f8144 1 of 6; declared); Monte Carlo 183 and
292 equal; Heuristics++ 131 of 131 on the log shape and 91 of 91 per frame
shape and option set. Kept declared: the analysis' empty-log fitness 1
(Minera's own summary; `/api/evaluate` says pm4py's 0) and the log
shape's whole-mine message for a missing label or time. The footprints
lines of the POWL model that differed compared the net's footprints;
Minera gives the POWL model's (`footprints.apply` on the POWL), which
equal pm4py's. Open: gfR/f8532 against f8533 (one silent transition's
log2 count moves; pm4py's is stable over 8 hash seeds).

GG's suite: the full ctest 670 of 670 (29 minutes, -j2, first run; the new
`minera_server_stochastic_fits_gg_tests` fails 14 of its checks on GF's
binary), Python script tests 406 of 406, and the 16 manifests replayed
2,673 responses unchanged.
Harness and probes: `review-harness/round-gg/`.

Round GH re-reviewed GG on fresh banks and aligned Heuristics++ on CSV
uploads' log shape and the alignments of an upload without traces with
pm4py (Review GH1). GG's CI run 45082708237 passed on Linux, macOS and
the Python, verifier and frontend jobs (Windows ctest still running at
GH's end). No device was reachable, so no newer reviewer handoff was
read from the Mac. New banks: `gen/gengh.py` (GG's generator plus shuffled
event order, first-event time gaps and swapped times; ghA to ghD, seeds
9400-9639; hhA and hhB Heuristics++ uploads, 9700-9859), `gen/genhcsv.py`
(960 CSV Heuristics++ uploads hcA to hcH with typed instance, lifecycle and
start columns and gaps), 201 CSV copies of the gh uploads and 44 CSV
probes. Fixed: GH1-1, Heuristics++ reads a CSV upload's log shape as
`convert_to_event_log` of its frame holds it: instances by the column's
dtype ("1" and "1.0" of a float64 column pair), a missing or non-str
`lifecycle:transition` raises pm4py's AttributeError, a lifecycle column
under another header is ignored, and any start-timestamp column keeps the
log as it is, then raises pm4py's NaT ValueError or Timestamp TypeError
(str, int, float, bool or all-missing columns); GH1-2, `/api/evaluate`'s
alignment fitness and `/api/align` of an upload without traces fail on a
net that is not easy sound (a heuristics net without transitions), since
pm4py checks easy soundness before reading the log. After: Heuristics++ on
CSV uploads 1,004 of 1,004 equal or failing alike (13 matrices and 339
errors differed before), the option sets likewise; the alignment fitness 711 of 711 equal or
failing on both sides; the stochastic map 163 POWL and 161 heuristics
sessions equal on the new banks (Monte Carlo 100 and 128, playout 163 and
129, CSV copies 192 and 193); Heuristics++ on the new XES banks' frame
shapes 160 of 160 per shape and option set; `/api/align` costs equal on
every variant (moves and search counters are tie choices the contract
does not bind). Declared: the element usage of gfR/f8532 against f8533,
ghA/f9426 against f9427 and ghB/f9461 against f9462 and the stochastic
map of gcsv/f9631 vary with pm4py's hash seed (GG's note that f8532 was
seed-stable was wrong); the log shape's whole-mine message when a later
event lacks the start timestamp (pm4py `KeyError: 'start_timestamp'`),
as for a missing time. The Alpha model's log-shape totals on the GG and
GH banks equal pm4py's (327 replay totals, 269 footprints) but for the
declared empty-log fitness (60) and footprints failing on both sides
(118, unbounded nets).

GH's suite: the full ctest 671 of 671 (27 minutes, -j2, first run; the new
`minera_server_review_gh_tests` fails 11 of its 15 checks on GG's
binary), Python script tests 406 of 406, and the 16 manifests replayed
2,673 responses unchanged.
Harness and probes: `review-harness/round-gh/`.

Round GI re-reviewed GH and aligned Heuristics++ on CSV uploads' frame
shapes and big-int start columns, and the alignments of an upload without
traces on supplied nets, in the Petri alignment workbench and in the export
(Review GI1). GH's CI run 45090146735 passed on Linux, macOS and the
Python, verifier and frontend jobs (Windows ctest still running at GI's
end); GG's run 45082708237 finished green, Windows included. No device was reachable, so no newer reviewer handoff was read from
the Mac. New banks: `gen/genhcsv2.py` (720 CSV Heuristics++ uploads icA to
icG, seeds 10700-11419, with date, mixed, big-int and offset instance
columns, date, upper-case and numeric lifecycle columns, and start columns
of uint64, object (beyond uint64 or below int64), float and offset cells),
GH's 1,161 CSV uploads on the frame shapes, 12 CSV probes. Fixed: GI1-1, a
CSV start column of ints beyond int64 (uint64, or an object column of
Python ints with NaN gaps) raises pm4py's Timestamp TypeError on the log
shape, the starts compared exactly; GI1-2, the frame shapes
(`mining_input_shape=dataframe|lazyframe`) raise pm4py's errors before any
interval (start column cells as the frame's objects, lifecycle cells
lower-cased case by case: `float`/`NoneType` for a gap, the column's type
otherwise) and pair instances by the frame's dtype (a float64 column's NaN
pairs with nothing, Polars' null with a null, 1 with 1.0); GI1-3, an empty
log's alignments on a net that is not easy sound fail for supplied nets
too (`/api/align` with `miner=imported`, `/api/align-petri-workbench`, the
export's model alignments), and the decomposed fitness of an empty log
fails on a net without transitions. After: the 720 uploads 114 equal and
606 failing alike per shape and option set (45 declared Polars object
columns on the LazyFrame shape), where the frame shapes differed on 606
and the log shape on 70 before; GH's uploads 836 equal and 317 failing
alike per shape (8 untimed, declared AE2-7). The Inductive model's log
shape footprints 387 of 387 equal; the Alpha model's as in GH. Declared:
Polars' copy of a CSV object column (needs pyarrow), the decomposed
empty-log message.

GI's suite: the full ctest 672 of 672 (36 minutes, -j2, first run; the new
`minera_server_review_gi_tests` fails 26 of its 43 checks on GH's server
binary), Python script tests 406 of 406, and the 16 manifests replayed
2,673 responses unchanged.
Harness and probes: `review-harness/round-gi/`.

Round GJ re-reviewed GI and aligned Heuristics++ with a configured start
key or business hours with pm4py on CSV uploads' three shapes and on XES
uploads' frame shapes (Review GJ1). GI's CI run 45102064108 passed every
job, Windows ctest included. No device was reachable, so no newer reviewer
handoff was read from the Mac. New banks: `gen/genhcsv3.py` (600 CSV
Heuristics++ uploads jcA to jcF, seeds 12000-12599: NA-word and
zero-padded instance columns, a second start column `st2`, spans across
nights and weekends), GH's hhA and hhB and GE's and GF's heuristics banks
on the frame shapes with the new option sets (`ct/optval2.py`), 8 CSV and
2 XES probes. Fixed: GJ1-1, the CSV frame shapes read a configured start
key like the default one (any event column, the resource, lifecycle and
timestamp columns too; a `case:` key or one the frame lacks reads the
lifecycle) and with business hours raise `BusinessHours`'s errors (str:
`str.replace() takes no keyword arguments`, other non-dates
`AttributeError ... 'replace'`, NaT `cannot convert float NaN to
integer`); GJ1-2, the CSV log shape likewise (business-hours errors, a
role column as the key, NA words in a text start column); GJ1-3, the XES
frame shapes read any start key and business hours on the frame path
(pandas' NaT raises at its interval with business hours, so transitions
are read in order). After: CSV jcA-jcD 3,200 of 3,200 per shape equal or
failing alike (714 on the log shape and 2,460 per frame shape differed);
XES hhA-hhB 1,120 of 1,120 per frame shape (826 and 827 differed); GE's
and GF's banks 1,960 of 1,960 per frame shape; `/api/discover-workflow` on
jcE and jcF equal; the routes on a CSV frame's Heuristics++ net (GH's 401
uploads per frame shape) equal but for the declared empty-log fitness and
55 unbounded nets' footprints failing on both sides. Declared: the anti-
and multi-alignments of an upload without traces (pm4py's "not an sound
net" or `min()`/`max()` errors, Minera's empty-log message).

GJ's suite: the full ctest 673 of 673 (29 minutes, -j2, first run; the new
`minera_server_review_gj_tests` fails 28 of its 49 checks on GI's server
binary), Python script tests 406 of 406, and the 16 manifests replayed
2,673 responses unchanged.
Harness and probes: `review-harness/round-gj/`.

Round GK re-reviewed GJ and aligned the classic heuristics performance
view with business hours or a configured start key with pm4py on CSV
uploads' three shapes and on XES uploads' frame and log shapes, and
Heuristics++ with the activity column as the start key (Review GK1).
GJ's CI run 45120506995 passed the Linux ctest, the Python script tests,
the parity verifier and the frontend; macOS failed only
`minera_cancellation_hooks_tests` ("Subprocess aborted", a timing test GJ
did not touch; it passed here in both full runs and 3 of 3 alone), and
Windows' ctest was still
running at handoff. No device was reachable, so no newer reviewer handoff
was read from the Mac. New banks: `gen/genhcsv3.py` (GJ's generator plus
offset `+01:00` start columns, `STK=tz`, and timestamp gaps, `TGAP`): 900
CSV uploads kcA-kcF and kcT, kcY, kcZ, seeds 13000-13899; GC's and GH's
610 XES uploads (gcB-gcX, hhA, hhB) on the frame and log shapes; 6 CSV
and 11 XES probes. Fixed: GK1-1, the classic performance DFG with business
hours or a start key on a CSV upload's log shape measures each
directly-follows pair on the frame's cells (start(later) - complete(earlier),
`BusinessHours(complete, start)`): a non-date raises TypeError (the later
start's type first), a NaT reads as a zero wait (with business hours
`cannot convert float NaN to integer`), a `case:` key or one the frame
lacks raises KeyError at the first pair, offset starts read in UTC,
before the noise cleaning; GK1-2, the CSV frame shapes likewise (Polars'
None raises, renamed headers included); GK1-3, the XES frame shapes
likewise on read_xes's frame and its Polars copy; GK1-4, Heuristics++ with
the activity column as the start key compares the column's text with the
timestamps (`'<=' not supported between instances of 'Timestamp' and
'str'`, Polars `'datetime.datetime'`), and on CSV frames business hours
read the UTC clock; GK1-5, the classic performance view on an XES upload's
log shape (the hybrid legacy log, whose events hold only the keys they
carry) reads start(later) and then complete(earlier) (complete first with
business hours): a key an event lacks raises KeyError, a non-date
TypeError or `BusinessHours`' errors, where Minera raised its own "valid
event timestamp" and "missing the configured start timestamp attribute"
errors; GK1-6, a wait's seconds follow the backend: pandas'
`Timedelta.total_seconds()` of the exact difference floored to the
microsecond on a CSV log and the pandas frames (1.118 s is
1.1179999999999999), Python's `timedelta.total_seconds()` on an XES
upload's legacy log and the Polars copies (1.118; Minera had them the other
way round there, hidden by the sweeps' six-digit rounding). After: CSV
kcA-kcD 3,200 of 3,200 per shape equal or failing
alike (1,867 on the log shape and 1,871 per frame shape differed); kcE,
kcT, kcY, kcZ with start keys naming the activity, case, instance and
lifecycle columns and renamed headers 4,800 of 4,800 per shape (2,432 and
2,552 of 4,400 differed); Heuristics++ on kcT, kcY, kcZ 1,800 of 1,800 per
shape (900 and 1,008 of 3,000 differed); XES frames 6,100 of 6,100 per
shape (3,386 differed); XES log shape 8,232 of 8,540 (14 option sets; 3,100 differed), the other
308 the 22 uploads per option set whose mine stops earlier (below); exact
doubles on kcE, kcT and hhA 2,520 of 2,520 (8 differed). Declared: none new;
the mine of an XES upload whose single-event trace lacks an activity
still stops at the Inductive miner with KeyError 'concept:name' (pm4py's
variants raise it too), before the heuristics step.

GK's suite: the full ctest 674 of 674 (40 minutes, -j2) on the second
run; the first failed only FR's `review_fr_nanosecond_waits` (the first
cut of GK1-1 dropped a CSV start's nanoseconds, now kept). The new
`minera_server_review_gk_tests` (70 checks, 57 of them fail on GJ's
server binary) passes, Python script tests 406 of 406, and the 16
manifests replayed 2,673 responses unchanged.
Harness and probes: `review-harness/round-gk/`.

Round GL re-reviewed GK and aligned the classic heuristics performance
view with every XES value kind and with another timestamp key, and a
custom typed activity key on an XES upload's log shape, with pm4py
(Review GL1). GK's CI run 45148894611 passed every job, macOS and Windows
included (`minera_cancellation_hooks_tests` did not abort again). No
device was reachable, so no newer reviewer handoff was read from the Mac.
New banks: `gen/gengl.py` (1,100 XES uploads glA-glL, seeds 20000-21099:
typed start, time, `ts2` and `act2` values of every XES kind, lists,
valueless strings, NaN floats, offsets, traces without names) on the log
and frame shapes; `gen/genhcsv3.py` with `ST2=1` (60 CSV uploads glM,
seeds 21200-21259); 59 XES and 11 CSV probes. Fixed: GL1-1, the XES log
shape's classic performance view reads every value kind as the legacy log
holds it (a <list> a dict or None, a valueless string None, NaN floats,
ids) and another timestamp key's value as the complete (the start stays
time:timestamp): two numbers raise `AttributeError: 'int' object has no
attribute 'total_seconds'` (float when either is one); GL1-2, a custom
typed activity key on the XES log shape follows pm4py: a dict is
unhashable, a pair without the key raises KeyError (single-event traces
are skipped), a pair whose first element is no str is not subscriptable,
the AND measure sorts each node's successors in the dependency pass's
order (`'<' not supported between instances of 'str' and 'int'`), equal
numbers (1, 1.0, True) are one node named by the first, and `/api/mine`'s
net conversion raises `can only concatenate str (not "int") to str`; on
CSV logs the eager `'<'` rule gave way to the same successor sort and
conversion check; GL1-3, the XES frame shapes measure another timestamp
column's cells against time:timestamp (or the start key) on read_xes's
frame and its Polars copy, every column typed as read_xes types it (dates
with None NaT, text with None NaN, numbers with a gap float64, booleans
with a gap object, mixes object with dicts, None and NaN, a column of None
alone too), pandas' own TypeError for Timestamp - int, NaT beside an int a
zero wait, a missing start column's KeyError before the timestamp
column's; a time:timestamp column of other kinds no longer leaves the
default, business-hours or start-key view on Minera's own clock; GL1-4,
CSV uploads' three shapes measure another timestamp column the same way
(format_dataframe parses the date columns). After: XES log shape glA-glE
21,525 runs (25 option sets, both decorations, `/api/mine` and
`/api/discover-workflow`) with none differing (3,978 before), glF-glL
31,200 with none; XES frames with `ts2` (glG, glH, glJ, glL) 2,400 of
2,400 per shape (489 of 1,800 differed on glH, glJ, glL); XES frames with GK's
option sets on glF, glG, glK 5,400 of 5,400 per shape (960 of 4,500
differed); CSV glM with another timestamp key 720 of 720 per shape (396 of
540 differed; GK's start-key and business-hours sets on glM 360 of 360
on the log and DataFrame shapes); the routes on a classic net mined with business hours or a
start key (CSV kcE, kcT, kcZ, three shapes: token replay and footprints)
equal but for 7 unbounded nets whose footprints fail on both sides.
Carried: a custom typed activity key on XES frame shapes (pandas' and
Polars' labels and their errors: 381 and 147 of 1,800 differ) and
`/api/mine`'s steps after the net with such a key on the log shape (token
replay, decorations).

GL's suite: the full ctest 675 of 675 (38 minutes, -j2, first run; the new
`minera_server_review_gl_tests` fails all 47 of its checks on GK's server
binary), Python script tests 406 of 406, and the 16 manifests replayed
2,673 responses unchanged.
Harness and probes: `review-harness/round-gl/`.

Round GM re-reviewed GL and aligned a custom activity key naming a typed
column on XES uploads' frame shapes and CSV uploads' three shapes with
pm4py (Review GM1), and kept XES frame columns of None alone. GL's CI run
45165980512 passed every job, macOS and Windows included. No device was
reachable, so no newer reviewer handoff was read from the Mac. New banks:
GL's `gen/gengl.py` with `AC2`/`A2K` (300 XES uploads gmA-gmC, seeds
22000-22299: `act2` of every XES kind, text-heavy mixes, all events
carrying it); about 60 XES probes (`pout/`). Fixed: GM1-1, pm4py's classic
heuristics miner on read_xes's frame reads the key's column as pandas
typed it (classic_frame_cells): a dict is unhashable (pandas counts the
activities first), the first directly-follows pair whose first element is
no str raises `'<type>' object is not subscriptable`, the AND measure sorts
each node's successors as Python sorts their values (`'<' not supported
between instances of 'str' and 'int'`), equal numbers (1, 1.0, True) are
one node named by the first in row order, each NaN of a float64 column (and
an object column's parsed NaN float) is its own node while gaps share one,
values that print alike but differ (str '1', int 1) stay two nodes of one
name, and the heuristics Petri conversion record raises `can only
concatenate str (not "<type>") to str` for a node of another type with an
incoming edge (a NaN or None node included: `HeuristicsNet::
petri_conversion_type`, raised by `to_petri_net`); Polars' copy holds an
object column as str() text and every gap as None; a column of None alone
(or None and gaps) is a column of both frames; GM1-2, a CSV upload's
frames read another column as the key the same way, each cell as read_csv
typed it (ints int, a gap or NA text NaN, the timestamp column Timestamps,
datetime on Polars), with the performance view on the upload's own
timestamp column; Minera refused the resource and lifecycle columns
("event is missing activity attribute") and read the others as text;
GM1-3, `/api/mine`'s token replay and decorations of the CSV log shape read
the resource and lifecycle columns as the activity key too (KeyError
before; a missing cell stays a KeyError, as convert_to_event_log drops NaN);
GM1-4, XES frame columns whose cells are valueless (None) count as present
for every family (pandas keeps a column of None alone, of None and gaps as
float64 NaN): the Polars Alpha, log skeleton, transition system and
temporal profile refusals now name the column as pm4py does. The XES log
shape's `_post` cases (GL) are pm4py's: its token replay raises KeyError
for an event without the key. After: XES frames with `act2` (glD, glK, glL,
gmA-gmC, 13 option sets with both decorations and the conversion record)
4,414 of 4,414 pandas and 4,976 of 4,976 Polars judgments equal (381 and
147 of 1,800 differed on GL's binary); CSV frames with seven typed keys
(glM, kcE, kcT) 3,018 of 3,022 per shape (1,954 differed), the log shape
with the replay 2,593 of 2,600 (367 differed); the left cases are uploads
with missing timestamps (declared below). /api/mine's families on gl
banks with the default keys: Polars temporal profile refusals 54 to 39
differing; the rest is carried (below).

GM's suite: the full ctest 676 of 676 (43 minutes, -j2, first run; the new
`minera_server_review_gm_tests` fails 29 of its 42 checks on GL's server
binary), Python script tests 406 of 406, and the 16 manifests replayed
2,673 responses unchanged.
Harness and probes: `review-harness/round-gm/`.

Round GN re-reviewed GM and aligned /api/mine's Alpha, temporal profile,
Inductive and POWL families on XES uploads' frame shapes whose
time:timestamp and start_timestamp columns hold other XES kinds with pm4py
(Review GN1), and a CSV upload's frames keyed by a column pm4py's event log
lacks (GN2). GM's CI run 45188920161 passed Linux, the Python script tests,
the frontend and the verifier (macOS and Windows still ran at handoff). No
device was reachable, so no newer reviewer handoff was read from the Mac.
New banks: `gen/gengn.py` (2,600 XES uploads gnA-gnC, seeds 30000-32599:
timestamp and start columns of dates, offset dates, date text, text, ints,
floats, NaN, booleans, None, lists and gaps), GL's `gengl.py` with `AC2`
and `TS2` (500 uploads gnD-gnE, seeds 33000-33499); about 200 probes.
Fixed: GN1-1, read_xes types each column (a new `xes_frame_typed.hpp`:
dates datetime64, text str, ints int64 or float64 beside a gap, booleans
bool or object beside a gap, mixes object; an empty `<list/>` or a
valueless value is None, NaT in a dates column, and a dates column's NaN
floats are NaT); pandas sorts the frame by case, start and timestamp with
each column's Categorical codes (the last key coded first: a dict is
unhashable, numbers beside dates are "'values' is not ordered", a gap
sorts last), so Alpha and Inductive read that order instead of parsing
dates ("Alpha: invalid selected timestamp", "timestamp field ... missing or
null" before); GN1-2, pandas' temporal profile compares each pair's
timestamp with the later row's start and subtracts them with pandas' own
errors for every dtype pair (`Invalid comparison between dtype=... and
ndarray|StringArray`, elementwise `'<=' not supported between instances
of 'Timestamp' and 'T'` or the reflected `'>='`, `cannot subtract
DatetimeArray from ndarray[object]`, numpy's boolean subtract, Python's
`unsupported operand type(s) for -`, then `Can only use .dt accessor with
datetimelike values`), no pair left an empty profile and one measured pair
a profile of one entry; GN1-3, Polars' copy holds an object column as
str() of each cell (True/False, Python's float repr, a list's dict repr)
and sorts unstably with a missing value first (Alpha, Inductive), and its
temporal profile fails on the two columns' dtypes alone, rows or none: a
Datetime against a String column at the pairs' filter ("cannot compare
'date/datetime/time' to a string value"), against a numeric or Boolean one
and a String against a numeric one resolving the flow time ("- not allowed
on S and T", "arithmetic on dtypes ... try an explicit cast first" with
its nested plan), the rest evaluating it (String or Boolean subtraction,
"expected `Duration`, got `i64|f64`" with the narrower side cast); GN1-4,
the temporal profile's date path reads a dates column's NaN floats, empty
lists and valueless values as NaT on both shapes; GN2, pm4py's classic
heuristics miner on a CSV upload's frames reads the frame's event log,
whose events lack the case columns: keyed by such a column (or one the
frame lacks) a log of single-event traces (rows without a timestamp
dropped) leaves a net without activities (Minera refused the key; with a
pair, Minera keeps naming the key where pm4py raises KeyError). After:
/api/mine's seven families on XES frames (gl banks glA-glL and gnA-gnC,
3,700 uploads per shape) differ only where declared: pandas 4 Inductive
and POWL operand orders (float/str), Polars 225 (NoneType/str, which vary
inside one process: 25 against 6 on glA with one seed), where GM's binary
differed for Alpha 527/233, the temporal profile 548/196 and Inductive
122/154 (pandas/Polars); GM's typed custom keys with business hours, a
start key and `ts2` as the timestamp key on fresh banks (gnD-gnE, both
decorations and the conversion record) 4,000 of 4,000 judgments per shape;
CSV uploads with seven typed keys (kcE, kcT) 2,000 of 2,000 per frame
shape (the four uploads with missing timestamps included) and 1,993 of
2,000 on the log shape (the seven replays carried below).

GN's suite: the full ctest 677 of 677 (-j2; the new
`minera_server_review_gn_tests` fails 59 of its 74 checks on GM's server
binary), Python script tests 406 of 406, and the 16 manifests replayed 2,673
responses unchanged.
Harness and probes: `review-harness/round-gn/`.

Round GO re-reviewed GN and aligned Heuristics++, the classic performance
view and Declare on XES uploads' frame shapes whose time:timestamp and
start_timestamp columns hold other XES kinds (Review GO1), and the CSV log
shape's token replay keyed by a column with gaps (GO2). GN's CI run
45208653724 passed Linux, the Python script tests, the frontend and the
verifier (macOS cancelled, Windows ctest still running at GO's start). No
device was reachable, so no newer reviewer handoff was read from the Mac.
GN's fixes held: `famval.py` on fresh GN-style banks agreed. New banks:
`gen/gengn.py` with GO seeds (goA-goC, 1,300 XES uploads), `gen/gengo.py`
(goL, goM: 700 uploads with lifecycle and instance columns beside typed
timestamps), `gen/genk.py` (goK: 400 CSV uploads whose text, float, date,
int, bool and mixed columns have gaps); about 30 probes.
Fixed: GO1-1, pm4py's Heuristics++ on such a frame (observed black-box)
converts it to an event log that keeps each typed cell: to_interval
subtracts each complete's cell and its start's (numbers raise
`'int'|'float' object has no attribute 'total_seconds'`, NaT beside an int
subtracts to NaT, a Timestamp minus an int raises pandas' own TypeError,
text, None and dicts `unsupported operand type(s) for -`), the abstraction
sorts each trace by its start cells (Python's list.sort, `'<'` errors),
reads the activity and timestamp keys (KeyError after the first trace's
sort), compares complete <= start for the first following event, then
subtracts start - previous complete over every trace and reads
timestamp() of every start, then every complete (NaT `NaTType does not
support timestamp`, other kinds AttributeError); Minera answered
"Heuristics Miner PLUSPLUS requires a valid event timestamp" for every
such frame (shared `plus_subtract_seconds`, both shapes); GO1-2, with
those cells a lifecycle gap raises lower() only where the transitions are
read in order, after an earlier interval's subtraction error (Minera
raised lower() first); GO1-3, the classic performance view keeps its
frame-cell waits only without a custom start key or business hours;
GO1-4, Declare's frame check reads the columns as read_xes types them on
both shapes (`xes_frame_typed_dtypes`), conformance on a Polars frame
included (Minera refused a dates column whose other cells are valueless
lists, "should (at least) contain a column of type date"), and the Polars
sort compares the typed time:timestamp cells. GO2-1, convert_to_event_log
keeps NaN (one object in a text or object column, a fresh float per cell
in a float64 column) and NaT in every event, so `/api/mine`'s replay of
the heuristics net keyed by such a column reads the cells
(`csv_log_keyed_replay.hpp`): it first sorts the net's transitions by
name, where a gap's node beside a text name raises `'str' and 'float'`
when it leads the net's activities, else the reverse (bools and numbers
order beside NaN), then sorts the variants by (count, tuple) descending
(NaN or NaT beside text raises), then replays each cell against the node
of an equal object, dates as UTC Timestamps; the decorations read the
same log (Minera raised KeyError for a gap, and missed every transition
of a date column).
After, per shape: Heuristics++, Heuristics++ performance and the classic
performance view on goA-goC and goL/goM 2,000 of 2,000 uploads; Declare
discovery and conformance 2,000 of 2,000 but the declared operand order
of pandas' rule-column sort (39 uploads at PYTHONHASHSEED=0, 'float'
first in about a third of the missing-activity uploads, changing with the
seed); kcT's seven keyed replays equal; goK 2,400 key judgments, every
error and every replay total equal, 30 nets with several gaps in a
float64 column differ (open, below). The option sets with business hours
and custom timestamp or start keys on goA/goL (250 uploads each) differ
as they did on GN's binary (open, below).

GO's suite: the full ctest 678 of 678 (-j2; the new
`minera_server_review_go_tests` fails 43 of its 44 checks on GN's server
binary), Python script tests 406 of 406, and the 16 manifests replayed 2,673
responses unchanged. Regression on older banks: GL's glA-glL (1,100 XES
uploads per shape) for the heuristics miners 4,400 of 4,400 and Declare
2,200 of 2,200 but the declared order; CSV kcE and kcT on three shapes
every judgment equal.
Harness and probes: `review-harness/round-go/`.

Round GP re-reviewed GO and aligned the temporal profile on XES uploads'
frame shapes with typed timestamp and start columns under business hours or
custom keys (Review GP1), Heuristics++ with start_timestamp as the
timestamp key and the classic performance view's KeyError order (GP2), and
`/api/discover-graph` footprints on XES frame sessions (GP3). GO's CI run
45232314482 passed Linux, macOS, the frontend, the Python script tests and
the verifier (Windows ctest still running at GP's handoff). No device
was reachable, so no newer reviewer handoff was read from the Mac. GO's
fixes held: Heuristics++, Heuristics++ performance, business hours and a
start key on fresh GO-style banks (gpA 600, gpL 500 uploads per shape,
`gen/gengn.py` and `gen/gengo.py` with GP seeds) equal pm4py's.
Fixed: GP1-1, pandas' temporal profile with business hours applies the
business seconds per pair row (observed black-box): an empty pair frame
raises `ValueError: Cannot set a DataFrame with multiple columns to the
single column @@flow_time` (pairs whose row lacks an activity count; the
groupby drops them later), and BusinessHours strips tzinfo from the
earlier cell, then the later one (text `str.replace() takes no keyword
arguments`, other kinds AttributeError; a datetime64[UTC] column reads the
UTC wall, an object column's datetime its own wall); Minera subtracted
first (`xes_business_cells`, `xes_pandas_tp`). GP1-2, Polars filters the
pairs by index and complete <= start before its UDF: Datetime beside String
raises the resolution error, beside Boolean a SchemaError, a number beside
String a ComputeError, whatever the rows; otherwise the UDF raises for the
first non-datetime argument (`polars_typed_tp_business`). GP1-3, with a
custom key pm4py's default start key is start_timestamp, not the route's
timestamp target; a start key equal to the timestamp key reads that column
once (pandas and the Polars plan, the missing-case alias included). GP2-1,
Heuristics++ with start_timestamp as the timestamp key reads the same column
as its start, so to_interval keeps the log and the abstraction keeps frame
order (Minera sorted the start and end activities, or raised "interval input
is missing the configured start timestamp"). GP2-2, the classic performance
view reads the later event's start before the earlier one's timestamp: a
start key naming no column raises its own KeyError first. GP3-1, pm4py's
footprints of an XES frame session convert the frame to an EventLog in which
a missing activity is NaN (pandas, one shared object) or None (Polars), one
more activity in the log's and the traces' footprints, the precision and the
fitness (Minera answered "activity selector is missing or null"); GP3-2, the
conversion groups the rows by case id in order of first appearance, so the
XES traces without a case id are one trace (Minera kept them apart, so the
log footprints, precision and fitness differed wherever two or more
traces lacked one); a frame without the activity column raises pm4py's
`KeyError: 'concept:name'`.
Open: the CSV log shape keyed by a float64 column with several gaps (pm4py's
heuristics net has one node per NaN object, each its own transition; Minera
names one `nan` node: activities are text keys throughout the miner).
Declared: the order of pandas' missing-key list in the temporal profile's
selection with a custom activity key (a set iteration, changes with the hash
seed: 4 gpAs uploads at PYTHONHASHSEED=0).
After, per shape: the option sets with business hours and custom keys
(`ct/optval.py`, 12 sets) on gpA and gpL 7,200 of 7,200 and 6,000 of
6,000 judgments (GO's binary differed in 3,246 of gpAs/gpLs' judgments
over both shapes, `out/o1_*`); the footprints route
(`ct/fpmiss.py`) on gpA 414 of 414 uploads with a heuristics net (11 with
a missing activity) and on gpLs 180 of 180 (6) equal in the log's and the
model's footprints, the log_model violations, precision and fitness; 20
and 4 unbounded nets fail on both sides (pm4py does not finish). Families
on GO's goA50 (`ct/famval.py`) agree but the declared operand order of
Polars' Inductive/POWL sort TypeError (3 uploads, as on GO's binary).

GP's suite: the full ctest 679 of 679 (-j2; the new
`minera_server_review_gp_tests` fails all 68 of its checks on GO's server
binary), Python script tests 406 of 406, and the 16 manifests replayed
2,673 responses unchanged.
Harness and probes: `review-harness/round-gp/`.

Round GQ re-reviewed GP and aligned the temporal profile's conformance on
XES uploads' frame shapes (Review GQ1), `/api/mine`'s footprints family on
XES frame sessions (GQ2), a CSV upload's log shape keyed by a float64
column with several gaps (GQ3, open since GO) and `/api/discover-graph`
transition systems keyed by a typed column of an XES frame (GQ4). GP's CI
run 45249798603 passed every job (Linux, Windows and macOS ctest, the
frontend, the Python script tests and the verifier). No device was
reachable, so no newer reviewer handoff was read from the Mac. GP's fixes
held on fresh banks (gqA 600, gqL 500 uploads per shape with GQ seeds):
GP's option sets with business hours and custom keys (`ct/optval.py`, 12
sets) agree in 12,000 of 12,000 judgments on gqAs and gqLs (the first 250
of gqA and gqL) over both shapes, and `/api/discover-graph` footprints
(`ct/fpmiss.py`) in 4,068 of 4,068 (24 unbounded nets fail on both sides).
Fixed: GQ1-1, `/api/analyze-behavior` temporal_conformance on a frame
session read the XES traces; pm4py reads the frame as its discovery does
(round GP): pandas raises `KeyError: 'case:concept:name'` for a missing case
column before anything else, Polars a select failure; the typed timestamp
and start columns, the business seconds per pair row (an empty pair frame
raises the same ValueError as discovery) and the custom keys follow GP's
rules; a Polars plan error past the join names the join's plan (RESOLVING
'select', the context stack select, join left, join, filter, select), and
a missing activity column fails the plan at its final select. The route's
default zeta is 1.0 (pm4py's wrapper), and a deviation needs distance >
zeta (a zero deviation the sys.maxsize sentinel). GQ2-1, `/api/mine`'s
footprints family on a frame session required every cached model (the
process tree and POWL of the default variants), so a session whose Inductive
or POWL discovery failed answered "requested footprint bundle requires
successfully discovered source models"; it now reads only the models its
selection needs, as `/api/discover-graph` does (round GD), with the frame's
missing label (NaN on pandas, None on Polars) and its case grouping (GP3).
GQ3-1, convert_to_event_log holds each gap of a float64 column as its own
NaN object, so pm4py's heuristics miner names one node per gap (each its own
transition: the token replay's variants, generalization and simplicity
count them); Minera named one `nan` node (`distinct_float_gap_labels`, the
names told apart by trailing \x1f and shown as printed). GQ4-1, a transition
system keyed by a typed column of an XES frame reads each cell's Python
object: str() names it (a Timestamp `2021-03-05 08:06:00+00:00`), Python's
== matches transition labels (int 1 and float 1.0 alike, a NaN or NaT
never), a state's list or set matches a NaN by its object (each cell of a
float64 column its own) and NaT always, and pandas' set and multiset views
hash a dict cell (`TypeError: unhashable type: 'dict'`); Minera read the
XES text (`frame_typed_activities` now in its own header).
Declared: the set view's state names are Python sets, printed in hash-slot
order, so two equal sets built in a different insertion order can stay two
states in pm4py (11 of 2,000 set-view judgments over both banks and
shapes; str hashes vary with the seed, None's with its address); Polars' temporal
conformance lists its cases in unique() order (nondeterministic).
Open: a transition system keyed by a column holding values Python finds
equal but prints apart (0 and False, 1 and 1.0): pm4py names each state and
transition after the object that created it, in its case order, while
Minera names each value after its first row (1 of 250 gqLs uploads per
key). Also fixed on the way: Polars' transition system with the case key
equal to the activity key raises DuplicateError at the group_by (Minera
answered).
After, per shape: temporal conformance (`ct/tpcval.py`, 12 option sets) on
gqAs and gqLs 12,000 of 12,000 judgments (GP's binary differed in 4,647 of
gqAs' 6,000, `out/t4_*`) and the hand-made frames without a case column,
an activity or a timestamp 72 of 72; the footprints family
(`ct/fpmine.py`) 5,424 of 5,424 judgments over 339 uploads per shape with a
heuristics net (GP's binary failed 275 of them over both shapes with the
bundle error, `out/f0_*`), 24 unbounded nets failing on both sides; the
CSV log keyed by K1-K6 (`ct/csvk.py`, gqK 400 uploads) 3,318 of 3,318 nets, replay totals and
generalization/simplicity/precision (GP's binary differed in 42 nets);
transition systems (`ct/tsgraph.py`, 10 option sets, 5 of them typed keys)
9,487 of 10,000 judgments equal on gqAs and gqLs; 500 more (Polars with the
case key equal to the activity key) agree after the DuplicateError fix
(rechecked on 40 uploads), 11 set views are declared and 2 open.
GQ's suite: the full ctest 680 of 680 (-j2; the new
`minera_server_review_gq_tests` fails all 80 of its checks on GP's server
binary; the Polars DuplicateError fix came after it, then the transition
system, server and review tests passed again), Python script tests 406 of
406, and the 16 manifests replayed 2,673 responses unchanged (a first
replay caught the footprints family's bundle error changing on a table
session: the selection rule now applies to XES frame sessions only).
Harness and probes: `review-harness/round-gq/`.

Round GR re-reviewed GQ and aligned `/api/mine`'s footprints family with a
custom footprints key on XES frame sessions (Review GR1), the families of a
CSV upload's log shape keyed by a typed column or one with gaps (GR2), the
footprints bundle's error when its model's discovery fails on a frame
session (GR3) and the temporal profile on CSV uploads' frame shapes (GR4,
the re-review of GQ1). GQ's CI run 45270284863 passed every job that ran
(Linux and Windows ctest, the frontend, the Python script tests and the
verifier); macOS was cancelled without a runner. No
device was reachable, so no newer reviewer handoff was read from the Mac.
Fixed: GR1-1, pm4py converts the frame to an EventLog grouped by the
footprints case key's column (traces in the order of their first row, rows
in frame order, values equal as Python keys them: int 1, float 1.0 and True
alike, each NaN of a float64 column its own case, a text column's gaps one,
NaT one); Minera kept the XES traces. GR1-2, the activity key reads each
cell's Python object (`frame_typed_activities`): one label per value Python
tells apart, a dict `TypeError: unhashable type: 'dict'` on pandas; a key
the frame lacks raises `KeyError` (the case key's first), and a `case:`
column, which the conversion moves to the trace, raises it as an activity
key (Minera answered "activity selector is missing or null"). GR2-1, a CSV
log shape keyed by another column of its frame reads convert_to_event_log's
cells for Alpha (with its token replay), the temporal profile, the
transition system and the log skeleton: a gap is NaN (one object in a text
column, one per cell in a float64 column, `distinct_float_gap_labels`) or
NaT, dates UTC Timestamps; the transition system's NaN or NaT label equals
none (each step its own transition), the log skeleton's NaN or NaT is its
own equivalence, and an Alpha net naming a text column's NaN raises the
replay's sort TypeError beside a str name (operand order declared, a set).
GR3-1, on a frame session a footprints bundle whose model failed answered
"requested footprint bundle requires successfully discovered source
models"; pm4py raises the model discovery's own error (the heuristics
miner's `TypeError: 'float' object is not subscriptable` on pandas, the
`'NoneType'` one on Polars), which the bundle now forwards (the model's
family, then its Petri conversion, then the discovery variants' models).
GR4-1, pm4py's temporal profile on a CSV upload's frame without a start key
reads the frame's `start_timestamp` column when there is one (format_dataframe
types it as dates), else the timestamp itself; Minera read the timestamp
(discovery and `/api/analyze-behavior` conformance, both shapes). GR4-2,
pandas compares the timestamp with a start column of ints or floats
(`TypeError: Invalid comparison between dtype=datetime64[us, UTC] and
ndarray`) or text (`... and StringArray`) before anything else, business
hours or not; with the timestamp as its own start (a custom timestamp key
and no start column) it raises `KeyError` for a column the frame lacks, and
without business hours `AttributeError: Can only use .dt accessor with
datetimelike values` (numbers) or `TypeError: unsupported operand type(s)
for -: 'str' and 'str'` (text). Two workbench fixtures on CSV frames
without their timestamp column now show `KeyError: 'absent clock'` and
`KeyError: 'timestamp'` for the temporal profile family (rewritten).
Open: footprints with time:timestamp as the activity key on pandas (2 of
250 grAs uploads): two traces' NaN start activities stay two objects in
pm4py's sets (Minera one `nan`), and 0 and False are named after the first
object (as GQ's transition systems).
After, per shape: the footprints family with custom keys (`ct/fpmine.py`,
7 key sets: case key concept:name, start_timestamp, time:timestamp;
activity key org:resource, start_timestamp, time:timestamp,
case:concept:name) on grAs (250 uploads) 11,892 of 11,900 judgments over
both shapes with a heuristics net (42 fail on both sides: unbounded nets,
reference timeouts, unhashable dicts) and on grA60 2,669 of 2,674; the
model-error uploads (`ct/fpmine.py` now compares them) 18 of 18 per shape on
grA60; the CSV log families (`ct/csvfam.py`, K1-K6) on fresh grK uploads (61) 1,464 of 1,464 judgments for Alpha with its replay, the temporal profile, the transition system and the log skeleton (`out/c2_*` and `out/c3_*` on grK40 before the fixes); the CSV
frames' temporal profile (`ct/tpdcsv.py`, kcT 100 uploads, 6 option sets)
on pandas 600 of 600 judgments (GQ's binary 308 of 600, `out/td_*`) and on Polars 445 of 600 (GQ's 307): 132 reference-only Polars plan errors on start columns of other values than dates and 23 plan errors of a missing timestamp column are open; the conformance (`ct/tpccsv.py`, 12 sets) on pandas 1,126 of 1,200 (GQ's binary 575 of 1,100 sets without `szzz`, `out/t4_*`; open: 51 business-hours judgments with the start column as the timestamp key, 23 reference-only ValueErrors of business hours on an empty pair frame), on Polars 504 of 1,200 (GQ's 387 of 1,100; open: the deviations' order within a case, Polars' plan errors and business hours).
GR's suite: the full ctest 681 of 681 (680 at -j2 beside sweeps, then the
new `minera_server_review_gr_tests`: 194 checks, 175 of which fail on GQ's
server binary), Python script tests 406 of 406, the active-source fixture
vitest files 298 of 298, and the 16 manifests replayed 2,673 responses with
the 2 CSV frame captures above changed as intended (rewritten on a fresh
server over the whole manifest).
Harness and probes: `review-harness/round-gr/`.

Round GS re-reviewed GR and aligned the temporal profile and its
conformance on CSV uploads' frame shapes (Review GS1 Polars, GS2 pandas,
the rest of GR4) and re-ran GR1's custom footprints keys on a fresh bank
(GS3, no change). GR's CI run 45293923182 passed the Linux ctest, the
frontend, the Python script tests and the verifier; its macOS build and
Windows ctest were still running at GS's handoff. No device was reachable,
so no newer reviewer handoff was read from the Mac.
Fixed: GS1-1, Polars resolves the pair frame's plan over a CSV frame's own
dtypes (`csv_frame_temporal_start`, the plan text over the frame's names:
format_dataframe's copies case:concept:name, concept:name and
time:timestamp, its "@@index", no row index): a start column of ints,
floats, Booleans or text beside the dates fails with the plan errors of an
XES upload's frame (`detail::polars_tp_dtype_error`, now shared as
`polars_typed_tp_error`, `polars_tp_business_dtype_error` and
`polars_tp_udf_error`), a frame without its timestamp column fails at the
with_columns, and with business hours only the filter resolves, the first
pair that passes it then failing in the UDF's SchemaError (no pair: an
empty profile, and a conformance listing its cases without deviations);
Minera answered the profile or another error. GS1-2, the Polars
conformance of a CSV frame lists each case's deviations in pair order:
the pair count read the configured start key on the converted log, which
holds the start as `__minera_temporal_target`, so every CSV frame's pair
frame looked empty and the deviations followed the profile's order. GS1-3,
the business hours SchemaError on a CSV frame names the timestamp copy
time:timestamp and orders the pairs by the frame's "@@index"
(`discover_temporal_profile_named_start`, `conform_temporal_profile_named_start`).
GS2-1, pandas with business hours and the timestamp as its own start (a
custom timestamp key and no start column) measures the pairs row by row:
a value that is no datetime raises in BusinessHours (`AttributeError:
'float' object has no attribute 'replace'`, `TypeError: str.replace() takes
no keyword arguments`), no pair leaves the empty frame's `ValueError:
Cannot set a DataFrame with multiple columns to the single column
@@flow_time`, which the conformance raises before it reads the profile.
GS2-2, without business hours a text column as its own start subtracts
only once a pair passes the filter (`TypeError: unsupported operand
type(s) for -: 'str' and 'str'`, else the .dt `AttributeError`), and a
Boolean column fails numpy's subtraction at any length (`TypeError: numpy
boolean subtract, ...`). GS2-3, a Boolean column with a gap is read_csv's
object column of Python bools: as the start beside the dates each present
pair (NaN skipped) raises `TypeError: '<=' not supported between instances
of 'Timestamp' and 'bool'`, no such pair `TypeError: cannot subtract
DatetimeArray from ndarray[object]` (business hours: the empty frame's
ValueError); as its own start the .dt `AttributeError`; Polars' copy holds
it as text ("True"/"False").
After, single-threaded Polars (POLARS_MAX_THREADS=1; with several threads
pm4py's UDF can name another failing pair, 3 of 1,200 conformance
judgments): the CSV frames' temporal profile (`ct/tpdcsv.py`, kcT 100
uploads, 9 option sets) on Polars 900 of 900 judgments (GR's binary 445 of
600 over its 6 sets, `out/d0_la`) and on pandas 900 of 900; the conformance
(`ct/tpccsv.py`, 12 sets) on Polars 1,200 of 1,200 (GR's 504) and on pandas 1,200 of 1,200 (GR's 1,126); `tc/mkgs.py`
over kcT and 12 synthetic uploads (Boolean, float, int, text and object
start columns, `syn/`, `syn2/`) 4,256 of 4,256 discovery and conformance
cases on the final binary. XES frames after the refactor: the conformance
(`ct/tpcval.py`, 12 sets) on grA60 1,440 of 1,440 over both shapes and the
discovery option sets (`ct/optval.py`, 10 `tp_*` sets) 1,199 of 1,200, the
one a pandas `KeyError` listing the missing columns of an unknown activity
key in an order that follows the hash seed (PYTHONHASHSEED 0 and 1
`['time:timestamp', 'org:resource']`, 2 and 3 Minera's order; declared).
GS3: the footprints family with 8 other custom keys (`fpk2.sh`: activity
key lifecycle:transition, concept:instance, a Boolean column, a dict
column, org:resource; case key lifecycle:transition, concept:instance, the
Boolean column) on a fresh bank of 200 XES uploads (`gen/gengs.py`, seeds
67000-67199) 18,397 of 18,397 compared judgments over both shapes (112 fail
on both sides: reference timeouts, unbounded nets).
Measured and carried: Declare and Inductive on a CSV log keyed by a column
with gaps (`ct/csvfam.py` FAMS=declare,inductive; pm4py's Inductive recurses
past Python's limit or raises a TypeError on distinct NaN objects across
traces, and its Declare sorts rules keyed by tuples beside scalars into a
TypeError; Minera refuses the null activities), the 2 grAs footprints
uploads with time:timestamp as the activity key (mixed-type timestamps).
GS's suite: the full ctest 682 of 682 (with the new
`minera_server_review_gs_tests`: 260 checks, 236 of which fail on GR's
server binary), Python script tests 406 of 406, and the 16 manifests
replayed 2,673 responses unchanged (the frontend is untouched).
Harness and probes: `review-harness/round-gs/`.

Round GT re-reviewed GS and the carried CSV families (fresh banks:
`gen/gentp.py` gtT, 120 CSV uploads with start columns of every kind,
seeds 68500-68619; gtK, 200 CSV uploads keyed by six columns with gaps,
seeds 68000-68199; `gen/gents.py` gtS, 200 XES uploads whose column k
mixes Python-equal values, seeds 69000-69199; `gen/genst.py` gtP, 200 CSV
uploads with a valid start_timestamp column, seeds 69500-69699).
GT1-1, the temporal profile and its conformance on a CSV upload's log
shape read convert_to_event_log's events, per trace and pair i < j the
earlier event's source cell then the later one's target cell through
`.timestamp()` (the default target key is time:timestamp, not the source
key): the first cell that is no Timestamp raises `AttributeError: '<type>'
object has no attribute 'timestamp'`, a NaT `ValueError: NaTType does not
support timestamp`, an absent key `KeyError` (new
`check_csv_log_temporal_reads`, also on `/api/analyze-behavior`'s variant
log). GT1-2, read_csv keeps a column of ints above int64 beside a negative
cell, or of uint64 ints beside a gap, as its text (str); the importer typed
it Python ints (`csv_event_log_importer.cpp`, the event, case, case-id and
activity columns). GT1-3, a pandas frame's start column of Python ints
past uint64 (object) compares with the dates (`'<=' not supported between
instances of 'Timestamp' and 'int'`), and a uint64 own start with business
hours measures as itself; Polars' Int64 cast of such a column leaves no
pair. After, on gtT: the log shape's discovery (`ct/tpdcsv2.py`, 9 sets)
1,080 of 1,080 judgments (GS's binary 580) and conformance
(`ct/tpccsv2.py`, 15 sets) 1,800 of 1,800 (GS's 1,009); the pandas frame
1,042 of 1,080 (GS's 923), the rest 4 uploads of carried kinds (a Boolean
column whose gap rows format_dataframe dropped, text with NaN beside
dates); Polars 1,025 of 1,080 as before (a uint64 column's plan text and
business-hours UDF error, carried).
GT2-1, Declare keyed by a typed column: pm4py's rule table holds (template,
act, act2), so an int64 column's second activities show as floats ("2.0"),
and a falsy or missing second activity (0, 0.0, False, NaN, NaT) leaves
the rule under act alone, which the conformance fails on in its template
order (`TypeError: '<type>' object is not subscriptable`, or `cannot
unpack non-iterable <type> object` from nonsuccession and
nonchainsuccession; `check_csv_keyed_declare_keys`,
`rename_csv_keyed_declare_targets`). GT2-2, a text column with gaps fails
the rules' sort (`'<' not supported between instances of 'str' and
'float'`; the operands' order follows the NaN's hash, declared). GT2-3, a
float64 column's gaps are one activity each, an object column's one;
pm4py's rule sets can also hold equal NaN keys apart (declared). GT2-4,
Inductive keyed by a column of one value in one-event traces fails with
`__str__ returned non-string`, other single-value logs concatenate
(`frame_keyed_inductive_error`). After (`ct/csvdcl.py`, gtK, 6 keys):
1,315 of 1,460 judgments (GS's binary 393 of 1,416); the rest are the text
column's operand order and pm4py's separate NaN keys (one activity per
float64 gap measured worse, 1,306). GT3, the DFG's EventLog counters (native, frequency_triples,
case_attributes) give each gap of a CSV upload's float64 key column its
own node, as convert_to_event_log's NaN objects never match
(`dfg_variants.cpp`): `ct/csvdfg.py` on gtK (7 keys, 6 variants) 8,400 of
8,400 (an earlier binary differed on the float64 key in 2 of the first
6 uploads); the variants route
keyed by those columns (`ct/csvvar.py`) 1,400 of 1,400 and 118 of 120 on
gtT's start columns (the 2 carried Boolean uploads). GT4, the transition
system keyed by a column of mixed objects (0 and False, 1, True and 1.0)
names each state after the window that created it and each transition
after the activity of its first step, and places a state in the root set
by its own objects' repr (`transition_system.cpp`, `TraceSequence::shown`):
`ct/tsgraph2.py` on gtS (5 view/window sets) on pandas 967 of 1,000
judgments (GS's binary 691); the rest pick between two value-equal root
states (a step from [0] to [False]) the one Python's set iterates first,
which follows str hashes (PYTHONHASHSEED 0, 1, 4 and 5 [False], 2 and 3
[0]; declared); on Polars 990 of 1,000 before and after (a set's repr
order, the same hash rule; declared). The 2 grAs footprints uploads with
time:timestamp as the activity key are the same kind (declared): two
traces' NaN start activities stay two objects in pm4py's sets, and 0 and
False keep the object the frame's grouping meets first.
Found and carried (GT5): pm4py's `discover_dfg` and
`discover_performance_dfg` on an upload's frame read its start_timestamp
column when it has one (as GC1-4's Alpha and temporal profile do): each
case's rows sorted by (start, timestamp), NaT last and ties stable, give
the directly-follows pairs; a pair's duration is the later start minus the
earlier completion, signed (business hours clamp below at 0), a NaT start
NaN (skipped by the aggregations; an edge of only NaN durations has sum 0
and NaN otherwise); start and end activities keep the timestamp order; an
all-empty start column raises `TypeError: cannot subtract DatetimeArray
from ndarray[float64]`, a text or mixed one its own errors. The EventLog
path (native table sources) reads the start attribute too, clamped at 0,
in timestamp order. Minera's process map (`/api/mine` edges, start and end
counts, `performance.edges`) and the DFG performance variant ignore it:
`ct/pmap.py` on gtP 47 of 200 uploads with equal frequency edges and 16
with equal performance edges (`x/pst.py` holds the probes).
GT's suite: the full ctest 683 of 683 (with the new
`minera_server_review_gt_tests`: 169 checks, 133 of which fail on GS's
server binary), Python script tests 406 of 406, and the 16 manifests
replayed 2,673 responses unchanged (the frontend is untouched).
Harness and probes: `review-harness/round-gt/`.

Round GU re-reviewed GT and took GT5. GT's CI run 45324381722 passed on
Linux, macOS, the frontend, the Python scripts and the parity verifier
(its Windows job was still building at GU's handoff). Fresh bank:
`gen/genstx.py` guX, 200 uploads, XES and CSV alternating, whose
start_timestamp column holds dates (gaps, swaps, ties, XES offsets),
ints, floats, text, booleans, date text, mixed cells or nothing, seeds
71000-71199; with GT's gtP. GU1, pm4py's frame DFG reads an upload's
start_timestamp column (`frame_dfg.cpp`, `frame_dfg_start_order`): each
case's rows sorted by the column's own pandas ranks, then the timestamp,
ties in frame order and a gap last, give the directly-follows pairs of
the process map (`/api/mine` edges) and of the DFG performance variant
(`/api/mine`, `/api/discover-dfg`); a pair's duration is the later row's
start minus the earlier row's timestamp, signed, a NaT start NaN, which
the aggregates skip (an edge of only NaN durations sums to 0 and is NaN
otherwise, now written as null rather than refused by the variant's
finite check); start and end counts keep the timestamp order; business
hours clamp at 0 and fail on a NaT start (`ValueError: cannot convert
float NaN to integer`). GU1-2, a start column of no dates no longer
fails the mine: the edges keep the column's order and carry no timings,
and `performance.edges_error` holds pm4py's `TypeError: cannot subtract
DatetimeArray from ndarray[float64]` (numbers, all empty) or
`ndarray[object]` (text, booleans, mixed); the performance variant fails
with it, with business hours at the `.dt` accessor. A column pandas cannot
sort (an XES column of mixed Python objects) fails the mine with
discover_dfg's `TypeError: 'values' is not ordered, ...` (declared: the
process map is the mine's core result). After (`ct/pmap2.py`): gtP 200 of
200 uploads with equal edges, start and end counts, performance edges and
variant (GT's binary 47, 200, 16 and 16); guX 194 mines with equal edges
and start and end counts and 6 equal mine errors, 101 equal performance
edges and 93 equal `edges_error`, 101 equal variants and 99 equal errors.
GU2, passed time without a start key (the pandas variant) pairs each row
with the next row's start, unclamped, signed, NaN pairs counted in the
frequency (`passed_time.cpp`), and cycle time on a CSV upload's frame
takes the start_timestamp column format_dataframe names in its attrs
(read_xes names none; `handle_cycle_time`): `ct/ptc.py` on gtP 0 diffs
in 100 cycle times and 822 passed-time judgments, and in 420 more with
business hours (GT's binary on the first 50 uploads: 42 of 50 cycle
times and 339 of 420 judgments wrong, 355 of 420 with business hours); on
guX the only diffs are uploads whose mine fails
(the routes then answer from the previous session). GU3,
`/api/dfg-dataframe` with pandas' default start (the frame's own
start_timestamp column, unclamped) kept no edge for a NaN flow; it now
keeps it with NaN aggregates as above, and business hours fail converting
it (`dfg_dataframe.cpp`): `ct/dfgdf.py` on gtP 600 of 600 (GT's binary 38
performance and 38 both-measure results wrong of 200 each), with business
hours 150 of 150 on 50 uploads. Declared: native table sources' EventLog
names no start key in its properties, so its performance DFG keeps
time:timestamp (an EventLog converted from a format_dataframe frame
carries it and clamps at 0); heuristics (apply_heu), the EFG default,
footprints, Inductive, the transition system, log skeleton, Declare,
POWL, variants, service time and overlap do not read the column.
Carried (GU3-2): `/api/dfg-dataframe` on a start column of no dates
answers `DFG table discovery: invalid start timestamp`, where pandas sorts
by the column for the frequency and raises the TypeError above for the
performance (an XES text column of date strings is object there, dates
in Minera); 40 of the test generator's specs.
GU's suite: the full ctest 684 of 684 (with the new
`minera_server_review_gu_tests`: 200 checks, 175 of which fail on GT's
server binary), Python script tests 406 of 406, the 16 manifests replayed
2,673 responses unchanged, and the frontend type check passes (types.ts
gains `performance.edges_error`).
Harness and probes: `review-harness/round-gu/`.

Round GV re-reviewed GU. GU's CI run 45335957548 passed on every job
(Linux, macOS, Windows, the frontend, the Python scripts and the parity
verifier), as did GT's Windows job in run 45324381722. Fresh banks (seeds
in brackets): gvT, 120 CSV uploads whose start_timestamp column holds
every kind (dates, gaps, ints, uint64 and past-uint64 ints, floats, text,
booleans, mixed, nothing) [72000-72119]; gvK, 200 CSV uploads keyed by
K1..K6 with gaps [72200-72399]; gvP, 200 CSV uploads with a valid start
column [72500-72699]; gvX, 200 uploads, XES and CSV alternating
[72800-72999]; gvN and gvM, gvP's CSV and gvX's XES files without the
start column; with GU's guX and GT's gtP. GV1 (GU3-2),
`/api/dfg-dataframe` on a start column of no dates: the frequency takes
the column's pandas order (each row ranked as a sort of the whole column
ranks it, the gaps last; `DfgFrameStartRanks`, `dfg_frame_start_ranks`),
the performance fails with pandas' `TypeError: cannot subtract
DatetimeArray from ndarray[float64|object]`, business hours at the `.dt`
accessor (`dfg_dataframe.cpp`): `ct/dfgdf.py` on guX 0 diffs (GU's binary
265 wrong), with business hours the same, gtP 600 of 600. GV2, the frame
DFG start order on the other routes: the stochastic map's frequency and
performance DFG (`ct/smap.py`, gtP 200 of 200; GU's binary 54 graphs and
16 maps wrong) and the classic DFG playout, which now raises pm4py's
`KeyError: '<activity>'` when a popped activity has no outgoing edge and
is no end activity (`dfg_playout.cpp`; `ct/playc.py`, gtP 200 of 200 with
12 KeyErrors, GU's binary 47 wrong). GV3, the lazy shape
(`mining_input_shape=lazyframe`, pm4py on the frame's Polars copy;
`frame_dfg_start_order(log, polars, columns)`): the whole copy sorted by
(case, start, timestamp) as Polars' unstable sort orders it, nulls first,
the start by the copy's dtype (a CSV frame's str and object columns
String, by bytes); a duration is max_horizontal(start, timestamp) minus
the timestamp in whole seconds (a null start 0); a start column of no
dates fails resolving the plan with the copy's dtype (`- not allowed on
i64|f64`, a uint64 column cast to i64 on both sides, `failed to determine
supertype of bool`, `got invalid or ambiguous dtypes: '[str, ...]'`; the
DF node from the frame's columns, an XES frame's in read_xes order). With
business hours (the DFG performance variant) a numeric start resolves and
the business-time UDF fails on the number (`'int'|'float' object has no
attribute 'replace'`, none without a pair), a uint64 start's strict cast
fails on the pairs' values past int64 (Polars lists up to ten: two, an
ellipsis and the last, then "Did not show all failed cases"), and the
UDF's Float64 Series refuses the first Python int duration in the sorted
rows (`polars_business_int`), also without a start column (each event
starting at its timestamp; XES frames read the events' timestamps).
After (`ct/pmapl.py`): gtP, gvP, gvX and guX all equal in frequency,
start and end counts, performance edges and variant (GU's binary on gtP
51, 16 and 29 wrong); gvT 119 of 120 (t72074, declared below); with
business hours gtP 200, gvP 200, gvX 200, gvT 119 and gvN 100 equal
(before the UDF rules 97 of 200 gvX, 39 of 120 gvT and 61 of 100 gvN
wrong). GV4, the CSV log shape (probed, no change): `/api/mine`'s default
shape is the log shape, and its process map and DFG performance variant
keep the DataFrame contract of the 2026-11-05 sweep and GU
(`ct/pmap2.py`); pm4py's EventLog from convert_to_event_log names
start_timestamp in its properties (read_xes's frame names none), and its
performance DFG then pairs each trace in timestamp order with max(0,
start - timestamp), a NaT start 0, and fails at the first pair whose start
is no date (`unsupported operand type(s) for -: '<type>' and
'Timestamp'`, a gap NaN as float; with business hours Timestamp.replace's
errors or `cannot convert float NaN to integer`); the routes reviewed
here (process map, DFG variant, stochastic map, playout) read the frame,
and `dfg_discovery.apply` without parameters ignores the start key. GT1 on fresh banks: the temporal
profile (`ct/tpdcsv2.py`, 9 sets) and its conformance (`ct/tpccsv2.py`,
15 sets) on gvT's 119 uploads: the log shape 1,071 of 1,071 and 1,785 of
1,785 equal; the pandas frame all but t72012, t72045 and t72109 (text with NaN
beside dates, carried); Polars all but the three uint64 uploads (carried
below). `ct/csvdfg.py` on gvK 9,800 of 9,800, `ct/csvvar.py` 1,400 of
1,400; `ct/csvdcl.py` the same counts as GU's binary (GT2's Declare
residuals, carried). Declared: an upload whose timestamps are all empty
(gvT t72074): pm4py's frame functions and convert_to_event_log refuse it
(`the dataframe should (at least) contain a column of type date`), Minera
mines it.
GV's suite: the full ctest 685 of 685 (with the new
`minera_server_review_gv_tests`: 119 checks, 111 of which fail on GU's
server binary), Python script tests 406 of 406, the 16 manifests replayed
2,673 responses unchanged, and the frontend is unchanged.
Harness and probes: `review-harness/round-gv/`.

Round GW re-reviewed GV. GV's CI run 45360152483 passed Linux, Windows,
the frontend, the Python scripts and the parity verifier; macOS failed one
check of the new `minera_server_review_gv_tests`: the classic DFG playout
of gtP s69503 counted `a;c` 401 times instead of glibc's 400. A variant's occurrences are
ceil(probability * max_variants) with the probability exp(-sum(-log p)), so
a product next to an integer follows the platform's libm in pm4py and Minera
alike (`test_dfg_performance_playout_parity.cpp` declares the same for the
probabilities); off glibc the GV test now accepts the same variants with
counts one away and prints how many (declared). GV3 on more banks, with the
GV banks plus gtT, gtP and guX: the lazy shape's `/api/discover-dfg`
(`ct/ddfg.py`, frequency, start and end counts and performance; gvP, gvX
and gvT, and with business hours gtP and guX), the stochastic map and the
classic playout on the lazy shape (`ct/smap.py`, `ct/playc.py`, gtP, guX,
gvT) are all equal but t72074; Polars' business-hours SchemaError names the
same int on 220 erroring uploads. t72074 (every timestamp empty) stays
declared: the frame shapes raise Minera's subtraction or plan errors where
pm4py's frame functions refuse the frame first (`the dataframe should (at
least) contain a column of type date`, the `.dt` accessor), part of the
open CSV non-date timestamp contract. GW1, `/api/mine`'s eventually-follows
graph on the default shape now reads the upload's start_timestamp column
as pm4py's frame discovery does (`frame_eventually_follows` in
`minera_server.cpp`): the pairs (i<j) of a case whose end is at or before
the later event's start, CSV rows in the frame's @@index order, XES rows
stably sorted by (case, start, timestamp) (`xes_pandas_tp` with a pair
list); a start column of no dates raises what the reference raises,
`'<=' not supported between instances of 'Timestamp' and 'bool'|'int'` on an
object column with a later start, `Invalid comparison between
dtype=datetime64[us|ns, UTC] and StringArray|ndarray` on text that does not
parse or other dtypes, else the flow time's `cannot subtract DatetimeArray
from ndarray[object]`, reported as `eventually_follows_error` with an empty
graph (the frontend shows it). The Polars lazy shape ignores the column (no
change), and an explicit start or another timestamp key keeps the log
graph. `ct/efg.py`: gvP and gtP 400 of 400 (GV's binary 27 of gvP's 200),
gvX and guX 199 graphs and 201 errors equal, gvT and gtT 46 graphs and 193
errors equal (t72074 declared), gvN and gvM 200 of 200, the lazy shape 400
of 400. GW2, the Polars temporal profile with a uint64 start column (11
gvT uploads, all wrong before): the dtype model gains u64; beside dates both
sides are cast to Int64 (values past int64 null), the supertype of u64 and
i64 is f64 and of u64 and bool u64, the business-time UDF fails with
`'int'`; discovery (`ct/tpdcsv2.py`) and conformance (`ct/tpccsv2.py`) 11 of
11 on every set. GW3 (GT3's residuals): the pandas temporal profile on a
text start column whose values all parse as dates raises the subtraction
TypeError, with business hours the UDF's `str.replace() takes no keyword
arguments` when a pair passes and the empty-flow error otherwise
(`xes_text_date_micros`); a True/False column with a gap in a row
format_dataframe dropped stays object (read_csv typed it with the row;
`DataFrameColumn::missing_cell`), so its temporal profile follows the object rules. gtT's 9 wrong uploads
now equal; the DFG variants keyed by those columns stay 120 of 120. Regressions: GV's sweep
(`ct/pmap2.py`, `ct/dfgdf.py`, `ct/smap.py`, `ct/playc.py`, `ct/pmapl.py`)
and the temporal profile on gvT's three shapes equal but t72074, the log
shape 1,071 of 1,071 and 1,785 of 1,785.
GW's suite: the full ctest 686 of 686 (with the new
`minera_server_review_gw_tests`: 64 checks, 56 of which fail on GV's server
binary), Python script tests 406 of 406, the 16 manifests replayed 2,673
responses unchanged, the frontend's tsc and the eventually-follows tests
clean.
Harness and probes: `review-harness/round-gw/`.

Round GX re-reviewed GW. GW's CI run 45376350277 passed Linux, macOS (GV's
playout check with its libm tolerance), Windows, the frontend, the Python
scripts and the parity verifier. GX1, `/api/mine`'s eventually-follows graph
with another timestamp key K (`eventually_follows_timestamp_attribute`) is
now `discover_eventually_follows_graph(frame, timestamp_key=K)` on CSV and
XES uploads (`frame_eventually_follows` in `minera_server.cpp`): the frame's
check of K first (`not contained in the dataframe. Available columns:` with
the sorted columns, `should be of time datetime` / Polars `of type datetime`,
`should not contain any empty value.`), then the pairs i<j of a case kept
where K of the earlier row is at most the later row's start (the frame's
start_timestamp column, else K; CSV rows in @@index order, read_xes's frame
sorted by case, start and K). The lazy shape follows Polars' copy: the whole
copy sorted by case and K with Polars' unstable sort (`polars_sort.hpp`),
every pair counted, and K = start_timestamp a DuplicateError plan error; the
Polars default (time:timestamp) is that graph too, so its ties now follow
Polars (93 of gxL's 200 lazy default graphs were wrong). The explicit
DataFrame, LazyFrame and stream shapes (`active_source_mining.hpp`) now use
the same frame graph and nest `eventually_follows_error` in their analysis
(the frontend reads it there). gxE (400 uploads; K in X, start_timestamp,
the CSV header, res, a missing key): default shape 273 graphs and 719
errors equal (before 158 and 37), the explicit DataFrame and stream shapes
273 and 717, the lazy shape 1,000 of 1,000 (217 graphs and every error
wrong before); the remaining runs are 2 uploads whose whole mine fails
(declared GU1-2). GX2, an explicit start key S
(`eventually_follows_start_attribute`) keeps FT1-1's log variant on the
frame's EventLog, now as Python runs it (`frame_interval_eventually_follows`):
each trace sorted by S with CPython's list.sort (KeyError for an event
without S), then event by event its completion (KeyError when missing, on
the last event too) compared by `<=` with each later start, values as the
frame's Python objects (Timestamp to the nanosecond, float/NaN, int, bool,
str, NaT; datetime.datetime in an XES object column), cross-family compares
raising TypeError; the lazy shape converts the Polars copy (datetime.datetime,
None, str of an object column). Before, 582 of the GW banks' 1,040 runs and
345 of gxE's 400 had Minera's own errors; now every start key (start_timestamp,
X, time:timestamp, @@index, concept:name, case:concept:name, org:resource, a
missing key) is equal on both shapes but GU1-2 and t72074. GX3, XES ints past
INT64_MAX (within uint64) on these routes are typed as pandas types them
(`xes_frame_cells(..., big_ints)`): uint64 alone, float64 beside floats or
gaps, object beside a negative int; gxB (200 uploads) was 140 of 200 keyed
runs wrong, now equal but GU1-2. Other routes keep their fallback on such
columns (carried). Regressions: GW's `ct/efg.py` on gvP/gtP, gvX/guX,
gvT/gtT, gvN/gvM and the lazy shape unchanged (t72074 declared).
GX's suite: the full ctest 686 of 686 (33 minutes, -j2) plus the new
`minera_server_review_gx_tests` (162 checks, 138 of which fail on GW's
server binary: another timestamp key on three shapes, explicit start keys
on the pandas and Polars logs, XES uint64 columns), Python script tests 406
of 406, the 16 manifests replayed 2,673 responses unchanged, the frontend's
tsc clean.
Harness and probes: `review-harness/round-gx/`.

Round GY re-reviewed GX. GX's CI run 45395730735 passed every job (Linux,
macOS, Windows, the frontend, the Python scripts and the parity verifier).
New checker `ct/u64.py`: `/api/mine` families keyed by an XES upload's
columns (activity, timestamp and start keys of Alpha, heuristics,
Heuristics++, the classic performance view, Inductive, the log skeleton,
the temporal profile with and without business hours, Declare) on the
DataFrame and LazyFrame shapes vs the algorithm applies on read_xes's
frame and its Polars copy, run on GX's gxB (ints past int64 in X and
start_timestamp) and gxE. GY1, a log skeleton keyed by another column
(`log_skeleton_activity_attribute`, even org:resource) failed the whole
mine on the explicit DataFrame and LazyFrame shapes with a
presentation_error (`log skeleton pair activity is not registered`,
`log skeleton activity is not registered: r1`): the skeleton's labels now
join the process map's activities as on the default shape since GR
(`active_source_mining.hpp`; a Polars frame's missing activity stays out).
gxB: 163 and 122 whole-mine failures (keys X and start_timestamp) on
pandas, 199 and 146 on Polars, now none. GY2, the routes that kept a
fallback on an XES column of ints past INT64_MAX now type it as pandas
does (`xes_frame_cells(..., big_ints)` on every timestamp and start
column: Heuristics++ and its interval checks, the classic performance
view, the temporal profile's pandas and Polars models, Alpha's and
Inductive's frame keys, Declare's Polars timestamp): uint64 alone (Polars
UInt64, `u64` in its plan errors), float64 beside floats or gaps, object
beside a negative int (Polars String). Two ints compare exactly by their
digits (`xes_cell_compare`, `xes_polars_cell_compare`) and an int no
double holds is its own Categorical value whatever the platform's long
double. Before, Heuristics++ answered `requires a valid start timestamp`,
Inductive `timestamp field 'X' is invalid`, and the temporal profile and
the classic view Minera's own errors. gxB, all 200 uploads: pandas 3,794
non-equal judgments of 7,000 down to 1,511, Polars 3,654 down to 903; every
timestamp and start key set is now equal on both shapes, the rest are
activity keys (carried below). Activity and case columns keep their
fallback on such ints (`classic_frame_cells(..., false)`). Checked without a
change: the eventually-follows filter (pandas and Polars LTL) reads no start
column (`x/p2.py`); the only ceil or floor of a libm product remains the
DFG playout (GW's tolerance), CPython's random.sample set size ceil(log(3k)
/ log(4)) cannot land on an integer and the stochastic Petri slices divide
only; GU1-2 stays declared (pm4py's discover_dfg refuses the frame, the
process map is the mine's core result); GT2 and GT4's residuals stay
declared (hash order). Regressions: GX's `ct/efg2.py` on gxB (keys none,
X, start_timestamp) equal to GX's binary (108 graphs and 452 errors equal,
40 GU1-2 runs).
GY's suite: the full ctest 687 of 687 (34 minutes, -j2) and the new
`minera_server_review_gy_tests` on its own (186 checks, 158 of which fail
on GX's server binary), Python script tests 406 of 406, the 16 manifests replayed
2,673 responses unchanged, no frontend change.
Harness and probes: `review-harness/round-gy/`.

Round GZ re-reviewed GY. GY's CI run 45415196193 passed every job (Linux,
macOS, Windows, the frontend, the Python scripts and the parity verifier).
GZ took GY's open activity keys (`ct/u64.py` on gxE and gxB, both frame
shapes): the families keyed by a typed XES column now read it as read_xes
types it and key their models by the Python objects (new
`xes_frame_activity.hpp`: each cell's object, its str() label and its
Python-equality identity, ints past int64 included). GZ1 Alpha (pandas):
the consecutive pairs of a case are grouped by the objects, the targets
after shift() (an int64 or uint64 column's ints become floats), labels str()
of the first equal object, a case without a named activity starts and ends
with its gap (float64: a NaN per case; dates NaT; object None), the groupby
sort's TypeError on mixed objects (an object column's dates named
datetime.datetime); without a start key pandas adds the frame's missing
start_timestamp column as a copy of the timestamp column, which the activity
key start_timestamp reads; Polars' object copy is str() of each value, and
an activity key equal to the case, timestamp or start key is Polars'
duplicate-column error. GZ2 Inductive: the distinct objects are sorted
(TypeError; Polars None beside the typed value), str() of a tree with
non-str leaves fails as pm4py's fold does (`__str__ returned non-string`
for a root leaf, `can only concatenate str (not "T") to str` otherwise),
pandas gaps reach the miner as objects of their own, Polars object columns
show str() labels. GZ3 log skeleton: typed labels, pandas' `KeyError:
NaT|None|nan`; the session's model is mined on the texts so its
registration and replay keep the log's labels (the mine had failed with
`log skeleton activity is not registered: 0`). GZ4 temporal profile: typed
labels, equal objects one activity (pandas Python equality, ints past 2**53
in a float64 column one float; Polars equal values, an object column's
str()), pandas' groupby safe_sort TypeError after the frame's other errors.
GZ5 classic heuristics (and Heuristics++, transition systems) read activity
columns of ints past int64 typed instead of the text fallback (the
subscript TypeError, float labels), ties compared exactly. GZ6 Declare: a
typed column's objects are sorted (TypeError) and the gap rule carries its
`TypeError:` prefix. The session nets, trees and models keep the XES texts
the log replays; only the family JSON shows the typed labels. gxB, all 200
uploads: pandas 1,511 non-equal judgments of 7,000 down to 369, Polars 903
down to 74. Left on pandas: Declare's
typed values (240 on gxB pandas), Heuristics++ activity keys (69),
Inductive's sort operands (56) and RecursionErrors (2), two temporal
profile operand orders. Found while probing: pm4py's Inductive sorts
`set(activities)`, so a TypeError's operands follow CPython's set iteration
(hash(None) is the object's address in 3.11, str and datetime hashes follow
the hash seed): declared, Minera sorts in row order (same error class).
GZ's suite: the full ctest 688 of 689 (39 minutes, -j2; the Declare unit
test pinned the gap's message without its new `TypeError:` prefix, updated
and passing on its own) and the new
`minera_server_review_gz_tests` (194 checks, 147 of which fail on GY's
server binary), Python script tests 406 of 406, the 16 manifests replayed
2,673 responses unchanged, no frontend change.
Harness and probes: `review-harness/round-gz/`.

Round HA re-reviewed GZ. GZ's CI run 45445812558 passed every job (Linux,
macOS, Windows, the frontend, the Python scripts and the parity verifier).
HA took GZ's open activity keys (`ct/u64.py` dc_aX, dc_ast, hpp_aX,
hpp_ast on gxB and gxE, both frame shapes). HA1 Declare (pandas): the
family result is the model pm4py's classic variant answers through its
rules frame (`typed_declare_model`, first written as a Python model that
matched pm4py on every value of both banks): each case its slice of the
frame's rows, the column's objects as events (a float64 column's NaN a
new object per row, so an activity each; an object or str column's gaps
the shared NaN; NaT and None one activity each), self pairs where `!=`
holds (NaN, NaT), the rules frame's MultiIndex levels factorized by
Python equality with their inferred dtype (an int level with gaps reads
back floats, a collision raises `Categorical categories must be unique`),
lookups that raise `KeyError` with the full tuple or, on a non-unique
index, answer the row count, the reversed (product, key) sort of the
automatic thresholds, and the output's collapse of a rule whose second
activity is missing or falsy. The session keeps the text model the log
replays (empty where the text model fails, so the presentation stays
valid). HA2 Heuristics++: typed nodes as the classic reads them (float
labels of ints past int64, equal floats merged, each float64 NaN a node),
to_interval pairing by node, the concurrency and dependency comparisons of
the objects (TypeError on mixed objects), and pm4py's
`tuple(sorted(pair))` concurrency keys, which keep a NaN pair in the given
order (`concurrency_key`); the Petri conversion type of a non-str node.
HA3: start_timestamp as the Heuristics++ activity key is the interval's
start too (Minera had dropped that column from the events). HA4: Polars
Declare keyed by an object column shows str() of each pandas object, as
the Polars copy holds them (`True`, a date's str()), not the XES texts
(the sweep's date normalization had hidden it; `tc/mkha.py` compares
exactly). gxB: Declare
pandas 237 of 400 judgments differing down to 123, every one an error of
the same class whose operands or KeyError template follow the hash seed
(declared), Polars 16 down to 0; Heuristics++ pandas 69 down to 0, Polars
42 down to 0; gxE Declare pandas 56 down to 11 operand orders, Polars
5 down to 0, Heuristics++ both shapes 51 down to 0; the
other Heuristics++ options and the classic heuristics unchanged.
HA's suite: the full ctest 690 of 690 (39 minutes, -j2) with the new
`minera_server_review_ha_tests` (168 checks, 114 of which fail on GZ's
server binary), Python script tests 406 of 406, the 16 manifests replayed
2,673 responses unchanged, no frontend change.
Harness and probes: `review-harness/round-ha/`.

Round HB re-reviewed HA. HA's CI run 45471275567 passed every job (Linux,
macOS, Windows, the frontend, the Python scripts and the parity verifier).
HB took HA's open Declare items. HB1 CSV-keyed Declare
(GT2-2, GT2-3): a CSV upload's pandas frame and its log shape feed
format_dataframe's typed cells (read_csv's numbers, booleans, dates and
objects, its gaps one NaN of an object column or a NaN per row of a
float64 column) to HA's rules-frame model through
`discover_declare_typed_frame`, so the model, its KeyErrors and its sort
TypeErrors are pm4py's; on the log shape (the legacy response) a
conformance error fails the whole mine, as pm4py's apply does. The
Polars frame checks its copy's column first: a column of numbers,
booleans or dates is no String (`the activity column should be of type
string.`), read_csv's gaps are nulls (`... should not contain any empty
value.`), and an object column (True/False beside a gap included) holds
str() of each object (`csv_polars_declare_view`). HB2 Declare conformance
keyed by a typed column: `typed_declare_conformance` checks pm4py's
families in their order against the typed model's keys, `in` and dict
lookups by identity or equality, `!=` pure (NaN and NaT never equal), a
collapsed one-element key subscripted (`'<type>' object is not
subscriptable`) or unpacked (`cannot unpack non-iterable <type> object`),
a str key indexing its characters; deviations named by the typed labels.
It answers the declare_conformance family on XES pandas frames and on the
CSV shapes (the log shape's list in the log's case order); on an XES
Polars frame keyed by an object column the conformance runs on the same
str() relabelled projection as the shown model. The session keeps the
text model and its own conformance. HB3 Heuristics++ and-measures of
typed nodes: not observable. A node's and-measures name its connected
neighbours, and pm4py's Petri conversion fails (`"pre_" + act`) on any
non-str node with an incoming connection, while a str node's neighbours
are str (mixed pairs already raise in the dependency pass); the
discover-workflow route serializes the net only after the conversion. Of
300 gxB/gxE pandas mines keyed by X or start_timestamp, one net carried
and-measures over typed nodes, and its conversion failed. HB4 exact
labels: `ct/u64x.py` without date normalization on gxB and gxE, both frame
shapes, shows no label difference; what is left is declared: sort
TypeError operands (Inductive, Declare, temporal profile), Declare's
KeyError template, the log skeleton's bool equal to an int (3 on gxE
pandas), and two Inductive RecursionErrors on pandas: pm4py's UVCL DFG
reads start and end activities with `==`, so a NaN is never one; a sublog
of a NaN and another activity then has no cut and the flower model
recurses on itself, where Minera fails at str() of the tree. Found while
probing: a collapsed-key order on the CSV log shape (k68020 K5,
`'bool' object is not subscriptable`) does not follow the hash seed but
the set order of tuples holding a NaN, whose hash is its address
(declared, like HA's operand orders). gtK, 40 uploads, keys K1-K6: log
shape 79 of 308 judgments differing down to 36, pandas frame 336 of 425
down to 30, every one a TypeError whose operands follow the hash seed or
the address order above; Polars frame 180 of 260 down to 0. Declare
conformance on XES: gxB pandas 98 of 287 down to 61 and gxE pandas 66 of
323 down to 11, every one a discovery error HA declared (operand orders,
KeyError templates), gxB Polars 3 down to 0, gxE Polars 0. The fixture
replay caught a CSV frame whose activity column is text without gaps
failing the typed path's frame check (no date column, a header-only
upload) where Minera's own path answers: the typed path now steps aside
before any check for such a column. Two GT cases
(`dcl K6 k68001.csv`, `k68006.csv`) pinned an operand order pm4py varies
by hash seed (checked over seeds 0-5); they compare the error class now.
Found for the next round: a CSV upload's frame shapes keyed by a typed or
gapped column for Alpha, Inductive, the log skeleton, the temporal
profile and Heuristics++ (`ct/csvk.py` on gtK, 30 uploads: pandas 735 of
1,080 judgments differ, Polars 732; the classic heuristics already
match): Minera reads the column's texts with gaps as empty labels where
pm4py reads format_dataframe's objects.
HB's suite: the full ctest 691 of 691 (38 minutes, -j2) with the new
`minera_server_review_hb_tests` (230 checks, 170 of which fail on HA's
server binary), the 16 manifests replayed 2,673 responses unchanged, no
Python script or frontend change.
Harness and probes: `review-harness/round-hb/`.

Round HC re-reviewed HB. HB's CI run 45500826116 passed every job (Linux,
macOS, Windows, the frontend, the Python scripts and the parity verifier).
HC1 a CSV upload's frame shapes keyed by a typed or gapped column (numbers,
booleans, dates or objects as read_csv types them, or a column with gaps)
for Alpha, the Inductive miner, the log skeleton (and its conformance),
the temporal profile and Heuristics++: the families read
format_dataframe's typed cells through the XES frame paths
(`session_typed_frame_table`, `csv_typed_frame_view`): a gap is a cell the
row lacks, as read_xes's frame holds an event without the key, a column
with no value keeps its NaN cells (read_csv keeps the column), and the
Polars copy holds a gap as null and an object column (Python ints, or
True/False beside a gap, even one in a row format_dataframe drops) as
str() of each object. A start column of other values than dates, or one
with a NaT, sends Alpha and Heuristics++ to the typed frame too (their
sort compares its objects). The family JSON shows the typed result; the
session keeps the text model for replay (the typed one where the texts
fail), as HB did for Declare. Pandas' Alpha keeps one NaN activity per
case boundary on an all-NaN float64 column. HC2 the typed frame reads
pm4py's defaults: an unsupplied case or timestamp key is the copy
case:concept:name or time:timestamp (a CSV whose headers differ sorts and
names its plans by the copies), and the frame keeps read_csv's column
order, then the copies and the index columns, which Polars' plans list
(`kFrameColumnOrderProperty` keeps `xes_frame_present_columns` from
reordering by first appearance). HC3 the classic heuristics miner on a
Polars frame keyed by a True/False column whose only gap sits in a row
format_dataframe drops: read_csv's object column, so its Polars copy is a
String column (Minera raised `'bool' object is not subscriptable`), and
the Heuristics++ frame check reads such a column the same way. HC4 the
Declare playout of a typed model: pm4py's playout reads every key as a
tuple (a str key as its 1-tuple), so the first unary or collapsed key
that is no str raises `TypeError: '<type>' object is not subscriptable`
and a collapsed str key `IndexError: tuple index out of range`; the
session records that error from the typed model on the CSV log shape and
pandas frame (`TypedDeclareResult::playout_error`). The family order
pm4py's model dict holds puts the base binary families first in an order
the rule columns' hash and address order decide, so a model of mixed key
types (True/False with NaN, a date column with NaT) may name the other
type (declared; 2 of 240 hcS DataFrame playouts). Harness note: `run.sh`
ran the reference with `python -I`, which ignores PYTHONHASHSEED, so the
sweeps of earlier rounds ran pm4py under random hash seeds; HC's runs use
`python -s -P` with PYTHONHASHSEED=0. Sweeps (`ct/csvk.py`, `ct/csvo.py`,
`ct/dplay.py`): gtK (30 uploads) pandas 735 of 1,080 judgments differing
down to 25 and Polars 732 down to 126, every one an Inductive message
class declared below; hcS (40 uploads, keys K1-K6 over every value kind
with gaps) and hcR (the same uploads under the headers cid, act, ts):
Alpha, the log skeleton, the temporal profile, Heuristics++ and the
classic heuristics match on both shapes (1,200 judgments per shape and
bank), and with start keys (start_timestamp, the date column K3) and
business hours for Alpha, Heuristics++ and the temporal profile (1,920 per
shape and bank) as well; the Declare playout on hcS DataFrames 228 of 240
down to 2 (both declared above), and on the log shape every playout of a
mine that succeeds matches. Declared: the Inductive miner keyed by a
typed or gapped column on the frame shapes keeps HA's classes (the sort
TypeError's operands follow the set order of the activities, which Polars'
random hashing and NaN's address decide; RecursionError where pm4py's
UVCL DFG never takes a NaN as start or end; the first failing leaf of
str(tree)); an activity column without any value, where pm4py's Polars
tree is made of None leaves (tau) and pandas fails at str() of a NaN
leaf, Minera refuses the null activity (4 of 240 Polars, 2 pandas).
Found for the next round: Declare on the pandas frame keyed by a
True/False column with gaps (hcS s68519, s68524, s68537 K5): pm4py's
collapsed key True holds (True, nan)'s support and confidence where
Minera keeps (True, False)'s, stable over allocations at PYTHONHASHSEED=0
(s68539 varies with addresses), so the rules' write order of collapsed
keys is not the column order HB modelled.
HC's suite: the full ctest 692 of 692 (-j2; 584 before a container restart, the other 108 after it) with the new
`minera_server_review_hc_tests` (363 checks, 261 of which fail on HB's
server binary), the 16 manifests replayed 2,673 responses unchanged, no
Python script or frontend change.
Harness and probes: `review-harness/round-hc/`.

Round HD re-reviewed HC. HC's CI run 45522928201 passed Linux, macOS, the
frontend, the Python scripts and the parity verifier (Windows' ctest was
still running at HD's handoff). HD1 Declare's collapsed keys: pm4py
writes the derived binary families (succession, altsuccession,
chainsuccession, coexistence, noncoexistence, nonsuccession,
nonchainsuccession) act by act and partner by partner over a Python set
of the activities, filled trace by trace, so a key collapsed onto its act
(a False, 0, 0.0 or '' partner, or a gap) keeps the stats of the partner
the set lists last (observed black-box with objects of chosen hashes).
`typed_set_ranks` (declare.cpp) replays CPython 3.11's set: False, 0,
0.0 and '' hash to 0, numbers by the numeric hash, str by SipHash under
PYTHONHASHSEED=0, NaT to -2**63, a Timestamp as pandas hashes it (the
int of its value when it has nanoseconds, else the tuple hash of the
timedelta (days, seconds, microseconds) of its UTC instant), None by its
address in the pinned interpreter (0xad8), NaN as 2**60 - 1 (the last
slot, where the reference's np.nan lands after False and True in these
small tables). The base families keep the column order: they come from a
set of (template, act, act2) tuples whose hash takes NaN's whole
address. hcS K5 (True/False with gaps) on the DataFrame shape: model
diffs 6 to 1, conformance messages 17 to 9, the log shape's mine errors
15 to 5; every residual is a base family's NaN-addressed order (s68543's
collapsed True varies run to run in pm4py itself). HD2 the earlier
rounds' declared operand orders rechecked at a real PYTHONHASHSEED=0: the
sort TypeErrors' operands and the base families' collapsed keys follow
np.nan's address, which moves with the imports and allocations of the
process even at seed 0 (two harnesses disagree on s68504 K1), so they
stay declared. HD3 Heuristics++ keyed by a typed or gapped column of a
CSV upload's log shape: pm4py's EventLog variant mines the net the frame
variant mines on format_dataframe's frame (convert_to_event_log keeps
the frame's objects, NaN and NaT included), so `handle_mine`'s legacy
path mines it from the typed frame (`typed_csv_frame_table`, split out of
`session_typed_frame_table`) and runs the frame's Heuristics++ checks
(`check_plus_plus_csv_frame(..., log_shape)`); the Petri conversion's
TypeError visits each node's successors in the order their pair first
follows it (pm4py's output connections, `finish_typed_plus_net`). Alpha,
the log skeleton, the temporal profile and the transition system on the
log shape already matched (`ct/csvfam.py`, hdN = hcS without its
start_timestamp column, and hcH); Heuristics++ 155 of 1,080 judgments
matching to 1,080 of 1,080 (hdN + hcH; h68034 K5 was the conversion's
successor order), and 40 hcS uploads with their start column 240 of
240. Declared: the Inductive miner keyed so keeps HC's classes; Alpha's
token replay on the log shape names a transition set's address order in
its error.
HD's suite: the full ctest 693 of 693 (-j2, 25 minutes) with the new
`minera_server_review_hd_tests` (70 checks, 53 of which fail on HC's
server binary), the 16 manifests replayed 2,673 responses unchanged, no
Python script or frontend change.
Harness and probes: `review-harness/round-hd/`.

Round HE re-reviewed HD. HD's CI run 45536343985 passed every job (Linux,
macOS, Windows, the frontend, the Python scripts and the parity verifier).
HE1 Declare keyed by a typed attribute on
an XES upload's log shape (read_xes' legacy log): the importer stores an
int as a Python int of any width, a float as float() (each NaN its own
object), a boolean as `str(v).lower() == "true"`, a date as fromisoformat
with fix_naivety's `replace(tzinfo=utc)` (the wall clock kept, the offset
dropped), a valueless value as None and a list with children as a dict,
and project_on_event_attribute raises `KeyError` on an event without the
key. `discover_declare_typed_log` (declare.cpp) reads those objects
(`legacy_declare_cell`) and feeds HA's rules-frame model
(`typed_declare_result`, split out of the frame path); `handle_mine`'s
legacy path takes it for XES inputs, so the conformance's error fails the
mine and the playout reads the typed model as on the frame. gxB `xl`
(keys X and start_timestamp): 148 judgments where pm4py raised and Minera
mined, 72 model diffs and 22 conformance differences to none; the 40 left
are GU1-2's declared whole-mine `'values' is not ordered` (the process
map). HE2 the activity set pm4py's Declare discovery iterates is a copy
of the projected set (`set(set(...))`, observed black-box with a set
subclass): CPython's copy resizes the empty table to the smallest power
of two above twice the entries once five times them reach 21, so from
five activities on its slots, and with them the step-1 columns, absence,
the derived families and the auto threshold sort's operands, can differ
from the original's (`CPythonSetOrder::copy`). With HD's tuple hashes of
(template, act, act2) for the base families' set (NaT as -2**63, a
Timestamp by pandas' hash), the base columns are modelled wherever no NaN
takes part: hcS K3 (dates with NaT) 4 of 120 conformance messages and 4
of 120 log-shape mine errors to none; gxB `xd` sort TypeErrors 118 to 13,
each with a NaN in the column (declared, np.nan's address). HE3 the
Inductive miner keyed by a column of gaps only (read_csv's float64 NaN):
pandas and the log shape raise `TypeError: __str__ returned non-string
(type float)` for one event, `maximum recursion depth exceeded in
comparison` when every case holds one event, else `TypeError: can only
concatenate str (not "float") to str`; Polars mines a tau tree when every
case holds one event, else `ValueError: None cannot be a node`
(`frame_keyed_inductive_error`, `all_gap_activity_tree`); 51 of 51 on 50
synthetic CSVs and hcS/hcH uploads over the three shapes.
HE's suite: the full ctest 694 of 694 (-j2, 32 minutes) with the new
`minera_server_review_he_tests` (156 checks, 126 of which fail on HD's
server binary), the 16 manifests replayed 2,673 responses unchanged, no
Python script or frontend change.
Harness and probes: `review-harness/round-he/`.

Round HF re-reviewed HE. HE's CI run 45547956280 passed every job (Linux,
macOS, Windows, the frontend, the Python scripts and the parity verifier).
HF1 the other families keyed by a typed attribute on an XES upload's log
shape: `legacy_keyed_activities` (frame_keys.cpp) reads read_xes' legacy
objects (`legacy_declare_cell`, moved to xes_legacy_cell.hpp). Values equal
as dict keys (1, 1.0 and True) are one activity, named str() of the first
one met in log order; values printed alike but unequal ('1' beside 1, each
NaN float) take trailing `\x1f`; an event without the key and a dict (a
list with children) are flagged. `handle_mine` feeds it to the transition
system, the temporal profile (each pair (i, j) of a trace reads both
events' keys: KeyError, then unhashable dict; a trace of one event reads
nothing), the log skeleton (every key read first: KeyError, then
unhashable dict) and Alpha (an event without the key in a trace of one
event is left out, in a longer trace the KeyError stands; the token replay
sorts the net's transitions by their legacy objects, so values that do not
order raise TypeError and the left-out event a KeyError after it:
`check_legacy_alpha_transition_names`, `legacy_log_keyed_replay_log`).
HF2 Heuristics++ on that log shape (`legacy_keyed_plus_plus`): the typed
labels and comparisons (`plus_label_comparisons`, split out of the frame
path), the converted net named by the typed objects, its replay as Alpha's,
and pm4py's read order with a start key: in interval mode the log-order
KeyError or unhashable dict; before the interval a dict first, then per
trace the start reads (KeyError), the sort and the activity reads
interleaved with the comparisons (`check_legacy_plus_intervals` over
`check_plus_interval_cells`' new read callbacks), and a typed start
column's subtraction TypeError. HF3 the Inductive miner on that log shape
(`legacy_inductive_outcome`): every key is read first (KeyError with the
key's repr, also when no event holds it), then the activity set is hashed
(unhashable dict) and sorted: its first failing comparison names its
operands in CPython's set order of the objects under PYTHONHASHSEED=0
(`python_object_set.hpp`: str, number, None and datetime hashes, None by
its address); a log of None alone mines a tau when every trace holds one
event, else networkx's DFG fails; otherwise the tree's str() fails on the
first leaf that is no str. Minera mines over ranks of Python's order of the
values (numbers by value, dates by instant) when no NaN is among them,
which reaches pm4py's first leaf more often than their printed text. HF4
Declare: a binary rule ending in an empty str (falsy, as a typed 0 or
False) is keyed by its first activity alone on all three shapes, and the
export validator accepts it.
Sweeps (`xesfam.py`) over hfA (100 uploads), hfG (60, keys missing) and
hfN (60, valueless values) with keys K1..K6 and gxB with X and
start_timestamp, 1,720 judgments per family, same as pm4py on HE's server
vs HF's: Heuristics++ 455 -> 1,720, Alpha 482 -> 1,556, Inductive
877 -> 1,689, the log skeleton 983 -> 1,666, the temporal profile
910 -> 1,652, the transition system 998 -> 1,651; Declare (`dcx.py`, hfE
on the three shapes) every judgment (375 on the log, 470 on the DataFrame,
290 on the LazyFrame). Declared: Alpha's and Heuristics++'s
replay sort operands (the transitions' set in address order, 159), the
log skeleton's, temporal profile's and transition system's entries named
by whichever of two equal values printed differently (1 and 1.0, -0.0 and
0.0) each dict keeps (hfA/hfG/hfN K2 and K5), Inductive's leaf order of
pm4py's raw tree before folding (31, K2 and K5), the transition system's
states over dict values (named by their repr), and GU1-2's whole-mine
`'values' is not ordered` on gxB's typed start columns.
HF's suite: the full ctest 695 of 695 (-j2, 39 minutes) with the new
`minera_server_review_hf_tests` (415 checks, 386 of which fail on HE's
server binary), the 16 manifests replayed 2,673 responses unchanged, no
Python script or frontend change.
Harness and probes: `review-harness/round-hf/`.

Round HG re-reviewed HF. HF's CI run 45568534096 passed every job (Linux,
macOS, Windows, the frontend, the Python scripts and the parity verifier).
HG1 the session routes keyed by a typed attribute or column on an upload's
log shape read what /api/mine's families read (round HF's legacy objects
on an XES upload, the frame's cells on a CSV upload): the log skeleton of
/api/discover-constraints (every key read first: KeyError, then
unhashable dict; its conformance keyed by another key meets the model's
objects by Python's ==, `legacy_keyed_activities_in`), the temporal
profile and its conformance of /api/analyze-behavior (`legacy_temporal_conformance_log`,
`csv_temporal_conformance_log`: a profile from JSON names str, which no
typed value equals) and the transition system of /api/discover-graph
(`transition_keyed_log`). pm4py's log skeleton conformance pairs a trace's
activities a != b, so a NaN or NaT meets its own equivalence
(`keep_missing_self_equivalence`, on /api/mine too).
HG2 /api/evaluate on the Alpha and Heuristics++ nets mined under such a key
replays the log as the net meets it (`legacy_replay_keyed`,
`csv_log_keyed_replay_log`, `csv_log_keyed_replay_in`); pm4py's
ETConformance precision (both token variants, and the composite's) joins
each trace's prefix (every event but the last) with ",", so the first
trace in log order whose prefix holds an object other than a str raises
`TypeError: sequence item k: expected str instance, T found`
(`legacy_precision_error`). /api/mine's token-replay precision reports it
as `precision_error` beside a null precision, and the conformance view
names it in the precision gauge's hint.
HG3 an explicitly supplied token-replay key replays its own objects (XES)
or cells (CSV) against each net: the Alpha and Heuristics++ nets'
transitions sort by their objects first, whatever the replay key
(`check_legacy_alpha_transition_names`, `check_csv_keyed_alpha_transition_names`,
`check_csv_keyed_heuristic_transitions`), the variants read every event's
key (KeyError, unhashable dict) and sort, and each value fires the
transition named by an equal object (1 == 1.0 == True; '1' never fires 1,
nor one NaN another): `legacy_keyed_activities_in` on XES,
`csv_log_keyed_replay_in` on CSV for the Alpha, heuristics, Inductive and
POWL nets. The Alpha miner's own KeyError stands when a trace of two or
more events lacks its key. A transition pm4py labels None (an Alpha net
keyed by an attribute with a valueless value) is silent, and an event whose
key is None fires it as its own (`none_silent_replay` over
`check_conformance_null_silent`).
Sweeps (`ct/routes.py`) over hfA, hfG and hfN (XES, keys K1..K6) and hcS
(CSV, 120 uploads), same as pm4py on HF's server vs HG's: XES the log
skeleton 448 -> 1,078 and its conformance 773 -> 1,092, the temporal
profile 494 -> 1,285 and its conformance 490 -> 1,320, the transition
system 682 -> 1,282, evaluate on Alpha (replay fitness 145 -> 857,
precision 176 -> 857, generalization 243 -> 857) and Heuristics++
(145 -> 430, 176 -> 430, 199 -> 430), /api/mine's token-replay precision
(Alpha 201 -> 857, Heuristics++ 201 -> 430), the Alpha net replayed with
the key supplied 145 -> 857 and with concept:name 150 -> 869; CSV the log
skeleton 131 -> 720 and its conformance 160 -> 720, the temporal profile
167 -> 720 and its conformance 141 -> 720, the transition system
131 -> 720, evaluate on Alpha (131 -> 542, 76 -> 542, 138 -> 542), the
token-replay precision 92 -> 542, the Alpha net replayed with the key
supplied 131 -> 542 (the mine failed on 560 judgments before, 55 now, each
a declared operand order) and with concept:name 0 -> 542. Declared: the transitions' sort operands (the set in address
order, as HF: 98 + 97 + 108 XES and 8 + 6 + 55 CSV whole-mine messages),
and equal values printed differently (1, 1.0 and True; 0.0 and -0.0) named
by the object each container entry met first in the log skeleton, the
temporal profile and the transition system (83 judgments, where Minera
names the activity once), where pm4py's transition system also keeps two
such states apart (4).
HG's suite: the full ctest 696 of 696 (-j2, 26 minutes) with the new
`minera_server_review_hg_tests` (711 checks, 391 of which fail on HF's
server binary), the 16 manifests replayed 2,673 responses unchanged, the frontend
typecheck and its 240 test files (one new precision-gauge case), no Python
script change.
Harness and probes: `review-harness/round-hg/`.

Round HH re-reviewed HG. HG's CI run 45585099162 passed the Linux native
build and ctest, the frontend, the Python scripts and the parity verifier
(macOS and Windows were still in ctest at HH's handoff).
HH1 /api/evaluate's alignment-based replay fitness (explicit, or picked for
an easy-sound net) and its AUTOMATON_AFTER_ALIGN precision on the Alpha and
Heuristics++ nets mined under a typed key align the keyed log, whose reads
fail otherwise than the token replay's: after the net's easy-soundness
check pm4py's alignments read every event's key (an event without it a
KeyError, a dict value unhashable, in log order) and then concatenate the
first object that is no str (`TypeError: can only concatenate str (not
"T") to str`, `keyed_alignment_error`). The token replay's checks
(`check_legacy_replay_keyed`, the variant sorts of `csv_log_keyed_replay_log`
and `csv_log_keyed_replay_in`, now optional) run only where the evaluation
fell back to the token replay.
HH2 the minimum self-distances of the EventLog variant keyed by such a key
(/api/mine and /api/analyze-behavior) read the legacy objects or the CSV
cells (`keyed_minimum_self_distances`): an unhashable dict anywhere first,
then a KeyError for an event without the key; values equal as dict keys are
one activity, and a NaN, unequal to itself, never repeats.
HH3 the footprints of /api/discover-graph keyed by such a key (entire log,
trace by trace, the model's footprints and their conformance) read the same
objects (`check_legacy_footprints_keys`: an event without the key in a trace
of two or more events raises KeyError, then a dict unhashable, then any
other absent key).
HH4 pm4py's log skeleton conformance sorts each trace's deviation list with
Python's list.sort: the activity-frequency entries compare their activities
(the trace's in first-occurrence order, then the model's absent ones in the
order its discovery met them), so text beside a number or a date, or None
beside anything, raises the first comparison's TypeError
(`check_log_skeleton_conformance_typed`, log_skeleton_typed.hpp; numbers,
bools and NaN compare with each other, Timestamps and NaT with each other).
The session route's conformance keyed otherwise than its discovery meets
the model's cells by Python's == on CSV uploads too (`csv_log_keyed_replay_in`;
a gap is a cell, no longer a missing activity).
HH5 /api/align with an alignment key of such values (state-equation and
Dijkstra variants) raises the same errors once the net's final marking is
reachable; pm4py's less-memory variant matches the objects themselves and
is not modelled.
Checked without change: evaluate's replay of an Alpha net's None label
(silent in pm4py; 30 all-None probes equal on replay fitness,
generalization, ETConformance precision and the composite).
Sweeps over hfA, hfG and hfN (XES, keys K1..K6) and hcS (CSV, 120
uploads), same as pm4py (equal or failing alike) on HG's server vs HH's:
evaluate's explicit alignment fitness XES Alpha 134 -> 807 and
Heuristics++ 142 -> 427, CSV Alpha 49 -> 485 and Heuristics++ 6 -> 20; the
automatic fitness XES 184 -> 857 and 145 -> 430, CSV 106 -> 542 and 9 -> 23;
AUTOMATON_AFTER_ALIGN XES 92 -> 762 and 93 -> 375, CSV 47 -> 477 and
5 -> 19; the log skeleton's conformance keyed by concept:name CSV
417 -> 720 and hfA 333 -> 600, keyed by the next key CSV 53 -> 661 and
hfA 290 -> 514 (the rest the declared naming class below), keyed like its
discovery 720 and 600 unchanged; the minimum self-distance and footprints
sweeps (`ct/fam.py`) found no difference beyond the declared classes on
the uploads they reached before pm4py's model footprints stalled (its
reachability walk of some Alpha nets runs for minutes), and the new
test's cases (15-second limit per reference call) cover both families;
/api/align with the key supplied (state-equation and Dijkstra variants,
standard costs) on 10 hfA and 10 hcS uploads: every typed key fails as
pm4py does, the rest equal in costs and fitness. Declared: the messages for
a net that is not easy sound (Minera's alignments name the unreachable
final marking, 50 + 57 + 3 evaluate judgments and the /api/align ones),
the AUTOMATON_AFTER_ALIGN values that rest on tie choices (45 + 52 XES and
8 + 1 CSV, cases that failed before HH among them), the mine's
transitions-sort operands (as HF and HG), and equal values printed
differently named by the object each container met first (the log
skeleton's deviations: 86 hfA and 59 CSV judgments, the footprints' 1.0
beside 1).
HH's suite: the full ctest 697 of 697 (-j2, 55 minutes under a parallel
sweep) before the new test's cases, then the new
`minera_server_review_hh_tests` (387 checks, 248 of which fail on HG's
server binary, 11 seconds); the 16 manifests
replayed 2,673 responses unchanged; no frontend or Python script change.
Harness and probes: `review-harness/round-hh/`.

Round HI re-reviewed HH. HH's CI run 45612060209 passed all six jobs (the
Linux, macOS and Windows native builds and ctest, the frontend, the Python
scripts and the parity verifier).
HI1 /api/mine's correlation miner keyed otherwise than concept:name reads
the objects pm4py's variants read (`correlation_key_objects`, the
`key_objects` option, correlation_key_objects.hpp): on an XES upload's log
shape read_xes' legacy objects (one per event in log order: an event without
the key, a dict, each NaN its own float), on a CSV upload the formatted
frame's cells (a gap NaN, one object per cell in a float64 column and one
shared object in a text or object column, NaT in a date column; read from
`dataframe_frame_event_log`, as `frame_keyed_activities` does). The classic
variant reads every event's key first (KeyError), then sorts the set of the
objects in time order (unhashable dict; CPython's set order by
`python_hash_set_order`, NaT hashing as the int64 minimum) with Python's
list.sort (TypeError for text beside a number or a date, None beside
anything); a NaN or NaT activity holds no event, its events drop out.
classic_split on the legacy log walks the events in log order (KeyError or
unhashable dict at the first one) and sorts the objects' first appearances;
every NaN is its own activity with its own event, but each chunk groups
events by ==, so a NaN activity precedes and follows nothing (zero
probability and duration). On a frame it first drops the rows whose object
is NaN, NaT or None (none left: `ValueError: n must be a positive
integer`). The trace-based variant reads concept:name whatever the key.
Labels are str() of the objects (later ones printed alike take trailing
\x1f), and the activities enter the transportation problem in the sorted
order. A key holding text alone stays on the text path unless some value is
blank (the text path refused it; pm4py reads it as any label).
HI2 the correlation transportation solver could spin forever (and grow until
the server ran out of memory): its single double cost put pm4py's 1e11
missing-pair cost beside costs near 1e5, and rounding left residual cycles
of tiny negative reduced cost on which Dijkstra never settled, with an
absolute tolerance of 1e-12. Costs are now kept as whole missing units plus
the rest (`FlowCost`), the tolerance scales with the largest cost, and a
search that does not settle fails instead of spinning. Pre-existing: HH's
server hangs on a concept:name CSV upload of hcS (s68519, classic) as on a
3x3 problem keyed by K1 (s68577, classic_split).
HI3 /api/align with standard costs on a discovered net reads pm4py's log
alignments (`reference_log_alignment`, as a supplied net did): each trace's
fitness is 1 - floor(cost / 10000) / floor(best-worst cost / 10000), so a
silent move's cost of 1 leaves a trace fitting, and the less-memory Dijkstra
prices silent moves and the best-worst cost its own way; legacy unit costs
keep Minera's bundle aligner.
HI4 /api/align's less-memory variant on a typed key of an upload's log shape
(an Alpha or Heuristics++ net mined under the same key) matches each
event's object with the net's labels by == (a NaN only the transition it
labels itself): it aligns the keyed log's labels
(`legacy_keyed_activities`, `csv_keyed_family`), which the net mined under
the key carries too; an event without the key or holding None reads None,
which no visible transition carries.
Checked without change: /api/align with standard costs on concept:name and
on keys holding text alone with the state-equation and Dijkstra variants
(totals equal before and after, move orders aside).
Sweeps (`ct/cor.py`, `ct/al.py`) over hfA, hfG and hfN (XES, keys K1..K6)
and hcS (CSV, 120 uploads), same as pm4py on HH's server vs HI's: the
correlation miner's frequency and performance DFGs keyed by K1..K6 hfA
494 -> 1,528 of 1,800 judgments, hfG 128 -> 537 of 1,080 (456 raising
alike in both), hfN 110 -> 514 of 1,080 (470 raising alike now), CSV
1,761 equal and 185 raising alike of 2,160 (HH's server hung on the 20th
upload, s68519, a concept:name case of HI2, and ran out of memory; its
first 19 uploads gave 66 equal); concept:name unchanged on XES (566 equal),
CSV 310 equal now. The rest are pm4py's cvxopt LP plans (classified by the
LP's own cost matrix, `lp_tie`: both plans optimal 201 + 75 + 90 + 213 on
the keys and 36 + 12 + 18 + 48 on concept:name; pm4py's plan not optimal,
declared since BQ, 71 + 12 + 6 + 0 and 17 + 3 + 8 + 2), and one CSV classic
TypeError whose operands rest on a NaN's address (declared). /api/align
with standard costs on concept:name (hfA and hcS; Alpha and Heuristics++,
three variants): the less-memory variant's totals (fitness, fitting traces,
total cost) Alpha 29 -> 192 and Heuristics++ 10 -> 110, the state-equation
and Dijkstra variants Heuristics++ 59 -> 110 each (Alpha 192 unchanged);
the less-memory variant on keys K1..K6 (before and after HI4) hfA
472 -> 841, hfG 119 -> 204, hfN 111 -> 185 (CSV 505 equal now, none
differing). Declared: the move
orders of equal-cost alignments, the messages for a net that is not easy
sound (Minera names the unreachable final marking), the move label of a
blank activity (pm4py prints '', Minera a silent-looking null; totals
equal), the CSV Alpha mine's transitions-sort operands (as HF), and a
model move on a None-labelled transition, silent in pm4py (cost 1) and
visible in Minera's net (4 judgments of hfN f90228, an event without the
key).
HI's suite: the full ctest 698 of 698 (-j2, 27 minutes) with the new
`minera_server_review_hi_tests` (454 checks, 302 of which fail on HH's
server binary, 8 seconds; one hash-seed dependent correlation case left
out, `tc/seedchk.py`); after HI4 was narrowed to nets mined under the same
key, the 47 server suites and the alignment suites passed again; the 16
manifests replayed 2,673 responses unchanged (before and after); no
frontend or Python script change.
Harness and probes: `review-harness/round-hi/`.

Round HJ re-reviewed HI. HI's CI run 45631958356 passed all six jobs (the
Linux, macOS and Windows native builds and ctest, the frontend, the Python
scripts and the parity verifier).
HJ1 /api/analyze-behavior's correlation family keyed by a typed attribute
or column (`behavior_correlation_key_objects`, the `key_objects` option)
reads the objects pm4py reads on the route's inputs: the upload's frame
(read_xes' DataFrame, format_dataframe of a CSV) and convert_to_event_log
of it, so every adapter reads the frame's cells, not read_xes' legacy
objects. An XES upload's column holds them as pandas types it
(`xes_frame_activity_column`: an int column beside a float or a gap
float64, dates UTC Timestamps in a datetime64 column and
datetime.datetime objects with their own offsets in an object column, a
gap NaN, NaT or None; each NaN of a float64 column its own float, one
shared NaN in a text or object column), a CSV upload's as /api/mine reads
them (`correlation_event_objects`, now shared with /api/mine's
`correlation_key_objects`). The log and stream adapters walk the converted
EventLog (classic_split keeps the gaps, in log order), the dataframe and
lazyframe adapters the frame's rows (classic_split drops the rows whose
object is NaN, NaT or None). A TypeError names a date datetime.datetime
when it is one (a legacy object, an object column's cell), else Timestamp.
HJ2 pandas' typing of an XES column of ints past INT64_MAX beside a
negative int (`xes_frame_cells`): the range check stops at the first None
(an event without the key) but not at a NaN gap, so the column is object
only when both ints come before the first None ([2**63, None, -1] is
float64, [2**63, nan, -1] and [-1, 2**63, None] object). It serves every
XES frame path keyed by such a column.
HJ3 a key the frame has no column for: the frame adapters raise pandas'
column selection error (`KeyError: "['K'] not in index"`) before any row
is read; the EventLog adapters a KeyError for the key at the first event.
HJ4 /api/align's moves name a blank event label: a log or synchronous move
on '' writes `"activity_name": ""` (pm4py prints ''), only a model move on a
silent transition writes null and `silent` (`write_alignments`); the
Conformance view shows it as `""` (`alignmentMoveLabel`).
HJ5 a transition labelled None (an Alpha or Heuristics++ net mined under a
key some event lacks) is silent in pm4py's alignments: /api/align aligns a
copy of the net with those transitions silent (`none_silent_net`, as the
token replay did since HG), and the less-memory Dijkstra prices every
silent transition 1 in a trace holding None (`less_memory_silent_costs`'
`none_activity`), since the trace's None meets each silent transition's
None label (HI's declared 4 judgments of hfN f90228, now equal).
Checked without change: /api/rework keyed by typed columns (pm4py's
get_rework_cases_per_activity checks the frame's column first: "the
activity column should be of type string.", as Minera answers; the legacy
log's statistic would print the objects).
Sweeps (`ct/cs2.py`: HI's `ct/cs.py` with the LP tie classification of
`ct/cor.py`; `ct/al.py`) over hfA, hfG and hfN (XES, keys K1..K6, 100 + 60
+ 60 uploads) and hcS (CSV, 120 uploads), three variants and four adapters
each: /api/analyze-behavior's correlation hfA 6,130 equal of 7,200
judgments (786 LP ties where both plans are optimal, 284 where pm4py's
plan is not), hfG 3,470 equal and 244 raising alike of 4,320 (516 + 78
LP), hfN 3,226 + 476 of 4,320 (530 + 76 LP), hcS 6,334 + 1,020 of 8,640
(1,222 LP ties); on the 10 hfA and 10 hcS uploads HI measured, HI's server
216 and 376 equal of 720 each, HJ's 496 and 616 + 80 raising alike (the
rest LP ties). Left: 88 TypeErrors whose operands rest on a NaN's address
(K6, a text column with gaps: one shared np.nan, hashed by its address in
3.11; declared since HI). /api/align (less-memory, Alpha and Heuristics++
nets mined under K1..K6): totals hfA 841 equal (unchanged), hfN 185 -> 189
of 189; moves hfA 768 and hfN 175 equal. Declared: the move orders of
equal-cost alignments (47 + 14), the messages for a net that is not easy
sound (45), equal values printed otherwise (30: -0.0 beside 0.0, 1 beside
1.0; pm4py keeps whichever object it met first), the mines' sort
TypeError operands beside None (hfN 98 Alpha, 1 Heuristics++; hash(None)
is an address in 3.11, declared since GZ), and a less-memory TypeError of a trace mixing None and
text (pm4py sorts the trace's labels; not met in the banks).
HJ's suite: the full ctest 699 of 699 (-j2, 30 minutes) with the new
`minera_server_review_hj_tests` (389 checks: 346 correlation judgments over
the four adapters and 43 alignments, 232 of which fail on HI's server
binary, 13 seconds; 8 cases whose TypeError operands follow the hash seed
left out, `tc/seedchk.py`); the 16 manifests replayed 2,673 responses
unchanged; frontend tsc and vitest passed (240 files, a new
`alignmentMoveLabel` case for the blank label); no Python script change.
Harness and probes: `review-harness/round-hj/`.

Round HK re-reviewed HJ. HJ's CI run 45649670104 passed every job: the
Linux, macOS and Windows native builds and ctests, the frontend, the Python
scripts and the parity verifier.
HK1 /api/align-petri-workbench aligns a supplied net as pm4py's public log
alignments do, as `/api/align`'s imported model does since HI
(`imported_model_alignment`): the easy-soundness check first, every
trace's key, the best-worst cost and each variant in order, with standard
costs where `cost_profile=standard` and the dispatcher's default variant
(state equation A*) where the form names none. It aligned with the older
per-trace check before, whose costs, fitness and totals differed from
pm4py's on most nets (hfA 1,023 of 1,800 judgments, concept:name too).
HK2 a log or synchronous move on a blank event label is a move (pm4py
lists ('', '>>')); the alignment search dropped it.
HK3 the workbench keyed by an attribute or column of other objects than text
(`workbench_keyed_input`) reads pm4py's objects on each adapter: an XES
upload's log and stream read_xes' legacy objects (an event without the key
`KeyError`, a dict `TypeError: unhashable type: 'dict'`, each NaN its own
float, dates datetime.datetime; the stream regroups the events by case),
its dataframe the read_xes frame's cells (`xes_frame_activity_column`; the
cases by id) and its lazyframe their Polars copy (a gap None, an object
column str() of each cell; the cases as they first appear); a CSV
upload's formatted frame's cells, its Polars copy holding an object column
(True/False beside a gap) as String. The trace-net searches fail at the
first object that is no str (`TypeError: can only concatenate str (not
"int") to str`), the less-memory search sorts each trace's distinct objects
in CPython's set order (`TypeError` for two kinds, None beside anything);
a missing column is the frames' column error (`KeyError: 'Column not
found: K'`, Polars' plan error). Otherwise each object aligns as an event
no transition of the PNML's text labels meets (str() of it marked apart,
None the dispatcher's None label beside the net's silent transitions);
the moves and activities show the values as printed, and the result names
the key the form gave (the frontend checks it against the input context).
The keyed error is raised only where the net is easy sound, else the
uncomputed result of GH1-2 stands.
HK4 /api/mine's POWL keyed by an attribute or column of other objects than
text on an upload's log shape (`powl_log_shape_keyed`): pm4py reads each
event's object (an XES upload's legacy objects; a CSV upload's
convert_to_event_log cells, a float64 column's gaps one NaN each, NaT in a
date column), raising `KeyError` for an event without the key and
`TypeError: unhashable type: 'dict'`, sorts the set of the objects
(CPython's set order; `TypeError` with its operands where two kinds meet),
and mines them in Python's order, objects equal as dict keys one activity
(1, 1.0 and True; 0.0 and -0.0). Minera mines a copy named by each
object's rank, which its text order then follows, and names the net's
transitions str() of the objects. The frame path of round BS had named
them str() of the frame's cells (an absent attribute NaN, 1e3 and 1000 one
label but -0.0 and 0.0 two) and ordered them as text, so the POWL differed
on most typed keys (hfA 194 of 240 keyed judgments).
HK5 the POWL net's token replay keyed by the key it was mined by reads the
net's labels (pm4py's transitions are labelled by the objects themselves,
which the replay meets): Minera replayed the typed attribute, whose
objects never meet a text label, so every event was skipped (fit, with
fewer tokens) or, on a CSV upload, failed with the replay's KeyError. The
decoration reads the same names. Another replay key (an explicit
`token_replay_activity_attribute`) reads its own attribute and no longer
projects the log by it, which had raised the POWL miner's sort TypeError.
Checked without change: the explicit-key totals of the POWL net mined on
concept:name (HJ's open item) were equal on hfA, hfG and hcS.
Sweeps (`ct/wb.py`, `ct/pw.py`) over hfA, hfG and hfN (XES, keys
concept:name and K1..K6, 25 + 25 + 25 uploads) and hcS (CSV, 25): the
workbench's alignments with Alpha and Inductive nets (Minera's PNML export
and pm4py's Inductive on str() of the key), two variants and four
adapters, judgments equal (moves and costs, or the same error) before ->
after: hfA 50 -> 1,316 of 1,800 (totals 191 -> 1,214 of 1,214), hfG 101 ->
652 of 896, hfN 123 -> 750 of 928, hcS 113 -> 1,186 of 1,464 (totals 169
-> 882 of 882). Left, declared: the move orders of equal-cost alignments
(hfA 273, hfG 129, hfN 107, hcS 96), equal-cost alternatives that follow
the hash seed (91, 35, 63, 6; pm4py's own choice moves under
PYTHONHASHSEED=3), and the message for a net that is not easy sound (120,
80, 8, 176: pm4py raises, Minera reports the uncomputed result of GH1-2).
POWL token replay totals (40 uploads per bank, keys K1..K6, the key as
powl_activity_key, as both keys, or only as the replay key): keyed hfA 40
-> 227 of 240 equal, hfG 17 + 122 errors -> 96 + 144, hfN 19 -> 93 + 146
errors, hcS 59 + 47 -> 138 + 44; with only the replay key unchanged but
for hfN's 5 TypeErrors of its projection (225 of 240 equal or raising
alike). Left, declared: a key whose objects
include NaN or NaT (pm4py's miner, comparing them with ==, drops such an
activity or recurses until RecursionError: hfA 13, hfN 1, hcS 49 keyed
judgments), the TypeError operands beside a text column's shared NaN (hcS
9; its hash is an address, declared since HI), and a None-valued event
replayed by an explicit key (hfN 15: pm4py's label map sends None to one
of the POWL net's silent transitions, the last its set of transitions
lists, an address order).
Not reached: the session routes that replay or align the cached POWL net
under its typed key after the mine (/api/align, decorations and the
others project the log as /api/mine does, so their labels agree, but
pm4py's alignments there were not compared).
HK's suite: the full ctest 700 of 700 (-j2, 31 minutes) with the new
`minera_server_review_hk_tests` (560 checks over 24 uploads and 68 PNML
models, 462 failing on HJ's binary; 5 seed-dependent cases left out,
`tc/seedchk.py`); the 16 manifests replayed 2,668 responses unchanged and
5 changed, the workbench's standalone Petri captures (log, stream,
dataframe, lazyframe, complete typed metadata), refreshed for HK1's
default variant (the variant and the LP diagnostics only); frontend tsc
and vitest passed (240 files, 10,734 tests); no frontend source or Python
script change.
Harness and probes: `review-harness/round-hk/`.

Round HL re-reviewed HK. HK's CI run 45668934361 passed the Linux, macOS,
frontend, Python script and parity verifier jobs; its Windows ctest was
still running when HL started.
HL1 the session routes on a POWL net mined under a typed key
(`powl_activity_key` naming an attribute or column of other objects than
text) read a projected log whose key attribute holds each event's name as
the net names it (`powl_keyed_projection` in `normalize_powl_event_log`):
pm4py's less-memory alignments compare the key's objects with the
transitions' labels by ==, and the net's labels are those objects, so
every event meets its transition. The projection had kept the attribute as
uploaded, so /api/align miner=powl met 1.0 against a transition named 1 as
a log move, and on a CSV upload an event with a gap in the key failed with
`conformance: event is missing activity attribute 'K3'` where pm4py aligns.
Sweep `ct/pa.py` (40 uploads per bank, keys K1..K6, standard costs, the
default less-memory variant): judgments equal (each trace's cost, fitness
and moves) before -> after: hfA 124 -> 216 of 234 (10 more equal but for
the printing of equal objects, a move on -0.0 shown 0.0), hfG 55 -> 94 of
96 (+2), hfN 51 -> 87 of 94 (+4), hcS 59 -> 128 of 175 (108 one-sided
errors -> 0); totals hfA 124 -> 226, hfG 55 -> 96, hfN 51 -> 91, hcS 59 ->
131. The trace-net variants raise pm4py's concat TypeError alike
(unchanged). The decoration of the POWL net (`ct/pdec.py`, firing counts
over the fitting traces against pm4py's token replay keyed the same, the
nets compared by Weisfeiler-Lehman colours) was equal wherever the nets are
(hfA 226, hfG 96, hfN 91, hcS 128); HK's binary gives the same on the
ctest's cases (HK5 had projected the names it reads).
HL2 /api/stochastic-map?miner=powl fits the POWL net replaying the key it
was mined by, as pm4py's get_map_from_log_and_net does under that
activity key: it replayed concept:name, whose names never meet a typed
key's transitions, so every transition got the immediate distribution.
Sweep `ct/pdec.py` smap (the multiset of distribution, parameters and
weight): hfA 141 -> 177 of 234, hfG 69 -> 85 of 96, hfN 61 -> 77 of 94,
hcS 115 of 175 unchanged (its differences are nets with NaT keys).
HL3 /api/analyze-behavior's temporal_profile and temporal_conformance on
the stream adapter read the session's log shape as the log adapter does:
pm4py's stream variant converts the stream back to the log it came from
(observed black-box: equal results for every typed key). Minera read the
stream as text cells (an XES upload's typed values as text, a CSV
upload's gaps as empty labels). Sweep `ct/bh.py` (hfA 20 + hfN 10 uploads,
K1..K6, 180 judgments per family): stream profile 84 -> 169 of 180 (11
more equal but for printing, below), stream conformance 83 -> 180; hcS (20
uploads, 120): profile 44 -> 120, conformance 85 -> 120.
HL4 the XES dataframe's temporal profile names typed activities str() of
the frame's objects (`temporal_profile_xes_frame_labels`, as /api/mine's
family result since GZ): the route named them by the merged XES text (1e3
for 1000.0). HL5 pandas' groupby drops a pair whose activity is a float
column's NaN as it drops a gap; Minera kept a NaN group. Sweep: dataframe
profile 74 (+38 printing) -> 177 of 180 (+3 printing).
Left for HM (not declared): the printing of equal objects in temporal
profile pairs (pm4py's log variant keys each pair by the first pair's
tuple of objects, (-0.0, 0.0) or (1.0, 1); pandas' groupby by each
column's first value in the pair frame; Minera names both by the column's
first text: log and stream 11 of 180, dataframe 3); the stochastic map's
falsy labels (a transition labelled by 0, 0.0, -0.0, False or '' gets the
immediate distribution in pm4py, its durations unsampled: hfA 53, hfG 11,
hfN 15, hcS 16 judgments); the XES dataframe's temporal conformance keyed
by typed attributes (pandas' merge raises `ValueError: You are trying to
merge on int64 and str columns for key 'K1'`, 105 of 180 one-sided
errors); minimum_self_distance keyed by typed attributes on the pandas
(concat TypeError) and Polars (str() labels, None) variants; the CSV frame
adapters of the temporal families (hcS dataframe 17 differences).
Left, declared: the POWL nets of keys holding NaN or NaT (HK's residual:
the alignments, decorations and maps on them follow the differing net:
hfA 8, hfN 3, hcS 45 alignment judgments), the printing of equal objects
in alignment moves (above), and equal-cost move orders of the A* variant
(hfA 26, hfG 5, hfN 4, hcS 3).
Not reached: /api/playout and the POWL evaluate/precision routes on a net
mined under a typed key (`ct/pev.py` written, not run), the lazyframe
adapter of the temporal families (pm4py's Polars path for the profile
needs pyarrow, which the reference venv must not hold).
HL's suite: the full ctest 701 of 701 (-j2, 26 minutes) with the new
`minera_server_review_hl_tests` (356 checks over 40 uploads: POWL
alignments, decorations and stochastic maps under typed keys, temporal
profiles on the stream and dataframe adapters and conformance on the
stream; 189 fail on HK's binary; `tc/seedchk.py` under PYTHONHASHSEED 1
and 3: none differ); the 16 manifests replayed 2,673 responses unchanged;
no frontend or Python script change.
Harness and probes: `review-harness/round-hl/`.

Next round (HM) should start by re-reviewing round HL:

- HL's CI run, and HK's Windows job;
- the HL items left for HM above (temporal profile pair labels of equal
  objects, the stochastic map's falsy labels, the typed-key dataframe
  temporal conformance merge error, minimum_self_distance's pandas and
  Polars variants on typed keys, the CSV frame adapters of the temporal
  families);
- /api/playout and POWL evaluate/precision on a net mined under a typed
  key (`ct/pev.py`);
- whether other routes iterate a copied Python set (HE2, not reached in
  HF..HL);
- the declared residuals (HF's..HL's above, the NaN-addressed orders and
  operands, the GU1-2 whole-mine failure, the GT2 and GT4 residuals) if a
  cheaper model appears;
- the carried items below.

Previous list for round HL (kept for its older items):

Next round (HL) should start by re-reviewing round HK:

- HK's CI run;
- the session routes on a POWL net mined under a typed key (/api/align,
  decorations, playout, stochastic map) against pm4py;
- the other /api/analyze-behavior families keyed by a typed attribute or
  column on the stream and lazyframe adapters (HG..HI covered the log and
  frame shapes; carried from HJ);
- whether other routes iterate a copied Python set (HE2, not reached in
  HF..HK);
- the declared residuals (HF's..HK's above, the NaN-addressed orders and
  operands, the GU1-2 whole-mine failure, the GT2 and GT4 residuals) if a
  cheaper model appears;
- the carried items below.

Previous list for round HK (kept for its older items):

Next round (HK) should start by re-reviewing round HJ:

- HJ's CI run;
- the workbench's alignments on typed keys (/api/align-petri-workbench with
  `alignment_activity_attribute` over the four adapters) and the POWL net's
  totals with an explicit replay key (still unverified: the reference
  harness does not mine POWL);
- the other /api/analyze-behavior families keyed by a typed attribute or
  column on the stream and lazyframe adapters (HG..HI covered the log and
  frame shapes);
- whether other routes iterate a copied Python set (HE2, not reached in
  HF..HJ);
- the declared residuals (HF's..HJ's above, the NaN-addressed orders and
  operands, the GU1-2 whole-mine failure, the GT2 and GT4 residuals) if a
  cheaper model appears;
- the carried items below.

Previous list for round HJ (kept for its older items):

Next round (HJ) should start by re-reviewing round HI:

- HI's CI run;
- /api/analyze-behavior's correlation keyed by a typed attribute or column
  (four adapters; `ct/cs.py`): pm4py reads the upload's frame (read_xes'
  DataFrame, format_dataframe of a CSV) and convert_to_event_log of it, so
  an XES upload's objects are the frame's cells, not the legacy objects;
  148 of 360 judgments per adapter equal on 10 hfA and 10 hcS uploads
  (LP ties not yet separated there);
- a None-labelled transition of a net mined under a typed key in the
  alignments (silent in pm4py, HI's declared 4), and the move label of a
  blank activity;
- the workbench's alignments on typed keys and the POWL net's totals with an
  explicit replay key (still unverified: the reference harness does not
  mine POWL);
- whether other routes iterate a copied Python set (HE2, not reached in
  HF..HI);
- the declared residuals (HF's..HI's above, the NaN-addressed orders and
  operands, the GU1-2 whole-mine failure, the GT2 and GT4 residuals) if a
  cheaper model appears;
- the carried items below.

Previous list for round HI (kept for its older items):

Next round (HI) should start by re-reviewing round HH:

- HH's CI run;
- the correlation miner keyed otherwise than concept:name (found in HH,
  not fixed; `ct/cor.py`): pm4py's trace-based variant reads concept:name
  whatever the activity key (XES and CSV alike, Minera reads the key); on an
  XES upload's log shape the classic variants name typed values str() of
  the legacy objects ('1000.0', 'False', a Timestamp) where Minera shows the
  XES text, and the classic variant drops a NaN activity's events while
  classic_split keeps them; on CSV uploads Minera refuses a gap ("activity
  selector ... is blank") where pm4py reads NaN, and pm4py's classic variant
  raises TypeError sorting NaN beside text;
- /api/align's per-variant fitness with standard costs on nets with silent
  transitions (suspected from the fitting-trace counts of K6 Heuristics++
  nets: pm4py's fitness seems to drop the silent moves' cost of 1; check
  on concept:name too);
- the less-memory alignment variant on typed keys (pm4py matches the
  objects and sorts the net's and the trace's labels apart), the
  workbench's alignments on such keys, and the POWL net's totals with an
  explicit replay key (still unverified: the reference harness does not
  mine POWL);
- whether other routes iterate a copied Python set (HE2, not reached in
  HF, HG or HH);
- the declared residuals (HF's, HG's and HH's above, the NaN-addressed
  orders and operands, the GU1-2 whole-mine failure, the GT2 and GT4
  residuals) if a cheaper model appears;
- the carried items below.

Previous list for round HH (kept for its older items):

Next round (HH) should start by re-reviewing round HG:

- HG's CI run;
- the same typed keys on the routes HG did not reach: alignments
  (/api/align and the workbench) and the AUTOMATON_AFTER_ALIGN precision on
  nets mined under them, the POWL net's totals with an explicit replay key
  (Minera replays the frame's cells; the reference harness does not mine
  POWL), the evaluate routes' replay of an Alpha net's None label (silent
  in pm4py; /api/mine models it, evaluate does not), a CSV log skeleton
  conformance keyed otherwise than its discovery, and the footprints,
  minimum self-distance and correlation families keyed by such values;
- whether other routes iterate a copied Python set (HE2, not reached in HF
  or HG);
- the declared residuals (HF's and HG's above, the NaN-addressed orders and
  operands, the GU1-2 whole-mine failure, the GT2 and GT4 residuals) if a
  cheaper model appears;
- the carried items below.

Previous list for round HG (kept for its older items):

Next round (HG) should start by re-reviewing round HF:

- HF's CI run;
- the families keyed by a typed attribute on an XES upload's log shape on
  routes past /api/mine's defaults: the token replay with an explicitly
  supplied replay key, the workbench and evaluate routes on the Alpha and
  Heuristics++ nets, and the conformance families of the log skeleton and
  temporal profile with such keys;
- whether other routes iterate a copied Python set (HE2, not reached in
  HF);
- the declared residuals (HF's above, the NaN-addressed orders and
  operands, the GU1-2 whole-mine failure, the GT2 and GT4 residuals) if a
  cheaper model appears;
- the carried items below.

Previous list for round HF (kept for its older items):

Next round (HF) should start by re-reviewing round HE:

- HE's CI run;
- the other families keyed by a typed attribute on an XES upload's log
  shape (Alpha, Inductive, the log skeleton, the temporal profile,
  Heuristics++, the transition system): HE fed Declare the legacy log's
  objects (`legacy_declare_cell`); the same reader may serve them;
- whether other routes iterate a copied Python set (HE2): any pm4py path
  that builds `set(some_set)` of five or more activities and lists it;
- the declared residuals (NaN-addressed orders and operands, the GU1-2
  whole-mine failure, the GT2 and GT4 residuals) if a cheaper model
  appears;
- the carried items below.

Previous list for round HE (kept for its older items):

Next round (HE) should start by re-reviewing round HD:

- HD's CI run (and HC's Windows ctest);
- Declare keyed by a typed attribute on an XES upload's log shape (the
  hybrid legacy log): gxB with keys X and start_timestamp, 148 of 400
  judgments where pm4py raises (sort TypeErrors between mixed types,
  KeyErrors of a rule column holding a big int beside its float,
  "Categorical categories must be unique") and Minera mines, 72 model
  diffs (uint64 and big ints beside floats), pre-existing (`sw.sh`'s `xl`
  sweep; the DataFrame shape `xd` matches but for the declared operand
  orders and KeyError templates);
- the base Declare families' order of a date column with NaT (hcS K3, 4
  of 120 conformance messages name NaTType where pm4py names Timestamp):
  their tuple set holds no NaN, so its order may be modelled from the
  tuple hash of (template, act, act2);
- the Inductive miner on an activity column with no value (carried);
- the declared residuals if a cheaper model appears;
- the carried items below.

Previous list for round HD (kept for its older items):

Next round (HD) should start by re-reviewing round HC:

- HC's CI run;
- Declare's collapsed-key write order on a CSV frame keyed by True/False
  with gaps (above), and whether the earlier rounds' declared operand
  orders still hold with the reference at PYTHONHASHSEED=0 (their sweeps
  ran pm4py under random seeds);
- a CSV upload's log shape keyed by typed or gapped columns for Alpha,
  Inductive, the log skeleton, the temporal profile and Heuristics++
  (convert_to_event_log of the typed frame; HC swept the frame shapes);
- the Inductive miner on an activity column with no value (pm4py's tau
  tree on Polars, the str() TypeError on pandas);
- the declared residuals (sort TypeError operands, KeyError templates,
  address-ordered collapsed keys, the log skeleton's bool equal to an int,
  Inductive's RecursionError on a NaN activity, the playout's family order,
  the GU1-2 whole-mine failure, the GT2 and GT4 residuals) if a cheaper
  model appears;
- the carried items below.

Previous list for round HC (kept for its older items):

Next round (HC) should start by re-reviewing round HB:

- HB's CI run;
- activity keys over a CSV upload's typed or gapped columns on its frame
  shapes for Alpha, Inductive, the log skeleton, the temporal profile and
  Heuristics++ (`ct/csvk.py`): read format_dataframe's typed cells (or
  the Polars copy's) as the XES frame paths read theirs, as GM did for
  the classic heuristics (`csv_frame_typed_heuristics`);
- the session's Declare routes that replay its model (the playout reads
  the text model where pm4py would play out the typed one);
- the declared residuals (sort TypeError operands, KeyError templates,
  address-ordered collapsed keys, the log skeleton's bool equal to an int,
  Inductive's RecursionError on a NaN activity, the GU1-2 whole-mine
  failure, the GT2 and GT4 residuals) if a cheaper model appears;
- the carried items below.

Previous list for round HB (kept for its older items):

Next round (HB) should start by re-reviewing round HA:

- HA's CI run;
- Declare keyed by a typed column on a CSV upload's frame shapes (GT2-2,
  GT2-3: the text column's operand order and pm4py's separate NaN keys):
  HA's rules-frame model (`typed_declare_model`) may settle them if the
  CSV frame's objects feed it;
- Declare conformance keyed by a typed XES column (the model's typed keys
  read back by `conformance_declare`), and the session's Declare routes
  that replay the model;
- the and-measure order of a Heuristics++ net with typed nodes (Python's
  sorted() of a node's successors, NaN pairs), which the family JSON does
  not show;
- `ct/u64.py`'s date normalization (`dnorm`) hides a date label shown as
  the XES text instead of str(): rerun the other families keyed by typed
  columns with exact labels (as `tc/mkha.py` compares them);
- the sort TypeErrors' operands (declared), the log skeleton where a bool
  equals an int (declared), the GU1-2 whole-mine failure (declared) and the
  GT2 and GT4 residuals (declared) if a cheaper model appears;
- the carried items below.

Previous list for round HA (kept for its older items):

Next round (HA) should start by re-reviewing round GZ:

- GZ's CI run;
- Declare keyed by a typed XES column (`ct/u64.py` dc_aX, dc_ast): pm4py's
  rule keys mix the column's objects with floats (`('1', '5.0')`), raise
  `KeyError: ('precedence', 0, 1.8446744073709552e+19)` on ints past 2**53,
  and keep one-element keys in binary families (`'True': (5, 4)`); Minera
  answers with the texts, and on a float64 column with gaps it refuses
  (`event activity selector is missing or null`) where pm4py answers;
- Heuristics++ keyed by a typed column (hpp_aX, hpp_ast): float labels of
  ints past int64 (`9.223372036854776e+18`), the abstraction's sort
  TypeErrors and missing-value nodes;
- the sort TypeErrors' operands (declared): emulating CPython's set order
  for numbers and None would settle Polars' `'T' and 'NoneType'` cases, but
  hash(None) is an address in 3.11, so it stays declared unless a cheaper
  model appears; the log skeleton where a bool equals an int (declared:
  pm4py's relation sets keep whichever equal object each inserted first);
- the GU1-2 whole-mine failure (declared) and the GT2 and GT4 residuals
  (declared) if a cheaper model appears;
- the carried items below.

Previous list for round GZ (kept for its older items):

Next round (GZ) should start by re-reviewing round GY:

- GY's CI run;
- activity keys over typed XES columns on the frame shapes (`ct/u64.py`
  keys a_aX, i_aX, dc_aX, hpp_aX, ls_aX, tp_aX): ints past int64 as labels
  (Alpha mixes floats from its shifted pairs with the column's ints,
  Declare raises a KeyError on a float, heuristics a subscript TypeError,
  Inductive refuses), int64 labels that Alpha shows as floats ("-3.0"),
  the sort and concatenation TypeErrors pm4py's Inductive, Alpha,
  Declare and Heuristics++ raise on mixed-object keys where Minera answers,
  the log skeleton's `KeyError: NaT` (Minera `nan`), and an activity key
  named start_timestamp on a frame without that column (pm4py's frame paths
  add start_timestamp as the timestamp first, so the timestamps become the
  activities); the log skeleton keyed by a typed XES column on the frame
  shapes, which now answers instead of failing the mine but labels its pairs
  with the XES text (pandas' str of the typed value: dates as Timestamps,
  booleans True/False, 0 and False one key);
- the GU1-2 whole-mine failure (declared) and the GT2 and GT4 residuals
  (declared) if a cheaper model appears;
- the carried items below.

Previous list for round GY (kept for its older items):

Next round (GY) should start by re-reviewing round GX:

- GX's CI run;
- the routes besides the eventually-follows graph that still fall back on
  an XES column of ints past int64 (the temporal profile, the frame DFG,
  heuristics, Declare), and the whole-mine `'values' is not ordered`
  failure on XES mixed-object columns (GU1-2), now the only difference left
  in the eventually-follows sweeps;
- the eventually-follows filter and any other route that reads pm4py's
  frame partial order;
- the libm-sensitive counts elsewhere (any ceil or floor of a probability
  product next to an integer) on macOS and Windows;
- the GT2 and GT4 residuals still open (Declare's separate NaN keys, the
  transition system on a CSV log shape keyed by mixed objects);
- the carried items below.

Previous list for round GX (kept for its older items):

Next round (GX) should start by re-reviewing round GW:

- GW's CI run (GV's macOS playout check now tolerates libm-driven counts);
- the eventually-follows graph's other inputs: an explicit
  `start_timestamp_attribute` or another timestamp key (Minera keeps the log
  graph there), XES frames where a typed column cannot be built, the
  eventually-follows filter and any other route that reads pm4py's frame
  partial order;
- the libm-sensitive counts elsewhere (any ceil or floor of a probability
  product next to an integer) on macOS and Windows;
- the GT2 and GT4 residuals still open (Declare's separate NaN keys, the
  transition system on a CSV log shape keyed by mixed objects);
- the carried items below.

Previous list for round GW (kept for its older items):

Next round (GW) should start by re-reviewing round GV:

- GV's CI run;
- GV3 on more banks: the lazy shape's other routes that build a DFG
  (`/api/discover-dfg`, the stochastic map and playout on the lazy shape),
  Polars' tie order with business hours (which int the SchemaError names);
- the all-empty timestamp upload (t72074) on the frame shapes, where every
  pm4py frame function checks the date column first;
- the temporal profile on a uint64 own start with Polars (the dtype named
  `i64` for `u64`, and the business-time UDF's `'int'` error where Minera
  measures);
- the GT2-GT4 residuals (Declare's separate NaN keys, the Boolean uploads
  whose gap rows format_dataframe dropped, the transition system on a CSV
  log shape keyed by mixed objects);
- the carried items below.

Previous list for round GV (kept for its older items):

Next round (GV) should start by re-reviewing round GU:

- GU's CI run (and GT's Windows job, still building at GU's handoff);
- GU3-2, `/api/dfg-dataframe` on a start column of no dates (frequency in
  the column's pandas order, the subtraction TypeError for performance,
  XES date text as object);
- the frame DFG start order on the other routes that build a DFG from an
  upload: the stochastic map and playout performance DFGs, the Polars
  lazy shape's process map (Polars sorts a null start first and clamps),
  the CSV upload's log shape (an EventLog whose properties name the start
  key: clamped at 0, in timestamp order) where a route mines it;
- GT1 on fresh banks and the GT2-GT4 residuals (not taken in GU, below);
- the carried items below.

Previous list for round GU (kept for its older items):

Next round (GU) should start by re-reviewing round GT:

- GT's CI run (and GS's macOS and Windows ctest, still running at GT's
  handoff);
- GT5, the start_timestamp column of an upload's frame in the process map
  (`/api/mine` edges, start and end counts, `performance.edges`) and the
  DFG performance variant, on CSV and XES uploads and the EventLog path of
  table sources (`ct/pmap.py`, `x/pst.py`, gtP), then the other routes
  that read pm4py's frame DFG (the Inductive miner's frame path, the
  heuristics and footprints frame paths, `/api/dfg-dataframe`);
- GT1 on fresh banks: the CSV log shape's temporal reads with business
  hours and custom keys on other routes, read_csv's str typing of big ints
  on the other CSV families;
- the GT2-GT4 residuals: Declare's separate NaN keys, the 2 Boolean
  uploads whose gap rows format_dataframe dropped (variants, the temporal
  profile), Polars' uint64 plan text and business-hours UDF (11 judgments),
  the transition system on a CSV log shape keyed by mixed objects;
- the carried items below.

Previous list for round GT (kept for its older items):

Next round (GT) should start by re-reviewing round GS:

- GS's CI run (and GR's macOS and Windows jobs, still running at GS's
  handoff);
- GS1/GS2 on fresh CSV banks: start columns of other kinds (object ints
  above int64, mixed bools and numbers, dates with gaps beside text),
  business hours with gaps in both columns, the conformance with a custom
  zeta and a Boolean start column; the `/api/analyze-behavior` temporal
  conformance of a CSV log shape (variant log) with the same columns;
- the carried Declare and Inductive families on CSV logs keyed by a
  column with gaps (above), and the DFG and variants families there;
- the 2 grAs footprints uploads with time:timestamp as the activity key
  (two traces' NaN start activities stay two objects in pm4py's sets,
  Minera one `nan`; 0 and False named after the first object);
- the open transition-system item from GQ (0 and False named by the
  creating object) and the carried items below.

Previous list for round GS (kept for its older items):

Next round (GS) should start by re-reviewing round GR:

- GR's CI run;
- GR1 on fresh banks: other custom footprints keys (concept:instance,
  lifecycle:transition, a bool or dict column) and `/api/discover-graph`
  footprints with custom keys on frame sessions; the 2 open grAs uploads;
- GR4 on the rest of the CSV frame shapes: Polars' temporal profile on a
  start column of other values than dates (plan errors such as `- not
  allowed on f64 and datetime[μs, UTC]`), Polars' conformance deviation
  order within a case (`ct/tpccsv.py` lazyframe: 40 of 100 default
  judgments differ only in order), pandas business hours with the start
  column as the timestamp key, the empty pair frame's ValueError in CSV
  conformance, an object start column;
- GR2's carried families: Declare and Inductive on a CSV log keyed by a
  column with gaps (pm4py keeps a NaN in odd places or raises TypeError;
  Minera's messages differ), the DFG and variants families there;
- the open transition-system item from GQ (0 and False named by the
  creating object) and the carried items below.

Previous list for round GR (kept for its older items):


Next round (GR) should start by re-reviewing round GQ:

- GQ's CI run;
- GQ1 on fresh banks: temporal conformance with business hours and custom
  keys on CSV uploads' frame shapes, and the Polars plan errors with gaps in
  both columns;
- custom footprints keys on XES frame sessions (found in GQ, not fixed):
  a case key naming an event column of the frame (concept:name) groups the
  frame's rows by that column in pm4py's converted EventLog, one trace per
  value, while Minera keeps the XES traces (`ct/fpmine.py` FPKEY=case_act:
  228 of 1,336 judgments equal per shape on gqAs); an activity key the
  frame lacks raises pm4py's `KeyError: 'org:resource'` (Minera "activity
  selector is missing or null", FPKEY=act_res, 174 uploads per shape); a
  typed activity key with gaps (start_timestamp) raises pm4py's KeyError
  of the key (FPKEY=act_st; the probe's harness needs a guard for
  responses without an analysis);
- GQ4 on the other views of a typed key (multiset counts with NaN, backward
  windows) and on CSV uploads' frame shapes; the float64 gap rule (GQ3) on
  the other miners of a CSV log shape (DFG, Alpha, variants), where pm4py's
  EventLog holds the same distinct NaN objects;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1, and the
  anti- and multi-alignments of an upload without traces declared in GJ).

Previous list for round GQ (kept for its older items):

Next round (GQ) should start by re-reviewing round GP:

- GP's CI run (and GO's Windows ctest, still running at GP's handoff);
- GP1 on fresh banks: the temporal profile's conformance with business hours
  (pandas raises the same ValueError on an empty pair frame; Minera's
  conformance path is unchanged), business hours with a custom timestamp
  key naming a typed column, the Polars frame with business hours and gaps
  in both columns;
- GP3 on the other routes reading an XES frame session as a log:
  `/api/mine`'s footprints family on frame sessions with traces without a
  case id or a missing activity, the footprints conformance variants
  (trace_extensive, log_extensive) and a custom footprints activity or case
  key on such sessions, `/api/discover-graph` transition systems there;
- the open float64 key with several gaps (above);
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1, and the
  anti- and multi-alignments of an upload without traces declared in GJ).

Previous list for round GP (kept for its older items):

Next round (GP) should start by re-reviewing round GO:

- GO's CI run (and GN's Windows ctest, still running at GO's handoff;
  GN's macOS job was cancelled);
- GO1 on fresh banks: Heuristics++ with business hours on typed-timestamp
  XES frames, the frequency decoration, `/api/discover-workflow` on the
  same sessions, token replay and alignments of the nets mined there;
- the option sets on typed-timestamp XES frames that differ before and
  after GO (`ct/optval.py`, goA/goL): the temporal profile with business
  hours (pandas' `Cannot set a DataFrame with multiple columns to the
  single column @@flow_time`, `str.replace() takes no keyword arguments`
  where Minera subtracts first) and with a custom timestamp or start key
  naming a typed column (pandas' KeyError wording, `Invalid comparison
  between dtype=datetime64[us, UTC] and ndarray`, dicts), Heuristics++
  with `start_timestamp` as the timestamp key (KeyError wording, typed
  cells), the classic performance view with a start key (2 to 5 uploads);
- `/api/discover-graph` footprints on an XES frame with a missing
  activity: pm4py computes the log's and the cached model's footprints
  (the NaN label one more activity), Minera answers "activity selector is
  missing or null" (8 of 200 goA uploads, both shapes);
- the CSV log shape keyed by a float64 column with several gaps: pm4py
  names one node per gap (each NaN its own object), Minera one `nan` node;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1, and the
  anti- and multi-alignments of an upload without traces declared in GJ).

Previous list for round GO (kept for its older items):

Next round (GO) should start by re-reviewing round GN:

- GN's CI run (and GM's Windows job: GM's Linux, macOS, script, frontend and
  verifier jobs passed, Windows ctest was still running at handoff);
- GN1 on fresh banks: the other families on typed-timestamp XES frames
  (heuristics, log skeleton and transition system matched; Declare,
  Heuristics++, the performance view, token replay and alignments of the
  mined nets, `/api/discover-workflow`, conformance routes), a custom
  timestamp or start key naming a typed column, and business hours on the
  temporal profile of such frames;
- the CSV log shape with a custom activity key whose column has gaps:
  convert_to_event_log keeps NaN and NaT in the event dicts (Minera drops
  them), so pm4py names a 'nan' node and its token replay sorts the
  variants with a NaN beside text (`'<' not supported between instances
  of 'float' and 'str'`), and a NaT key or the timestamp column as the key
  replays without KeyError (kcT j13663, j13675, j13690, j13695);
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1, and the
  anti- and multi-alignments of an upload without traces declared in GJ).

Previous list for round GN (kept for its older items):

Next round (GN) should start by re-reviewing round GM:

- GM's CI run;
- GM1 on fresh banks: typed custom activity keys with business hours, a
  start key or another timestamp key on XES and CSV frames (left on
  Minera's path), the Heuristics++ variant with such a key, and
  `/api/discover-workflow` on the same sessions;
- CSV uploads with missing timestamps and a custom activity key (kcT
  j13663, j13675, j13690, j13695: frames read the case and st2 columns on
  Minera's path; the log shape's replay and sort order);
- /api/mine's other families on XES frames whose time:timestamp and
  columns hold other kinds (gl banks, default keys, glA/glC/glF/glG/glK:
  pandas Alpha 138, temporal profile 137, Inductive and POWL 34 of 250
  differ; Polars 67, 53 and 37): messages for typed timestamps ("Alpha:
  invalid selected timestamp", pandas' own TypeErrors), None against NaN,
  an elist read as '' instead of None, distinct NaN objects;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1, and the
  anti- and multi-alignments of an upload without traces declared in GJ).

Previous list for round GM (kept for its older items):

Next round (GM) should start by re-reviewing round GL:

- GL's CI run;
- GL1 on fresh banks: another timestamp key with offset dates in pandas
  object columns under business hours (read on their own wall clock),
  the frequency decoration with custom keys, a timestamp key naming a
  role column (resource, lifecycle, case) on all shapes;
- the custom typed activity key on XES frame shapes (carried: str() labels
  of Polars' cells, pandas' object-column types in the subscript, sort
  and conversion errors) and on CSV frames;
- `/api/mine`'s steps after a net mined with a typed custom activity key on
  the XES log shape (token replay, decorations);
- columns of None alone on XES frames for the other families (Alpha,
  Inductive, Declare, temporal profile): pandas keeps them, Minera's
  present columns drop them;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1, and the
  anti- and multi-alignments of an upload without traces declared in GJ).

Previous list for round GL (kept for its older items):

Next round (GL) should start by re-reviewing round GK:

- GK's CI run (and GJ's Windows ctest job if it was still running; check
  whether macOS' `minera_cancellation_hooks_tests` abort recurs);
- GK1 on fresh banks: the classic performance view on XES uploads' log
  shape with other attribute types (lists and containers, ids, NaN
  floats, nulls, NaT; left on Minera's own path), dates with offsets under
  business hours, a custom timestamp or activity key, and the frequency
  decoration on the same logs; through `/api/discover-workflow` (a
  heuristics-only route, where an event without an activity in a
  single-event trace does not stop the mine) as well as `/api/mine`;
- the routes reading a classic heuristics net mined with business hours
  or a start key (token replay, the heuristics Petri conversion, the
  performance view) on CSV and XES frames;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1, and the
  anti- and multi-alignments of an upload without traces declared in GJ).

Previous list for round GK (kept for its older items):

Next round (GK) should start by re-reviewing round GJ:

- GJ's CI run;
- GJ1 on fresh banks: the classic heuristics variant with business hours
  or a start key on CSV and XES frame shapes (the performance
  decoration's spans), Heuristics++ with business hours over a CSV
  timestamp column with gaps on the log shape, offset (`+01:00`) start
  columns with business hours, and a start key naming the activity column
  (left on Minera's own path: the frame lists no dtype for it);
- the performance view (`/api/discover-graph` performance DFG and the
  heuristics performance decoration) on a CSV frame's Heuristics++ net;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round GJ (kept for its older items):

Next round (GJ) should start by re-reviewing round GI:

- GI's CI run (and GH's Windows ctest job if it was still running);
- GI1 on fresh banks: Heuristics++ on CSV frame shapes with a configured
  start-timestamp key or business hours (`check_plus_plus_csv_frame`
  leaves both on the older path), with text instance columns holding NA
  words ("NA", "null", "None") and zero-padded ints, and through
  `/api/discover-workflow` as well as `/api/mine`;
- the routes reading a CSV frame's Heuristics++ net (token replay, the
  heuristics Petri conversion, the performance view), whose net now pairs
  instances by the frame's dtype;
- the decomposed alignments' other variants and the anti-alignments on an
  upload without traces;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round GI (kept for its older items):

Next round (GI) should start by re-reviewing round GH:

- GH's CI run (and GG's Windows ctest job if it was still running);
- GH1 on fresh banks: Heuristics++ on CSV uploads with other columns
  (date and mixed instance columns, a lifecycle column of dates, unsigned
  ints beyond int64 in a start column, a renamed timestamp column beside
  `start_timestamp`) and on CSV uploads' frame shapes
  (`mining_input_shape=dataframe|lazyframe`), with `ct/hppcsv.py` and
  `ct/hppcsv2.py`;
- the other alignment consumers of an upload without traces (the
  decomposed alignments, the mine analysis' conformance summary, the
  `minera_export` CLI) and of imported nets, against pm4py's easy-soundness
  check;
- the Alpha model's footprints on bounded nets and the Inductive model's
  footprints on the log shape (`cmroutes4.py` with `MODEL=inductive`);
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round GH (kept for its older items):

Next round (GH) should start by re-reviewing round GG:

- GG's CI run (and GF's Windows job if it was still running);
- GG1 on fresh banks: `smapsw2.py` (both models) on uploads with more
  missing times, first-event gaps and events out of time order,
  `mccheck.py` and `pcheck.py` for the fits, and Heuristics++
  (`hpplog.py`, `sweepopt.sh`, `hppmin.py`) on CSV uploads, whose log
  shape now pairs by the frame's column dtype (unchecked against pm4py's
  `convert_to_event_log` of the CSV frame);
- the element usage of gfR/f8532 against f8533 (open since GF);
- the alignment-based routes on cached POWL and heuristics nets (the
  replay copy's names against alignment ties), and the Alpha model's log
  shape totals on the GG banks (GG's sweep ran past the round);
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round GG (kept for its older items):

Next round (GG) should start by re-reviewing round GF:

- GF's CI run (and GE's Windows job if it was still running);
- GF1 on fresh banks: `cmroutes4.py` on the log shape with every model,
  `ct/evprobe.py`, `cpcmp.py` and `ct/smapsw.py` with both models, and
  Heuristics++ (`ct/hpplog.py`, `sweepopt.sh`) with typed instances beside
  missing labels, bool and date instances, and NaN floats;
- the stochastic map on uploads whose events are out of time order
  (gfR/f8540, f8564: pm4py's weights differ from Minera's on both the
  file-order and the time-sorted log; find which order its replay and its
  service times read), and `/api/compare-petri` of gfR/f8532 against
  f8533 (log2 out of time order, deterministic in pm4py);
- the playout and Monte Carlo routes fitted from a cached POWL or
  heuristics net, the alignment-based routes on those nets (names do not
  break alignment ties the same way; check), and the footprints of the
  Alpha and Inductive models with the fixed harness;
- the log shape's whole-mine error for Heuristics++ with a missing time
  (Minera's core message where pm4py raises `KeyError: 'time:timestamp'`)
  and the empty-log fitness of the analysis (1 against pm4py's 0): decide
  whether to follow pm4py;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round GF (kept for its older items):

Next round (GF) should start by re-reviewing round GE:

- GE's CI run (and GD's Windows jobs if they were still running);
- GE1 on fresh banks: the POWL replay (`cmroutes2.py` with `MODEL=powl`
  on both frame shapes, `cmroutes3.py` on the log shape) on new banks with
  more loops, partial orders and silent routing, and Heuristics++ with
  `concept:instance` pairs beside lifecycle and time gaps;
- the default log shape of `/api/mine` on XES uploads against pm4py's
  EventLog: 177 uploads' trace counts differ from
  `read_xes(return_legacy_log_object=True)` (it keeps and replays empty
  traces) and 178 empty logs give fitness 1 where pm4py has none; find
  which reference the log shape follows (the legacy log or the converted
  frame), then the heuristics, Alpha and Inductive routing ties on that
  shape (GD's heuristics names apply on frames only);
- the decorations (frequency and performance) of the POWL and heuristics
  nets and the other routes replaying a cached net (`/api/compare-petri`
  with `miner=powl`, the stochastic map's log source): pm4py's replay
  ties go by its names, Minera's replay there by its own;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round GE (kept for its older items):

Next round (GE) should start by re-reviewing round GD:

- GD's CI run (and GC's Windows job if it was still running);
- GD1 on fresh banks: `cm/cmroutes2.py` with `MODEL=heuristic|alpha|
  inductive|powl` (with footprints for the heuristic model) on new
  `gen/gengc.py` banks, more cases with only missing labels (Alpha's lone
  NaN transition), several enabled silent transitions under Polars' None,
  and a probe of the Alpha name sort's operand order (if a rule beats the
  set order, model it);
- Heuristics++ on Polars with lifecycle gaps and a complete event without
  a timestamp (hdL/f7365, f7375): pm4py raises `TypeError: unsupported
  operand type(s) for -: 'NoneType' and 'NoneType'` (cases in first-row
  order, the subtraction before a later missing lifecycle), Minera the
  lifecycle `AttributeError`;
- the POWL replay's remaining totals (pandas gaL/f2306, gcB/f5213; Polars
  gaL/f2310, gaM/f2136, gaS/f2606, gaT/f2531, gbQ/f4708, gbQ/f4712,
  gcQ/f5716): deterministic in pm4py although its POWL net names
  transitions by uuid, so compare the nets first, then the routing;
- the other routes that replay a session model on XES frames (the
  workbench and `/api/conformance-*` routes reading the cached nets, the
  replay durations and decorations), against the same frame rules;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round GD (kept for its older items):

Next round (GD) should start by re-reviewing round GC:

- GC's CI run (and GB's Windows job if it was still running);
- GC1 on fresh banks: `optval.py` start-key and Heuristics++ sets,
  `cm/conv.py`, `wb/wfopt.py` with `gen/gengc.py` (more mixed start
  columns, int and bool keys beside gaps, starts after completions);
- token replay of the cached heuristics model on XES frames
  (`cm/cmroutes.py`, round GC's r1 sweep): pm4py replays the frame's
  variants (`pandas_utils.get_traces`: pandas groupby sorted without the
  null case, Polars group_by in first-row order with the null group),
  sorts the variants by count and tuple (a `TypeError` between a missing
  label and a name; Polars' None label replays a silent transition), and
  Minera's `/api/mine` replays `cached_model_log` instead (null-case
  traces counted: gbN/f4834 and gcB/f5249 report 8 traces where pm4py
  has 7; gcW/f5921 produced and consumed off by 4 on both shapes; the
  sweep needs a per-upload timeout: r1 stalled inside pm4py after gcW,
  likely on the reference footprints' reachability graph);
- `/api/discover-graph` footprints of the cached heuristics model: it
  answers "cached POWL model is unavailable" when POWL failed on the
  upload, where pm4py computes the heuristics net's footprints;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round GC (kept for its older items):

Next round (GC) should start by re-reviewing round GB:

- GB's CI run;
- GB1 on fresh banks: `optval.py` with the Heuristics++ and performance
  option sets (`hpp`, `hpp_thr`, `hpp_perf`, `hpp_min`, `hpp_min3`,
  `hpp_and`, `hperf`, `hperf_min`) and the start-key sets (`a_szzz`,
  `a_sst`, `a_sres`, `a_sts`, `tp_szzz`, `tp_sts`, `tp_sres`) on new
  `gen/gengb.py` banks (lifecycle columns with gaps, a start timestamp
  column, numeric and boolean start columns, mixed text and dates), and
  `wb/cskel.py` and `wb/wfval.py` (the workbench routes) with options
  (noise thresholds, Alpha+ `remove_unconnected`, heuristics thresholds);
- the routes that read the cached models of an XES session (footprints,
  token replay, model conversion on a heuristics net with a None label);
- the remaining `std::stable_sort` on Polars paths
  (`polars_variants_filter.cpp`, `polars_sequence_filters.cpp`,
  `polars_sort.hpp`'s Null-dtype keys, `polars_ltl_filter.cpp`,
  `temporal_lazy_reference.cpp`): probe pm4py's own determinism first,
  as GB did for the top selections;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round GB (kept for its older items):

Next round (GB) should start by re-reviewing round GA:

- GA's CI run (and FZ's if it was still running);
- GA1 on fresh banks: `famval.py` (FAMS alpha,heuristic,temporal_profile,
  log_skeleton), `optval.py` with `OPTS=aplus,aplus_ru` and `lsconf.py`
  (with and without `NOISE`) on new `gen/genfz.py` banks, more NaN/None
  repeats inside traces (length-one and length-two loops through the
  missing label), and the Python emulation `apm/emucheck.py` for Alpha+
  where set order decides;
- the families' options still on the older path for these frames: the
  Heuristics++ variant and the performance decoration (pm4py raises on
  most frames with gaps; ten classic performance results differ), Alpha's
  and the temporal profile's explicit start keys (`a_szzz`, `a_sst`,
  `a_sres`, `a_sts`, `tp_szzz`, `tp_sts`, `tp_sres` in `optval.py`: a text
  start column answers in Minera where pm4py raises; Polars messages
  differ);
- the workbench routes that read the log skeleton, heuristics and Alpha
  models of an XES session (same discovery, not swept);
- the remaining `std::stable_sort` on Polars paths (`polars_case_filter.cpp`
  kCount, `polars_attributes_filter.cpp` top activities,
  `polars_variants_filter.cpp`, `polars_sequence_filters.cpp`,
  `polars_sort.hpp`, `polars_ltl_filter.cpp`, `temporal_lazy_reference.cpp`),
  the two items carried from FU;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round GA (kept for its older items):

Next round (GA) should start by re-reviewing round FZ:

- FZ's CI run;
- FZ1 on fresh banks: `famval.py` with `FAMS=alpha,heuristic,temporal_profile,log_skeleton`
  on new `gen/genfz.py` banks (other gap mixes, longer traces, more
  activities, tied timestamps), and with options: Alpha+ (`short_loops`,
  still on the older path for frames), the heuristics' thresholds,
  `connect_all`, the Heuristics++ variant and the performance decoration
  on these frames (all still on the older path), the temporal profile's
  `zeta` and start keys;
- log skeleton conformance with a Polars null activity (the model now
  holds `None`; conformance against it is unswept), and the workbench
  routes that read the log skeleton, heuristics and Alpha models of an XES
  session (same discovery, not swept);
- carried from FY's list: footprints on CSV uploads (frame order and
  header-only files) and a check that the plan's `DF` line holds for CSV
  frames and for the log shape;
- the remaining `std::stable_sort` on Polars paths (`polars_case_filter.cpp`
  kCount, `polars_attributes_filter.cpp` top activities,
  `polars_variants_filter.cpp`, `polars_sequence_filters.cpp`,
  `polars_sort.hpp`, `polars_ltl_filter.cpp`, `temporal_lazy_reference.cpp`),
  the two items carried from FU;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round FZ (kept for its older items):

Next round (FZ) should start by re-reviewing round FY:

- FY's CI run (and FX's Windows job);
- FY1-1 on fresh banks: `famval.py` with `SHAPE` and `FAMS` on new
  `gen/genfp.py` banks with mixed gap kinds and higher rates, larger
  traces, and options (`noise_threshold`, the transition system's window
  and direction, the Inductive variants); the TS, model and log skeleton
  workbench routes on XES sessions (same code paths, not swept);
- left from FY: the log skeleton on a Polars frame with a null activity
  (pm4py answers with `None` among the activities and in `activ_freq`;
  needs a null label in the model and its JSON), and the families not
  yet aligned on these frames: Alpha (missing columns as pandas
  `KeyError`s and Polars `with_columns`/`sort` plans, NaN or None counted
  as an activity, the Polars null case group), the heuristics (pm4py
  converts the frame to an EventLog: a NaN case is one trace, an unnamed
  event raises `TypeError: 'float' object is not subscriptable`, or
  `'NoneType'` on Polars) and the temporal profile (missing columns);
  `famval.py` already compares all seven families;
- carried from FX's list: footprints on CSV uploads (frame order and
  header-only files) and a check that the plan's `DF` line holds for CSV
  frames and for the log shape;
- the remaining `std::stable_sort` on Polars paths (`polars_case_filter.cpp`
  kCount, `polars_attributes_filter.cpp` top activities,
  `polars_variants_filter.cpp`, `polars_sequence_filters.cpp`,
  `polars_sort.hpp`, `polars_ltl_filter.cpp`, `temporal_lazy_reference.cpp`),
  the two items carried from FU;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round FY (kept for its older items):

Next round (FY) should start by re-reviewing round FX:

- FX's CI run (and FW's Windows job);
- FX1-1 on fresh banks: `fpkeys.py` with new `SEED`s on `gen/genrk.py`
  banks (random attribute names, collisions, start keys, both sort modes),
  `fpcmp.py` with `OPTS` on `gen/genfp.py` banks with higher gap rates,
  CSV uploads (the frame order and header-only files), and a check that
  the plan's `DF` line holds for CSV frames and for the log shape;
- FX1-2: `driftsum.py` on new banks (unnamed traces at the start, middle
  and end, both shapes of the upload);
- the new area: the `/api/mine` families on the `dataframe` and
  `lazyframe` shapes of XES uploads with gaps against pm4py's algorithm
  applies (`famgap.py` with `SHAPE`; banks gC, gA, gT from `gen/genfp.py`
  with one gap kind each). Seen on `dataframe`: the transition system
  refuses rows without a case id or an activity where pm4py answers;
  Inductive and POWL refuse rows without a timestamp where pm4py answers,
  and name a missing case id or activity where pm4py raises np.unique's
  `TypeError` (`project_univariate`); the heuristics answer an unnamed
  event where pm4py raises `TypeError: 'float' object is not
  subscriptable`; the log skeleton names a missing activity where pm4py
  raises `KeyError: nan`; on a log without events or with no trace named
  the frame lacks those columns (`KeyError`s of various shapes) while
  Minera answers or names its own error. Round BM's declaration compared
  the `discover_*` wrappers; the families mirror the algorithm applies
  (round FU), so compare there, value by value where both answer;
- the remaining `std::stable_sort` on Polars paths (`polars_case_filter.cpp`
  kCount, `polars_attributes_filter.cpp` top activities,
  `polars_variants_filter.cpp`, `polars_sequence_filters.cpp`,
  `polars_sort.hpp`, `polars_ltl_filter.cpp`, `temporal_lazy_reference.cpp`),
  the two items carried from FU;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round FX (kept for its older items):

Next round (FX) should start by re-reviewing round FW:

- FW's CI run (and FV's: its macOS job was cancelled during the build with
  no compile error in the log; a later run builds macOS again);
- FW1-1 and FW1-2 on fresh banks: `bhsweep.py` with `START`, sub-second
  times, larger `NC`, custom business-hour slots and an own `@@index`, the
  conformance with zeta near the deviations, `bhfull.py`/`bhab.py` for the
  messages (one Polars thread); `dcmsg2.py` and `dc3.py ALG=1` on new
  seeds on both shapes (`gen/gendcbad.py`, round-fv `dc2.py` options);
- found at the end of FW: a CSV upload without rows (a header only) on
  the `lazyframe` shape, where pm4py's `format_dataframe` turns every
  other object column into a date, so the Polars Declare check refuses a
  case column read from an alias header (`the case ID column should be of
  type string.`) while Minera answers with an empty model; check the other
  frame consumers of header-only CSVs the same way;
- the column rule of FW1-2 elsewhere: the other users of
  `check_dataframe_columns` on XES tables (`token_replay_event_data.cpp`,
  `concept_drift_table.cpp`) may count a first-class column with no value
  that read_xes's frame does not have;
- the remaining `std::stable_sort` on Polars paths, the two items carried
  from FU (the Polars frequency triples, library only, and
  `dfg_dataframe.cpp`'s Null-dtype key rule);
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round FW (kept for its older items):

Next round (FW) should start by re-reviewing round FV:

- FV's CI run;
- FV1-1 to FV1-5 on fresh banks: `lazy2.py` with larger `NC`/`NE` and
  `DUP` (POWL and variants on both frame shapes), `sdpowl.py`, `dc2.py`
  with `IDX`, `TS` and `NULLS` on new seeds, `tplazy.py`, `tpconf.py` and
  `tpcsv.py` and `tpstart.py` on larger frames (several thousand pairs,
  where Polars' joins run in parallel), a start column on a CSV upload and
  zeta near the deviations;
- FV's leftover: decide whether the Declare routes mirror the selected
  timestamp column check of `pm4py.discover_declare` and
  `conformance_declare` on Polars frames (`round-fv/probe/dcwrap.py`; the
  harness `dc2.py` compares with the wrappers; a fix needs the timestamp
  selector in the Declare options, which the fixtures serialize);
- new areas: the temporal profile with business hours on a Polars frame
  (not swept in FV), the Declare `dataframe` shape against pm4py's frame
  check (round BM declared it answers; decide whether to align it with
  the `lazyframe` shape), other Polars inputs that join or group after a
  sort (the remaining `std::stable_sort` on Polars paths), and the two
  items carried from FU (the Polars frequency triples, library only, and
  `dfg_dataframe.cpp`'s Null-dtype key rule);
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round FV (kept for its older items):

Next round (FV) should start by re-reviewing round FU:

- FU's CI run;
- FU1-1 to FU1-5 on fresh banks: `tie/tiesweep.py` with `NC`/`NE` large
  enough for frames over 20 rows (the Polars spectrum variants), `AMB=1`
  and `DUP`; `wf/wfcsv.py` with `FMT=mixed`/`off` and larger `NC`/`NE`;
  `am/alphamine.py` on new seeds; `mut/mutsweep.py` with more boolean and
  mixed columns; `am/lazyfam.py` with larger frames; `dc/declxes.py` with
  `DUP` and both shapes;
- new areas: the other Polars inputs that group cases after a sort (POWL,
  footprints, variants and minimum self distance on
  `mining_input_shape=lazyframe` with XES ties); Declare on a Polars
  frame that carries its own `@@index` (pm4py sorts by it instead of a row
  count), with null case ids (one group, first) or null activities, and
  with a timestamp selector other than `time:timestamp` (pm4py still sorts
  by `time:timestamp`); the Polars frequency triples (library only) and
  `dfg_dataframe.cpp`'s Null-dtype key rule (an all-null start column),
  both carried from FU;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round FU (kept for its older items):

Next round (FU) should start by re-reviewing round FT:

- FT's CI run;
- FT1-1 to FT1-7 on fresh banks: `efg/efgns.py` with `DENSE=1` and
  `XES=1` on new seeds (also completion keys with a CSV's own header),
  `corr/corrss.py` with exact time matching on larger groups (the
  `sys.maxsize` absorption) and sample sizes, `corr/etm/etmmodel.py` on
  wider matrices, `po/dfgpox.py` with `NC`/`NE` up to a few thousand rows
  (the pdqsort's 2,000-row recursion and 128-row blocks) and `MISS`,
  `dfg/dfgstart.py` and `po/dfgpo2.py` with `NC`/`NE`,
  `heu/mut/mutprobe.py` with mixed columns, boolean columns and blanks;
- new areas: the Polars frequency triples (`discover_triples_table`
  still sorts stably; library only, no route reaches it) and the other
  Polars sorts by case and timestamp outside the filters (any
  `std::stable_sort` on a Polars path; `dfg_dataframe.cpp`'s unstable
  sort does not yet apply `sort_by_keys`' rule for a Null-dtype key, an
  all-null start column), the
  other workflow input shapes with CSV text columns (alpha, classic
  heuristics on `dataframe`/`lazyframe`), the temporal profile and
  performance spectrum on XES uploads with ties, and any other route
  whose reference reaches SciPy's `linear_sum_assignment` (spy for the
  call while sweeping, as corr/etm/spy2.py did);
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round FT (kept for its older items):

Next round (FT) should start by re-reviewing round FS:

- FS's CI run;
- FS1-1 to FS1-5 on fresh banks: `heu/heuns.py` with `DENSE=1` and new
  seeds (also lifecycle logs without a start key, and XES uploads),
  `corr/corrst.py` with `XES=1` and new seeds (also one-activity logs and
  supplied completion keys), `corr/psdsweep.py` for classic and
  classic_split (also `correlation_sample_size` below the event count, so
  several chunks), `fp/fpst.py` with `XES=1` and the alpha model (which
  reads its own log), `po/dfgpo.py` with `TSNS`/`STNS` and
  `dfg_keep_first_following`, `dfg_strict`;
- new areas, carried from FS: the other routes that read a timedelta's
  `total_seconds()` on an upload's frame (the temporal profile, the
  performance spectrum, the eventually-follows performance), the plusplus
  heuristics through `apply_heu_pandas` with lifecycle columns; also the
  Polars graph DFG and concurrency on XES uploads (only CSV swept), the
  streaming correlation and footprints routes against the same keys;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round FS (kept for its older items):

Next round (FS) should start by re-reviewing round FR:

- FR's CI run (and FQ's Windows job);
- FR1-1 to FR1-5 on fresh banks: `bt/casekeys.py` and `bt/casest.py` on
  new seeds (also XES uploads and case columns of dates with offsets),
  `heu/heustart.py` with `SKS`, `TSNS`, `STNS` and `NSOFF=1` on new
  seeds, `soj/soj.py`, `dfg/dfgstart.py` with `TSNS=0.5`; the
  whole-seconds-plus-microseconds wait rule on XES uploads with a sweep
  (only probed so far, `ut/heu_xes.py`) and with business hours;
- new areas: other routes that read a timedelta's `total_seconds()` on
  an upload's frame (pm4py's Timedelta gives whole seconds plus
  microseconds / 1e6 and drops nanoseconds): the footprints and
  correlation miner start keys, the temporal profile, the performance
  spectrum, the eventually-follows performance; the decision mining and
  data Petri net guards on date attributes; the OCEL conversions of a
  CSV log (`/api/log-convert`); the plusplus eventually-follows and
  concurrency comparisons between sub-microsecond instants;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round FR (kept for its older items):

Next round (FR) should start by re-reviewing round FQ:

- FQ's CI run;
- FQ1-1 to FQ1-3 on fresh banks: `nsdates.py --only features` on new
  seeds (also with case date columns only, blanks, and XES uploads, which
  keep microseconds), `llmns.py` on new seeds (log attributes with
  nanosecond `case:` and event columns, variants with ties at the
  microsecond);
- new areas: other consumers of a CSV log's datetime64[ns] cells: the
  table functions that read a `case:` date column's unit (the CSV frame
  table types only event date columns by their nanosecond cells), profile
  clustering and the other `log_to_features` callers with date string
  features on nanosecond CSV logs, the decision mining and data Petri net
  guards on date attributes, the OCEL conversions of a CSV log; the LLM
  descriptions on XES uploads with more fraction digits (`llmns.py --xes`
  now generates them); interleavings and link analysis with odd relation
  files and nanosecond times together (`il/il3.py` with `ODD=1`);
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round FQ (kept for its older items):

Next round (FQ) should start by re-reviewing round FP:

- FP's CI run;
- FP1-1 to FP1-4 on fresh banks: the frame path with nanosecond times
  (`rpframe.py` with `NEAR=1` and new seeds, `rpframex.py` for XES), the
  log variant with lifecycle columns (`rlwl.py` on new seeds, then
  `org/rpsweep.py` with `PLAIN=1`, `RAWDT=1` for XES; also logs whose
  first sorted event has `start_timestamp`), root cause on nanosecond CSV
  logs (`rcns.py` on new seeds, also with the multiplier, numeric date
  columns and the unknown-activity form);
- new areas: the other routes that print a CSV log's dates through
  `str(Timestamp)` (the attribute value lists and filters, the features
  and one-hot encodings, the case attributes in the trace explorer) on
  nanosecond CSV logs; other pm4py functions that convert a log to
  intervals by lifecycle (the interval features, the temporal profile and
  the cycle time with lifecycles) on raw logs with lifecycle columns;
  local diagnostics on raw logs with a group column; the SNA pandas
  variants on CSV uploads with offsets and nanoseconds;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Previous list for round FP (kept for its older items):

Next round (FP) should start by re-reviewing round FO:

- FO's CI run;
- FO1-1 to FO1-3 on fresh raw-log banks (`org/orgna.py`,
  `org/rpsweep.py` with `PLAIN=1`, `rlw.py --only passed`, `RAWDT=1` for
  XES), also with lifecycle columns;
- the frame path's resource-profile durations with nanosecond times
  (`org/rpframe.py`: `average_duration_activity` subtracts doubles,
  `average_case_duration` means an ulp off);
- new areas: the root cause and decision workbenches on nanosecond CSV
  logs (not reached in FO); the log variant's interval metrics on logs
  with lifecycle transitions; the other organizational routes (roles,
  SNA, local diagnostics) on raw logs with offsets;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Round FP's list, taken in FP: FO's CI (six green); FO1-1 to FO1-3 on fresh CSV and XES raw-log banks, also
with lifecycle columns (0 outside FP1-2 and FP1-4); the frame path's
durations with nanosecond times (fixed as FP1-1); root cause on
nanosecond CSV logs (fixed as FP1-3; XES 0) and the decision workbench
(native, no pm4py route); the log variant with lifecycles (fixed as
FP1-2); SNA and roles on raw logs (0 outside the declared items); local
diagnostics not swept (the banks have no group column). Carried to FQ:
EV1-1, the SQLite leftovers, the new Polars dtypes, DW1, the other
declared items, the rest of the CU list and the open choices.

Previous list for round FO (kept for its older items):

Next round (FO) should start by re-reviewing round FN:

- FN's CI run;
- FN1-1 to FN1-7 on fresh raw-log banks (other seeds and slot modes,
  `rlw.py`, `rlt.py`, `rlp.py` with `RAWDT=1` for XES) and on fresh
  Polars DFG uploads (`rlpd.py`, also with `dfg_measure=both`, keep once
  per case and a window above one);
- new areas: the heuristics miner and the OC-DFG with non-default
  business-hour slots (the merge rule of FN1-5 reached the OC-DFG edge
  performance and the network analysis without a sweep of their own);
  the other raw-log workbenches (organizational, root cause, decision)
  on nanosecond CSV logs; the Polars batches and minimum self-distance
  on uploads with offsets;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Round FN's list, taken in FN: FM's CI and FL's Windows job (green); the raw-log workbenches on CSV and XES logs
with offsets and more than six fraction digits (the time analysis,
temporal and timing workbenches: FN1-1 to FN1-6), non-default slots
through the same consumers (FN1-5), the Polars variants with business
hours (FN1-7 on the DFG; the case filter raises like pm4py; the other
Polars statistics are not exposed). Carried to FO: EV1-1, the SQLite
leftovers, the new Polars dtypes, DW1, the other declared items, the
rest of the CU list and the open choices.

Previous list for round FN (kept for its older items):

Next round (FN) should start by re-reviewing round FM:

- FM's CI run (a docs-only commit);
- new areas, since FM found no new class: the raw-log workbenches (the
  original_* routes and the JSON event-log transport) against pm4py's
  EventLog variants on logs read from XES and CSV with offsets and with
  more than six fraction digits (case statistics, DFG performance,
  temporal profile and conformance, heuristics miner, with and without
  business hours); non-default business-hour slots (overlapping, adjacent
  within a second, fractional, crossing the week's end) through the same
  consumers; the Polars variants with business hours (case filter,
  passed time, service time, case statistics, eventually-follows, the
  Polars DFG) on CSV uploads with offsets and nanoseconds;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Round FM's list, taken in FM: FL's CI (five green, Windows
still running); FL2-1 and FL2-2 on the
network analysis, the time analysis routes on XES offsets, the pandas
case filter, the DFG performance and the temporal conformance (0; the
streaming temporal conformance and the replay duration diagnostics have
no business hours); FL2-4 on OCEL 2.0 JSON, XML and SQLite, OCEL 1.0 and
CSV banks listed out of id order through the imports, the route sweeps
and the chains (0 outside the declared items). Carried to FN: EV1-1, the
SQLite leftovers, the new Polars dtypes, DW1, the other declared items,
the rest of the CU list and the open choices.

Previous list for round FM (kept for its older items):

Next round (FM) should start by re-reviewing round FL:

- FL's CI run;
- FL2-1 and FL2-2 on the other business-hours consumers that read an
  upload's times: the temporal workbench and the time analysis workbench
  routes, the streaming temporal profile and conformance, the table and
  pandas case filters with business hours, the organizational workbench
  and the token replay durations, on CSV uploads with offsets and with
  more than six fraction digits, and on XES uploads;
- FL2-4 on the other outputs that follow pm4py's objects table order: the
  OCEL 2.0 JSON, XML and SQLite banks with the objects listed out of id
  order (the o-generators write them sorted), through the object
  statistics, summaries, graphs, the lifecycle enrichment and the split's
  central objects after filters and enrichments;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Round FL's list, taken in FL: FK's CI (six green); FK1-1 on the
enrichments, the type matching and the connected components and
ancestors splits with the nanosecond row in another component (0 outside
FL2-4), a SQLite log whose nanosecond time sits on a repeated id (the
unit agrees; the duplicated event is AP's declared difference) and a
SQLite file written by Minera (0); FK1-2 on the OC-DFG's own business
hours (0) and the traditional routes (the frame paths 0; the temporal
profile, its conformance, the heuristics miner and the EventLog
statistics fixed as FL2-1 to FL2-3). Carried to FM: EV1-1, the SQLite
leftovers, the new Polars dtypes, DW1, the other declared items, the rest
of the CU list and the open choices.

Previous list for round FL (kept for its older items):

Next round (FL) should start by re-reviewing round FK:

- FK's CI run;
- FK1-1 on the other places that keep or rebuild a log's frames: the
  enrichments (O2O, lifecycle), the type matching, the connected
  components and ancestor splits with the nanosecond row in another
  component, a SQLite log whose nanosecond time sits on a repeated id,
  and an OCEL 2.0 SQLite file written by Minera from such a log;
- FK1-2 on the other business-hours consumers on nanosecond inputs: the
  OC-DFG's own business hours (0 here), and the traditional event log
  routes that convert epoch doubles with llround (case statistics, case
  arrival, the DataFrame DFG, the heuristics miner) on CSV or XES logs
  whose times have more than six fraction digits;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Round FK's list, taken in FK: FJ's CI (six green); FJ1-1 on the chains'
exact filters after splits and windows (0), an object column holding
naive and aware datetimes next to NaT (0) and declared attribute types
that change a column's kind (0); FJ1-2 after the pre steps, on SQLite's
all-NULL columns and on OCEL 1.0 XML dates (0); FJ1-3 on the other time
units (a naive CSV column without any time 0, the writers 0, OCEL 1.0
times read at microseconds 0; an aware CSV or SQLite column at
nanoseconds through the chains fixed as FK1-1). Carried to FL: EV1-1,
the SQLite leftovers, the new Polars dtypes, DW1, the other declared
items, the rest of the CU list and the open choices.

Previous list for round FK (kept for its older items):

Next round (FK) should start by re-reviewing round FJ:

- FJ's CI run;
- FJ1-1 on the other places that hand pandas a value list for a datetime
  column: the chains' exact filters after splits and windows (`onx` with
  xe3's lists), an object column holding naive and aware datetimes next
  to NaT, and the exact filters after a type declaration changed the
  column's kind;
- FJ1-2 on the non-exact filters after the pre steps (`PRE=evnone`,
  `otnone`, `winnone`, `actnone`), on SQLite all-NULL columns (an object
  column) and on OCEL 1.0 XML date elements kept as texts;
- FJ1-3 on the other time units: a naive CSV column without any time
  (unit s), an aware CSV column at nanoseconds through the chains and the
  writers, and OCEL 1.0 times with nanoseconds;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Round FJ's list, taken in FJ: FI's CI (six green); FI1-1 on the other
OC-DFG selectors holding NaT (the activity and event id both
`ocel:timestamp` on naive CSV times, the timing with a NaT event id key,
every events column as the event id: 0; an objects column as the object
type key raises pm4py's KeyError in both); FI1-2 and FI1-3 on the other
columns added in the type matching (every pair of relations columns,
temporary columns named like relations columns, OCEL 1.0 logs without
relations: 0 outside 'nocol'); FI1-4 on the other isin consumers (the
exact filters' other value lists fixed as FJ1-1 and FJ1-3, the non-exact
filters as FJ1-2; the flattening and the features take no value list).
Carried to FK: EV1-1, the SQLite leftovers, the new Polars dtypes, DW1,
the other declared items, the rest of the CU list and the open choices.

Previous list for round FJ (kept for its older items):

Next round (FJ) should start by re-reviewing round FI:

- FI's CI run;
- FI1-1 on the other OC-DFG selectors holding NaT: the activity and
  event id both `ocel:timestamp` on naive CSV times (the label None), the
  OC-DFG timing with an event id key holding NaT, and the object type key
  on a datetime64 object column with a missing cell;
- FI1-2 and FI1-3 on the other columns added in the type matching: the
  event id as the type key next to NaT, the qualifier of a CSV log with
  some relations without objects, a temporary column named like a
  relation column on a degenerate frame, and an OCEL 1.0 log without
  relations;
- FI1-4 on the other isin consumers: the non-exact attribute filters'
  value lists, the object attribute exact filter on a datetime64 object
  column with an aware value of another offset, the flattening's and the
  features' value lists if any, and lists mixing NaT, NaN and None with
  one datetime;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Round FI's list, taken in FI: FH's CI (six green); FH1-1 on the other NaT
keys (the OC-DFG's own event id fixed as FI1-1, the type matching's time
key fixed as FI1-2, with FI1-3 next to it; the flattening 0; the OC-DFG with
the time as event id on o92's SQLite and XML and on c93 and c94: 0); FH1-2 on the other
column-type consumers (the exact filters' mixed null lists fixed as FI1-4;
the chains' window after splits 0; the features' families on null-only
columns 0); FH1-3 (aware Timestamps with nanoseconds, booleans next to 0
and 1, OCEL 1.0 XML dates: 0 outside the seed-dependent set order);
FH1-4 (the related objects' families and `filter_per_type` with one
failing type: 0). Carried to FJ: EV1-1, the SQLite leftovers, the new
Polars dtypes, DW1, the other declared items, the rest of the CU list and
the open choices.

Previous list for round FI (kept for its older items):

Next round (FI) should start by re-reviewing round FH:

- FH's CI run;
- FH1-1 on the other places that group by a key holding NaT: the OC-DFG's
  own `ocdfg_event_id_attribute=ocel:timestamp` on CSV logs with empty
  times, the flattening and the type matching with a time column as a
  key, and the SQLite and XML readers' missing times if any reach a key;
- FH1-2 on the other consumers of a frame's column type after dropped
  rows: the exact filters' other value lists (NaN and NaT next to None),
  the event attribute window of the non-exact filter, the features'
  timestamp families on a null-only column, and the chains' splits
  (connected components and ancestors) before the window;
- FH1-3 on the other seed-free hashes in a Python set: the events'
  string family (no concatenation there, but the column order), aware
  Timestamps with nanoseconds, booleans next to 0 and 1, and the OCEL 1.0
  XML reader's dates as object values;
- FH1-4 on the other families pm4py runs inside another: the events'
  related objects (the objects algorithm inside the events one) with the
  object attribute lists and an event the objects' families fail on, and
  `filter_per_type` with a failing family on one type only;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Round FH's list, taken in FH: FG's CI (six green), FG1-1 on the other readers of the attribute lists
(fat2 0 outside the CSV declared features; related events fixed as
FH1-4; the native names style is native-only; 0 seed-dependent string
family outcomes in 169 columns, but the aware datetime's place fixed as
FH1-3), FG1-2 after filters and splits (onchain 0 outside the hash-seed
class; the exact window fixed as FH1-2) and on the OCEL 1.0 JSON reader,
SQLite's unrelated events and three-entry transports (0), FG1-3 on the
id keys (the event id key's NaT fixed as FH1-1; the object type key
agrees). Carried to FI: EV1-1, the SQLite leftovers, the new Polars
dtypes, DW1, the other declared items, the rest of the CU list and the
open choices.

Previous list for round FH (kept for its older items):

Next round (FH) should start by re-reviewing round FG:

- FG's CI run;
- FG1-1 on the other places that read the attribute lists: the features'
  related-objects family of the events and the objects' related events
  with attribute lists, `filter_per_type`, the native names style next to
  the compatibility one, OCEL CSV logs with the declared CSV features
  (repeated event ids), and a text pushing a number aside in the objects'
  set (declared, but worth a count);
- FG1-2 on the other routes that read a frame after rows were dropped: a
  filter or split dropping the rows that hold a null-only column's nulls
  and then the OC-DFG timing or an exact null filter, the OCEL 1.0 JSON
  reader's dropped events and objects with nulls, the SQLite reader's
  unrelated events, and transports written before FG (three entries);
- FG1-3 on the edge metrics' event id key of a datetime64 column (both
  raise KeyError on a missing column; a present one with a missing cell
  is untested) and on the object type key of an object time column with a
  missing value next to respelled instants;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Round FG's list, taken in FG: FF's CI (six green, macOS built), FF1-1 on
the other routes (the flattening, exact filters, routes, edge metrics 0
outside the declared items; the features drop nulls; the dropped rows'
nulls fixed as FG1-2), FF1-2 on the other time keys (the flattening and
edge metrics agree; the type matching and the split never group by a
time), FF1-3 on CSV NaT times (ocd and fl 0 outside the hash-seed pair).
Carried to FH: EV1-1, the SQLite leftovers, the new Polars dtypes, DW1,
the other declared items, the rest of the CU list and the open choices.

Previous list for round FG (kept for its older items):

Next round (FG) should start by re-reviewing round FF:

- FF's CI run (and the macOS job FE's run lost to the checkout timeout);
- FF1-1 on the other routes that read a JSON log's null cells by the
  column's dtype (the edge metrics' and the flattening's labels of a null
  cell, the features' and statistics' null cells, the OCEL 1.0 XML
  reader's empty values next to missing ones), with banks whose nulls
  share a column with missing cells only;
- FF1-2 on the other keys that group by a time (the flattening's
  `case:concept:name` of a time object column, the type matching's
  object type key, the split's object type, the OC-DFG's attribute
  activity when a datetime64 column is selected through the edge
  metrics), with hand-made logs spelling one instant twice (the random
  banks never do);
- FF1-3 on the OC-DFG's start and end activities and the flattening with
  a NaT time on CSV logs with and without offsets;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Round FF's list, taken in FF: FE's CI (five green, macOS lost to a
checkout timeout), FE1-1 on the CLI and chain results (0 outside the
declared CSV features; the frontend's form posts to the same route),
FE1-2 on the other readers' nulls (FF1-1) and the None-vs-NaN routes
(0), FE1-3 on the edge metrics (FF1-2, FF1-3) and CSV offset logs through
chains (q31 cswu 0 of 13,510). Carried to FG: EV1-1, the SQLite
leftovers, the new Polars dtypes, DW1, the other declared items, the rest
of the CU list and the open choices.

Previous list for round FF (kept for its older items):

Next round (FF) should start by re-reviewing round FE:

- FE's CI run (FD's run 44827214734 finished green in all six jobs);
- FE1-1 on the other places that flatten (the CLI's
  `--flatten-object-type` with both selectors, the flattening of filter
  and chain results, the frontend's flatten form), with selectors naming
  object columns of a CSV objects file and timestamps as `case:` cells;
- FE1-2 on the other readers' null cells (OCEL 2.0 JSON `null` values,
  OCEL 2.0 XML elements without text, OCEL 1.0 JSON nulls) and on the
  routes that read None and NaN apart beyond the exact filters (the
  non-exact filters on SQLite, the attribute statistics and features, the
  CSV writer, the event type tables' NULL cells);
- FE1-3 on the other labels taken from the event time of a CSV log with an
  offset (the edge metrics and the event-object graph with ocel:timestamp
  as the key, compared as raw text rather than through the harness's UTC
  normaliser) and such logs through chains;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Round FE's list, taken in FE: FD's CI (all six jobs green),
FD1-1 on the other attribute-timed routes (the edge metrics' performance
and the OC-DFG of chain results: only declared classes) and the null
activity label on the flattening (FE1-1, FE1-3), FD1-2 on the non-exact
filters (xn 0 on o81) and the carried SQLite NULL cells (FE1-2), FD1-3
with the temporary column on unheld names (0 of 21,168 chains on o81's
JSON, the routes after them only the declared download fractions). Carried to FF: EV1-1, the
SQLite leftovers, the new Polars dtypes, DW1, the other declared items,
the rest of the CU list and the open choices.

Previous list for round FE (kept for its older items):

Next round (FE) should start by re-reviewing round FD:

- FD's CI run (FC's run 44803654768 finished green in all six jobs);
- FD1-1 on the other routes that time pairs by an attribute column (the
  edge metrics' performance, the OC-DFG of filter and chain results) and
  with object types whose first couples tie on one event, and the null
  activity label on other routes that take ocel:timestamp as a key (the
  edge metrics, the event-object graph, the flattening);
- FD1-2 on the other routes that pass values to isin or compare them (the
  non-exact attribute filters, the object filters with levels, the
  windows), and the carried SQLite item: pm4py's SQLite read has None where
  a cell is NULL (object columns), native leaves the attribute out (xe on
  o81's SQLite 106 of 2,187, the OC-DFG time cells), which touches the
  transports, the writers and the fixtures;
- FD1-3 after the other OCEL constructions (the sampling-free routes that
  copy the frames: the flattening, the OC-DFG, the object graphs, the
  downloads of chains of three steps) and with the temporary column on
  attribute columns the relations frame does not hold;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Round FD's list, taken in FD: FC's CI, FC1-1 with the temporary column
and the separator on typed relations columns (atm2 0 on j92, s91 and o81;
the carried temporary-column item: FD1-3), FC1-2 on the other routes with
selectors (the OC-DFG discovery's own: FD1-1; the statistics route takes
none; repeated event id keys follow groupby first), FC1-3 on CSV object
tables and chains (xe 0 on q28's object attribute filters; the chains in
`q/csw.py` SET=chain on q28: only the declared hash-seed object order of
the files without an objects file), FC1-4 on the other empty-frame
comparisons (xe after the event, object type and activity filters that
keep nothing: 0; the null matching is FD1-2). Carried to FE: EV1-1, the
SQLite leftovers (now with NULL cells), the new Polars dtypes, DW1, the
other declared items, the rest of the CU list and the open choices.

Previous list for round FD (kept for its older items):

Next round (FD) should start by re-reviewing round FC:

- FC's CI run (FB's run 44787379225 finished green in all six jobs);
- FC1-1 with the temporary column and the separator on typed relations
  columns (qualifiers of SQLite and OCEL 2.0 XML reads), and the carried
  temporary-column item: pm4py's result lacks a relations column the
  temporary column names; compare the routes after such a result (OC-DFG,
  flattening, downloads, a second type matching);
- FC1-2 on the other OC-DFG routes that take selectors (the statistics
  route's edge associations, performance with business hours) and with
  non-unique event id keys from attributes (groupby first skips a missing
  cell: native rejects it, declared);
- FC1-3 on CSV object tables (str columns there too: object attribute
  filters, the object side's null-only columns) and on CSV results posted
  through chains (the mark survives the transport);
- FC1-4 on the other empty-frame comparisons (attribute filters on dtypes
  without rows, the timestamp filter's own column after a filter that
  keeps nothing);
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Round FC's list, taken in FC: FB's CI, FB1-1 on the literal-name routes
(edge metrics: FC1-2; exact filters' negations and levels, flattening and
OC-DFG of exact results: 0 through csw3 and the key chains; OCEL 2.0 inputs
with prefixed names: o81, o82), FB1-2 on other readers, object-side keys and
dropped rows (FC1-3, FC1-4; object keys KeyError on both), FB1-3 on objects
with changes and CSV read back (dsw 0), FB1-4 and FB1-5 on multi-type logs
and change-combining routes (o81, o82: 0). Also FC1-1 from the literal
reading of type matching. Carried to FD: EV1-1, the SQLite leftovers, the
new Polars dtypes, DW1, the other declared items, the rest of the CU list
and the open choices.

Previous list for round FC (kept for its older items):

Next round (FC) should start by re-reviewing round FB:

- FB's CI run (FA's run 44765194634 finished green in all six jobs);
- FB1-1 on the other routes that read their input with literal names
  (edge metrics, the exact filters' negations and levels, the flattening
  and OC-DFG of an exact filter result) and on OCEL 2.0 inputs with
  "ocel:"-prefixed attribute names;
- FB1-2 on other readers' column kinds (SQLite TEXT and INTEGER cells
  under TIMESTAMP, CSV, OCEL 1.0 XML), on object-side keys, and on frames
  with dropped rows (a null-only column after a filter: the window treats
  it as float64 when the hint shows dropped rows, a guess);
- FB1-3 on the objects table with changes and on CSV imports read back;
- FB1-4 with several object types and change-only fields of other types,
  and FB1-5 on other routes that concatenate the objects and changes
  frames (the flattening, the object change routes) and on XML and SQLite
  inputs;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list, the open choices, EV1-1).

Round FB's list, taken in FB: FA's CI, FA1-5's mark on more banks (c63,
c65, w65s, w65x: FB1-1), FA1-3's rule on the event side and on OCEL 2.0
inputs (o71, o72: FB1-2, FB1-3, FB1-5), three-step chains and CSV through
crsw (q24, q25, w65, o72: FB1-4), the sampling routes (no native route).
Carried to FC: EV1-1, the SQLite leftovers, the new Polars dtypes, DW1,
the other declared items, the rest of the CU list and the open choices.

Previous list for round FB (kept for its older items):

Next round (FB) should start by re-reviewing round FA:

- FA's CI run;
- FA1-5's change mark on more banks and routes (OCEL 1.0 XML changes
  under date keys through the flattening, the OC-DFG, the event-object
  graph and the object change routes; other c2gen seeds; changes of
  `ocel:`-prefixed keys);
- FA1-3's rule on the event side and on OCEL 2.0 inputs with date
  declarations (texts the OCEL 2.0 readers do not convert, numbers under a
  date type), and the exact window and timestamp filters on such columns;
- the chain sweep on chains of three steps, on CSV inputs past the
  declared classes and through rsw3 (`fa/q/crsw.py`), and the sampling
  routes (pm4py's object and component sampling draw without a seed:
  invariants only);
- EV1-1 if a cheap model appears;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list and the open choices).

Round FA's list, taken in FA: EZ's CI, EZ1-1 on fresh banks and transforms
(every filter family, both splits, filters after enrichments and splits,
filters of filter results through the transport; the sampling routes not
yet), EY1-3's mark on more OCEL 1.0 XML banks (c2gen; FA1-3 to FA1-5) and
through the OCEL 2.0 XML import of a native export (declared OC8),
EV1-1 (declared). Carried to FB: the sampling routes, EV1-1, the SQLite
leftovers, the new Polars dtypes, DW1, the other declared items, the rest
of the CU list and the open choices.

Previous list for round FA (kept for its older items):

Next round (FA) should start by re-reviewing round EZ:

- EZ's CI run (EY's run 44725615359 finished green in all six jobs);
- EZ1-1 on more banks and transforms: the filter and split download
  sweeps on the rest of j5, k5, s105 and c58, on CSV and OCEL 2.0 XML and
  SQLite inputs, the other filter families (attribute values, object type
  counts, lifecycle, connected components, activity-type matching), the
  sampling routes, a filter after an enrichment, a filter of a filter
  result read back through the transport;
- EY1-3's mark on more OCEL 1.0 XML banks (other seeds of c1gen, events
  with mixed `<date>` and `<string>` values, a `<string>` that does not
  read as a date) and through the OCEL 2.0 XML import of a native export;
- EV1-1 if a cheap model appears;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list and the open choices).

Round EZ's list, taken in EZ: EY's CI, EY1-1 and EY1-2 on fresh banks
(every filter family, the splits, the enrichments, SQLite NULL and REAL
qualifiers, classic date catalogs), EY1-3 (fixed), EV1-1 (declared), the
filter and split result frames (EZ1-1, fixed). Carried to FA: EV1-1, the
SQLite leftovers, the new Polars dtypes, DW1, the other declared items,
the rest of the CU list and the open choices.

Previous list for round EZ (kept for its older items):

Next round (EZ) should start by re-reviewing round EY:

- EY's CI run;
- EY1-1 and EY1-2 on fresh banks (other seeds and source logs: an emptied
  table after every filter family, the splits and the enrichments, SQLite
  inputs with NULL and REAL qualifiers, classic files with date, time and
  timestamp catalogs through every download and the CSV import);
- EY1-3 if a per-cell text mark is cheap enough (the OCEL 1.0 XML reader,
  both transports, the six writers, the attribute filters and the
  flattening);
- EV1-1 if a cheap model appears;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list and the open choices).

Round EY's list, taken in EY: EX's CI, EX1-1 on fresh banks, the emptied
table dtype (EY1-1), the classic date catalog (EY1-2), a fresh OCEL 1.0
bank (EY1-3, declared). Carried to EZ: EY1-3, EV1-1, the SQLite leftovers,
the new Polars dtypes, DW1, the other declared items, the rest of the CU
list and the open choices.

Previous list for round EY (kept for its older items):

Next round (EY) should start by re-reviewing round EX:

- EX's CI run;
- EX1-1 on fresh JSON qualifier banks (other seeds and source logs, the
  JSON downloads read back by pm4py, the typed transport through the
  filters and enrichments, the frontend's graph panel on such a log);
- an empty-table dtype hint: an emptied relation or O2O table keeps the
  read column's dtype in pm4py's SQLite writer (INTEGER or REAL, declared
  since EW1), so the transport could carry the column's kind when no row
  is left;
- the classic date catalog (native reads declared date attributes of a
  classic JSON file as dates): a transport marker might keep them as
  texts, as typedQualifiers does for qualifiers;
- EV1-1 if a cheap model appears (a day-offset reading in the route
  parsers only for texts the XML reader kept);
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list and the open choices).

Round EX's list, taken in EX: EW's CI, typed qualifiers from the JSON
readers (both formats, the bool kind, the writers, the graph), EW1-1 on a
fresh SQLite bank (s103). Carried to EY: EV1-1, the SQLite leftovers, the
new Polars dtypes, DW1, the other declared items, the rest of the CU list
and the open choices.

Previous list for round EX (kept for its older items):

Next round (EX) should start by re-reviewing round EW:

- EW's CI run and the rest of EV's (Linux and Windows ctest);
- EW1-1 on fresh qualifier fuzz banks (other source logs, every affinity,
  the transport through the filters and enrichments, the SQLite download
  read back by pm4py);
- typed qualifiers from the JSON readers: read_ocel2_json and read_ocel_json
  keep JSON numbers and booleans (pandas types the column from the Python
  values: an int next to a missing qualifier becomes a float, a bool stays
  a bool, so a bool kind joins int and float), and a null qualifier in OCEL
  2.0 JSON reads as ''; probe `ew/q/jq.py`, then the writers as in EW;
- EV1-1 if a cheap model appears (a day-offset reading in the route
  parsers only for texts the XML reader kept);
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list and the open choices).

Round EW's list, taken in EW: EV's CI, the typed qualifier for SQLite
inputs (reader, transport, JSON and XML writers, SQLite writer, graph).
Carried to EX: JSON inputs with number qualifiers, EV1-1, the SQLite
leftovers, the new Polars dtypes, DW1, the other declared items, the rest
of the CU list and the open choices.

Previous list for round EW (kept for its older items):

Next round (EW) should start by re-reviewing round EV:

- EV's CI run;
- a typed qualifier in the OCEL model for EV1-2 (int and float qualifiers
  from the SQLite reader through the transports, the OCEL 2.0 and classic
  JSON writers, write_ocel2_xml's TypeError for an O2O qualifier and its
  text for an event-object one, the SQLite writer's column type, the
  event-object graph's sort), with `ev/q1/qprobe.py` and f91's u91_0245 as
  the start, and the same for OCEL 2.0 JSON inputs whose qualifiers are
  JSON numbers;
- EV1-1 if a cheap model appears (a day-offset reading in the route
  parsers only for texts the XML reader kept);
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list and the open choices).

Round EV's list, taken in EV: EU's CI, EU1-1 in every reader position on
XML and JSON banks, EU1-2 through the edge-association consumers (d66 on
JSON, XML and SQLite, d905 on CSV, fsw2), the objects named but not listed
next to repeated relations (one model, declared), the day-offset values
downstream (EV1-1, declared). Carried to EW: the SQLite leftovers, the new
Polars dtypes, DW1, the other declared items, the rest of the CU list and
the open choices.

Previous list for round EV (kept for its older items):

Next round (EV) should start by re-reviewing round EU:

- EU's CI run;
- EU1-1 on OCEL 2.0 XML banks whose event times mix offsets of a day or
  more with other zones in every reader position (dropped events, events
  after a walk error, files without relations), and the declared day-offset
  values through every download if a cheap model appears;
- EU1-2's repeated triples through the edge-association consumers (OC-DFG
  performance with business hours, the LLM abstractions of the OC-DFG, the
  filters' outputs fed back to the statistics) on JSON, XML, SQLite and
  CSV inputs with repeated relations;
- repeated relations next to object rows without id or type (d904's five
  files) if the declared KeyErrors turn out to share one model;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list and the open choices).

Round EU's list, taken in EU: ET's CI, the `ocel:field` alias in a
structural fuzz across several object tables (f89, f90), NaN and NaT
changes through every download on SQLite inputs (f89, f90), relations
repeated with other qualifiers through the CSV, OCEL 2.0 and classic
downloads and the filters (d65, d904, k91, fsw2), the XML day offsets
(EU1-1). Carried to EV: the declared `ocel:field` changes (no cheap
model), the SQLite leftovers, the new Polars dtypes, DW1, the other
declared items, the rest of the CU list and the open choices.

Previous list for round EU (kept for its older items):

Next round (EU) should start by re-reviewing round ET:

- ET's CI run;
- the `ocel:field` alias (ET1-1) in a structural fuzz that renames or adds
  `ocel:field`, `ocel_changed_field` and the time columns across several
  object tables (with and without change rows, with and without a time
  column in each table), through rsw3, esw and xsw;
- NaN-valued and NaT changes (ET1-2) through every OCEL 2.0 download
  (JSON, XML, SQLite) and the classic ones, on SQLite inputs with change
  rows whose value or field is missing;
- relations repeated with other qualifiers (ET1-3) through the CSV and
  OCEL 2.0 downloads and the filters (fsw, fsw2), and in the classic XML
  of JSON and CSV inputs;
- the XML day offsets (JSON closed in ET) and the declared `ocel:field`
  changes if a cheap model appears;
- the carried items below (the SQLite leftovers, the new Polars dtypes,
  DW1, the declared items, the CU list and the open choices).

Round ET's list, taken in ET: ES's CI, the SQLite writer's hard names on
JSON, XML and CSV inputs (h65, r64), the repeated labels and the O2O
qualifier in a fresh fuzz (f87, f88), the CSV reader's checks on mutated
CSV banks (cx902, cx903, rx902, rx903), the JSON day offsets. EE1-3's
overrides stay declared (no cheap model). Carried to EU: the XML day
offsets, the SQLite leftovers, the new Polars dtypes, DW1, the other
declared items, the rest of the CU list and the open choices.

Previous list for round ET (kept for its older items):

Next round (ET) should start by re-reviewing round ES:

- ES's CI run;
- the SQLite writer's hard names (ES1-5) through rsw3, esw and xsw on
  JSON, XML and CSV inputs (an object attribute `ocel_time` or
  `ocel_changed_field` next to change rows of every kind), and attributes
  named like the frames' columns in the OCEL 2.0 XML bank (r63 has JSON
  and SQLite only);
- the repeated labels (ES1-6) and the O2O qualifier (ES1-7) in a fresh
  structural fuzz with mutations that repeat labels and break relation
  tables next to the other faults (`es/sq/hb3.py` with new seeds);
- the CSV reader's checks (ES1-3, ES1-4) on CSV banks with missing,
  repeated and reserved columns in both files;
- the declared overrides (EE1-3) of the r63 bank (attributes named
  `ocel:eid`, `ocel:oid`, `ocel:activity`, `ocel:type`, `ocel:timestamp`:
  r63_0015, 0023, 0044, 0060, 0073, 0080, 0104, 0110, 0111) if a cheap
  model appears;
- the carried items below (the XML/JSON day offsets, the SQLite leftovers,
  the new Polars dtypes, DW1, the declared items, the CU list and the open
  choices).

Round ES's list, taken in ES: ER's CI, the classic downloads' prefixed
attributes and the SQLite writer's reserved-looking columns on CSV and
JSON inputs, untimed SQLite change rows and empty listed type tables in the
fuzz (f85, f86), a fresh structural SQLite fuzz, the declared
duplicate-label cases (now modelled, ES1-6). The extra object_object
columns stay declared. Carried to ET: the XML/JSON day offsets, the SQLite
leftovers, the new Polars dtypes, DW1, the other declared items, the rest
of the CU list and the open choices.

Previous list for round ES (kept for its older items):

Next round (ES) should start by re-reviewing round ER:

- ER's CI run;
- the classic JSON download (ER1-6) on CSV and OCEL 2.0 banks whose
  columns or attributes start with `ocel:` (osw, dsw), and the SQLite
  writer's reserved-looking columns (ER1-3) on CSV and JSON inputs that
  carry them;
- untimed SQLite change rows (ER1-4) through rsw3, esw and xsw, with
  integer, real and text time columns in the other tables;
- listed types with empty type tables (ER1-9) through esw and xsw, and in
  the structural fuzz (empty a listed type's table, keep its columns);
- a fresh structural SQLite fuzz (`er/sq/hb2.py` with new seeds and more
  mutations per file), and the declared duplicate-label and extra
  object_object column cases if a cheap model appears;
- the carried items below (the XML/JSON day offsets, the SQLite leftovers,
  the new Polars dtypes, DW1, the declared items, the CU list and the open
  choices).

Round ER's list, taken in ER: EQ's CI, native SQLite downloads read back
(b52, b55, b56), the CSV feature banks through esw, fsw and fsw2, the
SQLite files of other writers (hand-built and fuzzed), the OCPN cap
against `max_traces` and the other playouts, the objects named but not
listed (kept declared), CU's classic JSON prefixed attributes. Carried to
ES: the XML/JSON day offsets, the SQLite leftovers, the new Polars dtypes,
DW1, the other declared items, the rest of the CU list and the open
choices.

Previous list for round ER (kept for its older items):

Next round (ER) should start by re-reviewing round EQ:

- EQ's CI run;
- native SQLite downloads read back through every sweep on more banks
  (EQ1-1: the OCEL 2.0 JSON/XML banks with object changes, b55 and b56,
  and the CSV feature banks), and the SQLite files of other writers
  (hand-built files with native's tables and unusual type tables);
- the OCPN playout cap (EQ1-3) against `max_traces` and the causal-net
  cap, and the other playouts and simulations on logs that blow up;
- CSV objects named but not listed (the largest declared class: every
  object route on `FEAT=notlisted`); model it if the reference's choices
  turn out deterministic;
- the carried items below (the XML/JSON day offsets, the SQLite leftovers,
  the new Polars dtypes, DW1, the declared items, the CU list and the open
  choices); the fresh seeds of the table, sequence and pandas domains were
  taken in EQ (12 banks, 0 mismatches).

Round EQ's list, taken in EQ: EP's CI, esw on the CSV feature banks and on
unrelated objects next to NaT times, a fresh OCEL 1.0 bank with unrelated
objects through the features, the routes on SQLite read back from
time-first CSV logs, psw on NaT logs (b58), the deeper object filters on
`notlisted` banks and fsw2 at levels 3 and 4, fresh seeds of the table,
sequence and pandas domains. Carried to ER: the XML/JSON day offsets, the
SQLite leftovers, the new Polars dtypes, DW1, the declared items, the CU
list and the open choices.

Previous list for round EQ (kept for its older items):

Next round (EQ) should start by re-reviewing round EP:

- EP's CI run;
- the feature routes (esw) on the CSV feature banks of `ep/csv/runf.sh`
  (EP ran only imports and downloads there) and on objects listed but not
  related next to NaT times (EP1-5 with EG's NaT rules);
- a fresh OCEL 1.0 XML/JSON bank with objects that relations do not name
  through the features (the classic readers drop them; check the drop
  order matches);
- the routes on a SQLite log read back from a CSV with its time column
  first (EP1-3 through rsw3, esw, xsw);
- causal-net routes on NaT logs (psw on b58);
- the deeper object filters (EP1-7) on CSV banks with objects named but
  not listed (`FEAT=notlisted`), which native's CSV reader declares, and
  the other filters of `rt/fsw.py` on the OCEL 1.0 banks at levels 3 and
  above;
- the carried items below (fresh seeds of the table, sequence and pandas
  domains, the XML/JSON day offsets, the SQLite leftovers, the new Polars
  dtypes, DW1, the declared items, the CU list and the open choices).

Round EP's list, taken in EP: EO's CI, b56 through esw and xsw, an OCEL
CSV bank through dsw (and every sweep), the LLM abstraction and context on
the OCEL 1.0 frames and raw/NaT columns (esw on b53 to b56 and b58), the
OCEL 1.0 XML vmap entries and keyless values (the fuzz), the None/NaT time
routes on CSV (b57, b58). Not taken (carried to EQ): causal-net routes on
NaT logs beyond EO's psw, and EO's carried items.

Previous list for round EP (kept for its older items):

Next round (EP) should start by re-reviewing round EO:

- EO's CI run;
- the OCEL 1.0 bank b56 through esw and xsw (EO ran rsw3, osw and dsw on
  it), and an OCEL CSV bank through dsw (read_ocel_csv's frames with
  dropped rows were not swept);
- the LLM OCEL abstraction and context on the OCEL 1.0 readers' frames and
  on raw/NaT columns (psw covers playout and OCPN discovery only);
- from EN's list, not taken: the remaining OCEL routes on a time column
  of None and NaT times, the OCEL 1.0 XML vmap entries and keyless values,
  causal-net routes on NaT logs;
- the carried items below (fresh seeds of the table, sequence and pandas
  domains, the XML/JSON day offsets, the SQLite leftovers, the new Polars
  dtypes, DW1, the declared items, the CU list and the open choices).

Round EO's list, taken in EO: EN's CI, x52_0046/x52_0190's dates, the
frame hints through the OCEL 1.0 readers (bank b56), playout, causal-net
playout and OCPN discovery on raw columns (psw), plus a download-content
sweep of every bank (dsw). Not taken (carried to EP): the LLM abstraction
on raw columns and the OCEL 1.0 frames, and EN's carried items.

Previous list for round EO (kept for its older items):

Next round (EO) should start by re-reviewing round EN:

- EN's CI run;
- the OCEL 2.0 XML date attribute column mixing aware and naive datetimes
  (x52_0046, x52_0190: pm4py writes the values as datetimes, native as
  strings);
- the raw-column routes no sweep reaches yet (playout, causal nets, the
  OCPN routes, the LLM abstraction) on b53, b54 and b55;
- the frame hints through the OCEL 1.0 XML and JSON readers (b55 found no
  frame type or order difference through the OCEL 2.0 XML reader, on EM's
  server either);
- from EM's list, not taken: the remaining OCEL routes on a time column of
  None and NaT times, the LLM OCEL context on the other readers' frames,
  the OCEL 1.0 XML vmap entries and keyless values, causal-net routes on
  NaT logs;
- the carried items below (fresh seeds of the table, sequence and pandas
  domains, the XML/JSON day offsets, the SQLite leftovers, the new Polars
  dtypes, DW1, the declared items, the CU list and the open choices).

Round EN's list, taken in EN: EM's CI, s53_0172's SQLite download, None
for NULL times of object columns (OC-DFG's NoneType), the frame hints
through the OCEL 2.0 XML reader (bank b55), the frames' column order of
every download (osw), the SQLite change cells. Not taken (carried to EO):
playout, causal nets, OCPN and the other routes the sweeps do not reach,
the OCEL 1.0 readers' frame hints, and EM's carried items.

Previous list for round EN (kept for its older items):

Next round (EN) should start by re-reviewing round EM:

- EM's CI run;
- s53_0172's SQLite download: pm4py's to_sql types each object type
  table's object column by that table's values, its change rows' cells in
  other columns included, which needs the changes hint per object type;
- the relations frame of a raw event time column holds None for a NULL
  time of an object-dtype table (the events frame NaN), so OC-DFG's and
  the LLM abstraction's messages name `NoneType` (s54_0163; pm4py's pair
  also varies between runs there);
- the frame hints through the OCEL 2.0 XML and OCEL 1.0 readers (EM fed
  them from the JSON and SQLite readers and the transport), and the
  remaining raw-column routes b53 and b54 did not reach (playout, causal
  nets, the OCPN routes);
- from EL's list, not taken: the remaining OCEL routes on a time column of
  None and NaT times, the LLM OCEL context on the other readers' frames,
  the OCEL 1.0 XML vmap entries and keyless values, causal-net routes on
  NaT logs;
- the carried items below (fresh seeds of the table, sequence and pandas
  domains, the XML/JSON day offsets, the SQLite leftovers, the new Polars
  dtypes, DW1, the declared items, the CU list and the open choices).

Round EM's list, taken in EM: EL's CI, the writers' frame column order and
pre-drop dtype, the naive change times in write_ocel2_xml, g52_0173,
s53_0172 (its OCEL 2.0 XML download; the SQLite download carried), a fresh
spoiled SQLite bank (b54) with numbers-only, NULL-first and text-first
columns and raw change columns through the routes rsw3, esw and xsw cover.
Not taken (carried to EN): playout, causal nets, OCPN and the other
routes those sweeps do not reach, and EL's carried items.

Previous list for round EM (kept for its older items):

Next round (EM) should start by re-reviewing round EL:

- EL's CI run;
- the frame column order and the pre-drop dtype in the writers (file
  order including dropped rows; an unrelated object's value retypes a
  column: g52_0127, g52_0290), which need frame hints in the OCEL struct
  or the transport;
- a naive change time next to aware ones in write_ocel2_xml (`+00:00`:
  g52_0047, g52_0107, g52_0223) and float-declared change texts pm4py
  keeps as text after a round trip (g52_0173), and a change text "1" in a
  text column that the OCEL 2.0 XML and SQLite downloads type as a number
  (s53_0172);
- a fresh spoiled SQLite bank (other seeds of `ek/s/sgen.py`, more raw
  columns: numbers only, NaN first, raw change columns with numbers) and
  the raw columns through the routes b53 did not reach (playout, causal
  nets, the OCPN routes, splits);
- from EK's list, not taken: the remaining OCEL routes on a time column of
  None and NaT times, the LLM OCEL context on the other readers' frames,
  the OCEL 1.0 XML vmap entries and keyless values, causal-net routes on
  NaT logs;
- the carried items below (fresh seeds of the table, sequence and pandas
  domains, the XML/JSON day offsets, the SQLite leftovers, the new Polars
  dtypes, DW1, the declared items, the CU list and the open choices).

Round EL's list, taken in EL: EK's CI, the spoiled SQLite bank through the
routes (raw event and change time columns, change rows without a field;
duplicate rows declared). Not taken (carried to EM): the writers' frame
column order and pre-drop dtype, the naive change times in
write_ocel2_xml, g52_0173, and EK's carried items.

Previous list for round EL (kept for its older items):

Next round (EL) should start by re-reviewing round EK:

- EK's CI run;
- the spoiled SQLite bank through the routes (`ek/run7.sh`, out7/b53s.*:
  rsw2 586 of 10,101, esw 136 of 4,144, xsw 92 of 3,367): event type
  tables whose `ocel_time` holds invalid dates, integers, floats, `''` or
  texts (pm4py keeps an object/str time column and each route fails its
  own way), change rows with an empty field, attribute columns mixing
  texts and numbers; duplicate rows are declared;
- the frame column order and the pre-drop dtype in the writers (file
  order including dropped rows; an unrelated object's value retypes a
  column: g52_0127, g52_0290), which need frame hints in the OCEL struct
  or the transport;
- a naive change time next to aware ones in write_ocel2_xml (`+00:00`:
  g52_0047, g52_0107, g52_0223) and float-declared change texts pm4py
  keeps as text after a round trip (g52_0173);
- from EK's list, not taken: the remaining OCEL routes on a time column of
  None and NaT times, the LLM OCEL context on the other readers' frames,
  the OCEL 1.0 XML vmap entries and keyless values, causal-net routes on
  NaT logs;
- the carried items below (fresh seeds of the table, sequence and pandas
  domains, the XML/JSON day offsets, the SQLite leftovers, the new Polars
  dtypes, DW1, the declared items, the CU list and the open choices).

Round EK's list, taken in EK: EJ's CI, the spoiled OCEL 2.0 JSON bank
through the routes (writer types and spellings, NaN, `+25:00` change
times, its xsw and esw), NaT change times beyond the writers (none reach
other routes), the SQLite reader's other dtypes (a spoiled bank: its
structure errors fixed, its routes open above). Not taken (carried to
EL): the remaining None/NaT time routes, the LLM context on the other
readers, the XML vmap entries and keyless values, causal-net routes on
NaT logs, and the carried items.

Previous list for round EK (kept for its older items):

Next round (EK) should start by re-reviewing round EJ:

- EJ's CI run;
- the spoiled OCEL 2.0 JSON bank through the routes (`ej/run4.sh`,
  out4/b52j.rsw.*; its xsw and esw are still to run): date-declared
  columns holding non-date values (writer types and
  spellings; dateutil reads native's XML `type="date"` value `123` as the
  year 123 with today's month and day), float-declared texts in the change
  frame (`'inf'`, `'1000.0'` read as floats by pm4py), change times with an
  invalid offset (`+25:00`: pm4py's OCEL filters raise `AttributeError ...
  'tz_convert'`), a NaN value where pm4py has none (g52_0038);
- NaT change times (EJ1-7) through the routes beyond the writers (the
  filters with time bounds, the temporal and object-change features);
- the OCEL 2.0 SQLite reader's other `read_sql` dtypes (text and integer
  mixes, NULL ids, event tables);
- from EJ's list, not taken: the remaining OCEL routes on a time column of
  None and NaT times (time-bound filters, other splits, temporal and
  statistics routes), the LLM OCEL context on the other readers' frames,
  the OCEL 1.0 XML vmap entries and keyless values, causal-net routes on
  NaT logs;
- the carried items below (fresh seeds of the table, sequence and pandas
  domains, the XML/JSON day offsets, the SQLite leftovers, the new Polars
  dtypes, DW1, the declared items, the CU list and the open choices).

Round EI's list, taken in EJ: EI's CI, the OCEL 2.0 JSON reader's
values of the wrong declared type and object attribute times (the
imports; the routes are open above), the OCEL 2.0 XML walk order. Not
taken (carried to EK): the LLM context on the other readers, the
remaining None/NaT time routes, the XML vmap entries and keyless values,
causal-net routes on NaT logs, the fresh seeds, the day offsets, the
SQLite leftovers, the dtypes, DW1, the declared items, the CU list and the
open choices.

Previous list for round EJ (kept for its older items):

Next round (EJ) should start by re-reviewing round EI:

- EI's CI run, and the end of EH's Linux and Windows ctest (run
  44422565629);
- the LLM OCEL context on the other readers' frames now that it is the
  description (SQLite and OCEL 2.0 JSON/XML times in UTC, budgets that cut
  inside the objects), and the description and LLM routes on logs with
  object changes or O2O links only;
- the remaining OCEL routes on a time column of None and on NaT times
  (the OCEL filters with time bounds, splits other than
  ancestors/descendants, the temporal and statistics routes);
- the OCEL 2.0 JSON reader's other values (attribute values of the wrong
  type for their declared type, object attribute times), and the OCEL 2.0
  XML reader's walk order on missing attributes (EI1-6's JSON order was not
  checked for XML);
- the OCEL 1.0 XML vmap entries still refused (EG1-6) and the keyless
  values (not probed in EI);
- the carried items below (fresh seeds of the table, sequence and pandas
  domains, the XML/JSON day offsets, the SQLite leftovers, the new Polars
  dtypes, DW1, the declared items, the CU list and the open choices).

Round EH's list, taken in EI: EH's CI and EG's ctest, NaT and None times
through the LLM abstractions, OCEL conformance, the feature families and
playout, the OCEL 2.0 JSON/XML readers on unreadable times, CSV imports
with mixed offsets and NaT. Not taken (carried to EJ): the causal-net
routes on NaT logs, conformance on flattened logs beyond EH's, the XML
vmap entries and keyless values, the fresh seeds, the XML/JSON day
offsets, the SQLite leftovers, the new dtypes, the DW1 keys, the declared
items, the CU list and the open choices.

Previous list for round EI (kept for its older items):

Next round (EI) should start by re-reviewing round EH:

- EH's CI run, and the end of EG's Linux and Windows ctest (run
  44405676226);
- NaT and None event times through the routes rsw and xsw do not cover
  (the LLM abstractions, conformance on flattened logs, the OCEL feature
  variants beyond compatibility, the playout and causal-net routes), and
  the OCEL 2.0 JSON/XML readers' frames when an event time is unreadable
  (they raise in pm4py; check native refuses alike);
- CSV imports with mixed offsets and NaT, the descriptions and LLM
  context on such logs (the LLM context still prints UTC: its model is
  read_ocel_json's frame);
- the OCEL 1.0 XML vmap entries still refused (EG1-6) and the keyless
  values (pm4py's NaN-named column; EH1 notes `write_ocel_json` raises
  `AttributeError: 'float' object has no attribute 'startswith'`);
- the carried items below (fresh seeds of the table, sequence and pandas
  domains, the XML/JSON day offsets, the SQLite leftovers, the new Polars
  dtypes, DW1, the declared items, the CU list and the open choices).

Round EG's list, taken in EH: EG's CI and EF's ctest, the NaT times from
the other readers and the time-based routes on mixed NaT logs, the XML
vmap entries and keyless values (probed again, still declared). Not taken
(carried to EI): the fresh seeds, the XML/JSON day offsets, the SQLite
leftovers, the new dtypes, the DW1 keys, the declared items, the CU list
and the open choices.

Previous list for round EH (kept for its older items):

Next round (EH) should start by re-reviewing round EG:

- EG's CI run, and the end of EF's Linux and Windows ctest (run
  44395233990);
- NaT event times from the other readers through the routes (EG swept the
  SQLite ones only): CSV time cells pm4py cannot read, OCEL 2.0 JSON/XML
  times, the filters and time-based routes on a log mixing NaT and times;
- the OCEL 1.0 XML vmap entries still refused (EG1-6 declared: a row that
  stays with another activity, id or time) and the keyless values (a NaN
  column);
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the OCEL 2.0 XML/JSON day offsets that depend on the column, the SQLite
  leftovers (texts beyond ASCII, the kept columns downstream);
- the Duration, Time, Decimal and Int128 Polars dtypes (the transport has
  no cells for them yet);
- the declared row-mask keys, dicts mapping str index labels and list
  stakes of str items (DW1), the declared items (DQ1-2, DO1-2, DN1-6), the
  rest of the CU list, and the open choices on CSV timestamp columns of
  plain numbers and "roles to diagnostics groups" (still declared unless
  Philipp answers).

Round EF's targets, taken in EG (findings EG1-1 to EG1-6): EF's CI and
EE's Windows ctest, the SQLite NaT times through the routes, the other
readers' relations naming missing entities, the XML vmap overrides. Not
taken (carried to EH): the fresh seeds, the keyless XML values, the XML/JSON
day offsets, the SQLite leftovers, the new dtypes, the DW1 keys, the
declared items, the CU list and the open choices.

Previous list for round EG (kept for its older items):

Next round (EG) should start by re-reviewing round EF:

- EF's CI run, and the end of EE's Windows ctest (run 44383563142);
- event times pm4py holds as NaT (OCEL 2.0 SQLite `''` or NULL times, sent
  on as transports with `"minera:unreadableEventTimes"`) through the routes
  (`ef/sq/sgen.py`, `READER=sqlite rsw.py`, `ef/sq/q1.out`): pm4py
  describes them (`NaT`, sorted last) where native refuses; its
  compatibility event features and lifecycle object features raise
  `ValueError: NaTType does not support timestamp`; its OC-DFG measures a
  pair with a NaT end as `[0]`; `write_ocel_xml` writes no timestamp
  element for such an event (native `value=""`); `write_ocel2_sqlite`
  raises `KeyError: 'ocel_time'` when a type's only event has a NaT time;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- routes on logs of the other readers with events or objects left without
  relations (OCEL 2.0 JSON/XML/SQLite files whose relations name missing
  entities), and the native-style features on them (they still skip);
- the OCEL 1.0 XML vmap `ocel:eid`/`ocel:activity`/`ocel:timestamp`
  overrides (refused; EE's three-step filtering model may now let them be
  read as pm4py reads them) and keyless values (a NaN column);
- the OCEL 2.0 XML/JSON day offsets that depend on the column, the SQLite
  leftovers (texts beyond ASCII, the kept columns downstream);
- the Duration, Time, Decimal and Int128 Polars dtypes (the transport has
  no cells for them yet);
- the declared row-mask keys, dicts mapping str index labels and list
  stakes of str items (DW1), the declared items (DQ1-2, DO1-2, DN1-6), the
  rest of the CU list, and the open choices on CSV timestamp columns of
  plain numbers and "roles to diagnostics groups" (still declared unless
  Philipp answers).

Round EE's targets, taken in EF (findings EF1-1 to EF1-5): EE's CI, routes
fed classic JSON, CSV and SQLite transports through the routes (the
SQLite NaT findings kept for EG). Not taken (carried to EG): the fresh
seeds, the XML vmap overrides and keyless
values, the XML/JSON day offsets, the SQLite leftovers, the new dtypes,
the DW1 keys, the declared items, the CU list and the open choices.

Previous list for round EF (kept for its older items):

Next round (EF) should start by re-reviewing round EE:

- EE's CI run (Windows in particular: `minera_review_eb2_tests` with
  EE1-8's TEXT-affinity cells must pass; macOS was cancelled in ED's run);
- routes fed classic JSON: their outputs on files EE now reads differently
  (overrides kept, float columns, events kept without relations), and
  transports of logs from the other readers (OCEL 1.0 XML vmap `ocel:*`
  keys, CSV/SQLite empty times) through filters and exports;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the OCEL 1.0 XML vmap `ocel:eid`/`ocel:activity`/`ocel:timestamp`
  overrides (refused; EE's three-step filtering model may now let them be
  read as pm4py reads them) and keyless values (a NaN column);
- the OCEL 2.0 XML/JSON day offsets that depend on the column, the SQLite
  leftovers (texts beyond ASCII, the kept columns downstream);
- the Duration, Time, Decimal and Int128 Polars dtypes (the transport has
  no cells for them yet);
- the declared row-mask keys, dicts mapping str index labels and list
  stakes of str items (DW1), the declared items (DQ1-2, DO1-2, DN1-6), the
  rest of the CU list, and the open choices on CSV timestamp columns of
  plain numbers and "roles to diagnostics groups" (still declared unless
  Philipp answers).

Round ED's targets, taken in EE (findings EE1-1 to EE1-8): ED's Windows
ctest, the classic JSON reader, its transport. Not taken (carried to EF):
the fresh seeds, the XML vmap overrides and
keyless values, the XML/JSON day offsets, the SQLite leftovers, the new
dtypes, the DW1 keys, the declared items, the CU list and the open choices.

Previous list for round EE (kept for its older items):

Next round (EE) should start by re-reviewing round ED:

- ED's CI run (Windows in particular: ED1-7 changes the SQLite REAL
  texts there again; `minera_review_eb2_tests` must pass);
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the classic JSON reader beyond date values (ED1, `ed/js/jp1.py`):
  non-string and null activities, non-string and string omaps, a missing
  `ocel:vmap`/`ocel:ovmap` key, ovmap `ocel:type` and vmap `ocel:*`
  overrides, list/dict/big-int/NaN values, unreadable times. Native reads
  classic JSON as its own transport too, so keep the transport's reading
  where the two must differ and declare it;
- the OCEL 1.0 XML vmap `ocel:eid`/`ocel:activity`/`ocel:timestamp`
  overrides (the reference's result depends on which events survive its
  filtering; refused for now) and keyless values (a NaN column);
- the OCEL 2.0 XML/JSON day offsets that depend on the column, the SQLite
  leftovers (texts beyond ASCII, the kept columns downstream);
- the Duration, Time, Decimal and Int128 Polars dtypes (the transport has
  no cells for them yet);
- the declared row-mask keys, dicts mapping str index labels and list
  stakes of str items (DW1), the declared items (DQ1-2, DO1-2, DN1-6), the
  rest of the CU list, and the open choices on CSV timestamp columns of
  plain numbers and "roles to diagnostics groups" (still declared unless
  Philipp answers).

Round EC's targets, taken in ED (findings ED1-1 to ED1-7): EC's CI, the
fresh seeds, the OCEL 1.0 XML reference shapes, the classic JSON reader
(probed, findings kept for EE). Not taken (carried to EE): the XML/JSON
day offsets, the SQLite leftovers beyond ASCII, the new dtypes, the DW1
keys, the declared items, the CU list and the open choices.

Previous list for round ED (kept for its older items):

Next round (ED) should start by re-reviewing round EC:

- EC's CI run (Windows in particular: EC1-5 changed the SQLite REAL
  texts there);
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the OCEL 1.0 XML reader's other reference-shaped events and objects
  (`ec/cl2/p11.py`): every keyed child read whatever its tag, a repeated
  key winning last (timestamp, activity, vmap lists), a `<string
  key="timestamp">` kept as text, `<int>` and `<boolean>` values kept as
  strings, a value without `value` as `"None"`, one without a key under
  NaN; then the classic JSON reader beyond date values;
- the OCEL 2.0 XML/JSON day offsets that depend on the column, the SQLite
  leftovers (texts beyond ASCII, the kept columns downstream);
- the Duration, Time, Decimal and Int128 Polars dtypes (the transport has
  no cells for them yet);
- the declared row-mask keys, dicts mapping str index labels and list
  stakes of str items (DW1), the declared items (DQ1-2, DO1-2, DN1-6), the
  rest of the CU list, and the open choices on CSV timestamp columns of
  plain numbers and "roles to diagnostics groups" (still declared unless
  Philipp answers).

Round EB's targets, taken in EC (findings EC1-1 to EC1-5): EB's CI, the
fresh seeds, the CSV leftovers, the SQLite re-review, the classic readers'
date values (and the XML omap lists found on the way). Not taken (carried
to ED): the XML/JSON day offsets, the SQLite leftovers beyond ASCII, the
new dtypes, the DW1 keys, the declared items, the CU list and the open
choices.

Previous list for round EC (kept for its older items):

Next round (EC) should start by re-reviewing round EB:

- EB's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the SQLite leftovers (texts beyond ASCII, digit runs inside texts, the
  kept columns' values downstream of the importer), the OCEL 2.0 XML/JSON
  day offsets that depend on the column, the CSV leftovers (years outside
  1000-9999, offsets of 24 hours or more, time-first texts, quarters,
  float-like texts) now that `to_datetime_utc_mixed` reads several of them
  for SQLite, and the classic OCEL readers' (JSON, XML) date values;
- the Duration, Time, Decimal and Int128 Polars dtypes (the transport has
  no cells for them yet);
- the declared row-mask keys, dicts mapping str index labels and list
  stakes of str items (DW1), the declared items (DQ1-2, DO1-2, DN1-6), the
  rest of the CU list, and the open choices on CSV timestamp columns of
  plain numbers and "roles to diagnostics groups" (still declared unless
  Philipp answers).

Round EA's targets, taken in EB (findings EB1-1 to EB2-1): EA's CI, the
fresh seeds, the OCEL 2.0 XML, JSON and SQLite timestamp columns. Not taken
(carried to EC): the CSV leftovers, the new dtypes, the DW1 keys, the
declared items, the CU list and the open choices.

Previous list for round EB (kept for its older items):

Next round (EB) should start by re-reviewing round EA:

- EA's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the CSV timestamp leftovers (years outside 1000-9999, offsets of 24
  hours or more, texts that start with a time and take today's date,
  quarters, float-like texts, a nanosecond text next to a year outside the
  nanosecond range) and the other OCEL readers' timestamp columns (XML,
  JSON, SQLite) against pandas' reading;
- the Duration, Time, Decimal and Int128 Polars dtypes (the transport has
  no cells for them yet);
- the declared row-mask keys, dicts mapping str index labels and list
  stakes of str items (DW1), the declared items (DQ1-2, DO1-2, DN1-6), the
  rest of the CU list, and the open choices on CSV timestamp columns of
  plain numbers and "roles to diagnostics groups" (still declared unless
  Philipp answers).

Round DZ's targets, taken in EA (findings EA1-1 to EA1-3): DZ's CI, the
fresh seeds, the CSV timestamp columns pandas guesses no format for (with
the NA and NaT spellings found on the way). Not taken (carried to EB): the
CSV leftovers above, the new dtypes, the DW1 keys, the declared items, the
CU list and the open choices.

Previous list for round EA (kept for its older items):

Next round (EA) should start by re-reviewing round DZ:

- DZ's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the CSV timestamp columns pandas guesses no format for (dateutil element
  by element: a lone one-digit hour, a comma or ten-digit fraction after a
  non-ISO date, month names, AM/PM, out-of-range first texts) and the
  other DZ1-2 leftovers (offsets of 24 hours or more, years outside
  1000-9999, leading or trailing spaces);
- the Duration, Time, Decimal and Int128 Polars dtypes (the transport has
  no cells for them yet);
- the declared row-mask keys, dicts mapping str index labels and list
  stakes of str items (DW1), the declared items (DQ1-2, DO1-2, DN1-6), the
  rest of the CU list, and the open choices on CSV timestamp columns of
  plain numbers and "roles to diagnostics groups" (still declared unless
  Philipp answers).

Round DY's targets, taken in DZ (findings DZ1-1 to DZ1-3): DY's CI, the
fresh seeds, the consecutive addition, the CSV timestamp shapes. Not taken
(carried to EA): the new dtypes, the DW1 keys, the declared items, the CU
list and the open choices.

Previous list for round DZ (kept for its older items):

Next round (DZ) should start by re-reviewing round DY:

- DY's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the consecutive grouping's `df[key] + df[act]` with a list key
  (declared in DX1: the DataFrame plus Series alignment and its per-dtype
  errors), the declared row-mask keys, dicts mapping str index labels,
  and list stakes of str items;
- the CSV timestamp shapes outside the ISO family (DV1-2: the OCEL CSV
  reader against pandas' format guess and dateutil);
- the Duration, Time, Decimal and Int128 Polars dtypes (the transport has
  no cells for them yet);
- the declared items (DQ1-2, DO1-2, DN1-6), the rest of the CU list, and
  the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DX's targets, taken in DY (findings DY1-1 to DY1-5): DX's CI, the
fresh seeds, the Polars table filters' list keys (and DQ1-5). Not taken
(carried to DZ): the consecutive addition, the CU list, the new dtypes,
the CSV timestamp shapes and the open choices.

Previous list for round DY (kept for its older items):

Next round (DY) should start by re-reviewing round DX:

- DX's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- list case keys in the Polars table filters (probe
  `review-harness/round-dx/pp/mk_p2.py`, 205 of 15,462 calls differ): the
  LTL filters' "outside the observed domain" answers for list items
  (Polars fails on duplicate `literal` names, repeated join keys, sort
  lengths, comparisons), the attribute filters' selector with several
  listed columns, dict and set items failing as literals in pm4py;
- the consecutive grouping's `df[key] + df[act]` with a list key
  (declared in DX1: the DataFrame plus Series alignment and its per-dtype
  errors), the declared row-mask keys, dicts mapping str index labels,
  and list stakes of str items;
- the declared items (DQ1-5, DQ1-2, DO1-2, DN1-6), the rest of the CU
  list, the Duration, Time, Decimal and Int128 Polars dtypes, the CSV
  timestamp shapes outside the ISO family (DV), and the open choices on
  CSV timestamp columns of plain numbers and "roles to diagnostics
  groups" (still declared unless Philipp answers).

Round DW's targets, taken in DX (findings DX1-1 to DX1-10): DW's CI, the
fresh seeds, list case keys in the pandas sequence filters, then the
Polars sequence filters' list keys. Not taken (carried to DY): the Polars
table filters' list keys, the declared items, the CU list, the new dtypes
and the open choices.

Previous list for round DX (kept for its older items):

Next round (DX) should start by re-reviewing round DW:

- DW's CI run (and DV's Windows job);
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- list case keys in the pandas sequence filters: native raises
  `KeyError: ''` for every list key, pandas reads them as the table
  filters do (probe `review-harness/round-dw/pp/mk_s1.py`: 6,029 of 7,230
  calls differ); then the Polars filters' list keys, the declared row-mask
  keys, dicts mapping str index labels, and list stakes of str items;
- the declared items (DQ1-5, DQ1-2, DO1-2, DN1-6), the rest of the CU
  list, the Duration, Time, Decimal and Int128 Polars dtypes, the CSV
  timestamp shapes outside the ISO family (DV), and the open choices on
  CSV timestamp columns of plain numbers and "roles to diagnostics
  groups" (still declared unless Philipp answers).

Round DV's targets, taken in DW (findings DW1-1 to DW1-6): DV's CI, the
fresh seeds, list case keys across the pandas filters. Not taken (carried
to DX): the declared items, the CU list, the new dtypes and the open
choices.

Previous list for round DW (kept for its older items):

Next round (DW) should start by re-reviewing round DV:

- DV's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- list items in a list case key across the pandas filters (variants,
  timestamp, between, attributes, cases, paths, LTL): pandas takes a list
  item as an array grouper (`Grouper and axis must be same length`, or
  groups by its values when it matches the rows), a tuple item as a label,
  prints list items as tuples in `None of [Index(...)]`, and some
  functions read their timestamp or activity column first; probe
  `review-harness/round-dv/pp/mk_l1.py` (217 of 1,863 calls differ);
- the declared items (DQ1-5, DQ1-2, DO1-2, DN1-6), the rest of the CU
  list, the Duration, Time, Decimal and Int128 Polars dtypes, the CSV
  timestamp shapes outside the ISO family (DV), and the open choices on
  CSV timestamp columns of plain numbers and "roles to diagnostics
  groups" (still declared unless Philipp answers).

Round DU's targets, taken in DV (findings DV1-1 to DV1-4): DU's CI, the
relation-free OCEL frames, the CSV reader's timestamp formats, the
PROPAGATE pass order and the fresh seeds. Not taken (carried to DW): the
declared items, the CU list, the new dtypes and the open choices.

Previous list for round DV (kept for its older items):

Next round (DV) should start by re-reviewing round DU:

- DU's CI run (and DT's macOS and Windows jobs);
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- relation-free OCEL CSV imports and empty playouts: pm4py's relations
  frame holds only `ocel:qualifier` (an empty playout's events and
  objects frames have no columns), so nearly every function raises
  KeyError (per function in `review-harness/round-du/u/ocdeg_du.out`);
  native answers as for an empty log;
- the OCEL CSV reader's timestamp parsing: pandas infers one format from
  the first text and refuses later texts of another shape and mixed
  offsets (`unconverted data remains`, `doesn't match format`, `Mixed
  timezones detected`); native reads them;
- link analysis under PROPAGATE without look-forward and first
  occurrences (three declared calls; not taken in DT or DU);
- the declared items (DQ1-5, DQ1-2, DO1-2, DN1-6), the rest of the CU
  list, the Duration, Time, Decimal and Int128 Polars dtypes, and the open
  choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DT's targets, taken in DU (findings DU1-1 to DU1-4): DT's CI, the
fresh seeds, the untyped-relations flag across routes that build logs
(it was already carried; the naive frame and the column order were not),
the tuple and masked operands. Not taken (carried to DV): PROPAGATE, the
declared items, the CU list, the new dtypes and the open choices.

Previous list for round DU (kept for its older items):

Next round (DU) should start by re-reviewing round DT:

- DT's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the OCEL transport flag `minera:untypedRelations` across every route
  that builds a new log (split sublogs, playout, enrichment, the SQLite and
  CSV imports): pm4py's in-memory frame types after each;
- other pandas Series comparisons with a tuple operand (the timestamp
  filters' bounds, the variants filters), and numpy's tuple rules for
  masked (Int64, Float64, boolean) columns in the numeric attribute
  filters;
- link analysis under PROPAGATE without look-forward and first
  occurrences (three declared calls; a key-level pass where sources
  sharing an in key share their result fits some of them; not taken in
  DT);
- the keep-once relative occurrence count with a dict case key naming
  several existing columns, the nanosecond window at pandas' int64 range
  ends, the eager paths performance race (DO1-2), the thread-count quirk
  of DN1-6 (all declared);
- the rest of the CU list (equal-size joins, CJ/CF/CD items); Duration,
  Time, Decimal and Int128 column dtypes are not in any Polars domain yet;
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DS's targets, taken in DT (findings DT1-1 to DT1-4): DS's CI run and
DR's Windows job, the fresh seeds, the tuple bounds of the other pandas
filters, the degenerate OCELs. Not taken (carried to DU): PROPAGATE, the
declared items, the CU list, the new dtypes and the open choices.

Previous list for round DT (kept for its older items):

Next round (DT) should start by re-reviewing round DS:

- DS's CI run and DR's Windows job (still building when DS pushed);
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- tuple bounds in the other pandas filters that compare a Series with a
  bound (the numeric attribute filters, paths performance, the sequence
  filters' size bounds): DS modelled numpy's array rules only in the case
  performance and case size filters;
- degenerate OCELs elsewhere: files without relations, objects or events
  through the other OCEL endpoints and the XML and SQLite readers (pm4py's
  XML readers raise AttributeError and its SQLite reader `No objects to
  concatenate` on a relation-free log);
- link analysis under PROPAGATE without look-forward and first occurrences
  (three declared calls; a key-level pass where sources sharing an in key
  share their result fits some of them);
- the keep-once relative occurrence count with a dict case key naming
  several existing columns, the nanosecond window at pandas' int64 range
  ends, the eager paths performance race (DO1-2), the thread-count quirk
  of DN1-6 (all declared);
- the rest of the CU list (equal-size joins, CJ/CF/CD items); Duration,
  Time, Decimal and Int128 column dtypes are not in any Polars domain yet;
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DR's targets, taken in DS (findings DS1-1 to DS1-6): DR's CI run and
DQ's Windows job, the fresh seeds, the temporal profile and flattening
merges, the PROPAGATE order (all but three calls). Not taken (carried to
DT): the declared items, the CU list, the new dtypes and the open choices.

Previous list for round DS (kept for its older items):

Next round (DS) should start by re-reviewing round DR:

- DR's CI run (and DQ's Linux and Windows jobs, still running when DR
  pushed);
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- pandas' merge order in the two-key merge of the temporal profile
  conformance on a frame (per-case deviation order when a profile lacks
  some activity pairs) and in the OCEL flattening's merges (row order of
  the flattened frame);
- link analysis under PROPAGATE: the target order of a set grown by
  `set.union` (CPython's set_merge copies the table when sizes allow and
  presizes otherwise; declared);
- the keep-once relative occurrence count with a dict case key naming
  several existing columns (declared, outside the native domain);
- the nanosecond window at pandas' int64 range ends (declared);
- the eager paths performance race on frames of four rows or more (DO1-2,
  declared) and the thread-count quirk of DN1-6 (declared);
- the rest of the CU list (equal-size joins, CJ/CF/CD items); Duration,
  Time, Decimal and Int128 column dtypes are not in any Polars domain yet
  (the table transport has no cells for them);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DQ's targets, taken in DR (findings DR1-1 to DR1-5): DQ's CI run,
the fresh seeds and pandas' merge order elsewhere (the LTL list bounds'
plain order, link analysis, case relations checked). Not taken (carried
to DS): the declared items, the CU list, the new dtypes and the open
choices.

Previous list for round DR (kept for its older items):

Next round (DR) should start by re-reviewing round DQ:

- DQ's CI run (the first with the macOS job since DO, and the first
  Linux run on the vendored time zone database);
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- pandas' merge order elsewhere: the other pandas filters and statistics
  that return or iterate a merge of non-unique keys in merge order (the
  equal-length reordering of DQ1-5 changes row order, not row sets);
- the keep-once relative occurrence count with a dict case key naming
  several existing columns (declared, outside the native domain);
- the nanosecond window at pandas' int64 range ends (declared);
- the eager paths performance race on frames of four rows or more (DO1-2,
  declared) and the thread-count quirk of DN1-6 (declared);
- the rest of the CU list (equal-size joins, CJ/CF/CD items); Duration,
  Time, Decimal and Int128 column dtypes are not in any Polars domain yet
  (the table transport has no cells for them);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DP's targets, taken in DQ (findings DQ1-1 to DQ1-5): DP's CI run,
the fresh seeds, dicts with keys that are no str and tuple-keyed dicts in
`paths.apply`, date strings against datetime64 columns of other units and
zones, and the dateutil question; also the time zone database. Not taken
(carried to DR): the eager paths race, the thread-count quirk of DN1-6,
the CU list, the new dtypes and the open choices.

Previous list for round DQ (kept for its older items):

Next round (DQ) should start by re-reviewing round DP:

- DP's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- dict keys with non-str keys (`{1: "a"}`, `{None: 1}`) as attribute, case
  and activity keys in the Polars filters (pl.col names the key's type;
  the bank transport keeps only the key text, so the transport needs a
  typed key first), and tuple-keyed dicts in `paths.apply` (DP1-1);
- date strings in the other pandas filters that compare with a
  datetime64 column (between, timestamp, case performance) and datetime64
  columns of other units (`ns`, `ms`) in the numeric filters; whether a
  small dateutil subset (month names, `1e3`) is worth modelling (DP1-4);
- the eager paths performance race on frames of four rows or more (DO1-2,
  declared) and the thread-count quirk of DN1-6 (declared);
- the rest of the CU list (equal-size joins, CJ/CF/CD items); Duration,
  Time, Decimal and Int128 column dtypes are not in any Polars domain yet
  (the table transport has no cells for them);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DO's targets, taken in DP (findings DP1-1 to DP1-5): DO's CI run,
the fresh seeds, iterated arguments in the Polars table filters, dict keys
in the other sequence filters, and date strings and datetime bounds in the
pandas filters; also dict attribute keys in the attribute counts. Not
taken (carried to DQ): the eager paths race, the thread-count quirk of
DN1-6, the CU list, the new dtypes and the open choices.

Previous list for round DP (kept for its older items):

Next round (DP) should start by re-reviewing round DO:

- DO's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the eager paths performance race on frames of four rows or more (DO1-2:
  the reference's error follows the process, not the layout; a model
  would need what in the process decides it, such as the thread pool's
  state);
- iterated arguments (str, dict, set, tuple) in the Polars table filters
  that take value lists (attributes, between, LTL activities, paths),
  as sv2 did for the sequence filters, eagerly and lazily;
- dict keys in the other sequence filters' case and activity keys beyond
  the variant filters (rework, activity_split, start/end activities),
  eagerly and lazily;
- date strings pandas can parse as bounds against datetime64 columns in
  the pandas numeric filters (the reference compares them, the native
  filter refuses every str there; DO1-6), and datetime bounds in the other
  pandas filters that compare (between, timestamp, case performance);
- the declared thread-count quirk of DN1-6;
- the rest of the CU list (equal-size joins, CJ/CF/CD items); Duration,
  Time, Decimal and Int128 column dtypes are not in any Polars domain yet
  (the table transport has no cells for them);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DN's targets, taken in DO (findings DO1-1 to DO1-6): DN's CI run,
the fresh seeds, the partition race in the case, paths and timestamp
filters, the eager sequence probes over the set, big-int and scalar
domains on `literal`-column frames and the v1 shapes in the sequence
variant filters; also the pandas datetime bounds of a fresh call. Not taken (carried to DP): the thread-count quirk of
DN1-6, the CU list and the open choices.

Previous list for round DO (kept for its older items):

Next round (DO) should start by re-reviewing round DN:

- DN's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- whether the other filters with struct-cast bounds (the case, timestamp
  and paths performance filters) also filter in row partitions on frames
  of four or more columns, and race as the numeric attribute filters do
  (only `numeric_predicate` models partitions);
- eager sequence probes over the set, big-int and scalar domains on
  `literal`-column frames (DN covered the base CK, CP, CJ and CH domains
  only), and the v1 shapes (literal case keys, empty activity lists, None
  keys beside literal timestamps) in the sequence variant filters;
- the declared thread-count quirk of DN1-6 (a filter over four or more
  columns with a struct comparison on null numeric cells);
- the rest of the CU list (equal-size joins, CJ/CF/CD items); Duration,
  Time, Decimal and Int128 column dtypes are not in any Polars domain yet
  (the table transport has no cells for them);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DM's targets, taken in DN (findings DN1-1 to DN1-6): DM's CI run,
the fresh seeds, the 138 carried eager table calls, the eager sequence
probes over `literal`-column and empty frames and the partition race. Not
taken (carried to DO): the CU list and the open choices.

Previous list for round DN (kept for its older items):

Next round (DN) should start by re-reviewing round DM:

- DM's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the 138 eager table calls DM carried (DM1-3; banks and dumps
  `dm/out/t9*.json.gz` and `dm/out/t9*.jsonl` in the harness, replay with
  `dm/trq.sh TAG`): the variants filters' sort before pl.col of a set,
  list or scalar activity key; dict bounds holding 128-bit ints in the
  case filters (`cannot perform '<=' comparison ... struct[1]`); a set
  bound in `filter_on_ncases` before the empty key's join; the between
  separator after `is_in`; attribute value counts and relative occurrence
  reaching collect() before a list or dict key's hash; LTL case keys past
  Int128 before the sort; one native refusal in
  `attr_value_different_persons`;
- eager sequence probes over the `literal`-column domains (round DK's
  `dk/dom/_probe_plseq_lit_domain.py`) and empty eager frames;
- maybe the majority outcome of the partition race (DF1-7, seen again in
  DI, DJ and DK; the reference itself varies);
- the rest of the CU list (equal-size joins, CJ/CF/CD items); Duration,
  Time, Decimal and Int128 column dtypes are not in any Polars domain yet
  (the table transport has no cells for them);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DL's targets, taken in DM (findings DM1-1 to DM1-4): DL's CI run, the
fresh seeds, the Polars sequence filters on eager DataFrames and the eager
table probes (set, big-int, `literal`-column, empty frames; part carried).
Not taken (carried to DN): the partition race, the CU list and the open
choices.

Previous list for round DM (kept for its older items):

Next round (DM) should start by re-reviewing round DL:

- DL's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- more eager table probes: the set, big-int, None/NaN/datetime and
  `literal`-column probes of earlier rounds on eager frames (so far only
  the plain table domains ran eagerly), and empty eager frames;
- the Polars sequence filters on eager DataFrames (the sequence bank driver
  does not call the `EventTable` overloads for eager cases yet);
- maybe the majority outcome of the partition race (DF1-7, seen again in
  DI, DJ and DK; the reference itself varies);
- the rest of the CU list (equal-size joins, CJ/CF/CD items); Duration,
  Time, Decimal and Int128 column dtypes are not in any Polars domain yet
  (the table transport has no cells for them);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DK's targets, taken in DL (findings DL1-1 to DL1-3): DK's CI run, the
fresh seeds, the 325 eager LTL calls and the le1 refusal, and eager probes
of the other Polars table filters. Not taken (carried to DM): the partition
race, the CU list and the open choices.

Previous list for round DL (kept for its older items):

Next round (DL) should start by re-reviewing round DK:

- DK's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the 325 eager LTL calls of probe `dk/le2.py` (dump `dk/le2-carried.jsonl`
  in the harness): on an eager frame the first activity's filter runs
  before the [case, resource] step (`cannot compare string with numeric
  type`, list activity casts), a non-str resource key is a literal named
  `literal` in that step, and four_eyes' resource suffix TypeErrors come
  after it; then eager probes of the other Polars table filters (only the
  LTL module calls the eager overload in the bank driver so far);
- the one le1 call still refused (a None item beside present names in an
  eager four_eyes list key);
- maybe the majority outcome of the partition race (DF1-7, seen again in
  DI, DJ and DK; the reference itself varies);
- the rest of the CU list (equal-size joins, CJ/CF/CD items); Duration,
  Time, Decimal and Int128 column dtypes are not in any Polars domain yet
  (the table transport has no cells for them);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DJ's targets, taken in DK (findings DK1-1 to DK1-5): DJ's CI run, the
fresh seeds, the 917 LTL calls on `literal` frames, and LTL calls on eager
frames (a first pass, beyond the `literal` items). Not taken (carried to
DL): the partition race, equal-size joins and the older CJ, CF and CD items.

Previous list for round DK (kept for its older items):

Next round (DK) should start by re-reviewing round DJ:

- DJ's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the 917 LTL calls on frames with their own `literal` column that the
  native still refuses (probe `dj/lt7.py`, dump `cp/out/lt7d.jsonl` in the
  harness): list keys with a `literal` item (join keys of mismatched
  dtypes, `literal_right` repeats, missing names against `[literal,
  resource]`), scalar literal keys in eventually_follows (the
  `literal_right` repeat one join later) and in four_eyes and
  attr_value_different_persons (the frame's column beside the appended
  literal);
- LTL and other filters in eager calls with a `literal` list item (still
  refused; the bank tests only call the lazy overloads);
- maybe the majority outcome of the partition race (DF1-7, seen again in
  DI and DJ; the reference itself varies);
- the rest of the CU list (equal-size joins, CJ/CF/CD items); Duration,
  Time, Decimal and Int128 column dtypes are not in any Polars domain yet;
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DI's targets, taken in DJ (findings DJ1-1 to DJ1-5): DI's CI run, the
fresh seeds, the 113 scalar-key sequence calls, the per-case attribute
counts on frames with a `literal` column or no rows, and a first pass at the
`literal` item on frames with a `literal` column (LTL). Not taken (carried
to DK): eager `literal` items, the partition race, equal-size joins and the
older CJ, CF and CD items.

Previous list for round DJ (kept for its older items):

Next round (DJ) should start by re-reviewing round DI:

- DI's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the 113 scalar-key sequence calls DI carried (`out/s4-scal-plseq*.jsonl`
  in the harness): datetime keys (77; route them to the `PolarsFilterValue`
  overloads and model the datetime literal in the sort and group_by
  plans), None or NaN case keys in the start/end activity, prefix,
  suffix, rework, trace and variants calls, NaN `case_id_glue` in the
  nocc filter;
- per-case attribute counts over a key list on frames with a `literal`
  column or no rows, and a list item inside such a key (still refused);
- the `literal` list item where the frame has a `literal` column, and in
  eager calls (still refused);
- maybe the majority outcome of the partition race (DF1-7);
- the rest of the CU list (equal-size joins, CJ/CF/CD items); Duration,
  Time, Decimal and Int128 column dtypes are not in any Polars domain yet;
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DH's targets, taken in DI (findings DI1-1 to DI1-4): DH's CI run,
the fresh seeds, the 161 sequence big-int calls and the attribute counts
over list keys. Not taken (carried to DJ): the `literal` item with a frame
`literal` column or eager, the partition race, equal-size joins and the
older CJ, CF and CD items.

Previous list for round DI (kept for its older items):

Next round (DI) should start by re-reviewing round DH:

- DH's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the 161 sequence big-int calls DH carried (`out/s2-big-plseq*.jsonl`
  in the harness): scalar keys that are not str (`pl.col(int)` fails
  `invalid input for col`, a group or sort key is a literal; the str
  overloads read them as column names today) and ints past Int128 in
  values and parameters built as `pl.lit` (ends_with and prefix values,
  rework `min_occurrences`, nocc case-key lists);
- attribute counts over list and tuple keys (probe `dh/specs/a1.py`, 636
  of 880 calls refused natively; observations in DH1-5);
- the `literal` list item where the frame has a `literal` column, and in
  eager calls (still refused);
- maybe the majority outcome of the partition race (DF1-7);
- the rest of the CU list (equal-size joins, CJ/CF/CD items); Duration,
  Time, Decimal and Int128 column dtypes are not in any Polars domain yet;
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DG's targets, taken in DH (findings DH1-1 to DH1-6): DG's CI run, the
fresh seeds, the big-int LTL/between/timestamp/variants calls and a first
big-int probe of the sequence filters. Not taken (carried to DI): the
`literal` item with a frame `literal` column or eager, the partition race,
equal-size joins and the older CJ, CF and CD items.

Previous list for round DH (kept for its older items):

Next round (DH) should start by re-reviewing round DG:

- DG's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- ints past the Int64 range in the LTL, between, timestamp and variants
  filters (the 896 calls DG dropped from the big-int probes,
  `dom/_probe_pltbl*_big_domain.py` seed 1): native refusals today; the
  `pl.lit` refusal past Int128 comes before the column and join checks;
- the Polars sequence filters with ints past Int64 (no big-int probe yet);
- the `literal` list item where the frame has a `literal` column, and in
  eager calls (still refused);
- maybe the majority outcome of the partition race (DF1-7);
- the rest of the CU list (equal-size joins, CJ/CF/CD items); Duration,
  Time, Decimal and Int128 column dtypes are not in any Polars domain yet;
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DF's targets, taken in DG (findings DG1-1 to DG1-8): DF's CI run, the
fresh seeds, the Int128 literals and the third-seed set probes on CJ, CI and
CH. Not taken (carried to DH): the `literal` item with a frame `literal`
column or eager, the partition race, equal-size joins and the older CJ, CF
and CD items.

Previous list for round DG (kept for its older items):

Next round (DG) should start by re-reviewing round DF:

- DF's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- Int128 literals in the Polars filters (probe `df/specs/i1.py`, 409
  native refusals): scalars in [-2**63-1 .. -2**127] and [2**64 ..
  2**127-1] as Int128 literals in the numeric bounds, `is_in` lists of
  ints past UInt64 as Int128/UInt128 Series (`int value too large for
  Polars integer types` past u128), comparisons folded to
  `when(col.is_not_null())` and the path performance printout with an
  Int128 bound;
- the `literal` list item where the frame has a `literal` column, and in
  eager calls (still refused);
- third-seed set probes on the CJ, CI and CH table domains (seed 8);
- maybe the majority outcome of the partition race (DF1-7): a cast error
  in a later partition beat an earlier partition's comparison error in
  25 of 30 runs;
- the rest of the CU list (equal-size joins, CJ/CF/CD items); Duration,
  Time, Decimal and Int128 column dtypes are not in any Polars domain yet;
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DE's targets, taken in DF (findings DF1-1 to DF1-6): DE's CI run,
the fresh seeds, the `literal` items and two Nones beside the resource,
and the second-seed set probes on CK, CN, CP, CK and CJ. Partly taken:
ints past Int64 (the refusals above). Not taken (carried to DG):
equal-size joins and the older CJ, CF and CD items.

Previous list for round DF (kept for its older items):

Next round (DF) should start by re-reviewing round DE:

- DE's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the LTL list keys still refused (probe `de/specs/lt5.py`): a str item
  named `literal`, which meets the key's own Series literal column (306
  calls), and two Nones beside the resource in four_eyes (`already
  joined on literal and literal`);
- second-seed set probes on the CK and CN table domains and the CP, CK
  and CJ sequence domains;
- the rest of the CU list (equal-size joins, CJ/CF/CD items);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DD's targets, taken in DE (findings DE1-1 to DE1-9): DD's CI run,
the fresh seeds, the LTL list-key shapes DD listed and the second-seed
set probes on CJ, CI and CH. Not taken (carried to DF): the `literal`
items above, equal-size joins and the older CJ, CF and CD items.

Previous list for round DE (kept for its older items):

Next round (DE) should start by re-reviewing round DD:

- DD's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the LTL list-key shapes still refused (probe `dd/specs/lt2.py`, 440
  calls, and lt1's 24): `eventually_follows` with a timestamp key that is
  no str and no activity, a negative sign, a float, a bool or a
  two-name list; None beside str items in `eventually_follows` with two
  or more activities (`DuplicateError ... <name>_right already exists`
  through a `_POLARS_1` key), None beside a missing name and three-item
  keys with None; `four_eyes_principle` with None beside the resource
  (values and `literal_right`/`<resource>_right` duplicates) and a
  repeated resource;
- second-seed set probes on the busiest table domains (CJ, CI, CH), now
  that every filter domain has one;
- the rest of the CU list (equal-size joins, CJ/CF/CD items);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DC's targets, taken in DD (findings DD1-1 to DD1-8): DC's CI run,
the fresh seeds, set probes on the five domains, the several-column
shapes and most LTL list-key cases. Not taken (carried to DE): the LTL
list-key shapes listed above, equal-size joins and the older CJ, CF and
CD items.

Previous list for round DD (kept for its older items):

Next round (DD) should start by re-reviewing round DC:

- DC's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- set probes on the domains still without one (the Polars CJ, CF and CG
  table domains, the Polars CH sequence domain, the pandas CN LTL domain
  with `four_eyes_principle` and `attr_value_different_persons`;
  `review-harness/round-dc/tools/setcap.sh` with a new
  `dom/_probe_*_set_domain.py` patched by `dom/setcommon.py`);
- the several-column shapes left refused: `filter_df_on_attribute_values`
  and the stream filter keys over several columns, several attribute
  names beside a single paths target, dicts holding a set (a struct with
  an Object field);
- the rest of the CU list (LTL list-key cases, equal-size joins, CJ/CF/CD
  items);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DB's targets, taken in DC (findings DC1-1 to DC1-11): DB's CI run,
the fresh seeds, set probes on the six domains, the keyword helpers' set
keys, the set-order artefact (harness), `filter_variants_top_k` one-row
second columns (declared). Not taken (carried to DD): the rest of the CU
list.

Previous list for round DC (kept for its older items):

Next round (DC) should start by re-reviewing round DB:

- DB's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- fresh set probes on the domains not yet probed with sets (the Polars
  CN/CI/CH table and CK/CJ sequence domains, the pandas sequence CK
  domain; `review-harness/round-db/tools/setcap.sh` with a new
  `dom/_probe_*_set_domain.py`), and the set-key calls of the Polars
  keyword helpers `get_end_activities` and
  `filter_df_on_end_activities_nocc` if a native overload is wanted;
- the set-order artefact could be removed by capturing the worker's
  iteration order in the bank (a harness change, not a model one);
- the rest of the CU list (`filter_variants_top_k` one-row second
  columns, LTL list-key cases, equal-size joins, CJ/CF/CD items);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round DA's targets, taken in DB (findings DB1-1 to DB1-4): DA's CI run,
the fresh seeds, the rest of the Polars set probes and fresh set probes of
the pandas table and LTL domains (plus the Polars CK table and CP
sequence domains). Not taken (carried to DC): the rest of the CU list.

Previous list for round DB (kept for its older items):

Next round (DB) should start by re-reviewing round DA:

- DA's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the rest of the Polars set probes (`review-harness/round-da/`,
  `tools/setcap.sh plseq|pltbl`): lists holding a set as `pl.lit` builds
  them (`probe2/objlit.py`: `Nested object types`, the supertype pairs
  with `object`, the String builder's `found value of type Object`), set
  bounds of the numeric, case-size and case-performance comparisons, set
  timestamp keys in the variant filters' sort lists, set keys in the
  table filters' selectors; then fresh set probes of the pandas table
  and LTL domains;
- the rest of the CU list (`filter_variants_top_k` one-row second
  columns, LTL list-key cases, equal-size joins, CJ/CF/CD items);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round CZ's targets, taken in DA (findings DA1-1 to DA1-4): CZ's CI run,
the fresh seeds, sets in the sequence filters (pandas complete, Polars
first pass) and the paths list case keys repeating a name. No upload
route was left for the side-file readers item. Not taken (carried to DB):
the rest of the Polars set probes and of the CU list.

Previous list for round DA (kept for its older items):

Next round (DA) should start by re-reviewing round CZ:

- CZ's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- sets and frozensets in the pandas and Polars sequence filters (their
  value models and bank codecs have no set kind yet) and in the Polars
  table filters' `pl.lit` model;
- upload routes whose relation or side files pm4py reads with other
  readers than `format_dataframe` (CZ found none left among the CSV
  routes; the OCEL and streaming routes have their own readers);
- the CU list (paths list case keys repeating a name,
  `filter_variants_top_k` one-row second columns, LTL list-key cases,
  equal-size joins, CJ/CF/CD items);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round CY's targets, taken in CZ (findings CZ1-1 to CZ1-3): CY's CI run,
the fresh seeds, the `read_csv` upload routes (timestamp interleavings)
and set values in the pandas bank codec. Not taken (carried to DA): the
rest of the CU list.

Previous list for round CZ (kept for its older items):

Next round (CZ) should start by re-reviewing round CY:

- CY's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- other routes that read an upload with `read_csv` in pm4py rather than
  `format_dataframe` (compare each route's reference call with the reading
  the server uses; `read_csv_reference_table` now gives that frame);
- the CU list below (paths list case keys repeating a name,
  `filter_variants_top_k` one-row second columns, set values in the pandas
  bank codec, LTL list-key cases, equal-size joins, CJ/CF/CD items);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round CX's targets, taken in CY (findings CY1-1 to CY1-3): CX's CI run,
the fresh seeds and the merge case relations sweep. Not taken (carried to
CZ): the CU list.

Previous list for round CY (kept for its older items):


- CX's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- merge case relations on uploads: the route deliberately reads both
  files as `read_csv` tables (parity doc, "These deliberately stay ... on
  the frame's rows"); a sweep against pm4py's case-relations merge on
  `read_csv` and `read_xes` frames is still owed;
- the CU list below (paths list case keys repeating a name,
  `filter_variants_top_k` one-row second columns, set values in the pandas
  bank codec, LTL list-key cases, equal-size joins, CJ/CF/CD items);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round CW's targets, taken in CX (findings CX1-1 to CX1-4): CW's CI run,
the fresh seeds and the upload routes log util, timestamp interleavings
and organizational SNA. Not taken (carried to CY): merge case relations
and the CU list.

Previous list for round CX (kept for its older items):

- CW's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the upload routes still on Minera's log: log util, merge case
  relations, timestamp interleavings, organizational SNA (passed time was
  swept in CW: equal);
- the CU list below (paths list case keys repeating a name,
  `filter_variants_top_k` one-row second columns, set values in the pandas
  bank codec, LTL list-key cases, equal-size joins, CJ/CF/CD items);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round CV's targets, taken in CW (findings CW1-1 to CW1-8): CV's CI run,
the three carried calls, the fresh seeds, `dict(pairs)` in every decoder
and the upload routes rework, cycle time, case arrival, boundary
activities and chaotic activities (passed time swept, equal). Not taken
(carried to CX): log util, merge case relations, timestamp
interleavings, organizational SNA and the CU list.

Previous list for round CW (kept for its older items):

- CV's CI run;
- the three carried calls of fresh Polars CK 2026111913: case 117
  (`eventually_follows` with a dict activity and a dict timestamp key: the
  struct comparison fails before the literal sort), cases 410 and 650
  (`filter_traces_attribute_in_timeframe` with a dict timestamp key
  naming the case column: an empty result on Int64 with a dict value, the
  categorical comparison error on Enum with a None value);
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- repeated dict keys (`dict(pairs)`) in the other three bank decoders
  (pandas table, pandas sequence, Polars sequence), with a replay of every
  docs bank;
- upload routes still on Minera's log rather than the frame: rework
  (`/api/rework`, pm4py's rework statistics on the frame), chaotic
  activities, passed time, cycle time, case arrival, boundary activities,
  log util, merge case relations, timestamp interleavings, organizational
  SNA (compare the server's route list with the sweeps named in the round
  notes; CR-CV swept label splitting, prefix tree, interval tree, link
  analysis, trace-attribute clustering, process cube, SaCoFa, transform
  targets and concept drift);
- the CU list below (paths list case keys repeating a name,
  `filter_variants_top_k` one-row second columns, set values in the pandas
  bank codec, LTL list-key cases, equal-size joins, CJ/CF/CD items);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round CU's targets, taken in CV (findings CV1-1 to CV1-6): CU's CI run,
the fresh seeds and the upload routes (concept drift). Not taken (carried
to CW): paths list case keys repeating a name, `filter_variants_top_k`
one-row second columns, set values in the pandas bank codec, the LTL
list-key cases outside the model, equal-size joins and the older CJ, CF
and CD items.

Previous list for round CV (kept for its older items):

- CU's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the paths filters with a list case key that repeats a name (pm4py's
  `DuplicateError` plan) and `filter_variants_top_k` with several key names
  on one-row frames whose second column is a List, Struct, Binary,
  Decimal, Duration or Time column;
- set values in the pandas bank codec (not taken in CU; the drivers cannot
  carry them yet);
- upload routes no round has swept on typed frames yet (CU found the
  transform-target route; compare the server's route list with the sweeps
  named in the round notes);
- the CS list below (LTL list-key cases, equal-size joins, CJ/CF/CD items);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round CT's targets, taken in CU (findings CU1-1 to CU1-9): CT's CI run,
the fresh seeds, one-row NaN joins in the other filters, the list glue of
`filter_df_on_end_activities` and the upload routes (transform targets).
Not taken (carried to CV): set values in the pandas bank codec, the LTL
list-key cases outside the model, equal-size joins and the older CJ, CF and
CD items.

Previous list for round CU (kept for its older items):

- CT's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- one-row NaN joins in the filters CT did not probe (LTL, paths, between,
  `ncases`) and the other one-row join shapes (two float keys, an
  Int/Float key pair);
- `filter_df_on_end_activities` with a list `case_id_glue` keyword
  (outside the model, probe s2);
- set values in the pandas bank codec (the drivers cannot carry them yet);
- upload routes no round has swept on typed frames yet (compare the
  server's route list with the sweeps named in the round notes);
- the CS list below (LTL list-key cases, equal-size joins, CJ/CF/CD items);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round CS's targets, taken in CT (findings CT1-1 to CT1-8): CS's CI run,
the fresh seeds, the process cube and anonymization sweeps, and int and
datetime range folding in the other Polars filters. Not taken (carried to
CU): the LTL list-key cases outside the model, equal-size joins and the
older CJ, CF and CD items; the embeddings route cannot be compared against
the pinned reference.

Previous list for round CT (kept for its older items):

- CS's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- sweeps of the upload routes not yet swept on typed frames (embeddings,
  the process cube, anonymization);
- int and datetime range folding in the other Polars filters (CS found it
  in `filter_case_performance`; the paths filter folds str bounds only);
- the CR list below (LTL list-key cases, equal-size joins, CJ/CF/CD items);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round CR's targets, taken in CS (findings CS1-1 to CS1-7): CR's CI run,
the fresh seeds, one-row `A_next_B_next_C` list keys, and trace-attribute
clustering on uploads. Not taken (carried to CT): embeddings, the process
cube and anonymization sweeps, the LTL list-key cases outside the model,
equal-size joins and the older CJ, CF and CD items.

Previous list for round CS (kept for its older items):

- CR's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- one-row `A_next_B_next_C` with a list case key of str, int or bool
  items, a tuple, or None (probe `probe/cr/ln1.py` has the reference's
  answers), still refused;
- sweeps of other upload routes on typed frames (embeddings, log
  clustering, the process cube, anonymization);
- the CR list below (LTL list-key cases, equal-size joins, CJ/CF/CD items);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round CQ's targets, taken in CR (findings CR1-1 to CR1-7): CQ's CI run,
the fresh seeds, and new areas (label splitting, prefix tree, interval
tree and link analysis on uploads). Not taken (carried to CS): the LTL
list-key cases outside the model, equal-size joins and the older CJ, CF
and CD items.

Previous list for round CR (kept for its older items):

- CQ's CI run;
- fresh seeds of the CK, CN, CJ, CI and CH table domains, the CP, CK, CJ
  and CH sequence domains and the pandas CK and CN domains;
- the LTL list-key cases left outside the model: str items naming the
  resource, `count`, `_index` or the timestamp column, None next to str
  items, a non-column timestamp key, and composite `eventually_follows`
  keys with null or NaN cells;
- inner joins of two equal-size sides (row order from a per-process hash
  seed) stay unordered unless Polars' hashing is modelled;
- the older items of the CQ list below (CJ, CF and CD items);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round CP's targets, taken in CQ (findings CQ1-1 to CQ1-6): CP's CI run,
the three `apply_numeric` calls, the k6 probe, LTL list case keys of
several or str items, four or more `four_eyes_principle` activities with a
literal key, the fresh seeds and the CK1-1 calls. Not taken (carried to
CR): the older CJ, CF and CD items (see the list below).

Previous list for round CQ (kept for its older items):

- CP's CI run;
- the three `apply_numeric` calls with struct-cast error order (CM seed
  2026111550 case 190, CK seed 2026111590 case 46, CJ seed 2026111595
  case 451; full banks in `review-harness/round-cm/banks/` and
  `round-cp/fresh/`). The conjuncts fail in an operator-dependent order
  (`A & D` gives D's error, `D & A` A's, `D | A` D's, null bounds fold
  in one filter and split across sequential ones); a right-first model
  broke 360 bound calls, so left-first stays until a model fits every
  bank;
- list activity keys naming two columns in the Polars sequence filters
  (probe `k6`, 18 calls, in `review-harness/round-cp/probe/`);
- LTL list case keys of several items or of str items and four or more
  `four_eyes_principle` activities with a literal case key (see the CP
  list below);
- fresh seeds of the CK, CN, CJ, CI and CH domains and of the CP
  sequence domain;
- the three CK1-1 calls: CJ table case 214 (`A_next_B_next_C`), CJ
  sequence case 36 (`traces.apply`, the Null case column join error), CB
  sequence case 431 (`rework.apply` with `MIN_OCCURRENCES` 1 keeps the
  rows in another order);
- still open from CJ's list: Int128 literals, `list[u64]` with a
  datetime struct field, a `bytes` anchor on a Null column, the items
  carried from CF (`coerced_scalar_is_in` on str and Categorical/Enum
  columns, `list_literal` with wide ints after a float and in nested
  lists, `lit_supertype` of Int128 with temporal types, ints past the
  UInt64 range as paths bounds, `plan_text_at` inside a merged node) and
  from CD (Duration, Time, Decimal and Int128 columns, `>`/`<` on
  Categorical and Enum columns with unknown strs, pre-1970 dates on ns
  columns, `integer_supertype` widths, the sequence filter helpers, the
  pandas sequence filter items and the items carried from CB);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round CO's targets, taken in CP (findings CP1-1 to CP1-5): CO's CI run,
the Polars sequence calls (23 → 0), the fresh pandas seeds (6 → 0), the
Polars table leftovers (17 → 3), fresh CK, CJ, CI and CH seeds. Not
taken (carried to CQ): the three `apply_numeric` calls, LTL list case
keys of several or str items, four or more `four_eyes_principle`
activities with a literal key, the CK1-1 calls and the older items (see
the CQ list above).

Previous list for round CP (kept for its older items):

- CO's follow-up CI run;
- the Polars sequence calls (9 of seed 2026111532, 6 of 2026111551, 5
  of 2026111561, 2 of the fresh 2026111581, 1 of the fresh CJ
  2026111586; full banks in `review-harness/round-cl/banks/`,
  `round-cm/banks/`, `round-cn/banks/` and `round-co/fresh/`): list case
  keys holding a dict in `ends_with`, `prefixes`, `suffixes`, `traces`,
  `rework`, `start_activities`, `end_activities`, `activity_split` (the
  sort and `group_by` read them as literals; the native missing-column
  message names "" today); `polars_group_keys.hpp` may carry over;
- the fresh pandas seeds CO captured but did not replay (CK table
  2026111582, CK sequence 2026111583, CN LTL 2026111584, in
  `review-harness/round-co/fresh/`);
- Polars table leftovers: `A_next_B_next_C` with a dict attribute key
  (seed 2026111560 cases 60 and 289, CM seed 2026111550 cases 37 and
  301: the reference fails the final join on an empty selection before
  it reads the key), CM seed 2026111550 cases 190 (`apply_numeric`), 300
  (`four_eyes_principle`, list case key build before the activity list),
  22, 159, 182 and 380 (`variants` with list case or timestamp keys
  holding a dict); LTL list case keys of several items or of str items
  (`ColumnNotFoundError` on the `literal` select, `joining with repeated
  key names`, duplicate `literal` outputs; probe `lit1`/`p13` outputs in
  the harness); four or more `four_eyes_principle` activities with a
  literal case key (the failing pair's plan);
- fresh seeds of the CK, CN, CJ, CI and CH domains;
- the three CK1-1 calls and the older items listed for CO below
  (Int128 literals, CF and CD carry-overs);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers).

Round CN's targets, taken in CO (findings CO1-1 to CO1-7): CN's CI run,
the Polars table calls (40 → 0, fresh seed 60 35 → 2), the CM fresh
table seed `cases.filter_on_ncases`/timeframe classes, fresh CK, CH, CI,
CJ seeds. Not taken (carried to CP): the Polars sequence calls, the
pandas fresh seeds, the A_next dict attribute calls, the three CK1-1
calls and the older items (see the CP list above).

Round CM's targets, taken in CN (findings CN1-1 to CN1-8): CM's CI run,
fresh seeds of the CK, CJ, CI and CH domains, the Polars table calls
(65 → 40, fresh seed 64 → 35), the Polars sequence calls (17 → 9 and
17 → 6), the pandas table case 501. Not taken (carried to CO): the CM
fresh table seed leftovers, the three CK1-1 calls and the older items
(see the CO list above).

Round CL's targets, taken in CM (findings CM1-1 to CM1-8): CL's CI run,
fresh seeds of the CK, CJ, CI and CH domains, the Polars table dict keys
(497 → 65), the Polars sequence calls (fresh seed leftovers fixed, the 17
carried classes open), the pandas table fresh seed. Not taken (carried
to CN): the pandas table case 501, the three CK1-1 calls and the older
items (see the CN list above).

Round CK's targets, taken in CL (findings CL1-1 to CL1-4): CK's CI run,
the pandas sequence dict bank (fixed), the Polars sequence bank (17
carried), the Polars table bank (497 carried). Not taken (carried to CM):
fresh seeds of the CJ, CI and CH domains, the pandas table case 501, the
three CK1-1 calls and the older items (see the CM list above).

Round CJ's targets, taken in CK (findings CK1-1 to CK1-8): CJ's CI run,
fresh seeds of the CJ, CI, CH, CG, CF and CE domains, dict case ids and
dicts in the pandas table filters, dict items and parameters and
Categorical/Enum prefix plans in the Polars sequence filters (domains
written, findings carried). Not taken (carried to CL): see the CL list
above.

Round CI's targets, taken in CJ (findings CJ1-1 to CJ1-10): CI's CI run,
fresh seeds of the CI, CH, CG, CF and CE domains, dict values in the
sequence filters, `between.apply`, the case filters' bounds,
`filter_df_relative_occurrence_event_attribute`, `apply_numeric` and the
paths items on non-String columns, the `[[1], [{"a": 1}], 5]` message,
struct items of `pl.lit` lists against typed columns. Not taken (carried
to CK): see the CK list above.

Round CH's targets, taken in CI (findings CI1-1 to CI1-8): CH's CI run,
fresh seeds of the CH, CG, CF and CE domains, the float-as-text sites,
lists of dicts as dict fields. Not taken (carried to CJ): see the CJ list
above.

Round CG's targets, taken in CH (findings CH1-1 to CH1-7): CG's CI run,
fresh seeds of the CG, CF and CE domains, Polars' Float to String cast,
nested dict fields, `literal_list_key` and wide ints on `Float32`. Not
taken (carried to CI): see the CI list in Review CH1.

Round CF's targets, taken in CG (findings CG1-1 to CG1-6): CF's CI run,
fresh seeds of the CF and CE domains, timeframe dict values beyond String
columns, list bounds of `validate_bound` in the case filters and
`filter_on_case_size`'s merges. Not taken (carried to CH): see the CH
list above.

Round CE's targets, taken in CF (findings CF1-1 to CF1-9): CE's CI run,
fresh seeds of the CE domain, edge values in the between anchors and
timestamp attribute values, more numeric bounds, `coerced_scalar_is_in`
on Datetime and Date columns, float display past the sampled thresholds,
paths bounds before a failing one, timeframe dict values and all-str
lists. Found unreachable: `validate_bound` with several paths
(`paths.apply_performance` takes one path) and more than two stream
filters (`apply_numeric` reads only `stream_filter_key1` and `key2`).
Not taken (carried to CG and then CH): see the CH list above.

Round CD's targets, taken in CE (findings CE1-1 to CE1-10): CD's CI run,
the six open calls of Review CD1, Boolean and Null activity columns,
ints past 2**53 and the Int64 range (now in every table filter), floats
past 1e16, dates before 1970 on ms columns, the interval merge with mixed
literals and cast columns, list values on Boolean and temporal attribute
columns, and fresh seeds. Not taken (carried to CF, CG and then CH): see the
CH list above.

Round CC's targets, taken in CD (findings CD1-1 to CD1-7): the Polars
table filters on typed keys and CC's CI run. Not taken (carried to CE):

- `src/backend/polars_lit_model.hpp` (moved from
  `src/backend/polars_sequence_filters.cpp` in CD): `lit_supertype` and
  `cast_lit` (Duration, Time, Decimal and Int128 columns, `UInt8` and
  `Int16` with dates, nested lists of lists), `build_strict` (nested nulls,
  empty lists), `comparable` (Categorical and Enum columns against dates),
  `column_value_text` (ns columns before 1970, fixed-offset zones), and
  the `evaluate` hook of `lazy_variant_filter` (errors of later patterns);
- `src/backend/pandas_sequence_filters.cpp`: `datetime_list_converts`
  (aware lists of two offsets that are one zone, `numpy.datetime64` and
  `pd.Timestamp` items, `date` items), `ambiguous_wall` (zones whose DST
  shift is not one hour, the column's ns unit), `float_keys` on `UInt`
  and `boolean` columns, the activity split's datetime rule on aware
  columns of other zones and on `datetime64[s]` with fractions;
- carried over from CB's list: `check_is_in` on Duration, Time, Decimal
  and Int128 columns and `cast_to` parsing str to Datetime and Date; the
  pandas case grouping with `timedelta64` partners; the OCPN playout's
  dead-state memo with `objects_unique_per_trace`, two bindings and IMd
  models, the visitor's stop at `max_traces` with `return_sequences`; the
  other Polars modules that build `is_in` filters or variant lists (DFG
  and statistics on Categorical and Enum activities); pandas period and
  interval keys (no cell in the table transport; declare or add a
  transport kind).

Round CB's targets, taken in CC (findings CC1-1 to CC1-4):

- `src/backend/polars_sequence_filters.cpp`: `list_literal` (mixed naive
  and aware datetimes, nested lists, values of other types),
  `check_is_in` and `literal_dtype_name` (Duration, Time, Decimal and
  Int128 columns, a Date list on Datetime data), `literal_datetime_text`
  (fixed offsets, dates before 1970, DST gaps), `coerced_is_in`,
  `cast_to` (str to Datetime and Date parsing), `string_cast` (ns
  Datetime, Time, Duration), `python_value` and `same_key` in the end
  activity counts;
- `src/backend/pandas_sequence_filters.cpp`: `datetime_run_key` (object
  partners of mixed elements, `timedelta64` partners, s and ms units),
  the datetime branch of `pandas_activity_split_apply` (aware columns of
  other zones, mixed units), the ns rule in rework and `nocc` (us, ms and
  s keys), `counts_value` and `matches_value` on datetime keys;
- `src/backend/ocpn_reference_playout.cpp`: the dead-state memo with
  `objects_unique_per_trace`, two bindings and IMd models; the visitor's
  stop at `max_traces` with `return_sequences`; the route's memory under
  the server's limits;
- carried over from CB's list: the Polars table filters with Datetime,
  Date and Int activity columns (`scripts/polars_filter_domain.py` has no
  `--domain` option yet); the other Polars modules that build `is_in`
  filters or variant lists (DFG and statistics on Categorical and Enum
  activities); the narrow Polars dtypes (`Int8` to `UInt32`, `Float32`,
  with nulls) in the table filters; pandas period and interval keys (no
  cell in the table transport; declare or add a transport kind);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers);
- the CI run of the CB commit.

Round CA's targets, taken in CB (findings CB1-1 to CB1-3):

- `src/backend/polars_sequence_filters.cpp`: `short_dtype`, `enum_is_in`
  and its plan texts, `require_string_activities`, `CellOrder` (Enum
  ranks, floats, Booleans), the Enum cast in `list_filter`, the case list
  dtype in `join_case_list`; the same rules in the other Polars modules
  that build `is_in` filters or variant lists (table filters on Enum
  activities, DFG and statistics on Categorical/Enum activities);
- `src/backend/pandas_sequence_filters.cpp`: the CA branches in
  `consecutive_act_case_grouping`, `position_filter`, `counts_value`,
  the `nocc` NA check and `pandas_activity_split_apply`;
- the narrow integer dtypes in the Polars table filters (`Int8` to
  `UInt32`, `Float32`, with nulls; error texts that print the dtype);
- `ocel_statistics.cpp` `edge_associations` and `ocel.cpp`
  `describe_ocel_tuples` with repeats; the other OCEL statistics and
  routes that count relation rows (activity and object type associations,
  enrichment, filters with repeats);
- the OCPN playout route without `max_traces`: response size (the OCEL log
  is serialized next to the traces) and the explicit bound; the reference
  search's speed (one sweep log: 756,756 sequences in 574 s against
  pm4py's 78 s, while another with 728,112 takes 24 s against 63 s;
  pm4py seems to skip dead ends that `ocpn_reference_playout.cpp` explores);
- new areas: the Polars sequence and table filters with Datetime, Date and
  Int activity columns; the pandas sequence filters with `datetime64`,
  period and interval keys; the OCPN playout with
  `objects_unique_per_trace` and IMd models with repeats;
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers);
- the CI run of the CA commit.

Round BY's targets, re-reviewed in BZ (findings BZ1-1 to BZ1-10):

- `src/backend/pandas_paths_filter.cpp`: `typed_concatenation` (dtype
  names it does not list, such as `string[python]`, `Int64` with missing
  values, `object` columns of mixed types, `float16`);
- `src/backend/pandas_datetime_target.hpp` and its callers in the
  timestamp attribute and repetition filters (zones other than UTC,
  offsets, units other than ns and us);
- `src/backend/pandas_variants_filter.cpp`: `case_runs`, `variant_less`,
  `items_equal` with fresh NaN/NaT, the `split_hash` rule of `membership`,
  the coverage denominator, and the `variants_df` and `positive=false`
  paths with missing values;
- `src/backend/pandas_between_filter.cpp`: `numpy_digits` and
  `unordered_nat` (ms and s columns, NaT only cases);
- `src/backend/ocel.cpp`: the relation sort by instant rank in
  `sort_ocel_events` and the transport markers in `serialize_ocel_json` /
  `parse_ocel_json_names` (logs that routes pass on: filter, playout,
  split and enrich results; the XML and CSV writers);
- `src/backend/ocel_sqlite.cpp`: the change order (NaT or unreadable
  change times, the `object_changes` table branch) and
  `src/backend/ocel_transform.cpp`: lifecycle qualifiers per relation row;
- new areas: repeated `event_object` rows of an OCEL SQLite file through
  the OCEL 2.0 transport (it keeps one relation per event and object, OC2,
  while `read_ocel2_sqlite` keeps repeats; pm4py's lifecycle then labels
  both repeats `other`); the order of O2O rows across source objects in
  the OCEL 2.0 transport (nested per source; SQLite keeps `object_object`
  row order); OCPN and SAW discovery (`imd`, relation order) and the OCEL
  filters on SQLite logs with tied events; the pandas LTL and sequence
  filters with non-default keys; timedelta, categorical and nullable
  columns as keys on the pandas table filters;
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers);
- the CI run of the BY commit.

Round BX's targets, re-reviewed in BY (findings BY1-1 to BY1-9):

- `src/backend/polars_sort.hpp` (the pdqsort port: `partial_insertion_sort`,
  `choose_pivot`, `partition_in_blocks`, the 2,000-row split) and its
  callers in `polars_paths_filter.cpp`, `polars_between_filter.cpp`,
  `polars_variants_filter.cpp`, `polars_ltl_filter.cpp` (which still sorts
  stably when the case or timestamp column is missing) and
  `polars_sequence_filters.cpp`; `sort_by_keys` (stable when a key has the
  `Null` dtype: are other dtypes dropped or sorted differently, such as
  Object, List or Struct keys?); the tie sweep did not cover the LTL and
  sequence filters;
- `polars_datetime::cast_text` (zones other than UTC, negative offsets,
  cells before 1970, ms columns) and the String-cast branches of the paths
  and variants filters;
- `src/backend/polars_variants_filter.cpp`: the `DuplicateError` of top_k
  with the activity key on the case column (other operations with equal
  keys);
- `src/backend/polars_timestamp_filter.cpp`: `nanosecond_aware`,
  `at_most_text`, the Boolean `cast(String)` match and the temporal
  attribute errors (`attribute_resolution`);
- `src/backend/ocel.cpp`: `description_time` and the lifecycle duration
  of the description;
- new areas: the pandas table filters with non-default keys
  (`tblkeys.py --eng=pandas`, 200 seeds: 140 paths and 126 path
  performance error texts differ on number, boolean and datetime
  activities, where pandas raises the vectorised `UFuncTypeError: ufunc
  'add' did not contain a loop with signature matching types ...` or
  `TypeError: unsupported operand type(s) for +: 'DatetimeArray' and
  'str'`; path performance with the case key on the activity column
  raises `ValueError: Can only compare identically-labeled ... DataFrame
  objects`; case performance with the case key on the timestamp column
  raises `ValueError: Grouper for 'time:timestamp' not 1-dimensional`
  (39); top_k and between on a case column with NaT return results where
  Minera raises `'<' not supported between instances of 'NoneType' ...`
  (26 and 14); the timestamp attribute filter on a datetime attribute
  parses a string value and matches (14); repetition with the target
  `NaT` on a datetime column returns no rows (2)); the OCEL routes not yet
  swept on nanosecond SQLite timestamps (conformance, enrich with
  lifecycle qualifiers, ancestors and descendants splits, object changes,
  statistics);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers);
- the CI run of the BX commit.

Round BW's targets, re-reviewed in BX (findings BX1-1 to BX1-5):

- `src/backend/polars_datetime.hpp` (unit parsing, `floor`, `ticks`,
  `display`) and its callers in the Polars timestamp, case, paths,
  variants, between, LTL and attributes filters; in
  `polars_timestamp_filter.cpp` `floor_bound` and `at_most` (which
  comparisons floor on a ns column);
- `src/backend/pandas_paths_filter.cpp`: the exact-instant order of dates
  of one awareness in `compare`;
- `src/backend/ocel_features.cpp`: `event_times` (ns instants through
  `frame_times::epoch_seconds`) and its consumers (lifecycle, prefix time
  differences);
- `src/backend/ocel_sqlite.cpp` and `ocel_frame_types.hpp`: the
  `microseconds` flag of `utc_timestamp_text` and `offset_timestamp_text`
  (event times, change times, typed timestamp attributes);
- new areas: Polars' unstable multi-column sort of tied timestamps (paths,
  path performance, variants, between, LTL on frames with equal times in
  a case: find whether a deterministic model, such as the Rust unstable
  sort on Polars' row encoding, reproduces it; harness seeds 120, 133 and
  346 of `tbl.py --eng=polars`); Polars Datetime columns as activity or
  case keys (string casts print the column unit's fraction); Date and
  pandas `datetime64[s]`/`[ms]` columns on the paths and performance
  filters; the OCEL routes not yet swept on nanosecond SQLite timestamps
  (statistics, description, conformance, object changes);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers);
- the CI run of the BW commit.

Round BV's targets, re-reviewed in BW (findings BW1-1 to BW1-4):

- `src/backend/llm_context.cpp`: `frame_timestamp_context` (which keys
  and variants take the frame, `own_timestamp_key`, the check order after
  `model_frame_key`, the declared fallbacks returning `nullopt`),
  `frame_timestamped` and `frame_timestamp_check`;
- `src/backend/llm_abstractions.cpp`: `frame_stream_ranks` (mixed types,
  ties, nanosecond cells) and the event stream's `timestamp_key`;
- `src/backend/temporal_profile.cpp`: the new
  `discover_temporal_profile_frame` overload with a separate timestamp
  key;
- `src/backend/log_trace_filter.cpp`: `python_type` and the two-pass
  value read (list, dict and date values on XES uploads);
- new areas: the `/api/reference-table-*` routes on typed native
  EventTable JSON inputs (never swept); the OCEL routes other than the
  LLM context and the edge associations on nanosecond SQLite timestamps
  (`/api/ocel/filter` time ranges, `/api/ocel/features` timestamp and
  duration features, `/api/ocel/io` exports); the LLM context's timestamp
  attribute on XES uploads with date attributes that have gaps;
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers);
- the CI run of the BV commit.

Round BU's targets, re-reviewed in BV (findings BV1-1 and BV1-2):

- `src/backend/llm_context.cpp`: `model_frame_key` (which keys and
  variants take the frame, the error order, the case id KeyError) and
  `model_context`; the `ocel` variant's `instant` order and texts;
- `src/backend/frame_keys.cpp`: the new `FrameKeyScope` argument of
  `frame_keyed_activities` (only the LLM context passes `kDataFrame`; XES
  uploads' `case:` attributes now key the model descriptions);
- `src/backend/llm_abstractions.cpp`: `ocel_lifecycle_paths` and the
  `@@ocel_lif_path_` feature texts;
- `src/backend/tools/minera_server.cpp`: `audited_query_route` (only
  2xx answers are replaced; a parameter read on one path only is now
  refused on the others), the `audited_*` lookups in the helpers shared
  with unwrapped routes, the Monte Carlo and stochastic allow lists;
- new areas: the log filter bank (`/api/reference-log-filter`,
  `/api/reference-log-attributes`) on typed XES attributes and the
  `/api/reference-table-*` routes on typed native EventTable JSON inputs
  (never swept in rounds AO to BU); the LLM context with both an activity
  and a timestamp attribute; the OCEL routes other than the LLM context on
  nanosecond timestamps;
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers);
- the CI run of the BU commit (and BT's Linux and Windows ctest).

Round BT's targets, re-reviewed in BU (findings BU1-1 to BU1-4):

- `src/backend/dfg_variants.cpp`: `frame_keyed_dfg_statistics`,
  `frame_keyed_clean_dfg` (runs split at missing cells),
  `discover_clean_time_dfg` (int64 ticks, `KahanMean`, the nanosecond
  switch and its range guard), `dfg_variant_activities` and its callers in
  `minera_server.cpp` and `minera_export.cpp`;
- `src/backend/dfg_dataframe.cpp`: the `csv_frame` overload and its
  `thread_local` (only the pandas graph takes it; partial order and
  concurrent events keep the text);
- `src/backend/llm_abstractions.cpp`: `frame_activity_key`, the keyed DFG
  pairs, the variants' Python sort (`python_list_sort`, case order, typed
  comparisons), `describe_features`' `>=`; `llm_context.cpp`'s `frame_key`
  (which variants keep the projection);
- `src/backend/boundary_activities.cpp` and `variant_statistics.cpp`:
  `frame_activity_column_error` and the keyed frame;
- `src/backend/tools/minera_server.cpp`: `reject_unconsulted_controls` on
  the four upload routes (a control consulted only on some paths is now
  refused on the others), the check-model and compare-petri allow lists,
  `parse_playout_marking`'s form fields, replay fitness's default key;
- new areas: the session routes that read their controls from the query
  string and still ignore unknown parameters (`/api/align`, evaluation,
  decomposed and multi-alignments); typed keys on the filters
  (`reference-log-filter`, `reference-table-*`) and the remaining
  statistics (attribute statistics, case statistics); the LLM context's
  other variants keyed by a column; `clean_polars` on typed keys; the OCEL
  routes on nanosecond timestamps (not reached in BT);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers);
- the CI run of the BT commit.

Round BS's targets, re-reviewed in BT (findings BT1-1 to BT1-7):

- `src/backend/frame_keys.cpp`: `frame_keyed_activities` (the key's typed
  cells as `python::str` labels, `NaT`/`nan` for missing cells, the
  `non_text`, `any_text`, `distinct` and `first_non_text_type` flags) and
  the three error builders; the callers in `alpha_miner.cpp`
  (`alpha_frame_keyed_log`, the alpha plus cleanup alternatives),
  `heuristic_miner.cpp` (all-text keys now run on the keyed log and mark
  `activity_ids_are_local`), `inductive_miner.cpp`
  (`prepare_inductive_event_log`) and `powl_discovery.cpp`; the DFG,
  filters and other families keyed by a typed column were not swept;
- `src/backend/tools/minera_server.cpp`: `handle_compare_petri`'s
  `powl_key`; `reject_unread_multipart_parts` and `ocel_request_body`'s
  extra uploads on the 30 routes (routes that read controls from either the
  query or the form, such as replay fitness, token replay, the alignment
  workbench, check-model and compare-petri, still ignore unknown fields of
  both kinds);
- `src/backend/tools/pnml_model_options.hpp`: `stochastic_map_reachability`
  (which exceptions count as an unavailable CTMC; the CLI shares it);
- `src/backend/llm_abstractions.cpp`: `LogClock` (only CSV frame logs with
  a nanosecond timestamp cell use nanoseconds), `timestamp_repr`'s nanos,
  `numpy_int_quantile`;
- new areas: typed activity keys on the DFG routes, the filters and the
  statistics families (pm4py's frame keeps the typed cell); the replay and
  alignment upload routes' unknown fields; the OCEL routes on nanosecond
  timestamps; the LLM abstractions on XES uploads and with typed keys;
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers);
- the CI run of the BS commit.

Round BR's targets, re-reviewed in BS (findings BS1-1 to BS1-5):

- `src/backend/tools/minera_server.cpp`: `csv_activity_alias`,
  `model_key_field` and `supplies_model_field` on every model route (a key
  is hidden only when it names the selected column under either spelling
  and the model's key does too; routes that take an explicit key outside
  this list, such as the tree workbench or the BPMN and POWL views, were not
  swept), `miner_activity_key` for imported models, the compare-petri CSV
  reference read with the session's selectors (a reference whose headers
  differ from the upload's falls back to the standard headers), the DFG
  precision key mapped by `csv_frame_key`;
- `src/backend/batches.cpp` (`log_instant`, `pandas_nanoseconds`; a frame
  table without dtypes reads exact seconds), `log_to_features.cpp`
  (`python_epoch`, `csv_nanosecond_column` scans the projected cells once
  per key, `temporal_metric_instant` now parses nine decimals for a
  nanosecond source), `interval_tree.cpp`, `stochastic_map.cpp`
  (`token_time`, `elapsed_seconds`);
- new areas: date columns as activity keys across the miners (pm4py labels
  them `str()` of the UTC `Timestamp`: alpha, heuristics, Inductive, DFG,
  the filters), the stochastic-map route's reachability failure (return the
  map with the reachability as an optional section?), the remaining log
  families on nanosecond cells (sojourn and waiting time statistics outside
  the features, the rework and interval durations, the LLM abstractions'
  durations), routes on XES uploads with nanosecond dates, the multipart
  field handling of query-only routes such as `/api/dfg-precision`;
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers);
- the CI run of the BR commit.

Round BQ's targets, re-reviewed in BR (findings BR1-1 to BR1-7):

- `src/backend/performance_spectrum/components.cpp`: `order_key` (the
  exact instant from `parse_timestamp_exact`, or the double's split when
  the text does not parse exactly) in `sort_case_events`, `split_runs` and
  `sort_cases_by_first_instant`; `log_instant` (only nanosecond cells are
  rounded);
- `src/backend/temporal_profile.cpp`: `event_instant`'s `python_epoch`
  flag and its callers (`discover_temporal_profile_ranked` defaults it on,
  the DataFrame fallback passes false, `conform_log` passes `!timedelta`);
- `src/backend/correlation_miner.cpp`: `python_epoch_seconds` in
  `selected_timestamp` and `selected_event_timestamp`; the sort still uses
  the rounded floats and the input index (pandas' stable sort of the
  rounded column agrees on every swept shape; classic_split chunks with
  ties in one microsecond were not swept);
- `src/backend/tools/minera_server.cpp`: `inductive_log_for_key` (used by
  the tree reduction and the Petri comparison only; other routes on the
  cached Inductive log with an activity key, such as alignments and the
  tree workbench, were not checked on CSV headers);
  `src/backend/process_tree_reduction.cpp`'s `reference_selected_field`
  fallback;
- new areas: the other log families on nanosecond cells (sojourn and
  waiting times, Monte Carlo case arrival, the batches' log variant, the
  log's interval and rework durations); DFG precision and alpha keyed by a
  timestamp or lifecycle header; the Petri comparison with a CSV reference
  log under its own headers (the route requires the standard headers
  there); the CLI's other CSV modes against pm4py on the frame (the
  footprints, Declare and log skeleton discovery, the alignments);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers);
- the CI run of the BQ commit (green on all six jobs, as was BP's run
  43304285286, whose Windows ctest was still running when BQ closed).

Round BP's targets, re-reviewed in BQ (findings BQ1-1 to BQ1-6):

- `src/backend/performance_spectrum/components.cpp`: `frame_instant`
  (`pandas_units_per_second` reads only `EventTable::dtypes`; a LazyFrame,
  a stream table and a table without dtypes count as microseconds; the
  DataFrame components on an `EventLog` still multiply by 0.001), the
  points' `scaled_instant`, sorting by the parsed seconds;
- `src/backend/tools/minera_export.cpp`: `csv_input_source` and its callers
  (`csv_batch_frame`, the DataFrame spectrum variants, the DFG adapters with
  `csv_frame_key` on their three keys and `mark_csv_nan_cells` for pandas);
  the CLI's MSD still reads `event_log_to_table(log)` (it matched pm4py on
  every swept shape);
- the `reference_selected_field` fallbacks in rework, chaotic activities,
  variant statistics, boundary activities, clustering, decision replay and
  overlap (a resource, timestamp or lifecycle header as the activity key
  reads that projection; `process_tree_reduction`, `petri_usage_comparison`,
  `dfg_precision` and the alpha selector were not changed: check them on CSV
  headers);
- new areas: log families on CSV uploads with nanosecond cells (pm4py's
  `Timestamp.timestamp()` rounds to microseconds: durations, sojourn and
  waiting times, the spectrum's log variants; declared in BP for the
  spectrum), the DataFrame routes keyed by a number column (pm4py's pandas
  MSD and spectrum raise TypeError on an int activity column, the Polars MSD
  names a float column's activities `1.0`; typed frame cells beyond the
  batches), the CLI's remaining CSV modes against pm4py on the frame
  (temporal profile, correlation, transition system);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers);
- the CI run of the BP commit.

Round BO's targets, re-reviewed in BP (findings BP1-1 to BP1-4):


- `src/backend/tools/csv_frame_table.hpp` (moved from the server, now also
  used by `minera_export`'s `csv_batch_frame`: the CLI rebuilds the source
  from the file and uses it only when the log's events pair up with it);
  `csv_frame_key` and its callers (`overlay_active_source_selectors` with
  `csv_frame`, the `EvaluationParams` overlay, the temporal source and
  target keys, `handle_constraint_discovery`'s `frame_key`, the original
  Declare, log skeleton and footprints routes' submitted options);
  `type_csv_frame_cells` (only the batches' indexed frame is typed);
- `src/backend/csv_event_log_importer.cpp`: `replaced_by_copy` (only the
  batch import), the frame `order`, `nanosecond_cells`;
- `src/backend/batches.cpp`: `append_frame_log_events` (the role-name
  aliases, NaN identity by column type, the frame-key KeyError), the dict
  groups of the log variant (`!include_event_id`), the Polars activity
  check; `src/backend/json_export.cpp`'s relaxed batch checks;
- carried over: the `activity_source_key`/`timestamp_source_key` lookups of
  other families (grep for `event is missing activity attribute`), checked
  in BO only through the metamorphic sweeps;
- new areas: the CLI's other DataFrame modes on CSV inputs (performance
  spectrum, DFG adapters, temporal profile and the rest still read
  `event_log_to_table(log)`, not the frame: compare `minera_export` with
  pm4py on the frame as `clibatch.py` does); typed frame cells on the other
  frame-table routes (the DFG adapters, the spectrum and footprints keyed
  by a number column); the pandas and Polars batches' `org:resource` alias
  on CSV frames (pm4py raises `KeyError`; the log variant's role names are
  declared);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers);
- the CI run of the BO commit.

Round BN's targets, re-reviewed in BO (findings BO1-1 to BO1-6):

- `src/backend/tools/minera_server.cpp`: `session_frame_table`,
  `session_frame_lazy`, `session_frame_stream`, `session_indexed_frame_*`
  (the `CsvFrameCache` shared by session copies), `session_selected_view`
  and `cached_source_view` (which kinds get the frame), `csv_frame_stream`,
  `adapter_source_table` in the workbenches, the date dtypes at the end of
  `csv_frame_table`, `mark_csv_nan_cells` (only the DFG route and the
  batches use it) and the timing route's batches;
- `src/backend/batches.cpp`: the default `@@index` event id in
  `discover_table` (a table with its own `@@index` column that is not a
  frame's), the NaT start (`kNaT`) for an empty start cell, the inverted
  events in all three variants and their batch types;
- `src/backend/dfg_dataframe.cpp`: `declared_datetime_unit` and its two
  callers (a table with dtypes but rows, a start column declared as text);
- `src/backend/heuristic_miner.cpp` and `src/backend/model_decoration.cpp`:
  the `activity_source_key`/`timestamp_source_key` fallbacks (other
  families with the same lookup: grep for `event is missing activity
  attribute`);
- new areas: the conversions of a CSV frame (`/api/log-convert`
  `to_event_log`/`to_data_frame` against `convert_to_event_log(frame)`,
  which keeps NaN and NaT attributes and the `@@index`/`@@case_index`
  columns); the upload workbenches' table shapes on CSV uploads with custom
  headers and the literal default keys (`concept:name`): pm4py's frame has
  the standard columns, Minera's table refuses (`declcsv.py` without
  `--std`); the batches' log variant with a blank resource (`nan`), and the
  CLI's batches (`minera_export`) on CSV inputs; `str()` names of non-text
  activity keys in the other keyed DataFrame routes (carried over);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers);
- the CI run of the BN commit.

Round BM's targets, re-reviewed in BN (findings BN1-1 to BN1-6):

- `src/backend/dfg_dataframe.cpp`: `parse_time` (blank is NaT),
  `sort_rows` with `pandas` (the case-only `numpy_object_quick_argsort`,
  missing cases last), `same_case`, `instant_less`, `sub_microsecond`, the
  four modelled pandas errors in `discover_table_dfg`, the partial order and
  the concurrent events (the candidate pre-check, the `epoch_seconds`
  overlap); `src/backend/frame_times.cpp`'s `epoch_seconds` for nanosecond
  times (other callers: cycle time and the other frame arithmetic);
- `src/backend/tools/minera_server.cpp`: `csv_frame_table` (with and
  without the index columns), `prepare_csv_frame_view` and its use in
  `active_source_mining.hpp`; whether filtered CSV sessions keep the frame
  order; `frame_slice_table` in `src/backend/declare.cpp` with caseless
  rows;
- new areas: the other routes that hand a CSV upload's table to a DataFrame
  function in file order (the original Declare and log skeleton routes'
  `source_table`/`source_lazy` calls, `/api/analyze-behavior`'s
  `variant=dataframe` families other than the temporal profile,
  `/api/discover-constraints`, the footprints and minimum self-distance
  pandas variants; the metamorphic check `metacsv.py` fits them); the
  interval-based statistics (`overlap` family) and batches with a start
  column on CSV uploads; the DFG adapters without column reduction on CSV
  uploads (pandas drops a row with any NaN, and a blank CSV cell is NaN
  there, while `row_has_no_nulls` reads `""` as a value); `str()` names of non-text activity keys in the
  other keyed DataFrame routes;
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers);
- the CI run of the BM commit.

Round BL's targets, re-reviewed in BM (findings BM1-1 to BM1-7):

- `src/backend/temporal_profile.cpp`: `temporal_table_to_log` (the
  `CaseKey` with the rows without a case last, the ranks from the frame
  timestamp or a table's own `@@index`, `first_rows` for the conformance
  order), `temporal_table_columns`' start fallback,
  `frame_arithmetic_profile` with `index`, `has_source` and `has_target`,
  `discover_temporal_profile_ranked` and `conform_log` with ranks, the keyed
  `frame_temporal_profile` (`python::identity` groups, `safe_sort_order`
  on each level, `frame_cases_ordered`) and the default one (rows without
  an activity);
- the `mark_frame_index_rows` call sites in
  `src/backend/tools/minera_server.cpp` (behavior route, temporal
  conformance route) and `active_source_mining.hpp`; whether other routes
  reach `discover_temporal_profile_dataframe` or
  `conform_temporal_profile_dataframe` with a CSV table and no flag;
- new areas: other DataFrame algorithms on CSV uploads whose results follow
  `format_dataframe`'s `@@index` or drop rows without a case or activity
  (the DFG with a start column, the performance spectrum, the Declare and
  log skeleton tables, the interval-based statistics); XES traces without a
  name in the other DataFrame paths (pandas' `groupby` drops NaN cases, a
  self-merge keeps them); `str()` names of non-text activity keys in the
  other keyed DataFrame routes;
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers);
- the CI run of the BL commit.

Round BK's targets, re-reviewed in BL (findings BL1-1 to BL1-6):

- `src/backend/xes_event_log_importer.cpp` (`parse_native_value`'s
  canonical boolean and float text; whether the line-by-line, chunk-regex
  and Rust variants' exact `true` test still matches pm4py's iterparse
  variants on `TRUE` and valueless booleans);
- `src/backend/declare.cpp` (`frame_slice_table`, `frame_slice_view`, the
  two `*_frame_slices` functions) and their call sites in
  `original_declare_route.hpp` (the list sliced, the diagnostics merged)
  and `active_source_mining.hpp`, also with the mining filters, a supplied
  model and `conformance_activity_key`;
- `src/backend/temporal_profile.cpp`: `temporal_table_to_log`'s sort by
  start, then completion; `frame_arithmetic_profile` with targets;
  `frame_temporal_profile`'s keyed overload (text activities only, one
  column for both keys); `temporal_stream_log`; `selected_event_value`'s
  first-class fallback; `conform_log`'s exact comparison; with business
  hours and a start column on frames (`tptoy.py --bh`), lifecycle as the
  activity key, CSV uploads with a start column;
- `discover_variant_statistics_as_uploaded` on the server and in the CLI
  (`minera_export`), with a non-default activity key and after the mining
  filters;
- new areas: other pm4py functions that read an `EventStream` through its
  EventLog (the log skeleton, footprints, the DFG on the behavior route's
  `variant=stream`), and other routes that hand an upload on as a table
  (any spelling the table transport still refuses);
- the open choices on CSV timestamp columns of plain numbers and "roles to
  diagnostics groups" (still declared unless Philipp answers);
- the CI run of the BK commit.

Round BJ's targets, re-reviewed in BK (findings BK1-1 to BK1-6):

- `src/backend/declare.cpp` (`check_declare_conformance_as_uploaded`, a
  trace without rows) and the two Declare call sites in
  `src/backend/tools/minera_server.cpp` (the list sliced, the diagnostics
  merged), swept with `declconf.py` and `constrconf.py`, also after the
  mining filters (`/api/mine` with filters on interleaved uploads) and with
  a supplied model or `conformance_activity_key`;
- `src/backend/temporal_profile.cpp` (`conform_log` with `timedelta`, the
  first-row order of `conform_temporal_profile_dataframe`): different source
  and target timestamp columns, nanoseconds, the stream and lazy-table
  inputs, swept with `tempconf.py`;
- `src/backend/segment_filter.cpp` (`filter_between` on frames, through
  `frame_cases` and `relabel_frame_rows`, now in `frame_keys.cpp`) combined
  with the segment prefix and suffix and the mining filters before it
  (`combosweep.py --kinds ...,segbetween`);
- `/api/mine`'s `temporal_profile` with non-default activity or timestamp
  keys on uploads (declared in BJ1: the EventLog algorithm; `bj/tpkeys.py`
  shows the differences), the Declare workbench, `/api/variant-statistics`
  (declared in BI1), the business-hours temporal profile on frames;
- the open choice on CSV timestamp columns of plain numbers (still
  declared unless Philipp answers);
- the CI runs of the BI and BJ commits.

Round BI's targets, re-reviewed in BJ (findings BJ1-1 to BJ1-4):

- `src/backend/log_filter.cpp`: `frame_cases`, `relabel_frame_rows` (case
  cells when a sub-case gathers rows of several cases),
  `frame_activity_split` and `frame_between` combined with the other
  filters and with case attributes, swept with `combosweep.py` (plain,
  `--plain`, `--novariant`, `--kinds`); whether the `segment_between_*`
  variants on interleaved uploads follow the same marks;
  `src/backend/case_grouping.cpp` (`group_consecutive_frame_rows`);
- `src/backend/temporal_profile.cpp` and `exact_stdev.hpp`:
  `ExactMoments` (speed and memory on large logs with many pairs, durations
  far apart in magnitude), `frame_arithmetic_profile`, the defaults check of
  `discover_temporal_profile_as_uploaded`, swept with `minemodels.py`
  (`--xes`, `--frac`, through `bg/ilwrap2.py`) and `behaviortp.py`;
- leads already confirmed against pm4py while BI's suite ran (harness
  `round-bi/harness/bj/`): Declare conformance on XES uploads reads each
  case as its slice in the list form (`/api/mine`'s `declare_conformance`,
  discover-constraints; `declconf.py`: 5 of 5 interleaved uploads differ,
  `declslice.py`'s slice model matches pm4py on 30 of 30), while the
  diagnostics table reads the merged cases in file order (it already
  matches); the DataFrame temporal conformance (behavior route
  `variant=dataframe`, `/api/temporal-conformance`) lists the cases in the
  order of their first rows, not by sorted id (`tempconf.py`; within a case
  the order matches); `segment_between_*` on interleaved uploads follows
  BI1-5's slice marks (`combosweep.py --novariant --kinds
  segbetween,...`: 3 of 58 differ; `segbetween_model.py`: the model with
  list endpoints and a separator matches pm4py on 3,000 random frames);
- the other pm4py DataFrame functions that may read a case as a slice
  (AE2-10): the Declare workbench, `get_variants`-based routes
  (`/api/variant-statistics`, declared in BI1), the business-hours
  temporal profile on frames;
- the open choice on CSV timestamp columns of plain numbers (still
  declared unless Philipp answers);
- the CI run of the BI commit.

Round BH's targets, re-reviewed in BI (findings BI1-1 to BI1-6):

- `src/backend/llm_abstractions.cpp` (`llm_log_skeleton_description`) and
  `src/backend/llm_context.cpp` (`declare_frame_cases`, the temporal
  profile dispatch), swept with `llmmodels.py` (plain, `--xes`, and
  through `bg/ilwrap2.py`); `src/backend/temporal_profile.cpp`
  (`discover_temporal_profile_frame`: nanosecond timestamps, equal
  timestamps, days-long durations, NaT);
- `src/backend/log_filter.cpp`: the frame slices in the variant pass
  combined with earlier filters (endpoints, lengths, case splits,
  consecutive-activity grouping) before `top_variants` and the coverage
  bounds, and whether `variant_percentage`, `variant_coverage` and
  `min_variant_frequency` have a DataFrame counterpart in pm4py that
  counts the same slices (`fsweep.py`, `topkmodel.py`);
- whether `/api/mine`'s own `declare`, `log_skeleton` and
  `temporal_profile` sections on uploads should follow pm4py's DataFrame
  discoveries the way the LLM descriptions now do, and the other pm4py
  DataFrame functions that may read a case as a slice from its first row
  (`get_variants`-based routes, the variants explorer, the LLM variants
  description on interleaved uploads with performance);
- the open choice on CSV timestamp columns of plain numbers (still
  declared unless Philipp answers);
- the CI run of the BH commit.

Round BG's targets, re-reviewed in BH (findings BH1-1 to BH1-4):

- `src/backend/attribute_statistics.cpp` and `case_statistics.cpp`: the
  frame branches in `frame_rows` order (once-per-case pairs across
  non-adjacent rows, groups of another case column), swept with
  `ilwrap.py statsweep.py --xes` and `ilwrap2.py axsweep2.py --xes`;
- `src/backend/llm_abstractions.cpp`: `frame_view`'s column order from the
  rows in file order, and the log-skeleton description's activity order,
  which still reads the traces (`llmsweep.py` does not cover it);
- `src/backend/csv_event_log_importer.cpp`: the long-fraction rule
  (`two_digit_clock`), comma fractions after basic dates and the rewritten
  `reference_clock_first` (month names, zones between clock and date),
  fuzzed with `mixfuzz6.py` and `mixfuzz7.py` and BF's `mixfuzz5.py`;
- frame loops not yet swept interleaved: the pandas filters on uploads,
  the features' event-based rows, temporal profiles and the remaining
  `log.traces` loops of `organizational_mining.cpp` and `role_discovery.cpp`
  outside the pandas variants;
- an open choice: a CSV timestamp column of plain numbers (int64 in
  pm4py's frame) could be modelled as numbers in the frame routes instead
  of the declared opaque timestamps (BG1 declaration, `pdsweep3.py` seed
  3059);
- the CI run of the BG commit.

Round BF's targets, re-reviewed in BG (findings BG1-1 to BG1-6):

- `src/backend/python_list_sort.hpp`: `PythonListSort` (run detection,
  `merge_lo`/`merge_hi` galloping, the state after a raising comparison),
  fuzzed with `seqfuzz.py`; every caller of `python::sort` and
  `sort_reverse` (KDE, SNA, batches, organizational mining, the pandas
  variants filter);
- `src/backend/frame_keys.cpp`: `frame_rows`, and the frame loops that
  still iterate `log.traces` (SNA `require_sortable` values, roles, LLM
  context, statistics, features, filters): sweep them with `ilwrap.py`
  (any sweep script with interleaved trace names) and with XES uploads
  whose trace names are not in sorted order;
- `src/backend/csv_event_log_importer.cpp`: `reference_clock_first` (zones
  and offsets joined to the date, commas, AM/PM) and
  `reference_six_digit_date`, fuzzed with `mixfuzz5.py` and `mixfuzz4.py`,
  and through `/api/format-dataframe` (`fmtprobe.py`, which must send typed
  values for int and float cells);
- `src/backend/organizational_mining.cpp`: the first-row order of the case
  sequences in `require_sortable_variants`, and joint activities' per-column
  NaN drop (`snamix3.py` and `snamixcsv.py`); open: subcontracting
  (pandas) with NaT ties (declared, seed 7039);
- the CI run of the BF commit.

Round BE's targets, re-reviewed in BF (findings BF1-1 to BF1-5):

- `src/backend/attribute_kde.cpp`: `frame_attribute_kde` (the sort, the
  single-value `float()` branch with `parse_python_float_text`, bools and
  dates in the numeric estimator, the first sorted cell of the date
  estimator), swept with `kdesweep2.py` and through the CLI
  (`clipost.py`);
- `src/backend/frame_keys.cpp`: `@@index` and `@@case_index` on XES
  uploads in every other route that reads `frame_column_type` (filters,
  features, SNA, resource profiles);
- `src/backend/csv_event_log_importer.cpp`: `reference_numeric_cell_date`
  (fuzz with `ndfuzz.py` and `mixfuzz.py`, new shapes such as clocks after
  delimited dates and longer digit runs), `reference_timestamp`'s choice
  between it and the general grammar (the ISO path stays the general
  grammar's; `isonum.py`), and the timestamp scan's gate
  (`numeric_timestamps()`: only a column read_csv types as text);
- the public `dataframe_format` route and every other caller of
  `reference_mixed_timestamp`, which now read partial dates too;
- `python::sort` for 64 items or more (CPython 3.11's powersort runs and
  galloping; 8 of 1,700 random mixed lists name the failing pair the other
  way round);
- the CI run of the BE commit.

Round BD's targets, re-reviewed in BE (findings BE1-1 to BE1-4):

- `src/backend/log_to_features.cpp`: `frame_trace_set_numeric` (each
  case's first row, `case:` columns of CSV uploads with per-row cells,
  `VARY=1` of `boolselcase.py`);
- `src/backend/resource_profiles.cpp`: `workload_conversion_error` (the
  `same_case` shift, `shifted_numbers`, `value_codes` and the second sort),
  swept with `wlshift.py` and `objtime.py`; check that
  `average_duration_activity` without a start column needs no second sort
  (`probes/adaprobe.py` says it compares with the window first);
- `src/backend/organizational_mining.cpp`: the `grouped` skip of rows
  without a case (`snamixcsv.py` with case columns of other types);
- `src/backend/attribute_distribution.cpp`: `frame_event_distribution`
  (`evdist.py` with ISO week 53 and pre-1970 dates, the CLI path);
- new: attribute KDE on uploads (`kdesweep.py`: 64 of 90 jobs give
  pm4py's own error texts, which Minera words as its own, and a bool or
  bool-and-int column is a numeric KDE in pm4py but an error in Minera);
- new: pandas' mixed date parsing of number-like text in a CSV column of
  dates (format_dataframe converts every text column whose cells all
  parse: `007` is 0001-01-07, `2020` is 2020-01-01; Minera keeps such a
  column as text, `snamixcsv.py` seed 152);
- the CI run of the BD commit.

Round BC's targets, re-reviewed in BD (findings BD1-1 to BD1-4):


- `src/backend/organizational_mining.cpp`: `require_sortable` (first
  appearance, `column_type_error`), `require_sortable_variants` (numpy's
  argsort of the cases' tuples, then the per-position categorical check),
  the `NaT` selector (`nat_cells`) and the case-grouped variants' key
  order; sweep `snamix2.py` with CSV uploads (round BC swept XES);
- `src/backend/resource_profiles.cpp`: `case_groupby`, the `case_metric`
  branch, `average_case_duration`'s per-group minimum and
  `frame_average_case_duration`'s window, and the two-object-column
  subtraction of `average_duration_activity`;
- `src/backend/log_to_features.cpp`: `frame_set_numeric` (event columns
  only; the case-level `inferred_trace_attributes` still counts cases,
  check pm4py's selection of `case:` columns of numbers and bools with
  gaps), and every route that reads the trace-based default selection on
  uploads (features, LLM context, clustering, root cause);
  `python::numeric_hash` for ints past 64 bits;
- open from BC: `average_workload` and `multitasking` without a start
  column on an XES object timestamp column of text and ints
  (`objtime.py` seed 279, pm4py names `float`, Minera `str`);
- the CI run of the BC commit.

Still open: the set-order selection of mixed-type keys (AQ, AV1; text and
dates hash with the seed, so it stays declared), and Philipp's answer on
whether the roles to diagnostics button should refuse uploads without an
`org:group` column (AT). Harness: `review-harness/round-bd/` (README
there; round BC's is `review-harness/round-bc/`).

Round BB's targets, re-reviewed in BC (findings BC1-1 to BC1-8 in the
code BB touched, its open SNA and LLM targets, and network analysis):

- `src/backend/resource_profiles.cpp`: `workload_conversion_error` (the
  per-row `.timestamp()` order, the shifted start of the previous same-case
  row, `missing_name`), the masked branch's dtype rule (`any_kept`,
  `all_kept`), the `case_metric` filter in `offending_type`, the
  `average_case_duration` bool message, the subtraction messages of
  `average_duration_activity` with its start column named twice, the NaT
  skip in the view, and `require_duration_rows` (rows without a case now
  counted, `repeated_case` only for a case with a dated row); sweep
  `rpfuzz.py` and `mtmatrix3.py` with XES uploads (object columns of
  dates, text and numbers), which round BB swept on CSV only;
- `src/backend/local_diagnostics.cpp`: the pair groupby over every row,
  `membership` (members in the groupby's order), `sort_error`, `date_repr`
  (datetime.datetime in its own offset, Timestamp in a datetime column)
  and the moved resource-as-activity `KeyError`;
- `src/backend/role_discovery.cpp`: the pandas variant's `groupby` model
  (`resource_rank`, the activity join over the groups), the log variant's
  per-role names (`display`, `first_text`), `NaT` as one resource of a
  datetime column, and `resource_count` from the shown names;
- found at the end of BB, not yet fixed: SNA on a datetime resource
  column with gaps (CSV or XES). pm4py keeps the missing cells as `pd.NaT`,
  one resource `NaT` that sorts beside Timestamps without error, in every
  variant (`working_together`: `('NaT', '2020-01-01 00:00:00+00:00'): 1.0`);
  Minera's `require_sortable_classes` counts the missing cell as a float
  and raises `TypeError: '<' not supported between instances of
  'Timestamp' and 'float'` (`snamix.py` seed 16 in the round's harness;
  150 seeds: 38 such or NaN-declared results, plus three seeds where
  pm4py's empty-network `ValueError` comes first). Check the same `NaT`
  and `Counter` rules in resource profiles' and network analysis' frame
  paths;
- the standard OCEL JSON transport's `minera:nanoseconds` (`ocel.cpp`:
  `parse_ocel_json_names`, the standard writer): every route that posts
  the standard form (OCEL filters, flattening, statistics, exports);
- the CI run of the BB commit.

Still open: the LLM context's trace-based selection of a bool column with
gaps (Review BA1 declarations; `llmsweep.py` seed 17 with `VARY=1`), the
set-order selection of mixed-type keys (AQ, AV1), and Philipp's answer on
whether the roles to diagnostics button should refuse uploads without an
`org:group` column (AT). Harness: `review-harness/round-bb/` (README
there).

Round BA's targets, re-reviewed in BB (findings BB1-1 to BB1-9 in the code
BA touched or its neighbours; BB1-10 to BB1-12 from reruns of earlier
sweeps):


- `src/backend/resource_profiles.cpp`: `masked_frequency` and
  `masked_distinct` (the one-column masks, the error branch they share
  with `activity_completions`, the all-resources rows), the `Grouper`
  check of the case metrics in `require_unique_selection`, the moved
  start-key `KeyError` of `average_case_duration`; sweep
  `mtmatrix2.py` with typed (int, float, bool, object) columns in each
  role, which round BA's matrix did not vary;
- `frame_passed_time` in `src/backend/passed_time.cpp`: the
  `column_subtraction` call (the shifted dtype table, the one-pair whole
  array) on CSV and XES uploads whose timestamp or start columns are
  object, text, int, float or bool with missing cells, one pair or several;
- `require_frame_columns` in `src/backend/frame_keys.cpp` (missing labels
  once) and `check_list_columns` in `pandas_attributes_filter.cpp` and
  `pandas_timestamp_filter.cpp` (an all-missing list key that names a
  label twice is `None of [Index([...])]` with both, as pandas says);
  still unchecked: `require_columns` in `pandas_sequence_filters.cpp`
  deduplicates the names before its `None of` message, right only if the
  pm4py function never selects a label twice;
- the `minera:nanoseconds` marker in `src/backend/ocel.cpp`
  (`write_nanoseconds`, the reader's `keep_nanoseconds`, every route that
  passes classic JSON on: filters, flattening, OCDFG, statistics, exports,
  the frontend's OCEL store);
- `frame_column_type` for `case:` columns and the CLI's XES frame
  (`minera_export.cpp`): every CLI flag on XES inputs now takes the frame
  path, not only the time statistics;
- the CI run of the BA commit.

Still open: the LLM context's trace-based selection of a bool column with
gaps (Review BA1 declarations; `llmsweep.py` seed 17 with `VARY=1`, and
the probes `boolnan.py` and `boolnan2.py` in the round's harness), and the
set-order selection of mixed-type keys (AQ, AV1). Candidate new areas: the
EventLog paths of the business-hours model through the CLI; two-row
uploads through the remaining routes; the other CLI flags on XES and CSV
inputs against pm4py's frame functions. Harness: `review-harness/round-ba/`
(README there).

Round AZ's targets, re-reviewed in BA (no finding in AZ's code; BA1-1
replaced the OCEL 1.0 JSON declaration):

- the two-row label rule: `pandas_row_label_keys` and `label_characters`
  (`src/backend/frame_keys.cpp`), the `pair_labels` branches of
  `src/backend/local_diagnostics.cpp` (`label_members`, `label_shares`,
  the error order against the group `KeyError`) and of
  `src/backend/role_discovery.cpp`; and whether other pandas `groupby`
  calls with a key list (attribute statistics, case statistics, features,
  the temporal profile, the DataFrame DFG) meet the same rule on two-row
  uploads with a key that is no column;
- the DataFrame-scope `case:` branch of `frame_value` and
  `frame_row_scalar` in `frame_keys.cpp`: every consumer that reads a
  `case:` key on the frame (attribute statistics, features, root cause,
  the filters on the frame, LLM context) now sees the row's own value;
  sweep uploads whose `case:` cells vary within a case through them;
- `require_duration_column` and `require_duration_rows` in
  `src/backend/resource_profiles.cpp` (the per-resource identities, the
  date identity of an all-date resource column);
- `naive_timestamp`'s `refused_year` (`local_time.{hpp,cpp}`) and the
  three callers, and `natively_stored` in `src/backend/ocel.cpp` (texts
  with nanoseconds respelled, `truncates` off for the classic JSON
  transport): OCEL exports, filters and statistics on
  such timestamps from JSON and XML against SQLite and CSV inputs;
- the CI run of the AZ commit.

Carried over from AX (not taken in AY or AZ, taken in BA: BA1-6 to
BA1-8): `frame_times::column_subtraction` and its callers,
`frame_passed_time`, `frame_cases` and `frame_column_type`, and the NaN
cell rule of `note_cell` and `typed_columns` (details in AX's list
below). Candidate new areas: the
EventLog paths of the business-hours model through the CLI (uploads all
take the frame path), and two-row uploads through the remaining routes.
Still open from earlier rounds: the trace-based selection of mixed-type
and bool-with-gaps keys by Python set order (AQ, AV1). Harness:
`review-harness/round-az/` (README there).

Round AY's targets, re-reviewed in AZ (no finding in the code AY
changed; AZ1-6 came from the OCDFG business-hours sweep):

- `business_durations` and `local_wall_micros` in
  `src/backend/network_analysis.cpp` (the row-wise apply, its error order
  against the sort and merge checks, the NaN differences and
  `network_analysis_json`'s `null`, the frontend validators and the
  organizational workbench tables);
- `detail::business_elapsed_seconds_microseconds` (`business_hours.hpp`)
  through its consumers on the EventLog paths, which AY swept only on
  frames: service time, the DFG performance, heuristics, the temporal
  profile, OCDFG edge performance, token-replay durations, OCEL
  statistics, with fractional slots and cross-week gaps;
- `local_time.{hpp,cpp}` (CPython's fold-0 search, `wall_seconds` on
  Windows through the zone database) and its three callers; only CI shows
  the Windows side;
- `case_arrival.cpp`: frames ignore business hours, logs keep them;
- the CI runs: AY3's passed everywhere; the zone-database test prints the
  values that follow Windows' tz release only when it fails, so a later
  Windows image may need the declared zone list revisited;

Found in AY's sweeps and fixed in AZ (AZ1-1, AZ1-3; CSV uploads of
`aysweep.py`, the same on round AX's server): local diagnostics on an upload without
`org:resource` raises `KeyError: 'org:resource'` where pm4py raises
`KeyError: 'org:group'` (default options, seeds 231 and 301) or
`ZeroDivisionError: float division by zero` (another group column, seeds
231 and 324); resource profiles' `average_duration_activity` with a
`resource` column and a text start column raises the `.dt` accessor error
where pm4py returns 0.0 (seed 324). pm4py tolerates the missing resource
column in odd ways: with an `org:group` column it returns groups keyed by
single characters (probe: `ldprobe_ay.py` in the round's harness).

AY's candidate new areas, taken in AZ (AZ1-5, AZ1-7): CPython's `year 0
is out of range` for `datetime(1, 1, 1).timestamp()` in a zone east of
UTC; `long` (32-bit on Windows) in `trace_attribute_reference.cpp`.
Harness of AY: `review-harness/round-ay/` (README there).

Round AX's targets (AY took the CI run, network analysis and business
hours from this list, findings AY1 and AY2; the column subtraction, passed
time, case statistics and NaN-cell items carry over to AZ):

- `frame_times::column_subtraction` in `src/backend/frame_times.cpp` and
  its callers: the `slots.empty()` branch of `frame_service_durations`
  (`service_time.cpp`), the non-business branch of `frame_cases`
  (`case_statistics.cpp`), and whether cycle time, case arrival, resource
  profiles or network analysis subtract columns in ways the model now
  covers;
- `frame_passed_time` in `src/backend/passed_time.cpp`: the sort codes,
  the raised start (`held`, `ge_error`, the float64 and missing-timestamp
  rules), the pair-less errors, the text and object pairs, the business
  conversion (`nat_text`, `unlikely_date`, `inferred_date_format`), the
  aggregation, and `passed_time_json` with the frontend validator;
- `frame_cases` in `case_statistics.cpp` (custom case key, `nargsort`
  order, `sort_by_given`, the business-hour replace and `int()` errors,
  the empty-group errors) and `frame_column_type` in `frame_keys.cpp`
  (bool with a blank, CSV only);
- `note_cell` and `typed_columns` in `src/backend/dataframe_columns.cpp`
  (the NaN cell rule): every XES consumer of frame dtypes (LLM context,
  attribute statistics, organizational routes, features, root cause) sees
  the new `str`/date/float64 types on round-trip XES files.

- the CI run of the AX commit: Windows has never linked or run ctest, so
  its first link and test failures are findings; the macOS run should be
  green. Run the suite once under AddressSanitizer and UBSan on Linux (a
  separate build tree; it found AX2-2's use after free at once), and check
  other tests for references into temporaries.

Candidate new areas: passed time on the native path with an explicit
`time:timestamp` start and another timestamp key; pandas' inferred datetime
format for business hours on text columns (declared in AX1); event
distribution and the remaining DataFrame statistics on frames with NaT
cells; business hours in network analysis with fractional seconds (still
epoch doubles there, not swept in AX); the open network-analysis error
order of AX1 (XES seed 59 of `atsweep2.py`). Still open from earlier rounds: the trace-based selection
of mixed-type and bool-with-gaps keys by Python set order (AQ, AV1).
Harness: `review-harness/round-ax/` (README there).

Round AW's targets, re-reviewed in AX (findings AX1-1 to AX1-5, fixed):

- `src/backend/frame_times.{hpp,cpp}` (`Frame`, `case_cells`,
  `subtraction_error` with XES `datetime.datetime` cells, `total_seconds`,
  `epoch_seconds`) and the frame branches built on it:
  `frame_service_durations` in `service_time.cpp`, `frame_cycle_events` in
  `cycle_time.cpp`, `frame_case_endpoints` in `case_arrival.cpp`,
  `frame_cases` in `case_statistics.cpp`, and the null/negative JSON in
  `json_export.cpp` with the frontend validators in `types.ts`;
- `src/backend/resource_profiles.cpp`: `require_timed_selection` (the
  case, timestamp, resource and activity repeats, with and without a
  start column, the one-column case), the start-on-case branch of the
  duration, the reversed-interval error;
- the exact durations in `network_analysis.cpp` (business hours still use
  epoch doubles there: worth a sweep with fractional seconds).

Round AV's targets, re-reviewed in AW (findings AW1-1 to AW1-3, fixed):

- `src/backend/llm_abstractions.cpp`: `frame_view` (column order from
  `DataFrameColumns::order` or the XES event stream, per-column dtypes,
  `object_scalar` for XES object dates), `frame_columns_description` and
  `describe_columns` (the dtype table, the all-NaN float quantiles,
  `merge_numbers`), `frame_event_stream_description`, the lazy variant
  performance, and `exact` in `src/backend/llm_context.cpp`;
- `DataFrameColumns::order` in `src/backend/csv_event_log_importer.cpp`
  (header, copies, `@@index`), and `frame_row_scalar` now shared from
  `src/backend/frame_keys.cpp`;
- `src/backend/resource_profiles.cpp`: the multitasking interval dedup, the
  strict window in `frame_average_case_duration`, the float64 NaN rule
  there, the compare-first rule before `require_unique_selection`;
- the empty-role keys in `frame_local_diagnostics` and the NaT check in the
  temporal `missing_category` of `src/backend/log_to_features.cpp`.

Round AU's targets, re-reviewed in AV (findings AV1-2 to AV1-5 and AV1-8,
fixed):

- `src/backend/attribute_statistics.cpp`, the frame branch of
  `discover_attribute_statistics` (`scalar_key`, `frame_row_cell` with the
  per-row case cells, the trace-scope key resolution, the once-per-case
  `groupby` and its `ValueError`/`TypeError`), and the CLI and LLM callers
  that now take it for logs with `dataframe_columns`;
- `src/backend/resource_profiles.cpp`: `require_duration_column` (the
  selection, the start dtype table, the single-event-case rule),
  `frame_interaction_count`, the `compared_rows` sort by case and
  timestamp codes, the float64 NaN rule in `offending_type`, the
  `cannot subtract DatetimeArray from ndarray[...]` branch;
- `src/backend/network_analysis.cpp`: `require_timestamp_column` (pairs in
  source order), the CSV bool-with-blank object dtype in `frame_dtype`
  (also seen by `require_mergeable`), the own `@@index` cells;
- AT1-4 date columns in the remaining CSV routes not swept in AU (features,
  root cause, temporal features, filters on the frame, LLM context), with
  offsets, `T`, partial and time-only shapes (AV1-1, AV1-6, AV1-7; filters
  not swept).

Harness of AU: `review-harness/round-au/`
(`statsweep.py [--xes]`, `socmatrix.py <xes|csv> <metric> [cols]`,
`socref.py`/`socmodel.py` for the interaction model, `adaprobe.py` and
`adaprobe2.py` for the duration probes; `atsweep2.py` and `atref.py` from
`review-harness/round-at/` plus `orgxes.py` from `round-as/`).

Round AT's targets, re-reviewed in AU (findings AU1-2 to AU1-10, fixed):

- `src/backend/resource_profiles.cpp`, `frame_resource_profiles` and
  `frame_average_case_duration` (the per-metric `frame_selection`, the
  `offending_type` rows, `query_key` date parsing and which metrics use
  `isin`, the duplicate-column exceptions, the case-column `groupby`
  check, the view of one trace per case group with NaN-case rows kept as
  their own intervals, `activity_completions` with the resource on the
  timestamp column) and the `frame` flag in `compute_resource_profiles`
  (NaN activities, slot cases without a resource);
- `frame_local_diagnostics` in `src/backend/local_diagnostics.cpp` and the
  frame branch of `discover_network_analysis` (`require_mergeable`,
  `frame_dtype`, `require_timestamp_column`, the `@@index` relation);
- the AT1-4 date columns of `import_csv_event_log` and `csv_dataframe_cell`:
  every CSV route that reads typed frame columns now sees such a column as
  dates (attribute filters and statistics on the frame, features, root
  cause, temporal features, OCEL and LLM context): sweep CSV uploads with
  date-text columns (with offsets, `T`, partial and time-only shapes)
  through them;
- the frontend `withSessionResourceKeys` and the roles to diagnostics
  handoff (explicit groups raise `KeyError: 'org:group'` on uploads without
  that column, as pm4py does; flagged to Philipp).

AU then took attribute statistics onto the frame (AU1-1). Harness of AT:
`review-harness/round-at/` (`atref.py`, `atsweep.py --only
profiles,diagnostics,network [--xes]`; it reuses `orgxes.py` from
`review-harness/round-as/`).

Round AS's targets, re-reviewed in AT (findings AT1-1 to AT1-3, fixed):

- `src/backend/frame_keys.cpp` (`frame_keys` per scope: CSV `@@index`
  columns, the activity column under its own header read as text, the XES
  `time:timestamp`, `case:` keys of the DataFrame scope; `frame_value`
  typing and the NaN/NaT cells; `FrameNames`, which merges numbers equal in
  value and orders them by value; `require_sortable_classes`);
- the frame branch of `discover_organizational_sna` (which keys each
  variant requires, the TypeError pre-pass and the joint-activities pandas
  exception, `regroup_traces` on the frame dropping NaN case keys, the
  optional resource sequences of handover and subcontracting, the typed
  variant order of subcontracting pandas) and of `discover_roles` (the
  activity join TypeError, the PANDAS groupby drop and sort, the typed
  resource order, empty names in `role_discovery_json`);
- other callers of both functions that now take the frame path for logs
  with `dataframe_columns` (CLI `--sna`/`--roles` on CSV inputs, the
  organizational workbench routes, LLM context), and the frontend default
  (`sessionResourceKey`, `withSessionResourceKey`; the roles to local
  diagnostics handoff passes the roles resource key on).

AT then moved the remaining organizational routes (resource profiles, local
diagnostics, network analysis) onto the frame (AT1-5 to AT1-7). Harness of
AS: `review-harness/round-as/` (`orgref.py`, `orgsweep.py` for CSV,
`orgxes.py` for XES).

Round AR's targets, re-reviewed in AS (no finding):

- the frame block of `import_csv_event_log`
  (`src/backend/csv_event_log_importer.cpp`): `activity_copy` (a
  `concept:name` column left out beside another activity column) and the
  `kCaseId` trace column for a `case:` case header other than
  `case:concept:name` (its place among the `case:` columns, its type from
  `case_kind`/`case_ints`);
- the trace branch of `dataframe_frame_event_log`
  (`src/backend/dataframe_columns.cpp`): the dropped CSV trace
  `concept:name` and the `kCaseId` cell from `Trace::case_id`; in
  `log_to_features`, the `case_id_source_key` cleared on the frame's traces
  under `standard_keys` (CSV sources only);
- `log_to_features` on the frame view for the calendar TEMPORAL variant
  (`grouper_freq`): UTC timestamp copies with offsets, XES frames, keys that
  name the case column under its own header.

Round AQ's targets, re-reviewed in AR (findings AR1-1 to AR1-4, fixed):

- `src/backend/dataframe_columns.cpp`: `typed_columns` and `note_cell`
  (shared by event and trace columns), `trace_columns` for XES (traces
  without events left out, `trace_case_id_attribute`), the trace branch of
  `dataframe_frame_event_log` (attributes replaced in place, missing columns
  appended, CSV values through `csv_dataframe_cell`), `utc_date_cell`, the
  CSV-only `@@index`/`@@case_index` cells, the dropped `concept:name` and
  cleared resource and lifecycle fields, and the date-kind NaT cell (its
  category in `root_cause.cpp`);
- the frame block of `import_csv_event_log`: the kCaseId and kTimestamp
  columns, the `time:timestamp` copy, the `case:` trace columns (typed over
  `row.case_attributes`), `row_index`/`case_index` after the final sort;
- `log_to_features` reading the frame view for the trace- and event-based
  variants (and what that changes for its other callers: clustering profile
  features, LLE, the CLI, `original_features_workbench_json.cpp`), the
  `lifecycle:transition` exclusion in `inferred_event_attributes`;
- `read_type_table_changes` with `first_rows` and `TypeTableBase::kFirstRow`
  in `src/backend/ocel_sqlite.cpp`.

Round AP's targets, re-reviewed in AQ (findings AQ1-1 to AQ1-7, fixed):

- `src/backend/dataframe_columns.cpp`: `xes_dataframe_columns` (dtype
  inference from typed values, the activity and timestamp keys left out,
  resource and lifecycle fields when an event keeps them out of its
  attribute list), `xes_value`, `dataframe_frame_event_log` (column order,
  unlisted attributes after, float64 ints as floats, NaT in date columns);
- the column block in the batch path of `import_csv_event_log`
  (`src/backend/csv_event_log_importer.cpp`: `infer_reference_column` over
  every pending row, the kObject-of-ints rule, unique keys) and
  `csv_dataframe_cell`; where `dataframe_columns` travels
  (`copy_event_log_context`, `select_event_log_fields`, `from_xes_frame`,
  `import_xes_dataframe_log`) and whether any other consumer should read the
  frame view (only `token_replay_root_cause.cpp` does now);
- `import_ocel_sqlite`: the relaxed event and object loops
  (`insert_or_assign`, `activities` and `object_type_names` from every
  listing), the change-type fix-up, the empty-frame rule, and
  `read_type_table_changes` (NULL value for a missing column, empty fields).

Round AO's targets, re-reviewed in AP (findings AP1-1 to AP1-4, fixed):

- `import_ocel_sqlite` in `src/backend/ocel_sqlite.cpp`: `TypeTableRows`
  (row positions, `TypeTableBase`, first row per id), the event and object
  frames built per type table in name order, `object_tables_have_base_rows`,
  `read_type_table_changes` with the global flag, the table-level relevance
  pass at the end (replacing `drop_unrelated_ocel_entries` there), the NULL
  type and missing-table errors, `validate_references` on empty types;
- the NaT branch (`undated`) of `sort_ocel_events_by_timestamp` for
  `kInstantNanoseconds` in `src/backend/ocel.cpp`;
- `append_classic_relations` (dedupe of reference-shaped omap entries only)
  and the no-relation error in `parse_ocel_xml` in `src/backend/ocel_xml.cpp`,
  including routes and fixtures that import XML (an XML upload without
  relations is now a 400).

Round AN's targets, re-reviewed in AO (findings AO1-1 to AO1-7, fixed):

- `drop_empty_ocel_names` in `src/backend/ocel.cpp` and its call sites
  (`parse_ocel_json_names` unless literal names, `parse_ocel_xml` after the
  date readings, `import_ocel_sqlite`); `parse_standard` (repeated event
  ids: first place, last content, per-event relation lists) and
  `parse_standard_type_table` (last listing wins, empty declared types
  skipped); the relaxed classic catalog (empty attribute names);
- the XML readers in `src/backend/ocel_xml.cpp`: empty ids and types no
  longer refused (an empty id may repeat), the classic relation to an empty
  id, `parse_standard_xml_type_table`; the SQLite event and object loops
  (rows without id or type skipped unless `validate_references` is true);
- `parse_ocdfg_json` endpoint fields and `merge_saw_nets` without object
  types (plus `ocel-net-model.ts`, which skips a merged net without places).

Round AM's targets, re-reviewed in AN (findings AN1-1 to AN1-5, fixed): the
OCEL readers on unlisted objects, `OcelSqliteOptions::validate_references`,
`ocdfg_edge_key` and `serialize_edges`.

Round AL's targets, re-reviewed in AM without findings: pending nodes in
`Aligner::align_configured` (inherited priority, 1e-6 margin, re-queue
with the old sequence number), `same_search` and `bundle_searches` in
`reference_log_alignment`, and the arena bound in `minera_server.cpp`.

Round AK's targets, re-reviewed in AL without findings: `bounds` and
`lazy_bounds` in `align_configured`; the threaded variant loop of
`align_petri_log`, `EmptyRunCache`, the order in which errors raise.

Round AJ's own targets, re-reviewed in AK without findings:

- `check_alignment` in `src/backend/conformance.cpp`: the parallel variant
  loop (`parallel_variants`, `PrecomputedVariant`, `stop_at`, the locked
  cancellation callback) and that `Aligner::align_configured` stays free of
  shared mutable state (`bound_cache_` belongs to `Aligner::align` only);
  `mallopt(M_ARENA_MAX, 4)` in `minera_server.cpp`;
- `LogToFeaturesOptions::standard_keys` in `src/backend/log_to_features.cpp`
  (`event_feature_keys`, `inferred_trace_attributes`, the case-column
  fallback in `numeric_cell`, the resource-header rule) and where the server
  and CLI set it (`standard_keys_for_session`, `.csv` inputs);
- the drift threshold (`concept_drift.cpp`, `request_params.cpp`, CLI help,
  `Conformance.tsx`).

Round AP's second area (AP1-5 and AP1-6, fixed), started at the end of AO: root cause at the route
level (`POST /api/mine` with `token_replay_include_root_cause_diagnostics`,
compared with `diagnose_from_trans_fitness` and
`diagnose_from_notexisting_activities` on `pm4py.convert_to_event_log` of
the formatted CSV). Random CSV logs without missing cells matched (120 of
120 tree comparisons, seeds 0 to 14). With missing cells (seeds 100 to 139)
16 of 272 differed and 6 logs raised in Minera only. The cause: pm4py's
DataFrame-to-EventLog conversion keeps a missing cell as a NaN value on the
event, so it overwrites the context that accumulates along the trace; a
missing string cell becomes the category "nan", a missing numeric cell NaN
data, and an integer column with a missing cell becomes float64, so its
string feature reads "@1.0". Minera drops missing CSV cells, so the
context keeps the earlier value (or "UNDEFINED"), and a numeric attribute
raises "missing or invalid numeric attribute". The fix belongs to the
DataFrame-path sources (CSV and XES uploads) feeding
`token_replay_root_cause.cpp`. (XES events without the key do not stay
absent: `pm4py.read_xes` returns a frame, where they hold NaN; fixed in AP
as AP1-6.) A numeric attribute holding strings raises in both (pm4py:
`could not convert string to float`), so that case is consistent. Harness:
random CSV logs per seed, `/api/mine` with random string and numeric
attribute lists and multiplier, models rebuilt from the response's
`*_petri_net` JSON, trees compared on feature names, data (NaN-aware),
targets, classes and root counts. The round's scripts are kept in the
project files under `review-harness/round-ao/` (not in git); round AP's
(`rcsweep2.py`, `rcxes.py` for root cause on CSV and XES, `sqfuzz.py`
with `--mutate` for SQLite) under `review-harness/round-ap/`.

Round AQ's harness scripts are under `review-harness/round-aq/` in the
project files (not in git): `featsweep.py` (features on CSV, `--custom 1`
for custom headers), `featxes.py` (features on XES), `casegap.py` (`case:`
columns with blanks), `rccsv4.py` and `rcxes3.py` (root cause), `sqfuzz2.py`,
and `refltf.py`, which reruns the reference under other hash seeds for the
declared mixed-type keys.
Round AS's (`review-harness/round-as/`): `orgref.py` (pm4py side),
`orgsweep.py` (SNA and roles on CSV uploads with custom headers, typed and
blank resource cells) and `orgxes.py` (the same on XES uploads, from
`rcxes3.py`'s generator), `tempoff.py` (temporal features on CSV timestamps
with offsets).
Round AR's (`review-harness/round-ar/`): `featcase.py` (CSV case columns
with `case:` headers, varying `case:` cells, copied columns beside custom
ones), `tempsweep.py` and `tempxes.py` (calendar TEMPORAL features on CSV
and XES), `profsweep.py` (reference profile clustering, `--gaps 0|1`; it
counts k-means results Minera flags as ties apart) and `profprobe.py`
(features and sklearn labels of one case).

Areas without a recent random-input sweep that are good candidates for the
second reviewer: the remaining organizational routes on CSV uploads (above), other
DataFrame-path consumers on CSV and XES uploads whose pm4py reference reads
attributes (decision mining is native at the route level; profile
clustering and temporal features were swept in AR), and the legacy-cost
imported-model alignment (the bundle's unit
costs differ from the dispatcher's, so both still search: about 110 s on
1,000 cases). In stack samples of the A* searches on these logs the LP
solver (`solve_lp_sparse`) was about half of the busy frames. One way to
cut it: pm4py's trusted bound. When the parent's LP solution
(`LpResult::solution`) uses the move at least once, that solution less the
move solves the child's LP at the parent's bound minus the move's cost,
which consistency makes optimal, so no LP is needed. The derived value can
differ from a solved one in the last bits, so equal-priority ties must be
checked (fast-path pins, the BPMN fuzz comparison) before relying on it.

Known open or declared: mine-view generalization sum order (2 ulp); POWL
brute-force filtering and dynamic clustering rare cases; BPMN→net
align-ETC ties and pm4py's hash-seed dependence on BPMN inputs; weighted
easy soundness on 5 of 36 parallel-arc probe nets; the
`/api/evaluate` alignment `average_trace_fitness` last bit on 3 of 257 logs
(pm4py sums per variant); BOSE norm ties decided by the BLAS kernel
(pm4py's results depend on the CPU; Minera models the Windows kernel); the
OCEL 1.0 JSON reader reads an empty timestamp in file order (pm4py raises;
the classic format is the native transport), and repeated object ids in
the XML and SQLite readers stay refused (pm4py keeps a SQLite event or object
per type-table row; native reads the first), a SQLite type missing from the
map table falls back to native table names (pm4py raises); `merge_dataframe_cases` and gzip decompression are
not polled for cancellation; pm4py's default trace-based feature selection
types a mixed-type key by the first value in Python set order (random with
strings or NaN), Minera reads it as one-hot when any value is a string;
pm4py's SNA, log-variant roles and resource profiles key each NaN resource
cell apart (float identity, several unequal `nan` nodes), Minera leaves NaN
cells out (a datetime column's NaT cells are one `NaT` resource in the
log-variant roles, BB1-12); an all-resources profile request raises when
pm4py would raise for any one resource; an XES object column with two
unequal values of one `str()` (the text `'5'` and the int `5`) is one name
to Minera (roles merge them, local diagnostics refuse); the rework filter
names two missing keys in a fixed order where pm4py's set order follows
the hash seed.

Suite before each push (about 2 h at -j2; run it from one session only):
touch the changed sources, `cmake --build build -j2`, `ctest --test-dir build
-j2 --timeout 1800`, `npx tsc --noEmit -p .` and `npx vitest run
--maxWorkers=2` in `src/frontend`, `python -m unittest discover -s
scripts/tests`, then replay every `src/frontend/tests/fixtures/*manifest*.json`
with `scripts/capture-workbench-fixtures.py --origin <local minera_server>`
(no `--write`) and read the full output. Use a freshly started server (the
active-source manifest depends on the server's state) that no other client
uses at the same time (routes read the latest mined session). The 16
`original-*` bundle manifests and `literal-controls` are not replayable
(the script raises KeyError on them); 16 manifests replay. The script tests
need a Python with duckdb (`/tmp/refvenv/bin/python`). `minera_portable_integers` fails on
any `__int128` or compiler builtin in core code. CI (Linux, macOS, Windows)
runs on every push to `main` since the repository became public on
2026-11-10; integrations cannot rerun or dispatch a workflow, so only a
push or Philipp's Re-run starts one.
