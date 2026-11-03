#include "minera/random_tree.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "minera/random.hpp"
#include <stdexcept>

namespace minera {
namespace {

void validate_options(const RandomTreeOptions& options) {
  if (options.min_rec_depth > options.max_rec_depth)
    throw std::invalid_argument(
        "random tree: min_rec_depth must not exceed max_rec_depth");
  if (options.max_rec_depth > 512)
    throw std::invalid_argument("random tree: max_rec_depth is too large");
  if (!std::isfinite(options.prob_leaf) || options.prob_leaf < 0.0 ||
      options.prob_leaf > 1.0)
    throw std::invalid_argument(
        "random tree: prob_leaf must be finite and in [0,1]");
  std::set<std::string> unique(options.activities.begin(),
                               options.activities.end());
  if (unique.size() != options.activities.size() ||
      unique.contains(std::string{}))
    throw std::invalid_argument(
        "random tree: activities must be non-empty and unique");

  if (options.variant != RandomTreeVariant::kPtAndLogGenerator) return;
  // numpy.random.seed accepts 32-bit seeds only.
  if (options.seed > 0xffffffffULL)
    throw std::invalid_argument("random tree: PT-and-log seed must be below 2^32");

  if (options.min == 0 || options.min > options.max ||
      options.mode < options.min || options.mode > options.max)
    throw std::invalid_argument(
        "random tree: PT-and-log min, mode, and max must satisfy 1 <= min <= max");
  if (options.max > 10000)
    throw std::invalid_argument("random tree: PT-and-log max is too large");
  if (options.no_models > kMaxRandomTreeModels)
    throw std::invalid_argument(
        "random tree: no_models exceeds the native safety limit");
  if (options.no_models != 0 &&
      options.max > kMaxRandomTreeActivityBudget / options.no_models)
    throw std::invalid_argument(
        "random tree: model activity budget exceeds the native safety limit");
  const auto check_probability = [](double value, const char* name) {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0)
      throw std::invalid_argument(std::string("random tree: ") + name +
                                  " must be finite and in [0,1]");
  };
  check_probability(options.sequence, "sequence");
  check_probability(options.choice, "choice");
  check_probability(options.parallel, "parallel");
  check_probability(options.loop, "loop");
  check_probability(options.or_probability, "or");
  check_probability(options.silent, "silent");
  check_probability(options.duplicate, "duplicate");
  if (options.sequence + options.choice + options.parallel + options.loop +
          options.or_probability <=
      0.0)
    throw std::invalid_argument(
        "random tree: at least one PT-and-log operator probability must be positive");
}

// Leaf labels of the BASIC generator: six characters, each
// "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"[randbelow(36)]. With caller
// activities (a declared extension) one randbelow(n) picks the label.
class BasicGenerator {
 public:
  BasicGenerator(const RandomTreeOptions& options, rng::PythonRandom& random)
      : options_(options), random_(random) {
    for (const auto& name : options.activities) intern(name);
  }

  ProcessTree run() {
    auto root = node(options_.rec_depth);
    return ProcessTree(names_, std::move(root));
  }

 private:
  std::size_t intern(const std::string& name) {
    const auto found = index_.find(name);
    if (found != index_.end()) return found->second;
    index_.emplace(name, names_.size());
    names_.push_back(name);
    return names_.size() - 1;
  }

  ProcessTreeNode label() {
    if (!options_.activities.empty())
      return ProcessTreeNode::leaf(random_.choice_index(options_.activities.size()));
    static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string name;
    for (int i = 0; i < 6; ++i) name.push_back(kAlphabet[random_.randbelow(36)]);
    return ProcessTreeNode::leaf(intern(name));
  }

  // gen(d): a leaf beyond max_rec_depth, or (from min_rec_depth on) with
  // probability prob_leaf; depth 0 is ->(label, gen(1), label); otherwise
  // one random() picks a sequence of randrange(2, 6) children, a loop
  // *(gen, gen, tau), an exclusive choice of randrange(2, 5) or a parallel
  // block of randrange(2, 4), the children generated at depth + 1.
  ProcessTreeNode node(std::size_t depth) {
    if (depth > options_.max_rec_depth) return label();
    if (depth >= options_.min_rec_depth && random_.random() < options_.prob_leaf) return label();
    if (depth == 0) {
      ProcessTreeNode root{ProcessTreeOperator::kSequence, 0, {}};
      root.children.push_back(label());
      root.children.push_back(node(1));
      root.children.push_back(label());
      return root;
    }
    const double pick = random_.random();
    const auto block = [&](ProcessTreeOperator op, std::int64_t low, std::int64_t high) {
      ProcessTreeNode out{op, 0, {}};
      const auto count = static_cast<std::size_t>(random_.randrange(low, high));
      for (std::size_t i = 0; i < count; ++i) out.children.push_back(node(depth + 1));
      return out;
    };
    if (pick < 0.25) return block(ProcessTreeOperator::kSequence, 2, 6);
    if (pick < 0.5) {
      ProcessTreeNode loop{ProcessTreeOperator::kLoop, 0, {}};
      loop.children.push_back(node(depth + 1));
      loop.children.push_back(node(depth + 1));
      loop.children.push_back(ProcessTreeNode::tau());
      return loop;
    }
    if (pick < 0.75) return block(ProcessTreeOperator::kExclusive, 2, 5);
    return block(ProcessTreeOperator::kParallel, 2, 4);
  }

