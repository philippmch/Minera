#include "minera/original_model_context.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <map>
namespace minera { namespace {
std::size_t characters(std::string_view text){return std::count_if(text.begin(),text.end(),[](unsigned char c){return (c&0xc0)!=0x80;});}
LlmContextResult finish(std::string variant,std::string text){const auto count=characters(text);return {std::move(variant),std::move(text),count,false};}
// The public description uses binary64 fixed-point, two decimals, nearest-even.
// to_chars is locale independent and does not consult the process rounding mode.
std::string fixed(double value){
 if(std::isnan(value))return "nan"; // Python suppresses a NaN sign, including -NaN.
 std::array<char,512> buffer{}; // All finite binary64 fixed-point values fit (<=313 bytes).
 const auto result=std::to_chars(buffer.data(),buffer.data()+buffer.size(),value,std::chars_format::fixed,2);
 if(result.ec!=std::errc{})throw std::runtime_error("temporal description number formatting failed");
 return {buffer.data(),result.ptr};
}
// Python-style string representation for the observed string-label domain.
// Invalid native bytes remain literal bytes; arbitrary Python non-string names,
// Unicode printability classification and all quote edge cases are not proven.
std::string repr(std::string_view value){const char delimiter=value.find('\'')!=std::string_view::npos&&value.find('"')==std::string_view::npos?'"':'\'';std::string out(1,delimiter);constexpr char hex[]="0123456789abcdef";
 for(std::size_t i=0;i<value.size();++i){const unsigned char c=value[i];if(c=='\\'||c==static_cast<unsigned char>(delimiter)){out+='\\';out+=static_cast<char>(c);}else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if(c=='\t')out+="\\t";else if(c<32||c==127){out+="\\x";out+=hex[c>>4];out+=hex[c&15];}else if(c==0xed&&i+2<value.size()&&(static_cast<unsigned char>(value[i+1])&0xe0)==0xa0&&(static_cast<unsigned char>(value[i+2])&0xc0)==0x80){const unsigned code=((c&15)<<12)|((static_cast<unsigned char>(value[i+1])&63)<<6)|(static_cast<unsigned char>(value[i+2])&63);out+="\\u";for(int shift=12;shift>=0;shift-=4)out+=hex[(code>>shift)&15];i+=2;}else out+=static_cast<char>(c);}return out+delimiter;}
std::string join(const std::vector<std::string>& values,std::string_view separator){std::string text;bool first=true;for(const auto& value:values){if(!first)text+=separator;first=false;text+=value;}return text;}
std::string pair(const std::string& a,const std::string& b){return "("+repr(a)+", "+repr(b)+")";}
std::string transition(const PnmlDocument& document,std::size_t id){const auto& net=document.net;const auto& node=net.transitions().at(id);const auto name=net.transition_names()?net.transition_names()->at(id).value_or("None"):document.transition_ids.empty()?"t"+std::to_string(id):document.transition_ids.at(id);return "("+name+", "+(node.activity?repr(net.activities().at(*node.activity)):"None")+")";}
} // namespace
LlmContextResult abstract_declare(const DeclareModel& model,bool include_header){
 std::string text=include_header?"\nI have a DECLARE declarative process model containing the following constraints (here we provide a short explanation):\nExistence: the activity is executed at least once.\nAbsence: the activity is not executed.\nExactly 1: the activity is executed exactly one time.\nInitialization: the trace starts with one of the given activities.\nResponded existence: given a couple of activities (A, B), if A occurs then B also occurs.\nCo-Existence: given a couple of activities (A, B), if A occurs then B also occurs.\nResponse: given a couple of activities (A, B), if A occurs then B also occurs in the future of the trace.\nPrecedence: given a couple of activities (A, B), if B occurs then also A occurs in the past of the trace.\nSuccession: given a couple of activities (A, B), both the response and precedence constraints are satisfied.\nAlternate response, alternate precedence, alternate succession: as the constraints mentioned above, but strenghtened by specifying that the events must alternate without repetitions.\nChain response, chain precedence, chain succession: as the constraints mentioned above, strenghtened by imposing the directly-follows relation.\nNon Co-Existence: given a couple of activities (A, B), if A occurs then B should not occur.\nNon Succession and non Chain succession: given a couple of activities (A, B), B should not follow A.\n\n\n":"\n";text+="These are the constraints of the model:\n";
 static const std::map<std::string,std::string> display={{"existence","Existence"},{"absence","Absence"},{"exactly_one","Exactly 1"},{"init","Initialization"},{"responded_existence","Responded existence"},{"coexistence","Co-Existence"},{"response","Response"},{"precedence","Precedence"},{"succession","Succession"},{"altresponse","Alternate response"},{"altprecedence","Alternate precedence"},{"altsuccession","Alternate succession"},{"chainresponse","Chain response"},{"chainprecedence","Chain precedence"},{"chainsuccession","Chain succession"},{"noncoexistence","Non Co-Existence"},{"nonsuccession","Non Succession"},{"nonchainsuccession","Non Chain succession"}};
 for(const auto& [family,rules]:model.rules){if(rules.empty())continue;std::vector<std::string> items;for(const auto& rule:rules)items.push_back(rule.binary?pair(rule.first,rule.second):rule.first);const auto found=display.find(family);text+=(found==display.end()?family:found->second)+": "+join(items,", ")+"\n";}
 return finish("declare",std::move(text));
}
LlmContextResult abstract_log_skeleton(const LogSkeletonModel& model,bool include_header){
 std::string text=include_header?"\n\nThe Log Skeleton process model contains the following declarative constraints:\n- Equivalence (if the first activity occurs, then it has the same occurrences as the second one)\n- Always Before (if the first activity occur, then the second activity should have been executed previously)\n- Always After (if the first activity occur, then the second activity is executed in one of the following events)\n- Never Together (the two activities cannot co-exist inside the same case)\n- Activity Occurrences (bounds the number of occurrences for an activity in a case)\n- Directly-Follows Constraints (if the first activity occurs, then the second activity shall occur immediately after)\n    \n\nI have a Log Skeleton process model containing the following declarative constraints:\n\n":"\n";
 const auto pairs=[&](std::string_view label,const auto& entries){std::vector<std::string> values;for(const auto& entry:entries)values.push_back(pair(entry.source,entry.target));text+=std::string(label)+":  "+join(values,", ")+"\n\n";};
 pairs("Equivalence",model.equivalence);pairs("Always Before",model.always_before);pairs("Always After",model.always_after);pairs("Never Together",model.never_together);
 std::vector<std::string> frequencies;for(const auto& [activity,counts]:model.activ_freq){std::vector<std::string> values;for(const auto count:counts)values.push_back(std::to_string(count));frequencies.push_back(activity+": "+join(values,", ")+";");}
 text+="Activity Occurrences:  "+join(frequencies," ")+"\n\n";pairs("Directly-Follows Constraints",model.directly_follows);return finish("log_skeleton",std::move(text));
}
LlmContextResult abstract_temporal_profile(const TemporalProfile& profile,bool include_header){
 std::string text=include_header?"\n\nThe temporal profile is a model describing the average and the standard deviation of the times between couples of activities eventually (not only directly) following each other in at least a process execution (so in a trace <A,B,C,D> the couples (A,B) (A,C) (A,D) (B,C) (B,D) (C,D) shall be considered). Given a positive value ZETA, a deviation occurs in a process execution when the time between two activities is lower than AVG - ZETA * STDEV or greater than AVG + ZETA * STDEV.\nFor this process, the model is:\n\n":"\n\n";
 // The public empty model has only the leading/header newline, without an entry separator.
 if(profile.entries.empty())text.pop_back();
 bool first=true;for(const auto& entry:profile.entries){if(!first)text+='\n';first=false;text+=entry.source+" -> "+entry.target+" :  AVG: "+fixed(entry.mean_seconds)+" s  STD: "+fixed(entry.standard_deviation_seconds)+" s";}
 return finish("temporal_profile",std::move(text));
}
LlmContextResult abstract_petri_net(const PnmlDocument& document,bool response_header){const auto& net=document.net;
 const auto place=[&](std::size_t id){return document.place_ids.empty()?"p"+std::to_string(id):document.place_ids.at(id);};
 std::vector<std::string> places,transitions,arcs;for(std::size_t p=0;p<net.place_count();++p)places.push_back(place(p));
 for(std::size_t t=0;t<net.transitions().size();++t){const auto label=transition(document,t);transitions.push_back(label);const auto& node=net.transitions().at(t);for(auto p:node.inputs)arcs.push_back(place(p)+"->"+label);for(auto p:node.outputs)arcs.push_back(label+"->"+place(p));for(auto p:node.reset_inputs)arcs.push_back(place(p)+"->"+label);for(auto p:node.inhibitor_inputs)arcs.push_back(place(p)+"->"+label);}
 std::sort(places.begin(),places.end());std::sort(transitions.begin(),transitions.end());std::sort(arcs.begin(),arcs.end());
 std::string text="\n\n";if(response_header)text+="If I have a Petri net:\n\n";text+="places: [ "+join(places,", ")+" ]\ntransitions: [ "+join(transitions,", ")+" ]\narcs: [ "+join(arcs,", ")+" ]\n\n";
 for(const bool initial:{true,false}){const auto& marking=initial?document.initial_marking:document.final_marking;const auto& keys=initial?document.initial_marking_keys:document.final_marking_keys;std::vector<std::string> entries;for(std::size_t p=0;p<marking.size();++p){if(keys.empty()?marking[p]!=0:keys.at(p))entries.push_back(place(p)+":"+marking[p].decimal());}std::sort(entries.begin(),entries.end());std::vector<std::string> quoted;for(const auto& entry:entries)quoted.push_back(repr(entry));text+=(initial?"initial marking: [":"final marking: [")+join(quoted,", ")+"]\n";}
 text+='\n';return finish("petri_net",std::move(text));
}
LlmContextResult abstract_petri_net(const PetriNet& net,const TokenMarking& initial,const TokenMarking& final,bool response_header){PnmlDocument document;document.net=net;document.initial_marking=initial;document.final_marking=final;document.initial_marking_keys.assign(initial.size(),true);document.final_marking_keys.assign(final.size(),true);return abstract_petri_net(document,response_header);}
} // namespace minera
