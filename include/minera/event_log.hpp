#pragma once

#include <cstddef>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <optional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace minera {

// The XES scalar attribute type an event attribute round-trips as.  The text
// projection remains available in value for compatibility with existing
// consumers; typed_value carries a validated native representation when one
// is available.
enum class AttributeKind { kString, kDate, kInt, kFloat, kBoolean, kId };

enum class AttributeContainerKind { kScalar, kList, kContainer };

enum class FirstClassAttributeState { kMissing, kPresent, kNull };

// Flat adapters attach this role to the value cell that represents a
// first-class event field.  It is transport metadata on the cell itself, so
// adapters do not have to inject collision-prone synthetic columns or mutate
// user metadata to remember custom source-key spellings.
enum class FirstClassAttributeRole {
  kNone,
  kCaseId,
  kActivity,
  kTimestamp,
  kResource,
  kLifecycle,
};

// Dates are kept as a validated, normalized lexical instant.  Keeping a
// distinct wrapper (rather than a bare string) means a date cannot be
// confused with a regular string when an attribute is copied between
// adapters.
struct NativeDate {
  std::string value;

  bool operator==(const NativeDate&) const = default;
};

// Scalar identity follows copies independently of value equality. This is
// needed for alias-sensitive NaN comparisons without sharing heap storage.
// Replace scalars through assignment or emplace to renew that identity.
// Mutating a held value through get/get_if/visit or a Base reference cannot
// renew identity and therefore does not preserve replacement alias semantics.
class NativeAttributeValue
    : public std::variant<std::monostate, std::int64_t, double, bool, NativeDate> {
 public:
  using Base = std::variant<std::monostate, std::int64_t, double, bool, NativeDate>;
  using Base::Base;

  NativeAttributeValue() = default;
  NativeAttributeValue(const NativeAttributeValue&) = default;
  NativeAttributeValue(NativeAttributeValue&&) noexcept = default;
  NativeAttributeValue& operator=(const NativeAttributeValue&) = default;
  NativeAttributeValue& operator=(NativeAttributeValue&&) noexcept = default;

  template <class T>
    requires (!std::is_same_v<std::remove_cvref_t<T>, NativeAttributeValue> &&
              std::is_assignable_v<Base&, T&&>)
  NativeAttributeValue& operator=(T&& value) {
    scalar_identity_ = next_identity();
    Base::operator=(std::forward<T>(value));
    return *this;
  }

  template <class T, class... Args>
  decltype(auto) emplace(Args&&... args) {
    scalar_identity_ = next_identity();
    return Base::template emplace<T>(std::forward<Args>(args)...);
  }
  template <std::size_t Index, class... Args>
  decltype(auto) emplace(Args&&... args) {
    scalar_identity_ = next_identity();
    return Base::template emplace<Index>(std::forward<Args>(args)...);
  }
  template <class T, class U, class... Args>
  decltype(auto) emplace(std::initializer_list<U> values, Args&&... args) {
    scalar_identity_ = next_identity();
    return Base::template emplace<T>(values, std::forward<Args>(args)...);
  }
  template <std::size_t Index, class U, class... Args>
  decltype(auto) emplace(std::initializer_list<U> values, Args&&... args) {
    scalar_identity_ = next_identity();
    return Base::template emplace<Index>(values, std::forward<Args>(args)...);
  }

  void swap(NativeAttributeValue& other) noexcept(noexcept(std::declval<Base&>().swap(std::declval<Base&>()))) {
    Base::swap(other);
    std::swap(scalar_identity_, other.scalar_identity_);
  }
  friend void swap(NativeAttributeValue& left, NativeAttributeValue& right) noexcept(noexcept(left.swap(right))) {
    left.swap(right);
  }
  [[nodiscard]] std::uint64_t scalar_identity() const noexcept { return scalar_identity_; }

  friend bool operator==(const NativeAttributeValue& left, const NativeAttributeValue& right) {
    return static_cast<const Base&>(left) == static_cast<const Base&>(right);
  }

 private:
  static std::uint64_t next_identity() noexcept {
    static std::atomic<std::uint64_t> sequence{1};
    return sequence.fetch_add(1, std::memory_order_relaxed);
  }
  std::uint64_t scalar_identity_ = next_identity();
};

