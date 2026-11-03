#include "minera/source_acquisition.hpp"
#include "secure_temp.hpp"
#include "minera/browser_history.hpp"
#include "minera/communication_history.hpp"
#include "minera/enterprise_history.hpp"
#include "minera/log_conversion.hpp"
#include "minera/request_params.hpp"
#include "minera/windows_event_log.hpp"
#include "minera/workflow_history.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/exact_filter_discovery_json.hpp"
#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <locale>
#include <variant>
#include <vector>
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
  // A private (0700, unpredictable) directory, removed on destruction and on
  // SIGTERM/SIGINT; see secure_temp.hpp.
  explicit UploadedFile(const std::string& bytes):directory_("minera-source-") {
    path_=directory_.file("original.data");std::ofstream output(path_,std::ios::binary);output.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));if(!output)throw std::runtime_error("source acquisition: could not materialize uploaded file");
  }
  const std::filesystem::path& path() const {return path_;}
 private:secure_temp::PrivateDirectory directory_;std::filesystem::path path_;
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
  if(present("outlook_source")) {
    if((input.connector!="outlook_mail_extractor"&&input.connector!="outlook_calendar")||field("outlook_source")!="com")throw std::invalid_argument("Outlook acquisition requires an Outlook connector and com source");
    if(result.original_bytes||!input.server_path.empty())throw std::invalid_argument("Outlook COM acquisition does not accept an upload or server path");
    OutlookSourceOptions options;options.history=parse_communication_history_params(field,present);if(!present("communication_max_items"))options.history.max_items=1000;options.email_user=field("outlook_email_user");
    if(present("outlook_calendar_id")){const auto value=field("outlook_calendar_id");const auto parsed=std::from_chars(value.data(),value.data()+value.size(),options.calendar_id);if(value.empty()||parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size())throw std::invalid_argument("outlook_calendar_id requires a positive folder number");}
#ifdef _WIN32
    if(input.outlook_application)result.outlook_capture=capture_outlook_source_with_dispatch(options,input.outlook_application);else
#endif
    result.outlook_capture=capture_outlook_source(options);result.log=result.outlook_capture->log;result.native_result_json=communication_history_json(result.log,options.history.variant);
  }else if(present("database_driver")) {
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
  if(result.outlook_capture)out<<",\"native_acquisition\":"<<outlook_source_capture_json(*result.outlook_capture)<<",\"outlook_tables\":{\"items\":"<<result.outlook_capture->item_table_json<<",\"formatted\":"<<result.outlook_capture->formatted_table_json<<'}';
  out<<'}';return out.str();
}
namespace {
[[noreturn]] void invalid(std::string message) {
  throw std::invalid_argument("Exact source acquisition JSON: " + message);
}

struct Number { std::string text; };
struct Json {
  using Array = std::vector<Json>;
  using Object = std::map<std::string, Json, std::less<>>;
  std::variant<std::nullptr_t, bool, Number, std::string, Array, Object> value;
  bool null() const { return std::holds_alternative<std::nullptr_t>(value); }
  const Object& object() const {
    if (const auto* result = std::get_if<Object>(&value)) return *result;
    invalid("expected an object");
  }
  const Array& array() const {
    if (const auto* result = std::get_if<Array>(&value)) return *result;
    invalid("expected an array");
  }
  std::string string() const {
    if (const auto* result = std::get_if<std::string>(&value)) return *result;
    const auto* object = std::get_if<Object>(&value);
    if(!object||object->size()!=2||!object->contains("native_bytes")||!object->contains("data"))invalid("expected a NativeString");
    const auto* tag=std::get_if<std::string>(&object->at("native_bytes").value);const auto* hex=std::get_if<std::string>(&object->at("data").value);
    if(!tag||*tag!="hex-v1"||!hex||hex->size()%2)invalid("invalid NativeString byte carrier");
    const auto digit=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};std::string out;out.reserve(hex->size()/2);
    for(std::size_t i=0;i<hex->size();i+=2){const int a=digit((*hex)[i]),b=digit((*hex)[i+1]);if(a<0||b<0)invalid("invalid NativeString hexadecimal bytes");out+=static_cast<char>((a<<4)|b);}return out;
  }
  bool boolean() const {
    if (const auto* result = std::get_if<bool>(&value)) return *result;
    invalid("expected a boolean");
  }
};

void append_utf8(std::string& output, std::uint32_t value) {
  if (value <= 0x7f) output += static_cast<char>(value);
  else if (value <= 0x7ff) {
    output += static_cast<char>(0xc0 | (value >> 6));
    output += static_cast<char>(0x80 | (value & 63));
  } else if (value <= 0xffff) {
    output += static_cast<char>(0xe0 | (value >> 12));
    output += static_cast<char>(0x80 | ((value >> 6) & 63));
    output += static_cast<char>(0x80 | (value & 63));
  } else {
    output += static_cast<char>(0xf0 | (value >> 18));
    output += static_cast<char>(0x80 | ((value >> 12) & 63));
    output += static_cast<char>(0x80 | ((value >> 6) & 63));
    output += static_cast<char>(0x80 | (value & 63));
  }
}

