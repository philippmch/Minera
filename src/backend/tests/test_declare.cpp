#include "minera/declare.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using namespace minera;

namespace {
EventLog log(std::initializer_list<std::initializer_list<const char*>> traces) {
  EventLog result;
  std::size_t order = 0;
  for (auto values : traces) {
    Trace trace;
    trace.case_id = std::to_string(result.traces.size());
    for (const auto* value : values) trace.events.push_back(Event{trace.case_id, value, {}, {}, order++});
    result.traces.push_back(std::move(trace));
  }
  return result;
}

void all_templates_are_named_and_filterable() {
  const auto discovered = discover_declare(log({{"A", "B"}, {"A"}, {"B"}}));
  assert(discovered.rules.size() == 18);
  assert(discovered.rules.contains(kDeclareNonChainSuccession));
  DeclareDiscoveryOptions options;
  options.considered_activities = {"A", "B"};
  options.allowed_templates = {kDeclareExistence};
  options.has_min_support_ratio = true;
  options.min_support_ratio = 0.5;
  const auto filtered = discover_declare(log({{"A"}, {"A"}, {"B"}}), options);
  assert(!filtered.rules.at(kDeclareExistence).empty());
  assert(filtered.rules.at(kDeclareExistence).front().first == "A");
  assert(filtered.rules.at(kDeclareExistence).front().support == 3);
}

void duplicate_traces_and_thresholds_are_weighted() {
  DeclareDiscoveryOptions options;
  options.allowed_templates = {kDeclareExistence};
  options.has_min_confidence_ratio = true;
  options.min_confidence_ratio = 0.6;
  const auto model = discover_declare(log({{"A"}, {"A"}, {"B"}}), options);
  assert(model.rules.at(kDeclareExistence).size() == 1);
  assert(model.rules.at(kDeclareExistence).front().confidence == 2);
  DeclareDiscoveryOptions automatic;
  automatic.allowed_templates = {kDeclareExistence};
  const auto auto_model = discover_declare(log({{"A"}, {"B"}}), automatic);
  assert(auto_model.rules.at(kDeclareExistence).size() == 2);
}

void conformance_checks_direction_and_order() {
  DeclareModel model;
  model.add(kDeclareExistence, {"A", {}, false});
  model.add(kDeclareResponse, {"A", "B", true});
  model.add(kDeclarePrecedence, {"A", "B", true});
  model.add(kDeclareAbsence, {"C", {}, false});
  model.add(kDeclareSuccession, {"A", "B", true});
  const auto fit = check_declare_conformance(std::vector<std::string>{"A", "B"}, model);
  assert(fit.is_fit);
  const auto bad = check_declare_conformance(std::vector<std::string>{"B", "A", "C"}, model);
  assert(!bad.is_fit);
  assert(bad.no_constr_total == 5);
  assert(bad.no_dev_total >= 3);
  assert(bad.deviations.front().template_name == kDeclareResponse ||
         bad.deviations.front().template_name == kDeclarePrecedence);
  DeclareModel unknown;
  unknown.rules["future_template"].push_back({"A", "B", true});
  const auto unknown_result = check_declare_conformance(std::vector<std::string>{"A"}, unknown);
  assert(unknown_result.no_constr_total == 1);
  assert(unknown_result.is_fit);
}

void vacuity_and_empty_inputs() {
  DeclareModel model;
  model.add(kDeclareResponse, {"A", "B", true});
  model.add(kDeclarePrecedence, {"A", "B", true});
  assert(check_declare_conformance(std::vector<std::string>{"C"}, model).is_fit);
  assert(check_declare_conformance(std::vector<std::string>{}, model).is_fit);
  assert(discover_declare(EventLog{}).size() == 0);
}

void every_template_family_and_prerequisite_rules() {
  const std::vector<std::string> all = {
      kDeclareExistence, kDeclareExactlyOne, kDeclareInit,
      kDeclareRespondedExistence, kDeclareResponse, kDeclarePrecedence,
      kDeclareSuccession, kDeclareAltResponse, kDeclareAltPrecedence,
      kDeclareAltSuccession, kDeclareChainResponse, kDeclareChainPrecedence,
      kDeclareChainSuccession, kDeclareAbsence, kDeclareCoexistence,
      kDeclareNonCoexistence, kDeclareNonSuccession,
      kDeclareNonChainSuccession};
  DeclareDiscoveryOptions options;
  options.allowed_templates = all;
  options.has_min_support_ratio = true;
  options.has_min_confidence_ratio = true;
  const auto model = discover_declare(log({{"A", "B"}, {"B", "A"}}), options);
  for (const auto& family : all) {
    assert(model.rules.contains(family));
    assert(!model.rules.at(family).empty());
  }
  assert(model.rules.at(kDeclareExistence).size() == 2);
  assert(!model.rules.at(kDeclareSuccession).empty());
  assert(!model.rules.at(kDeclareNonSuccession).empty());

  // Exercise every family through a concrete rule, including both a fitting
  // and a violating/activation trace where the template has directional
  // vacuity semantics.
  DeclareModel unary;
  unary.add(kDeclareExistence, {"A", {}, false});
  unary.add(kDeclareExactlyOne, {"A", {}, false});
  unary.add(kDeclareInit, {"A", {}, false});
  unary.add(kDeclareAbsence, {"B", {}, false});
  assert(check_declare_conformance(std::vector<std::string>{"A"}, unary).is_fit);
  assert(!check_declare_conformance(std::vector<std::string>{"A", "A", "B"}, unary).is_fit);

  DeclareModel relational;
  relational.add(kDeclareRespondedExistence, {"A", "B", true});
  relational.add(kDeclareCoexistence, {"A", "B", true});
  relational.add(kDeclareNonCoexistence, {"A", "B", true});
  relational.add(kDeclareResponse, {"A", "B", true});
  relational.add(kDeclarePrecedence, {"A", "B", true});
  relational.add(kDeclareSuccession, {"A", "B", true});
  relational.add(kDeclareAltResponse, {"A", "B", true});
  relational.add(kDeclareAltPrecedence, {"A", "B", true});
  relational.add(kDeclareAltSuccession, {"A", "B", true});
  relational.add(kDeclareChainResponse, {"A", "B", true});
  relational.add(kDeclareChainPrecedence, {"A", "B", true});
  relational.add(kDeclareChainSuccession, {"A", "B", true});
  relational.add(kDeclareNonSuccession, {"A", "B", true});
  relational.add(kDeclareNonChainSuccession, {"A", "B", true});
  DeclareModel positive_relational;
  positive_relational.rules = relational.rules;
  positive_relational.rules[kDeclareNonCoexistence].clear();
  positive_relational.rules[kDeclareNonSuccession].clear();
  positive_relational.rules[kDeclareNonChainSuccession].clear();
  assert(check_declare_conformance(std::vector<std::string>{"A", "B"},
                                   positive_relational)
             .is_fit);
  assert(!check_declare_conformance(std::vector<std::string>{"A", "B"}, relational)
              .is_fit);
  const auto relational_bad = check_declare_conformance(
      std::vector<std::string>{"B", "A", "A"}, relational);
  assert(!relational_bad.is_fit);
  assert(relational_bad.no_dev_total > 0);
  // Directional constraints are vacuous when their activation is absent.
  assert(check_declare_conformance(std::vector<std::string>{"B"}, relational)
             .deviations.size() > 0);

  DeclareDiscoveryOptions absence_only;
  absence_only.allowed_templates = {kDeclareAbsence};
  assert(discover_declare(log({{"A"}}), absence_only).size() == 0);
  absence_only.allowed_templates = {kDeclareAbsence, kDeclareExistence};
  absence_only.has_min_support_ratio = true;
  absence_only.has_min_confidence_ratio = true;
  assert(discover_declare(log({{"A"}}), absence_only).rules.at(kDeclareAbsence).size() == 1);
}

void directional_vacuity_and_negative_conformance() {
  DeclareModel model;
  model.add(kDeclareResponse, {"A", "B", true});
  model.add(kDeclarePrecedence, {"A", "B", true});
  model.add(kDeclareAltResponse, {"A", "B", true});
  model.add(kDeclareAltPrecedence, {"A", "B", true});
  model.add(kDeclareChainResponse, {"A", "B", true});
  model.add(kDeclareChainPrecedence, {"A", "B", true});
  model.add(kDeclareSuccession, {"A", "B", true});
  model.add(kDeclareNonSuccession, {"A", "B", true});
  const auto only_b = check_declare_conformance(std::vector<std::string>{"B"}, model);
  assert(!only_b.is_fit);
  assert(only_b.no_dev_total == 4);  // precedence/alt/chain/succession
  const auto bad = check_declare_conformance(std::vector<std::string>{"A"}, model);
  assert(!bad.is_fit);
  assert(bad.no_dev_total == 4);
  const auto neg = check_declare_conformance(std::vector<std::string>{"A", "B"}, model);
  assert(!neg.is_fit);
  assert(neg.deviations.back().template_name == kDeclareNonSuccession);
}

void explicit_empty_and_unusual_activity_keys() {
  DeclareDiscoveryOptions empty;
  empty.considered_activities = std::vector<std::string>{};
  assert(discover_declare(log({{"A"}}), empty).size() == 0);
  empty.considered_activities.reset();
  empty.allowed_templates = std::vector<std::string>{};
  assert(discover_declare(log({{"A"}}), empty).size() == 0);

  const std::string control = std::string("A") + static_cast<char>(0x1f) + "B";
  DeclareDiscoveryOptions unusual;
  unusual.considered_activities = std::vector<std::string>{control, ""};
  unusual.allowed_templates = std::vector<std::string>{kDeclareResponse};
  unusual.has_min_support_ratio = true;
  unusual.has_min_confidence_ratio = true;
  const auto model = discover_declare(log({{control.c_str(), ""}, {control.c_str()}}), unusual);
  bool found = false;
  for (const auto& rule : model.rules.at(kDeclareResponse)) {
    if (rule.first == control && rule.second.empty()) {
      assert(rule.binary);
      found = true;
    }
  }
  assert(found);
}

void threshold_edge_values_follow_positive_predicates() {
  DeclareDiscoveryOptions options;
  options.allowed_templates = {kDeclareExistence};
  options.considered_activities = {"A"};
  options.has_min_support_ratio = true;
  options.min_support_ratio = std::numeric_limits<double>::quiet_NaN();
  assert(discover_declare(log({{"A"}}), options).rules.at(kDeclareExistence).empty());
  options.min_support_ratio = std::numeric_limits<double>::infinity();
  assert(discover_declare(log({{"A"}}), options).rules.at(kDeclareExistence).empty());
  options.min_support_ratio = -std::numeric_limits<double>::infinity();
  assert(!discover_declare(log({{"A"}}), options).rules.at(kDeclareExistence).empty());
  options.has_min_support_ratio = true;
  options.min_support_ratio = 0.0;
  options.has_min_confidence_ratio = true;
  options.min_confidence_ratio = std::numeric_limits<double>::infinity();
  assert(discover_declare(log({{"A"}}), options).rules.at(kDeclareExistence).empty());
  options.min_confidence_ratio = -std::numeric_limits<double>::infinity();
  assert(!discover_declare(log({{"A"}}), options).rules.at(kDeclareExistence).empty());
  options.has_min_support_ratio = false;
  options.has_min_confidence_ratio = false;
  options.auto_selection_multiplier = std::numeric_limits<double>::quiet_NaN();
  assert(discover_declare(log({{"A"}}), options).rules.at(kDeclareExistence).empty());
  options.auto_selection_multiplier = 0.8;
  options.has_min_confidence_ratio = true;
  options.min_confidence_ratio = -1.0;
  assert(!discover_declare(log({{"A"}}), options).rules.at(kDeclareExistence).empty());
}

void conformance_deviation_order_is_fixed() {
  DeclareModel model;
  model.add(kDeclareAbsence, {"A", {}, false});
  model.add(kDeclareExistence, {"B", {}, false});
  model.add(kDeclareResponse, {"A", "B", true});
  model.add(kDeclarePrecedence, {"A", "B", true});
  const auto result = check_declare_conformance(std::vector<std::string>{"A"}, model);
  assert(result.deviations.size() == 3);
  assert(result.deviations[0].template_name == kDeclareExistence ||
         result.deviations[0].template_name == kDeclareAbsence);
  assert(result.deviations[1].template_name == kDeclareResponse);
  assert(result.deviations[2].template_name == kDeclareAbsence);
}

void missing_exactly_one_activity_does_not_create_empty_occurrences() {
  DeclareModel model;
  model.add(kDeclareExactlyOne, {"A", {}, false});
  model.add(kDeclareResponse, {"A", "B", true});
  model.add(kDeclarePrecedence, {"A", "B", true});

  // The absent A violates EXACTLY_ONE, is a vacuous response activation, and
  // cannot satisfy precedence for the observed B.  In particular, checking
  // EXACTLY_ONE must not insert an empty A occurrence that later predicates
  // treat as present.
  const auto result = check_declare_conformance(
      std::vector<std::string>{"B"}, model);
  assert(!result.is_fit);
  assert(result.no_constr_total == 3);
  assert(result.no_dev_total == 2);
  bool saw_exactly_one = false;
  bool saw_precedence = false;
  bool saw_response = false;
  for (const auto& deviation : result.deviations) {
    saw_exactly_one |= deviation.template_name == kDeclareExactlyOne;
    saw_precedence |= deviation.template_name == kDeclarePrecedence;
    saw_response |= deviation.template_name == kDeclareResponse;
  }
  assert(saw_exactly_one);
  assert(saw_precedence);
  assert(!saw_response);

  DeclareModel exactly_one_only;
  exactly_one_only.add(kDeclareExactlyOne, {"A", {}, false});
  const auto absent = check_declare_conformance(
      std::vector<std::string>{"B"}, exactly_one_only);
  assert(!absent.is_fit);
  assert(absent.no_dev_total == 1);
}
}  // namespace

int main() {
  all_templates_are_named_and_filterable();
  duplicate_traces_and_thresholds_are_weighted();
  conformance_checks_direction_and_order();
  vacuity_and_empty_inputs();
  every_template_family_and_prerequisite_rules();
  directional_vacuity_and_negative_conformance();
  explicit_empty_and_unusual_activity_keys();
  threshold_edge_values_follow_positive_predicates();
  conformance_deviation_order_is_fixed();
  missing_exactly_one_activity_does_not_create_empty_occurrences();
  std::cout << "declare tests passed\n";
}
