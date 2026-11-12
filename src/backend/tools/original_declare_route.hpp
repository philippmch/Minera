#pragma once

// Every normative model and input belongs to this request. Native overloads
// consume their selected source shape; no cached model or synthetic log is used.
void handle_original_declare_workbench(const httplib::Request& request,
                                       httplib::Response& response) {
  try {
    if (!request.is_multipart_form_data() || !request.has_file("model_file"))
      throw std::invalid_argument("Declare workbench requires multipart model_file");
    const auto supplied = [&](std::string_view name) {return active_source_supplied(request, name);};
    const auto field = [&](std::string_view name) {return active_source_field(request, name);};
    for (const auto* name : {"model_file", "model_format"})
      if (request.params.count(name) + request.files.count(name) > 1)
        throw std::invalid_argument(std::string("duplicate Declare control: ") + name);
    const auto format = supplied("model_format") ? field("model_format") : "exact";
    const auto& model_file = request.get_file_value("model_file");
    const auto model = [&] {
      if (format == "exact") return minera::parse_declare_workbench_model(model_file.content);
      if (format == "stream_compatibility") return minera::parse_declare_stream_model_json(model_file.content);
      throw std::invalid_argument("Declare model_format must be exact or stream_compatibility");
    }();
    const auto model_before = minera::serialize_declare_workbench_model_json(model);
    const auto operation = supplied("operation") ? field("operation") : "conformance";
    const bool multiple = operation == "conformance" || operation == "conformance_view";
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
    std::optional<std::vector<std::string>> word;
    std::optional<minera::Trace> trace;
    std::optional<minera::ActiveEventSource> source;
    std::optional<minera::DeclareConformanceOptions> conformance_options;
    std::optional<minera::DeclareDiagnosticsOptions> diagnostics_options;
    // The options as submitted, echoed in the response; a CSV source's calls
    // read the standard keys as its selected columns (round BO).
    std::optional<minera::DeclareConformanceOptions> submitted_conformance;
    std::optional<minera::DeclareDiagnosticsOptions> submitted_diagnostics;
    std::optional<minera::DeclarePlayoutOptions> playout_options;
    std::set<std::string> conformance_members, diagnostics_members, playout_members;
    bool include_diagnostics = false, check_generated = false, executed = false;
    std::string input_shape = multiple ? (supplied("mining_input_shape") ? field("mining_input_shape") : "log") :
      operation == "conformance_word" ? "word" : operation == "conformance_trace" ? "trace" : "";
    const auto table_json = [](const minera::EventTable* value) {
      return value ? minera::serialize_event_table_input_json(*value) : std::string("null");
    };
    const auto log_json = [](const minera::EventLog* value) {
      return value ? minera::serialize_active_source_log_json(*value) : std::string("null");
    };
    const auto input_snapshot = [&] {
      if (word) return minera::serialize_alignment_trace_words_json(*word);
      if (trace) return minera::serialize_declare_workbench_trace_json(*trace);
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
    CsvFrameView frame;
    const auto source_view = [&] {return frame.view ? *frame.view : source->selected_view();};
    const auto source_table = [&]() -> const minera::EventTable& {return frame.table ? *frame.table : source->checked_table();};
    const auto source_lazy = [&]() -> const minera::LazyEventTable& {return frame.lazy ? *frame.lazy : source->checked_lazy();};
    std::optional<minera::DeclareConformance> scalar;
    std::optional<std::vector<minera::DeclareConformance>> conformances, generated_conformance, listed;
    std::optional<minera::EventLog> generated;
    std::optional<minera::EventTable> diagnostics;
    std::string failure = "null", diagnostics_error = "null", generated_check_error = "null";
    std::string calls = "[", stage = "configuration_error", source_before = "null", input_before = "null";
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
        "model_file", "input_file", "operation", "model_format", "conformance_options_json", "diagnostics_options_json",
        "playout_options_json", "include_diagnostics", "check_generated"};
      const auto check_control = [&](const std::string& key) {
        if (request.params.count(key) + request.files.count(key) != 1)
          throw std::invalid_argument("duplicate Declare control: " + key);
        if (!controls.contains(key) && !source_controls.contains(key))
          throw std::invalid_argument("unknown Declare control: " + key);
        if (source_controls.contains(key) && !multiple)
          throw std::invalid_argument("source control is inapplicable to this Declare operation: " + key);
      };
      for (const auto& [key, value] : request.params) {(void)value; check_control(key);}
      for (const auto& [key, value] : request.files) {(void)value; check_control(key);}
      if (!multiple && operation != "conformance_word" && operation != "conformance_trace" && operation != "playout")
        throw std::invalid_argument("unsupported Declare operation: " + operation);
      if (operation == "playout" ? supplied("input_file") : !input_file)
        throw std::invalid_argument(operation == "playout" ? "input_file is inapplicable to Declare playout" : "Declare conformance requires input_file");
      if (supplied("include_diagnostics")) include_diagnostics = petri_anti_multi_boolean(field("include_diagnostics"), "include_diagnostics");
      if (supplied("check_generated")) check_generated = petri_anti_multi_boolean(field("check_generated"), "check_generated");
      if (supplied("check_generated") && operation != "playout")
        throw std::invalid_argument("check_generated is only available for Declare playout");
      if (supplied("include_diagnostics") && !multiple && !(operation == "playout" && check_generated))
        throw std::invalid_argument("include_diagnostics requires multiple-source conformance or checked Declare playout");
      if (supplied("diagnostics_options_json") && !include_diagnostics)
        throw std::invalid_argument("diagnostics_options_json requires include_diagnostics=true");
      if (supplied("conformance_options_json") && !(multiple || operation == "conformance_trace" || (operation == "playout" && check_generated)))
        throw std::invalid_argument("conformance_options_json is inapplicable to this Declare operation");
      if (supplied("playout_options_json") && operation != "playout")
        throw std::invalid_argument("playout_options_json is only available for Declare playout");
      if (multiple && input_shape != "log" && input_shape != "stream" && input_shape != "dataframe" && input_shape != "lazyframe")
        throw std::invalid_argument("mining_input_shape must be log, stream, dataframe or lazyframe");
      if (multiple || operation == "conformance_trace" || (operation == "playout" && check_generated))
        conformance_options = minera::parse_declare_conformance_workbench_options(supplied("conformance_options_json") ? field("conformance_options_json") : "{}", &conformance_members);
      if (include_diagnostics)
        diagnostics_options = minera::parse_declare_diagnostics_workbench_options(supplied("diagnostics_options_json") ? field("diagnostics_options_json") : "{}", &diagnostics_members);
      if (operation == "playout")
        playout_options = minera::parse_declare_playout_workbench_options(supplied("playout_options_json") ? field("playout_options_json") : "{}", &playout_members);
      // Import controls configure the owned source only. Native option groups
      // keep their own literal selector defaults and are never overlaid.
      auto imports = minera::parse_mine_params(field, supplied);
      stage = "preparation_error";
      if (operation == "conformance_word") word = minera::parse_alignment_trace_words_json(input_file->content);
      else if (operation == "conformance_trace") trace = minera::parse_declare_workbench_trace_json(input_file->content);
      else if (multiple) {
        source = prepare_active_event_source(request, input_file->content, input_file->filename, imports);
        source_before = source_snapshot();
        (void)source->selected_view();
        // pm4py reads a CSV upload's table shapes as format_dataframe's frame,
        // in its row order (round BN).
        prepare_csv_frame_view(*source, frame);
        // pm4py's frame of a CSV upload holds the standard keys as copies of
        // its selected columns (round BO, csv_frame_key).
        submitted_conformance = conformance_options;
        submitted_diagnostics = diagnostics_options;
        using Role = minera::FirstClassAttributeRole;
        if (conformance_options) {
          conformance_options->activity_key = csv_frame_key(*source, Role::kActivity, conformance_options->activity_key);
          conformance_options->case_id_key = csv_frame_key(*source, Role::kCaseId, conformance_options->case_id_key);
        }
        if (diagnostics_options)
          diagnostics_options->case_id_key = csv_frame_key(*source, Role::kCaseId, diagnostics_options->case_id_key);
      }
      input_before = input_snapshot();
      stage = "native_error";
      if (operation == "conformance_word")
        invoke("check_declare_conformance(word, model)", [&] {scalar = minera::check_declare_conformance(*word, model);});
      else if (operation == "conformance_trace")
        invoke("check_declare_conformance(trace, model, options)", [&] {scalar = minera::check_declare_conformance(*trace, model, *conformance_options);});
      else if (operation == "conformance_view")
        invoke("check_declare_conformance(source_view, model, options)", [&] {conformances = minera::check_declare_conformance(source_view(), model, *conformance_options);});
      else if (operation == "conformance") {
        switch (source->selected_kind()) {
          case minera::EventDataKind::kEventLog:
            invoke("check_declare_conformance(source_log, model, options)", [&] {conformances = minera::check_declare_conformance(source->checked_log(), model, *conformance_options);}); break;
          case minera::EventDataKind::kEventStream:
            invoke("check_declare_conformance(source_stream, model, options)", [&] {conformances = minera::check_declare_conformance(frame.stream ? *frame.stream : source->checked_stream(), model, *conformance_options);}); break;
          case minera::EventDataKind::kEventTable:
            invoke("check_declare_conformance(source_table, model, options)", [&] {conformances = minera::check_declare_conformance(source_table(), model, *conformance_options);}); break;
          case minera::EventDataKind::kLazyEventTable:
            invoke("check_declare_conformance(source_lazy, model, options)", [&] {conformances = minera::check_declare_conformance(source_lazy(), model, *conformance_options);}); break;
        }
      } else
        invoke("declare_playout(model, options)", [&] {generated = minera::declare_playout(model, *playout_options);});
      // pm4py's DataFrame conformance list reads each case of an XES upload
      // as its slice of the frame's rows (traces that share a name apart in
      // the file); the diagnostics table below keeps the merged cases.
      if (conformances && source && source->file().format == "xes" &&
          source->selected_kind() != minera::EventDataKind::kEventStream) {
        const auto view = [&]() -> minera::EventDataView {
          if (operation == "conformance_view") return source->selected_view();
          switch (source->selected_kind()) {
            case minera::EventDataKind::kEventLog: return minera::EventDataView(source->checked_log());
            case minera::EventDataKind::kEventTable: return minera::EventDataView(source->checked_table());
            case minera::EventDataKind::kLazyEventTable: return minera::EventDataView(source->checked_lazy());
            case minera::EventDataKind::kEventStream: break;
          }
          return source->selected_view();
        }();
        listed = minera::check_declare_conformance_frame_slices(view, model, *conformance_options);
      }
      executed = true;
      if (generated && check_generated) {
        try {
          invoke("check_declare_conformance(generated_log, model, options)", [&] {generated_conformance = minera::check_declare_conformance(*generated, model, *conformance_options);});
        } catch (const std::exception& error) {generated_check_error = error_value("native_error", error.what());}
      }
      if (include_diagnostics && (conformances || generated_conformance)) {
        try {
          if (generated_conformance)
            invoke("declare_conformance_diagnostics(generated_log, results, options)", [&] {diagnostics = minera::declare_conformance_diagnostics(*generated, *generated_conformance, *diagnostics_options);});
          else if (operation == "conformance_view")
            invoke("declare_conformance_diagnostics(source_view, results, options)", [&] {diagnostics = minera::declare_conformance_diagnostics(source_view(), *conformances, *diagnostics_options);});
          else {
            switch (source->selected_kind()) {
              case minera::EventDataKind::kEventLog:
                invoke("declare_conformance_diagnostics(source_log, results, options)", [&] {diagnostics = minera::declare_conformance_diagnostics(source->checked_log(), *conformances, *diagnostics_options);}); break;
              case minera::EventDataKind::kEventStream:
                invoke("declare_conformance_diagnostics(source_stream, results, options)", [&] {diagnostics = minera::declare_conformance_diagnostics(frame.stream ? *frame.stream : source->checked_stream(), *conformances, *diagnostics_options);}); break;
              case minera::EventDataKind::kEventTable:
                invoke("declare_conformance_diagnostics(source_table, results, options)", [&] {diagnostics = minera::declare_conformance_diagnostics(source_table(), *conformances, *diagnostics_options);}); break;
              case minera::EventDataKind::kLazyEventTable:
                invoke("declare_conformance_diagnostics(source_lazy, results, options)", [&] {diagnostics = minera::declare_conformance_diagnostics(source_lazy(), *conformances, *diagnostics_options);}); break;
            }
          }
        } catch (const std::exception& error) {diagnostics_error = error_value("native_error", error.what());}
      }
    } catch (const std::exception& error) {failure = error_value(stage, error.what());}
    calls += ']';
    const auto source_after = source_snapshot();
    const auto input_after = input_snapshot();
    const std::string result_json = scalar ? "{\"type\":\"conformance\",\"value\":" + minera::serialize_declare_workbench_conformance_json(*scalar) + '}' :
      conformances ? "{\"type\":\"conformance_vector\",\"value\":" + minera::serialize_declare_workbench_conformance_json(
          // An XES source's list in pm4py's DataFrame case order; the derived
          // diagnostics table keeps the frame's (file) order.
          in_constraint_order(listed ? *listed : *conformances, source && conformance_options
              ? constraint_case_order(*source, source->selected_view(), conformance_options->activity_key,
                                      conformance_options->case_id_key)
              : std::nullopt)) + '}' :
      generated ? "{\"type\":\"event_log\",\"value\":" + minera::serialize_active_source_log_json(*generated) + '}' : "null";
    const auto members_json = [&](const std::set<std::string>& members) {
      std::string value = "[";
      for (const auto& member : members) {if (value.size() > 1) value += ','; value += quote(member);}
      return value + ']';
    };
    if (submitted_conformance) conformance_options = submitted_conformance;
    if (submitted_diagnostics) diagnostics_options = submitted_diagnostics;
    const std::string options_json = "{\"conformance\":" + (conformance_options ? minera::serialize_declare_workbench_options_json(*conformance_options) : "null") +
      ",\"diagnostics\":" + (diagnostics_options ? minera::serialize_declare_workbench_options_json(*diagnostics_options) : "null") +
      ",\"playout\":" + (playout_options ? minera::serialize_declare_workbench_options_json(*playout_options) : "null") + '}';
    std::map<std::string, std::string> exports;
    const auto export_value = [&](const std::string& key, const auto& produce) {
      try {exports[key] = produce();}
      catch (const std::exception& error) {exports[key + "_error"] = error.what();}
    };
    export_value("model_json", [&] {return minera::serialize_declare_workbench_model_json(model);});
    if (source && source->checked_log_if_available())
      export_value("source_xes", [&] {return minera::to_xes(source->checked_log(), "Original Declare source");});
    if (generated)
      export_value("generated_xes", [&] {return minera::to_xes(*generated, "Declare playout");});
    std::string exports_json = "{";
    const auto optional_export = [&](const std::string& key) {
      const auto found = exports.find(key);
      return found == exports.end() ? std::string("null") : quote(found->second);
    };
    for (const auto& key : {"model_json", "source_xes", "generated_xes"}) {
      if (exports_json.size() > 1) exports_json += ',';
      exports_json += quote(key) + ':' + optional_export(key) + ',' + quote(std::string(key) + "_error") + ':' + optional_export(std::string(key) + "_error");
    }
    exports_json += '}';
    const std::string input_json = input_file ? "{\"dataset\":" + quote(input_file->filename) + ',' + original_bytes(input_file->content) +
      ",\"word\":" + (word ? minera::serialize_alignment_trace_words_json(*word) : "null") +
      ",\"trace\":" + (trace ? minera::serialize_declare_workbench_trace_json(*trace) : "null") + '}' : "null";
    response.status = failure == "null" ? 200 : 400;
    response.set_content(active_source_safe_json("{\"family\":\"ORIGINAL_DECLARE_WORKBENCH\",\"operation\":" + quote(operation) +
      ",\"executed\":" + flag(executed) + ",\"native_calls\":" + calls + ",\"error\":" + failure +
      ",\"diagnostics_error\":" + diagnostics_error + ",\"generated_check_error\":" + generated_check_error +
      ",\"requested\":{\"include_diagnostics\":" + flag(include_diagnostics) + ",\"check_generated\":" + flag(check_generated) + '}' +
      ",\"input_context\":{\"model_dataset\":" + quote(model_file.filename) + ",\"original_model_bytes\":" + std::to_string(model_file.content.size()) +
      ",\"model_format\":" + quote(format) + ",\"model_unchanged\":" + flag(model_before == minera::serialize_declare_workbench_model_json(model)) +
      ",\"input_dataset\":" + (input_file ? quote(input_file->filename) : "null") +
      ",\"original_input_bytes\":" + (input_file ? std::to_string(input_file->content.size()) : "null") +
      ",\"input_shape\":" + (input_shape.empty() ? std::string("null") : quote(input_shape)) +
      ",\"source_unchanged\":" + flag(source_before == source_after) +
      ",\"input_unchanged\":" + flag(input_before == input_after && (!input_file || input_file->content == input_bytes_before)) + '}' +
      ",\"model\":{\"original_model\":" + quote(model_file.content) + ",\"declare_model\":" + model_before + '}' +
      ",\"input\":" + input_json + ",\"source\":" + source_after +
      ",\"submitted_options\":" + options_json + ",\"configured_options\":" + options_json +
      ",\"supplied_options\":{\"conformance\":" + members_json(conformance_members) + ",\"diagnostics\":" + members_json(diagnostics_members) +
      ",\"playout\":" + members_json(playout_members) + "},\"result\":" + result_json +
      ",\"generated_conformance\":" + (generated_conformance ? minera::serialize_declare_workbench_conformance_json(*generated_conformance) : "null") +
      ",\"diagnostics\":" + (diagnostics ? minera::serialize_event_table_input_json(*diagnostics) : "null") +
      ",\"exports\":" + exports_json + '}'), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(active_source_safe_json(error_json(error.what())), "application/json");
  }
}
