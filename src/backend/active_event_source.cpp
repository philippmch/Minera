#include "minera/active_event_source.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <mutex>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "minera/dataframe_format_json.hpp"
#include "minera/graph_alignment_workbench_json.hpp"
#include "minera/workflow_discovery_input_json.hpp"

namespace minera {
namespace {

enum class SourceOrigin { kTable, kCsv, kLog };

struct OriginalSource {
  ActiveSourceFile file;
  ActiveSourceSelectors selectors;
  LogConversionOptions options;
  SourceOrigin origin = SourceOrigin::kTable;
  std::string table_origin;
  std::optional<EventTable> table;
  std::optional<EventLog> log;
  std::string log_error;
  std::string table_error;
  ActiveEventSource::RepresentativeRows representatives;
  // The canonical flat record of each source row, needed only to project a
  // filtered log back onto source rows (with_filters). It is a full copy of
  // the rows, so it is built on first use rather than kept for every source;
  // construction still builds it once to report the same errors.
  mutable std::once_flag native_rows_once;
  mutable std::vector<std::optional<EventRecord>> native_rows;
};

void collect_keys(const EventAttribute& attribute, std::set<std::string>& keys) {
  keys.insert(attribute.key);
  keys.insert(attribute.first_class_source_key);
  for (const auto& child : attribute.children) collect_keys(child, keys);
}

void collect_attributes(const std::vector<EventAttribute>& attributes,
                        std::set<std::string>& keys) {
  for (const auto& attribute : attributes) collect_keys(attribute, keys);
}

void collect_metadata(const EventDataMetadata& metadata,
                      std::set<std::string>& keys) {
  collect_attributes(metadata.trace_globals, keys);
  collect_attributes(metadata.event_globals, keys);
  collect_attributes(metadata.properties, keys);
  for (const auto& global : metadata.globals)
    collect_attributes(global.attributes, keys);
  for (const auto& classifier : metadata.classifiers)
    for (const auto& key : classifier.keys) keys.insert(key);
}

void collect_index(const EventTableIndexLabel& label,
                   std::set<std::string>& keys) {
  if (const auto* scalar = std::get_if<EventAttribute>(&label.value))
    collect_keys(*scalar, keys);
  else
    for (const auto& child : std::get<std::vector<EventTableIndexLabel>>(label.value))
      collect_index(child, keys);
}

std::string marker_key(const OriginalSource& source, const EventLog* current,
                       const std::vector<std::string>& referenced_keys) {
  std::set<std::string> keys(referenced_keys.begin(), referenced_keys.end());
  for (const auto* key : {&source.selectors.case_id_key,
                          &source.selectors.activity_key,
                          &source.selectors.timestamp_key,
                          &source.selectors.resource_key,
                          &source.selectors.lifecycle_key})
    keys.insert(*key);
  if (source.table) {
    const auto& table = *source.table;
    keys.insert(table.columns.begin(), table.columns.end());
    collect_attributes(table.attributes, keys);
    collect_metadata(table.metadata, keys);
    for (const auto& row : table.rows)
      for (const auto& cell : row)
        if (cell) collect_keys(*cell, keys);
    if (table.index)
      for (const auto& label : table.index->labels) collect_index(label, keys);
    for (const auto& category : table.categorical_dtypes)
      if (category) collect_attributes(category->categories, keys);
  }
  const auto collect_log = [&](const EventLog& log) {
    collect_attributes(log.attributes, keys);
    collect_metadata(log.metadata, keys);
    for (const auto& trace : log.traces) {
      keys.insert(trace.case_id_source_key);
      if (trace.case_id_attribute) collect_keys(*trace.case_id_attribute, keys);
      collect_attributes(trace.attributes, keys);
      collect_attributes(trace.properties, keys);
      for (const auto& event : trace.events) {
        for (const auto* key : {&event.case_id_source_key,
                                &event.activity_source_key,
                                &event.timestamp_source_key,
                                &event.resource_source_key,
                                &event.lifecycle_source_key})
          keys.insert(*key);
        for (const auto* selected : {&event.activity_attribute,
                                     &event.timestamp_attribute,
                                     &event.resource_attribute,
                                     &event.lifecycle_attribute})
          if (*selected) collect_keys(**selected, keys);
        collect_attributes(event.attributes, keys);
      }
    }
  };
  if (source.log) collect_log(*source.log);
  if (current) collect_log(*current);
  for (std::size_t suffix = 0;; ++suffix) {
    const auto candidate = "__minera_private_source_row_" + std::to_string(suffix);
    if (!keys.contains(candidate)) return candidate;
    if (suffix == std::numeric_limits<std::size_t>::max())
      throw std::length_error("active source: no provenance attribute key available");
  }
}

EventAttribute ordinal_attribute(const std::string& key, std::size_t ordinal) {
  EventAttribute result{key, std::to_string(ordinal), AttributeKind::kInt};
  result.typed_value = static_cast<std::uint64_t>(ordinal);
  return result;
}

std::optional<std::size_t> remove_marker(Event& event, const std::string& key,
                                        std::size_t bound) {
  std::optional<std::size_t> ordinal;
  std::size_t matches = 0;
  for (const auto& attribute : event.attributes) {
    if (attribute.key != key) continue;
    ++matches;
    const auto* value = std::get_if<std::uint64_t>(&attribute.typed_value);
    if (value && *value < bound && attribute.value == std::to_string(*value) &&
        attribute.kind == AttributeKind::kInt && !attribute.is_null &&
        attribute.container == AttributeContainerKind::kScalar &&
        attribute.children.empty() && !attribute.scalar_wrapper &&
        !attribute.scalar_children_list &&
        attribute.first_class_role == FirstClassAttributeRole::kNone)
      ordinal = static_cast<std::size_t>(*value);
  }
  std::erase_if(event.attributes,
                [&](const auto& attribute) { return attribute.key == key; });
  return matches == 1 ? ordinal : std::nullopt;
}

LogConversionOptions preparation_options(const ActiveSourceSelectors& selectors) {
  LogConversionOptions options;
  options.case_id_key = selectors.case_id_key;
  options.include_case_attributes = false;
  return options;
}

EventStream selected_stream(const EventTable& table,
                            const ActiveSourceSelectors& selectors,
                            const LogConversionOptions& options) {
  auto stream = event_table_to_event_stream(table, options);
  const std::array<std::pair<FirstClassAttributeRole, const std::string*>, 5> roles{{
      {FirstClassAttributeRole::kCaseId, &selectors.case_id_key},
      {FirstClassAttributeRole::kActivity, &selectors.activity_key},
      {FirstClassAttributeRole::kTimestamp, &selectors.timestamp_key},
      {FirstClassAttributeRole::kResource, &selectors.resource_key},
      {FirstClassAttributeRole::kLifecycle, &selectors.lifecycle_key}}};
  for (auto& record : stream.events) {
    for (auto& field : record.fields) {
      const auto source_role = field.first_class_role;
      const auto source_state = field.first_class_state;
      const auto source_key = field.first_class_source_key;
      field.first_class_role = FirstClassAttributeRole::kNone;
      field.first_class_state = FirstClassAttributeState::kMissing;
      field.first_class_source_key.clear();
      for (const auto& [role, key] : roles) {
        if (field.key != *key) continue;
        // A single physical cell has one native role. Keep the native role
        // priority when selectors name the same column; do not add columns.
        field.first_class_role = role;
        field.first_class_state = source_role == role
                                      ? source_state
                                      : (field.is_null ? FirstClassAttributeState::kNull
                                                       : FirstClassAttributeState::kPresent);
        field.first_class_source_key = source_role == role ? source_key : *key;
        break;
      }
    }
  }
  return stream;
}

EventStream canonical_stream(const EventLog& log,
                             const LogConversionOptions& preparation) {
  auto options = preparation;
  options.include_case_attributes = true;
  auto stream = event_log_to_event_stream(log, options);
  // Missing/null traces receive an adapter-only trace-index grouping value.
  // Its renumbering after selection is not a mutation of a source cell.
  std::size_t encounter = 0;
  for (const auto& trace : log.traces) {
    for (std::size_t event = 0; event < trace.events.size(); ++event) {
      for (auto& field : stream.events.at(encounter).fields)
        if (field.first_class_role == FirstClassAttributeRole::kCaseId &&
            trace.case_id_state != FirstClassAttributeState::kPresent &&
            trace.case_id.empty()) field.value.clear();
      ++encounter;
    }
  }
  return stream;
}

std::vector<std::optional<EventRecord>> build_native_rows(const OriginalSource& source) {
  std::vector<std::optional<EventRecord>> native_rows;
  if (!source.log || !source.table) return native_rows;
  native_rows.resize(source.table->rows.size());
  const auto flat = canonical_stream(*source.log, source.options);
  std::size_t encounter = 0;
  for (const auto& rows : source.representatives) {
    for (const auto row : rows) {
      if (row && *row < native_rows.size())
        native_rows[*row] = flat.events.at(encounter);
      ++encounter;
    }
  }
  return native_rows;
}

// Builds the rows once to raise any error at construction, as before, and
// drops them; native_rows() rebuilds them (deterministically) when needed.
void retain_native_rows(OriginalSource& source) { (void)build_native_rows(source); }

const std::vector<std::optional<EventRecord>>& native_rows(const OriginalSource& source) {
  std::call_once(source.native_rows_once, [&] {source.native_rows = build_native_rows(source);});
  return source.native_rows;
}

bool equal_attribute(const EventAttribute& left, const EventAttribute& right) {
  if (left.key != right.key || left.value != right.value ||
      left.kind != right.kind || left.is_null != right.is_null ||
      left.container != right.container || left.scalar_wrapper != right.scalar_wrapper ||
      left.scalar_children_list != right.scalar_children_list ||
      left.first_class_role != right.first_class_role ||
      left.first_class_state != right.first_class_state ||
      left.first_class_source_key != right.first_class_source_key ||
      left.typed_value.index() != right.typed_value.index() ||
      left.children.size() != right.children.size()) return false;
  if (const auto* value = std::get_if<double>(&left.typed_value)) {
    if (std::bit_cast<std::uint64_t>(*value) !=
        std::bit_cast<std::uint64_t>(std::get<double>(right.typed_value))) return false;
  } else if (const auto* value = std::get_if<NativeDate>(&left.typed_value)) {
    const auto& other = std::get<NativeDate>(right.typed_value);
    if (value->value != other.value || value->resolution != other.resolution ||
        value->timezone != other.timezone) return false;
  } else if (!(left.typed_value == right.typed_value)) {
    return false;
  }
  for (std::size_t index = 0; index < left.children.size(); ++index)
    if (!equal_attribute(left.children[index], right.children[index])) return false;
  return true;
}

struct ProjectedOccurrence {
  std::size_t source_row;
  std::vector<std::pair<std::string, std::optional<EventAttribute>>> changes;
};

std::vector<std::pair<std::string, std::optional<EventAttribute>>> record_changes(
    const EventRecord& before, const EventRecord& after) {
  std::vector<std::pair<std::string, std::optional<EventAttribute>>> changes;
  std::set<std::string> keys;
  for (const auto* record : {&before, &after})
    for (const auto& field : record->fields) keys.insert(field.key);
  for (const auto& key : keys) {
    const auto* old_value = before.field(key);
    const auto* new_value = after.field(key);
    if (old_value && new_value && equal_attribute(*old_value, *new_value)) continue;
    if (!old_value && !new_value) continue;
    changes.emplace_back(key, new_value ? std::optional<EventAttribute>(*new_value)
                                        : std::nullopt);
  }
  return changes;
}

EventTable project_table(const OriginalSource& source, const EventLog& log,
                         const ActiveEventSource::RepresentativeRows& representatives,
                         std::vector<std::size_t>& ordinals,
                         std::vector<ActiveSourceCellChange>& changes) {
  if (!source.table)
    throw std::invalid_argument(source.table_error.empty()
                                    ? "active source: original table is unavailable"
                                    : source.table_error);
  const auto flat = canonical_stream(log, source.options);
  const auto& source_rows = native_rows(source);
  std::vector<ProjectedOccurrence> occurrences;
  occurrences.reserve(flat.events.size());
  bool transformed = false;
  std::set<std::size_t> seen;
  std::size_t encounter = 0;
  for (const auto& trace_rows : representatives) {
    for (const auto row : trace_rows) {
      if (!row || *row >= source.table->rows.size() ||
          *row >= source_rows.size() || !source_rows[*row])
        throw std::invalid_argument(
            "active source: filtered event has no source-row representative; checked flat projection is unavailable");
      auto mutations = record_changes(*source_rows[*row], flat.events.at(encounter));
      transformed = transformed || !mutations.empty() || !seen.insert(*row).second;
      occurrences.push_back({*row, std::move(mutations)});
      ++encounter;
    }
  }
  if (encounter != flat.events.size())
    throw std::invalid_argument("active source: filtered event provenance width is inconsistent");
  // A pure selection keeps physical source order. Mutations or repeated
  // occurrences retain deterministic native filtered trace/event order.
  if (!transformed)
    std::stable_sort(occurrences.begin(), occurrences.end(), [](const auto& left, const auto& right) {
      return left.source_row < right.source_row;
    });

  EventTable result = source.origin == SourceOrigin::kCsv
                          ? project_csv_event_table(*source.table, log)
                          : *source.table;
  result.rows.clear();
  result.columns = source.table->columns;
  result.dtypes = source.table->dtypes;
  result.categorical_dtypes = source.table->categorical_dtypes;
  // An added field is a new projection column. Existing declarations remain
  // source declarations; the new field uses the generic object declaration.
  for (const auto& occurrence : occurrences) {
    for (const auto& [key, value] : occurrence.changes) {
      if (!value || std::find(result.columns.begin(), result.columns.end(), key) != result.columns.end()) continue;
      result.columns.push_back(key);
      if (!result.dtypes.empty()) result.dtypes.push_back("object");
      if (!result.categorical_dtypes.empty()) result.categorical_dtypes.push_back(std::nullopt);
    }
  }
  bool identity = occurrences.size() == source.table->rows.size();
  for (std::size_t index = 0; identity && index < occurrences.size(); ++index)
    identity = occurrences[index].source_row == index;
  result.index = source.table->index;
  if (!result.index && !identity) result.index = EventTableIndex{};
  if (result.index) {
    if (!identity && result.index->kind == "RangeIndex") result.index->kind = "Index";
    result.index->labels.clear();
  }
  for (const auto& occurrence : occurrences) {
    auto row = source.table->rows[occurrence.source_row];
    row.resize(result.columns.size());
    const auto checked_row = result.rows.size();
    for (const auto& [key, value] : occurrence.changes) {
      const auto found = std::find(result.columns.begin(), result.columns.end(), key);
      if (found == result.columns.end()) continue;
      const auto column = static_cast<std::size_t>(found - result.columns.begin());
      row[column] = value;
      changes.push_back({checked_row, occurrence.source_row, key, !value});
    }
    result.rows.push_back(std::move(row));
    ordinals.push_back(occurrence.source_row);
    if (result.index && source.table->index)
      result.index->labels.push_back(source.table->index->labels[occurrence.source_row]);
    else if (result.index) {
      EventAttribute label{"", std::to_string(occurrence.source_row), AttributeKind::kInt};
      label.typed_value = static_cast<std::int64_t>(occurrence.source_row);
      result.index->labels.push_back({std::move(label)});
    }
  }
  result.validate();
  return result;
}

std::vector<std::string> filter_keys(const LogFilterOptions& options) {
  std::vector<std::string> keys{options.attribute_key, options.case_attribute_key};
  if (options.numeric_attribute) keys.push_back(options.numeric_attribute->attribute_key);
  if (options.top_activities) keys.push_back(options.top_activities->attribute_key);
  if (options.relative_attribute_occurrence) keys.push_back(options.relative_attribute_occurrence->attribute_key);
  if (options.attribute_occurrence_threshold) keys.push_back(options.attribute_occurrence_threshold->attribute_key);
  if (options.attribute_value_repetition) keys.push_back(options.attribute_value_repetition->attribute_key);
  if (options.consecutive_activity_grouping) keys.push_back(options.consecutive_activity_grouping->activity_key);
  if (options.timestamp_case_grouping) {
    keys.push_back(options.timestamp_case_grouping->activity_key);
    keys.push_back(options.timestamp_case_grouping->timestamp_key);
  }
  return keys;
}

std::string quote(const std::string& value) {
  const auto array = serialize_alignment_trace_words_json({value});
  return array.substr(1, array.size() - 2);
}

std::string safe_legacy_json(std::string value) {
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t offset = 0; offset + 2 < value.size(); ++offset) {
    const auto first = static_cast<unsigned char>(value[offset]);
    const auto second = static_cast<unsigned char>(value[offset + 1]);
    const auto third = static_cast<unsigned char>(value[offset + 2]);
    if (first != 0xed || second < 0xa0 || second > 0xbf || third < 0x80 || third > 0xbf) continue;
    const unsigned codepoint = ((first & 15u) << 12) | ((second & 63u) << 6) | (third & 63u);
    std::string escaped = "\\u0000";
    for (unsigned digit = 0; digit < 4; ++digit)
      escaped[2 + digit] = hex[(codepoint >> (12 - 4 * digit)) & 15u];
    value.replace(offset, 3, escaped);
    offset += 5;
  }
  return value;
}

std::string attribute_state_json(const EventAttribute& value) {
  std::string result = "{\"key\":" + quote(value.key) + ",\"value\":" + quote(value.value) +
      ",\"null\":" + std::string(value.is_null ? "true" : "false") +
      ",\"scalar_wrapper\":" + std::string(value.scalar_wrapper ? "true" : "false") +
      ",\"scalar_children_list\":" + std::string(value.scalar_children_list ? "true" : "false") +
      ",\"kind_index\":" + std::to_string(static_cast<int>(value.kind)) +
      ",\"container_index\":" + std::to_string(static_cast<int>(value.container)) +
      ",\"first_class_role_index\":" + std::to_string(static_cast<int>(value.first_class_role)) +
      ",\"first_class_state_index\":" + std::to_string(static_cast<int>(value.first_class_state)) +
      ",\"first_class_source_key\":" + quote(value.first_class_source_key) +
      ",\"typed_value_index\":" + std::to_string(value.typed_value.index()) + ",\"typed_value\":";
  result += std::visit([](const auto& payload) -> std::string {
    using T = std::decay_t<decltype(payload)>;
    if constexpr (std::is_same_v<T, std::monostate>)
      return "{\"type\":\"null\",\"value\":null}";
    else if constexpr (std::is_same_v<T, NativeDate>)
      return "{\"type\":\"date\",\"value\":{\"value\":" + quote(payload.value) +
          ",\"resolution_index\":" + std::to_string(static_cast<int>(payload.resolution)) +
          ",\"timezone\":" + (payload.timezone ? quote(*payload.timezone) : "null") + "}}";
    else if constexpr (std::is_same_v<T, NativeNA>)
      return "{\"type\":\"native_na\",\"value\":\"NA\"}";
    else
      return serialize_workflow_structured_value_json(HeuristicStructuredValue{payload});
  }, static_cast<const NativeAttributeValue::Base&>(value.typed_value));
  result += ",\"children\":[";
  for (std::size_t index = 0; index < value.children.size(); ++index) {
    if (index) result += ',';
    result += attribute_state_json(value.children[index]);
  }
  return result + "]}";
}

std::string attributes_json(const std::vector<EventAttribute>& attributes) {
  EventTable carrier;
  carrier.attributes = attributes;
  std::string result = "{\"carrier\":";
  try {
    result += serialize_event_table_input_json(carrier);
  } catch (const std::exception& error) {
    // Raw EventAttribute storage has wider domains than the table INPUT
    // grammar (for example a retained source key on a role-none attribute).
    // Always retain the complete raw output even if no table carrier exists.
    result += "null,\"carrier_error\":" + quote(error.what());
  }
  result += ",\"native_state\":[";
  for (std::size_t index = 0; index < attributes.size(); ++index) {
    if (index) result += ',';
    result += attribute_state_json(attributes[index]);
  }
  return result + "]}";
}

std::string optional_attribute_json(const std::optional<EventAttribute>& attribute) {
  return attribute ? attributes_json({*attribute}) : "null";
}

std::string metadata_json(const EventDataMetadata& metadata) {
  EventTable carrier;
  carrier.metadata = metadata;
  std::string result = "{\"carrier\":";
  try {
    result += serialize_event_table_input_json(carrier);
  } catch (const std::exception& error) {
    result += "null,\"carrier_error\":" + quote(error.what());
  }
  result += ",\"extensions\":[";
  for (std::size_t index = 0; index < metadata.extensions.size(); ++index) {
    if (index) result += ',';
    const auto& extension = metadata.extensions[index];
    result += "{\"name\":" + quote(extension.name) + ",\"prefix\":" + quote(extension.prefix) +
        ",\"uri\":" + quote(extension.uri) + '}';
  }
  result += "],\"classifiers\":[";
  for (std::size_t index = 0; index < metadata.classifiers.size(); ++index) {
    if (index) result += ',';
    result += "{\"name\":" + quote(metadata.classifiers[index].name) +
        ",\"keys\":" + serialize_alignment_trace_words_json(metadata.classifiers[index].keys) + '}';
  }
  result += "],\"trace_globals\":" + attributes_json(metadata.trace_globals) +
      ",\"event_globals\":" + attributes_json(metadata.event_globals) +
      ",\"properties\":" + attributes_json(metadata.properties) + ",\"globals\":[";
  const auto append_globals = [&](const std::vector<LogGlobal>& globals, std::string& output) {
    for (std::size_t index = 0; index < globals.size(); ++index) {
      if (index) output += ',';
      output += "{\"scope\":" + quote(globals[index].scope) +
          ",\"attributes\":" + attributes_json(globals[index].attributes) + '}';
    }
  };
  append_globals(metadata.globals, result);
  result += "],\"compatibility_snapshot_valid\":" + std::string(metadata.compatibility_snapshot_valid ? "true" : "false") +
      ",\"compatibility_globals_snapshot\":[";
  append_globals(metadata.compatibility_globals_snapshot, result);
  result += "],\"compatibility_trace_globals_snapshot\":" + attributes_json(metadata.compatibility_trace_globals_snapshot) +
      ",\"compatibility_event_globals_snapshot\":" + attributes_json(metadata.compatibility_event_globals_snapshot) + '}';
  return result;
}

std::string event_state_json(const Event& event) {
  std::string result = "{\"input_order\":" + std::to_string(event.input_order) +
      ",\"attributes\":" + attributes_json(event.attributes);
  const auto field = [&](const std::string& name, const std::string& value,
                         FirstClassAttributeState state, const std::string& source_key) {
    result += ',' + quote(name) + ':' + quote(value) + ',' + quote(name + "_state_index") + ':' +
        std::to_string(static_cast<int>(state)) + ',' + quote(name + "_source_key") + ':' + quote(source_key);
  };
  field("case_id", event.case_id, event.case_id_state, event.case_id_source_key);
  field("activity", event.activity, event.activity_state, event.activity_source_key);
  field("timestamp", event.timestamp, event.timestamp_state, event.timestamp_source_key);
  field("resource", event.resource, event.resource_state, event.resource_source_key);
  field("lifecycle", event.lifecycle, event.lifecycle_state, event.lifecycle_source_key);
  return result + ",\"activity_attribute\":" + optional_attribute_json(event.activity_attribute) +
      ",\"timestamp_attribute\":" + optional_attribute_json(event.timestamp_attribute) +
      ",\"resource_attribute\":" + optional_attribute_json(event.resource_attribute) +
      ",\"lifecycle_attribute\":" + optional_attribute_json(event.lifecycle_attribute) + '}';
}

std::string object_json(const std::vector<std::pair<std::string, std::string>>& fields) {
  std::string result = "{";
  for (std::size_t index = 0; index < fields.size(); ++index) {
    if (index) result += ',';
    result += quote(fields[index].first) + ':' + fields[index].second;
  }
  return result + '}';
}

template <class Range, class Writer>
std::string array_json(const Range& values, Writer writer) {
  std::string result = "[";
  bool first = true;
  for (const auto& value : values) {
    if (!first) result += ',';
    first = false;
    result += writer(value);
  }
  return result + ']';
}

std::string boolean_json(bool value) { return value ? "true" : "false"; }

std::string real_json(double value) {
  if (!std::isfinite(value))
    return serialize_workflow_structured_value_json(HeuristicStructuredValue{value});
  std::ostringstream result;
  result.imbue(std::locale::classic());
  result << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
  return result.str();
}

template <class Range>
std::string integers_json(const Range& values) {
  return array_json(values, [](auto value) { return std::to_string(value); });
}

std::string relation_json(const LogSkeletonPair& relation) {
  return object_json({{"source", quote(relation.source)}, {"target", quote(relation.target)}});
}

std::string skeleton_conformance_json(const LogSkeletonConformance& result) {
  return object_json({{"deviations", array_json(result.deviations, [](const auto& deviation) {
    return object_json({{"constraint", quote(deviation.constraint)}, {"relation", relation_json(deviation.relation)},
        {"activity", quote(deviation.activity)}, {"observed_frequency", std::to_string(deviation.observed_frequency)},
        {"allowed_frequencies", integers_json(deviation.allowed_frequencies)}});
  })}, {"no_dev_total", std::to_string(result.no_dev_total)},
      {"no_constr_total", std::to_string(result.no_constr_total)}, {"dev_fitness", real_json(result.dev_fitness)},
      {"is_fit", boolean_json(result.is_fit)}});
}

std::string declare_constraint_json(const DeclareConstraint& constraint) {
  return object_json({{"first", quote(constraint.first)}, {"second", quote(constraint.second)},
      {"binary", boolean_json(constraint.binary)}, {"support", std::to_string(constraint.support)},
      {"confidence", std::to_string(constraint.confidence)}});
}

std::string transition_direction_json(TransitionDirection direction) {
  switch (direction) {
    case TransitionDirection::kForward: return quote("forward");
    case TransitionDirection::kBackward: return quote("backward");
  }
  return quote("UNKNOWN");
}

std::string transition_view_json(TransitionView view) {
  switch (view) {
    case TransitionView::kSequence: return quote("sequence");
    case TransitionView::kSet: return quote("set");
    case TransitionView::kMultiset: return quote("multiset");
  }
  return quote("UNKNOWN");
}

std::string transition_payload_json(const TransitionPayload& values) {
  return array_json(values, [](const auto& value) { return value ? quote(*value) : "null"; });
}

std::string occurrence_json(const TransitionOccurrence& value) {
  return object_json({{"trace_index", std::to_string(value.trace_index)}, {"event_index", std::to_string(value.event_index)}});
}

std::string correlation_variant_json(CorrelationVariant variant) {
  switch (variant) {
    case CorrelationVariant::kClassic: return quote("CLASSIC");
    case CorrelationVariant::kClassicSplit: return quote("CLASSIC_SPLIT");
    case CorrelationVariant::kTraceBased: return quote("TRACE_BASED");
  }
  return quote("UNKNOWN");
}

template <class Map, class Writer>
std::string pair_map_json(const Map& values, Writer writer, const std::string& field) {
  return array_json(values, [&](const auto& entry) {
    return object_json({{"source", quote(entry.first.first)}, {"target", quote(entry.first.second)},
                        {field, writer(entry.second)}});
  });
}

std::string spectrum_variant_json(PerformanceSpectrumVariant variant) {
  switch (variant) {
    case PerformanceSpectrumVariant::kConnected: return quote("CONNECTED");
    case PerformanceSpectrumVariant::kDisconnected: return quote("DISCONNECTED");
    case PerformanceSpectrumVariant::kDataframe: return quote("DATAFRAME");
    case PerformanceSpectrumVariant::kLog: return quote("LOG");
    case PerformanceSpectrumVariant::kDataframeDisconnected: return quote("DATAFRAME_DISCONNECTED");
    case PerformanceSpectrumVariant::kLogDisconnected: return quote("LOG_DISCONNECTED");
    case PerformanceSpectrumVariant::kLazyframe: return quote("LAZYFRAME");
    case PerformanceSpectrumVariant::kLazyframeDisconnected: return quote("LAZYFRAME_DISCONNECTED");
  }
  return quote("UNKNOWN");
}

std::string batch_variant_json(BatchDiscoveryVariant variant) {
  switch (variant) {
    case BatchDiscoveryVariant::kLog: return quote("LOG");
    case BatchDiscoveryVariant::kPandas: return quote("PANDAS");
    case BatchDiscoveryVariant::kPolars: return quote("POLARS");
  }
  return quote("UNKNOWN");
}

std::string batch_type_json(BatchType type) {
  switch (type) {
    case BatchType::kSimultaneous: return quote("simultaneous");
    case BatchType::kBatchingOnStart: return quote("batching_on_start");
    case BatchType::kBatchingOnEnd: return quote("batching_on_end");
    case BatchType::kSequential: return quote("sequential");
    case BatchType::kConcurrent: return quote("concurrent");
  }
  return quote("UNKNOWN");
}

}  // namespace

