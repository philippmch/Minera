#undef NDEBUG
#include "minera/source_acquisition.hpp"
#include "minera/xes_export.hpp"
#include "outlook_source_fixture.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <oleauto.h>
#endif
using namespace minera;
template<class F> void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}assert(failed);}
#ifdef _WIN32
using namespace minera::test_outlook;
namespace {
void save(const std::filesystem::path& output,const std::string& name,const SourceAcquisitionResult& result){if(output.empty())return;std::ofstream(output/(name+".json"),std::ios::binary)<<source_acquisition_json(result);std::ofstream(output/(name+"-items.json"),std::ios::binary)<<result.outlook_capture->item_table_json;std::ofstream(output/(name+"-formatted.json"),std::ios::binary)<<result.outlook_capture->formatted_table_json;XesExportOptions xes;xes.preserve_nulls=true;std::ofstream(output/(name+".xes"),std::ios::binary)<<serialize_xes(result.log,xes);}
}
#endif
int main(int argc,char** argv){
 OutlookSourceOptions invalid;invalid.history.max_items=0;rejects([&]{(void)capture_outlook_source(invalid);});invalid.history.max_items=1;invalid.calendar_id=0;rejects([&]{(void)capture_outlook_source(invalid);});invalid.calendar_id=9;invalid.history.current_time="invalid";rejects([&]{(void)capture_outlook_source(invalid);});
#ifdef _WIN32
 const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);assert(SUCCEEDED(initialized));std::filesystem::path output;if(argc>1){output=std::filesystem::u8path(argv[1]);assert(!std::filesystem::exists(output));std::filesystem::create_directories(output);}Graph graph;SourceAcquisitionInput input;input.connector="outlook_mail_extractor";input.parameters={{"outlook_source","com"}};input.outlook_application=graph.application;
 auto acquired=acquire_source(input);assert(acquired.log.trace_count()==2&&acquired.log.event_count()==4&&acquired.outlook_capture->total_items==5&&acquired.outlook_capture->skipped_items==1&&!acquired.outlook_capture->limited&&!acquired.original_bytes);assert(acquired.log.traces[0].case_id=="conversation-b"&&acquired.log.traces[0].events[0].activity=="Sent Meeting Request");assert(*acquired.log.traces[0].events[0].attribute("recipients")=="Owned recipient 1 AND Owned recipient 2");save(output,"mail",acquired);graph.balanced();
 input.parameters["communication_max_items"]="1";auto bounded=acquire_source(input);assert(bounded.outlook_capture->limited&&bounded.outlook_capture->examined_items==1&&bounded.outlook_capture->total_items==5&&bounded.log.event_count()==1);save(output,"mail-bound",bounded);graph.balanced();input.parameters.erase("communication_max_items");
 input.connector="outlook_calendar";input.parameters["communication_current_time"]="2026-10-09T00:00:00Z";auto calendar=acquire_source(input);assert(calendar.log.trace_count()==2&&calendar.log.event_count()==11&&calendar.log.traces[0].events[2].activity=="Meeting Reminder Fired");assert(calendar.log.traces[0].events[2].timestamp=="2024-01-03T09:45:00.000000000Z");save(output,"calendar",calendar);graph.balanced();
 input.parameters["communication_include_reminders"]="false";auto no_reminders=acquire_source(input);assert(no_reminders.log.event_count()==9);save(output,"calendar-no-reminders",no_reminders);input.parameters.erase("communication_include_reminders");
 input.parameters["outlook_email_user"]="owner@example.invalid";auto shared=acquire_source(input);assert(shared.log.event_count()==11);save(output,"calendar-shared",shared);graph.balanced();assert(std::find(graph.calls.begin(),graph.calls.end(),L"GetSharedDefaultFolder")!=graph.calls.end());graph.resolved=false;rejects([&]{(void)acquire_source(input);});graph.balanced();graph.resolved=true;input.parameters.erase("outlook_email_user");
 input.parameters["communication_current_time"]="2020-01-01T00:00:00Z";assert(acquire_source(input).log.event_count()==9);input.parameters["communication_current_time"]="2026-10-09T00:00:00Z";
 graph.appointments[0]->values[graph.appointments[0]->ids[L"Subject"]]=[](DISPPARAMS*,VARIANT*){return E_ACCESSDENIED;};auto skipped=acquire_source(input);assert(skipped.log.trace_count()==1&&skipped.log.event_count()==6&&skipped.outlook_capture->skipped_items==1&&skipped.outlook_capture->item_table_json.find("com_property_error")!=std::string::npos);save(output,"calendar-skipped",skipped);graph.balanced();
 graph.appointments[0]->values[graph.appointments[0]->ids[L"Subject"]]=[](DISPPARAMS*,VARIANT* out){out->vt=VT_BSTR;out->bstrVal=SysAllocString(L"Owned\x0001 subject, \u6771\u4eac");return S_OK;};auto controls=acquire_source(input);assert(source_acquisition_json(controls).find("\\u0001")!=std::string::npos);save(output,"calendar-controls",controls);graph.balanced();
 input.source_bytes="body";rejects([&]{(void)acquire_source(input);});input.source_bytes.reset();input.server_path="explicit";rejects([&]{(void)acquire_source(input);});input.server_path.clear();input.parameters["outlook_calendar_id"]="0";rejects([&]{(void)acquire_source(input);});input.parameters.erase("outlook_calendar_id");graph.denied=true;rejects([&]{(void)acquire_source(input);});graph.balanced();CoUninitialize();std::cout<<"Controlled COM Outlook extraction, limits, shared selection, skips and complete exports passed\n";
#else
 OutlookSourceOptions options;rejects([&]{(void)capture_outlook_source(options);});
#endif
}
