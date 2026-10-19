#pragma once
void handle_original_quality_workbench(const httplib::Request& request,
    httplib::Response& response,const httplib::ContentReader& reader) {
  std::vector<minera::DiscoveryWorkbenchPart> query;
  for(const auto& [name,value]:request.params)query.push_back({name,"",value,false});
  const auto content_type=request.get_header_value("Content-Type");
  std::string body;bool complete=false;
  {
    // Vendored httplib read_content_core selects its multipart decoder by the
    // actual Request Content-Type even for ContentReader::reader_. That decoder
    // discards raw framing and filename-attribute presence (filename="" and no
    // filename both become an empty std::string). The callback receives a const
    // view of a nonconst request. Temporarily selecting its public raw receiver
    // here retains original MIME bytes; RAII restores every original header
    // entry before any parsing/response. Existing routes/library are unchanged.
    auto& actual=const_cast<httplib::Request&>(request);
    struct RestoreHeaders {
      httplib::Request& request;httplib::Headers saved;
      explicit RestoreHeaders(httplib::Request& value):request(value),saved(value.headers){
        request.headers.erase("Content-Type");
        request.headers.emplace("Content-Type","application/octet-stream");
      }
      ~RestoreHeaders(){request.headers=std::move(saved);}
    } restore(actual);
    try{complete=reader([&](const char* data,std::size_t size){
      if(size>body.max_size()-body.size())return false;
      body.append(data,size);return true;
    });}catch(const std::exception&){complete=false;}
  }
  const auto read_status=response.status;
  const auto result=minera::execute_quality_workbench_multipart(
      content_type,body,query,complete);
  response.status=!complete&&read_status>=400?read_status:result.status;
  response.set_content(result.json,"application/json");
}
