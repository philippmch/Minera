#include "minera/dfg_utils.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace minera;

DfgFrequency graph_fixture() {
  return {{{"A", "B"}, 5},
          {{"B", "A"}, 2},
          {{"B", "C"}, 3},
          {{"D", "C"}, 7},
          {{"C", "C"}, 4},
          {{"X", "Y"}, 1}};
}

DfgFrequency cycle_fixture() {
  return {{{"A", "B"}, 1}, {{"B", "A"}, 1}, {{"B", "C"}, 1}};
}

void adjacency_and_boundary_contract() {
  const DfgFrequency dfg = graph_fixture();
  const auto outgoing = outgoing_adjacency(dfg);
  const auto incoming = incoming_adjacency(dfg);
  assert(outgoing.at("A").at("B") == 5);
  assert(outgoing.at("C").at("C") == 4);
  assert(incoming.at("A").at("B") == 2);
  assert(incoming.at("C").at("D") == 7);

  assert((infer_start_activities(dfg) == DfgActivityList{"D", "X"}));
  assert((infer_end_activities(dfg) == DfgActivityList{"Y"}));

  const DfgFrequency parent = {{{"Y", "A"}, 1}, {{"Y", "X"}, 1},
                                {{"W", "B"}, 1}, {{"W", "V"}, 1},
                                {{"B", "Z"}, 1}, {{"Q", "Z"}, 1}};
  const DfgFrequency current = {{{"A", "B"}, 1}};
  const DfgActivitySet activities{"A", "B"};
  assert((infer_start_activities_from_parent(parent, current, activities,
                                             false) ==
          DfgActivitySet{"A", "B"}));
  assert((infer_start_activities_from_parent(parent, current, activities) ==
          DfgActivitySet{"A", "B"}));
  assert((infer_end_activities_from_parent(parent, current, activities,
                                           false) == DfgActivitySet{"B"}));
  assert((infer_end_activities_from_parent(parent, current, activities) ==
          DfgActivitySet{"B"}));

  // The helpers return the second outside hop, not the outside activity that
  // directly touches the current boundary.
  assert((outside_outputs_to_current_starts(parent, current, activities) ==
          DfgActivitySet{"V", "X"}));
  assert((outside_inputs_from_current_ends(parent, current, activities) ==
          DfgActivitySet{"Q"}));
}

void activity_and_incident_count_contract() {
  const DfgFrequency dfg = graph_fixture();
  assert((alphabet(dfg) ==
          DfgActivitySet{"A", "B", "C", "D", "X", "Y"}));
  assert((activity_names(dfg) ==
          DfgActivityList{"A", "B", "C", "D", "X", "Y"}));
  assert(maximum_incident_edge(dfg, "B").value() == 5);
  assert(!maximum_incident_edge(dfg, "missing"));

  const auto outgoing = outgoing_adjacency(dfg);
  assert(sum_adjacency_values(outgoing, "B") == 5);
  bool missing_row_rejected = false;
  try {
    (void)sum_adjacency_values(outgoing, "missing");
  } catch (const std::out_of_range&) {
    missing_row_rejected = true;
  }
  assert(missing_row_rejected);
  assert(maximum_incident_sum(dfg).value() == 14);
  assert(maximum_incident_sum_for_activities(dfg, {"C"}).value() == 14);
  assert(!maximum_incident_sum_for_activities(dfg, {"missing"}));

  assert(start_activity_total(dfg) == 8);
  assert(end_activity_total(dfg) == 1);
  assert(activity_total(dfg, DfgActivityList{"A"}) == 3);
  assert(activity_total(dfg, DfgActivityList{"A"}, false) == 7);
  assert(activity_total(dfg, DfgActivitySet{"A", "D"}) == 10);
}

