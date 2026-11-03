#include "minera/dfg_playout_input_json.hpp"
#include "minera/timestamp.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {
using namespace minera;

template <class Action>
void rejects(Action action) {
  bool rejected = false;
  try { action(); } catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}

void contains(const std::string& text, std::string_view expected) {
  assert(text.find(expected) != std::string::npos);
}

void empty_and_partial_records_keep_native_defaults() {
  const auto omitted = parse_dfg_playout_input_json("{}");
  assert(omitted.frequency.empty() && omitted.start_activities.empty());
  assert(omitted.end_activities.empty() && !omitted.performance);
  const auto full = serialize_dfg_playout_input_json(omitted);
  assert(full == R"({"frequency":[],"start_activities":[],"end_activities":[],"performance":null})");
  assert(serialize_dfg_playout_input_json(parse_dfg_playout_input_json(full)) == full);
  const auto engaged = parse_dfg_playout_input_json(R"({"performance":[]})");
  assert(engaged.performance && engaged.performance->empty());
  contains(serialize_dfg_playout_input_json(engaged), R"("performance":[])");
  const auto partial = parse_dfg_playout_input_json(
      R"({"frequency":[{"source":"","target":" "}],"start_activities":[{"activity":""}],"end_activities":[{"activity":" "}],"performance":[{"source":"x","target":"y"},{"source":"y","target":"x","statistics":{"summary":{},"raw_values":[],"value":{"index":2,"value":{}}}}]})");
  assert(partial.frequency.at({"", " "}) == 0);
  assert(partial.start_activities.at("") == 0 && partial.end_activities.at(" ") == 0);
  assert(partial.performance->at({"x", "y"}).value.index() == 0);
  assert(std::get<double>(partial.performance->at({"x", "y"}).value) == 0);
  assert(partial.performance->at({"y", "x"}).value.index() == 2);
  assert(std::get<DfgPerformanceSummary>(partial.performance->at({"y", "x"}).value).count == 0);
}

void exact_counts_and_all_stored_performance_fields_round_trip() {
  const auto parsed = parse_dfg_playout_input_json(
      R"({"frequency":[{"source":"A","target":"B","count":18446744073709551615},{"source":"B","target":"A","count":0}],"start_activities":[{"activity":"A","count":9007199254740993}],"end_activities":[{"activity":"B","count":18446744073709551615}],"performance":[{"source":"A","target":"B","statistics":{"summary":{"count":18446744073709551615,"mean":-0.0,"median":"NaN","minimum":"-INF","maximum":"INF","standard_deviation":4.9406564584124654e-324,"sum":1.7976931348623157e308},"raw_values":[-0.0,4.9406564584124654e-324,-1e-9999,"NaN","INF","-INF"],"value":{"index":0,"value":-0.0}}},{"source":"B","target":"C","statistics":{"summary":{"mean":10},"raw_values":[1,2],"value":{"index":1,"value":[3,4,"NaN"]}}},{"source":"C","target":"D","statistics":{"summary":{"count":7,"mean":8,"median":9,"minimum":10,"maximum":11,"standard_deviation":12,"sum":13},"raw_values":[14],"value":{"index":2,"value":{"count":9007199254740993,"mean":15,"median":16,"minimum":17,"maximum":18,"standard_deviation":19,"sum":20}}}}]})");
  assert(parsed.frequency.at({"A", "B"}) == std::numeric_limits<std::uint64_t>::max());
  assert(parsed.frequency.count({"B", "A"}) == 1 && parsed.frequency.at({"B", "A"}) == 0);
  assert(parsed.start_activities.at("A") == UINT64_C(9007199254740993));
  const auto& scalar = parsed.performance->at({"A", "B"});
  assert(scalar.summary.count == std::numeric_limits<std::uint64_t>::max());
  assert(std::signbit(scalar.summary.mean) && std::isnan(scalar.summary.median));
  assert(std::isinf(scalar.summary.minimum) && scalar.summary.minimum < 0);
  assert(std::isinf(scalar.summary.maximum) && scalar.summary.maximum > 0);
  assert(scalar.summary.standard_deviation == std::numeric_limits<double>::denorm_min());
  assert(scalar.summary.sum == std::numeric_limits<double>::max());
  assert(scalar.raw_values.size() == 6 && std::signbit(scalar.raw_values[0]));
  assert(scalar.raw_values[1] == std::numeric_limits<double>::denorm_min());
  assert(scalar.raw_values[2] == 0 && std::signbit(scalar.raw_values[2]));
  assert(std::isnan(scalar.raw_values[3]) && std::isinf(scalar.raw_values[4]));
  assert(std::signbit(std::get<double>(scalar.value)));
  const auto& vector = std::get<std::vector<double>>(parsed.performance->at({"B", "C"}).value);
  assert(vector.size() == 3 && vector[0] == 3 && vector[1] == 4 && std::isnan(vector[2]));
  const auto& summary = std::get<DfgPerformanceSummary>(parsed.performance->at({"C", "D"}).value);
  assert(summary.count == UINT64_C(9007199254740993));
  assert(summary.mean == 15 && summary.median == 16 && summary.minimum == 17);
  assert(summary.maximum == 18 && summary.standard_deviation == 19 && summary.sum == 20);
  const auto json = serialize_dfg_playout_input_json(parsed);
  contains(json, R"("count":18446744073709551615)");
  contains(json, R"("mean":-0.0)");
  contains(json, R"("median":"NaN")");
  assert(serialize_dfg_playout_input_json(parse_dfg_playout_input_json(json)) == json);
}

