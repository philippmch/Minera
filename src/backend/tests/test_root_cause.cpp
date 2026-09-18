#include "minera/root_cause.hpp"
#include "../root_cause_trace_key.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

static_assert(std::variant_size_v<minera::NativeAttributeValue> == 6);
static_assert(std::variant_size_v<const minera::NativeAttributeValue> == 6);
static_assert(std::is_same_v<std::variant_alternative_t<0, minera::NativeAttributeValue>, std::monostate>);
static_assert(std::is_same_v<std::variant_alternative_t<2, minera::NativeAttributeValue>, double>);
static_assert(std::is_same_v<std::variant_alternative_t<4, minera::NativeAttributeValue>, minera::NativeDate>);
static_assert(std::is_same_v<std::variant_alternative_t<5, minera::NativeAttributeValue>, std::uint64_t>);
static_assert(std::is_same_v<std::variant_alternative_t<2, const minera::NativeAttributeValue>, const double>);

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
  // Either one-hot column yields the same optimal Gini split.
  assert(model->tree[0].feature_index == 0 || model->tree[0].feature_index == 1);
  assert(model->tree[0].threshold == 0.5);
  for (const int child : {model->tree[0].left, model->tree[0].right})
    assert(model->tree[child].class_counts[0] == 0 || model->tree[child].class_counts[1] == 0);
  for (std::size_t row = 0; row < model->data.size(); ++row)
    assert(predict(*model, model->data[row]) == model->target[row]);

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

void test_categorical_feature_collisions() {
  const auto observation = [](std::initializer_list<minera::EventAttribute> attributes) {
    RootCauseObservation result;
    result.context.attributes = attributes;
    return result;
  };
  const auto check = [](const std::optional<minera::RootCauseModel>& model,
                        const std::vector<std::string>& names,
                        const std::vector<std::vector<double>>& data,
                        const std::vector<int>& target) {
    assert(model);
    assert(model->feature_names == names);
    assert(model->data == data);
    assert(model->target == target);
    assert((model->classes == std::vector<std::string>{"fit", "underfed"}));
  };
  check(fit_root_cause_model({observation({{"g", "a"}})}, {observation({{"g", "b"}})}, {{"g", "g"}, {}}),
        {"event:g@a", "event:g@b", "event:g@a", "event:g@b"},
        {{0, 0, 1, 0}, {0, 0, 0, 1}}, {0, 1});
  check(fit_root_cause_model({observation({{"g", "x@y"}, {"g@x", "y"}})},
                            {observation({{"g", "z"}, {"g@x", "z"}})}, {{"g", "g@x"}, {}}),
        {"event:g@x@y", "event:g@z", "event:g@x@y", "event:g@x@z"},
        {{0, 0, 1, 0}, {0, 1, 0, 1}}, {0, 1});
  check(fit_root_cause_model({observation({{"g", "x"}, {"g@x", "2"}})},
                            {observation({{"g", "y"}, {"g@x", "8"}})}, {{"g"}, {"g@x"}}),
        {"event:g@x", "event:g@y", "event:g@x"},
        {{0, 0, 2}, {0, 1, 8}}, {0, 1});
  minera::EventAttribute null{"g", "ignored"};
  null.is_null = true;
  check(fit_root_cause_model({observation({}), observation({null})},
                            {observation({{"g", "UNDEFINED"}}), observation({{"g", "None"}})}, {{"g"}, {}}),
        {"event:g@None", "event:g@UNDEFINED"},
        {{0, 1}, {1, 0}, {0, 1}, {1, 0}}, {0, 0, 1, 1});
}

void test_public_root_cause_helpers() {
  using minera::build_root_cause_log;
  using minera::build_root_cause_representation;
  const std::vector<RootCauseObservation> fit{numeric(1, 7), numeric(2, 9)};
  const std::vector<RootCauseObservation> problem{numeric(7, 11), numeric(8, 13), numeric(9, 15), numeric(10, 17), numeric(11, 19)};
  RootCauseOptions options{{}, {"cost"}, true};
  const auto representation = build_root_cause_representation(fit, problem, options);
  assert((representation.feature_names == std::vector<std::string>{"event:cost"}));
  assert((representation.data == std::vector<std::vector<double>>{{1}, {2}, {1}, {2}, {7}, {8}, {9}, {10}, {11}}));
  assert((representation.target == std::vector<int>{0, 0, 0, 0, 1, 1, 1, 1, 1}));
  assert((representation.source_trace_indices == std::vector<std::size_t>{7, 9, 7, 9, 11, 13, 15, 17, 19}));
  const auto trained = fit_root_cause_model(fit, problem, options);
  assert(trained->feature_names == representation.feature_names && trained->data == representation.data);
  assert(trained->target == representation.target && trained->source_trace_indices == representation.source_trace_indices);
  auto detached = build_root_cause_log(fit, problem, true);
  assert(detached.traces.size() == 9);
  for (std::size_t row = 0; row < detached.traces.size(); ++row) {
    const auto& trace = detached.traces[row];
    assert(trace.events.size() == 1 && trace.case_id.empty() && trace.attributes.empty() && trace.properties.empty());
    assert(std::get<double>(trace.events[0].attributes[0].typed_value) == representation.data[row][0]);
  }
  detached.traces[0].events[0].attributes[0].typed_value = 99.0;
  assert(std::get<double>(fit[0].context.attributes[0].typed_value) == 1);
  assert(std::get<double>(detached.traces[2].events[0].attributes[0].typed_value) == 1);
  assert(build_root_cause_log({}, {}).traces.empty());
  assert(build_root_cause_log({}, fit).traces.size() == 2);
  assert(build_root_cause_log(fit, {}).traces.size() == 2);
  const auto no_features = build_root_cause_representation(fit, {}, {});
  assert(no_features.feature_names.empty());
  assert((no_features.data == std::vector<std::vector<double>>{{}, {}}));
  assert((no_features.target == std::vector<int>{0, 0}));
  assert((no_features.source_trace_indices == std::vector<std::size_t>{7, 9}));
  const auto only_problem = build_root_cause_representation({}, fit, {{}, {"cost"}});
  assert((only_problem.data == std::vector<std::vector<double>>{{1}, {2}}));
  assert((only_problem.target == std::vector<int>{1, 1}));
  const auto empty = build_root_cause_representation({}, {}, {{"group"}, {"cost", "cost"}});
  assert((empty.feature_names == std::vector<std::string>{"event:cost", "event:cost"}));
  assert(empty.data.empty() && empty.target.empty() && empty.source_trace_indices.empty());
  const auto collision = build_root_cause_representation({categorical("a")}, {categorical("b")}, {{"group", "group"}, {}});
  assert((collision.feature_names == std::vector<std::string>{"event:group@a", "event:group@b", "event:group@a", "event:group@b"}));
  assert((collision.data == std::vector<std::vector<double>>{{0, 0, 1, 0}, {0, 0, 0, 1}}));
  const auto unbounded = build_root_cause_representation(
      {numeric(std::numeric_limits<double>::infinity()), numeric(1e39), numeric(std::numeric_limits<double>::quiet_NaN())}, {}, {{}, {"cost"}});
  assert(std::isinf(unbounded.data[0][0]) && unbounded.data[1][0] == 1e39 && std::isnan(unbounded.data[2][0]));
  auto null = numeric(1);
  null.context.attributes[0].is_null = true;
  rejects([&] { (void)build_root_cause_representation({null}, {}, {{}, {"cost"}}); });
  for (const bool both_empty : {false, true}) {
    bool rejected_log = false, rejected_representation = false;
    const std::vector<RootCauseObservation> cohort = both_empty ? std::vector<RootCauseObservation>{} : fit;
    try { (void)build_root_cause_log({}, cohort, true); }
    catch (const std::domain_error&) { rejected_log = true; }
    try { (void)build_root_cause_representation({}, cohort, options); }
    catch (const std::domain_error&) { rejected_representation = true; }
    assert(rejected_log && rejected_representation);
    assert(!fit_root_cause_model({}, cohort, options));
  }
}