std::string serialize_active_source_log_json(const EventLog& log) {
  LogConversionResult legacy;
  legacy.log = log;
  std::string result = "{\"legacy_view\":" + safe_legacy_json(log_conversion_json(legacy)) +
      ",\"native_state\":{\"attributes\":" + attributes_json(log.attributes) +
      ",\"metadata\":" + metadata_json(log.metadata) + ",\"traces\":[";
  for (std::size_t trace_index = 0; trace_index < log.traces.size(); ++trace_index) {
    if (trace_index) result += ',';
    const auto& trace = log.traces[trace_index];
    result += "{\"case_id\":" + quote(trace.case_id) +
        ",\"case_id_state_index\":" + std::to_string(static_cast<int>(trace.case_id_state)) +
        ",\"case_id_source_key\":" + quote(trace.case_id_source_key) +
        ",\"case_id_attribute\":" + optional_attribute_json(trace.case_id_attribute) +
        ",\"attributes\":" + attributes_json(trace.attributes) +
        ",\"properties\":" + attributes_json(trace.properties) + ",\"events\":[";
    for (std::size_t event = 0; event < trace.events.size(); ++event) {
      if (event) result += ',';
      result += event_state_json(trace.events[event]);
    }
    result += "]}";
  }
  return result + "]}}";
}

