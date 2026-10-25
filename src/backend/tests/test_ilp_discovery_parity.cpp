// Replays the PM4Py 2.7.22 ILP discovery bank (docs/ilp-discovery-bank.json.gz)
// against reference_ilp_net.
//
// Calls the capture classifies as `exact` (every pair LP rounds the same at
// every optimum) must reproduce the reference net exactly: transition names
// and labels, place names, producers and consumers. For `tied` calls the
// reference's choice follows its LP solver's vertex, so each numbered
// reference place must instead be an optimal rounding of a contained causal
// pair's LP, and so must each numbered native place. Errors must match by
// class and message.
#include "minera/ilp_reference.hpp"

#include <cstddef>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace minera;

const char* const kLines[] = {
#include "ilp_discovery_bank_cases.inc"
};

struct Outcome {
  std::string error;  // "<class> <message>" or empty
  std::set<std::pair<std::string, std::string>> transitions;
  std::set<std::string> places;  // "name producers consumers"
  std::vector<std::vector<std::string>> numbered;  // name, producers..., "|", consumers...
};

struct BankCase {
  std::string name;
  std::string model;
  ReferenceIlpOptions options;
  std::vector<std::vector<std::string>> traces;
  std::vector<Outcome> outcomes;
};

std::vector<std::string> split_commas(const std::string& text) {
  std::vector<std::string> parts;
  if (text == "-") return parts;
  std::string part;
  std::istringstream stream(text);
  while (std::getline(stream, part, ',')) parts.push_back(part);
  return parts;
}

std::vector<BankCase> cases() {
  std::vector<BankCase> result;
  for (const char* raw : kLines) {
    std::istringstream stream(raw);
    std::string tag;
    stream >> tag;
    if (tag == "C") {
      auto& item = result.emplace_back();
      std::string alpha;
      stream >> item.name >> item.model >> alpha;
      item.options.alpha = std::stod(alpha);
      continue;
    }
    auto& current = result.back();
    if (tag == "S") {
      stream >> current.options.artificial_start_activity >> current.options.artificial_end_activity;
    } else if (tag == "P") {
      std::string a, b;
      stream >> a >> b;
      if (!current.options.causal_relation) current.options.causal_relation.emplace();
      current.options.causal_relation->emplace_back(a, b);
    } else if (tag == "T") {
      auto& trace = current.traces.emplace_back();
      for (std::string activity; stream >> activity;) trace.push_back(activity);
    } else if (tag == "O") {
      current.outcomes.emplace_back();
    } else if (tag == "E") {
      std::string rest;
      std::getline(stream, rest);
      current.outcomes.back().error = rest.substr(1);
    } else if (tag == "X") {
      std::string name, label;
      stream >> name >> label;
      current.outcomes.back().transitions.emplace(name, label);
    } else if (tag == "Q") {
      std::string name, inputs, outputs;
      stream >> name >> inputs >> outputs;
      auto& outcome = current.outcomes.back();
      outcome.places.insert(name + " " + inputs + " " + outputs);
      if (name != "source" && name != "sink") {
        std::vector<std::string> entry{name};
        for (auto& input : split_commas(inputs)) entry.push_back(input);
        entry.push_back("|");
        for (auto& output : split_commas(outputs)) entry.push_back(output);
        outcome.numbered.push_back(std::move(entry));
      }
    }
  }
  return result;
}

std::string join(const std::vector<std::string>& parts) {
  if (parts.empty()) return "-";
  std::string text;
  for (const auto& part : parts) text += (text.empty() ? "" : ",") + part;
  return text;
}

Outcome describe(const ReferenceIlpNet& net) {
  Outcome outcome;
  for (const auto& transition : net.transitions)
    outcome.transitions.emplace(transition.name, transition.label.value_or("-"));
  for (const auto& place : net.places) {
    outcome.places.insert(place.name + " " + join(place.inputs) + " " + join(place.outputs));
    if (place.name == "source" || place.name == "sink") continue;
    std::vector<std::string> entry{place.name};
    entry.insert(entry.end(), place.inputs.begin(), place.inputs.end());
    entry.push_back("|");
    entry.insert(entry.end(), place.outputs.begin(), place.outputs.end());
    outcome.numbered.push_back(std::move(entry));
  }
  return outcome;
}

// True when every numbered place is an optimal rounding of some contained
// causal pair.
bool places_are_optimal_roundings(const BankCase& item, const Outcome& outcome) {
  const auto pairs = reference_ilp_causal_pairs(item.traces, item.options);
  for (const auto& entry : outcome.numbered) {
    std::vector<std::string> inputs, outputs;
    bool consumers = false;
    for (std::size_t k = 1; k < entry.size(); ++k) {
      if (entry[k] == "|") consumers = true;
      else (consumers ? outputs : inputs).push_back(entry[k]);
    }
    const std::set<std::string> in(inputs.begin(), inputs.end()), out(outputs.begin(), outputs.end());
    bool found = false;
    for (const auto& pair : pairs) {
      if (!in.count(pair.first) || !out.count(pair.second)) continue;
      if (reference_ilp_rounding_is_optimal(item.traces, item.options, pair, inputs, outputs)) {
        found = true;
        break;
      }
    }
    if (!found) return false;
  }
  return true;
}

std::size_t exact_cases = 0, tied_cases = 0, tied_equal = 0, error_cases = 0;

std::string check(const BankCase& item) {
  Outcome actual;
  try {
    actual = describe(reference_ilp_net(item.traces, item.options));
  } catch (const ReferenceIlpError& error) {
    actual.error = error.python_type + " " + error.what();
  } catch (const std::exception& error) {
    return std::string{"native exception "} + error.what();
  }
  const auto& reference = item.outcomes.front();
  if (!reference.error.empty() || !actual.error.empty()) {
    ++error_cases;
    if (reference.error == actual.error) return {};
    return "error: native '" + actual.error + "' reference '" + reference.error + "'";
  }
  const bool equal = actual.transitions == reference.transitions && actual.places == reference.places;
  if (item.model == "exact") {
    ++exact_cases;
    if (equal) return {};
    std::string text = "net differs; native:";
    for (const auto& place : actual.places) text += " [" + place + "]";
    text += " reference:";
    for (const auto& place : reference.places) text += " [" + place + "]";
    return text;
  }
  ++tied_cases;
  if (equal) ++tied_equal;
  if (!places_are_optimal_roundings(item, reference)) return "reference place is not an optimal rounding";
  if (!places_are_optimal_roundings(item, actual)) return "native place is not an optimal rounding";
  return {};
}
}  // namespace

int main() {
  std::size_t calls = 0, failures = 0;
  for (const auto& item : cases()) {
    ++calls;
    if (item.outcomes.size() != 1) {
      ++failures;
      std::cerr << item.name << ": reference varies across hash seeds\n";
      continue;
    }
    const auto failure = check(item);
    if (failure.empty()) continue;
    ++failures;
    std::cerr << item.name << " [" << item.model << "]: " << failure << '\n';
  }
  std::cout << calls << " calls compared: " << exact_cases << " exact nets, " << tied_cases
            << " tied (" << tied_equal << " equal anyway), " << error_cases << " errors, "
            << failures << " failures\n";
  return failures || calls < 1100 || exact_cases < 340 ? 1 : 0;
}
