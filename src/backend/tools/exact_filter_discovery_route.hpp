#pragma once
namespace exact_filter_discovery_route {
inline bool requested(const httplib::Request& request){
 for(const auto& [key,value]:request.params){(void)value;if(minera::exact_filter_discovery_control(key))return true;}
 for(const auto& [key,value]:request.files){(void)value;if(minera::exact_filter_discovery_control(key))return true;}
 return false;
}
inline void inputs(const httplib::Request& request,minera::ExactFilterDiscoveryContext& context){
 context.input_order="httplib_key_order";
 for(const auto& [name,value]:request.params)context.query_inputs.push_back({name,"",value,false});
 for(const auto& [name,value]:request.files)context.original_inputs.push_back({name,value.filename,value.content,value.filename_present});
 if(!request.is_multipart_form_data()&&!request.body.empty())context.original_inputs.push_back({"log","",request.body,false});
}
inline void validate(const minera::ExactFilterDiscoveryContext& context){
 std::set<std::string> names;
 for(const auto* rows:{&context.original_inputs,&context.query_inputs})for(const auto& part:*rows){
  if(!names.insert(part.name).second)throw std::invalid_argument("duplicate exact request control: "+part.name);
  if(minera::exact_filter_discovery_control(part.name)&&part.name.ends_with("_file")&&!part.file)throw std::invalid_argument(part.name+": requires actual File filename presence");
 }
}
template<class Handler>void integrated(const httplib::Request& request,httplib::Response& response,std::string_view operation,Handler legacy){
 if(!requested(request)){legacy(request,response);return;}
 minera::ExactFilterDiscoveryContext context;inputs(request,context);minera::ExactFilterDiscoveryScope scope(context);
 try{
  validate(context);
  const auto presence=[&](std::string_view key){return request.has_param(std::string(key))||request.has_file(std::string(key));};
  if(minera::exact_heuristic_options_requested(presence))for(const auto key:{"mining_selectors_json","discovery_selectors_json"})if(presence(key)){
   const auto value=request.has_file(key)?request.get_file_value(key).content:request.get_param_value(key);
   for(const auto& [name,selector]:minera::parse_workflow_selector_strings_json(value)){(void)selector;if(name.starts_with("heuristic_"))throw std::invalid_argument(std::string("heuristic_options_json: conflicts by presence with ")+key);}
  }
  auto adapted=request;
  if(operation=="mine"&&request.is_multipart_form_data()){
   for(const auto& [name,value]:request.params){httplib::MultipartFormData field;field.name=name;field.content=value;adapted.files.emplace(name,std::move(field));}
   adapted.params.clear();
  } else if(operation=="discover_workflow"){
   for(const auto& [name,value]:request.files)adapted.params.emplace(name,value.content);
   adapted.files.clear();
  }
  legacy(adapted,response);
 }catch(const std::exception& e){response.status=400;response.set_content(error_json(e.what()),"application/json");}
 const auto body=response.body;const int status=response.status==-1 ? 200 : response.status;
 response.set_content(minera::exact_filter_discovery_response(context,operation,status,body),"application/json");response.status=status;
}
inline bool is_ltl(const httplib::Request& request){
 const auto range=request.params.equal_range("operation");for(auto i=range.first;i!=range.second;++i)if(i->second=="filter_ltl")return true;
 const auto files=request.files.equal_range("operation");for(auto i=files.first;i!=files.second;++i)if(i->second.content=="filter_ltl")return true;
 return false;
}
inline void embedding(const httplib::Request& request,httplib::Response& response){
 if(!is_ltl(request)){handle_original_embedding_workbench(request,response);return;}
 minera::ExactFilterDiscoveryContext context;inputs(request,context);
 const auto result=minera::execute_exact_ltl_filter(context.original_inputs,context.query_inputs,"httplib_key_order");
 response.status=result.status;response.set_content(result.json,"application/json");
}
} // namespace exact_filter_discovery_route
