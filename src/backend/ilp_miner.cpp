#include "minera/ilp_miner.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace minera {
namespace {

constexpr double kRegionEps = 1e-8;
constexpr std::size_t kRegionSolverNodeLimit = 200000;

struct NormalizedLog {
  std::vector<std::string> names;
  struct Trace {
    std::vector<std::size_t> sequence;
    std::size_t frequency = 1;
    // Number of cumulative sequence-encoding rows retained for this variant
    // after alpha filtering; a shorter prefix means the remaining suffix is
    // intentionally absent from region feasibility constraints.
    std::size_t retained_prefix = 0;
  };
  std::vector<Trace> traces;
  std::vector<std::pair<std::size_t, std::size_t>> causal_relation;
  bool causal_relation_supplied = false;
  std::set<std::pair<std::size_t, std::size_t>> sequence_edges;
};

using SequenceVector = std::vector<std::int64_t>;

using BitMask = std::vector<std::uint64_t>;

BitMask empty_mask(std::size_t activity_count) {
  return BitMask((activity_count + 63) / 64, 0);
}

void set_bit(BitMask& mask, std::size_t activity) {
  mask[activity / 64] |= std::uint64_t{1} << (activity % 64);
}

bool has_bit(const BitMask& mask, std::size_t activity) {
  return (mask[activity / 64] & (std::uint64_t{1} << (activity % 64))) != 0;
}

std::size_t bit_count(const BitMask& mask) {
  std::size_t count = 0;
  for (const auto word : mask) count += std::popcount(word);
  return count;
}

bool any_bit(const BitMask& mask) {
  return std::any_of(mask.begin(), mask.end(),
                     [](std::uint64_t word) { return word != 0; });
}

bool masks_equal(const BitMask& left, const BitMask& right) {
  return left == right;
}

struct Region {
  // One bit per activity.  A place may both consume and produce an activity
  // (a self-loop), which is deliberately distinct from a zero net change.
  BitMask input;
  BitMask output;
  std::size_t initial = 0;
  std::size_t final = 0;
  // Cumulative-row objective used to choose the best region for a pair.
  std::int64_t objective = 0;
};

struct RegionRow {
  // Coefficients for input (place -> transition) and output
  // (transition -> place) arcs in one cumulative sequence row.
  std::vector<std::int64_t> input;
  std::vector<std::int64_t> output;
  bool equality = false;
};

struct RegionProblem {
  std::size_t activity_count = 0;
  std::vector<RegionRow> rows;
  std::vector<std::int64_t> objective_input;
  std::vector<std::int64_t> objective_output;
  bool have_observed_prefix = false;
};

using RegionRowKey =
    std::tuple<bool, std::vector<std::int64_t>, std::vector<std::int64_t>>;

std::int64_t saturating_add(std::int64_t left, std::int64_t right) {
  if (right > 0 && left > std::numeric_limits<std::int64_t>::max() - right)
    return std::numeric_limits<std::int64_t>::max();
  if (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right)
    return std::numeric_limits<std::int64_t>::min();
  return static_cast<std::int64_t>(left + right);
}

NormalizedLog normalize(const EventLog& log, const ILPMinerOptions& options) {
  if (log.traces.empty() || log.event_count() == 0)
    throw std::invalid_argument("ILP miner requires a non-empty event log");

  if (!std::isfinite(options.alpha) || options.alpha < 0.0 || options.alpha > 1.0)
    throw std::invalid_argument("ILP miner alpha must be in [0,1]");
  if (!options.artificial_start_activity.empty() &&
      options.artificial_start_activity == options.artificial_end_activity)
    throw std::invalid_argument("ILP miner artificial boundary labels must differ");

  std::set<std::string> names;
  std::vector<std::vector<std::string>> raw;
  for (const auto& trace : log.traces) {
    std::vector<std::string> sequence;
    sequence.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      std::string label = event.activity;
      if (!options.activity_key.empty() && options.activity_key != "activity" &&
          options.activity_key != "concept:name") {
        if (options.activity_key == "resource" || options.activity_key == "org:resource") {
          if (!event.resource.empty()) label = event.resource;
        } else if (const auto* value = event.attribute(options.activity_key);
                   value != nullptr && !value->empty()) {
          label = *value;
        }
      }
      if (!label.empty()) {
        names.insert(label);
        sequence.push_back(std::move(label));
      }
    }
    if (!sequence.empty()) {
      if (!options.artificial_start_activity.empty())
        sequence.insert(sequence.begin(), options.artificial_start_activity);
      if (!options.artificial_end_activity.empty())
        sequence.push_back(options.artificial_end_activity);
      raw.push_back(std::move(sequence));
    }
  }
  if (raw.empty() || names.empty())
    throw std::invalid_argument("ILP miner found no activities");