std::string serialize_active_source_stream_json(const EventStream& stream) {
  LogConversionResult legacy;
  legacy.variant = LogConversionVariant::kToEventStream;
  legacy.stream = stream;
  std::string result = "{\"legacy_view\":" + safe_legacy_json(log_conversion_json(legacy)) +
      ",\"native_state\":{\"attributes\":" + attributes_json(stream.attributes) +
      ",\"metadata\":" + metadata_json(stream.metadata) + ",\"events\":[";
  for (std::size_t event = 0; event < stream.events.size(); ++event) {
    if (event) result += ',';
    result += "{\"fields\":" + attributes_json(stream.events[event].fields) + '}';
  }
  return result + "]}}";
}

std::string serialize_active_source_skeleton_json(
    const LogSkeletonModel& model, const std::vector<LogSkeletonConformance>* conformance) {
  std::vector<std::pair<std::string, std::string>> frequency;
  for (const auto& [activity, counts] : model.activ_freq) frequency.emplace_back(activity, integers_json(counts));
  const auto relations = [](const auto& values) { return array_json(values, relation_json); };
  return object_json({{"model", object_json({{"equivalence", relations(model.equivalence)},
      {"always_after", relations(model.always_after)}, {"always_before", relations(model.always_before)},
      {"never_together", relations(model.never_together)}, {"directly_follows", relations(model.directly_follows)},
      {"activ_freq", object_json(frequency)}})},
      {"conformance", conformance ? array_json(*conformance, skeleton_conformance_json) : "null"}});
}

