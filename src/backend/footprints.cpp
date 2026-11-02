#include "minera/footprints.hpp"

#include <algorithm>
#include <cctype>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

#include "footprints/conformance_evaluation.hpp"
#include "footprints/conformance_log_extensive.hpp"
#include "footprints/conformance_log_model.hpp"
#include "footprints/conformance_trace_extensive.hpp"
#include "footprints/discovery_registry.hpp"
#include "minera/log_conversion.hpp"

namespace minera {
namespace {

void validate_selector(std::string_view value, std::string_view name) {
  if (value.empty() || std::all_of(value.begin(), value.end(), [](char c) {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
      })) {
    throw std::invalid_argument(std::string(name) +
                                " selector must not be empty");
  }
}

void validate_discovery_options(const FootprintsDiscoveryOptions& options) {
  validate_selector(options.activity_attribute, "activity");
  validate_selector(options.case_id_attribute, "case");
  validate_selector(options.timestamp_attribute, "timestamp");
  if (options.start_timestamp_attribute)
    validate_selector(*options.start_timestamp_attribute, "start timestamp");
}

footprints_detail::DiscoveryParameters discovery_parameters(
    const FootprintsDiscoveryOptions& options) {
  validate_discovery_options(options);
  footprints_detail::DiscoveryParameters parameters;
  parameters.activity_key = options.activity_attribute;
  parameters.case_id_key = options.case_id_attribute;
  parameters.timestamp_key = options.timestamp_attribute;
  parameters.start_timestamp_key = options.start_timestamp_attribute;
  parameters.index_key = options.index_attribute;
  parameters.sort_required = options.sort_required;
  parameters.initial_marking = options.initial_marking;
  parameters.transition_names = options.transition_names;
  return parameters;
}

const EventAttribute* find_attribute(
    const std::vector<EventAttribute>& attributes, std::string_view key) {
  const auto found = std::find_if(
      attributes.begin(), attributes.end(), [&](const auto& attribute) {
        return attribute.key == key;
      });
  return found == attributes.end() ? nullptr : &*found;
}

EventAttribute plain_attribute(EventAttribute value, std::string_view key) {
  value.key = std::string(key);
  value.first_class_role = FirstClassAttributeRole::kNone;
  value.first_class_state = FirstClassAttributeState::kMissing;
  value.first_class_source_key.clear();
  return value;
}

EventAttribute text_attribute(std::string_view key, std::string value,
                              AttributeKind kind = AttributeKind::kString) {
  EventAttribute result{std::string(key), std::move(value), kind, false};
  return result;
}

bool canonical_case_selector(std::string_view key) {
  return key == "case:concept:name" || key == "concept:name" ||
         key == "case_id" || key == "case:id";
}

bool canonical_activity_selector(std::string_view key) {
  return key == "concept:name" || key == "activity";
}

bool canonical_timestamp_selector(std::string_view key) {
  return key == "time:timestamp" || key == "timestamp";
}

std::optional<EventAttribute> selected_event_attribute(
    const Event& event, std::string_view key) {
  if (canonical_activity_selector(key) ||
      (!event.activity_source_key.empty() && key == event.activity_source_key)) {
    if (!event.has_activity()) {
      return EventAttribute{std::string(key), {}, AttributeKind::kString, true};
    }
    auto result = event.activity_attribute.value_or(
        text_attribute(key, event.activity));
    result.key = std::string(key);
    result.is_null = event.activity_state == FirstClassAttributeState::kNull ||
                     result.is_null;
    return plain_attribute(std::move(result), key);
  }
  if (canonical_timestamp_selector(key) ||
      (!event.timestamp_source_key.empty() &&
       key == event.timestamp_source_key)) {
    if (!event.has_timestamp()) {
      return EventAttribute{std::string(key), {}, AttributeKind::kDate, true};
    }
    auto result = event.timestamp_attribute.value_or(
        text_attribute(key, event.timestamp, AttributeKind::kDate));
    result.key = std::string(key);
    result.is_null = event.timestamp_state == FirstClassAttributeState::kNull ||
                     result.is_null;
    return plain_attribute(std::move(result), key);
  }
  if (const auto* attribute = find_attribute(event.attributes, key))
    return plain_attribute(*attribute, key);
  return std::nullopt;
}

std::optional<EventAttribute> selected_case_attribute(
    const Trace& trace, const Event& event, std::string_view key) {
  if (canonical_case_selector(key) ||
      (!trace.case_id_source_key.empty() && key == trace.case_id_source_key)) {
    if (!trace.has_case_id()) {
      return EventAttribute{std::string(key), {}, AttributeKind::kString, true};
    }
    auto result = trace.case_id_attribute.value_or(
        text_attribute(key, trace.case_id));
    result.key = std::string(key);
    result.is_null = trace.case_id_state == FirstClassAttributeState::kNull ||
                     result.is_null;
    return plain_attribute(std::move(result), key);
  }

  // Exact event columns take precedence for unprefixed selectors.  This is
  // important when a trace-level attribute and a genuine event column share a
  // spelling: the table adapter would expose the event column verbatim.
  if (!key.starts_with("case:")) {
    if (const auto* attribute = find_attribute(event.attributes, key))
      return plain_attribute(*attribute, key);
  }
  if (const auto* attribute = find_attribute(trace.attributes, key))
    return plain_attribute(*attribute, key);
  if (key.starts_with("case:")) {
    const auto unprefixed = key.substr(5);
    if (const auto* attribute = find_attribute(trace.attributes, unprefixed))
      return plain_attribute(*attribute, key);
  }
  if (const auto* attribute = find_attribute(event.attributes, key))
    return plain_attribute(*attribute, key);
  return std::nullopt;
}

std::string unique_case_glue_key(const EventLog& log,
                                 const FootprintsDiscoveryOptions& options) {
  std::set<std::string> occupied{options.activity_attribute,
                                 options.case_id_attribute,
                                 options.timestamp_attribute,
                                 options.index_attribute};
  if (options.start_timestamp_attribute)
    occupied.insert(*options.start_timestamp_attribute);
  for (const auto& trace : log.traces) {
    occupied.insert(trace.case_id_source_key);
    for (const auto& attribute : trace.attributes) occupied.insert(attribute.key);
    for (const auto& event : trace.events) {
      occupied.insert(event.activity_source_key);
      occupied.insert(event.timestamp_source_key);
      occupied.insert(event.resource_source_key);
      occupied.insert(event.lifecycle_source_key);
      for (const auto& attribute : event.attributes)
        occupied.insert(attribute.key);
    }
  }
  std::string candidate = "@@minera_footprints_case_id";
  std::size_t suffix = 0;
  while (occupied.contains(candidate))
    candidate = "@@minera_footprints_case_id_" + std::to_string(++suffix);
  return candidate;
}

bool has_column(const EventTable& table, std::string_view key) {
  return std::find(table.columns.begin(), table.columns.end(), key) !=
         table.columns.end();
}

std::optional<std::string> copy_alias_dtype(const EventTable& table,
                                            std::string_view source_key) {
  const auto source = std::find(table.columns.begin(), table.columns.end(),
                                source_key);
  if (source != table.columns.end()) {
    const auto index = static_cast<std::size_t>(source - table.columns.begin());
    return table.dtypes[index];
  }
  return std::nullopt;
}

void append_alias_column(EventTable& table, std::string_view key,
                         std::string_view source_key,
                         const std::vector<std::optional<EventAttribute>>& cells) {
  if (has_column(table, key)) return;
  if (cells.size() != table.rows.size())
    throw std::logic_error("footprints projection row count mismatch");
  std::optional<std::string> dtype;
  std::optional<EventTableCategoricalDtype> categorical_dtype;
  const bool had_categorical_metadata = !table.categorical_dtypes.empty();
  if (!table.dtypes.empty()) dtype = copy_alias_dtype(table, source_key);
  if (had_categorical_metadata) {
    const auto source = std::find(table.columns.begin(), table.columns.end(),
                                  source_key);
    if (source != table.columns.end()) {
      const auto index =
          static_cast<std::size_t>(source - table.columns.begin());
      categorical_dtype = table.categorical_dtypes[index];
    }
  }
  table.columns.emplace_back(key);
  if (dtype) {
    table.dtypes.push_back(std::move(*dtype));
    if (had_categorical_metadata)
      table.categorical_dtypes.push_back(std::move(categorical_dtype));
    else if (table.dtypes.back() == "category") {
      table.categorical_dtypes.assign(table.columns.size() - 1, std::nullopt);
      table.categorical_dtypes.push_back(std::nullopt);
    }
  } else if (!table.dtypes.empty()) {
    // The selected field has no exact source column in this projection.  Drop
    // the partial declaration rather than guessing a dtype for the alias.
    table.dtypes.clear();
    table.categorical_dtypes.clear();
  }
  for (std::size_t i = 0; i < table.rows.size(); ++i)
    table.rows[i].push_back(cells[i]);
}

std::vector<const Event*> source_events(const EventLog& log) {
  std::vector<const Event*> result;
  result.reserve(log.event_count());
  for (const auto& trace : log.traces)
    for (const auto& event : trace.events) result.push_back(&event);
  return result;
}

std::vector<const Trace*> source_traces_for_events(const EventLog& log) {
  std::vector<const Trace*> result;
  result.reserve(log.event_count());
  for (const auto& trace : log.traces)
    for (std::size_t i = 0; i < trace.events.size(); ++i)
      result.push_back(&trace);
  return result;
}

std::vector<std::string> resolve_case_ids_impl(
    const EventLog& log, std::string_view case_id_attribute) {
  validate_selector(case_id_attribute, "case");
  const bool canonical_case = canonical_case_selector(case_id_attribute);
  std::vector<std::string> result;
  result.reserve(log.traces.size());
  for (const auto& trace : log.traces) {
    if (trace.events.empty()) {
      if (canonical_case) {
        result.push_back(trace.case_id);
        continue;
      }
      const Event empty_event;
      const auto selected = selected_case_attribute(
          trace, empty_event, case_id_attribute);
      if (!selected || selected->is_null)
        throw std::invalid_argument("case selector is missing or null");
      result.push_back(selected->value);
      continue;
    }
    std::optional<std::string> value;
    for (const auto& event : trace.events) {
      const auto selected =
          selected_case_attribute(trace, event, case_id_attribute);
      if ((!selected || selected->is_null) && canonical_case) {
        if (!value) value = trace.case_id;
        continue;
      }
      if (!selected || selected->is_null)
        throw std::invalid_argument("case selector is missing or null");
      if (!value) value = selected->value;
      if (*value != selected->value)
        throw std::invalid_argument(
            "case selector must be constant within each trace");
    }
    result.push_back(*value);
  }
  return result;
}

}  // namespace

Footprint discover_footprints(const EventLog& log) {
  return discover_footprints(log, FootprintsDiscoveryOptions{});
}

Footprint discover_footprints(const EventLog& log,
                              const FootprintsDiscoveryOptions& options) {
  return footprints_detail::EntireEventLogFootprintsDiscovery::apply(
      log, discovery_parameters(options));
}

std::vector<Footprint> discover_trace_footprints(const EventLog& log) {
  return discover_trace_footprints(log, FootprintsDiscoveryOptions{});
}

std::vector<Footprint> discover_trace_footprints(
    const EventLog& log, const FootprintsDiscoveryOptions& options) {
  return footprints_detail::TraceByTraceFootprintsDiscovery::apply(
      log, discovery_parameters(options));
}

Footprint discover_footprints(const EventTable& table,
                              const FootprintsDiscoveryOptions& options) {
  validate_discovery_options(options);
  return footprints_detail::EntireDataframeFootprintsDiscovery::apply(
      table, discovery_parameters(options));
}

Footprint discover_footprints(const LazyEventTable& table,
                              const FootprintsDiscoveryOptions& options) {
  validate_discovery_options(options);
  return footprints_detail::PolarsLazyframesFootprintsDiscovery::apply(
      table, discovery_parameters(options));
}

EventLog prepare_footprints_event_log(
    const EventLog& log, const FootprintsDiscoveryOptions& options) {
  validate_discovery_options(options);
  const auto case_ids = resolve_case_ids_impl(log, options.case_id_attribute);
  EventLog result = log;
  for (std::size_t trace_index = 0; trace_index < result.traces.size();
       ++trace_index) {
    auto& trace = result.traces[trace_index];
    trace.case_id = case_ids[trace_index];
    const bool canonical_case =
        canonical_case_selector(options.case_id_attribute);
    if (!canonical_case) {
      trace.case_id_state = FirstClassAttributeState::kPresent;
      trace.case_id_source_key = options.case_id_attribute;
    }
    if (!trace.events.empty()) {
      const auto selected_case = selected_case_attribute(
          log.traces[trace_index], log.traces[trace_index].events.front(),
          options.case_id_attribute);
      if (selected_case && !selected_case->is_null) {
        auto projected_case = *selected_case;
        projected_case.key = options.case_id_attribute;
        trace.case_id_attribute = plain_attribute(
            std::move(projected_case), options.case_id_attribute);
      }
    }
    for (std::size_t event_index = 0; event_index < trace.events.size();
         ++event_index) {
      const auto& source_event =
          log.traces[trace_index].events[event_index];
      auto selected_activity =
          selected_event_attribute(source_event, options.activity_attribute);
      if (!selected_activity || selected_activity->is_null)
        throw std::invalid_argument(
            "activity selector is missing or null");
      auto& event = trace.events[event_index];
      event.activity = selected_activity->value;
      event.activity_state = FirstClassAttributeState::kPresent;
      event.activity_source_key = options.activity_attribute;
      selected_activity->key = options.activity_attribute;
      event.activity_attribute = plain_attribute(
          std::move(*selected_activity), options.activity_attribute);
      event.case_id = trace.case_id;
      event.case_id_state = trace.case_id_state;
      event.case_id_source_key = trace.case_id_source_key;
    }
  }
  return result;
}

EventTable prepare_footprints_event_table(
    const EventLog& log, const FootprintsDiscoveryOptions& options) {
  validate_discovery_options(options);
  // A private grouping key prevents the conversion adapter from replacing a
  // genuine event column whose name is selected as the case identifier.
  LogConversionOptions conversion;
  conversion.case_id_key = unique_case_glue_key(log, options);
  auto table = event_log_to_table(log, conversion);
  table.validate();
  const auto events = source_events(log);
  const auto traces = source_traces_for_events(log);
  if (table.rows.size() != events.size() || events.size() != traces.size())
    throw std::logic_error("footprints projection row count mismatch");

  const auto event_cells = [&](std::string_view key) {
    std::vector<std::optional<EventAttribute>> cells;
    cells.reserve(events.size());
    for (const auto* event : events)
      cells.push_back(selected_event_attribute(*event, key));
    return cells;
  };
  const auto case_cells = [&]() {
    std::vector<std::optional<EventAttribute>> cells;
    cells.reserve(events.size());
    for (std::size_t i = 0; i < events.size(); ++i)
      cells.push_back(selected_case_attribute(*traces[i], *events[i],
                                              options.case_id_attribute));
    return cells;
  };
  const auto activity_source = options.activity_attribute == "activity"
                                   ? std::string_view{"concept:name"}
                                   : std::string_view{options.activity_attribute};
  const auto timestamp_source = options.timestamp_attribute == "timestamp"
                                    ? std::string_view{"time:timestamp"}
                                    : std::string_view{options.timestamp_attribute};
  append_alias_column(table, options.activity_attribute, activity_source,
                      event_cells(options.activity_attribute));
  append_alias_column(table, options.timestamp_attribute, timestamp_source,
                      event_cells(options.timestamp_attribute));
  append_alias_column(table, options.case_id_attribute, conversion.case_id_key,
                      case_cells());
  if (options.start_timestamp_attribute)
    append_alias_column(
        table, *options.start_timestamp_attribute,
        *options.start_timestamp_attribute == "timestamp"
            ? std::string_view{"time:timestamp"}
            : std::string_view{*options.start_timestamp_attribute},
                        event_cells(*options.start_timestamp_attribute));

  std::vector<std::size_t> order(table.rows.size());
  std::iota(order.begin(), order.end(), 0);
  std::stable_sort(order.begin(), order.end(), [&](std::size_t left,
                                                   std::size_t right) {
    return events[left]->input_order < events[right]->input_order;
  });
  std::vector<EventTable::Row> ordered_rows;
  ordered_rows.reserve(table.rows.size());
  for (const auto index : order)
    ordered_rows.push_back(std::move(table.rows[index]));
  table.rows = std::move(ordered_rows);
  if (table.index) {
    if (table.index->labels.size() != order.size())
      throw std::invalid_argument(
          "footprints projection index label count does not match rows");
    std::vector<EventTableIndexLabel> ordered_labels;
    ordered_labels.reserve(order.size());
    for (const auto index : order)
      ordered_labels.push_back(std::move(table.index->labels[index]));
    table.index->labels = std::move(ordered_labels);
  }
  table.validate();
  return table;
}

LazyEventTable prepare_footprints_lazy_table(
    const EventLog& log, const FootprintsDiscoveryOptions& options) {
  return LazyEventTable::from_table(
      prepare_footprints_event_table(log, options));
}

std::vector<std::string> resolve_footprints_case_ids(
    const EventLog& log, std::string_view case_id_attribute) {
  return resolve_case_ids_impl(log, case_id_attribute);
}

Footprint discover_dfg_footprints(
    const std::map<FootprintPair, std::uint64_t>& dfg) {
  return footprints_detail::DfgFootprintsDiscovery::apply(dfg);
}

Footprint discover_dfg_footprints(const DirectlyFollowsGraph& graph) {
  return footprints_detail::DfgFootprintsDiscovery::apply(graph);
}

FootprintViolations check_footprints_log_model(
    const Footprint& log, const Footprint& model,
    FootprintsLogModelOptions options) {
  return footprints_conformance::LogModelVariant::apply(log, model, options);
}

std::vector<FootprintViolations> check_footprints_log_model(
    const std::vector<Footprint>& log, const Footprint& model,
    FootprintsLogModelOptions options) {
  return footprints_conformance::LogModelVariant::apply_tracewise(log, model,
                                                                    options,
                                                                    options.case_id_attribute);
}

FootprintsExtensiveResult check_footprints_log_extensive(
    const Footprint& log, const Footprint& model) {
  return footprints_conformance::LogExtensiveVariant::apply(log, model);
}

std::vector<FootprintsExtensiveResult> check_footprints_trace_extensive(
    const std::vector<Footprint>& log, const Footprint& model,
    FootprintsTraceExtensiveOptions options) {
  return footprints_conformance::TraceExtensiveVariant::apply(log, model,
                                                                options,
                                                                options.case_id_attribute);
}

double fp_precision(const Footprint& log, const Footprint& model) {
  return footprints_conformance::EvaluationUtility::precision(log, model);
}

double fp_fitness(const Footprint& log, const Footprint& model,
                  const FootprintsExtensiveResult& conformance) {
  return footprints_conformance::EvaluationUtility::fitness(log, model,
                                                              conformance);
}

FootprintsFitnessSummary fp_fitness(
    const std::vector<Footprint>& log, const Footprint& model,
    const std::vector<FootprintsExtensiveResult>& conformance) {
  return footprints_conformance::EvaluationUtility::summarize(log, model,
                                                                conformance);
}

}  // namespace minera
