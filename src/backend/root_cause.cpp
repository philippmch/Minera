#include "minera/root_cause.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <utility>

namespace minera {
namespace {

std::string real_category(double value) {
  if (std::isnan(value)) return "nan";
  if (std::isinf(value)) return value < 0 ? "-inf" : "inf";
  char buffer[128];
  const double magnitude = std::abs(value);
  const auto format = magnitude == 0 || (magnitude >= 1e-4 && magnitude < 1e16)
                          ? std::chars_format::fixed : std::chars_format::scientific;
  const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value, format);
  if (converted.ec != std::errc{})
    throw std::invalid_argument("root cause: cannot encode categorical number");
  std::string text(buffer, converted.ptr);
  if (text.find_first_of(".e") == std::string::npos) text += ".0";
  return text;
}

std::optional<EventAttribute> value_for(const Event& event,
                                      const std::string& key) {
  const auto cell_value = [](EventAttribute value) -> std::optional<EventAttribute> {
    if (value.first_class_role != FirstClassAttributeRole::kNone &&
        value.first_class_state == FirstClassAttributeState::kMissing)
      return std::nullopt;
    if (value.first_class_state == FirstClassAttributeState::kNull)
      value.is_null = true;
    return value;
  };
  for (const auto& value : event.attributes)
    if (value.key == key) return cell_value(value);
  for (const auto& value : event.attributes)
    if (!value.first_class_source_key.empty() && value.first_class_source_key == key)
      return cell_value(value);
  const auto projected = [&](const std::string& value,
                             FirstClassAttributeState state,
                             const std::optional<EventAttribute>& stored)
      -> std::optional<EventAttribute> {
    if (state == FirstClassAttributeState::kMissing && (stored || value.empty()))
      return std::nullopt;
    if (stored && state != FirstClassAttributeState::kNull) return cell_value(*stored);
    EventAttribute result;
    result.key = key;
    result.value = value;
    result.is_null = state == FirstClassAttributeState::kNull;
    return result;
  };
  const auto matches = [&](const std::string& source, std::string_view canonical,
                           std::string_view alias) {
    return key == canonical || key == alias || (!source.empty() && key == source);
  };
  if (matches(event.activity_source_key, "concept:name", "activity"))
    return projected(event.activity, event.activity_state, event.activity_attribute);
  if (matches(event.resource_source_key, "org:resource", "resource"))
    return projected(event.resource, event.resource_state, event.resource_attribute);
  if (matches(event.timestamp_source_key, "time:timestamp", "timestamp"))
    return projected(event.timestamp, event.timestamp_state, event.timestamp_attribute);
  if (matches(event.lifecycle_source_key, "lifecycle:transition", "lifecycle"))
    return projected(event.lifecycle, event.lifecycle_state, event.lifecycle_attribute);
  return std::nullopt;
}

std::string category_for(const Event& event, const std::string& key) {
  const auto value = value_for(event, key);
  if (!value) return "UNDEFINED";
  if (value->is_null) return "None";
  if (value->container != AttributeContainerKind::kScalar)
    throw std::invalid_argument("root cause: invalid categorical attribute: " + key);
  if (const auto* boolean = std::get_if<bool>(&value->typed_value))
    return *boolean ? "True" : "False";
  if (const auto* integer = std::get_if<std::int64_t>(&value->typed_value))
    return std::to_string(*integer);
  if (const auto* real = std::get_if<double>(&value->typed_value))
    return real_category(*real);
  if (value->kind == AttributeKind::kBoolean) {
    if (value->value == "true" || value->value == "1") return "True";
    if (value->value == "false" || value->value == "0") return "False";
  }
  return value->value;
}

double decimal_number(const std::string& text, const std::string& key) {
  const auto invalid = [&]() -> double {
    throw std::invalid_argument("root cause: invalid numeric attribute: " + key);
  };
  const auto begin = text.find_first_not_of(" \t\r\n\f\v");
  if (begin == std::string::npos) return invalid();
  const auto end = text.find_last_not_of(" \t\r\n\f\v");
  std::string token = text.substr(begin, end - begin + 1);
  std::size_t position = token[0] == '+' || token[0] == '-' ? 1 : 0;
  std::string special = token.substr(position);
  for (auto& ch : special)
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  if (special == "nan")
    return std::copysign(std::numeric_limits<double>::quiet_NaN(), token[0] == '-' ? -1.0 : 1.0);
  if (special == "inf" || special == "infinity")
    return token[0] == '-' ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
  const auto digit = [](char ch) { return ch >= '0' && ch <= '9'; };
  // Separators are allowed only between decimal digits, including exponent
  // digits. Validate before removing them so malformed literals stay errors.
  for (std::size_t i = 0; i < token.size(); ++i)
    if (token[i] == '_' && (i == 0 || i + 1 == token.size() || !digit(token[i - 1]) || !digit(token[i + 1])))
      return invalid();
  token.erase(std::remove(token.begin(), token.end(), '_'), token.end());
  const auto digits = [&]() {
    const auto start = position;
    while (position < token.size() && digit(token[position])) ++position;
    return position - start;
  };
  std::size_t mantissa_digits = digits();
  if (position < token.size() && token[position] == '.') {
    ++position;
    mantissa_digits += digits();
  }
  if (mantissa_digits == 0) return invalid();
  if (position < token.size() && (token[position] == 'e' || token[position] == 'E')) {
    ++position;
    if (position < token.size() && (token[position] == '+' || token[position] == '-')) ++position;
    if (digits() == 0) return invalid();
  }
  if (position != token.size()) return invalid();
  char* parsed_end = nullptr;
  // strtod retains subnormals and signed zero on underflow. Overflow remains
  // infinity and is rejected by the matrix's float32 range validation.
  const double result = std::strtod(token.c_str(), &parsed_end);
  if (parsed_end != token.c_str() + token.size()) return invalid();
  return result;
}

double number_for(const Event& event, const std::string& key) {
  const auto value = value_for(event, key);
  if (!value || value->is_null || value->container != AttributeContainerKind::kScalar)
    throw std::invalid_argument("root cause: missing or invalid numeric attribute: " + key);
  double result = 0;
  if (const auto* real = std::get_if<double>(&value->typed_value)) result = *real;
  else if (const auto* integer = std::get_if<std::int64_t>(&value->typed_value)) result = static_cast<double>(*integer);
  else if (const auto* boolean = std::get_if<bool>(&value->typed_value)) result = *boolean ? 1.0 : 0.0;
  else if (value->kind == AttributeKind::kBoolean && (value->value == "true" || value->value == "false"))
    result = value->value == "true" ? 1.0 : 0.0;
  else result = decimal_number(value->value, key);
  if (std::isinf(result) || std::isinf(static_cast<float>(result)))
    throw std::invalid_argument("root cause: numeric attribute outside float32 range: " + key);
  return result;
}

using Counts = std::array<std::size_t, 2>;
double purity(const Counts& counts) {
  const double a = static_cast<double>(counts[0]);
  const double b = static_cast<double>(counts[1]);
  return a + b == 0 ? 0 : (a * a + b * b) / (a + b);
}

class Trainer {
 public:
  explicit Trainer(RootCauseModel& model) : model_(model) {}
  int grow(const std::vector<std::size_t>& rows, int depth) {
    RootCauseTreeNode node;
    for (const auto row : rows) ++node.class_counts[model_.target[row]];
    const int index = static_cast<int>(model_.tree.size());
    model_.tree.push_back(node);
    if (depth == 7 || rows.size() < 2 || node.class_counts[0] == 0 || node.class_counts[1] == 0)
      return index;
    double best_score = -1;
    int best_feature = -1;
    double best_threshold = 0;
    bool best_missing_left = false;
    for (std::size_t feature = 0; feature < model_.feature_names.size(); ++feature) {
      std::vector<std::pair<float, int>> values;
      Counts missing{};
      for (const auto row : rows) {
        const float value = static_cast<float>(model_.data[row][feature]);
        if (std::isnan(value)) ++missing[model_.target[row]];
        else values.emplace_back(value, model_.target[row]);
      }
      std::sort(values.begin(), values.end());
      Counts left{};
      Counts right{node.class_counts[0] - missing[0], node.class_counts[1] - missing[1]};
      const auto consider = [&](double threshold, bool missing_left) {
        Counts l = left, r = right;
        auto& destination = missing_left ? l : r;
        for (std::size_t c = 0; c < 2; ++c) destination[c] += missing[c];
        if (l[0] + l[1] == 0 || r[0] + r[1] == 0) return;
        const double score = purity(l) + purity(r);
        if (score > best_score + 1e-12) {
          best_score = score;
          best_feature = static_cast<int>(feature);
          best_threshold = threshold;
          best_missing_left = missing[0] + missing[1] == 0
                                  ? l[0] + l[1] > r[0] + r[1] : missing_left;
        }
      };
      for (std::size_t i = 0; i < values.size(); ++i) {
        ++left[values[i].second];
        --right[values[i].second];
        if (i + 1 == values.size()) {
          if (missing[0] + missing[1] > 0) consider(std::numeric_limits<double>::infinity(), false);
        } else if (values[i + 1].first > values[i].first + 1e-7f) {
          const double threshold = static_cast<double>(values[i].first) / 2 + static_cast<double>(values[i + 1].first) / 2;
          consider(threshold, false);
          if (missing[0] + missing[1] > 0) consider(threshold, true);
        }
      }
    }
    if (best_feature < 0) return index;
    std::vector<std::size_t> left_rows, right_rows;
    for (const auto row : rows) {
      const float value = static_cast<float>(model_.data[row][static_cast<std::size_t>(best_feature)]);
      const bool goes_left = std::isnan(value) ? best_missing_left : value <= best_threshold;
      (goes_left ? left_rows : right_rows).push_back(row);
    }
    model_.tree[index].feature_index = best_feature;
    model_.tree[index].missing_only = std::isinf(best_threshold);
    model_.tree[index].threshold = std::isinf(best_threshold) ? 0 : best_threshold;
    model_.tree[index].missing_go_to_left = best_missing_left;
    const int left_index = grow(left_rows, depth + 1);
    const int right_index = grow(right_rows, depth + 1);
    model_.tree[index].left = left_index;
    model_.tree[index].right = right_index;
    return index;
  }
 private:
  RootCauseModel& model_;
};

}  // namespace

