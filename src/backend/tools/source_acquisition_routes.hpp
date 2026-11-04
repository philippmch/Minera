// Included after the shared live request helpers, inside the server namespace.
struct AcquiredSourceRecord {
  std::mutex mutex;
  std::string id,name,mode,path,provided_sha256;
  httplib::Params configuration;
  minera::SourceAcquisitionResult acquired;
  bool authenticated=false;
  std::vector<minera::DiscoveryWorkbenchPart> acquisition_parts, acquisition_query;
  std::optional<std::string> uploaded_filename;
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

// Exact acquisition is opt-in. The old handlers below retain their original
// parsing and serialization path when the new controls are absent.
std::string acquired_safe(std::string_view value) {
  return minera::serialize_embedding_workbench_string_json(value);
}
struct AcquiredExactRequest {
  httplib::Request normalized;
  std::vector<minera::DiscoveryWorkbenchPart> parts, query;
  std::optional<minera::DiscoveryWorkbenchPart> source;
};
const std::set<std::string>& acquired_public_fields() {
  static const std::set<std::string> fields = {
    "source_id", "connector", "mode", "source_name", "source_path", "source_sha256", "source", "exact_result",
    "browser_history_max_events", "windows_events_max_events", "windows_events_source", "windows_events_channel",
    "windows_events_query", "windows_events_reverse", "windows_events_timeout_ms", "repository_owner", "repository_name",
    "repository_api_base_url", "repository_max_pages", "repository_max_issues", "repository_max_events", "workflow_max_events",
    "database_driver", "database_dialect", "database_prefix", "database_max_rows", "database_timeout_seconds",
    "enterprise_max_events", "communication_include_reminders", "communication_current_time", "communication_max_items",
    "communication_max_events", "outlook_source", "outlook_email_user", "outlook_calendar_id", "format", "response", "variant",
    "gzip", "name", "preserve_nulls", "infer_metadata", "formatted", "xes_export_options_json", "xes_export_options_file"
  };
  return fields;
}
bool acquired_exact_requested(const httplib::Request& request) {
  return request.has_param("exact_result") || request.has_file("exact_result");
}
AcquiredExactRequest acquired_collect(const httplib::Request& request) {
  AcquiredExactRequest exact;
  exact.normalized.method = request.method;
  exact.normalized.path = request.path;
  for (const auto& [name, value] : request.params) {
    // Unsupported fields cannot be a credential-reporting back door.
    const auto bytes = acquired_public_fields().contains(name) ? value : std::string{};
    exact.query.push_back({name, {}, bytes, false});
    exact.normalized.params.emplace(name, value);
  }
  for (const auto& [name, part] : request.files) {
    const auto bytes = acquired_public_fields().contains(name) ? part.content : std::string{};
    exact.parts.push_back({name, part.filename, bytes, part.filename_present});
    if (name == "source") exact.source = exact.parts.back();
    else exact.normalized.params.emplace(name, part.content);
  }
  if (!request.is_multipart_form_data() && !request.body.empty()) {
    exact.parts.push_back({"source", {}, request.body, false});
    exact.source = exact.parts.back();
  }
  return exact;
}
void acquired_exact_validate(const AcquiredExactRequest& exact, const std::set<std::string>& allowed) {
  std::set<std::string> names;
  for (const auto* parts : {&exact.query, &exact.parts}) for (const auto& part : *parts) {
    if (!allowed.contains(part.name)) throw std::invalid_argument("unknown exact acquisition field: " + part.name);
    if (!names.insert(part.name).second) throw std::invalid_argument("duplicate or conflicting exact acquisition field: " + part.name);
  }
  for (const auto& part : exact.query) if (part.name == "source")
    throw std::invalid_argument("source must be supplied as an upload body or actual File");
  const auto& request = exact.normalized;
  if (request.has_param("exact_result") && request.get_param_value("exact_result") != "true")
    throw std::invalid_argument("exact_result requires true");
}
std::string acquired_parts_json(const std::vector<minera::DiscoveryWorkbenchPart>& parts, bool query) {
  std::string out = "[";
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i) out += ',';
    const auto& part = parts[i];
    out += "{\"field\":" + acquired_safe(part.name) + ",\"bytes\":" + acquired_safe(part.bytes);
    if (!query) out += ",\"filename\":" + (part.file ? acquired_safe(part.filename) : "null") +
        ",\"file\":" + (part.file ? "true" : "false");
    out += '}';
  }
  return out + ']';
}
std::string acquired_provenance(const AcquiredExactRequest& request) {
  return "\"original_inputs\":" + acquired_parts_json(request.parts, false) +
      ",\"query_inputs\":" + acquired_parts_json(request.query, true) + ",\"input_order\":\"httplib_key_order\"";
}
struct AcquiredExactState {
  std::string operation = "acquire", phase = "parse", error, native_error;
  int status = 200;
  bool entered = false, returned = false, released = false;
  std::shared_ptr<AcquiredSourceRecord> record;
};
std::string acquired_call_json(bool entered, bool returned, const std::string& error) {
  return std::string("{\"entered\":") + (entered ? "true" : "false") +
      ",\"returned\":" + (returned ? "true" : "false") +
      ",\"error\":" + (error.empty() ? "null" : acquired_safe(error)) + '}';
}
std::string acquired_error_json(const std::string& phase, const std::string& error) {
  return error.empty() ? "null" : "{\"phase\":" + acquired_safe(phase) + ",\"message\":" + acquired_safe(error) + '}';
}
std::string acquired_exact_payload(const AcquiredExactState& state, const AcquiredExactRequest& request) {
  const auto& record = state.record;
  const bool has_result = record && (state.operation != "acquire" || state.returned);
  std::string out = "{\"family\":\"EXACT_SOURCE_ACQUISITION\",\"operation\":" + acquired_safe(state.operation) +
      ",\"status\":" + std::to_string(state.status) + ",\"source_id\":" +
      (record ? acquired_safe(record->id) : request.normalized.has_param("source_id") ?
        acquired_safe(request.normalized.get_param_value("source_id")) : "null") + ',' + acquired_provenance(request) +
      ",\"error\":" + acquired_error_json(state.phase, state.error) +
      ",\"native_call\":" + acquired_call_json(state.entered, state.returned, state.native_error);
  if (!record) out += ",\"source\":null,\"configuration\":[],\"acquisition_inputs\":{\"original_inputs\":[],\"query_inputs\":[]}";
  else {
    const auto& value = record->acquired;
    out += ",\"source\":{\"name\":" + acquired_safe(record->name) + ",\"mode\":" + acquired_safe(record->mode) +
        ",\"path\":" + acquired_safe(record->path) + ",\"has_original\":" + (has_result && value.original_bytes ? "true" : "false") +
        ",\"size\":" + (has_result && value.original_bytes ? std::to_string(value.original_bytes->size()) : "null") +
        ",\"provided_sha256\":" + acquired_safe(record->provided_sha256) +
        ",\"authenticated\":" + (record->authenticated ? "true" : "false") +
        ",\"uploaded_filename\":" + (record->uploaded_filename ? acquired_safe(*record->uploaded_filename) : "null") + "},\"configuration\":[";
    bool first = true;
    for (const auto& [key, value] : record->configuration) {
      if (!first) out += ',';
      first = false;
      out += "{\"key\":" + acquired_safe(key) + ",\"value\":" + acquired_safe(value) + '}';
    }
    out += "],\"acquisition_inputs\":{\"original_inputs\":" + acquired_parts_json(record->acquisition_parts, false) +
        ",\"query_inputs\":" + acquired_parts_json(record->acquisition_query, true) + '}';
  }
  out += ",\"result\":" + (has_result ? minera::source_acquisition_result_exact_json(record->acquired) : "null") +
      ",\"artifacts\":" + (has_result ? minera::source_acquisition_artifacts_json(record->acquired) : "{}") +
      ",\"released\":" + (state.released ? "true" : "false") + '}';
  return out;
}
// Refuses a new source ID when the registry is full (see kLiveMaxSessions).
// Callers hold g_acquired_mutex.
void acquired_capacity_locked(const std::string& id) {
  if (!g_acquired_sources.contains(id) && g_acquired_sources.size() >= kLiveMaxSessions)
    throw LiveCapacityError(429, "too many acquired sources (" + std::to_string(kLiveMaxSessions) + "); release one before acquiring another");
}
void acquired_capacity(const std::string& id) { std::lock_guard lock(g_acquired_mutex); acquired_capacity_locked(id); }
void acquired_exact_failure(AcquiredExactState& state, const std::exception& error) {
  state.error = error.what();
  if (const auto* capacity = dynamic_cast<const LiveCapacityError*>(&error)) state.status = capacity->status;
  else if (dynamic_cast<const minera::StreamingSessionConflict*>(&error)) state.status = 409;
  else if (dynamic_cast<const std::out_of_range*>(&error) && !(state.entered && !state.returned)) state.status = 404;
  else state.status = 400;
}
std::set<std::string> acquired_native_parameters(const std::string& connector) {
  if (connector == "chrome_history" || connector == "firefox_history") return {"browser_history_max_events"};
  if (connector == "windows_events") return {"windows_events_max_events", "windows_events_source", "windows_events_channel", "windows_events_query", "windows_events_reverse", "windows_events_timeout_ms"};
  if (connector == "github_repo") return {"repository_owner", "repository_name", "repository_api_base_url", "repository_max_pages", "repository_max_issues", "repository_max_events"};
  if (connector == "camunda_workflow") return {"workflow_max_events", "database_driver", "database_dialect", "database_prefix", "database_max_rows", "database_timeout_seconds"};
  if (connector == "sap_o2c" || connector == "sap_accounting") return {"enterprise_max_events", "database_driver", "database_dialect", "database_prefix", "database_max_rows", "database_timeout_seconds"};
  if (connector == "outlook_mail_extractor" || connector == "outlook_calendar") return {"communication_include_reminders", "communication_current_time", "communication_max_items", "communication_max_events", "outlook_source", "outlook_email_user", "outlook_calendar_id"};
  throw std::invalid_argument("unknown acquisition connector");
}
void handle_acquire_source_exact_using(const httplib::Request& provided, httplib::Response& response,
    minera::SourceAcquisitionResult (*acquire)(const minera::SourceAcquisitionInput&)) {
  auto exact = acquired_collect(provided);
  AcquiredExactState state;
  try {
    const auto& request = exact.normalized;
    const auto connector = live_required(request, "connector");
    const auto parameters = acquired_native_parameters(connector);
    std::set<std::string> allowed = {"source_id", "connector", "mode", "source_name", "source_path", "source_sha256", "source", "exact_result"};
    allowed.insert(parameters.begin(), parameters.end());
    acquired_exact_validate(exact, allowed);
    const auto mode = live_required(request, "mode"), id = live_required(request, "source_id");
    live_id(id);
    minera::SourceAcquisitionInput input;
    input.connector = connector;
    for (const auto& [key, value] : request.params) if (parameters.contains(key)) input.parameters.emplace(key, value);
    if ((mode == "upload" || mode == "path") && connector == "windows_events" &&
        request.has_param("windows_events_source") && request.get_param_value("windows_events_source") == "service")
      throw std::invalid_argument("Windows service source requires windows_service mode");
    if (mode == "upload") {
      if (connector == "github_repo") throw std::invalid_argument("repository acquisition requires API mode");
      if (request.has_param("source_path")) throw std::invalid_argument("upload acquisition does not accept a server path");
      if (provided.is_multipart_form_data() && (!exact.source || !exact.source->file))
        throw std::invalid_argument("source requires an actual File with filename presence");
      input.source_bytes = exact.source ? exact.source->bytes : std::string{};
    } else {
      if (exact.source) throw std::invalid_argument("non-upload acquisition does not accept a source body");
      if (mode == "path") {
        if (connector == "github_repo") throw std::invalid_argument("repository acquisition requires API mode");
        input.server_path = live_required(request, "source_path");
      } else if (mode == "repository_api") {
        if (connector != "github_repo" || request.has_param("source_path")) throw std::invalid_argument("API acquisition requires a repository and no file/path");
      } else if (mode == "windows_service") {
        if (connector != "windows_events" || request.has_param("source_path")) throw std::invalid_argument("Windows service mode requires a Windows connector and no upload/path");
        if (input.parameters.contains("windows_events_source") && input.parameters.at("windows_events_source") != "service") throw std::invalid_argument("Windows service mode requires service source");
        input.parameters["windows_events_source"] = "service";
      } else if (mode == "outlook_com") {
        if ((connector != "outlook_calendar" && connector != "outlook_mail_extractor") || request.has_param("source_path")) throw std::invalid_argument("Outlook COM mode requires an Outlook connector and no upload/path");
        if (input.parameters.contains("outlook_source") && input.parameters.at("outlook_source") != "com") throw std::invalid_argument("Outlook COM mode requires com source");
        input.parameters["outlook_source"] = "com";
      } else if (mode == "database") {
        if ((connector != "camunda_workflow" && connector != "sap_o2c" && connector != "sap_accounting") ||
            !input.parameters.contains("database_driver") || input.parameters.at("database_driver") != "odbc" || request.has_param("source_path"))
          throw std::invalid_argument("Database connection mode requires an ODBC connector and no file/path");
      } else throw std::invalid_argument("acquisition mode requires upload, path, repository_api, windows_service, database or outlook_com");
    }
    const auto authorization = provided.get_header_value("Authorization");
    if (!authorization.empty()) {
      if (connector != "github_repo" || authorization.rfind("Bearer ", 0) != 0 || authorization.size() == 7)
        throw std::invalid_argument("repository authentication requires a non-empty bearer header");
      input.auth_token = authorization.substr(7);
    }
    const auto connection = provided.get_header_value("X-Minera-Database-Connection");
    if (!connection.empty()) {
      if (mode != "database") throw std::invalid_argument("Database credentials require database connection mode");
      input.database_connection = connection;
    }
    if (mode != "database" && input.parameters.contains("database_driver") && input.parameters.at("database_driver") == "odbc")
      throw std::invalid_argument("ODBC source requires database connection mode");
    if (mode != "outlook_com" && (input.parameters.contains("outlook_source") || input.parameters.contains("outlook_email_user") || input.parameters.contains("outlook_calendar_id")))
      throw std::invalid_argument("Outlook driver options require Outlook COM mode");
    input.repository_transport = minera::tools::repository_history_http_transport();
    auto record = std::make_shared<AcquiredSourceRecord>();
    state.record = record;
    record->id = id;
    record->configuration = request.params;
    record->configuration.erase("exact_result");
    record->name = request.has_param("source_name") ? request.get_param_value("source_name") : connector;
    record->mode = mode;
    record->path = input.server_path;
    record->provided_sha256 = request.has_param("source_sha256") ? request.get_param_value("source_sha256") : "";
    record->authenticated = !input.auth_token.empty() || !input.database_connection.empty();
    record->acquisition_parts = exact.parts;
    record->acquisition_query = exact.query;
    if (exact.source && exact.source->file) record->uploaded_filename = exact.source->filename;
    state.phase = "native";
    state.entered = true;
    acquired_capacity(id);
    try { record->acquired = acquire(input); state.returned = true; }
    catch (const std::exception& error) { state.native_error = error.what(); throw; }
    state.phase = "registry";
    std::shared_ptr<AcquiredSourceRecord> existing;
    {
      std::lock_guard lock(g_acquired_mutex);
      acquired_capacity_locked(id);
      const auto [it, inserted] = g_acquired_sources.emplace(id, record);
      if (!inserted) existing = it->second;
    }
    if (existing) {
      std::lock_guard lock(existing->mutex);
      if (existing->configuration != record->configuration ||
          minera::source_acquisition_result_exact_json(existing->acquired) != minera::source_acquisition_result_exact_json(record->acquired))
        throw minera::StreamingSessionConflict("acquisition source ID already has different inputs or results");
      state.record = existing;
    }
  } catch (const std::exception& error) { acquired_exact_failure(state, error); }
  response.status = state.status;
  if (state.record) {
    std::lock_guard lock(state.record->mutex);
    response.set_content(acquired_exact_payload(state, exact), "application/json");
  } else response.set_content(acquired_exact_payload(state, exact), "application/json");
}
void handle_acquired_exact_snapshot(const httplib::Request& provided, httplib::Response& response, bool release) {
  auto exact = acquired_collect(provided);
  AcquiredExactState state;
  state.operation = release ? "release" : "snapshot";
  try {
    acquired_exact_validate(exact, {"source_id", "exact_result"});
    state.phase = "registry";
    state.record = acquired_find(exact.normalized);
    std::lock_guard lock(state.record->mutex);
    state.released = release;
    const auto payload = acquired_exact_payload(state, exact);
    if (release) { std::lock_guard registry(g_acquired_mutex); g_acquired_sources.erase(state.record->id); }
    response.status = 200;
    response.set_content(payload, "application/json");
    return;
  } catch (const std::exception& error) { acquired_exact_failure(state, error); }
  response.status = state.status;
  response.set_content(acquired_exact_payload(state, exact), "application/json");
}
void handle_acquired_exact_list(const httplib::Request& provided, httplib::Response& response) {
  auto exact = acquired_collect(provided);
  AcquiredExactState state;
  state.operation = "list";
  std::string sources = "[";
  try {
    acquired_exact_validate(exact, {"exact_result"});
    std::vector<std::shared_ptr<AcquiredSourceRecord>> records;
    { std::lock_guard lock(g_acquired_mutex); for (const auto& [id, value] : g_acquired_sources) { (void)id; records.push_back(value); } }
    for (std::size_t i = 0; i < records.size(); ++i) {
      if (i) sources += ',';
      AcquiredExactState snapshot;
      snapshot.operation = "snapshot";
      snapshot.record = records[i];
      std::lock_guard lock(records[i]->mutex);
      sources += acquired_exact_payload(snapshot, exact);
    }
  } catch (const std::exception& error) { acquired_exact_failure(state, error); }
  sources += ']';
  response.status = state.status;
  response.set_content("{\"family\":\"EXACT_SOURCE_ACQUISITION_LIST\",\"operation\":\"list\",\"status\":" +
      std::to_string(state.status) + ",\"source_id\":null," + acquired_provenance(exact) +
      ",\"error\":" + acquired_error_json(state.phase, state.error) +
      ",\"native_call\":" + acquired_call_json(false, false, {}) + ",\"sources\":" + sources + '}', "application/json");
}
void handle_acquired_exact_export(const httplib::Request& provided, httplib::Response& response,
    const std::string& operation = "export") {
  auto exact = acquired_collect(provided);
  AcquiredExactState state;
  state.operation = operation;
  std::string format = operation == "export" ? "" : operation;
  std::string before = "null", after = "null", options_before = "null", options_after = "null";
  // Recognize an unambiguous format before validating other controls, so
  // every owned parse failure has a truthful nullable format discriminator.
  if (operation == "export" && exact.normalized.get_param_value_count("format") == 1) {
    const auto supplied = exact.normalized.get_param_value("format");
    if (supplied == "xes" || supplied == "json") format = supplied;
  }
  std::string output = "null", result = "null", legacy_artifact = "{\"bytes\":null,\"error\":null}";
  try {
    std::set<std::string> allowed = {"source_id", "exact_result"};
    if (operation == "export") allowed.insert({"format", "response", "variant", "gzip", "name", "preserve_nulls", "infer_metadata", "xes_export_options_json", "xes_export_options_file"});
    if (operation == "table") allowed.insert("formatted");
    acquired_exact_validate(exact, allowed);
    const auto& request = exact.normalized;
    if (operation == "export") {
      if (provided.method == "POST" && (!request.has_param("response") || request.get_param_value("response") != "report"))
        throw std::invalid_argument("exact export POST requires response=report");
      if (request.has_param("response") && request.get_param_value("response") != "report")
        throw std::invalid_argument("exact export response requires report");
      const auto supplied = live_required(request, "format");
      if (supplied != "xes" && supplied != "json")
        throw std::invalid_argument("acquired source export format requires json or xes");
      format = supplied;
    }
    const bool has_json = request.has_param("xes_export_options_json");
    const bool has_file = request.has_param("xes_export_options_file");
    if (has_json && has_file) throw std::invalid_argument("XES options JSON and File conflict by presence");
    if (format != "xes") for (const auto* key : {"variant", "gzip", "name", "preserve_nulls", "infer_metadata", "xes_export_options_json", "xes_export_options_file"})
      if (request.has_param(key)) throw std::invalid_argument("XES options require format=xes");
    std::optional<minera::XesExportOptions> options;
    if (format == "xes") {
      if (has_json || has_file) {
        for (const auto* key : {"variant", "gzip", "name", "preserve_nulls", "infer_metadata"})
          if (request.has_param(key)) throw std::invalid_argument("full XES options conflict with scalar controls by presence");
        if (has_file) {
          const auto it = std::find_if(exact.parts.begin(), exact.parts.end(), [](const auto& part) { return part.name == "xes_export_options_file"; });
          if (it == exact.parts.end() || !it->file) throw std::invalid_argument("xes_export_options_file requires an actual File with filename presence");
        }
        options = minera::parse_source_acquisition_xes_options_json(request.get_param_value(has_file ? "xes_export_options_file" : "xes_export_options_json"));
      } else {
        options.emplace();
        options->compress = ocel_query_bool(request, "gzip", false);
        options->preserve_nulls = ocel_query_bool(request, "preserve_nulls", false);
        options->infer_metadata = ocel_query_bool(request, "infer_metadata", false);
        if (request.has_param("variant")) options->variant = minera::parse_xes_export_variant(request.get_param_value("variant"));
        if (request.has_param("name")) options->name = request.get_param_value("name");
      }
      options_before = options_after = minera::source_acquisition_xes_options_json(*options);
    }
    state.phase = "registry";
    state.record = acquired_find(request);
    std::lock_guard lock(state.record->mutex);
    const auto& value = state.record->acquired;
    before = minera::exact_source_snapshot(minera::EventDataView{value.log});
    after = before;
    std::string bytes, mime;
    state.phase = "export";
    if (format == "xes" || format == "json") {
      state.entered = true;
      try {
        if (format == "xes") {
          bytes = minera::serialize_xes(value.log, *options);
          mime = options->compress.value_or(false) ? "application/gzip" : "application/xml";
        } else {
          bytes = minera::serialize_embedding_workbench_raw_log_json(value.log);
          result = "{\"type\":\"EventLog\",\"value\":" + bytes + '}';
          mime = "application/json";
        }
        state.returned = true;
      } catch (const std::exception& error) {
        state.native_error = error.what();
        after = minera::exact_source_snapshot(minera::EventDataView{value.log});
        if (options) options_after = minera::source_acquisition_xes_options_json(*options);
        throw;
      }
      after = minera::exact_source_snapshot(minera::EventDataView{value.log});
      if (options) options_after = minera::source_acquisition_xes_options_json(*options);
      if (format == "json") {
        try { minera::LogConversionResult converted; converted.log = value.log;
          legacy_artifact = "{\"bytes\":" + acquired_safe(minera::log_conversion_json(converted)) + ",\"error\":null}";
        } catch (const std::exception& error) { legacy_artifact = "{\"bytes\":null,\"error\":" + acquired_safe(error.what()) + '}'; }
      }
    } else if (operation == "original") {
      if (!value.original_bytes) throw std::out_of_range("this API/profile-directory source has no single original upload");
      bytes = *value.original_bytes; mime = "application/octet-stream";
    } else if (operation == "capture") {
      if (value.outlook_capture) { bytes = value.outlook_capture->item_table_json; mime = "application/json"; }
      else if (value.windows_capture) { bytes = value.windows_capture->xml; mime = "application/xml"; }
      else throw std::out_of_range("this acquired source has no native item/event capture");
    } else if (operation == "table") {
      const bool formatted = ocel_query_bool(request, "formatted", false);
      if (value.outlook_capture) bytes = formatted ? value.outlook_capture->formatted_table_json : value.outlook_capture->item_table_json;
      else if (value.database_capture) bytes = formatted ? value.database_capture->formatted_table_json : value.database_capture->raw_table_json;
      else throw std::out_of_range("this acquired source has no query/item table");
      mime = "application/json";
    }
    output = "{\"bytes\":" + acquired_safe(bytes) + ",\"mime_type\":" + acquired_safe(mime) + ",\"actual_path\":null}";
  } catch (const std::exception& error) { acquired_exact_failure(state, error); }
  response.status = state.status;
  response.set_content("{\"family\":\"EXACT_SOURCE_EXPORT\",\"operation\":" + acquired_safe(operation) +
      ",\"status\":" + std::to_string(state.status) + ",\"source_id\":" +
      (exact.normalized.has_param("source_id") ? acquired_safe(exact.normalized.get_param_value("source_id")) : "null") +
      ",\"format\":" + (format.empty() ? "null" : acquired_safe(format)) + ',' + acquired_provenance(exact) +
      ",\"error\":" + acquired_error_json(state.phase, state.error) +
      ",\"native_call\":" + acquired_call_json(state.entered, state.returned, state.native_error) +
      ",\"source_before\":" + before + ",\"source_after\":" + after +
      ",\"source_unchanged\":" + (before == "null" || after == "null" ? "null" : before == after ? "true" : "false") +
      ",\"options_before\":" + options_before + ",\"options_after\":" + options_after +
      ",\"options_unchanged\":" + (options_before == "null" || options_after == "null" ? "null" : options_before == options_after ? "true" : "false") +
      ",\"output\":" + output + ",\"result\":" + result + ",\"artifacts\":{\"legacy_log_json\":" + legacy_artifact + "}}", "application/json");
}