  const RandomTreeOptions& options_;
  rng::PythonRandom& random_;
  std::vector<std::string> names_;
  std::map<std::string, std::size_t> index_;
};

// Python's round() of a float: half to even.
double python_round(double value) {
  const double floor = std::floor(value);
  const double fraction = value - floor;
  if (fraction > 0.5 || (fraction == 0.5 && std::fmod(floor, 2.0) != 0.0)) return floor + 1.0;
  return floor;
}

// The k-th label of the PT-and-log generator: a, ..., z, aa, ab, ...
std::string ptand_label(std::size_t k) {
  std::string out;
  for (std::size_t n = k + 1; n > 0; n = (n - 1) / 26)
    out.insert(out.begin(), static_cast<char>('a' + (n - 1) % 26));
  return out;
}

// PTANDLOGGENERATOR (exact under random.seed and numpy.random.seed with the
// same value). Per model: the activity count is round(min + (max - min) * x)
// with x the triangular inverse CDF (c = (mode - min) / (max - min)) of one
// numpy random_sample (no draw when min == max). From a single silent root,
// each step draws random() * total weight against the cumulative weights of
// sequence, choice, parallel, loop and or, then randbelow(2) for the child
// order, then a leaf (randbelow over the leaves in breadth-first order; the
// first step draws over two copies of the root). The leaf becomes the
// operator with its own label (a new one when silent) and a new leaf that is
// silent when random() < silent or the count is reached; the operator moves
// to the end of its parent's children. Labels are a, b, ... in creation
// order. Finally, with more than two leaves, one random() per leaf (in
// breadth-first order) below `duplicate` marks it; each marked leaf in turn
// copies its label onto rest[randbelow(len(rest))], rest the leaves equal
// (by label) to no marked leaf, redrawing while the target equals one of the
// marked leaf's other sibling leaves.
class PtandGenerator {
 public:
  PtandGenerator(const RandomTreeOptions& options, rng::PythonRandom& random,
                 rng::NumpyRandomState& numpy)
      : options_(options), random_(random), numpy_(numpy) {}

  ProcessTree run() {
    nodes_.clear();
    labels_.clear();
    std::size_t total = options_.min;
    if (options_.min != options_.max) {
      const double span = static_cast<double>(options_.max - options_.min);
      const double c = static_cast<double>(options_.mode - options_.min) / span;
      const double u = numpy_.random_sample();
      const double x = u < c ? std::sqrt(c * u) : 1.0 - std::sqrt((1.0 - c) * (1.0 - u));
      total = static_cast<std::size_t>(python_round(x * span + static_cast<double>(options_.min)));
    }
    const std::vector<double> weights = {options_.sequence, options_.choice, options_.parallel,
                                         options_.loop, options_.or_probability};
    std::vector<double> cumulative;
    double running = 0.0;
    for (const double w : weights) cumulative.push_back(running += w);
    static constexpr ProcessTreeOperator kOps[] = {
        ProcessTreeOperator::kSequence, ProcessTreeOperator::kExclusive, ProcessTreeOperator::kParallel,
        ProcessTreeOperator::kLoop, ProcessTreeOperator::kInclusiveChoice};

    nodes_.push_back(Node{});  // the silent root
    std::size_t created = 0;
    bool first = true;
    while (created < total) {
      const double r = random_.random() * cumulative.back();
      std::size_t op = 0;
      while (op < 4 && cumulative[op] <= r) ++op;
      const auto order = random_.randbelow(2);
      std::size_t leaf = 0;
      if (first) {
        (void)random_.randbelow(2);
        first = false;
      } else {
        const auto leaves = breadth_first_leaves();
        leaf = leaves[static_cast<std::size_t>(random_.randbelow(leaves.size()))];
      }
      const auto own = nodes_[leaf].label >= 0 ? nodes_[leaf].label : new_label(created);
      const bool silent = random_.random() < options_.silent || created >= total;
      const auto other = silent ? -1 : new_label(created);
      const auto first_child = add_leaf(order == 0 ? own : other, leaf);
      const auto second_child = add_leaf(order == 0 ? other : own, leaf);
      auto& node = nodes_[leaf];
      node.label = -1;
      node.op = kOps[op];
      node.children = {first_child, second_child};
      if (node.parent >= 0) {
        auto& siblings = nodes_[static_cast<std::size_t>(node.parent)].children;
        siblings.erase(std::find(siblings.begin(), siblings.end(), leaf));
        siblings.push_back(leaf);
      }
    }
    const auto leaves = breadth_first_leaves();
    if (leaves.size() > 2) {
      // Leaves compare by label (two silent leaves are equal), as process
      // trees compare structurally in the reference.
      std::vector<std::size_t> marked;
      for (const auto leaf : leaves)
        if (random_.random() < options_.duplicate) marked.push_back(leaf);
      std::vector<std::size_t> rest;
      for (const auto leaf : leaves) {
        bool equal_to_marked = false;
        for (const auto m : marked) equal_to_marked |= nodes_[m].label == nodes_[leaf].label;
        if (!equal_to_marked) rest.push_back(leaf);
      }
      for (const auto leaf : marked) {
        if (rest.empty())
          throw std::invalid_argument(
              "random tree: no leaf left to take a duplicate label (the reference raises IndexError)");
        // A target must not equal a sibling of the marked leaf (siblings
        // equal to the marked leaf itself excepted); the reference redraws
        // until it finds one, and loops forever when there is none.
        const auto& siblings = nodes_[static_cast<std::size_t>(nodes_[leaf].parent)].children;
        const auto rejected = [&](std::size_t target) {
          for (const auto sibling : siblings) {
            const auto& node = nodes_[sibling];
            if (!node.children.empty() || node.label == nodes_[leaf].label) continue;
            if (node.label == nodes_[target].label) return true;
          }
          return false;
        };
        if (std::all_of(rest.begin(), rest.end(), rejected))
          throw std::invalid_argument(
              "random tree: no admissible leaf for a duplicate label (the reference loops forever)");
        std::size_t target = 0;
        do {
          target = rest[static_cast<std::size_t>(random_.randbelow(rest.size()))];
        } while (rejected(target));
        nodes_[target].label = nodes_[leaf].label;
      }
    }
    // Activities in creation order, keeping only labels still on a leaf.
    std::vector<int> used(labels_.size(), 0);
    for (const auto& node : nodes_)
      if (node.children.empty() && node.label >= 0) used[static_cast<std::size_t>(node.label)] = 1;
    std::vector<std::string> names;
    std::vector<std::size_t> remap(labels_.size(), 0);
    for (std::size_t i = 0; i < labels_.size(); ++i)
      if (used[i]) {
        remap[i] = names.size();
        names.push_back(labels_[i]);
      }
    return ProcessTree(names, convert(0, remap));
  }

