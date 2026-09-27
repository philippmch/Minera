#include "minera/edit_distance_alignment.hpp"

#include "minera/dfg_variants.hpp"
#include "minera/log_conversion.hpp"
#include "root_cause_trace_key.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace minera {
namespace {

using Variant = std::vector<std::string>;

void require_direct_hashable_activity(const EventAttribute& value);

struct TargetVariant {
  Variant activities;
  std::size_t first_index = 0;
};

void validate_selector(std::string_view selector, std::string_view role) {
  if (selector.empty() || std::all_of(selector.begin(), selector.end(),
                                      [](unsigned char c) {
                                        return std::isspace(c) != 0;
                                      })) {
    throw std::invalid_argument("edit-distance " + std::string(role) +
                                " selector must not be blank");
  }
}

const std::string& required_activity(const Event& event,
                                     std::string_view selector) {
  const auto* value = dfg_event_field_value(event, selector);
  if (value == nullptr) {
    throw std::invalid_argument(
        "edit-distance: missing or null activity " + std::string(selector));
  }
  return *value;
}

const EventAttribute* selected_attribute(const Event& event,
                                         std::string_view selector) {
  if (event.activity_attribute && !event.activity_attribute->is_null &&
      (selector == "concept:name" || selector == "activity" ||
       selector == event.activity_source_key ||
       selector == event.activity_attribute->key))
    return &*event.activity_attribute;
  if (event.timestamp_attribute && !event.timestamp_attribute->is_null &&
      (selector == "time:timestamp" || selector == "timestamp" ||
       selector == event.timestamp_source_key ||
       selector == event.timestamp_attribute->key))
    return &*event.timestamp_attribute;
  if (event.resource_attribute && !event.resource_attribute->is_null &&
      (selector == "org:resource" || selector == "resource" ||
       selector == event.resource_source_key ||
       selector == event.resource_attribute->key))
    return &*event.resource_attribute;
  if (event.lifecycle_attribute && !event.lifecycle_attribute->is_null &&
      (selector == "lifecycle:transition" || selector == "lifecycle" ||
       selector == event.lifecycle_source_key ||
       selector == event.lifecycle_attribute->key))
    return &*event.lifecycle_attribute;
  for (const auto& attribute : event.attributes)
    if (attribute.key == selector && !attribute.is_null) return &attribute;
  return nullptr;
}

std::vector<std::string> activities_of(const Trace& trace,
                                       std::string_view selector) {
  Variant result;
  result.reserve(trace.events.size());
  for (const auto& event : trace.events)
    result.push_back(required_activity(event, selector));
  return result;
}

std::size_t levenshtein(const Variant& a, const Variant& b) {
  std::vector<std::size_t> previous(b.size() + 1), current(b.size() + 1);
  for (std::size_t j = 0; j <= b.size(); ++j) previous[j] = j;
  for (std::size_t i = 1; i <= a.size(); ++i) {
    current[0] = i;
    for (std::size_t j = 1; j <= b.size(); ++j) {
      const std::size_t substitution = previous[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1);
      current[j] = std::min({previous[j] + 1, current[j - 1] + 1, substitution});
    }
    previous.swap(current);
  }
  return previous[b.size()];
}

// difflib.SequenceMatcher's matching blocks are the recursively selected
// longest *contiguous* matches, breaking ties by earliest source then target
// position. Its default autojunk mode suppresses popular target symbols when
// the target has at least 200 elements (the >1%+1 threshold). A sorted map is
// used instead of Python's hash table, making b2j iteration deterministic.
struct Block { std::size_t a = 0, b = 0, n = 0; };

Block find_longest(const Variant& a, std::size_t alo, std::size_t ahi,
                   const Variant& b, std::size_t blo, std::size_t bhi) {
  std::map<std::string, std::vector<std::size_t>> b2j;
  std::map<std::string, std::size_t> counts;
  for (std::size_t j = 0; j < b.size(); ++j) {
    b2j[b[j]].push_back(j);
    ++counts[b[j]];
  }
  if (b.size() >= 200) {
    const std::size_t popularity = b.size() / 100 + 1;
    for (auto it = b2j.begin(); it != b2j.end();) {
      if (counts[it->first] > popularity) it = b2j.erase(it);
      else ++it;
    }
  }
  std::size_t best_a = alo, best_b = blo, best_n = 0;
  std::map<std::size_t, std::size_t> previous;
  for (std::size_t i = alo; i < ahi; ++i) {
    std::map<std::size_t, std::size_t> current;
    const auto indexed = b2j.find(a[i]);
    if (indexed != b2j.end()) {
      for (const std::size_t j : indexed->second) {
        if (j < blo || j >= bhi) continue;
        const auto prior = previous.find(j - (j > 0 ? 1 : 0));
        const std::size_t n = (j > 0 && prior != previous.end() && prior->first + 1 == j)
                                  ? prior->second + 1 : 1;
        current[j] = n;
        if (n > best_n || (n == best_n && n != 0 &&
                           (i - n + 1 < best_a ||
                            (i - n + 1 == best_a && j - n + 1 < best_b)))) {
          best_a = i - n + 1; best_b = j - n + 1; best_n = n;
        }
      }
    }
    previous.swap(current);
  }
  // Extend a popular-element match around the non-popular core, as
  // SequenceMatcher does after its b2j scan.
  while (best_a > alo && best_b > blo && a[best_a - 1] == b[best_b - 1])
    --best_a, --best_b, ++best_n;
  while (best_a + best_n < ahi && best_b + best_n < bhi &&
         a[best_a + best_n] == b[best_b + best_n]) ++best_n;
  return {best_a, best_b, best_n};
}

void matching_blocks(const Variant& a, std::size_t alo, std::size_t ahi,
                     const Variant& b, std::size_t blo, std::size_t bhi,
                     std::vector<Block>& out) {
  struct Work { std::size_t alo, ahi, blo, bhi; bool emit; Block block; };
  std::vector<Work> stack{{alo, ahi, blo, bhi, false, {}}};
  while (!stack.empty()) {
    Work work = stack.back();
    stack.pop_back();
    if (work.emit) { out.push_back(work.block); continue; }
    const Block block = find_longest(a, work.alo, work.ahi, b, work.blo, work.bhi);
    if (block.n == 0) continue;
    stack.push_back({block.a + block.n, work.ahi, block.b + block.n, work.bhi, false, {}});
    stack.push_back({0, 0, 0, 0, true, block});
    stack.push_back({work.alo, block.a, work.blo, block.b, false, {}});
  }
  std::vector<Block> collapsed;
  for (const auto& block : out) {
    if (!collapsed.empty() && collapsed.back().a + collapsed.back().n == block.a &&
        collapsed.back().b + collapsed.back().n == block.b)
      collapsed.back().n += block.n;
    else collapsed.push_back(block);
  }
  out.swap(collapsed);
}

std::vector<EditDistanceMove> make_moves(const Variant& source,
                                         const Variant& target) {
  if (source == target) {
    std::vector<EditDistanceMove> exact;
    for (const auto& activity : source) {
      EditDistanceMove move;
      move.type = EditDistanceMoveType::kSynchronous;
      move.log_activity = move.model_activity = move.activity = activity;
      move.log = move.model = activity;
      exact.push_back(std::move(move));
    }
    return exact;
  }
  std::vector<Block> blocks;
  matching_blocks(source, 0, source.size(), target, 0, target.size(), blocks);
  std::vector<EditDistanceMove> moves;
  std::size_t i = 0, j = 0;
  auto log_move = [&](const std::string& activity) {
    EditDistanceMove move;
    move.type = EditDistanceMoveType::kLog;
    move.log = activity;
    move.log_activity = activity;
    move.activity = activity;
    moves.push_back(std::move(move));
  };
  auto model_move = [&](const std::string& activity) {
    EditDistanceMove move;
    move.type = EditDistanceMoveType::kModel;
    move.model = activity;
    move.model_activity = activity;
    move.activity = activity;
    moves.push_back(std::move(move));
  };
  for (const auto& block : blocks) {
    while (i < block.a) log_move(source[i++]);
    while (j < block.b) model_move(target[j++]);
    for (std::size_t k = 0; k < block.n; ++k) {
      EditDistanceMove move;
      move.type = EditDistanceMoveType::kSynchronous;
      move.log = source[i];
      move.model = target[j];
      move.log_activity = source[i];
      move.model_activity = target[j];
      move.activity = source[i];
      moves.push_back(std::move(move));
      ++i; ++j;
    }
  }
  while (i < source.size()) log_move(source[i++]);
  while (j < target.size()) model_move(target[j++]);
  return moves;
}

struct SelectedTimestamp {
  std::string value;
  FirstClassAttributeState state = FirstClassAttributeState::kMissing;
  std::string source_key;
  std::optional<EventAttribute> attribute;
};

SelectedTimestamp timestamp_value(const Event& event,
                                 std::string_view selector,
                                 bool allow_conventional_missing = false) {
  const auto* value = dfg_event_field_value(event, selector);
  if (value == nullptr) {
    if (allow_conventional_missing &&
        (selector == "time:timestamp" || selector == "timestamp") &&
        event.timestamp_state == FirstClassAttributeState::kMissing &&
        event.timestamp.empty() && !event.timestamp_attribute) {
      SelectedTimestamp result;
      result.source_key = event.timestamp_source_key;
      result.state = event.timestamp_state;
      return result;
    }
    throw std::invalid_argument("edit-distance: missing or null timestamp " +
                                std::string(selector));
  }
  SelectedTimestamp result;
  result.value = *value;
  if (const auto* attribute = selected_attribute(event, selector)) {
    result.attribute = *attribute;
    result.source_key = attribute->key;
    result.state = FirstClassAttributeState::kPresent;
  } else {
    result.source_key = event.timestamp_source_key.empty()
                            ? std::string(selector)
                            : event.timestamp_source_key;
    result.state = event.timestamp_state == FirstClassAttributeState::kMissing
                       ? FirstClassAttributeState::kPresent
                       : event.timestamp_state;
  }
  return result;
}

SelectedTimestamp public_timestamp_value(const Event& event,
                                         std::string_view selector) {
  const bool conventional_timestamp =
      selector == "time:timestamp" || selector == "timestamp";
  const bool first_class_timestamp =
      conventional_timestamp || event.timestamp_source_key == selector ||
      (event.timestamp_attribute &&
       event.timestamp_attribute->key == selector);

  const EventAttribute* selected = nullptr;
  if (first_class_timestamp && event.timestamp_attribute &&
      (conventional_timestamp ||
       event.timestamp_attribute->key == selector ||
       event.timestamp_source_key == selector)) {
    selected = &*event.timestamp_attribute;
  }
  if (selected == nullptr) {
    const auto found = std::find_if(
        event.attributes.begin(), event.attributes.end(),
        [&](const EventAttribute& attribute) {
          return attribute.key == selector;
        });
    if (found != event.attributes.end()) selected = &*found;
  }

  if (selected != nullptr) {
    SelectedTimestamp result;
    result.value = selected->value;
    result.source_key = selector;
    result.state = (selected->is_null ||
                    selected->first_class_state ==
                        FirstClassAttributeState::kNull ||
                    (first_class_timestamp &&
                     event.timestamp_state ==
                         FirstClassAttributeState::kNull))
                       ? FirstClassAttributeState::kNull
                       : FirstClassAttributeState::kPresent;
    result.attribute = *selected;
    result.attribute->key = selector;
    if (result.state == FirstClassAttributeState::kNull)
      result.attribute->is_null = true;
    return result;
  }

  if (first_class_timestamp &&
      (event.timestamp_state != FirstClassAttributeState::kMissing ||
       !event.timestamp.empty())) {
    SelectedTimestamp result;
    result.value = event.timestamp;
    result.state = event.timestamp_state == FirstClassAttributeState::kMissing
                       ? FirstClassAttributeState::kPresent
                       : event.timestamp_state;
    result.source_key = std::string(selector);
    if (result.state == FirstClassAttributeState::kNull) {
      result.attribute = EventAttribute{
          std::string(selector), "", AttributeKind::kDate, true};
    }
    return result;
  }

  throw std::invalid_argument("edit-distance: missing timestamp " +
                              std::string(selector));
}

EventAttribute projected_activity_attribute(const EventAttribute& source,
                                             std::string_view key,
                                             std::string_view value) {
  auto result = source;
  result.key = key.empty() ? source.key : std::string(key);
  result.value = value;
  result.is_null = false;
  result.container = AttributeContainerKind::kScalar;
  result.children.clear();
  result.scalar_wrapper = false;
  result.scalar_children_list = false;
  if (result.kind != AttributeKind::kString && result.kind != AttributeKind::kId)
    result.kind = AttributeKind::kString;
  result.typed_value = std::monostate{};
  return result;
}

void replace_selected_attribute(std::vector<EventAttribute>& attributes,
                                const EventAttribute& selected) {
  attributes.erase(
      std::remove_if(attributes.begin(), attributes.end(),
                     [&](const EventAttribute& attribute) {
                       return attribute.key == selected.key;
                     }),
      attributes.end());
  attributes.push_back(selected);
}

void set_projected_activity(Event& target, const Event* source,
                            std::string_view selector,
                            std::string_view value) {
  target.activity = value;
  target.activity_state = FirstClassAttributeState::kPresent;
  if (source != nullptr) {
    if (const auto* attribute = selected_attribute(*source, selector)) {
      target.activity_source_key = attribute->key;
      target.activity_attribute = projected_activity_attribute(
          *attribute, attribute->key, value);
      replace_selected_attribute(target.attributes, *target.activity_attribute);
      return;
    }
    if ((selector == "concept:name" || selector == "activity") &&
        !source->activity_source_key.empty()) {
      target.activity_source_key = source->activity_source_key;
      if (source->activity_attribute)
        target.activity_attribute = projected_activity_attribute(
            *source->activity_attribute, target.activity_source_key, value);
      else
        target.activity_attribute.reset();
      if (target.activity_attribute)
        replace_selected_attribute(target.attributes, *target.activity_attribute);
      return;
    }
  }
  target.activity_source_key = std::string(selector);
  target.activity_attribute.reset();
}

// Synchronous events retain the source attribute bag exactly.  Only the
// convenience projection is rebound to the selected value/context; typed
// metadata and attribute order are copied verbatim from the source event.
void bind_selected_activity(Event& target, const Event& source,
                            std::string_view selector,
                            std::string_view value) {
  target.activity = value;
  target.activity_state = FirstClassAttributeState::kPresent;
  if (const auto* attribute = selected_attribute(source, selector)) {
    target.activity_source_key = attribute->key;
    target.activity_attribute = *attribute;
  } else {
    target.activity_source_key = source.activity_source_key;
    target.activity_attribute = source.activity_attribute;
  }
}

void set_projected_timestamp(Event& target, const SelectedTimestamp& timestamp) {
  target.timestamp = timestamp.value;
  target.timestamp_state = timestamp.state;
  target.timestamp_source_key = timestamp.source_key;
  target.timestamp_attribute = timestamp.attribute;
  if (target.timestamp_attribute)
    replace_selected_attribute(target.attributes, *target.timestamp_attribute);
}

void bind_selected_timestamp(Event& target,
                             const SelectedTimestamp& timestamp) {
  target.timestamp = timestamp.value;
  target.timestamp_state = timestamp.state;
  target.timestamp_source_key = timestamp.source_key;
  target.timestamp_attribute = timestamp.attribute;
}

void set_synthetic_activity(Event& event, std::string_view selector,
                            std::string_view value) {
  if (selector == "concept:name" || selector == "activity") {
    event.activity = value;
    event.activity_state = FirstClassAttributeState::kPresent;
    event.activity_source_key = std::string(selector);
  } else if (selector == "time:timestamp" || selector == "timestamp") {
    event.timestamp = value;
    event.timestamp_state = FirstClassAttributeState::kPresent;
    event.timestamp_source_key = std::string(selector);
  } else if (selector == "org:resource" || selector == "resource") {
    event.resource = value;
    event.resource_state = FirstClassAttributeState::kPresent;
    event.resource_source_key = std::string(selector);
  } else if (selector == "lifecycle:transition" || selector == "lifecycle") {
    event.lifecycle = value;
    event.lifecycle_state = FirstClassAttributeState::kPresent;
    event.lifecycle_source_key = std::string(selector);
  } else if (selector == "case:concept:name" || selector == "case_id" ||
             selector == "case") {
    event.case_id = value;
    event.case_id_state = FirstClassAttributeState::kPresent;
    event.case_id_source_key = std::string(selector);
  } else {
    event.attributes.push_back(
        {std::string(selector), std::string(value), AttributeKind::kString});
  }
}

std::optional<EventAttribute> normalized_selected_activity(
    const EventAttribute& value) {
  if (value.first_class_role != FirstClassAttributeRole::kNone &&
      value.first_class_state == FirstClassAttributeState::kMissing)
    return std::nullopt;
  EventAttribute result = value;
  if (result.first_class_state == FirstClassAttributeState::kNull)
    result.is_null = true;
  return result;
}

std::optional<EventAttribute> first_class_activity_value(
    const std::optional<EventAttribute>& stored, std::string_view source_key,
    FirstClassAttributeState state, const std::string& text,
    FirstClassAttributeRole role, std::string_view canonical_key,
    std::string_view selector) {
  const bool source_match = !source_key.empty() && source_key == selector;
  const bool stored_match = stored &&
      (stored->key == selector || stored->first_class_source_key == selector);
  const bool implicit_canonical = !stored && source_key.empty() &&
                                  selector == canonical_key;
  if (!source_match && !stored_match && !implicit_canonical)
    return std::nullopt;

  if (stored) {
    if (auto selected = normalized_selected_activity(*stored)) {
      // The slot that owns this attribute is authoritative when adapters
      // omitted the transport role metadata. Enrich the copy so typed moves
      // retain that first-class identity without changing the input log.
      if (selected->first_class_role == FirstClassAttributeRole::kNone) {
        selected->first_class_role = role;
        const bool is_null = selected->is_null ||
            selected->first_class_state == FirstClassAttributeState::kNull ||
            state == FirstClassAttributeState::kNull;
        selected->is_null = is_null;
        selected->first_class_state =
            is_null ? FirstClassAttributeState::kNull
                    : (state == FirstClassAttributeState::kMissing
                           ? FirstClassAttributeState::kPresent
                           : state);
        selected->first_class_source_key = source_key.empty()
                                               ? std::string(selector)
                                               : std::string(source_key);
      }
      return selected;
    }
    return std::nullopt;
  }
  if (state == FirstClassAttributeState::kMissing && text.empty())
    return std::nullopt;

  EventAttribute value{std::string(selector), text, AttributeKind::kString};
  value.first_class_role = role;
  value.first_class_state = state == FirstClassAttributeState::kMissing
                                ? FirstClassAttributeState::kPresent
                                : state;
  value.first_class_source_key = std::string(source_key);
  value.is_null = state == FirstClassAttributeState::kNull;
  return value;
}

std::optional<EventAttribute> selected_typed_activity(
    const Event& event, std::string_view selector) {
  const auto decorate_generic_selection = [&](EventAttribute selected) {
    if (selected.first_class_role != FirstClassAttributeRole::kNone)
      return selected;

    FirstClassAttributeRole role = FirstClassAttributeRole::kNone;
    std::string source_key;
    const auto matches_slot = [&](
        const std::optional<EventAttribute>& stored,
        const std::string& stored_source_key, std::string_view canonical,
        std::string_view alias) {
      return selector == canonical || selector == alias ||
             stored_source_key == selector ||
             (stored && (stored->key == selector ||
                         stored->first_class_source_key == selector));
    };
    if (matches_slot(event.activity_attribute, event.activity_source_key,
                     "concept:name", "activity")) {
      role = FirstClassAttributeRole::kActivity;
      source_key = event.activity_source_key;
    } else if (matches_slot(event.timestamp_attribute,
                            event.timestamp_source_key, "time:timestamp",
                            "timestamp")) {
      role = FirstClassAttributeRole::kTimestamp;
      source_key = event.timestamp_source_key;
    } else if (matches_slot(event.resource_attribute,
                            event.resource_source_key, "org:resource",
                            "resource")) {
      role = FirstClassAttributeRole::kResource;
      source_key = event.resource_source_key;
    } else if (matches_slot(event.lifecycle_attribute,
                            event.lifecycle_source_key,
                            "lifecycle:transition", "lifecycle")) {
      role = FirstClassAttributeRole::kLifecycle;
      source_key = event.lifecycle_source_key;
    }
    if (role != FirstClassAttributeRole::kNone) {
      selected.first_class_role = role;
      selected.first_class_state =
          selected.is_null || selected.first_class_state ==
                                  FirstClassAttributeState::kNull
              ? FirstClassAttributeState::kNull
              : FirstClassAttributeState::kPresent;
      selected.first_class_source_key = source_key.empty()
                                            ? std::string(selector)
                                            : std::move(source_key);
    }
    return selected;
  };

  // Generic attributes are exact mapping keys and take precedence over the
  // native first-class projections. Present nulls remain values here.
  for (const auto& attribute : event.attributes) {
    if (attribute.key != selector) continue;
    if (auto selected = normalized_selected_activity(attribute))
      return decorate_generic_selection(std::move(*selected));
  }

  if (auto selected = first_class_activity_value(
          event.activity_attribute, event.activity_source_key,
          event.activity_state, event.activity, FirstClassAttributeRole::kActivity,
          "concept:name", selector))
    return selected;
  if (auto selected = first_class_activity_value(
          event.timestamp_attribute, event.timestamp_source_key,
          event.timestamp_state, event.timestamp,
          FirstClassAttributeRole::kTimestamp, "time:timestamp", selector))
    return selected;
  if (auto selected = first_class_activity_value(
          event.resource_attribute, event.resource_source_key,
          event.resource_state, event.resource, FirstClassAttributeRole::kResource,
          "org:resource", selector))
    return selected;
  if (auto selected = first_class_activity_value(
          event.lifecycle_attribute, event.lifecycle_source_key,
          event.lifecycle_state, event.lifecycle,
          FirstClassAttributeRole::kLifecycle, "lifecycle:transition", selector))
    return selected;
  return std::nullopt;
}

class TypedActivityInterner {
 public:
  std::string intern(const EventAttribute& activity) {
    require_direct_hashable_activity(activity);
    const std::string fingerprint =
        root_cause_scalar_key_fingerprint(activity);
    auto& bucket = buckets_[fingerprint];
    for (const std::size_t index : bucket) {
      if (root_cause_scalar_keys_equal(values_[index], activity))
        return label(index);
    }
    const std::size_t index = values_.size();
    values_.push_back(activity);
    bucket.push_back(index);
    labels_.emplace(label(index), index);
    return label(index);
  }

