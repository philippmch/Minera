#undef NDEBUG
#include "minera/embedding_workbench_json.hpp"
#include "minera/footprints_workbench_json.hpp"
#include "minera/declare_workbench_json.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using namespace minera;
template<class Call>void rejects(Call call){bool threw=false;try{call();}catch(const std::invalid_argument&){threw=true;}assert(threw);}
void contains(const std::string& value,std::string_view part){if(value.find(part)==std::string::npos)std::cerr<<"Missing "<<part<<" in "<<value<<std::endl;assert(value.find(part)!=std::string::npos);}
struct RegistryScope{RegistryScope(){clear_default_embedding_provider();}~RegistryScope(){clear_default_embedding_provider();}};
EmbeddingOptions unit_options(){EmbeddingOptions o;o.provider=EmbeddingProvider::TRANSFORMER;o.embedding_model="unit-only-explicit-host";o.dimensions=2;o.transformer_callback=[](std::string_view text,std::string_view,std::size_t){return text.starts_with("keep")?std::vector<double>{1,0}:std::vector<double>{0,1};};return o;}
EventLog full_log(){
 auto log=parse_footprints_workbench_raw_log_json(R"({"attributes":[{"key":"log","value":"lexical","kind_index":91,"typed_value":{"index":5,"value":18446744073709551615},"children":[{"key":"child","value":"-0","typed_value":{"index":2,"value":-0.0}}]}],"metadata":{"extensions":[{"name":"extension","prefix":"x","uri":"urn:literal"}],"classifiers":[{"name":"class","keys":["a,b","","a,b"]}],"trace_globals":[{"key":"tg","value":"trace"}],"event_globals":[{"key":"eg","value":"event"}],"properties":[{"key":"p","value":"literal"}],"globals":[{"scope":"trace","attributes":[{"key":"global","value":"ordered"}]}],"compatibility_snapshot_valid":true,"compatibility_globals_snapshot":[{"scope":"latent","attributes":[{"key":"g","value":"snapshot"}]}],"compatibility_trace_globals_snapshot":[{"key":"st","value":"different"}],"compatibility_event_globals_snapshot":[{"key":"se","value":"different"}]},"traces":[{"case_id":"dup","case_id_state_index":93,"case_id_source_key":"raw-case","case_id_attribute":{"key":"case","value":"contradictory"},"attributes":[{"key":"trace","value":"context"}],"properties":[{"key":"property","value":"stored"}],"events":[{"case_id":"event-case-disagrees","activity":"keep","timestamp":"raw-time","resource":"raw-resource","lifecycle":"raw-life","input_order":99,"activity_state_index":94,"activity_source_key":"raw-label","activity_attribute":{"key":"label","value":"selected-disagrees"},"attributes":[{"key":"ID","value":"shared"},{"key":"typed","value":"lexical","typed_value":{"index":1,"value":-9223372036854775808}}]},{"activity":"drop","input_order":42,"attributes":[{"key":"ID","value":"drop"}]}]},{"case_id":"dup","events":[{"activity":"other","input_order":700,"attributes":[{"key":"ID","value":"shared"}]}]},{"case_id":"dup","events":[]},{"case_id":"outside","events":[{"activity":"other","input_order":1,"attributes":[{"key":"ID","value":"unique"}]}]}]})");
 assert(log.traces.size()==4&&log.event_count()==4);return log;
}
void all_option_defaults_storage_and_presence(){
 const auto o=parse_embedding_workbench_options_json("{}");assert(!o.transformer_callback&&o.provider==EmbeddingProvider::HASHING&&o.dimensions==64&&o.max_items==10000&&!o.keep_cases&&o.event_id_key.empty());
 const auto snapshot=serialize_embedding_workbench_options_json(o);contains(snapshot,R"("variant":{"name":"cases_transformers","index":0})");contains(snapshot,R"("provider":{"name":"hashing","index":0})");assert(serialize_embedding_workbench_options_json(parse_embedding_workbench_options_json(snapshot))==snapshot);
 std::set<std::string> supplied={"stale"};const auto raw=R"({"variant":-2147483648,"provider":2147483647,"embedding_model":" \ud800\u0000, ","attribute_key":"","event_id_key":"literal,\n\u0000","dimensions":18446744073709551615,"max_items":0,"keep_cases":true})";
 const auto arbitrary=parse_embedding_workbench_options_json(raw,&supplied);assert(supplied.size()==8&&!arbitrary.transformer_callback&&arbitrary.dimensions==std::numeric_limits<std::size_t>::max()&&arbitrary.max_items==0&&arbitrary.keep_cases&&arbitrary.attribute_key.empty());
 const auto encoded=serialize_embedding_workbench_options_json(arbitrary);assert(serialize_embedding_workbench_options_json(parse_embedding_workbench_options_json(encoded))==encoded);contains(encoded,R"("name":null,"index":-2147483648)");
 for(const auto* bad:{R"({"transformer_callback":{}})",R"({"dimensions":-0})",R"({"dimensions":1.0})",R"({"dimensions":1e0})",R"({"dimensions":18446744073709551616})",R"({"max_items":null})",R"({"keep_cases":0})",R"({"embedding_model":null})",R"({"variant":2147483648})",R"({"variant":"CASES_TRANSFORMERS"})",R"({"variant":{"index":0,"name":"events_transformers"}})",R"({"provider":{"index":4,"name":"hashing"}})",R"({"provider":{"name":"hashing"}})",R"({"provider":0,"provid\u0065r":0})","[]"}){
  supplied={"sentinel"};rejects([&]{(void)parse_embedding_workbench_options_json(bad,&supplied);});assert(supplied==std::set<std::string>{"sentinel"});
 }
 assert(parse_embedding_workbench_size("18446744073709551615")==std::numeric_limits<std::size_t>::max());
 for(const auto* bad:{""," 1","1 ","+1","-0","1.0","1e0","18446744073709551616"})rejects([&]{(void)parse_embedding_workbench_size(bad);});
}
void full_results_scores_and_ieee_have_no_presentation_restrictions(){
 const auto raw=R"({"variant":-1,"provider":2147483647,"embedding_model":"","attribute_key":"\ud800\u0000","dimensions":18446744073709551615,"items":[{"id":"","values":[-0.0,4.9406564584124654e-324,"NaN","INF","-INF",1.7976931348623157e308]},{"id":"dup","values":[]},{"id":"dup","values":[1.0]}]})";
 const auto result=parse_embedding_workbench_result_json(raw);assert(result.items.size()==3&&std::signbit(result.items[0].values[0])&&result.items[0].values[1]==std::numeric_limits<double>::denorm_min()&&std::isnan(result.items[0].values[2]));
 const auto encoded=serialize_embedding_workbench_result_json(result);assert(serialize_embedding_workbench_result_json(parse_embedding_workbench_result_json(encoded))==encoded);contains(encoded,"18446744073709551615");
 const auto defaults=parse_embedding_workbench_result_json("{}");assert(defaults.dimensions==0&&defaults.items.empty()&&defaults.embedding_model.empty()&&defaults.attribute_key.empty());
 const auto matches=parse_embedding_workbench_matches_json(R"([{"id":"dup","score":-0.0},{"id":"dup","score":"INF"},{"id":"","score":-3.0},{}])");assert(matches.size()==4&&std::signbit(matches[0].score)&&std::isinf(matches[1].score)&&matches[2].score==-3&&matches[3].score==0);
 const auto m=serialize_embedding_workbench_matches_json(matches);assert(serialize_embedding_workbench_matches_json(parse_embedding_workbench_matches_json(m))==m);
 const auto vector=parse_embedding_workbench_vector_json(R"([-0.0,4.9406564584124654e-324,"NaN","INF","-INF"])");assert(std::signbit(vector[0])&&std::isnan(vector[2]));const auto v=serialize_embedding_workbench_vector_json(vector);assert(serialize_embedding_workbench_vector_json(parse_embedding_workbench_vector_json(v))==v);
 for(const auto* bad:{R"({"items":[{"values":[null]}]})",R"({"items":[{"values":["nan"]}]})",R"({"items":[{"values":[1e9999]}]})",R"({"items":[{"id":1}]})",R"({"items":[{"values":null}]})",R"({"dimensions":-1})",R"({"items":[],"it\u0065ms":[]})",R"({"model":"lost"})"})rejects([&]{(void)parse_embedding_workbench_result_json(bad);});
}
void actual_hashing_profiles_and_arbitrary_text_bytes(){
 RegistryScope guard;int callbacks=0;EmbeddingOptions o;o.dimensions=32;o.transformer_callback=[&](auto,auto,auto){++callbacks;return std::vector<double>{};};set_default_embedding_provider(o.transformer_callback);
 assert(embed_text("ABC abc_123",o)==embed_text("abc abc_123",o));const auto plain=embed_text("keep Ship",o);auto v2=o;v2.embedding_model="hashing-v2";assert(embed_text("keep Ship",v2)!=plain);auto salted=o;salted.embedding_model=" literal profile ";assert(embed_text("keep Ship",salted)!=plain);
 const auto zeros=embed_text(std::string("\xff\xed\xa0\x80\xed\xb0\x80\0 \r\n",12),o);assert(std::all_of(zeros.begin(),zeros.end(),[](double x){return x==0;}));assert(callbacks==0);
}
void filter_positive_preserves_all_raw_context_and_zero_preserves_none(){
 RegistryScope guard;const auto log=full_log();const auto original=serialize_footprints_workbench_raw_log_json(log);auto o=unit_options();o.variant=EmbeddingVariant::EVENTS_TRANSFORMERS;o.event_id_key="ID";
 const auto selected=filter_embeddings_top_k(log,"keep",1,o);assert(selected.traces.size()==2&&selected.event_count()==2);assert(selected.traces[0].events[0].input_order==99);
 auto expected=log;expected.traces={log.traces[0],log.traces[1]};expected.traces[0].events.resize(1);assert(serialize_footprints_workbench_raw_log_json(selected)==serialize_footprints_workbench_raw_log_json(expected));
 o.keep_cases=true;const auto complete=filter_embeddings_top_k(log,"keep",1,o);expected=log;expected.traces.resize(3);assert(complete.traces.size()==3&&complete.traces[2].events.empty());assert(serialize_footprints_workbench_raw_log_json(complete)==serialize_footprints_workbench_raw_log_json(expected));
 assert(serialize_footprints_workbench_raw_log_json(log)==original);
 auto zero_options=o;zero_options.dimensions=0;zero_options.max_items=1;zero_options.transformer_callback=[](auto,auto,auto)->std::vector<double>{throw std::runtime_error("zero-filter must not invoke callback");};
 const auto zero=filter_embeddings_top_k(log,"keep",0,zero_options);assert(serialize_footprints_workbench_raw_log_json(zero)==serialize_footprints_workbench_raw_log_json(EventLog{}));
 zero_options.max_items=0;rejects([&]{(void)filter_embeddings_top_k(log,"keep",0,zero_options);});
 auto cases=unit_options();const auto selected_cases=filter_embeddings_top_k(log,"keep",1,cases);assert(selected_cases.traces.size()==3);cases.keep_cases=true;assert(serialize_footprints_workbench_raw_log_json(filter_embeddings_top_k(log,"keep",1,cases))==serialize_footprints_workbench_raw_log_json(selected_cases));
}

