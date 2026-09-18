// Public replay grouping SHA256: 974c33f7e4e5cc5fe017118163d88b7ef26026b7d82060ce2df62226e4fa459d
#include "minera/conformance.hpp"
#pragma once
#include "case_relations_reference_cases.hpp"
#include <limits>
#include <iomanip>
#include <sstream>
#include <charconv>

namespace case_relations_dtype_reference {
using namespace case_relations_reference;

template<class T> EventAttribute dtype_number(const std::string& key, T value) {
  auto cell = reference_number(key, value);
  if constexpr (std::is_floating_point_v<T>) {
    std::ostringstream text;
    text << std::setprecision(std::numeric_limits<T>::max_digits10) << value;
    cell.value = text.str();
  }
  return cell;
}
template<class T> bool dtype_integer_text(const std::string& text, T expected) {
  T parsed{};
  const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size() && parsed == expected;
}
inline bool dtype_projection_valid(const EventAttribute& cell) {
  if (cell.is_null || cell.container != AttributeContainerKind::kScalar) return true;
  if (const auto* v = std::get_if<std::int64_t>(&cell.typed_value)) return dtype_integer_text(cell.value, *v);
  if (const auto* v = std::get_if<std::uint64_t>(&cell.typed_value)) return dtype_integer_text(cell.value, *v);
  if (const auto* v = std::get_if<bool>(&cell.typed_value))
    return *v ? (cell.value == "true" || cell.value == "True" || cell.value == "1")
              : (cell.value == "false" || cell.value == "False" || cell.value == "0");
  if (const auto* v = std::get_if<double>(&cell.typed_value)) {
    try { std::size_t used{}; const auto parsed = std::stod(cell.value, &used);
      return used == cell.value.size() && parsed == *v;
    } catch (...) { return false; }
  }
  if (const auto* v = std::get_if<NativeDate>(&cell.typed_value)) return cell.value == v->value;
  return true;
}

inline EventAttribute dtype_tuple(const std::string& key,
                                  std::vector<EventAttribute> children) {
  EventAttribute result{key, "", AttributeKind::kString};
  result.container = AttributeContainerKind::kTuple;
  result.children = std::move(children);
  return result;
}
inline bool dtype_attribute_equal(const EventAttribute& actual, const EventAttribute& expected) {
  if (actual.is_null != expected.is_null) return false;
  if (actual.is_null) return true;
  if (!dtype_projection_valid(actual)) return false;
  if (actual.kind != expected.kind || actual.container != expected.container ||
      actual.children.size() != expected.children.size())
    return false;
  bool scalar_equal = actual.typed_value == expected.typed_value;
  if (const auto* a = std::get_if<std::uint64_t>(&actual.typed_value)) {
    if (const auto* e = std::get_if<std::int64_t>(&expected.typed_value))
      scalar_equal = *e >= 0 && *a == static_cast<std::uint64_t>(*e);
  }
  if (const auto* a = std::get_if<std::int64_t>(&actual.typed_value)) {
    if (const auto* e = std::get_if<std::uint64_t>(&expected.typed_value))
      scalar_equal = *a >= 0 && static_cast<std::uint64_t>(*a) == *e;
  }
  if (!scalar_equal) return false;
  if (std::holds_alternative<std::monostate>(expected.typed_value) && actual.value != expected.value)
    return false;
  for (std::size_t i = 0; i < expected.children.size(); ++i)
    if (!dtype_attribute_equal(actual.children[i], expected.children[i])) return false;
  return true;
}
inline void check_dtype_table(const std::string& name, const EventTable& actual,
                              const EventTable& expected) {
  actual.validate();
  if (actual.columns != expected.columns || actual.rows.size() != expected.rows.size())
    throw std::runtime_error(name + ": output shape/columns differ");
  if (actual.dtypes != expected.dtypes) {
    std::string detail;
    for (std::size_t i = 0; i < expected.columns.size(); ++i)
      detail += " " + expected.columns[i] + "=" +
          (i < actual.dtypes.size() ? actual.dtypes[i] : "<absent>") +
          " (expected " + expected.dtypes[i] + ")";
    throw std::runtime_error(name + ": column dtypes differ:" + detail);
  }
  for (std::size_t column = 0; column < expected.columns.size(); ++column) {
    const auto actual_category = actual.categorical_dtypes.empty()
        ? std::optional<EventTableCategoricalDtype>{} : actual.categorical_dtypes[column];
    const auto expected_category = expected.categorical_dtypes.empty()
        ? std::optional<EventTableCategoricalDtype>{} : expected.categorical_dtypes[column];
    if (actual_category != expected_category)
      throw std::runtime_error(name + ": categorical metadata differs for " + expected.columns[column]);
    for (std::size_t row = 0; row < expected.rows.size(); ++row) {
      const auto& observed = actual.rows[row][column];
      const auto& reference = expected.rows[row][column];
      if (observed && observed->key != actual.columns[column])
        throw std::runtime_error(name + ": output cell key differs");
      if (reference_missing(reference) != reference_missing(observed) ||
          (!reference_missing(reference) && !dtype_attribute_equal(*observed, *reference)))
        throw std::runtime_error(name + ": scalar value/kind differs for " + expected.columns[column]);
    }
  }
}

}