  const EventAttribute& value(std::string_view label_value) const {
    const auto found = labels_.find(std::string(label_value));
    if (found == labels_.end())
      throw std::logic_error("edit-distance typed activity token is unknown");
    return values_[found->second];
  }

 private:
  static std::string label(std::size_t index) {
    return "__minera_typed_activity_" + std::to_string(index);
  }

  std::vector<EventAttribute> values_;
  std::map<std::string, std::vector<std::size_t>> buckets_;
  std::map<std::string, std::size_t> labels_;
};

EventLog intern_typed_activities(const EventLog& source,
                                std::string_view selector,
                                TypedActivityInterner& interner) {
  EventLog encoded;
  encoded.traces.reserve(source.traces.size());
  for (const auto& trace : source.traces) {
    Trace encoded_trace;
    encoded_trace.case_id = trace.case_id;
    encoded_trace.events.reserve(trace.events.size());
    for (const auto& source_event : trace.events) {
      const auto activity = selected_typed_activity(source_event, selector);
      if (!activity)
        throw std::out_of_range("'" + std::string(selector) + "'");
      const std::string id = interner.intern(*activity);
      Event encoded_event;
      set_synthetic_activity(encoded_event, selector, id);
      encoded_trace.events.push_back(std::move(encoded_event));
    }
    encoded.traces.push_back(std::move(encoded_trace));
  }
  return encoded;
}

std::vector<EventAttribute> decode_typed_variant(
    const Variant& encoded, const TypedActivityInterner& interner) {
  std::vector<EventAttribute> result;
  result.reserve(encoded.size());
  for (const auto& label : encoded) result.push_back(interner.value(label));
  return result;
}

EditDistanceTypedMove decode_typed_alignment_move(
    const EditDistanceMove& encoded, const TypedActivityInterner& interner) {
  EditDistanceTypedMove result;
  result.type = encoded.type;
  if (encoded.type == EditDistanceMoveType::kSynchronous) {
    result.log_activity = interner.value(encoded.log_activity);
    result.model_activity = interner.value(encoded.model_activity);
  } else if (encoded.type == EditDistanceMoveType::kLog) {
    result.log_activity = interner.value(encoded.log_activity);
  } else {
    result.model_activity = interner.value(encoded.model_activity);
  }
  return result;
}

std::string join_variant(const Variant& v) {
  std::string result;
  for (std::size_t i = 0; i < v.size(); ++i) {
    if (i) result += ',';
    result += v[i];
  }
  return result;
}

void set_attr(std::vector<EventAttribute>& attrs, std::string key,
              std::string value, AttributeKind kind = AttributeKind::kString) {
  auto first = attrs.end();
  for (auto it = attrs.begin(); it != attrs.end();) {
    if (it->key != key) {
      ++it;
    } else if (first == attrs.end()) {
      first = it++;
    } else {
      it = attrs.erase(it);
    }
  }
  if (first != attrs.end()) {
    first->value = std::move(value);
    first->is_null = false;
    first->kind = kind;
  } else {
    attrs.push_back({std::move(key), std::move(value), kind});
  }
}

void erase_attr(std::vector<EventAttribute>& attrs, std::string_view key) {
  attrs.erase(std::remove_if(attrs.begin(), attrs.end(),
                             [&](const EventAttribute& attr) {
                               return attr.key == key;
                             }),
              attrs.end());
}

EventLog event_data_as_log(const EventDataView& data,
                           const EditDistanceOptions& options) {
  validate_selector(options.case_id_attribute, "case id");
  LogConversionOptions conversion;
  conversion.case_id_key = options.case_id_attribute;
  return event_data_to_event_log(data, conversion);
}

const std::vector<std::string>* table_columns(const EventDataView& data) {
  if (data.kind() == EventDataKind::kEventTable)
    return &data.table()->columns;
  if (data.kind() == EventDataKind::kLazyEventTable)
    return &data.lazy_table()->columns();
  return nullptr;
}

void require_table_column(const EventDataView& data, const EventLog& log,
                          std::string_view selector,
                          std::string_view role) {
  if (log.event_count() == 0) return;
  const auto* columns = table_columns(data);
  if (columns == nullptr ||
      std::find(columns->begin(), columns->end(), selector) != columns->end())
    return;
  throw std::invalid_argument("edit-distance: missing " +
                              std::string(role) + " column " +
                              std::string(selector));
}

void require_flat_alignment_columns(const EventDataView& data,
                                    const EventLog& log,
                                    const EditDistanceOptions& options) {
  if (data.kind() == EventDataKind::kEventLog || log.event_count() == 0)
    return;
  require_table_column(data, log, options.case_id_attribute, "case id");
  require_table_column(data, log, options.activity_attribute, "activity");
}

void require_flat_projection_fields(const EventDataView& data,
                                    const EventLog& log,
                                    const EditDistanceOptions& options) {
  if (data.kind() == EventDataKind::kEventLog || log.event_count() == 0)
    return;
  require_table_column(data, log, options.case_id_attribute, "case id");
  require_table_column(data, log, options.activity_attribute, "activity");
  require_table_column(data, log, options.timestamp_attribute, "timestamp");
}

EventLog project_public_variant(
    const EventLog& source,
    const std::vector<EditDistanceAlignment>& alignments,
    const std::vector<std::string>& variant,
    const EditDistanceOptions& options) {
  if (alignments.size() != source.traces.size()) {
    throw std::invalid_argument(
        "edit-distance projection requires one alignment per source trace");
  }

  EventLog projected;
  set_attr(projected.attributes, "@@aligned_variant", join_variant(variant));
  for (std::size_t trace_index = 0; trace_index < source.traces.size();
       ++trace_index) {
    const auto& original = source.traces[trace_index];
    // The public implementation drops empty traces while it constructs the
    // projected event records.
    if (original.events.empty()) continue;

    const auto& alignment = alignments[trace_index];
    Trace projected_trace;
    set_attr(projected_trace.attributes, "@@original_trace",
             join_variant(alignment.source_variant));

    SelectedTimestamp current_timestamp = public_timestamp_value(
        original.events.front(), options.timestamp_attribute);
    std::size_t source_pos = 0;
    for (const auto& move : alignment.alignment) {
      if (move.type == EditDistanceMoveType::kLog) {
        // The public projection cursor advances only on synchronous moves.
        continue;
      }

      if (move.type == EditDistanceMoveType::kSynchronous) {
        if (source_pos >= original.events.size()) {
          throw std::invalid_argument(
              "edit-distance projection alignment consumes too many source events");
        }
        const auto& activity_source = original.events[source_pos++];
        current_timestamp = public_timestamp_value(
            activity_source, options.timestamp_attribute);
      }

      Event event;
      event.activity = move.model_activity;
      event.activity_state = FirstClassAttributeState::kPresent;
      event.activity_source_key = options.activity_attribute;
      EventAttribute selected_activity{options.activity_attribute,
                                       move.model_activity,
                                       AttributeKind::kString};
      event.activity_attribute = selected_activity;
      event.attributes.push_back(std::move(selected_activity));

      auto projected_timestamp = current_timestamp;
      if (projected_timestamp.attribute) {
        projected_timestamp.attribute->key = options.timestamp_attribute;
        projected_timestamp.attribute->first_class_role =
            FirstClassAttributeRole::kNone;
        projected_timestamp.attribute->first_class_state =
            FirstClassAttributeState::kMissing;
        projected_timestamp.attribute->first_class_source_key.clear();
        projected_timestamp.source_key = options.timestamp_attribute;
      }
      set_projected_timestamp(event, projected_timestamp);
      if (event.timestamp_attribute) {
        event.timestamp_source_key = options.timestamp_attribute;
      } else if (current_timestamp.state != FirstClassAttributeState::kMissing ||
                 !current_timestamp.value.empty()) {
        EventAttribute selected{options.timestamp_attribute,
                                current_timestamp.value,
                                AttributeKind::kString};
        event.timestamp_attribute = selected;
        event.timestamp_source_key = options.timestamp_attribute;
        event.attributes.push_back(std::move(selected));
      }

      set_attr(event.attributes, "@@is_conforming",
               move.type == EditDistanceMoveType::kSynchronous ? "true" : "false",
               AttributeKind::kBoolean);
      event.input_order = projected_trace.events.size();
      projected_trace.events.push_back(std::move(event));
    }
    projected.traces.push_back(std::move(projected_trace));
  }
  return projected;
}

std::uint64_t checked_cost(std::size_t deviations, std::uint64_t unit) {
  if (deviations > std::numeric_limits<std::uint64_t>::max() / unit)
    throw std::invalid_argument("edit-distance cost overflows uint64_t");
  return static_cast<std::uint64_t>(deviations) * unit;
}

Variant utf8_codepoint_tokens(std::string_view value) {
  Variant tokens;
  for (std::size_t i = 0; i < value.size();) {
    const auto lead = static_cast<unsigned char>(value[i]);
    std::size_t width = 0;
    if (lead <= 0x7f) {
      width = 1;
    } else if (lead >= 0xc2 && lead <= 0xdf) {
      width = 2;
    } else if (lead >= 0xe0 && lead <= 0xef) {
      width = 3;
    } else if (lead >= 0xf0 && lead <= 0xf4) {
      width = 4;
    } else {
      throw std::invalid_argument(
          "edit-distance direct alignment contains invalid UTF-8");
    }
    if (width > value.size() - i) {
      throw std::invalid_argument(
          "edit-distance direct alignment contains invalid UTF-8");
    }
    for (std::size_t j = 1; j < width; ++j) {
      const auto continuation = static_cast<unsigned char>(value[i + j]);
      if (continuation < 0x80 || continuation > 0xbf) {
        throw std::invalid_argument(
            "edit-distance direct alignment contains invalid UTF-8");
      }
    }
    if ((lead == 0xe0 && static_cast<unsigned char>(value[i + 1]) < 0xa0) ||
        (lead == 0xed && static_cast<unsigned char>(value[i + 1]) > 0x9f) ||
        (lead == 0xf0 && static_cast<unsigned char>(value[i + 1]) < 0x90) ||
        (lead == 0xf4 && static_cast<unsigned char>(value[i + 1]) > 0x8f)) {
      throw std::invalid_argument(
          "edit-distance direct alignment contains invalid UTF-8");
    }
    tokens.emplace_back(value.substr(i, width));
    i += width;
  }
  return tokens;
}

Variant select_direct_target(const Variant& source_tokens,
                             const std::vector<std::string>& list_encodings,
                             const std::set<std::string>& set_encodings,
                             bool anti_alignment) {
  std::string encoded_source;
  for (const auto& token : source_tokens) encoded_source += token;
  if (!anti_alignment &&
      set_encodings.find(encoded_source) != set_encodings.end()) {
    // The exact-match set is independent of the ordered candidate list.
    return source_tokens;
  }

  Variant selected_target;
  bool selected = false;
  std::size_t best_distance = 0;
  for (const auto& encoding : list_encodings) {
    Variant target_tokens = utf8_codepoint_tokens(encoding);
    const std::size_t distance = levenshtein(source_tokens, target_tokens);
    const bool better = !selected ||
        (anti_alignment ? distance > best_distance
                        : distance < best_distance) ||
        (anti_alignment && distance == best_distance &&
         target_tokens.size() > selected_target.size());
    if (better) {
      selected = true;
      best_distance = distance;
      selected_target = std::move(target_tokens);
    }
  }
  if (!selected || (anti_alignment && best_distance == 0)) {
    throw std::invalid_argument(
        "edit-distance direct alignment has no selectable target");
  }
  return selected_target;
}

struct TypedMappingEntry {
  EventAttribute key;
  std::string encoding;
};

std::string python_unhashable_type(const EventAttribute& value) {
  switch (value.container) {
    case AttributeContainerKind::kList: return "list";
    case AttributeContainerKind::kSet: return "set";
    case AttributeContainerKind::kContainer: return "dict";
    case AttributeContainerKind::kTuple:
    case AttributeContainerKind::kFrozenSet:
      for (const auto& child : value.children) {
        if (!root_cause_key_is_hashable(child))
          return python_unhashable_type(child);
      }
      break;
    case AttributeContainerKind::kScalar:
      break;
  }
  return "dict";
}

void require_direct_hashable_activity(const EventAttribute& value) {
  if (!root_cause_key_is_hashable(value)) {
    throw std::invalid_argument("unhashable type: '" +
                                python_unhashable_type(value) + "'");
  }
}

struct NormalizedTypedMapping {
  std::vector<TypedMappingEntry> entries;
  std::map<std::string, std::vector<std::size_t>> buckets;
};

NormalizedTypedMapping normalize_typed_mapping(
    const EditDistanceTypedTraceMapping& mapping) {
  NormalizedTypedMapping result;
  result.entries.reserve(mapping.size());
  for (const auto& [key, encoding] : mapping) {
    require_direct_hashable_activity(key);
    const std::string fingerprint = root_cause_scalar_key_fingerprint(key);
    auto& bucket = result.buckets[fingerprint];
    const auto equal = std::find_if(
        bucket.begin(), bucket.end(), [&](std::size_t index) {
          return root_cause_scalar_keys_equal(result.entries[index].key, key);
        });
    if (equal != bucket.end()) {
      // Assignment to an existing Python dict key replaces its value without
      // replacing the original key object or changing insertion order.
      result.entries[*equal].encoding = encoding;
    } else {
      bucket.push_back(result.entries.size());
      result.entries.push_back({key, encoding});
    }
  }
  return result;
}

EventAttribute direct_attribute_value(EventAttribute value) {
  if (value.first_class_state == FirstClassAttributeState::kNull)
    value.is_null = true;
  return value;
}

EventAttribute direct_typed_trace_activity(const Event& event,
                                           std::string_view selector) {
  const auto exact = std::find_if(
      event.attributes.begin(), event.attributes.end(),
      [&](const EventAttribute& attribute) { return attribute.key == selector; });
  if (exact != event.attributes.end()) return direct_attribute_value(*exact);

  if (event.activity_attribute &&
      (event.activity_attribute->key == selector ||
       (!event.activity_source_key.empty() &&
        event.activity_source_key == selector))) {
    return direct_attribute_value(*event.activity_attribute);
  }

  if (!event.activity_source_key.empty() &&
      event.activity_source_key == selector &&
      (event.activity_state != FirstClassAttributeState::kMissing ||
       !event.activity.empty())) {
    EventAttribute value{event.activity_source_key, event.activity,
                         AttributeKind::kString};
    value.first_class_role = FirstClassAttributeRole::kActivity;
    value.first_class_state = event.activity_state;
    value.first_class_source_key = event.activity_source_key;
    value.is_null = event.activity_state == FirstClassAttributeState::kNull;
    return value;
  }

  // Events created through the legacy native constructor have no explicit
  // source-key metadata; their activity member represents concept:name only.
  if (selector == "concept:name" && event.activity_source_key.empty() &&
      !event.activity_attribute &&
      (event.activity_state != FirstClassAttributeState::kMissing ||
       !event.activity.empty())) {
    EventAttribute value{"concept:name", event.activity, AttributeKind::kString};
    value.first_class_role = FirstClassAttributeRole::kActivity;
    value.first_class_state = event.activity_state;
    value.is_null = event.activity_state == FirstClassAttributeState::kNull;
    return value;
  }

  throw std::out_of_range(
      "edit-distance direct alignment has no selected activity: " +
      std::string(selector));
}

const TypedMappingEntry* find_typed_mapping_entry(
    const NormalizedTypedMapping& mapping, const EventAttribute& activity) {
  // Public dict lookup hashes every selected activity before it compares
  // candidates, even when a cache entry for the eventual encoded trace exists.
  require_direct_hashable_activity(activity);
  const std::string fingerprint = root_cause_scalar_key_fingerprint(activity);
  const auto bucket = mapping.buckets.find(fingerprint);
  if (bucket == mapping.buckets.end()) return nullptr;
  for (const std::size_t index : bucket->second) {
    if (root_cause_scalar_keys_equal(mapping.entries[index].key, activity))
      return &mapping.entries[index];
  }
  return nullptr;
}

const EventAttribute& decode_direct_typed_codepoint(
    const std::string& token,
    const std::map<std::string, EventAttribute>& reverse_mapping) {
  const auto found = reverse_mapping.find(token);
  if (found == reverse_mapping.end()) {
    throw std::out_of_range(
        "edit-distance direct alignment has no reverse mapping for UTF-8 "
        "codepoint: " + token);
  }
  return found->second;
}

EditDistanceTypedMove decode_direct_typed_move(
    const EditDistanceMove& encoded,
    const std::map<std::string, EventAttribute>& reverse_mapping) {
  EditDistanceTypedMove result;
  result.type = encoded.type;
  if (encoded.type == EditDistanceMoveType::kSynchronous) {
    result.log_activity =
        decode_direct_typed_codepoint(encoded.log_activity, reverse_mapping);
    result.model_activity =
        decode_direct_typed_codepoint(encoded.model_activity, reverse_mapping);
  } else if (encoded.type == EditDistanceMoveType::kLog) {
    result.log_activity =
        decode_direct_typed_codepoint(encoded.log_activity, reverse_mapping);
  } else {
    result.model_activity =
        decode_direct_typed_codepoint(encoded.model_activity, reverse_mapping);
  }
  return result;
}

struct DirectTraceActivity {
  const std::string* value = nullptr;
  bool is_string = false;
};

bool is_direct_string_attribute(const EventAttribute& attribute) {
  require_direct_hashable_activity(attribute);
  return !attribute.is_null &&
         attribute.first_class_state != FirstClassAttributeState::kNull &&
         attribute.kind == AttributeKind::kString &&
         std::holds_alternative<std::monostate>(attribute.typed_value);
}

DirectTraceActivity direct_trace_activity(const Event& event,
                                          std::string_view selector) {
  // Public Trace events are mappings: a present non-string value is distinct
  // from a missing key, and only an actual string can match a string mapping
  // key. Inspect exact-key attributes before the native first-class view.
  const auto exact = std::find_if(
      event.attributes.begin(), event.attributes.end(),
      [&](const EventAttribute& attribute) { return attribute.key == selector; });
  if (exact != event.attributes.end()) {
    return {&exact->value, is_direct_string_attribute(*exact)};
  }

  if (event.activity_attribute &&
      (event.activity_attribute->key == selector ||
       (!event.activity_source_key.empty() &&
        event.activity_source_key == selector))) {
    return {&event.activity_attribute->value,
            is_direct_string_attribute(*event.activity_attribute)};
  }

  if (!event.activity_source_key.empty() &&
      event.activity_source_key == selector &&
      (event.activity_state != FirstClassAttributeState::kMissing ||
       !event.activity.empty())) {
    return {&event.activity,
            event.activity_state != FirstClassAttributeState::kNull};
  }

  // Events created through the legacy native constructor have no explicit
  // source-key metadata; their activity member represents concept:name only.
  if (selector == "concept:name" && event.activity_source_key.empty() &&
      !event.activity_attribute &&
      (event.activity_state != FirstClassAttributeState::kMissing ||
       !event.activity.empty())) {
    return {&event.activity,
            event.activity_state != FirstClassAttributeState::kNull};
  }

  throw std::out_of_range(
      "edit-distance direct alignment has no selected activity: " +
      std::string(selector));
}

std::string decode_direct_codepoint(
    const std::string& token,
    const std::map<std::string, std::string>& reverse_mapping) {
  const auto found = reverse_mapping.find(token);
  if (found == reverse_mapping.end()) {
    throw std::out_of_range(
        "edit-distance direct alignment has no reverse mapping for UTF-8 "
        "codepoint: " + token);
  }
  return found->second;
}

EditDistanceMove decode_direct_move(
    const EditDistanceMove& encoded,
    const std::map<std::string, std::string>& reverse_mapping) {
  EditDistanceMove result;
  result.type = encoded.type;
  if (encoded.type == EditDistanceMoveType::kSynchronous) {
    result.log_activity =
        decode_direct_codepoint(encoded.log_activity, reverse_mapping);
    result.model_activity =
        decode_direct_codepoint(encoded.model_activity, reverse_mapping);
    result.activity = result.log_activity;
    result.log = result.log_activity;
    result.model = result.model_activity;
  } else if (encoded.type == EditDistanceMoveType::kLog) {
    result.log_activity =
        decode_direct_codepoint(encoded.log_activity, reverse_mapping);
    result.activity = result.log_activity;
    result.log = result.log_activity;
  } else {
    result.model_activity =
        decode_direct_codepoint(encoded.model_activity, reverse_mapping);
    result.activity = result.model_activity;
    result.model = result.model_activity;
  }
  return result;
}

}  // namespace

