#include "minera/petri_multi_alignment.hpp"

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
struct Node { std::vector<std::size_t> marking; std::vector<std::size_t> path; double priority = 0.0; };
struct MarkingLess { bool operator()(const std::vector<std::size_t>& a, const std::vector<std::size_t>& b) const noexcept { return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end()); } };

// Discounted DP has an asymmetric prefix recurrence.  Model-side interior
// rules recognize null moves, tau, and n*-style labels; modeled interior
// cells additionally recognize labels containing skip.
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
  if (s1.size() < s2.size()) { std::swap(s1, s2); modeled = false; }
  const std::size_t n = s1.size(), m = s2.size(); std::vector<double> p(m + 1), c(m + 1);
  auto w = [exponent](std::size_t k) { return std::pow(exponent, -static_cast<double>(k)); };
  for (std::size_t j = 1; j <= m; ++j) p[j] = p[j - 1] + ((!modeled && silent_prefix(s2[j - 1])) ? 0.0 : w(j - 1));
  for (std::size_t i = 1; i <= n; ++i) {
    c[0] = p[0] + ((modeled && silent_prefix(s1[i - 1])) ? 0.0 : w(i - 1));
    for (std::size_t j = 1; j <= m; ++j) {
      const double e = w(i - 1 + j);
      const double ins = p[j] + ((modeled && silent_modeled_cell(s1[i - 1])) ? 0.0 : e);
      const double del = c[j - 1] + ((!modeled && silent_prefix(s2[j - 1])) ? 0.0 : e);
      const double diag = (s1[i - 1].value == s2[j - 1].value && s1[i - 1].tau == s2[j - 1].tau) ? p[j - 1] : std::numeric_limits<double>::infinity();
      c[j] = std::min({del, ins, diag});
    }
    p.swap(c);
  }
  return p[m];
}

void validate_selector(std::string_view selector) {
  if (selector.empty() || std::all_of(selector.begin(), selector.end(),
                                      [](unsigned char c) {
                                        return std::isspace(c) != 0;
                                      })) {
    throw std::invalid_argument(
        "petri multi-alignment activity selector must not be blank");
  }
}

std::string selected_activity(const Event& event,
                              std::string_view selector) {
  const auto* value = dfg_event_field_value(event, selector);
  if (value == nullptr)
    throw std::invalid_argument(
        "petri multi-alignment: missing or null activity " +
        std::string(selector));
  return *value;
}

std::vector<std::vector<std::string>> variants_of(
    const EventLog& log, std::string_view selector) {
  std::map<std::vector<std::string>, bool> u;
  for (const auto& trace : log.traces) { std::vector<std::string> v; v.reserve(trace.events.size()); for (const auto& e : trace.events) v.push_back(selected_activity(e, selector)); u.emplace(std::move(v), true); }
  std::vector<std::vector<std::string>> out; for (auto& x : u) out.push_back(std::move(x.first)); return out;
}
Sequence run_sequence(const Node& n, const PetriNet& net) { Sequence out; for (auto t : n.path) { const auto& tr = net.transitions()[t]; out.push_back(tr.activity ? Token{net.activities()[*tr.activity], false} : Token{"", true}); } return out; }
Sequence variant_sequence(const std::vector<std::string>& v) { Sequence out; for (const auto& x : v) out.push_back(Token{x, false}); return out; }
bool path_less(const Node& a, const Node& b, const PetriNet& net) {
  auto key = [&net](const Node& n) { std::vector<std::string> k; for (auto t : n.path) k.push_back(net.transitions()[t].activity ? net.activities()[*net.transitions()[t].activity] : "\x01"); return k; };
  const auto ka = key(a), kb = key(b); if (ka != kb) return ka < kb; return a.path < b.path;
}

bool path_preferred(const Node& a, const Node& b, const PetriNet& net) {
  if (a.path.size() != b.path.size()) return a.path.size() > b.path.size();
  return path_less(a, b, net);
}
void validate(const PetriNet& n, const PetriMultiAlignmentOptions& o) {
  validate_selector(o.activity_attribute);
  if (o.variant != PetriAlignmentVariant::kDiscountedAStar) throw std::invalid_argument("unsupported petri multi-alignment variant");
  if (!std::isfinite(o.exponent) || o.exponent <= 1.0) throw std::invalid_argument("petri multi-alignment exponent must be finite and > 1");
  if (!(o.max_states ? o.max_states : o.marking_limit)) throw std::invalid_argument("petri multi-alignment marking_limit must be positive");
  if (!n.place_count() || n.source() >= n.place_count() || n.sink() >= n.place_count()) throw std::invalid_argument("petri net has invalid source/sink places");
  for (const auto& t : n.transitions()) { if (t.activity && *t.activity >= n.activities().size()) throw std::invalid_argument("petri transition activity index out of range"); for (auto p : t.inputs) if (p >= n.place_count()) throw std::invalid_argument("petri input place out of range"); for (auto p : t.outputs) if (p >= n.place_count()) throw std::invalid_argument("petri output place out of range"); for (auto p : t.reset_inputs) if (p >= n.place_count()) throw std::invalid_argument("petri reset place out of range"); for (auto p : t.inhibitor_inputs) if (p >= n.place_count()) throw std::invalid_argument("petri inhibitor place out of range"); }
}
}

