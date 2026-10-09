// Included after the shared live request helpers, inside the server namespace.
struct AcquiredSourceRecord {
  std::mutex mutex;
  std::string id,name,mode,path,provided_sha256;
  httplib::Params configuration;
  minera::SourceAcquisitionResult acquired;
  bool authenticated=false;
};
std::mutex g_acquired_mutex;
std::map<std::string,std::shared_ptr<AcquiredSourceRecord>> g_acquired_sources;
std::shared_ptr<AcquiredSourceRecord> acquired_find(const httplib::Request& request) {
  const auto id=live_required(request,"source_id");std::lock_guard lock(g_acquired_mutex);const auto it=g_acquired_sources.find(id);if(it==g_acquired_sources.end())throw std::out_of_range("acquired source is unavailable; it may have been released or the server restarted");return it->second;
}
std::string acquired_payload(const AcquiredSourceRecord& record) {
  auto json=minera::source_acquisition_json(record.acquired);json.pop_back();const auto quote=minera::embedding_runtime::detail::escape_json;std::ostringstream out;
  out<<json<<",\"source_id\":"<<quote(record.id)<<",\"configuration\":{";bool first=true;for(const auto& [key,value]:record.configuration){if(!first)out<<',';first=false;out<<quote(key)<<':'<<quote(value);}
  out<<"},\"source\":{\"name\":"<<quote(record.name)<<",\"mode\":"<<quote(record.mode)<<",\"path\":"<<quote(record.path)<<",\"has_original\":"<<(record.acquired.original_bytes?"true":"false")<<",\"size\":"<<(record.acquired.original_bytes?std::to_string(record.acquired.original_bytes->size()):"null")<<",\"provided_sha256\":"<<quote(record.provided_sha256)<<",\"authenticated\":"<<(record.authenticated?"true":"false")<<"}}";return out.str();
}
void handle_acquire_source(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{
    const auto connector=live_required(request,"connector"),mode=live_required(request,"mode"),id=live_required(request,"source_id");live_id(id);
    std::set<std::string> allowed={"source_id","connector","mode","source_name","source_path","source_sha256"};std::set<std::string> native_parameters;
    if(connector=="chrome_history"||connector=="firefox_history")native_parameters={"browser_history_max_events"};
    else if(connector=="windows_events")native_parameters={"windows_events_max_events","windows_events_source","windows_events_channel","windows_events_query","windows_events_reverse","windows_events_timeout_ms"};
    else if(connector=="github_repo")native_parameters={"repository_owner","repository_name","repository_api_base_url","repository_max_pages","repository_max_issues","repository_max_events"};
    else if(connector=="camunda_workflow")native_parameters={"workflow_max_events","database_driver","database_dialect","database_prefix","database_max_rows","database_timeout_seconds"};
    else if(connector=="sap_o2c"||connector=="sap_accounting")native_parameters={"enterprise_max_events","database_driver","database_dialect","database_prefix","database_max_rows","database_timeout_seconds"};
    else if(connector=="outlook_mail_extractor"||connector=="outlook_calendar")native_parameters={"communication_include_reminders","communication_current_time","communication_max_items","communication_max_events","outlook_source","outlook_email_user","outlook_calendar_id"};
    else throw std::invalid_argument("unknown acquisition connector");
    allowed.insert(native_parameters.begin(),native_parameters.end());live_params(request,allowed);
    minera::SourceAcquisitionInput input;input.connector=connector;for(const auto& [key,value]:request.params)if(native_parameters.contains(key))input.parameters.emplace(key,value);
    if((mode=="upload"||mode=="path")&&connector=="windows_events"&&request.has_param("windows_events_source")&&request.get_param_value("windows_events_source")=="service")throw std::invalid_argument("Windows service source requires windows_service mode");
    if(mode=="upload"){if(connector=="github_repo")throw std::invalid_argument("repository acquisition requires API mode");if(request.has_param("source_path"))throw std::invalid_argument("upload acquisition does not accept a server path");input.source_bytes=ocel_request_body(request,"source");}
    else if(mode=="path"){if(connector=="github_repo")throw std::invalid_argument("repository acquisition requires API mode");input.server_path=live_required(request,"source_path");if(!request.body.empty())throw std::invalid_argument("server path acquisition does not accept an upload body");}
    else if(mode=="repository_api"){if(connector!="github_repo"||request.has_param("source_path")||!request.body.empty())throw std::invalid_argument("API acquisition requires a repository and no file/path");}
    else if(mode=="windows_service"){if(connector!="windows_events"||request.has_param("source_path")||!request.body.empty())throw std::invalid_argument("Windows service mode requires a Windows connector and no upload/path");if(input.parameters.contains("windows_events_source")&&input.parameters.at("windows_events_source")!="service")throw std::invalid_argument("Windows service mode requires service source");input.parameters["windows_events_source"]="service";}
    else if(mode=="outlook_com"){if((connector!="outlook_calendar"&&connector!="outlook_mail_extractor")||request.has_param("source_path")||!request.body.empty())throw std::invalid_argument("Outlook COM mode requires an Outlook connector and no upload/path");if(input.parameters.contains("outlook_source")&&input.parameters.at("outlook_source")!="com")throw std::invalid_argument("Outlook COM mode requires com source");input.parameters["outlook_source"]="com";}
    else if(mode=="database"){if((connector!="camunda_workflow"&&connector!="sap_o2c"&&connector!="sap_accounting")||!input.parameters.contains("database_driver")||input.parameters.at("database_driver")!="odbc"||request.has_param("source_path")||!request.body.empty())throw std::invalid_argument("Database connection mode requires an ODBC connector and no file/path");}
    else throw std::invalid_argument("acquisition mode requires upload, path, repository_api or windows_service");
    const auto authorization=request.get_header_value("Authorization");
    if(!authorization.empty()){if(connector!="github_repo"||authorization.rfind("Bearer ",0)!=0||authorization.size()==7)throw std::invalid_argument("repository authentication requires a non-empty bearer header");input.auth_token=authorization.substr(7);}
    const auto connection=request.get_header_value("X-Minera-Database-Connection");if(!connection.empty()){if(mode!="database")throw std::invalid_argument("Database credentials require database connection mode");input.database_connection=connection;}
    if(mode!="database"&&input.parameters.contains("database_driver")&&input.parameters.at("database_driver")=="odbc")throw std::invalid_argument("ODBC source requires database connection mode");
    if(mode!="outlook_com"&&(input.parameters.contains("outlook_source")||input.parameters.contains("outlook_email_user")||input.parameters.contains("outlook_calendar_id")))throw std::invalid_argument("Outlook driver options require Outlook COM mode");
    input.repository_transport=minera::tools::repository_history_http_transport();
    auto record=std::make_shared<AcquiredSourceRecord>();record->id=id;record->configuration=request.params;record->name=request.has_param("source_name")?request.get_param_value("source_name"):connector;record->mode=mode;record->path=input.server_path;record->provided_sha256=request.has_param("source_sha256")?request.get_param_value("source_sha256"):"";record->authenticated=!input.auth_token.empty()||!input.database_connection.empty();record->acquired=minera::acquire_source(input);
    std::shared_ptr<AcquiredSourceRecord> existing;
    {std::lock_guard lock(g_acquired_mutex);auto [it,inserted]=g_acquired_sources.emplace(id,record);if(!inserted)existing=it->second;}
    if(existing){std::lock_guard source_lock(existing->mutex);if(existing->configuration!=record->configuration||existing->acquired.original_bytes!=record->acquired.original_bytes||existing->acquired.database_capture.has_value()!=record->acquired.database_capture.has_value()||(existing->acquired.database_capture&&(existing->acquired.database_capture->raw_table_json!=record->acquired.database_capture->raw_table_json||existing->acquired.database_capture->formatted_table_json!=record->acquired.database_capture->formatted_table_json))||existing->acquired.windows_capture.has_value()!=record->acquired.windows_capture.has_value()||(existing->acquired.windows_capture&&existing->acquired.windows_capture->xml!=record->acquired.windows_capture->xml)||minera::source_acquisition_json(existing->acquired)!=minera::source_acquisition_json(record->acquired))throw minera::StreamingSessionConflict("acquisition source ID already has different inputs or results");record=existing;}
    std::lock_guard lock(record->mutex);response.set_content(acquired_payload(*record),"application/json");
  });
}
void handle_acquired_snapshot(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"source_id"});auto s=acquired_find(request);std::lock_guard lock(s->mutex);response.set_content(acquired_payload(*s),"application/json");});
}
void handle_acquired_original(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"source_id"});auto s=acquired_find(request);std::lock_guard lock(s->mutex);if(!s->acquired.original_bytes)throw std::out_of_range("this API/profile-directory source has no single original upload");response.set_content(*s->acquired.original_bytes,"application/octet-stream");});
}
void handle_acquired_export(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"source_id","format","variant","gzip","preserve_nulls","infer_metadata"});auto s=acquired_find(request);std::lock_guard lock(s->mutex);const auto format=live_required(request,"format");
    if(format=="json"){minera::LogConversionResult value;value.log=s->acquired.log;response.set_content(minera::log_conversion_json(value),"application/json");}
    else if(format=="xes"){minera::XesExportOptions options;if(request.has_param("variant"))options.variant=minera::parse_xes_export_variant(request.get_param_value("variant"));options.compress=ocel_query_bool(request,"gzip",false);options.preserve_nulls=ocel_query_bool(request,"preserve_nulls",true);options.infer_metadata=ocel_query_bool(request,"infer_metadata",false);response.set_content(minera::serialize_xes(s->acquired.log,options),*options.compress?"application/gzip":"application/xml");}
    else throw std::invalid_argument("acquired source export format requires json or xes");
  });
}
void handle_acquired_capture(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"source_id"});auto s=acquired_find(request);std::lock_guard lock(s->mutex);if(s->acquired.outlook_capture)response.set_content(s->acquired.outlook_capture->item_table_json,"application/json");else{if(!s->acquired.windows_capture)throw std::out_of_range("this acquired source has no native item/event capture");response.set_content(s->acquired.windows_capture->xml,"application/xml");}});
}
void handle_acquired_database_table(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"source_id","formatted"});auto s=acquired_find(request);std::lock_guard lock(s->mutex);const bool formatted=ocel_query_bool(request,"formatted",false);if(s->acquired.outlook_capture)response.set_content(formatted?s->acquired.outlook_capture->formatted_table_json:s->acquired.outlook_capture->item_table_json,"application/json");else{if(!s->acquired.database_capture)throw std::out_of_range("this acquired source has no query/item table");response.set_content(formatted?s->acquired.database_capture->formatted_table_json:s->acquired.database_capture->raw_table_json,"application/json");}});
}
void handle_acquired_list(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{});std::vector<std::shared_ptr<AcquiredSourceRecord>> sources;{std::lock_guard lock(g_acquired_mutex);for(const auto& [id,s]:g_acquired_sources){(void)id;sources.push_back(s);}}std::ostringstream out;out<<"{\"sources\":[";bool first=true;for(const auto& s:sources){std::lock_guard lock(s->mutex);if(!first)out<<',';first=false;out<<acquired_payload(*s);}out<<"]}";response.set_content(out.str(),"application/json");});
}
void handle_acquired_release(const httplib::Request& request,httplib::Response& response) {
  live_response(response,[&]{live_params(request,{"source_id"});auto s=acquired_find(request);std::lock_guard lock(s->mutex);auto payload=acquired_payload(*s);payload.pop_back();if(s->acquired.database_capture)payload+=",\"database_tables\":{\"raw\":"+s->acquired.database_capture->raw_table_json+",\"formatted\":"+s->acquired.database_capture->formatted_table_json+"}";payload+=",\"released\":true}";{std::lock_guard registry(g_acquired_mutex);g_acquired_sources.erase(s->id);}response.set_content(payload,"application/json");});
}
