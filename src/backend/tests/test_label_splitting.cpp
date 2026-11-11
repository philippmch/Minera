#include "minera/label_splitting.hpp"
#include "minera/xes_export.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>

using namespace minera;

static EventLog fixture() {
  EventLog log;
  Trace first;
  first.events = {Event{"c1", "A", "", "", 0}, Event{"c1", "B", "", "", 1},
                  Event{"c1", "A", "", "", 2}, Event{"c1", "C", "", "", 3}};
  Trace second;
  second.events = {Event{"c2", "D", "", "", 4}, Event{"c2", "A", "", "", 5},
                   Event{"c2", "E", "", "", 6}};
  log.traces = {first, second};
  return log;
}

static EventLog token_boundary_fixture() {
  EventLog log;
  Trace first;
  first.events = {Event{"c1", "aa", "", "", 0},
                  Event{"c1", "b", "", "", 1},
                  Event{"c1", "A", "", "", 2}};
  Trace second;
  second.events = {Event{"c2", "a", "", "", 3},
                   Event{"c2", "ab", "", "", 4},
                   Event{"c2", "A", "", "", 5}};
  log.traces = {first, second};
  return log;
}

static EventLog custom_key_fixture() {
  EventLog log;
  Trace first;
  first.events = {
      Event{"c1", "raw-left", "", "", 0, {{"kind", "left"}}, ""},
      Event{"c1", "raw-A", "", "", 1,
            {{"kind", "A"}, {"split", "original"}}, ""},
      Event{"c1", "raw-B", "", "", 2, {{"kind", "B"}}, ""}};
  Trace second;
  second.events = {
      Event{"c2", "raw-right", "", "", 3, {{"kind", "right"}}, ""},
      Event{"c2", "raw-A", "", "", 4,
            {{"kind", "A"}, {"split", "original"}}, ""},
      Event{"c2", "raw-B", "", "", 5, {{"kind", "B"}}, ""}};
  log.traces = {first, second};
  return log;
}

static EventLog edge_threshold_fixture() {
  EventLog log;
  Trace first;
  first.events = {Event{"c1", "x", "", "", 0},
                  Event{"c1", "y", "", "", 1},
                  Event{"c1", "A", "", "", 2}};
  Trace second;
  second.events = {Event{"c2", "x", "", "", 3},
                   Event{"c2", "z", "", "", 4},
                   Event{"c2", "A", "", "", 5}};
  log.traces = {first, second};
  return log;
}

static EventLog weighted_gain_fixture() {
  EventLog log;
  const std::vector<std::vector<std::string>> prefixes = {
      {"a", "b", "c"}, {"a", "b", "d"}, {"a", "e", "f"},
      {"g", "h", "i"}};
  for (std::size_t i = 0; i < prefixes.size(); ++i) {
    Trace trace;
    std::size_t timestamp = i * 4;
    for (const auto& activity : prefixes[i])
      trace.events.push_back(Event{"c" + std::to_string(i), activity, "", "", timestamp++});
    trace.events.push_back(Event{"c" + std::to_string(i), "A", "", "", timestamp});
    log.traces.push_back(std::move(trace));
  }
  return log;
}

static EventLog zero_edge_fixture() {
  EventLog log;
  Trace first;
  first.events = {Event{"c1", "a", "", "", 0},
                  Event{"c1", "b", "", "", 1},
                  Event{"c1", "A", "", "", 2}};
  Trace second;
  second.events = {Event{"c2", "x", "", "", 3},
                   Event{"c2", "y", "", "", 4},
                   Event{"c2", "A", "", "", 5}};
  log.traces = {first, second};
  return log;
}

static EventLog tied_edges_fixture() {
  EventLog log;
  const std::vector<std::vector<std::string>> prefixes = {
      {"a", "a"}, {"a", "b"}, {"x", "x"}, {"x", "y"}};
  for (std::size_t i = 0; i < prefixes.size(); ++i) {
    Trace trace;
    trace.events.push_back(Event{"c" + std::to_string(i), prefixes[i][0], "", "", i * 3});
    trace.events.push_back(Event{"c" + std::to_string(i), prefixes[i][1], "", "", i * 3 + 1});
    trace.events.push_back(Event{"c" + std::to_string(i), "A", "", "", i * 3 + 2});
    log.traces.push_back(std::move(trace));
  }
  return log;
}

