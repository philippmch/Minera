#include "minera/event_table_json.hpp"
#include "minera/log_conversion.hpp"
#include "minera/timestamp.hpp"
#include "minera/xes_event_log_importer.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <limits>
#include <locale>
#include <cstdint>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <variant>

namespace minera {
namespace {

constexpr std::string_view kActivityKey = "concept:name";
constexpr std::string_view kTimestampKey = "time:timestamp";
constexpr std::string_view kResourceKey = "org:resource";
constexpr std::string_view kLifecycleKey = "lifecycle:transition";

struct StandardExtension {
  std::string_view prefix;
  std::string_view name;
  std::string_view uri;
};

constexpr StandardExtension kStandardExtensions[] = {
    {"concept", "Concept", "http://www.xes-standard.org/concept.xesext"},
    {"time", "Time", "http://www.xes-standard.org/time.xesext"},
    {"org", "Organizational", "http://www.xes-standard.org/org.xesext"},
    {"lifecycle", "Lifecycle",
     "http://www.xes-standard.org/lifecycle.xesext"},
    {"cost", "Cost", "http://www.xes-standard.org/cost.xesext"},
    {"identity", "Identity",
     "http://www.xes-standard.org/identity.xesext"},
    {"artifactlifecycle", "Artifact Lifecycle",
     "http://www.xes-standard.org/artifactlifecycle.xesext"},
    {"micro", "Micro", "http://www.xes-standard.org/micro.xesext"},
    {"semantic", "Semantic",
     "http://www.xes-standard.org/semantic.xesext"},
    {"swcomm", "Software Communication",
     "http://www.xes-standard.org/swcomm.xesext"},
    {"swevent", "Software Event",
     "http://www.xes-standard.org/swevent.xesext"},
    {"swtelemetry", "Software Telemetry",
     "http://www.xes-standard.org/swtelemetry.xesext"},
};

void upsert(std::vector<EventAttribute>& attributes, EventAttribute value) {
  const auto found = std::find_if(
      attributes.begin(), attributes.end(), [&](const auto& attribute) {
        return attribute.key == value.key;
      });
  if (found == attributes.end())
    attributes.push_back(std::move(value));
  else
    *found = std::move(value);
}

bool first_class_present(FirstClassAttributeState state,
                         std::string_view value) {
  return state != FirstClassAttributeState::kMissing || !value.empty();
}

FirstClassAttributeState effective_first_class_state(
    FirstClassAttributeState state, std::string_view value) {
  return state == FirstClassAttributeState::kMissing && !value.empty()
             ? FirstClassAttributeState::kPresent
             : state;
}

void add_projection(std::vector<EventAttribute>& attributes,
                     std::string_view source_key,
                     std::string_view canonical_key,
                     const std::string& value, AttributeKind kind,
                     FirstClassAttributeState state,
                     FirstClassAttributeRole role,
                     const EventAttribute* metadata = nullptr) {
  if (!first_class_present(state, value)) return;
  const auto resolved_key =
      source_key.empty() ? std::string(canonical_key) : std::string(source_key);
  EventAttribute projection{resolved_key, value, kind,
                            state == FirstClassAttributeState::kNull};
  const auto effective_state = effective_first_class_state(state, value);
  if (metadata && metadata->value == value &&
      metadata->is_null == projection.is_null) {
    // Keep the selected parent type and complete nested payload. The string
    // projection remains authoritative if a caller edited the Event field
    // after import; stale native values are never copied in that case.
    projection.kind = metadata->kind;
    projection.typed_value = metadata->typed_value;
    projection.container = metadata->container;
    projection.children = metadata->children;
    projection.scalar_wrapper = metadata->scalar_wrapper;
    projection.scalar_children_list = metadata->scalar_children_list;
  }
  projection.first_class_role = role;
  projection.first_class_state = effective_state;
  projection.first_class_source_key = resolved_key;
  upsert(attributes, std::move(projection));
}

void append_or_merge_attribute(std::vector<EventAttribute>& fields,
                               const EventAttribute& attribute) {
  const auto found = std::find_if(
      fields.begin(), fields.end(), [&](const auto& field) {
        return field.key == attribute.key;
      });
  if (found == fields.end()) {
    fields.push_back(attribute);
    return;
  }
  if (found->first_class_role == FirstClassAttributeRole::kNone ||
      (attribute.children.empty() && !attribute.scalar_wrapper && !attribute.scalar_children_list))
    return;
  // A selected scalar can still carry nested/meta attributes. Preserve those
  // children on the projected cell without creating a duplicate column.
  found->children = attribute.children;
  found->scalar_wrapper = attribute.scalar_wrapper;
  found->scalar_children_list = attribute.scalar_children_list;
}

std::vector<EventAttribute> event_fields(const Event& event) {
  std::vector<EventAttribute> fields;
  fields.reserve(event.attributes.size() + 4);
  add_projection(fields, event.activity_source_key, kActivityKey,
                 event.activity, AttributeKind::kString, event.activity_state,
                 FirstClassAttributeRole::kActivity,
                 event.activity_attribute ? &*event.activity_attribute : nullptr);
  add_projection(fields, event.timestamp_source_key, kTimestampKey,
                 event.timestamp, AttributeKind::kDate, event.timestamp_state,
                 FirstClassAttributeRole::kTimestamp,
                 event.timestamp_attribute ? &*event.timestamp_attribute : nullptr);
  add_projection(fields, event.resource_source_key, kResourceKey,
                 event.resource, AttributeKind::kString, event.resource_state,
                 FirstClassAttributeRole::kResource,
                 event.resource_attribute ? &*event.resource_attribute : nullptr);
  add_projection(fields, event.lifecycle_source_key, kLifecycleKey,
                 event.lifecycle, AttributeKind::kString,
                 event.lifecycle_state,
                 FirstClassAttributeRole::kLifecycle,
                 event.lifecycle_attribute ? &*event.lifecycle_attribute : nullptr);
  for (const auto& attribute : event.attributes)
    append_or_merge_attribute(fields, attribute);
  return fields;
}

std::vector<EventAttribute> trace_fields(const Trace& trace) {
  std::vector<EventAttribute> fields;
  fields.reserve(trace.attributes.size() + 1);
  add_projection(fields, trace.case_id_source_key, kActivityKey, trace.case_id,
                 AttributeKind::kString, trace.case_id_state,
                 FirstClassAttributeRole::kCaseId,
                 trace.case_id_attribute ? &*trace.case_id_attribute : nullptr);
  for (const auto& attribute : trace.attributes)
    append_or_merge_attribute(fields, attribute);
  return fields;
}

const EventAttribute* find_attribute(
    const std::vector<EventAttribute>& attributes, std::string_view key) {
  const auto found = std::find_if(
      attributes.begin(), attributes.end(), [&](const auto& attribute) {
        return attribute.key == key;
      });
  return found == attributes.end() ? nullptr : &*found;
}

std::optional<EventAttribute> selected_trace_attribute(
    const Trace& trace, std::string_view key) {
  if (key == "concept:name" || key == "case_id" ||
      key == "case:concept:name") {
    if (!first_class_present(trace.case_id_state, trace.case_id))
      return std::nullopt;
    auto selected = trace.case_id_attribute.value_or(EventAttribute{
        std::string(key), trace.case_id, AttributeKind::kString,
        trace.case_id_state == FirstClassAttributeState::kNull});
    selected.key = std::string(key);
    selected.first_class_role = FirstClassAttributeRole::kNone;
    selected.first_class_state = FirstClassAttributeState::kMissing;
    selected.first_class_source_key.clear();
    return selected;
  }
  if (!trace.case_id_source_key.empty() &&
      key == trace.case_id_source_key &&
      first_class_present(trace.case_id_state, trace.case_id)) {
    auto selected = trace.case_id_attribute.value_or(EventAttribute{
        std::string(key), trace.case_id, AttributeKind::kString,
        trace.case_id_state == FirstClassAttributeState::kNull});
    selected.key = std::string(key);
    selected.first_class_role = FirstClassAttributeRole::kNone;
    selected.first_class_state = FirstClassAttributeState::kMissing;
    selected.first_class_source_key.clear();
    return selected;
  }
  if (const auto* value = find_attribute(trace.attributes, key)) {
    if (value->is_null) return std::nullopt;
    return *value;
  }
  return std::nullopt;
}

std::optional<EventAttribute> selected_event_attribute(
    const Event& event, std::string_view key) {
  if (key == "concept:name" || key == "activity") {
    if (!first_class_present(event.activity_state, event.activity))
      return std::nullopt;
    auto selected = event.activity_attribute.value_or(EventAttribute{
        std::string(key), event.activity, AttributeKind::kString,
        event.activity_state == FirstClassAttributeState::kNull});
    selected.key = std::string(key);
    selected.first_class_role = FirstClassAttributeRole::kNone;
    selected.first_class_state = FirstClassAttributeState::kMissing;
    selected.first_class_source_key.clear();
    return selected;
  }
  if (key == "time:timestamp" || key == "timestamp") {
    if (!first_class_present(event.timestamp_state, event.timestamp))
      return std::nullopt;
    auto selected = event.timestamp_attribute.value_or(EventAttribute{
        std::string(key), event.timestamp, AttributeKind::kDate,
        event.timestamp_state == FirstClassAttributeState::kNull});
    selected.key = std::string(key);
    selected.first_class_role = FirstClassAttributeRole::kNone;
    selected.first_class_state = FirstClassAttributeState::kMissing;
    selected.first_class_source_key.clear();
    return selected;
  }
  if (key == "org:resource" || key == "resource") {
    if (!first_class_present(event.resource_state, event.resource))
      return std::nullopt;
    auto selected = event.resource_attribute.value_or(EventAttribute{
        std::string(key), event.resource, AttributeKind::kString,
        event.resource_state == FirstClassAttributeState::kNull});
    selected.key = std::string(key);
    selected.first_class_role = FirstClassAttributeRole::kNone;
    selected.first_class_state = FirstClassAttributeState::kMissing;
    selected.first_class_source_key.clear();
    return selected;
  }
  if (key == "lifecycle:transition" || key == "lifecycle") {
    if (!first_class_present(event.lifecycle_state, event.lifecycle))
      return std::nullopt;
    auto selected = event.lifecycle_attribute.value_or(EventAttribute{
        std::string(key), event.lifecycle, AttributeKind::kString,
        event.lifecycle_state == FirstClassAttributeState::kNull});
    selected.key = std::string(key);
    selected.first_class_role = FirstClassAttributeRole::kNone;
    selected.first_class_state = FirstClassAttributeState::kMissing;
    selected.first_class_source_key.clear();
    return selected;
  }
  if (!event.activity_source_key.empty() && key == event.activity_source_key &&
      first_class_present(event.activity_state, event.activity)) {
    auto selected = event.activity_attribute.value_or(EventAttribute{
        std::string(key), event.activity, AttributeKind::kString,
        event.activity_state == FirstClassAttributeState::kNull});
    selected.key = std::string(key);
    selected.first_class_role = FirstClassAttributeRole::kNone;
    selected.first_class_state = FirstClassAttributeState::kMissing;
    selected.first_class_source_key.clear();
    return selected;
  }
  if (!event.timestamp_source_key.empty() &&
      key == event.timestamp_source_key &&
      first_class_present(event.timestamp_state, event.timestamp)) {
    auto selected = event.timestamp_attribute.value_or(EventAttribute{
        std::string(key), event.timestamp, AttributeKind::kDate,
        event.timestamp_state == FirstClassAttributeState::kNull});
    selected.key = std::string(key);
    selected.first_class_role = FirstClassAttributeRole::kNone;
    selected.first_class_state = FirstClassAttributeState::kMissing;
    selected.first_class_source_key.clear();
    return selected;
  }
  if (!event.resource_source_key.empty() && key == event.resource_source_key &&
      first_class_present(event.resource_state, event.resource)) {
    auto selected = event.resource_attribute.value_or(EventAttribute{
        std::string(key), event.resource, AttributeKind::kString,
        event.resource_state == FirstClassAttributeState::kNull});
    selected.key = std::string(key);
    selected.first_class_role = FirstClassAttributeRole::kNone;
    selected.first_class_state = FirstClassAttributeState::kMissing;
    selected.first_class_source_key.clear();
    return selected;
  }
  if (!event.lifecycle_source_key.empty() &&
      key == event.lifecycle_source_key &&
      first_class_present(event.lifecycle_state, event.lifecycle)) {
    auto selected = event.lifecycle_attribute.value_or(EventAttribute{
        std::string(key), event.lifecycle, AttributeKind::kString,
        event.lifecycle_state == FirstClassAttributeState::kNull});
    selected.key = std::string(key);
    selected.first_class_role = FirstClassAttributeRole::kNone;
    selected.first_class_state = FirstClassAttributeState::kMissing;
    selected.first_class_source_key.clear();
    return selected;
  }
  if (const auto* value = find_attribute(event.attributes, key)) {
    if (value->is_null) return std::nullopt;
    return *value;
  }
  return std::nullopt;
}

void validate_options(const LogConversionOptions& options) {
  if (options.case_id_key.empty())
    throw std::invalid_argument("log conversion: case_id_key must not be empty");
  if (options.graph_case_id_attribute.empty())
    throw std::invalid_argument(
        "log conversion: graph_case_id_attribute must not be empty");
  if (options.max_graph_nodes == 0 || options.max_graph_edges == 0)
    throw std::invalid_argument(
        "log conversion: graph bounds must be positive");
  for (const auto* attributes : {&options.case_attributes_as_nodes,
                                 &options.event_attributes_as_nodes}) {
    if (std::any_of(attributes->begin(), attributes->end(),
                    [](const auto& key) { return key.empty(); }))
      throw std::invalid_argument(
          "log conversion: attribute-node keys must not be empty");
  }
}

void enforce_event_log_row_cap(const EventLog& log,
                               const LogConversionOptions& options) {
  // EventLog-to-table conversion has one output row per input event. Check
  // the complete count before flattening so eager and lazy outputs fail with
  // the same deterministic result and never partially materialize a table.
  if (options.max_rows_to_materialize &&
      log.event_count() > *options.max_rows_to_materialize)
    throw std::length_error("log conversion: row materialization cap exceeded");
}

void infer_standard_extensions(const LogConversionTable& table,
                               EventDataMetadata& metadata) {
  for (const auto& column : table.columns) {
    const auto separator = column.find(':');
    if (separator == std::string::npos || separator == 0) continue;
    const auto prefix = std::string_view(column).substr(0, separator);
    const auto found = std::find_if(
        std::begin(kStandardExtensions), std::end(kStandardExtensions),
        [&](const auto& extension) { return extension.prefix == prefix; });
    if (found == std::end(kStandardExtensions)) continue;
    const auto already_declared = std::any_of(
        metadata.extensions.begin(), metadata.extensions.end(),
        [&](const auto& extension) { return extension.prefix == prefix; });
    if (already_declared) continue;
    metadata.extensions.push_back(
        {std::string(found->name), std::string(found->prefix),
         std::string(found->uri)});
  }
}

// Numeric Python dictionary keys compare by value across bool/int/float.
// Canonical integer strings retain exact uint64/int64 boundaries without
// rounding through double; nonintegral/out-of-range floats have separate keys.
std::pair<AttributeKind, std::string> scalar_grouping_key(
    const EventAttribute& value) {
  const auto original = std::pair{value.kind, std::string("lexical:") + value.value};
  if (value.is_null) return original;
  if (const auto* date = std::get_if<NativeDate>(&value.typed_value)) {
    // NativeDate.value is the authoritative source spelling.  Keep naive and
    // aware values in distinct domains, while aware values compare by their
    // exact normalized instant rather than by offset spelling.  Invalid
    // native dates retain a typed-value-specific key so they cannot alias a
    // valid date through a stale EventAttribute::value projection.
    const auto parsed = parse_timestamp_exact(date->value);
    if (!parsed) {
      return {AttributeKind::kDate, "invalid-date:" + date->value};
    }
    return {AttributeKind::kDate,
            std::string("date:") + (parsed->timezone_aware ? "aware:" : "naive:") +
                std::to_string(parsed->seconds) + ":" +
                std::to_string(parsed->nanoseconds)};
  }
  const auto integer = [](auto number) {
    return std::pair{AttributeKind::kInt, std::string("integer:") + std::to_string(number)};
  };
  const auto floating = [&](double number) {
    if (std::isnan(number))
      return std::pair{AttributeKind::kFloat, std::string("nan:") +
          std::to_string(value.typed_value.scalar_identity())};
    if (std::isfinite(number) && std::trunc(number) == number) {
      if (number >= 0 && number < 18446744073709551616.0)
        return integer(static_cast<std::uint64_t>(number));
      if (number < 0 && number >= -9223372036854775808.0)
        return integer(static_cast<std::int64_t>(number));
    }
    std::ostringstream text;
    text.imbue(std::locale::classic());
    text << std::hexfloat << number;
    return std::pair{AttributeKind::kFloat, std::string("float:") + text.str()};
  };
  if (const auto* number = std::get_if<std::int64_t>(&value.typed_value)) return integer(*number);
  if (const auto* number = std::get_if<std::uint64_t>(&value.typed_value)) return integer(*number);
  if (const auto* number = std::get_if<bool>(&value.typed_value)) return integer(*number ? 1 : 0);
  if (const auto* number = std::get_if<double>(&value.typed_value)) return floating(*number);
  if (!std::holds_alternative<std::monostate>(value.typed_value)) return original;
  if (value.kind == AttributeKind::kBoolean) {
    if (value.value == "true" || value.value == "True" || value.value == "1") return integer(1);
    if (value.value == "false" || value.value == "False" || value.value == "0") return integer(0);
  }
  std::string_view text = value.value;
  if (text.starts_with('+')) text.remove_prefix(1);
  const auto parse = [&](auto& number) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), number);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
  };
  if (value.kind == AttributeKind::kInt) {
    if (text.starts_with('-')) {
      std::int64_t number = 0;
      if (parse(number)) return integer(number);
    } else {
      std::uint64_t number = 0;
      if (parse(number)) return integer(number);
    }
  } else if (value.kind == AttributeKind::kFloat) {
    double number = 0;
    if (parse(number)) return floating(number);
  }
  return original;
}

