#include "minera/ocel.hpp"
#include "minera/ocdfg_edge_performance.hpp"
#include "minera/conformance.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/timestamp.hpp"
#include "minera/filtering_common.hpp"
#include "ocel_frame_types.hpp"
#include "ocel_reader_index.hpp"
#include "ocel_xml_times.hpp"
#include "ocpn_projection.hpp"
#include "python_isoformat.hpp"
#include "pandas_to_datetime.hpp"
#include "frame_times.hpp"
#include "python_scalar.hpp"
#include "reference_tree_net.hpp"

#include <algorithm>
#include <deque>
#include <string_view>
#include <bit>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <iomanip>
#include <iterator>
#include <limits>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace minera {
namespace {

// Default elapsed edge timing. Exact parsing keeps microseconds that a
// double epoch value loses at present-day magnitudes, and the conversion is
// shared with the typed timing path. Shapes the exact parser rejects keep the
// historic double projection.
std::optional<double> legacy_edge_elapsed(const std::string& from_text,
                                          const std::string& to_text) {
  // Round EG: a pair with a NaT end (an OCEL 2.0 SQLite '' or NULL time)
  // measures 0, as the reference's OC-DFG does (observed black-box).
  if (ocel_frame::nat_time_text(from_text) || ocel_frame::nat_time_text(to_text)) return 0.0;
  const auto from = parse_timestamp_exact(from_text);
  const auto to = parse_timestamp_exact(to_text);
  if (from && to) return ocdfg_elapsed_seconds(*from, *to);
  const auto from_seconds = parse_timestamp(from_text);
  const auto to_seconds = parse_timestamp(to_text);
  if (from_seconds && to_seconds) return *to_seconds - *from_seconds;
  return std::nullopt;
}

// An integer past 64 bits as Python's json.load reads it (an int of any
// size): its digits as written (Parser::python_numbers).
struct JsonBigInt {
  std::string text;
};

struct Json {
  using Object = std::map<std::string, Json>;
  using Array = std::vector<Json>;
  std::variant<std::nullptr_t, bool, std::int64_t, std::uint64_t, double, std::string, Array,
               Object, JsonBigInt>
      value;
  // An object's keys in document order, as the reference's dict keeps them.
  std::vector<std::string> keys;
  bool is_null() const { return std::holds_alternative<std::nullptr_t>(value); }
  bool is_bool() const { return std::holds_alternative<bool>(value); }
  bool is_int() const { return std::holds_alternative<std::int64_t>(value); }
  bool is_uint() const { return std::holds_alternative<std::uint64_t>(value); }
  bool is_big_int() const { return std::holds_alternative<JsonBigInt>(value); }
  bool is_number() const { return is_int() || std::holds_alternative<double>(value); }
  bool is_string() const { return std::holds_alternative<std::string>(value); }
  bool is_scalar() const {
    return is_null() || is_bool() || is_number() || is_string();
  }
  bool is_array() const { return std::holds_alternative<Array>(value); }
  bool is_object() const { return std::holds_alternative<Object>(value); }
  const Object& object() const { return std::get<Object>(value); }
  const Array& array() const { return std::get<Array>(value); }
  const std::string& string() const { return std::get<std::string>(value); }
};

class Parser {
 public:
  Parser(std::string_view input, const OcelJsonLimits& limits,
         bool unsigned_graph_counts = false)
      : input_(input), limits_(limits), unsigned_graph_counts_(unsigned_graph_counts) {
    if (input_.size() > limits_.max_bytes)
      throw std::invalid_argument("OCEL JSON file exceeds the " +
                                  ocel_limit_text(limits_.max_bytes) +
                                  " OCEL limit");
  }
  // A repeated object key keeps its first place and its last value, as
  // Python's json.load (the reference OCEL JSON readers) builds a dict.
  Parser& last_repeated_key_wins() {
    last_repeated_key_wins_ = true;
    return *this;
  }
  // Numbers as Python's json.load reads them (both reference OCEL JSON
  // readers, round EE): NaN, Infinity and -Infinity, a real past the double
  // range as an infinity, an integer past 64 bits as a JsonBigInt.
  Parser& python_numbers() {
    python_numbers_ = true;
    return *this;
  }
  Json parse() {
    skip();
    Json result = value(0);
    skip();
    if (pos_ != input_.size()) fail("trailing JSON data");
    return result;
  }

 private:
  [[noreturn]] void fail(const char* message) const {
    throw std::invalid_argument(std::string("invalid OCEL JSON at ") +
                                std::to_string(pos_) + ": " + message);
  }
  void skip() {
    while (pos_ < input_.size() && (input_[pos_] == ' ' || input_[pos_] == '\n' ||
                                    input_[pos_] == '\r' || input_[pos_] == '\t'))
      ++pos_;
  }
  void expect(char c) {
    skip();
    if (pos_ >= input_.size() || input_[pos_] != c) fail("unexpected character");
    ++pos_;
  }
  static void append_utf8(std::string& out, unsigned code) {
    if (code <= 0x7f) out.push_back(static_cast<char>(code));
    else if (code <= 0x7ff) { out.push_back(static_cast<char>(0xc0 | (code >> 6))); out.push_back(static_cast<char>(0x80 | (code & 0x3f))); }
    else if (code <= 0xffff) { out.push_back(static_cast<char>(0xe0 | (code >> 12))); out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f))); out.push_back(static_cast<char>(0x80 | (code & 0x3f))); }
    else { out.push_back(static_cast<char>(0xf0 | (code >> 18))); out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f))); out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f))); out.push_back(static_cast<char>(0x80 | (code & 0x3f))); }
  }
  unsigned unicode_escape() {
    if (pos_ + 4 > input_.size()) fail("short unicode escape");
    unsigned code = 0;
    for (int i = 0; i < 4; ++i) {
      const char h = input_[pos_++];
      code <<= 4;
      if (h >= '0' && h <= '9') code += h - '0';
      else if (h >= 'a' && h <= 'f') code += h - 'a' + 10;
      else if (h >= 'A' && h <= 'F') code += h - 'A' + 10;
      else fail("bad unicode escape");
    }
    return code;
  }
  std::string string_value() {
    expect('"');
    std::string out;
    while (pos_ < input_.size()) {
      unsigned char c = static_cast<unsigned char>(input_[pos_++]);
      if (c == '"') {
        if (out.size() > limits_.max_string_bytes) fail("string exceeds size limit");
        return out;
      }
      if (c < 0x20) fail("control character in string");
      if (c != '\\') { out.push_back(static_cast<char>(c)); continue; }
      if (pos_ >= input_.size()) fail("unterminated escape");
      const char esc = input_[pos_++];
      switch (esc) {
        case '"': out.push_back('"'); break; case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break; case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break; case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break; case 't': out.push_back('\t'); break;
        case 'u': {
          unsigned code = unicode_escape();
          if (code >= 0xd800 && code <= 0xdbff) {
            if (input_.substr(pos_, 2) != "\\u")
              fail("missing low surrogate");
            pos_ += 2;
            const unsigned low = unicode_escape();
            if (low < 0xdc00 || low > 0xdfff) fail("bad low surrogate");
            code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
          } else if (code >= 0xdc00 && code <= 0xdfff) {
            fail("unexpected low surrogate");
          }
          append_utf8(out, code); break;
        }
        default: fail("unknown escape");
      }
      if (out.size() > limits_.max_string_bytes) fail("string exceeds size limit");
    }
    fail("unterminated string");
  }
  Json value(std::size_t depth) {
    if (depth > limits_.max_depth) fail("nesting exceeds depth limit");
    if (++nodes_ > limits_.max_nodes) fail("JSON node limit exceeded");
    skip();
    if (pos_ >= input_.size()) fail("unexpected end");
    const char c = input_[pos_];
    if (c == '"') return Json{string_value()};
    if (c == '{') {
      ++pos_; Json::Object object; std::vector<std::string> keys; skip();
      if (pos_ < input_.size() && input_[pos_] == '}') { ++pos_; return Json{std::move(object), {}}; }
      while (true) {
        skip(); if (pos_ >= input_.size() || input_[pos_] != '"') fail("object key must be string");
        auto key = string_value();
        const bool repeated = object.find(key) != object.end();
        if (repeated && !last_repeated_key_wins_) fail("duplicate object key");
        if (!repeated) keys.push_back(key);
        expect(':'); auto item = value(depth + 1); object.insert_or_assign(std::move(key), std::move(item)); skip();
        if (pos_ < input_.size() && input_[pos_] == '}') { ++pos_; break; }
        expect(',');
      }
      return Json{std::move(object), std::move(keys)};
    }
    if (c == '[') {
      ++pos_; Json::Array array; skip();
      if (pos_ < input_.size() && input_[pos_] == ']') { ++pos_; return Json{std::move(array)}; }
      while (true) { array.push_back(value(depth + 1)); skip(); if (pos_ < input_.size() && input_[pos_] == ']') { ++pos_; break; } expect(','); }
      return Json{std::move(array)};
    }
    if (input_.substr(pos_, 4) == "true") { pos_ += 4; return Json{true}; }
    if (input_.substr(pos_, 5) == "false") { pos_ += 5; return Json{false}; }
    if (input_.substr(pos_, 4) == "null") { pos_ += 4; return Json{nullptr}; }
    if (python_numbers_) {
      if (input_.substr(pos_, 3) == "NaN") { pos_ += 3; return Json{std::numeric_limits<double>::quiet_NaN()}; }
      if (input_.substr(pos_, 8) == "Infinity") { pos_ += 8; return Json{std::numeric_limits<double>::infinity()}; }
      if (input_.substr(pos_, 9) == "-Infinity") { pos_ += 9; return Json{-std::numeric_limits<double>::infinity()}; }
    }
    const std::size_t start = pos_;
    if (input_[pos_] == '-') ++pos_;
    if (pos_ >= input_.size() || input_[pos_] < '0' || input_[pos_] > '9') fail("expected value");
    if (input_[pos_] == '0') ++pos_; else while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') ++pos_;
    bool real = false;
    if (pos_ < input_.size() && input_[pos_] == '.') { real = true; ++pos_; if (pos_ >= input_.size() || input_[pos_] < '0' || input_[pos_] > '9') fail("bad number"); while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') ++pos_; }
    if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) { real = true; ++pos_; if (pos_ < input_.size() && (input_[pos_] == '+' || input_[pos_] == '-')) ++pos_; if (pos_ >= input_.size() || input_[pos_] < '0' || input_[pos_] > '9') fail("bad exponent"); while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') ++pos_; }
    const auto token = input_.substr(start, pos_ - start);
    if (real) { std::string text(token); char* end = nullptr; const double n = std::strtod(text.c_str(), &end); if (end != text.c_str() + text.size() || (!std::isfinite(n) && !python_numbers_)) fail("bad number"); return Json{n}; }
    std::int64_t n = 0;
    const auto result = std::from_chars(token.data(), token.data() + token.size(), n);
    if (result.ec == std::errc() && result.ptr == token.data() + token.size())
      return Json{n};
    if (unsigned_graph_counts_ && token.front() != '-') {
      std::uint64_t count = 0;
      const auto unsigned_result =
          std::from_chars(token.data(), token.data() + token.size(), count);
      if (unsigned_result.ec == std::errc() &&
          unsigned_result.ptr == token.data() + token.size())
        return Json{count};
    }
    if (python_numbers_) return Json{JsonBigInt{std::string(token)}};
    fail("integer out of range");
  }
  std::string_view input_; const OcelJsonLimits& limits_; std::size_t pos_ = 0; std::size_t nodes_ = 0;
  bool unsigned_graph_counts_ = false;
  bool last_repeated_key_wins_ = false;
  bool python_numbers_ = false;
};

const Json& required(const Json::Object& o, std::string_view key) {
  auto it = o.find(std::string(key)); if (it == o.end()) throw std::invalid_argument("missing OCEL field: " + std::string(key)); return it->second;
}
const Json* optional(const Json::Object& o, std::string_view key) { auto it = o.find(std::string(key)); return it == o.end() ? nullptr : &it->second; }
std::string str(const Json& j, std::string_view field) { if (!j.is_string()) throw std::invalid_argument("OCEL field must be string: " + std::string(field)); return j.string(); }
OcelValue scalar(const Json& j, std::string_view field) {
  if (j.is_null()) return nullptr; if (j.is_bool()) return std::get<bool>(j.value); if (j.is_int()) return std::get<std::int64_t>(j.value); if (std::holds_alternative<double>(j.value)) return std::get<double>(j.value); if (j.is_string()) return j.string(); throw std::invalid_argument("OCEL attribute must be scalar: " + std::string(field));
}
void attrs(const Json* j, std::vector<OcelAttribute>& out, std::string_view field) {
  if (!j) return; if (!j->is_object()) throw std::invalid_argument("OCEL attributes must be object: " + std::string(field));
  // Document order, as the reference's dict keeps it (its frame columns
  // follow first appearance).
  const auto& object = j->object();
  for (const auto& key : j->keys) { if (key.rfind("ocel:", 0) == 0) continue; out.push_back({key, scalar(object.at(key), key)}); }
}
const Json::Object& obj(const Json& j, std::string_view field) { if (!j.is_object()) throw std::invalid_argument("OCEL field must be object: " + std::string(field)); return j.object(); }

