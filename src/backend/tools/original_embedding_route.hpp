#pragma once
// Global include and immutable startup callback instructions: integration.json.
// Every native input is request-owned; no cached session is accessed.
void handle_original_embedding_workbench(const httplib::Request& request,httplib::Response& response){
 using namespace minera;
 try{
  if(!request.is_multipart_form_data())throw std::invalid_argument("embedding workbench requires multipart data");
  const auto supplied=[&](std::string_view name){return active_source_supplied(request,name);};
  const auto field=[&](std::string_view name){return active_source_field(request,name);};
  const std::vector<std::string> roles={"source_file","embedding_result_file","target_file","text_file"};
  using Upload=std::decay_t<decltype(request.get_file_value("source_file"))>;
  std::map<std::string,Upload> uploads;
  for(const auto& role:roles){if(request.params.count(role)+request.files.count(role)>1)throw std::invalid_argument("ambiguous duplicate embedding upload: "+role);if(request.has_file(role))uploads.emplace(role,request.get_file_value(role));}
  const auto q=serialize_embedding_workbench_string_json;
  const auto flag=[](bool value){return value?"true":"false";};
  const auto nullable=[&](const std::string& value){return value.empty()?std::string("null"):q(value);};
  const auto exception_name=[](const std::exception& error){return dynamic_cast<const std::invalid_argument*>(&error)?"invalid_argument":dynamic_cast<const std::overflow_error*>(&error)?"overflow_error":dynamic_cast<const std::out_of_range*>(&error)?"out_of_range":dynamic_cast<const std::runtime_error*>(&error)?"runtime_error":"exception";};
  const auto error_value=[&](std::string_view type,const std::exception& error){return "{\"type\":"+q(type)+",\"message\":"+q(error.what())+",\"exception_type\":"+q(exception_name(error))+'}';};
  const auto encoded=[](std::string_view bytes){bool text=true;for(std::size_t i=0;i<bytes.size()&&text;){const auto a=static_cast<unsigned char>(bytes[i++]);if(a<128)continue;const int n=a>=0xc2&&a<=0xdf?1:a>=0xe0&&a<=0xef?2:a>=0xf0&&a<=0xf4?3:-1;if(n<0||i+static_cast<std::size_t>(n)>bytes.size()){text=false;break;}unsigned code=a&(n==1?31:n==2?15:7);for(int j=0;j<n;++j){const auto b=static_cast<unsigned char>(bytes[i++]);if((b&192)!=128){text=false;break;}code=(code<<6)|(b&63);}if((n==1&&code<128)||(n==2&&code<2048)||(n==3&&code<65536)||code>0x10ffff||(code>=0xd800&&code<=0xdfff))text=false;}std::string data(bytes);if(!text){constexpr char h[]="0123456789abcdef";data.clear();for(const unsigned char c:bytes){data+=h[c>>4];data+=h[c&15];}}return std::pair{text,std::move(data)};};
  const auto original=[&](std::string_view name,std::string_view bytes){const auto [text,data]=encoded(bytes);return "{\"dataset\":"+q(name)+",\"encoding\":"+q(text?"utf8":"hex")+",\"data\":"+q(data)+",\"bytes\":"+std::to_string(bytes.size())+",\"sha256\":"+q(footprints_workbench_sha256(bytes))+'}';};

  const auto original_bytes=[&](std::string_view bytes){const auto [text,data]=encoded(bytes);return "\"original_input_encoding\":"+q(text?"utf8":"hex")+",\"original_input\":"+q(data)+",\"original_input_bytes\":"+std::to_string(bytes.size());};
  std::string raw_controls="[";
  const auto control=[&](const std::string& name,const std::string& bytes){if(raw_controls.size()>1)raw_controls+=',';raw_controls+="{\"name\":"+q(name)+",\"original\":"+original(name,bytes)+'}';};
  for(const auto& [name,value]:request.params)control(name,value);
  for(const auto& [name,value]:request.files)if(std::find(roles.begin(),roles.end(),name)==roles.end())control(name,value.content);
  raw_controls+=']';
  struct Spec{bool source=false,embedding=false,target=false,text=false,top_k=false;std::string result;};
  const std::map<std::string,Spec> specs={{"filter",{true,false,true,false,true,"event_log"}},{"to_embeddings",{true,false,false,false,false,"embeddings"}},{"similarity_result",{false,true,true,false,true,"matches"}},{"similarity_log",{true,false,true,false,true,"similarity"}},{"embed_text",{false,false,false,true,false,"vector"}}};
  const auto operation=supplied("operation")?field("operation"):"filter";
  Spec spec;if(const auto it=specs.find(operation);it!=specs.end())spec=it->second;
  const auto source_format=supplied("source_format")?field("source_format"):"import";
  const auto binding=supplied("transformer_binding")?field("transformer_binding"):"native_default";
  const auto options_text=supplied("options_json")?field("options_json"):"{}";
  const auto submitted_options=supplied("options_json")&&encoded(options_text).first?q(options_text):std::string("null");
  std::size_t top_k=0;bool executed=false;
  std::optional<EmbeddingOptions> options;
  std::optional<EmbeddingResult> embedding_input,prepared_embeddings;
  std::optional<EventLog> raw_log,prepared_log,filtered;
  std::optional<ActiveEventSource> active;
  std::optional<std::vector<EmbeddingSimilarity>> matches;
  std::optional<std::vector<double>> vector;
  std::set<std::string> option_members;
  std::map<std::string,std::set<std::string>> input_members;
  std::map<std::string,std::string> before,native_kind,formats;
  formats={{"source_file",source_format},{"embedding_result_file","embedding_result_json"},{"target_file","raw_bytes"},{"text_file","raw_bytes"}};
  std::string calls="[",failure="null",phase="configuration_error",source_error,result_value="null";
  const auto invoke=[&](std::string_view name,std::string_view stage,std::string_view kind,const auto& call){if(calls.size()>1)calls+=',';const auto prefix="{\"name\":"+q(name)+",\"stage\":"+q(stage)+",\"input_kind\":"+q(kind)+",\"returned\":";try{call();calls+=prefix+"true,\"error\":null}";}catch(const std::exception& error){calls+=prefix+"false,\"error\":"+error_value("native_error",error)+'}';throw;}};
  const auto ids=[](const auto& values){std::string json="[";for(const auto value:values){if(json.size()>1)json+=',';json+=std::to_string(value);}return json+']';};
  const auto conversion_options=[&](const LogConversionOptions& o){return "{\"variant\":{\"name\":"+q(log_conversion_variant_name(o.variant))+",\"index\":"+std::to_string(static_cast<int>(o.variant))+"},\"include_case_attributes\":"+flag(o.include_case_attributes)+",\"case_attribute_prefix\":"+q(o.case_attribute_prefix)+",\"case_id_key\":"+q(o.case_id_key)+",\"remove_null_fields\":"+flag(o.remove_null_fields)+",\"max_rows_to_materialize\":"+(o.max_rows_to_materialize?std::to_string(*o.max_rows_to_materialize):"null")+",\"deep_copy\":"+flag(o.deep_copy)+",\"compress\":"+flag(o.compress)+",\"lazy_output\":"+flag(o.lazy_output)+",\"graph_case_id_attribute\":"+q(o.graph_case_id_attribute)+",\"include_directly_follows\":"+flag(o.include_directly_follows)+",\"case_attributes_as_nodes\":"+serialize_alignment_trace_words_json(o.case_attributes_as_nodes)+",\"event_attributes_as_nodes\":"+serialize_alignment_trace_words_json(o.event_attributes_as_nodes)+",\"return_variants\":"+flag(o.return_variants)+",\"max_variants_to_return\":"+(o.max_variants_to_return?std::to_string(*o.max_variants_to_return):"null")+",\"max_graph_nodes\":"+std::to_string(o.max_graph_nodes)+",\"max_graph_edges\":"+std::to_string(o.max_graph_edges)+'}';};
  const auto table=[](const EventTable* value){return value?serialize_embedding_workbench_table_json(*value):std::string("null");};
  const auto log=[](const EventLog* value){return value?serialize_embedding_workbench_raw_log_json(*value):std::string("null");};
  const auto active_snapshot=[&](){
   if(!active)return std::string("null");const auto& a=*active;const auto& s=a.selectors();const auto* lazy=a.checked_lazy_if_available();
   std::string lazy_table="null",lazy_count="null";if(lazy){lazy_table=serialize_embedding_workbench_table_json(lazy->materialize());if(const auto count=lazy->row_count())lazy_count=std::to_string(*count);}
   std::string representatives="[";for(const auto& row:a.representative_row_ordinals()){if(representatives.size()>1)representatives+=',';representatives+='[';bool first=true;for(const auto cell:row){if(!first)representatives+=',';first=false;representatives+=cell?std::to_string(*cell):"null";}representatives+=']';}representatives+=']';
   std::string changes="[";for(const auto& change:a.projection_changes()){if(changes.size()>1)changes+=',';changes+="{\"checked_row\":"+std::to_string(change.checked_row)+",\"source_row\":"+std::to_string(change.source_row)+",\"column\":"+q(change.column)+",\"removed\":"+flag(change.removed)+'}';}changes+=']';
   return "{\"dataset\":"+q(a.file().name)+",\"format\":"+q(a.file().format=="table"?"table":"auto")+",\"detected_format\":"+q(a.file().format)+','+original_bytes(a.file().bytes)+",\"source_selectors\":{\"case_id_key\":"+q(s.case_id_key)+",\"activity_key\":"+q(s.activity_key)+",\"timestamp_key\":"+q(s.timestamp_key)+",\"resource_key\":"+q(s.resource_key)+",\"lifecycle_key\":"+q(s.lifecycle_key)+"},\"input_before\":"+table(a.original_table_if_available())+",\"input_after\":"+table(a.original_table_if_available())+",\"checked_table\":"+table(a.checked_table_if_available())+",\"original_log\":"+log(a.original_log_if_available())+",\"prepared_log\":"+log(a.checked_log_if_available())+",\"log_preparation_error\":"+nullable(a.checked_log_preparation_error())+",\"flat_projection_error\":"+nullable(a.checked_flat_projection_error())+",\"selected_shape\":"+q(active_source_shape(a.selected_kind()))+",\"selected_kind_index\":"+std::to_string(static_cast<int>(a.selected_kind()))+",\"prepared_stream\":"+(a.checked_stream_if_available()?serialize_embedding_workbench_stream_json(*a.checked_stream_if_available()):"null")+",\"prepared_lazy_table\":"+lazy_table+",\"lazy_row_count\":"+lazy_count+",\"lazy_valid\":"+(lazy?std::string(flag(lazy->valid())):"null")+",\"original_table_origin\":"+q(a.original_table_origin())+",\"checked_table_origin\":"+q(a.checked_table_origin())+",\"source_row_ordinals\":"+ids(a.source_row_ordinals())+",\"representative_row_ordinals\":"+representatives+",\"projection_changes\":"+changes+",\"preparation_options\":"+conversion_options(a.preparation_options())+'}';
  };
  const auto source_state=[&](){return raw_log?serialize_embedding_workbench_raw_log_json(*raw_log):active_snapshot();};
  const auto remember=[&](const std::string& role,const std::string& state,std::string kind,bool json){before[role]=state;native_kind[role]=std::move(kind);if(json)input_members[role]=footprints_workbench_supplied_members(uploads.at(role).content);};

  try{
   static const std::set<std::string> source_controls={"mining_input_format","mining_input_shape","mining_selectors_json","case_column","activity_column","timestamp_column","resource_column","lifecycle_column","delimiter","xes_variant","xes_case_attribute","xes_activity_attribute","xes_timestamp_attribute","xes_resource_attribute","xes_lifecycle_attribute","xes_require_timestamp","xes_timestamp_sort","xes_reverse_sort","xes_max_traces","xes_attributes_to_read","xes_max_decompressed_bytes"};
   static const std::set<std::string> controls={"operation","source_format","options_json","top_k","transformer_binding"};
   const auto check=[&](const std::string& name){if(request.params.count(name)+request.files.count(name)!=1)throw std::invalid_argument("duplicate embedding control: "+name);if(std::find(roles.begin(),roles.end(),name)!=roles.end()){if(!uploads.contains(name))throw std::invalid_argument("embedding upload must be a file: "+name);}else if(!controls.contains(name)&&!source_controls.contains(name))throw std::invalid_argument("unknown embedding control: "+name);};
   for(const auto& [name,value]:request.params){(void)value;check(name);}for(const auto& [name,value]:request.files){(void)value;check(name);}
   if(!specs.contains(operation))throw std::invalid_argument("unsupported embedding operation: "+operation);
   const std::map<std::string,bool> required_roles={{"source_file",spec.source},{"embedding_result_file",spec.embedding},{"target_file",spec.target},{"text_file",spec.text}};
   for(const auto& [role,required]:required_roles)if(required!=uploads.contains(role))throw std::invalid_argument((required?"missing embedding upload: ":"inapplicable embedding upload: ")+role);
   if(supplied("source_format")&&!spec.source)throw std::invalid_argument("source_format requires source_file");
   if(spec.source&&source_format!="raw_log"&&source_format!="import")throw std::invalid_argument("source_format must be raw_log or import");
   for(const auto& name:source_controls)if(supplied(name)&&(!spec.source||source_format!="import"))throw std::invalid_argument("source import control is inapplicable: "+name);
   if(supplied("top_k")&&!spec.top_k)throw std::invalid_argument("top_k is inapplicable to this operation");
   if(supplied("top_k"))top_k=parse_embedding_workbench_size(field("top_k"));
   if(binding!="native_default"&&binding!="server")throw std::invalid_argument("transformer_binding must be native_default or server");
   if(binding=="server"&&(!g_embedding_runtime_enabled||!g_embedding_explicit_runtime))throw std::invalid_argument("server transformer binding is unavailable");
   options=parse_embedding_workbench_options_json(options_text,&option_members);
   if(binding=="server")options->transformer_callback=g_embedding_explicit_runtime;
   phase="preparation_error";
   for(const auto& role:{"target_file","text_file"})if(uploads.contains(role)){const auto& file=uploads.at(role);remember(role,original(file.filename,file.content),"string_view",false);}
   if(spec.embedding){embedding_input=parse_embedding_workbench_result_json(uploads.at("embedding_result_file").content);remember("embedding_result_file",serialize_embedding_workbench_result_json(*embedding_input),"EmbeddingResult",true);}
   if(spec.source){const auto& file=uploads.at("source_file");try{
    if(source_format=="raw_log"){raw_log=parse_embedding_workbench_raw_log_json(file.content);remember("source_file",source_state(),"EventLog",true);prepared_log=*raw_log;}
    else{auto params=parse_mine_params(field,supplied);if(field("mining_input_format")=="table"){
      auto native_table=parse_embedding_workbench_table_json(file.content);ActiveSourceSelectors selectors;
      if(supplied("mining_selectors_json"))for(const auto& [key,value]:parse_workflow_selector_strings_json(field("mining_selectors_json"))){if(key=="case_id_key")selectors.case_id_key=value;else if(key=="activity_key")selectors.activity_key=value;else if(key=="timestamp_key")selectors.timestamp_key=value;else if(key=="resource_key")selectors.resource_key=value;else if(key=="lifecycle_key")selectors.lifecycle_key=value;else throw std::invalid_argument("unsupported literal mining source selector: "+key);}
      const auto kind=active_source_kind(field("mining_input_shape"));invoke("ActiveEventSource::from_table(table,file,selectors,kind)","preparation","EventTable",[&]{active=ActiveEventSource::from_table(std::move(native_table),ActiveSourceFile{file.filename,"table",file.content},selectors,kind);});
     }else invoke("prepare_active_event_source(request,bytes,dataset,imports)","preparation","original_source_file",[&]{active=prepare_active_event_source(request,file.content,file.filename,params);});
     remember("source_file",source_state(),"ActiveEventSource",false);
     invoke("ActiveEventSource::checked_log()","preparation","ActiveEventSource",[&]{prepared_log=active->checked_log();});
    }
   }catch(const std::exception& error){source_error=error.what();throw;}}
   phase="native_error";
   if(operation=="filter")invoke("filter_embeddings_top_k(log,target,top_k,options)","primary","EventLog",[&]{filtered=filter_embeddings_top_k(*prepared_log,uploads.at("target_file").content,top_k,*options);result_value=serialize_embedding_workbench_raw_log_json(*filtered);});
   else if(operation=="to_embeddings")invoke("to_embeddings(log,options)","primary","EventLog",[&]{prepared_embeddings=to_embeddings(*prepared_log,*options);result_value=serialize_embedding_workbench_result_json(*prepared_embeddings);});
   else if(operation=="similarity_result")invoke("embedding_similarity(target,embedding_result,options,top_k)","primary","EmbeddingResult",[&]{matches=embedding_similarity(uploads.at("target_file").content,*embedding_input,*options,top_k);result_value=serialize_embedding_workbench_matches_json(*matches);});
   else if(operation=="similarity_log"){
    invoke("to_embeddings(log,options)","primary","EventLog",[&]{prepared_embeddings=to_embeddings(*prepared_log,*options);});
    invoke("embedding_similarity(target,prepared_embeddings,options,top_k)","primary","EmbeddingResult",[&]{matches=embedding_similarity(uploads.at("target_file").content,*prepared_embeddings,*options,top_k);});
    result_value="{\"embeddings\":"+serialize_embedding_workbench_result_json(*prepared_embeddings)+",\"matches\":"+serialize_embedding_workbench_matches_json(*matches)+'}';
   }else invoke("embed_text(text,options)","primary","string_view",[&]{vector=embed_text(uploads.at("text_file").content,*options);result_value=serialize_embedding_workbench_vector_json(*vector);});
   executed=true;
  }catch(const std::exception& error){failure=error_value(phase,error);}
  const std::vector<std::string> artifacts={"source_log_json","source_xes","embedding_result_input_json","prepared_embeddings_json","result_json","filtered_log_json","filtered_xes","legacy_embeddings_json","legacy_similarity_json"};
  std::map<std::string,std::optional<std::string>> exports;std::map<std::string,std::string> export_errors;
  for(const auto& key:artifacts){exports[key]=std::nullopt;export_errors[key]="null";}
  const auto artifact=[&](const std::string& key,const auto& make){try{exports[key]=make();}catch(const std::exception& error){export_errors[key]=error_value("export_error",error);}};
  if(prepared_log){artifact("source_log_json",[&]{return serialize_embedding_workbench_raw_log_json(*prepared_log);});artifact("source_xes",[&]{std::string value;invoke("to_xes(input_log)","export","EventLog",[&]{value=to_xes(*prepared_log,"Original embedding source");});return value;});}
  if(embedding_input)artifact("embedding_result_input_json",[&]{return serialize_embedding_workbench_result_json(*embedding_input);});
  if(prepared_embeddings)artifact("prepared_embeddings_json",[&]{return serialize_embedding_workbench_result_json(*prepared_embeddings);});
  if(executed)artifact("result_json",[&]{return result_value;});
  if(filtered){artifact("filtered_log_json",[&]{return serialize_embedding_workbench_raw_log_json(*filtered);});artifact("filtered_xes",[&]{std::string value;invoke("to_xes(filtered_log)","export","EventLog",[&]{value=to_xes(*filtered,"Original embedding filter result");});return value;});}
  const auto* embeddings=embedding_input?&*embedding_input:prepared_embeddings?&*prepared_embeddings:nullptr;
  if(embeddings)artifact("legacy_embeddings_json",[&]{std::string value;invoke("serialize_embeddings_json(embeddings)","export","EmbeddingResult",[&]{value=serialize_embeddings_json(*embeddings);});return value;});
  if(matches&&embeddings)artifact("legacy_similarity_json",[&]{std::string value;invoke("serialize_embedding_similarity_json(target,embeddings,matches,top_k)","export","EmbeddingResult",[&]{value=serialize_embedding_similarity_json(uploads.at("target_file").content,*embeddings,*matches,top_k);});return value;});
  calls+=']';
  std::string exports_json="{",artifact_originals="{";
  for(const auto& key:artifacts){if(exports_json.size()>1){exports_json+=',';artifact_originals+=',';}exports_json+=q(key)+':'+(exports[key]&&encoded(*exports[key]).first?q(*exports[key]):"null")+','+q(key+"_error")+':'+export_errors[key];artifact_originals+=q(key)+':'+(exports[key]?original(key,*exports[key]):"null");}exports_json+='}';artifact_originals+='}';
  const auto strings=[&](const std::set<std::string>& values){std::string json="[";for(const auto& v:values){if(json.size()>1)json+=',';json+=q(v);}return json+']';};
  std::map<std::string,std::string> after;
  if(raw_log||active)after["source_file"]=source_state();
  if(embedding_input)after["embedding_result_file"]=serialize_embedding_workbench_result_json(*embedding_input);
  for(const auto& role:{"target_file","text_file"})if(before.contains(role)){const auto& f=uploads.at(role);after[role]=original(f.filename,f.content);}
  std::string context="{\"size_t_bytes\":"+std::to_string(sizeof(std::size_t))+",\"source_format\":"+q(source_format)+",\"top_k\":"+std::to_string(top_k)+",\"top_k_supplied\":"+flag(supplied("top_k"))+",\"transformer_binding\":"+q(binding)+",\"uploads\":{";
  bool first=true;for(const auto& role:roles){if(!first)context+=',';first=false;context+=q(role)+':';if(!uploads.contains(role)){context+="null";continue;}const auto& f=uploads.at(role);context+="{\"dataset\":"+q(f.filename)+",\"bytes\":"+std::to_string(f.content.size())+",\"sha256\":"+q(footprints_workbench_sha256(f.content))+",\"encoding\":"+q(encoded(f.content).first?"utf8":"hex")+",\"format\":"+q(formats[role])+",\"native_kind\":"+(native_kind.contains(role)?q(native_kind[role]):"null")+",\"supplied_members\":"+strings(input_members[role])+",\"unchanged\":"+(before.contains(role)?std::string(flag(before[role]==after[role])):"null")+",\"before\":"+(before.contains(role)?before[role]:"null")+",\"after\":"+(after.contains(role)?after[role]:"null")+'}';}context+="}}";
  const auto role_original=[&](const std::string& role){const auto& f=uploads.at(role);return original(f.filename,f.content);};
  const auto source_json=uploads.contains("source_file")?"{\"original\":"+role_original("source_file")+",\"format\":"+q(source_format)+",\"raw_log\":"+(raw_log?serialize_embedding_workbench_raw_log_json(*raw_log):"null")+",\"active_source\":"+active_snapshot()+",\"prepared_log\":"+(prepared_log?serialize_embedding_workbench_raw_log_json(*prepared_log):"null")+",\"preparation_error\":"+nullable(source_error)+'}':"null";
  const auto input_json=uploads.contains("embedding_result_file")?"{\"original\":"+role_original("embedding_result_file")+",\"value\":"+(embedding_input?serialize_embedding_workbench_result_json(*embedding_input):"null")+'}':"null";
  const auto text_role=[&](const std::string& role){if(!uploads.contains(role))return std::string("null");const auto& f=uploads.at(role);return "{\"original\":"+role_original(role)+",\"text\":"+(encoded(f.content).first?q(f.content):"null")+'}';};
  const auto runtime="{\"transformer_binding\":"+q(binding)+",\"registry_available\":"+flag(g_embedding_runtime_enabled)+",\"registry_source\":"+(g_embedding_runtime_enabled?q("server_http_bridge"):"null")+",\"options_callback_present\":"+(options?std::string(flag(static_cast<bool>(options->transformer_callback))):"null")+",\"options_callback_source\":"+(options?q(options->transformer_callback?"server_http_bridge":"native_empty"):"null")+'}';
  response.status=failure=="null"?200:400;
  response.set_content(active_source_safe_json("{\"family\":\"ORIGINAL_EMBEDDING_WORKBENCH\",\"operation\":"+q(operation)+",\"executed\":"+flag(executed)+",\"native_calls\":"+calls+",\"error\":"+failure+",\"input_context\":"+context+",\"source\":"+source_json+",\"embedding_result_input\":"+input_json+",\"target\":"+text_role("target_file")+",\"text\":"+text_role("text_file")+",\"submitted_options\":"+submitted_options+",\"configured_options\":"+(options?serialize_embedding_workbench_options_json(*options):"null")+",\"supplied_options\":"+strings(option_members)+",\"raw_controls\":"+raw_controls+",\"runtime\":"+runtime+",\"prepared_embeddings\":"+(prepared_embeddings?serialize_embedding_workbench_result_json(*prepared_embeddings):"null")+",\"result\":"+(executed?"{\"type\":"+q(spec.result)+",\"value\":"+result_value+'}':"null")+",\"exports\":"+exports_json+",\"artifact_originals\":"+artifact_originals+'}'),"application/json");
 }catch(const std::exception& error){response.status=400;response.set_content("{\"error\":"+minera::serialize_embedding_workbench_string_json(error.what())+'}',"application/json");}
}
