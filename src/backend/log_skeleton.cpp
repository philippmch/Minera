#include "minera/log_skeleton.hpp"

#include "minera/log_conversion.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace minera {
namespace {

using Activities = std::vector<std::string>;
using CountMap = std::map<std::string, std::size_t>;
using VariantMap = std::map<Activities, std::size_t>;

void validate_activity_key(std::string_view activity_key) {
  if (activity_key.empty() ||
      std::all_of(activity_key.begin(), activity_key.end(), [](char value) {
        return std::isspace(static_cast<unsigned char>(value)) != 0;
      })) {
    throw std::invalid_argument(
        "log skeleton: activity selector must not be blank");
  }
}

void validate_case_selector_key(std::string_view case_id_key) {
  if (case_id_key.empty() ||
      std::all_of(case_id_key.begin(), case_id_key.end(), [](char value) {
        return std::isspace(static_cast<unsigned char>(value)) != 0;
      })) {
    throw std::invalid_argument(
        "log skeleton: case-id selector must not be blank");
  }
}

bool considered(const LogSkeletonOptions& options, const std::string& name) {
  return (!options.considered_constraints_present &&
          options.considered_constraints.empty()) ||
         std::find(options.considered_constraints.begin(),
                   options.considered_constraints.end(), name) !=
             options.considered_constraints.end();
}

double threshold(std::size_t denominator, double noise) {
  return static_cast<double>(denominator) * (1.0 - noise);
}

const std::string* first_class_value(
    const std::string& value, FirstClassAttributeState state) {
  if (state == FirstClassAttributeState::kNull) return nullptr;
  if (state != FirstClassAttributeState::kMissing || !value.empty())
    return &value;
  return nullptr;
}

const std::string* resolve_event_field(const Event& event,
                                       std::string_view selector) {
  const auto resolve = [](const std::string& value,
                          FirstClassAttributeState state,
                          const std::optional<EventAttribute>& attribute)
      -> const std::string* {
    if (state == FirstClassAttributeState::kNull) return nullptr;
    if (const auto* selected = first_class_value(value, state))
      return selected;
    if (attribute && !attribute->is_null) return &attribute->value;
    return nullptr;
  };

  std::vector<const std::string*> candidates;
  int best_priority = 0;
  const auto consider = [&](int priority, const std::string* value) {
    if (priority < best_priority) return;
    if (priority > best_priority) {
      best_priority = priority;
      candidates.clear();
    }
    candidates.push_back(value);
  };
  const auto consider_first_class =
      [&](std::string_view source_key, const std::string& value,
          FirstClassAttributeState state,
          const std::optional<EventAttribute>& attribute, bool alias) {
        const bool source_match =
            (!source_key.empty() && source_key == selector) ||
            (attribute && attribute->key == selector);
        if (!source_match && !alias) return;
        consider(source_match ? 2 : 1, resolve(value, state, attribute));
      };

  for (const auto& attribute : event.attributes)
    if (attribute.key == selector)
      consider(3, attribute.is_null ? nullptr : &attribute.value);
  consider_first_class(
      event.activity_source_key, event.activity, event.activity_state,
      event.activity_attribute,
      selector == "concept:name" || selector == "activity");
  consider_first_class(
      event.timestamp_source_key, event.timestamp, event.timestamp_state,
      event.timestamp_attribute,
      selector == "time:timestamp" || selector == "timestamp");
  consider_first_class(
      event.resource_source_key, event.resource, event.resource_state,
      event.resource_attribute,
      selector == "org:resource" || selector == "resource");
  consider_first_class(
      event.lifecycle_source_key, event.lifecycle, event.lifecycle_state,
      event.lifecycle_attribute,
      selector == "lifecycle:transition" || selector == "lifecycle");
  const bool case_source = !event.case_id_source_key.empty() &&
                           event.case_id_source_key == selector;
  const bool case_alias = selector == "case:concept:name" ||
                          selector == "case:id" || selector == "case_id" ||
                          selector == "case";
  if (case_source || case_alias)
    consider(case_source ? 2 : 1,
             first_class_value(event.case_id, event.case_id_state));

  if (candidates.size() > 1)
    throw std::invalid_argument(
        "log skeleton: event selector is ambiguous: '" +
        std::string(selector) + "'");
  return candidates.empty() ? nullptr : candidates.front();
}

std::string selected_activity(const Event& event, std::string_view activity_key) {
  validate_activity_key(activity_key);
  if (const auto* value = resolve_event_field(event, activity_key)) return *value;
  throw std::invalid_argument(
      "log skeleton: event activity selector is missing or null: '" +
      std::string(activity_key) + "'");
}

Activities activities_of(const Trace& trace, std::string_view activity_key) {
  validate_activity_key(activity_key);
  Activities result;
  result.reserve(trace.events.size());
  for (const auto& event : trace.events)
    result.push_back(selected_activity(event, activity_key));
  return result;
}

CountMap frequencies(const Activities& trace) {
  CountMap result;
  for (const auto& activity : trace) ++result[activity];
  return result;
}

std::set<LogSkeletonPair> relation_set(const Activities& trace,
                                       const std::string& family) {
  std::set<LogSkeletonPair> result;
  const auto counts = frequencies(trace);
  if (family == "equivalence") {
    for (const auto& [a, ca] : counts)
      for (const auto& [b, cb] : counts)
        if (a != b && ca == cb) result.insert({a, b});
  } else if (family == "always_after" || family == "always_before") {
    for (std::size_t i = 0; i < trace.size(); ++i)
      for (std::size_t j = 0; j < trace.size(); ++j) {
        if (i == j) continue;
        const bool after = j > i;
        if ((family == "always_after" && after) ||
            (family == "always_before" && !after))
          result.insert({trace[i], trace[j]});
      }
  } else if (family == "never_together") {
    for (const auto& a : counts)
      for (const auto& b : counts)
        if (a.first != b.first) result.insert({a.first, b.first});
  } else if (family == "directly_follows") {
    for (std::size_t i = 1; i < trace.size(); ++i)
      result.insert({trace[i - 1], trace[i]});
  }
  return result;
}

std::vector<LogSkeletonPair> sorted_pairs(const std::set<LogSkeletonPair>& pairs) {
  return {pairs.begin(), pairs.end()};
}

int activity_selector_priority(const EventAttribute& field,
                               std::string_view selector) {
  if (field.key == selector) return 3;
  if (!field.first_class_source_key.empty() &&
      field.first_class_source_key == selector)
    return 2;
  const bool activity_alias = selector == "concept:name" || selector == "activity";
  return activity_alias &&
                 (field.key == "concept:name" || field.key == "activity")
             ? 1
             : 0;
}

int case_selector_priority(const EventAttribute& field,
                           std::string_view selector) {
  if (field.key == selector) return 3;
  if (!field.first_class_source_key.empty() &&
      field.first_class_source_key == selector)
    return 2;
  const bool case_alias = selector == "case:concept:name" ||
                          selector == "case:id" || selector == "case_id" ||
                          selector == "case";
  return case_alias &&
                 (field.key == "case:concept:name" || field.key == "case:id" ||
                  field.key == "case_id" || field.key == "case")
             ? 1
             : 0;
}

template <typename Matcher>
void promote_selected_role(std::vector<EventAttribute>& fields,
                           std::string_view selector,
                           FirstClassAttributeRole role,
                           std::string_view field_name, Matcher&& matcher) {
  struct Candidate {
    std::size_t index = 0;
    FirstClassAttributeState state = FirstClassAttributeState::kMissing;
    std::string source_key;
    bool already_projected = false;
  };
  std::vector<Candidate> candidates;
  int best_priority = 0;
  for (std::size_t index = 0; index < fields.size(); ++index) {
    const auto& field = fields[index];
    const int priority = matcher(field, selector);
    if (priority == 0) continue;
    if (priority > best_priority) {
      best_priority = priority;
      candidates.clear();
    }
    if (priority == best_priority)
      candidates.push_back({index, field.first_class_state,
                            field.first_class_source_key,
                            field.first_class_role !=
                                FirstClassAttributeRole::kNone});
  }
  if (candidates.size() > 1)
    throw std::invalid_argument("log skeleton: " + std::string(field_name) +
                                " selector is ambiguous: " +
                                std::string(selector));
  for (auto& field : fields) {
    if (field.first_class_role != role) continue;
    field.first_class_role = FirstClassAttributeRole::kNone;
    field.first_class_state = FirstClassAttributeState::kMissing;
    field.first_class_source_key.clear();
  }
  if (candidates.empty()) return;
  const auto candidate = candidates.front();
  auto& field = fields[candidate.index];
  field.first_class_role = role;
  field.first_class_state =
      candidate.already_projected
          ? candidate.state
          : (field.is_null ? FirstClassAttributeState::kNull
                           : FirstClassAttributeState::kPresent);
  field.first_class_source_key =
      candidate.source_key == selector ? candidate.source_key : field.key;
}

void promote_selected(std::vector<EventAttribute>& fields,
                      std::string_view activity_key,
                      std::string_view case_id_key) {
  promote_selected_role(fields, activity_key,
                        FirstClassAttributeRole::kActivity, "activity",
                        activity_selector_priority);
  promote_selected_role(fields, case_id_key,
                        FirstClassAttributeRole::kCaseId, "case-id",
                        case_selector_priority);
}

void normalize_row_keys(EventTable::Row& row,
                        const std::vector<std::string>& columns) {
  for (std::size_t index = 0; index < row.size() && index < columns.size();
       ++index)
    if (row[index]) row[index]->key = columns[index];
}

void promote_selected(EventRecord& record, std::string_view activity_key,
                      std::string_view case_id_key) {
  promote_selected(record.fields, activity_key, case_id_key);
}

void promote_selected(EventStream& stream, std::string_view activity_key,
                      std::string_view case_id_key) {
  for (auto& record : stream.events)
    promote_selected(record, activity_key, case_id_key);
}

void promote_selected(EventTable& table, std::string_view activity_key,
                      std::string_view case_id_key) {
  table.validate();
  for (auto& row : table.rows) {
    normalize_row_keys(row, table.columns);
    std::vector<EventAttribute> fields;
    std::vector<std::size_t> indexes;
    fields.reserve(row.size());
    indexes.reserve(row.size());
    for (std::size_t index = 0; index < row.size(); ++index) {
      if (!row[index]) continue;
      indexes.push_back(index);
      fields.push_back(*row[index]);
    }
    promote_selected(fields, activity_key, case_id_key);
    for (std::size_t index = 0; index < fields.size(); ++index)
      *row[indexes[index]] = std::move(fields[index]);
  }
}

LazyEventTable promoted_selector_table(const LazyEventTable& source,
                                       std::string activity_key,
                                       std::string case_id_key) {
  return LazyEventTable(
      source.columns(),
      [source, activity_key = std::move(activity_key),
       case_id_key = std::move(case_id_key)](
          const LazyEventTable::RowConsumer& consumer) {
        source.scan([&](const LazyEventTable::Row& input) {
          auto row = input;
          normalize_row_keys(row, source.columns());
          std::vector<EventAttribute> fields;
          std::vector<std::size_t> indexes;
          fields.reserve(row.size());
          indexes.reserve(row.size());
          for (std::size_t index = 0; index < row.size(); ++index) {
            if (!row[index]) continue;
            indexes.push_back(index);
            fields.push_back(*row[index]);
          }
          promote_selected(fields, activity_key, case_id_key);
          for (std::size_t index = 0; index < fields.size(); ++index)
            *row[indexes[index]] = std::move(fields[index]);
          if (consumer) consumer(row);
        });
      },
      source.attributes(), source.metadata(), source.row_count());
}

EventLog event_data_log(const EventDataView& data,
                        std::string_view activity_key,
                        std::string_view case_id_key) {
  validate_activity_key(activity_key);
  validate_case_selector_key(case_id_key);
  LogConversionOptions conversion;
  conversion.case_id_key = std::string(case_id_key);
  switch (data.kind()) {
    case EventDataKind::kEventLog:
      return *data.log();
    case EventDataKind::kEventStream: {
      auto stream = *data.stream();
      promote_selected(stream, activity_key, case_id_key);
      return event_data_to_event_log(EventDataView(stream), conversion);
    }
    case EventDataKind::kEventTable: {
      auto table = *data.table();
      promote_selected(table, activity_key, case_id_key);
      return event_data_to_event_log(EventDataView(table), conversion);
    }
    case EventDataKind::kLazyEventTable: {
      auto table = promoted_selector_table(*data.lazy_table(),
                                            std::string(activity_key),
                                            std::string(case_id_key));
      return event_data_to_event_log(EventDataView(table), conversion);
    }
  }
  throw std::invalid_argument("event data: invalid input kind");
}

LogSkeletonModel discover_from_variants(
    const VariantMap& variants, const std::vector<Activities>& variant_order,
    const LogSkeletonOptions& options) {
  validate_activity_key(options.activity_key);
  LogSkeletonModel model;
  CountMap all_activs;
  std::set<std::string> seen_activities;
  std::vector<std::string> activity_order;
  std::size_t events_count = 0;
  for (const auto& trace : variant_order) {
    const auto weight = variants.at(trace);
    events_count += trace.size() * weight;
    for (const auto& activity : trace) {
      all_activs[activity] += weight;
      if (seen_activities.insert(activity).second)
        activity_order.push_back(activity);
    }
  }

  {
    std::map<LogSkeletonPair, std::size_t> support;
    for (const auto& [trace, weight] : variants)
      for (const auto& pair : relation_set(trace, "equivalence"))
        support[pair] += weight * frequencies(trace).at(pair.source);
    std::set<LogSkeletonPair> kept;
    for (const auto& [pair, count] : support)
      if (count >= threshold(all_activs[pair.source], options.noise_threshold))
        kept.insert(pair);
    model.equivalence = sorted_pairs(kept);
  }

  for (const std::string family : {"always_after", "always_before"}) {
    std::map<LogSkeletonPair, std::size_t> support;
    for (const auto& [trace, weight] : variants)
      for (const auto& pair : relation_set(trace, family)) support[pair] += weight;
    std::set<LogSkeletonPair> kept;
    for (const auto& [pair, count] : support)
      if (count >= threshold(all_activs[pair.source], options.noise_threshold))
        kept.insert(pair);
    if (family == "always_after") model.always_after = sorted_pairs(kept);
    else model.always_before = sorted_pairs(kept);
  }

  {
    std::map<LogSkeletonPair, long long> support;
    for (const auto& [a, _] : all_activs)
      for (const auto& [b, __] : all_activs)
        if (a != b) support[{a, b}] = static_cast<long long>(all_activs[a]);
    for (const auto& [trace, weight] : variants) {
      for (const auto& pair : relation_set(trace, "never_together")) {
        auto it = support.find(pair);
        if (it == support.end()) continue;
        it->second -= static_cast<long long>(weight);
        if (it->second <= 0) support.erase(it);
      }
    }
    std::set<LogSkeletonPair> kept;
    for (const auto& [pair, count] : support)
      if (count >= threshold(all_activs[pair.source], options.noise_threshold))
        kept.insert(pair);
    model.never_together = sorted_pairs(kept);
  }

  {
    std::map<LogSkeletonPair, std::size_t> support;
    for (const auto& [trace, weight] : variants)
      for (std::size_t i = 1; i < trace.size(); ++i)
        support[{trace[i - 1], trace[i]}] += weight;
    std::set<LogSkeletonPair> kept;
    for (const auto& [pair, count] : support)
      if (count >= threshold(all_activs[pair.source], options.noise_threshold))
        kept.insert(pair);
    model.directly_follows = sorted_pairs(kept);
  }

  {
    std::map<std::string, std::vector<std::pair<std::size_t, std::size_t>>> support;
    for (const auto& trace : variant_order) {
      const auto weight = variants.at(trace);
      const auto counts = frequencies(trace);
      for (const auto& activity : activity_order) {
        const auto frequency = counts.count(activity) ? counts.at(activity) : 0;
        auto& histogram = support[activity];
        auto hit = std::find_if(histogram.begin(), histogram.end(),
                                [&](const auto& entry) {
                                  return entry.first == frequency;
                                });
        if (hit == histogram.end()) histogram.push_back({frequency, weight});
        else hit->second += weight;
      }
    }
    for (auto& [activity, histogram] : support) {
      auto ranked = histogram;
      std::stable_sort(ranked.begin(), ranked.end(),
                       [](const auto& left, const auto& right) {
                         if (left.second != right.second)
                           return left.second > right.second;
                         return false;
                       });
      std::size_t cumulative = 0;
      std::vector<std::size_t> allowed;
      for (const auto& [frequency, count] : ranked) {
        allowed.push_back(frequency);
        cumulative += count;
        if (static_cast<double>(cumulative) >=
            threshold(events_count, options.noise_threshold))
          break;
      }
      std::sort(allowed.begin(), allowed.end());
      model.activ_freq.emplace(activity, std::move(allowed));
    }
  }
  return model;
}

}  // namespace