// This parser retains numeric tokens rather than rounding JSON integers to
// double. Grammar, UTF8/WTF8 and duplicate keys are checked; there is no
// independent byte/node/depth cap beyond transport and process resources.
class Parser {
 public:
  explicit Parser(std::string_view text):text_(text){}
  Json parse() {
    auto result = value(0);
    whitespace();
    if (position_ != text_.size()) invalid("trailing data");
    return result;
  }
 private:
  bool digit() const { return position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9'; }
  void whitespace() {
    while (position_ < text_.size() && (text_[position_] == ' ' || text_[position_] == '\t' || text_[position_] == '\n' || text_[position_] == '\r')) ++position_;
  }
  bool take(char c) {
    whitespace();
    if (position_ < text_.size() && text_[position_] == c) { ++position_; return true; }
    return false;
  }
  void require(char c) { if (!take(c)) invalid("unexpected character"); }
  std::uint32_t hex4() {
    std::uint32_t result = 0;
    for (int i = 0; i < 4; ++i) {
      if (position_ == text_.size()) invalid("short Unicode escape");
      const char c = text_[position_++];
      const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
      if (d < 0) invalid("invalid Unicode escape");
      result = (result << 4) | static_cast<unsigned>(d);
    }
    return result;
  }
  std::string string() {
    require('"');
    std::string result;
    while (position_ < text_.size()) {
      const auto c = static_cast<unsigned char>(text_[position_++]);
      if (c == '"') return result;
      if (c < 0x20) invalid("unescaped control character");
      if (c >= 0x80) {
        const int count = c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 : c >= 0xf0 && c <= 0xf4 ? 3 : -1;
        if (count < 0) invalid("invalid UTF-8");
        std::uint32_t code = c & (count == 1 ? 31 : count == 2 ? 15 : 7);
        for (int i = 0; i < count; ++i) {
          if (position_ == text_.size()) invalid("short UTF-8 sequence");
          const auto next = static_cast<unsigned char>(text_[position_++]);
          if ((next & 0xc0) != 0x80) invalid("invalid UTF-8 continuation");
          code = (code << 6) | (next & 63);
        }
        if ((count == 1 && code < 0x80) || (count == 2 && code < 0x800) || (count == 3 && code < 0x10000) || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff && !escaped_lone_surrogates_)) invalid("invalid UTF-8 scalar");
        append_utf8(result, code);
        continue;
      }
      if (c != '\\') { result += static_cast<char>(c); continue; }
      if (position_ == text_.size()) invalid("unterminated escape");
      switch (text_[position_++]) {
        case '"': result += '"'; break;
        case '\\': result += '\\'; break;
        case '/': result += '/'; break;
        case 'b': result += '\b'; break;
        case 'f': result += '\f'; break;
        case 'n': result += '\n'; break;
        case 'r': result += '\r'; break;
        case 't': result += '\t'; break;
        case 'u': {
          auto code = hex4();
          if (code >= 0xd800 && code <= 0xdbff) {
            if (position_ + 2 <= text_.size() && text_[position_] == '\\' && text_[position_ + 1] == 'u') {
              const auto saved = position_;
              position_ += 2;
              const auto low = hex4();
              if (low >= 0xdc00 && low <= 0xdfff)
                code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
              else {
                if (!escaped_lone_surrogates_) invalid("unpaired Unicode surrogate");
                position_ = saved;
              }
            } else if (!escaped_lone_surrogates_) invalid("unpaired Unicode surrogate");
          } else if (code >= 0xdc00 && code <= 0xdfff && !escaped_lone_surrogates_)
            invalid("unpaired Unicode surrogate");
          append_utf8(result, code);
          break;
        }
        default: invalid("unknown escape");
      }
    }
    invalid("unterminated string");
  }
  Json value(unsigned depth) {
    whitespace();
    if (position_ == text_.size()) invalid("unexpected end of input");
    const auto c = text_[position_];
    if (c == '"') return Json{string()};
    if (take('{')) {
      Json::Object result;
      if (!take('}')) {
        do {
          auto key = string();
          require(':');
          if (!result.emplace(std::move(key), value(depth + 1)).second) invalid("duplicate object key");
        } while (take(','));
        require('}');
      }
      return Json{std::move(result)};
    }
    if (take('[')) {
      Json::Array result;
      if (!take(']')) {
        do { result.push_back(value(depth + 1)); } while (take(','));
        require(']');
      }
      return Json{std::move(result)};
    }
    for (const auto token : {std::string_view("null"), std::string_view("true"), std::string_view("false")}) {
      if (text_.substr(position_, token.size()) == token) {
        position_ += token.size();
        if (token == "null") return Json{nullptr};
        return Json{token == "true"};
      }
    }
    const auto start = position_;
    if (text_[position_] == '-') ++position_;
    if (!digit()) invalid("expected a value");
    if (text_[position_] == '0') ++position_;
    else while (digit()) ++position_;
    if (position_ < text_.size() && text_[position_] == '.') {
      ++position_;
      if (!digit()) invalid("missing fractional digits");
      while (digit()) ++position_;
    }
    if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
      ++position_;
      if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) ++position_;
      if (!digit()) invalid("missing exponent digits");
      while (digit()) ++position_;
    }
    return Json{Number{std::string(text_.substr(start, position_ - start))}};
  }
  std::string_view text_;
  bool escaped_lone_surrogates_ = true;
  std::size_t position_ = 0;
};