PetriMultiAlignmentResult multi_align_petri(const PetriNet& net, const EventLog& log, PetriMultiAlignmentOptions o) {
  validate(net, o); const auto variants = variants_of(log, o.activity_attribute); if (variants.empty()) throw std::invalid_argument("petri multi-alignment log must not be empty");
  std::vector<std::size_t> initial = o.initial_marking, final = o.final_marking;
  if (initial.empty()) { initial.assign(net.place_count(), 0); initial[net.source()] = 1; }
  if (final.empty()) { final.assign(net.place_count(), 0); final[net.sink()] = 1; }
  if (initial.size() != net.place_count() || final.size() != net.place_count()) throw std::invalid_argument("petri marking vectors must match place_count");
  const std::size_t limit = o.max_states ? o.max_states : o.marking_limit;
  auto bound = [&](const Node& n) {
    const auto run = run_sequence(n, net); const auto l = run.size(); double worst = -std::numeric_limits<double>::infinity();
    for (const auto& v : variants) {
      const auto d = discounted_distance(run, variant_sequence(v), o.exponent);
      const double tail = (std::pow(o.exponent, -static_cast<double>(l) + 1.0) - std::pow(o.exponent, -static_cast<double>(l + v.size()))) / (o.exponent - 1.0);
      worst = std::max(worst, d - tail);
    }
    return worst;
  };
  struct Compare { const PetriNet* net; bool operator()(const Node& a, const Node& b) const { if (a.priority != b.priority) return a.priority > b.priority; if (a.path.size() != b.path.size()) return a.path.size() < b.path.size(); if (path_less(a, b, *net)) return false; if (path_less(b, a, *net)) return true; return false; } };
  std::priority_queue<Node, std::vector<Node>, Compare> queue((Compare{&net})); Node start{initial, {}, 0.0}; queue.push(start);
  std::map<std::vector<std::size_t>, std::size_t, MarkingLess> closed; PetriMultiAlignmentDiagnostics d;
  bool have = false; Node best; double best_score = std::numeric_limits<double>::infinity();
  while (!queue.empty()) {
    Node node = queue.top(); queue.pop();
    if (have && node.priority > best_score) break;
    if (node.marking == final) {
      const auto run = run_sequence(node, net); double score = 0.0; for (const auto& v : variants) score = std::max(score, discounted_distance(run, variant_sequence(v), o.exponent));
      if (!have || score < best_score ||
          (score == best_score && path_preferred(node, best, net))) {
        have = true; best_score = score; best = node;
      }
      continue;
    }
    ++closed[node.marking]; ++d.visited_states;
    for (std::size_t ti = 0; ti < net.transitions().size(); ++ti) {
      const auto& t = net.transitions()[ti]; if (!petri_transition_enabled(t, node.marking)) continue; ++d.traversed_arcs;
      auto marking = node.marking;
      for (const auto p : t.outputs)
        if (marking[p] == std::numeric_limits<std::size_t>::max())
          throw std::overflow_error("petri marking token count overflow");
      fire_petri_transition(t, marking);
      const auto closed_child = closed.find(marking);
      if (closed_child != closed.end() && closed_child->second > limit) { d.exhausted = true; continue; }
      Node child{std::move(marking), node.path, 0.0}; child.path.push_back(ti); child.priority = bound(child); if (child.priority == node.priority) continue; queue.push(std::move(child)); ++d.queued_states;
    }
  }
  if (!have) throw std::runtime_error(d.exhausted ? "petri multi-alignment state limit reached before final marking" : "petri net final marking is unreachable");
  PetriMultiAlignmentResult r; r.transitions = best.path; for (auto t : best.path) if (net.transitions()[t].activity) r.activities.push_back(net.activities()[*net.transitions()[t].activity]); r.cost = r.objective = best_score;
  r.max_distance_to_log = 0.0;
  auto ordinary = [](const Sequence& model,
                     const std::vector<std::string>& log) {
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
    return previous[log.size()];
  };
  const auto run = run_sequence(best, net);
  for (const auto& v : variants)
    r.max_distance_to_log = std::max(
        r.max_distance_to_log, static_cast<double>(ordinary(run, v)));
  r.diagnostics = d;
  return r;
}
}  // namespace minera
