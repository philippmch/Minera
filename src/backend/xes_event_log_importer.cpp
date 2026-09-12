#include "minera/xes_event_log_importer.hpp"

#include "gzip_codec.hpp"
#include "xml_pull_parser.hpp"
#include "rust_xes_unicode.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <utility>
#include <vector>

#include "minera/timestamp.hpp"

namespace minera {
namespace {

// XML names may be namespace-qualified.  The XES vocabulary is identified by
// its local name, while the attribute values remain untouched.
std::string_view local_name(std::string_view name) {
  const auto separator = name.rfind(':');
  return separator == std::string_view::npos ? name
                                             : name.substr(separator + 1);
}

// The XES attribute element names and the kind each maps to.
std::optional<AttributeKind> attribute_kind_of(std::string_view element) {
  element = local_name(element);
  if (element == "string") return AttributeKind::kString;
  if (element == "date") return AttributeKind::kDate;
  if (element == "int") return AttributeKind::kInt;
  if (element == "float") return AttributeKind::kFloat;
  if (element == "boolean") return AttributeKind::kBoolean;
  if (element == "id") return AttributeKind::kId;
  return std::nullopt;
}

bool true_attribute(const std::vector<std::pair<std::string, std::string>>& attrs,
                   std::string_view name) {
  const auto* value = detail::attribute(attrs, name);
  if (!value) return false;
  std::string normalized = *value;
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  return normalized == "true" || normalized == "1" || normalized == "yes";
}

NativeAttributeValue parse_native_value(AttributeKind kind,
                                       std::string_view value,
                                       XesImportVariant variant) {
  switch (kind) {
    case AttributeKind::kInt: {
      if (value.empty())
        throw std::invalid_argument("XES int attribute has an empty value");
      const char* first = value.data();
      const char* last = value.data() + value.size();
      if (*first == '+') ++first;
      std::int64_t parsed = 0;
      const auto result = std::from_chars(first, last, parsed, 10);
      if (result.ec != std::errc{} || result.ptr != last) {
        throw std::invalid_argument("XES int attribute has an invalid value: " +
                                    std::string(value));
      }
      return parsed;
    }
    case AttributeKind::kFloat: {
      if (value == "INF" || value == "+INF")
        return std::numeric_limits<double>::infinity();
      if (value == "-INF")
        return -std::numeric_limits<double>::infinity();
      if (value == "NaN") return std::numeric_limits<double>::quiet_NaN();
      double parsed = 0.0;
      const char* first = value.data();
      if (!value.empty() && *first == '+') ++first;
      const auto result = std::from_chars(
          first, value.data() + value.size(), parsed,
          std::chars_format::general);
      if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        throw std::invalid_argument(
            "XES float attribute has an invalid value: " +
            std::string(value));
      }
      return parsed;
    }
    case AttributeKind::kBoolean: {
      if (variant == XesImportVariant::kLineByLine ||
          variant == XesImportVariant::kChunkRegex ||
          variant == XesImportVariant::kRustXes)
        return value == "true";
      // Iterparse accepts case-insensitive true; every other lexical value
      // (including the XSD spelling 1) is false in the pinned reference.
      return value.size() == 4 && (value[0] == 't' || value[0] == 'T') &&
          (value[1] == 'r' || value[1] == 'R') &&
          (value[2] == 'u' || value[2] == 'U') &&
          (value[3] == 'e' || value[3] == 'E');
    }
    case AttributeKind::kDate: {
      auto parsed = parse_timestamp(value);
      if (!parsed) {
        throw std::invalid_argument("XES date attribute has an invalid value: " +
                                    std::string(value));
      }
      std::string wall_time;
      if (variant != XesImportVariant::kRustXes) {
        const auto time = value.find_first_of("Tt ");
        const auto offset = time == std::string_view::npos ? std::string_view::npos
            : value.find_first_of("+-", time + 1);
        if (offset != std::string_view::npos) {
          // These parser variations retain the clock fields and attach UTC.
          wall_time = std::string(value.substr(0, offset)) + 'Z';
          value = wall_time;
          parsed = parse_timestamp(value);
        }
      }
      if (const auto dot = value.find('.'); dot != std::string_view::npos) {
        auto end = dot + 1;
        while (end < value.size() && value[end] >= '0' && value[end] <= '9') ++end;
        if (end - dot - 1 > 3) {
          auto integral = std::string(value);
          integral.erase(dot, end - dot);
          const auto whole = parse_timestamp(integral);
          if (!whole) throw std::invalid_argument("XES date has an invalid integral instant");
          auto normalized = format_timestamp(*whole);
          normalized.resize(normalized.find('.'));
          normalized += '.';
          normalized += value.substr(dot + 1, std::min<std::size_t>(6, end - dot - 1));
          normalized += 'Z';
          return NativeDate{std::move(normalized)};
        }
      }
      return NativeDate{format_timestamp(*parsed)};
    }
    case AttributeKind::kString:
    case AttributeKind::kId:
      return std::monostate{};
  }
  return std::monostate{};
}

void skip_element(detail::XmlPullParser& parser,
                  const detail::XmlPullParser::Token& start) {
  constexpr std::size_t kMaxSkippedDepth = 128;
  if (start.self_closing) return;
  std::size_t depth = 1;
  while (depth != 0) {
    const auto token = parser.next();
    if (token.kind == detail::XmlPullParser::Kind::EndOfInput) return;
    if (token.kind == detail::XmlPullParser::Kind::StartElement &&
        !token.self_closing) {
      ++depth;
      if (depth > kMaxSkippedDepth)
        throw std::invalid_argument("XES attribute nesting exceeds the maximum depth");
    } else if (token.kind == detail::XmlPullParser::Kind::EndElement) {
      --depth;
    }
  }
}


struct RustXesNode {
  std::string key;
  std::string tag;
  std::string value;
  bool has_value = false;
  std::vector<RustXesNode> contents;
  std::vector<RustXesNode> own;
};

std::string rust_debug_string(std::string_view value) {
  std::string result = "\"";
  for (std::size_t i = 0; i < value.size();) {
    const auto begin = i;
    const auto first = static_cast<unsigned char>(value[i++]);
    std::uint32_t cp = first;
    unsigned more = 0;
    if (first >= 0xf0) { cp = first & 7; more = 3; }
    else if (first >= 0xe0) { cp = first & 15; more = 2; }
    else if (first >= 0xc0) { cp = first & 31; more = 1; }
    while (more-- && i < value.size()) cp = (cp << 6) | (static_cast<unsigned char>(value[i++]) & 63);
    if (cp == '"') result += "\\\"";
    else if (cp == '\\') result += "\\\\";
    else if (cp == '\n') result += "\\n";
    else if (cp == '\r') result += "\\r";
    else if (cp == '\t') result += "\\t";
    else if (cp == 0) result += "\\0";
    else if (detail::rust_xes_debug_escape(cp)) {
      char hex[16];
      const auto converted = std::to_chars(hex, hex + sizeof(hex), cp, 16);
      result += "\\u{" + std::string(hex, converted.ptr) + "}";
    } else result.append(value.substr(begin, i - begin));
  }
  return result + '"';
}

std::string rust_debug_float(double value) {
  if (std::isnan(value)) return "NaN";
  if (std::isinf(value)) return value < 0 ? "-inf" : "inf";
  char buffer[128];
  const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value,
                                     std::chars_format::scientific);
  std::string scientific(buffer, converted.ptr);
  const auto exponent_at = scientific.find('e');
  const auto exponent = std::stoi(scientific.substr(exponent_at + 1));
  auto mantissa = scientific.substr(0, exponent_at);
  if (exponent < -4 || exponent >= 16)
    return mantissa + 'e' + std::to_string(exponent);
  const bool negative = mantissa.starts_with('-');
  if (negative) mantissa.erase(0, 1);
  std::erase(mantissa, '.');
  const auto position = exponent + 1;
  if (position <= 0) mantissa = "0." + std::string(-position, '0') + mantissa;
  else if (static_cast<std::size_t>(position) >= mantissa.size())
    mantissa += std::string(position - mantissa.size(), '0') + ".0";
  else mantissa.insert(position, ".");
  return (negative ? "-" : "") + mantissa;
}

std::string rust_debug_date(std::string value) {
  if (value.size() < 19 || (value[10] != 'T' && value[10] != 't' && value[10] != ' '))
    throw std::invalid_argument("RUSTXES date requires a complete clock");
  const auto offset = value.find_first_of("+-", 19);
  if (offset != std::string::npos &&
      (value.size() - offset != 6 || value[offset + 3] != ':'))
    throw std::invalid_argument("RUSTXES date offset requires a colon");
  auto validation = value;
  if (validation.substr(17, 2) == "60") validation.replace(17, 2, "59");
  (void)parse_native_value(AttributeKind::kDate, validation, XesImportVariant::kRustXes);
  if (value.ends_with('Z') || value.ends_with('z')) value.replace(value.size() - 1, 1, "+00:00");
  else if (offset == std::string::npos) value += "+00:00";
  if (value.size() > 10) value[10] = 'T';
  const auto dot = value.find('.');
  if (dot != std::string::npos) {
    const auto end = value.find_first_not_of("0123456789", dot + 1);
    auto fraction = value.substr(dot + 1, end - dot - 1);
    while (!fraction.empty() && fraction.back() == '0') fraction.pop_back();
    if (fraction.empty()) value.erase(dot, end - dot);
    else {
      fraction.resize(fraction.size() <= 3 ? 3 : fraction.size() <= 6 ? 6 : 9, '0');
      value.replace(dot + 1, end - dot - 1, fraction);
    }
  }
  return value;
}

