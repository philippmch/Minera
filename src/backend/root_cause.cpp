#include "minera/root_cause.hpp"
#include "minera/timestamp.hpp"
#include "root_cause_unicode.hpp"
#include "root_cause_trace_key.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <numeric>
#include <random>
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
  const EventAttribute* selected = nullptr;
  int selected_priority = 0;
  std::size_t selected_count = 0;
  for (const auto& value : event.attributes) {
    const int priority = value.key == key ? 2
        : !value.first_class_source_key.empty() && value.first_class_source_key == key ? 1 : 0;
    if (priority == 0 || priority < selected_priority) continue;
    if (priority > selected_priority) {
      selected = &value;
      selected_priority = priority;
      selected_count = 1;
    } else ++selected_count;
  }
  if (selected_count > 1)
    throw std::invalid_argument("root cause: ambiguous selected attribute: " + key);
  if (selected) return cell_value(*selected);
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
  const auto matches = [&](const std::string& source, const std::optional<EventAttribute>& stored,
                           std::string_view canonical, std::string_view alias) {
    return key == canonical || key == alias || (!source.empty() && key == source) ||
           (stored && (stored->key == key ||
                       (!stored->first_class_source_key.empty() && stored->first_class_source_key == key)));
  };
  if (matches(event.activity_source_key, event.activity_attribute, "concept:name", "activity"))
    return projected(event.activity, event.activity_state, event.activity_attribute);
  if (matches(event.resource_source_key, event.resource_attribute, "org:resource", "resource"))
    return projected(event.resource, event.resource_state, event.resource_attribute);
  if (matches(event.timestamp_source_key, event.timestamp_attribute, "time:timestamp", "timestamp"))
    return projected(event.timestamp, event.timestamp_state, event.timestamp_attribute);
  if (matches(event.lifecycle_source_key, event.lifecycle_attribute, "lifecycle:transition", "lifecycle"))
    return projected(event.lifecycle, event.lifecycle_state, event.lifecycle_attribute);
  return std::nullopt;
}

double decimal_number(const std::string& text, const std::string& key);

char32_t category_codepoint(const std::string& value, std::size_t& position) {
    const auto first = static_cast<unsigned char>(value[position++]);
    char32_t ch = first;
    int continuation = 0;
    if (first >= 0xc2 && first <= 0xdf) { ch = first & 0x1f; continuation = 1; }
    else if (first >= 0xe0 && first <= 0xef) { ch = first & 0x0f; continuation = 2; }
    else if (first >= 0xf0 && first <= 0xf4) { ch = first & 0x07; continuation = 3; }
    else if (first >= 0x80) throw std::invalid_argument("root cause: invalid UTF-8 category");
    for (int i = 0; i < continuation; ++i) {
      if (position == value.size()) throw std::invalid_argument("root cause: invalid UTF-8 category");
      const auto next = static_cast<unsigned char>(value[position++]);
      if ((next & 0xc0) != 0x80) throw std::invalid_argument("root cause: invalid UTF-8 category");
      ch = (ch << 6) | (next & 0x3f);
    }
    if ((continuation == 1 && ch < 0x80) || (continuation == 2 && ch < 0x800) ||
        (continuation == 3 && ch < 0x10000) || ch > 0x10ffff || (ch >= 0xd800 && ch <= 0xdfff))
      throw std::invalid_argument("root cause: invalid UTF-8 category");
    return ch;
}

std::string numeric_unicode_text(const std::string& value) {
  std::string result;
  for (std::size_t position = 0; position < value.size();) {
    const auto start = position;
    const auto ch = category_codepoint(value, position);
    const int decimal = ch >= 128 ? root_cause_unicode_decimal(ch) : -1;
    if (decimal >= 0) result += static_cast<char>('0' + decimal);
    else if (ch >= 128 && root_cause_unicode_space(ch)) result += ' ';
    else result.append(value, start, position - start);
  }
  return result;
}

std::string quoted_category(const std::string& value) {
  const char quote = value.find('\'') != std::string::npos && value.find('"') == std::string::npos ? '"' : '\'';
  std::string result(1, quote);
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t position = 0; position < value.size();) {
    const auto start = position;
    const auto ch = category_codepoint(value, position);
    if (ch == '\\' || ch == static_cast<unsigned char>(quote)) {
      result += '\\';
      result += static_cast<char>(ch);
    } else if (ch == '\n') result += "\\n";
    else if (ch == '\r') result += "\\r";
    else if (ch == '\t') result += "\\t";
    else if (!root_cause_unicode_printable(ch)) {
      const int digits = ch <= 0xff ? 2 : ch <= 0xffff ? 4 : 8;
      result += digits == 2 ? "\\x" : digits == 4 ? "\\u" : "\\U";
      for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4)
        result += hex[(ch >> shift) & 15];
    } else result.append(value, start, position - start);
  }
  result += quote;
  return result;
}

