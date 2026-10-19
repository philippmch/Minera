#include "minera/structural_inputs_json.hpp"
#include "minera/trie_conversion.hpp"
#include "minera/object_centric_playout_workbench_json.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/json_export.hpp"
#include "minera/pnml.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using namespace minera;
using Parts=std::vector<DiscoveryWorkbenchPart>;
static bool has(const std::string& s,const std::string& t){return s.find(t)!=std::string::npos;}
static std::string q(std::string_view s){return serialize_embedding_workbench_string_json(s);}
static Parts request(std::string op,std::string input,std::string format="direct"){
 return {{"operation","",std::move(op),false},{"input_format","",std::move(format),false},{"input_file","saved.json",std::move(input),true}};
}
static void result(const StructuralInputsResponse& r,std::string_view type,const std::string& canonical){
 assert(r.status==200);assert(has(r.json,"\"native_entered\":true,\"native_returned\":true"));
 assert(has(r.json,"\"result\":{\"type\":"+q(type)+",\"value\":"+canonical+"}"));
 assert(has(r.json,"\"input_unchanged\":true"));
}
template<class F>static void native_failure(const Parts& parts,F&& direct){
 std::string message;try{direct();assert(false);}catch(const std::exception& e){message=e.what();}
 const auto r=execute_structural_inputs(parts);assert(r.status==400);assert(has(r.json,"\"native_entered\":true,\"native_returned\":false"));assert(has(r.json,q(message)));assert(has(r.json,"\"phase\":\"native\""));assert(has(r.json,"\"input_unchanged\":true"));
}
int main(){
 // G1: empty trace plus A cannot be represented by cached trie discovery.
 TrieModel trie;trie.root.final=true;trie.root.label="ignored root";trie.root.depth=99;
 trie.root.children={{"A",{},777,false}};trie.input_traces=std::numeric_limits<std::size_t>::max();trie.distinct_variants=123;trie.node_count=999;trie.truncated=true;
 const auto canonical=serialize_structural_input_json(trie);assert(parse_structural_input_trie_json(canonical)==trie);
 const auto net=convert_trie_to_petri_net(trie);assert(net.transitions().size()==2);assert(net.transitions().front().silent());
 auto r=execute_structural_inputs(request("trie_to_petri",canonical));result(r,"PetriNet",serialize_oc_playout_workbench_petri_net_json(net));
 assert(has(r.json,"\"before\":"+canonical));assert(has(r.json,"\"after\":"+canonical));assert(has(r.json,"\"configured_options\":null"));
 assert(has(r.json,q(serialize_trie_petri_net_json(net))));assert(has(r.json,q(to_pnml(net))));
 // Native leaf semantics, root/summary/depth fields are not repaired.
 TrieModel leaf=trie;leaf.root.final=false;const auto leaf_net=convert_trie_to_petri_net(leaf);assert(leaf_net.transitions().size()==1);assert(!leaf.root.children[0].final);
 result(execute_structural_inputs(request("trie_to_petri",serialize_structural_input_json(leaf))),"PetriNet",serialize_oc_playout_workbench_petri_net_json(leaf_net));
 // Native duplicate root and sibling errors are distinct, not constructor errors.
 auto dup=trie;dup.root.children.push_back(dup.root.children.front());native_failure(request("trie_to_petri",serialize_structural_input_json(dup)),[&]{(void)convert_trie_to_petri_net(dup);});
 dup=trie;dup.root.children[0].children={{"x",{},0,false},{"x",{},0,true}};native_failure(request("trie_to_petri",serialize_structural_input_json(dup)),[&]{(void)convert_trie_to_petri_net(dup);});
 // Arbitrary bytes and large stored indices do not become UTF8/safe-integer gates.
 auto bytes=trie;bytes.root.children[0].label=std::string("A\0",2)+char(0xff);const auto raw=serialize_structural_input_json(bytes);assert(has(raw,"native_bytes"));assert(parse_structural_input_trie_json(raw)==bytes);
 r=execute_structural_inputs(request("trie_to_petri",raw));result(r,"PetriNet",serialize_oc_playout_workbench_petri_net_json(convert_trie_to_petri_net(bytes)));assert(has(r.json,"\"encoding\":\"hex-v1\""));
 // G2: unsorted caller order, duplicate entries and complete native query fields.
 std::vector<OpenPathInterval> intervals={{10,20,"later","B","C",9,8,7},{-0.0,5,"earlier","A","B",std::numeric_limits<std::size_t>::max(),1,2},{10,20,"later","B","C",9,8,7}};
 const auto full=serialize_structural_input_json(intervals);const auto reopened=parse_structural_input_intervals_json(full);assert(reopened==intervals);assert(std::signbit(reopened[1].start));
 IntervalTreeQueryOptions options;options.point=12;options.range={{5,10}};const auto queried=query_open_path_intervals(intervals,options);assert((queried.point_matches==std::vector<std::size_t>{0,2}));assert((queried.range_matches==std::vector<std::size_t>{0,1,2}));
 auto parts=request("query_intervals",full);parts.push_back({"options_json","",serialize_structural_input_json(options),false});r=execute_structural_inputs(parts);result(r,"IntervalTreeQueryResult",serialize_structural_input_json(queried));assert(has(r.json,"\"options_unchanged\":true"));assert(has(r.json,q(interval_tree_json(queried))));assert(has(r.json,"\"pnml_bytes\":null"));
 // Actual omitted default argument and explicit default object are separate forms.
 r=execute_structural_inputs(request("query_intervals",full));result(r,"IntervalTreeQueryResult",serialize_structural_input_json(query_open_path_intervals(intervals)));assert(has(r.json,"\"call_form\":\"default_options\""));assert(has(r.json,"\"options_unchanged\":null"));
 parts=request("query_intervals",full);parts.push_back({"call_form","","with_options",false});r=execute_structural_inputs(parts);result(r,"IntervalTreeQueryResult",serialize_structural_input_json(query_open_path_intervals(intervals,{})));assert(has(r.json,"\"options_unchanged\":true"));
 // Explicit native defaults and both optionals, including engaged degenerate range.
 assert(parse_structural_input_trie_json("{}")==TrieModel{});assert(parse_structural_input_intervals_json("[{}]")==std::vector<OpenPathInterval>(1));
 const auto both=parse_structural_input_query_options_json("{\"point\":-0.0,\"range\":[-0.0,0.0]}");assert(std::signbit(*both.point));assert(std::signbit(both.range->first));
 // Native validates intervals before point, then range; not host semantic validation.
 auto invalid=intervals;invalid[0].start=std::numeric_limits<double>::quiet_NaN();options.point=std::numeric_limits<double>::infinity();options.range={{10,0}};
 parts=request("query_intervals",serialize_structural_input_json(invalid));parts.push_back({"options_json","",serialize_structural_input_json(options),false});native_failure(parts,[&]{(void)query_open_path_intervals(invalid,options);});
 options.range={{10,0}};options.point=std::numeric_limits<double>::infinity();parts=request("query_intervals",full);parts.push_back({"options_json","",serialize_structural_input_json(options),false});native_failure(parts,[&]{(void)query_open_path_intervals(intervals,options);});
 options.point=1;parts=request("query_intervals",full);parts.push_back({"options_json","",serialize_structural_input_json(options),false});native_failure(parts,[&]{(void)query_open_path_intervals(intervals,options);});
 // Actual old native downloads and actual frontend download/report envelopes.
 const auto legacy_trie=trie_json(trie);r=execute_structural_inputs(request("trie_to_petri",legacy_trie,"legacy_trie"));result(r,"PetriNet",serialize_oc_playout_workbench_petri_net_json(net));
 const auto wrapped="{\"family\":\"STRUCTURAL_TRANSFORMATION\",\"source_id\":\"unrelated\",\"cases\":[],\"result\":"+legacy_trie+",\"trie\":null,\"pnml\":null}";
 r=execute_structural_inputs(request("trie_to_petri",wrapped,"legacy_transformation"));result(r,"PetriNet",serialize_oc_playout_workbench_petri_net_json(net));assert(has(r.json,"\"selection\":[\"result\"]"));
 const auto conversion="{\"family\":\"STRUCTURAL_TRANSFORMATION\",\"result\":{\"family\":\"TRIE_CONVERSION\"},\"trie\":"+legacy_trie+"}";
 r=execute_structural_inputs(request("trie_to_petri",conversion,"legacy_transformation"));result(r,"PetriNet",serialize_oc_playout_workbench_petri_net_json(net));assert(has(r.json,"\"selection\":[\"trie\"]"));
 const auto report="{\"family\":\"STRUCTURAL_REPORT\",\"request\":{},\"response\":"+wrapped+"}";
 r=execute_structural_inputs(request("trie_to_petri",report,"legacy_report"));result(r,"PetriNet",serialize_oc_playout_workbench_petri_net_json(net));assert(has(r.json,"\"selection\":[\"response\",\"result\"]"));
 const auto legacy_intervals=interval_tree_json(queried);r=execute_structural_inputs(request("query_intervals",legacy_intervals,"legacy_interval"));result(r,"IntervalTreeQueryResult",serialize_structural_input_json(query_open_path_intervals(intervals)));
 const auto interval_wrapper="{\"family\":\"STRUCTURAL_TRANSFORMATION\",\"result\":"+legacy_intervals+",\"trie\":null,\"pnml\":null}";
 r=execute_structural_inputs(request("query_intervals",interval_wrapper,"legacy_transformation"));result(r,"IntervalTreeQueryResult",serialize_structural_input_json(query_open_path_intervals(intervals)));
 r=execute_structural_inputs(request("query_intervals",serialize_structural_input_json(queried),"query_result"));result(r,"IntervalTreeQueryResult",serialize_structural_input_json(query_open_path_intervals(intervals)));
 // Malformed controls/storage do not reach native or get coerced/defaulted.
 std::vector<Parts> malformed;
 parts=request("trie_to_petri",canonical);parts.push_back({"operation","","trie_to_petri",false});malformed.push_back(parts);
 parts=request("trie_to_petri",canonical);parts.push_back({"options_json","","{}",false});malformed.push_back(parts);
 parts=request("query_intervals",full);parts.push_back({"call_form","","default_options",false});parts.push_back({"options_json","","{}",false});malformed.push_back(parts);
 malformed.push_back(request("trie_to_petri","{\"root\":{},\"root\":{}}"));malformed.push_back(request("trie_to_petri","{\"node_count\":1.0}"));malformed.push_back(request("trie_to_petri","{\"extra\":false}"));malformed.push_back(request("query_intervals","{}"));malformed.push_back(request("query_intervals",canonical,"legacy_trie"));
 for(const auto& bad:malformed){const auto response=execute_structural_inputs(bad);assert(response.status==400);assert(has(response.json,"\"native_entered\":false"));assert(has(response.json,"\"after\":null"));}
 // Actual multipart framing, empty filename, arbitrary MIME and query originals.
 const std::string body="--s83\r\nContent-Disposition: form-data; name=\"input_file\"; filename=\"\"\r\nContent-Type: strange/custom\r\n\r\n"+canonical+"\r\n--s83--\r\n";
 r=execute_structural_inputs_multipart("multipart/form-data; boundary=s83",body,{{"operation","","trie_to_petri",false},{"input_format","","direct",false}});result(r,"PetriNet",serialize_oc_playout_workbench_petri_net_json(net));assert(has(r.json,"\"filename\":\"\",\"file\":true"));assert(has(r.json,"\"origin\":\"query\""));
 r=execute_structural_inputs_multipart("multipart/form-data; boundary=s83",body,{},false);assert(r.status==400);assert(has(r.json,"\"native_entered\":false"));
}
