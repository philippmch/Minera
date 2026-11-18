// Review DW1: list case keys as pandas reads them in the pandas filters.
// pm4py 2.7.22 hands the case key to pandas three ways: df[key] (np.asarray
// first: a regular 2-D key's rows turn to tuples, a deeper key fails as an
// Index or on its buffer, bools mask rows), groupby(key) (each item a label,
// an array, a mapping or a callable; a deep array fails when the groups
// factorize) and set_index(key). between walks the cases of the key's frame
// flattened row by row, the relative occurrence filter multiplies the stake
// by df[key].nunique() (a Series), and the LTL filters select
// df[[case, attribute, ...]]. Observed black-box (review-harness round-dw,
// pp/mk_l1.py, mk_l2.py, mk_l3.py, bt/b.py; banked in
// docs/pandas-filter-dw-bank.json.gz).
#include "minera/pandas_between_filter.hpp"

#include "../pandas_list_key.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>
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

[[noreturn]] void fail(const std::string& type, const std::string& message) {
  throw std::runtime_error(type + ": " + message);
}

std::string error_of(const std::function<void()>& call) {
  try {
    call();
  } catch (const PandasBetweenError& error) {
    return error.type + ": " + error.message;
  } catch (const std::exception& error) {
    return error.what();
  }
  return {};
}

EventAttribute text(const std::string& value) { return EventAttribute{"", value}; }

EventAttribute integer(long long value) {
  EventAttribute result{"", std::to_string(value), AttributeKind::kInt};
  result.typed_value = static_cast<std::int64_t>(value);
  return result;
}

EventAttribute boolean(bool value) {
  EventAttribute result{"", value ? "True" : "False", AttributeKind::kBoolean};
  result.typed_value = value;
  return result;
}

EventAttribute none() {
  EventAttribute result{"", ""};
  result.is_null = true;
  return result;
}

EventAttribute sequence(std::vector<EventAttribute> children, AttributeContainerKind kind = AttributeContainerKind::kList) {
  EventAttribute result{"", ""};
  result.container = kind;
  result.children = std::move(children);
  return result;
}

std::string label_repr(const EventAttribute& value) {
  if (value.is_null) return "None";
  if (value.container == AttributeContainerKind::kTuple) {
    std::string out = "(";
    for (std::size_t i = 0; i < value.children.size(); ++i) out += (i ? ", " : "") + label_repr(value.children[i]);
    return out + (value.children.size() == 1 ? ",)" : ")");
  }
  if (value.kind == AttributeKind::kString) return "'" + value.value + "'";
  return value.value;
}

std::string getitem(const std::vector<std::string>& columns, const EventAttribute& key, std::size_t rows) {
  return error_of([&] {
    (void)detail::pandas_getitem_list(columns, key, rows, label_repr, [](const std::string& m) { return m; }, fail);
  });
}

void getitem_keys() {
  const std::vector<std::string> columns{"cid", "act", "rd"};
  expect(getitem(columns, sequence({sequence({text("cid")})}), 3) ==
             "KeyError: None of [Index([('cid',)], dtype='object')] are in the [columns]",
         "a 2-D key's rows are tuples");
  expect(getitem(columns, sequence({sequence({sequence({integer(0)}), sequence({integer(1)})})}), 3) ==
             "ValueError: Index data must be 1-dimensional",
         "a 3-D numeric key is refused as an Index");
  expect(getitem(columns, sequence({sequence({sequence({integer(0), text("a")})})}), 3) ==
             "ValueError: Buffer has wrong number of dimensions (expected 1, got 3)",
         "a 3-D str key fails on its buffer");
  expect(getitem(columns, sequence({sequence({sequence({none()})})}), 3) ==
             "ValueError: Buffer has wrong number of dimensions (expected 1, got 3)",
         "a 3-D object key fails on its buffer");
  expect(getitem(columns, sequence({boolean(true), boolean(false), boolean(true)}), 2) ==
             "ValueError: Item wrong length 3 instead of 2.",
         "a bool key masks rows");
  expect(getitem(columns, sequence({text("cid"), text("nope")}), 3) == "KeyError: ['nope'] not in index",
         "a missing label among found ones");
  expect(getitem(columns, sequence({integer(1), integer(2)}), 3) ==
             "KeyError: None of [Index([1, 2], dtype='int64')] are in the [columns]",
         "int labels make an int64 Index");
  expect(getitem(columns, sequence({sequence({integer(1)}), sequence({integer(2), integer(3)})}), 3) ==
             "TypeError: unhashable type: 'list'",
         "a ragged key keeps its lists, which do not hash");
  std::vector<std::size_t> found;
  expect(error_of([&] {
           found = detail::pandas_getitem_list(columns, sequence({text("rd"), text("cid"), text("rd")}), 3, label_repr,
                                               [](const std::string& m) { return m; }, fail);
         }).empty() &&
             found == std::vector<std::size_t>{2, 0, 2},
         "found labels give their positions, duplicates kept");
}

