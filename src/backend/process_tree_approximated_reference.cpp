#include "minera/process_tree_approximated_reference.hpp"
#include "minera/cancellation.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <cstdint>
#include <limits>
#include <queue>
#include <set>
#include <string_view>
#include <unordered_map>

namespace minera {

namespace {

using Op = ProcessTreeOperator;
using Kind = ProcessTreeApproximatedSplitKind;
using Sets = ProcessTreeApproximatedSets;
using Moves = ProcessTreeApproximatedMoves;
using Trace = std::vector<std::string>;

constexpr std::string_view kSkip = ">>";

bool supported(Op op) {
  return op == Op::kSequence || op == Op::kExclusive || op == Op::kParallel || op == Op::kLoop;
}

ProcessTreeNode binarize(const ProcessTreeNode& node) {
  if (node.op == Op::kActivity || node.op == Op::kTau) return node;
  if (!supported(node.op)) throw ProcessTreeApproximatedReferenceError("unsupported process-tree operator");
  ProcessTreeNode out{node.op, 0, {}};
  if (node.children.size() <= 2) {
    for (const auto& child : node.children) out.children.push_back(binarize(child));
    return out;
  }
  out.children.push_back(binarize(node.children.front()));
  ProcessTreeNode rest{node.op == Op::kLoop ? Op::kExclusive : node.op, 0,
                       {node.children.begin() + 1, node.children.end()}};
  out.children.push_back(binarize(rest));
  return out;
}

bool contains(const std::vector<std::string>& sorted, const std::string& value) {
  return std::binary_search(sorted.begin(), sorted.end(), value);
}

std::vector<std::string> unite(const std::vector<std::string>& a, const std::vector<std::string>& b) {
  std::vector<std::string> out;
  std::set_union(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(out));
  return out;
}

Sets sets_of(const ProcessTree& tree, const ProcessTreeNode& node, std::vector<Sets>& out) {
  const std::size_t slot = out.size();
  out.emplace_back();
  Sets result;
  if (node.op == Op::kActivity) {
    const auto& label = tree.activities().at(node.activity);
    result = {{label}, {label}, {label}, false, 1};
  } else if (node.op == Op::kTau) {
    result = {{}, {}, {}, true, 1};
  } else {
    if (node.children.size() != 2) {
      throw ProcessTreeApproximatedReferenceError("the approximated variant needs operators with two children");
    }
    const Sets first = sets_of(tree, node.children[0], out);
    const Sets second = sets_of(tree, node.children[1], out);
    result.activities = unite(first.activities, second.activities);
    result.height = 1 + std::max(first.height, second.height);
    switch (node.op) {
      case Op::kSequence:
        result.start = first.tau ? unite(first.start, second.start) : first.start;
        result.end = second.tau ? unite(first.end, second.end) : second.end;
        result.tau = first.tau && second.tau;
        break;
      case Op::kExclusive:
      case Op::kParallel:
        result.start = unite(first.start, second.start);
        result.end = unite(first.end, second.end);
        result.tau = node.op == Op::kExclusive ? first.tau || second.tau : first.tau && second.tau;
        break;
      default:  // loop
        result.start = first.tau ? unite(first.start, second.start) : first.start;
        result.end = first.tau ? unite(first.end, second.end) : first.end;
        result.tau = first.tau;
        break;
    }
  }
  out[slot] = result;
  return result;
}

// ---------------------------------------------------------------------------
// The split programs.

std::string var(char family, std::size_t i, std::size_t j) {
  return std::string(1, family) + "_" + std::to_string(i) + "_" + std::to_string(j);
}
std::string var(char family, std::size_t j) { return std::string(1, family) + "_" + std::to_string(j); }

long long weight(const Sets& child) {
  if (child.tau) return 0;
  for (const auto& a : child.start) {
    if (contains(child.end, a)) return 1;
  }
  return 2;
}

class Builder {
 public:
  using Terms = std::vector<std::pair<std::string, double>>;
  void row(Terms terms, int sense, double rhs) { program.rows.push_back({std::move(terms), sense, -rhs}); }
  void cost(std::string name, double value) {
    if (value != 0.0) program.objective.emplace_back(std::move(name), value);
  }
  ProcessTreeApproximatedProgram program;
};

constexpr int kEq = 0, kLe = -1, kGe = 1;

void middle_rows(Builder& b, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      b.row({{var('e', i, j), 1}, {var('s', i, j), 1}, {var('v', i, j), 1}, {var('x', i, j), -1}}, kGe, 0);
      b.row({{var('v', i, j), 1}, {var('x', i, j), -1}}, kLe, 0);
      b.row({{var('e', i, j), 1}, {var('v', i, j), 1}}, kLe, 1);
      b.row({{var('s', i, j), 1}, {var('v', i, j), 1}}, kLe, 1);
    }
  }
}

