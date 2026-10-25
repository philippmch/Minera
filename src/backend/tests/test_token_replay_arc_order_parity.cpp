// Forward token replay against the public reference on shuffled PNML
// documents (docs/token-replay-arc-order-bank.json.gz). The reference breaks
// ties between equally short silent paths by the order in which a place's
// outgoing arcs are visited, which for an imported document is the PNML arc
// order; place, transition and arc element order and element names are
// shuffled independently, so no other order can pass. Rows whose public
// outcome moved with the hash seed or with the model's object addresses are
// excluded by the capture.
#include "minera/conformance.hpp"
#include "minera/event_log.hpp"
#include "minera/pnml.hpp"

#include <cstddef>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

namespace {

using namespace minera;

enum RowState { kRow, kError, kSkipped };

struct Row {
  std::vector<std::string> trace;
  RowState state;
  std::vector<std::string> activated;
  long long missing, consumed, remaining, produced;
  bool fit;
};

struct BankCase {
  const char* name;
  const char* pnml;
  std::vector<Row> rows;
};

const std::vector<BankCase>& bank() {
  static const std::vector<BankCase> cases = {
#include "token_replay_arc_order_cases.inc"
  };
  return cases;
}

EventLog single_trace(const std::vector<std::string>& activities) {
  EventLog log;
  Trace trace;
  trace.case_id = "0";
  trace.case_id_state = FirstClassAttributeState::kPresent;
  std::size_t order = 0;
  for (const auto& activity : activities) {
    Event event;
    event.case_id = trace.case_id;
    event.activity = activity;
    event.activity_state = FirstClassAttributeState::kPresent;
    event.input_order = order++;
    trace.events.push_back(std::move(event));
  }
  log.traces.push_back(std::move(trace));
  return log;
}

// Empty when the replay matches the row, otherwise a description.
std::string replay_difference(const PnmlDocument& document, const Row& row, bool arc_order) {
  TokenReplayOptions options;
  options.stop_immediately_unfit = false;
  options.consider_remaining_in_fitness = true;
  options.try_to_reach_final_marking_through_hidden = true;
  options.exact_initial_marking = document.initial_marking;
  options.exact_final_marking = document.final_marking;
  options.initial_marking_keys = document.initial_marking_keys;
  options.final_marking_keys = document.final_marking_keys;
  options.place_names = document.place_ids;
  options.transition_names = document.transition_ids;
  if (arc_order)
    options.place_outgoing_transition_order = pnml_place_outgoing_transition_order(document);
  const auto rows = replay_token_result(single_trace(row.trace), document.net, options).traces;
  if (rows.size() != 1) return "expected one trace row";
  const auto& actual = rows.front();
  std::vector<std::string> activated;
  for (const auto transition : actual.activated_transitions)
    activated.push_back(document.transition_ids.at(transition));
  std::string detail;
  if (activated != row.activated) {
    detail += "activated";
    for (const auto& name : activated) detail += " " + name;
    detail += " vs";
    for (const auto& name : row.activated) detail += " " + name;
    detail += "; ";
  }
  if (actual.missing != row.missing || actual.consumed != row.consumed ||
      actual.remaining != row.remaining || actual.produced != row.produced)
    detail += "token counters differ; ";
  if (actual.trace_is_fit != row.fit) detail += "fitness flag differs; ";
  return detail;
}

}  // namespace

int main() {
  int failures = 0;
  std::size_t compared = 0, skipped = 0, rejected = 0, order_sensitive = 0, tie_rows = 0;
  for (const auto& item : bank()) {
    PnmlImportOptions import;
    import.public_marking_semantics = true;
    const auto document = parse_pnml_document(item.pnml, import);
    if (!pnml_place_outgoing_transition_order(document)) {
      ++failures;
      std::fprintf(stderr, "%s: no arc order derived from the document\n", item.name);
      continue;
    }
    for (const auto& row : item.rows) {
      if (row.state == kSkipped) { ++skipped; continue; }
      if (row.state == kError) {
        // The generator never builds a net the public replay rejects.
        ++rejected;
        continue;
      }
      ++compared;
      if (std::string(item.name).starts_with("tie_")) ++tie_rows;
      std::string detail;
      try {
        detail = replay_difference(document, row, true);
      } catch (const std::exception& error) {
        detail = std::string("threw ") + error.what();
      }
      if (!detail.empty()) {
        ++failures;
        std::fprintf(stderr, "%s: %s\n", item.name, detail.c_str());
      }
      try {
        if (!replay_difference(document, row, false).empty()) ++order_sensitive;
      } catch (const std::exception&) {
        ++order_sensitive;
      }
    }
  }
  std::printf("%zu rows compared (%zu tie rows), %zu skipped as unstable, %zu public errors; "
              "%zu rows differ under native transition order\n",
              compared, tie_rows, skipped, rejected, order_sensitive);
  // Guard against a capture that lost its observations or stopped
  // discriminating between the orders.
  if (compared < 3000 || tie_rows < 1000 || rejected != 0 || order_sensitive < 500) {
    ++failures;
    std::fprintf(stderr, "bank coverage below the pinned floor\n");
  }
  return failures == 0 ? 0 : 1;
}
