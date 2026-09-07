#pragma once

#include "minera/event_log.hpp"

#include <string>

namespace minera {

// Private shared attribute representation used by feature encoding and
// event-value equality. Trace-level values instead retain typed equality.
std::string root_cause_attribute_category(const EventAttribute& attribute);
bool root_cause_traces_equal(const Trace& left, const Trace& right);
// Candidate bucket key; callers still confirm matches with trace equality.
std::string root_cause_trace_fingerprint(const Trace& trace);

}  // namespace minera
