// Generates the bundled order-to-cash sample event log (deterministic).

#include <array>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

struct Variant {
  int weight;
  std::vector<std::string> steps;
};

const std::array<Variant, 7> kVariants = {{
    {45, {"Receive Order", "Check Credit", "Approve Order", "Pick Items",
          "Pack Shipment", "Ship Order", "Send Invoice", "Receive Payment"}},
    {14, {"Receive Order", "Check Credit", "Approve Order", "Pick Items",
          "Pack Shipment", "Ship Order", "Send Invoice", "Send Reminder",
          "Receive Payment"}},
    {12, {"Receive Order", "Check Credit", "Request Credit Review",
          "Check Credit", "Approve Order", "Pick Items", "Pack Shipment",
          "Ship Order", "Send Invoice", "Receive Payment"}},
    {10, {"Receive Order", "Check Credit", "Approve Order", "Amend Order",
          "Approve Order", "Pick Items", "Pack Shipment", "Ship Order",
          "Send Invoice", "Receive Payment"}},
    {8, {"Receive Order", "Check Credit", "Reject Order"}},
    {6, {"Receive Order", "Check Credit", "Approve Order", "Cancel Order"}},
    {5, {"Receive Order", "Check Credit", "Approve Order", "Pick Items",
         "Pack Shipment", "Ship Order", "Send Invoice", "Send Reminder",
         "Send Reminder", "Receive Payment"}},
}};

constexpr int kCaseCount = 320;

// Minutes since 2026-01-01T00:00 rendered as an ISO-8601 timestamp; long
// enough horizons only ever land in January–June 2026.
std::string to_timestamp(long minutes) {
  static constexpr std::array<int, 6> days_in_month = {31, 28, 31, 30, 31, 30};
  int day_index = static_cast<int>(minutes / (24 * 60));
  const int minute_of_day = static_cast<int>(minutes % (24 * 60));
  int month = 0;
  while (day_index >= days_in_month[month]) {
    day_index -= days_in_month[month];
    ++month;
  }
  char buffer[24];
  std::snprintf(buffer, sizeof(buffer), "2026-%02d-%02dT%02d:%02d:00",
                month + 1, day_index + 1, minute_of_day / 60,
                minute_of_day % 60);
  return buffer;
}

// The team that performs each activity, so the sample carries an organizational
// perspective (handover-of-work between roles) alongside its timing.
std::string resource_for(const std::string& activity) {
  if (activity == "Receive Order") return "Sales";
  if (activity == "Check Credit" || activity == "Request Credit Review") {
    return "Finance";
  }
  if (activity == "Approve Order" || activity == "Reject Order" ||
      activity == "Amend Order" || activity == "Cancel Order") {
    return "Manager";
  }
  if (activity == "Pick Items" || activity == "Pack Shipment") {
    return "Warehouse";
  }
  if (activity == "Ship Order") return "Logistics";
  return "Billing";  // Send Invoice, Send Reminder, Receive Payment
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <output.csv>\n";
    return 2;
  }

  std::ofstream out(argv[1]);
  if (!out) {
    std::cerr << "error: cannot open output file " << argv[1] << '\n';
    return 1;
  }

  std::vector<const Variant*> population;
  for (const auto& variant : kVariants) {
    for (int i = 0; i < variant.weight; ++i) {
      population.push_back(&variant);
    }
  }

  std::mt19937 rng(17);
  std::uniform_int_distribution<std::size_t> pick(0, population.size() - 1);
  std::uniform_int_distribution<long> start_offset(0, 120L * 24 * 60);
  std::uniform_int_distribution<long> step_gap(30, 36L * 60);

  out << "case_id,activity,timestamp,resource\n";
  for (int id = 1; id <= kCaseCount; ++id) {
    const auto& steps = population[pick(rng)]->steps;
    long minutes = 4L * 24 * 60 + start_offset(rng);
    char case_id[16];
    std::snprintf(case_id, sizeof(case_id), "ORD-%04d", id);
    for (const auto& step : steps) {
      out << case_id << ',' << step << ',' << to_timestamp(minutes) << ','
          << resource_for(step) << '\n';
      minutes += step_gap(rng);
    }
  }

  return 0;
}