std::size_t size_value(const Json& value, std::string_view field) {
  if (value.is_uint()) {
    const auto count = std::get<std::uint64_t>(value.value);
    if (count <= std::numeric_limits<std::size_t>::max())
      return static_cast<std::size_t>(count);
  }
  if (!value.is_int() || std::get<std::int64_t>(value.value) < 0 ||
      static_cast<std::uint64_t>(std::get<std::int64_t>(value.value)) >
          static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
    throw std::invalid_argument("graph JSON field must be a non-negative integer: " +
                                std::string(field));
  return static_cast<std::size_t>(std::get<std::int64_t>(value.value));
}

double number_value(const Json& value, std::string_view field) {
  if (value.is_int()) return static_cast<double>(std::get<std::int64_t>(value.value));
  if (std::holds_alternative<double>(value.value)) {
    const auto result = std::get<double>(value.value);
    if (std::isfinite(result)) return result;
  }
  throw std::invalid_argument("graph JSON field must be a finite number: " +
                              std::string(field));
}

std::vector<std::string> string_array(const Json* raw,
                                      std::string_view field) {
  if (raw == nullptr) return {};
  if (!raw->is_array())
    throw std::invalid_argument("graph JSON field must be an array: " +
                                std::string(field));
  std::vector<std::string> result;
  result.reserve(raw->array().size());
  for (const auto& value : raw->array())
    result.push_back(str(value, field));
  return result;
}

std::vector<std::pair<std::string, std::string>> pair_array(
    const Json* raw, std::string_view field) {
  if (raw == nullptr) return {};
  if (!raw->is_array())
    throw std::invalid_argument("graph JSON field must be an array: " +
                                std::string(field));
  std::vector<std::pair<std::string, std::string>> result;
  result.reserve(raw->array().size());
  for (const auto& value : raw->array()) {
    if (!value.is_array() || value.array().size() != 2)
      throw std::invalid_argument("graph JSON pair must contain two strings: " +
                                  std::string(field));
    result.emplace_back(str(value.array()[0], field),
                        str(value.array()[1], field));
  }
  return result;
}

std::vector<std::tuple<std::string, std::string, std::string>> triple_array(
    const Json* raw, std::string_view field) {
  if (raw == nullptr) return {};
  if (!raw->is_array())
    throw std::invalid_argument("graph JSON field must be an array: " +
                                std::string(field));
  std::vector<std::tuple<std::string, std::string, std::string>> result;
  result.reserve(raw->array().size());
  for (const auto& value : raw->array()) {
    if (!value.is_array() || value.array().size() != 3)
      throw std::invalid_argument("graph JSON triple must contain three strings: " +
                                  std::string(field));
    result.emplace_back(str(value.array()[0], field),
                        str(value.array()[1], field),
                        str(value.array()[2], field));
  }
  return result;
}

std::vector<double> number_array(const Json* raw, std::string_view field) {
  if (raw == nullptr) return {};
  if (!raw->is_array())
    throw std::invalid_argument("graph JSON field must be an array: " +
                                std::string(field));
  std::vector<double> result;
  result.reserve(raw->array().size());
  for (const auto& value : raw->array()) result.push_back(number_value(value, field));
  return result;
}

using StandardAttributeTypes =
    std::map<std::string, std::map<std::string, std::string>>;

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

// Round EI: read_ocel2_json walks the file in this order (observed
// black-box): the eventTypes table (each entry's attribute names and types,
// then its name), the objectTypes table, the events (type, time, attributes'
// names and values, relationships' objectId and qualifier, then id), the
// objects (type, attributes' values and names, relationships, then id), and
// only then reads the event times (fromisoformat, a time that is no str
// raising AttributeError on .endswith). A missing key raises KeyError with
// the key, a relationship without a qualifier too, and empty or null
// attribute and relationship lists are skipped. Native named the field
// ("missing OCEL field: ...") and read relationships without a qualifier.
const Json& standard_key(const Json::Object& o, std::string_view key) {
  auto it = o.find(std::string(key));
  if (it == o.end()) throw std::invalid_argument("KeyError: " + ocel_frame::python_str_repr(key));
  return it->second;
}
bool standard_truthy(const Json* value) {
  if (value == nullptr || value->is_null()) return false;
  if (value->is_bool()) return std::get<bool>(value->value);
  if (value->is_int()) return std::get<std::int64_t>(value->value) != 0;
  if (std::holds_alternative<double>(value->value)) return std::get<double>(value->value) != 0.0;
  if (value->is_string()) return !value->string().empty();
  if (value->is_array()) return !value->array().empty();
  if (value->is_object()) return !value->object().empty();
  return true;
}
std::string python_json_type_name(const Json& value) {
  if (value.is_null()) return "NoneType";
  if (value.is_bool()) return "bool";
  if (std::holds_alternative<double>(value.value)) return "float";
  if (value.is_string()) return "str";
  if (value.is_array()) return "list";
  if (value.is_object()) return "dict";
  return "int";
}
std::int64_t iso_wall_microseconds(std::string_view original);

void parse_standard_type_table(const Json* raw, StandardAttributeTypes& types,
                               std::string_view field) {
  if (!raw) return;
  if (!raw->is_array())
    throw std::invalid_argument("OCEL " + std::string(field) +
                                " must be an array");
  for (const auto& entry : raw->array()) {
    const auto& descriptor = obj(entry, field);
    // read_ocel2_json takes a type listed twice from its last listing, an
    // attribute listed twice from its last entry, and reads a value whose
    // declared type is empty as undeclared; an empty type name is no error
    // (its events or objects are dropped later), observed black-box.
    // Round EI: the attributes are read before the name (pm4py assigns
    // types[et["name"]] = {x["name"]: x["type"] ...}).
    std::map<std::string, std::string> attributes;
    if (const auto* raw_attributes = optional(descriptor, "attributes")) {
      if (!raw_attributes->is_array())
        throw std::invalid_argument("OCEL type attributes must be an array");
      for (const auto& raw_attribute : raw_attributes->array()) {
        const auto& attribute = obj(raw_attribute, "type attributes[]");
        const auto attribute_name =
            str(standard_key(attribute, "name"), "attribute name");
        const auto attribute_type =
            str(standard_key(attribute, "type"), "attribute type");
        if (attribute_type.empty()) attributes.erase(attribute_name);
        else attributes.insert_or_assign(attribute_name, attribute_type);
      }
    }
    const auto name = str(standard_key(descriptor, "name"), "type name");
    types.insert_or_assign(name, std::move(attributes));
  }
}

// Round EY: whether a catalog type declares a date (as the native frames
// read one, see ocel_frame::declared_date).
bool date_declaration(std::string type) {
  for (auto& ch : type) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return type.find("date") != std::string::npos || type.find("time") != std::string::npos;
}

// Round EY: read_ocel_json ignores a classic file's ocel:eventTypes and
// ocel:objectTypes, so a declared date or time attribute keeps the file's
// texts, a str column whose writers declare it "string" (observed
// black-box, probe ey p/d1.py), where the native frames read a declared
// date's timestamp texts as datetimes. A classic document's date
// declarations therefore read as "string", save in a transport of a log
// whose dates are datetimes (an OCEL 1.0 XML import, say), which says so
// with "minera:declaredDates".
bool declares_dates(const Ocel& log) {
  for (const auto* catalog : {&log.event_type_attributes, &log.object_type_attributes})
    for (const auto& [owner, attributes] : *catalog)
      for (const auto& [name, type] : attributes)
        if (date_declaration(type)) return true;
  return false;
}

void read_classic_dates_as_texts(Ocel& log) {
  for (auto* catalog : {&log.event_type_attributes, &log.object_type_attributes})
    for (auto& [owner, attributes] : *catalog)
      for (auto& [name, type] : attributes)
        if (date_declaration(type)) type = "string";
}

void parse_classic_type_catalog(const Json* raw, OcelTypeCatalog& catalog,
                                std::string_view field) {
  if (!raw) return;
  if (!raw->is_object())
    throw std::invalid_argument("OCEL " + std::string(field) +
                                " must be an object");
  // An empty type name is read (the reference readers ignore this catalog);
  // drop_empty_ocel_names removes it unless names are literal.
  for (const auto& [type, raw_attributes] : raw->object()) {
    const auto& attributes = obj(raw_attributes, field);
    auto& declared = catalog[type];
    // An empty attribute name is a name (read_ocel2_json keeps the column).
    for (const auto& [name, raw_type] : attributes) {
      const auto attribute_type = str(raw_type, "OCEL attribute type");
      if (attribute_type.empty())
        throw std::invalid_argument("OCEL attribute type is empty");
      const auto [it, inserted] = declared.emplace(name, attribute_type);
      if (!inserted && it->second != attribute_type)
        throw std::invalid_argument("conflicting OCEL attribute type");
    }
  }
}

const std::string* standard_attribute_type(const StandardAttributeTypes& types,
                                            std::string_view owner,
                                            std::string_view attribute) {
  const auto owner_it = types.find(std::string(owner));
  if (owner_it == types.end()) return nullptr;
  const auto attribute_it = owner_it->second.find(std::string(attribute));
  return attribute_it == owner_it->second.end() ? nullptr : &attribute_it->second;
}

// Mirrors the reference OCEL 2.0 JSON value coercion: the declared type is
// tried in reference order (date/time, float/double, int, bool) and a value
// that cannot be converted keeps its raw JSON scalar instead of failing.
OcelValue standard_scalar(const Json& value, std::string_view field,
                          const std::string* declared_type) {
  if (declared_type == nullptr || declared_type->empty())
    return scalar(value, field);
  const auto type = lowercase(*declared_type);
  if (value.is_null()) return nullptr;
  const auto trimmed = [](const std::string& text) {
    const auto first = text.find_first_not_of(" \t\n\r\f\v");
    if (first == std::string::npos) return std::string{};
    const auto last = text.find_last_not_of(" \t\n\r\f\v");
    return text.substr(first, last - first + 1);
  };
  if (type.find("date") != std::string::npos ||
      type.find("time") != std::string::npos) {
    if (value.is_string()) return value.string();
    return scalar(value, field);
  }
  if (type.find("float") != std::string::npos ||
      type.find("double") != std::string::npos || type == "number") {
    if (std::holds_alternative<double>(value.value))
      return std::get<double>(value.value);
    if (value.is_int())
      return static_cast<double>(std::get<std::int64_t>(value.value));
    if (value.is_bool()) return std::get<bool>(value.value) ? 1.0 : 0.0;
    if (value.is_string()) {
      const auto text = trimmed(value.string());
      // Round EK: float("nan") (any case, a sign allowed) is NaN, a missing
      // cell of the frame (observed black-box: the writers leave it out).
      // float("inf") stays the text (declared: the transport holds no
      // infinity).
      const auto unsigned_text = lowercase(!text.empty() && (text.front() == '+' || text.front() == '-')
                                               ? text.substr(1)
                                               : text);
      if (unsigned_text == "nan") return nullptr;
      if (!text.empty() && text.find_first_of("xXpP") == std::string::npos) {
        char* end = nullptr;
        const double number = std::strtod(text.c_str(), &end);
        if (end == text.c_str() + text.size() && std::isfinite(number))
          return number;
      }
    }
    return scalar(value, field);
  }
  if (type.find("int") != std::string::npos ||
      type.find("long") != std::string::npos) {
    if (value.is_int()) return std::get<std::int64_t>(value.value);
    if (value.is_bool()) return static_cast<std::int64_t>(std::get<bool>(value.value));
    if (value.is_string()) {
      auto text = trimmed(value.string());
      if (!text.empty() && text.front() == '+') text.erase(0, 1);
      std::int64_t result = 0;
      const auto parsed = std::from_chars(text.data(), text.data() + text.size(),
                                          result);
      if (!text.empty() && parsed.ec == std::errc() &&
          parsed.ptr == text.data() + text.size())
        return result;
    }
    if (std::holds_alternative<double>(value.value)) {
      // int(float) truncates toward zero.
      const double number = std::trunc(std::get<double>(value.value));
      if (std::isfinite(number) &&
          number >= static_cast<double>(std::numeric_limits<std::int64_t>::min()) &&
          number < 0x1p63)
        return static_cast<std::int64_t>(number);
    }
    return scalar(value, field);
  }
  if (type.find("bool") != std::string::npos) {
    if (value.is_bool()) return std::get<bool>(value.value);
    if (value.is_string()) {
      const auto text = lowercase(trimmed(value.string()));
      if (text == "true") return true;
      if (text == "false") return false;
      return !value.string().empty();  // bool(str): any non-empty text
    }
    if (value.is_int()) return std::get<std::int64_t>(value.value) != 0;
    if (std::holds_alternative<double>(value.value))
      return std::get<double>(value.value) != 0.0;
    return scalar(value, field);
  }
  return scalar(value, field);
}

// ---------------------------------------------------------------------------
// Round EE: read_ocel_json's reading of a classic OCEL JSON file (observed
// black-box with pm4py 2.7.22, pandas 3.0.5; probes and fuzz in the round-ee
// harness). The reference walks the objects first, then the events, each in
// file order, and raises Python's own errors on the way: an object takes its
// "ocel:type", "ocel:ovmap" and "ocel:o2o" in that order, an event its
// "ocel:timestamp" (fromisoformat after a trailing Z becomes "+00:00"),
// "ocel:activity", "ocel:vmap", "ocel:omap" and "ocel:typedOmap". A file
// without events then raises KeyError 'ocel:timestamp', the three global
// scopes are looked up (log, event, object) and the object changes read.
// Its frames are pandas frames built from those records, so a column's dtype
// comes from all its rows before any is dropped: an activity or object type
// that is no string is str() of the column's value (an int next to a float
// or a missing value is a float, "1.0"), a missing one (None, NaN) drops its
// event or object, and an int attribute value in such a column is a float.
std::int64_t iso_wall_microseconds(std::string_view original);

namespace classic_json {

[[noreturn]] void raise(const std::string& message) { throw std::invalid_argument(message); }

std::string key_error(std::string_view key) {
  return "KeyError: " + ocel_frame::python_str_repr(key);
}

// Python's type name of a value json.load built.
std::string type_name(const Json& value) {
  if (value.is_null()) return "NoneType";
  if (value.is_bool()) return "bool";
  if (value.is_int() || value.is_uint() || value.is_big_int()) return "int";
  if (std::holds_alternative<double>(value.value)) return "float";
  if (value.is_string()) return "str";
  if (value.is_array()) return "list";
  return "dict";
}

// Round EW: a qualifier of a document Minera wrote with
// "minera:typedQualifiers" may be a JSON int, float or (round EX) bool
// (OcelQualifierKind); nullopt for any other non-string value.
std::optional<std::pair<std::string, OcelQualifierKind>> typed_transport_qualifier(const Json& value);

std::string repr(const Json& value) {
  if (value.is_null()) return "None";
  if (value.is_bool()) return std::get<bool>(value.value) ? "True" : "False";
  if (value.is_int()) return std::to_string(std::get<std::int64_t>(value.value));
  if (value.is_uint()) return std::to_string(std::get<std::uint64_t>(value.value));
  if (value.is_big_int()) return std::get<JsonBigInt>(value.value).text;
  if (const auto* real = std::get_if<double>(&value.value)) return ocel_frame::float_repr(*real);
  if (value.is_string()) return ocel_frame::python_str_repr(value.string());
  std::string out;
  if (value.is_array()) {
    out = "[";
    for (std::size_t i = 0; i < value.array().size(); ++i) {
      if (i) out += ", ";
      out += repr(value.array()[i]);
    }
    return out + "]";
  }
  out = "{";
  for (std::size_t i = 0; i < value.keys.size(); ++i) {
    if (i) out += ", ";
    out += ocel_frame::python_str_repr(value.keys[i]) + ": " + repr(value.object().at(value.keys[i]));
  }
  return out + "}";
}

std::optional<std::pair<std::string, OcelQualifierKind>> typed_transport_qualifier(const Json& value) {
  if (value.is_int() || value.is_uint()) return std::pair{repr(value), OcelQualifierKind::kInt};
  if (const auto* real = std::get_if<double>(&value.value); real != nullptr && std::isfinite(*real))
    return std::pair{repr(value), OcelQualifierKind::kFloat};
  if (value.is_bool()) return std::pair{repr(value), OcelQualifierKind::kBool};
  return std::nullopt;
}

// container[key] for a str key.
const Json& getitem(const Json& container, const std::string& key) {
  if (container.is_object()) {
    const auto found = container.object().find(key);
    if (found == container.object().end()) raise(key_error(key));
    return found->second;
  }
  if (container.is_array()) raise("list indices must be integers or slices, not str");
  if (container.is_string()) raise("string indices must be integers, not 'str'");
  raise("'" + type_name(container) + "' object is not subscriptable");
}

// needle in container.
bool contains(const Json& container, std::string_view needle) {
  if (container.is_object()) return container.object().count(std::string(needle)) != 0;
  if (container.is_array())
    return std::any_of(container.array().begin(), container.array().end(),
                       [&](const Json& item) { return item.is_string() && item.string() == needle; });
  if (container.is_string()) return container.string().find(needle) != std::string::npos;
  raise("argument of type '" + type_name(container) + "' is not iterable");
}

// The text a value names as a key of a dict of str keys (x in d, d[x]): a
// str matches by its text, a list or dict raises, any other value matches
// nothing.
const std::string* hashable_text(const Json& value) {
  if (value.is_array()) raise("unhashable type: 'list'");
  if (value.is_object()) raise("unhashable type: 'dict'");
  return value.is_string() ? &value.string() : nullptr;
}

// for item in value: a list's items, a dict's keys, a str's characters.
template <class F>
void for_each_item(const Json& value, F&& visit) {
  if (value.is_array()) {
    for (const auto& item : value.array()) visit(item);
  } else if (value.is_object()) {
    for (const auto& key : value.keys) visit(Json{key});
  } else if (value.is_string()) {
    const auto& text = value.string();
    for (std::size_t p = 0; p < text.size();) {
      const auto lead = static_cast<unsigned char>(text[p]);
      const std::size_t length = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
      visit(Json{text.substr(p, length)});
      p += length;
    }
  } else {
    raise("'" + type_name(value) + "' object is not iterable");
  }
}

// for key in container: container[key], for "ocel:objects" and "ocel:events".
template <class F>
void for_each_entry(const Json& container, F&& visit) {
  if (container.is_object()) {
    for (const auto& key : container.keys) visit(key, container.object().at(key));
    return;
  }
  if (container.is_array()) {
    if (container.array().empty()) return;
    const auto& first = container.array().front();
    if (first.is_int() || first.is_bool() || first.is_uint() || first.is_big_int())
      raise("OCEL JSON lists entries by position: read_ocel_json indexes the list with its own "
            "items, which a native OCEL does not read");
    raise("list indices must be integers or slices, not " + type_name(first));
  }
  if (container.is_string()) {
    if (container.string().empty()) return;
    raise("string indices must be integers, not 'str'");
  }
  raise("'" + type_name(container) + "' object is not iterable");
}

// The dtype pandas gives a column built from records of these values.
struct Column {
  bool text = false, container = false, boolean = false, integer = false;
  bool big_unsigned = false, big_other = false, real = false, missing = false, negative = false;
  void observe(const Json* value) {
    if (value == nullptr || value->is_null()) { missing = true; return; }
    if (value->is_string()) text = true;
    else if (value->is_array() || value->is_object()) container = true;
    else if (value->is_bool()) boolean = true;
    else if (value->is_int()) {
      integer = true;
      negative = negative || std::get<std::int64_t>(value->value) < 0;
    } else if (value->is_uint()) big_unsigned = true;
    else if (value->is_big_int()) {
      const auto& digits = std::get<JsonBigInt>(value->value).text;
      const bool fits = digits.front() != '-' &&
                        (digits.size() < 20 || (digits.size() == 20 && digits <= "18446744073709551615"));
      (fits ? big_unsigned : big_other) = true;
    } else real = true;
  }
  enum class Kind { kObject, kBool, kInt, kFloat };
  [[nodiscard]] Kind kind() const {
    if (text || container) return Kind::kObject;
    if (boolean) return integer || big_unsigned || big_other || real || missing ? Kind::kObject : Kind::kBool;
    // An integer past 64 bits keeps the column of Python objects; one that
    // fits uint64 reads as float next to floats or missing values, and as
    // uint64 next to non-negative integers only.
    if (big_other) return Kind::kObject;
    if (big_unsigned) return negative ? Kind::kObject : real || missing ? Kind::kFloat : Kind::kInt;
    if (integer || real) return real || missing ? Kind::kFloat : Kind::kInt;
    return Kind::kObject;
  }
};

bool missing_value(const Json& value) {
  if (value.is_null()) return true;
  const auto* real = std::get_if<double>(&value.value);
  return real != nullptr && std::isnan(*real);
}

// A number of a float column as that column holds it (the nearest double).
std::optional<double> float_cell(const Json& value) {
  if (value.is_int()) return static_cast<double>(std::get<std::int64_t>(value.value));
  if (value.is_uint()) return static_cast<double>(std::get<std::uint64_t>(value.value));
  if (value.is_big_int()) return std::strtod(std::get<JsonBigInt>(value.value).text.c_str(), nullptr);
  if (const auto* real = std::get_if<double>(&value.value)) return *real;
  return std::nullopt;
}

// astype(str) of a cell of a column of `kind`: nullopt for a missing one.
std::optional<std::string> column_str(const Json& value, Column::Kind kind) {
  if (missing_value(value)) return std::nullopt;
  if (kind == Column::Kind::kFloat)
    if (const auto real = float_cell(value)) return ocel_frame::float_repr(*real);
  return value.is_string() ? value.string() : repr(value);
}

// Round EX: a qualifier as read_ocel2_json's and read_ocel_json's frames
// hold it (observed black-box, probes ex p/p1.py..p5c.py): the relations
// and O2O frames are built from records, so the qualifier column's dtype
// comes from every row the frame is built from (`kind`, a Column over
// them): a str stays a str and a missing qualifier (None, NaN, or an
// omap entry without one) reads as ''; an int is an int in an int64 or
// object column and a float next to floats or missing qualifiers; a bool
// stays a bool. A list, a dict, an integer past 64 bits or an infinite
// float is refused (`refusal` names it).
std::pair<std::string, OcelQualifierKind> frame_qualifier(const Json* value, Column::Kind kind,
                                                          std::string& refusal) {
  if (value == nullptr || missing_value(*value)) return {std::string(), OcelQualifierKind::kStr};
  if (value->is_string()) return {value->string(), OcelQualifierKind::kStr};
  if (value->is_bool()) return {repr(*value), OcelQualifierKind::kBool};
  if (value->is_array() || value->is_object()) {
    refusal = "is a " + type_name(*value) + " (" + repr(*value) + "), which pandas keeps as a Python object";
    return {std::string(), OcelQualifierKind::kStr};
  }
  if (value->is_uint() || value->is_big_int()) {
    refusal = "is an integer past 64 bits (" + repr(*value) + ")";
    return {std::string(), OcelQualifierKind::kStr};
  }
  if (const auto* real = std::get_if<double>(&value->value); real != nullptr && !std::isfinite(*real)) {
    refusal = "is an infinite float (" + repr(*value) + ")";
    return {std::string(), OcelQualifierKind::kStr};
  }
  if (kind == Column::Kind::kFloat) return {ocel_frame::float_repr(*float_cell(*value)), OcelQualifierKind::kFloat};
  return {repr(*value), value->is_int() ? OcelQualifierKind::kInt : OcelQualifierKind::kFloat};
}

// Round EY: the qualifier dtype of a frame column of `kind` (see
// Ocel::relation_qualifier_dtype; an object column is kStr).
OcelQualifierKind frame_qualifier_dtype(Column::Kind kind) {
  switch (kind) {
    case Column::Kind::kBool: return OcelQualifierKind::kBool;
    case Column::Kind::kInt: return OcelQualifierKind::kInt;
    case Column::Kind::kFloat: return OcelQualifierKind::kFloat;
    case Column::Kind::kObject: break;
  }
  return OcelQualifierKind::kStr;
}

// Round EY: the readers fill a missing qualifier with '' after dropping
// the rows of empty ids, empty types and missing activities and before
// dropping dangling links and unrelated or empty-typed objects' links, so a
// missing qualifier left at that point makes an int, float or bool column
// an object one (observed black-box, probes ey p/e5.py, p/e6.py).
OcelQualifierKind filled_qualifier_dtype(Column::Kind kind, bool filled) {
  return filled ? OcelQualifierKind::kStr : frame_qualifier_dtype(kind);
}

// A vmap, ovmap or change value: the native value, or the reason it cannot
// be one (a list, dict or integer past 64 bits stays a Python object in the
// reference frame).
struct CellValue {
  OcelValue value;
  std::string unrepresentable;
};

CellValue cell_value(const Json& value) {
  if (value.is_array() || value.is_object())
    return {nullptr, "is a " + type_name(value) + " (" + repr(value) + "), which pandas keeps as a Python object"};
  if (value.is_big_int() || value.is_uint())
    return {nullptr, "is an integer past 64 bits (" + repr(value) + ")"};
  if (value.is_null()) return {nullptr, {}};
  if (value.is_bool()) return {std::get<bool>(value.value), {}};
  if (value.is_int()) return {std::get<std::int64_t>(value.value), {}};
  if (const auto* real = std::get_if<double>(&value.value)) return {*real, {}};
  return {value.string(), {}};
}

// One table of attribute rows (events, objects or changes) with its columns.
struct AttributeRows {
  struct Row {
    std::vector<std::pair<std::string, const Json*>> values;
  };
  std::vector<Row> rows;
  std::vector<std::string> names;
  std::unordered_map<std::string, Column> columns;
  void finish() {
    for (const auto& row : rows)
      for (const auto& [key, value] : row.values)
        if (!columns.count(key)) { names.push_back(key); columns[key]; }
    for (const auto& name : names) {
      auto& column = columns[name];
      for (const auto& row : rows) {
        const Json* cell = nullptr;
        for (const auto& [key, value] : row.values) if (key == name) cell = value;
        column.observe(cell);
      }
    }
  }
  // A cell of column `key` as the frame holds it.
  [[nodiscard]] CellValue cell(const std::string& key, const Json& raw) const {
    if (columns.at(key).kind() == Column::Kind::kFloat)
      if (const auto real = float_cell(raw)) return {*real, {}};
    return cell_value(raw);
  }
  // The row's attributes as the frame holds them; `bad` takes the reason a
  // value cannot be held.
  std::vector<OcelAttribute> attributes(std::size_t index, std::string& bad) const {
    std::vector<OcelAttribute> out;
    for (const auto& [key, raw] : rows[index].values) {
      auto value = cell(key, *raw);
      if (!value.unrepresentable.empty() && bad.empty()) bad = "value '" + key + "' " + value.unrepresentable;
      out.push_back({key, std::move(value.value)});
    }
    return out;
  }
};

// Round EM: a transport's frame hint (OcelFrameHint, "minera:frameHints")
// types a frame's columns too: the dropped rows' value kinds and their
// missing cells. The changes frame's hint also holds the cells a change row
// has in the other fields' columns (no rows of their own), which fill the
// missing cells of the transport's rows there.
void apply_transport_frame_hint(const Json* hints, const char* name, AttributeRows& rows, bool changes) {
  if (hints == nullptr || !hints->is_object()) return;
  const auto* hint = optional(hints->object(), name);
  if (hint == nullptr || !hint->is_object()) return;
  std::size_t dropped_rows = 0;
  if (const auto* count = optional(hint->object(), "rows"); count != nullptr && count->is_int())
    dropped_rows = static_cast<std::size_t>(std::max<std::int64_t>(0, std::get<std::int64_t>(count->value)));
  std::unordered_map<std::string, std::pair<std::int64_t, std::int64_t>> dropped;  // kinds, present
  if (const auto* list = optional(hint->object(), "dropped"); list != nullptr && list->is_array())
    for (const auto& item : list->array())
      if (item.is_array() && item.array().size() == 3 && item.array()[0].is_string() &&
          item.array()[1].is_int() && item.array()[2].is_int())
        dropped[item.array()[0].string()] = {std::get<std::int64_t>(item.array()[1].value),
                                             std::get<std::int64_t>(item.array()[2].value)};
  for (auto& [key, column] : rows.columns) {
    const auto found = dropped.find(key);
    const std::int64_t kinds = found == dropped.end() ? 0 : found->second.first;
    const std::int64_t present = found == dropped.end() ? 0 : found->second.second;
    if (kinds & (kOcelHintString | kOcelHintNaiveDate | kOcelHintAwareDate | kOcelHintNone)) column.text = true;
    if (kinds & kOcelHintBool) column.boolean = true;
    if (kinds & kOcelHintInt) column.integer = true;
    if (kinds & kOcelHintFloat) column.real = true;
    if (!changes) {
      if (present < static_cast<std::int64_t>(dropped_rows)) column.missing = true;
      continue;
    }
    // A change row lacking the column has a cell there only among the
    // hint's; its own field's null value stays missing.
    std::int64_t lacking = 0;
    bool null_value = false;
    for (const auto& row : rows.rows) {
      const auto cell = std::find_if(row.values.begin(), row.values.end(),
                                     [&](const auto& value) { return value.first == key; });
      if (cell == row.values.end()) ++lacking;
      else null_value = null_value || cell->second == nullptr || cell->second->is_null();
    }
    column.missing = null_value || lacking + static_cast<std::int64_t>(dropped_rows) > present;
  }
}

}  // namespace classic_json

// `lenient_times`: a transport that says it holds event times read_ocel_json
// cannot read (see serialize_ocel_json) keeps them for the file-order sort;
// `lenient_change_times` likewise keeps the change times as written (an
// OCEL 2.0 JSON import's NaT change time, round EJ).
// `require_events`: the file is read as the reference reads it (no
// keepUnrelated or untypedRelations marker), so a file without events
// raises before the global scopes are looked up. `keep_unrelated`: a
// transport keeping events and objects without relations (the reference's
// filtering by relations is left out).
//
// The reference then drops, in each frame on its own, the rows with a
// missing or empty id, activity or type (the relations frame by the event's
// and object's own values), keeps the events and objects some remaining
// relation names, and then the relations whose event and object remain
// (observed black-box: an event an ovmap "ocel:oid" or "ocel:type" takes
// from the objects frame stays without relations). A row whose frame cell
// disagrees with the relations frame, or holds what a native OCEL cannot,
// is refused when it remains.
struct ClassicListing {
  bool objects = false, relations = false;  // the file lists any (before the filtering)
  bool o2o = false;                         // round EO: any object link (Ocel::untyped_o2o)
};

// Round EL: a transport's "minera:rawTime" of an event or change (see
// OcelRawTime): "str", "int", "float" or "nan".
OcelRawTime transport_raw_time(const Json* marker) {
  if (marker == nullptr) return OcelRawTime::kNone;
  if (!marker->is_string()) throw std::invalid_argument("minera:rawTime must be str, int, float, nan or none");
  const auto& kind = marker->string();
  if (kind == "str") return OcelRawTime::kStr;
  if (kind == "int") return OcelRawTime::kInt;
  if (kind == "float") return OcelRawTime::kFloat;
  if (kind == "nan") return OcelRawTime::kNaN;
  if (kind == "none") return OcelRawTime::kNull;  // round EN
  throw std::invalid_argument("minera:rawTime must be str, int, float, nan or none");
}

Ocel parse_classic_reference(const Json& root_json, bool repeated_relations, bool lenient_times,
                             bool require_events, bool keep_unrelated, ClassicListing& listing,
                             bool lenient_change_times = false, bool typed_qualifiers = false) {
  using namespace classic_json;
  const auto& root = root_json.object();
  Ocel log;
  parse_classic_type_catalog(optional(root, "ocel:objectTypes"), log.object_type_attributes, "ocel:objectTypes");
  parse_classic_type_catalog(optional(root, "ocel:eventTypes"), log.event_type_attributes, "ocel:eventTypes");

  // Objects.
  // An ovmap "ocel:oid" or "ocel:type" (a vmap "ocel:eid" or
  // "ocel:activity") takes the objects-frame (events-frame) cell while the
  // relations frame keeps the object's (event's) own value (observed): a
  // type or activity read so must agree with the relations frame's (else
  // refused when the row remains), and an id that names no related
  // object (event) drops the row from the filtered frames.
  struct RawObject {
    std::string id;
    const Json* type;
    const Json* frame_type;
    const Json* frame_id;
  };
  std::vector<RawObject> objects;
  AttributeRows object_rows;
  struct RawLink {
    std::string source;
    Json target;
    const Json* qualifier;
  };
  std::vector<RawLink> links;
  std::unordered_map<std::string, std::size_t> object_index;
  for_each_entry(getitem(root_json, "ocel:objects"), [&](const std::string& id, const Json& raw) {
    const Json& type = getitem(raw, "ocel:type");
    const Json& ovmap = getitem(raw, "ocel:ovmap");
    if (!ovmap.is_object()) raise("'" + type_name(ovmap) + "' object has no attribute 'items'");
    AttributeRows::Row row;
    const Json* frame_type = &type;
    const Json* frame_id = nullptr;
    for (const auto& key : ovmap.keys) {
      const Json& value = ovmap.object().at(key);
      if (key == "ocel:type") frame_type = &value;
      else if (key == "ocel:oid") frame_id = &value;
      else row.values.emplace_back(key, &value);
    }
    if (const auto* o2o = optional(raw.object(), "ocel:o2o")) {
      for_each_item(*o2o, [&](const Json& entry) {
        const Json& target = getitem(entry, "ocel:oid");
        const Json& qualifier = getitem(entry, "ocel:qualifier");
        links.push_back({id, target, &qualifier});
      });
    }
    // A repeated id is one dict entry (json.load), so ids are unique here.
    object_index.emplace(id, objects.size());
    objects.push_back({id, &type, frame_type, frame_id});
    object_rows.rows.push_back(std::move(row));
  });

  // Events.
  struct RawEvent {
    std::string id;
    const Json* activity;
    const Json* frame_activity;
    const Json* frame_id;
    const Json* frame_time;  // a vmap "ocel:timestamp", raw in the events frame
    std::string time;
    std::vector<std::size_t> relations;  // indices into `relations` below
    OcelRawTime raw_time = OcelRawTime::kNone;
  };
  std::vector<RawEvent> events;
  AttributeRows event_rows;
  struct RawRelation {
    std::size_t event, object;
    const Json* qualifier = nullptr;
  };
  std::vector<RawRelation> relations;
  for_each_entry(getitem(root_json, "ocel:events"), [&](const std::string& id, const Json& raw) {
    const Json& time = getitem(raw, "ocel:timestamp");
    if (!time.is_string()) raise("'" + type_name(time) + "' object has no attribute 'endswith'");
    if (!lenient_times) (void)iso_wall_microseconds(time.string());
    const Json& activity = getitem(raw, "ocel:activity");
    const Json& vmap = getitem(raw, "ocel:vmap");
    if (!vmap.is_object()) raise("'" + type_name(vmap) + "' object has no attribute 'items'");
    AttributeRows::Row row;
    const Json* frame_activity = &activity;
    const Json* frame_id = nullptr;
    const Json* frame_time = nullptr;
    for (const auto& key : vmap.keys) {
      const Json& value = vmap.object().at(key);
      if (key == "ocel:activity") frame_activity = &value;
      else if (key == "ocel:eid") frame_id = &value;
      else if (key == "ocel:timestamp") frame_time = &value;
      else row.values.emplace_back(key, &value);
    }
    const auto event_index = events.size();
    events.push_back({id, &activity, frame_activity, frame_id, frame_time, time.string(), {}});
    auto& event = events.back();
    if (lenient_times) event.raw_time = transport_raw_time(optional(raw.object(), "minera:rawTime"));
    std::unordered_map<std::string, std::vector<std::size_t>> related;
    for_each_item(getitem(raw, "ocel:omap"), [&](const Json& item) {
      const auto* oid = hashable_text(item);
      if (oid == nullptr) return;
      const auto found = object_index.find(*oid);
      // The reference drops a relation to an object the file does not list.
      if (found == object_index.end()) return;
      auto& seen = related[*oid];
      if (!seen.empty() && !repeated_relations) return;
      seen.push_back(relations.size());
      event.relations.push_back(relations.size());
      relations.push_back({event_index, found->second, nullptr});
    });
    if (const auto* typed = optional(raw.object(), "ocel:typedOmap")) {
      std::unordered_map<std::string, std::size_t> assigned;
      for_each_item(*typed, [&](const Json& entry) {
        if (!contains(entry, "ocel:oid")) return;
        const auto* oid = hashable_text(getitem(entry, "ocel:oid"));
        if (oid == nullptr) return;
        const auto found = related.find(*oid);
        // A qualifier only qualifies a relation the omap made.
        if (found == related.end()) return;
        const Json& qualifier = getitem(entry, "ocel:qualifier");
        if (repeated_relations) {
          const auto index = assigned[*oid]++;
          if (index < found->second.size()) relations[found->second[index]].qualifier = &qualifier;
          else {
            event.relations.push_back(relations.size());
            relations.push_back({event_index, object_index.at(*oid), &qualifier});
          }
        } else {
          for (const auto index : found->second) relations[index].qualifier = &qualifier;
        }
      });
    }
    event_rows.rows.push_back(std::move(row));
  });
  if (require_events && events.empty()) raise(key_error("ocel:timestamp"));
  // The events frame sorts its time column: a vmap "ocel:timestamp" puts
  // its raw value there (a null is NaT), a list or dict
  // cannot be hashed, and a number or boolean next to a parsed time raises.
  // A text next to one leaves the column of Python objects (observed).
  bool parsed_times = false, number_times = false, text_times = false;
  for (const auto& event : events) {
    if (event.frame_time == nullptr) {
      parsed_times = true;
      continue;
    }
    const auto& time = *event.frame_time;
    if (time.is_array() || time.is_object()) (void)hashable_text(time);
    if (time.is_string()) text_times = true;
    else if (!missing_value(time)) number_times = true;
  }
  if (parsed_times && number_times)
    raise("'values' is not ordered, please explicitly specify the categories order by passing in a "
          "categories argument.");

  // The global scopes (any value; a dict's scalars are read).
  for (const auto& category : {"ocel:global-log", "ocel:global-event", "ocel:global-object"}) {
    const Json& scope = getitem(root_json, category);
    if (!scope.is_object()) continue;
    for (const auto& key : scope.keys) {
      const Json& value = scope.object().at(key);
      if (value.is_scalar()) log.globals[std::string(category) + "." + key] = scalar(value, key);
    }
  }
  log.global_event_scope = true;
  log.global_object_scope = true;

  // Object changes: a frame of their records, so a value column's dtype
  // comes from every change row (other fields' rows are missing cells),
  // also those of objects the file does not list, which are dropped. A
  // change has its object's type whatever its own "ocel:type" says.
  struct RawChange {
    std::size_t object;
    std::string time, field;
    OcelValue value;
    std::string refusal;  // why a native OCEL cannot hold it, if it remains
    OcelRawTime raw_time = OcelRawTime::kNone;
    std::vector<OcelAttribute> cells;  // round EN: see OcelObjectChange::cells
  };
  std::vector<RawChange> changes;
  if (const auto* raw_changes = optional(root, "ocel:objectChanges"); raw_changes && !raw_changes->is_null()) {
    if (raw_changes->is_object()) {
      const bool scalars = std::all_of(raw_changes->keys.begin(), raw_changes->keys.end(), [&](const std::string& key) {
        return raw_changes->object().at(key).is_scalar() || raw_changes->object().at(key).is_big_int();
      });
      raise(scalars && !raw_changes->keys.empty() ? "If using all scalar values, you must pass an index"
                                                  : "OCEL JSON ocel:objectChanges must be a list of changes");
    }
    if (!raw_changes->is_array()) raise("DataFrame constructor not properly called!");
    // pandas builds the changes frame from records when the first entry is
    // a dict, taking dict(entry) of each (an empty str or list is an empty
    // row); any other first entry gives a frame without an "ocel:oid"
    // column (observed black-box).
    static const Json empty_entry{Json::Object{}};
    std::vector<const Json*> entries;
    const auto& list = raw_changes->array();
    const bool records = list.empty() || list.front().is_object();
    for (const auto& entry : list) {
      if (!records) break;
      if (entry.is_object()) {
        entries.push_back(&entry);
      } else if (entry.is_string() || entry.is_array()) {
        if (entry.is_string() ? entry.string().empty() : entry.array().empty()) {
          entries.push_back(&empty_entry);
        } else if (entry.is_string()) {
          raise("dictionary update sequence element #0 has length 1; 2 is required");
        } else {
          raise("OCEL JSON object change " + repr(entry) +
                ": pm4py reads a list of key-value pairs as a dict, which Minera does not read");
        }
      } else {
        raise("'" + type_name(entry) + "' object is not iterable");
      }
    }
    // Changes join the objects frame, which a file listing no object has
    // without columns (observed).
    if (objects.empty() && !list.empty())
      raise("KeyError: \"None of [Index(['ocel:oid', 'ocel:type'], dtype='str')] are in the [columns]\"");
    if (!records) raise(key_error("ocel:oid"));
    if (!entries.empty() && std::none_of(entries.begin(), entries.end(), [](const Json* entry) {
          return entry->object().count("ocel:oid") != 0;
        }))
      raise(key_error("ocel:oid"));
    // The columns hold every row: the ids read as str (by the column's
    // dtype), a time pm4py cannot read (or one that is no str) leaves every
    // time as given, and each value column takes its dtype over all rows,
    // the rows of unknown objects included (observed).
    Column ids;
    bool raw_times = false;
    AttributeRows change_rows;
    const auto readable_time = [](const std::string& text) {
      try {
        (void)iso_wall_microseconds(text);
        return true;
      } catch (const std::invalid_argument&) {
        return false;
      }
    };
    for (const auto* entry : entries) {
      const auto& change = entry->object();
      ids.observe(optional(change, "ocel:oid"));
      if (const auto* time = optional(change, "ocel:timestamp"); time && !missing_value(*time))
        raw_times = raw_times || (!lenient_change_times && (!time->is_string() || !readable_time(time->string())));
      AttributeRows::Row row;
      for (const auto& key : entry->keys)
        if (key != "ocel:oid" && key != "ocel:type" && key != "ocel:timestamp" && key != "ocel:field" &&
            !(lenient_change_times && key == "minera:rawTime"))
          row.values.emplace_back(key, &change.at(key));
      change_rows.rows.push_back(std::move(row));
    }
    change_rows.finish();
    apply_transport_frame_hint(optional(root, "minera:frameHints"), "changes", change_rows, true);
    for (const auto* entry : entries) {
      const auto& change = entry->object();
      const auto* raw_id = optional(change, "ocel:oid");
      if (raw_id == nullptr) continue;
      const auto id = column_str(*raw_id, ids.kind());
      if (!id) continue;
      const auto found = object_index.find(*id);
      if (found == object_index.end()) continue;  // the reference drops it
      // The change takes its object's type whatever its own "ocel:type"
      // says; an id that is no str finds no type (the change keeps NaN).
      RawChange raw{found->second, {}, {}, nullptr, {}};
      if (lenient_change_times) raw.raw_time = transport_raw_time(optional(change, "minera:rawTime"));
      const auto refuse = [&](const std::string& why) {
        if (raw.refusal.empty())
          raw.refusal = "OCEL JSON object change of " + *id + ": " + why + ", which a native OCEL cannot hold";
      };
      if (!raw_id->is_string()) refuse("its id " + repr(*raw_id) + " is no str, so pm4py gives it no object type");
      const auto* time = optional(change, "ocel:timestamp");
      if (time == nullptr || missing_value(*time)) {
        refuse("it has no time (pm4py keeps NaT)");
      } else if (!time->is_string()) {
        refuse("its time " + repr(*time) + " is no str");
      } else {
        raw.time = time->string();
        if (raw_times && readable_time(raw.time))
          refuse("pm4py keeps its time '" + raw.time + "' as text next to a time it cannot read");
      }
      const auto* field = optional(change, "ocel:field");
      if (field == nullptr || missing_value(*field)) {
        refuse("it names no field");
      } else if (!field->is_string()) {
        refuse("its field " + repr(*field) + " is no str");
      } else {
        raw.field = field->string();
        if (raw.field == "ocel:oid") {
          raw.value = *id;
        } else if (raw.field == "ocel:field") {
          raw.value = raw.field;
        } else if (raw.field == "ocel:type" || raw.field == "ocel:timestamp") {
          refuse("its field names the frame column " + raw.field);
        } else if (const auto* cell = optional(change, raw.field)) {
          // A field without its value keeps NaN (null natively).
          auto value = change_rows.cell(raw.field, *cell);
          if (!value.unrepresentable.empty()) refuse("value '" + raw.field + "' " + value.unrepresentable);
          else raw.value = std::move(value.value);
        }
      }
      // Round EN: the entry's other non-null keys are the row's other cells
      // (OcelObjectChange::cells); one native cannot hold stays a column
      // type only, as before.
      for (const auto& key : entry->keys) {
        if (key == "ocel:oid" || key == "ocel:type" || key == "ocel:timestamp" || key == "ocel:field" ||
            key == raw.field || key.starts_with("minera:"))
          continue;
        const auto& cell_json = change.at(key);
        if (missing_value(cell_json)) continue;
        auto value = change_rows.cell(key, cell_json);
        if (!value.unrepresentable.empty() || std::holds_alternative<std::nullptr_t>(value.value)) continue;
        raw.cells.push_back({key, std::move(value.value)});
      }
      changes.push_back(std::move(raw));
    }
  }

  // The frames' columns and texts.
  Column activities, types, relation_activities, relation_types;
  for (const auto& event : events) activities.observe(event.frame_activity);
  for (const auto& object : objects) types.observe(object.frame_type);
  for (const auto& relation : relations) {
    relation_activities.observe(events[relation.event].activity);
    relation_types.observe(objects[relation.object].type);
  }
  event_rows.finish();
  object_rows.finish();
  // Round EM: a transport's frame hints (OcelFrameHint, "minera:frameHints")
  // type its columns too: the dropped rows' value kinds, and their missing
  // cells (apply_transport_frame_hint).
  apply_transport_frame_hint(optional(root, "minera:frameHints"), "events", event_rows, false);
  apply_transport_frame_hint(optional(root, "minera:frameHints"), "objects", object_rows, false);
  const auto nonempty = [](const std::optional<std::string>& text) { return text && !text->empty(); };
  // A frame id: the row's own, or the override (missing drops the row; a
  // value that is no str is refused if its row remains).
  const auto frame_id = [](const Json* override_id, const std::string& own) -> std::optional<std::string> {
    if (override_id == nullptr) return own;
    if (override_id->is_string()) return override_id->string();
    if (missing_value(*override_id)) return std::nullopt;
    return "\x01" + repr(*override_id);
  };
  std::vector<std::optional<std::string>> event_activity(events.size()), event_frame_id(events.size()),
      relation_activity(events.size());
  std::vector<std::optional<std::string>> object_type(objects.size()), object_frame_id(objects.size()),
      relation_type(objects.size());
  for (std::size_t i = 0; i < events.size(); ++i) {
    event_activity[i] = column_str(*events[i].frame_activity, activities.kind());
    event_frame_id[i] = frame_id(events[i].frame_id, events[i].id);
    if (!events[i].relations.empty())
      relation_activity[i] = column_str(*events[i].activity, relation_activities.kind());
  }
  std::vector<bool> object_related(objects.size(), false);
  for (const auto& relation : relations) object_related[relation.object] = true;
  for (std::size_t i = 0; i < objects.size(); ++i) {
    object_type[i] = column_str(*objects[i].frame_type, types.kind());
    object_frame_id[i] = frame_id(objects[i].frame_id, objects[i].id);
    if (object_related[i])
      relation_type[i] = column_str(*objects[i].type, relation_types.kind());
  }

  // The reference's filtering (see above).
  listing.objects = !objects.empty();
  listing.relations = !relations.empty();
  std::vector<bool> relation_kept(relations.size());
  std::unordered_set<std::string> related_event_ids, related_object_ids;
  for (std::size_t r = 0; r < relations.size(); ++r) {
    const auto& relation = relations[r];
    relation_kept[r] = !events[relation.event].id.empty() && nonempty(relation_activity[relation.event]) &&
                       !objects[relation.object].id.empty() && nonempty(relation_type[relation.object]);
    if (!relation_kept[r]) continue;
    related_event_ids.insert(events[relation.event].id);
    related_object_ids.insert(objects[relation.object].id);
  }
  std::vector<bool> event_kept(events.size()), object_kept(objects.size());
  std::unordered_set<std::string> frame_event_ids, frame_object_ids;
  for (std::size_t i = 0; i < events.size(); ++i) {
    event_kept[i] = nonempty(event_frame_id[i]) && nonempty(event_activity[i]) &&
                    (keep_unrelated || related_event_ids.count(*event_frame_id[i]));
    if (event_kept[i]) frame_event_ids.insert(*event_frame_id[i]);
  }
  for (std::size_t i = 0; i < objects.size(); ++i) {
    object_kept[i] = nonempty(object_frame_id[i]) && nonempty(object_type[i]) &&
                     (keep_unrelated || related_object_ids.count(*object_frame_id[i]));
    if (object_kept[i]) frame_object_ids.insert(*object_frame_id[i]);
  }
  // When no relation keeps both its event and its object, the frames keep
  // no event and no object either (observed: an event whose only object the
  // objects frame drops stays when another relation remains, and goes with
  // the last one).
  const bool relation_remains = std::any_of(relations.begin(), relations.end(), [&, r = std::size_t{0}](const RawRelation& relation) mutable {
    const bool kept = relation_kept[r++];
    return kept && frame_event_ids.count(events[relation.event].id) && frame_object_ids.count(objects[relation.object].id);
  });
  if (!keep_unrelated && !relation_remains) {
    std::fill(event_kept.begin(), event_kept.end(), false);
    std::fill(object_kept.begin(), object_kept.end(), false);
  }
  std::unordered_set<std::string> kept_event_ids, kept_object_ids;
  for (std::size_t i = 0; i < events.size(); ++i) {
    if (!event_kept[i]) continue;
    if (*event_frame_id[i] != events[i].id)
      raise("OCEL JSON event " + events[i].id + " vmap entry ocel:eid " + repr(*events[i].frame_id) +
            ": pm4py puts it in the events frame next to the row it names, which a native OCEL cannot hold");
    kept_event_ids.insert(events[i].id);
  }
  for (std::size_t i = 0; i < objects.size(); ++i) {
    if (!object_kept[i]) continue;
    if (*object_frame_id[i] != objects[i].id)
      raise("OCEL JSON object " + objects[i].id + " ovmap entry ocel:oid " + repr(*objects[i].frame_id) +
            ": pm4py puts it in the objects frame next to the row it names, which a native OCEL cannot hold");
    kept_object_ids.insert(objects[i].id);
  }

  // The kept rows, in file order.
  for (std::size_t i = 0; i < objects.size(); ++i) {
    if (!object_kept[i]) continue;
    std::string bad;
    auto attributes = object_rows.attributes(i, bad);
    if (!bad.empty()) raise("OCEL JSON object " + objects[i].id + ": " + bad + ", which a native OCEL cannot hold");
    log.objects.push_back({objects[i].id, *object_type[i], std::move(attributes)});
    log.object_type_attributes.try_emplace(*object_type[i]);
  }
  for (std::size_t i = 0; i < events.size(); ++i) {
    if (!event_kept[i]) continue;
    auto& raw = events[i];
    if (raw.frame_time == nullptr && text_times)
      raise("OCEL JSON event " + raw.id + ": another event's vmap entry ocel:timestamp puts a text in the "
            "events frame's time column, which pm4py then keeps as Python objects; a native OCEL cannot hold that");
    if (raw.frame_time != nullptr)
      raise("OCEL JSON event " + raw.id + " vmap entry ocel:timestamp: pm4py puts the raw value " +
            repr(*raw.frame_time) + " in the events frame and keeps the parsed time in the relations "
            "frame, which a native OCEL cannot hold");
    std::string bad;
    auto attributes = event_rows.attributes(i, bad);
    if (!bad.empty()) raise("OCEL JSON event " + raw.id + ": " + bad + ", which a native OCEL cannot hold");
    log.events.push_back({raw.id, *event_activity[i], std::move(raw.time), std::move(attributes), raw.raw_time});
    log.event_type_attributes.try_emplace(*event_activity[i]);
  }
  // Round EX: the relations and O2O frames type their qualifier columns
  // from every row they are built from, the dropped ones too (an omap entry
  // without a typedOmap qualifier is missing there; see frame_qualifier); a
  // transport's typed qualifiers keep their own kinds.
  Column relation_qualifiers, link_qualifiers;
  for (const auto& relation : relations) relation_qualifiers.observe(relation.qualifier);
  for (const auto& link : links) link_qualifiers.observe(link.qualifier);
  const auto qualifier_of = [&](const Json* value, const Column& column,
                                const std::string& what) -> std::pair<std::string, OcelQualifierKind> {
    if (typed_qualifiers && value != nullptr && !value->is_null()) {
      if (const auto typed = typed_transport_qualifier(*value)) return *typed;
      if (!value->is_string())
        raise(what + ": qualifier " + repr(*value) + " is no str, which a native OCEL cannot hold");
      return {value->string(), OcelQualifierKind::kStr};
    }
    std::string refusal;
    auto result = frame_qualifier(value, column.kind(), refusal);
    if (!refusal.empty()) raise(what + ": qualifier " + refusal + ", which a native OCEL cannot hold");
    return result;
  };
  for (std::size_t r = 0; r < relations.size(); ++r) {
    const auto& relation = relations[r];
    const auto& event = events[relation.event];
    const auto& object = objects[relation.object];
    if (!relation_kept[r] || !kept_event_ids.count(event.id) || !kept_object_ids.count(object.id)) continue;
    if (*relation_activity[relation.event] != *event_activity[relation.event])
      raise("OCEL JSON event " + event.id + ": pm4py reads its activity as '" + *event_activity[relation.event] +
            "' in the events frame and '" + *relation_activity[relation.event] +
            "' in the relations frame (the columns' dtypes differ), which a native OCEL cannot hold");
    if (*relation_type[relation.object] != *object_type[relation.object])
      raise("OCEL JSON object " + object.id + ": pm4py reads its type as '" + *object_type[relation.object] +
            "' in the objects frame and '" + *relation_type[relation.object] +
            "' in the relations frame (the columns' dtypes differ), which a native OCEL cannot hold");
    auto [qualifier, qualifier_kind] =
        qualifier_of(relation.qualifier, relation_qualifiers, "OCEL JSON relation " + event.id + " - " + object.id);
    log.relations.push_back({event.id, object.id, std::move(qualifier), qualifier_kind});
  }

  // Round EY: the columns keep their dtypes when rows go (a transport's
  // come from its marker, see Ocel::relation_qualifier_dtype).
  if (!typed_qualifiers) {
    log.relation_qualifier_dtype = frame_qualifier_dtype(relation_qualifiers.kind());
    log.o2o_qualifier_dtype = filled_qualifier_dtype(
        link_qualifiers.kind(), std::any_of(links.begin(), links.end(), [](const auto& link) {
          return !link.source.empty() && (link.qualifier == nullptr || missing_value(*link.qualifier));
        }));
  }
  // Object links: the target column reads as str (an int target 1 names
  // object "1"); a link to an object the frames do not keep is dropped.
  listing.o2o = !links.empty();
  Column targets;
  for (const auto& link : links) targets.observe(&link.target);
  for (const auto& link : links) {
    const auto target = column_str(link.target, targets.kind());
    if (!target || !kept_object_ids.count(link.source) || !kept_object_ids.count(*target)) continue;
    auto [qualifier, qualifier_kind] =
        qualifier_of(link.qualifier, link_qualifiers, "OCEL JSON object link " + link.source + " - " + *target);
    log.o2o.push_back({link.source, *target, std::move(qualifier), qualifier_kind});
  }
  for (auto& change : changes) {
    if (!object_kept[change.object]) continue;
    if (!change.refusal.empty()) raise(change.refusal);
    // The change takes the objects frame's type cell before it reads as str.
    if (const auto* type = objects[change.object].frame_type; !type->is_string())
      raise("OCEL JSON object change of " + objects[change.object].id + ": pm4py gives it the object type " +
            repr(*type) + " as the file has it (no str), which a native OCEL cannot hold");
    log.object_changes.push_back({objects[change.object].id, *object_type[change.object], std::move(change.time),
                                  std::move(change.field), std::move(change.value), change.raw_time,
                                  std::move(change.cells)});
  }

  // Native extensions the reference ignores: event links (an entry native
  // cannot read is skipped).
  if (const auto* raw_e2e = optional(root, "ocel:e2e"); raw_e2e && raw_e2e->is_array()) {
    for (const auto& raw : raw_e2e->array()) {
      if (!raw.is_object()) continue;
      const auto* source = optional(raw.object(), "ocel:eid");
      const auto* target = optional(raw.object(), "ocel:eid_2");
      const auto* qualifier = optional(raw.object(), "ocel:qualifier");
      if (source == nullptr || target == nullptr || !source->is_string() || !target->is_string() ||
          (qualifier != nullptr && !qualifier->is_string()) || !kept_event_ids.count(source->string()) ||
          !kept_event_ids.count(target->string()))
        continue;
      log.e2e.push_back({source->string(), target->string(), qualifier == nullptr ? std::string{} : qualifier->string()});
    }
  }
  // Round EO: read_ocel_json builds its events and objects frames over every
  // entry of the file and drops rows afterwards (observed black-box), so the
  // frames keep the dropped rows' columns, in the file's order, and their
  // values' kinds: the frame hints (OcelFrameHint), as the other readers
  // record them. A transport brings its own.
  if (!keep_unrelated && optional(root, "minera:frameHints") == nullptr) {
    std::vector<ocel_frame::HintRow> extra_events, extra_objects;
    for (std::size_t i = 0; i < events.size(); ++i) {
      if (event_kept[i]) continue;
      std::string bad;
      extra_events.push_back({event_activity[i].value_or(std::string{}), event_rows.attributes(i, bad)});
    }
    for (std::size_t i = 0; i < objects.size(); ++i) {
      if (object_kept[i]) continue;
      std::string bad;
      extra_objects.push_back({object_type[i].value_or(std::string{}), object_rows.attributes(i, bad)});
    }
    ocel_frame::hint_extra_rows(log, extra_events, extra_objects, {});
    if (!extra_events.empty()) log.event_frame_hint.columns = event_rows.names;
    if (!extra_objects.empty()) log.object_frame_hint.columns = object_rows.names;
  }
  return log;
}

// The former classic reading, kept for exact filter inputs (literal names).
Ocel parse_classic(const Json::Object& root, bool literal_names, bool repeated_relations = false) {
  Ocel log; std::unordered_set<std::string> object_ids, event_ids; std::unordered_map<std::string, std::string> types;
  // Round EW: int and float qualifiers of a transport (see serialize_ocel_json).
  const auto* typed_marker = optional(root, "minera:typedQualifiers");
  const bool typed_qualifiers = typed_marker != nullptr && typed_marker->is_bool() && std::get<bool>(typed_marker->value);
  const auto qualifier_of = [&](const Json* value) -> std::pair<std::string, OcelQualifierKind> {
    if (value == nullptr) return {std::string(), OcelQualifierKind::kStr};
    if (typed_qualifiers)
      if (const auto typed = classic_json::typed_transport_qualifier(*value)) return *typed;
    return {str(*value, "ocel:qualifier"), OcelQualifierKind::kStr};
  };
  parse_classic_type_catalog(optional(root, "ocel:objectTypes"),
                             log.object_type_attributes, "ocel:objectTypes");
  parse_classic_type_catalog(optional(root, "ocel:eventTypes"),
                             log.event_type_attributes, "ocel:eventTypes");
  // Objects and events are read in document order; the stable timestamp
  // sort then keeps that order among equal instants.
  const auto& objects_json = required(root, "ocel:objects");
  const auto& objects = obj(objects_json, "ocel:objects");
  for (const auto& id : objects_json.keys) {
    const auto& raw = objects.at(id);
    if (!object_ids.insert(id).second) throw std::invalid_argument("duplicate object id");
    const auto& x = obj(raw, "object"); OcelObject o{id, str(required(x, "ocel:type"), "ocel:type"), {}}; attrs(optional(x, "ocel:ovmap"), o.attributes, "ocel:ovmap"); log.objects.push_back(std::move(o)); log.object_type_attributes.try_emplace(log.objects.back().type); types.emplace(id, log.objects.back().type);
    if (const auto* rs = optional(x, "ocel:o2o")) { if (!rs->is_array()) throw std::invalid_argument("ocel:o2o must be array"); for (const auto& r : rs->array()) { const auto& rr = obj(r, "ocel:o2o"); const auto [qualifier, kind] = qualifier_of(optional(rr, "ocel:qualifier")); log.o2o.push_back({id, str(required(rr, "ocel:oid"), "ocel:oid"), qualifier, kind}); } }
  }
  const auto& events_json = required(root, "ocel:events");
  const auto& events = obj(events_json, "ocel:events");
  for (const auto& id : events_json.keys) {
    const auto& raw = events.at(id);
    if (!event_ids.insert(id).second) throw std::invalid_argument("duplicate event id");
    const auto& x = obj(raw, "event"); OcelEvent e{id, str(required(x, "ocel:activity"), "ocel:activity"), str(required(x, "ocel:timestamp"), "ocel:timestamp"), {}}; attrs(optional(x, "ocel:vmap"), e.attributes, "ocel:vmap"); log.events.push_back(std::move(e)); log.event_type_attributes.try_emplace(log.events.back().activity);
    const auto* omap = optional(x, "ocel:omap"); if (!omap || !omap->is_array()) throw std::invalid_argument("ocel:omap must be array");
    std::unordered_set<std::string> seen;
    std::map<std::string,std::vector<std::size_t>> occurrences;
    for (const auto& oidj : omap->array()) {
      const auto oid = str(oidj, "ocel:omap");
      // The reference reader drops a relation to an object the document does
      // not list (observed black-box, as for every OCEL 2.0 reader); the
      // event goes too when it keeps no relation.
      if (!types.count(oid)) continue;
      const bool first=seen.insert(oid).second;
      if(literal_names || repeated_relations || first){occurrences[oid].push_back(log.relations.size());log.relations.push_back({id,oid,""});}
    }
    if (const auto* typed = optional(x, "ocel:typedOmap")) {
      if (!typed->is_array()) throw std::invalid_argument("ocel:typedOmap must be array");
      std::map<std::string,std::size_t> assigned;
      for (const auto& r : typed->array()) {
        const auto& rr=obj(r,"ocel:typedOmap");
        const auto oid=str(required(rr,"ocel:oid"),"ocel:oid");
        // A qualifier only qualifies a relation the omap lists; one for any
        // other object is ignored, as the reference reader ignores it.
        if(!seen.count(oid))continue;
        const auto [qualifier, kind] = qualifier_of(optional(rr, "ocel:qualifier"));
        if(literal_names || repeated_relations){
          const auto index=assigned[oid]++;
          if(index<occurrences[oid].size()){log.relations[occurrences[oid][index]].qualifier=qualifier;log.relations[occurrences[oid][index]].qualifier_kind=kind;}
          else log.relations.push_back({id,oid,qualifier,kind});
        }else{
          // Event ids are unique, so this event's relations to oid are the
          // ones its omap just added (one each without literal names).
          for(const auto index:occurrences[oid]){log.relations[index].qualifier=qualifier;log.relations[index].qualifier_kind=kind;}
        }
      }
    }
  }
  if (const auto* raw_e2e = optional(root, "ocel:e2e")) {
    if (!raw_e2e->is_array())
      throw std::invalid_argument("ocel:e2e must be array");
    for (const auto& raw : raw_e2e->array()) {
      const auto& relation = obj(raw, "ocel:e2e");
      const auto* source = optional(relation, "ocel:eid");
      const auto* target = optional(relation, "ocel:eid_2");
      if (source == nullptr || target == nullptr)
        throw std::invalid_argument("ocel:e2e requires source and target event ids");
      const auto source_id = str(*source, "ocel:eid");
      const auto target_id = str(*target, "ocel:eid_2");
      if (!event_ids.count(source_id) || !event_ids.count(target_id))
        throw std::invalid_argument("ocel:e2e references unknown event");
      const auto* qualifier = optional(relation, "ocel:qualifier");
      log.e2e.push_back({source_id, target_id,
                         qualifier == nullptr ? std::string{} : str(*qualifier, "ocel:qualifier")});
    }
  }
  // A change has its object's type, as the reference reader gives it
  // whatever the change's own "ocel:type" says (observed black-box).
  if (const auto* ch = optional(root, "ocel:objectChanges")) { if (!ch->is_array()) throw std::invalid_argument("ocel:objectChanges must be array"); for (const auto& r : ch->array()) { const auto& rr = obj(r, "ocel:objectChanges"); OcelObjectChange c; c.object_id = str(required(rr, "ocel:oid"), "ocel:oid"); if (!types.count(c.object_id)) continue; /* the reference drops it */ if (const auto* type = optional(rr, "ocel:type")) (void)str(*type, "ocel:type"); c.object_type = types[c.object_id]; c.timestamp = str(required(rr, "ocel:timestamp"), "ocel:timestamp"); c.field = str(required(rr, "ocel:field"), "ocel:field"); c.value = scalar(required(rr, c.field), c.field); log.object_changes.push_back(std::move(c)); } }
  for (const auto& category : {"ocel:global-log", "ocel:global-event", "ocel:global-object"}) if (const auto* g = optional(root, category)) { for (const auto& [key, value] : obj(*g, category)) { if (value.is_scalar()) log.globals[std::string(category) + "." + key] = scalar(value, key); } }
  // read_ocel_json keeps the scopes as the file lists them, empty or not.
  log.global_event_scope = optional(root, "ocel:global-event") != nullptr;
  log.global_object_scope = optional(root, "ocel:global-object") != nullptr;
  // The reference readers drop an object link to an unlisted object.
  std::erase_if(log.o2o, [&](const OcelO2oRelation& r) { return !types.count(r.target_id); });
  return log;
}

// Round EX: `typed_qualifiers` (a transport's "minera:typedQualifiers")
// keeps each qualifier's JSON kind; `repeated_relations` keeps every
// relation of an event to one object (the reader keeps the last one).
Ocel parse_standard(const Json::Object& root, bool typed_qualifiers, bool repeated_relations) {
  Ocel log;
  std::unordered_map<std::string, std::string> types;
  StandardAttributeTypes object_attribute_types;
  StandardAttributeTypes event_attribute_types;
  parse_standard_type_table(optional(root, "eventTypes"), event_attribute_types,
                            "eventTypes");
  parse_standard_type_table(optional(root, "objectTypes"), object_attribute_types,
                            "objectTypes");
  log.object_type_attributes = object_attribute_types;
  log.event_type_attributes = event_attribute_types;
  struct StandardAttribute {
    std::string name;
    OcelValue value;
    bool has_time = false;
    std::string time;
  };
  // A change time of a JSON type other than str or null (refused after the
  // walk, see the objects below).
  bool untexted_change_time = false;
  const auto& event_array = standard_key(root, "events");
  if (!event_array.is_array())
    throw std::invalid_argument("events must be array");
  // An id listed twice is one event, as the reference reader keeps it
  // (observed black-box): in the place of its first listing with everything
  // of its last one, relations included.
  std::unordered_map<std::string, std::size_t> event_index;
  std::vector<std::vector<OcelRelation>> event_relations;
  // Round EX: each relation's (and object link's) qualifier as the file has
  // it, typed after the walk (see the relations below).
  std::vector<std::vector<const Json*>> event_qualifiers;
  // The Python type of each event time that is no str (by event id, the
  // last listing's), raised when the times are read.
  std::unordered_map<std::string, std::string> untexted_times;
  for (const auto& raw : event_array.array()) {
    const auto& x = obj(raw, "events[]");
    const auto activity = str(standard_key(x, "type"), "type");
    const auto& raw_time = standard_key(x, "time");
    const bool texted_time = raw_time.is_string();
    OcelEvent event{{}, activity, texted_time ? raw_time.string() : std::string{}, {}};
    if (const auto* aa = optional(x, "attributes"); standard_truthy(aa)) {
      if (!aa->is_array())
        throw std::invalid_argument("event attributes must be array");
      for (const auto& ar : aa->array()) {
        const auto& a = obj(ar, "attributes[]");
        const auto name = str(standard_key(a, "name"), "name");
        event.attributes.push_back(
            {name, standard_scalar(
                       standard_key(a, "value"), "value",
                       standard_attribute_type(event_attribute_types, activity,
                                               name))});
      }
    }
    std::vector<OcelRelation> relations;
    std::vector<const Json*> qualifiers;
    if (const auto* rels = optional(x, "relationships"); standard_truthy(rels)) {
      if (!rels->is_array())
        throw std::invalid_argument("event relationships must be array");
      for (const auto& r : rels->array()) {
        const auto& y = obj(r, "relationship");
        const auto oid = str(standard_key(y, "objectId"), "objectId");
        qualifiers.push_back(&standard_key(y, "qualifier"));
        relations.push_back({{}, oid, {}});
      }
    }
    const auto id = str(standard_key(x, "id"), "id");
    event.id = id;
    for (auto& relation : relations) relation.event_id = id;
    if (texted_time) untexted_times.erase(id);
    else untexted_times.insert_or_assign(id, python_json_type_name(raw_time));
    const auto [slot, added] = event_index.try_emplace(id, log.events.size());
    if (added) {
      log.events.push_back(std::move(event));
      event_relations.push_back(std::move(relations));
      event_qualifiers.push_back(std::move(qualifiers));
    } else {
      log.events[slot->second] = std::move(event);
      event_relations[slot->second] = std::move(relations);
      event_qualifiers[slot->second] = std::move(qualifiers);
    }
  }
  const auto& object_array = standard_key(root, "objects");
  if (!object_array.is_array())
    throw std::invalid_argument("objects must be array");
  // An id listed twice (under another type, say) is one object, as the
  // reference reader keeps it (observed black-box): in the place of its
  // first listing with the type, attribute values and object relationships
  // of its last one, while the attribute changes of every listing stay, in
  // document order, each read with its own listing's declared types and all
  // under the last type.
  std::unordered_map<std::string, std::size_t> object_index;
  std::vector<std::vector<OcelO2oRelation>> object_o2o;
  std::vector<std::vector<const Json*>> object_qualifiers;
  for (const auto& raw : object_array.array()) {
    const auto& x = obj(raw, "objects[]");
    // An empty id or type is dropped with drop_empty_ocel_names.
    const auto type = str(standard_key(x, "type"), "type");
    OcelObject object{{}, type, {}};
    std::vector<std::pair<std::string, std::vector<StandardAttribute>>> grouped;
    if (const auto* aa = optional(x, "attributes"); standard_truthy(aa)) {
      if (!aa->is_array())
        throw std::invalid_argument("object attributes must be array");
      // Grouped by name in first-appearance order (the reference's frame
      // columns follow first appearance).
      std::unordered_map<std::string, std::size_t> group_index;
      for (const auto& ar : aa->array()) {
        const auto& a = obj(ar, "attributes[]");
        const auto& raw_value = standard_key(a, "value");
        // An empty name is a name: read_ocel2_json keeps that column.
        const auto name = str(standard_key(a, "name"), "name");
        StandardAttribute entry{name,
                                standard_scalar(
                                    raw_value, "value",
                                    standard_attribute_type(object_attribute_types,
                                                            type, name))};
        // Round EJ: read_ocel2_json takes no time for an attribute's first
        // entry (the static value), and reads a change's missing or null
        // time as NaT, kept here as the empty text (observed
        // black-box). A time of another JSON type stays refused (EI1:
        // values of the wrong JSON type), after the walk.
        if (const auto* time = optional(a, "time"); time != nullptr && !time->is_null()) {
          if (time->is_string()) {
            entry.has_time = true;
            entry.time = str(*time, "time");
          }
        } else {
          entry.has_time = true;
          entry.time.clear();
        }
        const auto [slot, added] = group_index.try_emplace(name, grouped.size());
        if (added) grouped.emplace_back(name, std::vector<StandardAttribute>{});
        grouped[slot->second].second.push_back(std::move(entry));
      }
    }
    std::vector<OcelO2oRelation> o2o;
    std::vector<const Json*> link_qualifiers;
    if (const auto* rr = optional(x, "relationships"); standard_truthy(rr)) {
      if (!rr->is_array())
        throw std::invalid_argument("object relationships must be array");
      for (const auto& r : rr->array()) {
        const auto& y = obj(r, "relationship");
        const auto target = str(standard_key(y, "objectId"), "objectId");
        link_qualifiers.push_back(&standard_key(y, "qualifier"));
        o2o.push_back({{}, target, {}});
      }
    }
    const auto id = str(standard_key(x, "id"), "id");
    object.id = id;
    for (auto& relation : o2o) relation.source_id = id;
    types.insert_or_assign(id, type);
    for (auto& [name, entries] : grouped) {
      if (entries.empty()) continue;
      object.attributes.push_back({name, entries.front().value});
      for (std::size_t i = 1; i < entries.size(); ++i) {
        if (!entries[i].has_time) {
          untexted_change_time = true;
          continue;
        }
        log.object_changes.push_back(
            {id, type, entries[i].time, name, entries[i].value});
      }
    }
    const auto [slot, added] = object_index.try_emplace(id, log.objects.size());
    if (added) {
      log.objects.push_back(std::move(object));
      object_o2o.push_back(std::move(o2o));
      object_qualifiers.push_back(std::move(link_qualifiers));
    } else {
      log.objects[slot->second] = std::move(object);
      object_o2o[slot->second] = std::move(o2o);
      object_qualifiers[slot->second] = std::move(link_qualifiers);
    }
  }
  // Round EX: read_ocel2_json keeps a JSON number or bool qualifier, typed
  // by its frame (classic_json::frame_qualifier; observed black-box, probes
  // ex p/p1.py, p3.py..p5.py): the O2O frame is built from every link of
  // each object's last listing (dangling links, links of objects it then
  // drops), the relations frame from each event's last listing without its
  // relations to unlisted objects, one per object (the last); events and
  // objects it then drops (empty names) count too. A list, dict, integer
  // past 64 bits or infinite float is refused. A transport's typed
  // qualifiers keep their own kinds.
  const auto type_qualifiers = [&](auto& rows, const std::vector<const Json*>& values,
                                   const std::vector<bool>& counted, const classic_json::Column& column,
                                   const char* what) {
    for (std::size_t i = 0; i < rows.size(); ++i) {
      if (!counted[i]) continue;
      const Json* value = values[i];
      if (typed_qualifiers && !value->is_null()) {
        if (const auto typed = classic_json::typed_transport_qualifier(*value)) {
          std::tie(rows[i].qualifier, rows[i].qualifier_kind) = *typed;
          continue;
        }
        rows[i].qualifier = str(*value, "qualifier");
        continue;
      }
      std::string refusal;
      std::tie(rows[i].qualifier, rows[i].qualifier_kind) = classic_json::frame_qualifier(value, column.kind(), refusal);
      if (!refusal.empty())
        throw std::invalid_argument(std::string("OCEL JSON ") + what + " qualifier " + refusal +
                                    ", which a native OCEL cannot hold");
    }
  };
  for (auto& change : log.object_changes) change.object_type = types.at(change.object_id);
  for (const auto& object : log.objects) log.object_type_attributes.try_emplace(object.type);
  // The times are read after the walk, in event order: a time that is no
  // str raises there, a text fromisoformat cannot read before it raises too.
  if (!untexted_times.empty())
    for (const auto& event : log.events) {
      if (const auto found = untexted_times.find(event.id); found != untexted_times.end())
        throw std::invalid_argument("'" + found->second + "' object has no attribute 'endswith'");
      iso_wall_microseconds(event.timestamp);
    }
  if (untexted_change_time) throw std::invalid_argument("OCEL field must be string: time");
  {
    classic_json::Column column;
    for (const auto& values : object_qualifiers)
      for (const auto* value : values) column.observe(value);
    for (std::size_t o = 0; o < object_o2o.size(); ++o)
      type_qualifiers(object_o2o[o], object_qualifiers[o], std::vector<bool>(object_o2o[o].size(), true), column,
                      "object relationship");
    // Round EY: the column keeps its dtype when rows go (Ocel::o2o_qualifier_dtype).
    bool filled = false;
    for (std::size_t o = 0; o < object_qualifiers.size(); ++o)
      if (!log.objects[o].id.empty())
        for (const auto* value : object_qualifiers[o]) filled = filled || classic_json::missing_value(*value);
    if (!typed_qualifiers) log.o2o_qualifier_dtype = classic_json::filled_qualifier_dtype(column.kind(), filled);
  }
  for (auto& relations : object_o2o)
    for (auto& relation : relations) log.o2o.push_back(std::move(relation));
  // read_ocel2_json drops a relation to an unlisted object (and the event
  // when it keeps none), observed black-box.
  for (std::size_t e = 0; e < event_relations.size(); ++e) {
    auto& relations = event_relations[e];
    auto& qualifiers = event_qualifiers[e];
    std::size_t kept = 0;
    for (std::size_t i = 0; i < relations.size(); ++i) {
      if (!types.count(relations[i].object_id)) continue;
      if (kept != i) {
        relations[kept] = std::move(relations[i]);
        qualifiers[kept] = qualifiers[i];
      }
      ++kept;
    }
    relations.resize(kept);
    qualifiers.resize(kept);
  }
  // Round EX: the relations frame's qualifier column (see the O2O frame
  // above); a relation the reader replaces by a later one to the same
  // object is no row of it.
  {
    std::vector<std::vector<bool>> counted(event_relations.size());
    classic_json::Column column;
    for (std::size_t e = 0; e < event_relations.size(); ++e) {
      const auto& relations = event_relations[e];
      counted[e].assign(relations.size(), repeated_relations);
      if (!repeated_relations) {
        std::unordered_set<std::string> seen;
        for (std::size_t i = relations.size(); i-- > 0;)
          if (seen.insert(relations[i].object_id).second) counted[e][i] = true;
      }
      for (std::size_t i = 0; i < relations.size(); ++i)
        if (counted[e][i]) column.observe(event_qualifiers[e][i]);
    }
    for (std::size_t e = 0; e < event_relations.size(); ++e)
      type_qualifiers(event_relations[e], event_qualifiers[e], counted[e], column, "event relationship");
    if (!typed_qualifiers) log.relation_qualifier_dtype = classic_json::frame_qualifier_dtype(column.kind());
  }
  for (auto& relations : event_relations)
    for (auto& relation : relations) log.relations.push_back(std::move(relation));
  for (const auto& event : log.events) log.event_type_attributes.try_emplace(event.activity);
  const Json* raw_e2e = optional(root, "eventRelationships");
  if (raw_e2e == nullptr) raw_e2e = optional(root, "e2e");
  if (raw_e2e != nullptr) {
    if (!raw_e2e->is_array())
      throw std::invalid_argument("eventRelationships must be array");
    for (const auto& raw : raw_e2e->array()) {
      const auto& relation = obj(raw, "eventRelationships[]");
      const auto* source = optional(relation, "sourceEventId");
      if (source == nullptr) source = optional(relation, "source");
      const auto* target = optional(relation, "targetEventId");
      if (target == nullptr) target = optional(relation, "target");
      if (source == nullptr || target == nullptr)
        throw std::invalid_argument("eventRelationships requires sourceEventId and targetEventId");
      const auto source_id = str(*source, "sourceEventId");
      const auto target_id = str(*target, "targetEventId");
      if (!event_index.count(source_id) || !event_index.count(target_id))
        throw std::invalid_argument("eventRelationships references unknown event");
      const auto* qualifier = optional(relation, "qualifier");
      log.e2e.push_back({source_id, target_id,
                         qualifier == nullptr ? std::string{} : str(*qualifier, "qualifier")});
    }
  }
  // read_ocel2_json declares every global scope, empty.
  log.global_event_scope = true;
  log.global_object_scope = true;
  if (const auto* globals = optional(root, "globals")) {
    for (const auto& [key, value] : obj(*globals, "globals")) {
      log.globals[key] = scalar(value, key);
    }
  }
  // Round EO: read_ocel2_json's O2O frame is float64 when the file lists no
  // object link at all, str once it lists one, dangling or not (observed
  // black-box; see Ocel::untyped_o2o).
  log.untyped_o2o = log.o2o.empty();
  // The reference readers drop an object link to an unlisted object.
  std::erase_if(log.o2o, [&](const OcelO2oRelation& r) { return !types.count(r.target_id); });
  // Round EJ: read_ocel2_json builds its events, objects and changes frames
  // from every listed row before it drops any (events and objects without
  // relations, empty names), so a column of ints with a missing cell in any
  // of those rows is float64, the ints floats, which the writers then
  // declare and print as floats (observed black-box). The rows that stay may
  // all hold a value, so the ints become floats here. A change row holds
  // only its own field.
  const auto floatify = [](std::size_t rows, const ocel_frame::Columns& columns, auto&& for_each_value) {
    (void)rows;
    for_each_value([&](const std::string& key, OcelValue& value) {
      if (const auto* integer = std::get_if<std::int64_t>(&value);
          integer != nullptr && columns.dtype(key) == ocel_frame::Dtype::kFloat64)
        value = static_cast<double>(*integer);
    });
  };
  {
    ocel_frame::Columns columns;
    for (const auto& event : log.events) columns.observe_row(event.attributes);
    floatify(log.events.size(), columns, [&](auto&& visit) {
      for (auto& event : log.events)
        for (auto& attribute : event.attributes) visit(attribute.key, attribute.value);
    });
  }
  {
    ocel_frame::Columns columns;
    for (const auto& object : log.objects) columns.observe_row(object.attributes);
    floatify(log.objects.size(), columns, [&](auto&& visit) {
      for (auto& object : log.objects)
        for (auto& attribute : object.attributes) visit(attribute.key, attribute.value);
    });
  }
  {
    ocel_frame::Columns columns;
    for (const auto& change : log.object_changes) columns.observe_row({{change.field, change.value}});
    floatify(log.object_changes.size(), columns, [&](auto&& visit) {
      for (auto& change : log.object_changes) visit(change.field, change.value);
    });
  }
  // Round EO: read_ocel2_json builds its changes frame from records of an
  // object id, type, "ocel:field", the field's value and "ocel:timestamp",
  // so its field column stands first and its time column after the first
  // change's field (observed black-box, g52_0223): write_ocel2_sqlite's type
  // tables show it for a field without a static value (OcelFrameHint).
  if (!log.object_changes.empty()) {
    ocel_frame::Columns columns;
    for (const auto& change : log.object_changes) columns.observe_change(change);
    log.change_frame_hint.columns = columns.names();
    log.change_frame_hint.field_at = 0;
    log.change_frame_hint.time_at = 2;
  }
  return log;
}

void write_string(std::ostringstream& out, std::string_view s) { out << '"'; for (unsigned char c : s) { switch (c) { case '"': out << "\\\""; break; case '\\': out << "\\\\"; break; case '\b': out << "\\b"; break; case '\f': out << "\\f"; break; case '\n': out << "\\n"; break; case '\r': out << "\\r"; break; case '\t': out << "\\t"; break; default: if (c < 0x20) { out << "\\u00" << "0123456789abcdef"[(c >> 4) & 15] << "0123456789abcdef"[c & 15]; } else out << static_cast<char>(c); } } out << '"'; }
// An OCDFG edge's key in its object type's map: "source->target". Activity
// names may hold "->" themselves, so two edges can share that text (a ->
// "b->c" and "a->b" -> c); a repeat gets "#2", "#3", ... so the JSON object
// keeps every edge. Readers take the pair from the edge's own fields.
std::string ocdfg_edge_key(std::set<std::string>& used, const std::string& source,
                           const std::string& target) {
  const std::string base = source + "->" + target;
  std::string key = base;
  for (std::size_t n = 2; !used.insert(key).second; ++n) key = base + "#" + std::to_string(n);
  return key;
}
void write_value(std::ostringstream& out, const OcelValue& value) {
  if (std::holds_alternative<std::nullptr_t>(value)) {
    out << "null";
  } else if (const auto* bool_value = std::get_if<bool>(&value)) {
    out << (*bool_value ? "true" : "false");
  } else if (const auto* int_value = std::get_if<std::int64_t>(&value)) {
    out << *int_value;
  } else if (const auto* double_value = std::get_if<double>(&value)) {
    // Python's repr, as the reference's json writer prints a float (the
    // shortest text that reads back as the same double); JSON has no NaN.
    if (std::isfinite(*double_value)) out << filtering_float_repr(*double_value);
    else out << "null";
  } else {
    write_string(out, std::get<std::string>(value));
  }
}
// A value as write_ocel2_json prints its frame cell: an integer in a float64
// column (one some row lacks, or one holding floats too) prints as a float.
// Round EK: a datetime (`date`) prints as its column holds it: "...Z" in a
// datetime64 column (fraction kept, declared), its isoformat in an object
// one (observed black-box).
void write_frame_value(std::ostringstream& out, const OcelValue& value,
                       ocel_frame::Dtype dtype, bool date = false) {
  if (date) {
    const auto& text = std::get<std::string>(value);
    write_string(out, dtype == ocel_frame::Dtype::kDatetime ? ocel_frame::datetime64_json_text(text)
                                                            : ocel_frame::object_datetime_text(text));
    return;
  }
  if (const auto* int_value = std::get_if<std::int64_t>(&value);
      int_value != nullptr && dtype == ocel_frame::Dtype::kFloat64) {
    write_value(out, OcelValue(static_cast<double>(*int_value)));
    return;
  }
  write_value(out, value);
}
// Keys in first-appearance order with the last value of a repeated key, as a
// Python dict built from the attributes keeps them.
void write_attrs(std::ostringstream& out, const std::vector<OcelAttribute>& attrs) {
  std::vector<std::pair<std::string_view, const OcelValue*>> ordered;
  std::unordered_map<std::string_view, std::size_t> index;
  for (const auto& a : attrs) {
    const auto [slot, added] = index.try_emplace(a.key, ordered.size());
    if (added) ordered.emplace_back(a.key, &a.value);
    else ordered[slot->second].second = &a.value;
  }
  out << '{'; bool first = true;
  for (const auto& [k, v] : ordered) { if (!first) out << ','; first = false; write_string(out, k); out << ':'; write_value(out, *v); }
  out << '}';
}

std::string classic_value_type(const OcelValue& value) {
  if (std::holds_alternative<bool>(value)) return "boolean";
  if (std::holds_alternative<std::int64_t>(value)) return "int";
  if (std::holds_alternative<double>(value)) return "float";
  return "string";
}

void collect_classic_types(const Ocel& log, OcelTypeCatalog& object_types,
                           OcelTypeCatalog& event_types) {
  object_types = log.object_type_attributes;
  event_types = log.event_type_attributes;
  for (const auto& object : log.objects) {
    auto& attributes = object_types[object.type];
    for (const auto& attribute : object.attributes) {
      auto& declared = attributes[attribute.key];
      if (declared.empty()) declared = classic_value_type(attribute.value);
    }
  }
  for (const auto& change : log.object_changes) {
    // Round EL: a change without a value declares no column (an OCEL 2.0
    // SQLite change of '' or of a field its table lacks: pm4py's changes
    // frame has no such column, see write_ocel_xml's KeyError).
    auto& attributes = object_types[change.object_type];
    if (std::holds_alternative<std::nullptr_t>(change.value) && !attributes.count(change.field)) continue;
    auto& declared = attributes[change.field];
    if (declared.empty()) declared = classic_value_type(change.value);
  }
  for (const auto& event : log.events) {
    auto& attributes = event_types[event.activity];
    for (const auto& attribute : event.attributes) {
      auto& declared = attributes[attribute.key];
      if (declared.empty()) declared = classic_value_type(attribute.value);
    }
  }
}

void write_classic_type_catalog(std::ostringstream& out,
                                const OcelTypeCatalog& catalog) {
  out << '{';
  bool first_type = true;
  for (const auto& [type, attributes] : catalog) {
    if (!first_type) out << ',';
    first_type = false;
    write_string(out, type);
    out << ':';
    out << '{';
    bool first_attribute = true;
    for (const auto& [name, declared_type] : attributes) {
      if (!first_attribute) out << ',';
      first_attribute = false;
      write_string(out, name);
      out << ':';
      write_string(out, declared_type.empty() ? "string" : declared_type);
    }
    out << '}';
  }
  out << '}';
}

// pm4py.write_ocel2_json, write_ocel2_xml and write_ocel_json (objects) drop
// an attribute whose name starts with "ocel:" (an OCEL CSV column such as
// "ocel:foo" or "ocel:eid.1"), which their frames take for a reserved column
// (observed black-box).
bool reference_drops_attribute(std::string_view key) { return key.starts_with("ocel:"); }

// write_ocel2_json declares each attribute as its frame column holds it, as
// write_ocel2_xml does: "float" for a float64 column (an integer attribute
// some object or event lacks is one), "date" for a timestamp attribute,
// otherwise "string" (also integer and boolean columns); its reader then casts
// the values to the declared type (observed black-box). "ocel:"-prefixed
// attributes are dropped (reference_drops_attribute).
using StandardTypeTable = ocel_frame::TypeDeclarations;

void collect_standard_types(const Ocel& log, StandardTypeTable& object_types,
                            StandardTypeTable& event_types, bool with_catalog = false) {
  ocel_frame::reference_type_declarations(log, object_types, event_types, with_catalog);
}

void write_standard_type_table(std::ostringstream& out,
                               const StandardTypeTable& types) {
  out << '[';
  bool first_type = true;
  for (const auto& [type, attributes] : types) {
    if (!first_type) out << ',';
    first_type = false;
    out << "{\"name\":";
    write_string(out, type);
    out << ",\"attributes\":[";
    bool first_attribute = true;
    for (const auto& [name, declared_type] : attributes) {
      if (!first_attribute) out << ',';
      first_attribute = false;
      out << "{\"name\":";
      write_string(out, name);
      out << ",\"type\":";
      write_string(out, declared_type.empty() ? "string" : declared_type);
      out << '}';
    }
    out << "]}";
  }
  out << ']';
}

// Round EW: an int or float qualifier (OcelQualifierKind) is a JSON number,
// as pm4py's JSON writers write it (observed black-box, probe ew q/wq.py);
// round EX: a bool one a JSON bool (probe ex p/wj.py).
void write_qualifier(std::ostringstream& out, const std::string& text, OcelQualifierKind kind) {
  if (kind == OcelQualifierKind::kBool) out << (text == "True" ? "true" : "false");
  else if (kind != OcelQualifierKind::kStr) out << text;
  else write_string(out, text);
}

// `relations` are one source object's o2o relations in log order.
void write_o2o(std::ostringstream& out, const std::vector<const OcelO2oRelation*>& relations) {
  out << '[';
  bool first = true;
  for (const auto* relation : relations) {
    if (!first) out << ',';
    first = false;
    out << "{\"ocel:oid\":";
    write_string(out, relation->target_id);
    out << ",\"ocel:qualifier\":";
    write_qualifier(out, relation->qualifier, relation->qualifier_kind);
    out << '}';
  }
  out << ']';
}

void write_e2e(std::ostringstream& out, const Ocel& log,
               bool standard_names) {
  std::vector<const OcelE2eRelation*> relations;
  relations.reserve(log.e2e.size());
  for (const auto& relation : log.e2e) relations.push_back(&relation);
  out << '[';
  for (std::size_t i = 0; i < relations.size(); ++i) {
    if (i != 0) out << ',';
    const auto& relation = *relations[i];
    if (standard_names) {
      out << "{\"sourceEventId\":";
      write_string(out, relation.source_id);
      out << ",\"targetEventId\":";
      write_string(out, relation.target_id);
      out << ",\"qualifier\":";
      write_string(out, relation.qualifier);
      out << '}';
    } else {
      out << "{\"ocel:eid\":";
      write_string(out, relation.source_id);
      out << ",\"ocel:eid_2\":";
      write_string(out, relation.target_id);
      out << ",\"ocel:qualifier\":";
      write_string(out, relation.qualifier);
      out << '}';
    }
  }
  out << ']';
}

void write_global_category(std::ostringstream& out, const Ocel& log,
                           std::string_view category) {
  std::map<std::string, const OcelValue*> values;
  const std::string prefix = std::string(category) + ".";
  for (const auto& [key, value] : log.globals) {
    if (key.rfind(prefix, 0) == 0) values.emplace(key.substr(prefix.size()), &value);
  }
  // write_ocel_json fills an undeclared event or object scope with its
  // defaults (an OCEL read from OCEL 2.0 XML, SQLite, CSV or OCEL 1.0 XML).
  if (values.empty() && category == "ocel:global-event" && !log.global_event_scope) {
    out << "{\"ocel:activity\":\"__INVALID__\"}";
    return;
  }
  if (values.empty() && category == "ocel:global-object" && !log.global_object_scope) {
    out << "{\"ocel:type\":\"__INVALID__\"}";
    return;
  }
  out << '{';
  bool first = true;
  for (const auto& [key, value] : values) {
    if (!first) out << ',';
    first = false;
    write_string(out, key);
    out << ':';
    write_value(out, *value);
  }
  out << '}';
}

// A change time pandas holds as NaT: the empty text (see
// ocel_frame::nat_time_text, an OCEL 2.0 JSON change without a time) and
// "NaT" as written.
bool nat_change_time(std::string_view text) { return text.empty() || text == "NaT"; }

const char* raw_time_name(OcelRawTime kind) {
  switch (kind) {
    case OcelRawTime::kStr: return "str";
    case OcelRawTime::kInt: return "int";
    case OcelRawTime::kFloat: return "float";
    case OcelRawTime::kNaN: return "nan";
    case OcelRawTime::kNull: return "none";
    default: return "";
  }
}

// A raw time as a JSON value: a str as a string, an int or float as the
// number Python's json writes (the text holds its digits or repr; a float
// repr without a dot or exponent cannot occur), NaN as NaN.
void write_raw_time(std::ostringstream& out, OcelRawTime kind, const std::string& text) {
  if (kind == OcelRawTime::kStr) write_string(out, text);
  else if (kind == OcelRawTime::kNaN) out << "NaN";
  else if (kind == OcelRawTime::kNull) out << "null";
  else if (text == "inf") out << "Infinity";
  else if (text == "-inf") out << "-Infinity";
  else out << text;
}

// Round EJ: write_ocel_json leaves out a NaT change time (observed
// black-box); the transport keeps it.
void write_object_changes(std::ostringstream& out, const Ocel& log, bool transport_markers) {
  std::vector<const OcelObjectChange*> changes;
  changes.reserve(log.object_changes.size());
  for (const auto& change : log.object_changes) changes.push_back(&change);
  std::unordered_map<std::string_view, std::string_view> object_type_of;
  if (!transport_markers)
    for (const auto& object : log.objects) object_type_of.emplace(object.id, object.type);
  const auto owner_of = [&](const OcelObjectChange& change) {
    std::string owner = change.object_type;
    if (owner.empty())
      if (const auto found = object_type_of.find(change.object_id); found != object_type_of.end())
        owner = std::string(found->second);
    return owner;
  };
  // Round EO: a download prints an int of a float64 changes frame column as
  // a float (observed black-box: read_ocel2_sqlite's union of the type
  // tables, see OcelFrameHint); the transport keeps the value.
  ocel_frame::Columns change_frame;
  if (!transport_markers) {
    change_frame.seed(log.change_frame_hint);
    for (const auto* change : changes) {
      change_frame.add_rows(1);
      change_frame.observe_change(*change, log.object_type_attributes, owner_of(*change));
    }
  }
  const auto write_cell = [&](const std::string& key, const OcelValue& value) {
    if (const auto* integer = std::get_if<std::int64_t>(&value);
        integer != nullptr && !transport_markers && change_frame.dtype(key) == ocel_frame::Dtype::kFloat64)
      write_value(out, OcelValue(static_cast<double>(*integer)));
    else
      write_value(out, value);
  };
  out << '[';
  bool first = true;
  for (const auto* change : changes) {
    if (!first) out << ',';
    first = false;
    out << "{\"ocel:oid\":";
    write_string(out, change->object_id);
    out << ",\"ocel:type\":";
    write_string(out, change->object_type);
    if (transport_markers) {
      out << ",\"ocel:timestamp\":";
      write_string(out, change->timestamp);
      if (change->raw_time != OcelRawTime::kNone)
        out << ",\"minera:rawTime\":\"" << raw_time_name(change->raw_time) << '"';
    }
    out << ",\"ocel:field\":";
    write_string(out, change->field);
    // Round EK: a download writes a change as write_ocel_json does (observed
    // black-box): a datetime value and the time as their isoformat, the time
    // after the value, no value key for a null value.
    const std::string owner = owner_of(*change);
    if (transport_markers || !std::holds_alternative<std::nullptr_t>(change->value)) {
      out << ',';
      write_string(out, change->field);
      out << ':';
      if (!transport_markers &&
          ocel_frame::reference_date_value(log.object_type_attributes, owner, change->field, change->value))
        write_string(out, ocel_frame::object_datetime_text(std::get<std::string>(change->value)));
      else
        write_cell(change->field, change->value);
    }
    if (!transport_markers && change->raw_time != OcelRawTime::kNone) {
      if (!ocel_raw_time_missing(change->raw_time)) {
        out << ",\"ocel:timestamp\":";
        write_raw_time(out, change->raw_time, change->timestamp);
      }
    } else if (!transport_markers && !nat_change_time(change->timestamp)) {
      out << ",\"ocel:timestamp\":";
      write_string(out, ocel_frame::utc_timestamp_text(change->timestamp, 'T', "+00:00"));
    }
    // Round EN: the row's other non-null cells, read_ocel2_sqlite's
    // "@@cumcount" last (write_ocel_json prints every non-null cell of the
    // changes frame row, observed black-box); the transport keeps them so.
    for (const auto& cell : change->cells) {
      if (std::holds_alternative<std::nullptr_t>(cell.value)) continue;
      out << ',';
      write_string(out, cell.key);
      out << ':';
      if (!transport_markers &&
          ocel_frame::reference_date_value(log.object_type_attributes, owner, cell))
        write_string(out, ocel_frame::object_datetime_text(std::get<std::string>(cell.value)));
      else
        write_cell(cell.key, cell.value);
    }
    out << '}';
  }
  out << ']';
}

// Round EK: a download's vmap or ovmap, as write_ocel_json prints its frame
// cells: a datetime as its column holds it (write_frame_value).
// In the frame's column order, null cells left out (round EK, observed
// black-box). Round EO: an int as a float where the column prints as float64
// (Columns::printed_dtype).
// Round ER: write_ocel_json writes an "ocel:"-prefixed attribute (see
// reference_drops_attribute) of an event as a key of the event entry, after
// its time and before "ocel:vmap", and leaves an object's out (observed
// black-box, probe er h10); `prefixed` writes those keys (each after a
// comma), otherwise the map without them.
void write_frame_entries(std::ostringstream& out, const std::vector<OcelAttribute>& attrs,
                         const ocel_frame::Columns& frame, const OcelTypeCatalog& catalog,
                         const std::string& owner, bool prefixed) {
  std::vector<const OcelAttribute*> ordered;
  for (const auto* attribute : ocel_frame::frame_row(attrs, frame))
    if (reference_drops_attribute(attribute->key) == prefixed) ordered.push_back(attribute);
  if (!prefixed) out << '{';
  bool first = true;
  for (const auto* attribute : ordered) {
    if (!first || prefixed) out << ',';
    first = false;
    write_string(out, attribute->key);
    out << ':';
    const std::string& key = attribute->key;
    const OcelValue* v = &attribute->value;
    if (ocel_frame::reference_date_value(catalog, owner, *attribute)) {
      const auto& text = std::get<std::string>(*v);
      write_string(out, frame.dtype(key) == ocel_frame::Dtype::kDatetime ? ocel_frame::datetime64_json_text(text)
                                                                         : ocel_frame::object_datetime_text(text));
    } else if (const auto* integer = std::get_if<std::int64_t>(v);
               integer != nullptr && frame.printed_dtype(key) == ocel_frame::Dtype::kFloat64) {
      write_value(out, OcelValue(static_cast<double>(*integer)));
    } else {
      write_value(out, *v);
    }
  }
  if (!prefixed) out << '}';
}
void write_frame_attrs(std::ostringstream& out, const std::vector<OcelAttribute>& attrs,
                       const ocel_frame::Columns& frame, const OcelTypeCatalog& catalog,
                       const std::string& owner) {
  write_frame_entries(out, attrs, frame, catalog, owner, false);
}

}  // namespace

