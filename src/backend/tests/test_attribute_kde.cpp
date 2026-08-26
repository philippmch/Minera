#include "minera/attribute_kde.hpp"
#include "minera/json_export.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>

using namespace minera;

namespace {

Event event(std::string activity, std::string timestamp) {
  Event result;
  result.activity = std::move(activity);
  result.timestamp = std::move(timestamp);
  return result;
}

void attribute(Event& event, std::string key, std::string value) {
  event.attributes.push_back(
      {std::move(key), std::move(value), AttributeKind::kString});
}

EventLog sample() {
  auto first = event("A", "2026-01-01T00:00:00Z");
  attribute(first, "amount", "1");
  auto second = event("B", "2026-01-02T00:00:00Z");
  attribute(second, "amount", "2");
  auto third = event("C", "2026-01-03T00:00:00Z");
  attribute(third, "amount", "4");
  return EventLog{{Trace{"case-1", {std::move(first), std::move(second),
                                    std::move(third)}}}};
}

}  // namespace

int main() {
  AttributeKdeOptions numeric;
  numeric.attribute_key = "amount";
  numeric.graph_points = 16;
  const auto result = discover_attribute_kde(sample(), numeric);
  assert(result.variant == AttributeKdeVariant::kNumeric);
  assert(result.x.size() == result.y.size() && result.x.size() >= 2);
  assert(std::abs(result.x.front() - 1.0) < 1e-12);
  assert(std::abs(result.x.back() - 4.0) < 1e-12);
  for (const auto value : result.y)
    assert(std::isfinite(value) && value >= 0.0);
  const auto json = attribute_kde_json(result);
  assert(json.find("\"family\":\"ATTRIBUTE_KDE\"") != std::string::npos);
  assert(json.find("\"attribute_key\":\"amount\"") != std::string::npos);
  assert(attribute_kde_variant_name(AttributeKdeVariant::DATE) ==
         std::string("DATE"));

  AttributeKdeOptions date;
  date.attribute_key = "timestamp";
  date.variant = AttributeKdeVariant::kDate;
  date.graph_points = 9;
  const auto dates = discover_attribute_kde(sample(), date);
  assert(dates.x.size() == 9 && dates.x.size() == dates.y.size());
  assert(dates.x.front() < dates.x.back());

  auto single = sample();
  single.traces.front().events.resize(1);
  numeric.graph_points = 7;
  const auto spike = discover_attribute_kde(single, numeric);
  assert(spike.x.size() == 7 && spike.y[spike.y.size() / 2] == 1.0);

  bool rejected = false;
  try {
    AttributeKdeOptions bad = numeric;
    bad.attribute_key.clear();
    (void)discover_attribute_kde(sample(), bad);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  rejected = false;
  try {
    AttributeKdeOptions bad = numeric;
    bad.graph_points = 0;
    (void)discover_attribute_kde(sample(), bad);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  rejected = false;
  try {
    AttributeKdeOptions bad = numeric;
    bad.attribute_key = "amount";
    auto invalid = sample();
    invalid.traces.front().events.front().attributes.front().value = "NaN";
    (void)discover_attribute_kde(invalid, bad);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
  return 0;
}
