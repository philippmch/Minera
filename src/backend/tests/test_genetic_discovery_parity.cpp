// Replays the PM4Py 2.7.22 genetic discovery bank
// (docs/genetic-discovery-bank.json.gz) against genetic_reference.hpp.
//
// Deterministic calls must match exactly: workflow sources/sinks, the causal
// matrix to Petri net conversion (place names in seed-independent form),
// partition edits, tournament fitness and sort order. The evolutionary
// operators and apply are seeded-random and string-hash dependent, so their
// outputs are checked by invariants: individuals/crossover/mutate give
// well-formed matrices, repair keeps every link mirrored, and each apply net is
// the conversion of the matrix it encodes, with the fitness of the last
// generation's best individual.
#include "minera/genetic_reference.hpp"

#include <algorithm>
#include <cmath>
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
#include "genetic_discovery_bank_cases.inc"
};

std::vector<std::string> words(const std::string& line) {
  std::vector<std::string> result;
  std::istringstream stream(line);
  for (std::string word; stream >> word;) result.push_back(word);
  return result;
}

std::vector<std::string> split(const std::string& text, char separator) {
  std::vector<std::string> parts;
  if (text == "-") return parts;
  std::string part;
  std::istringstream stream(text);
  while (std::getline(stream, part, separator)) parts.push_back(part);
  return parts;
}

std::vector<std::string> split_commas(const std::string& text) { return split(text, ','); }

struct NetShape {
  std::multiset<std::string> places;
  std::multiset<std::string> transitions;  // "label inputs outputs"
};

struct BankCase {
  std::string name, kind, op, key, transition;
  std::vector<ReferenceGeneticMatrix> matrices;  // one, or the population
  std::vector<ReferenceGeneticReplayOrder> orders;  // aligned with matrices
  ReferenceGeneticMap map, expected_map;
  std::vector<std::vector<std::string>> traces;
  std::vector<std::string> sources, sinks, initial, final_places;
  NetShape net;
  std::string error;
  std::vector<double> fitness, sorted_fitness;
  std::vector<std::size_t> order;
  double best = 0.0;
};

ReferenceGeneticPartitions partitions_of(const std::vector<std::string>& tokens) {
  ReferenceGeneticPartitions result;
  for (std::size_t i = 2; i < tokens.size(); ++i) result.push_back(split_commas(tokens[i]));
  return result;
}

std::vector<double> numbers(const std::vector<std::string>& tokens) {
  std::vector<double> result;
  for (std::size_t i = 1; i < tokens.size(); ++i) result.push_back(std::stod(tokens[i]));
  return result;
}

std::vector<BankCase> cases() {
  std::vector<BankCase> result;
  for (const char* raw : kLines) {
    const std::string line(raw);
    const auto tokens = words(line);
    const auto& tag = tokens.at(0);
    if (tag == "C") {
      auto& item = result.emplace_back();
      item.name = tokens.at(1);
      item.kind = tokens.at(2);
      if (tokens.size() > 3) item.op = tokens[3];
      if (tokens.size() > 4) item.key = tokens[4];
      if (tokens.size() > 5) item.transition = tokens[5];
      if (item.kind != "fitness") {
        item.matrices.emplace_back();
        item.orders.emplace_back();
      }
      continue;
    }
    auto& current = result.back();
    std::vector<std::string> rest(tokens.begin() + 1, tokens.end());
    if (tag == "G") {
      current.matrices.emplace_back();
      current.orders.emplace_back();
    } else if (tag == "A") {
      auto& keys = current.orders.back().arcs[tokens.at(1)];
      for (auto key : split(tokens.at(2), '|')) {
        std::replace(key.begin(), key.end(), '/', ' ');
        keys.push_back(key);
      }
    } else if (tag == "N") {
      auto name = tokens.at(2);
      std::replace(name.begin(), name.end(), '/', ' ');
      current.orders.back().place_names[tokens.at(1)] = name;
    } else if (tag == "T") {
      current.matrices.back().transitions = rest;
    } else if (tag == "I" && current.kind == "partition") {
      current.map[tokens.at(1)] = partitions_of(tokens);
    } else if (tag == "I") {
      current.matrices.back().inputs[tokens.at(1)] = partitions_of(tokens);
    } else if (tag == "O") {
      current.matrices.back().outputs[tokens.at(1)] = partitions_of(tokens);
    } else if (tag == "R") {
      current.expected_map[tokens.at(1)] = partitions_of(tokens);
    } else if (tag == "RS") {
      current.sources = rest;
    } else if (tag == "RK") {
      current.sinks = rest;
    } else if (tag == "P") {
      current.net.places.insert(tokens.at(1));
    } else if (tag == "X") {
      current.net.transitions.insert(tokens.at(1) + " " + tokens.at(2) + " " + tokens.at(3));
    } else if (tag == "M") {
      current.initial = rest;
    } else if (tag == "F") {
      current.final_places = rest;
    } else if (tag == "E") {
      current.error = line.substr(2);
    } else if (tag == "L") {
      current.traces.push_back(rest);
    } else if (tag == "V") {
      current.fitness = numbers(tokens);
    } else if (tag == "W") {
      current.sorted_fitness = numbers(tokens);
    } else if (tag == "Q") {
      for (const auto& value : rest) current.order.push_back(std::stoul(value));
    } else if (tag == "B") {
      current.best = std::stod(tokens.at(1));
    }
  }
  return result;
}

