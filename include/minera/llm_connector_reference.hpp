#pragma once

// algo.querying.llm.connectors of the PM4Py 2.7.22 reference (anthropic,
// openai, google), as observed with requests.post stubbed: each connector
// builds one POST from the prompt and its parameters, sends it and reads the
// answer out of the response. The parameters are the reference's dict
// (a kObject of ConnectorReferenceJson; absent keys take the defaults below)
// and the answer is the JSON value found, usually a string.
//
// Common rules: image_path, when present and not null, is read and sent as
// base64 with the media type "image/" + its extension lowercased (the text
// after the last dot of the file name, so "a.jpg" is image/jpg and a name
// without one gives "image/"); a missing file raises FileNotFoundError before
// anything is sent. extra_payload, when not null, is merged into the payload
// last (its keys override). api_url gets a trailing '/' when it lacks one.
// A response holding "error" raises Exception(response["error"]["message"]);
// otherwise the answer is read by direct indexing, so a missing key raises
// KeyError, an index past the end IndexError and indexing a value that is
// no dict or list TypeError, as the reference does.
//
// anthropic: POST api_url ("https://api.anthropic.com/v1/") + "messages",
// headers content-type, anthropic-version 2023-06-01 and x-api-key (null
// when no api_key); payload model (anthropic_model, "claude-sonnet-4-
// 20250514"), max_tokens (8192), and when thinking_tokens is not null an
// "anthropic-beta: output-128k-2025-02-19" header, thinking {type enabled,
// budget_tokens} and max_tokens raised by the budget; then messages, whose
// content is the prompt text or, with an image, a text and an image block.
// The answer is content[-1]["text"].
//
// openai: api_url ("https://api.openai.com/v1/"); the Responses API is used
// when use_responses_api is truthy, by default when the url contains
// "api.openai.com". The model defaults to "gpt-5" with an image, "gpt-4.1"
// without; Authorization is "Bearer " + str(api_key). Responses: POST
// "responses" with input [{role user, content [input_text (, input_image
// data url)]}], answer output[-1]["content"][0]["text"]. Chat: POST
// "chat/completions" with max_tokens (with an image always, 16384 by default;
// without only when given and not null) and messages (the prompt text, or a
// text and an image_url part with an image), answer
// choices[0]["message"]["content"]. Both send timeout 1200.
//
// google: POST https://generativelanguage.googleapis.com/v1beta/models/ +
// google_model ("gemini-2.5-flash") + ":generateContent?key=" + api_key
// (TypeError unless a string, checked after the image is read), payload
// contents [{parts [text (, inline_data)]}], answer
// candidates[0]["content"]["parts"][0]["text"].

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "minera/connector_reference.hpp"

namespace minera {

struct LlmReferenceRequest {
  std::string url;
  std::vector<std::pair<std::string, ConnectorReferenceJson>> headers;
  ConnectorReferenceJson payload;
  std::optional<int> timeout;
};

// requests.post(...).json()
using LlmReferencePost = std::function<ConnectorReferenceJson(const LlmReferenceRequest&)>;

[[nodiscard]] ConnectorReferenceJson reference_anthropic_query(const std::string& prompt,
                                                               const ConnectorReferenceJson& parameters,
                                                               const LlmReferencePost& post);
[[nodiscard]] ConnectorReferenceJson reference_openai_query(const std::string& prompt,
                                                            const ConnectorReferenceJson& parameters,
                                                            const LlmReferencePost& post);
[[nodiscard]] ConnectorReferenceJson reference_google_query(const std::string& prompt,
                                                            const ConnectorReferenceJson& parameters,
                                                            const LlmReferencePost& post);

}  // namespace minera
