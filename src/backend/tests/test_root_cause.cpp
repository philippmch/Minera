#include "minera/root_cause.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using minera::RootCauseObservation;
using minera::RootCauseOptions;
using minera::fit_root_cause_model;

RootCauseObservation numeric(double value, std::size_t trace = 0) {
  RootCauseObservation observation;
  observation.trace_index = trace;
  minera::EventAttribute attribute;
  attribute.key = "cost";
  attribute.kind = minera::AttributeKind::kFloat;
  attribute.typed_value = value;
  attribute.value = std::to_string(value);
  observation.context.attributes.push_back(attribute);
  return observation;
}

RootCauseObservation categorical(const std::string& value) {
  RootCauseObservation observation;
  observation.context.attributes.push_back({"group", value});
  return observation;
}

template <typename Action> void rejects(Action action) {
  bool threw = false;
  try { action(); } catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}

int predict(const minera::RootCauseModel& model, const std::vector<double>& row) {
  int index = 0;
  while (model.tree[index].feature_index >= 0) {
    const auto& node = model.tree[index];
    const float value = static_cast<float>(row[node.feature_index]);
    index = (std::isnan(value) ? node.missing_go_to_left : node.missing_only || value <= node.threshold)
                ? node.left : node.right;
  }
  return model.tree[index].class_counts[1] > model.tree[index].class_counts[0] ? 1 : 0;
}

int depth(const minera::RootCauseModel& model, int index = 0) {
  const auto& node = model.tree[index];
  if (node.feature_index < 0) return 0;
  return 1 + std::max(depth(model, node.left), depth(model, node.right));
}

void test_numeric_tree() {
  const RootCauseOptions options{{}, {"cost"}};
  const auto model = fit_root_cause_model({numeric(1, 3), numeric(2, 4)},
                                        {numeric(8, 7), numeric(9, 9)}, options);
  assert(model && model->tree.size() == 3);
  assert(model->feature_names == std::vector<std::string>{"event:cost"});
  assert((model->classes == std::vector<std::string>{"fit", "underfed"}));
  assert((model->target == std::vector<int>{0, 0, 1, 1}));
  assert((model->source_trace_indices == std::vector<std::size_t>{3, 4, 7, 9}));
  assert(model->tree[0].threshold == 5.0);
  assert((model->tree[0].class_counts == std::array<std::size_t, 2>{2, 2}));
  for (std::size_t i = 0; i < model->data.size(); ++i)
    assert(predict(*model, model->data[i]) == model->target[i]);
  auto text_number = categorical("unused");
  text_number.context.attributes.push_back({"cost", " 2.5 "});
  assert(fit_root_cause_model({text_number}, {numeric(8)}, options)->data[0][0] == 2.5);
  const auto rounded = fit_root_cause_model({numeric(0.1)}, {numeric(0.2)}, options);
  assert(rounded->data[0][0] == 0.1);
  assert(rounded->tree[0].threshold == (static_cast<double>(0.1f) + static_cast<double>(0.2f)) / 2);
  const auto collapsed = fit_root_cause_model({numeric(16777216)}, {numeric(16777217)}, options);
  assert(collapsed->tree.size() == 1);
  for (const double delta : {1e-8, 1e-7})
    assert(fit_root_cause_model({numeric(0)}, {numeric(delta)}, options)->tree.size() == 1);
  const auto near = fit_root_cause_model({numeric(0)}, {numeric(1.1e-7)}, options);
  assert(near->tree.size() == 3);
  assert(near->tree[0].threshold == static_cast<double>(static_cast<float>(1.1e-7)) / 2);
}

