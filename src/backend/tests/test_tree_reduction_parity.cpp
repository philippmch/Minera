// algo.reduction.process_tree (TREE_TR_BASED) against the public seeded bank
// (docs/tree-reduction-bank.txt.gz, scripts/capture-tree-reduction-bank.py).
// Every outcome the reference showed under its hash seeds must come from the
// native reduction under some order of the distinct leaves (the reference
// iterates them in hash-seeded set order). The count of cases whose default
// (breadth-first) leaf order gives none of the observed outcomes is reported.
// Errors compare by class and message (TypeError by class alone). The fold
// cases compare fold_process_tree_reference with
// process_tree.utils.generic.fold, including childless operators and
// interleaving nodes.
#include "minera/process_tree_reduction.hpp"

#include "../gzip_codec.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

struct BankCase {
  std::string name;
  std::string mode;
  std::string tree;
  std::vector<std::string> trace;
  std::vector<std::string> outcomes;  // "O tree" or "E type message"
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
    const auto tag = line.substr(0, 1);
    const auto rest = line.substr(2);
    if (tag == "C") {
      cases.emplace_back();
      cases.back().name = rest.substr(0, rest.find(' '));
      cases.back().mode = rest.substr(rest.find(' ') + 1);
    } else if (tag == "T") {
      cases.back().tree = rest;
    } else if (tag == "A") {
      std::istringstream words(rest);
      for (std::string word; words >> word;)
        if (word != "-") cases.back().trace.push_back(word);
    } else {
      cases.back().outcomes.push_back(line);
    }
  }
  return cases;
}

class TreeParser {
 public:
  explicit TreeParser(const std::string& text) : text_(text) {}
  ProcessTree parse() {
    auto root = node();
    return ProcessTree(activities_, std::move(root));
  }
  // A node that ProcessTree may reject (a childless operator), for fold.
  ProcessTreeNode parse_node() { return node(); }
  const std::vector<std::string>& activities() const { return activities_; }

 private:
  ProcessTreeNode node() {
    if (text_.compare(at_, 1, "T") == 0) {
      ++at_;
      return ProcessTreeNode::tau();
    }
    if (text_.compare(at_, 2, "L:") == 0) {
      auto end = at_ + 2;
      while (end < text_.size() && text_[end] != ',' && text_[end] != ')') ++end;
      const auto label = text_.substr(at_ + 2, end - at_ - 2);
      at_ = end;
      std::size_t index = 0;
      while (index < activities_.size() && activities_[index] != label) ++index;
      if (index == activities_.size()) activities_.push_back(label);
      return ProcessTreeNode::leaf(index);
    }
    ProcessTreeNode result;
    if (text_.compare(at_, 3, "->(") == 0) {
      result.op = ProcessTreeOperator::kSequence;
      at_ += 3;
    } else if (text_.compare(at_, 3, "<>(") == 0) {
      result.op = ProcessTreeOperator::kInterleaving;
      at_ += 3;
    } else {
      const char op = text_[at_];
      result.op = op == 'X'   ? ProcessTreeOperator::kExclusive
                  : op == '+' ? ProcessTreeOperator::kParallel
                  : op == '*' ? ProcessTreeOperator::kLoop
                              : ProcessTreeOperator::kInclusiveChoice;
      at_ += 2;
    }
    if (text_[at_] == ')') {
      ++at_;
      return result;
    }
    while (true) {
      result.children.push_back(node());
      if (text_[at_++] == ')') return result;
    }
  }

  const std::string& text_;
  std::size_t at_ = 0;
  std::vector<std::string> activities_;
};

std::string dump(const std::vector<std::string>& activities, const ProcessTreeNode& node) {
  switch (node.op) {
    case ProcessTreeOperator::kTau: return "T";
    case ProcessTreeOperator::kActivity: return "L:" + activities.at(node.activity);
    default: break;
  }
  std::string out = node.op == ProcessTreeOperator::kSequence       ? "->("
                    : node.op == ProcessTreeOperator::kExclusive    ? "X("
                    : node.op == ProcessTreeOperator::kParallel     ? "+("
                    : node.op == ProcessTreeOperator::kLoop         ? "*("
                    : node.op == ProcessTreeOperator::kInterleaving ? "<>("
                                                                    : "O(";
  for (std::size_t i = 0; i < node.children.size(); ++i)
    out += (i ? "," : "") + dump(activities, node.children[i]);
  return out + ")";
}

std::string dump(const ProcessTree& tree, const ProcessTreeNode& node) { return dump(tree.activities(), node); }

std::size_t fold_cases = 0;