LogSkeletonModel discover_log_skeleton(const EventLog& log,
                                       const LogSkeletonOptions& options) {
  validate_activity_key(options.activity_key);
  VariantMap variants;
  std::vector<Activities> variant_order;
  for (const auto& trace : log.traces) {
    auto acts = activities_of(trace, options.activity_key);
    auto variant_it = variants.find(acts);
    if (variant_it == variants.end()) {
      variants.emplace(acts, 1);
      variant_order.push_back(acts);
    } else {
      ++variant_it->second;
    }
  }
  return discover_from_variants(variants, variant_order, options);
}

LogSkeletonModel discover_log_skeleton(
    std::initializer_list<Trace> traces, const LogSkeletonOptions& options) {
  EventLog log;
  log.traces.assign(traces.begin(), traces.end());
  return discover_log_skeleton(log, options);
}

LogSkeletonModel discover_log_skeleton(
    const EventStream& stream, const LogSkeletonOptions& options) {
  return discover_log_skeleton(
      event_data_log(EventDataView(stream), options.activity_key,
                     options.case_id_key),
      options);
}

LogSkeletonModel discover_log_skeleton(
    const EventTable& table, const LogSkeletonOptions& options) {
  return discover_log_skeleton(
      event_data_log(EventDataView(table), options.activity_key,
                     options.case_id_key),
      options);
}

