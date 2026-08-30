#include "minera/petri_analysis.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace minera {
namespace {

constexpr double kEps = 1e-8;
constexpr std::int64_t kMaxExactSolverInteger =
    static_cast<std::int64_t>(1ULL << 53);

// Select a deterministic, evenly spaced subset while retaining both ends.
// Endpoint additions intentionally allow up to two more points than the
// requested interior sample count.
std::vector<std::size_t> select_evenly_spaced_splits(
    const std::vector<std::size_t>& splits, std::size_t requested) {
  if (requested == 0 || splits.size() <= requested) return splits;

  const auto count = splits.size();
  std::vector<std::size_t> selected;
  selected.reserve(requested + 2);
  for (std::size_t i = 0; i < requested; ++i) {
    const auto point = i * count / requested + count / (2 * requested);
    selected.push_back(splits[point]);
  }
  if (std::find(selected.begin(), selected.end(), splits.front()) ==
      selected.end())
    selected.insert(selected.begin(), splits.front());
  if (std::find(selected.begin(), selected.end(), splits.back()) ==
      selected.end())
    selected.push_back(splits.back());
  return selected;
}

std::string validate_net(const PetriNet& net) {
  if (net.place_count() == 0) return "net has no places";
  if (net.source() >= net.place_count() || net.sink() >= net.place_count())
    return "source or sink place is outside the net";
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    const auto& transition = net.transitions()[t];
    if (transition.activity && *transition.activity >= net.activities().size())
      return "transition activity is outside the activity registry";
    for (const auto p : transition.inputs)
      if (p >= net.place_count()) return "transition input place is outside the net";
    for (const auto p : transition.outputs)
      if (p >= net.place_count()) return "transition output place is outside the net";
    for (const auto p : transition.reset_inputs)
      if (p >= net.place_count()) return "transition reset place is outside the net";
    for (const auto p : transition.inhibitor_inputs)
      if (p >= net.place_count()) return "transition inhibitor place is outside the net";
  }
  return {};
}

struct FlowEdge {
  std::size_t target;
  std::size_t reverse;
  int capacity;
};

class LimitedMaxFlow {
 public:
  explicit LimitedMaxFlow(std::size_t node_count) : graph_(node_count) {}

  void add_edge(std::size_t from, std::size_t to, int capacity) {
    FlowEdge forward{to, graph_[to].size(), capacity};
    FlowEdge reverse{from, graph_[from].size(), 0};
    graph_[from].push_back(forward);
    graph_[to].push_back(reverse);
  }

  int up_to_two(std::size_t source, std::size_t sink) {
    if (source == sink) return 2;
    int flow = 0;
    while (flow < 2) {
      level_.assign(graph_.size(), -1);
      std::queue<std::size_t> queue;
      level_[source] = 0;
      queue.push(source);
      while (!queue.empty()) {
        const auto node = queue.front();
        queue.pop();
        for (const auto& edge : graph_[node]) {
          if (edge.capacity <= 0 || level_[edge.target] >= 0) continue;
          level_[edge.target] = level_[node] + 1;
          queue.push(edge.target);
        }
      }
      if (level_[sink] < 0) break;
      next_edge_.assign(graph_.size(), 0);
      while (flow < 2) {
        const auto pushed = send(source, sink, 2 - flow);
        if (pushed == 0) break;
        flow += pushed;
      }
    }
    return flow;
  }

 private:
  int send(std::size_t node, std::size_t sink, int requested) {
    if (node == sink) return requested;
    for (auto& cursor = next_edge_[node]; cursor < graph_[node].size(); ++cursor) {
      auto& edge = graph_[node][cursor];
      if (edge.capacity <= 0 || level_[edge.target] != level_[node] + 1) continue;
      const auto pushed = send(edge.target, sink, std::min(requested, edge.capacity));
      if (pushed == 0) continue;
      edge.capacity -= pushed;
      graph_[edge.target][edge.reverse].capacity += pushed;
      return pushed;
    }
    return 0;
  }

  std::vector<std::vector<FlowEdge>> graph_;
  std::vector<int> level_;
  std::vector<std::size_t> next_edge_;
};

std::vector<std::vector<std::size_t>> structural_pair_diagnostics(
    const PetriNet& net) {
  const auto place_count = net.place_count();
  const auto transition_count = net.transitions().size();
  const auto element_count = place_count + transition_count;
  const auto node_count = element_count * 2;
  if (place_count == 0 || transition_count == 0) return {};

  // A node-split graph gives every model element unit capacity. A flow of two
  // is therefore two internally distinct structural routes in that direction.
  std::set<std::pair<std::size_t, std::size_t>> structural_edges;
  const auto place_in = [](std::size_t place) { return place * 2; };
  const auto transition_in = [place_count](std::size_t transition) {
    return (place_count + transition) * 2;
  };
  for (std::size_t transition = 0; transition < transition_count; ++transition) {
    const auto& model_transition = net.transitions()[transition];
    const auto transition_node = transition_in(transition);
    const auto add_input = [&](const auto& places) {
      for (const auto place : places)
        structural_edges.emplace(place_in(place) + 1, transition_node);
    };
    add_input(model_transition.inputs);
    add_input(model_transition.reset_inputs);
    add_input(model_transition.inhibitor_inputs);
    for (const auto place : model_transition.outputs)
      structural_edges.emplace(transition_node + 1, place_in(place));
  }

  const auto flow = [&](std::size_t source, std::size_t sink) {
    LimitedMaxFlow graph(node_count);
    for (std::size_t element = 0; element < element_count; ++element)
      graph.add_edge(element * 2, element * 2 + 1, 1);
    for (const auto [from, to] : structural_edges) graph.add_edge(from, to, 1);
    return graph.up_to_two(source, sink);
  };

  std::vector<std::vector<std::size_t>> result;
  for (std::size_t place = 0; place < place_count; ++place) {
    const auto place_node = place_in(place);
    for (std::size_t transition = 0; transition < transition_count; ++transition) {
      const auto transition_node = transition_in(transition);
      if (flow(place_node + 1, transition_node) > 1)
        result.push_back({place_node + 1, transition_node});
      if (flow(transition_node + 1, place_node) > 1)
        result.push_back({transition_node + 1, place_node});
    }
  }
  return result;
}

std::vector<std::string> structural_node_labels(const PetriNet& net) {
  const auto element_count = net.place_count() + net.transitions().size();
  std::vector<std::string> labels(element_count * 2);
  for (std::size_t place = 0; place < net.place_count(); ++place) {
    labels[place * 2] = "place:" + std::to_string(place) + ":in";
    labels[place * 2 + 1] = "place:" + std::to_string(place) + ":out";
  }
  for (std::size_t transition = 0; transition < net.transitions().size(); ++transition) {
    const auto node = (net.place_count() + transition) * 2;
    std::string label = "transition:" + std::to_string(transition);
    if (net.transitions()[transition].activity &&
        *net.transitions()[transition].activity < net.activities().size()) {
      label += ":" + net.activities()[*net.transitions()[transition].activity];
    } else {
      label += ":tau";
    }
    labels[node] = label + ":in";
    labels[node + 1] = label + ":out";
  }
  return labels;
}

struct ShortCircuitNetResult {
  PetriNet net;
  bool valid = false;
  bool strongly_connected = false;
  std::optional<std::size_t> transition;
  std::vector<std::string> diagnostics;
};

std::vector<std::vector<std::size_t>> structural_graph(const PetriNet& net) {
  const auto element_count = net.place_count() + net.transitions().size();
  std::vector<std::vector<std::size_t>> graph(element_count);
  for (std::size_t transition = 0;
       transition < net.transitions().size(); ++transition) {
    const auto node = net.place_count() + transition;
    const auto add_input = [&](const auto& places) {
      for (const auto place : places) {
        if (place < net.place_count()) graph[place].push_back(node);
      }
    };
    add_input(net.transitions()[transition].inputs);
    add_input(net.transitions()[transition].reset_inputs);
    add_input(net.transitions()[transition].inhibitor_inputs);
    for (const auto place : net.transitions()[transition].outputs) {
      if (place < net.place_count()) graph[node].push_back(place);
    }
  }
  return graph;
}

bool strongly_connected(const PetriNet& net) {
  const auto graph = structural_graph(net);
  if (graph.empty()) return false;
  const auto visit = [&](const auto& adjacency) {
    std::vector<bool> seen(adjacency.size(), false);
    std::queue<std::size_t> pending;
    seen[0] = true;
    pending.push(0);
    while (!pending.empty()) {
      const auto node = pending.front();
      pending.pop();
      for (const auto next : adjacency[node]) {
        if (next >= seen.size() || seen[next]) continue;
        seen[next] = true;
        pending.push(next);
      }
    }
    return std::all_of(seen.begin(), seen.end(), [](bool value) {
      return value;
    });
  };
  if (!visit(graph)) return false;
  std::vector<std::vector<std::size_t>> reverse(graph.size());
  for (std::size_t from = 0; from < graph.size(); ++from)
    for (const auto to : graph[from]) reverse[to].push_back(from);
  return visit(reverse);
}

ShortCircuitNetResult short_circuit_workflow_net(
    const PetriNet& input, const WorkflowNetResult& workflow,
    const std::string& validation_error) {
  ShortCircuitNetResult result;
  if (!validation_error.empty()) {
    result.diagnostics.push_back(validation_error);
    return result;
  }
  if (!workflow.valid) {
    result.diagnostics.push_back(
        workflow.reason.empty() ? "workflow net validation failed"
                                : workflow.reason);
    if (workflow.source_candidates.empty())
      result.diagnostics.push_back("no structural source place was found");
    if (workflow.sink_candidates.empty())
      result.diagnostics.push_back("no structural sink place was found");
    return result;
  }
  result.net = input;
  const auto source = workflow.source_candidates.front();
  const auto sink = workflow.sink_candidates.front();
  result.transition = result.net.add_transition(std::nullopt, {sink}, {source});
  result.valid = true;
  result.strongly_connected = strongly_connected(result.net);
  if (result.strongly_connected) {
    result.diagnostics.push_back(
        "short-circuit transition connects the unique sink to the unique source");
    result.diagnostics.push_back(
        "short-circuited structural graph is strongly connected");
  } else {
    result.diagnostics.push_back(
        "short-circuited structural graph is not strongly connected");
  }
  return result;
}

