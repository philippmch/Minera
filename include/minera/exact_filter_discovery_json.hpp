#pragma once
#include "minera/request_params.hpp"
#include "minera/discovery_workbench_json.hpp"
#include <set>
#include <string>
#include <string_view>
#include <vector>
namespace minera {
[[nodiscard]] bool exact_filter_discovery_control(std::string_view);
[[nodiscard]] bool exact_ltl_requested(const FieldPresence&);
[[nodiscard]] bool exact_heuristic_pairs_requested(const FieldPresence&);
[[nodiscard]] std::vector<std::string> parse_exact_ltl_values_json(std::string_view);
[[nodiscard]] bool exact_heuristic_options_requested(const FieldPresence&);
[[nodiscard]] HeuristicMinerOptions parse_exact_heuristic_options_json(std::string_view);
[[nodiscard]] HeuristicClassicDfgInput parse_exact_heuristic_dfg_json(std::string_view);
[[nodiscard]] std::string serialize_exact_heuristic_dfg_json(const HeuristicClassicDfgInput&);
[[nodiscard]] LtlFilterOptions parse_exact_ltl_options_json(std::string_view);
[[nodiscard]] std::set<std::pair<std::string,std::string>> parse_exact_heuristic_pairs_json(std::string_view);
[[nodiscard]] std::string serialize_exact_ltl_options_json(const LtlFilterOptions&);
[[nodiscard]] std::string serialize_exact_heuristic_options_json(const HeuristicMinerOptions&);
[[nodiscard]] std::string serialize_exact_heuristics_net_json(const HeuristicsNet&);
[[nodiscard]] EventLog parse_exact_ltl_log_json(std::string_view);
[[nodiscard]] DiscoveryWorkbenchResponse execute_exact_ltl_filter(
    const std::vector<DiscoveryWorkbenchPart>&,const std::vector<DiscoveryWorkbenchPart>& query={},
    std::string_view input_order="provided_order",std::string_view transport_error={});

// A request-local recorder. The scope's thread-local pointer never owns inputs,
// survives nested parser calls, and never infers a native call from HTTP status.
struct ExactFilterDiscoveryContext {
  std::vector<DiscoveryWorkbenchPart> original_inputs,query_inputs;
  std::string input_order="provided_order";
  std::string parsed_ltl="null",parsed_heuristic="null";
  std::string configured_ltl="null",configured_heuristic="null";
  std::string ltl_stage="unavailable",heuristic_stage="unavailable";
  bool heuristic_entered=false,heuristic_returned=false;
  std::string call_source_before="null",call_source_after="null",call_options_before="null",call_options_after="null",call_error_type,call_error;
  std::map<std::string,std::string> optional_artifacts;
  std::string pairset="null";
  std::string source_before="null",source_after="null",heuristics_net="null",petri_net="null";
  std::string source_before_error,source_after_error,heuristics_net_error,petri_net_error;
};
class ExactFilterDiscoveryScope {
 public:
  explicit ExactFilterDiscoveryScope(ExactFilterDiscoveryContext&);
  ~ExactFilterDiscoveryScope();
  ExactFilterDiscoveryScope(const ExactFilterDiscoveryScope&)=delete;
  ExactFilterDiscoveryScope& operator=(const ExactFilterDiscoveryScope&)=delete;
 private: ExactFilterDiscoveryContext* previous_;
};
void record_exact_parsed_options(const MineParams&);
void record_exact_effective_options(const MineParams&);
void record_exact_heuristic_options(const HeuristicMinerOptions&,bool effective);
void record_exact_source(const HeuristicClassicDfgInput&,bool after);
void record_exact_source(const EventLog&,bool after);
void record_exact_source(const EventTable&,bool after);
void record_exact_heuristics_net(const HeuristicsNet&);
void record_exact_petri_net(const PetriNet&);
bool exact_filter_discovery_recording();
std::string exact_source_snapshot(const HeuristicClassicDfgInput&);
std::string exact_source_snapshot(const EventDataView&);
void record_exact_heuristic_begin(std::string,const HeuristicMinerOptions&);
void record_exact_heuristic_end(std::string,const HeuristicMinerOptions&,const HeuristicsNet*,std::string_view={},std::string_view={});
void record_exact_optional_artifact(std::string_view,std::string_view,bool error=false);
void record_exact_snapshot_failure(std::string_view,bool after);
template<class Input,class Call>HeuristicsNet observe_exact_heuristic_call(const Input& input,const HeuristicMinerOptions& options,Call call){
 if(!exact_filter_discovery_recording())return call();
 // Snapshot errors are diagnostic only, and cannot prevent the actual kernel.
 bool after=false;
 const auto snapshot=[&](){try{return exact_source_snapshot(input);}catch(const std::exception& e){record_exact_snapshot_failure(e.what(),after);record_exact_optional_artifact("source_snapshot",e.what(),true);return std::string("null");}};
 record_exact_heuristic_begin(snapshot(),options);
 try{auto result=call();after=true;record_exact_heuristic_end(snapshot(),options,&result);return result;}
 catch(const HeuristicPandasError& e){after=true;record_exact_heuristic_end(snapshot(),options,nullptr,e.reference_type(),e.what());throw;}
 catch(const std::exception& e){after=true;record_exact_heuristic_end(snapshot(),options,nullptr,"native_error",e.what());throw;}
}
[[nodiscard]] std::string exact_filter_discovery_response(
    const ExactFilterDiscoveryContext&,std::string_view operation,int legacy_status,std::string_view legacy_body);
} // namespace minera
