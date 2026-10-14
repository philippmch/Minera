#pragma once

// Original models and all conversion/reduction state belong to this request.
// Each operation invokes its exact public overload without reading g_session.
void handle_model_conversion_workbench(const httplib::Request& request,
                                       httplib::Response& response) {
  try {
    if (!request.is_multipart_form_data() || !request.has_file("model_file"))
      throw std::invalid_argument("model conversion requires multipart model_file");
    const auto lookup = [&](std::string_view name) {
      const std::string key(name);
      return request.has_file(key) ? form_field(request, key)
          : request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_file(std::string(name)) || request.has_param(std::string(name));
    };
    const auto operation = present("operation") ? lookup("operation") : "tree_to_powl";
    static const std::set<std::string> operations = {
        "tree_to_powl", "net_to_powl", "powl_to_tree", "net_to_tree", "powl_to_net", "reduce_petri"};
    if (!operations.contains(operation)) throw std::invalid_argument("unsupported model conversion operation: " + operation);
    const bool reduction = operation == "reduce_petri";
    const bool tree_input = operation == "tree_to_powl";
    const bool net_input = reduction || operation == "net_to_powl" || operation == "net_to_tree";
    const auto format = present("model_format") ? lookup("model_format") : "auto";
    static const std::set<std::string> formats = {"auto", "ptml", "tree_json", "pnml", "powl_text"};
    if (!formats.contains(format)) throw std::invalid_argument("unsupported model_format: " + format);
    static const std::set<std::string> pnml_controls = {
        "pnml_infer_initial_marking", "pnml_infer_final_marking", "pnml_import_stochastic_map",
        "pnml_require_workflow_boundaries", "pnml_public_marking_semantics"};
    static const std::set<std::string> reduction_controls = {
        "petri_reduction_initial_marking", "petri_reduction_final_marking",
        "petri_reduction_initial_marking_json", "petri_reduction_final_marking_json",
        "petri_reduction_parallel_place_fusion", "petri_reduction_abstraction", "petri_reduction_preserve_join_bridges"};
    const auto check_control = [&](const std::string& key) {
      if (key == "model_file" || key == "operation" || key == "model_format") return;
      if ((net_input && pnml_controls.contains(key)) || (reduction && reduction_controls.contains(key))) return;
      throw std::invalid_argument("unknown or inapplicable model conversion control: " + key);
    };
    for (const auto& [key, value] : request.params) {(void)value; check_control(key);}
    for (const auto& [key, value] : request.files) {(void)value; check_control(key);}
    const auto& file = request.get_file_value("model_file");
    auto detected = format;
    if (detected == "auto") {
      const auto first = file.content.find_first_not_of(" \t\r\n");
      if (first != std::string::npos && file.content[first] == '{') detected = "tree_json";
      else if (first != std::string::npos && file.content[first] == '<')
        detected = tree_input ? "ptml" : "pnml";
      else detected = "powl_text";
    }
    if ((tree_input && detected != "ptml" && detected != "tree_json") ||
        (net_input && detected != "pnml") || (!tree_input && !net_input && detected != "powl_text"))
      throw std::invalid_argument("model_format is incompatible with operation " + operation);
    minera::PnmlImportOptions imports;
    const auto boolean_option = [&](std::string_view name, bool& value) {
      if (present(name)) value = petri_anti_multi_boolean(lookup(name), name);
    };
    if (net_input) {
      boolean_option("pnml_infer_initial_marking", imports.infer_initial_marking);
      boolean_option("pnml_infer_final_marking", imports.infer_final_marking);
      boolean_option("pnml_import_stochastic_map", imports.import_stochastic_map);
      boolean_option("pnml_require_workflow_boundaries", imports.require_workflow_boundaries);
      boolean_option("pnml_public_marking_semantics", imports.public_marking_semantics);
    }
    std::optional<minera::ProcessTree> source_tree, result_tree;
    std::optional<minera::PowlNode> source_powl, result_powl;
    std::optional<minera::PnmlDocument> source_document;
    std::optional<minera::PetriNet> result_net;
    std::optional<minera::PetriNetReductionResult> result_reduction;
    if (tree_input) source_tree = detected == "tree_json"
        ? minera::parse_process_tree_alignment_input_json(file.content) : minera::parse_ptml(file.content);
    else if (net_input) source_document = minera::parse_pnml_document(file.content, imports);
    else source_powl = minera::PowlNode::parse(file.content);
    const auto snapshot = [&]() {
      if (source_tree) return minera::serialize_process_tree_alignment_input_json(*source_tree);
      if (source_document) return minera::serialize_petri_workbench_document_json(*source_document);
      return minera::serialize_model_conversion_powl_json(*source_powl);
    };
    const auto before = snapshot();
    auto options = reduction ? minera::parse_model_reduction_workbench_params(lookup, present)
                             : minera::PetriNetReductionOptions{};
    const auto marking_origin = [&](std::string_view field, const std::vector<std::int64_t>& values) {
      const auto name = std::string(field);
      if (!present(name) && !present(name + "_json")) return std::string("pnml");
      if (values.empty()) return std::string("explicit_empty_unspecified");
      return present(name + "_json") ? std::string("explicit_json") : std::string("explicit_legacy");
    };
    const auto initial_origin = reduction ? marking_origin("petri_reduction_initial_marking", options.initial_marking) : std::string{};
    const auto final_origin = reduction ? marking_origin("petri_reduction_final_marking", options.final_marking) : std::string{};
    bool executed = false;
    std::string native_error;
    try {
      if (operation == "tree_to_powl") result_powl = minera::to_powl(*source_tree);
      else if (operation == "net_to_powl") result_powl = minera::to_powl(source_document->net);
      else if (operation == "powl_to_tree") result_tree = minera::to_process_tree(*source_powl);
      else if (operation == "net_to_tree") result_tree = minera::to_process_tree(source_document->net);
      else if (operation == "powl_to_net") result_net = minera::to_petri_net(*source_powl);
      else {
        if (initial_origin == "pnml") options.initial_marking = minera::tools::pnml_marking<std::int64_t>(source_document->initial_marking);
        if (final_origin == "pnml") options.final_marking = minera::tools::pnml_marking<std::int64_t>(source_document->final_marking);
        result_reduction = minera::reduce_petri_net(source_document->net, options);
      }
      executed = true;
    } catch (const std::exception& error) {native_error = error.what();}
    const auto quote = minera::embedding_runtime::detail::escape_json;
    const auto flag = [](bool value) {return value ? "true" : "false";};
    const auto text = [&](const std::string& value) {return value.empty() ? std::string("null") : quote(value);};
    std::map<std::string, std::string> exports;
    const auto exported = [&](const std::string& key, const auto& produce) {
      try {exports[key] = produce();}
      catch (const std::exception& error) {exports[key + "_error"] = error.what();}
    };
    const auto export_tree = [&](const std::string& key, const minera::ProcessTree& tree) {
      if (!process_tree_alignment_export_indices_safe(tree)) {
        exports[key + "_error"] = "PTML export is unavailable because an activity leaf index is outside the native activity registry.";
        return;
      }
      exported(key, [&] {return minera::to_ptml(tree, "Independent model conversion");});
    };
    if (source_document) exported("source_pnml", [&] {return minera::to_pnml(*source_document, "Original imported PNML model");});
    if (source_tree) export_tree("source_ptml", *source_tree);
    if (source_powl) exported("source_powl", [&] {return source_powl->serialize();});
    if (result_powl) exported("result_powl", [&] {return result_powl->serialize();});
    if (result_tree) export_tree("result_ptml", *result_tree);
    if (result_net) exported("result_pnml", [&] {return minera::to_pnml(*result_net, "Converted POWL model");});
    if (result_reduction) exported("result_pnml", [&] {
      // Original document IDs/arc metadata do not describe the reduced net.
      return minera::to_pnml(result_reduction->net, result_reduction->markings.initial,
                             result_reduction->markings.final, "Reduced original PNML model");
    });
    const auto tree_json = [&](const std::optional<minera::ProcessTree>& tree) {
      return tree ? minera::serialize_process_tree_alignment_input_json(*tree) : std::string("null");
    };
    const auto powl_json = [&](const std::optional<minera::PowlNode>& powl) {
      return powl ? minera::serialize_model_conversion_powl_json(*powl) : std::string("null");
    };
    std::string exports_json = "{";
    for (const auto& key : {"source_pnml", "source_ptml", "source_powl", "result_pnml", "result_ptml", "result_powl"}) {
      if (exports_json.size() > 1) exports_json += ',';
      exports_json += quote(key) + ':' + text(exports[key]) + ',' + quote(std::string(key) + "_error") + ':' + text(exports[std::string(key) + "_error"]);
    }
    exports_json += '}';
    const auto result = executed ? "{\"process_tree\":" + tree_json(result_tree) + ",\"powl\":" + powl_json(result_powl) +
        ",\"petri_net\":" + (result_net ? minera::workflow_input_petri_json(*result_net) : "null") +
        ",\"reduction\":" + (result_reduction ? minera::serialize_model_reduction_result_json(*result_reduction) : "null") + '}' : std::string("null");
    const auto imports_json = net_input ? std::string("{\"infer_initial_marking\":") + flag(imports.infer_initial_marking) +
        ",\"infer_final_marking\":" + flag(imports.infer_final_marking) + ",\"import_stochastic_map\":" + flag(imports.import_stochastic_map) +
        ",\"require_workflow_boundaries\":" + flag(imports.require_workflow_boundaries) + ",\"public_marking_semantics\":" + flag(imports.public_marking_semantics) + '}' : std::string("null");
    response.status = native_error.empty() ? 200 : 400;
    response.set_content(workflow_input_safe_json("{\"family\":\"MODEL_CONVERSION_WORKBENCH\",\"operation\":" + quote(operation) +
        ",\"executed\":" + flag(executed) + ",\"error\":" + (native_error.empty() ? "null" : "{\"type\":\"native_error\",\"message\":" + quote(native_error) + "}") +
        ",\"input_context\":{\"format\":" + quote(format) + ",\"detected_format\":" + quote(detected) + ",\"dataset\":" + quote(file.filename) +
        ",\"original_model_bytes\":" + std::to_string(file.content.size()) + ",\"model_unchanged\":" + flag(before == snapshot()) +
        ",\"initial_marking_origin\":" + text(initial_origin) + ",\"final_marking_origin\":" + text(final_origin) + '}' +
        ",\"options\":" + (reduction ? minera::serialize_model_reduction_options_json(options) : "null") + ",\"pnml_import_options\":" + imports_json +
        ",\"model\":{\"process_tree\":" + tree_json(source_tree) + ",\"powl\":" + powl_json(source_powl) +
        ",\"petri_net\":" + (source_document ? minera::workflow_input_petri_json(source_document->net) : "null") +
        ",\"pnml_document\":" + (source_document ? before : "null") + ",\"original_model\":" + quote(file.content) + '}' +
        ",\"result\":" + result + ",\"exports\":" + exports_json + '}'), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(workflow_input_safe_json(error_json(error.what())), "application/json");
  }
}
