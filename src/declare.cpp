#include "minera/declare.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <unordered_map>

namespace minera {
namespace {

const std::vector<std::string>& families() {
  static const std::vector<std::string> value = {
      kDeclareExistence, kDeclareExactlyOne, kDeclareInit,
      kDeclareRespondedExistence, kDeclareResponse, kDeclarePrecedence,
      kDeclareSuccession, kDeclareAltResponse, kDeclareAltPrecedence,
      kDeclareAltSuccession, kDeclareChainResponse, kDeclareChainPrecedence,
      kDeclareChainSuccession, kDeclareAbsence, kDeclareCoexistence,
      kDeclareNonCoexistence, kDeclareNonSuccession,
      kDeclareNonChainSuccession};
  return value;
}

struct CandidateKey {
  std::string family;
  std::string first;
  std::string second;
  bool binary = false;

  friend bool operator<(const CandidateKey& lhs, const CandidateKey& rhs) {
    return std::tie(lhs.family, lhs.first, lhs.second, lhs.binary) <
           std::tie(rhs.family, rhs.first, rhs.second, rhs.binary);
  }
  friend bool operator==(const CandidateKey& lhs, const CandidateKey& rhs) {
    return lhs.family == rhs.family && lhs.first == rhs.first &&
           lhs.second == rhs.second && lhs.binary == rhs.binary;
  }
};

CandidateKey key(const std::string& family, const std::string& a,
                 const std::string& b = {}, bool binary = false) {
  return CandidateKey{family, a, b, binary};
}

bool has(const std::set<std::string>& allowed, const char* family) {
  return allowed.find(family) != allowed.end();
}

bool in(const std::unordered_map<std::string, std::vector<int>>& idx,
        const std::string& a) {
  const auto it = idx.find(a);
  return it != idx.end() && !it->second.empty();
}

bool alt_response(const std::vector<std::string>& trace,
                  const std::unordered_map<std::string, std::vector<int>>& idx,
                  const std::string& a, const std::string& b) {
  const auto ia = idx.find(a);
  if (ia == idx.end()) return true;
  const auto ib = idx.find(b);
  if (ib == idx.end()) return false;
  std::size_t p = 0;
  for (std::size_t i = 0; i < ia->second.size(); ++i) {
    const int next = i + 1 < ia->second.size() ? ia->second[i + 1]
                                                : static_cast<int>(trace.size());
    while (p < ib->second.size() && ib->second[p] <= ia->second[i]) ++p;
    if (p == ib->second.size() || ib->second[p] >= next) return false;
  }
  return true;
}

bool alt_precedence(const std::unordered_map<std::string, std::vector<int>>& idx,
                    const std::string& a, const std::string& b) {
  const auto ib = idx.find(b);
  if (ib == idx.end()) return true;
  const auto ia = idx.find(a);
  if (ia == idx.end()) return false;
  std::size_t p = 0;
  int previous = -1;
  for (const int bi : ib->second) {
    while (p < ia->second.size() && ia->second[p] <= previous) ++p;
    if (p == ia->second.size() || ia->second[p] >= bi) return false;
    previous = bi;
  }
  return true;
}

bool chain_response(const std::vector<std::string>& trace,
                    const std::unordered_map<std::string, std::vector<int>>& idx,
                    const std::string& a, const std::string& b) {
  const auto ia = idx.find(a);
  if (ia == idx.end()) return true;
  for (const int i : ia->second) {
    if (i + 1 >= static_cast<int>(trace.size()) || trace[i + 1] != b) return false;
  }
  return true;
}

bool chain_precedence(const std::vector<std::string>& trace,
                      const std::unordered_map<std::string, std::vector<int>>& idx,
                      const std::string& a, const std::string& b) {
  const auto ib = idx.find(b);
  if (ib == idx.end()) return true;
  for (const int i : ib->second) {
    if (i == 0 || trace[i - 1] != a) return false;
  }
  return true;
}

using Statuses = std::map<CandidateKey, int>;

Statuses evaluate(const std::vector<std::string>& trace,
                  const std::set<std::string>& activities,
                  const std::set<std::string>& allowed) {
  Statuses result;
  std::unordered_map<std::string, std::vector<int>> idx;
  for (int i = 0; i < static_cast<int>(trace.size()); ++i) idx[trace[i]].push_back(i);

  auto put = [&](const char* f, const std::string& a, int value,
                 const std::string& b = {}, bool binary = false) {
    result[key(f, a, b, binary)] = value;
  };
  auto pairs = [&](const char* f, auto&& fn) {
    if (!has(allowed, f)) return;
    for (const auto& a : activities) {
      for (const auto& b : activities) {
        if (a == b) continue;
        auto value = fn(a, b);
        if (value.has_value()) put(f, a, *value, b, true);
      }
    }
    // Step 2 in PM4Py zero-fills all ordered keys of an allowed family.
    for (const auto& a : activities)
      for (const auto& b : activities)
        if (a != b) {
          const auto k = key(f, a, b, true);
          if (!result.contains(k)) result[k] = 0;
        }
  };

  if (has(allowed, kDeclareExistence))
    for (const auto& a : activities) put(kDeclareExistence, a, in(idx, a) ? 1 : -1);
  if (has(allowed, kDeclareExactlyOne))
    for (const auto& a : activities)
      put(kDeclareExactlyOne, a,
          idx.contains(a) && idx.at(a).size() == 1 ? 1 : -1);
  if (has(allowed, kDeclareInit))
    for (const auto& a : activities)
      put(kDeclareInit, a, !trace.empty() && trace.front() == a ? 1 : -1);

  pairs(kDeclareRespondedExistence, [&](const std::string& a, const std::string& b)
                                  -> std::optional<int> {
    if (!in(idx, a)) return std::nullopt;
    return in(idx, b) ? 1 : -1;
  });
  pairs(kDeclareResponse, [&](const std::string& a, const std::string& b)
                         -> std::optional<int> {
    if (!in(idx, a)) return std::nullopt;
    return !in(idx, b) || idx.at(a).back() >= idx.at(b).back() ? -1 : 1;
  });
  pairs(kDeclarePrecedence, [&](const std::string& a, const std::string& b)
                           -> std::optional<int> {
    if (!in(idx, b)) return std::nullopt;
    return !in(idx, a) || idx.at(a).front() >= idx.at(b).front() ? -1 : 1;
  });
  pairs(kDeclareAltResponse, [&](const std::string& a, const std::string& b)
                              -> std::optional<int> {
    if (!in(idx, a)) return std::nullopt;
    return alt_response(trace, idx, a, b) ? 1 : -1;
  });
  pairs(kDeclareChainResponse, [&](const std::string& a, const std::string& b)
                                -> std::optional<int> {
    if (!in(idx, a)) return std::nullopt;
    return chain_response(trace, idx, a, b) ? 1 : -1;
  });
  pairs(kDeclareAltPrecedence, [&](const std::string& a, const std::string& b)
                                -> std::optional<int> {
    if (!in(idx, b)) return std::nullopt;
    return alt_precedence(idx, a, b) ? 1 : -1;
  });
  pairs(kDeclareChainPrecedence, [&](const std::string& a, const std::string& b)
                                  -> std::optional<int> {
    if (!in(idx, b)) return std::nullopt;
    return chain_precedence(trace, idx, a, b) ? 1 : -1;
  });

  auto compound = [&](const char* f, const char* x, const char* y,
                      auto fn) {
    if (!has(allowed, f) || !has(allowed, x) || !has(allowed, y)) return;
    for (const auto& a : activities)
      for (const auto& b : activities)
        if (a != b) put(f, a, fn(a, b), b, true);
  };
  compound(kDeclareSuccession, kDeclareResponse, kDeclarePrecedence,
           [&](const std::string& a, const std::string& b) {
             const int r = result[key(kDeclareResponse, a, b, true)];
             const int p = result[key(kDeclarePrecedence, a, b, true)];
             return std::min(r, p);
           });
  compound(kDeclareAltSuccession, kDeclareAltResponse, kDeclareAltPrecedence,
           [&](const std::string& a, const std::string& b) {
             return std::min(result[key(kDeclareAltResponse, a, b, true)],
                             result[key(kDeclareAltPrecedence, a, b, true)]);
           });
  compound(kDeclareChainSuccession, kDeclareChainResponse, kDeclareChainPrecedence,
           [&](const std::string& a, const std::string& b) {
             return std::min(result[key(kDeclareChainResponse, a, b, true)],
                             result[key(kDeclareChainPrecedence, a, b, true)]);
           });
  compound(kDeclareCoexistence, kDeclareRespondedExistence, kDeclareRespondedExistence,
           [&](const std::string& a, const std::string& b) {
             return std::min(result[key(kDeclareRespondedExistence, a, b, true)],
                             result[key(kDeclareRespondedExistence, b, a, true)]);
           });
  if (has(allowed, kDeclareAbsence) && has(allowed, kDeclareExistence))
    for (const auto& a : activities) put(kDeclareAbsence, a,
                                         -result[key(kDeclareExistence, a)]);
  compound(kDeclareNonCoexistence, kDeclareCoexistence, kDeclareRespondedExistence,
           [&](const std::string& a, const std::string& b) {
             return -result[key(kDeclareCoexistence, a, b, true)];
           });
  if (has(allowed, kDeclareNonSuccession) && has(allowed, kDeclareSuccession) &&
      has(allowed, kDeclareResponse) && has(allowed, kDeclarePrecedence))
    for (const auto& a : activities)
      for (const auto& b : activities)
        if (a != b) put(kDeclareNonSuccession, a,
                        -result[key(kDeclareSuccession, a, b, true)], b, true);
  if (has(allowed, kDeclareNonChainSuccession) &&
      has(allowed, kDeclareChainSuccession) && has(allowed, kDeclareChainResponse) &&
      has(allowed, kDeclareChainPrecedence))
    for (const auto& a : activities)
      for (const auto& b : activities)
        if (a != b) put(kDeclareNonChainSuccession, a,
                        -result[key(kDeclareChainSuccession, a, b, true)], b, true);
  return result;
}

std::set<std::string> default_activities(const EventLog& log) {
  std::set<std::string> result;
  for (const auto& trace : log.traces)
    for (const auto& event : trace.events) result.insert(event.activity);
  return result;
}

std::set<std::string> to_set(const std::vector<std::string>& values) {
  return std::set<std::string>(values.begin(), values.end());
}

std::vector<std::string> project(const Trace& trace,
                                 const std::set<std::string>& activities) {
  std::vector<std::string> result;
  for (const auto& e : trace.events)
    if (activities.contains(e.activity)) result.push_back(e.activity);
  return result;
}

std::pair<int, int> stats_for(const std::vector<Statuses>& rows,
                              const std::vector<int>& weights,
                              const CandidateKey& candidate) {
  int support = 0, confidence = 0;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const int status = rows[i].contains(candidate) ? rows[i].at(candidate) : 0;
    if (status != 0) support += weights[i];
    if (status == 1) confidence += weights[i];
  }
  return {support, confidence};
}

}  // namespace