void used_rows(Builder& b, std::size_t n) {
  for (std::size_t j = 0; j < 2; ++j) {
    for (std::size_t i = 0; i < n; ++i) b.row({{var('u', j), 1}, {var('x', i, j), -1}}, kGe, 0);
    Builder::Terms starts, ends;
    for (std::size_t i = 0; i < n; ++i) starts.emplace_back(var('s', i, j), -1);
    starts.emplace_back(var('u', j), 1);
    for (std::size_t i = 0; i < n; ++i) ends.emplace_back(var('e', i, j), -1);
    ends.emplace_back(var('u', j), 1);
    b.row(std::move(starts), kLe, 0);
    b.row(std::move(ends), kLe, 0);
  }
}

struct Mismatch {
  std::vector<std::array<int, 2>> a, sa, ea;
};

Mismatch mismatches(const Trace& trace, const std::vector<Sets>& children) {
  Mismatch m;
  for (const auto& event : trace) {
    std::array<int, 2> a{}, sa{}, ea{};
    for (std::size_t j = 0; j < 2; ++j) {
      a[j] = !contains(children[j].activities, event);
      sa[j] = !contains(children[j].start, event);
      ea[j] = !contains(children[j].end, event);
    }
    m.a.push_back(a);
    m.sa.push_back(sa);
    m.ea.push_back(ea);
  }
  return m;
}

void part_objective(Builder& b, const Trace& trace, const std::vector<Sets>& children) {
  const auto m = mismatches(trace, children);
  const std::size_t n = trace.size();
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < 2; ++j) b.cost(var('v', i, j), m.a[i][j]);
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < 2; ++j) b.cost(var('s', i, j), m.sa[i][j]);
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < 2; ++j) b.cost(var('e', i, j), m.ea[i][j]);
  long long constant = 0;
  for (std::size_t j = 0; j < 2; ++j) {
    const long long w = weight(children[j]);
    b.cost(var('u', j), -static_cast<double>(w));
    constant += w;
  }
  b.program.constant = static_cast<double>(constant);
}

void common_head(Builder& b, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) b.row({{var('x', i, 0), 1}, {var('x', i, 1), 1}}, kEq, 1);
  for (std::size_t j = 0; j < 2; ++j) {
    b.row({{var('s', 0, j), -1}, {var('x', 0, j), 1}}, kLe, 0);
    b.row({{var('e', n - 1, j), -1}, {var('x', n - 1, j), 1}}, kLe, 0);
  }
}

ProcessTreeApproximatedProgram sequence_program(const Trace& trace, const std::vector<Sets>& children) {
  const std::size_t n = trace.size();
  Builder b;
  common_head(b, n);
  for (std::size_t i = 1; i < n; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      b.row({{var('s', i, j), 1}, {var('x', i - 1, j), 1}, {var('x', i, j), -1}}, kGe, 0);
      b.row({{var('s', i, j), 1}, {var('x', i, j), -1}}, kLe, 0);
      b.row({{var('s', i, j), 1}, {var('x', i - 1, j), 1}}, kLe, 1);
    }
  }
  for (std::size_t i = 0; i < n; ++i) b.row({{var('s', i, 0), 1}, {var('s', i, 1), 1}}, kLe, 1);
  for (std::size_t i = 0; i + 1 < n; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      b.row({{var('e', i, j), 1}, {var('x', i, j), -1}, {var('x', i + 1, j), 1}}, kGe, 0);
      b.row({{var('e', i, j), 1}, {var('x', i, j), -1}}, kLe, 0);
      b.row({{var('e', i, j), 1}, {var('x', i + 1, j), 1}}, kLe, 1);
    }
  }
  for (std::size_t i = 0; i < n; ++i) b.row({{var('e', i, 0), 1}, {var('e', i, 1), 1}}, kLe, 1);
  for (std::size_t i = 0; i + 1 < n; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      Builder::Terms terms{{var('x', i, j), -1}};
      for (std::size_t l = j; l < 2; ++l) terms.emplace_back(var('x', i + 1, l), 1);
      b.row(std::move(terms), kGe, 0);
    }
  }
  used_rows(b, n);
  middle_rows(b, n);
  part_objective(b, trace, children);
  return std::move(b.program);
}

ProcessTreeApproximatedProgram parallel_program(const Trace& trace, const std::vector<Sets>& children) {
  const std::size_t n = trace.size();
  Builder b;
  common_head(b, n);
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      b.row({{var('s', i, j), 1}, {var('x', i, j), -1}}, kLe, 0);
      for (std::size_t l = 0; l < i; ++l) b.row({{var('s', i, j), 1}, {var('x', l, j), 1}}, kLe, 1);
    }
    b.row({{var('s', i, 0), 1}, {var('s', i, 1), 1}}, kLe, 1);
  }
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      b.row({{var('e', i, j), 1}, {var('x', i, j), -1}}, kLe, 0);
      for (std::size_t l = i + 1; l < n; ++l) b.row({{var('e', i, j), 1}, {var('x', l, j), 1}}, kLe, 1);
    }
    b.row({{var('e', i, 0), 1}, {var('e', i, 1), 1}}, kLe, 1);
  }
  used_rows(b, n);
  middle_rows(b, n);
  part_objective(b, trace, children);
  return std::move(b.program);
}

