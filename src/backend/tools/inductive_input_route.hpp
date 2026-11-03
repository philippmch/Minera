#pragma once

// Independent uploads and native discovery; the cached mining session is unused.
void handle_inductive_input_discovery(const httplib::Request& request,
                                      httplib::Response& response) {
  try {
    if (!request.is_multipart_form_data() || !request.has_file("input"))
      throw std::invalid_argument("inductive input discovery requires multipart input");
    const auto lookup = [&](std::string_view name) {
      const std::string key(name);
      return request.has_file(key) ? form_field(request, key)
          : request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_file(std::string(name)) || request.has_param(std::string(name));
    };
    const auto shape = present("input_shape") ? lookup("input_shape") : "log";
    const auto requested_format = present("input_format") ? lookup("input_format") : "auto";
    auto format = requested_format;
    if (shape != "log" && shape != "stream" && shape != "dataframe" &&
        shape != "lazyframe" && shape != "dfg" && shape != "variants")
      throw std::invalid_argument("unsupported inductive input_shape");
    if (format != "auto" && format != "table" && format != "dfg_json" && format != "variants_json")
      throw std::invalid_argument("input_format must be auto, table, dfg_json or variants_json");
    if ((shape == "dfg") != (format == "dfg_json") ||
        (shape == "variants") != (format == "variants_json"))
      throw std::invalid_argument("DFG and compact variants require their matching input formats");
    static const std::set<std::string> controls = {
      "input", "input_shape", "input_format", "discovery_selectors_json", "discovery_tree_variant",
      "inductive_variant", "noise_threshold", "inductive_disable_strict_sequence_cut",
      "inductive_disable_fallthroughs", "inductive_multiprocessing", "inductive_sort_events_by_timestamp",
      "inductive_activity_key", "inductive_timestamp_key", "inductive_case_id_key",
      "case_column", "activity_column", "timestamp_column", "resource_column", "lifecycle_column", "delimiter",
      "xes_variant", "xes_case_attribute", "xes_activity_attribute", "xes_timestamp_attribute", "xes_resource_attribute", "xes_lifecycle_attribute",
      "xes_require_timestamp", "xes_timestamp_sort", "xes_reverse_sort", "xes_max_traces", "xes_attributes_to_read", "xes_max_decompressed_bytes"};
    const auto check_control = [&](const std::string& key) {
      if (!controls.contains(key)) throw std::invalid_argument("unknown inductive input control: " + key);
    };
    for (const auto& [key, value] : request.params) {(void)value; check_control(key);}
    for (const auto& [key, value] : request.files) {(void)value; check_control(key);}
    const auto selectors = present("discovery_selectors_json")
        ? minera::parse_workflow_selector_strings_json(lookup("discovery_selectors_json"))
        : std::map<std::string, std::string>{};
    for (const auto& [key, value] : selectors) {
      (void)value;
      if (key != "inductive_activity_key" && key != "inductive_timestamp_key" && key != "inductive_case_id_key")
        throw std::invalid_argument("unsupported literal inductive selector: " + key);
    }
    auto params = minera::parse_mine_params(
        [&](std::string_view key) {return selectors.contains(std::string(key)) ? std::string{} : lookup(key);},
        [&](std::string_view key) {return !selectors.contains(std::string(key)) && present(key);});
    const auto supplied = [&](const std::string& key) {return present(key) || selectors.contains(key);};
    const auto conversion = minera::parse_process_tree_petri_net_variant(
        present("discovery_tree_variant") ? lookup("discovery_tree_variant") : "standard");
    const std::string conversion_name = minera::process_tree_petri_net_variant_name(conversion);
    const auto& file = request.get_file_value("input");
    minera::EventLog source_log;
    std::optional<minera::EventTable> table;
    std::optional<minera::InductiveDfgInput> dfg;
    std::optional<minera::InductiveMinerVariantLog> variants;
    std::optional<std::size_t> original_cases, original_events;
    std::string origin;
    if (format == "dfg_json") {
      dfg = minera::parse_inductive_dfg_input_json(file.content); origin = "uploaded_dfg_constructor";
    } else if (format == "variants_json") {
      variants = minera::parse_inductive_variant_log_json(file.content); origin = "uploaded_weighted_variants";
    } else if (format == "table") {
      table = minera::parse_event_table_json(file.content); origin = "uploaded_typed_table";
      original_events = table->size();
    } else {
      std::istringstream input(file.content);
      if (looks_like_xes(file.filename, file.content)) {
        source_log = minera::import_xes_event_log(input, params.xes); format = "xes"; origin = "uploaded_grouped_log";
      } else {
        auto imported = minera::import_csv_event_data(input, params.csv);
        source_log = std::move(imported.log); table = std::move(imported.table);
        format = "csv"; origin = "uploaded_csv_rows";
        if (!supplied("inductive_activity_key")) params.inductive.activity_key = params.csv.activity_column;
        if (!supplied("inductive_timestamp_key")) params.inductive.timestamp_key = params.csv.timestamp_column;
        if (!supplied("inductive_case_id_key")) params.inductive.case_id_key = params.csv.case_id_column;
      }
      original_cases = source_log.trace_count(); original_events = source_log.event_count();
    }
    for (const auto& [key, value] : selectors) {
      if (key == "inductive_activity_key") params.inductive.activity_key = value;
      else if (key == "inductive_timestamp_key") params.inductive.timestamp_key = value;
      else params.inductive.case_id_key = value;
    }
    if (!table && shape != "dfg" && shape != "variants" && shape != "log")
      table = minera::event_log_to_table(source_log);
    const auto before = table ? minera::serialize_event_table_input_json(*table) : "null";
    const auto variant = params.inductive.variant == minera::InductiveMinerVariant::kLog ? "IM"
        : params.inductive.variant == minera::InductiveMinerVariant::kImf ? "IMF" : "IMD";
    const auto quote = minera::embedding_runtime::detail::escape_json;
    const auto optional_text = [&](const std::string& value) {return value.empty() ? std::string("null") : quote(value);};
    const auto optional_count = [](const std::optional<std::size_t>& value) {return value ? std::to_string(*value) : std::string("null");};
    std::string input_log = "null", input_log_error, prepared_log = "null", prepared_error,
        native_dfg = "null", native_tree = "null", native_net = "null", legacy_result, legacy_error,
        source_xes, source_xes_error, pnml, pnml_error, ptml, ptml_error, bpmn, bpmn_error, conversion_error,
        native_error;
    std::optional<minera::ProcessTree> tree;
    std::optional<minera::PetriNet> net;
    std::optional<minera::DirectlyFollowsGraph> graph;
    std::optional<minera::EventLog> prepared;
    std::optional<std::size_t> prepared_events, prepared_cases;
    bool computed = false;
    try {
      if (dfg) {
        graph = minera::DirectlyFollowsGraph::from_frequencies(dfg->activities, dfg->edges, dfg->start_counts, dfg->end_counts);
        native_dfg = minera::serialize_inductive_native_dfg_json(*graph);
        tree = minera::discover_process_tree(*graph, params.inductive);
      } else if (variants) {
        tree = minera::discover_process_tree(*variants, params.inductive);
      } else {
        std::optional<minera::EventStream> stream;
        std::optional<minera::LazyEventTable> lazy;
        std::optional<minera::EventLog> grouped;
        if (shape == "stream") stream = minera::event_table_to_event_stream(*table);
        if (shape == "lazyframe") lazy = minera::LazyEventTable::from_table(*table);
        if (shape == "log" && requested_format == "table") {
          grouped = graph_alignment_selected_log(*table, params.inductive.case_id_key);
          original_cases = grouped->trace_count();
        }
        const auto data = shape == "log" ? minera::EventDataView(grouped ? *grouped : source_log)
            : shape == "stream" ? minera::EventDataView(*stream)
            : shape == "dataframe" ? minera::EventDataView(*table) : minera::EventDataView(*lazy);
        if (shape == "log") {
          try {input_log = graph_alignment_log_json(grouped ? *grouped : source_log);}
          catch (const std::exception& error) {input_log_error = error.what();}
        } else if (requested_format != "table") {
          try {input_log = graph_alignment_log_json(source_log);}
          catch (const std::exception& error) {input_log_error = error.what();}
        }
        tree = minera::discover_process_tree(data, params.inductive);
        // Reuse the exact native adapter for supplemental preparation; errors
        // here cannot discard a completed discovery or its complete tree.
        try {
          prepared = minera::prepare_inductive_event_log(data, params.inductive);
          prepared_events = prepared->event_count(); prepared_cases = prepared->trace_count();
          try {prepared_log = graph_alignment_log_json(*prepared);}
          catch (const std::exception& error) {prepared_error = error.what();}
        } catch (const std::exception& error) {prepared_error = error.what();}
      }
      computed = true;
      native_tree = minera::serialize_inductive_process_tree_json(*tree);
    } catch (const std::exception& error) {native_error = error.what();}
    if (computed && prepared) {
      try {source_xes = minera::to_xes(*prepared, "Independent Inductive native prepared source");}
      catch (const std::exception& error) {source_xes_error = error.what();}
      try {
        graph = minera::discover_directly_follows_graph(*prepared);
        native_dfg = minera::serialize_inductive_native_dfg_json(*graph);
      } catch (const std::exception& error) {legacy_error = error.what();}
    }
    if (computed) {
      try {net = minera::to_petri_net(*tree, conversion); native_net = minera::workflow_input_petri_json(*net);}
      catch (const std::exception& error) {conversion_error = error.what();}
      try {ptml = minera::to_ptml(*tree, "Independent Inductive discovery");}
      catch (const std::exception& error) {ptml_error = error.what();}
      try {bpmn = minera::to_bpmn(*tree, "Independent Inductive discovery");}
      catch (const std::exception& error) {bpmn_error = error.what();}
      if (net) {
        try {pnml = minera::to_pnml(minera::process_tree_pnml_document(*tree, conversion), "Independent Inductive discovery");}
        catch (const std::exception& error) {pnml_error = error.what();}
      }
      if (prepared && graph) {
        try {
          // The display may intern model labels, so retain native_dfg above
          // before constructing this separate legacy diagram projection.
          auto display_graph = *graph;
          for (const auto& name : tree->activities()) display_graph.ensure_activity(name);
          legacy_result = minera::analysis_json(file.filename, *prepared, display_graph, &*tree,
              nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr, nullptr, net ? &*net : nullptr);
        } catch (const std::exception& error) {legacy_error = error.what();}
      } else if (legacy_error.empty()) {
        legacy_error = "Legacy event-log analysis is unavailable for direct graphs or compact variants; use the complete native model.";
      }
    }
    const auto after = table ? minera::serialize_event_table_input_json(*table) : "null";
    response.status = native_error.empty() ? 200 : 400;
    response.set_content(workflow_input_safe_json("{\"family\":\"INDUCTIVE_INPUT_DISCOVERY\",\"variant\":" + quote(variant) +
        ",\"computed\":" + (computed ? "true" : "false") +
        ",\"error\":" + (native_error.empty() ? "null" : "{\"type\":\"native_error\",\"message\":" + quote(native_error) + "}") +
        ",\"input_context\":{\"adapter\":" + quote(shape) + ",\"format\":" + quote(requested_format) +
        ",\"detected_format\":" + quote(format) + ",\"dataset\":" + quote(file.filename) + ",\"origin\":" + quote(origin) +
        ",\"original_input_bytes\":" + std::to_string(file.content.size()) + ",\"input_event_count\":" + optional_count(original_events) +
        ",\"input_case_count\":" + optional_count(original_cases) +
        ",\"weighted_variant_count\":" + (variants ? std::to_string(variants->size()) : "null") +
        ",\"activity_count\":" + (dfg ? std::to_string(dfg->activities.size()) : "null") + "}" +
        ",\"options_context\":" + minera::serialize_inductive_options_json(params.inductive) +
        ",\"input_before\":" + before + ",\"input_after\":" + after + ",\"input_log\":" + input_log +
        ",\"input_log_error\":" + optional_text(input_log_error) +
        ",\"dfg_input\":" + (dfg ? minera::serialize_inductive_dfg_input_json(*dfg) : "null") +
        ",\"variant_log_input\":" + (variants ? minera::serialize_inductive_variant_log_json(*variants) : "null") +
        ",\"native_dfg\":" + native_dfg + ",\"native_tree\":" + native_tree +
        ",\"discovery_context\":{\"requested_variant\":" + quote(variant) + ",\"effective_variant\":" + quote(dfg ? "IMD" : variant) +
        ",\"direct_graph_semantics\":" + (dfg ? "true" : "false") + ",\"prepared_event_count\":" + optional_count(prepared_events) +
        ",\"prepared_case_count\":" + optional_count(prepared_cases) + "}" +
        ",\"prepared_log\":" + prepared_log + ",\"prepared_log_error\":" + optional_text(prepared_error) +
        ",\"source_xes\":" + optional_text(source_xes) + ",\"source_xes_error\":" + optional_text(source_xes_error) +
        ",\"native_net\":" + native_net + ",\"conversion_context\":{\"variant\":" + quote(conversion_name) +
        ",\"computed\":" + (net ? "true" : "false") + ",\"error\":" + optional_text(conversion_error) + "}" +
        ",\"result\":" + (legacy_result.empty() ? "null" : legacy_result) + ",\"result_error\":" + optional_text(legacy_error) +
        ",\"ptml\":" + optional_text(ptml) + ",\"ptml_error\":" + optional_text(ptml_error) +
        ",\"pnml\":" + optional_text(pnml) + ",\"pnml_error\":" + optional_text(pnml_error) +
        ",\"bpmn\":" + optional_text(bpmn) + ",\"bpmn_error\":" + optional_text(bpmn_error) + '}'), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(workflow_input_safe_json(error_json(error.what())), "application/json");
  }
}
