#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "minera/batches.hpp"
#include "minera/performance_spectrum.hpp"

int main() {
  const auto throws = [](const auto& function) {
    try {
      function();
    } catch (const std::invalid_argument&) {
      return true;
    }
    return false;
  };
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "c";
  trace.events = {{"c", "a", "2024-01-01T00:00:00Z"},
                  {"c", "x", "2024-01-01T00:01:00Z"},
                  {"c", "b", "2024-01-01T00:02:00Z"}};
  log.traces.push_back(trace);
  auto spectrum = minera::compute_performance_spectrum(log, {"a", "b"});
  assert(spectrum.observations.size() == 1);
  assert(spectrum.observations.front().activities.size() == 2);
  minera::EventLog custom;
  minera::Trace custom_trace;
  custom_trace.case_id = "ignored-case-id";
  custom_trace.attributes.push_back({"case_ref", "custom-case"});
  custom_trace.events = {
      {"ignored-case-id", "ignored-a", "ignored-time", "", 0,
       {{"phase", "a"}, {"occurred_at", "2024-01-01T00:00:00Z"}}, ""},
      {"ignored-case-id", "ignored-b", "ignored-time", "", 1,
       {{"phase", "b"}, {"occurred_at", "2024-01-01T00:01:00Z"}}, ""}};
  custom.traces.push_back(custom_trace);
  minera::PerformanceSpectrumOptions custom_options;
  custom_options.variant = minera::PerformanceSpectrumVariant::kDataframe;
  custom_options.activity_attribute = "phase";
  custom_options.timestamp_attribute = "occurred_at";
  custom_options.case_id_attribute = "case_ref";
  const auto custom_result =
      minera::compute_performance_spectrum(custom, {"a", "b"}, custom_options);
  assert(custom_result.observations.size() == 1);
  assert(custom_result.observations.front().case_id == "custom-case");
  assert(custom_result.activity_attribute == "phase");
  for (const auto variant : {
           minera::PerformanceSpectrumVariant::kLog,
           minera::PerformanceSpectrumVariant::kDataframeDisconnected,
           minera::PerformanceSpectrumVariant::kLogDisconnected}) {
    auto alias_options = custom_options;
    alias_options.variant = variant;
    assert(minera::compute_performance_spectrum(custom, {"a", "b"},
                                                alias_options)
               .observations.size() == 1);
  }
  minera::PerformanceSpectrumOptions bad_variant;
  bad_variant.variant = static_cast<minera::PerformanceSpectrumVariant>(99);
  assert(throws([&] {
    (void)minera::compute_performance_spectrum(log, {"a", "b"}, bad_variant);
  }));

  // Sorting toggle and overlapping windows.
  minera::EventLog ordering;
  minera::Trace ot;
  ot.case_id = "order";
  ot.events = {{"order", "b", "2024-01-01T00:02:00Z"},
               {"order", "a", "2024-01-01T00:01:00Z"},
               {"order", "a", "2024-01-01T00:03:00Z"},
               {"order", "b", "2024-01-01T00:04:00Z"}};
  ordering.traces.push_back(ot);
  assert(minera::compute_performance_spectrum(ordering, {"a", "b", "a"})
             .observations.size() == 1);
  minera::PerformanceSpectrumOptions unsorted;
  unsorted.sort_log = false;
  assert(minera::compute_performance_spectrum(ordering, {"a", "b", "a"}, unsorted)
             .observations.empty());
  minera::EventLog loop;
  minera::Trace lt;
  lt.case_id = "loop";
  lt.events = {{"loop", "a", "2024-01-01T00:00:00Z"},
               {"loop", "a", "2024-01-01T00:01:00Z"},
               {"loop", "a", "2024-01-01T00:02:00Z"}};
  loop.traces.push_back(lt);
  assert(minera::compute_performance_spectrum(loop, {"a", "a"})
             .observations.size() == 2);
  minera::Trace bad;
  bad.case_id = "bad";
  bad.events = {{"bad", "a", "not-a-time"}, {"bad", "b", "2024-01-01"}};
  minera::EventLog bad_log;
  bad_log.traces.push_back(bad);
  assert(throws([&] {
    (void)minera::compute_performance_spectrum(bad_log, {"a", "b"});
  }));

  // Disconnected mode emits longest matches first and suppresses their
  // subsets while retaining endpoint overlap.
  minera::PerformanceSpectrumOptions disconnected;
  disconnected.variant = minera::PerformanceSpectrumVariant::kDisconnected;
  minera::EventLog partial;
  minera::Trace pt;
  pt.case_id = "partial";
  pt.events = {{"partial", "a", "2024-01-01T00:00:00Z"},
               {"partial", "b", "2024-01-01T00:01:00Z"},
               {"partial", "c", "2024-01-01T00:02:00Z"}};
  partial.traces.push_back(pt);
  auto disconnected_result =
      minera::compute_performance_spectrum(partial, {"a", "b", "c"}, disconnected);
  assert(disconnected_result.observations.size() == 1);
  assert(disconnected_result.observations[0].activities.size() == 3);
  minera::Trace p2;
  p2.case_id = "subpattern";
  p2.events = {{"subpattern", "b", "2024-01-01T00:00:00Z"},
               {"subpattern", "c", "2024-01-01T00:01:00Z"}};
  partial.traces.push_back(p2);
  auto partial_result =
      minera::compute_performance_spectrum(partial, {"a", "b", "c"}, disconnected);
  assert(partial_result.observations.size() == 2);
  assert(partial_result.observations.back().activities ==
         std::vector<std::string>({"b", "c"}));
  assert(partial_result.observations.back().pattern_offset == 1);
  minera::EventLog non_adjacent;
  minera::Trace nt;
  nt.case_id = "non-adjacent";
  nt.events = {{"non-adjacent", "a", "2024-01-01T00:00:00Z"},
               {"non-adjacent", "x", "2024-01-01T00:01:00Z"},
               {"non-adjacent", "c", "2024-01-01T00:02:00Z"}};
  non_adjacent.traces.push_back(nt);
  assert(minera::compute_performance_spectrum(non_adjacent, {"a", "b", "c"}, disconnected)
             .observations.empty());
  minera::EventLog endpoint_loop;
  minera::Trace et;
  et.case_id = "endpoint-loop";
  et.events = {{"endpoint-loop", "a", "2024-01-01T00:00:00Z"},
               {"endpoint-loop", "b", "2024-01-01T00:01:00Z"},
               {"endpoint-loop", "a", "2024-01-01T00:02:00Z"},
               {"endpoint-loop", "b", "2024-01-01T00:03:00Z"},
               {"endpoint-loop", "a", "2024-01-01T00:04:00Z"}};
  endpoint_loop.traces.push_back(et);
  auto endpoint_result = compute_performance_spectrum(
      endpoint_loop, {"a", "b", "a"}, disconnected);
  assert(endpoint_result.observations.size() == 2);
  assert(endpoint_result.observations[0].activities ==
         std::vector<std::string>({"a", "b", "a"}));
  minera::EventLog repeats;
  minera::Trace rt;
  rt.case_id = "repeats";
  for (int i = 0; i < 4; ++i) {
    rt.events.push_back({"repeats", "a",
                         "2024-01-01T00:0" + std::to_string(i) + ":00Z"});
  }
  repeats.traces.push_back(rt);
  auto repeated = compute_performance_spectrum(
      repeats, {"a", "a", "a"}, disconnected);
  assert(repeated.observations.size() == 2);
  minera::EventLog duplicate_pattern;
  duplicate_pattern.traces.push_back(rt);
  auto duplicate_result = compute_performance_spectrum(
      duplicate_pattern, {"a", "a", "a", "a"}, disconnected);
  assert(duplicate_result.observations.size() == 1);
  minera::EventLog compressed;
  minera::Trace ct;
  ct.case_id = "compressed";
  const std::vector<std::string> compressed_activities{"A", "D", "D",
                                                        "A", "D", "D"};
  for (std::size_t i = 0; i < compressed_activities.size(); ++i) {
    ct.events.push_back({"compressed", compressed_activities[i],
                         "2024-01-01T00:0" + std::to_string(i) + ":00Z"});
  }
  compressed.traces.push_back(std::move(ct));
  auto compressed_result = compute_performance_spectrum(
      compressed, {"A", "D", "D"}, disconnected);
  assert(compressed_result.observations.size() == 3);
  bool found_compressed_pair = false;
  for (const auto& observation : compressed_result.observations)
    found_compressed_pair = found_compressed_pair ||
                            observation.activities ==
                                std::vector<std::string>({"D", "D"});
  assert(found_compressed_pair);
  // Exact midpoint sampling: n=11,m=4 -> indices 0,1,3,6,9,10.
  minera::EventLog many;
  for (int i = 0; i < 11; ++i) {
    minera::Trace mt;
    mt.case_id = std::to_string(i);
    mt.events = {{mt.case_id, "a", "2024-01-01T00:00:00Z"},
                 {mt.case_id, "b", "2024-01-01T00:00:01Z"}};
    many.traces.push_back(std::move(mt));
  }
  minera::PerformanceSpectrumOptions sampled;
  sampled.sample_size = 4;
  auto sampled_result = minera::compute_performance_spectrum(many, {"a", "b"}, sampled);
  assert(sampled_result.observations.size() == 6);
  const std::vector<std::string> sampled_ids{"0", "1", "3", "6", "9", "10"};
  for (std::size_t i = 0; i < sampled_ids.size(); ++i)
    assert(sampled_result.observations[i].case_id == sampled_ids[i]);
  sampled.sample_size = 0;
  const auto zero_sample =
      minera::compute_performance_spectrum(many, {"a", "b"}, sampled);
  assert(zero_sample.observations.size() == 2);
  assert(zero_sample.observations.front().case_id == "0");
  assert(zero_sample.observations.back().case_id == "10");
  auto batches = minera::discover_batches(log);
  assert(batches.groups.empty());

  minera::EventLog batch_log;
  std::size_t batch_case = 0;
  auto add = [&](const char* activity, const char* time) {
    minera::Trace trace;
    trace.case_id = "batch-" + std::to_string(batch_case++);
    minera::Event event;
    event.case_id = trace.case_id;
    event.activity = activity;
    event.timestamp = time;
    trace.events.push_back(std::move(event));
    batch_log.traces.push_back(std::move(trace));
  };
  add("a", "2024-01-01T00:00:00Z");
  add("a", "2024-01-01T00:00:00Z");
  add("a", "2024-01-01T02:00:00Z");
  add("a", "2024-01-01T02:00:00Z");
  for (int i = 0; i < 5; ++i) add("b", "2024-01-01T05:00:00Z");
  minera::BatchDiscoveryOptions batch_options;
  batch_options.merge_distance_seconds = 0.0;
  auto discovered = minera::discover_batches(batch_log, batch_options);
  assert(discovered.groups.size() == 2);
  assert(discovered.groups[0].activity == "a");
  assert(discovered.groups[0].batch_count == 2);
  assert(discovered.groups[0].event_count == 4);
  assert(discovered.groups[0].resource.empty());
  assert(discovered.groups[1].batch_count == 1);
  for (const auto variant : {minera::BatchDiscoveryVariant::kPandas,
                             minera::BatchDiscoveryVariant::kPolars}) {
    auto adapter_options = batch_options;
    adapter_options.variant = variant;
    const auto adapter_result =
        minera::discover_batches(batch_log, adapter_options);
    assert(adapter_result.variant == variant);
    assert(adapter_result.groups.size() == discovered.groups.size());
    assert(adapter_result.groups[0].event_count == discovered.groups[0].event_count);
  }
  auto min_three = batch_options;
  min_three.min_batch_size = 3;
  const auto size_filtered = minera::discover_batches(batch_log, min_three);
  assert(size_filtered.groups.size() == 1);
  assert(size_filtered.groups[0].activity == "b");
  assert(size_filtered.groups[0].event_count == 5);

  // Classification covers each public batch type.
  minera::EventLog typed;
  std::size_t typed_case = 0;
  auto add_interval = [&](const char* activity, const char* start,
                          const char* complete) {
    minera::Trace trace;
    trace.case_id = "typed-" + std::to_string(typed_case++);
    minera::Event event;
    event.case_id = trace.case_id;
    event.activity = activity;
    event.timestamp = complete;
    event.attributes.push_back({"st", start, minera::AttributeKind::kDate});
    trace.events.push_back(std::move(event));
    typed.traces.push_back(std::move(trace));
  };
  add_interval("sim", "2024-01-01T00:00:00Z", "2024-01-01T00:01:00Z");
  add_interval("sim", "2024-01-01T00:00:00Z", "2024-01-01T00:01:00Z");
  add_interval("bos", "2024-01-01T00:00:00Z", "2024-01-01T00:01:00Z");
  add_interval("bos", "2024-01-01T00:00:00Z", "2024-01-01T00:02:00Z");
  add_interval("boe", "2024-01-01T00:00:00Z", "2024-01-01T00:03:00Z");
  add_interval("boe", "2024-01-01T00:01:00Z", "2024-01-01T00:03:00Z");
  add_interval("seq", "2024-01-01T00:00:00Z", "2024-01-01T00:01:00Z");
  add_interval("seq", "2024-01-01T00:01:00Z", "2024-01-01T00:02:00Z");
  add_interval("con", "2024-01-01T00:00:00Z", "2024-01-01T00:03:00Z");
  add_interval("con", "2024-01-01T00:01:00Z", "2024-01-01T00:02:00Z");
  minera::BatchDiscoveryOptions typed_options;
  typed_options.start_timestamp_attribute = "st";
  typed_options.merge_distance_seconds = 0.0;
  auto typed_result = minera::discover_batches(typed, typed_options);
  const auto type_of = [&](const char* activity) {
    for (const auto& group : typed_result.groups)
      if (group.activity == activity) return group.batches.front().type;
    return minera::BatchType::kConcurrent;
  };
  assert(type_of("sim") == minera::BatchType::kSimultaneous);
  assert(type_of("bos") == minera::BatchType::kBatchingOnStart);
  assert(type_of("boe") == minera::BatchType::kBatchingOnEnd);
  assert(type_of("seq") == minera::BatchType::kSequential);
  assert(type_of("con") == minera::BatchType::kConcurrent);

  // All batch selectors can target generic event/trace attributes, matching
  // the column-oriented variants while keeping the native event log shape.
  minera::EventLog selected_batch_log;
  minera::Trace selected_trace;
  selected_trace.case_id = "source-case";
  selected_trace.attributes.push_back(
      {"case_ref", "selected-case", minera::AttributeKind::kString});
  std::size_t selected_case = 0;
  auto add_selected = [&](const char* phase, const char* team,
                          const char* start, const char* finish) {
    minera::Event event;
    event.case_id = selected_trace.case_id;
    event.activity = "ignored";
    event.resource = "ignored";
    event.timestamp = "1970-01-01T00:00:00Z";
    event.attributes.push_back(
        {"phase", phase, minera::AttributeKind::kString});
    event.attributes.push_back(
        {"team", team, minera::AttributeKind::kString});
    event.attributes.push_back(
        {"started_at", start, minera::AttributeKind::kDate});
    event.attributes.push_back(
        {"finished_at", finish, minera::AttributeKind::kDate});
    event.attributes.push_back(
        {"case_ref", "selected-case-" + std::to_string(selected_case++),
         minera::AttributeKind::kString});
    selected_trace.events.push_back(std::move(event));
  };
  add_selected("phase-A", "team-1", "1970-01-01T00:00:00Z",
               "1970-01-01T00:01:00Z");
  add_selected("phase-A", "team-1", "1970-01-01T00:00:00Z",
               "1970-01-01T00:01:00Z");
  selected_batch_log.traces.push_back(std::move(selected_trace));
  minera::BatchDiscoveryOptions selected_options;
  selected_options.activity_attribute = "phase";
  selected_options.resource_attribute = "team";
  selected_options.timestamp_attribute = "finished_at";
  selected_options.case_id_attribute = "case_ref";
  selected_options.start_timestamp_attribute = "started_at";
  selected_options.merge_distance_seconds = 0.0;
  const auto selected_result =
      minera::discover_batches(selected_batch_log, selected_options);
  assert(selected_result.groups.size() == 1);
  assert(selected_result.groups.front().activity == "phase-A");
  assert(selected_result.groups.front().resource == "team-1");
  assert(selected_result.groups.front().batches.front().events.front().case_id ==
         "selected-case-0");
  assert(selected_result.groups.front().batches.front().complete == 60.0);
  assert(throws([&] {
    auto bad = selected_options;
    bad.activity_attribute = "missing_phase";
    (void)minera::discover_batches(selected_batch_log, bad);
  }));

  // Overlap, touching, threshold gaps, transitivity, duplicate identity and
  // validation behavior.
  minera::EventLog merge_log;
  minera::Trace merge_trace;
  merge_trace.case_id = "merge";
  auto add_merge = [&](const char* start, const char* complete) {
    minera::Event event;
    event.case_id = "merge";
    event.activity = "m";
    event.timestamp = complete;
    event.attributes.push_back({"st", start, minera::AttributeKind::kDate});
    merge_trace.events.push_back(std::move(event));
  };
  add_merge("2024-01-01T00:00:00Z", "2024-01-01T00:01:00Z");
  add_merge("2024-01-01T00:00:30Z", "2024-01-01T00:01:30Z");
  add_merge("2024-01-01T00:01:30Z", "2024-01-01T00:02:00Z");
  add_merge("2024-01-01T00:02:20Z", "2024-01-01T00:03:00Z");
  add_merge("2024-01-01T00:02:30Z", "2024-01-01T00:03:20Z");
  merge_log.traces.push_back(merge_trace);
  minera::BatchDiscoveryOptions merge_options;
  merge_options.start_timestamp_attribute = "st";
  merge_options.merge_distance_seconds = 20.0;
  auto merged = minera::discover_batches(merge_log, merge_options);
  assert(merged.groups.size() == 1 && merged.groups[0].batches.size() == 1);
  assert(merged.groups[0].batches[0].events.size() == 5);
  assert(merged.groups[0].batches[0].events[2].event_index == 2);
  merge_options.merge_distance_seconds = 0.0;
  auto strict = minera::discover_batches(merge_log, merge_options);
  assert(strict.groups.size() == 1 && strict.groups[0].batches.size() == 2);
  // A negative distance merges only overlaps, and a minimum of one keeps
  // single-event intervals, as in the reference.
  auto negative = merge_options;
  negative.merge_distance_seconds = -1.0;
  (void)minera::discover_batches(merge_log, negative);
  assert(throws([&] {
    auto bad = merge_options;
    bad.merge_distance_seconds = std::numeric_limits<double>::infinity();
    (void)minera::discover_batches(merge_log, bad);
  }));
  auto singles = merge_options;
  singles.min_batch_size = 1;
  assert(!minera::discover_batches(merge_log, singles).groups.empty());
  assert(throws([&] {
    auto bad = merge_options;
    bad.start_timestamp_attribute = "missing";
    (void)minera::discover_batches(merge_log, bad);
  }));
  minera::EventLog invalid_log = merge_log;
  invalid_log.traces[0].events[0].timestamp = "invalid";
  assert(throws([&] { (void)minera::discover_batches(invalid_log); }));
  invalid_log = merge_log;
  invalid_log.traces[0].events[0].attributes[0].value = "invalid";
  assert(throws([&] { (void)minera::discover_batches(invalid_log, merge_options); }));
  // An event that starts after it completes is taken as it is (pm4py's three
  // variants do; review BN1-4): one concurrent batch of all five events.
  invalid_log = merge_log;
  invalid_log.traces[0].events[0].attributes[0].value =
      "2024-01-01T00:02:00Z";
  const auto inverted = minera::discover_batches(invalid_log, merge_options);
  assert(inverted.groups.size() == 1 && inverted.groups[0].batches.size() == 1);
  assert(inverted.groups[0].batches[0].type == minera::BatchType::kConcurrent);
  assert(inverted.groups[0].batches[0].events.size() == 5);
  assert(inverted.groups[0].batches[0].start == 1704067230.0 &&
         inverted.groups[0].batches[0].complete == 1704067400.0);
  assert(std::any_of(inverted.groups[0].batches[0].events.begin(), inverted.groups[0].batches[0].events.end(),
                     [](const auto& event) { return event.start == 1704067320.0 && event.complete == 1704067260.0; }));
  std::cout << "temporal/behavioral tests passed\n";
}
