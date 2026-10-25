#pragma once

// algo.transformation.to_embeddings of the PM4Py 2.7.22 reference, observed
// with sentence_transformers stubbed: the model is a hook, encode(model,
// sentences), standing for SentenceTransformer(model).encode(sentences) and
// returning one float32 vector per sentence. The default model is
// "all-MiniLM-L6-v2".
//
// util.embed_sentence encodes [sentence] and returns the first vector.
// util.similarity returns the cosine of the target (a sentence is embedded
// first) with every embedding, as np.dot(a, b) / (norm(a) * norm(b)): in
// float32 when both vectors are float32 arrays, otherwise in float64 (a
// float32 norm then widened). Sums run left to right; the bank uses dyadic
// vectors whose sums are exact, so BLAS's summation order cannot show.
//
// variants.cases_transformers embeds one sentence per case: the attribute
// values of its events in row order joined by spaces (TypeError for a value
// that is no str), cases sorted by identifier. variants.events_transformers
// embeds every event's attribute value, identified by event_id_key
// ("@@index"). A missing column raises KeyError. keep_top_k_per_similarity
// embeds the target sentence with the default model whatever the
// parameters say, scores every case (event), sorts the (similarity,
// identifier) pairs in descending order (so ties go to the larger
// identifier), keeps the first k as Python slices them (k < 0 drops the
// last -k) and returns the rows whose identifier was kept, in log order;
// with keep_cases the events transformer keeps the whole cases of those
// events instead. Failures throw ConnectorReferenceError with the Python
// exception type.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace minera {

using EmbeddingReferenceEncode =
    std::function<std::vector<std::vector<float>>(const std::string& model, const std::vector<std::string>& sentences)>;

// An embedding as Python holds it: a float32 array or a list of floats.
struct EmbeddingReferenceVector {
  std::vector<double> values;
  bool float32 = false;
};

struct EmbeddingReferenceSimilarity {
  double value = 0.0;
  bool float32 = false;
  bool operator==(const EmbeddingReferenceSimilarity&) const = default;
};

// A log cell: an int (such as @@index) or a str.
using EmbeddingReferenceValue = std::variant<std::int64_t, std::string>;

struct EmbeddingReferenceLog {
  std::vector<std::string> columns;
  std::vector<std::vector<EmbeddingReferenceValue>> rows;
};

struct EmbeddingReferenceParameters {
  std::string model = "all-MiniLM-L6-v2";
  std::string case_id_key = "case:concept:name";
  std::string attribute_key = "concept:name";
  std::string event_id_key = "@@index";
  bool keep_cases = false;
};

struct EmbeddingReferenceEmbeddings {
  std::vector<EmbeddingReferenceValue> identifiers;
  std::vector<std::vector<float>> embeddings;
};

[[nodiscard]] std::vector<float> reference_embed_sentence(const std::string& sentence, const std::string& model,
                                                          const EmbeddingReferenceEncode& encode);

[[nodiscard]] std::vector<EmbeddingReferenceSimilarity> reference_embedding_similarity(
    const std::variant<std::string, EmbeddingReferenceVector>& target,
    const std::vector<EmbeddingReferenceVector>& embeddings, const std::string& model,
    const EmbeddingReferenceEncode& encode);

[[nodiscard]] EmbeddingReferenceEmbeddings reference_case_embeddings(const EmbeddingReferenceLog& log,
                                                                     const EmbeddingReferenceParameters& parameters,
                                                                     const EmbeddingReferenceEncode& encode);
[[nodiscard]] EmbeddingReferenceEmbeddings reference_event_embeddings(const EmbeddingReferenceLog& log,
                                                                      const EmbeddingReferenceParameters& parameters,
                                                                      const EmbeddingReferenceEncode& encode);

// The rows keep_top_k_per_similarity keeps. identifiers and embeddings,
// when given, replace the transformer's own (embeddings then are lists of
// floats or float32 arrays as given).
struct EmbeddingReferenceGiven {
  std::vector<EmbeddingReferenceValue> identifiers;
  std::vector<EmbeddingReferenceVector> embeddings;
};

[[nodiscard]] std::vector<std::size_t> reference_keep_top_k_cases(const EmbeddingReferenceLog& log,
                                                                  const std::string& target, std::int64_t k,
                                                                  const std::optional<EmbeddingReferenceGiven>& given,
                                                                  const EmbeddingReferenceParameters& parameters,
                                                                  const EmbeddingReferenceEncode& encode);
[[nodiscard]] std::vector<std::size_t> reference_keep_top_k_events(const EmbeddingReferenceLog& log,
                                                                   const std::string& target, std::int64_t k,
                                                                   const std::optional<EmbeddingReferenceGiven>& given,
                                                                   const EmbeddingReferenceParameters& parameters,
                                                                   const EmbeddingReferenceEncode& encode);

}  // namespace minera