void test_encoding() {
  const auto model = fit_root_cause_model({categorical("a"), categorical("a")},
                                        {categorical("b"), categorical("b")}, {{"group"}, {}});
  assert((model->feature_names == std::vector<std::string>{"event:group@a", "event:group@b"}));
  assert((model->data[0] == std::vector<double>{1, 0}));
  assert((model->data[2] == std::vector<double>{0, 1}));
  // Either one-hot column yields the same optimal Gini split. Determinism
  // chooses the first column and both children are pure.
  assert(model->tree[0].feature_index == 0);
  assert(model->tree[0].threshold == 0.5);
  assert(model->tree[model->tree[0].left].class_counts[0] == 0);
  assert(model->tree[model->tree[0].right].class_counts[1] == 0);

  auto absent = RootCauseObservation{};
  auto null = categorical("");
  null.context.attributes[0].is_null = true;
  auto boolean = categorical("true");
  boolean.context.attributes[0].kind = minera::AttributeKind::kBoolean;
  boolean.context.attributes[0].typed_value = true;
  const auto missing = fit_root_cause_model({absent, null}, {boolean}, {{"group"}, {}});
  assert((missing->feature_names == std::vector<std::string>{"event:group@None", "event:group@True", "event:group@UNDEFINED"}));
  auto real_a = numeric(2.0), real_b = numeric(3.5);
  const auto reals = fit_root_cause_model({real_a}, {real_b}, {{"cost"}, {}});
  assert((reals->feature_names == std::vector<std::string>{"event:cost@2.0", "event:cost@3.5"}));

  auto a = numeric(2), b = numeric(9);
  a.context.attributes.push_back({"b", "q"});
  a.context.attributes.push_back({"a", "z"});
  a.context.attributes.push_back({"x", "1"});
  b.context.attributes.push_back({"b", "r"});
  b.context.attributes.push_back({"a", "a"});
  b.context.attributes.push_back({"x", "8"});
  const auto ordered = fit_root_cause_model({a}, {b}, {{"b", "a"}, {"cost", "x"}});
  assert((ordered->feature_names == std::vector<std::string>{"event:b@q", "event:b@r", "event:a@a", "event:a@z", "event:cost", "event:x"}));
  assert((ordered->data[0] == std::vector<double>{1, 0, 0, 1, 2, 1}));
  const auto duplicate = fit_root_cause_model({a}, {b}, {{}, {"cost", "cost"}});
  assert((duplicate->data[0] == std::vector<double>{0, 2}));
  assert(duplicate->tree[0].feature_index == 1);

  RootCauseObservation projected;
  projected.context.resource = "alice";
  projected.context.resource_source_key = "performer";
  const auto custom = fit_root_cause_model({projected}, {absent}, {{"performer"}, {}});
  assert((custom->feature_names == std::vector<std::string>{"event:performer@UNDEFINED", "event:performer@alice"}));
}

void test_balancing() {
  RootCauseOptions options{{}, {"cost"}};
  const std::vector<RootCauseObservation> fit{numeric(1, 0), numeric(2, 1)};
  const std::vector<RootCauseObservation> bad{numeric(7, 2), numeric(8, 3), numeric(9, 4), numeric(10, 5), numeric(11, 6)};
  assert(fit_root_cause_model(fit, bad, options)->data.size() == 7);
  options.enable_multiplier = true;
  const auto model = fit_root_cause_model(fit, bad, options, "containing");
  assert(model->data.size() == 9 && model->fit_count == 2 && model->problem_count == 5);
  assert((model->source_trace_indices == std::vector<std::size_t>{0, 1, 0, 1, 2, 3, 4, 5, 6}));
  assert(model->classes[1] == "containing");
  assert(fit_root_cause_model(fit, {bad[0], bad[1], bad[2]}, options)->data.size() == 5);
  const auto opposite = fit_root_cause_model(bad, {fit[0]}, options);
  assert((opposite->tree[0].class_counts == std::array<std::size_t, 2>{5, 5}));
}

