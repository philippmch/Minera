// Seeded PERFORMANCE DFG playout against the reference capture
// (scripts/capture-dfg-performance-playout-bank.py; 300 random DFGs in
// DataFrame-discovery order, identical under PYTHONHASHSEED 0, 1 and 2).
// Every case id, activity and microsecond timestamp must match.
// CLASSIC against scripts/capture-dfg-classic-playout-bank.py: random DFGs
// and parameters, each trace's activities and probability repr in log order.
#include "minera/dfg_playout.hpp"
#include "minera/timestamp.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

struct Case {
  std::string name;
  std::uint64_t seed = 0;
  std::size_t traces = 0;
  double rate = 1.0;
  DfgFrequency frequency;
  DfgPerformance performance;
  DfgActivityCounts starts, ends;
  std::vector<std::string> start_order;
  std::vector<std::vector<std::pair<std::string, std::int64_t>>> expected;
  bool error = false;
};

std::vector<Case> load(const char* path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error(std::string("cannot read ") + path);
  std::vector<Case> cases;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream fields(line);
    std::string tag;
    fields >> tag;
    if (tag == "C") {
      Case item;
      std::string rate;
      fields >> item.name >> item.seed >> item.traces >> rate;
      item.rate = std::stod(rate);
      cases.push_back(std::move(item));
      continue;
    }
    auto& item = cases.back();
    if (tag == "E") {
      std::string a, b, mean;
      std::uint64_t count = 0;
      fields >> a >> b >> count >> mean;
      item.frequency[{a, b}] = count;
      DfgPerformanceStatistics statistics;
      statistics.summary.mean = std::stod(mean);
      statistics.value = statistics.summary.mean;
      item.performance[{a, b}] = statistics;
    } else if (tag == "S") {
      std::string a;
      std::uint64_t count = 0;
      fields >> a >> count;
      item.starts[a] = count;
      item.start_order.push_back(a);
    } else if (tag == "N") {
      std::string a;
      std::uint64_t count = 0;
      fields >> a >> count;
      item.ends[a] = count;
    } else if (tag == "R") {
      auto& trace = item.expected.emplace_back();
      std::string cell;
      while (fields >> cell) {
        const auto at = cell.rfind('@');
        trace.emplace_back(cell.substr(0, at), std::stoll(cell.substr(at + 1)));
      }
    } else if (tag == "X") {
      item.error = true;
    }
  }
  return cases;
}

std::int64_t epoch_micros(const std::string& text) {
  // YYYY-MM-DDTHH:MM:SS.ffffffZ
  const auto seconds = parse_timestamp(text.substr(0, 19) + "Z");
  if (!seconds) throw std::runtime_error("unparsable timestamp " + text);
  return static_cast<std::int64_t>(*seconds) * 1000000 + std::stoll(text.substr(20, 6));
}

struct ClassicCase {
  std::string name;
  DfgPlayoutOptions options;
  DfgFrequency frequency;
  DfgActivityCounts starts, ends;
  std::vector<std::pair<std::string, std::vector<std::string>>> expected;
  bool error = false;
};

std::vector<ClassicCase> load_classic(const char* path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error(std::string("cannot read ") + path);
  std::vector<ClassicCase> cases;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream fields(line);
    std::string tag;
    fields >> tag;
    if (tag == "C") {
      ClassicCase item;
      std::string probability;
      int interrupt = 0, add_new = 0, only_complete = 0;
      fields >> item.name >> item.options.max_variants >> probability >>
          item.options.max_occurrences_per_activity >> interrupt >> add_new >>
          item.options.min_variant_occurrences >> only_complete;
      item.options.min_weighted_probability = std::stod(probability);
      item.options.interrupt_when_dfg_complete = interrupt != 0;
      item.options.add_trace_if_takes_new_edges = add_new != 0;
      item.options.return_only_if_complete = only_complete != 0;
      cases.push_back(std::move(item));
      continue;
    }
    auto& item = cases.back();
    std::string a, b;
    std::uint64_t count = 0;
    if (tag == "E") {
      fields >> a >> b >> count;
      item.frequency[{a, b}] = count;
    } else if (tag == "S") {
      fields >> a >> count;
      item.starts[a] = count;
    } else if (tag == "N") {
      fields >> a >> count;
      item.ends[a] = count;
    } else if (tag == "R") {
      auto& trace = item.expected.emplace_back();
      fields >> trace.first;
      while (fields >> a) trace.second.push_back(a);
    } else if (tag == "X") {
      item.error = true;
    }  // "Z": the reference returned None; Minera returns an empty log.
  }
  return cases;
}