std::string joined(const std::vector<std::string>& values, char separator = ',') {
  if (values.empty()) return "-";
  std::string text;
  for (const auto& value : values) {
    if (!text.empty()) text += separator;
    text += value;
  }
  return text;
}

NetShape shape(const ReferenceGeneticNet& net) {
  NetShape result;
  std::vector<std::string> keys;
  for (const auto& place : net.places) {
    keys.push_back(reference_genetic_place_key(place));
    result.places.insert(keys.back());
  }
  for (const auto& transition : net.transitions) {
    std::vector<std::string> inputs, outputs;
    for (const auto id : transition.inputs) inputs.push_back(keys[id]);
    for (const auto id : transition.outputs) outputs.push_back(keys[id]);
    result.transitions.insert(transition.label.value_or("-") + " " + joined(inputs, ';') + " " +
                              joined(outputs, ';'));
  }
  return result;
}

std::string describe(const NetShape& net) {
  std::string text;
  for (const auto& place : net.places) text += " [" + place + "]";
  for (const auto& transition : net.transitions) text += " {" + transition + "}";
  return text;
}

EventLog event_log(const std::vector<std::vector<std::string>>& traces) {
  EventLog log;
  for (const auto& activities : traces) {
    Trace trace;
    for (const auto& activity : activities) trace.events.push_back(Event{"", activity});
    log.traces.push_back(std::move(trace));
  }
  return log;
}

bool close(double left, double right) { return std::fabs(left - right) <= 1e-12; }

// The matrix an apply net encodes: every visible transition's partitions are
// read back from the shared, input and output places it touches.
// The reference orders the transitions by first occurrence in the log.
ReferenceGeneticMatrix encoded_matrix(const NetShape& net,
                                      const std::vector<std::vector<std::string>>& traces) {
  ReferenceGeneticMatrix matrix;
  for (const auto& trace : traces)
    for (const auto& activity : trace)
      if (std::find(matrix.transitions.begin(), matrix.transitions.end(), activity) ==
          matrix.transitions.end())
        matrix.transitions.push_back(activity);
  std::map<std::string, std::set<ReferenceGeneticPartition>> inputs, outputs;
  for (const auto& transition : net.transitions) {
    const auto parts = words(transition);
    if (parts.at(0) == "-") continue;
    const auto& t = parts[0];
    inputs[t];
    outputs[t];
    // Shared places are "p:X:Y", input places "i:y:X", output places "o:x:Y".
    for (std::size_t side = 1; side <= 2; ++side) {
      for (const auto& key : split(parts[side], ';')) {
        if (key == "i" || key == "o") continue;
        const auto first = key.find(':'), second = key.find(':', first + 1);
        const auto kind = key.substr(0, first);
        const auto middle = split_commas(key.substr(first + 1, second - first - 1));
        const auto last = split_commas(key.substr(second + 1));
        if (side == 1) inputs[t].insert(kind == "p" ? middle : last);
        else outputs[t].insert(last);
      }
    }
  }
  for (const auto& t : matrix.transitions) {
    matrix.inputs[t].assign(inputs[t].begin(), inputs[t].end());
    matrix.outputs[t].assign(outputs[t].begin(), outputs[t].end());
  }
  return matrix;
}

// Every link of a repaired individual is mirrored: y in some O[x] iff x in
// some I[y]; partitions are nonempty and drawn from the transitions.
bool links_are_mirrored(const ReferenceGeneticMatrix& matrix) {
  const std::set<std::string> names(matrix.transitions.begin(), matrix.transitions.end());
  std::set<std::pair<std::string, std::string>> out_links, in_links;
  for (const auto& [x, partitions] : matrix.outputs)
    for (const auto& partition : partitions) {
      if (partition.empty()) return false;
      for (const auto& y : partition) {
        if (!names.count(y)) return false;
        out_links.emplace(x, y);
      }
    }
  for (const auto& [y, partitions] : matrix.inputs)
    for (const auto& partition : partitions) {
      if (partition.empty()) return false;
      for (const auto& x : partition) {
        if (!names.count(x)) return false;
        in_links.emplace(x, y);
      }
    }
  return out_links == in_links;
}

