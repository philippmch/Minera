#include "minera/petri_multi_alignment.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <stdexcept>

namespace minera {
namespace {
struct Token { std::string value; bool tau = false; };
using Sequence = std::vector<Token>;
struct Node { std::vector<std::size_t> marking; std::vector<std::size_t> path; double priority = 0.0; };
struct MarkingLess { bool operator()(const std::vector<std::size_t>& a, const std::vector<std::size_t>& b) const noexcept { return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end()); } };

// Keep the pinned PM4Py distinction between prefix handling and modeled-cell
// handling: null transitions are silent, prefixes recognize exact "tau" and
// labels beginning with 'n', and modeled cells additionally recognize
// labels containing "skip".
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

std::vector<std::vector<std::string>> variants_of(const EventLog& log) {
  std::map<std::vector<std::string>, bool> u;
  for (const auto& trace : log.traces) { std::vector<std::string> v; for (const auto& e : trace.events) v.push_back(e.activity); u.emplace(std::move(v), true); }
  std::vector<std::vector<std::string>> out; for (auto& x : u) out.push_back(std::move(x.first)); return out;
}
Sequence run_sequence(const Node& n, const PetriNet& net) { Sequence out; for (auto t : n.path) { const auto& tr = net.transitions()[t]; out.push_back(tr.activity ? Token{net.activities()[*tr.activity], false} : Token{"", true}); } return out; }
Sequence variant_sequence(const std::vector<std::string>& v) { Sequence out; for (const auto& x : v) out.push_back(Token{x, false}); return out; }
bool path_less(const Node& a, const Node& b, const PetriNet& net) {
  auto key = [&net](const Node& n) { std::vector<std::string> k; for (auto t : n.path) k.push_back(net.transitions()[t].activity ? net.activities()[*net.transitions()[t].activity] : "\x01"); return k; };
  const auto ka = key(a), kb = key(b); if (ka != kb) return ka < kb; return a.path < b.path;
}
void validate(const PetriNet& n, const PetriMultiAlignmentOptions& o) {
  if (o.variant != PetriAlignmentVariant::kDiscountedAStar) throw std::invalid_argument("unsupported petri multi-alignment variant");
  if (!std::isfinite(o.exponent) || o.exponent <= 1.0) throw std::invalid_argument("petri multi-alignment exponent must be finite and > 1");
  if (!(o.max_states ? o.max_states : o.marking_limit)) throw std::invalid_argument("petri multi-alignment marking_limit must be positive");
  if (!n.place_count() || n.source() >= n.place_count() || n.sink() >= n.place_count()) throw std::invalid_argument("petri net has invalid source/sink places");
  for (const auto& t : n.transitions()) { if (t.activity && *t.activity >= n.activities().size()) throw std::invalid_argument("petri transition activity index out of range"); for (auto p : t.inputs) if (p >= n.place_count()) throw std::invalid_argument("petri input place out of range"); for (auto p : t.outputs) if (p >= n.place_count()) throw std::invalid_argument("petri output place out of range"); }
}
}

PetriMultiAlignmentResult multi_align_petri(const PetriNet& net, const EventLog& log, PetriMultiAlignmentOptions o) {
  validate(net, o); const auto variants = variants_of(log); if (variants.empty()) throw std::invalid_argument("petri multi-alignment log must not be empty");
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
  struct Compare { const PetriNet* net; bool operator()(const Node& a, const Node& b) const { if (a.priority != b.priority) return a.priority > b.priority; if (path_less(a, b, *net)) return false; if (path_less(b, a, *net)) return true; return false; } };
  std::priority_queue<Node, std::vector<Node>, Compare> queue((Compare{&net})); Node start{initial, {}, 0.0}; queue.push(start);
  std::map<std::vector<std::size_t>, std::size_t, MarkingLess> closed; PetriMultiAlignmentDiagnostics d;
  bool have = false; Node best; double best_score = std::numeric_limits<double>::infinity();
  while (!queue.empty()) {
    Node node = queue.top(); queue.pop();
    if (have && node.priority > best_score) break;
    if (node.marking == final) {
      const auto run = run_sequence(node, net); double score = 0.0; for (const auto& v : variants) score = std::max(score, discounted_distance(run, variant_sequence(v), o.exponent));
      if (!have || score < best_score || (score == best_score && path_less(node, best, net))) { have = true; best_score = score; best = node; }
      continue;
    }
    ++closed[node.marking]; ++d.visited_states;
    for (std::size_t ti = 0; ti < net.transitions().size(); ++ti) {
      const auto& t = net.transitions()[ti]; std::vector<std::size_t> req(net.place_count(), 0); for (auto p : t.inputs) ++req[p]; bool enabled = true; for (std::size_t p = 0; p < net.place_count(); ++p) if (node.marking[p] < req[p]) { enabled = false; break; } if (!enabled) continue; ++d.traversed_arcs;
      auto marking = node.marking; for (std::size_t p = 0; p < net.place_count(); ++p) marking[p] -= req[p]; for (auto p : t.outputs) { if (marking[p] == std::numeric_limits<std::size_t>::max()) throw std::overflow_error("petri marking token count overflow"); ++marking[p]; }
      const auto closed_child = closed.find(marking);
      if (closed_child != closed.end() && closed_child->second > limit) { d.exhausted = true; continue; }
      Node child{std::move(marking), node.path, 0.0}; child.path.push_back(ti); child.priority = bound(child); if (child.priority == node.priority) continue; queue.push(std::move(child)); ++d.queued_states;
    }
  }
  if (!have) throw std::runtime_error(d.exhausted ? "petri multi-alignment state limit reached before final marking" : "petri net final marking is unreachable");
  PetriMultiAlignmentResult r; r.transitions = best.path; for (auto t : best.path) if (net.transitions()[t].activity) r.activities.push_back(net.activities()[*net.transitions()[t].activity]); r.cost = r.objective = best_score;
  r.max_distance_to_log = 0.0; auto ordinary = [](const std::vector<std::string>& a, const std::vector<std::string>& b) { std::vector<std::size_t> p(b.size() + 1), c(b.size() + 1); for (std::size_t j = 0; j <= b.size(); ++j) p[j] = j; for (std::size_t i = 1; i <= a.size(); ++i) { c[0] = p[0] + (silent_run_label(a[i - 1]) ? 0u : 1u); for (std::size_t j = 1; j <= b.size(); ++j) c[j] = std::min({p[j] + (silent_run_label(a[i - 1]) ? 0u : 1u), c[j - 1] + 1, p[j - 1] + (a[i - 1] == b[j - 1] ? 0u : 2u)}); p.swap(c); } return p[b.size()]; };
  for (const auto& v : variants) r.max_distance_to_log = std::max(r.max_distance_to_log, static_cast<double>(ordinary(r.activities, v))); r.diagnostics = d; return r;
}
}  // namespace minera