void literal_labels_do_not_use_an_activity_registry() {
  const auto input = parse_dfg_playout_input_json(
      R"({"frequency":[{"source":"\u0000\ud800","target":" \n\udc00","count":0},{"source":"\ud83d\ude00","target":"","count":1}],"start_activities":[{"activity":"\u0000\ud800","count":1},{"activity":"","count":0}],"end_activities":[{"activity":" \n\udc00","count":1}],"performance":[{"source":"unrelated","target":"\ud800"}]})");
  const std::string first("\0\xed\xa0\x80", 4);
  const std::string second(" \n\xed\xb0\x80", 5);
  assert(input.frequency.at({first, second}) == 0);
  assert(input.frequency.at({"\xf0\x9f\x98\x80", ""}) == 1);
  assert(input.start_activities.at(first) == 1 && input.start_activities.count("") == 1);
  assert(input.performance->count({"unrelated", "\xed\xa0\x80"}) == 1);
  const auto json = serialize_dfg_playout_input_json(input);
  contains(json, R"("source":"\u0000\ud800")");
  contains(json, R"("target":" \n\udc00")");
  assert(serialize_dfg_playout_input_json(parse_dfg_playout_input_json(json)) == json);
  auto malformed = input;
  malformed.start_activities[std::string("\xc0\x80", 2)] = 1;
  rejects([&] { (void)serialize_dfg_playout_input_json(malformed); });
}

