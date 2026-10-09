#include "minera/outlook_source.hpp"
#include "minera/timestamp.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <oleauto.h>
#endif

namespace minera {
namespace {
std::string quote(const std::string& value) {
  std::ostringstream out;out<<'"';for(unsigned char c:value){if(c=='"'||c=='\\')out<<'\\'<<c;else if(c<32)out<<"\\u00"<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(c)<<std::dec;else out<<c;}out<<'"';return out.str();
}
void validate(const OutlookSourceOptions& o) {
  if(!o.history.max_items||o.history.max_items>1000000||!o.history.max_events||o.history.max_events>10000000||!o.calendar_id||o.calendar_id>1024||o.email_user.size()>1024||o.email_user.find('\0')!=std::string::npos)throw std::invalid_argument("Outlook acquisition: invalid item/event limit, folder ID or shared account");
  if(o.history.variant==CommunicationHistoryVariant::kMailbox&&(!o.email_user.empty()||o.calendar_id!=9))throw std::invalid_argument("Outlook mailbox acquisition does not accept shared calendar options");
  if(!o.history.current_time.empty()&&!parse_timestamp_exact(o.history.current_time))throw std::invalid_argument("Outlook acquisition: current time must be an ISO timestamp");
}
#ifdef _WIN32
class ComError:public std::runtime_error {
 public: ComError(const char* operation,HRESULT code):std::runtime_error(message(operation,code)),code(code){}HRESULT code;
 private:static std::string message(const char* operation,HRESULT code){std::ostringstream out;out<<"Outlook acquisition: "<<operation<<" failed (HRESULT 0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<static_cast<std::uint32_t>(code)<<"). Check Outlook desktop registration, the current profile and folder access.";return out.str();}
};
void check(HRESULT code,const char* operation){if(FAILED(code))throw ComError(operation,code);}
std::wstring wide(const std::string& v){if(v.empty())return {};const auto n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,v.data(),static_cast<int>(v.size()),nullptr,0);if(!n)throw std::invalid_argument("Outlook acquisition: invalid UTF-8 account");std::wstring s(n,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,v.data(),static_cast<int>(v.size()),s.data(),n);return s;}
std::string narrow(BSTR v){if(!v)return {};const auto size=SysStringLen(v);const auto n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,v,size,nullptr,0,nullptr,nullptr);if(size&&!n)throw std::invalid_argument("Outlook acquisition: invalid Unicode property");if(n>16*1024*1024)throw std::length_error("Outlook acquisition: property exceeds 16 MiB");std::string s(n,'\0');if(n)WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,v,size,s.data(),n,nullptr,nullptr);return s;}
struct Value {
 VARIANT data;Value(){VariantInit(&data);}~Value(){VariantClear(&data);}Value(const Value&)=delete;Value& operator=(const Value&)=delete;
 Value(Value&& v)noexcept:data(v.data){VariantInit(&v.data);}Value& operator=(Value&& v)noexcept{if(this!=&v){VariantClear(&data);data=v.data;VariantInit(&v.data);}return *this;}
 static Value number(long n){Value v;v.data.vt=VT_I4;v.data.lVal=n;return v;}
 static Value text(const std::string& s){Value v;const auto w=wide(s);v.data.vt=VT_BSTR;v.data.bstrVal=SysAllocStringLen(w.data(),static_cast<UINT>(w.size()));if(!v.data.bstrVal)throw std::bad_alloc();return v;}
 static Value object(IDispatch* p){Value v;v.data.vt=VT_DISPATCH;v.data.pdispVal=p;if(p)p->AddRef();return v;}
};
Value invoke(IDispatch* object,const wchar_t* name,WORD flags,std::vector<Value> args={}) {
 if(!object)throw std::invalid_argument("Outlook acquisition: missing COM object");DISPID id=0;LPOLESTR property=const_cast<wchar_t*>(name);check(object->GetIDsOfNames(IID_NULL,&property,1,LOCALE_USER_DEFAULT,&id),"property lookup");
 std::vector<VARIANT> reversed;for(auto i=args.rbegin();i!=args.rend();++i)reversed.push_back(i->data);DISPPARAMS params{reversed.data(),nullptr,static_cast<UINT>(reversed.size()),0};Value result;EXCEPINFO info{};UINT argument=0;const auto code=object->Invoke(id,IID_NULL,LOCALE_USER_DEFAULT,flags,&params,&result.data,&info,&argument);
 if(info.pfnDeferredFillIn)info.pfnDeferredFillIn(&info);SysFreeString(info.bstrSource);SysFreeString(info.bstrDescription);SysFreeString(info.bstrHelpFile);check(code,"property/method invocation");return result;
}
Value property(IDispatch* p,const wchar_t* name){return invoke(p,name,DISPATCH_PROPERTYGET);}
IDispatch* object(const Value& v){if(v.data.vt!=VT_DISPATCH||!v.data.pdispVal)throw std::invalid_argument("Outlook acquisition: expected a COM object property");return v.data.pdispVal;}
std::string text(const Value& v){if(v.data.vt!=VT_BSTR)throw std::invalid_argument("Outlook acquisition: expected a text property");return narrow(v.data.bstrVal);}
long number(const Value& v){if(v.data.vt==VT_I4)return v.data.lVal;if(v.data.vt==VT_I2)return v.data.iVal;throw std::invalid_argument("Outlook acquisition: expected an integer property");}
bool boolean(const Value& v){if(v.data.vt!=VT_BOOL)throw std::invalid_argument("Outlook acquisition: expected a boolean property");return v.data.boolVal!=VARIANT_FALSE;}
std::string instant(std::int64_t usec){using namespace std::chrono;const sys_time<microseconds> t{microseconds{usec}};const auto d=floor<days>(t);const year_month_day ymd{d};const hh_mm_ss time{t-d};std::ostringstream out;out<<std::setfill('0')<<std::setw(4)<<static_cast<int>(ymd.year())<<'-'<<std::setw(2)<<static_cast<unsigned>(ymd.month())<<'-'<<std::setw(2)<<static_cast<unsigned>(ymd.day())<<'T'<<std::setw(2)<<time.hours().count()<<':'<<std::setw(2)<<time.minutes().count()<<':'<<std::setw(2)<<time.seconds().count()<<'.'<<std::setw(9)<<duration_cast<nanoseconds>(time.subseconds()).count()<<'Z';return out.str();}
std::int64_t date(const Value& v) {
 if(v.data.vt!=VT_DATE||!std::isfinite(v.data.date))throw std::invalid_argument("Outlook acquisition: expected an OLE date property");SYSTEMTIME local{},utc{};if(!VariantTimeToSystemTime(v.data.date,&local))throw std::invalid_argument("Outlook acquisition: invalid OLE date");
 // DATE holds local wall time. Retain its fractional microseconds separately
 // because the SYSTEMTIME conversion can round them to whole seconds.
 const double fraction=std::abs(v.data.date-std::trunc(v.data.date));const auto micros=static_cast<std::int64_t>(std::llround(fraction*86400000000.0));using namespace std::chrono;const auto local_time=sys_days(year{1899}/12/30)+days{static_cast<int>(std::trunc(v.data.date))}+microseconds{micros};const auto local_day=floor<days>(local_time);const year_month_day ymd{local_day};const hh_mm_ss clock{local_time-local_day};local.wYear=static_cast<WORD>(static_cast<int>(ymd.year()));local.wMonth=static_cast<WORD>(static_cast<unsigned>(ymd.month()));local.wDay=static_cast<WORD>(static_cast<unsigned>(ymd.day()));local.wHour=static_cast<WORD>(clock.hours().count());local.wMinute=static_cast<WORD>(clock.minutes().count());local.wSecond=static_cast<WORD>(clock.seconds().count());local.wMilliseconds=0;
 if(!TzSpecificLocalTimeToSystemTimeEx(nullptr,&local,&utc))throw std::invalid_argument("Outlook acquisition: local date cannot be converted to UTC");using namespace std::chrono;const auto days=sys_days(year{utc.wYear}/month{utc.wMonth}/day{utc.wDay});return duration_cast<microseconds>(days.time_since_epoch()+hours{utc.wHour}+minutes{utc.wMinute}+std::chrono::seconds{utc.wSecond}).count()+micros%1000000;
}
std::vector<Value> arguments(Value a){std::vector<Value> v;v.push_back(std::move(a));return v;}
Value folder(IDispatch* ns,const OutlookSourceOptions& o,unsigned id){if(o.email_user.empty())return invoke(ns,L"GetDefaultFolder",DISPATCH_METHOD,arguments(Value::number(id)));auto recipient=invoke(ns,L"CreateRecipient",DISPATCH_METHOD,arguments(Value::text(o.email_user)));(void)invoke(object(recipient),L"Resolve",DISPATCH_METHOD);if(!boolean(property(object(recipient),L"Resolved")))throw std::invalid_argument("Outlook acquisition: shared calendar account did not resolve");auto args=arguments(Value::object(object(recipient)));args.push_back(Value::number(id));return invoke(ns,L"GetSharedDefaultFolder",DISPATCH_METHOD,std::move(args));}
std::string csv_cell(const std::string& s){std::string out="\"";for(char c:s){out+=c;if(c=='"')out+='"';}return out+'"';}
std::string table(const std::string& family,const std::vector<std::string>& columns,const std::vector<std::vector<std::string>>& rows,bool limited,const std::vector<std::string>& errors={}) {
 std::ostringstream out;out<<"{\"family\":"<<quote(family)<<",\"columns\":[";for(std::size_t i=0;i<columns.size();++i){if(i)out<<',';out<<quote(columns[i]);}out<<"],\"row_count\":"<<rows.size()<<",\"limited\":"<<(limited?"true":"false")<<",\"rows\":[";for(std::size_t i=0;i<rows.size();++i){if(i)out<<',';out<<'[';for(std::size_t j=0;j<rows[i].size();++j){if(j)out<<',';out<<quote(rows[i][j]);}out<<']';}out<<"],\"skipped\":[";for(std::size_t i=0;i<errors.size();++i){if(i)out<<',';out<<errors[i];}out<<"]}";return out.str();
}
#endif
}  // namespace

