// Replays the PM4Py 2.7.22 contextual label-splitting bank
// (docs/label-splitting-bank.json.gz) against the native splitter.
#include "minera/label_splitting.hpp"

#include <cstddef>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace minera;

const char* const kLines[] = {
#include "label_splitting_bank_cases.inc"
};

struct BankCase {
  std::string name;
  std::vector<std::vector<std::string>> traces;
  // Per configuration, every observed outcome: labels, or "!Error".
  std::map<std::string, std::vector<std::vector<std::string>>> outcomes;
};

std::vector<std::string> words(const std::string& line) {
  std::istringstream stream(line);
  std::vector<std::string> result;
  for (std::string word; stream >> word;) result.push_back(word);
  return result;
}

std::vector<BankCase> cases() {
  std::vector<BankCase> result;
  for (const char* raw : kLines) {
    auto parts = words(raw);
    if (parts[0] == "C") {
      result.push_back({parts[1], {}, {}});
    } else if (parts[0] == "T") {
      result.back().traces.push_back({parts.begin() + 1, parts.end()});
    } else {
      result.back().outcomes[parts[1]].push_back({parts.begin() + 2, parts.end()});
    }
  }
  return result;
}

LabelSplittingOptions options_for(const std::string& config) {
  LabelSplittingOptions options;
  if (config == "short") options.prefix_length = options.suffix_length = 1;
  if (config == "long") options.prefix_length = options.suffix_length = 3;
  if (config == "no_prefix") {
    options.prefix_length = 0;
    options.suffix_length = 2;
  }
  if (config == "weight_half") options.min_edge_weight = 0.5;
  if (config == "weight_high") options.min_edge_weight = 0.8;
  if (config == "targets") options.target_activities = {"a", "b"};
  if (config == "column") {
    options.target_key = "split";
    options.activities_suffix = "#";
  }
  return options;
}

// Returns an empty string when the native labels match an observed outcome.
std::string split_case(const BankCase& item, const std::string& config) {
  EventLog log;
  std::size_t time = 0;
  for (std::size_t index = 0; index < item.traces.size(); ++index) {
    Trace trace;
    trace.case_id = "c" + std::to_string(index);
    for (const auto& activity : item.traces[index])
      trace.events.push_back(Event{trace.case_id, activity, "", "", time++});
    log.traces.push_back(std::move(trace));
  }
  const auto options = options_for(config);
  std::vector<std::string> labels;
  try {
    const auto result = split_activity_labels(log, options);
    for (const auto& trace : result.log.traces)
      for (const auto& event : trace.events) {
        if (options.target_key == "activity") {
          labels.push_back(event.activity);
        } else {
          const auto* value = event.attribute(options.target_key);
          labels.push_back(value ? *value : "<missing>");
        }
      }
  } catch (const std::exception& error) {
    labels = {std::string{"!native:"} + error.what()};
  }
  const auto& observed = item.outcomes.at(config);
  for (const auto& outcome : observed)
    if (outcome == labels) return {};
  std::string message = "native";
  for (const auto& label : labels) message += " " + label;
  message += " | reference";
  for (const auto& label : observed.front()) message += " " + label;
  return message;
}
}  // namespace

int main() {
  std::size_t calls = 0, failures = 0;
  for (const auto& item : cases()) {
    for (const auto& [config, outcomes] : item.outcomes) {
      ++calls;
      const auto failure = split_case(item, config);
      if (failure.empty()) continue;
      ++failures;
      std::cerr << item.name << " [" << config << "]: " << failure << '\n';
    }
  }
  std::cout << calls << " calls compared\n";
  if (failures || calls < 1600) {
    std::cerr << failures << " mismatches\n";
    return 1;
  }
  return 0;
}