void validate_marking(const Marking& marking, std::size_t places,
                      const char* name) {
  if (marking.size() != places)
    throw std::invalid_argument(std::string(name) + " has the wrong dimension");
  if (std::any_of(marking.begin(), marking.end(), [](auto value) { return value < 0; }))
    throw std::invalid_argument(std::string(name) + " contains a negative token count");
    if (std::any_of(marking.begin(), marking.end(), [](auto value) {
        return value > kMaxExactSolverInteger;
      }))
    throw std::invalid_argument(std::string(name) +
                                " exceeds the exact numeric solver range");
}

void validate_single_place_marking(const Marking& marking, const char* name) {
  const auto marked_places = static_cast<std::size_t>(std::count_if(
      marking.begin(), marking.end(), [](const auto value) { return value > 0; }));
  if (marked_places != 1)
    throw std::invalid_argument(std::string(name) +
                                " must contain exactly one marked place");
}

WorkflowMarkings resolved_markings(const PetriNet& net, const Marking& initial,
                                   const Marking& final) {
  auto defaults = default_workflow_markings(net);
  if (!initial.empty()) defaults.initial = initial;
  if (!final.empty()) defaults.final = final;
  validate_marking(defaults.initial, net.place_count(), "initial marking");
  validate_marking(defaults.final, net.place_count(), "final marking");
  return defaults;
}

WorkflowMarkings resolved_woflan_markings(const PetriNet& net,
                                           const WorkflowNetResult& workflow,
                                           const Marking& initial,
                                           const Marking& final) {
  auto result = resolved_markings(net, initial, final);
  if (workflow.valid && initial.empty()) {
    result.initial.assign(net.place_count(), 0);
    result.initial[workflow.source_candidates.front()] = 1;
  }
  if (workflow.valid && final.empty()) {
    result.final.assign(net.place_count(), 0);
    result.final[workflow.sink_candidates.front()] = 1;
  }
  validate_marking(result.initial, net.place_count(), "initial marking");
  validate_marking(result.final, net.place_count(), "final marking");
  return result;
}

std::vector<std::int64_t> arc_weights(const std::vector<std::size_t>& arcs,
                                      std::size_t places) {
  std::vector<std::int64_t> weights(places, 0);
  for (const auto place : arcs) ++weights[place];
  return weights;
}

bool enabled(const Marking& marking, const PetriTransition& transition) {
  return petri_transition_enabled(transition, marking);
}

bool fire(const Marking& marking, const PetriTransition& transition,
          Marking& next) {
  if (!enabled(marking, transition)) return false;
  next = marking;
  for (const auto p : transition.inputs) --next[p];
  for (const auto p : transition.reset_inputs) next[p] = 0;
  for (const auto p : transition.outputs) {
    if (next[p] == std::numeric_limits<std::int64_t>::max()) return false;
    ++next[p];
  }
  return true;
}

std::vector<std::vector<double>> incidence(const PetriNet& net) {
  std::vector<std::vector<double>> matrix(
      net.place_count(), std::vector<double>(net.transitions().size(), 0.0));
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    for (const auto p : net.transitions()[t].inputs) matrix[p][t] -= 1.0;
    for (const auto p : net.transitions()[t].outputs) matrix[p][t] += 1.0;
  }
  return matrix;
}

// Two-phase dense simplex for max c*x subject to A*x<=b, x>=0.
class Simplex {
 public:
  Simplex(const std::vector<std::vector<double>>& a,
          const std::vector<double>& b, const std::vector<double>& c)
      : m_(b.size()), n_(c.size()), basis_(m_), nonbasis_(n_ + 1),
        d_(m_ + 2, std::vector<double>(n_ + 2)) {
    for (std::size_t i = 0; i < m_; ++i)
      for (std::size_t j = 0; j < n_; ++j) d_[i][j] = a[i][j];
    for (std::size_t i = 0; i < m_; ++i) {
      basis_[i] = static_cast<int>(n_ + i);
      d_[i][n_] = -1;
      d_[i][n_ + 1] = b[i];
    }
    for (std::size_t j = 0; j < n_; ++j) {
      nonbasis_[j] = static_cast<int>(j);
      d_[m_][j] = -c[j];
    }
    nonbasis_[n_] = -1;
    d_[m_ + 1][n_] = 1;
  }

  enum class Status { OPTIMAL, INFEASIBLE, UNBOUNDED };
  Status solve(std::vector<double>& x, double& value) {
    std::size_t row = 0;
    for (std::size_t i = 1; i < m_; ++i)
      if (d_[i][n_ + 1] < d_[row][n_ + 1]) row = i;
    if (m_ && d_[row][n_ + 1] < -kEps) {
      pivot(row, n_);
      if (!run(1) || d_[m_ + 1][n_ + 1] < -kEps) return Status::INFEASIBLE;
      if (std::abs(d_[m_ + 1][n_ + 1]) > kEps) return Status::INFEASIBLE;
      for (std::size_t i = 0; i < m_; ++i) {
        if (basis_[i] != -1) continue;
        std::size_t col = 0;
        for (std::size_t j = 1; j <= n_; ++j)
          if (d_[i][j] < d_[i][col] - kEps ||
              (std::abs(d_[i][j] - d_[i][col]) <= kEps && nonbasis_[j] < nonbasis_[col])) col = j;
        pivot(i, col);
      }
    }
    if (!run(0)) return Status::UNBOUNDED;
    x.assign(n_, 0.0);
    for (std::size_t i = 0; i < m_; ++i)
      if (basis_[i] >= 0 && static_cast<std::size_t>(basis_[i]) < n_)
        x[static_cast<std::size_t>(basis_[i])] = d_[i][n_ + 1];
    value = d_[m_][n_ + 1];
    return Status::OPTIMAL;
  }

 private:
  void pivot(std::size_t r, std::size_t s) {
    const double inverse = 1.0 / d_[r][s];
    for (std::size_t i = 0; i < m_ + 2; ++i) {
      if (i == r) continue;
      for (std::size_t j = 0; j < n_ + 2; ++j) {
        if (j == s) continue;
        d_[i][j] -= d_[r][j] * d_[i][s] * inverse;
      }
    }
    for (std::size_t j = 0; j < n_ + 2; ++j)
      if (j != s) d_[r][j] *= inverse;
    for (std::size_t i = 0; i < m_ + 2; ++i)
      if (i != r) d_[i][s] *= -inverse;
    d_[r][s] = inverse;
    std::swap(basis_[r], nonbasis_[s]);
  }

  bool run(int phase) {
    const std::size_t objective = phase == 1 ? m_ + 1 : m_;
    while (true) {
      std::optional<std::size_t> col;
      for (std::size_t j = 0; j <= n_; ++j) {
        if (phase == 0 && nonbasis_[j] == -1) continue;
        if (!col ||
            d_[objective][j] < d_[objective][*col] - kEps ||
            (std::abs(d_[objective][j] - d_[objective][*col]) <= kEps &&
             nonbasis_[j] < nonbasis_[*col])) {
          col = j;
        }
      }
      if (!col || d_[objective][*col] >= -kEps) return true;
      std::optional<std::size_t> row;
      for (std::size_t i = 0; i < m_; ++i) {
        if (d_[i][*col] <= kEps) continue;
        if (!row || d_[i][n_ + 1] / d_[i][*col] < d_[*row][n_ + 1] / d_[*row][*col] - kEps ||
            (std::abs(d_[i][n_ + 1] / d_[i][*col] - d_[*row][n_ + 1] / d_[*row][*col]) <= kEps && basis_[i] < basis_[*row])) row = i;
      }
      if (!row) return false;
      pivot(*row, *col);
    }
  }

  std::size_t m_, n_;
  std::vector<int> basis_, nonbasis_;
  std::vector<std::vector<double>> d_;
};

struct IntegerProgramResult {
  bool feasible = false;
  bool exhausted = false;
  std::size_t nodes_explored = 0;
  double objective = 0.0;
  double root_lower_bound = 0.0;
  std::vector<double> solution;
};

IntegerProgramResult solve_integer_program(
    const std::vector<std::vector<double>>& base_a,
    const std::vector<double>& base_b, const std::vector<double>& costs,
    std::size_t node_limit) {
  struct Bound { std::size_t variable; bool upper; double value; };
  struct Node { std::vector<Bound> bounds; };
  IntegerProgramResult result;
  double incumbent = std::numeric_limits<double>::infinity();
  std::vector<Node> stack(1);
  std::size_t explored = 0;
  while (!stack.empty()) {
    if (explored >= node_limit) { result.exhausted = true; break; }
    ++explored;
    auto node = std::move(stack.back()); stack.pop_back();
    auto a = base_a; auto b = base_b;
    for (const auto& bound : node.bounds) {
      std::vector<double> row(costs.size(), 0.0);
      row[bound.variable] = bound.upper ? 1.0 : -1.0;
      a.push_back(std::move(row)); b.push_back(bound.upper ? bound.value : -bound.value);
    }
    std::vector<double> maximize(costs.size());
    for (std::size_t i = 0; i < costs.size(); ++i) maximize[i] = -costs[i];
    Simplex lp(a, b, maximize);
    std::vector<double> x; double maximum = 0;
    if (lp.solve(x, maximum) != Simplex::Status::OPTIMAL) continue;
    const double lower = -maximum;
    if (explored == 1) result.root_lower_bound = lower;
    if (lower >= incumbent - kEps) continue;
    std::optional<std::size_t> fractional;
    for (std::size_t i = 0; i < x.size(); ++i) {
      if (std::abs(x[i] - std::round(x[i])) > 1e-7) { fractional = i; break; }
    }
    if (!fractional) {
      incumbent = lower; result.solution = std::move(x); result.feasible = true;
      continue;
    }
    const auto variable = *fractional;
    const double floor_value = std::floor(x[variable]);
    const double ceil_value = std::ceil(x[variable]);
    // Push the upper branch second so deterministic LIFO explores it first.
    auto lower_branch = node; lower_branch.bounds.push_back({variable, false, ceil_value});
    node.bounds.push_back({variable, true, floor_value});
    stack.push_back(std::move(lower_branch));
    stack.push_back(std::move(node));
  }
  result.nodes_explored = explored;
  if (result.feasible) result.objective = incumbent;
  return result;
}

struct LinearProgramResult {
  bool feasible = false;
  double objective = 0.0;
  std::vector<double> solution;
};