std::string date_category(std::string text, bool nested) {
  text.erase(text.find_last_not_of(" \t\r\n\f\v") + 1);
  const auto offset_start = text.find_first_of("+-Zz", 10);
  const auto civil_text = text.substr(0, offset_start);
  if (!parse_timestamp(civil_text) || (offset_start != std::string::npos && civil_text.size() <= 10))
    throw std::invalid_argument("root cause: invalid categorical date");
  const auto field = [&](std::size_t start, std::size_t size) {
    return std::stoi(text.substr(start, size));
  };
  const int year = field(0, 4), month = field(5, 2), day = field(8, 2);
  if (year == 0) throw std::invalid_argument("root cause: invalid categorical date year");
  int hour = 0, minute = 0, second = 0, microsecond = 0;
  std::size_t position = 10;
  if (position < text.size()) {
    hour = field(11, 2);
    minute = field(14, 2);
    position = 16;
    if (position < text.size() && text[position] == ':') {
      second = field(position + 1, 2);
      position += 3;
    }
    if (position < text.size() && text[position] == '.') {
      ++position;
      int places = 0;
      while (position < text.size() && text[position] >= '0' && text[position] <= '9') {
        if (places < 6) microsecond = microsecond * 10 + (text[position] - '0');
        ++places;
        ++position;
      }
      while (places++ < 6) microsecond *= 10;
    }
  }
  const bool has_offset = position < text.size();
  std::int64_t offset = 0;
  if (has_offset && (text[position] == 'Z' || text[position] == 'z')) {
    if (position + 1 != text.size()) throw std::invalid_argument("root cause: invalid categorical date offset");
  } else if (has_offset) {
    const int sign = text[position] == '-' ? -1 : 1;
    ++position;
    const auto pair = [&]() {
      if (position + 2 > text.size() || text[position] < '0' || text[position] > '9' ||
          text[position + 1] < '0' || text[position + 1] > '9')
        throw std::invalid_argument("root cause: invalid categorical date offset");
      const int value = (text[position] - '0') * 10 + text[position + 1] - '0';
      position += 2;
      return value;
    };
    const int offset_hour = pair();
    int offset_minute = 0, offset_second = 0, offset_microsecond = 0;
    const bool colon = position < text.size() && text[position] == ':';
    if (colon) ++position;
    if (position < text.size() || colon) offset_minute = pair();
    if (position < text.size()) {
      if (colon) {
        if (text[position++] != ':') throw std::invalid_argument("root cause: invalid categorical date offset");
      }
      offset_second = pair();
      if (position < text.size() && text[position] == '.') {
        ++position;
        int places = 0;
        while (position < text.size() && text[position] >= '0' && text[position] <= '9') {
          if (places < 6) offset_microsecond = offset_microsecond * 10 + text[position] - '0';
          ++places;
          ++position;
        }
        if (places == 0) throw std::invalid_argument("root cause: invalid categorical date offset");
        while (places++ < 6) offset_microsecond *= 10;
      }
    }
    if (position != text.size() || offset_hour >= 24 || offset_minute >= 60 || offset_second >= 60)
      throw std::invalid_argument("root cause: invalid categorical date offset");
    offset = ((static_cast<std::int64_t>(offset_hour) * 60 + offset_minute) * 60 + offset_second) * 1000000 + offset_microsecond;
    offset *= sign;
  }
  const auto pad = [](int value, std::size_t width) {
    const auto digits = std::to_string(value);
    return std::string(width - digits.size(), '0') + digits;
  };
  if (!nested) {
    std::string result = text.substr(0, 10) + " " + pad(hour, 2) + ":" + pad(minute, 2) + ":" + pad(second, 2);
    if (microsecond != 0) result += "." + pad(microsecond, 6);
    if (has_offset) {
      const auto magnitude = std::abs(offset);
      result += std::string(offset < 0 ? "-" : "+") + pad(static_cast<int>(magnitude / 3600000000LL), 2) + ":" + pad(static_cast<int>(magnitude / 60000000 % 60), 2);
      if (magnitude % 60000000 != 0) {
        result += ":" + pad(static_cast<int>(magnitude / 1000000 % 60), 2);
        if (magnitude % 1000000 != 0) result += "." + pad(static_cast<int>(magnitude % 1000000), 6);
      }
    }
    return result;
  }
  std::string result = "datetime.datetime(" + std::to_string(year) + ", " + std::to_string(month) + ", " + std::to_string(day) + ", " + std::to_string(hour) + ", " + std::to_string(minute);
  if (second != 0 || microsecond != 0) result += ", " + std::to_string(second);
  if (microsecond != 0) result += ", " + std::to_string(microsecond);
  if (has_offset) {
    if (offset == 0) result += ", tzinfo=datetime.timezone.utc";
    else {
      result += ", tzinfo=datetime.timezone(datetime.timedelta(";
      const auto remainder = offset < 0 ? 86400000000LL + offset : offset;
      bool comma = false;
      if (offset < 0) { result += "days=-1"; comma = true; }
      if (remainder / 1000000 != 0) {
        if (comma) result += ", ";
        result += "seconds=" + std::to_string(remainder / 1000000);
        comma = true;
      }
      if (remainder % 1000000 != 0) {
        if (comma) result += ", ";
        result += "microseconds=" + std::to_string(remainder % 1000000);
      }
      result += "))";
    }
  }
  return result + ")";
}

