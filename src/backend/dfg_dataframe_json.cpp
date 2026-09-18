#include "minera/dfg_dataframe_json.hpp"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <variant>

namespace minera {
namespace {

void write_string(std::ostream& out, std::string_view value) {
  out << '"';
  for (const unsigned char c : value) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (c < 0x20) {
          constexpr char hex[] = "0123456789abcdef";
          out << "\\u00" << hex[c >> 4] << hex[c & 0x0f];
        } else {
          out << static_cast<char>(c);
        }
    }
  }
  out << '"';
}

void write_number(std::ostream& out, double value, std::string_view name) {
  if (!std::isfinite(value))
    throw std::invalid_argument("DFG dataframe JSON: non-finite " +
                                std::string(name));
  out << std::setprecision(17) << value;
}

const char* backend_name(DfgTableBackend backend) {
  switch (backend) {
    case DfgTableBackend::kPandas: return "pandas";
    case DfgTableBackend::kPolars: return "polars";
  }
  throw std::invalid_argument("DFG dataframe JSON: invalid backend");
}

const char* measure_name(DfgTableMeasure measure) {
  switch (measure) {
    case DfgTableMeasure::kFrequency: return "frequency";
    case DfgTableMeasure::kPerformance: return "performance";
    case DfgTableMeasure::kBoth: return "both";
    case DfgTableMeasure::kCost: return "cost";
  }
  throw std::invalid_argument("DFG dataframe JSON: invalid measure");
}

const char* kind_name(AttributeKind kind) {
  switch (kind) {
    case AttributeKind::kString: return "string";
    case AttributeKind::kDate: return "date";
    case AttributeKind::kInt: return "int";
    case AttributeKind::kFloat: return "float";
    case AttributeKind::kBoolean: return "boolean";
    case AttributeKind::kId: return "id";
    case AttributeKind::kBytes: return "bytes";
  }
  throw std::invalid_argument("DFG dataframe JSON: invalid attribute kind");
}

const char* container_name(AttributeContainerKind container) {
  switch (container) {
    case AttributeContainerKind::kScalar: return "scalar";
    case AttributeContainerKind::kList: return "list";
    case AttributeContainerKind::kContainer: return "container";
    case AttributeContainerKind::kTuple: return "tuple";
    case AttributeContainerKind::kSet: return "set";
    case AttributeContainerKind::kFrozenSet: return "frozenset";
  }
  throw std::invalid_argument(
      "DFG dataframe JSON: invalid attribute container");
}

const char* state_name(FirstClassAttributeState state) {
  switch (state) {
    case FirstClassAttributeState::kMissing: return "missing";
    case FirstClassAttributeState::kPresent: return "present";
    case FirstClassAttributeState::kNull: return "null";
  }
  throw std::invalid_argument("DFG dataframe JSON: invalid attribute state");
}

const char* role_name(FirstClassAttributeRole role) {
  switch (role) {
    case FirstClassAttributeRole::kNone: return "none";
    case FirstClassAttributeRole::kCaseId: return "case_id";
    case FirstClassAttributeRole::kActivity: return "activity";
    case FirstClassAttributeRole::kTimestamp: return "timestamp";
    case FirstClassAttributeRole::kResource: return "resource";
    case FirstClassAttributeRole::kLifecycle: return "lifecycle";
  }
  throw std::invalid_argument("DFG dataframe JSON: invalid attribute role");
}

void write_typed_value(std::ostream& out, const NativeAttributeValue& value) {
  std::visit(
      [&](const auto& item) {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          out << "null";
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
          out << item;
        } else if constexpr (std::is_same_v<T, std::uint64_t>) {
          // Keep uint64 values as JSON integers. Converting through double
          // would silently change identifiers above 2^53.
          out << item;
        } else if constexpr (std::is_same_v<T, double>) {
          write_number(out, item, "typed attribute value");
        } else if constexpr (std::is_same_v<T, bool>) {
          out << (item ? "true" : "false");
        } else if constexpr (std::is_same_v<T, NativeDate>) {
          write_string(out, item.value);
        }
      },
      value);
}

void write_attribute(std::ostream& out, const EventAttribute& attribute);

void write_attributes(std::ostream& out,
                      const std::vector<EventAttribute>& attributes) {
  out << '[';
  for (std::size_t i = 0; i < attributes.size(); ++i) {
    if (i) out << ',';
    write_attribute(out, attributes[i]);
  }
  out << ']';
}