double rust_float_value(std::string_view lexical) {
  try {
    return std::get<double>(parse_native_value(AttributeKind::kFloat, lexical,
                                               XesImportVariant::kRustXes));
  } catch (const std::invalid_argument&) {}
  auto text = lexical;
  if (text.starts_with('+')) text.remove_prefix(1);
  double ignored = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), ignored);
  if (parsed.ec != std::errc::result_out_of_range || parsed.ptr != text.data() + text.size()) return 0;
  const bool negative = text.starts_with('-');
  if (negative) text.remove_prefix(1);
  const auto exponent_at = text.find_first_of("eE");
  const auto mantissa = text.substr(0, exponent_at);
  // Saturation only bounds arithmetic; range failures already establish that
  // the exact result lies outside the finite representable interval.
  std::int64_t exponent = 0;
  if (exponent_at != std::string_view::npos) {
    auto digits = text.substr(exponent_at + 1);
    const bool exponent_negative = digits.starts_with('-');
    if (digits.starts_with('-') || digits.starts_with('+')) digits.remove_prefix(1);
    for (const auto digit : digits)
      exponent = std::min<std::int64_t>(1000000000, exponent * 10 + digit - '0');
    if (exponent_negative) exponent = -exponent;
  }
  const auto dot = mantissa.find('.');
  const auto integer_digits = dot == std::string_view::npos ? mantissa.size() : dot;
  std::size_t digit_index = 0;
  for (const auto digit : mantissa) {
    if (digit == '.') continue;
    if (digit != '0') break;
    ++digit_index;
  }
  const auto magnitude = exponent + static_cast<std::int64_t>(integer_digits) -
      static_cast<std::int64_t>(digit_index) - 1;
  const auto result = magnitude >= 0 ? std::numeric_limits<double>::infinity() : 0.0;
  return negative ? -result : result;
}

// The table adapter's heterogeneous-column display is distinct from Rust
// container Debug output: short decimals survive, longer ones are rounded.
std::string rust_mixed_float(double value) {
  if (!std::isfinite(value)) return rust_debug_float(value);
  char buffer[768];
  auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value,
                                 std::chars_format::fixed);
  std::string text(buffer, converted.ptr);
  const auto scientific = [&](bool rounded) {
    const auto result = rounded ?
        std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::scientific, 4) :
        std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::scientific);
    std::string out(buffer, result.ptr);
    const auto at = out.find('e');
    return out.substr(0, at + 1) + std::to_string(std::stoi(out.substr(at + 1)));
  };
  if (text.size() <= 9) {
    if (std::abs(value) >= 1e6) return scientific(false);
    if (text.find('.') == std::string::npos) text += ".0";
    return text;
  }
  if (std::abs(value) > 999999 || (value != 0 && std::abs(value) < 1e-6))
    return scientific(true);
  converted = std::to_chars(buffer, buffer + sizeof(buffer), value,
                            std::chars_format::fixed, 6);
  text.assign(buffer, converted.ptr);
  while (text.ends_with('0') && text[text.size() - 2] != '.') text.pop_back();
  return text;
}

RustXesNode parse_rust_node(detail::XmlPullParser& parser,
                            const detail::XmlPullParser::Token& start,
                            std::size_t depth);

void parse_rust_contents(detail::XmlPullParser& parser, std::string_view end_name,
                         std::vector<RustXesNode>& contents, std::size_t depth) {
  if (depth > 128) throw std::invalid_argument("XES attribute nesting exceeds the maximum depth");
  bool finished_values = false;
  while (true) {
    const auto token = parser.next();
    if (token.kind == detail::XmlPullParser::Kind::EndOfInput)
      throw std::runtime_error("RUSTXES container is not closed");
    if (token.kind == detail::XmlPullParser::Kind::EndElement && local_name(token.name) == end_name) return;
    if (token.kind != detail::XmlPullParser::Kind::StartElement) continue;
    if (finished_values) { skip_element(parser, token); continue; }
    if (local_name(token.name) == "values") {
      if (!token.self_closing) parse_rust_contents(parser, "values", contents, depth + 1);
      finished_values = true;
    } else contents.push_back(parse_rust_node(parser, token, depth + 1));
  }
}

RustXesNode parse_rust_node(detail::XmlPullParser& parser,
                            const detail::XmlPullParser::Token& start,
                            std::size_t depth) {
  if (depth > 128) throw std::invalid_argument("XES attribute nesting exceeds the maximum depth");
  RustXesNode result;
  result.tag = local_name(start.name);
  if (const auto* key = detail::attribute(start.attributes, "key")) result.key = *key;
  if (const auto* value = detail::attribute(start.attributes, "value")) {
    result.value = *value;
    result.has_value = true;
  }
  if (start.self_closing) return result;
  if (result.tag == "list" || result.tag == "container") {
    parse_rust_contents(parser, result.tag, result.contents, depth);
  } else {
    while (true) {
      const auto token = parser.next();
      if (token.kind == detail::XmlPullParser::Kind::EndOfInput)
        throw std::runtime_error("RUSTXES attribute is not closed");
      if (token.kind == detail::XmlPullParser::Kind::EndElement && local_name(token.name) == result.tag) break;
      if (token.kind == detail::XmlPullParser::Kind::StartElement)
        result.own.push_back(parse_rust_node(parser, token, depth + 1));
    }
  }
  return result;
}

std::string rust_uuid_value(const std::string& input) {
  auto id = input;
    if (id.starts_with("urn:uuid:")) id.erase(0, 9);
    else if (id.size() == 38 && id.front() == '{' && id.back() == '}')
      id = id.substr(1, 36);
    std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const bool layout = id.size() == 32 ||
        (id.size() == 36 && id[8] == '-' && id[13] == '-' && id[18] == '-' && id[23] == '-');
    std::erase(id, '-');
    if (layout && id.size() == 32 && std::all_of(id.begin(), id.end(),
        [](unsigned char c) { return std::isxdigit(c) != 0; })) {
      id.insert(20, "-"); id.insert(16, "-"); id.insert(12, "-"); id.insert(8, "-");
    } else id = "00000000-0000-0000-0000-000000000000";
  return id;
}

std::string rust_debug_nodes(const std::vector<RustXesNode>& nodes);
std::string rust_debug_node(const RustXesNode& node) {
  std::string value = "None";
  if (!node.has_value && node.tag != "list" && node.tag != "container") {}
  else if (node.tag == "string") value = "String(" + rust_debug_string(node.value) + ")";
  else if (node.tag == "id") {
    value = "ID(" + rust_uuid_value(node.value) + ")";
  } else if (node.tag == "date") {
    try { value = "Date(" + rust_debug_date(node.value) + ")"; }
    catch (const std::invalid_argument&) { value = "None"; }
  } else if (node.tag == "int") {
    std::int64_t number = 0;
    try { number = std::get<std::int64_t>(parse_native_value(AttributeKind::kInt, node.value, XesImportVariant::kRustXes)); }
    catch (const std::invalid_argument&) {}
    value = "Int(" + std::to_string(number) + ")";
  } else if (node.tag == "float") {
    const double number = rust_float_value(node.value);
    value = "Float(" + rust_debug_float(number) + ")";
  }
  else if (node.tag == "boolean") value = node.value == "true" ? "Boolean(true)" : "Boolean(false)";
  else if (node.tag == "list" || node.tag == "container") value = (node.tag == "list" ? "List(" : "Container(") + rust_debug_nodes(node.contents) + ")";
  return "Attribute { key: " + rust_debug_string(node.key) + ", value: " + value +
      ", own_attributes: " + (node.own.empty() ? "None" : "Some(" + rust_debug_nodes(node.own) + ")") + " }";
}
std::string rust_debug_nodes(const std::vector<RustXesNode>& nodes) {
  std::string result = "[";
  for (const auto& node : nodes) {
    if (result.size() != 1) result += ", ";
    result += rust_debug_node(node);
  }
  return result + ']';
}