void append_grouping_part(std::string& result, std::string_view part) {
  // Length delimiters keep values such as ("a:b", "c") distinct from
  // ("a", "b:c") and avoid relying on an escaped separator alphabet.
  result += std::to_string(part.size());
  result.push_back(':');
  result.append(part);
}

std::string grouping_component(const EventAttribute& value) {
  if (value.is_null || value.container == AttributeContainerKind::kScalar) {
    const auto scalar = scalar_grouping_key(value);
    std::string result = "scalar";
    result.push_back(value.is_null ? '1' : '0');
    result += std::to_string(static_cast<int>(scalar.first));
    append_grouping_part(result, scalar.second);
    return result;
  }
  switch (value.container) {
    case AttributeContainerKind::kScalar:
      break;
    case AttributeContainerKind::kTuple:
    case AttributeContainerKind::kFrozenSet: {
      std::vector<std::string> children;
      children.reserve(value.children.size());
      for (const auto& child : value.children)
        children.push_back(grouping_component(child));
      if (value.container == AttributeContainerKind::kFrozenSet) {
        std::sort(children.begin(), children.end());
        children.erase(std::unique(children.begin(), children.end()),
                       children.end());
      }
      std::string result = value.container == AttributeContainerKind::kTuple
                               ? "tuple"
                               : "frozenset";
      append_grouping_part(result, value.is_null ? "1" : "0");
      append_grouping_part(result, std::to_string(static_cast<int>(value.kind)));
      append_grouping_part(result, std::to_string(children.size()));
      for (const auto& child : children) append_grouping_part(result, child);
      return result;
    }
    case AttributeContainerKind::kList:
      throw std::invalid_argument("unhashable type: 'list'");
    case AttributeContainerKind::kContainer:
      throw std::invalid_argument("unhashable type: 'dict'");
    case AttributeContainerKind::kSet:
      throw std::invalid_argument("unhashable type: 'set'");
  }
  throw std::invalid_argument("log conversion: invalid attribute container kind");
}