LogSkeletonModel discover_log_skeleton(
    const LazyEventTable& table, const LogSkeletonOptions& options) {
  return discover_log_skeleton(
      event_data_log(EventDataView(table), options.activity_key,
                     options.case_id_key),
      options);
}

LogSkeletonModel discover_log_skeleton(const EventDataView& data,
                                       const LogSkeletonOptions& options) {
  if (data.kind() == EventDataKind::kEventLog)
    return discover_log_skeleton(*data.log(), options);
  return discover_log_skeleton(
      event_data_log(data, options.activity_key, options.case_id_key), options);
}

LogSkeletonModel discover_log_skeleton(const LogSkeletonVariantList& input,
                                       const LogSkeletonOptions& options) {
  // The explicit variant-list entry point follows the source API contract:
  // each list entry contributes one trace and its count field is metadata only.
  // Duplicate entries therefore retain their multiplicity through the normal
  // variant aggregation, while an empty entry remains a real empty trace.
  validate_activity_key(options.activity_key);
  VariantMap variants;
  std::vector<Activities> variant_order;
  for (const auto& [activities, _count] : input) {
    auto [it, inserted] = variants.emplace(activities, 1);
    if (inserted) {
      variant_order.push_back(activities);
    } else {
      ++it->second;
    }
  }
  return discover_from_variants(variants, variant_order, options);
}

