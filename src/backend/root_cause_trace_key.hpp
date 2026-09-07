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
// Typed scalar key comparison. Nested values are unhashable and rejected;
// cell keys and adapter metadata do not participate in equality.
bool root_cause_scalar_keys_equal(const EventAttribute& left, const EventAttribute& right);
std::string root_cause_scalar_key_fingerprint(const EventAttribute& value);

}  // namespace minera
