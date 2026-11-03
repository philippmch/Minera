// Included after the existing request/import helpers in minera_server.cpp.
#include "minera/streaming_session.hpp"

struct LiveUpload {
  std::string id, name, format, provided_sha256, bytes;
};
struct LiveCapacityError : std::runtime_error { int status; LiveCapacityError(int code,const std::string& message):std::runtime_error(message),status(code){} };
// Server-wide ceilings across every live and reader session, on top of each
// session's own budget: without them 32 sessions at their per-session limits
// could hold tens of GiB. A full budget refuses (429) until a session is
// released. One live event costs about 2 KB once queued and delivered.
constexpr std::size_t kStreamGlobalMaxStoredBytes = std::size_t{1} << 30;
constexpr std::size_t kStreamGlobalMaxEvents = 2000000;
std::atomic<std::size_t> g_stream_budget_bytes{0}, g_stream_budget_events{0};
void stream_budget_take(std::atomic<std::size_t>& counter,std::size_t limit,std::size_t amount,const char* what) {
  auto current=counter.load();
  do {
    if(amount>limit||current>limit-amount)
      throw LiveCapacityError(429,std::string("the server's stream ")+what+" budget ("+std::to_string(limit)+") across live and reader sessions is exhausted; release a session and retry");
  } while(!counter.compare_exchange_weak(current,current+amount));
}
// What one session holds of the global budget; returned when it is released
// or destroyed, whichever comes first.
struct StreamBudgetCharge {
  std::size_t bytes=0, events=0;
  StreamBudgetCharge()=default;
  StreamBudgetCharge(const StreamBudgetCharge&)=delete;
  StreamBudgetCharge& operator=(const StreamBudgetCharge&)=delete;
  ~StreamBudgetCharge() {release();}
  void reserve(std::size_t more_bytes,std::size_t more_events) {
    stream_budget_take(g_stream_budget_bytes,kStreamGlobalMaxStoredBytes,more_bytes,"byte");
    try {stream_budget_take(g_stream_budget_events,kStreamGlobalMaxEvents,more_events,"event");}
    catch(...) {g_stream_budget_bytes-=more_bytes;throw;}
    bytes+=more_bytes;events+=more_events;
  }
  void absorb(StreamBudgetCharge& other) noexcept {bytes+=other.bytes;events+=other.events;other.bytes=other.events=0;}
  void release() noexcept {g_stream_budget_bytes-=bytes;g_stream_budget_events-=events;bytes=events=0;}
};
// The server-wide stream budget in use, for the session lists:
// "stream_budget":{"bytes":N,"events":N,"max_bytes":N,"max_events":N}.
std::string stream_budget_json() {
  return "\"stream_budget\":{\"bytes\":"+std::to_string(g_stream_budget_bytes.load())+",\"events\":"+std::to_string(g_stream_budget_events.load())+
         ",\"max_bytes\":"+std::to_string(kStreamGlobalMaxStoredBytes)+",\"max_events\":"+std::to_string(kStreamGlobalMaxEvents)+"}";
}
// When a request last named a session (steady clock ticks). Sessions nobody
// touches for the idle expiry are freed by the stream idle reaper.
using StreamTouch = std::atomic<std::chrono::steady_clock::rep>;
inline std::chrono::steady_clock::rep stream_now() { return std::chrono::steady_clock::now().time_since_epoch().count(); }
struct LiveSessionRecord {
  std::mutex mutex;
  StreamTouch touched{stream_now()};
  std::string id, method;
  httplib::Params configuration;
  minera::CsvImportOptions csv;
  minera::XesImportOptions xes;
  // Shared so a request can wait for the queue without the record lock while
  // a concurrent release drops the record's own reference.
  std::shared_ptr<minera::StreamingSession> monitor;
  std::size_t stored_bytes = 0, stored_events = 0;
  std::vector<LiveUpload> sources;
  StreamBudgetCharge budget;
  bool released = false;
};
// Sessions live in server memory until released, so the registries are
// bounded: a full registry refuses new sessions (429) until one is released,
// and a session refuses uploads beyond its stored-byte budget (413).
constexpr std::size_t kLiveMaxSessions = 32;
constexpr std::size_t kLiveMaxStoredBytes = 256u * 1024u * 1024u;
// Each queued event and its delivery record cost a few KB in memory.
constexpr std::size_t kLiveMaxEvents = 1000000;
std::mutex g_live_mutex;
std::map<std::string,std::shared_ptr<LiveSessionRecord>> g_live_sessions;