LinearProgramResult solve_linear_program(
    const std::vector<std::vector<double>>& constraints,
    const std::vector<double>& bounds, const std::vector<double>& costs) {
  std::vector<double> maximize(costs.size());
  for (std::size_t i = 0; i < costs.size(); ++i) maximize[i] = -costs[i];
  Simplex program(constraints, bounds, maximize);
  LinearProgramResult result;
  double maximum = 0.0;
  result.feasible =
      program.solve(result.solution, maximum) == Simplex::Status::OPTIMAL;
  if (result.feasible) result.objective = -maximum;
  return result;
}

std::vector<double> default_marking_equation_costs(const PetriNet& net) {
  return std::vector<double>(net.transitions().size(), 0.0);
}

std::vector<double> default_extended_model_costs(const PetriNet& net) {
  std::vector<double> costs;
  costs.reserve(net.transitions().size());
  for (const auto& transition : net.transitions())
    costs.push_back(transition.silent() ? 1.0 : 10000.0);
  return costs;
}

void validate_costs(const std::vector<double>& costs, std::size_t expected,
                    const char* name) {
  if (costs.size() != expected)
    throw std::invalid_argument(std::string(name) + " has the wrong dimension");
  for (const auto cost : costs)
    if (!std::isfinite(cost) || cost < 0)
      throw std::invalid_argument(std::string(name) + " contains an invalid cost");
}

struct RecoveryState {
  Marking marking;
  std::vector<std::int64_t> remaining;
  bool operator<(const RecoveryState& other) const {
    return std::tie(marking, remaining) < std::tie(other.marking, other.remaining);
  }
};

std::vector<std::size_t> recover_sequence(
    const PetriNet& net, const Marking& initial, const Marking& final,
    const std::vector<double>& counts, std::size_t limit, bool& exhausted) {
  std::vector<std::int64_t> integer_counts(counts.size());
  std::uint64_t total = 0;
  for (std::size_t i = 0; i < counts.size(); ++i) {
    const double rounded = std::round(counts[i]);
    if (std::abs(counts[i] - rounded) > 1e-7 || rounded < 0 || rounded > 1000000) return {};
    integer_counts[i] = static_cast<std::int64_t>(rounded);
    total += static_cast<std::uint64_t>(integer_counts[i]);
  }
  if (total > limit) { exhausted = true; return {}; }
  struct Node { RecoveryState state; std::vector<std::size_t> path; };
  std::queue<Node> queue;
  std::set<RecoveryState> seen;
  queue.push({{initial, integer_counts}, {}});
  seen.insert(queue.front().state);
  while (!queue.empty()) {
    if (seen.size() > limit) { exhausted = true; return {}; }
    auto node = std::move(queue.front()); queue.pop();
    const bool done = std::all_of(node.state.remaining.begin(), node.state.remaining.end(), [](auto x) { return x == 0; });
    if (done && node.state.marking == final) return node.path;
    for (std::size_t t = 0; t < net.transitions().size(); ++t) {
      if (node.state.remaining[t] == 0) continue;
      Marking next_marking;
      if (!fire(node.state.marking, net.transitions()[t], next_marking)) continue;
      auto next = node.state;
      next.marking = std::move(next_marking);
      --next.remaining[t];
      if (!seen.insert(next).second) continue;
      auto path = node.path; path.push_back(t);
      queue.push({std::move(next), std::move(path)});
    }
  }
  return {};
}

// Compute a deterministic family of non-negative place invariants.  A plain
// Gaussian null-space basis is useful algebraically, but its vectors commonly
// contain both signs and therefore cannot be used as a coverage diagnostic.
// For every place we instead solve a small LP over
//
//     C^T y = 0, y >= 0, sum(y) <= 1,
//
// maximising that place's coefficient.  A positive optimum is a certificate
// that the place belongs to a positive P-invariant.  The existing simplex is
// deliberately reused here so the analysis remains self-contained.
std::vector<std::vector<double>> invariant_basis(const PetriNet& net) {
  const auto c = incidence(net);
  const std::size_t places = net.place_count();
  std::vector<std::vector<double>> constraints;
  std::vector<double> bounds;
  constraints.reserve(2 * net.transitions().size() + 1);
  bounds.reserve(constraints.capacity());
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    std::vector<double> row(places, 0.0);
    for (std::size_t p = 0; p < places; ++p) row[p] = c[p][t];
    constraints.push_back(row);
    bounds.push_back(0.0);
    for (auto& value : row) value = -value;
    constraints.push_back(std::move(row));
    bounds.push_back(0.0);
  }
  constraints.push_back(std::vector<double>(places, 1.0));
  bounds.push_back(1.0);

  std::vector<std::vector<double>> basis;
  for (std::size_t target = 0; target < places; ++target) {
    std::vector<double> objective(places, 0.0);
    objective[target] = 1.0;
    Simplex simplex(constraints, bounds, objective);
    std::vector<double> solution;
    double optimum = 0.0;
    if (simplex.solve(solution, optimum) != Simplex::Status::OPTIMAL ||
        optimum <= kEps)
      continue;
    for (auto& value : solution) {
      if (std::abs(value) <= 1e-7) value = 0.0;
      if (value < 0.0 && value > -1e-7) value = 0.0;
    }
    const auto maximum = *std::max_element(solution.begin(), solution.end());
    if (maximum <= kEps) continue;
    for (auto& value : solution) value /= maximum;
    bool duplicate = false;
    for (const auto& existing : basis) {
      duplicate = existing.size() == solution.size();
      if (!duplicate) continue;
      for (std::size_t p = 0; p < solution.size(); ++p) {
        if (std::abs(existing[p] - solution[p]) > 1e-6) {
          duplicate = false;
          break;
        }
      }
      if (duplicate) break;
    }
    if (!duplicate) basis.push_back(std::move(solution));
  }
  return basis;
}

std::vector<std::vector<std::size_t>> invariant_supports(
    const std::vector<std::vector<double>>& basis, const PetriNet& net) {
  std::vector<std::vector<std::size_t>> supports;
  for (const auto& invariant : basis) {
    std::vector<std::size_t> support;
    for (std::size_t p = 0; p < invariant.size(); ++p)
      if (invariant[p] > kEps) support.push_back(p);
    if (support.empty()) continue;

    std::vector<bool> in_support(net.place_count(), false);
    for (const auto place : support) in_support[place] = true;
    bool state_machine = true;
    for (const auto& transition : net.transitions()) {
      std::set<std::size_t> inputs;
      std::set<std::size_t> outputs;
      for (const auto place : transition.inputs)
        if (place < in_support.size() && in_support[place]) inputs.insert(place);
      for (const auto place : transition.outputs)
        if (place < in_support.size() && in_support[place]) outputs.insert(place);
      if (inputs.empty() && outputs.empty()) continue;
      // An S-component is a state-machine subnet: every participating
      // transition has exactly one supported input and output place.
      if (transition.has_extended_arcs() || inputs.size() != 1 ||
          outputs.size() != 1) {
        state_machine = false;
        break;
      }
    }
    if (!state_machine) continue;
    // Keep the invariant support intact.  The reference diagnostic treats a
    // disconnected positive invariant as one S-component; connectivity is
    // not an additional criterion after the state-machine test.
    if (std::find(supports.begin(), supports.end(), support) == supports.end())
      supports.push_back(std::move(support));
  }
  std::sort(supports.begin(), supports.end());
  return supports;
}

bool same_vector(const std::vector<double>& lhs,
                 const std::vector<double>& rhs) {
  if (lhs.size() != rhs.size()) return false;
  return std::equal(lhs.begin(), lhs.end(), rhs.begin(),
                    [](double a, double b) { return std::abs(a - b) <= 1e-6; });
}

bool satisfies_place_invariant(
    const std::vector<double>& invariant,
    const std::vector<std::vector<double>>& matrix) {
  if (invariant.size() != matrix.size()) return false;
  if (matrix.empty()) return invariant.empty();
  const auto transition_count = matrix.front().size();
  for (const auto& row : matrix)
    if (row.size() != transition_count) return false;
  for (std::size_t t = 0; t < transition_count; ++t) {
    double balance = 0.0;
    for (std::size_t p = 0; p < invariant.size(); ++p)
      balance += invariant[p] * matrix[p][t];
    if (std::abs(balance) > 1e-6) return false;
  }
  return true;
}

void append_uniform_candidate(std::vector<std::vector<double>>& result,
                              std::vector<double> candidate,
                              const std::vector<std::vector<double>>& matrix) {
  if (candidate.empty() ||
      std::none_of(candidate.begin(), candidate.end(),
                   [](double value) { return value > kEps; }))
    return;
  for (auto& value : candidate) {
    if (std::abs(value) <= 1e-7) value = 0.0;
    if (std::abs(value - 1.0) <= 1e-7) value = 1.0;
  }
  if (std::any_of(candidate.begin(), candidate.end(),
                  [](double value) { return value < -kEps || value > 1.0 + kEps; }))
    return;
  if (!satisfies_place_invariant(candidate, matrix)) return;
  if (std::none_of(result.begin(), result.end(),
                   [&](const auto& existing) {
                     return same_vector(existing, candidate);
                   }))
    result.push_back(std::move(candidate));
}

struct UniformInvariantSearch {
  std::optional<std::vector<double>> candidate;
  bool exhausted = false;
  std::size_t nodes_explored = 0;
};