std::pair<AttributeKind, std::string> case_grouping_scalar(
    const EventAttribute& value) {
  if (value.is_null) return scalar_grouping_key(value);
  if (value.container == AttributeContainerKind::kList)
    throw std::invalid_argument("unhashable type: 'list'");
  if (value.container == AttributeContainerKind::kContainer)
    throw std::invalid_argument("unhashable type: 'dict'");
  if (value.container == AttributeContainerKind::kSet)
    throw std::invalid_argument("unhashable type: 'set'");
  if (value.container == AttributeContainerKind::kTuple ||
      value.container == AttributeContainerKind::kFrozenSet)
    return {value.kind, grouping_component(value)};
  return scalar_grouping_key(value);
}

bool is_case_attribute_field(const EventAttribute& field,
                             const LogConversionOptions& options) {
  if (!options.include_case_attributes) return false;
  // An empty prefix is an explicit request to treat every non-glue field as
  // trace data.  This mirrors the flattening contract instead of relying on
  // string::starts_with("") for accidental matching.
  if (options.case_attribute_prefix.empty()) {
    // Empty prefixes are used by external stream/table callers to request
    // unprefixed trace attributes. Canonical event columns still describe
    // the event and must never be swallowed as trace data.
    if (field.key == kActivityKey || field.key == "activity" ||
        field.key == kTimestampKey || field.key == "timestamp" ||
        field.key == kResourceKey || field.key == "resource" ||
        field.key == kLifecycleKey || field.key == "lifecycle")
      return false;
    return field.key != options.case_id_key;
  }
  return field.key.starts_with(options.case_attribute_prefix);
}

Event record_to_event(const LogConversionRecord& record,
                      std::string case_id,
                      FirstClassAttributeState case_state,
                      const LogConversionOptions& options,
                      std::string_view activity_source_key,
                      std::string_view timestamp_source_key,
                      std::string_view resource_source_key,
                      std::string_view lifecycle_source_key) {
  Event event;
  event.case_id = std::move(case_id);
  event.case_id_state = case_state;
  std::string selected_activity_key;
  std::string selected_timestamp_key;
  std::string selected_resource_key;
  std::string selected_lifecycle_key;
  const auto assign_first_class = [&](const EventAttribute& field,
                                      std::string& target,
                                      FirstClassAttributeState& state,
                                      std::string& source_key,
                                      std::string& selected_key,
                                      std::optional<EventAttribute>& metadata) {
    target = field.value;
    state = field.first_class_role == FirstClassAttributeRole::kNone
                ? (field.is_null ? FirstClassAttributeState::kNull
                                 : FirstClassAttributeState::kPresent)
                : field.first_class_state;
    source_key = field.first_class_source_key.empty()
                     ? field.key
                     : field.first_class_source_key;
    selected_key = field.key;
    metadata = field;
    metadata->key = source_key;
    metadata->first_class_role = FirstClassAttributeRole::kNone;
    metadata->first_class_state = FirstClassAttributeState::kMissing;
    metadata->first_class_source_key.clear();
    if (field.scalar_wrapper || field.scalar_children_list || !field.children.empty()) {
      auto preserved = field;
      preserved.key = source_key;
      preserved.first_class_role = FirstClassAttributeRole::kNone;
      preserved.first_class_state = FirstClassAttributeState::kMissing;
      preserved.first_class_source_key.clear();
      upsert(event.attributes, std::move(preserved));
    }
  };

  for (const auto& field : record.fields) {
    switch (field.first_class_role) {
      case FirstClassAttributeRole::kActivity:
        assign_first_class(field, event.activity, event.activity_state,
                           event.activity_source_key, selected_activity_key,
                           event.activity_attribute);
        break;
      case FirstClassAttributeRole::kTimestamp:
        assign_first_class(field, event.timestamp, event.timestamp_state,
                           event.timestamp_source_key, selected_timestamp_key,
                           event.timestamp_attribute);
        break;
      case FirstClassAttributeRole::kResource:
        assign_first_class(field, event.resource, event.resource_state,
                           event.resource_source_key, selected_resource_key,
                           event.resource_attribute);
        break;
      case FirstClassAttributeRole::kLifecycle:
        assign_first_class(field, event.lifecycle, event.lifecycle_state,
                           event.lifecycle_source_key, selected_lifecycle_key,
                           event.lifecycle_attribute);
        break;
      case FirstClassAttributeRole::kNone:
      case FirstClassAttributeRole::kCaseId: break;
    }
  }

  const auto select_exact = [&](std::string_view key, std::string& target,
                                FirstClassAttributeState& state,
                                std::string& source_key,
                                std::string& selected_key,
                                std::optional<EventAttribute>& metadata) {
    if (key.empty() || !selected_key.empty()) return;
    const auto found = std::find_if(
        record.fields.begin(), record.fields.end(), [&](const auto& field) {
          return field.first_class_role == FirstClassAttributeRole::kNone &&
                 field.key == key;
        });
    if (found != record.fields.end())
      assign_first_class(*found, target, state, source_key, selected_key,
                         metadata);
  };
  select_exact(activity_source_key, event.activity, event.activity_state,
               event.activity_source_key, selected_activity_key,
               event.activity_attribute);
  select_exact(timestamp_source_key, event.timestamp, event.timestamp_state,
               event.timestamp_source_key, selected_timestamp_key,
               event.timestamp_attribute);
  select_exact(resource_source_key, event.resource, event.resource_state,
               event.resource_source_key, selected_resource_key,
               event.resource_attribute);
  select_exact(lifecycle_source_key, event.lifecycle, event.lifecycle_state,
               event.lifecycle_source_key, selected_lifecycle_key,
               event.lifecycle_attribute);

  for (const auto& field : record.fields) {
    if (field.first_class_role != FirstClassAttributeRole::kNone) continue;
    if (field.key == options.case_id_key ||
        is_case_attribute_field(field, options))
      continue;
    if (field.key == selected_activity_key ||
        field.key == selected_timestamp_key ||
        field.key == selected_resource_key ||
        field.key == selected_lifecycle_key)
      continue;
    if ((field.key == kActivityKey || field.key == "activity") &&
        selected_activity_key.empty()) {
      assign_first_class(field, event.activity, event.activity_state,
                         event.activity_source_key, selected_activity_key,
                         event.activity_attribute);
      continue;
    }
    if ((field.key == kTimestampKey || field.key == "timestamp") &&
        selected_timestamp_key.empty()) {
      assign_first_class(field, event.timestamp, event.timestamp_state,
                         event.timestamp_source_key, selected_timestamp_key,
                         event.timestamp_attribute);
      continue;
    }
    if ((field.key == kResourceKey || field.key == "resource") &&
        selected_resource_key.empty()) {
      assign_first_class(field, event.resource, event.resource_state,
                         event.resource_source_key, selected_resource_key,
                         event.resource_attribute);
      continue;
    }
    if ((field.key == kLifecycleKey || field.key == "lifecycle") &&
        selected_lifecycle_key.empty()) {
      assign_first_class(field, event.lifecycle, event.lifecycle_state,
                         event.lifecycle_source_key, selected_lifecycle_key,
                         event.lifecycle_attribute);
      continue;
    }
    upsert(event.attributes, field);
  }
  return event;
}