// The objective coefficient of an empty execution, times 100000.
long long empty_cost_scaled(const Sets& child) {
  const long long w = weight(child);
  return w ? w * 100000 : -1;
}

ProcessTreeApproximatedProgram loop_program(const Trace& trace, const std::vector<Sets>& children) {
  const std::size_t n = trace.size();
  Builder b;
  b.row({{var('t', 0, 1), 1}}, kEq, 0);
  b.row({{var('t', n, 1), 1}}, kEq, 0);
  b.row({{var('t', 0, 0), -1}, {var('x', 0, 0), -1}}, kLe, -1);
  b.row({{var('t', n, 0), -1}, {var('x', n - 1, 0), -1}}, kLe, -1);
  for (std::size_t i = 0; i < n; ++i) {
    for (char family : {'x', 's', 'e', 'v'}) {
      b.row({{var(family, i, 0), 1}, {var(family, i, 1), 1}}, family == 'x' ? kEq : kLe, 1);
    }
  }
  for (std::size_t i = 0; i <= n; ++i) b.row({{var('t', i, 0), 1}, {var('t', i, 1), 1}}, kLe, 1);
  for (std::size_t i = 1; i < n; ++i)
    for (std::size_t j = 0; j < 2; ++j) b.row({{var('t', i, j), 1}, {var('x', i - 1, 1 - j), -1}}, kLe, 0);
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < 2; ++j) b.row({{var('t', i, j), 1}, {var('x', i, 1 - j), -1}}, kLe, 0);
  b.row({{var('t', n, 0), 1}, {var('x', n - 1, 1), -1}}, kLe, 0);
  for (std::size_t i = 0; i + 1 < n; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      b.row({{var('n', i, j), 1}, {var('t', i + 1, 1 - j), -1}, {var('x', i + 1, 1 - j), -1}}, kLe, 0);
      b.row({{var('n', i, j), 1}, {var('x', i + 1, 1 - j), -1}}, kGe, 0);
      b.row({{var('n', i, j), 1}, {var('t', i + 1, 1 - j), -1}}, kGe, 0);
    }
  }
  for (std::size_t j = 0; j < 2; ++j) b.row({{var('n', n - 1, j), 1}}, kEq, 1);
  b.row({{var('n', n - 1, 0), -1}, {var('t', n, 1), 1}}, kLe, 0);
  b.row({{var('n', n - 1, 1), -1}, {var('t', n, 0), 1}}, kLe, 0);
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      b.row({{var('e', i, j), 1}, {var('n', i, j), -1}}, kLe, 0);
      b.row({{var('e', i, j), 1}, {var('x', i, j), -1}}, kLe, 0);
      b.row({{var('e', i, j), 1}, {var('n', i, j), -1}, {var('x', i, j), -1}}, kGe, -1);
    }
  }
  for (std::size_t j = 0; j < 2; ++j) b.row({{var('p', 0, j), 1}}, kEq, 1);
  for (std::size_t i = 1; i < n; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      b.row({{var('p', i, j), 1}, {var('t', i, 1 - j), -1}, {var('x', i - 1, 1 - j), -1}}, kLe, 0);
      b.row({{var('p', i, j), 1}, {var('t', i, 1 - j), -1}}, kGe, 0);
      b.row({{var('p', i, j), 1}, {var('x', i - 1, 1 - j), -1}}, kGe, 0);
    }
  }
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      b.row({{var('p', i, j), -1}, {var('s', i, j), 1}, {var('x', i, j), -1}}, kGe, -1);
      b.row({{var('p', i, j), -1}, {var('s', i, j), 1}}, kLe, 0);
      b.row({{var('s', i, j), 1}, {var('x', i, j), -1}}, kLe, 0);
    }
  }
  b.row({{var('s', 0, 0), -1}, {var('t', 0, 0), -1}}, kLe, -1);
  middle_rows(b, n);
  const auto m = mismatches(trace, children);
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < 2; ++j) b.cost(var('s', i, j), m.sa[i][j]);
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < 2; ++j) b.cost(var('e', i, j), m.ea[i][j]);
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < 2; ++j) b.cost(var('v', i, j), m.a[i][j]);
  for (std::size_t i = 0; i <= n; ++i)
    for (std::size_t j = 0; j < 2; ++j) b.cost(var('t', i, j), empty_cost_scaled(children[j]) / 100000.0);
  b.program.constant = 0.0;
  return std::move(b.program);
}

