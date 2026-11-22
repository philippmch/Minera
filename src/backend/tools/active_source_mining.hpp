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
  // A CSV upload's DataFrame and LazyFrame families read format_dataframe's
  // row order (session_selected_view; rounds BM and BN).
  const auto data = [&] {return session_selected_view(*session);};
  std::map<std::string, std::string> records;
  std::map<std::string, std::string> options_overrides;
  // A client that disconnected stops the request too, before a family and
  // inside the model decorations.
  const auto disconnected = connection_cancel_hook(request);
  // Inside a family, a cancel-mine (or a newer source request) stops the
  // long searches too (mine_progress.hpp).
  const auto cancelled = mining_cancel_hook(ticket, disconnected);
  const auto run = [&](const std::string& family, std::string_view input_kind, const auto& call) {
    // A canceled or superseded request stops before its next family.
    abort_if_mining_canceled(ticket);
    minera::throw_if_cancelled(disconnected, "mining request was cancelled: the client disconnected");
    mine_progress_stage(mine_family_stage_name(family));
    std::string result = "null", error = "null";
    bool native_returned = false;
    try {result = call(native_returned);}
    catch (const minera::OperationCancelled&) {throw;}
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
  // An XES upload mined as a frame: the families read read_xes's frame, with
  // its gaps and without the columns no value fills (round FY).
  const bool xes_frame = xes_frame_session(*session);
  run("source_log_preparation", "source_conversion", [&](bool& native_returned) {
    // Shared with active_source (which the session keeps), not copied.
    session->log_view = &session->active_source->checked_log(); session->log_ready = true;
    native_returned = true;
    return minera::serialize_active_source_log_json(session_log(*session));
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
    // Alpha+ converts an XES upload's frame to an event log as pm4py's does (round GA).
    if (params.alpha.short_loops) {
      alpha_plus = xes_frame ? minera::discover_alpha_plus_result_xes_frame(data(), params.alpha)
                             : minera::discover_alpha_plus_result(data(), params.alpha);
      alpha = alpha_plus->net;
    }
    else alpha = xes_frame ? minera::discover_petri_net_xes_frame(data(), params.alpha)
                           : minera::discover_petri_net(data(), params.alpha);
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
    // An XES upload's frame converts to an EventLog the way pm4py's does (round FZ).
    heuristics = minera::observe_exact_heuristic_call(data(),params.heuristic,[&]{return xes_frame ? minera::discover_heuristics_net_xes_frame(data(), params.heuristic) : minera::discover_heuristics_net(data(), params.heuristic);});
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
    session->tree = xes_frame ? minera::discover_process_tree_xes_frame(data(), params.inductive)
                              : minera::discover_process_tree(data(), params.inductive);
    session->tree_ready = true;
    native_returned = true;
    return minera::serialize_inductive_process_tree_json(session->tree);
  });
  run("inductive_log_preparation", selected, [&](bool& native_returned) {
    session->inductive_log = xes_frame ? minera::prepare_inductive_event_log_xes_frame(data(), params.inductive)
                                       : minera::prepare_inductive_event_log(data(), params.inductive);
    session->inductive_log_ready = true;
    native_returned = true;
    return minera::serialize_active_source_log_json(session->inductive_log);
  });
  if (session->tree_ready) run("inductive_petri_conversion", "process_tree", [&](bool& native_returned) {
    auto net = minera::to_petri_net(session->tree); session->nets.emplace("inductive", net);
    native_returned = true;
    return minera::workflow_input_petri_json(net);
  });
  // pm4py's POWL reads a pandas or Polars frame sorted by case and timestamp
  // (round FV); an event stream stays on the log.
  const bool powl_frame = selected == "dataframe" || selected == "lazyframe";
  run("powl", powl_frame ? selected : std::string("log"), [&](bool& native_returned) {
    session->powl = powl_frame ? (xes_frame ? minera::discover_powl_xes_frame(data(), params.powl)
                                            : minera::discover_powl(data(), params.powl))
                               : minera::discover_powl(require_prepared_log(*session), params.powl);
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
    auto temporal_options = params.temporal_profile;
    mark_frame_index_rows(*session, temporal_options);
    temporal = xes_frame ? minera::discover_temporal_profile_xes_frame(data(), temporal_options,
                               active_source_supplied(request, "temporal_target_timestamp_attribute") ||
                               active_source_supplied(request, "temporal_target_start_timestamp_attribute"))
                         : minera::discover_temporal_profile(data(), temporal_options);
    native_returned = true;
    return minera::serialize_active_source_temporal_profile_json(*temporal);
  });
  std::optional<minera::TransitionSystem> transition;
  run("transition_system", selected, [&](bool& native_returned) {
    transition = xes_frame
        ? minera::discover_transition_system_xes_frame(data(), params.transition_system)
        : minera::discover_transition_system(data(), params.transition_system);
    native_returned = true;
    return minera::serialize_active_source_transition_json(*transition);
  });
  std::optional<minera::LogSkeletonModel> skeleton;
  std::optional<std::vector<minera::LogSkeletonConformance>> skeleton_conformance;
  // pm4py's DataFrame log skeleton and Declare meet an XES frame's cases in
  // case-id order and list their conformance in it (constraint_case_order).
  const auto skeleton_order = [&] {
    return constraint_case_order(*session->active_source, data(),
                                 params.log_skeleton.activity_key, params.log_skeleton.case_id_key);
  };
  run("log_skeleton", selected, [&](bool& native_returned) {
    if (xes_frame) {
      // A Polars row without an activity is the label None (round FZ).
      skeleton = minera::discover_log_skeleton_xes_frame(data(), params.log_skeleton, true);
    } else {
      const auto order = skeleton_order();
      skeleton = order ? minera::discover_log_skeleton(data(), params.log_skeleton, *order)
                       : minera::discover_log_skeleton(data(), params.log_skeleton);
    }
    native_returned = true;
    return minera::serialize_active_source_skeleton_json(*skeleton);
  });
  if (skeleton) run("log_skeleton_conformance", selected, [&](bool& native_returned) {
    if (xes_frame) {
      // The cases of pm4py's groupby, in its order (round GA). The analysis
      // presentation keeps its one result per trace of the session log.
      const auto frame = minera::check_log_skeleton_conformance_xes_frame(data(), *skeleton, params.log_skeleton);
      native_returned = true;
      try {skeleton_conformance = minera::check_log_skeleton_conformance(data(), *skeleton, params.log_skeleton);}
      catch (const minera::OperationCancelled&) {throw;}
      catch (const std::exception&) {}
      return minera::serialize_active_source_skeleton_json(*skeleton, &frame);
    }
    skeleton_conformance = minera::check_log_skeleton_conformance(data(), *skeleton, params.log_skeleton);
    native_returned = true;
    const auto ordered = in_constraint_order(*skeleton_conformance, skeleton_order());
    return minera::serialize_active_source_skeleton_json(*skeleton, &ordered);
  });
  std::optional<std::vector<minera::DeclareConformance>> declare_conformance;
  // pm4py's DataFrame Declare reads each case of an XES upload as its slice
  // of the frame's rows (traces that share a name apart in the file), in
  // every shape but the event stream (discover_declare_frame_slices).
  const bool declare_slices = session->active_source->file().format == "xes" &&
      session->active_source->selected_kind() != minera::EventDataKind::kEventStream;
  run("declare", selected, [&](bool& native_returned) {
    session->declare = declare_slices
        ? minera::discover_declare_frame_slices(data(), params.declare_options)
        : minera::discover_declare(data(), params.declare_options);
    session->declare_ready = true;
    native_returned = true;
    return minera::serialize_active_source_declare_json(session->declare);
  });
  if (session->declare_ready) run("declare_conformance", selected, [&](bool& native_returned) {
    declare_conformance = declare_slices
        ? minera::check_declare_conformance_frame_slices(data(), session->declare, params.declare_conformance_options)
        : minera::check_declare_conformance(data(), session->declare, params.declare_conformance_options);
    native_returned = true;
    const auto ordered = in_constraint_order(*declare_conformance,
        declare_case_order(*session->active_source, data(),
                              params.declare_conformance_options.activity_key,
                              params.declare_conformance_options.case_id_key));
    return minera::serialize_active_source_declare_json(session->declare, &ordered);
  });
  std::optional<minera::MinimumSelfDistances> distances;
  run("minimum_self_distance", params.minimum_self_distance.variant == minera::MinimumSelfDistanceVariant::kLog ? "log" : params.minimum_self_distance.variant == minera::MinimumSelfDistanceVariant::kPandas ? "dataframe" : "lazyframe", [&](bool& native_returned) {
    // Explicit variant dispatch is the public contract for this family.
    if (params.minimum_self_distance.variant == minera::MinimumSelfDistanceVariant::kLog)
      distances = minera::MinimumSelfDistanceLogDiscovery::apply(require_prepared_log(*session), params.minimum_self_distance);
    // The DataFrame variants read a CSV upload as the frame (round BN).
    else if (params.minimum_self_distance.variant == minera::MinimumSelfDistanceVariant::kPandas)
      distances = minera::MinimumSelfDistancePandasDiscovery::apply(*cached_source_view(*session, minera::EventDataKind::kEventTable).table(), params.minimum_self_distance);
    else distances = minera::MinimumSelfDistancePolarsDiscovery::apply(*cached_source_view(*session, minera::EventDataKind::kLazyEventTable).lazy_table(), params.minimum_self_distance);
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
    spectrum = discover_selected_performance_spectrum(session_log(*session), params.spectrum_activities,
        params.spectrum, session->active_source.get(), session_frame_table(*session),
        params.spectrum.variant == minera::PerformanceSpectrumVariant::kLazyframe ||
                params.spectrum.variant == minera::PerformanceSpectrumVariant::kLazyframeDisconnected
            ? session_frame_lazy(*session) : nullptr);
    native_returned = true;
    return minera::serialize_active_source_spectrum_json(*spectrum);
  });
  std::optional<minera::BatchDiscovery> batches;
  if (params.batches) run("batches", "selected_native_variant", [&](bool& native_returned) {
    batches = discover_selected_batches(session_log(*session), params.batch_options, session->active_source.get(),
                                        session_indexed_frame_table(*session),
                                        params.batch_options.variant == minera::BatchDiscoveryVariant::kPolars
                                            ? session_indexed_frame_lazy(*session) : nullptr);
    native_returned = true;
    return minera::serialize_active_source_batches_json(*batches);
  });
  std::optional<minera::DfgVariantsAnalysis> dfg_variants;
  if (!params.dfg_statistical_variants.empty() || !params.dfg_causal_variants.empty())
    run("dfg_variants", "log", [&](bool& native_returned) {
      dfg_variants = analyze_dfg_variants(require_prepared_log(*session), params.dfg_statistical_variants,
          params.dfg_causal_variants, dfg_statistics_for_source(params.dfg_statistics, session->active_source.get()));
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
    performance = minera::compute_performance_metrics(log, graph, dataframe_performance_source(session->active_source.get()));
    resources = minera::discover_resource_network(log);
    durations = minera::compute_variant_durations(log, graph);
    eventually = minera::discover_eventually_follows(log, graph, params.eventually_follows);
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
    if (name == "alpha") alpha_decoration = minera::decorate_model(cached_model_log(*session, name), petri, "concept:name", "time:timestamp", cancelled);
    else if (name == "inductive") inductive_decoration = minera::decorate_model(cached_model_log(*session, name), petri, "concept:name", "time:timestamp", cancelled);
    else if (name == "heuristic") heuristic_decoration = minera::decorate_model(require_prepared_log(*session), petri, params.heuristic.activity_attribute, params.heuristic.timestamp_attribute, cancelled);
    else {
      const auto projected = project_powl_activity(require_prepared_log(*session), params.powl.activity_key, params.powl.timestamp_key, params.powl.case_id_key);
      powl_decoration = minera::decorate_model(projected, petri, params.powl.activity_key, params.powl.timestamp_key, cancelled);
    }
    native_returned = true;
    return "{\"representation\":\"analysis_presentation\"}";
  });
  std::optional<minera::FootprintsAnalysis> footprints;
  run("footprints", "selected_native_variants", [&](bool& native_returned) {
    if (!session->alpha_log_ready || !session->tree_ready || !session->powl ||
        !net("inductive") || !net("alpha") || !net("heuristic") || !net("powl"))
      throw std::invalid_argument("requested footprint bundle requires successfully discovered source models");
    footprints = analyze_footprints(require_prepared_log(*session), require_graph(*session), session_alpha_log(*session),
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
        // pm4py replays an XES session's frame with every model's net (round
        // GD, below), so the replay needs no prepared log of the model; the
        // footprints still read it (pm4py's frame footprints sort by time).
        const bool model_log_ready = name == "alpha" ? session->alpha_log_ready
            : name == "inductive" ? session->inductive_log_ready : true;
        const auto& log = projected_powl ? *projected_powl
            : xes_frame && !model_log_ready ? require_prepared_log(*session) : cached_model_log(*session, name);
        auto options = params.token_replay;
        if (!params.token_replay_activity_attribute_supplied)
          options.activity_attribute = name == "alpha" ? params.alpha.activity_attribute : name == "inductive" ? params.inductive.activity_key : name == "heuristic" ? params.heuristic.activity_attribute : params.powl.activity_key;
        options = minera::tools::default_replay_input_options(log, std::move(options), params.token_replay_activity_attribute_supplied);
        // pm4py replays an XES session's frame (round GC): its missing columns raise.
        if (xes_frame) minera::detail::check_xes_frame_replay_columns(data(), options.case_id_attribute, options.activity_attribute);
        options_overrides[name + "_conformance"] = active_source_token_replay_options_json(options);
        options.cancelled = cancelled;
        // The frame's own cases and rows (round GD; xes_frame_token_replay.hpp).
        std::optional<minera::detail::XesFrameReplayLog> frame_log;
        if (xes_frame && name != "heuristic") {
          if (name == "alpha") minera::detail::check_alpha_null_transition_names(petri, data().kind() == minera::EventDataKind::kLazyEventTable);
          frame_log = minera::detail::xes_frame_replay_log(data(), options.case_id_attribute, options.activity_attribute);
        }
        if (xes_frame && name == "heuristic") {
          if (heuristics) minera::detail::check_heuristics_null_transition_names(*heuristics, petri);
          frame_log = minera::detail::xes_frame_replay_log(data(), options.case_id_attribute, options.activity_attribute);
          // pm4py breaks routing ties by its place names and picks a None
          // label's silent transition by its transition names.
          if (heuristics && options.place_names.empty() && options.transition_names.empty() &&
              !options.nullable_transition_names) {
            auto names = minera::detail::heuristics_petri_names(*heuristics);
            if (names.places.size() == petri.place_count() && names.transitions.size() == petri.transitions().size()) {
              options.place_names = std::move(names.places);
              options.nullable_transition_names = std::move(names.transitions);
            }
          }
        }
        // pm4py's POWL net names its places p_N and its silent transitions
        // <kind>_N, and its replay breaks routing ties by those names (round
        // GE; powl_petri_names.hpp), whatever the session's shape.
        // Its silent path search follows the arcs in the order the converter
        // made them (powl_replay_net).
        std::optional<minera::PetriNet> powl_replay_net;
        if (name == "powl" && session->powl && options.place_names.empty() &&
            options.transition_names.empty() && !options.nullable_transition_names &&
            !options.place_outgoing_transition_order) {
          if (auto names = minera::detail::powl_petri_names(*session->powl, petri)) {
            powl_replay_net = minera::detail::powl_replay_net(petri, *names);
            options.place_names = std::move(names->places);
            options.nullable_transition_names = std::move(names->transitions);
            options.place_outgoing_transition_order = std::move(names->place_outgoing);
          }
        }
        const auto& replay_petri = powl_replay_net ? *powl_replay_net : petri;
        const auto& replayed = frame_log ? frame_log->log : log;
        const auto reachability = minera::final_marking_reachability(petri, cancelled);
        // A missing label fires a silent transition: Polars' None (pm4py's
        // match of a null activity), and NaN on Alpha's net, whose only
        // silent transition is the missing label's own (the frame's object).
        auto token_replay = frame_log && (frame_log->polars || name == "alpha")
            ? minera::detail::check_conformance_null_silent(replayed, replay_petri, options)
            : minera::check_conformance(replayed, replay_petri, options);
        conformance.push_back({name, true, std::move(token_replay), minera::check_footprints(log, petri), std::nullopt});
        // pm4py lists the frame's cases in the case-id order of the replayed
        // log (the model's own case key; see session_case_order); the frame
        // replay's cases are already in its order.
        if (!frame_log)
          minera::order_trace_diagnostics(conformance.back().token_replay,
              session_case_order(session->active_source.get(), log));
        minera::set_model_soundness(conformance.back(), reachability);
        native_returned = true;
        return "{\"representation\":\"analysis_presentation\"}";
      });
      const auto constraint_order = skeleton_order();
      const minera::AnalysisConstraintCaseOrder constraint_list_order(
          constraint_order ? &*constraint_order : nullptr);
      analysis = minera::analysis_json(dataset, session_log(*session), graph,
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
