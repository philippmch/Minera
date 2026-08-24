#include "minera/embeddings.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace minera {
namespace {

std::mutex& default_transformer_mutex() {
  static std::mutex mutex;
  return mutex;
}

EmbeddingProviderCallback& default_transformer_storage() {
  static EmbeddingProviderCallback callback;
  return callback;
}

EmbeddingProviderCallback default_transformer_callback() {
  std::lock_guard lock(default_transformer_mutex());
  return default_transformer_storage();
}

std::string selected_value(const Event& event, std::string_view key) {
  if (key.empty() || key == "activity" || key == "concept:name")
    return event.activity;
  if (key == "timestamp" || key == "time:timestamp") return event.timestamp;
  if (key == "resource" || key == "org:resource") return event.resource;
  if (const auto* value = event.attribute(key)) return *value;
  return {};
}

std::string event_identifier(const Event& event, std::size_t ordinal,
                             std::string_view key) {
  if (!key.empty()) {
    const auto value = selected_value(event, key);
    if (!value.empty()) return value;
  }
  return std::to_string(ordinal);
}

std::uint64_t fnv1a(std::string_view text) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const unsigned char c : text) {
    hash ^= c;
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::uint64_t mix_hash(std::uint64_t seed, std::string_view text) {
  // FNV is deliberately kept as the stable primitive, while the model seed
  // makes separately selected encoder profiles independent of one another.
  std::uint64_t hash = seed ^ 1469598103934665603ULL;
  for (const unsigned char c : text) {
    hash ^= c;
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::vector<std::string> tokens(std::string_view text) {
  std::vector<std::string> result;
  std::string current;
  const auto flush = [&]() {
    if (!current.empty()) {
      result.push_back(std::move(current));
      current.clear();
    }
  };
  for (const unsigned char c : text) {
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
      current.push_back(static_cast<char>(c));
    } else if (c >= 'A' && c <= 'Z') {
      current.push_back(static_cast<char>(c - 'A' + 'a'));
    } else {
      flush();
    }
  }
  flush();
  return result;
}

void validate_options(const EmbeddingOptions& options) {
  if (options.variant != EmbeddingVariant::CASES_TRANSFORMERS &&
      options.variant != EmbeddingVariant::EVENTS_TRANSFORMERS)
    throw std::invalid_argument("embeddings: unknown variant");
  if (options.provider != EmbeddingProvider::HASHING &&
      options.provider != EmbeddingProvider::TRANSFORMER)
    throw std::invalid_argument("embeddings: unknown provider");
  if (options.embedding_model.empty())
    throw std::invalid_argument("embeddings: embedding_model must not be empty");
  if (options.attribute_key.empty())
    throw std::invalid_argument("embeddings: attribute_key must not be empty");
  if (options.dimensions == 0 || options.dimensions > 4096)
    throw std::invalid_argument("embeddings: dimensions must be in [1,4096]");
  if (options.max_items == 0 || options.max_items > 1'000'000)
    throw std::invalid_argument("embeddings: max_items must be in [1,1000000]");
}

struct EncoderProfile {
  std::uint64_t seed = 0;
  bool character_ngrams = false;
};

EncoderProfile encoder_profile(std::string_view model) {
  // `hashing-v1` is the original compact encoder and remains byte-for-byte
  // stable.  Other model names select an independent salted profile.  The
  // second built-in profile adds character n-grams, which gives callers a
  // deterministic local alternative without requiring a runtime model file.
  if (model == "hashing-v1") return {};
  if (model == "hashing-v2")
    return {fnv1a(model), true};
  return {fnv1a(model), false};
}

std::vector<double> mock_transformer(std::string_view text,
                                     std::string_view model,
                                     std::size_t dimensions) {
  std::vector<double> values(dimensions, 0.0);
  const auto seed = fnv1a("mock-transformer:") ^ fnv1a(model);
  for (const auto& token : tokens(text)) {
    const auto hash = mix_hash(seed, token);
    const auto bucket = static_cast<std::size_t>(hash % dimensions);
    values[bucket] += (hash & 1ULL) == 0ULL ? 1.0 : -1.0;
  }
  double norm = 0.0;
  for (const auto value : values) norm += value * value;
  if (norm > 0.0) {
    const auto scale = 1.0 / std::sqrt(norm);
    for (auto& value : values) value *= scale;
  }
  return values;
}

double cosine(const std::vector<double>& left,
              const std::vector<double>& right) {
  if (left.size() != right.size() || left.empty()) return 0.0;
  double dot = 0.0;
  double left_norm = 0.0;
  double right_norm = 0.0;
  for (std::size_t i = 0; i < left.size(); ++i) {
    dot += left[i] * right[i];
    left_norm += left[i] * left[i];
    right_norm += right[i] * right[i];
  }
  if (left_norm <= 0.0 || right_norm <= 0.0) return 0.0;
  const double value = dot / std::sqrt(left_norm * right_norm);
  return std::isfinite(value) ? value : 0.0;
}

void write_json_string(std::ostringstream& out, std::string_view value) {
  out << '"';
  for (const char c : value) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default: out << c; break;
    }
  }
  out << '"';
}

