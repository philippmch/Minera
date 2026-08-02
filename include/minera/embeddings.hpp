#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// The embedding transformation has two granularities.  The native encoder is
// deliberately deterministic and self-contained: it hashes normalized text
// tokens into a bounded vector, so identical inputs produce identical output
// across platforms and runs.
enum class EmbeddingVariant {
  CASES_TRANSFORMERS,
  EVENTS_TRANSFORMERS,
  kCasesTransformers = CASES_TRANSFORMERS,
  kEventsTransformers = EVENTS_TRANSFORMERS,
};

struct EmbeddingOptions {
  EmbeddingVariant variant = EmbeddingVariant::CASES_TRANSFORMERS;
  std::string embedding_model = "hashing-v1";
  std::string attribute_key = "concept:name";
  // Event identifiers are global, deterministic ordinal strings by default.
  // A non-empty custom key reads an event attribute (or activity/timestamp/
  // resource) and falls back to the ordinal when it is absent.
  std::string event_id_key;
  std::size_t dimensions = 64;
  std::size_t max_items = 10000;
  bool keep_cases = false;
};

struct EmbeddingItem {
  std::string id;
  std::vector<double> values;
};

struct EmbeddingResult {
  EmbeddingVariant variant = EmbeddingVariant::CASES_TRANSFORMERS;
  std::string embedding_model;
  std::string attribute_key;
  std::size_t dimensions = 0;
  std::vector<EmbeddingItem> items;
};

struct EmbeddingSimilarity {
  std::string id;
  double score = 0.0;
};

[[nodiscard]] std::vector<double> embed_text(
    std::string_view text, const EmbeddingOptions& options = {});

[[nodiscard]] EmbeddingResult to_embeddings(
    const EventLog& log, const EmbeddingOptions& options = {});

[[nodiscard]] std::vector<EmbeddingSimilarity> embedding_similarity(
    std::string_view target_text, const EmbeddingResult& result,
    const EmbeddingOptions& options = {}, std::size_t top_k = 0);

// Keeps the top-k matching cases or events.  For event embeddings,
// keep_cases=true retains every event in a selected case; otherwise only the
// selected events remain.  A zero top_k returns an empty log.
[[nodiscard]] EventLog filter_embeddings_top_k(
    const EventLog& log, std::string_view target_text, std::size_t top_k,
    const EmbeddingOptions& options = {});

[[nodiscard]] std::string serialize_embeddings_json(
    const EmbeddingResult& result);
[[nodiscard]] std::string serialize_embedding_similarity_json(
    std::string_view target_text, const EmbeddingResult& result,
    const std::vector<EmbeddingSimilarity>& matches, std::size_t top_k);

}  // namespace minera
