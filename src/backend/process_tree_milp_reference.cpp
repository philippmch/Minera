#include "minera/process_tree_milp_reference.hpp"
#include "minera/cancellation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace minera {

namespace {

using Op = ProcessTreeOperator;

bool is_tau(const ProcessTreeNode& node) { return node.op == Op::kTau; }

// A networkx MultiDiGraph: nodes in insertion order, per node its neighbours
// in first-edge order, per neighbour its parallel edges by key.
class FlowGraph {
 public:
  explicit FlowGraph(const ProcessTree& tree) : tree_(tree) {}

  std::size_t allocate() { return counter_++; }

  ProcessTreeMilpGraphNode& node(std::size_t id) {
    const auto found = index_.find(id);
    if (found != index_.end()) return nodes_[found->second];
    index_.emplace(id, nodes_.size());
    nodes_.push_back({});
    nodes_.back().id = id;
    adjacency_.emplace_back();
    return nodes_.back();
  }

  std::array<std::size_t, 3> edge(std::size_t from, std::size_t to, std::optional<std::string> label,
                                  double capacity, long long cost) {
    node(from);
    node(to);
    auto& neighbours = adjacency_[index_.at(from)];
    auto slot = std::find_if(neighbours.begin(), neighbours.end(), [&](const auto& n) { return n.first == to; });
    if (slot == neighbours.end()) {
      neighbours.emplace_back(to, std::vector<std::size_t>{});
      slot = neighbours.end() - 1;
    }
    slot->second.push_back(edges_.size());
    edges_.push_back({from, to, std::move(label), capacity, cost});
    return {from, to, slot->second.size() - 1};
  }

  void build(const ProcessTreeNode& tree, std::size_t start, std::size_t end, long long iac) {
    const double capacity = 1.0 / static_cast<double>(iac);
    switch (tree.op) {
      case Op::kTau:
        edge(start, end, std::nullopt, capacity, 0);
        return;
      case Op::kActivity:
        edge(start, end, tree_.activities().at(tree.activity), capacity, iac);
        return;
      case Op::kSequence: {
        std::vector<std::size_t> points{start};
        for (std::size_t k = 0; k + 1 < tree.children.size(); ++k) points.push_back(allocate());
        points.push_back(end);
        for (std::size_t k = 0; k < tree.children.size(); ++k) build(tree.children[k], points[k], points[k + 1], iac);
        return;
      }
      case Op::kExclusive:
        for (const auto& child : tree.children) build(child, start, end, iac);
        return;
      case Op::kParallel: {
        const long long inner = iac * static_cast<long long>(tree.children.size());
        const double branch = 1.0 / static_cast<double>(inner);
        node(start);
        node(end);
        std::vector<std::array<std::size_t, 3>> splits, joins;
        for (const auto& child : tree.children) {
          const std::size_t child_start = allocate();
          const std::size_t child_end = allocate();
          node(child_start);
          node(child_end);
          splits.push_back(edge(start, child_start, std::nullopt, branch, 0));
          build(child, child_start, child_end, inner);
          joins.push_back(edge(child_end, end, std::nullopt, branch, 0));
        }
        record(start, std::move(splits), iac, true);
        record(end, std::move(joins), iac, false);
        return;
      }
      case Op::kLoop: {
        const std::size_t loop_start = allocate();
        const std::size_t loop_end = allocate();
        node(loop_start);
        node(loop_end);
        edge(start, loop_start, std::nullopt, capacity, 0);
        edge(loop_end, end, std::nullopt, capacity, 0);
        build(tree.children[0], loop_start, loop_end, iac);
        build(tree.children[1], loop_end, loop_start, iac);
        return;
      }
      default:
        throw ProcessTreeMilpReferenceError("the MILP aligner covers sequence, choice, parallel and loop operators");
    }
  }

  ProcessTreeMilpGraph finish() {
    ProcessTreeMilpGraph out;
    out.nodes = nodes_;
    for (const auto& neighbours : adjacency_) {
      for (const auto& [to, list] : neighbours) {
        for (std::size_t e : list) out.edges.push_back(edges_[e]);
      }
    }
    return out;
  }

 private:
  void record(std::size_t id, std::vector<std::array<std::size_t, 3>> group, long long iac, bool split) {
    auto& n = node(id);
    if (n.shuffle.empty()) {
      n.iac = iac;
      (split ? n.split : n.join) = true;
    }
    n.shuffle.push_back(std::move(group));
  }

