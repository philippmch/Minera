#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <string>

#include "minera/edit_distance_alignment.hpp"
#include "minera/json_export.hpp"

using namespace minera;

static EventLog log_of(std::initializer_list<std::initializer_list<const char*>> traces) {
  EventLog log;
  std::size_t c = 0;
  for (const auto& activities : traces) {
    Trace trace;
    trace.case_id = std::to_string(++c);
    std::size_t i = 0;
    for (const auto activity : activities)
      trace.events.push_back(Event{trace.case_id, activity, "t" + std::to_string(i++), {}, i});
    log.traces.push_back(std::move(trace));
  }
  return log;
}

int main() {
  const EventLog source = log_of({{"a", "b"}, {"a", "x"}, {}});
  const EventLog target = log_of({{"a", "b"}, {"a", "c"}, {"a"}});
  const auto exact = align_edit_distance(log_of({{"a", "b"}}), target);
  assert(exact.size() == 1 && exact[0].cost == 0 && exact[0].fitness == 1.0);

  const auto deviations = align_edit_distance(log_of({{"a", "z"}}), log_of({{"a", "b"}}));
  assert(deviations[0].cost == 20000);
  assert(deviations[0].alignment.size() == 3);

  const auto repeated = align_edit_distance(log_of({{"a", "b", "a"}}),
                                            log_of({{"a", "a", "b", "a"}}));
  assert(repeated[0].cost == 10000);
  assert(repeated[0].alignment.front().type == EditDistanceMoveType::kModel);
  assert(repeated[0].alignment.front().model_activity == "a");

  // SequenceMatcher autojunk remains deterministic on long repeated traces;
  // exact-target matching is still synchronous.
  EventLog long_target;
  Trace long_trace;
  for (std::size_t i = 0; i < 240; ++i)
    long_trace.events.push_back(Event{"long", "repeat", "same", {}, i});
  long_target.traces.push_back(long_trace);
  const auto long_exact = align_edit_distance(long_target, long_target);
  assert(long_exact[0].cost == 0 && long_exact[0].alignment.size() == 240);
  const auto long_deviation = align_edit_distance(log_of({{"repeat"}}), long_target);
  assert(long_deviation[0].cost == 239 * 10000);

  // Equal distances use lexical target ordering, independent of target order.
  const auto tie = align_edit_distance(log_of({{"a"}}), log_of({{"b"}, {"c"}}));
  assert(tie[0].target_variant == std::vector<std::string>{"b"});
  EditDistanceOptions anti;
  anti.perform_anti_alignment = true;
  const auto far = align_edit_distance(log_of({{"a"}}), log_of({{"a"}, {"b", "c"}}), anti);
  assert(far[0].target_variant == std::vector<std::string>({"b", "c"}));

  const auto duplicate = align_edit_distance(log_of({{"a"}, {"a"}, {"b"}}),
                                             log_of({{"a"}, {"a"}}));
  assert(duplicate.size() == 3 && duplicate[0].target_index == duplicate[1].target_index);
  assert(duplicate[2].target_variant == std::vector<std::string>{"a"});

  assert(align_edit_distance(EventLog{}, target).empty());
  bool threw = false;
  try { (void)align_edit_distance(source, EventLog{}); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  const auto empty_trace = align_edit_distance(log_of({{}}), log_of({{"a"}}));
  assert(empty_trace[0].cost == 10000 && empty_trace[0].alignment.size() == 1);

  const auto projected = project_log_on_variant(log_of({{"a", "x"}, {}}), {"a", "b"});
  assert(projected.traces.size() == 2);
  assert(projected.attribute("@@aligned_variant") != nullptr);
  assert(projected.traces[0].attribute("@@original_trace") != nullptr);
  assert(projected.traces[0].events.size() == 2);
  assert(projected.traces[0].events[0].attribute("@@is_conforming")->compare("true") == 0);
  assert(projected.traces[0].events[1].attribute("@@is_conforming")->compare("false") == 0);
  assert(projected.traces[0].events[0].timestamp == "t0");
  assert(projected.traces[0].events[1].timestamp == "t0");
  assert(projected.traces[0].events[0].input_order == 0 &&
         projected.traces[0].events[1].input_order == 1);
  assert(projected.traces[1].events.size() == 2);  // empty source trace retained
  assert(projected.traces[1].events[0].timestamp.empty());

  EventLog marked = log_of({{"a"}});
  marked.attributes.push_back({"owner", "kept", AttributeKind::kString});
  marked.attributes.push_back({"@@aligned_variant", "old"});
  marked.attributes.push_back({"@@aligned_variant", "older"});
  marked.traces[0].attributes.push_back({"@@original_trace", "old"});
  marked.traces[0].attributes.push_back({"@@original_trace", "older"});
  marked.traces[0].events[0].attributes.push_back({"@@is_conforming", "old"});
  marked.traces[0].events[0].attributes.push_back({"@@is_conforming", "older"});
  const auto marked_projection = project_log_on_variant(marked, {"a"});
  assert(marked_projection.attribute("owner")->compare("kept") == 0);
  assert(marked_projection.attribute("@@aligned_variant")->compare("a") == 0);
  assert(marked_projection.traces[0].attribute("@@original_trace")->compare("a") == 0);
  assert(marked_projection.traces[0].events[0].attribute("@@is_conforming")->compare("true") == 0);
  assert(std::count_if(marked_projection.attributes.begin(), marked_projection.attributes.end(),
                       [](const auto& a) { return a.key == "@@aligned_variant"; }) == 1);
  assert(std::count_if(marked_projection.traces[0].attributes.begin(), marked_projection.traces[0].attributes.end(),
                       [](const auto& a) { return a.key == "@@original_trace"; }) == 1);
  assert(std::count_if(marked_projection.traces[0].events[0].attributes.begin(), marked_projection.traces[0].events[0].attributes.end(),
                       [](const auto& a) { return a.key == "@@is_conforming"; }) == 1);

  // Log projection must use the selected target independently for each
  // source trace; heterogeneous source logs must not be rejected.
  EventLog heterogeneous_source = log_of({{"a"}, {"b"}});
  EventLog heterogeneous_target = log_of({{"a"}, {"b"}});
  heterogeneous_source.traces[0].events[0].resource = "alice";
  heterogeneous_source.traces[0].events[0].lifecycle = "complete";
  heterogeneous_source.traces[0].events[0].attributes.push_back(
      {"amount", "7", AttributeKind::kInt});
  const auto heterogeneous_alignments =
      align_edit_distance(heterogeneous_source, heterogeneous_target);
  const auto heterogeneous_projection =
      project_log_on_alignments(heterogeneous_source, heterogeneous_alignments);
  assert(heterogeneous_projection.traces.size() == 2);
  assert(heterogeneous_projection.traces[0].attribute("@@aligned_variant") != nullptr);
  assert(heterogeneous_projection.traces[0].attribute("@@aligned_variant")->compare("a") == 0);
  assert(heterogeneous_projection.traces[1].attribute("@@aligned_variant")->compare("b") == 0);
  assert(heterogeneous_projection.traces[0].events[0].resource == "alice");
  assert(heterogeneous_projection.traces[0].events[0].lifecycle == "complete");
  assert(heterogeneous_projection.traces[0].events[0].input_order == 0);
  const auto projected_json = edit_distance_json(
      heterogeneous_alignments, false, &heterogeneous_projection);
  assert(projected_json.find("\"resource\":\"alice\"") != std::string::npos);
  assert(projected_json.find("\"lifecycle\":\"complete\"") != std::string::npos);
  assert(projected_json.find("\"input_order\":0") != std::string::npos);
  assert(projected_json.find("\"attribute_kinds\":{\"amount\":\"int\"") != std::string::npos);

  EventLog empty_source;
  empty_source.attributes.push_back({"@@aligned_variant", "stale", AttributeKind::kString});
  empty_source.attributes.push_back({"@@aligned_variants", "stale", AttributeKind::kString});
  const auto empty_projection = project_log_on_alignments(empty_source, {});
  assert(empty_projection.attribute("@@aligned_variant") == nullptr);
  assert(empty_projection.attribute("@@aligned_variants") == nullptr);
  return 0;
}
