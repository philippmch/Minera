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
  // Round HC (observed black-box): pm4py's families on format_dataframe's
  // typed frame read its copies case:concept:name and time:timestamp unless
  // the request names a key (the copy's own name included).
  const auto frame_key = [&](std::string_view control, const std::string& mapped, const char* copy) {
    if (!active_source_supplied(request, control)) return std::string(copy);
    return active_source_field(request, control) == copy ? std::string(copy) : mapped;
  };
  std::map<std::string, std::string> records;
  // Each failed family's exception, for the footprints bundle of an XES
  // frame session (round GR).
  std::map<std::string, std::exception_ptr> family_failures;
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
      family_failures[family] = std::current_exception();
      error = "{\"type\":" + active_source_quote(e.reference_type()) + ",\"message\":" + active_source_quote(e.what()) + '}';
    } catch (const std::exception& e) {
      family_failures[family] = std::current_exception();
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
    // Round GZ: a typed activity column's labels as pm4py's net shows them
    // (the session's net keeps each value's text for replay).
    std::vector<std::string> alpha_display;
    // Round HC: a CSV upload's frame keyed by a typed or gapped column shows
    // pm4py's net on format_dataframe's typed cells (csv_typed_frame_view);
    // the session's net stays on the texts the log replays (the typed net
    // where they fail).
    std::optional<minera::PetriNet> alpha_typed;
    // The start column the frame's Alpha sorts by (round HC): one of other
    // values than dates reads the typed frame too.
    const std::string alpha_start =
        params.alpha.start_timestamp_attribute.empty() ? "start_timestamp" : params.alpha.start_timestamp_attribute;
    if (!params.alpha.short_loops && !xes_frame)
      if (const auto typed = csv_typed_frame_view(*session, params.alpha.activity_attribute, &alpha_start)) {
        auto typed_options = params.alpha;
        typed_options.case_id_attribute =
            frame_key("alpha_case_id_attribute", typed_options.case_id_attribute, "case:concept:name");
        typed_options.timestamp_attribute =
            frame_key("alpha_timestamp_attribute", typed_options.timestamp_attribute, "time:timestamp");
        alpha_typed = minera::discover_petri_net_xes_frame(*typed, typed_options, &alpha_display);
        try {
          alpha = minera::discover_petri_net(data(), params.alpha);
        } catch (const minera::OperationCancelled&) {
          throw;
        } catch (const std::exception&) {
          alpha = *alpha_typed;
        }
      }
    if (!params.alpha.short_loops && !alpha)
      alpha = xes_frame ? minera::discover_petri_net_xes_frame(data(), params.alpha, &alpha_display)
                        : minera::discover_petri_net(data(), params.alpha);
    session->nets.emplace("alpha", *alpha);
    native_returned = true;
    if (alpha_typed) {
      if (!alpha_display.empty()) alpha_typed->set_activities(alpha_display);
      return minera::workflow_input_petri_json(*alpha_typed);
    }
    if (!alpha_plus && !alpha_display.empty()) {
      auto shown = *alpha;
      shown.set_activities(alpha_display);
      return minera::workflow_input_petri_json(shown);
    }
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
    // A CSV upload's frame raises Heuristics++'s errors as pm4py's does (round GI).
    if (!xes_frame) minera::tools::check_plus_plus_csv_frame(*session->active_source, params.heuristic);
    const auto* csv_columns = csv_frame_columns(*session);
    // Round HC: Heuristics++ keyed by a typed or gapped column of a CSV
    // upload's frame reads format_dataframe's typed cells as on an XES
    // upload's frame (csv_typed_frame_view).
    // A start column of other values than dates likewise.
    const std::string plus_start = params.heuristic.start_timestamp_attribute.value_or("start_timestamp");
    const auto plus_typed = !xes_frame && params.heuristic.variant == minera::HeuristicMinerVariant::kPlusPlus
                                ? csv_typed_frame_view(*session, params.heuristic.activity_attribute, &plus_start)
                                : std::nullopt;
    auto plus_options = params.heuristic;
    if (plus_typed) {
      plus_options.case_id_attribute =
          frame_key("heuristic_case_id_attribute", plus_options.case_id_attribute, "case:concept:name");
      plus_options.timestamp_attribute =
          frame_key("heuristic_timestamp_attribute", plus_options.timestamp_attribute, "time:timestamp");
    }
    heuristics = minera::observe_exact_heuristic_call(data(),params.heuristic,[&]{return xes_frame ? minera::discover_heuristics_net_xes_frame(data(), params.heuristic) : plus_typed ? minera::discover_heuristics_net_xes_frame(*plus_typed, plus_options) : csv_columns ? minera::discover_heuristics_net_csv_frame(data(), params.heuristic, *csv_columns) : minera::discover_heuristics_net(data(), params.heuristic);});
    native_returned = true;
    try{auto bytes=minera::workflow_input_heuristics_json(*heuristics);minera::record_exact_optional_artifact("legacy_heuristics_json",bytes);return bytes;}
    catch(const std::exception& e){minera::record_exact_optional_artifact("legacy_heuristics_json",e.what(),true);throw;}
  });
  if (heuristics) run("heuristic_petri_conversion", "heuristics_net", [&](bool& native_returned) {
    auto net = minera::to_petri_net(*heuristics); minera::record_exact_petri_net(net); session->nets.emplace("heuristic", net);
    // Under pm4py's names, for the routes replaying it (round GF).
    if (auto named = heuristics_reference_replay_net(*heuristics, net)) session->replay_nets.emplace("heuristic", std::move(*named));
    native_returned = true;
    try{auto bytes=minera::workflow_input_petri_json(net);minera::record_exact_optional_artifact("legacy_petri_json",bytes);return bytes;}
    catch(const std::exception& e){minera::record_exact_optional_artifact("legacy_petri_json",e.what(),true);throw;}
  });
  run("inductive", selected, [&](bool& native_returned) {
    std::vector<std::string> tree_display;
    // Round HC: a CSV upload's frame keyed by a typed or gapped column shows
    // pm4py's tree on format_dataframe's typed cells; the session's tree
    // stays on the texts (the typed tree where they fail).
    std::optional<minera::ProcessTree> tree_typed;
    if (!xes_frame)
      if (const auto typed = csv_typed_frame_view(*session, params.inductive.activity_key)) {
        auto typed_options = params.inductive;
        typed_options.case_id_key = frame_key("inductive_case_id_key", typed_options.case_id_key, "case:concept:name");
        typed_options.timestamp_key = frame_key("inductive_timestamp_key", typed_options.timestamp_key, "time:timestamp");
        tree_typed = minera::discover_process_tree_xes_frame(*typed, typed_options, &tree_display);
        try {
          session->tree = minera::discover_process_tree(data(), params.inductive);
        } catch (const minera::OperationCancelled&) {
          throw;
        } catch (const std::exception&) {
          session->tree = *tree_typed;
        }
      }
    if (!tree_typed)
      session->tree = xes_frame ? minera::discover_process_tree_xes_frame(data(), params.inductive, &tree_display)
                                : minera::discover_process_tree(data(), params.inductive);
    session->tree_ready = true;
    native_returned = true;
    if (tree_typed)
      return minera::serialize_inductive_process_tree_json(
          tree_display.empty() ? *tree_typed : minera::ProcessTree(tree_display, tree_typed->root()));
    // Round GZ: the labels pm4py's Polars frame shows; the session's tree
    // keeps the texts the log replays.
    if (!tree_display.empty())
      return minera::serialize_inductive_process_tree_json(minera::ProcessTree(tree_display, session->tree.root()));
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
    // Under pm4py's names, for the routes replaying it (round GF).
    if (auto named = powl_reference_replay_net(*session->powl, net)) session->replay_nets.emplace("powl", std::move(*named));
    native_returned = true;
    return minera::workflow_input_petri_json(net);
  });
  std::optional<minera::TemporalProfile> temporal;
  run("temporal_profile", selected, [&](bool& native_returned) {
    auto temporal_options = params.temporal_profile;
    mark_frame_index_rows(*session, temporal_options);
    // A CSV upload's frame reads its own start column (round GR); no pair
    // of Polars' reaches the flow time: an empty profile (round GS).
    std::string start_name, timestamp_name;
    const auto verdict = xes_frame ? CsvFrameTpVerdict::kRun
                                   : csv_frame_temporal_start(
                                         *session, session->active_source->selected_kind(), temporal_options,
                                         active_source_supplied(request, "temporal_target_timestamp_attribute") ||
                                             active_source_supplied(request, "temporal_target_start_timestamp_attribute"),
                                         &start_name, &timestamp_name);
    const bool temporal_explicit_start =
        active_source_supplied(request, "temporal_target_timestamp_attribute") ||
        active_source_supplied(request, "temporal_target_start_timestamp_attribute");
    // Round HC: a CSV upload's frame keyed by a typed or gapped column
    // reads format_dataframe's typed cells as on an XES upload's frame; the
    // session's profile stays on the texts (the typed one where they fail).
    const auto tp_typed = !xes_frame && verdict == CsvFrameTpVerdict::kRun
                              ? csv_typed_frame_view(*session, temporal_options.activity_attribute)
                              : std::nullopt;
    if (tp_typed) {
      auto typed_options = temporal_options;
      typed_options.case_id_attribute =
          frame_key("temporal_case_id_attribute", typed_options.case_id_attribute, "case:concept:name");
      const auto timestamp_copy = [&](std::string_view first, std::string_view second, const std::string& mapped) {
        if (active_source_supplied(request, first))
          return frame_key(first, mapped, "time:timestamp");
        return frame_key(second, mapped, "time:timestamp");
      };
      typed_options.source_timestamp_attribute =
          timestamp_copy("temporal_source_timestamp_attribute", "temporal_source_completion_timestamp_attribute",
                         typed_options.source_timestamp_attribute);
      typed_options.target_timestamp_attribute =
          timestamp_copy("temporal_target_timestamp_attribute", "temporal_target_start_timestamp_attribute",
                         typed_options.target_timestamp_attribute);
      auto shown = minera::discover_temporal_profile_xes_frame(*tp_typed, typed_options, temporal_explicit_start);
      try {
        temporal = minera::discover_temporal_profile_named_start(data(), temporal_options, start_name, timestamp_name);
      } catch (const minera::OperationCancelled&) {
        throw;
      } catch (const std::exception&) {
        temporal = shown;
      }
      native_returned = true;
      return minera::serialize_active_source_temporal_profile_json(minera::temporal_profile_xes_frame_labels(
          *tp_typed, typed_options, temporal_explicit_start, std::move(shown)));
    }
    temporal = xes_frame ? minera::discover_temporal_profile_xes_frame(data(), temporal_options, temporal_explicit_start)
               : verdict == CsvFrameTpVerdict::kEmpty ? minera::TemporalProfile{}
                                                      : minera::discover_temporal_profile_named_start(
                                                            data(), temporal_options, start_name, timestamp_name);
    native_returned = true;
    // Round GZ: a typed activity column's labels as pm4py's frame shows them.
    if (xes_frame)
      return minera::serialize_active_source_temporal_profile_json(
          minera::temporal_profile_xes_frame_labels(data(), temporal_options, temporal_explicit_start, *temporal));
    return minera::serialize_active_source_temporal_profile_json(*temporal);
  });
  std::optional<minera::TransitionSystem> transition;
  run("transition_system", selected, [&](bool& native_returned) {
    // Round HO: a CSV upload's frame keyed by a typed or gapped column reads
    // format_dataframe's typed cells as on an XES upload's frame.
    const auto& transition_key = params.transition_system.activity_attribute.empty()
                                     ? params.transition_system.activity_key
                                     : params.transition_system.activity_attribute;
    const auto transition_typed = xes_frame ? std::nullopt : csv_typed_frame_view(*session, transition_key);
    transition = xes_frame ? minera::discover_transition_system_xes_frame(data(), params.transition_system)
                 : transition_typed
                     ? minera::discover_transition_system_xes_frame(*transition_typed, params.transition_system)
                     : minera::discover_transition_system(data(), params.transition_system);
    native_returned = true;
    return minera::serialize_active_source_transition_json(*transition);
  });
  std::optional<minera::LogSkeletonModel> skeleton;
  // Round GZ: a typed activity column's model as pm4py labels it (the
  // session's model names each activity by its text, as the log does).
  std::optional<minera::LogSkeletonModel> skeleton_shown;
  std::optional<std::vector<minera::LogSkeletonConformance>> skeleton_conformance;
  std::optional<minera::detail::LogSkeletonFrameObjects> skeleton_objects;
  // pm4py's DataFrame log skeleton and Declare meet an XES frame's cases in
  // case-id order and list their conformance in it (constraint_case_order).
  const auto skeleton_order = [&] {
    return constraint_case_order(*session->active_source, data(),
                                 params.log_skeleton.activity_key, params.log_skeleton.case_id_key);
  };
  // Round HC: a CSV upload's frame keyed by a typed or gapped column reads
  // format_dataframe's typed cells as on an XES upload's frame; the
  // session's model and conformance stay on the texts (the typed model's
  // texts where they fail).
  const auto skeleton_typed =
      xes_frame ? std::nullopt : csv_typed_frame_view(*session, params.log_skeleton.activity_key);
  auto skeleton_typed_options = params.log_skeleton;
  if (skeleton_typed)
    skeleton_typed_options.case_id_key =
        frame_key("log_skeleton_case_id_key", skeleton_typed_options.case_id_key, "case:concept:name");
  run("log_skeleton", selected, [&](bool& native_returned) {
    if (xes_frame) {
      // A Polars row without an activity is the label None (round FZ).
      skeleton_shown = minera::discover_log_skeleton_xes_frame(data(), params.log_skeleton, true);
      skeleton = minera::log_skeleton_xes_frame_texts(data(), params.log_skeleton, *skeleton_shown);
    } else if (skeleton_typed) {
      skeleton_shown = minera::discover_log_skeleton_xes_frame(*skeleton_typed, skeleton_typed_options, true);
      try {
        const auto order = skeleton_order();
        skeleton = order ? minera::discover_log_skeleton(data(), params.log_skeleton, *order)
                         : minera::discover_log_skeleton(data(), params.log_skeleton);
      } catch (const minera::OperationCancelled&) {
        throw;
      } catch (const std::exception&) {
        skeleton = minera::log_skeleton_xes_frame_texts(*skeleton_typed, skeleton_typed_options, *skeleton_shown);
      }
    } else {
      const auto order = skeleton_order();
      skeleton = order ? minera::discover_log_skeleton(data(), params.log_skeleton, *order)
                       : minera::discover_log_skeleton(data(), params.log_skeleton);
    }
    native_returned = true;
    // Pairs of equal objects printed apart take the first pair met (round
    // HO); the conformance below checks the unrenamed model.
    if (skeleton_shown) {
      skeleton_objects = minera::detail::log_skeleton_xes_frame_objects(
          skeleton_typed ? *skeleton_typed : data(), skeleton_typed ? skeleton_typed_options : params.log_skeleton);
      if (skeleton_objects) {
        auto named = *skeleton_shown;
        std::vector<minera::LogSkeletonConformance> none;
        minera::detail::name_log_skeleton_pairs(named, none, skeleton_objects->labels, skeleton_objects->printed, {}, {});
        return minera::serialize_active_source_skeleton_json(named);
      }
    }
    return minera::serialize_active_source_skeleton_json(skeleton_shown ? *skeleton_shown : *skeleton);
  });
  if (skeleton) run("log_skeleton_conformance", selected, [&](bool& native_returned) {
    if (xes_frame || skeleton_typed) {
      // The cases of pm4py's groupby, in its order (round GA). The analysis
      // presentation keeps its one result per trace of the session log.
      const auto& shown = skeleton_shown ? *skeleton_shown : *skeleton;
      const auto frame = minera::check_log_skeleton_conformance_xes_frame(
          skeleton_typed ? *skeleton_typed : data(), shown, skeleton_typed ? skeleton_typed_options : params.log_skeleton);
      native_returned = true;
      try {skeleton_conformance = minera::check_log_skeleton_conformance(data(), *skeleton, params.log_skeleton);}
      catch (const minera::OperationCancelled&) {throw;}
      catch (const std::exception&) {}
      if (skeleton_objects && skeleton_shown) {
        auto named = *skeleton_shown;
        auto named_frame = frame;
        minera::detail::name_log_skeleton_pairs(named, named_frame, skeleton_objects->labels, skeleton_objects->printed,
                                                {}, {});
        return minera::serialize_active_source_skeleton_json(named, &named_frame);
      }
      return minera::serialize_active_source_skeleton_json(shown, &frame);
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
  // Round HB: the conformance against the typed model (pm4py's errors), as
  // the family shows it; the session's list stays on its own model.
  std::optional<minera::DeclareModel> declare_shown;
  std::optional<minera::DeclareShownConformance> declare_shown_conformance;
  std::optional<minera::LazyEventTable> declare_polars_view;
  const bool declare_same_keys =
      params.declare_conformance_options.activity_key == params.declare_options.activity_key &&
      params.declare_conformance_options.case_id_key == params.declare_options.case_id_key;
  run("declare", selected, [&](bool& native_returned) {
    // Round HA: a pandas frame keyed by a typed column shows pm4py's typed
    // model; the session keeps the model on the XES texts it replays.
    // Round HB: a CSV upload's pandas frame reads format_dataframe's typed
    // cells the same way (csv_typed_declare).
    // A CSV upload's Polars frame checks its copy's columns first
    // (csv_polars_declare_view).
    std::optional<minera::DeclareModel> shown;
    if (!declare_slices && csv_typed_declare_input(*session, params.declare_options)) {
      if (auto typed = csv_typed_declare(*session, params.declare_options)) {
        session->declare_playout_error = typed->playout_error;
        shown = std::move(typed->model);
        declare_shown_conformance = std::move(typed->conformance);
      }
    } else if (!declare_slices) {
      declare_polars_view = csv_polars_declare_view(*session, params.declare_options);
      if (declare_polars_view)
        shown = minera::discover_declare(minera::EventDataView(*declare_polars_view), params.declare_options);
    }
    if (!declare_slices && shown) {
      try {
        session->declare = minera::discover_declare(data(), params.declare_options);
      } catch (const minera::OperationCancelled&) {
        throw;
      } catch (const std::exception&) {
        session->declare = minera::DeclareModel{};
      }
    } else {
      session->declare = declare_slices
          ? minera::discover_declare_frame_slices(data(), params.declare_options, &shown, &declare_shown_conformance)
          : minera::discover_declare(data(), params.declare_options);
    }
    declare_shown = shown;
    session->declare_ready = true;
    native_returned = true;
    return minera::serialize_active_source_declare_json(shown ? *shown : session->declare);
  });
  if (session->declare_ready) run("declare_conformance", selected, [&](bool& native_returned) {
    try {
      declare_conformance = declare_slices
          ? minera::check_declare_conformance_frame_slices(data(), session->declare, params.declare_conformance_options)
          : minera::check_declare_conformance(data(), session->declare, params.declare_conformance_options);
    } catch (const minera::OperationCancelled&) {
      throw;
    } catch (const std::exception&) {
      if (!(declare_shown_conformance || declare_polars_view) || !declare_same_keys) throw;
    }
    if (declare_shown && declare_polars_view && declare_same_keys) {
      declare_shown_conformance.emplace();
      try {
        declare_shown_conformance->results = minera::check_declare_conformance(
            minera::EventDataView(*declare_polars_view), *declare_shown, params.declare_conformance_options);
      } catch (const minera::OperationCancelled&) {
        throw;
      } catch (const std::exception& error) {
        declare_shown_conformance->error = error.what();
      }
    }
    if (declare_shown && declare_shown_conformance && declare_same_keys) {
      if (!declare_shown_conformance->error.empty()) throw std::invalid_argument(declare_shown_conformance->error);
      native_returned = true;
      const auto order = declare_case_order(*session->active_source, data(),
                                            params.declare_conformance_options.activity_key,
                                            params.declare_conformance_options.case_id_key);
      const auto ordered = in_constraint_order(declare_shown_conformance->results, order);
      return minera::serialize_active_source_declare_json(*declare_shown, &ordered);
    }
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
  // The pandas frame's order, or the Polars copy's on the lazy shape (nulls
  // first, clamped whole-second durations; round GV).
  std::optional<minera::FrameDfgStartOrder> frame_dfg_order;
  std::optional<minera::ResourceNetwork> resources;
  std::optional<std::vector<minera::DurationStats>> durations;
  std::optional<std::vector<minera::EventuallyFollowsEdge>> eventually;
  std::string eventually_error;
  run("log_statistics_preparation", "log", [&](bool& native_returned) {
    const auto& log = require_prepared_log(*session); const auto& graph = require_graph(*session);
    // pm4py's frame DFG reads an upload's start_timestamp column (round GU).
    frame_dfg_order = session_frame_dfg_order(session->active_source.get(), log);
    if (frame_dfg_order)
      performance = minera::compute_performance_metrics(log, minera::frame_dfg_graph(graph, log, *frame_dfg_order),
                                                        true, &*frame_dfg_order);
    else
      performance = minera::compute_performance_metrics(log, graph, dataframe_performance_source(session->active_source.get()));
    resources = minera::discover_resource_network(log);
    durations = minera::compute_variant_durations(log, graph);
    // pm4py's eventually-follows graph on the upload's frame, as /api/mine's
    // default shape reads it (rounds GW, GX); its errors leave it empty.
    if (auto frame_eventually = frame_eventually_follows(session->active_source.get(), log, graph,
                                                         params.eventually_follows)) {
      eventually = std::move(frame_eventually->edges);
      eventually_error = std::move(frame_eventually->error);
    } else {
      eventually = minera::discover_eventually_follows(log, graph, params.eventually_follows);
    }
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
    // The heuristics and POWL nets replay under pm4py's names (round GF).
    else if (name == "heuristic") {
      std::optional<std::vector<std::vector<std::size_t>>> outgoing;
      const auto& replay = session_replay_net(*session, name, petri, &outgoing);
      heuristic_decoration = minera::detail::decorate_model(require_prepared_log(*session), replay, params.heuristic.activity_attribute, params.heuristic.timestamp_attribute, cancelled, outgoing);
    } else {
      const auto projected = project_powl_activity(require_prepared_log(*session), params.powl.activity_key, params.powl.timestamp_key, params.powl.case_id_key);
      std::optional<std::vector<std::vector<std::size_t>>> outgoing;
      const auto& replay = session_replay_net(*session, name, petri, &outgoing);
      powl_decoration = minera::detail::decorate_model(projected, replay, params.powl.activity_key, params.powl.timestamp_key, cancelled, outgoing);
    }
    native_returned = true;
    return "{\"representation\":\"analysis_presentation\"}";
  });
  std::optional<minera::FootprintsAnalysis> footprints;
  // The XES frame session's label of a missing activity (round GP, below).
  std::optional<std::string> footprints_missing_label;
  run("footprints", "selected_native_variants", [&](bool& native_returned) {
    if (!xes_frame) {
      if (!session->alpha_log_ready || !session->tree_ready || !session->powl ||
          !net("inductive") || !net("alpha") || !net("heuristic") || !net("powl"))
        throw std::invalid_argument("requested footprint bundle requires successfully discovered source models");
      footprints = analyze_footprints(require_prepared_log(*session), require_graph(*session), session_alpha_log(*session),
          session->tree, *session->powl, *net("inductive"), *net("alpha"), *net("heuristic"), *net("powl"), params.footprints,
          session->active_source.get());
      native_returned = true;
      return "{\"representation\":\"analysis_presentation\"}";
    }
    // On an XES frame session the bundle reads only the models its
    // selection names, as /api/discover-graph does (round GQ; round GD
    // there): a default variant whose model the session lacks is left out.
    auto options = params.footprints;
    if (!active_source_supplied(request, "footprints_discovery_variants")) {
      if (!session->tree_ready) options.discovery_variants.erase("process_tree");
      if (!session->powl) options.discovery_variants.erase("powl");
    }
    const auto& model = options.model;
    const bool reach = options.discovery_variants.contains("petri_reach_graph");
    const bool ready =
        (!options.discovery_variants.contains("process_tree") || session->tree_ready) &&
        (!options.discovery_variants.contains("powl") || session->powl) &&
        (model != "process_tree" || (session->tree_ready && (!reach || net("inductive")))) &&
        (model != "inductive" || net("inductive")) && (model != "powl" || (session->powl && (!reach || net("powl")))) &&
        (model != "alpha" || (net("alpha") && session->alpha_log_ready)) && (model != "heuristic" || net("heuristic"));
    if (!ready) {
      // pm4py raises the error of the model's own discovery (round GR):
      // the model's family first, then the discovery variants' models.
      std::vector<std::string> sources;
      if (model == "heuristic") sources = {"heuristic", "heuristic_petri_conversion"};
      else if (model == "alpha") sources = {"alpha", "alpha_log_preparation"};
      else if (model == "inductive" || model == "process_tree") sources = {"inductive", "inductive_petri_conversion"};
      else if (model == "powl") sources = {"powl", "powl_petri_conversion"};
      if (options.discovery_variants.contains("process_tree")) sources.push_back("inductive");
      if (options.discovery_variants.contains("powl")) sources.push_back("powl");
      for (const auto& source : sources)
        if (const auto failure = family_failures.find(source); failure != family_failures.end()) std::rethrow_exception(failure->second);
      throw std::invalid_argument("requested footprint bundle requires successfully discovered source models");
    }
    // pm4py converts an XES session's frame to an EventLog (round GP): a
    // missing activity is one more activity (NaN, Polars None), the traces
    // without a case id one trace.
    footprints_missing_label = session->active_source->selected_kind() == minera::EventDataKind::kLazyEventTable ? "None" : "nan";
    footprints = analyze_footprints_models(require_prepared_log(*session), require_graph(*session),
        session->alpha_log_ready ? &session_alpha_log(*session) : nullptr, session->tree_ready ? &session->tree : nullptr,
        session->powl ? &*session->powl : nullptr, net("inductive"), net("alpha"), net("heuristic"), net("powl"), options,
        session->active_source.get(), nullptr, footprints_missing_label ? &*footprints_missing_label : nullptr);
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
      // The footprints' label of a missing activity (round GQ, as
      // /api/discover-graph registers it).
      if (footprints && footprints_missing_label && params.footprints.model != "alpha") {
        bool missing = false, labelled = false;
        for (const auto& trace : session->active_source->checked_log().traces)
          for (const auto& event : trace.events)
            (!event.has_activity() || event.activity_state == minera::FirstClassAttributeState::kNull ||
                     (event.activity_attribute && event.activity_attribute->is_null)
                 ? missing
                 : labelled) = true;
        if (missing && labelled) graph.ensure_activity(*footprints_missing_label);
        // A custom key's labels (round GR).
        if (footprints_custom_keys(params.footprints)) register_footprints_activities(graph, *footprints);
      }
      if (temporal) for (const auto& entry : temporal->entries) {graph.ensure_activity(entry.source); graph.ensure_activity(entry.target);}
      if (transition) {
        for (const auto& item : transition->transitions())
          if (const auto shown = minera::transition_activity_shown(item.label)) graph.ensure_activity(*shown);
        // A window names a state by its own objects (round HO).
        for (const auto& state : transition->states())
          for (const auto& activity : state.payload)
            if (const auto shown = minera::transition_activity_shown(activity)) graph.ensure_activity(*shown);
      }
      if (distances) for (const auto& item : *distances) {if (!item.activity_is_null) graph.ensure_activity(item.activity); for (const auto& witness : item.witnesses) graph.ensure_activity(witness);}
      if (correlations) minera::register_correlation_activities(graph, *correlations);
      if (session->declare_ready) for (const auto& [name, rules] : session->declare.rules) { (void)name; for (const auto& rule : rules) {graph.ensure_activity(rule.first); if (rule.binary) graph.ensure_activity(rule.second);} }
      // A keyed log skeleton's labels, as the default shape registers them
      // (round GY: a custom key failed the whole mine's presentation); a
      // Polars frame's missing activity has no graph id.
      if (skeleton) {
        const auto label = [&](const std::string& activity) {
          if (activity != minera::kLogSkeletonNullActivity) graph.ensure_activity(activity);
        };
        for (const auto* relation : {&skeleton->equivalence, &skeleton->always_after, &skeleton->always_before,
                                     &skeleton->never_together, &skeleton->directly_follows})
          for (const auto& pair : *relation) {label(pair.source); label(pair.target);}
        for (const auto& [activity, counts] : skeleton->activ_freq) {(void)counts; label(activity);}
      }
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
        }
        // pm4py breaks routing ties by the place names of its heuristics and
        // POWL nets and picks a None label's silent transition by their
        // transition names; on the POWL net (p_N, <kind>_N) its silent path
        // search follows the arcs in the order the converter made them
        // (rounds GD and GE), whatever the session's shape (round GF).
        const auto& replay_petri = session_replay_net(*session, name, petri, options);
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
      analysis = minera::analysis_json(dataset, session_log(*session),
          frame_dfg_order ? minera::frame_dfg_graph(graph, require_prepared_log(*session), *frame_dfg_order) : graph,
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
      analysis = analysis.substr(0, end) + ",\"source_id\":" + active_source_quote(std::to_string(session->query_source_id)) +
                 (eventually_error.empty() ? std::string{} : ",\"eventually_follows_error\":" + active_source_quote(eventually_error)) + '}';
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
