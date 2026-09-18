// Reference report SHA256: 05b5e94b2c083870f99f284df25bedd00f2bdd6773309f4af4fcf6f577b74fac
// Generated from public PM4Py observations; do not edit by hand.
#pragma once

#include "minera/case_relations.hpp"
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace case_relations_reference {
using namespace minera;

inline EventAttribute reference_null(const std::string& key) {
  EventAttribute cell{key, "", AttributeKind::kString};
  cell.is_null = true;
  return cell;
}
template<class T> EventAttribute reference_number(const std::string& key, T value) {
  EventAttribute cell{key, std::to_string(value),
      std::is_same_v<T, bool> ? AttributeKind::kBoolean :
      std::is_integral_v<T> ? AttributeKind::kInt : AttributeKind::kFloat};
  cell.typed_value = value;
  return cell;
}
inline EventAttribute reference_date(const std::string& key, const std::string& value) {
  EventAttribute cell{key, value, AttributeKind::kDate};
  cell.typed_value = NativeDate{value};
  return cell;
}
inline bool reference_missing(const std::optional<EventAttribute>& cell) {
  return !cell || cell->is_null;
}
inline bool reference_cell_equal(const std::optional<EventAttribute>& actual,
                                 const std::optional<EventAttribute>& expected) {
  if (reference_missing(actual) || reference_missing(expected))
    return reference_missing(actual) == reference_missing(expected);
  if (const auto* number = std::get_if<std::int64_t>(&expected->typed_value)) {
    if (const auto* value = std::get_if<std::int64_t>(&actual->typed_value))
      return *value == *number;
    if (const auto* value = std::get_if<double>(&actual->typed_value))
      return std::isfinite(*value) && *value >= -9223372036854775808.0 &&
             *value < 9223372036854775808.0 && std::trunc(*value) == *value &&
             static_cast<std::int64_t>(*value) == *number;
    return false;
  }
  if (const auto* number = std::get_if<double>(&expected->typed_value)) {
    if (const auto* value = std::get_if<double>(&actual->typed_value)) return *value == *number;
    if (const auto* value = std::get_if<std::int64_t>(&actual->typed_value))
      return *number >= -9223372036854775808.0 && *number < 9223372036854775808.0 &&
             std::trunc(*number) == *number && static_cast<std::int64_t>(*number) == *value;
    return false;
  }
  if (const auto* boolean = std::get_if<bool>(&expected->typed_value)) {
    const auto* value = std::get_if<bool>(&actual->typed_value);
    return value && *value == *boolean;
  }
  if (const auto* expected_date = std::get_if<NativeDate>(&expected->typed_value)) {
    const auto* date = std::get_if<NativeDate>(&actual->typed_value);
    return date && date->value == expected_date->value;
  }
  return std::holds_alternative<std::monostate>(actual->typed_value) &&
         actual->value == expected->value;
}
inline void reference_check(const std::string& name, const EventTable& actual,
                            const EventTable& expected) {
  if (actual.columns != expected.columns || actual.rows.size() != expected.rows.size())
    throw std::runtime_error(name + ": public table shape/columns differ");
  actual.validate();
  for (std::size_t row = 0; row < expected.rows.size(); ++row)
    for (std::size_t col = 0; col < expected.columns.size(); ++col)
      if (!reference_cell_equal(actual.rows[row][col], expected.rows[row][col]))
        throw std::runtime_error(name + ": public cell differs at " +
            std::to_string(row) + ", " + expected.columns[col]);
}
} // namespace case_relations_reference

