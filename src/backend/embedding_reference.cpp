#include "minera/embedding_reference.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

#include "minera/connector_reference.hpp"

namespace minera {

namespace {

std::size_t column(const EmbeddingReferenceLog& log, const std::string& name) {
  const auto found = std::find(log.columns.begin(), log.columns.end(), name);
  if (found == log.columns.end()) throw ConnectorReferenceError("KeyError", "['" + name + "'] not in index");
  return static_cast<std::size_t>(found - log.columns.begin());
}

const std::string& text_of(const EmbeddingReferenceValue& value) {
  if (!std::holds_alternative<std::string>(value))
    throw ConnectorReferenceError("TypeError", "sequence item: expected str instance, int found");
  return std::get<std::string>(value);
}

// Python's < between two identifiers of one column.
bool less(const EmbeddingReferenceValue& a, const EmbeddingReferenceValue& b) {
  if (a.index() != b.index())
    throw ConnectorReferenceError("TypeError", "'<' not supported between instances of 'str' and 'int'");
  return a < b;
}

std::vector<std::vector<float>> encode_all(const EmbeddingReferenceEncode& encode, const std::string& model,
                                           const std::vector<std::string>& sentences) {
  auto out = encode(model, sentences);
  if (out.size() != sentences.size()) throw ConnectorReferenceError("ValueError", "encoder returned a wrong count");
  return out;
}

EmbeddingReferenceVector float32_vector(const std::vector<float>& values) {
  return {std::vector<double>(values.begin(), values.end()), true};
}

// np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b))
EmbeddingReferenceSimilarity cosine(const EmbeddingReferenceVector& a, const EmbeddingReferenceVector& b) {
  if (a.values.size() != b.values.size()) throw ConnectorReferenceError("ValueError", "shapes not aligned");
  auto norm32 = [](const EmbeddingReferenceVector& v) {
    float sum = 0.0F;
    for (const double x : v.values) sum += static_cast<float>(x) * static_cast<float>(x);
    return std::sqrt(sum);
  };
  if (a.float32 && b.float32) {
    float dot = 0.0F;
    for (std::size_t k = 0; k < a.values.size(); ++k) dot += static_cast<float>(a.values[k]) * static_cast<float>(b.values[k]);
    const float denominator = norm32(a) * norm32(b);
    return {static_cast<double>(dot / denominator), true};
  }
  auto norm = [&](const EmbeddingReferenceVector& v) {
    if (v.float32) return static_cast<double>(norm32(v));
    double sum = 0.0;
    for (const double x : v.values) sum += x * x;
    return std::sqrt(sum);
  };
  double dot = 0.0;
  for (std::size_t k = 0; k < a.values.size(); ++k) dot += a.values[k] * b.values[k];
  return {dot / (norm(a) * norm(b)), false};
}

// The rows of each identifier's group, identifiers sorted (pandas groupby).
std::vector<std::pair<EmbeddingReferenceValue, std::vector<std::size_t>>> groups(const EmbeddingReferenceLog& log,
                                                                                std::size_t key) {
  std::vector<std::pair<EmbeddingReferenceValue, std::vector<std::size_t>>> out;
  for (std::size_t r = 0; r < log.rows.size(); ++r) {
    const auto& id = log.rows[r][key];
    auto found = std::find_if(out.begin(), out.end(), [&](const auto& g) { return g.first == id; });
    if (found == out.end()) out.push_back({id, {r}});
    else found->second.push_back(r);
  }
  std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return less(a.first, b.first); });
  return out;
}

// sorted(zip(similarities, identifiers), reverse=True)[:k], identifiers only.
std::vector<EmbeddingReferenceValue> top_k(const std::vector<EmbeddingReferenceSimilarity>& similarities,
                                           const std::vector<EmbeddingReferenceValue>& identifiers, std::int64_t k) {
  std::vector<std::size_t> order(identifiers.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
    if (similarities[a].value != similarities[b].value) return similarities[a].value > similarities[b].value;
    return less(identifiers[b], identifiers[a]);
  });
  const auto n = static_cast<std::int64_t>(order.size());
  const auto end = std::clamp(k < 0 ? n + k : k, std::int64_t{0}, n);
  std::vector<EmbeddingReferenceValue> out;
  for (std::int64_t r = 0; r < end; ++r) out.push_back(identifiers[order[static_cast<std::size_t>(r)]]);
  return out;
}

std::vector<EmbeddingReferenceSimilarity> score(const std::string& target, const std::vector<EmbeddingReferenceVector>& embeddings,
                                                const EmbeddingReferenceEncode& encode) {
  const auto query = float32_vector(encode_all(encode, "all-MiniLM-L6-v2", {target})[0]);
  std::vector<EmbeddingReferenceSimilarity> out;
  for (const auto& e : embeddings) out.push_back(cosine(query, e));
  return out;
}

