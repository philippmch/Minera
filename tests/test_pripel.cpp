#include "minera/pripel.hpp"

#include <cassert>
#include <stdexcept>

using minera::AttributeKind;
using minera::Event;
using minera::EventAttribute;

namespace {
minera::EventLog source() {
  minera::EventLog log;
  log.traces.push_back({"secret-a", {{"secret-a", "A", "2020-01-01T00:00:00Z", "r1", 0,
                                       {{"keep", "one", AttributeKind::kString},
                                        {"secret", "x", AttributeKind::kString}}, ""},
                                      {"secret-a", "B", "2020-01-01T01:00:00Z", "r2", 1,
                                       {{"amount", "10", AttributeKind::kInt}}, ""}}});
  log.traces.push_back({"secret-b", {{"secret-b", "A", "2020-01-02T00:00:00Z", "r3", 0, {}, ""},
                                      {"secret-b", "C", "2020-01-02T01:00:00Z", "r4", 1, {}, ""}}});
  return log;
}

minera::EventLog query() {
  minera::EventLog q;
  q.traces.push_back({"released-id", {{"released-id", "A", "", "", 0, {}, ""},
                                       {"released-id", "B", "", "", 1, {}, ""}}});
  q.traces.push_back({"released-id-2", {{"released-id-2", "A", "", "", 0, {}, ""},
                                         {"released-id-2", "D", "", "", 1, {}, ""}}});
  return q;
}
}

int main() {
  minera::PripelOptions options;
  options.epsilon = 100.0;
  options.seed = 7;
  options.max_distance = 1;
  options.blocklist = {"secret"};
  const auto result = minera::anonymize_pripel(source(), query(), options);
  assert(result.log.trace_count() == 2);
  assert(result.diagnostics.exact_matches == 1);
  assert(result.diagnostics.nearest_matches == 1);
  assert(result.log.traces[0].case_id != "released-id");
  assert(result.log.traces[0].events[0].activity == "A");
  assert(result.log.traces[0].events[0].attribute("secret") == nullptr);
  assert(result.log.traces[0].events[0].timestamp <= result.log.traces[0].events[1].timestamp);
  const auto repeat = minera::anonymize_pripel(source(), query(), options);
  assert(repeat.log.traces[0].events[0].timestamp == result.log.traces[0].events[0].timestamp);
  options.max_output_traces = 1;
  assert(minera::anonymize_pripel(source(), query(), options).log.trace_count() == 1);
  options.max_output_traces = 100;
  options.epsilon = 0.0;
  bool threw = false;
  try { (void)minera::anonymize_pripel(source(), query(), options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  return 0;
}
