#include "minera/petri_anti_alignment.hpp"

#include "minera/dfg_variants.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <stdexcept>
#include <string_view>

namespace minera {
namespace {
struct Token { std::string value; bool tau = false; };
using Sequence = std::vector<Token>;

struct Node {
  std::vector<std::size_t> marking;
  std::vector<std::size_t> path;
  double priority = 0.0;
};
struct MarkingLess {
  bool operator()(const std::vector<std::size_t>& a,
                  const std::vector<std::size_t>& b) const noexcept {
    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
  }
};

// Discounted DP has an asymmetric prefix recurrence.  Its model-side rules
// recognize null moves, tau, and n*-style labels; modeled interior cells
// additionally recognize labels containing skip.
bool silent_prefix(const Token& token) {
  return token.tau || token.value == "tau" ||
         (!token.value.empty() && token.value.front() == 'n');
}
bool silent_modeled_cell(const Token& token) {
  return silent_prefix(token) || token.value.find("skip") != std::string::npos;
}
bool silent_run_label(const std::string& value) {
  return value.find("tau") != std::string::npos ||
         (!value.empty() && value.front() == 'n') ||
         value.find("skip") != std::string::npos;
}

bool silent_run_label(const Token& token) {
  return token.tau || silent_run_label(token.value);
}

double discounted_distance(Sequence s1, Sequence s2, double exponent) {
  bool modeled = true;
  if (s1.size() < s2.size()) {
    std::swap(s1, s2);
    modeled = false;
  }
  const std::size_t n = s1.size(), m = s2.size();
  std::vector<double> previous(m + 1, 0.0), current(m + 1, 0.0);
  auto weight = [exponent](std::size_t k) { return std::pow(exponent, -static_cast<double>(k)); };
  for (std::size_t j = 1; j <= m; ++j) {
    // In the unmodeled orientation, silent model elements are free along the
    // initial row, matching the asymmetric discounted recurrence.
    previous[j] = previous[j - 1] + ((!modeled && silent_prefix(s2[j - 1])) ? 0.0 : weight(j - 1));
  }
  for (std::size_t i = 1; i <= n; ++i) {
    // Silent modeled moves are free along the initial column in this
    // discounted recurrence; the ordinary diagnostic metric has separate
    // raw-index borders below.
    current[0] = previous[0] + ((modeled && silent_prefix(s1[i - 1])) ? 0.0 : weight(i - 1));
    for (std::size_t j = 1; j <= m; ++j) {
      const double edit = weight(i - 1 + j);
      const double insertion = previous[j] + ((modeled && silent_modeled_cell(s1[i - 1])) ? 0.0 : edit);
      const double deletion = current[j - 1] + ((!modeled && silent_prefix(s2[j - 1])) ? 0.0 : edit);
      double diagonal = std::numeric_limits<double>::infinity();
      if (s1[i - 1].value == s2[j - 1].value && s1[i - 1].tau == s2[j - 1].tau)
        diagonal = previous[j - 1];
      current[j] = std::min({deletion, insertion, diagonal});
    }
    previous.swap(current);
  }
  return previous[m];
}

double ordinary_insert_delete(const Sequence& model,
                              const std::vector<std::string>& log) {
  // This is the insertion/deletion distance used by the precision
  // diagnostic.  A silent model element can be deleted for free in an
  // interior cell, but the first column remains an ordinary prefix.
  std::vector<std::size_t> previous(log.size() + 1), current(log.size() + 1);
  for (std::size_t j = 0; j <= log.size(); ++j) previous[j] = j;
  for (std::size_t i = 1; i <= model.size(); ++i) {
    current[0] = previous[0] + 1;
    const auto& token = model[i - 1];
    const bool silent = silent_run_label(token);
    for (std::size_t j = 1; j <= log.size(); ++j) {
      if (silent) {
        current[j] = std::min(previous[j], current[j - 1] + 1);
      } else if (token.value == log[j - 1]) {
        current[j] = std::min({previous[j] + 1, current[j - 1] + 1,
                               previous[j - 1]});
      } else {
        current[j] = std::min(previous[j] + 1, current[j - 1] + 1);
      }
    }
    previous.swap(current);
  }
  return static_cast<double>(previous[log.size()]);
}

void validate_selector(std::string_view selector) {
  if (selector.empty() || std::all_of(selector.begin(), selector.end(),
                                      [](unsigned char c) {
                                        return std::isspace(c) != 0;
                                      })) {
    throw std::invalid_argument(
        "petri anti-alignment activity selector must not be blank");
  }
}

std::string selected_activity(const Event& event,
                              std::string_view selector) {
  const auto* value = dfg_event_field_value(event, selector);
  if (value == nullptr)
    throw std::invalid_argument(
        "petri anti-alignment: missing or null activity " +
        std::string(selector));
  return *value;
}

std::vector<std::vector<std::string>> variants_of(
    const EventLog& log, std::string_view selector) {
  std::map<std::vector<std::string>, bool> unique;
  for (const auto& trace : log.traces) {
    std::vector<std::string> v;
    v.reserve(trace.events.size());
    for (const auto& event : trace.events)
      v.push_back(selected_activity(event, selector));
    unique.emplace(std::move(v), true);
  }
  std::vector<std::vector<std::string>> out;
  for (auto& item : unique) out.push_back(std::move(item.first));
  return out;
}

Sequence run_sequence(const Node& node, const PetriNet& net) {
  Sequence out;
  for (const auto transition : node.path) {
    const auto& t = net.transitions()[transition];
    if (t.activity) out.push_back(Token{net.activities()[*t.activity], false});
    else out.push_back(Token{"", true});
  }
  return out;
}
Sequence variant_sequence(const std::vector<std::string>& v) {
  Sequence out; for (const auto& s : v) out.push_back(Token{s, false}); return out;
}

bool path_less(const Node& a, const Node& b, const PetriNet& net) {
  auto key = [&net](const Node& n) {
    std::vector<std::string> k;
    for (auto t : n.path) k.push_back(net.transitions()[t].activity ? net.activities()[*net.transitions()[t].activity] : "\x01");
    return k;
  };
  const auto ka = key(a), kb = key(b);
  if (ka != kb) return ka < kb;
  return a.path < b.path;
}

bool path_preferred(const Node& a, const Node& b, const PetriNet& net) {
  if (a.path.size() != b.path.size()) return a.path.size() > b.path.size();
  return path_less(a, b, net);
}

void validate(const PetriNet& net, const PetriAntiAlignmentOptions& o) {
  validate_selector(o.activity_attribute);
  if (o.variant != PetriAlignmentVariant::kDiscountedAStar) throw std::invalid_argument("unsupported petri anti-alignment variant");
  if (!std::isfinite(o.exponent) || o.exponent <= 1.0) throw std::invalid_argument("petri anti-alignment exponent must be finite and > 1");
  if (!std::isfinite(o.epsilon) || o.epsilon < 0.0) throw std::invalid_argument("petri anti-alignment epsilon must be finite and non-negative");
  if ((o.max_states ? o.max_states : o.marking_limit) == 0) throw std::invalid_argument("petri anti-alignment marking_limit must be positive");
  if (net.place_count() == 0 || net.source() >= net.place_count() || net.sink() >= net.place_count()) throw std::invalid_argument("petri net has invalid source/sink places");
  for (const auto& t : net.transitions()) {
    if (t.activity && *t.activity >= net.activities().size()) throw std::invalid_argument("petri transition activity index out of range");
    for (auto p : t.inputs) if (p >= net.place_count()) throw std::invalid_argument("petri input place out of range");
    for (auto p : t.outputs) if (p >= net.place_count()) throw std::invalid_argument("petri output place out of range");
    for (auto p : t.reset_inputs) if (p >= net.place_count()) throw std::invalid_argument("petri reset place out of range");
    for (auto p : t.inhibitor_inputs) if (p >= net.place_count()) throw std::invalid_argument("petri inhibitor place out of range");
  }
}
}

PetriAntiAlignmentResult anti_align_petri(const PetriNet& net, const EventLog& log, PetriAntiAlignmentOptions options) {
  validate(net, options);
  const auto variants = variants_of(log, options.activity_attribute);
  if (variants.empty()) throw std::invalid_argument("petri anti-alignment log must not be empty");
  std::vector<std::size_t> initial = options.initial_marking, final = options.final_marking;
  if (initial.empty()) { initial.assign(net.place_count(), 0); initial[net.source()] = 1; }
  if (final.empty()) { final.assign(net.place_count(), 0); final[net.sink()] = 1; }
  if (initial.size() != net.place_count() || final.size() != net.place_count()) throw std::invalid_argument("petri marking vectors must match place_count");
  const std::size_t limit = options.max_states ? options.max_states : options.marking_limit;
  auto bound = [&](const Node& n) {
    const auto run = run_sequence(n, net); const double divisor = std::pow(1.0 + options.epsilon, static_cast<double>(run.size()));
    double best = std::numeric_limits<double>::infinity();
    for (const auto& v : variants) {
      const auto d = discounted_distance(run, variant_sequence(v), options.exponent);
      // The tail starts after the complete DP matrix (model + variant
      // coordinates), while the normalization divisor uses model length.
      const double tail = std::pow(options.exponent, -static_cast<double>(run.size() + v.size())) / (options.exponent - 1.0);
      best = std::min(best, (d + tail) / divisor);
    }
    return -best;
  };
  struct Compare {
    const PetriNet* net; bool operator()(const Node& a, const Node& b) const {
      if (a.priority != b.priority) return a.priority > b.priority;
      if (a.path.size() != b.path.size()) return a.path.size() < b.path.size();
      if (path_less(a, b, *net)) return false;
      if (path_less(b, a, *net)) return true;
      return false;
    }
  };
  std::priority_queue<Node, std::vector<Node>, Compare> queue((Compare{&net}));
  // The prefix queue is seeded with g=0; bounds are computed after each
  // fired transition (and at final markings without the tail fraction).
  Node start{initial, {}, 0.0}; queue.push(start);
  std::map<std::vector<std::size_t>, std::size_t, MarkingLess> closed;
  PetriAntiAlignmentDiagnostics diagnostics;
  bool have_best = false; Node best_node; double best_score = -std::numeric_limits<double>::infinity();
  while (!queue.empty()) {
    Node node = queue.top(); queue.pop();
    // Priority is the negative optimistic score; once its bound is no
    // better than the incumbent (i.e. -priority <= best_score), stop.
    if (have_best && node.priority > -best_score) break;
    if (node.marking == final) {
      const auto run = run_sequence(node, net); const double div = std::pow(1.0 + options.epsilon, static_cast<double>(run.size()));
      double score = std::numeric_limits<double>::infinity();
      for (const auto& v : variants) score = std::min(score, discounted_distance(run, variant_sequence(v), options.exponent) / div);
      if (!have_best || score > best_score ||
          (score == best_score && path_preferred(node, best_node, net))) {
        have_best = true; best_score = score; best_node = node;
      }
      continue;
    }
    ++closed[node.marking]; ++diagnostics.visited_states;
    for (std::size_t ti = 0; ti < net.transitions().size(); ++ti) {
      const auto& t = net.transitions()[ti];
      if (!petri_transition_enabled(t, node.marking)) continue;
      ++diagnostics.traversed_arcs;
      auto marking = node.marking;
      for (auto p : t.outputs) {
        if (marking[p] == std::numeric_limits<std::size_t>::max())
          throw std::overflow_error("petri marking token count overflow");
      }
      fire_petri_transition(t, marking);
      const auto closed_child = closed.find(marking);
      if (closed_child != closed.end() && closed_child->second > limit) { diagnostics.exhausted = true; continue; }
      Node child{std::move(marking), node.path, 0.0}; child.path.push_back(ti); child.priority = bound(child);
      if (child.priority == node.priority) continue;
      queue.push(std::move(child)); ++diagnostics.queued_states;
    }
  }
  if (!have_best) throw std::runtime_error(diagnostics.exhausted ? "petri anti-alignment state limit reached before final marking" : "petri net final marking is unreachable");
  PetriAntiAlignmentResult result; result.transitions = best_node.path;
  for (auto t : best_node.path) if (net.transitions()[t].activity) result.activities.push_back(net.activities()[*net.transitions()[t].activity]);
  result.cost = result.objective = -best_score;
  double nearest = std::numeric_limits<double>::infinity();
  const auto run = run_sequence(best_node, net);
  for (const auto& v : variants) {
    const auto d = ordinary_insert_delete(run, v);
    const auto den = best_node.path.size() + v.size();
    const double n = den == 0 ? 0.0 : d / static_cast<double>(den);
    // Precision is discounted by the complete transition run, including
    // silent moves; only their interior deletion contribution is free.
    nearest = std::min(nearest, n / std::pow(1.0 + options.epsilon, static_cast<double>(best_node.path.size())));
  }
  result.precision = std::clamp(1.0 - nearest, 0.0, 1.0); result.diagnostics = diagnostics; return result;
}
}  // namespace minera
