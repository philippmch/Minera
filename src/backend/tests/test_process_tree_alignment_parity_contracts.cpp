// Black-box parity contracts for process-tree alignments against the reference
// banks docs/reference-2.7.22-conformance-parity-bank.json.gz and
// docs/reference-2.7.22-process-tree-alignment-bank.json.gz (random process
// trees, model-language traces and noisy variants).
//
// Every reference row is classified by comparing the native (cost, fitness)
// with the pairs the reference produced across the merged capture runs:
// equal, native cheaper, native costlier, or equal cost with another fitness.
// The per-variant tallies are pinned, so any behavioural change on the corpus
// fails here and the parity document records what each class means.
//
// SEARCH_GRAPH_PT alignments list tau leaves as silent model moves, so each
// native alignment must replay the trace and be a word of the tree with its
// tau leaves, at the reported cost; the reference tie choice among optimal
// alignments is not bound. The other variants' alignments are not bound: the
// reference leaves silent and some model moves out of them.
#include "minera/process_tree_alignment.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace minera;

struct TreeParityVariant {
  ProcessTreeAlignmentVariant variant;
  bool error;  // every reference run raised
  // Per trace, the (cost, fitness) pairs observed across runs.
  std::vector<std::vector<std::pair<double, double>>> rows;
};

struct TreeParityCase {
  std::string bank;
  std::string name;
  std::string tree;
  std::vector<std::vector<std::string>> traces;
  std::vector<TreeParityVariant> variants;
};

#include "process_tree_alignment_parity_cases.inc"

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

// Model-side words of a tree whose tau leaves are explicit symbols. Every
// subtree emits at least one symbol, so the recursion always shrinks.
const std::string kTau = "\x1f";

class TreeLanguage {
 public:
  explicit TreeLanguage(const ProcessTree& tree) : tree_(tree) {}

  bool accepts(const std::vector<std::string>& word) {
    return accepts(tree_.root(), word);
  }

 private:
  using Key = std::pair<const ProcessTreeNode*, std::vector<std::string>>;

  void alphabet(const ProcessTreeNode& node, std::vector<std::string>& out) const {
    if (node.op == ProcessTreeOperator::kTau) out.push_back(kTau);
    if (node.op == ProcessTreeOperator::kActivity)
      out.push_back(tree_.activities()[node.activity]);
    for (const auto& child : node.children) alphabet(child, out);
  }

  bool sequence(const ProcessTreeNode& node, std::size_t child,
                const std::vector<std::string>& word, std::size_t start) {
    if (child == node.children.size()) return start == word.size();
    for (std::size_t end = start + 1; end <= word.size(); ++end) {
      const std::vector<std::string> part(word.begin() + start, word.begin() + end);
      if (accepts(node.children[child], part) && sequence(node, child + 1, word, end))
        return true;
    }
    return false;
  }

  bool loop(const ProcessTreeNode& node, const std::vector<std::string>& word,
            std::size_t start) {
    for (std::size_t end = start + 1; end <= word.size(); ++end) {
      const std::vector<std::string> body(word.begin() + start, word.begin() + end);
      if (!accepts(node.children[0], body)) continue;
      if (end == word.size()) return true;
      for (std::size_t next = end + 1; next < word.size(); ++next) {
        const std::vector<std::string> redo(word.begin() + end, word.begin() + next);
        for (std::size_t r = 1; r < node.children.size(); ++r)
          if (accepts(node.children[r], redo) && loop(node, word, next)) return true;
      }
    }
    return false;
  }

  bool parallel(const ProcessTreeNode& node, const std::vector<std::string>& word,
                std::size_t position, std::vector<std::vector<std::string>>& parts,
                const std::vector<std::vector<std::string>>& alphabets) {
    if (position == word.size()) {
      for (std::size_t i = 0; i < parts.size(); ++i)
        if (!accepts(node.children[i], parts[i])) return false;
      return true;
    }
    for (std::size_t i = 0; i < parts.size(); ++i) {
      bool owns = false;
      for (const auto& symbol : alphabets[i]) owns = owns || symbol == word[position];
      if (!owns) continue;
      parts[i].push_back(word[position]);
      const bool ok = parallel(node, word, position + 1, parts, alphabets);
      parts[i].pop_back();
      if (ok) return true;
    }
    return false;
  }