DeclareModel::DeclareModel() {
  for (const auto& family : families()) rules.emplace(family, std::vector<DeclareConstraint>{});
}

void DeclareModel::add(std::string family, DeclareConstraint constraint) {
  rules[std::move(family)].push_back(std::move(constraint));
}

std::size_t DeclareModel::size() const noexcept {
  std::size_t n = 0;
  for (const auto& [_, values] : rules) n += values.size();
  return n;
}

DeclareModel discover_declare(const EventLog& log, const DeclareDiscoveryOptions& options) {
  const auto activities = options.considered_activities.has_value()
                              ? to_set(*options.considered_activities)
                              : default_activities(log);
  std::set<std::string> allowed;
  if (!options.allowed_templates.has_value())
    allowed.insert(families().begin(), families().end());
  else
    allowed = to_set(*options.allowed_templates);
  DeclareModel model;
  if (activities.empty() || log.traces.empty()) return model;

  std::map<std::vector<std::string>, int> projected;
  for (const auto& trace : log.traces) ++projected[project(trace, activities)];
  std::vector<Statuses> rows;
  std::vector<int> weights;
  for (const auto& [trace, count] : projected) {
    rows.push_back(evaluate(trace, activities, allowed));
    weights.push_back(count);
  }
  std::set<CandidateKey> candidates;
  for (const auto& row : rows)
    for (const auto& [candidate, _] : row) candidates.insert(candidate);
  const int row_count = static_cast<int>(log.traces.size());
  double min_support = options.has_min_support_ratio ? options.min_support_ratio : 0.0;
  double min_confidence = options.has_min_confidence_ratio ? options.min_confidence_ratio : 0.0;
  if (!options.has_min_support_ratio && !options.has_min_confidence_ratio) {
    double best = -1.0;
    CandidateKey best_key;
    int best_support = 0, best_confidence = 0;
    for (const auto& candidate : candidates) {
      const auto [support, confidence] = stats_for(rows, weights, candidate);
      if (support <= 0) continue;
      const double product = static_cast<double>(confidence) / row_count;
      if (product > best || (product == best && best_key < candidate)) {
        best = product; best_key = candidate; best_support = support; best_confidence = confidence;
      }
    }
    if (best >= 0.0) {
      min_support = static_cast<double>(best_support) / row_count * options.auto_selection_multiplier;
      min_confidence = static_cast<double>(best_confidence) / best_support * options.auto_selection_multiplier;
    }
  }
  for (const auto& candidate : candidates) {
    const auto [support, confidence] = stats_for(rows, weights, candidate);
    // PM4Py uses positive comparisons (`>=`); spelling these as negated
    // `<` would accidentally accept NaN thresholds.
    if (!(support >= row_count * min_support) || support == 0 ||
        !(confidence >= support * min_confidence))
      continue;
    DeclareConstraint constraint;
    constraint.first = candidate.first;
    constraint.second = candidate.second;
    constraint.binary = candidate.binary;
    constraint.support = support;
    constraint.confidence = confidence;
    model.add(candidate.family, std::move(constraint));
  }
  return model;
}

