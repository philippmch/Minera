#pragma once

// Independent models and checked inputs; no cached mining session is accessed.
std::string graph_alignment_log_json(const minera::EventLog& log) {
  minera::LogConversionResult result;
  result.log = log;
  return minera::log_conversion_json(result);
}

minera::EventLog graph_alignment_selected_log(const minera::EventTable& table,
                                            const std::string& selector) {
  minera::LogConversionOptions options;
  options.case_id_key = selector;
  options.include_case_attributes = false;
  const bool physical_column = std::find(table.columns.begin(), table.columns.end(), selector) != table.columns.end();
  auto stream = minera::event_table_to_event_stream(table, options);
  for (auto& record : stream.events) for (auto& field : record.fields) {
    if (field.first_class_role != minera::FirstClassAttributeRole::kCaseId) continue;
    if (field.key == selector || (!physical_column && !selector.empty() && field.first_class_source_key == selector)) continue;
    field.first_class_role = minera::FirstClassAttributeRole::kNone;
    field.first_class_state = minera::FirstClassAttributeState::kMissing;
    field.first_class_source_key.clear();
  }
  return minera::event_stream_to_event_log(stream, options);
}

template <typename Integer>
std::string graph_alignment_integer_array(const std::vector<Integer>& values) {
  std::ostringstream output;
  output << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) output << ',';
    output << values[i];
  }
  output << ']';
  return output.str();
}