  bool accepts(const ProcessTreeNode& node, const std::vector<std::string>& word) {
    const Key key{&node, word};
    if (const auto found = memo_.find(key); found != memo_.end()) return found->second;
    bool result = false;
    switch (node.op) {
      case ProcessTreeOperator::kTau:
        result = word.size() == 1 && word[0] == kTau;
        break;
      case ProcessTreeOperator::kActivity:
        result = word.size() == 1 && word[0] == tree_.activities()[node.activity];
        break;
      case ProcessTreeOperator::kSequence:
        result = sequence(node, 0, word, 0);
        break;
      case ProcessTreeOperator::kExclusive:
        for (const auto& child : node.children) result = result || accepts(child, word);
        break;
      case ProcessTreeOperator::kParallel: {
        std::vector<std::vector<std::string>> alphabets(node.children.size());
        for (std::size_t i = 0; i < node.children.size(); ++i)
          alphabet(node.children[i], alphabets[i]);
        std::vector<std::vector<std::string>> parts(node.children.size());
        result = parallel(node, word, 0, parts, alphabets);
        break;
      }
      case ProcessTreeOperator::kLoop:
        result = loop(node, word, 0);
        break;
      default:
        throw std::invalid_argument("operator outside the corpus");
    }
    memo_.emplace(key, result);
    return result;
  }

  const ProcessTree& tree_;
  std::map<Key, bool> memo_;
};

EventLog log_of(const std::vector<std::vector<std::string>>& traces) {
  EventLog log;
  for (std::size_t t = 0; t < traces.size(); ++t) {
    Trace trace;
    for (std::size_t e = 0; e < traces[t].size(); ++e)
      trace.events.push_back(Event{std::to_string(t), traces[t][e], {}, {}, e});
    log.traces.push_back(std::move(trace));
  }
  return log;
}

bool close(double left, double right) { return std::fabs(left - right) <= 1e-6; }

struct Tally {
  std::size_t equal = 0;
  std::size_t cheaper = 0;   // native cost below every reference run
  std::size_t costlier = 0;  // native cost above every reference run
  std::size_t fitness = 0;   // a reference cost, but another fitness
  std::size_t reference_errors = 0;
  bool operator==(const Tally&) const = default;
};

void check_alignment(const ProcessTree& tree, const std::vector<std::string>& trace,
                     const ProcessTreeTraceAlignment& row) {
  std::vector<std::string> log_side;
  std::vector<std::string> model_side;
  std::uint64_t cost = 0;
  for (const auto& move : row.moves) {
    switch (move.type) {
      case ProcessTreeAlignmentMoveType::kSynchronous:
        assert(move.log_activity == move.model_activity);
        log_side.push_back(move.log_activity);
        model_side.push_back(move.model_activity);
        break;
      case ProcessTreeAlignmentMoveType::kLog:
        log_side.push_back(move.log_activity);
        ++cost;
        break;
      case ProcessTreeAlignmentMoveType::kModel:
        if (move.model_activity.empty()) {
          model_side.push_back(kTau);
        } else {
          model_side.push_back(move.model_activity);
          ++cost;
        }
        break;
    }
  }
  assert(log_side == trace);
  assert(cost == row.cost);
  assert(TreeLanguage(tree).accepts(model_side));
}

