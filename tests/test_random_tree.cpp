#undef NDEBUG

#include "minera/random_tree.hpp"
#include "minera/json_export.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>

namespace {
using namespace minera;

bool equal_node(const ProcessTreeNode& lhs, const ProcessTreeNode& rhs) {
  if (lhs.op != rhs.op || lhs.activity != rhs.activity ||
      lhs.children.size() != rhs.children.size())
    return false;
  for (std::size_t i = 0; i < lhs.children.size(); ++i)
    if (!equal_node(lhs.children[i], rhs.children[i])) return false;
  return true;
}

std::size_t depth(const ProcessTreeNode& node) {
  std::size_t result = 0;
  for (const auto& child : node.children)
    result = std::max(result, depth(child));
  return result + (node.children.empty() ? 0 : 1);
}

void deterministic_and_valid() {
  RandomTreeOptions options;
  options.seed = 7;
  const auto first = generate_random_process_tree(options);
  const auto second = generate_random_process_tree(options);
  assert(first.activities() == second.activities());
  assert(equal_node(first.root(), second.root()));
  assert(first.root().op == ProcessTreeOperator::kSequence);
  assert(first.root().children.size() == 3);
  assert(depth(first.root()) <= options.max_rec_depth + 2);
}

void probability_extremes_and_seed() {
  RandomTreeOptions leaf_options;
  leaf_options.prob_leaf = 1.0;
  const auto leaf_tree = generate_random_process_tree(leaf_options);
  assert(leaf_tree.root().children[1].op == ProcessTreeOperator::kActivity);
  RandomTreeOptions internal_options;
  internal_options.prob_leaf = 0.0;
  internal_options.max_rec_depth = 1;
  const auto internal_tree = generate_random_process_tree(internal_options);
  assert(!internal_tree.root().children[1].children.empty());
  internal_options.seed = 99;
  const auto changed = generate_random_process_tree(internal_options);
  assert(!equal_node(internal_tree.root(), changed.root()) ||
         internal_tree.activities() != changed.activities());
}

void rejects_bad_options() {
  for (auto options : {RandomTreeOptions{.min_rec_depth = 3, .max_rec_depth = 1},
                       RandomTreeOptions{.prob_leaf = -0.1},
                       RandomTreeOptions{.prob_leaf = 1.1},
                       RandomTreeOptions{.activities = {}}}) {
    bool threw = false;
    try {
      (void)generate_random_process_tree(options);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void json_shape_is_stable() {
  const auto json = random_tree_json(generate_random_process_tree({}));
  assert(json.find(R"({"family":"RANDOM_TREE","variant":"BASIC")" ) !=
         std::string::npos);
  assert(json.find(R"("tree":{"operator":)" ) != std::string::npos);
}

}  // namespace

int main() {
  deterministic_and_valid();
  probability_extremes_and_seed();
  rejects_bad_options();
  json_shape_is_stable();
}
