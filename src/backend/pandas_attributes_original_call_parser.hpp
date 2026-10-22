#pragma once
#include "minera/pandas_attributes_filter.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "minera/timestamp.hpp"
#include "log_ltl_original_call_parser.hpp"

namespace minera::pandas_attributes_original_call_detail {
namespace shared=polars_ltl_original_call_detail;
[[noreturn]] inline void invalid(std::string_view message){throw std::invalid_argument("original pandas attributes call_json: "+std::string(message));}
inline void exact_fields(const shared::Node& node,std::initializer_list<std::string_view> keys){
  if(node.kind!=shared::Node::kObject||node.fields.size()!=keys.size())invalid("invalid typed control fields");
  for(auto key:keys)if(!shared::field(node,key))invalid("missing typed control field");
}
inline EventAttribute attributes_typed(std::string_view raw,const shared::Node& node,std::string key={}){
  const auto* tag=shared::field(node,"type");const auto* value=shared::field(node,"value");
  if(node.kind!=shared::Node::kObject||!tag||tag->kind!=shared::Node::kString)invalid("typed values require a string type");
  if(tag->text=="pandas_timestamp"){
    exact_fields(node,{"type","value","resolution","timezone"});
    const auto& unit=*shared::field(node,"resolution");const auto& zone=*shared::field(node,"timezone");
    if(!value||value->kind!=shared::Node::kString||unit.kind!=shared::Node::kString||(unit.text!="ns"&&unit.text!="us")||
        (zone.kind!=shared::Node::kNull&&(zone.kind!=shared::Node::kString||zone.text!="UTC")))invalid("pandas_timestamp requires ISO value, ns/us resolution and null/UTC timezone");
    const auto instant=parse_timestamp_exact(value->text);if(!instant)invalid("invalid pandas_timestamp ISO value");
    if(unit.text=="us"&&instant->nanoseconds%1000)invalid("microsecond timestamp contains finer precision");
    if(zone.kind==shared::Node::kString&&(!instant->timezone_aware||instant->offset_minutes!=0))invalid("UTC timestamp requires a zero-offset aware ISO value");
    EventAttribute result{std::move(key),value->text,AttributeKind::kDate};
    result.typed_value=NativeDate{value->text,unit.text=="ns"?NativeDateResolution::kNanosecond:NativeDateResolution::kMicrosecond,
        zone.kind==shared::Node::kNull?std::nullopt:std::optional<std::string>{zone.text}};return result;
  }
  const bool sequence=tag->text=="list"||tag->text=="tuple"||tag->text=="set"||tag->text=="frozenset";
  if(sequence||tag->text=="map"){
    exact_fields(node,{"type","value"});
    if(!value||value->kind!=(sequence?shared::Node::kArray:shared::Node::kObject))invalid("invalid typed container shape");
    shared::validate_map_order(node);EventAttribute result{std::move(key),""};
    result.container=tag->text=="map"?AttributeContainerKind::kContainer:tag->text=="list"?AttributeContainerKind::kList:tag->text=="tuple"?AttributeContainerKind::kTuple:tag->text=="set"?AttributeContainerKind::kSet:AttributeContainerKind::kFrozenSet;
    if(sequence)for(std::size_t i=0;i<value->items.size();++i)result.children.push_back(attributes_typed(raw,value->items[i],std::to_string(i)));
    else for(const auto& [name,child]:value->fields)result.children.push_back(attributes_typed(raw,child,name));
    if(tag->text=="set"||tag->text=="frozenset")for(const auto& child:result.children)if(!root_cause_key_is_hashable(child))invalid("set members require immutable typed values");
    return result;
  }
  auto result=log_ltl_original_call_detail::log_typed(raw,node,std::move(key));return result;
}
struct Keyword {std::string key;EventAttribute value;std::optional<PandasAttributesCountMap> count_dictionary;};
struct Call {
  std::vector<EventAttribute> args;
  std::optional<std::vector<PandasAttributesFilterParameter>> parameters;
  std::optional<EventAttribute> raw_parameters;
  std::vector<Keyword> keywords;
  bool direct=false;
};
inline const Keyword* keyword(const Call& call,std::string_view key){for(const auto& entry:call.keywords)if(entry.key==key)return &entry;return nullptr;}
inline Call parse(std::string_view raw,std::string_view operation){
  try{
    const auto root=shared::Reader(raw).read();if(root.kind!=shared::Node::kObject)invalid("call must be an object");
    for(const auto& [name,value]:root.fields){(void)value;if(name!="schema"&&name!="args"&&name!="parameter_value"&&name!="parameter_entries"&&name!="keyword_entries")invalid("unknown call field");}
    const auto* schema=shared::field(root,"schema");if(!schema||schema->kind!=shared::Node::kString||schema->text!="minera-pandas-attributes-original-call-v1")invalid("unsupported or missing schema");
    const auto* args=shared::field(root,"args");const auto arity=operation=="top_activities"?0U:operation=="numeric_traces"||operation=="numeric_events"?2U:1U;
    if(!args||args->kind!=shared::Node::kArray||args->items.size()!=arity)invalid("args has the wrong observed positional operand count");
    Call call;call.direct=operation=="threshold"||operation=="top_activities"||operation=="on_values";
    const auto* raw_parameters=shared::field(root,"parameter_value");const auto* entries=shared::field(root,"parameter_entries");const auto* keywords=shared::field(root,"keyword_entries");
    if(raw_parameters&&entries)invalid("parameter_value and parameter_entries are mutually exclusive by presence");
    if(call.direct&&(raw_parameters||entries))invalid("direct helpers forbid parameter controls by presence");
    if(!call.direct&&keywords)invalid("parameter wrappers forbid keyword_entries by presence");
    for(const auto& arg:args->items)call.args.push_back(attributes_typed(raw,arg));
    if(raw_parameters){call.raw_parameters=attributes_typed(raw,*raw_parameters);if(call.raw_parameters->container==AttributeContainerKind::kContainer)invalid("mapping parameters require ordered parameter_entries");}
    if(entries){
      if(entries->kind!=shared::Node::kArray)invalid("parameter_entries must be an array");call.parameters.emplace();std::set<std::pair<std::string,bool>> identities;
      const std::set<std::string> known={"pm4py:param:attribute_key","pm4py:param:activity_key","pm4py:param:case_id_key","decreasingFactor","positive","stream_filter_key1","stream_filter_value1","stream_filter_key2","stream_filter_value2","keep_once_per_case","keep_nan_values"};
      for(const auto& entry:entries->items){
        exact_fields(entry,{"key","enum_key","value"});const auto& key=*shared::field(entry,"key");const auto& flag=*shared::field(entry,"enum_key");
        if(key.kind!=shared::Node::kString||flag.kind!=shared::Node::kBoolean||!known.contains(key.text))invalid("unsupported parameter identity or enum flag");
        if(!identities.insert({key.text,flag.boolean}).second)invalid("duplicate exact parameter identity");
        call.parameters->push_back({attributes_typed(raw,*shared::field(entry,"value"),key.text),flag.boolean});
      }
    }
    if(keywords){
      if(keywords->kind!=shared::Node::kArray)invalid("keyword_entries must be an array");std::set<std::string> seen;
      const std::set<std::string> allowed=operation=="threshold"?std::set<std::string>{"act_count0","activity_key","most_common_variant"}:operation=="top_activities"?std::set<std::string>{"activity_key","max_no_activities"}:std::set<std::string>{"case_id_glue","attribute_key","positive","keep_nan_values"};
      for(const auto& entry:keywords->items){
        exact_fields(entry,{"key","value"});const auto& key=*shared::field(entry,"key");const auto& value=*shared::field(entry,"value");
        if(key.kind!=shared::Node::kString||!allowed.contains(key.text)||!seen.insert(key.text).second)invalid("unknown or duplicate direct keyword");
        Keyword parsed;parsed.key=key.text;const auto* tag=shared::field(value,"type");
        if(key.text=="act_count0"&&tag&&tag->kind==shared::Node::kString&&tag->text=="attribute_counts"){
          exact_fields(value,{"type","entries"});const auto& pairs=*shared::field(value,"entries");if(pairs.kind!=shared::Node::kArray)invalid("attribute_counts entries must be an array");parsed.count_dictionary.emplace();
          for(const auto& pair:pairs.items){if(pair.kind!=shared::Node::kArray||pair.items.size()!=2)invalid("attribute_counts entries require typed key/count pairs");parsed.count_dictionary->entries.emplace_back(attributes_typed(raw,pair.items[0]),attributes_typed(raw,pair.items[1]));}
        }else{parsed.value=attributes_typed(raw,value);if(key.text=="act_count0"&&parsed.value.container==AttributeContainerKind::kContainer)invalid("dictionary counts require attribute_counts entries");}
        call.keywords.push_back(std::move(parsed));
      }
    }
    return call;
  }catch(const std::invalid_argument& error){
    const std::string message=error.what();for(std::string_view prefix:{"original Polars call_json: ","original LOG call_json: "})if(message.starts_with(prefix))invalid(std::string_view(message).substr(prefix.size()));throw;
  }
}
inline PandasAttributesFilterOptions direct_options(const Call& call){
  PandasAttributesFilterOptions options;
  for(const auto& entry:call.keywords){
    if(entry.key=="act_count0"){if(entry.count_dictionary)options.act_count0=*entry.count_dictionary;else options.act_count0=entry.value;}
    else if(entry.key=="activity_key")options.activity_key=entry.value;
    else if(entry.key=="most_common_variant")options.most_common_variant=entry.value;
    else if(entry.key=="case_id_glue")options.case_id_key=entry.value;
    else if(entry.key=="attribute_key")options.attribute_key=entry.value;
    else if(entry.key=="positive")options.positive=entry.value;
    else if(entry.key=="keep_nan_values")options.keep_nan_values=entry.value;
  }return options;
}
inline std::string cell_json(const EventAttribute& value){EventTable t;t.columns={"value"};t.dtypes={"object"};t.rows={{value}};return serialize_embedding_workbench_table_json(t);}
inline std::string controls_json(const Call& call){
  const auto q=[](std::string_view text){return serialize_embedding_workbench_string_json(text);};
  EventTable args;args.rows.emplace_back();
  for(std::size_t i=0;i<call.args.size();++i){args.columns.push_back(std::to_string(i));args.dtypes.push_back("object");args.rows.back().push_back(call.args[i]);}
  std::string state=call.direct?"not_applicable":call.parameters?"entries":call.raw_parameters?"raw":"omitted",parameters="null";
  if(call.parameters){EventTable p;p.columns={"key","value","enum_key"};p.dtypes={"object","object","object"};for(const auto& entry:*call.parameters)p.rows.push_back({EventAttribute{"",entry.value.key},entry.value,EventAttribute{"",entry.enum_key?"true":"false",AttributeKind::kBoolean}});parameters=q(serialize_embedding_workbench_table_json(p));}
  else if(call.raw_parameters)parameters=q(cell_json(*call.raw_parameters));
  std::string result="{\"args_json\":"+q(serialize_embedding_workbench_table_json(args))+",\"parameter_state\":"+q(state)+",\"parameters_json\":"+parameters+",\"keyword_entries\":[";bool first=true;
  for(const auto& entry:call.keywords){
    if(!first)result+=',';first=false;result+="{\"key\":"+q(entry.key);
    if(entry.count_dictionary){result+=",\"kind\":\"count_dictionary\",\"entries\":[";bool first_pair=true;for(const auto& [key,value]:entry.count_dictionary->entries){if(!first_pair)result+=',';first_pair=false;result+="{\"key_json\":"+q(cell_json(key))+",\"value_json\":"+q(cell_json(value))+'}';}result+="]}";}
    else result+=",\"kind\":\"value\",\"value_json\":"+q(cell_json(entry.value))+'}';
  }return result+"]}";
}
} // namespace minera::pandas_attributes_original_call_detail
