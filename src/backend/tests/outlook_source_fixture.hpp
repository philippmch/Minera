#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <oleauto.h>
#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
namespace minera::test_outlook {
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
inline void install_large_mail(Graph& g) {
 std::vector<Node*> sent,received;
 for(unsigned i=0;i<60;++i){const auto id=L"Owned-case-"+std::to_wstring(i)+L"-\u6771\u4eac";sent.push_back(g.item(L"Owned-sent-"+std::to_wstring(i),id,43,2));received.push_back(g.item(L"Owned-inbox-"+std::to_wstring(i),id,43,1));}
 sent.push_back(g.item(L"Owned-unsupported",L"ignored",999,1));auto inaccessible=g.item(L"Owned-inaccessible",L"ignored",43,1);inaccessible->values[inaccessible->ids[L"Subject"]]=[](DISPPARAMS*,VARIANT*){return E_ACCESSDENIED;};received.push_back(inaccessible);
 std::map<long,Node*> boxes;for(const auto& [id,items]:std::vector<std::pair<long,Node*>>{{5,g.collection(sent)},{6,g.collection(received)}}){auto box=g.node();box->object(L"Items",items);boxes.emplace(id,box);}
 const auto calendar=g.ns->values.at(g.ns->ids.at(L"GetDefaultFolder"));g.ns->values[g.ns->ids.at(L"GetDefaultFolder")]=[boxes,calendar](DISPPARAMS* p,VARIANT* out){assert(p->cArgs==1&&p->rgvarg[0].vt==VT_I4);const auto box=boxes.find(p->rgvarg[0].lVal);if(box==boxes.end())return calendar(p,out);out->vt=VT_DISPATCH;out->pdispVal=box->second;out->pdispVal->AddRef();return S_OK;};
}
}  // namespace minera::test_outlook
#endif
