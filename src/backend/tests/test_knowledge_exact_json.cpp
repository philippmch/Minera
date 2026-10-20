#include "minera/knowledge_exact_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/literal_controls_json.hpp"
#include "httplib.h"
#include <cassert>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
template<class F>void rejects(F f,std::string_view message={}){bool caught=false;try{f();}catch(const std::invalid_argument& e){caught=true;if(!message.empty())assert(std::string(e.what()).find(message)!=std::string::npos);}assert(caught);}
std::string replace_once(std::string source,std::string_view from,std::string_view to){const auto at=source.find(from);assert(at!=std::string::npos);source.replace(at,from.size(),to);return source;}
}
int main(){
 using namespace minera;
 const std::string binary("\xff\0\x80",3),wtf8("\xed\xa0\x80",3);
 const auto assumptions=parse_knowledge_exact_assumptions_json(R"(["","same","same"," \r\n ","\ud800",{"native_bytes":"hex-v1","data":"ff0080"}])");
 assert(assumptions.size()==6&&assumptions[0].empty()&&assumptions[1]==assumptions[2]&&assumptions[3]==" \r\n "&&assumptions[4]==wtf8&&assumptions[5]==binary);
 const auto raw_surrogate=parse_knowledge_exact_assumptions_json(std::string("[\"")+wtf8+"\"]");assert(raw_surrogate[0]==wtf8);
 const auto context=parse_knowledge_exact_context_json(R"([["z",""],["","v"],["a","x\u0000y"],[{"native_bytes":"hex-v1","data":"ff0080"},"\ud800"]])");
 assert(context.size()==4&&context.begin()->first.empty()&&context.at("a")==std::string("x\0y",3)&&context.at(binary)==wtf8);
 assert(parse_knowledge_exact_context_json("[]").empty()&&parse_knowledge_exact_assumptions_json("[]").empty());
 for(const auto* input:{"null","{}","[null]","[true]","[3]",R"([{"native_bytes":"bad","data":"ff"}])",R"([{"native_bytes":"hex-v1","data":"f"}])",R"([{"native_bytes":"hex-v1","data":"gg"}])",R"([{"native_bytes":"hex-v1","data":"ff","extra":0}])"})rejects([&]{(void)parse_knowledge_exact_assumptions_json(input);});
 rejects([&]{(void)parse_knowledge_exact_assumptions_json(std::string("[\"")+char(0xff)+"\"]");});
 for(const auto* input:{"null","{}","[null]","[[null,\"x\"]]","[[\"x\",null]]","[[\"x\",true]]","[[\"x\",0]]","[[\"x\"]]","[[\"x\",\"a\",\"b\"]]",R"([["x","a"],["x","b"]])",R"([["x","a"],[{"native_bytes":"hex-v1","data":"78"},"b"]])"})rejects([&]{(void)parse_knowledge_exact_context_json(input);});
 rejects([&]{(void)parse_knowledge_exact_context_json(std::string("[[\"")+wtf8+"\",\"a\"],[\"\\ud800\",\"b\"]]");},"duplicate decoded context key");
 KnowledgeInjectionOptions options;options.variant=KnowledgeInjectionVariant::kProcessMining;options.description="D";options.query="Q";options.database_schema="S";options.assumptions={"","\xce\xa9","same","same"};options.context={{"z",""},{"","v"},{"a",std::string("x\0y",3)}};options.max_input_bytes=19;options.max_prompt_bytes=92;
 const std::string expected="Process-mining task\nD\n\nQuestion\nQ\n\nAssumptions\n- \n- \xce\xa9\n- same\n- same\n\nContext\n: v\na: x"+std::string(1,'\0')+"y\nz: ";
 assert(expected.size()==92);const auto source=serialize_knowledge_exact_options_json(options);const auto round=parse_knowledge_exact_options_json(source);assert(round.assumptions==options.assumptions&&round.context==options.context&&round.description=="D"&&round.query=="Q"&&round.database_schema=="S"&&round.max_input_bytes==19&&round.max_prompt_bytes==92);
 const auto result=build_knowledge_prompt(round);assert(result.computed&&result.input_bytes==19&&result.prompt==expected&&result.error.empty());
 auto bounded=round;bounded.max_input_bytes=18;auto failed=build_knowledge_prompt(bounded);assert(!failed.computed&&failed.input_bytes==19&&failed.prompt.empty()&&failed.error=="knowledge injection input exceeds configured bound");
 bounded=round;bounded.max_prompt_bytes=91;failed=build_knowledge_prompt(bounded);assert(!failed.computed&&failed.input_bytes==19&&failed.prompt.empty()&&failed.error=="knowledge injection prompt exceeds configured bound");
 KnowledgeInjectionOptions empty;const auto empty_result=build_knowledge_prompt(parse_knowledge_exact_options_json(serialize_knowledge_exact_options_json(empty)));assert(empty_result.computed&&empty_result.prompt.empty()&&empty_result.input_bytes==0);
 empty.assumptions={""};assert(build_knowledge_prompt(empty).prompt=="Assumptions\n- "&&build_knowledge_prompt(empty).input_bytes==0);empty.assumptions.clear();empty.context={{"",""}};assert(build_knowledge_prompt(empty).prompt=="Context\n: "&&build_knowledge_prompt(empty).input_bytes==0);
 for(const auto* alias:{"description","process_mining","process-mining","database","db"}){auto doc=replace_once(source,"\"variant\":\"process_mining\"","\"variant\":\""+std::string(alias)+"\"");const auto native=parse_knowledge_exact_options_json(doc);assert(std::string(knowledge_injection_variant_name(native.variant))==(std::string(alias)=="process-mining"?"process_mining":std::string(alias)=="db"?"database":alias));}
 auto database=round;database.variant=KnowledgeInjectionVariant::kDatabase;database.max_prompt_bytes=1000;database.database_schema=" ";assert(build_knowledge_prompt(database).computed);database.database_schema="";const auto missing=build_knowledge_prompt(database);assert(!missing.computed&&missing.prompt.empty()&&missing.input_bytes==18&&missing.error=="database knowledge injection requires a schema");
 auto inactive=empty;inactive.context.clear();inactive.description="A\r\nB\rC";inactive.query="inactive";inactive.database_schema="also inactive";const auto raw=build_knowledge_prompt(inactive);assert(raw.prompt=="Description\nA\r\nB\rC"&&raw.input_bytes==inactive.description.size()+inactive.query.size()+inactive.database_schema.size());
 auto exact_bytes=round;exact_bytes.description=binary;exact_bytes.query=wtf8;exact_bytes.database_schema=std::string("\0",1);exact_bytes.assumptions=assumptions;exact_bytes.context=context;exact_bytes.max_input_bytes=10000;exact_bytes.max_prompt_bytes=10000;const auto byte_options=parse_knowledge_exact_options_json(serialize_knowledge_exact_options_json(exact_bytes));assert(byte_options.description==binary&&byte_options.query==wtf8&&byte_options.database_schema==std::string("\0",1)&&byte_options.context==context&&byte_options.assumptions==assumptions);const auto byte_result=build_knowledge_prompt(byte_options);assert(byte_result.computed);
 assert(!literal_controls_transport_error(serialize_knowledge_exact_options_json(byte_options)));assert(!literal_controls_transport_error(serialize_knowledge_exact_result_json(byte_result)));
 auto widest=empty;widest.max_input_bytes=std::numeric_limits<std::size_t>::max();widest.max_prompt_bytes=std::numeric_limits<std::size_t>::max();const auto widest_doc=serialize_knowledge_exact_options_json(widest);assert(parse_knowledge_exact_options_json(widest_doc).max_input_bytes==std::numeric_limits<std::size_t>::max());const auto decimal=replace_once(widest_doc,"\"max_input_bytes\":"+std::to_string(widest.max_input_bytes),"\"max_input_bytes\":\""+std::to_string(widest.max_input_bytes)+"\"");assert(parse_knowledge_exact_options_json(decimal).max_input_bytes==widest.max_input_bytes);
 for(const auto* value:{"-1","1.0","1e0","null","true","\"NaN\"","\"+1\"","\" 1\"","18446744073709551616"})rejects([&]{(void)parse_knowledge_exact_options_json(replace_once(source,"\"max_input_bytes\":19","\"max_input_bytes\":"+std::string(value)));});
 rejects([&]{(void)parse_knowledge_exact_options_json(replace_once(source,"\"description\":\"D\",",""));},"missing field description");rejects([&]{(void)parse_knowledge_exact_options_json(replace_once(source,"\"description\":\"D\"","\"description\":\"D\",\"description\":\"D\""));},"duplicate object key");rejects([&]{(void)parse_knowledge_exact_options_json(replace_once(source,"\"description\":\"D\"","\"description\":null"));});rejects([&]{(void)parse_knowledge_exact_options_json(replace_once(source,"\"description\":\"D\"","\"description\":\"D\",\"unknown\":0"));},"unknown field unknown");
 std::vector<KnowledgeExactInput> independent={{"knowledge_variant",std::nullopt,"process-mining"},{"knowledge_description",std::nullopt,"D"},{"knowledge_query",std::nullopt,"Q"},{"knowledge_database_schema",std::nullopt,"S"},{"knowledge_max_input_bytes",std::nullopt,"19"},{"knowledge_max_prompt_bytes",std::nullopt,"92"},{"knowledge_assumptions_file",std::string("a.json"),R"(["","\u03a9","same","same"])"},{"knowledge_context_json",std::nullopt,R"([["z",""],["","v"],["a","x\u0000y"]])"}};
 const auto independent_options=parse_knowledge_exact_inputs(independent);assert(build_knowledge_prompt(independent_options).prompt==expected);
 const auto defaults=parse_knowledge_exact_inputs({{"knowledge_context_json",std::nullopt,"[]"}});assert(defaults.max_input_bytes==1048576&&defaults.max_prompt_bytes==2097152&&defaults.context.empty()&&defaults.assumptions.empty());
 const auto zero_options=parse_knowledge_exact_options_json(replace_once(source,"\"max_input_bytes\":19","\"max_input_bytes\":0"));assert(zero_options.max_input_bytes==0);rejects([&]{(void)build_knowledge_prompt(zero_options);},"bounds must be positive");
 const auto independent_zero=parse_knowledge_exact_inputs({{"knowledge_max_prompt_bytes",std::nullopt,"0"},{"knowledge_assumptions_json",std::nullopt,"[]"}});assert(independent_zero.max_prompt_bytes==0);rejects([&]{(void)build_knowledge_prompt(independent_zero);},"bounds must be positive");
 rejects([&]{(void)parse_knowledge_exact_inputs({{"knowledge_options_json",std::nullopt,source},{"knowledge_description",std::nullopt,""}});},"complete knowledge options cannot combine");rejects([&]{(void)parse_knowledge_exact_inputs({{"knowledge_assumptions_json",std::nullopt,"[]"},{"knowledge_assumptions_file",std::string("f"),"[]"}});},"mutually exclusive");rejects([&]{(void)parse_knowledge_exact_inputs({{"knowledge_context_json",std::nullopt,"[]"},{"knowledge_context_json",std::nullopt,"[]"}});},"duplicate knowledge field");rejects([&]{(void)parse_knowledge_exact_inputs({{"knowledge_options_file",std::nullopt,source}});},"requires an actual File input");rejects([&]{(void)parse_knowledge_exact_inputs({{"knowledge_context_json",std::nullopt,""}});},"unexpected end of input");rejects([&]{(void)parse_knowledge_exact_inputs({{"other",std::nullopt,"x"}});},"unknown knowledge field");
 assert(parse_knowledge_exact_inputs({{"knowledge_options_file",std::string(""),source}}).description=="D");
 const std::vector<KnowledgeExactInput> byte_inputs={{"knowledge_options_file",std::string("original.json"),serialize_knowledge_exact_options_json(byte_options)}};
 const auto envelope=serialize_knowledge_exact_envelope_json(byte_options,byte_result,byte_inputs);assert(!literal_controls_transport_error(envelope)&&envelope.find("\"legacy_result_transport_error\":\"non_scalar_utf8\"")!=std::string::npos&&envelope.find("\"legacy_result_error\":null")!=std::string::npos&&envelope.find("original.json")!=std::string::npos);
 const auto failed_envelope=serialize_knowledge_exact_envelope_json(bounded,failed,independent);assert(failed_envelope.find("\"computed\":false")!=std::string::npos&&failed_envelope.find("\"prompt_bytes\":0")!=std::string::npos);
 const auto exception_envelope=serialize_knowledge_exact_envelope_json(zero_options,std::nullopt,{{"knowledge_options_json",std::nullopt,replace_once(source,"\"max_input_bytes\":19","\"max_input_bytes\":0")}},"knowledge injection bounds must be positive");assert(exception_envelope.find("\"result\":null")!=std::string::npos&&exception_envelope.find("\"max_input_bytes\":0")!=std::string::npos&&exception_envelope.find("\"type\":\"native_error\"")!=std::string::npos);
 // Additive httplib metadata distinguishes text from filename="" and resets
 // for each part, including incremental parsing and RFC5987 filenames.
 httplib::MultipartFormData legacy_aggregate{"field","data","name","text/plain"};assert(!legacy_aggregate.filename_present);
 const std::string multipart="--boundary\r\nContent-Disposition: form-data; name=\"text\"\r\n\r\nA\r\n--boundary\r\nContent-Disposition: form-data; name=\"empty\"; filename=\"\"\r\n\r\nB\r\n--boundary\r\nContent-Disposition: form-data; name=\"named\"; filename=\"sample.json\"\r\n\r\nC\r\n--boundary\r\nContent-Disposition: form-data; name=\"extended\"; filename*=UTF-8''%CE%A9.json\r\n\r\nD\r\n--boundary\r\nContent-Disposition: form-data; name=\"reset\"\r\n\r\nE\r\n--boundary\r\nContent-Disposition: form-data; name=\"knowledge_context_json\"; filename=\"arbitrary-byte-field.json\"\r\nContent-Type: application/octet-stream\r\n\r\n[]\r\n--boundary--\r\n";
 httplib::detail::MultipartFormDataParser parser;parser.set_boundary(std::string("boundary"));std::vector<httplib::MultipartFormData> parts;
 const auto header=[&](const httplib::MultipartFormData& part){parts.push_back(part);return true;};const auto data=[&](const char* bytes,std::size_t size){assert(!parts.empty());parts.back().content.append(bytes,size);return true;};
 for(std::size_t i=0;i<multipart.size();++i)assert(parser.parse(multipart.data()+i,1,data,header));assert(parser.is_valid()&&parts.size()==6);
 assert(!parts[0].filename_present&&parts[0].filename.empty()&&parts[0].content=="A");assert(parts[1].filename_present&&parts[1].filename.empty()&&parts[1].content=="B");assert(parts[2].filename_present&&parts[2].filename=="sample.json");assert(parts[3].filename_present&&parts[3].filename=="\xce\xa9.json");assert(!parts[4].filename_present&&parts[4].filename.empty());assert(parts[5].filename_present&&parts[5].filename=="arbitrary-byte-field.json");
 // Split the initial boundary at every internal position. Unlike feeding
 // only complete initial headers, this exercises the streaming prefix state.
 const auto check_fragmented=[&](const std::string& body,const std::vector<std::size_t>& widths){
  httplib::detail::MultipartFormDataParser fragmented;fragmented.set_boundary(std::string("boundary"));std::vector<httplib::MultipartFormData> observed;
  const auto on_header=[&](const httplib::MultipartFormData& part){observed.push_back(part);return true;};
  const auto on_data=[&](const char* bytes,std::size_t size){assert(!observed.empty());observed.back().content.append(bytes,size);return true;};
  std::size_t offset=0,index=0;while(offset<body.size()){const auto width=widths[(std::min)(index,widths.size()-1)];const auto size=(std::min)(width,body.size()-offset);assert(size>0);assert(fragmented.parse(body.data()+offset,size,on_data,on_header));offset+=size;++index;}
  assert(fragmented.is_valid()&&observed.size()==parts.size());
  for(std::size_t i=0;i<parts.size();++i){assert(observed[i].name==parts[i].name&&observed[i].content==parts[i].content&&observed[i].filename==parts[i].filename&&observed[i].filename_present==parts[i].filename_present&&observed[i].content_type==parts[i].content_type);}
 };
 const std::string initial="--boundary\r\n";
 check_fragmented(multipart,{multipart.size()});
 for(std::size_t split=1;split<initial.size();++split)check_fragmented(multipart,{split,multipart.size()});
 check_fragmented(multipart,{1});check_fragmented(multipart,{2});check_fragmented(multipart,{7});
 // Long preambles, false marker prefixes and a trailing partial prefix must
 // not lose the eventual real marker or grow retained state with the preamble.
 const std::string preamble=std::string(4097,'p')+"--boundX\r\n--boundar";
 check_fragmented(preamble+multipart,{1});check_fragmented(preamble+multipart,{7});check_fragmented(preamble+multipart,{4099,1});
 const KnowledgeExactInput scalar_blob{"knowledge_context_json",std::nullopt,"[]",std::string("arbitrary-byte-field.json")};assert(parse_knowledge_exact_inputs({scalar_blob}).context.empty());const auto blob_envelope=serialize_knowledge_exact_envelope_json(defaults,build_knowledge_prompt(defaults),{scalar_blob});assert(blob_envelope.find("\"name\":null,\"transport_filename\":\"arbitrary-byte-field.json\"")!=std::string::npos);
 return 0;
}