  if ((!options.artificial_start_activity.empty() &&
       names.count(options.artificial_start_activity) != 0) ||
      (!options.artificial_end_activity.empty() &&
       names.count(options.artificial_end_activity) != 0)) {
    throw std::invalid_argument(
        "ILP miner artificial boundary label collides with an activity");
  }
  if (!options.artificial_start_activity.empty())
    names.insert(options.artificial_start_activity);
  if (!options.artificial_end_activity.empty())
    names.insert(options.artificial_end_activity);

  NormalizedLog out;
  // Keep one representative per variant in first-seen order while retaining
  // its multiplicity for support-based filtering.  This bounds the region
  // search without discarding frequency information.
  std::map<std::vector<std::string>, std::size_t> variant_ids;
  std::vector<std::size_t> frequencies;
  std::vector<std::vector<std::string>> variants;
  for (auto& sequence : raw) {
    const auto [it, inserted] = variant_ids.emplace(sequence, variants.size());
    if (inserted) {
      frequencies.push_back(1);
      variants.push_back(std::move(sequence));
    } else {
      ++frequencies[it->second];
    }
  }
  std::size_t max_frequency = 0;
  for (const auto frequency : frequencies) max_frequency = std::max(max_frequency, frequency);
  std::vector<bool> retained(variants.size(), true);
  if (options.noise_threshold > 0.0) {
    for (std::size_t i = 0; i < frequencies.size(); ++i)
      retained[i] = static_cast<double>(frequencies[i]) + 1e-12 >=
                    options.noise_threshold * static_cast<double>(max_frequency);
    if (std::none_of(retained.begin(), retained.end(), [](bool value) { return value; }))
      retained[static_cast<std::size_t>(std::distance(
          frequencies.begin(), std::max_element(frequencies.begin(), frequencies.end())))] = true;
  }
  std::set<std::string> retained_names;
  for (std::size_t i = 0; i < variants.size(); ++i) {
    if (!retained[i]) continue;
    retained_names.insert(variants[i].begin(), variants[i].end());
  }
  out.names.assign(retained_names.begin(), retained_names.end());
  std::map<std::string, std::size_t> ids;
  for (std::size_t i = 0; i < out.names.size(); ++i)
    ids.emplace(out.names[i], i);
  out.traces.reserve(variants.size());
  for (std::size_t i = 0; i < variants.size(); ++i) {
    if (!retained[i]) continue;
    std::vector<std::size_t> encoded;
    encoded.reserve(variants[i].size());
    for (const auto& value : variants[i]) encoded.push_back(ids.at(value));
    const auto encoded_size = encoded.size();
    out.traces.push_back({std::move(encoded), frequencies[i], encoded_size});
  }
  if (out.traces.empty()) throw std::invalid_argument("ILP miner found no retained variants");