UniformInvariantSearch find_uniform_invariant(
    const std::vector<std::vector<double>>& matrix, std::size_t target,
    std::size_t node_limit) {
  const std::size_t places = matrix.size();
  if (target >= places) return {};

  // Solve C^T z = 0 with 0 <= z <= 1, z[target] = 1.  The integer solver
  // searches the finite binary polytope directly, so a valid support can be
  // found even when no individual weighted certificate has unit coefficients.
  std::vector<std::vector<double>> constraints;
  std::vector<double> bounds;
  const std::size_t transitions = places == 0 ? 0 : matrix.front().size();
  constraints.reserve(2 * transitions + places + 1);
  bounds.reserve(constraints.capacity());
  for (std::size_t t = 0; t < transitions; ++t) {
    std::vector<double> row(places, 0.0);
    for (std::size_t p = 0; p < places; ++p) row[p] = matrix[p][t];
    constraints.push_back(row);
    bounds.push_back(0.0);
    for (auto& value : row) value = -value;
    constraints.push_back(std::move(row));
    bounds.push_back(0.0);
  }
  for (std::size_t p = 0; p < places; ++p) {
    std::vector<double> row(places, 0.0);
    row[p] = 1.0;
    constraints.push_back(std::move(row));
    bounds.push_back(1.0);
  }
  std::vector<double> required(places, 0.0);
  required[target] = -1.0;
  constraints.push_back(std::move(required));
  bounds.push_back(-1.0);

  if (node_limit == 0) return {.candidate = std::nullopt, .exhausted = true};
  const auto solution = solve_integer_program(
      constraints, bounds, std::vector<double>(places, 1.0), node_limit);
  UniformInvariantSearch result;
  result.exhausted = solution.exhausted;
  result.nodes_explored = solution.nodes_explored;
  if (!solution.feasible || solution.solution.size() != places) return result;
  std::vector<double> candidate(places, 0.0);
  for (std::size_t p = 0; p < places; ++p) {
    const auto rounded = std::round(solution.solution[p]);
    if (std::abs(solution.solution[p] - rounded) > 1e-7 ||
        rounded < 0.0 || rounded > 1.0)
      return result;
    candidate[p] = rounded;
  }
  if (candidate[target] < 1.0 - kEps ||
      !satisfies_place_invariant(candidate, matrix))
    return result;
  result.candidate = std::move(candidate);
  return result;
}

