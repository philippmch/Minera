#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "minera/dfg_variants.hpp"
#include "minera/directly_follows_graph.hpp"

namespace minera {

// A weighted adjacency map.  In an outgoing map the outer key is the source
// activity; in an incoming map it is the target activity.  Maps and sets are
// used throughout this utility API so that results do not depend on input
// iteration order.
using DfgAdjacency =
    std::map<std::string, std::map<std::string, std::uint64_t>>;
using DfgActivitySet = std::set<std::string>;
using DfgActivityList = std::vector<std::string>;
using DfgRelation = std::map<std::string, DfgActivitySet>;
using DfgDirectionScores = std::map<std::string, double>;
using DfgDirectionRanking = std::vector<std::pair<std::string, double>>;

[[nodiscard]] DfgAdjacency outgoing_adjacency(const DfgFrequency& dfg);

[[nodiscard]] DfgAdjacency incoming_adjacency(const DfgFrequency& dfg);

[[nodiscard]] DfgActivityList infer_start_activities(
    const DfgFrequency& dfg);

[[nodiscard]] DfgActivityList infer_end_activities(const DfgFrequency& dfg);

[[nodiscard]] DfgActivitySet infer_start_activities_from_parent(
    const DfgFrequency& parent_dfg, const DfgFrequency& current_dfg,
    const DfgActivitySet& activities, bool include_current = true);

[[nodiscard]] DfgActivitySet infer_end_activities_from_parent(
    const DfgFrequency& parent_dfg, const DfgFrequency& current_dfg,
    const DfgActivitySet& activities, bool include_current = true);

// These two helpers expose the outside side of a boundary chain.  They inspect
// the outside activity touching a current boundary and then return its other
// outside neighbours.  Consequently, X -> Y -> A returns no output for the
// first helper: Y has no outside successor in that orientation.
[[nodiscard]] DfgActivitySet outside_outputs_to_current_starts(
    const DfgFrequency& parent_dfg, const DfgFrequency& current_dfg,
    const DfgActivitySet& activities);

[[nodiscard]] DfgActivitySet outside_inputs_from_current_ends(
    const DfgFrequency& parent_dfg, const DfgFrequency& current_dfg,
    const DfgActivitySet& activities);

[[nodiscard]] DfgActivitySet alphabet(const DfgFrequency& dfg);

[[nodiscard]] DfgActivityList activity_names(const DfgFrequency& dfg);

// The reference helper uses -1 when no incident edge exists.  Native callers
// receive the same information as nullopt without narrowing unsigned counts.
[[nodiscard]] std::optional<std::uint64_t> maximum_incident_edge(
    const DfgFrequency& dfg, const std::string& activity);

// A missing adjacency row is an invalid activity lookup and throws
// std::out_of_range; an existing empty row sums to zero.
[[nodiscard]] std::uint64_t sum_adjacency_values(
    const DfgAdjacency& adjacency, const std::string& activity);

// The reference helper uses -1 for an empty graph or an activity selection
// with no incident row; native callers receive that sentinel as nullopt.
[[nodiscard]] std::optional<std::uint64_t> maximum_incident_sum(
    const DfgFrequency& dfg);

[[nodiscard]] std::optional<std::uint64_t> maximum_incident_sum_for_activities(
    const DfgFrequency& dfg, const DfgActivitySet& activities);

[[nodiscard]] std::uint64_t start_activity_total(const DfgFrequency& dfg);

[[nodiscard]] std::uint64_t end_activity_total(const DfgFrequency& dfg);

// The compatibility halving rule adds both incident sides and halves the
// running total after every selected activity that has each side.  The
// selected activity order is therefore significant; callers needing the
// historical behavior should pass a vector in its original order.
[[nodiscard]] std::uint64_t activity_total(
    const DfgFrequency& dfg, const DfgActivityList& activities,
    bool enable_halving = true);

[[nodiscard]] std::uint64_t activity_total(
    const DfgFrequency& dfg, const DfgActivitySet& activities,
    bool enable_halving = true);

[[nodiscard]] DfgFrequency project_dfg(const DfgFrequency& dfg,
                                       const DfgActivitySet& activities);

[[nodiscard]] DfgFrequency remove_reciprocal_edges(const DfgFrequency& dfg);

[[nodiscard]] DfgDirectionScores activity_direction_scores(
    const DfgFrequency& dfg);

[[nodiscard]] DfgDirectionScores activity_direction_scores(
    const DfgFrequency& dfg, const DfgActivitySet& activities);

[[nodiscard]] DfgDirectionRanking rank_activity_directions(
    const DfgDirectionScores& scores);

[[nodiscard]] DfgActivityList self_loop_activities(const DfgFrequency& dfg);

// Returns connected components of the undirected interpretation of the
// supplied adjacency maps.  Explicit activities are retained as singleton
// components by default, even when they have no incident edge.
[[nodiscard]] std::vector<DfgActivitySet> undirected_connected_components(
    const DfgAdjacency& incoming, const DfgAdjacency& outgoing,
    const DfgActivitySet& activities = {},
    bool include_isolated_activities = true);

// Adds an unassigned activity to the component with the greatest weighted
// connection to it.  Equal scores keep the first component, making the
// result deterministic.  The input vector is not modified.  An empty
// component collection is invalid and throws std::invalid_argument.
[[nodiscard]] std::vector<DfgActivitySet> assign_to_strongest_component(
    const std::vector<DfgActivitySet>& components,
    const std::string& activity, const DfgAdjacency& incoming,
    const DfgAdjacency& outgoing);

[[nodiscard]] DfgActivitySet direct_successors(const DfgFrequency& dfg,
                                               const std::string& activity);

[[nodiscard]] DfgActivitySet direct_predecessors(const DfgFrequency& dfg,
                                                 const std::string& activity);

struct DfgDenseMatrix {
  std::vector<std::vector<double>> values;
  std::map<std::size_t, std::string> index_to_activity;
  std::map<std::string, std::size_t> activity_to_index;