std::string scalar_category(const EventAttribute& value, bool nested) {
  if (value.is_null) return "None";
  if (const auto* boolean = std::get_if<bool>(&value.typed_value))
    return *boolean ? "True" : "False";
  if (const auto* integer = std::get_if<std::int64_t>(&value.typed_value))
    return std::to_string(*integer);
  if (const auto* real = std::get_if<double>(&value.typed_value))
    return real_category(*real);
  if (const auto* date = std::get_if<NativeDate>(&value.typed_value))
    return date_category(date->value, nested);
  if (value.kind == AttributeKind::kDate) return date_category(value.value, nested);
  if (value.kind == AttributeKind::kBoolean) {
    if (value.value == "true" || value.value == "1") return "True";
    if (value.value == "false" || value.value == "0") return "False";
    throw std::invalid_argument("root cause: invalid categorical boolean: " + value.key);
  }
  if (value.kind == AttributeKind::kFloat) return real_category(decimal_number(value.value, value.key));
  if (value.kind == AttributeKind::kInt) {
    const auto begin = value.value.find_first_not_of(" \t\r\n\f\v");
    if (begin == std::string::npos) throw std::invalid_argument("root cause: invalid categorical integer: " + value.key);
    auto token = value.value.substr(begin, value.value.find_last_not_of(" \t\r\n\f\v") - begin + 1);
    const bool negative = token[0] == '-';
    if (token[0] == '+' || negative) token.erase(0, 1);
    if (token.empty() || token.find_first_not_of("0123456789") != std::string::npos)
      throw std::invalid_argument("root cause: invalid categorical integer: " + value.key);
    const auto significant = token.find_first_not_of('0');
    if (significant == std::string::npos) return "0";
    return (negative ? "-" : "") + token.substr(significant);
  }
  const auto quoted = quoted_category(value.value);
  return nested ? quoted : value.value;
}

std::string attribute_category(const EventAttribute& value, bool nested) {
  if (value.container == AttributeContainerKind::kScalar && value.children.empty())
    return scalar_category(value, nested);
  const bool list = value.container == AttributeContainerKind::kList;
  std::string result = "{'value': " + (value.container == AttributeContainerKind::kScalar ? scalar_category(value, true) : "None") + ", 'children': ";
  result += list ? '[' : '{';
  std::vector<const EventAttribute*> children;
  std::map<std::string, std::size_t> positions;
  for (const auto& child : value.children) {
    if (list) children.push_back(&child);
    else {
      const auto [found, inserted] = positions.emplace(child.key, children.size());
      if (inserted) children.push_back(&child);
      else children[found->second] = &child;
    }
  }
  for (std::size_t i = 0; i < children.size(); ++i) {
    if (i) result += ", ";
    if (list) result += '(';
    result += quoted_category(children[i]->key) + (list ? ", " : ": ") + attribute_category(*children[i], true);
    if (list) result += ')';
  }
  result += list ? ']' : '}';
  return result + '}';
}

std::string category_for(const Event& event, const std::string& key) {
  const auto value = value_for(event, key);
  return value ? attribute_category(*value, false) : "UNDEFINED";
}

double decimal_number(const std::string& source, const std::string& key) {
  const auto text = numeric_unicode_text(source);
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
  if (!value || value->is_null || value->container != AttributeContainerKind::kScalar || !value->children.empty() ||
      value->kind == AttributeKind::kDate || std::holds_alternative<NativeDate>(value->typed_value))
    throw std::invalid_argument("root cause: missing or invalid numeric attribute: " + key);
  double result = 0;
  if (const auto* real = std::get_if<double>(&value->typed_value)) result = *real;
  else if (const auto* integer = std::get_if<std::int64_t>(&value->typed_value)) result = static_cast<double>(*integer);
  else if (const auto* boolean = std::get_if<bool>(&value->typed_value)) result = *boolean ? 1.0 : 0.0;
  else if (value->kind == AttributeKind::kBoolean && (value->value == "true" || value->value == "false"))
    result = value->value == "true" ? 1.0 : 0.0;
  else result = decimal_number(value->value, key);
  return result;
}

