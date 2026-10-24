#include <cassert>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>
#include "minera/process_tree_alignment.hpp"

using namespace minera;
namespace {
EventLog log_of(const std::vector<std::string>& xs) { Trace t; t.case_id="c"; std::size_t i=0; for (const auto& x:xs) t.events.push_back(Event{"c",x,{}, {},i++}); return EventLog{{std::move(t)}}; }
EventLog log_of(std::initializer_list<std::string> xs) { return log_of(std::vector<std::string>(xs)); }
ProcessTree seq(std::vector<std::string> a) { std::vector<ProcessTreeNode> c; for(std::size_t i=0;i<a.size();++i)c.push_back(ProcessTreeNode::leaf(i)); return ProcessTree(std::move(a), ProcessTreeNode{ProcessTreeOperator::kSequence,0,std::move(c)}); }
ProcessTree loop2() { return ProcessTree({"a","b"}, ProcessTreeNode{ProcessTreeOperator::kLoop,0,{ProcessTreeNode::leaf(0),ProcessTreeNode::leaf(1)}}); }
ProcessTree parallel2() { return ProcessTree({"a","b"}, ProcessTreeNode{ProcessTreeOperator::kParallel,0,{ProcessTreeNode::leaf(0),ProcessTreeNode::leaf(1)}}); }
ProcessTree inclusive2() { return ProcessTree({"a","b"}, ProcessTreeNode{ProcessTreeOperator::kInclusiveChoice,0,{ProcessTreeNode::leaf(0),ProcessTreeNode::leaf(1)}}); }
ProcessTree interleaving2() { return ProcessTree({"a","b","c"}, ProcessTreeNode{ProcessTreeOperator::kInterleaving,0,{ProcessTreeNode{ProcessTreeOperator::kSequence,0,{ProcessTreeNode::leaf(0),ProcessTreeNode::leaf(1)}},ProcessTreeNode::leaf(2)}}); }
ProcessTree nested() { return ProcessTree({"a","b","c","d"}, ProcessTreeNode{ProcessTreeOperator::kSequence,0,{
  ProcessTreeNode{ProcessTreeOperator::kExclusive,0,{ProcessTreeNode::leaf(0),ProcessTreeNode::leaf(1)}},
  ProcessTreeNode{ProcessTreeOperator::kParallel,0,{ProcessTreeNode::leaf(2),ProcessTreeNode::leaf(3)}}}}); }
}

