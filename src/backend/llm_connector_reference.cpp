#include "minera/llm_connector_reference.hpp"

#include <fstream>
#include <iterator>

#include "minera/filtering_common.hpp"

namespace minera {

namespace {

using Json = ConnectorReferenceJson;

Json text(std::string value) {
  Json out;
  out.kind = Json::kText;
  out.text = std::move(value);
  return out;
}

Json integer(std::int64_t value) {
  Json out;
  out.kind = Json::kInt;
  out.integer = value;
  return out;
}

Json object(std::vector<std::pair<std::string, Json>> members) {
  Json out;
  out.kind = Json::kObject;
  out.members = std::move(members);
  return out;
}

Json array(std::vector<Json> items) {
  Json out;
  out.kind = Json::kArray;
  out.items = std::move(items);
  return out;
}

void set(Json& target, const std::string& key, Json value) {
  for (auto& [name, existing] : target.members)
    if (name == key) {
      existing = std::move(value);
      return;
    }
  target.members.emplace_back(key, std::move(value));
}

// parameters.get(key, None)
const Json* parameter(const Json& parameters, const std::string& key) {
  const auto* found = parameters.find(key);
  return found && found->kind != Json::kNull ? found : nullptr;
}

std::string string_parameter(const Json& parameters, const std::string& key, const std::string& fallback) {
  const auto* found = parameters.find(key);
  if (!found) return fallback;
  if (found->kind != Json::kText) throw ConnectorReferenceError("TypeError", key + " is no str");
  return found->text;
}

// str() of a value, as an f-string writes it.
std::string python_str(const Json& value) {
  switch (value.kind) {
    case Json::kNull: return "None";
    case Json::kBool: return value.boolean ? "True" : "False";
    case Json::kInt: return std::to_string(value.integer);
    case Json::kFloat: return filtering_float_repr(value.real);
    case Json::kText: return value.text;
    default: throw ConnectorReferenceError("TypeError", "no str() for a container here");
  }
}

bool truthy(const Json& value) {
  switch (value.kind) {
    case Json::kNull: return false;
    case Json::kBool: return value.boolean;
    case Json::kInt: return value.integer != 0;
    case Json::kFloat: return value.real != 0.0;
    case Json::kText: return !value.text.empty();
    case Json::kArray: return !value.items.empty();
    case Json::kObject: return !value.members.empty();
  }
  return false;
}

std::string base64(const std::string& bytes) {
  static const char* const kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  std::size_t k = 0;
  for (; k + 2 < bytes.size(); k += 3) {
    const unsigned v = (static_cast<unsigned char>(bytes[k]) << 16) | (static_cast<unsigned char>(bytes[k + 1]) << 8) |
                       static_cast<unsigned char>(bytes[k + 2]);
    for (int shift = 18; shift >= 0; shift -= 6) out += kAlphabet[(v >> shift) & 63];
  }
  if (k + 1 == bytes.size()) {
    const unsigned v = static_cast<unsigned char>(bytes[k]) << 16;
    out += kAlphabet[(v >> 18) & 63];
    out += kAlphabet[(v >> 12) & 63];
    out += "==";
  } else if (k + 2 == bytes.size()) {
    const unsigned v = (static_cast<unsigned char>(bytes[k]) << 16) | (static_cast<unsigned char>(bytes[k + 1]) << 8);
    out += kAlphabet[(v >> 18) & 63];
    out += kAlphabet[(v >> 12) & 63];
    out += kAlphabet[(v >> 6) & 63];
    out += '=';
  }
  return out;
}

struct Image {
  std::string data, media_type;
};

// encode_image plus the media type: "image/" + os.path.splitext(path)[1][1:].lower().
std::optional<Image> image_of(const Json& parameters) {
  const auto* path = parameter(parameters, "image_path");
  if (!path) return std::nullopt;
  if (path->kind != Json::kText) throw ConnectorReferenceError("TypeError", "image_path is no str");
  std::ifstream file(path->text, std::ios::binary);
  if (!file) throw ConnectorReferenceError("FileNotFoundError", "No such file or directory: '" + path->text + "'");
  Image out;
  out.data = base64(std::string(std::istreambuf_iterator<char>(file), {}));
  const auto& p = path->text;
  const auto slash = p.find_last_of('/');
  const std::string name = slash == std::string::npos ? p : p.substr(slash + 1);
  // splitext ignores leading dots of the file name.
  std::size_t lead = 0;
  while (lead < name.size() && name[lead] == '.') ++lead;
  const auto dot = name.find_last_of('.');
  std::string extension = dot != std::string::npos && dot >= lead ? name.substr(dot + 1) : "";
  for (auto& c : extension)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  out.media_type = "image/" + extension;
  return out;
}

void merge_extra(Json& payload, const Json& parameters) {
  const auto* extra = parameter(parameters, "extra_payload");
  if (!extra) return;
  if (extra->kind != Json::kObject) throw ConnectorReferenceError("TypeError", "extra_payload is no dict");
  for (const auto& [key, value] : extra->members) set(payload, key, value);
}

std::string base_url(const Json& parameters, const std::string& fallback) {
  auto url = string_parameter(parameters, "api_url", fallback);
  if (url.empty() || url.back() != '/') url += '/';
  return url;
}

// value[key] / value[index] as Python indexes JSON.
const Json& at(const Json& value, const std::string& key) {
  if (value.kind != Json::kObject) throw ConnectorReferenceError("TypeError", "not subscriptable by str");
  const auto* found = value.find(key);
  if (!found) throw ConnectorReferenceError("KeyError", "'" + key + "'");
  return *found;
}

const Json& at(const Json& value, std::int64_t index) {
  // JSON keys are strings, so an int key is never found.
  if (value.kind == Json::kObject) throw ConnectorReferenceError("KeyError", std::to_string(index));
  if (value.kind != Json::kArray) throw ConnectorReferenceError("TypeError", "not subscriptable by int");
  const auto size = static_cast<std::int64_t>(value.items.size());
  const auto k = index < 0 ? index + size : index;
  if (k < 0 || k >= size) throw ConnectorReferenceError("IndexError", "list index out of range");
  return value.items[static_cast<std::size_t>(k)];
}

// if "error" in response: raise Exception(response["error"]["message"])
void check_error(const Json& response) {
  bool has_error = false;
  switch (response.kind) {
    case Json::kObject: has_error = response.find("error") != nullptr; break;
    case Json::kArray:
      for (const auto& item : response.items) has_error = has_error || (item.kind == Json::kText && item.text == "error");
      break;
    case Json::kText: has_error = response.text.find("error") != std::string::npos; break;
    case Json::kNull: throw ConnectorReferenceError("TypeError", "argument of type 'NoneType' is not iterable");
    default: throw ConnectorReferenceError("TypeError", "argument is not iterable");
  }
  if (!has_error) return;
  const Json message = at(at(response, "error"), "message");
  throw ConnectorReferenceError("Exception", python_str(message));
}

}  // namespace

ConnectorReferenceJson reference_anthropic_query(const std::string& prompt, const ConnectorReferenceJson& parameters,
                                                 const LlmReferencePost& post) {
  const auto url = base_url(parameters, "https://api.anthropic.com/v1/");
  const auto* key = parameters.find("api_key");
  const auto model = string_parameter(parameters, "anthropic_model", "claude-sonnet-4-20250514");
  const auto* max_tokens_found = parameters.find("max_tokens");
  Json max_tokens = max_tokens_found ? *max_tokens_found : integer(8192);
  const auto* thinking = parameter(parameters, "thinking_tokens");
  LlmReferenceRequest request;
  request.headers = {{"content-type", text("application/json")},
                     {"anthropic-version", text("2023-06-01")},
                     {"x-api-key", key ? *key : Json{}}};
  Json payload = object({{"model", text(model)}, {"max_tokens", max_tokens}});
  if (thinking) {
    request.headers.emplace_back("anthropic-beta", text("output-128k-2025-02-19"));
    set(payload, "thinking", object({{"type", text("enabled")}, {"budget_tokens", *thinking}}));
    if (max_tokens.kind != Json::kInt || thinking->kind != Json::kInt)
      throw ConnectorReferenceError("TypeError", "unsupported operand type(s) for +");
    set(payload, "max_tokens", integer(max_tokens.integer + thinking->integer));
  }
  const auto picture = image_of(parameters);
  Json content = text(prompt);
  if (picture)
    content = array({object({{"type", text("text")}, {"text", text(prompt)}}),
                     object({{"type", text("image")},
                             {"source", object({{"type", text("base64")},
                                                {"media_type", text(picture->media_type)},
                                                {"data", text(picture->data)}})}})});
  set(payload, "messages", array({object({{"role", text("user")}, {"content", content}})}));
  merge_extra(payload, parameters);
  request.url = url + "messages";
  request.payload = std::move(payload);
  const auto response = post(request);
  check_error(response);
  return at(at(at(response, "content"), -1), "text");
}

ConnectorReferenceJson reference_openai_query(const std::string& prompt, const ConnectorReferenceJson& parameters,
                                              const LlmReferencePost& post) {
  const auto url = base_url(parameters, "https://api.openai.com/v1/");
  const auto* key = parameters.find("api_key");
  const bool has_image = parameter(parameters, "image_path") != nullptr;
  const auto model = string_parameter(parameters, "openai_model", has_image ? "gpt-5" : "gpt-4.1");
  const auto* responses_flag = parameters.find("use_responses_api");
  const bool responses = responses_flag ? truthy(*responses_flag) : url.find("api.openai.com") != std::string::npos;
  LlmReferenceRequest request;
  request.headers = {{"Content-Type", text("application/json")},
                     {"Authorization", text("Bearer " + (key ? python_str(*key) : std::string("None")))}};
  request.timeout = 1200;
  const auto picture = image_of(parameters);
  Json payload = object({{"model", text(model)}});
  if (responses) {
    std::vector<Json> content = {object({{"type", text("input_text")}, {"text", text(prompt)}})};
    if (picture)
      content.push_back(object({{"type", text("input_image")},
                                {"image_url", text("data:" + picture->media_type + ";base64," + picture->data)}}));
    set(payload, "input", array({object({{"role", text("user")}, {"content", array(std::move(content))}})}));
    request.url = url + "responses";
  } else {
    const auto* max_tokens = parameters.find("max_tokens");
    if (picture) set(payload, "max_tokens", max_tokens ? *max_tokens : integer(16384));
    else if (max_tokens && max_tokens->kind != Json::kNull) set(payload, "max_tokens", *max_tokens);
    Json content = text(prompt);
    if (picture)
      content = array({object({{"type", text("text")}, {"text", text(prompt)}}),
                       object({{"type", text("image_url")},
                               {"image_url", object({{"url", text("data:" + picture->media_type + ";base64," +
                                                                  picture->data)}})}})});
    set(payload, "messages", array({object({{"role", text("user")}, {"content", content}})}));
    request.url = url + "chat/completions";
  }
  merge_extra(payload, parameters);
  request.payload = std::move(payload);
  const auto response = post(request);
  check_error(response);
  if (responses) return at(at(at(at(at(response, "output"), -1), "content"), 0), "text");
  return at(at(at(at(response, "choices"), 0), "message"), "content");
}

ConnectorReferenceJson reference_google_query(const std::string& prompt, const ConnectorReferenceJson& parameters,
                                              const LlmReferencePost& post) {
  const auto* key = parameters.find("api_key");
  const auto model = string_parameter(parameters, "google_model", "gemini-2.5-flash");
  const auto picture = image_of(parameters);
  if (!key || key->kind != Json::kText)
    throw ConnectorReferenceError("TypeError", "can only concatenate str (not \"NoneType\") to str");
  LlmReferenceRequest request;
  request.url = "https://generativelanguage.googleapis.com/v1beta/models/" + model + ":generateContent?key=" + key->text;
  request.headers = {{"Content-Type", text("application/json")}};
  std::vector<Json> parts = {object({{"text", text(prompt)}})};
  if (picture)
    parts.push_back(object(
        {{"inline_data", object({{"mime_type", text(picture->media_type)}, {"data", text(picture->data)}})}}));
  Json payload = object({{"contents", array({object({{"parts", array(std::move(parts))}})})}});
  merge_extra(payload, parameters);
  request.payload = std::move(payload);
  const auto response = post(request);
  check_error(response);
  return at(at(at(at(at(at(response, "candidates"), 0), "content"), "parts"), 0), "text");
}

}  // namespace minera
