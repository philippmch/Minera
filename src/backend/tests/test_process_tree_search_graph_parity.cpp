// algo.conformance.alignments.process_tree SEARCH_GRAPH_PT helpers against
// the public seeded bank docs/process-tree-search-graph-bank.txt.gz
// (scripts/capture-process-tree-search-graph-bank.py):
// - search_graph_pt_replay_semantics: every recorded predicate, vertex move,
//   transform_tree and shortest path from a random walk's states must equal
//   the reference's path and state exactly;
// - search_graph_pt_frequency_annotation: the per-node num_events/num_cases
//   computed from the recorded search-state chains must equal what the
//   reference wrote, except where the reference search handed a state a path
//   that does not replay from its parent state (tallied separately).
#include "minera/process_tree_search_graph_reference.hpp"

#include "../gzip_codec.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

struct Json {
  enum Kind { kNull, kBool, kNumber, kString, kArray, kObject } kind = kNull;
  double number = 0;
  bool boolean = false;
  std::string text;
  std::vector<Json> items;
  std::map<std::string, Json> fields;
  [[nodiscard]] const Json* get(const std::string& key) const {
    const auto found = fields.find(key);
    return found == fields.end() ? nullptr : &found->second;
  }
};

class JsonReader {
 public:
  explicit JsonReader(const char* text) : at_(text) {}
  Json value() {
    space();
    Json out;
    if (*at_ == '{') {
      out.kind = Json::kObject;
      ++at_;
      for (space(); *at_ != '}'; space()) {
        auto key = value().text;
        space(); ++at_;  // ':'
        out.fields.emplace(std::move(key), value());
        space();
        if (*at_ == ',') ++at_;
      }
      ++at_;
    } else if (*at_ == '[') {
      out.kind = Json::kArray;
      ++at_;
      for (space(); *at_ != ']'; space()) {
        out.items.push_back(value());
        space();
        if (*at_ == ',') ++at_;
      }
      ++at_;
    } else if (*at_ == '"') {
      out.kind = Json::kString;
      for (++at_; *at_ != '"'; ++at_) {
        if (*at_ == '\\') {
          ++at_;
          out.text += *at_ == 'n' ? '\n' : *at_ == 't' ? '\t' : *at_;
          continue;
        }
        out.text += *at_;
      }
      ++at_;
    } else if (*at_ == 't' || *at_ == 'f') {
      out.kind = Json::kBool;
      out.boolean = *at_ == 't';
      at_ += out.boolean ? 4 : 5;
    } else if (*at_ == 'n') {
      at_ += 4;
    } else {
      out.kind = Json::kNumber;
      char* end = nullptr;
      out.number = std::strtod(at_, &end);
      at_ = end;
    }
    return out;
  }

 private:
  void space() { while (*at_ == ' ' || *at_ == '\n') ++at_; }
  const char* at_;
};

// Parses the reference's tree notation: 'label', tau, ->( ), X( ), +( ), *( ).
struct TreeParser {
  std::string text;
  std::size_t at = 0;
  std::vector<std::string> activities;

  void skip() {
    while (at < text.size() && (text[at] == ' ' || text[at] == ',')) ++at;
  }

  ProcessTreeNode node() {
    skip();
    if (text.compare(at, 3, "tau") == 0) {
      at += 3;
      return ProcessTreeNode::tau();
    }
    if (text[at] == '\'') {
      const auto end = text.find('\'', at + 1);
      const auto label = text.substr(at + 1, end - at - 1);
      at = end + 1;
      std::size_t index = 0;
      while (index < activities.size() && activities[index] != label) ++index;
      if (index == activities.size()) activities.push_back(label);
      return ProcessTreeNode::leaf(index);
    }
    ProcessTreeOperator op;
    if (text.compare(at, 2, "->") == 0) {
      op = ProcessTreeOperator::kSequence;
      at += 2;
    } else if (text[at] == 'X') {
      op = ProcessTreeOperator::kExclusive;
      ++at;
    } else if (text[at] == '+') {
      op = ProcessTreeOperator::kParallel;
      ++at;
    } else if (text[at] == '*') {
      op = ProcessTreeOperator::kLoop;
      ++at;
    } else {
      throw std::invalid_argument("unexpected tree text: " + text.substr(at));
    }
    skip();
    ++at;  // (
    ProcessTreeNode result{op, 0, {}};
    for (;;) {
      skip();
      if (text[at] == ')') {
        ++at;
        return result;
      }
      result.children.push_back(node());
    }
  }
};