std::vector<std::vector<double>> uniform_invariants(
    const std::vector<std::vector<double>>& basis,
    const std::vector<std::vector<double>>& matrix,
    std::size_t search_budget, bool& search_exhausted) {
  std::vector<std::vector<double>> result;
  search_exhausted = false;
  // Preserve valid supports already exposed by the positive basis, then use
  // one bounded binary search per uncovered place to recover combinations of
  // weighted basis vectors.  The latter is the important distinction from a
  // simple support-binarization pass.
  for (const auto& invariant : basis) {
    std::vector<double> uniform(invariant.size(), 0.0);
    for (std::size_t p = 0; p < invariant.size(); ++p)
      if (invariant[p] > kEps) uniform[p] = 1.0;
    append_uniform_candidate(result, std::move(uniform), matrix);
  }
  std::size_t remaining_budget = search_budget;
  for (std::size_t target = 0; target < matrix.size(); ++target) {
    bool covered = false;
    for (const auto& invariant : result)
      if (target < invariant.size() && invariant[target] > kEps) {
        covered = true;
        break;
      }
    if (covered) continue;
    if (remaining_budget == 0) {
      search_exhausted = true;
      break;
    }
    const auto search = find_uniform_invariant(matrix, target, remaining_budget);
    if (search.nodes_explored >= remaining_budget)
      remaining_budget = 0;
    else
      remaining_budget -= search.nodes_explored;
    if (search.candidate)
      append_uniform_candidate(result, *search.candidate, matrix);
    if (search.exhausted) {
      search_exhausted = true;
      break;
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}

std::vector<std::vector<double>> weighted_invariants(
    const std::vector<std::vector<double>>& basis,
    const std::vector<std::vector<double>>& matrix) {
  std::vector<std::vector<double>> result;
  for (const auto& invariant : basis) {
    double minimum = std::numeric_limits<double>::infinity();
    for (const auto value : invariant)
      if (value > kEps) minimum = std::min(minimum, value);
    if (!std::isfinite(minimum)) continue;
    std::vector<double> weighted(invariant.size(), 0.0);
    for (std::size_t p = 0; p < invariant.size(); ++p) {
      if (invariant[p] <= kEps) continue;
      weighted[p] = invariant[p] / minimum;
      const auto nearest = std::round(weighted[p]);
      if (std::abs(weighted[p] - nearest) <= 1e-6) weighted[p] = nearest;
    }
    if (!satisfies_place_invariant(weighted, matrix)) {
      // Rounding a near-integer certificate can introduce a real balance
      // error on weighted arcs.  Retain the normalized floating certificate
      // when it still satisfies the equation; otherwise omit the vector.
      weighted.assign(invariant.size(), 0.0);
      for (std::size_t p = 0; p < invariant.size(); ++p)
        if (invariant[p] > kEps) weighted[p] = invariant[p] / minimum;
    }
    if (!satisfies_place_invariant(weighted, matrix)) continue;
    if (std::none_of(result.begin(), result.end(),
                     [&](const auto& existing) {
                       return same_vector(existing, weighted);
                     }))
      result.push_back(std::move(weighted));
  }
  return result;
}

std::vector<std::size_t> uncovered_from_vectors(
    const std::vector<std::vector<double>>& vectors, std::size_t places) {
  std::vector<std::size_t> uncovered;
  for (std::size_t p = 0; p < places; ++p) {
    bool covered = false;
    for (const auto& vector : vectors)
      if (p < vector.size() && vector[p] > kEps) {
        covered = true;
        break;
      }
    if (!covered) uncovered.push_back(p);
  }
  return uncovered;
}

std::vector<std::size_t> uncovered_from_supports(
    const std::vector<std::vector<std::size_t>>& supports,
    std::size_t places) {
  std::vector<bool> covered(places, false);
  for (const auto& support : supports)
    for (const auto place : support)
      if (place < places) covered[place] = true;
  std::vector<std::size_t> uncovered;
  for (std::size_t p = 0; p < places; ++p)
    if (!covered[p]) uncovered.push_back(p);
  return uncovered;
}

// A compact Karp--Miller style coverability search.  -1 denotes omega.  The
// search is used only as an unboundedness certificate; the exact reachability
// graph below still supplies the soundness conditions whenever no certificate
// is found.  Accelerating a marking against an ancestor is a finite proof of
// unbounded growth and gives a reproducible transition witness.
struct CoverabilityResult {
  bool unbounded = false;
  bool exhausted = false;
  std::size_t node_count = 0;
  std::vector<std::size_t> witness;
  std::vector<std::size_t> omega_places;
  std::vector<Marking> markings;
  std::vector<std::vector<std::size_t>> edges;
};

constexpr std::int64_t kOmega = -1;

struct ReachabilityProjection {
  bool exhausted = false;
  std::vector<Marking> markings;
  std::vector<std::vector<std::size_t>> edges;
};

ReachabilityProjection reachability_projection(const PetriNet& net,
                                               const Marking& initial,
                                               std::size_t max_states) {
  struct Node {
    Marking marking;
  };
  ReachabilityProjection result;
  std::vector<Node> nodes{{initial}};
  std::map<Marking, std::size_t> ids{{initial, 0}};
  std::vector<std::vector<std::pair<std::size_t, std::size_t>>> edges(1);
  std::queue<std::size_t> pending;
  pending.push(0);
  const auto snapshot = [&]() {
    result.markings.clear();
    result.markings.reserve(nodes.size());
    for (const auto& node : nodes) result.markings.push_back(node.marking);
    result.edges.clear();
    for (std::size_t from = 0; from < edges.size(); ++from)
      for (const auto [to, transition] : edges[from])
        result.edges.push_back({from, to, transition});
  };
  while (!pending.empty()) {
    const auto from = pending.front();
    pending.pop();
    for (std::size_t transition = 0;
         transition < net.transitions().size(); ++transition) {
      Marking next;
      if (!fire(nodes[from].marking, net.transitions()[transition], next))
        continue;
      auto [it, inserted] = ids.emplace(next, nodes.size());
      if (inserted) {
        if (nodes.size() >= max_states) {
          result.exhausted = true;
          snapshot();
          return result;
        }
        nodes.push_back({std::move(next)});
        edges.emplace_back();
        pending.push(it->second);
      }
      edges[from].push_back({it->second, transition});
    }
  }
  snapshot();
  return result;
}

bool graph_projection_strongly_connected(
    std::size_t node_count,
    const std::vector<std::vector<std::size_t>>& edges) {
  if (node_count == 0) return false;
  std::vector<std::vector<std::size_t>> graph(node_count);
  std::vector<std::vector<std::size_t>> reverse(node_count);
  for (const auto& edge : edges) {
    if (edge.size() != 3 || edge[0] >= node_count || edge[1] >= node_count)
      continue;
    graph[edge[0]].push_back(edge[1]);
    reverse[edge[1]].push_back(edge[0]);
  }
  const auto reaches_all = [node_count](
                               const std::vector<std::vector<std::size_t>>& adjacency) {
    std::vector<bool> seen(node_count, false);
    std::queue<std::size_t> pending;
    seen[0] = true;
    pending.push(0);
    while (!pending.empty()) {
      const auto node = pending.front();
      pending.pop();
      for (const auto next : adjacency[node]) {
        if (seen[next]) continue;
        seen[next] = true;
        pending.push(next);
      }
    }
    return std::all_of(seen.begin(), seen.end(), [](bool value) {
      return value;
    });
  };
  return reaches_all(graph) && reaches_all(reverse);
}

bool cover_leq(const std::vector<std::int64_t>& lhs,
               const std::vector<std::int64_t>& rhs) {
  for (std::size_t p = 0; p < lhs.size(); ++p) {
    if (rhs[p] != kOmega &&
        (lhs[p] == kOmega || lhs[p] > rhs[p])) return false;
  }
  return true;
}

CoverabilityResult coverability_search(const PetriNet& net,
                                       const Marking& initial,
                                       std::size_t max_nodes) {
  struct Node {
    std::vector<std::int64_t> marking;
    std::size_t parent;
    std::size_t transition;
  };
  CoverabilityResult result;
  if (net.has_extended_arcs()) {
    result.exhausted = true;
    return result;
  }
  std::vector<Node> nodes;
  nodes.push_back({initial, std::numeric_limits<std::size_t>::max(),
                   std::numeric_limits<std::size_t>::max()});
  std::vector<std::vector<std::vector<std::size_t>>> edges;
  edges.emplace_back();
  const auto snapshot = [&]() {
    result.markings.clear();
    result.markings.reserve(nodes.size());
    for (const auto& node : nodes) result.markings.push_back(node.marking);
    result.edges.clear();
    for (std::size_t from = 0; from < edges.size(); ++from)
      for (const auto& edge : edges[from]) result.edges.push_back(edge);
  };
  std::queue<std::size_t> queue;
  queue.push(0);
  const auto path_to = [&](std::size_t id) {
    std::vector<std::size_t> path;
    while (nodes[id].parent != std::numeric_limits<std::size_t>::max()) {
      path.push_back(nodes[id].transition);
      id = nodes[id].parent;
    }
    std::reverse(path.begin(), path.end());
    return path;
  };
  while (!queue.empty()) {
    const auto id = queue.front();
    queue.pop();
    for (std::size_t t = 0; t < net.transitions().size(); ++t) {
      const auto& transition = net.transitions()[t];
      bool is_enabled = true;
      std::vector<std::int64_t> next = nodes[id].marking;
      const auto required = arc_weights(transition.inputs, next.size());
      for (std::size_t p = 0; p < next.size(); ++p) {
        if (next[p] != kOmega && next[p] < required[p]) {
          is_enabled = false;
          break;
        }
      }
      if (!is_enabled) continue;
      for (const auto p : transition.inputs)
        if (next[p] != kOmega) --next[p];
      for (const auto p : transition.outputs)
        if (next[p] != kOmega) ++next[p];

      bool accelerated = false;
      std::vector<bool> accelerated_places(next.size(), false);
      for (std::size_t ancestor = id;
           ancestor != std::numeric_limits<std::size_t>::max();
           ancestor = nodes[ancestor].parent) {
        if (!cover_leq(nodes[ancestor].marking, next)) continue;
        bool strict = false;
        for (std::size_t p = 0; p < next.size(); ++p) {
          if (nodes[ancestor].marking[p] == kOmega) continue;
          if (next[p] == kOmega || next[p] > nodes[ancestor].marking[p]) {
            next[p] = kOmega;
            accelerated_places[p] = true;
            strict = true;
          }
        }
        accelerated = accelerated || strict;
      }
      if (accelerated) {
        const auto accelerated_id = nodes.size();
        nodes.push_back({next, id, t});
        edges.emplace_back();
        edges[id].push_back({id, accelerated_id, t});
        result.unbounded = true;
        result.witness = path_to(id);
        result.witness.push_back(t);
        for (std::size_t p = 0; p < accelerated_places.size(); ++p)
          if (accelerated_places[p]) result.omega_places.push_back(p);
        std::sort(result.omega_places.begin(), result.omega_places.end());
        result.omega_places.erase(
            std::unique(result.omega_places.begin(), result.omega_places.end()),
            result.omega_places.end());
        result.node_count = nodes.size();
        snapshot();
        return result;
      }

      std::optional<std::size_t> covered_by;
      // A newly reached marking can be subsumed only when it is no larger
      // than an already explored coverability node.  Reversing this test
      // would discard the larger state that may contain the unbounded branch.
      for (std::size_t existing = 0; existing < nodes.size(); ++existing)
        if (cover_leq(next, nodes[existing].marking)) {
          covered_by = existing;
          break;
        }
      if (covered_by) {
        edges[id].push_back({id, *covered_by, t});
        continue;
      }
      if (nodes.size() >= max_nodes) {
        result.exhausted = true;
        result.node_count = nodes.size();
        snapshot();
        return result;
      }
      nodes.push_back({std::move(next), id, t});
      edges.emplace_back();
      edges[id].push_back({id, nodes.size() - 1, t});
      queue.push(nodes.size() - 1);
    }
  }
  result.node_count = nodes.size();
  snapshot();
  return result;
}

// Woflan also exposes a restricted coverability tree.  It deliberately keeps
// one outgoing edge per transition at each tree node and suppresses markings
// already seen anywhere in the tree.  This differs from the minimal
// coverability projection above: the tree preserves a deterministic witness
// branch for each first-seen transition and accelerates only against its
// ancestors.
struct RestrictedCoverabilityResult {
  bool exhausted = false;
  std::vector<Marking> markings;
  std::vector<std::vector<std::size_t>> edges;
};

bool restricted_marking_leq(const Marking& lhs, const Marking& rhs) {
  if (lhs.size() != rhs.size()) return false;
  for (std::size_t place = 0; place < lhs.size(); ++place) {
    if (rhs[place] == kOmega) continue;
    if (lhs[place] == kOmega || lhs[place] > rhs[place]) return false;
  }
  return true;
}

bool restricted_fire(const Marking& marking, const PetriTransition& transition,
                     Marking& next) {
  // The graph construction in the reference is defined for ordinary arcs.
  // Extended arcs are reported as an unavailable tree rather than being
  // silently interpreted with ordinary incidence semantics.
  if (!transition.reset_inputs.empty() || !transition.inhibitor_inputs.empty())
    return false;
  std::vector<std::size_t> required(marking.size(), 0);
  for (const auto place : transition.inputs) {
    if (place >= marking.size()) return false;
    if (marking[place] != kOmega) {
      ++required[place];
      if (marking[place] < static_cast<std::int64_t>(required[place]))
        return false;
    }
  }
  next = marking;
  for (const auto place : transition.inputs)
    if (next[place] != kOmega) --next[place];
  for (const auto place : transition.outputs) {
    if (next[place] == kOmega) continue;
    if (next[place] == std::numeric_limits<std::int64_t>::max()) return false;
    ++next[place];
  }
  return true;
}

RestrictedCoverabilityResult restricted_coverability_search(
    const PetriNet& net, const Marking& initial, std::size_t max_nodes) {
  RestrictedCoverabilityResult result;
  if (net.has_extended_arcs()) {
    result.exhausted = true;
    return result;
  }

  struct Node {
    Marking marking;
    std::size_t parent;
    std::size_t transition;
  };
  std::vector<Node> nodes{{initial, std::numeric_limits<std::size_t>::max(),
                           std::numeric_limits<std::size_t>::max()}};
  std::map<Marking, std::size_t> seen{{initial, 0}};
  std::vector<std::vector<std::pair<std::size_t, std::size_t>>> edges(1);

  const auto snapshot = [&]() {
    result.markings.clear();
    result.markings.reserve(nodes.size());
    for (const auto& node : nodes) result.markings.push_back(node.marking);
    result.edges.clear();
    for (std::size_t from = 0; from < edges.size(); ++from)
      for (const auto [to, transition] : edges[from])
        result.edges.push_back({from, to, transition});
  };

  bool new_arc = true;
  while (new_arc) {
    new_arc = false;
    std::size_t selected_node = std::numeric_limits<std::size_t>::max();
    std::size_t selected_transition = std::numeric_limits<std::size_t>::max();
    Marking selected_marking;

    // The reference walks the current node list from the newest node toward
    // the root and takes the first admissible transition in transition order.
    for (std::size_t node = nodes.size(); node-- > 0;) {
      if (std::find(nodes[node].marking.begin(), nodes[node].marking.end(),
                    kOmega) != nodes[node].marking.end())
        continue;
      for (std::size_t transition = 0;
           transition < net.transitions().size(); ++transition) {
        Marking next;
        if (!restricted_fire(nodes[node].marking,
                             net.transitions()[transition], next))
          continue;
        if (seen.find(next) != seen.end()) continue;
        const bool transition_used = std::any_of(
            edges[node].begin(), edges[node].end(),
            [transition](const auto edge) { return edge.second == transition; });
        if (transition_used) continue;
        selected_node = node;
        selected_transition = transition;
        selected_marking = std::move(next);
        new_arc = true;
        break;
      }
      if (new_arc) break;
    }
    if (!new_arc) break;

    // Mark every coordinate that strictly increases over an ancestor as
    // omega.  Ancestor order is nearest first, matching the tree contract.
    for (std::size_t ancestor = nodes[selected_node].parent;
         ancestor != std::numeric_limits<std::size_t>::max();
         ancestor = nodes[ancestor].parent) {
      if (!restricted_marking_leq(nodes[ancestor].marking, selected_marking))
        continue;
      for (std::size_t place = 0; place < selected_marking.size(); ++place) {
        if (nodes[ancestor].marking[place] != kOmega &&
            selected_marking[place] != kOmega &&
            nodes[ancestor].marking[place] < selected_marking[place])
          selected_marking[place] = kOmega;
      }
    }

    if (nodes.size() >= max_nodes) {
      result.exhausted = true;
      break;
    }
    const auto next_node = nodes.size();
    nodes.push_back({std::move(selected_marking), selected_node,
                     selected_transition});
    edges.emplace_back();
    edges[selected_node].push_back({next_node, selected_transition});
    seen[nodes.back().marking] = next_node;
  }

  snapshot();
  return result;
}

}  // namespace

WorkflowMarkings default_workflow_markings(const PetriNet& net) {
  if (const auto error = validate_net(net); !error.empty()) throw std::invalid_argument(error);
  WorkflowMarkings result{Marking(net.place_count(), 0), Marking(net.place_count(), 0)};
  result.initial[net.source()] = 1;
  result.final[net.sink()] = 1;
  return result;
}

MarkingEquationResult solve_marking_equation(const PetriNet& net,
                                             const MarkingEquationOptions& options) {
  if (const auto error = validate_net(net); !error.empty()) throw std::invalid_argument(error);
  if (net.has_extended_arcs()) {
    MarkingEquationResult unsupported;
    unsupported.computed = false;
    unsupported.error = "marking equation does not represent reset or inhibitor arcs";
    return unsupported;
  }
  const auto markings = resolved_markings(net, options.initial_marking, options.final_marking);
  auto costs = options.transition_costs.empty()
                   ? default_marking_equation_costs(net)
                   : options.transition_costs;
  validate_costs(costs, net.transitions().size(), "transition costs");
  if (options.realizability_limit == 0) throw std::invalid_argument("realizability limit must be positive");
  MarkingEquationResult result;
  result.computed = true;
  const auto matrix = incidence(net);
  std::vector<std::vector<double>> inequalities;
  std::vector<double> bounds;
  for (std::size_t p = 0; p < net.place_count(); ++p) {
    const double delta = static_cast<double>(markings.final[p]) -
                         static_cast<double>(markings.initial[p]);
    inequalities.push_back(matrix[p]);
    bounds.push_back(delta);
    auto negative = matrix[p];
    for (auto& x : negative) x = -x;
    inequalities.push_back(std::move(negative));
    bounds.push_back(-delta);
  }
  const auto program = solve_linear_program(inequalities, bounds, costs);
  if (!program.feasible) {
    result.feasible = false;
    result.error = "marking equation is infeasible";
    return result;
  }
  result.feasible = true;
  result.objective = program.objective;
  result.lower_bound = program.objective;
  result.transition_counts = program.solution;
  result.residual.resize(net.place_count());
  for (std::size_t p = 0; p < net.place_count(); ++p) {
    double value = static_cast<double>(markings.initial[p] - markings.final[p]);
    for (std::size_t t = 0; t < net.transitions().size(); ++t) value += matrix[p][t] * result.transition_counts[t];
    result.residual[p] = value;
  }
  for (std::size_t t = 0; t < result.transition_counts.size(); ++t)
    if (result.transition_counts[t] > kEps) result.activated_transitions.push_back(t);
  const bool integral = std::all_of(
      result.transition_counts.begin(), result.transition_counts.end(),
      [](double value) { return std::abs(value - std::round(value)) <= 1e-7; });
  result.recovery_attempted = integral;
  if (integral) {
    bool recovery_exhausted = false;
    result.firing_sequence = recover_sequence(
        net, markings.initial, markings.final, result.transition_counts,
        options.realizability_limit, recovery_exhausted);
    result.exhausted = recovery_exhausted;
    if (result.firing_sequence.empty()) {
      result.reaches_final = markings.initial == markings.final &&
          std::all_of(result.transition_counts.begin(),
                      result.transition_counts.end(),
                      [](double value) { return value <= kEps; });
    } else {
      result.reaches_final = true;
    }
  }
  return result;
}

ExtendedMarkingEquationResult solve_extended_marking_equation(
    const std::vector<std::string>& trace, const PetriNet& net,
    const ExtendedMarkingEquationOptions& options) {
  if (const auto error = validate_net(net); !error.empty())
    throw std::invalid_argument(error);
  if (net.has_extended_arcs()) {
    ExtendedMarkingEquationResult unsupported;
    unsupported.computed = false;
    unsupported.error = "extended marking equation does not represent reset or inhibitor arcs";
    return unsupported;
  }
  const auto markings = resolved_markings(net, options.initial_marking,
                                          options.final_marking);
  if (options.realizability_limit == 0)
    throw std::invalid_argument("realizability limit must be positive");

  ExtendedMarkingEquationResult result;
  result.computed = true;
  const auto transition_count = net.transitions().size();
  auto model_costs = options.model_transition_costs.empty()
                         ? default_extended_model_costs(net)
                         : options.model_transition_costs;
  validate_costs(model_costs, transition_count, "model transition costs");
  auto sync_costs = options.synchronous_transition_costs.empty()
                        ? std::vector<double>(transition_count, 0.0)
                        : options.synchronous_transition_costs;
  validate_costs(sync_costs, transition_count,
                 "synchronous transition costs");
  auto log_costs = options.log_move_costs.empty()
                       ? std::vector<double>(trace.size(), 10000.0)
                       : options.log_move_costs;
  validate_costs(log_costs, trace.size(), "log move costs");

  for (const auto split : options.split_indices) {
    if (split == 0 || split >= trace.size())
      throw std::invalid_argument(
          "split index must lie strictly inside the trace");
  }
  result.retained_splits = options.split_indices;
  std::sort(result.retained_splits.begin(), result.retained_splits.end());
  if (std::adjacent_find(result.retained_splits.begin(),
                         result.retained_splits.end()) !=
      result.retained_splits.end())
    throw std::invalid_argument("split indices must be unique");
  if (result.retained_splits.empty() && trace.size() > 1) {
    if (options.max_k_value == 0) {
      result.retained_splits = {1, trace.size() - 1};
    } else {
      result.retained_splits.reserve(trace.size() - 1);
      for (std::size_t i = 1; i < trace.size(); ++i)
        result.retained_splits.push_back(i);
    }
  }
  if (options.max_k_value > 0 &&
      result.retained_splits.size() > options.max_k_value) {
    result.retained_splits = select_evenly_spaced_splits(
        result.retained_splits, options.max_k_value);
  }
  result.segment_count = result.retained_splits.size() + 1;
  const auto segments = result.segment_count;
  const auto places = net.place_count();
  const auto matrix = incidence(net);
  const auto model_base = std::size_t{0};
  const auto sync_base = segments * transition_count;
  const auto log_base = sync_base + segments * transition_count;
  const auto variable_count = log_base + trace.size();
  std::vector<double> costs(variable_count, 0.0);
  for (std::size_t segment = 0; segment < segments; ++segment) {
    for (std::size_t transition = 0; transition < transition_count; ++transition) {
      costs[model_base + segment * transition_count + transition] =
          model_costs[transition];
      costs[sync_base + segment * transition_count + transition] =
          sync_costs[transition];
    }
  }
  for (std::size_t position = 0; position < trace.size(); ++position)
    costs[log_base + position] = log_costs[position];

  std::vector<std::vector<double>> inequalities;
  std::vector<double> bounds;
  const auto add_equality = [&](std::vector<double> row, double bound) {
    inequalities.push_back(row);
    bounds.push_back(bound);
    for (auto& value : row) value = -value;
    inequalities.push_back(std::move(row));
    bounds.push_back(-bound);
  };
  const auto segment_for = [&](std::size_t position) {
    return static_cast<std::size_t>(std::upper_bound(
        result.retained_splits.begin(), result.retained_splits.end(), position) -
                                    result.retained_splits.begin());
  };
  for (std::size_t position = 0; position < trace.size(); ++position) {
    const auto segment = segment_for(position);
    std::vector<double> row(variable_count, 0.0);
    row[log_base + position] = 1.0;
    for (std::size_t transition = 0; transition < transition_count;
         ++transition) {
      const auto activity = net.transitions()[transition].activity;
      if (activity && *activity < net.activities().size() &&
          net.activities()[*activity] == trace[position]) {
        row[sync_base + segment * transition_count + transition] = 1.0;
      }
    }
    add_equality(std::move(row), 1.0);
  }
  for (std::size_t segment = 0; segment < segments; ++segment) {
    for (std::size_t transition = 0; transition < transition_count;
         ++transition) {
      const auto activity = net.transitions()[transition].activity;
      if (!activity || *activity >= net.activities().size()) continue;
      bool appears = false;
      const auto begin = segment == 0 ? 0 : result.retained_splits[segment - 1];
      const auto end = segment < result.retained_splits.size()
                           ? result.retained_splits[segment]
                           : trace.size();
      for (std::size_t position = begin; position < end; ++position) {
        if (net.activities()[*activity] == trace[position]) {
          appears = true;
          break;
        }
      }
      if (!appears) {
        std::vector<double> row(variable_count, 0.0);
        row[sync_base + segment * transition_count + transition] = 1.0;
        inequalities.push_back(std::move(row));
        bounds.push_back(0.0);
      }
    }
  }

  // Every segment boundary must denote a non-negative marking. The final
  // boundary is also fixed exactly below, so this keeps the state equation
  // split-aware without pretending to prove an interior firing order.
  for (std::size_t segment = 0; segment < segments; ++segment) {
    for (std::size_t place = 0; place < places; ++place) {
      std::vector<double> row(variable_count, 0.0);
      for (std::size_t prior = 0; prior <= segment; ++prior) {
        for (std::size_t transition = 0; transition < transition_count;
             ++transition) {
          const auto coefficient = -matrix[place][transition];
          row[model_base + prior * transition_count + transition] =
              coefficient;
          row[sync_base + prior * transition_count + transition] =
              coefficient;
        }
      }
      inequalities.push_back(std::move(row));
      bounds.push_back(static_cast<double>(markings.initial[place]));
    }
  }
  for (std::size_t place = 0; place < places; ++place) {
    std::vector<double> row(variable_count, 0.0);
    for (std::size_t segment = 0; segment < segments; ++segment) {
      for (std::size_t transition = 0; transition < transition_count;
           ++transition) {
        row[model_base + segment * transition_count + transition] =
            matrix[place][transition];
        row[sync_base + segment * transition_count + transition] =
            matrix[place][transition];
      }
    }
    const auto delta = static_cast<double>(markings.final[place]) -
                       static_cast<double>(markings.initial[place]);
    add_equality(std::move(row), delta);
  }

  const auto program = solve_integer_program(inequalities, bounds, costs,
                                             options.realizability_limit);
  result.exhausted = program.exhausted;
  if (!program.feasible) {
    result.error = program.exhausted
                       ? "extended marking equation node limit reached"
                       : "extended marking equation is infeasible";
    return result;
  }
  result.feasible = true;
  result.objective = program.objective;
  result.lower_bound = program.root_lower_bound;
  result.transition_counts.assign(transition_count, 0.0);
  for (std::size_t segment = 0; segment < segments; ++segment) {
    for (std::size_t transition = 0; transition < transition_count;
         ++transition) {
      result.transition_counts[transition] +=
          program.solution[model_base + segment * transition_count + transition];
      result.transition_counts[transition] +=
          program.solution[sync_base + segment * transition_count + transition];
    }
  }
  bool recovery_exhausted = false;
  result.firing_sequence = recover_sequence(
      net, markings.initial, markings.final, result.transition_counts,
      options.realizability_limit, recovery_exhausted);
  result.reaches_final = !recovery_exhausted &&
                         (!result.firing_sequence.empty() ||
                          markings.initial == markings.final);
  if (recovery_exhausted) result.exhausted = true;
  return result;
}

WorkflowNetResult check_workflow_net(const PetriNet& net) {
  WorkflowNetResult result;
  if (net.place_count() == 0) { result.reason = "net has no places"; return result; }
  std::vector<std::vector<std::size_t>> outgoing(net.place_count() + net.transitions().size());
  std::vector<std::vector<std::size_t>> incoming(outgoing.size());
  bool invalid = false;
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    const auto node = net.place_count() + t;
    const auto& tr = net.transitions()[t];
    if (tr.activity && *tr.activity >= net.activities().size()) { result.offending_transitions.push_back(t); invalid = true; }
    for (const auto p : tr.inputs) {
      if (p >= net.place_count()) { result.offending_transitions.push_back(t); invalid = true; continue; }
      outgoing[p].push_back(node); incoming[node].push_back(p);
    }
    for (const auto p : tr.outputs) {
      if (p >= net.place_count()) { result.offending_transitions.push_back(t); invalid = true; continue; }
      outgoing[node].push_back(p); incoming[p].push_back(node);
    }
    for (const auto p : tr.reset_inputs) {
      if (p >= net.place_count()) { result.offending_transitions.push_back(t); invalid = true; continue; }
      outgoing[p].push_back(node); incoming[node].push_back(p);
    }
    for (const auto p : tr.inhibitor_inputs) {
      if (p >= net.place_count()) { result.offending_transitions.push_back(t); invalid = true; continue; }
      outgoing[p].push_back(node); incoming[node].push_back(p);
    }
  }
  std::sort(result.offending_transitions.begin(), result.offending_transitions.end());
  result.offending_transitions.erase(std::unique(result.offending_transitions.begin(), result.offending_transitions.end()), result.offending_transitions.end());
  if (invalid) { result.reason = "net contains an invalid place or activity id"; return result; }
  for (std::size_t p = 0; p < net.place_count(); ++p) {
    if (incoming[p].empty()) result.source_candidates.push_back(p);
    if (outgoing[p].empty()) result.sink_candidates.push_back(p);
  }
  if (result.source_candidates.size() != 1 || result.sink_candidates.size() != 1) {
    result.reason = "workflow net requires exactly one structural source and sink";
    result.offending_places = result.source_candidates;
    result.offending_places.insert(result.offending_places.end(), result.sink_candidates.begin(), result.sink_candidates.end());
    std::sort(result.offending_places.begin(), result.offending_places.end());
    result.offending_places.erase(std::unique(result.offending_places.begin(), result.offending_places.end()), result.offending_places.end());
    return result;
  }
  const auto source = result.source_candidates.front(), sink = result.sink_candidates.front();
  const auto visit = [](std::size_t start, const auto& graph) {
    std::vector<bool> seen(graph.size(), false); std::queue<std::size_t> q;
    seen[start] = true; q.push(start);
    while (!q.empty()) { const auto n = q.front(); q.pop(); for (const auto next : graph[n]) if (!seen[next]) { seen[next] = true; q.push(next); } }
    return seen;
  };
  const auto from_source = visit(source, outgoing);
  const auto to_sink = visit(sink, incoming);
  for (std::size_t p = 0; p < net.place_count(); ++p)
    if (!from_source[p] || !to_sink[p]) result.unreachable_places.push_back(p);
  for (std::size_t t = 0; t < net.transitions().size(); ++t) {
    const auto node = net.place_count() + t;
    if (!from_source[node] || !to_sink[node]) result.unreachable_transitions.push_back(t);
  }
  result.valid = result.unreachable_places.empty() && result.unreachable_transitions.empty();
  if (!result.valid) {
    result.reason = "some nodes are not on a source-to-sink path";
    result.offending_places = result.unreachable_places;
    result.offending_transitions = result.unreachable_transitions;
  }
  return result;
}