static EventLog global_modularity_fixture() {
  EventLog log;
  const std::vector<std::pair<std::string, std::string>> a_contexts = {
      {"la", "la"}, {"la", "lb"}, {"la", "lc"},
      {"ra", "lb"}, {"ra", "rb"}, {"ra", "rc"}};
  const std::vector<std::pair<std::string, std::string>> b_contexts = {
      {"ba", "b0"}, {"ba", "b1"}, {"ba", "b2"}, {"ba", "b3"},
      {"ba", "b4"}, {"ba", "b5"}, {"ba", "b6"}, {"ba", "b7"}};
  std::size_t case_index = 0;
  for (const auto& [left, right] : a_contexts) {
    Trace trace;
    const auto case_id = "a" + std::to_string(case_index++);
    trace.events = {Event{case_id, left, "", "", 0},
                    Event{case_id, right, "", "", 1},
                    Event{case_id, "A", "", "", 2}};
    log.traces.push_back(std::move(trace));
  }
  for (const auto& [left, right] : b_contexts) {
    Trace trace;
    const auto case_id = "b" + std::to_string(case_index++);
    trace.events = {Event{case_id, left, "", "", 0},
                    Event{case_id, right, "", "", 1},
                    Event{case_id, "B", "", "", 2}};
    log.traces.push_back(std::move(trace));
  }
  return log;
}

static EventLog cross_activity_context_fixture() {
  EventLog log;
  const std::vector<std::pair<std::string, std::string>> a_contexts = {
      {"same", "left"}, {"same", "right"}, {"same", "third"}};
  const std::vector<std::pair<std::string, std::string>> b_contexts = {
      {"same", "right"}, {"same", "third"}, {"same", "left"}};
  std::size_t case_index = 0;
  for (const auto& [label, contexts] :
       {std::pair<std::string, std::vector<std::pair<std::string, std::string>>>{
           "A", a_contexts},
        {"B", b_contexts}}) {
    for (const auto& [left, right] : contexts) {
      Trace trace;
      const auto case_id = "x" + std::to_string(case_index++);
      trace.events = {Event{case_id, left, "", "", 0},
                      Event{case_id, right, "", "", 1},
                      Event{case_id, label, "", "", 2}};
      log.traces.push_back(std::move(trace));
    }
  }
  return log;
}

