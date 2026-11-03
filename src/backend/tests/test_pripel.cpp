#include "minera/pripel.hpp"
#include "minera/json_export.hpp"
#include "minera/xes_export.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <cassert>
#include <algorithm>
#include <cmath>
#include <sstream>
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

minera::EventLog matching_source() {
  minera::EventLog log;
  log.traces.push_back({"source-ab", {{"source-ab", "A", "", "r1", 0, {}, ""},
                                      {"source-ab", "B", "", "r1", 1, {}, ""}}});
  log.traces.push_back({"source-acd", {{"source-acd", "A", "", "r2", 0, {}, ""},
                                       {"source-acd", "C", "", "r2", 1, {}, ""},
                                       {"source-acd", "D", "", "r2", 2, {}, ""}}});
  return log;
}

minera::EventLog matching_query() {
  minera::EventLog log;
  // The first query is closer to source-ab in isolation. A one-to-one
  // assignment must reserve source-ab for the exact second query and use
  // source-acd for this trace instead.
  log.traces.push_back({"query-abc", {{"query-abc", "A", "", "", 0, {}, ""},
                                      {"query-abc", "B", "", "", 1, {}, ""},
                                      {"query-abc", "C", "", "", 2, {}, ""}}});
  log.traces.push_back({"query-ab", {{"query-ab", "A", "", "", 0, {}, ""},
                                     {"query-ab", "B", "", "", 1, {}, ""}}});
  return log;
}

void matching_is_one_to_one() {
  minera::PripelOptions options;
  options.epsilon = 100.0;
  options.seed = 13;
  options.max_distance = 3;
  const auto result =
      minera::anonymize_pripel(matching_source(), matching_query(), options);
  assert(result.diagnostics.exact_matches == 1);
  assert(result.diagnostics.nearest_matches == 1);
  assert(result.diagnostics.synthesized_events == 1);
  assert(result.log.trace_count() == 2);
  // Query order is intentional: the globally optimal assignment gives the
  // longer first query to r2 and the exact second query to r1.
  assert(result.log.traces[0].events[0].resource == "r2");
  assert(result.log.traces[0].events[2].resource == "r2");
  assert(result.log.traces[1].events[0].resource == "r1");
}

void noisy_integer_values_survive_xes() {
  auto input = source();
  input.traces[1].events[1].attributes.push_back({"amount", "20", AttributeKind::kInt});
  input.traces[0].events[1].attributes.push_back({"approved", "true", AttributeKind::kBoolean});
  input.traces[1].events[1].attributes.push_back({"approved", "false", AttributeKind::kBoolean});
  // Exercise imported native scalars, not just manually constructed lexical
  // values: the exporter gives these scalars precedence over the text field.
  std::istringstream source_xml(minera::serialize_xes(input));
  input = minera::import_xes_event_log(source_xml);
  bool saw_fractional_value = false;
  for (std::uint64_t seed = 0; seed < 32; ++seed) {
    minera::PripelOptions options;
    options.seed = seed;
    const auto released = minera::anonymize_pripel(input, input, options);
    std::istringstream xml(minera::serialize_xes(released.log));
    const auto reopened = minera::import_xes_event_log(xml);
    for (std::size_t ti = 0; ti < released.log.traces.size(); ++ti) {
      const auto& before_attrs = released.log.traces[ti].events[1].attributes;
      const auto& after_attrs = reopened.traces[ti].events[1].attributes;
      const auto amount = [](const auto& attr) { return attr.key == "amount"; };
      const auto before = std::find_if(before_attrs.begin(), before_attrs.end(), amount);
      const auto after = std::find_if(after_attrs.begin(), after_attrs.end(), amount);
      assert(before != before_attrs.end() && after != after_attrs.end());
      const auto value = std::stod(before->value);
      assert(std::stod(after->value) == value);
      if (std::trunc(value) != value) {
        saw_fractional_value = true;
        assert(before->kind == AttributeKind::kFloat);
        assert(after->kind == AttributeKind::kFloat);
      }
      assert(*released.log.traces[ti].events[1].attribute("approved") ==
             *reopened.traces[ti].events[1].attribute("approved"));
    }
  }
  assert(saw_fractional_value);
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
  options.secure_random = true;
  const auto secure = minera::anonymize_pripel(source(), query(), options);
  assert(secure.diagnostics.secure_random);
  assert(secure.diagnostics.seed == 0);
  assert(secure.log.trace_count() <= options.max_output_traces);
  options.secure_random = false;
  options.epsilon = 0.0;
  bool threw = false;
  try { (void)minera::anonymize_pripel(source(), query(), options); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  auto malformed = result;
  ++malformed.diagnostics.released_trace_count;
  threw = false;
  try { (void)minera::pripel_json(malformed); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  matching_is_one_to_one();
  noisy_integer_values_survive_xes();
  return 0;
}
