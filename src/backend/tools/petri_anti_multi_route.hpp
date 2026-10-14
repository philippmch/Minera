#pragma once

bool petri_anti_multi_boolean(std::string value, std::string_view name) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {return static_cast<char>(std::tolower(c));});
  if (value == "true" || value == "1") return true;
  if (value == "false" || value == "0") return false;
  throw std::invalid_argument(std::string(name) + ": expected true, false, 1 or 0");
}

std::string petri_anti_multi_marking_json(const std::optional<std::vector<std::size_t>>& values) {
  if (!values) return "null";
  std::string result = "[";
  for (std::size_t i = 0; i < values->size(); ++i) {if (i) result += ','; result += std::to_string((*values)[i]);}
  return result + ']';
}

// Original sources, PNML documents and marking overrides are scoped to this call.
void handle_petri_anti_multi_workbench(const httplib::Request& request, httplib::Response& response) {
  try {
    if (!request.is_multipart_form_data() || !request.has_file("input_file") || !request.has_file("model_file"))
      throw std::invalid_argument("Petri anti/multi alignment requires multipart input_file and model_file");
    const auto lookup = [&](std::string_view name) {
      const std::string key(name);
      return request.has_file(key) ? form_field(request, key) : request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) {return request.has_file(std::string(name)) || request.has_param(std::string(name));};
    const auto algorithm = present("algorithm") ? lookup("algorithm") : "anti";
    if (algorithm != "anti" && algorithm != "multi") throw std::invalid_argument("algorithm must be anti or multi");
    const auto shape = present("input_shape") ? lookup("input_shape") : "log";
    const auto input_format = present("input_format") ? lookup("input_format") : "auto";
    const auto model_format = present("model_format") ? lookup("model_format") : "auto";
    if (shape != "log" && shape != "trace" && shape != "words") throw std::invalid_argument("input_shape must be log, trace or words");
    if (input_format != "auto" && input_format != "table" && input_format != "trace_words") throw std::invalid_argument("input_format must be auto, table or trace_words");
    if ((shape == "words") != (input_format == "trace_words")) throw std::invalid_argument("words shape requires trace_words format");
    if (model_format != "auto" && model_format != "pnml") throw std::invalid_argument("model_format must be auto or pnml");
    static const std::set<std::string> controls = {
      "input_file", "model_file", "algorithm", "input_shape", "input_format", "model_format", "trace_index",
      "variant", "exponent", "epsilon", "marking_limit", "max_states", "activity_attribute",
      "initial_marking", "final_marking", "initial_marking_json", "final_marking_json",
      "alignment_case_id_attribute", "alignment_selectors_json",
      "pnml_infer_initial_marking", "pnml_infer_final_marking", "pnml_import_stochastic_map", "pnml_require_workflow_boundaries", "pnml_public_marking_semantics",
      "case_column", "activity_column", "timestamp_column", "resource_column", "lifecycle_column", "delimiter",
      "xes_variant", "xes_case_attribute", "xes_activity_attribute", "xes_timestamp_attribute", "xes_resource_attribute", "xes_lifecycle_attribute",
      "xes_require_timestamp", "xes_timestamp_sort", "xes_reverse_sort", "xes_max_traces", "xes_attributes_to_read", "xes_max_decompressed_bytes"};
    const auto check_control = [&](const std::string& key) {
      if (!controls.contains(key) || (algorithm == "multi" && key == "epsilon")) throw std::invalid_argument("unknown Petri " + algorithm + " alignment control: " + key);
    };
    for (const auto& [key, value] : request.params) {(void)value; check_control(key);}
    for (const auto& [key, value] : request.files) {(void)value; check_control(key);}
    const auto selectors = present("alignment_selectors_json") ? minera::parse_workflow_selector_strings_json(lookup("alignment_selectors_json")) : std::map<std::string, std::string>{};
    for (const auto& [key, value] : selectors) {
      (void)value;
      if (key != "alignment_case_id_attribute" && key != "activity_attribute") throw std::invalid_argument("unsupported literal Petri alignment selector: " + key);
    }
    const auto parser_lookup = [&](std::string_view name) {return selectors.contains(std::string(name)) ? std::string{} : lookup(name);};
    const auto parser_presence = [&](std::string_view name) {return !selectors.contains(std::string(name)) && present(name);};
    auto anti_options = algorithm == "anti" ? minera::parse_petri_anti_workbench_params(parser_lookup, parser_presence) : minera::PetriAntiAlignmentOptions{};
    auto multi_options = algorithm == "multi" ? minera::parse_petri_multi_workbench_params(parser_lookup, parser_presence) : minera::PetriMultiAlignmentOptions{};
    auto& activity_selector = algorithm == "anti" ? anti_options.activity_attribute : multi_options.activity_attribute;
    auto& initial = algorithm == "anti" ? anti_options.initial_marking : multi_options.initial_marking;
    auto& final = algorithm == "anti" ? anti_options.final_marking : multi_options.final_marking;
    auto case_selector = present("alignment_case_id_attribute") ? lookup("alignment_case_id_attribute") : std::string("case:concept:name");
    for (const auto& [key, value] : selectors) {if (key == "activity_attribute") activity_selector = value; else case_selector = value;}
    const auto imports = minera::parse_mine_params(parser_lookup, parser_presence);
    minera::PnmlImportOptions pnml_options;
    const auto boolean_option = [&](std::string_view name, bool& value) {if (present(name)) value = petri_anti_multi_boolean(lookup(name), name);};
    boolean_option("pnml_infer_initial_marking", pnml_options.infer_initial_marking);
    boolean_option("pnml_infer_final_marking", pnml_options.infer_final_marking);
    boolean_option("pnml_import_stochastic_map", pnml_options.import_stochastic_map);
    boolean_option("pnml_require_workflow_boundaries", pnml_options.require_workflow_boundaries);
    boolean_option("pnml_public_marking_semantics", pnml_options.public_marking_semantics);
    std::size_t trace_index = 0;
    if (present("trace_index")) {
      if (shape != "trace") throw std::invalid_argument("trace_index requires trace input_shape");
      const auto value = lookup("trace_index"); const auto parsed = std::from_chars(value.data(), value.data() + value.size(), trace_index);
      if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) throw std::invalid_argument("trace_index expects an exact unsigned integer");
    }
    const auto& file = request.get_file_value("input_file");
    const auto& model = request.get_file_value("model_file");
    const auto document = minera::parse_pnml_document(model.content, pnml_options);
    const auto document_json = minera::serialize_petri_workbench_document_json(document);
    const auto native_net = minera::workflow_input_petri_json(document.net);
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
        if (!present("activity_attribute") && !selectors.contains("activity_attribute")) activity_selector = imports.csv.activity_column;
      }
      input_cases = source_log.trace_count(); input_events = source_log.event_count();
    }
    const auto before = table ? minera::serialize_event_table_input_json(*table) : "null";
    const auto quote = minera::embedding_runtime::detail::escape_json;
    const auto optional_text = [&](const std::string& value) {return value.empty() ? std::string("null") : quote(value);};
    const auto optional_count = [](const std::optional<std::size_t>& value) {return value ? std::to_string(*value) : std::string("null");};
    const auto flag = [](bool value) {return value ? "true" : "false";};
    const auto marking_origin = [&](std::string_view name, const std::vector<std::size_t>& value) {
      const auto key = std::string(name);
      return !present(key) && !present(key + "_json") ? std::string("pnml") : value.empty() ? std::string("explicit_empty_native_default") : present(key + "_json") ? std::string("explicit_json") : std::string("explicit_legacy");
    };
    const auto initial_origin = marking_origin("initial_marking", initial), final_origin = marking_origin("final_marking", final);
    std::optional<std::vector<std::size_t>> effective_initial, effective_final;
    std::optional<minera::EventLog> checked;
    std::string input_log = "null", input_log_error, checked_log = "null", checked_log_error, result = "null", native_error,
        source_xes, source_xes_error, pnml, pnml_error, checked_pnml, checked_pnml_error;
    bool executed = false;
    try {
      if (words) source_log = process_tree_alignment_word_log(*words, activity_selector);
      else if (input_format == "table") {source_log = graph_alignment_selected_log(*table, case_selector); input_cases = source_log.trace_count();}
      try {input_log = graph_alignment_log_json(source_log);} catch (const std::exception& error) {input_log_error = error.what();}
      if (shape == "trace") {
        if (trace_index >= source_log.traces.size()) throw std::invalid_argument("trace_index is outside the original grouped input");
        checked.emplace(); checked->attributes = source_log.attributes; checked->metadata = source_log.metadata; checked->traces.push_back(source_log.traces[trace_index]);
      } else checked = source_log;
      checked_cases = checked->trace_count(); checked_events = checked->event_count();
      try {checked_log = graph_alignment_log_json(*checked);} catch (const std::exception& error) {checked_log_error = error.what();}
      if (initial_origin == "pnml") initial = minera::tools::pnml_marking<std::size_t>(document.initial_marking);
      if (final_origin == "pnml") final = minera::tools::pnml_marking<std::size_t>(document.final_marking);
      const auto effective = [&](const std::vector<std::size_t>& value, std::size_t boundary) -> std::optional<std::vector<std::size_t>> {
        if (!value.empty()) return value;
        if (!document.net.place_count() || boundary >= document.net.place_count()) return std::nullopt;
        std::vector<std::size_t> resolved(document.net.place_count(), 0); resolved[boundary] = 1; return resolved;
      };
      effective_initial = effective(initial, document.net.source()); effective_final = effective(final, document.net.sink());
      if (algorithm == "anti") {
        const auto native_result = minera::anti_align_petri(document.net, *checked, anti_options);
        executed = true; result = minera::serialize_petri_anti_workbench_result_json(native_result);
      } else {
        const auto native_result = minera::multi_align_petri(document.net, *checked, multi_options);
        executed = true; result = minera::serialize_petri_multi_workbench_result_json(native_result);
      }
    } catch (const std::exception& error) {native_error = error.what();}
    if (checked) {
      try {source_xes = minera::to_xes(*checked, "Independent Petri anti/multi checked source");} catch (const std::exception& error) {source_xes_error = error.what();}
    }
    try {pnml = minera::to_pnml(document, "Independent original PNML model");} catch (const std::exception& error) {pnml_error = error.what();}
    if (effective_initial && effective_final) {
      try {
        auto selected = document;
        selected.initial_marking.assign(effective_initial->begin(), effective_initial->end()); selected.final_marking.assign(effective_final->begin(), effective_final->end());
        selected.initial_marking_keys.assign(effective_initial->size(), true); selected.final_marking_keys.assign(effective_final->size(), true);
        checked_pnml = minera::to_pnml(selected, "Independent PNML model with resolved checked markings");
      } catch (const std::exception& error) {checked_pnml_error = error.what();}
    } else checked_pnml_error = "Resolved checked markings are unavailable.";
    const auto options_json = algorithm == "anti" ? minera::serialize_petri_anti_workbench_options_json(anti_options) : minera::serialize_petri_multi_workbench_options_json(multi_options);
    const auto after = table ? minera::serialize_event_table_input_json(*table) : "null";
    response.status = native_error.empty() ? 200 : 400;
    response.set_content(workflow_input_safe_json("{\"family\":\"PETRI_ANTI_MULTI_ALIGNMENT_WORKBENCH\",\"algorithm\":" + quote(algorithm) +
        ",\"variant\":\"VERSION_DISCOUNTED_A_STAR\",\"input_shape\":" + quote(shape) + ",\"result_shape\":\"aggregate\",\"executed\":" + flag(executed) +
        ",\"error\":" + (native_error.empty() ? "null" : "{\"type\":\"native_error\",\"message\":" + quote(native_error) + "}") +
        ",\"input_context\":{\"format\":" + quote(input_format) + ",\"detected_format\":" + quote(detected_input) + ",\"dataset\":" + quote(file.filename) +
        ",\"origin\":" + quote(origin) + ",\"original_input_bytes\":" + std::to_string(file.content.size()) + ",\"input_event_count\":" + std::to_string(input_events) +
        ",\"input_case_count\":" + optional_count(input_cases) + ",\"checked_event_count\":" + optional_count(checked_events) + ",\"checked_case_count\":" + optional_count(checked_cases) +
        ",\"trace_index\":" + (shape == "trace" ? std::to_string(trace_index) : "null") + ",\"activity_attribute\":" + quote(activity_selector) + ",\"case_id_attribute\":" + quote(case_selector) + "}" +
        ",\"model_context\":{\"format\":" + quote(model_format) + ",\"detected_format\":\"pnml\",\"dataset\":" + quote(model.filename) + ",\"original_input_bytes\":" + std::to_string(model.content.size()) + ",\"native_document\":" + document_json + "}" +
        ",\"native_net\":" + native_net + ",\"options_context\":" + options_json +
        ",\"pnml_import_options\":{\"infer_initial_marking\":" + flag(pnml_options.infer_initial_marking) + ",\"infer_final_marking\":" + flag(pnml_options.infer_final_marking) +
        ",\"import_stochastic_map\":" + flag(pnml_options.import_stochastic_map) + ",\"require_workflow_boundaries\":" + flag(pnml_options.require_workflow_boundaries) + ",\"public_marking_semantics\":" + flag(pnml_options.public_marking_semantics) + "}" +
        ",\"marking_context\":{\"initial_origin\":" + quote(initial_origin) + ",\"final_origin\":" + quote(final_origin) + ",\"effective_initial\":" + petri_anti_multi_marking_json(effective_initial) + ",\"effective_final\":" + petri_anti_multi_marking_json(effective_final) + "}" +
        ",\"input_before\":" + before + ",\"input_after\":" + after + ",\"input_trace_words\":" + (words ? minera::serialize_alignment_trace_words_json(*words) : "null") +
        ",\"input_log\":" + input_log + ",\"input_log_error\":" + optional_text(input_log_error) + ",\"checked_log\":" + checked_log + ",\"checked_log_error\":" + optional_text(checked_log_error) +
        ",\"result\":" + result + ",\"source_xes\":" + optional_text(source_xes) + ",\"source_xes_error\":" + optional_text(source_xes_error) +
        ",\"pnml\":" + optional_text(pnml) + ",\"pnml_error\":" + optional_text(pnml_error) + ",\"checked_pnml\":" + optional_text(checked_pnml) + ",\"checked_pnml_error\":" + optional_text(checked_pnml_error) + '}'), "application/json");
  } catch (const std::exception& error) {response.status = 400; response.set_content(workflow_input_safe_json(error_json(error.what())), "application/json");}
}