  // Build the cumulative occurrence-vector sequence encoding.  Each event is
  // represented by the complete prefix count vector, while the predecessor
  // vector is negated to distinguish the source prefix from the current
  // prefix.  The alpha threshold is applied to continuations sharing the
  // same predecessor encoding, and a rejected continuation cuts the rest of
  // that variant just as the bounded sequence-encoding search does.
  struct EncodingStep {
    SequenceVector previous;
    SequenceVector current;
  };
  std::map<SequenceVector, std::map<SequenceVector, std::uint64_t>> support;
  std::vector<std::vector<EncodingStep>> encoding_steps;
  encoding_steps.reserve(out.traces.size());
  for (const auto& trace : out.traces) {
    SequenceVector cumulative(out.names.size(), 0);
    std::vector<EncodingStep> steps;
    steps.reserve(trace.sequence.size());
    for (std::size_t i = 0; i < trace.sequence.size(); ++i) {
      SequenceVector previous = cumulative;
      if (i != 0) {
        for (auto& value : previous) value = -value;
      }
      ++cumulative[trace.sequence[i]];
      SequenceVector current = cumulative;
      auto& support_count = support[previous][current];
      const auto frequency = static_cast<std::uint64_t>(trace.frequency);
      if (std::numeric_limits<std::uint64_t>::max() - support_count < frequency)
        support_count = std::numeric_limits<std::uint64_t>::max();
      else
        support_count += frequency;
      steps.push_back({std::move(previous), std::move(current)});
    }
    encoding_steps.push_back(std::move(steps));
  }
  for (std::size_t trace_index = 0; trace_index < out.traces.size(); ++trace_index) {
    auto& trace = out.traces[trace_index];
    const auto& steps = encoding_steps[trace_index];
    trace.retained_prefix = 0;
    for (std::size_t i = 0; i < steps.size(); ++i) {
      const auto support_it = support.find(steps[i].previous);
      if (support_it == support.end()) break;
      std::uint64_t strongest = 0;
      for (const auto& [_, count] : support_it->second)
        strongest = std::max(strongest, count);
      const auto continuation_it = support_it->second.find(steps[i].current);
      if (continuation_it == support_it->second.end()) break;
      const auto count = continuation_it->second;
      const bool keep_step = options.alpha >= 1.0 ||
                             static_cast<long double>(count) >=
                                 (1.0L - static_cast<long double>(options.alpha)) *
                                     static_cast<long double>(strongest);
      if (!keep_step) break;
      trace.retained_prefix = i + 1;
      if (i > 0)
        out.sequence_edges.emplace(trace.sequence[i - 1], trace.sequence[i]);
    }
  }

  out.causal_relation_supplied = !options.causal_relation.empty();
  if (options.causal_relation.empty()) {
    // The default causal relation is the one-way portion of the normalized
    // directly-follows graph.  Reverse edges are excluded so loops and
    // concurrency do not turn every observed transition into a causal pair.
    std::map<std::pair<std::size_t, std::size_t>, std::uint64_t> dfg;
    for (const auto& trace : out.traces) {
      for (std::size_t i = 1; i < trace.sequence.size(); ++i)
        dfg[{trace.sequence[i - 1], trace.sequence[i]}] +=
            static_cast<std::uint64_t>(trace.frequency);
    }
    for (const auto& [edge, count] : dfg) {
      if (count == 0 || dfg.contains({edge.second, edge.first})) continue;
      out.causal_relation.push_back(edge);
    }
  } else {
    for (const auto& [from, to] : options.causal_relation) {
      const auto from_it = ids.find(from);
      const auto to_it = ids.find(to);
      if (from_it == ids.end() || to_it == ids.end())
        throw std::invalid_argument("ILP miner causal relation names an unknown activity");
      out.causal_relation.emplace_back(from_it->second, to_it->second);
    }
  }
  return out;
}

RegionProblem build_region_problem(
    const std::vector<NormalizedLog::Trace>& traces, std::size_t activity_count) {
  RegionProblem problem;
  problem.activity_count = activity_count;
  problem.objective_input.assign(activity_count, 0);
  problem.objective_output.assign(activity_count, 0);
  // The linear formulation has one constraint per distinct encoding row.
  // Keep objective accumulation below outside this set: the reference
  // objective is still summed once per retained variant row, while repeated
  // rows need not be solved repeatedly as identical feasibility constraints.
  std::set<RegionRowKey> constraint_rows;
  for (const auto& trace_entry : traces) {
    const auto& trace = trace_entry.sequence;
    const auto retained_prefix =
        std::min(trace_entry.retained_prefix, trace.size());
    std::vector<std::int64_t> cumulative(activity_count, 0);
    for (std::size_t i = 0; i < retained_prefix; ++i) {
      std::vector<std::int64_t> previous = cumulative;
      if (i != 0) {
        for (auto& value : previous) value = -value;
      }
      if (cumulative[trace[i]] < std::numeric_limits<std::int64_t>::max())
        ++cumulative[trace[i]];

      RegionRow row;
      row.input = cumulative;
      row.output = std::move(previous);
      row.equality = retained_prefix == trace.size() && i + 1 == retained_prefix;
      if (constraint_rows
              .insert({row.equality, row.output, row.input})
              .second)
        problem.rows.push_back(row);
      problem.have_observed_prefix = true;
      for (std::size_t activity = 0; activity < activity_count; ++activity) {
        problem.objective_output[activity] = saturating_add(
            problem.objective_output[activity], cumulative[activity]);
        problem.objective_input[activity] = saturating_add(
            problem.objective_input[activity], -cumulative[activity]);
      }
    }
  }
  return problem;
}