std::optional<EventAttribute> parse_attribute_node(
    detail::XmlPullParser& parser,
    const detail::XmlPullParser::Token& start,
    XesImportVariant variant,
    std::size_t depth = 0) {
  constexpr std::size_t kMaxAttributeDepth = 128;
  if (depth > kMaxAttributeDepth)
    throw std::invalid_argument("XES attribute nesting exceeds the maximum depth");
  const auto element = local_name(start.name);
  const auto kind = attribute_kind_of(element);
  const bool list = element == "list";
  const bool container = element == "container";
  if (variant == XesImportVariant::kRustXes && (list || container)) {
    const auto node = parse_rust_node(parser, start, depth);
    return EventAttribute{node.key, rust_debug_nodes(node.contents), AttributeKind::kString};
  }

  if (!kind && !list && !container) {
    skip_element(parser, start);
    if (variant == XesImportVariant::kRustXes)
      if (const auto* key = detail::attribute(start.attributes, "key"))
        return EventAttribute{*key, "", AttributeKind::kString, true};
    return std::nullopt;
  }

  const std::string empty_key;
  const auto* key = detail::attribute(start.attributes, "key");
  if (!key && variant == XesImportVariant::kRustXes) key = &empty_key;
  if (!key) {
    skip_element(parser, start);
    return std::nullopt;
  }
  if (variant == XesImportVariant::kRustXes && kind) {
    const auto* raw = detail::attribute(start.attributes, "value");
    skip_element(parser, start);
    if (!raw) return EventAttribute{*key, "", *kind, true};
    EventAttribute result{*key, *raw, *kind};
    if (*kind == AttributeKind::kInt) {
      std::int64_t value = 0;
      try { value = std::get<std::int64_t>(parse_native_value(*kind, *raw, variant)); }
      catch (const std::invalid_argument&) {}
      result.typed_value = value;
      result.value = std::to_string(value);
    } else if (*kind == AttributeKind::kFloat) {
      const double value = rust_float_value(*raw);

      result.typed_value = value;
      result.value = rust_debug_float(value);
    } else if (*kind == AttributeKind::kDate) {
      try {
        auto date = rust_debug_date(*raw);
        if (date.substr(17, 2) == "60") {
          // Scalar table dates carry the leap second into the next instant;
          // the container debug representation deliberately retains :60.
          date.replace(17, 2, "59");
          const auto dot = date.find('.');
          std::string fraction;
          if (dot != std::string::npos) {
            const auto end = date.find_first_not_of("0123456789", dot + 1);
            fraction = date.substr(dot, end - dot);
            date.erase(dot, end - dot);
          }
          date = format_timestamp(*parse_timestamp(date) + 1);
          if (!fraction.empty()) date.replace(date.find('.'), 4, fraction);
        }
        result.typed_value = parse_native_value(*kind, date, variant);
        result.value = std::get<NativeDate>(result.typed_value).value;
      } catch (const std::invalid_argument&) { result.is_null = true; }
    } else if (*kind == AttributeKind::kId) {
      result.kind = AttributeKind::kString;
      result.value = rust_uuid_value(*raw);
    } else result.typed_value = parse_native_value(*kind, *raw, variant);
    return result;
  }
  const bool is_null = true_attribute(start.attributes, "null") ||
                       true_attribute(start.attributes, "is_null");
  EventAttribute result;
  result.key = *key;
  result.container = list ? AttributeContainerKind::kList
                          : container ? AttributeContainerKind::kContainer
                                      : AttributeContainerKind::kScalar;
  result.kind = kind.value_or(AttributeKind::kString);
  result.is_null = is_null;
  if (const auto* value = detail::attribute(start.attributes, "value")) {
    result.value = *value;
  } else if (!is_null && kind) {
    throw std::invalid_argument("XES scalar attribute is missing its value");
  }
  if (!is_null && kind) {
    result.typed_value = parse_native_value(*kind, result.value, variant);
    if (const auto* date = std::get_if<NativeDate>(&result.typed_value))
      result.value = date->value;
  }
  if (start.self_closing) return result;
  if (kind && (variant == XesImportVariant::kChunkRegex ||
               variant == XesImportVariant::kRustXes)) {
    skip_element(parser, start);
    return result;
  }
  if (list && variant != XesImportVariant::kIterparse20 &&
      variant != XesImportVariant::kRustXes)
    result.container = AttributeContainerKind::kContainer;

  while (true) {
    const auto token = parser.next();
    if (token.kind == detail::XmlPullParser::Kind::EndOfInput) {
      throw std::runtime_error("XES attribute element is not closed");
    }
    if (token.kind == detail::XmlPullParser::Kind::EndElement) {
      if (local_name(token.name) == element) return result;
      continue;
    }
    if (token.kind != detail::XmlPullParser::Kind::StartElement) continue;
    if (kind && local_name(token.name) == "values" &&
        (variant == XesImportVariant::kIterparse ||
         variant == XesImportVariant::kIterparseMemoryCompressed ||
         variant == XesImportVariant::kIterparse20)) {
      if (result.scalar_wrapper || !result.children.empty()) {
        skip_element(parser, token);
        continue;
      }
      result.scalar_wrapper = true;
      if (variant == XesImportVariant::kIterparse20) {
        skip_element(parser, token);
        continue;
      }
      result.scalar_children_list = true;
      if (token.self_closing) continue;
      while (true) {
        const auto child_token = parser.next();
        if (child_token.kind == detail::XmlPullParser::Kind::EndOfInput)
          throw std::runtime_error("XES scalar <values> element is not closed");
        if (child_token.kind == detail::XmlPullParser::Kind::EndElement &&
            local_name(child_token.name) == "values") break;
        if (child_token.kind == detail::XmlPullParser::Kind::StartElement)
          if (const auto child = parse_attribute_node(parser, child_token, variant, depth + 1))
            result.children.push_back(*child);
      }
      continue;
    }
    if (!kind) {
      // Wrapped and direct list entries have different result shapes across
      // the parser variations, even when their scalar contents are identical.
      if (list && local_name(token.name) == "values") {
        if (variant == XesImportVariant::kIterparse20) {
          skip_element(parser, token);
          continue;
        }
        if (variant != XesImportVariant::kChunkRegex)
          result.container = AttributeContainerKind::kList;
        if (token.self_closing) continue;
        while (true) {
          const auto values_token = parser.next();
          if (values_token.kind == detail::XmlPullParser::Kind::EndOfInput)
            throw std::runtime_error("XES list <values> element is not closed");
          if (values_token.kind == detail::XmlPullParser::Kind::EndElement) {
            if (local_name(values_token.name) == "values") break;
            continue;
          }
          if (values_token.kind != detail::XmlPullParser::Kind::StartElement)
            continue;
          if (const auto child =
                  parse_attribute_node(parser, values_token, variant, depth + 1))
            result.children.push_back(*child);
        }
      } else if (const auto child =
                     parse_attribute_node(parser, token, variant, depth + 1)) {
        result.children.push_back(*child);
      }
    } else if (const auto child =
                   parse_attribute_node(parser, token, variant, depth + 1)) {
      // Scalar attributes are normally self-closing, but the attribute
      // representation also permits recursive metadata children. Preserve
      // those children instead of discarding the whole nested subtree.
      result.children.push_back(*child);
    }
  }
}

void replace_attribute(std::vector<EventAttribute>& attributes,
                       EventAttribute attribute, bool overwrite = true) {
  const auto existing = std::find_if(attributes.begin(), attributes.end(),
      [&](const auto& item) { return item.key == attribute.key; });
  if (existing == attributes.end()) attributes.push_back(std::move(attribute));
  else if (overwrite) *existing = std::move(attribute);
}

template <typename Emit>
void emit_imported_attribute(EventAttribute attribute, XesImportVariant variant,
                             const Emit& emit) {
  if (variant != XesImportVariant::kLineByLine) {
    emit(std::move(attribute));
    return;
  }
  auto children = std::move(attribute.children);
  attribute.children.clear();
  attribute.scalar_wrapper = false;
  attribute.scalar_children_list = false;
  if (attribute.container == AttributeContainerKind::kScalar)
    emit(std::move(attribute));
  for (auto& child : children)
    emit_imported_attribute(std::move(child), variant, emit);
}

int selector_match_rank(std::string_view key, std::string_view selector) {
  // The configured spelling is authoritative; its short/qualified alternate
  // is only a fallback when the authoritative key is absent.
  if (key == selector) return 2;
  if (selector == "activity" || selector == "concept:name")
    return key == "activity" || key == "concept:name" ? 1 : 0;
  if (selector == "resource" || selector == "org:resource")
    return key == "resource" || key == "org:resource" ? 1 : 0;
  if (selector == "timestamp" || selector == "time:timestamp")
    return key == "timestamp" || key == "time:timestamp" ? 1 : 0;
  if (selector == "lifecycle" || selector == "lifecycle:transition")
    return key == "lifecycle" || key == "lifecycle:transition" ? 1 : 0;
  return 0;
}

int case_selector_match_rank(std::string_view key, std::string_view selector) {
  if (key == selector) return 2;
  if (selector == "concept:name" || selector == "case_id" ||
      selector == "case:concept:name") {
    return key == "concept:name" || key == "case_id" ||
                   key == "case:concept:name"
               ? 1
               : 0;
  }
  return 0;
}

// Classifier keys are conventionally whitespace-separated, with single
// quotes around keys that contain whitespace.  Accept both quote styles and
// a backslash escape so classifier declarations survive a complete round
// trip even when a key itself contains a quote.
std::vector<std::string> tokenize_classifier_keys(std::string_view text) {
  std::vector<std::string> keys;
  std::size_t position = 0;
  while (position < text.size()) {
    while (position < text.size() &&
           std::isspace(static_cast<unsigned char>(text[position]))) {
      ++position;
    }
    if (position == text.size()) break;

    std::string key;
    const char quote = text[position] == '\'' || text[position] == '"'
                           ? text[position]
                           : '\0';
    if (quote != '\0') ++position;
    bool closed = quote == '\0';
    while (position < text.size()) {
      const char current = text[position++];
      if (current == '\\' && position < text.size()) {
        // Escapes are useful for quoted keys and harmless for an unquoted
        // key; retain the escaped character without its quoting backslash.
        key.push_back(text[position++]);
        continue;
      }
      if (quote != '\0' && current == quote) {
        closed = true;
        break;
      }
      if (quote == '\0' &&
          std::isspace(static_cast<unsigned char>(current))) {
        break;
      }
      key.push_back(current);
    }
    // An unterminated quote is kept as a best-effort token.  This mirrors the
    // permissive handling of other unknown XES metadata while never dropping
    // a classifier key from the imported document.
    (void)closed;
    keys.push_back(std::move(key));
  }
  return keys;
}

