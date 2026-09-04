#include "minera/dfg_precision.hpp"
#include "minera/log_conversion.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace minera {
namespace {

using Prefix = std::vector<std::string>;
using ActivitySet = std::set<std::string>;

const std::string& event_activity(const Event& event,
                                  std::string_view attribute) {
  if (attribute == "activity" || attribute == "concept:name" ||
      attribute == "Activity")
    return event.activity;
  // Flat adapters may project a selected source column into Event::activity
  // while retaining the original spelling separately. Resolve that
  // first-class source before looking at ordinary attributes so custom
  // selectors keep working after a round trip through a stream or table.
  if ((!event.activity_source_key.empty() &&
       event.activity_source_key == attribute) ||
      (event.activity_attribute &&
       event.activity_attribute->key == attribute))
    return event.activity;
  const auto* value = event.attribute(attribute);
  if (!value)
    throw std::invalid_argument("DFG precision: missing activity attribute " +
                                std::string(attribute));
  return *value;
}

bool allowed_prefix(const Prefix& prefix,
                    const ActivitySet& starts,
                    const std::map<std::string, ActivitySet>& exiting) {
  if (prefix.empty()) return true;
  if (!starts.contains(prefix.front())) return false;
  for (std::size_t i = 1; i < prefix.size(); ++i) {
    const auto previous = exiting.find(prefix[i - 1]);
    if (previous == exiting.end() || !previous->second.contains(prefix[i]))
      return false;
  }
  return exiting.contains(prefix.back());
}

std::string json_quote(std::string_view value) {
  std::ostringstream out;
  out << '"';
  for (const unsigned char character : value) {
    switch (character) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (character < 0x20U) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<unsigned int>(character) << std::dec
              << std::setfill(' ');
        } else {
          out << static_cast<char>(character);
        }
    }
  }
  out << '"';
  return out.str();
}

int dfg_activity_selector_priority(const EventAttribute& field,
                                   std::string_view selector) {
  if (field.key == selector) return 3;
  if (!field.first_class_source_key.empty() &&
      field.first_class_source_key == selector)
    return 2;
  const bool alias = selector == "concept:name" || selector == "activity";
  return alias && (field.key == "concept:name" || field.key == "activity")
             ? 1
             : 0;
}

int dfg_case_selector_priority(const EventAttribute& field,
                               std::string_view selector) {
  if (field.key == selector) return 3;
  if (!field.first_class_source_key.empty() &&
      field.first_class_source_key == selector)
    return 2;
  const bool alias = selector == "case:concept:name" ||
                     selector == "case:id" || selector == "case_id" ||
                     selector == "case";
  return alias && (field.key == "case:concept:name" ||
                   field.key == "case:id" || field.key == "case_id" ||
                   field.key == "case")
             ? 1
             : 0;
}

std::optional<std::size_t> dfg_selected_field(
    const EventRecord& record, std::string_view selector,
    std::string_view name,
    int (*priority)(const EventAttribute&, std::string_view)) {
  int best = 0;
  std::optional<std::size_t> selected;
  for (std::size_t index = 0; index < record.fields.size(); ++index) {
    const int current = priority(record.fields[index], selector);
    if (current == 0) continue;
    if (current > best) {
      best = current;
      selected = index;
    } else if (current == best) {
      throw std::invalid_argument("DFG precision: " + std::string(name) +
                                  " selector is ambiguous: " +
                                  std::string(selector));
    }
  }
  return selected;
}

void project_dfg_stream(EventStream& stream,
                        const DfgPrecisionOptions& options) {
  for (auto& record : stream.events) {
    const auto case_index = dfg_selected_field(
        record, options.case_id_attribute, "case-id",
        dfg_case_selector_priority);
    const auto activity_index = dfg_selected_field(
        record, options.activity_attribute, "activity",
        dfg_activity_selector_priority);
    const auto selected_case =
        case_index ? std::optional<EventAttribute>(record.fields[*case_index])
                   : std::nullopt;
    const auto selected_activity =
        activity_index ? std::optional<EventAttribute>(record.fields[*activity_index])
                       : std::nullopt;
    for (auto& field : record.fields) {
      field.first_class_role = FirstClassAttributeRole::kNone;
      field.first_class_state = FirstClassAttributeState::kMissing;
      field.first_class_source_key.clear();
    }
    const auto append_projection = [&](EventAttribute field,
                                       FirstClassAttributeRole role,
                                       std::string_view source_key) {
      const auto state = field.first_class_role == FirstClassAttributeRole::kNone
                             ? (field.is_null
                                    ? FirstClassAttributeState::kNull
                                    : FirstClassAttributeState::kPresent)
                             : field.first_class_state;
      field.first_class_role = role;
      field.first_class_state = state;
      field.first_class_source_key = std::string(source_key);
      record.fields.push_back(std::move(field));
    };
    if (selected_case) {
      append_projection(*selected_case, FirstClassAttributeRole::kCaseId,
                        options.case_id_attribute);
    }
    if (selected_activity) {
      append_projection(*selected_activity,
                        FirstClassAttributeRole::kActivity,
                        options.activity_attribute);
    }
  }
}

}  // namespace