namespace {

const OcelValue* find_attribute(const std::vector<OcelAttribute>& attributes,
                                std::string_view key) {
  for (const auto& attribute : attributes) {
    if (attribute.key == key) return &attribute.value;
  }
  return nullptr;
}

std::string selector_value(const OcelValue& value, std::string_view selector,
                           std::string_view record_kind,
                           std::string_view record_id,
                           std::string_view family) {
  if (std::holds_alternative<std::nullptr_t>(value))
    throw std::invalid_argument(std::string(family) + " " +
                                std::string(record_kind) +
                                " selector '" + std::string(selector) +
                                "' is null for " + std::string(record_id));
  if (const auto* text = std::get_if<std::string>(&value)) {
    if (text->empty())
      throw std::invalid_argument(std::string(family) + " " +
                                  std::string(record_kind) +
                                  " selector '" + std::string(selector) +
                                  "' is empty for " + std::string(record_id));
    return *text;
  }
  if (const auto* boolean = std::get_if<bool>(&value))
    return *boolean ? "true" : "false";
  if (const auto* integer = std::get_if<std::int64_t>(&value))
    return std::to_string(*integer);
  const auto number = std::get<double>(value);
  if (!std::isfinite(number))
    throw std::invalid_argument(std::string(family) + " " +
                                std::string(record_kind) +
                                " selector '" + std::string(selector) +
                                "' is non-finite for " +
                                std::string(record_id));
  std::ostringstream rendered;
  rendered << std::setprecision(std::numeric_limits<double>::max_digits10)
           << number;
  return rendered.str();
}

std::string selected_event_activity(const OcelEvent& event,
                                    std::string_view selector,
                                    std::string_view family) {
  if (selector.empty() || selector == "activity" ||
      selector == "concept:name" || selector == "ocel:activity")
    return event.activity;
  const auto* value = find_attribute(event.attributes, selector);
  if (value == nullptr)
    throw std::invalid_argument(std::string(family) +
                                " event activity attribute '" +
                                std::string(selector) + "' is missing for " +
                                event.id);
  return selector_value(*value, selector, "event activity", event.id, family);
}

std::string selected_object_type(const OcelObject& object,
                                 std::string_view selector,
                                 std::string_view family) {
  if (selector.empty() || selector == "type" || selector == "object_type" ||
      selector == "ocel:type")
    return object.type;
  const auto* value = find_attribute(object.attributes, selector);
  if (value == nullptr)
    throw std::invalid_argument(std::string(family) +
                                " object type attribute '" +
                                std::string(selector) + "' is missing for " +
                                object.id);
  return selector_value(*value, selector, "object type", object.id, family);
}

}  // namespace

Ocel project_ocel_labels(const Ocel& log, std::string_view activity_attribute,
                         std::string_view object_type_attribute,
                         std::string_view family) {
  Ocel projected = log;
  for (auto& event : projected.events)
    event.activity = selected_event_activity(event, activity_attribute, family);

  std::map<std::string, std::string> object_types;
  for (auto& object : projected.objects) {
    object.type = selected_object_type(object, object_type_attribute, family);
    object_types[object.id] = object.type;
  }
  for (auto& change : projected.object_changes) {
    const auto it = object_types.find(change.object_id);
    if (it != object_types.end()) change.object_type = it->second;
  }
  return projected;
}

Ocel project_ocpn_labels(const Ocel& log,
                         const OcpnDiscoveryOptions& options) {
  return project_ocel_labels(log, options.activity_attribute,
                             options.object_type_attribute, "OCPN");
}

static void sort_ocel_events(Ocel& log, OcelTimestampReader reader, bool keep_nanoseconds,
                             const std::vector<std::optional<std::int64_t>>* given_keys = nullptr);

