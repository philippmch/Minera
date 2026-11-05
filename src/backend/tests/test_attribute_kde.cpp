#include "minera/attribute_kde.hpp"
#include "minera/json_export.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <vector>

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

bool close(double actual, double expected, double relative) {
  return std::abs(actual - expected) <= relative * std::abs(expected);
}

// pm4py.statistics.attributes.pandas.get on the formatted DataFrame of the
// same rows (pm4py 2.7.22, scipy 1.17.1). Numeric grids are exact; date
// densities carry the reference covariance's BLAS summation, which moves a
// narrow kernel over epoch seconds by about ulp(max x) / bandwidth.
void reference_values() {
  AttributeKdeOptions numeric;
  numeric.attribute_key = "amount";
  numeric.graph_points = 16;
  const auto kde = discover_attribute_kde(sample(), numeric);
  // linspace(1, 4, 8) and geomspace(1, 4, 8) share both ends exactly.
  assert(kde.x.size() == 14);
  assert(kde.x[1] == 1.2190136542044754 && kde.x[2] == 1.4285714285714286);
  assert(close(kde.y[0], 0.19165564463735402, 1e-12));
  assert(close(kde.y[1], 0.20355737912394883, 1e-12));
  assert(close(kde.y.back(), 0.14256473531710823, 1e-12));

  AttributeKdeOptions date;
  date.attribute_key = "time:timestamp";
  date.variant = AttributeKdeVariant::kDate;
  date.graph_points = 9;
  const auto dates = discover_attribute_kde(sample(), date);
  assert(dates.x.size() == 9 && dates.x[0] == 1767225600.0 &&
         dates.x[1] == 1767247200.0);
  // Three values fill the 400 sample points as 134/133/133 copies.
  assert(close(dates.y[0], 6.269073484897337e-06, 1e-9));
  assert(close(dates.y[1], 3.8136207202398048e-06, 1e-9));
  assert(close(dates.y[2], 1.6042583554229242e-06, 1e-9));

  // Unsorted, tied and sub-second rows: the DataFrame path picks its sample
  // positions from the sorted column (floor(i * n / k) when k >= n, else
  // n // 2k + floor(i * n / k) plus both ends), not from log order.
  const std::vector<std::string> times{
      "2026-01-01 00:00:41.5",  "2026-01-01 00:00:03.25",
      "2026-01-01 00:00:59",    "2026-01-01 00:00:03.25",
      "2026-01-01 00:00:17.125", "2026-01-01 00:00:30",
      "2026-01-01 00:00:08"};
  EventLog unsorted;
  unsorted.traces.push_back(Trace{"0", {}});
  unsorted.traces.push_back(Trace{"1", {}});
  for (std::size_t i = 0; i < times.size(); ++i)
    unsorted.traces[i % 2].events.push_back(event("A", times[i]));
  date.attribute_key = "timestamp";
  date.graph_points = 5;
  date.points_to_sample = 3;
  const auto down = discover_attribute_kde(unsorted, date);
  const std::vector<double> down_y{0.01271115886355768, 0.013107546187352857,
                                   0.011027233068801138, 0.009563724030103943,
                                   0.007582052088297745};
  assert(down.y.size() == down_y.size());
  for (std::size_t i = 0; i < down_y.size(); ++i)
    assert(close(down.y[i], down_y[i], 1e-7));
  assert(close(down.x[0], 1767225603.25, 1e-15) &&
         close(down.x[1], 1767225617.1875, 1e-15));
  date.graph_points = 4;
  date.points_to_sample = 10;
  const auto up = discover_attribute_kde(unsorted, date);
  const std::vector<double> up_y{0.018281095994285613, 0.01569084390033751,
                                 0.009637824300561672, 0.004837889223825191};
  for (std::size_t i = 0; i < up_y.size(); ++i)
    assert(close(up.y[i], up_y[i], 1e-7));

  // A key no event carries is not a column: the reference raises KeyError.
  bool rejected = false;
  try {
    AttributeKdeOptions absent;
    absent.attribute_key = "absent";
    (void)discover_attribute_kde(sample(), absent);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

}  // namespace

int main() {
  reference_values();
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
