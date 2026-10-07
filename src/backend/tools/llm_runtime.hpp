#pragma once

#include <chrono>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "embedding_runtime.hpp"

namespace minera::llm_runtime {

inline constexpr std::size_t max_input_bytes = 2u * 1024u * 1024u;
inline constexpr std::size_t max_response_bytes = 4u * 1024u * 1024u;

struct CompletionOptions {
  std::string provider = "openai";
  std::string model;
  std::size_t max_tokens = 4096;
  std::optional<double> temperature;
};

inline void validate_url(std::string_view url) {
  try { embedding_runtime::detail::validate_url(url); }
  catch (const std::exception&) {
    throw std::invalid_argument("LLM runtime URL must be http://host[:port] without a path or credentials");
  }
}

inline httplib::Result request(std::string_view url, std::string_view route,
                               const std::string* body = nullptr) {
  validate_url(url);
  httplib::Client client{std::string(url)};
  client.set_connection_timeout(std::chrono::seconds(5));
  client.set_write_timeout(std::chrono::seconds(5));
  client.set_read_timeout(std::chrono::seconds(90));
  if (body && (body->empty() || body->size() > max_input_bytes))
    throw std::invalid_argument("LLM request must be nonempty and within the runtime input bound");
  auto response = body ? client.Post(std::string(route), *body, "application/json")
                       : client.Get(std::string(route));
  if (!response) throw std::runtime_error("Could not reach the configured LLM runtime");
  if (response->body.size() > max_response_bytes)
    throw std::runtime_error("LLM runtime response exceeds the application bound");
  return response;
}

inline std::string completion_json(std::string_view url, std::string_view prompt,
                                    const CompletionOptions& options) {
  if (options.provider != "openai" && options.provider != "anthropic" && options.provider != "google")
    throw std::invalid_argument("unsupported LLM provider");
  if (options.model.empty()) throw std::invalid_argument("LLM model must not be empty");
  if (!options.max_tokens || options.max_tokens > 1000000)
    throw std::invalid_argument("LLM token limit must be from 1 through 1000000");
  if (options.temperature && (!std::isfinite(*options.temperature) || *options.temperature < 0 || *options.temperature > 1))
    throw std::invalid_argument("LLM temperature must be from 0 through 1");
  const auto quote = embedding_runtime::detail::escape_json;
  auto body = "{\"provider\":" + quote(options.provider) + ",\"model\":" + quote(options.model) +
              ",\"prompt\":" + quote(prompt) + ",\"max_tokens\":" + std::to_string(options.max_tokens);
  if (options.temperature) body += ",\"temperature\":" + std::to_string(*options.temperature);
  body += "}";
  const auto response = request(url, "/complete", &body);
  if (response->status < 200 || response->status >= 300)
    throw std::runtime_error("LLM runtime returned HTTP " + std::to_string(response->status));
  return response->body;
}

} // namespace minera::llm_runtime