// Check one candidate signature against the cumulative sequence rows.  The
// binary variables describe transition-to-place (output) and
// place-to-transition (input) arcs.  The deviation variable is fixed to zero,
// so each retained row enforces pre-fire enablement and each complete trace's
// last row is an equality.
bool solve_region(const BitMask& input, const BitMask& output,
                  const RegionProblem& problem, Region& out) {
  if (!any_bit(input) && !any_bit(output)) return false;
  std::int64_t objective = 0;
  for (const auto& row : problem.rows) {
    std::int64_t lhs = 0;
    for (std::size_t activity = 0; activity < problem.activity_count;
         ++activity) {
      if (has_bit(output, activity))
        lhs = saturating_add(lhs, row.output[activity]);
      if (has_bit(input, activity))
        lhs = saturating_add(lhs, row.input[activity]);
    }
    if (row.equality ? lhs != 0 : lhs > 0) return false;
  }
  if (!problem.have_observed_prefix) return false;
  for (std::size_t activity = 0; activity < problem.activity_count;
       ++activity) {
    if (has_bit(output, activity))
      objective = saturating_add(objective, problem.objective_output[activity]);
    if (has_bit(input, activity))
      objective = saturating_add(objective, problem.objective_input[activity]);
  }
  out = Region{input, output, 0, 0, objective};
  return true;
}

bool region_less(const Region& a, const Region& b);

struct BinaryRegionSearchResult {
  bool feasible = false;
  bool exhausted = false;
  std::size_t nodes_explored = 0;
  Region region;
};

// Two-phase dense simplex for max c*x subject to A*x<=b, x>=0.  Keeping the
// relaxation local to the miner lets the integer region search remain
// dependency-free while still solving large sparse alphabets efficiently.
class RegionSimplex {
 public:
  RegionSimplex(const std::vector<std::vector<double>>& a,
                const std::vector<double>& b, const std::vector<double>& c)
      : m_(b.size()), n_(c.size()), basis_(m_), nonbasis_(n_ + 1),
        tableau_(m_ + 2, std::vector<double>(n_ + 2)) {
    for (std::size_t i = 0; i < m_; ++i)
      for (std::size_t j = 0; j < n_; ++j) tableau_[i][j] = a[i][j];
    for (std::size_t i = 0; i < m_; ++i) {
      basis_[i] = static_cast<int>(n_ + i);
      tableau_[i][n_] = -1.0;
      tableau_[i][n_ + 1] = b[i];
    }
    for (std::size_t j = 0; j < n_; ++j) {
      nonbasis_[j] = static_cast<int>(j);
      tableau_[m_][j] = -c[j];
    }
    nonbasis_[n_] = -1;
    tableau_[m_ + 1][n_] = 1.0;
  }

  enum class Status { kOptimal, kInfeasible, kUnbounded };

