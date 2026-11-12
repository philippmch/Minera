#pragma once
// Global includes are supplied by the integration snippet. Every original
// belongs to this request; no cached model or publication state is consulted.
void handle_original_footprints_workbench(const httplib::Request& request,httplib::Response& response) {
 using namespace minera;
 namespace fc=minera::footprints_conformance;
 try {
  if(!request.is_multipart_form_data())throw std::invalid_argument("footprints workbench requires multipart data");
  const auto supplied=[&](std::string_view n){return active_source_supplied(request,n);};
  const auto field=[&](std::string_view n){return active_source_field(request,n);};
  const std::vector<std::string> roles={"model_file","observed_file","conformance_file","source_file","tree_file"};
  using Upload=std::decay_t<decltype(request.get_file_value("model_file"))>;
  std::map<std::string,Upload> uploads;
  for(const auto& role:roles){if(request.params.count(role)+request.files.count(role)>1)throw std::invalid_argument("ambiguous duplicate footprints upload: "+role);if(request.has_file(role))uploads.emplace(role,request.get_file_value(role));}
  const auto q=active_source_quote;
  const auto flag=[](bool v){return v?"true":"false";};
  const auto quote=q;
  const auto nullable=[&](const std::string& v){return v.empty()?std::string("null"):q(v);};
  const auto exception_name=[](const std::exception& e){return dynamic_cast<const std::invalid_argument*>(&e)?"invalid_argument":dynamic_cast<const std::overflow_error*>(&e)?"overflow_error":dynamic_cast<const std::out_of_range*>(&e)?"out_of_range":dynamic_cast<const std::runtime_error*>(&e)?"runtime_error":"exception";};
  const auto error_value=[&](std::string_view type,const std::exception& e){return "{\"type\":"+q(type)+",\"message\":"+q(e.what())+",\"exception_type\":"+q(exception_name(e))+'}';};
  const auto encoded=[](std::string_view bytes){bool text=true;for(std::size_t i=0;i<bytes.size()&&text;){const auto a=static_cast<unsigned char>(bytes[i++]);if(a<128)continue;const int n=a>=0xc2&&a<=0xdf?1:a>=0xe0&&a<=0xef?2:a>=0xf0&&a<=0xf4?3:-1;if(n<0||i+static_cast<std::size_t>(n)>bytes.size()){text=false;break;}unsigned code=a&(n==1?31:n==2?15:7);for(int j=0;j<n;++j){const auto b=static_cast<unsigned char>(bytes[i++]);if((b&192)!=128){text=false;break;}code=(code<<6)|(b&63);}if((n==1&&code<128)||(n==2&&code<2048)||(n==3&&code<65536)||code>0x10ffff||(code>=0xd800&&code<=0xdfff))text=false;}std::string data(bytes);if(!text){constexpr char h[]="0123456789abcdef";data.clear();for(const unsigned char c:bytes){data+=h[c>>4];data+=h[c&15];}}return std::pair{text,std::move(data)};};
  const auto original=[&](std::string_view name,std::string_view bytes){const auto [text,data]=encoded(bytes);return "{\"dataset\":"+q(name)+",\"encoding\":"+q(text?"utf8":"hex")+",\"data\":"+q(data)+",\"bytes\":"+std::to_string(bytes.size())+",\"sha256\":"+q(footprints_workbench_sha256(bytes))+'}';};
  const auto source_original_bytes=[&](std::string_view bytes){const auto [text,data]=encoded(bytes);return "\"original_input_encoding\":"+q(text?"utf8":"hex")+",\"original_input\":"+q(data)+",\"original_input_bytes\":"+std::to_string(bytes.size());};
  std::string raw_controls="[";
  const auto raw_control=[&](const std::string& key,const std::string& value){if(raw_controls.size()>1)raw_controls+=',';raw_controls+="{\"name\":"+q(key)+",\"original\":"+original(key,value)+'}';};
  for(const auto& [key,value]:request.params)raw_control(key,value);
  for(const auto& [key,value]:request.files)if(std::find(roles.begin(),roles.end(),key)==roles.end())raw_control(key,value.content);
  raw_controls+=']';
  struct Spec {bool model=false;std::string observed,conformance,group,result;bool source=false,tree=false,diagnostics=false,metrics=false;};
  static const std::map<std::string,Spec> specs={
   {"log_model",{true,"footprint","","log_model","violations"}},
   {"log_model_traces",{true,"footprint_vector","","log_model","violations_vector",false,false,true}},
   {"log_extensive",{true,"footprint","","","extensive",false,false,false,true}},
   {"trace_extensive",{true,"footprint_vector","","trace_extensive","extensive_vector",false,false,true,true}},
   {"precision",{true,"footprint","","","double"}},
   {"fitness",{true,"footprint","extensive","","double"}},
   {"fitness_traces",{true,"footprint_vector","extensive_vector","","fitness_summary"}},
   {"diagnostics_log_model",{false,"","violations_vector","diagnostics","log_model_diagnostics",true}},
   {"diagnostics_trace_extensive",{false,"","extensive_vector","diagnostics","trace_extensive_diagnostics",true}},
   {"log_extensive_compare",{true,"footprint","","log_extensive_compare","extensive",false,false,false,true}},
   {"finalize_extensive",{false,"","extensive","","finalized_extensive"}},
   {"flatten",{false,"footprint_vector","","","footprint"}},
   {"flatten_conformance",{false,"","extensive_vector","","extensive"}},
   {"project",{false,"footprint","","","projection"}},
   {"relation_configurations",{false,"footprint","","","relations"}},
   {"relation_activities",{false,"","relations","","activities"}},
   {"tree_decorations",{false,"","decoration_sets","","tree_decorations",false,true}},
   {"tree_decorations_extensive",{false,"","extensive","","tree_decorations",false,true}}};
  const auto operation=supplied("operation")?field("operation"):"log_model";
  Spec spec;if(const auto found=specs.find(operation);found!=specs.end())spec=found->second;
  const auto observed_mode=supplied("observed_mode")?field("observed_mode"):"exact";
  const auto source_format=supplied("source_format")?field("source_format"):"import";
  const auto tree_format=supplied("tree_format")?field("tree_format"):"auto";
  const auto discovery_variant=supplied("discovery_variant")?field("discovery_variant"):spec.observed=="footprint_vector"?"trace_by_trace":"entire_event_log";
  bool include_diagnostics=false,include_metrics=false,executed=false;
  std::optional<Footprint> model,observed;
  std::optional<std::vector<Footprint>> observed_vector;
  std::optional<FootprintsExtensiveResult> extensive_input,primary_extensive;
  std::optional<std::vector<FootprintsExtensiveResult>> extensive_vector_input,primary_extensives;
  std::optional<FootprintViolations> relations_input;
  std::optional<std::vector<FootprintViolations>> violations_input,primary_violations;
  std::optional<FootprintsWorkbenchDecorationSets> decoration_input;
  std::optional<ProcessTree> tree;
  std::optional<EventLog> raw_log,prepared_log;
  std::optional<ActiveEventSource> active;
  std::optional<FootprintsLogModelOptions> log_options;
  std::optional<FootprintsTraceExtensiveOptions> trace_options;
  std::optional<FootprintsWorkbenchDiagnosticsOptions> diagnostics_options;
  std::optional<FootprintsWorkbenchCompareOptions> compare_options;
  std::optional<FootprintsDiscoveryOptions> discovery_options;
  std::map<std::string,std::string> options_json={{"log_model","null"},{"trace_extensive","null"},{"diagnostics","null"},{"log_extensive_compare","null"},{"discovery","null"}};
  std::map<std::string,std::set<std::string>> option_members,input_members;
  std::map<std::string,std::string> before,native_kind,formats;
  formats={{"model_file","footprint_json"},{"observed_file",spec.observed+"_json"},{"conformance_file",spec.conformance+"_json"},{"source_file",source_format},{"tree_file",tree_format}};
  std::string source_preparation_error;
  const auto table_json=[](const EventTable* v){return v?serialize_event_table_input_json(*v):std::string("null");};
  const auto log_json=[](const EventLog* v){return v?serialize_active_source_log_json(*v):std::string("null");};
    const auto active_snapshot = [&]() {
      if (!active) return std::string("null");
      const auto& s = active->selectors();
      const auto* lazy = active->checked_lazy_if_available();
      std::string lazy_table = "null", lazy_count = "null";
      if (lazy) {
        lazy_table = minera::serialize_event_table_input_json(lazy->materialize());
        if (const auto count = lazy->row_count()) lazy_count = std::to_string(*count);
      }
      return "{\"dataset\":" + quote(active->file().name) + ",\"format\":" + quote(active->file().format == "table" ? "table" : "auto") +
        ",\"detected_format\":" + quote(active->file().format) + ',' + source_original_bytes(active->file().bytes) +
        ",\"source_selectors\":{\"case_id_key\":" + quote(s.case_id_key) + ",\"activity_key\":" + quote(s.activity_key) +
        ",\"timestamp_key\":" + quote(s.timestamp_key) + ",\"resource_key\":" + quote(s.resource_key) +
        ",\"lifecycle_key\":" + quote(s.lifecycle_key) + "},\"input_before\":" + table_json(active->original_table_if_available()) +
        ",\"input_after\":" + table_json(active->original_table_if_available()) +
        ",\"checked_table\":" + table_json(active->checked_table_if_available()) +
        ",\"original_log\":" + log_json(active->original_log_if_available()) +
        ",\"prepared_log\":" + log_json(active->checked_log_if_available()) +
        ",\"log_preparation_error\":" + nullable(active->checked_log_preparation_error()) +
        ",\"flat_projection_error\":" + nullable(active->checked_flat_projection_error()) +
        ",\"selected_shape\":" + quote(active_source_shape(active->selected_kind())) +
        ",\"selected_kind_index\":" + std::to_string(static_cast<int>(active->selected_kind())) +
        ",\"prepared_stream\":" + (active->checked_stream_if_available() ? minera::serialize_active_source_stream_json(*active->checked_stream_if_available()) : "null") +
        ",\"prepared_lazy_table\":" + lazy_table + ",\"lazy_row_count\":" + lazy_count +
        ",\"lazy_valid\":" + (lazy ? std::string(flag(lazy->valid())) : std::string("null")) + '}';
    };

  const auto conformance_json=[&](){if(extensive_input)return serialize_footprints_workbench_json(*extensive_input);if(extensive_vector_input)return serialize_footprints_workbench_json(*extensive_vector_input);if(violations_input)return serialize_footprints_workbench_json(*violations_input);if(relations_input)return serialize_footprints_workbench_json(*relations_input);if(decoration_input)return serialize_footprints_workbench_json(*decoration_input);return std::string("null");};
  const auto observed_json=[&](){return observed?serialize_footprints_workbench_json(*observed):observed_vector?serialize_footprints_workbench_json(*observed_vector):std::string("null");};
  const auto source_state=[&](){return raw_log?serialize_footprints_workbench_raw_log_json(*raw_log):active_snapshot();};
  std::string calls="[",failure="null",diagnostics_error="null",metrics_error="null",diagnostics_json="null",metrics_json="null",result_value="null";
  const auto invoke=[&](std::string_view name,std::string_view stage,std::string_view kind,const auto& call){if(calls.size()>1)calls+=',';const auto prefix="{\"name\":"+q(name)+",\"stage\":"+q(stage)+",\"input_kind\":"+q(kind)+",\"returned\":";try{call();calls+=prefix+"true,\"error\":null}";}catch(const std::exception& e){calls+=prefix+"false,\"error\":"+error_value("native_error",e)+'}';throw;}};
  std::string phase="configuration_error";
  const auto remember=[&](const std::string& role,const std::string& state,std::string_view kind,bool json_input=true){before[role]=state;native_kind[role]=kind;if(json_input)input_members[role]=footprints_workbench_supplied_members(uploads.at(role).content);};
  const auto prepare_source=[&](std::string_view stage){if(raw_log||active)return;const auto& file=uploads.at("source_file");try{if(source_format=="raw_log"){raw_log=parse_footprints_workbench_raw_log_json(file.content);remember("source_file",source_state(),"EventLog");}else{auto params=parse_mine_params(field,supplied);invoke("prepare_active_event_source(request, bytes, dataset, imports)",stage,"original_source_file",[&]{active=prepare_active_event_source(request,file.content,file.filename,params);});remember("source_file",source_state(),"ActiveEventSource",false);}}catch(const std::exception& e){source_preparation_error=e.what();throw;}};
  const auto diagnostic_log=[&](std::string_view stage)->const EventLog&{prepare_source(stage);try{if(!prepared_log){if(raw_log)prepared_log=*raw_log;else invoke("ActiveEventSource::checked_log()",stage,"ActiveEventSource",[&]{prepared_log=active->checked_log();});}return *prepared_log;}catch(const std::exception& e){source_preparation_error=e.what();throw;}};
  // A CSV source's case:concept:name is its case column (round BO, csv_frame_key).
  const auto diagnostics_case_key=[&]{return active?csv_frame_key(*active,FirstClassAttributeRole::kCaseId,diagnostics_options->case_id_key):diagnostics_options->case_id_key;};
  try {
   static const std::set<std::string> source_controls={"mining_input_format","mining_input_shape","mining_selectors_json","case_column","activity_column","timestamp_column","resource_column","lifecycle_column","delimiter","xes_variant","xes_case_attribute","xes_activity_attribute","xes_timestamp_attribute","xes_resource_attribute","xes_lifecycle_attribute","xes_require_timestamp","xes_timestamp_sort","xes_reverse_sort","xes_max_traces","xes_attributes_to_read","xes_max_decompressed_bytes"};
   static const std::set<std::string> controls={"operation","observed_mode","source_format","tree_format","discovery_variant","include_diagnostics","include_metrics","log_model_options_json","trace_extensive_options_json","diagnostics_options_json","log_extensive_compare_options_json","discovery_options_json"};
   const auto check=[&](const std::string& key){if(request.params.count(key)+request.files.count(key)!=1)throw std::invalid_argument("duplicate footprints control: "+key);if(std::find(roles.begin(),roles.end(),key)!=roles.end()){if(!uploads.contains(key))throw std::invalid_argument("footprints upload must be a file: "+key);}else if(!controls.contains(key)&&!source_controls.contains(key))throw std::invalid_argument("unknown footprints control: "+key);};
   for(const auto& [key,value]:request.params){(void)value;check(key);}for(const auto& [key,value]:request.files){(void)value;check(key);}
   if(!specs.contains(operation))throw std::invalid_argument("unsupported footprints operation: "+operation);
   if(supplied("include_diagnostics")){if(!spec.diagnostics)throw std::invalid_argument("include_diagnostics is inapplicable to this operation");include_diagnostics=petri_anti_multi_boolean(field("include_diagnostics"),"include_diagnostics");}
   if(supplied("include_metrics")){if(!spec.metrics)throw std::invalid_argument("include_metrics is inapplicable to this operation");include_metrics=petri_anti_multi_boolean(field("include_metrics"),"include_metrics");}
   if(supplied("observed_mode")&&spec.observed.empty())throw std::invalid_argument("observed_mode requires an observed footprint operation");
   if(observed_mode!="exact"&&observed_mode!="source")throw std::invalid_argument("observed_mode must be exact or source");
   const bool discover=!spec.observed.empty()&&observed_mode=="source";
   const bool needs_source=spec.source||include_diagnostics||discover;
   std::set<std::string> required_roles;if(spec.model)required_roles.insert("model_file");if(!spec.observed.empty()&&!discover)required_roles.insert("observed_file");if(!spec.conformance.empty())required_roles.insert("conformance_file");if(spec.tree)required_roles.insert("tree_file");if(needs_source)required_roles.insert("source_file");
   for(const auto& role:roles)if(required_roles.contains(role)!=uploads.contains(role))throw std::invalid_argument((required_roles.contains(role)?"missing footprints upload: ":"inapplicable footprints upload: ")+role);
   if(supplied("source_format")&&!needs_source)throw std::invalid_argument("source_format requires source_file");
   if(needs_source&&source_format!="import"&&source_format!="raw_log")throw std::invalid_argument("source_format must be import or raw_log");
   if(supplied("tree_format")&&!spec.tree)throw std::invalid_argument("tree_format requires tree_file");
   if(spec.tree&&tree_format!="auto"&&tree_format!="ptml"&&tree_format!="tree_json")throw std::invalid_argument("tree_format must be auto, ptml or tree_json");
   for(const auto& name:source_controls)if(supplied(name)&&(!needs_source||source_format!="import"))throw std::invalid_argument("source import control is inapplicable: "+name);
   if(supplied("discovery_variant")&&!discover)throw std::invalid_argument("discovery_variant requires observed_mode=source");
   if(discover){if(discovery_variant!="entire_event_log"&&discovery_variant!="trace_by_trace"&&discovery_variant!="entire_dataframe"&&discovery_variant!="polars_lazyframes")throw std::invalid_argument("unsupported real footprints discovery adapter");if((discovery_variant=="trace_by_trace")!=(spec.observed=="footprint_vector"))throw std::invalid_argument("discovery adapter scalar/vector output does not match operation");}
   for(const auto& [group,unused]:options_json){(void)unused;const bool applicable=group==spec.group||(group=="discovery"&&discover)||(group=="diagnostics"&&include_diagnostics);if(supplied(group+"_options_json")&&!applicable)throw std::invalid_argument("inapplicable footprints option group: "+group);if(!applicable)continue;const auto text=supplied(group+"_options_json")?field(group+"_options_json"):"{}";auto* members=&option_members[group];
    if(group=="log_model"){log_options=parse_footprints_workbench_log_model_options(text,members);options_json[group]=serialize_footprints_workbench_options_json(*log_options);}
    else if(group=="trace_extensive"){trace_options=parse_footprints_workbench_trace_extensive_options(text,members);options_json[group]=serialize_footprints_workbench_options_json(*trace_options);}
    else if(group=="diagnostics"){diagnostics_options=parse_footprints_workbench_diagnostics_options(text,members);options_json[group]=serialize_footprints_workbench_options_json(*diagnostics_options);}
    else if(group=="log_extensive_compare"){compare_options=parse_footprints_workbench_compare_options(text,members);options_json[group]=serialize_footprints_workbench_options_json(*compare_options);}
    else {discovery_options=parse_footprints_workbench_discovery_options(text,members);options_json[group]=serialize_footprints_workbench_options_json(*discovery_options);}
   }
   if(needs_source&&source_format=="import"){auto imports=parse_mine_params(field,supplied);(void)imports;if(supplied("mining_input_shape")){const auto shape=field("mining_input_shape");if(shape!="log"&&shape!="stream"&&shape!="dataframe"&&shape!="lazyframe")throw std::invalid_argument("mining_input_shape must be log, stream, dataframe or lazyframe");}}
   phase="preparation_error";
   if(spec.model){model=parse_footprints_workbench_footprint_json(uploads.at("model_file").content);remember("model_file",serialize_footprints_workbench_json(*model),"Footprint");}
   if(!spec.observed.empty()&&!discover){const auto& bytes=uploads.at("observed_file").content;if(spec.observed=="footprint")observed=parse_footprints_workbench_footprint_json(bytes);else observed_vector=parse_footprints_workbench_footprints_json(bytes);remember("observed_file",observed_json(),spec.observed=="footprint"?"Footprint":"vector<Footprint>");}
   if(!spec.conformance.empty()){const auto& bytes=uploads.at("conformance_file").content;if(spec.conformance=="extensive")extensive_input=parse_footprints_workbench_extensive_json(bytes);else if(spec.conformance=="extensive_vector")extensive_vector_input=parse_footprints_workbench_extensives_json(bytes);else if(spec.conformance=="violations_vector")violations_input=parse_footprints_workbench_violations_json(bytes);else if(spec.conformance=="relations")relations_input=parse_footprints_workbench_relations_json(bytes);else decoration_input=parse_footprints_workbench_decoration_json(bytes);const auto kind=spec.conformance=="extensive"?"FootprintsExtensiveResult":spec.conformance=="extensive_vector"?"vector<FootprintsExtensiveResult>":spec.conformance=="violations_vector"?"vector<FootprintViolations>":spec.conformance=="relations"?"set<FootprintPair>":"FootprintsWorkbenchDecorationSets";remember("conformance_file",conformance_json(),kind);}
   if(spec.tree){const auto& bytes=uploads.at("tree_file").content;const auto pos=bytes.find_first_not_of(" \r\n\t");const bool ptml=tree_format=="ptml"||(tree_format=="auto"&&pos!=std::string::npos&&bytes[pos]=='<');tree=ptml?parse_ptml(bytes):parse_process_tree_alignment_input_json(bytes);remember("tree_file",serialize_process_tree_alignment_input_json(*tree),"ProcessTree",!ptml);}
   // pm4py's frame of a CSV upload holds the standard keys as copies of its
   // selected columns (round BO, csv_frame_key); the options echo keeps the
   // submitted keys.
   const auto frame_keys=[&]{
    if(!active)return;
    using Role=FirstClassAttributeRole;
    if(discovery_options){discovery_options->activity_attribute=csv_frame_key(*active,Role::kActivity,discovery_options->activity_attribute);discovery_options->case_id_attribute=csv_frame_key(*active,Role::kCaseId,discovery_options->case_id_attribute);discovery_options->timestamp_attribute=csv_frame_key(*active,Role::kTimestamp,discovery_options->timestamp_attribute);}
   };
   // A CSV upload's table shapes as format_dataframe's frame (round BO).
   std::optional<EventTable> frame_table;std::optional<LazyEventTable> frame_lazy;
   const auto csv_frame=[&]()->const EventTable*{if(!active)return nullptr;if(!frame_table){try{frame_table=csv_frame_table(*active,false);}catch(const std::exception&){return nullptr;}}return frame_table?&*frame_table:nullptr;};
   if(discover){
    prepare_source("preparation");
    frame_keys();
    if(discovery_variant=="entire_event_log"){const auto& log=diagnostic_log("preparation");invoke("discover_footprints(log, discovery_options)","preparation","EventLog",[&]{observed=discover_footprints(log,*discovery_options);});}
    else if(discovery_variant=="trace_by_trace"){const auto& log=diagnostic_log("preparation");invoke("discover_trace_footprints(log, discovery_options)","preparation","EventLog",[&]{observed_vector=discover_trace_footprints(log,*discovery_options);});}
    else if(discovery_variant=="entire_dataframe"){std::optional<EventTable> converted;const EventTable* table=nullptr;if(raw_log)invoke("prepare_footprints_event_table(log, discovery_options)","preparation","EventLog",[&]{converted=prepare_footprints_event_table(*raw_log,*discovery_options);table=&*converted;});else invoke("ActiveEventSource::checked_table()","preparation","ActiveEventSource",[&]{const auto* frame=csv_frame();table=frame?frame:&active->checked_table();});invoke("discover_footprints(table, discovery_options)","preparation","EventTable",[&]{observed=discover_footprints(*table,*discovery_options);});}
    else{std::optional<LazyEventTable> converted;const LazyEventTable* lazy=nullptr;if(raw_log)invoke("prepare_footprints_lazy_table(log, discovery_options)","preparation","EventLog",[&]{converted=prepare_footprints_lazy_table(*raw_log,*discovery_options);lazy=&*converted;});else invoke("ActiveEventSource::checked_lazy()","preparation","ActiveEventSource",[&]{if(const auto* frame=csv_frame()){frame_lazy=LazyEventTable::from_table(*frame);lazy=&*frame_lazy;}else lazy=&active->checked_lazy();});invoke("discover_footprints(lazy, discovery_options)","preparation","LazyEventTable",[&]{observed=discover_footprints(*lazy,*discovery_options);});}
   }
   if(spec.source)(void)diagnostic_log("preparation");
   phase="native_error";
   const auto primary=[&](std::string_view name,std::string_view kind,const auto& call){invoke(name,"primary",kind,call);};
   if(operation=="log_model")primary("check_footprints_log_model(observed, model, options)","Footprint",[&]{result_value=serialize_footprints_workbench_json(check_footprints_log_model(*observed,*model,*log_options));});
   else if(operation=="log_model_traces")primary("check_footprints_log_model(observed_vector, model, options)","vector<Footprint>",[&]{primary_violations=check_footprints_log_model(*observed_vector,*model,*log_options);result_value=serialize_footprints_workbench_json(*primary_violations);});
   else if(operation=="log_extensive"||operation=="log_extensive_compare")primary(operation=="log_extensive"?"check_footprints_log_extensive(observed, model)":"LogExtensiveVariant::compare(observed, model, check_act_always_executed)","Footprint",[&]{primary_extensive=operation=="log_extensive"?check_footprints_log_extensive(*observed,*model):fc::LogExtensiveVariant::compare(*observed,*model,compare_options->check_act_always_executed);result_value=serialize_footprints_workbench_json(*primary_extensive);});
   else if(operation=="trace_extensive")primary("check_footprints_trace_extensive(observed_vector, model, options)","vector<Footprint>",[&]{primary_extensives=check_footprints_trace_extensive(*observed_vector,*model,*trace_options);result_value=serialize_footprints_workbench_json(*primary_extensives);});
   else if(operation=="precision")primary("fp_precision(observed, model)","Footprint",[&]{result_value=serialize_footprints_workbench_json(fp_precision(*observed,*model));});
   else if(operation=="fitness")primary("fp_fitness(observed, model, supplied_extensive)","Footprint",[&]{result_value=serialize_footprints_workbench_json(fp_fitness(*observed,*model,*extensive_input));});
   else if(operation=="fitness_traces")primary("fp_fitness(observed_vector, model, supplied_extensive_vector)","vector<Footprint>",[&]{result_value=serialize_footprints_workbench_json(fp_fitness(*observed_vector,*model,*extensive_vector_input));});
   else if(operation=="diagnostics_log_model")primary("LogModelVariant::diagnostics(log, supplied_violations, case_id_key)","EventLog",[&]{result_value=serialize_footprints_workbench_json(fc::LogModelVariant::diagnostics(*prepared_log,*violations_input,diagnostics_case_key()));});
   else if(operation=="diagnostics_trace_extensive")primary("TraceExtensiveVariant::diagnostics(log, supplied_extensive_vector, case_id_key)","EventLog",[&]{result_value=serialize_footprints_workbench_json(fc::TraceExtensiveVariant::diagnostics(*prepared_log,*extensive_vector_input,diagnostics_case_key()));});
   else if(operation=="finalize_extensive")primary("LogExtensiveVariant::finalize(owned_copy)","FootprintsExtensiveResult",[&]{auto copy=*extensive_input;fc::LogExtensiveVariant::finalize(copy);result_value="{\"before\":"+serialize_footprints_workbench_json(*extensive_input)+",\"after\":"+serialize_footprints_workbench_json(copy)+'}';});
   else if(operation=="flatten")primary("EvaluationUtility::flatten(observed_vector)","vector<Footprint>",[&]{result_value=serialize_footprints_workbench_json(fc::EvaluationUtility::flatten(*observed_vector));});
   else if(operation=="flatten_conformance")primary("EvaluationUtility::flatten_conformance(supplied_extensive_vector)","vector<FootprintsExtensiveResult>",[&]{result_value=serialize_footprints_workbench_json(fc::EvaluationUtility::flatten_conformance(*extensive_vector_input));});
   else if(operation=="project")primary("FootprintRelationProjection::project(observed)","Footprint",[&]{result_value=serialize_footprints_workbench_json(fc::FootprintRelationProjection::project(*observed));});
   else if(operation=="relation_configurations")primary("FootprintRelationProjection::relation_configurations(observed)","Footprint",[&]{result_value=serialize_footprints_workbench_json(fc::FootprintRelationProjection::relation_configurations(*observed));});
   else if(operation=="relation_activities")primary("FootprintRelationProjection::relation_activities(relations)","set<FootprintPair>",[&]{result_value=serialize_footprints_workbench_json(fc::FootprintRelationProjection::relation_activities(*relations_input));});
   else if(operation=="tree_decorations")primary("TreeVisualizationProjection::decorate(tree, relations, start, end, always)","ProcessTree",[&]{result_value=serialize_footprints_workbench_json(fc::TreeVisualizationProjection::decorate(*tree,decoration_input->relations,decoration_input->start_activities,decoration_input->end_activities,decoration_input->activities_always_happening));});
   else primary("TreeVisualizationProjection::decorate(tree, supplied_extensive)","ProcessTree",[&]{result_value=serialize_footprints_workbench_json(fc::TreeVisualizationProjection::decorate(*tree,*extensive_input));});
   executed=true;
   if(include_diagnostics){try{const auto& log=diagnostic_log("diagnostics");if(primary_violations)invoke("LogModelVariant::diagnostics(log, primary_violations, case_id_key)","diagnostics","EventLog",[&]{diagnostics_json="{\"type\":\"log_model_diagnostics\",\"value\":"+serialize_footprints_workbench_json(fc::LogModelVariant::diagnostics(log,*primary_violations,diagnostics_case_key()))+'}';});else invoke("TraceExtensiveVariant::diagnostics(log, primary_extensive_vector, case_id_key)","diagnostics","EventLog",[&]{diagnostics_json="{\"type\":\"trace_extensive_diagnostics\",\"value\":"+serialize_footprints_workbench_json(fc::TraceExtensiveVariant::diagnostics(log,*primary_extensives,diagnostics_case_key()))+'}';});}catch(const std::exception& e){diagnostics_error=error_value("native_error",e);}}
   if(include_metrics){
    std::string precision="null",fitness="null",precision_error="null",fitness_error="null",flattened="null";std::optional<Footprint> flat;
    try{const Footprint* scalar_observed=observed?&*observed:nullptr;if(!scalar_observed){invoke("EvaluationUtility::flatten(observed_vector)","metrics","vector<Footprint>",[&]{flat=fc::EvaluationUtility::flatten(*observed_vector);});flattened=serialize_footprints_workbench_json(*flat);scalar_observed=&*flat;}invoke("fp_precision(observed, model)","metrics","Footprint",[&]{precision=serialize_footprints_workbench_json(fp_precision(*scalar_observed,*model));});}catch(const std::exception& e){precision_error=error_value("native_error",e);metrics_error=precision_error;}
    try{if(primary_extensives)invoke("fp_fitness(observed_vector, model, primary_extensive_vector)","metrics","vector<Footprint>",[&]{fitness=serialize_footprints_workbench_json(fp_fitness(*observed_vector,*model,*primary_extensives));});else invoke("fp_fitness(observed, model, primary_extensive)","metrics","Footprint",[&]{fitness=serialize_footprints_workbench_json(fp_fitness(*observed,*model,*primary_extensive));});}catch(const std::exception& e){fitness_error=error_value("native_error",e);if(metrics_error=="null")metrics_error=fitness_error;}
    metrics_json="{\"precision\":{\"value\":"+precision+",\"error\":"+precision_error+"},\"fitness\":{\"value\":"+fitness+",\"error\":"+fitness_error+"},\"flattened_observed\":"+flattened+'}';
   }
  }catch(const std::exception& e){failure=error_value(phase,e);}
  const auto result_json=executed?"{\"type\":"+q(spec.result)+",\"value\":"+result_value+'}':std::string("null");
  std::map<std::string,std::string> exports;
  const auto export_value=[&](const std::string& key,const auto& produce){try{exports[key]=produce();}catch(const std::exception& e){exports[key+"_error"]=e.what();}};
  if(model)export_value("model_json",[&]{return serialize_footprints_workbench_json(*model);});
  if(observed||observed_vector)export_value("observed_json",observed_json);
  if(conformance_json()!="null")export_value("conformance_json",conformance_json);
  if(executed)export_value("result_json",[&]{return result_value;});
  if(raw_log||prepared_log||(active&&active->checked_log_if_available()))export_value("source_xes",[&]{std::string result;const auto& log=raw_log?*raw_log:prepared_log?*prepared_log:active->checked_log();invoke("to_xes(source_log)","export","EventLog",[&]{result=to_xes(log,"Original footprints source");});return result;});
  if(tree){export_value("tree_json",[&]{return serialize_process_tree_alignment_input_json(*tree);});export_value("tree_ptml",[&]{if(!process_tree_alignment_export_indices_safe(*tree))throw std::invalid_argument("Model export is unavailable because an activity leaf index is outside the native activity registry.");std::string result;invoke("to_ptml(tree)","export","ProcessTree",[&]{result=to_ptml(*tree,"Original footprints tree");});return result;});}
  calls+=']';
  std::string exports_json="{";for(const auto& key:{"model_json","observed_json","conformance_json","source_xes","tree_ptml","tree_json","result_json"}){if(exports_json.size()>1)exports_json+=',';const auto value=[&](const std::string& n){const auto it=exports.find(n);return it==exports.end()?std::string("null"):q(it->second);};exports_json+=q(key)+':'+value(key)+','+q(std::string(key)+"_error")+':'+value(std::string(key)+"_error");}exports_json+='}';
  const auto map_members=[&](const std::set<std::string>& values){std::string r="[";for(const auto& v:values){if(r.size()>1)r+=',';r+=q(v);}return r+']';};
  std::string options="{",supplied_options="{";for(const auto& [group,value]:options_json){if(options.size()>1){options+=',';supplied_options+=',';}options+=q(group)+':'+value;supplied_options+=q(group)+':'+map_members(option_members[group]);}options+='}';supplied_options+='}';
  std::map<std::string,std::string> after;if(model)after["model_file"]=serialize_footprints_workbench_json(*model);if(observed||observed_vector)after["observed_file"]=observed_json();if(conformance_json()!="null")after["conformance_file"]=conformance_json();if(tree)after["tree_file"]=serialize_process_tree_alignment_input_json(*tree);if(raw_log||active)after["source_file"]=source_state();
  std::string context="{\"size_t_bytes\":"+std::to_string(sizeof(std::size_t))+",\"uploads\":{";
  for(const auto& role:roles){if(context.back()!='{')context+=',';context+=q(role)+':';const auto it=uploads.find(role);if(it==uploads.end()){context+="null";continue;}const auto& file=it->second;const auto [text,data]=encoded(file.content);(void)data;const auto kind=native_kind.find(role);context+="{\"dataset\":"+q(file.filename)+",\"bytes\":"+std::to_string(file.content.size())+",\"sha256\":"+q(footprints_workbench_sha256(file.content))+",\"encoding\":"+q(text?"utf8":"hex")+",\"format\":"+q(formats[role])+",\"native_kind\":"+(kind==native_kind.end()?std::string("null"):q(kind->second))+",\"supplied_members\":"+map_members(input_members[role])+",\"unchanged\":"+flag(!before.contains(role)||before[role]==after[role])+'}';}context+="}}";
  const auto role_original=[&](const std::string& role){const auto& file=uploads.at(role);return original(file.filename,file.content);};
  const std::string model_json=uploads.contains("model_file")?"{\"original\":"+role_original("model_file")+",\"footprint\":"+(model?serialize_footprints_workbench_json(*model):"null")+'}':"null";
  const std::string observed_role=(uploads.contains("observed_file")||(observed_mode=="source"&&!spec.observed.empty()))?"{\"original\":"+(uploads.contains("observed_file")?role_original("observed_file"):std::string("null"))+",\"mode\":"+q(observed_mode)+",\"type\":"+q(spec.observed.empty()?"footprint":spec.observed)+",\"value\":"+observed_json()+'}':"null";
  const std::string conformance_role=uploads.contains("conformance_file")?"{\"original\":"+role_original("conformance_file")+",\"type\":"+q(spec.conformance.empty()?"unknown":spec.conformance)+",\"value\":"+conformance_json()+'}':"null";
  const std::string source_role=uploads.contains("source_file")?"{\"original\":"+role_original("source_file")+",\"format\":"+q(source_format)+",\"raw_log\":"+(raw_log?serialize_footprints_workbench_raw_log_json(*raw_log):"null")+",\"active_source\":"+active_snapshot()+",\"prepared_log\":"+(prepared_log?serialize_footprints_workbench_raw_log_json(*prepared_log):"null")+",\"preparation_error\":"+nullable(source_preparation_error)+'}':"null";
  const std::string tree_role=uploads.contains("tree_file")?"{\"original\":"+role_original("tree_file")+",\"format\":"+q(tree_format)+",\"constructor_state\":"+(tree?serialize_process_tree_alignment_input_json(*tree):"null")+'}':"null";
  response.status=failure=="null"?200:400;
  response.set_content(active_source_safe_json("{\"family\":\"ORIGINAL_FOOTPRINTS_WORKBENCH\",\"operation\":"+q(operation)+",\"executed\":"+flag(executed)+",\"native_calls\":"+calls+",\"error\":"+failure+",\"diagnostics_error\":"+diagnostics_error+",\"metrics_error\":"+metrics_error+",\"requested\":{\"include_diagnostics\":"+flag(include_diagnostics)+",\"include_metrics\":"+flag(include_metrics)+"},\"input_context\":"+context+",\"model\":"+model_json+",\"observed\":"+observed_role+",\"conformance_input\":"+conformance_role+",\"source\":"+source_role+",\"tree\":"+tree_role+",\"submitted_options\":"+options+",\"configured_options\":"+options+",\"supplied_options\":"+supplied_options+",\"raw_controls\":"+raw_controls+",\"result\":"+result_json+",\"diagnostics\":"+diagnostics_json+",\"metrics\":"+metrics_json+",\"exports\":"+exports_json+'}'),"application/json");
 }catch(const std::exception& e){response.status=400;response.set_content(active_source_safe_json(error_json(e.what())),"application/json");}
}
