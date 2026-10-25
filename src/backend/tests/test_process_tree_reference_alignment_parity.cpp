// algo.conformance.alignments.process_tree against public seeded banks.
// DYNAMIC_PROGRAMMING (docs/process-tree-dp-alignment-bank.txt.gz,
// scripts/capture-process-tree-dp-alignment-bank.py): every trace's cost,
// alignment and fitness must equal the reference's, except a pinned set of
// reference cache artefacts: alignments whose log moves do not spell the
// trace, and (rarer) results whose cost or tie choice the cache-free model
// does not reproduce. The pinned tallies make any change visible.
#include "minera/process_tree_reference_alignment.hpp"

#include "../gzip_codec.hpp"

#include <cmath>
#include <cstdio>
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
  std::string name, tree;
  Json traces, record;
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
    } else if (line[0] == 'L') {
      cases.back().traces = JsonReader(line.c_str() + 2).value();
    } else if (line[0] == 'R') {
      cases.back().record = JsonReader(line.c_str() + 2).value();
    }
  }
  return cases;
}

std::vector<std::vector<std::string>> traces_of(const Json& traces) {
  std::vector<std::vector<std::string>> out;
  for (const auto& trace : traces.items) {
    auto& events = out.emplace_back();
    for (const auto& event : trace.items) events.push_back(event.text);
  }
  return out;
}

std::string moves_text(const std::vector<ProcessTreeReferenceMove>& moves) {
  std::string out;
  for (const auto& move : moves) out += "(" + move.log + "," + move.model + ")";
  return out;
}

std::string moves_text(const Json& moves) {
  std::string out;
  for (const auto& move : moves.items) out += "(" + move.items[0].text + "," + move.items[1].text + ")";
  return out;
}

std::vector<std::string> log_projection(const Json& moves) {
  std::vector<std::string> out;
  for (const auto& move : moves.items)
    if (move.items[0].text != ">>") out.push_back(move.items[0].text);
  return out;
}

struct Tally {
  std::size_t traces = 0, exact = 0, invalid_reference = 0, ties = 0, other = 0, errors = 0;
};

bool verbose = false;

std::string check_dp(const BankCase& item, Tally& tally) {
  const auto traces = traces_of(item.traces);
  if (const auto* error = item.record.get("error")) {
    try {
      (void)reference_process_tree_dp_alignments(parse_tree(item.tree), traces);
    } catch (const ProcessTreeReferenceAlignmentError& raised) {
      if (raised.what() == error->items[1].text) {
        ++tally.errors;
        return {};
      }
      return std::string("\n  raised ") + raised.what() + ", expected " + error->items[1].text;
    }
    return "\n  expected " + error->items[0].text + " " + error->items[1].text;
  }
  const auto actual = reference_process_tree_dp_alignments(parse_tree(item.tree), traces);
  std::string failures;
  for (std::size_t t = 0; t < traces.size(); ++t) {
    ++tally.traces;
    const auto& expected = item.record.items[t];
    const bool cost = actual[t].cost == expected.items[0].number &&
                      std::fabs(actual[t].fitness - expected.items[2].number) <= 1e-12;
    const bool moves = moves_text(actual[t].moves) == moves_text(expected.items[1]);
    if (cost && moves) {
      ++tally.exact;
      continue;
    }
    // The reference returned another segment's alignment from a cache; its
    // cost then belongs to that alignment too.
    if (log_projection(expected.items[1]) != traces[t]) {
      ++tally.invalid_reference;
      continue;
    }
    if (cost) {
      ++tally.ties;
    } else {
      ++tally.other;
    }
    if (verbose) {
      char buffer[96];
      std::snprintf(buffer, sizeof buffer, " cost %g/%g fitness %.17g/%.17g", actual[t].cost,
                    expected.items[0].number, actual[t].fitness, expected.items[2].number);
      std::cerr << item.name << " trace " << t << buffer << "\n    expected " << moves_text(expected.items[1])
                << "\n    actual   " << moves_text(actual[t].moves) << '\n';
    }
  }
  return failures;
}

}  // namespace

int main(int argc, char** argv) try {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " DP-BANK.txt.gz [--verbose]\n";
    return 2;
  }
  verbose = argc > 2 && std::string(argv[2]) == "--verbose";
  Tally tally;
  std::size_t failures = 0;
  for (const auto& item : read_bank(argv[1])) {
    std::string failure;
    try {
      failure = check_dp(item, tally);
    } catch (const std::exception& error) {
      failure = std::string("\n  raised ") + error.what();
    }
    if (failure.empty()) continue;
    if (++failures <= 20) std::cerr << item.name << ":" << failure << '\n';
  }
  std::cout << "DYNAMIC_PROGRAMMING: " << tally.traces << " traces, " << tally.exact << " exact, "
            << tally.invalid_reference << " invalid reference alignments, " << tally.ties << " ties, "
            << tally.other << " other cost differences, " << tally.errors << " reference errors; " << failures
            << " failing cases\n";
  const bool pinned = tally.traces == 24000 && tally.exact == 23915 && tally.invalid_reference == 62 &&
                      tally.ties == 18 && tally.other == 5;
  if (!pinned) std::cerr << "tallies differ from the pinned 24000/23915/62/18/5\n";
  return failures || !pinned ? 1 : 0;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << "\n";
  return 1;
}
