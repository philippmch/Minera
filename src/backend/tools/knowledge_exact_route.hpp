#pragma once
// Exact controls are opt-in; legacy handling and native prompt construction
// remain unchanged. Raw inputs are retained even if parsing/construction fails.
void handle_knowledge_exact(const httplib::Request& request,httplib::Response& response){
 std::vector<minera::KnowledgeExactInput> inputs;
 for(const auto& [name,value]:request.params)inputs.push_back({name,std::nullopt,value});
 for(const auto& [name,file]:request.files){
  const auto filename=file.filename_present?std::optional<std::string>(file.filename):std::nullopt;
  inputs.push_back({name,name.ends_with("_file")?filename:std::nullopt,file.content,filename});
 }
 std::optional<minera::KnowledgeInjectionOptions> options;std::optional<minera::KnowledgeInjectionResult> result;std::string error;
 try{options=minera::parse_knowledge_exact_inputs(inputs);result=minera::build_knowledge_prompt(*options);}catch(const std::exception& e){error=e.what();}
 response.status=error.empty()?200:400;response.set_content(minera::serialize_knowledge_exact_envelope_json(options,result,inputs,error),"application/json");
}
