#include "minera/log_util_json.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace minera {
namespace {

void write_json_string(std::ostream& out, std::string_view value) {
  out << '"';
  for (const unsigned char ch : value) {
    switch (ch) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (ch < 0x20) {
          out << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(ch) << std::dec << std::setfill('0');
        } else {
          out << static_cast<char>(ch);
        }
    }
  }
  out << '"';
}

const char* attribute_kind_name(AttributeKind kind) {
  switch (kind) {
    case AttributeKind::kString: return "string";
    case AttributeKind::kDate: return "date";
    case AttributeKind::kInt: return "int";
    case AttributeKind::kFloat: return "float";
    case AttributeKind::kBoolean: return "boolean";
    case AttributeKind::kId: return "id";
    case AttributeKind::kBytes: return "bytes";
  }
  return "string";
}

const char* attribute_container_name(AttributeContainerKind kind) {
  switch (kind) {
    case AttributeContainerKind::kScalar: return "scalar";
    case AttributeContainerKind::kList: return "list";
    case AttributeContainerKind::kContainer: return "container";
    case AttributeContainerKind::kTuple: return "tuple";
    case AttributeContainerKind::kSet: return "set";
    case AttributeContainerKind::kFrozenSet: return "frozenset";
  }
  return "scalar";
}

const char* first_class_state_name(FirstClassAttributeState state) {
  switch (state) {
    case FirstClassAttributeState::kMissing: return "missing";
    case FirstClassAttributeState::kPresent: return "present";
    case FirstClassAttributeState::kNull: return "null";
  }
  return "missing";
}

FirstClassAttributeState effective_first_class_state(
    FirstClassAttributeState state, std::string_view value) {
  return state == FirstClassAttributeState::kMissing && !value.empty()
             ? FirstClassAttributeState::kPresent
             : state;
}

void write_attributes(std::ostream& out,
                      const std::vector<EventAttribute>& attributes) {
  out << '[';
  for (std::size_t i = 0; i < attributes.size(); ++i) {
    if (i != 0) out << ',';
    const auto& attribute = attributes[i];
    out << "{\"key\":";
    write_json_string(out, attribute.key);
    out << ",\"value\":";
    write_json_string(out, attribute.value);
    out << ",\"kind\":";
    write_json_string(out, attribute_kind_name(attribute.kind));
    out << ",\"container\":";
    write_json_string(out, attribute_container_name(attribute.container));
    if (attribute.is_null) out << ",\"null\":true";
    if (!std::holds_alternative<std::monostate>(attribute.typed_value)) {
      out << ",\"typed_value\":";
      if (const auto* integer_value =
              std::get_if<std::int64_t>(&attribute.typed_value))
        out << *integer_value;
      else if (const auto* unsigned_integer_value =
                   std::get_if<std::uint64_t>(&attribute.typed_value))
        out << *unsigned_integer_value;
      else if (const auto* float_value =
                   std::get_if<double>(&attribute.typed_value)) {
        if (std::isfinite(*float_value))
          out << std::setprecision(17) << *float_value;
        else if (std::isnan(*float_value))
          write_json_string(out, "NaN");
        else
          write_json_string(out, *float_value < 0 ? "-INF" : "INF");
      }
      else if (const auto* boolean_value =
                   std::get_if<bool>(&attribute.typed_value))
        out << (*boolean_value ? "true" : "false");
      else if (const auto* date_value =
                   std::get_if<NativeDate>(&attribute.typed_value))
        write_json_string(out, date_value->value);
    }
    if (attribute.scalar_wrapper) out << ",\"scalar_wrapper\":true";
    if (attribute.scalar_children_list) out << ",\"scalar_children_list\":true";
    if (!attribute.children.empty()) {
      out << ",\"children\":";
      write_attributes(out, attribute.children);
    }
    out << '}';
  }
  out << ']';
}