// An estimate, from above, of the events a stream source holds, without
// parsing it. CSV: the non-blank lines after the header (a quoted field that
// spans lines counts once per line). XES: the <event start tags, counting
// only those before the (max_traces + 1)-th <trace when xes_max_traces limits
// the import. A gzip XES source is inflated within the import limit only to
// count.
std::size_t reader_estimated_events(const std::string& source,const std::string& kind,const minera::XesImportOptions& xes) {
  if(kind=="csv_events"){
    std::size_t lines=0;bool content=false;
    for(const char c:source){
      if(c=='\n'){if(content)++lines;content=false;}
      else if(c!='\r'&&c!=' '&&c!='\t')content=true;
    }
    if(content)++lines;
    return lines>0?lines-1:0;  // the header row is not an event
  }
  const std::string inflated=minera::detail::is_gzip_data(source)?minera::detail::gzip_decompress(source,xes.max_decompressed_bytes):std::string();
  const std::string_view text=inflated.empty()?std::string_view(source):std::string_view(inflated);
  // A start tag named exactly `name`: "<name" followed by whitespace, '>' or '/'.
  const auto tag_at=[&](std::size_t at,std::string_view name){
    if(text.compare(at,name.size(),name)!=0)return false;
    const auto next=at+name.size();
    if(next>=text.size())return false;
    const char c=text[next];return c=='>'||c=='/'||c==' '||c=='\t'||c=='\n'||c=='\r';
  };
  std::size_t events=0,traces=0;
  for(auto at=text.find('<');at!=std::string_view::npos;at=text.find('<',at+1)){
    if(tag_at(at,"<event"))++events;
    else if(tag_at(at,"<trace")&&++traces>xes.max_traces)break;
  }
  return events;
}
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
  if(it==g_live_sessions.end())throw std::out_of_range("live session is unavailable; it may have been released, expired after being idle, or the server restarted");it->second->touched=stream_now();return it->second;
}
void live_open(const LiveSessionRecord& session) {
  if(session.released||!session.monitor)throw std::out_of_range("live session was released");
}
std::size_t live_revision(const httplib::Request& request) { (void)live_required(request,"expected_revision");return ocel_query_size(request,"expected_revision",0); }
std::size_t live_delivery_limit(const httplib::Request& request) {
  const auto limit=ocel_query_size(request,"deliveries_limit",minera::StreamingSession::kDefaultDeliveryLimit);
  if(limit>100000)throw std::invalid_argument("deliveries_limit must be at most 100000");return limit;
}
// Callers hold session.mutex; the monitor snapshot never waits for the queue
// here (requests wait first, without the record lock).
std::string live_payload(LiveSessionRecord& session,std::size_t delivery_limit=minera::StreamingSession::kDefaultDeliveryLimit) {
  live_open(session);auto json=session.monitor->snapshot_json(false,delivery_limit);json.pop_back();const auto quote=minera::embedding_runtime::detail::escape_json;
  std::ostringstream out;out<<json<<",\"session_id\":"<<quote(session.id)<<",\"configuration\":{";
  bool first=true;for(const auto& [key,value]:session.configuration){if(!first)out<<',';first=false;out<<quote(key)<<':'<<quote(value);}out<<"},\"sources\":[";
  for(std::size_t i=0;i<session.sources.size();++i){const auto& s=session.sources[i];if(i)out<<',';out<<"{\"source_id\":"<<quote(s.id)<<",\"name\":"<<quote(s.name)<<",\"format\":"<<quote(s.format)<<",\"size\":"<<s.bytes.size()<<",\"provided_sha256\":"<<quote(s.provided_sha256)<<'}';}out<<"]}";return out.str();
}
template<class F> void live_response(httplib::Response& response,F action) {
  try {action();}
  catch(const LiveCapacityError& e){response.status=e.status;response.set_content(error_json(e.what()),"application/json");}
  catch(const minera::StreamingSessionConflict& e){response.status=409;response.set_content(error_json(e.what()),"application/json");}
  catch(const std::out_of_range& e){response.status=404;response.set_content(error_json(e.what()),"application/json");}
  catch(const std::exception& e){response.status=400;response.set_content(error_json(e.what()),"application/json");}
}
void handle_live_create(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{
    const auto method=live_required(request,"method"),id=live_required(request,"session_id");live_id(id);
    const std::string prefix=method=="dfg"?"streaming_":"streaming_"+method+"_";
    std::set<std::string> allowed={"session_id","method","streaming_event_order","streaming_include_diagnostics","csv_delimiter","csv_trim_fields","csv_has_header","csv_case_aliases","csv_resource_column","csv_lifecycle_column","xes_variant","xes_case_attribute","xes_activity_attribute","xes_timestamp_attribute","xes_resource_attribute","xes_lifecycle_attribute"};
    allowed.insert(prefix+"case_id_column");allowed.insert(prefix+"activity_column");
    if(method=="footprints")for(const auto* suffix:{"activities","start_activities","end_activities","sequence","parallel","terminate_all"})allowed.insert(prefix+suffix);
    else if(method=="temporal")for(const auto* suffix:{"profile","zeta","timestamp_column","start_timestamp_column"})allowed.insert(prefix+suffix);
    else if(method=="tbr")for(const auto* suffix:{"variant","stop_immediately","max_invisible","terminate_all"})allowed.insert(prefix+suffix);
    else if(method=="declare")allowed.insert(prefix+"timestamp_column");
    else if(method!="dfg")throw std::invalid_argument("unknown live monitor method");
    // Only the model-backed monitors store an uploaded source to name.
    if(method=="tbr"||method=="declare"){allowed.insert("source_name");allowed.insert("source_sha256");}
    live_params(request,allowed);
    // Live snapshots always carry their delivery diagnostics; the streaming
    // flag is accepted for query compatibility with /api/streaming-* only
    // when it asks for that behavior.
    if(!ocel_query_bool(request,"streaming_include_diagnostics",true))throw std::invalid_argument("live snapshots always include diagnostics; streaming_include_diagnostics must be true");
    if(ocel_query_bool(request,prefix+"terminate_all",false))throw std::invalid_argument("finalize live cases using a session action");
    auto record=std::make_shared<LiveSessionRecord>();record->id=id;record->method=method;record->configuration=request.params;
    if(request.has_param(prefix+"case_id_column"))record->csv.case_id_column=request.get_param_value(prefix+"case_id_column");
    if(request.has_param(prefix+"activity_column"))record->csv.activity_column=request.get_param_value(prefix+"activity_column");
    if(request.has_param(prefix+"timestamp_column"))record->csv.timestamp_column=request.get_param_value(prefix+"timestamp_column");
    // Batches are uploaded event streams: keep rows the batch importer would
    // drop for an NA-spelled or empty case id/activity, as the reference CSV
    // stream reader does.
    record->csv.keep_na_first_class_values=true;
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
    } else if(method=="declare") {
      options.method=minera::StreamingSessionMethod::kDeclare;const auto bytes=form_field(request,"model");if(bytes.empty())throw std::invalid_argument("live Declare requires a model JSON document");options.declare_model=minera::parse_declare_stream_model_json(bytes);
      record->sources.push_back({"model",request.has_param("source_name")?request.get_param_value("source_name"):"declare-model.json","declare_json",request.has_param("source_sha256")?request.get_param_value("source_sha256"):"",bytes});
    } else if(method=="tbr") {
      options.method=minera::StreamingSessionMethod::kTbr;const auto variant=request.has_param(prefix+"variant")?request.get_param_value(prefix+"variant"):"forward";
      if(variant=="backwards")options.tbr.variant=minera::StreamingTbrVariant::kBackwards;else if(variant!="forward")throw std::invalid_argument("unknown replay variant");
      options.tbr.maximum_iterations_invisibles=ocel_query_size(request,prefix+"max_invisible",options.tbr.maximum_iterations_invisibles);options.tbr.stop_immediately_unfit=ocel_query_bool(request,prefix+"stop_immediately",false);
      const auto bytes=form_field(request,"model");if(bytes.empty())throw std::invalid_argument("live replay requires a PNML model");options.net=minera::parse_pnml(bytes);
      record->sources.push_back({"model",request.has_param("source_name")?request.get_param_value("source_name"):"model.pnml","pnml",request.has_param("source_sha256")?request.get_param_value("source_sha256"):"",bytes});
    }
    record->monitor=std::make_shared<minera::StreamingSession>(std::move(options));
    for(const auto& source:record->sources)record->stored_bytes+=source.bytes.size();
    // Charged before registering; a record that loses to an existing ID
    // returns its charge when it is destroyed.
    record->budget.reserve(record->stored_bytes,0);
    {std::lock_guard lock(g_live_mutex);const auto found=g_live_sessions.find(id);
      if(found!=g_live_sessions.end()){record=found->second;record->touched=stream_now();}
      else if(g_live_sessions.size()>=kLiveMaxSessions)throw LiveCapacityError(429,"too many live sessions ("+std::to_string(kLiveMaxSessions)+"); release one before creating another");
      else{record->touched=stream_now();g_live_sessions.emplace(id,record);}}
    std::lock_guard lock(record->mutex);live_open(*record);
    if(record->configuration!=request.params||((method=="tbr"||method=="declare")&&record->sources.front().bytes!=form_field(request,"model")))throw minera::StreamingSessionConflict("live session ID already has a different configuration");
    response.set_content(live_payload(*record),"application/json");
  });
}
std::shared_ptr<minera::StreamingSession> live_monitor(LiveSessionRecord& session) { std::lock_guard lock(session.mutex);live_open(session);return session.monitor; }
void handle_live_snapshot(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"session_id","wait_until_drained","deliveries_limit"});const auto limit=live_delivery_limit(request);auto s=live_find(request);
    if(ocel_query_bool(request,"wait_until_drained",true))live_monitor(*s)->wait_until_drained();
    std::lock_guard lock(s->mutex);response.set_content(live_payload(*s,limit),"application/json");});
}
void handle_live_append(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{
    live_params(request,{"session_id","expected_revision","batch_id","format","source_name","source_sha256","wait_until_drained","deliveries_limit"});const auto limit=live_delivery_limit(request);auto s=live_find(request);std::shared_ptr<minera::StreamingSession> monitor;
    {std::unique_lock lock(s->mutex);live_open(*s);const auto revision=live_revision(request);
    if(s->monitor->revision()!=revision)throw minera::StreamingSessionConflict("live session revision changed; refresh before appending");
    const bool wait=ocel_query_bool(request,"wait_until_drained",true);
    const auto batch=live_required(request,"batch_id"),format=live_required(request,"format");live_id(batch);if(batch=="model")throw std::invalid_argument("model is reserved as a source ID");
    const auto bytes=ocel_request_body(request,"log");if(bytes.empty())throw std::invalid_argument("live event upload is empty");
    if(s->stored_bytes+bytes.size()>kLiveMaxStoredBytes)throw LiveCapacityError(413,"live session upload budget ("+std::to_string(kLiveMaxStoredBytes)+" bytes) is exhausted; release it and start another");
    // Parsing materializes every event, so bound the batch by an upper
    // estimate of its events before parsing it (see reader_estimated_events).
    if(format=="csv"||format=="xes"){const auto estimated=reader_estimated_events(bytes,format=="csv"?"csv_events":"xes_events",s->xes);
      if(s->stored_events+estimated>kLiveMaxEvents)throw LiveCapacityError(413,"live session event budget ("+std::to_string(kLiveMaxEvents)+" events) cannot take a batch of up to "+std::to_string(estimated)+" events; release it and start another");}
    StreamBudgetCharge charge;charge.reserve(bytes.size(),0);std::istringstream input(bytes);minera::EventLog log;
    if(format=="csv")log=minera::import_csv_event_log(input,s->csv);else if(format=="xes")log=minera::import_xes_event_log(input,s->xes);else throw std::invalid_argument("live batches require CSV or XES format");
    httplib::Request order;order.params.emplace("streaming_event_order",s->configuration.contains("streaming_event_order")?s->configuration.find("streaming_event_order")->second:"input");std::vector<minera::Event> events;for(const auto* event:streaming_events(log,order))events.push_back(*event);
    // The monitors select the session's activity and timestamp fields by its
    // column names; an XES batch exposes its selected XES attributes under
    // them unless the event carries a literal field of that name.
    if(format=="xes")for(auto& event:events){const auto literal=[&](const std::string& key){return std::any_of(event.attributes.begin(),event.attributes.end(),[&](const auto& a){return a.key==key;});};if(!literal(s->csv.activity_column))event.activity_source_key=s->csv.activity_column;if(!literal(s->csv.timestamp_column))event.timestamp_source_key=s->csv.timestamp_column;}
    if(s->stored_events+events.size()>kLiveMaxEvents)throw LiveCapacityError(413,"live session event budget ("+std::to_string(kLiveMaxEvents)+" events) is exhausted; release it and start another");
    charge.reserve(0,events.size());
    const auto appended=events.size();s->monitor->append(std::move(events),batch,revision);s->budget.absorb(charge);s->stored_events+=appended;s->sources.push_back({batch,request.has_param("source_name")?request.get_param_value("source_name"):batch,format,request.has_param("source_sha256")?request.get_param_value("source_sha256"):"",bytes});s->stored_bytes+=bytes.size();
    if(!wait){response.set_content(live_payload(*s,limit),"application/json");return;}
    monitor=s->monitor;}
    monitor->wait_until_drained();
    std::lock_guard lock(s->mutex);response.set_content(live_payload(*s,limit),"application/json");
  });
}
void handle_live_action(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{
    live_params(request,{"session_id","expected_revision","action","case_id","deliveries_limit"});const auto limit=live_delivery_limit(request);auto s=live_find(request);const auto monitor=live_monitor(*s);
    const std::map<std::string,minera::StreamingSessionAction> actions={{"start",minera::StreamingSessionAction::kStart},{"drain",minera::StreamingSessionAction::kDrain},{"finish",minera::StreamingSessionAction::kFinish},{"register",minera::StreamingSessionAction::kRegister},{"deregister",minera::StreamingSessionAction::kDeregister},{"reset_all",minera::StreamingSessionAction::kResetAll},{"reset_case",minera::StreamingSessionAction::kResetCase},{"terminate_case",minera::StreamingSessionAction::kTerminateCase},{"terminate_all",minera::StreamingSessionAction::kTerminateAll}};
    const auto action=live_required(request,"action");const auto it=actions.find(action);if(it==actions.end())throw std::invalid_argument("unknown live action");
    // The monitor serializes and revision-checks actions itself and waits for
    // the queue without a lock, so the record lock is not held meanwhile.
    monitor->action(it->second,live_revision(request),request.has_param("case_id")?request.get_param_value("case_id"):"");std::lock_guard lock(s->mutex);response.set_content(live_payload(*s,limit),"application/json");
  });
}
void handle_live_list(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{});std::vector<std::shared_ptr<LiveSessionRecord>> sessions;{std::lock_guard lock(g_live_mutex);for(const auto& [id,s]:g_live_sessions){(void)id;sessions.push_back(s);}}
    std::ostringstream out;out<<"{\"sessions\":[";bool first=true;for(const auto& s:sessions){std::lock_guard lock(s->mutex);if(s->released)continue;if(!first)out<<',';first=false;out<<live_payload(*s,0);}out<<"],"<<stream_budget_json()<<"}";response.set_content(out.str(),"application/json");});
}
void handle_live_source(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"session_id","source_id"});auto s=live_find(request);std::lock_guard lock(s->mutex);live_open(*s);const auto id=live_required(request,"source_id");for(const auto& source:s->sources)if(source.id==id){response.set_content(source.bytes,"application/octet-stream");return;}throw std::out_of_range("live source upload is unavailable");});
}
void handle_live_release(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"session_id","expected_revision"});auto s=live_find(request);const auto revision=live_revision(request);
    {const auto monitor=live_monitor(*s);if(monitor->revision()!=revision)throw minera::StreamingSessionConflict("live session revision changed; refresh before releasing");monitor->wait_until_drained();}
    std::lock_guard lock(s->mutex);live_open(*s);if(s->monitor->revision()!=revision)throw minera::StreamingSessionConflict("live session revision changed; refresh before releasing");
    // An unstarted session may still hold queued events; releasing it
    // discards that queue (the stream's stop clears an inactive queue).
    const auto before=s->monitor->snapshot_json(false,0);
    if(before.find("\"state\":\"ACTIVE\"")!=std::string::npos)s->monitor->action(minera::StreamingSessionAction::kFinish,revision);
    auto payload=live_payload(*s);payload.pop_back();payload+=",\"released\":true}";s->released=true;s->monitor.reset();
    // The uploads are dropped now too, not when the last request lets go.
    std::vector<LiveUpload>().swap(s->sources);s->budget.release();
    {std::lock_guard registry_lock(g_live_mutex);g_live_sessions.erase(s->id);}response.set_content(payload,"application/json");});
  // A released session's queue, deliveries and uploads go back to the system.
  if(response.status<300)minera_release_free_memory();
}