DfgPrecisionResult evaluate_dfg_precision(
    const EventLog& log, const DfgCleanAnalysis& model,
    const DfgPrecisionOptions& options) {
  if (options.activity_attribute.empty())
    throw std::invalid_argument("DFG precision: activity attribute must not be empty");
  if (options.max_rows_to_materialize &&
      log.event_count() > *options.max_rows_to_materialize)
    throw std::length_error("DFG precision: row materialization cap exceeded");

  std::map<std::string, ActivitySet> exiting;
  for (const auto& [edge, count] : model.graph) {
    if (edge.first.empty() || edge.second.empty())
      throw std::invalid_argument("DFG precision: model labels must not be empty");
    // The graph maps describe the model relation; their frequencies are not
    // used by escaping-edge precision. Keep zero-frequency entries visible so
    // imported sparse graphs retain their declared edges.
    exiting[edge.first].insert(edge.second);
  }
  ActivitySet starts;
  for (const auto& [activity, count] : model.start_activities) {
    if (activity.empty())
      throw std::invalid_argument("DFG precision: model labels must not be empty");
    // As with graph edges, a declared start key is a model start relation even
    // when its optional frequency is zero.
    starts.insert(activity);
  }

  struct PrefixObservation {
    ActivitySet next;
    std::uint64_t count = 0;
  };
  std::map<Prefix, PrefixObservation> observations;
  for (const auto& trace : log.traces) {
    Prefix prefix;
    for (const auto& event : trace.events) {
      const auto& activity = event_activity(event, options.activity_attribute);
      auto& observation = observations[prefix];
      observation.next.insert(activity);
      if (observation.count == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("DFG precision: prefix count overflow");
      ++observation.count;
      prefix.push_back(activity);
    }
  }

  DfgPrecisionResult result;
  for (const auto& [prefix, observation] : observations) {
    if (result.prefix_occurrences >
        std::numeric_limits<std::uint64_t>::max() - observation.count)
      throw std::overflow_error("DFG precision: prefix count overflow");
    result.prefix_occurrences += observation.count;
    if (!allowed_prefix(prefix, starts, exiting)) continue;

    if (result.allowed_prefix_occurrences >
        std::numeric_limits<std::uint64_t>::max() - observation.count)
      throw std::overflow_error("DFG precision: prefix count overflow");
    result.allowed_prefix_occurrences += observation.count;
    const ActivitySet* activated = nullptr;
    if (prefix.empty()) {
      activated = &starts;
    } else {
      const auto it = exiting.find(prefix.back());
      if (it == exiting.end()) continue;
      activated = &it->second;
    }
    const auto weight = observation.count;
    if (weight != 0 && activated->size() >
                           std::numeric_limits<std::uint64_t>::max() / weight)
      throw std::overflow_error("DFG precision: transition count overflow");
    const auto activated_count =
        static_cast<std::uint64_t>(activated->size()) * weight;
    if (activated_count > std::numeric_limits<std::uint64_t>::max() -
                              result.activated_transitions)
      throw std::overflow_error("DFG precision: transition count overflow");
    result.activated_transitions += activated_count;
    std::uint64_t escaping = 0;
    for (const auto& activity : *activated)
      if (!observation.next.contains(activity)) ++escaping;
    if (weight != 0 && escaping >
                           std::numeric_limits<std::uint64_t>::max() / weight)
      throw std::overflow_error("DFG precision: transition count overflow");
    const auto escaping_count = escaping * weight;
    if (escaping_count > std::numeric_limits<std::uint64_t>::max() -
                             result.escaping_transitions)
      throw std::overflow_error("DFG precision: transition count overflow");
    result.escaping_transitions += escaping_count;
  }
  if (result.activated_transitions != 0) {
    result.precision =
        1.0 - static_cast<double>(result.escaping_transitions) /
                    static_cast<double>(result.activated_transitions);
    result.precision = std::clamp(result.precision, 0.0, 1.0);
  }
  result.computed = true;
  return result;
}

DfgPrecisionResult evaluate_dfg_precision(
    const EventDataView& data, const DfgCleanAnalysis& model,
    const DfgPrecisionOptions& options) {
  if (options.activity_attribute.empty())
    throw std::invalid_argument(
        "DFG precision: activity attribute must not be empty");
  if (options.case_id_attribute.empty())
    throw std::invalid_argument(
        "DFG precision: case-id attribute must not be empty");

  if (options.max_rows_to_materialize) {
    std::optional<std::size_t> known_rows;
    switch (data.kind()) {
      case EventDataKind::kEventLog:
        known_rows = data.log()->event_count();
        break;
      case EventDataKind::kEventStream:
        known_rows = data.stream()->events.size();
        break;
      case EventDataKind::kEventTable:
        known_rows = data.table()->rows.size();
        break;
      case EventDataKind::kLazyEventTable:
        known_rows = data.lazy_table()->row_count();
        break;
    }
    if (known_rows && *known_rows > *options.max_rows_to_materialize)
      throw std::length_error("DFG precision: row materialization cap exceeded");
  }

  LogConversionOptions conversion;
  conversion.case_id_key = options.case_id_attribute;
  conversion.max_rows_to_materialize = options.max_rows_to_materialize;
  auto stream = event_data_to_event_stream(data, conversion);
  project_dfg_stream(stream, options);
  return evaluate_dfg_precision(event_stream_to_event_log(stream, conversion),
                                model, options);
}

std::string serialize_dfg_precision_json(const DfgPrecisionResult& result) {
  if (!result.computed || !std::isfinite(result.precision) ||
      result.precision < 0.0 || result.precision > 1.0)
    throw std::invalid_argument("DFG precision: result is not serializable");
  std::ostringstream out;
  out << std::setprecision(17)
      << "{\"family\":\"DFG_PRECISION\",\"computed\":"
      << (result.computed ? "true" : "false")
      << ",\"precision\":" << result.precision
      << ",\"prefix_occurrences\":" << result.prefix_occurrences
      << ",\"allowed_prefix_occurrences\":"
      << result.allowed_prefix_occurrences
      << ",\"activated_transitions\":" << result.activated_transitions
      << ",\"escaping_transitions\":" << result.escaping_transitions
      << ",\"error\":" << json_quote(result.error) << '}';
  return out.str();
}

}  // namespace minera
