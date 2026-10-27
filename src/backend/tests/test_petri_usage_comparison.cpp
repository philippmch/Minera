#include <cassert>
#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <string_view>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "minera/csv_event_log_importer.hpp"
#include "minera/json_export.hpp"
#include "minera/petri_usage_comparison.hpp"
#include "minera/powl_discovery.hpp"
#include "minera/xes_event_log_importer.hpp"
#include "../tools/pnml_model_options.hpp"

namespace {

void public_dynamic_color_reference_contract() {
  // Pinned public black-box capture: all 2001 directions at 0.001 steps;
  // adjacent representable values around each of the 255 bin boundaries.
  constexpr std::array<std::string_view, 256> palette = {
      "#00BFFF", "#35C5F8", "#6ACBF0", "#88CFEC", "#8AD0EC", "#8BD0EC", "#8DD1ED", "#8ED2ED",
      "#8FD3ED", "#91D4EE", "#92D4EE", "#94D5EE", "#95D6EF", "#97D7EF", "#98D8EF", "#9AD8F0",
      "#9BD9F0", "#9DDAF0", "#9EDBF1", "#9FDCF1", "#A1DCF1", "#A2DDF2", "#A4DEF2", "#A5DFF2",
      "#A7E0F3", "#A8E0F3", "#AAE1F3", "#ABE2F3", "#ADE3F4", "#AEE4F4", "#AFE4F4", "#B1E5F5",
      "#B2E6F5", "#B4E7F5", "#B5E8F6", "#B7E8F6", "#B8E9F6", "#BAEAF7", "#BBEBF7", "#BDECF7",
      "#BEECF8", "#BFEDF8", "#C1EEF8", "#C2EFF9", "#C4F0F9", "#C5F0F9", "#C7F1FA", "#C8F2FA",
      "#CAF3FA", "#CBF4FB", "#CDF4FB", "#CEF5FB", "#CFF6FC", "#D1F7FC", "#D2F8FC", "#D4F8FD",
      "#D5F9FD", "#D7FAFD", "#D8FBFE", "#DAFCFE", "#DBFCFE", "#DDFDFF", "#DEFEFF", "#DFFFFF",
      "#E0FFFF", "#E0FEFE", "#E0FDFD", "#DFFCFC", "#DFFBFB", "#DFF9F9", "#DEF8F8", "#DEF7F7",
      "#DEF6F6", "#DDF5F5", "#DDF4F4", "#DDF3F3", "#DCF1F1", "#DCF0F0", "#DCEFEF", "#DBEEEE",
      "#DBEDED", "#DBECEC", "#DAEBEB", "#DAE9E9", "#DAE8E8", "#D9E7E7", "#D9E6E6", "#D9E5E5",
      "#D8E4E4", "#D8E2E2", "#D8E1E1", "#D7E0E0", "#D7DFDF", "#D7DEDE", "#D6DDDD", "#D6DCDC",
      "#D6DADA", "#D5D9D9", "#D5D8D8", "#D5D7D7", "#D4D6D6", "#D4D5D5", "#D3D3D3", "#CDCDCD",
      "#C6C6C6", "#C0C0C0", "#B9B9B9", "#B3B3B3", "#ACACAC", "#A6A6A6", "#9F9F9F", "#999999",
      "#929292", "#8C8C8C", "#858585", "#818181", "#858585", "#888888", "#8B8B8B", "#8E8E8E",
      "#929292", "#959595", "#989898", "#9B9B9B", "#9F9F9F", "#A2A2A2", "#A5A5A5", "#A8A8A8",
      "#ACACAC", "#AFAFAF", "#B2B2B2", "#B5B5B5", "#B9B9B9", "#BCBCBC", "#BFBFBF", "#C2C2C2",
      "#C6C6C6", "#C9C9C9", "#CCCCCC", "#CFCFCF", "#D3D3D3", "#D4D4D4", "#D5D4D4", "#D6D4D4",
      "#D7D5D5", "#D8D5D5", "#D8D5D5", "#D9D6D5", "#DAD6D6", "#DBD6D6", "#DCD7D6", "#DDD7D6",
      "#DED7D7", "#DED8D7", "#DFD8D7", "#E0D8D8", "#E1D9D8", "#E2D9D8", "#E3D9D8", "#E4DAD9",
      "#E5DAD9", "#E5DAD9", "#E6DBD9", "#E7DBDA", "#E8DBDA", "#E9DCDA", "#EADCDB", "#EBDCDB",
      "#EBDDDB", "#ECDDDB", "#EDDDDC", "#EEDEDC", "#EFDEDC", "#F0DEDC", "#F1DFDD", "#F1DFDD",
      "#F2DFDD", "#F3E0DE", "#F4E0DE", "#F5E0DE", "#F6E1DE", "#F7E1DF", "#F8E1DF", "#F8E2DF",
      "#F9E2E0", "#FAE2E0", "#FBE3E0", "#FCE3E0", "#FDE3E1", "#FEE4E1", "#FEE4E1", "#FFE4E1",
      "#FFE3E0", "#FFE2DE", "#FFE0DD", "#FFDEDB", "#FFDDD9", "#FFDBD7", "#FFD9D5", "#FFD8D3",
      "#FFD6D2", "#FFD5D0", "#FFD3CE", "#FFD1CC", "#FED0CA", "#FECEC9", "#FECCC7", "#FECBC5",
      "#FEC9C3", "#FEC7C1", "#FEC6BF", "#FEC4BE", "#FEC3BC", "#FEC1BA", "#FEBFB8", "#FEBEB6",
      "#FDBCB5", "#FDBAB3", "#FDB9B1", "#FDB7AF", "#FDB6AD", "#FDB4AC", "#FDB2AA", "#FDB1A8",
      "#FDAFA6", "#FDADA4", "#FDACA2", "#FDAAA1", "#FCA89F", "#FCA79D", "#FCA59B", "#FCA499",
      "#FCA298", "#FCA096", "#FC9F94", "#FC9D92", "#FC9B90", "#FC9A8F", "#FC988D", "#FC968B",
      "#FC9589", "#FB9387", "#FB9285", "#FB9084", "#FB8E82", "#FB8D80", "#FB8B7E", "#FB897C",
      "#FB887B", "#FB8679", "#FB8577", "#FB8375", "#FB8173", "#FC7A69", "#FE6F58", "#FF6347",
  };
  constexpr std::array<unsigned, 765> boundary_colors = {
      0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8,
      8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 15, 16, 16,
      16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 20, 20, 20, 21, 21, 21, 22, 22, 22, 23, 23, 23, 24, 24,
      24, 25, 25, 25, 26, 26, 26, 27, 27, 27, 28, 28, 28, 29, 29, 29, 30, 30, 30, 31, 31, 31, 32, 32,
      32, 33, 33, 33, 34, 34, 34, 35, 35, 35, 36, 36, 36, 37, 37, 37, 38, 38, 38, 39, 39, 39, 40, 40,
      40, 41, 41, 41, 42, 42, 42, 43, 43, 43, 44, 44, 44, 45, 45, 45, 46, 46, 46, 47, 47, 47, 48, 48,
      48, 49, 49, 49, 50, 50, 50, 51, 51, 51, 52, 52, 52, 53, 53, 53, 54, 54, 54, 55, 55, 55, 56, 56,
      56, 57, 57, 57, 58, 58, 58, 59, 59, 59, 60, 60, 60, 61, 61, 61, 62, 62, 62, 63, 63, 63, 64, 64,
      65, 65, 65, 66, 66, 66, 67, 67, 67, 68, 68, 68, 69, 69, 69, 70, 70, 70, 71, 71, 71, 72, 72, 72,
      73, 73, 73, 74, 74, 74, 75, 75, 75, 76, 76, 76, 77, 77, 77, 78, 78, 78, 79, 79, 79, 80, 80, 80,
      81, 81, 81, 82, 82, 82, 83, 83, 83, 84, 84, 84, 85, 85, 85, 86, 86, 86, 87, 87, 87, 88, 88, 88,
      89, 89, 89, 90, 90, 90, 91, 91, 91, 92, 92, 92, 93, 93, 93, 94, 94, 94, 95, 95, 95, 96, 96, 96,
      97, 97, 97, 98, 98, 98, 99, 99, 99, 100, 100, 100, 101, 101, 101, 102, 102, 102, 103, 103, 103, 104, 104, 104,
      105, 105, 105, 106, 106, 106, 107, 107, 107, 108, 108, 108, 109, 109, 109, 110, 110, 110, 111, 111, 111, 112, 112, 112,
      113, 113, 113, 114, 114, 114, 115, 115, 115, 114, 114, 114, 117, 117, 117, 118, 118, 118, 119, 119, 119, 112, 112, 112,
      121, 121, 121, 122, 122, 122, 123, 123, 123, 110, 110, 110, 125, 125, 125, 126, 126, 126, 127, 127, 127, 108, 108, 108,
      129, 129, 129, 130, 130, 130, 131, 131, 131, 106, 106, 106, 133, 133, 133, 134, 134, 134, 135, 135, 135, 104, 104, 104,
      137, 137, 137, 138, 138, 138, 139, 139, 139, 102, 102, 102, 141, 141, 141, 142, 142, 142, 143, 143, 143, 144, 144, 144,
      145, 145, 145, 145, 145, 145, 147, 147, 147, 148, 148, 148, 149, 149, 149, 150, 150, 150, 151, 151, 151, 152, 152, 152,
      153, 153, 153, 154, 154, 154, 155, 155, 155, 156, 156, 156, 157, 157, 157, 158, 158, 158, 159, 159, 159, 160, 160, 160,
      160, 160, 160, 162, 162, 162, 163, 163, 163, 164, 164, 164, 165, 165, 165, 166, 166, 166, 167, 167, 167, 168, 168, 168,
      169, 169, 169, 170, 170, 170, 171, 171, 171, 172, 172, 172, 173, 173, 173, 174, 174, 174, 174, 174, 174, 176, 176, 176,
      177, 177, 177, 178, 178, 178, 179, 179, 179, 180, 180, 180, 181, 181, 181, 182, 182, 182, 183, 183, 183, 184, 184, 184,
      185, 185, 185, 186, 186, 186, 187, 187, 187, 188, 188, 188, 189, 189, 189, 189, 189, 189, 191, 191, 191, 192, 192, 192,
      193, 193, 193, 194, 194, 194, 195, 195, 195, 196, 196, 196, 197, 197, 197, 198, 198, 198, 199, 199, 199, 200, 200, 200,
      201, 201, 201, 202, 202, 202, 203, 203, 203, 204, 204, 204, 205, 205, 205, 206, 206, 206, 207, 207, 207, 208, 208, 208,
      209, 209, 209, 210, 210, 210, 211, 211, 211, 212, 212, 212, 213, 213, 213, 214, 214, 214, 215, 215, 215, 216, 216, 216,
      217, 217, 217, 218, 218, 218, 219, 219, 219, 220, 220, 220, 221, 221, 221, 222, 222, 222, 223, 223, 223, 224, 224, 224,
      225, 225, 225, 226, 226, 226, 227, 227, 227, 228, 228, 228, 229, 229, 229, 230, 230, 230, 231, 231, 231, 232, 232, 232,
      233, 233, 233, 234, 234, 234, 235, 235, 235, 236, 236, 236, 237, 237, 237, 238, 238, 238, 239, 239, 239, 240, 240, 240,
      241, 241, 241, 242, 242, 242, 243, 243, 243, 244, 244, 244, 245, 245, 245, 246, 246, 246, 247, 247, 247, 248, 248, 248,
      249, 249, 249, 250, 250, 250, 251, 251, 251, 252, 252, 252, 253, 253, 253, 254, 254, 254, 255, 255, 255,
  };
  for (int i = -1000; i <= 1000; ++i) {
    const double direction = i / 1000.0;
    const auto bin = std::min(255, static_cast<int>(std::floor((0.5 + 0.5 * direction) * 256)));
    assert(minera::petri_usage_direction_color(direction) == palette[bin]);
  }
  std::size_t sample = 0;
  for (int bin = 1; bin < 256; ++bin) {
    const double boundary = bin / 128.0 - 1.0;
    for (const auto direction : {std::nextafter(boundary, -std::numeric_limits<double>::infinity()),
                                 boundary, std::nextafter(boundary, std::numeric_limits<double>::infinity())})
      assert(minera::petri_usage_direction_color(direction) == palette[boundary_colors[sample++]]);
  }
  for (const auto invalid : {std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::infinity(), -1.01, 1.01}) {
    bool rejected = false;
    try { (void)minera::petri_usage_direction_color(invalid); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  }
}


minera::EventLog log_of(std::initializer_list<std::string> activities) {
  minera::Trace trace;
  trace.case_id = "case";
  for (const auto& activity : activities) {
    minera::Event event;
    event.case_id = trace.case_id;
    event.activity = activity;
    trace.events.push_back(std::move(event));
  }
  return minera::EventLog{{std::move(trace)}};
}

minera::PetriNet simple_net() {
  minera::PetriNet net;
  const auto p0 = net.add_place();
  const auto p1 = net.add_place();
  const auto p2 = net.add_place();
  const auto p3 = net.add_place();  // deliberately unused
  (void)p3;
  net.set_source(p0);
  net.set_sink(p2);
  net.set_activities({"A"});
  net.add_transition(0, {p0}, {p1});
  net.add_transition(std::nullopt, {p1}, {p2});
  return net;
}

void public_weighted_forced_replenishment_contract() {
  constexpr std::array<unsigned, 112> source_usage = {
    1, 2, 3, 4, 2, 4, 6, 8, 3, 6, 9, 12, 4, 8, 12, 16,
    1, 2, 3, 4, 3, 5, 7, 9, 4, 7, 10, 13, 5, 9, 13, 17,
    2, 2, 3, 4, 2, 4, 6, 8, 5, 8, 11, 14, 6, 10, 14, 18,
    3, 3, 3, 4, 3, 5, 7, 9, 3, 6, 9, 12, 7, 11, 15, 19,
    4, 4, 4, 4, 4, 4, 6, 8, 4, 7, 10, 13, 4, 8, 12, 16,
    5, 5, 5, 5, 5, 5, 7, 9, 5, 8, 11, 14, 5, 9, 13, 17,
    6, 6, 6, 6, 6, 6, 6, 8, 6, 6, 9, 12, 6, 10, 14, 18,
  };
  std::size_t sample = 0;
  for (unsigned tokens = 0; tokens <= 6; ++tokens)
    for (unsigned weight = 1; weight <= 4; ++weight)
      for (unsigned repeats = 1; repeats <= 4; ++repeats) {
        minera::PetriNet net;
        const auto source = net.add_place(), sink = net.add_place();
        net.set_source(source); net.set_sink(sink); net.set_activities({"A"});
        net.add_transition(0, std::vector<std::size_t>(weight, source), {sink});
        auto log = log_of({});
        for (unsigned i = 0; i < repeats; ++i) { minera::Event event; event.activity = "A"; log.traces[0].events.push_back(event); }
        minera::PetriUsageComparisonOptions options;
        options.initial_marking = {tokens, 0}; options.final_marking = {0, 1};
        const auto result = minera::compare_petri_usage(net, log, minera::EventLog{}, options);
        assert(result.places[source].log1_occurrences == source_usage[sample++]);
        assert(result.places[sink].log1_occurrences == repeats);
        assert(result.transitions[0].log1_occurrences == repeats);
      }
}

void public_static_color_and_null_activity_contract() {
  const std::array<double, 4> boundaries = {-0.5, -0.1, 0.0, 0.5};
  const std::array<std::string_view, 5> colors = {"#4444FF", "#AAAAFF", "#CCCCCC", "#FFAAAA", "#FF4444"};
  for (std::size_t i = 0; i < boundaries.size(); ++i) {
    assert(minera::petri_usage_direction_color_static(std::nextafter(boundaries[i], -1.0)) == colors[i]);
    assert(minera::petri_usage_direction_color_static(boundaries[i]) == colors[i]);
    assert(minera::petri_usage_direction_color_static(std::nextafter(boundaries[i], 1.0)) == colors[i + 1]);
  }
  for (int i = -1000; i <= 1000; ++i) {
    const auto expected = i <= -500 ? colors[0] : i <= -100 ? colors[1] : i <= 0 ? colors[2] : i <= 500 ? colors[3] : colors[4];
    assert(minera::petri_usage_direction_color_static(i / 1000.0) == expected);
  }
  minera::PetriNet net;
  const auto source = net.add_place(), sink = net.add_place();
  net.set_source(source); net.set_sink(sink); net.set_activities({"A"});
  net.add_transition(0, {source}, {sink});
  for (const bool custom : {false, true}) {
    auto log = log_of({"A", "A"});
    minera::PetriUsageComparisonOptions options;
    if (custom) {
      options.activity_attribute = "step";
      log.traces[0].events[0].attributes.push_back({"step", "", minera::AttributeKind::kString, true});
      log.traces[0].events[1].attributes.push_back({"step", "A"});
    } else {
      log.traces[0].events[0].activity.clear();
      log.traces[0].events[0].activity_state = minera::FirstClassAttributeState::kNull;
    }
    options.stop_immediately_unfit = true;
    const auto result = minera::compare_petri_usage(net, log, minera::EventLog{}, options);
    assert(result.transitions[0].log1_occurrences == 1);
  }
}

void null_activity_resolution_preserves_projection_priority() {
  const auto net = simple_net();
  auto log = log_of({"A"});
  auto& event = log.traces[0].events[0];
  event.resource_state = minera::FirstClassAttributeState::kNull;
  assert(minera::compare_petri_usage(net, log, minera::EventLog{}).transitions[0].log1_occurrences == 1);
  event.attributes.push_back({"concept:name", "", minera::AttributeKind::kString, true});
  minera::PetriUsageComparisonOptions options;
  options.activity_attribute = "concept:name";
  assert(minera::compare_petri_usage(net, log, minera::EventLog{}, options).transitions[0].log1_occurrences == 1);
  event.activity_source_key = "step";
  event.resource_source_key = "step";
  options.activity_attribute = "step";
  assert(minera::compare_petri_usage(net, log, minera::EventLog{}, options).transitions[0].log1_occurrences == 1);
  event.activity_state = minera::FirstClassAttributeState::kNull;
  event.resource_state = minera::FirstClassAttributeState::kPresent;
  event.resource = "A";
  assert(minera::compare_petri_usage(net, log, minera::EventLog{}, options).transitions[0].log1_occurrences == 0);
}

void comparison_module_contract() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source); net.set_sink(sink); net.set_activities({"A"});
  net.add_transition(0, {source}, {sink});
  const auto result = minera::compare_petri_usage(net, log_of({}), log_of({"A"}));
  const auto alias = minera::compare_element_usage_two_logs(net, log_of({}), log_of({"A"}));
  assert(result.places[source].log1_occurrences == 1);
  assert(result.places[sink].log1_occurrences == 1);
  assert(result.transitions[0].log1_occurrences == 0);
  assert(result.transitions[0].log2_occurrences == 1);
  assert(result.transitions[0].color == "#FF6347");
  assert(minera::petri_usage_comparison_json(result) == minera::petri_usage_comparison_json(alias));
  assert(minera::petri_usage_comparison_json(result).find("#ACACAC") != std::string::npos);
  const auto no_places = minera::compare_petri_usage(minera::PetriNet{}, log_of({}), log_of({}));
  assert(no_places.places.empty() && no_places.transitions.empty() && no_places.arcs.empty());
  auto missing = log_of({"A"});
  missing.traces[0].events[0].activity.clear();
  bool rejected = false;
  try { (void)minera::compare_petri_usage(net, missing, log_of({})); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}

void public_forced_repeat_place_counts() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source); net.set_sink(sink); net.set_activities({"A"});
  net.add_transition(0, {source}, {sink});
  for (const auto& left : {log_of({"A", "A"}), log_of({"A", "X", "A"})}) {
    const auto result = minera::compare_petri_usage(net, left, log_of({"A"}));
    assert(result.transitions[0].log1_occurrences == 2);
    for (const auto& place : result.places) {
      assert(place.log1_occurrences == 2 && place.log2_occurrences == 1);
      assert(place.color == "#D9E7E7");
    }
  }
}