EditDistanceTraceHandle apply_edit_distance_trace(
    const Trace& trace, const std::vector<std::string>& list_encodings,
    const std::set<std::string>& set_encodings,
    const EditDistanceTraceMapping& mapping, EditDistanceTraceCache* cache,
    EditDistanceOptions options) {
  std::string encoded_source;
  for (const auto& event : trace.events) {
    const auto activity =
        direct_trace_activity(event, options.activity_attribute);
    if (!activity.is_string) continue;
    const auto mapped = std::find_if(
        mapping.begin(), mapping.end(), [&](const auto& entry) {
          return entry.first == *activity.value;
        });
    if (mapped != mapping.end()) encoded_source += mapped->second;
  }

  if (cache != nullptr) {
    const auto cached = std::find_if(
        cache->begin(), cache->end(), [&](const auto& entry) {
          return entry.first == encoded_source;
        });
    if (cached != cache->end()) return cached->second;
  }

  if (options.deviation_cost == 0) {
    throw std::invalid_argument(
        "edit-distance deviation_cost must be positive");
  }

  Variant source_tokens = utf8_codepoint_tokens(encoded_source);
  Variant selected_target = select_direct_target(
      source_tokens, list_encodings, set_encodings,
      options.perform_anti_alignment);

  std::map<std::string, std::string> reverse_mapping;
  for (const auto& [activity, encoding] : mapping)
    reverse_mapping[encoding] = activity;

  const auto encoded_moves = make_moves(source_tokens, selected_target);
  EditDistanceTraceResult result;
  result.alignment.reserve(encoded_moves.size());
  std::size_t deviations = 0;
  for (const auto& move : encoded_moves) {
    if (move.type != EditDistanceMoveType::kSynchronous) ++deviations;
    result.alignment.push_back(decode_direct_move(move, reverse_mapping));
  }
  result.cost = checked_cost(deviations, options.deviation_cost);

  auto handle = std::make_shared<std::any>(std::move(result));
  if (cache != nullptr) cache->emplace_back(encoded_source, handle);
  return handle;
}