bool is_workflow_net(const PetriNet& net) { return check_workflow_net(net).valid; }

SoundnessResult analyze_soundness(const PetriNet& net, const SoundnessOptions& options) {
  SoundnessResult result;
  result.variant = options.variant;
  result.workflow = check_workflow_net(net);
  const auto validation_error = validate_net(net);
  std::optional<WorkflowMarkings> woflan_markings;
  if (options.variant == SoundnessVariant::kWoflan && validation_error.empty()) {
    woflan_markings.emplace(resolved_woflan_markings(
        net, result.workflow, options.initial_marking, options.final_marking));
    validate_single_place_marking(woflan_markings->initial, "initial marking");
    validate_single_place_marking(woflan_markings->final, "final marking");
  }
  std::optional<PetriNet> short_circuited;
  if (options.variant == SoundnessVariant::kWoflan) {
    const auto short_circuit = short_circuit_workflow_net(
        net, result.workflow, validation_error);
    result.short_circuit_valid = short_circuit.valid;
    result.short_circuit_strongly_connected =
        short_circuit.strongly_connected;
    result.short_circuit_transition = short_circuit.transition;
    result.short_circuit_diagnostic_messages = short_circuit.diagnostics;
    if (short_circuit.valid)
      short_circuited.emplace(short_circuit.net);
  }
  const PetriNet* structural_net = short_circuited ? &*short_circuited : &net;
  if (validation_error.empty()) {
    const auto structural_incidence = incidence(*structural_net);
    result.place_invariant_basis = invariant_basis(*structural_net);
    result.uncovered_places = uncovered_from_vectors(
        result.place_invariant_basis, structural_net->place_count());
    result.uniform_place_invariants =
        uniform_invariants(result.place_invariant_basis, structural_incidence,
                           options.invariant_search_limit,
                           result.uniform_invariant_search_exhausted);
    result.uncovered_places_uniform = uncovered_from_vectors(
        result.uniform_place_invariants, structural_net->place_count());
    result.weighted_place_invariants =
        weighted_invariants(result.place_invariant_basis, structural_incidence);
    result.uncovered_places_weighted = uncovered_from_vectors(
        result.weighted_place_invariants, structural_net->place_count());
    if (result.workflow.valid) {
      result.s_components = invariant_supports(result.uniform_place_invariants,
                                               *structural_net);
      result.uncovered_places_s_component = uncovered_from_supports(
          *result.s_components, structural_net->place_count());
    }
  }
  if (!result.workflow.valid) {
    result.status = SoundnessStatus::UNSOUND; result.computed = true;
    result.returned_asap = options.return_asap_when_unsound;
    result.error = result.workflow.reason.empty() ? validation_error
                                                  : result.workflow.reason;
    if (options.diagnostics) {
      result.diagnostic_messages.push_back(result.error);
      for (const auto& message : result.short_circuit_diagnostic_messages)
        result.diagnostic_messages.push_back("Woflan: " + message);
      if (!result.uncovered_places.empty())
        result.diagnostic_messages.push_back(
            "positive place-invariant coverage is missing for one or more places");
      if (result.uniform_invariant_search_exhausted)
        result.diagnostic_messages.push_back(
            "uniform invariant search budget exhausted; uniform coverage is incomplete");
    }
    return result;
  }
  if (!validation_error.empty()) throw std::invalid_argument(validation_error);

  // The structural Woflan pipeline stops at the first conclusive S-component
  // failure when the caller requests an early result.  Keep this decision
  // ahead of the remaining structural projections as well as the behavioral
  // searches; those projections belong to later Woflan stages.
  if (options.variant == SoundnessVariant::kWoflan &&
      result.s_components.has_value() &&
      !result.uniform_invariant_search_exhausted &&
      !result.uncovered_places_s_component.empty() &&
      options.return_asap_when_unsound) {
    result.status = SoundnessStatus::UNSOUND;
    result.computed = true;
    result.returned_asap = true;
    result.error = "S-component coverage is missing for one or more places";
    if (options.diagnostics) {
      result.diagnostic_messages.push_back(result.error);
      if (!result.uncovered_places.empty())
        result.diagnostic_messages.push_back(
            "positive place-invariant coverage is missing for one or more places");
      if (!result.uncovered_places_uniform.empty())
        result.diagnostic_messages.push_back(
            "uniform invariant coverage is missing for one or more places");
    }
    return result;
  }
  result.structural_node_labels = structural_node_labels(*structural_net);
  result.handled_pairs = structural_pair_diagnostics(*structural_net);
  // Keep the generic projection and the Woflan-named projection in lockstep
  // so adapters can use either contract without recomputing diagnostics.
  result.not_well_handled_pairs = result.handled_pairs;
  if (options.variant == SoundnessVariant::kWoflan &&
      result.s_components.has_value()) {
    if (result.uniform_invariant_search_exhausted) {
      // Incomplete binary support search is not evidence for or against the
      // Woflan structural branch. Leave the decision disengaged and continue
      // with behavioral diagnostics.
    } else if (result.uncovered_places_s_component.empty()) {
      result.left = true;
    } else if (!options.return_asap_when_unsound) {
      // The structural pair stage is the next Woflan decision point when
      // invariant coverage is incomplete.  A non-empty diagnostic set keeps
      // the pipeline on the behavioral path; an empty set proves it cannot.
      result.left = result.handled_pairs.has_value() &&
                    !result.handled_pairs->empty();
    }
  }
  if (options.max_states == 0)
    throw std::invalid_argument("max_states must be positive");
  if (options.invariant_search_limit == 0)
    throw std::invalid_argument("invariant_search_limit must be positive");
  const auto markings = woflan_markings
                            ? *woflan_markings
                            : resolved_markings(net, options.initial_marking,
                                                options.final_marking);
  if (short_circuited) {
    const auto short_circuit_graph = reachability_projection(
        *short_circuited, markings.initial, options.max_states);
    result.short_circuit_reachability_exhausted =
        short_circuit_graph.exhausted;
    result.short_circuit_reachability_markings =
        short_circuit_graph.markings;
    result.short_circuit_reachability_edges = short_circuit_graph.edges;
    result.short_circuit_reachability_strongly_connected =
        !short_circuit_graph.exhausted && graph_projection_strongly_connected(
            short_circuit_graph.markings.size(), short_circuit_graph.edges);
    if (options.diagnostics) {
      for (const auto& message : result.short_circuit_diagnostic_messages)
        result.diagnostic_messages.push_back("Woflan: " + message);
      if (short_circuit_graph.exhausted)
        result.diagnostic_messages.push_back(
            "Woflan short-circuit reachability state limit reached");
      else if (result.short_circuit_reachability_strongly_connected)
        result.diagnostic_messages.push_back(
            "Woflan short-circuit reachability graph is strongly connected");
      else
        result.diagnostic_messages.push_back(
            "Woflan short-circuit reachability graph is not strongly connected");
    }
  }
  const PetriNet* coverability_net = short_circuited ? &*short_circuited : &net;
  if (options.variant == SoundnessVariant::kWoflan) {
    const auto restricted = restricted_coverability_search(
        *coverability_net, markings.initial, options.max_states);
    result.restricted_coverability_tree_exhausted = restricted.exhausted;
    result.restricted_coverability_markings = restricted.markings;
    result.restricted_coverability_edges = restricted.edges;
  }
  const auto coverability = coverability_search(
      *coverability_net, markings.initial, options.max_states);
  result.coverability_markings = coverability.markings;
  result.coverability_edges = coverability.edges;
  if (coverability.unbounded) {
    result.bounded = false;
    result.computed = true;
    result.status = SoundnessStatus::UNSOUND;
    result.returned_asap = options.return_asap_when_unsound;
    result.unbounded_witness = coverability.witness;
    result.coverability_node_count = coverability.node_count;
    result.reachable_marking_count = coverability.node_count - 1;
    if (options.diagnostics) {
      std::string message = "coverability acceleration proves unbounded token growth";
      if (!coverability.omega_places.empty()) {
        message += " (omega places:";
        for (const auto p : coverability.omega_places)
          message += " " + std::to_string(p);
        message += ")";
      }
      result.diagnostic_messages.push_back(std::move(message));
      if (!result.uncovered_places.empty())
        result.diagnostic_messages.push_back(
            "positive place-invariant coverage is missing for one or more places");
      if (!result.uncovered_places_uniform.empty())
        result.diagnostic_messages.push_back(
            "uniform invariant coverage is missing for one or more places");
      if (result.uniform_invariant_search_exhausted)
        result.diagnostic_messages.push_back(
            "uniform invariant search budget exhausted; uniform coverage is incomplete");
      if (!result.uncovered_places_weighted.empty())
        result.diagnostic_messages.push_back(
            "weighted invariant coverage is missing for one or more places");
      if (result.s_components)
        result.diagnostic_messages.push_back(
            "positive place-invariant S-components found: " +
            std::to_string(result.s_components->size()));
    }
    return result;
  }
  struct Node { Marking marking; std::size_t parent; std::size_t transition; };
  std::vector<Node> nodes{{markings.initial, std::numeric_limits<std::size_t>::max(), std::numeric_limits<std::size_t>::max()}};
  std::map<Marking, std::size_t> ids{{markings.initial, 0}};
  std::queue<std::size_t> queue; queue.push(0);
  std::vector<std::vector<std::pair<std::size_t, std::size_t>>> edges(1);
  const auto store_reachability_graph = [&]() {
    result.reachability_markings.clear();
    result.reachability_markings.reserve(nodes.size());
    for (const auto& node : nodes) result.reachability_markings.push_back(node.marking);
    result.reachability_edges.clear();
    for (std::size_t from = 0; from < edges.size(); ++from)
      for (const auto [to, transition] : edges[from])
        result.reachability_edges.push_back({from, to, transition});
  };
  std::vector<bool> fired(net.transitions().size(), false);
  const auto path_to = [&](std::size_t id) {
    std::vector<std::size_t> path;
    while (nodes[id].parent != std::numeric_limits<std::size_t>::max()) {
      path.push_back(nodes[id].transition); id = nodes[id].parent;
    }
    std::reverse(path.begin(), path.end()); return path;
  };
  const auto return_asap = [&](std::string message) {
    result.bounded = true;
    result.computed = true;
    result.status = SoundnessStatus::UNSOUND;
    result.returned_asap = true;
    result.reachable_marking_count = nodes.size();
    result.coverability_node_count = nodes.size();
    result.error = std::move(message);
    store_reachability_graph();
    if (options.diagnostics)
      result.diagnostic_messages.push_back(result.error);
    return result;
  };
  while (!queue.empty()) {
    const auto id = queue.front(); queue.pop();
    bool any = false;
    for (std::size_t t = 0; t < net.transitions().size(); ++t) {
      Marking next;
      if (!fire(nodes[id].marking, net.transitions()[t], next)) continue;
      any = true; fired[t] = true;
      for (std::size_t ancestor = id; ancestor != std::numeric_limits<std::size_t>::max(); ancestor = nodes[ancestor].parent) {
        bool dominates = true, strict = false;
        for (std::size_t p = 0; p < next.size(); ++p) {
          if (next[p] < nodes[ancestor].marking[p]) { dominates = false; break; }
          strict = strict || next[p] > nodes[ancestor].marking[p];
        }
        if (dominates && strict) {
          result.bounded = false; result.computed = true; result.status = SoundnessStatus::UNSOUND;
          result.returned_asap = options.return_asap_when_unsound;
          result.unbounded_witness = path_to(id); result.unbounded_witness.push_back(t);
          result.coverability_node_count = nodes.size() + 1;
          result.reachable_marking_count = nodes.size();
          store_reachability_graph();
          if (options.diagnostics) result.diagnostic_messages.push_back("ancestor dominance proves unbounded token growth");
          return result;
        }
      }
      auto [it, inserted] = ids.emplace(next, nodes.size());
      if (inserted) {
        if (nodes.size() >= options.max_states) {
          result.status = SoundnessStatus::UNKNOWN; result.computed = false; result.exhausted = true;
          result.reachable_marking_count = nodes.size(); result.coverability_node_count = nodes.size();
          result.error = "reachability state limit reached";
          store_reachability_graph();
          if (options.diagnostics) result.diagnostic_messages.push_back(result.error);
          return result;
        }
        nodes.push_back({next, id, t}); edges.emplace_back(); queue.push(it->second);
      }
      edges[id].push_back({it->second, t});
    }
    if (!any && nodes[id].marking != markings.final) {
      result.deadlocks.push_back(nodes[id].marking);
      result.deadlock_witnesses.push_back(path_to(id));
      if (options.return_asap_when_unsound)
        return return_asap("reachable non-final deadlock found");
    }
    bool covers_final = true;
    for (std::size_t p = 0; p < markings.final.size(); ++p) {
      if (nodes[id].marking[p] < markings.final[p]) {
        covers_final = false;
        break;
      }
    }
    if (covers_final && nodes[id].marking != markings.final) {
      result.improper_markings.push_back(nodes[id].marking);
      result.improper_witnesses.push_back(path_to(id));
      if (options.return_asap_when_unsound)
        return return_asap("reachable improper completion marking found");
    }
  }
  result.bounded = true;
  result.reachable_marking_count = result.coverability_node_count = nodes.size();
  store_reachability_graph();
  const auto final_it = ids.find(markings.final);
  result.final_reachable = final_it != ids.end();
  if (final_it != ids.end()) result.completion_witness = path_to(final_it->second);
  result.proper_completion = result.improper_markings.empty();
  for (std::size_t t = 0; t < fired.size(); ++t) if (!fired[t]) result.dead_transitions.push_back(t);
  if (options.variant == SoundnessVariant::kWoflan) {
    // The structural Woflan task test is based on the short-circuited
    // coverability graph and considers visible tasks only.  The exact
    // original-net firing set remains available as dead_transitions for the
    // ordinary soundness contract.  Extended-arc nets and capped searches do
    // not expose a complete coverability graph, so use the exact graph as a
    // conservative fallback once it has finished.
    std::vector<bool> coverability_fired(net.transitions().size(), false);
    if (!coverability.exhausted && !coverability.unbounded) {
      for (const auto& edge : coverability.edges) {
        if (edge.size() == 3 && edge[2] < coverability_fired.size())
          coverability_fired[edge[2]] = true;
      }
    } else {
      coverability_fired = fired;
    }
    for (std::size_t t = 0; t < coverability_fired.size(); ++t) {
      if (coverability_fired[t] || net.transitions()[t].silent()) continue;
      result.dead_tasks.push_back(t);
    }
  } else {
    result.dead_tasks = result.dead_transitions;
  }
  std::vector<std::vector<std::size_t>> reverse(nodes.size());
  for (std::size_t from = 0; from < edges.size(); ++from)
    for (const auto [to, transition] : edges[from]) { (void)transition; reverse[to].push_back(from); }
  std::vector<bool> can_complete(nodes.size(), false);
  if (final_it != ids.end()) {
    std::queue<std::size_t> reverse_queue; reverse_queue.push(final_it->second); can_complete[final_it->second] = true;
    while (!reverse_queue.empty()) { const auto id = reverse_queue.front(); reverse_queue.pop(); for (const auto prev : reverse[id]) if (!can_complete[prev]) { can_complete[prev] = true; reverse_queue.push(prev); } }
  }
  result.option_to_complete = std::all_of(can_complete.begin(), can_complete.end(), [](bool value) { return value; });
  if (!result.option_to_complete) {
    for (std::size_t id = 1; id < nodes.size(); ++id) {
      if (can_complete[id]) continue;
      auto path = path_to(id);
      if (path.empty()) continue;
      if (result.locking_witness.empty()) result.locking_witness = path;
      result.locking_scenarios.push_back(std::move(path));
    }
  }
  const bool no_dead_tasks = options.variant == SoundnessVariant::kWoflan
                                 ? result.dead_tasks.empty()
                                 : result.dead_transitions.empty();
  const bool sound = result.final_reachable && result.option_to_complete &&
                     result.proper_completion && no_dead_tasks;
  result.status = sound ? SoundnessStatus::SOUND : SoundnessStatus::UNSOUND;
  result.computed = true;
  if (options.diagnostics) {
    result.diagnostic_messages.push_back(sound ? "all workflow soundness conditions hold" : "one or more workflow soundness conditions fail");
    if (result.s_components)
      result.diagnostic_messages.push_back(
          "positive place-invariant S-components found: " +
          std::to_string(result.s_components->size()));
    if (result.handled_pairs && !result.handled_pairs->empty())
      result.diagnostic_messages.push_back(
          "structural pair diagnostics found: " +
          std::to_string(result.handled_pairs->size()));
    if (!result.uncovered_places.empty())
      result.diagnostic_messages.push_back(
          "positive place-invariant coverage is missing for one or more places");
    if (!result.uncovered_places_uniform.empty())
      result.diagnostic_messages.push_back(
          "uniform invariant coverage is missing for one or more places");
    if (result.uniform_invariant_search_exhausted)
      result.diagnostic_messages.push_back(
          "uniform invariant search budget exhausted; uniform coverage is incomplete");
    if (!result.uncovered_places_weighted.empty())
      result.diagnostic_messages.push_back(
          "weighted invariant coverage is missing for one or more places");
    if (!result.uncovered_places_s_component.empty())
      result.diagnostic_messages.push_back(
          "S-component coverage is missing for one or more places");
    if (options.variant != SoundnessVariant::kWoflan &&
        !result.dead_transitions.empty())
      result.diagnostic_messages.push_back("the net contains dead transitions");
    if (!result.dead_tasks.empty())
      result.diagnostic_messages.push_back("dead tasks: " +
                                           std::to_string(result.dead_tasks.size()));
    if (!result.deadlocks.empty())
      result.diagnostic_messages.push_back("the net contains non-final deadlocks");
    if (!result.locking_scenarios.empty())
      result.diagnostic_messages.push_back(
          "locking scenarios: " +
          std::to_string(result.locking_scenarios.size()));
  }
  return result;
}

}  // namespace minera