void check_split_input(const Trace& trace, const std::vector<Sets>& children) {
  if (trace.empty()) throw ProcessTreeApproximatedReferenceError("an operator cannot split the empty trace");
  if (children.size() != 2) throw ProcessTreeApproximatedReferenceError("a split needs two children");
}

// Cost (times 100000) of a part of one child given by event positions.
long long part_cost(const Mismatch& m, const std::vector<std::size_t>& part, std::size_t j) {
  if (part.size() == 1) return (m.sa[part[0]][j] + m.ea[part[0]][j]) * 100000LL;
  long long c = m.sa[part.front()][j] + m.ea[part.back()][j];
  for (std::size_t k = 1; k + 1 < part.size(); ++k) c += m.a[part[k]][j];
  return c * 100000;
}

constexpr long long kInfinity = std::numeric_limits<long long>::max() / 4;

std::pair<ProcessTreeApproximatedSplit, long long> sequence_split(const Trace& trace,
                                                                  const std::vector<Sets>& children) {
  const auto m = mismatches(trace, children);
  const std::size_t n = trace.size();
  // Child 0 takes events [0, cut), child 1 the rest.
  long long best = kInfinity;
  std::size_t best_cut = 0;
  for (std::size_t cut = 0; cut <= n; ++cut) {
    long long c = 0;
    for (std::size_t j = 0; j < 2; ++j) {
      std::vector<std::size_t> part;
      for (std::size_t i = j ? cut : 0; i < (j ? n : cut); ++i) part.push_back(i);
      c += part.empty() ? weight(children[j]) * 100000 : part_cost(m, part, j);
    }
    if (c < best) {
      best = c;
      best_cut = cut;
    }
  }
  ProcessTreeApproximatedSplit split;
  for (std::size_t i = 0; i < n; ++i) split.assignment.push_back(i < best_cut ? 0 : 1);
  return {split, best};
}

std::pair<ProcessTreeApproximatedSplit, long long> parallel_split(const Trace& trace,
                                                                  const std::vector<Sets>& children) {
  const auto m = mismatches(trace, children);
  const std::size_t n = trace.size();
  // Per child: 0 not started, 1 open, 2 closed; a state packs both children.
  constexpr std::size_t kStates = 9;
  std::vector<std::array<long long, kStates>> cost(n + 1);
  std::vector<std::array<std::pair<std::size_t, std::size_t>, kStates>> from(n + 1);  // (state, child)
  for (auto& row : cost) row.fill(kInfinity);
  cost[0][0] = 0;
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t state = 0; state < kStates; ++state) {
      if (cost[i][state] >= kInfinity) continue;
      for (std::size_t j = 0; j < 2; ++j) {
        const std::size_t mine = j ? state / 3 : state % 3;
        if (mine == 2) continue;
        for (std::size_t close = 0; close < 2; ++close) {
          long long c = cost[i][state];
          if (mine == 0) c += m.sa[i][j] * 100000LL;
          if (close) c += m.ea[i][j] * 100000LL;
          if (mine == 1 && !close) c += m.a[i][j] * 100000LL;
          const std::size_t next_mine = close ? 2 : 1;
          const std::size_t next = j ? (state % 3) + 3 * next_mine : next_mine + 3 * (state / 3);
          if (c < cost[i + 1][next]) {
            cost[i + 1][next] = c;
            from[i + 1][next] = {state, j};
          }
        }
      }
    }
  }
  long long best = kInfinity;
  std::size_t best_state = 0;
  for (std::size_t state = 0; state < kStates; ++state) {
    if (cost[n][state] >= kInfinity || state % 3 == 1 || state / 3 == 1) continue;
    long long c = cost[n][state];
    if (state % 3 == 0) c += weight(children[0]) * 100000;
    if (state / 3 == 0) c += weight(children[1]) * 100000;
    if (c < best) {
      best = c;
      best_state = state;
    }
  }
  ProcessTreeApproximatedSplit split;
  split.assignment.assign(n, 0);
  for (std::size_t i = n, state = best_state; i > 0; --i) {
    split.assignment[i - 1] = from[i][state].second;
    state = from[i][state].first;
  }
  return {split, best};
}