static Ocel parse_ocel_json_names(std::string_view json, const OcelJsonLimits& limits,
                                 bool literal_names) {
  // pm4py reads the document with json.load: an object id repeated in the
  // classic "ocel:objects" (under another type, say) is the last entry, in
  // the place of the first.
  const Json root = Parser(json, limits).last_repeated_key_wins().python_numbers().parse();
  if (!root.is_object()) throw std::invalid_argument("OCEL root must be object");
  const auto& r = root.object();
  if(const auto* marker=optional(r,"minera:literalNames")) {
    if(!marker->is_bool())throw std::invalid_argument("minera:literalNames must be boolean");
    literal_names=literal_names || std::get<bool>(marker->value);
  }
  const bool standard = r.find("events") != r.end() || r.find("objects") != r.end();
  // Both reference JSON readers keep one relation per event and object (the
  // OCEL 2.0 reader the last one, its qualifier and place; the OCEL 1.0
  // reader the first place with the last qualifier). A document Minera wrote
  // for a log that relates an event to one object more than once (an OCEL
  // SQLite import keeps every event_object row) says so with
  // "minera:repeatedRelations" and keeps them all (round BZ).
  bool repeated_relations = false;
  if (const auto* marker = optional(r, "minera:repeatedRelations")) {
    if (!marker->is_bool())
      throw std::invalid_argument("minera:repeatedRelations must be boolean");
    repeated_relations = std::get<bool>(marker->value);
  }
  // Round EE: a classic file reads as read_ocel_json walks it (see
  // parse_classic_reference); exact filter inputs with literal names keep
  // the former reading. A transport holding event times read_ocel_json
  // cannot read says so and keeps them.
  bool lenient_times = false;
  if (const auto* marker = optional(r, "minera:unreadableEventTimes")) {
    if (!marker->is_bool())
      throw std::invalid_argument("minera:unreadableEventTimes must be boolean");
    lenient_times = std::get<bool>(marker->value);
  }
  // Round EW: int and float qualifiers of a transport (see serialize_ocel_json).
  bool typed_qualifiers = false;
  if (const auto* marker = optional(r, "minera:typedQualifiers")) {
    if (!marker->is_bool())
      throw std::invalid_argument("minera:typedQualifiers must be boolean");
    typed_qualifiers = std::get<bool>(marker->value);
  }
  bool lenient_change_times = false;
  if (const auto* marker = optional(r, "minera:unreadableChangeTimes")) {
    if (!marker->is_bool())
      throw std::invalid_argument("minera:unreadableChangeTimes must be boolean");
    lenient_change_times = std::get<bool>(marker->value);
  }
  const auto* keep_unrelated_marker = optional(r, "minera:keepUnrelated");
  const bool classic_reference = !standard && !literal_names;
  const bool keep_unrelated_flag = keep_unrelated_marker && keep_unrelated_marker->is_bool() &&
                                  std::get<bool>(keep_unrelated_marker->value);
  ClassicListing listing;
  Ocel result = standard ? parse_standard(r, typed_qualifiers, repeated_relations)
                : classic_reference
                    ? parse_classic_reference(root, repeated_relations, lenient_times,
                                              !keep_unrelated_flag && !optional(r, "minera:untypedRelations"),
                                              keep_unrelated_flag, listing, lenient_change_times,
                                              typed_qualifiers)
                    : parse_classic(r, literal_names, repeated_relations);
  if (standard && !repeated_relations) {
    std::set<std::pair<std::string, std::string>> seen;
    std::vector<OcelRelation> kept;
    for (auto it = result.relations.rbegin(); it != result.relations.rend(); ++it)
      if (seen.emplace(it->event_id, it->object_id).second) kept.push_back(std::move(*it));
    result.relations.assign(std::make_move_iterator(kept.rbegin()),
                            std::make_move_iterator(kept.rend()));
  }
  // A document Minera wrote for a log whose relations or object changes are
  // not in the order the document nests them (an OCEL SQLite import: the
  // reference keeps event_object rows and object type table rows in their
  // own order, sorted by time) lists each relation's event, and each
  // change's object and field, in log order (round BY). Entries take the
  // nested ones per event, or per object and field, in turn; any left over
  // keep their nested order after them.
  if (const auto* marker = optional(r, "minera:relationOrder")) {
    const auto order = string_array(marker, "minera:relationOrder");
    std::unordered_map<std::string, std::deque<std::size_t>> pending;
    for (std::size_t index = 0; index < result.relations.size(); ++index)
      pending[result.relations[index].event_id].push_back(index);
    std::vector<OcelRelation> ordered;
    std::vector<bool> taken(result.relations.size(), false);
    for (const auto& event_id : order) {
      const auto found = pending.find(event_id);
      if (found == pending.end() || found->second.empty()) continue;
      const auto index = found->second.front();
      found->second.pop_front();
      taken[index] = true;
      ordered.push_back(result.relations[index]);
    }
    for (std::size_t index = 0; index < result.relations.size(); ++index)
      if (!taken[index]) ordered.push_back(std::move(result.relations[index]));
    result.relations = std::move(ordered);
  }
  // O2O links nest per source object in both forms; "minera:o2oOrder" lists
  // each link's source in log order (the object_object row order of an OCEL
  // SQLite import), taken per source in turn like the relations (round BZ).
  if (const auto* marker = optional(r, "minera:o2oOrder")) {
    const auto order = string_array(marker, "minera:o2oOrder");
    std::unordered_map<std::string, std::deque<std::size_t>> pending;
    for (std::size_t index = 0; index < result.o2o.size(); ++index)
      pending[result.o2o[index].source_id].push_back(index);
    std::vector<OcelO2oRelation> ordered;
    std::vector<bool> taken(result.o2o.size(), false);
    for (const auto& source_id : order) {
      const auto found = pending.find(source_id);
      if (found == pending.end() || found->second.empty()) continue;
      const auto index = found->second.front();
      found->second.pop_front();
      taken[index] = true;
      ordered.push_back(result.o2o[index]);
    }
    for (std::size_t index = 0; index < result.o2o.size(); ++index)
      if (!taken[index]) ordered.push_back(std::move(result.o2o[index]));
    result.o2o = std::move(ordered);
  }
  if (const auto* marker = optional(r, "minera:objectChangeOrder")) {
    const auto order = pair_array(marker, "minera:objectChangeOrder");
    std::map<std::pair<std::string, std::string>, std::deque<std::size_t>> pending;
    for (std::size_t index = 0; index < result.object_changes.size(); ++index)
      pending[{result.object_changes[index].object_id, result.object_changes[index].field}].push_back(index);
    std::vector<OcelObjectChange> ordered;
    std::vector<bool> taken(result.object_changes.size(), false);
    for (const auto& key : order) {
      const auto found = pending.find(key);
      if (found == pending.end() || found->second.empty()) continue;
      const auto index = found->second.front();
      found->second.pop_front();
      taken[index] = true;
      ordered.push_back(result.object_changes[index]);
    }
    for (std::size_t index = 0; index < result.object_changes.size(); ++index)
      if (!taken[index]) ordered.push_back(std::move(result.object_changes[index]));
    result.object_changes = std::move(ordered);
  }
  // A classic document Minera wrote for a log whose event times share one
  // non-zero offset (see serialize_ocel_json) keeps those texts as written;
  // their order is the same by wall clock and by instant.
  std::unordered_map<std::string, std::string> offset_times;
  if (const auto* marker = optional(r, "minera:eventTimeOffset")) {
    if (!marker->is_bool())
      throw std::invalid_argument("minera:eventTimeOffset must be boolean");
    if (std::get<bool>(marker->value) && !standard)
      for (const auto& event : result.events) offset_times.emplace(event.id, event.timestamp);
  }
  // Round DU: a document Minera wrote for a log whose reference events frame
  // is naive (an OCEL CSV import without offsets, a playout; see
  // Ocel::naive_event_time_unit) names the frame's resolution and keeps the
  // texts as written, in either form.
  std::string naive_unit;
  if (const auto* marker = optional(r, "minera:naiveEventTimes")) {
    naive_unit = str(*marker, "minera:naiveEventTimes");
    if (naive_unit != "s" && naive_unit != "ms" && naive_unit != "us" && naive_unit != "ns")
      throw std::invalid_argument("minera:naiveEventTimes must be s, ms, us or ns");
    for (const auto& event : result.events) offset_times.emplace(event.id, event.timestamp);
  }
  // A document Minera wrote for a log with times past the microsecond (an
  // OCEL SQLite or CSV import, which the reference reads at nanoseconds)
  // says so, and keeps them; read_ocel_json and read_ocel2_json of any other
  // file keep microseconds (round BA). The OCEL 2.0 form carries the marker
  // too: the frontend opens an OCEL SQLite file as that form (round BB).
  bool keep_nanoseconds = false;
  if (const auto* marker = optional(r, "minera:nanoseconds")) {
    if (!marker->is_bool())
      throw std::invalid_argument("minera:nanoseconds must be boolean");
    keep_nanoseconds = std::get<bool>(marker->value);
  }
  sort_ocel_events(result, standard ? OcelTimestampReader::kIsoFormat
                                    : OcelTimestampReader::kIsoFormatOrFileOrder,
                   keep_nanoseconds);
  if (!offset_times.empty())
    for (auto& event : result.events)
      if (const auto found = offset_times.find(event.id); found != offset_times.end())
        event.timestamp = found->second;
  // Round EZ: values the reference keeps as texts under a date declaration
  // (see OcelAttribute::text), before the date conversion.
  if (const auto* marker = optional(r, "minera:textValues")) {
    const auto refuse = [] {
      throw std::invalid_argument("minera:textValues must map events and objects to ids and their keys");
    };
    if (!marker->is_object()) refuse();
    for (const auto& [scope, ids] : marker->object()) {
      if ((scope != "events" && scope != "objects") || !ids.is_object()) refuse();
      std::unordered_map<std::string, std::unordered_set<std::string>> keys;
      for (const auto& [id, names] : ids.object()) {
        if (!names.is_array()) refuse();
        for (const auto& name : names.array()) {
          if (!name.is_string()) refuse();
          keys[id].insert(name.string());
        }
      }
      const auto mark = [&](const std::string& id, std::vector<OcelAttribute>& attributes) {
        const auto found = keys.find(id);
        if (found == keys.end()) return;
        for (auto& attribute : attributes)
          if (found->second.count(attribute.key)) attribute.text = true;
      };
      if (scope == "events")
        for (auto& event : result.events) mark(event.id, event.attributes);
      else
        for (auto& object : result.objects) mark(object.id, object.attributes);
    }
  }
  // read_ocel2_json converts declared date values (a text it cannot read
  // stays); read_ocel_json keeps them as text.
  if (standard) convert_ocel_date_attributes(result, OcelTimestampReader::kIsoFormatOrFileOrder);
  // Every reference reader (OCEL 1.0 and 2.0 JSON alike) drops events and
  // objects that take part in no event-object relation. A document Minera
  // wrote for an OCEL that holds such entries (an OCEL CSV import, whose
  // reference reader keeps them, or a filter or playout result of one) says
  // so, and keeps them: routes pass OCELs to each other as this JSON.
  bool keep_unrelated = false;
  if (const auto* marker = optional(r, "minera:keepUnrelated")) {
    if (!marker->is_bool())
      throw std::invalid_argument("minera:keepUnrelated must be boolean");
    keep_unrelated = std::get<bool>(marker->value);
  }
  // Round DT: both reference JSON readers fail on a file that lists no event
  // (its events frame has no timestamp column to sort by), and read a file
  // that lists no object as an objects frame without columns, where every
  // later lookup of ocel:oid or ocel:type fails (observed black-box through
  // read_ocel_json and read_ocel2_json; see require_ocel_object_column).
  // Documents Minera wrote for a log whose reference reader keeps unrelated
  // entries (an OCEL CSV import) and literal names keep the native reading.
  // A document Minera wrote for a log without relations says whether its
  // relations frame is float64 ("minera:untypedRelations", see
  // Ocel::untyped_relations) and reads as that log, empty or not.
  const auto* untyped_marker = optional(r, "minera:untypedRelations");
  if (untyped_marker && !untyped_marker->is_bool())
    throw std::invalid_argument("minera:untypedRelations must be boolean");
  const bool reference_read = !literal_names && !keep_unrelated && !untyped_marker;
  // The classic reference reading raises that itself, and its frames keep
  // their columns when the filtering empties them.
  if (reference_read && !classic_reference && result.events.empty())
    throw std::invalid_argument("KeyError: 'ocel:timestamp'");
  const bool no_object_listed =
      reference_read && (classic_reference ? !listing.objects : result.objects.empty());
  // Both reference JSON readers drop events with an empty id or activity and
  // objects with an empty id or type (observed black-box); literal names
  // (exact filter inputs) keep them.
  // The classic reference reading filters its frames itself (an event can
  // stay without relations there); drop_empty_ocel_names then only clears
  // empty type names from the catalogs.
  if (!literal_names) drop_empty_ocel_names(result);
  if (!keep_unrelated && !classic_reference) drop_unrelated_ocel_entries(result);
  // Round EY: a missing qualifier the readers keep makes the relations
  // column an object one (classic_json::filled_qualifier_dtype); the rows
  // they drop before the fill are gone here.
  if (result.relation_qualifier_dtype && *result.relation_qualifier_dtype != OcelQualifierKind::kStr &&
      std::any_of(result.relations.begin(), result.relations.end(),
                  [](const OcelRelation& relation) { return relation.qualifier_kind == OcelQualifierKind::kStr; }))
    result.relation_qualifier_dtype = OcelQualifierKind::kStr;
  // A log whose reference frame is activity first (read from XML, see
  // Ocel::event_activity_first) keeps that order between routes.
  if (const auto* marker = optional(r, "minera:eventActivityFirst")) {
    if (!marker->is_bool())
      throw std::invalid_argument("minera:eventActivityFirst must be boolean");
    result.event_activity_first = std::get<bool>(marker->value) && !standard;
  }
  if (const auto* raw = optional(r, "minera:tableColumns")) {
    const auto& columns = obj(*raw, "minera:tableColumns");
    const auto read = [&](const char* name,
                          std::optional<std::vector<std::string>>& target) {
      if (const auto* value = optional(columns, name))
        target = string_array(value, name);
    };
    read("events", result.table_columns.events);
    read("objects", result.table_columns.objects);
    read("relations", result.table_columns.relations);
  }
  if (no_object_listed && !result.table_columns.objects)
    result.table_columns.objects.emplace();
  result.untyped_relations =
      untyped_marker ? std::get<bool>(untyped_marker->value)
                     : reference_read && (classic_reference ? !listing.relations : result.relations.empty());
  // Round EO: the O2O frame of read_ocel_json and read_ocel2_json is float64
  // when the file lists no object link (see Ocel::untyped_o2o).
  if (const auto* marker = optional(r, "minera:untypedO2o")) {
    if (!marker->is_bool()) throw std::invalid_argument("minera:untypedO2o must be boolean");
    result.untyped_o2o = std::get<bool>(marker->value);
  } else {
    result.untyped_o2o = !keep_unrelated && !literal_names &&
                         (classic_reference ? !listing.o2o : standard && result.untyped_o2o);
  }
  // Round EY: a classic document's date declarations (see
  // read_classic_dates_as_texts).
  if (!standard) {
    bool declared_dates = false;
    if (const auto* marker = optional(r, "minera:declaredDates")) {
      if (!marker->is_bool()) throw std::invalid_argument("minera:declaredDates must be boolean");
      declared_dates = std::get<bool>(marker->value);
    }
    if (!declared_dates) read_classic_dates_as_texts(result);
  }
  // Round EY: see Ocel::relation_qualifier_dtype (written with the
  // untyped markers).
  if (const auto* marker = optional(r, "minera:qualifierDtypes")) {
    const auto refuse = [] {
      throw std::invalid_argument("minera:qualifierDtypes must map relations or o2o to object, int64, float64 or bool");
    };
    if (!marker->is_object()) refuse();
    for (const auto& [key, value] : marker->object()) {
      if ((key != "relations" && key != "o2o") || !value.is_string()) refuse();
      const auto& name = value.string();
      const auto kind = name == "object"    ? OcelQualifierKind::kStr
                        : name == "int64"   ? OcelQualifierKind::kInt
                        : name == "float64" ? OcelQualifierKind::kFloat
                        : name == "bool"    ? OcelQualifierKind::kBool
                                            : (refuse(), OcelQualifierKind::kStr);
      (key == "relations" ? result.relation_qualifier_dtype : result.o2o_qualifier_dtype) = kind;
    }
  }
  // A document without the marker fixes the dtype its rows show, so a
  // filter that empties them keeps it (the reference read's frame).
  if (!result.relation_qualifier_dtype)
    result.relation_qualifier_dtype = ocel_qualifier_dtype(result.relations, std::nullopt);
  if (!result.o2o_qualifier_dtype) result.o2o_qualifier_dtype = ocel_qualifier_dtype(result.o2o, std::nullopt);
  result.naive_event_time_unit = std::move(naive_unit);
  if (const auto* marker = optional(r, "minera:noneEventTimes")) {
    if (!marker->is_bool()) throw std::invalid_argument("minera:noneEventTimes must be boolean");
    result.none_event_times = std::get<bool>(marker->value);
  }
  // Round EM: see Ocel::event_frame_hint (written by serialize_ocel_json).
  if (const auto* marker = optional(r, "minera:frameHints")) {
    const auto& hints = obj(*marker, "minera:frameHints");
    const auto count = [](const Json& value, const char* name) -> std::size_t {
      if (value.is_uint()) return static_cast<std::size_t>(std::get<std::uint64_t>(value.value));
      if (value.is_int() && std::get<std::int64_t>(value.value) >= 0)
        return static_cast<std::size_t>(std::get<std::int64_t>(value.value));
      throw std::invalid_argument(std::string("minera:frameHints ") + name + " must be a count");
    };
    const auto read = [&](const char* name, OcelFrameHint& hint) {
      const auto* raw = optional(hints, name);
      if (raw == nullptr) return;
      const auto& entry = obj(*raw, "minera:frameHints");
      if (const auto* columns = optional(entry, "columns")) hint.columns = string_array(columns, "columns");
      if (const auto* dropped = optional(entry, "dropped")) {
        if (!dropped->is_array()) throw std::invalid_argument("minera:frameHints dropped must be array");
        for (const auto& item : dropped->array()) {
          if (!item.is_array() || item.array().size() != 3 || !item.array()[0].is_string())
            throw std::invalid_argument("minera:frameHints dropped entries must be [key, kinds, present]");
          const auto kinds = count(item.array()[1], "kinds");
          if (kinds > 127) throw std::invalid_argument("minera:frameHints kinds must be below 128");
          hint.dropped.push_back({item.array()[0].string(), static_cast<unsigned char>(kinds),
                                  count(item.array()[2], "present")});
        }
      }
      if (const auto* rows = optional(entry, "rows")) hint.dropped_rows = count(*rows, "rows");
      // Round EN: the changes frame's field and time places (both or none).
      const auto* field = optional(entry, "field");
      const auto* time = optional(entry, "time");
      if ((field == nullptr) != (time == nullptr))
        throw std::invalid_argument("minera:frameHints field and time come together");
      if (field != nullptr) {
        hint.field_at = count(*field, "field");
        hint.time_at = count(*time, "time");
        if (*hint.field_at == *hint.time_at || *hint.field_at > hint.columns.size() + 1 ||
            *hint.time_at > hint.columns.size() + 1)
          throw std::invalid_argument("minera:frameHints field and time must be distinct places");
      }
      // Round EP: the frame's own columns among the attributes
      // (OcelFrameHint::reserved; -1 after every attribute column).
      if (const auto* reserved = optional(entry, "reserved")) {
        if (!reserved->is_array()) throw std::invalid_argument("minera:frameHints reserved must be array");
        for (const auto& item : reserved->array()) {
          if (!item.is_array() || item.array().size() != 2 || !item.array()[0].is_string())
            throw std::invalid_argument("minera:frameHints reserved entries must be [column, place]");
          const auto& place = item.array()[1];
          const bool end = place.is_int() && std::get<std::int64_t>(place.value) == -1;
          const std::size_t at = end ? kOcelHintEnd : count(place, "reserved place");
          if (!end && at > hint.columns.size())
            throw std::invalid_argument("minera:frameHints reserved place is past the columns");
          hint.reserved.emplace_back(item.array()[0].string(), at);
        }
      }
    };
    read("events", result.event_frame_hint);
    read("objects", result.object_frame_hint);
    read("changes", result.change_frame_hint);
  }
  // Round ES: both reference JSON readers lose an event attribute "@@index"
  // (see drop_reader_event_index); exact filter inputs with literal names
  // keep it.
  if (!literal_names) ocel_frame::drop_reader_event_index(result);
  return result;
}

namespace {

// The shape and calendar helpers are the shared fromisoformat reader's
// (python_isoformat.hpp); this driver adds the reference's error messages.
using detail::python_iso::digits;
using detail::python_iso::leap;
using detail::python_iso::monday_index;

// Wall-clock microseconds since 1970 of an ISO 8601 text as the reference
// OCEL JSON importers read it: extended or basic calendar and week dates, any
// single separator character, then clock fields and an optional Z or +/-
// offset that is validated and dropped. Throws with the reference message.
// The fixtures in minera_recheck_regression_tests and about 7,800 generated
// strings (shapes, separators, fractions, offsets) were checked against the
// pinned reference: every acceptance, value and message agrees.
std::int64_t iso_wall_microseconds(std::string_view original) {
  // A trailing Z reads as "+00:00" (so "2024-01-01Z" is midnight and a stray
  // character may precede it), also in the message, as observed.
  std::string spelled(original);
  if (!spelled.empty() && spelled.back() == 'Z') spelled.replace(spelled.size() - 1, 1, "+00:00");
  const auto invalid = [&](const std::string& reason = {}) {
    throw std::invalid_argument(reason.empty()
        ? "Invalid isoformat string: " + ocel_frame::python_str_repr(spelled) : reason);
  };
  const std::string_view text = spelled;
  if (text.size() < 7) invalid();
  // Where the date ends, from the shapes YYYY-MM-DD, YYYYMMDD, YYYY-Www[-D]
  // and YYYYWww[D].
  std::size_t date_end = 0;
  if (text.size() == 7) date_end = 7;
  else if (text[4] == '-') {
    if (text[5] == 'W') {
      if (text.size() < 8) invalid();
      if (text.size() > 8 && text[8] == '-') {
        if (text.size() == 9) invalid();
        date_end = text.size() > 10 && std::isdigit(static_cast<unsigned char>(text[10])) != 0 ? 8 : 10;
      } else date_end = 8;
    } else date_end = 10;
  } else if (text[4] == 'W') {
    std::size_t index = 7;
    while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) ++index;
    date_end = index < 9 ? index : (index % 2 == 0 ? 7 : 8);
  } else date_end = 8;
  if (date_end > text.size()) invalid();
  const auto date = text.substr(0, date_end);
  int year = 0, month = 1, day = 1, week = 0, weekday = 1;
  bool week_date = false;
  if (!digits(date, 0, 4, year)) invalid();
  if (date.size() > 5 && date[4] == '-' && date[5] == 'W') {
    week_date = true;
    if (!digits(date, 6, 2, week)) invalid();
    if (date.size() == 10) {
      if (date[8] != '-' || !digits(date, 9, 1, weekday)) invalid();
    } else if (date.size() != 8) invalid();
  } else if (date.size() > 4 && date[4] == '-') {
    if (date.size() != 10 || date[7] != '-' || !digits(date, 5, 2, month) ||
        !digits(date, 8, 2, day)) invalid();
  } else if (date.size() > 4 && date[4] == 'W') {
    week_date = true;
    if (!digits(date, 5, 2, week)) invalid();
    if (date.size() == 8) {
      if (!digits(date, 7, 1, weekday)) invalid();
    } else if (date.size() != 7) invalid();
  } else if (date.size() != 8 || !digits(date, 4, 2, month) ||
             !digits(date, 6, 2, day)) invalid();
  // A week date is checked with its shape, before the clock and offset.
  if (week_date) {
    const auto january_first = monday_index(detail::python_iso::days_from_civil(year, 1, 1));
    const int weeks = january_first == 3 || (january_first == 2 && leap(year)) ? 53 : 52;
    if (week < 1 || week > weeks || weekday < 1 || weekday > 7) invalid();
  }
  int clock[3] = {0, 0, 0};
  std::int64_t micro = 0;
  if (date_end < text.size()) {
    const auto time = text.substr(date_end + 1);  // any one separator character
    const auto zone = time.find_first_of("Z+-");
    const int read = detail::python_iso::clock(time.substr(0, zone), clock, micro);
    if (read < 0 || (zone == std::string_view::npos && read != 0)) invalid();
    if (zone != std::string_view::npos) {
      if (time[zone] == 'Z') {
        if (zone + 1 != time.size()) invalid();
      } else {
        int offset[3] = {0, 0, 0};
        std::int64_t offset_micro = 0;
        if (detail::python_iso::clock(time.substr(zone + 1), offset, offset_micro) != 0) invalid();
        if (offset[0] * 3600 + offset[1] * 60 + offset[2] >= 86400) {
          // The message ends with the offset's timedelta repr.
          const std::int64_t sign = time[zone] == '-' ? -1 : 1;
          const std::int64_t total = sign * ((offset[0] * 3600LL + offset[1] * 60LL + offset[2]) * 1000000 + offset_micro);
          const std::int64_t unit = 86400LL * 1000000;
          const std::int64_t days = total >= 0 ? total / unit : -((-total + unit - 1) / unit);
          const std::int64_t rest = total - days * unit;
          std::string repr = "datetime.timedelta(days=" + std::to_string(days);
          if (rest / 1000000 != 0) repr += ", seconds=" + std::to_string(rest / 1000000);
          if (rest % 1000000 != 0) repr += ", microseconds=" + std::to_string(rest % 1000000);
          invalid("offset must be a timedelta strictly between -timedelta(hours=24) and timedelta(hours=24), not " + repr + ").");
        }
      }
    }
  }
  // Calendar ranges are checked after the whole shape, as observed.
  // Year 0 fails as a calendar date; as a week date it fails converting to one.
  if (year < 1) invalid(week_date ? "month must be in 1..12" : "year " + std::to_string(year) + " is out of range");
  std::int64_t days = 0;
  if (week_date) {
    const auto january_fourth = detail::python_iso::days_from_civil(year, 1, 4);
    days = january_fourth - monday_index(january_fourth) + (week - 1) * 7 + (weekday - 1);
  } else {
    if (month < 1 || month > 12) invalid("month must be in 1..12");
    static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const int last = month == 2 && leap(year) ? 29 : kDays[month - 1];
    if (day < 1 || day > last) invalid("day is out of range for month");
    days = detail::python_iso::days_from_civil(year, month, day);
  }
  if (clock[0] > 23) invalid("hour must be in 0..23");
  if (clock[1] > 59) invalid("minute must be in 0..59");
  if (clock[2] > 59) invalid("second must be in 0..59");
  return (days * 86400 + clock[0] * 3600 + clock[1] * 60 + clock[2]) * 1000000 + micro;
}

// OCEL 2.0 XML falls back to dateutil when fromisoformat fails. That value
// keeps its own offset (it is not made naive and stamped UTC like the ISO
// reading), so pandas compares an offset value by its instant, and a value
// without one is naive, which the reference cannot sort next to aware ones.
struct LenientTimestamp {
  std::int64_t microseconds;  // the UTC instant when aware, else the wall clock
  bool aware;
};

constexpr std::string_view kMonthAbbreviations[] = {"jan", "feb", "mar", "apr", "may", "jun",
                                                     "jul", "aug", "sep", "oct", "nov", "dec"};
constexpr std::string_view kMonthNames[] = {"january", "february", "march",     "april",
                                            "may",     "june",     "july",      "august",
                                            "september", "october", "november", "december"};
constexpr std::string_view kWeekdayNames[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun",
                                              "monday", "tuesday", "wednesday", "thursday",
                                              "friday", "saturday", "sunday"};

std::string lower_ascii(std::string_view text) {
  std::string out(text);
  for (auto& ch : out) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return out;
}

int month_from_word(std::string_view word) {
  const auto lowered = lower_ascii(word);
  for (int month = 0; month < 12; ++month)
    if (lowered == kMonthAbbreviations[month] || lowered == kMonthNames[month]) return month + 1;
  return lowered == "sept" ? 9 : 0;
}

// pm4py's dateutil fallback for a text fromisoformat refuses (round EB,
// observed black-box on dateutil 2.9): dateutil.parser.parse with today's
// date for missing fields, fuzzy for read_ocel2_xml (unknown words and
// numbers that fit no field are skipped), not for read_ocel2_json. Throws
// dateutil's error; nullopt for a text outside the port's model (characters
// beyond printable ASCII, integers past 18 digits, a zone named like the
// host's own) and for an offset of a day or more: dateutil keeps it, and
// whether pandas then raises depends on the other values of the column
// (declared).
std::optional<LenientTimestamp> dateutil_reading(std::string_view text, bool fuzzy) {
  const auto reading = pandas_to_datetime::dateutil_parser_parse(text, fuzzy);
  if (!reading) return std::nullopt;
  const auto& v = reading->value;
  const std::int64_t wall =
      (detail::python_iso::days_from_civil(v.year, v.month, v.day) * 86400 + v.hour * 3600LL +
       v.minute * 60LL + v.second) * 1000000 + v.nanoseconds / 1000;
  if (!reading->offset_seconds) return LenientTimestamp{wall, false};
  const auto offset = *reading->offset_seconds;
  if (offset >= 86400 || offset <= -86400) return std::nullopt;
  return LenientTimestamp{wall - offset * 1000000, true};
}

// Whether dateutil's fuzzy reading certainly finds no date: no digit and no
// month or weekday word.
bool certainly_not_a_date(std::string_view text) {
  if (std::any_of(text.begin(), text.end(), [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)) != 0; }))
    return false;
  for (std::size_t p = 0; p < text.size();) {
    if (std::isalpha(static_cast<unsigned char>(text[p])) == 0) { ++p; continue; }
    const std::size_t begin = p;
    while (p < text.size() && std::isalpha(static_cast<unsigned char>(text[p])) != 0) ++p;
    const auto token = text.substr(begin, p - begin);
    if (month_from_word(token)) return false;
    const auto lowered = lower_ascii(token);
    for (const auto name : kWeekdayNames) if (lowered == name) return false;
  }
  return true;
}

// "YYYY-MM-DDTHH:MM:SS[.ffffff|.fffffffff]+00:00" for a UTC value, the
// isoformat of what the reference stores; native readers take it as is.
// `suffix` replaces "+00:00": "Z" (the same UTC value) or "" (a naive one).
std::string utc_timestamp_text(std::int64_t seconds, std::uint32_t nanoseconds,
                               std::string_view suffix = "+00:00") {
  std::int64_t days = seconds >= 0 ? seconds / 86400 : -((-seconds + 86399) / 86400);
  const std::int64_t clock = seconds - days * 86400;
  // civil_from_days (Howard Hinnant).
  days += 719468;
  const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const std::int64_t day_of_era = days - era * 146097;
  const std::int64_t year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
  const std::int64_t day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
  const std::int64_t mp = (5 * day_of_year + 2) / 153;
  const std::int64_t day = day_of_year - (153 * mp + 2) / 5 + 1;
  const std::int64_t month = mp < 10 ? mp + 3 : mp - 9;
  const std::int64_t year = year_of_era + era * 400 + (month <= 2);
  char buffer[64];
  int length = std::snprintf(buffer, sizeof buffer, "%04lld-%02lld-%02lldT%02lld:%02lld:%02lld",
                             static_cast<long long>(year), static_cast<long long>(month),
                             static_cast<long long>(day), static_cast<long long>(clock / 3600),
                             static_cast<long long>(clock / 60 % 60), static_cast<long long>(clock % 60));
  std::string out(buffer, static_cast<std::size_t>(length));
  if (nanoseconds % 1000 != 0) {
    length = std::snprintf(buffer, sizeof buffer, ".%09u", nanoseconds);
    out.append(buffer, static_cast<std::size_t>(length));
  } else if (nanoseconds != 0) {
    length = std::snprintf(buffer, sizeof buffer, ".%06u", nanoseconds / 1000);
    out.append(buffer, static_cast<std::size_t>(length));
  }
  return out.append(suffix);
}

std::string utc_timestamp_text(std::int64_t microseconds, std::string_view suffix = "+00:00") {
  const std::int64_t seconds =
      microseconds >= 0 ? microseconds / 1000000 : -((-microseconds + 999999) / 1000000);
  return utc_timestamp_text(seconds, static_cast<std::uint32_t>((microseconds - seconds * 1000000) * 1000),
                            suffix);
}

// Whether the native readers take `text` as the value `microseconds` the
// reference stores: a UTC one (no non-zero offset of its own, and an offset
// at all: the reference stamps a naive text UTC), or a naive one when
// `aware` is false (an OCEL 2.0 XML dateutil value without an offset).
// Otherwise the stored text is replaced (stored_timestamp_text) so filters,
// features, statistics and exporters see the value the reference stores.
// A text with digits past the microsecond is respelled too: these readers
// keep microseconds (fromisoformat drops the rest: "01:54:16.000000001Z" is
// 01:54:16), where native parsing would keep the nanoseconds (round AZ: OCDFG
// edge durations were a microsecond off). `truncates` is false for the event
// and change times of the classic JSON, which routes use as a lossless
// transport (read_ocel_json keeps microseconds there too; declared).
bool natively_stored(std::string_view text, std::int64_t microseconds, bool aware = true,
                     bool truncates = true) {
  const auto exact = parse_timestamp_exact(text);
  if (!exact || exact->timezone_aware != aware ||
      (exact->timezone_aware && exact->offset_minutes != 0) ||
      (truncates && exact->nanoseconds % 1000 != 0))
    return false;
  const auto native = parse_timestamp_microseconds(text);
  return native && *native == microseconds;
}

// The text stored for `text` when natively_stored refuses it. A naive event
// time is stamped "Z", as the reference JSON writers spell an event time, so
// a naive document written back as JSON reads like theirs; an object change
// (whose time they print as isoformat), an offset and a shape native does
// not read become "+00:00", and a naive dateutil value stays naive.
std::string stored_timestamp_text(std::string_view text, std::int64_t microseconds,
                                  bool event, bool aware = true) {
  if (!aware) return utc_timestamp_text(microseconds, "");
  const auto exact = parse_timestamp_exact(text);
  // An ISO shape the native parser does not read ("20200101T100000") is
  // naive when it still reads with a "Z" appended (an offset would not).
  bool naive = exact && !exact->timezone_aware;
  if (!exact) {
    try {
      naive = iso_wall_microseconds(std::string(text) + "Z") == microseconds;
    } catch (const std::invalid_argument&) {
      naive = false;
    }
  }
  return utc_timestamp_text(microseconds, event && naive ? "Z" : "+00:00");
}

}  // namespace

bool ocel_reference_generated_global(std::string_view key, const OcelValue& value) {
  const auto* text = std::get_if<std::string>(&value);
  if (key == "ocel:global-event.ocel:activity" || key == "ocel:global-object.ocel:type")
    return text != nullptr && *text == "__INVALID__";
  if (key == "ocel:global-log.version" || key == "ocel:global-log.ocel:version")
    return text != nullptr && *text == "1.0";
  if (key == "ocel:global-log.ordering" || key == "ocel:global-log.ocel:ordering")
    return text != nullptr && *text == "timestamp";
  // The attribute-name and object-type lists (flattened by older readers).
  return key == "ocel:global-log.attribute-names" || key == "ocel:global-log.object-types" ||
         key == "ocel:global-log.ocel:attribute-names" ||
         key == "ocel:global-log.ocel:object-types";
}

bool ocel_has_unrelated_entries(const Ocel& log) {
  std::unordered_set<std::string_view> events;
  std::unordered_set<std::string_view> objects;
  for (const auto& event : log.events) events.insert(event.id);
  for (const auto& object : log.objects) objects.insert(object.id);
  std::unordered_set<std::string_view> related_events;
  std::unordered_set<std::string_view> related_objects;
  for (const auto& relation : log.relations) {
    if (!events.count(relation.event_id) || !objects.count(relation.object_id)) return true;
    related_events.insert(relation.event_id);
    related_objects.insert(relation.object_id);
  }
  if (related_events.size() != events.size() || related_objects.size() != objects.size())
    return true;
  return std::any_of(log.o2o.begin(), log.o2o.end(),
                     [&](const OcelO2oRelation& relation) {
                       return !objects.count(relation.source_id) ||
                              !objects.count(relation.target_id);
                     }) ||
         std::any_of(log.e2e.begin(), log.e2e.end(),
                     [&](const OcelE2eRelation& relation) {
                       return !events.count(relation.source_id) ||
                              !events.count(relation.target_id);
                     }) ||
         std::any_of(log.object_changes.begin(), log.object_changes.end(),
                     [&](const OcelObjectChange& change) {
                       return !objects.count(change.object_id);
                     });
}

namespace {
// Whether a relation, object link, event link or object change names a
// missing event or object (no reader leaves one; the writers refuse it
// themselves when they validate references).
bool ocel_has_dangling_references(const Ocel& log) {
  std::unordered_set<std::string_view> events, objects;
  for (const auto& event : log.events) events.insert(event.id);
  for (const auto& object : log.objects) objects.insert(object.id);
  return std::any_of(log.relations.begin(), log.relations.end(),
                     [&](const OcelRelation& relation) {
                       return !events.count(relation.event_id) || !objects.count(relation.object_id);
                     }) ||
         std::any_of(log.o2o.begin(), log.o2o.end(),
                     [&](const OcelO2oRelation& relation) {
                       return !objects.count(relation.source_id) || !objects.count(relation.target_id);
                     }) ||
         std::any_of(log.e2e.begin(), log.e2e.end(),
                     [&](const OcelE2eRelation& relation) {
                       return !events.count(relation.source_id) || !events.count(relation.target_id);
                     }) ||
         std::any_of(log.object_changes.begin(), log.object_changes.end(),
                     [&](const OcelObjectChange& change) { return !objects.count(change.object_id); });
}
}  // namespace

const Ocel& ocel_written_entries(const Ocel& log, std::optional<Ocel>& storage) {
  if (!ocel_has_unrelated_entries(log)) return log;
  // A dangling reference stays for the writer's own validation to refuse.
  if (ocel_has_dangling_references(log)) return log;
  storage.emplace(log);
  drop_unrelated_ocel_entries(*storage);
  return *storage;
}

void drop_unrelated_ocel_entries(Ocel& log) {
  std::unordered_set<std::string> events;
  std::unordered_set<std::string> objects;
  for (const auto& event : log.events) events.insert(event.id);
  for (const auto& object : log.objects) objects.insert(object.id);
  std::unordered_set<std::string> related_events;
  std::unordered_set<std::string> related_objects;
  for (const auto& relation : log.relations) {
    if (!events.count(relation.event_id) || !objects.count(relation.object_id))
      continue;
    related_events.insert(relation.event_id);
    related_objects.insert(relation.object_id);
  }
  if (related_events.size() == events.size() &&
      related_objects.size() == objects.size() &&
      std::all_of(log.relations.begin(), log.relations.end(),
                  [&](const OcelRelation& relation) {
                    return events.count(relation.event_id) &&
                           objects.count(relation.object_id);
                  }) &&
      std::all_of(log.o2o.begin(), log.o2o.end(),
                  [&](const OcelO2oRelation& relation) {
                    return objects.count(relation.source_id) &&
                           objects.count(relation.target_id);
                  }) &&
      std::all_of(log.e2e.begin(), log.e2e.end(),
                  [&](const OcelE2eRelation& relation) {
                    return events.count(relation.source_id) &&
                           events.count(relation.target_id);
                  }) &&
      std::all_of(log.object_changes.begin(), log.object_changes.end(),
                  [&](const OcelObjectChange& change) {
                    return objects.count(change.object_id) != 0;
                  }))
    return;
  const auto erase_if = [](auto& rows, auto predicate) {
    rows.erase(std::remove_if(rows.begin(), rows.end(), predicate), rows.end());
  };
  // Round EM: the frames keep what the dropped rows add (OcelFrameHint).
  ocel_frame::hint_dropped_rows(
      log, [&](const OcelEvent& event) { return !related_events.count(event.id); },
      [&](const OcelObject& object) { return !related_objects.count(object.id); },
      [&](const OcelObjectChange& change) { return !related_objects.count(change.object_id); });
  erase_if(log.events, [&](const OcelEvent& event) {
    return !related_events.count(event.id);
  });
  erase_if(log.objects, [&](const OcelObject& object) {
    return !related_objects.count(object.id);
  });
  erase_if(log.relations, [&](const OcelRelation& relation) {
    return !related_events.count(relation.event_id) ||
           !related_objects.count(relation.object_id);
  });
  erase_if(log.o2o, [&](const OcelO2oRelation& relation) {
    return !related_objects.count(relation.source_id) ||
           !related_objects.count(relation.target_id);
  });
  erase_if(log.e2e, [&](const OcelE2eRelation& relation) {
    return !related_events.count(relation.source_id) ||
           !related_events.count(relation.target_id);
  });
  erase_if(log.object_changes, [&](const OcelObjectChange& change) {
    return !related_objects.count(change.object_id);
  });
}

