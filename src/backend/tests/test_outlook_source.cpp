#undef NDEBUG
#include "minera/source_acquisition.hpp"
#include "minera/xes_export.hpp"
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
namespace {
struct Node:IDispatch {
 ULONG references=1;std::map<std::wstring,DISPID> ids;std::map<DISPID,std::function<HRESULT(DISPPARAMS*,VARIANT*)>> values;std::vector<std::wstring>* calls;
 explicit Node(std::vector<std::wstring>& history):calls(&history){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** p)override{if(!p)return E_POINTER;*p=nullptr;if(iid==IID_IUnknown||iid==IID_IDispatch){*p=this;AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++references;}
 ULONG STDMETHODCALLTYPE Release()override{assert(references>0);return --references;}
 HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* n)override{*n=0;return S_OK;}
 HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT,LCID,ITypeInfo**)override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR* names,UINT count,LCID,DISPID* result)override{if(count!=1)return E_INVALIDARG;const auto it=ids.find(names[0]);if(it==ids.end())return DISP_E_UNKNOWNNAME;*result=it->second;return S_OK;}
 HRESULT STDMETHODCALLTYPE Invoke(DISPID id,REFIID,LCID,WORD flags,DISPPARAMS* args,VARIANT* result,EXCEPINFO*,UINT*)override{if(flags&(DISPATCH_PROPERTYPUT|DISPATCH_PROPERTYPUTREF))return E_ACCESSDENIED;VariantInit(result);for(const auto& [name,index]:ids)if(index==id)calls->push_back(name);const auto it=values.find(id);return it==values.end()?DISP_E_MEMBERNOTFOUND:it->second(args,result);}
 void method(const wchar_t* name,std::function<HRESULT(DISPPARAMS*,VARIANT*)> f){const auto id=static_cast<DISPID>(ids.size()+1);ids.emplace(name,id);values.emplace(id,std::move(f));}
 void text(const wchar_t* name,const std::wstring& s){method(name,[s](DISPPARAMS*,VARIANT* out){out->vt=VT_BSTR;out->bstrVal=SysAllocStringLen(s.data(),static_cast<UINT>(s.size()));return out->bstrVal?S_OK:E_OUTOFMEMORY;});}
 void integer(const wchar_t* name,long n){method(name,[n](DISPPARAMS*,VARIANT* out){out->vt=VT_I4;out->lVal=n;return S_OK;});}
 void boolean(const wchar_t* name,bool b){method(name,[b](DISPPARAMS*,VARIANT* out){out->vt=VT_BOOL;out->boolVal=b?VARIANT_TRUE:VARIANT_FALSE;return S_OK;});}
 void object(const wchar_t* name,Node* p){method(name,[p](DISPPARAMS*,VARIANT* out){out->vt=VT_DISPATCH;out->pdispVal=p;p->AddRef();return S_OK;});}
 void date(const wchar_t* name,unsigned day,unsigned hour=10,unsigned year=2024,unsigned microsecond=0){SYSTEMTIME utc{},local{};utc.wYear=static_cast<WORD>(year);utc.wMonth=1;utc.wDay=static_cast<WORD>(day);utc.wHour=static_cast<WORD>(hour);assert(SystemTimeToTzSpecificLocalTimeEx(nullptr,&utc,&local));DATE d=0;assert(SystemTimeToVariantTime(&local,&d));d+=static_cast<double>(microsecond)/86400000000.0;method(name,[d](DISPPARAMS*,VARIANT* out){out->vt=VT_DATE;out->date=d;return S_OK;});}
};
struct Graph {
 std::vector<std::wstring> calls;std::vector<std::unique_ptr<Node>> nodes;Node *application,*ns,*recipient;std::vector<Node*> appointments;bool resolved=true,denied=false;
 Node* node(){nodes.push_back(std::make_unique<Node>(calls));return nodes.back().get();}
 Node* collection(const std::vector<Node*>& items){auto n=node();n->integer(L"Count",static_cast<long>(items.size()));n->method(L"Item",[items](DISPPARAMS* args,VARIANT* out){if(args->cArgs!=1||args->rgvarg[0].vt!=VT_I4)return DISP_E_TYPEMISMATCH;const auto i=args->rgvarg[0].lVal;if(i<=0||i>static_cast<long>(items.size()))return DISP_E_BADINDEX;out->vt=VT_DISPATCH;out->pdispVal=items[static_cast<std::size_t>(i-1)];out->pdispVal->AddRef();return S_OK;});return n;}
 Node* item(const std::wstring& name,const std::wstring& conversation,long klass,unsigned day){auto n=node();n->integer(L"Class",klass);n->text(L"ConversationID",conversation);n->text(L"Subject",name+L" subject, \u6771\u4eac");n->text(L"ConversationTopic",name+L" topic");n->date(L"CreationTime",day);n->date(L"LastModificationTime",day+1);n->date(L"Start",day+2);n->integer(L"Duration",60);n->integer(L"MeetingStatus",0);n->boolean(L"ReminderSet",true);n->integer(L"ReminderMinutesBeforeStart",15);auto sender=node();sender->text(L"Name",L"Owned sender");n->object(L"Sender",sender);auto first=node(),second=node();first->text(L"Name",L"Owned recipient 1");second->text(L"Name",L"Owned recipient 2");n->object(L"Recipients",collection({first,second}));return n;}
 Graph(){application=node();ns=node();recipient=node();recipient->method(L"Resolve",[](DISPPARAMS* p,VARIANT*){assert(!p->cArgs);return S_OK;});recipient->method(L"Resolved",[this](DISPPARAMS*,VARIANT* out){out->vt=VT_BOOL;out->boolVal=resolved?VARIANT_TRUE:VARIANT_FALSE;return S_OK;});
  auto sent=collection({item(L"sent-1",L"conversation-\u6771\u4eac",43,2),item(L"sent-2",L"conversation-b",53,1),item(L"unknown",L"ignored",999,1)});auto received=collection({item(L"received-1",L"conversation-\u6771\u4eac",43,1),item(L"received-2",L"conversation-b",56,3)});appointments={item(L"appointment-1",L"calendar-\u6771\u4eac",26,1),item(L"appointment-2",L"calendar-b",26,4)};appointments[1]->values[appointments[1]->ids[L"MeetingStatus"]]=[](DISPPARAMS*,VARIANT* out){out->vt=VT_I4;out->lVal=5;return S_OK;};
  std::map<long,Node*> folders;for(const auto& [id,items]:std::vector<std::pair<long,Node*>>{{5,sent},{6,received},{9,collection(appointments)}}){auto box=node();box->object(L"Items",items);folders.emplace(id,box);}
  application->method(L"GetNamespace",[this](DISPPARAMS* p,VARIANT* out){assert(p->cArgs==1&&p->rgvarg[0].vt==VT_BSTR&&std::wstring(p->rgvarg[0].bstrVal)==L"MAPI");out->vt=VT_DISPATCH;out->pdispVal=ns;ns->AddRef();return S_OK;});
  ns->method(L"GetDefaultFolder",[this,folders](DISPPARAMS* p,VARIANT* out){if(denied)return E_ACCESSDENIED;assert(p->cArgs==1&&p->rgvarg[0].vt==VT_I4);const auto it=folders.find(p->rgvarg[0].lVal);if(it==folders.end())return DISP_E_BADINDEX;out->vt=VT_DISPATCH;out->pdispVal=it->second;out->pdispVal->AddRef();return S_OK;});
  ns->method(L"CreateRecipient",[this](DISPPARAMS* p,VARIANT* out){assert(p->cArgs==1&&p->rgvarg[0].vt==VT_BSTR&&std::wstring(p->rgvarg[0].bstrVal)==L"owner@example.invalid");out->vt=VT_DISPATCH;out->pdispVal=recipient;recipient->AddRef();return S_OK;});
  ns->method(L"GetSharedDefaultFolder",[this,folders](DISPPARAMS* p,VARIANT* out){assert(p->cArgs==2&&p->rgvarg[0].vt==VT_I4&&p->rgvarg[1].vt==VT_DISPATCH&&p->rgvarg[1].pdispVal==recipient);out->vt=VT_DISPATCH;out->pdispVal=folders.at(p->rgvarg[0].lVal);out->pdispVal->AddRef();return S_OK;});
 }
 void balanced(){for(const auto& n:nodes)assert(n->references==1);}
};
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
