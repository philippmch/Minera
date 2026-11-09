#pragma once

#include "source_read_cancel.hpp"

// Included in the server implementation namespace. The source owns every
// native view; publication is guarded independently of client cancellation.
// Returns the allocator's free pages to the system (glibc keeps them otherwise).
void minera_release_free_memory() {
#if defined(__GLIBC__)
  malloc_trim(0);
#endif
}

// Valid UTF-8 JSON under the shared policy (json_utf8.hpp): WTF-8
// surrogates as \uXXXX escapes, other ill-formed bytes as U+FFFD.
std::string active_source_safe_json(std::string_view input) {
  return minera::detail::repair_json_utf8(input);
}

std::string active_source_quote(std::string_view value) {
  return active_source_safe_json(minera::embedding_runtime::detail::escape_json(value));
}

std::string active_source_shape(minera::EventDataKind kind) {
  switch (kind) {
    case minera::EventDataKind::kEventLog: return "log";
    case minera::EventDataKind::kEventStream: return "stream";
    case minera::EventDataKind::kEventTable: return "dataframe";
    case minera::EventDataKind::kLazyEventTable: return "lazyframe";
  }
  throw std::invalid_argument("invalid native source kind");
}

minera::EventDataKind active_source_kind(std::string_view name) {
  if (name.empty() || name == "log") return minera::EventDataKind::kEventLog;
  if (name == "stream") return minera::EventDataKind::kEventStream;
  if (name == "dataframe") return minera::EventDataKind::kEventTable;
  if (name == "lazyframe") return minera::EventDataKind::kLazyEventTable;
  throw std::invalid_argument("mining_input_shape must be log, stream, dataframe or lazyframe");
}

std::string active_source_field(const httplib::Request& request, std::string_view name) {
  audit_form_field(name);
  const std::string key(name);
  return request.has_file(key) ? form_field(request, key)
      : request.has_param(key) ? request.get_param_value(key) : std::string{};
}

bool active_source_supplied(const httplib::Request& request, std::string_view name) {
  audit_form_field(name);
  return request.has_file(std::string(name)) || request.has_param(std::string(name));
}

template <typename Supplied>
void overlay_active_source_selectors(minera::MineParams& p,
    const minera::ActiveSourceSelectors& s, const Supplied& supplied) {
  const auto set = [&](std::string_view control, std::string& target, const std::string& value) {
    if (!supplied(control)) target = value;
  };
  set("alpha_case_id_attribute", p.alpha.case_id_attribute, s.case_id_key);
  set("alpha_activity_attribute", p.alpha.activity_attribute, s.activity_key);
  set("alpha_timestamp_attribute", p.alpha.timestamp_attribute, s.timestamp_key);
  set("heuristic_case_id_attribute", p.heuristic.case_id_attribute, s.case_id_key);
  set("heuristic_activity_attribute", p.heuristic.activity_attribute, s.activity_key);
  set("heuristic_timestamp_attribute", p.heuristic.timestamp_attribute, s.timestamp_key);
  set("inductive_case_id_key", p.inductive.case_id_key, s.case_id_key);
  set("inductive_activity_key", p.inductive.activity_key, s.activity_key);
  set("inductive_timestamp_key", p.inductive.timestamp_key, s.timestamp_key);
  set("powl_case_id_key", p.powl.case_id_key, s.case_id_key);
  set("powl_activity_key", p.powl.activity_key, s.activity_key);
  set("powl_timestamp_key", p.powl.timestamp_key, s.timestamp_key);
  set("token_replay_activity_attribute", p.token_replay.activity_attribute, s.activity_key);
  set("footprints_case_id_attribute", p.footprints.case_id_attribute, s.case_id_key);
  set("footprints_activity_attribute", p.footprints.activity_attribute, s.activity_key);
  set("footprints_timestamp_attribute", p.footprints.timestamp_attribute, s.timestamp_key);
  set("log_skeleton_case_id_key", p.log_skeleton.case_id_key, s.case_id_key);
  set("log_skeleton_activity_attribute", p.log_skeleton.activity_key, s.activity_key);
  set("declare_case_id_key", p.declare_options.case_id_key, s.case_id_key);
  set("declare_activity_attribute", p.declare_options.activity_key, s.activity_key);
  set("declare_conformance_case_id_key", p.declare_conformance_options.case_id_key, s.case_id_key);
  set("declare_activity_attribute", p.declare_conformance_options.activity_key, s.activity_key);
  set("transition_case_id_attribute", p.transition_system.case_id_key, s.case_id_key);
  set("transition_activity_attribute", p.transition_system.activity_key, s.activity_key);
  set("minimum_self_distance_case_id_key", p.minimum_self_distance.case_id_key, s.case_id_key);
  set("minimum_self_distance_activity_key", p.minimum_self_distance.activity_key, s.activity_key);
  set("correlation_case_id_attribute", p.correlation.case_id_attribute, s.case_id_key);
  set("correlation_activity_attribute", p.correlation.activity_attribute, s.activity_key);
  set("correlation_timestamp_attribute", p.correlation.timestamp_attribute, s.timestamp_key);
  set("spectrum_case_id_key", p.spectrum.case_id_attribute, s.case_id_key);
  set("spectrum_activity_key", p.spectrum.activity_attribute, s.activity_key);
  set("spectrum_timestamp_key", p.spectrum.timestamp_attribute, s.timestamp_key);
  set("batch_case_id_attribute", p.batch_options.case_id_attribute, s.case_id_key);
  set("batch_activity_attribute", p.batch_options.activity_attribute, s.activity_key);
  set("batch_timestamp_attribute", p.batch_options.timestamp_attribute, s.timestamp_key);
  set("batch_resource_attribute", p.batch_options.resource_attribute, s.resource_key);
  set("temporal_case_id_attribute", p.temporal_profile.case_id_attribute, s.case_id_key);
  set("temporal_activity_attribute", p.temporal_profile.activity_attribute, s.activity_key);
  if (!supplied("temporal_source_timestamp_attribute") && !supplied("temporal_source_completion_timestamp_attribute"))
    p.temporal_profile.source_timestamp_attribute = s.timestamp_key;
  if (!supplied("temporal_target_timestamp_attribute") && !supplied("temporal_target_start_timestamp_attribute"))
    p.temporal_profile.target_timestamp_attribute = s.timestamp_key;
  set("dfg_case_id_attribute", p.dfg_table_options.case_id_attribute, s.case_id_key);
  set("dfg_activity_attribute", p.dfg_table_options.activity_attribute, s.activity_key);
  set("dfg_timestamp_attribute", p.dfg_table_options.timestamp_attribute, s.timestamp_key);
}

