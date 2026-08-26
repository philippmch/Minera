#include "minera/streaming_footprints.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

namespace {

minera::Footprint model() {
  minera::Footprint fp;
  fp.activities = {"A", "B", "C"};
  fp.start_activities = {"A"};
  fp.end_activities = {"C"};
  fp.sequence.insert({"A", "B"});
  fp.parallel.insert({"B", "C"});
  return fp;
}

void accepts_prefix_and_checks_end() {
  minera::StreamingFootprintsConformance monitor(model());
  auto first = monitor.receive("case-1", "A");
  assert(first.event_index == 1 && first.deviations == 0 && first.is_fit);
  auto second = monitor.receive("case-1", "B");
  assert(second.event_index == 2 && second.deviations == 0 && second.is_fit);
  auto third = monitor.receive("case-1", "C");
  assert(third.event_index == 3 && third.deviations == 0 && third.is_fit);
  assert(monitor.terminate_case("case-1"));
  assert(monitor.snapshot().cases.empty());
}

void records_start_pair_activity_and_end_deviations() {
  minera::StreamingFootprintsConformance monitor(model());
  assert(monitor.receive("bad", "B").deviations == 1);
  assert(monitor.receive("bad", "A").deviations == 1);
  assert(monitor.receive("bad", "X").deviations == 1);
  const auto snapshot = monitor.snapshot();
  assert(snapshot.total_events_processed == 3);
  assert(snapshot.total_deviations == 3);
  assert(snapshot.cases.at("bad").deviations == 3);
  assert(!snapshot.cases.at("bad").is_fit);
  assert(!monitor.terminate_case("bad"));
}

void reset_and_serialization_are_deterministic() {
  minera::StreamingFootprintsConformance monitor(model());
  (void)monitor.receive("z", "A");
  (void)monitor.receive("a", "A");
  const auto snapshot = monitor.snapshot();
  const auto json = minera::serialize_streaming_footprints_json(snapshot);
  assert(json.find("STREAMING_FOOTPRINTS") != std::string::npos);
  assert(json.find("\"case_id\":\"a\"") <
         json.find("\"case_id\":\"z\""));
  monitor.reset_case("a");
  assert(monitor.snapshot().cases.size() == 1);
  monitor.reset_all();
  assert(monitor.snapshot().total_events_processed == 0);
}

void empty_events_and_options_are_rejected() {
  minera::StreamingFootprintsConformance monitor(model());
  assert(monitor.receive("", "A").event_index == 0);
  assert(monitor.receive("c", "").event_index == 0);
  bool rejected = false;
  try {
    minera::StreamingFootprintsConformance invalid(
        model(), {"", "case_id"});
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

}  // namespace

int main() {
  accepts_prefix_and_checks_end();
  records_start_pair_activity_and_end_deviations();
  reset_and_serialization_are_deterministic();
  empty_events_and_options_are_rejected();
  return 0;
}