EventLog variant_projection(const EventLog& source,
                            const LogConversionOptions& options,
                            std::vector<LogVariantProjection>& metadata) {
  struct VariantCount {
    std::vector<std::string> activities;
    std::size_t count = 0;
  };
  std::map<std::vector<std::string>, std::size_t> counts;
  for (const auto& trace : source.traces) {
    std::vector<std::string> activities;
    activities.reserve(trace.events.size());
    for (const auto& event : trace.events) activities.push_back(event.activity);
    ++counts[std::move(activities)];
  }
  std::vector<VariantCount> variants;
  variants.reserve(counts.size());
  for (auto& [activities, count] : counts)
    variants.push_back({activities, count});
  std::sort(variants.begin(), variants.end(), [](const auto& left,
                                                 const auto& right) {
    if (left.count != right.count) return left.count > right.count;
    return left.activities > right.activities;
  });
  if (options.max_variants_to_return && *options.max_variants_to_return > 0 &&
      variants.size() > *options.max_variants_to_return)
    variants.resize(*options.max_variants_to_return);

  EventLog result;
  const bool all_copies =
      options.variant == LogConversionVariant::kAllTracesPerVariant;
  for (const auto& variant : variants) {
    LogVariantProjection projected;
    projected.activities = variant.activities;
    projected.count = variant.count;
    const auto copies = all_copies ? variant.count : std::size_t{1};
    for (std::size_t copy = 0; copy < copies; ++copy) {
      Trace trace;
      for (std::size_t index = 0; index < variant.activities.size(); ++index) {
        Event event;
        event.activity = variant.activities[index];
        event.input_order = index;
        trace.events.push_back(std::move(event));
      }
      projected.trace_indexes.push_back(result.traces.size());
      result.traces.push_back(std::move(trace));
    }
    if (all_copies && options.return_variants)
      metadata.push_back(std::move(projected));
  }
  return result;
}

std::string json_escape(std::string_view value) {
  std::string escaped;
  for (const unsigned char character : value) {
    switch (character) {
      case '"': escaped += "\\\""; break;
      case '\\': escaped += "\\\\"; break;
      case '\b': escaped += "\\b"; break;
      case '\f': escaped += "\\f"; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default:
        if (character < 0x20) {
          static constexpr char hex[] = "0123456789abcdef";
          escaped += "\\u00";
          escaped.push_back(hex[character >> 4]);
          escaped.push_back(hex[character & 0x0f]);
        } else {
          escaped.push_back(static_cast<char>(character));
        }
    }
  }
  return escaped;
}

std::string_view kind_name(AttributeKind kind) {
  switch (kind) {
    case AttributeKind::kString: return "string";
    case AttributeKind::kDate: return "date";
    case AttributeKind::kInt: return "int";
    case AttributeKind::kFloat: return "float";
    case AttributeKind::kBoolean: return "boolean";
    case AttributeKind::kId: return "id";
    case AttributeKind::kBytes: return "bytes";
  }
  throw std::invalid_argument("log conversion: invalid attribute kind");
}

std::string_view container_name(AttributeContainerKind kind) {
  switch (kind) {
    case AttributeContainerKind::kScalar: return "scalar";
    case AttributeContainerKind::kList: return "list";
    case AttributeContainerKind::kContainer: return "container";
    case AttributeContainerKind::kTuple: return "tuple";
    case AttributeContainerKind::kSet: return "set";
    case AttributeContainerKind::kFrozenSet: return "frozenset";
  }
  throw std::invalid_argument("log conversion: invalid attribute container kind");
}

std::string_view first_class_state_name(FirstClassAttributeState state) {
  switch (state) {
    case FirstClassAttributeState::kMissing: return "missing";
    case FirstClassAttributeState::kPresent: return "present";
    case FirstClassAttributeState::kNull: return "null";
  }
  throw std::invalid_argument("log conversion: invalid first-class state");
}

std::string_view first_class_role_name(FirstClassAttributeRole role) {
  switch (role) {
    case FirstClassAttributeRole::kNone: return "none";
    case FirstClassAttributeRole::kCaseId: return "case_id";
    case FirstClassAttributeRole::kActivity: return "activity";
    case FirstClassAttributeRole::kTimestamp: return "timestamp";
    case FirstClassAttributeRole::kResource: return "resource";
    case FirstClassAttributeRole::kLifecycle: return "lifecycle";
  }
  throw std::invalid_argument("log conversion: invalid first-class role");
}

void write_attributes(std::ostream& output,
                      const std::vector<EventAttribute>& attributes);
void write_attribute_details(std::ostream& output,
                             const std::vector<EventAttribute>& attributes);
void write_attribute(std::ostream& output, const EventAttribute& attribute);

void write_date_metadata(std::ostream& output,
                         const EventAttribute& attribute) {
  if (attribute.is_null || attribute.kind != AttributeKind::kDate) return;
  const auto* date = std::get_if<NativeDate>(&attribute.typed_value);
  if (!date) return;
  if (date->resolution == NativeDateResolution::kNanosecond)
    output << ",\"date_resolution\":\"ns\"";
  if (date->timezone && !date->timezone->empty())
    output << ",\"date_timezone\":\""
           << json_escape(*date->timezone) << '"';
}

// Table-only metadata is deliberately additive to the long-standing table
// JSON shape. A missing dtype vector means that the source did not declare
// dtypes; an explicit index is emitted only when the source supplied one.
void write_event_table_index_label(std::ostream& output,
                                   const EventTableIndexLabel& label) {
  output << "{\"kind\":";
  if (const auto* scalar = std::get_if<EventAttribute>(&label.value)) {
    output << "\"scalar\",\"value\":";
    write_attribute(output, *scalar);
  } else if (const auto* tuple =
                 std::get_if<std::vector<EventTableIndexLabel>>(&label.value)) {
    output << "\"tuple\",\"items\":[";
    for (std::size_t index = 0; index < tuple->size(); ++index) {
      if (index != 0) output << ',';
      write_event_table_index_label(output, (*tuple)[index]);
    }
    output << ']';
  } else {
    throw std::invalid_argument("log conversion: invalid table index label");
  }
  output << '}';
}

void write_event_table_metadata(std::ostream& output,
                                const EventTable& table) {
  if (!table.dtypes.empty()) {
    output << ",\"dtypes\":[";
    for (std::size_t index = 0; index < table.dtypes.size(); ++index) {
      if (index != 0) output << ',';
      output << '"' << json_escape(table.dtypes[index]) << '"';
    }
    output << ']';
  }
  if (table.index) {
    output << ",\"index\":{\"class\":\""
           << json_escape(table.index->kind) << "\",\"dtype\":\""
           << json_escape(table.index->dtype) << "\",\"names\":[";
    for (std::size_t index = 0; index < table.index->names.size(); ++index) {
      if (index != 0) output << ',';
      if (table.index->names[index])
        output << '"' << json_escape(*table.index->names[index]) << '"';
      else
        output << "null";
    }
    output << "],\"labels\":[";
    for (std::size_t index = 0; index < table.index->labels.size(); ++index) {
      if (index != 0) output << ',';
      write_event_table_index_label(output, table.index->labels[index]);
    }
    output << "]}";
  }
  if (!table.categorical_dtypes.empty()) {
    output << ",\"categorical_dtypes\":[";
    for (std::size_t column = 0; column < table.categorical_dtypes.size();
         ++column) {
      if (column != 0) output << ',';
      const auto& descriptor = table.categorical_dtypes[column];
      if (!descriptor) {
        output << "null";
        continue;
      }
      output << "{\"categories_dtype\":\""
             << json_escape(descriptor->categories_dtype)
             << "\",\"ordered\":"
             << (descriptor->ordered ? "true" : "false")
             << ",\"categories\":[";
      for (std::size_t category = 0;
           category < descriptor->categories.size(); ++category) {
        if (category != 0) output << ',';
        write_attribute(output, descriptor->categories[category]);
      }
      output << "]}";
    }
    output << ']';
  }
}

void write_attribute(std::ostream& output, const EventAttribute& attribute) {
  output << "{\"key\":\"" << json_escape(attribute.key)
         << "\",\"value\":\"" << json_escape(attribute.value)
         << "\",\"kind\":\"" << kind_name(attribute.kind)
         << "\",\"container\":\"" << container_name(attribute.container)
         << "\"";
  if (attribute.is_null) output << ",\"null\":true";
  if (!std::holds_alternative<std::monostate>(attribute.typed_value)) {
    output << ",\"typed_value\":";
    if (const auto* integer_value =
            std::get_if<std::int64_t>(&attribute.typed_value))
      output << *integer_value;
    else if (const auto* unsigned_value =
                 std::get_if<std::uint64_t>(&attribute.typed_value))
      output << *unsigned_value;
    else if (const auto* float_value =
                 std::get_if<double>(&attribute.typed_value)) {
      if (std::isfinite(*float_value))
        output << std::setprecision(17) << *float_value;
      else if (std::isnan(*float_value))
        output << "\"NaN\"";
      else
        output << '"' << (*float_value < 0 ? "-INF" : "INF") << '"';
    } else if (const auto* boolean_value =
                   std::get_if<bool>(&attribute.typed_value))
      output << (*boolean_value ? "true" : "false");
    else if (const auto* date_value =
                  std::get_if<NativeDate>(&attribute.typed_value))
      output << '"' << json_escape(date_value->value) << '"';
  }
  write_date_metadata(output, attribute);
  if (attribute.scalar_wrapper) output << ",\"scalar_wrapper\":true";
  if (attribute.scalar_children_list) output << ",\"scalar_children_list\":true";
  if (!attribute.children.empty()) {
    output << ",\"children\":";
    write_attributes(output, attribute.children);
  }
  if (attribute.first_class_role != FirstClassAttributeRole::kNone) {
    output << ",\"first_class_role\":\""
           << first_class_role_name(attribute.first_class_role)
           << "\",\"first_class_state\":\""
           << first_class_state_name(attribute.first_class_state)
           << "\",\"first_class_source_key\":\""
           << json_escape(attribute.first_class_source_key) << '"';
  }
  output << "}";
}

