#include "minera/source_acquisition.hpp"
#include "minera/browser_history.hpp"
#include "minera/communication_history.hpp"
#include "minera/enterprise_history.hpp"
#include "minera/log_conversion.hpp"
#include "minera/request_params.hpp"
#include "minera/windows_event_log.hpp"
#include "minera/workflow_history.hpp"
#include <atomic>
#include <chrono>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>

namespace minera {
namespace {
constexpr std::size_t maximum_bytes=512ULL*1024*1024;
std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path,std::ios::binary);if(!input)throw std::invalid_argument("source acquisition: could not open the supplied file");
  std::string bytes;char buffer[16384];while(input.read(buffer,sizeof(buffer))||input.gcount()){if(bytes.size()+static_cast<std::size_t>(input.gcount())>maximum_bytes)throw std::length_error("source acquisition: file exceeds 512 MiB");bytes.append(buffer,static_cast<std::size_t>(input.gcount()));}if(input.bad())throw std::runtime_error("source acquisition: source read failed");return bytes;
}
class UploadedFile {
 public:
  explicit UploadedFile(const std::string& bytes) {
    static std::atomic<std::size_t> counter=0;
    const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
    directory_=std::filesystem::temp_directory_path()/("minera-source-"+std::to_string(stamp)+"-"+std::to_string(counter++)+"-"+std::to_string(std::random_device{}()));
    if(!std::filesystem::create_directory(directory_))throw std::runtime_error("source acquisition: temporary directory collision");
    path_=directory_/"original.data";std::ofstream output(path_,std::ios::binary);output.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));if(!output){std::error_code error;std::filesystem::remove(path_,error);std::filesystem::remove(directory_,error);throw std::runtime_error("source acquisition: could not materialize uploaded file");}
  }
  ~UploadedFile(){std::error_code error;std::filesystem::remove(path_,error);std::filesystem::remove(directory_,error);}
  const std::filesystem::path& path() const {return path_;}
 private:std::filesystem::path directory_,path_;
};
}
SourceAcquisitionResult acquire_source(const SourceAcquisitionInput& input) {
  static const std::set<std::string> connectors={"chrome_history","firefox_history","windows_events","github_repo","camunda_workflow","sap_o2c","sap_accounting","outlook_mail_extractor","outlook_calendar"};
  if(!connectors.contains(input.connector))throw std::invalid_argument("source acquisition: unknown connector");
  SourceAcquisitionResult result;result.connector=input.connector;
  auto native_parameters=input.parameters;
  if(input.connector=="sap_o2c"||input.connector=="sap_accounting")native_parameters["enterprise_variant"]=input.connector=="sap_o2c"?"order_to_cash":"accounting";
  if(input.connector=="outlook_mail_extractor"||input.connector=="outlook_calendar")native_parameters["communication_variant"]=input.connector=="outlook_calendar"?"calendar":"mailbox";
  const auto field=[&](std::string_view key){const auto it=native_parameters.find(std::string(key));return it==native_parameters.end()?std::string{}:it->second;};
  const auto present=[&](std::string_view key){return native_parameters.contains(std::string(key));};
  if(input.source_bytes&&!input.server_path.empty())throw std::invalid_argument("source acquisition: choose an upload or an explicit server path");
  result.original_bytes=input.source_bytes;
  std::filesystem::path path=std::filesystem::u8path(input.server_path);
  if(!input.server_path.empty()&&std::filesystem::is_regular_file(path))result.original_bytes=read_file(path);
  if(result.original_bytes&&result.original_bytes->size()>maximum_bytes)throw std::length_error("source acquisition: source exceeds 512 MiB");
  std::unique_ptr<UploadedFile> uploaded;
  if(present("database_driver")) {
    DatabaseHistoryOptions options;options.connector=input.connector;options.driver=field("database_driver");options.prefix=field("database_prefix");options.dialect=field("database_dialect");options.connection_string=input.database_connection;
    if(options.driver=="sqlite"){if(input.server_path.empty()&&result.original_bytes){uploaded=std::make_unique<UploadedFile>(*result.original_bytes);path=uploaded->path();}options.path=path;}
    else if(result.original_bytes||!input.server_path.empty())throw std::invalid_argument("ODBC acquisition does not accept an upload or server file path");
    const auto number=[&](std::string_view key,std::size_t limit,std::size_t fallback){if(!present(key))return fallback;const auto value=field(key);std::size_t n=0;const auto parsed=std::from_chars(value.data(),value.data()+value.size(),n);if(value.empty()||parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size()||!n||n>limit)throw std::invalid_argument(std::string(key)+" is outside its native limit");return n;};
    options.max_rows=number("database_max_rows",10000000,1000);const auto event_key=input.connector=="camunda_workflow"?"workflow_max_events":"enterprise_max_events";options.max_rows=std::min(options.max_rows,number(event_key,10000000,10000000));options.timeout_seconds=static_cast<unsigned>(number("database_timeout_seconds",300,30));
    result.database_capture=capture_database_history(options,input.database_reader);result.log=result.database_capture->log;
    if(input.connector=="camunda_workflow")result.native_result_json=workflow_history_json(result.log);
    else result.native_result_json=enterprise_history_json(result.log,input.connector=="sap_o2c"?EnterpriseHistoryVariant::kOrderToCash:EnterpriseHistoryVariant::kAccounting);
  }else if(input.connector=="chrome_history"||input.connector=="firefox_history") {
    if(result.original_bytes){uploaded=std::make_unique<UploadedFile>(*result.original_bytes);path=uploaded->path();}
    if(path.empty())throw std::invalid_argument("source acquisition: browser history requires a database upload or explicit profile path");
    auto params=input.parameters;params["browser_history_variant"]=input.connector;params["browser_history_path"]="explicit-source";
    const auto lookup=[&](std::string_view key){const auto it=params.find(std::string(key));return it==params.end()?std::string{}:it->second;};
    const auto supplied=[&](std::string_view key){return params.contains(std::string(key));};auto options=parse_browser_history_params(lookup,supplied);options.history_path=path;result.log=import_browser_history(options);result.native_result_json=browser_history_json(result.log,options.variant);
  }else if(input.connector=="windows_events") {
    const auto source=present("windows_events_source")?field("windows_events_source"):"xml";
    if(source!="xml"&&source!="evtx"&&source!="service")throw std::invalid_argument("Windows acquisition source requires xml, evtx or service");
    if(source=="xml"&&(present("windows_events_channel")||present("windows_events_query")||present("windows_events_reverse")||present("windows_events_timeout_ms")))throw std::invalid_argument("Windows native query options require service or EVTX acquisition");
    if(source!="xml") {
      WindowsEventSourceOptions capture;capture.kind=source=="service"?WindowsEventSourceKind::kChannel:WindowsEventSourceKind::kEvtx;
      if(source=="service"){if(result.original_bytes||!input.server_path.empty())throw std::invalid_argument("Windows service acquisition does not accept a file or path");capture.channel=field("windows_events_channel");}
      else {if(present("windows_events_channel"))throw std::invalid_argument("EVTX acquisition does not accept a channel");if(result.original_bytes){uploaded=std::make_unique<UploadedFile>(*result.original_bytes);path=uploaded->path();}capture.path=path;}
      if(present("windows_events_query"))capture.query=field("windows_events_query");
      if(present("windows_events_reverse")){const auto v=field("windows_events_reverse");if(v!="true"&&v!="false")throw std::invalid_argument("windows_events_reverse requires true or false");capture.reverse=v=="true";}
      const auto number=[&](std::string_view key,std::size_t maximum,std::size_t fallback){if(!present(key))return fallback;const auto value=field(key);std::size_t n=0;const auto p=std::from_chars(value.data(),value.data()+value.size(),n);if(value.empty()||p.ec!=std::errc{}||p.ptr!=value.data()+value.size()||n==0||n>maximum)throw std::invalid_argument(std::string(key)+" is outside its native limit");return n;};
      capture.max_events=number("windows_events_max_events",10'000'000,1000);capture.timeout_ms=static_cast<unsigned>(number("windows_events_timeout_ms",30000,1000));
      result.windows_capture=capture_windows_events(capture,input.windows_event_reader);uploaded=std::make_unique<UploadedFile>(result.windows_capture->xml);path=uploaded->path();
    }else if(result.original_bytes){uploaded=std::make_unique<UploadedFile>(*result.original_bytes);path=uploaded->path();}
    if(path.empty())throw std::invalid_argument("source acquisition: Windows events require an exported XML upload or explicit file path");
    auto params=input.parameters;params["windows_events_path"]="explicit-source";const auto lookup=[&](std::string_view key){const auto it=params.find(std::string(key));return it==params.end()?std::string{}:it->second;};const auto supplied=[&](std::string_view key){return params.contains(std::string(key));};auto options=parse_windows_event_log_params(lookup,supplied);options.path=path;result.log=import_windows_event_log(options);result.native_result_json=windows_event_log_json(result.log);
  }else if(input.connector=="github_repo") {
    if(input.source_bytes||!input.server_path.empty())throw std::invalid_argument("source acquisition: repository acquisition uses its configured API");
    auto options=parse_repository_history_params(field,present);options.auth_token=input.auth_token;options.transport=input.repository_transport;result.log=import_repository_history(options);result.native_result_json=repository_history_json(result.log,options.owner,options.repository);
  }else {
    if(!result.original_bytes)throw std::invalid_argument("source acquisition: this connector requires original CSV data");
    if(input.connector=="camunda_workflow"){auto options=parse_workflow_history_params(field,present);result.log=import_workflow_history_csv(*result.original_bytes,options);result.native_result_json=workflow_history_json(result.log);}
    else if(input.connector=="sap_o2c"||input.connector=="sap_accounting") {
      auto options=parse_enterprise_history_params(field,present);options.variant=input.connector=="sap_o2c"?EnterpriseHistoryVariant::kOrderToCash:EnterpriseHistoryVariant::kAccounting;result.log=import_enterprise_history_csv(*result.original_bytes,options);result.native_result_json=enterprise_history_json(result.log,options.variant);
    }else if(input.connector=="outlook_mail_extractor"||input.connector=="outlook_calendar") {
      auto options=parse_communication_history_params(field,present);options.variant=input.connector=="outlook_calendar"?CommunicationHistoryVariant::kCalendar:CommunicationHistoryVariant::kMailbox;result.log=import_communication_history_csv(*result.original_bytes,options);result.native_result_json=communication_history_json(result.log,options.variant);
    }else throw std::invalid_argument("source acquisition: unknown connector");
  }
  return result;
}
std::string source_acquisition_json(const SourceAcquisitionResult& result) {
  // Connector names are selected from the fixed native registry above.
  LogConversionResult full;full.log=result.log;std::ostringstream out;
  out<<"{\"family\":\"SOURCE_ACQUISITION\",\"connector\":\""<<result.connector<<"\",\"trace_count\":"<<result.log.trace_count()<<",\"event_count\":"<<result.log.event_count()<<",\"native_result\":"<<result.native_result_json<<",\"native_log\":"<<log_conversion_json(full);
  if(result.windows_capture)out<<",\"native_acquisition\":"<<windows_event_capture_json(*result.windows_capture);
  if(result.database_capture)out<<",\"native_acquisition\":"<<database_history_capture_json(*result.database_capture);
  out<<'}';return out.str();
}
}  // namespace minera
