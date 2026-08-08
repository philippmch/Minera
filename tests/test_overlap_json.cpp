// Keep assertions live in Release builds.
#undef NDEBUG

#include "minera/json_export.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace minera;

namespace {

Event event(std::string case_id, std::string activity, std::string timestamp) {
  Event result;
  result.case_id = std::move(case_id);
  result.activity = std::move(activity);
  result.timestamp = std::move(timestamp);
  return result;
}

EventLog sample_log() {
  Trace first;
  first.case_id = "c1";
  first.events = {event("c1", "A", "2024-01-01T00:00:00Z"),
                  event("c1", "B", "2024-01-01T00:01:00Z")};
  Trace second;
  second.case_id = "c2";
  second.events = {event("c2", "C", "2024-01-01T00:02:00Z")};
  return EventLog{{std::move(first), std::move(second)}};
}

void emits_overlap_contract() {
  const auto log = sample_log();
  const auto graph = discover_directly_follows_graph(log);
  const IntervalEventOverlapCounts interval_events{2, 3, 1};
  const CaseOverlapCounts cases{2, 1};
  const ConcurrentActivityOverlapResult concurrent{
      {{"a", "z"}, 1}, {{"A\"", "B\\"}, 4}};

  const auto json = analysis_json(
      "overlap", log, graph,
      /*tree*/ nullptr, /*net*/ nullptr, /*heuristics*/ nullptr,
      /*conformance*/ nullptr, /*performance*/ nullptr, /*resources*/ nullptr,
      /*variant_durations*/ nullptr, /*eventually_follows*/ nullptr,
      /*decisions*/ nullptr, /*petri_decoration*/ nullptr, /*spectrum*/ nullptr,
      /*inductive_net*/ nullptr, /*inductive_decoration*/ nullptr,
      /*heuristic_petri*/ nullptr, /*heuristic_decoration*/ nullptr,
      /*temporal_profile*/ nullptr, /*minimum_self_distances*/ nullptr,
      /*transition_system*/ nullptr, /*powl*/ nullptr, /*powl_petri*/ nullptr,
      /*powl_decoration*/ nullptr, /*log_skeleton*/ nullptr,
      /*log_skeleton_conformance*/ nullptr, /*declare*/ nullptr,
      /*declare_conformance*/ nullptr, /*footprints*/ nullptr,
      /*dfg_variants*/ nullptr, /*correlation_mining*/ nullptr,
      /*batch_discovery*/ nullptr, /*dfg_alignment*/ nullptr,
      /*edit_distance*/ nullptr, /*edit_anti*/ false,
      /*dfg_projection*/ nullptr, /*dfg_projected_log*/ nullptr,
      /*edit_projected_log*/ nullptr, /*process_tree_alignment*/ nullptr,
      /*decomposed_alignment*/ nullptr, /*petri_anti_alignment*/ nullptr,
      /*petri_multi_alignment*/ nullptr, /*reduced_process_tree*/ nullptr,
      /*reduced_trace_index*/ 0, /*evaluation*/ nullptr,
      /*earth_movers_distance*/ nullptr,
      /*interval_event_overlap_counts*/ &interval_events,
      /*case_overlap_counts*/ &cases,
      /*concurrent_activity_overlap*/ &concurrent);

  assert(json.find("\"overlap\": {\"interval_events\": [2, 3, 1], \"cases\": [2, 1], \"concurrent_activities\": [{\"first\": \"A\\\"\", \"second\": \"B\\\\\", \"count\": 4}, {\"first\": \"a\", \"second\": \"z\", \"count\": 1}]}") !=
         std::string::npos);
}

void rejects_noncanonical_pair_keys() {
  const auto log = sample_log();
  const auto graph = discover_directly_follows_graph(log);
  const ConcurrentActivityOverlapResult malformed{{{"z", "a"}, 1}};
  bool threw = false;
  try {
    (void)analysis_json(
        "overlap", log, graph,
        /*tree*/ nullptr, /*net*/ nullptr, /*heuristics*/ nullptr,
        /*conformance*/ nullptr, /*performance*/ nullptr, /*resources*/ nullptr,
        /*variant_durations*/ nullptr, /*eventually_follows*/ nullptr,
        /*decisions*/ nullptr, /*petri_decoration*/ nullptr, /*spectrum*/ nullptr,
        /*inductive_net*/ nullptr, /*inductive_decoration*/ nullptr,
        /*heuristic_petri*/ nullptr, /*heuristic_decoration*/ nullptr,
        /*temporal_profile*/ nullptr, /*minimum_self_distances*/ nullptr,
        /*transition_system*/ nullptr, /*powl*/ nullptr, /*powl_petri*/ nullptr,
        /*powl_decoration*/ nullptr, /*log_skeleton*/ nullptr,
        /*log_skeleton_conformance*/ nullptr, /*declare*/ nullptr,
        /*declare_conformance*/ nullptr, /*footprints*/ nullptr,
        /*dfg_variants*/ nullptr, /*correlation_mining*/ nullptr,
        /*batch_discovery*/ nullptr, /*dfg_alignment*/ nullptr,
        /*edit_distance*/ nullptr, /*edit_anti*/ false,
        /*dfg_projection*/ nullptr, /*dfg_projected_log*/ nullptr,
        /*edit_projected_log*/ nullptr, /*process_tree_alignment*/ nullptr,
        /*decomposed_alignment*/ nullptr, /*petri_anti_alignment*/ nullptr,
        /*petri_multi_alignment*/ nullptr, /*reduced_process_tree*/ nullptr,
        /*reduced_trace_index*/ 0, /*evaluation*/ nullptr,
        /*earth_movers_distance*/ nullptr,
        /*interval_event_overlap_counts*/ nullptr,
        /*case_overlap_counts*/ nullptr,
        /*concurrent_activity_overlap*/ &malformed);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void rejects_mismatched_vector_lengths() {
  const auto log = sample_log();
  const auto graph = discover_directly_follows_graph(log);
  const IntervalEventOverlapCounts malformed{1};
  bool threw = false;
  try {
    (void)analysis_json(
        "overlap", log, graph,
        /*tree*/ nullptr, /*net*/ nullptr, /*heuristics*/ nullptr,
        /*conformance*/ nullptr, /*performance*/ nullptr, /*resources*/ nullptr,
        /*variant_durations*/ nullptr, /*eventually_follows*/ nullptr,
        /*decisions*/ nullptr, /*petri_decoration*/ nullptr, /*spectrum*/ nullptr,
        /*inductive_net*/ nullptr, /*inductive_decoration*/ nullptr,
        /*heuristic_petri*/ nullptr, /*heuristic_decoration*/ nullptr,
        /*temporal_profile*/ nullptr, /*minimum_self_distances*/ nullptr,
        /*transition_system*/ nullptr, /*powl*/ nullptr, /*powl_petri*/ nullptr,
        /*powl_decoration*/ nullptr, /*log_skeleton*/ nullptr,
        /*log_skeleton_conformance*/ nullptr, /*declare*/ nullptr,
        /*declare_conformance*/ nullptr, /*footprints*/ nullptr,
        /*dfg_variants*/ nullptr, /*correlation_mining*/ nullptr,
        /*batch_discovery*/ nullptr, /*dfg_alignment*/ nullptr,
        /*edit_distance*/ nullptr, /*edit_anti*/ false,
        /*dfg_projection*/ nullptr, /*dfg_projected_log*/ nullptr,
        /*edit_projected_log*/ nullptr, /*process_tree_alignment*/ nullptr,
        /*decomposed_alignment*/ nullptr, /*petri_anti_alignment*/ nullptr,
        /*petri_multi_alignment*/ nullptr, /*reduced_process_tree*/ nullptr,
        /*reduced_trace_index*/ 0, /*evaluation*/ nullptr,
        /*earth_movers_distance*/ nullptr,
        /*interval_event_overlap_counts*/ &malformed);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

}  // namespace

int main() {
  emits_overlap_contract();
  rejects_noncanonical_pair_keys();
  rejects_mismatched_vector_lengths();
}
