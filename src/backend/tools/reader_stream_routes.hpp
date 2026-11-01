// Included after live_stream_routes.hpp and the shared import/request helpers.

struct ReaderSessionRecord {
  std::mutex mutex;
  std::string id;
  httplib::Params configuration;
  LiveUpload source;
  std::unique_ptr<minera::ReaderStreamSession> reader;
  bool released=false;
};
std::mutex g_reader_mutex;
std::map<std::string,std::shared_ptr<ReaderSessionRecord>> g_reader_sessions;
std::shared_ptr<ReaderSessionRecord> reader_find(const httplib::Request& request) {
  const auto id=live_required(request,"session_id");std::lock_guard lock(g_reader_mutex);
  const auto it=g_reader_sessions.find(id);if(it==g_reader_sessions.end())throw std::out_of_range("reader session is unavailable; it may have been released or the server restarted");return it->second;
}
void reader_open(const ReaderSessionRecord& record) {if(record.released||!record.reader)throw std::out_of_range("reader session was released");}
std::string reader_payload(ReaderSessionRecord& record,bool wait=true) {
  reader_open(record);auto json=record.reader->snapshot_json(wait);json.pop_back();
  const auto quote=minera::embedding_runtime::detail::escape_json;std::ostringstream out;
  out<<json<<",\"session_id\":"<<quote(record.id)<<",\"configuration\":{";
  bool first=true;for(const auto& [key,value]:record.configuration){if(!first)out<<',';first=false;out<<quote(key)<<':'<<quote(value);}
  const auto& s=record.source;out<<"},\"source\":{\"name\":"<<quote(s.name)<<",\"format\":"<<quote(s.format)<<",\"size\":"<<s.bytes.size()<<",\"provided_sha256\":"<<quote(s.provided_sha256)<<"}}";return out.str();
}
void handle_reader_create(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{
    live_params(request,{"session_id","kind","source_name","source_sha256","case_id_column","activity_column","timestamp_column","csv_delimiter","csv_trim_fields","csv_case_aliases","csv_resource_column","csv_lifecycle_column","xes_variant","xes_case_attribute","xes_activity_attribute","xes_timestamp_attribute","xes_resource_attribute","xes_lifecycle_attribute","xes_timestamp_sort","xes_reverse_sort","xes_max_traces","activity_prefix","accept_case","accept_activity","min_trace_events","max_trace_events"});
    const auto id=live_required(request,"session_id"),kind=live_required(request,"kind");live_id(id);
    minera::ReaderStreamOptions options;
    if(kind=="csv_events")options.kind=minera::ReaderStreamKind::kCsvEvents;
    else if(kind=="xes_events")options.kind=minera::ReaderStreamKind::kXesEvents;
    else if(kind=="xes_traces")options.kind=minera::ReaderStreamKind::kXesTraces;
    else throw std::invalid_argument("reader kind requires csv_events, xes_events or xes_traces");
    options.source=ocel_request_body(request,"log");if(options.source.empty())throw std::invalid_argument("reader source upload is empty");
    for(const auto& [parameter,target]:std::initializer_list<std::pair<const char*,std::string*>>{{"case_id_column",&options.csv.case_id_column},{"activity_column",&options.csv.activity_column},{"timestamp_column",&options.csv.timestamp_column},{"csv_resource_column",&options.csv.resource_column},{"csv_lifecycle_column",&options.csv.lifecycle_column}})if(request.has_param(parameter))*target=request.get_param_value(parameter);
    if(request.has_param("csv_delimiter")){const auto value=request.get_param_value("csv_delimiter");if(value.size()!=1||value[0]=='\0'||value[0]=='\n'||value[0]=='\r')throw std::invalid_argument("CSV delimiter must be one character");options.csv.delimiter=value[0];}
    options.csv.trim_fields=ocel_query_bool(request,"csv_trim_fields",true);options.csv.allow_case_id_aliases=ocel_query_bool(request,"csv_case_aliases",false);
    if(request.has_param("xes_variant"))options.xes.variant=minera::parse_xes_import_variant(request.get_param_value("xes_variant"));
    for(const auto& [parameter,target]:std::initializer_list<std::pair<const char*,std::string*>>{{"xes_case_attribute",&options.xes.case_attribute},{"xes_activity_attribute",&options.xes.activity_attribute},{"xes_timestamp_attribute",&options.xes.timestamp_attribute},{"xes_resource_attribute",&options.xes.resource_attribute},{"xes_lifecycle_attribute",&options.xes.lifecycle_attribute}})if(request.has_param(parameter))*target=request.get_param_value(parameter);
    options.xes.timestamp_sort=ocel_query_bool(request,"xes_timestamp_sort",false);options.xes.reverse_sort=ocel_query_bool(request,"xes_reverse_sort",false);options.xes.max_traces=ocel_query_size(request,"xes_max_traces",options.xes.max_traces);
    if(request.has_param("activity_prefix")) {
      const auto prefix=request.get_param_value("activity_prefix");
      options.events.transform=[prefix](minera::Event event){const auto source=event;minera::set_event_activity_context(source,prefix+source.activity,event);return event;};
      options.traces.transform=[prefix](minera::Trace trace){for(auto& event:trace.events){const auto source=event;minera::set_event_activity_context(source,prefix+source.activity,event);}return trace;};
    }
    const auto case_id=request.has_param("accept_case")?std::optional<std::string>(request.get_param_value("accept_case")):std::nullopt;
    const auto activity=request.has_param("accept_activity")?std::optional<std::string>(request.get_param_value("accept_activity")):std::nullopt;
    const auto minimum=ocel_query_size(request,"min_trace_events",0),maximum=ocel_query_size(request,"max_trace_events",std::numeric_limits<std::size_t>::max());
    if(minimum>maximum)throw std::invalid_argument("minimum trace events exceeds maximum");
    options.events.acceptance=[case_id,activity](const minera::Event& event){return (!case_id||event.case_id==*case_id)&&(!activity||event.activity==*activity);};
    options.traces.acceptance=[case_id,activity,minimum,maximum](const minera::Trace& trace){return (!case_id||trace.case_id==*case_id)&&trace.events.size()>=minimum&&trace.events.size()<=maximum&&(!activity||std::any_of(trace.events.begin(),trace.events.end(),[&](const auto& event){return event.activity==*activity;}));};
    auto record=std::make_shared<ReaderSessionRecord>();record->id=id;record->configuration=request.params;
    record->source={"source",request.has_param("source_name")?request.get_param_value("source_name"):"reader-upload",kind=="csv_events"?"csv":"xes",request.has_param("source_sha256")?request.get_param_value("source_sha256"):"",options.source};
    record->reader=std::make_unique<minera::ReaderStreamSession>(std::move(options));
    {std::lock_guard lock(g_reader_mutex);const auto found=g_reader_sessions.find(id);
      if(found!=g_reader_sessions.end())record=found->second;
      else if(g_reader_sessions.size()>=kLiveMaxSessions)throw LiveCapacityError(429,"too many reader sessions ("+std::to_string(kLiveMaxSessions)+"); release one before creating another");
      else g_reader_sessions.emplace(id,record);}
    std::lock_guard lock(record->mutex);reader_open(*record);
    if(record->configuration!=request.params||record->source.bytes!=ocel_request_body(request,"log"))throw minera::StreamingSessionConflict("reader session ID already has a different source or configuration");
    response.set_content(reader_payload(*record),"application/json");
  });
}
void handle_reader_snapshot(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"session_id","wait_until_drained"});auto s=reader_find(request);std::lock_guard lock(s->mutex);response.set_content(reader_payload(*s,ocel_query_bool(request,"wait_until_drained",true)),"application/json");});
}
void handle_reader_read(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"session_id","expected_revision","batch_id","maximum_items","wait_until_drained"});auto s=reader_find(request);std::lock_guard lock(s->mutex);reader_open(*s);
    const bool wait=ocel_query_bool(request,"wait_until_drained",true);const auto batch=live_required(request,"batch_id");live_id(batch);s->reader->read(ocel_query_size(request,"maximum_items",25),batch,live_revision(request));response.set_content(reader_payload(*s,wait),"application/json");});
}
void handle_reader_action(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"session_id","expected_revision","action"});auto s=reader_find(request);std::lock_guard lock(s->mutex);reader_open(*s);
    const std::map<std::string,minera::ReaderStreamAction> actions={{"start",minera::ReaderStreamAction::kStart},{"drain",minera::ReaderStreamAction::kDrain},{"finish",minera::ReaderStreamAction::kFinish},{"register",minera::ReaderStreamAction::kRegister},{"deregister",minera::ReaderStreamAction::kDeregister},{"reset_reader",minera::ReaderStreamAction::kResetReader}};
    const auto action=live_required(request,"action");const auto it=actions.find(action);if(it==actions.end())throw std::invalid_argument("unknown reader action");s->reader->action(it->second,live_revision(request));response.set_content(reader_payload(*s),"application/json");});
}
void handle_reader_source(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"session_id"});auto s=reader_find(request);std::lock_guard lock(s->mutex);reader_open(*s);response.set_content(s->source.bytes,"application/octet-stream");});
}
void handle_reader_export(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"session_id","scope","format","variant","gzip","preserve_nulls","infer_metadata"});auto s=reader_find(request);std::lock_guard lock(s->mutex);reader_open(*s);
    const auto scope=live_required(request,"scope"),format=live_required(request,"format");if(scope!="read"&&scope!="delivered")throw std::invalid_argument("reader export scope requires read or delivered");
    auto log=s->reader->selected_log(scope=="delivered");
    if(format=="json"){minera::LogConversionResult value;value.log=std::move(log);response.set_content(minera::log_conversion_json(value),"application/json");}
    else if(format=="xes"){minera::XesExportOptions options;if(request.has_param("variant"))options.variant=minera::parse_xes_export_variant(request.get_param_value("variant"));options.compress=ocel_query_bool(request,"gzip",false);options.preserve_nulls=ocel_query_bool(request,"preserve_nulls",true);options.infer_metadata=ocel_query_bool(request,"infer_metadata",false);response.set_content(minera::serialize_xes(log,options),*options.compress?"application/gzip":"application/xml");}
    else throw std::invalid_argument("reader export format requires json or xes");
  });
}
void handle_reader_list(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{});std::vector<std::shared_ptr<ReaderSessionRecord>> entries;{std::lock_guard lock(g_reader_mutex);for(const auto& [id,s]:g_reader_sessions){(void)id;entries.push_back(s);}}
    std::ostringstream out;out<<"{\"sessions\":[";bool first=true;for(const auto& s:entries){std::lock_guard lock(s->mutex);if(s->released)continue;if(!first)out<<',';first=false;out<<reader_payload(*s,false);}out<<"]}";response.set_content(out.str(),"application/json");});
}
void handle_reader_release(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"session_id","expected_revision"});auto s=reader_find(request);std::lock_guard lock(s->mutex);reader_open(*s);const auto revision=live_revision(request);
    if(s->reader->revision()!=revision)throw minera::StreamingSessionConflict("reader session revision changed; refresh before releasing");
    s->reader->action(minera::ReaderStreamAction::kFinish,revision);auto payload=reader_payload(*s);payload.pop_back();payload+=",\"released\":true}";
    s->released=true;s->reader.reset();{std::lock_guard registry(g_reader_mutex);g_reader_sessions.erase(s->id);}response.set_content(payload,"application/json");});
}