std::string serialize_active_source_declare_json(
    const DeclareModel& model, const std::vector<DeclareConformance>* conformance) {
  std::vector<std::pair<std::string, std::string>> rules;
  for (const auto& [family, constraints] : model.rules)
    rules.emplace_back(family, array_json(constraints, declare_constraint_json));
  return object_json({{"model", object_json({{"rules", object_json(rules)}})},
      {"conformance", conformance ? array_json(*conformance, [](const auto& result) {
    return object_json({{"deviations", array_json(result.deviations, [](const auto& deviation) {
      return object_json({{"template_name", quote(deviation.template_name)}, {"first", quote(deviation.first)},
          {"second", quote(deviation.second)}, {"binary", boolean_json(deviation.binary)}});
    })}, {"no_constr_total", std::to_string(result.no_constr_total)}, {"no_dev_total", std::to_string(result.no_dev_total)},
        {"dev_fitness", real_json(result.dev_fitness)}, {"is_fit", boolean_json(result.is_fit)}});
  }) : "null"}});
}

std::string serialize_active_source_transition_json(const TransitionSystem& result) {
  const auto& options = result.options();
  return object_json({{"options", object_json({{"direction", transition_direction_json(options.direction)},
      {"direction_index", std::to_string(static_cast<int>(options.direction))}, {"view", transition_view_json(options.view)},
      {"view_index", std::to_string(static_cast<int>(options.view))}, {"window", std::to_string(options.window)},
      {"include_data", boolean_json(options.include_data)}, {"activity_key", quote(options.activity_key)},
      {"case_id_key", quote(options.case_id_key)}, {"activity_attribute", quote(options.activity_attribute)},
      {"case_id_attribute", quote(options.case_id_attribute)}})},
      {"states", array_json(result.states(), [](const auto& state) {
    return object_json({{"id", std::to_string(state.id)}, {"payload", transition_payload_json(state.payload)},
        {"start_count", std::to_string(state.start_count)}, {"end_count", std::to_string(state.end_count)},
        {"incoming_occurrences", array_json(state.incoming_occurrences, occurrence_json)},
        {"outgoing_occurrences", array_json(state.outgoing_occurrences, occurrence_json)},
        {"in_root", boolean_json(state.in_root)}, {"name_order", transition_payload_json(state.name_order)},
        {"incoming", integers_json(state.incoming)}, {"outgoing", integers_json(state.outgoing)}});
  })}, {"transitions", array_json(result.transitions(), [](const auto& transition) {
    return object_json({{"source", std::to_string(transition.source)}, {"target", std::to_string(transition.target)},
        {"label", transition.label ? quote(*transition.label) : "null"}, {"count", std::to_string(transition.count)},
        {"occurrences", array_json(transition.occurrences, occurrence_json)}});
  })}});
}

