// Generates the bundled order-to-cash sample event log (deterministic).

#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

// A uniform integer in [lo, hi] (span below 2^32) from 32-bit MT19937
// outputs by Lemire's multiply-and-reject method. Spelled out rather than
// left to std::uniform_int_distribution, whose algorithm the standard does
// not fix; it reproduces the bundled data/sample_event_log.csv, which was
// generated with this method.
template <class Int>
Int uniform(std::mt19937& engine, Int lo, Int hi) {
  const auto range = static_cast<std::uint32_t>(hi - lo) + 1U;
  std::uint64_t product = static_cast<std::uint64_t>(engine()) * range;
  auto low = static_cast<std::uint32_t>(product);
  if (low < range) {
    const std::uint32_t threshold = (0U - range) % range;
    while (low < threshold) {
      product = static_cast<std::uint64_t>(engine()) * range;
      low = static_cast<std::uint32_t>(product);
    }
  }
  return lo + static_cast<Int>(product >> 32U);
}

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
  char buffer[64];
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
  const auto usage = [&](std::ostream& stream) {
    stream << "Usage: " << argv[0] << " <output.csv>\n"
           << "Writes the deterministic order-to-cash sample event log to <output.csv>.\n";
  };
  if (argc == 2) {
    const std::string argument = argv[1];
    if (argument == "-h" || argument == "--help") {
      usage(std::cout);
      return 0;
    }
    // An option-looking argument is never taken as the output file name, so a
    // mistyped flag cannot create a file called "--verbose" in the cwd.
    if (!argument.empty() && argument.front() == '-') {
      std::cerr << "error: unknown option " << argument << '\n';
      usage(std::cerr);
      return 2;
    }
  }
  if (argc != 2) {
    usage(std::cerr);
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
  const auto pick = [&] { return uniform<std::size_t>(rng, 0, population.size() - 1); };
  const auto start_offset = [&] { return uniform<long>(rng, 0, 120L * 24 * 60); };
  const auto step_gap = [&] { return uniform<long>(rng, 30, 36L * 60); };

  out << "case_id,activity,timestamp,resource\n";
  for (int id = 1; id <= kCaseCount; ++id) {
    const auto& steps = population[pick()]->steps;
    long minutes = 4L * 24 * 60 + start_offset();
    char case_id[16];
    std::snprintf(case_id, sizeof(case_id), "ORD-%04d", id);
    for (const auto& step : steps) {
      out << case_id << ',' << step << ',' << to_timestamp(minutes) << ','
          << resource_for(step) << '\n';
      minutes += step_gap();
    }
  }

  return 0;
}
