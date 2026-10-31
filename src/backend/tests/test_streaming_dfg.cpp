#include "minera/streaming_dfg.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <cassert>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

// One-shot streaming routes feed every parsed event whose case id the
// monitors take. PM4Py 2.7.22 on the XES event stream of this log (the
// second trace has no concept:name) discovers {(A,D):1} with activities
// {A:1,D:1}; CSV ids are always present, an empty one included.
void events_without_case_ids_are_not_taken() {
  std::istringstream xes(R"(<log xes.version="1.0">
<trace><string key="concept:name" value="c"/>
<event><string key="concept:name" value="A"/></event><event><string key="concept:name" value="D"/></event></trace>
<trace><event><string key="concept:name" value="B"/></event><event><string key="concept:name" value="C"/></event></trace></log>)");
  const auto log = minera::import_xes_event_log(xes);
  minera::StreamingDfgDiscovery stream({"concept:name", "case:concept:name"});
  std::size_t taken = 0;
  for (const auto& trace : log.traces)
    for (const auto& event : trace.events)
      if (minera::streaming_monitor_takes_event(event)) ++taken, stream.receive(event, event.case_id);
  assert(taken == 2);
  const auto shot = stream.snapshot();
  assert(shot.events == 2 && shot.activities.size() == 2 && shot.activities.at("A") == 1);
  assert(shot.graph.size() == 1 && shot.graph.begin()->second == 1);

  std::istringstream csv("case_id,activity\nc,A\n");
  const auto csv_log = minera::import_csv_event_log(csv);
  assert(minera::streaming_monitor_takes_event(csv_log.traces.at(0).events.at(0)));
  minera::Event empty_present;
  empty_present.case_id_state = minera::FirstClassAttributeState::kPresent;
  assert(minera::streaming_monitor_takes_event(empty_present));
  minera::Event null_id;
  null_id.case_id_state = minera::FirstClassAttributeState::kNull;
  assert(minera::streaming_monitor_takes_event(null_id));
  assert(!minera::streaming_monitor_takes_event(minera::Event{}));
}

}  // namespace

int main() {
  events_without_case_ids_are_not_taken();
  // Reference streaming DFG: empty labels and case ids are ordinary values.
  // Arrival -> "" -> Review gives Arrival->"" and ""->Review, never an
  // invented Arrival->Review shortcut.
  {
    minera::StreamingDfgDiscovery empty_values;
    assert(empty_values.receive("c", "Arrival"));
    assert(empty_values.receive("c", ""));
    assert(empty_values.receive("c", "Review"));
    assert(empty_values.receive("", "A"));
    assert(empty_values.receive("", "B"));
    const auto shot = empty_values.snapshot();
    assert(shot.events == 5);
    assert(shot.graph.size() == 3);
    assert(shot.graph.at({"Arrival", ""}) == 1);
    assert(shot.graph.at({"", "Review"}) == 1);
    assert(shot.graph.at({"A", "B"}) == 1);
    assert(shot.activities.at("") == 1);
    assert(shot.open_cases.at("") == "B");
  }
  // The selected activity attribute wins over the canonical name; an event
  // without the key is skipped and a present null reads "None".
  {
    const auto event = [](std::string canonical, std::optional<std::string> picked) {
      minera::Event result;
      result.case_id = "c";
      result.activity = std::move(canonical);
      if (picked) result.attributes.push_back({"picked", *picked, minera::AttributeKind::kString});
      return result;
    };
    minera::StreamingDfgDiscovery picked({"picked", "case_id"});
    assert(picked.receive(event("OldOne", "SelectedOne"), "c"));
    assert(picked.receive(event("OldTwo", "SelectedTwo"), "c"));
    assert(!picked.receive(event("OldThree", std::nullopt), "c"));
    auto null_pick = event("OldFour", "");
    null_pick.attributes.back().is_null = true;
    assert(picked.receive(null_pick, "c"));
    const auto shot = picked.snapshot();
    assert(shot.events == 3);
    assert(shot.graph.at({"SelectedOne", "SelectedTwo"}) == 1);
    assert(shot.graph.at({"SelectedTwo", "None"}) == 1);
    assert(!shot.activities.contains("OldOne"));
  }

  minera::StreamingDfgDiscovery stream;
  assert(stream.receive("c1", "A"));
  assert(stream.receive("c2", "A"));
  assert(stream.receive("c1", "B"));
  assert(stream.receive("c1", "C"));
  assert(stream.receive("c2", "B"));

  const auto snapshot = stream.snapshot();
  assert(snapshot.events == 5);
  assert(snapshot.activities.at("A") == 2);
  assert(snapshot.activities.at("B") == 2);
  assert(snapshot.activities.at("C") == 1);
  assert(snapshot.start_activities.at("A") == 2);
  assert(snapshot.graph.at({"A", "B"}) == 2);
  assert(snapshot.graph.at({"B", "C"}) == 1);
  assert(snapshot.end_activities.at("B") == 1);
  assert(snapshot.end_activities.at("C") == 1);
  assert(snapshot.open_cases.at("c1") == "C");
  assert(snapshot.open_cases.at("c2") == "B");

  const auto json = minera::serialize_streaming_dfg_json(snapshot);
  assert(json.find("\"family\":\"STREAMING_DFG\"") !=
         std::string::npos);
  assert(json.find("\"source\":\"A\"") != std::string::npos);

  stream.reset();
  assert(stream.snapshot().events == 0);
  bool rejected = false;
  try {
    minera::StreamingDfgDiscovery invalid({"", "case_id"});
    (void)invalid;
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  return 0;
}