void write_attributes(std::ostream& output,
                      const std::vector<EventAttribute>& attributes) {
  output << '[';
  for (std::size_t i = 0; i < attributes.size(); ++i) {
    if (i != 0) output << ',';
    write_attribute(output, attributes[i]);
  }
  output << ']';
}

// Selected first-class fields retain their source attribute separately from
// the convenient string projection. Emit that attribute as a complete,
// recursive detail object so typed values and nested children survive every
// EventLog-shaped JSON boundary.
void write_attribute_detail(std::ostream& output,
                            const EventAttribute& attribute) {
  output << "{\"key\":\"" << json_escape(attribute.key)
         << "\",\"value\":\"" << json_escape(attribute.value)
         << "\",\"kind\":\"" << kind_name(attribute.kind)
         << "\",\"container\":\"" << container_name(attribute.container)
         << "\",\"null\":" << (attribute.is_null ? "true" : "false");
  if (!std::holds_alternative<std::monostate>(attribute.typed_value)) {
    output << ",\"typed_value\":";
    if (const auto* integer_value =
            std::get_if<std::int64_t>(&attribute.typed_value))
      output << *integer_value;
    else if (const auto* unsigned_value =
                 std::get_if<std::uint64_t>(&attribute.typed_value))
      output << *unsigned_value;
    else if (const auto* float_value =
                 std::get_if<double>(&attribute.typed_value)) {
      if (std::isfinite(*float_value))
        output << std::setprecision(17) << *float_value;
      else if (std::isnan(*float_value))
        output << "\"NaN\"";
      else
        output << '"' << (*float_value < 0 ? "-INF" : "INF") << '"';
    } else if (const auto* boolean_value =
                   std::get_if<bool>(&attribute.typed_value))
      output << (*boolean_value ? "true" : "false");
    else if (const auto* date_value =
                 std::get_if<NativeDate>(&attribute.typed_value))
      output << '"' << json_escape(date_value->value) << '"';
  }
  write_date_metadata(output, attribute);
  if (attribute.scalar_wrapper) output << ",\"scalar_wrapper\":true";
  if (attribute.scalar_children_list) output << ",\"scalar_children_list\":true";
  if (!attribute.children.empty()) {
    output << ",\"children\":";
    write_attribute_details(output, attribute.children);
  }
  output << '}';
}

void write_attribute_details(
    std::ostream& output,
    const std::vector<EventAttribute>& attributes) {
  output << '[';
  for (std::size_t i = 0; i < attributes.size(); ++i) {
    if (i != 0) output << ',';
    write_attribute_detail(output, attributes[i]);
  }
  output << ']';
}

void write_optional_attribute_detail(
    std::ostream& output, std::string_view field,
    const std::optional<EventAttribute>& attribute) {
  if (!attribute) return;
  output << ",\"" << field << "\":";
  write_attribute_detail(output, *attribute);
}

void write_metadata(std::ostream& output, const EventDataMetadata& metadata) {
  output << "{\"extensions\":[";
  for (std::size_t i = 0; i < metadata.extensions.size(); ++i) {
    if (i != 0) output << ',';
    const auto& extension = metadata.extensions[i];
    output << "{\"name\":\"" << json_escape(extension.name)
           << "\",\"prefix\":\"" << json_escape(extension.prefix)
           << "\",\"uri\":\"" << json_escape(extension.uri) << "\"}";
  }
  output << "],\"classifiers\":[";
  for (std::size_t i = 0; i < metadata.classifiers.size(); ++i) {
    if (i != 0) output << ',';
    const auto& classifier = metadata.classifiers[i];
    output << "{\"name\":\"" << json_escape(classifier.name)
           << "\",\"keys\":[";
    for (std::size_t j = 0; j < classifier.keys.size(); ++j) {
      if (j != 0) output << ',';
      output << '"' << json_escape(classifier.keys[j]) << '"';
    }
    output << "]}";
  }
  output << "],\"trace_globals\":";
  write_attributes(output, metadata.trace_globals);
  output << ",\"event_globals\":";
  write_attributes(output, metadata.event_globals);
  output << ",\"properties\":";
  write_attributes(output, metadata.properties);
  output << ",\"globals\":[";
  for (std::size_t i = 0; i < metadata.globals.size(); ++i) {
    if (i) output << ',';
    output << "{\"scope\":\"" << json_escape(metadata.globals[i].scope)
           << "\",\"attributes\":";
    write_attributes(output, metadata.globals[i].attributes);
    output << '}';
  }
  output << ']';
  output << '}';
}

void write_event_log(std::ostream& output, const EventLog& log) {
  output << "{\"attributes\":";
  write_attributes(output, log.attributes);
  output << ",\"traces\":[";
  for (std::size_t trace_index = 0; trace_index < log.traces.size();
       ++trace_index) {
    if (trace_index != 0) output << ',';
    const auto& trace = log.traces[trace_index];
    output << "{\"case_id\":\"" << json_escape(trace.case_id)
           << "\",\"case_id_state\":\""
           << first_class_state_name(effective_first_class_state(
                  trace.case_id_state, trace.case_id))
           << "\",\"case_id_source_key\":\""
           << json_escape(trace.case_id_source_key)
           << '"';
    write_optional_attribute_detail(output, "case_id_attribute",
                                    trace.case_id_attribute);
    output << ",\"attributes\":";
    write_attributes(output, trace.attributes);
    output << ",\"properties\":";
    write_attributes(output, trace.properties);
    output << ",\"events\":[";
    for (std::size_t event_index = 0; event_index < trace.events.size();
         ++event_index) {
      if (event_index != 0) output << ',';
      const auto& event = trace.events[event_index];
      output << "{\"case_id\":\"" << json_escape(event.case_id)
             << "\",\"case_id_state\":\""
             << first_class_state_name(effective_first_class_state(
                    event.case_id_state, event.case_id))
             << "\",\"case_id_source_key\":\""
             << json_escape(event.case_id_source_key)
             << "\",\"activity\":\"" << json_escape(event.activity)
             << "\",\"activity_state\":\""
             << first_class_state_name(effective_first_class_state(
                    event.activity_state, event.activity))
              << "\",\"activity_source_key\":\""
              << json_escape(event.activity_source_key)
              << '"';
       write_optional_attribute_detail(output, "activity_attribute",
                                       event.activity_attribute);
       output << ",\"timestamp\":\"" << json_escape(event.timestamp)
             << "\",\"timestamp_state\":\""
             << first_class_state_name(effective_first_class_state(
                    event.timestamp_state, event.timestamp))
              << "\",\"timestamp_source_key\":\""
              << json_escape(event.timestamp_source_key)
              << '"';
       write_optional_attribute_detail(output, "timestamp_attribute",
                                       event.timestamp_attribute);
       output << ",\"resource\":\"" << json_escape(event.resource)
             << "\",\"resource_state\":\""
             << first_class_state_name(effective_first_class_state(
                    event.resource_state, event.resource))
              << "\",\"resource_source_key\":\""
              << json_escape(event.resource_source_key)
              << '"';
       write_optional_attribute_detail(output, "resource_attribute",
                                       event.resource_attribute);
       output << ",\"lifecycle\":\"" << json_escape(event.lifecycle)
             << "\",\"lifecycle_state\":\""
             << first_class_state_name(effective_first_class_state(
                    event.lifecycle_state, event.lifecycle))
              << "\",\"lifecycle_source_key\":\""
              << json_escape(event.lifecycle_source_key)
              << '"';
       write_optional_attribute_detail(output, "lifecycle_attribute",
                                       event.lifecycle_attribute);
       output << ",\"input_order\":" << event.input_order
             << ",\"attributes\":";
      write_attributes(output, event.attributes);
      output << '}';
    }
    output << "]}";
  }
  output << "],\"metadata\":";
  write_metadata(output, log.metadata);
  output << "}";
}

std::string_view node_type_name(LogGraphNodeType type) {
  switch (type) {
    case LogGraphNodeType::kCase: return "CASE";
    case LogGraphNodeType::kEvent: return "EVENT";
    case LogGraphNodeType::kAttribute: return "ATTRIBUTE_NODE";
  }
  throw std::invalid_argument("log conversion: invalid graph node type");
}

std::string_view edge_type_name(LogGraphEdgeType type) {
  switch (type) {
    case LogGraphEdgeType::kBelongsTo: return "BELONGS_TO";
    case LogGraphEdgeType::kDirectlyFollows: return "DF";
    case LogGraphEdgeType::kAttribute: return "ATTRIBUTE_EDGE";
  }
  throw std::invalid_argument("log conversion: invalid graph edge type");
}

}  // namespace