EditDistanceTraceHandle apply_typed_edit_distance_trace(
    const Trace& trace, const std::vector<std::string>& list_encodings,
    const std::set<std::string>& set_encodings,
    const EditDistanceTypedTraceMapping& mapping,
    EditDistanceTraceCache* cache, EditDistanceOptions options) {
  const NormalizedTypedMapping normalized_mapping =
      normalize_typed_mapping(mapping);

  std::string encoded_source;
  for (const auto& event : trace.events) {
    const EventAttribute activity =
        direct_typed_trace_activity(event, options.activity_attribute);
    const auto* mapped = find_typed_mapping_entry(normalized_mapping, activity);
    if (mapped != nullptr) encoded_source += mapped->encoding;
  }

  if (cache != nullptr) {
    const auto cached = std::find_if(
        cache->begin(), cache->end(), [&](const auto& entry) {
          return entry.first == encoded_source;
        });
    if (cached != cache->end()) return cached->second;
  }

  if (options.deviation_cost == 0) {
    throw std::invalid_argument(
        "edit-distance deviation_cost must be positive");
  }

  const Variant source_tokens = utf8_codepoint_tokens(encoded_source);
  const Variant selected_target = select_direct_target(
      source_tokens, list_encodings, set_encodings,
      options.perform_anti_alignment);

  std::map<std::string, EventAttribute> reverse_mapping;
  for (const auto& entry : normalized_mapping.entries)
    reverse_mapping[entry.encoding] = entry.key;

  const auto encoded_moves = make_moves(source_tokens, selected_target);
  EditDistanceTypedTraceResult result;
  result.alignment.reserve(encoded_moves.size());
  std::size_t deviations = 0;
  for (const auto& move : encoded_moves) {
    if (move.type != EditDistanceMoveType::kSynchronous) ++deviations;
    result.alignment.push_back(
        decode_direct_typed_move(move, reverse_mapping));
  }
  result.cost = checked_cost(deviations, options.deviation_cost);

  auto handle = std::make_shared<std::any>(std::move(result));
  if (cache != nullptr) cache->emplace_back(encoded_source, handle);
  return handle;
}