void test_missing_and_errors() {
  const RootCauseOptions options{{}, {"cost"}};
  assert(!fit_root_cause_model({}, {numeric(1)}, {}));
  assert(!fit_root_cause_model({numeric(1)}, {}, {}));
  rejects([&] { (void)fit_root_cause_model({numeric(1)}, {numeric(2)}, {}); });
  rejects([&] { (void)fit_root_cause_model({RootCauseObservation{}}, {numeric(2)}, options); });
  auto null = numeric(1);
  null.context.attributes[0].is_null = true;
  rejects([&] { (void)fit_root_cause_model({null}, {numeric(2)}, options); });
  for (const double value : {std::numeric_limits<double>::infinity(), 1e100})
    rejects([&] { (void)fit_root_cause_model({numeric(value)}, {numeric(2)}, options); });
  auto invalid = numeric(1);
  invalid.context.attributes[0].typed_value = std::monostate{};
  invalid.context.attributes[0].value = "3tail";
  rejects([&] { (void)fit_root_cause_model({invalid}, {numeric(2)}, options); });
  invalid.context.attributes[0].value = "1";
  invalid.context.attributes[0].typed_value = minera::NativeDate{"2026-01-02T00:00:00Z"};
  rejects([&] { (void)fit_root_cause_model({invalid}, {numeric(2)}, options); });
  invalid.context.attributes[0].typed_value = std::monostate{};
  invalid.context.attributes[0].kind = minera::AttributeKind::kDate;
  rejects([&] { (void)fit_root_cause_model({invalid}, {numeric(2)}, options); });
  const auto nan = numeric(std::numeric_limits<double>::quiet_NaN());
  const auto model = fit_root_cause_model({nan, numeric(2)}, {numeric(8), numeric(9)}, options);
  assert(std::isnan(model->data[0][0]));
  assert(model->tree[0].threshold == 5 && model->tree[0].missing_go_to_left);
  assert(predict(*model, model->data[0]) == 0);
  const auto missing_only = fit_root_cause_model({nan}, {numeric(2)}, options);
  assert(missing_only->tree[0].missing_only && missing_only->tree[0].threshold == 0);
  assert(predict(*missing_only, missing_only->data[0]) == 0);
  const auto missing_problem = fit_root_cause_model({numeric(1), numeric(1)}, {nan, nan}, options);
  assert(missing_problem->tree[0].missing_only);
  assert(!missing_problem->tree[0].missing_go_to_left);
  assert(predict(*missing_problem, missing_problem->data[0]) == 0);
  assert(predict(*missing_problem, missing_problem->data[2]) == 1);
}

void test_first_class_encoding() {
  const auto category = [](const RootCauseObservation& observation, const std::string& selector,
                           const std::string& expected) {
    const auto model = fit_root_cause_model({observation}, {RootCauseObservation{}}, {{selector}, {}});
    const auto name = "event:" + selector + "@" + expected;
    const auto found = std::find(model->feature_names.begin(), model->feature_names.end(), name);
    assert(found != model->feature_names.end());
    assert(model->data[0][static_cast<std::size_t>(found - model->feature_names.begin())] == 1);
  };
  RootCauseObservation projected;
  projected.context.activity = "A";
  projected.context.resource = "alice";
  projected.context.timestamp = "2026-01-01";
  projected.context.lifecycle = "complete";
  projected.context.activity_source_key = "step";
  projected.context.resource_source_key = "performer";
  projected.context.timestamp_source_key = "finished";
  projected.context.lifecycle_source_key = "phase";
  // Legacy projections without a typed cell retain non-empty values despite
  // their default Missing state, under canonical, short, and source aliases.
  for (const auto* key : {"concept:name", "activity", "step"}) category(projected, key, "A");
  for (const auto* key : {"org:resource", "resource", "performer"}) category(projected, key, "alice");
  for (const auto* key : {"time:timestamp", "timestamp", "finished"}) category(projected, key, "2026-01-01");
  for (const auto* key : {"lifecycle:transition", "lifecycle", "phase"}) category(projected, key, "complete");

  minera::EventAttribute cell;
  cell.key = "activity";
  cell.value = "stale";
  cell.first_class_role = minera::FirstClassAttributeRole::kActivity;
  cell.first_class_source_key = "step";
  projected.context.attributes = {cell};
  category(projected, "activity", "UNDEFINED");
  category(projected, "step", "UNDEFINED");
  projected.context.attributes[0].first_class_state = minera::FirstClassAttributeState::kNull;
  category(projected, "activity", "None");
  category(projected, "step", "None");
  projected.context.attributes[0].first_class_state = minera::FirstClassAttributeState::kPresent;
  category(projected, "step", "stale");
  projected.context.attributes.push_back({"step", "exact"});
  category(projected, "step", "exact");

  projected.context.attributes.clear();
  cell.first_class_state = minera::FirstClassAttributeState::kPresent;
  projected.context.activity_attribute = cell;
  // A stored cell cannot resurrect an explicitly missing projection.
  category(projected, "activity", "UNDEFINED");
  projected.context.activity_state = minera::FirstClassAttributeState::kNull;
  category(projected, "activity", "None");
  projected.context.activity_state = minera::FirstClassAttributeState::kPresent;
  category(projected, "activity", "stale");
  projected.context.activity_attribute->first_class_state = minera::FirstClassAttributeState::kMissing;
  category(projected, "activity", "UNDEFINED");

  auto missing_numeric = numeric(1);
  missing_numeric.context.attributes[0].first_class_role = minera::FirstClassAttributeRole::kActivity;
  rejects([&] { (void)fit_root_cause_model({missing_numeric}, {numeric(8)}, {{}, {"cost"}}); });
  missing_numeric.context.attributes[0].first_class_state = minera::FirstClassAttributeState::kNull;
  rejects([&] { (void)fit_root_cause_model({missing_numeric}, {numeric(8)}, {{}, {"cost"}}); });
}

