#pragma once
// Independent duration computations consume caller cohorts or saved event-level
// diagnostics. This route never invokes token replay or publishes a session.
void handle_replay_duration_workbench(const httplib::Request& request, httplib::Response& response) {
 try {
  if(!request.is_multipart_form_data()||!request.has_file("log"))throw std::invalid_argument("duration analysis requires multipart log and analysis");
  const auto lookup=[&](std::string_view n){return active_source_field(request,n);};
  const auto present=[&](std::string_view n){return active_source_supplied(request,n);};
  if(!present("analysis"))throw std::invalid_argument("duration analysis requires analysis JSON");
  auto analysis=minera::parse_replay_duration_workbench_json(lookup("analysis"));
  const bool saved_file=present("saved_diagnostics_file");
  if(saved_file){
   if(analysis.mode!="saved_diagnostics")throw std::invalid_argument("saved_diagnostics_file requires saved_diagnostics mode");
   if(analysis.has_saved_diagnostics)throw std::invalid_argument("cannot combine inline saved_diagnostics with saved_diagnostics_file");
   if(!request.has_file("saved_diagnostics_file"))throw std::invalid_argument("saved_diagnostics_file must be a multipart File");
   analysis.saved_diagnostics=minera::parse_saved_replay_duration_diagnostics_json(request.get_file_value("saved_diagnostics_file").content,analysis.saved_input_shape);
   analysis.has_saved_diagnostics=true;
  }
  if(analysis.mode=="saved_diagnostics"&&!analysis.has_saved_diagnostics)throw std::invalid_argument("saved diagnostic reanalysis requires saved diagnostics");
  const auto params=minera::parse_mine_params(lookup,present);
  const auto adapter=minera::parse_token_replay_input_adapter(lookup,present);
  const auto& file=request.get_file_value("log");
  const auto source_format=present("duration_input_format")?lookup("duration_input_format"):"auto";
  if(source_format!="auto"&&source_format!="raw_log")throw std::invalid_argument("duration_input_format must be auto or raw_log");
  std::optional<minera::ActiveEventSource> source;
  minera::EventLog source_log;
  std::optional<minera::EventTable> source_table;
  std::string detected_format="raw_log";
  if(source_format=="raw_log")source_log=minera::parse_embedding_workbench_raw_log_json(file.content);
  else {
   auto source_params=params;source=prepare_active_event_source(request,file.content,file.filename,source_params);
   source_log=source->checked_log();detected_format=source->file().format;
   if(detected_format!="xes")source_table=source->checked_table();
  }
  std::optional<minera::PnmlDocument> document;
  std::string model_error;
  if(request.has_file("model")) {
   try {
   auto imports=minera::tools::pnml_model_import_options();imports.import_stochastic_map=false;
   document=minera::parse_pnml_document(request.get_file_value("model").content,imports);
   } catch(const std::exception& e) { model_error=e.what(); }
  }
  if(analysis.mode=="saved_diagnostics"&&!document)throw std::invalid_argument(model_error.empty()?"saved diagnostic reanalysis requires the original PNML model":model_error);
  auto options=minera::tools::default_replay_input_options(source_log,params.token_replay,analysis.activity_attribute.has_value()||params.token_replay_activity_attribute_supplied);
  if(source&&(detected_format=="table"||present("mining_selectors_json"))){
   if(!present("token_replay_activity_attribute")&&!analysis.activity_attribute)options.activity_attribute=source->selectors().activity_key;
   if(!present("token_replay_case_id_attribute"))options.case_id_attribute=source->selectors().case_id_key;
   if(!analysis.timestamp_attribute)options.timestamp_attribute=source->selectors().timestamp_key;
  }
  if(source&&detected_format!="table"&&!present("mining_selectors_json")&&detected_format!="xes"&&!present("token_replay_case_id_attribute"))options.case_id_attribute=params.csv.case_id_column;
  if(source_table&&source_log.event_count()==0&&!analysis.activity_attribute&&!params.token_replay_activity_attribute_supplied&&detected_format!="table"){
   auto selected=std::find(source_table->columns.begin(),source_table->columns.end(),params.csv.activity_column);
   if(selected!=source_table->columns.end())options.activity_attribute=*selected;
  }
  if(analysis.activity_attribute)options.activity_attribute=*analysis.activity_attribute;
  if(analysis.timestamp_attribute)options.timestamp_attribute=*analysis.timestamp_attribute;
  // Preserve the ordinary imported CSV timestamp source for omitted controls.
  if(!analysis.timestamp_attribute&&source_format=="auto"&&detected_format=="csv"&&options.timestamp_attribute=="time:timestamp")options.timestamp_attribute=params.csv.timestamp_column;
  minera::EventLog log;
  if(adapter=="log")log=source_log;
  else if(adapter=="stream"){
   const auto stream=source_table?minera::event_table_to_event_stream(*source_table):minera::event_log_to_event_stream(source_log);
   log=minera::normalize_token_replay_event_log(minera::EventDataView(stream),options);
  }else if(adapter=="dataframe"){
   const auto table=source_table?*source_table:minera::event_log_to_table(source_log);
   log=minera::normalize_token_replay_event_log(minera::EventDataView(table),options);
  }else{
   const auto lazy=source_table?minera::LazyEventTable::from_table(*source_table):minera::event_log_to_lazy_table(source_log);
   log=minera::normalize_token_replay_event_log(minera::EventDataView(lazy),options);
  }
  const auto q=minera::serialize_embedding_workbench_string_json;
  std::string result="{\"duration_diagnostics\":null,\"case_duration_seconds\":null,\"median_duration_seconds\":null}";
  std::string computation="{\"complete\":true,\"error\":null,\"error_type\":null,\"error_args\":[]}";
  const auto failure=[&](const std::exception& e,std::string type,std::string arg){computation="{\"complete\":false,\"error\":"+q(e.what())+",\"error_type\":"+q(type)+",\"error_args\":["+q(arg)+"]}";};
  try{result=minera::serialize_replay_duration_workbench_result_json(log,document?&document->net:nullptr,analysis,options);}
  catch(const minera::TokenReplayDurationError& e){failure(e,e.reference_type(),e.reference_args()[0]);}
  catch(const minera::TokenReplayKeyError& e){failure(e,e.reference_type(),e.reference_args()[0]);}
  catch(const std::out_of_range& e){failure(e,"std::out_of_range",e.what());}
  catch(const std::exception& e){failure(e,"std::invalid_argument",e.what());}
  std::string errors="[",prepared="null",before="null",after="null";bool first_error=true;
  const auto presentation=[&](std::string_view name,const auto& render){try{auto rendered=render();if(const auto fault=minera::literal_controls_transport_error(rendered))throw std::invalid_argument("optional presentation transport: "+*fault);return rendered;}catch(const std::exception& e){if(!first_error)errors+=',';first_error=false;errors+="{\"stage\":"+q(name)+",\"error\":"+q(e.what())+'}';return std::string("null");}};
  prepared=presentation("prepared_log",[&]{return minera::serialize_active_source_log_json(log);});
  if(source_table){before=presentation("input_before",[&]{return minera::serialize_event_table_input_json(*source_table);});after=before;}
  std::string model_context="null",original_model="null",model_name="null";std::size_t model_bytes=0;
  if(request.has_file("model")){const auto& model=request.get_file_value("model");original_model=q(model.content);model_name=q(model.filename);model_bytes=model.content.size();}
  if(!model_error.empty()){if(!first_error)errors+=',';first_error=false;errors+="{\"stage\":\"model_import\",\"error\":"+q(model_error)+'}';}
  if(document){
   model_context="{\"places\":[";
   for(std::size_t p=0;p<document->net.place_count();++p){if(p)model_context+=',';model_context+="{\"index\":"+std::to_string(p)+",\"id\":"+q(document->place_ids.at(p))+",\"initial_tokens\":"+minera::TokenInteger(document->initial_marking.at(p)).decimal()+",\"final_tokens\":"+minera::TokenInteger(document->final_marking.at(p)).decimal()+",\"initial_key\":"+(document->initial_marking_keys.at(p)?"true":"false")+",\"final_key\":"+(document->final_marking_keys.at(p)?"true":"false")+'}';}
   model_context+="],\"transitions\":[";
   for(std::size_t t=0;t<document->net.transitions().size();++t){if(t)model_context+=',';const auto& transition=document->net.transitions()[t];model_context+="{\"index\":"+std::to_string(t)+",\"id\":"+q(document->transition_ids.at(t))+",\"label\":"+(transition.activity?q(document->net.activities().at(*transition.activity)):"null")+'}';}model_context+="]}";
  }
  std::string original_saved="null",saved_name="null";std::size_t saved_bytes=0;
  if(saved_file){const auto& saved=request.get_file_value("saved_diagnostics_file");original_saved=q(saved.content);saved_name=q(saved.filename);saved_bytes=saved.content.size();}
  const auto saved_context="{\"source\":"+q(saved_file?"uploaded_file":analysis.has_saved_diagnostics?"inline":"none")+",\"name\":"+saved_name+",\"bytes\":"+std::to_string(saved_bytes)+",\"input_shape\":"+q(analysis.saved_input_shape)+'}';
  std::string body="{\"family\":\"REPLAY_DURATION_WORKFLOW\",\"mode\":"+q(analysis.mode)+",\"replay_executed\":false,\"analysis_raw\":"+q(analysis.raw_json)+",\"original_saved_diagnostics\":"+original_saved+",\"saved_diagnostics_context\":"+saved_context+",\"input\":"+analysis.raw_json+",\"result\":"+result+",\"computation\":"+computation+",\"input_context\":{\"adapter\":"+q(adapter)+",\"origin\":"+q(source_format=="raw_log"?"uploaded_raw_log":detected_format=="table"?"uploaded_typed_table":source_table&&adapter!="log"?"uploaded_csv_rows":"uploaded_grouped_log")+",\"detected_format\":"+q(detected_format)+",\"dataset\":"+q(file.filename)+",\"model\":"+model_name+",\"log_bytes\":"+std::to_string(file.content.size())+",\"model_bytes\":"+std::to_string(model_bytes)+",\"source_case_count\":"+std::to_string(source_log.trace_count())+",\"source_event_count\":"+std::to_string(source_log.event_count())+",\"evaluated_case_count\":"+std::to_string(log.trace_count())+",\"evaluated_event_count\":"+std::to_string(log.event_count())+",\"activity_attribute\":"+q(options.activity_attribute)+",\"timestamp_attribute\":"+q(options.timestamp_attribute)+",\"case_id_attribute\":"+q(options.case_id_attribute)+"},\"model_context\":"+model_context+",\"original_log\":"+q(file.content)+",\"original_model\":"+original_model+",\"input_before\":"+before+",\"input_after\":"+after+",\"prepared_log\":"+prepared+",\"prepared_raw_log\":"+minera::serialize_embedding_workbench_raw_log_json(log)+",\"presentation_errors\":"+errors+"]}";
  response.set_content(body,"application/json");
 }catch(const std::exception& e){response.status=400;response.set_content(error_json(e.what()),"application/json");}
}