void validate_variant(XesImportVariant variant) {
  switch (variant) {
    case XesImportVariant::kIterparse:
    case XesImportVariant::kLineByLine:
    case XesImportVariant::kIterparseMemoryCompressed:
    case XesImportVariant::kIterparse20:
    case XesImportVariant::kChunkRegex:
    case XesImportVariant::kRustXes:
      return;
  }
  throw std::invalid_argument("XES import: unsupported parser variant");
}

// LINE_BY_LINE is a lexical reader. It considers one physical LF-delimited
// line at a time, takes the final tag spelling, and extracts two double-quoted
// fields by position. Normalize that flat stream for the shared projection code.
std::string line_by_line_xml(std::string_view source, std::size_t max_traces) {
  std::string result = "<log>";
  std::string trace, event;
  bool in_trace = false, in_event = false;
  std::size_t traces = 0;
  const auto escaped = [](std::string_view value) {
    std::string out;
    for (const auto ch : value) {
      if (ch == '&') out += "&amp;";
      else if (ch == '<') out += "&lt;";
      else if (ch == '>') out += "&gt;";
      else if (ch == '"') out += "&quot;";
      else out += ch;
    }
    return out;
  };
  while (!source.empty() && traces < max_traces) {
    const auto end = source.find('\n');
    const auto line = source.substr(0, end);
    source = end == std::string_view::npos ? std::string_view{} : source.substr(end + 1);
    const auto open = line.rfind('<');
    if (open == std::string_view::npos) continue;
    const auto tag_text = line.substr(open + 1);
    const auto tag = tag_text.substr(0, tag_text.find_first_of(" >\t\r"));
    std::array<std::size_t, 4> quotes{};
    std::size_t quote_count = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
      if (line[i] != '"') continue;
      if (quote_count == quotes.size()) { ++quote_count; break; }
      quotes[quote_count++] = i;
    }
    if (quote_count == 4) {
      if (!in_trace) continue;
      const auto scalar_open = line.rfind('<', quotes[0]);
      const auto scalar_text = line.substr(scalar_open + 1);
      const auto scalar_tag = scalar_text.substr(0, scalar_text.find_first_of(" >\t\r"));
      auto kind = attribute_kind_of(scalar_tag);
      const auto type = kind ? std::string(scalar_tag) : std::string("string");
      const auto key = line.substr(quotes[0] + 1, quotes[1] - quotes[0] - 1);
      const auto value = line.substr(quotes[2] + 1, quotes[3] - quotes[2] - 1);
      // Conversion errors occur while reading the line, even if the trace
      // is never closed and therefore never appended to the result.
      if (kind) (void)parse_native_value(*kind, std::string(value), XesImportVariant::kLineByLine);
      (in_event ? event : trace) += "<" + type + " key=\"" + escaped(key) +
          "\" value=\"" + escaped(value) + "\"/>";
      continue;
    }
    if (quote_count != 0) continue;
    if (in_event) {
      if (tag == "/event") {
        trace += "<event>" + event + "</event>";
        event.clear();
        in_event = false;
      }
    } else if (in_trace) {
      if (tag == "event" || tag == "event/") in_event = true;
      else if (tag == "/trace") {
        result += "<trace>" + trace + "</trace>";
        trace.clear();
        in_trace = false;
        ++traces;
      }
    } else if (tag == "trace" || tag == "trace/") in_trace = true;
  }
  return result + "</log>";
}

using TimestampOrderKey = std::pair<std::int64_t, std::int64_t>;

TimestampOrderKey required_timestamp(const Event& event) {
  if (event.timestamp.empty()) {
    throw std::runtime_error(
        "XES timestamp sorting requires a timestamp on every event");
  }
  const auto parsed = parse_timestamp(event.timestamp);
  if (!parsed) {
    throw std::runtime_error(
        "XES timestamp sorting encountered an invalid timestamp");
  }
  auto integral = event.timestamp;
  std::int64_t fraction = 0;
  if (const auto dot = integral.find('.'); dot != std::string::npos) {
    auto end = dot + 1;
    unsigned digits = 0;
    while (end < integral.size() && std::isdigit(static_cast<unsigned char>(integral[end]))) {
      if (digits++ < 9) fraction = fraction * 10 + integral[end] - '0';
      ++end;
    }
    while (digits++ < 9) fraction *= 10;
    integral.erase(dot, end - dot);
  }
  return {static_cast<std::int64_t>(*parse_timestamp(integral)), fraction};
}

void sort_log_by_timestamp(EventLog& log, bool reverse) {
  std::vector<std::pair<TimestampOrderKey, Trace>> timed_traces;
  timed_traces.reserve(log.traces.size());
  for (auto& trace : log.traces) {
    if (trace.events.empty()) continue;
    std::vector<std::pair<TimestampOrderKey, Event>> timed_events;
    timed_events.reserve(trace.events.size());
    for (auto& event : trace.events) {
      const auto timestamp = required_timestamp(event);
      timed_events.emplace_back(timestamp, std::move(event));
    }
    std::stable_sort(timed_events.begin(), timed_events.end(),
                     [reverse](const auto& lhs, const auto& rhs) {
                       return reverse ? lhs.first > rhs.first
                                      : lhs.first < rhs.first;
                     });
    trace.events.clear();
    trace.events.reserve(timed_events.size());
    for (auto& entry : timed_events)
      trace.events.push_back(std::move(entry.second));
    timed_traces.emplace_back(timed_events.front().first, std::move(trace));
  }
  std::stable_sort(timed_traces.begin(), timed_traces.end(),
                   [reverse](const auto& lhs, const auto& rhs) {
                     return reverse ? lhs.first > rhs.first
                                    : lhs.first < rhs.first;
                   });
  log.traces.clear();
  log.traces.reserve(timed_traces.size());
  for (auto& entry : timed_traces)
    log.traces.push_back(std::move(entry.second));
}

}  // namespace

XesImportVariant parse_xes_import_variant(std::string_view value) {
  std::string normalized(value);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  if (normalized == "iterparse" || normalized == "lxml")
    return XesImportVariant::kIterparse;
  if (normalized == "line_by_line" || normalized == "nonstandard")
    return XesImportVariant::kLineByLine;
  if (normalized == "iterparse_mem_compressed")
    return XesImportVariant::kIterparseMemoryCompressed;
  if (normalized == "iterparse_20") return XesImportVariant::kIterparse20;
  if (normalized == "chunk_regex") return XesImportVariant::kChunkRegex;
  if (normalized == "rustxes" || normalized == "r4pm")
    return XesImportVariant::kRustXes;
  throw std::invalid_argument(
      "XES import variant must be iterparse, line_by_line, "
      "iterparse_mem_compressed, iterparse_20, chunk_regex, or rustxes");
}

std::string_view xes_import_variant_name(XesImportVariant variant) {
  switch (variant) {
    case XesImportVariant::kIterparse: return "iterparse";
    case XesImportVariant::kLineByLine: return "line_by_line";
    case XesImportVariant::kIterparseMemoryCompressed:
      return "iterparse_mem_compressed";
    case XesImportVariant::kIterparse20: return "iterparse_20";
    case XesImportVariant::kChunkRegex: return "chunk_regex";
    case XesImportVariant::kRustXes: return "rustxes";
  }
  throw std::invalid_argument("XES import: unsupported parser variant");
}