  const ProcessTree& tree_;
  std::size_t counter_ = 0;
  std::map<std::size_t, std::size_t> index_;
  std::vector<ProcessTreeMilpGraphNode> nodes_;
  std::vector<std::vector<std::pair<std::size_t, std::vector<std::size_t>>>> adjacency_;
  std::vector<ProcessTreeMilpGraphEdge> edges_;
};

void check_operators(const ProcessTreeNode& node) {
  if (node.op == Op::kLoop && node.children.size() != 2) {
    throw ProcessTreeMilpReferenceError("the MILP aligner covers binary loops");
  }
  if (node.op != Op::kTau && node.op != Op::kActivity && node.children.empty()) {
    throw ProcessTreeMilpReferenceError("operator without children");
  }
  for (const auto& child : node.children) check_operators(child);
}

// ---------------------------------------------------------------------------
// Bounded primal simplex (dense tableau, Dantzig pricing with a Bland fallback
// on degenerate runs) and depth-first branch and bound.

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kEps = 1e-9;

struct LpSolution {
  bool feasible = false;
  double value = 0.0;
  std::vector<double> x;
};

class BoundedSimplex {
 public:
  BoundedSimplex(const ProcessTreeMilpProgram& program, const std::vector<double>& lower,
                 const std::vector<double>& upper)
      : variables_(program.objective.size()) {
    const std::size_t equalities = program.equalities.size();
    const std::size_t inequalities = program.inequalities.size();
    rows_ = equalities + inequalities;
    structural_ = variables_ + inequalities;  // structural plus slacks
    columns_ = structural_ + rows_;           // plus artificials
    width_ = columns_;
    tableau_.assign(rows_ * width_, 0.0);
    upper_.assign(columns_, kInf);
    cost_.assign(columns_, 0.0);
    for (std::size_t j = 0; j < variables_; ++j) {
      upper_[j] = upper[j] - lower[j];
      cost_[j] = program.objective[j];
    }
    std::vector<double> rhs(rows_, 0.0);
    auto load = [&](std::size_t r, const ProcessTreeMilpRow& row, double b) {
      double shifted = b;
      for (const auto& [j, value] : row) {
        at(r, j) += value;
        shifted -= value * lower[j];
      }
      rhs[r] = shifted;
    };
    for (std::size_t r = 0; r < equalities; ++r) load(r, program.equalities[r], program.equality_rhs[r]);
    for (std::size_t k = 0; k < inequalities; ++k) {
      load(equalities + k, program.inequalities[k], program.inequality_rhs[k]);
      at(equalities + k, variables_ + k) = 1.0;
    }
    basis_.resize(rows_);
    value_.resize(rows_);
    for (std::size_t r = 0; r < rows_; ++r) {
      if (rhs[r] < 0) {
        for (std::size_t j = 0; j < structural_; ++j) at(r, j) = -at(r, j);
        rhs[r] = -rhs[r];
      }
      at(r, structural_ + r) = 1.0;
      basis_[r] = structural_ + r;
      value_[r] = rhs[r];
    }
    at_upper_.assign(columns_, false);
    basic_.assign(columns_, false);
    for (std::size_t r = 0; r < rows_; ++r) basic_[basis_[r]] = true;
  }

  LpSolution solve() {
    LpSolution out;
    for (std::size_t j = 0; j < variables_; ++j) {
      if (upper_[j] < -kEps) return out;
    }
    // Phase 1: minimise the artificials.
    std::vector<double> phase1(columns_, 0.0);
    for (std::size_t r = 0; r < rows_; ++r) phase1[structural_ + r] = 1.0;
    if (!iterate(phase1)) return out;
    double infeasibility = 0.0;
    for (std::size_t r = 0; r < rows_; ++r) {
      if (basis_[r] >= structural_) infeasibility += value_[r];
    }
    if (infeasibility > 1e-7) return out;
    for (std::size_t r = 0; r < rows_; ++r) {
      if (basis_[r] < structural_) continue;
      for (std::size_t j = 0; j < structural_; ++j) {
        if (!basic_[j] && std::abs(at(r, j)) > 1e-7) {
          const double entering = at_upper_[j] ? upper_[j] : 0.0;
          pivot(r, j);
          value_[r] = entering;
          break;
        }
      }
    }
    for (std::size_t r = 0; r < rows_; ++r) upper_[structural_ + r] = 0.0;
    // Nonbasic artificials stay at zero and basic ones sit in redundant rows,
    // so phase 2 neither prices nor updates the artificial columns.
    columns_ = structural_;
    if (!iterate(cost_)) return out;
    out.feasible = true;
    out.x.assign(variables_, 0.0);
    for (std::size_t j = 0; j < variables_; ++j) {
      if (!basic_[j] && at_upper_[j]) out.x[j] = upper_[j];
    }
    for (std::size_t r = 0; r < rows_; ++r) {
      if (basis_[r] < variables_) out.x[basis_[r]] = value_[r];
    }
    for (std::size_t j = 0; j < variables_; ++j) out.value += cost_[j] * out.x[j];
    return out;
  }

