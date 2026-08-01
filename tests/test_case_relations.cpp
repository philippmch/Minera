#undef NDEBUG

#include "minera/case_relations.hpp"
#include "minera/json_export.hpp"

#include <cassert>
#include <sstream>
#include <stdexcept>

namespace {
using namespace minera;

Event event(std::string case_id, std::string activity, std::string timestamp) {
  Event result;
  result.case_id = std::move(case_id);
  result.activity = std::move(activity);
  result.timestamp = std::move(timestamp);
  return result;
}

EventLog base_log() {
  Trace trace;
  trace.case_id = "L1";
  trace.events = {event("L1", "A", "2024-01-01T00:00:00Z")};
  return EventLog{{trace}};
}

EventLog incoming_log() {
  Trace trace;
  trace.case_id = "R1";
  trace.events = {event("R1", "B", "2024-01-01T00:00:01Z"),
                  event("R1", "C", "2024-01-01T00:00:02Z")};
  return EventLog{{trace}};
}

void merges_and_drops() {
  const auto merged = merge_case_relations(base_log(), incoming_log(),
                                           {{"L1", "R1"}, {"L2", "R1"}});
  assert(merged.traces.size() == 2);
  assert(merged.traces[0].case_id == "L1");
  assert(merged.traces[0].events.size() == 3);
  assert(merged.traces[0].events[1].case_id == "L1");
  assert(merged.traces[1].case_id == "L2");
  assert(merged.traces[1].events.size() == 2);
  const auto dropped = merge_case_relations(base_log(), incoming_log(), {});
  assert(dropped.traces.size() == 1 && dropped.traces[0].events.size() == 1);
}

void parses_relations_and_custom_timestamps() {
  std::istringstream input("case_id_LEFT,case_id_RIGHT\nL1,R1\nL1,R1\n");
  const auto relations = parse_case_relations_csv(input);
  assert((relations.size() == 2 &&
          relations[0] == CaseRelation{"L1", "R1"}));
  std::istringstream quoted("case_id_LEFT,case_id_RIGHT\n\"L,1\",\"R\"\"1\"\n");
  const auto quoted_relations = parse_case_relations_csv(quoted);
  assert((quoted_relations.size() == 1 &&
          quoted_relations[0] == CaseRelation{"L,1", "R\"1"}));
  auto base = base_log();
  auto incoming = incoming_log();
  for (auto& event : base.traces[0].events)
    event.attributes.push_back({"started", event.timestamp, AttributeKind::kDate});
  for (auto& event : incoming.traces[0].events) {
    event.attributes.push_back({"started", event.timestamp, AttributeKind::kDate});
  }
  CaseRelationsOptions options;
  options.timestamp_key = "started";
  const auto merged = merge_case_relations(base, incoming, {{"L1", "R1"}}, options);
  assert(merged.traces[0].events.size() == 3);
  bool threw = false;
  try {
    std::istringstream bad("case_id_LEFT\nL1\n");
    (void)parse_case_relations_csv(bad);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  for (const auto& malformed : {
           std::string("case_id_LEFT,case_id_RIGHT\n\"L1\"junk,R1\n"),
           std::string("case_id_LEFT,case_id_RIGHT\nL\"1,R1\n"),
           std::string("case_id_LEFT,case_id_LEFT\nL1,R1\n")}) {
    threw = false;
    try {
      std::istringstream bad(malformed);
      (void)parse_case_relations_csv(bad);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void rejects_invalid_timestamps() {
  auto base = base_log();
  base.traces[0].events[0].timestamp = "invalid";
  bool threw = false;
  try {
    (void)merge_case_relations(base, {}, {});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void json_shape_is_stable() {
  const auto merged = merge_case_relations(base_log(), incoming_log(),
                                           {{"L1", "R1"}});
  const auto json = case_relations_json(merged);
  assert(json.find(R"({"family":"MERGE_CASE_RELATIONS","log":)" ) !=
         std::string::npos);
  assert(json.find(R"("case_id":"L1")") != std::string::npos);
  assert(json.find(R"("activity":"B")") != std::string::npos);
}

}  // namespace

int main() {
  merges_and_drops();
  parses_relations_and_custom_timestamps();
  rejects_invalid_timestamps();
  json_shape_is_stable();
}