void test_equivalent_feature_choices() {
  std::array<bool, 2> seen{};
  for (int trial = 0; trial < 256; ++trial) {
    const auto model = fit_root_cause_model({categorical("a"), categorical("a")},
                                          {categorical("b"), categorical("b")}, {{"group"}, {}});
    assert((model->data == std::vector<std::vector<double>>{{1, 0}, {1, 0}, {0, 1}, {0, 1}}));
    assert((model->target == std::vector<int>{0, 0, 1, 1}));
    const auto& root = model->tree.front();
    assert(root.feature_index == 0 || root.feature_index == 1);
    seen[static_cast<std::size_t>(root.feature_index)] = true;
    assert(root.threshold == 0.5 && model->tree.size() == 3);
    for (const int child : {root.left, root.right})
      assert(model->tree[child].class_counts[0] == 0 || model->tree[child].class_counts[1] == 0);
    for (std::size_t row = 0; row < model->data.size(); ++row)
      assert(predict(*model, model->data[row]) == model->target[row]);
  }
  // Independent entropy-seeded fits exercise both equally optimal columns;
  // the chance of 256 fair choices all taking one column is 2^-255.
  assert(seen[0] && seen[1]);
}

void test_missing_route_order() {
  const RootCauseOptions options{{}, {"cost"}};
  const auto nan = numeric(std::numeric_limits<double>::quiet_NaN());
  const auto ordinary = fit_root_cause_model({numeric(0), numeric(3), nan},
                                            {numeric(1), numeric(2)}, options);
  assert(ordinary->tree[0].threshold == 2.5 && !ordinary->tree[0].missing_go_to_left);
  const auto missing = fit_root_cause_model({numeric(0), numeric(2), numeric(3)},
                                           {numeric(1), nan, nan}, options);
  assert(missing->tree[0].missing_only && !missing->tree[0].missing_go_to_left);

  // Exhaust every nonconstant labeling. Compute candidate partitions afresh,
  // independently of the trainer's incremental sorted-count scan.
  for (unsigned mask = 1; mask < 63; ++mask) {
    std::vector<RootCauseObservation> fit, problem;
    for (unsigned row = 0; row < 6; ++row)
      (mask & (1U << row) ? problem : fit).push_back(row < 4 ? numeric(row) : nan);
    double best_score = -1, expected_threshold = 0;
    bool expected_missing_left = false;
    for (const bool missing_left : {false, true}) {
      for (const double threshold : {0.5, 1.5, 2.5, std::numeric_limits<double>::infinity()}) {
        if (missing_left && std::isinf(threshold)) continue;
        std::array<std::array<double, 2>, 2> counts{};
        for (unsigned row = 0; row < 6; ++row) {
          const bool left = row < 4 ? row <= threshold : missing_left;
          ++counts[left ? 0 : 1][mask & (1U << row) ? 1 : 0];
        }
        if (counts[0][0] + counts[0][1] == 0 || counts[1][0] + counts[1][1] == 0) continue;
        double score = 0;
        for (const auto& side : counts) score += (side[0] * side[0] + side[1] * side[1]) / (side[0] + side[1]);
        if (score > best_score) {
          best_score = score;
          expected_threshold = threshold;
          expected_missing_left = missing_left;
        }
      }
    }
    const auto model = fit_root_cause_model(fit, problem, options);
    const auto& root = model->tree.front();
    assert(root.feature_index == 0);
    assert(root.missing_go_to_left == expected_missing_left);
    assert(root.missing_only == std::isinf(expected_threshold));
    assert(root.threshold == (root.missing_only ? 0 : expected_threshold));
  }
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

  RootCauseObservation metadata;
  metadata.context.resource_state = minera::FirstClassAttributeState::kPresent;
  metadata.context.resource_attribute = minera::EventAttribute{"stored_worker", "alice"};
  metadata.context.resource_attribute->first_class_source_key = "source_worker";
  category(metadata, "stored_worker", "alice");
  category(metadata, "source_worker", "alice");
  metadata.context.resource_attribute->value = "2.5";
  for (const auto* key : {"stored_worker", "source_worker"}) {
    const auto model = fit_root_cause_model({metadata}, {metadata}, {{}, {key}});
    assert(model->data[0][0] == 2.5);
  }
  metadata.context.resource_state = minera::FirstClassAttributeState::kMissing;
  category(metadata, "stored_worker", "UNDEFINED");
  metadata.context.resource_state = minera::FirstClassAttributeState::kNull;
  category(metadata, "source_worker", "None");

  RootCauseObservation ambiguous;
  minera::EventAttribute first{"first", "1"}, second{"second", "2"};
  first.first_class_source_key = second.first_class_source_key = "shared";
  ambiguous.context.attributes = {first, second};
  for (const bool numeric_selector : {false, true}) {
    const RootCauseOptions selected = numeric_selector ? RootCauseOptions{{}, {"shared"}}
                                                       : RootCauseOptions{{"shared"}, {}};
    rejects([&] { (void)fit_root_cause_model({ambiguous}, {ambiguous}, selected); });
  }
  // A unique exact key supersedes every provenance match, in either order.
  ambiguous.context.attributes.push_back({"shared", "3"});
  for (int ordering = 0; ordering < 2; ++ordering) {
    category(ambiguous, "shared", "3");
    assert(fit_root_cause_model({ambiguous}, {ambiguous}, {{}, {"shared"}})->data[0][0] == 3);
    std::reverse(ambiguous.context.attributes.begin(), ambiguous.context.attributes.end());
  }
  ambiguous.context.attributes.push_back({"shared", "4"});
  rejects([&] { (void)fit_root_cause_model({ambiguous}, {ambiguous}, {{"shared"}, {}}); });
  rejects([&] { (void)fit_root_cause_model({ambiguous}, {ambiguous}, {{}, {"shared"}}); });
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

void test_native_scalar_identity() {
  using minera::NativeAttributeValue;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  NativeAttributeValue first = nan;
  const auto first_id = first.scalar_identity();
  NativeAttributeValue copied = first;
  assert(copied.scalar_identity() == first_id);
  NativeAttributeValue moved = std::move(copied);
  assert(moved.scalar_identity() == first_id);
  NativeAttributeValue assigned;
  assigned = moved;
  assert(assigned.scalar_identity() == first_id);
  NativeAttributeValue move_assigned;
  move_assigned = std::move(assigned);
  assert(move_assigned.scalar_identity() == first_id);
  NativeAttributeValue independent = nan;
  assert(independent.scalar_identity() != first_id);
  first = nan;
  assert(first.scalar_identity() != first_id);
  const auto reassigned_id = first.scalar_identity();
  first.emplace<double>(nan);
  assert(first.scalar_identity() != reassigned_id);
  const auto emplaced_id = first.scalar_identity();
  first.emplace<2>(nan);
  assert(first.scalar_identity() != emplaced_id);
  const auto swapped_id = first.scalar_identity();
  first.swap(moved);
  assert(first.scalar_identity() == first_id && moved.scalar_identity() == swapped_id);
  using std::swap;
  swap(first, moved);
  assert(first.scalar_identity() == swapped_id && moved.scalar_identity() == first_id);
  std::swap(first, moved);
  assert(first.scalar_identity() == first_id && moved.scalar_identity() == swapped_id);
  assert(std::holds_alternative<double>(first));
  assert(std::isnan(std::get<double>(first)) && std::isnan(std::get<2>(first)));
  assert(std::get_if<double>(&first) != nullptr);
  assert(std::visit([](const auto&) { return true; }, first));
  NativeAttributeValue integer = std::int64_t{7};
  NativeAttributeValue other_integer = std::int64_t{7};
  assert(integer.scalar_identity() != other_integer.scalar_identity() && integer == other_integer);
  NativeAttributeValue unsigned_integer = std::numeric_limits<std::uint64_t>::max();
  assert(std::holds_alternative<std::uint64_t>(unsigned_integer));
  assert(std::get<5>(unsigned_integer) == std::numeric_limits<std::uint64_t>::max());
  assert(unsigned_integer.scalar_identity() != integer.scalar_identity());
  NativeAttributeValue boolean(std::in_place_type<bool>, true);
  assert(std::get<bool>(boolean));
  boolean = minera::NativeDate{"2026-01-01T00:00:00Z"};
  assert(std::get<minera::NativeDate>(boolean).value == "2026-01-01T00:00:00Z");
  boolean = std::monostate{};
  assert(std::holds_alternative<std::monostate>(boolean));

  minera::EventAttribute exact{"value", "", minera::AttributeKind::kInt};
  exact.typed_value = std::numeric_limits<std::uint64_t>::max();
  assert(minera::root_cause_attribute_category(exact) ==
         "18446744073709551615");
  assert(minera::native_scalar_key_compare(
             exact, minera::EventAttribute{
                        "value", "18446744073709551614",
                        minera::AttributeKind::kInt}) > 0);
}

void test_trace_equivalence() {
  const auto compare = [](const minera::Trace& left, const minera::Trace& right, bool expected) {
    assert(minera::root_cause_traces_equal(left, right) == expected);
    assert(minera::root_cause_traces_equal(right, left) == expected);
    if (expected) assert(minera::root_cause_trace_fingerprint(left) == minera::root_cause_trace_fingerprint(right));
  };
  const auto event_trace = [](minera::EventAttribute value) {
    minera::Trace trace;
    trace.case_id = "case-one";
    minera::Event event;
    event.activity = "A";
    value.key = "value";
    event.attributes.push_back(std::move(value));
    trace.events.push_back(std::move(event));
    return trace;
  };
  const auto attr_trace = [](minera::EventAttribute value) {
    minera::Trace trace;
    value.key = "value";
    trace.attributes.push_back(std::move(value));
    return trace;
  };
  minera::EventAttribute integer{"value", "1", minera::AttributeKind::kInt};
  minera::EventAttribute real{"value", "1.0", minera::AttributeKind::kFloat};
  minera::EventAttribute boolean{"value", "true", minera::AttributeKind::kBoolean};
  minera::EventAttribute text{"value", "1"};
  compare(event_trace(integer), event_trace(text), true);
  compare(event_trace(integer), event_trace(real), false);
  compare(event_trace(integer), event_trace(boolean), false);
  compare(attr_trace(integer), attr_trace(real), true);
  compare(attr_trace(integer), attr_trace(boolean), true);
  compare(attr_trace(integer), attr_trace(text), false);
  minera::EventAttribute null{"value", "ignored"};
  null.is_null = true;
  compare(event_trace(null), event_trace({"value", "None"}), true);
  compare(attr_trace(null), attr_trace({"value", "None"}), false);
  compare(event_trace({"value", "NaN", minera::AttributeKind::kFloat}), event_trace({"value", "nan", minera::AttributeKind::kFloat}), true);
  compare(attr_trace({"value", "NaN", minera::AttributeKind::kFloat}), attr_trace({"value", "nan", minera::AttributeKind::kFloat}), false);
  auto nan_trace = attr_trace({"value", "NaN", minera::AttributeKind::kFloat});
  nan_trace.attributes[0].typed_value = std::numeric_limits<double>::quiet_NaN();
  auto aliased_nan_trace = nan_trace;
  compare(nan_trace, aliased_nan_trace, true);
  aliased_nan_trace.attributes[0].typed_value = std::numeric_limits<double>::quiet_NaN();
  compare(nan_trace, aliased_nan_trace, false);
  assert(minera::root_cause_trace_fingerprint(nan_trace) != minera::root_cause_trace_fingerprint(aliased_nan_trace));
  minera::EventAttribute nested_nan{"value", "parent"};
  nested_nan.children = nan_trace.attributes;
  auto nested_nan_trace = attr_trace(nested_nan);
  auto nested_alias = nested_nan_trace;
  compare(nested_nan_trace, nested_alias, true);
  nested_alias.attributes[0].children[0].typed_value.emplace<double>(std::numeric_limits<double>::quiet_NaN());
  compare(nested_nan_trace, nested_alias, false);
  auto event_nan = event_trace(nan_trace.attributes[0]);
  auto independent_event_nan = event_nan;
  independent_event_nan.events[0].attributes[0].typed_value = std::numeric_limits<double>::quiet_NaN();
  compare(event_nan, independent_event_nan, true);
  compare(event_trace({"value", "0.0", minera::AttributeKind::kFloat}), event_trace({"value", "-0.0", minera::AttributeKind::kFloat}), false);
  compare(attr_trace({"value", "0.0", minera::AttributeKind::kFloat}), attr_trace({"value", "-0.0", minera::AttributeKind::kFloat}), true);
  compare(attr_trace({"value", "1267650600228229401496703205376", minera::AttributeKind::kInt}), attr_trace({"value", "1.2676506002282294e30", minera::AttributeKind::kFloat}), true);
  compare(attr_trace({"value", "1267650600228229401496703205377", minera::AttributeKind::kInt}), attr_trace({"value", "1.2676506002282294e30", minera::AttributeKind::kFloat}), false);

  auto a = event_trace(text), b = a;
  a.events[0].attributes.push_back({"other", "x"});
  b = a;
  std::reverse(b.events[0].attributes.begin(), b.events[0].attributes.end());
  b.events[0].case_id = "ignored event projection";
  b.events[0].input_order = 99;
  b.properties.push_back({"ignored", "property"});
  compare(a, b, true);
  b.case_id = "case-two";
  compare(a, b, false);
  b = a;
  b.attributes.push_back({"extra", "different"});
  compare(a, b, false);
  b = a;
  b.events.push_back(b.events[0]);
  compare(a, b, false);
  a.events.push_back(a.events[0]);
  a.events[1].activity = "B";
  b = a;
  std::reverse(b.events.begin(), b.events.end());
  compare(a, b, false);

  minera::EventAttribute nested{"value", "parent"};
  nested.children = {{"one", "1", minera::AttributeKind::kInt}, {"two", "2", minera::AttributeKind::kInt}};
  auto reordered = nested;
  std::reverse(reordered.children.begin(), reordered.children.end());
  compare(event_trace(nested), event_trace(reordered), false);
  compare(attr_trace(nested), attr_trace(reordered), true);
  nested.container = reordered.container = minera::AttributeContainerKind::kList;
  compare(attr_trace(nested), attr_trace(reordered), false);
  nested.children.push_back({"one", "3", minera::AttributeKind::kInt});
  reordered = nested;
  reordered.children[0].value = "3";
  reordered.children.pop_back();
  nested.container = reordered.container = minera::AttributeContainerKind::kContainer;
  compare(attr_trace(nested), attr_trace(reordered), true);

  const auto date = [](const char* value) { return minera::EventAttribute{"value", value, minera::AttributeKind::kDate}; };
  compare(attr_trace(date("2026-01-02T03:04:05.123456+02:30")), attr_trace(date("2026-01-02T00:34:05.123456Z")), true);
  compare(attr_trace(date("2026-01-02T00:34:05.123456")), attr_trace(date("2026-01-02T00:34:05.123456Z")), false);
  compare(attr_trace(date("9999-01-02T00:34:05.123456Z")), attr_trace(date("9999-01-02T00:34:05.123457Z")), false);

  a = event_trace(text);
  b = a;
  b.events[0].activity_state = minera::FirstClassAttributeState::kPresent;
  b.events[0].activity_source_key = "concept:name";
  b.events[0].activity_attribute = minera::EventAttribute{"concept:name", "A"};
  compare(a, b, true);
  b.events[0].attributes.push_back({"concept:name", "B"});
  a.events[0].activity = "B";
  compare(a, b, true);
  b.events[0].attributes.back().first_class_role = minera::FirstClassAttributeRole::kActivity;
  a.events[0].activity.clear();
  compare(a, b, true);
  b.events[0].attributes.back().first_class_state = minera::FirstClassAttributeState::kNull;
  a.events[0].activity = "None";
  compare(a, b, true);
}

void test_shared_context_log() {
  using namespace minera;
  Event input;
  EventAttribute nested{"v", "parent"};
  nested.children = {{"n", "1", AttributeKind::kInt}};
  input.attributes = {nested, {"cost", "1"}};
  auto source = make_shared_root_cause_context(input);
  std::vector<RootCauseSharedObservation> fit{{3, source}};
  auto problem = make_shared_root_cause_context(numeric(8).context);
  std::vector<RootCauseSharedObservation> bad{{7, problem}, {8, problem}};
  auto log = build_shared_root_cause_log(fit, bad, true);
  assert(log.contexts.size() == 4);
  auto object = log.contexts[0].attributes[0].value;
  object->value = "changed";
  object->children[0].value->value = "2";
  assert(source.attributes[0].value->value == "changed");
  assert(fit[0].context.attributes[0].value->children[0].value->value == "2");
  assert(log.contexts[1].attributes[0].value == object);
  // Creating the reference graph is a snapshot of the value-owned Event.
  assert(input.attributes[0].value == "parent");
  auto replacement = make_shared_root_cause_context(Event{});
  replacement.attributes.push_back({"v", std::make_shared<RootCauseSharedValue>()});
  replacement.attributes[0].value->value = "replacement";
  log.contexts[0].attributes[0] = replacement.attributes[0];
  assert(log.contexts[0].attributes[0].value->value == "replacement");
  assert(log.contexts[1].attributes[0].value->value == "changed");
  log.contexts[0].attributes.erase(log.contexts[0].attributes.begin() + 1);
  assert(source.attributes.size() == 2 && log.contexts[1].attributes.size() == 2);
  const auto snapshot = materialize_root_cause_log(log);
  assert(snapshot.traces.size() == 4 && snapshot.traces[0].attributes.empty());
  assert(snapshot.traces[1].events[0].attributes[0].children[0].value == "2");
  object->children[0].value->value = "3";
  assert(snapshot.traces[1].events[0].attributes[0].children[0].value == "2");
  auto matrix = build_shared_root_cause_representation(fit, bad, {{"v"}, {}});
  assert((matrix.source_trace_indices == std::vector<std::size_t>{3, 7, 8}));
  assert(std::find(matrix.feature_names.begin(), matrix.feature_names.end(),
      "event:v@{'value': 'changed', 'children': {'n': 3}}") != matrix.feature_names.end());
  auto owner = make_shared_root_cause_context(input);
  auto surviving = build_shared_root_cause_log({{0, owner}}, {});
  owner = {};
  assert(materialize_root_cause_log(surviving).traces[0].events[0].attributes[0].value == "parent");
  assert(build_shared_root_cause_log({}, {}).contexts.empty());
  bool rejected = false;
  try { (void)build_shared_root_cause_log(fit, {}, true); }
  catch (const std::domain_error&) { rejected = true; }
  assert(rejected);
  Event projected;
  projected.resource = "stale";
  projected.resource_state = FirstClassAttributeState::kPresent;
  projected.resource_attribute = nested;
  projected.resource_attribute->key = "org:resource";
  auto shared_projection = make_shared_root_cause_context(projected);
  auto projection_log = build_shared_root_cause_log({{0, shared_projection}}, {});
  projection_log.contexts[0].resource_attribute->value->value = "updated";
  assert(materialize_root_cause_context(shared_projection).resource_attribute->value == "updated");
  auto cyclic = make_shared_root_cause_context(input);
  auto shared_child = cyclic.attributes[0].value->children[0];
  shared_child.key = "another";
  cyclic.attributes[0].value->children.push_back(shared_child);
  auto dag = materialize_root_cause_context(cyclic);
  assert(dag.attributes[0].children.size() == 2);
  assert(dag.attributes[0].children[0].value == dag.attributes[0].children[1].value);
  cyclic.attributes[0].value->children.push_back(cyclic.attributes[0]);
  rejects([&] { (void)materialize_root_cause_context(cyclic); });
  cyclic.attributes[0].value->children.pop_back();
  cyclic.attributes[0].value.reset();
  rejects([&] { (void)materialize_root_cause_context(cyclic); });
}

void test_tuple_values() {
  using namespace minera;
  const auto tuple = [](std::vector<EventAttribute> children) {
    EventAttribute result{"v", ""};
    result.container = AttributeContainerKind::kTuple;
    result.children = std::move(children);
    return result;
  };
  const auto one = EventAttribute{"ignored", "1", AttributeKind::kInt};
  assert(root_cause_attribute_category(tuple({})) == "()");
  assert(root_cause_attribute_category(tuple({one})) == "(1,)");
  EventAttribute null{"", "", AttributeKind::kString, true};
  auto nested = tuple({null, tuple({one, {"", "2", AttributeKind::kInt}})});
  assert(root_cause_attribute_category(nested) == "(None, (1, 2))");
  auto a = tuple({one, {"", "x"}});
  auto b = tuple({{"different-key", "true", AttributeKind::kBoolean}, {"other", "x"}});
  assert(root_cause_scalar_keys_equal(a, b));
  assert(root_cause_scalar_key_fingerprint(a) == root_cause_scalar_key_fingerprint(b));
  b.children[0] = {"", "1"};
  assert(!root_cause_scalar_keys_equal(a, b));
  Trace left, right;
  left.attributes = {a};
  right.attributes = {a};
  right.attributes[0].children[0].key = "position-only";
  assert(root_cause_traces_equal(left, right));
  assert(root_cause_trace_fingerprint(left) == root_cause_trace_fingerprint(right));
  right.attributes[0].container = AttributeContainerKind::kList;
  assert(!root_cause_traces_equal(left, right));
  auto nan = EventAttribute{"", "nan", AttributeKind::kFloat};
  nan.typed_value = std::numeric_limits<double>::quiet_NaN();
  a = tuple({nan}); b = a;
  assert(root_cause_scalar_keys_equal(a, b));
  b.children[0].typed_value = std::numeric_limits<double>::quiet_NaN();
  assert(!root_cause_scalar_keys_equal(a, b));
  a.children[0].container = AttributeContainerKind::kList;
  assert(!root_cause_key_is_hashable(a));
  rejects([&] { (void)root_cause_scalar_key_fingerprint(a); });
  a.is_null = true;
  assert(root_cause_scalar_keys_equal(a, null));
  assert(root_cause_scalar_key_fingerprint(a) == root_cause_scalar_key_fingerprint(null));
  assert(root_cause_attribute_category(a) == "None");
  RootCauseObservation observation;
  observation.context.attributes = {nested};
  const auto representation = build_root_cause_representation({observation}, {}, {{"v"}, {}});
  assert((representation.feature_names == std::vector<std::string>{"event:v@(None, (1, 2))"}));
  rejects([&] { (void)build_root_cause_representation({observation}, {}, {{}, {"v"}}); });
}

void test_shared_cyclic_encoding() {
  using namespace minera;
  Event event;
  EventAttribute parent{"v", "parent"};
  parent.children = {{"n", "1", AttributeKind::kInt}};
  event.attributes = {parent, {"cost", "1"}};
  auto context = make_shared_root_cause_context(event);
  auto node = context.attributes[0].value;
  node->children = {{"self", node}};
  std::vector<RootCauseSharedObservation> rows{{4, context}};
  const auto encode = [&](const RootCauseOptions& options) {
    return build_shared_root_cause_representation(rows, {}, options);
  };
  auto matrix = encode({{"v"}, {}});
  assert((matrix.feature_names == std::vector<std::string>{"event:v@{'value': 'parent', 'children': {'self': {...}}}"}));
  assert((matrix.data == std::vector<std::vector<double>>{{1}}));
  assert((encode({{}, {"cost"}}).data == std::vector<std::vector<double>>{{1}}));
  assert((encode({{}, {}}).data == std::vector<std::vector<double>>{{}}));
  rejects([&] { (void)encode({{}, {"v"}}); });
  node->first_class_role = FirstClassAttributeRole::kResource;
  node->first_class_state = FirstClassAttributeState::kNull;
  matrix = encode({{"v"}, {}});
  assert((matrix.feature_names == std::vector<std::string>{"event:v@{'value': None, 'children': {'self': {...}}}"}));
  node->first_class_role = FirstClassAttributeRole::kNone;
  node->first_class_state = FirstClassAttributeState::kMissing;
  node->container = AttributeContainerKind::kList;
  matrix = encode({{"v"}, {}});
  assert((matrix.feature_names == std::vector<std::string>{"event:v@{'value': None, 'children': [('self', {...})]}"}));
  node->children.clear();
  node->container = AttributeContainerKind::kScalar;
  auto tuple = std::make_shared<RootCauseSharedValue>();
  tuple->container = AttributeContainerKind::kTuple;
  tuple->children = {{"", node}};
  node->children = {{"tuple", tuple}};
  rows[0].context.attributes[0].value = tuple;
  matrix = encode({{"v"}, {}});
  assert((matrix.feature_names == std::vector<std::string>{"event:v@({'value': 'parent', 'children': {'tuple': (...)}},)"}));
  node->children.clear();
  tuple->children.clear();
  auto plain = make_shared_root_cause_context(event);
  plain.attributes.push_back({"unused", nullptr});
  const auto safe = build_shared_root_cause_representation({{4, plain}}, {}, {{"v"}, {"cost"}});
  const auto expected = build_root_cause_representation({{4, event}}, {}, {{"v"}, {"cost"}});
  assert(safe.feature_names == expected.feature_names && safe.data == expected.data);
  assert(safe.source_trace_indices == expected.source_trace_indices);
  rejects([&] { (void)build_shared_root_cause_representation({{4, plain}}, {}, {{"unused"}, {}}); });
  auto aliased = plain.attributes[0];
  aliased.key = "alias";
  aliased.value = std::make_shared<RootCauseSharedValue>(*aliased.value);
  aliased.value->first_class_source_key = "v";
  aliased.value->value = "lower priority";
  plain.attributes.push_back(aliased);
  const auto priority = build_shared_root_cause_representation({{4, plain}}, {}, {{"v"}, {"cost"}});
  assert(priority.feature_names == safe.feature_names && priority.data == safe.data);
  plain.attributes.push_back(plain.attributes[0]);
  rejects([&] { (void)build_shared_root_cause_representation({{4, plain}}, {}, {{"v"}, {}}); });
}

void test_integer_float_overflow() {
  using namespace minera;
  RootCauseObservation huge;
  huge.context.attributes = {{"v", "1" + std::string(400, '0'), AttributeKind::kInt}};
  bool overflow = false;
  try { (void)build_root_cause_representation({huge}, {}, {{}, {"v"}}); }
  catch (const std::overflow_error&) { overflow = true; }
  assert(overflow);
  huge.context.attributes[0].kind = AttributeKind::kString;
  assert(std::isinf(build_root_cause_representation({huge}, {}, {{}, {"v"}}).data[0][0]));
}

void test_byte_values() {
  using namespace minera;
  std::string all;
  for (int i = 0; i < 256; ++i) all += static_cast<char>(i);
  assert(decode_bytes_attribute(make_bytes_attribute("v", all)) == all);
  assert(root_cause_attribute_category(make_bytes_attribute("v", "")) == "b''");
  assert(root_cause_attribute_category(make_bytes_attribute("v", "a'b")) == "b\"a'b\"");
  const std::string controls{"\0\t\n\r\x1f\x7f\x80\xff", 8};
  assert(root_cause_attribute_category(make_bytes_attribute("v", controls)) == "b'\\x00\\t\\n\\r\\x1f\\x7f\\x80\\xff'");
  auto a = make_bytes_attribute("v", std::string(1, static_cast<char>(0xaf)));
  auto b = a;
  b.value = "AF";
  assert(root_cause_scalar_keys_equal(a, b));
  assert(root_cause_scalar_key_fingerprint(a) == root_cause_scalar_key_fingerprint(b));
  a = make_bytes_attribute("v", "1");
  assert(!root_cause_scalar_keys_equal(a, EventAttribute{"v", "1"}));
  assert(!root_cause_scalar_keys_equal(a, EventAttribute{"v", "b'1'"}));
  const auto representation = [](const EventAttribute& value) {
    RootCauseObservation observation;
    observation.context.attributes = {value};
    return build_root_cause_representation({observation}, {}, {{}, {"v"}});
  };
  assert(representation(make_bytes_attribute("v", "1_2.5")).data[0][0] == 12.5);
  Trace derived;
  derived.case_id_attribute = make_bytes_attribute("case", "original");
  set_trace_case_id(derived, "derived-case");
  assert(derived.case_id_attribute->kind == AttributeKind::kString);
  assert(derived.case_id_attribute->value == "derived-case");
  auto authoritative = make_bytes_attribute("v", "1_2.5");
  for (const NativeAttributeValue stale : {NativeAttributeValue{std::int64_t{99}}, NativeAttributeValue{NativeDate{"invalid"}}}) {
    authoritative.typed_value = stale;
    assert(representation(authoritative).data[0][0] == 12.5);
    assert(root_cause_attribute_category(authoritative) == "b'1_2.5'");
    assert(root_cause_scalar_keys_equal(authoritative, make_bytes_attribute("v", "1_2.5")));
  }
  assert(std::isinf(representation(make_bytes_attribute("v", "inf")).data[0][0]));
  assert(std::isnan(representation(make_bytes_attribute("v", "nan")).data[0][0]));
  for (const auto& raw : {std::string(""), std::string("\xc2\xa0" "1"), std::string("\xef\xbc\x91"), std::string("1\0", 2)})
    rejects([&] { (void)representation(make_bytes_attribute("v", raw)); });
  rejects([&] { (void)decode_bytes_attribute(EventAttribute{"v", "f", AttributeKind::kBytes}); });
  rejects([&] { (void)decode_bytes_attribute(EventAttribute{"v", "zz", AttributeKind::kBytes}); });
  EventAttribute tuple{"v", ""};
  tuple.container = AttributeContainerKind::kTuple;
  tuple.children = {a};
  assert(root_cause_attribute_category(tuple) == "(b'1',)");
}

void test_set_values() {
  using namespace minera;
  const auto collection = [](AttributeContainerKind kind, std::vector<EventAttribute> children) {
    EventAttribute value{"v", ""}; value.container = kind; value.children = std::move(children); return value;
  };
  const auto set = AttributeContainerKind::kSet, frozen = AttributeContainerKind::kFrozenSet;
  const EventAttribute one{"", "1", AttributeKind::kInt}, truth{"", "true", AttributeKind::kBoolean};
  auto a = collection(frozen, {one, truth, {"", "x"}});
  auto b = collection(frozen, {{"", "x"}, {"", "1.0", AttributeKind::kFloat}});
  assert(root_cause_attribute_category(collection(set, {})) == "set()");
  assert(root_cause_attribute_category(collection(frozen, {})) == "frozenset()");
  assert(root_cause_attribute_category(a) == "frozenset({1, 'x'})");
  assert(root_cause_scalar_keys_equal(a, b));
  assert(root_cause_scalar_key_fingerprint(a) == root_cause_scalar_key_fingerprint(b));
  Trace left, right;
  left.attributes = {a}; right.attributes = {b};
  right.attributes[0].container = set;
  assert(root_cause_traces_equal(left, right));
  assert(root_cause_trace_fingerprint(left) == root_cause_trace_fingerprint(right));
  Event left_event, right_event;
  left_event.attributes = {a}; right_event.attributes = {b};
  left.events = {left_event}; right.events = {right_event};
  assert(!root_cause_traces_equal(left, right));
  right.events[0].attributes = {a};
  assert(root_cause_traces_equal(left, right));
  right.events[0].attributes[0].container = set;
  assert(!root_cause_traces_equal(left, right));
  std::vector<EventAttribute> many, reversed;
  for (int i = 0; i < 2048; ++i) many.push_back({"", std::to_string(i), AttributeKind::kInt});
  for (int i = 2047; i >= 0; --i) reversed.push_back({"", std::to_string(i) + ".0", AttributeKind::kFloat});
  auto large = collection(frozen, many), large_reversed = collection(frozen, reversed);
  large.children.insert(large.children.end(), reversed.begin(), reversed.end());
  assert(root_cause_set_members(large).size() == 2048);
  assert(root_cause_set_members(large)[1]->kind == AttributeKind::kInt);
  assert(root_cause_scalar_keys_equal(large, large_reversed));
  assert(root_cause_scalar_key_fingerprint(large) == root_cause_scalar_key_fingerprint(large_reversed));
  large_reversed.children.back().value = "2048.0";
  assert(!root_cause_scalar_keys_equal(large, large_reversed));
  auto nested = collection(set, {a, b});
  assert(root_cause_attribute_category(nested) == "{frozenset({1, 'x'})}");
  assert(!root_cause_key_is_hashable(nested));
  rejects([&] { (void)root_cause_scalar_key_fingerprint(nested); });
  auto invalid = collection(frozen, {nested});
  rejects([&] { (void)root_cause_attribute_category(invalid); });
  assert(!root_cause_key_is_hashable(invalid));
  auto nan = EventAttribute{"", "nan", AttributeKind::kFloat};
  nan.typed_value = std::numeric_limits<double>::quiet_NaN();
  auto other_nan = nan; other_nan.typed_value = std::numeric_limits<double>::quiet_NaN();
  assert(root_cause_set_members(collection(set, {nan, nan, other_nan})).size() == 2);
  RootCauseObservation observation;
  observation.context.attributes = {nested};
  const auto encoded = build_root_cause_representation({observation}, {}, {{"v"}, {}});
  assert((encoded.feature_names == std::vector<std::string>{"event:v@{frozenset({1, 'x'})}"}));
  rejects([&] { (void)build_root_cause_representation({observation}, {}, {{}, {"v"}}); });
  RootCauseSharedObservation shared;
  shared.context = make_shared_root_cause_context(observation.context);
  const auto shared_encoded = build_shared_root_cause_representation({shared}, {}, {{"v"}, {}});
  assert(shared_encoded.feature_names == encoded.feature_names);
  rejects([&] { (void)build_shared_root_cause_representation({shared}, {}, {{}, {"v"}}); });
  observation.context.attributes = {a};
  rejects([&] { (void)build_root_cause_representation({observation}, {}, {{}, {"v"}}); });
  auto frozen_shared = shared;
  frozen_shared.context = make_shared_root_cause_context(observation.context);
  rejects([&] { (void)build_shared_root_cause_representation({frozen_shared}, {}, {{}, {"v"}}); });
  auto shared_value = shared.context.attributes[0].value;
  shared_value->children = {{"", shared_value}};
  rejects([&] { (void)build_shared_root_cause_representation({shared}, {}, {{"v"}, {}}); });
  shared_value->is_null = true;
  assert((build_shared_root_cause_representation({shared}, {}, {{"v"}, {}}).feature_names == std::vector<std::string>{"event:v@None"}));
  shared_value->children.clear();
  invalid.is_null = true;
  assert(root_cause_attribute_category(invalid) == "None");
  EventAttribute null{"", "", AttributeKind::kString, true};
  assert(root_cause_scalar_keys_equal(invalid, null));
  assert(root_cause_scalar_key_fingerprint(invalid) == root_cause_scalar_key_fingerprint(null));
}

int main() {
  {
    minera::EventAttribute scalar{"flag", "true", minera::AttributeKind::kBoolean};
    scalar.typed_value = true;
    auto wrapped = scalar;
    wrapped.scalar_wrapper = true;
    assert(!minera::root_cause_key_is_hashable(wrapped));
    assert(minera::native_scalar_order_kind(wrapped) == minera::NativeScalarOrderKind::kUnsupported);
    assert(minera::root_cause_attribute_category(wrapped) == "{'value': True, 'children': {}}");
    minera::Trace plain_trace, wrapped_trace;
    plain_trace.attributes.push_back(scalar);
    wrapped_trace.attributes.push_back(wrapped);
    assert(!minera::root_cause_traces_equal(plain_trace, wrapped_trace));
    assert(minera::root_cause_trace_fingerprint(plain_trace) != minera::root_cause_trace_fingerprint(wrapped_trace));
    auto integer_wrapper = wrapped;
    integer_wrapper.kind = minera::AttributeKind::kInt;
    integer_wrapper.value = "1";
    integer_wrapper.typed_value = std::int64_t{1};
    minera::Trace integer_trace;
    integer_trace.attributes.push_back(integer_wrapper);
    assert(minera::root_cause_traces_equal(wrapped_trace, integer_trace));
    assert(minera::root_cause_trace_fingerprint(wrapped_trace) == minera::root_cause_trace_fingerprint(integer_trace));
    auto date_wrapper = wrapped;
    date_wrapper.kind = minera::AttributeKind::kDate;
    date_wrapper.value = "2024-01-01T00:00:00Z";
    date_wrapper.typed_value = minera::NativeDate{date_wrapper.value};
    minera::Trace date_trace;
    date_trace.attributes.push_back(date_wrapper);
    const auto copied_date_trace = date_trace;
    assert(minera::root_cause_traces_equal(date_trace, copied_date_trace));
    assert(minera::root_cause_trace_fingerprint(date_trace) == minera::root_cause_trace_fingerprint(copied_date_trace));
    auto ordered = wrapped;
    ordered.scalar_wrapper = false;  // Ordered children alone imply wrapping.
    ordered.scalar_children_list = true;
    assert(!minera::root_cause_key_is_hashable(ordered));
    assert(minera::root_cause_attribute_category(ordered) == "{'value': True, 'children': []}");
    minera::Trace ordered_trace;
    ordered_trace.attributes.push_back(ordered);
    assert(!minera::root_cause_traces_equal(wrapped_trace, ordered_trace));
    assert(minera::root_cause_trace_fingerprint(wrapped_trace) != minera::root_cause_trace_fingerprint(ordered_trace));
    ordered_trace.attributes[0].children = {{"same", "first"}, {"same", "second"}};
    const auto ordered_copy = ordered_trace;
    assert(minera::root_cause_traces_equal(ordered_trace, ordered_copy));
    assert(minera::root_cause_trace_fingerprint(ordered_trace) == minera::root_cause_trace_fingerprint(ordered_copy));
    std::reverse(ordered_trace.attributes[0].children.begin(), ordered_trace.attributes[0].children.end());
    assert(!minera::root_cause_traces_equal(ordered_trace, ordered_copy));
    bool rejected = false;
    try { (void)minera::native_attribute_number(wrapped); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  }
  test_set_values();
  test_byte_values();
  test_integer_float_overflow();
  test_shared_cyclic_encoding();
  test_tuple_values();
  test_shared_context_log();
  test_native_scalar_identity();
  test_trace_equivalence();
  test_numeric_tree();
  test_encoding();
  test_categorical_feature_collisions();
  test_public_root_cause_helpers();
  test_equivalent_feature_choices();
  test_missing_route_order();
  test_balancing();
  test_missing_and_errors();
  test_numeric_string_syntax();
  test_first_class_encoding();
  test_extended_categories();
  test_depth_and_interactions();
  std::cout << "Root cause tests passed\n";
}