bool cached_source_uses_bound_defaults(const MinedSession& session,
    bool explicit_flat = false) {
  return session.active_source && (session.active_source_native_dispatch || explicit_flat);
}

void overlay_cached_source_selectors(const httplib::Request& request,
    const MinedSession& session, minera::MineParams& params,
    bool explicit_flat = false) {
  if (cached_source_uses_bound_defaults(session, explicit_flat))
    overlay_active_source_selectors(params, session.active_source->selectors(),
        [&](std::string_view key) {return active_source_supplied(request, key);});
}

void overlay_cached_source_selectors(const httplib::Request& request,
    const MinedSession& session, minera::EvaluationParams& params,
    bool explicit_flat = false) {
  if (!cached_source_uses_bound_defaults(session, explicit_flat)) return;
  const auto& selectors = session.active_source->selectors();
  const auto set = [&](std::string_view name, std::string& activity, std::string& case_id) {
    if (!active_source_supplied(request, std::string(name) + "_activity_attribute")) activity = selectors.activity_key;
    if (!active_source_supplied(request, std::string(name) + "_case_id_attribute")) case_id = selectors.case_id_key;
  };
  set("evaluation", params.replay_fitness_options.activity_attribute, params.replay_fitness_options.case_id_attribute);
  set("evaluation", params.precision_options.activity_attribute, params.precision_options.case_id_attribute);
  set("evaluation", params.generalization_options.activity_attribute, params.generalization_options.case_id_attribute);
  set("evaluation", params.composite_options.activity_attribute, params.composite_options.case_id_attribute);
}

void bind_imported_csv_source_selectors(const minera::CsvEventData& data,
    const minera::CsvImportOptions& options,
    const std::map<std::string, std::string>& literal,
    minera::ActiveSourceSelectors& selectors) {
  // The native importer may resolve its standard aliases while retaining the
  // actual physical headers. Bind defaults to that source, without changing
  // the requested importer controls or any literal selector.
  const auto has_column = [&](std::string_view name) {
    return std::find(data.table.columns.begin(), data.table.columns.end(), name) != data.table.columns.end();
  };
  const minera::Trace* first_trace = data.log.traces.empty() ? nullptr : &data.log.traces.front();
  const minera::Event* first_event = nullptr;
  for (const auto& trace : data.log.traces) {
    if (!trace.events.empty()) { first_event = &trace.events.front(); break; }
  }
  const auto bind = [&](std::string_view key, std::string& selected,
      std::string_view imported, std::initializer_list<std::string_view> aliases) {
    if (literal.contains(std::string(key)) || has_column(selected)) return;
    if (std::find(aliases.begin(), aliases.end(), selected) == aliases.end()) return;
    if (std::find(aliases.begin(), aliases.end(), imported) != aliases.end() && has_column(imported)) {
      selected = imported;
      return;
    }
    // Header-only CSV has no role-bearing events. The same exact-first alias
    // order as the native importer still identifies its selected column.
    for (const auto alias : aliases) {
      if (has_column(alias)) { selected = alias; return; }
    }
  };
  if (options.allow_case_id_aliases)
    bind("case_id_key", selectors.case_id_key,
        first_trace ? first_trace->case_id_source_key : std::string_view{},
        {"case_id", "case:concept:name"});
  bind("activity_key", selectors.activity_key,
      first_event ? first_event->activity_source_key : std::string_view{},
      {"activity", "concept:name"});
  bind("timestamp_key", selectors.timestamp_key,
      first_event ? first_event->timestamp_source_key : std::string_view{},
      {"timestamp", "time:timestamp"});
  bind("resource_key", selectors.resource_key,
      first_event ? first_event->resource_source_key : std::string_view{},
      {"resource", "org:resource"});
  bind("lifecycle_key", selectors.lifecycle_key,
      first_event ? first_event->lifecycle_source_key : std::string_view{},
      {"lifecycle", "lifecycle:transition"});
}