void test_extended_categories() {
  const auto encode = [](minera::EventAttribute attribute) {
    RootCauseObservation observation;
    attribute.key = "group";
    observation.context.attributes.push_back(std::move(attribute));
    const auto model = fit_root_cause_model({observation}, {categorical("other")}, {{"group"}, {}});
    const auto column = std::find(model->data[0].begin(), model->data[0].end(), 1.0);
    assert(column != model->data[0].end());
    return model->feature_names[static_cast<std::size_t>(column - model->data[0].begin())].substr(12);
  };
  minera::EventAttribute parent{"group", "parent"};
  parent.children = {{"n", "002", minera::AttributeKind::kInt}, {"b", "true", minera::AttributeKind::kBoolean}};
  assert(encode(parent) == "{'value': 'parent', 'children': {'n': 2, 'b': True}}");
  parent.children.push_back({"n", "3", minera::AttributeKind::kInt});
  assert(encode(parent) == "{'value': 'parent', 'children': {'n': 3, 'b': True}}");
  parent.container = minera::AttributeContainerKind::kList;
  parent.children = {{"x", "hello"}, {"n", "2", minera::AttributeKind::kInt}, {"x", "again"}};
  assert(encode(parent) == "{'value': None, 'children': [('x', 'hello'), ('n', 2), ('x', 'again')]}");
  auto nested = parent;
  parent.container = minera::AttributeContainerKind::kContainer;
  parent.children = {nested};
  assert(encode(parent) == "{'value': None, 'children': {'group': {'value': None, 'children': [('x', 'hello'), ('n', 2), ('x', 'again')]}}}");
  parent.container = minera::AttributeContainerKind::kList;
  parent.children.clear();
  assert(encode(parent) == "{'value': None, 'children': []}");
  parent.container = minera::AttributeContainerKind::kContainer;
  assert(encode(parent) == "{'value': None, 'children': {}}");

  minera::EventAttribute quoted{"group", "a'b"};
  assert(encode(quoted) == "a'b");
  quoted.children = {{"key", "a'b"}, {"both", "a'\"b"}, {"control", "\t\n\r\\\b\x7f"}};
  assert(encode(quoted) == "{'value': \"a'b\", 'children': {'key': \"a'b\", 'both': 'a\\\'\"b', 'control': '\\t\\n\\r\\\\\\x08\\x7f'}}");
  quoted.value = "unicode";
  quoted.children = {{"print", "\xc3\xa9\xf0\x9f\x98\x80"}, {"hidden", "\xc2\xa0\xe2\x80\x8b\xf3\xa0\x80\x81"}};
  assert(encode(quoted) == "{'value': 'unicode', 'children': {'print': '\xc3\xa9\xf0\x9f\x98\x80', 'hidden': '\\xa0\\u200b\\U000e0001'}}");
  for (const auto& invalid : {std::string("\xc0\xaf"), std::string("\xed\xa0\x80"), std::string("\xf4\x90\x80\x80"), std::string("\xe2\x80")}) {
    quoted.children[0].value = invalid;
    rejects([&] { (void)encode(quoted); });
  }

  assert(encode({"group", "+002", minera::AttributeKind::kInt}) == "2");
  assert(encode({"group", " +000184467440737095516160000 ", minera::AttributeKind::kInt}) == "184467440737095516160000");
  assert(encode({"group", "-000184467440737095516160000", minera::AttributeKind::kInt}) == "-184467440737095516160000");
  assert(encode({"group", "-000", minera::AttributeKind::kInt}) == "0");
  for (const auto* invalid : {"+", "1.0", "1e2", "1 2", "0x10"})
    rejects([&] { (void)encode({"group", invalid, minera::AttributeKind::kInt}); });
  assert(encode({"group", "2e0", minera::AttributeKind::kFloat}) == "2.0");
  assert(encode({"group", "0", minera::AttributeKind::kBoolean}) == "False");
  for (const auto& [text, direct, nested_text] : std::vector<std::array<std::string, 3>>{
           {"2026-01-02T03:04:05.123456+02:30", "2026-01-02 03:04:05.123456+02:30", "datetime.datetime(2026, 1, 2, 3, 4, 5, 123456, tzinfo=datetime.timezone(datetime.timedelta(seconds=9000)))"},
           {"2026-01-02T03:04:05.123456Z", "2026-01-02 03:04:05.123456+00:00", "datetime.datetime(2026, 1, 2, 3, 4, 5, 123456, tzinfo=datetime.timezone.utc)"},
           {"2026-01-02T03:04:00.000000-02:30", "2026-01-02 03:04:00-02:30", "datetime.datetime(2026, 1, 2, 3, 4, tzinfo=datetime.timezone(datetime.timedelta(days=-1, seconds=77400)))"},
           {"2026-01-02T03:04:00.000001", "2026-01-02 03:04:00.000001", "datetime.datetime(2026, 1, 2, 3, 4, 0, 1)"},
           {"2026-01-02T03:04:00+15:00", "2026-01-02 03:04:00+15:00", "datetime.datetime(2026, 1, 2, 3, 4, tzinfo=datetime.timezone(datetime.timedelta(seconds=54000)))"},
           {"2026-01-02T03:04:00+01:02:03", "2026-01-02 03:04:00+01:02:03", "datetime.datetime(2026, 1, 2, 3, 4, tzinfo=datetime.timezone(datetime.timedelta(seconds=3723)))"},
           {"2026-01-02T03:04:00-01:02:03.000005", "2026-01-02 03:04:00-01:02:03.000005", "datetime.datetime(2026, 1, 2, 3, 4, tzinfo=datetime.timezone(datetime.timedelta(days=-1, seconds=82676, microseconds=999995)))"},
           {"2026-01-02T03:04:00+00:00:00.000005", "2026-01-02 03:04:00+00:00:00.000005", "datetime.datetime(2026, 1, 2, 3, 4, tzinfo=datetime.timezone(datetime.timedelta(microseconds=5)))"},
           {"2026-01-02T03:04:00+23:59:59.999999", "2026-01-02 03:04:00+23:59:59.999999", "datetime.datetime(2026, 1, 2, 3, 4, tzinfo=datetime.timezone(datetime.timedelta(seconds=86399, microseconds=999999)))"},
           {"2026-01-02T03:04:00-23:59:59.999999", "2026-01-02 03:04:00-23:59:59.999999", "datetime.datetime(2026, 1, 2, 3, 4, tzinfo=datetime.timezone(datetime.timedelta(days=-1, microseconds=1)))"},
           {"2026-01-02T03:04:00+000000.000000", "2026-01-02 03:04:00+00:00", "datetime.datetime(2026, 1, 2, 3, 4, tzinfo=datetime.timezone.utc)"},
           {"2026-01-02", "2026-01-02 00:00:00", "datetime.datetime(2026, 1, 2, 0, 0)"}}) {
    minera::EventAttribute date{"group", text, minera::AttributeKind::kDate};
    assert(encode(date) == direct);
    date.typed_value = minera::NativeDate{text};
    assert(encode(date) == direct);
    parent.children = {date};
    assert(encode(parent) == "{'value': None, 'children': {'group': " + nested_text + "}}");
  }
  rejects([&] { (void)encode({"group", "2026-02-30T00:00:00Z", minera::AttributeKind::kDate}); });
  for (const auto* offset : {"+24:00", "-24:00:00", "+01:60", "+01:02:60", "+01:02:03.",
                            "+1:00", "+01:", "+01:02:", "+01:0203", "Ztail", "+01:02:03.x"})
    rejects([&] { (void)encode({"group", std::string("2026-01-02T03:04:00") + offset, minera::AttributeKind::kDate}); });
  auto number = numeric(2);
  number.context.attributes[0].children = {{"nested", "value"}};
  rejects([&] { (void)fit_root_cause_model({number}, {numeric(8)}, {{}, {"cost"}}); });
  number.context.attributes[0].children.clear();
  number.context.attributes[0].container = minera::AttributeContainerKind::kList;
  rejects([&] { (void)fit_root_cause_model({number}, {numeric(8)}, {{}, {"cost"}}); });
}

