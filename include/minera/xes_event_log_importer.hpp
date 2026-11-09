#pragma once

#include <filesystem>
#include <functional>
#include <istream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "minera/event_data.hpp"

namespace minera {

enum class XesImportVariant {
  kIterparse,
  kLineByLine,
  kIterparseMemoryCompressed,
  kIterparse20,
  kChunkRegex,
  kRustXes,
  ITERPARSE = kIterparse,
  LINE_BY_LINE = kLineByLine,
  ITERPARSE_MEM_COMPRESSED = kIterparseMemoryCompressed,
  ITERPARSE_20 = kIterparse20,
  CHUNK_REGEX = kChunkRegex,
  RUSTXES = kRustXes,
};

[[nodiscard]] XesImportVariant parse_xes_import_variant(
    std::string_view value);
[[nodiscard]] std::string_view xes_import_variant_name(
    XesImportVariant variant);

// XES (eXtensible Event Stream, IEEE 1849-2016) is the standard interchange
// format for process-mining event logs. Minera reads the common subset: a
// <log> of <trace> elements, each carrying <event> elements, where leaf
// attribute elements (<string>, <date>, <int>, …) supply the case id,
// activity name, and timestamp via their standard `concept:name` /
// `time:timestamp` keys.
struct XesImportOptions {
  // Parser strategy selector. The EventLog convenience API retains its
  // native log contract; the result API also exposes Rust table modes.
  XesImportVariant variant = XesImportVariant::kIterparse;
  // Trace-level attribute that identifies the case.
  std::string case_attribute = "concept:name";
  // Event-level attribute that names the activity.
  std::string activity_attribute = "concept:name";
  // Event-level attribute that carries the timestamp.
  std::string timestamp_attribute = "time:timestamp";
  // Event-level attribute that names the performing resource (org perspective).
  std::string resource_attribute = "org:resource";
  // Event-level attribute for the lifecycle transition ("start", "complete").
  std::string lifecycle_attribute = "lifecycle:transition";
  // When set, a log whose events never carry the timestamp attribute is
  // rejected instead of imported with empty timestamps.
  bool require_timestamp = false;
  // Preserve XML trace/event order by default. When enabled, events are
  // sorted within each trace and traces by their first selected timestamp.
  bool timestamp_sort = false;
  bool reverse_sort = false;
  // Import at most this many traces in XML order. Zero imports no traces.
  std::size_t max_traces = std::numeric_limits<std::size_t>::max();
  // Empty means all scalar attributes. First-class selectors are always read
  // so the resulting EventLog remains structurally valid.
  std::vector<std::string> attributes_to_read;
  // Bounds the XML document (plain, or after transparent gzip expansion).
  // Reading holds about 17 times the document size, so 256 MiB needs about
  // 4.3 GB; raise it explicitly for larger logs on larger machines.
  std::size_t max_decompressed_bytes = 256ULL * 1024ULL * 1024ULL;
  // Rust table return modes; other variants continue returning an EventLog.
  // Legacy takes precedence when both are true.
  // The EventLog-only convenience API always retains its legacy contract.
  bool return_legacy_log_object = true;
  bool return_pl_lazyframe = false;
  // Rust legacy table-conversion parameters, separate from native projection
  // selectors. nullopt represents an explicit Python None; raw modes ignore these.
  std::optional<std::string> conversion_case_id_key = "case:concept:name";
  std::optional<std::string> conversion_case_attribute_prefix = "case:";
  // Accepted public conversion switches have no observed effect in RustXES.
  bool conversion_stream_postprocessing = false;
  bool conversion_deepcopy = false;
  bool conversion_include_case_attributes = true;
  bool conversion_compress = false;
  // nullopt infers standard extensions; an empty list suppresses them.
  std::optional<std::vector<LogExtension>> conversion_extensions;
  // Polled while the document is parsed and its traces are built (every few
  // thousand XML tokens); once it answers true the import throws
  // OperationCancelled. Unset: never cancels, and the result is unchanged.
  std::function<bool()> cancelled;
};

struct XesImportResult {
  EventDataKind kind = EventDataKind::kEventLog;
  EventLog log;
  EventTable table;
  // Restartable scanner over eagerly parsed, owned in-memory rows.
  LazyEventTable lazy_table;
  // Public table dtype spellings, aligned with the selected table's columns.
  std::vector<std::string> dtypes;
};

[[nodiscard]] XesImportResult import_xes_result(
    std::istream& input, const XesImportOptions& options = {});
[[nodiscard]] XesImportResult import_xes_result(
    const std::filesystem::path& path, const XesImportOptions& options = {});

[[nodiscard]] EventLog import_xes_event_log(
    std::istream& input,
    const XesImportOptions& options = {});

[[nodiscard]] EventLog import_xes_event_log(
    const std::filesystem::path& path,
    const XesImportOptions& options = {});

// An XES input on pm4py's DataFrame path (pm4py.read_xes returns a frame):
// import_xes_event_log followed by merge_dataframe_cases, so traces that share
// a case id are one case and a trace without events is no case. Routes and
// tools whose reference reads the frame use this; the EventLog importer
// contracts (return_legacy_log_object, the log filter bank) keep
// import_xes_event_log.
[[nodiscard]] EventLog import_xes_dataframe_log(
    std::istream& input, const XesImportOptions& options = {});
[[nodiscard]] EventLog import_xes_dataframe_log(
    const std::filesystem::path& path, const XesImportOptions& options = {});

}  // namespace minera
