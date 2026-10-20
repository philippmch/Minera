#pragma once
bool request_uses_original_model_context(const httplib::Request& request){
 static const std::set<std::string> names={"model_file","model_format","context_options_json","context_options_file","initial_marking_json","initial_marking_file","final_marking_json","final_marking_file","pnml_import_options_json","pnml_import_options_file"};
 for(const auto& [name,value]:request.params){(void)value;if(names.contains(name))return true;}
 for(const auto& [name,file]:request.files){(void)file;if(names.contains(name))return true;}return false;
}
void handle_original_model_context(const httplib::Request& request,httplib::Response& response){
 std::vector<minera::DiscoveryWorkbenchPart> parts,query;
 for(const auto& [name,value]:request.params)query.push_back({name,"",value,false});
 for(const auto& [name,file]:request.files)parts.push_back({name,file.filename,file.content,file.filename_present});
 const auto result=minera::execute_original_model_context(parts,query,"httplib_key_order",request.is_multipart_form_data()?"":"original model context requires multipart File input");
 response.status=result.status;response.set_content(result.json,"application/json");
}
