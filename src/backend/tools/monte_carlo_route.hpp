#pragma once

// Independent original-model requests never read or publish the mined session.
void handle_monte_carlo_workbench(const httplib::Request& request,
                                  httplib::Response& response) {
  try {
    if (!request.is_multipart_form_data() || !request.has_file("model_file"))
      throw std::invalid_argument("Monte Carlo requires multipart model_file");
    const auto supplied = [&](std::string_view name) {return active_source_supplied(request, name);};
    const auto field = [&](std::string_view name) {return active_source_field(request, name);};
    const auto operation = supplied("operation") ? field("operation") : "simulate_net";
    const bool log_mode = operation != "simulate_net";
    if (operation != "simulate_net" && operation != "simulate_log" &&
        operation != "fit_options" && operation != "fit_then_simulate")
      throw std::invalid_argument("unsupported Monte Carlo operation: " + operation);
    static const std::set<std::string> pnml_controls = {
      "pnml_infer_initial_marking", "pnml_infer_final_marking", "pnml_import_stochastic_map",
      "pnml_require_workflow_boundaries", "pnml_public_marking_semantics"};
    static const std::set<std::string> source_controls = {
      "log", "mining_input_format", "mining_input_shape", "mining_selectors_json",
      "case_column", "activity_column", "timestamp_column", "resource_column", "lifecycle_column", "delimiter",
      "xes_variant", "xes_case_attribute", "xes_activity_attribute", "xes_timestamp_attribute",
      "xes_resource_attribute", "xes_lifecycle_attribute", "xes_require_timestamp", "xes_timestamp_sort",
      "xes_reverse_sort", "xes_max_traces", "xes_attributes_to_read", "xes_max_decompressed_bytes"};
    const auto check = [&](const std::string& key) {
      if (request.params.count(key) + request.files.count(key) != 1)
        throw std::invalid_argument("duplicate Monte Carlo control: " + key);
      if (key == "model_file" || key == "operation" || key == "options_json" || pnml_controls.contains(key)) return;
      if (log_mode && source_controls.contains(key)) return;
      throw std::invalid_argument("unknown or inapplicable Monte Carlo control: " + key);
    };
    for (const auto& [key, value] : request.params) {(void)value; check(key);}
    for (const auto& [key, value] : request.files) {(void)value; check(key);}
    if (log_mode && !request.has_file("log"))
      throw std::invalid_argument("this Monte Carlo operation requires an original log file");
    if (supplied("mining_input_shape") && field("mining_input_shape") != "log")
      throw std::invalid_argument("Monte Carlo fitting requires mining_input_shape=log");
    std::set<std::string> members;
    const auto submitted = minera::parse_monte_carlo_workbench_options(
        supplied("options_json") ? field("options_json") : "{}", &members);
    auto configured = submitted;
    minera::PnmlImportOptions imports;
    const auto boolean_option = [&](std::string_view name, bool& value) {
      if (supplied(name)) value = petri_anti_multi_boolean(field(name), name);
    };
    boolean_option("pnml_infer_initial_marking", imports.infer_initial_marking);
    boolean_option("pnml_infer_final_marking", imports.infer_final_marking);
    boolean_option("pnml_import_stochastic_map", imports.import_stochastic_map);
    boolean_option("pnml_require_workflow_boundaries", imports.require_workflow_boundaries);
    boolean_option("pnml_public_marking_semantics", imports.public_marking_semantics);
    const auto& file = request.get_file_value("model_file");
    const auto document = minera::parse_pnml_document(file.content, imports);
    const auto before = minera::serialize_petri_workbench_document_json(document);
    const auto quote = active_source_quote;
    const auto flag = [](bool value) {return value ? "true" : "false";};
    const auto nullable = [&](const std::string& value) {return value.empty() ? std::string("null") : quote(value);};
    const auto error_json_value = [&](std::string_view type, std::string_view message) {
      return "{\"type\":" + quote(type) + ",\"message\":" + quote(message) + '}';
    };
    std::optional<minera::ActiveEventSource> source;
    const auto source_snapshot = [&]() {
      if (!source) return std::string("null");
      const auto& s = source->selectors();
      const auto table = [](const minera::EventTable* value) {
        return value ? minera::serialize_event_table_input_json(*value) : std::string("null");
      };
      const auto log = [](const minera::EventLog* value) {
        return value ? minera::serialize_active_source_log_json(*value) : std::string("null");
      };
      // Compressed XES is an original binary upload. Only Unicode/WTF-8
      // text may be embedded as JSON text; hex retains all other raw bytes.
      const auto text_bytes = [](std::string_view bytes) {
        for (std::size_t i = 0; i < bytes.size();) {
          const auto first = static_cast<unsigned char>(bytes[i++]);
          if (first < 0x80) continue;
          const auto count = first >= 0xc2 && first <= 0xdf ? 1 : first >= 0xe0 && first <= 0xef ? 2 : first >= 0xf0 && first <= 0xf4 ? 3 : -1;
          if (count < 0 || i + count > bytes.size()) return false;
          unsigned code = first & (count == 1 ? 31 : count == 2 ? 15 : 7);
          for (int j = 0; j < count; ++j) {
            const auto next = static_cast<unsigned char>(bytes[i++]);
            if ((next & 0xc0) != 0x80) return false;
            code = (code << 6) | (next & 63);
          }
          if ((count == 1 && code < 0x80) || (count == 2 && code < 0x800) || (count == 3 && code < 0x10000) || code > 0x10ffff) return false;
        }
        return true;
      };
      auto original = source->file().bytes;
      const bool textual = text_bytes(original);
      if (!textual) {
        constexpr char hex[] = "0123456789abcdef";
        std::string encoded;
        encoded.reserve(original.size() * 2);
        for (const unsigned char byte : original) {encoded += hex[byte >> 4]; encoded += hex[byte & 15];}
        original = std::move(encoded);
      }
      return "{\"dataset\":" + quote(source->file().name) + ",\"format\":" + quote(source->file().format == "table" ? "table" : "auto") +
        ",\"detected_format\":" + quote(source->file().format) +
        ",\"original_input_encoding\":" + quote(textual ? "utf8" : "hex") +
        ",\"original_input\":" + quote(original) + ",\"original_input_bytes\":" + std::to_string(source->file().bytes.size()) +
        ",\"source_selectors\":{\"case_id_key\":" + quote(s.case_id_key) + ",\"activity_key\":" + quote(s.activity_key) +
        ",\"timestamp_key\":" + quote(s.timestamp_key) + ",\"resource_key\":" + quote(s.resource_key) +
        ",\"lifecycle_key\":" + quote(s.lifecycle_key) + "},\"input_before\":" + table(source->original_table_if_available()) +
        ",\"input_after\":" + table(source->original_table_if_available()) + ",\"checked_table\":" + table(source->checked_table_if_available()) +
        ",\"original_log\":" + log(source->original_log_if_available()) + ",\"prepared_log\":" + log(source->checked_log_if_available()) +
        ",\"log_preparation_error\":" + nullable(source->checked_log_preparation_error()) +
        ",\"flat_projection_error\":" + nullable(source->checked_flat_projection_error()) + '}';
    };
    const auto initial_origin = members.contains("initial_marking") ? "explicit" : "pnml";
    const auto final_origin = members.contains("final_marking") ? "explicit" : "pnml";
    const auto map_origin = members.contains("stochastic_map") ? "explicit" : document.stochastic_map ? "pnml" : "native_default";
    std::string failure = "null", source_before = "null", calls = "[";
    std::optional<minera::MonteCarloOptions> fitted;
    std::optional<minera::MonteCarloResult> result;
    bool executed = false;
    std::string stage = "configuration_error";
    const auto invoke = [&](std::string_view name, const auto& call) {
      if (calls.size() > 1) calls += ',';
      const auto prefix = "{\"name\":" + quote(name) + ",\"returned\":";
      try {call(); calls += prefix + "true,\"error\":null}";}
      catch (const std::exception& error) {
        calls += prefix + "false,\"error\":" + error_json_value("native_error", error.what()) + '}';
        throw;
      }
    };
    try {
      // Omission alone enables document defaults. Explicit empty vectors/null
      // remain native requests and are never replaced with imported values.
      if (!members.contains("initial_marking")) {
        configured.initial_marking = minera::tools::pnml_marking<std::uint32_t>(document.initial_marking);
        if (!members.contains("initial_marking_keys")) configured.initial_marking_keys = document.initial_marking_keys;
      }
      if (!members.contains("final_marking")) {
        configured.final_marking = minera::tools::pnml_marking<std::uint32_t>(document.final_marking);
        if (!members.contains("final_marking_keys")) configured.final_marking_keys = document.final_marking_keys;
      }
      if (!members.contains("stochastic_map")) configured.stochastic_map = document.stochastic_map;
      if (!members.contains("place_names")) configured.place_names = document.place_ids;
      if (!members.contains("transition_names")) configured.transition_names = document.transition_ids;
      if (log_mode) {
        stage = "preparation_error";
        auto params = minera::parse_mine_params(field, supplied);
        const auto& log_file = request.get_file_value("log");
        source = prepare_active_event_source(request, log_file.content, log_file.filename, params);
        source_before = source_snapshot();
        (void)source->checked_log();
      }
      stage = "native_error";
      if (operation == "simulate_net") {
        invoke("monte_carlo(net, options)", [&] {result = minera::monte_carlo(document.net, configured);});
      } else if (operation == "simulate_log") {
        invoke("monte_carlo(log, net, options)", [&] {result = minera::monte_carlo(source->checked_log(), document.net, configured);});
      } else {
        invoke("fit_monte_carlo_options(log, net, options)", [&] {fitted = minera::fit_monte_carlo_options(source->checked_log(), document.net, configured);});
        if (operation == "fit_then_simulate")
          invoke("monte_carlo(net, options)", [&] {result = minera::monte_carlo(document.net, *fitted);});
      }
      executed = true;
    } catch (const std::exception& error) {failure = error_json_value(stage, error.what());}
    calls += ']';
    std::map<std::string, std::string> exports;
    const auto export_value = [&](const std::string& key, const auto& produce) {
      try {exports[key] = produce();}
      catch (const std::exception& error) {exports[key + "_error"] = error.what();}
    };
    export_value("source_pnml", [&] {return minera::to_pnml(document, "Original Monte Carlo model");});
    if (source && source->checked_log_if_available())
      export_value("source_xes", [&] {return minera::to_xes(source->checked_log(), "Original Monte Carlo fitting source");});
    if (result) export_value("generated_xes", [&] {return minera::to_xes(result->log, "Monte Carlo generated log");});
    std::string exports_json = "{", supplied_json = "[";
    for (const auto& key : {"source_pnml", "source_xes", "generated_xes"}) {
      if (exports_json.size() > 1) exports_json += ',';
      exports_json += quote(key) + ':' + nullable(exports[key]) + ',' + quote(std::string(key) + "_error") + ':' + nullable(exports[std::string(key) + "_error"]);
    }
    for (const auto& member : members) {if (supplied_json.size() > 1) supplied_json += ','; supplied_json += quote(member);}
    exports_json += '}'; supplied_json += ']';
    const auto source_after = source_snapshot();
    response.status = failure == "null" ? 200 : 400;
    response.set_content(active_source_safe_json("{\"family\":\"MONTE_CARLO_WORKBENCH\",\"operation\":" + quote(operation) +
      ",\"executed\":" + flag(executed) + ",\"native_calls\":" + calls + ",\"error\":" + failure +
      ",\"input_context\":{\"model_dataset\":" + quote(file.filename) + ",\"original_model_bytes\":" + std::to_string(file.content.size()) +
      ",\"model_unchanged\":" + flag(before == minera::serialize_petri_workbench_document_json(document)) +
      ",\"source_unchanged\":" + flag(source_before == source_after) + ",\"initial_marking_origin\":" + quote(initial_origin) +
      ",\"final_marking_origin\":" + quote(final_origin) + ",\"stochastic_map_origin\":" + quote(map_origin) + '}' +
      ",\"pnml_import_options\":{\"infer_initial_marking\":" + flag(imports.infer_initial_marking) + ",\"infer_final_marking\":" + flag(imports.infer_final_marking) +
      ",\"import_stochastic_map\":" + flag(imports.import_stochastic_map) + ",\"require_workflow_boundaries\":" + flag(imports.require_workflow_boundaries) +
      ",\"public_marking_semantics\":" + flag(imports.public_marking_semantics) + '}' +
      ",\"model\":{\"pnml_document\":" + before + ",\"petri_net\":" + minera::workflow_input_petri_json(document.net) +
      ",\"original_model\":" + quote(file.content) + "},\"source\":" + source_after +
      ",\"submitted_options\":" + minera::serialize_monte_carlo_workbench_options_json(submitted) + ",\"supplied_options\":" + supplied_json +
      ",\"configured_options\":" + minera::serialize_monte_carlo_workbench_options_json(configured) +
      ",\"fitted_options\":" + (fitted ? minera::serialize_monte_carlo_workbench_options_json(*fitted) : "null") +
      ",\"result\":" + (result ? minera::serialize_monte_carlo_workbench_result_json(*result) : "null") + ",\"exports\":" + exports_json + '}'), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(active_source_safe_json(error_json(error.what())), "application/json");
  }
}