void drop_empty_ocel_names(Ocel& log) {
  std::unordered_set<std::string> dropped_events;
  std::unordered_set<std::string> dropped_objects;
  for (const auto& event : log.events)
    if (event.id.empty() || event.activity.empty()) dropped_events.insert(event.id);
  for (const auto& object : log.objects)
    if (object.id.empty() || object.type.empty()) dropped_objects.insert(object.id);
  log.event_type_attributes.erase("");
  log.object_type_attributes.erase("");
  if (dropped_events.empty() && dropped_objects.empty()) return;
  const auto event_gone = [&](const std::string& id) {
    return id.empty() || dropped_events.count(id) != 0;
  };
  const auto object_gone = [&](const std::string& id) {
    return id.empty() || dropped_objects.count(id) != 0;
  };
  // Round EM: the frames keep what the dropped rows add (OcelFrameHint).
  ocel_frame::hint_dropped_rows(
      log, [&](const OcelEvent& event) { return event_gone(event.id); },
      [&](const OcelObject& object) { return object_gone(object.id); },
      [&](const OcelObjectChange& change) { return object_gone(change.object_id); });
  std::erase_if(log.events, [&](const OcelEvent& event) { return event_gone(event.id); });
  std::erase_if(log.objects, [&](const OcelObject& object) { return object_gone(object.id); });
  std::erase_if(log.relations, [&](const OcelRelation& relation) {
    return event_gone(relation.event_id) || object_gone(relation.object_id);
  });
  std::erase_if(log.o2o, [&](const OcelO2oRelation& relation) {
    return object_gone(relation.source_id) || object_gone(relation.target_id);
  });
  std::erase_if(log.e2e, [&](const OcelE2eRelation& relation) {
    return event_gone(relation.source_id) || event_gone(relation.target_id);
  });
  std::erase_if(log.object_changes, [&](const OcelObjectChange& change) {
    return object_gone(change.object_id);
  });
}

namespace {

// convert_ocel_date_value; with `stamp_naive_change` false a naive dateutil
// change value of read_ocel2_json stays naive. Returns whether the value is
// a naive dateutil reading.
bool convert_date_value(OcelValue& value, OcelTimestampReader reader, bool change, bool stamp_naive_change) {
  auto* text = std::get_if<std::string>(&value);
  if (text == nullptr) return false;
  std::optional<std::int64_t> microseconds;
  bool aware = true, lenient_route = false;
  try {
    microseconds = iso_wall_microseconds(*text);
  } catch (const std::invalid_argument&) {
    if (reader == OcelTimestampReader::kIsoFormat) throw;
    if (reader != OcelTimestampReader::kDateutil &&
        reader != OcelTimestampReader::kIsoFormatOrFileOrder)
      return false;
    std::optional<LenientTimestamp> lenient;
    if (reader == OcelTimestampReader::kDateutil) {
      // read_ocel2_xml reads fuzzy and raises dateutil's error.
      lenient = dateutil_reading(*text, true);
      if (!lenient) {
        if (certainly_not_a_date(*text))
          throw std::invalid_argument("String does not contain a date: " + *text);
        return false;  // outside the native model: kept as written
      }
    } else {
      // read_ocel2_json keeps any text dateutil does not read (observed
      // black-box), and one outside the native model.
      try {
        lenient = dateutil_reading(*text, false);
      } catch (const std::invalid_argument&) {
        return false;
      }
      if (!lenient) return false;
    }
    // read_ocel2_json makes a change column of naive values UTC, so a naive
    // dateutil value there is stamped UTC (its event and object values stay
    // naive), observed black-box; read_ocel2_xml keeps it naive.
    microseconds = lenient->microseconds;
    aware = lenient->aware ||
            (stamp_naive_change && change && reader == OcelTimestampReader::kIsoFormatOrFileOrder);
    lenient_route = true;
  }
  // A text with surrounding white space, or any text fromisoformat refuses
  // ("... 20:40z"), is not kept as written: the reference reads it through
  // dateutil and stores the datetime.
  const bool padded = !text->empty() &&
                      (std::isspace(static_cast<unsigned char>(text->front())) != 0 ||
                       std::isspace(static_cast<unsigned char>(text->back())) != 0);
  // A change value keeps "+00:00", an event or object value "Z", as the
  // reference JSON writers spell them.
  // A space-separated clock is respelled with 'T', as the reference stores
  // a datetime ("1999-09-08 13:37Z" is written "1999-09-08T13:37:00Z").
  const bool spaced = text->size() > 10 && (*text)[10] == ' ';
  if (!padded && !lenient_route && !spaced && natively_stored(*text, *microseconds, aware) && !(change && text->ends_with('Z')))
    return false;
  *text = utc_timestamp_text(*microseconds, !aware ? "" : change ? "+00:00" : "Z");
  return !aware;
}

}  // namespace

void convert_ocel_date_value(OcelValue& value, OcelTimestampReader reader, bool change) {
  convert_date_value(value, reader, change, true);
}

void check_ocel2_xml_date_text(std::string_view text) {
  try {
    iso_wall_microseconds(text);
    return;
  } catch (const std::invalid_argument&) {
  }
  if (!dateutil_reading(text, true) && certainly_not_a_date(text))
    throw std::invalid_argument("String does not contain a date: " + std::string(text));
}

// Round EU: read_ocel2_xml reads a time fromisoformat refuses through
// dateutil, which keeps an offset of a day or more, and builds its events
// frame of every event it read (also those it drops later). pandas keeps a
// time column whose values all share one such offset, and raises the
// datetime module's own error when the column mixes one with any other
// zone or with naive times (observed black-box, probes eu x1 xoff*.py and
// xsafe.py). Date attributes and change times with such offsets read.
void check_ocel2_xml_event_time_offsets(const Ocel& log) {
  std::optional<std::int64_t> shared;
  bool day_offset = false, other = false;
  for (const auto& event : log.events) {
    std::optional<std::int64_t> offset;
    try {
      (void)iso_wall_microseconds(event.timestamp);
    } catch (const std::invalid_argument&) {
      try {
        const auto reading = pandas_to_datetime::dateutil_parser_parse(event.timestamp, true);
        if (reading && reading->offset_seconds &&
            (*reading->offset_seconds >= 86400 || *reading->offset_seconds <= -86400))
          offset = reading->offset_seconds;
      } catch (const std::invalid_argument&) {
      }
    }
    if (!offset) {
      other = true;
      continue;
    }
    day_offset = true;
    if (shared && *shared != *offset) other = true;
    shared = offset;
  }
  if (day_offset && other)
    throw std::invalid_argument(
        "offset must be a timedelta strictly between -timedelta(hours=24) and timedelta(hours=24).");
}

void convert_ocel_date_attributes(Ocel& log, OcelTimestampReader reader) {
  // read_ocel2_xml converts a type naming "date" (case-sensitive: "Date",
  // "time" and "timestamp" stay text), read_ocel2_json one naming "date" or
  // "time" in any case (observed black-box).
  const auto declared = [reader](const OcelTypeCatalog& catalog, const std::string& owner,
                                 const std::string& key) {
    const auto schema = catalog.find(owner);
    if (schema == catalog.end()) return false;
    const auto found = schema->second.find(key);
    if (found == schema->second.end()) return false;
    if (reader == OcelTimestampReader::kDateutil)
      return found->second.find("date") != std::string::npos;
    const auto type = lower_ascii(found->second);
    return type.find("date") != std::string::npos || type.find("time") != std::string::npos;
  };
  const auto convert = [reader](OcelValue& value, bool change) {
    convert_ocel_date_value(value, reader, change);
  };
  // Round EZ: a value the reference keeps as a text stays (OcelAttribute::text).
  for (auto& event : log.events)
    for (auto& attribute : event.attributes)
      if (!attribute.text && declared(log.event_type_attributes, event.activity, attribute.key))
        convert(attribute.value, false);
  std::unordered_map<std::string_view, std::string_view> object_types;
  for (auto& object : log.objects) {
    object_types.emplace(object.id, object.type);
    for (auto& attribute : object.attributes)
      if (!attribute.text && declared(log.object_type_attributes, object.type, attribute.key))
        convert(attribute.value, false);
  }
  // read_ocel2_json stamps a field's naive change values UTC only when the
  // field's column of the changes frame holds nothing but naive datetimes
  // (and missing values): next to a text, a number or an aware value the
  // column stays object and a naive dateutil value stays naive (round EB,
  // observed black-box).
  const bool json = reader == OcelTimestampReader::kIsoFormatOrFileOrder;
  std::vector<std::size_t> naive_changes;
  std::set<std::string> mixed_fields;
  for (std::size_t index = 0; index < log.object_changes.size(); ++index) {
    auto& change = log.object_changes[index];
    std::string type = change.object_type;
    if (type.empty())
      if (const auto found = object_types.find(change.object_id); found != object_types.end())
        type = std::string(found->second);
    bool naive = false;
    if (declared(log.object_type_attributes, type, change.field))
      naive = json ? convert_date_value(change.value, reader, true, false) : (convert(change.value, true), false);
    if (naive) naive_changes.push_back(index);
    else if (json && !std::holds_alternative<std::nullptr_t>(change.value)) mixed_fields.insert(change.field);
  }
  for (const auto index : naive_changes) {
    auto& change = log.object_changes[index];
    if (!mixed_fields.count(change.field)) std::get<std::string>(change.value) += "+00:00";
  }
}

void sort_ocel_events_by_timestamp(Ocel& log, bool nanoseconds) {
  sort_ocel_events_by_timestamp(log, nanoseconds ? OcelTimestampReader::kInstantNanoseconds
                                                 : OcelTimestampReader::kIsoFormat);
}

void sort_ocel_events_by_timestamp(Ocel& log, OcelTimestampReader reader) {
  sort_ocel_events(log, reader, false);
}

namespace {
// -1, 0, 1 as Python compares an int or float with another, exactly.
int python_number_compare(const OcelSqliteTime& a, const OcelSqliteTime& b) {
  if (a.kind == OcelSqliteTime::Kind::kInteger && b.kind == OcelSqliteTime::Kind::kInteger)
    return a.integer < b.integer ? -1 : a.integer > b.integer ? 1 : 0;
  if (a.kind == OcelSqliteTime::Kind::kReal && b.kind == OcelSqliteTime::Kind::kReal)
    return a.real < b.real ? -1 : a.real > b.real ? 1 : 0;
  const bool flip = a.kind == OcelSqliteTime::Kind::kReal;
  const std::int64_t integer = flip ? b.integer : a.integer;
  const double real = flip ? a.real : b.real;
  int result = 0;  // integer against real
  if (real >= 9223372036854775808.0) {
    result = -1;
  } else if (real < -9223372036854775808.0) {
    result = 1;
  } else {
    const auto whole = static_cast<std::int64_t>(std::trunc(real));
    const double fraction = real - std::trunc(real);
    if (integer != whole) result = integer < whole ? -1 : 1;
    else if (fraction != 0) result = fraction > 0 ? -1 : 1;
  }
  return flip ? -result : result;
}

std::vector<pandas_to_datetime::Cell> sqlite_time_cells(const std::vector<OcelSqliteTime>& times,
                                                        const std::vector<const std::string*>& texts) {
  namespace ptd = pandas_to_datetime;
  std::vector<ptd::Cell> cells(times.size());
  for (std::size_t i = 0; i < times.size(); ++i) {
    auto& cell = cells[i];
    switch (times[i].kind) {
      case OcelSqliteTime::Kind::kNull: break;
      case OcelSqliteTime::Kind::kInteger:
        cell.kind = ptd::Cell::Kind::kInteger;
        cell.integer = times[i].integer;
        break;
      case OcelSqliteTime::Kind::kReal:
        cell.kind = ptd::Cell::Kind::kReal;
        cell.real = times[i].real;
        break;
      case OcelSqliteTime::Kind::kText:
        cell.kind = ptd::Cell::Kind::kText;
        cell.text = *texts[i];
        break;
      case OcelSqliteTime::Kind::kBlob:
        cell.kind = ptd::Cell::Kind::kBlob;
        break;
    }
  }
  return cells;
}

// The text native keeps for a time pandas read as `instant`: the text itself
// when native reads the same instant from it (and no naive reading of an
// aware value), else the instant's UTC text; NaT is the empty text.
std::string sqlite_time_text(const std::string& text, const OcelSqliteTime& time,
                             const std::optional<pandas_to_datetime::UtcInstant>& instant) {
  if (!instant) return {};
  const auto native = parse_timestamp_exact(text);
  if (native && native->seconds == instant->seconds && native->nanoseconds == instant->nanoseconds &&
      (native->timezone_aware || !instant->aware) && time.kind == OcelSqliteTime::Kind::kText)
    return text;
  return utc_timestamp_text(instant->seconds, instant->nanoseconds, instant->aware ? "+00:00" : "Z");
}

// Round EL: a time of a column pandas leaves as it is keeps its Python type
// and value (see OcelRawTime): the integer's digits, the float's repr, the
// text, NaN for NULL.
OcelRawTime raw_sqlite_time(const OcelSqliteTime& time, std::string& text) {
  switch (time.kind) {
    case OcelSqliteTime::Kind::kInteger:
      text = std::to_string(time.integer);
      return OcelRawTime::kInt;
    case OcelSqliteTime::Kind::kReal:
      if (std::isnan(time.real)) {
        text.clear();
        return OcelRawTime::kNaN;
      }
      text = python::float_repr(time.real);
      return OcelRawTime::kFloat;
    case OcelSqliteTime::Kind::kText: return OcelRawTime::kStr;
    default: text.clear(); return time.none ? OcelRawTime::kNull : OcelRawTime::kNaN;
  }
}

}  // namespace

// pm4py.read_ocel2_sqlite reads the event time column with pandas: the
// type tables' columns concatenate (read_sql's dtype per table: int64 when
// every value is an integer, float64 for numbers with NULLs or reals, else
// object), a numeric column stays numbers, and an object column goes
// through pd.to_datetime(utc=True, format="mixed"), which leaves it as it is
// when it raises (round EB, observed black-box).
void sort_ocel2_sqlite_events(Ocel& log, const std::vector<OcelSqliteTime>& times, OcelSqliteTimeColumn column) {
  namespace ptd = pandas_to_datetime;
  if (times.size() != log.events.size()) throw std::logic_error("sort_ocel2_sqlite_events: one time per event");
  std::vector<std::optional<std::int64_t>> keys(times.size());
  if (column != OcelSqliteTimeColumn::kObject) {
    // Numbers sort as numbers (declared: native keeps their texts), NaN last.
    for (std::size_t i = 0; i < times.size(); ++i) {
      const auto& time = times[i];
      if (time.kind == OcelSqliteTime::Kind::kInteger && column == OcelSqliteTimeColumn::kInteger) {
        keys[i] = time.integer;
      } else if (time.kind == OcelSqliteTime::Kind::kInteger || time.kind == OcelSqliteTime::Kind::kReal) {
        double value = time.kind == OcelSqliteTime::Kind::kInteger ? static_cast<double>(time.integer) : time.real;
        if (std::isnan(value)) continue;
        if (value == 0) value = 0;  // -0.0 ties with 0.0
        std::int64_t bits;
        std::memcpy(&bits, &value, sizeof bits);
        keys[i] = bits >= 0 ? bits : bits ^ std::numeric_limits<std::int64_t>::max();
      }
    }
    sort_ocel_events(log, OcelTimestampReader::kInstantNanoseconds, false, &keys);
    return;
  }
  std::vector<const std::string*> event_texts;
  event_texts.reserve(times.size());
  for (const auto& event : log.events) event_texts.push_back(&event.timestamp);
  const auto cells = sqlite_time_cells(times, event_texts);
  std::optional<std::vector<std::optional<ptd::UtcInstant>>> instants;
  bool raised = false;
  try {
    instants = ptd::to_datetime_utc_mixed(cells);
  } catch (const std::invalid_argument&) {
    raised = true;
  }
  if (instants) {
    // Each event keeps its text when native reads it as pandas' instant;
    // otherwise it becomes that instant's UTC text, NaT an empty text.
    std::vector<std::string> texts(times.size());
    for (std::size_t i = 0; i < times.size(); ++i)
      texts[i] = sqlite_time_text(log.events[i].timestamp, times[i], (*instants)[i]);
    for (std::size_t i = 0; i < times.size(); ++i) log.events[i].timestamp = std::move(texts[i]);
    sort_ocel_events(log, OcelTimestampReader::kInstantNanoseconds, false);
    return;
  }
  // A column pandas leaves as it is sorts its numbers first (by value), then
  // its texts as Python sorts them (code points, which is UTF-8 byte order),
  // NULL and NaN last, each group stably (observed black-box). Blobs among
  // them are outside the model (native's reading of the texts).
  const auto is_number = [&](std::size_t i) {
    return times[i].kind == OcelSqliteTime::Kind::kInteger ||
           (times[i].kind == OcelSqliteTime::Kind::kReal && !std::isnan(times[i].real));
  };
  bool blobs = false;
  for (const auto& time : times) blobs = blobs || time.kind == OcelSqliteTime::Kind::kBlob;
  if (!raised || blobs) {
    sort_ocel_events(log, OcelTimestampReader::kInstantNanoseconds, false);
    return;
  }
  const auto compare = [&](std::size_t left, std::size_t right) -> int {
    return python_number_compare(times[left], times[right]);
  };
  std::vector<std::size_t> numbers, texts;
  for (std::size_t i = 0; i < times.size(); ++i) {
    if (is_number(i)) numbers.push_back(i);
    else if (times[i].kind == OcelSqliteTime::Kind::kText) texts.push_back(i);
  }
  std::stable_sort(numbers.begin(), numbers.end(),
                   [&](std::size_t left, std::size_t right) { return compare(left, right) < 0; });
  std::stable_sort(texts.begin(), texts.end(), [&](std::size_t left, std::size_t right) {
    return log.events[left].timestamp < log.events[right].timestamp;
  });
  std::int64_t rank = 0;
  for (std::size_t k = 0; k < numbers.size(); ++k) {
    if (k > 0 && compare(numbers[k - 1], numbers[k]) != 0) ++rank;
    keys[numbers[k]] = rank;
  }
  ++rank;
  for (std::size_t k = 0; k < texts.size(); ++k) {
    if (k > 0 && log.events[texts[k]].timestamp != log.events[texts[k - 1]].timestamp) ++rank;
    keys[texts[k]] = rank;
  }
  for (std::size_t i = 0; i < times.size(); ++i)
    log.events[i].raw_time = raw_sqlite_time(times[i], log.events[i].timestamp);
  sort_ocel_events(log, OcelTimestampReader::kInstantNanoseconds, false, &keys);
}

// Round EL: pm4py.read_ocel2_sqlite reads the change frame's time column as
// it reads the event column (sort_ocel2_sqlite_events): the change rows'
// times only (the object tables' base rows play no part) through
// pd.to_datetime(utc=True, format="mixed"), integers and floats as
// nanoseconds since the epoch, then a stable sort by time with NaT last; a
// column pandas leaves as it is sorts its numbers first, then its texts as
// Python sorts them, NULL last (observed black-box, probe el cp.py). The
// changes come in the reference frame's order before the sort (each table's
// rows by place, then type), `times` one per change. Round EM: a numeric
// column (`column`: every table read is int64, or int64 and float64, by
// read_sql's dtype over all of its rows) skips pd.to_datetime and keeps its
// numbers, sorted by value with NaN last, the ints of a float64 column
// floats (observed black-box, probe em ct.py).
void sort_ocel2_sqlite_changes(Ocel& log, const std::vector<OcelSqliteTime>& given_times,
                               OcelSqliteTimeColumn column) {
  namespace ptd = pandas_to_datetime;
  auto& changes = log.object_changes;
  if (given_times.size() != changes.size()) throw std::logic_error("sort_ocel2_sqlite_changes: one time per change");
  auto times = given_times;
  if (column == OcelSqliteTimeColumn::kReal)
    for (auto& time : times)
      if (time.kind == OcelSqliteTime::Kind::kInteger) {
        time.kind = OcelSqliteTime::Kind::kReal;
        time.real = static_cast<double>(time.integer);
      }
  std::vector<const std::string*> texts;
  texts.reserve(changes.size());
  for (const auto& change : changes) texts.push_back(&change.timestamp);
  const auto cells = sqlite_time_cells(times, texts);
  std::optional<std::vector<std::optional<ptd::UtcInstant>>> instants;
  bool raised = column != OcelSqliteTimeColumn::kObject;
  if (!raised) {
    try {
      instants = ptd::to_datetime_utc_mixed(cells);
    } catch (const std::invalid_argument&) {
      raised = true;
    }
  }
  bool blobs = false;
  for (const auto& time : times) blobs = blobs || time.kind == OcelSqliteTime::Kind::kBlob;
  std::vector<std::size_t> order(changes.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  if (instants) {
    for (std::size_t i = 0; i < changes.size(); ++i)
      changes[i].timestamp = sqlite_time_text(changes[i].timestamp, times[i], (*instants)[i]);
    const auto& read = *instants;
    std::stable_sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
      const auto& a = read[left];
      const auto& b = read[right];
      if (!a || !b) return a.has_value() && !b.has_value();
      return std::tie(a->seconds, a->nanoseconds) < std::tie(b->seconds, b->nanoseconds);
    });
  } else if (raised && !blobs) {
    // Numbers first (by value), then texts (code points), NULL and NaN last.
    const auto group = [&](std::size_t i) {
      const auto& time = times[i];
      if (time.kind == OcelSqliteTime::Kind::kInteger ||
          (time.kind == OcelSqliteTime::Kind::kReal && !std::isnan(time.real)))
        return 0;
      return time.kind == OcelSqliteTime::Kind::kText ? 1 : 2;
    };
    std::stable_sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
      const int a = group(left), b = group(right);
      if (a != b) return a < b;
      if (a == 0) return python_number_compare(times[left], times[right]) < 0;
      if (a == 1) return changes[left].timestamp < changes[right].timestamp;
      return false;
    });
    for (std::size_t i = 0; i < changes.size(); ++i)
      changes[i].raw_time = raw_sqlite_time(times[i], changes[i].timestamp);
  } else {
    // Outside the model (blobs, readings to_datetime leaves out): native's
    // reading of the texts, the frame order when one is unreadable.
    std::vector<std::pair<std::int64_t, std::int64_t>> read;
    bool readable = true;
    for (const auto& change : changes) {
      const auto instant = parse_timestamp_exact(change.timestamp);
      if (!instant) { readable = false; break; }
      read.emplace_back(instant->seconds, instant->nanoseconds);
    }
    if (readable)
      std::stable_sort(order.begin(), order.end(),
                       [&](std::size_t left, std::size_t right) { return read[left] < read[right]; });
  }
  std::vector<OcelObjectChange> sorted;
  sorted.reserve(order.size());
  for (const auto index : order) sorted.push_back(std::move(changes[index]));
  changes = std::move(sorted);
}

// read_ocel_xml builds the time column from each event's typed value, so a
// column holding texts or floats is an object or float column, which pandas
// sorts as described at sort_ocel_xml_events (round ED, observed black-box).
void sort_ocel_xml_events(Ocel& log, const std::vector<OcelXmlTime>& times) {
  using Kind = OcelXmlTime::Kind;
  if (times.size() != log.events.size()) throw std::logic_error("sort_ocel_xml_events: one time per event");
  const auto is_float = [&](std::size_t i) {
    return times[i].kind == Kind::kFloat && !std::isnan(times[i].real);
  };
  bool dates = false, floats = false, texts = false, missing = false;
  for (std::size_t i = 0; i < times.size(); ++i) {
    dates = dates || times[i].kind == Kind::kDate;
    floats = floats || is_float(i);
    texts = texts || times[i].kind == Kind::kText;
    missing = missing || times[i].kind == Kind::kMissing ||
              (times[i].kind == Kind::kFloat && std::isnan(times[i].real));
  }
  if (!floats && !texts && !missing) {
    sort_ocel_events(log, OcelTimestampReader::kIsoFormat, false);
    return;
  }
  if (dates && floats)
    throw std::invalid_argument("'values' is not ordered, please explicitly specify the categories "
                                "order by passing in a categories argument.");
  // Dates (or floats) first by value, then texts by code points (UTF-8 byte
  // order), each group stably; missing times last.
  std::vector<std::int64_t> wall(times.size(), 0);
  std::vector<std::size_t> numbers, words;
  for (std::size_t i = 0; i < times.size(); ++i) {
    auto& text = log.events[i].timestamp;
    switch (times[i].kind) {
      case Kind::kDate:
        wall[i] = iso_wall_microseconds(text);
        if (!natively_stored(text, wall[i])) text = stored_timestamp_text(text, wall[i], true);
        numbers.push_back(i);
        break;
      case Kind::kFloat:
        if (std::isnan(times[i].real)) {
          text.clear();  // NaN sorts and reads as a missing time
        } else {
          text = ocel_frame::float_repr(times[i].real);
          numbers.push_back(i);
        }
        break;
      case Kind::kText:
        words.push_back(i);
        break;
      case Kind::kMissing:
        text.clear();
        break;
    }
  }
  const auto less = [&](std::size_t left, std::size_t right) {
    if (times[left].kind == Kind::kDate) return wall[left] < wall[right];
    return times[left].real < times[right].real;
  };
  std::stable_sort(numbers.begin(), numbers.end(), less);
  std::stable_sort(words.begin(), words.end(), [&](std::size_t left, std::size_t right) {
    return log.events[left].timestamp < log.events[right].timestamp;
  });
  std::vector<std::optional<std::int64_t>> keys(times.size());
  std::int64_t rank = 0;
  for (std::size_t k = 0; k < numbers.size(); ++k) {
    if (k > 0 && less(numbers[k - 1], numbers[k])) ++rank;
    keys[numbers[k]] = rank;
  }
  ++rank;
  for (std::size_t k = 0; k < words.size(); ++k) {
    if (k > 0 && log.events[words[k]].timestamp != log.events[words[k - 1]].timestamp) ++rank;
    keys[words[k]] = rank;
  }
  sort_ocel_events(log, OcelTimestampReader::kIsoFormat, false, &keys);
}

// `keep_nanoseconds`: a classic document Minera wrote with the
// "minera:nanoseconds" marker keeps the digits past the microsecond.
// `given_keys` (SQLite, a column pandas keeps as numbers or texts): the
// event order's keys, nullopt last; event texts stay as they are.
static void sort_ocel_events(Ocel& log, OcelTimestampReader reader, bool keep_nanoseconds,
                             const std::vector<std::optional<std::int64_t>>* given_keys) {
  // The key is the timestamp at the importer's precision: SQLite keeps
  // nanoseconds of the instant, the JSON and XML importers microseconds of
  // the ISO wall-clock fields (an OCEL 2.0 XML dateutil fallback with an
  // offset keys on its instant).
  std::vector<std::tuple<std::int64_t, std::int64_t, std::size_t>> keys;
  keys.reserve(log.events.size());
  bool aware = false, naive = false, unknown = false;  // kDateutil readings
  // Each stored text becomes the value the reference stores: the wall clock
  // stamped UTC for an ISO reading, the UTC instant for SQLite and for an
  // aware dateutil value, the wall clock for a naive one.
  std::vector<std::pair<std::size_t, std::string>> rewrites;
  const bool truncates = !keep_nanoseconds;
  const auto store = [&](std::size_t index, std::int64_t microseconds, bool aware = true) {
    const auto& text = log.events[index].timestamp;
    if (!natively_stored(text, microseconds, aware, truncates))
      rewrites.emplace_back(index, stored_timestamp_text(text, microseconds, true, aware));
  };
  // SQLite events without a time (NaT) go after every dated event, in their
  // order, as pandas places NaT last (observed black-box).
  std::vector<std::size_t> undated;
  for (std::size_t index = 0; index < log.events.size(); ++index) {
    const auto& text = log.events[index].timestamp;
    if (given_keys) {
      if (const auto& key = (*given_keys)[index]) keys.emplace_back(*key, 0, index);
      else undated.push_back(index);
    } else if (log.events[index].raw_time != OcelRawTime::kNone) {
      unknown = true;  // round EL: a raw time keeps the file order
    } else if (reader == OcelTimestampReader::kInstantNanoseconds) {
      if (text.empty()) { undated.push_back(index); continue; }
      const auto instant = parse_timestamp_exact(text);
      if (!instant) { unknown = true; continue; }
      // A naive text is the UTC instant, stamped as the reference does.
      if (instant->timezone_aware && instant->offset_minutes != 0)
        rewrites.emplace_back(index, utc_timestamp_text(instant->seconds, instant->nanoseconds));
      else if (!instant->timezone_aware)
        rewrites.emplace_back(index, utc_timestamp_text(instant->seconds, instant->nanoseconds, "Z"));
      keys.emplace_back(instant->seconds, instant->nanoseconds, index);
    } else if (reader == OcelTimestampReader::kIsoFormatOrFileOrder) {
      try {
        const auto wall = iso_wall_microseconds(text);
        store(index, wall);
        keys.emplace_back(wall, 0, index);
      } catch (const std::invalid_argument&) {
        unknown = true;
      }
    } else if (reader == OcelTimestampReader::kDateutil) {
      std::optional<std::int64_t> wall;
      try {
        wall = iso_wall_microseconds(text);
      } catch (const std::invalid_argument&) {
      }
      if (wall) {
        store(index, *wall);
        keys.emplace_back(*wall, 0, index);
        aware = true;
      } else if (const auto lenient = dateutil_reading(text, true)) {
        (lenient->aware ? aware : naive) = true;
        store(index, lenient->microseconds, lenient->aware);
        keys.emplace_back(lenient->microseconds, 0, index);
      } else if (certainly_not_a_date(text)) {
        throw std::invalid_argument("String does not contain a date: " + text);
      } else {
        unknown = true;  // a shape native does not read: keep file order
      }
    } else {
      const auto wall = iso_wall_microseconds(text);
      store(index, wall);
      keys.emplace_back(wall, 0, index);
    }
  }
  for (auto& [index, text] : rewrites) log.events[index].timestamp = std::move(text);
  // Round EN: the reference builds the events frame in file order and sorts
  // it after, so its columns keep the file's first appearance (observed
  // black-box, read_ocel2_json): the frame hint keeps that order when the
  // sorted events would give another.
  std::vector<std::string> file_columns;
  {
    ocel_frame::Columns frame;
    frame.seed(log.event_frame_hint);
    for (const auto& event : log.events) frame.observe_row(event.attributes);
    file_columns = frame.names();
  }
  const auto keep_file_columns = [&] {
    ocel_frame::Columns frame;
    frame.seed(log.event_frame_hint);
    for (const auto& event : log.events) frame.observe_row(event.attributes);
    if (frame.names() != file_columns) log.event_frame_hint.columns = file_columns;
  };
  // Object changes take the same reading as events, so a change keeps its
  // value through every export format. A text the reader cannot take, and a
  // 1970-01-01T00:00:00 base entry (kept raw by the reference), stays as is.
  // read_ocel2_xml and read_ocel2_json keep a change time of "0" as its text
  // too. Round EN: read_ocel2_xml's kept texts are strs in its frame (next
  // to the datetimes an object column); read_ocel2_json converts the column
  // with pd.to_datetime(utc=True) after, so they are strs only when a text
  // pandas cannot read ("0") keeps the column object (observed black-box,
  // probes en xt.py, jt.py). The routes and writers see the strs.
  const bool standard = reader == OcelTimestampReader::kDateutil || reader == OcelTimestampReader::kIsoFormat;
  bool kept_texts = reader == OcelTimestampReader::kDateutil;
  // Round EO: a text the reader cannot take at all (a "+25:00" offset)
  // keeps the column object too, and stays a str there next to the
  // datetimes (observed black-box, g52_0020: write_ocel2_sqlite then writes
  // the datetimes and NULL for the str).
  const auto unreadable_change_time = [](const std::string& text) {
    if (ocel_frame::pandas_nat_time_text(text)) return false;
    try {
      (void)iso_wall_microseconds(text);
      return false;
    } catch (const std::invalid_argument&) {
      return true;
    }
  };
  if (reader == OcelTimestampReader::kIsoFormat)
    for (const auto& change : log.object_changes)
      if (change.raw_time == OcelRawTime::kNone &&
          (change.timestamp == "0" ||
           (change.timestamp.starts_with("1970-01-01T00:00:00") && !parse_timestamp_exact(change.timestamp)) ||
           unreadable_change_time(change.timestamp)))
        kept_texts = true;
  for (auto& change : log.object_changes) {
    const auto& text = change.timestamp;
    if (change.raw_time != OcelRawTime::kNone) continue;  // round EL: kept as read
    if (text.starts_with("1970-01-01T00:00:00") || (standard && text == "0")) {
      if (kept_texts) change.raw_time = OcelRawTime::kStr;
      // Round EO: read_ocel2_json's pd.to_datetime(utc=True) of a datetime
      // column reads a naive one as UTC, as its other naive change times
      // (observed black-box, g52_0164); an aware one stays for the writers'
      // UTC instant.
      const auto exact = reader == OcelTimestampReader::kIsoFormat && !kept_texts
                             ? parse_timestamp_exact(text)
                             : std::nullopt;
      if (!exact || exact->timezone_aware) continue;
    }
    if (reader == OcelTimestampReader::kInstantNanoseconds) {
      const auto instant = parse_timestamp_exact(text);
      if (instant && (!instant->timezone_aware || instant->offset_minutes != 0))
        change.timestamp = utc_timestamp_text(instant->seconds, instant->nanoseconds);
      continue;
    }
    std::optional<std::int64_t> microseconds;
    bool aware = true;
    try {
      microseconds = iso_wall_microseconds(text);
      // read_ocel_json converts an "ocel:objectChanges" time to its UTC
      // instant ("2020-01-03T00:00:00+01:00" is 2020-01-02 23:00 UTC), while
      // its event times and every OCEL 2.0 reader keep the wall clock
      // (observed black-box). The offset is the one the native parser reads.
      if (reader == OcelTimestampReader::kIsoFormatOrFileOrder)
        if (const auto exact = parse_timestamp_exact(text);
            exact && exact->timezone_aware && exact->offset_minutes != 0)
          *microseconds -= static_cast<std::int64_t>(exact->offset_minutes) * 60 * 1000000;
    } catch (const std::invalid_argument&) {
      if (reader == OcelTimestampReader::kDateutil) {
        if (const auto lenient = dateutil_reading(text, true)) {
          microseconds = lenient->microseconds;
          aware = lenient->aware;
        } else if (certainly_not_a_date(text)) {
          throw std::invalid_argument("String does not contain a date: " + text);
        }
      }
    }
    if (!microseconds && standard && kept_texts && !ocel_frame::pandas_nat_time_text(text))
      change.raw_time = OcelRawTime::kStr;
    if (microseconds && !natively_stored(text, *microseconds, aware, truncates))
      change.timestamp = stored_timestamp_text(text, *microseconds, false, aware);
  }
  if (unknown) return;
  // pandas cannot order tz-naive values next to tz-aware ones (the message
  // the reference raises from sort_values).
  if (aware && naive)
    throw std::invalid_argument("'values' is not ordered, please explicitly specify the categories "
                                "order by passing in a categories argument.");
  // Relations follow their events' order even when the events already are
  // in order.
  std::stable_sort(keys.begin(), keys.end());
  std::vector<OcelEvent> events;
  events.reserve(log.events.size());
  for (const auto& key : keys) events.push_back(std::move(log.events[std::get<2>(key)]));
  for (const auto index : undated) events.push_back(std::move(log.events[index]));
  log.events = std::move(events);
  keep_file_columns();
  // Relations sort by their event's time, stably: events at one instant keep
  // their relations' order, which is the event order in the JSON, XML and
  // CSV files (relations nested in events) but the event_object row order
  // in SQLite (round BY, observed black-box). An undated event's relations
  // go after the dated ones, those of an unknown event last.
  std::unordered_map<std::string, std::size_t> instant_rank;
  std::size_t ranks = 0;
  for (std::size_t index = 0; index < keys.size(); ++index) {
    if (index > 0 && (std::get<0>(keys[index]) != std::get<0>(keys[index - 1]) ||
                      std::get<1>(keys[index]) != std::get<1>(keys[index - 1])))
      ++ranks;
    instant_rank.emplace(log.events[index].id, ranks);
  }
  const auto undated_rank = ranks + 1;
  for (std::size_t index = keys.size(); index < log.events.size(); ++index)
    instant_rank.emplace(log.events[index].id, undated_rank);
  const auto rank = [&](const OcelRelation& relation) {
    const auto found = instant_rank.find(relation.event_id);
    return found == instant_rank.end() ? undated_rank + 1 : found->second;
  };
  std::stable_sort(log.relations.begin(), log.relations.end(),
                   [&](const auto& left, const auto& right) {
                     return rank(left) < rank(right);
                   });
}

std::string ocel_limit_text(std::size_t bytes) {
  constexpr std::size_t mebibyte = 1024u * 1024u;
  if (bytes != 0 && bytes % mebibyte == 0)
    return std::to_string(bytes / mebibyte) + " MiB";
  return std::to_string(bytes) + " bytes";
}

std::optional<std::string> ocel_json_limit_violation(
    std::string_view json, const OcelJsonLimits& limits) {
  if (json.size() > limits.max_bytes)
    return "OCEL JSON file exceeds the " + ocel_limit_text(limits.max_bytes) +
           " OCEL limit";
  // Mirrors the parser's accounting: every value (not object keys) is a
  // node, its depth is the number of enclosing containers, and strings are
  // measured decoded.
  std::vector<char> containers;
  bool key_next = false;
  std::size_t nodes = 0;
  const auto hex = [&](std::size_t at, unsigned& code) {
    if (at + 4 > json.size()) return false;
    code = 0;
    for (std::size_t k = at; k < at + 4; ++k) {
      const char h = json[k];
      code <<= 4;
      if (h >= '0' && h <= '9') code += static_cast<unsigned>(h - '0');
      else if (h >= 'a' && h <= 'f') code += static_cast<unsigned>(h - 'a' + 10);
      else if (h >= 'A' && h <= 'F') code += static_cast<unsigned>(h - 'A' + 10);
      else return false;
    }
    return true;
  };
  for (std::size_t i = 0; i < json.size();) {
    const char c = json[i];
    if (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == ':') { ++i; continue; }
    if (c == ',') {
      key_next = !containers.empty() && containers.back() == '{';
      ++i;
      continue;
    }
    if (c == '}' || c == ']') {
      if (!containers.empty()) containers.pop_back();
      key_next = false;
      ++i;
      continue;
    }
    const bool key = key_next;
    key_next = false;
    if (!key) {
      if (containers.size() > limits.max_depth)
        return std::string("OCEL JSON nesting exceeds the depth limit of ") +
               std::to_string(limits.max_depth);
      if (++nodes > limits.max_nodes)
        return "OCEL JSON has more than " + std::to_string(limits.max_nodes) +
               " values, the OCEL node limit";
    }
    if (c == '"') {
      std::size_t decoded = 0;
      ++i;
      while (i < json.size() && json[i] != '"') {
        if (json[i] != '\\') { ++decoded; ++i; continue; }
        if (i + 1 < json.size() && json[i + 1] == 'u') {
          unsigned code = 0;
          if (!hex(i + 2, code)) { ++decoded; i += 2; continue; }
          i += 6;
          unsigned low = 0;
          if (code >= 0xd800 && code <= 0xdbff && i + 1 < json.size() &&
              json[i] == '\\' && json[i + 1] == 'u' && hex(i + 2, low)) {
            decoded += 4;
            i += 6;
          } else {
            decoded += code <= 0x7f ? 1 : code <= 0x7ff ? 2 : 3;
          }
        } else {
          ++decoded;
          i += 2;
        }
      }
      ++i;
      if (decoded > limits.max_string_bytes)
        return "OCEL JSON has a string longer than " +
               ocel_limit_text(limits.max_string_bytes) +
               ", the OCEL string limit";
      continue;
    }
    if (c == '{') { containers.push_back('{'); key_next = true; ++i; continue; }
    if (c == '[') { containers.push_back('['); ++i; continue; }
    while (i < json.size()) {
      const char d = json[i];
      if (d == ' ' || d == '\n' || d == '\r' || d == '\t' || d == ',' ||
          d == ']' || d == '}' || d == ':' || d == '"' || d == '{' || d == '[')
        break;
      ++i;
    }
  }
  return std::nullopt;
}

