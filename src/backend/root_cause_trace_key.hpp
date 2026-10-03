#pragma once

#include "minera/event_log.hpp"

#include <string>

namespace minera {

// Native scalar float conversion, including decimal strings and ASCII bytes.
// Null, dates, and containers reject; floating nonfinite values are retained.
double native_attribute_number(const EventAttribute& attribute);
std::optional<EventAttribute> native_event_attribute(const Event& event, const std::string& key);
// Private shared attribute representation used by feature encoding and
// event-value equality. Trace-level values instead retain typed equality.
std::string root_cause_attribute_category(const EventAttribute& attribute);
bool root_cause_traces_equal(const Trace& left, const Trace& right);
// Semantic projections shared by trace equality and the full replay codec.
std::vector<EventAttribute> native_event_attributes(const Event& event);
std::vector<EventAttribute> native_trace_attributes(const Trace& trace);
// Candidate bucket key; callers still confirm matches with trace equality.
std::string root_cause_trace_fingerprint(const Trace& trace);
// Typed scalar, tuple, and frozen-set key comparison. Mutable nested values are rejected;
// cell keys and adapter metadata do not participate in equality.
bool root_cause_scalar_keys_equal(const EventAttribute& left, const EventAttribute& right);
// Validate immutable members and retain the first typed-equal occurrence.
// Input member order controls display; runtime hash iteration is not inferred.
std::vector<const EventAttribute*> root_cause_set_members(const EventAttribute& value);
bool root_cause_key_is_hashable(const EventAttribute& value);
std::string root_cause_scalar_key_fingerprint(const EventAttribute& value);
enum class NativeScalarOrderKind { kUnsupported, kString, kNumeric, kBytes, kDate };
NativeScalarOrderKind native_scalar_order_kind(const EventAttribute& value);
// Compare supported scalar values without rounding integer values to doubles.
// Incompatible scalar types reject; containers have no ordering here.
int native_scalar_key_compare(const EventAttribute& left, const EventAttribute& right);

}  // namespace minera
