#pragma once

// Original model and source belong to this request. The native decision APIs
// accept PetriNet, so PNML token counts remain document provenance; no extra
// marking policy, cached model, projection or synthetic miner is injected.
void handle_original_decision_workbench(const httplib::Request& request,
                                        httplib::Response& response) {
  try {
    if (!request.is_multipart_form_data() || !request.has_file("model_file") || !request.has_file("log"))
      throw std::invalid_argument("decision workbench requires multipart model_file and log");
    const auto supplied = [&](std::string_view name) {return active_source_supplied(request, name);};
    const auto field = [&](std::string_view name) {return active_source_field(request, name);};
    const auto operation = supplied("operation") ? field("operation") : "decision_mining";
    if (operation != "decision_mining" && operation != "decision_tables" && operation != "data_petri_net")
      throw std::invalid_argument("unsupported decision operation: " + operation);
    const auto format = supplied("model_format") ? field("model_format") : "auto";
    if (format != "auto" && format != "pnml")
      throw std::invalid_argument("decision model_format must be auto or pnml");
    static const std::set<std::string> controls = {
      "model_file", "log", "operation", "model_format", "options_json",
      "pnml_infer_initial_marking", "pnml_infer_final_marking", "pnml_import_stochastic_map",
      "pnml_require_workflow_boundaries", "pnml_public_marking_semantics",
      "mining_input_format", "mining_input_shape", "mining_selectors_json",
      "case_column", "activity_column", "timestamp_column", "resource_column", "lifecycle_column", "delimiter",
      "xes_variant", "xes_case_attribute", "xes_activity_attribute", "xes_timestamp_attribute",
      "xes_resource_attribute", "xes_lifecycle_attribute", "xes_require_timestamp", "xes_timestamp_sort",
      "xes_reverse_sort", "xes_max_traces", "xes_attributes_to_read", "xes_max_decompressed_bytes"};
    const auto check = [&](const std::string& key) {
      if (request.params.count(key) + request.files.count(key) != 1)
        throw std::invalid_argument("duplicate decision control: " + key);
      if (!controls.contains(key)) throw std::invalid_argument("unknown decision control: " + key);
    };
    for (const auto& [key, value] : request.params) {(void)value; check(key);}
    for (const auto& [key, value] : request.files) {(void)value; check(key);}
    if (supplied("mining_input_shape") && field("mining_input_shape") != "log")
      throw std::invalid_argument("decision calls require mining_input_shape=log");
    using Options = std::variant<minera::DecisionMiningOptions, minera::DecisionTableOptions, minera::DataPetriNetOptions>;
    std::set<std::string> members;
    const auto options_text = supplied("options_json") ? field("options_json") : "{}";
    const Options submitted = [&]() -> Options {
      if (operation == "decision_mining") return minera::parse_decision_workbench_options(options_text, &members);
      if (operation == "decision_tables") return minera::parse_decision_table_workbench_options(options_text, &members);
      return minera::parse_data_petri_workbench_options(options_text, &members);
    }();
    const auto options_json = [](const Options& value) {
      return std::visit([](const auto& item) {return minera::serialize_decision_workbench_options_json(item);}, value);
    };
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
    const auto model_before = minera::serialize_petri_workbench_document_json(document);
    const auto quote = active_source_quote;
    const auto flag = [](bool value) {return value ? "true" : "false";};
    const auto nullable = [&](const std::string& value) {return value.empty() ? std::string("null") : quote(value);};
    const auto error_value = [&](std::string_view type, std::string_view message) {
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

    using Result = std::variant<std::monostate, minera::DecisionMiningResult, minera::DecisionTablesResult, minera::DataPetriNet>;
    Result result;
    std::string failure = "null", source_before = "null", calls = "[", stage = "preparation_error";
    bool executed = false;
    const auto invoke = [&](std::string_view name, const auto& call) {
      const auto prefix = "{\"name\":" + quote(name) + ",\"returned\":";
      try {call(); calls += prefix + "true,\"error\":null}";}
      catch (const std::exception& error) {
        calls += prefix + "false,\"error\":" + error_value("native_error", error.what()) + '}';
        throw;
      }
    };
    try {
      auto params = minera::parse_mine_params(field, supplied);
      const auto& log_file = request.get_file_value("log");
      source = prepare_active_event_source(request, log_file.content, log_file.filename, params);
      source_before = source_snapshot();
      (void)source->checked_log();
      stage = "native_error";
      if (operation == "decision_mining")
        invoke("mine_decisions(source_log, net, options)", [&] {
          result = minera::mine_decisions(source->checked_log(), document.net, std::get<minera::DecisionMiningOptions>(submitted));
        });
      else if (operation == "decision_tables")
        invoke("mine_decision_tables(source_log, net, options)", [&] {
          result = minera::mine_decision_tables(source->checked_log(), document.net, std::get<minera::DecisionTableOptions>(submitted));
        });
      else
        invoke("mine_data_petri_net(source_log, net, options)", [&] {
          result = minera::mine_data_petri_net(source->checked_log(), document.net, std::get<minera::DataPetriNetOptions>(submitted));
        });
      executed = true;
    } catch (const std::exception& error) {failure = error_value(stage, error.what());}
    calls += ']';
    const auto result_json = std::visit([&](const auto& value) -> std::string {
      using T = std::decay_t<decltype(value)>;
      if constexpr (std::is_same_v<T, std::monostate>) return "null";
      else return "{\"type\":" + quote(operation) + ",\"value\":" + minera::serialize_decision_workbench_result_json(value) + '}';
    }, result);
    std::map<std::string, std::string> exports;
    const auto export_value = [&](const std::string& key, const auto& produce) {
      try {exports[key] = produce();}
      catch (const std::exception& error) {exports[key + "_error"] = error.what();}
    };
    export_value("source_pnml", [&] {return minera::to_pnml(document, "Original decision model");});
    if (source && source->checked_log_if_available())
      export_value("source_xes", [&] {return minera::to_xes(source->checked_log(), "Original decision source");});
    if (const auto* value = std::get_if<minera::DataPetriNet>(&result))
      export_value("result_pnml", [&] {
        auto structural = document;
        structural.net = value->net;
        return minera::to_pnml(structural, "Structural decision result; learned guards are separate");
      });
    std::string exports_json = "{", supplied_json = "[";
    const auto optional_export = [&](const std::string& key) {
      const auto found = exports.find(key);
      return found == exports.end() ? std::string("null") : quote(found->second);
    };
    for (const auto& key : {"source_pnml", "source_xes", "result_pnml"}) {
      if (exports_json.size() > 1) exports_json += ',';
      exports_json += quote(key) + ':' + optional_export(key) + ',' + quote(std::string(key) + "_error") + ':' + optional_export(std::string(key) + "_error");
    }
    for (const auto& member : members) {if (supplied_json.size() > 1) supplied_json += ','; supplied_json += quote(member);}
    exports_json += '}'; supplied_json += ']';
    const auto source_after = source_snapshot();
    const std::string import_json = std::string("{\"infer_initial_marking\":") + flag(imports.infer_initial_marking) +
      ",\"infer_final_marking\":" + flag(imports.infer_final_marking) + ",\"import_stochastic_map\":" + flag(imports.import_stochastic_map) +
      ",\"require_workflow_boundaries\":" + flag(imports.require_workflow_boundaries) +
      ",\"public_marking_semantics\":" + flag(imports.public_marking_semantics) + '}';
    response.status = failure == "null" ? 200 : 400;
    response.set_content(active_source_safe_json("{\"family\":\"ORIGINAL_DECISION_WORKBENCH\",\"operation\":" + quote(operation) +
      ",\"executed\":" + flag(executed) + ",\"native_calls\":" + calls + ",\"error\":" + failure +
      ",\"input_context\":{\"model_dataset\":" + quote(file.filename) + ",\"original_model_bytes\":" + std::to_string(file.content.size()) +
      ",\"model_format\":" + quote(format) + ",\"detected_format\":\"pnml\"" +
      ",\"model_unchanged\":" + flag(model_before == minera::serialize_petri_workbench_document_json(document)) +
      ",\"source_unchanged\":" + flag(source_before == source_after) +
      ",\"document_marking_counts_applied\":false,\"native_marking_policy\":\"workflow_source_sink_defaults\"}" +
      ",\"pnml_import_options\":" + import_json + ",\"model\":{\"original_model\":" + quote(file.content) +
      ",\"pnml_document\":" + model_before + ",\"petri_net\":" + minera::workflow_input_petri_json(document.net) + '}' +
      ",\"source\":" + source_after + ",\"submitted_options\":" + options_json(submitted) + ",\"supplied_options\":" + supplied_json +
      ",\"configured_options\":" + options_json(submitted) + ",\"result\":" + result_json + ",\"exports\":" + exports_json + '}'), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(active_source_safe_json(error_json(error.what())), "application/json");
  }
}
