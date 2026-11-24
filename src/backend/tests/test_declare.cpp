#include "minera/declare.hpp"
#include "minera/log_conversion.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
#include <string_view>
#include <tuple>
#include <vector>

using namespace minera;

namespace {
EventLog log(std::initializer_list<std::initializer_list<const char*>> traces) {
  EventLog result;
  std::size_t order = 0;
  for (auto values : traces) {
    Trace trace;
    trace.case_id = std::to_string(result.traces.size());
    for (const auto* value : values) {
      Event event{trace.case_id, value, {}, {}, order++};
      event.activity_state = FirstClassAttributeState::kPresent;
      trace.events.push_back(std::move(event));
    }
    result.traces.push_back(std::move(trace));
  }
  return result;
}

void case_provenance_does_not_shadow_event_activity() {
  auto selected = log({{"A", "B"}});
  const auto expected = discover_declare(selected);
  auto& trace = selected.traces.front();
  trace.case_id = "case-one";
  trace.case_id_state = FirstClassAttributeState::kPresent;
  trace.case_id_source_key = "concept:name";
  for (auto& event : trace.events) {
    event.case_id = trace.case_id;
    event.case_id_state = FirstClassAttributeState::kPresent;
    event.case_id_source_key = "concept:name";
    event.activity_source_key = "concept:name";
    event.activity_attribute = EventAttribute{"concept:name", event.activity};
  }
  assert(discover_declare(selected).rules == expected.rules);
  const auto& responses = expected.rules.at(kDeclareChainResponse);
  assert(std::any_of(responses.begin(), responses.end(), [](const auto& rule) {
    return rule.first == "A" && rule.second == "B";
  }));

  DeclareDiscoveryOptions options;
  options.allowed_templates = {kDeclareExistence};
  options.activity_key = "case_id";
  const auto case_model = discover_declare(selected, options);
  assert(case_model.rules.at(kDeclareExistence).size() == 1);
  assert(case_model.rules.at(kDeclareExistence).front().first == "case-one");
  for (auto& event : trace.events) event.case_id_source_key = "batch";
  options.activity_key = "batch";
  assert(discover_declare(selected, options).rules == case_model.rules);

  options.activity_key = "concept:name";
  for (auto& event : trace.events) {
    event.resource = "worker";
    event.resource_state = FirstClassAttributeState::kPresent;
    event.resource_source_key = "concept:name";
  }
  bool rejected = false;
  try { (void)discover_declare(selected, options); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
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

void self_pairs_follow_reference_comparisons() {
  // The reference compares the last (response), first (precedence) or both
  // (succession) occurrences of A with themselves, so a case holding A
  // satisfies the self-pair and one without A is vacuous.
  for (const char* family : {kDeclareResponse, kDeclarePrecedence, kDeclareSuccession}) {
    DeclareModel model;
    model.add(family, {"A", "A", true});
    for (const auto& trace : {std::vector<std::string>{"A"},
                              std::vector<std::string>{"A", "B", "A"},
                              std::vector<std::string>{"B"}}) {
      const auto result = check_declare_conformance(trace, model);
      assert(result.is_fit);
      assert(result.no_dev_total == 0);
    }
  }
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
  assert(discover_declare({}).size() == 0);
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

EventLog attribute_activity_log(bool include_missing = false, bool include_null = false) {
  EventLog result;
  Trace trace;
  trace.case_id = "0";
  Event first{"0", "ignored"};
  first.attributes.push_back(EventAttribute{"kind", "A"});
  Event second{"0", "ignored"};
  if (include_missing) {
    second.attributes.push_back(EventAttribute{"other", "B"});
  } else if (include_null) {
    second.attributes.push_back(
        EventAttribute{"kind", "", AttributeKind::kString, true});
  } else {
    second.attributes.push_back(EventAttribute{"kind", "B"});
  }
  trace.events = {std::move(first), std::move(second)};
  result.traces.push_back(std::move(trace));
  return result;
}

EventLog projected_activity_log() {
  auto result = attribute_activity_log();
  for (auto& event : result.traces[0].events) {
    const auto value = *event.attribute("kind");
    event.activity = value;
    event.activity_state = FirstClassAttributeState::kPresent;
    event.activity_source_key = "kind";
    event.activity_attribute = EventAttribute{"kind", value};
    event.attributes.clear();
  }
  return result;
}

EventLog first_class_activity_log(std::string_view selector,
                                  bool missing = false, bool null_value = false) {
  EventLog result;
  Trace trace;
  trace.case_id = "0";
  const bool resource_field = selector == "org:resource" ||
                              selector == "resource" || selector == "worker";
  const bool timestamp_field = selector == "time:timestamp" ||
                               selector == "timestamp" || selector == "when";
  const bool lifecycle_field = selector == "lifecycle:transition" ||
                               selector == "lifecycle" || selector == "phase";
  for (const char* value : {"A", "B"}) {
    Event event{"0", "ignored"};
    if (resource_field) {
      event.resource_source_key = selector == "worker" ? "worker" : "";
      event.resource = missing || null_value ? "" : value;
      event.resource_state = missing
                                 ? FirstClassAttributeState::kMissing
                                 : (null_value ? FirstClassAttributeState::kNull
                                               : FirstClassAttributeState::kPresent);
      if (selector == "worker" && !missing)
        event.resource_attribute = EventAttribute{
            "worker", missing || null_value ? "" : value,
            AttributeKind::kString, null_value};
    } else if (timestamp_field) {
      event.timestamp_source_key = selector == "when" ? "when" : "";
      event.timestamp = missing || null_value ? "" : value;
      event.timestamp_state = missing
                                  ? FirstClassAttributeState::kMissing
                                  : (null_value ? FirstClassAttributeState::kNull
                                                : FirstClassAttributeState::kPresent);
      if (selector == "when" && !missing)
        event.timestamp_attribute = EventAttribute{
            "when", missing || null_value ? "" : value,
            AttributeKind::kString, null_value};
    } else if (lifecycle_field) {
      event.lifecycle_source_key = selector == "phase" ? "phase" : "";
      event.lifecycle = missing || null_value ? "" : value;
      event.lifecycle_state = missing
                                  ? FirstClassAttributeState::kMissing
                                  : (null_value ? FirstClassAttributeState::kNull
                                                : FirstClassAttributeState::kPresent);
      if (selector == "phase" && !missing)
        event.lifecycle_attribute = EventAttribute{
            "phase", missing || null_value ? "" : value,
            AttributeKind::kString, null_value};
    }
    trace.events.push_back(std::move(event));
  }
  result.traces.push_back(std::move(trace));
  return result;
}

void custom_activity_selector_is_strict() {
  DeclareDiscoveryOptions discovery;
  discovery.activity_key = "kind";
  discovery.allowed_templates = {kDeclareExistence};
  discovery.has_min_support_ratio = true;
  discovery.has_min_confidence_ratio = true;
  const auto custom_model = discover_declare(attribute_activity_log(), discovery);
  assert(custom_model.rules.at(kDeclareExistence).size() == 2);
  assert(custom_model.rules.at(kDeclareExistence)[0].first == "A");
  assert(custom_model.rules.at(kDeclareExistence)[1].first == "B");
  const auto projected_model = discover_declare(projected_activity_log(), discovery);
  assert(projected_model.rules.at(kDeclareExistence).size() == 2);

  DeclareConformanceOptions conformance;
  conformance.activity_key = "kind";
  assert(check_declare_conformance(attribute_activity_log().traces[0],
                                   custom_model, conformance)
             .is_fit);
  const auto event_log_result = check_declare_conformance(
      attribute_activity_log(), custom_model, conformance);
  assert(event_log_result.size() == 1 && event_log_result[0].is_fit);

  bool missing_threw = false;
  try {
    (void)discover_declare(attribute_activity_log(true), discovery);
  } catch (const std::invalid_argument&) {
    missing_threw = true;
  }
  assert(missing_threw);
  missing_threw = false;
  try {
    (void)check_declare_conformance(attribute_activity_log(true).traces[0],
                                    custom_model, conformance);
  } catch (const std::invalid_argument&) {
    missing_threw = true;
  }
  assert(missing_threw);
  missing_threw = false;
  try {
    (void)check_declare_conformance(attribute_activity_log(true), custom_model,
                                    conformance);
  } catch (const std::invalid_argument&) {
    missing_threw = true;
  }
  assert(missing_threw);

  bool null_threw = false;
  try {
    (void)discover_declare(attribute_activity_log(false, true), discovery);
  } catch (const std::invalid_argument&) {
    null_threw = true;
  }
  assert(null_threw);
}

void blank_activity_selectors_reject_empty_inputs() {
  DeclareDiscoveryOptions discovery;
  discovery.activity_key = " \t ";
  bool threw = false;
  try {
    (void)discover_declare(EventLog{}, discovery);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  DeclareConformanceOptions conformance;
  conformance.activity_key = "\t";
  threw = false;
  try {
    (void)check_declare_conformance(EventLog{}, DeclareModel{}, conformance);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  threw = false;
  try {
    (void)check_declare_conformance(Trace{}, DeclareModel{}, conformance);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void grouped_inputs_ignore_flat_case_selector() {
  DeclareDiscoveryOptions discovery;
  discovery.case_id_key = " \t ";
  discovery.allowed_templates = {kDeclareExistence};
  const auto source = log({{"A"}});
  const auto model = discover_declare(source, discovery);
  assert(model.rules.at(kDeclareExistence).size() == 1);

  DeclareConformanceOptions conformance;
  conformance.case_id_key = "";
  assert(check_declare_conformance(source, model, conformance).size() == 1);
  assert(check_declare_conformance(source.traces[0], model, conformance)
             .is_fit);

  bool threw = false;
  try {
    (void)discover_declare(event_log_to_table(source), discovery);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void first_class_activity_selectors_are_supported() {
  for (const std::string selector : {"org:resource", "time:timestamp",
                                     "lifecycle:transition", "worker", "when",
                                     "phase"}) {
    DeclareDiscoveryOptions discovery;
    discovery.activity_key = selector;
    discovery.allowed_templates = {kDeclareExistence};
    discovery.has_min_support_ratio = true;
    discovery.has_min_confidence_ratio = true;
    const auto selected = first_class_activity_log(selector);
    const auto model = discover_declare(selected, discovery);
    assert(model.rules.at(kDeclareExistence).size() == 2);
    DeclareConformanceOptions conformance;
    conformance.activity_key = selector;
    assert(check_declare_conformance(selected.traces[0], model, conformance)
               .is_fit);
    const auto log_result =
        check_declare_conformance(selected, model, conformance);
    assert(log_result.size() == 1 && log_result[0].is_fit);
  }

  DeclareDiscoveryOptions discovery;
  discovery.activity_key = "org:resource";
  discovery.allowed_templates = {kDeclareExistence};
  bool threw = false;
  try {
    (void)discover_declare(first_class_activity_log("org:resource", true),
                           discovery);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)discover_declare(first_class_activity_log("org:resource", false, true),
                           discovery);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  DeclareConformanceOptions conformance;
  conformance.activity_key = "org:resource";
  const auto model = discover_declare(first_class_activity_log("org:resource"),
                                      discovery);
  threw = false;
  try {
    (void)check_declare_conformance(
        first_class_activity_log("org:resource", true).traces[0], model,
        conformance);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)check_declare_conformance(
        first_class_activity_log("org:resource", false, true), model,
        conformance);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

// pm4py's Polars Declare path checks the frame before projecting it (round
// FV): a frame needs a date column, so the synthetic lazy inputs below gain
// one shared time, which keeps their row order through the time sort.
EventTable with_time(EventTable table) {
  const auto found = std::find(table.columns.begin(), table.columns.end(), "time:timestamp");
  const auto column = static_cast<std::size_t>(found - table.columns.begin());
  if (found == table.columns.end()) {
    table.columns.push_back("time:timestamp");
    if (!table.dtypes.empty()) table.dtypes.push_back("datetime64[ns, UTC]");
  } else if (!table.dtypes.empty()) {
    table.dtypes[column] = "datetime64[ns, UTC]";
  }
  for (auto& row : table.rows) {
    if (row.size() <= column) row.resize(column + 1);
    row[column] = EventAttribute{"time:timestamp", "2020-01-01T00:00:00Z", AttributeKind::kDate};
  }
  return table;
}

void activity_selector_precedence_and_ambiguity_are_strict() {
  const auto collision_log = [](bool exact_attribute, bool exact_null,
                                bool ambiguous_source) {
    EventLog result;
    Trace trace;
    trace.case_id = "0";
    for (const char* activity : {"A", "B"}) {
      Event event{"0", "canonical"};
      event.resource = "resource";
      event.resource_state = FirstClassAttributeState::kPresent;
      event.resource_source_key = "worker";
      event.resource_attribute = EventAttribute{"worker", "resource"};
      if (ambiguous_source) {
        event.timestamp = "timestamp";
        event.timestamp_state = FirstClassAttributeState::kPresent;
        event.timestamp_source_key = "worker";
        event.timestamp_attribute = EventAttribute{"worker", "timestamp"};
      }
      if (exact_attribute)
        event.attributes.push_back(EventAttribute{
            "worker", exact_null ? "" : activity, AttributeKind::kString,
            exact_null});
      trace.events.push_back(std::move(event));
    }
    result.traces.push_back(std::move(trace));
    return result;
  };

  DeclareDiscoveryOptions options;
  options.activity_key = "worker";
  options.allowed_templates = {kDeclareExistence};
  options.has_min_support_ratio = true;
  options.has_min_confidence_ratio = true;
  const auto exact = discover_declare(collision_log(true, false, false), options);
  assert(exact.rules.at(kDeclareExistence).size() == 2);
  assert(exact.rules.at(kDeclareExistence)[0].first == "A");
  assert(exact.rules.at(kDeclareExistence)[1].first == "B");

  for (const auto candidate : {collision_log(true, true, false),
                               collision_log(false, false, true)}) {
    bool threw = false;
    try {
      (void)discover_declare(candidate, options);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }
}

void flat_inputs_and_diagnostics_are_equivalent() {
  DeclareDiscoveryOptions discovery;
  discovery.allowed_templates = {kDeclareExistence, kDeclareResponse};
  discovery.has_min_support_ratio = true;
  discovery.has_min_confidence_ratio = true;
  const auto source = log({{"A", "B"}, {"A"}});
  const auto stream = event_log_to_event_stream(source);
  const auto table = event_log_to_table(source);
  const auto lazy = LazyEventTable::from_table(with_time(table));

  const auto expected = discover_declare(source, discovery);
  assert(discover_declare(stream, discovery).rules == expected.rules);
  assert(discover_declare(table, discovery).rules == expected.rules);
  assert(discover_declare(lazy, discovery).rules == expected.rules);
  assert(discover_declare(EventDataView(table), discovery).rules == expected.rules);

  const auto results = check_declare_conformance(source, expected);
  assert(check_declare_conformance(stream, expected).size() == results.size());
  assert(check_declare_conformance(table, expected).size() == results.size());
  assert(check_declare_conformance(lazy, expected).size() == results.size());
  assert(check_declare_conformance(EventDataView(stream), expected).size() ==
         results.size());
  assert(results.size() == 2);

  const auto diagnostics = declare_conformance_diagnostics(source, results);
  assert(diagnostics.columns ==
         std::vector<std::string>({"case_id", "no_dev_total",
                                   "no_constr_total", "dev_fitness"}));
  assert(diagnostics.rows.size() == 2);
  assert(diagnostics.rows[0][0]->value == "0");
  assert(diagnostics.rows[1][0]->value == "1");
  assert(diagnostics.rows[0][0]->first_class_role ==
         FirstClassAttributeRole::kCaseId);
  assert(std::get<std::int64_t>(diagnostics.rows[0][1]->typed_value) ==
         results[0].no_dev_total);
  assert(diagnostics.rows[0][3]->kind == AttributeKind::kFloat);
  assert(declare_conformance_diagnostics(table, results).rows ==
         diagnostics.rows);

  DeclareDiagnosticsOptions custom_case;
  custom_case.case_id_key = "case:batch";
  auto custom = source;
  custom.traces[0].attributes.push_back({"case:batch", "north"});
  custom.traces[1].attributes.push_back({"case:batch", "south"});
  const auto custom_results = check_declare_conformance(custom, expected);
  const auto custom_diagnostics =
      declare_conformance_diagnostics(custom, custom_results, custom_case);
  assert(custom_diagnostics.rows[0][0]->value == "north");
  assert(custom_diagnostics.rows[1][0]->value == "south");

  bool threw = false;
  try {
    (void)declare_conformance_diagnostics(source, {results.front()});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    DeclareDiagnosticsOptions missing;
    missing.case_id_key = "case:missing";
    (void)declare_conformance_diagnostics(source, results, missing);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void custom_flat_selectors_have_precedence_and_group_cases() {
  const auto make_table = [](bool custom_activity_first,
                             bool custom_case_first) {
    EventTable table;
    const auto activities =
        custom_activity_first
            ? std::vector<std::string>{"phase", "concept:name"}
            : std::vector<std::string>{"concept:name", "phase"};
    const auto cases =
        custom_case_first
            ? std::vector<std::string>{"case:batch", "case:concept:name"}
            : std::vector<std::string>{"case:concept:name", "case:batch"};
    table.columns = activities;
    table.columns.insert(table.columns.end(), cases.begin(), cases.end());
    const auto append = [&](const char* case_id, const char* activity) {
      EventTable::Row row(table.columns.size());
      for (std::size_t index = 0; index < table.columns.size(); ++index) {
        const auto& column = table.columns[index];
        const auto value = column == "phase"
                               ? std::string(activity)
                               : (column == "case:batch"
                                      ? std::string(case_id)
                                      : (column == "concept:name"
                                             ? std::string("wrong")
                                             : std::string("wrong-case")));
        EventAttribute field{column, value};
        if (column == "phase") {
          field.first_class_role = FirstClassAttributeRole::kLifecycle;
          field.first_class_state = FirstClassAttributeState::kPresent;
          field.first_class_source_key = column;
        } else if (column == "concept:name") {
          field.first_class_role = FirstClassAttributeRole::kActivity;
          field.first_class_state = FirstClassAttributeState::kPresent;
          field.first_class_source_key = column;
        } else if (column == "case:concept:name") {
          field.first_class_role = FirstClassAttributeRole::kCaseId;
          field.first_class_state = FirstClassAttributeState::kPresent;
          field.first_class_source_key = column;
        }
        row[index] = std::move(field);
      }
      table.rows.push_back(std::move(row));
    };
    append("c1", "A");
    append("c1", "B");
    append("c2", "A");
    return table;
  };

  DeclareDiscoveryOptions expected_options;
  expected_options.allowed_templates = {kDeclareExistence,
                                         kDeclareResponse};
  expected_options.has_min_support_ratio = true;
  expected_options.has_min_confidence_ratio = true;
  const auto expected_log = log({{"A", "B"}, {"A"}});
  const auto expected = discover_declare(expected_log, expected_options);

  DeclareDiscoveryOptions selected_options = expected_options;
  selected_options.activity_key = "phase";
  selected_options.case_id_key = "case:batch";
  DeclareConformanceOptions conformance_options;
  conformance_options.activity_key = "phase";
  conformance_options.case_id_key = "case:batch";

  for (const bool custom_first : {false, true})
    for (const bool custom_case_first : {false, true}) {
      const auto table = make_table(custom_first, custom_case_first);
      const auto stream = event_table_to_event_stream(table);
      const auto lazy = LazyEventTable::from_table(with_time(table));
      assert(discover_declare(table, selected_options).rules == expected.rules);
      assert(discover_declare(stream, selected_options).rules == expected.rules);
      assert(discover_declare(lazy, selected_options).rules == expected.rules);
      const auto results =
          check_declare_conformance(table, expected, conformance_options);
      assert(results.size() == 2);
      assert(check_declare_conformance(stream, expected, conformance_options)
                 .size() == results.size());
      assert(check_declare_conformance(lazy, expected, conformance_options)
                 .size() == results.size());
      DeclareDiagnosticsOptions diagnostics_options;
      diagnostics_options.case_id_key = "case:batch";
      const auto diagnostics = declare_conformance_diagnostics(
          table, results, diagnostics_options);
      assert(diagnostics.rows[0][0]->value == "c1");
      assert(diagnostics.rows[1][0]->value == "c2");
    }

  auto typed = make_table(false, false);
  const auto case_column = static_cast<std::size_t>(
      std::find(typed.columns.begin(), typed.columns.end(), "case:batch") -
      typed.columns.begin());
  for (auto& row : typed.rows) {
    auto& cell = *row[case_column];
    cell.value = cell.value == "c1" ? "7" : "8";
    cell.kind = AttributeKind::kInt;
    cell.typed_value = static_cast<std::int64_t>(cell.value == "7" ? 7 : 8);
  }
  const auto typed_results =
      check_declare_conformance(typed, expected, conformance_options);
  DeclareDiagnosticsOptions diagnostics_options;
  diagnostics_options.case_id_key = "case:batch";
  const auto typed_diagnostics = declare_conformance_diagnostics(
      typed, typed_results, diagnostics_options);
  assert(typed_diagnostics.rows[0][0]->kind == AttributeKind::kInt);
  assert(std::get<std::int64_t>(typed_diagnostics.rows[0][0]->typed_value) ==
         7);

  const auto expect_rejected = [](auto&& action) {
    bool threw = false;
    try {
      action();
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  };
  auto null_table = make_table(false, false);
  const auto phase_column = static_cast<std::size_t>(
      std::find(null_table.columns.begin(), null_table.columns.end(), "phase") -
      null_table.columns.begin());
  null_table.rows[0][phase_column]->is_null = true;
  null_table.rows[0][phase_column]->first_class_state =
      FirstClassAttributeState::kNull;
  const auto null_stream = event_table_to_event_stream(null_table);
  const auto null_lazy = LazyEventTable::from_table(with_time(null_table));
  expect_rejected(
      [&] { (void)discover_declare(null_table, selected_options); });
  expect_rejected(
      [&] { (void)discover_declare(null_stream, selected_options); });
  expect_rejected(
      [&] { (void)discover_declare(null_lazy, selected_options); });

  EventTable ambiguous;
  ambiguous.columns = {"resource_value", "timestamp_value", "case:batch"};
  EventAttribute resource{"resource_value", "A"};
  resource.first_class_role = FirstClassAttributeRole::kResource;
  resource.first_class_state = FirstClassAttributeState::kPresent;
  resource.first_class_source_key = "worker";
  EventAttribute timestamp{"timestamp_value", "B"};
  timestamp.first_class_role = FirstClassAttributeRole::kTimestamp;
  timestamp.first_class_state = FirstClassAttributeState::kPresent;
  timestamp.first_class_source_key = "worker";
  EventAttribute case_id{"case:batch", "c1"};
  ambiguous.rows.push_back(
      {std::move(resource), std::move(timestamp), std::move(case_id)});
  auto ambiguous_options = selected_options;
  ambiguous_options.activity_key = "worker";
  const auto ambiguous_stream = event_table_to_event_stream(ambiguous);
  const auto ambiguous_lazy = LazyEventTable::from_table(ambiguous);
  expect_rejected(
      [&] { (void)discover_declare(ambiguous, ambiguous_options); });
  expect_rejected(
      [&] { (void)discover_declare(ambiguous_stream, ambiguous_options); });
  expect_rejected(
      [&] { (void)discover_declare(ambiguous_lazy, ambiguous_options); });
}

// Round FU (pm4py 2.7.22 discover_declare on a frame and on its Polars
// copy): the Polars projection sorts by case, "time:timestamp" and row, so a
// case's events follow their times there and their frame order in pandas.
void polars_frame_reads_cases_in_time_order() {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  for (const auto& [case_id, activity, second] :
       std::vector<std::tuple<const char*, const char*, const char*>>{
           {"c2", "B", "02"}, {"c2", "A", "01"}, {"c1", "A", "05"}, {"c1", "A", "03"}})
    table.rows.push_back({EventAttribute{"case:concept:name", case_id},
                          EventAttribute{"concept:name", activity},
                          EventAttribute{"time:timestamp", std::string("2020-01-01T00:00:") + second + "Z",
                                         AttributeKind::kDate}});
  DeclareDiscoveryOptions discovery;
  discovery.allowed_templates = {kDeclareInit, kDeclareResponse};
  const auto pandas = discover_declare(table, discovery);
  assert(pandas.rules.at(kDeclareInit).empty());
  assert(pandas.rules.at(kDeclareResponse).size() == 1);
  assert(pandas.rules.at(kDeclareResponse)[0].first == "B" &&
         pandas.rules.at(kDeclareResponse)[0].second == "A");
  const auto polars = discover_declare(LazyEventTable::from_table(table), discovery);
  assert(polars.rules.at(kDeclareResponse).empty());
  assert(polars.rules.at(kDeclareInit).size() == 1);
  assert(polars.rules.at(kDeclareInit)[0].first == "A");
  assert(polars.rules.at(kDeclareInit)[0].support == 2 && polars.rules.at(kDeclareInit)[0].confidence == 2);
}

// Round FV: pm4py's Polars projection sorts by a frame's own "@@index"
// column instead of a row count (ties then follow Polars' unstable pdqsort),
// and refuses a missing case id, then a missing activity. Reference values
// from pm4py 2.7.22 (review-harness round-fv ut/decut.py).
EventTable fv_tie_frame(std::optional<std::vector<int>> index) {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  table.dtypes = {"str", "str", "datetime64[us, UTC]"};
  if (index) {
    table.columns.push_back("@@index");
    table.dtypes.push_back("int64");
  }
  const std::vector<std::tuple<const char*, const char*, const char*>> rows{
      {"c0", "C", "0"}, {"c1", "A", "1"}, {"c1", "B", "1"}, {"c0", "A", "1"}, {"c0", "B", "1"},
      {"c2", "A", "1"}, {"c1", "C", "0"}, {"c2", "A", "1"}, {"c0", "A", "0"}, {"c2", "C", "0"},
      {"c1", "C", "0"}, {"c1", "C", "0"}, {"c2", "A", "1"}, {"c1", "C", "0"}, {"c1", "A", "0"},
      {"c1", "B", "0"}, {"c1", "C", "0"}, {"c0", "C", "1"}, {"c0", "C", "1"}, {"c2", "C", "1"},
      {"c2", "C", "0"}, {"c1", "B", "1"}};
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const auto& [case_id, activity, second] = rows[i];
    EventTable::Row row{EventAttribute{"case:concept:name", case_id},
                        EventAttribute{"concept:name", activity},
                        EventAttribute{"time:timestamp", std::string("2020-01-01T00:00:0") + second + "Z",
                                       AttributeKind::kDate}};
    if (index) row.push_back(EventAttribute{"@@index", std::to_string((*index)[i]), AttributeKind::kInt});
    table.rows.push_back(std::move(row));
  }
  return table;
}

double fv_confidence(const DeclareModel& model, const char* family, const char* first,
                     const char* second = nullptr) {
  for (const auto& rule : model.rules.at(family))
    if (rule.first == first && (!second || rule.second == second)) return rule.confidence;
  return -1;
}

void polars_frame_sorts_by_its_own_index() {
  DeclareDiscoveryOptions discovery;
  discovery.allowed_templates = {kDeclareInit, kDeclareChainResponse, kDeclareChainPrecedence};
  discovery.min_support_ratio = 0.0;
  discovery.min_confidence_ratio = 0.0;
  discovery.has_min_support_ratio = discovery.has_min_confidence_ratio = true;
  const auto plain = discover_declare(LazyEventTable::from_table(fv_tie_frame(std::nullopt)), discovery);
  assert(fv_confidence(plain, kDeclareInit, "C") == 3);
  assert(fv_confidence(plain, kDeclareChainResponse, "A", "B") == 1);
  assert(fv_confidence(plain, kDeclareChainResponse, "B", "C") == 1);
  // Every row ties on the index: the pdqsort moves the 22 rows.
  const auto zeros = discover_declare(
      LazyEventTable::from_table(fv_tie_frame(std::vector<int>(22, 0))), discovery);
  assert(fv_confidence(zeros, kDeclareInit, "A") == 1);
  assert(fv_confidence(zeros, kDeclareInit, "B") == 1);
  assert(fv_confidence(zeros, kDeclareInit, "C") == 1);
  assert(fv_confidence(zeros, kDeclareChainResponse, "A", "B") == 0);
  assert(fv_confidence(zeros, kDeclareChainResponse, "B", "C") == 0);
  std::vector<int> descending(22);
  for (int i = 0; i < 22; ++i) descending[i] = 22 - i;
  const auto desc = discover_declare(LazyEventTable::from_table(fv_tie_frame(descending)), discovery);
  assert(fv_confidence(desc, kDeclareInit, "A") == 1);
  assert(fv_confidence(desc, kDeclareInit, "B") == 0);
  assert(fv_confidence(desc, kDeclareInit, "C") == 2);
  assert(fv_confidence(desc, kDeclareChainResponse, "B", "A") == 1);
  assert(fv_confidence(desc, kDeclareChainPrecedence, "B", "A") == 1);
  assert(fv_confidence(desc, kDeclareChainPrecedence, "C", "B") == 1);

  const auto refused = [&](EventTable table, const std::string& message) {
    try {
      (void)discover_declare(LazyEventTable::from_table(table), discovery);
    } catch (const std::invalid_argument& error) {
      assert(error.what() == message);
      return;
    }
    assert(false);
  };
  auto missing_case = fv_tie_frame(std::nullopt);
  missing_case.rows[3][0]->is_null = true;
  missing_case.rows[5][1]->is_null = true;
  refused(missing_case, "the case ID column should not contain any empty value.");
  auto missing_activity = fv_tie_frame(std::nullopt);
  missing_activity.rows[5][1]->is_null = true;
  refused(missing_activity, "the activity column should not contain any empty value.");
  // The frame checks come first: an XES upload whose events carry no
  // concept:name leaves one text column of three (pm4py's message, observed
  // black-box), and an integer activity column among three text ones.
  auto no_activity = fv_tie_frame(std::vector<int>(22, 0));
  no_activity.columns.erase(no_activity.columns.begin() + 1);
  no_activity.dtypes.erase(no_activity.dtypes.begin() + 1);
  for (auto& row : no_activity.rows) row.erase(row.begin() + 1);
  refused(no_activity,
          "the dataframe should (at least) contain a column of type string for the case identifier and a column "
          "of type string for the activity.");
  auto int_activity = fv_tie_frame(std::nullopt);
  int_activity.columns.push_back("org:resource");
  int_activity.dtypes.push_back("str");
  int_activity.dtypes[1] = "int64";
  for (auto& row : int_activity.rows) {
    row[1] = EventAttribute{"concept:name", "1", AttributeKind::kInt};
    row.push_back(EventAttribute{"org:resource", "r"});
  }
  refused(int_activity, "the activity column should be of type string.");
  // A categorical activity (pandas category, Polars Categorical) is text to
  // Polars, so the frame passes and mines as the plain one does.
  auto categorical = fv_tie_frame(std::nullopt);
  categorical.dtypes[1] = "category";
  const auto category_model = discover_declare(LazyEventTable::from_table(categorical), discovery);
  assert(category_model.rules == plain.rules);
}
// Round FW: pm4py's Declare algorithm on a pandas frame checks the frame
// (three columns, two text, one date), reads the case, activity and
// "time:timestamp" columns (KeyError, in that order), runs np.unique on the
// case column in frame order (numpy's stable object argsort raises Python's
// TypeError on a missing case id among named ones) and, in discovery, sorts
// rule columns that hold a missing activity (TypeError). Its conformance
// keeps a row without an activity as an event no rule names. Reference
// values from pm4py 2.7.22 (review-harness round-fw dcpandas.py,
// tecase2.py, teconfnan.py).
EventTable fw_pandas_frame(const std::vector<std::pair<std::optional<std::string>, std::optional<std::string>>>& rows,
                           bool time = true) {
  EventTable table;
  table.columns = {"case:concept:name", "concept:name", time ? "time:timestamp" : "ts"};
  table.dtypes = {"object", "object", "datetime64[ns, UTC]"};
  for (std::size_t i = 0; i < rows.size(); ++i) {
    EventTable::Row row(3);
    if (rows[i].first) row[0] = EventAttribute{"case:concept:name", *rows[i].first};
    if (rows[i].second) row[1] = EventAttribute{"concept:name", *rows[i].second};
    row[2] = EventAttribute{table.columns[2], "2021-03-01T08:00:0" + std::to_string(i % 10) + "Z", AttributeKind::kDate};
    table.rows.push_back(std::move(row));
  }
  return table;
}

std::string fw_declare_error(const EventTable& table, bool discovery) {
  try {
    if (discovery) {
      (void)discover_declare_frame_slices(EventDataView(table), {});
    } else {
      DeclareModel model;
      model.rules[kDeclareResponse].push_back({"a", "b", true, 1, 1});
      (void)check_declare_conformance_frame_slices(EventDataView(table), model, {});
    }
  } catch (const std::exception& error) {
    return error.what();
  }
  return "";
}

void pandas_frame_runs_the_algorithm_checks() {
  using Rows = std::vector<std::pair<std::optional<std::string>, std::optional<std::string>>>;
  const std::string sf = "'<' not supported between instances of 'str' and 'float'";
  const std::string fs = "'<' not supported between instances of 'float' and 'str'";
  const auto c = [](const char* value) { return std::optional<std::string>(value); };
  const std::optional<std::string> none;
  // The frame check: two columns are too few.
  auto narrow = fw_pandas_frame({{c("c1"), c("a")}});
  narrow.columns.erase(narrow.columns.begin());
  narrow.dtypes.erase(narrow.dtypes.begin());
  for (auto& row : narrow.rows) row.erase(row.begin());
  for (const bool discovery : {true, false})
    assert(fw_declare_error(narrow, discovery) ==
           "the dataframe should (at least) contain a column for the case identifier, a column for the activity and a column for the timestamp.");
  // KeyErrors: the case column, then the activity column, then time:timestamp.
  auto renamed = fw_pandas_frame({{c("c1"), c("a")}});
  renamed.columns[0] = "org:resource";
  renamed.columns[1] = "org:role";
  assert(fw_declare_error(renamed, true) == "KeyError: 'case:concept:name'");
  renamed.columns[0] = "case:concept:name";
  assert(fw_declare_error(renamed, false) == "KeyError: 'concept:name'");
  const auto no_time = fw_pandas_frame({{c("c1"), c("a")}, {none, none}}, false);
  for (const bool discovery : {true, false}) assert(fw_declare_error(no_time, discovery) == "KeyError: 'time:timestamp'");
  // np.unique's stable argsort over the case column in frame order.
  const std::vector<std::pair<Rows, std::string>> cases{
      {{{c("c1"), c("a")}, {none, c("a")}}, sf},
      {{{none, c("a")}, {c("c1"), c("a")}}, fs},
      {{{c("c1"), c("a")}, {c("c2"), c("a")}, {none, c("a")}}, sf},
      {{{c("c2"), c("a")}, {c("c1"), c("a")}, {none, c("a")}}, fs},
      {{{c("c2"), c("a")}, {none, c("a")}, {c("c1"), c("a")}}, sf},
      {{{c("b"), c("a")}, {c("a"), c("a")}, {c("c"), c("a")}, {none, c("a")}}, fs},
      {{{c("a"), c("a")}, {c("b"), c("a")}, {c("c"), c("a")}, {c("d"), c("a")}, {none, c("a")}}, sf}};
  for (const auto& [rows, message] : cases)
    for (const bool discovery : {true, false}) assert(fw_declare_error(fw_pandas_frame(rows), discovery) == message);
  // A missing activity: discovery raises (Python's TypeError, round GZ),
  // conformance reads an event no rule names.
  const auto gap = fw_pandas_frame({{c("c1"), c("b")}, {c("c1"), none}, {c("c1"), c("c")}});
  assert(fw_declare_error(gap, true) == "TypeError: " + sf);
  DeclareModel model;
  model.rules[kDeclareChainResponse].push_back({"b", "c", true, 1, 1});
  model.rules[kDeclareChainPrecedence].push_back({"b", "c", true, 1, 1});
  model.rules[kDeclareInit].push_back({"b", "", false, 1, 1});
  model.rules[kDeclareExactlyOne].push_back({"b", "", false, 1, 1});
  model.rules[kDeclareAbsence].push_back({"x", "", false, 1, 1});
  const auto result = check_declare_conformance_frame_slices(EventDataView(gap), model, {});
  assert(result.size() == 1 && result[0].no_constr_total == 5 && result[0].no_dev_total == 2);
  assert(result[0].dev_fitness == 0.6 && !result[0].is_fit);
  const auto leading = check_declare_conformance_frame_slices(
      EventDataView(fw_pandas_frame({{c("c1"), none}, {c("c1"), c("b")}, {c("c1"), c("c")}})), model, {});
  assert(leading.size() == 1 && leading[0].no_dev_total == 1 && leading[0].dev_fitness == 0.8);
  // A frame without gaps answers as before.
  assert(fw_declare_error(fw_pandas_frame({{c("c1"), c("a")}, {c("c1"), c("b")}, {c("c2"), c("a")}}), true).empty());
  // An XES upload's frame has no column for an attribute no event carries:
  // with every trace unnamed the column count fails, or the case column is
  // missing when another text column makes up the count.
  const std::string too_few =
      "the dataframe should (at least) contain a column for the case identifier, a column for the activity and a column for the timestamp.";
  const auto unnamed = fw_pandas_frame({{none, c("a")}, {none, c("b")}});
  for (const bool discovery : {true, false}) assert(fw_declare_error(unnamed, discovery) == too_few);
  auto unnamed_resource = unnamed;
  unnamed_resource.columns.push_back("org:resource");
  unnamed_resource.dtypes.push_back("object");
  for (auto& row : unnamed_resource.rows) row.push_back(EventAttribute{"org:resource", "r"});
  for (const bool discovery : {true, false})
    assert(fw_declare_error(unnamed_resource, discovery) == "KeyError: 'case:concept:name'");
  // The Polars frame of an XES upload follows the same column rule; a CSV
  // frame (the generic path) keeps every header column, so a header-only CSV
  // answers with an empty model.
  const auto lazy_error = [](const EventTable& table, bool xes) -> std::string {
    try {
      if (xes) check_declare_xes_polars_frame(LazyEventTable::from_table(table), {});
      else (void)discover_declare(LazyEventTable::from_table(table), {});
    } catch (const std::exception& error) {
      return error.what();
    }
    return "";
  };
  assert(lazy_error(unnamed, true) == too_few);
  assert(lazy_error(unnamed, false) != too_few);
  {
    const auto lazy = LazyEventTable::from_table(unnamed);
    std::string message;
    try {
      (void)discover_declare_frame_slices(EventDataView(lazy), {});
    } catch (const std::exception& error) {
      message = error.what();
    }
    assert(message == too_few);
  }
  auto header_only = fw_pandas_frame({});
  assert(lazy_error(header_only, true) == too_few);
  assert(lazy_error(header_only, false).empty());
}

}  // namespace


int main() {
  case_provenance_does_not_shadow_event_activity();
  all_templates_are_named_and_filterable();
  duplicate_traces_and_thresholds_are_weighted();
  conformance_checks_direction_and_order();
  self_pairs_follow_reference_comparisons();
  vacuity_and_empty_inputs();
  every_template_family_and_prerequisite_rules();
  directional_vacuity_and_negative_conformance();
  explicit_empty_and_unusual_activity_keys();
  threshold_edge_values_follow_positive_predicates();
  conformance_deviation_order_is_fixed();
  missing_exactly_one_activity_does_not_create_empty_occurrences();
  custom_activity_selector_is_strict();
  blank_activity_selectors_reject_empty_inputs();
  grouped_inputs_ignore_flat_case_selector();
  first_class_activity_selectors_are_supported();
  activity_selector_precedence_and_ambiguity_are_strict();
  flat_inputs_and_diagnostics_are_equivalent();
  custom_flat_selectors_have_precedence_and_group_cases();
  polars_frame_reads_cases_in_time_order();
  polars_frame_sorts_by_its_own_index();
  pandas_frame_runs_the_algorithm_checks();
  std::cout << "declare tests passed\n";
}
