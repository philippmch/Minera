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

std::size_t visible_leaves(const ProcessTreeNode& node) {
  if (node.op == ProcessTreeOperator::kActivity) return 1;
  std::size_t count = 0;
  for (const auto& child : node.children) count += visible_leaves(child);
  return count;
}

bool has_operator(const ProcessTreeNode& node, ProcessTreeOperator op) {
  if (node.op == op) return true;
  for (const auto& child : node.children)
    if (has_operator(child, op)) return true;
  return false;
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

void ptandlog_generation_is_seeded_and_bounded() {
  RandomTreeOptions options;
  options.variant = RandomTreeVariant::kPtAndLogGenerator;
  options.seed = 23;
  options.min = 4;
  options.mode = 5;
  options.max = 7;
  const auto first = generate_random_process_tree(options);
  const auto second = generate_random_process_tree(options);
  assert(equal_node(first.root(), second.root()));
  const auto leaves = visible_leaves(first.root());
  assert(leaves >= options.min && leaves <= options.max);

  RandomTreeOptions asymmetric = options;
  asymmetric.min = 1;
  asymmetric.mode = 2;
  asymmetric.max = 100;
  asymmetric.seed = 101;
  const auto asymmetric_tree = generate_random_process_tree(asymmetric);
  const auto asymmetric_leaves = visible_leaves(asymmetric_tree.root());
  assert(asymmetric_leaves >= asymmetric.min &&
         asymmetric_leaves <= asymmetric.max);

  RandomTreeOptions sequence = options;
  sequence.sequence = 1.0;
  sequence.choice = sequence.parallel = sequence.loop =
      sequence.or_probability = 0.0;
  const auto sequence_tree = generate_random_process_tree(sequence);
  assert(has_operator(sequence_tree.root(), ProcessTreeOperator::kSequence));

  RandomTreeOptions choice = options;
  choice.sequence = choice.parallel = choice.loop = choice.or_probability = 0.0;
  choice.choice = 1.0;
  choice.silent = 1.0;
  choice.duplicate = 1.0;
  const auto choice_tree = generate_random_process_tree(choice);
  assert(has_operator(choice_tree.root(), ProcessTreeOperator::kExclusive));

  RandomTreeOptions loop = options;
  loop.min = loop.mode = loop.max = 4;
  loop.sequence = loop.choice = loop.parallel = loop.or_probability = 0.0;
  loop.loop = 1.0;
  loop.silent = 1.0;
  assert(visible_leaves(generate_random_process_tree(loop).root()) == 4);
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

  for (auto options : {RandomTreeOptions{.variant = RandomTreeVariant::kPtAndLogGenerator,
                                         .mode = 2,
                                         .min = 3,
                                         .max = 1},
                       RandomTreeOptions{.variant = RandomTreeVariant::kPtAndLogGenerator,
                                         .no_models = 2},
                       RandomTreeOptions{.variant = RandomTreeVariant::kPtAndLogGenerator,
                                         .sequence = -0.1},
                       RandomTreeOptions{.variant = RandomTreeVariant::kPtAndLogGenerator,
                                         .or_probability = 1.0}}) {
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
  RandomTreeOptions options;
  options.variant = RandomTreeVariant::kPtAndLogGenerator;
  options.min = options.mode = options.max = 3;
  const auto pt_json = random_tree_json(
      generate_random_process_tree(options), options.variant);
  assert(pt_json.find(R"("variant":"PTANDLOGGENERATOR")") !=
         std::string::npos);
}

}  // namespace

int main() {
  deterministic_and_valid();
  probability_extremes_and_seed();
  ptandlog_generation_is_seeded_and_bounded();
  rejects_bad_options();
  json_shape_is_stable();
}
