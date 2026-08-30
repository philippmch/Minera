#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "minera/event_log.hpp"

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
  std::string case_id_key = "case:concept:name";
  std::string graph_case_id_attribute = "concept:name";
  bool include_directly_follows = true;
  std::vector<std::string> case_attributes_as_nodes;
  std::vector<std::string> event_attributes_as_nodes;
  bool return_variants = false;
  std::optional<std::size_t> max_variants_to_return;
  std::size_t max_graph_nodes = 1000000;
  std::size_t max_graph_edges = 2000000;
};

// One ordered, typed record in a flattened stream. Standard event fields and
// injected case fields use the same representation as generic attributes.
struct LogConversionRecord {
  std::vector<EventAttribute> fields;

  [[nodiscard]] const EventAttribute* field(std::string_view key) const;
};

struct ConvertedEventStream {
  std::vector<EventAttribute> attributes;
  std::vector<LogConversionRecord> events;
};

struct LogConversionTable {
  std::vector<std::string> columns;
  std::vector<std::vector<std::optional<EventAttribute>>> rows;
};

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
[[nodiscard]] AttributedLogGraph event_log_to_attributed_graph(
    const EventLog& log, const LogConversionOptions& options = {});

[[nodiscard]] LogConversionResult convert_event_log(
    const EventLog& log, const LogConversionOptions& options = {});

[[nodiscard]] std::string log_conversion_json(
    const LogConversionResult& result);

}  // namespace minera
