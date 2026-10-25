// algo.comparison.petrinet.element_usage_comparison against the public seeded
// bank (docs/element-usage-bank.txt.gz, scripts/capture-element-usage-bank.py).
// Each case replays two logs on a frozen PNML net; every place, transition and
// arc record (both counts, total, label, direction, color) must equal one of
// the outputs the reference showed under its hash seeds. Elements are named
// by PNML ids, arcs as "source->target" (a weighted arc is one record).
#include "minera/petri_usage_comparison.hpp"
#include "minera/pnml.hpp"

#include "../gzip_codec.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

std::string json_string(const std::string& text, std::size_t& at) {
  std::string out;
  for (++at; text[at] != '"'; ++at) {
    if (text[at] == '\\') {
      ++at;
      out += text[at] == 'n' ? '\n' : text[at] == 't' ? '\t' : text[at];
      continue;
    }
    out += text[at];
  }
  ++at;
  return out;
}

// One record row: kind, element, log1, log2, total, label, direction, color.
using Row = std::tuple<std::string, std::string, unsigned long long, unsigned long long, unsigned long long,
                       std::string, double, std::string>;
using Output = std::vector<Row>;  // sorted; an error is one row of kind "error"

Output parse_output(const std::string& text) {
  Output out;
  std::size_t at = 1;
  if (text.compare(0, 9, "[\"error\",") == 0) {
    at = 9;
    const auto type = json_string(text, at);
    ++at;
    out.push_back({"error", type, 0, 0, 0, json_string(text, at), 0.0, ""});
    return out;
  }
  while (text[at] == '[') {
    ++at;
    Row row;
    std::get<0>(row) = json_string(text, at);
    ++at;
    std::get<1>(row) = json_string(text, at);
    char* end = nullptr;
    std::get<2>(row) = std::strtoull(text.c_str() + at + 1, &end, 10);
    std::get<3>(row) = std::strtoull(end + 1, &end, 10);
    std::get<4>(row) = std::strtoull(end + 1, &end, 10);
    at = static_cast<std::size_t>(end - text.c_str()) + 1;
    std::get<5>(row) = json_string(text, at);
    std::get<6>(row) = std::strtod(text.c_str() + at + 1, &end);
    at = static_cast<std::size_t>(end - text.c_str()) + 1;
    std::get<7>(row) = json_string(text, at);
    at += 2;  // "]," or "]]"
    out.push_back(std::move(row));
  }
  std::sort(out.begin(), out.end());
  return out;
}

struct BankCase {
  std::string name;
  std::size_t net = 0;
  std::vector<std::vector<std::string>> log1, log2;
  std::vector<Output> outcomes;
};

struct Bank {
  std::map<std::size_t, std::string> nets;
  std::vector<BankCase> cases;
};

Bank read_bank(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error(std::string("cannot read ") + path);
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  std::istringstream text(detail::gzip_decompress(bytes, std::size_t{1} << 31));
  Bank bank;
  std::string line;
  while (std::getline(text, line)) {
    if (line.size() < 2) continue;
    const auto rest = line.substr(2);
    if (line[0] == 'N') {
      const auto space = rest.find(' ');
      std::size_t at = space + 1;
      bank.nets[std::stoul(rest.substr(0, space))] = json_string(rest, at);
    } else if (line[0] == 'C') {
      bank.cases.emplace_back();
      std::istringstream words(rest);
      words >> bank.cases.back().name >> bank.cases.back().net;
    } else if (line[0] == 'A' || line[0] == 'B') {
      std::vector<std::string> trace;
      std::istringstream words(rest);
      for (std::string word; words >> word;)
        if (word != "-") trace.push_back(word);
      (line[0] == 'A' ? bank.cases.back().log1 : bank.cases.back().log2).push_back(std::move(trace));
    } else if (line[0] == 'O') {
      bank.cases.back().outcomes.push_back(parse_output(rest));
    }
  }
  return bank;
}