LogSkeletonConformance check_log_skeleton_conformance(
    const std::vector<std::string>& trace, const LogSkeletonModel& model,
    const LogSkeletonOptions& options) {
  LogSkeletonConformance result;
  const auto counts = frequencies(trace);
  auto present = std::set<std::string>(trace.begin(), trace.end());
  auto check_pairs = [&](const std::string& family,
                         const std::vector<LogSkeletonPair>& expected) {
    if (!considered(options, family)) return;
    std::set<LogSkeletonPair> actual = relation_set(trace, family);
    std::set<LogSkeletonPair> filtered;
    for (const auto& pair : expected)
      if (present.count(pair.source)) filtered.insert(pair);
    result.no_constr_total += filtered.size();
    for (const auto& pair : filtered)
      if (family == "never_together" ? actual.count(pair) != 0 : actual.count(pair) == 0) {
        ++result.no_dev_total;
        LogSkeletonDeviation deviation;
        deviation.constraint = family;
        deviation.relation = pair;
        result.deviations.push_back(std::move(deviation));
      }
  };
  check_pairs("equivalence", model.equivalence);
  check_pairs("always_after", model.always_after);
  check_pairs("always_before", model.always_before);
  check_pairs("never_together", model.never_together);
  check_pairs("directly_follows", model.directly_follows);

  if (considered(options, "activ_freq")) {
    std::size_t constraints = 0;
    // trace_skel.get_trace_info returns an activity-frequency Counter; its
    // iteration is over distinct activities, not every event occurrence.
    for (const auto& [activity, observed] : counts) {
      ++constraints;
      auto it = model.activ_freq.find(activity);
      if (it == model.activ_freq.end()) {
        ++result.no_dev_total;
        LogSkeletonDeviation deviation;
        deviation.constraint = "activ_freq";
        deviation.activity = activity;
        // An unknown activity is reported as the absent (zero) model
        // frequency, rather than echoing its observed count.
        deviation.observed_frequency = 0;
        result.deviations.push_back(std::move(deviation));
      } else if (std::find(it->second.begin(), it->second.end(), observed) == it->second.end()) {
        ++result.no_dev_total;
        LogSkeletonDeviation deviation;
        deviation.constraint = "activ_freq";
        deviation.activity = activity;
        deviation.observed_frequency = observed;
        deviation.allowed_frequencies = it->second;
        result.deviations.push_back(std::move(deviation));
      }
    }
    for (const auto& [activity, allowed] : model.activ_freq) {
      if (!allowed.empty() && allowed.front() > 0 && !present.count(activity)) {
        ++constraints;
        ++result.no_dev_total;
        LogSkeletonDeviation deviation;
        deviation.constraint = "activ_freq";
        deviation.activity = activity;
        deviation.allowed_frequencies = allowed;
        result.deviations.push_back(std::move(deviation));
      }
    }
    result.no_constr_total += constraints;
  }
  std::sort(result.deviations.begin(), result.deviations.end(), [](const auto& a, const auto& b) {
    if (a.constraint != b.constraint) return a.constraint < b.constraint;
    if (a.relation.source != b.relation.source) return a.relation.source < b.relation.source;
    if (a.relation.target != b.relation.target) return a.relation.target < b.relation.target;
    return a.activity < b.activity;
  });
  result.dev_fitness = result.no_constr_total == 0
                           ? 1.0
                           : 1.0 - static_cast<double>(result.no_dev_total) /
                                     static_cast<double>(result.no_constr_total);
  result.is_fit = result.deviations.empty();
  return result;
}

