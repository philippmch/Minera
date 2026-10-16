#pragma once
// Include after original_embedding_route.hpp; global codec/header dependencies
// and registration are listed in the staged integration.json.
void handle_original_emd_workbench(const httplib::Request& request,httplib::Response& response){
 using namespace minera;
 try{
  if(!request.is_multipart_form_data())throw std::invalid_argument("EMD workbench requires multipart data");
  const auto supplied=[&](std::string_view name){return active_source_supplied(request,name);};
  const auto field=[&](std::string_view name){return active_source_field(request,name);};
  const std::vector<std::string> roles={"left_file","right_file","source_file"};
  using Upload=std::decay_t<decltype(request.get_file_value("source_file"))>;
  std::map<std::string,Upload> uploads;
  for(const auto& role:roles){if(request.params.count(role)+request.files.count(role)>1)throw std::invalid_argument("ambiguous duplicate EMD upload: "+role);if(request.has_file(role))uploads.emplace(role,request.get_file_value(role));}
  const auto q=serialize_embedding_workbench_string_json;
  const auto flag=[](bool value){return value?"true":"false";};
  const auto exception_name=[](const std::exception& error){return dynamic_cast<const std::invalid_argument*>(&error)?"invalid_argument":dynamic_cast<const std::overflow_error*>(&error)?"overflow_error":dynamic_cast<const std::out_of_range*>(&error)?"out_of_range":dynamic_cast<const std::runtime_error*>(&error)?"runtime_error":"exception";};
  const auto error_value=[&](std::string_view type,const std::exception& error){return "{\"type\":"+q(type)+",\"message\":"+q(error.what())+",\"exception_type\":"+q(exception_name(error))+'}';};
  const auto encoded=[](std::string_view bytes){bool text=true;for(std::size_t i=0;i<bytes.size()&&text;){const auto a=static_cast<unsigned char>(bytes[i++]);if(a<128)continue;const int n=a>=0xc2&&a<=0xdf?1:a>=0xe0&&a<=0xef?2:a>=0xf0&&a<=0xf4?3:-1;if(n<0||i+static_cast<std::size_t>(n)>bytes.size()){text=false;break;}unsigned code=a&(n==1?31:n==2?15:7);for(int j=0;j<n;++j){const auto b=static_cast<unsigned char>(bytes[i++]);if((b&192)!=128){text=false;break;}code=(code<<6)|(b&63);}if((n==1&&code<128)||(n==2&&code<2048)||(n==3&&code<65536)||code>0x10ffff||(code>=0xd800&&code<=0xdfff))text=false;}std::string data(bytes);if(!text){constexpr char h[]="0123456789abcdef";data.clear();for(const unsigned char c:bytes){data+=h[c>>4];data+=h[c&15];}}return std::pair{text,std::move(data)};};
  const auto original=[&](std::string_view name,std::string_view bytes){const auto [text,data]=encoded(bytes);return "{\"dataset\":"+q(name)+",\"encoding\":"+q(text?"utf8":"hex")+",\"data\":"+q(data)+",\"bytes\":"+std::to_string(bytes.size())+",\"sha256\":"+q(footprints_workbench_sha256(bytes))+'}';};
  std::string raw_controls="[";
  const auto control=[&](const std::string& name,const std::string& bytes){if(raw_controls.size()>1)raw_controls+=',';raw_controls+="{\"name\":"+q(name)+",\"original\":"+original(name,bytes)+'}';};
  for(const auto& [name,value]:request.params)control(name,value);
  for(const auto& [name,value]:request.files)if(std::find(roles.begin(),roles.end(),name)==roles.end())control(name,value.content);
  raw_controls+=']';
  const auto operation=supplied("operation")?field("operation"):"evaluate_pair";
  const bool pair=operation=="evaluate_pair"||operation=="distance_pair";
  const bool derive=operation=="derive_original_log";
  const auto options_text=supplied("options_json")?field("options_json"):"{}";
  const auto submitted_options=supplied("options_json")&&encoded(options_text).first?q(options_text):std::string("null");
  std::optional<EmdOptions> options;
  std::optional<EmdWorkbenchCallbackTables> tables;
  const auto invocations=std::make_shared<std::vector<EmdWorkbenchCallbackInvocation>>();
  std::optional<StochasticLanguage> left,right,derived;
  std::optional<EventLog> source;
  std::optional<EmdResult> emd_result;
  std::optional<double> distance;
  std::optional<bool> normalize;
  std::set<std::string> option_members,table_members;
  std::map<std::string,std::set<std::string>> input_members;
  std::map<std::string,std::string> before,after,native_kind;
  std::string phase="configuration_error",failure="null",calls="[",result_value="null";
  bool executed=false;
  const auto invoke=[&](std::string_view name,std::string_view stage,std::string_view kind,const auto& call){if(calls.size()>1)calls+=',';const auto prefix="{\"name\":"+q(name)+",\"stage\":"+q(stage)+",\"input_kind\":"+q(kind)+",\"returned\":";try{call();calls+=prefix+"true,\"error\":null}";}catch(const std::exception& error){calls+=prefix+"false,\"error\":"+error_value("native_error",error)+'}';throw;}};
  const auto remember=[&](const std::string& role,std::string state,std::string kind){before[role]=std::move(state);native_kind[role]=std::move(kind);input_members[role]=footprints_workbench_supplied_members(uploads.at(role).content);};
  try{
   static const std::set<std::string> controls={"operation","options_json","callback_tables_json","normalize"};
   const auto check=[&](const std::string& name){if(request.params.count(name)+request.files.count(name)!=1)throw std::invalid_argument("duplicate EMD control: "+name);if(std::find(roles.begin(),roles.end(),name)!=roles.end()){if(!uploads.contains(name))throw std::invalid_argument("EMD upload must be a file: "+name);}else if(!controls.contains(name))throw std::invalid_argument("unknown EMD control: "+name);};
   for(const auto& [name,value]:request.params){(void)value;check(name);}for(const auto& [name,value]:request.files){(void)value;check(name);}
   if(!pair&&!derive)throw std::invalid_argument("unsupported EMD operation: "+operation);
   for(const auto& role:roles){const bool required=role=="source_file"?derive:pair;if(required!=uploads.contains(role))throw std::invalid_argument((required?"missing EMD upload: ":"inapplicable EMD upload: ")+role);}
   if(pair&&supplied("normalize"))throw std::invalid_argument("normalize is only applicable to derive_original_log");
   if(derive&&(supplied("options_json")||supplied("callback_tables_json")))throw std::invalid_argument("EMD options and callback tables are only applicable to pair operations");
   if(pair){
    options=parse_emd_workbench_options_json(options_text,&option_members);
    if(supplied("callback_tables_json")){tables=parse_emd_workbench_callback_tables_json(field("callback_tables_json"),&table_members);bind_emd_workbench_callback_tables(*options,*tables,invocations);}
   }else normalize=parse_emd_workbench_normalize_json(supplied("normalize")?field("normalize"):"false");
   phase="preparation_error";
   if(pair){left=parse_emd_workbench_language_json(uploads.at("left_file").content);remember("left_file",serialize_emd_workbench_language_json(*left),"StochasticLanguage");right=parse_emd_workbench_language_json(uploads.at("right_file").content);remember("right_file",serialize_emd_workbench_language_json(*right),"StochasticLanguage");}
   else{source=parse_embedding_workbench_raw_log_json(uploads.at("source_file").content);remember("source_file",serialize_embedding_workbench_raw_log_json(*source),"EventLog");}
   phase="native_error";
   if(operation=="evaluate_pair"){
    invoke("evaluate_emd(left,right,options)","primary","StochasticLanguage pair",[&]{emd_result=evaluate_emd(*left,*right,*options);});executed=true;result_value=serialize_emd_workbench_result_json(*emd_result);
   }else if(operation=="distance_pair"){
    invoke("earth_movers_distance(left,right,options)","primary","StochasticLanguage pair",[&]{distance=earth_movers_distance(*left,*right,*options);});executed=true;result_value=serialize_emd_workbench_scalar_json(*distance);
   }else{
    invoke("derive_stochastic_language(log,normalize)","primary","EventLog",[&]{derived=derive_stochastic_language(*source,*normalize);});executed=true;result_value=serialize_emd_workbench_language_json(*derived);
   }
  }catch(const std::exception& error){failure=error_value(phase,error);}
  const std::vector<std::string> artifacts={"left_language_json","right_language_json","source_log_json","source_xes","options_json","callback_tables_json","result_json","legacy_emd_json"};
  std::map<std::string,std::optional<std::string>> exports;std::map<std::string,std::string> export_errors;
  for(const auto& key:artifacts){exports[key]=std::nullopt;export_errors[key]="null";}
  const auto artifact=[&](const std::string& key,const auto& make){try{exports[key]=make();}catch(const std::exception& error){export_errors[key]=error_value("export_error",error);}};
  if(left)artifact("left_language_json",[&]{return serialize_emd_workbench_language_json(*left);});
  if(right)artifact("right_language_json",[&]{return serialize_emd_workbench_language_json(*right);});
  if(source){artifact("source_log_json",[&]{return serialize_embedding_workbench_raw_log_json(*source);});artifact("source_xes",[&]{std::string value;invoke("to_xes(source_log)","export","EventLog",[&]{value=to_xes(*source,"Original EMD source");});return value;});}
  if(options)artifact("options_json",[&]{return serialize_emd_workbench_options_json(*options);});
  if(tables)artifact("callback_tables_json",[&]{return serialize_emd_workbench_callback_tables_json(*tables);});
  if(executed)artifact("result_json",[&]{return result_value;});
  if(emd_result)artifact("legacy_emd_json",[&]{std::string value;invoke("emd_json(result)","export","EmdResult",[&]{value=emd_json(*emd_result);});return value;});
  calls+=']';
  std::string exports_json="{",artifact_originals="{";
  for(const auto& key:artifacts){if(exports_json.size()>1){exports_json+=',';artifact_originals+=',';}exports_json+=q(key)+':'+(exports[key]&&encoded(*exports[key]).first?q(*exports[key]):"null")+','+q(key+"_error")+':'+export_errors[key];artifact_originals+=q(key)+':'+(exports[key]?original(key,*exports[key]):"null");}exports_json+='}';artifact_originals+='}';
  if(left)after["left_file"]=serialize_emd_workbench_language_json(*left);if(right)after["right_file"]=serialize_emd_workbench_language_json(*right);if(source)after["source_file"]=serialize_embedding_workbench_raw_log_json(*source);
  const auto strings=[&](const std::set<std::string>& values){std::string json="[";for(const auto& value:values){if(json.size()>1)json+=',';json+=q(value);}return json+']';};
  std::string context="{\"size_t_bytes\":"+std::to_string(sizeof(std::size_t))+",\"uploads\":{";
  bool first=true;for(const auto& role:roles){if(!first)context+=',';first=false;context+=q(role)+':';if(!uploads.contains(role)){context+="null";continue;}const auto& file=uploads.at(role);context+="{\"dataset\":"+q(file.filename)+",\"bytes\":"+std::to_string(file.content.size())+",\"sha256\":"+q(footprints_workbench_sha256(file.content))+",\"encoding\":"+q(encoded(file.content).first?"utf8":"hex")+",\"format\":"+q(role=="source_file"?"raw_log":"language_json")+",\"native_kind\":"+(native_kind.contains(role)?q(native_kind[role]):"null")+",\"supplied_members\":"+strings(input_members[role])+",\"unchanged\":"+(before.contains(role)?std::string(flag(before[role]==after[role])):"null")+",\"before\":"+(before.contains(role)?before[role]:"null")+",\"after\":"+(after.contains(role)?after[role]:"null")+'}';}context+="}}";
  const auto role_original=[&](const std::string& role){const auto& file=uploads.at(role);return original(file.filename,file.content);};
  const auto language_input=[&](const std::string& role,const std::optional<StochasticLanguage>& value){return uploads.contains(role)?"{\"original\":"+role_original(role)+",\"value\":"+(value?serialize_emd_workbench_language_json(*value):"null")+'}':"null";};
  const auto source_json=uploads.contains("source_file")?"{\"original\":"+role_original("source_file")+",\"raw_log\":"+(source?serialize_embedding_workbench_raw_log_json(*source):"null")+'}':"null";
  const auto callback_adapter=derive?std::string("null"):"{\"kind\":\"finite_cost_tables_v1\",\"control_supplied\":"+std::string(flag(supplied("callback_tables_json")))+",\"supplied_slots\":"+strings(table_members)+",\"tables\":"+(tables?serialize_emd_workbench_callback_tables_json(*tables):"null")+",\"native_callback_presence\":"+(options?serialize_emd_workbench_callback_presence_json(*options):"null")+",\"invocations\":"+serialize_emd_workbench_callback_invocations_json(*invocations)+'}';
  const auto result_type=operation=="evaluate_pair"?"emd_result":operation=="distance_pair"?"distance":"stochastic_language";
  response.status=failure=="null"?200:400;
  response.set_content("{\"family\":\"ORIGINAL_EMD_WORKBENCH\",\"operation\":"+q(operation)+",\"executed\":"+flag(executed)+",\"error\":"+failure+",\"native_calls\":"+calls+",\"input_context\":"+context+",\"left\":"+language_input("left_file",left)+",\"right\":"+language_input("right_file",right)+",\"source\":"+source_json+",\"submitted_options\":"+submitted_options+",\"configured_options\":"+(options?serialize_emd_workbench_options_json(*options):"null")+",\"supplied_options\":"+strings(option_members)+",\"derive_options\":"+(normalize?"{\"normalize\":"+std::string(flag(*normalize))+'}':"null")+",\"callback_adapter\":"+callback_adapter+",\"raw_controls\":"+raw_controls+",\"result\":"+(executed?"{\"type\":"+q(result_type)+",\"value\":"+result_value+'}':"null")+",\"exports\":"+exports_json+",\"artifact_originals\":"+artifact_originals+'}',"application/json");
 }catch(const std::exception& error){response.status=400;response.set_content("{\"error\":"+minera::serialize_embedding_workbench_string_json(error.what())+'}',"application/json");}
}