std::pair<ProcessTreeApproximatedSplit, long long> loop_split(const Trace& trace, const std::vector<Sets>& children) {
  const auto m = mismatches(trace, children);
  const std::size_t n = trace.size();
  const std::array<long long, 2> empty{empty_cost_scaled(children[0]), empty_cost_scaled(children[1])};
  auto role = [&](std::size_t i, std::size_t j, bool first, bool last) -> long long {
    long long c = 0;
    if (first) c += m.sa[i][j];
    if (last) c += m.ea[i][j];
    if (!first && !last) c += m.a[i][j];
    return c * 100000;
  };
  // State after placing event i: (its child, whether it starts its segment);
  // its own cost is added once the next gap decides whether it ends there.
  struct Back {
    std::size_t state = 0;
    std::optional<std::size_t> empty;
  };
  std::vector<std::array<long long, 4>> cost(n);
  std::vector<std::array<Back, 4>> from(n);
  for (auto& row : cost) row.fill(kInfinity);
  for (std::size_t j = 0; j < 2; ++j) {
    const std::size_t state = j * 2 + 1;  // starts its segment
    cost[0][state] = j == 1 ? empty[0] : 0;
    from[0][state] = {0, j == 1 ? std::optional<std::size_t>(0) : std::nullopt};
  }
  for (std::size_t i = 1; i < n; ++i) {
    for (std::size_t state = 0; state < 4; ++state) {
      if (cost[i - 1][state] >= kInfinity) continue;
      const std::size_t c = state / 2;
      const bool started = state % 2;
      for (std::size_t d = 0; d < 2; ++d) {
        std::vector<std::optional<std::size_t>> gaps{std::nullopt};
        if (d == c) gaps.push_back(1 - c);
        for (const auto& gap : gaps) {
          const bool ends = d != c || gap.has_value();
          long long total = cost[i - 1][state] + role(i - 1, c, started, ends);
          if (gap) total += empty[*gap];
          const std::size_t next = d * 2 + (ends ? 1 : 0);
          if (total < cost[i][next]) {
            cost[i][next] = total;
            from[i][next] = {state, gap};
          }
        }
      }
    }
  }
  long long best = kInfinity;
  std::size_t best_state = 0;
  for (std::size_t state = 0; state < 4; ++state) {
    if (cost[n - 1][state] >= kInfinity) continue;
    const std::size_t c = state / 2;
    long long total = cost[n - 1][state] + role(n - 1, c, state % 2, true) + (c == 1 ? empty[0] : 0);
    if (total < best) {
      best = total;
      best_state = state;
    }
  }
  ProcessTreeApproximatedSplit split;
  split.assignment.assign(n, 0);
  split.empty.assign(n + 1, std::nullopt);
  if (best_state / 2 == 1) split.empty[n] = 0;
  for (std::size_t i = n, state = best_state; i > 0; --i) {
    split.assignment[i - 1] = state / 2;
    split.empty[i - 1] = from[i - 1][state].empty;
    state = from[i - 1][state].state;
  }
  return {split, best};
}

// ---------------------------------------------------------------------------
// The bordered net and its Dijkstra alignment.

struct NetTransition {
  std::optional<std::string> label;  // nullopt for silent
  bool leaf = false;
  std::vector<std::size_t> inputs, outputs;
};

struct BorderedNet {
  std::size_t places = 0;
  std::vector<NetTransition> transitions;
  std::size_t source = 0, sink = 0;
};

class NetBuilder {
 public:
  explicit NetBuilder(const ProcessTree& tree) : tree_(tree) {}

  BorderedNet build(const ProcessTreeNode& root) {
    net_.source = place();
    net_.sink = place();
    add(root, net_.source, net_.sink);
    return std::move(net_);
  }

 private:
  std::size_t place() { return net_.places++; }

  void silent(std::vector<std::size_t> inputs, std::vector<std::size_t> outputs) {
    net_.transitions.push_back({std::nullopt, false, std::move(inputs), std::move(outputs)});
  }

  void add(const ProcessTreeNode& node, std::size_t in, std::size_t out) {
    if (node.op == Op::kActivity || node.op == Op::kTau) {
      std::optional<std::string> label;
      if (node.op == Op::kActivity) label = tree_.activities().at(node.activity);
      net_.transitions.push_back({label, true, {in}, {out}});
      return;
    }
    if (node.op == Op::kParallel) {
      std::vector<std::size_t> starts, ends;
      for (std::size_t k = 0; k < node.children.size(); ++k) {
        starts.push_back(place());
        ends.push_back(place());
      }
      silent({in}, starts);
      silent(ends, {out});
      for (std::size_t k = 0; k < node.children.size(); ++k) add(node.children[k], starts[k], ends[k]);
      return;
    }
    const std::size_t a = place(), b = place();
    silent({in}, {a});
    silent({b}, {out});
    if (node.op == Op::kSequence) {
      std::size_t current = a;
      for (std::size_t k = 0; k < node.children.size(); ++k) {
        const std::size_t next = k + 1 == node.children.size() ? b : place();
        add(node.children[k], current, next);
        current = next;
      }
    } else if (node.op == Op::kExclusive) {
      for (const auto& child : node.children) add(child, a, b);
    } else if (node.op == Op::kLoop) {
      add(node.children.front(), a, b);
      for (std::size_t k = 1; k < node.children.size(); ++k) add(node.children[k], b, a);
    } else {
      throw ProcessTreeApproximatedReferenceError("unsupported process-tree operator");
    }
  }