bool contains(const std::vector<EmbeddingReferenceValue>& values, const EmbeddingReferenceValue& value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

}  // namespace

std::vector<float> reference_embed_sentence(const std::string& sentence, const std::string& model,
                                            const EmbeddingReferenceEncode& encode) {
  return encode_all(encode, model, {sentence})[0];
}

std::vector<EmbeddingReferenceSimilarity> reference_embedding_similarity(
    const std::variant<std::string, EmbeddingReferenceVector>& target,
    const std::vector<EmbeddingReferenceVector>& embeddings, const std::string& model,
    const EmbeddingReferenceEncode& encode) {
  const auto query = std::holds_alternative<std::string>(target)
                         ? float32_vector(reference_embed_sentence(std::get<std::string>(target), model, encode))
                         : std::get<EmbeddingReferenceVector>(target);
  std::vector<EmbeddingReferenceSimilarity> out;
  for (const auto& e : embeddings) out.push_back(cosine(query, e));
  return out;
}

EmbeddingReferenceEmbeddings reference_case_embeddings(const EmbeddingReferenceLog& log,
                                                       const EmbeddingReferenceParameters& parameters,
                                                       const EmbeddingReferenceEncode& encode) {
  const auto case_column = column(log, parameters.case_id_key);
  const auto attribute_column = column(log, parameters.attribute_key);
  EmbeddingReferenceEmbeddings out;
  std::vector<std::string> sentences;
  for (const auto& [id, rows] : groups(log, case_column)) {
    std::string sentence;
    for (std::size_t k = 0; k < rows.size(); ++k) sentence += (k ? " " : "") + text_of(log.rows[rows[k]][attribute_column]);
    out.identifiers.push_back(id);
    sentences.push_back(std::move(sentence));
  }
  out.embeddings = encode_all(encode, parameters.model, sentences);
  return out;
}

EmbeddingReferenceEmbeddings reference_event_embeddings(const EmbeddingReferenceLog& log,
                                                        const EmbeddingReferenceParameters& parameters,
                                                        const EmbeddingReferenceEncode& encode) {
  const auto id_column = column(log, parameters.event_id_key);
  const auto attribute_column = column(log, parameters.attribute_key);
  EmbeddingReferenceEmbeddings out;
  std::vector<std::string> sentences;
  for (const auto& row : log.rows) {
    out.identifiers.push_back(row[id_column]);
    sentences.push_back(text_of(row[attribute_column]));
  }
  out.embeddings = encode_all(encode, parameters.model, sentences);
  return out;
}

namespace {

EmbeddingReferenceGiven own(const EmbeddingReferenceEmbeddings& computed) {
  EmbeddingReferenceGiven out{computed.identifiers, {}};
  for (const auto& e : computed.embeddings) out.embeddings.push_back(float32_vector(e));
  return out;
}

}  // namespace

std::vector<std::size_t> reference_keep_top_k_cases(const EmbeddingReferenceLog& log, const std::string& target,
                                                    std::int64_t k, const std::optional<EmbeddingReferenceGiven>& given,
                                                    const EmbeddingReferenceParameters& parameters,
                                                    const EmbeddingReferenceEncode& encode) {
  const auto scored = given ? *given : own(reference_case_embeddings(log, parameters, encode));
  const auto kept = top_k(score(target, scored.embeddings, encode), scored.identifiers, k);
  const auto case_column = column(log, parameters.case_id_key);
  std::vector<std::size_t> rows;
  for (std::size_t r = 0; r < log.rows.size(); ++r)
    if (contains(kept, log.rows[r][case_column])) rows.push_back(r);
  return rows;
}

std::vector<std::size_t> reference_keep_top_k_events(const EmbeddingReferenceLog& log, const std::string& target,
                                                     std::int64_t k, const std::optional<EmbeddingReferenceGiven>& given,
                                                     const EmbeddingReferenceParameters& parameters,
                                                     const EmbeddingReferenceEncode& encode) {
  const auto scored = given ? *given : own(reference_event_embeddings(log, parameters, encode));
  const auto kept = top_k(score(target, scored.embeddings, encode), scored.identifiers, k);
  const auto id_column = column(log, parameters.event_id_key);
  std::vector<std::size_t> rows;
  if (parameters.keep_cases) {
    const auto case_column = column(log, parameters.case_id_key);
    std::vector<EmbeddingReferenceValue> cases;
    for (const auto& row : log.rows)
      if (contains(kept, row[id_column])) cases.push_back(row[case_column]);
    for (std::size_t r = 0; r < log.rows.size(); ++r)
      if (contains(cases, log.rows[r][case_column])) rows.push_back(r);
    return rows;
  }
  for (std::size_t r = 0; r < log.rows.size(); ++r)
    if (contains(kept, log.rows[r][id_column])) rows.push_back(r);
  return rows;
}

}  // namespace minera
