#pragma once

#include <ostream>
#include <string>
#include <string_view>

#include "minera/event_log.hpp"

namespace minera {

// Serializes an event log as XES (eXtensible Event Stream, IEEE 1849-2016) —
// the standard interchange format read by ProM, PM4Py, and other
// process-mining tools. Each trace carries its case id as `concept:name`;
// each event carries its activity as `concept:name`, its timestamp as
// `time:timestamp`, and — when present — its resource as `org:resource`.
// Timestamps that parse as instants are normalized to ISO-8601 UTC and
// written as <date> attributes; a non-empty timestamp in an unrecognized
// format is preserved verbatim as a <string> so no data is lost, and an
// empty one is omitted. Generic event attributes are written under the XES
// type each was imported with (CSV columns as strings), so extra data
// round-trips. `name` becomes the log's own `concept:name`.
void write_xes(std::ostream& out,
               const EventLog& log,
               std::string_view name = "log");

[[nodiscard]] std::string to_xes(const EventLog& log,
                                 std::string_view name = "log");

}  // namespace minera
