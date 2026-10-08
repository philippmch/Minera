#pragma once

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "embedding_runtime.hpp"

namespace minera::llm_runtime {

inline constexpr std::size_t max_input_bytes = 2u * 1024u * 1024u;
inline constexpr std::size_t max_response_bytes = 4u * 1024u * 1024u;

struct CompletionOptions {
  std::string provider = "openai";
  std::string model;
  std::size_t max_tokens = 4096;
  std::optional<double> temperature;
  std::string openai_api;
  std::string extra_payload;
  struct Image { std::string name, media_type, data, detail; };
  std::vector<Image> images;
};

inline std::string bounded_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::invalid_argument("Could not open LLM input file");
  std::string bytes(max_input_bytes + 1, '\0');
  input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  bytes.resize(static_cast<std::size_t>(input.gcount()));
  if (bytes.empty() || bytes.size() > max_input_bytes)
    throw std::invalid_argument("LLM input file must be nonempty and within the runtime input bound");
  return bytes;
}

inline CompletionOptions::Image image_file(const std::filesystem::path& path) {
  const auto bytes = bounded_file(path);
  std::string type;
  if (bytes.starts_with("\x89PNG\r\n\x1a\n")) type = "image/png";
  else if (bytes.starts_with("\xff\xd8\xff")) type = "image/jpeg";
  else if (bytes.starts_with("GIF87a") || bytes.starts_with("GIF89a")) type = "image/gif";
  else if (bytes.starts_with("RIFF") && bytes.size() >= 12 && bytes.substr(8, 4) == "WEBP") type = "image/webp";
  else throw std::invalid_argument("Expected a PNG, JPEG, WebP or GIF image");
  constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string encoded;
  encoded.reserve((bytes.size() + 2) / 3 * 4);
  for (std::size_t i = 0; i < bytes.size(); i += 3) {
    const auto a = static_cast<unsigned char>(bytes[i]);
    const auto b = i + 1 < bytes.size() ? static_cast<unsigned char>(bytes[i + 1]) : 0;
    const auto c = i + 2 < bytes.size() ? static_cast<unsigned char>(bytes[i + 2]) : 0;
    encoded += alphabet[a >> 2];
    encoded += alphabet[((a & 3) << 4) | (b >> 4)];
    encoded += i + 1 < bytes.size() ? alphabet[((b & 15) << 2) | (c >> 6)] : '=';
    encoded += i + 2 < bytes.size() ? alphabet[c & 63] : '=';
  }
  const auto name = path.filename().u8string();
  return {std::string(name.begin(), name.end()), type, encoded, ""};
}

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
  const auto maximum = options.provider == "anthropic" ? 1 : 2;
  if (options.temperature && (!std::isfinite(*options.temperature) || *options.temperature < 0 || *options.temperature > maximum))
    throw std::invalid_argument("LLM temperature exceeds the provider range");
  if (!options.openai_api.empty() && (options.provider != "openai" || (options.openai_api != "responses" && options.openai_api != "chat_completions")))
    throw std::invalid_argument("OpenAI API must be responses or chat_completions");
  if (options.images.size() > 8) throw std::invalid_argument("At most eight images can be attached");
  const auto quote = embedding_runtime::detail::escape_json;
  auto body = "{\"provider\":" + quote(options.provider) + ",\"model\":" + quote(options.model) +
              ",\"prompt\":" + quote(prompt) + ",\"max_tokens\":" + std::to_string(options.max_tokens);
  if (options.temperature) body += ",\"temperature\":" + std::to_string(*options.temperature);
  if (!options.openai_api.empty()) body += ",\"openai_api\":" + quote(options.openai_api);
  if (!options.extra_payload.empty()) body += ",\"extra_payload\":" + quote(options.extra_payload);
  if (!options.images.empty()) {
    body += ",\"images\":[";
    for (std::size_t i = 0; i < options.images.size(); ++i) {
      if (i) body += ",";
      const auto& image = options.images[i];
      body += "{\"name\":" + quote(image.name) + ",\"media_type\":" + quote(image.media_type) + ",\"data\":" + quote(image.data);
      if (!image.detail.empty()) body += ",\"detail\":" + quote(image.detail);
      body += "}";
    }
    body += "]";
  }
  body += "}";
  const auto response = request(url, "/complete", &body);
  if (response->status < 200 || response->status >= 300)
    throw std::runtime_error("LLM runtime returned HTTP " + std::to_string(response->status));
  return response->body;
}

} // namespace minera::llm_runtime