std::string serialize_active_source_distances_json(const MinimumSelfDistances& result) {
  return array_json(result, [](const auto& distance) {
    return object_json({{"activity", quote(distance.activity)}, {"distance", std::to_string(distance.distance)},
        {"witnesses", serialize_alignment_trace_words_json(distance.witnesses)},
        {"activity_is_null", boolean_json(distance.activity_is_null)},
        {"witnesses_include_null", boolean_json(distance.witnesses_include_null)}});
  });
}

std::string serialize_active_source_correlation_json(const CorrelationVariantsAnalysis& result) {
  return object_json({{"variants", array_json(result.variants, [](const auto& named) {
    const auto& analysis = named.analysis;
    std::vector<std::pair<std::string, std::string>> counts;
    for (const auto& [activity, count] : analysis.activity_counts) counts.emplace_back(activity, std::to_string(count));
    return object_json({{"name", quote(named.name)}, {"analysis", object_json({
        {"variant", correlation_variant_json(analysis.variant)}, {"variant_index", std::to_string(static_cast<int>(analysis.variant))},
        {"activity_counts", object_json(counts)},
        {"frequency_dfg", pair_map_json(analysis.frequency_dfg, [](auto value) { return std::to_string(value); }, "count")},
        {"performance_dfg", pair_map_json(analysis.performance_dfg, real_json, "value")},
        {"precede_succeed_probability", pair_map_json(analysis.precede_succeed_probability, real_json, "value")},
        {"duration_matrix", pair_map_json(analysis.duration_matrix, real_json, "value")}})}});
  })}});
}

