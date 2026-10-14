#pragma once

// Included inside the server's implementation namespace after its import and
// response helpers. This source-free route never reads or replaces g_session.
std::string workflow_input_safe_json(std::string json) {
  // Old serializers can emit the native WTF-8 spelling of a JSON surrogate.
  // Escape only those sequences for this new response so UTF-8 browser decoding
  // retains the original UTF-16 code unit instead of replacing the label.
  constexpr char hex[] = "0123456789abcdef";
  std::string output;
  output.reserve(json.size());
  for (std::size_t i = 0; i < json.size(); ++i) {
    const auto first = static_cast<unsigned char>(json[i]);
    if (first == 0xed && i + 2 < json.size()) {
      const auto second = static_cast<unsigned char>(json[i + 1]);
      const auto third = static_cast<unsigned char>(json[i + 2]);
      if (second >= 0xa0 && second <= 0xbf && third >= 0x80 && third <= 0xbf) {
        const auto unit = ((first & 0x0f) << 12) | ((second & 0x3f) << 6) | (third & 0x3f);
        output += "\\u";
        for (int shift = 12; shift >= 0; shift -= 4) output += hex[(unit >> shift) & 0x0f];
        i += 2;
        continue;
      }
    }
    output += json[i];
  }
  return output;
}

minera::EventLog workflow_input_selected_log(const minera::EventTable& table,
                                           const std::string& case_selector) {
  minera::LogConversionOptions options;
  options.case_id_key = case_selector;
  options.include_case_attributes = false;
  auto stream = minera::event_table_to_event_stream(table, options);
  for (auto& record : stream.events) for (auto& field : record.fields) {
    if (field.first_class_role != minera::FirstClassAttributeRole::kCaseId ||
        field.key == case_selector || field.first_class_source_key == case_selector) continue;
    field.first_class_role = minera::FirstClassAttributeRole::kNone;
    field.first_class_state = minera::FirstClassAttributeState::kMissing;
    field.first_class_source_key.clear();
  }
  return minera::event_stream_to_event_log(stream, options);
}

minera::DirectlyFollowsGraph workflow_input_graph(
    std::vector<std::string> names, const minera::DfgFrequency& edges,
    const std::map<std::string, std::uint64_t>& starts = {},
    const std::map<std::string, std::uint64_t>& ends = {}) {
  std::map<std::string, std::size_t> ids;
  for (std::size_t i = 0; i < names.size(); ++i) ids.emplace(names[i], i);
  const auto add = [&](const std::string& name) {
    if (!ids.contains(name)) {ids.emplace(name, names.size()); names.push_back(name);}
  };
  for (const auto& [pair, count] : edges) {(void)count; add(pair.first); add(pair.second);}
  for (const auto& [name, count] : starts) {(void)count; add(name);}
  for (const auto& [name, count] : ends) {(void)count; add(name);}
  std::vector<minera::DirectlyFollowsEdge> paths;
  for (const auto& [pair, count] : edges) paths.push_back({ids.at(pair.first), ids.at(pair.second), count});
  std::vector<std::uint64_t> initial(names.size()), final(names.size());
  for (const auto& [name, count] : starts) initial[ids.at(name)] = count;
  for (const auto& [name, count] : ends) final[ids.at(name)] = count;
  return minera::DirectlyFollowsGraph::from_frequencies(names, paths, initial, final);
}