EventLog event_log(const std::vector<std::vector<std::string>>& traces) {
  EventLog log;
  for (std::size_t index = 0; index < traces.size(); ++index) {
    Trace trace;
    trace.case_id = std::to_string(index);
    trace.case_id_state = FirstClassAttributeState::kPresent;
    std::size_t order = 0;
    for (const auto& activity : traces[index]) {
      Event event;
      event.case_id = trace.case_id;
      event.activity = activity;
      event.activity_state = FirstClassAttributeState::kPresent;
      event.input_order = order++;
      trace.events.push_back(std::move(event));
    }
    log.traces.push_back(std::move(trace));
  }
  return log;
}

std::size_t counts[2] = {};

// compare_element_usage_two_logs on one net and two logs.
std::string check_comparison(const Bank& bank, const BankCase& item) {
  ++counts[0];
  if (item.outcomes.size() > 1) ++counts[1];
  const auto document = parse_pnml_document(bank.nets.at(item.net));
  TokenReplayOptions replay;
  replay.transition_names = document.transition_ids;
  replay.place_names = document.place_ids;
  replay.exact_initial_marking = document.initial_marking;
  replay.exact_final_marking = document.final_marking;
  replay.place_outgoing_transition_order = pnml_place_outgoing_transition_order(document);
  Output actual;
  try {
    const auto result =
        compare_element_usage_token_replay(document.net, event_log(item.log1), event_log(item.log2), replay);
    const auto row = [](std::string kind, std::string element, const auto& value) {
      return Row{std::move(kind), std::move(element), value.log1_occurrences, value.log2_occurrences,
                 value.total_occurrences, value.label, value.direction, value.color};
    };
    std::set<Row> rows;
    for (const auto& place : result.places) rows.insert(row("place", document.place_ids.at(place.id), place));
    for (const auto& transition : result.transitions)
      rows.insert(row("transition", document.transition_ids.at(transition.id), transition));
    for (const auto& arc : result.arcs) {
      const auto& place = document.place_ids.at(arc.place);
      const auto& transition = document.transition_ids.at(arc.transition);
      rows.insert(row("arc", arc.input ? place + "->" + transition : transition + "->" + place, arc));
    }
    actual.assign(rows.begin(), rows.end());
  } catch (const std::exception& error) {
    actual.push_back({"error", "native", 0, 0, 0, error.what(), 0.0, ""});
  }
  for (const auto& outcome : item.outcomes)
    if (outcome == actual) return {};
  const auto& expected = item.outcomes.front();
  for (std::size_t i = 0; i < std::max(expected.size(), actual.size()); ++i) {
    const auto describe = [](const Output& output, std::size_t index) {
      if (index >= output.size()) return std::string("absent");
      const auto& [kind, element, log1, log2, total, label, direction, color] = output[index];
      return kind + " " + element + " " + std::to_string(log1) + "/" + std::to_string(log2) + "/" +
             std::to_string(total) + " '" + label + "' " + std::to_string(direction) + " " + color;
    };
    if (i >= expected.size() || i >= actual.size() || expected[i] != actual[i])
      return "\n  expected " + describe(expected, i) + "\n  actual   " + describe(actual, i);
  }
  return "differs";
}

}  // namespace

int main(int argc, char** argv) try {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " BANK.txt.gz\n";
    return 2;
  }
  const auto bank = read_bank(argv[1]);
  std::size_t failures = 0;
  for (const auto& item : bank.cases) {
    const auto failure = check_comparison(bank, item);
    if (failure.empty()) continue;
    if (++failures <= 20) std::cerr << item.name << ": " << failure << '\n';
  }
  std::cout << counts[0] << " comparisons (" << counts[1] << " with hash-seed dependent outputs), " << failures
            << " failures\n";
  return failures || counts[0] < 1900 ? 1 : 0;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << "\n";
  return 1;
}