std::vector<EditDistanceAlignment> align_edit_distance(
    const EventLog& source, const EventLog& target, EditDistanceOptions options) {
  validate_selector(options.activity_attribute, "activity");
  validate_selector(options.timestamp_attribute, "timestamp");
  if (options.deviation_cost == 0) {
    throw std::invalid_argument("edit-distance deviation_cost must be positive");
  }
  if (target.traces.empty()) {
    throw std::invalid_argument("edit-distance target log must not be empty");
  }
  if (source.traces.empty()) return {};

  std::vector<TargetVariant> targets;
  std::map<Variant, std::size_t> target_seen;
  for (std::size_t i = 0; i < target.traces.size(); ++i) {
    Variant v = activities_of(target.traces[i], options.activity_attribute);
    if (target_seen.emplace(v, targets.size()).second) targets.push_back({std::move(v), i});
  }
  std::size_t shortest = std::numeric_limits<std::size_t>::max();
  for (const auto& t : targets) shortest = std::min(shortest, t.activities.size());
  std::map<Variant, EditDistanceAlignment> cache;
  std::vector<EditDistanceAlignment> result;
  result.reserve(source.traces.size());
  for (const auto& trace : source.traces) {
    Variant source_variant = activities_of(trace, options.activity_attribute);
    auto cached = cache.find(source_variant);
    if (cached == cache.end()) {
      std::size_t selected = 0;
      std::size_t selected_distance = options.perform_anti_alignment ? 0 : std::numeric_limits<std::size_t>::max();
      for (std::size_t ti = 0; ti < targets.size(); ++ti) {
        const std::size_t distance = levenshtein(source_variant, targets[ti].activities);
        bool better = options.perform_anti_alignment ? distance > selected_distance
                                                       : distance < selected_distance;
        if (!better && distance == selected_distance) {
          if (options.perform_anti_alignment) {
            // Anti-alignment first prefers a longer target. Equal-length
            // ties choose the last distinct target in first-seen order.
            better = targets[ti].activities.size() >
                         targets[selected].activities.size() ||
                     (targets[ti].activities.size() ==
                          targets[selected].activities.size() &&
                      targets[ti].first_index > targets[selected].first_index);
          } else {
            // Ordinary alignment first prefers a shorter target. Equal-length
            // ties then choose the last distinct target in first-seen order.
            better = targets[ti].activities.size() <
                         targets[selected].activities.size() ||
                     (targets[ti].activities.size() ==
                          targets[selected].activities.size() &&
                      targets[ti].first_index > targets[selected].first_index);
          }
        }
        if (better) { selected = ti; selected_distance = distance; }
      }
      if (options.perform_anti_alignment && selected_distance == 0) {
        // The public anti-alignment path cannot construct a result when all
        // target candidates are exact. Preserve that rejection as a clear
        // native validation error, including duplicate and empty variants.
        throw std::invalid_argument(
            "edit-distance anti-alignment has no positive-distance target");
      }
      const auto moves = make_moves(source_variant, targets[selected].activities);
      std::uint64_t deviations = 0;
      for (const auto& move : moves)
        if (move.type != EditDistanceMoveType::kSynchronous) ++deviations;
      EditDistanceAlignment alignment;
      alignment.alignment = moves;
      alignment.moves = moves;
      alignment.source_variant = source_variant;
      alignment.target_variant = targets[selected].activities;
      alignment.target_index = targets[selected].first_index;
      alignment.cost = checked_cost(deviations, options.deviation_cost);
      if (source_variant.size() > std::numeric_limits<std::size_t>::max() - shortest)
        throw std::invalid_argument("edit-distance denominator overflows size_t");
      alignment.denominator = checked_cost(source_variant.size() + shortest,
                                           options.deviation_cost);
      // The complete worst-case denominator `bwc` (the shortest
      // target plus every source event as a deviation), not merely the target
      // component of that sum.
      alignment.best_worst_cost = alignment.denominator;
      alignment.bwc = alignment.denominator;
      alignment.fitness = alignment.cost == 0 ? 1.0
          : (alignment.denominator == 0 ? 0.0 : 1.0 - static_cast<double>(deviations) /
                                                static_cast<double>(source_variant.size() + shortest));
      cached = cache.emplace(source_variant, std::move(alignment)).first;
    }
    result.push_back(cached->second);
  }
  return result;
}