void allowed_fields(const Json::Object& object,
                    std::initializer_list<std::string_view> allowed) {
  for (const auto& [name, value] : object) {
    (void)value;
    if (std::find(allowed.begin(), allowed.end(), name) == allowed.end())
      invalid("unknown field " + name);
  }
}

const Json* field(const Json::Object& object, std::string_view name) {
  const auto found = object.find(name);
  return found == object.end() ? nullptr : &found->second;
}

int native_int(const Json& value) {
  const auto* number = std::get_if<Number>(&value.value);
  if (!number || number->text.empty() ||
      number->text.find_first_of(".eE") != std::string::npos)
    invalid("expected an exact native int token");
  int result = 0;
  const auto parsed = std::from_chars(number->text.data(),
      number->text.data() + number->text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != number->text.data() + number->text.size())
    invalid("integer exceeds native int storage range");
  return result;
}


std::string exact_string(std::string_view value) {
  return serialize_embedding_workbench_string_json(value);
}
std::string exact_log(const EventLog& value) {
  return exact_source_snapshot(EventDataView{value});
}
std::string exact_windows(const WindowsEventCapture& value) {
  const auto index = static_cast<int>(value.kind);
  const auto name = index == 0 ? "\"kChannel\"" : index == 1 ? "\"kEvtx\"" : "null";
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << "{\"kind\":{\"name\":" << name << ",\"index\":" << index
      << "},\"channel\":" << exact_string(value.channel)
      << ",\"query\":" << exact_string(value.query)
      << ",\"reverse\":" << (value.reverse ? "true" : "false")
      << ",\"events\":" << value.events
      << ",\"limited\":" << (value.limited ? "true" : "false")
      << ",\"xml\":" << exact_string(value.xml) << '}';
  return out.str();
}
std::string exact_database_table(const DatabaseHistoryTable& value) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  const auto strings = [&](const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i) out << ',';
      out << exact_string(values[i]);
    }
    out << ']';
  };
  out << "{\"columns\":"; strings(value.columns);
  out << ",\"column_types\":"; strings(value.column_types);
  out << ",\"rows\":[";
  for (std::size_t i = 0; i < value.rows.size(); ++i) {
    if (i) out << ',';
    out << '[';
    for (std::size_t j = 0; j < value.rows[i].size(); ++j) {
      if (j) out << ',';
      out << (value.rows[i][j] ? exact_string(*value.rows[i][j]) : "null");
    }
    out << ']';
  }
  out << "],\"limited\":" << (value.limited ? "true" : "false")
      << ",\"query_timeout_applied\":" << (value.query_timeout_applied ? "true" : "false") << '}';
  return out.str();
}
std::string exact_database(const DatabaseHistoryCapture& value) {
  return "{\"connector\":" + exact_string(value.connector) +
      ",\"driver\":" + exact_string(value.driver) +
      ",\"prefix\":" + exact_string(value.prefix) +
      ",\"dialect\":" + exact_string(value.dialect) +
      ",\"query\":" + exact_string(value.query) +
      ",\"reference_query\":" + exact_string(value.reference_query) +
      ",\"table\":" + exact_database_table(value.table) +
      ",\"log\":" + exact_log(value.log) +
      ",\"raw_table_json\":" + exact_string(value.raw_table_json) +
      ",\"formatted_table_json\":" + exact_string(value.formatted_table_json) + '}';
}
std::string exact_outlook(const OutlookSourceCapture& value) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << "{\"log\":" << exact_log(value.log)
      << ",\"connector\":" << exact_string(value.connector)
      << ",\"email_user\":" << exact_string(value.email_user)
      << ",\"calendar_id\":" << value.calendar_id
      << ",\"total_items\":" << value.total_items
      << ",\"examined_items\":" << value.examined_items
      << ",\"skipped_items\":" << value.skipped_items
      << ",\"limited\":" << (value.limited ? "true" : "false")
      << ",\"item_table_json\":" << exact_string(value.item_table_json)
      << ",\"formatted_table_json\":" << exact_string(value.formatted_table_json) << '}';
  return out.str();
}
template <class Call>
std::string exact_artifact(Call call) {
  try { return "{\"bytes\":" + exact_string(call()) + ",\"error\":null}"; }
  catch (const std::exception& error) {
    return "{\"bytes\":null,\"error\":" + exact_string(error.what()) + '}';
  }
}
constexpr std::string_view missing_artifact = "{\"bytes\":null,\"error\":null}";
}  // namespace

