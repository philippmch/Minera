#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <cmath>
#include <stdexcept>
#include <string>
#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "minera/polars_ltl_filter.hpp"
#include "minera/token_integer.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include "httplib.h"
#include "../tools/ltl_filter_route.hpp"

namespace {
using namespace minera;

EventAttribute null_value(std::string key) {
  EventAttribute value{std::move(key), ""}; value.is_null = true; return value;
}
template <class Integer>
EventAttribute integer_value(std::string key, Integer number) {
  EventAttribute value{std::move(key), std::to_string(number), AttributeKind::kInt};
  value.typed_value = number; return value;
}
EventAttribute float_value(std::string key, double number) {
  EventAttribute value{std::move(key), std::to_string(number), AttributeKind::kFloat};
  value.typed_value = number; return value;
}
EventAttribute boolean_value(std::string key, bool boolean) {
  EventAttribute value{std::move(key), boolean ? "true" : "false", AttributeKind::kBoolean};
  value.typed_value = boolean; return value;
}
EventAttribute date_value(std::string key, std::string instant) {
  EventAttribute value{std::move(key), instant, AttributeKind::kDate};
  value.typed_value = NativeDate{instant}; return value;
}
EventAttribute collection_value(std::string key, AttributeContainerKind kind,
                                std::vector<EventAttribute> children) {
  EventAttribute value{std::move(key), ""}; value.container = kind;
  value.children = std::move(children); return value;
}

void same_frame(const EventTable& actual, const EventTable& expected, const std::string& context) {
  if (actual.columns != expected.columns || actual.dtypes != expected.dtypes || actual.rows != expected.rows ||
      actual.attributes != expected.attributes || actual.metadata != expected.metadata ||
      actual.index != expected.index || actual.categorical_dtypes != expected.categorical_dtypes)
    throw std::runtime_error("Full Polars LTL frame mismatch: " + context);
  for (std::size_t row = 0; row < actual.rows.size(); ++row)
    for (std::size_t column = 0; column < actual.columns.size(); ++column) {
      const auto& a = actual.rows[row][column]; const auto& b = expected.rows[row][column];
      if (!a || !b) continue;
      const auto* left = std::get_if<double>(&a->typed_value);
      const auto* right = std::get_if<double>(&b->typed_value);
      if (left && right && std::bit_cast<std::uint64_t>(*left) != std::bit_cast<std::uint64_t>(*right))
        throw std::runtime_error("Polars LTL floating bits changed: " + context);
    }
}

template <class Action>
void error_is(Action action, const std::string& type, const std::string& phase,
              const std::string& message, const std::string& context) {
  bool thrown = false;
  try { action(); }
  catch (const PolarsLtlFilterError& error) {
    thrown = true;
    if (error.type != type || error.phase != phase || error.message != message) {
      const auto mismatch = std::mismatch(message.begin(), message.end(), error.message.begin(), error.message.end());
      std::cerr << "Wrong Polars LTL diagnostic for " << context << ": " << error.type << '/' << error.phase
                << "; first differing byte " << std::distance(message.begin(), mismatch.first)
                << "; expected bytes " << message.size() << "; actual bytes " << error.message.size() << '\n';
      std::cerr.write(error.message.data(), static_cast<std::streamsize>(error.message.size()));
      throw std::runtime_error("Captured Polars LTL type/phase/full-message mismatch: " + context);
    }
  }
  if (!thrown) throw std::runtime_error("Expected retained Polars LTL diagnostic: " + context);
}
LazyEventTable counted_source(const EventTable& table, std::size_t& scans) {
  return LazyEventTable(table.columns, [table, &scans](const auto& consume) {
    ++scans; for (const auto& row : table.rows) consume(row);
  }, table.attributes, table.metadata, table.rows.size(), table.dtypes, table.index, table.categorical_dtypes);
}

#include "polars_ltl_filter_public_vectors.inc"
#include "polars_ltl_workbench_public_vectors.inc"

// An ephemeral listener owned by this test exercises the actual multipart route.
// It never attaches to, replaces or terminates an existing application listener.
class OriginalCallHttpServer {
 public:
  OriginalCallHttpServer() {
    server_.Post("/api/reference-ltl-filter", handle_reference_ltl_filter);
    port_ = server_.bind_to_any_port("127.0.0.1");
    if (port_ <= 0) throw std::runtime_error("original-call owned HTTP bind failed");
    worker_ = std::thread([this] { listened_ = server_.listen_after_bind(); finished_ = true; });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!server_.is_running() && !finished_ && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!server_.is_running()) { stop(); throw std::runtime_error("original-call owned HTTP did not start"); }
  }
  ~OriginalCallHttpServer() { stop(); }
  void stop() { server_.stop(); if (worker_.joinable()) worker_.join(); }
  int port() const { return port_; }
  bool listened() const { return listened_; }
 private:
  httplib::Server server_;
  int port_ = -1;
  std::thread worker_;
  std::atomic<bool> finished_ = false, listened_ = false;
};
DiscoveryWorkbenchResponse original_call_http(httplib::Client& client,
    const std::vector<DiscoveryWorkbenchPart>& parts) {
  const std::string boundary = "minera-original-call-retained-public-boundary";
  std::string body;
  for (const auto& part : parts) {
    body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + part.name + '"';
    if (part.file) body += "; filename=\"" + part.filename + '"';
    body += "\r\nContent-Type: application/octet-stream\r\n\r\n" + part.bytes + "\r\n";
  }
  body += "--" + boundary + "--\r\n";
  const auto response = client.Post("/api/reference-ltl-filter", body, "multipart/form-data; boundary=" + boundary);
  if (!response || !response->get_header_value("Content-Type").starts_with("application/json"))
    throw std::runtime_error("original-call owned HTTP response missing");
  return {response->status, response->body};
}

