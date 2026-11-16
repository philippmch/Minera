// Repeated dict keys in the filter banks' typed arguments (round CW).
//
// The bank codec writes a dict as {"m": [[key, value], ...]} straight from
// the generator, so a pair list may repeat a key. The reference decodes it as
// {decode(k): decode(v) for k, v in pairs}: a key equal to an earlier one
// under Python's == (1 == 1.0 == True, naive and aware datetimes never equal,
// NaN never equal) keeps the earlier key and its place and takes the later
// value. python_dict_pairs rewrites every such pair list in a call's
// arguments before a bank replay decodes them, so all four bank drivers read
// a dict as the reference built it.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace minera::bank_dict_pairs {

template <typename Json>
bool tagged(const Json& node) {
  return node.kind == Json::kObject && node.fields.size() == 1;
}

template <typename Json>
std::string tag_name(const Json& node) {
  return tagged(node) ? node.fields.begin()->first : std::string{};
}

inline std::optional<double> float_bits(const std::string& hex) {
  if (hex.size() != 16) return std::nullopt;
  std::uint64_t bits = 0;
  for (const char c : hex) {
    bits <<= 4;
    if (c >= '0' && c <= '9') bits |= static_cast<std::uint64_t>(c - '0');
    else if (c >= 'a' && c <= 'f') bits |= static_cast<std::uint64_t>(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') bits |= static_cast<std::uint64_t>(c - 'A' + 10);
    else return std::nullopt;
  }
  double out = 0;
  std::memcpy(&out, &bits, sizeof out);
  return out;
}

inline std::string normal_integer(std::string text) {
  if (!text.empty() && text.front() == '+') text.erase(0, 1);
  const bool negative = !text.empty() && text.front() == '-';
  std::size_t first = negative ? 1 : 0;
  while (first + 1 < text.size() && text[first] == '0') ++first;
  std::string digits = text.substr(first);
  if (digits == "0" || digits.empty()) return "0";
  return negative ? "-" + digits : digits;
}

// The exact integer a bool, int or integral float key equals, as decimal text.
template <typename Json>
std::optional<std::string> integer_text(const Json& node) {
  const auto tag = tag_name(node);
  const auto& value = node.fields.begin()->second;
  if (tag == "b") return std::string(value.boolean ? "1" : "0");
  if (tag == "i") return normal_integer(value.text);
  if (tag == "f") {
    const auto number = float_bits(value.text);
    if (!number || !std::isfinite(*number) || std::floor(*number) != *number) return std::nullopt;
    char buffer[400];
    std::snprintf(buffer, sizeof buffer, "%.0f", *number);
    return normal_integer(buffer);
  }
  return std::nullopt;
}

// Microseconds of a naive "YYYY-MM-DD[ T]HH:MM:SS[.ffffff]" stamp since 1970.
inline std::optional<long long> stamp_micros(const std::string& text) {
  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
  if (std::sscanf(text.c_str(), "%d-%d-%d%*c%d:%d:%d", &year, &month, &day, &hour, &minute, &second) < 3) return std::nullopt;
  long long micros = 0;
  const auto dot = text.find('.', 10);
  if (dot != std::string::npos) {
    std::string digits;
    for (std::size_t i = dot + 1; i < text.size() && digits.size() < 6 && text[i] >= '0' && text[i] <= '9'; ++i) digits += text[i];
    while (digits.size() < 6) digits += '0';
    micros = std::stoll(digits);
  }
  // days_from_civil
  const int y = year - (month <= 2 ? 1 : 0);
  const long long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = static_cast<unsigned>((153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1);
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const long long days = era * 146097 + static_cast<long long>(doe) - 719468;
  return ((days * 24 + hour) * 60 + minute) * 60 * 1000000LL + static_cast<long long>(second) * 1000000LL + micros;
}

template <typename Json>
bool python_equal(const Json& a, const Json& b) {
  if (!tagged(a) || !tagged(b)) return false;
  const auto ta = tag_name(a), tb = tag_name(b);
  const auto& va = a.fields.begin()->second;
  const auto& vb = b.fields.begin()->second;
  const auto numeric = [](const std::string& t) { return t == "b" || t == "i" || t == "f"; };
  if (numeric(ta) && numeric(tb)) {
    if (ta == "f" && tb == "f") {
      const auto x = float_bits(va.text), y = float_bits(vb.text);
      return x && y && *x == *y;
    }
    const auto x = integer_text(a), y = integer_text(b);
    return x && y && *x == *y;
  }
  if (ta != tb) return false;
  if (ta == "n") return true;
  if (ta == "s" || ta == "date") return va.text == vb.text;
  if (ta == "t") {
    if (va.items.size() != vb.items.size()) return false;
    for (std::size_t i = 0; i < va.items.size(); ++i)
      if (!python_equal(va.items[i], vb.items[i])) return false;
    return true;
  }
  if (ta == "w") {
    if (va.items.size() != 2 || vb.items.size() != 2) return false;
    const bool aware_a = va.items[1].kind != Json::kNull, aware_b = vb.items[1].kind != Json::kNull;
    if (aware_a != aware_b) return false;
    const auto x = stamp_micros(va.items[0].text), y = stamp_micros(vb.items[0].text);
    if (!x || !y) return va.items[0].text == vb.items[0].text && (!aware_a || va.items[1].text == vb.items[1].text);
    if (!aware_a) return *x == *y;
    return *x - std::stoll(va.items[1].text) * 60000000LL == *y - std::stoll(vb.items[1].text) * 60000000LL;
  }
  // Lists, sets and dicts are unhashable: the reference raises on them.
  return false;
}

template <typename Json>
void python_dict_pairs(Json& node) {
  for (auto& item : node.items) python_dict_pairs(item);
  for (auto& field : node.fields) python_dict_pairs(field.second);
  if (tag_name(node) != "m") return;
  auto& pairs = node.fields.begin()->second;
  if (pairs.kind != Json::kArray) return;
  std::vector<Json> kept;
  for (auto& pair : pairs.items) {
    bool merged = false;
    if (pair.kind == Json::kArray && pair.items.size() == 2)
      for (auto& earlier : kept)
        if (earlier.kind == Json::kArray && earlier.items.size() == 2 && python_equal(earlier.items[0], pair.items[0])) {
          earlier.items[1] = std::move(pair.items[1]);
          merged = true;
          break;
        }
    if (!merged) kept.push_back(std::move(pair));
  }
  pairs.items = std::move(kept);
}

// Normalizes the args and kwargs of every call of a bank.
template <typename Json>
void python_dict_pairs_in_calls(Json& bank) {
  auto found = bank.fields.find("cases");
  if (found == bank.fields.end()) return;
  for (auto& item : found->second.items) {
    auto calls = item.fields.find("calls");
    if (calls == item.fields.end()) continue;
    for (auto& call : calls->second.items) {
      for (const char* key : {"args", "kwargs"}) {
        auto field = call.fields.find(key);
        if (field != call.fields.end()) python_dict_pairs(field->second);
      }
    }
  }
}

}  // namespace minera::bank_dict_pairs