std::string source_acquisition_result_exact_json(const SourceAcquisitionResult& result) {
  return "{\"connector\":" + exact_string(result.connector) +
      ",\"log\":" + exact_log(result.log) +
      ",\"native_result_json\":" + exact_string(result.native_result_json) +
      ",\"original_bytes\":" + (result.original_bytes ? exact_string(*result.original_bytes) : "null") +
      ",\"windows_capture\":" + (result.windows_capture ? exact_windows(*result.windows_capture) : "null") +
      ",\"database_capture\":" + (result.database_capture ? exact_database(*result.database_capture) : "null") +
      ",\"outlook_capture\":" + (result.outlook_capture ? exact_outlook(*result.outlook_capture) : "null") + '}';
}
std::string source_acquisition_artifacts_json(const SourceAcquisitionResult& result) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << "{\"source_acquisition_json\":" << exact_artifact([&] { return source_acquisition_json(result); })
      << ",\"log_conversion_json\":" << exact_artifact([&] {
           LogConversionResult converted; converted.log = result.log; return log_conversion_json(converted);
         });
  const auto artifact = [&](std::string_view name, bool present, auto call) {
    out << ',' << exact_string(name) << ':' << (present ? exact_artifact(call) : std::string(missing_artifact));
  };
  artifact("windows_event_capture_json", result.windows_capture.has_value(), [&] { return windows_event_capture_json(*result.windows_capture); });
  artifact("database_history_capture_json", result.database_capture.has_value(), [&] { return database_history_capture_json(*result.database_capture); });
  artifact("outlook_source_capture_json", result.outlook_capture.has_value(), [&] { return outlook_source_capture_json(*result.outlook_capture); });
  artifact("windows_xml", result.windows_capture.has_value(), [&] { return result.windows_capture->xml; });
  artifact("database_raw_table_json", result.database_capture.has_value(), [&] { return result.database_capture->raw_table_json; });
  artifact("database_formatted_table_json", result.database_capture.has_value(), [&] { return result.database_capture->formatted_table_json; });
  artifact("outlook_item_table_json", result.outlook_capture.has_value(), [&] { return result.outlook_capture->item_table_json; });
  artifact("outlook_formatted_table_json", result.outlook_capture.has_value(), [&] { return result.outlook_capture->formatted_table_json; });
  out << '}';
  return out.str();
}
XesExportOptions parse_source_acquisition_xes_options_json(std::string_view input) {
  const auto document = Parser(input).parse();
  const auto& object = document.object();
  allowed_fields(object, {"variant", "compress", "name", "infer_metadata", "preserve_nulls"});
  if (object.size() != 5) invalid("XES options require all five native fields");
  const auto& variant = object.at("variant");
  int index;
  if (std::holds_alternative<Json::Object>(variant.value)) {
    const auto& entry = variant.object();
    allowed_fields(entry, {"name", "index"});
    if (entry.size() != 2 || !entry.contains("name") || !entry.contains("index")) invalid("variant requires name and index");
    if (!entry.at("name").null()) (void)entry.at("name").string();
    index = native_int(entry.at("index"));
  } else index = native_int(variant);
  XesExportOptions options;
  options.variant = static_cast<XesExportVariant>(index);
  if (!object.at("compress").null()) options.compress = object.at("compress").boolean();
  options.name = object.at("name").string();
  options.infer_metadata = object.at("infer_metadata").boolean();
  options.preserve_nulls = object.at("preserve_nulls").boolean();
  return options;
}
std::string source_acquisition_xes_options_json(const XesExportOptions& options) {
  const auto index = static_cast<int>(options.variant);
  const auto name = index == 0 ? "\"LINE_BY_LINE\"" : index == 1 ? "\"ETREE\"" : "null";
  return std::string("{\"variant\":{\"name\":") + name + ",\"index\":" + std::to_string(index) +
      "},\"compress\":" + (options.compress ? (*options.compress ? "true" : "false") : "null") +
      ",\"name\":" + exact_string(options.name) +
      ",\"infer_metadata\":" + (options.infer_metadata ? "true" : "false") +
      ",\"preserve_nulls\":" + (options.preserve_nulls ? "true" : "false") + '}';
}
}  // namespace minera
