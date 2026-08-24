#pragma once

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "httplib.h"
#include "minera/embeddings.hpp"

namespace minera::embedding_runtime {
namespace detail {

inline void skip_space(std::string_view input, std::size_t& position) {
  while (position < input.size()) {
    const auto c = static_cast<unsigned char>(input[position]);
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') return;
    ++position;
  }
}

inline void expect(std::string_view input, std::size_t& position, char value,
                   std::string_view context) {
  skip_space(input, position);
  if (position >= input.size() || input[position] != value)
    throw std::invalid_argument("embedding runtime: malformed " +
                                std::string(context));
  ++position;
}

inline std::string escape_json(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size() + 2);
  escaped.push_back('"');
  static constexpr char hex[] = "0123456789abcdef";
  for (const unsigned char c : value) {
    switch (c) {
      case '"': escaped += "\\\""; break;
      case '\\': escaped += "\\\\"; break;
      case '\b': escaped += "\\b"; break;
      case '\f': escaped += "\\f"; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default:
        if (c < 0x20) {
          escaped += "\\u00";
          escaped.push_back(hex[c >> 4]);
          escaped.push_back(hex[c & 0x0f]);
        } else {
          escaped.push_back(static_cast<char>(c));
        }
        break;
    }
  }
  escaped.push_back('"');
  return escaped;
}

inline double parse_number(std::string_view input, std::size_t& position) {
  skip_space(input, position);
  const auto start = position;
  if (position < input.size() && input[position] == '-') ++position;
  if (position >= input.size() || input[position] < '0' ||
      input[position] > '9')
    throw std::invalid_argument("embedding runtime: vector contains a non-number");
  if (input[position] == '0') {
    ++position;
    if (position < input.size() && input[position] >= '0' &&
        input[position] <= '9')
      throw std::invalid_argument("embedding runtime: malformed vector number");
  } else {
    while (position < input.size() && input[position] >= '0' &&
           input[position] <= '9')
      ++position;
  }
  if (position < input.size() && input[position] == '.') {
    ++position;
    const auto fraction_start = position;
    while (position < input.size() && input[position] >= '0' &&
           input[position] <= '9')
      ++position;
    if (position == fraction_start)
      throw std::invalid_argument("embedding runtime: malformed vector number");
  }
  if (position < input.size() &&
      (input[position] == 'e' || input[position] == 'E')) {
    ++position;
    if (position < input.size() &&
        (input[position] == '+' || input[position] == '-'))
      ++position;
    const auto exponent_start = position;
    while (position < input.size() && input[position] >= '0' &&
           input[position] <= '9')
      ++position;
    if (position == exponent_start)
      throw std::invalid_argument("embedding runtime: malformed vector exponent");
  }
  const std::string token(input.substr(start, position - start));
  char* end = nullptr;
  const auto value = std::strtod(token.c_str(), &end);
  if (end != token.c_str() + token.size() || !std::isfinite(value))
    throw std::invalid_argument("embedding runtime: vector contains a non-finite number");
  return value;
}

inline std::vector<double> parse_response(std::string_view body,
                                          std::size_t dimensions) {
  std::size_t position = 0;
  expect(body, position, '{', "response object");
  skip_space(body, position);
  constexpr std::string_view key = "\"embedding\"";
  if (body.substr(position, key.size()) != key)
    throw std::invalid_argument(
        "embedding runtime: response must contain only an embedding field");
  position += key.size();
  expect(body, position, ':', "response field");
  expect(body, position, '[', "embedding array");
  std::vector<double> values;
  values.reserve(dimensions == 0 ? 64 : dimensions);
  skip_space(body, position);
  if (position >= body.size() || body[position] == ']')
    throw std::invalid_argument("embedding runtime: response embedding is empty");
  while (true) {
    if (values.size() >= 4096 ||
        (dimensions != 0 && values.size() >= dimensions))
      throw std::invalid_argument(
          dimensions == 0
              ? "embedding runtime: response embedding is oversized"
              : "embedding runtime: response embedding has the wrong dimension");
    values.push_back(parse_number(body, position));
    skip_space(body, position);
    if (position >= body.size())
      throw std::invalid_argument("embedding runtime: unterminated embedding array");
    if (body[position] == ']') {
      ++position;
      break;
    }
    if (body[position] != ',')
      throw std::invalid_argument("embedding runtime: malformed embedding array");
    ++position;
  }
  if (dimensions != 0 && values.size() != dimensions)
    throw std::invalid_argument(
        "embedding runtime: response embedding has the wrong dimension");
  expect(body, position, '}', "response object");
  skip_space(body, position);
  if (position != body.size())
    throw std::invalid_argument("embedding runtime: trailing response data");
  return values;
}

inline void validate_url(std::string_view url) {
  if (url.size() < 8 || url.substr(0, 7) != "http://")
    throw std::invalid_argument(
        "embedding runtime URL must use http://host[:port]");
  const auto authority = url.substr(7);
  if (authority.empty() || authority.find_first_of("/?#@\\ \t\r\n") !=
                                std::string_view::npos)
    throw std::invalid_argument("embedding runtime URL has an invalid authority");
  std::string_view host = authority;
  std::string_view port;
  if (authority.front() == '[') {
    const auto close = authority.find(']');
    if (close == std::string_view::npos || close == 1) {
      throw std::invalid_argument("embedding runtime URL has an invalid host");
    }
    host = authority.substr(1, close - 1);
    if (close + 1 < authority.size()) {
      if (authority[close + 1] != ':')
        throw std::invalid_argument("embedding runtime URL has an invalid port");
      port = authority.substr(close + 2);
      if (port.empty())
        throw std::invalid_argument("embedding runtime URL has an invalid port");
    }
  } else {
    const auto colon = authority.find(':');
    if (colon != std::string_view::npos) {
      if (authority.find(':', colon + 1) != std::string_view::npos)
        throw std::invalid_argument("embedding runtime URL has an invalid host");
      host = authority.substr(0, colon);
      port = authority.substr(colon + 1);
      if (port.empty())
        throw std::invalid_argument("embedding runtime URL has an invalid port");
    }
  }
  if (host.empty() || host.find_first_of("[]") != std::string_view::npos)
    throw std::invalid_argument("embedding runtime URL has an invalid host");
  if (!port.empty()) {
    unsigned long value = 0;
    for (const char c : port) {
      if (c < '0' || c > '9')
        throw std::invalid_argument("embedding runtime URL has an invalid port");
      value = value * 10 + static_cast<unsigned long>(c - '0');
      if (value > 65535) break;
    }
    if (value == 0 || value > 65535)
      throw std::invalid_argument("embedding runtime URL port is out of range");
  }
}

}  // namespace detail

inline minera::EmbeddingProviderCallback make_http_provider(
    std::string url) {
  detail::validate_url(url);
  return [url = std::move(url)](std::string_view text, std::string_view model,
                                std::size_t dimensions) {
    httplib::Client client(url);
    if (!client.is_valid())
      throw std::runtime_error("embedding runtime: unable to create HTTP client");
    constexpr auto timeout = std::chrono::seconds(5);
    client.set_connection_timeout(timeout);
    client.set_read_timeout(timeout);
    client.set_write_timeout(timeout);
    const auto body = std::string("{\"text\":") + detail::escape_json(text) +
                      ",\"model\":" + detail::escape_json(model) +
                      ",\"dimensions\":" + std::to_string(dimensions) + "}";
    const auto result = client.Post("/embed", body, "application/json");
    if (!result)
      throw std::runtime_error("embedding runtime: HTTP request failed (" +
                               httplib::to_string(result.error()) + ")");
    if (result->status < 200 || result->status >= 300)
      throw std::runtime_error("embedding runtime: HTTP status " +
                               std::to_string(result->status));
    return detail::parse_response(result->body, dimensions);
  };
}

}  // namespace minera::embedding_runtime