void invalid_transport_is_rejected_without_silent_map_merging() {
  const char* invalid[] = {
    "null", "[]", "{} trailing", R"({"extra":0})", R"({"frequency":null})",
    R"({"performance":{}})", R"({"start_activities":false})",
    R"({"frequency":[],"frequ\u0065ncy":[]})",
    R"({"frequency":[{"source":"A","source":"B","target":"C"}]})",
    R"({"frequency":[{"source":"A","target":"B","extra":0}]})",
    R"({"frequency":[{"target":"B","count":1}]})",
    R"({"frequency":[{"source":"A","target":null}]})",
    R"({"frequency":[{"source":"A","target":"B","count":18446744073709551616}]})",
    R"({"frequency":[{"source":"A","target":"B","count":-1}]})",
    R"({"frequency":[{"source":"A","target":"B","count":1.0}]})",
    R"({"frequency":[{"source":"A","target":"B","count":1e0}]})",
    R"({"frequency":[{"source":"A","target":"B","count":"1"}]})",
    R"({"frequency":[{"source":"A","target":"B","count":true}]})",
    R"({"frequency":[{"source":"A","target":"B"},{"source":"\u0041","target":"B"}]})",
    R"({"start_activities":[{"activity":"A"},{"activity":"\u0041"}]})",
    R"({"end_activities":[{"activity":"A"},{"activity":"A"}]})",
    R"({"start_activities":[{"count":1}]})",
    R"({"performance":[{"source":"A","target":"B"},{"source":"A","target":"B"}]})",
    R"({"performance":[{"source":"A","target":"B","statistics":null}]})",
    R"({"performance":[{"source":"A","target":"B","statistics":{"summary":{"extra":0}}}]})",
    R"({"performance":[{"source":"A","target":"B","statistics":{"summary":{"count":-0}}}]})",
    R"({"performance":[{"source":"A","target":"B","statistics":{"summary":{"mean":"nan"}}}]})",
    R"({"performance":[{"source":"A","target":"B","statistics":{"summary":{"mean":1e9999}}}]})",
    R"({"performance":[{"source":"A","target":"B","statistics":{"raw_values":[null]}}]})",
    R"({"performance":[{"source":"A","target":"B","statistics":{"value":{"index":3,"value":0}}}]})",
    R"({"performance":[{"source":"A","target":"B","statistics":{"value":{"index":-1,"value":0}}}]})",
    R"({"performance":[{"source":"A","target":"B","statistics":{"value":{"index":0.0,"value":0}}}]})",
    R"({"performance":[{"source":"A","target":"B","statistics":{"value":{"index":0}}}]})",
    R"({"performance":[{"source":"A","target":"B","statistics":{"value":{"value":0}}}]})",
    R"({"performance":[{"source":"A","target":"B","statistics":{"value":{"index":0,"value":[]}}}]})",
    R"({"performance":[{"source":"A","target":"B","statistics":{"value":{"index":1,"value":0}}}]})",
    R"({"performance":[{"source":"A","target":"B","statistics":{"value":{"index":2,"value":[]}}}]})",
    R"({"performance":[{"source":"A","target":"B","statistics":{"value":{"index":2,"value":{"sum":true}}}}]})",
    R"({"frequency":[{"source":"\uD800\u00XX","target":"B"}]})"
  };
  for (const auto* json : invalid) rejects([&] { (void)parse_dfg_playout_input_json(json); });
  rejects([] { (void)parse_dfg_playout_input_json(std::string("{\"frequency\":[{\"source\":\"\xed\xa0\x80\",\"target\":\"B\"}]}")); });
  rejects([] { (void)parse_dfg_playout_input_json(std::string(32u * 1024u * 1024u + 1, ' ')); });
}