LogSkeletonConformance check_log_skeleton_conformance(
    const Trace& trace, const LogSkeletonModel& model,
    const LogSkeletonOptions& options) {
  return check_log_skeleton_conformance(
      activities_of(trace, options.activity_key), model, options);
}

std::vector<LogSkeletonConformance> check_log_skeleton_conformance(
    const EventLog& log, const LogSkeletonModel& model,
    const LogSkeletonOptions& options) {
  validate_activity_key(options.activity_key);
  std::vector<LogSkeletonConformance> result;
  result.reserve(log.traces.size());
  for (const auto& trace : log.traces)
    result.push_back(check_log_skeleton_conformance(trace, model, options));
  return result;
}

std::vector<LogSkeletonConformance> check_log_skeleton_conformance(
    const EventStream& stream, const LogSkeletonModel& model,
    const LogSkeletonOptions& options) {
  return check_log_skeleton_conformance(
      event_data_log(EventDataView(stream), options.activity_key,
                     options.case_id_key),
      model,
      options);
}

std::vector<LogSkeletonConformance> check_log_skeleton_conformance(
    const EventTable& table, const LogSkeletonModel& model,
    const LogSkeletonOptions& options) {
  return check_log_skeleton_conformance(
      event_data_log(EventDataView(table), options.activity_key,
                     options.case_id_key),
      model,
      options);
}