void write_attribute(std::ostream& out, const EventAttribute& attribute) {
  out << "{\"key\":";
  write_string(out, attribute.key);
  out << ",\"value\":";
  write_string(out, attribute.value);
  out << ",\"kind\":";
  write_string(out, kind_name(attribute.kind));
  out << ",\"container\":";
  write_string(out, container_name(attribute.container));
  out << ",\"null\":" << (attribute.is_null ? "true" : "false");
  if (!std::holds_alternative<std::monostate>(attribute.typed_value)) {
    out << ",\"typed_value\":";
    write_typed_value(out, attribute.typed_value);
  }
  if (attribute.first_class_role != FirstClassAttributeRole::kNone) {
    out << ",\"first_class_role\":";
    write_string(out, role_name(attribute.first_class_role));
    out << ",\"first_class_state\":";
    write_string(out, state_name(attribute.first_class_state));
    out << ",\"first_class_source_key\":";
    write_string(out, attribute.first_class_source_key);
  }
  if (attribute.scalar_wrapper) out << ",\"scalar_wrapper\":true";
  if (attribute.scalar_children_list) out << ",\"scalar_children_list\":true";
  if (!attribute.children.empty()) {
    out << ",\"children\":";
    write_attributes(out, attribute.children);
  }
  out << '}';
}

FirstClassAttributeState effective_state(FirstClassAttributeState state,
                                         std::string_view value) {
  return state == FirstClassAttributeState::kMissing && !value.empty()
             ? FirstClassAttributeState::kPresent
             : state;
}

void write_optional_attribute(std::ostream& out, std::string_view name,
                              const std::optional<EventAttribute>& attribute) {
  if (!attribute) return;
  out << ',';
  write_string(out, name);
  out << ':';
  write_attribute(out, *attribute);
}

void write_event(std::ostream& out, const Event& event) {
  out << "{\"case_id\":";
  write_string(out, event.case_id);
  out << ",\"case_id_state\":";
  write_string(out, state_name(effective_state(event.case_id_state,
                                               event.case_id)));
  out << ",\"case_id_source_key\":";
  write_string(out, event.case_id_source_key);
  out << ",\"activity\":";
  write_string(out, event.activity);
  out << ",\"activity_state\":";
  write_string(out, state_name(effective_state(event.activity_state,
                                               event.activity)));
  out << ",\"activity_source_key\":";
  write_string(out, event.activity_source_key);
  out << ",\"timestamp\":";
  write_string(out, event.timestamp);
  out << ",\"timestamp_state\":";
  write_string(out, state_name(effective_state(event.timestamp_state,
                                               event.timestamp)));
  out << ",\"timestamp_source_key\":";
  write_string(out, event.timestamp_source_key);
  out << ",\"resource\":";
  write_string(out, event.resource);
  out << ",\"resource_state\":";
  write_string(out, state_name(effective_state(event.resource_state,
                                               event.resource)));
  out << ",\"resource_source_key\":";
  write_string(out, event.resource_source_key);
  out << ",\"lifecycle\":";
  write_string(out, event.lifecycle);
  out << ",\"lifecycle_state\":";
  write_string(out, state_name(effective_state(event.lifecycle_state,
                                               event.lifecycle)));
  out << ",\"lifecycle_source_key\":";
  write_string(out, event.lifecycle_source_key);
  out << ",\"input_order\":" << event.input_order;
  write_optional_attribute(out, "activity_attribute", event.activity_attribute);
  write_optional_attribute(out, "timestamp_attribute",
                           event.timestamp_attribute);
  write_optional_attribute(out, "resource_attribute", event.resource_attribute);
  write_optional_attribute(out, "lifecycle_attribute",
                           event.lifecycle_attribute);
  out << ",\"attributes\":";
  write_attributes(out, event.attributes);
  out << '}';
}

void write_summary(std::ostream& out, const DfgPerformanceSummary& summary) {
  out << "{\"count\":" << summary.count << ",\"mean\":";
  write_number(out, summary.mean, "performance mean");
  out << ",\"median\":";
  write_number(out, summary.median, "performance median");
  out << ",\"min\":";
  write_number(out, summary.minimum, "performance minimum");
  out << ",\"max\":";
  write_number(out, summary.maximum, "performance maximum");
  out << ",\"stdev\":";
  write_number(out, summary.standard_deviation, "performance standard deviation");
  out << ",\"sum\":";
  write_number(out, summary.sum, "performance sum");
  out << '}';
}

void write_values(std::ostream& out, const std::vector<double>& values) {
  out << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    write_number(out, values[i], "performance observation");
  }
  out << ']';
}

void write_selected_value(std::ostream& out, const DfgPerformanceValue& value) {
  std::visit(
      [&](const auto& selected) {
        using T = std::decay_t<decltype(selected)>;
        if constexpr (std::is_same_v<T, double>) {
          write_number(out, selected, "selected performance value");
        } else if constexpr (std::is_same_v<T, std::vector<double>>) {
          write_values(out, selected);
        } else {
          write_summary(out, selected);
        }
      },
      value);
}

