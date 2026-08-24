#include "minera/embeddings.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace minera;

static EventLog fixture() {
  EventLog log;
  Trace first;
  first.case_id = "c1";
  first.events = {
      Event{"c1", "Approve", "2024-01-01T00:00:00Z", "r1", 0},
      Event{"c1", "Ship", "2024-01-01T01:00:00Z", "r2", 1},
  };
  Trace second;
  second.case_id = "c2";
  second.events = {
      Event{"c2", "Reject", "2024-01-01T00:00:00Z", "r3", 2},
  };
  log.traces = {first, second};
  return log;
}

int main() {
  const auto log = fixture();
  EmbeddingOptions options;
  options.dimensions = 32;
  const auto cases = to_embeddings(log, options);
  assert(cases.variant == EmbeddingVariant::CASES_TRANSFORMERS);
  assert(cases.items.size() == 2 && cases.items[0].values.size() == 32);
  const auto again = to_embeddings(log, options);
  assert(cases.items[0].values == again.items[0].values);
  const auto matches = embedding_similarity("Approve Ship", cases, options, 1);
  assert(matches.size() == 1 && matches[0].id == "c1");
  const auto selected = filter_embeddings_top_k(log, "Approve Ship", 1, options);
  assert(selected.traces.size() == 1 && selected.traces[0].case_id == "c1");

  // The selected model is part of the encoder contract. The n-gram profile is
  // deterministic and intentionally produces a different vector from v1.
  auto v2_options = options;
  v2_options.embedding_model = "hashing-v2";
  const auto v2 = embed_text("Approve Ship", v2_options);
  const auto v2_again = embed_text("Approve Ship", v2_options);
  assert(v2 == v2_again);
  assert(v2 != embed_text("Approve Ship", options));
  auto named_options = options;
  named_options.embedding_model = "all-MiniLM-L6-v2";
  assert(embed_text("Approve Ship", named_options) !=
         embed_text("Approve Ship", options));
  bool model_mismatch = false;
  try {
    (void)embedding_similarity("Approve Ship", cases, v2_options, 1);
  } catch (const std::invalid_argument&) {
    model_mismatch = true;
  }
  assert(model_mismatch);

  EmbeddingOptions transformer_options = options;
  transformer_options.provider = EmbeddingProvider::TRANSFORMER;
  transformer_options.embedding_model = "demo-transformer";
  std::vector<std::string> callback_texts;
  std::string callback_model;
  std::size_t callback_dimensions = 0;
  transformer_options.transformer_callback =
      [&](std::string_view text, std::string_view model, std::size_t dimensions) {
        callback_texts.emplace_back(text);
        callback_model = std::string(model);
        callback_dimensions = dimensions;
        std::vector<double> values(dimensions, 0.0);
        values[0] = 1.0;
        return values;
      };
  const auto transformer_cases = to_embeddings(log, transformer_options);
  assert(transformer_cases.provider == EmbeddingProvider::TRANSFORMER);
  assert(callback_texts.size() == 2 && callback_texts[0] == "Approve Ship" &&
         callback_texts[1] == "Reject");
  assert(callback_model == "demo-transformer" && callback_dimensions == 32);
  const auto transformer_matches = embedding_similarity(
      "Approve Ship", transformer_cases, transformer_options, 0);
  assert(transformer_matches.size() == 2 && transformer_matches[0].id == "c2" &&
         transformer_matches[1].id == "c1");
  const auto transformer_top = embedding_similarity(
      "Approve Ship", transformer_cases, transformer_options, 1);
  assert(transformer_top.size() == 1 && transformer_top[0].id == "c2");
  const auto transformer_selected = filter_embeddings_top_k(
      log, "Approve Ship", 1, transformer_options);
  assert(transformer_selected.traces.size() == 1 &&
         transformer_selected.traces[0].case_id == "c2");
  assert(callback_texts.back() == "Approve Ship");

  // Callback execution is grouped by case and remains deterministic for
  // exact score ties: identifiers are ordered descending, with input order
  // retained for duplicate identifiers.
  assert(transformer_matches[0].id == "c2" &&
         transformer_matches[1].id == "c1");

  EventLog empty_log;
  const auto empty_cases = to_embeddings(empty_log, options);
  assert(empty_cases.items.empty());
  options.variant = EmbeddingVariant::EVENTS_TRANSFORMERS;
  const auto empty_events = to_embeddings(empty_log, options);
  assert(empty_events.items.empty());
  assert(embedding_similarity("anything", empty_events, options, 3).empty());

  transformer_options.variant = EmbeddingVariant::EVENTS_TRANSFORMERS;
  callback_texts.clear();
  const auto transformer_events = to_embeddings(log, transformer_options);
  assert(transformer_events.items.size() == 3);
  assert((callback_texts == std::vector<std::string>{"Approve", "Ship", "Reject"}));
  (void)embedding_similarity("Reject", transformer_events, transformer_options, 1);
  assert(callback_texts.back() == "Reject");

  auto invalid_variant = options;
  invalid_variant.variant = static_cast<EmbeddingVariant>(99);
  bool invalid_option = false;
  try {
    (void)to_embeddings(log, invalid_variant);
  } catch (const std::invalid_argument& error) {
    invalid_option = std::string(error.what()).find("unknown variant") !=
                     std::string::npos;
  }
  assert(invalid_option);

  auto invalid_provider = options;
  invalid_provider.provider = static_cast<EmbeddingProvider>(99);
  invalid_option = false;
  try {
    (void)embed_text("A", invalid_provider);
  } catch (const std::invalid_argument& error) {
    invalid_option = std::string(error.what()).find("unknown provider") !=
                     std::string::npos;
  }
  assert(invalid_option);

  auto invalid_model = options;
  invalid_model.embedding_model.clear();
  invalid_option = false;
  try {
    (void)embed_text("A", invalid_model);
  } catch (const std::invalid_argument&) {
    invalid_option = true;
  }
  assert(invalid_option);

  EmbeddingOptions missing_transformer = transformer_options;
  missing_transformer.transformer_callback = {};
  missing_transformer.embedding_model = "real-transformer";
  bool missing_provider = false;
  try {
    (void)embed_text("A", missing_transformer);
  } catch (const std::invalid_argument& error) {
    missing_provider = std::string(error.what()).find("requires transformer_callback") !=
                       std::string::npos;
  }
  assert(missing_provider);

  // Applications may register one process-level runtime bridge when passing
  // a callback through every options object is inconvenient. The bridge is
  // copied under a lock and invoked without holding that lock.
  std::vector<std::string> registered_texts;
  bool empty_registration_rejected = false;
  try {
    set_default_embedding_provider(EmbeddingProviderCallback{});
  } catch (const std::invalid_argument&) {
    empty_registration_rejected = true;
  }
  assert(empty_registration_rejected);
  set_default_embedding_provider(
      [&](std::string_view text, std::string_view model, std::size_t dimensions) {
        assert(model == "registered-transformer");
        registered_texts.emplace_back(text);
        return std::vector<double>(dimensions, 0.25);
      });
  auto registered_options = transformer_options;
  registered_options.transformer_callback = {};
  registered_options.embedding_model = "registered-transformer";
  registered_options.variant = EmbeddingVariant::CASES_TRANSFORMERS;
  const auto registered_cases = to_embeddings(log, registered_options);
  assert(registered_cases.items.size() == 2);
  registered_options.variant = EmbeddingVariant::EVENTS_TRANSFORMERS;
  const auto registered_events = to_embeddings(log, registered_options);
  assert(registered_events.items.size() == 3);
  assert((registered_texts == std::vector<std::string>{
              "Approve Ship", "Reject", "Approve", "Ship", "Reject"}));
  clear_default_embedding_provider();
  bool cleared_provider = false;
  try {
    (void)embed_text("A", registered_options);
  } catch (const std::invalid_argument& error) {
    cleared_provider =
        std::string(error.what()).find("requires transformer_callback") !=
        std::string::npos;
  }
  assert(cleared_provider);

  auto bad_length = transformer_options;
  bad_length.transformer_callback = [](std::string_view, std::string_view,
                                       std::size_t) { return std::vector<double>{1.0}; };
  bool bad_vector = false;
  try { (void)embed_text("A", bad_length); }
  catch (const std::invalid_argument&) { bad_vector = true; }
  assert(bad_vector);

  auto malformed_result = transformer_events;
  malformed_result.items[0].values[0] = std::numeric_limits<double>::infinity();
  bool malformed_rejected = false;
  try {
    (void)embedding_similarity("A", malformed_result, transformer_options, 1);
  } catch (const std::invalid_argument& error) {
    malformed_rejected =
        std::string(error.what()).find("result vector") != std::string::npos;
  }
  assert(malformed_rejected);

  auto mismatched_result = transformer_events;
  mismatched_result.variant = EmbeddingVariant::CASES_TRANSFORMERS;
  bool mismatch_rejected = false;
  try {
    (void)embedding_similarity("A", mismatched_result, transformer_options, 1);
  } catch (const std::invalid_argument& error) {
    mismatch_rejected =
        std::string(error.what()).find("result variant") != std::string::npos;
  }
  assert(mismatch_rejected);
  auto bad_values = transformer_options;
  bad_values.transformer_callback = [](std::string_view, std::string_view,
                                       std::size_t dimensions) {
    return std::vector<double>(dimensions, std::numeric_limits<double>::quiet_NaN());
  };
  bad_vector = false;
  try { (void)embed_text("A", bad_values); }
  catch (const std::invalid_argument&) { bad_vector = true; }
  assert(bad_vector);

  EmbeddingOptions mock_transformer = transformer_options;
  mock_transformer.transformer_callback = {};
  mock_transformer.embedding_model = "mock_transformer";
  assert(to_embeddings(log, mock_transformer).provider ==
         EmbeddingProvider::TRANSFORMER);
  assert(embed_text("Approve Ship", mock_transformer) !=
         embed_text("Approve Ship", options));

  options.variant = EmbeddingVariant::EVENTS_TRANSFORMERS;
  const auto events = to_embeddings(log, options);
  assert(events.items.size() == 3);
  const auto event_matches = embedding_similarity("Reject", events, options, 1);
  assert(event_matches.size() == 1 && event_matches[0].id == "2");
  const auto event_selected = filter_embeddings_top_k(log, "Reject", 1, options);
  assert(event_selected.traces.size() == 1 &&
         event_selected.traces[0].events.size() == 1 &&
         event_selected.traces[0].events[0].activity == "Reject");
  options.keep_cases = true;
  const auto case_selected = filter_embeddings_top_k(log, "Reject", 1, options);
  assert(case_selected.traces.size() == 1 &&
         case_selected.traces[0].events.size() == 1 &&
         case_selected.traces[0].case_id == "c2");

  const auto json = serialize_embeddings_json(cases);
  assert(json.find("\"family\":\"TO_EMBEDDINGS\"") != std::string::npos);
  const auto similarity_json = serialize_embedding_similarity_json(
      "Approve Ship", cases, matches, 1);
  assert(similarity_json.find("TO_EMBEDDINGS_SIMILARITY") != std::string::npos);
  assert(similarity_json.find("\"embedding_model\":\"hashing-v1\"") !=
         std::string::npos);
  assert(similarity_json.find("\"embedding_provider\":\"hashing\"") !=
         std::string::npos);
  assert(similarity_json.find("\"dimensions\":32") != std::string::npos);

  bool threw = false;
  try {
    options.dimensions = 0;
    (void)to_embeddings(log, options);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  return 0;
}
