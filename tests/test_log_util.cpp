#include "minera/log_util.hpp"
#include "minera/log_util_json.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

using namespace minera;

namespace {

Event event(std::string case_id, std::string activity, std::string timestamp,
            std::size_t input_order) {
  Event result;
  result.case_id = std::move(case_id);
  result.activity = std::move(activity);
  result.timestamp = std::move(timestamp);
  result.input_order = input_order;
  return result;
}

EventLog source_log() {
  EventLog log;
  log.attributes.push_back({"source", "fixture", AttributeKind::kString});
  Trace first;
  first.case_id = "c1";
  first.attributes.push_back({"segment", "one", AttributeKind::kString});
  first.events = {event("c1", "A", "1", 0), event("c1", "B", "2", 1),
                 event("c1", "C", "3", 2)};
  Trace second;
  second.case_id = "c2";
  second.events = {event("c2", "A", "10", 3), event("c2", "B", "12", 4)};
  log.traces = {first, second};
  return log;
}

void split_is_seeded_and_preserves_metadata() {
  const auto log = source_log();
  const auto left = split_log_train_test(log, 0.5, 42);
  const auto right = split_log_train_test(log, 0.5, 42);
  assert(left.training.traces.size() == 2);
  assert(left.testing.traces.empty());
  assert(left.training.traces[0].case_id == right.training.traces[0].case_id);
  assert(left.training.attributes.size() == 1);
  assert(left.training.attributes[0].key == "source");
  const auto original = std::find_if(
      log.traces.begin(), log.traces.end(), [&](const auto& trace) {
        return trace.case_id == left.training.traces[0].case_id;
      });
  assert(original != log.traces.end());
  assert(left.training.traces[0].attributes.size() ==
         original->attributes.size());
  bool rejected = false;
  try {
    (void)split_log_train_test(log, 1.1, 0);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void fixed_and_all_prefixes_match_contract() {
  const auto log = source_log();
  const auto fixed = get_log_prefixes(log, 2);
  assert(fixed.traces.size() == 2);
  assert(fixed.traces[0].events.size() == 2);
  assert(fixed.traces[1].events.size() == 2);
  const auto all = get_log_with_prefixes(log);
  assert(all.log.traces.size() == 10);  // two entries around each event
  assert(all.change_indexes.size() == 2);
  assert((all.change_indexes[0] == std::vector<std::size_t>{5, 5, 5}));
  assert((all.change_indexes[1] == std::vector<std::size_t>{9, 9}));
  const auto json = log_prefix_json(all.log, LogUtilityVariant::kAllPrefixes,
                                    all.change_indexes);
  assert(json.find("\"family\":\"LOG_UTIL\"") != std::string::npos);
  assert(json.find("\"change_indexes\":[[5,5,5],[9,9]]") != std::string::npos);
}

void until_activity_returns_prefix_and_duration() {
  const auto log = source_log();
  LogUtilityOptions options;
  options.activity_key = "activity";
  options.timestamp_key = "timestamp";
  const auto result = get_log_traces_until_activity(log, "C", options);
  assert(result.log.traces.size() == 1);
  assert(result.log.traces[0].case_id == "c1");
  assert(result.log.traces[0].events.size() == 2);
  assert(result.elapsed_seconds.size() == 1);
  assert(std::abs(result.elapsed_seconds[0] - 1.0) < 1e-12);
  const auto json = log_until_activity_json(result);
  assert(json.find("\"elapsed_seconds\":[1]") != std::string::npos);
  const auto split_json = log_train_test_json(split_log_train_test(log, 0.5, 1));
  assert(split_json.find("\"training\"") != std::string::npos);
  assert(split_json.find("\"testing\"") != std::string::npos);
}

}  // namespace

int main() {
  split_is_seeded_and_preserves_metadata();
  fixed_and_all_prefixes_match_contract();
  until_activity_returns_prefix_and_duration();
  std::cout << "Log utility tests passed\n";
}