  Status solve(std::vector<double>& solution, double& value) {
    std::size_t row = 0;
    for (std::size_t i = 1; i < m_; ++i)
      if (tableau_[i][n_ + 1] < tableau_[row][n_ + 1]) row = i;
    if (m_ && tableau_[row][n_ + 1] < -kRegionEps) {
      pivot(row, n_);
      if (!run(1) || tableau_[m_ + 1][n_ + 1] < -kRegionEps)
        return Status::kInfeasible;
      if (std::abs(tableau_[m_ + 1][n_ + 1]) > kRegionEps)
        return Status::kInfeasible;
      for (std::size_t i = 0; i < m_; ++i) {
        if (basis_[i] != -1) continue;
        std::size_t column = 0;
        for (std::size_t j = 1; j <= n_; ++j)
          if (tableau_[i][j] < tableau_[i][column] - kRegionEps ||
              (std::abs(tableau_[i][j] - tableau_[i][column]) <= kRegionEps &&
               nonbasis_[j] < nonbasis_[column]))
            column = j;
        pivot(i, column);
      }
    }
    if (!run(0)) return Status::kUnbounded;
    solution.assign(n_, 0.0);
    for (std::size_t i = 0; i < m_; ++i)
      if (basis_[i] >= 0 && static_cast<std::size_t>(basis_[i]) < n_)
        solution[static_cast<std::size_t>(basis_[i])] = tableau_[i][n_ + 1];
    value = tableau_[m_][n_ + 1];
    return Status::kOptimal;
  }

 private:
  void pivot(std::size_t row, std::size_t column) {
    const double inverse = 1.0 / tableau_[row][column];
    for (std::size_t i = 0; i < m_ + 2; ++i) {
      if (i == row) continue;
      for (std::size_t j = 0; j < n_ + 2; ++j) {
        if (j == column) continue;
        tableau_[i][j] -= tableau_[row][j] * tableau_[i][column] * inverse;
      }
    }
    for (std::size_t j = 0; j < n_ + 2; ++j)
      if (j != column) tableau_[row][j] *= inverse;
    for (std::size_t i = 0; i < m_ + 2; ++i)
      if (i != row) tableau_[i][column] *= -inverse;
    tableau_[row][column] = inverse;
    std::swap(basis_[row], nonbasis_[column]);
  }

  bool run(int phase) {
    const std::size_t objective = m_ + static_cast<std::size_t>(phase);
    while (true) {
      std::size_t column = 0;
      for (std::size_t j = 1; j <= n_; ++j) {
        if (phase == 0 && nonbasis_[j] == -1) continue;
        if (tableau_[objective][j] < tableau_[objective][column] - kRegionEps ||
            (std::abs(tableau_[objective][j] - tableau_[objective][column]) <=
                 kRegionEps &&
             nonbasis_[j] < nonbasis_[column]))
          column = j;
      }
      if (tableau_[objective][column] >= -kRegionEps) return true;
      std::optional<std::size_t> row;
      for (std::size_t i = 0; i < m_; ++i) {
        if (tableau_[i][column] <= kRegionEps) continue;
        if (!row ||
            tableau_[i][n_ + 1] / tableau_[i][column] <
                tableau_[*row][n_ + 1] / tableau_[*row][column] - kRegionEps ||
            (std::abs(tableau_[i][n_ + 1] / tableau_[i][column] -
                      tableau_[*row][n_ + 1] / tableau_[*row][column]) <=
                 kRegionEps &&
             basis_[i] < basis_[*row]))
          row = i;
      }
      if (!row) return false;
      pivot(*row, column);
    }
  }

  std::size_t m_, n_;
  std::vector<int> basis_, nonbasis_;
  std::vector<std::vector<double>> tableau_;
};