EventTable subset(const EventTable& source,const std::vector<std::size_t>& selected) {
  EventTable result=source;result.rows.clear();for(const auto i:selected)result.rows.push_back(source.rows.at(i));return result;
}
void native_ltl_controls() {
  auto source=public_ltl_frame_0();source.attributes={{"native context",std::string("retained\0payload",16),AttributeKind::kString}};
  const auto before=source;
  const auto values=collection_value("",AttributeContainerKind::kList,{{"","A"},{"","B"},{"","C"}});
  std::size_t scans=0;const auto input=counted_source(source,scans);
  const auto eventual=filter_polars_ltl_eventually_follows(input,values);
  assert(scans==0);
  const auto eventual_expected=subset(source,{0,1,2,3,4,5,6,7,11,12,13,14});
  same_frame(eventual.materialize(),eventual_expected,"friendly eventual whole cases/source order/context");assert(scans==1);
  same_frame(eventual.materialize(),eventual_expected,"restartable eventual producer");assert(scans==2);
  same_frame(filter_polars_ltl_eventually_follows(source,values),subset(source,{3,4,5,6,7,11,12,13,14,0,1,2}),"friendly eager eventual sorted order");
  const auto next=filter_polars_ltl_next(input,values);assert(scans==3);
  same_frame(next.materialize(),subset(source,{0,1,2,11,12,13,14}),"friendly next apply-time scan/collect/source order");assert(scans==4);
  same_frame(next.materialize(),subset(source,{0,1,2,11,12,13,14}),"restartable next producer");assert(scans==5);
  same_frame(filter_polars_ltl_four_eyes(source,values),subset(source,{0,1,2,3,4,5,6,7}),"friendly pairwise four-eyes positive");
  PolarsLtlFilterOptions negative;negative.positive=boolean_value("",false);
  same_frame(filter_polars_ltl_four_eyes(source,values,negative),subset(source,{}),"negative four-eyes requires overlap in every pair");
  const auto persons_expected=subset(source,{11,12,13,14,19,20,21});
  same_frame(filter_polars_ltl_different_persons(source,{"","A"}),persons_expected,"friendly different-persons eager");
  same_frame(filter_polars_ltl_different_persons(input,{"","A"}).materialize(),persons_expected,"friendly different-persons lazy");
  PolarsLtlFilterOptions ignored;ignored.enable_timestamp=boolean_value("",true);ignored.timestamp_diff_boundaries=EventAttribute{"","inert invalid boundaries"};
  same_frame(filter_polars_ltl_eventually_follows(input,values,ignored).materialize(),eventual_expected,"public inert timestamp constraints");
  const auto no_match=collection_value("",AttributeContainerKind::kList,{{"","unobserved label"}});
  const auto pending=filter_polars_ltl_next(input,no_match);
  bool schema=false;try{(void)pending.materialize();}catch(const PolarsLtlFilterError& e){schema=e.type=="SchemaError"&&e.phase=="collect";}assert(schema);
  PolarsLtlFilterOptions literal_case;literal_case.case_id_key=integer_value("",std::int64_t{7});
  const auto literal=filter_polars_ltl_different_persons(input,{"","A"},literal_case);
  assert(literal.columns().back()=="literal"&&literal.dtypes().back()=="Int32");
  auto literal_expected=source;literal_expected.columns.push_back("literal");literal_expected.dtypes.push_back("Int32");
  for(auto& row:literal_expected.rows)row.push_back(integer_value("literal",std::int64_t{7}));
  same_frame(literal.materialize(),literal_expected,"literal grouping projection and lazy schema");
  PolarsLtlFilterOptions missing;missing.attribute_key={"","%FRAME%"};
  bool preserved=false;
  try{(void)filter_polars_ltl_different_persons(input,{"","A"},missing).materialize();}
  catch(const PolarsLtlFilterError& e){preserved=e.message.starts_with("unable to find column \"%FRAME%\"");}assert(preserved);
  const auto bad_query=collection_value("",AttributeContainerKind::kList,{{"","%FRAME%"}});
  preserved=false;try{(void)filter_polars_ltl_different_persons(input,bad_query).materialize();}
  catch(const PolarsLtlFilterError& e){preserved=e.message.find("[\"%FRAME%\"].cast(String)")!=std::string::npos;}assert(preserved);
  auto enum_source=source;enum_source.dtypes[0]="Enum(categories=['unused', 'g', 'f', 'e', 'd', 'c', 'b', 'a', 'z'])";
  EventTableCategoricalDtype dictionary;dictionary.categories_dtype="String";dictionary.ordered=true;
  for(const auto* key:{"unused","g","f","e","d","c","b","a","z"})dictionary.categories.push_back({"",key});
  enum_source.categorical_dtypes.resize(enum_source.columns.size());enum_source.categorical_dtypes[0]=dictionary;
  const auto enum_result=filter_polars_ltl_eventually_follows(LazyEventTable::from_table(enum_source),values).materialize();
  assert(enum_result.categorical_dtypes==enum_source.categorical_dtypes && enum_result.rows.size()==12);
  enum_source.categorical_dtypes[0]=std::nullopt;bool rejected=false;
  try{(void)filter_polars_ltl_eventually_follows(LazyEventTable::from_table(enum_source),values).materialize();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
  enum_source.categorical_dtypes[0]=dictionary;enum_source.rows[0][0]->value="unknown enum cell";rejected=false;
  try{(void)filter_polars_ltl_eventually_follows(LazyEventTable::from_table(enum_source),values).materialize();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
  same_frame(source,before,"native source/context preservation");
}
}  // namespace

int main() {
  try {
    retained_polars_ltl_filter_cases();native_ltl_controls();
    retained_polars_ltl_workbench_cases([](const auto& parts) { return execute_ltl_filter_workbench(parts); });
    OriginalCallHttpServer owned;
    httplib::Client client("127.0.0.1", owned.port());
    client.set_connection_timeout(5, 0); client.set_read_timeout(5, 0);
    retained_polars_ltl_workbench_cases([&](const auto& parts) { return original_call_http(client, parts); });
    owned.stop();
    if (!owned.listened()) throw std::runtime_error("original-call owned HTTP listener failed");
    std::cout << "All 675 original calls passed direct workbench and owned HTTP transport\n";
    std::cout << "Polars LTL: 675 retained calls, 516 complete frames, 159 full diagnostics; native controls PASS\n";
  } catch(const std::exception& e) {
    std::cerr << "Polars LTL native failure: " << e.what() << '\n';return 1;
  }
}