std::string variant_name(EmbeddingVariant variant) {
  return variant == EmbeddingVariant::EVENTS_TRANSFORMERS
             ? "events_transformers"
             : "cases_transformers";
}

std::string provider_name(EmbeddingProvider provider) {
  return provider == EmbeddingProvider::TRANSFORMER ? "transformer"
                                                    : "hashing";
}

void validate_embedding_vector(const std::vector<double>& values,
                               std::size_t dimensions,
                               std::string_view source) {
  if (values.size() != dimensions)
    throw std::invalid_argument(std::string("embeddings: ") +
                                std::string(source) +
                                " returned the wrong dimension");
  if (!std::all_of(values.begin(), values.end(), [](const double value) {
        return std::isfinite(value);
      }))
    throw std::invalid_argument(std::string("embeddings: ") +
                                std::string(source) +
                                " returned a non-finite value");
}

void validate_result(const EmbeddingResult& result,
                     const EmbeddingOptions& options) {
  if (result.variant != options.variant)
    throw std::invalid_argument("embeddings: result variant does not match options");
  if (result.dimensions == 0 || result.dimensions > 4096 ||
      result.dimensions != options.dimensions)
    throw std::invalid_argument(
        "embeddings: result dimensions do not match options");
  if (!result.embedding_model.empty() &&
      result.embedding_model != options.embedding_model)
    throw std::invalid_argument(
        "embeddings: result embedding_model does not match options");
  if (result.provider != options.provider)
    throw std::invalid_argument("embeddings: result provider does not match options");
  if (!result.attribute_key.empty() && result.attribute_key != options.attribute_key)
    throw std::invalid_argument(
        "embeddings: result attribute_key does not match options");
  for (const auto& item : result.items)
    validate_embedding_vector(item.values, result.dimensions, "result vector");
}

}  // namespace

void set_default_embedding_provider(EmbeddingProviderCallback callback) {
  if (!callback)
    throw std::invalid_argument(
        "embeddings: default provider callback must not be empty");
  std::lock_guard lock(default_transformer_mutex());
  default_transformer_storage() = std::move(callback);
}

void clear_default_embedding_provider() {
  std::lock_guard lock(default_transformer_mutex());
  default_transformer_storage() = {};
}

