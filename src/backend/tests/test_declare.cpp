#include "minera/declare.hpp"
#include "minera/log_conversion.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>

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
  const auto lazy = LazyEventTable::from_table(table);

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
      const auto lazy = LazyEventTable::from_table(table);
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
  const auto null_lazy = LazyEventTable::from_table(null_table);
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
  custom_activity_selector_is_strict();
  blank_activity_selectors_reject_empty_inputs();
  grouped_inputs_ignore_flat_case_selector();
  first_class_activity_selectors_are_supported();
  activity_selector_precedence_and_ambiguity_are_strict();
  flat_inputs_and_diagnostics_are_equivalent();
  custom_flat_selectors_have_precedence_and_group_cases();
  std::cout << "declare tests passed\n";
}
