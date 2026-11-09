#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "minera/event_data.hpp"

namespace minera {

enum class LogConversionVariant {
  kToEventLog,
  kToEventStream,
  kToDataFrame,
  kToAttributedGraph,
  kOneTracePerVariant,
  kAllTracesPerVariant,

  TO_EVENT_LOG = kToEventLog,
  TO_EVENT_STREAM = kToEventStream,
  TO_DATA_FRAME = kToDataFrame,
  TO_NX = kToAttributedGraph,
  DF_TO_EVENT_LOG_1V = kOneTracePerVariant,
  DF_TO_EVENT_LOG_NV = kAllTracesPerVariant,
};

struct LogConversionOptions {
  LogConversionVariant variant = LogConversionVariant::kToEventLog;
  bool include_case_attributes = true;
  std::string case_attribute_prefix = "case:";
  // Literal grouping field for flat inputs and outputs. An empty string is a
  // valid column label, not a request to substitute the canonical case key.
  std::string case_id_key = "case:concept:name";
  // Null cells are retained in stream/table output.  Set this flag for the
  // optional post-processing pass that drops present-null event fields.
  bool remove_null_fields = false;
  // A lazy source may be materialized only up to this many rows.  Exceeding
  // the limit raises std::length_error instead of silently truncating input.
  std::optional<std::size_t> max_rows_to_materialize;
  // These options are accepted for adapters whose implementation can expose
  // identity or compression details. Native copies retain immutable scalar
  // identity in either mode; these flags do not change conversion results.
  bool deep_copy = true;
  bool compress = false;
  bool lazy_output = false;
  std::string graph_case_id_attribute = "concept:name";
  bool include_directly_follows = true;
  std::vector<std::string> case_attributes_as_nodes;
  std::vector<std::string> event_attributes_as_nodes;
  bool return_variants = false;
  std::optional<std::size_t> max_variants_to_return;
  std::size_t max_graph_nodes = 1000000;
  std::size_t max_graph_edges = 2000000;
  // Polled by the log/table/stream conversions every few thousand events;
  // once it answers true they throw OperationCancelled. Unset: never
  // cancels, and the result is unchanged.
  std::function<bool()> cancelled;
};

// Compatibility names retained for clients of the original conversion API.
// Both now denote the native value-semantic event-data objects.
using LogConversionRecord = EventRecord;
using ConvertedEventStream = EventStream;
using LogConversionTable = EventTable;

enum class LogGraphNodeType { kCase, kEvent, kAttribute };
enum class LogGraphEdgeType { kBelongsTo, kDirectlyFollows, kAttribute };

struct LogGraphNode {
  std::string id;
  LogGraphNodeType type = LogGraphNodeType::kAttribute;
  std::vector<EventAttribute> attributes;
};

struct LogGraphEdge {
  std::size_t source = 0;
  std::size_t target = 0;
  LogGraphEdgeType type = LogGraphEdgeType::kBelongsTo;
  std::string attribute_name;
};

struct AttributedLogGraph {
  std::vector<LogGraphNode> nodes;
  std::vector<LogGraphEdge> edges;
};

struct LogVariantProjection {
  std::vector<std::string> activities;
  std::size_t count = 0;
  std::vector<std::size_t> trace_indexes;
};

struct LogConversionResult {
  LogConversionVariant variant = LogConversionVariant::kToEventLog;
  EventLog log;
  ConvertedEventStream stream;
  LogConversionTable table;
  AttributedLogGraph graph;
  std::vector<LogVariantProjection> variants;
  std::optional<LazyEventTable> lazy_table;
};

[[nodiscard]] LogConversionVariant parse_log_conversion_variant(
    std::string_view value);
[[nodiscard]] std::string_view log_conversion_variant_name(
    LogConversionVariant variant);

[[nodiscard]] ConvertedEventStream event_log_to_event_stream(
    const EventLog& log, const LogConversionOptions& options = {});
[[nodiscard]] EventLog event_stream_to_event_log(
    const ConvertedEventStream& stream,
    const LogConversionOptions& options = {});
[[nodiscard]] LogConversionTable event_stream_to_table(
    const ConvertedEventStream& stream);
[[nodiscard]] LogConversionTable event_stream_to_table(
    const ConvertedEventStream& stream,
    const LogConversionOptions& options);
[[nodiscard]] ConvertedEventStream event_table_to_event_stream(
    const LogConversionTable& table,
    const LogConversionOptions& options = {});
[[nodiscard]] EventLog event_table_to_event_log(
    const LogConversionTable& table,
    const LogConversionOptions& options = {});
[[nodiscard]] LogConversionTable event_log_to_table(
    const EventLog& log, const LogConversionOptions& options = {});
[[nodiscard]] LazyEventTable event_log_to_lazy_table(
    const EventLog& log, const LogConversionOptions& options = {});
[[nodiscard]] LogConversionTable lazy_event_table_to_table(
    const LazyEventTable& table,
    const LogConversionOptions& options = {});
[[nodiscard]] ConvertedEventStream lazy_event_table_to_event_stream(
    const LazyEventTable& table,
    const LogConversionOptions& options = {});
[[nodiscard]] EventLog lazy_event_table_to_event_log(
    const LazyEventTable& table,
    const LogConversionOptions& options = {});
[[nodiscard]] ConvertedEventStream event_data_to_event_stream(
    const EventDataView& data, const LogConversionOptions& options = {});
[[nodiscard]] LogConversionTable event_data_to_table(
    const EventDataView& data, const LogConversionOptions& options = {});
[[nodiscard]] EventLog event_data_to_event_log(
    const EventDataView& data, const LogConversionOptions& options = {});
[[nodiscard]] LazyEventTable event_data_to_lazy_table(
    const EventDataView& data, const LogConversionOptions& options = {});
[[nodiscard]] AttributedLogGraph event_log_to_attributed_graph(
    const EventLog& log, const LogConversionOptions& options = {});

[[nodiscard]] LogConversionResult convert_event_log(
    const EventLog& log, const LogConversionOptions& options = {});

[[nodiscard]] std::string log_conversion_json(
    const LogConversionResult& result);

struct XesImportResult;
// Serializes the selected import result, including raw table dtype metadata.
[[nodiscard]] std::string xes_import_result_json(const XesImportResult& result);

}  // namespace minera