void literal_selection_first_null_shadow_ids_and_grouped_texts(){
 RegistryScope guard;auto log=full_log();std::vector<std::string> texts;std::vector<std::size_t> widths;auto o=unit_options();o.dimensions=0;o.transformer_callback=[&](std::string_view text,std::string_view model,std::size_t width){assert(model==o.embedding_model);texts.emplace_back(text);widths.push_back(width);return std::vector<double>{1,0};};
 const auto cases=to_embeddings(log,o);assert(cases.items.size()==4&&cases.dimensions==2&&o.dimensions==0);assert(texts==std::vector<std::string>({"keep drop","other","","other"}));assert(widths==std::vector<std::size_t>({0,0,0,0}));assert(cases.items[0].id=="dup"&&cases.items[2].id=="dup");
 texts.clear();widths.clear();o.variant=EmbeddingVariant::EVENTS_TRANSFORMERS;o.event_id_key="";const auto events=to_embeddings(log,o);assert(events.items.size()==4&&events.items[0].id=="0"&&events.items[1].id=="1"&&events.items[2].id=="2"&&events.items[3].id=="3");assert(texts==std::vector<std::string>({"keep","drop","other","other"}));
 auto selected=parse_declare_workbench_trace_json(R"({"case_id":"c","events":[{"activity":"raw","timestamp":"time","resource":"resource","attributes":[{"key":"literal,\n","null":true,"value":"first-null"},{"key":"literal,\n","value":"later"},{"key":"typed","value":"lexical","typed_value":{"index":1,"value":999}}]},{"activity":"","attributes":[{"key":"ID","value":""}]},{"activity":"missing","attributes":[{"key":"ID","null":true,"value":"shadow"},{"key":"ID","value":"later"}]}]})");
 EventLog custom;custom.traces={selected};o.event_id_key="ID";o.attribute_key="literal,\n";texts.clear();const auto nulls=to_embeddings(custom,o);assert(texts==std::vector<std::string>({"","",""}));assert(nulls.items[0].id=="0"&&nulls.items[1].id=="1"&&nulls.items[2].id=="2");
 o.attribute_key="typed";texts.clear();(void)to_embeddings(custom,o);assert(texts[0]=="lexical");
 for(const auto& [key,expected]:std::vector<std::pair<std::string,std::string>>{{"activity","raw"},{"concept:name","raw"},{"timestamp","time"},{"time:timestamp","time"},{"resource","resource"},{"org:resource","resource"}}){o.attribute_key=key;texts.clear();(void)to_embeddings(custom,o);assert(texts.front()==expected);}
}
void explicit_callback_registry_mock_priority_and_inference(){
 RegistryScope guard;int registered=0,explicit_calls=0;set_default_embedding_provider([&](auto,auto,std::size_t width){++registered;assert(width==2);return std::vector<double>{0,1};});auto o=unit_options();o.transformer_callback=[&](auto,auto,std::size_t width){++explicit_calls;assert(width==2);return std::vector<double>{1,0};};
 assert(embed_text("text",o)==std::vector<double>({1,0}));assert(explicit_calls==1&&registered==0);
 o.embedding_model="mock_transformer";assert(embed_text("text",o)==std::vector<double>({1,0}));assert(explicit_calls==2&&registered==0);
 o.transformer_callback={};(void)embed_text("text",o);assert(registered==0);
 o.embedding_model="real-label-not-downloaded";assert(embed_text("text",o)==std::vector<double>({0,1}));assert(registered==1);
 clear_default_embedding_provider();rejects([&]{(void)embed_text("text",o);});
 o.dimensions=0;const auto empty=filter_embeddings_top_k(full_log(),"target",0,o);assert(empty.traces.empty());
 o.embedding_model="mock_transformer";rejects([&]{(void)embed_text("text",o);});
 o.transformer_callback=[](auto,auto,std::size_t width){assert(width==0);return std::vector<double>{1,0,0};};
 assert(embed_text("text",o).size()==3);rejects([&]{(void)to_embeddings(EventLog{},o);});
 auto inconsistent=o;int count=0;inconsistent.transformer_callback=[&](auto,auto,auto){return ++count==1?std::vector<double>{1,0}:std::vector<double>{1,0,0};};rejects([&]{(void)to_embeddings(full_log(),inconsistent);});
 for(int mode=0;mode<5;++mode){auto bad=o;bad.transformer_callback=[mode](auto,auto,auto){if(mode==0)return std::vector<double>{};if(mode==1)return std::vector<double>(4097,0);if(mode==2)return std::vector<double>{std::numeric_limits<double>::quiet_NaN()};if(mode==3)return std::vector<double>{std::numeric_limits<double>::infinity()};return std::vector<double>{-std::numeric_limits<double>::infinity()};};rejects([&]{(void)embed_text("text",bad);});}
 auto wrong=o;wrong.dimensions=2;wrong.transformer_callback=[](auto,auto,auto){return std::vector<double>{1};};rejects([&]{(void)embed_text("text",wrong);});
 o.transformer_callback=[](auto,auto,auto)->std::vector<double>{throw std::runtime_error("isolated-host-exception");};bool propagated=false;try{(void)embed_text("text",o);}catch(const std::runtime_error& error){propagated=std::string(error.what())=="isolated-host-exception";}assert(propagated);
}
void actual_similarity_validation_zero_ties_and_legacy_boundaries(){
 RegistryScope guard;auto o=unit_options();int calls=0;o.transformer_callback=[&](auto,auto,auto){++calls;return std::vector<double>{1,0};};EmbeddingResult result;result.provider=o.provider;result.dimensions=2;result.items={{"a",{0,1}},{"z",{0,1}},{"",{0,1}},{"z",{0,1}}};
 const auto ranked=embedding_similarity("",result,o,0);assert(calls==1&&ranked.size()==4&&ranked[0].id=="z"&&ranked[1].id=="z"&&ranked[2].id=="a"&&ranked[3].id.empty());assert(std::all_of(ranked.begin(),ranked.end(),[](const auto& x){return x.score==0;}));assert(embedding_similarity(" \r\n",result,o,1).size()==1);assert(embedding_similarity("",result,o,std::numeric_limits<std::size_t>::max()).size()==4);
 // Native acceptance is wider than either optional legacy presentation writer.
 rejects([&]{(void)serialize_embeddings_json(result);});rejects([&]{(void)serialize_embedding_similarity_json("",result,ranked,0);});
 result.embedding_model=o.embedding_model;result.attribute_key=o.attribute_key;rejects([&]{(void)serialize_embeddings_json(result);});
 const auto stored=serialize_embedding_workbench_result_json(result);assert(parse_embedding_workbench_result_json(stored).items.size()==4);
 result.items.clear();calls=0;assert(embedding_similarity("",result,o,0).empty()&&calls==1);
 auto invalid=result;invalid.dimensions=0;calls=0;rejects([&]{(void)embedding_similarity("",invalid,o,0);});assert(calls==0);
 for(int which=0;which<6;++which){invalid=result;if(which==0)invalid.variant=EmbeddingVariant::EVENTS_TRANSFORMERS;else if(which==1)invalid.provider=EmbeddingProvider::HASHING;else if(which==2)invalid.embedding_model="different";else if(which==3)invalid.attribute_key="different";else if(which==4)invalid.items={{"x",{1}}};else invalid.items={{"x",{std::numeric_limits<double>::infinity(),0}}};calls=0;rejects([&]{(void)embedding_similarity("",invalid,o,0);});assert(calls==0);}
 result.items={{"huge",{std::numeric_limits<double>::max(),std::numeric_limits<double>::max()}}};assert(embedding_similarity("",result,o,0)[0].score==0);
 result.items={{"ok",{1,0}}};const auto good=embedding_similarity("target",result,o,0);contains(serialize_embeddings_json(result),"TO_EMBEDDINGS");contains(serialize_embedding_similarity_json("target",result,good,0),"TO_EMBEDDINGS_SIMILARITY");
 const std::string raw_target="\xff\xed\xa0\x80\xed\xb0\x80";const auto byte_matches=embedding_similarity(raw_target,result,o,0);const auto legacy=serialize_embedding_similarity_json(raw_target,result,byte_matches,0);assert(legacy.find(raw_target)!=std::string::npos);
}