  [[nodiscard]] std::size_t size() const noexcept { return values.size(); }

  [[nodiscard]] const std::vector<double>& operator[](
      std::size_t index) const {
    return values.at(index);
  }
};

[[nodiscard]] DfgDenseMatrix make_dfg_matrix(const DfgFrequency& dfg);

struct DfgVariantSummary {
  DfgFrequency graph;
  DfgActivityList activities;
  std::map<std::string, std::uint64_t> start_activities;
  std::map<std::string, std::uint64_t> end_activities;
};

// A sequence collection follows the set-based variant utility contract:
// duplicate sequences are collapsed and each distinct non-empty sequence
// contributes once to the summary.
[[nodiscard]] DfgVariantSummary summarize_variants(
    const std::vector<DfgActivityList>& variants);

// TraceVariant carries an occurrence count.  This separate native extension
// preserves those counts rather than changing the exact sequence utility's
// set-based semantics.
[[nodiscard]] DfgVariantSummary summarize_variant_counts(
    const std::vector<TraceVariant>& variants);

class DfgDirectedGraph {
 public:
  [[nodiscard]] const DfgActivitySet& activities() const noexcept {
    return activities_;
  }

  [[nodiscard]] const std::set<DfgActivityPair>& edges() const noexcept {
    return edges_;
  }

  [[nodiscard]] const DfgActivitySet& successors(
      const std::string& activity) const;

  [[nodiscard]] const DfgActivitySet& predecessors(
      const std::string& activity) const;

  [[nodiscard]] bool has_edge(const std::string& source,
                              const std::string& target) const;

  [[nodiscard]] bool has_activity(const std::string& activity) const;

 private:
  friend DfgDirectedGraph make_directed_graph(const DfgFrequency&);
  friend DfgDirectedGraph make_directed_graph(const DfgFrequency&,
                                              const DfgActivitySet&);

  DfgActivitySet activities_;
  std::set<DfgActivityPair> edges_;
  std::map<std::string, DfgActivitySet> successors_;
  std::map<std::string, DfgActivitySet> predecessors_;
};

[[nodiscard]] DfgDirectedGraph make_directed_graph(const DfgFrequency& dfg);

[[nodiscard]] DfgDirectedGraph make_directed_graph(
    const DfgFrequency& dfg, const DfgActivitySet& activities);

[[nodiscard]] DfgRelation transitive_successors(const DfgFrequency& dfg);

[[nodiscard]] DfgRelation transitive_successors(
    const DfgFrequency& dfg, const DfgActivitySet& activities);

[[nodiscard]] DfgRelation transitive_predecessors(const DfgFrequency& dfg);

[[nodiscard]] DfgRelation transitive_predecessors(
    const DfgFrequency& dfg, const DfgActivitySet& activities);

struct DfgTransitiveRelations {
  DfgRelation predecessors;
  DfgRelation successors;
};

[[nodiscard]] DfgTransitiveRelations transitive_relations(
    const DfgFrequency& dfg);

[[nodiscard]] DfgTransitiveRelations transitive_relations(
    const DfgFrequency& dfg, const DfgActivitySet& activities);

}  // namespace minera
