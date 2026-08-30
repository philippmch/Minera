#pragma once

#include <filesystem>
#include <istream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "minera/event_log.hpp"

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
  // Parser strategy selector. All strategies produce the same native EventLog
  // contract; the pull parser is the shared standards-compliant backend.
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
};

[[nodiscard]] EventLog import_xes_event_log(
    std::istream& input,
    const XesImportOptions& options = {});

[[nodiscard]] EventLog import_xes_event_log(
    const std::filesystem::path& path,
    const XesImportOptions& options = {});

}  // namespace minera
