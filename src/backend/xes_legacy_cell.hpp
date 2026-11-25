#pragma once

// pm4py's legacy event objects of an XES upload (read_xes with
// return_legacy_log_object, the hybrid log /api/mine's log shape reads):
// event[key] as the iterparse importer stores each XES attribute. Shared by
// Declare (round HE) and the other families keyed by an attribute on that
// shape (round HF).

#include "minera/event_log.hpp"
#include "minera/timestamp.hpp"
#include "python_float_text.hpp"
#include "python_scalar.hpp"

#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace minera {

// event[key] of pm4py's legacy event dict, as the iterparse importer stores
// an XES attribute (round HE, observed black-box; the classic heuristics'
// reading of rounds GK and GL): a string or id a str, an int an int of any
// width, a float float() of its text (each NaN its own object), a boolean
// True where its text is "true" in any case, a date a datetime.datetime
// whose wall clock is kept and marked UTC (fix_naivety drops the offset; the
// microseconds of a finer fraction), a valueless string or a <list> without
// children None, a <list> with children (or a scalar with children) a dict;
// a <container> is not stored. kUnmodelled where Minera cannot tell.
struct LegacyDeclareCell {
  enum class State { kValue, kAbsent, kDict, kUnmodelled };
  State state = State::kValue;
  python::Scalar value;
  std::string type;
};

inline LegacyDeclareCell legacy_declare_attribute(const EventAttribute& attribute) {
  using State = LegacyDeclareCell::State;
  LegacyDeclareCell out;
  const auto none = [&] {
    out.value = python::none();
    out.type = "NoneType";
    return out;
  };
  if (attribute.container != AttributeContainerKind::kScalar || attribute.scalar_wrapper ||
      !attribute.children.empty()) {
    if (attribute.xes_public_shape == XesPublicShape::kNone) return none();
    if (attribute.xes_public_shape == XesPublicShape::kAbsent) out.state = State::kAbsent;
    else if (attribute.xes_public_shape == XesPublicShape::kWrapper || attribute.scalar_wrapper) out.state = State::kDict;
    else out.state = State::kUnmodelled;
    return out;
  }
  if (attribute.is_null || is_native_na_attribute(attribute)) return none();
  switch (attribute.kind) {
    case AttributeKind::kString:
    case AttributeKind::kId:
      out.value = python::text(attribute.value);
      out.type = "str";
      return out;
    case AttributeKind::kInt:
      if (const auto* i = std::get_if<std::int64_t>(&attribute.typed_value)) out.value = python::integer(*i);
      else if (const auto* u = std::get_if<std::uint64_t>(&attribute.typed_value)) out.value = python::unsigned_integer(*u);
      else if (const auto parsed = python::integer_literal(attribute.value)) out.value = *parsed;
      else {
        out.state = State::kUnmodelled;
        return out;
      }
      out.type = "int";
      return out;
    case AttributeKind::kFloat:
      if (const auto* d = std::get_if<double>(&attribute.typed_value)) out.value = python::real(*d);
      else if (const auto parsed = detail::parse_python_float_text(attribute.value)) out.value = python::real(*parsed);
      else {
        out.state = State::kUnmodelled;
        return out;
      }
      out.type = "float";
      return out;
    case AttributeKind::kBoolean: {
      bool truth = false;
      if (const auto* b = std::get_if<bool>(&attribute.typed_value)) truth = *b;
      else {
        std::string lower = attribute.value;
        for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        truth = lower == "true";
      }
      out.value = python::boolean(truth);
      out.type = "bool";
      return out;
    }
    case AttributeKind::kDate: {
      auto parsed = parse_timestamp_exact(attribute.value);
      if (!parsed) {
        out.state = State::kUnmodelled;
        return out;
      }
      if (parsed->timezone_aware) parsed->seconds += static_cast<std::int64_t>(parsed->offset_minutes) * 60;
      parsed->offset_minutes = 0;
      parsed->timezone_aware = true;
      parsed->nanoseconds -= parsed->nanoseconds % 1000;
      out.value.kind = python::Scalar::Kind::kDate;
      out.value.date = *parsed;
      out.value.text = attribute.value;
      out.type = "datetime.datetime";
      return out;
    }
    default: out.state = State::kUnmodelled; return out;
  }
}

inline LegacyDeclareCell legacy_declare_cell(const Event& event, const std::string& key) {
  using State = LegacyDeclareCell::State;
  const auto key_of = [](const std::string& source, std::string_view standard) {
    return source.empty() ? std::string(standard) : source;
  };
  const auto attribute_value = [&] {
    for (const auto& attribute : event.attributes)
      if (attribute.key == key) return legacy_declare_attribute(attribute);
    LegacyDeclareCell out;
    out.state = State::kAbsent;
    return out;
  };
  // A first-class key the event holds as no scalar stays among its attributes.
  const auto field = [&](bool present, FirstClassAttributeState state, const std::optional<EventAttribute>& attribute,
                         const std::string& value, bool date) {
    if (!present) return attribute_value();
    if (attribute) return legacy_declare_attribute(*attribute);
    LegacyDeclareCell out;
    if (state == FirstClassAttributeState::kNull) {
      out.value = python::none();
      out.type = "NoneType";
      return out;
    }
    EventAttribute cell{key, value, date ? AttributeKind::kDate : AttributeKind::kString};
    return legacy_declare_attribute(cell);
  };
  if (key == key_of(event.timestamp_source_key, "time:timestamp"))
    return field(event.has_timestamp(), event.timestamp_state, event.timestamp_attribute, event.timestamp, true);
  if (key == key_of(event.activity_source_key, "concept:name"))
    return field(event.has_activity(), event.activity_state, event.activity_attribute, event.activity, false);
  if (key == key_of(event.resource_source_key, "org:resource"))
    return field(event.has_resource(), event.resource_state, event.resource_attribute, event.resource, false);
  if (key == key_of(event.lifecycle_source_key, "lifecycle:transition"))
    return field(event.has_lifecycle(), event.lifecycle_state, event.lifecycle_attribute, event.lifecycle, false);
  return attribute_value();
}

}  // namespace minera