// One generic event attribute beyond the four first-class fields: any extra
// CSV column, or any XES event attribute other than the activity, timestamp,
// and resource keys.
struct EventAttribute {
  std::string key;
  std::string value;
  AttributeKind kind = AttributeKind::kString;
  // A present null differs from a missing attribute and from an empty string.
  // Text formats that cannot represent null values may omit such attributes;
  // the XES adapter uses an explicit null marker.
  bool is_null = false;

  // Numeric, date, and boolean XES values are validated on import and retained
  // here without changing the source-compatible text projection above.  A
  // monostate means that a manually constructed attribute has not supplied a
  // native value; exporters validate its lexical text before writing it.
  NativeAttributeValue typed_value{};
  AttributeContainerKind container = AttributeContainerKind::kScalar;
  // Children are ordered.  Their keys are retained because container values
  // may use either positional keys or meaningful nested names.
  std::vector<EventAttribute> children;

  // Adapter-only role/state data. These fields travel with stream/table cells.
  // XES and analysis JSON omit them from semantic attribute payloads; the
  // lossless conversion JSON exposes them as adapter metadata.
  FirstClassAttributeRole first_class_role = FirstClassAttributeRole::kNone;
  FirstClassAttributeState first_class_state =
      FirstClassAttributeState::kMissing;
  std::string first_class_source_key;

  bool operator==(const EventAttribute& other) const {
    if (key != other.key || value != other.value || kind != other.kind ||
        is_null != other.is_null || container != other.container ||
        children != other.children ||
        first_class_role != other.first_class_role ||
        first_class_state != other.first_class_state ||
        first_class_source_key != other.first_class_source_key)
      return false;
    if (const auto* left = std::get_if<double>(&typed_value)) {
      const auto* right = std::get_if<double>(&other.typed_value);
      if (right && std::isnan(*left) && std::isnan(*right)) return true;
    }
    return typed_value == other.typed_value;
  }
};

struct LogExtension {
  std::string name;
  std::string prefix;
  std::string uri;

  bool operator==(const LogExtension&) const = default;
};

struct LogClassifier {
  std::string name;
  std::vector<std::string> keys;

  bool operator==(const LogClassifier&) const = default;
};

struct LogGlobal {
  std::string scope;
  std::vector<EventAttribute> attributes;

  bool operator==(const LogGlobal&) const = default;
};

// Metadata shared by trace logs, flat streams, and tabular event data.
struct EventDataMetadata {
  std::vector<LogExtension> extensions;
  std::vector<LogClassifier> classifiers;
  std::vector<EventAttribute> trace_globals;
  std::vector<EventAttribute> event_globals;
  std::vector<EventAttribute> properties;
  // Every global declaration in document order, including repeated scopes.
  // trace_globals and event_globals remain compatibility projections formed
  // by concatenating attributes from matching scopes in that same order.
  std::vector<LogGlobal> globals;

  // Importers retain a private synchronization baseline so an adapter can
  // distinguish a deliberate edit to one compatibility projection from an
  // edit to the document-order globals.  These fields are transport state,
  // not additional metadata, and are intentionally omitted from JSON/XES.
  bool compatibility_snapshot_valid = false;
  std::vector<LogGlobal> compatibility_globals_snapshot;
  std::vector<EventAttribute> compatibility_trace_globals_snapshot;
  std::vector<EventAttribute> compatibility_event_globals_snapshot;

  bool operator==(const EventDataMetadata& other) const {
    return extensions == other.extensions && classifiers == other.classifiers &&
           trace_globals == other.trace_globals &&
           event_globals == other.event_globals && properties == other.properties &&
           globals == other.globals;
  }
};

struct Event {
  std::string case_id;
  std::string activity;
  std::string timestamp;
  std::string resource;  // who performed the activity; empty when unknown
  std::size_t input_order = 0;
  std::vector<EventAttribute> attributes;  // in input order
  // XES lifecycle:transition ("start", "complete", …); empty when the log
  // records atomic events. Kept last so aggregate initializers without it
  // stay valid.
  std::string lifecycle;

  // Presence and source spelling for first-class projections.  Existing code
  // may continue to use the string fields; non-empty manually constructed
  // values are treated as present by the helper methods below.
  FirstClassAttributeState case_id_state = FirstClassAttributeState::kMissing;
  FirstClassAttributeState activity_state = FirstClassAttributeState::kMissing;
  FirstClassAttributeState timestamp_state = FirstClassAttributeState::kMissing;
  FirstClassAttributeState resource_state = FirstClassAttributeState::kMissing;
  FirstClassAttributeState lifecycle_state = FirstClassAttributeState::kMissing;
  std::string case_id_source_key;
  std::string activity_source_key;
  std::string timestamp_source_key;
  std::string resource_source_key;
  std::string lifecycle_source_key;

