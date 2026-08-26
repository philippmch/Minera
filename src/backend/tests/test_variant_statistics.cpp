#include "minera/json_export.hpp"
#include "minera/variant_statistics.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

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

int main() {
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
