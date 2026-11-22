#include "minera/json_export.hpp"
#include "minera/variant_statistics.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

minera::Event event(std::string activity) {
  minera::Event value;
  value.activity = std::move(activity);
  value.attributes.push_back({"phase", value.activity,
                              minera::AttributeKind::kString});
  return value;
}

minera::EventLog fixture() {
  minera::EventLog log;
  minera::Trace first;
  first.events = {event("A"), event("B")};
  minera::Trace second;
  second.events = {event("A"), event("B")};
  minera::Trace third;
  third.events = {event("A")};
  minera::Trace empty;
  log.traces = {std::move(first), std::move(second), std::move(third),
                std::move(empty)};
  return log;
}

}  // namespace

// Round FV: pm4py's get_variants on a Polars frame sorts it by case and
// timestamp with the unstable pdqsort (the 22-row tie frame moves); reference
// from pm4py 2.7.22 (review-harness round-fv ut/decut.py).
static void polars_frame_variants_follow_the_polars_sort() {
  minera::EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  table.dtypes = {"str", "str", "datetime64[us, UTC]"};
  const char* rows[][3] = {
      {"c0", "C", "0"}, {"c1", "A", "1"}, {"c1", "B", "1"}, {"c0", "A", "1"}, {"c0", "B", "1"},
      {"c2", "A", "1"}, {"c1", "C", "0"}, {"c2", "A", "1"}, {"c0", "A", "0"}, {"c2", "C", "0"},
      {"c1", "C", "0"}, {"c1", "C", "0"}, {"c2", "A", "1"}, {"c1", "C", "0"}, {"c1", "A", "0"},
      {"c1", "B", "0"}, {"c1", "C", "0"}, {"c0", "C", "1"}, {"c0", "C", "1"}, {"c2", "C", "1"},
      {"c2", "C", "0"}, {"c1", "B", "1"}};
  for (const auto& row : rows)
    table.rows.push_back({minera::EventAttribute{"case:concept:name", row[0]},
                          minera::EventAttribute{"concept:name", row[1]},
                          minera::EventAttribute{"time:timestamp", std::string("2020-01-01T00:00:0") + row[2] + "Z",
                                                 minera::AttributeKind::kDate}});
  const auto result = minera::discover_variant_statistics(minera::LazyEventTable::from_table(table));
  assert(result.variants.size() == 3);
  const auto has = [&](std::vector<std::string> activities) {
    for (const auto& entry : result.variants)
      if (entry.activities == activities) return entry.count == 1;
    return false;
  };
  assert(has({"A", "C", "C", "C", "A", "B"}));
  assert(has({"B", "C", "C", "C", "C", "A", "C", "A", "B", "B"}));
  assert(has({"C", "C", "A", "A", "A", "C"}));
  auto missing = table;
  missing.rows[4][0]->is_null = true;
  bool refused = false;
  try {
    (void)minera::discover_variant_statistics(minera::LazyEventTable::from_table(missing));
  } catch (const std::invalid_argument& error) {
    refused = std::string(error.what()) == "the case ID column should not contain any empty value.";
  }
  assert(refused);
}

int main() {
  polars_frame_variants_follow_the_polars_sort();
  const auto log = fixture();
  const auto result = minera::discover_variant_statistics(log);
  assert(result.variants.size() == 3);
  assert((result.variants[0].activities == std::vector<std::string>{"A", "B"}));
  assert(result.variants[0].count == 2);
  assert(std::abs(result.variants[0].probability - 0.5) < 1e-9);
  assert(result.variants[2].activities.empty());
  assert(std::string(minera::variant_statistics_variant_name()) == "CLASSIC");
  const auto json = minera::variant_statistics_json(result);
  assert(json.find("\"family\":\"VARIANT_STATISTICS\"") !=
         std::string::npos);
  assert(json.find("\"probability\":0.5") != std::string::npos);

  minera::VariantStatisticsOptions options;
  options.activity_key = "phase";
  options.max_variants = 1;
  const auto limited = minera::discover_variant_statistics(log, options);
  assert(limited.variants.size() == 1);

  bool rejected = false;
  try {
    minera::VariantStatisticsOptions invalid;
    invalid.activity_key = "missing";
    (void)minera::discover_variant_statistics(log, invalid);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  std::cout << "Variant-statistics tests passed\n";
}
