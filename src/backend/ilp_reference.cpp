#include "minera/ilp_reference.hpp"

#include "minera/cancellation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace minera {
namespace {

constexpr double kEps = 1e-9;
constexpr double kInf = std::numeric_limits<double>::infinity();

// ---------------------------------------------------------------------------
// Small dense LP: min c.x subject to rows and lo <= x <= hi.

enum class Sense { kLe, kEq, kGe };

struct LpRow {
  std::vector<double> a;
  Sense sense = Sense::kGe;
  double b = 0;
};

enum class LpStatus { kOptimal, kInfeasible, kUnbounded };

struct LpResult {
  LpStatus status = LpStatus::kInfeasible;
  std::vector<double> x;
  double value = 0;
};

// Two-phase tableau simplex with Bland's rule. `cancelled`, when given, is
// polled every 16 pivots (one LP over a large log can run for minutes) and
// throws OperationCancelled; it never changes the pivots taken.
LpResult solve_dense(const std::vector<double>& c, const std::vector<LpRow>& input_rows,
                     const std::vector<double>& lo, const std::vector<double>& hi,
                     const std::function<bool()>* cancelled = nullptr) {
  const std::size_t n = c.size();
  for (std::size_t j = 0; j < n; ++j)
    if (lo[j] > hi[j] + kEps) return {};
  // Shift x = lo + z, z >= 0; finite upper bounds become rows.
  std::vector<LpRow> rows;
  rows.reserve(input_rows.size() + n);
  for (const auto& row : input_rows) {
    LpRow shifted = row;
    for (std::size_t j = 0; j < n; ++j) shifted.b -= row.a[j] * lo[j];
    rows.push_back(std::move(shifted));
  }
  for (std::size_t j = 0; j < n; ++j) {
    if (hi[j] == kInf) continue;
    LpRow bound;
    bound.a.assign(n, 0.0);
    bound.a[j] = 1;
    bound.sense = Sense::kLe;
    bound.b = hi[j] - lo[j];
    rows.push_back(std::move(bound));
  }
  for (auto& row : rows) {
    if (row.b < 0) {
      for (auto& value : row.a) value = -value;
      row.b = -row.b;
      if (row.sense == Sense::kLe) row.sense = Sense::kGe;
      else if (row.sense == Sense::kGe) row.sense = Sense::kLe;
    }
  }
  const std::size_t m = rows.size();
  std::size_t slack_count = 0, artificial_count = 0;
  for (const auto& row : rows) {
    if (row.sense != Sense::kEq) ++slack_count;
    if (row.sense != Sense::kLe) ++artificial_count;
  }
  const std::size_t columns = n + slack_count + artificial_count;
  const std::size_t width = columns + 1;  // last column is the right-hand side
  std::vector<double> tableau(m * width, 0.0);
  std::vector<std::size_t> basis(m);
  std::size_t slack = n, artificial = n + slack_count;
  for (std::size_t i = 0; i < m; ++i) {
    double* line = &tableau[i * width];
    for (std::size_t j = 0; j < n; ++j) line[j] = rows[i].a[j];
    line[columns] = rows[i].b;
    if (rows[i].sense == Sense::kLe) {
      line[slack] = 1;
      basis[i] = slack++;
    } else {
      if (rows[i].sense == Sense::kGe) line[slack++] = -1;
      line[artificial] = 1;
      basis[i] = artificial++;
    }
  }
  std::vector<double> cost(width, 0.0);
  auto price = [&](const std::vector<double>& objective) {
    std::fill(cost.begin(), cost.end(), 0.0);
    for (std::size_t j = 0; j < columns; ++j) cost[j] = objective[j];
    for (std::size_t i = 0; i < m; ++i) {
      const double weight = objective[basis[i]];
      if (weight == 0) continue;
      const double* line = &tableau[i * width];
      for (std::size_t j = 0; j < width; ++j) cost[j] -= weight * line[j];
    }
  };
  auto pivot = [&](std::size_t row, std::size_t column) {
    double* line = &tableau[row * width];
    const double divisor = line[column];
    for (std::size_t j = 0; j < width; ++j) line[j] /= divisor;
    for (std::size_t i = 0; i < m; ++i) {
      if (i == row) continue;
      double* other = &tableau[i * width];
      const double factor = other[column];
      if (factor == 0) continue;
      for (std::size_t j = 0; j < width; ++j) other[j] -= factor * line[j];
      other[column] = 0;
    }
    const double factor = cost[column];
    if (factor != 0) {
      for (std::size_t j = 0; j < width; ++j) cost[j] -= factor * line[j];
      cost[column] = 0;
    }
    basis[row] = column;
  };
  std::size_t pivots = 0;
  auto run = [&](std::size_t allowed) -> bool {
    for (;;) {
      if (cancelled && (++pivots & 15u) == 0)
        throw_if_cancelled(*cancelled, "ILP discovery was cancelled");
      std::size_t entering = allowed;
      for (std::size_t j = 0; j < allowed; ++j)
        if (cost[j] < -kEps) {
          entering = j;
          break;
        }
      if (entering == allowed) return true;
      std::size_t leaving = m;
      double best = kInf;
      for (std::size_t i = 0; i < m; ++i) {
        const double coefficient = tableau[i * width + entering];
        if (coefficient <= kEps) continue;
        const double ratio = tableau[i * width + columns] / coefficient;
        if (leaving == m || ratio < best - kEps ||
            (ratio <= best + kEps && basis[i] < basis[leaving])) {
          best = leaving == m ? ratio : std::min(best, ratio);
          leaving = i;
        }
      }
      if (leaving == m) return false;
      pivot(leaving, entering);
    }
  };
  if (artificial_count) {
    std::vector<double> phase_one(columns, 0.0);
    for (std::size_t j = n + slack_count; j < columns; ++j) phase_one[j] = 1;
    price(phase_one);
    run(columns);
    double infeasibility = 0;
    for (std::size_t i = 0; i < m; ++i)
      if (basis[i] >= n + slack_count) infeasibility += tableau[i * width + columns];
    if (infeasibility > 1e-7) return {};
    // Drive remaining (zero) artificials out of the basis where possible.
    for (std::size_t i = 0; i < m; ++i) {
      if (basis[i] < n + slack_count) continue;
      for (std::size_t j = 0; j < n + slack_count; ++j)
        if (std::abs(tableau[i * width + j]) > kEps) {
          pivot(i, j);
          break;
        }
    }
  }
  std::vector<double> phase_two(columns, 0.0);
  for (std::size_t j = 0; j < n; ++j) phase_two[j] = c[j];
  price(phase_two);
  if (!run(n + slack_count)) return {LpStatus::kUnbounded, {}, 0};
  LpResult result;
  result.status = LpStatus::kOptimal;
  result.x = lo;
  for (std::size_t i = 0; i < m; ++i)
    if (basis[i] < n) result.x[basis[i]] += tableau[i * width + columns];
  for (std::size_t j = 0; j < n; ++j) result.value += c[j] * result.x[j];
  return result;
}

// Solves with the equality rows and lazily added inequality rows: the
// relaxation is re-solved with every violated row until none is left.
LpResult solve_generated(const std::vector<double>& c, const std::vector<LpRow>& fixed_rows,
                         const std::vector<LpRow>& lazy_rows, const std::vector<double>& lo,
                         const std::vector<double>& hi,
                         const std::function<bool()>* cancelled = nullptr) {
  std::vector<LpRow> active = fixed_rows;
  std::vector<char> used(lazy_rows.size(), 0);
  for (;;) {
    if (cancelled) throw_if_cancelled(*cancelled, "ILP discovery was cancelled");
    auto result = solve_dense(c, active, lo, hi, cancelled);
    if (result.status != LpStatus::kOptimal) return result;
    bool added = false;
    for (std::size_t r = 0; r < lazy_rows.size(); ++r) {
      if (used[r]) continue;
      const auto& row = lazy_rows[r];
      double lhs = 0;
      for (std::size_t j = 0; j < c.size(); ++j) lhs += row.a[j] * result.x[j];
      const bool violated = (row.sense == Sense::kGe && lhs < row.b - 1e-9) ||
                            (row.sense == Sense::kLe && lhs > row.b + 1e-9) ||
                            (row.sense == Sense::kEq && std::abs(lhs - row.b) > 1e-9);
      if (violated) {
        active.push_back(row);
        used[r] = 1;
        added = true;
      }
    }
    if (!added) return result;
  }
}

// ---------------------------------------------------------------------------
// The per-pair region model.

struct RegionModel {
  std::vector<std::string> activities;  // wrapped-log activities, first appearance
  std::map<std::string, std::size_t> index;
  std::vector<double> objective;        // over (x..., y...)
  std::vector<LpRow> prefix_rows;       // >= 0
  std::vector<LpRow> final_rows;        // == 0
};

std::vector<std::vector<std::string>> wrap(const std::vector<std::vector<std::string>>& traces,
                                           const ReferenceIlpOptions& options) {
  std::vector<std::vector<std::string>> wrapped;
  wrapped.reserve(traces.size());
  for (const auto& trace : traces) {
    std::vector<std::string> extended;
    extended.reserve(trace.size() + 2);
    extended.push_back(options.artificial_start_activity);
    extended.insert(extended.end(), trace.begin(), trace.end());
    extended.push_back(options.artificial_end_activity);
    wrapped.push_back(std::move(extended));
  }
  return wrapped;
}

RegionModel build_model(const std::vector<std::vector<std::string>>& wrapped, double alpha) {
  RegionModel model;
  for (const auto& trace : wrapped)
    for (const auto& activity : trace)
      if (model.index.emplace(activity, model.activities.size()).second)
        model.activities.push_back(activity);
  const std::size_t n = model.activities.size();
  using Vector = std::vector<int>;
  // Continuation counts per prefix occurrence vector.
  std::map<Vector, std::map<std::size_t, std::size_t>> continuations;
  for (const auto& trace : wrapped) {
    Vector count(n, 0);
    for (const auto& activity : trace) {
      const std::size_t a = model.index.at(activity);
      ++continuations[count][a];
      ++count[a];
    }
  }
  std::map<Vector, std::set<std::size_t>> kept;
  for (const auto& [prefix, counts] : continuations) {
    std::size_t maximum = 0;
    for (const auto& [activity, weight] : counts) maximum = std::max(maximum, weight);
    const double threshold = (1 - alpha) * static_cast<double>(maximum);
    auto& keep = kept[prefix];
    for (const auto& [activity, weight] : counts)
      if (static_cast<double>(weight) >= threshold) keep.insert(activity);
  }
  model.objective.assign(2 * n, 0.0);
  std::set<std::vector<double>> seen_prefix, seen_final;
  std::set<std::vector<std::string>> variants;
  for (const auto& trace : wrapped) {
    if (!variants.insert(trace).second) continue;
    Vector count(n, 0);
    bool complete = true;
    for (const auto& activity : trace) {
      const std::size_t a = model.index.at(activity);
      if (!kept[count].count(a)) {
        complete = false;
        break;
      }
      std::vector<double> row(2 * n, 0.0);
      for (std::size_t j = 0; j < n; ++j) {
        row[j] = count[j];
        row[n + j] = -count[j];
      }
      row[n + a] -= 1;
      if (seen_prefix.insert(row).second) model.prefix_rows.push_back({row, Sense::kGe, 0});
      ++count[a];
      for (std::size_t j = 0; j < n; ++j) {
        model.objective[j] += count[j];
        model.objective[n + j] -= count[j];
      }
    }
    if (!complete) continue;
    std::vector<double> row(2 * n, 0.0);
    for (std::size_t j = 0; j < n; ++j) {
      row[j] = count[j];
      row[n + j] = -count[j];
    }
    if (seen_final.insert(row).second) model.final_rows.push_back({row, Sense::kEq, 0});
  }
  return model;
}

std::pair<std::vector<double>, std::vector<double>> pair_bounds(const RegionModel& model,
                                                               std::size_t producer,
                                                               std::size_t consumer) {
  const std::size_t n = model.activities.size();
  std::vector<double> lo(2 * n, 0.0), hi(2 * n, 1.0);
  lo[producer] = 1;
  lo[n + consumer] = 1;
  return {lo, hi};
}

std::size_t activity_index(const RegionModel& model, const std::string& activity) {
  const auto it = model.index.find(activity);
  if (it == model.index.end())
    throw ReferenceIlpError("ValueError", "'" + activity + "' is not in list");
  return it->second;
}

std::vector<std::pair<std::string, std::string>> causal_pairs(
    const std::vector<std::vector<std::string>>& wrapped, const ReferenceIlpOptions& options) {
  std::vector<std::pair<std::string, std::string>> pairs;
  if (options.causal_relation) {
    pairs = *options.causal_relation;
  } else {
    std::vector<std::pair<std::string, std::string>> order;
    std::set<std::pair<std::string, std::string>> dfg;
    for (const auto& trace : wrapped)
      for (std::size_t i = 0; i + 1 < trace.size(); ++i)
        if (dfg.emplace(trace[i], trace[i + 1]).second) order.emplace_back(trace[i], trace[i + 1]);
    for (const auto& [a, b] : order)
      if (a != b && !dfg.count({b, a})) pairs.emplace_back(a, b);
  }
  auto in_graph = [&](const std::string& node) {
    for (const auto& [a, b] : pairs)
      if (a == node || b == node) return true;
    return false;
  };
  for (const auto* node : {&options.artificial_start_activity, &options.artificial_end_activity})
    if (!in_graph(*node))
      throw ReferenceIlpError("NetworkXError", "The node " + *node + " is not in the digraph.");
  return pairs;
}

// ---------------------------------------------------------------------------
// Net assembly and reductions.

struct WorkingPlace {
  std::string name;
  std::set<std::string> inputs, outputs;
  int initial = 0;
  int final = 0;
};

struct WorkingNet {
  std::vector<ReferenceIlpTransition> transitions;
  std::vector<WorkingPlace> places;
};

// PM4Py's murata reduction: place p is implicit when some a_p >= 1, a_q >= 0
// make a_p*p - sum a_q*q a P-flow whose pre-weights stay within its initial
// marking k >= 0. Candidates are visited in name order; marked places stay.
bool implicit(const WorkingNet& net, std::size_t p) {
  const std::size_t places = net.places.size();
  std::vector<double> c(places, 0.0), lo(places, 0.0), hi(places, kInf);
  lo[p] = 1;
  auto sign = [&](std::size_t q) { return q == p ? 1.0 : -1.0; };
  std::vector<LpRow> rows;
  for (const auto& transition : net.transitions) {
    LpRow flow{std::vector<double>(places, 0.0), Sense::kEq, 0};
    LpRow pre{std::vector<double>(places, 0.0), Sense::kLe, 0};
    for (std::size_t q = 0; q < places; ++q) {
      const auto& place = net.places[q];
      const double post = place.inputs.count(transition.name) ? 1 : 0;
      const double consumed = place.outputs.count(transition.name) ? 1 : 0;
      flow.a[q] = sign(q) * (post - consumed);
      pre.a[q] = sign(q) * (consumed - place.initial);
    }
    rows.push_back(std::move(flow));
    rows.push_back(std::move(pre));
  }
  LpRow marking{std::vector<double>(places, 0.0), Sense::kGe, 0};
  for (std::size_t q = 0; q < places; ++q) marking.a[q] = sign(q) * net.places[q].initial;
  rows.push_back(std::move(marking));
  return solve_dense(c, rows, lo, hi).status == LpStatus::kOptimal;
}

void murata_reduction(WorkingNet& net, const std::function<bool()>& cancelled) {
  std::vector<std::string> names;
  for (const auto& place : net.places)
    if (!place.initial && !place.final) names.push_back(place.name);
  std::sort(names.begin(), names.end());
  for (const auto& name : names) {
    std::size_t p = 0;
    while (net.places[p].name != name) ++p;
    throw_if_cancelled(cancelled, "ILP discovery was cancelled");
    if (implicit(net, p)) net.places.erase(net.places.begin() + static_cast<std::ptrdiff_t>(p));
  }
}

bool is_silent(const WorkingNet& net, const std::string& name) {
  for (const auto& transition : net.transitions)
    if (transition.name == name) return !transition.label.has_value();
  return false;
}

void remove_transition(WorkingNet& net, const std::string& name) {
  net.transitions.erase(std::remove_if(net.transitions.begin(), net.transitions.end(),
                                       [&](const auto& t) { return t.name == name; }),
                        net.transitions.end());
  for (auto& place : net.places) {
    place.inputs.erase(name);
    place.outputs.erase(name);
  }
}

std::vector<std::size_t> preset(const WorkingNet& net, const std::string& transition) {
  std::vector<std::size_t> result;
  for (std::size_t p = 0; p < net.places.size(); ++p)
    if (net.places[p].outputs.count(transition)) result.push_back(p);
  return result;
}

std::vector<std::size_t> postset(const WorkingNet& net, const std::string& transition) {
  std::vector<std::size_t> result;
  for (std::size_t p = 0; p < net.places.size(); ++p)
    if (net.places[p].inputs.count(transition)) result.push_back(p);
  return result;
}

// A silent transition whose single input place feeds only it and has one
// producer hands that producer to its output places.
bool merge_single_input(WorkingNet& net, const std::string& t) {
  const auto in = preset(net, t);
  if (in.size() != 1) return false;
  const auto& place = net.places[in[0]];
  if (place.outputs.size() != 1 || place.inputs.size() != 1) return false;
  const auto producers = place.inputs;
  for (const auto q : postset(net, t))
    for (const auto& producer : producers) net.places[q].inputs.insert(producer);
  net.places.erase(net.places.begin() + static_cast<std::ptrdiff_t>(in[0]));
  remove_transition(net, t);
  return true;
}

// A silent transition whose single output place is fed only by it and has
// one consumer hands that consumer to its input places.
bool merge_single_output(WorkingNet& net, const std::string& t) {
  const auto out = postset(net, t);
  if (out.size() != 1) return false;
  const auto& place = net.places[out[0]];
  if (place.inputs.size() != 1 || place.outputs.size() != 1) return false;
  const auto consumers = place.outputs;
  for (const auto p : preset(net, t))
    for (const auto& consumer : consumers) net.places[p].outputs.insert(consumer);
  net.places.erase(net.places.begin() + static_cast<std::ptrdiff_t>(out[0]));
  remove_transition(net, t);
  return true;
}

void simple_reduction(WorkingNet& net) {
  for (bool changed = true; changed;) {
    changed = false;
    using Merge = bool (*)(WorkingNet&, const std::string&);
    for (const Merge pass : {Merge{&merge_single_input}, Merge{&merge_single_output}}) {
      std::vector<std::string> silent;
      for (const auto& transition : net.transitions)
        if (!transition.label) silent.push_back(transition.name);
      std::sort(silent.begin(), silent.end());
      for (const auto& name : silent)
        if (is_silent(net, name) && pass(net, name)) changed = true;
    }
  }
}

std::pair<std::set<std::string>, std::set<std::string>> round_place(const RegionModel& model,
                                                                    const std::vector<double>& x) {
  const std::size_t n = model.activities.size();
  std::set<std::string> inputs, outputs;
  for (std::size_t j = 0; j < n; ++j) {
    if (x[j] > 0.5) inputs.insert(model.activities[j]);
    if (x[n + j] > 0.5) outputs.insert(model.activities[j]);
  }
  return {inputs, outputs};
}

}  // namespace

