#pragma once

// Every normative model and input belongs to this request. Native overloads
// consume their selected source shape; no cached model or synthetic log is used.
void handle_original_log_skeleton_workbench(const httplib::Request& request,
                                       httplib::Response& response) {
  try {
    if (!request.is_multipart_form_data()) throw std::invalid_argument("log-skeleton workbench requires multipart data");
    const auto supplied = [&](std::string_view name) {return active_source_supplied(request, name);};
    const auto field = [&](std::string_view name) {return active_source_field(request, name);};
    for (const auto* name : {"operation", "model_file", "model_format"})
      if (request.params.count(name) + request.files.count(name) > 1)
        throw std::invalid_argument(std::string("duplicate log-skeleton control: ") + name);
    const auto operation = supplied("operation") ? field("operation") : "conformance";
    const bool standalone = operation == "diagnostics" || operation == "diagnostics_view";
    const bool multiple = operation == "conformance" || operation == "conformance_view";
    const bool source_operation = multiple || standalone;
    const auto format = supplied("model_format") ? field("model_format") : "exact";
    const auto model_upload = request.has_file("model_file") ? std::make_optional(request.get_file_value("model_file")) : std::nullopt;
    std::optional<minera::LogSkeletonModel> model;
    if (!standalone) {
      if (!model_upload) throw std::invalid_argument("log-skeleton conformance requires model_file");
      if (format != "exact") throw std::invalid_argument("log-skeleton model_format must be exact");
      model = minera::parse_log_skeleton_workbench_model(model_upload->content);
    }
    const auto model_before = model ? minera::serialize_log_skeleton_workbench_model_json(*model) : std::string("null");
    const auto quote = active_source_quote;
    const auto flag = [](bool value) {return value ? "true" : "false";};
    const auto nullable = [&](const std::string& value) {return value.empty() ? std::string("null") : quote(value);};
    const auto error_value = [&](std::string_view type, std::string_view message) {
      return "{\"type\":" + quote(type) + ",\"message\":" + quote(message) + '}';
    };
    // JSON text retains Unicode/WTF-8 exactly. Binary original uploads use hex.
    const auto original_bytes = [&](std::string_view bytes) {
      bool textual = true;
      for (std::size_t i = 0; i < bytes.size() && textual;) {
        const auto first = static_cast<unsigned char>(bytes[i++]);
        if (first < 0x80) continue;
        const int count = first >= 0xc2 && first <= 0xdf ? 1 : first >= 0xe0 && first <= 0xef ? 2 : first >= 0xf0 && first <= 0xf4 ? 3 : -1;
        if (count < 0 || i + static_cast<std::size_t>(count) > bytes.size()) {textual = false; break;}
        unsigned code = first & (count == 1 ? 31 : count == 2 ? 15 : 7);
        for (int j = 0; j < count; ++j) {
          const auto next = static_cast<unsigned char>(bytes[i++]);
          if ((next & 0xc0) != 0x80) {textual = false; break;}
          code = (code << 6) | (next & 63);
        }
        if ((count == 1 && code < 0x80) || (count == 2 && code < 0x800) ||
            (count == 3 && code < 0x10000) || code > 0x10ffff) textual = false;
      }
      std::string encoded(bytes);
      if (!textual) {
        constexpr char hex[] = "0123456789abcdef";
        encoded.clear(); encoded.reserve(bytes.size() * 2);
        for (const unsigned char byte : bytes) {encoded += hex[byte >> 4]; encoded += hex[byte & 15];}
      }
      return std::string("\"original_input_encoding\":") + quote(textual ? "utf8" : "hex") +
        ",\"original_input\":" + quote(encoded) + ",\"original_input_bytes\":" + std::to_string(bytes.size());
    };
    const auto input_upload = request.has_file("input_file") ? std::make_optional(request.get_file_value("input_file")) : std::nullopt;
    const auto* input_file = input_upload ? &*input_upload : nullptr;
    const std::string input_bytes_before = input_file ? input_file->content : std::string{};
    const auto results_upload = request.has_file("results_file") ? std::make_optional(request.get_file_value("results_file")) : std::nullopt;
    const auto* results_file = results_upload ? &*results_upload : nullptr;
    const std::string results_bytes_before = results_file ? results_file->content : std::string{};
    std::optional<std::vector<std::string>> word;
    std::optional<minera::Trace> trace;
    std::optional<minera::LogSkeletonVariantList> variants;
    std::optional<std::vector<minera::LogSkeletonConformance>> provided_results;
    std::optional<minera::ActiveEventSource> source;
    std::optional<minera::LogSkeletonOptions> conformance_options;
    std::optional<minera::LogSkeletonDiagnosticsOptions> diagnostics_options;
    std::set<std::string> conformance_members, diagnostics_members;
    bool include_diagnostics = false, executed = false;
    std::string input_shape = source_operation ? (supplied("mining_input_shape") ? field("mining_input_shape") : "log") :
      operation == "conformance_word" ? "word" : operation == "conformance_trace" ? "trace" : operation == "conformance_variants" ? "variants" : "";
    const auto table_json = [](const minera::EventTable* value) {
      return value ? minera::serialize_event_table_input_json(*value) : std::string("null");
    };
    const auto log_json = [](const minera::EventLog* value) {
      return value ? minera::serialize_active_source_log_json(*value) : std::string("null");
    };
    const auto input_snapshot = [&] {
      if (word) return minera::serialize_alignment_trace_words_json(*word);
      if (trace) return minera::serialize_declare_workbench_trace_json(*trace);
      if (variants) return minera::serialize_log_skeleton_workbench_variants_json(*variants);
      return std::string("null");
    };
    const auto source_snapshot = [&]() {
      if (!source) return std::string("null");
      const auto& s = source->selectors();
      const auto* lazy = source->checked_lazy_if_available();
      std::string lazy_table = "null", lazy_count = "null";
      if (lazy) {
        lazy_table = minera::serialize_event_table_input_json(lazy->materialize());
        if (const auto count = lazy->row_count()) lazy_count = std::to_string(*count);
      }
      return "{\"dataset\":" + quote(source->file().name) + ",\"format\":" + quote(source->file().format == "table" ? "table" : "auto") +
        ",\"detected_format\":" + quote(source->file().format) + ',' + original_bytes(source->file().bytes) +
        ",\"source_selectors\":{\"case_id_key\":" + quote(s.case_id_key) + ",\"activity_key\":" + quote(s.activity_key) +
        ",\"timestamp_key\":" + quote(s.timestamp_key) + ",\"resource_key\":" + quote(s.resource_key) +
        ",\"lifecycle_key\":" + quote(s.lifecycle_key) + "},\"input_before\":" + table_json(source->original_table_if_available()) +
        ",\"input_after\":" + table_json(source->original_table_if_available()) +
        ",\"checked_table\":" + table_json(source->checked_table_if_available()) +
        ",\"original_log\":" + log_json(source->original_log_if_available()) +
        ",\"prepared_log\":" + log_json(source->checked_log_if_available()) +
        ",\"log_preparation_error\":" + nullable(source->checked_log_preparation_error()) +
        ",\"flat_projection_error\":" + nullable(source->checked_flat_projection_error()) +
        ",\"selected_shape\":" + quote(active_source_shape(source->selected_kind())) +
        ",\"selected_kind_index\":" + std::to_string(static_cast<int>(source->selected_kind())) +
        ",\"prepared_stream\":" + (source->checked_stream_if_available() ? minera::serialize_active_source_stream_json(*source->checked_stream_if_available()) : "null") +
        ",\"prepared_lazy_table\":" + lazy_table + ",\"lazy_row_count\":" + lazy_count +
        ",\"lazy_valid\":" + (lazy ? std::string(flag(lazy->valid())) : std::string("null")) + '}';
    };
    std::optional<minera::LogSkeletonConformance> scalar;
    std::optional<std::vector<minera::LogSkeletonConformance>> conformances;
    std::optional<minera::LogSkeletonVariantConformance> variant_conformance;
    std::optional<minera::EventTable> primary_diagnostics, diagnostics;
    std::string failure = "null", diagnostics_error = "null";
    std::string calls = "[", stage = "configuration_error", source_before = "null", input_before = "null", results_before = "null";
    const auto invoke = [&](std::string_view name, const auto& call) {
      if (calls.size() > 1) calls += ',';
      const auto prefix = "{\"name\":" + quote(name) + ",\"returned\":";
      try {call(); calls += prefix + "true,\"error\":null}";}
      catch (const std::exception& error) {
        calls += prefix + "false,\"error\":" + error_value("native_error", error.what()) + '}';
        throw;
      }
    };
    try {
      static const std::set<std::string> source_controls = {
        "mining_input_format", "mining_input_shape", "mining_selectors_json", "case_column", "activity_column",
        "timestamp_column", "resource_column", "lifecycle_column", "delimiter", "xes_variant", "xes_case_attribute",
        "xes_activity_attribute", "xes_timestamp_attribute", "xes_resource_attribute", "xes_lifecycle_attribute",
        "xes_require_timestamp", "xes_timestamp_sort", "xes_reverse_sort", "xes_max_traces", "xes_attributes_to_read", "xes_max_decompressed_bytes"};
      static const std::set<std::string> controls = {
        "model_file", "input_file", "results_file", "operation", "model_format", "conformance_options_json", "diagnostics_options_json", "include_diagnostics"};
      const auto check_control = [&](const std::string& key) {
        if (request.params.count(key) + request.files.count(key) != 1)
          throw std::invalid_argument("duplicate log-skeleton control: " + key);
        if (!controls.contains(key) && !source_controls.contains(key))
          throw std::invalid_argument("unknown log-skeleton control: " + key);
        if (source_controls.contains(key) && !source_operation)
          throw std::invalid_argument("source control is inapplicable to this log-skeleton operation: " + key);
      };
      for (const auto& [key, value] : request.params) {(void)value; check_control(key);}
      for (const auto& [key, value] : request.files) {(void)value; check_control(key);}
      if (!source_operation && operation != "conformance_word" && operation != "conformance_trace" && operation != "conformance_variants")
        throw std::invalid_argument("unsupported log-skeleton operation: " + operation);
      if (!input_file) throw std::invalid_argument("log-skeleton workbench requires input_file");
      if (standalone && supplied("model_file")) throw std::invalid_argument("model_file is inapplicable to standalone log-skeleton diagnostics");
      if (format != "exact") throw std::invalid_argument("log-skeleton model_format must be exact");
      if (standalone ? !results_file : supplied("results_file"))
        throw std::invalid_argument(standalone ? "standalone diagnostics requires results_file" : "results_file is only available for standalone diagnostics");
      if (supplied("include_diagnostics")) {
        if (!multiple) throw std::invalid_argument("include_diagnostics is only available for source conformance operations");
        include_diagnostics = petri_anti_multi_boolean(field("include_diagnostics"), "include_diagnostics");
      }
      if (standalone && supplied("conformance_options_json"))
        throw std::invalid_argument("conformance_options_json is inapplicable to standalone diagnostics");
      if (supplied("diagnostics_options_json") && !(standalone || include_diagnostics))
        throw std::invalid_argument("diagnostics_options_json requires standalone or requested derived diagnostics");
      if (source_operation && input_shape != "log" && input_shape != "stream" && input_shape != "dataframe" && input_shape != "lazyframe")
        throw std::invalid_argument("mining_input_shape must be log, stream, dataframe or lazyframe");
      if (!standalone)
        conformance_options = minera::parse_log_skeleton_conformance_workbench_options(supplied("conformance_options_json") ? field("conformance_options_json") : "{}", &conformance_members);
      if (standalone || include_diagnostics)
        diagnostics_options = minera::parse_log_skeleton_diagnostics_workbench_options(supplied("diagnostics_options_json") ? field("diagnostics_options_json") : "{}", &diagnostics_members);
      // Source import selectors and native option selectors are independent.
      auto imports = minera::parse_mine_params(field, supplied);
      stage = "preparation_error";
      if (operation == "conformance_word") word = minera::parse_alignment_trace_words_json(input_file->content);
      else if (operation == "conformance_trace") trace = minera::parse_declare_workbench_trace_json(input_file->content);
      else if (operation == "conformance_variants") variants = minera::parse_log_skeleton_workbench_variants_json(input_file->content);
      else {
        source = prepare_active_event_source(request, input_file->content, input_file->filename, imports);
        source_before = source_snapshot();
        (void)source->selected_view();
      }
      input_before = input_snapshot();
      if (standalone) {
        provided_results = minera::parse_log_skeleton_workbench_conformances_json(results_file->content);
        results_before = minera::serialize_log_skeleton_workbench_conformance_json(*provided_results);
      }
      // Generic dispatch retains the concrete native source type. The same
      // helper serves typed conformance and typed diagnostics without coercion.
      const auto concrete = [&](const auto& call) {
        switch (source->selected_kind()) {
          case minera::EventDataKind::kEventLog: return call(source->checked_log());
          case minera::EventDataKind::kEventStream: return call(source->checked_stream());
          case minera::EventDataKind::kEventTable: return call(source->checked_table());
          case minera::EventDataKind::kLazyEventTable: return call(source->checked_lazy());
        }
        throw std::invalid_argument("invalid native source kind");
      };
      const auto source_name = [&] {
        switch (source->selected_kind()) {
          case minera::EventDataKind::kEventLog: return "source_log";
          case minera::EventDataKind::kEventStream: return "source_stream";
          case minera::EventDataKind::kEventTable: return "source_table";
          case minera::EventDataKind::kLazyEventTable: return "source_lazy";
        }
        throw std::invalid_argument("invalid native source kind");
      };
      stage = "native_error";
      if (operation == "conformance_word")
        invoke("check_log_skeleton_conformance(word, model, options)", [&] {scalar = minera::check_log_skeleton_conformance(*word, *model, *conformance_options);});
      else if (operation == "conformance_trace")
        invoke("check_log_skeleton_conformance(trace, model, options)", [&] {scalar = minera::check_log_skeleton_conformance(*trace, *model, *conformance_options);});
      else if (operation == "conformance_variants")
        invoke("check_log_skeleton_conformance(variants, model, options)", [&] {variant_conformance = minera::check_log_skeleton_conformance(*variants, *model, *conformance_options);});
      else if (operation == "conformance_view")
        invoke("check_log_skeleton_conformance(source_view, model, options)", [&] {conformances = minera::check_log_skeleton_conformance(source->selected_view(), *model, *conformance_options);});
      else if (operation == "conformance")
        invoke(std::string("check_log_skeleton_conformance(") + source_name() + ", model, options)", [&] {
          conformances = concrete([&](const auto& value) {return minera::check_log_skeleton_conformance(value, *model, *conformance_options);});
        });
      else if (operation == "diagnostics_view")
        invoke("log_skeleton_conformance_diagnostics(source_view, results, options)", [&] {primary_diagnostics = minera::log_skeleton_conformance_diagnostics(source->selected_view(), *provided_results, *diagnostics_options);});
      else
        invoke(std::string("log_skeleton_conformance_diagnostics(") + source_name() + ", results, options)", [&] {
          primary_diagnostics = concrete([&](const auto& value) {return minera::log_skeleton_conformance_diagnostics(value, *provided_results, *diagnostics_options);});
        });
      executed = true;
      if (include_diagnostics && conformances) {
        try {
          if (operation == "conformance_view")
            invoke("log_skeleton_conformance_diagnostics(source_view, results, options)", [&] {diagnostics = minera::log_skeleton_conformance_diagnostics(source->selected_view(), *conformances, *diagnostics_options);});
          else
            invoke(std::string("log_skeleton_conformance_diagnostics(") + source_name() + ", results, options)", [&] {
              diagnostics = concrete([&](const auto& value) {return minera::log_skeleton_conformance_diagnostics(value, *conformances, *diagnostics_options);});
            });
        } catch (const std::exception& error) {diagnostics_error = error_value("native_error", error.what());}
      }
    } catch (const std::exception& error) {failure = error_value(stage, error.what());}
    calls += ']';
    const auto source_after = source_snapshot();
    const auto input_after = input_snapshot();
    const std::string results_after = provided_results ? minera::serialize_log_skeleton_workbench_conformance_json(*provided_results) : "null";
    const std::string result_json = scalar ? "{\"type\":\"conformance\",\"value\":" + minera::serialize_log_skeleton_workbench_conformance_json(*scalar) + '}' :
      conformances ? "{\"type\":\"conformance_vector\",\"value\":" + minera::serialize_log_skeleton_workbench_conformance_json(
          // An XES source's list in pm4py's DataFrame case order; the derived
          // diagnostics table keeps the frame's (file) order.
          in_constraint_order(*conformances, source && source->selected_kind() == minera::EventDataKind::kEventLog
              ? constraint_case_order(*source, source->checked_log()) : std::nullopt)) + '}' :
      variant_conformance ? "{\"type\":\"variant_conformance\",\"value\":" + minera::serialize_log_skeleton_workbench_conformance_json(*variant_conformance) + '}' :
      primary_diagnostics ? "{\"type\":\"diagnostics\",\"value\":" + minera::serialize_event_table_input_json(*primary_diagnostics) + '}' : "null";
    const auto members_json = [&](const std::set<std::string>& members) {
      std::string value = "[";
      for (const auto& member : members) {if (value.size() > 1) value += ','; value += quote(member);}
      return value + ']';
    };
    const std::string options_json = "{\"conformance\":" + (conformance_options ? minera::serialize_log_skeleton_workbench_options_json(*conformance_options) : "null") +
      ",\"diagnostics\":" + (diagnostics_options ? minera::serialize_log_skeleton_workbench_options_json(*diagnostics_options) : "null") + '}';
    std::map<std::string, std::string> exports;
    const auto export_value = [&](const std::string& key, const auto& produce) {
      try {exports[key] = produce();}
      catch (const std::exception& error) {exports[key + "_error"] = error.what();}
    };
    if (model) export_value("model_json", [&] {return minera::serialize_log_skeleton_workbench_model_json(*model);});
    if (source && source->checked_log_if_available())
      export_value("source_xes", [&] {return minera::to_xes(source->checked_log(), "Original log-skeleton source");});
    std::string exports_json = "{";
    const auto optional_export = [&](const std::string& key) {
      const auto found = exports.find(key);
      return found == exports.end() ? std::string("null") : quote(found->second);
    };
    for (const auto& key : {"model_json", "source_xes"}) {
      if (exports_json.size() > 1) exports_json += ',';
      exports_json += quote(key) + ':' + optional_export(key) + ',' + quote(std::string(key) + "_error") + ':' + optional_export(std::string(key) + "_error");
    }
    exports_json += '}';
    const std::string input_json = input_file ? "{\"dataset\":" + quote(input_file->filename) + ',' + original_bytes(input_file->content) +
      ",\"word\":" + (word ? minera::serialize_alignment_trace_words_json(*word) : "null") +
      ",\"trace\":" + (trace ? minera::serialize_declare_workbench_trace_json(*trace) : "null") +
      ",\"variants\":" + (variants ? minera::serialize_log_skeleton_workbench_variants_json(*variants) : "null") + '}' : "null";
    const std::string results_input_json = results_file ? "{\"dataset\":" + quote(results_file->filename) + ',' + original_bytes(results_file->content) +
      ",\"results\":" + results_after + '}' : "null";
    response.status = failure == "null" ? 200 : 400;
    response.set_content(active_source_safe_json("{\"family\":\"ORIGINAL_LOG_SKELETON_WORKBENCH\",\"operation\":" + quote(operation) +
      ",\"executed\":" + flag(executed) + ",\"native_calls\":" + calls + ",\"error\":" + failure +
      ",\"diagnostics_error\":" + diagnostics_error + ",\"requested\":{\"include_diagnostics\":" + flag(include_diagnostics) + '}' +
      ",\"input_context\":{\"model_dataset\":" + (model_upload ? quote(model_upload->filename) : "null") +
      ",\"original_model_bytes\":" + (model_upload ? std::to_string(model_upload->content.size()) : "null") +
      ",\"model_format\":" + quote(format) + ",\"model_unchanged\":" + flag(model_before == (model ? minera::serialize_log_skeleton_workbench_model_json(*model) : "null")) +
      ",\"input_dataset\":" + (input_file ? quote(input_file->filename) : "null") +
      ",\"original_input_bytes\":" + (input_file ? std::to_string(input_file->content.size()) : "null") +
      ",\"input_shape\":" + (input_shape.empty() ? std::string("null") : quote(input_shape)) +
      ",\"source_unchanged\":" + flag(source_before == source_after) +
      ",\"input_unchanged\":" + flag(input_before == input_after && (!input_file || input_file->content == input_bytes_before)) +
      ",\"results_dataset\":" + (results_file ? quote(results_file->filename) : "null") +
      ",\"original_results_bytes\":" + (results_file ? std::to_string(results_file->content.size()) : "null") +
      ",\"results_unchanged\":" + flag(results_before == results_after && (!results_file || results_file->content == results_bytes_before)) + '}' +
      ",\"model\":" + (model ? "{\"original_model\":" + quote(model_upload->content) + ",\"skeleton_model\":" + model_before + '}' : "null") +
      ",\"input\":" + input_json + ",\"diagnostic_results_input\":" + results_input_json + ",\"source\":" + source_after +
      ",\"submitted_options\":" + options_json + ",\"configured_options\":" + options_json +
      ",\"supplied_options\":{\"conformance\":" + members_json(conformance_members) + ",\"diagnostics\":" + members_json(diagnostics_members) + "},\"result\":" + result_json +
      ",\"diagnostics\":" + (diagnostics ? minera::serialize_event_table_input_json(*diagnostics) : "null") +
      ",\"exports\":" + exports_json + '}'), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(active_source_safe_json(error_json(error.what())), "application/json");
  }
}