int main() {
  const auto log = fixture();
  LabelSplittingOptions options;
  options.prefix_length = 1;
  options.suffix_length = 1;
  const auto result = split_activity_labels(log, options);
  assert(result.log.traces.size() == 2);
  assert(result.log.traces[0].events.size() == 4);
  assert(result.log.traces[0].events[0].activity.rfind("A_", 0) == 0);
  assert(result.log.traces[0].events[2].activity.rfind("A_", 0) == 0);
  assert(result.log.traces[1].events[1].activity.rfind("A_", 0) == 0);

  options.target_activities = {"B"};
  const auto targeted = split_activity_labels(log, options);
  assert(targeted.log.traces[0].events[0].activity == "A");
  assert(targeted.log.traces[0].events[1].activity == "B");
  options.min_edge_weight = std::numeric_limits<double>::quiet_NaN();
  bool threw = false;
  try { (void)split_activity_labels(log, options); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);

  // Context distance is measured over activity tokens, not the characters in
  // their labels. "aa,b" and "a,ab" must therefore remain separate even
  // though their concatenated text happens to be identical.
  LabelSplittingOptions token_options;
  token_options.prefix_length = 2;
  token_options.suffix_length = 0;
  const auto token_result =
      split_activity_labels(token_boundary_fixture(), token_options);
  const auto& first_a = token_result.log.traces[0].events.back().activity;
  const auto& second_a = token_result.log.traces[1].events.back().activity;
  assert(first_a == "A_1");
  assert(second_a == "A_0");

  // A custom classifier can read one attribute while writing another. The
  // canonical activity and non-target events must remain untouched.
  LabelSplittingOptions custom_options;
  custom_options.activity_key = "kind";
  custom_options.target_key = "split";
  custom_options.target_activities = {"A"};
  custom_options.prefix_length = 1;
  custom_options.suffix_length = 0;
  const auto custom_result =
      split_activity_labels(custom_key_fixture(), custom_options);
  assert(custom_result.log.traces[0].events[0].activity == "raw-left");
  assert(custom_result.log.traces[0].events[1].activity == "raw-A");
  assert(custom_result.log.traces[0].events[1].attribute("split") != nullptr);
  assert(*custom_result.log.traces[0].events[1].attribute("split") == "A_1");
  assert(*custom_result.log.traces[1].events[1].attribute("split") == "A_0");
  // Like the reference's target column, unrelabeled events copy their label.
  assert(*custom_result.log.traces[0].events[2].attribute("split") ==
         *custom_result.log.traces[0].events[2].attribute("kind"));

  // The configured edge minimum is strict: a similarity equal to the
  // threshold is excluded, while a lower threshold joins the two segments.
  LabelSplittingOptions threshold_options;
  threshold_options.prefix_length = 2;
  threshold_options.suffix_length = 0;
  threshold_options.min_edge_weight = 0.5;
  const auto strict_result =
      split_activity_labels(edge_threshold_fixture(), threshold_options);
  assert(strict_result.log.traces[0].events.back().activity == "A_1");
  assert(strict_result.log.traces[1].events.back().activity == "A_0");
  threshold_options.min_edge_weight = 0.49;
  const auto relaxed_result =
      split_activity_labels(edge_threshold_fixture(), threshold_options);
  assert(relaxed_result.log.traces[0].events.back().activity == "A");
  assert(relaxed_result.log.traces[1].events.back().activity == "A");

  // Weighted modularity chooses the strong first edge and then evaluates the
  // merged community using the summed inter-community weight. The first three
  // contexts consequently form one community, while the isolated context
  // remains separate.
  LabelSplittingOptions weighted_options;
  weighted_options.prefix_length = 3;
  weighted_options.suffix_length = 0;
  const auto weighted_result =
      split_activity_labels(weighted_gain_fixture(), weighted_options);
  const auto weighted_first =
      weighted_result.log.traces[0].events.back().activity;
  assert(weighted_first ==
         weighted_result.log.traces[1].events.back().activity);
  assert(weighted_first ==
         weighted_result.log.traces[2].events.back().activity);
  assert(weighted_first !=
         weighted_result.log.traces[3].events.back().activity);

  // A graph without edges must leave every context as a singleton. This is
  // also a regression for the empty-edge path before modularity clustering.
  LabelSplittingOptions zero_edge_options;
  zero_edge_options.prefix_length = 2;
  zero_edge_options.suffix_length = 0;
  const auto zero_edge_result =
      split_activity_labels(zero_edge_fixture(), zero_edge_options);
  assert(zero_edge_result.log.traces[0].events.back().activity !=
         zero_edge_result.log.traces[1].events.back().activity);

  // Equal modularity gains use the context/node ordering as a stable tie
  // break. Repeating the same input must reproduce both assignments exactly.
  LabelSplittingOptions tie_options;
  tie_options.prefix_length = 2;
  tie_options.suffix_length = 0;
  const auto tied_result =
      split_activity_labels(tied_edges_fixture(), tie_options);
  const auto tied_repeat =
      split_activity_labels(tied_edges_fixture(), tie_options);
  for (std::size_t i = 0; i < tied_result.log.traces.size(); ++i)
    assert(tied_result.log.traces[i].events.back().activity ==
           tied_repeat.log.traces[i].events.back().activity);
  assert(tied_result.log.traces[0].events.back().activity == "A_1");
  assert(tied_result.log.traces[1].events.back().activity == "A_1");
  assert(tied_result.log.traces[2].events.back().activity == "A_0");
  assert(tied_result.log.traces[3].events.back().activity == "A_0");

  // Modularity is optimized on one graph containing all selected activities.
  // The A component has two dense groups joined by a weak bridge; it stays
  // split when selected alone, but the heavy B component changes the global
  // expected-edge term and makes that bridge merge.
  const auto global_log = global_modularity_fixture();
  LabelSplittingOptions local_modularity_options;
  local_modularity_options.prefix_length = 2;
  local_modularity_options.suffix_length = 0;
  local_modularity_options.target_activities = {"A"};
  const auto local_modularity =
      split_activity_labels(global_log, local_modularity_options);
  assert(local_modularity.log.traces[0].events.back().activity !=
         local_modularity.log.traces[3].events.back().activity);
  assert(local_modularity.log.traces[0].events.back().activity != "A");

  local_modularity_options.target_activities = {"A", "B"};
  const auto global_modularity =
      split_activity_labels(global_log, local_modularity_options);
  for (std::size_t i = 0; i < 6; ++i)
    assert(global_modularity.log.traces[i].events.back().activity == "A");

  // Similar contexts belonging to different activities are not graph edges.
  // With a strict threshold, each activity is edgeless even though the
  // context pairs are identical across activities; the assignments therefore
  // follow each activity's own insertion order.
  LabelSplittingOptions cross_activity_options;
  cross_activity_options.prefix_length = 2;
  cross_activity_options.suffix_length = 0;
  cross_activity_options.min_edge_weight = 0.75;
  cross_activity_options.target_activities = {"A", "B"};
  const auto cross_activity =
      split_activity_labels(cross_activity_context_fixture(),
                             cross_activity_options);
  assert(cross_activity.log.traces[0].events.back().activity == "A_2");
  assert(cross_activity.log.traces[1].events.back().activity == "A_1");
  assert(cross_activity.log.traces[2].events.back().activity == "A_0");

  // A changed label must survive an actual typed XES export/import cycle.
  auto typed_log = custom_key_fixture();
  for (auto& trace : typed_log.traces) {
    auto& attribute = trace.events[1].attributes[1];
    attribute.kind = AttributeKind::kInt;
    attribute.value = "7";
    attribute.typed_value = std::int64_t{7};
  }
  LabelSplittingOptions typed_options;
  typed_options.activity_key = "kind";
  typed_options.target_key = "split";
  typed_options.prefix_length = 1;
  typed_options.suffix_length = 0;
  typed_options.min_edge_weight = 0.5;
  typed_options.target_activities = {"A"};
  const auto typed_split = split_activity_labels(typed_log, typed_options);
  std::istringstream typed_xml(serialize_xes(typed_split.log));
  const auto typed_reopened = import_xes_event_log(typed_xml);
  for (std::size_t i = 0; i < typed_split.log.traces.size(); ++i) {
    const auto* expected = typed_split.log.traces[i].events[1].attribute("split");
    assert(expected && expected->rfind("A_", 0) == 0);
    assert(typed_reopened.traces[i].events[1].attribute("split") &&
           *typed_reopened.traces[i].events[1].attribute("split") == *expected);
  }

  // An imported custom first-class activity key must follow its new label.
  auto selected_log = custom_key_fixture();
  for (auto& trace : selected_log.traces) for (auto& event : trace.events) {
    event.activity = *event.attribute("kind");
    event.activity_source_key = "kind";
    event.activity_state = FirstClassAttributeState::kPresent;
    event.activity_attribute = EventAttribute{"kind", event.activity, AttributeKind::kString};
    std::erase_if(event.attributes, [](const auto& attribute) { return attribute.key == "kind"; });
  }
  auto selected_options = typed_options;
  selected_options.activity_key = "activity";
  selected_options.target_key = "kind";
  const auto selected_split = split_activity_labels(selected_log, selected_options);
  XesImportOptions selected_import; selected_import.activity_attribute = "kind";
  std::istringstream selected_xml(serialize_xes(selected_split.log));
  const auto selected_reopened = import_xes_event_log(selected_xml, selected_import);
  for (std::size_t i = 0; i < selected_split.log.traces.size(); ++i) {
    assert(selected_split.log.traces[i].events[1].activity.rfind("A_", 0) == 0);
    assert(selected_reopened.traces[i].events[1].activity == selected_split.log.traces[i].events[1].activity);
  }
}