std::vector<std::pair<std::string, std::string>> reference_ilp_causal_pairs(
    const std::vector<std::vector<std::string>>& traces, const ReferenceIlpOptions& options) {
  return causal_pairs(wrap(traces, options), options);
}

ReferenceIlpNet reference_ilp_net(const std::vector<std::vector<std::string>>& traces,
                                  const ReferenceIlpOptions& options) {
  const auto wrapped = wrap(traces, options);
  const auto pairs = causal_pairs(wrapped, options);
  const auto model = build_model(wrapped, options.alpha);

  WorkingNet net;
  for (const auto& activity : model.activities) {
    const bool artificial = activity == options.artificial_start_activity ||
                            activity == options.artificial_end_activity;
    net.transitions.push_back(
        {activity, artificial ? std::nullopt : std::optional<std::string>(activity)});
  }
  net.places.push_back({"source", {}, {options.artificial_start_activity}, 1, 0});
  net.places.push_back({"sink", {options.artificial_end_activity}, {}, 0, 1});

  std::set<std::pair<std::set<std::string>, std::set<std::string>>> found;
  for (const auto& [producer, consumer] : pairs) {
    const std::size_t a = activity_index(model, producer);
    const std::size_t b = activity_index(model, consumer);
    const auto [lo, hi] = pair_bounds(model, a, b);
    throw_if_cancelled(options.cancelled, "ILP discovery was cancelled");
    const auto result = solve_generated(model.objective, model.final_rows,
                                        model.prefix_rows, lo, hi, &options.cancelled);
    if (result.status != LpStatus::kOptimal) continue;
    auto place = round_place(model, result.x);
    if (!found.insert(place).second) continue;
    net.places.push_back({std::to_string(net.places.size()), place.first, place.second, 0, 0});
  }

  murata_reduction(net, options.cancelled);
  simple_reduction(net);

  ReferenceIlpNet output;
  output.transitions = net.transitions;
  for (const auto& place : net.places)
    output.places.push_back({place.name,
                             std::vector<std::string>(place.inputs.begin(), place.inputs.end()),
                             std::vector<std::string>(place.outputs.begin(), place.outputs.end())});
  return output;
}

