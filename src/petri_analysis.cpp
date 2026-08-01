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
  }
  return {};
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

WorkflowMarkings resolved_markings(const PetriNet& net, const Marking& initial,
                                   const Marking& final) {
  auto defaults = default_workflow_markings(net);
  if (!initial.empty()) defaults.initial = initial;
  if (!final.empty()) defaults.final = final;
  validate_marking(defaults.initial, net.place_count(), "initial marking");
  validate_marking(defaults.final, net.place_count(), "final marking");
  return defaults;
}

std::vector<std::int64_t> arc_weights(const std::vector<std::size_t>& arcs,
                                      std::size_t places) {
  std::vector<std::int64_t> weights(places, 0);
  for (const auto place : arcs) ++weights[place];
  return weights;
}

bool enabled(const Marking& marking, const PetriTransition& transition) {
  const auto required = arc_weights(transition.inputs, marking.size());
  for (std::size_t p = 0; p < marking.size(); ++p)
    if (marking[p] < required[p]) return false;
  return true;
}

bool fire(const Marking& marking, const PetriTransition& transition,
          Marking& next) {
  if (!enabled(marking, transition)) return false;
  next = marking;
  for (const auto p : transition.inputs) --next[p];
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
    const std::size_t objective = m_ + static_cast<std::size_t>(phase);
    while (true) {
      std::size_t col = 0;
      for (std::size_t j = 1; j <= n_; ++j) {
        if (phase == 0 && nonbasis_[j] == -1) continue;
        if (d_[objective][j] < d_[objective][col] - kEps ||
            (std::abs(d_[objective][j] - d_[objective][col]) <= kEps && nonbasis_[j] < nonbasis_[col])) col = j;
      }
      if (d_[objective][col] >= -kEps) return true;
      std::optional<std::size_t> row;
      for (std::size_t i = 0; i < m_; ++i) {
        if (d_[i][col] <= kEps) continue;
        if (!row || d_[i][n_ + 1] / d_[i][col] < d_[*row][n_ + 1] / d_[*row][col] - kEps ||
            (std::abs(d_[i][n_ + 1] / d_[i][col] - d_[*row][n_ + 1] / d_[*row][col]) <= kEps && basis_[i] < basis_[*row])) row = i;
      }
      if (!row) return false;
      pivot(*row, col);
    }
  }

  std::size_t m_, n_;
  std::vector<int> basis_, nonbasis_;
  std::vector<std::vector<double>> d_;
};

struct IntegerProgramResult {
  bool feasible = false;
  bool exhausted = false;
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
    if (explored++ >= node_limit) { result.exhausted = true; break; }
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
  if (result.feasible) result.objective = incumbent;
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
    const std::vector<std::vector<double>>& basis) {
  std::vector<std::vector<std::size_t>> supports;
  for (const auto& invariant : basis) {
    std::vector<std::size_t> support;
    for (std::size_t p = 0; p < invariant.size(); ++p)
      if (invariant[p] > kEps) support.push_back(p);
    if (support.empty()) continue;
    if (std::find(supports.begin(), supports.end(), support) == supports.end())
      supports.push_back(std::move(support));
  }
  return supports;
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
};

