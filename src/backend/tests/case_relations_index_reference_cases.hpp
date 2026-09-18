// Public index report SHA256: 83f3e11179f3a76cf0b6a61a874f4dc8f125d88232c2db18ed3038a60ecff461
#pragma once
#include "case_relations_reference_cases.hpp"
#include <limits>

inline void check_public_case_relations_index_reference_cases() {
  using namespace case_relations_reference;
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    base.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}}}};
    relations.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"Index", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("default_range", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("default_range: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    base.index = EventTableIndex{"Index", "int64", {"source_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{40})}, EventTableIndexLabel{reference_number("", std::int64_t{-7})}, EventTableIndexLabel{reference_number("", std::int64_t{40})}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}}}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {EventTableIndexLabel{EventAttribute{"", "rel-a", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-b", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-c", AttributeKind::kString}}}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"Index", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{40})}, EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{-7})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{40})}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("integer_labels", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("integer_labels: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    base.index = EventTableIndex{"Index", "str", {"row"}, {EventTableIndexLabel{EventAttribute{"", "z", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "a", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "a", AttributeKind::kString}}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}}}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {EventTableIndexLabel{EventAttribute{"", "rel-a", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-b", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-c", AttributeKind::kString}}}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"Index", "object", {std::nullopt}, {EventTableIndexLabel{EventAttribute{"", "a", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{EventAttribute{"", "a", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{EventAttribute{"", "z", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("string_labels", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("string_labels: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    base.index = EventTableIndex{"Index", "int64", {"row"}, {EventTableIndexLabel{reference_number("", std::int64_t{40})}, EventTableIndexLabel{reference_number("", std::int64_t{-7})}, EventTableIndexLabel{reference_number("", std::int64_t{40})}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}}}};
    relations.index = EventTableIndex{"Index", "str", {"row"}, {EventTableIndexLabel{EventAttribute{"", "rel-a", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-b", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-c", AttributeKind::kString}}}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"Index", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{40})}, EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{-7})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{40})}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("shared_name", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("shared_name: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    base.index = EventTableIndex{"Index", "object", {"row"}, {EventTableIndexLabel{reference_null("")}, EventTableIndexLabel{EventAttribute{"", "x", AttributeKind::kString}}, EventTableIndexLabel{reference_null("")}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}}}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {EventTableIndexLabel{EventAttribute{"", "rel-a", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-b", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-c", AttributeKind::kString}}}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"Index", "object", {std::nullopt}, {EventTableIndexLabel{reference_null("")}, EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{EventAttribute{"", "x", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_null("")}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("null_label", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("null_label: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    base.index = EventTableIndex{"DatetimeIndex", "datetime64[us, UTC]", {"row"}, {EventTableIndexLabel{reference_date("", "2024-01-03T00:00:00+00:00")}, EventTableIndexLabel{reference_date("", "2024-01-02T00:00:00+00:00")}, EventTableIndexLabel{reference_date("", "2024-01-01T00:00:00+00:00")}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}}}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {EventTableIndexLabel{EventAttribute{"", "rel-a", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-b", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-c", AttributeKind::kString}}}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"Index", "object", {std::nullopt}, {EventTableIndexLabel{reference_date("", "2024-01-01T00:00:00+00:00")}, EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_date("", "2024-01-02T00:00:00+00:00")}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_date("", "2024-01-03T00:00:00+00:00")}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("datetime_labels", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("datetime_labels: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    base.index = EventTableIndex{"MultiIndex", "object", {"batch", "row"}, {EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{"", "B", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}}, EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{"", "A", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}}, EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{"", "A", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{0})}}}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}}}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {EventTableIndexLabel{EventAttribute{"", "rel-a", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-b", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-c", AttributeKind::kString}}}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"Index", "object", {std::nullopt}, {EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{"", "A", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{0})}}}, EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{"", "A", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{"", "B", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("multiindex", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("multiindex: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    base.index = EventTableIndex{"Index", "int64", {"source_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{40})}, EventTableIndexLabel{reference_number("", std::int64_t{-7})}, EventTableIndexLabel{reference_number("", std::int64_t{40})}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"Index", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{40})}, EventTableIndexLabel{reference_number("", std::int64_t{-7})}, EventTableIndexLabel{reference_number("", std::int64_t{40})}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("empty_relations", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("empty_relations: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
}};
    base.index = EventTableIndex{"Index", "int64", {"source_row"}, {}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}}}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {EventTableIndexLabel{EventAttribute{"", "rel-a", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-b", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-c", AttributeKind::kString}}}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}}}};
    const auto expected_index = EventTableIndex{"Index", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("empty_base", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("empty_base: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    base.index = EventTableIndex{"MultiIndex", "object", {"batch", "row"}, {EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{"", "B", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}}, EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{"", "A", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}}, EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{"", "A", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{0})}}}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"MultiIndex", "object", {std::nullopt, std::nullopt}, {EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{"", "A", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{0})}}}, EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{"", "A", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}}, EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{"", "B", AttributeKind::kString}}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("multiindex_empty_relations", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("multiindex_empty_relations: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    base.index = EventTableIndex{"Index", "str", {"row"}, {EventTableIndexLabel{EventAttribute{"", "z", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "a", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "a", AttributeKind::kString}}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"Index", "str", {std::nullopt}, {EventTableIndexLabel{EventAttribute{"", "a", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "a", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "z", AttributeKind::kString}}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("string_empty_relations", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("string_empty_relations: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    base.index = EventTableIndex{"DatetimeIndex", "datetime64[us, UTC]", {"row"}, {EventTableIndexLabel{reference_date("", "2024-01-03T00:00:00+00:00")}, EventTableIndexLabel{reference_date("", "2024-01-02T00:00:00+00:00")}, EventTableIndexLabel{reference_date("", "2024-01-01T00:00:00+00:00")}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"DatetimeIndex", "datetime64[us, UTC]", {std::nullopt}, {EventTableIndexLabel{reference_date("", "2024-01-01T00:00:00+00:00")}, EventTableIndexLabel{reference_date("", "2024-01-02T00:00:00+00:00")}, EventTableIndexLabel{reference_date("", "2024-01-03T00:00:00+00:00")}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("datetime_empty_relations", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("datetime_empty_relations: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
}};
    base.index = EventTableIndex{"Index", "int64", {"source_row"}, {}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
}};
    const auto expected_index = EventTableIndex{"Index", "int64", {std::nullopt}, {}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("all_inputs_empty", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("all_inputs_empty: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    base.index = EventTableIndex{"Index", "Int64", {"nullable"}, {EventTableIndexLabel{reference_null("")}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}}}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {EventTableIndexLabel{EventAttribute{"", "rel-a", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-b", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-c", AttributeKind::kString}}}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"Index", "Int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_null("")}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("nullable_integer_index", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("nullable_integer_index: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    base.index = EventTableIndex{"Index", "float64", {"floats"}, {EventTableIndexLabel{reference_null("")}, EventTableIndexLabel{reference_number("", double{1.5})}, EventTableIndexLabel{reference_number("", double{2.5})}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}}}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {EventTableIndexLabel{EventAttribute{"", "rel-a", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-b", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-c", AttributeKind::kString}}}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"Index", "float64", {std::nullopt}, {EventTableIndexLabel{reference_number("", double{2.5})}, EventTableIndexLabel{reference_number("", double{0.0})}, EventTableIndexLabel{reference_number("", double{1.0})}, EventTableIndexLabel{reference_number("", double{1.5})}, EventTableIndexLabel{reference_number("", double{2.0})}, EventTableIndexLabel{reference_null("")}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("float_index", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("float_index: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
}};
    base.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}}}};
    relations.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L2", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}}}};
    const auto expected_index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("default_empty_base", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("default_empty_base: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
}};
    base.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
}};
    incoming.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
}};
    relations.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
}};
    const auto expected_index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("default_all_empty", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("default_all_empty: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}}}};
    base.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
}};
    relations.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("default_range_sorted_no_relations", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("default_range_sorted_no_relations: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    base.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{0})}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("default_range_empty_relations", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("default_range_empty_relations: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
}};
    base.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "Z", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "Z", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "A", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}}}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {EventTableIndexLabel{EventAttribute{"", "rel-a", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-b", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-c", AttributeKind::kString}}}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "A", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "A", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "Z", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "Z", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "Z", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "Z", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}}}};
    const auto expected_index = EventTableIndex{"Index", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("default_empty_base_reordered", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("default_empty_base_reordered: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    base.index = EventTableIndex{"RangeIndex", "int64", {"offset"}, {EventTableIndexLabel{reference_number("", std::int64_t{5})}, EventTableIndexLabel{reference_number("", std::int64_t{7})}, EventTableIndexLabel{reference_number("", std::int64_t{9})}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{9})}, EventTableIndexLabel{reference_number("", std::int64_t{7})}, EventTableIndexLabel{reference_number("", std::int64_t{5})}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("offset_range_empty_relations", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("offset_range_empty_relations: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}}}};
    base.index = EventTableIndex{"RangeIndex", "int64", {"offset"}, {EventTableIndexLabel{reference_number("", std::int64_t{5})}, EventTableIndexLabel{reference_number("", std::int64_t{7})}, EventTableIndexLabel{reference_number("", std::int64_t{9})}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{5})}, EventTableIndexLabel{reference_number("", std::int64_t{7})}, EventTableIndexLabel{reference_number("", std::int64_t{9})}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("offset_range_sorted_empty_relations", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("offset_range_sorted_empty_relations: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}}}};
    base.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L2", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"Index", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("range_nonarithmetic_sort", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("range_nonarithmetic_sort: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}}}};
    base.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{-3})}, EventTableIndexLabel{reference_number("", std::int64_t{-2})}, EventTableIndexLabel{reference_number("", std::int64_t{-1})}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "d", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "e", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}}}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {EventTableIndexLabel{EventAttribute{"", "rel-a", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-b", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-c", AttributeKind::kString}}}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "d", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "d", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "e", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}}}};
    const auto expected_index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{-3})}, EventTableIndexLabel{reference_number("", std::int64_t{-2})}, EventTableIndexLabel{reference_number("", std::int64_t{-1})}, EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("range_contiguous_concat", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("range_contiguous_concat: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "d", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "e", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "f", AttributeKind::kString}}}};
    base.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}, EventTableIndexLabel{reference_number("", std::int64_t{5})}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "R1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "R2", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}}}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {EventTableIndexLabel{reference_number("", std::int64_t{101})}, EventTableIndexLabel{reference_number("", std::int64_t{99})}}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}}}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {EventTableIndexLabel{EventAttribute{"", "rel-a", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-b", AttributeKind::kString}}, EventTableIndexLabel{EventAttribute{"", "rel-c", AttributeKind::kString}}}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i1", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R1", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "i2", AttributeKind::kString}, EventAttribute{"when", "b", AttributeKind::kString}, EventAttribute{"case:concept:name_LEFT", "L1", AttributeKind::kString}, EventAttribute{"case:concept:name_RIGHT", "R2", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b2", AttributeKind::kString}, EventAttribute{"when", "d", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "e", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "f", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"Index", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}, EventTableIndexLabel{reference_number("", std::int64_t{5})}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("range_discontinuous_concat_reordered", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("range_discontinuous_concat_reordered: public index metadata differs");
  }
  {
    auto base = EventTable{{"case:concept:name", "concept:name", "when"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}}}};
    base.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", (std::numeric_limits<std::int64_t>::min)())}, EventTableIndexLabel{reference_number("", std::int64_t{9223372036854775807})}}};
    auto incoming = EventTable{{"case:concept:name", "concept:name", "when"}, {
}};
    incoming.index = EventTableIndex{"Index", "int64", {"incoming_row"}, {}};
    auto relations = EventTable{{"case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
}};
    relations.index = EventTableIndex{"Index", "str", {"relation_row"}, {}};
    const auto expected = EventTable{{"case:concept:name", "concept:name", "when", "case:concept:name_LEFT", "case:concept:name_RIGHT"}, {
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-early", AttributeKind::kString}, EventAttribute{"when", "a", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")},
{EventAttribute{"case:concept:name", "L1", AttributeKind::kString}, EventAttribute{"concept:name", "b1-late", AttributeKind::kString}, EventAttribute{"when", "c", AttributeKind::kString}, reference_null("case:concept:name_LEFT"), reference_null("case:concept:name_RIGHT")}}};
    const auto expected_index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", (std::numeric_limits<std::int64_t>::min)())}, EventTableIndexLabel{reference_number("", std::int64_t{9223372036854775807})}}};
    CaseRelationsOptions options;
    options.case_id_key = "case:concept:name";
    options.timestamp_key = "when";
    const auto actual = merge_case_relations(base, incoming, relations, options);
    reference_check("range_extreme_step", actual, expected);
    EventTableIndex actual_index;
    if (actual.index) actual_index = *actual.index;
    else {
      actual_index.kind = "RangeIndex";
      for (std::size_t i = 0; i < actual.rows.size(); ++i)
        actual_index.labels.push_back(EventTableIndexLabel{reference_number("", static_cast<std::int64_t>(i))});
    }
    if (actual_index != expected_index) throw std::runtime_error("range_extreme_step: public index metadata differs");
  }
}