ReferenceIlpNet reference_ilp_net(const EventLog& log, const ReferenceIlpOptions& options) {
  std::vector<std::vector<std::string>> traces;
  traces.reserve(log.traces.size());
  for (const auto& trace : log.traces) {
    std::vector<std::string> activities;
    activities.reserve(trace.events.size());
    for (const auto& event : trace.events) activities.push_back(event.activity);
    traces.push_back(std::move(activities));
  }
  return reference_ilp_net(traces, options);
}

bool reference_ilp_rounding_is_optimal(const std::vector<std::vector<std::string>>& traces,
                                       const ReferenceIlpOptions& options,
                                       const std::pair<std::string, std::string>& pair,
                                       const std::vector<std::string>& inputs,
                                       const std::vector<std::string>& outputs) {
  const auto model = build_model(wrap(traces, options), options.alpha);
  const std::size_t n = model.activities.size();
  const std::size_t a = activity_index(model, pair.first);
  const std::size_t b = activity_index(model, pair.second);
  auto [lo, hi] = pair_bounds(model, a, b);
  const auto optimum = solve_generated(model.objective, model.final_rows, model.prefix_rows, lo, hi);
  if (optimum.status != LpStatus::kOptimal) return false;
  constexpr double kSide = 1e-7;
  const std::set<std::string> in(inputs.begin(), inputs.end()), out(outputs.begin(), outputs.end());
  for (const auto& name : in)
    if (!model.index.count(name)) return false;
  for (const auto& name : out)
    if (!model.index.count(name)) return false;
  for (std::size_t j = 0; j < n; ++j) {
    const auto& name = model.activities[j];
    for (const std::size_t k : {j, n + j}) {
      const bool arc = (k < n ? in : out).count(name) > 0;
      if (arc) lo[k] = std::max(lo[k], 0.5 - kSide);
      else hi[k] = std::min(hi[k], 0.5 + kSide);
      if (lo[k] > hi[k]) return false;
    }
  }
  std::vector<LpRow> rows = model.final_rows;
  rows.push_back({model.objective, Sense::kLe,
                  optimum.value + 1e-7 * std::max(1.0, std::abs(optimum.value))});
  const std::vector<double> zero(2 * n, 0.0);
  return solve_generated(zero, rows, model.prefix_rows, lo, hi).status == LpStatus::kOptimal;
}

}  // namespace minera
