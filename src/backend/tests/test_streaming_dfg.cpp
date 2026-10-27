#include "minera/streaming_dfg.hpp"

#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>

int main() {
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
