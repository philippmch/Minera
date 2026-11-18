// Review DX1: list case keys in the pandas sequence filters. pm4py 2.7.22
// hands the case key to pandas as groupby(key) (start/end activities,
// traces, prefixes and suffixes, variants), groupby([key, timestamp]) (the
// timestamp grouping), set_index(key) (the trace filters) or df[key] (the
// activity split and the consecutive grouping); the shared model in
// pandas_list_key.hpp reads each. In the Polars filters a list case key
// inside another list (sort([key, timestamp])) is a list literal, and Polars
// converts every value of it before building a type: a dict key that is no
// str fails the whole list. Observed black-box (review-harness round-dx,
// pp/mk_s1.py, mk_s2.py, mk_p1.py, mk_p2.py; banked in
// docs/pandas-sequence-filter-dx-bank.json.gz and
// docs/polars-sequence-filter-dx-bank.json.gz).
#include "minera/pandas_sequence_filters.hpp"

#include "../pandas_list_key.hpp"
#include "../polars_lit_model.hpp"

#include <functional>
#include <iostream>
#include <string>
#include <vector>

using namespace minera;

namespace {

int failures = 0;

void expect(bool condition, const std::string& what) {
  if (!condition) {
    ++failures;
    std::cerr << "FAILED: " << what << '\n';
  }
}

using V = PandasFilterValue;

std::string error_of(const std::function<void()>& call) {
  try {
    call();
  } catch (const PandasSequenceFilterError& error) {
    return error.type + ": " + error.message;
  } catch (const std::exception& error) {
    return error.what();
  }
  return {};
}

// cid str ["1", "1", "2", "2", "3"], act str ["a", "b", "a", "c", "b"],
// rd int64 [1, 2, 3, 4, 5]; `rows` keeps the first rows only.
EventTable frame(std::size_t rows = 5) {
  const std::vector<std::string> cid{"1", "1", "2", "2", "3"}, act{"a", "b", "a", "c", "b"};
  EventTable table;
  table.columns = {"cid", "act", "rd"};
  table.dtypes = {"str", "str", "int64"};
  for (std::size_t row = 0; row < rows; ++row) {
    EventAttribute rd{"rd", std::to_string(row + 1), AttributeKind::kInt};
    rd.typed_value = static_cast<std::int64_t>(row + 1);
    table.rows.push_back({EventAttribute{"cid", cid[row]}, EventAttribute{"act", act[row]}, rd});
  }
  return table;
}

PandasFilterParameters parameters(V case_key, const std::string& other_key, V other) {
  return std::vector<PandasFilterParameter>{{"pm4py:param:case_id_key", false, std::move(case_key)},
                                            {other_key, false, std::move(other)}};
}

PandasFilterParameters keyed(V case_key) { return parameters(std::move(case_key), "pm4py:param:activity_key", V::of("act")); }

V labels(const std::vector<std::string>& names) {
  std::vector<V> items;
  for (const auto& name : names) items.push_back(V::of(name));
  return V::list(items);
}

V integers(const std::vector<int>& values) {
  std::vector<V> items;
  for (const auto v : values) items.push_back(V::of(v));
  return V::list(items);
}

std::vector<std::string> column(const EventTable& table, const std::string& name) {
  std::vector<std::string> out;
  for (std::size_t c = 0; c < table.columns.size(); ++c)
    if (table.columns[c] == name)
      for (const auto& row : table.rows) out.push_back(row[c] ? row[c]->value : "<null>");
  return out;
}

void grouped_filters() {
  EventTable out;
  expect(error_of([&] { out = pandas_start_activities_apply(frame(), V::list({V::of("a")}), keyed(labels({"cid"}))); })
                 .empty() &&
             column(out, "rd") == std::vector<std::string>{"1", "2", "3", "4"},
         "one listed column groups like the column");
  expect(error_of([&] {
           out = pandas_start_activities_apply(frame(), V::list({V::of("a")}), keyed(labels({"cid", "act"})));
         }).empty() &&
             column(out, "rd") == std::vector<std::string>{"1", "3"},
         "two listed columns group by both");
  expect(error_of([&] {
           out = pandas_start_activities_apply(frame(), V::list({V::of("a")}), keyed(V::list({integers({0, 0, 1, 1, 0})})));
         }).empty() &&
             column(out, "rd") == std::vector<std::string>{"1", "2", "3", "4", "5"},
         "an array as long as the frame groups its rows");
  std::vector<std::string> nope;
  for (int i = 0; i < 30; ++i) nope.push_back((i < 10 ? "nope0" : "nope") + std::to_string(i));
  expect(error_of([&] { (void)pandas_start_activities_apply(frame(), V::list({V::of("a")}), keyed(labels(nope))); }) ==
             "KeyError: 'nope00'",
         "the first unknown label of a key shorter than the frame");
  expect(error_of([&] {
           out = pandas_trace_filter_apply(frame(), V::list({V::tuple({V::of("a"), V::of("b")})}), keyed(labels({"cid"})));
         }).empty() &&
             column(out, "rd") == std::vector<std::string>{"1", "2"},
         "the trace filter's set_index keeps the listed column's cases");
  expect(error_of([&] { (void)pandas_prefix_filter_apply(frame(), V::of("b"), keyed(labels({"cid"}))); }) ==
             "TypeError: unhashable type: 'list'",
         "the prefix filter's positions per case fail on the list");
}

void timestamp_grouping() {
  EventTable out;
  expect(error_of([&] {
           out = pandas_timestamp_case_grouping_apply(
               frame(), parameters(integers({0, 0, 1, 1, 1}), "pm4py:param:timestamp_key", V::of("act")));
         }).empty() &&
             out.columns == std::vector<std::string>{"level_0", "act", "cid", "rd"} &&
             out.dtypes == std::vector<std::string>{"int64", "str", "str", "int64"} &&
             column(out, "level_0") == std::vector<std::string>{"0", "0", "1", "1", "1"},
         "an array case key comes back first as level_0");
  expect(error_of([&] {
           out = pandas_timestamp_case_grouping_apply(frame(0),
                                                      parameters(V::list({}), "pm4py:param:timestamp_key", V::of("rd")));
         }).empty() &&
             out.columns == std::vector<std::string>{"level_0", "rd", "cid", "act"} &&
             out.dtypes == std::vector<std::string>{"float64", "int64", "str", "str"} && out.rows.empty(),
         "an empty array's level is numpy's float64");
}

void column_keys() {
  expect(error_of([&] { (void)pandas_activity_split_apply(frame(), V::of("a"), keyed(labels({"cid", "cid"}))); }) ==
             "IndexError: index 5 is out of bounds for axis 0 with size 5",
         "the split walks the key's frame flattened past its rows");
  expect(error_of([&] { (void)pandas_activity_split_apply(frame(), V::of("a"), keyed(labels({"cid"}))); }) ==
             "ValueError: Columns must be same length as key",
         "one listed column cannot take the split cases back");
  std::vector<std::string> nope;
  for (int i = 0; i < 30; ++i) nope.push_back((i < 10 ? "nope0" : "nope") + std::to_string(i));
  expect(error_of([&] { (void)pandas_activity_split_apply(frame(), V::of("a"), keyed(labels(nope))); }) ==
             "KeyError: \"None of [Index(['nope00', 'nope01', 'nope02', 'nope03', 'nope04', 'nope05', 'nope06',\\n"
             "       'nope07', 'nope08', 'nope09', 'nope10', 'nope11', 'nope12', 'nope13',\\n"
             "       'nope14', 'nope15', 'nope16', 'nope17', 'nope18', 'nope19', 'nope20',\\n"
             "       'nope21', 'nope22', 'nope23', 'nope24', 'nope25', 'nope26', 'nope27',\\n"
             "       'nope28', 'nope29'],\\n      dtype='str')] are in the [columns]\"",
         "a long Index wraps in the KeyError");
  // Round DZ (Review DZ1-1): adding the key's frame and the activity column
  // aligns the Series on the columns ("cid" then the labels 0 to 4), so a
  // NaN float64 column meets a str activity (pm4py 2.7.22, pandas 3.0.5).
  for (const auto& key : {std::vector<std::string>{"cid"}, std::vector<std::string>{"cid", "act"},
                          std::vector<std::string>{}})
    expect(error_of([&] { (void)pandas_consecutive_act_case_grouping_apply(frame(), keyed(labels(key))); }) ==
               "UFuncTypeError: ufunc 'add' did not contain a loop with signature matching types "
               "(dtype('float64'), dtype('<U1')) -> None",
           "the consecutive grouping's addition with " + std::to_string(key.size()) + " listed names");
}

void index_repr() {
  std::vector<std::string> ints;
  for (int i = 1000; i < 1150; ++i) ints.push_back(std::to_string(i));
  expect(detail::pandas_index_repr(ints, "int64", true) ==
             "Index([1000, 1001, 1002, 1003, 1004, 1005, 1006, 1007, 1008, 1009,\n"
             "       ...\n"
             "       1140, 1141, 1142, 1143, 1144, 1145, 1146, 1147, 1148, 1149],\n"
             "      dtype='int64', length=150)",
         "more than 100 items cut to ten at each end");
  std::vector<std::string> floats;
  for (int i = 0; i < 12; ++i)
    for (const auto* v : {"1.5", "22.25", "3.0"}) floats.push_back(v);
  expect(detail::pandas_index_repr(floats, "float64", true) ==
             "Index([  1.5, 22.25,   3.0,   1.5, 22.25,   3.0,   1.5, 22.25,   3.0,   1.5,\n"
             "       22.25,   3.0,   1.5, 22.25,   3.0,   1.5, 22.25,   3.0,   1.5, 22.25,\n"
             "         3.0,   1.5, 22.25,   3.0,   1.5, 22.25,   3.0,   1.5, 22.25,   3.0,\n"
             "         1.5, 22.25,   3.0,   1.5, 22.25,   3.0],\n"
             "      dtype='float64')",
         "wrapped numbers right-justify to one width");
  expect(detail::pandas_index_repr({"'a'", "'b'"}, "str", false) == "Index(['a', 'b'], dtype='str')",
         "a short Index stays on one line");
}

std::string literal_error(const std::vector<V>& items) {
  try {
    (void)detail::polars_lit::list_literal(items);
  } catch (const PolarsSequenceFilterError& error) {
    return error.type + ": " + error.message;
  }
  return {};
}

void polars_struct_keys() {
  const auto keyed_by = [](V key) { return V::dict({{std::move(key), V::of(1)}}); };
  expect(literal_error({V::of("a"), V::of(1), keyed_by(V::of(0))}) == "TypeError: 'int' object is not an instance of 'str'",
         "an int dict key fails before the String Series meets the int");
  expect(literal_error({V::none(), keyed_by(V::none())}) == "TypeError: 'None' is not an instance of 'str'",
         "a None dict key, after a None item");
  expect(literal_error({V::dict({{V::of("a"), keyed_by(V::of(1.5))}, {V::of(0), V::of(2)}})}) ==
             "TypeError: 'float' object is not an instance of 'str'",
         "each entry's value is converted before the next key");
  expect(literal_error({V::list({V::of(1), keyed_by(V::tuple({V::of(1), V::of(2)}))})}) ==
             "TypeError: 'tuple' object is not an instance of 'str'",
         "a nested list's dicts are read too");
  expect(literal_error({keyed_by(V::of("a"))}).empty(), "str keys build a struct");
}

}  // namespace

int main() {
  grouped_filters();
  timestamp_grouping();
  column_keys();
  index_repr();
  polars_struct_keys();
  if (failures) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review DX1 tests passed\n";
  return 0;
}