std::vector<LogSkeletonConformance> check_log_skeleton_conformance(
    const LazyEventTable& table, const LogSkeletonModel& model,
    const LogSkeletonOptions& options) {
  return check_log_skeleton_conformance(
      event_data_log(EventDataView(table), options.activity_key,
                     options.case_id_key),
      model,
      options);
}

std::vector<LogSkeletonConformance> check_log_skeleton_conformance(
    const EventDataView& data, const LogSkeletonModel& model,
    const LogSkeletonOptions& options) {
  if (data.kind() == EventDataKind::kEventLog)
    return check_log_skeleton_conformance(*data.log(), model, options);
  return check_log_skeleton_conformance(
      event_data_log(data, options.activity_key, options.case_id_key), model,
      options);
}

LogSkeletonVariantConformance check_log_skeleton_conformance(
    const LogSkeletonVariantList& variants, const LogSkeletonModel& model,
    const LogSkeletonOptions& options) {
  validate_activity_key(options.activity_key);
  LogSkeletonVariantConformance result;
  for (const auto& [activities, _count] : variants)
    result[activities] =
        check_log_skeleton_conformance(activities, model, options);
  return result;
}

namespace {

void validate_case_id_key(std::string_view key, std::string_view context) {
  if (key.empty() ||
      std::all_of(key.begin(), key.end(), [](char value) {
        return std::isspace(static_cast<unsigned char>(value)) != 0;
      }))
    throw std::invalid_argument(std::string(context) +
                                ": case-id selector must not be blank");
}

std::optional<EventAttribute> trace_case_attribute(
    const Trace& trace, std::string_view case_id_key) {
  if (trace.case_id_attribute && trace.case_id_attribute->key == case_id_key) {
    if (trace.case_id_attribute->is_null) return std::nullopt;
    return *trace.case_id_attribute;
  }
  for (const auto& attribute : trace.attributes)
    if (attribute.key == case_id_key)
      return attribute.is_null ? std::nullopt
                               : std::optional<EventAttribute>(attribute);
  const bool intrinsic =
      case_id_key == "case:concept:name" || case_id_key == "case:id" ||
      case_id_key == "case_id" || case_id_key == "case" ||
      (!trace.case_id_source_key.empty() &&
       trace.case_id_source_key == case_id_key);
  if (intrinsic) {
    if (trace.case_id_state == FirstClassAttributeState::kNull ||
        !trace.has_case_id())
      return std::nullopt;
    if (!trace.case_id_source_key.empty() &&
        trace.case_id_source_key == case_id_key && trace.case_id_attribute)
      return *trace.case_id_attribute;
    return EventAttribute{std::string(case_id_key), trace.case_id};
  }
  for (const auto& event : trace.events) {
    if (!event.case_id_source_key.empty() &&
        event.case_id_source_key == case_id_key) {
      if (event.case_id_state == FirstClassAttributeState::kNull ||
          !event.has_case_id())
        return std::nullopt;
      return EventAttribute{std::string(case_id_key), event.case_id};
    }
  }
  return std::nullopt;
}

EventLog case_id_log(const EventDataView& data,
                     std::string_view case_id_key) {
  return event_data_log(data, "concept:name", case_id_key);
}

EventAttribute integer_cell(std::string key, std::int64_t value) {
  EventAttribute result{std::move(key), std::to_string(value),
                        AttributeKind::kInt};
  result.typed_value = value;
  return result;
}

std::int64_t diagnostic_count(std::size_t value) {
  if (value > static_cast<std::size_t>(
                  std::numeric_limits<std::int64_t>::max()))
    throw std::overflow_error(
        "log skeleton diagnostics: count exceeds the int64 table range");
  return static_cast<std::int64_t>(value);
}

EventAttribute floating_cell(std::string key, double value) {
  if (!std::isfinite(value))
    throw std::invalid_argument(
        "log skeleton diagnostics: result fitness must be finite");
  std::ostringstream formatted;
  formatted << std::setprecision(std::numeric_limits<double>::max_digits10)
            << value;
  EventAttribute result{std::move(key), formatted.str(), AttributeKind::kFloat};
  result.typed_value = value;
  return result;
}

EventTable make_diagnostics_table(
    const EventLog& log, const std::vector<LogSkeletonConformance>& results,
    const LogSkeletonDiagnosticsOptions& options) {
  validate_case_id_key(options.case_id_key, "log skeleton diagnostics");
  if (results.size() != log.traces.size())
    throw std::invalid_argument(
        "log skeleton diagnostics: result count does not match trace count");
  EventTable table;
  table.columns = {"case_id", "no_dev_total", "no_constr_total",
                   "dev_fitness"};
  table.rows.reserve(results.size());
  for (std::size_t index = 0; index < results.size(); ++index) {
    auto case_id =
        trace_case_attribute(log.traces[index], options.case_id_key);
    if (!case_id)
      throw std::invalid_argument(
          "log skeleton diagnostics: case-id selector is missing or null");
    auto case_cell = std::move(*case_id);
    case_cell.key = "case_id";
    case_cell.first_class_role = FirstClassAttributeRole::kCaseId;
    case_cell.first_class_state = FirstClassAttributeState::kPresent;
    case_cell.first_class_source_key = options.case_id_key;
    const auto& result = results[index];
    table.rows.push_back({std::move(case_cell),
                          integer_cell("no_dev_total",
                                       diagnostic_count(result.no_dev_total)),
                          integer_cell("no_constr_total",
                                       diagnostic_count(
                                           result.no_constr_total)),
                          floating_cell("dev_fitness", result.dev_fitness)});
  }
  table.validate();
  return table;
}

}  // namespace