std::vector<EditDistanceAlignment> align_edit_distance(
  const EventDataView& source, const EventDataView& target,
    EditDistanceOptions options) {
  const auto source_log = event_data_as_log(source, options);
  require_flat_alignment_columns(source, source_log, options);
  const auto target_log = event_data_as_log(target, options);
  require_flat_alignment_columns(target, target_log, options);
  return align_edit_distance(source_log, target_log, std::move(options));
}

std::vector<EditDistanceTypedAlignment> align_typed_edit_distance(
    const EventLog& source, const EventLog& target,
    EditDistanceOptions options) {
  validate_selector(options.activity_attribute, "activity");
  validate_selector(options.timestamp_attribute, "timestamp");
  if (options.deviation_cost == 0) {
    throw std::invalid_argument("edit-distance deviation_cost must be positive");
  }

  // Public trace values are hash keys. Extract and intern every source value
  // first, then every target value, before the legacy engine's empty-log
  // shortcuts or target validation can run.
  TypedActivityInterner interner;
  const EventLog encoded_source = intern_typed_activities(
      source, options.activity_attribute, interner);
  const EventLog encoded_target = intern_typed_activities(
      target, options.activity_attribute, interner);
  const auto encoded_alignments = align_edit_distance(
      encoded_source, encoded_target, options);

  std::vector<EditDistanceTypedAlignment> result;
  result.reserve(encoded_alignments.size());
  for (const auto& encoded : encoded_alignments) {
    EditDistanceTypedAlignment alignment;
    alignment.alignment.reserve(encoded.alignment.size());
    for (const auto& move : encoded.alignment)
      alignment.alignment.push_back(
          decode_typed_alignment_move(move, interner));
    alignment.moves = alignment.alignment;
    alignment.source_variant =
        decode_typed_variant(encoded.source_variant, interner);
    alignment.target_variant =
        decode_typed_variant(encoded.target_variant, interner);
    alignment.target_index = encoded.target_index;
    alignment.cost = encoded.cost;
    alignment.fitness = encoded.fitness;
    alignment.best_worst_cost = encoded.best_worst_cost;
    alignment.bwc = encoded.bwc;
    alignment.denominator = encoded.denominator;
    result.push_back(std::move(alignment));
  }
  return result;
}

