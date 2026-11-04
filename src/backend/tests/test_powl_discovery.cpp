#include "minera/powl_discovery.hpp"

#include "minera/csv_event_log_importer.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

using namespace minera;

namespace {
EventLog log(std::initializer_list<std::initializer_list<const char*>> traces) {
  EventLog out;
  for (const auto& t : traces) {
    Trace trace;
    for (const auto* a : t) trace.events.push_back(Event{"", a});
    out.traces.push_back(std::move(trace));
  }
  return out;
}

void append_trace(EventLog& event_log,
                  std::initializer_list<const char*> activities,
                  std::size_t count) {
  for (std::size_t copy = 0; copy < count; ++copy) {
    Trace trace;
    for (const auto* activity : activities)
      trace.events.push_back(Event{"", activity});
    event_log.traces.push_back(std::move(trace));
  }
}

template <typename F> void throws(F&& f) {
  bool ok = false;
  try { f(); } catch (const std::exception&) { ok = true; }
  assert(ok);
}
}

int main() {
  // Base/empty traces and empty activity labels are valid inputs.
  assert(discover_powl(EventLog{}).kind() == PowlNodeKind::kSilent);
  assert(discover_powl(log({{}})).kind() == PowlNodeKind::kSilent);
  assert(discover_powl(log({{""}})).label().empty());
  const auto optional = discover_powl(log({{}, {"A"}}));
  assert(optional.kind() == PowlNodeKind::kXor);
  assert(optional.children()[0].kind() == PowlNodeKind::kSilent);
  assert(optional.children()[1].label() == "A");

  // A non-canonical activity selector reads the exact event attribute and
  // rejects both missing and explicit-null values deterministically.
  EventLog keyed_log;
  Trace keyed_trace;
  Event keyed_a{"case-1", "canonical-A"};
  keyed_a.attributes.push_back(EventAttribute{"phase", "A"});
  Event keyed_b{"case-1", "canonical-B"};
  keyed_b.attributes.push_back(EventAttribute{"phase", "B"});
  keyed_trace.events = {std::move(keyed_a), std::move(keyed_b)};
  keyed_log.traces.push_back(std::move(keyed_trace));
  PowlDiscoveryOptions keyed_options;
  keyed_options.activity_key = "phase";
  const auto keyed_model = discover_powl(keyed_log, keyed_options);
  assert(keyed_model.serialize().find("T(\"A\")") != std::string::npos);
  assert(keyed_model.serialize().find("T(\"B\")") != std::string::npos);
  assert(keyed_model.serialize().find("canonical-A") == std::string::npos);
  assert(keyed_model.serialize().find("canonical-B") == std::string::npos);
  auto missing_activity = keyed_log;
  missing_activity.traces[0].events[1].attributes.clear();
  bool missing_threw = false;
  try {
    (void)discover_powl(missing_activity, keyed_options);
  } catch (const std::invalid_argument& error) {
    missing_threw = std::string(error.what()).find("phase") != std::string::npos;
  }
  assert(missing_threw);
  auto null_activity = keyed_log;
  null_activity.traces[0].events[1].attributes[0].is_null = true;
  bool null_threw = false;
  try {
    (void)discover_powl(null_activity, keyed_options);
  } catch (const std::invalid_argument& error) {
    null_threw = std::string(error.what()).find("phase") != std::string::npos;
  }
  assert(null_threw);

  // Native logs may arrive as one flattened trace.  Custom selectors must
  // regroup by the selected case field and stably order each case by its
  // selected timestamp while projecting the selected activity label.
  EventLog flattened;
  Trace flattened_trace;
  const auto append_flattened_event = [&](const char* case_key,
                                          const char* phase,
                                          const char* when) {
    Event event{"legacy-case", "canonical"};
    event.attributes.push_back(EventAttribute{"batch", case_key});
    event.attributes.push_back(EventAttribute{"phase", phase});
    event.attributes.push_back(EventAttribute{"when", when});
    flattened_trace.events.push_back(std::move(event));
  };
  append_flattened_event("case-b", "B-late", "2024-01-01T00:00:03Z");
  append_flattened_event("case-a", "A-late", "2024-01-01T00:00:02Z");
  append_flattened_event("case-b", "B-early", "2024-01-01T00:00:00Z");
  append_flattened_event("case-a", "A-early", "2024-01-01T00:00:01Z");
  flattened.traces.push_back(std::move(flattened_trace));
  PowlDiscoveryOptions alternate_selectors = keyed_options;
  alternate_selectors.activity_key = "phase";
  alternate_selectors.timestamp_key = "when";
  alternate_selectors.case_id_key = "batch";
  const auto normalized =
      normalize_powl_event_log(flattened, alternate_selectors);
  assert(normalized.traces.size() == 2);
  assert(normalized.traces[0].case_id == "case-a");
  assert(normalized.traces[1].case_id == "case-b");
  assert(normalized.traces[0].events.size() == 2);
  assert(normalized.traces[1].events.size() == 2);
  assert(normalized.traces[0].events[0].activity == "A-early");
  assert(normalized.traces[0].events[1].activity == "A-late");
  assert(normalized.traces[1].events[0].activity == "B-early");
  assert(normalized.traces[1].events[1].activity == "B-late");
  for (const auto& trace : normalized.traces)
    for (const auto& event : trace.events) {
      assert(event.case_id == trace.case_id);
      assert(event.attribute("phase") != nullptr);
      assert(event.attribute("when") != nullptr);
    }
  const auto canonical_projection = normalize_powl_event_log(flattened);
  assert(canonical_projection.traces.size() == 1);
  assert(canonical_projection.traces[0].events[0].activity == "canonical");
  assert(canonical_projection.traces[0].events[1].activity == "canonical");
  auto missing_case = flattened;
  missing_case.traces[0].events[0].attributes.erase(
      missing_case.traces[0].events[0].attributes.begin());
  bool missing_case_threw = false;
  try {
    (void)normalize_powl_event_log(missing_case, alternate_selectors);
  } catch (const std::invalid_argument& error) {
    missing_case_threw = std::string(error.what()).find("batch") !=
                         std::string::npos;
  }
  assert(missing_case_threw);
  auto invalid_timestamp = flattened;
  invalid_timestamp.traces[0].events[0].attributes[2].value = "not-a-time";
  bool invalid_timestamp_threw = false;
  try {
    (void)normalize_powl_event_log(invalid_timestamp, alternate_selectors);
  } catch (const std::invalid_argument& error) {
    invalid_timestamp_threw = std::string(error.what()).find("when") !=
                              std::string::npos;
  }
  assert(invalid_timestamp_threw);

  // CSV first-class projections retain the exact headers that supplied them.
  // POWL selectors therefore work even though selected columns are not copied
  // into the generic attribute bag.
  CsvImportOptions csv_options;
  csv_options.case_id_column = "order_ref";
  csv_options.activity_column = "step_name";
  csv_options.timestamp_column = "occurred_at";
  csv_options.resource_column = "worker";
  csv_options.lifecycle_column = "phase_state";
  csv_options.require_timestamp_column = true;
  std::istringstream csv(
      "order_ref,step_name,occurred_at,worker,phase_state,amount\n"
      "case-b,B-late,2024-01-01T00:00:03Z,r2,complete,20\n"
      "case-a,A-late,2024-01-01T00:00:02Z,r1,complete,10\n"
      "case-b,B-early,2024-01-01T00:00:00Z,r2,start,20\n"
      "case-a,A-early,2024-01-01T00:00:01Z,r1,start,10\n");
  const auto csv_log = import_csv_event_log(csv, csv_options);
  const auto& csv_event = csv_log.traces.front().events.front();
  assert(csv_event.case_id_source_key == "order_ref");
  assert(csv_event.activity_source_key == "step_name");
  assert(csv_event.timestamp_source_key == "occurred_at");
  assert(csv_event.resource_source_key == "worker");
  assert(csv_event.lifecycle_source_key == "phase_state");
  assert(csv_event.attribute("step_name") == nullptr);
  assert(csv_event.attribute("occurred_at") == nullptr);
  assert(csv_event.attribute("worker") == nullptr);
  assert(csv_event.attribute("phase_state") == nullptr);
  PowlDiscoveryOptions csv_powl_options;
  csv_powl_options.variant = PowlDiscoveryVariant::kTree;
  csv_powl_options.activity_key = "step_name";
  csv_powl_options.timestamp_key = "occurred_at";
  csv_powl_options.case_id_key = "order_ref";
  const auto normalized_csv =
      normalize_powl_event_log(csv_log, csv_powl_options);
  assert(normalized_csv.traces.size() == 2);
  assert(normalized_csv.traces[0].case_id == "case-a");
  assert(normalized_csv.traces[0].events[0].activity == "A-early");
  assert(normalized_csv.traces[1].case_id == "case-b");
  assert(normalized_csv.traces[1].events[0].activity == "B-early");
  const auto csv_powl = discover_powl(csv_log, csv_powl_options).serialize();
  assert(csv_powl.find("A-early") != std::string::npos);
  assert(csv_powl.find("B-early") != std::string::npos);

  PowlDiscoveryOptions tree;
  tree.variant = PowlDiscoveryVariant::kTree;
  const auto seq = discover_powl(log({{"A", "B"}}), tree);
  assert(seq.kind() == PowlNodeKind::kStrictPartialOrder);
  assert(seq.children().size() == 2 && seq.children()[0].label() == "A" && seq.children()[1].label() == "B");
  assert(seq.has_edge(0, 1) && !seq.has_edge(1, 0));

  const auto conc = discover_powl(log({{"A", "B"}, {"B", "A"}}), tree);
  assert(conc.kind() == PowlNodeKind::kStrictPartialOrder && conc.children().size() == 2);
  assert(conc.order().empty());

  const auto choice = discover_powl(log({{"A"}, {"B"}}), tree);
  assert(choice.kind() == PowlNodeKind::kXor && choice.children().size() == 2);
  assert(choice.children()[0].label() == "A" && choice.children()[1].label() == "B");

  const auto once_inside_repeat =
      discover_powl(log({{"A", "B", "A"}}), tree);
  assert(once_inside_repeat.kind() == PowlNodeKind::kLoop);
  assert(once_inside_repeat.children()[0].label() == "A");
  assert(once_inside_repeat.children()[1].label() == "B");

  const auto strict_tau =
      discover_powl(log({{"A", "B", "A", "B"}}), tree);
  assert(strict_tau.kind() == PowlNodeKind::kLoop);
  assert(strict_tau.children()[1].kind() == PowlNodeKind::kSilent);
  assert(strict_tau.children()[0].kind() ==
         PowlNodeKind::kStrictPartialOrder);
  assert(strict_tau.children()[0].children().size() == 2);
  assert(strict_tau.children()[0].children()[0].label() == "A");
  assert(strict_tau.children()[0].children()[1].label() == "B");

  // The exact common-loop cut keeps ordered behavior inside the redo log
  // instead of flattening every middle activity into one XOR.
  const auto ordered_redo =
      discover_powl(log({{"A", "B", "C", "A"}, {"A", "D", "A"}}),
                    tree);
  assert(ordered_redo.kind() == PowlNodeKind::kLoop);
  assert(ordered_redo.children()[0].label() == "A");
  assert(ordered_redo.children()[1].kind() == PowlNodeKind::kXor);
  assert(ordered_redo.children()[1].children().size() == 2);
  assert(ordered_redo.children()[1].children()[0].kind() ==
         PowlNodeKind::kStrictPartialOrder);
  assert(ordered_redo.children()[1].children()[0].has_edge(0, 1));
  assert(ordered_redo.children()[1].children()[0].children()[0].label() ==
         "B");
  assert(ordered_redo.children()[1].children()[0].children()[1].label() ==
         "C");
  assert(ordered_redo.children()[1].children()[1].label() == "D");

  // Different start/end boundary activities still form one do group.
  const auto boundary_union =
      discover_powl(log({{"A", "B", "A"}, {"Z", "B", "Z"}}), tree);
  assert(boundary_union.kind() == PowlNodeKind::kLoop);
  assert(boundary_union.children()[0].kind() == PowlNodeKind::kXor);
  assert(boundary_union.children()[0].children()[0].label() == "A");
  assert(boundary_union.children()[0].children()[1].label() == "Z");
  assert(boundary_union.children()[1].label() == "B");

  // Boundary reachability repair absorbs ordinary prefix/suffix components;
  // this log has no loop cut despite sharing its outer activities.
  const auto repaired_boundaries =
      discover_powl(log({{"A", "B", "C"}, {"A", "D", "C"}}), tree);
  assert(repaired_boundaries.kind() != PowlNodeKind::kLoop);

  // Partial return-to-start evidence is not a complete redo component.
  const auto incomplete_start =
      discover_powl(log({{"A", "X", "B", "A"}, {"B", "A", "C"}}),
                    tree);
  assert(incomplete_start.kind() != PowlNodeKind::kLoop);

  // The dual end-side completeness repair rejects a component that is
  // reached from one possible end but not from every possible end.
  const auto incomplete_end =
      discover_powl(log({{"A", "C"}, {"B", "C", "X", "D"}}), tree);
  assert(incomplete_end.kind() != PowlNodeKind::kLoop);

  // Separate return components must not be merged into a false common loop
  // when each one returns only to its own boundary activity.
  const auto split_returns =
      discover_powl(log({{"A", "X", "A"}, {"B", "Y", "B"}}), tree);
  assert(split_returns.kind() != PowlNodeKind::kLoop);

  // A redo component is valid when it returns to every possible boundary;
  // the common loop cut keeps the boundary alternatives in do.
  const auto common_boundary_redo = discover_powl(
      log({{"A", "X", "A"}, {"B", "X", "B"}, {"A", "X", "B"},
           {"B", "X", "A"}}),
      tree);
  assert(common_boundary_redo.kind() == PowlNodeKind::kLoop);
  assert(common_boundary_redo.children()[0].kind() == PowlNodeKind::kXor);
  assert(common_boundary_redo.children()[1].label() == "X");

  // A shared multi-activity redo segment must remain ordered while all
  // start/end boundary alternatives stay in the do part.
  const auto shared_ordered_redo = discover_powl(
      log({{"A", "X", "Y", "A"}, {"B", "X", "Y", "B"},
           {"A", "X", "Y", "B"}, {"B", "X", "Y", "A"}}),
      tree);
  assert(shared_ordered_redo.kind() == PowlNodeKind::kLoop);
  assert(shared_ordered_redo.children()[0].kind() == PowlNodeKind::kXor);
  assert(shared_ordered_redo.children()[1].kind() ==
         PowlNodeKind::kStrictPartialOrder);
  assert(shared_ordered_redo.children()[1].has_edge(0, 1));

  // Asymmetric boundaries still admit a common redo when the middle
  // component returns to every start and is reachable from every end.
  const auto asymmetric_boundaries = discover_powl(
      log({{"A", "D", "X", "A"}, {"A", "D"}, {"A", "X", "A"}}),
      tree);
  assert(asymmetric_boundaries.kind() == PowlNodeKind::kLoop);
  assert(asymmetric_boundaries.children()[1].label() == "X");

  // Two boundary sets with separate return components are not one common
  // loop: each component is incomplete against the other boundary.
  const auto asymmetric_false_positive = discover_powl(
      log({{"A", "D", "X", "A"}, {"A", "D"}, {"A", "X", "A"},
           {"B", "Y", "B"}}),
      tree);
  assert(asymmetric_false_positive.kind() != PowlNodeKind::kLoop);

  // A connected middle component that returns to only one boundary is not a
  // common loop; accepting it would create a false-positive redo cut.
  const auto partial_shared_return = discover_powl(
      log({{"A", "X", "Y", "A"}, {"B", "X", "B"}}), tree);
  assert(partial_shared_return.kind() != PowlNodeKind::kLoop);

  // Multiple redo slices retain their multiplicity and order in the merged
  // redo projection rather than collapsing to a single activity choice.
  const auto multiple_redo =
      discover_powl(log({{"A", "X", "A", "Y", "A"}}), tree);
  assert(multiple_redo.kind() == PowlNodeKind::kLoop);
  assert(multiple_redo.children()[0].label() == "A");
  assert(multiple_redo.children()[1].kind() == PowlNodeKind::kXor);
  assert(multiple_redo.children()[1].children().size() == 2);

  // Disconnected redo components are merged without losing the common do
  // boundary: both return paths become alternatives inside one loop redo.
  const auto disconnected_redo =
      discover_powl(log({{"A", "X", "A"}, {"A", "Y", "A"}}), tree);
  assert(disconnected_redo.kind() == PowlNodeKind::kLoop);
  assert(disconnected_redo.children()[0].label() == "A");
  assert(disconnected_redo.children()[1].kind() == PowlNodeKind::kXor);
  assert(disconnected_redo.children()[1].children().size() == 2);
  assert(disconnected_redo.children()[1].children()[0].label() == "X");
  assert(disconnected_redo.children()[1].children()[1].label() == "Y");

  // A single closed variant is a normal LOOP(do, redo), not a tau-loop
  // fall-through with a different language.
  const auto single_closed = discover_powl(log({{"A", "B", "A"}}), tree);
  assert(single_closed.kind() == PowlNodeKind::kLoop);
  assert(single_closed.children()[0].label() == "A");
  assert(single_closed.children()[1].label() == "B");

  // ActivityOncePerTrace is reached when a once-only activity moves around a
  // structured remainder, preventing a normal top-level cut.
  const auto once =
      discover_powl(log({{"A", "B", "C"}, {"B", "C", "A"}}), tree);
  assert(once.kind() == PowlNodeKind::kStrictPartialOrder);
  assert(once.order().size() == 1 && once.children().size() == 3);
  assert(once.children()[0].label() == "A");
  assert(once.children()[1].label() == "B");
  assert(once.children()[2].label() == "C");
  assert(once.has_edge(1, 2));

  // ActivityConcurrent follows ActivityOnce: A repeats in every trace, and
  // removing it exposes the XOR(B,C) cut.
  const auto concurrent_fall_through = discover_powl(
      log({{"B", "A", "A"}, {"A", "A", "B"}, {"A", "A", "C"}}),
      tree);
  assert(concurrent_fall_through.kind() ==
         PowlNodeKind::kStrictPartialOrder);
  // Strict sequence repair preserves the end-only C stage instead of
  // flattening it into the concurrency fall-through.
  assert(!concurrent_fall_through.order().empty());
  assert(concurrent_fall_through.children().size() == 2);
  assert(concurrent_fall_through.children()[0].kind() ==
         PowlNodeKind::kStrictPartialOrder);
  assert(concurrent_fall_through.children()[1].kind() ==
         PowlNodeKind::kXor);
  assert(concurrent_fall_through.children()[1].children().size() == 2);
  assert(concurrent_fall_through.children()[1].children()[0].kind() ==
         PowlNodeKind::kSilent);
  assert(concurrent_fall_through.children()[1].children()[1].label() == "C");

  // Brute-force starts at k=n-1, uses EFG (not merely adjacent DFG), and is deterministic.
  PowlDiscoveryOptions brute; brute.variant = PowlDiscoveryVariant::kBruteForce;
  const auto b = discover_powl(log({{"A", "B", "C"}}), brute);
  assert(b.serialize() ==
         "PO=(nodes={T(\"A\"),T(\"B\"),T(\"C\")},"
         "order={A-->B,A-->C,B-->C})");
  const auto b_rev = discover_powl(log({{"A", "B", "C"}}), brute).serialize();
  assert(b.serialize() == b_rev);
  auto limited_brute = brute;
  limited_brute.brute_force_activity_limit = 2;
  throws([&] { discover_powl(log({{"A", "B", "C"}}), limited_brute); });
  const auto brute_partition_order = discover_powl(
      log({{"A", "B", "C"}, {"A", "C", "B"}, {"B", "C", "A"}}),
      brute);
  assert(brute_partition_order.serialize() ==
         "PO=(nodes={T(\"A\"),T(\"B\"),T(\"C\")},order={})");

  // A repeated one-activity projection is not a base case.  Filtering it
  // first must preserve the singleton transition and its optional prefix.
  EventLog repeated_projection;
  append_trace(repeated_projection, {"B", "A"}, 5);
  append_trace(repeated_projection, {"A", "A"}, 3);
  auto repeated_projection_options = brute;
  repeated_projection_options.filtering_threshold = 0.9;
  const auto repeated_projection_result =
      discover_powl(repeated_projection, repeated_projection_options);
  assert(repeated_projection_result.kind() ==
         PowlNodeKind::kStrictPartialOrder);
  assert(repeated_projection_result.children().size() == 2);
  assert(repeated_projection_result.has_edge(0, 1));
  assert(repeated_projection_result.children()[0].kind() ==
         PowlNodeKind::kXor);
  assert(repeated_projection_result.children()[0].children().size() == 2);
  assert(repeated_projection_result.children()[0].children()[0].kind() ==
         PowlNodeKind::kSilent);
  assert(repeated_projection_result.children()[0].children()[1].label() ==
         "B");
  assert(repeated_projection_result.children()[1].kind() ==
         PowlNodeKind::kTransition);
  assert(repeated_projection_result.children()[1].label() == "A");

  // Maximal groups activities with identical one-way EFG neighborhoods.
  PowlDiscoveryOptions maximal; maximal.variant = PowlDiscoveryVariant::kMaximal;
  const auto m = discover_powl(log({{"A", "B"}, {"A", "C"}}), maximal);
  assert(m.kind() == PowlNodeKind::kStrictPartialOrder);
  assert(m.children().size() == 2 && m.has_edge(0, 1));
  const auto maximal_chain = discover_powl(log({{"A", "B", "C"}}), maximal);
  assert(maximal_chain.serialize() == "PO=(nodes={T(\"A\"),T(\"B\"),T(\"C\")},order={A-->B,A-->C,B-->C})");
  const auto maximal_neighborhood_order =
      discover_powl(log({{"A", "B", "C"}, {"C", "A", "B"}}), maximal);
  assert(maximal_neighborhood_order.serialize() ==
         "PO=(nodes={T(\"A\"),T(\"B\"),T(\"C\")},order={A-->B})");

  // Plain and frequency dynamic are distinct source variants; both terminate
  // on a three-activity non-transitive fixture.
  PowlDiscoveryOptions plain; plain.variant = PowlDiscoveryVariant::kDynamicClusteringPlain;
  PowlDiscoveryOptions frequency; frequency.variant = PowlDiscoveryVariant::kDynamicClustering; frequency.order_frequency_ratio = 0.9;
  const auto dp = discover_powl(log({{"A", "B"}, {"B", "C"}, {"A", "C"}}), plain);
  const auto df = discover_powl(log({{"A", "B"}, {"B", "C"}, {"A", "C"}}), frequency);
  assert(!dp.serialize().empty() && !df.serialize().empty());
  assert(dp.serialize() ==
         "PO=(nodes={X(tau,T(\"A\")),X(tau,T(\"B\")),"
         "X(tau,T(\"C\"))},"
         "order={X(tau,T(\"A\"))-->X(tau,T(\"B\")),"
         "X(tau,T(\"A\"))-->X(tau,T(\"C\")),"
         "X(tau,T(\"B\"))-->X(tau,T(\"C\"))})");
  assert(df.serialize() == dp.serialize());

  // Dynamic clustering coarsens unrelated predecessor activities before
  // materializing the common successor edge.
  const auto dynamic_choice =
      discover_powl(log({{"A", "B"}, {"C", "B"}}), plain);
  assert(dynamic_choice.kind() == PowlNodeKind::kStrictPartialOrder);
  assert(dynamic_choice.children().size() == 2);
  assert(dynamic_choice.has_edge(0, 1));
  assert(dynamic_choice.children()[0].kind() == PowlNodeKind::kXor);
  assert(dynamic_choice.children()[0].children().size() == 2);
  assert(dynamic_choice.children()[1].label() == "B");

  // Multi-round choice merges keep the canonical child order after a merged
  // group moves past unrelated groups between rounds.
  const auto merge_order_probe = discover_powl(
      log({{"A", "D"}, {"B", "D"}, {"C", "D"}, {"D", "E"}}), plain);
  assert(merge_order_probe.serialize() ==
         "PO=(nodes={X(tau,T(\"A\"),T(\"B\"),T(\"C\")),T(\"D\"),"
         "X(tau,T(\"E\"))},order={X(tau,T(\"A\"),T(\"B\"),T(\"C\"))-->"
         "D,X(tau,T(\"A\"),T(\"B\"),T(\"C\"))-->"
         "X(tau,T(\"E\")),D-->X(tau,T(\"E\"))})");

  // A reverse-supported transitive cycle must merge the conflicting groups
  // before the remaining order is materialized.
  const auto cycle_probe = discover_powl(
      log({{"A", "B"}, {"B", "C"}, {"C", "A"}, {"A", "D"}}), plain);
  assert(cycle_probe.serialize() ==
         "PO=(nodes={X(tau,T(\"B\"),T(\"D\")),X(tau,T(\"A\")),"
         "X(tau,T(\"C\"))},"
         "order={X(tau,T(\"C\"))-->X(tau,T(\"A\"))})");

  // Equal predecessor/successor neighborhoods form stable XOR groups while
  // preserving the surrounding chain.
  const auto equal_probe = discover_powl(
      log({{"A", "C"}, {"B", "C"}, {"A", "D"}, {"B", "D"},
           {"C", "E"}, {"D", "E"}}), plain);
  assert(equal_probe.serialize() ==
         "PO=(nodes={X(tau,T(\"A\"),T(\"B\")),X(T(\"C\"),T(\"D\")),"
         "X(tau,T(\"E\"))},order={X(tau,T(\"A\"),T(\"B\"))-->"
         "X(T(\"C\"),T(\"D\")),X(tau,T(\"A\"),T(\"B\"))-->"
         "X(tau,T(\"E\")),X(T(\"C\"),T(\"D\"))-->"
         "X(tau,T(\"E\"))})");

  // Reverse choice evidence must not introduce an artificial order between
  // the merged alternatives and their common successor.
  const auto reverse_choice_probe = discover_powl(
      log({{"A", "D"}, {"B", "D"}, {"C", "D"}, {"D", "A"},
           {"D", "B"}}), plain);
  assert(reverse_choice_probe.serialize() ==
         "PO=(nodes={X(T(\"A\"),T(\"B\"),T(\"C\")),T(\"D\")},order={})");

  // A later merge that targets a group already appended by an earlier merge
  // retains activity-argument order rather than vector-position order.
  const auto canonical_multi_merge_probe = discover_powl(
      log({{"A", "B"}, {"A", "C"}, {"A", "C2"}, {"A", "D"},
           {"B", "C2"}, {"C2", "D"}}), plain);
  assert(canonical_multi_merge_probe.serialize() ==
         "PO=(nodes={X(tau,T(\"A\")),X(PO=(nodes={X(tau,T(\"B\")),"
         "X(tau,T(\"C2\")),X(tau,T(\"D\"))},order={X(tau,T(\"B\"))-->"
         "X(tau,T(\"C2\")),X(tau,T(\"B\"))-->X(tau,T(\"D\")),"
         "X(tau,T(\"C2\"))-->X(tau,T(\"D\"))}),T(\"C\"))},"
         "order={X(tau,T(\"A\"))-->X(PO=(nodes={X(tau,T(\"B\")),"
         "X(tau,T(\"C2\")),X(tau,T(\"D\"))},order={X(tau,T(\"B\"))-->"
         "X(tau,T(\"C2\")),X(tau,T(\"B\"))-->X(tau,T(\"D\")),"
         "X(tau,T(\"C2\"))-->X(tau,T(\"D\"))}),T(\"C\"))})");
  // Plain keeps the two-node bidirectional relation as an unordered PO;
  // frequency uses multiplicity-weighted directional support.
  EventLog ratio_log;
  for (int i = 0; i < 8; ++i) { Trace t; t.events = {Event{"", "A"}, Event{"", "B"}}; ratio_log.traces.push_back(std::move(t)); }
  for (int i = 0; i < 2; ++i) { Trace t; t.events = {Event{"", "B"}, Event{"", "A"}}; ratio_log.traces.push_back(std::move(t)); }
  const auto plain_ratio = discover_powl(ratio_log, plain);
  frequency.order_frequency_ratio = 0.75;
  const auto freq_ratio = discover_powl(ratio_log, frequency);
  assert(plain_ratio.serialize() == "PO=(nodes={T(\"A\"),T(\"B\")},order={})");
  assert(freq_ratio.serialize() == "PO=(nodes={T(\"A\"),T(\"B\")},order={A-->B})");

  // Frequency support is counted once per direction and per trace. Repeating
  // A inside one trace must not outweigh an equally frequent reverse trace.
  EventLog repeated_ratio;
  append_trace(repeated_ratio, {"A", "A", "A", "B"}, 1);
  append_trace(repeated_ratio, {"B", "A"}, 1);
  frequency.order_frequency_ratio = 0.75;
  const auto repeated_ratio_result =
      discover_powl(repeated_ratio, frequency);
  assert(repeated_ratio_result.kind() == PowlNodeKind::kStrictPartialOrder);
  assert(repeated_ratio_result.order().empty());
  assert(repeated_ratio_result.children().size() == 2);

  // Bidirectional cross evidence is accepted as an unordered block by the
  // brute partition validator (A is the sole minimal block).
  brute.brute_force_activity_limit = 0;
  const auto unordered = discover_powl(log({{"A", "B", "C"}, {"A", "C", "B"}}), brute);
  assert(unordered.serialize() == "PO=(nodes={T(\"A\"),PO=(nodes={T(\"B\"),T(\"C\")},order={})},order={A-->PO=(nodes={T(\"B\"),T(\"C\")},order={})})");
  assert(unordered.kind() == PowlNodeKind::kStrictPartialOrder);
  assert(unordered.children().size() == 2 && unordered.children()[0].label() == "A");
  assert(unordered.order().size() == 1 && unordered.has_edge(0, 1));
  frequency.order_frequency_ratio = 0.5;
  throws([&] { discover_powl(log({{"A"}}), frequency); });

  // Decreasing-factor filtering is chained through the previously retained
  // frequency: 100,60,35 at 0.5 keeps all three (35 >= 60*0.5).
  EventLog filtering_log;
  append_trace(filtering_log, {"B", "A", "A"}, 100);
  append_trace(filtering_log, {"A", "A", "B"}, 60);
  append_trace(filtering_log, {"A", "A", "C"}, 35);
  PowlDiscoveryOptions filtered = tree;
  filtered.filtering_threshold = 0.5;
  const auto chained = discover_powl(filtering_log, filtered).serialize();
  assert(chained.find("T(\"C\")") != std::string::npos);

  // Dynamic filtering is only reached after the normal strict-sequence cut;
  // this log therefore retains C in the exact sequence projection.
  filtered.filtering_type = PowlFilteringType::kDynamic;
  filtered.filtering_threshold = 0.2;
  const auto dynamically_filtered =
      discover_powl(filtering_log, filtered).serialize();
  assert(dynamically_filtered.find("T(\"C\")") != std::string::npos);
  // Dynamic filtering ignores the decreasing-factor threshold, including
  // its zero default, and removes every least-frequent variant.
  filtered.filtering_threshold = 0.0;
  const auto dynamic_default_threshold =
      discover_powl(filtering_log, filtered).serialize();
  assert(dynamic_default_threshold.find("T(\"C\")") != std::string::npos);

  // Decreasing-factor filtering is exercised on a cyclic no-cut fixture, so
  // filtering is reached before any structured fall-through can retain the
  // low-frequency activities.  Ranking is frequency-only and stable: the
  // equal-frequency variants remain in input order, the boundary value is
  // rejected by strict `>`, and scanning stops at that first rejection.
  EventLog equality_log;
  append_trace(equality_log, {"A", "B", "C"}, 100);
  append_trace(equality_log, {"B", "C", "X", "A"}, 50);
  append_trace(equality_log, {"C", "A", "Y", "B"}, 50);
  auto strict_filter = tree;
  strict_filter.filtering_threshold = 0.5;
  const auto strict_boundary = discover_powl(equality_log, strict_filter).serialize();
  assert(strict_boundary.find("T(\"X\")") == std::string::npos);
  assert(strict_boundary.find("T(\"Y\")") == std::string::npos);
  // When ranking changes insertion order but retains every variant, filtering
  // must recognize semantic equality and continue to fall-through discovery
  // instead of recursively rediscovering the same multiset.
  EventLog all_retained_reordered;
  append_trace(all_retained_reordered, {"B", "C", "X", "A"}, 50);
  append_trace(all_retained_reordered, {"A", "B", "C"}, 100);
  append_trace(all_retained_reordered, {"C", "A", "Y", "B"}, 50);
  auto retain_all = tree;
  retain_all.filtering_threshold = 0.1;
  const auto retained =
      discover_powl(all_retained_reordered, retain_all).serialize();
  assert(retained.find("T(\"X\")") != std::string::npos);
  assert(retained.find("T(\"Y\")") != std::string::npos);
  throws([&] { auto o = filtered; o.filtering_threshold = 1.0; discover_powl(log({{"A"}}), o); });
  throws([&] { auto o = filtered; o.filtering_threshold = -0.1; discover_powl(log({{"A"}}), o); });
  throws([&] { auto o = filtered; o.filtering_type = static_cast<PowlFilteringType>(99); discover_powl(log({{"A"}}), o); });
  throws([&] { auto o = filtered; o.variant = static_cast<PowlDiscoveryVariant>(99); discover_powl(log({{"A"}}), o); });
  throws([&] { auto o = filtered; o.order_frequency_ratio = std::numeric_limits<double>::quiet_NaN(); discover_powl(log({{"A"}}), o); });
  throws([&] { auto o = filtered; o.activity_key = " "; discover_powl(log({{"A"}}), o); });
  throws([&] { auto o = filtered; o.timestamp_key = "\t"; discover_powl(log({{"A"}}), o); });
  throws([&] { auto o = filtered; o.case_id_key = ""; discover_powl(log({{"A"}}), o); });

  const auto ordered_a = discover_powl(log({{"B", "A"}, {"A", "B"}}), tree).serialize();
  const auto ordered_b = discover_powl(log({{"A", "B"}, {"B", "A"}}), tree).serialize();
  assert(ordered_a == ordered_b);

  // Boundary alternatives and repeated redo slices still form one common
  // loop when the middle component returns to every boundary activity.
  const auto repeated_common_loop = discover_powl(
      log({{"A", "X", "A"}, {"B", "X", "B"}, {"A", "X", "B"},
           {"B", "X", "A"}, {"A", "X", "A", "X", "A"}}),
      tree);
  assert(repeated_common_loop.kind() == PowlNodeKind::kLoop);
  assert(repeated_common_loop.children()[0].kind() == PowlNodeKind::kXor);
  assert(repeated_common_loop.children()[1].label() == "X");

  // An end-only boundary with incomplete incoming coverage must be repaired
  // into the do side instead of being accepted as a common redo cut.
  const auto incomplete_boundary_loop = discover_powl(
      log({{"A", "X", "A"}, {"A", "X", "B"}, {"A", "Y", "B"}}),
      tree);
  assert(incomplete_boundary_loop.kind() != PowlNodeKind::kLoop);

  PowlDiscoveryOptions nontransitive = tree;
  nontransitive.variant = PowlDiscoveryVariant::kBruteForce;
  const auto nontransitive_result = discover_powl(
      log({{"A", "B", "C"}, {"C", "A"}}), nontransitive);
  // The A->B->C evidence is not closed by A->C, while C and A are
  // bidirectional.  A merely acyclic partition would accept that order;
  // strict-partial-order validation rejects it and the fall-through retains
  // B's optional projected behavior.
  assert(nontransitive_result.kind() == PowlNodeKind::kStrictPartialOrder);
  assert(nontransitive_result.children().size() == 3);
  assert(nontransitive_result.children()[0].label() == "A");
  assert(nontransitive_result.children()[1].kind() == PowlNodeKind::kXor);
  assert(nontransitive_result.children()[2].label() == "C");
  assert(nontransitive_result.order().size() == 1 &&
         nontransitive_result.has_edge(1, 2));

  // A repeated singleton is the canonical flower/loop boundary case: the
  // executable do side comes first and the silent redo side comes second.
  const auto repeated_singleton = discover_powl(log({{"A", "A"}}), tree);
  assert(repeated_singleton.kind() == PowlNodeKind::kLoop);
  assert(repeated_singleton.children()[0].label() == "A");
  assert(repeated_singleton.children()[1].kind() == PowlNodeKind::kSilent);

  const auto flower_result = discover_powl(
      log({{"A", "D", "C", "B", "A"},
           {"D", "C", "B", "A", "D"},
           {"C", "B", "A", "D", "C"},
           {"B", "A", "D", "C", "B"},
           {"A", "D", "C", "B", "A"}}),
      tree);
  assert(flower_result.kind() == PowlNodeKind::kLoop);
  assert(flower_result.children()[0].kind() == PowlNodeKind::kXor);
  assert(flower_result.children()[0].children().size() == 4);
  assert(flower_result.children()[1].kind() == PowlNodeKind::kSilent);

  // Activity-concurrent also probes ordinary cuts for dynamic variants.  The
  // remainder exposes the same XOR split as the tree variant.  The probe is
  // intentionally boolean-only; its result must not depend on recursively
  // materializing the projected child models.
  for (const auto variant : {PowlDiscoveryVariant::kDynamicClustering,
                             PowlDiscoveryVariant::kDynamicClusteringPlain}) {
    auto dynamic = tree;
    dynamic.variant = variant;
    const auto concurrent = discover_powl(
        log({{"B", "A", "A"}, {"A", "A", "B"}, {"A", "A", "C"}}),
        dynamic);
    assert(concurrent.kind() == PowlNodeKind::kStrictPartialOrder);
    assert(concurrent.order().empty() && concurrent.children().size() == 2);
    assert(concurrent.children()[0].kind() == PowlNodeKind::kLoop);
    assert(concurrent.children()[0].children()[0].label() == "A");
    assert(concurrent.children()[0].children()[1].kind() ==
           PowlNodeKind::kSilent);
    assert(concurrent.children()[1].kind() == PowlNodeKind::kXor);
    assert(concurrent.children()[1].children().size() == 2);
  }

  // Final normalization folds an optional loop into its canonical loop form
  // and flattens a disconnected nested order into its parent.
  const auto simplified_optional =
      discover_powl(log({{}, {"A", "A"}}), tree);
  assert(simplified_optional.kind() == PowlNodeKind::kLoop);
  assert(simplified_optional.children()[0].kind() == PowlNodeKind::kSilent);
  assert(simplified_optional.children()[1].label() == "A");

  // Brute force on many activities refuses a partition level that would pass
  // its memory bound, before generating it, instead of allocating gigabytes
  // (a 32-activity log needs 15,642,600 partitions into 29 groups).
  {
    EventLog many;
    std::uint32_t seed = 20090;
    const auto next = [&seed](std::uint32_t bound) {
      seed = seed * 1664525u + 1013904223u;
      return (seed >> 8) % bound;
    };
    const std::vector<std::string> names = [] {
      std::vector<std::string> out;
      for (int i = 0; i < 32; ++i) out.push_back("A" + std::to_string(i));
      return out;
    }();
    for (int c = 0; c < 40; ++c) {
      Trace trace;
      const auto length = 3 + next(10);
      for (std::uint32_t e = 0; e < length; ++e) trace.events.push_back(Event{"", names[next(32)]});
      many.traces.push_back(std::move(trace));
    }
    Trace all;
    for (const auto& name : names) all.events.push_back(Event{"", name});
    many.traces.push_back(std::move(all));
    PowlDiscoveryOptions bounded;
    bounded.variant = PowlDiscoveryVariant::kBruteForce;
    std::string message;
    try { (void)discover_powl(many, bounded); } catch (const std::runtime_error& error) { message = error.what(); }
    assert(message.find("POWL brute force would enumerate ") == 0);
    assert(message.find("more than its memory bound allows") != std::string::npos);
  }

  std::cout << "POWL discovery tests passed\n";
}