#ifdef _WIN32
OutlookSourceCapture capture_outlook_source_with_dispatch(const OutlookSourceOptions& o,IDispatch* application) {
 validate(o);const bool calendar=o.history.variant==CommunicationHistoryVariant::kCalendar;OutlookSourceCapture c;c.connector=calendar?"outlook_calendar":"outlook_mail_extractor";c.email_user=o.email_user;c.calendar_id=o.calendar_id;
 auto ns=invoke(application,L"GetNamespace",DISPATCH_METHOD,arguments(Value::text("MAPI")));
 const std::vector<std::string> columns=calendar?std::vector<std::string>{"conversation_id","subject","created_at","last_changed_at","start_at","end_at","cancelled","reminder_at","duration_minutes","meeting_status","reminder_set","reminder_minutes_before_start"}:std::vector<std::string>{"conversation_id","direction","item_class","created_at","sender","recipients","topic","subject"};
 std::vector<std::vector<std::string>> rows;std::vector<std::string> errors;std::size_t bytes=0;
 const std::map<long,std::string> classes={{26,"Appointment"},{40,"Contact"},{41,"Document"},{42,"Journal"},{43,"Mail"},{44,"Note"},{45,"Post"},{46,"Delivery Report"},{47,"Remote"},{48,"Task"},{49,"Task Request"},{50,"Task Request Update"},{51,"Task Request Acceptance"},{52,"Task Request Declination"},{53,"Meeting Request"},{54,"Meeting Cancellation"},{55,"Meeting Declination"},{56,"Meeting Acceptance"},{57,"Meeting Tentatively Accepted"},{69,"Distribution List"},{104,"Sharing"},{113,"Storage"},{181,"Meeting Forward Notification"}};
 for(unsigned id:calendar?std::vector<unsigned>{o.calendar_id}:std::vector<unsigned>{5,6}) {
  auto box=folder(object(ns),o,id),items=property(object(box),L"Items");const auto total=number(property(object(items),L"Count"));if(total<0)throw std::invalid_argument("Outlook acquisition: negative folder count");c.total_items+=static_cast<std::size_t>(total);
  for(long i=1;i<=total;++i){if(c.examined_items==o.history.max_items){c.limited=true;break;}++c.examined_items;
   try {
    auto item=invoke(object(items),L"Item",DISPATCH_METHOD|DISPATCH_PROPERTYGET,arguments(Value::number(i)));auto p=object(item);std::vector<std::string> row;
    if(calendar){const auto created=date(property(p,L"CreationTime")),changed=date(property(p,L"LastModificationTime")),start=date(property(p,L"Start"));const auto duration=number(property(p,L"Duration")),status=number(property(p,L"MeetingStatus"));const bool reminder=o.history.include_reminders&&boolean(property(p,L"ReminderSet"));const auto minutes=reminder?number(property(p,L"ReminderMinutesBeforeStart")):0;
     if(duration<0||duration>10000000||minutes<0||minutes>10000000)throw std::invalid_argument("Outlook acquisition: duration/reminder minutes outside capture bound");row={text(property(p,L"ConversationID")),text(property(p,L"Subject")),instant(created),instant(changed),instant(start),instant(start+static_cast<std::int64_t>(duration)*60000000),status==5?"true":"false",reminder?instant(start-static_cast<std::int64_t>(minutes)*60000000):"",std::to_string(duration),std::to_string(status),reminder?"true":"false",std::to_string(minutes)};
    }else{const auto klass=number(property(p,L"Class"));if(!classes.contains(klass)){++c.skipped_items;errors.push_back("{\"folder_id\":"+std::to_string(id)+",\"item_position\":"+std::to_string(i)+",\"reason\":\"unsupported_class\",\"item_class\":"+std::to_string(klass)+"}");continue;}const auto created=date(property(p,L"CreationTime"));auto sender=property(p,L"Sender"),recipients=property(p,L"Recipients");std::string names;const auto count=number(property(object(recipients),L"Count"));if(count<0||count>100000)throw std::invalid_argument("Outlook acquisition: recipient count outside capture bound");for(long j=1;j<=count;++j){auto recipient=invoke(object(recipients),L"Item",DISPATCH_METHOD|DISPATCH_PROPERTYGET,arguments(Value::number(j)));if(j>1)names+=" AND ";names+=text(property(object(recipient),L"Name"));}
     row={text(property(p,L"ConversationID")),id==5?"Sent":"Received",std::to_string(klass),instant(created),text(property(object(sender),L"Name")),names,text(property(p,L"ConversationTopic")),text(property(p,L"Subject"))};}
    if(row[0].empty())throw std::invalid_argument("Outlook acquisition: missing conversation ID");for(const auto& s:row){bytes+=s.size();if(bytes>256*1024*1024)throw std::length_error("Outlook acquisition: item capture exceeds 256 MiB");}rows.push_back(std::move(row));
   }catch(const ComError& e){++c.skipped_items;errors.push_back("{\"folder_id\":"+std::to_string(id)+",\"item_position\":"+std::to_string(i)+",\"reason\":\"com_property_error\",\"hresult\":"+quote(std::to_string(static_cast<std::uint32_t>(e.code)))+"}");}
   catch(const std::invalid_argument&){++c.skipped_items;errors.push_back("{\"folder_id\":"+std::to_string(id)+",\"item_position\":"+std::to_string(i)+",\"reason\":\"invalid_item_property\"}");}
  }
 }
 auto history=o.history;if(history.current_time.empty())history.current_time=format_timestamp(std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count());std::ostringstream csv;for(std::size_t i=0;i<columns.size();++i){if(i)csv<<',';csv<<columns[i];}csv<<'\n';for(const auto& row:rows){for(std::size_t i=0;i<row.size();++i){if(i)csv<<',';csv<<csv_cell(row[i]);}csv<<'\n';}
 c.item_table_json=table("OUTLOOK_ITEM_CAPTURE",columns,rows,c.limited,errors);c.log=import_communication_history_csv(csv.str(),history);
 if(!calendar)std::sort(c.log.traces.begin(),c.log.traces.end(),[](const Trace& a,const Trace& b){return a.case_id<b.case_id;});
 std::vector<std::string> formatted_columns=calendar?std::vector<std::string>{"case:concept:name","case:subject","time:timestamp","concept:name","@@index","@@case_index"}:std::vector<std::string>{"case:concept:name","concept:name","time:timestamp","org:resource","recipients","topic","subject","@@index","@@case_index"};std::ostringstream formatted;formatted<<"{\"family\":\"OUTLOOK_HISTORY_FORMATTED\",\"columns\":[";for(std::size_t i=0;i<formatted_columns.size();++i){if(i)formatted<<',';formatted<<quote(formatted_columns[i]);}formatted<<"],\"row_count\":"<<c.log.event_count()<<",\"limited\":"<<(c.limited?"true":"false")<<",\"rows\":[";bool first=true;
 for(std::size_t i=0;i<c.log.traces.size();++i){auto& t=c.log.traces[i];t.case_id_source_key="concept:name";for(auto& e:t.events){const auto subject_ptr=e.attribute("subject");const std::string subject=subject_ptr?*subject_ptr:"";if(calendar&&t.attributes.empty())t.attributes.push_back({"subject",subject,AttributeKind::kString,false});std::vector<std::string> values=calendar?std::vector<std::string>{e.case_id,subject,e.timestamp,e.activity}:std::vector<std::string>{e.case_id,e.activity,e.timestamp,e.resource,e.attribute("recipients")?*e.attribute("recipients"):"",e.attribute("topic")?*e.attribute("topic"):"",subject};
  e.timestamp_source_key="time:timestamp";e.activity_source_key="concept:name";e.case_id_source_key="case:concept:name";e.resource_source_key="org:resource";e.case_id_state=e.activity_state=e.timestamp_state=FirstClassAttributeState::kPresent;if(!calendar)e.resource_state=FirstClassAttributeState::kPresent;
  e.attributes.push_back({"case:concept:name",e.case_id,AttributeKind::kString,false});e.attributes.push_back({"concept:name",e.activity,AttributeKind::kString,false});EventAttribute time{"time:timestamp",e.timestamp,AttributeKind::kDate,false};time.typed_value=NativeDate{e.timestamp,NativeDateResolution::kMicrosecond,std::string("UTC")};e.timestamp_attribute=time;e.attributes.push_back(std::move(time));if(!calendar)e.attributes.push_back({"org:resource",e.resource,AttributeKind::kString,false});else e.attributes.push_back({"case:subject",subject,AttributeKind::kString,false});e.attributes.push_back({"@@index",std::to_string(e.input_order),AttributeKind::kInt,false});e.attributes.push_back({"@@case_index",std::to_string(i),AttributeKind::kInt,false});
  if(!first)formatted<<',';first=false;formatted<<'[';for(std::size_t j=0;j<values.size();++j){if(j)formatted<<',';formatted<<quote(values[j]);}formatted<<','<<e.input_order<<','<<i<<']';}}
 formatted<<"]}";c.formatted_table_json=formatted.str();return c;
}
#endif
OutlookSourceCapture capture_outlook_source(const OutlookSourceOptions& o){validate(o);
#ifdef _WIN32
 const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);check(initialized,"COM apartment initialization");struct Apartment{~Apartment(){CoUninitialize();}} apartment;CLSID clsid{};check(CLSIDFromProgID(L"Outlook.Application",&clsid),"Outlook registration");IDispatch* root=nullptr;check(CoCreateInstance(clsid,nullptr,CLSCTX_LOCAL_SERVER,IID_IDispatch,reinterpret_cast<void**>(&root)),"Outlook activation");auto application=Value::object(root);root->Release();return capture_outlook_source_with_dispatch(o,object(application));
#else
 throw std::runtime_error("Outlook acquisition: native Outlook COM requires Windows");
#endif
}
std::string outlook_source_capture_json(const OutlookSourceCapture& c){std::ostringstream out;out<<"{\"driver\":\"outlook_com\",\"connector\":"<<quote(c.connector)<<",\"email_user\":"<<quote(c.email_user)<<",\"calendar_id\":"<<c.calendar_id<<",\"total_items\":"<<c.total_items<<",\"examined_items\":"<<c.examined_items<<",\"captured_items\":"<<c.examined_items-c.skipped_items<<",\"skipped_items\":"<<c.skipped_items<<",\"limited\":"<<(c.limited?"true":"false")<<",\"has_item_capture\":true,\"item_bytes\":"<<c.item_table_json.size()<<",\"formatted_bytes\":"<<c.formatted_table_json.size()<<'}';return out.str();}
}  // namespace minera