Ocel parse_ocel_json(std::string_view json, const OcelJsonLimits& limits) {
  return parse_ocel_json_names(json,limits,false);
}
// Only a frame declared without any column counts: frames declared with
// other names for the id and type (exact call models) keep both.
void require_ocel_object_column(const Ocel& log, std::string_view column) {
  const auto& declared = log.table_columns.objects;
  if (declared && declared->empty())
    throw std::invalid_argument("KeyError: '" + std::string(column) + "'");
}

void require_ocel_object_columns(const Ocel& log) {
  const auto& declared = log.table_columns.objects;
  if (declared && declared->empty())
    throw std::invalid_argument(
        "KeyError: \"None of [Index(['ocel:oid', 'ocel:type'], dtype='str')] are in the [columns]\"");
}

bool ocel_relations_frame_degenerate(const Ocel& log) {
  const auto& declared = log.table_columns.relations;
  if (!declared) return false;
  for (const auto& name : *declared)
    if (name == "ocel:eid" || name == "ocel:oid" || name == "ocel:activity" ||
        name == "ocel:type" || name == "ocel:timestamp")
      return false;
  return true;
}

void require_ocel_relation_column(const Ocel& log, std::string_view column) {
  if (!ocel_relations_frame_degenerate(log)) return;
  const auto& declared = *log.table_columns.relations;
  if (std::find(declared.begin(), declared.end(), column) == declared.end())
    throw std::invalid_argument("KeyError: '" + std::string(column) + "'");
}

bool ocel_events_frame_columnless(const Ocel& log) {
  return log.table_columns.events && log.table_columns.events->empty();
}

void require_ocel_event_column(const Ocel& log, std::string_view column) {
  if (ocel_events_frame_columnless(log))
    throw std::invalid_argument("KeyError: '" + std::string(column) + "'");
}

void mark_ocel_relation_free_frames(Ocel& log, bool columnless_entities) {
  if (!log.relations.empty()) return;
  log.table_columns.relations = std::vector<std::string>{"ocel:qualifier"};
  if (columnless_entities && log.events.empty() && log.objects.empty()) {
    log.table_columns.events.emplace();
    log.table_columns.objects.emplace();
  }
}

Ocel parse_ocel_json_literal_names(std::string_view json, const OcelJsonLimits& limits) {
  return parse_ocel_json_names(json,limits,true);
}

std::string serialize_ocel_json(const Ocel& input, OcelJsonFormat format,
                                bool transport_markers) {
  // Round EH: a user's file leaves out events and objects without relations.
  std::optional<Ocel> written;
  const Ocel& log = transport_markers ? input : ocel_written_entries(input, written);
  std::ostringstream out;
  // Marks a log that holds events or objects without an event-object
  // relation, so parse_ocel_json keeps them instead of dropping them as the
  // reference file readers do.
  // Round DT: a log without relations travels with its frame type, so a
  // route reads an empty filter result as the in-memory log it was rather
  // than as a file without events (see Ocel::untyped_relations).
  // Round EW: typed qualifiers (OcelQualifierKind) travel as JSON numbers
  // and (round EX) bools; round EX: the OCEL 2.0 transport (an OCEL SQLite
  // import's open source view) says so too, which EW left out (its
  // qualifiers then failed every route).
  const auto write_typed_qualifiers = [&] {
    if (std::any_of(log.relations.begin(), log.relations.end(),
                    [](const OcelRelation& relation) { return relation.qualifier_kind != OcelQualifierKind::kStr; }) ||
        std::any_of(log.o2o.begin(), log.o2o.end(),
                    [](const OcelO2oRelation& relation) { return relation.qualifier_kind != OcelQualifierKind::kStr; }))
      out << ",\"minera:typedQualifiers\":true";
  };
  // Round EZ: the keys of each event's and object's values the reference
  // keeps as texts under a date declaration (OcelAttribute::text), by id,
  // in either form.
  // The OCEL 2.0 form, whose reader converts the declared dates, marks
  // every other text under a date declaration that its reader would
  // convert too (see the type table below), so it reads back as written.
  const auto write_text_values = [&](bool standard_form) {
    const auto kept = [&](const OcelAttribute& attribute, const OcelTypeCatalog& catalog, const std::string& owner) {
      if (attribute.text) return true;
      if (!standard_form || !std::holds_alternative<std::string>(attribute.value) ||
          !ocel_frame::declared_date(catalog, owner, attribute.key) ||
          ocel_frame::reference_date_value(catalog, owner, attribute))
        return false;
      OcelValue converted = attribute.value;
      try {
        convert_ocel_date_value(converted, OcelTimestampReader::kIsoFormatOrFileOrder);
      } catch (const std::exception&) {
        return true;
      }
      return converted != attribute.value;
    };
    const auto marked = [&](const std::vector<OcelAttribute>& attributes, const OcelTypeCatalog& catalog,
                            const std::string& owner) {
      return std::any_of(attributes.begin(), attributes.end(),
                         [&](const OcelAttribute& attribute) { return kept(attribute, catalog, owner); });
    };
    const bool events_marked = std::any_of(log.events.begin(), log.events.end(), [&](const OcelEvent& event) {
      return marked(event.attributes, log.event_type_attributes, event.activity);
    });
    const bool objects_marked = std::any_of(log.objects.begin(), log.objects.end(), [&](const OcelObject& object) {
      return marked(object.attributes, log.object_type_attributes, object.type);
    });
    if (!events_marked && !objects_marked) return;
    const auto write_scope = [&](const char* name, const auto& rows, const OcelTypeCatalog& catalog,
                                 const auto& owner_of) {
      out << '"' << name << "\":{";
      std::set<std::string> written;
      bool first_row = true;
      for (const auto& row : rows) {
        const std::string& owner = owner_of(row);
        if (!marked(row.attributes, catalog, owner) || !written.insert(row.id).second) continue;
        if (!first_row) out << ',';
        first_row = false;
        write_string(out, row.id);
        out << ":[";
        bool first_key = true;
        for (const auto& attribute : row.attributes) {
          if (!kept(attribute, catalog, owner)) continue;
          if (!first_key) out << ',';
          first_key = false;
          write_string(out, attribute.key);
        }
        out << ']';
      }
      out << '}';
    };
    out << ",\"minera:textValues\":{";
    write_scope("events", log.events, log.event_type_attributes,
                [](const OcelEvent& event) -> const std::string& { return event.activity; });
    out << ',';
    write_scope("objects", log.objects, log.object_type_attributes,
                [](const OcelObject& object) -> const std::string& { return object.type; });
    out << '}';
  };
  const auto write_untyped_relations = [&] {
    if (log.relations.empty())
      out << ",\"minera:untypedRelations\":" << (log.untyped_relations ? "true" : "false");
    // Round EO: see Ocel::untyped_o2o.
    if (log.o2o.empty()) out << ",\"minera:untypedO2o\":" << (log.untyped_o2o ? "true" : "false");
    // Round EY: a qualifier column's dtype that its rows do not show (see
    // Ocel::relation_qualifier_dtype).
    const auto relations_dtype = log.relation_qualifier_dtype &&
                                         *log.relation_qualifier_dtype != ocel_qualifier_dtype(log.relations, std::nullopt)
                                     ? ocel_qualifier_dtype_name(*log.relation_qualifier_dtype)
                                     : nullptr;
    const auto o2o_dtype = log.o2o_qualifier_dtype &&
                                   *log.o2o_qualifier_dtype != ocel_qualifier_dtype(log.o2o, std::nullopt)
                               ? ocel_qualifier_dtype_name(*log.o2o_qualifier_dtype)
                               : nullptr;
    if (relations_dtype || o2o_dtype) {
      out << ",\"minera:qualifierDtypes\":{";
      if (relations_dtype) out << "\"relations\":\"" << relations_dtype << '"';
      if (o2o_dtype) out << (relations_dtype ? "," : "") << "\"o2o\":\"" << o2o_dtype << '"';
      out << '}';
    }
  };
  // Round DU: a log whose reference events frame is naive travels with its
  // resolution and its texts (see Ocel::naive_event_time_unit).
  const auto write_naive_event_times = [&] {
    // Round EH: see Ocel::none_event_times.
    if (log.none_event_times) out << ",\"minera:noneEventTimes\":true";
    if (log.naive_event_time_unit.empty()) return;
    out << ",\"minera:naiveEventTimes\":";
    write_string(out, log.naive_event_time_unit);
  };
  // Round EM: the frames' hints (Ocel::event_frame_hint, OcelFrameHint):
  // "minera:frameHints": {"events": {"columns": [...], "dropped": [[key,
  // kinds, present], ...], "rows": n}, "objects": ..., "changes": ...}.
  const auto write_frame_hints = [&] {
    if (log.event_frame_hint.empty() && log.object_frame_hint.empty() && log.change_frame_hint.empty()) return;
    out << ",\"minera:frameHints\":{";
    bool first = true;
    const auto write = [&](const char* name, const OcelFrameHint& hint) {
      if (hint.empty()) return;
      if (!first) out << ',';
      first = false;
      write_string(out, name);
      out << ":{\"columns\":[";
      for (std::size_t i = 0; i < hint.columns.size(); ++i) {
        if (i) out << ',';
        write_string(out, hint.columns[i]);
      }
      out << "],\"dropped\":[";
      for (std::size_t i = 0; i < hint.dropped.size(); ++i) {
        if (i) out << ',';
        out << '[';
        write_string(out, hint.dropped[i].key);
        out << ',' << static_cast<unsigned>(hint.dropped[i].kinds) << ',' << hint.dropped[i].present << ']';
      }
      out << "],\"rows\":" << hint.dropped_rows;
      if (hint.field_at && hint.time_at) out << ",\"field\":" << *hint.field_at << ",\"time\":" << *hint.time_at;
      if (!hint.reserved.empty()) {
        out << ",\"reserved\":[";
        for (std::size_t i = 0; i < hint.reserved.size(); ++i) {
          if (i != 0) out << ',';
          out << '[';
          write_string(out, hint.reserved[i].first);
          out << ',';
          if (hint.reserved[i].second == kOcelHintEnd) out << "-1";
          else out << hint.reserved[i].second;
          out << ']';
        }
        out << ']';
      }
      out << '}';
    };
    write("events", log.event_frame_hint);
    write("objects", log.object_frame_hint);
    write("changes", log.change_frame_hint);
    out << '}';
  };
  // A download spells a naive event time as both reference JSON writers do,
  // the wall clock with "Z" (black-box, round DU), the fraction kept as for
  // an aware time (declared); the transport keeps the text.
  // Round EP: so does a time of a log whose event times share one non-zero
  // offset (an OCEL CSV read, datetime64[us, UTC+02:00]): both writers
  // print its wall clock with "Z" (black-box, bank b57: `14:00:00+02:00`
  // is `2024-02-01T14:00:00Z`), formerly declared.
  const auto shared_event_offset = [&]() -> std::optional<std::int32_t> {
    if (transport_markers) return std::nullopt;
    std::vector<std::string_view> texts;
    texts.reserve(log.events.size());
    for (const auto& event : log.events)
      if (event.raw_time == OcelRawTime::kNone) texts.emplace_back(event.timestamp);
    return ocel_frame::shared_nonzero_offset(texts);
  }();
  const auto event_time = [&](const std::string& text) -> std::string {
    if (transport_markers) return text;
    const auto exact = parse_timestamp_exact(text);
    if (!exact) return text;
    if (exact->timezone_aware) {
      if (!shared_event_offset) return text;
      return ocel_frame::offset_timestamp_text(text, 'T', shared_event_offset, std::string_view("Z"));
    }
    return utc_timestamp_text(exact->seconds, exact->nanoseconds, "Z");
  };
  const auto write_keep_unrelated = [&] {
    std::unordered_set<std::string_view> related_events, related_objects;
    for (const auto& relation : log.relations) {
      related_events.insert(relation.event_id);
      related_objects.insert(relation.object_id);
    }
    const bool unrelated =
        std::any_of(log.events.begin(), log.events.end(),
                    [&](const auto& event) { return !related_events.contains(event.id); }) ||
        std::any_of(log.objects.begin(), log.objects.end(),
                    [&](const auto& object) { return !related_objects.contains(object.id); });
    if (unrelated) out << ",\"minera:keepUnrelated\":true";
  };
  // Marks a log whose event times all share one non-zero offset (an OCEL
  // CSV import), so parse_ocel_json keeps those texts as written instead of
  // the wall clock stamped UTC that read_ocel_json stores: the XML and SQLite
  // writers print that offset as the reference does for such a log, also
  // after the log passed between routes as this JSON.
  const auto write_event_time_offset = [&] {
    std::optional<std::int32_t> offset;
    for (const auto& event : log.events) {
      // Round EH: a NaT time keeps the column's offset (pandas keeps the
      // dtype); the transport keeps its empty text.
      if (ocel_frame::nat_time_text(event.timestamp)) continue;
      const auto parsed = parse_timestamp_exact(event.timestamp);
      if (!parsed || !parsed->timezone_aware || parsed->offset_minutes == 0 ||
          (offset && *offset != parsed->offset_minutes))
        return;
      offset = parsed->offset_minutes;
    }
    if (offset) out << ",\"minera:eventTimeOffset\":true";
  };
  // Marks a log whose event or object-change times hold digits past the
  // microsecond (the SQLite and CSV readers keep nanoseconds), so
  // parse_ocel_json keeps them instead of the microseconds read_ocel_json
  // keeps: routes pass OCELs to each other as this JSON.
  const auto write_nanoseconds = [&] {
    const auto fine = [](const std::string& text) {
      const auto parsed = parse_timestamp_exact(text);
      return parsed && parsed->nanoseconds % 1000 != 0;
    };
    const bool any =
        std::any_of(log.events.begin(), log.events.end(), [&](const auto& event) { return fine(event.timestamp); }) ||
        std::any_of(log.object_changes.begin(), log.object_changes.end(),
                    [&](const auto& change) { return fine(change.timestamp); });
    if (any) out << ",\"minera:nanoseconds\":true";
  };
  const auto write_table_columns = [&] {
    const auto& columns = log.table_columns;
    if (!columns.events && !columns.objects && !columns.relations) return;
    out << ",\"minera:tableColumns\":{";
    bool first = true;
    const auto write = [&](const char* name,
                            const std::optional<std::vector<std::string>>& values) {
      if (!values) return;
      if (!first) out << ',';
      first = false;
      write_string(out, name);
      out << ":[";
      for (std::size_t i = 0; i < values->size(); ++i) {
        if (i) out << ',';
        write_string(out, (*values)[i]);
      }
      out << ']';
    };
    write("events", columns.events);
    write("objects", columns.objects);
    write("relations", columns.relations);
    out << '}';
  };
  // Events, objects, attributes and relations in log order, as the reference
  // writers emit their frames: a reader then rebuilds the same row and column
  // order (readers keep file order; the timestamp sort is stable).
  std::vector<const OcelEvent*> events;
  for (const auto& event : log.events) events.push_back(&event);
  std::vector<const OcelObject*> objects;
  for (const auto& object : log.objects) objects.push_back(&object);

  // Relations and O2O links indexed once by their event / source id, in log
  // order, so serialising stays linear in the log size.
  std::unordered_map<std::string_view, std::vector<const OcelRelation*>> relations_by_event;
  for (const auto& relation : log.relations)
    relations_by_event[relation.event_id].push_back(&relation);
  std::unordered_map<std::string_view, std::vector<const OcelO2oRelation*>> o2o_by_source;
  for (const auto& relation : log.o2o) o2o_by_source[relation.source_id].push_back(&relation);
  // Each object's changes in log order, so no object rescans every change.
  std::unordered_map<std::string_view, std::vector<const OcelObjectChange*>> changes_by_object;
  for (const auto& change : log.object_changes) changes_by_object[change.object_id].push_back(&change);
  const auto source_o2o = [&](std::string_view source) {
    const auto found = o2o_by_source.find(source);
    return found == o2o_by_source.end() ? std::vector<const OcelO2oRelation*>{} : found->second;
  };
  auto event_relations = [&](const OcelEvent& event) {
    std::vector<const OcelRelation*> relations;
    if (const auto found = relations_by_event.find(event.id); found != relations_by_event.end())
      relations = found->second;
    return relations;
  };
  // "minera:relationOrder" and "minera:objectChangeOrder" (see
  // parse_ocel_json_names): written when the log order of the relations, or
  // of the changes a reader takes back, is not the order the document nests
  // them in.
  const auto write_order_markers = [&](bool standard) {
    std::unordered_set<std::string_view> written_events;
    for (const auto* event : events) written_events.insert(event->id);
    std::vector<const OcelRelation*> nested, logged;
    for (const auto* event : events)
      if (const auto found = relations_by_event.find(event->id); found != relations_by_event.end())
        nested.insert(nested.end(), found->second.begin(), found->second.end());
    for (const auto& relation : log.relations)
      if (written_events.contains(relation.event_id)) logged.push_back(&relation);
    if (nested != logged) {
      out << ",\"minera:relationOrder\":[";
      for (std::size_t index = 0; index < logged.size(); ++index) {
        if (index) out << ',';
        write_string(out, logged[index]->event_id);
      }
      out << ']';
    }
    // An event related to one object more than once keeps every relation
    // (the reference readers would keep one).
    {
      std::set<std::pair<std::string_view, std::string_view>> pairs;
      for (const auto* relation : logged)
        if (!pairs.emplace(relation->event_id, relation->object_id).second) {
          out << ",\"minera:repeatedRelations\":true";
          break;
        }
    }
    std::vector<const OcelO2oRelation*> nested_o2o, logged_o2o;
    std::unordered_set<std::string_view> written_objects;
    for (const auto* object : objects) {
      written_objects.insert(object->id);
      if (const auto found = o2o_by_source.find(object->id); found != o2o_by_source.end())
        nested_o2o.insert(nested_o2o.end(), found->second.begin(), found->second.end());
    }
    for (const auto& relation : log.o2o)
      if (written_objects.contains(relation.source_id)) logged_o2o.push_back(&relation);
    if (nested_o2o != logged_o2o) {
      out << ",\"minera:o2oOrder\":[";
      for (std::size_t index = 0; index < logged_o2o.size(); ++index) {
        if (index) out << ',';
        write_string(out, logged_o2o[index]->source_id);
      }
      out << ']';
    }
    if (!standard) return;  // the classic form lists changes in log order
    // The changes the OCEL 2.0 reader takes back, in its order: per object,
    // per attribute name in first appearance (static values first), the
    // entries after the name's first one (a name without a static value
    // reads its first change as the static value).
    std::vector<const OcelObjectChange*> read_back;
    for (const auto* object : objects) {
      std::vector<std::string_view> names;
      std::unordered_map<std::string_view, std::vector<const OcelObjectChange*>> groups;
      std::unordered_set<std::string_view> with_static;
      for (const auto& attribute : object->attributes) {
        if (reference_drops_attribute(attribute.key)) continue;
        if (groups.try_emplace(attribute.key).second) names.push_back(attribute.key);
        with_static.insert(attribute.key);
      }
      std::vector<const OcelObjectChange*> changes;
      if (const auto found = changes_by_object.find(object->id); found != changes_by_object.end())
        changes = found->second;
      // Round ES: a change of an "ocel:"-prefixed field is written (its
      // static value is not), so the reader takes its first change back as
      // the static value.
      for (const auto* change : changes) {
        if (groups.try_emplace(change->field).second) names.push_back(change->field);
        groups[change->field].push_back(change);
      }
      for (const auto name : names) {
        const auto& group = groups[name];
        const std::size_t skip = with_static.contains(name) ? 0 : 1;
        for (std::size_t index = skip; index < group.size(); ++index) read_back.push_back(group[index]);
      }
    }
    std::unordered_set<const OcelObjectChange*> kept(read_back.begin(), read_back.end());
    std::vector<const OcelObjectChange*> logged_changes;
    for (const auto& change : log.object_changes)
      if (kept.contains(&change)) logged_changes.push_back(&change);
    if (logged_changes == read_back) return;
    out << ",\"minera:objectChangeOrder\":[";
    for (std::size_t index = 0; index < logged_changes.size(); ++index) {
      if (index) out << ',';
      out << '[';
      write_string(out, logged_changes[index]->object_id);
      out << ',';
      write_string(out, logged_changes[index]->field);
      out << ']';
    }
    out << ']';
  };
  // The frames the reference prints its cells from (write_frame_value).
  // Round EK: date values count as datetimes (a date column next to other
  // values is an object column, see write_frame_value).
  ocel_frame::Columns event_frame, object_frame, change_frame;
  std::unordered_map<std::string_view, std::string_view> object_type_of;
  for (const auto& object : log.objects) object_type_of.emplace(object.id, object.type);
  const auto change_owner = [&](const OcelObjectChange& change) -> std::string {
    if (!change.object_type.empty()) return change.object_type;
    const auto found = object_type_of.find(change.object_id);
    return found == object_type_of.end() ? std::string() : std::string(found->second);
  };
  const auto change_date = [&](const OcelObjectChange& change) {
    return ocel_frame::reference_date_value(log.object_type_attributes, change_owner(change), change.field,
                                            change.value);
  };
  {
    // Round EM: with the rows the reader or writer dropped (OcelFrameHint).
    event_frame = ocel_frame::event_frame(log, true);
    object_frame = ocel_frame::object_frame(log, true);
    change_frame.seed(log.change_frame_hint);
    for (const auto& change : log.object_changes) {
      change_frame.add_rows(1);
      change_frame.observe_change(change, log.object_type_attributes, change_owner(change));
    }
  }
  // Round EK: a download prints an object change's time as the reference
  // writers print the changes frame's datetime64 column, its isoformat
  // ("+00:00"); the transport keeps the text.
  const auto change_time = [&](const std::string& text) -> std::string {
    if (transport_markers || nat_change_time(text)) return text;
    return ocel_frame::utc_timestamp_text(text, 'T', "+00:00");
  };
  // Round EK: a download prints a row as the reference frame holds it:
  // attributes in the frame's column order, null cells left out (observed
  // black-box); the transport keeps the attributes as they are.
  const auto row_of = [&](const std::vector<OcelAttribute>& attrs, const ocel_frame::Columns& frame) {
    if (!transport_markers) return ocel_frame::frame_row(attrs, frame);
    std::vector<const OcelAttribute*> row;
    for (const auto& attribute : attrs) row.push_back(&attribute);
    return row;
  };
  auto write_standard_attributes = [&](const std::vector<OcelAttribute>& attrs, const std::string& owner) {
    out << '[';
    bool first = true;
    for (const auto* attribute : row_of(attrs, event_frame)) {
      if (reference_drops_attribute(attribute->key)) continue;
      if (!first) out << ',';
      first = false;
      out << "{\"name\":";
      write_string(out, attribute->key);
      out << ",\"value\":";
      write_frame_value(out, attribute->value, event_frame.printed_dtype(attribute->key),
                        ocel_frame::reference_date_value(log.event_type_attributes, owner, *attribute));
      out << '}';
    }
    out << ']';
  };
  auto write_standard_o2o = [&](std::string_view source) {
    std::vector<const OcelO2oRelation*> relations;
    if (const auto found = o2o_by_source.find(source); found != o2o_by_source.end())
      relations = found->second;
    out << '[';
    bool first = true;
    for (const auto* relation : relations) {
      if (!first) out << ',';
      first = false;
      out << "{\"objectId\":";
      write_string(out, relation->target_id);
      out << ",\"qualifier\":";
      write_qualifier(out, relation->qualifier, relation->qualifier_kind);
      out << '}';
    }
    out << ']';
  };

  if (format == OcelJsonFormat::kStandard) {
    // Round EG: write_ocel2_json raises on a NaT event time (an OCEL 2.0
    // SQLite '' or NULL time; observed black-box). A download does too; the
    // open source view of a route keeps the empty time.
    if (!transport_markers)
      for (const auto* event : events)
        if (ocel_raw_time_missing(event->raw_time) ||
            (event->raw_time == OcelRawTime::kNone && ocel_frame::nat_time_text(event->timestamp)))
          throw std::invalid_argument("KeyError: 'ocel:timestamp'");
    // Round EJ: and on a NaT change time (an OCEL 2.0 JSON change without a
    // time; observed black-box). Round ET: only of a change it writes, so
    // not of one whose value is missing (NaN, or a field without a column;
    // observed black-box, probe et t4 nat.py, fuzz file h87_0028).
    if (!transport_markers)
      for (const auto& change : log.object_changes) {
        if (std::holds_alternative<std::nullptr_t>(change.value)) continue;
        if (ocel_raw_time_missing(change.raw_time) ||
            (change.raw_time == OcelRawTime::kNone && nat_change_time(change.timestamp)))
          throw std::invalid_argument("KeyError: 'ocel:timestamp'");
      }
    StandardTypeTable object_types;
    StandardTypeTable event_types;
    collect_standard_types(log, object_types, event_types, transport_markers);
    // Round EZ: the transport keeps a date declaration where the values
    // type the column otherwise (dates next to texts, which then carry
    // "minera:textValues"), so the dates read back as dates.
    if (transport_markers) {
      const auto keep_dates = [](StandardTypeTable& table, const OcelTypeCatalog& catalog) {
        for (auto& [owner, declarations] : table)
          for (auto& [name, declared] : declarations) {
            std::string lowered = declared;
            for (auto& ch : lowered) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            const bool date_type = lowered.find("date") != std::string::npos || lowered.find("time") != std::string::npos;
            if (!date_type && ocel_frame::declared_date(catalog, owner, name)) declared = catalog.at(owner).at(name);
          }
      };
      keep_dates(object_types, log.object_type_attributes);
      keep_dates(event_types, log.event_type_attributes);
    }
    out << "{\"ocelVersion\":\"2.0\",\"objectTypes\":";
    write_standard_type_table(out, object_types);
    out << ",\"eventTypes\":";
    write_standard_type_table(out, event_types);
    out << ",\"events\":[";
    bool first = true;
    first = true;
    for (const auto* event : events) {
      if (!first) out << ',';
      first = false;
      out << "{\"id\":";
      write_string(out, event->id);
      out << ",\"type\":";
      write_string(out, event->activity);
      out << ",\"time\":";
      // Round EL: a raw time as its Python value (observed black-box).
      if (event->raw_time != OcelRawTime::kNone) write_raw_time(out, event->raw_time, event->timestamp);
      else write_string(out, event_time(event->timestamp));
      out << ",\"attributes\":";
      write_standard_attributes(event->attributes, event->activity);
      out << ",\"relationships\":[";
      bool relation_first = true;
      for (const auto* relation : event_relations(*event)) {
        if (!relation_first) out << ',';
        relation_first = false;
        out << "{\"objectId\":";
        write_string(out, relation->object_id);
        out << ",\"qualifier\":";
        write_qualifier(out, relation->qualifier, relation->qualifier_kind);
        out << '}';
      }
      out << "]}";
    }
    out << "],\"objects\":[";
    first = true;
    for (const auto* object : objects) {
      if (!first) out << ',';
      first = false;
      out << "{\"id\":";
      write_string(out, object->id);
      out << ",\"type\":";
      write_string(out, object->type);
      out << ",\"attributes\":[";
      bool attribute_first = true;
      std::vector<const OcelAttribute*> static_attributes;
      for (const auto* attribute : row_of(object->attributes, object_frame))
        if (!reference_drops_attribute(attribute->key)) static_attributes.push_back(attribute);
      for (const auto* attribute : static_attributes) {
        if (!attribute_first) out << ',';
        attribute_first = false;
        out << "{\"name\":";
        write_string(out, attribute->key);
        out << ",\"time\":\"1970-01-01T00:00:00Z\"";
        out << ",\"value\":";
        write_frame_value(out, attribute->value, object_frame.printed_dtype(attribute->key),
                          ocel_frame::reference_date_value(log.object_type_attributes, object->type, *attribute));
        out << '}';
      }
      std::vector<const OcelObjectChange*> changes;
      if (const auto found = changes_by_object.find(object->id); found != changes_by_object.end())
        changes = found->second;
      // Round EJ: in log order, as write_ocel2_json writes an object's
      // changes (observed black-box: not sorted by time).
      // A field without a static value gets no base entry: write_ocel2_json
      // writes only its changes (observed black-box), and a reader takes
      // the first change as the base, as pm4py reads its own export.
      // Round ES: write_ocel2_json drops an "ocel:"-prefixed column of the
      // objects frame but writes the changes of such a field (observed
      // black-box, probe es x3).
      for (const auto* change : changes) {
        // Round EK: write_ocel2_json leaves out a change whose value is
        // null (observed black-box); the transport keeps it.
        if (!transport_markers && std::holds_alternative<std::nullptr_t>(change->value)) continue;
        if (!attribute_first) out << ',';
        attribute_first = false;
        out << "{\"name\":";
        write_string(out, change->field);
        out << ",\"value\":";
        // Round EK: a change's datetime prints its isoformat (the reference
        // writes the changes frame's values so, observed black-box).
        write_frame_value(out, change->value,
                          change_date(*change) ? ocel_frame::Dtype::kObject : change_frame.dtype(change->field),
                          change_date(*change));
        out << ",\"time\":";
        if (change->raw_time != OcelRawTime::kNone) write_raw_time(out, change->raw_time, change->timestamp);
        else write_string(out, change_time(change->timestamp));
        out << '}';
      }
      out << "],\"relationships\":";
      write_standard_o2o(object->id);
      out << '}';
    }
    out << ']';
    if (!log.e2e.empty()) {
      out << ",\"eventRelationships\":";
      write_e2e(out, log, true);
    }
    // The reference writers' own globals (scope defaults, computed log
    // scope) are left out: read_ocel2_json declares empty scopes, so the
    // OCEL 1.0 writers then write them empty, as the reference does.
    if (std::any_of(log.globals.begin(), log.globals.end(), [](const auto& global) {
          return !ocel_reference_generated_global(global.first, global.second);
        })) {
      out << ",\"globals\":{";
      first = true;
      for (const auto& [key, value] : log.globals) {
        if (ocel_reference_generated_global(key, value)) continue;
        if (!first) out << ',';
        first = false;
        write_string(out, key);
        out << ':';
        write_value(out, value);
      }
      out << '}';
    }
    write_table_columns();
    write_keep_unrelated();
    // The OCEL 2.0 transport (the frontend's opened OCEL SQLite, split and
    // enriched logs) keeps times past the microsecond too (round BB).
    if (transport_markers) {
      write_untyped_relations();
      write_typed_qualifiers();
      write_text_values(true);
      write_naive_event_times();
      write_frame_hints();
      write_nanoseconds();
      write_order_markers(true);
    }
    out << '}';
    return out.str();
  }

  OcelTypeCatalog classic_object_types;
  OcelTypeCatalog classic_event_types;
  collect_classic_types(log, classic_object_types, classic_event_types);
  // Round EK: a download's type catalogs are write_ocel_json's, typed by
  // what each type's rows hold as write_ocel2_json types them (observed
  // black-box); the transport keeps the declared catalogs.
  if (!transport_markers) {
    StandardTypeTable object_table, event_table;
    collect_standard_types(log, object_table, event_table);
    const auto to_catalog = [](const StandardTypeTable& table) {
      OcelTypeCatalog catalog;
      for (const auto& [owner, declarations] : table) {
        auto& attributes = catalog[owner];
        for (const auto& [name, type] : declarations) attributes.emplace(name, type);
      }
      return catalog;
    };
    classic_object_types = to_catalog(object_table);
    classic_event_types = to_catalog(event_table);
  }
  ocel_frame::Columns classic_event_frame, classic_object_frame;
  if (!transport_markers) {
    // Round EM: with the rows the reader or writer dropped (OcelFrameHint).
    classic_event_frame = ocel_frame::event_frame(log, true);
    classic_object_frame = ocel_frame::object_frame(log, true);
  }

  out << "{\"ocel:events\":{";
  bool first = true;
  for (const auto* event : events) {
    if (!first) out << ',';
    first = false;
    write_string(out, event->id);
    out << ":{\"ocel:activity\":";
    write_string(out, event->activity);
    // Round EG: write_ocel_json leaves a NaT time out of the event entry
    // (observed black-box), so a download does; the transport keeps it.
    if (transport_markers && event->raw_time != OcelRawTime::kNone) {
      out << ",\"ocel:timestamp\":";
      write_string(out, event->timestamp);
      out << ",\"minera:rawTime\":\"" << raw_time_name(event->raw_time) << '"';
    } else if (event->raw_time != OcelRawTime::kNone) {
      // Round EL: write_ocel_json writes a raw time as its Python value, a
      // number as a JSON number, and leaves NaN out (observed black-box).
      if (!ocel_raw_time_missing(event->raw_time)) {
        out << ",\"ocel:timestamp\":";
        write_raw_time(out, event->raw_time, event->timestamp);
      }
    } else if (transport_markers || !ocel_frame::nat_time_text(event->timestamp)) {
      out << ",\"ocel:timestamp\":";
      write_string(out, event_time(event->timestamp));
    }
    if (!transport_markers)
      write_frame_entries(out, event->attributes, classic_event_frame, log.event_type_attributes, event->activity, true);
    const auto relations = event_relations(*event);
    out << ",\"ocel:omap\":[";
    bool relation_first = true;
    for (const auto* relation : relations) {
      if (!relation_first) out << ',';
      relation_first = false;
      write_string(out, relation->object_id);
    }
    out << "],\"ocel:typedOmap\":[";
    relation_first = true;
    for (const auto* relation : relations) {
      if (!relation_first) out << ',';
      relation_first = false;
      out << "{\"ocel:oid\":";
      write_string(out, relation->object_id);
      out << ",\"ocel:qualifier\":";
      write_qualifier(out, relation->qualifier, relation->qualifier_kind);
      out << '}';
    }
    out << "],\"ocel:vmap\":";
    if (transport_markers) write_attrs(out, event->attributes);
    else write_frame_attrs(out, event->attributes, classic_event_frame, log.event_type_attributes, event->activity);
    out << '}';
  }
  out << '}';
  // Round EO: write_ocel_json leaves out an empty type catalog (no events or
  // no objects left, observed black-box: c56_0010); the transport keeps it.
  if (transport_markers || !classic_event_types.empty()) {
    out << ",\"ocel:eventTypes\":";
    write_classic_type_catalog(out, classic_event_types);
  }
  if (transport_markers || !classic_object_types.empty()) {
    out << ",\"ocel:objectTypes\":";
    write_classic_type_catalog(out, classic_object_types);
  }
  if (!log.e2e.empty()) {
    out << ",\"ocel:e2e\":";
    write_e2e(out, log, false);
  }
  out << ",\"ocel:objects\":{";
  first = true;
  for (const auto* object : objects) {
    if (!first) out << ',';
    first = false;
    write_string(out, object->id);
    out << ":{\"ocel:type\":";
    write_string(out, object->type);
    out << ",\"ocel:ovmap\":";
    if (transport_markers) write_attrs(out, object->attributes);
    else write_frame_attrs(out, object->attributes, classic_object_frame, log.object_type_attributes, object->type);
    out << ",\"ocel:o2o\":";
    write_o2o(out, source_o2o(object->id));
    out << '}';
  }
  out << '}';
  // Round EO: write_ocel_json writes no "ocel:objectChanges" for a log
  // without objects (observed black-box: c56_0010); the transport keeps it.
  if (transport_markers || !objects.empty()) {
    out << ",\"ocel:objectChanges\":";
    write_object_changes(out, log, transport_markers);
  }
  out << ",\"ocel:global-log\":";
  if (transport_markers) {
    write_global_category(out, log, "ocel:global-log");
  } else {
    // Round EO: write_ocel_json writes the log scope it computes, whatever
    // the log read: the objects frame's types in order, the frames' sorted
    // attribute names (as write_ocel_xml lists them), version and ordering
    // (observed black-box).
    out << "{\"ocel:object-types\":[";
    std::unordered_set<std::string_view> seen_types;
    bool first_type = true;
    for (const auto* object : objects) {
      if (!seen_types.insert(object->type).second) continue;
      if (!first_type) out << ',';
      first_type = false;
      write_string(out, object->type);
    }
    out << "],\"ocel:attribute-names\":[";
    bool first_name = true;
    for (const auto& name : ocel_frame::frame_attribute_names(log)) {
      if (!first_name) out << ',';
      first_name = false;
      write_string(out, name);
    }
    out << "],\"ocel:version\":\"1.0\",\"ocel:ordering\":\"timestamp\"}";
  }
  out << ",\"ocel:global-event\":";
  write_global_category(out, log, "ocel:global-event");
  out << ",\"ocel:global-object\":";
  write_global_category(out, log, "ocel:global-object");
  write_table_columns();
  write_keep_unrelated();
  if (transport_markers) {
    write_untyped_relations();
    write_text_values(false);
    // Round EY: see read_classic_dates_as_texts.
    if (declares_dates(log)) out << ",\"minera:declaredDates\":true";
    write_naive_event_times();
    write_frame_hints();
    write_event_time_offset();
    write_nanoseconds();
    write_order_markers(false);
    // Round EE: read_ocel_json raises on an event time fromisoformat cannot
    // read (an empty or NaT time of a CSV or SQLite import, a text or float
    // time of an OCEL 1.0 XML one); the transport keeps them.
    if (std::any_of(log.events.begin(), log.events.end(), [](const OcelEvent& event) {
          if (event.raw_time != OcelRawTime::kNone) return true;
          try {
            (void)iso_wall_microseconds(event.timestamp);
            return false;
          } catch (const std::invalid_argument&) {
            return true;
          }
        }))
      out << ",\"minera:unreadableEventTimes\":true";
    // Round EW: an int or float qualifier travels as a JSON number, which
    // parse_ocel_json reads back only from a document that says so.
    write_typed_qualifiers();
    // Round EJ: a change time read_ocel_json cannot read (the NaT of an
    // OCEL 2.0 JSON change without a time) is kept between routes too.
    if (std::any_of(log.object_changes.begin(), log.object_changes.end(), [](const OcelObjectChange& change) {
          if (change.raw_time != OcelRawTime::kNone) return true;
          try {
            (void)iso_wall_microseconds(change.timestamp);
            return false;
          } catch (const std::invalid_argument&) {
            return true;
          }
        }))
      out << ",\"minera:unreadableChangeTimes\":true";
    if (log.event_activity_first) out << ",\"minera:eventActivityFirst\":true";
  }
  out << '}';
  return out.str();
}

EtotGraph discover_etot_classic(const Ocel& log) {
  EtotGraph result; std::set<std::string> activities, types; std::map<std::pair<std::string, std::string>, std::size_t> counts; std::unordered_map<std::string, std::string> lookup; std::unordered_map<std::string, std::string> event_activity; for (const auto& o : log.objects) lookup[o.id] = o.type; for (const auto& e : log.events) event_activity[e.id] = e.activity; for (const auto& r : log.relations) { auto it = lookup.find(r.object_id); auto a = event_activity.find(r.event_id); if (it == lookup.end() || a == event_activity.end()) continue; activities.insert(a->second); types.insert(it->second); ++counts[{a->second, it->second}]; } result.activities.assign(activities.begin(), activities.end()); result.object_types.assign(types.begin(), types.end()); for (const auto& [key, frequency] : counts) result.edges.push_back({key.first, key.second, frequency}); return result;
}