EventTable log_skeleton_conformance_diagnostics(
    const EventLog& log, const std::vector<LogSkeletonConformance>& results,
    const LogSkeletonDiagnosticsOptions& options) {
  return make_diagnostics_table(log, results, options);
}

EventTable log_skeleton_conformance_diagnostics(
    const EventStream& stream,
    const std::vector<LogSkeletonConformance>& results,
    const LogSkeletonDiagnosticsOptions& options) {
  return make_diagnostics_table(case_id_log(EventDataView(stream),
                                             options.case_id_key),
                                results, options);
}

EventTable log_skeleton_conformance_diagnostics(
    const EventTable& table,
    const std::vector<LogSkeletonConformance>& results,
    const LogSkeletonDiagnosticsOptions& options) {
  return make_diagnostics_table(case_id_log(EventDataView(table),
                                             options.case_id_key),
                                results, options);
}

EventTable log_skeleton_conformance_diagnostics(
    const LazyEventTable& table,
    const std::vector<LogSkeletonConformance>& results,
    const LogSkeletonDiagnosticsOptions& options) {
  return make_diagnostics_table(case_id_log(EventDataView(table),
                                             options.case_id_key),
                                results, options);
}

EventTable log_skeleton_conformance_diagnostics(
    const EventDataView& data,
    const std::vector<LogSkeletonConformance>& results,
    const LogSkeletonDiagnosticsOptions& options) {
  return make_diagnostics_table(case_id_log(data, options.case_id_key),
                                results, options);
}

}  // namespace minera
