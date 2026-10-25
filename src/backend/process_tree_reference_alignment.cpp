#include "minera/process_tree_reference_alignment.hpp"

#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace minera {
namespace {

using Segment = std::vector<int>;
constexpr int kSkip = -1;  // ">>"

struct Node {
  ProcessTreeOperator op = ProcessTreeOperator::kTau;
  int label = kSkip;  // leaves
  std::vector<std::unique_ptr<Node>> children;
  std::set<int> labels;  // every leaf label below
};

struct Move {
  int log = kSkip, model = kSkip;
};

struct Result {
  long cost = 0;
  std::vector<Move> moves;
};

Result join(const Result& left, const Result& right) {
  Result out{left.cost + right.cost, left.moves};
  out.moves.insert(out.moves.end(), right.moves.begin(), right.moves.end());
  return out;
}

struct Builder {
  const ProcessTree& tree;
  std::map<std::string, int>& ids;

  int id(const std::string& name) { return ids.emplace(name, static_cast<int>(ids.size())).first->second; }

  std::unique_ptr<Node> build(const ProcessTreeNode& source) {
    auto node = std::make_unique<Node>();
    node->op = source.op;
    switch (source.op) {
      case ProcessTreeOperator::kActivity:
        node->label = id(tree.activities().at(source.activity));
        node->labels.insert(node->label);
        return node;
      case ProcessTreeOperator::kTau:
        return node;
      case ProcessTreeOperator::kInclusiveChoice:
        throw ProcessTreeReferenceAlignmentError("Operator O not implemented.");
      case ProcessTreeOperator::kInterleaving:
        throw ProcessTreeReferenceAlignmentError("Operator <> not implemented.");
      default:
        break;
    }
    std::vector<std::unique_ptr<Node>> children;
    for (const auto& child : source.children) children.push_back(build(child));
    if (source.op != ProcessTreeOperator::kLoop) {
      // Right nesting: ->(a, b, c) becomes ->(a, ->(b, c)).
      while (children.size() > 2) {
        auto nested = std::make_unique<Node>();
        nested->op = source.op;
        nested->children.push_back(std::move(children[children.size() - 2]));
        nested->children.push_back(std::move(children.back()));
        for (const auto& child : nested->children) nested->labels.insert(child->labels.begin(), child->labels.end());
        children.pop_back();
        children.back() = std::move(nested);
      }
    }
    node->children = std::move(children);
    for (const auto& child : node->children) node->labels.insert(child->labels.begin(), child->labels.end());
    return node;
  }
};

struct Aligner {
  std::map<std::pair<const Node*, Segment>, Result> memo;

  const Result& align(const Node& node, const Segment& segment) {
    const auto key = std::make_pair(&node, segment);
    if (const auto found = memo.find(key); found != memo.end()) return found->second;
    Result result;
    switch (node.op) {
      case ProcessTreeOperator::kActivity:
      case ProcessTreeOperator::kTau:
        result = leaf(node, segment);
        break;
      case ProcessTreeOperator::kExclusive:
        result = choice(node.children, 0, segment);
        break;
      case ProcessTreeOperator::kSequence:
        result = sequence(node, segment);
        break;
      case ProcessTreeOperator::kParallel:
        result = parallel(node, segment);
        break;
      case ProcessTreeOperator::kLoop:
        result = loop(node, segment);
        break;
      default:
        throw ProcessTreeReferenceAlignmentError("unsupported operator");
    }
    return memo.emplace(key, std::move(result)).first->second;
  }

  static Result leaf(const Node& node, const Segment& segment) {
    Result out;
    std::size_t matched = segment.size();
    if (node.label != kSkip)
      for (std::size_t i = 0; i < segment.size() && matched == segment.size(); ++i)
        if (segment[i] == node.label) matched = i;
    for (std::size_t i = 0; i < segment.size(); ++i)
      out.moves.push_back(i == matched ? Move{node.label, node.label} : Move{segment[i], kSkip});
    out.cost = static_cast<long>(segment.size());
    if (node.label != kSkip) {
      if (matched < segment.size()) {
        --out.cost;
      } else {
        ++out.cost;
        out.moves.push_back({kSkip, node.label});
      }
    }
    return out;
  }

  // The first cheapest of children[from..] on the segment.
  Result choice(const std::vector<std::unique_ptr<Node>>& children, std::size_t from, const Segment& segment) {
    const Result* best = nullptr;
    for (std::size_t c = from; c < children.size(); ++c) {
      const auto& candidate = align(*children[c], segment);
      if (!best || candidate.cost < best->cost) best = &candidate;
    }
    return *best;
  }