BinaryRegionSearchResult solve_binary_region(
    const RegionProblem& problem, std::size_t required_output,
    std::size_t required_input, std::size_t node_limit) {
  BinaryRegionSearchResult result;
  const std::size_t n = problem.activity_count;
  if (required_output >= n || required_input >= n || !problem.have_observed_prefix)
    return result;
  const std::size_t variable_count = 2 * n;
  std::vector<std::vector<double>> constraints;
  std::vector<double> bounds;
  const auto add_inequality = [&](std::vector<double> row, double bound) {
    constraints.push_back(std::move(row));
    bounds.push_back(bound);
  };
  const auto add_equality = [&](std::vector<double> row) {
    add_inequality(row, 0.0);
    for (auto& value : row) value = -value;
    add_inequality(std::move(row), 0.0);
  };
  for (const auto& row : problem.rows) {
    std::vector<double> coefficients(variable_count, 0.0);
    for (std::size_t activity = 0; activity < n; ++activity) {
      coefficients[activity] = static_cast<double>(row.output[activity]);
      coefficients[n + activity] = static_cast<double>(row.input[activity]);
    }
    if (row.equality)
      add_equality(std::move(coefficients));
    else
      add_inequality(std::move(coefficients), 0.0);
  }
  std::vector<double> non_empty(variable_count, -1.0);
  add_inequality(std::move(non_empty), -1.0);
  for (std::size_t variable = 0; variable < variable_count; ++variable) {
    std::vector<double> upper(variable_count, 0.0);
    upper[variable] = 1.0;
    add_inequality(std::move(upper), 1.0);
  }
  auto fix = [&](std::size_t variable) {
    std::vector<double> row(variable_count, 0.0);
    row[variable] = 1.0;
    add_inequality(row, 1.0);
    row[variable] = -1.0;
    add_inequality(std::move(row), -1.0);
  };
  fix(required_output);
  fix(n + required_input);

  std::vector<double> costs(variable_count, 0.0);
  const double tie_scale = static_cast<double>(variable_count + 1);
  for (std::size_t activity = 0; activity < n; ++activity) {
    costs[activity] =
        static_cast<double>(problem.objective_output[activity]) * tie_scale + 1.0;
    costs[n + activity] =
        static_cast<double>(problem.objective_input[activity]) * tie_scale + 1.0;
  }

  struct Bound {
    std::size_t variable;
    bool upper;
    double value;
  };
  struct Node {
    std::vector<Bound> bounds;
  };
  std::vector<Node> stack(1);
  bool found = false;
  double incumbent = std::numeric_limits<double>::infinity();
  Region best_region;
  while (!stack.empty()) {
    if (result.nodes_explored >= node_limit) {
      result.exhausted = true;
      break;
    }
    ++result.nodes_explored;
    auto node = std::move(stack.back());
    stack.pop_back();
    auto node_constraints = constraints;
    auto node_bounds = bounds;
    for (const auto& bound : node.bounds) {
      std::vector<double> row(variable_count, 0.0);
      row[bound.variable] = bound.upper ? 1.0 : -1.0;
      node_constraints.push_back(std::move(row));
      node_bounds.push_back(bound.upper ? bound.value : -bound.value);
    }
    std::vector<double> maximize(costs.size(), 0.0);
    for (std::size_t variable = 0; variable < costs.size(); ++variable)
      maximize[variable] = -costs[variable];
    RegionSimplex simplex(node_constraints, node_bounds, maximize);
    std::vector<double> solution;
    double maximum = 0.0;
    if (simplex.solve(solution, maximum) != RegionSimplex::Status::kOptimal)
      continue;
    const double lower = -maximum;
    if (found && lower >= incumbent - kRegionEps) continue;
    std::optional<std::size_t> fractional;
    for (std::size_t variable = 0; variable < solution.size(); ++variable) {
      if (std::abs(solution[variable] - std::round(solution[variable])) > 1e-7) {
        fractional = variable;
        break;
      }
    }
    if (!fractional) {
      BitMask input = empty_mask(n);
      BitMask output = empty_mask(n);
      for (std::size_t activity = 0; activity < n; ++activity) {
        if (solution[activity] > 0.5) set_bit(output, activity);
        if (solution[n + activity] > 0.5) set_bit(input, activity);
      }
      Region candidate;
      if (!solve_region(input, output, problem, candidate)) continue;
      const double candidate_cost =
          static_cast<double>(candidate.objective) * tie_scale +
          static_cast<double>(bit_count(candidate.input) +
                              bit_count(candidate.output));
      if (!found || candidate_cost < incumbent - kRegionEps ||
          (std::abs(candidate_cost - incumbent) <= kRegionEps &&
           region_less(candidate, best_region))) {
        found = true;
        incumbent = candidate_cost;
        best_region = std::move(candidate);
      }
      continue;
    }
    const auto variable = *fractional;
    const double floor_value = std::floor(solution[variable]);
    const double ceil_value = std::ceil(solution[variable]);
    Node lower_branch = node;
    lower_branch.bounds.push_back({variable, false, ceil_value});
    node.bounds.push_back({variable, true, floor_value});
    // Push the lower branch first so the LIFO stack explores the upper branch
    // first, matching the deterministic tie order used by the other integer
    // analyses.
    stack.push_back(std::move(lower_branch));
    stack.push_back(std::move(node));
  }
  if (found) {
    result.feasible = true;
    result.region = std::move(best_region);
  }
  return result;
}