// Preserve the selected source attribute alongside the convenient string
// field. This detail is recursive and keeps the native type, null state, and
// nested children available to consumers of log-utility results.
void write_attribute_detail(std::ostream& out,
                            const EventAttribute& attribute) {
  out << "{\"key\":";
  write_json_string(out, attribute.key);
  out << ",\"value\":";
  write_json_string(out, attribute.value);
  out << ",\"kind\":";
  write_json_string(out, attribute_kind_name(attribute.kind));
  out << ",\"container\":";
  write_json_string(out, attribute_container_name(attribute.container));
  out << ",\"null\":" << (attribute.is_null ? "true" : "false");
  if (!std::holds_alternative<std::monostate>(attribute.typed_value)) {
    out << ",\"typed_value\":";
    if (const auto* integer_value =
            std::get_if<std::int64_t>(&attribute.typed_value))
      out << *integer_value;
    else if (const auto* unsigned_integer_value =
                 std::get_if<std::uint64_t>(&attribute.typed_value))
      out << *unsigned_integer_value;
    else if (const auto* float_value =
                 std::get_if<double>(&attribute.typed_value)) {
      if (std::isfinite(*float_value))
        out << std::setprecision(17) << *float_value;
      else if (std::isnan(*float_value))
        write_json_string(out, "NaN");
      else
        write_json_string(out, *float_value < 0 ? "-INF" : "INF");
    } else if (const auto* boolean_value =
                   std::get_if<bool>(&attribute.typed_value))
      out << (*boolean_value ? "true" : "false");
    else if (const auto* date_value =
                 std::get_if<NativeDate>(&attribute.typed_value))
      write_json_string(out, date_value->value);
  }
  if (attribute.scalar_wrapper) out << ",\"scalar_wrapper\":true";
  if (attribute.scalar_children_list) out << ",\"scalar_children_list\":true";
  if (!attribute.children.empty()) {
    out << ",\"children\":";
    out << '[';
    for (std::size_t i = 0; i < attribute.children.size(); ++i) {
      if (i != 0) out << ',';
      write_attribute_detail(out, attribute.children[i]);
    }
    out << ']';
  }
  out << '}';
}

void write_optional_attribute_detail(
    std::ostream& out, std::string_view field,
    const std::optional<EventAttribute>& attribute) {
  if (!attribute) return;
  out << ",";
  write_json_string(out, field);
  out << ':';
  write_attribute_detail(out, *attribute);
}

void write_metadata(std::ostream& out, const EventDataMetadata& metadata) {
  out << "{\"extensions\":[";
  for (std::size_t i = 0; i < metadata.extensions.size(); ++i) {
    if (i) out << ',';
    const auto& extension = metadata.extensions[i];
    out << "{\"name\":";
    write_json_string(out, extension.name);
    out << ",\"prefix\":";
    write_json_string(out, extension.prefix);
    out << ",\"uri\":";
    write_json_string(out, extension.uri);
    out << '}';
  }
  out << "],\"classifiers\":[";
  for (std::size_t i = 0; i < metadata.classifiers.size(); ++i) {
    if (i) out << ',';
    const auto& classifier = metadata.classifiers[i];
    out << "{\"name\":";
    write_json_string(out, classifier.name);
    out << ",\"keys\":[";
    for (std::size_t j = 0; j < classifier.keys.size(); ++j) {
      if (j) out << ',';
      write_json_string(out, classifier.keys[j]);
    }
    out << "]}";
  }
  out << "],\"trace_globals\":";
  write_attributes(out, metadata.trace_globals);
  out << ",\"event_globals\":";
  write_attributes(out, metadata.event_globals);
  out << ",\"properties\":";
  write_attributes(out, metadata.properties);
  out << ",\"globals\":[";
  for (std::size_t i = 0; i < metadata.globals.size(); ++i) {
    if (i) out << ',';
    out << "{\"scope\":";
    write_json_string(out, metadata.globals[i].scope);
    out << ",\"attributes\":";
    write_attributes(out, metadata.globals[i].attributes);
    out << '}';
  }
  out << ']';
  out << '}';
}