  // The selected XES cells are retained separately from the convenient text
  // projections above.  A first-class selector may point at an int, float,
  // date, id, or boolean attribute, and this metadata is what lets flat
  // adapters and XES export preserve that parent type and its nested
  // children without changing the long-standing string fields.
  std::optional<EventAttribute> activity_attribute;
  std::optional<EventAttribute> timestamp_attribute;
  std::optional<EventAttribute> resource_attribute;
  std::optional<EventAttribute> lifecycle_attribute;

  [[nodiscard]] bool has_case_id() const noexcept {
    return case_id_state != FirstClassAttributeState::kMissing ||
           !case_id.empty();
  }
  [[nodiscard]] bool has_activity() const noexcept {
    return activity_state != FirstClassAttributeState::kMissing ||
           !activity.empty();
  }
  [[nodiscard]] bool has_timestamp() const noexcept {
    return timestamp_state != FirstClassAttributeState::kMissing ||
           !timestamp.empty();
  }
  [[nodiscard]] bool has_resource() const noexcept {
    return resource_state != FirstClassAttributeState::kMissing ||
           !resource.empty();
  }
  [[nodiscard]] bool has_lifecycle() const noexcept {
    return lifecycle_state != FirstClassAttributeState::kMissing ||
           !lifecycle.empty();
  }

  // The value of the named generic attribute, or nullptr when it is missing
  // or explicitly null. Linear, but events carry only a handful of attributes.
  [[nodiscard]] const std::string* attribute(std::string_view key) const {
    for (const auto& entry : attributes) {
      if (entry.key == key) return entry.is_null ? nullptr : &entry.value;
    }
    return nullptr;
  }
};

struct Trace {
  std::string case_id;
  std::vector<Event> events;
  // Case-level attributes beyond the id: any other scalar XES trace attribute,
  // or a `case:`-prefixed CSV column (constant per case, first value wins).
  std::vector<EventAttribute> attributes;
  std::vector<EventAttribute> properties;
  FirstClassAttributeState case_id_state = FirstClassAttributeState::kMissing;
  std::string case_id_source_key;
  // See Event's first-class attribute metadata. This is the selected trace
  // cell before it is projected into case_id.
  std::optional<EventAttribute> case_id_attribute;

  [[nodiscard]] bool has_case_id() const noexcept {
    return case_id_state != FirstClassAttributeState::kMissing ||
           !case_id.empty();
  }

  // The value of the named case attribute, or nullptr when it is missing or
  // explicitly null. Linear, but traces carry only a handful of attributes.
  [[nodiscard]] const std::string* attribute(std::string_view key) const {
    for (const auto& entry : attributes) {
      if (entry.key == key) return entry.is_null ? nullptr : &entry.value;
    }
    return nullptr;
  }
};

// Copy the trace-level context when a transformation rebuilds a trace's event
// vector.  Keeping this operation in one place is important for custom
// selectors: case_id_attribute carries the selected XES/flat cell's native
// kind, nested children, and lexical source key in addition to the convenient
// string projection in case_id.
inline void copy_trace_context(const Trace& source, Trace& target) {
  target.case_id = source.case_id;
  target.attributes = source.attributes;
  target.properties = source.properties;
  target.case_id_state = source.case_id_state;
  target.case_id_source_key = source.case_id_source_key;
  target.case_id_attribute = source.case_id_attribute;
}

// Update a projected trace id and keep its selected first-class attribute in
// sync.  A derived id (for example, a subcase suffix) is no longer guaranteed
// to satisfy the source scalar type, so typed numeric/date/boolean metadata is
// safely represented as a string rather than leaving a stale native payload.
inline void set_trace_case_id(Trace& trace, std::string value) {
  trace.case_id = std::move(value);
  trace.case_id_state = trace.case_id.empty()
                            ? FirstClassAttributeState::kMissing
                            : FirstClassAttributeState::kPresent;
  if (trace.case_id_attribute) {
    auto& selected = *trace.case_id_attribute;
    selected.key = trace.case_id_source_key.empty()
                       ? selected.key
                       : trace.case_id_source_key;
    selected.value = trace.case_id;
    selected.is_null = false;
    selected.container = AttributeContainerKind::kScalar;
    selected.children.clear();
    if (selected.kind == AttributeKind::kDate ||
        selected.kind == AttributeKind::kInt ||
        selected.kind == AttributeKind::kFloat ||
        selected.kind == AttributeKind::kBoolean) {
      selected.kind = AttributeKind::kString;
    }
    selected.typed_value = std::monostate{};
  }
  for (auto& event : trace.events) {
    event.case_id = trace.case_id;
    event.case_id_state = trace.case_id_state;
    event.case_id_source_key = trace.case_id_source_key;
  }
}