LogConversionVariant parse_log_conversion_variant(std::string_view value) {
  std::string normalized(value);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char character) {
                   if (character == '-') return '_';
                   return static_cast<char>(std::tolower(character));
                 });
  if (normalized == "to_event_log" || normalized == "event_log")
    return LogConversionVariant::kToEventLog;
  if (normalized == "to_event_stream" || normalized == "event_stream")
    return LogConversionVariant::kToEventStream;
  if (normalized == "to_data_frame" || normalized == "to_dataframe" ||
      normalized == "dataframe")
    return LogConversionVariant::kToDataFrame;
  if (normalized == "to_nx" || normalized == "to_graph" ||
      normalized == "graph")
    return LogConversionVariant::kToAttributedGraph;
  if (normalized == "df_to_event_log_1v" || normalized == "one_per_variant")
    return LogConversionVariant::kOneTracePerVariant;
  if (normalized == "df_to_event_log_nv" || normalized == "all_per_variant")
    return LogConversionVariant::kAllTracesPerVariant;
  throw std::invalid_argument(
      "log conversion variant: unsupported selector");
}

std::string_view log_conversion_variant_name(LogConversionVariant variant) {
  switch (variant) {
    case LogConversionVariant::kToEventLog: return "TO_EVENT_LOG";
    case LogConversionVariant::kToEventStream: return "TO_EVENT_STREAM";
    case LogConversionVariant::kToDataFrame: return "TO_DATA_FRAME";
    case LogConversionVariant::kToAttributedGraph: return "TO_NX";
    case LogConversionVariant::kOneTracePerVariant:
      return "DF_TO_EVENT_LOG_1V";
    case LogConversionVariant::kAllTracesPerVariant:
      return "DF_TO_EVENT_LOG_NV";
  }
  throw std::invalid_argument("log conversion: invalid variant");
}

ConvertedEventStream event_log_to_event_stream(
    const EventLog& log, const LogConversionOptions& options) {
  validate_options(options);
  ConvertedEventStream stream;
  stream.attributes = log.attributes;
  stream.metadata = log.metadata;
  stream.events.reserve(log.event_count());
  for (std::size_t trace_index = 0; trace_index < log.traces.size();
       ++trace_index) {
    const auto& trace = log.traces[trace_index];
    const auto effective_case_state =
        trace.case_id_state == FirstClassAttributeState::kMissing &&
                !trace.case_id.empty()
            ? FirstClassAttributeState::kPresent
            : trace.case_id_state;
    for (const auto& event : trace.events) {
      LogConversionRecord record;
      record.fields = event_fields(event);
      const auto case_source_key =
          trace.case_id_source_key.empty()
              ? std::string(kActivityKey)
              : trace.case_id_source_key;
      std::optional<EventAttribute> case_projection;
      if (options.include_case_attributes) {
        for (const auto& attribute : trace_fields(trace)) {
          auto injected = attribute;
          // The configured glue key is authoritative for the case id.  Other
          // trace attributes use the configured prefix.
          if (injected.first_class_role == FirstClassAttributeRole::kCaseId) {
            injected.key = options.case_id_key;
            case_projection = std::move(injected);
          } else {
            injected.key = options.case_attribute_prefix + injected.key;
            upsert(record.fields, std::move(injected));
          }
        }
        if (!case_projection) {
          // A flat representation needs a grouping value even when the
          // original trace had no case id.  Use a per-trace surrogate, mark it
          // as missing, and restore the absent state on the reverse adapter.
          const auto glue_value =
              effective_case_state == FirstClassAttributeState::kMissing ||
                      effective_case_state == FirstClassAttributeState::kNull
                  ? std::to_string(trace_index)
                  : trace.case_id;
          case_projection = EventAttribute{
              options.case_id_key, glue_value, AttributeKind::kString,
              effective_case_state == FirstClassAttributeState::kNull ||
                  effective_case_state == FirstClassAttributeState::kMissing};
        }
      } else {
        case_projection = EventAttribute{
            options.case_id_key, std::to_string(trace_index),
            AttributeKind::kString,
            effective_case_state != FirstClassAttributeState::kPresent};
      }
      case_projection->first_class_role = FirstClassAttributeRole::kCaseId;
      case_projection->first_class_state = effective_case_state;
      case_projection->first_class_source_key = case_source_key;
      // Insert the grouping cell last so a prefixed trace alias can never
      // overwrite the authoritative case glue field.
      upsert(record.fields, std::move(*case_projection));
      if (options.remove_null_fields) {
        record.fields.erase(
            std::remove_if(record.fields.begin(), record.fields.end(),
                           [&](const auto& field) {
                              return field.is_null &&
                                     field.first_class_role !=
                                         FirstClassAttributeRole::kCaseId;
                            }),
            record.fields.end());
      }
      stream.events.push_back(std::move(record));
    }
  }
  return stream;
}

EventLog event_stream_to_event_log(const ConvertedEventStream& stream,
                                   const LogConversionOptions& options) {
  validate_options(options);
  EventLog log;
  log.attributes = stream.attributes;
  log.metadata = stream.metadata;
  const auto role_field = [](const LogConversionRecord& record,
                             FirstClassAttributeRole role) {
    const auto found = std::find_if(
        record.fields.begin(), record.fields.end(), [&](const auto& field) {
          return field.first_class_role == role;
        });
    return found == record.fields.end() ? nullptr : &*found;
  };
  std::map<std::tuple<AttributeKind, std::string, bool, int,
                      std::string>,
           std::size_t>
      trace_indexes;
  for (const auto& record : stream.events) {
    const auto* glue = role_field(record, FirstClassAttributeRole::kCaseId);
    if (!glue) glue = record.field(options.case_id_key);
    if (!glue)
      throw std::invalid_argument(
          "log conversion: event stream record has no case id field");
    const auto case_state =
        glue->first_class_role == FirstClassAttributeRole::kCaseId
            ? glue->first_class_state
            : (glue->is_null ? FirstClassAttributeState::kNull
                             : FirstClassAttributeState::kPresent);
    const auto row_case_source_key =
        glue->first_class_source_key.empty()
            ? (glue->key == options.case_id_key &&
                       glue->key == "case:concept:name"
                   ? std::string(kActivityKey)
                   : glue->key)
            : glue->first_class_source_key;
    const auto scalar_key = case_grouping_scalar(*glue);
    auto [found, inserted] = trace_indexes.emplace(
        std::tuple{scalar_key.first, scalar_key.second, glue->is_null,
                   static_cast<int>(case_state),
                   row_case_source_key},
        log.traces.size());
    if (inserted) {
      Trace trace;
      if (case_state == FirstClassAttributeState::kPresent) {
        if (const auto* date = std::get_if<NativeDate>(&glue->typed_value))
          trace.case_id = date->value;
        else
          trace.case_id = glue->value;
      }
      trace.case_id_state = case_state;
      trace.case_id_source_key = row_case_source_key;
      auto selected_case = *glue;
      if (const auto* date = std::get_if<NativeDate>(&selected_case.typed_value))
        selected_case.value = date->value;
      selected_case.key = row_case_source_key;
      selected_case.first_class_role = FirstClassAttributeRole::kNone;
      selected_case.first_class_state = FirstClassAttributeState::kMissing;
      selected_case.first_class_source_key.clear();
      trace.case_id_attribute = std::move(selected_case);
      if (glue->scalar_wrapper || glue->scalar_children_list || !glue->children.empty()) {
        auto selected_attribute = *glue;
        selected_attribute.key = row_case_source_key;
        selected_attribute.first_class_role = FirstClassAttributeRole::kNone;
        selected_attribute.first_class_state =
            FirstClassAttributeState::kMissing;
        selected_attribute.first_class_source_key.clear();
        upsert(trace.attributes, std::move(selected_attribute));
      }
      if (options.include_case_attributes) {
        for (const auto& field : record.fields) {
          if (field.first_class_role != FirstClassAttributeRole::kNone)
            continue;
          if (!is_case_attribute_field(field, options)) continue;
          auto attribute = field;
          if (!options.case_attribute_prefix.empty())
            attribute.key.erase(0, options.case_attribute_prefix.size());
          if (attribute.key == kActivityKey &&
              !options.case_attribute_prefix.empty()) {
            if (!attribute.is_null) {
              if (const auto* date =
                      std::get_if<NativeDate>(&attribute.typed_value))
                trace.case_id = date->value;
              else
                trace.case_id = attribute.value;
            }
            // A null canonical case label is an absent label, not an empty
            // replacement for the non-null glue value.
            continue;
          }
          upsert(trace.attributes, std::move(attribute));
        }
      }
      log.traces.push_back(std::move(trace));
    }
    auto& trace = log.traces[found->second];
    auto event = record_to_event(record, trace.case_id, trace.case_id_state,
                                 options, kActivityKey, kTimestampKey,
                                 kResourceKey, kLifecycleKey);
    event.case_id = trace.case_id;
    event.case_id_state = trace.case_id_state;
    event.case_id_source_key = trace.case_id_source_key;
    event.input_order = trace.events.size();
    trace.events.push_back(std::move(event));
  }
  return log;
}

LogConversionTable event_stream_to_table(const ConvertedEventStream& stream) {
  return event_stream_to_table(stream, LogConversionOptions{});
}