 private:
  double& at(std::size_t r, std::size_t j) { return tableau_[r * width_ + j]; }

  void pivot(std::size_t r, std::size_t j) {
    double* source = &tableau_[r * width_];
    const double scale = source[j];
    nonzero_.clear();
    for (std::size_t k = 0; k < columns_; ++k) {
      if (source[k] == 0.0) continue;
      source[k] /= scale;
      nonzero_.push_back(k);
    }
    for (std::size_t i = 0; i < rows_; ++i) {
      if (i == r) continue;
      double* target = &tableau_[i * width_];
      const double factor = target[j];
      if (factor == 0.0) continue;
      for (std::size_t k : nonzero_) target[k] -= factor * source[k];
      target[j] = 0.0;
    }
    if (!reduced_.empty()) {
      const double factor = reduced_[j];
      if (factor != 0.0) {
        for (std::size_t k : nonzero_) reduced_[k] -= factor * source[k];
        reduced_[j] = 0.0;
      }
    }
    basic_[basis_[r]] = false;
    at_upper_[basis_[r]] = false;
    basis_[r] = j;
    basic_[j] = true;
    at_upper_[j] = false;
  }

  bool iterate(const std::vector<double>& cost) {
    std::size_t degenerate = 0;
    // Reduced costs d_j = c_j - c_B B^-1 A_j, kept up to date by pivot().
    reduced_.assign(cost.begin(), cost.end());
    for (std::size_t r = 0; r < rows_; ++r) {
      const double cb = cost[basis_[r]];
      if (cb == 0.0) continue;
      const double* row = &tableau_[r * width_];
      for (std::size_t j = 0; j < columns_; ++j) reduced_[j] -= cb * row[j];
    }
    const std::vector<double>& reduced = reduced_;
    for (std::size_t guard = 0; guard < 200000; ++guard) {
      const bool bland = degenerate > 50;
      std::size_t entering = columns_;
      double best = 0.0;
      for (std::size_t j = 0; j < columns_; ++j) {
        if (basic_[j] || upper_[j] <= kEps) continue;
        const double gain = at_upper_[j] ? reduced[j] : -reduced[j];
        if (gain <= 1e-9) continue;
        if (bland) {
          entering = j;
          break;
        }
        if (gain > best) {
          best = gain;
          entering = j;
        }
      }
      if (entering == columns_) {
        reduced_.clear();
        return true;
      }
      const double direction = at_upper_[entering] ? -1.0 : 1.0;
      double step = upper_[entering];
      std::size_t leaving = rows_;
      bool leaves_at_upper = false;
      for (std::size_t r = 0; r < rows_; ++r) {
        const double a = at(r, entering);
        if (std::abs(a) <= kEps) continue;
        const double delta = -direction * a;
        const std::size_t b = basis_[r];
        double limit = kInf;
        bool to_upper = false;
        if (delta < 0) {
          limit = std::max(value_[r], 0.0) / -delta;
        } else if (upper_[b] < kInf) {
          limit = std::max(upper_[b] - value_[r], 0.0) / delta;
          to_upper = true;
        }
        if (limit < step - 1e-12 || (limit <= step + 1e-12 && leaving < rows_ && b < basis_[leaving])) {
          step = limit;
          leaving = r;
          leaves_at_upper = to_upper;
        }
      }
      if (step == kInf) {
        reduced_.clear();
        return false;  // unbounded
      }
      degenerate = step <= 1e-12 ? degenerate + 1 : 0;
      for (std::size_t r = 0; r < rows_; ++r) value_[r] -= direction * at(r, entering) * step;
      if (leaving == rows_) {
        at_upper_[entering] = !at_upper_[entering];
        continue;
      }
      const double entering_value = at_upper_[entering] ? upper_[entering] - step : step;
      const std::size_t old = basis_[leaving];
      pivot(leaving, entering);
      value_[leaving] = entering_value;
      at_upper_[old] = leaves_at_upper;
    }
    return false;
  }

