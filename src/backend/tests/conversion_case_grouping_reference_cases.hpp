// Public grouping SHA256: 25663016634fbe0087a1ed560cd46886192f98af26b803a17e8cfe0a76d04ea7
#pragma once
#include "dataframe_conversion_metadata_reference_cases.hpp"
namespace conversion_case_grouping_reference {
using namespace dataframe_conversion_metadata_reference;
inline bool grouping_scalar_equal(const EventAttribute& actual, const EventAttribute& expected) {
  const auto* number = std::get_if<double>(&expected.typed_value);
  if (number && std::isnan(*number)) {
    const auto* observed = std::get_if<double>(&actual.typed_value);
    return actual.kind == expected.kind && !actual.is_null && observed && std::isnan(*observed);
  }
  return dtype_attribute_equal(actual, expected);
}
inline void check_grouping_0() {
  EventStream input;
  input.events.push_back({{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "A2"}}});
  input.events.push_back({{dtype_number("case:concept:name", std::int64_t{2}), EventAttribute{"concept:name", "A3"}}});
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("integer_float_bool: trace count differs");
  { const auto& trace = actual.traces[0]; const auto expected_case = dtype_number("concept:name", std::int64_t{1});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("integer_float_bool: typed representative differs");
    const std::vector<std::string> expected = {"A0", "A1", "A2"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("integer_float_bool: event grouping/order differs"); }
  { const auto& trace = actual.traces[1]; const auto expected_case = dtype_number("concept:name", std::int64_t{2});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("integer_float_bool: typed representative differs");
    const std::vector<std::string> expected = {"A3"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("integer_float_bool: event grouping/order differs"); }
  if (!grouping_scalar_equal(input.events[0].fields[0], dtype_number("case:concept:name", std::int64_t{1}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[1].fields[0], dtype_number("case:concept:name", double{1.0}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[2].fields[0], dtype_number("case:concept:name", true))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[3].fields[0], dtype_number("case:concept:name", std::int64_t{2}))) throw std::runtime_error("input scalar changed");
}
inline void check_grouping_1() {
  EventStream input;
  input.events.push_back({{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "A2"}}});
  input.events.push_back({{dtype_number("case:concept:name", false), EventAttribute{"concept:name", "A3"}}});
  input.events.push_back({{dtype_number("case:concept:name", std::int64_t{0}), EventAttribute{"concept:name", "A4"}}});
  input.events.push_back({{dtype_number("case:concept:name", double{-0.0}), EventAttribute{"concept:name", "A5"}}});
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("bool_first: trace count differs");
  { const auto& trace = actual.traces[0]; const auto expected_case = dtype_number("concept:name", true);
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("bool_first: typed representative differs");
    const std::vector<std::string> expected = {"A0", "A1", "A2"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("bool_first: event grouping/order differs"); }
  { const auto& trace = actual.traces[1]; const auto expected_case = dtype_number("concept:name", false);
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("bool_first: typed representative differs");
    const std::vector<std::string> expected = {"A3", "A4", "A5"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("bool_first: event grouping/order differs"); }
  if (!grouping_scalar_equal(input.events[0].fields[0], dtype_number("case:concept:name", true))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[1].fields[0], dtype_number("case:concept:name", std::int64_t{1}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[2].fields[0], dtype_number("case:concept:name", double{1.0}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[3].fields[0], dtype_number("case:concept:name", false))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[4].fields[0], dtype_number("case:concept:name", std::int64_t{0}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[5].fields[0], dtype_number("case:concept:name", double{-0.0}))) throw std::runtime_error("input scalar changed");
}
inline void check_grouping_2() {
  EventStream input;
  input.events.push_back({{dtype_number("case:concept:name", double{0.0}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{dtype_number("case:concept:name", double{-0.0}), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{dtype_number("case:concept:name", std::int64_t{0}), EventAttribute{"concept:name", "A2"}}});
  input.events.push_back({{dtype_number("case:concept:name", false), EventAttribute{"concept:name", "A3"}}});
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 1) throw std::runtime_error("signed_zero: trace count differs");
  { const auto& trace = actual.traces[0]; const auto expected_case = dtype_number("concept:name", double{0.0});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("signed_zero: typed representative differs");
    const std::vector<std::string> expected = {"A0", "A1", "A2", "A3"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("signed_zero: event grouping/order differs"); }
  if (!grouping_scalar_equal(input.events[0].fields[0], dtype_number("case:concept:name", double{0.0}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[1].fields[0], dtype_number("case:concept:name", double{-0.0}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[2].fields[0], dtype_number("case:concept:name", std::int64_t{0}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[3].fields[0], dtype_number("case:concept:name", false))) throw std::runtime_error("input scalar changed");
}
inline void check_grouping_3() {
  EventStream input;
  input.events.push_back({{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "A2"}}});
  input.events.push_back({{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "A3"}}});
  input.events.push_back({{EventAttribute{"case:concept:name", "True", AttributeKind::kString}, EventAttribute{"concept:name", "A4"}}});
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 3) throw std::runtime_error("strings_separate: trace count differs");
  { const auto& trace = actual.traces[0]; const auto expected_case = dtype_number("concept:name", std::int64_t{1});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("strings_separate: typed representative differs");
    const std::vector<std::string> expected = {"A0", "A2", "A3"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("strings_separate: event grouping/order differs"); }
  { const auto& trace = actual.traces[1]; const auto expected_case = EventAttribute{"concept:name", "1", AttributeKind::kString};
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("strings_separate: typed representative differs");
    const std::vector<std::string> expected = {"A1"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("strings_separate: event grouping/order differs"); }
  { const auto& trace = actual.traces[2]; const auto expected_case = EventAttribute{"concept:name", "True", AttributeKind::kString};
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("strings_separate: typed representative differs");
    const std::vector<std::string> expected = {"A4"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("strings_separate: event grouping/order differs"); }
  if (!grouping_scalar_equal(input.events[0].fields[0], dtype_number("case:concept:name", std::int64_t{1}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[1].fields[0], EventAttribute{"case:concept:name", "1", AttributeKind::kString})) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[2].fields[0], dtype_number("case:concept:name", double{1.0}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[3].fields[0], dtype_number("case:concept:name", true))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[4].fields[0], EventAttribute{"case:concept:name", "True", AttributeKind::kString})) throw std::runtime_error("input scalar changed");
}
inline void check_grouping_4() {
  EventStream input;
  input.events.push_back({{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "A2"}}});
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("wide_exact: trace count differs");
  { const auto& trace = actual.traces[0]; const auto expected_case = dtype_number("concept:name", std::int64_t{9007199254740992});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("wide_exact: typed representative differs");
    const std::vector<std::string> expected = {"A0", "A2"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("wide_exact: event grouping/order differs"); }
  { const auto& trace = actual.traces[1]; const auto expected_case = dtype_number("concept:name", std::int64_t{9007199254740993});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("wide_exact: typed representative differs");
    const std::vector<std::string> expected = {"A1"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("wide_exact: event grouping/order differs"); }
  if (!grouping_scalar_equal(input.events[0].fields[0], dtype_number("case:concept:name", std::int64_t{9007199254740992}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[1].fields[0], dtype_number("case:concept:name", std::int64_t{9007199254740993}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[2].fields[0], dtype_number("case:concept:name", double{9007199254740992.0}))) throw std::runtime_error("input scalar changed");
}
inline void check_grouping_5() {
  EventStream input;
  input.events.push_back({{reference_number("case:concept:name", std::uint64_t{18446744073709551615ULL}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{reference_number("case:concept:name", std::uint64_t{18446744073709551614ULL}), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{dtype_number("case:concept:name", double{1.8446744073709552e+19}), EventAttribute{"concept:name", "A2"}}});
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 3) throw std::runtime_error("unsigned: trace count differs");
  { const auto& trace = actual.traces[0]; const auto expected_case = reference_number("concept:name", std::uint64_t{18446744073709551615ULL});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("unsigned: typed representative differs");
    const std::vector<std::string> expected = {"A0"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("unsigned: event grouping/order differs"); }
  { const auto& trace = actual.traces[1]; const auto expected_case = reference_number("concept:name", std::uint64_t{18446744073709551614ULL});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("unsigned: typed representative differs");
    const std::vector<std::string> expected = {"A1"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("unsigned: event grouping/order differs"); }
  { const auto& trace = actual.traces[2]; const auto expected_case = dtype_number("concept:name", double{1.8446744073709552e+19});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("unsigned: typed representative differs");
    const std::vector<std::string> expected = {"A2"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("unsigned: event grouping/order differs"); }
  if (!grouping_scalar_equal(input.events[0].fields[0], reference_number("case:concept:name", std::uint64_t{18446744073709551615ULL}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[1].fields[0], reference_number("case:concept:name", std::uint64_t{18446744073709551614ULL}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[2].fields[0], dtype_number("case:concept:name", double{1.8446744073709552e+19}))) throw std::runtime_error("input scalar changed");
}
inline void check_grouping_6() {
  EventStream input;
  input.events.push_back({{reference_number("case:concept:name", (std::numeric_limits<std::int64_t>::min)()), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{dtype_number("case:concept:name", double{-9.223372036854776e+18}), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{dtype_number("case:concept:name", std::int64_t{-9223372036854775807}), EventAttribute{"concept:name", "A2"}}});
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("signed_min: trace count differs");
  { const auto& trace = actual.traces[0]; const auto expected_case = reference_number("concept:name", (std::numeric_limits<std::int64_t>::min)());
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("signed_min: typed representative differs");
    const std::vector<std::string> expected = {"A0", "A1"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("signed_min: event grouping/order differs"); }
  { const auto& trace = actual.traces[1]; const auto expected_case = dtype_number("concept:name", std::int64_t{-9223372036854775807});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("signed_min: typed representative differs");
    const std::vector<std::string> expected = {"A2"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("signed_min: event grouping/order differs"); }
  if (!grouping_scalar_equal(input.events[0].fields[0], reference_number("case:concept:name", (std::numeric_limits<std::int64_t>::min)()))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[1].fields[0], dtype_number("case:concept:name", double{-9.223372036854776e+18}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[2].fields[0], dtype_number("case:concept:name", std::int64_t{-9223372036854775807}))) throw std::runtime_error("input scalar changed");
}
inline void check_grouping_7() {
  EventStream input;
  input.events.push_back({{dtype_number("case:concept:name", double{1.5}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{input.events[0].fields[0], EventAttribute{"concept:name", "A2"}}});
  input.events.push_back({{dtype_number("case:concept:name", double{-1.5}), EventAttribute{"concept:name", "A3"}}});
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 3) throw std::runtime_error("fractional: trace count differs");
  { const auto& trace = actual.traces[0]; const auto expected_case = dtype_number("concept:name", double{1.5});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("fractional: typed representative differs");
    const std::vector<std::string> expected = {"A0", "A2"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("fractional: event grouping/order differs"); }
  { const auto& trace = actual.traces[1]; const auto expected_case = dtype_number("concept:name", std::int64_t{1});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("fractional: typed representative differs");
    const std::vector<std::string> expected = {"A1"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("fractional: event grouping/order differs"); }
  { const auto& trace = actual.traces[2]; const auto expected_case = dtype_number("concept:name", double{-1.5});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("fractional: typed representative differs");
    const std::vector<std::string> expected = {"A3"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("fractional: event grouping/order differs"); }
  if (!grouping_scalar_equal(input.events[0].fields[0], dtype_number("case:concept:name", double{1.5}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[1].fields[0], dtype_number("case:concept:name", std::int64_t{1}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[2].fields[0], dtype_number("case:concept:name", double{1.5}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[3].fields[0], dtype_number("case:concept:name", double{-1.5}))) throw std::runtime_error("input scalar changed");
}
inline void check_grouping_8() {
  EventStream input;
  input.events.push_back({{reference_null("case:concept:name"), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{input.events[0].fields[0], EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{EventAttribute{"case:concept:name", "None", AttributeKind::kString}, EventAttribute{"concept:name", "A2"}}});
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("null: trace count differs");
  { const auto& trace = actual.traces[0]; const auto expected_case = reference_null("concept:name");
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("null: typed representative differs");
    const std::vector<std::string> expected = {"A0", "A1"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("null: event grouping/order differs"); }
  { const auto& trace = actual.traces[1]; const auto expected_case = EventAttribute{"concept:name", "None", AttributeKind::kString};
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("null: typed representative differs");
    const std::vector<std::string> expected = {"A2"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("null: event grouping/order differs"); }
  if (!grouping_scalar_equal(input.events[0].fields[0], reference_null("case:concept:name"))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[1].fields[0], reference_null("case:concept:name"))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[2].fields[0], EventAttribute{"case:concept:name", "None", AttributeKind::kString})) throw std::runtime_error("input scalar changed");
}
inline void check_grouping_9() {
  EventStream input;
  input.events.push_back({{dtype_number("case:concept:name", std::numeric_limits<double>::quiet_NaN()), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{dtype_number("case:concept:name", std::numeric_limits<double>::quiet_NaN()), EventAttribute{"concept:name", "A1"}}});
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("different_nan: trace count differs");
  { const auto& trace = actual.traces[0]; const auto expected_case = dtype_number("concept:name", std::numeric_limits<double>::quiet_NaN());
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("different_nan: typed representative differs");
    const std::vector<std::string> expected = {"A0"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("different_nan: event grouping/order differs"); }
  { const auto& trace = actual.traces[1]; const auto expected_case = dtype_number("concept:name", std::numeric_limits<double>::quiet_NaN());
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("different_nan: typed representative differs");
    const std::vector<std::string> expected = {"A1"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("different_nan: event grouping/order differs"); }
  if (!grouping_scalar_equal(input.events[0].fields[0], dtype_number("case:concept:name", std::numeric_limits<double>::quiet_NaN()))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[1].fields[0], dtype_number("case:concept:name", std::numeric_limits<double>::quiet_NaN()))) throw std::runtime_error("input scalar changed");
}
inline void check_grouping_10() {
  EventStream input;
  input.events.push_back({{dtype_number("case:concept:name", std::numeric_limits<double>::quiet_NaN()), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{input.events[0].fields[0], EventAttribute{"concept:name", "A1"}}});
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 1) throw std::runtime_error("shared_nan: trace count differs");
  { const auto& trace = actual.traces[0]; const auto expected_case = dtype_number("concept:name", std::numeric_limits<double>::quiet_NaN());
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("shared_nan: typed representative differs");
    const std::vector<std::string> expected = {"A0", "A1"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("shared_nan: event grouping/order differs"); }
  if (!grouping_scalar_equal(input.events[0].fields[0], dtype_number("case:concept:name", std::numeric_limits<double>::quiet_NaN()))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[1].fields[0], dtype_number("case:concept:name", std::numeric_limits<double>::quiet_NaN()))) throw std::runtime_error("input scalar changed");
}
inline void check_grouping_11() {
  EventStream input;
  input.events.push_back({{dtype_number("case:concept:name", std::numeric_limits<double>::infinity()), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{dtype_number("case:concept:name", std::numeric_limits<double>::infinity()), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{dtype_number("case:concept:name", -std::numeric_limits<double>::infinity()), EventAttribute{"concept:name", "A2"}}});
  input.events.push_back({{dtype_number("case:concept:name", -std::numeric_limits<double>::infinity()), EventAttribute{"concept:name", "A3"}}});
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("infinities: trace count differs");
  { const auto& trace = actual.traces[0]; const auto expected_case = dtype_number("concept:name", std::numeric_limits<double>::infinity());
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("infinities: typed representative differs");
    const std::vector<std::string> expected = {"A0", "A1"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("infinities: event grouping/order differs"); }
  { const auto& trace = actual.traces[1]; const auto expected_case = dtype_number("concept:name", -std::numeric_limits<double>::infinity());
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("infinities: typed representative differs");
    const std::vector<std::string> expected = {"A2", "A3"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("infinities: event grouping/order differs"); }
  if (!grouping_scalar_equal(input.events[0].fields[0], dtype_number("case:concept:name", std::numeric_limits<double>::infinity()))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[1].fields[0], dtype_number("case:concept:name", std::numeric_limits<double>::infinity()))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[2].fields[0], dtype_number("case:concept:name", -std::numeric_limits<double>::infinity()))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[3].fields[0], dtype_number("case:concept:name", -std::numeric_limits<double>::infinity()))) throw std::runtime_error("input scalar changed");
}
inline void check_grouping_12() {
  EventStream input;
  input.events.push_back({{reference_number("case:concept:name", std::uint64_t{9223372036854775808ULL}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{reference_number("case:concept:name", std::uint64_t{9223372036854775809ULL}), EventAttribute{"concept:name", "A2"}}});
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("uint_float_exact: trace count differs");
  { const auto& trace = actual.traces[0]; const auto expected_case = reference_number("concept:name", std::uint64_t{9223372036854775808ULL});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("uint_float_exact: typed representative differs");
    const std::vector<std::string> expected = {"A0", "A1"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("uint_float_exact: event grouping/order differs"); }
  { const auto& trace = actual.traces[1]; const auto expected_case = reference_number("concept:name", std::uint64_t{9223372036854775809ULL});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("uint_float_exact: typed representative differs");
    const std::vector<std::string> expected = {"A2"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("uint_float_exact: event grouping/order differs"); }
  if (!grouping_scalar_equal(input.events[0].fields[0], reference_number("case:concept:name", std::uint64_t{9223372036854775808ULL}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[1].fields[0], dtype_number("case:concept:name", double{9.223372036854776e+18}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[2].fields[0], reference_number("case:concept:name", std::uint64_t{9223372036854775809ULL}))) throw std::runtime_error("input scalar changed");
}
inline void check_grouping_13() {
  EventStream input;
  input.events.push_back({{dtype_number("case:concept:name", std::int64_t{-9007199254740993}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{dtype_number("case:concept:name", std::int64_t{-9007199254740992}), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{dtype_number("case:concept:name", double{-9007199254740992.0}), EventAttribute{"concept:name", "A2"}}});
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("negative_wide: trace count differs");
  { const auto& trace = actual.traces[0]; const auto expected_case = dtype_number("concept:name", std::int64_t{-9007199254740993});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("negative_wide: typed representative differs");
    const std::vector<std::string> expected = {"A0"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("negative_wide: event grouping/order differs"); }
  { const auto& trace = actual.traces[1]; const auto expected_case = dtype_number("concept:name", std::int64_t{-9007199254740992});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("negative_wide: typed representative differs");
    const std::vector<std::string> expected = {"A1", "A2"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("negative_wide: event grouping/order differs"); }
  if (!grouping_scalar_equal(input.events[0].fields[0], dtype_number("case:concept:name", std::int64_t{-9007199254740993}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[1].fields[0], dtype_number("case:concept:name", std::int64_t{-9007199254740992}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[2].fields[0], dtype_number("case:concept:name", double{-9007199254740992.0}))) throw std::runtime_error("input scalar changed");
}
inline void check_grouping_14() {
  EventStream input;
  input.events.push_back({{dtype_number("case:concept:name", double{-1.0}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{dtype_number("case:concept:name", std::int64_t{-1}), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{dtype_number("case:concept:name", double{-1.5}), EventAttribute{"concept:name", "A2"}}});
  input.events.push_back({{input.events[2].fields[0], EventAttribute{"concept:name", "A3"}}});
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("negative_fraction: trace count differs");
  { const auto& trace = actual.traces[0]; const auto expected_case = dtype_number("concept:name", double{-1.0});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("negative_fraction: typed representative differs");
    const std::vector<std::string> expected = {"A0", "A1"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("negative_fraction: event grouping/order differs"); }
  { const auto& trace = actual.traces[1]; const auto expected_case = dtype_number("concept:name", double{-1.5});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("negative_fraction: typed representative differs");
    const std::vector<std::string> expected = {"A2", "A3"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("negative_fraction: event grouping/order differs"); }
  if (!grouping_scalar_equal(input.events[0].fields[0], dtype_number("case:concept:name", double{-1.0}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[1].fields[0], dtype_number("case:concept:name", std::int64_t{-1}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[2].fields[0], dtype_number("case:concept:name", double{-1.5}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[3].fields[0], dtype_number("case:concept:name", double{-1.5}))) throw std::runtime_error("input scalar changed");
}
inline void check_grouping_15() {
  EventStream input;
  input.events.push_back({{reference_number("case:concept:name", std::uint64_t{18446744073709549568ULL}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{dtype_number("case:concept:name", double{1.844674407370955e+19}), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{reference_number("case:concept:name", std::uint64_t{18446744073709549569ULL}), EventAttribute{"concept:name", "A2"}}});
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("positive_boundary: trace count differs");
  { const auto& trace = actual.traces[0]; const auto expected_case = reference_number("concept:name", std::uint64_t{18446744073709549568ULL});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("positive_boundary: typed representative differs");
    const std::vector<std::string> expected = {"A0", "A1"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("positive_boundary: event grouping/order differs"); }
  { const auto& trace = actual.traces[1]; const auto expected_case = reference_number("concept:name", std::uint64_t{18446744073709549569ULL});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("positive_boundary: typed representative differs");
    const std::vector<std::string> expected = {"A2"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("positive_boundary: event grouping/order differs"); }
  if (!grouping_scalar_equal(input.events[0].fields[0], reference_number("case:concept:name", std::uint64_t{18446744073709549568ULL}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[1].fields[0], dtype_number("case:concept:name", double{1.844674407370955e+19}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[2].fields[0], reference_number("case:concept:name", std::uint64_t{18446744073709549569ULL}))) throw std::runtime_error("input scalar changed");
}
inline void check_grouping_16() {
  EventStream input;
  input.events.push_back({{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "A0"}}});
  input.events.push_back({{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "A1"}}});
  input.events.push_back({{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A2"}}});
  input.events.push_back({{EventAttribute{"case:concept:name", "1.0", AttributeKind::kString}, EventAttribute{"concept:name", "A3"}}});
  const auto actual = event_stream_to_event_log(input);
  if (actual.traces.size() != 2) throw std::runtime_error("float_first: trace count differs");
  { const auto& trace = actual.traces[0]; const auto expected_case = dtype_number("concept:name", double{1.0});
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("float_first: typed representative differs");
    const std::vector<std::string> expected = {"A0", "A1", "A2"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("float_first: event grouping/order differs"); }
  { const auto& trace = actual.traces[1]; const auto expected_case = EventAttribute{"concept:name", "1.0", AttributeKind::kString};
    if (!trace.case_id_attribute || !grouping_scalar_equal(*trace.case_id_attribute, expected_case)) throw std::runtime_error("float_first: typed representative differs");
    const std::vector<std::string> expected = {"A3"};
    std::vector<std::string> observed; for (const auto& event : trace.events) { observed.push_back(event.activity); if (!event.attributes.empty()) throw std::runtime_error("unexpected event fields"); }
    if (observed != expected) throw std::runtime_error("float_first: event grouping/order differs"); }
  if (!grouping_scalar_equal(input.events[0].fields[0], dtype_number("case:concept:name", double{1.0}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[1].fields[0], dtype_number("case:concept:name", true))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[2].fields[0], dtype_number("case:concept:name", std::int64_t{1}))) throw std::runtime_error("input scalar changed");
  if (!grouping_scalar_equal(input.events[3].fields[0], EventAttribute{"case:concept:name", "1.0", AttributeKind::kString})) throw std::runtime_error("input scalar changed");
}
}
inline void check_public_conversion_case_grouping() {
  conversion_case_grouping_reference::check_grouping_0();
  conversion_case_grouping_reference::check_grouping_1();
  conversion_case_grouping_reference::check_grouping_2();
  conversion_case_grouping_reference::check_grouping_3();
  conversion_case_grouping_reference::check_grouping_4();
  conversion_case_grouping_reference::check_grouping_5();
  conversion_case_grouping_reference::check_grouping_6();
  conversion_case_grouping_reference::check_grouping_7();
  conversion_case_grouping_reference::check_grouping_8();
  conversion_case_grouping_reference::check_grouping_9();
  conversion_case_grouping_reference::check_grouping_10();
  conversion_case_grouping_reference::check_grouping_11();
  conversion_case_grouping_reference::check_grouping_12();
  conversion_case_grouping_reference::check_grouping_13();
  conversion_case_grouping_reference::check_grouping_14();
  conversion_case_grouping_reference::check_grouping_15();
  conversion_case_grouping_reference::check_grouping_16();
}