LogConversionTable event_stream_to_table(
    const ConvertedEventStream& stream, const LogConversionOptions& options) {
  validate_options(options);
  LogConversionTable table;
  table.attributes = stream.attributes;
  table.metadata = stream.metadata;
  std::map<std::string, std::size_t> column_indexes;
  for (const auto& record : stream.events) {
    for (const auto& field : record.fields) {
      if (options.remove_null_fields && field.is_null &&
          field.key != options.case_id_key)
        continue;
      if (!column_indexes.contains(field.key)) {
        column_indexes.emplace(field.key, table.columns.size());
        table.columns.push_back(field.key);
      }
    }
  }
  table.rows.reserve(stream.events.size());
  for (const auto& record : stream.events) {
    std::vector<std::optional<EventAttribute>> row(table.columns.size());
    for (const auto& field : record.fields) {
      if (!options.remove_null_fields || !field.is_null ||
          field.key == options.case_id_key)
        row[column_indexes.at(field.key)] = field;
    }
    table.rows.push_back(std::move(row));
  }
  table.validate();
  return table;
}

ConvertedEventStream event_table_to_event_stream(
    const LogConversionTable& table, const LogConversionOptions& options) {
  validate_options(options);
  table.validate();
  ConvertedEventStream stream;
  stream.attributes = table.attributes;
  // Dataframes have attrs mapped to metadata.properties, not log attributes.
  // Preserve native log attributes when present; otherwise declare the same
  // tabular origin as the public dataframe conversion.
  if (stream.attributes.empty()) stream.attributes.push_back({"origin", "csv"});
  stream.metadata = table.metadata;
  std::erase_if(stream.metadata.properties, [](const auto& property) {
    return property.key == "pm4py:param:case_id_key";
  });
  infer_standard_extensions(table, stream.metadata);
  stream.events.reserve(table.rows.size());
  for (const auto& row : table.rows) {
    LogConversionRecord record;
    record.fields.reserve(table.columns.size());
    for (std::size_t index = 0; index < table.columns.size(); ++index) {
      if (!row[index]) continue;
      if (options.remove_null_fields && row[index]->is_null &&
          table.columns[index] != options.case_id_key)
        continue;
      auto field = *row[index];
      field.key = table.columns[index];
      record.fields.push_back(std::move(field));
    }
    stream.events.push_back(std::move(record));
  }
  return stream;
}

LogConversionTable remove_null_cells(LogConversionTable table,
                                    std::string_view case_id_key = {}) {
  table.validate();
  for (auto& row : table.rows) {
    for (auto& cell : row) {
      if (cell && cell->is_null && cell->key != case_id_key) cell.reset();
    }
  }
  return table;
}

EventLog event_table_to_event_log(const LogConversionTable& table,
                                  const LogConversionOptions& options) {
  return event_stream_to_event_log(event_table_to_event_stream(table, options),
                                   options);
}

LogConversionTable event_log_to_table(const EventLog& log,
                                      const LogConversionOptions& options) {
  validate_options(options);
  enforce_event_log_row_cap(log, options);
  return event_stream_to_table(event_log_to_event_stream(log, options),
                               options);
}

LazyEventTable event_log_to_lazy_table(const EventLog& log,
                                       const LogConversionOptions& options) {
  return LazyEventTable::from_table(event_log_to_table(log, options));
}

LogConversionTable lazy_event_table_to_table(
    const LazyEventTable& table, const LogConversionOptions& options) {
  validate_options(options);
  auto materialized = table.materialize(options.max_rows_to_materialize);
  if (!options.remove_null_fields) return materialized;
  return remove_null_cells(std::move(materialized), options.case_id_key);
}

ConvertedEventStream lazy_event_table_to_event_stream(
    const LazyEventTable& table, const LogConversionOptions& options) {
  return event_table_to_event_stream(lazy_event_table_to_table(table, options),
                                     options);
}

EventLog lazy_event_table_to_event_log(const LazyEventTable& table,
                                       const LogConversionOptions& options) {
  return event_stream_to_event_log(
      lazy_event_table_to_event_stream(table, options), options);
}

ConvertedEventStream event_data_to_event_stream(
    const EventDataView& data, const LogConversionOptions& options) {
  switch (data.kind()) {
    case EventDataKind::kEventLog:
      return event_log_to_event_stream(*data.log(), options);
    case EventDataKind::kEventStream: {
      auto stream = *data.stream();
      if (options.remove_null_fields) {
        for (auto& record : stream.events)
          record.fields.erase(
              std::remove_if(record.fields.begin(), record.fields.end(),
                             [&](const auto& field) {
                               return field.is_null &&
                                      field.key != options.case_id_key;
                             }),
              record.fields.end());
      }
      return stream;
    }
    case EventDataKind::kEventTable:
      return event_table_to_event_stream(*data.table(), options);
    case EventDataKind::kLazyEventTable:
      return lazy_event_table_to_event_stream(*data.lazy_table(), options);
  }
  throw std::invalid_argument("event data: invalid input kind");
}

LogConversionTable event_data_to_table(const EventDataView& data,
                                       const LogConversionOptions& options) {
  switch (data.kind()) {
    case EventDataKind::kEventLog:
      return event_log_to_table(*data.log(), options);
    case EventDataKind::kEventStream:
      return event_stream_to_table(*data.stream(), options);
    case EventDataKind::kEventTable:
      data.table()->validate();
      if (!options.remove_null_fields) return *data.table();
      return remove_null_cells(*data.table(), options.case_id_key);
    case EventDataKind::kLazyEventTable:
      return lazy_event_table_to_table(*data.lazy_table(), options);
  }
  throw std::invalid_argument("event data: invalid input kind");
}

EventLog event_data_to_event_log(const EventDataView& data,
                                 const LogConversionOptions& options) {
  switch (data.kind()) {
    case EventDataKind::kEventLog:
      return *data.log();
    case EventDataKind::kEventStream:
      return event_stream_to_event_log(*data.stream(), options);
    case EventDataKind::kEventTable:
      return event_table_to_event_log(*data.table(), options);
    case EventDataKind::kLazyEventTable:
      return lazy_event_table_to_event_log(*data.lazy_table(), options);
  }
  throw std::invalid_argument("event data: invalid input kind");
}

LazyEventTable event_data_to_lazy_table(const EventDataView& data,
                                        const LogConversionOptions& options) {
  switch (data.kind()) {
    case EventDataKind::kEventLog:
      return event_log_to_lazy_table(*data.log(), options);
    case EventDataKind::kEventStream:
      return LazyEventTable::from_table(
          event_stream_to_table(*data.stream(), options));
    case EventDataKind::kEventTable:
      data.table()->validate();
      if (!options.remove_null_fields)
        return LazyEventTable::from_table(*data.table());
      return LazyEventTable::from_table(
          remove_null_cells(*data.table(), options.case_id_key));
    case EventDataKind::kLazyEventTable:
      {
        const auto source = *data.lazy_table();
        if (!options.remove_null_fields &&
            !options.max_rows_to_materialize)
          return source;
        const auto max_rows = options.max_rows_to_materialize;
        return LazyEventTable(
            source.columns(),
            [source, max_rows, remove_null_fields = options.remove_null_fields](
                const LazyEventTable::RowConsumer& consumer) {
              source.scan([&](const LazyEventTable::Row& input) {
                auto row = input;
                if (remove_null_fields) {
                  for (auto& cell : row) {
                    if (cell && cell->is_null) cell.reset();
                  }
                }
                if (consumer) consumer(row);
              }, max_rows);
            },
            source.attributes(), source.metadata(), source.row_count(),
            source.dtypes(), source.index(), source.categorical_dtypes());
      }
  }
  throw std::invalid_argument("event data: invalid input kind");
}