minera::ActiveEventSource prepare_active_event_source(
    const httplib::Request& request, const std::string& bytes,
    const std::string& dataset, minera::MineParams& params,
    std::string* paired_csv_error = nullptr, bool allow_legacy_csv_log = false) {
  auto format = active_source_field(request, "mining_input_format");
  if (format.empty()) format = "auto";
  if (format != "auto" && format != "table")
    throw std::invalid_argument("mining_input_format must be auto or table");
  const auto kind = active_source_kind(active_source_field(request, "mining_input_shape"));
  const bool xes = format == "auto" && looks_like_xes(dataset, bytes);
  minera::ActiveSourceSelectors selectors;
  if (format != "table") {
    if (xes) {
      selectors = {"case:" + params.xes.case_attribute, params.xes.activity_attribute,
          params.xes.timestamp_attribute, params.xes.resource_attribute, params.xes.lifecycle_attribute};
    } else {
      selectors = {params.csv.case_id_column, params.csv.activity_column,
          params.csv.timestamp_column, params.csv.resource_column, params.csv.lifecycle_column};
    }
  }
  const auto literal = active_source_supplied(request, "mining_selectors_json")
      ? minera::parse_workflow_selector_strings_json(active_source_field(request, "mining_selectors_json"))
      : std::map<std::string, std::string>{};
  for (const auto& [key, value] : literal) {
    if (key == "case_id_key") selectors.case_id_key = value;
    else if (key == "activity_key") selectors.activity_key = value;
    else if (key == "timestamp_key") selectors.timestamp_key = value;
    else if (key == "resource_key") selectors.resource_key = value;
    else if (key == "lifecycle_key") selectors.lifecycle_key = value;
    else throw std::invalid_argument("unsupported literal mining source selector: " + key);
  }
  if (format != "table" && !literal.empty()) {
    if (xes) {
      if (literal.contains("case_id_key")) params.xes.case_attribute = selectors.case_id_key.starts_with("case:") ? selectors.case_id_key.substr(5) : selectors.case_id_key;
      if (literal.contains("activity_key")) params.xes.activity_attribute = selectors.activity_key;
      if (literal.contains("timestamp_key")) params.xes.timestamp_attribute = selectors.timestamp_key;
      if (literal.contains("resource_key")) params.xes.resource_attribute = selectors.resource_key;
      if (literal.contains("lifecycle_key")) params.xes.lifecycle_attribute = selectors.lifecycle_key;
    } else {
      if (literal.contains("case_id_key")) params.csv.case_id_column = selectors.case_id_key;
      if (literal.contains("activity_key")) params.csv.activity_column = selectors.activity_key;
      if (literal.contains("timestamp_key")) params.csv.timestamp_column = selectors.timestamp_key;
      if (literal.contains("resource_key")) params.csv.resource_column = selectors.resource_key;
      if (literal.contains("lifecycle_key")) params.csv.lifecycle_column = selectors.lifecycle_key;
    }
  }
  if (format == "table" || kind != minera::EventDataKind::kEventLog || !literal.empty())
    overlay_active_source_selectors(params, selectors,
        [&](std::string_view key) {return active_source_supplied(request, key);});
  const auto import = [&] {
    minera::ActiveSourceFile file{dataset, format == "table" ? "table" : xes ? "xes" : "csv", bytes};
    if (format == "table")
      return minera::ActiveEventSource::from_table(minera::parse_event_table_json(bytes), std::move(file), selectors, kind);
    SourceReadStream input(bytes);
    // The read-cancel hook also reaches the work after the last slice: the
    // XES parse, the CSV importer's grouping and the source build poll it.
    const auto cancelled = source_read_cancel_hook();
    // An XES input follows pm4py's read_xes frame: its log holds the frame's
    // cases (traces that share a case id are one case, a trace without events
    // is no case) and its table the frame's rows in file order.
    if (xes) {
      auto xes_options = params.xes;
      xes_options.cancelled = cancelled;
      return minera::ActiveEventSource::from_xes_frame(minera::import_xes_event_log(input, xes_options),
          std::move(file), selectors, kind, cancelled);
    }
    try {
      auto csv_options = params.csv;
      csv_options.cancelled = cancelled;
      auto data = minera::import_csv_event_data(input, csv_options);
      check_source_read_cancelled();
      bind_imported_csv_source_selectors(data, params.csv, literal, selectors);
      if (kind != minera::EventDataKind::kEventLog || !literal.empty())
        overlay_active_source_selectors(params, selectors,
            [&](std::string_view key) {return active_source_supplied(request, key);});
      return minera::ActiveEventSource::from_csv(std::move(data), std::move(file), selectors, kind, cancelled);
    } catch (const minera::OperationCancelled&) {
      throw;  // a canceled read is not retried as a legacy log
    } catch (const std::exception& error) {
      if (!allow_legacy_csv_log || kind != minera::EventDataKind::kEventLog || !params.correlation_variants.empty()) throw;
      if (paired_csv_error) *paired_csv_error = error.what();
      SourceReadStream legacy(bytes);
      // A valid grouped-log input can have an invalid flat-table schema.
      // Preserve its actual native log instead of silently repairing headers.
      return minera::ActiveEventSource::from_log(minera::import_csv_event_log(legacy, params.csv),
          minera::ActiveSourceFile{dataset, "csv", bytes}, selectors, kind);
    }
  };
  auto source = import();
  if (params.filter.any() || (params.segment_enabled && params.segment) ||
      (params.path_enabled && params.path) || (params.ltl_enabled && params.ltl)) {
    std::vector<std::string> referenced{selectors.case_id_key, selectors.activity_key,
        selectors.timestamp_key, selectors.resource_key, selectors.lifecycle_key,
        params.filter.attribute_key, params.filter.case_attribute_key};
    if (params.filter.numeric_attribute) referenced.push_back(params.filter.numeric_attribute->attribute_key);
    if (params.filter.top_activities) referenced.push_back(params.filter.top_activities->attribute_key);
    if (params.filter.relative_attribute_occurrence) referenced.push_back(params.filter.relative_attribute_occurrence->attribute_key);
    if (params.filter.attribute_occurrence_threshold) referenced.push_back(params.filter.attribute_occurrence_threshold->attribute_key);
    if (params.filter.attribute_value_repetition) referenced.push_back(params.filter.attribute_value_repetition->attribute_key);
    if (params.filter.consecutive_activity_grouping) referenced.push_back(params.filter.consecutive_activity_grouping->activity_key);
    if (params.filter.timestamp_case_grouping) {
      referenced.push_back(params.filter.timestamp_case_grouping->activity_key);
      referenced.push_back(params.filter.timestamp_case_grouping->timestamp_key);
    }
    if (params.path) {referenced.push_back(params.path->activity_attribute); referenced.push_back(params.path->timestamp_attribute);}
    if (params.ltl) {referenced.push_back(params.ltl->attribute_key); referenced.push_back(params.ltl->resource_key);}
    // Conservatively reserve every submitted selector string, including keys
    // used by composed segment/path/LTL filters, outside the provenance field.
    for (const auto& [key, value] : request.files) {referenced.push_back(key); referenced.push_back(value.content);}
    for (const auto& [key, value] : request.params) {referenced.push_back(key); referenced.push_back(value);}
    source = source.with_filters([&](const minera::EventLog& original) {
      auto log = original;
      if (params.filter.any()) log = minera::filter_event_log(log, params.filter);
      if (params.segment_enabled && params.segment) log = minera::filter_segments(log, *params.segment);
      if (params.path_enabled && params.path) log = minera::filter_adjacent_path(log, *params.path);
      if (params.ltl_enabled && params.ltl) log = minera::filter_ltl_event_log(log, *params.ltl);
      return log;
    }, referenced);
  }
  return source;
}