void test_numeric_string_syntax() {
  const RootCauseOptions options{{}, {"cost"}};
  const auto from_text = [](const std::string& text) {
    RootCauseObservation observation;
    observation.context.attributes.push_back({"cost", text});
    return observation;
  };
  for (const auto& [text, expected] : std::vector<std::pair<std::string, double>>{
           {"1e-400", 0}, {"-1e-400", -0.0}, {"1_000", 1000},
           {" \t+2.5\r\n", 2.5}, {".25", 0.25}, {"2.", 2},
           {"1_2.5_0e+0_1", 125}, {"1e-310", 1e-310}}) {
    const auto model = fit_root_cause_model({from_text(text)}, {numeric(8)}, options);
    assert(model->data[0][0] == expected);
    assert(std::signbit(model->data[0][0]) == std::signbit(expected));
  }
  for (const auto& [text, expected] : std::vector<std::pair<std::string, double>>{
           {"\xef\xbc\x91\xef\xbc\x92.\xef\xbc\x95", 12.5},
           {"\xc2\xa0\xd9\xa2.\xd9\xa5\xc2\xa0", 2.5},
           {"\xd9\xa1_\xd9\xa2.\xd9\xa3" "e\xd9\xa2", 1230}})
    assert(fit_root_cause_model({from_text(text)}, {numeric(8)}, options)->data[0][0] == expected);
  for (const auto& invalid : {std::string("\xe2\x80\x8b") + "1", std::string("\x1c") + "\xd9\xa1", std::string("\xc0\xaf")})
    rejects([&] { (void)fit_root_cause_model({from_text(invalid)}, {numeric(8)}, options); });
  for (const auto* text : {"nan", "+NaN", "-NAN"})
    assert(std::isnan(fit_root_cause_model({from_text(text)}, {numeric(8)}, options)->data[0][0]));
  for (const auto* text : {"0x1p2", "0x10", "_1", "1_", "1__0", "1_.0", "1._0",
                           "1e_2", "1e+_2", "1e", "+", ".", "1 2", "nan(payload)",
                           "inf", "-Infinity", "1e400", "1e39"})
    rejects([&] { (void)fit_root_cause_model({from_text(text)}, {numeric(8)}, options); });
}

void test_depth_and_interactions() {
  const RootCauseOptions options{{}, {"cost"}};
  std::vector<RootCauseObservation> fit, bad;
  for (int i = 0; i < 40; ++i) (i % 2 ? bad : fit).push_back(numeric(i));
  const auto bounded = fit_root_cause_model(fit, bad, options);
  assert(depth(*bounded) == 7);
  const auto point = [](int x, int y) {
    auto result = numeric(x);
    result.context.attributes.push_back({"y", std::to_string(y)});
    return result;
  };
  const auto interaction = fit_root_cause_model({point(0, 0), point(1, 1)},
                                               {point(0, 1), point(1, 0)}, {{}, {"cost", "y"}});
  assert(depth(*interaction) == 2);
  for (std::size_t i = 0; i < interaction->data.size(); ++i)
    assert(predict(*interaction, interaction->data[i]) == interaction->target[i]);
}
}  // namespace

int main() {
  test_numeric_tree();
  test_encoding();
  test_balancing();
  test_missing_and_errors();
  test_numeric_string_syntax();
  test_first_class_encoding();
  test_extended_categories();
  test_depth_and_interactions();
  std::cout << "Root cause tests passed\n";
}