void usage_directions_and_silent_activation() {
  const auto net = simple_net();
  const auto one = log_of({"A"});
  const auto empty = log_of({});
  const auto equal = minera::compare_petri_usage(net, one, one);
  assert(equal.transitions.size() == 2);
  assert(equal.transitions[0].log1_occurrences == 1);
  assert(equal.transitions[0].log2_occurrences == 1);
  assert(equal.transitions[0].direction == 0.0);
  assert(equal.transitions[0].label == "A (1/1/2)");
  assert(equal.transitions[0].color == "#ACACAC");
  assert(equal.transitions[1].label == "(1/1/2)");
  assert(equal.arcs[0].label == equal.transitions[0].label);
  assert(equal.arcs[0].color == equal.transitions[0].color);
  assert(equal.transitions[1].log1_occurrences == 1);  // silent routing
  assert(equal.places[3].total_occurrences == 0);
  assert(equal.places[3].direction == 0.0);  // zero-total is finite and neutral

  const auto negative = minera::compare_element_usage_two_logs(net, one, empty);
  assert(negative.transitions[0].direction == -1.0);
  assert(negative.transitions[1].direction == -1.0);
  assert(negative.transitions[0].color == "#00BFFF");
  const auto positive = minera::compare_petri_usage(net, empty, one);
  assert(positive.transitions[0].direction == 1.0);
  assert(positive.transitions[0].color == "#FF6347");
  assert(std::isfinite(positive.places[0].direction));
}

