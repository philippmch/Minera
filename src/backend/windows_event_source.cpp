#include "minera/windows_event_source.hpp"
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winevt.h>
#endif
namespace minera {
namespace {
std::string quote(std::string_view s) {
  const char* digits="0123456789abcdef";std::string out="\"";
  for(unsigned char c:s){if(c=='"'||c=='\\'){out+='\\';out+=static_cast<char>(c);}else if(c<32){out+="\\u00";out+=digits[c>>4];out+=digits[c&15];}else out+=static_cast<char>(c);}return out+'"';
}
#ifdef _WIN32
std::wstring wide(const std::string& text) {
  const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
  if(!n)throw std::invalid_argument("Windows event source: invalid UTF-8 path or query");
  std::wstring result(n,L'\0');if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),result.data(),n))throw std::runtime_error("Windows event source: UTF-8 conversion failed");return result;
}
std::string utf8(const wchar_t* data,int length) {
  const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,data,length,nullptr,0,nullptr,nullptr);
  if(!n)throw std::runtime_error("Windows event source: event XML has invalid UTF-16");
  std::string result(n,'\0');if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,data,length,result.data(),n,nullptr,nullptr))throw std::runtime_error("Windows event source: event XML conversion failed");return result;
}
struct EventHandle {
  EVT_HANDLE value=nullptr;
  explicit EventHandle(EVT_HANDLE v):value(v){}
  ~EventHandle(){if(value)EvtClose(value);}
  EventHandle(const EventHandle&)=delete;
};
WindowsEventNext native_reader(const WindowsEventSourceOptions& options) {
  const auto query=wide(options.query),path=options.kind==WindowsEventSourceKind::kChannel?wide(options.channel):options.path.wstring();
  const DWORD flags=(options.kind==WindowsEventSourceKind::kChannel?EvtQueryChannelPath:EvtQueryFilePath)|(options.reverse?EvtQueryReverseDirection:EvtQueryForwardDirection);
  auto result=std::make_shared<EventHandle>(EvtQuery(nullptr,path.c_str(),query.c_str(),flags));
  if(!result->value)throw std::invalid_argument("Windows event source: EvtQuery failed ("+std::to_string(GetLastError())+"); check the explicit channel/file, query and current account access");
  return [result,timeout=options.timeout_ms]()->std::optional<std::string>{
    EVT_HANDLE event=nullptr;DWORD returned=0;
    if(!EvtNext(result->value,1,&event,timeout,0,&returned)){const auto error=GetLastError();if(error==ERROR_NO_MORE_ITEMS)return std::nullopt;throw std::runtime_error("Windows event source: EvtNext failed ("+std::to_string(error)+")");}
    EventHandle owner(event);DWORD bytes=0,properties=0;
    if(EvtRender(nullptr,event,EvtRenderEventXml,0,nullptr,&bytes,&properties)||GetLastError()!=ERROR_INSUFFICIENT_BUFFER||!bytes)throw std::runtime_error("Windows event source: event XML sizing failed");
    if(bytes>64U*1024*1024)throw std::length_error("Windows event source: event XML exceeds 64 MiB");
    std::vector<wchar_t> buffer((bytes+sizeof(wchar_t)-1)/sizeof(wchar_t));
    if(!EvtRender(nullptr,event,EvtRenderEventXml,bytes,buffer.data(),&bytes,&properties))throw std::runtime_error("Windows event source: EvtRender failed ("+std::to_string(GetLastError())+")");
    const auto length=wcsnlen_s(buffer.data(),buffer.size());return utf8(buffer.data(),static_cast<int>(length));
  };
}
#endif
}  // namespace
WindowsEventCapture capture_windows_events(const WindowsEventSourceOptions& o,WindowsEventNext next) {
  if(!o.max_events||o.max_events>10'000'000||!o.timeout_ms||o.timeout_ms>30000)throw std::invalid_argument("Windows event source: invalid event limit or timeout");
  if(o.query.empty()||o.query.size()>65536||o.query.find('\0')!=std::string::npos)throw std::invalid_argument("Windows event source: query must contain 1 through 65536 UTF-8 bytes without NUL");
  if(o.kind==WindowsEventSourceKind::kChannel){if(o.channel.empty()||o.channel.size()>4096||o.channel.find('\0')!=std::string::npos||!o.path.empty())throw std::invalid_argument("Windows event source: one explicit local channel is required");}
  else if(o.path.empty()||!o.channel.empty())throw std::invalid_argument("Windows event source: EVTX requires a file and no channel");
  if(!next){
#ifdef _WIN32
    next=native_reader(o);
#else
    throw std::runtime_error("Windows event source: native channel/EVTX acquisition requires Windows");
#endif
  }
  WindowsEventCapture result{o.kind,o.channel,o.query,o.reverse,0,false,"<Events>\n"};
  while(result.events<o.max_events){auto xml=next();if(!xml)break;if(xml->empty())throw std::invalid_argument("Windows event source: empty event XML fragment");if(result.xml.size()+xml->size()>512ULL*1024*1024-10)throw std::length_error("Windows event source: captured XML exceeds 512 MiB");result.xml+=*xml;result.xml+='\n';++result.events;}
  if(result.events==o.max_events)result.limited=next().has_value();
  result.xml+="</Events>";return result;
}
std::string windows_event_capture_json(const WindowsEventCapture& c) {
  std::ostringstream out;out<<"{\"driver\":\""<<(c.kind==WindowsEventSourceKind::kChannel?"windows_event_service":"windows_evtx")<<"\",\"channel\":"<<quote(c.channel)<<",\"query\":"<<quote(c.query)<<",\"reverse\":"<<(c.reverse?"true":"false")<<",\"captured_events\":"<<c.events<<",\"limited\":"<<(c.limited?"true":"false")<<",\"has_xml_capture\":true,\"xml_bytes\":"<<c.xml.size()<<'}';return out.str();
}
}  // namespace minera
