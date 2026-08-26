#include "minera/attribute_value_repetition.hpp"
#include "minera/log_filter.hpp"

#include <cassert>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

minera::EventLog sample_log() {
  minera::EventLog log;
  log.attributes.push_back({"source", "fixture", minera::AttributeKind::kString});
  minera::Trace repeated;
  repeated.case_id = "r";
  repeated.attributes.push_back({"region", "north", minera::AttributeKind::kString});
  repeated.events = {
      {"r", "A", "", "alice", 0, {{"channel", "web", minera::AttributeKind::kString}}, ""},
      {"r", "A", "", "bob", 1, {{"channel", "web", minera::AttributeKind::kString}}, ""},
      {"r", "r", "", "alice", 2, {{"channel", "fax", minera::AttributeKind::kString}}, ""},
  };
  minera::Trace singleton;
  singleton.case_id = "s";
  singleton.events = {
      {"s", "A", "", "alice", 0, {{"channel", "web", minera::AttributeKind::kString}}, ""},
      {"s", "B", "", "bob", 1, {{"channel", "mail", minera::AttributeKind::kString}}, ""},
  };
  log.traces = {repeated, singleton};
  return log;
}

}  // namespace

int main() {
  const auto log = sample_log();
  minera::AttributeValueRepetitionOptions defaults;
  defaults.value = "A";
  const auto repeated = minera::filter_attribute_value_repetition(log, defaults);
  assert(repeated.traces.size() == 1);
  assert(repeated.traces[0].case_id == "r");
  assert(repeated.attributes[0].value == "fixture");

  minera::AttributeValueRepetitionOptions generic;
  generic.attribute_key = "channel";
  generic.value = "web";
  generic.min_repetitions = 2;
  generic.max_repetitions = 2;
  assert(minera::filter_attr_value_repetition(log, generic).trace_count() == 1);

  minera::AttributeValueRepetitionOptions resource;
  resource.attribute_key = "resource";
  resource.value = "alice";
  resource.min_repetitions = 2;
  assert(minera::filter_event_log_attr_value_repetition(log, resource).trace_count() == 1);

  minera::AttributeValueRepetitionOptions zero;
  zero.value = "missing";
  zero.min_repetitions = 0;
  zero.max_repetitions = 0;
  assert(minera::filter_attribute_value_repetition(log, zero).trace_count() == 2);

  minera::LogFilterOptions integrated;
  integrated.attribute_value_repetition = generic;
  const auto integrated_result = minera::filter_event_log(log, integrated);
  assert(integrated_result.trace_count() == 1);
  assert(integrated_result.traces[0].case_id == "r");

  bool threw = false;
  try {
    generic.min_repetitions = 3;
    generic.max_repetitions = 2;
    (void)minera::filter_attribute_value_repetition(log, generic);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  return 0;
}
