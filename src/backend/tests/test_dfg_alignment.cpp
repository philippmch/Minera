#include <cassert>
#include <cmath>
#include <map>
#include <stdexcept>

#include "minera/dfg_alignment.hpp"

using namespace minera;

int main() {
  DfgFrequency d{{{"a", "b"}, 1}};
  std::map<std::string, std::uint64_t> s{{"a", 1}}, e{{"b", 1}};
  auto exact = align_trace_on_dfg(std::vector<std::string>{"a", "b"}, d, s, e);
  assert(exact.cost == 0 && exact.alignment.size() == 2);
  assert(exact.alignment[0].log == "a" && exact.alignment[0].model == "a");

  auto unknown = align_trace_on_dfg(std::vector<std::string>{"x"}, d, s, e);
  assert(unknown.alignment.size() >= 3 && unknown.cost == 30000);
  auto empty = align_trace_on_dfg(std::vector<std::string>{}, d, s, e);
  assert(empty.cost == 20000 && empty.alignment.size() == 2);

  EventLog log;
  Trace t1; t1.case_id = "1"; t1.events.push_back(Event{"1", "a", "t1", {}, 0});
  t1.events.push_back(Event{"1", "b", "t2", {}, 1});
  Trace t2=t1; t2.case_id="2"; log.traces={t2,t1};
  auto all=align_log_on_dfg(log,d,s,e); assert(all.alignments.size()==2 && all.unique_variants==1);
  auto projected=project_log_on_dfg(log,d,s,e); assert(projected.traces.size()==2);
  assert(projected.traces[0].events[0].attribute("@@is_conforming") != nullptr);
  assert(projected.attribute("@@aligned_dfg") != nullptr);
  auto agg=project_alignments_on_dfg(log,d,s,e); assert(agg.activities["a"].sync==2);
  assert(agg.activities["b"].sync==2 && agg.dfg.at({"a", "b"}).sync==2);

  // Reserved projection metadata replaces collisions, including copied
  // source event attributes, and the final log move is counted.
  EventLog colliding = log;
  colliding.attributes.push_back({"@@aligned_dfg", "stale", AttributeKind::kString});
  colliding.traces[0].attributes.push_back({"@@original_trace", "stale", AttributeKind::kString});
  colliding.traces[0].events[0].attributes.push_back({"@@is_conforming", "stale", AttributeKind::kString});
  colliding.traces[0].events[0].lifecycle = "complete";
  colliding.traces[0].events.push_back(Event{"2", "a", "t3", {}, 2});
  auto pc = project_log_on_dfg(colliding, d, s, e);
  assert(pc.attribute("@@aligned_dfg") && *pc.attribute("@@aligned_dfg") != "stale");
  assert(pc.traces[0].attribute("@@original_trace") && *pc.traces[0].attribute("@@original_trace") != "stale");
  assert(pc.traces[0].events[0].attribute("@@is_conforming") && *pc.traces[0].events[0].attribute("@@is_conforming") == "true");
  assert(pc.traces[0].events[0].lifecycle == "complete");
  auto agc = project_alignments_on_dfg(colliding, d, s, e);
  assert(agc.activities["a"].lm == 1);

  DfgAlignmentOptions bounded; bounded.max_visited_states = 1; bool limit = false;
  try { (void)align_trace_on_dfg(std::vector<std::string>{"a", "b"}, d, s, e, bounded); } catch (const std::runtime_error&) { limit = true; }
  assert(limit);
  DfgFrequency cyc{{{"a", "b"}, 1}, {{"b", "a"}, 1}};
  auto cycle = align_trace_on_dfg(std::vector<std::string>{"a", "b", "a"}, cyc, s, s);
  assert(cycle.cost == 0 && cycle.alignment.size() == 3);

  DfgAlignmentOptions custom; custom.log_move_cost["x"]=2; custom.internal_log_move_cost["x"]=0.5;
  auto c=align_trace_on_dfg(std::vector<std::string>{"x"},d,s,e,custom); assert(c.cost==20002 && c.internal_cost< c.cost);
  bool threw=false; try { custom.log_move_cost["x"]=-1; (void)align_trace_on_dfg(std::vector<std::string>{"x"},d,s,e,custom); } catch(const std::invalid_argument&) { threw=true; } assert(threw);

  DfgFrequency bad{{{"a", "b"},1},{{"c", "d"},1}};
  threw=false; try { (void)align_trace_on_dfg({},bad,s,e); } catch(const std::invalid_argument&) { threw=true; } assert(threw);
  return 0;
}
