#pragma once

// Reference-shaped building blocks of the classic genetic miner (PM4Py 2.7.22
// algo.discovery.genetic). A causal matrix maps every transition to ordered
// lists of input and output partitions; each partition is a set of
// transition names. The deterministic steps (workflow source/sink selection,
// matrix-to-net conversion, partition edits, fitness and tournament order)
// follow the reference exactly. The evolutionary operators are seeded-random
// in the reference and also depend on Python's string-hash seed, so only their
// structural invariants are part of the contract (see
// reference_genetic_matrix_is_well_formed).

#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "minera/event_log.hpp"
#include "minera/petri_analysis.hpp"
#include "minera/petri_net.hpp"

namespace minera {

// One partition is a set: members are kept sorted and unique.
using ReferenceGeneticPartition = std::vector<std::string>;
using ReferenceGeneticPartitions = std::vector<ReferenceGeneticPartition>;
using ReferenceGeneticMap = std::map<std::string, ReferenceGeneticPartitions>;

struct ReferenceGeneticMatrix {
  std::vector<std::string> transitions;  // the reference's T order
  ReferenceGeneticMap inputs;
  ReferenceGeneticMap outputs;
};

class ReferenceGeneticError : public std::invalid_argument {
 public:
  ReferenceGeneticError(std::string python_type, const std::string& message)
      : std::invalid_argument(message), python_type(std::move(python_type)) {}
  std::string python_type;
};

struct ReferenceGeneticSourcesSinks {
  std::vector<std::string> sources;
  std::vector<std::string> sinks;
};

// get_src_sink_sets_for_wfnet: sources depend on the output partitions only,
// sinks on the input partitions only.
[[nodiscard]] ReferenceGeneticSourcesSinks reference_genetic_sources_sinks(
    const ReferenceGeneticMatrix& matrix);

// util.add_singleton_partition / util.remove_transition_from_partitions,
// editing the map in place like the reference.
void reference_genetic_add_singleton_partition(ReferenceGeneticMap& map,
                                               const std::string& key,
                                               const std::string& transition);
void reference_genetic_remove_transition_from_partitions(
    ReferenceGeneticMap& map, const std::string& key,
    const std::string& transition);

enum class ReferenceGeneticPlaceKind {
  kStart,   // the reference's place "i"
  kEnd,     // the reference's place "o"
  kShared,  // a consistent (X, Y) place
  kOutput,  // o(x, Y): an output partition without a shared place
  kInput,   // i(y, X): an input partition without a shared place
};

struct ReferenceGeneticPlace {
  ReferenceGeneticPlaceKind kind = ReferenceGeneticPlaceKind::kStart;
  std::string transition;  // x for kOutput, y for kInput
  ReferenceGeneticPartition producers;  // X (kShared, kInput)
  ReferenceGeneticPartition consumers;  // Y (kShared, kOutput)
};

// The reference names places with Python frozenset reprs, whose member order
// follows the string-hash seed; this key is the seed-independent form.
[[nodiscard]] std::string reference_genetic_place_key(
    const ReferenceGeneticPlace& place);

struct ReferenceGeneticTransition {
  std::optional<std::string> label;  // empty for the reference's silent ""
  std::vector<std::size_t> inputs;   // place indexes, sorted by key
  std::vector<std::size_t> outputs;
};

struct ReferenceGeneticNet {
  std::vector<ReferenceGeneticPlace> places;  // start and end first
  std::vector<ReferenceGeneticTransition> transitions;
  std::size_t start = 0;
  std::size_t end = 1;
};

// matrix2petrinet. Unknown transitions referenced by a partition raise the
// reference's KeyError.
[[nodiscard]] ReferenceGeneticNet reference_genetic_net(
    const ReferenceGeneticMatrix& matrix);

// The reference's place name with set members in sorted order, e.g.
// "(frozenset({'a'}), frozenset({'b', 'c'}))" or "o('a', frozenset({'b'}))".
// The reference prints members in string-hash order, so this is its name
// under any seed where that order happens to be sorted.
[[nodiscard]] std::string reference_genetic_place_name(
    const ReferenceGeneticPlace& place);

// "label inputs outputs" with place keys joined by ';' ("-" for silent or
// empty); behaviorally identical transitions share a key.
[[nodiscard]] std::string reference_genetic_transition_key(
    const ReferenceGeneticNet& net, std::size_t transition);

// Per place key, the transition keys of its outgoing arcs in the order the
// reference iterates them. Token replay breaks equal-length silent-path ties
// by this order; for nets the reference builds in memory it follows object
// identity, so it is an observed input rather than something derivable.
using ReferenceGeneticArcOrder = std::map<std::string, std::vector<std::string>>;

// What token replay observes of a net the reference built in memory: equal
// silent paths are ordered by place name first, then by arc order. Names
// default to reference_genetic_place_name; nets from apply hold set (not
// frozenset) partitions and so print differently.
struct ReferenceGeneticReplayOrder {
  ReferenceGeneticArcOrder arcs;
  std::map<std::string, std::string> place_names;  // by place key
};

struct ReferenceGeneticPetriNet {
  PetriNet net;
  WorkflowMarkings markings;
  // One row per place: the transitions consuming from it, in replay order.
  std::vector<std::vector<std::size_t>> outgoing_order;
  std::vector<std::string> place_names;
};

// Without an observed order, a place's outgoing transitions follow the net's
// transition order (visible transitions first, then silent ones by edge).
[[nodiscard]] ReferenceGeneticPetriNet reference_genetic_petri_net(
    const ReferenceGeneticNet& net, const ReferenceGeneticReplayOrder* order = nullptr);

// Fitness used by the reference tournament:
// 0.4 * average trace fitness + 0.6 * share of fitting traces (token replay).
[[nodiscard]] double reference_genetic_fitness(
    const ReferenceGeneticMatrix& matrix, const EventLog& log,
    const ReferenceGeneticReplayOrder* order = nullptr);

// tournament(population, log, T, sort): fitness of every individual and,
// when sorted, the population order by descending fitness (stable).
struct ReferenceGeneticTournament {
  std::vector<std::size_t> order;
  std::vector<double> fitness;  // aligned with order
};
[[nodiscard]] ReferenceGeneticTournament reference_genetic_tournament(
    const std::vector<ReferenceGeneticMatrix>& population, const EventLog& log,
    bool sort = true, const std::vector<ReferenceGeneticReplayOrder>* orders = nullptr);

// Structural invariants that every individual produced by the reference's
// individuals, crossover, mutate and repair satisfies: maps keyed by exactly
// the transitions, partitions nonempty, disjoint within a transition, drawn
// from the transitions and not repeated, and every output member mirrored by
// an input member (y in some O[x] iff x in some I[y]).
[[nodiscard]] bool reference_genetic_matrix_is_well_formed(
    const ReferenceGeneticMatrix& matrix);

}  // namespace minera