std::vector<EditDistanceTypedAlignment> align_typed_edit_distance(
    const EventDataView& source, const EventDataView& target,
    EditDistanceOptions options) {
  const auto source_log = event_data_as_log(source, options);
  require_flat_alignment_columns(source, source_log, options);
  const auto target_log = event_data_as_log(target, options);
  require_flat_alignment_columns(target, target_log, options);
  return align_typed_edit_distance(source_log, target_log, std::move(options));
}

namespace {

bool typed_variants_equal(const std::vector<EventAttribute>& left,
                          const std::vector<EventAttribute>& right) {
  if (left.size() != right.size()) return false;
  for (std::size_t i = 0; i < left.size(); ++i) {
    if (!root_cause_scalar_keys_equal(left[i], right[i])) return false;
  }
  return true;
}

std::string join_typed_variant(const std::vector<EventAttribute>& variant) {
  std::string result;
  for (std::size_t i = 0; i < variant.size(); ++i) {
    if (i != 0) result += ',';
    result += root_cause_attribute_category(variant[i]);
  }
  return result;
}

void set_typed_projected_activity(Event& target, const Event* source,
                                  std::string_view selector,
                                  const EventAttribute& selected) {
  const auto source_selection = source == nullptr
                                    ? std::optional<EventAttribute>{}
                                    : selected_typed_activity(*source, selector);
  // The configured selector is the projected identity even when either log
  // stores the selected first-class value under a canonical XES key.
  const std::string key(selector);

  EventAttribute activity = selected;
  activity.key = key;
  if (activity.first_class_role == FirstClassAttributeRole::kNone &&
      source_selection) {
    activity.first_class_role = source_selection->first_class_role;
  }
  if (activity.first_class_role == FirstClassAttributeRole::kNone &&
      source != nullptr) {
    const auto matches_source_slot = [&](
        const std::optional<EventAttribute>& stored,
        const std::string& source_key) {
      return source_key == selector ||
             (stored && (stored->key == selector ||
                         stored->first_class_source_key == selector));
    };
    if (matches_source_slot(source->activity_attribute,
                            source->activity_source_key)) {
      activity.first_class_role = FirstClassAttributeRole::kActivity;
    } else if (matches_source_slot(source->timestamp_attribute,
                                   source->timestamp_source_key)) {
      activity.first_class_role = FirstClassAttributeRole::kTimestamp;
    } else if (matches_source_slot(source->resource_attribute,
                                   source->resource_source_key)) {
      activity.first_class_role = FirstClassAttributeRole::kResource;
    } else if (matches_source_slot(source->lifecycle_attribute,
                                   source->lifecycle_source_key)) {
      activity.first_class_role = FirstClassAttributeRole::kLifecycle;
    }
  }
  if (activity.first_class_role == FirstClassAttributeRole::kNone) {
    const auto role_from_key = [](std::string_view candidate) {
      if (candidate == "concept:name" || candidate == "activity")
        return FirstClassAttributeRole::kActivity;
      if (candidate == "time:timestamp" || candidate == "timestamp")
        return FirstClassAttributeRole::kTimestamp;
      if (candidate == "org:resource" || candidate == "resource")
        return FirstClassAttributeRole::kResource;
      if (candidate == "lifecycle:transition" || candidate == "lifecycle")
        return FirstClassAttributeRole::kLifecycle;
      return FirstClassAttributeRole::kNone;
    };
    activity.first_class_role = role_from_key(selector);
    if (activity.first_class_role == FirstClassAttributeRole::kNone)
      activity.first_class_role = role_from_key(selected.key);
    if (activity.first_class_role == FirstClassAttributeRole::kNone &&
        source_selection)
      activity.first_class_role = role_from_key(source_selection->key);
  }
  const bool is_null = activity.is_null ||
      activity.first_class_state == FirstClassAttributeState::kNull;
  activity.is_null = is_null;
  if (activity.first_class_role != FirstClassAttributeRole::kNone) {
    activity.first_class_state = is_null ? FirstClassAttributeState::kNull
                                         : FirstClassAttributeState::kPresent;
    activity.first_class_source_key = key;
  }

  target.activity = activity.value;
  target.activity_state = is_null ? FirstClassAttributeState::kNull
                                  : FirstClassAttributeState::kPresent;
  target.activity_source_key = key;
  target.activity_attribute = activity;
  const auto existing = std::find_if(
      target.attributes.begin(), target.attributes.end(),
      [&](const EventAttribute& attribute) { return attribute.key == key; });
  if (existing == target.attributes.end())
    target.attributes.push_back(activity);
  else
    *existing = activity;

  const auto state = is_null ? FirstClassAttributeState::kNull
                             : FirstClassAttributeState::kPresent;
  switch (activity.first_class_role) {
    case FirstClassAttributeRole::kTimestamp:
      target.timestamp = activity.value;
      target.timestamp_state = state;
      target.timestamp_source_key = key;
      target.timestamp_attribute = activity;
      break;
    case FirstClassAttributeRole::kResource:
      target.resource = activity.value;
      target.resource_state = state;
      target.resource_source_key = key;
      target.resource_attribute = activity;
      break;
    case FirstClassAttributeRole::kLifecycle:
      target.lifecycle = activity.value;
      target.lifecycle_state = state;
      target.lifecycle_source_key = key;
      target.lifecycle_attribute = activity;
      break;
    case FirstClassAttributeRole::kNone:
    case FirstClassAttributeRole::kCaseId:
    case FirstClassAttributeRole::kActivity:
      break;
  }
}

}  // namespace