  const ProcessTree& tree_;
  BorderedNet net_;
};

std::pair<Moves, long long> dijkstra(const BorderedNet& net, const Trace& trace) {
  using Marking = std::vector<std::uint8_t>;
  struct State {
    Marking marking;
    std::size_t index;
    long long cost;
    std::size_t parent;
    // The move into this state: a transition, or none for a log move.
    std::optional<std::size_t> transition;
    bool sync;
  };
  std::vector<State> states;
  std::unordered_map<std::string, std::size_t> seen;
  auto key = [](const Marking& marking, std::size_t index) {
    std::string out(marking.begin(), marking.end());
    out += '|';
    out += std::to_string(index);
    return out;
  };
  using Entry = std::pair<long long, std::size_t>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue;
  std::vector<bool> closed;
  auto push = [&](Marking marking, std::size_t index, long long cost, std::size_t parent,
                  std::optional<std::size_t> transition, bool sync) {
    const auto k = key(marking, index);
    const auto found = seen.find(k);
    if (found != seen.end()) {
      State& old = states[found->second];
      if (closed[found->second] || old.cost <= cost) return;
      old.cost = cost;
      old.parent = parent;
      old.transition = transition;
      old.sync = sync;
      queue.emplace(cost, found->second);
      return;
    }
    seen.emplace(k, states.size());
    states.push_back({std::move(marking), index, cost, parent, transition, sync});
    closed.push_back(false);
    queue.emplace(cost, states.size() - 1);
  };
  Marking start(net.places, 0);
  start[net.source] = 1;
  push(start, 0, 0, 0, std::nullopt, false);
  Marking goal(net.places, 0);
  goal[net.sink] = 1;
  std::optional<std::size_t> reached;
  while (!queue.empty()) {
    const auto [cost, id] = queue.top();
    queue.pop();
    if (closed[id] || cost != states[id].cost) continue;
    closed[id] = true;
    const Marking marking = states[id].marking;
    const std::size_t index = states[id].index;
    if (index == trace.size() && marking == goal) {
      reached = id;
      break;
    }
    if (index < trace.size()) push(marking, index + 1, cost + 10000, id, std::nullopt, false);
    for (std::size_t t = 0; t < net.transitions.size(); ++t) {
      const auto& transition = net.transitions[t];
      if (!std::all_of(transition.inputs.begin(), transition.inputs.end(),
                       [&](std::size_t p) { return marking[p] > 0; }))
        continue;
      Marking next = marking;
      for (std::size_t p : transition.inputs) --next[p];
      for (std::size_t p : transition.outputs) ++next[p];
      if (!transition.label) {
        push(next, index, cost + 1, id, t, false);
        continue;
      }
      push(next, index, cost + 10000, id, t, false);
      if (index < trace.size() && trace[index] == *transition.label) push(next, index + 1, cost, id, t, true);
    }
  }
  if (!reached) throw ProcessTreeApproximatedReferenceError("the bordered net cannot reach its final marking");
  Moves moves;
  for (std::size_t id = *reached; id != 0; id = states[id].parent) {
    const State& state = states[id];
    if (!state.transition) {
      moves.push_back({trace[state.index - 1], std::string(kSkip)});
      continue;
    }
    const auto& transition = net.transitions[*state.transition];
    if (!transition.leaf) continue;
    if (state.sync) {
      moves.push_back({*transition.label, *transition.label});
    } else {
      moves.push_back({std::string(kSkip), transition.label});
    }
  }
  std::reverse(moves.begin(), moves.end());
  return {moves, states[*reached].cost};
}

// ---------------------------------------------------------------------------
// The recursion.

struct Indexed {
  const ProcessTreeNode* node;
  std::vector<std::size_t> children;  // preorder indices
};

void index_nodes(const ProcessTreeNode& node, std::vector<Indexed>& out) {
  const std::size_t slot = out.size();
  out.push_back({&node, {}});
  for (const auto& child : node.children) {
    out[slot].children.push_back(out.size());
    index_nodes(child, out);
  }
}

class Aligner {
 public:
  Aligner(const ProcessTree& binary, const ProcessTreeApproximatedOptions& options,
          const ProcessTreeApproximatedOracle& oracle)
      : tree_(binary), options_(options), oracle_(oracle), sets_(reference_approximated_sets(binary)) {
    index_nodes(binary.root(), nodes_);
  }

