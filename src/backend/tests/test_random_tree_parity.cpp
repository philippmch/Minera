// Seeded random process trees against the reference capture
// (scripts/capture-random-tree-bank.py: BASIC and PTANDLOGGENERATOR over
// random parameters, identical under PYTHONHASHSEED 0, 1 and 2). Every tree,
// labels included, must match; reference errors must be errors here.
#include "minera/random_tree.hpp"

#include <cstdio>
#include <exception>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

std::string text(const ProcessTreeNode& node, const ProcessTree& tree) {
  switch (node.op) {
    case ProcessTreeOperator::kTau: return "tau";
    case ProcessTreeOperator::kActivity: return tree.activities().at(node.activity);
    default: break;
  }
  std::string out;
  switch (node.op) {
    case ProcessTreeOperator::kSequence: out = "->("; break;
    case ProcessTreeOperator::kExclusive: out = "X("; break;
    case ProcessTreeOperator::kParallel: out = "+("; break;
    case ProcessTreeOperator::kLoop: out = "*("; break;
    case ProcessTreeOperator::kInclusiveChoice: out = "O("; break;
    case ProcessTreeOperator::kInterleaving: out = "<>("; break;
    default: out = "?("; break;
  }
  for (std::size_t i = 0; i < node.children.size(); ++i)
    out += (i ? "," : "") + text(node.children[i], tree);
  return out + ")";
}

struct Case {
  std::string line, name;
  RandomTreeOptions options;
  std::vector<std::string> trees;
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
      item.line = line;
      std::string kind;
      fields >> kind >> item.name >> item.options.seed;
      auto& o = item.options;
      if (kind == "basic") {
        o.variant = RandomTreeVariant::kBasic;
        fields >> o.rec_depth >> o.min_rec_depth >> o.max_rec_depth >> o.prob_leaf;
      } else {
        o.variant = RandomTreeVariant::kPtAndLogGenerator;
        fields >> o.min >> o.mode >> o.max >> o.sequence >> o.choice >> o.parallel >> o.loop >>
            o.or_probability >> o.silent >> o.duplicate >> o.no_models;
      }
      cases.push_back(std::move(item));
    } else if (tag == "T") {
      std::string tree;
      fields >> tree;
      cases.back().trees.push_back(tree);
    } else if (tag == "X") {
      cases.back().error = true;
    }
  }
  return cases;
}

}  // namespace

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "src/backend/tests/fixtures/random_tree_bank.txt";
  int failures = 0;
  std::size_t compared = 0, trees = 0, errors = 0;
  for (const auto& item : load(path)) {
    ++compared;
    std::string detail;
    try {
      const auto generated = generate_random_process_trees(item.options);
      if (item.error) {
        detail = "expected a reference error";
      } else if (generated.size() != item.trees.size()) {
        detail = "model count " + std::to_string(generated.size());
      } else {
        for (std::size_t i = 0; i < generated.size() && detail.empty(); ++i) {
          const auto got = text(generated[i].root(), generated[i]);
          if (got != item.trees[i]) detail = "model " + std::to_string(i) + ": expected " + item.trees[i] + " | got " + got;
          ++trees;
        }
      }
    } catch (const std::exception& error) {
      if (item.error) ++errors;
      else detail = std::string("raised ") + error.what();
    }
    if (!detail.empty()) {
      ++failures;
      if (failures <= 20) std::fprintf(stderr, "%s: %s\n", item.line.c_str(), detail.substr(0, 600).c_str());
    }
  }
  std::printf("%zu generator calls compared (%zu trees, %zu reference errors), %d failing\n", compared, trees,
              errors, failures);
  if (compared < 850) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  return failures == 0 ? 0 : 1;
}
