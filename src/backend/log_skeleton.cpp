#include "minera/log_skeleton.hpp"

#include "minera/log_conversion.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace minera {
namespace {

using Activities = std::vector<std::string>;

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
  // Case provenance belongs to the trace scope. XES commonly uses the same
  // source key for a trace id and an event activity; only use the case value
  // when no event field claimed this selector (including null event cells).
  if ((case_source || case_alias) && candidates.empty())
    consider(0,
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

// activities_of without copies: each event's selected label in place.
void selected_activities(const Trace& trace, std::string_view activity_key,
                         std::vector<const std::string*>& result) {
  validate_activity_key(activity_key);
  result.clear();
  for (const auto& event : trace.events) {
    const auto* value = resolve_event_field(event, activity_key);
    if (!value)
      throw std::invalid_argument(
          "log skeleton: event activity selector is missing or null: '" +
          std::string(activity_key) + "'");
    result.push_back(value);
  }
}

Activities activities_of(const Trace& trace, std::string_view activity_key) {
  validate_activity_key(activity_key);
  Activities result;
  result.reserve(trace.events.size());
  for (const auto& event : trace.events)
    result.push_back(selected_activity(event, activity_key));
  return result;
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
      source.attributes(), source.metadata(), source.row_count(),
      source.dtypes(), source.index(), source.categorical_dtypes());
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

// A log's distinct activity sequences over interned activities. Activity ids
// follow the names' string order, so ordering ids orders names; variants are
// in first-appearance order, each with its weight (number of traces).
struct InternedVariants {
  std::vector<std::string> names;
  std::vector<std::vector<std::uint32_t>> variants;
  std::vector<std::size_t> weights;
};

struct IdSequenceHash {
  std::size_t operator()(const std::vector<std::uint32_t>& ids) const noexcept {
    std::uint64_t value = 1469598103934665603ull;
    for (const auto id : ids) value = (value ^ id) * 1099511628211ull;
    return static_cast<std::size_t>(value);
  }
};

// Collects variants from activity sequences given one at a time as name
// pointers, then renumbers the activities into string order.
class VariantCollector {
 public:
  void add(const std::vector<const std::string*>& sequence) {
    scratch_.clear();
    for (const auto* name : sequence) {
      const auto [it, added] =
          ids_.try_emplace(*name, static_cast<std::uint32_t>(names_.size()));
      if (added) names_.push_back(&it->first);
      scratch_.push_back(it->second);
    }
    const auto [it, added] = variant_ids_.try_emplace(scratch_, result_.variants.size());
    if (added) {
      result_.variants.push_back(scratch_);
      result_.weights.push_back(1);
    } else {
      ++result_.weights[it->second];
    }
  }
  InternedVariants finish() {
    std::vector<std::uint32_t> order(names_.size());
    for (std::uint32_t id = 0; id < order.size(); ++id) order[id] = id;
    std::sort(order.begin(), order.end(), [&](std::uint32_t left, std::uint32_t right) {
      return *names_[left] < *names_[right];
    });
    std::vector<std::uint32_t> renumber(order.size());
    result_.names.reserve(order.size());
    for (std::uint32_t rank = 0; rank < order.size(); ++rank) {
      renumber[order[rank]] = rank;
      result_.names.push_back(*names_[order[rank]]);
    }
    for (auto& variant : result_.variants)
      for (auto& id : variant) id = renumber[id];
    return std::move(result_);
  }

 private:
  std::unordered_map<std::string, std::uint32_t> ids_;
  std::vector<const std::string*> names_;
  std::unordered_map<std::vector<std::uint32_t>, std::size_t, IdSequenceHash> variant_ids_;
  std::vector<std::uint32_t> scratch_;
  InternedVariants result_;
};

// The reference's relation counts over interned variants. Every count is an
// integer sum, so summing per variant in any order gives the reference's
// value; label pairs are kept in (source, target) order, the order of the
// reference's sorted pair sets. A pair table holds A^2 cells (the reference
// already materialises every never-together pair).
LogSkeletonModel discover_from_variants(const InternedVariants& input,
                                        const LogSkeletonOptions& options) {
  validate_activity_key(options.activity_key);
  LogSkeletonModel model;
  const std::size_t n = input.names.size();
  std::vector<std::size_t> all_activs(n, 0);
  std::vector<std::uint32_t> activity_order;
  std::vector<char> seen(n, 0);
  std::size_t events_count = 0;
  for (std::size_t v = 0; v < input.variants.size(); ++v) {
    const auto& trace = input.variants[v];
    const auto weight = input.weights[v];
    events_count += trace.size() * weight;
    for (const auto activity : trace) {
      all_activs[activity] += weight;
      if (!seen[activity]) {
        seen[activity] = 1;
        activity_order.push_back(activity);
      }
    }
  }

  // Per variant: its distinct activities, their counts, first and last
  // positions.
  std::vector<std::size_t> counts(n, 0), first(n, 0), last(n, 0);
  std::vector<std::uint32_t> present;
  const auto profile = [&](const std::vector<std::uint32_t>& trace) {
    for (const auto a : present) counts[a] = 0;
    present.clear();
    for (std::size_t i = 0; i < trace.size(); ++i) {
      const auto a = trace[i];
      if (counts[a]++ == 0) {
        present.push_back(a);
        first[a] = i;
      }
      last[a] = i;
    }
  };
  const auto kept_pairs = [&](const std::vector<std::size_t>& support,
                              const std::vector<char>& found) {
    std::vector<LogSkeletonPair> kept;
    for (std::size_t a = 0; a < n; ++a)
      for (std::size_t b = 0; b < n; ++b) {
        const auto cell = a * n + b;
        if (found[cell] &&
            support[cell] >= threshold(all_activs[a], options.noise_threshold))
          kept.push_back({input.names[a], input.names[b]});
      }
    return kept;
  };

  {
    std::vector<std::size_t> support(n * n, 0);
    std::vector<char> found(n * n, 0);
    for (std::size_t v = 0; v < input.variants.size(); ++v) {
      profile(input.variants[v]);
      const auto weight = input.weights[v];
      for (const auto a : present)
        for (const auto b : present)
          if (a != b && counts[a] == counts[b]) {
            support[a * n + b] += weight * counts[a];
            found[a * n + b] = 1;
          }
    }
    model.equivalence = kept_pairs(support, found);
  }

  for (const bool after : {true, false}) {
    std::vector<std::size_t> support(n * n, 0);
    std::vector<char> found(n * n, 0);
    for (std::size_t v = 0; v < input.variants.size(); ++v) {
      profile(input.variants[v]);
      const auto weight = input.weights[v];
      // (trace[i], trace[j]) over positions i != j, j after i (always_after)
      // or before it (always_before): a label pair occurs in that order
      // exactly when the first a precedes the last b (after), or the last a
      // follows the first b (before).
      for (const auto a : present)
        for (const auto b : present)
          if (after ? first[a] < last[b] : last[a] > first[b]) {
            support[a * n + b] += weight;
            found[a * n + b] = 1;
          }
    }
    if (after) model.always_after = kept_pairs(support, found);
    else model.always_before = kept_pairs(support, found);
  }

  {
    // Every pair of distinct activities starts at its source's count; each
    // variant holding both takes its weight off, and a pair at or below zero
    // is dropped for good.
    std::vector<long long> support(n * n, 0);
    std::vector<char> alive(n * n, 0);
    for (std::size_t a = 0; a < n; ++a)
      for (std::size_t b = 0; b < n; ++b)
        if (a != b) {
          support[a * n + b] = static_cast<long long>(all_activs[a]);
          alive[a * n + b] = 1;
        }
    for (std::size_t v = 0; v < input.variants.size(); ++v) {
      profile(input.variants[v]);
      const auto weight = static_cast<long long>(input.weights[v]);
      for (const auto a : present)
        for (const auto b : present) {
          const auto cell = a * n + b;
          if (a == b || !alive[cell]) continue;
          support[cell] -= weight;
          if (support[cell] <= 0) alive[cell] = 0;
        }
    }
    for (std::size_t a = 0; a < n; ++a)
      for (std::size_t b = 0; b < n; ++b) {
        const auto cell = a * n + b;
        if (alive[cell] &&
            support[cell] >= threshold(all_activs[a], options.noise_threshold))
          model.never_together.push_back({input.names[a], input.names[b]});
      }
  }

  {
    std::vector<std::size_t> support(n * n, 0);
    std::vector<char> found(n * n, 0);
    for (std::size_t v = 0; v < input.variants.size(); ++v) {
      const auto& trace = input.variants[v];
      for (std::size_t i = 1; i < trace.size(); ++i) {
        const auto cell = std::size_t(trace[i - 1]) * n + trace[i];
        support[cell] += input.weights[v];
        found[cell] = 1;
      }
    }
    model.directly_follows = kept_pairs(support, found);
  }

  {
    // Histograms in first-seen frequency order, per activity in name order.
    std::vector<std::vector<std::pair<std::size_t, std::size_t>>> support(n);
    for (std::size_t v = 0; v < input.variants.size(); ++v) {
      profile(input.variants[v]);
      const auto weight = input.weights[v];
      for (const auto activity : activity_order) {
        const auto frequency = counts[activity];
        auto& histogram = support[activity];
        auto hit = std::find_if(histogram.begin(), histogram.end(),
                                [&](const auto& entry) {
                                  return entry.first == frequency;
                                });
        if (hit == histogram.end()) histogram.push_back({frequency, weight});
        else hit->second += weight;
      }
    }
    for (std::size_t activity = 0; activity < n; ++activity) {
      auto ranked = support[activity];
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
      model.activ_freq.emplace(input.names[activity], std::move(allowed));
    }
  }
  return model;
}

}  // namespace

LogSkeletonModel discover_log_skeleton(const EventLog& log,
                                       const LogSkeletonOptions& options) {
  validate_activity_key(options.activity_key);
  VariantCollector variants;
  std::vector<const std::string*> sequence;
  for (const auto& trace : log.traces) {
    selected_activities(trace, options.activity_key, sequence);
    variants.add(sequence);
  }
  return discover_from_variants(variants.finish(), options);
}

LogSkeletonModel discover_log_skeleton(const EventLog& log,
                                       const LogSkeletonOptions& options,
                                       const std::vector<std::size_t>& trace_order) {
  validate_activity_key(options.activity_key);
  if (trace_order.size() != log.traces.size())
    throw std::invalid_argument("log skeleton: trace order must list every trace once");
  VariantCollector variants;
  std::vector<const std::string*> sequence;
  for (const auto index : trace_order) {
    if (index >= log.traces.size())
      throw std::invalid_argument("log skeleton: trace order position out of range");
    selected_activities(log.traces[index], options.activity_key, sequence);
    variants.add(sequence);
  }
  return discover_from_variants(variants.finish(), options);
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

LogSkeletonModel discover_log_skeleton(const EventDataView& data,
                                       const LogSkeletonOptions& options,
                                       const std::vector<std::size_t>& trace_order) {
  if (data.kind() == EventDataKind::kEventLog)
    return discover_log_skeleton(*data.log(), options, trace_order);
  return discover_log_skeleton(
      event_data_log(data, options.activity_key, options.case_id_key), options,
      trace_order);
}

std::vector<std::size_t> log_skeleton_case_order(const EventDataView& data,
                                                 const LogSkeletonOptions& options) {
  if (data.kind() == EventDataKind::kEventLog) return case_id_order(*data.log());
  return case_id_order(event_data_log(data, options.activity_key, options.case_id_key));
}

LogSkeletonModel discover_log_skeleton(const LogSkeletonVariantList& input,
                                       const LogSkeletonOptions& options) {
  // The explicit variant-list entry point follows the source API contract:
  // each list entry contributes one trace and its count field is metadata only.
  // Duplicate entries therefore retain their multiplicity through the normal
  // variant aggregation, while an empty entry remains a real empty trace.
  validate_activity_key(options.activity_key);
  VariantCollector variants;
  std::vector<const std::string*> sequence;
  for (const auto& [activities, _count] : input) {
    sequence.clear();
    for (const auto& activity : activities) sequence.push_back(&activity);
    variants.add(sequence);
  }
  return discover_from_variants(variants.finish(), options);
}

namespace {

// A model prepared for checking many traces: labels interned (trace labels
// outside the model get ids as they appear), each pair family's expected
// pairs deduplicated in (source, target) order — the reference's filtered
// set — and the activity-frequency rules by id.
class SkeletonChecker {
 public:
  SkeletonChecker(const LogSkeletonModel& model, const LogSkeletonOptions& options)
      : options_(options) {
    const std::pair<const char*, const std::vector<LogSkeletonPair>*> families[] = {
        {"equivalence", &model.equivalence},
        {"always_after", &model.always_after},
        {"always_before", &model.always_before},
        {"never_together", &model.never_together},
        {"directly_follows", &model.directly_follows}};
    for (std::size_t f = 0; f < kPairFamilies; ++f) {
      families_[f].name = families[f].first;
      families_[f].considered = considered(options, families[f].first);
      if (!families_[f].considered) continue;
      const std::set<LogSkeletonPair> unique(families[f].second->begin(),
                                             families[f].second->end());
      for (const auto& pair : unique)
        families_[f].pairs.push_back({id(pair.source), id(pair.target), pair});
    }
    activ_freq_considered_ = considered(options, "activ_freq");
    for (const auto& [activity, allowed] : model.activ_freq)
      activ_freq_.push_back({id(activity), &activity, &allowed});
    rule_of_.assign(names_.size(), nullptr);
    for (const auto& rule : activ_freq_) rule_of_[rule.activity] = rule.allowed;
  }

  // The trace's labels as ids, interning labels the model does not name.
  void intern(const std::vector<const std::string*>& trace, std::vector<std::uint32_t>& ids) {
    ids.clear();
    for (const auto* name : trace) ids.push_back(id(*name));
  }

  LogSkeletonConformance check(const std::vector<std::uint32_t>& trace) {
    const std::size_t n = names_.size();
    if (counts_.size() < n) {
      counts_.resize(n, 0);
      first_.resize(n, 0);
      last_.resize(n, 0);
      rule_of_.resize(n, nullptr);
    }
    for (const auto a : present_) counts_[a] = 0;
    present_.clear();
    follows_.clear();
    for (std::size_t i = 0; i < trace.size(); ++i) {
      const auto a = trace[i];
      if (counts_[a]++ == 0) {
        present_.push_back(a);
        first_[a] = i;
      }
      last_[a] = i;
      if (i > 0) follows_.push_back((std::uint64_t(trace[i - 1]) << 32) | a);
    }
    std::sort(follows_.begin(), follows_.end());
    const auto in = [&](std::uint32_t a) { return counts_[a] != 0; };

    LogSkeletonConformance result;
    for (std::size_t f = 0; f < kPairFamilies; ++f) {
      const auto& family = families_[f];
      if (!family.considered) continue;
      for (const auto& expected : family.pairs) {
        const auto a = expected.source, b = expected.target;
        if (!in(a)) continue;
        ++result.no_constr_total;
        bool actual = false;
        switch (f) {
          case 0: actual = a != b && in(b) && counts_[a] == counts_[b]; break;
          case 1: actual = in(b) && first_[a] < last_[b]; break;
          case 2: actual = in(b) && last_[a] > first_[b]; break;
          case 3: actual = a != b && in(b); break;
          default:
            actual = std::binary_search(follows_.begin(), follows_.end(),
                                        (std::uint64_t(a) << 32) | b);
        }
        if (f == 3 ? actual : !actual) {
          ++result.no_dev_total;
          LogSkeletonDeviation deviation;
          deviation.constraint = family.name;
          deviation.relation = expected.pair;
          result.deviations.push_back(std::move(deviation));
        }
      }
    }

    if (activ_freq_considered_) {
      std::size_t constraints = 0;
      // trace_skel.get_trace_info returns an activity-frequency Counter; its
      // iteration is over distinct activities, not every event occurrence.
      std::sort(present_.begin(), present_.end(), [&](std::uint32_t left, std::uint32_t right) {
        return *names_[left] < *names_[right];
      });
      for (const auto activity : present_) {
        const auto observed = counts_[activity];
        ++constraints;
        const auto* allowed = rule_of_[activity];
        if (!allowed) {
          ++result.no_dev_total;
          LogSkeletonDeviation deviation;
          deviation.constraint = "activ_freq";
          deviation.activity = *names_[activity];
          // An unknown activity is reported as the absent (zero) model
          // frequency, rather than echoing its observed count.
          deviation.observed_frequency = 0;
          result.deviations.push_back(std::move(deviation));
        } else if (std::find(allowed->begin(), allowed->end(), observed) == allowed->end()) {
          ++result.no_dev_total;
          LogSkeletonDeviation deviation;
          deviation.constraint = "activ_freq";
          deviation.activity = *names_[activity];
          deviation.observed_frequency = observed;
          deviation.allowed_frequencies = *allowed;
          result.deviations.push_back(std::move(deviation));
        }
      }
      for (const auto& rule : activ_freq_) {
        const auto& allowed = *rule.allowed;
        if (!allowed.empty() && allowed.front() > 0 && !in(rule.activity)) {
          ++constraints;
          ++result.no_dev_total;
          LogSkeletonDeviation deviation;
          deviation.constraint = "activ_freq";
          deviation.activity = *rule.name;
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

 private:
  static constexpr std::size_t kPairFamilies = 5;
  struct ExpectedPair {
    std::uint32_t source, target;
    LogSkeletonPair pair;
  };
  struct Family {
    const char* name = "";
    bool considered = false;
    std::vector<ExpectedPair> pairs;
  };
  struct FrequencyRule {
    std::uint32_t activity;
    const std::string* name;
    const std::vector<std::size_t>* allowed;
  };

  std::uint32_t id(const std::string& name) {
    const auto [it, added] = ids_.try_emplace(name, static_cast<std::uint32_t>(names_.size()));
    if (added) names_.push_back(&it->first);
    return it->second;
  }

  const LogSkeletonOptions& options_;
  std::unordered_map<std::string, std::uint32_t> ids_;
  std::vector<const std::string*> names_;
  Family families_[kPairFamilies];
  bool activ_freq_considered_ = false;
  std::vector<FrequencyRule> activ_freq_;
  std::vector<const std::vector<std::size_t>*> rule_of_;
  std::vector<std::size_t> counts_, first_, last_;
  std::vector<std::uint32_t> present_;
  std::vector<std::uint64_t> follows_;
};

}  // namespace

LogSkeletonConformance check_log_skeleton_conformance(
    const std::vector<std::string>& trace, const LogSkeletonModel& model,
    const LogSkeletonOptions& options) {
  SkeletonChecker checker(model, options);
  std::vector<const std::string*> labels;
  labels.reserve(trace.size());
  for (const auto& activity : trace) labels.push_back(&activity);
  std::vector<std::uint32_t> ids;
  checker.intern(labels, ids);
  return checker.check(ids);
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
  // A trace's result depends only on its activity sequence, so each distinct
  // sequence (variant) is checked once.
  SkeletonChecker checker(model, options);
  std::unordered_map<std::vector<std::uint32_t>, std::size_t, IdSequenceHash> checked;
  std::vector<const std::string*> labels;
  std::vector<std::uint32_t> ids;
  for (const auto& trace : log.traces) {
    selected_activities(trace, options.activity_key, labels);
    checker.intern(labels, ids);
    const auto found = checked.find(ids);
    if (found != checked.end()) {
      result.push_back(result[found->second]);
      continue;
    }
    result.push_back(checker.check(ids));
    checked.emplace(ids, result.size() - 1);
  }
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
  SkeletonChecker checker(model, options);
  std::vector<const std::string*> labels;
  std::vector<std::uint32_t> ids;
  for (const auto& [activities, _count] : variants) {
    labels.clear();
    for (const auto& activity : activities) labels.push_back(&activity);
    checker.intern(labels, ids);
    result[activities] = checker.check(ids);
  }
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
  // The reference builds its frame from the result rows, so an empty log
  // gives a frame without columns.
  if (!results.empty())
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