ProcessTree parse_tree(const std::string& text) {
  TreeParser parser{text};
  auto root = parser.node();
  return ProcessTree(parser.activities, std::move(root));
}

struct BankCase {
  std::string name, tree, initial;
  Json records, annotation;
  bool annotated = false;
};

std::vector<BankCase> read_bank(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error(std::string("cannot read ") + path);
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  std::istringstream text(detail::gzip_decompress(bytes, std::size_t{1} << 31));
  std::vector<BankCase> cases;
  std::string line;
  while (std::getline(text, line)) {
    if (line.size() < 2) continue;
    if (line[0] == 'C') {
      cases.emplace_back();
      cases.back().name = line.substr(2);
    } else if (line[0] == 'T') {
      cases.back().tree = line.substr(2);
    } else if (line[0] == 'I') {
      cases.back().initial = line.substr(2);
    } else if (line[0] == 'R') {
      cases.back().records = JsonReader(line.c_str() + 2).value();
    } else if (line[0] == 'A') {
      cases.back().annotation = JsonReader(line.c_str() + 2).value();
      cases.back().annotated = true;
    }
  }
  return cases;
}

ProcessTreeReplayPath path_of(const Json& steps) {
  ProcessTreeReplayPath out;
  for (const auto& step : steps.items) {
    out.push_back({static_cast<std::size_t>(step.items[0].number), step.items[1].text[0]});
  }
  return out;
}

std::string path_text(const std::optional<ProcessTreeReplayPath>& path) {
  if (!path) return "None";
  std::string out;
  for (const auto& step : *path) out += std::to_string(step.node) + step.state + " ";
  return out;
}

struct Tally {
  std::size_t probes = 0, annotated = 0, inconsistent = 0, inconsistent_differ = 0;
  std::map<std::string, std::size_t> per_op;
};

std::string replay_semantics_case(const BankCase& item, Tally& tally) {
  const ProcessTreeReplaySemantics replay(parse_tree(item.tree));
  if (replay.initial_state() != item.initial) return "\n  initial state " + replay.initial_state();
  std::string failures;
  for (const auto& record : item.records.items) {
    const std::string& state = record.items[0].text;
    const auto node = static_cast<std::size_t>(record.items[1].number);
    const std::string& op = record.items[2].text;
    ++tally.probes;
    ++tally.per_op[op.substr(0, op.find(':'))];
    if (record.items[3].kind == Json::kBool) {
      const bool expected = record.items[3].boolean;
      const bool actual = op == "can_enable" ? replay.can_enable(node, state)
                          : op == "can_open" ? replay.can_open(node, state)
                                             : replay.can_close(node, state);
      if (actual != expected) failures += "\n  " + op + " " + std::to_string(node) + " on " + state;
      continue;
    }
    ProcessTreeReplayResult actual;
    if (op.rfind("transform_tree:", 0) == 0) {
      actual = replay.transform_tree(node, op.back(), state);
    } else if (op == "enable_vertex") {
      actual = replay.enable_vertex(node, state);
    } else if (op == "open_vertex") {
      actual = replay.open_vertex(node, state);
    } else if (op == "close_vertex") {
      actual = replay.close_vertex(node, state);
    } else if (op == "shortest_path_to_open") {
      actual = replay.shortest_path_to_open(node, state);
    } else if (op == "shortest_path_to_enable") {
      actual = replay.shortest_path_to_enable(node, state);
    } else if (op == "shortest_path_to_close") {
      actual = replay.shortest_path_to_close(node, state);
    } else {
      return "\n  unknown op " + op;
    }
    std::optional<ProcessTreeReplayPath> expected_path;
    std::string expected_state;
    if (record.items[3].kind == Json::kArray) {
      expected_path = path_of(record.items[3]);
      expected_state = record.items[4].text;
    }
    if (actual.path != expected_path || (expected_path && actual.state != expected_state)) {
      failures += "\n  " + op + " " + std::to_string(node) + " on " + state + "\n    expected " +
                  path_text(expected_path) + "-> " + expected_state + "\n    actual   " + path_text(actual.path) +
                  "-> " + actual.state;
    }
  }
  return failures;
}