// A summary snapshot leaves the seven complete source-state copies (tables, logs, stream) as null and marks
// itself "detail":"summary"; the source inspector fetches the complete snapshot from /api/active-source.
// Mining responses embed the summary so their size stays linear in the log with a small constant.
std::string active_source_snapshot_json(const MinedSession& session, bool complete = true) {
  if (!session.active_source) throw std::invalid_argument("the cached session has no retained original source");
  const auto& source = *session.active_source;
  const auto& s = source.selectors();
  const auto table = [complete](const minera::EventTable* value) {return complete && value ? minera::serialize_event_table_input_json(*value) : "null";};
  const auto log = [complete](const minera::EventLog* value) {return complete && value ? minera::serialize_active_source_log_json(*value) : "null";};
  // A view equal to an earlier member is written once: later members are
  // {"same_as":"<member>"} references, which the inspector resolves.
  const auto same_as = [](const std::string& value, const std::string& earlier, const char* name) {
    return value != "null" && value == earlier ? "{\"same_as\":\"" + std::string(name) + "\"}" : value;
  };
  const auto input = table(source.original_table_if_available());
  const auto checked = table(source.checked_table_if_available());
  const auto original_log = log(source.original_log_if_available());
  std::string body = "{\"format\":" + active_source_quote(source.file().format == "table" ? "table" : "auto") +
      ",\"detected_format\":" + active_source_quote(source.file().format) +
      ",\"shape\":" + active_source_quote(active_source_shape(source.selected_kind())) +
      ",\"source_id\":" + active_source_quote(std::to_string(session.query_source_id)) +
      ",\"original_dataset\":" + active_source_quote(source.file().name) +
      ",\"original_input_bytes\":" + std::to_string(source.file().bytes.size()) +
      ",\"source_unchanged\":true,\"source_selectors\":{\"case_id_key\":" + active_source_quote(s.case_id_key) +
      ",\"activity_key\":" + active_source_quote(s.activity_key) + ",\"timestamp_key\":" + active_source_quote(s.timestamp_key) +
      ",\"resource_key\":" + active_source_quote(s.resource_key) + ",\"lifecycle_key\":" + active_source_quote(s.lifecycle_key) +
      "},\"input_before\":" + input +
      ",\"input_after\":" + same_as(input, input, "input_before") +
      ",\"checked_table\":" + same_as(checked, input, "input_before") +
      ",\"original_log\":" + original_log +
      ",\"checked_log\":" + same_as(log(source.checked_log_if_available()), original_log, "original_log") +
      ",\"prepared_stream\":" + (complete && source.checked_stream_if_available() ? minera::serialize_active_source_stream_json(*source.checked_stream_if_available()) : "null") +
      ",\"prepared_lazy_table\":" + (source.checked_lazy_if_available() ? same_as(checked, checked, "checked_table") : "null") +
      (complete ? "" : ",\"detail\":\"summary\"") +
      ",\"original_table_origin\":" + active_source_quote(source.original_table_origin()) +
      ",\"paired_csv_import_error\":" + (session.paired_csv_import_error.empty() ? "null" : active_source_quote(session.paired_csv_import_error)) +
      ",\"checked_table_origin\":" + active_source_quote(source.checked_table_origin()) +
      ",\"checked_log_preparation_error\":" + (source.checked_log_preparation_error().empty() ? "null" : active_source_quote(source.checked_log_preparation_error())) +
      ",\"checked_flat_projection_error\":" + (source.checked_flat_projection_error().empty() ? "null" : active_source_quote(source.checked_flat_projection_error())) +
      ",\"source_row_ordinals\":[";
  for (std::size_t i = 0; i < source.source_row_ordinals().size(); ++i) {if (i) body += ','; body += std::to_string(source.source_row_ordinals()[i]);}
  body += "],\"representative_row_ordinals\":[";
  for (std::size_t i = 0; i < source.representative_row_ordinals().size(); ++i) {
    if (i) body += ','; body += '[';
    const auto& rows = source.representative_row_ordinals()[i];
    for (std::size_t j = 0; j < rows.size(); ++j) {if (j) body += ','; body += rows[j] ? std::to_string(*rows[j]) : "null";}
    body += ']';
  }
  body += "],\"projection_changes\":[";
  for (std::size_t i = 0; i < source.projection_changes().size(); ++i) {
    if (i) body += ','; const auto& change = source.projection_changes()[i];
    body += "{\"checked_row\":" + std::to_string(change.checked_row) + ",\"source_row\":" + std::to_string(change.source_row) +
        ",\"column\":" + active_source_quote(change.column) + ",\"removed\":" + (change.removed ? "true" : "false") + '}';
  }
  return active_source_safe_json(body + "]}");
}

minera::LogQueryTable cached_source_query_table(const MinedSession& session) {
  if (!session.active_source) return minera::log_query_table(require_prepared_log(session));
  minera::LogQueryTable result;
  result.table = session.active_source->checked_table();
  result.event_index_column = "__minera_event_index";
  const auto lower = [](std::string text) {for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return text;};
  while (std::any_of(result.table.columns.begin(), result.table.columns.end(), [&](const auto& column) {return lower(column) == lower(result.event_index_column);})) result.event_index_column += '_';
  result.table.columns.push_back(result.event_index_column);
  if (!result.table.dtypes.empty()) result.table.dtypes.push_back("int64");
  if (!result.table.categorical_dtypes.empty()) result.table.categorical_dtypes.push_back(std::nullopt);
  for (std::size_t i = 0; i < result.table.rows.size(); ++i) {
    minera::EventAttribute index;
    index.key = result.event_index_column; index.kind = minera::AttributeKind::kInt;
    index.value = std::to_string(i); index.typed_value = static_cast<std::int64_t>(i);
    result.table.rows[i].push_back(std::move(index));
  }
  result.table.validate();
  return result;
}

// The query table's columns and row count, read from the checked source
// without building the table (log-query/schema needs nothing else).
struct SourceQuerySchema {
  std::vector<std::string> columns;
  std::string event_index_column;
  std::size_t row_count = 0;
};

SourceQuerySchema source_query_schema(const MinedSession& session) {
  SourceQuerySchema result;
  if (!session.active_source) {
    auto table = cached_source_query_table(session);
    result.columns = std::move(table.table.columns);
    result.event_index_column = std::move(table.event_index_column);
    result.row_count = table.table.rows.size();
    return result;
  }
  const auto& table = session.active_source->checked_table();
  result.columns = table.columns;
  result.event_index_column = "__minera_event_index";
  const auto lower = [](std::string text) {for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return text;};
  while (std::any_of(result.columns.begin(), result.columns.end(), [&](const auto& column) {return lower(column) == lower(result.event_index_column);})) result.event_index_column += '_';
  result.columns.push_back(result.event_index_column);
  result.row_count = table.rows.size();
  return result;
}

std::vector<std::vector<std::size_t>> active_source_trace_table_rows(const MinedSession& session) {
  require_prepared_log(session);
  const auto& source = *session.active_source;
  const auto& ordinals = source.source_row_ordinals();
  if (ordinals.size() != source.checked_table().rows.size()) throw std::invalid_argument("checked source row provenance is unavailable");
  std::map<std::size_t, std::vector<std::size_t>> occurrences;
  std::map<std::size_t, std::size_t> used;
  for (std::size_t i = 0; i < ordinals.size(); ++i) occurrences[ordinals[i]].push_back(i);
  std::vector<std::vector<std::size_t>> result;
  std::size_t event_count = 0;
  for (const auto& trace : source.representative_row_ordinals()) {
    std::vector<std::size_t> rows;
    for (const auto& ordinal : trace) {
      if (!ordinal || !occurrences.contains(*ordinal) || used[*ordinal] >= occurrences.at(*ordinal).size())
        throw std::invalid_argument("prepared event provenance is unavailable");
      rows.push_back(occurrences.at(*ordinal).at(used[*ordinal]++)); ++event_count;
    }
    result.push_back(std::move(rows));
  }
  if (event_count != session_log(session).event_count() || event_count != ordinals.size())
    throw std::invalid_argument("prepared event provenance does not match checked source rows");
  return result;
}

