#include "minera/emd_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace minera;
namespace {
template<class F> void rejects(F fn){bool caught=false;try{fn();}catch(const std::invalid_argument&){caught=true;}assert(caught);}
bool same_double(double left,double right){if(std::isnan(left))return std::isnan(right);std::uint64_t a=0,b=0;static_assert(sizeof(a)==sizeof(left));std::memcpy(&a,&left,sizeof(a));std::memcpy(&b,&right,sizeof(b));return a==b;}
StochasticLanguage one(std::string activity,double weight=1.0){return {{{{std::move(activity)},weight}},-999.0};}
void storage_and_defaults(){
 const auto empty=parse_emd_workbench_language_json("{}");assert(empty.traces.empty()&&empty.total_mass==0.0);
 const auto partial=parse_emd_workbench_language_json(R"({"traces":[{}, {"activities":["", " A ", " A "]}]})");assert(partial.traces.size()==2&&partial.traces[0].activities.empty()&&partial.traces[1].activities.size()==3&&partial.traces[1].weight==0);
 const std::string raw("\xff\xed\xa0\x80\xed\xb0\x80\0x",9);
 StochasticLanguage value;value.total_mass=-std::numeric_limits<double>::infinity();
 const std::vector<double> weights={-0.0,std::numeric_limits<double>::denorm_min(),std::numeric_limits<double>::max(),std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()};
 for(double weight:weights)value.traces.push_back({{"",raw,"\xF0\x9F\x98\x80",raw},weight});
 const auto json=serialize_emd_workbench_language_json(value);assert(json.find("native_bytes")!=std::string::npos);const auto copy=parse_emd_workbench_language_json(json);assert(copy.traces.size()==weights.size()&&same_double(copy.total_mass,value.total_mass));
 for(std::size_t i=0;i<weights.size();++i){assert(copy.traces[i].activities==value.traces[i].activities);assert(same_double(copy.traces[i].weight,weights[i]));}
 assert(serialize_emd_workbench_language_json(copy)==json);
 auto options=parse_emd_workbench_options_json("{}");assert(options.normalize_masses&&!options.strict_equal_mass&&options.variant==EmdVariant::PYEMD&&options.use_fast_emd);assert(!options.distance&&!options.encoded_distance&&!options.string_distance&&!options.utf8_string_distance);
 std::set<std::string> members;options=parse_emd_workbench_options_json(R"({"normalize_masses":false,"strict_equal_mass":true,"variant":-2147483648,"use_fast_emd":false})",&members);assert(members.size()==4&&static_cast<std::int32_t>(options.variant)==std::numeric_limits<std::int32_t>::min());assert(serialize_emd_workbench_options_json(options).find("\"name\":null")!=std::string::npos);
 assert(parse_emd_workbench_options_json(serialize_emd_workbench_options_json(options)).variant==options.variant);
 EmdResult result;result.distance=-0.0;result.computed=false;result.normalized=true;result.error=raw;result.variant=static_cast<EmdVariant>(std::numeric_limits<std::int32_t>::max());result.use_fast_emd=false;
 const auto result_json=serialize_emd_workbench_result_json(result);const auto restored=parse_emd_workbench_result_json(result_json);assert(same_double(restored.distance,result.distance)&&restored.computed==result.computed&&restored.normalized==result.normalized&&restored.error==raw&&restored.variant==result.variant&&!restored.use_fast_emd);assert(serialize_emd_workbench_result_json(restored)==result_json);
 const auto default_result=parse_emd_workbench_result_json("{}");assert(std::isnan(default_result.distance)&&default_result.computed&&!default_result.normalized&&default_result.error.empty()&&default_result.variant==EmdVariant::PYEMD&&default_result.use_fast_emd);
}
void strict_types_and_transactional_presence(){
 for(const auto* text:{R"({"traces":null})",R"({"total_mass":true})",R"({"traces":[{"activities":[1]}]})",R"({"traces":[{"weight":null}]})",R"({"traces":[],"traces":[]})",R"({"unexpected":0})",R"({"traces":[{"activities":[{"native_bytes":"bad","data":"ff"}]}]})",R"({"traces":[{"activities":[{"native_bytes":"hex-v1","data":"0"}]}]})"})rejects([&]{(void)parse_emd_workbench_language_json(text);});
 for(const auto* text:{R"({"variant":2147483648})",R"({"variant":-2147483649})",R"({"variant":0.0})",R"({"variant":1e0})",R"({"variant":"PYEMD"})",R"({"variant":{"index":0,"name":"pyemd"}})",R"({"variant":{"index":123,"name":"unknown"}})",R"({"normalize_masses":0})",R"({"distance":null})",R"({"encoded_distance":{}})",R"({"normalize_masses":true,"normalize_masses":false})"}){std::set<std::string> present={"sentinel"};rejects([&]{(void)parse_emd_workbench_options_json(text,&present);});assert(present==std::set<std::string>{"sentinel"});}
 rejects([&]{(void)parse_emd_workbench_result_json(R"({"computed":1})");});
 assert(!parse_emd_workbench_normalize_json("false")&&parse_emd_workbench_normalize_json(" true "));rejects([]{(void)parse_emd_workbench_normalize_json("1");});
 std::string malformed="{\"traces\":[{\"activities\":[\"";malformed+='\xff';malformed+="\"]}]}";rejects([&]{(void)parse_emd_workbench_language_json(malformed);});
}
void native_weight_semantics(){
 StochasticLanguage left{{{{"A"},0.5},{{"A"},0.5},{{"B"},1.0}},std::numeric_limits<double>::quiet_NaN()};
 StochasticLanguage right{{{{"A"},1.0},{{"C"},1.0}},-std::numeric_limits<double>::infinity()};
 const auto before=serialize_emd_workbench_language_json(left);const auto result=evaluate_emd(left,right);assert(result.computed&&!result.normalized&&result.distance==1.0);assert(serialize_emd_workbench_language_json(left)==before);assert(earth_movers_distance(left,right)==result.distance);
 auto unequal=one("A",2.0), equal=one("A");auto normalized=evaluate_emd(unequal,equal);assert(normalized.computed&&normalized.normalized&&normalized.distance==0.0);
 EmdOptions strict;strict.strict_equal_mass=true;auto invalid=evaluate_emd(unequal,equal,strict);assert(!invalid.computed&&invalid.error=="masses must be equal"&&std::isnan(invalid.distance));
 strict.strict_equal_mass=false;strict.use_fast_emd=false;invalid=evaluate_emd(unequal,equal,strict);assert(!invalid.computed&&invalid.error=="masses must be equal");strict.variant=EmdVariant::EXACT;normalized=evaluate_emd(unequal,equal,strict);assert(normalized.computed&&normalized.normalized&&!normalized.use_fast_emd&&normalized.variant==EmdVariant::EXACT);
 strict.variant=EmdVariant::PYEMD;strict.strict_equal_mass=true;auto close=evaluate_emd(one("A",1.000009),equal,strict);assert(close.computed&&!close.normalized&&close.distance==0);
 StochasticLanguage negative{{{{"A"},-1.0},{{"A"},1.0}},0};invalid=evaluate_emd(negative,equal);assert(!invalid.computed&&invalid.error=="invalid language weight");
 for(double weight:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}){invalid=evaluate_emd(one("A",weight),equal);assert(!invalid.computed&&invalid.error=="invalid language weight");}
 StochasticLanguage overflow{{{{"A"},std::numeric_limits<double>::max()},{{"A"},std::numeric_limits<double>::max()}},0};invalid=evaluate_emd(overflow,equal);assert(!invalid.computed&&invalid.error=="invalid language weight");
 EmdOptions no_calls;std::size_t calls=0;no_calls.distance=[&](const auto&,const auto&)->double{++calls;throw std::runtime_error("must not run");};assert(evaluate_emd(one("A",0),one("different",-0.0),no_calls).distance==0.0&&calls==0);invalid=evaluate_emd(one("A",0),equal,no_calls);assert(!invalid.computed&&invalid.error=="stochastic language has zero mass"&&calls==0);
 no_calls.variant=static_cast<EmdVariant>(999);invalid=evaluate_emd(negative,equal,no_calls);assert(!invalid.computed&&invalid.error=="unsupported EMD variant"&&calls==0);
}
void callback_tables_priority_and_arguments(){
 const auto json=R"({"distance":{"fallback":{"value":1}},"encoded_distance":{"fallback":{"value":2}},"string_distance":{"fallback":{"value":3}},"utf8_string_distance":{"fallback":{"value":4}}})";
 auto tables=parse_emd_workbench_callback_tables_json(json);const auto canonical=serialize_emd_workbench_callback_tables_json(tables);assert(serialize_emd_workbench_callback_tables_json(parse_emd_workbench_callback_tables_json(canonical))==canonical);
 const std::vector<std::size_t> slots={1,2,3,0};const std::vector<double> costs={2,3,4,1};
 for(std::size_t step=0;step<slots.size();++step){auto trace=std::make_shared<std::vector<EmdWorkbenchCallbackInvocation>>();EmdOptions options;bind_emd_workbench_callback_tables(options,tables,trace);const auto result=evaluate_emd(one("A"),one("A"),options);assert(result.computed&&result.distance==costs[step]);assert(trace->size()==1&&(*trace)[0].slot==slots[step]&&(*trace)[0].used_fallback&&!(*trace)[0].entry_index);if(slots[step]==1||slots[step]==2)assert(std::get<std::u32string>((*trace)[0].left)==std::u32string{1});if(slots[step]==3)assert(std::get<std::string>((*trace)[0].left)==std::string(1,'\1'));if(slots[step]==0)assert(std::get<std::vector<std::string>>((*trace)[0].left)==std::vector<std::string>{"A"});tables.slots[slots[step]].reset();}
 auto trace=std::make_shared<std::vector<EmdWorkbenchCallbackInvocation>>();auto duplicate=parse_emd_workbench_callback_tables_json(R"({"distance":{"entries":[{"left":["A"],"right":["A"],"outcome":{"value":7}},{"left":["A"],"right":["A"],"outcome":{"value":99}}],"fallback":{"value":0}}})");EmdOptions options;bind_emd_workbench_callback_tables(options,duplicate,trace);assert(earth_movers_distance(one("A"),one("A"),options)==7);assert(trace->size()==1&&trace->front().entry_index==0&&!trace->front().used_fallback);assert(duplicate.slots[0]->entries.size()==2);
 auto odd_keys=parse_emd_workbench_callback_tables_json(R"({"encoded_distance":{"entries":[{"left":[0,55296,4294967295],"right":[1114112],"outcome":{"value":"INF"}}],"fallback":{"value":0}}})");assert(std::get<std::u32string>(odd_keys.slots[1]->entries[0].left).back()==static_cast<char32_t>(4294967295u));assert(serialize_emd_workbench_callback_tables_json(parse_emd_workbench_callback_tables_json(serialize_emd_workbench_callback_tables_json(odd_keys)))==serialize_emd_workbench_callback_tables_json(odd_keys));
 auto union_tables=parse_emd_workbench_callback_tables_json(R"({"distance":{"fallback":{"value":0}}})");trace->clear();bind_emd_workbench_callback_tables(options,union_tables,trace);StochasticLanguage a{{{{"A"},1},{{"zero"},0}},1};assert(evaluate_emd(a,one("A"),options).computed&&trace->size()==4);assert(std::get<std::vector<std::string>>(trace->back().left)==std::vector<std::string>{"zero"});assert(std::get<std::vector<std::string>>(trace->back().right)==std::vector<std::string>{"zero"});
}
void callback_errors_and_presence(){
 auto trace=std::make_shared<std::vector<EmdWorkbenchCallbackInvocation>>();EmdOptions options;auto empty=parse_emd_workbench_callback_tables_json(R"({"distance":{},"encoded_distance":null})");bind_emd_workbench_callback_tables(options,empty,trace);assert(options.distance&&!options.encoded_distance);auto result=evaluate_emd(one("A"),one("A"),options);assert(!result.computed&&result.error=="distance callback failed: EMD finite cost-table adapter missing entry for distance");assert(trace->size()==1&&trace->front().missing_entry);
 bool threw=false;try{(void)earth_movers_distance(one("A"),one("A"),options);}catch(const std::invalid_argument& error){threw=true;assert(std::string(error.what())==result.error);}assert(threw&&trace->size()==2);
 for(const auto* cost:{"-1","\"NaN\"","\"INF\"","\"-INF\""}){const auto tables=parse_emd_workbench_callback_tables_json("{\"utf8_string_distance\":{\"fallback\":{\"value\":"+std::string(cost)+"}}}");trace->clear();bind_emd_workbench_callback_tables(options,tables,trace);result=evaluate_emd(one("A"),one("A"),options);assert(!result.computed&&result.error=="invalid distance"&&trace->size()==1);}
 const auto throwing=parse_emd_workbench_callback_tables_json(R"({"string_distance":{"fallback":{"throw":{"native_bytes":"hex-v1","data":"626164ffeda080edb080"}}}})");bind_emd_workbench_callback_tables(options,throwing,trace);result=evaluate_emd(one("A"),one("A"),options);assert(!result.computed&&result.error.find("distance callback failed: bad")==0);const auto serialized=serialize_emd_workbench_result_json(result);assert(serialized.find("native_bytes")!=std::string::npos&&parse_emd_workbench_result_json(serialized).error==result.error);
 const auto nul_throw=parse_emd_workbench_callback_tables_json(R"({"distance":{"fallback":{"throw":"bad\u0000after"}}})");trace->clear();bind_emd_workbench_callback_tables(options,nul_throw,trace);result=evaluate_emd(one("A"),one("A"),options);assert(!result.computed&&result.error=="distance callback failed: bad");assert(trace->size()==1&&std::get<std::string>(trace->front().outcome)==std::string("bad\0after",9));assert(serialize_emd_workbench_callback_tables_json(nul_throw).find("after")!=std::string::npos);assert(serialize_emd_workbench_callback_invocations_json(*trace).find("after")!=std::string::npos);
 EmdOptions nonstandard;nonstandard.distance=[](const auto&,const auto&)->double{throw 42;};result=evaluate_emd(one("A"),one("A"),nonstandard);assert(!result.computed&&result.error=="distance callback failed");
 EmdOptions overflow;overflow.distance=[](const auto&,const auto&){return std::numeric_limits<double>::max();};result=evaluate_emd(one("A",2),one("A",2),overflow);assert(!result.computed&&result.error=="transportation cost overflow");
 for(const auto* bad:{R"({"distance":{"entries":[{"left":[],"right":[],"outcome":{"value":0,"throw":"bad"}}]}})",R"({"encoded_distance":{"entries":[{"left":[4294967296],"right":[],"outcome":{"value":0}}]}})",R"({"string_distance":{"entries":[{"left":[-1],"right":[],"outcome":{"value":0}}]}})",R"({"utf8_string_distance":{"entries":[{"left":[],"right":"","outcome":{"value":0}}]}})",R"({"distance":true})",R"({"distance":null,"distance":{}})"}){std::set<std::string> present={"sentinel"};rejects([&]{(void)parse_emd_workbench_callback_tables_json(bad,&present);});assert(present==std::set<std::string>{"sentinel"});}
 std::set<std::string> present;const auto explicit_null=parse_emd_workbench_callback_tables_json(R"({"distance":null})",&present);assert(present==std::set<std::string>{"distance"}&&!explicit_null.slots[0]);
}
void deterministic_large_alphabet_arguments(){
 std::vector<std::string> word;for(unsigned i=0;i<130;++i){auto suffix=std::to_string(i);word.push_back("label"+std::string(3-suffix.size(),'0')+suffix);}StochasticLanguage language{{{word,1}},1};
 auto trace=std::make_shared<std::vector<EmdWorkbenchCallbackInvocation>>();auto tables=parse_emd_workbench_callback_tables_json(R"({"encoded_distance":{"fallback":{"value":0}}})");EmdOptions options;bind_emd_workbench_callback_tables(options,tables,trace);assert(evaluate_emd(language,language,options).computed&&trace->size()==1);
 const auto encoded=std::get<std::u32string>(trace->front().left);assert(encoded.size()==130&&encoded.front()==1&&encoded.back()>127);const std::u32string forbidden=U"[]()*+^?\r\n \t$\"!#&%|.,;-'\\/{}";for(std::size_t i=0;i<encoded.size();++i){assert(forbidden.find(encoded[i])==std::u32string::npos);if(i)assert(encoded[i-1]<encoded[i]);}
 tables=parse_emd_workbench_callback_tables_json(R"({"utf8_string_distance":{"fallback":{"value":0}}})");trace->clear();bind_emd_workbench_callback_tables(options,tables,trace);assert(evaluate_emd(language,language,options).computed&&trace->size()==1);const auto utf8=std::get<std::string>(trace->front().left);std::string expected;for(char32_t code:encoded){assert(code<2048);if(code<128)expected+=static_cast<char>(code);else{expected+=static_cast<char>(0xc0|(code>>6));expected+=static_cast<char>(0x80|(code&63));}}assert(utf8==expected&&utf8.size()>word.size());
}
void derive_original_log(){
 EventLog log;Trace a;a.case_id="same";a.attributes.push_back({"literal","value"});Event event;event.activity="A";a.events.push_back(event);log.traces={a,a,Trace{}};
 const auto before=serialize_embedding_workbench_raw_log_json(log);const auto source=parse_embedding_workbench_raw_log_json(before);const auto language=derive_stochastic_language(source);assert(language.traces.size()==2&&language.total_mass==3&&language.traces[0].activities.empty()&&language.traces[0].weight==1&&language.traces[1].activities==std::vector<std::string>{"A"}&&language.traces[1].weight==2);const auto normalized=derive_stochastic_language(source,true);assert(normalized.total_mass==1&&normalized.traces[0].weight==1.0/3.0&&normalized.traces[1].weight==2.0/3.0);assert(serialize_embedding_workbench_raw_log_json(source)==before);
 assert(derive_stochastic_language(EventLog{},true).total_mass==0);
}
}
int main(){
 try{
  std::cerr<<"storage and defaults\n";storage_and_defaults();
  std::cerr<<"strict types and transactional presence\n";strict_types_and_transactional_presence();
  std::cerr<<"native weight semantics\n";native_weight_semantics();
  std::cerr<<"callback priority and arguments\n";callback_tables_priority_and_arguments();
  std::cerr<<"callback errors and presence\n";callback_errors_and_presence();
  std::cerr<<"deterministic large alphabet arguments\n";deterministic_large_alphabet_arguments();
  std::cerr<<"derive original log\n";derive_original_log();
 }catch(const std::exception& error){std::cerr<<"EMD workbench test exception: "<<error.what()<<'\n';return 1;}
}
