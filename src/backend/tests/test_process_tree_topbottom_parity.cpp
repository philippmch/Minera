// Seeded TOPBOTTOM process-tree playout against the reference capture
// (scripts/capture-process-tree-topbottom-bank.py; 300 trees x 3 seeds x 10
// traces, identical under PYTHONHASHSEED 0, 1 and 2). Every trace must match
// the reference activity for activity, and the log carries no case id and no
// timestamp, as the reference log does.
#include "minera/process_tree_playout.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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
  std::string tree;
  std::vector<std::vector<std::string>> expected;
};

class TreeParser {
 public:
  explicit TreeParser(const std::string& text) : text_(text) {}

  ProcessTree parse() {
    auto root = node();
    if (at_ != text_.size()) throw std::runtime_error("trailing tree text: " + text_);
    return ProcessTree(activities_, std::move(root));
  }

 private:
  ProcessTreeNode node() {
    static const std::vector<std::pair<std::string, ProcessTreeOperator>> kOps = {
        {"->(", ProcessTreeOperator::kSequence}, {"<>(", ProcessTreeOperator::kInterleaving},
        {"X(", ProcessTreeOperator::kExclusive}, {"+(", ProcessTreeOperator::kParallel},
        {"*(", ProcessTreeOperator::kLoop}};
    for (const auto& [prefix, op] : kOps) {
      if (text_.compare(at_, prefix.size(), prefix) != 0) continue;
      at_ += prefix.size();
      ProcessTreeNode out{op, 0, {}};
      out.children.push_back(node());
      while (text_.at(at_) == ',') {
        ++at_;
        out.children.push_back(node());
      }
      if (text_.at(at_++) != ')') throw std::runtime_error("expected ) in " + text_);
      return out;
    }
    const auto end = text_.find_first_of(",)", at_);
    const auto label = text_.substr(at_, end == std::string::npos ? std::string::npos : end - at_);
    at_ = end == std::string::npos ? text_.size() : end;
    if (label == "tau") return ProcessTreeNode::tau();
    const auto found = std::find(activities_.begin(), activities_.end(), label);
    const auto index = static_cast<std::size_t>(found - activities_.begin());
    if (found == activities_.end()) activities_.push_back(label);
    return ProcessTreeNode::leaf(index);
  }

  const std::string& text_;
  std::size_t at_ = 0;
  std::vector<std::string> activities_;
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
      fields >> item.name >> item.seed >> item.traces;
      cases.push_back(std::move(item));
    } else if (tag == "T") {
      fields >> cases.back().tree;
    } else if (tag == "R") {
      auto& trace = cases.back().expected.emplace_back();
      std::string activity;
      while (fields >> activity) trace.push_back(activity);
    }
  }
  return cases;
}

}  // namespace

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "src/backend/tests/fixtures/process_tree_topbottom_bank.txt";
  int failures = 0;
  std::size_t compared = 0, traces = 0;
  for (const auto& item : load(path)) {
    ++compared;
    std::string detail;
    try {
      const auto tree = TreeParser(item.tree).parse();
      ProcessTreePlayoutOptions options;  // TOPBOTTOM is the default
      options.traces = item.traces;
      options.seed = item.seed;
      const auto log = process_tree_playout(tree, options);
      if (log.traces.size() != item.expected.size()) {
        detail = "trace count " + std::to_string(log.traces.size());
      }
      for (std::size_t i = 0; detail.empty() && i < log.traces.size(); ++i) {
        const auto& trace = log.traces[i];
        if (!trace.case_id.empty() || trace.has_case_id())
          detail = "trace " + std::to_string(i) + " has a case id";
        std::vector<std::string> word;
        for (const auto& event : trace.events) {
          if (!event.timestamp.empty() || event.has_timestamp())
            detail = "trace " + std::to_string(i) + " has a timestamp";
          word.push_back(event.activity);
        }
        if (detail.empty() && word != item.expected[i]) {
          detail = "trace " + std::to_string(i) + " differs: expected";
          for (const auto& a : item.expected[i]) detail += " " + a;
          detail += " | got";
          for (const auto& a : word) detail += " " + a;
        }
        ++traces;
      }
    } catch (const std::exception& error) {
      detail = std::string("raised ") + error.what();
    }
    if (!detail.empty()) {
      ++failures;
      if (failures <= 20) std::fprintf(stderr, "%s %s: %s\n", item.name.c_str(), item.tree.c_str(), detail.c_str());
    }
  }
  std::printf("%zu TOPBOTTOM playouts compared (%zu traces), %d failing\n", compared, traces, failures);
  if (compared < 900) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  return failures == 0 ? 0 : 1;
}