std::vector<std::size_t> active_source_expanded_table_indices(
    const MinedSession& session, const std::vector<std::size_t>& indices, bool cases) {
  if (!session.active_source) return indices;
  const auto& source = *session.active_source;
  const auto& table = source.checked_table();
  std::set<std::size_t> selected;
  for (auto i : indices) {if (i >= table.rows.size()) throw std::invalid_argument("selected source row is out of range"); selected.insert(i);}
  if (cases) {
    for (const auto& trace : active_source_trace_table_rows(session)) {
      bool keep = false;
      for (auto row : trace) keep = keep || selected.contains(row);
      if (keep) for (auto row : trace) selected.insert(row);
    }
  }
  return {selected.begin(), selected.end()};
}

std::size_t active_source_selected_row_count(const MinedSession& session,
    const std::vector<std::size_t>& indices, bool cases) {
  return session.active_source ? active_source_expanded_table_indices(session, indices, cases).size()
      : minera::filter_query_events(require_prepared_log(session), indices, cases).event_count();
}

std::string active_source_table_json(const MinedSession& session,
    const std::vector<std::size_t>& indices, bool cases) {
  if (!session.active_source) return minera::serialize_event_table_input_json(minera::event_log_to_table(minera::filter_query_events(require_prepared_log(session), indices, cases)));
  auto table = session.active_source->checked_table();
  const auto selected = active_source_expanded_table_indices(session, indices, cases);
  bool identity = selected.size() == table.rows.size();
  for (std::size_t i = 0; identity && i < selected.size(); ++i) identity = selected[i] == i;
  auto rows = std::move(table.rows); table.rows.clear();
  std::vector<minera::EventTableIndexLabel> labels;
  if (!identity && !table.index) {
    table.index = minera::EventTableIndex{};
    for (std::size_t i = 0; i < rows.size(); ++i) {
      minera::EventAttribute label{"", std::to_string(i), minera::AttributeKind::kInt};
      label.typed_value = static_cast<std::int64_t>(i);
      table.index->labels.push_back({std::move(label)});
    }
  }
  if (!identity && table.index && table.index->kind == "RangeIndex") table.index->kind = "Index";
  if (table.index) {labels = std::move(table.index->labels); table.index->labels.clear();}
  for (auto index : selected) {table.rows.push_back(std::move(rows[index])); if (table.index) table.index->labels.push_back(std::move(labels[index]));}
  return minera::serialize_event_table_input_json(table);
}

minera::EventLog select_active_source_rows(const MinedSession& session,
    const std::vector<std::size_t>& indices, bool cases) {
  if (!session.active_source) return minera::filter_query_events(require_prepared_log(session), indices, cases);
  const auto selected = active_source_expanded_table_indices(session, indices, cases);
  const std::set<std::size_t> selected_rows(selected.begin(), selected.end());
  std::vector<std::size_t> log_indices;
  std::size_t index = 0;
  for (const auto& trace : active_source_trace_table_rows(session)) for (auto row : trace) {
    if (selected_rows.contains(row)) log_indices.push_back(index);
    ++index;
  }
  const auto& log = require_prepared_log(session);
  if (index != log.event_count()) throw std::invalid_argument("prepared event provenance does not match the active log");
  return minera::filter_query_events(log, log_indices, false);
}

std::string active_source_table_json_from_log_indices(const MinedSession& session,
    const std::vector<std::size_t>& indices, bool cases) {
  if (!session.active_source) return active_source_table_json(session, indices, cases);
  std::set<std::size_t> selected(indices.begin(), indices.end());
  std::vector<std::size_t> table_indices;
  std::size_t index = 0;
  for (const auto& trace : active_source_trace_table_rows(session)) for (auto row : trace) {
    if (selected.contains(index)) table_indices.push_back(row);
    ++index;
  }
  if (!selected.empty() && *selected.rbegin() >= index) throw std::invalid_argument("selected log event is out of range");
  return active_source_table_json(session, table_indices, cases);
}

struct ActiveMiningTicket {
  std::uint64_t generation = 0;
  std::string id;
  // Atomic so the running mine can poll it between stages without taking
  // g_session_mutex; it is still written under that mutex.
  std::atomic<bool> canceled = false;
  bool native_source_dispatch = false;
  std::shared_ptr<const MinedSession> previous;
  std::shared_ptr<const ClusteringAnalysis> previous_clustering;
  std::shared_ptr<const MinedSession> published;
  // Set once the rollback session was released: the client confirmed it
  // consumed the published response (an ack, or a later request bound to the
  // new source id) or the rollback window after the response was written
  // expired. Until then a cancel restores the previous session, even when the
  // response bytes already reached the socket.
  bool released = false;
  // Set (under g_session_mutex) once POST /api/mine returned for this
  // request, whatever its answer: a cancel then undoes only a published
  // session it may still roll back.
  bool ended = false;
  // When the identity entered g_active_mining_requests (a sequence number,
  // under g_session_mutex): a full table forgets its oldest finished entry.
  std::uint64_t recorded = 0;
};
// The mine holding the /api/mine slot (see mine_limits.hpp), for its 503.
std::weak_ptr<ActiveMiningTicket> g_running_mine_ticket;  // under g_session_mutex
// The ticket this request thread published, handed to the route wrapper.
thread_local std::shared_ptr<ActiveMiningTicket> t_published_mining_ticket;
std::atomic<std::uint64_t> g_active_mining_generation = 0;
std::map<std::string, std::shared_ptr<ActiveMiningTicket>> g_active_mining_requests;
// The one ticket that may still hold a rollback session (each new mining
// request drops every older rollback).
std::shared_ptr<ActiveMiningTicket> g_rollback_mining_ticket;
// The last ActiveMiningTicket::recorded handed out (under g_session_mutex).
std::uint64_t g_mining_ticket_records = 0;
// The most identities g_active_mining_requests holds.
constexpr std::size_t kMaxMiningRequestIdentities = 2048;

