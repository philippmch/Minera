#include "minera/embeddings.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>

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
