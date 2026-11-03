#pragma once

minera::EventLog process_tree_alignment_word_log(
    const std::vector<std::string>& words, const std::string& activity_selector) {
  minera::EventLog log;
  minera::Trace trace;
  trace.case_id = "trace_words";
  trace.case_id_state = minera::FirstClassAttributeState::kPresent;
  trace.case_id_source_key = "concept:name";
  for (std::size_t index = 0; index < words.size(); ++index) {
    minera::Event event;
    event.case_id = trace.case_id;
    event.case_id_state = minera::FirstClassAttributeState::kPresent;
    event.case_id_source_key = "case:concept:name";
    event.activity = words[index];
    event.activity_state = minera::FirstClassAttributeState::kPresent;
    event.activity_source_key = activity_selector;
    event.input_order = index;
    trace.events.push_back(std::move(event));
  }
  log.traces.push_back(std::move(trace));
  return log;
}

bool process_tree_alignment_export_indices_safe(const minera::ProcessTree& tree) {
  std::vector<const minera::ProcessTreeNode*> pending{&tree.root()};
  while (!pending.empty()) {
    const auto* node = pending.back(); pending.pop_back();
    if (node->op == minera::ProcessTreeOperator::kActivity && node->activity >= tree.activities().size()) return false;
    for (const auto& child : node->children) pending.push_back(&child);
  }
  return true;
}

std::string process_tree_alignment_complex_export_error(const minera::ProcessTree& tree) {
  std::vector<const minera::ProcessTreeNode*> pending{&tree.root()};
  while (!pending.empty()) {
    const auto* node = pending.back(); pending.pop_back();
    if ((node->op == minera::ProcessTreeOperator::kInclusiveChoice && node->children.size() > 12) ||
        (node->op == minera::ProcessTreeOperator::kInterleaving && node->children.size() > 8))
      return "Petri-net and BPMN exports are unavailable because the model exceeds the native alignment expansion limit.";
    for (const auto& child : node->children) pending.push_back(&child);
  }
  return {};
}

