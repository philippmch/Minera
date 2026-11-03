#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <type_traits>
#include <string>
#include <vector>

#include "minera/conformance.hpp"
#include "minera/petri_net.hpp"

namespace minera::detail {

// One aligned trace variant as automaton-after-align precision sees it: the
// variant's trace count and the visible model moves of one optimal alignment.
struct AutomatonPrecisionPath {
  std::uint64_t count = 0;
  std::vector<ProjectedAlignmentMove> moves;
};

struct AutomatonPrecisionScore {
  bool computed = true;
  double precision = 1.0;
};

// Token-based ETConformance precision as the reference evaluator computes it:
// each distinct proper prefix (labels joined with commas and split again, as
// the reference prefix utility does) is replayed on its own with
// stop-immediately-unfit and no final routing, and the labels eventually
// enabled at the reached marking are scored against the next activities the
// log takes; the empty prefix scores the initial marking. Markings, element
// names, arc order, the replay variant and the activity selector come from
// `replay_options`. Throws std::runtime_error when a silent search budget is
// exhausted.
double token_etconformance_precision(const EventLog& log, const PetriNet& net,
                                     TokenReplayOptions replay_options);

// One step of a scored sequence: `text` is the event's string label (null or
// typed activities have none); `identity` distinguishes label-less steps.
struct TokenPrecisionStep {
  std::optional<std::string> text;
  std::string identity;
};
struct TokenPrecisionSequence {
  std::vector<TokenPrecisionStep> steps;
  std::uint64_t count = 1;
};
// The same computation over already selected activity sequences, so callers
// that resolved activities with their own selector (check_conformance) score
// exactly the steps they replayed.
double token_etconformance_precision(const std::vector<TokenPrecisionSequence>& sequences,
                                     const PetriNet& net,
                                     TokenReplayOptions replay_options);

// The distinct proper prefixes of token precision as a trie. Node 0 is the
// empty prefix; node n > 0 extends parent[n] by one replayed event whose
// sample is labels[label[n]] (the event a materialised prefix trace would
// carry). `scored` lists the nodes of the proper prefixes in the order the
// reference creates them (first occurrence in the log).
struct TokenPrefixTrie {
  std::vector<std::size_t> parent{0};
  std::vector<std::size_t> label{0};
  std::vector<Event> labels;
  std::vector<std::size_t> scored;
};

// Forward replay of every scored prefix, as replay_token_result would replay
// the materialised prefix log, without materialising it: the prefixes are
// walked along the trie in `scored` order, a prefix's replay continues from
// the state its ancestor reached, and enabled transitions are cached by
// marking. `visit(i, enabled)` receives the enabled transitions at the
// marking scored prefix i reached, or nullptr when that prefix is unfit.
// Validation, exceptions and their order match the materialised replay.
// Requires forward replay options with stop_immediately_unfit and without
// final routing.
template <class Visit>
void forward_token_prefix_walk(const PetriNet& net, const TokenReplayOptions& options,
                               const TokenPrefixTrie& trie, Visit&& visit);
void forward_token_prefix_walk_impl(
    const PetriNet& net, const TokenReplayOptions& options, const TokenPrefixTrie& trie,
    void (*visit)(void*, std::size_t, const std::vector<std::size_t>*), void* context);
template <class Visit>
void forward_token_prefix_walk(const PetriNet& net, const TokenReplayOptions& options,
                               const TokenPrefixTrie& trie, Visit&& visit) {
  forward_token_prefix_walk_impl(
      net, options, trie,
      [](void* context, std::size_t index, const std::vector<std::size_t>* enabled) {
        (*static_cast<std::remove_reference_t<Visit>*>(context))(index, enabled);
      },
      &visit);
}

// Labels the evaluator observes as eventually enabled at a marking, searching
// silent transitions in transition-name order (index order without names).
bool eventually_enabled_labels(const PetriNet& net, const std::vector<int>& marking,
                               const std::vector<std::string>& transition_names,
                               std::size_t max_states, bool use_task_ids,
                               std::set<std::string>& labels);

// Scores the prefix automaton of the given alignments. Reference ties between
// equally cheap alignments change the paths, so equivalence checks score every
// optimal choice through this same function. A cache keeps the labels
// available at each marking across calls on the same net and options.
AutomatonPrecisionScore automaton_after_align_precision(
    const PetriNet& net, const std::vector<int>& initial_marking,
    const std::vector<AutomatonPrecisionPath>& paths,
    const std::vector<std::string>& transition_names, std::size_t max_states,
    bool use_task_ids,
    std::map<std::vector<int>, std::set<std::string>>* available_cache = nullptr);

}  // namespace minera::detail