Tally run_variant(ProcessTreeAlignmentVariant variant, bool native = true) {
  Tally tally;
  for (const auto& c : tree_parity_cases()) {
    for (const auto& expected : c.variants) {
      if (expected.variant != variant) continue;
      if (expected.error) {
        ++tally.reference_errors;
        continue;
      }
      const auto tree = parse_tree(c.tree);
      ProcessTreeAlignmentOptions options;
      options.variant = variant;
      options.native_search = native;
      const auto actual = align_process_tree(log_of(c.traces), tree, options);
      assert(actual.traces.size() == c.traces.size());
      for (std::size_t t = 0; t < c.traces.size(); ++t) {
        const auto& row = actual.traces[t];
        assert(row.completed);
        const double cost = static_cast<double>(row.cost);
        bool equal = false, cost_seen = false, below = true, above = true;
        for (const auto& [ref_cost, ref_fitness] : expected.rows[t]) {
          equal = equal || (close(cost, ref_cost) && close(row.fitness, ref_fitness));
          cost_seen = cost_seen || close(cost, ref_cost);
          below = below && cost < ref_cost - 1e-6;
          above = above && cost > ref_cost + 1e-6;
        }
        if (equal) ++tally.equal;
        else if (cost_seen) ++tally.fitness;
        else if (below) ++tally.cheaper;
        else if (above) ++tally.costlier;
        else throw std::logic_error("native cost between reference runs");
        if (native && variant == ProcessTreeAlignmentVariant::kSearchGraphPt)
          check_alignment(tree, c.traces[t], row);
      }
    }
  }
  std::cerr << (native ? "native " : "default ") << "variant " << static_cast<int>(variant) << ": equal " << tally.equal
            << ", cheaper " << tally.cheaper << ", costlier " << tally.costlier
            << ", fitness " << tally.fitness << ", reference errors "
            << tally.reference_errors << "\n";
  return tally;
}

void search_graph_pt_reference_contract() {
  const auto tally = run_variant(ProcessTreeAlignmentVariant::kSearchGraphPt);
  // The reference search prunes some optimal paths (nested loops with silent
  // redos, sync-first choices); native stays optimal on those 28 rows.
  assert((tally == Tally{4112, 28, 0, 0, 0}));
}

void milp_reference_contract() {
  const auto tally = run_variant(ProcessTreeAlignmentVariant::kMilp);
  // Root loops with a tau body: the reference gives the empty trace a positive
  // cost (10 rows) and divides every trace of those trees by it (14 rows).
  assert((tally == Tally{4116, 10, 0, 14, 0}));
}

void dynamic_programming_reference_contract() {
  const auto tally = run_variant(ProcessTreeAlignmentVariant::kDynamicProgramming);
  // 14 rows where the reference reuses a cached loop-iteration alignment for a
  // reordered segment.
  assert((tally == Tally{4126, 11, 3, 0, 0}));
}

void approximated_original_reference_contract() {
  const auto tally = run_variant(ProcessTreeAlignmentVariant::kApproximatedOriginal);
  // The reference splits the trace over subtrees with its activity-set
  // heuristic; native splits optimally and is cheaper on 164 rows.
  assert((tally == Tally{3976, 164, 0, 0, 0}));
}

// The default path runs the reference ports.
void default_path_reference_contract() {
  // SEARCH_GRAPH_PT reproduces every recorded row.
  assert((run_variant(ProcessTreeAlignmentVariant::kSearchGraphPt, false) ==
          Tally{4140, 0, 0, 0, 0}));
  // The 24 MILP rows of root loops with a tau body have an infeasible empty
  // trace program (the reference reports solver leftovers), so they run the
  // native search and keep its tally.
  assert((run_variant(ProcessTreeAlignmentVariant::kMilp, false) ==
          Tally{4116, 10, 0, 14, 0}));
  // DYNAMIC_PROGRAMMING: the 14 rows where the reference reuses a cached
  // alignment of another segment (process_tree_reference_alignment.hpp).
  assert((run_variant(ProcessTreeAlignmentVariant::kDynamicProgramming, false) ==
          Tally{4126, 11, 3, 0, 0}));
  // APPROXIMATED_ORIGINAL: 37 rows depend on CBC's choice among tied splits
  // and on object-address order, which the port does not reproduce.
  assert((run_variant(ProcessTreeAlignmentVariant::kApproximatedOriginal, false) ==
          Tally{4103, 20, 17, 0, 0}));
}

}  // namespace

int main() {
  default_path_reference_contract();
  search_graph_pt_reference_contract();
  milp_reference_contract();
  dynamic_programming_reference_contract();
  approximated_original_reference_contract();
  return 0;
}
