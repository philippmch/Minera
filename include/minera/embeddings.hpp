#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

// The embedding transformation has two granularities.  Hashing is a
// deterministic, self-contained provider.  Transformer mode is injectable so
// applications can connect their own model runtime without adding a runtime
// dependency to the native core.
enum class EmbeddingVariant {
  CASES_TRANSFORMERS,
  EVENTS_TRANSFORMERS,
  kCasesTransformers = CASES_TRANSFORMERS,
  kEventsTransformers = EVENTS_TRANSFORMERS,
};

enum class EmbeddingProvider {
  HASHING,
  TRANSFORMER,
  kHashing = HASHING,
  kTransformer = TRANSFORMER,
};

// Host runtimes provide model execution through this callback. The native
// library passes the already grouped case text or selected event value, the
// requested model label, and the exact output dimension. A callback must
// return exactly `dimensions` finite values; no provider fallback is applied
// when a transformer callback is absent.
using EmbeddingProviderCallback = std::function<std::vector<double>(
    std::string_view text, std::string_view model, std::size_t dimensions)>;

// Register the host application's transformer runtime for calls whose
// EmbeddingOptions do not carry an explicit callback. Registration and
// clearing are thread-safe; the callback itself is invoked outside the
// registry lock. A registered runtime is never used for hashing providers or
// the explicit `mock_transformer` model.
void set_default_embedding_provider(EmbeddingProviderCallback callback);
void clear_default_embedding_provider();

struct EmbeddingOptions {
  EmbeddingVariant variant = EmbeddingVariant::CASES_TRANSFORMERS;
  EmbeddingProvider provider = EmbeddingProvider::HASHING;
  // Built-in profiles are `hashing-v1` and `hashing-v2`. Other non-empty names
  // retain the deterministic salted hashing compatibility profile.
  std::string embedding_model = "hashing-v1";
  // Required for transformer mode unless a default provider is registered,
  // except for the explicit `mock_transformer` model, which is provided for
  // headless integration tests and demos. A non-mock model without either
  // callback is rejected rather than substituted.
  EmbeddingProviderCallback transformer_callback;
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
  EmbeddingProvider provider = EmbeddingProvider::HASHING;
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
