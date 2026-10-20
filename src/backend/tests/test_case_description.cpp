#include "minera/case_description_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include <cassert>
#include <iostream>
#include <limits>
using namespace minera;
namespace {
void success(const std::string& input,const std::string& options,const std::string& expected,std::size_t length){
 const auto supplied=parse_case_description_json(input);const auto controls=parse_case_description_options_json(options);const auto before=serialize_case_description_json(supplied);
 const auto result=abstract_case(supplied,controls);assert(result.text==expected&&result.characters==length&&result.variant=="case"&&!result.truncated);
 assert(serialize_case_description_json(supplied)==before);
 const auto roundtrip=parse_case_description_json(before);assert(abstract_case(roundtrip,controls).text==expected);
 const auto response=execute_supplied_case_context({{"case_file","supplied.case.json",input,true},{"case_options_json","",options,false}});
 assert(response.status==200);assert(response.json.find("\"native_returned\":true")!=std::string::npos);assert(response.json.find("\"case_unchanged\":true")!=std::string::npos);
 assert(response.json.find("\"native_text_bytes\":"+serialize_embedding_workbench_string_json(expected))!=std::string::npos);
}
void key_error(const std::string& input,const std::string& options,const std::string& key,const std::string& message){
 const auto supplied=parse_case_description_json(input);const auto before=serialize_case_description_json(supplied);bool thrown=false;
 try{(void)abstract_case(supplied,parse_case_description_options_json(options));}catch(const CaseDescriptionKeyError& error){thrown=true;assert(error.key==key&&error.what()==message);}assert(thrown&&serialize_case_description_json(supplied)==before);
 const auto response=execute_supplied_case_context({{"case_file","case.json",input,true},{"case_options_file","options.json",options,true}});
 assert(response.status==400&&response.json.find("\"native_entered\":true")!=std::string::npos&&response.json.find("\"native_returned\":false")!=std::string::npos);
 assert(response.json.find("\"type\":\"KeyError\"")!=std::string::npos&&response.json.find("\"case_unchanged\":true")!=std::string::npos);
}
#include "case_description_public_vectors.inc"
void configuration(){
 const std::string empty="{\"attributes\":{\"type\":\"builtins.dict\",\"items\":[]},\"events\":[]}";
 const auto supplied=parse_case_description_json(empty);
 const auto no_headers=parse_case_description_options_json("{\"include_case_attributes\":false,\"include_event_attributes\":false,\"include_timestamp\":false,\"include_header\":false}");
 const auto direct=abstract_case(supplied,no_headers);
 assert(direct.text=="\n\n\n\n\n"&&direct.characters==5&&!direct.truncated);
 const DiscoveryWorkbenchPart file{"case_file","case.json",empty,true};
 const auto controlled=execute_supplied_case_context({file,{"case_options_file","options.json",serialize_case_description_options_json(no_headers),true}});
 assert(controlled.status==200&&controlled.json.find("\"native_text_bytes\":"+serialize_embedding_workbench_string_json(direct.text))!=std::string::npos);
 assert(execute_supplied_case_context({file}).status==200);
 assert(execute_supplied_case_context({file},{{"variant","","case",false}}).status==200);
 assert(execute_supplied_case_context({file,file}).status==400);
 assert(execute_supplied_case_context({{"case_file","",empty,false}}).status==400);
 assert(execute_supplied_case_context({file},{{"variant","","dfg",false}}).status==400);
 assert(execute_supplied_case_context({file,{"case_options_json","","{}",false},{"case_options_file","o.json","{}",true}}).status==400);
 assert(execute_supplied_case_context({file,{"case_options_json","","{\"max_len\":1}",false}}).status==400);
 assert(execute_supplied_case_context({file,{"case_options_json","","{\"include_header\":0}",false}}).status==400);
 assert(execute_supplied_case_context({file},{},"provided_order","incomplete multipart").json.find("\"phase\":\"transport_error\"")!=std::string::npos);
 for(const std::string malformed:{"{\"attributes\":{},\"events\":[]}","{\"attributes\":{\"type\":\"builtins.dict\",\"items\":[[]]},\"events\":[]}","{\"attributes\":{\"type\":\"builtins.dict\",\"items\":[]},\"events\":[],\"extra\":0}"})assert(execute_supplied_case_context({{"case_file","c.json",malformed,true}}).status==400);
 const auto options=parse_case_description_options_json("{\"activity_key\":\"\",\"timestamp_key\":\"\\ud800\"}");assert(parse_case_description_options_json(serialize_case_description_options_json(options)).timestamp_key==options.timestamp_key);
}
} // namespace
int main(){public_byte_vectors();configuration();std::cout<<"Case description: 32 retained public vectors and supplied-case controls passed\n";}