std::size_t counts[8] = {};

std::string check_partition(const BankCase& item) {
  ++counts[2];
  auto map = item.map;
  if (item.op == "add") reference_genetic_add_singleton_partition(map, item.key, item.transition);
  else reference_genetic_remove_transition_from_partitions(map, item.key, item.transition);
  for (auto& [key, partitions] : map)
    for (auto& partition : partitions) std::sort(partition.begin(), partition.end());
  return map == item.expected_map ? "" : "partition map differs";
}

std::string check(const BankCase& item) {
  const auto& matrix = item.matrices.front();
  if (item.kind == "sources") {
    ++counts[0];
    const auto actual = reference_genetic_sources_sinks(matrix);
    if (actual.sources == item.sources && actual.sinks == item.sinks) return {};
    return "sources " + joined(actual.sources) + " sinks " + joined(actual.sinks) +
           " expected " + joined(item.sources) + " / " + joined(item.sinks);
  }
  if (item.kind == "net") {
    ++counts[1];
    std::string error;
    NetShape actual;
    try {
      actual = shape(reference_genetic_net(matrix));
    } catch (const ReferenceGeneticError& e) {
      error = e.python_type + " " + e.what();
    }
    if (!item.error.empty() || !error.empty())
      return error == item.error ? "" : "error '" + error + "' expected '" + item.error + "'";
    if (item.initial != std::vector<std::string>{"i"} || item.final_places != std::vector<std::string>{"o"})
      return "unexpected reference markings";
    if (actual.places == item.net.places && actual.transitions == item.net.transitions) return {};
    return "net differs; native:" + describe(actual) + " reference:" + describe(item.net);
  }
  if (item.kind == "fitness") {
    ++counts[3];
    const auto log = event_log(item.traces);
    const auto unsorted = reference_genetic_tournament(item.matrices, log, false, &item.orders);
    const auto sorted = reference_genetic_tournament(item.matrices, log, true, &item.orders);
    for (std::size_t i = 0; i < item.fitness.size(); ++i)
      if (!close(unsorted.fitness.at(i), item.fitness[i]))
        return "fitness " + std::to_string(i) + " native " + std::to_string(unsorted.fitness[i]) +
               " reference " + std::to_string(item.fitness[i]);
    // The bank's last individual is the first one again; the reference
    // reports both by the first's position.
    auto order = sorted.order;
    for (auto& index : order)
      if (index + 1 == item.matrices.size()) index = 0;
    if (order != item.order) return "tournament order differs";
    for (std::size_t i = 0; i < item.sorted_fitness.size(); ++i)
      if (!close(sorted.fitness.at(i), item.sorted_fitness[i])) return "sorted fitness differs";
    return {};
  }
  if (item.kind == "operator") {
    ++counts[4];
    if (item.op == "repair")
      return links_are_mirrored(matrix) ? "" : "repaired links are not mirrored";
    return reference_genetic_matrix_is_well_formed(matrix) ? "" : item.op + " output is not well formed";
  }
  ++counts[5];
  if (!item.error.empty()) {
    ++counts[6];
    return {};
  }
  const auto encoded = encoded_matrix(item.net, item.traces);
  const auto rebuilt = shape(reference_genetic_net(encoded));
  if (rebuilt.places != item.net.places || rebuilt.transitions != item.net.transitions)
    return "apply net is not the conversion of its matrix; native:" + describe(rebuilt) +
           " reference:" + describe(item.net);
  const double fitness =
      reference_genetic_fitness(encoded, event_log(item.traces), &item.orders.front());
  if (!close(fitness, item.best))
    return "apply fitness " + std::to_string(fitness) + " best " + std::to_string(item.best);
  return {};
}
}  // namespace

int main() {
  std::size_t calls = 0, failures = 0;
  for (const auto& item : cases()) {
    ++calls;
    const auto failure = item.kind == "partition" ? check_partition(item) : check(item);
    if (failure.empty()) continue;
    ++failures;
    if (failures <= 40) std::cerr << item.name << ": " << failure << '\n';
  }
  std::cout << calls << " calls compared: " << counts[0] << " sources/sinks, " << counts[1]
            << " nets, " << counts[2] << " partition edits, " << counts[3] << " tournaments, "
            << counts[4] << " operator outputs, " << counts[5] << " applies (" << counts[6]
            << " reference errors), " << failures << " failures\n";
  return failures || calls < 5700 ? 1 : 0;
}
