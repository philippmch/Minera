#pragma once

#include <filesystem>
#include <istream>
#include <string>

#include "minera/event_log.hpp"

namespace minera {

// XES (eXtensible Event Stream, IEEE 1849-2016) is the standard interchange
// format for process-mining event logs. Minera reads the common subset: a
// <log> of <trace> elements, each carrying <event> elements, where leaf
// attribute elements (<string>, <date>, <int>, …) supply the case id,
// activity name, and timestamp via their standard `concept:name` /
// `time:timestamp` keys.
struct XesImportOptions {
  // Trace-level attribute that identifies the case.
  std::string case_attribute = "concept:name";
  // Event-level attribute that names the activity.
  std::string activity_attribute = "concept:name";
  // Event-level attribute that carries the timestamp.
  std::string timestamp_attribute = "time:timestamp";
  // When set, a log whose events never carry the timestamp attribute is
  // rejected instead of imported with empty timestamps.
  bool require_timestamp = false;
};

[[nodiscard]] EventLog import_xes_event_log(
    std::istream& input,
    const XesImportOptions& options = {});

[[nodiscard]] EventLog import_xes_event_log(
    const std::filesystem::path& path,
    const XesImportOptions& options = {});

}  // namespace minera