EventLog project_log_on_typed_alignments(
    const EventLog& source,
    const std::vector<EditDistanceTypedAlignment>& alignments,
    EditDistanceOptions options) {
  validate_selector(options.activity_attribute, "activity");
  validate_selector(options.timestamp_attribute, "timestamp");
  if (alignments.size() != source.traces.size()) {
    throw std::invalid_argument(
        "edit-distance projection requires one alignment per source trace");
  }

  EventLog projected;
  copy_event_log_context(source, projected);
  erase_attr(projected.attributes, "@@aligned_variant");
  erase_attr(projected.attributes, "@@aligned_variants");
  bool uniform_target = true;
  for (std::size_t i = 1; i < alignments.size(); ++i) {
    if (!typed_variants_equal(alignments[i].target_variant,
                              alignments.front().target_variant)) {
      uniform_target = false;
      break;
    }
  }
  if (uniform_target && !alignments.empty()) {
    set_attr(projected.attributes, "@@aligned_variant",
             join_typed_variant(alignments.front().target_variant));
  } else if (!alignments.empty()) {
    set_attr(projected.attributes, "@@aligned_variants", "multiple");
  }

  for (std::size_t trace_index = 0; trace_index < source.traces.size();
       ++trace_index) {
    const auto& original = source.traces[trace_index];
    const auto& alignment = alignments[trace_index];
    Trace projected_trace;
    copy_trace_context(original, projected_trace);
    set_attr(projected_trace.attributes, "@@original_trace",
             join_typed_variant(alignment.source_variant));
    set_attr(projected_trace.attributes, "@@aligned_variant",
             join_typed_variant(alignment.target_variant));

    const Event* activity_template =
        original.events.empty() ? nullptr : &original.events.front();
    SelectedTimestamp current_timestamp;
    if (!original.events.empty()) {
      current_timestamp = timestamp_value(
          original.events.front(), options.timestamp_attribute, true);
    }
    std::size_t source_pos = 0;
    for (const auto& move : alignment.alignment) {
      if (move.type == EditDistanceMoveType::kLog) {
        if (source_pos >= original.events.size()) {
          throw std::invalid_argument(
              "edit-distance projection alignment consumes too many source events");
        }
        ++source_pos;
        continue;
      }
      if (!move.model_activity) {
        throw std::invalid_argument(
            "edit-distance typed projection move has no model activity");
      }

      Event event;
      if (move.type == EditDistanceMoveType::kSynchronous) {
        if (source_pos >= original.events.size()) {
          throw std::invalid_argument(
              "edit-distance projection alignment consumes too many source events");
        }
        const auto& source_event = original.events[source_pos++];
        event = source_event;
        bind_selected_activity(event, source_event, options.activity_attribute,
                               move.model_activity->value);
        current_timestamp = timestamp_value(
            source_event, options.timestamp_attribute, true);
        bind_selected_timestamp(event, current_timestamp);
        set_typed_projected_activity(event, &source_event,
                                     options.activity_attribute,
                                     *move.model_activity);
      } else {
        copy_event_case_context(original, event);
        if (activity_template != nullptr) {
          set_projected_activity(event, activity_template,
                                 options.activity_attribute,
                                 move.model_activity->value);
        } else {
          set_projected_activity(event, nullptr, options.activity_attribute,
                                 move.model_activity->value);
        }
        set_projected_timestamp(event, current_timestamp);
        set_typed_projected_activity(event, activity_template,
                                     options.activity_attribute,
                                     *move.model_activity);
      }
      event.input_order = projected_trace.events.size();
      set_attr(event.attributes, "@@is_conforming",
               move.type == EditDistanceMoveType::kSynchronous ? "true" : "false",
               AttributeKind::kBoolean);
      projected_trace.events.push_back(std::move(event));
    }
    projected.traces.push_back(std::move(projected_trace));
  }
  return projected;
}

EventLog project_log_on_alignments(
    const EventLog& source,
    const std::vector<EditDistanceAlignment>& alignments,
    EditDistanceOptions options) {
  validate_selector(options.activity_attribute, "activity");
  validate_selector(options.timestamp_attribute, "timestamp");
  if (alignments.size() != source.traces.size()) {
    throw std::invalid_argument(
        "edit-distance projection requires one alignment per source trace");
  }
  EventLog projected;
  copy_event_log_context(source, projected);
  erase_attr(projected.attributes, "@@aligned_variant");
  erase_attr(projected.attributes, "@@aligned_variants");
  bool uniform_target = true;
  for (std::size_t i = 1; i < alignments.size(); ++i) {
    if (alignments[i].target_variant != alignments.front().target_variant) {
      uniform_target = false;
      break;
    }
  }
  if (uniform_target && !alignments.empty()) {
    set_attr(projected.attributes, "@@aligned_variant",
             join_variant(alignments.front().target_variant));
  } else if (!alignments.empty()) {
    // A single log-level target marker would be ambiguous for heterogeneous
    // projections. Keep the marker explicit while recording the selected
    // variant on each trace below.
    set_attr(projected.attributes, "@@aligned_variants", "multiple");
  }
  for (std::size_t ti = 0; ti < source.traces.size(); ++ti) {
    const auto& original = source.traces[ti];
    const auto& alignment = alignments[ti];
    Trace projected_trace;
    copy_trace_context(original, projected_trace);
    set_attr(projected_trace.attributes, "@@original_trace",
             join_variant(alignment.source_variant));
    set_attr(projected_trace.attributes, "@@aligned_variant",
             join_variant(alignment.target_variant));
    const Event* activity_template =
        original.events.empty() ? nullptr : &original.events.front();
    SelectedTimestamp current_timestamp;
    if (!original.events.empty()) {
      current_timestamp =
          timestamp_value(original.events.front(), options.timestamp_attribute,
                         true);
    }
    std::size_t source_pos = 0;
    for (const auto& move : alignment.alignment) {
      if (move.type == EditDistanceMoveType::kLog) {
        if (source_pos >= original.events.size()) {
          throw std::invalid_argument(
              "edit-distance projection alignment consumes too many source events");
        }
        ++source_pos;
        continue;
      }
      Event event;
      if (move.type == EditDistanceMoveType::kSynchronous) {
        if (source_pos >= original.events.size()) {
          throw std::invalid_argument(
              "edit-distance projection alignment consumes too many source events");
        }
        const auto& source_event = original.events[source_pos++];
        event = source_event;
        bind_selected_activity(event, source_event, options.activity_attribute,
                               move.model_activity);
        current_timestamp =
            timestamp_value(source_event, options.timestamp_attribute, true);
        bind_selected_timestamp(event, current_timestamp);
      } else {
        copy_event_case_context(original, event);
        if (activity_template != nullptr) {
          set_projected_activity(event, activity_template,
                                 options.activity_attribute,
                                 move.model_activity);
        } else {
          set_projected_activity(event, nullptr, options.activity_attribute,
                                 move.model_activity);
        }
        set_projected_timestamp(event, current_timestamp);
      }
      event.input_order = projected_trace.events.size();
      set_attr(event.attributes, "@@is_conforming",
               move.type == EditDistanceMoveType::kSynchronous ? "true" : "false",
               AttributeKind::kBoolean);
      projected_trace.events.push_back(std::move(event));
    }
    projected.traces.push_back(std::move(projected_trace));
  }
  return projected;
}

EventLog project_log_on_alignments(
    const EventLog& source,
    const std::vector<EditDistanceAlignment>& alignments) {
  return project_log_on_alignments(source, alignments, EditDistanceOptions{});
}

EventLog project_log_on_variant(const EventLog& source,
                                const std::vector<std::string>& variant,
                                EditDistanceOptions options) {
  EventLog target;
  Trace target_trace;
  target_trace.events.reserve(variant.size());
  for (const auto& activity : variant) {
    Event event{"", activity};
    set_synthetic_activity(event, options.activity_attribute, activity);
    target_trace.events.push_back(std::move(event));
  }
  target.traces.push_back(std::move(target_trace));
  const auto alignments = align_edit_distance(source, target, options);
  return project_public_variant(source, alignments, variant, options);
}

EventLog project_log_on_variant(const EventDataView& source,
                                const std::vector<std::string>& variant,
                                EditDistanceOptions options) {
  const auto converted = event_data_as_log(source, options);
  require_flat_projection_fields(source, converted, options);
  return project_log_on_variant(converted, variant, std::move(options));
}

}  // namespace minera