void declared_maps_drive_classic_and_performance_native_calls() {
  const auto classic_input = parse_dfg_playout_input_json(
      R"({"frequency":[{"source":"A","target":"B","count":3},{"source":"A","target":"C","count":1}],"start_activities":[{"activity":"A","count":1}],"end_activities":[{"activity":"B","count":1},{"activity":"C","count":1}]})");
  const auto before = serialize_dfg_playout_input_json(classic_input);
  DfgPlayoutOptions classic;
  classic.max_variants = 2;
  const auto result = dfg_playout_result(classic_input.frequency, classic_input.start_activities,
      classic_input.end_activities, classic);
  assert(result.variant == DfgPlayoutVariant::kClassic && result.log.traces.size() == 2);
  assert(result.counts.at(std::vector<std::string>{"A", "B"}) == 2);
  assert(result.counts.at(std::vector<std::string>{"A", "C"}) == 1);
  assert(result.log.traces[0].events[1].activity == "B");
  assert(serialize_dfg_playout_input_json(classic_input) == before);

  const auto timed = parse_dfg_playout_input_json(
      R"({"frequency":[{"source":"A","target":"B","count":1}],"start_activities":[{"activity":"A","count":1}],"end_activities":[{"activity":"B","count":1}],"performance":[{"source":"A","target":"B","statistics":{"summary":{"count":18446744073709551615,"mean":9,"median":"NaN","minimum":"-INF","maximum":"INF","standard_deviation":-0.0,"sum":-9},"raw_values":[1,2],"value":{"index":0,"value":5}}}]})");
  DfgPlayoutOptions performance;
  performance.variant = DfgPlayoutVariant::kPerformance;
  performance.traces = 3;
  // The largest numpy.random.seed value.
  performance.seed = 0xffffffffULL;
  performance.case_arrival_rate = 10;
  auto execute = [&](const DfgPlayoutInput& input) {
    return dfg_playout_result(input.frequency, input.start_activities, input.end_activities,
        performance, input.performance ? &*input.performance : nullptr);
  };
  const auto timed_result = execute(timed);
  assert(timed_result.log.traces.size() == 3 && timed_result.counts.empty());
  assert(timed_result.completed_traces == 3 && timed_result.truncated_traces == 0);
  const auto timestamp = [&](std::size_t trace, std::size_t event) {
    return *parse_timestamp(timed_result.log.traces[trace].events[event].timestamp);
  };
  // The selected number is an exponential mean, not a fixed duration. The
  // seed reproduces the draws; equivalent selected means reproduce every
  // generated timestamp, independently of the inactive stored fields.
  assert(timestamp(0, 1) > timestamp(0, 0));
  assert(timestamp(1, 0) - timestamp(0, 0) == 10);
  const auto same_log = [](const EventLog& left, const EventLog& right) {
    assert(left.traces.size() == right.traces.size());
    for (std::size_t i = 0; i < left.traces.size(); ++i) {
      assert(left.traces[i].case_id == right.traces[i].case_id);
      assert(left.traces[i].events.size() == right.traces[i].events.size());
      for (std::size_t j = 0; j < left.traces[i].events.size(); ++j) {
        assert(left.traces[i].events[j].activity == right.traces[i].events[j].activity);
        assert(left.traces[i].events[j].timestamp == right.traces[i].events[j].timestamp);
      }
    }
  };
  same_log(timed_result.log, execute(timed).log);
  auto unused_mean = timed;
  unused_mean.performance->at({"A", "B"}).summary.mean = 1000;
  same_log(timed_result.log, execute(unused_mean).log);
  auto vector = timed;
  vector.performance->at({"A", "B"}).value = std::vector<double>{2, 4};
  const auto vector_result = execute(vector);
  auto scalar_three = timed;
  scalar_three.performance->at({"A", "B"}).value = 3.0;
  same_log(vector_result.log, execute(scalar_three).log);
  auto summary = timed;
  summary.performance->at({"A", "B"}).value = DfgPerformanceSummary{123, 999};
  const auto summary_result = execute(summary);
  auto scalar_nine = timed;
  scalar_nine.performance->at({"A", "B"}).value = 9.0;
  same_log(summary_result.log, execute(scalar_nine).log);
  summary.performance->at({"A", "B"}).value = std::vector<double>{};
  const auto empty_vector_result = execute(summary);
  same_log(empty_vector_result.log, execute(scalar_nine).log);
  auto missing = timed;
  missing.performance = DfgPerformance{};
  const auto missing_result = execute(missing);
  assert(missing_result.log.traces[0].events[0].timestamp == missing_result.log.traces[0].events[1].timestamp);
  missing.performance.reset();
  rejects([&] { (void)execute(missing); });
  auto invalid = timed;
  invalid.performance->at({"A", "B"}).summary.mean = -1;
  const auto invalid_json = serialize_dfg_playout_input_json(invalid);
  const auto parsed_invalid = parse_dfg_playout_input_json(invalid_json);
  rejects([&] { (void)execute(parsed_invalid); });
}
}  // namespace

int main() {
  empty_and_partial_records_keep_native_defaults();
  exact_counts_and_all_stored_performance_fields_round_trip();
  literal_labels_do_not_use_an_activity_registry();
  invalid_transport_is_rejected_without_silent_map_merging();
  declared_maps_drive_classic_and_performance_native_calls();
}
