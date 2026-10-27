#include "minera/declare_streaming.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <utility>

namespace minera {
namespace {

struct State {
  std::string name = "init";
  bool a_seen = false;
  bool b_seen = false;
  bool waiting_a = true;
  bool waiting_b = false;
  bool expecting_b = false;
  bool last_was_a = false;
  // The previous activity; nothing before the first event, and None for an
  // event without one.
  std::optional<std::optional<std::string>> last;
  std::size_t pending_a = 0;
  std::size_t count_a = 0;
};

std::optional<bool> known_binary_arity(const std::string& f) {
  if (f == kDeclareExistence || f == kDeclareExactlyOne ||
      f == kDeclareInit || f == kDeclareAbsence)
    return false;
  if (f == kDeclareRespondedExistence || f == kDeclareResponse ||
         f == kDeclarePrecedence || f == kDeclareSuccession ||
         f == kDeclareAltResponse || f == kDeclareAltPrecedence ||
         f == kDeclareAltSuccession || f == kDeclareChainResponse ||
         f == kDeclareChainPrecedence || f == kDeclareChainSuccession ||
         f == kDeclareCoexistence || f == kDeclareNonCoexistence ||
         f == kDeclareNonSuccession || f == kDeclareNonChainSuccession)
    return true;
  return std::nullopt;
}

std::string rule_name(const DeclareConstraint& c, const std::string& family) {
  if (!c.binary && c.second.empty()) return family + "(" + c.first + ")";
  return family + "(" + c.first + "," + c.second + ")";
}

}  // namespace

struct DeclareStreamingConformance::Rule {
  std::string family;
  DeclareConstraint constraint;
  std::string name;
};

struct DeclareStreamingConformance::Case {
  std::size_t events = 0;
  std::size_t deviations = 0;
  std::vector<State> states;
};

DeclareStreamingConformance::DeclareStreamingConformance(const DeclareModel& model)
    : model_(model) {
  for (const auto& [family, values] : model_.rules) {
    for (const auto& c : values) {
      const auto arity = known_binary_arity(family);
      if (arity.has_value() && c.binary != *arity)
        throw std::invalid_argument("Declare constraint '" + family +
                                    "' has an invalid arity");
      rules_.push_back(
          std::make_shared<Rule>(Rule{family, c, rule_name(c, family)}));
    }
  }
}

DeclareStreamingEventResult DeclareStreamingConformance::receive(const Event& event) {
  // A present case id is a value even when empty; only an event without one
  // falls back to "undefined_case", as in the reference.
  std::string case_id = event.case_id;
  if (case_id.empty() && event.case_id_state != FirstClassAttributeState::kPresent)
    case_id = "undefined_case";
  std::optional<std::string> activity;
  if (!event.activity.empty() || event.activity_state == FirstClassAttributeState::kPresent)
    activity = event.activity;
  return process(std::move(case_id), std::move(activity), event.timestamp);
}

DeclareStreamingEventResult DeclareStreamingConformance::receive_record(
    std::optional<std::string> case_id, bool case_is_null,
    std::optional<std::string> activity, std::string timestamp) {
  std::string key = case_is_null ? std::string("\0None", 5)
                                 : case_id.value_or("undefined_case");
  return process(std::move(key), std::move(activity), std::move(timestamp));
}

DeclareStreamingEventResult DeclareStreamingConformance::process(
    std::string case_id, std::optional<std::string> act, std::string event_timestamp) {
  const std::lock_guard lock(mutex_);
  ++total_events_;
  auto [it, inserted] = cases_.try_emplace(case_id, std::make_shared<Case>());
  if (inserted) it->second->states.resize(rules_.size());
  Case& cs = *it->second;
  ++cs.events;
  std::size_t deviations = 0;
  std::vector<DeclareStreamingConstraintIdentity> violated;
  for (std::size_t i = 0; i < rules_.size(); ++i) {
    const auto& r = *rules_[i];
    auto& s = cs.states[i];
    if (s.name == "violated") continue;
    const std::string& a = r.constraint.first;
    const std::string& b = r.constraint.second;
    bool bad = false;
    if (r.family == kDeclareExistence) {
      if (act == a) s.name = "seen";
    } else if (r.family == kDeclareAbsence) {
      bad = act == a;
    } else if (r.family == kDeclareExactlyOne) {
      if (act == a) { ++s.count_a; if (s.count_a == 1) s.name = "seen_once"; else bad = true; }
    } else if (r.family == kDeclareInit) {
      if (cs.events == 1) { if (act == a) s.name = "ok"; else bad = true; }
    } else if (r.family == kDeclareRespondedExistence || r.family == kDeclareCoexistence) {
      if (act == a) s.a_seen = true;
      if (act == b) s.b_seen = true;
    } else if (r.family == kDeclareResponse) {
      if (act == a) ++s.pending_a;
      if (act == b && s.pending_a > 0) --s.pending_a;
    } else if (r.family == kDeclarePrecedence) {
      if (act == b && !s.a_seen) bad = true;
      if (act == a) s.a_seen = true;
    } else if (r.family == kDeclareSuccession) {
      if (act == b && !s.a_seen) bad = true;
      if (act == a) { s.a_seen = true; ++s.pending_a; }
      else if (act == b && s.pending_a > 0) --s.pending_a;
    } else if (r.family == kDeclareAltResponse) {
      if (act == a) { if (s.waiting_b) bad = true; s.waiting_b = true; }
      if (act == b && s.waiting_b) s.waiting_b = false;
    } else if (r.family == kDeclareAltPrecedence) {
      if (act == b) { if (s.waiting_a) bad = true; s.waiting_a = true; }
      if (act == a) s.waiting_a = false;
    } else if (r.family == kDeclareAltSuccession) {
      if (act == a) { if (s.waiting_b) bad = true; s.waiting_b = true; s.waiting_a = false; }
      else if (act == b) { if (s.waiting_a) bad = true; s.waiting_a = true; s.waiting_b = false; }
    } else if (r.family == kDeclareChainResponse) {
      if (s.expecting_b && act != b) bad = true;
      if (!bad) { s.expecting_b = false; if (act == a) s.expecting_b = true; }
    } else if (r.family == kDeclareChainPrecedence) {
      if (act == b && s.last != std::optional<std::string>(a)) bad = true;
      s.last = act;
    } else if (r.family == kDeclareChainSuccession) {
      if (s.expecting_b && act != b) bad = true;
      if (!bad && act == b && s.last != std::optional<std::string>(a)) bad = true;
      if (!bad) { s.expecting_b = false; if (act == a) s.expecting_b = true; s.last = act; }
    } else if (r.family == kDeclareNonCoexistence) {
      if (act == a) { if (s.b_seen) bad = true; s.a_seen = true; }
      if (act == b) { if (s.a_seen) bad = true; s.b_seen = true; }
    } else if (r.family == kDeclareNonSuccession) {
      if (act == a) s.a_seen = true;
      if (act == b && s.a_seen) bad = true;
    } else if (r.family == kDeclareNonChainSuccession) {
      if (s.last_was_a && act == b) bad = true;
      s.last_was_a = (act == a);
    } else {
      // Unknown families are intentionally inert, but retain a stable state.
    }
    if (bad) {
      s.name = "violated";
      ++deviations;
      violated.push_back(
          {i, r.family, r.constraint.first, r.constraint.second,
           r.constraint.binary});
    }
  }
  cs.deviations += deviations;
  total_deviations_ += deviations;
  const std::string timestamp = event_timestamp.empty() ? std::to_string(total_events_) : event_timestamp;
  deviations_per_time_.push_back({timestamp, deviations});
  return {case_id, act.value_or(std::string{}), timestamp, total_events_, deviations, std::move(violated)};
}

DeclareStreamingEventResult DeclareStreamingConformance::receive(std::string case_id,
                                                                  std::string activity,
                                                                  std::string timestamp) {
  // Empty case ids and activities are ordinary values.
  return process(std::move(case_id), std::move(activity), std::move(timestamp));
}

void DeclareStreamingConformance::reset_case(const std::string& case_id) {
  const std::lock_guard lock(mutex_);
  cases_.erase(case_id);
}
void DeclareStreamingConformance::reset_all() {
  const std::lock_guard lock(mutex_);
  cases_.clear();
  total_events_ = total_deviations_ = 0;
  deviations_per_time_.clear();
}

DeclareStreamingSnapshot DeclareStreamingConformance::snapshot() const {
  const std::lock_guard lock(mutex_);
  DeclareStreamingSnapshot out{total_events_, total_deviations_, deviations_per_time_, {}};
  for (const auto& [id, csp] : cases_) {
    const auto& cs = *csp;
    DeclareStreamingCaseSnapshot row; row.events = cs.events; row.deviations = cs.deviations;
    row.constraint_states.reserve(rules_.size());
    for (std::size_t i = 0; i < rules_.size(); ++i) {
      const auto& rule = *rules_[i];
      row.constraint_states.push_back(
          {i, rule.family, rule.constraint.first, rule.constraint.second,
           rule.constraint.binary, cs.states[i].name});
    }
    out.cases.emplace(id, std::move(row));
  }
  return out;
}

}  // namespace minera