std::optional<RootCauseModel> fit_root_cause_model(
    const std::vector<RootCauseObservation>& fit,
    const std::vector<RootCauseObservation>& problem,
    const RootCauseOptions& options, const std::string& problem_class) {
  if (fit.empty() || problem.empty()) return std::nullopt;
  if (options.string_attributes.empty() && options.numeric_attributes.empty())
    throw std::invalid_argument("root cause: select at least one attribute");
  RootCauseModel model;
  model.classes = {"fit", problem_class};
  model.fit_count = fit.size();
  model.problem_count = problem.size();
  std::vector<std::vector<std::string>> categories;
  for (const auto& key : options.string_attributes) {
    std::set<std::string> values;
    for (const auto* cohort : {&fit, &problem})
      for (const auto& observation : *cohort) values.insert(category_for(observation.context, key));
    categories.emplace_back(values.begin(), values.end());
    for (const auto& value : values) model.feature_names.push_back("event:" + key + "@" + value);
  }
  for (const auto& key : options.numeric_attributes) model.feature_names.push_back("event:" + key);
  std::map<std::string, std::size_t> column_indices;
  for (std::size_t i = 0; i < model.feature_names.size(); ++i)
    column_indices[model.feature_names[i]] = i;
  const auto append = [&](const std::vector<RootCauseObservation>& cohort, int target, std::size_t repeats) {
    std::vector<std::vector<double>> encoded;
    for (const auto& observation : cohort) {
      std::vector<double> row(model.feature_names.size(), 0.0);
      for (std::size_t i = 0; i < categories.size(); ++i) {
        const auto value = category_for(observation.context, options.string_attributes[i]);
        row[column_indices.at("event:" + options.string_attributes[i] + "@" + value)] = 1.0;
      }
      for (const auto& key : options.numeric_attributes)
        row[column_indices.at("event:" + key)] = number_for(observation.context, key);
      encoded.push_back(std::move(row));
    }
    for (std::size_t repeat = 0; repeat < repeats; ++repeat)
      for (std::size_t i = 0; i < cohort.size(); ++i) {
        model.data.push_back(encoded[i]);
        model.target.push_back(target);
        model.source_trace_indices.push_back(cohort[i].trace_index);
      }
  };
  append(fit, 0, options.enable_multiplier ? std::max(std::size_t{1}, problem.size() / fit.size()) : 1);
  append(problem, 1, options.enable_multiplier ? std::max(std::size_t{1}, fit.size() / problem.size()) : 1);
  std::vector<std::size_t> rows(model.data.size());
  std::iota(rows.begin(), rows.end(), std::size_t{0});
  Trainer(model).grow(rows, 0);
  return model;
}

}  // namespace minera
