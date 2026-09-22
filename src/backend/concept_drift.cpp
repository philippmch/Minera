#include "minera/concept_drift.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <string_view>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "minera/timestamp.hpp"
#include "exact_count_ratio.hpp"

namespace minera {
namespace {

constexpr std::size_t kMaxPermutations = 1'000'000;
constexpr long double kMaxPermutationWork = 200'000'000.0L;
constexpr long double kMaxMaterializedTraceCopies = 2'000'000.0L;
constexpr long double kMaxCumulativeEventCopies = 20'000'000.0L;

std::optional<double> precise_epoch_seconds(std::string_view text) {
  const auto parsed = parse_timestamp_exact(text);
  // Keep the existing parser's additional accepted forms, including fractions
  // longer than nine digits. Exact ISO instants take the integer path below.
  if (!parsed) return parse_timestamp(text);
  const bool negative = parsed->seconds < 0;
  const auto seconds = negative
                           ? std::uint64_t{0} -
                                 static_cast<std::uint64_t>(parsed->seconds)
                           : static_cast<std::uint64_t>(parsed->seconds);
  constexpr std::uint64_t scale = 1000000000;
  // Four-digit years need fewer than 69 bits for epoch nanoseconds. Split the
  // product into 32-bit limbs so the calculation also works on MSVC.
  const auto low = (seconds & 0xffffffffULL) * scale;
  const auto high = (seconds >> 32) * scale;
  auto numerator = detail::add(detail::CountUint128{0, low},
                               detail::CountUint128{high >> 32, high << 32});
  const auto fraction = detail::CountUint128::from_uint64(parsed->nanoseconds);
  numerator = negative ? detail::subtract(numerator, fraction)
                       : detail::add(numerator, fraction);
  return detail::exact_ratio(numerator,
                            detail::CountUint128::from_uint64(scale), negative);
}

void validate_options(const ConceptDriftOptions& options, std::size_t trace_count,
                     std::size_t event_count, std::size_t feature_size) {
  if (options.variant != ConceptDriftVariant::kBose) {
    throw std::invalid_argument("concept drift: unsupported variant");
  }
  if (options.activity_attribute.empty() ||
      options.timestamp_attribute.empty() || options.case_id_attribute.empty()) {
    throw std::invalid_argument(
        "concept drift: activity, timestamp, and case-id attributes must not be empty");
  }
  if (options.sub_log_size == 0) {
    throw std::invalid_argument("concept drift: sub_log_size must be positive");
  }
  if (options.window_size == 0) {
    throw std::invalid_argument("concept drift: window_size must be positive");
  }
  if (options.window_size > std::numeric_limits<std::size_t>::max() / 2) {
    throw std::invalid_argument("concept drift: window_size is too large");
  }
  if (options.num_permutations == 0 ||
      options.num_permutations > kMaxPermutations) {
    throw std::invalid_argument(
        "concept drift: num_permutations must be in [1, 1000000]");
  }
  // A very large cap is usually a wrapped negative value at an API boundary;
  // rejecting it also keeps result allocation bounded.
  if (options.max_no_change_points > kMaxPermutations) {
    throw std::invalid_argument(
        "concept drift: max_no_change_points exceeds safe bound");
  }
  if (!std::isfinite(options.threshold) || options.threshold < 0.0 ||
      options.threshold > 1.0) {
    throw std::invalid_argument(
        "concept drift: threshold must be finite and in [0,1]");
  }

  const std::size_t sublog_count =
      trace_count == 0 ? 0 : (trace_count - 1) / options.sub_log_size + 1;
  if (sublog_count > 0) {
    const std::size_t complete_sublogs = trace_count / options.sub_log_size;
    const std::size_t remainder = trace_count % options.sub_log_size;
    const long double complete = static_cast<long double>(complete_sublogs);
    long double cumulative_trace_copies =
        static_cast<long double>(options.sub_log_size) * complete *
        (complete + 1.0L) / 2.0L;
    if (remainder != 0) cumulative_trace_copies += static_cast<long double>(trace_count);
    const long double materialized_trace_copies =
        cumulative_trace_copies + 2.0L * static_cast<long double>(trace_count);
    const long double cumulative_event_copies =
        static_cast<long double>(event_count) *
        static_cast<long double>(sublog_count);
    if (materialized_trace_copies > kMaxMaterializedTraceCopies ||
        cumulative_event_copies > kMaxCumulativeEventCopies) {
      throw std::invalid_argument(
          "concept drift: cumulative sub-log materialization exceeds safe bound");
    }
  }
  if (feature_size != 0 && sublog_count > 2 * options.window_size - 1) {
    const std::size_t windows = sublog_count - 2 * options.window_size + 1;
    const long double work = static_cast<long double>(windows) *
                             static_cast<long double>(options.num_permutations) *
                             static_cast<long double>(std::max<std::size_t>(1, feature_size)) *
                             static_cast<long double>(2 * options.window_size);
    if (work > kMaxPermutationWork) {
      throw std::invalid_argument(
          "concept drift: requested permutation work exceeds safe bound");
    }
  }
}

[[noreturn]] void reject_activity_selector(std::string_view selector,
                                         std::string_view reason) {
  throw std::invalid_argument(
      "concept drift: selected activity selector '" + std::string(selector) +
      "' " + std::string(reason));
}

const EventAttribute* event_attribute_entry(const Event& event,
                                           std::string_view key) {
  for (const auto& attribute : event.attributes) {
    if (attribute.key == key) return &attribute;
  }
  return nullptr;
}

void validate_activity_attribute_cell(const EventAttribute& attribute,
                                     std::string_view selector) {
  if (attribute.is_null ||
      attribute.first_class_state == FirstClassAttributeState::kNull) {
    reject_activity_selector(selector, "must be non-null");
  }
  if (attribute.container != AttributeContainerKind::kScalar ||
      !attribute.children.empty() || attribute.scalar_wrapper ||
      attribute.scalar_children_list) {
    reject_activity_selector(selector, "must be a scalar string");
  }
  // A monostate typed value means that no native value was supplied.  It is
  // the normal representation for manually constructed string cells; only a
  // native string kind with no conflicting typed value is accepted here.
  if (attribute.kind != AttributeKind::kString ||
      !std::holds_alternative<std::monostate>(attribute.typed_value)) {
    reject_activity_selector(selector, "must be a string");
  }
  if (attribute.first_class_role != FirstClassAttributeRole::kNone &&
      attribute.first_class_state == FirstClassAttributeState::kMissing) {
    reject_activity_selector(selector, "is missing");
  }
}

void validate_activity_field(std::string_view selector,
                            FirstClassAttributeState state, bool has_value,
                            const std::optional<EventAttribute>& metadata) {
  if (state == FirstClassAttributeState::kNull ||
      (metadata &&
       (metadata->is_null ||
        metadata->first_class_state == FirstClassAttributeState::kNull))) {
    reject_activity_selector(selector, "must be non-null");
  }
  if (!has_value) {
    reject_activity_selector(selector, "is missing");
  }
  if (metadata) validate_activity_attribute_cell(*metadata, selector);
}

void validate_selected_activity_values(const EventLog& log,
                                       std::string_view selector) {
  for (const auto& trace : log.traces) {
    if (trace.events.empty()) {
      throw std::invalid_argument(
          "concept drift: every trace must contain at least one event");
    }
    for (const auto& event : trace.events) {
      // Standard aliases select the corresponding first-class projection.
      if (selector == "concept:name" || selector == "activity") {
        validate_activity_field(selector, event.activity_state,
                                event.has_activity(), event.activity_attribute);
        continue;
      }
      if (selector == "timestamp" || selector == "time:timestamp") {
        validate_activity_field(selector, event.timestamp_state,
                                event.has_timestamp(), event.timestamp_attribute);
        continue;
      }
      if (selector == "resource" || selector == "org:resource") {
        validate_activity_field(selector, event.resource_state,
                                event.has_resource(), event.resource_attribute);
        continue;
      }
      if (selector == "lifecycle" || selector == "lifecycle:transition") {
        validate_activity_field(selector, event.lifecycle_state,
                                event.has_lifecycle(), event.lifecycle_attribute);
        continue;
      }

      const auto* attribute = event_attribute_entry(event, selector);
      if (!attribute) {
        reject_activity_selector(selector, "is missing");
      }
      validate_activity_attribute_cell(*attribute, selector);
    }
  }
}

const std::string* event_value(const Event& event, std::string_view key) {
  if (key.empty() || key == "activity" || key == "concept:name")
    return &event.activity;
  if (key == "timestamp" || key == "time:timestamp")
    return &event.timestamp;
  if (key == "resource" || key == "org:resource") return &event.resource;
  if (key == "lifecycle" || key == "lifecycle:transition")
    return &event.lifecycle;
  return event.attribute(key);
}

const std::string* selected_timestamp_value(const Event& event,
                                            std::string_view selector) {
  const auto first_class = [&](const std::string& value,
                               FirstClassAttributeState state,
                               bool has_value,
                               const std::optional<EventAttribute>& metadata)
      -> const std::string* {
    if (state == FirstClassAttributeState::kNull ||
        (metadata &&
         (metadata->is_null ||
          metadata->first_class_state == FirstClassAttributeState::kNull))) {
      return nullptr;
    }
    return has_value ? &value : nullptr;
  };

  if (selector == "timestamp" || selector == "time:timestamp") {
    return first_class(event.timestamp, event.timestamp_state,
                       event.has_timestamp(), event.timestamp_attribute);
  }
  if (selector == "activity" || selector == "concept:name") {
    return first_class(event.activity, event.activity_state,
                       event.has_activity(), event.activity_attribute);
  }
  if (selector == "resource" || selector == "org:resource") {
    return first_class(event.resource, event.resource_state,
                       event.has_resource(), event.resource_attribute);
  }
  if (selector == "lifecycle" || selector == "lifecycle:transition") {
    return first_class(event.lifecycle, event.lifecycle_state,
                       event.has_lifecycle(), event.lifecycle_attribute);
  }

  const auto* attribute = event_attribute_entry(event, selector);
  if (!attribute || attribute->is_null ||
      attribute->first_class_state == FirstClassAttributeState::kNull) {
    return nullptr;
  }
  return &attribute->value;
}

std::vector<std::string> sorted_activities_from_traces(
    const std::vector<Trace>& traces, std::string_view activity_attribute) {
  std::vector<std::string> activities;
  for (const auto& trace : traces) {
    for (const auto& event : trace.events) {
      const auto* value = event_value(event, activity_attribute);
      if (value) activities.push_back(*value);
    }
  }
  std::sort(activities.begin(), activities.end());
  activities.erase(std::unique(activities.begin(), activities.end()),
                   activities.end());
  return activities;
}

double permutation_p_value(const std::vector<std::vector<double>>& first,
                           const std::vector<std::vector<double>>& second,
                           std::mt19937& rng) {
  const std::size_t n1 = first.size();
  const std::size_t n2 = second.size();
  const std::size_t dimensions = first.front().size();
  std::vector<std::vector<double>> all;
  all.reserve(n1 + n2);
  all.insert(all.end(), first.begin(), first.end());
  all.insert(all.end(), second.begin(), second.end());
  std::vector<std::size_t> indices(all.size());
  std::iota(indices.begin(), indices.end(), 0);

  std::vector<double> mean1(dimensions, 0.0), mean2(dimensions, 0.0);
  for (const auto& row : first)
    for (std::size_t d = 0; d < dimensions; ++d) mean1[d] += row[d];
  for (const auto& row : second)
    for (std::size_t d = 0; d < dimensions; ++d) mean2[d] += row[d];
  for (double& value : mean1) value /= static_cast<double>(n1);
  for (double& value : mean2) value /= static_cast<double>(n2);
  double observed_squared = 0.0;
  for (std::size_t d = 0; d < dimensions; ++d) {
    const double delta = mean1[d] - mean2[d];
    observed_squared += delta * delta;
  }

  // NumPy's legacy RandomState uses a descending Fisher-Yates shuffle with a
  // rejection-sampled mask. std::shuffle has implementation-defined draws and
  // therefore cannot reproduce that public stream across runtimes.
  if (indices.size() > std::numeric_limits<std::uint32_t>::max()) {
    throw std::invalid_argument(
        "concept drift: permutation index range exceeds 32-bit BOSE stream");
  }
  for (std::size_t i = indices.size(); i > 1; --i) {
    const auto upper = static_cast<std::uint32_t>(i - 1);
    std::uint32_t mask = upper;
    mask |= mask >> 1;
    mask |= mask >> 2;
    mask |= mask >> 4;
    mask |= mask >> 8;
    mask |= mask >> 16;
    std::uint32_t draw = 0;
    do {
      draw = rng() & mask;
    } while (draw > upper);
    std::swap(indices[i - 1], indices[draw]);
  }
  std::vector<double> perm1(dimensions, 0.0), perm2(dimensions, 0.0);
  for (std::size_t i = 0; i < n1; ++i)
    for (std::size_t d = 0; d < dimensions; ++d)
      perm1[d] += all[indices[i]][d];
  for (std::size_t i = n1; i < n1 + n2; ++i)
    for (std::size_t d = 0; d < dimensions; ++d)
      perm2[d] += all[indices[i]][d];
  double perm_squared = 0.0;
  for (std::size_t d = 0; d < dimensions; ++d) {
    const double delta = perm1[d] / static_cast<double>(n1) -
                         perm2[d] / static_cast<double>(n2);
    perm_squared += delta * delta;
  }
  return perm_squared >= observed_squared ? 1.0 : 0.0;
}

std::mt19937 make_bose_rng(std::uint64_t seed) {
  if (seed <= std::numeric_limits<std::uint32_t>::max()) {
    return std::mt19937(static_cast<std::uint32_t>(seed));
  }
  const std::uint32_t low = static_cast<std::uint32_t>(seed);
  const std::uint32_t high = static_cast<std::uint32_t>(seed >> 32);
  std::seed_seq native_extension{low, high};
  return std::mt19937(native_extension);
}

}  // namespace

std::vector<std::string> extract_concept_drift_activities(
    const EventLog& log) {
  return extract_concept_drift_activities(log, "concept:name");
}

std::vector<std::string> extract_concept_drift_activities(
    const EventLog& log, std::string_view activity_attribute) {
  if (activity_attribute.empty()) {
    throw std::invalid_argument(
        "concept drift: activity_attribute must not be empty");
  }
  return sorted_activities_from_traces(log.traces, activity_attribute);
}

std::vector<double> extract_concept_drift_features(
    const std::vector<Trace>& traces,
    const std::vector<std::string>& activities) {
  return extract_concept_drift_features(traces, activities,
                                         "concept:name");
}

std::vector<double> extract_concept_drift_features(
    const std::vector<Trace>& traces,
    const std::vector<std::string>& alphabet,
    std::string_view activity_attribute) {
  if (activity_attribute.empty()) {
    throw std::invalid_argument(
        "concept drift: activity_attribute must not be empty");
  }
  const std::size_t alphabet_size = alphabet.size();
  if (alphabet_size > std::numeric_limits<std::size_t>::max() / 3) {
    throw std::invalid_argument("concept drift: activity feature vector is too large");
  }
  // Keep an internal unique index for relation counting while retaining the
  // caller's alphabet positions for the output (including duplicates).
  std::unordered_map<std::string, std::size_t> ids;
  ids.reserve(alphabet_size);
  for (const auto& activity : alphabet) {
    if (!ids.contains(activity)) ids.emplace(activity, ids.size());
  }
  const std::size_t unique_size = ids.size();
  if (unique_size != 0 &&
      unique_size > std::numeric_limits<std::size_t>::max() / unique_size) {
    throw std::invalid_argument("concept drift: activity relation matrix is too large");
  }
  std::vector<std::size_t> trace_counts(unique_size, 0);
  std::vector<std::size_t> follows(unique_size * unique_size, 0);

  for (const auto& trace : traces) {
    std::unordered_set<std::size_t> present;
    present.reserve(trace.events.size());
    std::unordered_set<std::size_t> relation;
    relation.reserve(trace.events.size() * 2);
    std::vector<std::size_t> seen;
    seen.reserve(trace.events.size());
    for (const auto& event : trace.events) {
      const auto* activity = event_value(event, activity_attribute);
      if (!activity) continue;
      const auto target_it = ids.find(*activity);
      if (target_it == ids.end()) {
        throw std::invalid_argument(
            "concept drift: event activity is absent from the supplied alphabet");
      }
      const std::size_t target = target_it->second;
      present.insert(target);
      for (const std::size_t source : seen)
        relation.insert(source * unique_size + target);
      seen.push_back(target);
    }
    for (const auto source : present) ++trace_counts[source];
    for (const auto key : relation) ++follows[key];
  }

  std::vector<std::size_t> ordered_ids;
  ordered_ids.reserve(alphabet_size);
  std::vector<std::size_t> multiplicities(unique_size, 0);
  for (const auto& activity : alphabet) {
    const auto id = ids.at(activity);
    ordered_ids.push_back(id);
    ++multiplicities[id];
  }
  std::vector<std::size_t> always_by_target(unique_size, 0);
  std::vector<std::size_t> sometimes_by_target(unique_size, 0);
  std::vector<std::size_t> never_by_target(unique_size, 0);
  for (std::size_t target = 0; target < unique_size; ++target) {
    std::size_t always = 0, sometimes = 0, never = 0;
    for (std::size_t source = 0; source < unique_size; ++source) {
      if (trace_counts[source] == 0 ||
          follows[source * unique_size + target] == 0) {
        never += multiplicities[source];
      } else if (follows[source * unique_size + target] ==
                 trace_counts[source]) {
        always += multiplicities[source];
      } else {
        sometimes += multiplicities[source];
      }
    }
    always_by_target[target] = always;
    sometimes_by_target[target] = sometimes;
    never_by_target[target] = never;
  }

  std::vector<double> features;
  features.reserve(alphabet_size * 3);
  for (const std::size_t target : ordered_ids) {
    features.push_back(static_cast<double>(always_by_target[target]));
    features.push_back(static_cast<double>(sometimes_by_target[target]));
    features.push_back(static_cast<double>(never_by_target[target]));
  }
  return features;
}

std::vector<double> extract_concept_drift_features(const EventLog& log) {
  return extract_concept_drift_features(log.traces,
                                        extract_concept_drift_activities(log),
                                        "concept:name");
}

ConceptDriftResult detect_concept_drift(const EventLog& log,
                                        ConceptDriftOptions options) {
  if (options.activity_attribute.empty()) {
    throw std::invalid_argument(
        "concept drift: activity_attribute must not be empty");
  }
  // Reject oversized trace materialization before inspecting every event or
  // deriving the alphabet. Event- and feature-dependent bounds are checked
  // again below once their sizes are known.
  validate_options(options, log.traces.size(), 0, 0);
  validate_selected_activity_values(log, options.activity_attribute);
  const auto activities = extract_concept_drift_activities(
      log, options.activity_attribute);
  if (activities.size() > std::numeric_limits<std::size_t>::max() / 3) {
    throw std::invalid_argument("concept drift: activity feature vector is too large");
  }
  std::size_t event_count = 0;
  for (const auto& trace : log.traces) {
    if (trace.events.size() > std::numeric_limits<std::size_t>::max() - event_count)
      throw std::invalid_argument("concept drift: event count is too large");
    event_count += trace.events.size();
  }
  validate_options(options, log.traces.size(), event_count,
                   activities.size() * 3);

  // BOSE validates the first event timestamp of every grouped trace before
  // doing any window work. Cache those values independently of whether the
  // caller asks for timestamp arrays in the result; later events are not part
  // of this case-start contract.
  std::vector<double> case_start_times;
  case_start_times.reserve(log.traces.size());
  for (const auto& trace : log.traces) {
    const auto* timestamp = selected_timestamp_value(
        trace.events.front(), options.timestamp_attribute);
    if (!timestamp || timestamp->empty()) {
      throw std::invalid_argument(
          "concept drift: selected case-start timestamp is missing");
    }
    const auto parsed = precise_epoch_seconds(*timestamp);
    if (!parsed || !std::isfinite(*parsed)) {
      throw std::invalid_argument(
          "concept drift: case-start timestamp is not finite");
    }
    case_start_times.push_back(*parsed);
  }

  ConceptDriftResult result;
  result.variant = options.variant;
  result.activities = activities;
  result.activity_attribute = options.activity_attribute;
  result.timestamp_attribute = options.timestamp_attribute;
  result.case_id_attribute = options.case_id_attribute;
  if (!log.traces.empty()) {
    const std::size_t sublog_count =
        (log.traces.size() - 1) / options.sub_log_size + 1;
    result.sublogs.reserve(sublog_count);
    result.feature_vectors.reserve(sublog_count);
    result.cumulative_sublogs.reserve(sublog_count);
    result.sublog_start_timestamps.reserve(sublog_count);
  }

  for (std::size_t start = 0; start < log.traces.size();
       start += options.sub_log_size) {
    const std::size_t end = std::min(
        log.traces.size(),
        start > std::numeric_limits<std::size_t>::max() - options.sub_log_size
            ? std::numeric_limits<std::size_t>::max()
            : start + options.sub_log_size);
    EventLog raw;
    copy_event_log_context(log, raw);
    raw.traces.insert(raw.traces.end(), log.traces.begin() + start,
                      log.traces.begin() + end);
    result.sublogs.push_back(raw);
    result.feature_vectors.push_back(
        extract_concept_drift_features(raw.traces, activities,
                                       options.activity_attribute));
    // Keep this field cumulative from the beginning, as requested by the API.
    EventLog cumulative_copy;
    copy_event_log_context(log, cumulative_copy);
    cumulative_copy.traces.insert(cumulative_copy.traces.end(), log.traces.begin(),
                                   log.traces.begin() + end);
    result.cumulative_sublogs.push_back(std::move(cumulative_copy));
    if (options.include_timestamps)
      result.sublog_start_timestamps.push_back(case_start_times[start]);
  }

  if (result.feature_vectors.size() >= 2 * options.window_size) {
    std::mt19937 rng = make_bose_rng(options.seed);
    const std::size_t windows =
        result.feature_vectors.size() - 2 * options.window_size + 1;
    std::vector<ConceptDriftChangePoint> candidates;
    candidates.reserve(windows);
    for (std::size_t i = 0; i < windows; ++i) {
      const auto first_begin = result.feature_vectors.begin() + i;
      const auto first_end = first_begin + options.window_size;
      const auto second_end = first_end + options.window_size;
      std::vector<std::vector<double>> first(first_begin, first_end);
      std::vector<std::vector<double>> second(first_end, second_end);
      double exceedances = 0.0;
      for (std::size_t permutation = 0; permutation < options.num_permutations;
           ++permutation) {
        exceedances += permutation_p_value(first, second, rng);
      }
      const double p_value = exceedances / options.num_permutations;
      if (p_value < options.threshold) {
        candidates.push_back({i + options.window_size, p_value});
      }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& lhs, const auto& rhs) {
                if (lhs.p_value != rhs.p_value) return lhs.p_value < rhs.p_value;
                return lhs.sublog_index < rhs.sublog_index;
              });
    if (candidates.size() > options.max_no_change_points)
      candidates.resize(options.max_no_change_points);
    result.change_points = candidates;
    if (options.include_timestamps) {
      result.change_timestamps.reserve(candidates.size());
      for (const auto& point : candidates) {
        result.change_timestamps.push_back(
            result.sublog_start_timestamps.at(point.sublog_index));
      }
    }
  }

  if (!result.sublogs.empty()) {
    std::vector<bool> drift_boundaries(result.sublogs.size() + 1, false);
    for (const auto& point : result.change_points) {
      if (point.sublog_index > 0 &&
          point.sublog_index < drift_boundaries.size()) {
        drift_boundaries[point.sublog_index] = true;
      }
    }

    EventLog segment;
    copy_event_log_context(log, segment);
    result.returned_sublogs.reserve(result.change_points.size() + 1);
    for (std::size_t i = 0; i < result.sublogs.size(); ++i) {
      const auto& raw = result.sublogs[i];
      segment.traces.insert(segment.traces.end(), raw.traces.begin(),
                            raw.traces.end());
      if (drift_boundaries[i + 1] || i + 1 == result.sublogs.size()) {
        result.returned_sublogs.push_back(std::move(segment));
        segment = EventLog{};
        copy_event_log_context(log, segment);
      }
    }
  }
  return result;
}

}  // namespace minera
