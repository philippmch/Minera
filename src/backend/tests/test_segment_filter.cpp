#include "minera/segment_filter.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {

minera::Trace trace(std::string id, std::initializer_list<const char*> names) {
  minera::Trace result;
  result.case_id = std::move(id);
  std::size_t order = 0;
  for (const auto* name : names) {
    result.events.push_back(minera::Event{result.case_id, name, "", "", order++});
  }
  return result;
}

std::vector<std::string> names(const minera::Trace& trace) {
  std::vector<std::string> result;
  for (const auto& event : trace.events) result.push_back(event.activity);
  return result;
}

void between_updates_derived_case_ids_without_stale_typed_payloads() {
  minera::EventLog log;
  auto source = trace("42", {"a", "x", "a", "y", "a", "b"});
  source.case_id_state = minera::FirstClassAttributeState::kPresent;
  source.case_id_source_key = "case.number";
  minera::EventAttribute selected{"case.number", "42",
                                 minera::AttributeKind::kInt};
  selected.typed_value = std::int64_t{42};
  source.case_id_attribute = selected;
  log.traces.push_back(std::move(source));

  minera::BetweenFilterOptions options;
  options.start_activities = {"a"};
  options.end_activities = {"b"};
  options.subcase_separator = "~";
  // pm4py 2.7.22's DataFrame filter_between, captured from the same case:
  // a->b on a,x,a,y,a,b gives one segment 42##@@0 spanning the case (the
  // later a's do not reopen); a->a gives 42##@@0 [a,x] and 42##@@1 [a,y,a].
  // Every segment is labelled, so the integer payload becomes a string.
  const auto result = minera::filter_between(log, options);
  assert(result.traces.size() == 1);
  const auto& segment = result.traces.front();
  assert(segment.case_id == "42~0");
  assert(segment.case_id_source_key == "case.number");
  assert(segment.case_id_attribute &&
         segment.case_id_attribute->value == "42~0" &&
         segment.case_id_attribute->kind == minera::AttributeKind::kString &&
         std::holds_alternative<std::monostate>(
             segment.case_id_attribute->typed_value));
  assert((names(segment) ==
          std::vector<std::string>{"a", "x", "a", "y", "a", "b"}));

  options.end_activities = {"a"};
  const auto split = minera::filter_between(log, options);
  assert(split.traces.size() == 2);
  assert((names(split.traces[0]) == std::vector<std::string>{"a", "x"}));
  assert((names(split.traces[1]) == std::vector<std::string>{"a", "y", "a"}));
  for (std::size_t i = 0; i < split.traces.size(); ++i) {
    const auto& derived = split.traces[i];
    assert(derived.case_id == "42~" + std::to_string(i));
    assert(derived.case_id_state ==
           minera::FirstClassAttributeState::kPresent);
    assert(derived.case_id_source_key == "case.number");
    assert(!derived.events.empty() &&
           derived.events.front().case_id == derived.case_id &&
           derived.events.front().case_id_source_key == "case.number");
    assert(derived.case_id_attribute &&
           derived.case_id_attribute->value == derived.case_id &&
           derived.case_id_attribute->kind == minera::AttributeKind::kString &&
           std::holds_alternative<std::monostate>(
               derived.case_id_attribute->typed_value));
  }
}

}  // namespace

