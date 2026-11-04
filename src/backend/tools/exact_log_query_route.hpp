#pragma once

bool exact_log_query_requested(const httplib::Request& request) {
  return request.is_multipart_form_data() || request.has_param("query_options_json") ||
      request.has_param("query_options_file") || request.has_file("query_options_json") || request.has_file("query_options_file");
}

// Materialize the same actual checked-table selection as the existing export
// adapter, retaining index labels and categorical/metadata storage. This is not
// reconstruction from the projected JSON export.
minera::EventTable exact_query_selected_table(const MinedSession& session,
    const std::vector<std::size_t>& indices,bool cases) {
  if(!session.active_source)
    return minera::event_log_to_table(minera::filter_query_events(require_prepared_log(session),indices,cases));
  auto table=session.active_source->checked_table();
  const auto selected=active_source_expanded_table_indices(session,indices,cases);
  bool identity=selected.size()==table.rows.size();
  for(std::size_t i=0;identity&&i<selected.size();++i)identity=selected[i]==i;
  auto rows=std::move(table.rows);table.rows.clear();
  std::vector<minera::EventTableIndexLabel> labels;
  if(!identity&&!table.index){
    table.index=minera::EventTableIndex{};
    for(std::size_t i=0;i<rows.size();++i){minera::EventAttribute label{"",std::to_string(i),minera::AttributeKind::kInt};label.typed_value=static_cast<std::int64_t>(i);table.index->labels.push_back({std::move(label)});}
  }
  if(!identity&&table.index&&table.index->kind=="RangeIndex")table.index->kind="Index";
  if(table.index){labels=std::move(table.index->labels);table.index->labels.clear();}
  for(auto index:selected){table.rows.push_back(std::move(rows[index]));if(table.index)table.index->labels.push_back(std::move(labels[index]));}
  return table;
}

void handle_exact_log_query(const httplib::Request& request,httplib::Response& response,bool export_requested) {
  minera::ExactLogQueryReport report(export_requested?"export":"query");report.input_order="httplib_key_order";
  const auto q=minera::exact_query_string;
  std::vector<minera::DiscoveryWorkbenchPart> parts,query;
  for(const auto& [name,value]:request.params)query.push_back({name,"",value,false});
  for(const auto& [name,file]:request.files)parts.push_back({name,file.filename,file.content,file.filename_present});
  std::shared_ptr<const MinedSession> session;std::optional<minera::LogQueryTable> table;
  std::string phase="control";
  try {
    auto controls=minera::prepare_exact_log_query(report,parts,query,true,
        request.is_multipart_form_data()?"":"exact log query requires multipart SQL/options controls");
    phase="source";
    session=query_session(request);
    // Multipart source_id must be checked against this exact retained snapshot,
    // even though the legacy query_session helper only reads URL parameters.
    if(controls.source_id!=std::to_string(session->query_source_id))throw std::invalid_argument("the source log changed; refresh the query workspace");
    report.source_id=std::to_string(session->query_source_id);
    std::string original="null",unavailable=q("legacy cached source retained no original File");
    if(session->active_source){const auto& f=session->active_source->file();original="{\"name\":"+q(f.name)+",\"format\":"+q(f.format)+",\"bytes\":"+q(f.bytes)+'}';unavailable="null";}
    report.source_context="{\"id\":"+q(report.source_id)+",\"dataset\":"+q(session->dataset)+",\"original_file\":"+original+",\"original_file_unavailable_reason\":"+unavailable+'}';
    table=cached_source_query_table(*session);
    minera::record_exact_query_source(report,*table);
    if(session->log_ready)minera::record_exact_query_log(report,require_prepared_log(*session));
    std::vector<std::size_t> indices;bool can_filter=false,truncated=false;
    phase="query";
    if(controls.options.engine=="sqlite"){
      const auto result=minera::execute_exact_sqlite_query(report,*table,controls.sql,controls.options);
      indices=result.selected_event_indices;can_filter=result.can_filter;truncated=result.output_truncated;
    }else{
      phase="runtime";
      if(g_query_runtime_url.empty())throw std::invalid_argument("DuckDB requires --query-runtime-url and scripts/query_runtime.py");
      const auto body=minera::exact_duck_query_body(*table,controls.sql,controls.options,report.source_id);
      report.query_call.known=false;
      try {
        const auto result=minera::query_runtime::request(g_query_runtime_url,"/query",&body);
        minera::record_exact_duck_query(report,result->body,result->status);
        // The existing bridge parses a separate minimal identity record. The
        // primary full runtime response has already been preserved above.
        const auto selection=minera::parse_exact_duck_query_selection(result->body,table->table.rows.size());
        can_filter=selection.can_filter;truncated=selection.output_truncated;indices=selection.indices;
      }catch(const std::exception& e){if(report.query_call.error=="null")report.query_call.error=minera::exact_query_error("runtime_error",e.what());throw;}
    }
    minera::record_exact_query_source(report,*table,true);
    if(session->log_ready)minera::record_exact_query_log(report,require_prepared_log(*session),true);
    minera::record_exact_query_options(report,controls.options,true);
    if(export_requested){
      phase="selection";if(!can_filter||truncated)throw std::invalid_argument("filter SQL must return valid event indices for a complete selection");
      std::string selected="[";for(auto i:indices){if(selected.size()>1)selected+=',';selected+=std::to_string(i);}selected+=']';
      report.selection="{\"indices\":"+selected+",\"mode\":"+q(controls.mode)+",\"complete\":true}";
      phase="filter";report.filter_call.entered=true;
      try{
        const auto selected_table=exact_query_selected_table(*session,indices,controls.mode=="cases");
        minera::record_exact_query_filtered(report,selected_table);
        std::optional<minera::EventLog> selected_log;
        if(session->log_ready){selected_log=select_active_source_rows(*session,indices,controls.mode=="cases");minera::record_exact_query_filtered(report,*selected_log);}
        report.filter_call.returned=true;
        phase="export";report.export_call.entered=true;
        try{
          if(controls.format=="json"){
            const auto bytes=minera::serialize_event_table_input_json(selected_table);
            minera::record_exact_query_output(report,"json","application/json",bytes);
          }else{
            if(!selected_log)throw std::invalid_argument("the active source has no prepared EventLog for XES export");
            minera::XesExportOptions options;  // nulls omitted, as pm4py writes None/NaN
            const auto bytes=minera::serialize_xes(*selected_log,options);
            minera::record_exact_query_output(report,"xes","application/xml",bytes);
          }
          report.export_call.returned=true;
        }catch(const std::exception& e){report.export_call.error=minera::exact_query_error(minera::exact_query_exception_type(e),e.what());throw;}
      }catch(const std::exception& e){if(!report.filter_call.returned)report.filter_call.error=minera::exact_query_error(minera::exact_query_exception_type(e),e.what());throw;}
    }
  }catch(const std::exception& e){
    if(table)minera::record_exact_query_source(report,*table,true);
    if(session&&session->log_ready)minera::record_exact_query_log(report,require_prepared_log(*session),true);
    report.fail(phase,e.what(),minera::exact_query_exception_type(e));
  }
  response.status=report.status;response.set_content(report.json(),"application/json");
}
