#pragma once
// Include after transformation_json, with a forward declaration before ILP.
// Only requests containing this endpoint group's applicable controls opt in.
bool handle_literal_controls_request(const httplib::Request& request,
    httplib::Response& response,std::string_view group) {
  using namespace minera;
  static const std::set<std::string> exact_names={"target_activities_json",
      "features_event_string_attributes_json","features_event_numeric_attributes_json",
      "features_case_string_attributes_json","features_case_numeric_attributes_json",
      "features_event_attributes_json","features_case_attributes_json",
      "features_event_successor_attributes_json","features_feature_names_json",
      "ilp_options_json","dfg_precision_options_json"};
  if(!literal_controls_requested(group,[&](std::string_view name){const auto key=std::string(name);return request.has_param(key)||request.has_file(key);}))return false;
  const auto q=serialize_embedding_workbench_string_json;
  const auto flag=[](bool value){return value?"true":"false";};
  const auto error_json_value=[&](std::string_view type,const std::exception& error){
    const auto name=dynamic_cast<const std::invalid_argument*>(&error)?"invalid_argument":
        dynamic_cast<const std::out_of_range*>(&error)?"out_of_range":
        dynamic_cast<const std::overflow_error*>(&error)?"overflow_error":
        dynamic_cast<const std::length_error*>(&error)?"length_error":
        dynamic_cast<const std::runtime_error*>(&error)?"runtime_error":"exception";
    return "{\"type\":"+q(type)+",\"message\":"+q(error.what())+",\"exception_type\":"+q(name)+'}';
  };
  const auto transport_error=[&](const std::string& type){return "{\"type\":"+q(type)+",\"message\":"+q("legacy presentation is not displayable: "+type)+",\"exception_type\":null}";};
  const auto original=[&](std::string_view name,std::string_view bytes){
    // Existing65 NativeString identifies every nonscalar byte sequence exactly.
    const auto native=q(bytes);const bool text=!native.empty()&&native.front()=='"';
    std::string data(bytes);
    if(!text){constexpr char h[]="0123456789abcdef";data.clear();for(const unsigned char c:bytes){data+=h[c>>4];data+=h[c&15];}}
    return "{\"dataset\":"+q(name)+",\"encoding\":"+q(text?"utf8":"hex")+",\"data\":"+q(data)+",\"bytes\":"+std::to_string(bytes.size())+",\"sha256\":"+q(footprints_workbench_sha256(bytes))+'}';
  };
  std::string raw_controls="[";
  const auto control=[&](const std::string& name,const std::string& value){if(raw_controls.size()>1)raw_controls+=',';raw_controls+="{\"name\":"+q(name)+",\"original\":"+original(name,value)+'}';};
  for(const auto& [name,value]:request.params)control(name,value);
  for(const auto& [name,value]:request.files)if(!(group=="dfg_precision"&&name=="dfg"))control(name,value.content);
  raw_controls+=']';
  const auto present=[&](std::string_view name){return exact_names.contains(std::string(name))?
      request.has_param(std::string(name))||request.has_file(std::string(name)):request.has_param(std::string(name));};
  const auto lookup=[&](std::string_view name){const auto key=std::string(name);if(request.has_param(key))return request.get_param_value(key);return exact_names.contains(key)&&request.has_file(key)?request.get_file_value(key).content:std::string{};};
  const bool is_target=group=="target",is_features=group=="features",is_ilp=group=="ilp",is_precision=group=="dfg_precision";
  const std::string family=is_target?"LOG_TO_TARGET":is_features?"LOG_TO_FEATURES":is_ilp?"ILP":"DFG_PRECISION";
  const std::string result_type=is_target?"LogToTargetResult":is_features?"LogToFeaturesResult":is_ilp?"ILPMinerResult":"DfgPrecisionResult";
  std::set<std::string> applicable;
  if(is_target)applicable={"target_activities_json"};
  else if(is_features){for(const auto& name:exact_names)if(name.starts_with("features_"))applicable.insert(name);}
  else if(is_ilp)applicable={"ilp_options_json"};
  else applicable={"dfg_precision_options_json"};
  std::shared_ptr<const MinedSession> session;
  {std::lock_guard<std::mutex> lock(g_session_mutex);session=g_session;}
  const auto dfg_bytes=is_precision?ocel_request_body(request,"dfg"):std::string{};
  const auto dfg_original=is_precision?original(request.has_file("dfg")?request.get_file_value("dfg").filename:"dfg",dfg_bytes):std::string("null");
  std::optional<LogToTargetResult> target_result;
  std::optional<LogToFeaturesResult> features_result;
  std::optional<ILPMinerResult> ilp_result;
  std::optional<DfgPrecisionResult> precision_result;
  std::optional<std::string> legacy,source_artifact;
  std::set<std::string> json_members;
  std::vector<std::string> parsed_controls;
  std::map<std::string,std::set<std::string>> parsed_members;
  std::string configured="null",native_result="null",failure="null",failure_message="null",phase="configuration_error",calls="[";
  std::string legacy_error="null",legacy_transport="null",source_error="null",display;
  bool executed=false,returned=false,include_source=false;
  int status=200;
  const auto invoke=[&](std::string_view name,const auto& make){
    executed=true;const auto prefix="{\"name\":"+q(name)+",\"returned\":";
    try{make();returned=true;calls+=prefix+"true,\"error\":null}";}
    catch(const std::exception& error){calls+=prefix+"false,\"error\":"+error_json_value("native_error",error)+'}';throw;}
  };
  try {
    if(!session){status=409;throw std::invalid_argument(is_precision?"no mined log; mine a log first":"no cached log; run /api/mine first");}
    static const std::set<std::string> target_allowed={"target_variant","target_activity_key","target_timestamp_key","target_enable_padding","target_pad_size","target_activities","source_id","include_source"};
    static const std::set<std::string> features_allowed={"features_variant","features_activity_key","features_case_id_key","features_timestamp_key","features_start_timestamp_key","features_resource_key","features_arrival_rate","features_finish_rate","features_service_time","features_waiting_time","features_sojourn_time","features_diff_start_end","features_grouper_freq","features_window_seconds","features_default_not_present","features_event_string_attributes","features_event_numeric_attributes","features_case_string_attributes","features_case_numeric_attributes","features_event_attributes","features_case_attributes","features_event_successor_attributes","features_feature_names","features_enable_activity_def_representation","features_enable_succ_def_representation","features_enable_all_extra_features","features_enable_case_duration","features_add_case_identifier_column","features_enable_times_from_first_occurrence","features_enable_times_from_last_occurrence","features_enable_direct_paths_times_last_occ","features_enable_indirect_paths_times_last_occ","features_enable_work_in_progress","features_enable_resource_workload","features_enable_first_last_activity_index","features_enable_max_concurrent_events","features_enable_max_concurrent_events_per_activity","features_epsilon","features_min_num_different_string_values","features_max_num_different_string_values","feature_variant","feature_activity_key","feature_case_id_key","feature_timestamp_key","feature_start_timestamp_key","feature_resource_key","feature_arrival_rate","feature_finish_rate","feature_service_time","feature_waiting_time","feature_sojourn_time","feature_diff_start_end","feature_grouper_freq","feature_window_seconds","feature_default_not_present","feature_event_string_attributes","feature_event_numeric_attributes","feature_case_string_attributes","feature_case_numeric_attributes","feature_event_attributes","feature_case_attributes","feature_event_successor_attributes","feature_feature_names","feature_enable_activity_def_representation","feature_enable_succ_def_representation","feature_enable_all_extra_features","feature_enable_case_duration","feature_add_case_identifier_column","feature_enable_times_from_first_occurrence","feature_enable_times_from_last_occurrence","feature_enable_direct_paths_times_last_occ","feature_enable_indirect_paths_times_last_occ","feature_enable_work_in_progress","feature_enable_resource_workload","feature_enable_first_last_activity_index","feature_enable_max_concurrent_events","feature_enable_max_concurrent_events_per_activity","feature_epsilon","feature_min_num_different_string_values","feature_max_num_different_string_values","source_id","include_source"};
    static const std::set<std::string> ilp_allowed={"ilp_activity_key","ilp_artificial_start_activity","ilp_artificial_end_activity","ilp_alpha","ilp_causal_relation","ilp_noise_threshold","ilp_max_nodes","source_id","include_pnml"};
    std::set<std::string> allowed;
    if(is_target)allowed=target_allowed;
    else if(is_features)allowed=features_allowed;
    else if(is_ilp)allowed=ilp_allowed;
    else allowed={"activity_attribute"};
    allowed.insert(applicable.begin(),applicable.end());
    for(const auto& name:exact_names)if(present(name)&&request.params.count(name)+request.files.count(name)!=1)throw std::invalid_argument("duplicate literal control: "+name);
    for(const auto& [name,value]:request.params){(void)value;if(!allowed.contains(name))throw std::invalid_argument("unknown "+std::string(group)+" parameter: "+name);}
    for(const auto& [name,value]:request.files){(void)value;if(is_precision&&name=="dfg"){if(request.files.count(name)!=1)throw std::invalid_argument("duplicate DFG body field");continue;}if(!applicable.contains(name))throw std::invalid_argument("inapplicable literal multipart control: "+name);}
    if(is_target||is_features)include_source=transformation_source_requested(request,*session);
    if(is_ilp)include_source=discovery_model_requested(request,*session);
    if(is_target){
      const auto options=parse_log_to_target_literal_params(lookup,present);parsed_controls.push_back("target_activities_json");configured=serialize_literal_controls_options_json(options);
      if(include_source&&options.pad_size>100000)throw std::invalid_argument("target padding exceeds the workflow limit of 100000 rows per case");
      phase="preparation_error";const auto& log=require_prepared_log(*session);phase="native_error";
      invoke("log_to_target(log,options)",[&]{target_result=log_to_target(log,options);});native_result=serialize_literal_controls_result_json(*target_result);
    }else if(is_features){
      const auto options=parse_log_to_features_literal_params(lookup,present,&parsed_controls);configured=serialize_literal_controls_options_json(options);
      phase="preparation_error";const auto& log=require_prepared_log(*session);phase="native_error";
      invoke("log_to_features(log,options)",[&]{features_result=log_to_features(log,options);});native_result=serialize_literal_controls_result_json(*features_result);
    }else if(is_ilp){
      const auto options=parse_ilp_literal_params(lookup,present,&json_members);parsed_controls.push_back("ilp_options_json");parsed_members.emplace("ilp_options_json",json_members);configured=serialize_literal_controls_options_json(options);
      // The explicit JSON path intentionally uses the full native size_t bound.
      phase="preparation_error";const auto& log=require_prepared_log(*session);phase="native_error";
      invoke("mine_ilp(log,options)",[&]{ilp_result=mine_ilp(log,options);});native_result=serialize_literal_controls_result_json(*ilp_result);
    }else{
      if(present("activity_attribute"))throw std::invalid_argument("dfg_precision_options_json: cannot combine with activity_attribute");
      const auto options=parse_literal_dfg_precision_options_json(lookup("dfg_precision_options_json"),&json_members);parsed_controls.push_back("dfg_precision_options_json");parsed_members.emplace("dfg_precision_options_json",json_members);configured=serialize_literal_controls_options_json(options);
      phase="preparation_error";if(dfg_bytes.empty())throw std::invalid_argument("missing DFG text body");
      const auto model=parse_dfg(dfg_bytes);
      const auto view=session->active_source?session->active_source->selected_view():EventDataView(require_prepared_log(*session));
      phase="native_error";invoke("evaluate_dfg_precision(view,model,options)",[&]{precision_result=evaluate_dfg_precision(view,model,options);});native_result=serialize_literal_controls_result_json(*precision_result);
    }
  }catch(const std::exception& error){if(status!=409)status=400;failure=error_json_value(phase,error);failure_message=q(error.what());}
  if(returned){
    try{if(target_result)legacy=log_to_target_json(*target_result);else if(features_result)legacy=log_to_features_json(*features_result);else if(ilp_result)legacy=ilp_miner_json(*ilp_result);else legacy=serialize_dfg_precision_json(*precision_result);}
    catch(const std::exception& error){legacy_error=error_json_value("export_error",error);}
    if(legacy){if(const auto problem=literal_controls_transport_error(*legacy))legacy_transport=transport_error(*problem);else display=*legacy;}
    if(legacy&&include_source){
      try{
        source_artifact=is_ilp?discovery_model_json(*legacy,ilp_result->net,ilp_result->markings,*session,ilp_result->diagnostics.computed):transformation_json(*session,*legacy);
        if(const auto problem=literal_controls_transport_error(*source_artifact))source_error=transport_error(*problem);else display=*source_artifact;
      }catch(const std::exception& error){source_error=error_json_value("export_error",error);}
    }
  }
  calls+=']';
  std::string controls="[",member_paths="[";
  for(const auto& name:parsed_controls){
    if(controls.size()>1){controls+=',';member_paths+=',';}controls+=q(name);
    member_paths+="{\"control\":"+q(name)+",\"members\":[";
    if(const auto found=parsed_members.find(name);found!=parsed_members.end()){bool first=true;for(const auto& member:found->second){if(!first)member_paths+=',';first=false;member_paths+=q(member);}}
    member_paths+="]}";
  }
  controls+=']';member_paths+=']';
  bool current=false;if(session){std::lock_guard<std::mutex> lock(g_session_mutex);current=g_session==session;}
  const auto source_kind=session&&is_precision&&session->active_source?static_cast<std::int32_t>(session->active_source->selected_kind()):0;
  const char* source_name=source_kind==0?"log":source_kind==1?"stream":source_kind==2?"dataframe":source_kind==3?"lazyframe":nullptr;
  const auto source=session?"{\"source_id\":"+q(std::to_string(session->query_source_id))+",\"dataset\":"+q(session->dataset)+
      ",\"dispatch\":"+q(is_precision?"selected_event_data_view":"prepared_event_log")+",\"kind_index\":"+std::to_string(source_kind)+
      ",\"kind_name\":"+(source_name?q(source_name):"null")+",\"cached_selector_overlay\":false,\"current_session\":"+flag(current)+'}':"null";
  const auto audit="{\"version\":\"literal-controls-v1\",\"group\":"+q(group)+",\"mode\":\"exact_json\",\"raw_controls\":"+raw_controls+
      ",\"supplied_controls\":"+controls+",\"supplied_members\":"+member_paths+",\"configured_options\":"+configured+
      ",\"default_origin\":"+q(is_target||is_features?"legacy_scalar_parameters_with_exact_list_overrides":"native_constructor_exact_json")+
      ",\"source\":"+source+",\"dfg_input\":"+dfg_original+",\"executed\":"+flag(executed)+",\"returned\":"+flag(returned)+
      ",\"native_calls\":"+calls+",\"result\":"+(returned?"{\"type\":"+q(result_type)+",\"value\":"+native_result+'}':"null")+
      ",\"error\":"+failure+",\"legacy_artifact\":"+(legacy?original("legacy_result_json",*legacy):"null")+
      ",\"legacy_export_error\":"+legacy_error+",\"legacy_transport_error\":"+legacy_transport+
      ",\"source_artifact\":"+(source_artifact?original("source_presentation_json",*source_artifact):"null")+",\"source_export_error\":"+source_error+'}';
  std::string body;
  if(failure!="null"){
    body="{\"error\":"+failure_message+",\"literal_controls\":"+audit+'}';
  }else if(!display.empty()){
    body=display;const auto end=body.find_last_not_of(" \t\r\n");body.insert(end,",\"literal_controls\":"+audit);
  }else body="{\"family\":"+q(family)+",\"legacy_result\":null,\"legacy_export_error\":"+legacy_error+
      ",\"legacy_transport_error\":"+legacy_transport+",\"literal_controls\":"+audit+'}';
  response.status=status;response.set_content(body,"application/json");return true;
}
