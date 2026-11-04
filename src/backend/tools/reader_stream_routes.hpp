// Included after live_stream_routes.hpp and the shared import/request helpers.

struct ReaderSessionRecord {
  std::mutex mutex;
  StreamTouch touched{stream_now()};
  std::string id;
  httplib::Params configuration;
  LiveUpload source;
  // Shared so a snapshot can wait for the queue without the record lock.
  std::shared_ptr<minera::ReaderStreamSession> reader;
  StreamBudgetCharge budget;
  bool released=false;
};
std::mutex g_reader_mutex;
std::map<std::string,std::shared_ptr<ReaderSessionRecord>> g_reader_sessions;
std::shared_ptr<ReaderSessionRecord> reader_find(const httplib::Request& request) {
  const auto id=live_required(request,"session_id");std::lock_guard lock(g_reader_mutex);
  const auto it=g_reader_sessions.find(id);if(it==g_reader_sessions.end())throw std::out_of_range("reader session is unavailable; it may have been released, expired after being idle, or the server restarted");it->second->touched=stream_now();return it->second;
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
    // A reader parses its whole source when it is created, so a 128 MiB
    // upload of short CSV rows can need several GiB. Before parsing, the
    // source is bounded by an estimate of its events (CSV rows, XES <event
    // elements within xes_max_traces), checked against the per-session event
    // limit and charged, with the source bytes, against the server's stream
    // budget.
    const auto estimated_events=reader_estimated_events(options.source,kind,options.xes);
    if(estimated_events>kLiveMaxEvents)throw LiveCapacityError(413,"reader source holds up to "+std::to_string(estimated_events)+" events, above the per-session limit of "+std::to_string(kLiveMaxEvents));
    auto record=std::make_shared<ReaderSessionRecord>();record->budget.reserve(options.source.size(),estimated_events);record->id=id;record->configuration=request.params;
    record->source={"source",request.has_param("source_name")?request.get_param_value("source_name"):"reader-upload",kind=="csv_events"?"csv":"xes",request.has_param("source_sha256")?request.get_param_value("source_sha256"):"",options.source};
    record->reader=std::make_shared<minera::ReaderStreamSession>(std::move(options));
    {std::lock_guard lock(g_reader_mutex);const auto found=g_reader_sessions.find(id);
      if(found!=g_reader_sessions.end()){record=found->second;record->touched=stream_now();}
      else if(g_reader_sessions.size()>=kLiveMaxSessions)throw LiveCapacityError(429,"too many reader sessions ("+std::to_string(kLiveMaxSessions)+"); release one before creating another");
      else{record->touched=stream_now();g_reader_sessions.emplace(id,record);}}
    std::lock_guard lock(record->mutex);reader_open(*record);
    if(record->configuration!=request.params||record->source.bytes!=ocel_request_body(request,"log"))throw minera::StreamingSessionConflict("reader session ID already has a different source or configuration");
    response.set_content(reader_payload(*record),"application/json");
  });
}
void handle_reader_snapshot(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"session_id","wait_until_drained"});const bool wait=ocel_query_bool(request,"wait_until_drained",true);auto s=reader_find(request);
    // Drain without the record lock, as the live snapshot does, so listing
    // and releasing other readers are not blocked by a long queue.
    if(wait){std::shared_ptr<minera::ReaderStreamSession> reader;{std::lock_guard lock(s->mutex);reader_open(*s);reader=s->reader;}reader->wait_until_drained();}
    std::lock_guard lock(s->mutex);response.set_content(reader_payload(*s,wait),"application/json");});
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
    else if(format=="xes"){minera::XesExportOptions options;if(request.has_param("variant"))options.variant=minera::parse_xes_export_variant(request.get_param_value("variant"));options.compress=ocel_query_bool(request,"gzip",false);options.preserve_nulls=ocel_query_bool(request,"preserve_nulls",false);options.infer_metadata=ocel_query_bool(request,"infer_metadata",false);response.set_content(minera::serialize_xes(log,options),*options.compress?"application/gzip":"application/xml");}
    else throw std::invalid_argument("reader export format requires json or xes");
  });
}
void handle_reader_list(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{});std::vector<std::shared_ptr<ReaderSessionRecord>> entries;{std::lock_guard lock(g_reader_mutex);for(const auto& [id,s]:g_reader_sessions){(void)id;entries.push_back(s);}}
    std::ostringstream out;out<<"{\"sessions\":[";bool first=true;for(const auto& s:entries){std::lock_guard lock(s->mutex);if(s->released)continue;if(!first)out<<',';first=false;out<<reader_payload(*s,false);}out<<"],"<<stream_budget_json()<<"}";response.set_content(out.str(),"application/json");});
}
void handle_reader_release(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"session_id","expected_revision"});auto s=reader_find(request);std::lock_guard lock(s->mutex);reader_open(*s);const auto revision=live_revision(request);
    if(s->reader->revision()!=revision)throw minera::StreamingSessionConflict("reader session revision changed; refresh before releasing");
    s->reader->action(minera::ReaderStreamAction::kFinish,revision);auto payload=reader_payload(*s);payload.pop_back();payload+=",\"released\":true}";
    s->released=true;s->reader.reset();std::string().swap(s->source.bytes);s->budget.release();
    {std::lock_guard registry(g_reader_mutex);g_reader_sessions.erase(s->id);}response.set_content(payload,"application/json");});
  if(response.status<300)minera_release_free_memory();
}

