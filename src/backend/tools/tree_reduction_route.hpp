#pragma once
// Scoped opt-in branch. Existing model conversion handlers remain unchanged.
bool request_uses_tree_reduction(const httplib::Request& request) {
  for (const auto& [name,value] : request.params)
    if (name == "operation" && value == "reduce_tree") return true;
  for (const auto& [name,file] : request.files)
    if (name == "operation" && file.content == "reduce_tree") return true;
  return false;
}
void handle_tree_reduction_workbench(const httplib::Request& request,
                                     httplib::Response& response) {
  std::vector<minera::DiscoveryWorkbenchPart> parts, query;
  for (const auto& [name,value] : request.params) query.push_back({name,"",value,false});
  for (const auto& [name,file] : request.files)
    parts.push_back({name,file.filename,file.content,file.filename_present});
  const auto result = minera::execute_tree_reduction_workbench(parts,query,
      "httplib_key_order",request.is_multipart_form_data() ? "" : "tree reduction requires multipart original Files");
  response.status=result.status;response.set_content(result.json,"application/json");
}