void weighted_arc_and_duplicate_label_contract() {
  minera::PetriNet net;
  const auto p0 = net.add_place();
  const auto p1 = net.add_place();
  const auto p2 = net.add_place();
  net.set_source(p0);
  net.set_sink(p2);
  net.set_activities({"A"});
  net.add_transition(0, {p0, p0}, {p1});  // weighted input arc
  net.add_transition(0, {p1}, {p2});      // duplicate label, distinct index

  minera::PetriUsageComparisonOptions options;
  options.transition_names = {"z_weighted", "a_following"};
  const auto result = minera::compare_petri_usage(net, log_of({"A"}), log_of({}), options);
  assert(result.transitions.size() == 2);
  assert(result.transitions[0].log1_occurrences == 1);
  assert(result.transitions[1].log1_occurrences == 0);
  assert(result.places[p0].log1_occurrences == 3);  // consumption plus retained partial token
  assert(result.arcs.size() == 5);
  assert(result.arcs[0].place == p0 && result.arcs[1].place == p0);
  assert(result.arcs[0].total_occurrences == result.transitions[0].total_occurrences);
}

void finish_routes_when_sink_has_a_token_and_other_work_remains() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto branch = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A"});
  net.add_transition(0, {source}, {sink, branch});
  net.add_transition(std::nullopt, {branch, sink}, {sink});

  const auto result = minera::compare_petri_usage(net, log_of({"A"}),
                                                   log_of({}));
  assert(result.transitions[1].log1_occurrences == 0);
  assert(result.places[branch].log1_occurrences == 1);
}

void explicit_markings_control_replay_boundaries() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto entry = net.add_place();
  const auto after = net.add_place();
  const auto alternate_final = net.add_place();
  net.set_source(source);
  net.set_sink(after);
  net.set_activities({"A"});
  net.add_transition(std::nullopt, {source}, {entry});
  net.add_transition(0, {entry}, {after});
  net.add_transition(std::nullopt, {after}, {alternate_final});

  minera::PetriUsageComparisonOptions options;
  options.initial_marking = {0, 1, 0, 0};
  options.final_marking = {0, 0, 0, 1};
  const auto result = minera::compare_petri_usage(
      net, log_of({"A"}), log_of({}), options);
  assert(result.transitions[0].log1_occurrences == 0);
  assert(result.transitions[1].log1_occurrences == 1);
  assert(result.transitions[2].log1_occurrences == 1);
  assert(result.places[alternate_final].log1_occurrences == 1);
}