static EventLog import_xes_impl(
    std::istream& input,
    const XesImportOptions& options, XesImportResult* raw_result = nullptr) {
  validate_variant(options.variant);
  const auto apply_extension_override = [&](EventLog& output) {
    if (!options.conversion_extensions) return;
    output.metadata.extensions.clear();
    std::unordered_map<std::string, std::size_t> indices;
    for (const auto& extension : *options.conversion_extensions) {
      const auto [at, inserted] = indices.emplace(extension.name, output.metadata.extensions.size());
      if (inserted) output.metadata.extensions.push_back(extension);
      else output.metadata.extensions[at->second] = extension;
    }
  };
  if (options.variant != XesImportVariant::RUSTXES &&
      (options.case_attribute.empty() || options.activity_attribute.empty() ||
      options.timestamp_attribute.empty() || options.resource_attribute.empty() ||
      options.lifecycle_attribute.empty())) {
    throw std::invalid_argument("XES import selectors must not be empty");
  }
  std::unordered_set<std::string> attributes_to_read(
      options.attributes_to_read.begin(), options.attributes_to_read.end());

  std::string xml((std::istreambuf_iterator<char>(input)),
                  std::istreambuf_iterator<char>());
  if (detail::is_gzip_data(xml)) {
    xml = detail::gzip_decompress(xml, options.max_decompressed_bytes);
  } else if (xml.size() > options.max_decompressed_bytes) {
    throw std::runtime_error("XES input exceeds the configured size limit");
  }

  // Strip a leading UTF-8 byte-order mark so the first '<' is found cleanly.
  if (xml.size() >= 3 && static_cast<unsigned char>(xml[0]) == 0xEF &&
      static_cast<unsigned char>(xml[1]) == 0xBB &&
      static_cast<unsigned char>(xml[2]) == 0xBF) {
    xml.erase(0, 3);
  }

  if (options.variant == XesImportVariant::kLineByLine)
    xml = line_by_line_xml(xml, options.max_traces);
  detail::XmlPullParser parser(xml);
  EventLog log;
  std::vector<std::string> stack;

  Trace current_trace;
  bool in_trace = false;
  std::size_t case_match_rank = 0;
  LogGlobal* active_global = nullptr;

  Event current_event;
  bool in_event = false;
  int activity_match_rank = 0;
  int timestamp_match_rank = 0;
  int resource_match_rank = 0;
  int lifecycle_match_rank = 0;

  std::size_t input_order = 0;
  bool saw_timestamp = false;

  const auto erase_selected = [](std::vector<EventAttribute>& attributes,
                                 std::string_view key) {
    if (key.empty()) return;
    const auto found = std::find_if(
        attributes.begin(), attributes.end(),
        [&](const auto& attribute) { return attribute.key == key; });
    if (found != attributes.end() && found->children.empty() && !found->scalar_wrapper)
      attributes.erase(found);
  };

  const auto apply_event_attribute = [&](EventAttribute attribute) {
    const auto key = std::string_view(attribute.key);
    const auto set_first_class = [&](int rank, int& current_rank,
                                     std::string& target,
                                     FirstClassAttributeState& state,
                                     std::string& source_key,
                                     std::optional<EventAttribute>& metadata) {
      if (rank == 0 || rank < current_rank ||
          (rank == current_rank && options.variant == XesImportVariant::kRustXes)) return;
      current_rank = rank;
      target = attribute.value;
      state = attribute.is_null ? FirstClassAttributeState::kNull
                                : FirstClassAttributeState::kPresent;
      source_key = attribute.key;
      metadata = attribute;
    };
    const auto activity_rank = selector_match_rank(
        key, options.activity_attribute);
    const auto timestamp_rank = selector_match_rank(
        key, options.timestamp_attribute);
    const auto resource_rank = selector_match_rank(
        key, options.resource_attribute);
    const auto lifecycle_rank = selector_match_rank(
        key, options.lifecycle_attribute);
    const bool scalar = attribute.container == AttributeContainerKind::kScalar;
    if (scalar) {
      set_first_class(activity_rank, activity_match_rank,
                      current_event.activity, current_event.activity_state,
                      current_event.activity_source_key,
                      current_event.activity_attribute);
      set_first_class(timestamp_rank, timestamp_match_rank,
                      current_event.timestamp, current_event.timestamp_state,
                      current_event.timestamp_source_key,
                      current_event.timestamp_attribute);
      set_first_class(resource_rank, resource_match_rank,
                      current_event.resource, current_event.resource_state,
                      current_event.resource_source_key,
                      current_event.resource_attribute);
      set_first_class(lifecycle_rank, lifecycle_match_rank,
                      current_event.lifecycle, current_event.lifecycle_state,
                      current_event.lifecycle_source_key,
                      current_event.lifecycle_attribute);
      if (timestamp_rank != 0) saw_timestamp = true;
    }
    // Keep aliases and conflicting canonical keys.  The selected key is
    // removed once the complete event has been read, after selector ranking is
    // known.
    if (attributes_to_read.empty() || attributes_to_read.contains(attribute.key) ||
        activity_rank != 0 || timestamp_rank != 0 || resource_rank != 0 ||
        lifecycle_rank != 0) {
      replace_attribute(current_event.attributes, std::move(attribute),
                        options.variant != XesImportVariant::kRustXes);
    }
  };

  const auto finish_event = [&]() {
    erase_selected(current_event.attributes, current_event.activity_source_key);
    erase_selected(current_event.attributes, current_event.timestamp_source_key);
    erase_selected(current_event.attributes, current_event.resource_source_key);
    erase_selected(current_event.attributes, current_event.lifecycle_source_key);
    current_event.input_order = input_order++;
    current_trace.events.push_back(std::move(current_event));
    current_event = Event{};
    in_event = false;
    activity_match_rank = 0;
    timestamp_match_rank = 0;
    resource_match_rank = 0;
    lifecycle_match_rank = 0;
  };

  const auto finish_trace = [&]() {
    erase_selected(current_trace.attributes, current_trace.case_id_source_key);
    for (auto& event : current_trace.events) {
      event.case_id = current_trace.case_id;
      event.case_id_state = current_trace.case_id_state;
      event.case_id_source_key = current_trace.case_id_source_key;
    }
    log.traces.push_back(std::move(current_trace));
    current_trace = Trace{};
    case_match_rank = 0;
    in_trace = false;
  };

  while (true) {
    auto token = parser.next();
    if (token.kind == detail::XmlPullParser::Kind::EndOfInput) {
      break;
    }

    if (token.kind == detail::XmlPullParser::Kind::StartElement) {
      const std::string parent =
           stack.empty() ? std::string{} : std::string(local_name(stack.back()));
      const std::string element = std::string(local_name(token.name));
      bool consumed_by_parser = false;

      if (element == "trace" && parent == "log") {
        if (log.traces.size() >= options.max_traces) break;
        if (options.variant == XesImportVariant::kRustXes && token.self_closing && log.traces.empty())
          throw std::invalid_argument("RUSTXES: Failed to parse XES: MissingLastTrace");
        if (in_trace) {
          throw std::runtime_error("XES input nests a <trace> inside a <trace>");
        }
        in_trace = true;
      } else if (element == "event" && in_trace && parent == "trace") {
        if (options.variant == XesImportVariant::kRustXes && token.self_closing)
          throw std::invalid_argument("RUSTXES: Empty event not handled?!");
        in_event = true;
      } else if (!in_trace && parent == "log" && element == "extension") {
        // Keep declarations in document order.  Missing optional values are
        // represented as empty strings so a permissive read does not discard
        // a declaration from an otherwise usable log.
        LogExtension extension;
        if (const auto* value = detail::attribute(token.attributes, "name"))
          extension.name = *value;
        if (const auto* value = detail::attribute(token.attributes, "prefix"))
          extension.prefix = *value;
        if (const auto* value = detail::attribute(token.attributes, "uri"))
          extension.uri = *value;
        log.metadata.extensions.push_back(std::move(extension));
      } else if (!in_trace && parent == "log" && element == "classifier") {
        LogClassifier classifier;
        if (const auto* value = detail::attribute(token.attributes, "name"))
          classifier.name = *value;
        if (const auto* value = detail::attribute(token.attributes, "keys"))
          classifier.keys = tokenize_classifier_keys(*value);
        log.metadata.classifiers.push_back(std::move(classifier));
      } else if (!in_trace && parent == "log" && element == "global") {
        LogGlobal global;
        if (const auto* scope = detail::attribute(token.attributes, "scope"))
          global.scope = *scope;
        log.metadata.globals.push_back(std::move(global));
        active_global = &log.metadata.globals.back();
      } else if (active_global && parent == "global") {
        consumed_by_parser = true;
        if (const auto attribute = parse_attribute_node(parser, token, options.variant)) {
          if (attributes_to_read.empty() ||
              attributes_to_read.contains(attribute->key)) {
            active_global->attributes.push_back(*attribute);
            if (active_global->scope == "trace")
              log.metadata.trace_globals.push_back(*attribute);
            if (active_global->scope == "event")
              log.metadata.event_globals.push_back(*attribute);
          }
        }
      } else if (in_event && parent == "event") {
        consumed_by_parser = true;
        if (const auto attribute = parse_attribute_node(parser, token, options.variant))
          emit_imported_attribute(*attribute, options.variant, apply_event_attribute);
      } else if (in_trace && !in_event && parent == "trace") {
        consumed_by_parser = true;
        if (const auto attribute = parse_attribute_node(parser, token, options.variant)) {
          emit_imported_attribute(*attribute, options.variant, [&](EventAttribute emitted) {
            const auto rank = emitted.container == AttributeContainerKind::kScalar
                ? case_selector_match_rank(emitted.key, options.case_attribute) : 0;
            if (rank != 0 && (rank > static_cast<int>(case_match_rank) ||
                (rank == static_cast<int>(case_match_rank) &&
                 options.variant != XesImportVariant::kRustXes))) {
              case_match_rank = static_cast<std::size_t>(rank);
              current_trace.case_id = emitted.value;
              current_trace.case_id_state = emitted.is_null ? FirstClassAttributeState::kNull
                                                            : FirstClassAttributeState::kPresent;
              current_trace.case_id_source_key = emitted.key;
              current_trace.case_id_attribute = emitted;
            }
            if (attributes_to_read.empty() || attributes_to_read.contains(emitted.key) || rank != 0)
              replace_attribute(current_trace.attributes, std::move(emitted),
                                options.variant != XesImportVariant::kRustXes);
          });
        }
      } else if (!in_trace && parent == "log") {
        consumed_by_parser = true;
        if (const auto attribute = parse_attribute_node(parser, token, options.variant))
          emit_imported_attribute(*attribute, options.variant, [&](EventAttribute emitted) {
            if (attributes_to_read.empty() || attributes_to_read.contains(emitted.key))
              replace_attribute(log.attributes, std::move(emitted),
                                options.variant != XesImportVariant::kRustXes);
          });
      }

      if (token.self_closing) {
        // A self-closing trace/event still opens and closes here.
        if (element == "event" && in_event && parent == "trace") {
          finish_event();
        } else if (element == "trace" && in_trace && parent == "log") {
          finish_trace();
        } else if (element == "global") {
          active_global = nullptr;
        }
      } else if (!consumed_by_parser) {
        stack.push_back(token.name);
      }
      continue;
    }

    // EndElement: unwind the stack to the matching open element. Tolerates
    // minor mismatches by popping until the name is found.
    while (!stack.empty() && local_name(stack.back()) != local_name(token.name)) {
      stack.pop_back();
    }
    if (!stack.empty()) {
      stack.pop_back();
    }
    const auto element = local_name(token.name);
    if (element == "global") {
      active_global = nullptr;
    }
    if (element == "event" && in_event) {
      finish_event();
    } else if (element == "trace" && in_trace) {
      finish_trace();
    }
  }

  if (options.require_timestamp && !saw_timestamp) {
    throw std::runtime_error(
        "XES log has no timestamps (attribute '" +
        options.timestamp_attribute + "')");
  }

  if (options.variant == XesImportVariant::kRustXes) {
    // Event columns with the table's reserved case prefix are discarded;
    // they neither supply a grouping ID nor override trace attributes.
    // Table column order is shared by auto-generated event names and trace
    // reconstruction. Include null-only columns and empty source traces.
    std::vector<std::string> table_columns;
    std::unordered_set<std::string> known_columns;
    const auto add_column = [&](const std::string& key) {
      if (known_columns.insert(key).second) table_columns.push_back(key);
    };
    for (const auto& trace : log.traces) {
      if (trace.case_id_attribute) add_column("case:" + trace.case_id_attribute->key);
      for (const auto& attribute : trace.attributes) add_column("case:" + attribute.key);
      for (const auto& event : trace.events) {
        for (const auto& attribute : event.attributes) add_column(attribute.key);
        for (const auto* selected : {&event.activity_attribute, &event.timestamp_attribute,
                                    &event.resource_attribute, &event.lifecycle_attribute})
          if (*selected) add_column((*selected)->key);
      }
    }
    // A global shuffle preserves observed cross-group correlations, while
    // backend allocation/hash probabilities remain unverified.
    static thread_local std::mt19937_64 column_order_generator(std::random_device{}());
    std::shuffle(table_columns.begin(), table_columns.end(), column_order_generator);
    std::unordered_map<std::string, std::size_t> column_rank;
    for (std::size_t i = 0; i < table_columns.size(); ++i) column_rank.emplace(table_columns[i], i);
    std::string empty_event_name;
    if (const auto empty = column_rank.find(""); empty != column_rank.end()) {
      empty_event_name = "column_" + std::to_string(empty->second);
      if (known_columns.contains(empty_event_name))
        throw std::invalid_argument("RUSTXES: duplicate column " + empty_event_name);
    }
    const auto normalize_rows = [](const std::vector<std::vector<EventAttribute*>>& rows, bool promote_missing = true) {
      std::unordered_map<std::string, std::vector<EventAttribute*>> columns;
      std::unordered_map<std::string, std::size_t> present_rows;
      for (const auto& row : rows) {
        std::unordered_set<std::string> seen;
        for (auto* cell : row) {
          columns[cell->key].push_back(cell);
          if (!cell->is_null && seen.insert(cell->key).second) ++present_rows[cell->key];
        }
      }
      for (const auto& [key, cells] : columns) {
        (void)key;
        std::set<AttributeKind> kinds;
        const auto present = present_rows[key];
        for (const auto* cell : cells) {
          if (cell->is_null) continue;
          kinds.insert(cell->kind);
        }
        const bool numeric = std::all_of(kinds.begin(), kinds.end(), [](auto kind) {
          return kind == AttributeKind::kInt || kind == AttributeKind::kFloat;
        });
        for (auto* cell : cells) {
          if (cell->is_null) continue;
          if (numeric && cell->kind == AttributeKind::kInt &&
              (kinds.contains(AttributeKind::kFloat) || (promote_missing && present < rows.size()))) {
            const auto value = static_cast<double>(std::get<std::int64_t>(cell->typed_value));
            cell->kind = AttributeKind::kFloat;
            cell->typed_value = value;
            cell->value = rust_debug_float(value);
          } else if (!numeric && kinds.size() > 1) {
            std::string text = cell->value;
            if (cell->kind == AttributeKind::kString || cell->kind == AttributeKind::kId)
              text = '"' + text + '"';
            else if (cell->kind == AttributeKind::kBoolean)
              text = std::get<bool>(cell->typed_value) ? "true" : "false";
            else if (cell->kind == AttributeKind::kFloat)
              text = rust_mixed_float(std::get<double>(cell->typed_value));
            else if (cell->kind == AttributeKind::kDate) {
              text = std::get<NativeDate>(cell->typed_value).value;
              if (text.ends_with('Z')) text.pop_back();
              if (text.size() > 10) text[10] = ' ';
              const auto dot = text.find('.');
              if (dot != std::string::npos) {
                if (text.find_first_not_of('0', dot + 1) == std::string::npos) text.erase(dot);
                else text.resize(dot + 7, '0');
              }
            }
            cell->kind = AttributeKind::kString;
            cell->value = std::move(text);
            cell->typed_value = std::monostate{};
          }
        }
      }
    };
    const bool custom_conversion = !raw_result &&
        (options.conversion_case_id_key != std::optional<std::string>{"case:concept:name"} ||
         options.conversion_case_attribute_prefix != std::optional<std::string>{"case:"});
    XesImportResult converted_result;
    auto* table_result = raw_result ? raw_result : &converted_result;
    if (raw_result || custom_conversion) {
      EventTable table;
      std::vector<std::optional<std::string>> case_projection_keys;
      table.columns = table_columns;
      for (auto& column : table.columns) if (column.empty()) column = empty_event_name;
      for (const auto& trace : log.traces) {
        for (const auto& event : trace.events) {
          EventTable::Row row(table.columns.size());
          const auto set = [&](const EventAttribute& source, bool trace_scope) {
            const auto key = trace_scope ? "case:" + source.key : source.key;
            if (!trace_scope && key.starts_with("case:")) return;
            auto value = source;
            value.key = key.empty() ? empty_event_name : key;
            row[column_rank.at(key)] = std::move(value);
          };
          for (const auto& value : trace.attributes) set(value, true);
          if (trace.case_id_attribute) set(*trace.case_id_attribute, true);
          for (const auto& value : event.attributes) set(value, false);
          for (const auto* selected : {&event.activity_attribute, &event.timestamp_attribute,
                                      &event.resource_attribute, &event.lifecycle_attribute})
            if (*selected) set(**selected, false);
          table.rows.push_back(std::move(row));
          case_projection_keys.push_back(trace.case_id_attribute ? std::optional<std::string>{trace.case_id_attribute->key} : std::nullopt);
        }
      }
      std::vector<std::vector<EventAttribute*>> rows;
      for (auto& row : table.rows) {
        auto& cells = rows.emplace_back();
        for (auto& cell : row) if (cell) cells.push_back(&*cell);
      }
      const bool lazy = raw_result && options.return_pl_lazyframe;
      normalize_rows(rows, !lazy);
      for (std::size_t column = 0; column < table.columns.size(); ++column) {
        std::optional<AttributeKind> kind;
        std::size_t present = 0;
        for (const auto& row : table.rows)
          if (row[column] && !row[column]->is_null) {
            kind = row[column]->kind;
            ++present;
          }
        std::string dtype = lazy ? "Null" : "object";
        if (kind) switch (*kind) {
          case AttributeKind::kInt: dtype = lazy ? "Int64" : "int64"; break;
          case AttributeKind::kFloat: dtype = lazy ? "Float64" : "float64"; break;
          case AttributeKind::kBoolean:
            dtype = lazy ? "Boolean" : present == table.rows.size() ? "bool" : "object"; break;
          case AttributeKind::kDate:
            dtype = lazy ? "Datetime(time_unit='us', time_zone='UTC')" : "datetime64[us, UTC]"; break;
          default: dtype = lazy ? "String" : "str"; break;
        }
        table_result->dtypes.push_back(std::move(dtype));
        for (auto& row : table.rows) {
          auto& cell = row[column];
          if (cell && !cell->is_null) continue;
          cell.reset();
          if (!lazy && (kind == AttributeKind::kFloat || kind == AttributeKind::kString || kind == AttributeKind::kId)) {
            cell = EventAttribute{table.columns[column], "NaN", AttributeKind::kFloat};
            cell->typed_value = std::numeric_limits<double>::quiet_NaN();
          } else if (!lazy && kind == AttributeKind::kDate) {
            cell = EventAttribute{table.columns[column], "NaT", AttributeKind::kDate};
            cell->typed_value = NativeDate{"NaT"};
          }
        }
      }
      if (custom_conversion) {
        const auto& glue = options.conversion_case_id_key;
        if (!glue && !table.rows.empty()) throw std::invalid_argument("RUSTXES: missing None");
        const auto& prefix = options.conversion_case_attribute_prefix;
        const auto missing = [](const std::optional<EventAttribute>& cell) {
          if (!cell || cell->is_null) return true;
          if (const auto* value = std::get_if<double>(&cell->typed_value)) return std::isnan(*value);
          if (const auto* value = std::get_if<NativeDate>(&cell->typed_value)) return value->value == "NaT";
          return false;
        };
        const auto group_column = std::find(table.columns.begin(), table.columns.end(), glue.value_or(""));
        if (group_column == table.columns.end() && !table.rows.empty())
          throw std::invalid_argument("RUSTXES: missing " + *glue);
        EventLog converted;
        converted.attributes = {{"origin", "csv"}};
        std::map<std::pair<AttributeKind, std::string>, std::size_t> groups;
        const auto project = [](auto& object, const std::string& selector, auto& value, auto& state,
                                auto& source, auto& metadata, bool case_scope = false) {
          auto selected = object.attributes.end();
          int best = 0;
          for (auto at = object.attributes.begin(); at != object.attributes.end(); ++at) {
            const int rank = case_scope ? case_selector_match_rank(at->key, selector) : selector_match_rank(at->key, selector);
            if (rank > best) { best = rank; selected = at; }
          }
          if (selected == object.attributes.end()) return;
          value = selected->value; source = selected->key; metadata = *selected;
          state = FirstClassAttributeState::kPresent;
        };
        for (const auto& row : table.rows) {
          const auto& identifier = row[group_column - table.columns.begin()];
          if (missing(identifier)) throw std::invalid_argument("RUSTXES: missing " + *glue);
          if (!prefix) throw std::invalid_argument("RUSTXES: startswith requires a string prefix");
          std::string key = identifier->value;
          if (identifier->kind == AttributeKind::kFloat) {
            const auto number = std::get<double>(identifier->typed_value);
            key = rust_debug_float(number == 0 ? 0 : number);
          } else if (identifier->kind == AttributeKind::kBoolean) {
            key = std::get<bool>(identifier->typed_value) ? "true" : "false";
          } else if (identifier->kind == AttributeKind::kDate) {
            key = std::get<NativeDate>(identifier->typed_value).value;
            if (key.find('.') != std::string::npos && key.ends_with('Z')) {
              key.pop_back();
              while (key.ends_with('0')) key.pop_back();
              if (key.ends_with('.')) key.pop_back();
              key += 'Z';
            }
          }
          const auto [position, inserted] = groups.emplace(std::pair{identifier->kind, key}, converted.traces.size());
          if (inserted) {
            Trace trace;
            for (std::size_t column = 0; column < table.columns.size(); ++column) {
              if (missing(row[column]) || !table.columns[column].starts_with(*prefix)) continue;
              auto attribute = *row[column];
              if (!prefix->empty()) {
                std::size_t at = 0;
                while ((at = attribute.key.find(*prefix, at)) != std::string::npos) attribute.key.erase(at, prefix->size());
              }
              replace_attribute(trace.attributes, std::move(attribute));
            }
            if (std::none_of(trace.attributes.begin(), trace.attributes.end(), [](const auto& value) { return value.key == "concept:name"; })) {
              auto name = *identifier; name.key = "concept:name";
              trace.attributes.push_back(std::move(name));
            }
            auto case_selector = options.case_attribute;
            bool has_case_projection = true;
            if (case_selector != "concept:name") {
              const auto& original = case_projection_keys[&row - table.rows.data()];
              has_case_projection = original.has_value();
              if (original) {
                case_selector = "case:" + *original;
                has_case_projection = case_selector.starts_with(*prefix);
                if (!prefix->empty()) {
                  std::size_t at = 0;
                  while ((at = case_selector.find(*prefix, at)) != std::string::npos) case_selector.erase(at, prefix->size());
                }
              }
            }
            if (has_case_projection)
              project(trace, case_selector, trace.case_id, trace.case_id_state,
                      trace.case_id_source_key, trace.case_id_attribute, true);
            if (trace.case_id_attribute) std::erase_if(trace.attributes, [&](const auto& attribute) {
              return attribute.key == trace.case_id_attribute->key;
            });
            converted.traces.push_back(std::move(trace));
          }
          Event event;
          event.input_order = static_cast<std::size_t>(&row - table.rows.data());
          for (std::size_t column = 0; column < table.columns.size(); ++column)
            if (!missing(row[column]) && !table.columns[column].starts_with(*prefix)) event.attributes.push_back(*row[column]);
          project(event, options.activity_attribute.empty() ? empty_event_name : options.activity_attribute, event.activity, event.activity_state, event.activity_source_key, event.activity_attribute);
          project(event, options.timestamp_attribute.empty() ? empty_event_name : options.timestamp_attribute, event.timestamp, event.timestamp_state, event.timestamp_source_key, event.timestamp_attribute);
          project(event, options.resource_attribute.empty() ? empty_event_name : options.resource_attribute, event.resource, event.resource_state, event.resource_source_key, event.resource_attribute);
          project(event, options.lifecycle_attribute.empty() ? empty_event_name : options.lifecycle_attribute, event.lifecycle, event.lifecycle_state, event.lifecycle_source_key, event.lifecycle_attribute);
          std::erase_if(event.attributes, [&](const auto& attribute) {
            return (event.activity_attribute && attribute.key == event.activity_attribute->key) ||
                   (event.timestamp_attribute && attribute.key == event.timestamp_attribute->key) ||
                   (event.resource_attribute && attribute.key == event.resource_attribute->key) ||
                   (event.lifecycle_attribute && attribute.key == event.lifecycle_attribute->key);
          });
          auto& trace = converted.traces[position->second];
          copy_event_case_context(trace, event);
          trace.events.push_back(std::move(event));
        }
        std::unordered_set<std::string> extension_prefixes;
        for (auto key : table_columns) {
          std::size_t at = 0;
          while ((at = key.find("case:", at)) != std::string::npos) key.erase(at, 5);
          if (const auto colon = key.find(':'); colon != std::string::npos) extension_prefixes.insert(key.substr(0, colon));
        }
        const std::pair<std::string_view, std::string_view> standards[] = {
            {"concept", "Concept"}, {"time", "Time"}, {"org", "Organizational"},
            {"lifecycle", "Lifecycle"}, {"cost", "Cost"}, {"identity", "Identity"},
            {"artifactlifecycle", "ArtifactLifecycle"}, {"micro", "Micro"},
            {"semantic", "Semantic"}, {"swcomm", "Software Communication"},
            {"swevent", "Software Event"}, {"swtelemetry", "Software Telemetry"}};
        for (const auto& [prefix, name] : standards)
          if (extension_prefixes.contains(std::string(prefix)))
            converted.metadata.extensions.push_back({std::string(name), std::string(prefix),
                "http://www.xes-standard.org/" + std::string(prefix) + ".xesext"});
        apply_extension_override(converted);
        if (options.timestamp_sort) sort_log_by_timestamp(converted, options.reverse_sort);
        return converted;
      }
      // Parsing is eager; the lazy result is a restartable scanner over the
      // immutable in-memory table, not a deferred XML parser.
      if (lazy) {
        table_result->kind = EventDataKind::kLazyEventTable;
        table_result->lazy_table = LazyEventTable::from_table(table);
      } else {
        table_result->kind = EventDataKind::kEventTable;
        table_result->table = std::move(table);
      }
      return {};
    }
    for (auto& trace : log.traces) {
      for (auto& event : trace.events) {
        std::erase_if(event.attributes, [](const EventAttribute& value) {
          return value.key.starts_with("case:");
        });
        for (auto* selected : {&event.activity_attribute, &event.timestamp_attribute,
                              &event.resource_attribute, &event.lifecycle_attribute})
          if (*selected && (*selected)->key.starts_with("case:")) (*selected)->is_null = true;
      }
    }
    // The public adapter materializes a table before rebuilding the EventLog;
    // source declarations and log attributes are not carried through it.
    std::unordered_set<std::string> prefixes;
    for (auto key : table_columns) {
      std::size_t at = 0;
      while ((at = key.find("case:", at)) != std::string::npos) key.erase(at, 5);
      if (const auto colon = key.find(':'); colon != std::string::npos) prefixes.insert(key.substr(0, colon));
    }
    const auto add_key = [&](std::string_view key) {
      const auto colon = key.find(':');
      if (colon != std::string_view::npos) prefixes.emplace(key.substr(0, colon));
    };
    const auto add_trace_key = [&](std::string key) {
      std::size_t at = 0;
      while ((at = key.find("case:", at)) != std::string::npos) key.erase(at, 5);
      add_key(key);
    };
    for (const auto& trace : log.traces) {
      if (trace.case_id_state != FirstClassAttributeState::kMissing || !trace.case_id.empty())
        add_trace_key(trace.case_id_source_key.empty() ? "concept:name" : trace.case_id_source_key);
      for (const auto& attribute : trace.attributes) add_trace_key(attribute.key);
      for (const auto& event : trace.events) {
        for (const auto& attribute : event.attributes) add_key(attribute.key);
        if (event.activity_attribute) add_key(event.activity_attribute->key);
        if (event.timestamp_attribute) add_key(event.timestamp_attribute->key);
        if (event.resource_attribute) add_key(event.resource_attribute->key);
        if (event.lifecycle_attribute) add_key(event.lifecycle_attribute->key);
      }
    }
    std::vector<std::vector<EventAttribute*>> trace_rows, event_rows;
    for (auto& trace : log.traces) {
      if (trace.events.empty()) continue;
      auto& trace_row = trace_rows.emplace_back();
      for (auto& attribute : trace.attributes) trace_row.push_back(&attribute);
      if (trace.case_id_attribute) trace_row.push_back(&*trace.case_id_attribute);
      for (auto& event : trace.events) {
        auto& row = event_rows.emplace_back();
        for (auto& attribute : event.attributes) row.push_back(&attribute);
        for (auto* selected : {&event.activity_attribute, &event.timestamp_attribute,
                              &event.resource_attribute, &event.lifecycle_attribute})
          if (*selected) row.push_back(&**selected);
      }
    }
    normalize_rows(trace_rows);
    normalize_rows(event_rows);
    const auto omitted = [](const EventAttribute& cell) {
      const auto* value = std::get_if<double>(&cell.typed_value);
      return cell.is_null || (value && std::isnan(*value));
    };
    const auto sync_projection = [&](auto& selected, auto& value, auto& state, auto& source) {
      if (!selected) return;
      if (omitted(*selected)) {
        selected.reset(); value.clear(); source.clear();
        state = FirstClassAttributeState::kMissing;
      } else value = selected->value;
    };
    for (auto& trace : log.traces) {
      std::erase_if(trace.attributes, omitted);
      sync_projection(trace.case_id_attribute, trace.case_id, trace.case_id_state, trace.case_id_source_key);
      for (auto& event : trace.events) {
        std::erase_if(event.attributes, omitted);
        sync_projection(event.activity_attribute, event.activity, event.activity_state, event.activity_source_key);
        sync_projection(event.timestamp_attribute, event.timestamp, event.timestamp_state, event.timestamp_source_key);
        sync_projection(event.resource_attribute, event.resource, event.resource_state, event.resource_source_key);
        sync_projection(event.lifecycle_attribute, event.lifecycle, event.lifecycle_state, event.lifecycle_source_key);
        if (!empty_event_name.empty()) {
          for (auto& attribute : event.attributes)
            if (attribute.key.empty()) attribute.key = empty_event_name;
          const auto rename_selected = [&](auto& selected, auto& source) {
            if (selected && selected->key.empty()) {
              selected->key = empty_event_name;
              source = empty_event_name;
            }
          };
          rename_selected(event.activity_attribute, event.activity_source_key);
          rename_selected(event.timestamp_attribute, event.timestamp_source_key);
          rename_selected(event.resource_attribute, event.resource_source_key);
          rename_selected(event.lifecycle_attribute, event.lifecycle_source_key);
        }
        copy_event_case_context(trace, event);
      }
    }
    std::map<std::pair<AttributeKind, std::string>, std::size_t> grouped;
    std::vector<Trace> reconstructed;
    for (auto& trace : log.traces) {
      if (trace.events.empty()) continue;
      const EventAttribute* identifier = nullptr;
      if (trace.case_id_attribute && trace.case_id_attribute->key == "concept:name")
        identifier = &*trace.case_id_attribute;
      else {
        const auto found = std::find_if(trace.attributes.begin(), trace.attributes.end(),
            [](const EventAttribute& value) { return value.key == "concept:name"; });
        if (found != trace.attributes.end()) identifier = &*found;
      }
      if (!identifier || omitted(*identifier))
        throw std::invalid_argument("RUSTXES: missing case:concept:name");
      auto key = identifier->value;
      if (identifier->kind == AttributeKind::kBoolean)
        key = std::get<bool>(identifier->typed_value) ? "true" : "false";
      else if (identifier->kind == AttributeKind::kFloat) {
        const auto value = std::get<double>(identifier->typed_value);
        key = rust_debug_float(value == 0 ? 0 : value);
      } else if (identifier->kind == AttributeKind::kDate) {
        key = std::get<NativeDate>(identifier->typed_value).value;
        const auto dot = key.find('.');
        if (dot != std::string::npos && key.ends_with('Z')) {
          key.pop_back();
          while (key.ends_with('0')) key.pop_back();
          if (key.ends_with('.')) key.pop_back();
          key += 'Z';
        }
      }
      const auto [position, inserted] = grouped.emplace(
          std::pair{identifier->kind, key}, reconstructed.size());
      if (inserted) {
        // The table reconstruction removes every occurrence of the prefix.
        // Colliding output names share the same column priority in every group.
        auto selected_key = trace.case_id_source_key;
        const auto normalize_key = [](std::string key) {
          std::size_t at = 0;
          while ((at = key.find("case:", at)) != std::string::npos) key.erase(at, 5);
          return key;
        };
        selected_key = normalize_key(selected_key);
        std::vector<const EventAttribute*> present;
        if (trace.case_id_attribute) present.push_back(&*trace.case_id_attribute);
        for (const auto& attribute : trace.attributes)
          if (!trace.case_id_attribute || attribute.key != trace.case_id_attribute->key)
            present.push_back(&attribute);
        std::sort(present.begin(), present.end(), [&](const auto* left, const auto* right) {
          return column_rank.at("case:" + left->key) < column_rank.at("case:" + right->key);
        });
        std::vector<EventAttribute> attributes;
        std::unordered_map<std::string, std::size_t> output_index;
        for (const auto* original : present) {
          auto attribute = *original;
          attribute.key = normalize_key(attribute.key);
          const auto [entry, fresh] = output_index.emplace(attribute.key, attributes.size());
          if (fresh) attributes.push_back(std::move(attribute));
          else attributes[entry->second] = std::move(attribute);
        }
        if (const auto named = output_index.find(selected_key);
            trace.case_id_attribute && named != output_index.end()) {
          const auto& selected = attributes[named->second];
          trace.case_id_attribute = selected;
          trace.case_id = selected.value;
          trace.case_id_state = FirstClassAttributeState::kPresent;
          trace.case_id_source_key = selected_key;
          attributes.erase(attributes.begin() + named->second);
        }
        trace.attributes = std::move(attributes);
        for (auto& event : trace.events) copy_event_case_context(trace, event);
        reconstructed.push_back(std::move(trace));
      }
      else {
        auto& target = reconstructed[position->second];
        for (auto& event : trace.events) {
          copy_event_case_context(target, event);
          target.events.push_back(std::move(event));
        }
      }
    }
    log.traces = std::move(reconstructed);
    log.attributes = {{"origin", "csv"}};
    log.metadata = {};
    const std::pair<std::string_view, std::string_view> extensions[] = {
        {"concept", "Concept"}, {"time", "Time"}, {"org", "Organizational"},
        {"lifecycle", "Lifecycle"}, {"cost", "Cost"}, {"identity", "Identity"},
        {"artifactlifecycle", "ArtifactLifecycle"}, {"micro", "Micro"},
        {"semantic", "Semantic"}, {"swcomm", "Software Communication"},
        {"swevent", "Software Event"}, {"swtelemetry", "Software Telemetry"}};
    for (const auto& [prefix, name] : extensions) {
      if (prefixes.contains(std::string(prefix)))
        log.metadata.extensions.push_back({std::string(name), std::string(prefix),
            "http://www.xes-standard.org/" + std::string(prefix) + ".xesext"});
    }
    apply_extension_override(log);
    std::erase_if(log.traces, [](const Trace& trace) { return trace.events.empty(); });
  }

  // Materialize the legacy projections after all arbitrary scopes have been
  // parsed. Duplicate trace/event scopes retain their document order by
  // concatenating their attributes in the compatibility vectors.

  log.metadata.compatibility_snapshot_valid = true;
  log.metadata.compatibility_globals_snapshot = log.metadata.globals;
  log.metadata.compatibility_trace_globals_snapshot =
      log.metadata.trace_globals;
  log.metadata.compatibility_event_globals_snapshot =
      log.metadata.event_globals;

  if (options.timestamp_sort && options.variant != XesImportVariant::kChunkRegex)
    sort_log_by_timestamp(log, options.reverse_sort);

  return log;
}

EventLog import_xes_event_log(std::istream& input, const XesImportOptions& options) {
  return import_xes_impl(input, options);
}

XesImportResult import_xes_result(std::istream& input, const XesImportOptions& options) {
  XesImportResult result;
  const bool raw = options.variant == XesImportVariant::kRustXes && !options.return_legacy_log_object;
  result.log = import_xes_impl(input, options, raw ? &result : nullptr);
  return result;
}

XesImportResult import_xes_result(const std::filesystem::path& path, const XesImportOptions& options) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("Unable to open XES event log: " + path.string());
  return import_xes_result(input, options);
}

EventLog import_xes_event_log(
    const std::filesystem::path& path,
    const XesImportOptions& options) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("Unable to open XES event log: " + path.string());
  }
  return import_xes_event_log(input, options);
}

}  // namespace minera