  std::size_t variables_ = 0, rows_ = 0, structural_ = 0, columns_ = 0, width_ = 0;
  std::vector<double> tableau_, upper_, cost_, value_, reduced_;
  std::vector<std::size_t> nonzero_;
  std::vector<std::size_t> basis_;
  std::vector<bool> at_upper_, basic_;
};

LpSolution solve_relaxation(const ProcessTreeMilpProgram& program, const std::vector<double>& lower,
                            const std::vector<double>& upper) {
  LpSolution out = BoundedSimplex(program, lower, upper).solve();
  if (out.feasible) {
    for (std::size_t j = 0; j < out.x.size(); ++j) out.x[j] += lower[j];
    out.value = 0.0;
    for (std::size_t j = 0; j < out.x.size(); ++j) out.value += program.objective[j] * out.x[j];
  }
  return out;
}

}  // namespace

ProcessTreeMilpGraph reference_milp_process_tree_graph(const ProcessTree& tree) {
  check_operators(tree.root());
  FlowGraph graph(tree);
  const ProcessTreeNode& root = tree.root();
  const bool tau_do = root.op == Op::kLoop && is_tau(root.children[0]);
  const std::size_t source = graph.allocate();
  const std::size_t sink = (is_tau(root) || tau_do) ? source : graph.allocate();
  graph.node(source).source = true;
  graph.node(sink).sink = true;
  if (is_tau(root)) {
    // A tau root leaves the source alone.
  } else if (root.op == Op::kLoop) {
    if (tau_do) {
      graph.build(root.children[1], source, source, 1);
    } else {
      graph.build(root.children[0], source, sink, 1);
      graph.build(root.children[1], sink, source, 1);
    }
  } else {
    graph.build(root, source, sink, 1);
  }
  return graph.finish();
}

ProcessTreeMilpProgram reference_milp_alignment_program(const ProcessTreeMilpGraph& graph,
                                                        const std::vector<std::string>& trace) {
  const std::size_t n = trace.size();
  const std::size_t edges = graph.edges.size();
  const std::size_t nodes = graph.nodes.size();
  std::map<std::size_t, std::size_t> position;
  for (std::size_t k = 0; k < nodes; ++k) position[graph.nodes[k].id] = k;
  ProcessTreeMilpProgram program;
  auto add = [&](double cost, double upper, bool integer) {
    program.objective.push_back(cost);
    program.lower.push_back(0.0);
    program.upper.push_back(upper);
    program.integer.push_back(integer);
    return program.objective.size() - 1;
  };
  auto model = [&](std::size_t p, std::size_t e) { return p * edges + e; };
  for (std::size_t p = 0; p <= n; ++p) {
    for (const auto& edge : graph.edges) add(static_cast<double>(edge.cost), edge.capacity, false);
  }
  const std::size_t log_base = program.objective.size();
  auto log = [&](std::size_t p, std::size_t v) { return log_base + p * nodes + v; };
  for (std::size_t p = 0; p < n; ++p) {
    for (std::size_t v = 0; v < nodes; ++v) add(1.0, 1.0, false);
  }
  // Synchronous variables per position, in edge order.
  std::vector<std::vector<std::pair<std::size_t, std::size_t>>> sync(n);  // (edge, variable)
  for (std::size_t p = 0; p < n; ++p) {
    for (std::size_t e = 0; e < edges; ++e) {
      const auto& edge = graph.edges[e];
      if (edge.label && *edge.label == trace[p]) {
        sync[p].emplace_back(e, add(1.0 - static_cast<double>(edge.cost), edge.capacity, false));
      }
    }
  }
  struct Group {
    std::size_t node;
    const std::vector<std::array<std::size_t, 3>>* edges;
  };
  std::vector<Group> groups;
  for (std::size_t v = 0; v < nodes; ++v) {
    for (const auto& group : graph.nodes[v].shuffle) groups.push_back({v, &group});
  }
  const std::size_t binary_base = program.objective.size();
  for (std::size_t p = 0; p <= n; ++p) {
    for (std::size_t g = 0; g < groups.size(); ++g) add(0.0, 1.0, true);
  }
  if (program.objective.empty()) return program;
  std::size_t source = 0, sink = 0;
  for (std::size_t v = 0; v < nodes; ++v) {
    if (graph.nodes[v].source) source = v;
    if (graph.nodes[v].sink) sink = v;
  }
  for (std::size_t p = 0; p <= n; ++p) {
    for (std::size_t v = 0; v < nodes; ++v) {
      std::map<std::size_t, double> row;
      const std::size_t id = graph.nodes[v].id;
      for (std::size_t e = 0; e < edges; ++e) {
        const auto& edge = graph.edges[e];
        if (edge.from == id) row[model(p, e)] -= 1.0;
        if (edge.to == id) row[model(p, e)] += 1.0;
      }
      if (p > 0) {
        for (const auto& [e, variable] : sync[p - 1]) {
          if (graph.edges[e].to == id) row[variable] += 1.0;
        }
      }
      if (p < n) {
        for (const auto& [e, variable] : sync[p]) {
          if (graph.edges[e].from == id) row[variable] -= 1.0;
        }
      }
      if (p < n) row[log(p, v)] -= 1.0;
      if (p > 0) row[log(p - 1, v)] += 1.0;
      ProcessTreeMilpRow sparse;
      for (const auto& [j, value] : row) {
        if (value != 0.0) sparse.emplace_back(j, value);
      }
      program.equalities.push_back(std::move(sparse));
      program.equality_rhs.push_back(v == source && p == 0 ? -1.0 : (v == sink && p == n ? 1.0 : 0.0));
    }
  }
  for (std::size_t g = 0; g < groups.size(); ++g) {
    for (std::size_t p = 0; p <= n; ++p) {
      std::map<std::size_t, double> row;
      for (const auto& [from, to, key] : *groups[g].edges) {
        std::size_t seen = 0;
        for (std::size_t e = 0; e < edges; ++e) {
          if (graph.edges[e].from != from || graph.edges[e].to != to) continue;
          if (seen++ == key) {
            row[model(p, e)] += 1.0;
            break;
          }
        }
      }
      row[binary_base + p * groups.size() + g] = -1.0 / static_cast<double>(graph.nodes[groups[g].node].iac);
      ProcessTreeMilpRow sparse(row.begin(), row.end());
      program.equalities.push_back(std::move(sparse));
      program.equality_rhs.push_back(0.0);
    }
  }
  for (std::size_t p = 0; p < n; ++p) {
    if (sync[p].size() < 2) continue;
    ProcessTreeMilpRow row;
    for (const auto& [e, variable] : sync[p]) row.emplace_back(variable, 1.0 / graph.edges[e].capacity);
    program.inequalities.push_back(std::move(row));
    program.inequality_rhs.push_back(1.0);
  }
  return program;
}