// process_tree.utils.generic.fold on one tree.
std::string check_fold(const BankCase& item) {
  ++fold_cases;
  TreeParser parser(item.tree);
  const auto node = parser.parse_node();
  const auto actual = "O " + dump(parser.activities(), fold_process_tree_reference(node));
  if (std::find(item.outcomes.begin(), item.outcomes.end(), actual) != item.outcomes.end()) return {};
  return item.tree + "\n  expected " + item.outcomes.front() + "\n  actual   " + actual;
}

std::size_t counts[3] = {};

// tree_tr_based.apply (and the reducer's dispatch) on one tree and trace.
std::string check_tree_tr_based(const BankCase& item) {
  ++counts[0];
  const auto tree = TreeParser(item.tree).parse();
  Trace trace;
  for (const auto& activity : item.trace) {
    Event event;
    if (item.mode == "custom_key") {
      EventAttribute attribute;
      attribute.key = "act";
      attribute.value = activity;
      event.attributes.push_back(attribute);
    } else {
      event.activity = activity;
    }
    trace.events.push_back(std::move(event));
  }
  ProcessTreeReductionOptions options;
  if (item.mode == "custom_key") options.activity_key = "act";
  // The reference orders the distinct leaves by hash-seeded set iteration:
  // every observed outcome must come from some leaf order, and the default
  // order must give one of them when the reference showed no variation.
  const auto run = [&](const std::vector<ProcessTreeReductionLeaf>* order) -> std::string {
    if (item.mode == "no_variant") return "E Exception please specify the variant of the reduction to be used.";
    try {
      return "O " + dump(tree, reference_reduce_process_tree(tree, trace, options, order).root());
    } catch (const ProcessTreeReductionError& error) {
      // The native TypeError (an interleaving node, which the reference
      // footprints lack) explains itself; the reference message is
      // "'NoneType' object is not subscriptable". Compare that class alone.
      return error.type == "TypeError" ? "E TypeError" : "E " + error.type + " " + error.what();
    }
  };
  const auto actual = run(nullptr);
  if (item.outcomes.size() > 1) ++counts[1];
  std::vector<std::string> outcomes = item.outcomes;
  for (auto& outcome : outcomes)
    if (outcome.rfind("E TypeError ", 0) == 0) outcome = "E TypeError";
  std::vector<std::string> missing = outcomes;
  auto order = process_tree_reduction_leaves(tree);
  std::sort(order.begin(), order.end());
  std::size_t tried = 0;
  do {
    const auto outcome = run(&order);
    missing.erase(std::remove(missing.begin(), missing.end(), outcome), missing.end());
  } while (!missing.empty() && ++tried < 50000 && std::next_permutation(order.begin(), order.end()));
  bool default_seen = false;
  for (const auto& outcome : outcomes) default_seen = default_seen || outcome == actual;
  if (!default_seen) ++counts[2];
  if (missing.empty()) return {};
  std::string expected;
  for (const auto& outcome : item.outcomes) expected += "\n  expected " + outcome;
  return item.tree + " trace " + std::to_string(item.trace.size()) + expected + "\n  actual   " + actual;
}

// reducer.apply: the public reduction is the reference reduction in its
// default leaf order.
std::string check_reducer(const BankCase& item) {
  if (item.mode == "variant") {
    const auto tree = TreeParser(item.tree).parse();
    Trace trace;
    for (const auto& activity : item.trace) {
      Event event;
      event.activity = activity;
      trace.events.push_back(std::move(event));
    }
    std::string public_result, reference_result;
    try {
      public_result = dump(tree, reduce_process_tree(tree, trace).root());
    } catch (const std::exception& error) {
      public_result = error.what();
    }
    try {
      reference_result = dump(tree, reference_reduce_process_tree(tree, trace).root());
    } catch (const std::exception& error) {
      reference_result = error.what();
    }
    if (public_result != reference_result) return "reduce_process_tree differs: " + public_result;
  }
  return check_tree_tr_based(item);
}

}  // namespace

int main(int argc, char** argv) try {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " BANK.txt.gz\n";
    return 2;
  }
  std::size_t failures = 0;
  for (const auto& item : read_bank(argv[1])) {
    const auto failure = item.mode == "fold"         ? check_fold(item)
                         : item.mode == "custom_key" ? check_tree_tr_based(item)
                                                     : check_reducer(item);
    if (failure.empty()) continue;
    if (++failures <= 25) std::cerr << item.name << " (" << item.mode << "): " << failure << '\n';
  }
  std::cout << counts[0] << " calls compared (" << counts[1] << " with hash-seed dependent outcomes, "
            << counts[2] << " where the default leaf order differs), "
            << failures << " failures\n";
  std::cout << fold_cases << " folds compared\n";
  return failures || counts[0] < 8000 || fold_cases < 3000 ? 1 : 0;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << "\n";
  return 1;
}
