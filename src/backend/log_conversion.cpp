#include "minera/log_conversion.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

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

void add_if_present(std::vector<EventAttribute>& attributes,
                    std::string_view key, const std::string& value,
                    AttributeKind kind = AttributeKind::kString) {
  if (!value.empty())
    upsert(attributes, {std::string(key), value, kind});
}

std::vector<EventAttribute> event_fields(const Event& event) {
  std::vector<EventAttribute> fields;
  fields.reserve(event.attributes.size() + 4);
  add_if_present(fields, kActivityKey, event.activity);
  add_if_present(fields, kTimestampKey, event.timestamp, AttributeKind::kDate);
  add_if_present(fields, kResourceKey, event.resource);
  add_if_present(fields, kLifecycleKey, event.lifecycle);
  for (const auto& attribute : event.attributes) upsert(fields, attribute);
  return fields;
}

std::vector<EventAttribute> trace_fields(const Trace& trace) {
  std::vector<EventAttribute> fields;
  fields.reserve(trace.attributes.size() + 1);
  upsert(fields, {std::string(kActivityKey), trace.case_id,
                  AttributeKind::kString});
  for (const auto& attribute : trace.attributes) upsert(fields, attribute);
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
      key == "case:concept:name")
    return EventAttribute{std::string(key), trace.case_id,
                          AttributeKind::kString};
  if (const auto* value = find_attribute(trace.attributes, key)) {
    if (value->is_null) return std::nullopt;
    return *value;
  }
  return std::nullopt;
}