std::optional<std::vector<double>> solve_reference_milp_program(const ProcessTreeMilpProgram& original) {
  if (original.objective.empty()) return std::nullopt;
  // A row tying k flows to one binary (a shuffle group: sum x = z / iac with
  // every x <= 1 / (iac k)) forces each flow to z / (iac k) in an integer
  // solution. Adding those equalities leaves the integer optimum unchanged and
  // tightens the relaxation a lot.
  ProcessTreeMilpProgram program = original;
  for (const auto& row : original.equalities) {
    std::size_t binaries = 0, binary = 0;
    for (const auto& [j, value] : row) {
      if (original.integer[j]) {
        ++binaries;
        binary = j;
      }
    }
    if (binaries != 1 || row.size() < 2) continue;
    double scale = 0.0;
    for (const auto& [j, value] : row) {
      if (j == binary) scale = -value;
    }
    const double share = scale / static_cast<double>(row.size() - 1);
    for (const auto& [j, value] : row) {
      if (j == binary) continue;
      program.equalities.push_back({{std::min(j, binary), j < binary ? 1.0 : -share},
                                    {std::max(j, binary), j < binary ? -share : 1.0}});
      program.equality_rhs.push_back(0.0);
    }
  }
  // Best-first branch and bound: nodes carry their parent's relaxation bound.
  struct Node {
    double bound;
    std::vector<double> lower, upper;
  };
  auto worse = [](const Node& a, const Node& b) { return a.bound > b.bound; };
  std::vector<Node> open{{-kInf, program.lower, program.upper}};
  std::optional<std::vector<double>> best;
  double best_value = kInf;
  std::size_t explored = 0;
  while (!open.empty()) {
    std::pop_heap(open.begin(), open.end(), worse);
    Node node = std::move(open.back());
    open.pop_back();
    if (node.bound >= best_value - 1e-9) continue;
    if (++explored > 200000) throw ProcessTreeMilpReferenceError("branch and bound did not finish");
    LpSolution relaxed = solve_relaxation(program, node.lower, node.upper);
    if (!relaxed.feasible || relaxed.value >= best_value - 1e-9) continue;
    std::size_t branch = program.objective.size();
    double fraction = 0.0;
    for (std::size_t j = 0; j < relaxed.x.size(); ++j) {
      if (!program.integer[j]) continue;
      const double distance = std::abs(relaxed.x[j] - std::round(relaxed.x[j]));
      if (distance > 1e-6 && distance > fraction) {
        fraction = distance;
        branch = j;
      }
    }
    if (branch == program.objective.size()) {
      for (std::size_t j = 0; j < relaxed.x.size(); ++j) {
        if (program.integer[j]) relaxed.x[j] = std::round(relaxed.x[j]);
      }
      best_value = relaxed.value;
      best = std::move(relaxed.x);
      continue;
    }
    const double value = relaxed.x[branch];
    Node down{relaxed.value, node.lower, node.upper};
    Node up{relaxed.value, std::move(node.lower), std::move(node.upper)};
    down.upper[branch] = std::floor(value);
    up.lower[branch] = std::ceil(value);
    open.push_back(std::move(down));
    std::push_heap(open.begin(), open.end(), worse);
    open.push_back(std::move(up));
    std::push_heap(open.begin(), open.end(), worse);
  }
  return best;
}

