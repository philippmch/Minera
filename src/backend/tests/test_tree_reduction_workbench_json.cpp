#include "minera/tree_reduction_workbench_json.hpp"
#include "minera/declare_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/ptml.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using namespace minera;
using Node=ProcessTreeNode;
using Op=ProcessTreeOperator;
using Parts=std::vector<DiscoveryWorkbenchPart>;
std::string safe(std::string_view value){return serialize_embedding_workbench_string_json(std::string(value));}
ProcessTree fixture(){return {{"A","B","unused","A"},{Op::kSequence,13,{{Op::kExclusive,17,{{Op::kTau,19,{}},Node::leaf(0)}},Node::leaf(1)}}};}
Trace words(std::initializer_list<std::string> names){Trace t;for(const auto& name:names){Event e;e.activity=name;t.events.push_back(e);}return t;}
Parts request(const ProcessTree& tree,const Trace& trace){return {{"operation","","reduce_tree",false},{"model_file","model.json",serialize_tree_reduction_tree_json(tree),true},{"trace_file","trace.json",serialize_tree_reduction_trace_json(trace),true}};}
void has(const std::string& text,std::string_view expected,const std::source_location where=std::source_location::current()){
 if(text.find(expected)==std::string::npos){std::cerr<<"Missing expected fragment at "<<where.file_name()<<":"<<where.line()<<" expected_bytes="<<expected.size()<<"\nEXPECTED(first1024): "<<expected.substr(0,1024)<<"\nACTUAL_JSON(first2048 of "<<text.size()<<"): "<<text.substr(0,2048)<<std::endl;}
 assert(text.find(expected)!=std::string::npos);
}
void fails(Parts parts,std::string_view message,const Parts& query={},const std::source_location where=std::source_location::current()){const auto r=execute_tree_reduction_workbench(parts,query);assert(r.status==400);has(r.json,message,where);has(r.json,"\"native_returned\":false",where);}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
void full_trace_storage(){
 Trace t;t.case_id=std::string("case\0id",7);t.case_id_state=static_cast<FirstClassAttributeState>(-9);t.case_id_source_key=std::string(1,static_cast<char>(0xff));
 EventAttribute a;a.key="duplicate";a.value=std::string("v\0x",3);a.kind=static_cast<AttributeKind>(-4);a.container=static_cast<AttributeContainerKind>(22);a.is_null=true;a.scalar_wrapper=true;a.scalar_children_list=true;a.first_class_role=static_cast<FirstClassAttributeRole>(-7);a.first_class_state=static_cast<FirstClassAttributeState>(42);a.first_class_source_key=std::string("\xed\xa0\x80",3);
 std::vector<NativeAttributeValue> payloads;
 payloads.emplace_back();payloads.emplace_back(std::in_place_type<std::int64_t>,std::numeric_limits<std::int64_t>::min());
 for(const double d:{-0.0,std::numeric_limits<double>::denorm_min(),std::numeric_limits<double>::max(),std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()})payloads.emplace_back(std::in_place_type<double>,d);
 payloads.emplace_back(std::in_place_type<bool>,true);
 NativeDate date;date.value=std::string(1,static_cast<char>(0xfe));date.resolution=static_cast<NativeDateResolution>(-31);date.timezone="";payloads.emplace_back(std::in_place_type<NativeDate>,date);
 payloads.emplace_back(std::in_place_type<std::uint64_t>,std::numeric_limits<std::uint64_t>::max());payloads.emplace_back(std::in_place_type<NativeNA>,NativeNA{});
 for(const auto& value:payloads){EventAttribute child=a;child.children.clear();child.typed_value=value;a.children.push_back(child);}
 t.case_id_attribute=a;t.attributes={a,a};t.properties={a};
 Event e;e.case_id=t.case_id;e.activity="B";e.timestamp=std::string("\xed\xa0\x80",3);e.resource=std::string(1,static_cast<char>(0xff));e.lifecycle="literal\r\n";e.input_order=std::numeric_limits<std::size_t>::max();e.attributes={a,a};
 e.case_id_state=static_cast<FirstClassAttributeState>(-1);e.activity_state=FirstClassAttributeState::kNull;e.timestamp_state=FirstClassAttributeState::kPresent;e.resource_state=static_cast<FirstClassAttributeState>(17);e.lifecycle_state=FirstClassAttributeState::kMissing;
 e.case_id_source_key="c";e.activity_source_key="a";e.timestamp_source_key="t";e.resource_source_key="r";e.lifecycle_source_key="l";e.activity_attribute=a;e.timestamp_attribute=a;e.resource_attribute=a;e.lifecycle_attribute=a;t.events={e,e};
 const auto bytes=serialize_tree_reduction_trace_json(t);const auto p=parse_tree_reduction_trace_json(bytes);
 assert(p.case_id==t.case_id&&p.case_id_source_key==t.case_id_source_key&&p.case_id_state==t.case_id_state&&p.attributes.size()==2&&p.properties.size()==1&&p.events.size()==2);
 const auto& x=p.events.front();assert(x.input_order==e.input_order&&x.activity_state==e.activity_state&&x.resource_state==e.resource_state&&x.timestamp==e.timestamp&&x.resource==e.resource&&x.lifecycle==e.lifecycle);
 assert(x.attributes.size()==2&&x.attributes[0].children.size()==payloads.size());
 assert(x.attributes[0].kind==a.kind&&x.attributes[0].container==a.container&&x.attributes[0].is_null&&x.attributes[0].scalar_wrapper&&x.attributes[0].scalar_children_list);
 assert(x.attributes[0].first_class_role==a.first_class_role&&x.attributes[0].first_class_state==a.first_class_state&&x.attributes[0].first_class_source_key==a.first_class_source_key);
 const auto& ch=x.attributes[0].children;
 assert(ch[0].typed_value.index()==0&&std::get<std::int64_t>(ch[1].typed_value)==std::numeric_limits<std::int64_t>::min());
 assert(std::signbit(std::get<double>(ch[2].typed_value))&&std::get<double>(ch[3].typed_value)==std::numeric_limits<double>::denorm_min()&&std::get<double>(ch[4].typed_value)==std::numeric_limits<double>::max());
 assert(std::isnan(std::get<double>(ch[5].typed_value))&&std::get<double>(ch[6].typed_value)>0&&std::get<double>(ch[7].typed_value)<0);
 assert(std::get<bool>(ch[8].typed_value));const auto& d=std::get<NativeDate>(ch[9].typed_value);assert(d.value==date.value&&d.resolution==date.resolution&&d.timezone&&d.timezone->empty());assert(std::get<std::uint64_t>(ch[10].typed_value)==std::numeric_limits<std::uint64_t>::max()&&ch[11].typed_value.index()==6);
 assert(serialize_tree_reduction_trace_json(p)==bytes);has(bytes,"hex-v1");has(bytes,"-0.0");
 const auto response=execute_tree_reduction_workbench(request(fixture(),t));assert(response.status==200);has(response.json,"\"trace_unchanged\":true");has(response.json,"\"model_unchanged\":true");has(response.json,"\"original_trace\":"+safe(bytes));
 // Existing independent serializer verifies all ordinary constructor field names.
 const auto ordinary=words({"B"});assert(serialize_tree_reduction_trace_json(ordinary)==serialize_declare_workbench_trace_json(ordinary));
 // Both raw WTF8 and escaped Unicode address the same native string bytes.
 const auto escaped=parse_tree_reduction_trace_json(R"({"events":[{"activity":"\ud800"}]})");
 const auto raw=parse_tree_reduction_trace_json(std::string("{\"events\":[{\"activity\":\"")+std::string("\xed\xa0\x80",3)+"\"}]}");assert(raw.events[0].activity==escaped.events[0].activity);
 rejects([]{(void)parse_tree_reduction_trace_json(std::string("{\"case_id\":\"")+char(0xff)+"\"}");});
}
void constructor_tree_domain(){
 auto tree=fixture();const auto before=serialize_tree_reduction_tree_json(tree);const auto p=parse_tree_reduction_tree_json(before);assert(p.activities()==tree.activities()&&p.root().activity==13&&p.root().children[0].activity==17&&p.root().children[0].children[0].activity==19);assert(serialize_tree_reduction_tree_json(p)==before);
 for(int index=-1;index<=8;++index){ProcessTree t({"",std::string(1,char(0xff)),std::string("\xed\xa0\x80",3),""},{static_cast<Op>(index),std::numeric_limits<std::size_t>::max(),{}});const auto b=serialize_tree_reduction_tree_json(t);assert(serialize_tree_reduction_tree_json(parse_tree_reduction_tree_json(b))==b);}
 rejects([]{(void)parse_tree_reduction_tree_json(R"({"activities":[],"root":{"op":"TAU","op_index":1,"activity":18446744073709551616,"children":[]}})");});
 rejects([]{(void)parse_tree_reduction_tree_json(R"({"activities":[],"root":{"op":"TAU","op_index":1,"activity":0.0,"children":[]}})");});
 rejects([]{(void)parse_tree_reduction_tree_json(R"({"activities":[],"root":{"op":"SEQUENCE","op_index":1,"activity":0,"children":[]}})");});
 // No independent64-level cap truncates reachable constructor state.
 Node n=Node::tau();for(int i=0;i<80;++i)n={Op::kSequence,0,{n}};ProcessTree deep({},n);assert(serialize_tree_reduction_tree_json(parse_tree_reduction_tree_json(serialize_tree_reduction_tree_json(deep)))==serialize_tree_reduction_tree_json(deep));
}
void execution_and_native_precedence(){
 auto parts=request(fixture(),words({"B","B","unknown"}));const auto response=execute_tree_reduction_workbench(parts);assert(response.status==200);has(response.json,"\"native_entered\":true,\"native_returned\":true");
 const ProcessTree expected(fixture().activities(),Node::leaf(1));has(response.json,"\"result\":{\"process_tree\":"+serialize_tree_reduction_tree_json(expected));has(response.json,"\"model_unchanged\":true");has(response.json,"\"trace_unchanged\":true");has(response.json,"\"original_model\":"+safe(parts[1].bytes));
 auto retained=request(fixture(),words({"A"}));assert(execute_tree_reduction_workbench(retained).status==200);
 ProcessTree invalid({}, {static_cast<Op>(99),123,{}});auto bad=request(invalid,words({"B"}));bad.push_back({"tree_reduction_options_json","",R"({"variant_index":-9,"activity_key":"missing"})",false});auto result=execute_tree_reduction_workbench(bad);assert(result.status==400);has(result.json,"unsupported process tree reduction variant");has(result.json,"\"native_entered\":true");has(result.json,"\"variant_index\":-9");has(result.json,"\"op_index\":99");
 bad.back().bytes=R"({"variant_index":0,"activity_key":"missing"})";fails(bad,"process tree contains an invalid operator");
 auto missing=request(fixture(),words({"B"}));missing.push_back({"tree_reduction_activity_key","","missing",false});fails(missing,"event is missing activity attribute");
 ProcessTree out_of_range({},Node::leaf(9));auto exporter=execute_tree_reduction_workbench(request(out_of_range,Trace{}));assert(exporter.status==400);has(exporter.json,"process tree activity index is out of range");has(exporter.json,"PTML export activity index is out of range");has(exporter.json,"\"model_unchanged\":true");
 // PTML is original independent input; convenience words has literal constructor defaults.
 auto ptml=request(fixture(),Trace{});ptml[1].bytes=to_ptml(fixture());ptml[1].filename="literal.ptml";ptml[2].bytes=R"(["B","B","unknown"])";ptml.push_back({"trace_format","","trace_words",false});assert(execute_tree_reduction_workbench(ptml).status==200);
 auto empty=request(ProcessTree({"A"},{Op::kExclusive,0,{Node::tau(),Node::leaf(0)}}),Trace{});assert(execute_tree_reduction_workbench(empty).status==200);
}
void custom_key_order_and_bytes(){
 const std::string key("k\0\xff",3);Trace t=words({"literal-not-A"});EventAttribute null{key,"ignored",AttributeKind::kInt,true};EventAttribute observed{key,"B"};t.events[0].attributes={null,observed,EventAttribute{key,"A"}};
 auto p=request(fixture(),t);p.push_back({"tree_reduction_activity_key_file","",key,true});auto r=execute_tree_reduction_workbench(p);assert(r.status==200);has(r.json,"\"activity_key\":"+safe(key));const ProcessTree expected(fixture().activities(),Node::leaf(1));has(r.json,"\"result\":{\"process_tree\":"+serialize_tree_reduction_tree_json(expected));
 t.events[0].attributes[0].is_null=false;p=request(fixture(),t);p.push_back({"tree_reduction_activity_key_file","key.bin",key,true});
 r=execute_tree_reduction_workbench(p);assert(r.status==400);
 // Actual std::exception::what() observation ends at the first NUL, while
 // options/input bytes retain the complete native key (6b00ff).
 has(r.json,"\"error\":{\"type\":\"native_error\",\"message\":"+safe("activity attribute 'k")+"}");
 has(r.json,"\"native_returned\":false");
 has(r.json,"\"options\":{\"variant\":\"TREE_TR_BASED\",\"variant_index\":0,\"activity_key\":"+safe(key)+"}");
 // Without NUL, the invalid UTF8 byte survives e.what() and the full message
 // requires the NativeString byte carrier. Numeric first still wins over string.
 const std::string byte_key("k\xff",2);t=words({"literal-not-A"});
 t.events[0].attributes={EventAttribute{byte_key,"numeric-first",AttributeKind::kInt},EventAttribute{byte_key,"B"}};
 p=request(fixture(),t);p.push_back({"tree_reduction_activity_key_file","byte-key.bin",byte_key,true});
 r=execute_tree_reduction_workbench(p);assert(r.status==400);
 has(r.json,"\"error\":{\"type\":\"native_error\",\"message\":"+safe(std::string("activity attribute '")+byte_key+"' must be a string")+"}");
 has(r.json,"\"native_returned\":false");
 has(r.json,"\"options\":{\"variant\":\"TREE_TR_BASED\",\"variant_index\":0,\"activity_key\":"+safe(byte_key)+"}");
 // Default/concept:name use the literal field even when projected state is null.
 t=words({"B"});t.events[0].activity_state=FirstClassAttributeState::kNull;p=request(fixture(),t);p.push_back({"tree_reduction_activity_key","","concept:name",false});assert(execute_tree_reduction_workbench(p).status==200);
}
void controls_and_failures(){
 auto p=request(fixture(),words({"B"}));p[1].filename="";p[2].filename="";assert(execute_tree_reduction_workbench(p).status==200);
 auto text=p;text[1].file=false;fails(text,"model_file requires an actual File");
 text=p;text.push_back({"tree_reduction_options_file","",R"({"variant_index":0,"activity_key":""})",false});fails(text,"tree_reduction_options_file requires an actual File");
 auto duplicate=p;duplicate.push_back(p[1]);fails(duplicate,"duplicate tree reduction control: model_file");fails(p,"duplicate tree reduction control: operation",{{"operation","","reduce_tree",false}});
 auto full=p;full.push_back({"tree_reduction_options_file","",R"({"variant_index":0,"activity_key":""})",true});assert(execute_tree_reduction_workbench(full).status==200);
 for(const auto* field:{"tree_reduction_variant","tree_reduction_activity_key","tree_reduction_activity_key_file"}){auto conflict=full;conflict.push_back({field,"","",std::string(field).ends_with("_file")});fails(conflict,"complete tree reduction options cannot combine");}
 auto conflict=full;conflict.push_back({"tree_reduction_options_json","","",false});fails(conflict,"are mutually exclusive");conflict=p;conflict.push_back({"tree_reduction_activity_key","","",false});conflict.push_back({"tree_reduction_activity_key_file","","",true});fails(conflict,"are mutually exclusive");
 auto blob=p;blob.push_back({"tree_reduction_options_json","arbitrary.bin",R"({"variant_index":0,"activity_key":""})",true});assert(execute_tree_reduction_workbench(blob).status==200);
 auto bad=p;bad[1].bytes="broken-original\r\n";const auto r=execute_tree_reduction_workbench(bad);assert(r.status==400);has(r.json,"\"native_entered\":false");has(r.json,"\"original_model\":"+safe(bad[1].bytes));has(r.json,"\"original_trace\":"+safe(bad[2].bytes));has(r.json,"\"model_before\":null");
 const auto transport=execute_tree_reduction_workbench(p,{},"httplib_key_order","incomplete transport");assert(transport.status==400);has(transport.json,"incomplete transport");has(transport.json,"\"input_order\":\"httplib_key_order\"");has(transport.json,"\"native_entered\":false");
 for(const auto* invalid:{"null","{}",R"({"variant_index":0,"activity_key":"","extra":0})",R"({"variant_index":0,"variant_index":0,"activity_key":""})",R"({"variant_index":0.0,"activity_key":""})",R"({"variant_index":2147483648,"activity_key":""})",R"({"variant_index":0,"activity_key":{"native_bytes":"hex-v1","data":"f"}})"})rejects([&]{(void)parse_tree_reduction_options_json(invalid);});
 rejects([]{(void)parse_tree_reduction_trace_json(R"({"events":[{"input_order":-1}]})");});
 rejects([]{(void)parse_tree_reduction_trace_json(R"({"attributes":[{"typed_value":{"index":1,"value":9223372036854775808}}]})");});
 rejects([]{(void)parse_tree_reduction_trace_json(R"({"attributes":[{"typed_value":{"index":2,"value":true}}]})");});
 rejects([]{(void)parse_tree_reduction_trace_json(R"({"events":[],"\u0065vents":[]})");});
}
} // namespace
int main(){full_trace_storage();constructor_tree_domain();execution_and_native_precedence();custom_key_order_and_bytes();controls_and_failures();}