std::vector<double> embed_text(std::string_view text,
                               const EmbeddingOptions& options) {
  validate_options(options);
  if (options.provider == EmbeddingProvider::TRANSFORMER) {
    std::vector<double> values;
    auto callback = options.transformer_callback;
    if (!callback && options.embedding_model != "mock_transformer")
      callback = default_transformer_callback();
    if (callback) {
      values = callback(text, options.embedding_model, options.dimensions);
    } else if (options.embedding_model == "mock_transformer") {
      values = mock_transformer(text, options.embedding_model,
                                options.dimensions);
    } else {
      throw std::invalid_argument(
          "embeddings: transformer provider requires transformer_callback");
    }
    validate_embedding_vector(values, options.dimensions,
                             "transformer callback");
    return values;
  }
  const auto profile = encoder_profile(options.embedding_model);
  std::vector<double> values(options.dimensions, 0.0);
  for (const auto& token : tokens(text)) {
    const auto hash = profile.seed == 0 ? fnv1a(token)
                                       : mix_hash(profile.seed, token);
    const auto bucket = static_cast<std::size_t>(hash % options.dimensions);
    values[bucket] += (hash & 1ULL) == 0ULL ? 1.0 : -1.0;

    if (!profile.character_ngrams) continue;
    std::string padded;
    padded.reserve(token.size() + 2);
    padded.push_back('^');
    padded += token;
    padded.push_back('$');
    if (padded.size() < 3) continue;
    for (std::size_t offset = 0; offset + 3 <= padded.size(); ++offset) {
      const auto ngram = std::string_view(padded).substr(offset, 3);
      const auto ngram_hash = mix_hash(profile.seed ^ 0x9e3779b97f4a7c15ULL,
                                       ngram);
      const auto ngram_bucket = static_cast<std::size_t>(
          ngram_hash % options.dimensions);
      values[ngram_bucket] +=
          (ngram_hash & 1ULL) == 0ULL ? 0.5 : -0.5;
    }
  }
  double norm = 0.0;
  for (const auto value : values) norm += value * value;
  if (norm > 0.0) {
    const auto scale = 1.0 / std::sqrt(norm);
    for (auto& value : values) value *= scale;
  }
  return values;
}

EmbeddingResult to_embeddings(const EventLog& log,
                              const EmbeddingOptions& options) {
  validate_options(options);
  EmbeddingResult result;
  result.variant = options.variant;
  result.provider = options.provider;
  result.embedding_model = options.embedding_model;
  result.attribute_key = options.attribute_key;
  result.dimensions = options.dimensions;

  if (options.variant == EmbeddingVariant::CASES_TRANSFORMERS) {
    if (log.traces.size() > options.max_items)
      throw std::invalid_argument("embeddings: case count exceeds max_items");
    result.items.reserve(log.traces.size());
    for (const auto& trace : log.traces) {
      std::ostringstream text;
      bool first = true;
      for (const auto& event : trace.events) {
        if (!first) text << ' ';
        first = false;
        text << selected_value(event, options.attribute_key);
      }
      result.items.push_back(
          {trace.case_id, embed_text(text.str(), options)});
    }
    return result;
  }

  if (log.event_count() > options.max_items)
    throw std::invalid_argument("embeddings: event count exceeds max_items");
  result.items.reserve(log.event_count());
  std::size_t ordinal = 0;
  for (const auto& trace : log.traces) {
    for (const auto& event : trace.events) {
      result.items.push_back({event_identifier(event, ordinal,
                                               options.event_id_key),
                              embed_text(selected_value(event,
                                                         options.attribute_key),
                                        options)});
      ++ordinal;
    }
  }
  return result;
}

std::vector<EmbeddingSimilarity> embedding_similarity(
    std::string_view target_text, const EmbeddingResult& result,
    const EmbeddingOptions& options, std::size_t top_k) {
  validate_options(options);
  validate_result(result, options);
  const auto target = embed_text(target_text, options);
  std::vector<EmbeddingSimilarity> matches;
  matches.reserve(result.items.size());
  for (const auto& item : result.items)
    matches.push_back({item.id, cosine(target, item.values)});
  std::stable_sort(matches.begin(), matches.end(), [](const auto& left, const auto& right) {
    if (left.score != right.score) return left.score > right.score;
    return left.id > right.id;
  });
  if (top_k != 0 && matches.size() > top_k) matches.resize(top_k);
  return matches;
}

