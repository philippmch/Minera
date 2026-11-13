#include "minera/llm_context.hpp"
#include "frame_cells.hpp"
#include "frame_temporal_profile.hpp"
#include "frame_times.hpp"
#include "python_scalar.hpp"
#include "minera/llm_abstractions.hpp"
#include "minera/original_model_context.hpp"
#include "minera/dataframe_validation.hpp"
#include "minera/declare.hpp"
#include "minera/frame_keys.hpp"
#include "minera/directly_follows_graph.hpp"
#include "minera/event_table_json.hpp"
#include "minera/log_conversion.hpp"
#include "minera/log_skeleton.hpp"
#include "minera/log_to_features.hpp"
#include "minera/inductive_miner.hpp"
#include "minera/ocel_features.hpp"
#include "minera/performance.hpp"
#include "minera/process_tree.hpp"
#include "minera/temporal_profile.hpp"
#include "minera/timestamp.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include "full_double.hpp"

namespace minera { namespace {
std::string quote(std::string_view value){std::string out="\"";constexpr char hex[]="0123456789abcdef";for(const unsigned char c:value){if(c=='"')out+="\\\"";else if(c=='\\')out+="\\\\";else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if(c=='\t')out+="\\t";else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=static_cast<char>(c);}return out+'"';}
std::size_t characters(std::string_view text){return std::count_if(text.begin(),text.end(),[](unsigned char c){return (c&0xc0)!=0x80;});}
std::string fixed(double value,int precision=3){std::ostringstream out;out<<std::fixed<<std::setprecision(precision)<<value;return out.str();}
std::string number(double value){std::ostringstream out;out<<std::setprecision(16)<<value;return out.str();}
void validate(const LlmContextOptions& options){const auto variants=llm_context_variants();if(std::find(variants.begin(),variants.end(),options.variant)==variants.end())throw std::invalid_argument("unknown LLM context variant");if(!options.max_len||options.max_len>2u*1024u*1024u)throw std::invalid_argument("context character bound must be 1 through 2097152");for(const auto& aggregation:{options.primary_aggregation,options.secondary_aggregation})if(!aggregation.empty()&&aggregation!="mean"&&aggregation!="median"&&aggregation!="min"&&aggregation!="max"&&aggregation!="sum"&&aggregation!="stdev")throw std::invalid_argument("unknown context performance aggregation");}
struct Text {
 std::string value;std::size_t budget,body=0;bool truncated=false;
 explicit Text(std::size_t maximum):budget(maximum){}
 bool line(std::string text){const auto size=characters(text);if(body&&body+size>budget){truncated=true;return false;}body+=size;value+=text;return true;}
 LlmContextResult finish(std::string variant){if(value.size()>2u*1024u*1024u)throw std::length_error("context output exceeds 2 MiB");return {std::move(variant),value,characters(value),truncated};}
};
// Reference abstraction options from the context request. The exact
// abstractions (include/minera/llm_abstractions.hpp) do not report whether the
// budget cut anything, so the result is compared with an unbounded call.
LlmAbstractionOptions abstraction_options(const LlmContextOptions& options){LlmAbstractionOptions result;result.max_len=options.max_len;result.response_header=options.include_header;result.include_header=options.include_header;result.include_performance=options.include_performance;result.relative_frequency=options.relative_frequency;result.primary_performance_aggregation=options.primary_aggregation;result.secondary_performance_aggregation=options.secondary_aggregation;result.activity_key=options.activity_attribute;return result;}
template<class Describe>LlmContextResult exact(const std::string& variant,const LlmContextOptions& options,Describe describe){auto bounded=abstraction_options(options);auto text=describe(bounded);auto unbounded=bounded;unbounded.max_len=static_cast<std::size_t>(-1)/4;// A description can fail at a line max_len cuts off (a one-case variant's stdev): the bounded one is then cut.
bool truncated=true;try{truncated=describe(unbounded)!=text;}catch(const std::invalid_argument&){}Text result(options.max_len);result.value=std::move(text);result.truncated=truncated;return result.finish(variant);}
LlmContextResult whole(const std::string& variant,LlmContextResult described){Text result(static_cast<std::size_t>(-1));result.value=std::move(described.text);return result.finish(variant);}
std::string date(std::string value){if(value.size()>10&&value[10]=='T')value[10]=' ';if(!value.empty()&&value.back()=='Z'){value.pop_back();value+="+00:00";}return value;}
std::string value(const EventAttribute& attribute){if(attribute.is_null||attribute.kind==AttributeKind::kNA)return "None";if(attribute.container!=AttributeContainerKind::kScalar){EventTable table;table.columns={attribute.key};table.rows={{attribute}};return serialize_event_table_json(table);}return attribute.kind==AttributeKind::kDate?date(attribute.value):attribute.value;}
double aggregate(const std::vector<double>& samples,std::string_view kind){if(samples.empty())return 0;const auto stats=summarize_durations(samples);if(kind=="median")return stats.median;if(kind=="min")return stats.min;if(kind=="max")return stats.max;if(kind=="sum")return stats.total;if(kind=="stdev"){if(samples.size()<2)return 0;double squares=0;for(auto sample:samples)squares+=(sample-stats.mean)*(sample-stats.mean);return std::sqrt(squares/(samples.size()-1));}return stats.mean;}
EventLog projected(const EventLog& source,const LlmContextOptions& options){auto log=source;for(auto& trace:log.traces)for(auto& event:trace.events){if(!options.activity_attribute.empty()){const auto* selected=event.attribute(options.activity_attribute);if(!selected)throw std::invalid_argument("context activity attribute is absent or null");const auto original=event;const auto label=*selected;set_event_activity_context(original,label,event);event.activity_source_key=options.activity_attribute;}if(!options.timestamp_attribute.empty()){const auto* selected=event.attribute(options.timestamp_attribute);if(!selected)throw std::invalid_argument("context timestamp attribute is absent or null");event.timestamp=*selected;event.timestamp_source_key=options.timestamp_attribute;event.timestamp_attribute.reset();event.timestamp_state=FirstClassAttributeState::kPresent;}}
 // A projected log is no longer the upload's frame: its abstractions read the events.
 if(!options.activity_attribute.empty()||!options.timestamp_attribute.empty())log.dataframe_columns.reset();
 return log;}
using Edge=std::pair<std::string,std::string>;
std::string transition(const PetriNet& net,std::size_t id){const auto& item=net.transitions()[id];const auto name=net.transition_names()?(*net.transition_names())[id].value_or("None"):"t"+std::to_string(id);return "("+name+", "+(item.activity?quote(net.activities().at(*item.activity)):"None")+")";}
void petri(Text& text,const PetriNet& net,const PnmlDocument* document,bool header){
 text.value="\n\n"+(header?std::string("If I have a Petri net:\n\n"):std::string());const auto place=[&](std::size_t id){return document&&id<document->place_ids.size()?document->place_ids[id]:"p"+std::to_string(id);};
 std::string line="places: [ ";for(std::size_t p=0;p<net.place_count();++p){if(p)line+=", ";line+=place(p);}text.line(line+" ]\n");line="transitions: [ ";for(std::size_t t=0;t<net.transitions().size();++t){if(t)line+=", ";line+=transition(net,t);}text.line(line+" ]\n");
 for(std::size_t t=0;t<net.transitions().size();++t){const auto& item=net.transitions()[t];for(std::size_t a=0;a<item.inputs.size();++a){std::ostringstream out;out<<place(item.inputs[a])<<" -> "<<transition(net,t)<<" (weight = "<<(item.input_arc_weight(a).negative?"-":"")<<item.input_arc_weight(a).magnitude<<")\n";text.line(out.str());}for(std::size_t a=0;a<item.outputs.size();++a){std::ostringstream out;out<<transition(net,t)<<" -> "<<place(item.outputs[a])<<" (weight = "<<(item.output_arc_weight(a).negative?"-":"")<<item.output_arc_weight(a).magnitude<<")\n";text.line(out.str());}for(auto p:item.reset_inputs)text.line(place(p)+" -> "+transition(net,t)+" (reset arc)\n");for(auto p:item.inhibitor_inputs)text.line(place(p)+" -> "+transition(net,t)+" (inhibitor arc)\n");}
 for(bool initial:{true,false}){std::ostringstream out;out<<(initial?"initial marking: [":"final marking: [");if(document){const auto& marking=initial?document->initial_marking:document->final_marking;for(std::size_t p=0;p<marking.size();++p)if(marking[p]!=0)out<<place(p)<<":"<<marking[p]<<" ";}else if(net.discovery_markings()){for(const auto& entry:initial?net.discovery_markings()->initial:net.discovery_markings()->final)out<<(entry.place.kind==PetriMarkingPlaceKind::kRoot?place(entry.place.id):"detached"+std::to_string(entry.place.id))<<":"<<entry.tokens<<" ";}else if(initial?net.has_source():net.has_sink())out<<place(initial?net.source():net.sink())<<":1";out<<"]\n";text.line(out.str());}text.value+="\n";
}
LlmContextResult model_context(const EventLog& log,const LlmContextOptions&,const LlmContextOptions& options){
 if(options.variant=="log_skeleton")return exact("log_skeleton",options,[&](const LlmAbstractionOptions& o){return llm_log_skeleton_description(log,o);});
 // The reference model descriptions have no character budget.
 if(options.variant=="declare")return whole("declare",abstract_declare(discover_declare(log.dataframe_columns?frame_slice_cases(log):log),options.include_header));
 // A CSV or XES upload reaches pm4py as a DataFrame, whose temporal profile
 // takes pandas' durations and groupby statistics (round BH).
 return whole("temporal_profile",abstract_temporal_profile(log.dataframe_columns?discover_temporal_profile_frame(log):discover_temporal_profile(log),options.include_header));
}
// The frame-keyed log of a model description keyed by a column of an
// upload's frame (nullopt for the events' own activity or another variant).
std::optional<EventLog> model_frame_key(const EventLog& source,const LlmContextOptions& options){
 const auto& key=options.activity_attribute;
 if(!source.dataframe_columns||key.empty()||key=="concept:name"||!options.timestamp_attribute.empty()||(options.variant!="log_skeleton"&&options.variant!="declare"&&options.variant!="temporal_profile"))return std::nullopt;
 const auto columns=frame_keys(source,FrameKeyScope::kDataFrame);
 if(!columns.contains(key))throw std::invalid_argument("the specified activity column is not contained in the dataframe. Available columns: "+dataframe_available_columns({columns.begin(),columns.end()}));
 auto keyed=frame_keyed_activities(source,key,FrameKeyScope::kDataFrame);
 if(!keyed)return std::nullopt;
 if(const auto error=frame_activity_column_error(*keyed);!error.empty())throw std::invalid_argument(error);
 if(options.variant=="temporal_profile"&&key=="case:concept:name")throw std::invalid_argument("KeyError: \"['case:concept:name_2'] not in index\"");
 return std::move(keyed->log);
}
} // namespace
std::vector<std::string> llm_context_variants(){return {"case","dfg","variants","event_stream","log_attributes","log_features","declare","log_skeleton","temporal_profile","petri_net","ocel","ocel_features","ocel_ocdfg"};}
bool llm_context_is_ocel(std::string_view variant){return variant=="ocel"||variant=="ocel_features"||variant=="ocel_ocdfg";}
LlmContextResult build_llm_context(const EventLog& source,const LlmContextOptions& options,const PetriNet* supplied_net,const PnmlDocument* document){
 validate(options);if(llm_context_is_ocel(options.variant))throw std::invalid_argument("object-centric context requires an OCEL source");
 // On an upload's frame pm4py.llm takes activity_key as a column of the
 // DataFrame (round BT, observed black-box): the DFG, variants and features
 // descriptions read it (LlmAbstractionOptions::activity_key), the stream and
 // column descriptions list the frame whatever the key, and concept:name is
 // the frame's own activity. The log then keeps its frame unprojected.
 const bool frame_key=source.dataframe_columns&&!options.activity_attribute.empty()&&options.timestamp_attribute.empty()&&(options.variant=="dfg"||options.variant=="variants"||options.variant=="event_stream"||options.variant=="log_attributes"||options.variant=="log_features");
 auto projection=options;if(frame_key||(source.dataframe_columns&&options.activity_attribute=="concept:name"))projection.activity_attribute.clear();
 // The model descriptions keyed by a column of an upload's frame describe
 // pm4py.discover_log_skeleton, discover_declare and
 // discover_temporal_profile(frame, activity_key=key) (round BU, observed
 // black-box): the key is checked as a DataFrame activity column (a column
 // the frame lacks, then one of another dtype than str, then a gap), and the
 // temporal profile keyed by the case column itself fails on its merge.
 if(const auto keyed=model_frame_key(source,options)){projection.activity_attribute.clear();return model_context(*keyed,projection,options);}
 const auto log=projected(source,projection);
 // Reference abstractions (pm4py.llm abstract_dfg, abstract_variants,
 // abstract_log_attributes, abstract_log_features, abstract_event_stream,
 // abstract_log_skeleton, abstract_declare and abstract_temporal_profile).
 if(options.variant=="dfg")return exact("dfg",options,[&](const LlmAbstractionOptions& o){return llm_log_dfg_description(log,o);});
 if(options.variant=="variants")return exact("variants",options,[&](const LlmAbstractionOptions& o){return llm_log_variants_description(log,o);});
 if(options.variant=="event_stream")return exact("event_stream",options,[&](const LlmAbstractionOptions& o){return llm_event_stream_description(log,o);});
 if(options.variant=="log_attributes")return exact("log_attributes",options,[&](const LlmAbstractionOptions& o){return llm_log_columns_description(log,o.max_len);});
 if(options.variant=="log_features")return exact("log_features",options,[&](const LlmAbstractionOptions& o){return llm_log_features_description(log,o);});
 if(options.variant=="log_skeleton"||options.variant=="declare"||options.variant=="temporal_profile")return model_context(log,projection,options);
 Text text(options.max_len);
 if(options.variant=="case"){
  if(options.case_index>=log.traces.size())throw std::out_of_range("context case index is outside the log");const auto& trace=log.traces[options.case_index];text.value="\n";
  if(options.include_header)text.value+=options.include_case_attributes?"If I have a case with the following (case) attributes:\n\n":"If I have a case\n\n";
  if(options.include_case_attributes){text.line("concept:name = "+trace.case_id+"\n");for(const auto& attribute:trace.attributes)if(attribute.first_class_role!=FirstClassAttributeRole::kCaseId&&attribute.key!="concept:name"&&attribute.key!=trace.case_id_source_key)text.line(attribute.key+" = "+value(attribute)+"\n");}text.value+="\n\n";
  if(options.include_header)text.value+="the case contains the following events (the activity of the event is reported first):\n\n";
  for(const auto& event:trace.events){std::string line=event.activity;if(options.include_timestamps||options.include_event_attributes){line+=" ( ";if(options.include_timestamps&&event.has_timestamp())line+="timestamp = "+date(event.timestamp)+" ;  ";if(options.include_event_attributes){std::vector<std::pair<std::string,std::string>> attributes;for(const auto& attribute:event.attributes)if(attribute.key!=event.activity_source_key&&attribute.key!=event.timestamp_source_key&&attribute.key!="concept:name"&&attribute.key!="time:timestamp")attributes.emplace_back(attribute.key,value(attribute));if(event.has_resource()&&std::none_of(attributes.begin(),attributes.end(),[&](const auto& entry){return entry.first==event.resource_source_key||entry.first=="org:resource";}))attributes.emplace_back(event.resource_source_key.empty()?"org:resource":event.resource_source_key,event.resource);std::sort(attributes.begin(),attributes.end());for(const auto& [key,val]:attributes)line+=key+" = "+val+" ;  ";}if(line.ends_with("  "))line.pop_back();line+=")\n";}else line+=" \n";if(!text.line(line))break;}text.value+="\n";
 }else if(options.variant=="petri_net"){if(supplied_net)petri(text,*supplied_net,document,options.include_header);else{const auto net=to_petri_net(discover_process_tree(log));petri(text,net,nullptr,options.include_header);}}
 return text.finish(options.variant);
}
LlmContextResult build_llm_context(const Ocel& log,const LlmContextOptions& options){
 validate(options);if(!llm_context_is_ocel(options.variant))throw std::invalid_argument("traditional log context requires a CSV/XES source");
 if(options.variant=="ocel_ocdfg")return exact("ocel_ocdfg",options,[&](const LlmAbstractionOptions& o){return llm_ocel_ocdfg_description(log,o);});
 // pm4py.llm.abstract_ocel_features with enable_object_lifecycle_paths set
 // by include_lifecycle_paths (round BU: the paths were never described, and
 // without them a Minera feature summary was printed instead).
 if(options.variant=="ocel_features"){if(options.object_type.empty())throw std::invalid_argument("choose an object type for OCEL feature context");return exact("ocel_features",options,[&](LlmAbstractionOptions o){o.ocel_lifecycle_paths=options.include_lifecycle_paths;return llm_ocel_features_description(log,options.object_type,o);});}
 Text text(options.max_len);
 if(options.variant=="ocel"){
  // pm4py.llm.abstract_ocel on read_ocel_json's frame (round BU, observed
  // black-box): the OCEL JSON readers hold UTC microseconds (offsets convert,
  // naive times read as UTC, digits past the sixth are cut), printed as
  // str(Timestamp); the events are ordered by timestamp, activity and id; a
  // lifecycle lasts end.timestamp() - start.timestamp() seconds; the object
  // lines are joined without a final newline.
  const auto instant=[](const std::string& value)->std::optional<ParsedExactTimestamp>{auto parsed=parse_timestamp_exact(value);if(!parsed)return std::nullopt;parsed->nanoseconds-=parsed->nanoseconds%1000;return parsed;};
  const auto before=[](const ParsedExactTimestamp& a,const ParsedExactTimestamp& b){return a.seconds!=b.seconds?a.seconds<b.seconds:a.nanoseconds<b.nanoseconds;};
  std::map<std::string,std::vector<std::string>> objects;std::map<std::string,std::vector<ParsedExactTimestamp>> timestamps;std::map<std::string,const OcelEvent*> events;for(const auto& event:log.events)events[event.id]=&event;for(const auto& relation:log.relations){objects[relation.event_id].push_back(relation.object_id);if(auto found=events.find(relation.event_id);found!=events.end())if(auto stamp=instant(found->second->timestamp))timestamps[relation.object_id].push_back(*stamp);}
  std::vector<std::pair<std::optional<ParsedExactTimestamp>,const OcelEvent*>> listed_events;for(const auto& event:log.events)listed_events.emplace_back(instant(event.timestamp),&event);
  // The events print sorted by timestamp, activity and event id.
  std::stable_sort(listed_events.begin(),listed_events.end(),[&](const auto& a,const auto& b){if(a.first&&b.first&&(a.first->seconds!=b.first->seconds||a.first->nanoseconds!=b.first->nanoseconds))return before(*a.first,*b.first);if(a.second->activity!=b.second->activity)return a.second->activity<b.second->activity;return a.second->id<b.second->id;});
  text.value="\n\nevents:\n\n";for(const auto& [stamp_key,listed]:listed_events){(void)stamp_key;const auto& event=*listed;auto related=objects[event.id];std::sort(related.begin(),related.end());std::string line=event.activity+" ( related objects: ";for(std::size_t i=0;i<related.size();++i){if(i)line+=", ";line+=related[i];}line+=" ) ";if(options.include_timestamps){const auto stamp=instant(event.timestamp);line+=" timestamp: "+(stamp?frame_times::utc_text(*stamp):date(event.timestamp));}if(!text.line(line+"\n"))break;}text.value+="\nobjects:\n\n";auto ordered=log.objects;std::sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.id<b.id;});
  bool listed=false;for(const auto& object:ordered){std::string line=object.id+" object type: "+object.type;const auto& samples=timestamps[object.id];if(options.include_timestamps&&!samples.empty()){const auto [start,end]=std::minmax_element(samples.begin(),samples.end(),before);line+=" ( lifecycle start: "+frame_times::utc_text(*start)+" ; lifecycle end: "+frame_times::utc_text(*end)+" ; lifecycle duration: "+python::float_repr(frame_times::epoch_seconds(*end)-frame_times::epoch_seconds(*start))+" )";}if(!text.line(line+"\n"))break;listed=true;}
  if(listed&&text.value.ends_with('\n'))text.value.pop_back();
 }
 return text.finish(options.variant);
}
std::string llm_context_json(const LlmContextResult& result,std::string_view source_id){return "{\"family\":\"LLM_CONTEXT\",\"variant\":"+quote(result.variant)+",\"source_id\":"+quote(source_id)+",\"text\":"+quote(result.text)+",\"characters\":"+std::to_string(result.characters)+",\"truncated\":"+(result.truncated?"true":"false")+"}";}
} // namespace minera