void groupers() {
  const auto column = [](const EventAttribute& item) -> std::optional<std::size_t> {
    if (item.kind == AttributeKind::kString && !item.is_null && item.container == AttributeContainerKind::kScalar &&
        item.value == "cid")
      return 0;
    return std::nullopt;
  };
  const auto value = [](const detail::PandasListGrouper& g, std::size_t row) -> const EventAttribute& { return g.values[row]; };
  std::vector<detail::PandasListGrouper> built;
  expect(error_of([&] {
           built = detail::pandas_list_groupers(sequence({sequence({sequence({sequence({integer(0)})})})}), 1, column,
                                                label_repr, fail);
         }).empty(),
         "a deep array grouper builds");
  expect(built.size() == 1 && built[0].depth == 3 &&
             error_of([&] { detail::pandas_list_factorize(built[0], 1, value, fail); }) ==
                 "ValueError: Buffer has wrong number of dimensions (expected 1, got 3)",
         "a deep array grouper fails when the groups factorize");
  expect(error_of([&] {
           (void)detail::pandas_list_groupers(sequence({text("cid"), none()}), 2, column, label_repr, fail);
         }) == "TypeError: 'NoneType' object is not callable",
         "None is called on each index label");
  expect(error_of([&] {
           (void)detail::pandas_list_groupers(sequence({sequence({integer(0)})}), 2, column, label_repr, fail);
         }) == "ValueError: Grouper and axis must be same length",
         "an array grouper must be as long as the frame");
  expect(error_of([&] {
           built = detail::pandas_list_groupers(sequence({sequence({sequence({integer(0), integer(1)}),
                                                                    sequence({integer(1), integer(2)})})}),
                                                2, column, label_repr, fail);
         }).empty() &&
             built.size() == 1 && built[0].values.size() == 2 &&
             built[0].values[0].container == AttributeContainerKind::kTuple,
         "a 2-D array grouper's rows are tuples");
}

EventTable frame(const std::vector<std::vector<std::string>>& columns, const std::vector<std::string>& names,
                 const std::vector<std::string>& dtypes) {
  EventTable table;
  table.columns = names;
  table.dtypes = dtypes;
  for (std::size_t row = 0; row < columns.front().size(); ++row) {
    EventTable::Row cells;
    for (std::size_t c = 0; c < names.size(); ++c) {
      EventAttribute cell{names[c], columns[c][row]};
      if (dtypes[c] == "int64") {
        cell.kind = AttributeKind::kInt;
        cell.typed_value = static_cast<std::int64_t>(std::stoll(columns[c][row]));
      }
      cells.push_back(cell);
    }
    table.rows.push_back(std::move(cells));
  }
  return table;
}

std::string between(const EventTable& table, const EventAttribute& key) {
  PandasBetweenOptions options;
  options.case_id_key = key;
  options.activity_key = text("act");
  return error_of([&] { (void)filter_pandas_between(table, text("a"), text("b"), options); });
}

void between_keys() {
  const auto table = frame({{"1", "1", "1", "2", "2"}, {"a", "x", "b", "a", "b"}, {"1", "2", "3", "4", "5"}},
                           {"cid", "act", "rd"}, {"str", "str", "int64"});
  expect(between(table, sequence({text("cid")})) == "ValueError: Columns must be same length as key",
         "one listed column of five rows cannot take the labels back");
  expect(between(table, sequence({text("cid"), text("cid")})) ==
             "IndexError: index 5 is out of bounds for axis 0 with size 5",
         "the flattened cases run past the frame");
  expect(between(table, sequence({text("cid"), text("rd")})) ==
             "TypeError: '<' not supported between instances of 'str' and 'int'",
         "an object array of str and int does not sort");
  expect(between(table, sequence({text("nope")})) ==
             "KeyError: \"None of [Index(['nope'], dtype='str')] are in the [columns]\"",
         "the key's columns are read first");
  const auto one = frame({{"1"}, {"a"}}, {"cid", "act"}, {"str", "str"});
  expect(between(one, sequence({text("cid")})) == "NotImplementedError: fillna is not defined for MultiIndex",
         "one row and one listed column fail on a MultiIndex");
  const auto spread = frame({{"a", "c", "a", "a"}, {"b", "d", "a", "b"}, {"a", "b", "a", "b"}}, {"c1", "c2", "act"},
                            {"str", "str", "str"});
  expect(between(spread, sequence({text("c1"), text("c2")})) == "ValueError: Columns must be same length as key",
         "cases whose flattened spans stay in the frame walk to the end");
  auto dated = frame({{"1", "2"}, {"a", "b"}, {"2021-03-01T08:00:00+00:00", "2021-03-01T09:00:00+00:00"}},
                     {"cid", "act", "ts"}, {"str", "str", "datetime64[us, UTC]"});
  for (auto& row : dated.rows) row[2]->kind = AttributeKind::kDate;
  expect(between(dated, sequence({text("cid"), text("ts")})) ==
             "TypeError: '<' not supported between instances of 'str' and 'Timestamp'",
         "a datetime column joins an object array as Timestamps");
}

}  // namespace

int main() {
  getitem_keys();
  groupers();
  between_keys();
  if (failures) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "review DW1 tests passed\n";
  return 0;
}
