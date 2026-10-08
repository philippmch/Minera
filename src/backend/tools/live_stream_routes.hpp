// Included after the existing request/import helpers in minera_server.cpp.
#include "minera/streaming_session.hpp"

struct LiveUpload {
  std::string id, name, format, provided_sha256, bytes;
};
struct LiveSessionRecord {
  std::mutex mutex;
  std::string id, method;
  httplib::Params configuration;
  minera::CsvImportOptions csv;
  minera::XesImportOptions xes;
  std::unique_ptr<minera::StreamingSession> monitor;
  std::vector<LiveUpload> sources;
  bool released = false;
};
std::mutex g_live_mutex;
std::map<std::string,std::shared_ptr<LiveSessionRecord>> g_live_sessions;

std::string live_required(const httplib::Request& request,const std::string& key) {
  if(!request.has_param(key)||request.get_param_value(key).empty())throw std::invalid_argument(key+" is required");
  return request.get_param_value(key);
}
void live_id(const std::string& value) {
  if(value.empty()||value.size()>128||!std::all_of(value.begin(),value.end(),[](unsigned char c){return std::isalnum(c)||c=='-'||c=='_';}))
    throw std::invalid_argument("live identifiers require 1 to 128 letters, digits, hyphens or underscores");
}
void live_params(const httplib::Request& request,const std::set<std::string>& allowed) {
  for(const auto& [key,value]:request.params) {
    (void)value;if(!allowed.contains(key)||request.params.count(key)!=1)throw std::invalid_argument("unknown or repeated live parameter: "+key);
  }
}
std::shared_ptr<LiveSessionRecord> live_find(const httplib::Request& request) {
  const auto id=live_required(request,"session_id");std::lock_guard lock(g_live_mutex);const auto it=g_live_sessions.find(id);
  if(it==g_live_sessions.end())throw std::out_of_range("live session is unavailable; it may have been released or the server restarted");return it->second;
}
void live_open(const LiveSessionRecord& session) {
  if(session.released||!session.monitor)throw std::out_of_range("live session was released");
}
std::size_t live_revision(const httplib::Request& request) { (void)live_required(request,"expected_revision");return ocel_query_size(request,"expected_revision",0); }
std::string live_payload(LiveSessionRecord& session,bool wait=true) {
  live_open(session);auto json=session.monitor->snapshot_json(wait);json.pop_back();const auto quote=minera::embedding_runtime::detail::escape_json;
  std::ostringstream out;out<<json<<",\"session_id\":"<<quote(session.id)<<",\"configuration\":{";
  bool first=true;for(const auto& [key,value]:session.configuration){if(!first)out<<',';first=false;out<<quote(key)<<':'<<quote(value);}out<<"},\"sources\":[";
  for(std::size_t i=0;i<session.sources.size();++i){const auto& s=session.sources[i];if(i)out<<',';out<<"{\"source_id\":"<<quote(s.id)<<",\"name\":"<<quote(s.name)<<",\"format\":"<<quote(s.format)<<",\"size\":"<<s.bytes.size()<<",\"provided_sha256\":"<<quote(s.provided_sha256)<<'}';}out<<"]}";return out.str();
}
template<class F> void live_response(httplib::Response& response,F action) {
  try {action();}
  catch(const minera::StreamingSessionConflict& e){response.status=409;response.set_content(error_json(e.what()),"application/json");}
  catch(const std::out_of_range& e){response.status=404;response.set_content(error_json(e.what()),"application/json");}
  catch(const std::exception& e){response.status=400;response.set_content(error_json(e.what()),"application/json");}
}
void handle_live_create(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{
    const auto method=live_required(request,"method"),id=live_required(request,"session_id");live_id(id);
    const std::string prefix=method=="dfg"?"streaming_":"streaming_"+method+"_";
    std::set<std::string> allowed={"session_id","method","streaming_event_order","streaming_include_diagnostics","source_name","source_sha256","csv_delimiter","csv_trim_fields","csv_has_header","csv_case_aliases","csv_resource_column","csv_lifecycle_column","xes_variant","xes_case_attribute","xes_activity_attribute","xes_timestamp_attribute","xes_resource_attribute","xes_lifecycle_attribute"};
    allowed.insert(prefix+"case_id_column");allowed.insert(prefix+"activity_column");
    if(method=="footprints")for(const auto* suffix:{"activities","start_activities","end_activities","sequence","parallel","terminate_all"})allowed.insert(prefix+suffix);
    else if(method=="temporal")for(const auto* suffix:{"profile","zeta","timestamp_column","start_timestamp_column"})allowed.insert(prefix+suffix);
    else if(method=="tbr")for(const auto* suffix:{"variant","stop_immediately","max_invisible","terminate_all"})allowed.insert(prefix+suffix);
    else if(method!="dfg")throw std::invalid_argument("unknown live monitor method");
    live_params(request,allowed);
    if(ocel_query_bool(request,prefix+"terminate_all",false))throw std::invalid_argument("finalize live cases using a session action");
    auto record=std::make_shared<LiveSessionRecord>();record->id=id;record->method=method;record->configuration=request.params;
    if(request.has_param(prefix+"case_id_column"))record->csv.case_id_column=request.get_param_value(prefix+"case_id_column");
    if(request.has_param(prefix+"activity_column"))record->csv.activity_column=request.get_param_value(prefix+"activity_column");
    if(request.has_param(prefix+"timestamp_column"))record->csv.timestamp_column=request.get_param_value(prefix+"timestamp_column");
    if(record->csv.case_id_column.empty()||record->csv.activity_column.empty()||record->csv.timestamp_column.empty())throw std::invalid_argument("live CSV selectors must not be empty");
    if(request.has_param("csv_delimiter")){const auto v=request.get_param_value("csv_delimiter");if(v.size()!=1||v[0]=='\0'||v[0]=='\r'||v[0]=='\n')throw std::invalid_argument("CSV delimiter must be one character");record->csv.delimiter=v[0];}
    record->csv.trim_fields=ocel_query_bool(request,"csv_trim_fields",record->csv.trim_fields);record->csv.has_header=ocel_query_bool(request,"csv_has_header",record->csv.has_header);record->csv.allow_case_id_aliases=ocel_query_bool(request,"csv_case_aliases",record->csv.allow_case_id_aliases);
    if(request.has_param("csv_resource_column"))record->csv.resource_column=request.get_param_value("csv_resource_column");
    if(request.has_param("csv_lifecycle_column"))record->csv.lifecycle_column=request.get_param_value("csv_lifecycle_column");
    if(request.has_param("xes_variant"))record->xes.variant=minera::parse_xes_import_variant(request.get_param_value("xes_variant"));
    for(const auto& [key,target]:std::initializer_list<std::pair<std::string,std::string*>>{{"xes_case_attribute",&record->xes.case_attribute},{"xes_activity_attribute",&record->xes.activity_attribute},{"xes_timestamp_attribute",&record->xes.timestamp_attribute},{"xes_resource_attribute",&record->xes.resource_attribute},{"xes_lifecycle_attribute",&record->xes.lifecycle_attribute}})
      if(request.has_param(key)){*target=request.get_param_value(key);if(target->empty())throw std::invalid_argument(key+" must not be empty");}
    minera::StreamingSessionOptions options;options.event_order=request.has_param("streaming_event_order")?request.get_param_value("streaming_event_order"):"input";
    options.dfg={record->csv.activity_column,record->csv.case_id_column};options.footprints={record->csv.activity_column,record->csv.case_id_column};options.temporal.activity_attribute=record->csv.activity_column;options.temporal.case_id_attribute=record->csv.case_id_column;options.temporal.timestamp_attribute=record->csv.timestamp_column;options.tbr.activity_attribute=record->csv.activity_column;options.tbr.case_id_attribute=record->csv.case_id_column;
    if(method=="footprints") {
      options.method=minera::StreamingSessionMethod::kFootprints;minera::Footprint model;
      const auto list=[&](const std::string& key){const auto raw=live_required(request,key);std::set<std::string> values;std::size_t begin=0;while(begin<=raw.size()){const auto end=raw.find(',',begin);auto value=raw.substr(begin,end==std::string::npos?std::string::npos:end-begin);const auto left=value.find_first_not_of(' '),right=value.find_last_not_of(' ');if(left==std::string::npos)throw std::invalid_argument("footprint names must not be empty");values.insert(value.substr(left,right-left+1));if(end==std::string::npos)break;begin=end+1;}return values;};
      model.activities=list(prefix+"activities");model.start_activities_present=request.has_param(prefix+"start_activities");model.end_activities_present=request.has_param(prefix+"end_activities");
      if(model.start_activities_present)model.start_activities=list(prefix+"start_activities");if(model.end_activities_present)model.end_activities=list(prefix+"end_activities");
      if(request.has_param(prefix+"sequence"))for(const auto& pair:minera::parse_activity_pairs(request.get_param_value(prefix+"sequence")))model.sequence.emplace(pair.predecessor,pair.successor);
      if(request.has_param(prefix+"parallel"))for(const auto& pair:minera::parse_activity_pairs(request.get_param_value(prefix+"parallel")))model.parallel.emplace(pair.predecessor,pair.successor);
      options.footprint_model=std::move(model);
    } else if(method=="temporal") {
      options.method=minera::StreamingSessionMethod::kTemporal;options.temporal_profile=parse_temporal_profile_parameter(live_required(request,prefix+"profile"),prefix+"profile");options.temporal.zeta=ocel_query_number(request,prefix+"zeta",options.temporal.zeta);
      if(request.has_param(prefix+"start_timestamp_column"))options.temporal.start_timestamp_attribute=request.get_param_value(prefix+"start_timestamp_column");
    } else if(method=="tbr") {
      options.method=minera::StreamingSessionMethod::kTbr;const auto variant=request.has_param(prefix+"variant")?request.get_param_value(prefix+"variant"):"forward";
      if(variant=="backwards")options.tbr.variant=minera::StreamingTbrVariant::kBackwards;else if(variant!="forward")throw std::invalid_argument("unknown replay variant");
      options.tbr.maximum_iterations_invisibles=ocel_query_size(request,prefix+"max_invisible",options.tbr.maximum_iterations_invisibles);options.tbr.stop_immediately_unfit=ocel_query_bool(request,prefix+"stop_immediately",false);
      const auto bytes=form_field(request,"model");if(bytes.empty())throw std::invalid_argument("live replay requires a PNML model");options.net=minera::parse_pnml(bytes);
      record->sources.push_back({"model",request.has_param("source_name")?request.get_param_value("source_name"):"model.pnml","pnml",request.has_param("source_sha256")?request.get_param_value("source_sha256"):"",bytes});
    }
    record->monitor=std::make_unique<minera::StreamingSession>(std::move(options));
    {std::lock_guard lock(g_live_mutex);auto [it,inserted]=g_live_sessions.emplace(id,record);if(!inserted){auto existing=it->second;record=std::move(existing);}}
    std::lock_guard lock(record->mutex);live_open(*record);
    if(record->configuration!=request.params||(method=="tbr"&&record->sources.front().bytes!=form_field(request,"model")))throw minera::StreamingSessionConflict("live session ID already has a different configuration");
    response.set_content(live_payload(*record),"application/json");
  });
}
void handle_live_snapshot(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"session_id","wait_until_drained"});auto s=live_find(request);std::lock_guard lock(s->mutex);response.set_content(live_payload(*s,ocel_query_bool(request,"wait_until_drained",true)),"application/json");});
}
void handle_live_append(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{
    live_params(request,{"session_id","expected_revision","batch_id","format","source_name","source_sha256","wait_until_drained"});auto s=live_find(request);std::lock_guard lock(s->mutex);live_open(*s);const auto revision=live_revision(request);
    if(s->monitor->revision()!=revision)throw minera::StreamingSessionConflict("live session revision changed; refresh before appending");
    const bool wait=ocel_query_bool(request,"wait_until_drained",true);
    const auto batch=live_required(request,"batch_id"),format=live_required(request,"format");live_id(batch);if(batch=="model")throw std::invalid_argument("model is reserved as a source ID");
    const auto bytes=ocel_request_body(request,"log");if(bytes.empty())throw std::invalid_argument("live event upload is empty");std::istringstream input(bytes);minera::EventLog log;
    if(format=="csv")log=minera::import_csv_event_log(input,s->csv);else if(format=="xes")log=minera::import_xes_event_log(input,s->xes);else throw std::invalid_argument("live batches require CSV or XES format");
    httplib::Request order;order.params.emplace("streaming_event_order",s->configuration.contains("streaming_event_order")?s->configuration.find("streaming_event_order")->second:"input");std::vector<minera::Event> events;for(const auto* event:streaming_events(log,order))events.push_back(*event);
    s->monitor->append(std::move(events),batch,revision);s->sources.push_back({batch,request.has_param("source_name")?request.get_param_value("source_name"):batch,format,request.has_param("source_sha256")?request.get_param_value("source_sha256"):"",bytes});
    response.set_content(live_payload(*s,wait),"application/json");
  });
}
void handle_live_action(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{
    live_params(request,{"session_id","expected_revision","action","case_id"});auto s=live_find(request);std::lock_guard lock(s->mutex);live_open(*s);
    const std::map<std::string,minera::StreamingSessionAction> actions={{"start",minera::StreamingSessionAction::kStart},{"drain",minera::StreamingSessionAction::kDrain},{"finish",minera::StreamingSessionAction::kFinish},{"register",minera::StreamingSessionAction::kRegister},{"deregister",minera::StreamingSessionAction::kDeregister},{"reset_all",minera::StreamingSessionAction::kResetAll},{"reset_case",minera::StreamingSessionAction::kResetCase},{"terminate_case",minera::StreamingSessionAction::kTerminateCase},{"terminate_all",minera::StreamingSessionAction::kTerminateAll}};
    const auto action=live_required(request,"action");const auto it=actions.find(action);if(it==actions.end())throw std::invalid_argument("unknown live action");s->monitor->action(it->second,live_revision(request),request.has_param("case_id")?request.get_param_value("case_id"):"");response.set_content(live_payload(*s),"application/json");
  });
}
void handle_live_list(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{});std::vector<std::shared_ptr<LiveSessionRecord>> sessions;{std::lock_guard lock(g_live_mutex);for(const auto& [id,s]:g_live_sessions){(void)id;sessions.push_back(s);}}
    std::ostringstream out;out<<"{\"sessions\":[";bool first=true;for(const auto& s:sessions){std::lock_guard lock(s->mutex);if(s->released)continue;if(!first)out<<',';first=false;out<<live_payload(*s,false);}out<<"]}";response.set_content(out.str(),"application/json");});
}
void handle_live_source(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"session_id","source_id"});auto s=live_find(request);std::lock_guard lock(s->mutex);live_open(*s);const auto id=live_required(request,"source_id");for(const auto& source:s->sources)if(source.id==id){response.set_content(source.bytes,"application/octet-stream");return;}throw std::out_of_range("live source upload is unavailable");});
}
void handle_live_release(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"session_id","expected_revision"});auto s=live_find(request);std::lock_guard lock(s->mutex);live_open(*s);const auto revision=live_revision(request);if(s->monitor->revision()!=revision)throw minera::StreamingSessionConflict("live session revision changed; refresh before releasing");
    const auto before=s->monitor->snapshot_json();if(before.find("\"state\":\"INACTIVE\"")!=std::string::npos&&before.find("\"pending\":0,")==std::string::npos)throw minera::StreamingSessionConflict("start and finish the queued stream before releasing");
    if(before.find("\"state\":\"ACTIVE\"")!=std::string::npos)s->monitor->action(minera::StreamingSessionAction::kFinish,revision);
    auto payload=live_payload(*s);payload.pop_back();payload+=",\"released\":true}";s->released=true;s->monitor.reset();{std::lock_guard registry_lock(g_live_mutex);g_live_sessions.erase(s->id);}response.set_content(payload,"application/json");});
}