// With g_session_mutex held: makes room in a full g_active_mining_requests
// by forgetting its oldest entry (other than `keep`) that no later request
// can still act on through its identity: a mine that ended and holds no
// session to roll back, or a cancel recorded for an upload that never
// arrived. A mine still running or still able to roll back stays. False
// when no entry may go.
bool evict_mining_ticket_locked(const std::shared_ptr<ActiveMiningTicket>& keep) {
  auto victim = g_active_mining_requests.end();
  for (auto it = g_active_mining_requests.begin(); it != g_active_mining_requests.end(); ++it) {
    const auto& ticket = it->second;
    if (ticket == keep) continue;
    if (ticket->generation != 0 && !ticket->ended) continue;  // running
    if (ticket->published && !ticket->released) continue;     // may still roll back
    if (victim == g_active_mining_requests.end() || ticket->recorded < victim->second->recorded)
      victim = it;
  }
  if (victim == g_active_mining_requests.end()) return false;
  g_active_mining_requests.erase(victim);
  return true;
}

std::shared_ptr<ActiveMiningTicket> begin_active_mining(const httplib::Request& request) {
  auto ticket = std::make_shared<ActiveMiningTicket>();
  ticket->id = active_source_field(request, "mining_request_id");
  if (!ticket->id.empty()) {
    if (ticket->id.size() != 36) throw std::invalid_argument("mining_request_id must be a UUID");
    for (std::size_t i = 0; i < ticket->id.size(); ++i) {
      const bool dash = i == 8 || i == 13 || i == 18 || i == 23;
      if (dash ? ticket->id[i] != '-' : !std::isxdigit(static_cast<unsigned char>(ticket->id[i])))
        throw std::invalid_argument("mining_request_id must be a UUID");
    }
  }
  std::lock_guard<std::mutex> lock(g_session_mutex);
  const auto existing = ticket->id.empty() ? g_active_mining_requests.end() : g_active_mining_requests.find(ticket->id);
  if (existing != g_active_mining_requests.end() && existing->second->generation != 0)
    throw std::invalid_argument("mining request identity was already used");
  if (existing != g_active_mining_requests.end()) ticket->canceled = existing->second->canceled.load();
  ticket->generation = ++g_active_mining_generation;
  g_running_mine_ticket = ticket;
  ticket->previous = g_session; ticket->previous_clustering = g_clustering_analysis;
  for (auto& [id, previous] : g_active_mining_requests) {
    (void)id;
    previous->previous.reset(); previous->previous_clustering.reset(); previous->published.reset();
  }
  if (g_rollback_mining_ticket) {
    g_rollback_mining_ticket->previous.reset(); g_rollback_mining_ticket->previous_clustering.reset();
    g_rollback_mining_ticket->published.reset(); g_rollback_mining_ticket.reset();
  }
  if (!ticket->id.empty()) {
    ticket->recorded = ++g_mining_ticket_records;
    g_active_mining_requests[ticket->id] = ticket;
    while (g_active_mining_requests.size() > kMaxMiningRequestIdentities &&
           evict_mining_ticket_locked(ticket)) {}
  }
  return ticket;
}

// Between mining stages: a canceled request, or one a newer source request
// replaced, stops with the answer publish_active_mining would give it at the
// end, so the remaining stages never run. A request that is neither runs on
// unchanged.
void abort_if_mining_canceled(const std::shared_ptr<ActiveMiningTicket>& ticket) {
  static const auto pause = [] {
    // MINERA_MINE_STAGE_PAUSE_MS (tests only) waits at every stage boundary,
    // so a cancel can be sent while a small log is still being mined.
    const char* text = std::getenv("MINERA_MINE_STAGE_PAUSE_MS");
    char* end = nullptr;
    const long value = text ? std::strtol(text, &end, 10) : 0;
    return text && end != text && *end == '\0' && value > 0 && value <= 60000 ? value : 0L;
  }();
  if (pause > 0) std::this_thread::sleep_for(std::chrono::milliseconds(pause));
  if (ticket->canceled.load() || ticket->generation != g_active_mining_generation.load())
    throw std::invalid_argument("mining request was canceled or replaced by a newer source request");
}

void publish_active_mining(const std::shared_ptr<ActiveMiningTicket>& ticket,
    const std::shared_ptr<MinedSession>& session) {
  std::lock_guard<std::mutex> lock(g_session_mutex);
  if (ticket->canceled || ticket->generation != g_active_mining_generation)
    throw std::invalid_argument("mining request was canceled or replaced by a newer source request");
  session->mining_request_id = ticket->id;
  session->active_source_native_dispatch = ticket->native_source_dispatch;
  g_session = session; g_clustering_analysis.reset(); ticket->published = session;
  g_rollback_mining_ticket = ticket;
  t_published_mining_ticket = ticket;
}

void release_mining_rollback(const std::shared_ptr<ActiveMiningTicket>& ticket) {
  std::shared_ptr<const MinedSession> previous, published;
  std::shared_ptr<const ClusteringAnalysis> previous_clustering;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    if (ticket->canceled || ticket->released) return;
    ticket->released = true;
    previous = std::move(ticket->previous); previous_clustering = std::move(ticket->previous_clustering);
    // A released ticket can never restore, so it must not keep the published
    // session alive either: once a later request (check-model) replaced
    // g_session, this reference would be the only owner of a full session.
    published = std::move(ticket->published);
    if (g_rollback_mining_ticket == ticket) g_rollback_mining_ticket.reset();
  }
  // Destroy the replaced sessions outside the lock.
  const bool sole_owner = published && published.use_count() == 1;
  published.reset();
  if (!previous && !previous_clustering && !sole_owner) return;
  previous.reset(); previous_clustering.reset();
  minera_release_free_memory();
}

// Called with g_session_mutex held when a request other than mining replaces
// g_session (check-model installs an imported model on a copy of `replaced`).
// A ticket that published `replaced` can no longer restore its previous
// session (cancel-mine requires g_session == published), so it gives up the
// published, previous and clustering references; they are moved into
// `garbage` so the caller destroys them after unlocking.
void forget_mining_rollback_locked(const std::shared_ptr<const MinedSession>& replaced,
    std::vector<std::shared_ptr<const void>>& garbage) {
  const auto forget = [&](const std::shared_ptr<ActiveMiningTicket>& ticket) {
    if (!ticket || !replaced || ticket->published != replaced) return;
    garbage.push_back(std::move(ticket->published)); ticket->published.reset();
    garbage.push_back(std::move(ticket->previous)); ticket->previous.reset();
    garbage.push_back(std::move(ticket->previous_clustering)); ticket->previous_clustering.reset();
  };
  for (const auto& [id, ticket] : g_active_mining_requests) {(void)id; forget(ticket);}
  if (g_rollback_mining_ticket && !g_rollback_mining_ticket->published) g_rollback_mining_ticket.reset();
}