void write_event_log(std::ostream& out, const EventLog& log) {
  out << "{\"attributes\":";
  write_attributes(out, log.attributes);
  out << ",\"metadata\":";
  write_metadata(out, log.metadata);
  out << ",\"traces\":[";
  for (std::size_t i = 0; i < log.traces.size(); ++i) {
    if (i != 0) out << ',';
    const auto& trace = log.traces[i];
    out << "{\"case_id\":";
    write_json_string(out, trace.case_id);
    out << ",\"case_id_state\":";
    write_json_string(
        out, first_class_state_name(effective_first_class_state(
            trace.case_id_state, trace.case_id)));
     out << ",\"case_id_source_key\":";
     write_json_string(out, trace.case_id_source_key);
     write_optional_attribute_detail(out, "case_id_attribute",
                                     trace.case_id_attribute);
     out << ",\"attributes\":";
    write_attributes(out, trace.attributes);
    out << ",\"properties\":";
    write_attributes(out, trace.properties);
    out << ",\"events\":[";
    for (std::size_t j = 0; j < trace.events.size(); ++j) {
      if (j != 0) out << ',';
      const auto& event = trace.events[j];
      out << "{\"case_id\":";
      write_json_string(out, event.case_id);
      out << ",\"case_id_state\":";
      write_json_string(
          out, first_class_state_name(effective_first_class_state(
              event.case_id_state, event.case_id)));
      out << ",\"case_id_source_key\":";
      write_json_string(out, event.case_id_source_key);
      out << ",\"activity\":";
      write_json_string(out, event.activity);
      out << ",\"activity_state\":";
      write_json_string(
          out, first_class_state_name(effective_first_class_state(
              event.activity_state, event.activity)));
      out << ",\"activity_source_key\":";
      write_json_string(out, event.activity_source_key);
      write_optional_attribute_detail(out, "activity_attribute",
                                      event.activity_attribute);
      out << ",\"timestamp\":";
      write_json_string(out, event.timestamp);
      out << ",\"timestamp_state\":";
      write_json_string(
          out, first_class_state_name(effective_first_class_state(
              event.timestamp_state, event.timestamp)));
      out << ",\"timestamp_source_key\":";
      write_json_string(out, event.timestamp_source_key);
      write_optional_attribute_detail(out, "timestamp_attribute",
                                      event.timestamp_attribute);
      out << ",\"resource\":";
      write_json_string(out, event.resource);
      out << ",\"resource_state\":";
      write_json_string(
          out, first_class_state_name(effective_first_class_state(
              event.resource_state, event.resource)));
      out << ",\"resource_source_key\":";
      write_json_string(out, event.resource_source_key);
      write_optional_attribute_detail(out, "resource_attribute",
                                      event.resource_attribute);
      out << ",\"lifecycle\":";
      write_json_string(out, event.lifecycle);
      out << ",\"lifecycle_state\":";
      write_json_string(
          out, first_class_state_name(effective_first_class_state(
              event.lifecycle_state, event.lifecycle)));
      out << ",\"lifecycle_source_key\":";
      write_json_string(out, event.lifecycle_source_key);
      write_optional_attribute_detail(out, "lifecycle_attribute",
                                      event.lifecycle_attribute);
      out << ",\"input_order\":" << event.input_order
          << ",\"attributes\":";
      write_attributes(out, event.attributes);
      out << '}';
    }
    out << "]}";
  }
  out << "]}";
}

const char* variant_name(LogUtilityVariant variant) {
  switch (variant) {
    case LogUtilityVariant::kSplitTrainTest: return "SPLIT_TRAIN_TEST";
    case LogUtilityVariant::kFixedPrefixes: return "FIXED_PREFIXES";
    case LogUtilityVariant::kAllPrefixes: return "ALL_PREFIXES";
    case LogUtilityVariant::kUntilActivity: return "UNTIL_ACTIVITY";
    case LogUtilityVariant::kToActivities: return "TO_ACTIVITIES";
    case LogUtilityVariant::kFeatureSelection: return "FEATURE_SELECTION";
  }
  return "SPLIT_TRAIN_TEST";
}

}  // namespace