std::array<std::size_t, 2> cohort_repeats(std::size_t fit, std::size_t problem, bool enabled) {
  if (!enabled) return {1, 1};
  if (fit == 0 || problem == 0)
    throw std::domain_error("root cause: cannot balance an empty cohort");
  return {std::max(std::size_t{1}, problem / fit), std::max(std::size_t{1}, fit / problem)};
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
    std::vector<std::size_t> feature_order(model_.feature_names.size());
    std::iota(feature_order.begin(), feature_order.end(), std::size_t{0});
    std::shuffle(feature_order.begin(), feature_order.end(), random_);
    for (const auto feature : feature_order) {
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
        if (score > best_score) {
          best_score = score;
          best_feature = static_cast<int>(feature);
          best_threshold = threshold;
          best_missing_left = missing[0] + missing[1] == 0
                                  ? l[0] + l[1] > r[0] + r[1] : missing_left;
        }
      };
      // Search every missing-right cut before any missing-left cut. Within
      // each route the lowest threshold wins equal-score ties.
      for (const bool missing_left : {false, true}) {
        if (missing_left && missing[0] + missing[1] == 0) break;
        left = {};
        right = {node.class_counts[0] - missing[0], node.class_counts[1] - missing[1]};
        for (std::size_t i = 0; i < values.size(); ++i) {
          ++left[values[i].second];
          --right[values[i].second];
          if (i + 1 == values.size()) {
            if (!missing_left && missing[0] + missing[1] > 0)
              consider(std::numeric_limits<double>::infinity(), false);
          } else if (values[i + 1].first > values[i].first + 1e-7f) {
            const double threshold = static_cast<double>(values[i].first) / 2 + static_cast<double>(values[i + 1].first) / 2;
            consider(threshold, missing_left);
          }
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
  std::mt19937 random_{std::random_device{}()};
};

}  // namespace

std::string root_cause_attribute_category(const EventAttribute& attribute) {
  return attribute_category(attribute, false);
}

EventLog build_root_cause_log(
    const std::vector<RootCauseObservation>& fit,
    const std::vector<RootCauseObservation>& problem,
    bool enable_multiplier) {
  const auto repeats = cohort_repeats(fit.size(), problem.size(), enable_multiplier);
  EventLog result;
  const auto append = [&](const auto& cohort, std::size_t copies) {
    for (std::size_t copy = 0; copy < copies; ++copy)
      for (const auto& observation : cohort) {
        Trace trace;
        trace.events.push_back(observation.context);
        result.traces.push_back(std::move(trace));
      }
  };
  append(fit, repeats[0]);
  append(problem, repeats[1]);
  return result;
}

RootCauseRepresentation build_root_cause_representation(
    const std::vector<RootCauseObservation>& fit,
    const std::vector<RootCauseObservation>& problem,
    const RootCauseOptions& options) {
  const auto repeats = cohort_repeats(fit.size(), problem.size(), options.enable_multiplier);
  RootCauseRepresentation model;
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
  append(fit, 0, repeats[0]);
  append(problem, 1, repeats[1]);
  return model;
}

std::optional<RootCauseModel> fit_root_cause_model(
    const std::vector<RootCauseObservation>& fit,
    const std::vector<RootCauseObservation>& problem,
    const RootCauseOptions& options, const std::string& problem_class) {
  if (fit.empty() || problem.empty()) return std::nullopt;
  if (options.string_attributes.empty() && options.numeric_attributes.empty())
    throw std::invalid_argument("root cause: select at least one attribute");
  auto representation = build_root_cause_representation(fit, problem, options);
  for (const auto& row : representation.data)
    for (const auto value : row)
      if (std::isinf(value) || std::isinf(static_cast<float>(value)))
        throw std::invalid_argument("root cause: numeric attribute outside float32 range");
  RootCauseModel model;
  model.feature_names = std::move(representation.feature_names);
  model.data = std::move(representation.data);
  model.target = std::move(representation.target);
  model.source_trace_indices = std::move(representation.source_trace_indices);
  model.classes = {"fit", problem_class};
  model.fit_count = fit.size();
  model.problem_count = problem.size();
  std::vector<std::size_t> rows(model.data.size());
  std::iota(rows.begin(), rows.end(), std::size_t{0});
  Trainer(model).grow(rows, 0);
  return model;
}

}  // namespace minera