OtgGraph discover_otg_classic(const Ocel& log) {
  require_ocel_object_column(log, "ocel:type");
  require_ocel_relation_column(log, "ocel:eid");
  OtgGraph result; std::set<std::string> types; std::unordered_map<std::string, std::string> type; for (const auto& o : log.objects) { type[o.id] = o.type; types.insert(o.type); } result.object_types.assign(types.begin(), types.end());
  std::unordered_map<std::string, std::vector<std::string>> by_event;
  for (const auto& r : log.relations)
    by_event[r.event_id].push_back(r.object_id);
  std::vector<std::string> order;
  for (const auto& e : log.events) {
    const auto it = by_event.find(e.id);
    if (it == by_event.end()) throw std::out_of_range("'" + e.id + "'");
    order.push_back(e.id);
    auto& objects = it->second;
    std::sort(objects.begin(), objects.end());
    objects.erase(std::unique(objects.begin(), objects.end()), objects.end());
  }
  std::map<std::tuple<std::string, std::string, std::string>, std::size_t> counts; auto add = [&](const std::string& a, const std::string& rel, const std::string& b) { if (type.count(a) && type.count(b)) ++counts[{type[a], rel, type[b]}]; };
  std::set<std::pair<std::string, std::string>> interactions;
  for (const auto& ev : order) {
    const auto& objects = by_event.at(ev);
    for (std::size_t i = 0; i < objects.size(); ++i)
      for (std::size_t j = i + 1; j < objects.size(); ++j)
        interactions.emplace(objects[i], objects[j]);
  }
  for (const auto& [source, target] : interactions)
    add(source, "object_interaction", target);
  std::unordered_set<std::string> seen; for (const auto& ev : order) { std::unordered_set<std::string> old(seen.begin(), seen.end()); auto& v = by_event[ev]; for (const auto& a : v) if (old.count(a)) for (const auto& b : v) if (!old.count(b)) add(a, "object_descendants", b); for (const auto& a : v) seen.insert(a); }
  seen.clear(); for (const auto& ev : order) { auto& v = by_event[ev]; std::vector<std::string> fresh; for (const auto& a : v) if (!seen.count(a)) fresh.push_back(a); for (std::size_t i = 0; i < fresh.size(); ++i) for (std::size_t j = i + 1; j < fresh.size(); ++j) add(fresh[i], "object_cobirth", fresh[j]); for (const auto& a : fresh) seen.insert(a); }
  seen.clear(); for (auto it = order.rbegin(); it != order.rend(); ++it) { auto& v = by_event[*it]; std::vector<std::string> fresh; for (const auto& a : v) if (!seen.count(a)) fresh.push_back(a); for (std::size_t i = 0; i < fresh.size(); ++i) for (std::size_t j = i + 1; j < fresh.size(); ++j) add(fresh[i], "object_codeath", fresh[j]); for (const auto& a : fresh) seen.insert(a); }
  std::unordered_map<std::string, std::size_t> first, last; for (std::size_t i = 0; i < order.size(); ++i) for (const auto& a : by_event[order[i]]) { if (!first.count(a)) first[a] = i; last[a] = i; } std::set<std::pair<std::string, std::string>> inheritance; for (std::size_t i = 0; i < order.size(); ++i) { for (const auto& a : by_event[order[i]]) if (last[a] == i) for (const auto& b : by_event[order[i]]) if (first[b] == i && a != b) inheritance.insert({a, b}); } for (const auto& [a, b] : inheritance) if (!inheritance.count({b, a})) add(a, "object_inheritance", b);
  for (const auto& [key, frequency] : counts) result.edges.push_back({std::get<0>(key), std::get<1>(key), std::get<2>(key), frequency}); return result;
}

std::string etot_json(const EtotGraph& graph) {
  std::ostringstream out;
  out << "{\"activities\":[";
  for (std::size_t i = 0; i < graph.activities.size(); ++i) {
    if (i != 0) out << ',';
    write_string(out, graph.activities[i]);
  }
  out << "],\"object_types\":[";
  for (std::size_t i = 0; i < graph.object_types.size(); ++i) {
    if (i != 0) out << ',';
    write_string(out, graph.object_types[i]);
  }
  out << "],\"edges\":[";
  for (std::size_t i = 0; i < graph.edges.size(); ++i) {
    if (i != 0) out << ',';
    const auto& edge = graph.edges[i];
    out << "{\"activity\":";
    write_string(out, edge.activity);
    out << ",\"object_type\":";
    write_string(out, edge.object_type);
    out << ",\"frequency\":" << edge.frequency << '}';
  }
  out << "]}";
  return out.str();
}

std::string otg_json(const OtgGraph& graph) {
  std::ostringstream out;
  out << "{\"object_types\":[";
  for (std::size_t i = 0; i < graph.object_types.size(); ++i) {
    if (i != 0) out << ',';
    write_string(out, graph.object_types[i]);
  }
  out << "],\"edges\":[";
  for (std::size_t i = 0; i < graph.edges.size(); ++i) {
    if (i != 0) out << ',';
    const auto& edge = graph.edges[i];
    out << "{\"source_type\":";
    write_string(out, edge.source_type);
    out << ",\"relation\":";
    write_string(out, edge.relation);
    out << ",\"target_type\":";
    write_string(out, edge.target_type);
    out << ",\"frequency\":" << edge.frequency << '}';
  }
  out << "]}";
  return out.str();
}

EtotGraph parse_etot_json(std::string_view json, const OcelJsonLimits& limits) {
  const auto parsed = Parser(json, limits, true).parse();
  const auto& root = obj(parsed, "ETOT graph");
  if (root.find("activities_indep") != root.end() ||
      root.find("activities_ot") != root.end() ||
      root.find("start_activities") != root.end() ||
      root.find("end_activities") != root.end() ||
      root.find("performance_computed") != root.end()) {
    throw std::invalid_argument("ETOT graph envelope does not match the selected kind");
  }
  EtotGraph result;
  result.activities = string_array(&required(root, "activities"),
                                   "activities");
  result.object_types = string_array(&required(root, "object_types"),
                                     "object_types");
  const auto& raw_edges = required(root, "edges");
  if (!raw_edges.is_array())
    throw std::invalid_argument("ETOT edges must be an array");
  result.edges.reserve(raw_edges.array().size());
  for (const auto& raw : raw_edges.array()) {
    const auto& edge = obj(raw, "ETOT edge");
    result.edges.push_back(
        {str(required(edge, "activity"), "activity"),
         str(required(edge, "object_type"), "object_type"),
         size_value(required(edge, "frequency"), "frequency")});
  }
  return result;
}

OtgGraph parse_otg_json(std::string_view json, const OcelJsonLimits& limits) {
  const auto parsed = Parser(json, limits, true).parse();
  const auto& root = obj(parsed, "OTG graph");
  if (root.find("activities") != root.end() ||
      root.find("activities_indep") != root.end() ||
      root.find("activities_ot") != root.end() ||
      root.find("start_activities") != root.end() ||
      root.find("end_activities") != root.end() ||
      root.find("performance_computed") != root.end()) {
    throw std::invalid_argument("OTG graph envelope does not match the selected kind");
  }
  OtgGraph result;
  result.object_types = string_array(&required(root, "object_types"),
                                     "object_types");
  const auto& raw_edges = required(root, "edges");
  if (!raw_edges.is_array())
    throw std::invalid_argument("OTG edges must be an array");
  result.edges.reserve(raw_edges.array().size());
  for (const auto& raw : raw_edges.array()) {
    const auto& edge = obj(raw, "OTG edge");
    result.edges.push_back(
        {str(required(edge, "source_type"), "source_type"),
         str(required(edge, "relation"), "relation"),
         str(required(edge, "target_type"), "target_type"),
         size_value(required(edge, "frequency"), "frequency")});
  }
  return result;
}

OcdfgGraph discover_ocdfg_classic(const Ocel& log, bool compute_edges_performance) {
  OcdfgDiscoveryOptions options;
  options.compute_edges_performance = compute_edges_performance;
  return discover_ocdfg_classic_with_options(log, options);
}

OcdfgGraph discover_ocdfg_classic_with_options(
    const Ocel& log, const OcdfgDiscoveryOptions& options) {
  const auto column = [](const std::string& selected, bool event) {
    if (selected.empty() || (event && (selected == "activity" ||
                                      selected == "concept:name")) ||
        (!event && (selected == "type" || selected == "object_type")))
      return std::string(event ? "ocel:activity" : "ocel:type");
    return selected;
  };
  std::set<std::string> event_columns{"ocel:eid", "ocel:activity", "ocel:timestamp"};
  std::set<std::string> object_columns{"ocel:oid", "ocel:type"};
  for (const auto& event : log.events)
    for (const auto& attribute : event.attributes) event_columns.insert(attribute.key);
  for (const auto& object : log.objects)
    for (const auto& attribute : object.attributes) object_columns.insert(attribute.key);
  for (const auto& [type, schema] : log.event_type_attributes)
    for (const auto& [name, value_type] : schema) event_columns.insert(name);
  for (const auto& [type, schema] : log.object_type_attributes)
    for (const auto& [name, value_type] : schema) object_columns.insert(name);
  std::set<std::string> relation_columns = event_columns;
  relation_columns.insert(object_columns.begin(), object_columns.end());
  relation_columns.insert("ocel:qualifier");
  const auto require_column = [](const auto& declared, const auto& inferred,
                                  const std::string& name) {
    const bool found = declared
        ? std::find(declared->begin(), declared->end(), name) != declared->end()
        : inferred.count(name) != 0;
    if (!found) throw std::out_of_range("'" + name + "'");
  };
  const auto activity_column = column(options.activity_attribute, true);
  const auto type_column = column(options.object_type_attribute, false);
  // Round DV: a degenerate relations frame fails on the activity first.
  require_ocel_relation_column(log, activity_column);
  require_column(log.table_columns.events, event_columns, activity_column);
  require_column(log.table_columns.objects, object_columns, type_column);
  require_column(log.table_columns.relations, relation_columns, activity_column);
  require_column(log.table_columns.relations, relation_columns, type_column);
  const Ocel projected = project_ocel_labels(
      log, options.activity_attribute, options.object_type_attribute, "OCDFG");
  OcdfgGraph result;
  result.performance_computed = options.compute_edges_performance;
  std::map<std::string, std::string> activity;
  std::map<std::string, std::string> timestamp;
  std::map<std::string, std::size_t> event_order;
  for (std::size_t i = 0; i < projected.events.size(); ++i) {
    activity[projected.events[i].id] = projected.events[i].activity;
    timestamp[projected.events[i].id] = projected.events[i].timestamp;
    event_order[projected.events[i].id] = i;
    result.activities.push_back(projected.events[i].activity);
  }
  std::sort(result.activities.begin(), result.activities.end());
  result.activities.erase(std::unique(result.activities.begin(), result.activities.end()), result.activities.end());
  std::map<std::string, std::string> object_type;
  for (const auto& object : projected.objects) object_type[object.id] = object.type;
  for (const auto& object : projected.objects) result.object_types.push_back(object.type);
  std::sort(result.object_types.begin(), result.object_types.end());
  result.object_types.erase(std::unique(result.object_types.begin(), result.object_types.end()), result.object_types.end());

  struct Rel { std::string event; std::string object; std::size_t order; std::size_t index; };
  std::vector<Rel> rels;
  for (std::size_t i = 0; i < projected.relations.size(); ++i) {
    const auto& relation = projected.relations[i];
    const auto event_it = event_order.find(relation.event_id);
    const auto type_it = object_type.find(relation.object_id);
    if (event_it == event_order.end() || type_it == object_type.end()) continue;
    rels.push_back({relation.event_id, relation.object_id, event_it->second * (projected.relations.size() + 1) + i, i});
  }
  auto add_association = [&](std::map<std::string, OcdfgActivityMetric>& target,
                             const std::string& act, const std::string& event,
                             const std::string& object) {
    auto& metric = target[act];
    metric.events.push_back(event);
    metric.unique_objects.push_back(object);
    metric.event_objects.emplace_back(event, object);
    metric.total_objects.emplace_back(event, object);
  };
  for (const auto& relation : rels) {
    const auto act = activity[relation.event];
    const auto type = object_type[relation.object];
    add_association(result.activities_indep, act, relation.event, relation.object);
    add_association(result.activities_ot[type], act, relation.event, relation.object);
  }
  auto normalize_activity = [](OcdfgActivityMetric& metric) {
    std::sort(metric.events.begin(), metric.events.end());
    metric.events.erase(std::unique(metric.events.begin(), metric.events.end()), metric.events.end());
    std::sort(metric.unique_objects.begin(), metric.unique_objects.end());
    metric.unique_objects.erase(std::unique(metric.unique_objects.begin(), metric.unique_objects.end()), metric.unique_objects.end());
    // Association lists retain the relation-row multiplicity and order.
  };
  for (auto& [act, metric] : result.activities_indep) normalize_activity(metric);
  for (auto& [type, metrics] : result.activities_ot) for (auto& [act, metric] : metrics) normalize_activity(metric);

  std::map<std::string, std::vector<Rel>> by_object;
  for (const auto& relation : rels) by_object[relation.object].push_back(relation);
  for (auto& [object, sequence] : by_object) {
    std::sort(sequence.begin(), sequence.end(), [](const Rel& a, const Rel& b) { return a.order < b.order; });
    if (sequence.empty()) continue;
    const auto type = object_type[object];
    // Start and end activities take the object's first and last relation in
    // relation order, while the edges follow the event order; the two differ
    // only among events at one instant whose relations an OCEL SQLite file
    // lists in another order (round BZ, observed black-box).
    const auto by_index = [](const Rel& a, const Rel& b) { return a.index < b.index; };
    const auto& first = *std::min_element(sequence.begin(), sequence.end(), by_index);
    const auto& last = *std::max_element(sequence.begin(), sequence.end(), by_index);
    add_association(result.start_activities[type], activity[first.event], first.event, object);
    add_association(result.end_activities[type], activity[last.event], last.event, object);
    for (std::size_t i = 1; i < sequence.size(); ++i) {
      const auto source = activity[sequence[i - 1].event];
      const auto target = activity[sequence[i].event];
      auto& edge = result.edges[type][{source, target}];
      edge.event_couples.emplace_back(sequence[i - 1].event, sequence[i].event);
      edge.unique_objects.push_back(object);
      edge.event_objects.emplace_back(sequence[i - 1].event, sequence[i].event, object);
      edge.total_objects.emplace_back(sequence[i - 1].event, sequence[i].event,
                                      object);
    }
  }
  // Round EL: a raw time column (OcelRawTime) fails on the first pair the
  // reference measures, the one whose events come first in the log
  // (observed black-box).
  if (options.compute_edges_performance && ocel_raw_event_times(log)) {
    std::unordered_map<std::string_view, std::size_t> position;
    for (std::size_t i = 0; i < log.events.size(); ++i) position.emplace(log.events[i].id, i);
    const auto at = [&](const std::string& id) {
      const auto found = position.find(id);
      return found == position.end() ? log.events.size() : found->second;
    };
    std::optional<std::pair<std::size_t, std::size_t>> first;
    for (const auto& [type, metrics] : result.edges)
      for (const auto& [pair, edge] : metrics)
        for (const auto& [from, to] : edge.event_couples) {
          const std::pair<std::size_t, std::size_t> key{at(from), at(to)};
          if (key.first < log.events.size() && key.second < log.events.size() && (!first || key < *first))
            first = key;
        }
    if (first)
      throw ocdfg_raw_pair_error(log.events[first->first].raw_time, log.events[first->second].raw_time,
                                 options.edge_performance_options && options.edge_performance_options->business_hours);
  }
  for (auto& [type, metrics] : result.start_activities) for (auto& [act, metric] : metrics) normalize_activity(metric);
  for (auto& [type, metrics] : result.end_activities) for (auto& [act, metric] : metrics) normalize_activity(metric);
  for (auto& [type, metrics] : result.edges) for (auto& [pair, edge] : metrics) {
    std::sort(edge.event_couples.begin(), edge.event_couples.end()); edge.event_couples.erase(std::unique(edge.event_couples.begin(), edge.event_couples.end()), edge.event_couples.end());
    std::sort(edge.unique_objects.begin(), edge.unique_objects.end()); edge.unique_objects.erase(std::unique(edge.unique_objects.begin(), edge.unique_objects.end()), edge.unique_objects.end());
    std::sort(edge.event_objects.begin(), edge.event_objects.end()); edge.event_objects.erase(std::unique(edge.event_objects.begin(), edge.event_objects.end()), edge.event_objects.end());
    std::sort(edge.total_objects.begin(), edge.total_objects.end()); edge.total_objects.erase(std::unique(edge.total_objects.begin(), edge.total_objects.end()), edge.total_objects.end());
    edge.performance.clear();
    edge.performance_event_couples.clear();
    edge.performance_total_objects.clear();
    if (options.compute_edges_performance && options.edge_performance_options) {
      const auto& timing_options = *options.edge_performance_options;
      edge.performance_event_couples = calculate_ocdfg_edge_performance(
          log, edge.event_couples, timing_options);
      std::vector<std::pair<std::string, std::string>> object_couples;
      object_couples.reserve(edge.total_objects.size());
      for (const auto& triple : edge.total_objects)
        object_couples.emplace_back(std::get<0>(triple), std::get<1>(triple));
      edge.performance_total_objects = calculate_ocdfg_edge_performance(
          log, object_couples, timing_options);
      edge.performance = edge.performance_total_objects;
    } else if (options.compute_edges_performance) {
      // Round EH: a time column of None (Ocel::none_event_times) fails on
      // the first pair, as the reference's OC-DFG does (observed black-box).
      if (log.none_event_times && (!edge.event_couples.empty() || !edge.total_objects.empty()))
        throw OcdfgPerformanceError("TypeError",
                                    "unsupported operand type(s) for -: 'NoneType' and 'NoneType'");
      // Round EL: and so does a raw one (OcelRawTime).
      if (ocel_raw_event_times(log) && (!edge.event_couples.empty() || !edge.total_objects.empty())) {
        std::unordered_map<std::string_view, OcelRawTime> kinds;
        for (const auto& event : log.events) kinds.emplace(event.id, event.raw_time);
        const auto& [from, to] = !edge.event_couples.empty()
                                     ? edge.event_couples.front()
                                     : std::pair<std::string, std::string>(std::get<0>(edge.total_objects.front()),
                                                                           std::get<1>(edge.total_objects.front()));
        const auto kind = [&](const std::string& id) {
          const auto found = kinds.find(id);
          return found == kinds.end() ? OcelRawTime::kNaN : found->second;
        };
        throw ocdfg_raw_pair_error(kind(from), kind(to), false);
      }
      for (const auto& couple : edge.event_couples) {
        if (const auto seconds = legacy_edge_elapsed(timestamp[couple.first],
                                                     timestamp[couple.second]))
          edge.performance_event_couples.push_back(*seconds);
      }
      for (const auto& triple : edge.total_objects) {
        if (const auto seconds = legacy_edge_elapsed(timestamp[std::get<0>(triple)],
                                                     timestamp[std::get<1>(triple)]))
          edge.performance_total_objects.push_back(*seconds);
      }
      // Keep the historic field as the total-object projection.
      edge.performance = edge.performance_total_objects;
    }
    std::sort(edge.performance_event_couples.begin(),
              edge.performance_event_couples.end());
    std::sort(edge.performance_total_objects.begin(),
              edge.performance_total_objects.end());
    std::sort(edge.performance.begin(), edge.performance.end());
  }
  return result;
}

std::string serialize_ocdfg_json(const OcdfgGraph& graph) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<double>::max_digits10);
  const auto performance_number = [&](double value) {
    // A bare -0 is an integer token and loses its sign when read as JSON.
    if (value == 0.0 && std::signbit(value)) out << "-0.0";
    else out << value;
  };
  const auto strings = [&](const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i != 0) out << ',';
      write_string(out, values[i]);
    }
    out << ']';
  };
  const auto activity_metric = [&](const OcdfgActivityMetric& metric) {
    out << "{\"events\":";
    strings(metric.events);
    out << ",\"unique_objects\":";
    strings(metric.unique_objects);
    out << ",\"event_objects\":[";
    for (std::size_t i = 0; i < metric.event_objects.size(); ++i) {
      if (i != 0) out << ',';
      out << '[';
      write_string(out, metric.event_objects[i].first);
      out << ',';
      write_string(out, metric.event_objects[i].second);
      out << ']';
    }
    out << "],\"total_objects\":[";
    const auto& total_objects = metric.total_objects.empty()
                                    ? metric.event_objects
                                    : metric.total_objects;
    for (std::size_t i = 0; i < total_objects.size(); ++i) {
      if (i != 0) out << ',';
      out << '[';
      write_string(out, total_objects[i].first);
      out << ',';
      write_string(out, total_objects[i].second);
      out << ']';
    }
    out << "]}";
  };
  const auto activity_map = [&](const auto& metrics) {
    out << '{';
    bool first = true;
    for (const auto& [activity_name, metric] : metrics) {
      if (!first) out << ',';
      first = false;
      write_string(out, activity_name);
      out << ':';
      activity_metric(metric);
    }
    out << '}';
  };
  const auto nested_activity_map = [&](const auto& metrics) {
    out << '{';
    bool first_type = true;
    for (const auto& [object_type_name, activities_for_type] : metrics) {
      if (!first_type) out << ',';
      first_type = false;
      write_string(out, object_type_name);
      out << ':';
      activity_map(activities_for_type);
    }
    out << '}';
  };

  out << "{\"activities\":";
  strings(graph.activities);
  out << ",\"object_types\":";
  strings(graph.object_types);
  out << ",\"activities_indep\":";
  activity_map(graph.activities_indep);
  out << ",\"activities_ot\":";
  nested_activity_map(graph.activities_ot);
  out << ",\"start_activities\":";
  nested_activity_map(graph.start_activities);
  out << ",\"end_activities\":";
  nested_activity_map(graph.end_activities);
  out << ",\"edges\":{";
  bool first_type = true;
  for (const auto& [object_type_name, edges_for_type] : graph.edges) {
    if (!first_type) out << ',';
    first_type = false;
    write_string(out, object_type_name);
    out << ":{";
    bool first_edge = true;
    std::set<std::string> used_keys;
    for (const auto& [activity_pair, edge] : edges_for_type) {
      if (!first_edge) out << ',';
      first_edge = false;
      write_string(out, ocdfg_edge_key(used_keys, activity_pair.first, activity_pair.second));
      out << ":{\"source_activity\":";
      write_string(out, activity_pair.first);
      out << ",\"target_activity\":";
      write_string(out, activity_pair.second);
      out << ",\"event_couples\":[";
      for (std::size_t i = 0; i < edge.event_couples.size(); ++i) {
        if (i != 0) out << ',';
        out << '[';
        write_string(out, edge.event_couples[i].first);
        out << ',';
        write_string(out, edge.event_couples[i].second);
        out << ']';
      }
      out << "],\"unique_objects\":";
      strings(edge.unique_objects);
      out << ",\"event_objects\":[";
      for (std::size_t i = 0; i < edge.event_objects.size(); ++i) {
        if (i != 0) out << ',';
        out << '[';
        write_string(out, std::get<0>(edge.event_objects[i]));
        out << ',';
        write_string(out, std::get<1>(edge.event_objects[i]));
        out << ',';
        write_string(out, std::get<2>(edge.event_objects[i]));
        out << ']';
      }
      out << "],\"total_objects\":[";
      const auto& total_objects = edge.total_objects.empty()
                                      ? edge.event_objects
                                      : edge.total_objects;
      for (std::size_t i = 0; i < total_objects.size(); ++i) {
        if (i != 0) out << ',';
        out << '[';
        write_string(out, std::get<0>(total_objects[i]));
        out << ',';
        write_string(out, std::get<1>(total_objects[i]));
        out << ',';
        write_string(out, std::get<2>(total_objects[i]));
        out << ']';
      }
      out << "],\"performance\":[";
      for (std::size_t i = 0; i < edge.performance.size(); ++i) {
        if (i != 0) out << ',';
        out.precision(17);
        performance_number(edge.performance[i]);
      }
      out << "],\"performance_event_couples\":[";
      for (std::size_t i = 0; i < edge.performance_event_couples.size(); ++i) {
        if (i != 0) out << ',';
        out.precision(17);
        performance_number(edge.performance_event_couples[i]);
      }
      out << "],\"performance_total_objects\":[";
      for (std::size_t i = 0; i < edge.performance_total_objects.size(); ++i) {
        if (i != 0) out << ',';
        out.precision(17);
        performance_number(edge.performance_total_objects[i]);
      }
      out << "]}";
    }
    out << '}';
  }
  out << "},\"performance_computed\":"
      << (graph.performance_computed ? "true" : "false") << '}';
  return out.str();
}

OcdfgGraph parse_ocdfg_json(std::string_view json,
                            const OcelJsonLimits& limits) {
  const auto parsed = Parser(json, limits).parse();
  const auto& root = obj(parsed, "OCDFG graph");
  OcdfgGraph result;
  result.activities =
      string_array(&required(root, "activities"), "activities");
  result.object_types =
      string_array(&required(root, "object_types"), "object_types");

  const auto parse_activity_metric = [&](const Json& raw,
                                          std::string_view context) {
    const auto& fields = obj(raw, context);
    OcdfgActivityMetric metric;
    metric.events = string_array(optional(fields, "events"), "events");
    metric.unique_objects =
        string_array(optional(fields, "unique_objects"), "unique_objects");
    metric.event_objects =
        pair_array(optional(fields, "event_objects"), "event_objects");
    metric.total_objects =
        pair_array(optional(fields, "total_objects"), "total_objects");
    return metric;
  };

  const auto parse_activity_map = [&](const Json* raw,
                                      std::string_view context,
                                      std::map<std::string, OcdfgActivityMetric>&
                                          target) {
    if (raw == nullptr) return;
    const auto& values = obj(*raw, context);
    for (const auto& [name, metric] : values) {
      target.emplace(name, parse_activity_metric(
                              metric, std::string(context) + ":" + name));
    }
  };
  const auto parse_nested_activity_map =
      [&](const Json* raw, std::string_view context,
          std::map<std::string,
                   std::map<std::string, OcdfgActivityMetric>>& target) {
        if (raw == nullptr) return;
        const auto& values = obj(*raw, context);
        for (const auto& [type, activities] : values) {
          const auto& activity_values = obj(
              activities, std::string(context) + ":" + type);
          auto& destination = target[type];
          for (const auto& [name, metric] : activity_values) {
            destination.emplace(
                name, parse_activity_metric(
                          metric, std::string(context) + ":" + type + ":" +
                                      name));
          }
        }
      };

  parse_activity_map(optional(root, "activities_indep"), "activities_indep",
                     result.activities_indep);
  parse_nested_activity_map(optional(root, "activities_ot"), "activities_ot",
                            result.activities_ot);
  parse_nested_activity_map(optional(root, "start_activities"),
                            "start_activities", result.start_activities);
  parse_nested_activity_map(optional(root, "end_activities"), "end_activities",
                            result.end_activities);

  const auto parse_edge_metric = [&](const Json& raw,
                                     std::string_view context) {
    const auto& fields = obj(raw, context);
    OcdfgEdgeMetric metric;
    metric.event_couples =
        pair_array(optional(fields, "event_couples"), "event_couples");
    metric.unique_objects =
        string_array(optional(fields, "unique_objects"), "unique_objects");
    metric.event_objects =
        triple_array(optional(fields, "event_objects"), "event_objects");
    metric.total_objects =
        triple_array(optional(fields, "total_objects"), "total_objects");
    metric.performance =
        number_array(optional(fields, "performance"), "performance");
    metric.performance_event_couples = number_array(
        optional(fields, "performance_event_couples"),
        "performance_event_couples");
    metric.performance_total_objects = number_array(
        optional(fields, "performance_total_objects"),
        "performance_total_objects");
    return metric;
  };

  const auto& edge_types = obj(required(root, "edges"), "edges");
  for (const auto& [type, raw_for_type] : edge_types) {
    const auto& raw_map = obj(raw_for_type, "edges type " + type);
    auto& destination = result.edges[type];
    for (const auto& [key, raw_metric] : raw_map) {
      const auto& fields = obj(raw_metric, "OCDFG edge " + key);
      // The endpoint fields name the pair (an activity may be empty or hold
      // "->"); only a document without them splits the key.
      std::string source;
      std::string target;
      const auto* source_value = optional(fields, "source_activity");
      const auto* target_value = optional(fields, "target_activity");
      if (source_value) source = str(*source_value, "source_activity");
      if (target_value) target = str(*target_value, "target_activity");
      if (!source_value || !target_value) {
        const auto separator = key.find("->");
        if (separator == std::string::npos || separator == 0 ||
            separator + 2 >= key.size())
          throw std::invalid_argument(
              "OCDFG edge requires source_activity and target_activity");
        source = key.substr(0, separator);
        target = key.substr(separator + 2);
      }
      auto metric = parse_edge_metric(
          raw_metric, "OCDFG edge " + type + ":" + key);
      if (!destination.emplace(std::make_pair(source, target),
                               std::move(metric))
               .second)
        throw std::invalid_argument("duplicate OCDFG edge");
    }
  }
  if (const auto* performance = optional(root, "performance_computed")) {
    if (!performance->is_bool())
      throw std::invalid_argument(
          "OCDFG performance_computed must be boolean");
    result.performance_computed = std::get<bool>(performance->value);
  }
  return result;
}

OcpnGraph discover_ocpn(const Ocel& log, const OcpnDiscoveryOptions& options) {
  if (!std::isfinite(options.double_arc_threshold) ||
      options.double_arc_threshold < 0.0 || options.double_arc_threshold > 1.0)
    throw std::invalid_argument("OCPN double_arc_threshold must be in [0,1]");
  if (!std::isfinite(options.noise_threshold) || options.noise_threshold < 0.0 ||
      options.noise_threshold > 1.0)
    throw std::invalid_argument("OCPN noise_threshold must be in [0,1]");
  // Round DV: the public discovery reads the relations frame's activity
  // before the objects frame's type (SAW-net discovery flattens per type, see
  // discover_saw_nets).
  if (options.require_related_object_types)
    require_ocel_relation_column(log, options.activity_attribute.empty()
                                          ? std::string_view("ocel:activity")
                                          : std::string_view(options.activity_attribute));
  if (options.object_type_attribute.empty() || options.object_type_attribute == "ocel:type")
    require_ocel_object_column(log, "ocel:type");

  const Ocel projected = project_ocpn_labels(log, options);
  OcpnGraph result;
  result.annotated = options.annotated;
  std::map<std::string, const OcelEvent*> event_by_id;
  std::map<std::string, std::size_t> event_order;
  for (std::size_t index = 0; index < projected.events.size(); ++index) {
    event_by_id[projected.events[index].id] = &projected.events[index];
    event_order[projected.events[index].id] = index;
    result.activities.push_back(projected.events[index].activity);
  }
  std::sort(result.activities.begin(), result.activities.end());
  result.activities.erase(
      std::unique(result.activities.begin(), result.activities.end()),
      result.activities.end());
  std::map<std::string, std::string> object_type;
  for (const auto& object : projected.objects) {
    object_type[object.id] = object.type;
    result.object_types.push_back(object.type);
  }
  std::sort(result.object_types.begin(), result.object_types.end());
  result.object_types.erase(
      std::unique(result.object_types.begin(), result.object_types.end()),
      result.object_types.end());

  // Preserve the object slots observed on each event.  The per-type nets
  // remain useful projections, while this relation map lets synchronized
  // playout bind one transition to the exact object types that co-occurred.
  std::map<std::string,
           std::map<std::string, std::map<std::string, std::size_t>>>
      relation_counts;
  std::map<std::string, std::map<std::string, std::set<std::string>>>
      object_ids_by_activity_event;
  for (const auto& relation : projected.relations) {
    const auto event_it = event_by_id.find(relation.event_id);
    const auto type_it = object_type.find(relation.object_id);
    if (event_it == event_by_id.end() || type_it == object_type.end()) continue;
    const auto& activity = event_it->second->activity;
    auto& object_ids = object_ids_by_activity_event[activity][relation.event_id];
    if (object_ids.insert(relation.object_id).second)
      ++relation_counts[activity][type_it->second][relation.event_id];
  }
  for (const auto& [activity, by_type] : relation_counts) {
    for (const auto& [type, by_event] : by_type) {
      const bool variable = std::any_of(
          by_event.begin(), by_event.end(),
          [](const auto& entry) { return entry.second > 1; });
      if (variable) {
        result.activity_variable_object_types[activity].push_back(type);
      } else {
        result.activity_object_types[activity].push_back(type);
      }
    }
  }

  if (options.annotated) {
    result.annotations = discover_ocdfg_classic(projected, false);
  }
  if (options.require_related_object_types) {
    std::set<std::string> related;
    for (const auto& relation : projected.relations) {
      const auto type_it = object_type.find(relation.object_id);
      if (type_it != object_type.end() && event_by_id.contains(relation.event_id))
        related.insert(type_it->second);
    }
    // Round EP: pm4py's KeyError on such a type (observed black-box, probe
    // ep wip/k3.py: an OCEL CSV's type whose objects are listed but not
    // related); pm4py's types come in a set's order, so which type follows
    // the hash seed (declared): native names the first by name.
    const std::set<std::string> types(result.object_types.begin(), result.object_types.end());
    for (const auto& type : types)
      if (!related.contains(type))
        throw std::invalid_argument("KeyError: " + ocel_frame::python_str_repr(type));
  }
  // Each type's log is flattened from the objects whose original type equals
  // the type label: with an object-type selector the selected labels name
  // the types, but the flattening still matches the canonical types.
  std::map<std::string, std::string> flattening_type;
  for (const auto& object : log.objects) flattening_type[object.id] = object.type;
  for (const auto& type : result.object_types) {
    OcpnTokenReplayDiagnostics* token_replay_diagnostics = nullptr;
    if (options.diagnostics_with_token_based_replay) {
      auto& diagnostics = result.tbr_results[type];
      diagnostics.enabled = true;
      token_replay_diagnostics = &diagnostics;
    }
    // Each relation row of an object of the type is one event of its trace
    // (an event related twice to one object appears twice), as the
    // reference's flattening keeps every relation row (round BZ). The
    // flattened traces list an object's rows in event order (the events
    // frame's order, which the readers sort by time); the IMd edges, the
    // replay diagnostics and SAW discovery read them so, the classic miner
    // sorts them by time. The IMd graph takes its start and end activities
    // from each object's first and last relation row instead, like the OCDFG
    // (observed black-box: the orders differ at instants whose relations an
    // OCEL SQLite file lists in another order, and on logs whose events are
    // not in time order).
    std::map<std::string, std::vector<std::string>> object_rows;
    for (const auto& relation : projected.relations) {
      const auto type_it = flattening_type.find(relation.object_id);
      if (type_it == flattening_type.end() || type_it->second != type ||
          !object_type.contains(relation.object_id) || !event_by_id.count(relation.event_id))
        continue;
      object_rows[relation.object_id].push_back(relation.event_id);
    }
    EventLog replay_flattened;
    for (const auto& [object_id, event_ids] : object_rows) {
      Trace trace;
      trace.case_id = object_id;
      for (const auto& event_id : event_ids) {
        const auto* event = event_by_id.at(event_id);
        trace.events.push_back(Event{object_id, event->activity, event->timestamp,
                                     {}, event_order[event_id], {}, {}});
      }
      std::stable_sort(trace.events.begin(), trace.events.end(),
                       [](const Event& a, const Event& b) { return a.input_order < b.input_order; });
      if (!trace.events.empty()) replay_flattened.traces.push_back(std::move(trace));
    }
    EventLog flattened = replay_flattened;
    // Round EH: events with a NaT time go after the dated ones, in relation
    // order, as pandas sorts NaT last (observed black-box).
    if (!options.mine_in_relation_order)
      for (auto& trace : flattened.traces) {
        const auto undated = std::stable_partition(
            trace.events.begin(), trace.events.end(),
            [](const Event& event) { return !ocel_frame::nat_time_text(event.timestamp); });
        std::vector<Event> dated(std::make_move_iterator(trace.events.begin()),
                                 std::make_move_iterator(undated));
        order_events_by_time(dated);
        std::move(trace.events.begin() + (undated - trace.events.begin()), trace.events.end(),
                  std::back_inserter(dated));
        trace.events = std::move(dated);
      }

    InductiveMinerOptions miner_options;
    miner_options.noise_threshold = options.noise_threshold;
    miner_options.disable_fallthroughs = options.disable_fallthroughs;
    miner_options.disable_strict_sequence_cut =
        options.disable_strict_sequence_cut;
    miner_options.variant = options.variant == OcpnInductiveVariant::kDfg
                                ? InductiveMinerVariant::kDfg
                                : InductiveMinerVariant::kLog;
    if (options.variant == OcpnInductiveVariant::kDfg && !flattened.traces.empty()) {
      std::set<std::string> alphabet;
      for (const auto& trace : flattened.traces)
        for (const auto& event : trace.events) alphabet.insert(event.activity);
      const std::vector<std::string> names(alphabet.begin(), alphabet.end());
      std::map<std::string, std::size_t> ids;
      for (std::size_t id = 0; id < names.size(); ++id) ids.emplace(names[id], id);
      std::map<std::pair<std::size_t, std::size_t>, std::uint64_t> edge_counts;
      std::vector<std::uint64_t> starts(names.size(), 0), ends(names.size(), 0);
      for (const auto& trace : replay_flattened.traces)
        for (std::size_t i = 1; i < trace.events.size(); ++i)
          ++edge_counts[{ids.at(trace.events[i - 1].activity), ids.at(trace.events[i].activity)}];
      for (const auto& [object_id, event_ids] : object_rows) {
        if (event_ids.empty()) continue;
        ++starts[ids.at(event_by_id.at(event_ids.front())->activity)];
        ++ends[ids.at(event_by_id.at(event_ids.back())->activity)];
      }
      std::vector<DirectlyFollowsEdge> edges;
      for (const auto& [pair, count] : edge_counts) edges.push_back({pair.first, pair.second, count});
      const auto graph = DirectlyFollowsGraph::from_frequencies(names, edges, starts, ends);
      result.petri_nets[type] = process_tree_reference_net(discover_process_tree(graph, miner_options));
    } else {
      result.petri_nets[type] = process_tree_reference_net(discover_process_tree(flattened, miner_options));
    }

    if (token_replay_diagnostics != nullptr) {
      const auto replay = replay_token_diagnostics(
          replay_flattened, result.petri_nets.at(type));
      token_replay_diagnostics->trace_count = replay.trace_count;
      token_replay_diagnostics->fitting_traces = replay.fitting_traces;
      token_replay_diagnostics->transition_activations =
          std::move(replay.transition_activations);
      token_replay_diagnostics->places.reserve(replay.places.size());
      for (const auto& place : replay.places) {
        token_replay_diagnostics->places.push_back(
            {place.missing, place.remaining, place.consumed, place.produced});
      }
    }

    if (options.annotated) {
      const auto type_it = result.annotations.activities_ot.find(type);
      if (type_it != result.annotations.activities_ot.end()) {
        for (const auto& [activity, metric] : type_it->second) {
          std::map<std::string, std::size_t> event_counts;
          for (const auto& event_object : metric.event_objects)
            ++event_counts[event_object.first];
          std::size_t single_events = 0;
          for (const auto& [event, count] : event_counts) {
            (void)event;
            if (count == 1) ++single_events;
          }
          const double ratio = event_counts.empty()
                                   ? 0.0
                                   : static_cast<double>(single_events) /
                                         static_cast<double>(event_counts.size());
          result.double_arcs_on_activity[type][activity] =
              ratio <= options.double_arc_threshold;
        }
      }
    }
  }
  return result;
}

