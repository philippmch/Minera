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
  options.variant = RandomTreeVariant::kBasic;
  options.seed = 7;
  const auto first = generate_random_process_tree(options);
  const auto second = generate_random_process_tree(options);
  assert(first.activities() == second.activities());
  assert(equal_node(first.root(), second.root()));
  assert(first.root().op == ProcessTreeOperator::kSequence);
  assert(first.root().children.size() == 3);
  assert(depth(first.root()) <= options.max_rec_depth + 2);
}

void default_variant_is_population_generator() {
  RandomTreeOptions options;
  assert(options.variant == RandomTreeVariant::kPtAndLogGenerator);
  const auto tree = generate_random_process_tree(options);
  const auto leaves = visible_leaves(tree.root());
  assert(leaves >= options.min && leaves <= options.max);
}

void probability_extremes_and_seed() {
  RandomTreeOptions leaf_options;
  leaf_options.variant = RandomTreeVariant::kBasic;
  leaf_options.prob_leaf = 1.0;
  const auto leaf_tree = generate_random_process_tree(leaf_options);
  assert(leaf_tree.root().children[1].op == ProcessTreeOperator::kActivity);
  RandomTreeOptions internal_options;
  internal_options.variant = RandomTreeVariant::kBasic;
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
  choice.duplicate = 0.5;
  const auto choice_tree = generate_random_process_tree(choice);
  assert(has_operator(choice_tree.root(), ProcessTreeOperator::kExclusive));

  RandomTreeOptions inclusive = options;
  inclusive.min = inclusive.mode = inclusive.max = 4;
  inclusive.sequence = inclusive.choice = inclusive.parallel = inclusive.loop =
      0.0;
  inclusive.or_probability = 1.0;
  const auto inclusive_tree = generate_random_process_tree(inclusive);
  assert(has_operator(inclusive_tree.root(),
                      ProcessTreeOperator::kInclusiveChoice));

  RandomTreeOptions loop = options;
  loop.min = loop.mode = loop.max = 4;
  loop.sequence = loop.choice = loop.parallel = loop.or_probability = 0.0;
  loop.loop = 1.0;
  loop.silent = 1.0;
  assert(visible_leaves(generate_random_process_tree(loop).root()) == 4);
}

void plural_generation_is_seeded_and_bounded() {
  RandomTreeOptions options;
  options.variant = RandomTreeVariant::kPtAndLogGenerator;
  options.seed = 23;
  options.min = 4;
  options.mode = 5;
  options.max = 7;

  auto empty_options = options;
  empty_options.no_models = 0;
  assert(generate_random_process_trees(empty_options).empty());

  options.no_models = 1;
  const auto one = generate_random_process_trees(options);
  assert(one.size() == 1);
  assert(equal_node(one.front().root(),
                    generate_random_process_tree(options).root()));

  options.no_models = 3;
  const auto first = generate_random_process_trees(options);
  const auto second = generate_random_process_trees(options);
  assert(first.size() == 3);
  assert(second.size() == first.size());
  for (std::size_t i = 0; i < first.size(); ++i) {
    assert(equal_node(first[i].root(), second[i].root()));
    const auto leaves = visible_leaves(first[i].root());
    assert(leaves >= options.min && leaves <= options.max);
  }

  auto first_only = options;
  first_only.no_models = 1;
  assert(equal_node(first.front().root(),
                    generate_random_process_tree(first_only).root()));

  RandomTreeOptions basic = options;
  basic.variant = RandomTreeVariant::kBasic;
  basic.no_models = 3;
  const auto basic_models = generate_random_process_trees(basic);
  assert(basic_models.size() == 1);
  assert(basic_models.front().root().op == ProcessTreeOperator::kSequence);

  auto over_limit = options;
  over_limit.no_models = kMaxRandomTreeModels + 1;
  bool threw = false;
  try {
    (void)generate_random_process_trees(over_limit);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  auto over_budget = options;
  over_budget.min = over_budget.mode = over_budget.max = 10000;
  over_budget.no_models = kMaxRandomTreeModels;
  threw = false;
  try {
    (void)generate_random_process_trees(over_budget);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  auto singular_empty = options;
  singular_empty.no_models = 0;
  threw = false;
  try {
    (void)generate_random_process_tree(singular_empty);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void rejects_bad_options() {
  for (auto options : {RandomTreeOptions{.min_rec_depth = 3, .max_rec_depth = 1},
                       RandomTreeOptions{.prob_leaf = -0.1},
                       RandomTreeOptions{.prob_leaf = 1.1},
                       RandomTreeOptions{.activities = {"A", "A"}},
                       RandomTreeOptions{.variant = RandomTreeVariant::kPtAndLogGenerator,
                                         .seed = 0x100000000ULL}}) {
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
                                         .no_models = kMaxRandomTreeModels + 1},
                       RandomTreeOptions{.variant = RandomTreeVariant::kPtAndLogGenerator,
                                         .no_models = 2},
                       RandomTreeOptions{.variant = RandomTreeVariant::kPtAndLogGenerator,
                                         .sequence = -0.1},
                       RandomTreeOptions{.variant = RandomTreeVariant::kPtAndLogGenerator,
                                         .or_probability = -0.1}}) {
    bool threw = false;
    try {
      (void)generate_random_process_tree(options);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

// The reference labels: six characters from A-Z0-9 for BASIC, a, b, ...,
// z, aa, ab, ... in creation order for PT-and-log; caller activities are a
// declared BASIC extension.
void reference_labels() {
  RandomTreeOptions basic;
  basic.variant = RandomTreeVariant::kBasic;
  const auto generated = generate_random_process_tree(basic);
  for (const auto& name : generated.activities()) {
    assert(name.size() == 6);
    for (const char c : name) assert((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'));
  }
  basic.activities = {"x", "y"};
  const auto supplied = generate_random_process_tree(basic);
  for (const auto& name : supplied.activities())
    assert(name == "x" || name == "y");
  RandomTreeOptions ptand;
  ptand.min = ptand.mode = ptand.max = 30;
  ptand.silent = 0.0;
  const auto tree = generate_random_process_tree(ptand);
  assert(tree.activities().size() == 30);
  assert(tree.activities().front() == "a");
  assert(std::find(tree.activities().begin(), tree.activities().end(), "ad") != tree.activities().end());
}

void json_shape_is_stable() {
  RandomTreeOptions basic;
  basic.variant = RandomTreeVariant::kBasic;
  const auto json = random_tree_json(generate_random_process_tree(basic));
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

  options.no_models = 2;
  const auto batch_json = random_tree_json(
      generate_random_process_trees(options), options.variant,
      options.activities);
  assert(batch_json.find(R"("trees":[)") != std::string::npos);
  assert(batch_json.find(R"("tree":)") == std::string::npos);

  options.no_models = 0;
  const auto empty_json = random_tree_json(
      generate_random_process_trees(options), options.variant,
      options.activities);
  assert(empty_json.find(
             "\"activities\":[],\"trees\":[]") !=
         std::string::npos);
}

}  // namespace

int main() {
  deterministic_and_valid();
  default_variant_is_population_generator();
  probability_extremes_and_seed();
  ptandlog_generation_is_seeded_and_bounded();
  plural_generation_is_seeded_and_bounded();
  rejects_bad_options();
  json_shape_is_stable();
  reference_labels();
}