std::string serialize_active_source_spectrum_json(const PerformanceSpectrum& result) {
  return object_json({{"activities", serialize_alignment_trace_words_json(result.activities)},
      {"observations", array_json(result.observations, [](const auto& observation) {
    return object_json({{"case_id", quote(observation.case_id)}, {"instants", array_json(observation.instants, real_json)},
        {"activities", serialize_alignment_trace_words_json(observation.activities)},
        {"pattern_offset", std::to_string(observation.pattern_offset)}});
  })}, {"variant", spectrum_variant_json(result.variant)}, {"variant_index", std::to_string(static_cast<int>(result.variant))},
      {"activity_attribute", quote(result.activity_attribute)}, {"timestamp_attribute", quote(result.timestamp_attribute)},
      {"case_id_attribute", quote(result.case_id_attribute)}});
}

std::string serialize_active_source_batches_json(const BatchDiscovery& result) {
  const auto& options = result.options;
  return object_json({{"options", object_json({{"variant", batch_variant_json(options.variant)},
      {"variant_index", std::to_string(static_cast<int>(options.variant))},
      {"merge_distance_seconds", real_json(options.merge_distance_seconds)}, {"min_batch_size", std::to_string(options.min_batch_size)},
      {"start_timestamp_attribute", quote(options.start_timestamp_attribute)}, {"activity_attribute", quote(options.activity_attribute)},
      {"resource_attribute", quote(options.resource_attribute)}, {"timestamp_attribute", quote(options.timestamp_attribute)},
      {"case_id_attribute", quote(options.case_id_attribute)}, {"event_id_attribute", quote(options.event_id_attribute)}})},
      {"variant", batch_variant_json(result.variant)}, {"variant_index", std::to_string(static_cast<int>(result.variant))},
      {"groups", array_json(result.groups, [](const auto& group) {
    return object_json({{"activity", quote(group.activity)}, {"resource", group.resource_is_null ? std::string("null") : quote(group.resource)},
        {"batch_count", std::to_string(group.batch_count)}, {"event_count", std::to_string(group.event_count)},
        {"batches", array_json(group.batches, [](const auto& batch) {
      return object_json({{"type", batch_type_json(batch.type)}, {"type_index", std::to_string(static_cast<int>(batch.type))},
          {"start", real_json(batch.start)}, {"complete", real_json(batch.complete)},
          {"events", array_json(batch.events, [](const auto& event) {
        return object_json({{"start", real_json(event.start)}, {"complete", real_json(event.complete)},
            {"case_id", quote(event.case_id)}, {"trace_index", std::to_string(event.trace_index)},
            {"event_index", std::to_string(event.event_index)}, {"event_id", quote(event.event_id)}});
      })}});
    })}});
  })}});
}

std::string serialize_active_source_temporal_profile_json(const TemporalProfile& result) {
  return object_json({{"entries", array_json(result.entries, [](const auto& entry) {
    return object_json({{"source", quote(entry.source)}, {"target", quote(entry.target)},
        {"count", std::to_string(entry.count)}, {"mean_seconds", real_json(entry.mean_seconds)},
        {"standard_deviation_seconds", real_json(entry.standard_deviation_seconds)}});
  })}});
}

