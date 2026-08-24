#include "minera/footprints.hpp"
#include "minera/powl.hpp"

#include <cassert>
#include <stdexcept>

using namespace minera;

int main() {
  // Process-tree sequence, optional boundary and parallel cleanup.
  ProcessTree tree({"A", "B", "C"},
      ProcessTreeNode{ProcessTreeOperator::kSequence, 0,
          {ProcessTreeNode::leaf(0),
           ProcessTreeNode{ProcessTreeOperator::kExclusive, 0,
               {ProcessTreeNode::leaf(1), ProcessTreeNode::tau()}},
           ProcessTreeNode::leaf(2)}});
  const auto tf = discover_footprints(tree);
  assert(tf.activities == std::set<std::string>({"A", "B", "C"}));
  assert(tf.sequence.count({"A", "B"}) && tf.sequence.count({"A", "C"}));
  assert(tf.sequence.count({"B", "C"}));
  assert(tf.min_trace_length && *tf.min_trace_length == 2);
  assert(tf.max_trace_length_without_loops && *tf.max_trace_length_without_loops == 3);

  const ProcessTree xor_tree({"A", "B"},
      ProcessTreeNode{ProcessTreeOperator::kExclusive, 0,
          {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}});
  const auto xf = discover_footprints(xor_tree);
  assert(xf.start_activities == std::set<std::string>({"A", "B"}) &&
         xf.parallel.empty());
  assert(xf.skippable && !*xf.skippable);

  const ProcessTree inclusive_tree({"A", "B"},
      ProcessTreeNode{ProcessTreeOperator::kInclusiveChoice, 0,
          {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}});
  const auto inf = discover_footprints(inclusive_tree);
  assert(inf.start_activities == std::set<std::string>({"A", "B"}));
  assert(inf.end_activities == std::set<std::string>({"A", "B"}));
  assert(inf.parallel.count({"A", "B"}) && inf.parallel.count({"B", "A"}));
  assert(inf.min_trace_length && *inf.min_trace_length == 1);
  assert(inf.max_trace_length_without_loops &&
         *inf.max_trace_length_without_loops == 2);
  assert(inf.skippable && !*inf.skippable);

  const ProcessTree optional_inclusive({"A"},
      ProcessTreeNode{ProcessTreeOperator::kInclusiveChoice, 0,
          {ProcessTreeNode::tau(), ProcessTreeNode::leaf(0)}});
  const auto oinf = discover_footprints(optional_inclusive);
  assert(oinf.skippable && *oinf.skippable);
  assert(oinf.min_trace_length && *oinf.min_trace_length == 0);
  assert(oinf.max_trace_length_without_loops &&
         *oinf.max_trace_length_without_loops == 1);
  assert(oinf.activities_always_happening &&
         oinf.activities_always_happening->empty());

  const ProcessTree par_tree({"A", "B"},
      ProcessTreeNode{ProcessTreeOperator::kParallel, 0,
          {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}});
  const auto par = discover_footprints(par_tree);
  assert(par.parallel.count({"A", "B"}) && par.parallel.count({"B", "A"}));
  const ProcessTree loop_tree({"A"},
      ProcessTreeNode{ProcessTreeOperator::kLoop, 0,
          {ProcessTreeNode::leaf(0), ProcessTreeNode::tau()}});
  // Relation cleanup classifies the symmetric self-relation as
  // parallel, so it is removed from sequence.
  const auto loop_fp = discover_footprints(loop_tree);
  assert(!loop_fp.sequence.count({"A", "A"}));
  assert(loop_fp.parallel.count({"A", "A"}));
  const ProcessTree multi_redo({"A", "B", "C"},
      ProcessTreeNode{ProcessTreeOperator::kLoop, 0,
          {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1), ProcessTreeNode::leaf(2)}});
  const auto mrf = discover_footprints(multi_redo);
  assert(mrf.parallel.count({"A", "B"}) && mrf.parallel.count({"B", "A"}));
  assert(mrf.parallel.count({"A", "C"}) && mrf.parallel.count({"C", "A"}));
  assert(!mrf.parallel.count({"B", "C"}));

  // POWL strict partial order: incomparable activities are parallel and a
  // transitive edge still contributes the expected sequence relation.
  auto a = PowlNode::transition("A");
  auto b = PowlNode::transition("B");
  auto c = PowlNode::transition("C");
  auto po = PowlNode::strict_partial_order({a, b, c}, {{0, 1}, {1, 2}, {0, 2}});
  const auto pf = discover_footprints(po);
  assert(pf.sequence.count({"A", "B"}) && pf.sequence.count({"B", "C"}));
  assert(!pf.sequence.count({"A", "C"})); // transitive edge is reduced
  assert(pf.min_trace_length && *pf.min_trace_length == 3);

  const auto optional = PowlNode::transition("O", PowlFrequency{0, 1});
  const auto optional_fp = discover_footprints(optional);
  assert(optional_fp.start_activities.count("O"));
  assert(optional_fp.end_activities.count("O"));
  const auto po_xor = PowlNode::xor_node({PowlNode::silent(), optional});
  const auto pxf = discover_footprints(po_xor);
  assert(pxf.skippable && pxf.min_trace_length && *pxf.min_trace_length == 0);
  assert(pxf.start_activities.count("O"));
  assert(pxf.end_activities.count("O"));
  const auto p_boundary = PowlNode::sequence({PowlNode::transition("A"), optional,
                                              PowlNode::transition("B")});
  const auto pbf = discover_footprints(p_boundary);
  assert(pbf.start_activities.count("A") && pbf.end_activities.count("B"));
  assert(pbf.sequence.count({"A", "O"}));
  assert(pbf.sequence.count({"O", "B"}));
  assert(pbf.sequence.count({"A", "B"}));
  const auto p_loop = PowlNode::loop(PowlNode::transition("L"), PowlNode::silent());
  const auto p_loop_fp = discover_footprints(p_loop);
  assert(!p_loop_fp.sequence.count({"L", "L"}));
  assert(p_loop_fp.parallel.count({"L", "L"}));

  // Reachability footprints from a tiny Petri net, including a silent prefix.
  PetriNet net;
  const auto p0 = net.add_place(), p1 = net.add_place(), p2 = net.add_place();
  net.set_source(p0); net.set_sink(p2); net.set_activities({"A"});
  net.add_transition(std::nullopt, {p0}, {p1});
  net.add_transition(std::size_t{0}, {p1}, {p2});
  const auto nf = discover_footprints(net);
  assert(nf.start_activities == std::set<std::string>({"A"}));
  assert(nf.activities == std::set<std::string>({"A"}));
  assert(!nf.dfg_present && !nf.end_activities_present && !nf.min_trace_length_present);

  // Two concurrent transitions carrying the same label remain distinct until
  // projection, yielding the expected (A,A) parallel relation.
  PetriNet same_label;
  const auto s0 = same_label.add_place(), s1 = same_label.add_place(), s2 = same_label.add_place(), s3 = same_label.add_place();
  same_label.set_source(s0); same_label.set_sink(s3); same_label.set_activities({"A"});
  same_label.add_transition(std::nullopt, {s0}, {s1, s2});
  same_label.add_transition(std::size_t{0}, {s1}, {s3});
  same_label.add_transition(std::size_t{0}, {s2}, {s3});
  assert(discover_footprints(same_label).parallel.count({"A", "A"}));

  PetriNet unbounded;
  const auto u0 = unbounded.add_place();
  unbounded.set_source(u0); unbounded.set_sink(u0); unbounded.set_activities({"A"});
  unbounded.add_transition(std::size_t{0}, {u0}, {u0, u0});
  bool rejected = false;
  try { (void)discover_footprints(unbounded); } catch (const std::runtime_error&) { rejected = true; }
  assert(rejected);

  PetriNet silent_growth;
  const auto g0 = silent_growth.add_place();
  silent_growth.set_source(g0); silent_growth.set_sink(g0);
  silent_growth.add_transition(std::nullopt, {g0}, {g0, g0});
  rejected = false;
  try { (void)discover_footprints(silent_growth); }
  catch (const std::runtime_error&) { rejected = true; }
  assert(rejected);
  return 0;
}
