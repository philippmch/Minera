#include "minera/oc_causal_net.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <string>
#include <stdexcept>
#include <vector>

namespace {

minera::OcCausalMarker marker(const std::string& activity,
                              const std::string& type,
                              std::size_t min_count = 1,
                              std::size_t max_count = 1,
                              std::int64_t key = 0) {
  return {activity, type, min_count, max_count, key};
}

minera::OcCausalMarkerGroup group(
    std::initializer_list<minera::OcCausalMarker> markers) {
  return {std::vector<minera::OcCausalMarker>(markers), 1};
}

minera::OcCausalNet chain_model() {
  minera::OcCausalNet model;
  model.object_types = {"order"};
  model.activities = {
      {"Create", {group({marker("START_order", "order", 1, 1, 1)})},
       {group({marker("Ship", "order", 1, 1, 1)})}},
      {"Ship", {group({marker("Create", "order", 1, 1, 1)})},
       {group({marker("END_order", "order", 1, 1, 1)})}},
  };
  return model;
}

minera::OcpnPlayoutOptions one_order_options() {
  minera::OcpnPlayoutOptions options;
  options.objects["order"] = {"o1"};
  options.max_traces = 8;
  options.max_states = 1000;
  options.return_sequences = true;
  return options;
}

bool same_sequences(const std::vector<std::vector<minera::OcpnBinding>>& left,
                    const std::vector<std::vector<minera::OcpnBinding>>& right) {
  if (left.size() != right.size()) return false;
  for (std::size_t trace = 0; trace < left.size(); ++trace) {
    if (left[trace].size() != right[trace].size()) return false;
    for (std::size_t binding = 0; binding < left[trace].size(); ++binding) {
      if (left[trace][binding].activity != right[trace][binding].activity ||
          left[trace][binding].objects != right[trace][binding].objects)
        return false;
    }
  }
  return true;
}

void test_chain_and_serialization() {
  const auto model = chain_model();
  auto options = one_order_options();
  const auto result = minera::oc_causal_net_extensive_playout_result(model, options);
  assert(result.trace_exists);
  assert(result.sequences.size() == 1);
  assert(result.sequences.front().size() == 2);
  assert(result.sequences.front()[0].activity == "Create");
  assert(result.sequences.front()[1].activity == "Ship");
  assert(result.log.events.size() == 2);
  assert(result.log.relations.size() == 2);
  assert(result.log.events.front().timestamp.find('T') != std::string::npos);

  auto traced_options = one_order_options();
  traced_options.return_sequences = false;
  traced_options.return_traces = true;
  const auto traced = minera::oc_causal_net_extensive_playout_result(
      model, traced_options);
  assert(traced.return_traces_requested);
  assert(traced.traces.size() == 1);
  assert(traced.traces.front().size() == 2);
  assert(traced.traces.front()[0].activity == "Create");
  assert(traced.traces.front()[0].objects.size() == 1);
  assert(traced.traces.front()[0].transitions.size() == 1);
  assert(traced.traces.front()[0].transitions[0].object_id == "o1");
  assert(traced.traces.front()[0].transitions[0].transition_index == 0);
  assert(traced.traces.front()[1].transitions[0].transition_index == 1);
  const auto traced_json = minera::serialize_oc_causal_net_playout_json(
      traced, minera::OcpnEventBinding::kIndependent);
  assert(traced_json.find("\"traces\"") != std::string::npos);
  assert(traced_json.find("\"transition_index\":1") != std::string::npos);

  auto projected = one_order_options();
  projected.event_binding = minera::OcpnEventBinding::kSharedActivity;
  projected.objects_unique_per_trace = true;
  const auto projected_result =
      minera::oc_causal_net_extensive_playout_result(model, projected);
  assert(projected_result.trace_exists);
  assert(projected_result.log.objects.size() == 1);
  assert(projected_result.log.objects.front().id == "o1_1");

  auto exists = one_order_options();
  exists.exists_trace = true;
  exists.return_sequences = false;
  const auto exists_result =
      minera::oc_causal_net_extensive_playout_result(model, exists);
  assert(exists_result.trace_exists);
  assert(exists_result.log.events.empty());
  assert(exists_result.sequences.empty());

  minera::OcCausalNet explicit_boundaries;
  explicit_boundaries.object_types = {"item"};
  explicit_boundaries.activities = {
      {"START_item", {}, {group({marker("Work", "item")})}},
      {"Work", {group({marker("START_item", "item")})},
       {group({marker("END_item", "item")})}},
      {"END_item", {group({marker("Work", "item")})}, {}},
  };
  auto explicit_options = one_order_options();
  explicit_options.objects.clear();
  explicit_options.objects["item"] = {"i1"};
  explicit_options.max_bindings_per_activity = 1;
  const auto explicit_result = minera::oc_causal_net_extensive_playout_result(
      explicit_boundaries, explicit_options);
  assert(explicit_result.trace_exists);
  assert(explicit_result.log.events.size() == 1);
  assert(explicit_result.log.events.front().activity == "Work");

  // Boundary starts are consumable per object even when the activity firing
  // cap is left at its default.  The same object must not produce duplicate
  // downstream events just because START_item remains syntactically enabled.
  auto explicit_default_options = one_order_options();
  explicit_default_options.objects.clear();
  explicit_default_options.objects["item"] = {"i1"};
  const auto explicit_default_result = minera::oc_causal_net_extensive_playout_result(
      explicit_boundaries, explicit_default_options);
  assert(explicit_default_result.trace_exists);
  assert(explicit_default_result.log.events.size() == 1);
  assert(explicit_default_result.sequences.size() == 1);
  assert(explicit_default_result.sequences.front().size() == 1);

  // A boundary activity only consumes objects of the type named by its
  // START_<type> suffix.  Both typed boundaries can coexist in one trace.
  minera::OcCausalNet typed_boundaries;
  typed_boundaries.object_types = {"item", "order"};
  typed_boundaries.activities = {
      {"START_item", {}, {group({marker("ItemWork", "item")})}},
      {"ItemWork", {group({marker("START_item", "item")})},
       {group({marker("END_item", "item")})}},
      {"END_item", {group({marker("ItemWork", "item")})}, {}},
      {"START_order", {}, {group({marker("OrderWork", "order")})}},
      {"OrderWork", {group({marker("START_order", "order")})},
       {group({marker("END_order", "order")})}},
      {"END_order", {group({marker("OrderWork", "order")})}, {}},
  };
  auto typed_options = one_order_options();
  typed_options.objects.clear();
  typed_options.objects["item"] = {"i1"};
  typed_options.objects["order"] = {"o1"};
  typed_options.max_traces = 1;
  const auto typed_result = minera::oc_causal_net_extensive_playout_result(
      typed_boundaries, typed_options);
  assert(typed_result.trace_exists);
  assert(typed_result.sequences.size() == 1);
  assert(typed_result.sequences.front().size() == 2);
  assert(typed_result.sequences.front()[0].activity == "ItemWork");
  assert(typed_result.sequences.front()[0].objects.front().first == "item");
  assert(typed_result.sequences.front()[1].activity == "OrderWork");
  assert(typed_result.sequences.front()[1].objects.front().first == "order");

  const auto json = minera::serialize_oc_causal_net_json(model);
  assert(json.find("\"variant\":\"MARKER_GROUPS\"") != std::string::npos);
  assert(json.find("\"input_marker_groups\"") != std::string::npos);
  assert(json.find("START_order") != std::string::npos);
}

void test_and_and_alternative_groups() {
  minera::OcCausalNet and_model;
  and_model.object_types = {"item", "order"};
  and_model.activities = {
      {"Close",
       {group({marker("START_item", "item", 1, 1, 1),
               marker("START_order", "order", 1, 1, 2)})},
       {group({marker("END_item", "item", 1, 1, 1),
               marker("END_order", "order", 1, 1, 2)})}},
  };
  auto and_options = one_order_options();
  and_options.objects.clear();
  and_options.objects["item"] = {"i1"};
  and_options.objects["order"] = {"o1"};
  and_options.event_binding = minera::OcpnEventBinding::kSynchronized;
  const auto and_result = minera::oc_causal_net_extensive_playout_result(
      and_model, and_options);
  assert(and_result.trace_exists);
  assert(and_result.sequences.size() == 1);
  assert(and_result.sequences.front().front().objects.size() == 2);
  auto independent_options = and_options;
  independent_options.event_binding = minera::OcpnEventBinding::kIndependent;
  const auto independent =
      minera::oc_causal_net_extensive_playout_result(and_model, independent_options);
  assert(independent.log.events.size() == 2);
  auto shared_options = and_options;
  shared_options.event_binding = minera::OcpnEventBinding::kSharedActivity;
  const auto shared =
      minera::oc_causal_net_extensive_playout_result(and_model, shared_options);
  assert(shared.log.events.size() == 1);
  auto synchronized_options = and_options;
  synchronized_options.event_binding = minera::OcpnEventBinding::kSynchronized;
  const auto synchronized = minera::oc_causal_net_extensive_playout_result(
      and_model, synchronized_options);
  assert(synchronized.log.events.size() == 1);

  minera::OcCausalNet alternative;
  alternative.object_types = {"item", "order"};
  alternative.activities = {
      {"Choose",
       {group({marker("START_item", "item", 1, 1, 1)}),
        group({marker("START_order", "order", 1, 1, 2)})},
       {group({marker("END_item", "item", 1, 1, 1)}),
        group({marker("END_order", "order", 1, 1, 2)})}},
  };
  const auto alternative_result = minera::oc_causal_net_extensive_playout_result(
      alternative, and_options);
  assert(alternative_result.trace_exists);
  assert(alternative_result.sequences.size() == 2);

  auto branch_trace_options = and_options;
  branch_trace_options.event_binding = minera::OcpnEventBinding::kIndependent;
  branch_trace_options.return_sequences = false;
  branch_trace_options.return_traces = true;
  branch_trace_options.max_traces = 2;
  const auto branch_traces = minera::oc_causal_net_extensive_playout_result(
      alternative, branch_trace_options);
  assert(branch_traces.trace_exists);
  assert(branch_traces.traces.size() == 2);
  assert(branch_traces.traces[0].size() == 2);
  assert(branch_traces.traces[1].size() == 2);
  assert(branch_traces.traces[0][0].activity == "Choose");
  assert(branch_traces.traces[1][0].activity == "Choose");
  assert(branch_traces.traces[0][0].transitions.size() == 1);
  assert(branch_traces.traces[1][0].transitions.size() == 1);
  assert(branch_traces.traces[0][0].transitions[0].transition_index == 0);
  assert(branch_traces.traces[1][0].transitions[0].transition_index == 0);
  assert(branch_traces.traces[0][1].transitions.size() == 1);
  assert(branch_traces.traces[1][1].transitions.size() == 1);
  assert(branch_traces.traces[0][1].transitions[0].transition_index == 0);
  assert(branch_traces.traces[1][1].transitions[0].transition_index == 0);
  assert(branch_traces.traces[0][0].objects !=
         branch_traces.traces[1][0].objects);
}

void test_cardinality_and_key_constraints() {
  minera::OcCausalNet batch;
  batch.object_types = {"item"};
  batch.activities = {
      {"Batch", {group({marker("START_item", "item", 2, 2, 1)})},
       {group({marker("END_item", "item", 2, 2, 1)})}},
  };
  auto options = one_order_options();
  options.objects.clear();
  options.objects["item"] = {"i1", "i2"};
  options.event_binding = minera::OcpnEventBinding::kSynchronized;
  batch.activities[0].output_marker_groups[0].markers[0].min_cardinality = 1;
  batch.activities[0].output_marker_groups[0].markers[0].max_cardinality = 1;
  const auto incomplete = minera::oc_causal_net_extensive_playout_result(
      batch, options);
  assert(!incomplete.trace_exists);
  batch.activities[0].output_marker_groups[0].markers[0].min_cardinality = 2;
  batch.activities[0].output_marker_groups[0].markers[0].max_cardinality = 2;
  const auto batch_result = minera::oc_causal_net_extensive_playout_result(
      batch, options);
  assert(batch_result.trace_exists);
  assert(batch_result.sequences.size() == 1);
  assert(batch_result.sequences.front().front().objects.size() == 2);

  minera::OcCausalNet key_conflict;
  key_conflict.object_types = {"item"};
  key_conflict.activities = {
      {"Seed", {group({marker("START_item", "item", 1, 1, 1)})},
       {group({marker("A", "item", 1, 1, 2),
               marker("B", "item", 1, 1, 3)})}},
      {"A", {group({marker("Seed", "item", 1, 1, 4)})},
       {group({marker("Join", "item", 1, 1, 5)})}},
      {"B", {group({marker("Seed", "item", 1, 1, 6)})},
       {group({marker("Join", "item", 1, 1, 7)})}},
      {"Join", {group({marker("A", "item", 1, 1, 7),
                       marker("B", "item", 1, 1, 7)})},
       {group({marker("END_item", "item", 1, 1, 7)})}},
  };
  auto key_options = one_order_options();
  key_options.objects.clear();
  key_options.objects["item"] = {"i1"};
  const auto rejected = minera::oc_causal_net_extensive_playout_result(
      key_conflict, key_options);
  assert(!rejected.trace_exists);
  key_conflict.activities[3].input_marker_groups[0].markers[1].marker_key = 8;
  const auto accepted = minera::oc_causal_net_extensive_playout_result(
      key_conflict, key_options);
  assert(accepted.trace_exists);
  assert(accepted.sequences.front().size() == 4);
  key_conflict.activities[3].input_marker_groups[0].markers[0].marker_key = 0;
  key_conflict.activities[3].input_marker_groups[0].markers[1].marker_key = 0;
  const auto zero_key = minera::oc_causal_net_extensive_playout_result(
      key_conflict, key_options);
  assert(zero_key.trace_exists);
}

void test_empty_and_deadlocked_states() {
  minera::OcCausalNet empty;
  empty.object_types = {"item"};
  empty.activities = {
      {"Done", {group({marker("START_item", "item")})},
       {group({marker("END_item", "item")})}},
  };
  auto empty_options = one_order_options();
  empty_options.objects.clear();
  empty_options.objects["item"] = {};
  const auto empty_result = minera::oc_causal_net_extensive_playout_result(
      empty, empty_options);
  assert(empty_result.trace_exists);
  assert(empty_result.sequences.size() == 1);
  assert(empty_result.sequences.front().empty());

  minera::OcCausalNet deadlocked;
  deadlocked.object_types = {"item"};
  deadlocked.activities = {
      {"Seed", {group({marker("START_item", "item")})},
       {group({marker("Blocked", "item")})}},
      {"Blocked", {group({marker("Other", "item")})}, {}},
      {"Other", {group({marker("Never", "item")})}, {}},
      {"Never", {group({marker("Never", "item")})}, {}}};
  auto deadlocked_options = one_order_options();
  deadlocked_options.objects.clear();
  deadlocked_options.objects["item"] = {"i1"};
  const auto deadlocked_result = minera::oc_causal_net_extensive_playout_result(
      deadlocked, deadlocked_options);
  assert(!deadlocked_result.trace_exists);

  minera::OcCausalNet reachability;
  reachability.object_types = {"item"};
  reachability.activities = {
      {"Start", {group({marker("START_item", "item")})},
       {group({marker("Blocked", "item")}), group({marker("Done", "item")})}},
      {"Blocked", {group({marker("Dead", "item")})}, {}},
      {"Dead", {group({marker("Dead", "item")})}, {}},
      {"Done", {group({marker("Start", "item")})},
       {group({marker("END_item", "item")})}},
  };
  auto reachability_options = deadlocked_options;
  reachability_options.exists_trace = true;
  reachability_options.return_sequences = false;
  const auto reachability_result = minera::oc_causal_net_extensive_playout_result(
      reachability, reachability_options);
  assert(reachability_result.trace_exists);
}

void test_activity_branching_cap() {
  minera::OcCausalNet alternatives;
  alternatives.object_types = {"item"};
  alternatives.activities = {
      {"A", {group({marker("START_item", "item")})}, {}},
      {"B", {group({marker("START_item", "item")})}, {}},
  };
  auto unlimited = one_order_options();
  unlimited.objects.clear();
  unlimited.objects["item"] = {"i1"};
  unlimited.max_traces = 8;
  const auto all = minera::oc_causal_net_extensive_playout_result(
      alternatives, unlimited);
  assert(all.trace_exists);
  assert(all.sequences.size() == 2);

  auto capped = unlimited;
  capped.branching_factor_activities = 1.0;
  capped.seed = 17;
  const auto first = minera::oc_causal_net_extensive_playout_result(
      alternatives, capped);
  const auto repeat = minera::oc_causal_net_extensive_playout_result(
      alternatives, capped);
  assert(first.trace_exists);
  assert(first.sequences.size() == 1);
  assert(first.sequences.size() == repeat.sequences.size());
  assert(first.sequences.front().size() == repeat.sequences.front().size());
  for (std::size_t index = 0; index < first.sequences.front().size(); ++index) {
    assert(first.sequences.front()[index].activity ==
           repeat.sequences.front()[index].activity);
    assert(first.sequences.front()[index].objects ==
           repeat.sequences.front()[index].objects);
  }
  assert(first.sequences.front().size() == 2);

  capped.branching_factor_activities = 1.5;
  const auto fractional_first = minera::oc_causal_net_extensive_playout_result(
      alternatives, capped);
  const auto fractional_repeat = minera::oc_causal_net_extensive_playout_result(
      alternatives, capped);
  assert(same_sequences(fractional_first.sequences, fractional_repeat.sequences));
  assert(fractional_first.sequences.size() >= 1);
  assert(fractional_first.sequences.size() <= 2);

  capped.branching_factor_activities = 0.0;
  const auto restored = minera::oc_causal_net_extensive_playout_result(
      alternatives, capped);
  assert(restored.sequences.size() == 2);

  for (const double invalid : {-1.0, std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity()}) {
    capped.branching_factor_activities = invalid;
    bool rejected = false;
    try {
      (void)minera::oc_causal_net_extensive_playout_result(alternatives, capped);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }
}

void test_discovery_and_fallback() {
  minera::Ocel log;
  log.events = {
      {"e1", "Create", "2026-01-01T02:00:00+02:00", {}},
      {"e2", "Ship", "2026-01-01T01:00:01Z", {}}};
  log.objects = {{"o1", "order", {}}};
  log.relations = {{"e1", "o1", ""}, {"e2", "o1", ""}};
  const auto model = minera::discover_oc_causal_net(log);
  assert(model.workflow_projection.object_types.size() == 1);
  assert(model.activities.size() == 2);
  auto options = one_order_options();
  const auto result = minera::oc_causal_net_extensive_playout_result(model, options);
  assert(result.trace_exists);
  assert(result.log.events.size() == 2);

  minera::OcCausalNet projection_only;
  projection_only.workflow_projection = model.workflow_projection;
  options.return_sequences = false;
  const auto fallback = minera::oc_causal_net_extensive_playout_result(
      projection_only, options);
  assert(fallback.trace_exists);
}

}  // namespace

int main() {
  test_chain_and_serialization();
  test_and_and_alternative_groups();
  test_cardinality_and_key_constraints();
  test_empty_and_deadlocked_states();
  test_activity_branching_cap();
  test_discovery_and_fallback();
  std::cout << "OC causal-net tests passed\n";
}