// How long a written mining response keeps its rollback session while the
// client has not confirmed it: long enough for a client that fails to read or
// validate the response to cancel, short enough to bound the memory held.
// MINERA_MINING_ROLLBACK_MS overrides it (milliseconds; the contract tests use it).
std::chrono::milliseconds mining_rollback_window() {
  static const auto window = [] {
    std::chrono::milliseconds value(60000);
    if (const char* text = std::getenv("MINERA_MINING_ROLLBACK_MS")) {
      std::uint64_t parsed = 0;
      const auto* end = text + std::strlen(text);
      const auto [at, error] = std::from_chars(text, end, parsed);
      if (error == std::errc() && at == end && *text && parsed <= 86400000) value = std::chrono::milliseconds(parsed);
    }
    return value;
  }();
  return window;
}

// Releases rollback sessions whose window expired. Its state is never
// destroyed, so the detached thread outlives static destruction safely.
struct MiningRollbackReaper {
  std::mutex mutex;
  std::condition_variable wake;
  std::vector<std::pair<std::chrono::steady_clock::time_point, std::weak_ptr<ActiveMiningTicket>>> due;
};

MiningRollbackReaper& mining_rollback_reaper() {
  static MiningRollbackReaper* reaper = [] {
    auto* created = new MiningRollbackReaper;
    std::thread([created] {
      std::unique_lock<std::mutex> lock(created->mutex);
      for (;;) {
        if (created->due.empty()) {created->wake.wait(lock); continue;}
        const auto next = std::min_element(created->due.begin(), created->due.end(),
            [](const auto& a, const auto& b) {return a.first < b.first;})->first;
        if (std::chrono::steady_clock::now() < next) {created->wake.wait_until(lock, next); continue;}
        std::vector<std::shared_ptr<ActiveMiningTicket>> expired;
        // Reserve up front so collecting cannot throw; on allocation failure
        // retry shortly instead of terminating this detached thread.
        try {expired.reserve(created->due.size());}
        catch (...) {created->wake.wait_for(lock, std::chrono::milliseconds(100)); continue;}
        const auto now = std::chrono::steady_clock::now();
        std::erase_if(created->due, [&](const auto& entry) {
          if (entry.first > now) return false;
          if (auto ticket = entry.second.lock()) expired.push_back(std::move(ticket));
          return true;
        });
        lock.unlock();
        for (const auto& ticket : expired) {
          try {release_mining_rollback(ticket);} catch (...) {}
        }
        expired.clear();
        lock.lock();
      }
    }).detach();
    return created;
  }();
  return *reaper;
}

// Never throws: it runs from a content-provider releaser, i.e. inside
// ~Response, where an escaping exception would terminate the server. If the
// rollback cannot be scheduled (allocation or thread-creation failure), it is
// released at once, giving up the cancel window rather than leaking it.
void schedule_mining_rollback_release(const std::shared_ptr<ActiveMiningTicket>& ticket) noexcept {
  try {
    // A request without an identity can never be canceled.
    if (ticket->id.empty() || mining_rollback_window().count() == 0) {release_mining_rollback(ticket); return;}
    auto& reaper = mining_rollback_reaper();
    {
      std::lock_guard<std::mutex> lock(reaper.mutex);
      reaper.due.emplace_back(std::chrono::steady_clock::now() + mining_rollback_window(), ticket);
    }
    reaper.wake.notify_one();
  } catch (...) {
    try {release_mining_rollback(ticket);} catch (...) {}
  }
}

// A request bound to the published source id proves its client consumed the
// mining response, so the rollback session is released.
void confirm_mining_source(const httplib::Request& request) {
  if (!request.has_param("source_id")) return;
  std::shared_ptr<ActiveMiningTicket> ticket;
  {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    if (!g_rollback_mining_ticket || !g_rollback_mining_ticket->published ||
        request.get_param_value("source_id") != std::to_string(g_rollback_mining_ticket->published->query_source_id)) return;
    ticket = g_rollback_mining_ticket;
  }
  release_mining_rollback(ticket);
}

