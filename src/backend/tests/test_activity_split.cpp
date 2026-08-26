#include "minera/activity_split.hpp"
#include "minera/log_filter.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

minera::EventLog sample_log() {
  minera::EventLog log;
  log.attributes.push_back({"source", "fixture", minera::AttributeKind::kString});
  minera::Trace trace;
  trace.case_id = "case-1";
  trace.attributes.push_back({"region", "north", minera::AttributeKind::kString});
  trace.events = {
      {"case-1", "P", "", "r0", 0, {}, ""},
      {"case-1", "A", "", "r1", 1, {{"n", "0", minera::AttributeKind::kInt}}, ""},
      {"case-1", "X", "", "r2", 2, {}, ""},
      {"case-1", "A", "", "r1", 3, {}, ""},
      {"case-1", "B", "", "r3", 4, {}, ""},
  };
  log.traces.push_back(std::move(trace));
  return log;
}

std::vector<std::string> activities(const minera::Trace& trace) {
  std::vector<std::string> result;
  for (const auto& event : trace.events) result.push_back(event.activity);
  return result;
}

}  // namespace

int main() {
  const auto log = sample_log();

  minera::ActivitySplitOptions this_options;
  this_options.activities = {"A"};
  const auto this_split = minera::split_activity(log, this_options);
  assert(this_split.traces.size() == 3);
  assert(activities(this_split.traces[0]) == std::vector<std::string>{"P"});
  assert(activities(this_split.traces[1]) ==
         std::vector<std::string>({"A", "X"}));
  assert(activities(this_split.traces[2]) ==
         std::vector<std::string>({"A", "B"}));
  assert(this_split.traces[0].case_id == "case-1##@@0");
  assert(this_split.traces[1].case_id == "case-1##@@1");
  assert(this_split.traces[2].case_id == "case-1##@@2");
  assert(this_split.traces[0].events[0].case_id ==
         this_split.traces[0].case_id);
  assert(this_split.traces[1].events[0].attribute("n") != nullptr);
  assert(this_split.traces[0].attributes[0].value == "north");
  assert(this_split.attributes[0].value == "fixture");

  minera::ActivitySplitOptions next_options;
  next_options.activities = {"A"};
  next_options.cut_mode = minera::ActivitySplitCutMode::kNext;
  const auto next_split = minera::filter_activity_split(log, next_options);
  assert(next_split.traces.size() == 3);
  assert(activities(next_split.traces[0]) ==
         std::vector<std::string>({"P", "A"}));
  assert(activities(next_split.traces[1]) ==
         std::vector<std::string>({"X", "A"}));
  assert(activities(next_split.traces[2]) ==
         std::vector<std::string>({"B"}));
  assert(next_split.traces[0].case_id == "case-1##@@0");
  assert(next_split.traces[1].case_id == "case-1##@@1");
  assert(next_split.traces[2].case_id == "case-1##@@2");

  minera::ActivitySplitOptions no_match;
  no_match.activities = {"Z"};
  const auto unchanged = minera::split_event_log_on_activity(log, no_match);
  assert(unchanged.traces.size() == 1);
  assert(unchanged.traces[0].case_id == "case-1##@@0");
  assert(activities(unchanged.traces[0]) ==
         std::vector<std::string>({"P", "A", "X", "A", "B"}));

  auto multi = minera::make_activity_split_options(
      {"X", "B"}, minera::ActivitySplitCutMode::kThis, "::");
  const auto multi_split = minera::split_activity(log, multi);
  assert(multi_split.traces.size() == 3);
  assert(multi_split.traces[0].case_id == "case-1::0");
  assert(activities(multi_split.traces[1]) ==
         std::vector<std::string>({"X", "A"}));
  assert(activities(multi_split.traces[2]) ==
         std::vector<std::string>({"B"}));

  minera::LogFilterOptions integrated;
  integrated.activity_split_activities = {"A"};
  integrated.activity_split_cut_mode = minera::ActivitySplitCutMode::kNext;
  const auto integrated_split = minera::filter_event_log(log, integrated);
  assert(integrated_split.traces.size() == 3);
  assert(integrated_split.traces[1].case_id == "case-1##@@1");
  assert(activities(integrated_split.traces[1]) ==
         std::vector<std::string>({"X", "A"}));

  for (const auto& invalid : std::vector<minera::ActivitySplitOptions>{
           {},
           minera::ActivitySplitOptions{{"A"}, "", minera::ActivitySplitCutMode::kThis},
           minera::ActivitySplitOptions{{""}, "##@@", minera::ActivitySplitCutMode::kThis}}) {
    bool threw = false;
    try {
      (void)minera::split_activity(log, invalid);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
  return 0;
}
