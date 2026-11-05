#pragma once

#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "minera/event_log.hpp"

namespace minera {

enum class XesExportVariant {
  kLineByLine,
  kElementTree,
  LINE_BY_LINE = kLineByLine,
  ETREE = kElementTree,
};

[[nodiscard]] XesExportVariant parse_xes_export_variant(
    std::string_view value);
[[nodiscard]] std::string_view xes_export_variant_name(
    XesExportVariant variant);

struct XesExportOptions {
  XesExportVariant variant = XesExportVariant::kLineByLine;
  // Unset selects compression from a .gz output suffix for file exports and
  // plain XML for in-memory serialization.
  std::optional<bool> compress;
  // Empty selects a stored log-level concept:name.  If no such attribute is
  // present, the default output has no log name; set infer_metadata to request
  // the conventional fallback name and standard declarations.
  std::string name;
  bool infer_metadata = false;
  // Opt into Minera null markers; reference exporters omit plain nulls.
  bool preserve_nulls = false;
  // Write the selected case id, activity, timestamp, resource and lifecycle
  // under the standard XES keys (trace concept:name, event concept:name,
  // time:timestamp, org:resource, lifecycle:transition) instead of the keys
  // they were read from, such as the CSV columns case_id or activity. This is
  // what pm4py.format_dataframe followed by write_xes produces; read_xes
  // otherwise yields no case:concept:name column. A trace or event that already has
  // another attribute under the standard key keeps its source key, so no
  // value is lost or overwritten. Off by default: native exports keep the
  // source keys.
  bool standard_keys = false;
};

// Serializes an event log as XES (eXtensible Event Stream, IEEE 1849-2016) —
// the standard interchange format read by ProM and other
// process-mining tools. Each trace carries its case id as `concept:name`;
// each event carries its activity as `concept:name`, its timestamp as
// `time:timestamp`, and — when present — its resource as `org:resource`.
// Timestamps that parse as instants retain their clock fields and offset and
// written as <date> attributes; a non-empty timestamp in an unrecognized
// format is preserved verbatim as a <string> so no data is lost, and an
// empty one is omitted. Explicit date attributes must contain a valid instant
// and are rejected otherwise. Generic event attributes are written under the
// XES type each was imported with (CSV columns as strings), so extra data
// round-trips. An explicit non-empty `name` overrides a stored log name.
void write_xes(std::ostream& out,
               const EventLog& log,
               std::string_view name = {});

[[nodiscard]] std::string to_xes(const EventLog& log,
                                 std::string_view name = {});

// Serializes plain XML or gzip bytes according to the explicit option.
[[nodiscard]] std::string serialize_xes(
    const EventLog& log, const XesExportOptions& options = {});

// Writes binary-safe XES output. Compression defaults from the .gz suffix;
// an explicitly compressed path without that suffix receives one.
[[nodiscard]] std::filesystem::path export_xes_event_log(
    const std::filesystem::path& path, const EventLog& log,
    const XesExportOptions& options = {});

}  // namespace minera
