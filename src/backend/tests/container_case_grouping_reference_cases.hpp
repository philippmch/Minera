// Pinned public container grouping SHA256: 5884713b425bf4bb32da9b13f0354664174e1d3616665bec7e5e9c1213ac4870
#pragma once
#include "conversion_case_grouping_reference_cases.hpp"
namespace container_case_grouping_reference {
using namespace conversion_case_grouping_reference;
inline EventAttribute container_cell(std::string key, AttributeContainerKind kind, std::vector<EventAttribute> children) {
  EventAttribute value{std::move(key), ""};
  value.container = kind;
  value.children = std::move(children);
  return value;
}
inline void check_0() {
  EventStream input;
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {dtype_number("", std::int64_t{1})}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {dtype_number("", double{1.0})}), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {dtype_number("", true)}), EventAttribute{"concept:name", "A2"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {dtype_number("", std::int64_t{2})}), EventAttribute{"concept:name", "A3"}}});
  const auto before = input;
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("tuple_numeric: trace count differs");
  { const auto& trace = actual.traces[0];
    const auto expected_case = container_cell("concept:name", AttributeContainerKind::kTuple, {dtype_number("", std::int64_t{1})});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("tuple_numeric: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A0", "A1", "A2"}) throw std::runtime_error("tuple_numeric: membership/order differs"); }
  { const auto& trace = actual.traces[1];
    const auto expected_case = container_cell("concept:name", AttributeContainerKind::kTuple, {dtype_number("", std::int64_t{2})});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("tuple_numeric: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A3"}) throw std::runtime_error("tuple_numeric: membership/order differs"); }
  for (std::size_t i = 0; i < input.events.size(); ++i) if (input.events[i].fields != before.events[i].fields) throw std::runtime_error("input changed");
}
inline void check_1() {
  EventStream input;
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {dtype_number("", std::int64_t{1}), dtype_number("", std::int64_t{2})}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {dtype_number("", std::int64_t{2}), dtype_number("", std::int64_t{1})}), EventAttribute{"concept:name", "A1"}}});
  const auto before = input;
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("tuple_order: trace count differs");
  { const auto& trace = actual.traces[0];
    const auto expected_case = container_cell("concept:name", AttributeContainerKind::kTuple, {dtype_number("", std::int64_t{1}), dtype_number("", std::int64_t{2})});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("tuple_order: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A0"}) throw std::runtime_error("tuple_order: membership/order differs"); }
  { const auto& trace = actual.traces[1];
    const auto expected_case = container_cell("concept:name", AttributeContainerKind::kTuple, {dtype_number("", std::int64_t{2}), dtype_number("", std::int64_t{1})});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("tuple_order: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A1"}) throw std::runtime_error("tuple_order: membership/order differs"); }
  for (std::size_t i = 0; i < input.events.size(); ++i) if (input.events[i].fields != before.events[i].fields) throw std::runtime_error("input changed");
}
inline void check_2() {
  EventStream input;
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {container_cell("", AttributeContainerKind::kTuple, {dtype_number("", std::int64_t{1})}), dtype_number("", std::int64_t{2})}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {container_cell("", AttributeContainerKind::kTuple, {dtype_number("", true)}), dtype_number("", double{2.0})}), EventAttribute{"concept:name", "A1"}}});
  const auto before = input;
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 1) throw std::runtime_error("nested_tuple: trace count differs");
  { const auto& trace = actual.traces[0];
    const auto expected_case = container_cell("concept:name", AttributeContainerKind::kTuple, {container_cell("", AttributeContainerKind::kTuple, {dtype_number("", std::int64_t{1})}), dtype_number("", std::int64_t{2})});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("nested_tuple: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A0", "A1"}) throw std::runtime_error("nested_tuple: membership/order differs"); }
  for (std::size_t i = 0; i < input.events.size(); ++i) if (input.events[i].fields != before.events[i].fields) throw std::runtime_error("input changed");
}
inline void check_3() {
  EventStream input;
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kFrozenSet, {dtype_number("", std::int64_t{1}), dtype_number("", std::int64_t{2})}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kFrozenSet, {dtype_number("", true), dtype_number("", double{2.0})}), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kFrozenSet, {dtype_number("", std::int64_t{3})}), EventAttribute{"concept:name", "A2"}}});
  const auto before = input;
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("frozenset_numeric: trace count differs");
  { const auto& trace = actual.traces[0];
    const auto expected_case = container_cell("concept:name", AttributeContainerKind::kFrozenSet, {dtype_number("", std::int64_t{1}), dtype_number("", std::int64_t{2})});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("frozenset_numeric: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A0", "A1"}) throw std::runtime_error("frozenset_numeric: membership/order differs"); }
  { const auto& trace = actual.traces[1];
    const auto expected_case = container_cell("concept:name", AttributeContainerKind::kFrozenSet, {dtype_number("", std::int64_t{3})});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("frozenset_numeric: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A2"}) throw std::runtime_error("frozenset_numeric: membership/order differs"); }
  for (std::size_t i = 0; i < input.events.size(); ++i) if (input.events[i].fields != before.events[i].fields) throw std::runtime_error("input changed");
}
inline void check_4() {
  EventStream input;
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kFrozenSet, {}), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{EventAttribute{"case:concept:name", "", AttributeKind::kString}, EventAttribute{"concept:name", "A2"}}});
  input.events.push_back({{reference_null("case:concept:name"), EventAttribute{"concept:name", "A3"}}});
  const auto before = input;
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 4) throw std::runtime_error("empty_domains: trace count differs");
  { const auto& trace = actual.traces[0];
    const auto expected_case = container_cell("concept:name", AttributeContainerKind::kTuple, {});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("empty_domains: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A0"}) throw std::runtime_error("empty_domains: membership/order differs"); }
  { const auto& trace = actual.traces[1];
    const auto expected_case = container_cell("concept:name", AttributeContainerKind::kFrozenSet, {});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("empty_domains: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A1"}) throw std::runtime_error("empty_domains: membership/order differs"); }
  { const auto& trace = actual.traces[2];
    const auto expected_case = EventAttribute{"concept:name", "", AttributeKind::kString};
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("empty_domains: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A2"}) throw std::runtime_error("empty_domains: membership/order differs"); }
  { const auto& trace = actual.traces[3];
    const auto expected_case = reference_null("concept:name");
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("empty_domains: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A3"}) throw std::runtime_error("empty_domains: membership/order differs"); }
  for (std::size_t i = 0; i < input.events.size(); ++i) if (input.events[i].fields != before.events[i].fields) throw std::runtime_error("input changed");
}
inline void check_5() {
  EventStream input;
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {EventAttribute{"", "a:b", AttributeKind::kString}, EventAttribute{"", "c", AttributeKind::kString}}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {EventAttribute{"", "a", AttributeKind::kString}, EventAttribute{"", "b:c", AttributeKind::kString}}), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {EventAttribute{"", "a:b", AttributeKind::kString}, EventAttribute{"", "c", AttributeKind::kString}}), EventAttribute{"concept:name", "A2"}}});
  const auto before = input;
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("tuple_strings: trace count differs");
  { const auto& trace = actual.traces[0];
    const auto expected_case = container_cell("concept:name", AttributeContainerKind::kTuple, {EventAttribute{"", "a:b", AttributeKind::kString}, EventAttribute{"", "c", AttributeKind::kString}});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("tuple_strings: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A0", "A2"}) throw std::runtime_error("tuple_strings: membership/order differs"); }
  { const auto& trace = actual.traces[1];
    const auto expected_case = container_cell("concept:name", AttributeContainerKind::kTuple, {EventAttribute{"", "a", AttributeKind::kString}, EventAttribute{"", "b:c", AttributeKind::kString}});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("tuple_strings: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A1"}) throw std::runtime_error("tuple_strings: membership/order differs"); }
  for (std::size_t i = 0; i < input.events.size(); ++i) if (input.events[i].fields != before.events[i].fields) throw std::runtime_error("input changed");
}
inline void check_6() {
  EventStream input;
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {dtype_number("", std::int64_t{9007199254740992})}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {dtype_number("", std::int64_t{9007199254740993})}), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {dtype_number("", double{9007199254740992.0})}), EventAttribute{"concept:name", "A2"}}});
  const auto before = input;
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("tuple_wide: trace count differs");
  { const auto& trace = actual.traces[0];
    const auto expected_case = container_cell("concept:name", AttributeContainerKind::kTuple, {dtype_number("", std::int64_t{9007199254740992})});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("tuple_wide: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A0", "A2"}) throw std::runtime_error("tuple_wide: membership/order differs"); }
  { const auto& trace = actual.traces[1];
    const auto expected_case = container_cell("concept:name", AttributeContainerKind::kTuple, {dtype_number("", std::int64_t{9007199254740993})});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("tuple_wide: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A1"}) throw std::runtime_error("tuple_wide: membership/order differs"); }
  for (std::size_t i = 0; i < input.events.size(); ++i) if (input.events[i].fields != before.events[i].fields) throw std::runtime_error("input changed");
}
inline void check_7() {
  EventStream input;
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {reference_null("")}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {reference_null("")}), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {EventAttribute{"", "None", AttributeKind::kString}}), EventAttribute{"concept:name", "A2"}}});
  const auto before = input;
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("tuple_null: trace count differs");
  { const auto& trace = actual.traces[0];
    const auto expected_case = container_cell("concept:name", AttributeContainerKind::kTuple, {reference_null("")});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("tuple_null: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A0", "A1"}) throw std::runtime_error("tuple_null: membership/order differs"); }
  { const auto& trace = actual.traces[1];
    const auto expected_case = container_cell("concept:name", AttributeContainerKind::kTuple, {EventAttribute{"", "None", AttributeKind::kString}});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("tuple_null: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A2"}) throw std::runtime_error("tuple_null: membership/order differs"); }
  for (std::size_t i = 0; i < input.events.size(); ++i) if (input.events[i].fields != before.events[i].fields) throw std::runtime_error("input changed");
}
inline void check_8() {
  EventStream input;
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {container_cell("", AttributeContainerKind::kFrozenSet, {dtype_number("", std::int64_t{1}), dtype_number("", std::int64_t{2})})}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {container_cell("", AttributeContainerKind::kFrozenSet, {dtype_number("", true), dtype_number("", double{2.0})})}), EventAttribute{"concept:name", "A1"}}});
  const auto before = input;
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 1) throw std::runtime_error("nested_frozenset: trace count differs");
  { const auto& trace = actual.traces[0];
    const auto expected_case = container_cell("concept:name", AttributeContainerKind::kTuple, {container_cell("", AttributeContainerKind::kFrozenSet, {dtype_number("", std::int64_t{1}), dtype_number("", std::int64_t{2})})});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("nested_frozenset: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A0", "A1"}) throw std::runtime_error("nested_frozenset: membership/order differs"); }
  for (std::size_t i = 0; i < input.events.size(); ++i) if (input.events[i].fields != before.events[i].fields) throw std::runtime_error("input changed");
}
inline void check_9() {
  EventStream input;
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {dtype_number("", std::int64_t{1}), dtype_number("", std::int64_t{2})}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kFrozenSet, {dtype_number("", std::int64_t{1}), dtype_number("", std::int64_t{2})}), EventAttribute{"concept:name", "A1"}}});
  const auto before = input;
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("tuple_vs_frozenset: trace count differs");
  { const auto& trace = actual.traces[0];
    const auto expected_case = container_cell("concept:name", AttributeContainerKind::kTuple, {dtype_number("", std::int64_t{1}), dtype_number("", std::int64_t{2})});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("tuple_vs_frozenset: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A0"}) throw std::runtime_error("tuple_vs_frozenset: membership/order differs"); }
  { const auto& trace = actual.traces[1];
    const auto expected_case = container_cell("concept:name", AttributeContainerKind::kFrozenSet, {dtype_number("", std::int64_t{1}), dtype_number("", std::int64_t{2})});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("tuple_vs_frozenset: typed representative differs");
    std::vector<std::string> observed; for (const auto& event : trace.events) observed.push_back(event.activity);
    if (observed != std::vector<std::string>{"A1"}) throw std::runtime_error("tuple_vs_frozenset: membership/order differs"); }
  for (std::size_t i = 0; i < input.events.size(); ++i) if (input.events[i].fields != before.events[i].fields) throw std::runtime_error("input changed");
}
inline void check_10() {
  EventStream input;
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kList, {dtype_number("", std::int64_t{1})}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kList, {dtype_number("", std::int64_t{1})}), EventAttribute{"concept:name", "A1"}}});
  const auto before = input;
  bool rejected = false;
  try { (void)event_stream_to_event_log(input); } catch (const std::invalid_argument& error) { rejected = std::string(error.what()) == "unhashable type: 'list'"; }
  if (!rejected) throw std::runtime_error("list_rejected: expected rejection differs");
  for (std::size_t i = 0; i < input.events.size(); ++i) if (input.events[i].fields != before.events[i].fields) throw std::runtime_error("input changed");
}
inline void check_11() {
  EventStream input;
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kContainer, {container_cell("", AttributeContainerKind::kTuple, {EventAttribute{"", "a", AttributeKind::kString}, dtype_number("", std::int64_t{1})})}), EventAttribute{"concept:name", "A0"}}});
  const auto before = input;
  bool rejected = false;
  try { (void)event_stream_to_event_log(input); } catch (const std::invalid_argument& error) { rejected = std::string(error.what()) == "unhashable type: 'dict'"; }
  if (!rejected) throw std::runtime_error("dict_rejected: expected rejection differs");
  for (std::size_t i = 0; i < input.events.size(); ++i) if (input.events[i].fields != before.events[i].fields) throw std::runtime_error("input changed");
}
inline void check_12() {
  EventStream input;
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kSet, {dtype_number("", std::int64_t{1})}), EventAttribute{"concept:name", "A0"}}});
  const auto before = input;
  bool rejected = false;
  try { (void)event_stream_to_event_log(input); } catch (const std::invalid_argument& error) { rejected = std::string(error.what()) == "unhashable type: 'set'"; }
  if (!rejected) throw std::runtime_error("set_rejected: expected rejection differs");
  for (std::size_t i = 0; i < input.events.size(); ++i) if (input.events[i].fields != before.events[i].fields) throw std::runtime_error("input changed");
}
inline void check_13() {
  EventStream input;
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {container_cell("", AttributeContainerKind::kList, {dtype_number("", std::int64_t{1})})}), EventAttribute{"concept:name", "A0"}}});
  const auto before = input;
  bool rejected = false;
  try { (void)event_stream_to_event_log(input); } catch (const std::invalid_argument& error) { rejected = std::string(error.what()) == "unhashable type: 'list'"; }
  if (!rejected) throw std::runtime_error("nested_list_rejected: expected rejection differs");
  for (std::size_t i = 0; i < input.events.size(); ++i) if (input.events[i].fields != before.events[i].fields) throw std::runtime_error("input changed");
}
inline void check_14() {
  EventStream input;
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {dtype_number("", std::int64_t{1})}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kTuple, {dtype_number("", std::int64_t{2})}), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{container_cell("case:concept:name", AttributeContainerKind::kList, {dtype_number("", std::int64_t{3})}), EventAttribute{"concept:name", "A2"}}});
  const auto before = input;
  bool rejected = false;
  try { (void)event_stream_to_event_log(input); } catch (const std::invalid_argument& error) { rejected = std::string(error.what()) == "unhashable type: 'list'"; }
  if (!rejected) throw std::runtime_error("late_unhashable: expected rejection differs");
  for (std::size_t i = 0; i < input.events.size(); ++i) if (input.events[i].fields != before.events[i].fields) throw std::runtime_error("input changed");
}
}
inline void check_public_container_case_grouping() {
  container_case_grouping_reference::check_0();
  container_case_grouping_reference::check_1();
  container_case_grouping_reference::check_2();
  container_case_grouping_reference::check_3();
  container_case_grouping_reference::check_4();
  container_case_grouping_reference::check_5();
  container_case_grouping_reference::check_6();
  container_case_grouping_reference::check_7();
  container_case_grouping_reference::check_8();
  container_case_grouping_reference::check_9();
  container_case_grouping_reference::check_10();
  container_case_grouping_reference::check_11();
  container_case_grouping_reference::check_12();
  container_case_grouping_reference::check_13();
  container_case_grouping_reference::check_14();
}