EventLog filter_embeddings_top_k(const EventLog& log,
                                 std::string_view target_text,
                                 std::size_t top_k,
                                 const EmbeddingOptions& options) {
  validate_options(options);
  if (top_k == 0) return {};
  const auto result = to_embeddings(log, options);
  const auto matches = embedding_similarity(target_text, result, options, top_k);
  std::unordered_set<std::string> selected;
  for (const auto& match : matches) selected.insert(match.id);
  std::unordered_set<std::string> selected_cases;

  EventLog filtered;
  filtered.attributes = log.attributes;
  std::size_t ordinal = 0;
  for (const auto& trace : log.traces) {
    if (options.variant == EmbeddingVariant::CASES_TRANSFORMERS) {
      if (selected.contains(trace.case_id)) filtered.traces.push_back(trace);
      ordinal += trace.events.size();
      continue;
    }
    for (const auto& event : trace.events) {
      const auto id = event_identifier(event, ordinal++, options.event_id_key);
      if (selected.contains(id)) selected_cases.insert(trace.case_id);
    }
  }
  if (options.variant == EmbeddingVariant::EVENTS_TRANSFORMERS) {
    ordinal = 0;
    for (const auto& trace : log.traces) {
      if (options.keep_cases && selected_cases.contains(trace.case_id)) {
        filtered.traces.push_back(trace);
        ordinal += trace.events.size();
        continue;
      }
      Trace copy = trace;
      copy.events.clear();
      for (const auto& event : trace.events) {
        const auto id = event_identifier(event, ordinal++, options.event_id_key);
        if (selected.contains(id)) copy.events.push_back(event);
      }
      if (!copy.events.empty()) filtered.traces.push_back(std::move(copy));
    }
  }
  return filtered;
}

std::string serialize_embeddings_json(const EmbeddingResult& result) {
  std::ostringstream out;
  out << std::setprecision(17);
  out << "{\"family\":\"TO_EMBEDDINGS\",\"variant\":";
  write_json_string(out, variant_name(result.variant));
  out << ",\"embedding_provider\":";
  write_json_string(out, provider_name(result.provider));
  out << ",\"embedding_model\":";
  write_json_string(out, result.embedding_model);
  out << ",\"attribute_key\":";
  write_json_string(out, result.attribute_key);
  out << ",\"dimensions\":" << result.dimensions << ",\"items\":[";
  for (std::size_t i = 0; i < result.items.size(); ++i) {
    if (i) out << ',';
    out << "{\"id\":";
    write_json_string(out, result.items[i].id);
    out << ",\"embedding\":[";
    for (std::size_t j = 0; j < result.items[i].values.size(); ++j) {
      if (j) out << ',';
      out << result.items[i].values[j];
    }
    out << "]}";
  }
  out << "]}";
  return out.str();
}

std::string serialize_embedding_similarity_json(
    std::string_view target_text, const EmbeddingResult& result,
    const std::vector<EmbeddingSimilarity>& matches, std::size_t top_k) {
  std::ostringstream out;
  out << std::setprecision(17);
  out << "{\"family\":\"TO_EMBEDDINGS_SIMILARITY\",\"variant\":";
  write_json_string(out, variant_name(result.variant));
  out << ",\"embedding_provider\":";
  write_json_string(out, provider_name(result.provider));
  out << ",\"target\":";
  write_json_string(out, target_text);
  out << ",\"embedding_model\":";
  write_json_string(out, result.embedding_model);
  out << ",\"dimensions\":" << result.dimensions
      << ",\"top_k\":" << top_k << ",\"items\":[";
  for (std::size_t i = 0; i < matches.size(); ++i) {
    if (i) out << ',';
    out << "{\"id\":";
    write_json_string(out, matches[i].id);
    out << ",\"score\":" << matches[i].score << '}';
  }
  out << "]}";
  return out.str();
}

}  // namespace minera
