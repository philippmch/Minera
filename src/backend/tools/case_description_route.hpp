#pragma once
#include "minera/case_description_workbench_json.hpp"
#include <set>
bool request_uses_supplied_case_context(const httplib::Request& request){
 static const std::set<std::string> names={"case_file","case_options_json","case_options_file"};
 for(const auto& [name,value]:request.params){(void)value;if(names.contains(name))return true;}
 for(const auto& [name,file]:request.files){(void)file;if(names.contains(name))return true;}return false;
}
void handle_supplied_case_context(const httplib::Request& request,httplib::Response& response){
 std::vector<minera::DiscoveryWorkbenchPart> parts,query;
 for(const auto& [name,value]:request.params)query.push_back({name,"",value,false});
 for(const auto& [name,file]:request.files)parts.push_back({name,file.filename,file.content,file.filename_present});
 const auto result=minera::execute_supplied_case_context(parts,query,"httplib_key_order",request.is_multipart_form_data()?"":"supplied case context requires multipart File input");
 response.status=result.status;response.set_content(result.json,"application/json");
}