// Standalone checked logs and uploaded original models never access g_session.
void handle_process_tree_alignment_workbench(const httplib::Request& request,
                                             httplib::Response& response) {
  try {
    if (!request.is_multipart_form_data() || !request.has_file("input_file") || !request.has_file("model_file"))
      throw std::invalid_argument("process-tree alignment requires multipart input_file and model_file");
    const auto lookup = [&](std::string_view name) {
      const std::string key(name);
      return request.has_file(key) ? form_field(request, key)
          : request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) {return request.has_file(std::string(name)) || request.has_param(std::string(name));};
    const auto shape = present("input_shape") ? lookup("input_shape") : "log";
    const auto input_format = present("input_format") ? lookup("input_format") : "auto";
    const auto model_format = present("model_format") ? lookup("model_format") : "auto";
    if (shape != "log" && shape != "trace" && shape != "words") throw std::invalid_argument("input_shape must be log, trace or words");
    if (input_format != "auto" && input_format != "table" && input_format != "trace_words") throw std::invalid_argument("input_format must be auto, table or trace_words");
    if ((shape == "words") != (input_format == "trace_words")) throw std::invalid_argument("words shape requires trace_words format");
    if (model_format != "auto" && model_format != "ptml" && model_format != "tree_json") throw std::invalid_argument("model_format must be auto, ptml or tree_json");
    static const std::set<std::string> controls = {
      "input_file", "model_file", "input_shape", "input_format", "model_format", "trace_index",
      "alignment_case_id_attribute", "alignment_selectors_json", "discovery_tree_variant",
      "pt_alignment_variant", "pt_alignment_max_states", "pt_alignment_max_trace_length", "pt_alignment_max_process_tree_height",
      "pt_alignment_max_time_ms", "pt_alignment_max_total_time_ms", "pt_alignment_max_trace_time_ms", "pt_alignment_max_per_trace_time_ms",
      "pt_alignment_use_global_cache", "pt_alignment_batch_size", "pt_alignment_clear_cache", "pt_alignment_activity_attribute",
      "case_column", "activity_column", "timestamp_column", "resource_column", "lifecycle_column", "delimiter",
      "xes_variant", "xes_case_attribute", "xes_activity_attribute", "xes_timestamp_attribute", "xes_resource_attribute", "xes_lifecycle_attribute",
      "xes_require_timestamp", "xes_timestamp_sort", "xes_reverse_sort", "xes_max_traces", "xes_attributes_to_read", "xes_max_decompressed_bytes"};
    const auto check_control = [&](const std::string& key) {if (!controls.contains(key)) throw std::invalid_argument("unknown process-tree alignment control: " + key);};
    for (const auto& [key, value] : request.params) {(void)value; check_control(key);}
    for (const auto& [key, value] : request.files) {(void)value; check_control(key);}
    const auto selectors = present("alignment_selectors_json")
        ? minera::parse_workflow_selector_strings_json(lookup("alignment_selectors_json")) : std::map<std::string, std::string>{};
    for (const auto& [key, value] : selectors) {
      (void)value;
      if (key != "alignment_case_id_attribute" && key != "pt_alignment_activity_attribute") throw std::invalid_argument("unsupported literal process-tree alignment selector: " + key);
    }
    const auto parser_lookup = [&](std::string_view key) {return selectors.contains(std::string(key)) ? std::string{} : lookup(key);};
    const auto parser_presence = [&](std::string_view key) {return !selectors.contains(std::string(key)) && present(key);};
    auto options = minera::parse_process_tree_alignment_params(parser_lookup, parser_presence);
    const auto imports = minera::parse_mine_params(parser_lookup, parser_presence);
    auto case_selector = present("alignment_case_id_attribute") ? lookup("alignment_case_id_attribute") : std::string("case:concept:name");
    for (const auto& [key, value] : selectors) {
      if (key == "alignment_case_id_attribute") case_selector = value;
      else options.activity_attribute = value;
    }
    const auto conversion = minera::parse_process_tree_petri_net_variant(present("discovery_tree_variant") ? lookup("discovery_tree_variant") : "standard");
    const std::string conversion_name = minera::process_tree_petri_net_variant_name(conversion);
    std::size_t trace_index = 0;
    if (present("trace_index")) {
      if (shape != "trace") throw std::invalid_argument("trace_index requires trace input_shape");
      const auto value = lookup("trace_index");
      const auto parsed = std::from_chars(value.data(), value.data() + value.size(), trace_index);
      if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) throw std::invalid_argument("trace_index expects an exact unsigned integer");
    }
    const auto& file = request.get_file_value("input_file");
    const auto& model = request.get_file_value("model_file");
    minera::EventLog source_log;
    std::optional<minera::EventTable> table;
    std::optional<std::vector<std::string>> words;
    std::optional<std::size_t> input_cases, checked_cases, checked_events;
    std::size_t input_events = 0;
    std::string detected_input = input_format, origin;
    if (input_format == "trace_words") {
      words = minera::parse_alignment_trace_words_json(file.content);
      input_events = words->size(); input_cases = 1; origin = "uploaded_words_converted_to_present_event_log";
    } else if (input_format == "table") {
      table = minera::parse_event_table_json(file.content); input_events = table->size(); origin = "uploaded_table_converted_to_event_log";
    } else {
      std::istringstream input(file.content);
      if (looks_like_xes(file.filename, file.content)) {
        source_log = minera::import_xes_event_log(input, imports.xes); detected_input = "xes"; origin = "uploaded_grouped_log";
      } else {
        auto imported = minera::import_csv_event_data(input, imports.csv);
        source_log = std::move(imported.log); table = std::move(imported.table); detected_input = "csv"; origin = "uploaded_csv_grouped_log";
        if (!present("alignment_case_id_attribute") && !selectors.contains("alignment_case_id_attribute")) case_selector = imports.csv.case_id_column;
        if (!present("pt_alignment_activity_attribute") && !selectors.contains("pt_alignment_activity_attribute")) options.activity_attribute = imports.csv.activity_column;
      }
      input_cases = source_log.trace_count(); input_events = source_log.event_count();
    }
    auto detected_model = model_format;
    if (detected_model == "auto") {
      const auto first = model.content.find_first_not_of(" \t\r\n");
      detected_model = first != std::string::npos && model.content[first] == '{' ? "tree_json" : "ptml";
    }
    const auto tree = detected_model == "tree_json" ? minera::parse_process_tree_alignment_input_json(model.content) : minera::parse_ptml(model.content);
    const auto tree_json = minera::serialize_process_tree_alignment_input_json(tree);
    const auto before = table ? minera::serialize_event_table_input_json(*table) : "null";
    const auto quote = minera::embedding_runtime::detail::escape_json;
    const auto optional_text = [&](const std::string& value) {return value.empty() ? std::string("null") : quote(value);};
    const auto optional_count = [](const std::optional<std::size_t>& value) {return value ? std::to_string(*value) : std::string("null");};
    const auto variant = options.variant == minera::ProcessTreeAlignmentVariant::kSearchGraphPt ? "SEARCH_GRAPH_PT"
        : options.variant == minera::ProcessTreeAlignmentVariant::kDynamicProgramming ? "DYNAMIC_PROGRAMMING"
        : options.variant == minera::ProcessTreeAlignmentVariant::kMilp ? "MILP" : "APPROXIMATED_ORIGINAL";
    std::string input_log = "null", input_log_error, checked_log = "null", checked_log_error,
        result_json = "null", native_error, source_xes, source_xes_error, native_net = "null",
        conversion_error, ptml, ptml_error, pnml, pnml_error, bpmn, bpmn_error;
    std::optional<minera::EventLog> checked;
    std::optional<minera::PetriNet> net;
    bool executed = false;
    try {
      if (words) source_log = process_tree_alignment_word_log(*words, options.activity_attribute);
      else if (input_format == "table") {
        source_log = graph_alignment_selected_log(*table, case_selector); input_cases = source_log.trace_count();
      }
      try {input_log = graph_alignment_log_json(source_log);}
      catch (const std::exception& error) {input_log_error = error.what();}
      if (shape == "trace") {
        if (trace_index >= source_log.traces.size()) throw std::invalid_argument("trace_index is outside the original grouped input");
        checked.emplace(); checked->attributes = source_log.attributes; checked->metadata = source_log.metadata;
        checked->traces.push_back(source_log.traces[trace_index]);
      } else checked = source_log;
      checked_cases = checked->trace_count(); checked_events = checked->event_count();
      try {checked_log = graph_alignment_log_json(*checked);}
      catch (const std::exception& error) {checked_log_error = error.what();}
      const auto result = minera::align_process_tree(*checked, tree, options);
      executed = true;
      result_json = minera::serialize_process_tree_alignment_result_json(result);
    } catch (const std::exception& error) {native_error = error.what();}
    if (checked) {
      try {source_xes = minera::to_xes(*checked, "Independent process-tree alignment checked source");}
      catch (const std::exception& error) {source_xes_error = error.what();}
    }
    // The public constructor is intentionally unvalidated. Existing model
    // exporters can dereference leaf indices. Large rejected expansion models
    // can also spend unbounded time reducing a supplementary Petri net. Keep
    // these export failures separate from native alignment and the full model.
    if (!process_tree_alignment_export_indices_safe(tree)) {
      conversion_error = ptml_error = pnml_error = bpmn_error = "Model export is unavailable because an activity leaf index is outside the native activity registry.";
    } else {
      try {ptml = minera::to_ptml(tree, "Independent process-tree alignment model");}
      catch (const std::exception& error) {ptml_error = error.what();}
      const auto export_limit_error = process_tree_alignment_complex_export_error(tree);
      if (!export_limit_error.empty()) {
        conversion_error = pnml_error = bpmn_error = export_limit_error;
      } else {
        try {net = minera::to_petri_net(tree, conversion); native_net = minera::workflow_input_petri_json(*net);}
        catch (const std::exception& error) {conversion_error = error.what();}
        try {bpmn = minera::to_bpmn(tree, "Independent process-tree alignment model");}
        catch (const std::exception& error) {bpmn_error = error.what();}
      }
      if (net) {
        try {pnml = minera::to_pnml(minera::process_tree_pnml_document(tree, conversion), "Independent process-tree alignment model");}
        catch (const std::exception& error) {pnml_error = error.what();}
      }
    }
    const auto after = table ? minera::serialize_event_table_input_json(*table) : "null";
    response.status = native_error.empty() ? 200 : 400;
    response.set_content(workflow_input_safe_json("{\"family\":\"PROCESS_TREE_ALIGNMENT_WORKBENCH\",\"variant\":" + quote(variant) +
        ",\"input_shape\":" + quote(shape) + ",\"result_shape\":\"log\",\"executed\":" + (executed ? "true" : "false") +
        ",\"error\":" + (native_error.empty() ? "null" : "{\"type\":\"native_error\",\"message\":" + quote(native_error) + "}") +
        ",\"input_context\":{\"format\":" + quote(input_format) + ",\"detected_format\":" + quote(detected_input) +
        ",\"dataset\":" + quote(file.filename) + ",\"origin\":" + quote(origin) + ",\"original_input_bytes\":" + std::to_string(file.content.size()) +
        ",\"input_event_count\":" + std::to_string(input_events) + ",\"input_case_count\":" + optional_count(input_cases) +
        ",\"checked_event_count\":" + optional_count(checked_events) + ",\"checked_case_count\":" + optional_count(checked_cases) +
        ",\"trace_index\":" + (shape == "trace" ? std::to_string(trace_index) : "null") +
        ",\"activity_attribute\":" + quote(options.activity_attribute) + ",\"case_id_attribute\":" + quote(case_selector) + "}" +
        ",\"model_context\":{\"format\":" + quote(model_format) + ",\"detected_format\":" + quote(detected_model) +
        ",\"dataset\":" + quote(model.filename) + ",\"original_input_bytes\":" + std::to_string(model.content.size()) + ",\"native_tree\":" + tree_json + "}" +
        ",\"options_context\":" + minera::serialize_process_tree_alignment_options_json(options) +
        ",\"input_before\":" + before + ",\"input_after\":" + after +
        ",\"input_trace_words\":" + (words ? minera::serialize_alignment_trace_words_json(*words) : "null") +
        ",\"input_log\":" + input_log + ",\"input_log_error\":" + optional_text(input_log_error) +
        ",\"checked_log\":" + checked_log + ",\"checked_log_error\":" + optional_text(checked_log_error) +
        ",\"result\":" + result_json + ",\"native_net\":" + native_net +
        ",\"conversion_context\":{\"variant\":" + quote(conversion_name) + ",\"computed\":" + (net ? "true" : "false") + ",\"error\":" + optional_text(conversion_error) + "}" +
        ",\"source_xes\":" + optional_text(source_xes) + ",\"source_xes_error\":" + optional_text(source_xes_error) +
        ",\"ptml\":" + optional_text(ptml) + ",\"ptml_error\":" + optional_text(ptml_error) +
        ",\"pnml\":" + optional_text(pnml) + ",\"pnml_error\":" + optional_text(pnml_error) +
        ",\"bpmn\":" + optional_text(bpmn) + ",\"bpmn_error\":" + optional_text(bpmn_error) + '}'), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(workflow_input_safe_json(error_json(error.what())), "application/json");
  }
}
