#include "minera/log_skeleton_workbench_json.hpp"
#include "minera/declare_workbench_json.hpp"
#include "minera/log_conversion.hpp"

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
template<class Exception=std::invalid_argument, class Action>
void rejects(Action action) {
  bool caught=false; try {action();} catch (const Exception&) {caught=true;} assert(caught);
}
LogSkeletonOptions family(std::string name) {
  LogSkeletonOptions result; result.considered_constraints={std::move(name)}; return result;
}
void exact_storage_and_independent_option_presence() {
  const auto model=parse_log_skeleton_workbench_model(R"({"equivalence":[{"source":"B","target":"A"},{"source":"A\u0000東京","target":"\ud800"},{"source":"B","target":"A"}],"activ_freq":{"B":[2,0,2,18446744073709551615],"A":[],"\udfff":[]}})");
  assert(model.equivalence.size()==3 && model.equivalence[0]==model.equivalence[2]);
  assert(model.activ_freq.at("B")==std::vector<std::size_t>({2,0,2,std::numeric_limits<std::size_t>::max()}));
  assert(model.activ_freq.at("A").empty());
  const auto encoded=serialize_log_skeleton_workbench_model_json(model);
  assert(serialize_log_skeleton_workbench_model_json(parse_log_skeleton_workbench_model(encoded))==encoded);
  assert(encoded.find(R"("always_before":[])")!=std::string::npos);
  assert(parse_log_skeleton_workbench_model("{}").activ_freq.empty());
  std::set<std::string> members={"stale"};
  const auto defaults=parse_log_skeleton_conformance_workbench_options("{}",&members);
  assert(members.empty() && defaults.noise_threshold==0 && defaults.considered_constraints.empty() && !defaults.considered_constraints_present && defaults.activity_key=="concept:name" && defaults.case_id_key=="case:concept:name");
  const auto literal=parse_log_skeleton_conformance_workbench_options(R"({"noise_threshold":"NaN","considered_constraints":["activ_freq","","activ_freq"," unknown,\ud800\u0000 "],"considered_constraints_present":false,"activity_key":"","case_id_key":" literal \udfff "})",&members);
  assert(members.size()==5 && std::isnan(literal.noise_threshold) && literal.considered_constraints.size()==4 && !literal.considered_constraints_present && literal.activity_key.empty());
  const auto options_json=serialize_log_skeleton_workbench_options_json(literal);
  assert(serialize_log_skeleton_workbench_options_json(parse_log_skeleton_conformance_workbench_options(options_json))==options_json);
  assert(std::signbit(parse_log_skeleton_conformance_workbench_options(R"({"noise_threshold":-0.0})").noise_threshold));
  assert(parse_log_skeleton_conformance_workbench_options(R"({"noise_threshold":4.9406564584124654e-324})").noise_threshold==std::numeric_limits<double>::denorm_min());
  assert(std::isinf(parse_log_skeleton_conformance_workbench_options(R"({"noise_threshold":"-INF"})").noise_threshold));
  members={"unchanged"}; rejects([&]{(void)parse_log_skeleton_conformance_workbench_options(R"({"activity_key":null})",&members);}); assert(members==std::set<std::string>{"unchanged"});
  const auto diagnostics=parse_log_skeleton_diagnostics_workbench_options(R"({"case_id_key":" raw id "})",&members);
  assert(diagnostics.case_id_key==" raw id " && members==std::set<std::string>{"case_id_key"});
}
void six_actual_families_and_native_source_presence() {
  struct Case {const char* name;std::vector<std::string> fit,bad;};
  const Case cases[]={{"equivalence",{"A","B"},{"A","A","B"}},{"always_after",{"A","B","A"},{"B","A"}},{"always_before",{"B","A"},{"A","B"}},{"never_together",{"A"},{"A","B"}},{"directly_follows",{"A","B"},{"A","C","B"}},{"activ_freq",{"A"},{"A","A"}}};
  for (const auto& item:cases) {
    LogSkeletonModel model;
    if (std::string(item.name)=="equivalence")model.equivalence={{"A","B"},{"A","B"},{"absent","unused"}};
    else if(std::string(item.name)=="always_after")model.always_after={{"A","B"}};
    else if(std::string(item.name)=="always_before")model.always_before={{"A","B"}};
    else if(std::string(item.name)=="never_together")model.never_together={{"A","B"}};
    else if(std::string(item.name)=="directly_follows")model.directly_follows={{"A","B"}};
    else model.activ_freq["A"]={1};
    const auto restored=parse_log_skeleton_workbench_model(serialize_log_skeleton_workbench_model_json(model));
    const auto good=check_log_skeleton_conformance(item.fit,restored,family(item.name));
    const auto bad=check_log_skeleton_conformance(item.bad,restored,family(item.name));
    assert(good.is_fit && good.no_constr_total==1 && good.no_dev_total==0);
    assert(!bad.is_fit && bad.no_constr_total==1 && bad.no_dev_total==1 && bad.deviations[0].constraint==item.name);
    if(std::string(item.name)!="activ_freq") {
      const auto absent=check_log_skeleton_conformance(std::vector<std::string>{"not-source"},restored,family(item.name));
      assert(absent.is_fit && absent.no_constr_total==0);
    }
  }
  LogSkeletonModel required;required.activ_freq["A"]={2,0,2};
  const auto absent=check_log_skeleton_conformance(std::vector<std::string>{},required);
  assert(!absent.is_fit && absent.no_constr_total==1 && absent.deviations[0].allowed_frequencies==std::vector<std::size_t>({2,0,2}));
  required.activ_freq["A"]={0,2};assert(check_log_skeleton_conformance(std::vector<std::string>{},required).no_constr_total==0);
  required.activ_freq["A"]={};const auto empty=check_log_skeleton_conformance(std::vector<std::string>{"A"},required);
  assert(!empty.is_fit && empty.deviations[0].observed_frequency==1 && empty.deviations[0].allowed_frequencies.empty());
  const auto unknown=check_log_skeleton_conformance(std::vector<std::string>{"unknown","unknown"},LogSkeletonModel{});
  assert(unknown.no_constr_total==1 && unknown.deviations[0].observed_frequency==0);
  auto none=parse_log_skeleton_conformance_workbench_options(R"({"considered_constraints":[],"considered_constraints_present":true,"activity_key":"","case_id_key":"","noise_threshold":"NaN"})");
  assert(check_log_skeleton_conformance(std::vector<std::string>{"A"},required,none).is_fit);
  none.considered_constraints_present=false;assert(!check_log_skeleton_conformance(std::vector<std::string>{"A"},required,none).is_fit);
  none.considered_constraints={"unknown"," unknown "};assert(check_log_skeleton_conformance(std::vector<std::string>{"A"},required,none).no_constr_total==0);
  rejects([&]{(void)check_log_skeleton_conformance(Trace{},required,none);});
  rejects([&]{(void)check_log_skeleton_conformance(LogSkeletonVariantList{},required,none);});
}
void ordered_variant_metadata_is_not_weighted_native_conformance() {
  const auto variants=parse_log_skeleton_workbench_variants_json(R"([{"activities":["A"],"count":0},{"activities":[],"count":18446744073709551615},{"activities":["A"],"count":18446744073709551615}])");
  assert(variants.size()==3 && variants[0].second==0 && variants[1].second==std::numeric_limits<std::size_t>::max());
  assert(serialize_log_skeleton_workbench_variants_json(parse_log_skeleton_workbench_variants_json(serialize_log_skeleton_workbench_variants_json(variants)))==serialize_log_skeleton_workbench_variants_json(variants));
  LogSkeletonModel model;model.activ_freq["A"]={1};const auto output=check_log_skeleton_conformance(variants,model);
  assert(output.size()==2 && output.at({"A"}).is_fit && !output.at({}).is_fit);
  const auto encoded=serialize_log_skeleton_workbench_conformance_json(output);
  assert(encoded.starts_with(R"([{"activities":[],"value":)") && encoded.find("count")==std::string::npos);
}
void complete_result_input_retains_inactive_and_contradictory_storage() {
  const auto results=parse_log_skeleton_workbench_conformances_json(R"([{"deviations":[{"constraint":" unknown\ud800 ","relation":{"source":"inactive\u0000","target":" 東京 "},"activity":"inactive","observed_frequency":18446744073709551615,"allowed_frequencies":[2,0,2,18446744073709551615]}],"no_dev_total":18446744073709551615,"no_constr_total":9007199254740993,"dev_fitness":"NaN","is_fit":true}])");
  assert(results.size()==1 && results[0].no_dev_total==std::numeric_limits<std::size_t>::max() && results[0].no_constr_total==UINT64_C(9007199254740993) && results[0].is_fit && std::isnan(results[0].dev_fitness));
  assert(results[0].deviations[0].observed_frequency==std::numeric_limits<std::size_t>::max());
  const auto encoded=serialize_log_skeleton_workbench_conformance_json(results);
  assert(serialize_log_skeleton_workbench_conformance_json(parse_log_skeleton_workbench_conformances_json(encoded))==encoded);
  assert(encoded.find(R"("no_dev_total":18446744073709551615)")!=std::string::npos);
}
void original_trace_four_shapes_and_typed_diagnostics() {
  LogSkeletonModel model;model.directly_follows={{"A","B"}};
  const auto trace=parse_declare_workbench_trace_json(R"({"case_id":"18446744073709551615","case_id_state_index":1,"case_id_source_key":" owned id ","case_id_attribute":{"key":" owned id ","value":"18446744073709551615","kind_index":2,"typed_value":{"index":5,"value":18446744073709551615}},"properties":[{"key":"original","value":"retained"}],"events":[{"activity":"ignored","attributes":[{"key":" literal task ","value":"A"}]},{"activity":"ignored","attributes":[{"key":" literal task ","value":"B"}]}]})");
  const auto before=serialize_declare_workbench_trace_json(trace);LogSkeletonOptions custom;custom.activity_key=" literal task ";custom.case_id_key="";custom.considered_constraints={"directly_follows"};
  assert(check_log_skeleton_conformance(trace,model,custom).is_fit);
  assert(serialize_declare_workbench_trace_json(trace)==before);
  EventLog log;
  for(const auto& labels:{std::vector<std::string>{"A","B"},std::vector<std::string>{"A"}}) {
    Trace row;row.case_id="case"+std::to_string(log.traces.size());for(const auto& activity:labels)row.events.push_back(Event{row.case_id,activity});log.traces.push_back(std::move(row));
  }
  const auto options=family("directly_follows");const auto expected=check_log_skeleton_conformance(log,model,options);assert(expected[0].is_fit && !expected[1].is_fit);
  const auto stream=event_log_to_event_stream(log);const auto table=event_log_to_table(log);const auto lazy=event_log_to_lazy_table(log);
  const auto text=serialize_log_skeleton_workbench_conformance_json(expected);
  assert(serialize_log_skeleton_workbench_conformance_json(check_log_skeleton_conformance(stream,model,options))==text);
  assert(serialize_log_skeleton_workbench_conformance_json(check_log_skeleton_conformance(table,model,options))==text);
  assert(serialize_log_skeleton_workbench_conformance_json(check_log_skeleton_conformance(lazy,model,options))==text);
  for(const auto& view:{EventDataView(log),EventDataView(stream),EventDataView(table),EventDataView(lazy)}) {
    assert(serialize_log_skeleton_workbench_conformance_json(check_log_skeleton_conformance(view,model,options))==text);
    assert(log_skeleton_conformance_diagnostics(view,expected).rows.size()==2);
  }
  auto blank=options;blank.case_id_key="";assert(check_log_skeleton_conformance(EventDataView(log),model,blank).size()==2);
  rejects([&]{(void)check_log_skeleton_conformance(EventDataView(table),model,blank);});
  EventLog typed;typed.traces.push_back(trace);LogSkeletonConformance arbitrary;arbitrary.no_dev_total=7;arbitrary.no_constr_total=2;arbitrary.dev_fitness=-0.0;arbitrary.is_fit=true;arbitrary.deviations.push_back({});
  const auto diagnostics_options=parse_log_skeleton_diagnostics_workbench_options(R"({"case_id_key":" owned id "})");
  const auto diagnostics=log_skeleton_conformance_diagnostics(typed,{arbitrary},diagnostics_options);
  assert(diagnostics.columns==std::vector<std::string>({"case_id","no_dev_total","no_constr_total","dev_fitness"}));
  assert(std::get<std::uint64_t>(diagnostics.rows[0][0]->typed_value)==std::numeric_limits<std::uint64_t>::max());
  assert(diagnostics.rows[0][0]->first_class_role==FirstClassAttributeRole::kCaseId && diagnostics.rows[0][0]->first_class_source_key==" owned id ");
  assert(std::get<std::int64_t>(diagnostics.rows[0][1]->typed_value)==7 && std::get<std::int64_t>(diagnostics.rows[0][2]->typed_value)==2 && std::signbit(std::get<double>(diagnostics.rows[0][3]->typed_value)));
  for(const auto fitness:{-2.0,2.0}) {arbitrary.dev_fitness=fitness;assert(log_skeleton_conformance_diagnostics(typed,{arbitrary},diagnostics_options).rows.size()==1);}
  arbitrary.dev_fitness=std::numeric_limits<double>::quiet_NaN();rejects([&]{(void)log_skeleton_conformance_diagnostics(typed,{arbitrary},diagnostics_options);});
  arbitrary.dev_fitness=1;arbitrary.no_dev_total=std::numeric_limits<std::size_t>::max();rejects<std::overflow_error>([&]{(void)log_skeleton_conformance_diagnostics(typed,{arbitrary},diagnostics_options);});
  rejects([&]{(void)log_skeleton_conformance_diagnostics(typed,{},diagnostics_options);});
  auto missing=diagnostics_options;missing.case_id_key="missing";rejects([&]{(void)log_skeleton_conformance_diagnostics(typed,{LogSkeletonConformance{}},missing);});
}
void strict_storage_errors_are_not_native_semantic_restrictions() {
  for(const auto* value:{"null",R"({"unknown":0})",R"({"equivalence":[{"source":"A"}]})",R"({"activ_freq":{"A":[1],"\u0041":[2]}})",R"({"activ_freq":{"A":[-0]}})",R"({"activ_freq":{"A":[1.0]}})",R"({"activ_freq":{"A":[18446744073709551616]}})"})rejects([&]{(void)parse_log_skeleton_workbench_model(value);});
  for(const auto* value:{R"({"considered_constraints_present":1})",R"({"considered_constraints":null})",R"({"noise_threshold":1e9999})",R"({"noise_threshold":0,"noise_threshold":1})",R"({"case_id_key":false})"})rejects([&]{(void)parse_log_skeleton_conformance_workbench_options(value);});
  for(const auto* value:{R"([{"activities":[],"count":-1}])",R"([{"activities":[],"count":true}])",R"([{"activities":[null],"count":0}])",R"([{"activities":[],"count":1e0}])"})rejects([&]{(void)parse_log_skeleton_workbench_variants_json(value);});
  for(const auto* value:{R"([{}])",R"([{"deviations":[],"no_dev_total":-1,"no_constr_total":0,"dev_fitness":1,"is_fit":true}])",R"([{"deviations":[],"no_dev_total":0,"no_constr_total":0,"dev_fitness":null,"is_fit":true}])",R"([{"deviations":[],"no_dev_total":0,"no_constr_total":0,"dev_fitness":1,"is_fit":0}])"})rejects([&]{(void)parse_log_skeleton_workbench_conformances_json(value);});
}
}  // namespace
int main() {
  const auto run=[](const char* name,auto action){std::cerr<<"Checking "<<name<<std::endl;try{action();}catch(const std::exception& error){std::cerr<<name<<": "<<error.what()<<std::endl;throw;}};
  run("exact storage and independent options",exact_storage_and_independent_option_presence);
  run("six actual families and source presence",six_actual_families_and_native_source_presence);
  run("ordered variants and ignored counts",ordered_variant_metadata_is_not_weighted_native_conformance);
  run("complete stored results",complete_result_input_retains_inactive_and_contradictory_storage);
  run("raw Trace, four native shapes and typed diagnostics",original_trace_four_shapes_and_typed_diagnostics);
  run("strict transport without invented semantics",strict_storage_errors_are_not_native_semantic_restrictions);
}