bool region_less(const Region& a, const Region& b) {
  if (a.objective != b.objective) return a.objective < b.objective;
  const auto arcs = [](const Region& r) {
    return bit_count(r.input) + bit_count(r.output);
  };
  if (arcs(a) != arcs(b)) return arcs(a) < arcs(b);
  if (a.initial != b.initial) return a.initial < b.initial;
  if (a.final != b.final) return a.final < b.final;
  if (a.input != b.input) return a.input < b.input;
  return a.output < b.output;
}

std::vector<Region> discover_regions(
    const std::vector<NormalizedLog::Trace>& traces, std::size_t n,
    const std::vector<std::pair<std::size_t, std::size_t>>& causal_relation,
    const std::set<std::pair<std::size_t, std::size_t>>& sequence_edges,
    bool causal_relation_supplied, ILPMinerDiagnostics& diagnostics,
    std::size_t max_nodes) {
  (void)sequence_edges;
  (void)causal_relation_supplied;
  // The classic region problem has one binary variable for every transition
  // output arc (x) and input arc (y).  The cumulative sequence rows are shared
  // across causal pairs; only the two pair-fixing equalities vary per solve.
  const auto problem = build_region_problem(traces, n);
  std::vector<Region> regions;
  std::set<std::pair<std::size_t, std::size_t>> solved_pairs;
  for (const auto [from, to] : causal_relation) {
    if (!solved_pairs.insert({from, to}).second) continue;
    if (diagnostics.visited_nodes >= max_nodes) {
      diagnostics.exhausted = true;
      break;
    }
    ++diagnostics.visited_nodes;
    ++diagnostics.lp_solved;

    if (from >= n || to >= n) continue;
    const auto solved =
        solve_binary_region(problem, from, to, kRegionSolverNodeLimit);
    diagnostics.branch_and_bound_nodes += solved.nodes_explored;
    if (solved.exhausted) {
      diagnostics.exhausted = true;
    }
    if (solved.feasible) regions.push_back(solved.region);
  }
  if (diagnostics.visited_nodes >= max_nodes) diagnostics.exhausted = true;
  std::sort(regions.begin(), regions.end(), region_less);
  regions.erase(std::unique(regions.begin(), regions.end(), [](const Region& a,
                                                               const Region& b) {
                  return masks_equal(a.input, b.input) &&
                         masks_equal(a.output, b.output) &&
                         a.initial == b.initial && a.final == b.final;
                }),
                regions.end());
  return regions;
}

}  // namespace

