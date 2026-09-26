#include <algorithm>
#include <any>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <variant>

#include "minera/edit_distance_alignment.hpp"
#include "minera/json_export.hpp"
#include "minera/log_conversion.hpp"
#include "minera/xes_export.hpp"

using namespace minera;

static EventLog log_of(std::initializer_list<std::initializer_list<const char*>> traces) {
  EventLog log;
  std::size_t c = 0;
  for (const auto& activities : traces) {
    Trace trace;
    trace.case_id = std::to_string(++c);
    std::size_t i = 0;
    for (const auto activity : activities)
      trace.events.push_back(Event{trace.case_id, activity, "t" + std::to_string(i++), {}, i});
    log.traces.push_back(std::move(trace));
  }
  return log;
}

int main() {
  const auto trace_with_activity_attribute = [](EventAttribute attribute) {
    Trace trace;
    Event event;
    event.attributes.push_back(std::move(attribute));
    trace.events.push_back(std::move(event));
    return trace;
  };
  const Trace direct_trace = log_of({{"A", "B"}}).traces.front();
  const EditDistanceTraceMapping direct_mapping{{"A", "a"}, {"B", "b"},
                                                 {"C", "c"}};
  const std::vector<std::string> direct_exact_target{"ab"};
  const std::set<std::string> direct_exact_set{"ab"};
  EditDistanceTraceCache direct_cache;
  const auto direct_exact = apply_edit_distance_trace(
      direct_trace, direct_exact_target, direct_exact_set, direct_mapping,
      &direct_cache);
  assert(direct_exact && direct_cache.size() == 1 &&
         direct_cache.front().first == "ab" &&
         direct_cache.front().second == direct_exact);
  const auto& direct_exact_result =
      std::any_cast<const EditDistanceTraceResult&>(*direct_exact);
  assert(direct_exact_result.cost == 0 &&
         direct_exact_result.alignment.size() == 2 &&
         direct_exact_result.alignment[0].type ==
             EditDistanceMoveType::kSynchronous &&
         direct_exact_result.alignment[0].log_activity == "A" &&
         direct_exact_result.alignment[1].model_activity == "B");

  // Public Trace lookup preserves scalar identity: numeric and null values
  // are present keys but do not match string keys in the supplied mapping.
  EventAttribute integer_activity{"concept:name", "1", AttributeKind::kInt};
  integer_activity.typed_value = std::int64_t{1};
  const auto integer_trace =
      trace_with_activity_attribute(std::move(integer_activity));
  const EditDistanceTraceMapping integer_mapping{{"1", "a"}};
  const auto integer_as_activity = apply_edit_distance_trace(
      integer_trace, std::vector<std::string>{"a"}, std::set<std::string>{},
      integer_mapping);
  const auto& integer_result =
      std::any_cast<const EditDistanceTraceResult&>(*integer_as_activity);
  assert(integer_result.cost == 10000 && integer_result.alignment.size() == 1 &&
         integer_result.alignment.front().type == EditDistanceMoveType::kModel &&
         integer_result.alignment.front().model_activity == "1");

  const Trace string_trace = trace_with_activity_attribute(
      EventAttribute{"concept:name", "1", AttributeKind::kString});
  const auto string_as_activity = apply_edit_distance_trace(
      string_trace, std::vector<std::string>{"a"}, std::set<std::string>{},
      integer_mapping);
  const auto& string_result =
      std::any_cast<const EditDistanceTraceResult&>(*string_as_activity);
  assert(string_result.cost == 0 && string_result.alignment.size() == 1 &&
         string_result.alignment.front().type ==
             EditDistanceMoveType::kSynchronous &&
         string_result.alignment.front().log_activity == "1");

  const Trace null_trace = trace_with_activity_attribute(
      EventAttribute{"concept:name", "None", AttributeKind::kString, true});
  const auto null_as_activity = apply_edit_distance_trace(
      null_trace, std::vector<std::string>{"a"}, std::set<std::string>{},
      EditDistanceTraceMapping{{"None", "a"}});
  const auto& null_result =
      std::any_cast<const EditDistanceTraceResult&>(*null_as_activity);
  assert(null_result.cost == 10000 && null_result.alignment.size() == 1 &&
         null_result.alignment.front().type == EditDistanceMoveType::kModel &&
         null_result.alignment.front().model_activity == "None");

  // Typed direct mapping keys use Python-compatible hash/equality while
  // preserving the original mapping key in decoded moves.
  EventAttribute typed_int_key{"mapping-key", "1", AttributeKind::kInt};
  typed_int_key.typed_value = std::int64_t{1};
  EventAttribute typed_float_activity{"concept:name", "1.0",
                                      AttributeKind::kFloat};
  typed_float_activity.typed_value = 1.0;
  const auto typed_float_trace =
      trace_with_activity_attribute(typed_float_activity);
  const auto typed_numeric = apply_typed_trace(
      typed_float_trace, std::vector<std::string>{"a"},
      std::set<std::string>{"a"},
      EditDistanceTypedTraceMapping{{typed_int_key, "a"}});
  const auto& typed_numeric_result =
      std::any_cast<const EditDistanceTypedTraceResult&>(*typed_numeric);
  assert(typed_numeric_result.cost == 0 &&
         typed_numeric_result.alignment.size() == 1 &&
         typed_numeric_result.alignment.front().type ==
             EditDistanceMoveType::kSynchronous &&
         typed_numeric_result.alignment.front().log_activity &&
         typed_numeric_result.alignment.front().model_activity &&
         typed_numeric_result.alignment.front().log_activity->kind ==
             AttributeKind::kInt &&
         std::get<std::int64_t>(typed_numeric_result.alignment.front()
                                    .log_activity->typed_value) == 1);

  EventAttribute typed_bool_key{"mapping-key", "True",
                                AttributeKind::kBoolean};
  typed_bool_key.typed_value = true;
  const auto typed_integer_bool_alias = apply_typed_edit_distance_trace(
      integer_trace, std::vector<std::string>{"a"},
      std::set<std::string>{"a"},
      EditDistanceTypedTraceMapping{{typed_bool_key, "a"}});
  const auto& typed_bool_result = std::any_cast<const EditDistanceTypedTraceResult&>(
      *typed_integer_bool_alias);
  assert(typed_bool_result.cost == 0 &&
         typed_bool_result.alignment.front().log_activity &&
         typed_bool_result.alignment.front().log_activity->kind ==
             AttributeKind::kBoolean &&
         std::get<bool>(typed_bool_result.alignment.front()
                            .log_activity->typed_value));

  EventAttribute typed_null_key{"mapping-key", "", AttributeKind::kString,
                                true};
  const Trace typed_null_trace = trace_with_activity_attribute(
      EventAttribute{"concept:name", "None", AttributeKind::kString, true});
  const auto typed_null = apply_typed_trace(
      typed_null_trace, std::vector<std::string>{"a"},
      std::set<std::string>{"a"},
      EditDistanceTypedTraceMapping{{typed_null_key, "a"}});
  const auto& typed_null_result =
      std::any_cast<const EditDistanceTypedTraceResult&>(*typed_null);
  assert(typed_null_result.cost == 0 &&
         typed_null_result.alignment.front().log_activity &&
         typed_null_result.alignment.front().log_activity->is_null);

  // Building the mapping follows dict key replacement: equal numeric keys
  // retain the first key object, replace its value, and keep its position.
  EventAttribute typed_bool_alias{"mapping-key", "True",
                                  AttributeKind::kBoolean};
  typed_bool_alias.typed_value = true;
  const auto typed_mapping_collision = apply_typed_trace(
      integer_trace, std::vector<std::string>{"a", "b"},
      std::set<std::string>{"b"},
      EditDistanceTypedTraceMapping{{typed_int_key, "a"},
                                    {typed_bool_alias, "b"}});
  const auto& typed_mapping_collision_result =
      std::any_cast<const EditDistanceTypedTraceResult&>(
          *typed_mapping_collision);
  assert(typed_mapping_collision_result.cost == 0 &&
         typed_mapping_collision_result.alignment.front().log_activity &&
         typed_mapping_collision_result.alignment.front().log_activity->kind ==
             AttributeKind::kInt &&
         std::get<std::int64_t>(typed_mapping_collision_result.alignment.front()
                                    .log_activity->typed_value) == 1);

  // Reverse collisions keep the last normalized mapping entry.
  EventAttribute typed_string_key{"mapping-key", "one",
                                  AttributeKind::kString};
  EventAttribute typed_cache_key{"mapping-key", "A",
                                 AttributeKind::kString};
  EventAttribute typed_string_activity{"concept:name", "1",
                                       AttributeKind::kInt};
  typed_string_activity.typed_value = std::int64_t{1};
  const auto typed_reverse_collision = apply_typed_trace(
      trace_with_activity_attribute(typed_string_activity),
      std::vector<std::string>{"x"}, std::set<std::string>{"x"},
      EditDistanceTypedTraceMapping{{typed_int_key, "x"},
                                    {typed_string_key, "x"}});
  const auto& typed_reverse_collision_result =
      std::any_cast<const EditDistanceTypedTraceResult&>(
          *typed_reverse_collision);
  assert(typed_reverse_collision_result.cost == 0 &&
         typed_reverse_collision_result.alignment.front().log_activity &&
         typed_reverse_collision_result.alignment.front().log_activity->kind ==
             AttributeKind::kString &&
         typed_reverse_collision_result.alignment.front().log_activity->value ==
             "one");

  EventAttribute tuple_int_key{"mapping-key", "", AttributeKind::kString};
  tuple_int_key.container = AttributeContainerKind::kTuple;
  tuple_int_key.children = {
      EventAttribute{"0", "A", AttributeKind::kString},
      EventAttribute{"1", "1", AttributeKind::kInt}};
  tuple_int_key.children[1].typed_value = std::int64_t{1};
  EventAttribute tuple_float_activity = tuple_int_key;
  tuple_float_activity.key = "concept:name";
  tuple_float_activity.children[1].kind = AttributeKind::kFloat;
  tuple_float_activity.children[1].value = "1.0";
  tuple_float_activity.children[1].typed_value = 1.0;
  const auto typed_tuple_alias = apply_typed_trace(
      trace_with_activity_attribute(tuple_float_activity),
      std::vector<std::string>{"t"}, std::set<std::string>{"t"},
      EditDistanceTypedTraceMapping{{tuple_int_key, "t"}});
  const auto& typed_tuple_result =
      std::any_cast<const EditDistanceTypedTraceResult&>(*typed_tuple_alias);
  assert(typed_tuple_result.cost == 0 &&
         typed_tuple_result.alignment.front().log_activity &&
         typed_tuple_result.alignment.front().log_activity->container ==
             AttributeContainerKind::kTuple &&
         typed_tuple_result.alignment.front().log_activity->children[1].kind ==
             AttributeKind::kInt);

  EventAttribute frozen_set_key{"mapping-key", "", AttributeKind::kString};
  frozen_set_key.container = AttributeContainerKind::kFrozenSet;
  frozen_set_key.children = {
      EventAttribute{"", "A", AttributeKind::kString},
      EventAttribute{"", "B", AttributeKind::kString}};
  EventAttribute frozen_set_activity = frozen_set_key;
  frozen_set_activity.key = "concept:name";
  std::reverse(frozen_set_activity.children.begin(),
               frozen_set_activity.children.end());
  const auto typed_frozen_set = apply_typed_trace(
      trace_with_activity_attribute(frozen_set_activity),
      std::vector<std::string>{"f"}, std::set<std::string>{"f"},
      EditDistanceTypedTraceMapping{{frozen_set_key, "f"}});
  const auto& typed_frozen_set_result =
      std::any_cast<const EditDistanceTypedTraceResult&>(*typed_frozen_set);
  assert(typed_frozen_set_result.cost == 0 &&
         typed_frozen_set_result.alignment.front().log_activity &&
         typed_frozen_set_result.alignment.front().log_activity->container ==
             AttributeContainerKind::kFrozenSet);

  const auto typed_bytes_key = make_bytes_attribute("mapping-key", "A");
  const auto typed_bytes_activity = make_bytes_attribute("concept:name", "A");
  const auto typed_bytes = apply_typed_trace(
      trace_with_activity_attribute(typed_bytes_activity),
      std::vector<std::string>{"y"}, std::set<std::string>{"y"},
      EditDistanceTypedTraceMapping{{typed_bytes_key, "y"}});
  const auto& typed_bytes_result =
      std::any_cast<const EditDistanceTypedTraceResult&>(*typed_bytes);
  assert(typed_bytes_result.cost == 0 &&
         typed_bytes_result.alignment.front().log_activity &&
         typed_bytes_result.alignment.front().log_activity->kind ==
             AttributeKind::kBytes);

  // Typed moves distinguish a present null activity from either gap.
  const auto typed_null_log_move = apply_typed_trace(
      typed_null_trace, std::vector<std::string>{""}, std::set<std::string>{},
      EditDistanceTypedTraceMapping{{typed_null_key, "a"}});
  const auto& typed_null_log_result = std::any_cast<const EditDistanceTypedTraceResult&>(
      *typed_null_log_move);
  assert(typed_null_log_result.cost == 10000 &&
         typed_null_log_result.alignment.size() == 1 &&
         typed_null_log_result.alignment.front().type ==
             EditDistanceMoveType::kLog &&
         typed_null_log_result.alignment.front().log_activity &&
         typed_null_log_result.alignment.front().log_activity->is_null &&
         !typed_null_log_result.alignment.front().model_activity);
  const Trace typed_empty_trace;
  const auto typed_model_gap = apply_typed_trace(
      typed_empty_trace, std::vector<std::string>{"a"},
      std::set<std::string>{}, EditDistanceTypedTraceMapping{{typed_int_key, "a"}});
  const auto& typed_model_gap_result =
      std::any_cast<const EditDistanceTypedTraceResult&>(*typed_model_gap);
  assert(typed_model_gap_result.cost == 10000 &&
         typed_model_gap_result.alignment.size() == 1 &&
         typed_model_gap_result.alignment.front().type ==
             EditDistanceMoveType::kModel &&
         !typed_model_gap_result.alignment.front().log_activity &&
         typed_model_gap_result.alignment.front().model_activity &&
         typed_model_gap_result.alignment.front().model_activity->kind ==
             AttributeKind::kInt);

  // Cache hits retain the exact handle, even across typed/string label
  // representations, and cached null remains distinct from a miss.
  EditDistanceTraceCache typed_cache;
  const Trace typed_string_cache_trace = trace_with_activity_attribute(
      EventAttribute{"concept:name", "A", AttributeKind::kString});
  const auto typed_cache_first = apply_typed_trace(
      trace_with_activity_attribute(EventAttribute{"concept:name", "A",
                                                    AttributeKind::kString}),
      std::vector<std::string>{"a"}, std::set<std::string>{},
      EditDistanceTypedTraceMapping{{typed_cache_key, "a"}}, &typed_cache);
  const auto typed_cache_hit = apply_typed_trace(
      typed_string_cache_trace, std::vector<std::string>{},
      std::set<std::string>{},
      EditDistanceTypedTraceMapping{{typed_cache_key, "a"}}, &typed_cache);
  assert(typed_cache.size() == 1 && typed_cache_hit == typed_cache_first);
  const auto typed_opaque = std::make_shared<std::any>(std::string("typed opaque"));
  EditDistanceTraceCache typed_opaque_cache{{"a", typed_opaque}};
  const auto typed_opaque_hit = apply_typed_trace(
      typed_string_cache_trace, std::vector<std::string>{},
      std::set<std::string>{},
      EditDistanceTypedTraceMapping{{typed_cache_key, "a"}},
      &typed_opaque_cache);
  assert(typed_opaque_hit == typed_opaque &&
         std::any_cast<const std::string&>(*typed_opaque_hit) == "typed opaque");
  EditDistanceTraceCache typed_cached_none{{"a", nullptr}};
  const auto typed_null_hit = apply_typed_trace(
      typed_string_cache_trace, std::vector<std::string>{},
      std::set<std::string>{},
      EditDistanceTypedTraceMapping{{typed_cache_key, "a"}},
      &typed_cached_none);
  assert(!typed_null_hit && typed_cached_none.size() == 1);

  // Hashing occurs before cache lookup for both helpers, so present lists
  // cannot masquerade as string activities or bypass the failure in cache.
  EventAttribute unhashable_activity{"concept:name", "", AttributeKind::kString};
  unhashable_activity.container = AttributeContainerKind::kList;
  unhashable_activity.children.push_back(
      EventAttribute{"0", "A", AttributeKind::kString});
  const Trace unhashable_trace =
      trace_with_activity_attribute(unhashable_activity);
  EditDistanceTraceCache unhashable_cache{{"", typed_opaque}};
  bool unhashable_threw = false;
  try {
    (void)apply_typed_trace(
        unhashable_trace, std::vector<std::string>{}, std::set<std::string>{},
        EditDistanceTypedTraceMapping{{typed_string_key, "a"}},
        &unhashable_cache);
  } catch (const std::invalid_argument& error) {
    unhashable_threw = std::string(error.what()) == "unhashable type: 'list'";
  }
  assert(unhashable_threw && unhashable_cache.size() == 1);
  unhashable_threw = false;
  try {
    (void)apply_edit_distance_trace(
        unhashable_trace, std::vector<std::string>{}, std::set<std::string>{},
        EditDistanceTraceMapping{{"A", "a"}}, &unhashable_cache);
  } catch (const std::invalid_argument& error) {
    unhashable_threw = std::string(error.what()) == "unhashable type: 'list'";
  }
  assert(unhashable_threw && unhashable_cache.size() == 1);

  // The exact-match set is independent of the ordered target list.
  const auto set_only_exact = apply_trace(
      direct_trace, std::vector<std::string>{"ac"}, direct_exact_set,
      direct_mapping);
  assert(set_only_exact &&
         std::any_cast<const EditDistanceTraceResult&>(*set_only_exact).cost ==
             0);

  // A present cache key wins before target selection and returns the same
  // type-erased object, including when that value is not our result type.
  const auto opaque = std::make_shared<std::any>(std::string("opaque"));
  EditDistanceTraceCache opaque_cache{{"ab", opaque}};
  EditDistanceOptions direct_anti;
  direct_anti.perform_anti_alignment = true;
  const auto opaque_hit = align_trace(
      direct_trace, std::vector<std::string>{}, std::set<std::string>{},
      direct_mapping, &opaque_cache, direct_anti);
  assert(opaque_hit == opaque && opaque_cache.size() == 1 &&
         std::any_cast<const std::string&>(*opaque_hit) == "opaque");
  EditDistanceTraceCache cached_none{{"ab", nullptr}};
  const auto null_hit = apply_trace(
      direct_trace, std::vector<std::string>{}, std::set<std::string>{},
      direct_mapping, &cached_none);
  assert(!null_hit && cached_none.size() == 1);

  // Mapping collisions reverse-decode to the last activity, and labels that
  // have no encoding are omitted from the concatenated source key.
  const Trace collision_trace =
      log_of({{"A", "unmapped", "A"}}).traces.front();
  const EditDistanceTraceMapping collision_mapping{{"A", "x"},
                                                    {"B", "x"}};
  const auto collision = apply_edit_distance_trace(
      collision_trace, std::vector<std::string>{"xx"}, std::set<std::string>{},
      collision_mapping);
  const auto& collision_result =
      std::any_cast<const EditDistanceTraceResult&>(*collision);
  assert(collision_result.cost == 0 &&
         collision_result.alignment.size() == 2 &&
         collision_result.alignment[0].log_activity == "B" &&
         collision_result.alignment[1].model_activity == "B");

  // Unicode supplementary-plane mapping values are one edit token each.
  const std::string face("\xF0\x9F\x98\x80");
  const std::string rocket("\xF0\x9F\x9A\x80");
  const std::string unicode_encoding = face + rocket;
  const EditDistanceTraceMapping unicode_mapping{{"A", face}, {"B", rocket}};
  const auto unicode_result_handle = apply_edit_distance_trace(
      direct_trace, std::vector<std::string>{unicode_encoding},
      std::set<std::string>{unicode_encoding}, unicode_mapping);
  const auto& unicode_result =
      std::any_cast<const EditDistanceTraceResult&>(*unicode_result_handle);
  assert(unicode_result.cost == 0 && unicode_result.alignment.size() == 2 &&
         unicode_result.alignment[0].log_activity == "A" &&
         unicode_result.alignment[1].model_activity == "B");

  // Equal-distance variants retain first list occurrence for both ordinary
  // and anti alignment; reversing the list changes the selected activity.
  const Trace one_activity = log_of({{"A"}}).traces.front();
  const EditDistanceTraceMapping tie_mapping{{"A", "a"}, {"B", "b"},
                                              {"C", "c"}};
  auto tied = apply_edit_distance_trace(
      one_activity, std::vector<std::string>{"b", "c"},
      std::set<std::string>{}, tie_mapping);
  auto tied_reversed = apply_edit_distance_trace(
      one_activity, std::vector<std::string>{"c", "b"},
      std::set<std::string>{}, tie_mapping);
  assert(std::any_cast<const EditDistanceTraceResult&>(*tied)
             .alignment.back().model_activity == "B" &&
         std::any_cast<const EditDistanceTraceResult&>(*tied_reversed)
                 .alignment.back().model_activity == "C");
  direct_anti.perform_anti_alignment = true;
  auto anti_tied = apply_edit_distance_trace(
      one_activity, std::vector<std::string>{"b", "c"},
      std::set<std::string>{}, tie_mapping, nullptr, direct_anti);
  assert(std::any_cast<const EditDistanceTraceResult&>(*anti_tied)
             .alignment.back().model_activity == "B");

  const Trace three_activities = log_of({{"A", "B", "C"}}).traces.front();
  const EditDistanceTraceMapping length_tie_mapping{
      {"A", "a"}, {"B", "b"}, {"C", "c"}, {"D", "d"}};
  const auto ordinary_short_first = apply_edit_distance_trace(
      three_activities, std::vector<std::string>{"ab", "abcd"},
      std::set<std::string>{}, length_tie_mapping);
  const auto ordinary_long_first = apply_edit_distance_trace(
      three_activities, std::vector<std::string>{"abcd", "ab"},
      std::set<std::string>{}, length_tie_mapping);
  assert(std::any_cast<const EditDistanceTraceResult&>(*ordinary_short_first)
             .alignment.back().type == EditDistanceMoveType::kLog &&
         std::any_cast<const EditDistanceTraceResult&>(*ordinary_long_first)
                 .alignment.back().model_activity == "D");
  const auto anti_length_tie_short_first = apply_edit_distance_trace(
      three_activities, std::vector<std::string>{"ab", "abcd"},
      std::set<std::string>{}, length_tie_mapping, nullptr, direct_anti);
  const auto anti_length_tie_long_first = apply_edit_distance_trace(
      three_activities, std::vector<std::string>{"abcd", "ab"},
      std::set<std::string>{}, length_tie_mapping, nullptr, direct_anti);
  assert(std::any_cast<const EditDistanceTraceResult&>(
             *anti_length_tie_short_first).alignment.back().model_activity ==
             "D" &&
         std::any_cast<const EditDistanceTraceResult&>(
             *anti_length_tie_long_first).alignment.back().model_activity ==
             "D");

  // Empty ordinary variants are valid; no candidate and anti alignment with
  // no positive-distance candidate follow the captured failure contract.
  const Trace direct_empty_trace = log_of({{}}).traces.front();
  const auto empty_exact = apply_edit_distance_trace(
      direct_empty_trace, std::vector<std::string>{""}, std::set<std::string>{""},
      EditDistanceTraceMapping{{"A", "a"}});
  assert(std::any_cast<const EditDistanceTraceResult&>(*empty_exact)
             .alignment.empty() &&
         std::any_cast<const EditDistanceTraceResult&>(*empty_exact).cost == 0);
  bool direct_threw = false;
  try {
    (void)apply_edit_distance_trace(direct_empty_trace, std::vector<std::string>{},
                                    std::set<std::string>{},
                                    EditDistanceTraceMapping{});
  } catch (const std::invalid_argument&) {
    direct_threw = true;
  }
  assert(direct_threw);
  direct_threw = false;
  try {
    (void)apply_edit_distance_trace(
        one_activity, std::vector<std::string>{"?"}, std::set<std::string>{},
        EditDistanceTraceMapping{{"A", "a"}});
  } catch (const std::out_of_range&) {
    direct_threw = true;
  }
  assert(direct_threw);
  Trace missing_activity_trace;
  Event custom_activity_event;
  custom_activity_event.attributes.push_back(
      {"task:name", "A", AttributeKind::kString});
  missing_activity_trace.events.push_back(std::move(custom_activity_event));
  direct_threw = false;
  try {
    (void)apply_edit_distance_trace(
        missing_activity_trace, direct_exact_target, direct_exact_set,
        direct_mapping);
  } catch (const std::out_of_range&) {
    direct_threw = true;
  }
  assert(direct_threw);
  EditDistanceOptions custom_activity_options;
  custom_activity_options.activity_attribute = "task:name";
  const auto custom_attribute_value = apply_edit_distance_trace(
      missing_activity_trace, std::vector<std::string>{"a"},
      std::set<std::string>{}, EditDistanceTraceMapping{{"A", "a"}}, nullptr,
      custom_activity_options);
  assert(std::any_cast<const EditDistanceTraceResult&>(
             *custom_attribute_value).cost == 0);

  Trace source_key_trace;
  Event source_key_event;
  source_key_event.activity = "A";
  source_key_event.activity_state = FirstClassAttributeState::kPresent;
  source_key_event.activity_source_key = "task:name";
  source_key_trace.events.push_back(std::move(source_key_event));
  direct_threw = false;
  try {
    (void)apply_edit_distance_trace(
        source_key_trace, std::vector<std::string>{"a"},
        std::set<std::string>{}, EditDistanceTraceMapping{{"A", "a"}});
  } catch (const std::out_of_range&) {
    direct_threw = true;
  }
  assert(direct_threw);
  const auto source_key_value = apply_edit_distance_trace(
      source_key_trace, std::vector<std::string>{"a"}, std::set<std::string>{},
      EditDistanceTraceMapping{{"A", "a"}}, nullptr,
      custom_activity_options);
  assert(std::any_cast<const EditDistanceTraceResult&>(*source_key_value).cost ==
         0);
  EditDistanceOptions empty_selector_options;
  empty_selector_options.activity_attribute.clear();
  direct_threw = false;
  try {
    (void)apply_edit_distance_trace(
        one_activity, std::vector<std::string>{"a"}, std::set<std::string>{},
        EditDistanceTraceMapping{{"A", "a"}}, nullptr, empty_selector_options);
  } catch (const std::out_of_range&) {
    direct_threw = true;
  }
  assert(direct_threw);
  source_key_trace.events[0].activity.clear();
  source_key_trace.events[0].activity_state = FirstClassAttributeState::kMissing;
  direct_threw = false;
  try {
    (void)apply_edit_distance_trace(
        source_key_trace, std::vector<std::string>{"a"}, std::set<std::string>{},
        EditDistanceTraceMapping{{"", "a"}}, nullptr, custom_activity_options);
  } catch (const std::out_of_range&) {
    direct_threw = true;
  }
  assert(direct_threw);
  direct_threw = false;
  try {
    (void)apply_edit_distance_trace(
        one_activity, std::vector<std::string>{"a"}, std::set<std::string>{},
        EditDistanceTraceMapping{{"A", "a"}}, nullptr, direct_anti);
  } catch (const std::invalid_argument&) {
    direct_threw = true;
  }
  assert(direct_threw);

  const EventLog source = log_of({{"a", "b"}, {"a", "x"}, {}});
  const EventLog target = log_of({{"a", "b"}, {"a", "c"}, {"a"}});
  const auto exact = align_edit_distance(log_of({{"a", "b"}}), target);
  assert(exact.size() == 1 && exact[0].cost == 0 && exact[0].fitness == 1.0);

  const auto deviations = align_edit_distance(log_of({{"a", "z"}}), log_of({{"a", "b"}}));
  assert(deviations[0].cost == 20000);
  assert(deviations[0].alignment.size() == 3);

  const auto repeated = align_edit_distance(log_of({{"a", "b", "a"}}),
                                            log_of({{"a", "a", "b", "a"}}));
  assert(repeated[0].cost == 10000);
  assert(repeated[0].alignment.front().type == EditDistanceMoveType::kModel);
  assert(repeated[0].alignment.front().model_activity == "a");

  // SequenceMatcher autojunk remains deterministic on long repeated traces;
  // exact-target matching is still synchronous.
  EventLog long_target;
  Trace long_trace;
  for (std::size_t i = 0; i < 240; ++i)
    long_trace.events.push_back(Event{"long", "repeat", "same", {}, i});
  long_target.traces.push_back(long_trace);
  const auto long_exact = align_edit_distance(long_target, long_target);
  assert(long_exact[0].cost == 0 && long_exact[0].alignment.size() == 240);
  const auto long_deviation = align_edit_distance(log_of({{"repeat"}}), long_target);
  assert(long_deviation[0].cost == 239 * 10000);

  bool threw = false;
  // Equal distances prefer the shorter target, then the last distinct target
  // in first-seen order when lengths match.
  const auto tie = align_edit_distance(log_of({{"a"}}), log_of({{"b"}, {"c"}}));
  assert(tie[0].target_variant == std::vector<std::string>{"c"});
  const auto tie_reversed =
      align_edit_distance(log_of({{"a"}}), log_of({{"c"}, {"b"}}));
  const auto tie_shorter =
      align_edit_distance(log_of({{"a"}}), log_of({{"a", "b"}, {"b"}}));
  const auto tie_shorter_reversed =
      align_edit_distance(log_of({{"a"}}), log_of({{"b"}, {"a", "b"}}));
  const auto tie_duplicate =
      align_edit_distance(log_of({{"a"}}), log_of({{"b"}, {"c"}, {"b"}}));
  assert(tie_reversed[0].target_variant == std::vector<std::string>{"b"});
  assert(tie_shorter[0].target_variant == std::vector<std::string>{"b"});
  assert(tie_shorter_reversed[0].target_variant ==
         std::vector<std::string>{"b"});
  assert(tie_duplicate[0].target_variant == std::vector<std::string>{"c"});
  EditDistanceOptions anti;
  anti.perform_anti_alignment = true;
  const auto far = align_edit_distance(log_of({{"a"}}), log_of({{"a"}, {"b", "c"}}), anti);
  assert(far[0].target_variant == std::vector<std::string>({"b", "c"}));
  const auto anti_tie_bc =
      align_edit_distance(log_of({{"a"}}), log_of({{"b"}, {"c"}}), anti);
  const auto anti_tie_cb =
      align_edit_distance(log_of({{"a"}}), log_of({{"c"}, {"b"}}), anti);
  assert(anti_tie_bc[0].target_variant == std::vector<std::string>{"c"} &&
         anti_tie_bc[0].target_index == 1);
  assert(anti_tie_cb[0].target_variant == std::vector<std::string>{"b"} &&
         anti_tie_cb[0].target_index == 1);
  const auto anti_tie_long_first = align_edit_distance(
      log_of({{"a"}}), log_of({{"a", "b"}, {"b"}}), anti);
  const auto anti_tie_short_first = align_edit_distance(
      log_of({{"a"}}), log_of({{"b"}, {"a", "b"}}), anti);
  const auto anti_tie_empty_first =
      align_edit_distance(log_of({{"a"}}), log_of({{}, {"a", "b"}}), anti);
  const auto anti_tie_empty_last =
      align_edit_distance(log_of({{"a"}}), log_of({{"a", "b"}, {}}), anti);
  const auto anti_tie_long_suffix_first = align_edit_distance(
      log_of({{"a", "b"}}), log_of({{"a"}, {"a", "b", "c"}}), anti);
  const auto anti_tie_long_suffix_last = align_edit_distance(
      log_of({{"a", "b"}}), log_of({{"a", "b", "c"}, {"a"}}), anti);
  assert(anti_tie_long_first[0].target_variant ==
             std::vector<std::string>({"a", "b"}) &&
         anti_tie_short_first[0].target_variant ==
             std::vector<std::string>({"a", "b"}) &&
         anti_tie_empty_first[0].target_variant ==
             std::vector<std::string>({"a", "b"}) &&
         anti_tie_empty_last[0].target_variant ==
             std::vector<std::string>({"a", "b"}) &&
         anti_tie_long_suffix_first[0].target_variant ==
             std::vector<std::string>({"a", "b", "c"}) &&
         anti_tie_long_suffix_last[0].target_variant ==
             std::vector<std::string>({"a", "b", "c"}));
  threw = false;
  try {
    (void)align_edit_distance(log_of({{"a"}}), log_of({{"a"}}), anti);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)align_edit_distance(log_of({{}}), log_of({{}, {}}), anti);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  const auto duplicate = align_edit_distance(log_of({{"a"}, {"a"}, {"b"}}),
                                             log_of({{"a"}, {"a"}}));
  assert(duplicate.size() == 3 && duplicate[0].target_index == duplicate[1].target_index);
  assert(duplicate[2].target_variant == std::vector<std::string>{"a"});

  assert(align_edit_distance(EventLog{}, target).empty());
  threw = false;
  try { (void)align_edit_distance(EventLog{}, EventLog{}); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  threw = false;
  try { (void)align_edit_distance(source, EventLog{}); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  const auto empty_trace = align_edit_distance(log_of({{}}), log_of({{"a"}}));
  assert(empty_trace[0].cost == 10000 && empty_trace[0].alignment.size() == 1);

  auto projection_source = log_of({{"a", "x"}, {}});
  projection_source.traces[0].events[0].timestamp_source_key =
      "business.when";
  const auto projected =
      project_log_on_variant(projection_source, {"a", "b"});
  assert(projected.traces.size() == 1);
  assert(projected.attribute("@@aligned_variant") != nullptr);
  assert(projected.traces[0].attribute("@@original_trace") != nullptr);
  assert(projected.traces[0].events.size() == 2);
  assert(projected.traces[0].events[0].attribute("@@is_conforming")->compare("true") == 0);
  assert(projected.traces[0].events[1].attribute("@@is_conforming")->compare("false") == 0);
  assert(projected.traces[0].events[0].timestamp == "t0");
  assert(projected.traces[0].events[1].timestamp == "t0");
  assert(projected.traces[0].events[1].timestamp_source_key ==
         "time:timestamp");
  assert(projected.traces[0].events[0].input_order == 0 &&
         projected.traces[0].events[1].input_order == 1);
  assert(projected.traces[0].attributes.size() == 1);
  assert(projected.traces[0].events[0].attributes.size() == 3);
  assert(projected.traces[0].events[1].attributes.size() == 3);

  const auto empty_variant_projection =
      project_log_on_variant(log_of({{"a"}}), {});
  assert(empty_variant_projection.attribute("@@aligned_variant") != nullptr &&
         *empty_variant_projection.attribute("@@aligned_variant") == "");
  assert(empty_variant_projection.traces.size() == 1 &&
         empty_variant_projection.traces[0].events.empty());
  const auto empty_log_projection = project_log_on_variant(EventLog{}, {"a"});
  assert(empty_log_projection.traces.empty() &&
         empty_log_projection.attribute("@@aligned_variant") != nullptr);

  EventLog marked = log_of({{"a"}});
  marked.attributes.push_back({"owner", "kept", AttributeKind::kString});
  marked.attributes.push_back({"@@aligned_variant", "old"});
  marked.attributes.push_back({"@@aligned_variant", "older"});
  marked.traces[0].attributes.push_back({"@@original_trace", "old"});
  marked.traces[0].attributes.push_back({"@@original_trace", "older"});
  marked.traces[0].properties.push_back({"source", "trace", AttributeKind::kString});
  marked.traces[0].events[0].attributes.push_back({"@@is_conforming", "old"});
  marked.traces[0].events[0].attributes.push_back({"@@is_conforming", "older"});
  marked.traces[0].events[0].attributes.push_back({"amount", "7", AttributeKind::kInt});
  marked.metadata.properties.push_back({"source", "table", AttributeKind::kString});
  const auto marked_projection = project_log_on_variant(marked, {"a"});
  assert(marked_projection.attribute("owner") == nullptr);
  assert(marked_projection.attribute("@@aligned_variant")->compare("a") == 0);
  assert(marked_projection.attributes.size() == 1);
  assert(marked_projection.metadata == EventDataMetadata{});
  assert(marked_projection.traces[0].attribute("@@original_trace")->compare("a") == 0);
  assert(marked_projection.traces[0].attributes.size() == 1 &&
         marked_projection.traces[0].properties.empty() &&
         marked_projection.traces[0].case_id.empty());
  assert(marked_projection.traces[0].events[0].attribute("@@is_conforming")->compare("true") == 0);
  assert(marked_projection.traces[0].events[0].attribute("amount") == nullptr);
  assert(marked_projection.traces[0].events[0].attributes.size() == 3);
  assert(std::count_if(marked_projection.attributes.begin(), marked_projection.attributes.end(),
                       [](const auto& a) { return a.key == "@@aligned_variant"; }) == 1);
  assert(std::count_if(marked_projection.traces[0].attributes.begin(), marked_projection.traces[0].attributes.end(),
                       [](const auto& a) { return a.key == "@@original_trace"; }) == 1);
  assert(std::count_if(marked_projection.traces[0].events[0].attributes.begin(), marked_projection.traces[0].events[0].attributes.end(),
                       [](const auto& a) { return a.key == "@@is_conforming"; }) == 1);

  // Log projection must use the selected target independently for each
  // source trace; heterogeneous source logs must not be rejected.
  EventLog heterogeneous_source = log_of({{"a"}, {"b"}});
  EventLog heterogeneous_target = log_of({{"a"}, {"b"}});
  heterogeneous_source.traces[0].events[0].resource = "alice";
  heterogeneous_source.traces[0].events[0].lifecycle = "complete";
  heterogeneous_source.traces[0].events[0].attributes.push_back(
      {"amount", "7", AttributeKind::kInt});
  heterogeneous_source.traces[0].events[0].attributes.back().typed_value =
      std::int64_t{7};
  EventAttribute nested{"payload", "", AttributeKind::kString};
  nested.container = AttributeContainerKind::kContainer;
  nested.children.push_back({"label", "kept"});
  heterogeneous_source.traces[0].events[0].attributes.push_back(
      std::move(nested));
  EventAttribute nan{"nan", "NaN", AttributeKind::kFloat};
  nan.typed_value = std::numeric_limits<double>::quiet_NaN();
  heterogeneous_source.traces[0].events[0].attributes.push_back(
      std::move(nan));
  EventAttribute positive_infinity{"positive", "INF",
                                   AttributeKind::kFloat};
  positive_infinity.typed_value = std::numeric_limits<double>::infinity();
  heterogeneous_source.traces[0].events[0].attributes.push_back(
      std::move(positive_infinity));
  EventAttribute negative_infinity{"negative", "-INF",
                                   AttributeKind::kFloat};
  negative_infinity.typed_value = -std::numeric_limits<double>::infinity();
  heterogeneous_source.traces[0].events[0].attributes.push_back(
      std::move(negative_infinity));
  heterogeneous_source.traces[0].case_id_source_key = "business.case";
  heterogeneous_source.traces[0].events[0].activity_source_key =
      "business.activity";
  heterogeneous_source.traces[0].events[0].timestamp_source_key =
      "business.when";
  heterogeneous_source.metadata.globals.push_back(
      {"custom", {{"ordered", "yes"}}});
  const auto heterogeneous_alignments =
      align_edit_distance(heterogeneous_source, heterogeneous_target);
  const auto heterogeneous_projection =
      project_log_on_alignments(heterogeneous_source, heterogeneous_alignments);
  assert(heterogeneous_projection.traces.size() == 2);
  assert(heterogeneous_projection.traces[0].attribute("@@aligned_variant") != nullptr);
  assert(heterogeneous_projection.traces[0].attribute("@@aligned_variant")->compare("a") == 0);
  assert(heterogeneous_projection.traces[1].attribute("@@aligned_variant")->compare("b") == 0);
  assert(heterogeneous_projection.traces[0].events[0].resource == "alice");
  assert(heterogeneous_projection.traces[0].events[0].lifecycle == "complete");
  assert(heterogeneous_projection.traces[0].events[0].input_order == 0);
  const auto projected_json = edit_distance_json(
      heterogeneous_alignments, false, &heterogeneous_projection);
  assert(projected_json.find("\"resource\":\"alice\"") != std::string::npos);
  assert(projected_json.find("\"lifecycle\":\"complete\"") != std::string::npos);
  assert(projected_json.find("\"input_order\":0") != std::string::npos);
  assert(projected_json.find("\"attribute_kinds\":{\"amount\":\"int\"") != std::string::npos);
  assert(projected_json.find("\"attribute_details\":[") !=
         std::string::npos);
  assert(projected_json.find("\"container\":\"container\"") !=
         std::string::npos);
  assert(projected_json.find("\"typed_value\":7") != std::string::npos);
  assert(projected_json.find("\"typed_value\":\"NaN\"") !=
         std::string::npos);
  assert(projected_json.find("\"typed_value\":\"INF\"") !=
         std::string::npos);
  assert(projected_json.find("\"typed_value\":\"-INF\"") !=
         std::string::npos);
  assert(projected_json.find(
             "\"globals\":[{\"scope\":\"custom\",\"attributes\":[") !=
         std::string::npos);
  assert(projected_json.find("\"case_id_source_key\":\"business.case\"") !=
         std::string::npos);
  assert(projected_json.find(
             "\"activity_source_key\":\"business.activity\"") !=
         std::string::npos);
  assert(projected_json.find(
             "\"timestamp_source_key\":\"business.when\"") !=
         std::string::npos);

  EventLog empty_source;
  empty_source.attributes.push_back({"@@aligned_variant", "stale", AttributeKind::kString});
  empty_source.attributes.push_back({"@@aligned_variants", "stale", AttributeKind::kString});
  const auto empty_projection = project_log_on_alignments(empty_source, {});
  assert(empty_projection.attribute("@@aligned_variant") == nullptr);
  assert(empty_projection.attribute("@@aligned_variants") == nullptr);

  // Typed selectors and their presence/source metadata must survive a model
  // insertion.  The inserted event inherits the timestamp cell that anchors
  // the projected trace, including its XES type and native value.
  EventLog typed_source;
  Trace typed_trace;
  typed_trace.case_id = "42";
  typed_trace.case_id_state = FirstClassAttributeState::kPresent;
  typed_trace.case_id_source_key = "case.number";
  EventAttribute selected_case{"case.number", "42", AttributeKind::kInt};
  selected_case.typed_value = std::int64_t{42};
  typed_trace.case_id_attribute = selected_case;
  Event typed_event;
  typed_event.case_id = "42";
  typed_event.case_id_state = FirstClassAttributeState::kPresent;
  typed_event.case_id_source_key = "case.number";
  typed_event.activity = "a";
  typed_event.activity_state = FirstClassAttributeState::kPresent;
  typed_event.activity_source_key = "activity.code";
  EventAttribute selected_activity{"activity.code", "7", AttributeKind::kInt};
  selected_activity.typed_value = std::int64_t{7};
  typed_event.activity_attribute = selected_activity;
  typed_event.timestamp = "2026-01-01T00:00:00Z";
  typed_event.timestamp_state = FirstClassAttributeState::kPresent;
  typed_event.timestamp_source_key = "when.value";
  EventAttribute selected_timestamp{
      "when.value", typed_event.timestamp, AttributeKind::kDate};
  selected_timestamp.typed_value =
      NativeDate{"2026-01-01T00:00:00.000Z"};
  typed_event.timestamp_attribute = selected_timestamp;
  typed_trace.events.push_back(std::move(typed_event));
  typed_source.traces.push_back(std::move(typed_trace));

  const auto typed_target = log_of({{"x", "a"}});
  const auto typed_alignments = align_edit_distance(typed_source, typed_target);
  const auto typed_projection =
      project_log_on_alignments(typed_source, typed_alignments);
  const auto& projected_typed_trace = typed_projection.traces.front();
  assert(projected_typed_trace.case_id_attribute &&
         projected_typed_trace.case_id_attribute->kind == AttributeKind::kInt &&
         std::get<std::int64_t>(
             projected_typed_trace.case_id_attribute->typed_value) == 42);
  assert(projected_typed_trace.events.size() == 2);
  const auto& inserted = projected_typed_trace.events.front();
  assert(inserted.activity == "x" &&
         inserted.case_id_state == FirstClassAttributeState::kPresent &&
         inserted.case_id_source_key == "case.number" &&
         inserted.activity_source_key == "activity.code" &&
         inserted.activity_attribute &&
         inserted.activity_attribute->kind == AttributeKind::kString &&
         inserted.activity_attribute->value == "x" &&
         std::holds_alternative<std::monostate>(
             inserted.activity_attribute->typed_value));
  assert(inserted.timestamp == "2026-01-01T00:00:00Z" &&
         inserted.timestamp_state == FirstClassAttributeState::kPresent &&
         inserted.timestamp_source_key == "when.value" &&
         inserted.timestamp_attribute &&
         inserted.timestamp_attribute->kind == AttributeKind::kDate &&
         std::get<NativeDate>(inserted.timestamp_attribute->typed_value).value ==
             "2026-01-01T00:00:00.000Z");
  const auto typed_xes = to_xes(typed_projection);
  assert(typed_xes.find("<int key=\"case.number\" value=\"42\"/>") !=
         std::string::npos);
  assert(typed_xes.find("<string key=\"activity.code\" value=\"x\"/>") !=
         std::string::npos);
  assert(typed_xes.find(
             "<date key=\"when.value\" value=\"2026-01-01T00:00:00+00:00\"/>") !=
         std::string::npos);

  const auto fresh_typed_projection =
      project_log_on_variant(typed_source, {"x", "a"});
  assert(fresh_typed_projection.metadata == EventDataMetadata{} &&
         fresh_typed_projection.attributes.size() == 1);
  assert(fresh_typed_projection.traces.front().case_id.empty() &&
         !fresh_typed_projection.traces.front().case_id_attribute &&
         fresh_typed_projection.traces.front().attributes.size() == 1);
  const auto& fresh_inserted =
      fresh_typed_projection.traces.front().events.front();
  assert(fresh_inserted.attributes.size() == 3 &&
         fresh_inserted.attribute("case.number") == nullptr &&
         fresh_inserted.attribute("activity.code") == nullptr &&
         fresh_inserted.attribute("@@is_conforming") != nullptr &&
         fresh_inserted.timestamp_attribute &&
         fresh_inserted.timestamp_attribute->kind == AttributeKind::kDate &&
         std::get<NativeDate>(fresh_inserted.timestamp_attribute->typed_value)
                 .value == "2026-01-01T00:00:00.000Z");

  // A non-default activity key drives source/target variants, and the same
  // selected generic cells are materialized on inserted model events.
  EventLog selected_source;
  Trace selected_source_trace;
  Event selected_source_event{"s", "ignored", "canonical"};
  selected_source_event.attributes.push_back(
      {"phase", "a", AttributeKind::kInt});
  selected_source_event.attributes.back().typed_value = std::int64_t{1};
  selected_source_event.attributes.push_back(
      {"when", "2026-02-01T00:00:00Z", AttributeKind::kDate});
  selected_source_event.attributes.back().typed_value =
      NativeDate{"2026-02-01T00:00:00.000Z"};
  selected_source_trace.events.push_back(std::move(selected_source_event));
  selected_source.traces.push_back(std::move(selected_source_trace));
  EventLog selected_target;
  Trace selected_target_trace;
  Event selected_target_a{"t", "ignored", "canonical"};
  selected_target_a.attributes.push_back({"phase", "a"});
  selected_target_a.attributes.push_back({"when", "target-a"});
  Event selected_target_b{"t", "ignored", "canonical"};
  selected_target_b.attributes.push_back({"phase", "b"});
  selected_target_b.attributes.push_back({"when", "target-b"});
  selected_target_trace.events.push_back(std::move(selected_target_a));
  selected_target_trace.events.push_back(std::move(selected_target_b));
  selected_target.traces.push_back(std::move(selected_target_trace));
  EditDistanceOptions selected_options;
  selected_options.activity_attribute = "phase";
  selected_options.timestamp_attribute = "when";
  const auto selected_alignments =
      align_edit_distance(selected_source, selected_target, selected_options);
  assert(selected_alignments.size() == 1 &&
         selected_alignments.front().target_variant ==
             std::vector<std::string>({"a", "b"}));
  const auto selected_projection = project_log_on_alignments(
      selected_source, selected_alignments, selected_options);
  assert(selected_projection.traces.front().events.size() == 2);
  const auto& selected_sync = selected_projection.traces.front().events[0];
  const auto& selected_inserted = selected_projection.traces.front().events[1];
  assert(selected_sync.attribute("phase") &&
         *selected_sync.attribute("phase") == "a" &&
         selected_sync.attributes.size() >= 3 &&
         selected_sync.attributes[0].key == "phase" &&
         selected_sync.attributes[0].kind == AttributeKind::kInt &&
         std::get<std::int64_t>(selected_sync.attributes[0].typed_value) == 1 &&
         selected_sync.attributes[1].key == "when" &&
         selected_sync.attributes[1].kind == AttributeKind::kDate &&
         std::get<NativeDate>(selected_sync.attributes[1].typed_value).value ==
             "2026-02-01T00:00:00.000Z");
  assert(selected_sync.timestamp_source_key == "when" &&
         selected_sync.timestamp_attribute &&
         selected_sync.timestamp_attribute->kind == AttributeKind::kDate &&
         std::get<NativeDate>(selected_sync.timestamp_attribute->typed_value).value ==
             "2026-02-01T00:00:00.000Z");
  assert(selected_inserted.activity == "b" &&
         selected_inserted.attribute("phase") &&
         *selected_inserted.attribute("phase") == "b");
  assert(selected_inserted.activity_source_key == "phase" &&
         selected_inserted.activity_attribute &&
         selected_inserted.activity_attribute->kind == AttributeKind::kString);
  assert(selected_inserted.attribute("when") &&
         *selected_inserted.attribute("when") ==
             "2026-02-01T00:00:00Z");
  assert(selected_inserted.timestamp_source_key == "when" &&
         selected_inserted.timestamp_attribute &&
         selected_inserted.timestamp_attribute->kind == AttributeKind::kDate);
  const auto selected_variant_projection = project_log_on_variant(
      selected_source, {"a", "b"}, selected_options);
  assert(selected_variant_projection.traces.front().events.size() == 2 &&
         selected_variant_projection.traces.front().events[1].attribute("phase") &&
         *selected_variant_projection.traces.front().events[1].attribute("phase") ==
             "b");
  EventLog timestamp_less;
  Trace timestamp_less_trace;
  timestamp_less_trace.events.push_back(Event{"c", "a"});
  timestamp_less.traces.push_back(std::move(timestamp_less_trace));
  const auto timestamp_less_alignments = align_edit_distance(
      timestamp_less, selected_target);
  const auto timestamp_less_projection = project_log_on_alignments(
      timestamp_less, timestamp_less_alignments);
  assert(timestamp_less_projection.traces.front().events.size() == 2 &&
         timestamp_less_projection.traces.front().events[0].timestamp.empty() &&
         timestamp_less_projection.traces.front().events[1].timestamp.empty());
  EditDistanceOptions missing_timestamp = selected_options;
  missing_timestamp.timestamp_attribute = "missing-when";
  threw = false;
  try {
    (void)project_log_on_alignments(selected_source, selected_alignments,
                                    missing_timestamp);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  EditDistanceOptions missing_activity = selected_options;
  missing_activity.activity_attribute = "missing-phase";
  threw = false;
  try {
    (void)align_edit_distance(selected_source, selected_target,
                              missing_activity);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  EditDistanceOptions blank_selector = selected_options;
  blank_selector.activity_attribute = " ";
  threw = false;
  try {
    (void)align_edit_distance(selected_source, selected_target,
                              blank_selector);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  EventLog null_activity = selected_source;
  null_activity.traces.front().events.front().attributes.front().is_null = true;
  threw = false;
  try {
    (void)align_edit_distance(null_activity, selected_target,
                              selected_options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  // EventDataView adapts interleaved rows in first-seen case order and uses
  // the configured case selector instead of requiring case:concept:name.
  EventTable flat_source;
  flat_source.columns = {"cid", "act", "when", "noise"};
  flat_source.attributes.push_back({"origin", "table", AttributeKind::kString});
  flat_source.metadata.properties.push_back(
      {"source", "dataframe", AttributeKind::kString});
  auto flat_row = [](std::string case_id, std::string activity,
                     std::string timestamp) {
    EventAttribute when{"when", timestamp, AttributeKind::kDate};
    when.typed_value = NativeDate{timestamp};
    EventTable::Row row;
    row.emplace_back(EventAttribute{"cid", std::move(case_id)});
    row.emplace_back(EventAttribute{"act", std::move(activity)});
    row.emplace_back(std::move(when));
    row.emplace_back(EventAttribute{"noise", "discard"});
    return row;
  };
  flat_source.rows.push_back(
      flat_row("case-z", "A", "2022-01-01T00:00:01Z"));
  flat_source.rows.push_back(
      flat_row("case-a", "B", "2022-01-02T00:00:01Z"));
  flat_source.rows.push_back(
      flat_row("case-z", "X", "2022-01-01T00:00:02Z"));
  flat_source.rows.push_back(
      flat_row("case-z", "B", "2022-01-01T00:00:03Z"));
  EventTable flat_target;
  flat_target.columns = {"cid", "act"};
  EventTable::Row target_a;
  target_a.emplace_back(EventAttribute{"cid", "model"});
  target_a.emplace_back(EventAttribute{"act", "A"});
  flat_target.rows.push_back(target_a);
  EventTable::Row target_b;
  target_b.emplace_back(EventAttribute{"cid", "model"});
  target_b.emplace_back(EventAttribute{"act", "B"});
  flat_target.rows.push_back(target_b);
  EditDistanceOptions flat_options;
  flat_options.case_id_attribute = "cid";
  flat_options.activity_attribute = "act";
  flat_options.timestamp_attribute = "when";
  const auto flat_alignments = apply(EventDataView(flat_source),
                                     EventDataView(flat_target), flat_options);
  assert(flat_alignments.size() == 2 &&
         flat_alignments[0].source_variant ==
             std::vector<std::string>({"A", "X", "B"}) &&
         flat_alignments[1].source_variant == std::vector<std::string>{"B"});
  const auto flat_projection = project_log_on_variant(
      EventDataView(flat_source), {"A", "B"}, flat_options);
  assert(flat_projection.traces.size() == 2 &&
         flat_projection.attributes.size() == 1 &&
         flat_projection.metadata == EventDataMetadata{});
  assert(flat_projection.traces[0].attribute("@@original_trace") &&
         *flat_projection.traces[0].attribute("@@original_trace") == "A,X,B" &&
         flat_projection.traces[1].attribute("@@original_trace") &&
         *flat_projection.traces[1].attribute("@@original_trace") == "B");
  const auto& flat_z = flat_projection.traces[0];
  assert(flat_z.attributes.size() == 1 && flat_z.events.size() == 2 &&
         flat_z.events[1].activity == "B" &&
         flat_z.events[1].timestamp == "2022-01-01T00:00:02Z" &&
         flat_z.events[1].attributes.size() == 3 &&
         flat_z.events[1].attribute("noise") == nullptr);

  EventTable empty_flat_table;
  const auto empty_flat_projection = project_log_on_variant(
      EventDataView(empty_flat_table), {"A", "B"}, flat_options);
  assert(empty_flat_projection.traces.empty() &&
         empty_flat_projection.attribute("@@aligned_variant") != nullptr);

  EventTable no_activity_column;
  no_activity_column.columns = {"cid", "when"};
  no_activity_column.rows.push_back(
      {EventAttribute{"cid", "case-z"},
       EventAttribute{"when", "2022-01-01T00:00:01Z"}});
  threw = false;
  try {
    (void)apply(EventDataView(no_activity_column),
                EventDataView(flat_target), flat_options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  EventTable no_timestamp_column;
  no_timestamp_column.columns = {"cid", "act"};
  EventTable::Row no_timestamp_row;
  no_timestamp_row.emplace_back(EventAttribute{"cid", "case-z"});
  no_timestamp_row.emplace_back(EventAttribute{"act", "A"});
  no_timestamp_column.rows.push_back(std::move(no_timestamp_row));
  threw = false;
  try {
    (void)project_log_on_variant(EventDataView(no_timestamp_column), {"A"},
                                 flat_options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  EventLog missing_first_time = log_of({{"A"}});
  auto& missing_first_event = missing_first_time.traces.front().events.front();
  missing_first_event.timestamp.clear();
  missing_first_event.timestamp_state = FirstClassAttributeState::kMissing;
  missing_first_event.timestamp_source_key.clear();
  missing_first_event.timestamp_attribute.reset();
  threw = false;
  try {
    (void)project_log_on_variant(missing_first_time, {"A"});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)project_log_on_variant(missing_first_time, {});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  EventLog missing_unused_logmove = log_of({{"A", "X"}});
  auto& unused_logmove = missing_unused_logmove.traces.front().events[1];
  unused_logmove.timestamp.clear();
  unused_logmove.timestamp_state = FirstClassAttributeState::kMissing;
  unused_logmove.timestamp_source_key.clear();
  unused_logmove.timestamp_attribute.reset();
  const auto missing_unused_projection =
      project_log_on_variant(missing_unused_logmove, {"A"});
  assert(missing_unused_projection.traces.front().events.size() == 1);

  EventLog missing_sync_after_logmove = log_of({{"A", "X", "B"}});
  auto& missing_cursor_event =
      missing_sync_after_logmove.traces.front().events[1];
  missing_cursor_event.timestamp.clear();
  missing_cursor_event.timestamp_state = FirstClassAttributeState::kMissing;
  missing_cursor_event.timestamp_source_key.clear();
  missing_cursor_event.timestamp_attribute.reset();
  threw = false;
  try {
    (void)project_log_on_variant(missing_sync_after_logmove, {"A", "B"});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);

  EventLog null_first_timestamp = log_of({{"A"}});
  auto& null_first_event = null_first_timestamp.traces.front().events.front();
  null_first_event.timestamp.clear();
  null_first_event.timestamp_state = FirstClassAttributeState::kNull;
  null_first_event.timestamp_source_key = "time:timestamp";
  EventAttribute null_timestamp{"time:timestamp", "", AttributeKind::kDate,
                                true};
  null_first_event.timestamp_attribute = null_timestamp;
  null_first_event.attributes.push_back(null_timestamp);
  const auto null_first_projection =
      project_log_on_variant(null_first_timestamp, {"A"});
  assert(null_first_projection.traces.front().events.front()
             .timestamp_attribute->is_null);
  const auto null_first_field = std::find_if(
      null_first_projection.traces.front().events.front().attributes.begin(),
      null_first_projection.traces.front().events.front().attributes.end(),
      [](const EventAttribute& attribute) {
        return attribute.key == "time:timestamp";
      });
  assert(null_first_field !=
             null_first_projection.traces.front().events.front().attributes.end() &&
         null_first_field->is_null);

  EventLog null_second_timestamp = log_of({{"A", "B"}});
  auto& null_second_event = null_second_timestamp.traces.front().events[1];
  null_second_event.timestamp.clear();
  null_second_event.timestamp_state = FirstClassAttributeState::kNull;
  null_second_event.timestamp_source_key = "time:timestamp";
  null_second_event.timestamp_attribute = null_timestamp;
  null_second_event.attributes.push_back(null_timestamp);
  const auto null_second_projection =
      project_log_on_variant(null_second_timestamp, {"A", "B"});
  assert(null_second_projection.traces.front().events[1]
             .timestamp_attribute->is_null);

  EditDistanceOptions null_custom_options;
  null_custom_options.timestamp_attribute = "custom";
  EventLog null_custom_timestamp = log_of({{"A"}});
  null_custom_timestamp.traces.front().events.front().attributes.push_back(
      {"custom", "", AttributeKind::kDate, true});
  const auto null_custom_projection = project_log_on_variant(
      null_custom_timestamp, {"A"}, null_custom_options);
  const auto null_custom_field = std::find_if(
      null_custom_projection.traces.front().events.front().attributes.begin(),
      null_custom_projection.traces.front().events.front().attributes.end(),
      [](const EventAttribute& attribute) {
        return attribute.key == "custom";
      });
  assert(null_custom_field !=
             null_custom_projection.traces.front().events.front().attributes.end() &&
         null_custom_field->is_null);

  EventTable nat_table;
  nat_table.columns = {"case:concept:name", "concept:name", "time:timestamp"};
  EventAttribute nat_timestamp{"time:timestamp", "", AttributeKind::kDate};
  nat_timestamp.typed_value = NativeDate{"NaT"};
  nat_table.rows.push_back(
      {EventAttribute{"case:concept:name", "case1"},
       EventAttribute{"concept:name", "A"}, nat_timestamp});
  const auto nat_projection =
      project_log_on_variant(EventDataView(nat_table), {"A"});
  assert(nat_projection.traces.front().events.front().timestamp_attribute &&
         std::holds_alternative<NativeDate>(
             nat_projection.traces.front().events.front()
                 .timestamp_attribute->typed_value) &&
         std::get<NativeDate>(nat_projection.traces.front().events.front()
                                  .timestamp_attribute->typed_value)
                 .value == "NaT");

  return 0;
}