void projection_and_reciprocal_contract() {
  const DfgFrequency dfg = graph_fixture();
  const DfgFrequency projected =
      project_dfg(dfg, DfgActivitySet{"A", "B", "C"});
  assert(projected.size() == 4);
  assert(projected.at({"A", "B"}) == 5);
  assert(projected.at({"C", "C"}) == 4);
  assert(!projected.count({"D", "C"}));

  const DfgFrequency negated = remove_reciprocal_edges(dfg);
  assert(!negated.count({"A", "B"}));
  assert(!negated.count({"B", "A"}));
  assert(!negated.count({"C", "C"}));
  assert(negated.at({"B", "C"}) == 3);
  assert(negated.at({"X", "Y"}) == 1);
}

void direction_and_self_loop_contract() {
  const DfgFrequency dfg = graph_fixture();
  const auto scores = activity_direction_scores(dfg);
  assert(scores.size() == 6);
  assert(std::abs(scores.at("D") - (7.0 / 8.0)) < 1e-12);
  assert(std::abs(scores.at("Y") + 0.5) < 1e-12);
  const auto selected = activity_direction_scores(dfg, {"A", "D"});
  assert(selected.size() == 2);
  assert(selected.count("C") == 0);
  assert(activity_direction_scores(dfg, {}).empty());

  const auto ranking = rank_activity_directions({{"alpha", 0.5},
                                                  {"beta", 0.5},
                                                  {"zero", 0.0}});
  assert(ranking.size() == 3);
  assert(ranking[0].first == "beta");
  assert(ranking[1].first == "alpha");
  assert(ranking[2].first == "zero");
  assert((self_loop_activities(dfg) == DfgActivityList{"C"}));
}

void component_and_assignment_contract() {
  const DfgFrequency dfg = graph_fixture();
  const auto components = undirected_connected_components(
      incoming_adjacency(dfg), outgoing_adjacency(dfg), {"I"});
  assert(components.size() == 3);
  assert(components[0] == DfgActivitySet({"A", "B", "C", "D"}));
  assert(components[1] == DfgActivitySet({"I"}));
  assert(components[2] == DfgActivitySet({"X", "Y"}));

  const auto no_isolates = undirected_connected_components(
      incoming_adjacency(dfg), outgoing_adjacency(dfg), {"I"}, false);
  assert(no_isolates.size() == 2);
  assert(no_isolates[0] == DfgActivitySet({"A", "B", "C", "D"}));
  assert(no_isolates[1] == DfgActivitySet({"X", "Y"}));

  const auto empty_fallback =
      undirected_connected_components({}, {}, {"I", "J"}, false);
  assert(empty_fallback.size() == 2);
  assert(empty_fallback[0] == DfgActivitySet({"I"}));
  assert(empty_fallback[1] == DfgActivitySet({"J"}));

  const DfgFrequency weighted = {{{"A", "E"}, 8}, {{"E", "C"}, 7}};
  const std::vector<DfgActivitySet> choices{{"A", "B"}, {"C", "D"}};
  const auto assigned = assign_to_strongest_component(
      choices, "E", incoming_adjacency(weighted), outgoing_adjacency(weighted));
  assert(assigned[0].count("E") == 1);
  assert(assigned[1].count("E") == 0);

  const DfgFrequency tied = {{{"A", "E"}, 2}, {{"C", "E"}, 2}};
  const auto tie_assigned = assign_to_strongest_component(
      choices, "E", incoming_adjacency(tied), outgoing_adjacency(tied));
  assert(tie_assigned[0].count("E") == 1);
  assert(tie_assigned[1].count("E") == 0);
  bool empty_components_rejected = false;
  try {
    (void)assign_to_strongest_component(
        {}, "E", incoming_adjacency(tied), outgoing_adjacency(tied));
  } catch (const std::invalid_argument&) {
    empty_components_rejected = true;
  }
  assert(empty_components_rejected);
}