std::string serialize_active_source_temporal_conformance_json(const TemporalConformanceResult& result) {
  return array_json(result, [](const auto& bucket) {
    return array_json(bucket, [](const auto& deviation) {
      return object_json({{"case_id", quote(deviation.case_id)}, {"source", quote(deviation.source)},
          {"target", quote(deviation.target)}, {"elapsed_seconds", real_json(deviation.elapsed_seconds)},
          {"standard_deviation_distance", deviation.zero_variance_sentinel ? std::string("9223372036854775807") : real_json(deviation.standard_deviation_distance)}});
    });
  });
}

struct ActiveEventSource::State {
  std::shared_ptr<const OriginalSource> original;
  EventDataKind selected = EventDataKind::kEventTable;
  // An unfiltered source views the original's table and log (aliasing
  // pointers that keep the original alive) instead of copying them; a
  // filtered source owns its own.
  std::shared_ptr<const EventTable> table;
  std::shared_ptr<const EventLog> log;
  // The lazy table scans the shared table in place. The stream is a full
  // copy of the events, so it is built when a route first asks for it;
  // prepare_flat builds it once up front only to report the same errors.
  std::optional<LazyEventTable> lazy;
  bool stream_available = false;
  mutable std::once_flag stream_once;
  mutable std::optional<EventStream> stream;
  std::string log_error;
  std::string flat_error;
  std::string table_origin;
  std::vector<std::size_t> ordinals;
  RepresentativeRows representatives;
  std::vector<ActiveSourceCellChange> changes;

  State() = default;
  State(const State&) = delete;
  State& operator=(const State&) = delete;

  void view_original() {
    if (original->table) table = std::shared_ptr<const EventTable>(original, &*original->table);
    if (original->log) log = std::shared_ptr<const EventLog>(original, &*original->log);
  }
  static LazyEventTable lazy_view(const std::shared_ptr<const EventTable>& source) {
    // As LazyEventTable::from_table, without copying the rows.
    source->validate();
    return LazyEventTable(
        source->columns,
        [source](const LazyEventTable::RowConsumer& consumer) {
          for (const auto& row : source->rows) consumer(row);
        },
        source->attributes, source->metadata, source->rows.size(), source->dtypes,
        source->index, source->categorical_dtypes);
  }
  void prepare_flat() {
    stream.reset();
    stream_available = false;
    lazy.reset();
    if (!table) return;
    try {
      (void)selected_stream(*table, original->selectors, original->options);
      stream_available = true;
      lazy = lazy_view(table);
    } catch (const std::exception& error) {
      flat_error = error.what();
    }
  }
  const EventStream* stream_if_available() const noexcept {
    if (!stream_available || !table) return nullptr;
    try {
      std::call_once(stream_once, [this] {
        stream.emplace(selected_stream(*table, original->selectors, original->options));
      });
    } catch (...) {
      return nullptr;
    }
    return stream ? &*stream : nullptr;
  }
};

ActiveEventSource::ActiveEventSource(std::shared_ptr<const State> state)
    : state_(std::move(state)) {}

ActiveEventSource ActiveEventSource::from_table(EventTable table, ActiveSourceFile file,
                                               ActiveSourceSelectors selectors,
                                               EventDataKind selected) {
  table.validate();
  auto original = std::make_shared<OriginalSource>();
  original->file = std::move(file);
  original->selectors = std::move(selectors);
  original->options = minera::preparation_options(original->selectors);
  original->table_origin = "original_declared_table";
  original->table = std::move(table);
  try {
    auto stream = selected_stream(*original->table, original->selectors, original->options);
    const auto marker = marker_key(*original, nullptr, {});
    for (std::size_t row = 0; row < stream.events.size(); ++row)
      stream.events[row].fields.push_back(ordinal_attribute(marker, row));
    original->log = event_stream_to_event_log(stream, original->options);
    for (auto& trace : original->log->traces) {
      std::vector<std::optional<std::size_t>> rows;
      for (auto& event : trace.events) {
        auto row = remove_marker(event, marker, original->table->rows.size());
        if (!row) throw std::invalid_argument("active source: table conversion lost source-row provenance");
        rows.push_back(row);
      }
      original->representatives.push_back(std::move(rows));
    }
    retain_native_rows(*original);
  } catch (const std::exception& error) {
    original->log.reset();
    original->representatives.clear();
    original->native_rows.clear();
    original->log_error = error.what();
  }
  auto state = std::make_shared<State>();
  state->original = original;
  state->selected = selected;
  state->view_original();
  state->log_error = original->log_error;
  state->table_origin = original->table_origin;
  state->representatives = original->representatives;
  state->ordinals.resize(original->table->rows.size());
  std::iota(state->ordinals.begin(), state->ordinals.end(), std::size_t{0});
  state->prepare_flat();
  return ActiveEventSource(std::move(state));
}

ActiveEventSource ActiveEventSource::from_csv(CsvEventData data, ActiveSourceFile file,
                                             ActiveSourceSelectors selectors,
                                             EventDataKind selected) {
  data.table.validate();
  auto original = std::make_shared<OriginalSource>();
  original->file = std::move(file);
  original->selectors = std::move(selectors);
  original->options = minera::preparation_options(original->selectors);
  original->origin = SourceOrigin::kCsv;
  original->table_origin = "paired_csv_import";
  original->table = std::move(data.table);
  original->log = std::move(data.log);
  for (const auto& trace : original->log->traces) {
    std::vector<std::optional<std::size_t>> rows;
    for (const auto& event : trace.events)
      rows.push_back(event.input_order < original->table->rows.size()
                         ? std::optional<std::size_t>(event.input_order) : std::nullopt);
    original->representatives.push_back(std::move(rows));
  }
  retain_native_rows(*original);
  auto state = std::make_shared<State>();
  state->original = original;
  state->selected = selected;
  state->view_original();
  state->table_origin = original->table_origin;
  state->representatives = original->representatives;
  state->ordinals.resize(original->table->rows.size());
  std::iota(state->ordinals.begin(), state->ordinals.end(), std::size_t{0});
  state->prepare_flat();
  return ActiveEventSource(std::move(state));
}

