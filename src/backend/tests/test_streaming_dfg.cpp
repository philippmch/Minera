#include "minera/streaming_dfg.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

int main() {
  minera::StreamingDfgDiscovery stream;
  assert(!stream.receive("", "A"));
  assert(!stream.receive("c1", ""));
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