void complete_byte_safe_log_result_table_and_reopen(){
 RegistryScope registry;
 const std::string ff("\xff\0tail",6);
 const std::string pair="\xed\xa0\x80\xed\xb0\x80";
 const auto bytes=ff+pair;
 auto attr=parse_declare_workbench_trace_json(R"({"attributes":[{"kind_index":1,"container_index":4,"null":false,"first_class_role_index":77,"first_class_state_index":78,"scalar_wrapper":true,"scalar_children_list":true,"typed_value":{"index":4,"value":{"value":"unparsed-date","resolution_index":99,"timezone":"not-a-timezone"}},"children":[{"key":"child","value":"lexical","typed_value":{"index":5,"value":18446744073709551615}}]}]})").attributes[0];
 attr.key=bytes;attr.value=pair;attr.first_class_source_key=ff;auto& date=std::get<NativeDate>(attr.typed_value);date.value=ff;date.timezone=pair;attr.children[0].key=ff;attr.children[0].value=bytes;attr.children[0].first_class_source_key=pair;
 EventLog log;log.attributes={attr};log.metadata.extensions={{bytes,ff,pair}};log.metadata.classifiers={{ff,{pair,bytes}}};log.metadata.trace_globals={attr};log.metadata.event_globals={attr};log.metadata.properties={attr};log.metadata.globals={{bytes,{attr}}};log.metadata.compatibility_snapshot_valid=true;log.metadata.compatibility_globals_snapshot={{pair,{attr}}};log.metadata.compatibility_trace_globals_snapshot={attr};log.metadata.compatibility_event_globals_snapshot={attr};
 Trace trace;trace.case_id=bytes;trace.case_id_source_key=ff;trace.case_id_attribute=attr;trace.attributes={attr};trace.properties={attr};Event event;event.case_id=ff;event.activity=pair;event.timestamp=bytes;event.resource=ff;event.lifecycle=pair;event.case_id_source_key=bytes;event.activity_source_key=ff;event.timestamp_source_key=pair;event.resource_source_key=bytes;event.lifecycle_source_key=ff;event.attributes={attr};event.activity_attribute=attr;event.timestamp_attribute=attr;event.resource_attribute=attr;event.lifecycle_attribute=attr;trace.events={event};log.traces={trace};
 const auto carrier=serialize_embedding_workbench_raw_log_json(log);contains(carrier,R"("native_byte_strings":"hex-v1")");const auto decoded=parse_embedding_workbench_raw_log_json(carrier);assert(serialize_embedding_workbench_raw_log_json(decoded)==carrier);assert(decoded.traces[0].case_id==bytes&&decoded.traces[0].events[0].activity==pair&&decoded.traces[0].events[0].timestamp_source_key==pair);const auto& restored=std::get<NativeDate>(decoded.attributes[0].typed_value);assert(restored.value==ff&&restored.timezone==pair&&static_cast<int>(restored.resolution)==99);assert(decoded.metadata.compatibility_snapshot_valid&&decoded.metadata.compatibility_globals_snapshot[0].scope==pair&&decoded.metadata.compatibility_event_globals_snapshot[0].children[0].value==bytes);
 EmbeddingOptions options;options.dimensions=4;const auto selected=filter_embeddings_top_k(decoded,"",1,options);const auto artifact=serialize_embedding_workbench_raw_log_json(selected);assert(artifact==carrier);assert(serialize_embedding_workbench_raw_log_json(parse_embedding_workbench_raw_log_json(artifact))==carrier);
 EmbeddingResult result;result.embedding_model=bytes;result.attribute_key=pair;result.dimensions=4;result.items={{ff,{0,-0.0,1,2}},{pair,{0,1,2,3}}};const auto r=serialize_embedding_workbench_result_json(result);contains(r,R"("native_bytes":"hex-v1")");const auto rr=parse_embedding_workbench_result_json(r);assert(rr.embedding_model==bytes&&rr.attribute_key==pair&&rr.items[0].id==ff&&rr.items[1].id==pair&&std::signbit(rr.items[0].values[1]));assert(serialize_embedding_workbench_result_json(rr)==r);
 options.embedding_model=bytes;options.attribute_key=ff;options.event_id_key=pair;const auto oo=parse_embedding_workbench_options_json(serialize_embedding_workbench_options_json(options));assert(oo.embedding_model==bytes&&oo.attribute_key==ff&&oo.event_id_key==pair&&!oo.transformer_callback);
 const auto scores=parse_embedding_workbench_matches_json(serialize_embedding_workbench_matches_json({{ff,-0.0},{pair,1}}));assert(scores[0].id==ff&&scores[1].id==pair&&std::signbit(scores[0].score));
 EventTable table;table.columns={bytes,"second"};table.rows={{std::make_optional(attr),std::nullopt}};table.attributes={attr};table.metadata=log.metadata;table.dtypes={ff,pair};EventTableIndex index;index.kind=bytes;index.dtype=ff;index.names={std::nullopt,pair};index.labels={EventTableIndexLabel{attr},EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{attr}}}};table.index=index;table.categorical_dtypes={EventTableCategoricalDtype{pair,{attr},true},std::nullopt};
 const auto tc=serialize_embedding_workbench_table_json(table);contains(tc,R"("native_schema":"event-table-raw-v1")");const auto tr=parse_embedding_workbench_table_json(tc);assert(serialize_embedding_workbench_table_json(tr)==tc&&tr.columns[0]==bytes&&tr.dtypes[0]==ff&&tr.index->kind==bytes&&tr.index->names[1]==pair&&tr.categorical_dtypes[0]->categories_dtype==pair);assert(std::get<NativeDate>(tr.rows[0][0]->typed_value).value==ff);
 EventStream stream;stream.attributes={attr};stream.metadata=log.metadata;stream.events={EventRecord{{attr}}};const auto sc=serialize_embedding_workbench_stream_json(stream);contains(sc,R"("native_schema":"event-stream-raw-v1")");contains(sc,"eda080edb080");
 for(const auto* bad:{R"({"native_byte_strings":"bad","value":{}})",R"({"native_byte_strings":"hex-v1","value":{"attributes":[{"key":"0"}]}})",R"({"native_byte_strings":"hex-v1","value":{"traces":[{"case_id":"zz"}]}})"})rejects([&]{(void)parse_embedding_workbench_raw_log_json(bad);});
 for(const auto* bad:{R"({"embedding_model":{"native_bytes":"bad","data":"ff"}})",R"({"embedding_model":{"native_bytes":"hex-v1","data":"f"}})",R"({"items":[{"id":{"native_bytes":"hex-v1","data":"xx"}}]})"})rejects([&]{(void)parse_embedding_workbench_result_json(bad);});
 assert(serialize_embedding_workbench_raw_log_json(full_log())==serialize_footprints_workbench_raw_log_json(full_log()));
}

template<class Call>void run(const char* section,Call call){std::cerr<<"[embedding_workbench_json] "<<section<<std::endl;try{call();}catch(const std::exception& error){std::cerr<<section<<": "<<error.what()<<std::endl;throw;}}
}  // namespace
int main(){
 run("all8 option defaults storage and transactional presence",all_option_defaults_storage_and_presence);
 run("full arbitrary vectors matches and result IEEE storage",full_results_scores_and_ieee_have_no_presentation_restrictions);
 run("complete FF pairedWTF date metadata table and artifact reopen",complete_byte_safe_log_result_table_and_reopen);
 run("actual hashing profiles and raw text bytes",actual_hashing_profiles_and_arbitrary_text_bytes);
 run("positive filter full metadata and distinct zero behavior",filter_positive_preserves_all_raw_context_and_zero_preserves_none);
 run("literal selection null shadows grouping and global IDs",literal_selection_first_null_shadow_ids_and_grouped_texts);
 run("explicit callback registry mock and inference isolation",explicit_callback_registry_mock_priority_and_inference);
 run("native similarity validation tie order and legacy errors",actual_similarity_validation_zero_ties_and_legacy_boundaries);
}