// A mining response is streamed from a shared body so its completion is
// observable: once it was written (in full or not) the rollback window
// starts, and an acknowledgement, a request on the new source or the window's
// end releases the previous session. Within the window cancel-mine restores
// it. The body bytes are unchanged.
#if defined(CPPHTTPLIB_ZLIB_SUPPORT)
// One gzip member of the whole body. The mining body can pass 100 MB, so it
// is deflated at zlib's fastest level: JSON still shrinks about tenfold, in a
// fraction of the time the default level takes.
std::optional<std::string> gzip_mining_body(const std::string& body) {
  z_stream stream{};
  if (deflateInit2(&stream, Z_BEST_SPEED, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
    return std::nullopt;
  std::string out;
  // JSON deflates about tenfold; start at an eighth and grow as needed.
  out.resize(body.size() / 8 + (1u << 16));
  std::size_t offset = 0;
  int status = Z_OK;
  while (status == Z_OK) {
    if (stream.avail_in == 0 && offset < body.size()) {
      const auto slice = std::min<std::size_t>(body.size() - offset, 1u << 30);
      stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(body.data() + offset));
      stream.avail_in = static_cast<uInt>(slice);
      offset += slice;
    }
    if (out.size() - stream.total_out < (1u << 16)) out.resize(out.size() + (out.size() >> 2) + (1u << 16));
    stream.next_out = reinterpret_cast<Bytef*>(out.data() + stream.total_out);
    stream.avail_out = static_cast<uInt>(std::min<std::size_t>(out.size() - stream.total_out, 1u << 30));
    status = deflate(&stream, offset == body.size() && stream.avail_in == 0 ? Z_FINISH : Z_NO_FLUSH);
    if (status == Z_BUF_ERROR) status = Z_OK;  // no progress possible yet: more room or input
  }
  const auto written = stream.total_out;
  deflateEnd(&stream);
  if (status != Z_STREAM_END) return std::nullopt;
  out.resize(written);
  out.shrink_to_fit();
  return out;
}
#endif

void deliver_published_mining(const httplib::Request& request, httplib::Response& response) {
  auto ticket = std::move(t_published_mining_ticket);
  t_published_mining_ticket.reset();
  if (!ticket) return;
  if (response.body.empty()) {schedule_mining_rollback_release(ticket); return;}
  // If wrapping the body fails (allocation), the ticket must still start its
  // rollback window, or the previous session stays pinned until the next mine.
  try {
  const auto type = response.get_header_value("Content-Type");
  auto body = std::make_shared<std::string>(std::move(response.body));
  if (type.starts_with("application/json") && !minera::detail::is_clean_utf8(*body))
    *body = active_source_safe_json(*body);
  response.body.clear();
#if defined(CPPHTTPLIB_ZLIB_SUPPORT)
  // httplib compresses only bodies it holds, not this fixed-length provider,
  // so a client that accepts gzip gets the repaired body gzipped here. A
  // Range request keeps the identity body, so its byte offsets stay those
  // of the JSON (httplib honours Range on GET only, so a POST mine has none).
  if (request.ranges.empty() &&
      httplib::detail::encoding_type(request, response) == httplib::detail::EncodingType::Gzip &&
      httplib::detail::can_compress_content_type(type)) {
    if (auto compressed = gzip_mining_body(*body)) {
      *body = std::move(*compressed);
      response.set_header("Content-Encoding", "gzip");
    }
  }
#else
  (void)request;
#endif
  // set_content_provider adds the Content-Type header again.
  response.headers.erase("Content-Type");
  const auto size = body->size();
  response.set_content_provider(size, type,
      [body](std::size_t offset, std::size_t length, httplib::DataSink& sink) {
        return sink.write(body->data() + offset, std::min(length, body->size() - offset));
      },
      [ticket, body](bool) mutable noexcept {
        body.reset();
        schedule_mining_rollback_release(ticket);  // noexcept
      });
  } catch (...) {
    if (!response.content_provider_resource_releaser_) schedule_mining_rollback_release(ticket);
    throw;
  }
}

// The client applied the mining response: its rollback session is released.
void handle_mine_ack(const httplib::Request& request, httplib::Response& response) {
  try {
    const auto id = request.get_param_value("request_id");
    if (id.size() != 36 || request.params.size() != 1 || !request.body.empty())
      throw std::invalid_argument("mine-ack requires a request UUID and an empty body");
    std::shared_ptr<ActiveMiningTicket> ticket;
    {
      std::lock_guard<std::mutex> lock(g_session_mutex);
      const auto found = g_active_mining_requests.find(id);
      if (found != g_active_mining_requests.end() && !found->second->canceled) ticket = found->second;
    }
    if (ticket) release_mining_rollback(ticket);
    response.set_content("{\"family\":\"ACTIVE_SOURCE_ACK\",\"request_id\":" + active_source_quote(id) + ",\"acknowledged\":" + (ticket ? "true" : "false") + "}", "application/json");
  } catch (const std::exception& error) {response.status = 400; response.set_content(error_json(error.what()), "application/json");}
}

void handle_cancel_mine(const httplib::Request& request, httplib::Response& response) {
  try {
    const auto id = request.get_param_value("request_id");
    if (id.size() != 36 || request.params.size() != 1 || !request.body.empty())
      throw std::invalid_argument("cancel-mine requires a request UUID and an empty body");
    // A rolled-back session is destroyed after the lock is released.
    std::shared_ptr<const MinedSession> replaced;
    std::string held;
    // true only when this cancel stops or undoes something: the first cancel
    // of an id not seen yet (it is recorded, so its upload is refused when it
    // arrives), the first cancel of a mine still running and not replaced,
    // or a rollback of a published session. A request that already failed,
    // ended canceled, was replaced, or was applied for good (acknowledged,
    // used, past its rollback window, or superseded by a newer mine) answers
    // false: nothing changed.
    bool canceled = false;
    {
    std::lock_guard<std::mutex> lock(g_session_mutex);
    // A full table forgets its oldest finished entry, as a new mine does.
    if (!g_active_mining_requests.contains(id) &&
        g_active_mining_requests.size() >= kMaxMiningRequestIdentities &&
        !evict_mining_ticket_locked(nullptr))
      throw std::invalid_argument("too many unknown mining request identities");
    auto& ticket = g_active_mining_requests[id];
    if (!ticket) {
      ticket = std::make_shared<ActiveMiningTicket>();
      ticket->id = id;
      ticket->recorded = ++g_mining_ticket_records;
    }
    const bool already = ticket->canceled.load();
    const bool current = ticket->generation == g_active_mining_generation;
    if (ticket->generation == 0) canceled = !already;  // its POST has not arrived
    else if (!ticket->ended && !ticket->published) canceled = !already && current;  // still running
    ticket->canceled = true;
    if (ticket->published && !ticket->released && g_session == ticket->published && current) {
      canceled = true;
      replaced = std::move(g_session);
      g_session = ticket->previous; g_clustering_analysis = ticket->previous_clustering;
      // The canceled ticket can never restore again, so it holds nothing.
      ticket->published.reset(); ticket->previous.reset(); ticket->previous_clustering.reset();
      if (g_rollback_mining_ticket == ticket) g_rollback_mining_ticket.reset();
    }
    // The source the backend holds once the cancel is applied, so the UI hands
    // back only the source that is really live, whatever the request order.
    held = g_session ? active_source_quote(std::to_string(g_session->query_source_id)) : std::string("null");
    }
    if (replaced) {replaced.reset(); minera_release_free_memory();}
    response.set_content("{\"family\":\"ACTIVE_SOURCE_CANCEL\",\"request_id\":" + active_source_quote(id) + ",\"canceled\":" + (canceled ? "true" : "false") + ",\"active_source_id\":" + held + "}", "application/json");
  } catch (const std::exception& error) {response.status = 400; response.set_content(error_json(error.what()), "application/json");}
}

std::shared_ptr<const MinedSession> active_source_session(const httplib::Request& request) {
  std::lock_guard<std::mutex> lock(g_session_mutex);
  if (!g_session || !g_session->active_source) throw std::invalid_argument("no retained active source; mine an original event dataset first");
  if (!request.has_param("source_id") || request.params.size() != 1 || request.get_param_value("source_id") != std::to_string(g_session->query_source_id))
    throw std::invalid_argument("the active source changed; refresh its descriptor");
  return g_session;
}

void handle_active_source(const httplib::Request& request, httplib::Response& response) {
  try {
    const auto session = active_source_session(request);
    response.set_content("{\"family\":\"ACTIVE_EVENT_SOURCE\",\"source_id\":" + active_source_quote(std::to_string(session->query_source_id)) + ",\"active_source\":" + active_source_snapshot_json(*session) + '}', "application/json");
  } catch (const std::exception& error) {response.status = 400; response.set_content(error_json(error.what()), "application/json");}
}

void handle_active_source_file(const httplib::Request& request, httplib::Response& response) {
  try {
    const auto session = active_source_session(request);
    response.set_content(session->active_source->file().bytes, "application/octet-stream");
  } catch (const std::exception& error) {response.status = 400; response.set_content(error_json(error.what()), "application/json");}
}