int main() {
  between_updates_derived_case_ids_without_stale_typed_payloads();
  minera::EventLog log;
  log.attributes.push_back({"source", "unit", minera::AttributeKind::kString});
  auto first = trace("case", {"a", "b", "a", "c", "b"});
  first.attributes.push_back({"owner", "alice", minera::AttributeKind::kString});
  log.traces.push_back(first);
  log.traces.push_back(trace("missing", {"x", "y"}));

  const auto prefix = minera::filter_prefix(log, "a", false);
  assert(prefix.trace_count() == 1);
  assert((names(prefix.traces[0]) == std::vector<std::string>{"a"}));
  // A cut that keeps no events drops the case, as pm4py's DataFrame
  // filter_prefixes/filter_suffixes leave no rows for it.
  const auto prefix_strict = minera::filter_prefix(log, "a", true);
  assert(prefix_strict.trace_count() == 0);
  minera::PrefixFilterOptions last_prefix;
  last_prefix.activity = "a";
  last_prefix.strict = false;
  last_prefix.occurrence = minera::AnchorOccurrence::last;
  assert((names(minera::filter_prefix(log, last_prefix).traces[0]) ==
          std::vector<std::string>{"a", "b", "a"}));

  // As pm4py's filter_suffixes: strict leaves the anchor out.
  const auto suffix = minera::filter_suffix(log, "b", false);
  assert(suffix.trace_count() == 1);
  assert((names(suffix.traces[0]) == std::vector<std::string>{"b", "a", "c", "b"}));
  const auto suffix_strict = minera::filter_suffix(log, "b", true);
  assert((names(suffix_strict.traces[0]) == std::vector<std::string>{"a", "c", "b"}));

  minera::SuffixFilterOptions last_suffix;
  last_suffix.activity = "b";
  last_suffix.occurrence = minera::AnchorOccurrence::last;
  const auto last_cut = minera::filter_suffix(log, last_suffix);
  assert(last_cut.trace_count() == 0);
  last_suffix.strict = false;
  const auto last_kept = minera::filter_suffix(log, last_suffix);
  assert((names(last_kept.traces[0]) == std::vector<std::string>{"b"}));

  minera::BetweenFilterOptions between;
  between.start_activities = {"a"};
  between.end_activities = {"b"};
  between.subcase_separator = "~";
  const auto segments = minera::filter_between(log, between);
  assert(segments.trace_count() == 2);
  assert(segments.traces[0].case_id == "case~0");
  assert(segments.traces[1].case_id == "case~1");
  assert((names(segments.traces[0]) == std::vector<std::string>{"a", "b"}));
  assert((names(segments.traces[1]) == std::vector<std::string>{"a", "c", "b"}));
  assert(*segments.traces[0].attribute("owner") == "alice");
  assert(segments.attributes.size() == log.attributes.size());
  assert(segments.attributes[0].key == log.attributes[0].key &&
         segments.attributes[0].value == log.attributes[0].value);

  minera::BetweenFilterOptions list_between;
  list_between.start_activities = {"start", "open"};
  list_between.end_activities = {"stop", "close"};
  minera::EventLog list_log;
  list_log.traces.push_back(trace("l", {"open", "x", "stop", "start", "y", "close"}));
  const auto listed = minera::filter_between(list_log, list_between);
  assert(listed.trace_count() == 2);
  assert((names(listed.traces[0]) == std::vector<std::string>{"open", "x", "stop"}));
  assert((names(listed.traces[1]) == std::vector<std::string>{"start", "y", "close"}));
  assert(listed.traces[0].case_id == "l##@@0" && listed.traces[1].case_id == "l##@@1");

  // Only equal start and end lists reopen at the closing event: with
  // [a,b]->[b] the reference cuts a,b,a,b into [a,b] and [a,b].
  minera::BetweenFilterOptions overlapping;
  overlapping.start_activities = {"a", "b"};
  overlapping.end_activities = {"b"};
  minera::EventLog overlap_log;
  overlap_log.traces.push_back(trace("e", {"a", "b", "a", "b"}));
  const auto overlap = minera::filter_between(overlap_log, overlapping);
  assert(overlap.trace_count() == 2);
  assert((names(overlap.traces[0]) == std::vector<std::string>{"a", "b"}));
  assert((names(overlap.traces[1]) == std::vector<std::string>{"a", "b"}));

  minera::BetweenFilterOptions equal;
  equal.start_activities = {"a"};
  equal.end_activities = {"a"};
  minera::EventLog equal_log;
  equal_log.traces.push_back(trace("e", {"a", "x", "a", "y", "a"}));
  const auto equal_result = minera::filter_between(equal_log, equal);
  // As in a DataFrame, where a row carries one label, the shared a belongs
  // to the later segment only.
  assert(equal_result.trace_count() == 2);
  assert((names(equal_result.traces[0]) == std::vector<std::string>{"a", "x"}));
  assert((names(equal_result.traces[1]) == std::vector<std::string>{"a", "y", "a"}));

  minera::EventLog adjacent_equal;
  adjacent_equal.traces.push_back(trace("adjacent", {"a", "a"}));
  const auto adjacent_result = minera::filter_between(adjacent_equal, equal);
  assert(adjacent_result.trace_count() == 1);
  assert((names(adjacent_result.traces[0]) ==
          std::vector<std::string>{"a", "a"}));

  bool threw = false;
  try {
    (void)minera::filter_between(log, minera::BetweenFilterOptions{});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)minera::filter_prefix(log, minera::PrefixFilterOptions{});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  return 0;
}