namespace {

void write_ocpn_annotation_fields(std::ostringstream& out,
                                  const OcdfgGraph& graph) {
  const auto performance_number = [&](double value) {
    if (value == 0.0 && std::signbit(value)) out << "-0.0";
    else out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
  };
  const auto strings = [&](const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i != 0) out << ',';
      write_string(out, values[i]);
    }
    out << ']';
  };
  const auto activity_metric = [&](const OcdfgActivityMetric& metric) {
    out << "{\"events\":";
    strings(metric.events);
    out << ",\"unique_objects\":";
    strings(metric.unique_objects);
    out << ",\"event_objects\":[";
    for (std::size_t i = 0; i < metric.event_objects.size(); ++i) {
      if (i != 0) out << ',';
      out << '[';
      write_string(out, metric.event_objects[i].first);
      out << ',';
      write_string(out, metric.event_objects[i].second);
      out << ']';
    }
    out << "],\"total_objects\":[";
    const auto& total_objects = metric.total_objects.empty()
                                    ? metric.event_objects
                                    : metric.total_objects;
    for (std::size_t i = 0; i < total_objects.size(); ++i) {
      if (i != 0) out << ',';
      out << '[';
      write_string(out, total_objects[i].first);
      out << ',';
      write_string(out, total_objects[i].second);
      out << ']';
    }
    out << "]}";
  };
  const auto activity_map = [&](const auto& metrics) {
    out << '{';
    bool first = true;
    for (const auto& [activity_name, metric] : metrics) {
      if (!first) out << ',';
      first = false;
      write_string(out, activity_name);
      out << ':';
      activity_metric(metric);
    }
    out << '}';
  };
  const auto nested_activity_map = [&](const auto& metrics) {
    out << '{';
    bool first_type = true;
    for (const auto& [object_type_name, activities_for_type] : metrics) {
      if (!first_type) out << ',';
      first_type = false;
      write_string(out, object_type_name);
      out << ':';
      activity_map(activities_for_type);
    }
    out << '}';
  };

  out << "\"activities_indep\":";
  activity_map(graph.activities_indep);
  out << ",\"activities_ot\":";
  nested_activity_map(graph.activities_ot);
  out << ",\"start_activities\":";
  nested_activity_map(graph.start_activities);
  out << ",\"end_activities\":";
  nested_activity_map(graph.end_activities);
  out << ",\"edges\":{";
  bool first_type = true;
  for (const auto& [object_type_name, edges_for_type] : graph.edges) {
    if (!first_type) out << ',';
    first_type = false;
    write_string(out, object_type_name);
    out << ":{";
    bool first_edge = true;
    std::set<std::string> used_keys;
    for (const auto& [activity_pair, edge] : edges_for_type) {
      if (!first_edge) out << ',';
      first_edge = false;
      write_string(out, ocdfg_edge_key(used_keys, activity_pair.first, activity_pair.second));
      out << ":{\"source_activity\":";
      write_string(out, activity_pair.first);
      out << ",\"target_activity\":";
      write_string(out, activity_pair.second);
      out << ",\"event_couples\":[";
      for (std::size_t i = 0; i < edge.event_couples.size(); ++i) {
        if (i != 0) out << ',';
        out << '[';
        write_string(out, edge.event_couples[i].first);
        out << ',';
        write_string(out, edge.event_couples[i].second);
        out << ']';
      }
      out << "],\"event_pairs\":[";
      for (std::size_t i = 0; i < edge.event_couples.size(); ++i) {
        if (i != 0) out << ',';
        out << '[';
        write_string(out, edge.event_couples[i].first);
        out << ',';
        write_string(out, edge.event_couples[i].second);
        out << ']';
      }
      out << "],\"unique_objects\":";
      strings(edge.unique_objects);
      out << ",\"event_objects\":[";
      for (std::size_t i = 0; i < edge.event_objects.size(); ++i) {
        if (i != 0) out << ',';
        out << '[';
        write_string(out, std::get<0>(edge.event_objects[i]));
        out << ',';
        write_string(out, std::get<1>(edge.event_objects[i]));
        out << ',';
        write_string(out, std::get<2>(edge.event_objects[i]));
        out << ']';
      }
      out << "],\"total_objects\":[";
      const auto& total_objects = edge.total_objects.empty()
                                      ? edge.event_objects
                                      : edge.total_objects;
      for (std::size_t i = 0; i < total_objects.size(); ++i) {
        if (i != 0) out << ',';
        out << '[';
        write_string(out, std::get<0>(total_objects[i]));
        out << ',';
        write_string(out, std::get<1>(total_objects[i]));
        out << ',';
        write_string(out, std::get<2>(total_objects[i]));
        out << ']';
      }
      out << ']';
      out << ",\"performance\":[";
      for (std::size_t i = 0; i < edge.performance.size(); ++i) {
        if (i != 0) out << ',';
        performance_number(edge.performance[i]);
      }
      out << "],\"performance_event_couples\":[";
      for (std::size_t i = 0; i < edge.performance_event_couples.size(); ++i) {
        if (i != 0) out << ',';
        performance_number(edge.performance_event_couples[i]);
      }
      out << "],\"performance_total_objects\":[";
      for (std::size_t i = 0; i < edge.performance_total_objects.size(); ++i) {
        if (i != 0) out << ',';
        performance_number(edge.performance_total_objects[i]);
      }
      out << "]}";
    }
    out << '}';
  }
  out << "},\"performance_computed\":"
      << (graph.performance_computed ? "true" : "false");
}

}  // namespace

std::string serialize_ocpn_json(const OcpnGraph& graph) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  const auto strings = [&](const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i != 0) out << ',';
      write_string(out, values[i]);
    }
    out << ']';
  };
  const auto numbers = [&](const std::vector<std::size_t>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i != 0) out << ',';
      out << values[i];
    }
    out << ']';
  };
  out << "{\"activities\":";
  strings(graph.activities);
  out << ",\"object_types\":";
  strings(graph.object_types);
  out << ",\"activity_object_types\":{";
  bool first_activity_binding = true;
  for (const auto& [activity, types] : graph.activity_object_types) {
    if (!first_activity_binding) out << ',';
    first_activity_binding = false;
    write_string(out, activity);
    out << ':';
    strings(types);
  }
  out << "},\"activity_variable_object_types\":{";
  bool first_variable_activity = true;
  for (const auto& [activity, types] : graph.activity_variable_object_types) {
    if (!first_variable_activity) out << ',';
    first_variable_activity = false;
    write_string(out, activity);
    out << ':';
    strings(types);
  }
  out << "},\"petri_nets\":{";
  bool first_type = true;
  for (const auto& [type, net] : graph.petri_nets) {
    if (!first_type) out << ',';
    first_type = false;
    write_string(out, type);
    out << ":{\"place_count\":" << net.place_count()
        << ",\"source\":" << net.source() << ",\"sink\":"
        << net.sink() << ",\"activities\":";
    strings(net.activities());
    out << ",\"transitions\":[";
    for (std::size_t index = 0; index < net.transitions().size(); ++index) {
      if (index != 0) out << ',';
      const auto& transition = net.transitions()[index];
      out << "{\"activity\":";
      if (transition.activity) out << *transition.activity;
      else out << "null";
      out << ",\"inputs\":";
      numbers(transition.inputs);
      out << ",\"outputs\":";
      numbers(transition.outputs);
      out << '}';
    }
    out << "]}";
  }
  out << "},\"double_arcs_on_activity\":{";
  first_type = true;
  for (const auto& [type, activities] : graph.double_arcs_on_activity) {
    if (!first_type) out << ',';
    first_type = false;
    write_string(out, type);
    out << ":{";
    bool first_activity = true;
    for (const auto& [activity, is_double] : activities) {
      if (!first_activity) out << ',';
      first_activity = false;
      write_string(out, activity);
      out << ':' << (is_double ? "true" : "false");
    }
    out << '}';
  }
  out << "},\"tbr_results\":{";
  bool first_diagnostic_type = true;
  for (const auto& [type, diagnostic] : graph.tbr_results) {
    if (!diagnostic.enabled) continue;
    if (!first_diagnostic_type) out << ',';
    first_diagnostic_type = false;
    write_string(out, type);
    out << ":{";
    out << "\"enabled\":" << (diagnostic.enabled ? "true" : "false")
        << ",\"trace_count\":" << diagnostic.trace_count
        << ",\"fitting_traces\":" << diagnostic.fitting_traces
        << ",\"places\":[";
    for (std::size_t place = 0; place < diagnostic.places.size(); ++place) {
      if (place != 0) out << ',';
      const auto& counts = diagnostic.places[place];
      out << "{\"m\":" << counts.missing
          << ",\"r\":" << counts.remaining
          << ",\"c\":" << counts.consumed
          << ",\"p\":" << counts.produced << '}';
    }
    out << "],\"transition_activations\":[";
    for (std::size_t transition = 0;
         transition < diagnostic.transition_activations.size(); ++transition) {
      if (transition != 0) out << ',';
      out << diagnostic.transition_activations[transition];
    }
    out << "]}";
  }
  out << "},\"annotations\":";
  if (graph.annotated) {
    out << '{';
    write_ocpn_annotation_fields(out, graph.annotations);
    out << '}';
  } else {
    out << "null";
  }
  out << '}';
  return out.str();
}

namespace {
// An event time as the reference prints its pandas Timestamp (str): UTC with
// "+00:00", fraction digits as stored (round BX). Round EH: in the column's
// offset when every time shares one non-zero offset (an OCEL CSV or SQLite
// import: pandas keeps that fixed offset as the column's zone, observed
// black-box), see ocel_frame::shared_nonzero_offset.
std::string description_time(const std::string& text, std::optional<std::int32_t> frame_offset) {
  // Round EG: a NaT time (an OCEL 2.0 SQLite '' or NULL) prints "NaT".
  if (ocel_frame::nat_time_text(text)) return "NaT";
  auto parsed = parse_timestamp_exact(text);
  if (!parsed) return text;
  if (!parsed->timezone_aware) return python::timestamp_str(*parsed);
  if (!frame_offset) return frame_times::utc_text(*parsed);
  parsed->offset_minutes = *frame_offset;
  return python::timestamp_str(*parsed);
}
}  // namespace

OcelTupleDescription describe_ocel_tuples(const Ocel& log,
                                           const OcelDescriptionOptions& options) {
  // Round DT: the reference selects the objects frame's id and type.
  require_ocel_object_columns(log);
  // Round DV: then it sorts the relations by their time.
  require_ocel_relation_column(log, "ocel:timestamp");
  OcelTupleDescription result;
  result.includes_timestamps = options.include_timestamps;
  std::map<std::string, const OcelEvent*> events;
  for (const auto& event : log.events) events[event.id] = &event;
  std::map<std::string, std::string> object_types;
  for (const auto& object : log.objects) object_types[object.id] = object.type;
  std::map<std::string, std::vector<std::string>> related_by_event;
  std::map<std::string, std::vector<std::string>> events_by_object;
  for (const auto& relation : log.relations) {
    if (!events.count(relation.event_id) || !object_types.count(relation.object_id))
      continue;
    related_by_event[relation.event_id].push_back(relation.object_id);
    events_by_object[relation.object_id].push_back(relation.event_id);
  }
  // Round EL: a raw time column (OcelRawTime) raises at the first object's
  // lifecycle, its last event's timestamp() taken first, with or without
  // the times in the text (observed black-box); round EM: its last event
  // with a time, as groupby's last() skips the missing ones (probe em
  // s54_0163).
  if (ocel_raw_event_times(log))
    for (const auto& object : log.objects)
      if (const auto found = events_by_object.find(object.id); found != events_by_object.end()) {
        auto kind = events.at(found->second.back())->raw_time;
        for (auto id = found->second.rbegin(); id != found->second.rend(); ++id)
          if (const auto own = events.at(*id)->raw_time; !ocel_raw_time_missing(own)) {
            kind = own;
            break;
          }
        throw std::invalid_argument(std::string("AttributeError: '") + ocel_raw_time_type(kind) +
                                    "' object has no attribute 'timestamp'");
      }
  // Related objects sort by id; a repeated event_object row lists its
  // object again ("o1, o1"), as pm4py keeps the rows (round CA).
  for (auto& [event_id, objects] : related_by_event) std::sort(objects.begin(), objects.end());
  // Events order by their exact instant (nanoseconds apart sort apart, round
  // BX), then activity and id.
  const auto event_less = [&](const std::string& left_id,
                              const std::string& right_id) {
    const auto* left = events.at(left_id);
    const auto* right = events.at(right_id);
    const auto left_time = parse_timestamp_exact(left->timestamp);
    const auto right_time = parse_timestamp_exact(right->timestamp);
    if (left_time && right_time && (left_time->seconds != right_time->seconds ||
                                    left_time->nanoseconds != right_time->nanoseconds))
      return left_time->seconds != right_time->seconds ? left_time->seconds < right_time->seconds
                                                       : left_time->nanoseconds < right_time->nanoseconds;
    // Round EG: a NaT time sorts after every time (pandas puts NaT last);
    // two NaT times tie.
    const bool left_nat = ocel_frame::nat_time_text(left->timestamp);
    const bool right_nat = ocel_frame::nat_time_text(right->timestamp);
    if (left_nat != right_nat) return right_nat;
    // Round EF: one instant spelled two ways (a classic JSON with "Z" and
    // "+00:00" times) is one time; only unreadable texts order as texts.
    if (!left_nat && !(left_time && right_time) && left->timestamp != right->timestamp)
      return left->timestamp < right->timestamp;
    if (left->activity != right->activity)
      return left->activity < right->activity;
    return left_id < right_id;
  };
  std::vector<std::string> event_ids;
  for (const auto& [event_id, event] : events) {
    (void)event;
    event_ids.push_back(event_id);
  }
  std::sort(event_ids.begin(), event_ids.end(), event_less);
  // Round EG: the reference takes every object's lifecycle from its
  // events' times first (with timestamps or without): NaT times are left
  // out, and an object whose events all have one raises, before the lookups
  // below (observed black-box).
  for (const auto& [object_id, object_events] : events_by_object)
    if (std::all_of(object_events.begin(), object_events.end(), [&](const std::string& id) {
          return ocel_frame::nat_time_text(events.at(id)->timestamp);
        }))
      throw std::invalid_argument(ocel_frame::nat_timestamp_error(log));
  // Round EF: the reference looks every event up among the related objects,
  // in this order, so an event without relations (kept by a CSV import or a
  // classic JSON whose objects frame dropped its objects) raises KeyError
  // with its id; with timestamps, so does an object without events, the
  // first by id, after the events (observed black-box).
  for (const auto& event_id : event_ids)
    if (!related_by_event.count(event_id))
      throw std::invalid_argument("KeyError: " + ocel_frame::python_str_repr(event_id));
  if (options.include_timestamps) {
    std::vector<std::string> unrelated_objects;
    for (const auto& object : log.objects)
      if (!events_by_object.count(object.id)) unrelated_objects.push_back(object.id);
    if (!unrelated_objects.empty())
      throw std::invalid_argument(
          "KeyError: " + ocel_frame::python_str_repr(
                             *std::min_element(unrelated_objects.begin(), unrelated_objects.end())));
  }
  const auto frame_offset = [&] {
    std::vector<std::string_view> texts;
    texts.reserve(log.events.size());
    for (const auto& event : log.events) texts.push_back(event.timestamp);
    return ocel_frame::shared_nonzero_offset(texts);
  }();
  for (const auto& event_id : event_ids) {
    const auto* event = events.at(event_id);
    OcelTupleEventDescription description;
    description.activity = event->activity;
    description.related_objects = related_by_event[event_id];
    if (options.include_timestamps) description.timestamp = description_time(event->timestamp, frame_offset);
    result.events.push_back(std::move(description));
  }

  for (auto& [object_id, object_events] : events_by_object) {
    std::sort(object_events.begin(), object_events.end(), event_less);
    object_events.erase(std::unique(object_events.begin(), object_events.end()),
                        object_events.end());
  }
  for (const auto& object : log.objects) {
    OcelTupleObjectDescription description;
    description.id = object.id;
    description.type = object.type;
    if (options.include_timestamps) {
      const auto it = events_by_object.find(object.id);
      if (it != events_by_object.end() && !it->second.empty()) {
        // NaT times sort last and stay out of the lifecycle (round EG).
        const auto timed_end = std::find_if(it->second.begin(), it->second.end(), [&](const std::string& id) {
          return ocel_frame::nat_time_text(events.at(id)->timestamp);
        });
        const auto* first = events.at(it->second.front());
        const auto* last = events.at(*std::prev(timed_end));
        description.lifecycle_start = description_time(first->timestamp, frame_offset);
        description.lifecycle_end = description_time(last->timestamp, frame_offset);
        const auto start = parse_timestamp_exact(first->timestamp);
        const auto end = parse_timestamp_exact(last->timestamp);
        if (!start || !end)
          throw std::invalid_argument("OCEL description requires valid timestamps");
        // end.timestamp() - start.timestamp(): epoch seconds rounded to the
        // microsecond (round BX, observed black-box).
        description.lifecycle_seconds = frame_times::epoch_seconds(*end) - frame_times::epoch_seconds(*start);
        description.has_lifecycle = true;
      }
    }
    result.objects.push_back(std::move(description));
  }
  std::sort(result.objects.begin(), result.objects.end(),
            [](const auto& left, const auto& right) { return left.id < right.id; });
  return result;
}

std::string describe_ocel_text(const Ocel& log,
                               const OcelDescriptionOptions& options) {
  const auto description = describe_ocel_tuples(log, options);
  std::ostringstream out;
  out << "\n\nevents:\n";
  for (const auto& event : description.events) {
    out << '\n' << event.activity << " ( related objects: ";
    for (std::size_t index = 0; index < event.related_objects.size(); ++index) {
      if (index != 0) out << ", ";
      out << event.related_objects[index];
    }
    out << " ) ";
    if (description.includes_timestamps)
      out << " timestamp: " << event.timestamp;
  }
  out << "\n\nobjects:\n";
  for (const auto& object : description.objects) {
    out << '\n' << object.id << " object type: " << object.type;
    if (description.includes_timestamps && object.has_lifecycle) {
      out << " ( lifecycle start: " << object.lifecycle_start
          << " ; lifecycle end: " << object.lifecycle_end
          << " ; lifecycle duration: "
          << ocel_frame::float_repr(object.lifecycle_seconds) << " )";
    }
  }
  return out.str();
}

std::string serialize_ocel_description_json(
    const OcelTupleDescription& description) {
  std::ostringstream out;
  out << "{\"includes_timestamps\":"
      << (description.includes_timestamps ? "true" : "false")
      << ",\"events\":[";
  for (std::size_t index = 0; index < description.events.size(); ++index) {
    if (index != 0) out << ',';
    const auto& event = description.events[index];
    out << "{\"activity\":";
    write_string(out, event.activity);
    out << ",\"related_objects\":[";
    for (std::size_t object_index = 0;
         object_index < event.related_objects.size(); ++object_index) {
      if (object_index != 0) out << ',';
      write_string(out, event.related_objects[object_index]);
    }
    out << "]";
    if (description.includes_timestamps) {
      out << ",\"timestamp\":";
      write_string(out, event.timestamp);
    }
    out << '}';
  }
  out << "],\"objects\":[";
  for (std::size_t index = 0; index < description.objects.size(); ++index) {
    if (index != 0) out << ',';
    const auto& object = description.objects[index];
    out << "{\"id\":";
    write_string(out, object.id);
    out << ",\"type\":";
    write_string(out, object.type);
    out << ",\"has_lifecycle\":"
        << (object.has_lifecycle ? "true" : "false");
    if (description.includes_timestamps) {
      out << ",\"lifecycle_start\":";
      write_string(out, object.lifecycle_start);
      out << ",\"lifecycle_end\":";
      write_string(out, object.lifecycle_end);
      out << ",\"lifecycle_seconds\":" << std::setprecision(17)
          << object.lifecycle_seconds;
    }
    out << '}';
  }
  out << "]}";
  return out.str();
}

const char* ocel_description_variant_name(OcelDescriptionVariant variant) {
  switch (variant) {
    case OcelDescriptionVariant::kVariant1: return "VARIANT1";
    case OcelDescriptionVariant::kVariant2: return "VARIANT2";
  }
  throw std::invalid_argument("OCEL description variant is invalid");
}

std::string serialize_ocel_description_json(
    const Ocel& log, OcelDescriptionVariant variant,
    const OcelDescriptionOptions& options) {
  if (variant != OcelDescriptionVariant::kVariant1 &&
      variant != OcelDescriptionVariant::kVariant2)
    throw std::invalid_argument("OCEL description variant is invalid");
  std::ostringstream out;
  out << "{\"family\":\"OCEL_DESCRIPTION\",\"variant\":\""
      << ocel_description_variant_name(variant)
      << "\",\"includes_timestamps\":"
      << (options.include_timestamps ? "true" : "false")
      << ",\"event_count\":" << log.events.size()
      << ",\"object_count\":" << log.objects.size();
  if (variant == OcelDescriptionVariant::kVariant1) {
    out << ",\"text\":";
    write_string(out, describe_ocel_text(log, options));
    out << '}';
    return out.str();
  }
  const auto description = describe_ocel_tuples(log, options);
  out << ",\"events\":[";
  for (std::size_t index = 0; index < description.events.size(); ++index) {
    if (index != 0) out << ',';
    const auto& event = description.events[index];
    out << '[';
    write_string(out, event.activity);
    for (const auto& object : event.related_objects) {
      out << ',';
      write_string(out, object);
    }
    if (description.includes_timestamps) {
      out << ',';
      write_string(out, event.timestamp);
    }
    out << ']';
  }
  out << "],\"event_object_counts\":[";
  for (std::size_t index = 0; index < description.events.size(); ++index) {
    if (index != 0) out << ',';
    out << description.events[index].related_objects.size();
  }
  out << "],\"objects\":[";
  for (std::size_t index = 0; index < description.objects.size(); ++index) {
    if (index != 0) out << ',';
    const auto& object = description.objects[index];
    out << '[';
    write_string(out, object.id);
    out << ',';
    write_string(out, object.type);
    if (description.includes_timestamps) {
      out << ',' << (object.has_lifecycle ? "true" : "false");
      out << ',';
      write_string(out, object.lifecycle_start);
      out << ',';
      write_string(out, object.lifecycle_end);
      out << ',' << std::setprecision(17) << object.lifecycle_seconds;
    }
    out << ']';
  }
  out << "]}";
  return out.str();
}

namespace {

double conformance_count_ratio(std::size_t numerator, std::size_t denominator) {
  if (denominator == 0) throw std::domain_error("ZeroDivisionError: division by zero");
  constexpr std::uint64_t exact_double_limit = std::uint64_t{1} << 53;
  if (numerator <= exact_double_limit && denominator <= exact_double_limit)
    return static_cast<double>(numerator) / static_cast<double>(denominator);

  // Round the integer quotient itself. Converting the operands separately
  // can lose a one-event difference or change the last bit of the ratio.
  const std::uint64_t divisor = denominator;
  const std::uint64_t quotient = numerator / divisor;
  std::uint64_t remainder = numerator % divisor;
  const auto next_bit = [&]() {
    // Double the remainder without overflowing a full-width counter.
    const bool bit = remainder >= divisor - remainder;
    remainder = bit ? remainder - (divisor - remainder) : remainder + remainder;
    return bit;
  };
  int exponent;
  std::uint64_t significand;
  if (quotient != 0) {
    exponent = static_cast<int>(std::bit_width(quotient)) - 1;
    if (exponent > 52) {
      const int shift = exponent - 52;
      significand = quotient >> shift;
      const auto discarded = quotient & ((std::uint64_t{1} << shift) - 1);
      const auto halfway = std::uint64_t{1} << (shift - 1);
      if (discarded > halfway ||
          (discarded == halfway && (remainder != 0 || (significand & 1))))
        ++significand;
      return std::ldexp(static_cast<double>(significand), exponent - 52);
    }
    significand = quotient;
    for (int i = exponent; i < 52; ++i)
      significand = (significand << 1) | next_bit();
  } else {
    if (numerator == 0) return 0.0;
    exponent = -1;
    while (!next_bit()) --exponent;
    significand = 1;
    for (int i = 0; i < 52; ++i)
      significand = (significand << 1) | next_bit();
  }
  if (remainder > divisor - remainder ||
      (remainder == divisor - remainder && (significand & 1)))
    ++significand;
  return std::ldexp(static_cast<double>(significand), exponent - 52);
}

double conformance_count_delta(std::size_t real, std::size_t normative) {
  return conformance_count_ratio(real >= normative ? real - normative
                                                   : normative - real,
                                 normative);
}

void write_conformance_number(std::ostringstream& out, double value) {
  if (!std::isfinite(value)) {
    out << "null";
    return;
  }
  // A bare -0 is an integer JSON token and discards the floating sign in
  // readers that distinguish integers from reals.
  if (value == 0.0 && std::signbit(value)) out << "-0.0";
  else out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
}

template <typename T>
std::vector<T> sorted_difference(const std::set<T>& left,
                                 const std::set<T>& right) {
  std::vector<T> result;
  std::set_difference(left.begin(), left.end(), right.begin(), right.end(),
                      std::back_inserter(result));
  return result;
}

std::map<std::pair<std::string, std::string>, std::size_t> etot_frequencies(
    const EtotGraph& graph) {
  std::map<std::pair<std::string, std::string>, std::size_t> result;
  for (const auto& edge : graph.edges)
    result[{edge.activity, edge.object_type}] += edge.frequency;
  return result;
}

std::map<std::tuple<std::string, std::string, std::string>, std::size_t>
otg_frequencies(const OtgGraph& graph) {
  std::map<std::tuple<std::string, std::string, std::string>, std::size_t>
      result;
  for (const auto& edge : graph.edges)
    result[{edge.source_type, edge.relation, edge.target_type}] += edge.frequency;
  return result;
}

std::set<std::pair<std::string, std::string>> ocdfg_flows(
    const OcdfgGraph& graph) {
  std::set<std::pair<std::string, std::string>> result;
  for (const auto& [object_type, edges] : graph.edges)
    for (const auto& [flow, metric] : edges) {
      (void)object_type;
      (void)metric;
      result.insert(flow);
    }
  return result;
}

std::size_t ocdfg_activity_measure(const OcdfgGraph& graph,
                                   const std::string& activity) {
  const auto it = graph.activities_indep.find(activity);
  return it == graph.activities_indep.end() ? 0u : it->second.events.size();
}

std::size_t ocdfg_flow_measure(const OcdfgGraph& graph,
                               const std::pair<std::string, std::string>& flow) {
  std::size_t result = 0;
  for (const auto& [object_type, edges] : graph.edges) {
    (void)object_type;
    const auto it = edges.find(flow);
    if (it != edges.end()) result += it->second.event_couples.size();
  }
  return result;
}

}  // namespace

EtotConformanceResult compare_etot(const EtotGraph& real,
                                   const EtotGraph& normative,
                                   const EtotConformanceOptions& options) {
  const std::set<std::string> real_activities(real.activities.begin(),
                                               real.activities.end());
  const std::set<std::string> normative_activities(normative.activities.begin(),
                                                    normative.activities.end());
  const std::set<std::string> real_types(real.object_types.begin(),
                                         real.object_types.end());
  const std::set<std::string> normative_types(normative.object_types.begin(),
                                               normative.object_types.end());
  const auto real_frequency = etot_frequencies(real);
  const auto normative_frequency = etot_frequencies(normative);
  std::set<std::pair<std::string, std::string>> real_edges;
  std::set<std::pair<std::string, std::string>> normative_edges;
  for (const auto& [edge, frequency] : real_frequency) {
    (void)frequency;
    real_edges.insert(edge);
  }
  for (const auto& [edge, frequency] : normative_frequency) {
    (void)frequency;
    normative_edges.insert(edge);
  }

  EtotConformanceResult result;
  result.missing_activities = sorted_difference(normative_activities, real_activities);
  result.additional_activities = sorted_difference(real_activities, normative_activities);
  result.missing_object_types = sorted_difference(normative_types, real_types);
  result.additional_object_types = sorted_difference(real_types, normative_types);
  result.missing_edges = sorted_difference(normative_edges, real_edges);
  result.additional_edges = sorted_difference(real_edges, normative_edges);

  std::size_t relative_mismatches = 0;
  for (const auto& edge : normative_edges) {
    if (!real_edges.count(edge)) continue;
    const auto normative_count = normative_frequency.at(edge);
    const auto real_count = real_frequency.at(edge);
    const double delta = conformance_count_delta(real_count, normative_count);
    result.relative_frequency_delta[edge] = delta;
    if (delta > options.theta_relative) ++relative_mismatches;
  }

  const double normalizer =
      options.alpha * static_cast<double>(normative_activities.size() + normative_types.size()) +
      options.beta * static_cast<double>(normative_edges.size()) +
      options.gamma * static_cast<double>(normative_edges.size());
  if (normalizer == 0.0) {
    throw std::domain_error("ZeroDivisionError: division by zero");
  } else {
    const double numerator =
        options.alpha * static_cast<double>(result.missing_activities.size() +
                                            result.missing_object_types.size()) +
        options.beta * static_cast<double>(result.missing_edges.size()) +
        options.gamma * static_cast<double>(relative_mismatches);
    result.fitness = 1.0 - numerator / normalizer;
  }
  return result;
}

EtotConformanceResult compare_etot(const Ocel& real,
                                   const EtotGraph& normative,
                                   const EtotConformanceOptions& options) {
  return compare_etot(discover_etot_classic(real), normative, options);
}

OtgConformanceResult compare_otg(const OtgGraph& real,
                                  const OtgGraph& normative,
                                  const OtgConformanceOptions& options) {
  const std::set<std::string> real_types(real.object_types.begin(),
                                         real.object_types.end());
  const std::set<std::string> normative_types(normative.object_types.begin(),
                                               normative.object_types.end());
  const auto real_frequency = otg_frequencies(real);
  const auto normative_frequency = otg_frequencies(normative);
  std::set<std::tuple<std::string, std::string, std::string>> real_edges;
  std::set<std::tuple<std::string, std::string, std::string>> normative_edges;
  for (const auto& [edge, frequency] : real_frequency) {
    (void)frequency;
    real_edges.insert(edge);
  }
  for (const auto& [edge, frequency] : normative_frequency) {
    (void)frequency;
    normative_edges.insert(edge);
  }

  OtgConformanceResult result;
  result.missing_object_types = sorted_difference(normative_types, real_types);
  result.additional_object_types = sorted_difference(real_types, normative_types);
  result.missing_edges = sorted_difference(normative_edges, real_edges);
  result.additional_edges = sorted_difference(real_edges, normative_edges);
  std::size_t non_conforming = 0;
  for (const auto& edge : normative_edges) {
    if (!real_edges.count(edge)) continue;
    const auto normative_count = normative_frequency.at(edge);
    const auto real_count = real_frequency.at(edge);
    const double delta = normative_count == 0
                             ? std::numeric_limits<double>::infinity()
                             : conformance_count_delta(real_count, normative_count);
    const auto relation = std::get<1>(edge);
    const auto theta_it = options.theta_by_relation.find(relation);
    const double theta = theta_it == options.theta_by_relation.end()
                             ? options.theta
                             : theta_it->second;
    if (delta > theta) {
      result.non_conforming_edges[edge] = delta;
      ++non_conforming;
    }
  }
  const double normalizer =
      options.alpha * static_cast<double>(normative_types.size()) +
      options.beta * static_cast<double>(normative_edges.size()) +
      options.gamma * static_cast<double>(normative_edges.size());
  if (normalizer == 0.0) {
    throw std::domain_error("ZeroDivisionError: division by zero");
  } else {
    const double numerator =
        options.alpha * static_cast<double>(result.missing_object_types.size()) +
        options.beta * static_cast<double>(result.missing_edges.size()) +
        options.gamma * static_cast<double>(non_conforming);
    result.fitness = 1.0 - numerator / normalizer;
  }
  return result;
}

OtgConformanceResult compare_otg(const Ocel& real,
                                  const OtgGraph& normative,
                                  const OtgConformanceOptions& options) {
  return compare_otg(discover_otg_classic(real), normative, options);
}

OcdfgConformanceResult compare_ocdfg(const OcdfgGraph& real,
                                     const OcdfgGraph& normative,
                                     const OcdfgConformanceOptions& options) {
  const std::set<std::string> real_activities(real.activities.begin(),
                                               real.activities.end());
  const std::set<std::string> normative_activities(normative.activities.begin(),
                                                    normative.activities.end());
  const auto real_flows = ocdfg_flows(real);
  const auto normative_flows = ocdfg_flows(normative);
  OcdfgConformanceResult result;
  result.missing_activities = sorted_difference(normative_activities, real_activities);
  result.additional_activities = sorted_difference(real_activities, normative_activities);
  result.missing_flows = sorted_difference(normative_flows, real_flows);
  result.additional_flows = sorted_difference(real_flows, normative_flows);

  std::set<std::string> all_activities = real_activities;
  all_activities.insert(normative_activities.begin(), normative_activities.end());
  std::size_t non_conforming_activities = 0;
  for (const auto& activity : all_activities) {
    const auto real_measure = ocdfg_activity_measure(real, activity);
    const auto normative_measure = ocdfg_activity_measure(normative, activity);
    const auto difference = static_cast<std::int64_t>(
        normative_measure >= real_measure ? normative_measure - real_measure
                                           : real_measure - normative_measure);
    result.activity_measure_differences[activity] = difference;
    if (static_cast<double>(difference) > options.theta_activity) {
      result.non_conforming_activities_in_measure.push_back(activity);
      ++non_conforming_activities;
    }
  }

  std::set<std::pair<std::string, std::string>> all_flows = real_flows;
  all_flows.insert(normative_flows.begin(), normative_flows.end());
  std::size_t non_conforming_flows = 0;
  for (const auto& flow : all_flows) {
    const auto real_measure = ocdfg_flow_measure(real, flow);
    const auto normative_measure = ocdfg_flow_measure(normative, flow);
    const auto difference = static_cast<std::int64_t>(
        normative_measure >= real_measure ? normative_measure - real_measure
                                           : real_measure - normative_measure);
    result.flow_measure_differences[flow] = difference;
    if (static_cast<double>(difference) > options.theta_flow) {
      result.non_conforming_flows_in_measure.push_back(flow);
      ++non_conforming_flows;
    }
  }

  const double normalizer =
      options.alpha * static_cast<double>(all_activities.size()) +
      options.beta * static_cast<double>(all_flows.size()) +
      options.gamma * static_cast<double>(all_activities.size()) +
      options.delta * static_cast<double>(all_flows.size());
  if (normalizer == 0.0) {
    result.fitness = 1.0;
  } else {
    const double numerator =
        options.alpha * static_cast<double>(result.missing_activities.size()) +
        options.beta * static_cast<double>(result.missing_flows.size()) +
        options.gamma * static_cast<double>(non_conforming_activities) +
        options.delta * static_cast<double>(non_conforming_flows);
    // Undefined weighted arithmetic clips to the lower bound as well.
    result.fitness = std::min(1.0, std::max(0.0, 1.0 - numerator / normalizer));
  }
  return result;
}

OcdfgConformanceResult compare_ocdfg(
    const Ocel& real, const OcdfgGraph& normative,
    const OcdfgConformanceOptions& options) {
  OcdfgDiscoveryOptions discovery;
  // Round EI: pm4py's discover_ocdfg always measures the edges, which fails
  // on a time column of None (Ocel::none_event_times), so that log measures
  // them here too, to raise as pm4py does.
  discovery.compute_edges_performance = real.none_event_times || ocel_raw_event_times(real);  // round EL: raw too
  return compare_ocdfg(
      discover_ocdfg_classic_with_options(real, discovery), normative,
      options);
}

std::string etot_conformance_json(const EtotConformanceResult& result) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  const auto strings = [&](const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i != 0) out << ',';
      write_string(out, values[i]);
    }
    out << ']';
  };
  const auto edges = [&](const auto& values) {
    out << '[';
    bool first = true;
    for (const auto& edge : values) {
      if (!first) out << ',';
      first = false;
      out << '[';
      write_string(out, edge.first);
      out << ',';
      write_string(out, edge.second);
      out << ']';
    }
    out << ']';
  };
  out << "{\"fitness\":";
  write_conformance_number(out, result.fitness);
  out << ",\"missing_activities\":";
  strings(result.missing_activities);
  out << ",\"additional_activities\":";
  strings(result.additional_activities);
  out << ",\"missing_object_types\":";
  strings(result.missing_object_types);
  out << ",\"additional_object_types\":";
  strings(result.additional_object_types);
  out << ",\"missing_edges\":";
  edges(result.missing_edges);
  out << ",\"additional_edges\":";
  edges(result.additional_edges);
  out << ",\"relative_frequency_delta\":[";
  bool first = true;
  for (const auto& [edge, delta] : result.relative_frequency_delta) {
    if (!first) out << ',';
    first = false;
    out << "{\"activity\":";
    write_string(out, edge.first);
    out << ",\"object_type\":";
    write_string(out, edge.second);
    out << ",\"delta\":";
    write_conformance_number(out, delta);
    out << '}';
  }
  out << "]}";
  return out.str();
}

std::string otg_conformance_json(const OtgConformanceResult& result) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  const auto strings = [&](const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i != 0) out << ',';
      write_string(out, values[i]);
    }
    out << ']';
  };
  const auto edges = [&](const auto& values) {
    out << '[';
    bool first = true;
    for (const auto& edge : values) {
      if (!first) out << ',';
      first = false;
      out << '[';
      write_string(out, std::get<0>(edge));
      out << ',';
      write_string(out, std::get<1>(edge));
      out << ',';
      write_string(out, std::get<2>(edge));
      out << ']';
    }
    out << ']';
  };
  out << "{\"fitness\":";
  write_conformance_number(out, result.fitness);
  out << ",\"missing_object_types\":";
  strings(result.missing_object_types);
  out << ",\"additional_object_types\":";
  strings(result.additional_object_types);
  out << ",\"missing_edges\":";
  edges(result.missing_edges);
  out << ",\"additional_edges\":";
  edges(result.additional_edges);
  out << ",\"non_conforming_edges\":[";
  bool first = true;
  for (const auto& [edge, delta] : result.non_conforming_edges) {
    if (!first) out << ',';
    first = false;
    out << "{\"source_type\":";
    write_string(out, std::get<0>(edge));
    out << ",\"relation\":";
    write_string(out, std::get<1>(edge));
    out << ",\"target_type\":";
    write_string(out, std::get<2>(edge));
    out << ",\"delta\":";
    write_conformance_number(out, delta);
    out << '}';
  }
  out << "]}";
  return out.str();
}

std::string ocdfg_conformance_json(const OcdfgConformanceResult& result) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  const auto strings = [&](const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i != 0) out << ',';
      write_string(out, values[i]);
    }
    out << ']';
  };
  const auto flows = [&](const auto& values) {
    out << '[';
    bool first = true;
    for (const auto& flow : values) {
      if (!first) out << ',';
      first = false;
      out << '[';
      write_string(out, flow.first);
      out << ',';
      write_string(out, flow.second);
      out << ']';
    }
    out << ']';
  };
  out << "{\"fitness\":";
  write_conformance_number(out, result.fitness);
  out << ",\"missing_activities\":";
  strings(result.missing_activities);
  out << ",\"additional_activities\":";
  strings(result.additional_activities);
  out << ",\"missing_flows\":";
  flows(result.missing_flows);
  out << ",\"additional_flows\":";
  flows(result.additional_flows);
  out << ",\"activity_measure_differences\":{";
  bool first = true;
  for (const auto& [activity, difference] : result.activity_measure_differences) {
    if (!first) out << ',';
    first = false;
    write_string(out, activity);
    out << ':' << difference;
  }
  out << "},\"non_conforming_activities_in_measure\":";
  strings(result.non_conforming_activities_in_measure);
  out << ",\"flow_measure_differences\":[";
  first = true;
  for (const auto& [flow, difference] : result.flow_measure_differences) {
    if (!first) out << ',';
    first = false;
    out << "{\"source_activity\":";
    write_string(out, flow.first);
    out << ",\"target_activity\":";
    write_string(out, flow.second);
    out << ",\"difference\":" << difference << '}';
  }
  out << "],\"non_conforming_flows_in_measure\":";
  flows(result.non_conforming_flows_in_measure);
  out << '}';
  return out.str();
}

}  // namespace minera