// True when every search state but the last (whose path leaves out the final
// closing) is its parent's state with its path applied.
bool chains_replay(const Json& alignments) {
  for (const auto& chain : alignments.items) {
    for (std::size_t k = 1; k + 1 < chain.items.size(); ++k) {
      std::string state = chain.items[k - 1].items[0].text;
      for (const auto& step : path_of(chain.items[k].items[1])) state[step.node] = step.state;
      if (state != chain.items[k].items[0].text) return false;
    }
  }
  return true;
}

std::string frequency_annotation_case(const BankCase& item, Tally& tally) {
  if (!item.annotated) return {};
  ++tally.annotated;
  std::vector<std::vector<ProcessTreeSearchSegment>> alignments;
  for (const auto& chain : item.annotation.items[0].items) {
    auto& segments = alignments.emplace_back();
    for (const auto& segment : chain.items) segments.push_back({segment.items[0].text, path_of(segment.items[1])});
  }
  const auto actual = reference_search_graph_frequency_annotation(parse_tree(item.tree), alignments);
  const auto& expected = item.annotation.items[1].items;
  bool same = actual.size() == expected.size();
  for (std::size_t n = 0; same && n < expected.size(); ++n) {
    if (expected[n].items[0].kind == Json::kNull) {
      same = !actual[n].has_value();
    } else {
      same = actual[n] && actual[n]->num_events == static_cast<long long>(expected[n].items[0].number) &&
             actual[n]->num_cases == static_cast<long long>(expected[n].items[1].number);
    }
  }
  const bool consistent = chains_replay(item.annotation.items[0]);
  if (!consistent) {
    ++tally.inconsistent;
    if (!same) ++tally.inconsistent_differ;
    return {};
  }
  if (same) return {};
  std::string text = "\n  annotation differs:";
  for (std::size_t n = 0; n < actual.size(); ++n) {
    text += " " + (actual[n] ? std::to_string(actual[n]->num_events) + "/" + std::to_string(actual[n]->num_cases)
                             : std::string("-"));
  }
  return text;
}

}  // namespace

int main(int argc, char** argv) try {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " SEARCH-GRAPH-BANK.txt.gz\n";
    return 2;
  }
  Tally tally;
  std::size_t failures = 0;
  for (const auto& item : read_bank(argv[1])) {
    std::string failure;
    try {
      failure = replay_semantics_case(item, tally) + frequency_annotation_case(item, tally);
    } catch (const std::exception& error) {
      failure = std::string("\n  raised ") + error.what();
    }
    if (failure.empty()) continue;
    if (++failures <= 20) std::cerr << item.name << " " << item.tree << ":" << failure << '\n';
  }
  std::string ops;
  for (const auto& [op, count] : tally.per_op) ops += (ops.empty() ? "" : ", ") + op + " " + std::to_string(count);
  std::cout << "search-graph replay: " << tally.probes << " probes (" << ops << "); frequency annotation: " << tally.annotated << " searches, " << tally.inconsistent
            << " with non-replaying search paths (" << tally.inconsistent_differ << " differing); " << failures
            << " failing cases\n";
  return failures ? 1 : 0;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << "\n";
  return 1;
}