 private:
  struct Node {
    ProcessTreeOperator op = ProcessTreeOperator::kTau;
    int label = -1;  // leaf label index, -1 for tau
    int parent = -1;
    std::vector<std::size_t> children;
  };

  int new_label(std::size_t& created) {
    labels_.push_back(ptand_label(created++));
    return static_cast<int>(labels_.size() - 1);
  }

  std::size_t add_leaf(int label, std::size_t parent) {
    Node node;
    node.label = label;
    node.parent = static_cast<int>(parent);
    nodes_.push_back(node);
    return nodes_.size() - 1;
  }

  std::vector<std::size_t> breadth_first_leaves() const {
    std::vector<std::size_t> queue{0}, leaves;
    for (std::size_t at = 0; at < queue.size(); ++at) {
      const auto& node = nodes_[queue[at]];
      if (node.children.empty()) leaves.push_back(queue[at]);
      else queue.insert(queue.end(), node.children.begin(), node.children.end());
    }
    return leaves;
  }

  ProcessTreeNode convert(std::size_t index, const std::vector<std::size_t>& remap) const {
    const auto& node = nodes_[index];
    if (node.children.empty())
      return node.label < 0 ? ProcessTreeNode::tau()
                            : ProcessTreeNode::leaf(remap[static_cast<std::size_t>(node.label)]);
    ProcessTreeNode out{node.op, 0, {}};
    for (const auto child : node.children) out.children.push_back(convert(child, remap));
    return out;
  }

  const RandomTreeOptions& options_;
  rng::PythonRandom& random_;
  rng::NumpyRandomState& numpy_;
  std::vector<Node> nodes_;
  std::vector<std::string> labels_;
};

}  // namespace

std::vector<ProcessTree> generate_random_process_trees(
    const RandomTreeOptions& options) {
  validate_options(options);
  // The reference draws from the global generators: random.seed(seed) and,
  // for PT-and-log, numpy.random.seed(seed).
  rng::PythonRandom random(options.seed);
  const std::size_t count =
      options.variant == RandomTreeVariant::kPtAndLogGenerator
          ? options.no_models
          : 1;
  std::vector<ProcessTree> result;
  result.reserve(count);
  if (options.variant == RandomTreeVariant::kPtAndLogGenerator) {
    rng::NumpyRandomState numpy(static_cast<std::uint32_t>(options.seed));
    PtandGenerator generator(options, random, numpy);
    for (std::size_t i = 0; i < count; ++i) result.push_back(generator.run());
  } else {
    result.push_back(BasicGenerator(options, random).run());
  }
  return result;
}

ProcessTree generate_random_process_tree(const RandomTreeOptions& options) {
  if (options.variant == RandomTreeVariant::kPtAndLogGenerator &&
      options.no_models != 1)
    throw std::invalid_argument(
        "random tree: singular generation requires no_models == 1");
  auto result = generate_random_process_trees(options);
  // BASIC always produces one model. PT-and-log reaches this path only with
  // no_models == 1, so the vector is non-empty after validation.
  return std::move(result.front());
}

}  // namespace minera