ActiveEventSource ActiveEventSource::from_log(EventLog log, ActiveSourceFile file,
                                             ActiveSourceSelectors selectors,
                                             EventDataKind selected) {
  auto original = std::make_shared<OriginalSource>();
  original->file = std::move(file);
  original->selectors = std::move(selectors);
  original->options = minera::preparation_options(original->selectors);
  original->origin = SourceOrigin::kLog;
  original->table_origin = "native_log_to_table";
  original->log = std::move(log);
  std::size_t ordinal = 0;
  for (const auto& trace : original->log->traces) {
    std::vector<std::optional<std::size_t>> rows;
    for (std::size_t event = 0; event < trace.events.size(); ++event) rows.push_back(ordinal++);
    original->representatives.push_back(std::move(rows));
  }
  try {
    auto options = original->options;
    options.include_case_attributes = true;
    original->table = event_log_to_table(*original->log, options);
    retain_native_rows(*original);
  } catch (const std::exception& error) {
    original->table.reset();
    original->table_error = error.what();
  }
  auto state = std::make_shared<State>();
  state->original = original;
  state->selected = selected;
  state->view_original();
  state->flat_error = original->table_error;
  state->table_origin = original->table_origin;
  state->representatives = original->representatives;
  if (original->table) {
    state->ordinals.resize(original->table->rows.size());
    std::iota(state->ordinals.begin(), state->ordinals.end(), std::size_t{0});
  }
  state->prepare_flat();
  return ActiveEventSource(std::move(state));
}

const ActiveSourceFile& ActiveEventSource::file() const noexcept { return state_->original->file; }
const ActiveSourceSelectors& ActiveEventSource::selectors() const noexcept { return state_->original->selectors; }
EventDataKind ActiveEventSource::selected_kind() const noexcept { return state_->selected; }
const LogConversionOptions& ActiveEventSource::preparation_options() const noexcept { return state_->original->options; }
const std::string& ActiveEventSource::original_table_origin() const noexcept { return state_->original->table_origin; }
const std::string& ActiveEventSource::checked_table_origin() const noexcept { return state_->table_origin; }
const EventTable* ActiveEventSource::original_table_if_available() const noexcept { return state_->original->table ? &*state_->original->table : nullptr; }
const EventLog* ActiveEventSource::original_log_if_available() const noexcept { return state_->original->log ? &*state_->original->log : nullptr; }
const EventTable* ActiveEventSource::checked_table_if_available() const noexcept { return state_->table ? &*state_->table : nullptr; }
const EventLog* ActiveEventSource::checked_log_if_available() const noexcept { return state_->log ? &*state_->log : nullptr; }
const EventStream* ActiveEventSource::checked_stream_if_available() const noexcept { return state_->stream_if_available(); }
const LazyEventTable* ActiveEventSource::checked_lazy_if_available() const noexcept { return state_->lazy ? &*state_->lazy : nullptr; }
const std::string& ActiveEventSource::checked_log_preparation_error() const noexcept { return state_->log_error; }
const std::string& ActiveEventSource::checked_flat_projection_error() const noexcept { return state_->flat_error; }
const std::vector<std::size_t>& ActiveEventSource::source_row_ordinals() const noexcept { return state_->ordinals; }
const ActiveEventSource::RepresentativeRows& ActiveEventSource::representative_row_ordinals() const noexcept { return state_->representatives; }
const std::vector<ActiveSourceCellChange>& ActiveEventSource::projection_changes() const noexcept { return state_->changes; }

const EventTable& ActiveEventSource::original_table() const {
  if (!state_->original->table) throw std::invalid_argument(state_->original->table_error);
  return *state_->original->table;
}
const EventLog& ActiveEventSource::original_log() const {
  if (!state_->original->log) throw std::invalid_argument(state_->original->log_error);
  return *state_->original->log;
}
const EventTable& ActiveEventSource::checked_table() const {
  if (!state_->table) throw std::invalid_argument(state_->flat_error);
  return *state_->table;
}
const EventLog& ActiveEventSource::checked_log() const {
  if (!state_->log) throw std::invalid_argument(state_->log_error);
  return *state_->log;
}
const EventStream& ActiveEventSource::checked_stream() const {
  const auto* stream = state_->stream_if_available();
  if (!stream) throw std::invalid_argument(state_->flat_error);
  return *stream;
}
const LazyEventTable& ActiveEventSource::checked_lazy() const {
  if (!state_->lazy) throw std::invalid_argument(state_->flat_error);
  return *state_->lazy;
}
EventDataView ActiveEventSource::view(EventDataKind kind) const {
  switch (kind) {
    case EventDataKind::kEventLog: return EventDataView(checked_log());
    case EventDataKind::kEventStream: return EventDataView(checked_stream());
    case EventDataKind::kEventTable: return EventDataView(checked_table());
    case EventDataKind::kLazyEventTable: return EventDataView(checked_lazy());
  }
  throw std::invalid_argument("active source: invalid input shape");
}
EventDataView ActiveEventSource::selected_view() const { return view(state_->selected); }
EventTable ActiveEventSource::mutable_table_copy() const { return checked_table(); }

ActiveEventSource ActiveEventSource::with_filters(const LogFilterOptions& options) const {
  return with_filters([&](const EventLog& log) { return filter_event_log(log, options); }, filter_keys(options));
}

ActiveEventSource ActiveEventSource::with_filters(
    const NativeFilterPipeline& pipeline,
    const std::vector<std::string>& referenced_attribute_keys) const {
  if (!pipeline) throw std::invalid_argument("active source: native filter pipeline is empty");
  auto tagged = checked_log();
  const auto marker = marker_key(*state_->original, &tagged, referenced_attribute_keys);
  std::vector<std::optional<std::size_t>> rows;
  for (std::size_t trace = 0; trace < tagged.traces.size(); ++trace) {
    for (std::size_t event = 0; event < tagged.traces[trace].events.size(); ++event) {
      const auto row = trace < state_->representatives.size() &&
                               event < state_->representatives[trace].size()
                           ? state_->representatives[trace][event] : std::nullopt;
      tagged.traces[trace].events[event].attributes.push_back(ordinal_attribute(marker, rows.size()));
      rows.push_back(row);
    }
  }
  auto filtered = pipeline(tagged);
  auto output = std::make_shared<State>();
  output->original = state_->original;
  output->selected = state_->selected;
  output->table_origin = "source_rows_with_native_filtered_field_overlays";
  for (auto& trace : filtered.traces) {
    std::vector<std::optional<std::size_t>> representatives;
    for (auto& event : trace.events) {
      const auto ordinal = remove_marker(event, marker, rows.size());
      representatives.push_back(ordinal ? rows[*ordinal] : std::nullopt);
    }
    output->representatives.push_back(std::move(representatives));
  }
  output->log = std::make_shared<const EventLog>(std::move(filtered));
  try {
    output->table = std::make_shared<const EventTable>(
        project_table(*output->original, *output->log, output->representatives,
                      output->ordinals, output->changes));
    output->prepare_flat();
  } catch (const std::exception& error) {
    output->table.reset();
    output->stream.reset();
    output->stream_available = false;
    output->lazy.reset();
    output->ordinals.clear();
    output->changes.clear();
    output->flat_error = error.what();
  }
  return ActiveEventSource(std::move(output));
}

}  // namespace minera