void neighbours_and_matrix_contract() {
  const DfgFrequency dfg = graph_fixture();
  assert((direct_successors(dfg, "B") == DfgActivitySet{"A", "C"}));
  assert((direct_predecessors(dfg, "C") ==
          DfgActivitySet{"B", "C", "D"}));
  assert(direct_successors(dfg, "missing").empty());

  const auto matrix = make_dfg_matrix(dfg);
  assert(matrix.size() == 6);
  assert(matrix.activity_to_index.at("A") == 0);
  assert(matrix.index_to_activity.at(5) == "Y");
  assert(matrix[0][1] == 5.0);
  assert(matrix[2][2] == 4.0);
  assert(matrix[3][2] == 7.0);
  assert(matrix[0][0] == 0.0);
}

void variant_summary_contract() {
  const std::vector<DfgActivityList> variants{{"A", "B", "C"},
                                               {"A", "B", "C"},
                                               {"C", "D"}, {}};
  const auto summary = summarize_variants(variants);
  assert((summary.activities == DfgActivityList{"A", "B", "C", "D"}));
  assert(summary.graph.size() == 3);
  assert(summary.graph.at({"A", "B"}) == 1);
  assert(summary.start_activities.at("A") == 1);
  assert(summary.start_activities.at("C") == 1);
  assert(summary.end_activities.at("C") == 1);
  assert(summary.end_activities.at("D") == 1);

  const std::vector<TraceVariant> weighted{{{"A", "B"}, 3},
                                            {{"B", "C"}, 2}};
  const auto weighted_summary = summarize_variant_counts(weighted);
  assert(weighted_summary.graph.at({"A", "B"}) == 3);
  assert(weighted_summary.graph.at({"B", "C"}) == 2);
  assert(weighted_summary.start_activities.at("A") == 3);
  assert(weighted_summary.end_activities.at("C") == 2);
}

void directed_graph_contract() {
  const DfgFrequency dfg = {{{"A", "B"}, 1}};
  const auto graph = make_directed_graph(dfg, {"A", "B", "isolated"});
  assert(graph.activities().size() == 3);
  assert(graph.has_activity("isolated"));
  assert(graph.has_edge("A", "B"));
  assert(!graph.has_edge("B", "A"));
  assert((graph.successors("A") == DfgActivitySet{"B"}));
  assert((graph.predecessors("B") == DfgActivitySet{"A"}));
  assert(graph.successors("isolated").empty());
  assert(graph.predecessors("missing").empty());
  assert(graph.edges().size() == 1);
}

void transitive_relation_contract() {
  const DfgFrequency dfg = cycle_fixture();
  const DfgActivitySet nodes{"A", "B", "C", "isolated"};
  const auto successors = transitive_successors(dfg, nodes);
  const auto predecessors = transitive_predecessors(dfg, nodes);
  assert((successors.at("A") == DfgActivitySet{"A", "B", "C"}));
  assert((successors.at("B") == DfgActivitySet{"A", "B", "C"}));
  assert(successors.at("C").empty());
  assert(successors.at("isolated").empty());
  assert((predecessors.at("A") == DfgActivitySet{"A", "B"}));
  assert((predecessors.at("B") == DfgActivitySet{"A", "B"}));
  assert((predecessors.at("C") == DfgActivitySet{"A", "B"}));

  const auto relations = transitive_relations(dfg, nodes);
  assert(relations.successors == successors);
  assert(relations.predecessors == predecessors);
  const auto inferred = transitive_successors(dfg);
  assert(inferred.at("A").count("A") == 1);
  assert(transitive_successors(dfg, {}).empty());
  assert(transitive_predecessors(dfg, {}).empty());
  const auto empty_relations = transitive_relations(dfg, {});
  assert(empty_relations.successors.empty());
  assert(empty_relations.predecessors.empty());
}

}  // namespace

int main() {
  adjacency_and_boundary_contract();
  activity_and_incident_count_contract();
  projection_and_reciprocal_contract();
  direction_and_self_loop_contract();
  component_and_assignment_contract();
  neighbours_and_matrix_contract();
  variant_summary_contract();
  directed_graph_contract();
  transitive_relation_contract();
  return 0;
}