constexpr std::int64_t kOmega = -1;

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
  std::vector<Node> nodes;
  nodes.push_back({initial, std::numeric_limits<std::size_t>::max(),
                   std::numeric_limits<std::size_t>::max()});
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
        result.unbounded = true;
        result.witness = path_to(id);
        result.witness.push_back(t);
        for (std::size_t p = 0; p < accelerated_places.size(); ++p)
          if (accelerated_places[p]) result.omega_places.push_back(p);
        std::sort(result.omega_places.begin(), result.omega_places.end());
        result.omega_places.erase(
            std::unique(result.omega_places.begin(), result.omega_places.end()),
            result.omega_places.end());
        result.node_count = nodes.size() + 1;
        return result;
      }

      bool covered = false;
      // A newly reached marking can be subsumed only when it is no larger
      // than an already explored coverability node.  Reversing this test
      // would discard the larger state that may contain the unbounded branch.
      for (const auto& existing : nodes)
        if (cover_leq(next, existing.marking)) {
          covered = true;
          break;
        }
      if (covered) continue;
      if (nodes.size() >= max_nodes) {
        result.exhausted = true;
        result.node_count = nodes.size();
        return result;
      }
      nodes.push_back({std::move(next), id, t});
      queue.push(nodes.size() - 1);
    }
  }
  result.node_count = nodes.size();
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
    inequalities.push_back(matrix[p]); bounds.push_back(delta);
    auto negative = matrix[p]; for (auto& x : negative) x = -x;
    inequalities.push_back(std::move(negative)); bounds.push_back(-delta);
  }
  const auto program = solve_integer_program(inequalities, bounds, costs,
                                             options.realizability_limit);
  result.exhausted = program.exhausted;
  if (!program.feasible) {
    result.feasible = false;
    result.error = program.exhausted ? "marking equation node limit reached"
                                     : "marking equation is infeasible";
    return result;
  }
  result.feasible = true;
  result.objective = program.objective;
  result.lower_bound = program.root_lower_bound;
  result.transition_counts = program.solution;
  result.residual.resize(net.place_count());
  for (std::size_t p = 0; p < net.place_count(); ++p) {
    double value = static_cast<double>(markings.initial[p] - markings.final[p]);
    for (std::size_t t = 0; t < net.transitions().size(); ++t) value += matrix[p][t] * result.transition_counts[t];
    result.residual[p] = value;
  }
  for (std::size_t t = 0; t < result.transition_counts.size(); ++t)
    if (result.transition_counts[t] > kEps) result.activated_transitions.push_back(t);
  result.recovery_attempted = true;
  bool recovery_exhausted = false;
  result.firing_sequence = recover_sequence(net, markings.initial, markings.final,
                                            result.transition_counts,
                                            options.realizability_limit,
                                            recovery_exhausted);
  result.exhausted = recovery_exhausted;
  if (result.firing_sequence.empty()) {
    result.reaches_final = markings.initial == markings.final &&
        std::all_of(result.transition_counts.begin(), result.transition_counts.end(), [](double x) { return x <= kEps; });
  } else result.reaches_final = true;
  return result;
}