AttributedLogGraph event_log_to_attributed_graph(
    const EventLog& log, const LogConversionOptions& options) {
  validate_options(options);
  AttributedLogGraph graph;
  std::map<std::pair<AttributeKind, std::string>, std::size_t> node_indexes;
  std::map<std::pair<std::size_t, std::size_t>, std::size_t> edge_indexes;
  const auto node = [&](std::string id, LogGraphNodeType type,
                        std::vector<EventAttribute> attributes,
                        AttributeKind identity_kind,
                        bool update_existing) {
    const auto key = std::pair{identity_kind, id};
    const auto found = node_indexes.find(key);
    if (found != node_indexes.end()) {
      if (update_existing) {
        auto& existing = graph.nodes[found->second];
        existing.type = type;
        existing.attributes = std::move(attributes);
      }
      return found->second;
    }
    if (graph.nodes.size() >= options.max_graph_nodes)
      throw std::runtime_error("log conversion: graph node bound exceeded");
    const auto index = graph.nodes.size();
    node_indexes.emplace(key, index);
    graph.nodes.push_back({std::move(id), type, std::move(attributes)});
    return index;
  };
  const auto edge = [&](LogGraphEdge value) {
    const auto key = std::pair{value.source, value.target};
    const auto found = edge_indexes.find(key);
    if (found != edge_indexes.end()) {
      graph.edges[found->second] = std::move(value);
      return;
    }
    if (graph.edges.size() >= options.max_graph_edges)
      throw std::runtime_error("log conversion: graph edge bound exceeded");
    edge_indexes.emplace(key, graph.edges.size());
    graph.edges.push_back(std::move(value));
  };

  for (const auto& trace : log.traces) {
    const auto case_identifier =
        selected_trace_attribute(trace, options.graph_case_id_attribute);
    if (!case_identifier)
      throw std::invalid_argument(
          "log conversion: graph case id attribute is missing");
    const auto case_id = "CASE=" + case_identifier->value;
    const auto case_node = node(case_id, LogGraphNodeType::kCase,
                                trace_fields(trace), AttributeKind::kString,
                                true);
    std::vector<std::size_t> event_nodes;
    event_nodes.reserve(trace.events.size());
    for (std::size_t index = 0; index < trace.events.size(); ++index) {
      const auto& event = trace.events[index];
      const auto event_id =
          "EVENT=" + case_identifier->value + "_" + std::to_string(index);
      const auto event_node = node(event_id, LogGraphNodeType::kEvent,
                                   event_fields(event),
                                   AttributeKind::kString, true);
      event_nodes.push_back(event_node);
      edge({event_node, case_node, LogGraphEdgeType::kBelongsTo, {}});
      for (const auto& key : options.event_attributes_as_nodes) {
        const auto attribute = selected_event_attribute(event, key);
        if (!attribute) continue;
        const auto attribute_node = node(
            attribute->value, LogGraphNodeType::kAttribute, {*attribute},
            attribute->kind, false);
        edge({event_node, attribute_node, LogGraphEdgeType::kAttribute, key});
      }
    }
    if (options.include_directly_follows) {
      for (std::size_t index = 1; index < event_nodes.size(); ++index)
        edge({event_nodes[index - 1], event_nodes[index],
              LogGraphEdgeType::kDirectlyFollows, {}});
    }
    for (const auto& key : options.case_attributes_as_nodes) {
      const auto attribute = selected_trace_attribute(trace, key);
      if (!attribute) continue;
      const auto attribute_node = node(
          attribute->value, LogGraphNodeType::kAttribute, {*attribute},
          attribute->kind, false);
      edge({case_node, attribute_node, LogGraphEdgeType::kAttribute, key});
    }
  }
  return graph;
}

LogConversionResult convert_event_log(const EventLog& log,
                                      const LogConversionOptions& options) {
  validate_options(options);
  (void)log_conversion_variant_name(options.variant);
  LogConversionResult result;
  result.variant = options.variant;
  switch (options.variant) {
    case LogConversionVariant::kToEventLog:
      result.log = log;
      break;
    case LogConversionVariant::kToEventStream:
      result.stream = event_log_to_event_stream(log, options);
      break;
    case LogConversionVariant::kToDataFrame: {
      result.table = event_log_to_table(log, options);
      if (options.lazy_output)
        result.lazy_table = LazyEventTable::from_table(result.table);
      break;
    }
    case LogConversionVariant::kToAttributedGraph:
      result.graph = event_log_to_attributed_graph(log, options);
      break;
    case LogConversionVariant::kOneTracePerVariant:
    case LogConversionVariant::kAllTracesPerVariant:
      result.log = variant_projection(log, options, result.variants);
      break;
  }
  return result;
}

std::string xes_import_result_json(const XesImportResult& result) {
  LogConversionResult converted;
  const bool legacy = result.kind == EventDataKind::kEventLog;
  const bool lazy = result.kind == EventDataKind::kLazyEventTable;
  if (!legacy && !lazy && result.kind != EventDataKind::kEventTable)
    throw std::invalid_argument("Unsupported XES import result kind");
  converted.variant = legacy ? LogConversionVariant::kToEventLog
                             : LogConversionVariant::kToDataFrame;
  if (legacy) converted.log = result.log;
  else if (lazy) {
    converted.lazy_table = result.lazy_table;
    converted.table = result.lazy_table.materialize();
  } else converted.table = result.table;
  if (!legacy) converted.table.validate();

  // Older callers populated only XesImportResult::dtypes, while newer table
  // adapters retain the declaration on EventTable/LazyEventTable as well.
  // Accept either representation, but reject disagreement so a serialized
  // result cannot silently advertise two different schemas.
  std::vector<std::string> dtypes = result.dtypes;
  const auto reconcile_dtypes = [&](const std::vector<std::string>& candidate,
                                    std::string_view source) {
    if (candidate.empty()) return;
    if (!dtypes.empty() && dtypes != candidate)
      throw std::invalid_argument("XES import dtype metadata conflicts with " +
                                  std::string(source));
    dtypes = candidate;
  };
  if (!legacy) {
    reconcile_dtypes(converted.table.dtypes, "table");
    if (lazy) reconcile_dtypes(result.lazy_table.dtypes(), "lazy table");
    if (!dtypes.empty() && dtypes.size() != converted.table.columns.size())
      throw std::invalid_argument("XES import dtype count does not match columns");
    if (converted.table.dtypes.empty() && !dtypes.empty())
      converted.table.dtypes = dtypes;
  }
  std::ostringstream output;
  output << "{\"family\":\"XES_IMPORT\",\"type\":\""
         << (legacy ? "EventLog" : lazy ? "LazyFrame" : "DataFrame")
         << "\",\"dtypes\":[";
  for (std::size_t i = 0; i < dtypes.size(); ++i) {
    if (i) output << ',';
    output << '"' << json_escape(dtypes[i]) << '"';
  }
  output << "],\"result\":" << log_conversion_json(converted) << '}';
  return output.str();
}


std::string serialize_event_table_json(const EventTable& table) {
  std::ostringstream output;
  output << "{\"columns\":[";
  for (std::size_t i = 0; i < table.columns.size(); ++i) {
    if (i != 0) output << ',';
    output << '\"' << json_escape(table.columns[i]) << '\"';
  }
  output << ']';
  write_event_table_metadata(output, table);
  output << ",\"attributes\":";
  write_attributes(output, table.attributes);
  output << ",\"metadata\":";
  write_metadata(output, table.metadata);
  output << ",\"rows\":[";
  for (std::size_t row = 0; row < table.rows.size(); ++row) {
    if (row != 0) output << ',';
    output << '[';
    for (std::size_t column = 0; column < table.rows[row].size();
         ++column) {
      if (column != 0) output << ',';
      if (!table.rows[row][column]) {
        output << "null";
        continue;
      }
      const auto& cell = *table.rows[row][column];
      write_attribute(output, cell);
    }
    output << ']';
  }
  output << ']';
  output << "}";
  return output.str();
}

std::string log_conversion_json(const LogConversionResult& result) {
  const auto variant = log_conversion_variant_name(result.variant);
  std::ostringstream output;
  output << "{\"family\":\"LOG_CONVERSION\",\"variant\":\""
         << variant << '\"';
  if (result.variant == LogConversionVariant::kToEventLog ||
      result.variant == LogConversionVariant::kOneTracePerVariant ||
      result.variant == LogConversionVariant::kAllTracesPerVariant) {
    output << ",\"log\":";
    write_event_log(output, result.log);
  } else if (result.variant == LogConversionVariant::kToEventStream) {
    output << ",\"stream\":{\"attributes\":";
    write_attributes(output, result.stream.attributes);
    output << ",\"metadata\":";
    write_metadata(output, result.stream.metadata);
    output << ",\"events\":[";
    for (std::size_t i = 0; i < result.stream.events.size(); ++i) {
      if (i != 0) output << ',';
      output << "{\"fields\":";
      write_attributes(output, result.stream.events[i].fields);
      output << '}';
    }
    output << "]}";
  } else if (result.variant == LogConversionVariant::kToDataFrame) {
    result.table.validate();
    auto table_json = serialize_event_table_json(result.table);
    if (result.lazy_table) table_json.insert(table_json.size() - 1, ",\"lazy\":true");
    output << ",\"table\":" << table_json;
  } else {
    output << ",\"graph\":{\"nodes\":[";
    for (std::size_t i = 0; i < result.graph.nodes.size(); ++i) {
      if (i != 0) output << ',';
      const auto& node = result.graph.nodes[i];
      output << "{\"id\":\"" << json_escape(node.id)
             << "\",\"type\":\"" << node_type_name(node.type)
             << "\",\"attributes\":";
      write_attributes(output, node.attributes);
      output << '}';
    }
    output << "],\"edges\":[";
    for (std::size_t i = 0; i < result.graph.edges.size(); ++i) {
      if (i != 0) output << ',';
      const auto& edge = result.graph.edges[i];
      output << "{\"source\":" << edge.source << ",\"target\":"
             << edge.target << ",\"type\":\""
             << edge_type_name(edge.type) << '\"';
      if (!edge.attribute_name.empty())
        output << ",\"name\":\"" << json_escape(edge.attribute_name)
               << '\"';
      output << '}';
    }
    output << "]}";
  }
  if (!result.variants.empty()) {
    output << ",\"variants\":[";
    for (std::size_t i = 0; i < result.variants.size(); ++i) {
      if (i != 0) output << ',';
      output << "{\"activities\":[";
      for (std::size_t j = 0; j < result.variants[i].activities.size(); ++j) {
        if (j != 0) output << ',';
        output << '\"' << json_escape(result.variants[i].activities[j])
               << '\"';
      }
      output << "],\"count\":" << result.variants[i].count
             << ",\"trace_indexes\":[";
      for (std::size_t j = 0; j < result.variants[i].trace_indexes.size();
           ++j) {
        if (j != 0) output << ',';
        output << result.variants[i].trace_indexes[j];
      }
      output << "]}";
    }
    output << ']';
  }
  output << '}';
  return output.str();
}

}  // namespace minera
