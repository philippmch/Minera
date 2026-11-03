#include "minera/conformance.hpp"
#include "minera/token_replay_error.hpp"
#include "minera/log_conversion.hpp"
#include "token_replay_dataframe_grouping_reference_cases.hpp"
#include "../root_cause_trace_key.hpp"

#include <cmath>
#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace minera;

namespace {
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
template <typename Exception, typename Action>
void rejects(Action action, const char* message) {
  try { action(); } catch (const Exception&) { return; }
  throw std::runtime_error(message);
}
template <typename Action>
void rejects_undefined_backwards_fitness(Action action, const char* message) {
  try {
    action();
  } catch (const std::domain_error& error) {
    require(std::string(error.what()) ==
                "backwards token-replay fitness is undefined when consumed or "
                "produced tokens are zero",
            message);
    return;
  }
  throw std::runtime_error(message);
}
EventRecord row(std::string case_id, std::string activity) {
  return {{{"case:concept:name", std::move(case_id)},
           {"concept:name", std::move(activity)}}};
}
ProcessTree sequence_tree() {
  return {{"A", "B"}, {ProcessTreeOperator::kSequence, 0,
      {ProcessTreeNode::leaf(0), ProcessTreeNode::leaf(1)}}};
}
std::vector<std::string> activities(const Trace& trace) {
  std::vector<std::string> result;
  for (const auto& event : trace.events) result.push_back(event.activity);
  return result;
}
void same_places(const std::vector<TokenReplayPlaceDiagnostics>& a,
                 const std::vector<TokenReplayPlaceDiagnostics>& b) {
  require(a.size() == b.size(), "place diagnostic size");
  for (std::size_t i = 0; i < a.size(); ++i)
    require(a[i].consumed == b[i].consumed && a[i].produced == b[i].produced &&
            a[i].missing == b[i].missing && a[i].remaining == b[i].remaining,
            "place diagnostic counters");
}
void same_traces(const std::vector<TokenReplayTraceDiagnostics>& a,
                 const std::vector<TokenReplayTraceDiagnostics>& b) {
  require(a.size() == b.size(), "trace diagnostic size");
  for (std::size_t i = 0; i < a.size(); ++i) {
    require(a[i].case_id == b[i].case_id && a[i].trace_index == b[i].trace_index &&
            a[i].trace_fitness == b[i].trace_fitness &&
            a[i].trace_is_fit == b[i].trace_is_fit &&
            a[i].produced == b[i].produced && a[i].consumed == b[i].consumed &&
            a[i].missing == b[i].missing && a[i].remaining == b[i].remaining &&
            a[i].activated_transitions == b[i].activated_transitions &&
            a[i].transitions_with_problems == b[i].transitions_with_problems &&
            a[i].enabled_transitions == b[i].enabled_transitions &&
            a[i].reached_marking == b[i].reached_marking &&
            a[i].unknown_event_indices == b[i].unknown_event_indices &&
            a[i].unknown_activities == b[i].unknown_activities &&
            a[i].unknown_activity_is_null == b[i].unknown_activity_is_null &&
            a[i].replay_stopped == b[i].replay_stopped &&
            a[i].replayed_events == b[i].replayed_events,
            "trace diagnostics retain grouped replay semantics");
    same_places(a[i].places, b[i].places);
  }
}
void same_conformance(const ConformanceResult& a, const ConformanceResult& b) {
  // An undefined precision (NaN, not computed) is the same outcome on both sides.
  require(a.fitness == b.fitness &&
          (a.precision == b.precision || (std::isnan(a.precision) && std::isnan(b.precision))) &&
          a.precision_computed == b.precision_computed &&
          a.generalization == b.generalization && a.simplicity == b.simplicity &&
          a.fitting_traces == b.fitting_traces && a.trace_count == b.trace_count &&
          a.produced == b.produced && a.consumed == b.consumed &&
          a.missing == b.missing && a.remaining == b.remaining &&
          a.transition_frequencies == b.transition_frequencies &&
          a.variants.size() == b.variants.size(), "aggregate replay equivalence");
  for (std::size_t i = 0; i < a.variants.size(); ++i)
    require(a.variants[i].activities == b.variants[i].activities &&
            a.variants[i].count == b.variants[i].count &&
            a.variants[i].fitness == b.variants[i].fitness, "variant equivalence");
  require(a.trace_diagnostics.has_value() == b.trace_diagnostics.has_value(),
          "optional diagnostics retained");
  if (a.trace_diagnostics) same_traces(*a.trace_diagnostics, *b.trace_diagnostics);
}
void replay_flat_inputs_preserve_source_order() {
  EventStream stream;
  stream.events = {row("z", "A"), row("a", "B"), row("z", "B"), row("a", "A"),
                   row("unknown", "X")};
  stream.events[0].fields.push_back({"time:timestamp", "2030-01-01"});
  stream.events[2].fields.push_back({"time:timestamp", "invalid-and-unused"});
  stream.attributes = {{"name", "flat"}};
  stream.metadata.properties = {{"producer", "fixture"}};
  stream.events[0].fields.push_back({"extra", "kept"});
  const auto table = event_stream_to_table(stream);
  const auto lazy = LazyEventTable::from_table(table);
  auto grouped = event_stream_to_event_log(stream);
  const auto tree = sequence_tree();
  const auto net = to_petri_net(tree);
  for (auto variant : {TokenReplayVariant::kForward, TokenReplayVariant::kBackwards}) {
    TokenReplayOptions options;
    options.variant = variant;
    options.include_trace_diagnostics = true;
    for (const auto view : {EventDataView(stream), EventDataView(table), EventDataView(lazy),
                            EventDataView(grouped)}) {
      const auto log = normalize_token_replay_event_log(view, options);
      auto expected = grouped;
      const bool tabular = view.kind() == EventDataKind::kEventTable ||
                           view.kind() == EventDataKind::kLazyEventTable;
      if (tabular)
        std::sort(expected.traces.begin(), expected.traces.end(),
                   [](const Trace& a, const Trace& b) { return a.case_id < b.case_id; });
      const auto z = tabular ? 2 : 0, case_a = tabular ? 0 : 1;
      require(log.traces[z].case_id == "z" && log.traces[case_a].case_id == "a",
              "stream first-seen and table sorted case order");
      require(activities(log.traces[z]) == std::vector<std::string>{"A", "B"} &&
              activities(log.traces[case_a]) == std::vector<std::string>{"B", "A"},
              "source order, without timestamp sorting or validation");
      require(log.attributes == stream.attributes && log.metadata == stream.metadata,
              "log metadata retained");
      require(std::none_of(log.traces[z].events[1].attributes.begin(),
                            log.traces[z].events[1].attributes.end(),
                            [](const auto& field) { return field.key == "extra"; }),
              "unselected missing cells stay absent");
      if (variant == TokenReplayVariant::kForward) {
        same_conformance(check_conformance(view, net, options),
                          check_conformance(expected, net, options));
        same_conformance(check_conformance(view, tree, options),
                          check_conformance(expected, tree, options));
        same_traces(replay_token_traces(view, net, options),
                     replay_token_traces(expected, net, options));
      } else {
        rejects_undefined_backwards_fitness(
            [&] { (void)check_conformance(view, net, options); },
            "undefined backwards fitness rejected for flat inputs");
        rejects_undefined_backwards_fitness(
            [&] { (void)check_conformance(view, tree, options); },
            "undefined backwards fitness rejected for tree inputs");
        rejects_undefined_backwards_fitness(
            [&] { (void)replay_token_traces(view, net, options); },
            "undefined backwards trace fitness rejected for flat inputs");
      }
      const auto a = replay_token_diagnostics(view, net, options);
      const auto b = replay_token_diagnostics(expected, net, options);
      require(a.trace_count == b.trace_count && a.fitting_traces == b.fitting_traces &&
              a.transition_activations == b.transition_activations, "diagnostic totals");
      same_places(a.places, b.places);
    }
    if (variant == TokenReplayVariant::kForward) {
      same_conformance(check_conformance(table, net, options),
                        check_conformance(lazy, net, options));
      same_traces(replay_token_traces(lazy, net, options),
                  replay_token_traces(table, net, options));
    } else {
      rejects_undefined_backwards_fitness(
          [&] { (void)check_conformance(table, net, options); },
          "undefined backwards table fitness rejected");
      rejects_undefined_backwards_fitness(
          [&] { (void)check_conformance(lazy, net, options); },
          "undefined backwards lazy fitness rejected");
      const auto table_counts = replay_token_diagnostics(table, net, options);
      const auto lazy_counts = replay_token_diagnostics(lazy, net, options);
      require(table_counts.trace_count == lazy_counts.trace_count &&
              table_counts.fitting_traces == lazy_counts.fitting_traces &&
              table_counts.transition_activations == lazy_counts.transition_activations,
              "count-only table and lazy diagnostics equivalent");
      same_places(table_counts.places, lazy_counts.places);

      // Excluding the zero-denominator X-only case preserves adapter parity
      // checks for valid backwards fitness results.
      auto valid_stream = stream;
      valid_stream.events.pop_back();
      const auto valid_table = event_stream_to_table(valid_stream);
      const auto valid_lazy = LazyEventTable::from_table(valid_table);
      const auto valid_grouped = event_stream_to_event_log(valid_stream);
      for (const auto valid_view : {EventDataView(valid_stream),
                                    EventDataView(valid_table),
                                    EventDataView(valid_lazy),
                                    EventDataView(valid_grouped)}) {
        const auto valid_log = normalize_token_replay_event_log(valid_view, options);
        same_conformance(check_conformance(valid_view, net, options),
                          check_conformance(valid_log, net, options));
        same_conformance(check_conformance(valid_view, tree, options),
                          check_conformance(valid_log, tree, options));
        same_traces(replay_token_traces(valid_view, net, options),
                    replay_token_traces(valid_log, net, options));
      }
      same_traces(replay_token_traces(valid_lazy, net, options),
                  replay_token_traces(valid_table, net, options));
    }
  }
}

void custom_selectors_and_typed_identity() {
  EventStream stream;
  EventAttribute numeric{"selected-case", "7", AttributeKind::kInt};
  numeric.typed_value = std::int64_t{7};
  EventAttribute label{"selected-label", "A"};
  label.children = {{"detail", "preserved"}};
  stream.events = {{{numeric, label, {"case:customer", "alice"}, {"case:concept:name", "stale"},
                     {"concept:name", "wrong"}}}};
  stream.events.push_back(stream.events[0]);
  stream.events[1].fields[0].kind = AttributeKind::kString;
  stream.events[1].fields[0].typed_value = std::monostate{};
  TokenReplayOptions options;
  options.case_id_attribute = "selected-case";
  options.activity_attribute = "selected-label";
  const auto table = event_stream_to_table(stream);
  const auto lazy = LazyEventTable::from_table(table);
  const auto net = to_petri_net(sequence_tree());
  for (const auto view : {EventDataView(stream), EventDataView(table), EventDataView(lazy)}) {
    const auto log = normalize_token_replay_event_log(view, options);
    require(log.traces.size() == 2, "typed case identities do not collapse");
    require(log.traces[0].case_id_attribute->typed_value == numeric.typed_value,
            "case native value retained");
    require(log.traces[0].events[0].activity_attribute->children == label.children,
            "selected activity child metadata retained");
    require(log.traces[0].events[0].activity_source_key == "selected-label",
            "selected source spelling retained");
    require(log.traces[0].events[0].attribute("case:customer") &&
            *log.traces[0].events[0].attribute("case:customer") == "alice",
            "prefixed case metadata retained without replacing selected grouping");
    for (auto variant : {TokenReplayVariant::kForward, TokenReplayVariant::kBackwards}) {
      options.variant = variant;
      same_conformance(check_conformance(view, net, options), check_conformance(log, net, options));
      const auto rows = replay_token_traces(view, net, options);
      same_traces(rows, replay_token_traces(log, net, options));
      require(rows[0].unknown_event_indices.empty() &&
              std::any_of(rows[0].activated_transitions.begin(), rows[0].activated_transitions.end(),
                  [&](std::size_t transition) {
                    const auto activity = net.transitions()[transition].activity;
                    return activity && net.activities()[*activity] == "A";
                  }), "custom A fires despite unrelated canonical wrong field");
      same_places(replay_token_diagnostics(view, net, options).places,
                    replay_token_diagnostics(log, net, options).places);
    }
  }
  EventLog grouped;
  Trace trace;
  trace.case_id = "same";
  trace.attributes = {{"trace-detail", "kept"}};
  Event event;
  event.activity = "old";
  event.input_order = 42;
  event.attributes = {{"selected-label", "A"}};
  trace.events.push_back(event);
  grouped.traces = {trace, trace, Trace{}};
  options.case_id_attribute = "not-a-flat-log";
  const auto normalized = normalize_token_replay_event_log(EventDataView(grouped), options);
  require(normalized.traces.size() == 3 && normalized.traces[2].events.empty(),
          "grouped duplicate and empty trace boundaries authoritative");
  require(normalized.traces[0].attributes == trace.attributes &&
          normalized.traces[0].events[0].input_order == 42 &&
          normalized.traces[0].events[0].activity == "A", "grouped metadata and projection");
  for (auto variant : {TokenReplayVariant::kForward, TokenReplayVariant::kBackwards}) {
    options.variant = variant;
    if (variant == TokenReplayVariant::kForward) {
      same_traces(replay_token_traces(EventDataView(grouped), net, options),
                  replay_token_traces(grouped, net, options));
    } else {
      rejects_undefined_backwards_fitness(
          [&] { (void)replay_token_traces(EventDataView(grouped), net, options); },
          "incomplete grouped trace has undefined backwards fitness");
      rejects_undefined_backwards_fitness(
          [&] { (void)replay_token_traces(grouped, net, options); },
          "incomplete grouped trace has undefined backwards fitness");
      const auto grouped_counts = replay_token_diagnostics(EventDataView(grouped), net, options);
      const auto direct_counts = replay_token_diagnostics(grouped, net, options);
      require(grouped_counts.trace_count == direct_counts.trace_count &&
              grouped_counts.fitting_traces == direct_counts.fitting_traces &&
              grouped_counts.transition_activations == direct_counts.transition_activations,
              "empty grouped count-only replay equivalent");
      same_places(grouped_counts.places, direct_counts.places);
      EventLog valid_grouped;
      auto complete_trace = trace;
      Event second_event;
      second_event.attributes = {{"selected-label", "B"}};
      complete_trace.events.push_back(std::move(second_event));
      valid_grouped.traces.push_back(std::move(complete_trace));
      same_traces(replay_token_traces(EventDataView(valid_grouped), net, options),
                  replay_token_traces(valid_grouped, net, options));
    }
  }
  grouped.traces.resize(1);
  grouped.traces[0].events[0].activity.clear();
  grouped.traces[0].events[0].attributes = {{"concept:name", "A"}};
  options = {};
  require(normalize_token_replay_event_log(EventDataView(grouped)).traces[0].events[0].activity == "A",
          "grouped exact generic activity works without first-class projection");
  same_traces(replay_token_traces(EventDataView(grouped), net), replay_token_traces(grouped, net));
  same_conformance(check_conformance(EventDataView(grouped), net), check_conformance(grouped, net));
  same_places(replay_token_diagnostics(EventDataView(grouped), net).places,
                replay_token_diagnostics(grouped, net).places);
}

void strict_selection_and_caps() {
  EventStream stream;
  stream.events = {row("c", "A"), row("c", "B")};
  const auto net = to_petri_net(sequence_tree());
  TokenReplayOptions options;
  options.max_rows_to_materialize = 2;
  const auto table = event_stream_to_table(stream);
  const auto lazy = LazyEventTable::from_table(table);
  const auto log = event_stream_to_event_log(stream);
  for (const auto view : {EventDataView(stream), EventDataView(table), EventDataView(lazy),
                          EventDataView(log)}) {
    require(normalize_token_replay_event_log(view, options).event_count() == 2, "exact cap accepted");
    for (std::size_t cap : {0, 1}) {
      options.max_rows_to_materialize = cap;
      rejects<std::length_error>([&] { (void)normalize_token_replay_event_log(view, options); },
                                  "cap rejects before projection");
      rejects<std::length_error>([&] { (void)check_conformance(view, net, options); }, "conformance cap");
      rejects<std::length_error>([&] { (void)replay_token_traces(view, net, options); }, "trace cap");
      rejects<std::length_error>([&] { (void)replay_token_diagnostics(view, net, options); }, "diagnostic cap");
    }
    options.max_rows_to_materialize = 2;
  }
  options.max_rows_to_materialize = 0;
  std::size_t scans = 0;
  const LazyEventTable known(table.columns, [&](const auto& consume) {
    ++scans;
    for (const auto& values : table.rows) consume(values);
  }, {}, {}, table.size());
  rejects<std::length_error>([&] { (void)normalize_token_replay_event_log(EventDataView(known), options); },
                              "known lazy size rejects before scanning");
  require(scans == 0, "cap blocks known oversized scanner");
  const LazyEventTable unknown(table.columns, [&](const auto& consume) {
    ++scans;
    for (const auto& values : table.rows) consume(values);
  });
  options.max_rows_to_materialize = 1;
  rejects<std::length_error>([&] { (void)normalize_token_replay_event_log(EventDataView(unknown), options); },
                              "unknown lazy size enforces cap during materialization");
  require(scans == 1, "unknown lazy source scanned once");
  options.max_rows_to_materialize = 0;
  EventStream empty_stream;
  EventTable empty_table;
  empty_table.columns = {"case:concept:name", "concept:name"};
  EventLog empty_log;
  const auto empty_lazy = LazyEventTable::from_table(empty_table);
  for (const auto view : {EventDataView(empty_stream), EventDataView(empty_table),
                          EventDataView(empty_log), EventDataView(empty_lazy)})
    require(normalize_token_replay_event_log(view, options).traces.empty(), "zero cap accepts empty inputs");
  EventTable no_schema;
  rejects<std::invalid_argument>([&] { (void)normalize_token_replay_event_log(EventDataView(no_schema)); },
                                "empty table still requires selected schema");
  for (std::size_t selected : {0, 1}) {
    for (int failure = 0; failure < 4; ++failure) {
      auto invalid = stream;
      if (failure == 0) invalid.events[0].fields.erase(invalid.events[0].fields.begin() + selected);
      if (failure == 1) continue;  // Present null has replay semantics, tested separately.
      if (failure == 2) invalid.events[0].fields.push_back(invalid.events[0].fields[selected]);
      if (failure == 3) {
        invalid.events[0].fields[selected].first_class_role = selected == 0
            ? FirstClassAttributeRole::kCaseId : FirstClassAttributeRole::kActivity;
        invalid.events[0].fields[selected].first_class_state = FirstClassAttributeState::kMissing;
      }
      rejects<std::invalid_argument>([&] {
        (void)normalize_token_replay_event_log(EventDataView(invalid));
      }, "missing/null/duplicate/missing-role selected fields rejected");
    }
    auto missing = table;
    missing.columns.push_back(selected == 0 ? "case" : "activity");
    for (auto& cells : missing.rows) cells.push_back(EventAttribute{missing.columns.back(), "fallback"});
    missing.rows[0][selected].reset();
    const auto missing_lazy = LazyEventTable::from_table(missing);
    for (const auto view : {EventDataView(missing), EventDataView(missing_lazy)})
      if (selected == 0)
        require(normalize_token_replay_event_log(view).event_count() == 1,
                "missing table case row discarded without alias fallback");
      else
        require(normalize_token_replay_event_log(view).traces[0].events[0].activity_state ==
                    FirstClassAttributeState::kNull, "missing selected activity is null, not alias");
  }
}

void transported_nulls_and_ambiguity() {
  EventStream stream;
  stream.events = {row("c", "A"), row("c", "B"), row("c", "A")};
  for (auto& record : stream.events) {
    auto& cell = record.fields[1];
    cell.key = "transported";
    cell.first_class_source_key = "concept:name";
    cell.first_class_role = FirstClassAttributeRole::kActivity;
    cell.first_class_state = FirstClassAttributeState::kPresent;
    record.fields.push_back({"activity", "fallback"});
  }
  require(normalize_token_replay_event_log(EventDataView(stream)).traces[0].events[0].activity == "A",
          "transported source beats alias");
  auto table = event_stream_to_table(stream);
  table.rows[0][table.column_index("transported")].reset();
  auto lazy = LazyEventTable::from_table(table);
  for (const auto view : {EventDataView(table), EventDataView(lazy)})
    require(normalize_token_replay_event_log(view).traces[0].events[0].activity_state ==
                FirstClassAttributeState::kNull, "missing transported cell must not fall back");
  table.rows[1][table.column_index("transported")]->first_class_source_key = "other-label";
  rejects<std::invalid_argument>([&] { (void)normalize_token_replay_event_log(EventDataView(table)); },
                                "conflicting absent-column provenance rejected");
  stream.events[0].fields.push_back({"concept:name", "B"});
  require(normalize_token_replay_event_log(EventDataView(stream)).traces[0].events[0].activity == "B",
          "exact selection beats transported source");
  stream.events[0].fields.back().is_null = true;
  require(normalize_token_replay_event_log(EventDataView(stream)).traces[0].events[0].activity_state ==
              FirstClassAttributeState::kNull, "null exact selection shadows source and alias");
}

void null_cases_and_activities_keep_distinct_semantics() {
  EventStream stream;
  stream.events = {row("c", "stale-null-payload"), row("c", "A"), row("c", "B")};
  stream.events[0].fields[1].is_null = true;
  const auto table = event_stream_to_table(stream);
  const auto lazy = LazyEventTable::from_table(table);
  const auto net = to_petri_net(sequence_tree());
  for (auto variant : {TokenReplayVariant::kForward, TokenReplayVariant::kBackwards}) {
    TokenReplayOptions options;
    options.variant = variant;
    for (const auto view : {EventDataView(stream), EventDataView(table), EventDataView(lazy)}) {
      const auto normalized = normalize_token_replay_event_log(view);
      require(normalized.event_count() == 3 && normalized.traces[0].events[0].activity_state ==
                  FirstClassAttributeState::kNull, "null activity retains event position");
      const auto rows = replay_token_traces(view, net, options);
      require(rows.size() == 1 && rows[0].unknown_event_indices == std::vector<std::size_t>{0} &&
              rows[0].unknown_activity_is_null == std::vector<bool>{true} &&
              rows[0].trace_is_fit, "null activity is ignored unknown by default");
      same_traces(rows, replay_token_traces(normalized, net, options));
      same_conformance(check_conformance(view, net, options), check_conformance(normalized, net, options));
      same_places(replay_token_diagnostics(view, net, options).places,
                    replay_token_diagnostics(normalized, net, options).places);
    }
  }
  for (auto& record : stream.events) record.fields[0].is_null = true;
  stream.events[1].fields[0].is_null = false;
  stream.events[1].fields[0].first_class_role = FirstClassAttributeRole::kCaseId;
  stream.events[1].fields[0].first_class_state = FirstClassAttributeState::kNull;
  require(normalize_token_replay_event_log(EventDataView(stream)).traces.size() == 1,
          "present null stream cases form a group");
  auto mixed = stream;
  mixed.events.push_back(row("", "A"));
  require(normalize_token_replay_event_log(EventDataView(mixed)).traces.size() == 2,
          "mixed null representations group together separately from empty case string");
  const auto null_case_table = event_stream_to_table(stream);
  require(normalize_token_replay_event_log(EventDataView(null_case_table)).traces.empty(),
          "null table case rows are omitted");
  stream.events = {row("c", "")};
  const auto empty_label_net = to_petri_net(ProcessTree({""}, ProcessTreeNode::leaf(0)));
  const auto empty_label_rows = replay_token_traces(stream, empty_label_net);
  require(empty_label_rows.size() == 1 && empty_label_rows[0].trace_is_fit &&
          empty_label_rows[0].unknown_event_indices.empty(), "empty string is a real activity label");

  stream.events = {row("1", "A"), row("2", "A"), row("1", "B")};
  stream.events[1].fields[0].kind = AttributeKind::kInt;
  stream.events[2].fields[0].kind = AttributeKind::kInt;
  const auto sorted = normalize_token_replay_event_log(EventDataView(event_stream_to_table(stream)));
  require(sorted.traces.size() == 3 && sorted.traces[0].case_id == "1" &&
          sorted.traces[0].case_id_attribute->kind == AttributeKind::kInt &&
          sorted.traces[1].case_id == "2" &&
          sorted.traces[2].case_id_attribute->kind == AttributeKind::kString,
          "table numeric keys sort numerically before distinct string keys");
}

void numeric_case_ordering_matches_inferred_index() {
  EventStream stream;
  stream.events = {row("9007199254740993", "A"), row("9007199254740992", "A"),
                   row("9223372036854775808", "A"), row("9223372036854775807", "A"),
                   row("9007199254740992", "A"), row("-1", "A"), row("-1.5", "A")};
  for (auto& record : stream.events) record.fields[0].kind = AttributeKind::kInt;
  stream.events[0].fields[0].typed_value = std::int64_t{9007199254740993LL};
  stream.events[2].fields[0].kind = AttributeKind::kFloat;
  stream.events[2].fields[0].typed_value = 0x1p63;
  stream.events[4].fields[0].kind = AttributeKind::kFloat;
  stream.events[6].fields[0].kind = AttributeKind::kFloat;
  const auto table = event_stream_to_table(stream);
  const auto lazy = LazyEventTable::from_table(table);
  // Float index inference overwrites earlier groups when numeric keys round
  // together. Exact stream conversion is intentionally a different boundary.
  const std::vector<std::string> expected = {"-1.5", "-1",
      "9007199254740993", "9223372036854775808"};
  for (const auto view : {EventDataView(table), EventDataView(lazy)}) {
    const auto log = normalize_token_replay_event_log(view);
    std::vector<std::string> actual;
    for (const auto& trace : log.traces) actual.push_back(trace.case_id);
    require(actual == expected, "numeric case sorting matches inferred index collisions");
    require(log.traces[2].case_id_attribute->kind == AttributeKind::kInt &&
            log.traces[2].events.size() == 1,
            "inferred key collisions keep the later exact group");
  }
}

void activity_errors_precede_backwards_fitness() {
  const auto net = to_petri_net(ProcessTree({"A"}, ProcessTreeNode::leaf(0)));
  const auto expect_error = [&](const EventLog& log, TokenReplayOptions options,
                                const std::string& expected) {
    try {
      if (options.variant == TokenReplayVariant::kBackwards)
        (void)replay_token_traces(log, net, options);
      else
        (void)replay_token_result(log, net, options);
    } catch (const std::invalid_argument& error) {
      require(error.what() == expected, "selected activity error precedence");
      if (const auto* key_error = dynamic_cast<const TokenReplayKeyError*>(&error)) {
        require(key_error->reference_type() == "builtins.KeyError" &&
                    key_error->reference_args() ==
                        std::array<std::string, 1>{options.activity_attribute},
                "missing key payload precedes backwards fitness");
      }
      return;
    }
    throw std::runtime_error("selected activity error must precede replay");
  };
  for (const auto selector : {"concept:name", "chosen"}) {
    Event valid;
    valid.attributes = {{selector, "A"}};
    Event missing;
    missing.attributes = {{selector == std::string("chosen") ? "concept:name" : "chosen", "A"}};
    for (const auto container : {AttributeContainerKind::kList,
                                AttributeContainerKind::kContainer}) {
      Event unhashable;
      EventAttribute cell{selector, ""};
      cell.container = container;
      cell.children = {{"x", "A"}};
      unhashable.attributes.push_back(cell);
      for (const auto variant : {TokenReplayVariant::kForward, TokenReplayVariant::kBackwards}) {
        for (const auto disable : {std::optional<bool>{}, std::optional<bool>{false},
                                   std::optional<bool>{true}}) {
          TokenReplayOptions options;
          options.variant = variant;
          options.activity_attribute = selector;
          options.disable_variants = disable;
          const std::string missing_error = "'" + std::string(selector) + "'";
          Trace valid_trace;
          valid_trace.events = {valid};
          Trace missing_trace;
          missing_trace.events = {missing};
          Trace mixed_trace;
          mixed_trace.events = {unhashable, missing};
          Trace unhashable_trace;
          unhashable_trace.events = {unhashable};
          EventLog log;
          log.traces = {Trace{}, valid_trace, missing_trace};
          expect_error(log, options, missing_error);
          log.traces = {Trace{}, mixed_trace};
          expect_error(log, options, missing_error);
          log.traces = {Trace{}, missing_trace, unhashable_trace};
          expect_error(log, options, missing_error);
          log.traces = {Trace{}, unhashable_trace, missing_trace};
          expect_error(log, options, variant == TokenReplayVariant::kForward
              ? missing_error : "unhashable activity variant key");
          log.traces = {Trace{}, unhashable_trace};
          expect_error(log, options, "unhashable activity variant key");
          if (variant == TokenReplayVariant::kBackwards) {
            log.traces = {Trace{}, valid_trace};
            rejects_undefined_backwards_fitness(
                [&] { (void)replay_token_traces(log, net, options); },
                "empty valid backwards trace still has undefined fitness");
          }
        }
      }
    }
  }
}

void activity_selectors_are_literal_keys() {
  const auto net = to_petri_net(ProcessTree({"A"}, ProcessTreeNode::leaf(0)));
  const std::vector<std::string> selectors = {
      "concept:name", "chosen", "activity", "", "'", "\"", "'\"", "\\",
      "\n", "\r", "\t", std::string(1, '\0'), "\x7f", "\xc2\xa0",
      "\xe2\x80\x8b", "\xcd\xb8", "\xe6\xbc\xa2\xe5\xad\x97",
      "\xf0\x9f\x8c\xbf", "\xed\xa0\x80", "\xed\xbf\xbf"};
  for (const auto& selector : selectors) {
    Event selected;
    selected.attributes = {{"concept:name", "B"}};
    if (selector == "concept:name") selected.attributes.clear();
    selected.attributes.push_back({selector, "A"});
    Trace trace;
    trace.case_id = "literal";
    trace.events = {selected};
    EventLog log;
    log.traces = {trace};
    EventStream stream;
    EventRecord record;
    record.fields = selected.attributes;
    record.fields.push_back({"case:concept:name", "literal"});
    stream.events = {record};
    const auto table = event_stream_to_table(stream);
    const auto lazy = LazyEventTable::from_table(table);
    for (const auto variant : {TokenReplayVariant::kForward, TokenReplayVariant::kBackwards}) {
      TokenReplayOptions options;
      options.variant = variant;
      options.activity_attribute = selector;
      for (const auto view : {EventDataView(log), EventDataView(stream),
                              EventDataView(table), EventDataView(lazy)}) {
        const auto normalized = normalize_token_replay_event_log(view, options);
        require(normalized.traces[0].events[0].activity == "A",
                "literal selected field beats concept:name");
        if (selector != "concept:name") {
          const auto canonical = native_event_attribute(normalized.traces[0].events[0], "concept:name");
          require(canonical && canonical->value == "B",
                  "normalization preserves the unselected canonical field");
        }
        const auto round_trip = event_stream_to_event_log(event_log_to_event_stream(normalized));
        const auto restored = native_event_attribute(round_trip.traces[0].events[0], selector);
        require(restored && restored->value == "A",
                "literal selected keys survive flat round trips");
        const auto rows = replay_token_traces(view, net, options);
        require(rows.size() == 1 && rows[0].trace_is_fit &&
                    rows[0].activated_transitions.size() == 1,
                "literal selector replays across all event-data adapters");
        require(check_conformance(view, net, options).fitting_traces == 1 &&
                    replay_token_diagnostics(view, net, options).fitting_traces == 1,
                "aggregate and diagnostic replay use the same literal selector");
      }
      const auto rows = replay_token_traces(log, net, options);
      require(rows[0].trace_is_fit, "direct grouped replay uses literal selector");
      require(check_conformance(log, net, options).fitting_traces == 1 &&
                  replay_token_diagnostics(log, net, options).fitting_traces == 1,
              "direct aggregate and diagnostics use literal selector");
      if (variant == TokenReplayVariant::kForward)
        require(replay_token_result(log, net, options).traces.size() == 1,
                "full replay resolves literal selector");
      if (selector == "concept:name") continue;
      Event canonical;
      canonical.activity = "A";
      canonical.activity_state = FirstClassAttributeState::kPresent;
      canonical.activity_source_key = "concept:name";
      canonical.activity_attribute = EventAttribute{"concept:name", "A"};
      canonical.attributes = {{"concept:name", "A"}};
      auto missing = log;
      missing.traces[0].events = {canonical};
      rejects<std::invalid_argument>([&] { (void)replay_token_traces(missing, net, options); },
          "canonical activity is not an alias for a literal selector");
      rejects<std::invalid_argument>([&] { (void)normalize_token_replay_event_log(EventDataView(missing), options); },
          "grouped adapter respects first-class source metadata");
      auto missing_stream = stream;
      missing_stream.events[0].fields = {{"case:concept:name", "literal"}, {"concept:name", "A"}};
      rejects<std::invalid_argument>([&] { (void)normalize_token_replay_event_log(EventDataView(missing_stream), options); },
          "flat adapter does not alias literal activity keys");
    }
  }
  Event projected;
  projected.activity = "A";
  projected.activity_state = FirstClassAttributeState::kPresent;
  projected.activity_source_key = "chosen";
  projected.activity_attribute = EventAttribute{"chosen", "A"};
  Trace trace;
  trace.events = {projected};
  EventLog log;
  log.traces = {trace};
  rejects<std::invalid_argument>([&] { (void)replay_token_traces(log, net); },
      "explicit projected key does not fabricate concept:name");
  TokenReplayOptions options;
  options.activity_attribute = "chosen";
  require(replay_token_traces(log, net, options)[0].trace_is_fit,
          "explicit first-class source key remains available");
}
void missing_keys_retain_public_error_data() {
  const auto net = to_petri_net(sequence_tree());
  const std::vector<std::pair<std::string, std::string>> cases = {
      {"", "''"}, {"'", "\"'\""}, {"'\"", "'\\\'\"'"},
      {std::string("pre\0suffix", 10), "'pre\\x00suffix'"},
      {"\n\r\t\\", "'\\n\\r\\t\\\\'"},
      {"漢字🌿", "'漢字🌿'"},
      {std::string("\355\240\200\355\277\277", 6), "'\\ud800\\udfff'"}};
  for (const auto& [key, message] : cases) {
    TokenReplayOptions options;
    options.activity_attribute = key;
    options.timestamp_attribute = key;
    Trace trace;
    trace.events = {Event{}};
    EventLog log;
    log.traces = {trace};
    const auto check = [&](auto action) {
      try { action(); }
      catch (const TokenReplayKeyError& error) {
        require(error.reference_type() == "builtins.KeyError", "public error type");
        require(error.reference_args() == std::array<std::string, 1>{key},
                "missing key retained without C-string truncation");
        require(std::string(error.what()) == message, "public key representation");
        const std::invalid_argument& compatible = error;
        require(std::string(compatible.what()) == message, "legacy catch compatibility");
        return;
      }
      throw std::runtime_error("expected typed missing-key error");
    };
    for (auto variant : {TokenReplayVariant::kForward, TokenReplayVariant::kBackwards}) {
      options.variant = variant;
      check([&] { (void)replay_token_traces(log, net, options); });
      check([&] { (void)normalize_token_replay_event_log(EventDataView(log), options); });
      auto marked = log;
      EventAttribute field{key, "A"};
      field.first_class_role = FirstClassAttributeRole::kActivity;
      field.first_class_source_key = key;
      field.first_class_state = FirstClassAttributeState::kMissing;
      marked.traces[0].events[0].attributes = {field};
      check([&] { (void)replay_token_traces(marked, net, options); });
      check([&] { (void)normalize_token_replay_event_log(EventDataView(marked), options); });
      marked.traces[0].events[0].attributes.clear();
      marked.traces[0].events[0].activity = "A";
      marked.traces[0].events[0].activity_source_key = key;
      marked.traces[0].events[0].activity_attribute = field;
      check([&] { (void)replay_token_traces(marked, net, options); });
      check([&] { (void)normalize_token_replay_event_log(EventDataView(marked), options); });
    }
    check([&] { (void)token_replay_case_duration(trace, key); });
    auto timed = trace;
    EventAttribute absent{key, "2020-01-01"};
    absent.first_class_role = FirstClassAttributeRole::kTimestamp;
    absent.first_class_state = FirstClassAttributeState::kMissing;
    timed.events[0].timestamp_source_key = key;
    timed.events[0].timestamp_attribute = absent;
    check([&] { (void)token_replay_case_duration(timed, key); });
    require(token_replay_transition_duration_diagnostics(log, {{0, "A", {0}}}, options).empty(),
            "an absent activity does not demand timestamps for an unused cohort");
  }
}
}  // namespace

int main() {
  try {
    check_public_replay_dataframe_grouping();
    replay_flat_inputs_preserve_source_order();
    custom_selectors_and_typed_identity();
    strict_selection_and_caps();
    transported_nulls_and_ambiguity();
    null_cases_and_activities_keep_distinct_semantics();
    numeric_case_ordering_matches_inferred_index();
    activity_errors_precede_backwards_fitness();
    activity_selectors_are_literal_keys();
    missing_keys_retain_public_error_data();
    std::cout << "Token replay event-data tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