void replay_controls_are_honored() {
  minera::PetriNet routed;
  const auto source = routed.add_place();
  const auto entry = routed.add_place();
  const auto sink = routed.add_place();
  routed.set_source(source);
  routed.set_sink(sink);
  routed.set_activities({"A"});
  routed.add_transition(std::nullopt, {source}, {entry});
  routed.add_transition(0, {entry}, {sink});

  const auto default_routing =
      minera::compare_petri_usage(routed, log_of({"A"}), log_of({}));
  assert(default_routing.transitions[0].log1_occurrences == 1);
  assert(default_routing.transitions[1].log1_occurrences == 1);

  minera::PetriUsageComparisonOptions no_entry_routing;
  no_entry_routing.walk_through_hidden_trans = false;
  const auto forced = minera::compare_petri_usage(
      routed, log_of({"A"}), log_of({}), no_entry_routing);
  assert(forced.transitions[0].log1_occurrences == 0);
  assert(forced.transitions[1].log1_occurrences == 1);

  minera::PetriUsageComparisonOptions no_final_routing;
  no_final_routing.try_to_reach_final_marking_through_hidden = false;
  const auto unfinished = minera::compare_petri_usage(
      simple_net(), log_of({"A"}), log_of({}), no_final_routing);
  assert(unfinished.transitions[0].log1_occurrences == 1);
  assert(unfinished.transitions[1].log1_occurrences == 0);
  assert(unfinished.places[1].log1_occurrences == 1);

  minera::PetriUsageComparisonOptions stop;
  stop.stop_immediately_unfit = true;
  const auto stopped_on_unknown = minera::compare_petri_usage(
      simple_net(), log_of({"unknown", "A"}), log_of({}), stop);
  assert(stopped_on_unknown.transitions[0].log1_occurrences == 1);
  const auto continued_after_unknown = minera::compare_petri_usage(
      simple_net(), log_of({"unknown", "A"}), log_of({}));
  assert(continued_after_unknown.transitions[0].log1_occurrences == 1);

  const auto stopped_after_forced = minera::compare_petri_usage(
      simple_net(), log_of({"A", "A", "A"}), log_of({}), stop);
  assert(stopped_after_forced.transitions[0].log1_occurrences == 1);
  const auto continued_after_forced = minera::compare_petri_usage(
      simple_net(), log_of({"A", "A", "A"}), log_of({}));
  assert(continued_after_forced.transitions[0].log1_occurrences == 3);
}

void extended_arcs_are_replayed_and_counted() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto buffered = net.add_place();
  const auto gate = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A"});
  net.add_transition(0, {source}, {buffered, buffered, gate});
  const auto reset = net.add_transition(std::nullopt, {gate}, {sink});
  net.add_reset_arc(reset, buffered);

  const auto result = minera::compare_petri_usage(
      net, log_of({"A"}), log_of({}));
  assert(result.transitions[1].log1_occurrences == 1);
  assert(result.places[buffered].log1_occurrences == 2);
  assert(result.arcs.size() == 7);
  assert(result.arcs.back().transition == reset);
  assert(result.arcs.back().place == buffered);
  assert(result.arcs.back().kind == minera::PetriArcUsageKind::kReset);
  assert(result.arcs.back().input);
  assert(result.arcs.back().log1_occurrences == 1);
  const auto json = minera::petri_usage_comparison_json(result);
  assert(json.find("\"kind\":\"reset\"") != std::string::npos);
}

void inhibitor_arcs_gate_silent_completion() {
  minera::PetriNet net;
  const auto source = net.add_place();
  const auto guard = net.add_place();
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.add_transition(std::nullopt, {source}, {sink});
  net.add_inhibitor_arc(0, guard);

  const auto enabled_result = minera::compare_petri_usage(
      net, log_of({}), log_of({}));
  assert(enabled_result.transitions[0].log1_occurrences == 1);
  assert(enabled_result.arcs.size() == 3);
  assert(enabled_result.arcs.back().place == guard);
  assert(enabled_result.arcs.back().kind ==
         minera::PetriArcUsageKind::kInhibitor);
  assert(enabled_result.arcs.back().input);
  assert(enabled_result.arcs.back().log1_occurrences == 1);
  assert(minera::petri_usage_comparison_json(enabled_result).find(
             "\"kind\":\"inhibitor\"") != std::string::npos);

  minera::PetriUsageComparisonOptions blocked_options;
  blocked_options.initial_marking = {1, 1, 0};
  blocked_options.final_marking = {1, 1, 0};
  const auto blocked_result = minera::compare_petri_usage(
      net, log_of({}), log_of({}), blocked_options);
  assert(blocked_result.transitions[0].log1_occurrences == 0);
}