void handle_workflow_input_discovery(const httplib::Request& request,
                                     httplib::Response& response) {
  try {
    if (!request.is_multipart_form_data() || !request.has_file("input"))
      throw std::invalid_argument("workflow input discovery requires multipart input");
    const auto lookup = [&](std::string_view name) {
      const std::string key(name);
      return request.has_file(key) ? form_field(request, key)
          : request.has_param(key) ? request.get_param_value(key) : std::string{};
    };
    const auto present = [&](std::string_view name) {
      return request.has_file(std::string(name)) || request.has_param(std::string(name));
    };
    const auto family = lookup("discovery_family"), adapter = lookup("input_shape");
    auto format = present("input_format") ? lookup("input_format") : "auto";
    const auto requested_format = format;
    if (family != "alpha" && family != "heuristic")
      throw std::invalid_argument("discovery_family must be alpha or heuristic");
    if (adapter != "log" && adapter != "stream" && adapter != "dataframe" &&
        adapter != "lazyframe" && adapter != "dfg" && adapter != "pandas_mutable")
      throw std::invalid_argument("unsupported workflow input_shape");
    if (format != "auto" && format != "table" && format != "dfg_json")
      throw std::invalid_argument("input_format must be auto,table or dfg_json");
    if ((adapter == "dfg") != (format == "dfg_json"))
      throw std::invalid_argument("DFG shape requires dfg_json and event shapes require event inputs");
    if (adapter == "pandas_mutable" && family != "heuristic")
      throw std::invalid_argument("mutable pandas discovery requires heuristic PLUSPLUS");
    static const std::set<std::string> controls = {
      "discovery_family", "input_shape", "input_format", "discovery_selectors_json",
      "case_column", "activity_column", "timestamp_column", "resource_column", "lifecycle_column", "delimiter",
      "xes_variant", "xes_case_attribute", "xes_activity_attribute", "xes_timestamp_attribute", "xes_resource_attribute", "xes_lifecycle_attribute",
      "xes_require_timestamp", "xes_timestamp_sort", "xes_reverse_sort", "xes_max_traces", "xes_attributes_to_read", "xes_max_decompressed_bytes",
      "alpha_variant", "alpha_activity_attribute", "alpha_case_id_attribute", "alpha_timestamp_attribute", "alpha_start_timestamp_attribute",
      "alpha_sort_caseid_required", "alpha_sort_timestamp_along_case_id", "alpha_remove_unconnected", "alpha_max_rows_to_materialize",
      "heuristic_variant", "heuristic_activity_attribute", "heuristic_case_id_attribute", "heuristic_timestamp_attribute", "heuristic_start_timestamp_attribute",
      "heuristic_lifecycle_instance_attribute", "dependency_threshold", "and_threshold", "min_edge_frequency", "heuristic_min_activity_count", "heuristic_connect_all",
      "heuristic_decoration", "heuristic_performance_aggregation", "heuristic_business_hours", "heuristic_business_hour_slots",
      "heuristic_dfg_pre_cleaning_noise_threshold", "heuristic_loop_length_two_threshold", "heuristic_keep_once_per_case", "heuristic_most_common_paths",
      "heuristic_plusplus_decoration_selector", "heuristic_plusplus_decoration_value", "heuristic_plusplus_min_activity_count", "heuristic_plusplus_min_frequency"};
    const auto check_control = [&](const std::string& key) {
      if (key == "input") return;
      if (!controls.contains(key)) throw std::invalid_argument("unknown workflow input control: " + key);
      if ((family == "alpha" && key.starts_with("heuristic_")) ||
          (family == "heuristic" && key.starts_with("alpha_")))
        throw std::invalid_argument("discovery control belongs to another family: " + key);
    };
    for (const auto& [key, value] : request.params) {(void)value; check_control(key);}
    for (const auto& [key, value] : request.files) {(void)value; check_control(key);}
    const auto selectors = present("discovery_selectors_json")
        ? minera::parse_workflow_selector_strings_json(lookup("discovery_selectors_json"))
        : std::map<std::string, std::string>{};
    static const std::set<std::string> alpha_selectors = {"alpha_activity_attribute", "alpha_case_id_attribute", "alpha_timestamp_attribute", "alpha_start_timestamp_attribute"};
    static const std::set<std::string> heuristic_selectors = {"heuristic_activity_attribute", "heuristic_case_id_attribute", "heuristic_timestamp_attribute", "heuristic_start_timestamp_attribute", "heuristic_lifecycle_instance_attribute", "heuristic_plusplus_decoration_selector"};
    for (const auto& [key, value] : selectors) {
      (void)value;
      if (!(family == "alpha" ? alpha_selectors : heuristic_selectors).contains(key))
        throw std::invalid_argument("unsupported literal discovery selector: " + key);
    }
    auto params = minera::parse_mine_params(
        [&](std::string_view key) {return selectors.contains(std::string(key)) ? std::string{} : lookup(key);},
        [&](std::string_view key) {return !selectors.contains(std::string(key)) && present(key);});
    const auto supplied = [&](const std::string& key) {return present(key) || selectors.contains(key);};
    const auto& file = request.get_file_value("input");
    minera::EventLog source_log;
    std::optional<minera::EventTable> table;
    std::optional<std::size_t> original_cases;
    std::string origin, dfg_json = "null";
    std::optional<minera::AlphaDfgDiscoveryInput> alpha_dfg;
    std::optional<minera::HeuristicClassicDfgInput> heuristic_dfg;
    if (format == "dfg_json") {
      origin = "uploaded_dfg";
      if (family == "alpha") {alpha_dfg = minera::parse_alpha_dfg_input_json(file.content); dfg_json = minera::serialize_alpha_dfg_input_json(*alpha_dfg);}
      else {heuristic_dfg = minera::parse_heuristic_dfg_input_json(file.content); dfg_json = minera::serialize_heuristic_dfg_input_json(*heuristic_dfg);}
    } else if (format == "table") {
      table = minera::parse_event_table_json(file.content);
      origin = "uploaded_typed_table";
    } else {
      const bool is_xes = looks_like_xes(file.filename, file.content);
      std::istringstream input(file.content);
      if (is_xes) {source_log = minera::import_xes_event_log(input, params.xes); format = "xes"; origin = "uploaded_grouped_log";}
      else {
        auto imported = minera::import_csv_event_data(input, params.csv);
        source_log = std::move(imported.log); table = std::move(imported.table);
        format = "csv"; origin = "uploaded_csv_rows";
        if (!supplied("alpha_activity_attribute")) params.alpha.activity_attribute = params.csv.activity_column;
        if (!supplied("alpha_case_id_attribute")) params.alpha.case_id_attribute = params.csv.case_id_column;
        if (!supplied("alpha_timestamp_attribute")) params.alpha.timestamp_attribute = params.csv.timestamp_column;
        if (!supplied("heuristic_activity_attribute")) params.heuristic.activity_attribute = params.csv.activity_column;
        if (!supplied("heuristic_case_id_attribute")) params.heuristic.case_id_attribute = params.csv.case_id_column;
        if (!supplied("heuristic_timestamp_attribute")) params.heuristic.timestamp_attribute = params.csv.timestamp_column;
      }
      original_cases = source_log.trace_count();
    }
    for (const auto& [key, value] : selectors) {
      if (key == "alpha_activity_attribute") params.alpha.activity_attribute = value;
      else if (key == "alpha_case_id_attribute") params.alpha.case_id_attribute = value;
      else if (key == "alpha_timestamp_attribute") params.alpha.timestamp_attribute = value;
      else if (key == "alpha_start_timestamp_attribute") params.alpha.start_timestamp_attribute = value;
      else if (key == "heuristic_activity_attribute") params.heuristic.activity_attribute = value;
      else if (key == "heuristic_case_id_attribute") params.heuristic.case_id_attribute = value;
      else if (key == "heuristic_timestamp_attribute") params.heuristic.timestamp_attribute = value;
      else if (key == "heuristic_start_timestamp_attribute") params.heuristic.start_timestamp_attribute = value;
      else if (key == "heuristic_lifecycle_instance_attribute") params.heuristic.lifecycle_instance_attribute = value;
      else params.heuristic.plusplus_decoration_selector = value;
    }
    if (!table && adapter != "dfg" && adapter != "log") table = minera::event_log_to_table(source_log);
    const auto before = table ? minera::serialize_event_table_input_json(*table) : "null";
    const auto original_events = table ? table->size() : source_log.event_count();
    const auto variant = family == "alpha" ? (params.alpha.short_loops ? "plus" : "classic") :
        (params.heuristic.variant == minera::HeuristicMinerVariant::kPlusPlus ? "plusplus" : "classic");
    std::optional<minera::AlphaPlusDiscoveryResult> plus;
    std::optional<minera::HeuristicsNet> heuristics;
    std::optional<minera::PetriNet> net;
    bool computed = false;
    std::string error_type, error_message, conversion_error, legacy_result, native_net = "null", native_heuristics = "null";
    try {
      if (adapter == "dfg") {
        if (family == "alpha") {
          if (alpha_dfg->kind == minera::AlphaDfgDiscoveryInput::Kind::kGraph)
            net = minera::discover_petri_net(workflow_input_graph(alpha_dfg->activities, alpha_dfg->dfg, *alpha_dfg->start_activities, *alpha_dfg->end_activities), params.alpha);
          else if (alpha_dfg->kind == minera::AlphaDfgDiscoveryInput::Kind::kClean)
            net = minera::discover_petri_net(minera::DfgCleanAnalysis{alpha_dfg->dfg, *alpha_dfg->start_activities, *alpha_dfg->end_activities}, params.alpha);
          else if (alpha_dfg->start_activities)
            net = minera::discover_petri_net(alpha_dfg->dfg, *alpha_dfg->start_activities, *alpha_dfg->end_activities, params.alpha);
          else net = minera::discover_petri_net(alpha_dfg->dfg, params.alpha);
        } else heuristics = minera::discover_heuristics_net(*heuristic_dfg, params.heuristic);
      } else if (adapter == "pandas_mutable") {
        if (params.heuristic.variant != minera::HeuristicMinerVariant::kPlusPlus)
          throw std::invalid_argument("mutable pandas shape requires PLUSPLUS");
        heuristics = minera::discover_plus_plus_pandas(*table, params.heuristic);
      } else {
        std::optional<minera::EventStream> stream;
        std::optional<minera::LazyEventTable> lazy;
        std::optional<minera::EventLog> grouped;
        if (adapter == "stream") stream = minera::event_table_to_event_stream(*table);
        if (adapter == "lazyframe") lazy = minera::LazyEventTable::from_table(*table);
        if (adapter == "log" && format == "table") {
          grouped = workflow_input_selected_log(*table,
              family == "alpha" ? params.alpha.case_id_attribute : params.heuristic.case_id_attribute);
        }
        const auto data = adapter == "log" ? minera::EventDataView(grouped ? *grouped : source_log)
            : adapter == "stream" ? minera::EventDataView(*stream)
            : adapter == "dataframe" ? minera::EventDataView(*table) : minera::EventDataView(*lazy);
        if (family == "alpha") {
          if (params.alpha.short_loops) {plus = minera::discover_alpha_plus_result(data, params.alpha); net = plus->net;}
          else net = minera::discover_petri_net(data, params.alpha);
        } else heuristics = minera::discover_heuristics_net(data, params.heuristic);
      }
      computed = true;
    } catch (const minera::HeuristicPandasError& error) {error_type = error.reference_type(); error_message = error.what();}
      catch (const std::exception& error) {error_type = "native_error"; error_message = error.what();}
    if (computed && heuristics) {
      native_heuristics = minera::workflow_input_heuristics_json(*heuristics);
      try {net = minera::to_petri_net(*heuristics);}
      catch (const std::exception& error) {conversion_error = error.what();}
    }
    if (computed && net) native_net = minera::workflow_input_petri_json(*net);
    const auto quote = minera::embedding_runtime::detail::escape_json;
    std::string pnml, bpmn, ptml, pnml_error, bpmn_error, ptml_error;
    if (computed && net) {
      try {
        if (!net->has_source() || !net->has_sink())
          throw std::invalid_argument("The complete native net has no workflow source/sink pair for the diagram export.");
        minera::EventLog display_log = source_log;
        if (format == "table" || (adapter != "log" && table)) {
          display_log = workflow_input_selected_log(*table,
              family == "alpha" ? params.alpha.case_id_attribute : params.heuristic.case_id_attribute);
        }
        auto graph = alpha_dfg ? workflow_input_graph(alpha_dfg->activities, alpha_dfg->dfg,
            alpha_dfg->start_activities.value_or(std::map<std::string,std::uint64_t>{}), alpha_dfg->end_activities.value_or(std::map<std::string,std::uint64_t>{}))
            : heuristic_dfg ? workflow_input_graph(heuristics->activities, heuristic_dfg->dfg)
            : minera::discover_directly_follows_graph(display_log);
        for (const auto& name : net->activities()) graph.ensure_activity(name);
        if (heuristics) for (const auto& name : heuristics->activities) graph.ensure_activity(name);
        legacy_result = minera::analysis_json(file.filename, display_log, graph,
            nullptr, family == "alpha" ? &*net : nullptr, heuristics ? &*heuristics : nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, family == "heuristic" ? &*net : nullptr);
      } catch (const std::exception& error) {conversion_error = error.what();}
      try {
        if (plus) {
          if (!plus->detached_initial_marking.empty() || !plus->detached_final_marking.empty())
            throw std::invalid_argument("Detached Alpha+ markings require complete discovery JSON; PNML cannot represent them.");
          minera::PnmlDocument document; document.net = *net;
          for (const auto tokens : plus->initial_marking) document.initial_marking.push_back(tokens);
          for (const auto tokens : plus->final_marking) document.final_marking.push_back(tokens);
          pnml = minera::to_pnml(document, "Independent Alpha+ discovery");
        } else pnml = minera::to_pnml(*net, "Independent workflow discovery");
      } catch (const std::exception& error) {pnml_error = error.what();}
      try {bpmn = minera::to_bpmn(*net, "Independent workflow discovery");}
      catch (const std::exception& error) {bpmn_error = error.what();}
      try {ptml = minera::to_ptml(minera::to_process_tree(*net), "Independent workflow discovery");}
      catch (const std::exception& error) {ptml_error = error.what();}
    }
    const auto optional_text = [&](const std::string& value) {return value.empty() ? std::string("null") : quote(value);};
    const auto after = table ? minera::serialize_event_table_input_json(*table) : "null";
    response.status = error_message.empty() ? 200 : 400;
    response.set_content(workflow_input_safe_json("{\"family\":\"WORKFLOW_INPUT_DISCOVERY\",\"method\":" + quote(family) +
        ",\"variant\":" + quote(variant) + ",\"computed\":" + (computed ? "true" : "false") +
        ",\"error\":" + (error_message.empty() ? "null" : "{\"type\":" + quote(error_type) + ",\"message\":" + quote(error_message) + "}") +
        ",\"input_context\":{\"adapter\":" + quote(adapter) + ",\"format\":" + quote(requested_format) +
        ",\"detected_format\":" + quote(format) +
        ",\"dataset\":" + quote(file.filename) + ",\"origin\":" + quote(origin) +
        ",\"original_input_bytes\":" + std::to_string(file.content.size()) + ",\"input_event_count\":" + std::to_string(original_events) +
        ",\"input_case_count\":" + (original_cases ? std::to_string(*original_cases) : "null") + "},\"discovery_context\":null" +
        ",\"input_before\":" + before + ",\"input_after\":" + after + ",\"dfg_input\":" + dfg_json +
        ",\"result\":" + (legacy_result.empty() ? "null" : legacy_result) + ",\"result_error\":" + optional_text(conversion_error) +
        ",\"native_net\":" + native_net + ",\"native_heuristics\":" + native_heuristics +
        ",\"alpha_discovery\":" + (plus ? minera::alpha_plus_result_json(*plus) : "null") +
        ",\"pnml\":" + optional_text(pnml) + ",\"pnml_error\":" + optional_text(pnml_error) +
        ",\"bpmn\":" + optional_text(bpmn) + ",\"bpmn_error\":" + optional_text(bpmn_error) +
        ",\"ptml\":" + optional_text(ptml) + ",\"ptml_error\":" + optional_text(ptml_error) + '}'), "application/json");
  } catch (const std::exception& error) {
    response.status = 400;
    response.set_content(workflow_input_safe_json(error_json(error.what())), "application/json");
  }
}