// Idle expiry for live and reader sessions. A session that no request has
// named (create, snapshot, read, append, action, source, export, release; the
// session lists do not count) for the idle expiry, 30 minutes by default, is
// released as an explicit release would: its queue, deliveries and uploads
// are dropped and its share of the server's stream budget is returned. A
// session still working through queued events (ACTIVE with pending events)
// is never expired; its idle time counts from its last request, so a recent
// append keeps it too. MINERA_STREAM_IDLE_MS overrides the expiry
// (milliseconds, 0 disables it; the contract tests use it).
std::chrono::milliseconds stream_idle_expiry() {
  static const auto expiry = [] {
    std::chrono::milliseconds value(30 * 60 * 1000);
    if (const char* text = std::getenv("MINERA_STREAM_IDLE_MS")) {
      std::uint64_t parsed = 0;
      const auto* end = text + std::strlen(text);
      const auto [at, error] = std::from_chars(text, end, parsed);
      if (error == std::errc() && at == end && *text && parsed <= 7ull * 86400000) value = std::chrono::milliseconds(parsed);
    }
    return value;
  }();
  return expiry;
}
// A stream snapshot's "state" and "pending" fields, read without waiting.
bool stream_busy(const std::string& snapshot) {
  if (snapshot.find("\"state\":\"ACTIVE\"") == std::string::npos) return false;
  const auto at = snapshot.find("\"pending\":");
  return at != std::string::npos && snapshot.compare(at + 10, 2, "0,") != 0;
}
bool stream_idle(const StreamTouch& touched, std::chrono::steady_clock::rep now) {
  return now - touched.load() >= std::chrono::duration_cast<std::chrono::steady_clock::duration>(stream_idle_expiry()).count();
}
// Frees the idle sessions; returns how many it released. Never throws.
std::size_t expire_idle_stream_sessions() noexcept {
  std::size_t expired = 0;
  try {
    const auto now = stream_now();
    std::vector<std::shared_ptr<LiveSessionRecord>> live;
    {std::lock_guard lock(g_live_mutex);for(const auto& [id,s]:g_live_sessions){(void)id;if(stream_idle(s->touched,now))live.push_back(s);}}
    for (const auto& s : live) {
      try {
        std::lock_guard lock(s->mutex);
        // Re-checked under the record lock: a request may have named it since.
        if (s->released || !s->monitor || !stream_idle(s->touched, stream_now())) continue;
        const auto snapshot = s->monitor->snapshot_json(false, 0);
        if (stream_busy(snapshot)) continue;
        if (snapshot.find("\"state\":\"ACTIVE\"") != std::string::npos)
          s->monitor->action(minera::StreamingSessionAction::kFinish, s->monitor->revision());
        s->released = true; s->monitor.reset(); std::vector<LiveUpload>().swap(s->sources); s->budget.release();
        {std::lock_guard registry(g_live_mutex);const auto it=g_live_sessions.find(s->id);if(it!=g_live_sessions.end()&&it->second==s)g_live_sessions.erase(it);}
        ++expired;
      } catch (...) {}
    }
    std::vector<std::shared_ptr<ReaderSessionRecord>> readers;
    {std::lock_guard lock(g_reader_mutex);for(const auto& [id,s]:g_reader_sessions){(void)id;if(stream_idle(s->touched,now))readers.push_back(s);}}
    for (const auto& s : readers) {
      try {
        std::lock_guard lock(s->mutex);
        if (s->released || !s->reader || !stream_idle(s->touched, stream_now())) continue;
        if (stream_busy(s->reader->snapshot_json(false))) continue;
        try {s->reader->action(minera::ReaderStreamAction::kFinish, s->reader->revision());} catch (...) {}
        s->released = true; s->reader.reset(); std::string().swap(s->source.bytes); s->budget.release();
        {std::lock_guard registry(g_reader_mutex);const auto it=g_reader_sessions.find(s->id);if(it!=g_reader_sessions.end()&&it->second==s)g_reader_sessions.erase(it);}
        ++expired;
      } catch (...) {}
    }
    if (expired) minera_release_free_memory();
  } catch (...) {}
  return expired;
}
// Runs the expiry on a schedule while the server listens; main owns it, so
// it stops before the registries are destroyed.
class StreamIdleReaper {
 public:
  StreamIdleReaper() {
    const auto expiry = stream_idle_expiry();
    if (expiry.count() == 0) return;
    // Checked at a tenth of the expiry, between 50 ms and 60 s.
    const auto period = std::clamp(expiry / 10, std::chrono::milliseconds(50), std::chrono::milliseconds(60000));
    thread_ = std::thread([this, period] {
      std::unique_lock lock(mutex_);
      while (!stop_) {
        if (wake_.wait_for(lock, period, [this] {return stop_;})) break;
        lock.unlock();
        expire_idle_stream_sessions();
        lock.lock();
      }
    });
  }
  ~StreamIdleReaper() {
    {std::lock_guard lock(mutex_);stop_ = true;}
    wake_.notify_all();
    if (thread_.joinable()) thread_.join();
  }
  StreamIdleReaper(const StreamIdleReaper&) = delete;
  StreamIdleReaper& operator=(const StreamIdleReaper&) = delete;
 private:
  std::mutex mutex_;
  std::condition_variable wake_;
  bool stop_ = false;
  std::thread thread_;
};
