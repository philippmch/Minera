#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/event_log.hpp"
#include "minera/petri_net.hpp"
#include "minera/pnml.hpp"

namespace minera {

// Usage of one numbered net element in one or both logs.  IDs are stable net
// indices (rather than labels), so duplicate transition labels remain distinct.
struct PetriUsageRecord {
  std::size_t id = 0;
  std::uint64_t log1_occurrences = 0;
  std::uint64_t log2_occurrences = 0;
  std::uint64_t total_occurrences = 0;
  double direction = 0.0;  // (log2 - log1) / total, or zero for total == 0
  std::string label;
  std::string color;
};

enum class PetriArcUsageKind {
  kInput,
  kOutput,
  kReset,
  kInhibitor,
};

// Arc usage is associated with the transition that owns the arc. `id` is a
// deterministic ordinal in the flattened ordinary/reset/inhibitor arc lists;
// repeated place IDs therefore preserve weighted arcs instead of being lost.
struct PetriArcUsageRecord {
  std::size_t id = 0;
  std::size_t transition = 0;
  std::size_t place = 0;
  PetriArcUsageKind kind = PetriArcUsageKind::kInput;
  // Compatibility projection: true for every place-to-transition arc.
  bool input = true;
  std::uint64_t log1_occurrences = 0;
  std::uint64_t log2_occurrences = 0;
  std::uint64_t total_occurrences = 0;
  double direction = 0.0;
  std::string label;
  std::string color;
};

struct PetriUsageComparison {
  std::vector<PetriUsageRecord> places;
  std::vector<PetriUsageRecord> transitions;
  std::vector<PetriArcUsageRecord> arcs;
};

// Optional replay markings.  An empty vector keeps the workflow-net defaults
// (one token on source and one token on sink); supplied vectors must contain
// one entry for every place.  Token counts are bounded to uint32 at the API
// boundary and widened internally for checked replay arithmetic.
struct PetriUsageComparisonOptions {
  std::vector<std::uint32_t> initial_marking;
  std::vector<std::uint32_t> final_marking;
  // Optional event attribute used as the activity label during replay. The
  // canonical Event::activity field is used when this is empty, "activity",
  // or "concept:name". Missing selected keys are rejected; present nulls and
  // unknown labels activate no model transition. Leading and
  // trailing whitespace is ignored; a non-empty all-whitespace selector is
  // rejected.
  std::string activity_attribute;
  // Replay controls inherited by the comparison operation. Silent routing can
  // be disabled independently before visible events and after the trace. When
  // immediate stop is enabled, the first disabled visible transition ends
  // that trace before firing; unknown activities are skipped.
  bool walk_through_hidden_trans = true;
  bool try_to_reach_final_marking_through_hidden = true;
  bool stop_immediately_unfit = false;
  // Clean stale marked places after forced visible firings, using structural
  // components. Each removed place contributes one remaining occurrence.
  bool cleaning_token_flood = false;
  // Optional model place names in index order, used to break equally short
  // silent-routing choices. Empty defaults to the exported names p0, p1, ... .
  std::vector<std::string> place_names;
  // Optional transition names for duplicate-label replay choices. Empty uses
  // the exported names t0, t1, ... in native transition-index order.
  std::vector<std::string> transition_names;
  // Optional marking-key membership, including explicitly present zero-token
  // places. Empty derives membership from positive token counts. Nonempty
  // vectors must have one entry per place and include every positive place.
  std::vector<bool> initial_marking_keys;
  std::vector<bool> final_marking_keys;
  // Select the public exhaustive pre-event hidden-routing policy.
  bool exhaustive_invisible_exploration = false;
  // Physical PNML arcs. When present, arc usage has one record per arc object,
  // so a weight-2 arc stays one record and genuine parallel arcs stay two.
  std::optional<std::vector<PnmlArcOccurrence>> arc_occurrences;
};

// Dynamic comparison palette for a finite direction in [-1, 1]. Uses the
// public 256-bin color lookup contract; invalid directions are rejected.
[[nodiscard]] std::string petri_usage_direction_color(double direction);
// Static public palette with asymmetric neutral interval (-0.1, 0].
[[nodiscard]] std::string petri_usage_direction_color_static(double direction);

// Compares forward token-replay element activations for two logs.  Replay is
// deliberately robust: unknown activities and missing tokens are counted as
// deviations, while the corresponding forced transition still counts as
// activated unless immediate-stop is enabled. Place usage adds consumed tokens
// and the greater of residual tokens and required final tokens per place.
[[nodiscard]] PetriUsageComparison compare_petri_usage(
    const PetriNet& net, const EventLog& log1, const EventLog& log2,
    const PetriUsageComparisonOptions& options = {});

// Descriptive compatibility spelling for callers that use the family name.
[[nodiscard]] PetriUsageComparison compare_element_usage_two_logs(
    const PetriNet& net, const EventLog& log1, const EventLog& log2,
    const PetriUsageComparisonOptions& options = {});

// compare_element_usage_two_logs of the reference as it composes the public
// forward token replay: each log is replayed with PLTR fitness under
// `replay` (markings, names and arc order as for replay_token_result). A
// transition's usage is the number of times it appears in the traces'
// activated transitions; a place's usage is its consumed plus remaining
// tokens in the PLTR place fitness. Arcs carry their transition's record:
// one per physical arc when `arc_occurrences` is given, otherwise one per
// endpoint entry of the net's arc vectors.
[[nodiscard]] PetriUsageComparison compare_element_usage_token_replay(
    const PetriNet& net, const EventLog& log1, const EventLog& log2,
    TokenReplayOptions replay = {},
    const std::vector<PnmlArcOccurrence>* arc_occurrences = nullptr);

// compare_element_usage_token_replay with the comparison options mapped onto
// its token replay (markings, names, hidden routing, stop and flood controls).
// This is what the CLI and server run.
[[nodiscard]] PetriUsageComparison compare_petri_usage_reference(
    const PetriNet& net, const EventLog& log1, const EventLog& log2,
    const PetriUsageComparisonOptions& options = {});

}  // namespace minera
