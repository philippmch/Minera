#include "minera/log_skeleton.hpp"

#include <cassert>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace minera;

namespace {
EventLog make_log(std::initializer_list<std::initializer_list<const char*>> rows) {
  EventLog log;
  for (const auto& row : rows) {
    Trace trace;
    for (const auto* activity : row) trace.events.push_back(Event{"", activity});
    log.traces.push_back(std::move(trace));
  }
  return log;
}

bool has(const std::vector<LogSkeletonPair>& relations, const char* source,
         const char* target) {
  for (const auto& relation : relations)
    if (relation.source == source && relation.target == target) return true;
  return false;
}

EventLog attribute_activity_log(bool include_missing = false,
                                bool include_null = false) {
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
  LogSkeletonOptions options;
  options.activity_key = "kind";
  const auto custom_model = discover_log_skeleton(attribute_activity_log(), options);
  assert(has(custom_model.directly_follows, "A", "B"));
  const auto projected_model = discover_log_skeleton(projected_activity_log(), options);
  assert(has(projected_model.directly_follows, "A", "B"));

  assert(check_log_skeleton_conformance(attribute_activity_log().traces[0],
                                        custom_model, options)
             .is_fit);
  const auto event_log_result = check_log_skeleton_conformance(
      attribute_activity_log(), custom_model, options);
  assert(event_log_result.size() == 1 && event_log_result[0].is_fit);

  bool missing_threw = false;
  try {
    (void)discover_log_skeleton(attribute_activity_log(true), options);
  } catch (const std::invalid_argument&) {
    missing_threw = true;
  }
  assert(missing_threw);
  missing_threw = false;
  try {
    (void)check_log_skeleton_conformance(attribute_activity_log(true).traces[0],
                                         custom_model, options);
  } catch (const std::invalid_argument&) {
    missing_threw = true;
  }
  assert(missing_threw);
  missing_threw = false;
  try {
    (void)check_log_skeleton_conformance(attribute_activity_log(true), custom_model,
                                         options);
  } catch (const std::invalid_argument&) {
    missing_threw = true;
  }
  assert(missing_threw);

  bool null_threw = false;
  try {
    (void)discover_log_skeleton(attribute_activity_log(false, true), options);
  } catch (const std::invalid_argument&) {
    null_threw = true;
  }
  assert(null_threw);
}

void blank_activity_selectors_reject_empty_inputs() {
  LogSkeletonOptions options;
  options.activity_key = " \t ";
  bool threw = false;
  try {
    (void)discover_log_skeleton(EventLog{}, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  threw = false;
  try {
    (void)check_log_skeleton_conformance(
        EventLog{}, LogSkeletonModel{}, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  threw = false;
  try {
    (void)check_log_skeleton_conformance(
        Trace{}, LogSkeletonModel{}, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void first_class_activity_selectors_are_supported() {
  for (const std::string selector : {"org:resource", "time:timestamp",
                                     "lifecycle:transition", "worker", "when",
                                     "phase"}) {
    LogSkeletonOptions options;
    options.activity_key = selector;
    const auto selected = first_class_activity_log(selector);
    const auto model = discover_log_skeleton(selected, options);
    assert(has(model.directly_follows, "A", "B"));
    assert(check_log_skeleton_conformance(selected.traces[0], model, options)
               .is_fit);
    const auto log_result =
        check_log_skeleton_conformance(selected, model, options);
    assert(log_result.size() == 1 && log_result[0].is_fit);
  }

  LogSkeletonOptions options;
  options.activity_key = "org:resource";
  bool threw = false;
  try {
    (void)discover_log_skeleton(first_class_activity_log("org:resource", true),
                                options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)discover_log_skeleton(
        first_class_activity_log("org:resource", false, true), options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  const auto model = discover_log_skeleton(
      first_class_activity_log("org:resource"), options);
  threw = false;
  try {
    (void)check_log_skeleton_conformance(
        first_class_activity_log("org:resource", true).traces[0], model,
        options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)check_log_skeleton_conformance(
        first_class_activity_log("org:resource", false, true), model,
        options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void considered_constraints_have_presence_semantics() {
  const auto family_log = make_log({{"A", "B"}, {"A"}});
  LogSkeletonOptions ignored_by_discovery;
  ignored_by_discovery.noise_threshold = 0.5;
  ignored_by_discovery.considered_constraints_present = true;
  const auto all_families =
      discover_log_skeleton(family_log, ignored_by_discovery);
  // Discovery always materializes all six families; the selector is a
  // conformance-only control.
  assert(!all_families.equivalence.empty());
  assert(!all_families.always_after.empty());
  assert(!all_families.always_before.empty());
  assert(!all_families.never_together.empty());
  assert(!all_families.directly_follows.empty());
  assert(!all_families.activ_freq.empty());

  LogSkeletonOptions omitted;
  const auto omitted_result = check_log_skeleton_conformance(
      std::vector<std::string>{"A", "B"}, all_families, omitted);
  assert(omitted_result.no_constr_total > 0);

  LogSkeletonOptions explicit_empty;
  explicit_empty.considered_constraints_present = true;
  const auto empty_result = check_log_skeleton_conformance(
      std::vector<std::string>{"A", "B"}, all_families, explicit_empty);
  assert(empty_result.no_constr_total == 0 && empty_result.is_fit);

  LogSkeletonOptions subset;
  subset.considered_constraints = {"directly_follows"};
  const auto subset_result = check_log_skeleton_conformance(
      std::vector<std::string>{"B", "A"}, all_families, subset);
  assert(subset_result.no_constr_total == 1 && !subset_result.is_fit);
}

void directly_follows_preserves_repeated_edge_support() {
  LogSkeletonOptions options;
  options.noise_threshold = 0.25;
  const auto model = discover_log_skeleton(
      make_log({{"A", "B", "A", "B"}}), options);
  // A->B occurs twice while A occurs twice.  At this threshold, collapsing
  // the edge to one occurrence would incorrectly discard the relation.
  assert(has(model.directly_follows, "A", "B"));
}

void thresholds_use_binary64_arithmetic() {
  EventLog boundary;
  for (int i = 0; i < 10; ++i) {
    Trace trace;
    trace.events.push_back(Event{"", "A"});
    if (i < 7) trace.events.push_back(Event{"", "B"});
    boundary.traces.push_back(std::move(trace));
  }
  LogSkeletonOptions options;
  options.noise_threshold = 0.3;
  const auto model = discover_log_skeleton(boundary, options);
  // Seven supporting variants out of ten meet 10 * (1 - 0.3) exactly in
  // binary64 arithmetic.  Extended-precision intermediate arithmetic would
  // place the threshold just above seven and incorrectly discard the pair.
  assert(has(model.always_after, "A", "B"));
}
}  // namespace

int main() {
  const auto base = discover_log_skeleton(make_log({{"A", "B", "A"}, {"A", "B"}}));
  assert(has(base.equivalence, "A", "B") == false);
  assert(!has(base.always_after, "A", "B"));
  assert(has(base.always_before, "B", "A"));
  assert(!has(base.directly_follows, "A", "B"));
  assert(has(discover_log_skeleton(make_log({{"A", "B"}})).directly_follows,
             "A", "B"));
  assert(base.activ_freq.at("A") == std::vector<std::size_t>({1, 2}));
  assert(base.activ_freq.at("B") == std::vector<std::size_t>({1}));

  const auto never = discover_log_skeleton(make_log({{"A"}, {"B"}}));
  assert(has(never.never_together, "A", "B"));
  assert(has(never.never_together, "B", "A"));
  LogSkeletonOptions zero_noise;
  zero_noise.noise_threshold = 1.0;
  assert(!has(discover_log_skeleton(make_log({{"A", "B"}}), zero_noise)
                  .never_together,
              "A", "B"));

  // A repeated activity contributes twice to all_activs, while a relation
  // observed in one weighted variant contributes only once per variant.
  EventLog weighted = make_log({{"A", "A"}, {"A", "B"}});
  LogSkeletonOptions noisy;
  noisy.noise_threshold = 0.5;
  const auto weighted_model = discover_log_skeleton(weighted, noisy);
  assert(!has(weighted_model.always_after, "A", "A"));
  noisy.noise_threshold = 2.0 / 3.0;
  assert(has(discover_log_skeleton(weighted, noisy).always_after, "A", "A"));

  // The pinned frequency loop drops lower-support buckets once its cumulative
  // event-count threshold is reached early.
  LogSkeletonOptions frequency_noise;
  frequency_noise.noise_threshold = 0.5;
  const auto all_buckets = discover_log_skeleton(
      make_log({{"A"}, {"A"}, {"A"}, {"A", "A"}}), frequency_noise);
  assert(all_buckets.activ_freq.at("A") == std::vector<std::size_t>({1}));

  // Equal-support buckets retain first-seen variant order: [AA] precedes [A],
  // so a threshold reached by the first bucket selects frequency 2.
  LogSkeletonOptions tie_noise;
  tie_noise.noise_threshold = 0.7;
  const auto tie_model = discover_log_skeleton(make_log({{"A", "A"}, {"A"}}), tie_noise);
  assert(tie_model.activ_freq.at("A") == std::vector<std::size_t>({2}));

  // The core preserves raw threshold arithmetic; user-facing parsers
  // validate [0,1], but direct callers are not silently normalized.
  LogSkeletonOptions negative_noise;
  negative_noise.noise_threshold = -0.5;
  assert(!has(discover_log_skeleton(make_log({{"A", "B"}}), negative_noise)
                  .directly_follows,
              "A", "B"));
  LogSkeletonOptions nan_noise;
  nan_noise.noise_threshold = std::numeric_limits<double>::quiet_NaN();
  assert(discover_log_skeleton(make_log({{"A", "B"}}), nan_noise)
             .directly_follows.empty());

  const auto fit = check_log_skeleton_conformance(
      std::vector<std::string>{"A", "B", "A"}, base);
  assert(fit.is_fit);
  assert(fit.no_dev_total == 0);
  const auto deviating = check_log_skeleton_conformance(
      std::vector<std::string>{"B", "A", "B"}, base);
  assert(!deviating.is_fit && deviating.no_dev_total > 0);

  // Unknown activities and missing positive frequencies are diagnostics.
  LogSkeletonModel frequency_model;
  frequency_model.activ_freq["A"] = {1};
  const auto unknown = check_log_skeleton_conformance(
      std::vector<std::string>{"A", "X"}, frequency_model);
  assert(!unknown.is_fit && unknown.no_dev_total == 1 &&
         unknown.deviations.front().observed_frequency == 0);
  const auto missing = check_log_skeleton_conformance(
      std::vector<std::string>{"X"}, frequency_model);
  assert(missing.no_dev_total == 2);  // X unknown, A required but absent

  // Family filtering and source-absent vacuity.
  LogSkeletonOptions only_after;
  only_after.considered_constraints = {"always_after"};
  const auto filtered = check_log_skeleton_conformance(
      std::vector<std::string>{"C"}, base, only_after);
  assert(filtered.no_constr_total == 0);  // source-absent constraints are vacuous
  assert(filtered.is_fit);

  const auto duplicate_results = check_log_skeleton_conformance(
      make_log({{"A", "B", "A"}, {"A", "B", "A"}}), base);
  assert(duplicate_results.size() == 2 && duplicate_results[0].is_fit &&
         duplicate_results[1].is_fit);
  const auto empty_model = discover_log_skeleton(EventLog{});
  assert(empty_model.equivalence.empty() && empty_model.activ_freq.empty());
  assert(check_log_skeleton_conformance(std::vector<std::string>{}, empty_model)
             .is_fit);
  assert(check_log_skeleton_conformance(make_log({{}}), empty_model)[0].is_fit);
  custom_activity_selector_is_strict();
  blank_activity_selectors_reject_empty_inputs();
  considered_constraints_have_presence_semantics();
  directly_follows_preserves_repeated_edge_support();
  thresholds_use_binary64_arithmetic();
  first_class_activity_selectors_are_supported();
}