  Result sequence(const Node& node, const Segment& segment) {
    std::optional<Result> best;
    long best_left = 0;
    for (std::size_t end = 0; end <= segment.size(); ++end) {
      const auto& left = align(*node.children[0], Segment(segment.begin(), segment.begin() + end));
      const auto& right = align(*node.children[1], Segment(segment.begin() + end, segment.end()));
      const long total = left.cost + right.cost;
      if (!best || total < best->cost || (total == best->cost && left.cost < best_left)) {
        best = join(left, right);
        best_left = left.cost;
      }
    }
    return *best;
  }

  Result parallel(const Node& node, const Segment& segment) {
    Result out;
    std::vector<const Result*> parts;
    for (const auto& child : node.children) {
      Segment projected;
      for (const int event : segment)
        if (child->labels.contains(event)) projected.push_back(event);
      parts.push_back(&align(*child, projected));
      out.cost += parts.back()->cost;
    }
    std::vector<std::size_t> next(parts.size(), 0);
    for (const int event : segment) {
      std::size_t c = 0;
      while (c < node.children.size() && !node.children[c]->labels.contains(event)) ++c;
      if (c == node.children.size()) {
        ++out.cost;
        out.moves.push_back({event, kSkip});
      } else {
        out.moves.push_back(parts[c]->moves.at(next[c]++));
      }
    }
    return out;
  }

  Result loop(const Node& node, const Segment& segment) {
    if (segment.empty()) return {};
    const std::size_t n = segment.size();
    const auto part = [&](std::size_t i, std::size_t j) {
      return Segment(segment.begin() + static_cast<std::ptrdiff_t>(i), segment.begin() + static_cast<std::ptrdiff_t>(j));
    };
    const auto body = [&](std::size_t i, std::size_t j) -> const Result& { return align(*node.children[0], part(i, j)); };
    const auto redo = [&](std::size_t i, std::size_t j) { return choice(node.children, 1, part(i, j)); };
    const auto keep = [](std::optional<Result>& best, Result candidate) {
      if (!best || candidate.cost < best->cost) best = std::move(candidate);
    };
    std::vector<Result> after_body(n + 1), after_redo(n + 1);
    for (std::size_t j = 0; j <= n; ++j) {
      if (j > 0) {
        std::optional<Result> best;
        keep(best, redo(0, j));
        for (std::size_t i = 1; i < j; ++i) keep(best, join(after_body[i], redo(i, j)));
        after_redo[j] = std::move(*best);
      }
      std::optional<Result> best;
      keep(best, body(0, j));
      for (std::size_t i = 1; i <= j; ++i) keep(best, join(after_redo[i], body(i, j)));
      after_body[j] = std::move(*best);
    }
    std::optional<Result> best;
    for (std::size_t i = 0; i <= n; ++i) {
      if (i == 0) {
        keep(best, body(0, n));
      } else {
        keep(best, join(after_redo[i], body(i, n)));
      }
      keep(best, join(after_body[i], body(i, n)));
    }
    return std::move(*best);
  }
};

}  // namespace

std::vector<ProcessTreeReferenceAlignment> reference_process_tree_dp_alignments(
    const ProcessTree& tree, const std::vector<std::vector<std::string>>& traces) {
  std::map<std::string, int> ids;
  Builder builder{tree, ids};
  const auto root = builder.build(tree.root());
  std::vector<std::string> names;
  const auto encode = [&](const std::vector<std::string>& trace) {
    Segment out;
    for (const auto& event : trace) out.push_back(builder.id(event));
    return out;
  };
  for (const auto& trace : traces) encode(trace);
  names.resize(ids.size());
  for (const auto& [name, index] : ids) names[static_cast<std::size_t>(index)] = name;
  const auto name = [&](int index) { return index == kSkip ? std::string(">>") : names[static_cast<std::size_t>(index)]; };

  Aligner aligner;
  const double floor = static_cast<double>(aligner.align(*root, {}).cost);
  std::vector<ProcessTreeReferenceAlignment> out;
  for (const auto& trace : traces) {
    const auto& result = aligner.align(*root, encode(trace));
    ProcessTreeReferenceAlignment alignment;
    alignment.cost = static_cast<double>(result.cost);
    for (const auto& move : result.moves) alignment.moves.push_back({name(move.log), name(move.model)});
    const double denominator = static_cast<double>(trace.size()) + floor;
    alignment.fitness = denominator == 0.0 ? 0.0 : 1.0 - alignment.cost / denominator;
    out.push_back(std::move(alignment));
  }
  return out;
}

}  // namespace minera