int compare_classic(const char* path) {
  int failures = 0;
  std::size_t compared = 0, traces = 0, raised = 0;
  for (const auto& item : load_classic(path)) {
    // Declared: the reference raises KeyError when the search reaches an
    // activity with neither successors nor an end weight; Minera skips it.
    if (item.error) { ++raised; continue; }
    ++compared;
    std::string detail;
    try {
      const auto result = dfg_playout_result(item.frequency, item.starts, item.ends, item.options);
      if (result.log.traces.size() != item.expected.size())
        detail = "trace count " + std::to_string(result.log.traces.size()) + " instead of " +
                 std::to_string(item.expected.size());
      for (std::size_t i = 0; i < std::min(result.log.traces.size(), item.expected.size()); ++i) {
        const auto& trace = result.log.traces[i];
        std::vector<std::string> activities;
        for (const auto& event : trace.events) activities.push_back(event.activity);
        const auto* probability = trace.attribute("probability");
        if (trace.case_id != std::to_string(i) || activities != item.expected[i].second ||
            !probability || *probability != item.expected[i].first) {
          detail += " | trace " + std::to_string(i) + " expected " + item.expected[i].first;
          for (const auto& a : item.expected[i].second) detail += " " + a;
          detail += " got " + (probability ? *probability : std::string("-"));
          for (const auto& a : activities) detail += " " + a;
          break;
        }
        ++traces;
      }
    } catch (const std::exception& error) {
      detail = std::string("raised ") + error.what();
    }
    if (!detail.empty()) {
      ++failures;
      if (failures <= 20) std::fprintf(stderr, "%s: %s\n", item.name.c_str(), detail.substr(0, 800).c_str());
    }
  }
  std::printf("%zu CLASSIC DFG playouts compared (%zu traces, %zu reference errors skipped), %d failing\n",
              compared, traces, raised, failures);
  if (compared < 350) {
    ++failures;
    std::fprintf(stderr, "CLASSIC bank coverage below the pinned floor\n");
  }
  return failures;
}

}  // namespace

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "src/backend/tests/fixtures/dfg_performance_playout_bank.txt";
  int failures = 0;
  std::size_t compared = 0, traces = 0;
  for (const auto& item : load(path)) {
    ++compared;
    std::string detail;
    try {
      DfgPlayoutOptions options;
      options.variant = DfgPlayoutVariant::kPerformance;
      options.traces = item.traces;
      options.seed = item.seed;
      options.case_arrival_rate = item.rate;
      options.start_activity_order = item.start_order;
      const auto result = dfg_playout_result(item.frequency, item.starts, item.ends, options, &item.performance);
      if (item.error) detail = "expected a reference error";
      else if (result.log.traces.size() != item.expected.size()) detail = "trace count differs";
      for (std::size_t i = 0; detail.empty() && i < result.log.traces.size(); ++i) {
        const auto& trace = result.log.traces[i];
        if (trace.case_id != std::to_string(i)) detail = "case id " + trace.case_id;
        std::vector<std::pair<std::string, std::int64_t>> got;
        for (const auto& event : trace.events) got.emplace_back(event.activity, epoch_micros(event.timestamp));
        if (detail.empty() && got != item.expected[i]) {
          detail = "trace " + std::to_string(i) + " differs: expected";
          for (const auto& [a, t] : item.expected[i]) detail += " " + a + "@" + std::to_string(t);
          detail += " | got";
          for (const auto& [a, t] : got) detail += " " + a + "@" + std::to_string(t);
        }
        ++traces;
      }
    } catch (const std::exception& error) {
      if (!item.error) detail = std::string("raised ") + error.what();
    }
    if (!detail.empty()) {
      ++failures;
      if (failures <= 20) std::fprintf(stderr, "%s: %s\n", item.name.c_str(), detail.substr(0, 800).c_str());
    }
  }
  std::printf("%zu PERFORMANCE DFG playouts compared (%zu traces), %d failing\n", compared, traces, failures);
  if (compared < 300) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  failures += compare_classic(argc > 2 ? argv[2] : "src/backend/tests/fixtures/dfg_classic_playout_bank.txt");
  return failures == 0 ? 0 : 1;
}
