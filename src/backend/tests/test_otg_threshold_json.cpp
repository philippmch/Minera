#include "minera/otg_threshold_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
template<class F> void rejects(F f) {bool caught=false;try{f();}catch(const std::invalid_argument&){caught=true;}assert(caught);}
std::string threshold(std::string_view key,std::string_view value) {
 return "[["+minera::serialize_embedding_workbench_string_json(key)+","+std::string(value)+"]]";
}
}
int main() {
 using namespace minera;
 const std::string key=" custom,relation:\xce\xa9 ";
 const std::string binary("\xff\0\x80",3),wtf8("\xed\xa0\x80",3);
 assert(parse_otg_theta_by_relation_json("[]").empty());
 auto zero=parse_otg_theta_by_relation_json(R"([["",-0.0],["plus",0.0]])");
 assert(zero.size()==2&&std::signbit(zero.at(""))&&!std::signbit(zero.at("plus")));
 const auto extremes=parse_otg_theta_by_relation_json(R"([["small",4.9406564584124654e-324],["big",1.7976931348623157e308],["negative",-1],["above",3],["nan","NaN"],["inf","INF"],["minus","-INF"]])");
 assert(extremes.at("small")==std::numeric_limits<double>::denorm_min());
 assert(extremes.at("big")==std::numeric_limits<double>::max());
 assert(extremes.at("negative")==-1&&extremes.at("above")==3);
 assert(std::isnan(extremes.at("nan"))&&std::isinf(extremes.at("inf"))&&extremes.at("minus")<0);
 auto bytes=parse_otg_theta_by_relation_json(threshold(binary,"0.3"));assert(bytes.at(binary)==0.3);
 auto surrogate=parse_otg_theta_by_relation_json(R"([["\ud800",0.2]])");assert(surrogate.at(wtf8)==0.2);
 assert(parse_otg_theta_by_relation_json(threshold(wtf8,"-0.0")).contains(wtf8));
 // Actual multipart UTF-16 lone surrogates arrive as raw WTF-8 triples.
 const auto raw_wtf8_text=std::string("[[\"")+wtf8+"\",0.2]]";
 assert(parse_otg_theta_by_relation_json(raw_wtf8_text).at(wtf8)==0.2);
 rejects([&]{(void)parse_otg_theta_by_relation_json(std::string("[[\"")+wtf8+"\",0.2],[\"\\ud800\",1]]");});
 rejects([&]{(void)parse_otg_theta_by_relation_json(std::string("[[\"")+wtf8+"\",0.2],[{\"native_bytes\":\"hex-v1\",\"data\":\"eda080\"},1]]");});

 const std::vector<std::string> malformed={"null","{}","[null]","[[null,0]]","[[true,0]]","[[1,0]]","[[\"x\",null]]","[[\"x\",true]]","[[\"x\",\"0.3\"]]","[[\"x\",\"nan\"]]","[[\"x\",\"+INF\"]]","[[\"x\",1e309]]","[[\"x\",0,1]]","[[\"x\"]]","[[\"x\",0],[\"x\",1]]",R"([["x",0],[{"native_bytes":"hex-v1","data":"78"},1]])",R"([[{"native_bytes":"hex-v1","data":"f"},0]])",R"([[{"native_bytes":"hex-v2","data":"ff"},0]])",R"([[{"native_bytes":"hex-v1","data":"ff","extra":0},0]])",R"([[{"native_bytes":"hex-v1","data":"gg"},0]])",R"([["x",01]])",R"([["x",0],])"};
 for(const auto& input:malformed)rejects([&]{(void)parse_otg_theta_by_relation_json(input);});
 rejects([&]{(void)parse_otg_theta_by_relation_json(std::string("[[\"")+char(0xff)+"\",0]]");});
 rejects([&]{(void)parse_otg_theta_by_relation_json(R"([["\ud800",0],[{"native_bytes":"hex-v1","data":"eda080"},1]])");});
 OtgGraph real{{"order","item"},{{"order",key,"item",12}}},norm{{"order","item"},{{"order",key,"item",10}}};
 OtgConformanceOptions options;options.theta=0.1;
 auto result=compare_otg(real,norm,options);assert(result.fitness==0.75&&result.non_conforming_edges.size()==1);
 options.theta_by_relation=parse_otg_theta_by_relation_json(threshold(key,"0.3"));result=compare_otg(real,norm,options);assert(result.fitness==1&&result.non_conforming_edges.empty());
 options.theta_by_relation=parse_otg_theta_by_relation_json(threshold(key,"0.2"));assert(compare_otg(real,norm,options).fitness==1);
 options.theta_by_relation=parse_otg_theta_by_relation_json(threshold("custom,relation:\xce\xa9","0.3"));assert(compare_otg(real,norm,options).fitness==0.75);
 for(const auto* value:{"\"NaN\"","\"INF\"","3","1.7976931348623157e308"}){options.theta_by_relation=parse_otg_theta_by_relation_json(threshold(key,value));assert(compare_otg(real,norm,options).non_conforming_edges.empty());}
 for(const auto* value:{"\"-INF\"","-1","-0.0","4.9406564584124654e-324"}){options.theta_by_relation=parse_otg_theta_by_relation_json(threshold(key,value));assert(compare_otg(real,norm,options).non_conforming_edges.size()==1);}
 options.theta_by_relation=parse_otg_theta_by_relation_json(threshold("unused","0.3"));assert(compare_otg(real,norm,options).fitness==0.75);
 // Vectors are storage, not sets. Exact integer tokens never pass through double.
 OtgGraph stored{{binary,"",binary,wtf8},{{binary,binary,wtf8,std::numeric_limits<std::size_t>::max()},{binary,binary,wtf8,0}}};
 const auto canonical=serialize_exact_otg_graph_json(stored);const auto round=parse_exact_otg_graph_json(canonical);
 assert(round.object_types==stored.object_types&&round.edges.size()==2);
 assert(round.edges[0].source_type==binary&&round.edges[0].relation==binary&&round.edges[0].target_type==wtf8&&round.edges[0].frequency==std::numeric_limits<std::size_t>::max());
 assert(round.edges[1].frequency==0);
 auto decimal=parse_exact_otg_graph_json("{\"object_types\":[\"\",\"\"],\"edges\":[{\"source_type\":\"\",\"relation\":\"\",\"target_type\":\"\",\"frequency\":\""+std::to_string(std::numeric_limits<std::size_t>::max())+"\"}]}");
 assert(decimal.object_types.size()==2&&decimal.edges[0].frequency==std::numeric_limits<std::size_t>::max());
 const auto raw_legacy=otg_json(stored);const auto raw=parse_exact_otg_graph_json(raw_legacy);assert(raw.object_types==stored.object_types&&raw.edges[0].frequency==stored.edges[0].frequency);
 for(const auto* value:{"-1","1.0","1e0","null","true","\"+1\"","\" 1\"","18446744073709551616"})rejects([&]{(void)parse_exact_otg_graph_json(std::string("{\"object_types\":[],\"edges\":[{\"source_type\":\"\",\"relation\":\"\",\"target_type\":\"\",\"frequency\":")+value+"}]}");});
 rejects([]{(void)parse_exact_otg_graph_json(R"({"object_types":[],"edges":[],"activities":[]})");});
 rejects([]{(void)parse_exact_otg_graph_json(R"({"object_types":[],"object_types":[],"edges":[]})");});
 OtgGraph binary_real{{binary,wtf8},{{binary,binary,wtf8,12}}},binary_norm{{binary,wtf8},{{binary,binary,wtf8,10}}};
 options.theta_by_relation=parse_otg_theta_by_relation_json(threshold(binary,"\"-INF\""));const auto binary_result=compare_otg(binary_real,binary_norm,options);
 assert(binary_result.non_conforming_edges.size()==1);
 const auto safe=serialize_exact_otg_result_json(binary_result);assert(!literal_controls_transport_error(safe));assert(safe.find("native_bytes")!=std::string::npos);
 options.theta_by_relation=zero;const auto opt=serialize_exact_otg_options_json(options);assert(opt.find("-0.0")!=std::string::npos);const auto mark=opt.find("\"theta_by_relation\":")+20;const auto recovered=parse_otg_theta_by_relation_json(opt.substr(mark,opt.size()-mark-1));assert(std::signbit(recovered.at("")));
 const auto envelope=serialize_exact_otg_conformance_json(binary_result,binary_real,binary_norm,options,std::string("\0\xff",2),raw_legacy," [] \r\n");
 assert(!literal_controls_transport_error(envelope));assert(envelope.find("\"legacy_result_transport_error\":\"non_scalar_utf8\"")!=std::string::npos);assert(envelope.find("\"legacy_result_error\":null")!=std::string::npos);assert(envelope.find("original_real")!=std::string::npos);
 // Every native result field survives even when semantically unusual.
 OtgConformanceResult unusual;unusual.fitness=-std::numeric_limits<double>::infinity();unusual.missing_object_types={"",binary,binary};unusual.additional_object_types={wtf8};unusual.missing_edges={{binary,"",wtf8}};unusual.additional_edges={{wtf8,binary,""}};unusual.non_conforming_edges[{"",binary,wtf8}]=std::numeric_limits<double>::quiet_NaN();
 const auto all=serialize_exact_otg_result_json(unusual);assert(!literal_controls_transport_error(all));assert(all.find("\"fitness\":\"-INF\"")!=std::string::npos&&all.find("\"delta\":\"NaN\"")!=std::string::npos);
 return 0;
}
