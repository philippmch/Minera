#include <cassert>
#include <cmath>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <utility>

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

  // Model moves inherit custom first-class selectors from the source event,
  // while an incompatible typed activity value is represented as a string.
  EventLog custom_log;
  Trace custom_trace;
  custom_trace.case_id = "42";
  custom_trace.case_id_state = FirstClassAttributeState::kPresent;
  custom_trace.case_id_source_key = "case.number";
  EventAttribute custom_case{"case.number", "42", AttributeKind::kInt};
  custom_case.typed_value = std::int64_t{42};
  custom_trace.case_id_attribute = custom_case;
  Event custom_event;
  custom_event.case_id = "42";
  custom_event.case_id_state = FirstClassAttributeState::kPresent;
  custom_event.case_id_source_key = "case.number";
  custom_event.activity = "7";
  custom_event.activity_state = FirstClassAttributeState::kPresent;
  custom_event.activity_source_key = "activity.code";
  EventAttribute custom_activity{"activity.code", "7", AttributeKind::kInt};
  custom_activity.typed_value = std::int64_t{7};
  custom_event.activity_attribute = custom_activity;
  custom_event.timestamp = "2026-01-01T00:00:00Z";
  custom_event.timestamp_state = FirstClassAttributeState::kPresent;
  custom_event.timestamp_source_key = "when.value";
  EventAttribute custom_timestamp{
      "when.value", custom_event.timestamp, AttributeKind::kDate};
  custom_timestamp.typed_value =
      NativeDate{"2026-01-01T00:00:00.000Z"};
  custom_event.timestamp_attribute = custom_timestamp;
  custom_trace.events.push_back(std::move(custom_event));
  custom_log.traces.push_back(std::move(custom_trace));
  const auto custom_projection = project_log_on_dfg(custom_log, d, s, e);
  assert(custom_projection.traces.front().case_id_attribute &&
         custom_projection.traces.front().case_id_attribute->kind ==
             AttributeKind::kInt);
  assert(custom_projection.traces.front().events.size() == 2);
  const auto& model_event = custom_projection.traces.front().events.front();
  assert(model_event.activity == "a" &&
         model_event.activity_source_key == "activity.code" &&
         model_event.activity_attribute &&
         model_event.activity_attribute->kind == AttributeKind::kString &&
         model_event.activity_attribute->value == "a");
  assert(model_event.timestamp_source_key == "when.value" &&
         model_event.timestamp_attribute &&
         model_event.timestamp_attribute->kind == AttributeKind::kDate);

  DfgAlignmentOptions bounded; bounded.max_visited_states = 1; bool limit = false;
  try { (void)align_trace_on_dfg(std::vector<std::string>{"a", "b"}, d, s, e, bounded); } catch (const std::runtime_error&) { limit = true; }
  assert(limit);
  DfgFrequency cyc{{{"a", "b"}, 1}, {{"b", "a"}, 1}};
  auto cycle = align_trace_on_dfg(std::vector<std::string>{"a", "b", "a"}, cyc, s, s);
  assert(cycle.cost == 0 && cycle.alignment.size() == 3);

  // Revisiting a graph node at a later trace position must remain a distinct
  // search state.  The exact cyclic run is free, while the repeated log-only
  // event in the second trace has one deviation and then rejoins the cycle.
  DfgFrequency revisiting{{{"a", "b"}, 1},
                          {{"b", "a"}, 1},
                          {{"a", "c"}, 1}};
  std::map<std::string, std::uint64_t> revisit_start{{"a", 1}};
  std::map<std::string, std::uint64_t> revisit_end{{"c", 1}};
  const auto exact_revisit = align_trace_on_dfg(
      std::vector<std::string>{"a", "b", "a", "c"}, revisiting,
      revisit_start, revisit_end);
  assert(exact_revisit.cost == 0.0 && exact_revisit.alignment.size() == 4);
  const auto deviating_revisit = align_trace_on_dfg(
      std::vector<std::string>{"a", "b", "b", "a", "c"}, revisiting,
      revisit_start, revisit_end);
  assert(deviating_revisit.cost == 10000.0);
  assert(deviating_revisit.alignment.size() == 5);

  DfgAlignmentOptions custom; custom.log_move_cost["x"]=2; custom.internal_log_move_cost["x"]=0.5;
  auto c=align_trace_on_dfg(std::vector<std::string>{"x"},d,s,e,custom); assert(c.cost==20002 && c.internal_cost< c.cost);
  bool threw=false; try { custom.log_move_cost["x"]=-1; (void)align_trace_on_dfg(std::vector<std::string>{"x"},d,s,e,custom); } catch(const std::invalid_argument&) { threw=true; } assert(threw);

  DfgFrequency bad{{{"a", "b"},1},{{"c", "d"},1}};
  threw=false; try { (void)align_trace_on_dfg({},bad,s,e); } catch(const std::invalid_argument&) { threw=true; } assert(threw);
  return 0;
}