  std::optional<Moves> align(std::size_t id, const Trace& trace) {
    const Sets& sets = sets_[id];
    if (trace.size() <= options_.max_trace_length || sets.height <= options_.max_tree_height) {
      if (oracle_.base) return oracle_.base(id, trace);
      return reference_approximated_optimal_alignment(tree_, *nodes_[id].node, trace).first;
    }
    const ProcessTreeNode& node = *nodes_[id].node;
    std::vector<Sets> children;
    for (std::size_t child : nodes_[id].children) children.push_back(sets_[child]);
    std::optional<Moves> result;
    if (node.op == Op::kExclusive) {
      const std::size_t pick = reference_approximated_choice(trace, children);
      if (oracle_.choice) oracle_.choice(id, trace, nodes_[id].children[pick]);
      result = align(nodes_[id].children[pick], trace);
    } else if (node.op == Op::kSequence || node.op == Op::kParallel || node.op == Op::kLoop) {
      const Kind kind = node.op == Op::kSequence ? Kind::kSequence
                        : node.op == Op::kParallel ? Kind::kParallel
                                                   : Kind::kLoop;
      ProcessTreeApproximatedSplit split;
      if (oracle_.solve) {
        const auto program = reference_approximated_program(kind, trace, children);
        split = reference_approximated_split_from_solution(kind, trace.size(), oracle_.solve(id, kind, trace, program));
      } else {
        split = reference_approximated_optimal_split(kind, trace, children).first;
      }
      std::vector<Moves> parts;
      bool none = false;
      for (const auto& [child, subtrace] : reference_approximated_plan(kind, trace, split)) {
        auto part = align(nodes_[id].children[child], subtrace);
        if (!part) {
          none = true;
          break;
        }
        parts.push_back(std::move(*part));
      }
      if (!none) result = reference_approximated_compose(kind, trace, split, parts);
    } else {
      return std::nullopt;  // a leaf the trace is too long for
    }
    if (oracle_.returned) oracle_.returned(id, result);
    return result;
  }

 private:
  const ProcessTree& tree_;
  const ProcessTreeApproximatedOptions& options_;
  const ProcessTreeApproximatedOracle& oracle_;
  std::vector<Sets> sets_;
  std::vector<Indexed> nodes_;
};

}  // namespace

ProcessTree reference_approximated_binary_tree(const ProcessTree& tree) {
  return ProcessTree(tree.activities(), binarize(tree.root()));
}

std::vector<ProcessTreeApproximatedSets> reference_approximated_sets(const ProcessTree& binary) {
  std::vector<Sets> out;
  sets_of(binary, binary.root(), out);
  return out;
}

ProcessTreeApproximatedProgram reference_approximated_program(ProcessTreeApproximatedSplitKind kind,
                                                              const std::vector<std::string>& trace,
                                                              const std::vector<ProcessTreeApproximatedSets>& children) {
  check_split_input(trace, children);
  switch (kind) {
    case Kind::kSequence:
      return sequence_program(trace, children);
    case Kind::kParallel:
      return parallel_program(trace, children);
    default:
      return loop_program(trace, children);
  }
}

double reference_approximated_objective(const ProcessTreeApproximatedProgram& program,
                                        const std::map<std::string, double>& solution) {
  double value = program.constant;
  for (const auto& [name, coefficient] : program.objective) {
    const auto found = solution.find(name);
    if (found != solution.end()) value += coefficient * found->second;
  }
  return value;
}

std::pair<ProcessTreeApproximatedSplit, double> reference_approximated_optimal_split(
    ProcessTreeApproximatedSplitKind kind, const std::vector<std::string>& trace,
    const std::vector<ProcessTreeApproximatedSets>& children) {
  check_split_input(trace, children);
  const auto [split, scaled] = kind == Kind::kSequence   ? sequence_split(trace, children)
                               : kind == Kind::kParallel ? parallel_split(trace, children)
                                                         : loop_split(trace, children);
  return {split, static_cast<double>(scaled) / 100000.0};
}

ProcessTreeApproximatedSplit reference_approximated_split_from_solution(ProcessTreeApproximatedSplitKind kind,
                                                                        std::size_t events,
                                                                        const std::map<std::string, double>& solution) {
  auto one = [&](const std::string& name) {
    const auto found = solution.find(name);
    return found != solution.end() && found->second > 0.5;
  };
  ProcessTreeApproximatedSplit split;
  for (std::size_t i = 0; i < events; ++i) split.assignment.push_back(one(var('x', i, 1)) ? 1 : 0);
  if (kind == Kind::kLoop) {
    for (std::size_t i = 0; i <= events; ++i) {
      std::optional<std::size_t> empty;
      for (std::size_t j = 0; j < 2; ++j)
        if (one(var('t', i, j))) empty = j;
      split.empty.push_back(empty);
    }
  }
  return split;
}