void marking_dimensions_are_checked() {
  const auto net = simple_net();
  minera::PetriUsageComparisonOptions options;
  options.initial_marking = {1};
  bool rejected = false;
  try {
    (void)minera::compare_petri_usage(net, log_of({}), log_of({}), options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void custom_activity_attribute_controls_replay() {
  const auto net = simple_net();
  minera::EventLog selected;
  minera::Trace trace;
  trace.case_id = "custom";
  minera::Event event;
  event.case_id = trace.case_id;
  event.activity = "ignored";
  event.attributes.push_back({"business_activity", "A",
                              minera::AttributeKind::kString});
  trace.events.push_back(std::move(event));
  selected.traces.push_back(std::move(trace));

  minera::PetriUsageComparisonOptions options;
  options.activity_attribute = "business_activity";
  const auto custom = compare_petri_usage(net, selected, log_of({}), options);
  assert(custom.transitions[0].log1_occurrences == 1);
  assert(custom.transitions[1].log1_occurrences == 1);

  options.activity_attribute = "missing_activity";
  bool missing_rejected = false;
  try { (void)compare_petri_usage(net, selected, log_of({}), options); }
  catch (const std::invalid_argument&) { missing_rejected = true; }
  assert(missing_rejected);

  options.activity_attribute = "concept:name";
  const auto canonical = compare_petri_usage(net, log_of({"A"}), log_of({}),
                                             options);
  assert(canonical.transitions[0].log1_occurrences == 1);

  selected.traces.front().events.front().attributes.clear();
  selected.traces.front().events.front().resource = "A";
  for (const auto attribute : {"resource", "org:resource"}) {
    options.activity_attribute = attribute;
    const auto resource =
        compare_petri_usage(net, selected, log_of({}), options);
    assert(resource.transitions[0].log1_occurrences == 1);
  }

  selected.traces.front().events.front().resource.clear();
  selected.traces.front().events.front().timestamp = "A";
  for (const auto attribute : {"timestamp", "time:timestamp"}) {
    options.activity_attribute = attribute;
    const auto timestamp =
        compare_petri_usage(net, selected, log_of({}), options);
    assert(timestamp.transitions[0].log1_occurrences == 1);
  }

  selected.traces.front().events.front().attributes.clear();
  selected.traces.front().events.front().timestamp.clear();
  selected.traces.front().events.front().lifecycle = "A";
  for (const auto attribute : {"lifecycle", "lifecycle:transition"}) {
    options.activity_attribute = attribute;
    const auto lifecycle =
        compare_petri_usage(net, selected, log_of({}), options);
    assert(lifecycle.transitions[0].log1_occurrences == 1);
  }
}

void aliases_prefer_first_class_fields_and_reject_blank_selectors() {
  const auto net = simple_net();
  minera::EventLog selected;
  minera::Trace trace;
  trace.case_id = "aliases";
  minera::Event event;
  event.case_id = trace.case_id;
  event.activity = "ignored";
  event.resource = "A";
  event.attributes.push_back({"resource", "not-the-resource",
                              minera::AttributeKind::kString});
  trace.events.push_back(std::move(event));
  selected.traces.push_back(std::move(trace));

  minera::PetriUsageComparisonOptions options;
  options.activity_attribute = "  org:resource  ";
  const auto result = compare_petri_usage(net, selected, log_of({}), options);
  assert(result.transitions[0].log1_occurrences == 1);

  options.activity_attribute = " \t ";
  bool rejected = false;
  try {
    (void)compare_petri_usage(net, selected, log_of({}), options);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void imported_custom_activity_attributes_are_replayable() {
  const auto net = simple_net();

  minera::CsvImportOptions csv_options;
  csv_options.activity_column = "business_activity";
  std::istringstream csv("case_id,business_activity\ncase-1,A\n");
  const auto csv_log = minera::import_csv_event_log(csv, csv_options);
  assert(csv_log.traces.size() == 1);
  assert(csv_log.traces.front().events.front().activity == "A");
  assert(csv_log.traces.front().events.front().activity_source_key ==
         "business_activity");
  assert(csv_log.traces.front().events.front().attribute("business_activity") ==
         nullptr);

  minera::PetriUsageComparisonOptions options;
  options.activity_attribute = "business_activity";
  const auto csv_result = compare_petri_usage(net, csv_log, log_of({}), options);
  assert(csv_result.transitions[0].log1_occurrences == 1);

  minera::XesImportOptions xes_options;
  xes_options.activity_attribute = "business_activity";
  std::istringstream xes(
      "<log><trace><string key=\"concept:name\" value=\"case-1\"/>"
      "<event><string key=\"business_activity\" value=\"A\"/>"
      "</event></trace></log>");
  const auto xes_log = minera::import_xes_event_log(xes, xes_options);
  assert(xes_log.traces.front().events.front().activity == "A");
  assert(xes_log.traces.front().events.front().activity_source_key ==
         "business_activity");
  assert(xes_log.traces.front().events.front().attribute("business_activity") ==
         nullptr);
  const auto xes_result = compare_petri_usage(net, xes_log, log_of({}), options);
  assert(xes_result.transitions[0].log1_occurrences == 1);
}

void imported_first_class_aliases_work_without_default_activity_columns() {
  const auto net = simple_net();
  const std::vector<std::pair<const char*, const char*>> csv_aliases = {
      {"org:resource", "resource"},
      {"time:timestamp", "timestamp"},
      {"lifecycle:transition", "lifecycle"},
  };
  for (const auto& [selector, column] : csv_aliases) {
    minera::CsvImportOptions import_options;
    import_options.activity_column = selector;
    std::istringstream csv(std::string("case_id,") + column +
                           "\ncase-1,A\n");
    const auto log = minera::import_csv_event_log(csv, import_options);
    assert(log.traces.size() == 1);
    minera::PetriUsageComparisonOptions comparison_options;
    comparison_options.activity_attribute = selector;
    const auto result =
        compare_petri_usage(net, log, log_of({}), comparison_options);
    assert(result.transitions[0].log1_occurrences == 1);
  }

  const std::vector<std::pair<const char*, const char*>> xes_aliases = {
      {"org:resource", "org:resource"},
      {"time:timestamp", "time:timestamp"},
      {"lifecycle:transition", "lifecycle:transition"},
  };
  for (const auto& [selector, key] : xes_aliases) {
    minera::XesImportOptions import_options;
    import_options.activity_attribute = selector;
    std::istringstream xes(
        std::string("<log><trace><string key=\"concept:name\" value=\"case-1\"/>"
                    "<event><string key=\"") +
        key + "\" value=\"A\"/></event></trace></log>");
    const auto log = minera::import_xes_event_log(xes, import_options);
    assert(log.traces.size() == 1);
    minera::PetriUsageComparisonOptions comparison_options;
    comparison_options.activity_attribute = selector;
    const auto result =
        compare_petri_usage(net, log, log_of({}), comparison_options);
    assert(result.transitions[0].log1_occurrences == 1);
  }
}

void imported_powl_reference_uses_all_cached_selectors() {
  const auto net = simple_net();
  minera::PowlDiscoveryOptions powl_options;
  powl_options.activity_key = "step_name";
  powl_options.timestamp_key = "finished_at";
  powl_options.case_id_key = "order_ref";

  // This mirrors the server's reference-CSV construction for a cached POWL
  // model.  The default case_id/timestamp names must not be retained when
  // the cached model selected custom POWL fields.
  bool default_options_rejected = false;
  try {
    std::istringstream csv(
        "order_ref,step_name,finished_at\n"
        "order-1,A,2024-01-01T00:00:00Z\n");
    (void)minera::import_csv_event_log(csv);
  } catch (const std::invalid_argument&) {
    default_options_rejected = true;
  }
  assert(default_options_rejected);

  minera::CsvImportOptions reference_options;
  reference_options.case_id_column = powl_options.case_id_key;
  reference_options.timestamp_column = powl_options.timestamp_key;
  reference_options.activity_column = powl_options.activity_key;
  std::istringstream csv(
      "order_ref,step_name,finished_at\n"
      "order-1,A,2024-01-01T00:00:00Z\n");
  const auto reference =
      minera::import_csv_event_log(csv, reference_options);
  const auto normalized =
      minera::normalize_powl_event_log(reference, powl_options);
  assert(normalized.traces.size() == 1);
  assert(normalized.traces.front().case_id == "order-1");
  assert(normalized.traces.front().events.size() == 1);
  assert(normalized.traces.front().events.front().activity == "A");
  assert(normalized.traces.front().events.front().timestamp ==
         "2024-01-01T00:00:00Z");

  minera::PetriUsageComparisonOptions comparison_options;
  comparison_options.activity_attribute = powl_options.activity_key;
  const auto comparison =
      minera::compare_petri_usage(net, normalized, normalized,
                                  comparison_options);
  assert(comparison.transitions.front().log1_occurrences == 1);
  assert(comparison.transitions.front().log2_occurrences == 1);
}

void xes_exact_selector_beats_alias_in_any_order() {
  const auto net = simple_net();
  const std::vector<std::string> event_attributes = {
      "<string key=\"activity\" value=\"B\"/>"
      "<string key=\"concept:name\" value=\"A\"/>",
      "<string key=\"concept:name\" value=\"A\"/>"
      "<string key=\"activity\" value=\"B\"/>"};
  for (const auto& attributes : event_attributes) {
    std::istringstream xes(
        std::string("<log><trace><string key=\"concept:name\" value=\"case-1\"/>"
                    "<event>") +
        attributes + "</event></trace></log>");
    const auto log = minera::import_xes_event_log(xes);
    const auto result = compare_petri_usage(net, log, log_of({}));
    assert(result.transitions[0].log1_occurrences == 1);
  }
}

void invalid_net_indices_are_rejected() {
  minera::PetriNet net;
  const auto p0 = net.add_place();
  net.set_source(p0);
  net.set_sink(p0);
  net.set_activities({"A"});
  net.add_transition(4, {}, {});
  bool rejected = false;
  try {
    (void)minera::compare_petri_usage(net, log_of({}), log_of({}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void silent_search_budget_is_reported() {
  constexpr std::size_t branch_count = 1400;  // More than 4096 structurally reachable places.
  minera::PetriNet net;
  const auto source = net.add_place();
  std::vector<std::size_t> pending, yes, no;
  pending.reserve(branch_count);
  yes.reserve(branch_count);
  no.reserve(branch_count);
  for (std::size_t i = 0; i < branch_count; ++i) {
    pending.push_back(net.add_place());
    yes.push_back(net.add_place());
    no.push_back(net.add_place());
  }
  const auto sink = net.add_place();
  net.set_source(source);
  net.set_sink(sink);
  net.set_activities({"A"});
  net.add_transition(std::nullopt, {source}, pending);
  for (std::size_t i = 0; i < branch_count; ++i) {
    // Wide structural exploration must report its explicit resource limit.
    net.add_transition(std::nullopt, {pending[i]}, {no[i]});
    net.add_transition(std::nullopt, {pending[i]}, {yes[i]});
  }
  net.add_transition(0, yes, {sink});

  bool rejected = false;
  try {
    (void)minera::compare_petri_usage(net, log_of({"A"}), log_of({}));
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  assert(rejected);
}

void token_flood_cleaning_preserves_usage_accounting() {
  minera::PetriNet net;
  for (int i = 0; i < 3; ++i) net.add_place();
  net.set_source(0); net.set_sink(2); net.set_activities({"A", "B"});
  net.add_transition(0, {0}, {1});
  net.add_transition(1, {1}, {2});
  minera::PetriUsageComparisonOptions options;
  options.cleaning_token_flood = true;
  const auto reference = log_of({"A", "B"});
  const auto reversed = minera::compare_petri_usage(net, log_of({"B", "A"}), reference, options);
  for (const auto& place : reversed.places) assert(place.log1_occurrences == 2);
  assert(reversed.transitions[0].log1_occurrences == 1 && reversed.transitions[1].log1_occurrences == 1);
  const auto repeated = minera::compare_petri_usage(net, log_of({"B", "B", "A"}), reference, options);
  assert(repeated.places[0].log1_occurrences == 2);
  assert(repeated.places[1].log1_occurrences == 3);
  assert(repeated.places[2].log1_occurrences == 2);
  // Adding another token to an existing place does not introduce a new key.
  const auto existing_output = minera::compare_petri_usage(net, log_of({"A", "A", "B", "A"}), reference, options);
  assert(existing_output.places[2].log1_occurrences == 1);
  options.initial_marking = {3, 0, 0};
  const auto removed_many = minera::compare_petri_usage(net, log_of({"B"}), reference, options);
  assert(removed_many.places[0].log1_occurrences == 1);  // One removed place, three tokens.
  options.stop_immediately_unfit = true;
  const auto stopped = minera::compare_petri_usage(net, log_of({"B"}), reference, options);
  assert(stopped.places[0].log1_occurrences == 3 && stopped.transitions[1].log1_occurrences == 0);
  options.cleaning_token_flood = false;
  options.stop_immediately_unfit = false;
  const auto disabled = minera::compare_petri_usage(net, log_of({"B"}), reference, options);
  assert(disabled.places[0].log1_occurrences == 3);
  options.cleaning_token_flood = true;
  options.initial_marking = {1, 0, 0};
  options.stop_immediately_unfit = true;
  auto missing_after_stop = log_of({"B", "A"});
  missing_after_stop.traces[0].events[1].activity.clear();
  for (const bool clean : {false, true}) {
    options.cleaning_token_flood = clean;
    bool rejected_missing = false;
    try { (void)minera::compare_petri_usage(net, missing_after_stop, reference, options); }
    catch (const std::invalid_argument&) { rejected_missing = true; }
    assert(rejected_missing);
  }
  options.cleaning_token_flood = true;
  options.initial_marking = {0, 0, 0};
  bool rejected = false;
  try { (void)minera::compare_petri_usage(net, log_of({}), reference, options); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}

void token_flood_cleaning_respects_public_depth_boundary() {
  // Public observations at depths 5, 6 and 7: only depth 5 includes the sink
  // in the source component, so the forced A then removes the source token.
  for (const std::size_t depth : {5, 6, 7}) {
    minera::PetriNet net;
    for (std::size_t i = 0; i < 2 * depth + 2; ++i) net.add_place();
    const auto sink = 2 * depth + 1;
    net.set_source(0); net.set_sink(sink); net.set_activities({"A", "B"});
    for (std::size_t i = 0; i < depth; ++i)
      net.add_transition(std::nullopt, {i}, {i + 1, depth + 1 + i});
    net.add_transition(0, {depth}, {sink});
    net.add_transition(1, {0}, {0});
    for (const bool clean : {false, true}) {
      minera::PetriUsageComparisonOptions options;
      options.walk_through_hidden_trans = false;
      options.cleaning_token_flood = clean;
      const auto result = minera::compare_petri_usage(net, log_of({"A", "B"}), log_of({}), options);
      assert(result.places[0].log1_occurrences == (clean && depth == 5 ? 3 : 2));
      assert(result.places[sink].log1_occurrences == (clean && depth == 5 ? 2 : 1));
    }
  }
}

void token_flood_component_budget_is_reported() {
  minera::PetriNet net;
  net.add_place();
  std::vector<std::size_t> inputs{0};
  for (int depth = 0; depth < 6; ++depth) {
    std::vector<std::size_t> outputs;
    for (int branch = 0; branch < 7; ++branch) outputs.push_back(net.add_place());
    net.add_transition(std::nullopt, inputs, outputs);
    inputs = std::move(outputs);
  }
  net.set_source(0); net.set_sink(inputs.back());
  minera::PetriUsageComparisonOptions options;
  options.cleaning_token_flood = true;
  options.try_to_reach_final_marking_through_hidden = false;
  bool rejected = false;
  try { (void)minera::compare_petri_usage(net, log_of({}), log_of({}), options); }
  catch (const std::runtime_error& error) {
    rejected = std::string_view(error.what()).find("cleaning-component state limit") != std::string_view::npos;
  }
  assert(rejected);
}

void public_partial_final_routing_contract() {
  minera::PetriNet weighted;
  weighted.add_place(); weighted.add_place();
  weighted.set_source(0); weighted.set_sink(1);
  weighted.add_transition(std::nullopt, {0}, {1});
  minera::PetriUsageComparisonOptions options;
  options.initial_marking = {3, 0}; options.final_marking = {0, 2};
  const auto moved = minera::compare_petri_usage(weighted, log_of({}), log_of({}), options);
  assert(moved.transitions[0].log1_occurrences == 1);
  assert(moved.places[0].log1_occurrences == 3 && moved.places[1].log1_occurrences == 2);
  options.initial_marking = {1, 1};
  assert(minera::compare_petri_usage(weighted, log_of({}), log_of({}), options)
             .transitions[0].log1_occurrences == 0);

  minera::PetriNet blocked;
  for (int place = 0; place < 4; ++place) blocked.add_place();
  blocked.set_source(0); blocked.set_sink(3);
  blocked.add_transition(std::nullopt, {0}, {1});
  blocked.add_transition(std::nullopt, {1, 2}, {3});
  const auto partial = minera::compare_petri_usage(blocked, log_of({}), log_of({}));
  assert(partial.transitions[0].log1_occurrences == 1);
  assert(partial.transitions[1].log1_occurrences == 0);
  assert(partial.places[1].log1_occurrences == 1);

  minera::PetriNet competing;
  for (int place = 0; place < 3; ++place) competing.add_place();
  competing.set_source(0); competing.set_sink(2);
  competing.add_transition(std::nullopt, {0}, {1});
  competing.add_transition(std::nullopt, {0}, {2});
  options = {};
  options.final_marking = {0, 1, 1};
  auto choices = minera::compare_petri_usage(competing, log_of({}), log_of({}), options);
  assert(choices.transitions[0].log1_occurrences == 1 && choices.transitions[1].log1_occurrences == 0);
  options.place_names = {"source", "z_final", "a_final"};
  choices = minera::compare_petri_usage(competing, log_of({}), log_of({}), options);
  assert(choices.transitions[0].log1_occurrences == 0 && choices.transitions[1].log1_occurrences == 1);
  options.place_names.clear(); options.initial_marking = {1, 1, 0};
  choices = minera::compare_petri_usage(competing, log_of({}), log_of({}), options);
  assert(choices.transitions[0].log1_occurrences == 1 && choices.transitions[1].log1_occurrences == 0);
  options.initial_marking = {3, 0, 0};
  choices = minera::compare_petri_usage(competing, log_of({}), log_of({}), options);
  assert(choices.transitions[0].log1_occurrences == 1 && choices.transitions[1].log1_occurrences == 1);
  options.place_names = {"incomplete"};
  bool rejected = false;
  try { (void)minera::compare_petri_usage(competing, log_of({}), log_of({}), options); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}

void public_final_routing_pass_limit() {
  for (const int stages : {1, 5, 6, 8}) {
    minera::PetriNet net;
    auto source = net.add_place();
    net.set_source(source);
    for (int stage = 0; stage < stages; ++stage) {
      const auto left = net.add_place(), right = net.add_place();
      const auto left_end = net.add_place(), right_end = net.add_place();
      const auto end = net.add_place();
      net.add_transition(std::nullopt, {source}, {left, right});
      net.add_transition(std::nullopt, {left}, {left_end});
      net.add_transition(std::nullopt, {right}, {right_end});
      net.add_transition(std::nullopt, {left_end, right_end}, {end});
      source = end;
    }
    net.set_sink(source);
    const auto result = minera::compare_petri_usage(net, log_of({}), log_of({}));
    std::uint64_t fired = 0;
    for (const auto& transition : result.transitions) fired += transition.log1_occurrences;
    assert(fired == static_cast<std::uint64_t>(std::min(stages * 4, 22)));
  }
  for (const std::size_t depth : {51, 52, 100}) {
    minera::PetriNet long_chain;
    for (std::size_t place = 0; place <= depth; ++place) long_chain.add_place();
    long_chain.set_source(0); long_chain.set_sink(depth);
    for (std::size_t place = 0; place < depth; ++place)
      long_chain.add_transition(std::nullopt, {place}, {place + 1});
    const auto result = minera::compare_petri_usage(long_chain, log_of({}), log_of({}));
    for (const auto& transition : result.transitions)
      assert(transition.log1_occurrences == (depth == 51 ? 1 : 0));
  }
}

void public_pre_event_prefix_and_duplicate_contract() {
  minera::PetriNet net;
  for (int place = 0; place < 5; ++place) net.add_place();
  net.set_source(0); net.set_sink(4); net.set_activities({"A"});
  net.add_transition(std::nullopt, {0}, {1});
  net.add_transition(std::nullopt, {1, 2}, {3});
  net.add_transition(0, {3}, {4});
  minera::PetriUsageComparisonOptions options;
  options.try_to_reach_final_marking_through_hidden = false;
  for (const bool stop : {false, true}) {
    options.stop_immediately_unfit = stop;
    const auto result = minera::compare_petri_usage(net, log_of({"A"}), log_of({}), options);
    assert(result.transitions[0].log1_occurrences == 1 && result.transitions[1].log1_occurrences == 0);
    assert(result.transitions[2].log1_occurrences == (stop ? 0 : 1));
    assert(result.places[1].log1_occurrences == 1);
  }
  net.add_transition(0, {1}, {4});
  options.transition_names = {"first", "join", "z", "a"};
  for (const bool stop : {false, true}) {
    options.stop_immediately_unfit = stop;
    const auto result = minera::compare_petri_usage(net, log_of({"A"}), log_of({}), options);
    assert(result.transitions[0].log1_occurrences == 1);
    assert(result.transitions[2].log1_occurrences == (stop ? 0 : 1));
    assert(result.transitions[3].log1_occurrences == 0);
  }
  options.transition_names.pop_back();
  bool rejected = false;
  try { (void)minera::compare_petri_usage(net, log_of({}), log_of({}), options); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}

void public_pre_event_weighted_and_path_limit() {
  minera::PetriNet weighted;
  for (int place = 0; place < 3; ++place) weighted.add_place();
  weighted.set_source(0); weighted.set_sink(2); weighted.set_activities({"A"});
  weighted.add_transition(std::nullopt, {0}, {1});
  weighted.add_transition(0, {1, 1}, {2});
  minera::PetriUsageComparisonOptions options;
  options.try_to_reach_final_marking_through_hidden = false;
  for (const std::uint32_t present : {0, 1}) {
    for (const bool stop : {false, true}) {
      options.initial_marking = {2, present, 0};
      options.stop_immediately_unfit = stop;
      const auto result = minera::compare_petri_usage(weighted, log_of({"A"}), log_of({}), options);
      assert(result.transitions[0].log1_occurrences == 1);
      assert(result.transitions[1].log1_occurrences == (stop && present == 0 ? 0 : 1));
    }
  }
  for (const std::size_t depth : {51, 52}) {
    minera::PetriNet chain;
    for (std::size_t place = 0; place <= depth + 1; ++place) chain.add_place();
    chain.set_source(0); chain.set_sink(depth + 1); chain.set_activities({"A"});
    for (std::size_t place = 0; place < depth; ++place)
      chain.add_transition(std::nullopt, {place}, {place + 1});
    chain.add_transition(0, {depth}, {depth + 1});
    options = {};
    options.try_to_reach_final_marking_through_hidden = false;
    for (const bool stop : {false, true}) {
      options.stop_immediately_unfit = stop;
      const auto result = minera::compare_petri_usage(chain, log_of({"A"}), log_of({}), options);
      assert(result.transitions[0].log1_occurrences == (depth == 51 ? 1 : 0));
      assert(result.transitions[depth].log1_occurrences == (stop && depth == 52 ? 0 : 1));
    }
  }
}

void public_zero_marking_keys_contract() {
  // Public zero-final probes route to an absent key even when its value is 0.
  minera::PetriNet silent;
  silent.add_place(); silent.add_place();
  silent.set_source(0); silent.set_sink(1);
  silent.add_transition(std::nullopt, {0}, {1});
  minera::PetriUsageComparisonOptions options;
  options.initial_marking = {1, 0};
  options.final_marking = {0, 0};
  const auto absent = minera::compare_petri_usage(silent, log_of({}), log_of({}), options);
  assert(absent.transitions[0].log1_occurrences == 0);
  options.final_marking_keys = {false, true};
  for (const bool clean : {false, true}) {
    options.cleaning_token_flood = clean;
    const auto routed = minera::compare_petri_usage(silent, log_of({}), log_of({}), options);
    assert(routed.transitions[0].log1_occurrences == 1);
    assert(routed.places[0].log1_occurrences == 1 && routed.places[1].log1_occurrences == 1);
  }
  options.initial_marking_keys = {true, true};
  for (const bool clean : {false, true}) {
    options.cleaning_token_flood = clean;
    const auto already_present = minera::compare_petri_usage(silent, log_of({}), log_of({}), options);
    assert(already_present.transitions[0].log1_occurrences == 0);
    assert(already_present.places[0].log1_occurrences == 1 && already_present.places[1].log1_occurrences == 0);
  }

  // Cleaning a stale zero key records one removal, as for a positive key.
  minera::PetriNet chain;
  chain.add_place(); chain.add_place(); chain.add_place();
  chain.set_source(0); chain.set_sink(2); chain.set_activities({"A", "B"});
  chain.add_transition(0, {0}, {1}); chain.add_transition(1, {1}, {2});
  options.initial_marking = {0, 0, 0};
  options.initial_marking_keys = {true, false, false};
  options.final_marking = {0, 0, 1};
  options.final_marking_keys = {false, false, true};
  const auto removed_zero = minera::compare_petri_usage(chain, log_of({"B"}), log_of({"A", "B"}), options);
  for (const auto& place : removed_zero.places)
    assert(place.log1_occurrences == 1 && place.log2_occurrences == 1);
  assert(removed_zero.transitions[0].log1_occurrences == 0 && removed_zero.transitions[1].log1_occurrences == 1);

  // A single zero initial key is a valid cleaning root; zero final demand
  // contributes no tokens to the place counts of a visible sequence.
  minera::PetriNet visible;
  visible.add_place(); visible.add_place();
  visible.set_source(0); visible.set_sink(1); visible.set_activities({"A"});
  visible.add_transition(0, {0}, {1});
  options.initial_marking = {0, 0};
  options.initial_marking_keys = {true, false};
  options.final_marking = {0, 1};
  options.final_marking_keys = {false, true};
  const auto initial_zero = minera::compare_petri_usage(visible, log_of({}), log_of({"A"}), options);
  assert(initial_zero.places[0].log1_occurrences == 0 && initial_zero.places[0].log2_occurrences == 1);
  assert(initial_zero.places[1].log1_occurrences == 1 && initial_zero.places[1].log2_occurrences == 1);
  assert(initial_zero.transitions[0].log1_occurrences == 0 && initial_zero.transitions[0].log2_occurrences == 1);
  options.initial_marking = {1, 0};
  options.final_marking = {0, 0};
  const auto final_zero = minera::compare_petri_usage(visible, log_of({}), log_of({"A"}), options);
  assert(final_zero.places[1].log1_occurrences == 0 && final_zero.places[1].log2_occurrences == 1);

  // Metadata must describe the supplied numeric marking, including positives.
  for (const auto& keys : {std::vector<bool>{true}, std::vector<bool>{false, false}}) {
    options.initial_marking_keys = keys;
    bool rejected = false;
    try { (void)minera::compare_petri_usage(visible, log_of({}), log_of({}), options); }
    catch (const std::invalid_argument& error) {
      rejected = std::string_view(error.what()).find("initial_marking_keys") != std::string_view::npos;
    }
    assert(rejected);
  }
  options.initial_marking_keys = {true, false};
  options.final_marking = {0, 1};
  for (const auto& keys : {std::vector<bool>{true}, std::vector<bool>{false, false}}) {
    options.final_marking_keys = keys;
    bool rejected = false;
    try { (void)minera::compare_petri_usage(visible, log_of({}), log_of({}), options); }
    catch (const std::invalid_argument& error) {
      rejected = std::string_view(error.what()).find("final_marking_keys") != std::string_view::npos;
    }
    assert(rejected);
  }
}

void cleaning_uses_duplicate_place_names() {
  minera::PetriNet net;
  net.set_activities({"A", "B"});
  for (std::size_t i = 0; i < 4; ++i) net.add_place();
  net.set_source(0); net.set_sink(3);
  net.add_transition(std::nullopt, {0}, {1});
  net.add_transition(0, {2}, {2}); net.add_transition(1, {1}, {3});
  minera::PetriUsageComparisonOptions options;
  options.cleaning_token_flood = true;
  options.place_names = {"start", "reach", "reach", "end"};
  const auto replayed = minera::compare_petri_usage(net, log_of({"A", "B"}), log_of({}), options);
  assert(replayed.transitions[0].log1_occurrences == 0);
  assert(replayed.transitions[1].log1_occurrences == 1 && replayed.transitions[2].log1_occurrences == 1);
  options.place_names[2] = "alias";
  assert(minera::compare_petri_usage(net, log_of({"A", "B"}), log_of({}), options).transitions[0].log1_occurrences == 1);

  minera::PetriNet alias;
  alias.set_activities({"A", "B"});
  for (std::size_t i = 0; i < 3; ++i) alias.add_place();
  alias.set_source(0); alias.set_sink(2);
  alias.add_transition(0, {1}, {1}); alias.add_transition(1, {0}, {2});
  options.place_names = {"reach", "reach", "end"};
  const auto preserved = minera::compare_petri_usage(alias, log_of({"A", "A", "B"}), log_of({}), options);
  assert(preserved.transitions[0].log1_occurrences == 2 && preserved.transitions[1].log1_occurrences == 1);
  assert(preserved.places[0].log1_occurrences == 1 && preserved.places[1].log1_occurrences == 3);
}
void cleaning_preserves_a_newly_produced_name_alias() {
  minera::PetriNet net;
  net.set_activities({"C"});
  for (std::size_t i = 0; i < 4; ++i) net.add_place();
  net.set_source(0); net.set_sink(2);
  net.add_transition(std::nullopt, {0}, {1});
  net.add_transition(0, {1}, {2, 3});
  minera::PetriUsageComparisonOptions options;
  options.place_names = {"X0", "R", "E", "X0"};
  options.cleaning_token_flood = true;
  options.walk_through_hidden_trans = false; options.try_to_reach_final_marking_through_hidden = false;
  options.initial_marking_keys = {true, false, false, false};
  for (const std::uint32_t tokens : {0u, 1u, 2u}) {
    options.initial_marking = {tokens, 0, 0, 0};
    const auto result = minera::compare_petri_usage(net, log_of({"C"}), log_of({}), options);
    assert(result.transitions[0].log1_occurrences == 0 && result.transitions[1].log1_occurrences == 1);
    assert(result.places[0].log1_occurrences == tokens && result.places[3].log1_occurrences == 1);
  }
  options.place_names[3] = "alias"; options.initial_marking = {2, 0, 0, 0};
  assert(minera::compare_petri_usage(net, log_of({"C"}), log_of({}), options).places[0].log1_occurrences == 1);
}
void cleaning_combines_nested_split_branches() {
  minera::PetriNet net;
  net.set_activities({"A", "N", "C", "B", "D"});
  for (std::size_t i = 0; i < 8; ++i) net.add_place();
  net.set_source(0); net.set_sink(7);
  net.add_transition(0, {0}, {1}); net.add_transition(1, {1}, {2});
  net.add_transition(2, {2}, {3, 4}); net.add_transition(3, {3}, {6});
  net.add_transition(4, {0}, {4, 5}); net.add_transition(std::nullopt, {4, 5, 6}, {7});
  minera::PetriUsageComparisonOptions options;
  options.place_names = {"start", "I", "Q", "R", "X", "Y", "Z", "end"};
  options.walk_through_hidden_trans = false;
  for (const bool clean : {false, true}) {
    options.cleaning_token_flood = clean;
    const auto result = minera::compare_petri_usage(net, log_of({"D", "B"}), log_of({}), options);
    assert(result.transitions[3].log1_occurrences == 1 && result.transitions[4].log1_occurrences == 1);
    assert(result.transitions[5].log1_occurrences == (clean ? 0 : 1));
  }
}
}  // namespace

// Reference compare_element_usage_two_logs returns one record per arc object:
// the sequence A -> B with arc weights 1, 2 and 3 always has four arc records,
// while two genuine parallel arcs stay two records (six in total).
std::string weighted_sequence_pnml(int weight, bool parallel) {
  const auto w = std::to_string(weight);
  std::string xml = "<?xml version=\"1.0\"?><pnml><net id=\"n\" type=\"http://www.pnml.org/version-2009/grammar/pnmlcoremodel\"><page id=\"pg\">"
      "<place id=\"p0\"><name><text>p0</text></name><initialMarking><text>" + w + "</text></initialMarking></place>"
      "<place id=\"p1\"><name><text>p1</text></name></place><place id=\"p2\"><name><text>p2</text></name></place>"
      "<transition id=\"t1\"><name><text>A</text></name></transition><transition id=\"t2\"><name><text>B</text></name></transition>";
  const auto arc = [&](std::string id, std::string source, std::string target, bool weighted) {
    xml += "<arc id=\"" + id + "\" source=\"" + source + "\" target=\"" + target + "\">";
    if (weighted) xml += "<inscription><text>" + w + "</text></inscription>";
    xml += "</arc>";
  };
  arc("a1", "p0", "t1", true);
  arc("a2", "t1", "p1", true);
  arc("a3", "p1", "t2", true);
  arc("a4", "t2", "p2", true);
  if (parallel) {
    arc("a1b", "p0", "t1", false);
    arc("a2b", "t1", "p1", false);
  }
  xml += "</page><finalmarkings><marking><place idref=\"p2\"><text>" + w +
         "</text></place></marking></finalmarkings></net></pnml>";
  return xml;
}

void weighted_arcs_keep_physical_arc_multiplicity() {
  const auto two_cases = [] {
    auto log = log_of({"A", "B"});
    log.traces.push_back(log.traces.front());
    log.traces.back().case_id = "case2";
    return log;
  }();
  const auto arcs_of = [&](int weight, bool parallel) {
    const auto document = minera::parse_pnml_document(weighted_sequence_pnml(weight, parallel));
    const auto options = minera::tools::pnml_comparison_options(document);
    return minera::compare_petri_usage_reference(document.net, two_cases, two_cases, options).arcs;
  };
  for (const int weight : {1, 2, 3}) {
    const auto arcs = arcs_of(weight, false);
    assert(arcs.size() == 4);
    for (const auto& arc : arcs) {
      assert(arc.log1_occurrences == 2 && arc.log2_occurrences == 2 && arc.total_occurrences == 4);
      assert(arc.label == (arc.transition == 0 ? "A (2/2/4)" : "B (2/2/4)"));
    }
  }
  const auto parallel = arcs_of(1, true);
  assert(parallel.size() == 6);
  const auto count = [&](std::size_t transition, minera::PetriArcUsageKind kind) {
    return std::count_if(parallel.begin(), parallel.end(),
                         [&](const auto& arc) { return arc.transition == transition && arc.kind == kind; });
  };
  assert(count(0, minera::PetriArcUsageKind::kInput) == 2);
  assert(count(0, minera::PetriArcUsageKind::kOutput) == 2);
  assert(count(1, minera::PetriArcUsageKind::kInput) == 1);
  assert(count(1, minera::PetriArcUsageKind::kOutput) == 1);
}

int main() {
  cleaning_uses_duplicate_place_names();
  cleaning_preserves_a_newly_produced_name_alias();
  cleaning_combines_nested_split_branches();
  public_zero_marking_keys_contract();
  public_pre_event_prefix_and_duplicate_contract();
  public_pre_event_weighted_and_path_limit();
  public_final_routing_pass_limit();
  public_partial_final_routing_contract();
  token_flood_component_budget_is_reported();
  token_flood_cleaning_respects_public_depth_boundary();
  token_flood_cleaning_preserves_usage_accounting();
  public_weighted_forced_replenishment_contract();
  public_static_color_and_null_activity_contract();
  null_activity_resolution_preserves_projection_priority();
  comparison_module_contract();
  public_dynamic_color_reference_contract();
  public_forced_repeat_place_counts();
  usage_directions_and_silent_activation();
  weighted_arc_and_duplicate_label_contract();
  finish_routes_when_sink_has_a_token_and_other_work_remains();
  explicit_markings_control_replay_boundaries();
  replay_controls_are_honored();
  extended_arcs_are_replayed_and_counted();
  inhibitor_arcs_gate_silent_completion();
  marking_dimensions_are_checked();
  custom_activity_attribute_controls_replay();
  aliases_prefer_first_class_fields_and_reject_blank_selectors();
  imported_custom_activity_attributes_are_replayable();
  imported_first_class_aliases_work_without_default_activity_columns();
  imported_powl_reference_uses_all_cached_selectors();
  xes_exact_selector_beats_alias_in_any_order();
  invalid_net_indices_are_rejected();
  silent_search_budget_is_reported();
  weighted_arcs_keep_physical_arc_multiplicity();
}