inline void check_public_case_relations_reference_cases() {
  using namespace case_relations_reference;
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "base-L2-late", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:05+00:00")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-L1-late", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:05+00:00")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "base-L2-early", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-L1-early", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:00+00:00")}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-R2-late", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:04+00:00")},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-R1-late", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:04+00:00")},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-R2-early", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:01+00:00")},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-R1-early", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:01+00:00")}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-L1-early", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:00+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-R1-early", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:01+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-R1-late", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:04+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-L1-late", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:05+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "base-L2-early", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:00+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-R2-early", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:01+00:00"), EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-R2-late", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:04+00:00"), EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "base-L2-late", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:05+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    reference_check("reversed_multi_case_global_order", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L3", AttributeKind::kString}, EventAttribute{"concept:name", "base-L3", AttributeKind::kString}, EventAttribute{"when", "0", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "base-L2", AttributeKind::kString}, EventAttribute{"when", "2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L0", AttributeKind::kString}, EventAttribute{"concept:name", "base-L0", AttributeKind::kString}, EventAttribute{"when", "0", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-L1", AttributeKind::kString}, EventAttribute{"when", "1", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-R2", AttributeKind::kString}, EventAttribute{"when", "3", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-R1", AttributeKind::kString}, EventAttribute{"when", "2", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L0", AttributeKind::kString}, EventAttribute{"concept:name", "base-L0", AttributeKind::kString}, EventAttribute{"when", "0", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-L1", AttributeKind::kString}, EventAttribute{"when", "1", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-R1", AttributeKind::kString}, EventAttribute{"when", "2", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "base-L2", AttributeKind::kString}, EventAttribute{"when", "2", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-R2", AttributeKind::kString}, EventAttribute{"when", "3", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L3", AttributeKind::kString}, EventAttribute{"concept:name", "base-L3", AttributeKind::kString}, EventAttribute{"when", "0", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    reference_check("reversed_relation_order_with_unrelated_base", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "rank"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-rank-2", AttributeKind::kString}, reference_number("rank", std::int64_t{2})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-rank-1", AttributeKind::kString}, reference_number("rank", std::int64_t{1})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-rank-tie", AttributeKind::kString}, reference_number("rank", std::int64_t{2})}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "rank"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-rank-3", AttributeKind::kString}, reference_number("rank", std::int64_t{3})},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-rank-1", AttributeKind::kString}, reference_number("rank", std::int64_t{1})},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-rank-tie", AttributeKind::kString}, reference_number("rank", std::int64_t{2})}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "rank";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "rank", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-rank-1", AttributeKind::kString}, reference_number("rank", std::int64_t{1}), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-rank-1", AttributeKind::kString}, reference_number("rank", std::int64_t{1}), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-rank-2", AttributeKind::kString}, reference_number("rank", std::int64_t{2}), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-rank-tie", AttributeKind::kString}, reference_number("rank", std::int64_t{2}), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-rank-tie", AttributeKind::kString}, reference_number("rank", std::int64_t{2}), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-rank-3", AttributeKind::kString}, reference_number("rank", std::int64_t{3}), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("numeric_integer_timestamp", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "rank"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-float-1.5", AttributeKind::kString}, reference_number("rank", double{1.5})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-float-1.0", AttributeKind::kString}, reference_number("rank", double{1.0})}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "rank"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-float-2.0", AttributeKind::kString}, reference_number("rank", double{2.0})},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-float-0.5", AttributeKind::kString}, reference_number("rank", double{0.5})}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "rank";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "rank", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-float-0.5", AttributeKind::kString}, reference_number("rank", double{0.5}), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-float-1.0", AttributeKind::kString}, reference_number("rank", double{1.0}), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-float-1.5", AttributeKind::kString}, reference_number("rank", double{1.5}), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-float-2.0", AttributeKind::kString}, reference_number("rank", double{2.0}), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("numeric_float_timestamp", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-z", AttributeKind::kString}, EventAttribute{"when", "z", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-a", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-m", AttributeKind::kString}, EventAttribute{"when", "m", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-b", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-y", AttributeKind::kString}, EventAttribute{"when", "y", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-a", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-b", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-m", AttributeKind::kString}, EventAttribute{"when", "m", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-y", AttributeKind::kString}, EventAttribute{"when", "y", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-z", AttributeKind::kString}, EventAttribute{"when", "z", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    reference_check("string_timestamp_lexical", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-ten", AttributeKind::kString}, EventAttribute{"when", "10", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-two", AttributeKind::kString}, EventAttribute{"when", "2", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-one", AttributeKind::kString}, EventAttribute{"when", "1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-twenty", AttributeKind::kString}, EventAttribute{"when", "20", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-one", AttributeKind::kString}, EventAttribute{"when", "1", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-ten", AttributeKind::kString}, EventAttribute{"when", "10", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-two", AttributeKind::kString}, EventAttribute{"when", "2", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-twenty", AttributeKind::kString}, EventAttribute{"when", "20", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("string_numeric_looking_timestamp_lexical", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "started"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-late", AttributeKind::kString}, reference_date("started", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-early", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00+00:00")}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "started"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-mid", AttributeKind::kString}, reference_date("started", "2024-01-01T12:00:00+00:00")},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-latest", AttributeKind::kString}, reference_date("started", "2024-01-03T00:00:00+00:00")}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "started";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "started", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-early", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-mid", AttributeKind::kString}, reference_date("started", "2024-01-01T12:00:00+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-late", AttributeKind::kString}, reference_date("started", "2024-01-02T00:00:00+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-latest", AttributeKind::kString}, reference_date("started", "2024-01-03T00:00:00+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("typed_datetime_timestamp_custom_selector", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-number", AttributeKind::kString}, reference_number("when", std::int64_t{2})},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-number-low", AttributeKind::kString}, reference_number("when", std::int64_t{1})}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-string", AttributeKind::kString}, EventAttribute{"when", "3", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-string-low", AttributeKind::kString}, EventAttribute{"when", "0", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-number-low", AttributeKind::kString}, reference_number("when", std::int64_t{1}), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-number", AttributeKind::kString}, reference_number("when", std::int64_t{2}), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-string-low", AttributeKind::kString}, EventAttribute{"when", "0", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-string", AttributeKind::kString}, EventAttribute{"when", "3", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("mixed_numeric_string_timestamp", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-date", AttributeKind::kString}, reference_date("when", "2024-01-02T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-date-early", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:00+00:00")}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-text", AttributeKind::kString}, EventAttribute{"when", "2024-01-03", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-text-early", AttributeKind::kString}, EventAttribute{"when", "2023-12-31", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-date-early", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:00+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-date", AttributeKind::kString}, reference_date("when", "2024-01-02T00:00:00+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-text-early", AttributeKind::kString}, EventAttribute{"when", "2023-12-31", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-text", AttributeKind::kString}, EventAttribute{"when", "2024-01-03", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("mixed_datetime_string_timestamp", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{reference_null("case:concept:name"), EventAttribute{"concept:name", "base-null-id", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-valid", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-valid", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-valid", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-valid", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{reference_null("case:concept:name"), EventAttribute{"concept:name", "base-null-id", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    reference_check("missing_case_id_in_base", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-valid", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{reference_null("case:concept:name"), EventAttribute{"concept:name", "incoming-null-id", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-valid", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-valid", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-valid", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("missing_case_id_in_incoming", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-valid", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-valid", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{reference_null("case:concept:name_LEFT"), EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-valid", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-valid", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{reference_null("case:concept:name"), EventAttribute{"concept:name", "incoming-valid", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("missing_case_id_in_relations", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-valid", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-valid", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{reference_null("case:concept:name_LEFT"), EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-valid", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-valid", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{reference_null("case:concept:name"), EventAttribute{"concept:name", "incoming-valid", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("nan_case_id_in_relations", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-valid", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-valid", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-valid", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-valid", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-valid", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("duplicate_valid_relations", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-valid", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-valid", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L9", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R9", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-valid", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-valid", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("unmatched_relation_with_valid_relation", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "completed_at"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-completed", AttributeKind::kString}, EventAttribute{"completed_at", "late", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-completed-early", AttributeKind::kString}, EventAttribute{"completed_at", "early", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "completed_at"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-completed", AttributeKind::kString}, EventAttribute{"completed_at", "middle", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "completed_at";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "completed_at", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-completed-early", AttributeKind::kString}, EventAttribute{"completed_at", "early", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-completed", AttributeKind::kString}, EventAttribute{"completed_at", "late", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-completed", AttributeKind::kString}, EventAttribute{"completed_at", "middle", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("custom_string_timestamp_selector", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when", "link"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, EventAttribute{"link", "base-link", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when", "link"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"link", "incoming-link", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT", "link", "extra"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}, EventAttribute{"link", "relation-link", AttributeKind::kString}, reference_number("extra", std::int64_t{42})}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "link", "link_x", "case:concept:name_LEFT", "case:concept:name_RIGHT", "link_y", "extra"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, EventAttribute{"link", "base-link", AttributeKind::kString}, reference_null("link_x"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT"), reference_null("link_y"), reference_null("extra")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("link"), EventAttribute{"link_x", "incoming-link", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}, EventAttribute{"link_y", "relation-link", AttributeKind::kString}, reference_number("extra", double{42.0})}}};
    reference_check("relation_extra_with_incoming_collision", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{reference_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "base", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{reference_number("case:concept:name", std::int64_t{2}), EventAttribute{"concept:name", "incoming", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "2", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    bool rejected = false;
    try { (void)merge_case_relations(base, incoming, relations, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    if (!rejected) throw std::runtime_error("numeric_case_ids_string_relation_ids: public rejection not reproduced");
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{reference_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "base-int", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "base-str", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{reference_number("case:concept:name", std::int64_t{2}), EventAttribute{"concept:name", "incoming-int", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-str", AttributeKind::kString}, EventAttribute{"when", "d", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{reference_number("case:concept:name_LEFT", std::int64_t{1}), reference_number("case:concept:name_RIGHT", std::int64_t{2})}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{reference_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "base-int", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{reference_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "incoming-int", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_number("case:concept:name_LEFT", double{1.0}), reference_number("case:concept:name_RIGHT", double{2.0})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "base-str", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    reference_check("mixed_object_event_ids_numeric_relations", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{reference_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "base-int", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "base-str", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{reference_number("case:concept:name", std::int64_t{2}), EventAttribute{"concept:name", "incoming-int", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "2", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-str", AttributeKind::kString}, EventAttribute{"when", "d", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{reference_number("case:concept:name_LEFT", std::int64_t{1}), reference_number("case:concept:name_RIGHT", std::int64_t{2})},
{EventAttribute{"case:concept:name_LEFT", "1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "2", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{reference_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "base-int", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{reference_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "incoming-int", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_number("case:concept:name_LEFT", std::int64_t{1}), reference_number("case:concept:name_RIGHT", std::int64_t{2})},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "base-str", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-str", AttributeKind::kString}, EventAttribute{"when", "d", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "2", AttributeKind::kString}}}};
    reference_check("mixed_object_event_ids_mixed_relations", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-one", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "base-two", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-one", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "base-two", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    reference_check("empty_relation_table", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{reference_null("case:concept:name"), EventAttribute{"concept:name", "incoming-null-right", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, reference_null("case:concept:name_RIGHT")}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null-right", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, reference_null("case:concept:name_RIGHT")}}};
    reference_check("null_right_matches_null_incoming", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-date", AttributeKind::kString}, reference_date("when", "2024-01-01T00:00:00")}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-number", AttributeKind::kString}, reference_number("when", std::int64_t{1})}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    bool rejected = false;
    try { (void)merge_case_relations(base, incoming, relations, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    if (!rejected) throw std::runtime_error("mixed_datetime_numeric_timestamp: public rejection not reproduced");
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-false", AttributeKind::kString}, reference_number("when", false)},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-true", AttributeKind::kString}, reference_number("when", true)}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-zero", AttributeKind::kString}, reference_number("when", std::int64_t{0})},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-one", AttributeKind::kString}, reference_number("when", std::int64_t{1})}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-false", AttributeKind::kString}, reference_number("when", std::int64_t{0}), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-zero", AttributeKind::kString}, reference_number("when", std::int64_t{0}), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-true", AttributeKind::kString}, reference_number("when", std::int64_t{1}), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-one", AttributeKind::kString}, reference_number("when", std::int64_t{1}), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("bool_numeric_timestamp", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-large-int", AttributeKind::kString}, reference_number("when", std::int64_t{9007199254740993})}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-large-float", AttributeKind::kString}, reference_number("when", double{9007199254740992.0})}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    bool rejected = false;
    try { (void)merge_case_relations(base, incoming, relations, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    if (!rejected) throw std::runtime_error("large_int_float_object_timestamp_precision_collision: public rejection not reproduced");
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when", "link", "link_x"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"link", "incoming-link", AttributeKind::kString}, EventAttribute{"link_x", "original-x", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT", "link"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}, EventAttribute{"link", "relation-link", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    bool rejected = false;
    try { (void)merge_case_relations(base, incoming, relations, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    if (!rejected) throw std::runtime_error("incoming_preexisting_link_x: public rejection not reproduced");
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "when", "link"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"link", "incoming-link", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT", "link", "link_y"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}, EventAttribute{"link", "relation-link", AttributeKind::kString}, EventAttribute{"link_y", "original-y", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    bool rejected = false;
    try { (void)merge_case_relations(base, incoming, relations, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    if (!rejected) throw std::runtime_error("relation_preexisting_link_y: public rejection not reproduced");
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base1", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000001+00:00")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base0", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-last", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.999999999+00:00")}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming2", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000002+00:00")},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000001+00:00")}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "time:timestamp";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "time:timestamp", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base0", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base1", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000001+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming2", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000002+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming2", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000002+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-last", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.999999999+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    reference_check("time_modern_default", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "started"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base1", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000001+00:00")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base0", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-last", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.999999999+00:00")}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "started"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming2", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000002+00:00")},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000001+00:00")}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "started";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "started", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base0", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base1", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000001+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming2", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000002+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming2", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000002+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-last", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.999999999+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    reference_check("time_modern_custom", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base1", AttributeKind::kString}, reference_date("time:timestamp", "1969-12-31T23:59:59.000000001+00:00")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base0", AttributeKind::kString}, reference_date("time:timestamp", "1969-12-31T23:59:59+00:00")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-last", AttributeKind::kString}, reference_date("time:timestamp", "1969-12-31T23:59:59.999999999+00:00")}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming2", AttributeKind::kString}, reference_date("time:timestamp", "1969-12-31T23:59:59.000000002+00:00")},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("time:timestamp", "1969-12-31T23:59:59.000000001+00:00")}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "time:timestamp";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "time:timestamp", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base0", AttributeKind::kString}, reference_date("time:timestamp", "1969-12-31T23:59:59+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base1", AttributeKind::kString}, reference_date("time:timestamp", "1969-12-31T23:59:59.000000001+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("time:timestamp", "1969-12-31T23:59:59.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("time:timestamp", "1969-12-31T23:59:59.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming2", AttributeKind::kString}, reference_date("time:timestamp", "1969-12-31T23:59:59.000000002+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming2", AttributeKind::kString}, reference_date("time:timestamp", "1969-12-31T23:59:59.000000002+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-last", AttributeKind::kString}, reference_date("time:timestamp", "1969-12-31T23:59:59.999999999+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    reference_check("time_pre_epoch_default", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "started"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base1", AttributeKind::kString}, reference_date("started", "1969-12-31T23:59:59.000000001+00:00")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base0", AttributeKind::kString}, reference_date("started", "1969-12-31T23:59:59+00:00")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-last", AttributeKind::kString}, reference_date("started", "1969-12-31T23:59:59.999999999+00:00")}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "started"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming2", AttributeKind::kString}, reference_date("started", "1969-12-31T23:59:59.000000002+00:00")},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("started", "1969-12-31T23:59:59.000000001+00:00")}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "started";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "started", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base0", AttributeKind::kString}, reference_date("started", "1969-12-31T23:59:59+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base1", AttributeKind::kString}, reference_date("started", "1969-12-31T23:59:59.000000001+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("started", "1969-12-31T23:59:59.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("started", "1969-12-31T23:59:59.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming2", AttributeKind::kString}, reference_date("started", "1969-12-31T23:59:59.000000002+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming2", AttributeKind::kString}, reference_date("started", "1969-12-31T23:59:59.000000002+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-last", AttributeKind::kString}, reference_date("started", "1969-12-31T23:59:59.999999999+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    reference_check("time_pre_epoch_custom", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}, reference_null("time:timestamp")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000002+00:00")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}, reference_null("time:timestamp")}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("time:timestamp")},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000001+00:00")}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "time:timestamp";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "time:timestamp", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000002+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}, reference_null("time:timestamp"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}, reference_null("time:timestamp"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("time:timestamp"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("time:timestamp"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("time_default_mixed", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}, reference_null("time:timestamp")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}, reference_null("time:timestamp")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}, reference_null("time:timestamp")}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("time:timestamp")},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_null("time:timestamp")}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "time:timestamp";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "time:timestamp", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}, reference_null("time:timestamp"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}, reference_null("time:timestamp"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}, reference_null("time:timestamp"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("time:timestamp"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("time:timestamp"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_null("time:timestamp"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_null("time:timestamp"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("time_default_all-null", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("time:timestamp")},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000001+00:00")}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "time:timestamp";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "time:timestamp", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}, reference_null("time:timestamp"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}, reference_null("time:timestamp"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}, reference_null("time:timestamp"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("time:timestamp"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("time:timestamp"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("time_default_base-column-absent", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}, reference_null("time:timestamp")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000002+00:00")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}, reference_null("time:timestamp")}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "time:timestamp";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "time:timestamp", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000002+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}, reference_null("time:timestamp"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}, reference_null("time:timestamp"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("time:timestamp"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("time:timestamp"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_null("time:timestamp"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_null("time:timestamp"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("time_default_incoming-column-absent", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "time:timestamp";
    bool rejected = false;
    try { (void)merge_case_relations(base, incoming, relations, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    if (!rejected) throw std::runtime_error("time_default_both-columns-absent: public rejection not reproduced");
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}, EventAttribute{"time:timestamp", "invalid", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}, EventAttribute{"time:timestamp", "", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}, reference_null("time:timestamp")}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "time:timestamp"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("time:timestamp")},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000001+00:00")}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "time:timestamp";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "time:timestamp", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}, EventAttribute{"time:timestamp", "", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}, EventAttribute{"time:timestamp", "invalid", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}, reference_null("time:timestamp"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("time:timestamp"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("time:timestamp"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("time_default_invalid-text", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "started"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}, reference_null("started")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000002+00:00")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}, reference_null("started")}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "started"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("started")},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000001+00:00")}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "started";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "started", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000002+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}, reference_null("started"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}, reference_null("started"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("started"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("started"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("time_custom_mixed", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "started"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}, reference_null("started")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}, reference_null("started")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}, reference_null("started")}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "started"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("started")},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_null("started")}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "started";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "started", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}, reference_null("started"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}, reference_null("started"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}, reference_null("started"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("started"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("started"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_null("started"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_null("started"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("time_custom_all-null", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "started"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("started")},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000001+00:00")}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "started";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "started", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}, reference_null("started"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}, reference_null("started"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}, reference_null("started"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("started"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("started"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("time_custom_base-column-absent", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "started"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}, reference_null("started")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000002+00:00")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}, reference_null("started")}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "started";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "started", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000002+00:00"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}, reference_null("started"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}, reference_null("started"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("started"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("started"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_null("started"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_null("started"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("time_custom_incoming-column-absent", merge_case_relations(base, incoming, relations, options), expected);
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "started";
    bool rejected = false;
    try { (void)merge_case_relations(base, incoming, relations, options); }
    catch (const std::invalid_argument&) { rejected = true; }
    if (!rejected) throw std::runtime_error("time_custom_both-columns-absent: public rejection not reproduced");
  }
  {
    const auto base = EventTable{{"case:concept:name", "concept:name", "started"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}, EventAttribute{"started", "invalid", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}, EventAttribute{"started", "", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}, reference_null("started")}}};
    const auto incoming = EventTable{{"case:concept:name", "concept:name", "started"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("started")},
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000001+00:00")}}};
    const auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "started";
    const auto expected = EventTable{{"case:concept:name", "concept:name", "started", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming1", AttributeKind::kString}, reference_date("started", "2024-01-01T00:00:00.000000001+00:00"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base2", AttributeKind::kString}, EventAttribute{"started", "", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-null", AttributeKind::kString}, EventAttribute{"started", "invalid", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "base-missing", AttributeKind::kString}, reference_null("started"), reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("started"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "incoming-null", AttributeKind::kString}, reference_null("started"), EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    reference_check("time_custom_invalid-text", merge_case_relations(base, incoming, relations, options), expected);
  }
}
