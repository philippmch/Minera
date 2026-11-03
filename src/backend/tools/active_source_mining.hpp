#pragma once

void handle_active_source_mining(const httplib::Request& request,
    httplib::Response& response, const std::string& dataset,
    const minera::MineParams& params,
    std::shared_ptr<const minera::ActiveEventSource> source,
    const std::shared_ptr<ActiveMiningTicket>& ticket, const std::string& paired_csv_import_error) {
  auto session = std::make_shared<MinedSession>();
  session->active_source = std::move(source);
  session->dataset = dataset;
  session->paired_csv_import_error = paired_csv_import_error;
  session->log_ready = session->alpha_log_ready = session->inductive_log_ready = false;
  session->tree_ready = session->graph_ready = session->declare_ready = false;
  session->inductive_options = params.inductive;
  session->token_replay = params.token_replay;
  session->alpha_activity_attribute = params.alpha.activity_attribute;
  session->alpha_case_id_attribute = params.alpha.case_id_attribute;
  session->heuristic_activity_attribute = params.heuristic.activity_attribute;
  session->heuristic_case_id_attribute = params.heuristic.case_id_attribute;
  session->powl_activity_key = params.powl.activity_key;
  session->powl_timestamp_key = params.powl.timestamp_key;
  session->powl_case_id_key = params.powl.case_id_key;
  const auto data = [&] {return session->active_source->selected_view();};
  std::map<std::string, std::string> records;
  std::map<std::string, std::string> options_overrides;
  const auto run = [&](const std::string& family, std::string_view input_kind, const auto& call) {
    std::string result = "null", error = "null";
    bool native_returned = false;
    try {result = call(native_returned);}
    catch (const minera::HeuristicPandasError& e) {
      error = "{\"type\":" + active_source_quote(e.reference_type()) + ",\"message\":" + active_source_quote(e.what()) + '}';
    } catch (const std::exception& e) {
      if(family=="heuristic_petri_conversion")minera::record_exact_optional_artifact("conversion",e.what(),true);
      error = "{\"type\":\"native_error\",\"message\":" + active_source_quote(e.what()) + '}';
    }
    records[family] = "{\"attempted\":true,\"executed\":" + std::string(native_returned ? "true" : "false") + ",\"native_result_returned\":" + std::string(native_returned ? "true" : "false") + ",\"native_input_kind\":" + active_source_quote(input_kind) +
        ",\"options\":" + (options_overrides.contains(family) ? options_overrides.at(family) : active_source_family_options_json(params, family)) +
        ",\"result\":" + result + ",\"error\":" + error + '}';
  };
  const auto selected = active_source_shape(session->active_source->selected_kind());
  run("source_log_preparation", "source_conversion", [&](bool& native_returned) {
    session->log = session->active_source->checked_log(); session->log_ready = true;
    native_returned = true;
    return minera::serialize_active_source_log_json(session->log);
  });
  run("directly_follows_graph", "log", [&](bool& native_returned) {
    session->graph = minera::discover_directly_follows_graph(require_prepared_log(*session));
    session->graph_ready = true;
    native_returned = true;
    return minera::serialize_inductive_native_dfg_json(session->graph);
  });
  std::optional<minera::PetriNet> alpha;
  std::optional<minera::AlphaPlusDiscoveryResult> alpha_plus;
  run("alpha", selected, [&](bool& native_returned) {
    if (params.alpha.short_loops) {alpha_plus = minera::discover_alpha_plus_result(data(), params.alpha); alpha = alpha_plus->net;}
    else alpha = minera::discover_petri_net(data(), params.alpha);
    session->nets.emplace("alpha", *alpha);
    native_returned = true;
    return alpha_plus ? minera::alpha_plus_result_json(*alpha_plus) : minera::workflow_input_petri_json(*alpha);
  });
  run("alpha_log_preparation", selected, [&](bool& native_returned) {
    session->alpha_log = minera::normalize_alpha_event_log(data(), params.alpha);
    session->alpha_log_ready = true;
    native_returned = true;
    return minera::serialize_active_source_log_json(session->alpha_log);
  });
  std::optional<minera::HeuristicsNet> heuristics;
  run("heuristic", selected, [&](bool& native_returned) {
    heuristics = minera::observe_exact_heuristic_call(data(),params.heuristic,[&]{return minera::discover_heuristics_net(data(), params.heuristic);});
    native_returned = true;
    try{auto bytes=minera::workflow_input_heuristics_json(*heuristics);minera::record_exact_optional_artifact("legacy_heuristics_json",bytes);return bytes;}
    catch(const std::exception& e){minera::record_exact_optional_artifact("legacy_heuristics_json",e.what(),true);throw;}
  });
  if (heuristics) run("heuristic_petri_conversion", "heuristics_net", [&](bool& native_returned) {
    auto net = minera::to_petri_net(*heuristics); minera::record_exact_petri_net(net); session->nets.emplace("heuristic", net);
    native_returned = true;
    try{auto bytes=minera::workflow_input_petri_json(net);minera::record_exact_optional_artifact("legacy_petri_json",bytes);return bytes;}
    catch(const std::exception& e){minera::record_exact_optional_artifact("legacy_petri_json",e.what(),true);throw;}
  });
  run("inductive", selected, [&](bool& native_returned) {
    session->tree = minera::discover_process_tree(data(), params.inductive);
    session->tree_ready = true;
    native_returned = true;
    return minera::serialize_inductive_process_tree_json(session->tree);
  });
  run("inductive_log_preparation", selected, [&](bool& native_returned) {
    session->inductive_log = minera::prepare_inductive_event_log(data(), params.inductive);
    session->inductive_log_ready = true;
    native_returned = true;
    return minera::serialize_active_source_log_json(session->inductive_log);
  });
  if (session->tree_ready) run("inductive_petri_conversion", "process_tree", [&](bool& native_returned) {
    auto net = minera::to_petri_net(session->tree); session->nets.emplace("inductive", net);
    native_returned = true;
    return minera::workflow_input_petri_json(net);
  });
  run("powl", "log", [&](bool& native_returned) {
    session->powl = minera::discover_powl(require_prepared_log(*session), params.powl);
    native_returned = true;
    return minera::serialize_model_conversion_powl_json(*session->powl);
  });
  if (session->powl) run("powl_petri_conversion", "powl", [&](bool& native_returned) {
    auto net = minera::to_petri_net(*session->powl); session->nets.emplace("powl", net);
    native_returned = true;
    return minera::workflow_input_petri_json(net);
  });
  std::optional<minera::TemporalProfile> temporal;
  run("temporal_profile", selected, [&](bool& native_returned) {
    temporal = minera::discover_temporal_profile(data(), params.temporal_profile);
    native_returned = true;
    return minera::serialize_active_source_temporal_profile_json(*temporal);
  });
  std::optional<minera::TransitionSystem> transition;
  run("transition_system", selected, [&](bool& native_returned) {
    transition = minera::discover_transition_system(data(), params.transition_system);
    native_returned = true;
    return minera::serialize_active_source_transition_json(*transition);
  });
  std::optional<minera::LogSkeletonModel> skeleton;
  std::optional<std::vector<minera::LogSkeletonConformance>> skeleton_conformance;
  run("log_skeleton", selected, [&](bool& native_returned) {
    skeleton = minera::discover_log_skeleton(data(), params.log_skeleton);
    native_returned = true;
    return minera::serialize_active_source_skeleton_json(*skeleton);
  });
  if (skeleton) run("log_skeleton_conformance", selected, [&](bool& native_returned) {
    skeleton_conformance = minera::check_log_skeleton_conformance(data(), *skeleton, params.log_skeleton);
    native_returned = true;
    return minera::serialize_active_source_skeleton_json(*skeleton, &*skeleton_conformance);
  });
  std::optional<std::vector<minera::DeclareConformance>> declare_conformance;
  run("declare", selected, [&](bool& native_returned) {
    session->declare = minera::discover_declare(data(), params.declare_options);
    session->declare_ready = true;
    native_returned = true;
    return minera::serialize_active_source_declare_json(session->declare);
  });
  if (session->declare_ready) run("declare_conformance", selected, [&](bool& native_returned) {
    declare_conformance = minera::check_declare_conformance(data(), session->declare, params.declare_conformance_options);
    native_returned = true;
    return minera::serialize_active_source_declare_json(session->declare, &*declare_conformance);
  });
  std::optional<minera::MinimumSelfDistances> distances;
  run("minimum_self_distance", params.minimum_self_distance.variant == minera::MinimumSelfDistanceVariant::kLog ? "log" : params.minimum_self_distance.variant == minera::MinimumSelfDistanceVariant::kPandas ? "dataframe" : "lazyframe", [&](bool& native_returned) {
    // Explicit variant dispatch is the public contract for this family.
    if (params.minimum_self_distance.variant == minera::MinimumSelfDistanceVariant::kLog)
      distances = minera::MinimumSelfDistanceLogDiscovery::apply(require_prepared_log(*session), params.minimum_self_distance);
    else if (params.minimum_self_distance.variant == minera::MinimumSelfDistanceVariant::kPandas)
      distances = minera::MinimumSelfDistancePandasDiscovery::apply(session->active_source->checked_table(), params.minimum_self_distance);
    else distances = minera::MinimumSelfDistancePolarsDiscovery::apply(session->active_source->checked_lazy(), params.minimum_self_distance);
    native_returned = true;
    return minera::serialize_active_source_distances_json(*distances);
  });
  std::optional<minera::CorrelationVariantsAnalysis> correlations;
  if (!params.correlation_variants.empty()) run("correlation", selected, [&](bool& native_returned) {
    correlations = analyze_correlation_variants(data(), params.correlation_variants, params.correlation);
    native_returned = true;
    return minera::serialize_active_source_correlation_json(*correlations);
  });
  std::optional<minera::PerformanceSpectrum> spectrum;
  if (!params.spectrum_activities.empty()) run("performance_spectrum", "selected_native_variant", [&](bool& native_returned) {
    spectrum = discover_selected_performance_spectrum(session->log, params.spectrum_activities,
        params.spectrum, session->active_source.get());
    native_returned = true;
    return minera::serialize_active_source_spectrum_json(*spectrum);
  });
  std::optional<minera::BatchDiscovery> batches;
  if (params.batches) run("batches", "selected_native_variant", [&](bool& native_returned) {
    batches = discover_selected_batches(session->log, params.batch_options, session->active_source.get());
    native_returned = true;
    return minera::serialize_active_source_batches_json(*batches);
  });
  std::optional<minera::DfgVariantsAnalysis> dfg_variants;
  if (!params.dfg_statistical_variants.empty() || !params.dfg_causal_variants.empty())
    run("dfg_variants", "log", [&](bool& native_returned) {
      dfg_variants = analyze_dfg_variants(require_prepared_log(*session), params.dfg_statistical_variants,
          params.dfg_causal_variants, params.dfg_statistics);
      native_returned = true;
      return "{\"representation\":\"analysis_presentation\"}";
    });
  std::optional<minera::IntervalEventOverlapCounts> event_overlap;
  std::optional<minera::CaseOverlapCounts> case_overlap;
  std::optional<minera::ConcurrentActivityOverlapResult> concurrent_overlap;
  if (params.overlap) run("overlap", "log", [&](bool& native_returned) {
    const auto& log = require_prepared_log(*session);
    event_overlap = minera::discover_interval_event_overlap_counts(log, params.overlap_options);
    case_overlap = minera::discover_case_overlap_counts(log, params.overlap_options);
    concurrent_overlap = minera::discover_concurrent_activity_overlap(log, params.overlap_options);
    native_returned = true;
    return "{\"representation\":\"analysis_presentation\"}";
  });
  std::optional<minera::PerformanceMetrics> performance;
  std::optional<minera::ResourceNetwork> resources;
  std::optional<std::vector<minera::DurationStats>> durations;
  std::optional<std::vector<minera::EventuallyFollowsEdge>> eventually;
  run("log_statistics_preparation", "log", [&](bool& native_returned) {
    const auto& log = require_prepared_log(*session); const auto& graph = require_graph(*session);
    performance = minera::compute_performance_metrics(log, graph);
    resources = minera::discover_resource_network(log);
    durations = minera::compute_variant_durations(log, graph);
    eventually = minera::discover_eventually_follows(log, graph);
    // These remain established presentation metrics, not complete raw native
    // snapshots. The full source is independently available in active_source.
    native_returned = true;
    return "{\"representation\":\"analysis_presentation\"}";
  });
  const auto net = [&](const std::string& name) -> const minera::PetriNet* {
    const auto found = session->nets.find(name); return found == session->nets.end() ? nullptr : &found->second;
  };
  std::optional<minera::ModelDecoration> alpha_decoration, inductive_decoration, heuristic_decoration, powl_decoration;
  for (const auto& [name, petri] : session->nets) run(name + "_decoration", "model_prepared_log", [&](bool& native_returned) {
    if (name == "alpha") alpha_decoration = minera::decorate_model(cached_model_log(*session, name), petri);
    else if (name == "inductive") inductive_decoration = minera::decorate_model(cached_model_log(*session, name), petri);
    else if (name == "heuristic") heuristic_decoration = minera::decorate_model(require_prepared_log(*session), petri, params.heuristic.activity_attribute, params.heuristic.timestamp_attribute);
    else {
      const auto projected = project_powl_activity(require_prepared_log(*session), params.powl.activity_key, params.powl.timestamp_key, params.powl.case_id_key);
      powl_decoration = minera::decorate_model(projected, petri, params.powl.activity_key, params.powl.timestamp_key);
    }
    native_returned = true;
    return "{\"representation\":\"analysis_presentation\"}";
  });
  std::optional<minera::FootprintsAnalysis> footprints;
  run("footprints", "selected_native_variants", [&](bool& native_returned) {
    if (!session->alpha_log_ready || !session->tree_ready || !session->powl ||
        !net("inductive") || !net("alpha") || !net("heuristic") || !net("powl"))
      throw std::invalid_argument("requested footprint bundle requires successfully discovered source models");
    footprints = analyze_footprints(require_prepared_log(*session), require_graph(*session), session->alpha_log,
        session->tree, *session->powl, *net("inductive"), *net("alpha"), *net("heuristic"), *net("powl"), params.footprints,
        session->active_source.get());
    native_returned = true;
    return "{\"representation\":\"analysis_presentation\"}";
  });
  std::string analysis = "null", presentation_error = "null";
  if (session->log_ready && session->graph_ready) {
    try {
      auto& graph = session->graph;
      for (const auto& [name, petri] : session->nets) { (void)name; for (const auto& label : petri.activities()) graph.ensure_activity(label); }
      if (session->tree_ready) for (const auto& label : session->tree.activities()) graph.ensure_activity(label);
      if (heuristics) for (const auto& label : heuristics->activities) graph.ensure_activity(label);
      if (session->powl) register_powl_labels(graph, *session->powl);
      if (temporal) for (const auto& entry : temporal->entries) {graph.ensure_activity(entry.source); graph.ensure_activity(entry.target);}
      if (transition) for (const auto& item : transition->transitions()) if (item.label) graph.ensure_activity(*item.label);
      if (distances) for (const auto& item : *distances) {if (!item.activity_is_null) graph.ensure_activity(item.activity); for (const auto& witness : item.witnesses) graph.ensure_activity(witness);}
      if (correlations) minera::register_correlation_activities(graph, *correlations);
      if (session->declare_ready) for (const auto& [name, rules] : session->declare.rules) { (void)name; for (const auto& rule : rules) {graph.ensure_activity(rule.first); if (rule.binary) graph.ensure_activity(rule.second);} }
      std::vector<minera::ModelConformance> conformance;
      for (const auto& [name, petri] : session->nets) run(name + "_conformance", "model_prepared_log", [&](bool& native_returned) {
        std::optional<minera::EventLog> projected_powl;
        if (name == "powl") projected_powl = project_powl_activity(require_prepared_log(*session), params.powl.activity_key, params.powl.timestamp_key, params.powl.case_id_key);
        const auto& log = projected_powl ? *projected_powl : cached_model_log(*session, name);
        auto options = params.token_replay;
        if (!params.token_replay_activity_attribute_supplied)
          options.activity_attribute = name == "alpha" ? params.alpha.activity_attribute : name == "inductive" ? params.inductive.activity_key : name == "heuristic" ? params.heuristic.activity_attribute : params.powl.activity_key;
        options = minera::tools::default_replay_input_options(log, std::move(options), params.token_replay_activity_attribute_supplied);
        options_overrides[name + "_conformance"] = active_source_token_replay_options_json(options);
        conformance.push_back({name, minera::final_marking_reachable(petri), minera::check_conformance(log, petri, options), minera::check_footprints(log, petri), std::nullopt});
        native_returned = true;
        return "{\"representation\":\"analysis_presentation\"}";
      });
      analysis = minera::analysis_json(dataset, session->log, graph,
          session->tree_ready ? &session->tree : nullptr, net("alpha"), heuristics ? &*heuristics : nullptr,
          &conformance, performance ? &*performance : nullptr, resources ? &*resources : nullptr,
          durations ? &*durations : nullptr, eventually ? &*eventually : nullptr,
          nullptr, alpha_decoration ? &*alpha_decoration : nullptr, spectrum ? &*spectrum : nullptr,
          net("inductive"), inductive_decoration ? &*inductive_decoration : nullptr,
          net("heuristic"), heuristic_decoration ? &*heuristic_decoration : nullptr,
          temporal ? &*temporal : nullptr, distances ? &*distances : nullptr, transition ? &*transition : nullptr,
          session->powl ? &*session->powl : nullptr, net("powl"), powl_decoration ? &*powl_decoration : nullptr,
          skeleton ? &*skeleton : nullptr, skeleton_conformance ? &*skeleton_conformance : nullptr,
          session->declare_ready ? &session->declare : nullptr, declare_conformance ? &*declare_conformance : nullptr,
          footprints ? &*footprints : nullptr, dfg_variants ? &*dfg_variants : nullptr, correlations ? &*correlations : nullptr,
          batches ? &*batches : nullptr,
          nullptr, nullptr, false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
          nullptr, 0, nullptr, nullptr,
          event_overlap ? &*event_overlap : nullptr, case_overlap ? &*case_overlap : nullptr,
          concurrent_overlap ? &*concurrent_overlap : nullptr, nullptr, params.minimum_self_distance.variant);
      const auto end = analysis.find_last_not_of(" \t\r\n");
      analysis = analysis.substr(0, end) + ",\"source_id\":" + active_source_quote(std::to_string(session->query_source_id)) + '}';
    } catch (const std::exception& error) {analysis = "null"; presentation_error = "{\"type\":\"presentation_error\",\"message\":" + active_source_quote(error.what()) + '}';}
  }
  std::string records_json = "{";
  for (const auto& [name, record] : records) {if (records_json.size() != 1) records_json += ','; records_json += active_source_quote(name) + ':' + record;}
  records_json += '}';
  std::string fields = "{";
  for (const auto& [name, field] : request.files) {if (name == "log") continue; if (fields.size() != 1) fields += ','; fields += active_source_quote(name) + ':' + active_source_quote(field.content);}
  for (const auto& [name, field] : request.params) {if (fields.size() != 1) fields += ','; fields += active_source_quote(name) + ':' + active_source_quote(field);}
  fields += '}';
  const auto body = active_source_safe_json("{\"family\":\"ACTIVE_SOURCE_MINING\",\"source_id\":" + active_source_quote(std::to_string(session->query_source_id)) +
      ",\"active_source\":" + active_source_snapshot_json(*session, false) + ",\"analysis\":" + analysis +
      ",\"family_results\":" + records_json + ",\"submitted_controls\":" + fields +
      ",\"inductive_options\":" + minera::serialize_inductive_options_json(params.inductive) +
      (params.filter.sampling ? ",\"sampling\":" + minera::sampling_options_json(*params.filter.sampling) : std::string{}) +
      ",\"error\":" + presentation_error + '}');
  publish_active_mining(ticket, session);
  response.set_content(body, "application/json");
}