DeclareConformance check_declare_conformance(const std::vector<std::string>& trace,
                                             const DeclareModel& model) {
  DeclareConformance result;
  for (const auto& [_, values] : model.rules) result.no_constr_total += static_cast<int>(values.size());
  std::unordered_map<std::string, std::vector<int>> idx;
  for (int i = 0; i < static_cast<int>(trace.size()); ++i) idx[trace[i]].push_back(i);
  auto add_dev = [&](const std::string& family, const DeclareConstraint& c) {
    result.deviations.push_back({family, c.first, c.second, c.binary});
  };
  auto check = [&](const std::string& family, auto&& predicate) {
    const auto it = model.rules.find(family); if (it == model.rules.end()) return;
    for (const auto& c : it->second) if (!predicate(c)) add_dev(family, c);
  };
  check(kDeclareExistence, [&](const auto& c){ return in(idx,c.first); });
  check(kDeclareExactlyOne, [&](const auto& c){
    const auto it = idx.find(c.first);
    return it != idx.end() && it->second.size() == 1;
  });
  check(kDeclareInit, [&](const auto& c){ return !trace.empty() && trace.front()==c.first; });
  check(kDeclareRespondedExistence, [&](const auto& c){ return !in(idx,c.first)||in(idx,c.second); });
  check(kDeclareCoexistence, [&](const auto& c){ return in(idx,c.first)==in(idx,c.second); });
  check(kDeclareNonCoexistence, [&](const auto& c){ return !(in(idx,c.first)&&in(idx,c.second)); });
  check(kDeclareResponse, [&](const auto& c){ return !in(idx,c.first)||(in(idx,c.second)&&idx.at(c.first).back()<idx.at(c.second).back()); });
  check(kDeclarePrecedence, [&](const auto& c){ return !in(idx,c.second)||(in(idx,c.first)&&idx.at(c.first).front()<idx.at(c.second).front()); });
  check(kDeclareSuccession, [&](const auto& c){ return (!in(idx,c.first)&&!in(idx,c.second)) || (in(idx,c.first)&&in(idx,c.second)&&idx.at(c.first).front()<idx.at(c.second).front()&&idx.at(c.first).back()<idx.at(c.second).back()); });
  check(kDeclareAltResponse, [&](const auto& c){ return alt_response(trace,idx,c.first,c.second); });
  check(kDeclareChainResponse, [&](const auto& c){ return chain_response(trace,idx,c.first,c.second); });
  check(kDeclareAltPrecedence, [&](const auto& c){ return alt_precedence(idx,c.first,c.second); });
  check(kDeclareChainPrecedence, [&](const auto& c){ return chain_precedence(trace,idx,c.first,c.second); });
  check(kDeclareAltSuccession, [&](const auto& c){ return alt_response(trace,idx,c.first,c.second)&&alt_precedence(idx,c.first,c.second); });
  check(kDeclareChainSuccession, [&](const auto& c){ return chain_response(trace,idx,c.first,c.second)&&chain_precedence(trace,idx,c.first,c.second); });
  check(kDeclareAbsence, [&](const auto& c){ return !in(idx,c.first); });
  check(kDeclareNonSuccession, [&](const auto& c){ return !in(idx,c.first)&&!in(idx,c.second) || !(in(idx,c.first)&&in(idx,c.second)&&idx.at(c.first).front()<idx.at(c.second).front()&&idx.at(c.first).back()<idx.at(c.second).back()); });
  check(kDeclareNonChainSuccession, [&](const auto& c){ return !in(idx,c.first)||!in(idx,c.second)||!(chain_response(trace,idx,c.first,c.second)&&chain_precedence(trace,idx,c.first,c.second)); });
  result.no_dev_total = static_cast<int>(result.deviations.size());
  result.dev_fitness = result.no_constr_total > 0 ? 1.0 - static_cast<double>(result.no_dev_total) / result.no_constr_total : 1.0;
  result.is_fit = result.no_dev_total == 0;
  return result;
}

std::vector<DeclareConformance> check_declare_conformance(const EventLog& log,
                                                          const DeclareModel& model) {
  std::vector<DeclareConformance> result;
  result.reserve(log.traces.size());
  for (const auto& trace : log.traces) {
    std::vector<std::string> activities;
    for (const auto& event : trace.events) activities.push_back(event.activity);
    result.push_back(check_declare_conformance(activities, model));
  }
  return result;
}

DeclareConformance check_declare_conformance(const Trace& trace,
                                             const DeclareModel& model) {
  std::vector<std::string> activities;
  activities.reserve(trace.events.size());
  for (const auto& event : trace.events) activities.push_back(event.activity);
  return check_declare_conformance(activities, model);
}

}  // namespace minera