std::string log_train_test_json(const LogTrainTestSplit& result) {
  std::ostringstream out;
  out << "{\"family\":\"LOG_UTIL\",\"variant\":\"SPLIT_TRAIN_TEST\",\"training\":";
  write_event_log(out, result.training);
  out << ",\"testing\":";
  write_event_log(out, result.testing);
  out << '}';
  return out.str();
}

std::string log_prefix_json(
    const EventLog& log, LogUtilityVariant variant,
    const std::vector<std::vector<std::size_t>>& change_indexes) {
  if (variant != LogUtilityVariant::kFixedPrefixes &&
      variant != LogUtilityVariant::kAllPrefixes) {
    throw std::invalid_argument("log prefix JSON requires a prefix variant");
  }
  if (variant == LogUtilityVariant::kAllPrefixes) {
    for (const auto& row : change_indexes) {
      for (const auto index : row) {
        if (index >= log.traces.size())
          throw std::invalid_argument("prefix change index is outside the log");
      }
    }
  }
  std::ostringstream out;
  out << "{\"family\":\"LOG_UTIL\",\"variant\":";
  write_json_string(out, variant_name(variant));
  out << ",\"log\":";
  write_event_log(out, log);
  if (variant == LogUtilityVariant::kAllPrefixes) {
    out << ",\"change_indexes\":[";
    for (std::size_t i = 0; i < change_indexes.size(); ++i) {
      if (i != 0) out << ',';
      out << '[';
      for (std::size_t j = 0; j < change_indexes[i].size(); ++j) {
        if (j != 0) out << ',';
        out << change_indexes[i][j];
      }
      out << ']';
    }
    out << ']';
  }
  out << '}';
  return out.str();
}

std::string log_until_activity_json(const LogUntilActivityResult& result) {
  if (result.log.traces.size() != result.elapsed_seconds.size()) {
    throw std::invalid_argument("until-activity durations do not match the log");
  }
  std::ostringstream out;
  out << "{\"family\":\"LOG_UTIL\",\"variant\":\"UNTIL_ACTIVITY\",\"log\":";
  write_event_log(out, result.log);
  out << ",\"elapsed_seconds\":[" << std::setprecision(17);
  for (std::size_t i = 0; i < result.elapsed_seconds.size(); ++i) {
    if (i != 0) out << ',';
    out << result.elapsed_seconds[i];
  }
  out << "]}";
  return out.str();
}

std::string log_activity_targets_json(const LogActivityTargetResult& result) {
  if (result.activities.size() != result.logs.size())
    throw std::invalid_argument("activity target names do not match sublogs");
  std::ostringstream out;
  out << "{\"family\":\"LOG_UTIL\",\"variant\":\"TO_ACTIVITIES\",\"activities\":[";
  for (std::size_t i = 0; i < result.activities.size(); ++i) {
    if (i != 0) out << ',';
    write_json_string(out, result.activities[i]);
  }
  out << "],\"logs\":[";
  for (std::size_t i = 0; i < result.logs.size(); ++i) {
    if (i != 0) out << ',';
    write_event_log(out, result.logs[i]);
  }
  out << "]}";
  return out.str();
}

std::string log_feature_selection_json(const LogFeatureSelectionResult& result) {
  std::ostringstream out;
  out << "{\"family\":\"LOG_UTIL\",\"variant\":\"FEATURE_SELECTION\",\"event_attributes\":[";
  for (std::size_t i = 0; i < result.event_attributes.size(); ++i) {
    if (i != 0) out << ',';
    write_json_string(out, result.event_attributes[i]);
  }
  out << "],\"case_attributes\":[";
  for (std::size_t i = 0; i < result.case_attributes.size(); ++i) {
    if (i != 0) out << ',';
    write_json_string(out, result.case_attributes[i]);
  }
  out << "]}";
  return out.str();
}

}  // namespace minera
