#pragma once
#include "minera/table_attributes_filter_workbench.hpp"
inline void handle_table_attributes_filter(const httplib::Request& request,httplib::Response& response) {
  std::vector<minera::DiscoveryWorkbenchPart> parts,query;
  for(const auto& [name,value]:request.params)query.push_back({name,"",value,false});
  for(const auto& [name,file]:request.files)parts.push_back({name,file.filename,file.content,file.filename_present});
  const auto result=minera::execute_table_attributes_filter(parts,query,
      request.is_multipart_form_data()?"":"table attributes filtering requires multipart source_file upload");
  response.status=result.status;response.set_content(result.json,"application/json");
}
