#include "minera/powl_discovery.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
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
  assert(once.order().empty() && once.children().size() == 2);
  assert(once.children()[0].label() == "A");
  assert(once.children()[1].kind() ==
         PowlNodeKind::kStrictPartialOrder);
  assert(once.children()[1].has_edge(0, 1));

  // ActivityConcurrent follows ActivityOnce: A repeats in every trace, and
  // removing it exposes the XOR(B,C) cut.
  const auto concurrent_fall_through = discover_powl(
      log({{"B", "A", "A"}, {"A", "A", "B"}, {"A", "A", "C"}}),
      tree);
  assert(concurrent_fall_through.kind() ==
         PowlNodeKind::kStrictPartialOrder);
  assert(concurrent_fall_through.order().empty());
  assert(concurrent_fall_through.children().size() == 2);
  assert(concurrent_fall_through.children()[0].kind() ==
         PowlNodeKind::kLoop);
  assert(concurrent_fall_through.children()[1].kind() ==
         PowlNodeKind::kXor);

  // Brute-force starts at k=n-1, uses EFG (not merely adjacent DFG), and is deterministic.
  PowlDiscoveryOptions brute; brute.variant = PowlDiscoveryVariant::kBruteForce;
  const auto b = discover_powl(log({{"A", "B", "C"}}), brute);
  assert(b.serialize() ==
         "PO=(nodes={T(\"A\"),T(\"B\"),T(\"C\")},"
         "order={A-->B,A-->C,B-->C})");
  const auto b_rev = discover_powl(log({{"A", "B", "C"}}), brute).serialize();
  assert(b.serialize() == b_rev);
  throws([&] { brute.brute_force_activity_limit = 2; discover_powl(log({{"A", "B", "C"}}), brute); });

  // Maximal groups activities with identical one-way EFG neighborhoods.
  PowlDiscoveryOptions maximal; maximal.variant = PowlDiscoveryVariant::kMaximal;
  const auto m = discover_powl(log({{"A", "B"}, {"A", "C"}}), maximal);
  assert(m.kind() == PowlNodeKind::kStrictPartialOrder);
  assert(m.children().size() == 2 && m.has_edge(0, 1));
  const auto maximal_chain = discover_powl(log({{"A", "B", "C"}}), maximal);
  assert(maximal_chain.serialize() == "PO=(nodes={T(\"A\"),T(\"B\"),T(\"C\")},order={A-->B,A-->C,B-->C})");

  // Plain and frequency dynamic are distinct source variants; both terminate
  // on a three-activity non-transitive fixture.
  PowlDiscoveryOptions plain; plain.variant = PowlDiscoveryVariant::kDynamicClusteringPlain;
  PowlDiscoveryOptions frequency; frequency.variant = PowlDiscoveryVariant::kDynamicClustering; frequency.order_frequency_ratio = 0.9;
  const auto dp = discover_powl(log({{"A", "B"}, {"B", "C"}, {"A", "C"}}), plain);
  const auto df = discover_powl(log({{"A", "B"}, {"B", "C"}, {"A", "C"}}), frequency);
  assert(!dp.serialize().empty() && !df.serialize().empty());
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

  // Dynamic filtering removes every minimum-frequency variant only after
  // normal cuts fail.
  filtered.filtering_type = PowlFilteringType::kDynamic;
  filtered.filtering_threshold = 0.2;
  const auto dynamically_filtered =
      discover_powl(filtering_log, filtered).serialize();
  assert(dynamically_filtered.find("T(\"C\")") == std::string::npos);
  // Dynamic filtering ignores the decreasing-factor threshold, including
  // its zero default, and removes every least-frequent variant.
  filtered.filtering_threshold = 0.0;
  const auto dynamic_default_threshold =
      discover_powl(filtering_log, filtered).serialize();
  assert(dynamic_default_threshold.find("T(\"C\")") == std::string::npos);

  // Decreasing-factor retention is strict: a variant exactly equal to the
  // threshold boundary is not retained.
  EventLog equality_log;
  append_trace(equality_log, {"B", "A", "A"}, 100);
  append_trace(equality_log, {"A", "A", "B"}, 50);
  append_trace(equality_log, {"A", "A", "C"}, 50);
  auto strict_filter = tree;
  strict_filter.filtering_threshold = 0.5;
  const auto strict_boundary = discover_powl(equality_log, strict_filter).serialize();
  assert(strict_boundary.find("T(\"C\")") == std::string::npos);
  throws([&] { auto o = filtered; o.filtering_threshold = 1.0; discover_powl(log({{"A"}}), o); });
  throws([&] { auto o = filtered; o.filtering_threshold = -0.1; discover_powl(log({{"A"}}), o); });
  throws([&] { auto o = filtered; o.filtering_type = static_cast<PowlFilteringType>(99); discover_powl(log({{"A"}}), o); });
  throws([&] { auto o = filtered; o.variant = static_cast<PowlDiscoveryVariant>(99); discover_powl(log({{"A"}}), o); });
  throws([&] { auto o = filtered; o.order_frequency_ratio = std::numeric_limits<double>::quiet_NaN(); discover_powl(log({{"A"}}), o); });

  const auto ordered_a = discover_powl(log({{"B", "A"}, {"A", "B"}}), tree).serialize();
  const auto ordered_b = discover_powl(log({{"A", "B"}, {"B", "A"}}), tree).serialize();
  assert(ordered_a == ordered_b);
  std::cout << "POWL discovery tests passed\n";
}
