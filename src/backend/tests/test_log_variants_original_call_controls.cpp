#include "../log_variants_original_call_parser.hpp"
#include <bit>
#include <iostream>
namespace {
using namespace minera;
namespace original=minera::log_variants_original_call_detail;
void require(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
std::string call(std::string args,std::string keywords="[]"){return "{\"schema\":\"minera-log-variants-original-call-v1\",\"args\":"+args+",\"keyword_entries\":"+keywords+'}';}
EventAttribute value(const std::string& json){return original::packed(json,original::shared::Reader(json).read());}
void controls(){
  require(original::failure_text_json("",true)=="\"\""&&original::failure_text_json("",false)=="null","empty native error must remain present on failure");
  const auto tuple=value(R"({"type":"tuple","values":[{"type":"float","hex":"-0x0.0p+0"},{"type":"list","values":[{"type":"int","value":9007199254740993}]}]})");
  require(tuple.container==AttributeContainerKind::kTuple&&tuple.children[0].key=="0"&&tuple.children[1].children[0].key=="0","indexed child keys lost");
  require(tuple.children[0].value=="-0.000000"&&std::bit_cast<std::uint64_t>(std::get<double>(tuple.children[0].typed_value))==std::bit_cast<std::uint64_t>(-0.0),"signed zero carrier changed");
  require(std::get<std::int64_t>(tuple.children[1].children[0].typed_value)==INT64_C(9007199254740993),"large exact integer lost");
  const auto big=value(R"({"type":"int","value":-999999999999999999999999999999})");require(big.value=="-999999999999999999999999999999"&&std::holds_alternative<std::monostate>(big.typed_value),"arbitrary-width int coerced");
  const auto u=value(R"({"type":"int","value":18446744073709551615})");require(std::get<std::uint64_t>(u.typed_value)==UINT64_MAX,"uint64 constructor boundary changed");
  const auto temporal=value(R"({"type":"datetime","iso":"2020-01-01T00:00:00+02:00","fold":1,"offset_seconds":7200.0})");const auto& date=std::get<NativeDate>(temporal.typed_value);require(date.resolution==NativeDateResolution::kMicrosecond&&!date.timezone,"generator datetime metadata changed");
  const auto nat=value(R"({"type":"nat"})");require(nat.is_null&&std::get<NativeDate>(nat.typed_value).resolution==NativeDateResolution::kNanosecond,"native NaT carrier changed");
  const auto uuid=value(R"({"type":"uuid","value":"12345678-1234-5678-1234-567812345678"})");require(uuid.kind==AttributeKind::kId,"UUID kind lost");
  require(is_native_na_attribute(value(R"({"type":"native_na"})")),"NA kind lost");
  require(decode_bytes_attribute(value(R"({"type":"bytes","hex":"ff000a"})"))==std::string("\xff\0\n",3),"byte value lost");
  const auto dict=value(R"({"type":"dict","items":[[{"type":"str","value":"z"},{"type":"NoneType","value":null}],[{"type":"str","value":"a"},{"type":"frozenset","values":[{"type":"str","value":"X"}]}]]})");require(dict.children[0].key=="z"&&dict.children[1].key=="a"&&dict.children[1].container==AttributeContainerKind::kFrozenSet,"ordinary map order/frozenset lost");
  for(const auto& hex:{"nan","inf","-inf","0x1.5555555555555p-2"}){const std::string json="{\"type\":\"float\",\"hex\":\""+std::string(hex)+"\"}";const auto a=value(json);require(a.value==std::to_string(std::get<double>(a.typed_value)),"float display diverged from generator");}
  auto omitted=original::parse(call("[]"),"filter_log_variants_percentage");require(omitted.bound&&omitted.bound->value=="0.800000"&&std::holds_alternative<double>(omitted.bound->typed_value),"default cumulative carrier lost");require(std::holds_alternative<std::monostate>(omitted.options.positive.typed_value),"default positive converted to explicit bool");
  const std::string params=R"([{"key":"parameters","value":{"type":"dict","items":[[{"type":"enum","module":"pm4py.algo.filtering.log.variants.variants_filter","name":"POSITIVE","value":{"type":"str","value":"positive"}},{"type":"bool","value":false}],[{"type":"str","value":"positive"},{"type":"bool","value":true}],[{"type":"str","value":"decreasingFactor"},{"type":"float","hex":"0x1.0000000000000p-1"}]]}}])";
  const auto explicit_params=original::parse(call(R"([{"type":"list","values":[]}])",params),"apply");require(explicit_params.parameter_state=="entries"&&std::get<bool>(explicit_params.options.positive.typed_value),"parameter identity/order resolution changed");require(explicit_params.keywords[0].second.parameters.size()==3,"ignored parameter value dropped");
  const auto state=original::shared::Reader(original::controls_json(explicit_params)).read();require(state.fields.size()==5&&original::at(state,"resolved").fields.size()==5,"actual control fields missing");
  const auto admission=original::parse(call(R"([{"type":"dict","items":[[{"type":"tuple","values":[{"type":"str","value":"A"}]},{"type":"int","value":9007199254740993}]]}])"),"apply");require(admission.args[0].native_admission.keys.size()==1&&admission.args[0].admission[0].second.value=="9007199254740993","admission ignored values lost");
  auto cache=original::parse(call(R"([{"type":"dict","items":[[{"type":"tuple","values":[]},{"type":"list","values":[{"type":"trace_ref","source_trace_index":0}]}],[{"type":"NoneType","value":null},{"type":"str","value":"ordinary"}]]},{"type":"int","value":1}])"),"find_auto_threshold");EventLog source;Trace trace;trace.properties={{"retained","trace"}};source.traces.push_back(trace);original::materialize(cache,source);require(cache.args[0].variants[0].traces[0].properties==trace.properties&&cache.args[0].variants[1].ordinary_value->value=="ordinary","trace copies/ordinary value union lost");
  for(const auto& bad:std::vector<std::string>{R"({"type":"int","value":1.0})",R"({"type":"float","hex":"1.0"})",R"({"type":"str","value":"A","extra":0})",R"({"type":"str","value":"A","\u0076alue":"B"})",R"({"type":"NoneType"})",R"({"type":"bytes","hex":"0g"})",R"({"type":"dict","items":[[{"type":"tuple","values":[]},{"type":"int","value":1}]]})",R"({"type":"trace_ref","source_trace_index":0})",R"({"type":"datetime","iso":"2020-01-01T00:00:00+02:00","fold":0,"offset_seconds":null})",R"({"type":"datetime","iso":"2020-01-01T00:00:00+02:00","fold":0,"offset_seconds":3600.0})",R"({"type":"datetime","iso":"2020-01-01T00:00:00","fold":0,"offset_seconds":0.0})"}){
    bool rejected=false;try{(void)value(bad);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"malformed/misplaced packed tag accepted: "+bad);
  }
  const auto bom=original::parse(std::string("\xef\xbb\xbf",3)+"\r\n"+call(R"([{"type":"int","value":9007199254740993}])"),"filter_variants_top_k");require(bom.bound->value=="9007199254740993","BOM slice offsets lost exact int");
}
}
int main(){try{controls();std::cout<<"private LOG variants packed codecs and actual caller controls passed\n";}catch(const std::exception& failure){std::cerr<<failure.what()<<'\n';return 1;}return 0;}
