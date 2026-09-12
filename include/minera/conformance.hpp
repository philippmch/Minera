#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <string>
#include <type_traits>
#include <vector>

#include "minera/event_data.hpp"
#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"
#include "minera/process_tree.hpp"
#include "minera/root_cause.hpp"

namespace minera {

// Token-replay fitness of a single trace variant against the model.
struct VariantConformance {
  std::vector<std::string> activities;  // activity names, in trace order
  std::uint64_t count = 0;              // traces collapsing to this variant
  double fitness = 1.0;                 // token-replay fitness; early stop can be negative
  // Null labels use an empty display string; these offsets distinguish them
  // from actual empty-string activities.
  std::vector<std::size_t> null_activity_indices;
};

// Per-place counters include the final-marking check.
struct TokenReplayPlaceDiagnostics {
  std::uint64_t missing = 0;
  std::uint64_t remaining = 0;
  std::uint64_t consumed = 0;
  std::uint64_t produced = 0;
};

// Transitions examined for one source event. Silent pre-event routing precedes
// visible firings; final-marking routing belongs only to the trace-level list.
struct TokenReplayEventDiagnostics {
  std::size_t event_index = 0;
  std::vector<std::size_t> activated_transitions;
  std::vector<std::size_t> transitions_with_problems;
};

struct TokenReplayTraceDiagnostics {
  std::size_t trace_index = 0;
  std::string case_id;
  bool trace_is_fit = true;
  double trace_fitness = 1.0;
  std::uint64_t produced = 0;
  std::uint64_t consumed = 0;
  std::uint64_t missing = 0;
  std::uint64_t remaining = 0;
  std::vector<std::size_t> activated_transitions;
  // Weak-fired visible transitions, or the blocked transition when forward
  // immediate-stop prevents firing. Repeated problems retain replay order.
  std::vector<std::size_t> transitions_with_problems;
  std::vector<std::size_t> enabled_transitions;
  std::vector<std::size_t> unknown_event_indices;
  std::vector<std::string> unknown_activities;
  std::vector<bool> unknown_activity_is_null;
  // State after optional final routing, before final tokens are consumed.
  std::vector<int> reached_marking;
  std::vector<TokenReplayPlaceDiagnostics> places;
  bool replay_stopped = false;
  // Examined events, including an event that causes immediate stopping.
  std::size_t replayed_events = 0;
  // One row per examined event, including unknown labels and blocked events.
  std::vector<TokenReplayEventDiagnostics> event_diagnostics;
  // Membership of the reached marking, including explicit zero-token keys.
  // Forward replay supplies one entry per place; older/backwards rows may omit it.
  std::vector<bool> reached_marking_keys;
};

struct TokenReplayDurationComparison {
  std::vector<std::size_t> fit_trace_indices;
  std::vector<std::size_t> problem_trace_indices;
  std::size_t fit_count = 0;
  std::size_t problem_count = 0;
  double fit_median_seconds = 0.0;
  double problem_median_seconds = 0.0;
  double relative_throughput = 0.0;
};

struct TokenReplayTransitionDurationDiagnostics : TokenReplayDurationComparison {
  std::size_t transition_index = 0;
};

struct TokenReplayUnknownActivityDurationDiagnostics : TokenReplayDurationComparison {
  std::string activity;
  bool activity_is_null = false;
};

struct TokenReplayDurationDiagnostics {
  std::string timestamp_attribute = "time:timestamp";
  std::vector<TokenReplayTransitionDurationDiagnostics> transitions;
  std::vector<TokenReplayUnknownActivityDurationDiagnostics> unknown_activities;
};

struct TokenReplayTransitionDurationCohort {
  std::size_t transition_index = 0;
  std::optional<std::string> activity;
  std::vector<std::size_t> problem_trace_indices;
};

struct TokenReplayUnknownActivityDurationCohort {
  std::string activity;
  bool activity_is_null = false;
  std::vector<std::size_t> problem_trace_indices;
};

struct TokenReplayTransitionRootCauseDiagnostics : RootCauseModel {
  std::size_t transition_index = 0;
};

struct TokenReplayUnknownRootCauseDiagnostics : RootCauseModel {
  std::string activity;
  bool activity_is_null = false;
  std::optional<EventAttribute> activity_attribute;
};

struct TokenReplayRootCauseDiagnostics {
  RootCauseOptions options;
  std::vector<TokenReplayTransitionRootCauseDiagnostics> transitions;
  std::vector<TokenReplayUnknownRootCauseDiagnostics> unknown_activities;
};

// Context entries identify representative source traces. Diagnostic helpers
// expand value-equal traces in source order and keep the first supplied context
// for each equivalence class; problem membership takes precedence over fit.
struct TokenReplayTransitionRootCauseCohort {
  std::size_t transition_index = 0;
  std::vector<RootCauseObservation> fit_contexts;
  std::vector<RootCauseObservation> problem_contexts;
};

struct TokenReplayUnknownRootCauseCohort {
  std::string activity;
  bool activity_is_null = false;
  std::vector<RootCauseObservation> problem_contexts;
  // Explicit scalar identity takes precedence over the legacy string/null.
  std::optional<EventAttribute> activity_attribute;
};

// Conformance of an event log against a discovered model: the four standard
// quality dimensions. Token-based replay fitness (does the model reproduce the
// log?), escaping-edges precision (does the model allow much more than the
// log?), generalization (will the model likely accommodate unseen but related
// behaviour, judged by how evenly replay exercises its transitions?), and
// simplicity (is the net itself lean, judged by its mean arc degree?).
struct ConformanceResult {
  double fitness = 1.0;         // log-level token fitness; early stop can be negative
  double precision = 1.0;       // escaping-edges precision in [0, 1]
  double generalization = 1.0;  // token-replay generalization in [0, 1]
  double simplicity = 1.0;      // inverse arc-degree simplicity in (0, 1]

