#include "minera/segment_filter.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
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

}  // namespace

int main() {
  minera::EventLog log;
  log.attributes.push_back({"source", "unit", minera::AttributeKind::kString});
  auto first = trace("case", {"a", "b", "a", "c", "b"});
  first.attributes.push_back({"owner", "alice", minera::AttributeKind::kString});
  log.traces.push_back(first);
  log.traces.push_back(trace("missing", {"x", "y"}));

  const auto prefix = minera::filter_prefix(log, "a", false);
  assert(prefix.trace_count() == 1);
  assert((names(prefix.traces[0]) == std::vector<std::string>{"a"}));
  const auto prefix_strict = minera::filter_prefix(log, "a", true);
  assert(prefix_strict.trace_count() == 1 && prefix_strict.traces[0].events.empty());
  minera::PrefixFilterOptions last_prefix;
  last_prefix.activity = "a";
  last_prefix.strict = false;
  last_prefix.occurrence = minera::AnchorOccurrence::last;
  assert((names(minera::filter_prefix(log, last_prefix).traces[0]) ==
          std::vector<std::string>{"a", "b", "a"}));

  const auto suffix = minera::filter_suffix(log, "b", false);
  assert(suffix.trace_count() == 1);
  assert((names(suffix.traces[0]) == std::vector<std::string>{"a", "c", "b"}));
  const auto suffix_strict = minera::filter_suffix(log, "b", true);
  assert((names(suffix_strict.traces[0]) == std::vector<std::string>{"b", "a", "c", "b"}));

  minera::SuffixFilterOptions last_suffix;
  last_suffix.activity = "b";
  last_suffix.occurrence = minera::AnchorOccurrence::last;
  last_suffix.strict = false;
  const auto last_cut = minera::filter_suffix(log, last_suffix);
  assert(last_cut.trace_count() == 1 && last_cut.traces[0].events.empty());

  minera::BetweenFilterOptions between;
  between.start_activities = {"a"};
  between.end_activities = {"b"};
  between.subcase_separator = "~";
  const auto segments = minera::filter_between(log, between);
  assert(segments.trace_count() == 2);
  assert(segments.traces[0].case_id == "case~1");
  assert(segments.traces[1].case_id == "case~2");
  assert((names(segments.traces[0]) == std::vector<std::string>{"a", "b"}));
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
  assert((names(listed.traces[1]) == std::vector<std::string>{"start", "y", "close"}));

  minera::BetweenFilterOptions equal;
  equal.start_activities = {"a"};
  equal.end_activities = {"a"};
  minera::EventLog equal_log;
  equal_log.traces.push_back(trace("e", {"a", "x", "a", "y", "a"}));
  const auto equal_result = minera::filter_between(equal_log, equal);
  assert(equal_result.trace_count() == 2);
  assert((names(equal_result.traces[0]) == std::vector<std::string>{"a", "x", "a"}));
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