#include "minera/dataframe_format.hpp"
#include <algorithm>
namespace replay_dataframe_grouping_reference {
using namespace case_relations_dtype_reference;
inline EventAttribute format_container(const std::string& key,
    AttributeContainerKind kind, std::vector<EventAttribute> children) {
  EventAttribute result{key, "", AttributeKind::kString};
  result.container = kind;
  result.children = std::move(children);
  return result;
}
inline EventTableIndex format_index(const EventTable& table) {
  if (table.index) return *table.index;
  EventTableIndex result;
  result.kind = "RangeIndex";
  for (std::size_t i = 0; i < table.rows.size(); ++i)
    result.labels.push_back({reference_number("", static_cast<std::int64_t>(i))});
  return result;
}
inline void check_format_table(const std::string& name, const EventTable& actual,
                               const EventTable& expected) {
  if (actual.columns != expected.columns || actual.rows.size() != expected.rows.size() ||
      actual.dtypes != expected.dtypes)
    throw std::runtime_error(name + ": columns, rows or dtypes differ");
  if (actual.categorical_dtypes != expected.categorical_dtypes) {
    const auto has_category = [](const auto& values) {
      return std::any_of(values.begin(), values.end(), [](const auto& value) { return value.has_value(); });
    };
    if (has_category(actual.categorical_dtypes) || has_category(expected.categorical_dtypes))
      throw std::runtime_error(name + ": categorical metadata differs");
  }
  for (std::size_t row = 0; row < expected.rows.size(); ++row) {
    if (actual.rows[row].size() != expected.rows[row].size())
      throw std::runtime_error(name + ": row width differs");
    for (std::size_t column = 0; column < expected.columns.size(); ++column) {
      const auto& observed = actual.rows[row][column];
      const auto& reference = expected.rows[row][column];
      if (observed && observed->key != expected.columns[column])
        throw std::runtime_error(name + ": cell key differs");
      if (reference_missing(observed) != reference_missing(reference) ||
          (!reference_missing(reference) && !dtype_attribute_equal(*observed, *reference)))
        throw std::runtime_error(name + ": cell differs at row " + std::to_string(row) + " column " + expected.columns[column]);
    }
  }
  if (format_index(actual) != format_index(expected))
    throw std::runtime_error(name + ": index differs");
  if (actual.metadata.properties != expected.metadata.properties)
    throw std::runtime_error(name + ": dataframe attrs differ");
}
inline void check_replay_grouping_0() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 1) throw std::runtime_error("upper: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {0};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("upper: replay path/counters differ"); }
  }
  check_format_table("upper: input after", input, expected_input);
}
inline void check_replay_grouping_1() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 1) throw std::runtime_error("upper_reverse: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {1};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("upper_reverse: replay path/counters differ"); }
  }
  check_format_table("upper_reverse: input after", input, expected_input);
}
inline void check_replay_grouping_2() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 2) throw std::runtime_error("wide: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {1, 2};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("wide: replay path/counters differ"); }
    { const auto& trace = actual[1]; const std::vector<std::size_t> expected = {0};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("wide: replay path/counters differ"); }
  }
  check_format_table("wide: input after", input, expected_input);
}
inline void check_replay_grouping_3() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 1) throw std::runtime_error("wide_float_first: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {1};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("wide_float_first: replay path/counters differ"); }
  }
  check_format_table("wide_float_first: input after", input, expected_input);
}
inline void check_replay_grouping_4() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 2) throw std::runtime_error("wide_only_int: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {1};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("wide_only_int: replay path/counters differ"); }
    { const auto& trace = actual[1]; const std::vector<std::size_t> expected = {0};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("wide_only_int: replay path/counters differ"); }
  }
  check_format_table("wide_only_int: input after", input, expected_input);
}
inline void check_replay_grouping_5() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 2) throw std::runtime_error("wide_float_unrelated: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {2};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("wide_float_unrelated: replay path/counters differ"); }
    { const auto& trace = actual[1]; const std::vector<std::size_t> expected = {0};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("wide_float_unrelated: replay path/counters differ"); }
  }
  check_format_table("wide_float_unrelated: input after", input, expected_input);
}
inline void check_replay_grouping_6() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{-9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{-9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{-1.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{-9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{-9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{-1.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 2) throw std::runtime_error("negative: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {1};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("negative: replay path/counters differ"); }
    { const auto& trace = actual[1]; const std::vector<std::size_t> expected = {2};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("negative: replay path/counters differ"); }
  }
  check_format_table("negative: input after", input, expected_input);
}
inline void check_replay_grouping_7() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_number("case:concept:name", std::uint64_t{18446744073709551615ULL}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551614ULL}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_number("case:concept:name", std::uint64_t{18446744073709551615ULL}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551614ULL}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 2) throw std::runtime_error("uint_only: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {1};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("uint_only: replay path/counters differ"); }
    { const auto& trace = actual[1]; const std::vector<std::size_t> expected = {0};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("uint_only: replay path/counters differ"); }
  }
  check_format_table("uint_only: input after", input, expected_input);
}
inline void check_replay_grouping_8() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_number("case:concept:name", std::uint64_t{18446744073709551615ULL}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551614ULL}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{reference_number("case:concept:name", std::uint64_t{18446744073709551615ULL}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551614ULL}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 2) throw std::runtime_error("uint_float: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {2};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("uint_float: replay path/counters differ"); }
    { const auto& trace = actual[1]; const std::vector<std::size_t> expected = {0};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("uint_float: replay path/counters differ"); }
  }
  check_format_table("uint_float: input after", input, expected_input);
}
inline void check_replay_grouping_9() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 4) throw std::runtime_error("bool_suppresses: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {2};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("bool_suppresses: replay path/counters differ"); }
    { const auto& trace = actual[1]; const std::vector<std::size_t> expected = {3};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("bool_suppresses: replay path/counters differ"); }
    { const auto& trace = actual[2]; const std::vector<std::size_t> expected = {1};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("bool_suppresses: replay path/counters differ"); }
    { const auto& trace = actual[3]; const std::vector<std::size_t> expected = {0};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("bool_suppresses: replay path/counters differ"); }
  }
  check_format_table("bool_suppresses: input after", input, expected_input);
}
inline void check_replay_grouping_10() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "x", AttributeKind::kString}, EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "x", AttributeKind::kString}, EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 4) throw std::runtime_error("string_suppresses: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {2};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("string_suppresses: replay path/counters differ"); }
    { const auto& trace = actual[1]; const std::vector<std::size_t> expected = {1};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("string_suppresses: replay path/counters differ"); }
    { const auto& trace = actual[2]; const std::vector<std::size_t> expected = {0};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("string_suppresses: replay path/counters differ"); }
    { const auto& trace = actual[3]; const std::vector<std::size_t> expected = {3};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("string_suppresses: replay path/counters differ"); }
  }
  check_format_table("string_suppresses: input after", input, expected_input);
}
inline void check_replay_grouping_11() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 1) throw std::runtime_error("upper_repeat: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {0, 2};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("upper_repeat: replay path/counters differ"); }
  }
  check_format_table("upper_repeat: input after", input, expected_input);
}
inline void check_replay_grouping_12() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{-1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551615ULL}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551614ULL}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{-1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551615ULL}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551614ULL}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 3) throw std::runtime_error("signed_unsigned: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {0};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("signed_unsigned: replay path/counters differ"); }
    { const auto& trace = actual[1]; const std::vector<std::size_t> expected = {2};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("signed_unsigned: replay path/counters differ"); }
    { const auto& trace = actual[2]; const std::vector<std::size_t> expected = {1};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("signed_unsigned: replay path/counters differ"); }
  }
  check_format_table("signed_unsigned: input after", input, expected_input);
}
inline void check_replay_grouping_13() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "E", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "E", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 4) throw std::runtime_error("bool_first: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {4};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("bool_first: replay path/counters differ"); }
    { const auto& trace = actual[1]; const std::vector<std::size_t> expected = {0, 1};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("bool_first: replay path/counters differ"); }
    { const auto& trace = actual[2]; const std::vector<std::size_t> expected = {3};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("bool_first: replay path/counters differ"); }
    { const auto& trace = actual[3]; const std::vector<std::size_t> expected = {2};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("bool_first: replay path/counters differ"); }
  }
  check_format_table("bool_first: input after", input, expected_input);
}
inline void check_replay_grouping_14() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "E", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", true), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{0.5}), EventAttribute{"concept:name", "E", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}, EventTableIndexLabel{reference_number("", std::int64_t{4})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 3) throw std::runtime_error("float_before_bool: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {4};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("float_before_bool: replay path/counters differ"); }
    { const auto& trace = actual[1]; const std::vector<std::size_t> expected = {0, 1};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("float_before_bool: replay path/counters differ"); }
    { const auto& trace = actual[2]; const std::vector<std::size_t> expected = {2};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("float_before_bool: replay path/counters differ"); }
  }
  check_format_table("float_before_bool: input after", input, expected_input);
}
inline void check_replay_grouping_15() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 3) throw std::runtime_error("int_before_float: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {0, 1};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("int_before_float: replay path/counters differ"); }
    { const auto& trace = actual[1]; const std::vector<std::size_t> expected = {3};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("int_before_float: replay path/counters differ"); }
    { const auto& trace = actual[2]; const std::vector<std::size_t> expected = {2};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("int_before_float: replay path/counters differ"); }
  }
  check_format_table("int_before_float: input after", input, expected_input);
}
inline void check_replay_grouping_18() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::numeric_limits<double>::infinity()), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", -std::numeric_limits<double>::infinity()), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551615ULL}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551614ULL}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::numeric_limits<double>::infinity()), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", -std::numeric_limits<double>::infinity()), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551615ULL}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{reference_number("case:concept:name", std::uint64_t{18446744073709551614ULL}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 3) throw std::runtime_error("infinity: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {1};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("infinity: replay path/counters differ"); }
    { const auto& trace = actual[1]; const std::vector<std::size_t> expected = {2};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("infinity: replay path/counters differ"); }
    { const auto& trace = actual[2]; const std::vector<std::size_t> expected = {0};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("infinity: replay path/counters differ"); }
  }
  check_format_table("infinity: input after", input, expected_input);
}
inline void check_replay_grouping_19() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "D", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}, EventTableIndexLabel{reference_number("", std::int64_t{3})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 1) throw std::runtime_error("float_first_wide_twice: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {1, 3};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("float_first_wide_twice: replay path/counters differ"); }
  }
  check_format_table("float_first_wide_twice: input after", input, expected_input);
}
inline void check_replay_grouping_21() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 1) throw std::runtime_error("prior_equal: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {0, 1};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_equal: replay path/counters differ"); }
  }
  check_format_table("prior_equal: input after", input, expected_input);
}
inline void check_replay_grouping_22() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 1) throw std::runtime_error("prior_wide_equal: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {0, 1};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_wide_equal: replay path/counters differ"); }
  }
  check_format_table("prior_wide_equal: input after", input, expected_input);
}
inline void check_replay_grouping_23() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 2) throw std::runtime_error("prior_wide_distinct: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {1};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_wide_distinct: replay path/counters differ"); }
    { const auto& trace = actual[1]; const std::vector<std::size_t> expected = {0};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_wide_distinct: replay path/counters differ"); }
  }
  check_format_table("prior_wide_distinct: input after", input, expected_input);
}
inline void check_replay_grouping_24() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{9007199254740993}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9007199254740992}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{9007199254740992.0}), EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 2) throw std::runtime_error("prior_wide_mixed: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {1, 2};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_wide_mixed: replay path/counters differ"); }
    { const auto& trace = actual[1]; const std::vector<std::size_t> expected = {0};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_wide_mixed: replay path/counters differ"); }
  }
  check_format_table("prior_wide_mixed: input after", input, expected_input);
}
inline void check_replay_grouping_25() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", double{9.223372036854776e+18}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", std::int64_t{9223372036854775807}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 1) throw std::runtime_error("prior_upper: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {0};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_upper: replay path/counters differ"); }
  }
  check_format_table("prior_upper: input after", input, expected_input);
}
inline void check_replay_grouping_26() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{-1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{-1.5}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{-1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{-1.5}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 2) throw std::runtime_error("prior_small_fraction: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {1};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_small_fraction: replay path/counters differ"); }
    { const auto& trace = actual[1]; const std::vector<std::size_t> expected = {0};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_small_fraction: replay path/counters differ"); }
  }
  check_format_table("prior_small_fraction: input after", input, expected_input);
}
inline void check_replay_grouping_27() {
  EventTable input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  input.metadata.properties = {};
  EventTable expected_input{{"case:concept:name", "concept:name", "time:timestamp"}, {
{dtype_number("case:concept:name", std::int64_t{1}), EventAttribute{"concept:name", "A", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{dtype_number("case:concept:name", double{1.0}), EventAttribute{"concept:name", "B", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")},
{EventAttribute{"case:concept:name", "1", AttributeKind::kString}, EventAttribute{"concept:name", "C", AttributeKind::kString}, reference_date("time:timestamp", "2024-01-01T00:00:00+00:00")}}};
  expected_input.dtypes = {"object", "str", "datetime64[us, UTC]"};
  expected_input.categorical_dtypes = {std::nullopt, std::nullopt, std::nullopt};
  expected_input.index = EventTableIndex{"RangeIndex", "int64", {std::nullopt}, {EventTableIndexLabel{reference_number("", std::int64_t{0})}, EventTableIndexLabel{reference_number("", std::int64_t{1})}, EventTableIndexLabel{reference_number("", std::int64_t{2})}}};
  expected_input.metadata.properties = {};
  PetriNet net; const auto place = net.add_place(); net.set_source(place); net.set_sink(place); net.set_activities({"A", "B", "C", "D", "E", "F", "G", "H"});
  for (std::size_t i=0; i<8; ++i) net.add_transition(i, {place}, {place});
  const auto lazy = LazyEventTable::from_table(input);
  for (const auto view : {EventDataView(input), EventDataView(lazy)}) {
    const auto actual = replay_token_traces(view, net);
    if (actual.size() != 2) throw std::runtime_error("prior_string_mixed: diagnostic count differs");
    { const auto& trace = actual[0]; const std::vector<std::size_t> expected = {0, 1};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 3 || trace.produced != 3 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_string_mixed: replay path/counters differ"); }
    { const auto& trace = actual[1]; const std::vector<std::size_t> expected = {2};
      if (trace.activated_transitions != expected || trace.trace_is_fit != true || trace.trace_fitness != 1.0 || trace.consumed != 2 || trace.produced != 2 || trace.missing != 0 || trace.remaining != 0) throw std::runtime_error("prior_string_mixed: replay path/counters differ"); }
  }
  check_format_table("prior_string_mixed: input after", input, expected_input);
}
}
inline void check_public_replay_dataframe_grouping() {
  replay_dataframe_grouping_reference::check_replay_grouping_0();
  replay_dataframe_grouping_reference::check_replay_grouping_1();
  replay_dataframe_grouping_reference::check_replay_grouping_2();
  replay_dataframe_grouping_reference::check_replay_grouping_3();
  replay_dataframe_grouping_reference::check_replay_grouping_4();
  replay_dataframe_grouping_reference::check_replay_grouping_5();
  replay_dataframe_grouping_reference::check_replay_grouping_6();
  replay_dataframe_grouping_reference::check_replay_grouping_7();
  replay_dataframe_grouping_reference::check_replay_grouping_8();
  replay_dataframe_grouping_reference::check_replay_grouping_9();
  replay_dataframe_grouping_reference::check_replay_grouping_10();
  replay_dataframe_grouping_reference::check_replay_grouping_11();
  replay_dataframe_grouping_reference::check_replay_grouping_12();
  replay_dataframe_grouping_reference::check_replay_grouping_13();
  replay_dataframe_grouping_reference::check_replay_grouping_14();
  replay_dataframe_grouping_reference::check_replay_grouping_15();
  replay_dataframe_grouping_reference::check_replay_grouping_18();
  replay_dataframe_grouping_reference::check_replay_grouping_19();
  replay_dataframe_grouping_reference::check_replay_grouping_21();
  replay_dataframe_grouping_reference::check_replay_grouping_22();
  replay_dataframe_grouping_reference::check_replay_grouping_23();
  replay_dataframe_grouping_reference::check_replay_grouping_24();
  replay_dataframe_grouping_reference::check_replay_grouping_25();
  replay_dataframe_grouping_reference::check_replay_grouping_26();
  replay_dataframe_grouping_reference::check_replay_grouping_27();
}