ILPMinerResult mine_ilp(const EventLog& event_log, const ILPMinerOptions& options) {
  if (!std::isfinite(options.noise_threshold) || options.noise_threshold < 0.0 ||
      options.noise_threshold > 1.0)
    throw std::invalid_argument("ILP miner noise_threshold must be in [0,1]");
  if (options.max_nodes == 0)
    throw std::invalid_argument("ILP miner max_nodes must be positive");

  const NormalizedLog log = normalize(event_log, options);
  const std::size_t n = log.names.size();
  const auto boundary_id = [&](const std::string& label) {
    if (label.empty()) return std::size_t{std::numeric_limits<std::size_t>::max()};
    const auto it = std::lower_bound(log.names.begin(), log.names.end(), label);
    return it == log.names.end() || *it != label
               ? std::size_t{std::numeric_limits<std::size_t>::max()}
               : static_cast<std::size_t>(it - log.names.begin());
  };
  const auto artificial_start = boundary_id(options.artificial_start_activity);
  const auto artificial_end = boundary_id(options.artificial_end_activity);
  std::vector<bool> artificial(n, false);
  if (artificial_start != std::numeric_limits<std::size_t>::max())
    artificial[artificial_start] = true;
  if (artificial_end != std::numeric_limits<std::size_t>::max())
    artificial[artificial_end] = true;
  std::vector<std::size_t> visible_id(n, std::numeric_limits<std::size_t>::max());
  std::vector<std::string> visible_names;
  visible_names.reserve(n);
  for (std::size_t a = 0; a < n; ++a) {
    if (artificial[a]) continue;
    visible_id[a] = visible_names.size();
    visible_names.push_back(log.names[a]);
  }
  std::vector<std::size_t> starts(n), ends(n), incoming(n), outgoing(n);
  for (const auto& trace_entry : log.traces) {
    const auto& trace = trace_entry.sequence;
    if (trace.empty()) continue;
    ++starts[trace.front()];
    ++ends[trace.back()];
    for (std::size_t i = 1; i < trace.size(); ++i) {
      ++outgoing[trace[i - 1]];
      ++incoming[trace[i]];
    }
  }

  ILPMinerResult result;
  result.net.set_activities(std::move(visible_names));
  const std::size_t source = result.net.add_place();
  const std::size_t sink = result.net.add_place();
  result.net.set_source(source);
  result.net.set_sink(sink);

  const auto regions = discover_regions(log.traces, n, log.causal_relation,
                                        log.sequence_edges,
                                        log.causal_relation_supplied,
                                        result.diagnostics, options.max_nodes);
  std::vector<std::vector<std::size_t>> inputs(n), outputs(n);
  std::vector<bool> persistent_output(n, false);
  std::vector<std::size_t> initial_places;
  std::vector<std::size_t> final_places;
  for (const auto& region : regions) {
    // Each selected integer region is a place in the discovered net.  The
    // solver's zero-deviation constraints already enforce all required
    // boundaries, so no post-hoc overlap or marking filters are applied here.
    const std::size_t place = result.net.add_place();
    for (std::size_t a = 0; a < n; ++a) {
      if (has_bit(region.input, a)) inputs[a].push_back(place);
      if (has_bit(region.output, a)) {
        outputs[a].push_back(place);
        if (region.final != 0) persistent_output[a] = true;
      }
    }
    // Initial/final tokens of a region are routed through the artificial
    // source/sink, keeping WorkflowMarkings canonical (one source token and
    // one sink token) while still supporting loops and persistent places.
    if (region.initial != 0) initial_places.push_back(place);
    if (region.final != 0) final_places.push_back(place);
  }
  for (std::size_t a = 0; a < n; ++a) {
    if (initial_places.empty() && (starts[a] != 0 || incoming[a] == 0))
      inputs[a].push_back(source);
    if (final_places.empty() && (ends[a] != 0 || outgoing[a] == 0) &&
        !persistent_output[a])
      outputs[a].push_back(sink);
  }
  // Every visible transition must lie on a source-to-sink path.  When a
  // persistent region split/join is present, give an otherwise unconstrained
  // activity its own routing place so it does not become an isolated node.
  for (std::size_t a = 0; a < n; ++a) {
    if (inputs[a].empty()) {
      if (initial_places.empty()) {
        inputs[a].push_back(source);
      } else {
        const auto place = result.net.add_place();
        initial_places.push_back(place);
        inputs[a].push_back(place);
      }
    }
    if (outputs[a].empty()) {
      if (final_places.empty()) {
        outputs[a].push_back(sink);
      } else {
        const auto place = result.net.add_place();
        final_places.push_back(place);
        outputs[a].push_back(place);
      }
    }
  }

  // A region marking is a vector, so initialize/finalize all persistent
  // regions in one source split and one sink join.  Separate silent edges
  // would compete for the single source token and make otherwise valid logs
  // unreplayable.
  if (!initial_places.empty())
    result.net.add_transition(std::nullopt, {source}, initial_places);
  if (!final_places.empty())
    result.net.add_transition(std::nullopt, final_places, {sink});
  for (std::size_t a = 0; a < n; ++a) {
    if (artificial[a])
      result.net.add_transition(std::nullopt, std::move(inputs[a]),
                                std::move(outputs[a]));
    else
      result.net.add_transition(visible_id[a], std::move(inputs[a]),
                                std::move(outputs[a]));
  }

  result.markings.initial.assign(result.net.place_count(), 0);
  result.markings.final.assign(result.net.place_count(), 0);
  result.markings.initial[result.net.source()] = 1;
  result.markings.final[result.net.sink()] = 1;
  result.diagnostics.computed = true;
  result.diagnostics.message = result.diagnostics.exhausted
                                  ? "node bound reached; partial region search returned"
                                  : "bounded integer-region search completed";
  return result;
}

}  // namespace minera
