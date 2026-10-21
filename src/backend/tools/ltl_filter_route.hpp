#pragma once
#include "minera/ltl_filter_workbench.hpp"
inline void handle_reference_ltl_filter(const httplib::Request& request,httplib::Response& response) {
  std::vector<minera::DiscoveryWorkbenchPart> parts,query;
  for(const auto& [name,value]:request.params)query.push_back({name,"",value,false});
  for(const auto& [name,file]:request.files)parts.push_back({name,file.filename,file.content,file.filename_present});
  const auto result=minera::execute_ltl_filter_workbench(parts,query,
      request.is_multipart_form_data()?"":"relationship filtering requires multipart controls and source File upload");
  response.status=result.status;response.set_content(result.json,"application/json");
}