int main() {
  for (auto v : {ProcessTreeAlignmentVariant::SEARCH_GRAPH_PT, ProcessTreeAlignmentVariant::DYNAMIC_PROGRAMMING, ProcessTreeAlignmentVariant::MILP, ProcessTreeAlignmentVariant::APPROXIMATED_ORIGINAL}) {
    auto r=align_process_tree(log_of({"a","b"}),seq({"a","b"}),ProcessTreeAlignmentOptions{v}); assert(r.computed&&r.traces.size()==1&&r.total_cost==0&&r.fitness==1.0);
    auto l0=align_process_tree(log_of({"a","a"}),loop2(),ProcessTreeAlignmentOptions{v}); assert(v==ProcessTreeAlignmentVariant::DYNAMIC_PROGRAMMING ? l0.total_cost==0 : l0.total_cost>0);  // the reference DP accepts a trailing extra body
    auto l1=align_process_tree(log_of({"a","b","a"}),loop2(),ProcessTreeAlignmentOptions{v}); assert(l1.total_cost==0);
  }
  for (auto v : {ProcessTreeAlignmentVariant::SEARCH_GRAPH_PT, ProcessTreeAlignmentVariant::DYNAMIC_PROGRAMMING, ProcessTreeAlignmentVariant::MILP, ProcessTreeAlignmentVariant::APPROXIMATED_ORIGINAL}) {
    ProcessTreeAlignmentOptions o; o.variant=v;
    assert(align_process_tree(log_of({"a","b","c"}),interleaving2(),o).total_cost==0);
    assert(align_process_tree(log_of({"c","a","b"}),interleaving2(),o).total_cost==0);
    assert(align_process_tree(log_of({"a","c","b"}),interleaving2(),o).total_cost>0);
  }
  auto tau=ProcessTree({"a"},ProcessTreeNode::tau()); auto e=align_process_tree(log_of({}),tau); assert(e.computed&&e.total_cost==0&&e.fitness==0.0);
  for (auto v : {ProcessTreeAlignmentVariant::SEARCH_GRAPH_PT, ProcessTreeAlignmentVariant::DYNAMIC_PROGRAMMING, ProcessTreeAlignmentVariant::MILP, ProcessTreeAlignmentVariant::APPROXIMATED_ORIGINAL}) {
    ProcessTreeAlignmentOptions o; o.variant=v;
    auto p=align_process_tree(log_of({"b","a"}),parallel2(),o); assert(p.computed&&p.total_cost==0);
    auto n=align_process_tree(log_of({"b","d","c"}),nested(),o); assert(n.computed&&n.total_cost==0);
    assert(n.diagnostics.traces_aligned==1);
  }
  for (auto v : {ProcessTreeAlignmentVariant::SEARCH_GRAPH_PT, ProcessTreeAlignmentVariant::DYNAMIC_PROGRAMMING, ProcessTreeAlignmentVariant::MILP, ProcessTreeAlignmentVariant::APPROXIMATED_ORIGINAL}) {
    ProcessTreeAlignmentOptions o; o.variant=v;
    for (const auto& trace : {std::vector<std::string>{"a"},
                              std::vector<std::string>{"b"},
                              std::vector<std::string>{"a", "b"},
                              std::vector<std::string>{"b", "a"}}) {
      const auto result = align_process_tree(log_of(trace), inclusive2(), o);
      assert(result.computed && result.total_cost == 0);
    }
  }
  ProcessTree dup({"a","a"},ProcessTreeNode{ProcessTreeOperator::kSequence,0,{ProcessTreeNode::leaf(0),ProcessTreeNode::leaf(1)}});
  ProcessTreeAlignmentOptions dp; dp.variant=ProcessTreeAlignmentVariant::DYNAMIC_PROGRAMMING;
  assert(align_process_tree(log_of({"a"}),dup,dp).total_cost==1);
  ProcessTree nary({"a","b","c"},ProcessTreeNode{ProcessTreeOperator::kLoop,0,{ProcessTreeNode::leaf(0),ProcessTreeNode::leaf(1),ProcessTreeNode::leaf(2)}});
  for (auto v : {ProcessTreeAlignmentVariant::SEARCH_GRAPH_PT, ProcessTreeAlignmentVariant::DYNAMIC_PROGRAMMING, ProcessTreeAlignmentVariant::APPROXIMATED_ORIGINAL}) {
    ProcessTreeAlignmentOptions o; o.variant=v; assert(align_process_tree(log_of({"a","c","a"}),nary,o).total_cost==0);
  }
  auto bad=seq({"a"}); ProcessTreeAlignmentOptions lim; lim.max_states=1; auto ex=align_process_tree(log_of({"a","b"}),bad,lim); assert(ex.exhausted&&!ex.traces.empty()&&!ex.traces.front().completed);
  ProcessTreeAlignmentOptions ap; ap.variant=ProcessTreeAlignmentVariant::APPROXIMATED_ORIGINAL; ap.max_trace_length=1; auto ar=align_process_tree(log_of({"a","b","a"}),loop2(),ap); assert(ar.diagnostics.message.find("decomposition")!=std::string::npos);
  ap.max_trace_length=10; auto abase=align_process_tree(log_of({"b","a"}),parallel2(),ap); assert(abase.total_cost==0);
  EventLog repeated=log_of({"b","a"}); repeated.traces.push_back(repeated.traces.front());
  dp.use_global_cache=true; dp.batch_size=1; dp.clear_cache=true;
  auto cached=align_process_tree(repeated,parallel2(),dp); assert(cached.traces.size()==2&&cached.total_cost==0&&cached.diagnostics.traces_aligned==2);
  ProcessTree parallel_duplicate({"a","a"},ProcessTreeNode{ProcessTreeOperator::kParallel,0,{ProcessTreeNode::leaf(0),ProcessTreeNode::leaf(1)}});
  auto duplicate_parallel=align_process_tree(log_of({"a","a"}),parallel_duplicate,dp); assert(duplicate_parallel.total_cost==0);
  auto missing_parallel=align_process_tree(log_of({"a"}),parallel2(),dp); assert(missing_parallel.total_cost==1); std::size_t move_cost=0; bool inserted_b=false; for(const auto& move:missing_parallel.traces.front().moves){if(move.type!=ProcessTreeAlignmentMoveType::kSynchronous)++move_cost;if(move.type==ProcessTreeAlignmentMoveType::kModel&&move.model_activity=="b")inserted_b=true;} assert(move_cost==missing_parallel.total_cost&&inserted_b);
  ProcessTree ordered_parallel({"a","b","c"},ProcessTreeNode{ProcessTreeOperator::kParallel,0,{ProcessTreeNode{ProcessTreeOperator::kSequence,0,{ProcessTreeNode::leaf(0),ProcessTreeNode::leaf(1)}},ProcessTreeNode::leaf(2)}});
  auto ordered=align_process_tree(log_of({"b","c"}),ordered_parallel,dp); assert(ordered.total_cost==1&&ordered.traces.front().moves.size()==3); assert(ordered.traces.front().moves[0].type==ProcessTreeAlignmentMoveType::kModel&&ordered.traces.front().moves[0].model_activity=="a"); assert(ordered.traces.front().moves[1].type==ProcessTreeAlignmentMoveType::kSynchronous&&ordered.traces.front().moves[1].activity=="b");
  bool saw_parallel_exhaustion=false; for(std::size_t cap=1;cap<40;++cap){auto limited=dp;limited.max_states=cap;auto candidate=align_process_tree(log_of({"a","a"}),parallel_duplicate,limited);if(candidate.exhausted){saw_parallel_exhaustion=true;assert(!candidate.computed&&!candidate.completed&&!candidate.traces.front().completed);}} assert(saw_parallel_exhaustion);
  ProcessTreeAlignmentOptions total_timeout; total_timeout.max_total_time_seconds=1e-15;
  auto timed=align_process_tree(repeated,parallel2(),total_timeout); assert(timed.timed_out&&!timed.completed&&timed.traces.size()==2&&timed.traces.front().timed_out&&timed.traces.back().timed_out);
  bool malformed=false; try { ProcessTree t({"a"},ProcessTreeNode::leaf(3)); (void)align_process_tree(log_of({"a"}),t); } catch(const std::invalid_argument&) { malformed=true; } assert(malformed);
  bool nonbinary=false; try { ProcessTree t({"a"},ProcessTreeNode{ProcessTreeOperator::kLoop,0,{ProcessTreeNode::leaf(0),ProcessTreeNode::leaf(0),ProcessTreeNode::leaf(0)}}); ProcessTreeAlignmentOptions o; o.variant=ProcessTreeAlignmentVariant::MILP; (void)align_process_tree(log_of({"a"}),t,o); } catch(const std::invalid_argument&) { nonbinary=true; } assert(nonbinary);
  bool bad_batch=false; try { ProcessTreeAlignmentOptions o; o.variant=ProcessTreeAlignmentVariant::DYNAMIC_PROGRAMMING; o.batch_size=0; (void)align_process_tree(log_of({"a"}),seq({"a"}),o); } catch(const std::invalid_argument&) { bad_batch=true; } assert(bad_batch);

  // Every native process-tree variant consumes the selected generic activity
  // field, not the convenience Event::activity projection.
  EventLog selected;
  Trace selected_trace;
  Event first{"c", "ignored"};
  first.attributes.push_back({"phase", "a"});
  Event second{"c", "ignored"};
  second.attributes.push_back({"phase", "b"});
  selected_trace.events = {std::move(first), std::move(second)};
  selected.traces.push_back(std::move(selected_trace));
  for (auto v : {ProcessTreeAlignmentVariant::SEARCH_GRAPH_PT,
                 ProcessTreeAlignmentVariant::DYNAMIC_PROGRAMMING,
                 ProcessTreeAlignmentVariant::MILP,
                 ProcessTreeAlignmentVariant::APPROXIMATED_ORIGINAL}) {
    ProcessTreeAlignmentOptions options;
    options.variant = v;
    options.activity_attribute = "phase";
    const auto result = align_process_tree(selected, seq({"a", "b"}), options);
    assert(result.computed && result.total_cost == 0 &&
           result.traces.front().trace == std::vector<std::string>({"a", "b"}));
  }
  ProcessTreeAlignmentOptions missing_selector;
  missing_selector.activity_attribute = "missing-phase";
  bool missing = false;
  try { (void)align_process_tree(selected, seq({"a", "b"}), missing_selector); }
  catch (const std::invalid_argument&) { missing = true; }
  assert(missing);
  EventLog null_selected = selected;
  null_selected.traces.front().events.front().attributes.front().is_null = true;
  ProcessTreeAlignmentOptions null_options;
  null_options.activity_attribute = "phase";
  missing = false;
  try { (void)align_process_tree(null_selected, seq({"a", "b"}), null_options); }
  catch (const std::invalid_argument&) { missing = true; }
  assert(missing);
  missing_selector.activity_attribute = "\t";
  missing = false;
  try { (void)align_process_tree(selected, seq({"a", "b"}), missing_selector); }
  catch (const std::invalid_argument&) { missing = true; }
  assert(missing);
  return 0;
}
