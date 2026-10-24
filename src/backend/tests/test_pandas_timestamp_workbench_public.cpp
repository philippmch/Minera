#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <set>
#include <thread>
#include <vector>
#include "httplib.h"
#include "../tools/table_timestamp_filter_route.hpp"
#include "../polars_ltl_original_call_parser.hpp"

// Reuse the unchanged admitted oracle for its independently mapped constructors.
// Its renamed main is never invoked; retained_pandas_timestamp_filter_cases() runs its assertions.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4715)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#endif
#define main pandas_timestamp_oracle_unused_entrypoint
#include "test_pandas_timestamp_filter.cpp"
#undef main
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace {
namespace tj=minera::polars_ltl_original_call_detail;
using PandasTimestampInvoke=std::function<DiscoveryWorkbenchResponse(const std::vector<DiscoveryWorkbenchPart>&)>;
void timestamp_need(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
const tj::Node& timestamp_at(const tj::Node& n,std::string_view key){const auto* v=tj::field(n,key);timestamp_need(v!=nullptr,"missing envelope field "+std::string(key));return *v;}
std::string timestamp_q(std::string_view s){return serialize_embedding_workbench_string_json(s);}
std::string timestamp_cell(const EventAttribute& a){EventTable table;table.columns={"value"};table.dtypes={"object"};table.rows={{a}};return serialize_embedding_workbench_table_json(table);}
std::string timestamp_text(const tj::Node& node){
 if(node.kind==tj::Node::kString)return node.text;
 timestamp_need(node.kind==tj::Node::kObject&&node.fields.size()==2,"not a native string");
 timestamp_need(timestamp_at(node,"native_bytes").text=="hex-v1","not native hex bytes");
 const auto& hex=timestamp_at(node,"data").text;timestamp_need(hex.size()%2==0,"bad native hex length");std::string out;
 for(std::size_t i=0;i<hex.size();i+=2){const auto digit=[](char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::runtime_error("bad native hex");};out+=static_cast<char>(16*digit(hex[i])+digit(hex[i+1]));}return out;
}
bool timestamp_equal(const tj::Node& a,std::string_view ar,const tj::Node& b,std::string_view br){
 if(a.kind!=b.kind)return false;
 if(a.kind==tj::Node::kString)return a.text==b.text;
 if(a.kind==tj::Node::kBoolean)return a.boolean==b.boolean;
 if(a.kind==tj::Node::kNull)return true;
 if(a.kind==tj::Node::kNumber)return ar.substr(a.begin,a.end-a.begin)==br.substr(b.begin,b.end-b.begin);
 if(a.kind==tj::Node::kArray){if(a.items.size()!=b.items.size())return false;for(std::size_t i=0;i<a.items.size();++i)if(!timestamp_equal(a.items[i],ar,b.items[i],br))return false;return true;}
 if(a.fields.size()!=b.fields.size())return false;
 for(const auto& [k,v]:a.fields){const auto* other=tj::field(b,k);if(!other||!timestamp_equal(v,ar,*other,br))return false;}return true;
}
void timestamp_json_equal(const std::string& a,const std::string& b,const std::string& label){timestamp_need(timestamp_equal(tj::Reader(a).read(),a,tj::Reader(b).read(),b),label);}
// Independent serialization of the actual native operands; never parsed back from the product descriptor.
std::string timestamp_controls(const std::vector<EventAttribute>& operands,const EventAttribute* raw,const std::vector<PandasTimestampFilterParameter>* entries,const std::string& state,const std::vector<std::string>& names){
 std::string args="[";for(std::size_t i=0;i<operands.size();++i){if(i)args+=',';args+="{\"name\":"+timestamp_q(names.at(i))+",\"position\":"+std::to_string(i)+",\"value_json\":"+timestamp_q(timestamp_cell(operands[i]))+'}';}args+=']';
 std::string parameters;
 if(raw)parameters="{\"kind\":\"value\",\"value_json\":"+timestamp_q(timestamp_cell(*raw))+'}';
 else{parameters="{\"kind\":\"parameter_entries\",\"entries\":[";for(std::size_t i=0;i<entries->size();++i){const auto& e=entries->at(i);if(i)parameters+=',';parameters+="{\"key\":"+timestamp_q(e.value.key)+",\"enum_key\":"+(e.enum_key?"true":"false")+",\"value_json\":"+timestamp_q(timestamp_cell(e.value))+'}';}parameters+="]}";}
 return "{\"schema\":\"minera-pandas-timestamp-native-controls-v1\",\"arguments\":"+args+",\"parameters\":"+parameters+",\"parameter_state\":"+timestamp_q(state)+",\"parameter_native_arm\":"+timestamp_q(raw?"raw_attribute":"ordered_parameter_entries")+'}';
}
void timestamp_reopen_table(const std::string& text){timestamp_need(serialize_embedding_workbench_table_json(parse_embedding_workbench_table_json(text))==text,"complete native table reopen changed");}
void timestamp_reopen_controls(const std::string& raw){
 const auto controls=tj::Reader(raw).read();
 for(const auto& a:timestamp_at(controls,"arguments").items)timestamp_reopen_table(timestamp_text(timestamp_at(a,"value_json")));
 const auto& parameters=timestamp_at(controls,"parameters");
 if(timestamp_at(parameters,"kind").text=="value")timestamp_reopen_table(timestamp_text(timestamp_at(parameters,"value_json")));
 else for(const auto& e:timestamp_at(parameters,"entries").items)timestamp_reopen_table(timestamp_text(timestamp_at(e,"value_json")));
}
void pandas_timestamp_transport_check(const PandasTimestampInvoke& invoke,std::size_t index,const std::string& operation,const std::string& call,const EventTable& source,const std::vector<EventAttribute>& operands,const EventAttribute* raw,const std::vector<PandasTimestampFilterParameter>* entries,const std::string& state,const std::vector<std::string>& names,const std::string& projection,const std::optional<EventTable>& expected_table,const std::optional<std::string>& expected_type,const std::optional<std::string>& expected_message,const std::function<EventTable()>& native){
 const auto label=std::to_string(index)+" "+operation+": ";
 const auto before=serialize_embedding_workbench_table_json(source),controls=timestamp_controls(operands,raw,entries,state,names);
 std::optional<EventTable> actual;std::optional<std::string> error_type,error;
 try{actual=native();}catch(const PandasTimestampFilterError& e){error_type=e.type;error=e.message;}
 timestamp_need(serialize_embedding_workbench_table_json(source)==before,label+"independent native source mutated");
 timestamp_need(error_type==expected_type&&error==expected_message,label+"independent native full error mismatch");
 timestamp_need(actual.has_value()==expected_table.has_value(),label+"independent native return state mismatch");
 const auto expected=expected_table?serialize_embedding_workbench_table_json(*expected_table):std::string{};
 if(actual)timestamp_need(serialize_embedding_workbench_table_json(*actual)==expected,label+"independent full native table mismatch");
 const auto response=invoke({{"engine","","pandas",false},{"operation","",operation,false},{"source_file","actual-native.json",before,true},{"call_json","call_json.txt",call,true}});
 const auto root=tj::Reader(response.json).read();
 const std::set<std::string> keys={"family","engine","operation","http_status","phase","apply_returned","source","result","source_unchanged","case_id_key","attribute_key","timestamp_key","source_table_json","filtered_table_json","error_type","error","call_mode","call_json","controls_before_json","controls_after_json","controls_unchanged","source_after_table_json","call_projection_json"};
 timestamp_need(root.fields.size()==keys.size(),label+"not exact23 envelope");for(const auto& [name,value]:root.fields){(void)value;timestamp_need(keys.contains(name),label+"unknown envelope key");}
 const auto node=[&](std::string_view key)->const tj::Node&{return timestamp_at(root,key);};
 const auto text=[&](std::string_view key){return timestamp_text(node(key));};
 const auto nil=[&](std::string_view key){timestamp_need(node(key).kind==tj::Node::kNull,label+"expected null "+std::string(key));};
 const auto flag=[&](std::string_view key,bool value){timestamp_need(node(key).kind==tj::Node::kBoolean&&node(key).boolean==value,label+"wrong flag "+std::string(key));};
 const auto raw_json=[&](std::string_view key){const auto& n=node(key);return std::string(response.json.substr(n.begin,n.end-n.begin));};
 const auto counts=[](const EventTable& t){return "{\"rows\":"+std::to_string(t.rows.size())+",\"columns\":"+std::to_string(t.columns.size())+'}';};
 timestamp_need(response.status==(expected_type?400:200)&&raw_json("http_status")==std::to_string(response.status),label+"wrong status");
 timestamp_need(text("family")=="TABLE_TIMESTAMP_FILTER"&&text("engine")=="pandas"&&text("operation")==operation&&text("call_mode")=="original"&&text("call_json")==call,label+"family/call echo mismatch");
 timestamp_need(text("phase")==(expected_type?"apply":"complete"),label+"wrong phase");flag("apply_returned",!expected_type.has_value());
 for(const auto key:{"case_id_key","attribute_key","timestamp_key"})nil(key);
 timestamp_need(text("source_table_json")==before&&text("source_after_table_json")==before,label+"complete source before/after mismatch");flag("source_unchanged",true);
 timestamp_json_equal(raw_json("source"),counts(source),label+"source count mismatch");
 timestamp_json_equal(text("controls_before_json"),controls,label+"actual controls-before mismatch");timestamp_json_equal(text("controls_after_json"),controls,label+"actual controls-after mismatch");flag("controls_unchanged",true);
 timestamp_json_equal(text("call_projection_json"),projection,label+"full projection metadata mismatch");
 if(expected_type){timestamp_need(text("error_type")==*expected_type&&text("error")==*expected_message,label+"full native diagnostic mismatch");nil("result");nil("filtered_table_json");}
 else{nil("error_type");nil("error");timestamp_need(text("filtered_table_json")==expected,label+"full returned native table mismatch");timestamp_json_equal(raw_json("result"),counts(*expected_table),label+"result count mismatch");timestamp_reopen_table(text("filtered_table_json"));}
 timestamp_reopen_table(text("source_table_json"));timestamp_reopen_controls(text("controls_before_json"));timestamp_reopen_controls(text("controls_after_json"));
 (void)tj::Reader(text("call_json")).read();(void)tj::Reader(text("call_projection_json")).read();
}
#include "pandas_timestamp_workbench_public_vectors.inc"

class PandasTimestampOriginalCallHttpServer {
 public:
  PandasTimestampOriginalCallHttpServer() {
    server_.Post("/api/reference-table-timestamp", handle_table_timestamp_filter);
    port_ = server_.bind_to_any_port("127.0.0.1");
    if (port_ <= 0) throw std::runtime_error("pandas timestamp original-call owned HTTP bind failed");
    worker_ = std::thread([this] { listened_ = server_.listen_after_bind(); finished_ = true; });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!server_.is_running() && !finished_ && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!server_.is_running()) { stop(); throw std::runtime_error("pandas timestamp owned HTTP did not start"); }
  }
  ~PandasTimestampOriginalCallHttpServer() { stop(); }
  void stop() { server_.stop(); if (worker_.joinable()) worker_.join(); }
  int port() const { return port_; }
  bool listened() const { return listened_; }
 private:
  httplib::Server server_;
  int port_ = -1;
  std::thread worker_;
  std::atomic<bool> finished_ = false, listened_ = false;
};
DiscoveryWorkbenchResponse pandas_timestamp_original_call_http(httplib::Client& client,
    const std::vector<DiscoveryWorkbenchPart>& parts) {
  const std::string boundary = "minera-pandas-timestamp-original-call-retained-public-boundary";
  std::string body;
  for (const auto& part : parts) {
    body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + part.name + '"';
    if (part.file) body += "; filename=\"" + part.filename + '"';
    body += "\r\nContent-Type: application/octet-stream\r\n\r\n" + part.bytes + "\r\n";
  }
  body += "--" + boundary + "--\r\n";
  const auto response = client.Post("/api/reference-table-timestamp", body, "multipart/form-data; boundary=" + boundary);
  if (!response || !response->get_header_value("Content-Type").starts_with("application/json"))
    throw std::runtime_error("pandas timestamp original-call owned HTTP response missing");
  return {response->status, response->body};
}
}

int main() {
  try {
    retained_pandas_timestamp_filter_cases();
    retained_pandas_timestamp_transport_cases([](const auto& parts) { return execute_table_timestamp_filter(parts); });
    PandasTimestampOriginalCallHttpServer owned;
    httplib::Client client("127.0.0.1", owned.port());
    client.set_connection_timeout(5, 0); client.set_read_timeout(10, 0);
    retained_pandas_timestamp_transport_cases([&](const auto& parts) { return pandas_timestamp_original_call_http(client, parts); });
    owned.stop();
    if (!owned.listened()) throw std::runtime_error("pandas timestamp original-call owned HTTP listener failed");
    std::cout << "861 original native +861 direct +861 owned HTTP pandas timestamp calls; all23 fields, actual controls and artifact reopen passed\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
