#pragma once
// Projection-only sibling; the existing search handler and algorithms remain
// unchanged. Submitted identity is not evidence of provenance or optimality.
void handle_saved_dfg_projection_workbench(const httplib::Request& request,httplib::Response& response) {
 const auto q=minera::serialize_embedding_workbench_string_json;
 const auto lookup=[&](std::string_view n){const std::string k(n);return request.has_file(k)?form_field(request,k):request.has_param(k)?request.get_param_value(k):std::string{};};
 const auto present=[&](std::string_view n){return request.has_file(std::string(n))||request.has_param(std::string(n));};
 const auto nullable=[&](const std::string& s){return s.empty()?std::string("null"):q(s);};
 std::string input_name,model_name,saved_name,original_input,original_model,original_saved;
 if(request.has_file("input_file")){const auto& f=request.get_file_value("input_file");input_name=f.filename;original_input=f.content;}
 if(request.has_file("model_file")){const auto& f=request.get_file_value("model_file");model_name=f.filename;original_model=f.content;}
 if(request.has_file("saved_result_file")){const auto& f=request.get_file_value("saved_result_file");saved_name=f.filename;original_saved=f.content;}
 else if(present("saved_result_json"))original_saved=lookup("saved_result_json");
 const auto format=present("input_format")?lookup("input_format"):"auto";
 const auto model_format=present("model_format")?lookup("model_format"):"auto";
 const auto saved_shape=present("saved_input_shape")?lookup("saved_input_shape"):"native_result";
 const auto projection_request=present("projection")?lookup("projection"):"both";
 std::string detected=format,detected_model=model_format,origin="uploaded_grouped_log",case_selector="case:concept:name";
 std::string canonical="null",result="null",input_table="null",input_log="null",input_raw="null",projected="null",projected_raw="null",metrics="null",options="null";
 std::string error,input_log_error,table_error,model_export,model_export_error,metrics_error,log_error,projected_log_error,projected_xes_error,projected_xes;
 std::size_t cases=0,events=0;bool executed=false,projection_complete=false;
 minera::DfgAlignmentOptions configured;
 try {
  if(!request.is_multipart_form_data()||!request.has_file("input_file")||!request.has_file("model_file"))throw std::invalid_argument("saved DFG projection requires multipart input_file and model_file");
  if(present("mode")&&lookup("mode")!="DFG_CLASSIC")throw std::invalid_argument("saved DFG projection requires mode DFG_CLASSIC");
  if(present("input_shape")&&lookup("input_shape")!="log")throw std::invalid_argument("saved DFG projection requires input_shape log");
  if(format!="auto"&&format!="table"&&format!="raw_log")throw std::invalid_argument("input_format must be auto, table or raw_log");
  if(model_format!="auto"&&model_format!="dfg_json"&&model_format!="dfg")throw std::invalid_argument("model_format must be auto, dfg_json or dfg");
  if(present("saved_result_file")==present("saved_result_json"))throw std::invalid_argument("supply exactly one saved_result_file or saved_result_json");
  if(present("saved_result_file")&&!request.has_file("saved_result_file"))throw std::invalid_argument("saved_result_file must be a multipart File");
  if(projection_request!="none"&&projection_request!="log"&&projection_request!="metrics"&&projection_request!="both")throw std::invalid_argument("projection must be none, log, metrics or both");
  static const std::set<std::string> controls={
   "operation","input_file","model_file","mode","input_format","input_shape","model_format","saved_result_file","saved_result_json","saved_input_shape","projection","alignment_selectors_json","alignment_case_id_attribute",
   "case_column","activity_column","timestamp_column","resource_column","lifecycle_column","delimiter","xes_variant","xes_case_attribute","xes_activity_attribute","xes_timestamp_attribute","xes_resource_attribute","xes_lifecycle_attribute","xes_require_timestamp","xes_timestamp_sort","xes_reverse_sort","xes_max_traces","xes_attributes_to_read","xes_max_decompressed_bytes",
   "dfg_variant","variant","dfg_max_states","dfg_max_time_ms","max_visited_states","max_time_ms","dfg_activity_attribute","dfg_timestamp_attribute","dfg_sync_cost","dfg_sync_costs","dfg_model_move_cost","dfg_model_move_costs","dfg_model_cost","dfg_log_move_cost","dfg_log_move_costs","dfg_log_cost","dfg_internal_log_move_cost","dfg_internal_log_move_costs","dfg_internal_cost","sync_cost","sync_costs","model_move_cost","model_costs","log_move_cost","log_costs","internal_log_move_cost","internal_costs","dfg_sync_cost_json","dfg_model_move_cost_json","dfg_log_move_cost_json","dfg_internal_log_move_cost_json"};
  for(const auto& [k,v]:request.params){(void)v;if(!controls.contains(k))throw std::invalid_argument("unknown saved DFG projection control: "+k);}
  for(const auto& [k,v]:request.files){(void)v;if(!controls.contains(k))throw std::invalid_argument("unknown saved DFG projection control: "+k);}
  const auto selectors=present("alignment_selectors_json")?minera::parse_saved_dfg_projection_selectors_json(lookup("alignment_selectors_json")):std::map<std::string,std::string>{};
  const auto parser_lookup=[&](std::string_view k){return selectors.contains(std::string(k))?std::string{}:lookup(k);};
  const auto parser_presence=[&](std::string_view k){return !selectors.contains(std::string(k))&&present(k);};
  configured=minera::parse_dfg_alignment_params(parser_lookup,parser_presence);
  case_selector=present("alignment_case_id_attribute")?lookup("alignment_case_id_attribute"):"case:concept:name";
  for(const auto& [k,v]:selectors){if(k=="alignment_case_id_attribute")case_selector=v;else if(k=="dfg_activity_attribute")configured.activity_attribute=v;else configured.timestamp_attribute=v;}
  options=minera::serialize_saved_dfg_projection_options_json(configured);
  const auto saved=minera::parse_saved_dfg_alignment_result_json(original_saved,saved_shape);result=minera::serialize_saved_dfg_alignment_result_json(saved);
  minera::AlphaDfgDiscoveryInput graph;
  if(detected_model=="auto"){const auto first=original_model.find_first_not_of(" \t\r\n");detected_model=first!=std::string::npos&&original_model[first]=='{'?"dfg_json":"dfg";}
  if(detected_model=="dfg_json")graph=minera::parse_saved_dfg_projection_model_json(original_model);
  else{const auto g=minera::parse_dfg(original_model);graph.kind=minera::AlphaDfgDiscoveryInput::Kind::kClean;graph.dfg=g.graph;graph.start_activities=g.start_activities;graph.end_activities=g.end_activities;}
  canonical=minera::serialize_saved_dfg_projection_model_json(graph);
  try{model_export=minera::to_dfg(minera::DfgCleanAnalysis{graph.dfg,*graph.start_activities,*graph.end_activities});}catch(const std::exception& e){model_export_error=e.what();}
  minera::EventLog log;std::optional<minera::EventTable> table;
  if(format=="raw_log"){log=minera::parse_embedding_workbench_raw_log_json(original_input);origin="uploaded_raw_log";}
  else if(format=="table"){table=minera::parse_event_table_json(original_input);log=graph_alignment_selected_log(*table,case_selector);origin="uploaded_typed_table";}
  else{const auto imports=minera::parse_mine_params(lookup,present);std::istringstream stream(original_input);
   if(looks_like_xes(input_name,original_input)){log=minera::import_xes_dataframe_log(stream,imports.xes);detected="xes";}
   else{auto imported=minera::import_csv_event_data(stream,imports.csv);log=std::move(imported.log);table=std::move(imported.table);detected="csv";origin="uploaded_csv_rows";if(!present("alignment_case_id_attribute")&&!selectors.contains("alignment_case_id_attribute"))case_selector=imports.csv.case_id_column;}
  }
  cases=log.trace_count();events=log.event_count();input_raw=minera::serialize_embedding_workbench_raw_log_json(log);
  const auto presentation=[&](const auto& render,std::string& fault){try{auto s=render();if(const auto transport=minera::literal_controls_transport_error(s))throw std::invalid_argument("optional presentation transport: "+*transport);return s;}catch(const std::exception& e){fault=e.what();return std::string("null");}};
  if(table)input_table=presentation([&]{return minera::serialize_event_table_input_json(*table);},table_error);
  input_log=presentation([&]{return graph_alignment_log_json(log);},input_log_error);
  executed=true;projection_complete=true;
  if(projection_request=="metrics"||projection_request=="both")try{metrics=minera::serialize_saved_dfg_projection_metrics_json(minera::project_alignments_on_dfg(log,graph.dfg,*graph.start_activities,*graph.end_activities,saved,configured));}catch(const std::exception& e){metrics_error=e.what();projection_complete=false;}
  if(projection_request=="log"||projection_request=="both")try{
   const auto value=minera::project_log_on_dfg(log,graph.dfg,*graph.start_activities,*graph.end_activities,saved,configured);
   projected_raw=minera::serialize_embedding_workbench_raw_log_json(value);
   projected=presentation([&]{return graph_alignment_log_json(value);},projected_log_error);
   try{projected_xes=minera::to_xes(value,"Saved DFG alignment projection");}catch(const std::exception& e){projected_xes_error=e.what();}
  }catch(const std::exception& e){log_error=e.what();projection_complete=false;}
 }catch(const std::exception& e){error=e.what();}
 std::string projection_error;if(!metrics_error.empty())projection_error="metrics: "+metrics_error;if(!log_error.empty()){if(!projection_error.empty())projection_error+="; ";projection_error+="log: "+log_error;}
 response.status=error.empty()?200:400;
 response.set_content("{\"family\":\"GRAPH_ALIGNMENT_WORKBENCH\",\"operation\":\"project_saved_dfg\",\"mode\":\"DFG_CLASSIC\",\"input_shape\":\"log\",\"result_shape\":\"log\",\"alignment_executed\":false,\"supplied_result\":true,\"executed\":"+std::string(executed?"true":"false")+",\"projection_complete\":"+(projection_complete?"true":"false")+",\"projection_requested\":"+q(projection_request)+",\"error\":"+(error.empty()?"null":"{\"type\":\"native_error\",\"message\":"+q(error)+"}")+ 
  ",\"input_context\":{\"format\":"+q(format)+",\"detected_format\":"+q(detected)+",\"dataset\":"+q(input_name)+",\"original_input_bytes\":"+std::to_string(original_input.size())+",\"input_event_count\":"+std::to_string(events)+",\"input_case_count\":"+std::to_string(cases)+",\"checked_event_count\":"+std::to_string(events)+",\"checked_case_count\":"+std::to_string(cases)+",\"origin\":"+q(origin)+",\"trace_index\":null,\"activity_attribute\":"+q(configured.activity_attribute)+",\"timestamp_attribute\":"+q(configured.timestamp_attribute)+",\"case_id_attribute\":"+q(case_selector)+"}"+
  ",\"model_context\":{\"format\":"+q(model_format)+",\"detected_format\":"+q(detected_model)+",\"dataset\":"+q(model_name)+",\"original_input_bytes\":"+std::to_string(original_model.size())+",\"dfg\":"+canonical+",\"document\":null,\"native_net\":null,\"markings\":null}"+
  ",\"saved_result_context\":{\"source\":"+q(request.has_file("saved_result_file")?"uploaded_file":"inline")+",\"name\":"+(request.has_file("saved_result_file")?q(saved_name):"null")+",\"bytes\":"+std::to_string(original_saved.size())+",\"input_shape\":"+q(saved_shape)+"}"+
  ",\"original_input\":"+q(original_input)+",\"original_model\":"+q(original_model)+",\"original_saved_result\":"+q(original_saved)+",\"input_table\":"+input_table+",\"input_table_error\":"+nullable(table_error)+",\"input_trace_words\":null,\"input_log\":"+input_log+",\"input_log_error\":"+nullable(input_log_error)+",\"input_raw_log\":"+input_raw+",\"options_context\":"+options+",\"result\":"+result+",\"projection\":"+metrics+",\"metrics_error\":"+nullable(metrics_error)+",\"log_projection_error\":"+nullable(log_error)+",\"projected_log\":"+projected+",\"projected_raw_log\":"+projected_raw+",\"projected_log_error\":"+nullable(projected_log_error)+",\"projected_xes\":"+nullable(projected_xes)+",\"projected_xes_error\":"+nullable(projected_xes_error)+",\"projection_error\":"+nullable(projection_error)+",\"model_export\":"+nullable(model_export)+",\"model_export_format\":\"dfg\",\"model_export_error\":"+nullable(model_export_error)+"}","application/json");
}