void handle_acquire_source_using(const httplib::Request& request,httplib::Response& response,
    minera::SourceAcquisitionResult (*acquire)(const minera::SourceAcquisitionInput&)) {
  if (acquired_exact_requested(request)) { handle_acquire_source_exact_using(request, response, acquire); return; }
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
    else throw std::invalid_argument("acquisition mode requires upload, path, repository_api, windows_service, database or outlook_com");
    const auto authorization=request.get_header_value("Authorization");
    if(!authorization.empty()){if(connector!="github_repo"||authorization.rfind("Bearer ",0)!=0||authorization.size()==7)throw std::invalid_argument("repository authentication requires a non-empty bearer header");input.auth_token=authorization.substr(7);}
    const auto connection=request.get_header_value("X-Minera-Database-Connection");if(!connection.empty()){if(mode!="database")throw std::invalid_argument("Database credentials require database connection mode");input.database_connection=connection;}
    if(mode!="database"&&input.parameters.contains("database_driver")&&input.parameters.at("database_driver")=="odbc")throw std::invalid_argument("ODBC source requires database connection mode");
    if(mode!="outlook_com"&&(input.parameters.contains("outlook_source")||input.parameters.contains("outlook_email_user")||input.parameters.contains("outlook_calendar_id")))throw std::invalid_argument("Outlook driver options require Outlook COM mode");
    input.repository_transport=minera::tools::repository_history_http_transport();
    auto record=std::make_shared<AcquiredSourceRecord>();
    record->id=id;record->configuration=request.params;
    record->name=request.has_param("source_name")?request.get_param_value("source_name"):connector;
    record->mode=mode;record->path=input.server_path;
    record->provided_sha256=request.has_param("source_sha256")?request.get_param_value("source_sha256"):"";
    record->authenticated=!input.auth_token.empty()||!input.database_connection.empty();
    { const auto original = acquired_collect(request); record->acquisition_parts = original.parts; record->acquisition_query = original.query;
      if (original.source && original.source->file) record->uploaded_filename = original.source->filename; }
    acquired_capacity(id);record->acquired=acquire(input);
    std::shared_ptr<AcquiredSourceRecord> existing;
    {std::lock_guard lock(g_acquired_mutex);acquired_capacity_locked(id);auto [it,inserted]=g_acquired_sources.emplace(id,record);if(!inserted)existing=it->second;}
    if(existing){std::lock_guard source_lock(existing->mutex);if(existing->configuration!=record->configuration||existing->acquired.original_bytes!=record->acquired.original_bytes||existing->acquired.database_capture.has_value()!=record->acquired.database_capture.has_value()||(existing->acquired.database_capture&&(existing->acquired.database_capture->raw_table_json!=record->acquired.database_capture->raw_table_json||existing->acquired.database_capture->formatted_table_json!=record->acquired.database_capture->formatted_table_json))||existing->acquired.windows_capture.has_value()!=record->acquired.windows_capture.has_value()||(existing->acquired.windows_capture&&existing->acquired.windows_capture->xml!=record->acquired.windows_capture->xml)||minera::source_acquisition_json(existing->acquired)!=minera::source_acquisition_json(record->acquired))throw minera::StreamingSessionConflict("acquisition source ID already has different inputs or results");record=existing;}
    std::lock_guard lock(record->mutex);response.set_content(acquired_payload(*record),"application/json");
  });
}
void handle_acquire_source(const httplib::Request& request,httplib::Response& response) {
  handle_acquire_source_using(request,response,minera::acquire_source);
}
void handle_acquired_snapshot(const httplib::Request& request,httplib::Response& response) {
  if (acquired_exact_requested(request)) { handle_acquired_exact_snapshot(request, response, false); return; }
  live_response(response,[&]{live_params(request,{"source_id"});auto s=acquired_find(request);std::lock_guard lock(s->mutex);response.set_content(acquired_payload(*s),"application/json");});
}
void handle_acquired_original(const httplib::Request& request,httplib::Response& response) {
  if (acquired_exact_requested(request)) { handle_acquired_exact_export(request, response, "original"); return; }
  live_response(response,[&]{live_params(request,{"source_id"});auto s=acquired_find(request);std::lock_guard lock(s->mutex);if(!s->acquired.original_bytes)throw std::out_of_range("this API/profile-directory source has no single original upload");response.set_content(*s->acquired.original_bytes,"application/octet-stream");});
}
void handle_acquired_export(const httplib::Request& request,httplib::Response& response) {
  if (request.method == "POST" || acquired_exact_requested(request)) { handle_acquired_exact_export(request, response); return; }
  live_response(response,[&]{live_params(request,{"source_id","format","variant","gzip","name","preserve_nulls","infer_metadata"});auto s=acquired_find(request);std::lock_guard lock(s->mutex);const auto format=live_required(request,"format");
    if(format=="json"){minera::LogConversionResult value;value.log=s->acquired.log;response.set_content(minera::log_conversion_json(value),"application/json");}
    else if(format=="xes"){
      minera::XesExportOptions options;
      if(request.has_param("variant"))options.variant=minera::parse_xes_export_variant(request.get_param_value("variant"));
      options.compress=ocel_query_bool(request,"gzip",false);
      options.preserve_nulls=ocel_query_bool(request,"preserve_nulls",false);
      options.infer_metadata=ocel_query_bool(request,"infer_metadata",false);
      if(request.has_param("name"))options.name=request.get_param_value("name");
      response.set_content(minera::serialize_xes(s->acquired.log,options),*options.compress?"application/gzip":"application/xml");
    }
    else throw std::invalid_argument("acquired source export format requires json or xes");
  });
}
void handle_acquired_capture(const httplib::Request& request,httplib::Response& response) {
  if (acquired_exact_requested(request)) { handle_acquired_exact_export(request, response, "capture"); return; }
  live_response(response,[&]{live_params(request,{"source_id"});auto s=acquired_find(request);std::lock_guard lock(s->mutex);if(s->acquired.outlook_capture)response.set_content(s->acquired.outlook_capture->item_table_json,"application/json");else{if(!s->acquired.windows_capture)throw std::out_of_range("this acquired source has no native item/event capture");response.set_content(s->acquired.windows_capture->xml,"application/xml");}});
}
void handle_acquired_database_table(const httplib::Request& request,httplib::Response& response) {
  if (acquired_exact_requested(request)) { handle_acquired_exact_export(request, response, "table"); return; }
  live_response(response,[&]{live_params(request,{"source_id","formatted"});auto s=acquired_find(request);std::lock_guard lock(s->mutex);const bool formatted=ocel_query_bool(request,"formatted",false);if(s->acquired.outlook_capture)response.set_content(formatted?s->acquired.outlook_capture->formatted_table_json:s->acquired.outlook_capture->item_table_json,"application/json");else{if(!s->acquired.database_capture)throw std::out_of_range("this acquired source has no query/item table");response.set_content(formatted?s->acquired.database_capture->formatted_table_json:s->acquired.database_capture->raw_table_json,"application/json");}});
}
void handle_acquired_list(const httplib::Request& request,httplib::Response& response) {
  if (acquired_exact_requested(request)) { handle_acquired_exact_list(request, response); return; }
  live_response(response,[&]{live_params(request,{});std::vector<std::shared_ptr<AcquiredSourceRecord>> sources;{std::lock_guard lock(g_acquired_mutex);for(const auto& [id,s]:g_acquired_sources){(void)id;sources.push_back(s);}}std::ostringstream out;out<<"{\"sources\":[";bool first=true;for(const auto& s:sources){std::lock_guard lock(s->mutex);if(!first)out<<',';first=false;out<<acquired_payload(*s);}out<<"]}";response.set_content(out.str(),"application/json");});
}
void handle_acquired_release(const httplib::Request& request,httplib::Response& response) {
  if (acquired_exact_requested(request)) { handle_acquired_exact_snapshot(request, response, true); return; }
  live_response(response,[&]{live_params(request,{"source_id"});auto s=acquired_find(request);std::lock_guard lock(s->mutex);auto payload=acquired_payload(*s);payload.pop_back();if(s->acquired.database_capture)payload+=",\"database_tables\":{\"raw\":"+s->acquired.database_capture->raw_table_json+",\"formatted\":"+s->acquired.database_capture->formatted_table_json+"}";payload+=",\"released\":true}";{std::lock_guard registry(g_acquired_mutex);g_acquired_sources.erase(s->id);}response.set_content(payload,"application/json");});
}
