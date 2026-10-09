#include "minera/source_acquisition.hpp"
#include "minera/xes_export.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace minera;
#undef assert
#define assert(condition) do{if(!(condition)){std::cerr<<"Assertion failed at "<<__LINE__<<": "<<#condition<<'\n';throw std::runtime_error(#condition);}}while(false)
template<class F> void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}assert(failed);}
void normalized_sources_preserve_original_bytes_and_native_fields() {
  SourceAcquisitionInput input;input.connector="camunda_workflow";input.source_bytes="process_id,case_id,activity,timestamp,resource\np,c1,\"A,東京\",2024-01-01T00:00:00Z,r\n";const auto workflow=acquire_source(input);assert(workflow.log.event_count()==1&&workflow.original_bytes==input.source_bytes);const auto* process=workflow.log.traces[0].attribute("case:process_id");assert(process&&*process=="p");assert(source_acquisition_json(workflow).find("\"native_log\":{")!=std::string::npos);
  input.connector="sap_o2c";input.source_bytes="case_id,activity,timestamp,resource,document_type\nc,Create,2024-01-01T00:00:00Z,r,東京\n";const auto order=acquire_source(input);assert(order.log.traces[0].events[0].activity=="Create"&&order.original_bytes==input.source_bytes);input.connector="sap_accounting";const auto accounting=acquire_source(input);const auto* document=accounting.log.traces[0].events[0].attribute("document_type");assert(document&&*document=="東京"&&accounting.native_result_json.find("\"variant\":\"accounting\"")!=std::string::npos);
}
void mailbox_and_calendar_native_event_expansion_is_preserved() {
  SourceAcquisitionInput input;input.connector="outlook_mail_extractor";input.source_bytes="conversation_id,direction,item_class,created_at,sender,recipients,topic,subject\nc,Sent,43,2024-01-01T00:00:00Z,A,B,T,Hello\n";assert(acquire_source(input).log.event_count()==1);
  input.connector="outlook_calendar";input.parameters["communication_current_time"]="2024-02-01T00:00:00Z";input.source_bytes="conversation_id,subject,created_at,last_changed_at,start_at,end_at,cancelled,reminder_at\nc,Meet,2024-01-01T00:00:00Z,2024-01-02T00:00:00Z,2024-01-03T09:00:00Z,2024-01-03T10:00:00Z,true,2024-01-03T08:45:00Z\n";assert(acquire_source(input).log.event_count()==6);input.parameters["communication_include_reminders"]="false";assert(acquire_source(input).log.event_count()==5);
}
void repository_transport_is_injected_and_tokens_are_not_serialized() {
  SourceAcquisitionInput input;input.connector="github_repo";input.parameters={{"repository_owner","acme"},{"repository_name","engine"},{"repository_api_base_url","https://api.example.test"}};input.auth_token="synthetic-secret";input.repository_transport=[](std::string_view url,std::string_view token){assert(token=="synthetic-secret");if(url.ends_with("page=1"))return RepositoryHistoryResponse{200,R"([{"timeline_url":"https://api.example.test/repos/acme/engine/issues/1/timeline","created_at":"2024-01-01T00:00:00Z","user":{"login":"a"},"author_association":"NONE","title":"T"}])"};return RepositoryHistoryResponse{200,"[]"};};const auto result=acquire_source(input);assert(result.log.trace_count()==1&&result.log.event_count()==1);assert(source_acquisition_json(result).find(input.auth_token)==std::string::npos);
}
void invalid_sources_fail_explicitly() {
  SourceAcquisitionInput input;input.connector="unknown";input.source_bytes="x";rejects([&]{(void)acquire_source(input);});input.connector="chrome_history";input.source_bytes="not sqlite";rejects([&]{(void)acquire_source(input);});input.connector="windows_events";input.source_bytes="<Events><Event><System><Provider Name=\"P\"/><EventID>1</EventID></System></Event></Events>";rejects([&]{(void)acquire_source(input);});input.server_path="also-a-path";rejects([&]{(void)acquire_source(input);});
}
int main(){try{normalized_sources_preserve_original_bytes_and_native_fields();mailbox_and_calendar_native_event_expansion_is_preserved();repository_transport_is_injected_and_tokens_are_not_serialized();invalid_sources_fail_explicitly();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