ExtendedMarkingEquationResult solve_extended_marking_equation(
    const std::vector<std::string>& trace, const PetriNet& net,
    const ExtendedMarkingEquationOptions& options) {
  if (const auto error = validate_net(net); !error.empty())
    throw std::invalid_argument(error);
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
  if (result.retained_splits.empty() && options.max_k_value > 0 &&
      trace.size() > 1) {
    const auto count = std::min(options.max_k_value, trace.size() - 1);
    for (std::size_t i = 1; i <= count; ++i)
      result.retained_splits.push_back(i);
    for (std::size_t i = 1; i <= count; ++i)
      result.retained_splits.push_back(trace.size() - i);
    std::sort(result.retained_splits.begin(), result.retained_splits.end());
    result.retained_splits.erase(
        std::unique(result.retained_splits.begin(), result.retained_splits.end()),
    result.retained_splits.end());
  }
  if (result.retained_splits.empty() && trace.size() > 1 &&
      options.max_k_value == 0) {
    result.retained_splits = {1, trace.size() - 1};
    std::sort(result.retained_splits.begin(), result.retained_splits.end());
    result.retained_splits.erase(
        std::unique(result.retained_splits.begin(), result.retained_splits.end()),
        result.retained_splits.end());
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
  result.workflow = check_workflow_net(net);
  const auto validation_error = validate_net(net);
  if (validation_error.empty()) {
    result.place_invariant_basis = invariant_basis(net);
    for (std::size_t p = 0; p < net.place_count(); ++p) {
      bool covered = false;
      for (const auto& invariant : result.place_invariant_basis)
        if (invariant[p] > kEps) { covered = true; break; }
      if (!covered) result.uncovered_places.push_back(p);
    }
    if (result.workflow.valid)
      result.s_components = invariant_supports(result.place_invariant_basis);
  }
  if (!result.workflow.valid) {
    result.status = SoundnessStatus::UNSOUND; result.computed = true;
    result.error = result.workflow.reason.empty() ? validation_error
                                                  : result.workflow.reason;
    if (options.diagnostics) {
      result.diagnostic_messages.push_back(result.error);
      if (!result.uncovered_places.empty())
        result.diagnostic_messages.push_back(
            "positive place-invariant coverage is missing for one or more places");
    }
    return result;
  }
  if (!validation_error.empty()) throw std::invalid_argument(validation_error);
  if (options.max_states == 0)
    throw std::invalid_argument("max_states must be positive");
  const auto markings = resolved_markings(net, options.initial_marking, options.final_marking);
  const auto coverability = coverability_search(net, markings.initial, options.max_states);
  if (coverability.unbounded) {
    result.bounded = false;
    result.computed = true;
    result.status = SoundnessStatus::UNSOUND;
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
  std::vector<bool> fired(net.transitions().size(), false);
  const auto path_to = [&](std::size_t id) {
    std::vector<std::size_t> path;
    while (nodes[id].parent != std::numeric_limits<std::size_t>::max()) {
      path.push_back(nodes[id].transition); id = nodes[id].parent;
    }
    std::reverse(path.begin(), path.end()); return path;
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
          result.unbounded_witness = path_to(id); result.unbounded_witness.push_back(t);
          result.coverability_node_count = nodes.size() + 1;
          result.reachable_marking_count = nodes.size();
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
          if (options.diagnostics) result.diagnostic_messages.push_back(result.error);
          return result;
        }
        nodes.push_back({next, id, t}); edges.emplace_back(); queue.push(it->second);
      }
      edges[id].push_back({it->second, t});
    }
    if (!any && nodes[id].marking != markings.final) result.deadlocks.push_back(nodes[id].marking);
    bool covers_final = true;
    for (std::size_t p = 0; p < markings.final.size(); ++p) {
      if (nodes[id].marking[p] < markings.final[p]) {
        covers_final = false;
        break;
      }
    }
    if (covers_final && nodes[id].marking != markings.final)
      result.improper_markings.push_back(nodes[id].marking);
  }
  result.bounded = true;
  result.reachable_marking_count = result.coverability_node_count = nodes.size();
  const auto final_it = ids.find(markings.final);
  result.final_reachable = final_it != ids.end();
  result.proper_completion = result.improper_markings.empty();
  for (std::size_t t = 0; t < fired.size(); ++t) if (!fired[t]) result.dead_transitions.push_back(t);
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
    for (std::size_t id = 1; id < nodes.size(); ++id)
      if (!can_complete[id]) { result.locking_witness = path_to(id); break; }
  }
  const bool sound = result.final_reachable && result.option_to_complete &&
                     result.proper_completion && result.dead_transitions.empty();
  result.status = sound ? SoundnessStatus::SOUND : SoundnessStatus::UNSOUND;
  result.computed = true;
  if (options.diagnostics) {
    result.diagnostic_messages.push_back(sound ? "all workflow soundness conditions hold" : "one or more workflow soundness conditions fail");
    if (result.s_components)
      result.diagnostic_messages.push_back(
          "positive place-invariant S-components found: " +
          std::to_string(result.s_components->size()));
    if (!result.uncovered_places.empty())
      result.diagnostic_messages.push_back(
          "positive place-invariant coverage is missing for one or more places");
    if (!result.dead_transitions.empty())
      result.diagnostic_messages.push_back("the net contains dead transitions");
    if (!result.deadlocks.empty())
      result.diagnostic_messages.push_back("the net contains non-final deadlocks");
  }
  return result;
}

}  // namespace minera