// Copy the first-class case projection onto a newly materialized event.  Event
// records repeat the case id for flat consumers, so an inserted/projected event
// must carry the same presence state and source spelling as its trace.
inline void copy_event_case_context(const Trace& source, Event& target) {
  target.case_id = source.case_id;
  target.case_id_state = source.case_id_state;
  target.case_id_source_key = source.case_id_source_key;
}

// Copy the timestamp projection, including selected typed metadata, onto a
// newly materialized event.  The caller remains responsible for choosing the
// source event whose timestamp should be inherited.
inline void copy_event_timestamp_context(const Event& source, Event& target) {
  target.timestamp = source.timestamp;
  target.timestamp_state = source.timestamp_state;
  target.timestamp_source_key = source.timestamp_source_key;
  target.timestamp_attribute = source.timestamp_attribute;
}

// Materialize a model activity while retaining the configured source key.  A
// model label is a new value, so a copied numeric/date/boolean selector cannot
// keep its old native payload; degrade that selected cell to a string payload
// rather than emitting a stale typed value under the custom key.
inline void set_event_activity_context(const Event& source, std::string value,
                                       Event& target) {
  const std::string source_key =
      !source.activity_source_key.empty()
          ? source.activity_source_key
          : (source.activity_attribute ? source.activity_attribute->key
                                        : std::string("concept:name"));
  target.activity = std::move(value);
  target.activity_state = FirstClassAttributeState::kPresent;
  target.activity_source_key = source_key;
  target.activity_attribute.reset();
  if (!source.activity_attribute) return;
  auto selected = *source.activity_attribute;
  selected.key = source_key;
  selected.value = target.activity;
  selected.is_null = false;
  selected.container = AttributeContainerKind::kScalar;
  selected.children.clear();
  if (selected.kind != AttributeKind::kString &&
      selected.kind != AttributeKind::kId) {
    selected.kind = AttributeKind::kString;
  }
  selected.typed_value = std::monostate{};
  target.activity_attribute = std::move(selected);
}

// Orders a trace's events by their parsed timestamp instant (normalized to UTC
// via parse_timestamp), so events recorded with different timezone offsets or
// formats still sort by the moment they actually occurred. Document order
// (input_order) breaks ties. When any event's timestamp is not a recognizable
// instant, the function falls back to a stable lexicographic order so the
// comparison stays a consistent strict weak ordering.
void order_events_by_time(std::vector<Event>& events);

struct EventLog {
  std::vector<Trace> traces;
  // Scalar log-level XES attributes and derived-model projection markers.
  // Kept after traces so existing aggregate initializers that provide only
  // the trace vector remain source-compatible.
  std::vector<EventAttribute> attributes;
  EventDataMetadata metadata;

  [[nodiscard]] const std::string* attribute(std::string_view key) const {
    for (const auto& entry : attributes) {
      if (entry.key == key) return entry.is_null ? nullptr : &entry.value;
    }
    return nullptr;
  }

  [[nodiscard]] std::size_t trace_count() const noexcept {
    return traces.size();
  }

  [[nodiscard]] std::size_t event_count() const noexcept {
    std::size_t count = 0;
    for (const auto& trace : traces) {
      count += trace.events.size();
    }
    return count;
  }
};

inline void copy_event_log_context(const EventLog& source, EventLog& target) {
  target.attributes = source.attributes;
  target.metadata = source.metadata;
}

}  // namespace minera

namespace std {
template <>
struct variant_size<minera::NativeAttributeValue>
    : variant_size<minera::NativeAttributeValue::Base> {};

template <size_t Index>
struct variant_alternative<Index, minera::NativeAttributeValue>
    : variant_alternative<Index, minera::NativeAttributeValue::Base> {};
}  // namespace std