std::vector<std::pair<std::size_t, std::vector<std::string>>> reference_approximated_plan(
    ProcessTreeApproximatedSplitKind kind, const std::vector<std::string>& trace,
    const ProcessTreeApproximatedSplit& split) {
  std::vector<std::pair<std::size_t, Trace>> calls;
  if (kind != Kind::kLoop) {
    for (std::size_t j = 0; j < 2; ++j) {
      Trace part;
      for (std::size_t i = 0; i < trace.size(); ++i)
        if (split.assignment[i] == j) part.push_back(trace[i]);
      calls.emplace_back(j, std::move(part));
    }
    return calls;
  }
  bool open = false;
  for (std::size_t i = 0; i <= trace.size(); ++i) {
    if (i < split.empty.size() && split.empty[i]) {
      calls.emplace_back(*split.empty[i], Trace{});
      open = false;
    }
    if (i == trace.size()) break;
    if (open && calls.back().first == split.assignment[i]) {
      calls.back().second.push_back(trace[i]);
    } else {
      calls.emplace_back(split.assignment[i], Trace{trace[i]});
      open = true;
    }
  }
  return calls;
}

ProcessTreeApproximatedMoves reference_approximated_compose(ProcessTreeApproximatedSplitKind kind,
                                                            const std::vector<std::string>& trace,
                                                            const ProcessTreeApproximatedSplit& split,
                                                            const std::vector<ProcessTreeApproximatedMoves>& children) {
  Moves out;
  if (kind != Kind::kParallel) {
    for (const auto& part : children) out.insert(out.end(), part.begin(), part.end());
    return out;
  }
  std::vector<std::size_t> next(children.size(), 0);
  for (std::size_t i = 0; i < trace.size(); ++i) {
    const std::size_t j = split.assignment[i];
    while (next[j] < children[j].size()) {
      const auto& move = children[j][next[j]++];
      out.push_back(move);
      if (move.log != kSkip) break;
    }
  }
  for (std::size_t j = 0; j < children.size(); ++j)
    out.insert(out.end(), children[j].begin() + static_cast<std::ptrdiff_t>(next[j]), children[j].end());
  return out;
}

std::size_t reference_approximated_choice(const std::vector<std::string>& trace,
                                          const std::vector<ProcessTreeApproximatedSets>& children) {
  if (trace.empty()) throw ProcessTreeApproximatedReferenceError("a choice cannot route the empty trace");
  std::size_t best = 0;
  std::size_t lowest = std::numeric_limits<std::size_t>::max();
  for (std::size_t j = 0; j < children.size(); ++j) {
    std::size_t mismatches = !contains(children[j].start, trace.front()) + !contains(children[j].end, trace.back());
    for (std::size_t i = 1; i + 1 < trace.size(); ++i) mismatches += !contains(children[j].activities, trace[i]);
    if (mismatches < lowest) {
      lowest = mismatches;
      best = j;
    }
  }
  return best;
}

std::pair<ProcessTreeApproximatedMoves, long long> reference_approximated_optimal_alignment(
    const ProcessTree& tree, const ProcessTreeNode& node, const std::vector<std::string>& trace) {
  return dijkstra(NetBuilder(tree).build(node), trace);
}

std::size_t reference_approximated_cost(const ProcessTreeApproximatedMoves& alignment) {
  std::size_t cost = 0;
  for (const auto& move : alignment) {
    if ((move.log == kSkip && move.model) || (move.model && *move.model == kSkip)) ++cost;
  }
  return cost;
}

std::vector<std::optional<ProcessTreeApproximatedAlignment>> reference_approximated_alignments(
    const ProcessTree& tree, const std::vector<std::vector<std::string>>& traces,
    const ProcessTreeApproximatedOptions& options, const ProcessTreeApproximatedOracle& oracle,
    const std::function<bool()>& cancelled) {
  const ProcessTree binary = reference_approximated_binary_tree(tree);
  Aligner aligner(binary, options, oracle);
  const double best_worst = static_cast<double>(dijkstra(NetBuilder(binary).build(binary.root()), {}).second);
  std::map<Trace, std::optional<ProcessTreeApproximatedAlignment>> done;
  std::vector<std::optional<ProcessTreeApproximatedAlignment>> out;
  for (const auto& trace : traces) {
    throw_if_cancelled(cancelled, "process-tree alignment was cancelled");
    auto found = done.find(trace);
    if (found == done.end()) {
      std::optional<ProcessTreeApproximatedAlignment> result;
      if (auto moves = aligner.align(0, trace)) {
        result = ProcessTreeApproximatedAlignment{std::move(*moves), 0, 0.0};
        result->cost = reference_approximated_cost(result->alignment);
        result->fitness = 1.0 - static_cast<double>(result->cost) / (best_worst + static_cast<double>(trace.size()));
      }
      found = done.emplace(trace, std::move(result)).first;
    }
    out.push_back(found->second);
  }
  return out;
}

}  // namespace minera
