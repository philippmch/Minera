#pragma once

// Existing cached query/export handlers keep their original contracts.
void handle_original_log_query_workbench(const httplib::Request& request,
                                          httplib::Response& response) {
  std::vector<minera::DiscoveryWorkbenchPart> parts,query;
  for (const auto& [name,value] : request.params)
    query.push_back({name,"",value,false});
  for (const auto& [name,file] : request.files)
    parts.push_back({name,file.filename,file.content,file.filename_present});
  const auto result=minera::execute_original_log_query_workbench(parts,query,
      "httplib_key_order",request.is_multipart_form_data()?"":
          "original log query requires multipart original Files");
  response.status=result.status;
  response.set_content(result.json,"application/json");
}