  std::uint64_t fitting_traces = 0;  // traces replayed with no deviation
  std::uint64_t trace_count = 0;

  // Aggregate token-replay bookkeeping, summed over all traces.
  std::uint64_t produced = 0;
  std::uint64_t consumed = 0;
  std::uint64_t missing = 0;
  std::uint64_t remaining = 0;
  std::vector<std::uint64_t> transition_frequencies;

  std::vector<VariantConformance> variants;  // worst-fitting first
  // Event field used to form replay variants. Standard aliases such as
  // "concept:name" and "activity" use Event::activity; other names resolve
  // through Event::attribute.
  std::string activity_attribute = "concept:name";
  std::optional<std::vector<TokenReplayTraceDiagnostics>> trace_diagnostics;
  std::optional<TokenReplayDurationDiagnostics> duration_diagnostics;
  std::optional<TokenReplayRootCauseDiagnostics> root_cause_diagnostics;
};

// Token-replay implementation variation. FORWARD is the modern replay used
// by default; BACKWARDS retains the deprecated reverse-routing behaviour.
enum class TokenReplayVariant {
  kForward,
  kBackwards,
  FORWARD = kForward,
  BACKWARDS = kBackwards,
};

struct TokenReplayOptions {
  TokenReplayVariant variant = TokenReplayVariant::kForward;
  bool stop_immediately_unfit = false;
  // Event field used to form replay variants. Standard aliases such as
  // "concept:name" and "activity" use Event::activity; other names resolve
  // through Event::attribute.
  std::string activity_attribute = "concept:name";
  // Fitting classification is separate from the numeric token-fitness formula.
  // Unknown activities do not change token counters; the opt-in policy marks
  // a trace unfit when an unknown activity is encountered.
  bool consider_remaining_in_fitness = true;
  bool try_to_reach_final_marking_through_hidden = true;
  bool consider_activities_not_in_model_in_fitness = false;
  // The three policies above apply to forward replay. Backwards replay keeps
  // its legacy contract: unknown labels are ignored, no final routing occurs,
  // and remaining tokens always make the trace unfit.
  // Empty vectors select the workflow-net defaults (one token at source and
  // one token at sink). Explicit markings must match the net's place count
  // and contain only non-negative token counts.
  std::vector<int> initial_marking;
  std::vector<int> final_marking;
  // Controls routing before visible events in both variations. Final routing
  // remains independent; the backwards variation never routes at trace end.
  bool walk_through_hidden_transitions = true;
  bool include_trace_diagnostics = false;
  // Flat input case selector. Already grouped Trace boundaries remain authoritative.
  std::string case_id_attribute = "case:concept:name";
  // Hard input event cap; exceeding it raises length_error, never truncates.
  std::optional<std::size_t> max_rows_to_materialize;
  bool include_duration_diagnostics = false;
  std::string timestamp_attribute = "time:timestamp";
  bool include_root_cause_diagnostics = false;
  RootCauseOptions root_cause;
  // Forward replay can remove stale marking entries after forced firings.
  // Each removed entry contributes one remaining occurrence; off by default.
  bool cleaning_token_flood = false;
  // Optional model IDs used for deterministic routing and duplicate labels.
  // Empty defaults to p0, p1, ... and t0, t1, ... respectively.
  std::vector<std::string> place_names;
  std::vector<std::string> transition_names;
  // Optional membership of initial/final markings, retaining zero-token keys.
  // Empty derives membership from positive counts. Nonempty vectors must have
  // one entry per place and include every positive-token place.
  std::vector<bool> initial_marking_keys;
  std::vector<bool> final_marking_keys;
  // Forward pre-event routing policy. The public exhaustive mode limits
  // recursive dependency exploration to the outer invocation.
  bool exhaustive_invisible_exploration = false;
};

[[nodiscard]] std::vector<TokenReplayTransitionRootCauseDiagnostics>
token_replay_transition_root_cause_diagnostics(
    const EventLog& log, const std::vector<TokenReplayTransitionRootCauseCohort>& cohorts,
    const RootCauseOptions& options = {});
[[nodiscard]] std::vector<TokenReplayUnknownRootCauseDiagnostics>
token_replay_unknown_root_cause_diagnostics(
    const EventLog& log, const std::vector<TokenReplayUnknownRootCauseCohort>& cohorts,
    const TokenReplayOptions& options = {});
[[nodiscard]] TokenReplayRootCauseDiagnostics compute_token_replay_root_cause_diagnostics(
    const EventLog& log, const PetriNet& net,
    const std::vector<TokenReplayTraceDiagnostics>& traces,
    const TokenReplayOptions& options = {});

// Seconds between the first and last source-order events. Empty traces and
// missing/null/invalid endpoint timestamps fail; intermediate events are not read.
[[nodiscard]] double token_replay_case_duration(
    const Trace& trace, const std::string& timestamp_attribute = "time:timestamp");
// Repeated indices retain their weight. An empty selection fails.
[[nodiscard]] double token_replay_median_case_duration(
    const EventLog& log, const std::vector<std::size_t>& trace_indices,
    const std::string& timestamp_attribute = "time:timestamp");
// Cohort membership is by source trace index, deduplicated before comparison.
// The baseline contains traces with the selected activity matching the transition
// label, excluding that row's problem cohort. Missing activities never match null.
// Rows with an empty side are omitted; nonpositive baseline medians give ratio 0.
[[nodiscard]] std::vector<TokenReplayTransitionDurationDiagnostics>
token_replay_transition_duration_diagnostics(
    const EventLog& log, const std::vector<TokenReplayTransitionDurationCohort>& cohorts,
    const TokenReplayOptions& options = {});
// Baselines exclude traces containing any supplied activity key (even keys with
// empty problem cohorts), then exclude the current row's problem trace indices.
// activity_attribute selects the generic field used for both cohort filters.
[[nodiscard]] std::vector<TokenReplayUnknownActivityDurationDiagnostics>
token_replay_unknown_activity_duration_diagnostics(
    const EventLog& log, const std::vector<TokenReplayUnknownActivityDurationCohort>& cohorts,
    const TokenReplayOptions& options = {});
[[nodiscard]] TokenReplayDurationDiagnostics compute_token_replay_duration_diagnostics(
    const EventLog& log, const PetriNet& net,
    const std::vector<TokenReplayTraceDiagnostics>& traces,
    const TokenReplayOptions& options = {});

// Per-place token accounting for a replayed log.  The four counters use the
// same forced-firing semantics as ConformanceResult: missing tokens are
// created when an input is unavailable, while remaining tokens are measured
// after the final-marking check.
struct TokenReplayDiagnostics {
  std::uint64_t trace_count = 0;
  std::uint64_t fitting_traces = 0;
  std::vector<TokenReplayPlaceDiagnostics> places;
  std::vector<std::uint64_t> transition_activations;
};

// Replays `event_log` against `tree` and reports all four quality dimensions.
// The process tree is translated to a workflow net; fitness comes from
// token-based replay (Rozinat & van der Aalst), precision from escaping edges
// (Munoz-Gama & Carmona), generalization from the replay's per-transition
// execution counts (Buijs, van Dongen & van der Aalst), and simplicity from
// the net's mean arc degree.
[[nodiscard]] ConformanceResult check_conformance(const EventLog& event_log,
                                                  const ProcessTree& tree,
                                                  TokenReplayOptions options = {});

// Token replay against any workflow net (the Alpha Miner produces one directly;
// a process tree or heuristics net is translated with to_petri_net). Replay is
// robust on unsound nets, so unlike alignments it always yields a result.
[[nodiscard]] ConformanceResult check_conformance(const EventLog& event_log,
                                                  const PetriNet& net,
                                                  TokenReplayOptions options = {});

// Replays every trace without collapsing variants and retains the per-place
// token accounting and transition activation counts used by object-centric
// diagnostics.  The default variation matches check_conformance's forward
// replay; callers may request the backwards variation when needed.
[[nodiscard]] TokenReplayDiagnostics replay_token_diagnostics(
    const EventLog& event_log, const PetriNet& net,
    TokenReplayOptions options = {});

// One row per source trace, preserving duplicate cases and event order.
// Enabled transition ids are visible steps reachable through silent routing
// from reached_marking, subject to the replay engine's silent-search bound.
[[nodiscard]] std::vector<TokenReplayTraceDiagnostics> replay_token_traces(
    const EventLog& event_log, const PetriNet& net,
    TokenReplayOptions options = {});

// Flat replay projection does not require timestamps or reorder case events.
[[nodiscard]] EventLog normalize_token_replay_event_log(
    const EventDataView& data, const TokenReplayOptions& options = {});
[[nodiscard]] ConformanceResult check_conformance(
    const EventDataView& data, const PetriNet& net, TokenReplayOptions options = {});
[[nodiscard]] ConformanceResult check_conformance(
    const EventDataView& data, const ProcessTree& tree, TokenReplayOptions options = {});
[[nodiscard]] std::vector<TokenReplayTraceDiagnostics> replay_token_traces(
    const EventDataView& data, const PetriNet& net, TokenReplayOptions options = {});
[[nodiscard]] TokenReplayDiagnostics replay_token_diagnostics(
    const EventDataView& data, const PetriNet& net, TokenReplayOptions options = {});

template <typename Input>
concept FlatTokenReplayInput = std::is_same_v<Input, EventStream> ||
    std::is_same_v<Input, EventTable> || std::is_same_v<Input, LazyEventTable>;

template <FlatTokenReplayInput Input>
[[nodiscard]] inline ConformanceResult check_conformance(
    const Input& data, const PetriNet& net, TokenReplayOptions options = {}) {
  return check_conformance(EventDataView(data), net, options);
}
template <FlatTokenReplayInput Input>
[[nodiscard]] inline ConformanceResult check_conformance(
    const Input& data, const ProcessTree& tree, TokenReplayOptions options = {}) {
  return check_conformance(EventDataView(data), tree, options);
}
template <FlatTokenReplayInput Input>
[[nodiscard]] inline std::vector<TokenReplayTraceDiagnostics> replay_token_traces(
    const Input& data, const PetriNet& net, TokenReplayOptions options = {}) {
  return replay_token_traces(EventDataView(data), net, options);
}
template <FlatTokenReplayInput Input>
[[nodiscard]] inline TokenReplayDiagnostics replay_token_diagnostics(
    const Input& data, const PetriNet& net, TokenReplayOptions options = {}) {
  return replay_token_diagnostics(EventDataView(data), net, options);
}

// Footprint (directly-follows relation) conformance between a log and a model.
// The footprint of each is the set of ordered activity pairs (a, b) where b can
// directly follow a — the log's read straight off its traces, the model's found
// by exploring the net's reachable markings. Comparing the two sets tells apart
// behaviour the log shows but the model forbids (a fitness gap) from behaviour
// the model allows but the log never shows (a precision gap). Robust on unsound
// nets, so it yields a reading even where alignments cannot.
struct FootprintRelation {
  std::string source;  // activity a
  std::string target;  // activity b, which directly follows a
};

struct FootprintResult {
  double fitness = 1.0;    // share of log successions the model also allows
  double precision = 1.0;  // share of model successions the log also shows
  std::uint64_t log_relations = 0;     // |L|
  std::uint64_t model_relations = 0;   // |M|
  std::uint64_t shared_relations = 0;  // |L ∩ M|
  bool exhausted = false;  // model exploration hit its bound (relation partial)
  std::vector<FootprintRelation> log_only;    // in log, not model (fitness gap)
  std::vector<FootprintRelation> model_only;  // in model, not log (precision gap)
};

// Footprint conformance of a log against any workflow net, or a process tree
// (translated with to_petri_net). Cheap and robust: it explores only the model's
// reachable markings, so unlike alignments it produces a result even for an
// unsound net.
[[nodiscard]] FootprintResult check_footprints(const EventLog& event_log,
                                               const PetriNet& net);
[[nodiscard]] FootprintResult check_footprints(const EventLog& event_log,
                                               const ProcessTree& tree);

// One move of an optimal alignment between a trace and the model.
enum class AlignmentMoveType {
  kSynchronous,  // log and model agree on the activity — no deviation (cost 0)
  kLog,          // the log event has no matching model step here (cost 1)
  kModel,        // the model must fire a visible transition the log skips (cost 1)
};

// A single step of an alignment. Silent model moves are retained as routing
// steps with an empty activity; all other moves carry an activity name.
struct AlignmentMove {
  AlignmentMoveType type;
  std::string activity;  // the activity name involved in the move
  std::size_t transition = static_cast<std::size_t>(-1);  // model transition id
};

// Alignment algorithm family. The generator variants retain all
// equal-cost optimal paths (subject to AlignmentOptions::max_alignments).
enum class AlignmentVariant {
  kStateEquationAStar,
  kDijkstraNoHeuristics,
  kDijkstraLessMemory,
  kDiscountedAStar,
  kGeneratorDijkstraNoHeuristics,
  kGeneratorDijkstraLessMemory,
};

enum class AlignmentCostProfile { kLegacyUnit, kStandard };

struct AlignmentOptions {
  AlignmentVariant variant = AlignmentVariant::kDijkstraLessMemory;
  AlignmentCostProfile cost_profile = AlignmentCostProfile::kLegacyUnit;
  std::size_t max_states = 200000;
  double max_time_seconds = 0.0;  // per trace; zero means unlimited
  double max_total_time_seconds = 0.0;
  std::size_t max_alignments = 32;
  double exponent = 2.0;          // discounted A* depth exponent
  bool synchronous_dijkstra = true;
  // nullopt derives standard or legacy values from cost_profile.
  std::optional<double> default_trace_cost;
  std::optional<double> default_model_cost;
  std::optional<double> default_sync_cost;
  std::optional<double> default_silent_cost;
  std::unordered_map<std::string, double> trace_costs;
  std::unordered_map<std::string, double> model_costs;
  std::unordered_map<std::string, double> sync_costs;
  std::vector<double> trace_costs_by_position;
  std::vector<double> model_costs_by_transition;
  std::vector<double> sync_costs_by_transition;
  // Optional cost callables. The second argument is the trace event
  // index for trace costs and the transition index for model/sync costs.
  std::function<double(const std::string&, std::size_t)> trace_cost_function;
  std::function<double(const std::string&, std::size_t)> model_cost_function;
  std::function<double(const std::string&, std::size_t)> sync_cost_function;
  // Event field used to form alignment variants. Standard aliases such as
  // "concept:name" and "activity" use Event::activity; other names resolve
  // through Event::attribute.
  std::string activity_attribute = "concept:name";
  // Empty vectors select the workflow-net defaults (one token at source and
  // one token at sink). Explicit markings must match the net's place count
  // and contain only non-negative token counts.
  std::vector<int> initial_marking;
  std::vector<int> final_marking;
};

struct AlignmentDiagnostics {
  std::size_t visited_states = 0;
  std::size_t queued_states = 0;
  std::size_t traversed_arcs = 0;
  std::size_t lp_solved = 0;
  std::size_t closed_set_length = 0;
  std::size_t num_visited_markings = 0;
};

// The optimal (least-cost) alignment of one trace variant against the model.
struct ProjectedAlignmentMove {
  std::string activity;
  std::size_t transition = static_cast<std::size_t>(-1);
  std::vector<int> marking_before;
  std::vector<int> marking_after;
};
struct VariantAlignment {
  std::vector<std::string> activities;  // activity names, in trace order
  std::uint64_t count = 0;              // traces collapsing to this variant
  std::vector<AlignmentMove> moves;     // alignment, including silent routing
  double cost = 0.0;                    // total cost (log + visible model moves)
  double fitness = 1.0;                 // 1 - cost / worst-case cost, in [0, 1]
  std::vector<std::vector<AlignmentMove>> optimal_alignments;
  AlignmentDiagnostics diagnostics;
  double worst_cost = 0.0;  // denominator used for log-fitness
  // Visible model projection (synchronous + model-only moves); tau and
  // log-only moves are omitted. Transition ids remain available for duplicate
  // labels in evaluator variants that opt into task identities.
  std::vector<ProjectedAlignmentMove> projected_model_moves;
  // Full optimal path retained explicitly for decision mining. Silent model
  // transitions use empty activity names, so hidden decision routing remains
  // observable without reconstructing the search path.
  std::vector<AlignmentMove> complete_moves;
};

// Alignment-based conformance of an event log against a discovered process
// tree. Unlike token replay, an alignment finds for each trace the cheapest
// sequence of synchronous, log-only, and model-only moves that walks the model
// from its initial to its final marking — giving an exact, optimal account of
// where and how each trace deviates.
struct AlignmentResult {
  // False when the model could not be aligned. Two distinct causes, told apart
  // by `exhausted`: the final marking is unreachable (an unsound net), or the
  // search exceeded its bound on a reachable but unbounded/very large net. In
  // either case the other fields are meaningless and `variants` is empty.
  bool computed = true;
  bool exhausted = false;  // when !computed: true = search bound hit, false = unsound
  double fitness = 1.0;               // cost-based log fitness in [0, 1]
  // Alignment-based escaping-edges precision: like token-replay precision it
  // asks how much extra behaviour the model allows at the states the log
  // visits, but the states come from each variant's optimal alignment rather
  // than from forced token replay — so on a deviating log the markings are
  // the trustworthy ones an optimal explanation actually reaches.
  double precision = 1.0;
  std::uint64_t fitting_traces = 0;  // traces with a perfect (cost-0) alignment
  std::uint64_t trace_count = 0;
  double total_cost = 0.0;           // summed optimal cost over all traces
  AlignmentVariant variant = AlignmentVariant::kDijkstraLessMemory;
  AlignmentDiagnostics diagnostics;
  std::vector<VariantAlignment> variants;  // worst-fitting first
  // Event field used to form alignment variants.
  std::string activity_attribute = "concept:name";
};

// Replays each trace variant (a sequence of activity names) against the net
// and returns, per variant, the transitions its clean token replay fires: one
// inner list per consumed event — silent routing first, the event's visible
// transition last — plus one final list for the routing to the sink. A variant
// that does not replay cleanly (an activity the net lacks, a missing token, or
// leftover tokens at the end) yields nullopt instead, because a forced
// replay's firings would not be a real path through the model. Feeds decision
// mining, which must know which transition consumed which place's token.
[[nodiscard]] std::vector<std::optional<std::vector<std::vector<std::size_t>>>>
replay_firing_sequences(const PetriNet& net,
                        const std::vector<std::vector<std::string>>& variants);

// Per-activity tally of alignment moves across the whole log — the aggregate
// behind the per-variant alignments: how often the activity aligned
// synchronously, was inserted (a log move: the log shows it where the model
// has no step for it), or was skipped (a model move: the model requires it
// where the log lacks it), each weighted by how many traces the variant
// stands for.
struct ActivityDeviation {
  std::string activity;
  std::uint64_t synchronous = 0;
  std::uint64_t log_moves = 0;
  std::uint64_t model_moves = 0;
};

// Aggregates an alignment result's per-variant moves into per-activity
// deviation counts, most deviating activities (log + model moves) first, ties
// most frequent first and then alphabetical. Empty when the alignment was not
// computed. This is the log-level answer to "which activities does the model
// get wrong", which the per-variant listing buries on a many-variant log.
[[nodiscard]] std::vector<ActivityDeviation> summarize_alignment_moves(
    const AlignmentResult& alignment);

// Whether `net`'s final marking is reachable from its initial marking — the
// practical bar for being a sound workflow net, and the precondition for
// alignment. Cheap: a single shortest-run search. The Alpha Miner can produce
// nets that fail this (e.g. concurrent start/end activities overflow the sink).
[[nodiscard]] bool final_marking_reachable(const PetriNet& net);

// Computes optimal alignments of every trace variant against the model using a
// uniform-cost search over the synchronous product, and reports cost-based
// fitness overall and per variant. The standard cost function charges 1 for a
// log-only or visible model-only move and 0 for a synchronous or silent move
// (Adriansyah, van der Aalst & van Dongen).
//
// The process-tree overload translates the tree to a (always sound) workflow
// net; the Petri-net overload aligns against any net directly but, because a
// net from the Alpha Miner need not be sound, returns `computed == false`
// rather than misleading numbers when its final marking cannot be reached.
[[nodiscard]] AlignmentResult check_alignment(const EventLog& event_log,
                                              const ProcessTree& tree);
[[nodiscard]] AlignmentResult check_alignment(const EventLog& event_log,
                                              const PetriNet& net);
[[nodiscard]] AlignmentResult check_alignment(const EventLog& event_log,
                                              const PetriNet& net,
                                              const AlignmentOptions& options);
[[nodiscard]] AlignmentResult check_alignment(const EventLog& event_log,
                                              const ProcessTree& tree,
                                              const AlignmentOptions& options);

// The conformance of one discovered model, so the frontend can offer any miner
// against any method. `miner` is the UI registry id ("inductive", "alpha",
// "heuristic"). Token replay is always computed (it is cheap and robust);
// `alignments` is optional so the server can defer the costlier alignment to an
// on-demand request while the CLI export still embeds it.
struct ModelConformance {
  std::string miner;
  bool sound = true;  // final marking reachable — whether alignment is possible
  ConformanceResult token_replay;
  FootprintResult footprints;
  std::optional<AlignmentResult> alignments;
};

}  // namespace minera
