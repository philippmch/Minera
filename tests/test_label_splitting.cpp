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
}
