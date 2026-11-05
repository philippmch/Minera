#pragma once

// Each independent request owns its original model and optional fitting log.
// No cached session is read or published, and every operation dispatches to
// exactly its declared public native overload.
void handle_playout_workbench(const httplib::Request& request,
                              httplib::Response& response) {
  try {
    if (!request.is_multipart_form_data() || !request.has_file("model_file"))
      throw std::invalid_argument("playout requires multipart model_file");
    const auto supplied = [&](std::string_view name) {return active_source_supplied(request, name);};
    const auto field = [&](std::string_view name) {return active_source_field(request, name);};
    const auto operation = supplied("operation") ? field("operation") : "petri_basic_log";
    static const std::set<std::string> operations = {
      "petri_basic_log", "petri_basic_visited", "petri_stochastic_log", "petri_stochastic_visited",
      "petri_stochastic_log_from_source", "petri_stochastic_visited_from_source", "petri_fit_weights",
      "petri_extensive_log", "petri_extensive_visited", "dfg_result", "dfg_log", "tree_result", "tree_log"};
    if (!operations.contains(operation))
      throw std::invalid_argument("unsupported playout operation: " + operation);
    const bool petri_mode = operation.starts_with("petri_");
    const bool dfg_mode = operation.starts_with("dfg_");
    const bool basic_mode = operation.starts_with("petri_basic_");
    const bool extensive_mode = operation.starts_with("petri_extensive_");
    const bool log_mode = operation == "petri_stochastic_log_from_source" ||
      operation == "petri_stochastic_visited_from_source" || operation == "petri_fit_weights";
    const std::string model_kind = petri_mode ? "petri" : dfg_mode ? "dfg" : "tree";
    const auto format = supplied("model_format") ? field("model_format") : "auto";
    static const std::set<std::string> formats = {"auto", "pnml", "dfg_json", "dfg_text", "ptml", "tree_json"};
    if (!formats.contains(format)) throw std::invalid_argument("unsupported model_format: " + format);
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
        throw std::invalid_argument("duplicate playout control: " + key);
      if (key == "model_file" || key == "operation" || key == "model_format" || key == "options_json") return;
      if (petri_mode && pnml_controls.contains(key)) return;
      if (log_mode && source_controls.contains(key)) return;
      throw std::invalid_argument("unknown or inapplicable playout control: " + key);
    };
    for (const auto& [key, value] : request.params) {(void)value; check(key);}
    for (const auto& [key, value] : request.files) {(void)value; check(key);}
    if (log_mode && !request.has_file("log"))
      throw std::invalid_argument("this playout operation requires an original log file");
    if (supplied("mining_input_shape") && field("mining_input_shape") != "log")
      throw std::invalid_argument("playout fitting requires mining_input_shape=log");
    using Options = std::variant<minera::PlayoutOptions, minera::StochasticPlayoutOptions,
      minera::ExtensivePlayoutOptions, minera::DfgPlayoutOptions, minera::ProcessTreePlayoutOptions>;
    std::set<std::string> members;
    const auto options_text = supplied("options_json") ? field("options_json") : "{}";
    const Options submitted = [&]() -> Options {
      if (basic_mode) return minera::parse_playout_workbench_options(options_text, &members);
      if (extensive_mode) return minera::parse_extensive_playout_workbench_options(options_text, &members);
      if (petri_mode) return minera::parse_stochastic_playout_workbench_options(options_text, &members);
      if (dfg_mode) return minera::parse_dfg_playout_workbench_options(options_text, &members);
      return minera::parse_process_tree_playout_workbench_options(options_text, &members);
    }();
    auto configured = submitted;
    const auto options_json = [](const Options& value) {
      return std::visit([](const auto& item) {return minera::serialize_playout_workbench_options_json(item);}, value);
    };
    minera::PnmlImportOptions imports;
    const auto boolean_option = [&](std::string_view name, bool& value) {
      if (supplied(name)) value = petri_anti_multi_boolean(field(name), name);
    };
    if (petri_mode) {
      boolean_option("pnml_infer_initial_marking", imports.infer_initial_marking);
      boolean_option("pnml_infer_final_marking", imports.infer_final_marking);
      boolean_option("pnml_import_stochastic_map", imports.import_stochastic_map);
      boolean_option("pnml_require_workflow_boundaries", imports.require_workflow_boundaries);
      boolean_option("pnml_public_marking_semantics", imports.public_marking_semantics);
    }
    const auto& file = request.get_file_value("model_file");
    auto detected = format;
    if (detected == "auto") {
      const auto first = file.content.find_first_not_of(" \t\r\n");
      const bool json = first != std::string::npos && file.content[first] == '{';
      detected = petri_mode ? "pnml" : dfg_mode ? (json ? "dfg_json" : "dfg_text") : (json ? "tree_json" : "ptml");
    }
    if ((petri_mode && detected != "pnml") ||
        (dfg_mode && detected != "dfg_json" && detected != "dfg_text") ||
        (!petri_mode && !dfg_mode && detected != "ptml" && detected != "tree_json"))
      throw std::invalid_argument("model_format is incompatible with operation " + operation);
    std::optional<minera::PnmlDocument> document;
    std::optional<minera::DfgPlayoutInput> dfg;
    std::optional<minera::ProcessTree> tree;
    if (petri_mode) document = minera::parse_pnml_document(file.content, imports);
    else if (dfg_mode) {
      if (detected == "dfg_json") dfg = minera::parse_dfg_playout_input_json(file.content);
      else {
        const auto parsed = minera::parse_dfg(file.content);
        dfg = minera::DfgPlayoutInput{parsed.graph, parsed.start_activities, parsed.end_activities, std::nullopt};
      }
    } else tree = detected == "tree_json" ? minera::parse_process_tree_alignment_input_json(file.content)
                                          : minera::parse_ptml(file.content);
    const auto snapshot = [&]() {
      if (document) return minera::serialize_petri_workbench_document_json(*document);
      if (dfg) return minera::serialize_dfg_playout_input_json(*dfg);
      return minera::serialize_process_tree_alignment_input_json(*tree);
    };
    const auto model_before = snapshot();
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

    const auto initial_origin = petri_mode ? quote(members.contains("initial_marking") ? "explicit" : "pnml") : "null";
    const auto final_origin = petri_mode ? quote(members.contains("final_marking") ? "explicit" : "pnml") : "null";
    std::string failure = "null", source_before = "null", calls = "[";
    using Result = std::variant<std::monostate, minera::EventLog, minera::PlayoutVisitedResult,
      minera::DfgPlayoutResult, minera::ProcessTreePlayoutResult, std::vector<double>>;
    Result result;
    bool executed = false;
    std::string stage = "configuration_error";
    const auto invoke = [&](std::string_view name, const auto& call) {
      const auto prefix = "{\"name\":" + quote(name) + ",\"returned\":";
      try {call(); calls += prefix + "true,\"error\":null}";}
      catch (const std::exception& error) {
        calls += prefix + "false,\"error\":" + error_value("native_error", error.what()) + '}';
        throw;
      }
    };
    try {
      // Only omitted fields receive PNML defaults. Explicit empty vectors keep
      // the native source/sink default semantics; no marking keys are invented.
      if (document) std::visit([&](auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, minera::PlayoutOptions> ||
                      std::is_same_v<T, minera::StochasticPlayoutOptions> ||
                      std::is_same_v<T, minera::ExtensivePlayoutOptions>) {
          if (!members.contains("initial_marking")) value.initial_marking = minera::tools::pnml_marking<std::uint32_t>(document->initial_marking);
          if (!members.contains("final_marking")) value.final_marking = minera::tools::pnml_marking<std::uint32_t>(document->final_marking);
        }
      }, configured);
      if (log_mode) {
        stage = "preparation_error";
        auto params = minera::parse_mine_params(field, supplied);
        const auto& log_file = request.get_file_value("log");
        source = prepare_active_event_source(request, log_file.content, log_file.filename, params);
        source_before = source_snapshot();
        (void)source->checked_log();
      }
      stage = "native_error";
      if (basic_mode) {
        const auto& options = std::get<minera::PlayoutOptions>(configured);
        if (operation == "petri_basic_log")
          invoke("playout(net, options)", [&] {result = minera::playout(document->net, options);});
        else invoke("playout_visited(net, options)", [&] {result = minera::playout_visited(document->net, options);});
      } else if (extensive_mode) {
        const auto& options = std::get<minera::ExtensivePlayoutOptions>(configured);
        if (operation == "petri_extensive_log")
          invoke("extensive_playout(net, options)", [&] {result = minera::extensive_playout(document->net, options);});
        else invoke("extensive_playout_visited(net, options)", [&] {result = minera::extensive_playout_visited(document->net, options);});
      } else if (petri_mode) {
        // The server's stochastic bounds (see handle_playout): every firing
        // counts, visited markings are bounded, a disconnect cancels.
        auto options = std::get<minera::StochasticPlayoutOptions>(configured);
        const bool visited_operation = operation == "petri_stochastic_visited" ||
                                       operation == "petri_stochastic_visited_from_source";
        options.max_total_firings =
            visited_operation ? kServerStochasticVisitedFirings : kServerStochasticFirings;
        if (visited_operation)
          options.max_visited_marking_entries = kServerStochasticVisitedMarkingEntries;
        options.cancelled = connection_cancel_hook(request);
        if (operation == "petri_stochastic_log")
          invoke("stochastic_playout(net, options)", [&] {result = minera::stochastic_playout(document->net, options);});
        else if (operation == "petri_stochastic_visited")
          invoke("stochastic_playout_visited(net, options)", [&] {result = minera::stochastic_playout_visited(document->net, options);});
        else if (operation == "petri_stochastic_log_from_source")
          invoke("stochastic_playout(net, source_log, options)", [&] {result = minera::stochastic_playout(document->net, source->checked_log(), options);});
        else if (operation == "petri_stochastic_visited_from_source")
          invoke("stochastic_playout_visited(net, source_log, options)", [&] {result = minera::stochastic_playout_visited(document->net, source->checked_log(), options);});
        else invoke("fit_stochastic_transition_weights(net, source_log, options)", [&] {
          result = minera::fit_stochastic_transition_weights(document->net, source->checked_log(), options);
        });
      } else if (dfg_mode) {
        const auto& options = std::get<minera::DfgPlayoutOptions>(configured);
        const auto* performance = dfg->performance ? &*dfg->performance : nullptr;
        if (operation == "dfg_result")
          invoke("dfg_playout_result(frequency, starts, ends, options, performance)", [&] {
            result = minera::dfg_playout_result(dfg->frequency, dfg->start_activities, dfg->end_activities, options, performance);
          });
        else invoke("dfg_playout(frequency, starts, ends, options, performance)", [&] {
          result = minera::dfg_playout(dfg->frequency, dfg->start_activities, dfg->end_activities, options, performance);
        });
      } else {
        auto& options = std::get<minera::ProcessTreePlayoutOptions>(configured);
        options.cancelled = connection_cancel_hook(request);
        if (operation == "tree_result")
          invoke("process_tree_playout_result(tree, options)", [&] {result = minera::process_tree_playout_result(*tree, options);});
        else invoke("process_tree_playout(tree, options)", [&] {result = minera::process_tree_playout(*tree, options);});
      }
      executed = true;
    } catch (const std::exception& error) {failure = error_value(stage, error.what());}
    calls += ']';
    const auto doubles = [](const std::vector<double>& values) {
      std::ostringstream out;
      out.imbue(std::locale::classic());
      out.precision(std::numeric_limits<double>::max_digits10);
      out << '[';
      for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out << ',';
        const double value = values[i];
        if (std::isnan(value)) out << "\"NaN\"";
        else if (std::isinf(value)) out << (value < 0 ? "\"-INF\"" : "\"INF\"");
        else if (value == 0 && std::signbit(value)) out << "-0.0";
        else out << value;
      }
      out << ']';
      return out.str();
    };
    const auto* fitted = std::get_if<std::vector<double>>(&result);
    const auto fitted_json = fitted ? doubles(*fitted) : std::string("null");
    const auto result_json = std::visit([&](const auto& value) -> std::string {
      using T = std::decay_t<decltype(value)>;
      if constexpr (std::is_same_v<T, std::monostate>) return "null";
      else if constexpr (std::is_same_v<T, minera::EventLog>)
        return "{\"type\":\"event_log\",\"value\":" + minera::serialize_active_source_log_json(value) + '}';
      else if constexpr (std::is_same_v<T, std::vector<double>>)
        return "{\"type\":\"weights\",\"value\":" + fitted_json + '}';
      else {
        const char* type = std::is_same_v<T, minera::PlayoutVisitedResult> ? "visited" :
          std::is_same_v<T, minera::DfgPlayoutResult> ? "dfg_result" : "tree_result";
        return "{\"type\":" + quote(type) + ",\"value\":" + minera::serialize_playout_workbench_result_json(value) + '}';
      }
    }, result);
    const auto* generated_log = std::get_if<minera::EventLog>(&result);
    if (const auto* value = std::get_if<minera::DfgPlayoutResult>(&result)) generated_log = &value->log;
    if (const auto* value = std::get_if<minera::ProcessTreePlayoutResult>(&result)) generated_log = &value->log;
    std::map<std::string, std::string> exports;
    const auto export_value = [&](const std::string& key, const auto& produce) {
      try {exports[key] = produce();}
      catch (const std::exception& error) {exports[key + "_error"] = error.what();}
    };
    if (document) export_value("source_pnml", [&] {return minera::to_pnml(*document, "Original playout model");});
    if (dfg) export_value("source_dfg", [&] {
      return minera::to_dfg(minera::DfgCleanAnalysis{dfg->frequency, dfg->start_activities, dfg->end_activities});
    });
    if (tree) {
      if (process_tree_alignment_export_indices_safe(*tree))
        export_value("source_ptml", [&] {return minera::to_ptml(*tree, "Original playout process tree");});
      else exports["source_ptml_error"] = "PTML export is unavailable because an activity leaf index is outside the native activity registry.";
    }
    if (source && source->checked_log_if_available())
      export_value("source_xes", [&] {return minera::to_xes(source->checked_log(), "Original playout fitting source");});
    if (generated_log) export_value("generated_xes", [&] {return minera::to_xes(*generated_log, "Generated playout log");});
    std::string exports_json = "{", supplied_json = "[";
    const auto optional_export = [&](const std::string& key) {
      const auto found = exports.find(key);
      return found == exports.end() ? std::string("null") : quote(found->second);
    };
    for (const auto& key : {"source_pnml", "source_ptml", "source_dfg", "source_xes", "generated_xes"}) {
      if (exports_json.size() > 1) exports_json += ',';
      exports_json += quote(key) + ':' + optional_export(key) + ',' + quote(std::string(key) + "_error") + ':' + optional_export(std::string(key) + "_error");
    }
    for (const auto& member : members) {if (supplied_json.size() > 1) supplied_json += ','; supplied_json += quote(member);}
    exports_json += '}'; supplied_json += ']';
    const auto source_after = source_snapshot();
    const std::string import_json = document ? std::string("{\"infer_initial_marking\":") + flag(imports.infer_initial_marking) +
      ",\"infer_final_marking\":" + flag(imports.infer_final_marking) + ",\"import_stochastic_map\":" + flag(imports.import_stochastic_map) +
      ",\"require_workflow_boundaries\":" + flag(imports.require_workflow_boundaries) +
      ",\"public_marking_semantics\":" + flag(imports.public_marking_semantics) + '}' : "null";
    response.status = failure == "null" ? 200 : 400;
    response.set_content(active_source_safe_json("{\"family\":\"PLAYOUT_WORKBENCH\",\"operation\":" + quote(operation) +
      ",\"model_kind\":" + quote(model_kind) + ",\"executed\":" + flag(executed) + ",\"native_calls\":" + calls + ",\"error\":" + failure +
      ",\"input_context\":{\"model_dataset\":" + quote(file.filename) + ",\"original_model_bytes\":" + std::to_string(file.content.size()) +
      ",\"model_format\":" + quote(format) + ",\"detected_format\":" + quote(detected) +
      ",\"model_unchanged\":" + flag(model_before == snapshot()) + ",\"source_unchanged\":" + flag(source_before == source_after) +
      ",\"initial_marking_origin\":" + initial_origin + ",\"final_marking_origin\":" + final_origin + '}' +
      ",\"pnml_import_options\":" + import_json + ",\"model\":{\"original_model\":" + quote(file.content) +
      ",\"pnml_document\":" + (document ? model_before : "null") +
      ",\"petri_net\":" + (document ? minera::workflow_input_petri_json(document->net) : "null") +
      ",\"dfg\":" + (dfg ? model_before : "null") + ",\"process_tree\":" + (tree ? model_before : "null") + '}' +
      ",\"source\":" + source_after + ",\"submitted_options\":" + options_json(submitted) + ",\"supplied_options\":" + supplied_json +
      ",\"configured_options\":" + options_json(configured) + ",\"fitted_weights\":" + fitted_json +
      ",\"result\":" + result_json + ",\"exports\":" + exports_json + '}'), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(active_source_safe_json(error_json(error.what())), "application/json");
  }
}