void write_performance_entries(std::ostream& out, const DfgPerformance& values,
                               std::string_view value_name) {
  out << '[';
  std::size_t index = 0;
  for (const auto& [pair, statistics] : values) {
    if (index++) out << ',';
    out << "{\"source\":";
    write_string(out, pair.first);
    out << ",\"target\":";
    write_string(out, pair.second);
    out << ",\"summary\":";
    write_summary(out, statistics.summary);
    out << ",\"raw_values\":";
    write_values(out, statistics.raw_values);
    out << ",";
    write_string(out, value_name);
    out << ":";
    write_selected_value(out, statistics.value);
    out << '}';
  }
  out << ']';
}

void write_frequency_entries(std::ostream& out, const DfgFrequency& values) {
  out << '[';
  std::size_t index = 0;
  for (const auto& [pair, count] : values) {
    if (index++) out << ',';
    out << "{\"source\":";
    write_string(out, pair.first);
    out << ",\"target\":";
    write_string(out, pair.second);
    out << ",\"count\":" << count << '}';
  }
  out << ']';
}

}  // namespace

std::string serialize_dfg_dataframe_json(const DfgTableResult& result) {
  std::ostringstream out;
  out << "{\"operation\":\"graph\",\"backend\":";
  write_string(out, backend_name(result.backend));
  out << ",\"measure\":";
  write_string(out, measure_name(result.measure));
  if (result.measure == DfgTableMeasure::kFrequency ||
      result.measure == DfgTableMeasure::kBoth) {
    out << ",\"frequency\":";
    write_frequency_entries(out, result.frequency);
  }
  if (result.measure == DfgTableMeasure::kPerformance ||
      result.measure == DfgTableMeasure::kBoth) {
    out << ",\"performance\":";
    write_performance_entries(out, result.performance, "value");
  }
  if (result.measure == DfgTableMeasure::kCost) {
    out << ",\"cost\":";
    write_performance_entries(out, result.performance, "value");
  }
  out << '}';
  return out.str();
}

std::string serialize_dfg_partial_order_json(
    const DfgPartialOrderResult& result) {
  std::ostringstream out;
  out << "{\"operation\":\"partial_order\",\"backend\":";
  write_string(out, backend_name(result.backend));
  out << ",\"pairs\":[";
  for (std::size_t i = 0; i < result.pairs.size(); ++i) {
    if (i) out << ',';
    const auto& pair = result.pairs[i];
    out << "{\"source_index\":";
    write_number(out, pair.source_index, "partial-order source index");
    out << ",\"target_index\":";
    write_number(out, pair.target_index, "partial-order target index");
    out << ",\"source_index_value\":";
    write_string(out, pair.source_index_value);
    out << ",\"target_index_value\":";
    write_string(out, pair.target_index_value);
    out << ",\"flow_time_seconds\":";
    write_number(out, pair.flow_time_seconds, "partial-order flow time");
    out << ",\"source\":";
    write_event(out, pair.source);
    out << ",\"target\":";
    write_event(out, pair.target);
    out << '}';
  }
  out << "]}";
  return out.str();
}

std::string serialize_dfg_concurrent_events_json(
    const DfgConcurrentResult& result) {
  std::ostringstream out;
  out << "{\"operation\":\"concurrent_events\",\"backend\":";
  write_string(out, backend_name(result.backend));
  out << ",\"columns\":{\"max_start_column\":";
  write_string(out, result.max_start_column);
  out << ",\"min_complete_column\":";
  write_string(out, result.min_complete_column);
  out << ",\"overlap_column\":";
  write_string(out, result.overlap_column);
  out << '}';
  out << ",\"pairs\":[";
  for (std::size_t i = 0; i < result.pairs.size(); ++i) {
    if (i) out << ',';
    const auto& pair = result.pairs[i];
    out << "{\"left_index\":";
    write_number(out, pair.left_index, "concurrent left index");
    out << ",\"right_index\":";
    write_number(out, pair.right_index, "concurrent right index");
    out << ",\"left_index_value\":";
    write_string(out, pair.left_index_value);
    out << ",\"right_index_value\":";
    write_string(out, pair.right_index_value);
    out << ",\"max_start_seconds\":";
    write_number(out, pair.max_start_seconds, "concurrent maximum start");
    out << ",\"min_complete_seconds\":";
    write_number(out, pair.min_complete_seconds,
                 "concurrent minimum completion");
    out << ",\"overlap_seconds\":";
    write_number(out, pair.overlap_seconds, "concurrent overlap");
    out << ",\"left\":";
    write_event(out, pair.left);
    out << ",\"right\":";
    write_event(out, pair.right);
    out << '}';
  }
  out << "]}";
  return out.str();
}

}  // namespace minera
