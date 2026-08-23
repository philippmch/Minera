#include "minera/label_splitting.hpp"

#include <cassert>
#include <cmath>
#include <limits>
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
  assert(custom_result.log.traces[0].events[2].attribute("split") == nullptr);

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
}
