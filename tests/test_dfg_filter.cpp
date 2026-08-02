#include "minera/dfg_filter.hpp"

#include <cassert>
#include <stdexcept>

namespace {

minera::DfgFilterData sample() {
  minera::EventLog log;
  log.traces = {
      {"1", {{"1", "a", "", "", 0}, {"1", "b", "", "", 1},
              {"1", "c", "", "", 2}, {"1", "d", "", "", 3}}, {}},
      {"2", {{"2", "a", "", "", 0}, {"2", "b", "", "", 1},
              {"2", "e", "", "", 2}, {"2", "d", "", "", 3}}, {}},
      {"3", {{"3", "a", "", "", 0}, {"3", "f", "", "", 1},
              {"3", "d", "", "", 2}}, {}},
      {"4", {{"4", "a", "", "", 0}, {"4", "g", "", "", 1},
              {"4", "h", "", "", 2}, {"4", "d", "", "", 3}}, {}},
  };
  return minera::make_dfg_filter_data(
      minera::discover_directly_follows_graph(log));
}

void assert_well_formed(const minera::DfgFilterData& data) {
  for (const auto& [edge, count] : data.edges) {
    assert(count > 0);
    assert(data.activities.count(edge.first));
    assert(data.activities.count(edge.second));
  }
  for (const auto& [activity, count] : data.start_activities) {
    assert(count > 0 && data.activities.count(activity));
  }
  for (const auto& [activity, count] : data.end_activities) {
    assert(count > 0 && data.activities.count(activity));
  }
}

}  // namespace

int main() {
  const auto data = sample();
  assert(data.activities.size() == 8);
  assert(data.start_activities.at("a") == 4);
  assert(data.end_activities.at("d") == 4);
  assert(data.edges.at({"a", "b"}) == 2);

  const auto activity_filtered =
      minera::filter_dfg_on_activities_percentage(data, 0.5);
  assert_well_formed(activity_filtered);
  assert(activity_filtered.activities.count("a"));
  assert(activity_filtered.activities.count("d"));

  const auto path_filtered =
      minera::filter_dfg_on_paths_percentage(data, 0.5, false);
  assert_well_formed(path_filtered);
  assert(path_filtered.activities.size() <= data.activities.size());
  assert(path_filtered.edges.size() <= data.edges.size());

  const auto dependency_filtered =
      minera::filter_dfg_keep_connected(data, 0.8);
  assert_well_formed(dependency_filtered);
  assert(!dependency_filtered.activities.count("f"));
  assert(!dependency_filtered.activities.count("g"));

  const auto to_c = minera::filter_dfg_to_activity(data, "c");
  assert(to_c.end_activities.size() == 1 && to_c.end_activities.count("c"));
  assert(to_c.start_activities.size() == 1 && to_c.start_activities.count("a"));
  assert(!to_c.activities.count("d"));

  const auto from_e = minera::filter_dfg_from_activity(data, "e");
  assert(from_e.start_activities.size() == 1 && from_e.start_activities.count("e"));
  assert(from_e.end_activities.size() == 1 && from_e.end_activities.count("d"));
  assert(!from_e.activities.count("a"));

  const auto containing_b = minera::filter_dfg_contain_activity(data, "b");
  assert(containing_b.activities.count("a"));
  assert(containing_b.activities.count("b"));
  assert(containing_b.activities.count("c"));
  assert(containing_b.activities.count("e"));
  assert(!containing_b.activities.count("f"));
  assert(!containing_b.activities.count("g"));

  bool rejected = false;
  try {
    (void)minera::filter_dfg_on_paths_percentage(data, 1.1);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  rejected = false;
  try {
    (void)minera::filter_dfg_to_activity(data, "missing");
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  return 0;
}