std::optional<EventAttribute> selected_event_attribute(
    const Event& event, std::string_view key) {
  if (key == "concept:name" || key == "activity")
    return EventAttribute{std::string(key), event.activity,
                          AttributeKind::kString};
  if (key == "time:timestamp" || key == "timestamp")
    return event.timestamp.empty()
               ? std::nullopt
               : std::optional<EventAttribute>{{std::string(key),
                                                 event.timestamp,
                                                 AttributeKind::kDate}};
  if (key == "org:resource" || key == "resource")
    return event.resource.empty()
               ? std::nullopt
               : std::optional<EventAttribute>{{std::string(key),
                                                 event.resource,
                                                 AttributeKind::kString}};
  if (key == "lifecycle:transition" || key == "lifecycle")
    return event.lifecycle.empty()
               ? std::nullopt
               : std::optional<EventAttribute>{{std::string(key),
                                                 event.lifecycle,
                                                 AttributeKind::kString}};
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

bool is_case_attribute_field(const EventAttribute& field,
                             const LogConversionOptions& options) {
  if (!options.include_case_attributes) return false;
  // An empty prefix is an explicit request to treat every non-glue field as
  // trace data.  This mirrors the flattening contract instead of relying on
  // string::starts_with("") for accidental matching.
  if (options.case_attribute_prefix.empty())
    return field.key != options.case_id_key;
  return field.key.starts_with(options.case_attribute_prefix);
}

Event record_to_event(const LogConversionRecord& record,
                      std::string case_id,
                      const LogConversionOptions& options) {
  Event event;
  event.case_id = std::move(case_id);
  for (const auto& field : record.fields) {
    if (field.key == options.case_id_key ||
        is_case_attribute_field(field, options))
      continue;
    // A present null is not a textual value for a standard field.  Keep null
    // generic attributes below, but leave the first-class member empty.
    if (field.is_null && (field.key == kActivityKey ||
                          field.key == "activity" ||
                          field.key == kTimestampKey ||
                          field.key == "timestamp" ||
                          field.key == kResourceKey ||
                          field.key == "resource" ||
                          field.key == kLifecycleKey ||
                          field.key == "lifecycle"))
      continue;
    if (field.key == kActivityKey || field.key == "activity")
      event.activity = field.value;
    else if (field.key == kTimestampKey || field.key == "timestamp")
      event.timestamp = field.value;
    else if (field.key == kResourceKey || field.key == "resource")
      event.resource = field.value;
    else if (field.key == kLifecycleKey || field.key == "lifecycle")
      event.lifecycle = field.value;
    else
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
  }
  throw std::invalid_argument("log conversion: invalid attribute kind");
}

void write_attributes(std::ostream& output,
                      const std::vector<EventAttribute>& attributes) {
  output << '[';
  for (std::size_t i = 0; i < attributes.size(); ++i) {
    if (i != 0) output << ',';
    output << "{\"key\":\"" << json_escape(attributes[i].key)
           << "\",\"value\":\"" << json_escape(attributes[i].value)
           << "\",\"kind\":\"" << kind_name(attributes[i].kind)
           << "\"";
    if (attributes[i].is_null) output << ",\"null\":true";
    output << "}";
  }
  output << ']';
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
           << "\",\"attributes\":";
    write_attributes(output, trace.attributes);
    output << ",\"properties\":";
    write_attributes(output, trace.properties);
    output << ",\"events\":[";
    for (std::size_t event_index = 0; event_index < trace.events.size();
         ++event_index) {
      if (event_index != 0) output << ',';
      const auto& event = trace.events[event_index];
      output << "{\"case_id\":\"" << json_escape(event.case_id)
             << "\",\"activity\":\"" << json_escape(event.activity)
             << "\",\"timestamp\":\"" << json_escape(event.timestamp)
             << "\",\"resource\":\"" << json_escape(event.resource)
             << "\",\"lifecycle\":\"" << json_escape(event.lifecycle)
             << "\",\"input_order\":" << event.input_order
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
    for (const auto& event : trace.events) {
      LogConversionRecord record;
      record.fields = event_fields(event);
      if (options.include_case_attributes) {
        for (const auto& attribute : trace_fields(trace)) {
          auto injected = attribute;
          // The configured glue key is authoritative for the case id.  Other
          // trace attributes use the configured prefix.
          if (injected.key == kActivityKey)
            injected.key = options.case_id_key;
          else
            injected.key = options.case_attribute_prefix + injected.key;
          upsert(record.fields, std::move(injected));
        }
      } else {
        upsert(record.fields,
               {options.case_id_key, std::to_string(trace_index),
                AttributeKind::kString});
      }
      if (options.remove_null_fields) {
        record.fields.erase(
            std::remove_if(record.fields.begin(), record.fields.end(),
                           [](const auto& field) { return field.is_null; }),
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
  std::map<std::pair<AttributeKind, std::string>, std::size_t> trace_indexes;
  for (const auto& record : stream.events) {
    const auto* glue = record.field(options.case_id_key);
    if (!glue || glue->is_null)
      throw std::invalid_argument(
          "log conversion: event stream record has no non-null case id");
    auto [found, inserted] = trace_indexes.emplace(
        std::pair{glue->kind, glue->value}, log.traces.size());
    if (inserted) {
      Trace trace;
      trace.case_id = glue->value;
      if (options.include_case_attributes) {
        for (const auto& field : record.fields) {
          if (!is_case_attribute_field(field, options)) continue;
          auto attribute = field;
          if (!options.case_attribute_prefix.empty())
            attribute.key.erase(0, options.case_attribute_prefix.size());
          if (attribute.key == kActivityKey &&
              !options.case_attribute_prefix.empty()) {
            if (!attribute.is_null) trace.case_id = attribute.value;
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
    auto event = record_to_event(record, trace.case_id, options);
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
      if (options.remove_null_fields && field.is_null) continue;
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
      if (!options.remove_null_fields || !field.is_null)
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
  stream.metadata = table.metadata;
  infer_standard_extensions(table, stream.metadata);
  stream.events.reserve(table.rows.size());
  for (const auto& row : table.rows) {
    LogConversionRecord record;
    record.fields.reserve(table.columns.size());
    for (std::size_t index = 0; index < table.columns.size(); ++index) {
      if (!row[index]) continue;
      if (options.remove_null_fields && row[index]->is_null) continue;
      auto field = *row[index];
      field.key = table.columns[index];
      record.fields.push_back(std::move(field));
    }
    stream.events.push_back(std::move(record));
  }
  return stream;
}

LogConversionTable remove_null_cells(LogConversionTable table) {
  table.validate();
  for (auto& row : table.rows) {
    for (auto& cell : row) {
      if (cell && cell->is_null) cell.reset();
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
  return remove_null_cells(std::move(materialized));
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
                             [](const auto& field) { return field.is_null; }),
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
      return remove_null_cells(*data.table());
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
      return LazyEventTable::from_table(remove_null_cells(*data.table()));
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
            source.attributes(), source.metadata(), source.row_count());
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
    output << ",\"table\":{\"columns\":[";
    for (std::size_t i = 0; i < result.table.columns.size(); ++i) {
      if (i != 0) output << ',';
      output << '\"' << json_escape(result.table.columns[i]) << '\"';
    }
    output << "],\"attributes\":";
    write_attributes(output, result.table.attributes);
    output << ",\"metadata\":";
    write_metadata(output, result.table.metadata);
    output << ",\"rows\":[";
    for (std::size_t row = 0; row < result.table.rows.size(); ++row) {
      if (row != 0) output << ',';
      output << '[';
      for (std::size_t column = 0; column < result.table.rows[row].size();
           ++column) {
        if (column != 0) output << ',';
        if (!result.table.rows[row][column]) {
          output << "null";
          continue;
        }
        const auto& cell = *result.table.rows[row][column];
        output << "{\"value\":\"" << json_escape(cell.value)
               << "\",\"kind\":\"" << kind_name(cell.kind) << "\"";
        if (cell.is_null) output << ",\"null\":true";
        output << "}";
      }
      output << ']';
    }
    output << ']';
    if (result.lazy_table) output << ",\"lazy\":true";
    output << "}";
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