void handle_graph_alignment_workbench(const httplib::Request& request,
                                      httplib::Response& response) {
  try {
    if (!request.is_multipart_form_data() || !request.has_file("input_file") || !request.has_file("model_file"))
      throw std::invalid_argument("graph alignment requires multipart input_file and model_file");
    const auto lookup = [&](std::string_view name) {
      const std::string key(name);
      return request.has_file(key) ? form_field(request, key)
          : request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_file(std::string(name)) || request.has_param(std::string(name));
    };
    const auto mode = lookup("mode");
    const bool is_dfg = mode == "DFG_CLASSIC";
    if (!is_dfg && mode != "RECOMPOS_MAXIMAL")
      throw std::invalid_argument("mode must be DFG_CLASSIC or RECOMPOS_MAXIMAL");
    const auto input_format = present("input_format") ? lookup("input_format") : "auto";
    const auto shape = present("input_shape") ? lookup("input_shape") : "log";
    const auto model_format = present("model_format") ? lookup("model_format") : "auto";
    if (input_format != "auto" && input_format != "table" && input_format != "trace_words")
      throw std::invalid_argument("input_format must be auto, table or trace_words");
    if (shape != "log" && shape != "trace" && shape != "words")
      throw std::invalid_argument("input_shape must be log, trace or words");
    if ((shape == "words") != (input_format == "trace_words"))
      throw std::invalid_argument("words shape requires trace_words format");
    if (is_dfg ? model_format != "auto" && model_format != "dfg_json" && model_format != "dfg"
               : model_format != "auto" && model_format != "pnml")
      throw std::invalid_argument("model_format does not belong to the selected alignment mode");
    static const std::set<std::string> common = {
      "input_file", "model_file", "mode", "input_format", "input_shape", "model_format", "trace_index",
      "alignment_selectors_json", "alignment_case_id_attribute", "projection",
      "case_column", "activity_column", "timestamp_column", "resource_column", "lifecycle_column", "delimiter",
      "xes_variant", "xes_case_attribute", "xes_activity_attribute", "xes_timestamp_attribute", "xes_resource_attribute", "xes_lifecycle_attribute",
      "xes_require_timestamp", "xes_timestamp_sort", "xes_reverse_sort", "xes_max_traces", "xes_attributes_to_read", "xes_max_decompressed_bytes"};
    static const std::set<std::string> dfg_controls = {
      "dfg_variant", "variant", "dfg_max_states", "dfg_max_time_ms", "max_visited_states", "max_time_ms",
      "dfg_activity_attribute", "dfg_timestamp_attribute", "dfg_sync_cost", "dfg_sync_costs", "dfg_model_move_cost", "dfg_model_move_costs", "dfg_model_cost",
      "dfg_log_move_cost", "dfg_log_move_costs", "dfg_log_cost", "dfg_internal_log_move_cost", "dfg_internal_log_move_costs", "dfg_internal_cost",
      "sync_cost", "sync_costs", "model_move_cost", "model_costs", "log_move_cost", "log_costs", "internal_log_move_cost", "internal_costs",
      "dfg_sync_cost_json", "dfg_model_move_cost_json", "dfg_log_move_cost_json", "dfg_internal_log_move_cost_json"};
    static const std::set<std::string> decomposed_controls = {
      "variant", "threshold_border_agreement", "max_time_trace", "max_time_total", "max_states", "max_total_states", "use_cache", "clear_cache",
      "log_move_cost", "model_move_cost", "tau_move_cost", "tau_model_cost", "sync_move_cost", "sync_cost", "activity_attribute",
      "activity_log_costs", "activity_model_costs", "activity_sync_costs", "initial_marking", "final_marking",
      "activity_log_costs_json", "activity_model_costs_json", "activity_sync_costs_json", "initial_marking_json", "final_marking_json",
      "pnml_infer_initial_marking", "pnml_infer_final_marking", "pnml_require_workflow_boundaries", "pnml_import_stochastic_map", "pnml_public_marking_semantics"};
    const auto check_control = [&](const std::string& key) {
      if (!common.contains(key) && !(is_dfg ? dfg_controls : decomposed_controls).contains(key))
        throw std::invalid_argument("unknown graph alignment control: " + key);
    };
    for (const auto& [key, value] : request.params) {(void)value; check_control(key);}
    for (const auto& [key, value] : request.files) {(void)value; check_control(key);}
    const auto selectors = present("alignment_selectors_json")
        ? minera::parse_workflow_selector_strings_json(lookup("alignment_selectors_json")) : std::map<std::string, std::string>{};
    for (const auto& [key, value] : selectors) {
      (void)value;
      if (key != "alignment_case_id_attribute" && (is_dfg ? key != "dfg_activity_attribute" && key != "dfg_timestamp_attribute" : key != "activity_attribute"))
        throw std::invalid_argument("unsupported graph alignment literal selector: " + key);
    }
    const auto parser_lookup = [&](std::string_view key) {return selectors.contains(std::string(key)) ? std::string{} : lookup(key);};
    const auto parser_presence = [&](std::string_view key) {return !selectors.contains(std::string(key)) && present(key);};
    const auto imports = minera::parse_mine_params(lookup, present);
    auto dfg_options = is_dfg ? minera::parse_dfg_alignment_params(parser_lookup, parser_presence) : minera::DfgAlignmentOptions{};
    auto decomposed_options = is_dfg ? minera::DecomposedAlignmentOptions{}
        : minera::parse_graph_alignment_decomposed_params(parser_lookup, parser_presence);
    dfg_options.cancelled = decomposed_options.cancelled = connection_cancel_hook(request);
    std::string case_selector = present("alignment_case_id_attribute") ? lookup("alignment_case_id_attribute") : "case:concept:name";
    for (const auto& [key, value] : selectors) {
      if (key == "alignment_case_id_attribute") case_selector = value;
      else if (key == "dfg_activity_attribute") dfg_options.activity_attribute = value;
      else if (key == "dfg_timestamp_attribute") dfg_options.timestamp_attribute = value;
      else decomposed_options.activity_attribute = value;
    }
    const auto projection_request = present("projection") ? lookup("projection") : "none";
    if (projection_request != "none" && projection_request != "log" && projection_request != "metrics" && projection_request != "both")
      throw std::invalid_argument("projection must be none, log, metrics or both");
    if (projection_request != "none" && (!is_dfg || shape != "log"))
      throw std::invalid_argument("DFG projections require grouped log alignment");
    std::size_t trace_index = 0;
    if (present("trace_index")) {
      if (shape != "trace") throw std::invalid_argument("trace_index requires trace input_shape");
      const auto value = lookup("trace_index");
      const auto parsed = std::from_chars(value.data(), value.data() + value.size(), trace_index);
      if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || value.empty())
        throw std::invalid_argument("trace_index expects an exact unsigned integer");
    }
    const auto& file = request.get_file_value("input_file");
    const auto& model = request.get_file_value("model_file");
    minera::EventLog source_log;
    std::optional<minera::EventTable> source_table;
    std::optional<std::vector<std::string>> words;
    std::optional<std::size_t> input_cases, checked_cases, checked_events;
    std::size_t input_events = 0;
    std::string detected_input = input_format, origin;
    if (input_format == "trace_words") {
      words = minera::parse_alignment_trace_words_json(file.content);
      input_events = words->size(); input_cases = 1; origin = "uploaded_trace_words";
    } else if (input_format == "table") {
      source_table = minera::parse_event_table_json(file.content);
      input_events = source_table->size(); origin = "uploaded_typed_table";
    } else {
      std::istringstream input(file.content);
      if (looks_like_xes(file.filename, file.content)) {
        source_log = minera::import_xes_dataframe_log(input, imports.xes);
        detected_input = "xes"; origin = "uploaded_grouped_log";
      } else {
        auto imported = minera::import_csv_event_data(input, imports.csv);
        source_log = std::move(imported.log); source_table = std::move(imported.table);
        detected_input = "csv"; origin = "uploaded_csv_rows";
        if (!present("alignment_case_id_attribute") && !selectors.contains("alignment_case_id_attribute")) case_selector = imports.csv.case_id_column;
      }
      input_cases = source_log.trace_count(); input_events = source_log.event_count();
    }
    const auto input_table = source_table ? minera::serialize_event_table_input_json(*source_table) : "null";
    std::optional<minera::AlphaDfgDiscoveryInput> graph;
    std::optional<minera::PnmlDocument> document;
    std::string detected_model = model_format, canonical_dfg = "null", document_json = "null", native_net = "null";
    std::string model_export, model_export_error;
    if (is_dfg) {
      if (detected_model == "auto") {
        const auto first = model.content.find_first_not_of(" \t\r\n");
        detected_model = first != std::string::npos && model.content[first] == '{' ? "dfg_json" : "dfg";
      }
      if (detected_model == "dfg_json") graph = minera::parse_alpha_dfg_input_json(model.content);
      else {
        const auto parsed = minera::parse_dfg(model.content);
        graph.emplace(); graph->kind = minera::AlphaDfgDiscoveryInput::Kind::kClean;
        graph->dfg = parsed.graph; graph->start_activities = parsed.start_activities; graph->end_activities = parsed.end_activities;
      }
      if (!graph->start_activities || !graph->end_activities)
        throw std::invalid_argument("normative DFG JSON requires explicit start_activities and end_activities maps, including empty maps");
      canonical_dfg = minera::serialize_alpha_dfg_input_json(*graph);
      try {model_export = minera::to_dfg(minera::DfgCleanAnalysis{graph->dfg, *graph->start_activities, *graph->end_activities});}
      catch (const std::exception& error) {model_export_error = error.what();}
    } else {
      detected_model = "pnml";
      const auto flag = [&](const std::string& key) {
        if (!present(key)) return false;
        const auto value = lookup(key);
        if (value == "true" || value == "1") return true;
        if (value == "false" || value == "0") return false;
        throw std::invalid_argument(key + " expects true/false or 1/0");
      };
      minera::PnmlImportOptions options;
      options.infer_initial_marking = flag("pnml_infer_initial_marking");
      options.infer_final_marking = flag("pnml_infer_final_marking");
      options.require_workflow_boundaries = flag("pnml_require_workflow_boundaries");
      options.import_stochastic_map = flag("pnml_import_stochastic_map");
      options.public_marking_semantics = flag("pnml_public_marking_semantics");
      document = minera::parse_pnml_document(model.content, options);
      document_json = minera::pnml_document_json(*document);
      native_net = minera::workflow_input_petri_json(document->net);
    }
    const auto result_shape = is_dfg ? shape == "log" ? "log" : "trace" : shape == "words" ? "trace" : "log";
    bool executed = false;
    std::string result_json = "null", projection_json = "null", projected_log_json = "null", projected_xes, projection_error;
    std::string native_error, input_log = "null", input_log_error, markings_json = "null";
    std::string options_json = is_dfg ? minera::serialize_dfg_alignment_options_json(dfg_options)
        : minera::serialize_decomposed_alignment_options_json(decomposed_options);
    const auto quote = minera::embedding_runtime::detail::escape_json;
    try {
      if (input_format == "table") {
        source_log = graph_alignment_selected_log(*source_table, case_selector);
        input_cases = source_log.trace_count();
      }
      minera::EventLog checked_log = source_log;
      if (shape == "trace") {
        if (trace_index >= source_log.traces.size()) throw std::out_of_range("trace_index is outside the original input log");
        checked_log.traces = {source_log.traces[trace_index]};
      }
      if (words) {checked_cases = 1; checked_events = words->size();}
      else {
        checked_cases = checked_log.trace_count(); checked_events = checked_log.event_count();
        try {input_log = graph_alignment_log_json(source_log);}
        catch (const std::exception& error) {input_log_error = error.what();}
      }
      if (is_dfg) {
        if (shape == "log") {
          const auto result = minera::align_log_on_dfg(checked_log, graph->dfg, *graph->start_activities, *graph->end_activities, dfg_options);
          result_json = minera::serialize_dfg_alignment_log_json(result); executed = true;
          const auto projection_failed = [&](const std::string& message) {if (!projection_error.empty()) projection_error += "; "; projection_error += message;};
          if (projection_request == "metrics" || projection_request == "both") {
            try {const auto projected = minera::project_alignments_on_dfg(checked_log, graph->dfg, *graph->start_activities, *graph->end_activities, result, dfg_options); projection_json = minera::serialize_dfg_alignment_projection_json(projected);}
            catch (const std::exception& error) {projection_failed(std::string("metrics: ") + error.what());}
          }
          if (projection_request == "log" || projection_request == "both") {
            try {const auto projected = minera::project_log_on_dfg(checked_log, graph->dfg, *graph->start_activities, *graph->end_activities, result, dfg_options); projected_log_json = graph_alignment_log_json(projected); projected_xes = minera::to_xes(projected, "Independent DFG alignment projection");}
            catch (const std::exception& error) {projection_failed(std::string("log: ") + error.what());}
          }
        } else {
          const auto result = words ? minera::align_trace_on_dfg(*words, graph->dfg, *graph->start_activities, *graph->end_activities, dfg_options)
              : minera::align_trace_on_dfg(checked_log.traces.front(), graph->dfg, *graph->start_activities, *graph->end_activities, dfg_options);
          result_json = minera::serialize_dfg_alignment_trace_json(result); executed = true;
        }
      } else {
        const bool initial_override = present("initial_marking") || present("initial_marking_json");
        const bool final_override = present("final_marking") || present("final_marking_json");
        if (!initial_override) decomposed_options.initial_marking = minera::tools::pnml_marking<std::size_t>(document->initial_marking);
        if (!final_override) decomposed_options.final_marking = minera::tools::pnml_marking<std::size_t>(document->final_marking);
        options_json = minera::serialize_decomposed_alignment_options_json(decomposed_options);
        auto initial = decomposed_options.initial_marking, final = decomposed_options.final_marking;
        if (initial.empty()) {initial.assign(document->net.place_count(), 0); if (document->net.source() < initial.size()) initial[document->net.source()] = 1;}
        if (final.empty()) {final.assign(document->net.place_count(), 0); if (document->net.sink() < final.size()) final[document->net.sink()] = 1;}
        const auto initial_origin = decomposed_options.initial_marking.empty() ? "native_default" : initial_override ? "override" : "pnml";
        const auto final_origin = decomposed_options.final_marking.empty() ? "native_default" : final_override ? "override" : "pnml";
        markings_json = "{\"initial_origin\":" + quote(initial_origin) + ",\"final_origin\":" + quote(final_origin) +
            ",\"options_initial_marking\":" + graph_alignment_integer_array(decomposed_options.initial_marking) +
            ",\"options_final_marking\":" + graph_alignment_integer_array(decomposed_options.final_marking) +
            ",\"effective_initial_marking\":" + graph_alignment_integer_array(initial) + ",\"effective_final_marking\":" + graph_alignment_integer_array(final) + '}';
        if (words) result_json = minera::serialize_decomposed_alignment_trace_json(minera::align_trace_decomposed(*words, document->net, decomposed_options));
        else result_json = minera::serialize_decomposed_alignment_result_json(minera::align_decomposed(checked_log, document->net, decomposed_options));
        executed = true;
        try {
          auto effective = *document;
          effective.initial_marking.clear(); effective.final_marking.clear();
          for (const auto tokens : initial) effective.initial_marking.push_back(tokens);
          for (const auto tokens : final) effective.final_marking.push_back(tokens);
          if (initial_override) {effective.initial_marking_keys.assign(initial.size(), !decomposed_options.initial_marking.empty()); for (std::size_t i = 0; i < initial.size(); ++i) if (initial[i]) effective.initial_marking_keys[i] = true;}
          if (final_override) {effective.final_marking_keys.assign(final.size(), !decomposed_options.final_marking.empty()); for (std::size_t i = 0; i < final.size(); ++i) if (final[i]) effective.final_marking_keys[i] = true;}
          model_export = minera::to_pnml(effective, "Independent decomposed alignment model");
        } catch (const std::exception& error) {model_export_error = error.what();}
      }
    } catch (const std::exception& error) {native_error = error.what();}
    const auto optional_text = [&](const std::string& value) {return value.empty() ? std::string("null") : quote(value);};
    const auto optional_count = [](const std::optional<std::size_t>& value) {return value ? std::to_string(*value) : std::string("null");};
    response.status = native_error.empty() ? 200 : 400;
    response.set_content(workflow_input_safe_json("{\"family\":\"GRAPH_ALIGNMENT_WORKBENCH\",\"mode\":" + quote(mode) +
        ",\"input_shape\":" + quote(shape) + ",\"result_shape\":" + quote(result_shape) + ",\"executed\":" + (executed ? "true" : "false") +
        ",\"error\":" + (native_error.empty() ? "null" : "{\"type\":\"native_error\",\"message\":" + quote(native_error) + "}") +
        ",\"input_context\":{\"format\":" + quote(input_format) + ",\"detected_format\":" + quote(detected_input) +
        ",\"dataset\":" + quote(file.filename) + ",\"original_input_bytes\":" + std::to_string(file.content.size()) +
        ",\"input_event_count\":" + std::to_string(input_events) + ",\"input_case_count\":" + optional_count(input_cases) +
        ",\"checked_event_count\":" + optional_count(checked_events) + ",\"checked_case_count\":" + optional_count(checked_cases) +
        ",\"origin\":" + quote(origin) + ",\"trace_index\":" + (shape == "trace" ? std::to_string(trace_index) : "null") +
        ",\"activity_attribute\":" + quote(is_dfg ? dfg_options.activity_attribute : decomposed_options.activity_attribute) +
        ",\"timestamp_attribute\":" + (is_dfg ? quote(dfg_options.timestamp_attribute) : "null") + ",\"case_id_attribute\":" + quote(case_selector) + '}' +
        ",\"model_context\":{\"format\":" + quote(model_format) + ",\"detected_format\":" + quote(detected_model) +
        ",\"dataset\":" + quote(model.filename) + ",\"original_input_bytes\":" + std::to_string(model.content.size()) +
        ",\"dfg\":" + canonical_dfg + ",\"document\":" + document_json + ",\"native_net\":" + native_net + ",\"markings\":" + markings_json + '}' +
        ",\"input_table\":" + input_table + ",\"input_trace_words\":" + (words ? minera::serialize_alignment_trace_words_json(*words) : "null") +
        ",\"options_context\":" + options_json + ",\"input_log\":" + input_log + ",\"input_log_error\":" + optional_text(input_log_error) +
        ",\"result\":" + result_json + ",\"projection\":" + projection_json +
        ",\"projected_log\":" + projected_log_json + ",\"projected_xes\":" + optional_text(projected_xes) + ",\"projection_error\":" + optional_text(projection_error) +
        ",\"model_export\":" + optional_text(model_export) + ",\"model_export_format\":" + quote(is_dfg ? "dfg" : "pnml") +
        ",\"model_export_error\":" + optional_text(model_export_error) + '}'), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(workflow_input_safe_json(error_json(error.what())), "application/json");
  }
}