ProcessTreeMilpAlignment reference_milp_alignment_from_solution(const ProcessTreeMilpGraph& graph,
                                                                const std::vector<std::string>& trace,
                                                                const ProcessTreeMilpProgram& program,
                                                                const std::vector<double>& solution) {
  if (solution.size() != program.objective.size()) {
    throw ProcessTreeMilpReferenceError("solution size does not match the program");
  }
  ProcessTreeMilpAlignment out;
  double cost = 0.0;
  for (std::size_t j = 0; j < solution.size(); ++j) cost += program.objective[j] * solution[j];
  out.cost = cost;
  std::size_t variable = graph.edges.size() * (trace.size() + 1) + graph.nodes.size() * trace.size();
  for (const auto& event : trace) {
    ProcessTreeMilpMove move{event, std::nullopt};
    for (const auto& edge : graph.edges) {
      if (!edge.label || *edge.label != event) continue;
      if (!move.model && solution.at(variable) > 1e-9) move.model = *edge.label;
      ++variable;
    }
    out.alignment.push_back(std::move(move));
  }
  return out;
}

ProcessTreeMilpAlignment reference_milp_align(const ProcessTree& tree, const std::vector<std::string>& trace) {
  const ProcessTreeMilpGraph graph = reference_milp_process_tree_graph(tree);
  const ProcessTreeMilpProgram program = reference_milp_alignment_program(graph, trace);
  if (program.objective.empty()) {
    ProcessTreeMilpAlignment out;
    for (const auto& event : trace) out.alignment.push_back({event, std::nullopt});
    return out;
  }
  const auto solution = solve_reference_milp_program(program);
  if (!solution) throw ProcessTreeMilpReferenceError("the alignment program is infeasible");
  return reference_milp_alignment_from_solution(graph, trace, program, *solution);
}

std::vector<ProcessTreeMilpAlignment> reference_milp_alignments(const ProcessTree& tree,
                                                                const std::vector<std::vector<std::string>>& traces,
                                                                const std::function<bool()>& cancelled) {
  // apply aligns each variant once, the empty trace first.
  std::map<std::vector<std::string>, ProcessTreeMilpAlignment> variants;
  const ProcessTreeMilpAlignment& empty = variants.emplace(std::vector<std::string>{}, reference_milp_align(tree, {}))
                                              .first->second;
  if (!empty.cost) throw ProcessTreeMilpReferenceError("the empty trace has no cost");
  const double floor = std::round((*empty.cost + 1e-14) * 1e13) / 1e13;
  std::vector<ProcessTreeMilpAlignment> out;
  for (const auto& trace : traces) {
    throw_if_cancelled(cancelled, "process-tree alignment was cancelled");
    auto found = variants.find(trace);
    if (found == variants.end()) found = variants.emplace(trace, reference_milp_align(tree, trace)).first;
    ProcessTreeMilpAlignment aligned = found->second;
    const double denominator = floor + static_cast<double>(trace.size());
    const double cost = aligned.cost.value_or(0.0);
    aligned.fitness = denominator > 0 ? 1.0 - cost / denominator : 0.0;
    out.push_back(std::move(aligned));
  }
  return out;
}

}  // namespace minera
