#pragma once

#include "minera/polars_attributes_filter.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "polars_ltl_original_call_parser.hpp"
#include "root_cause_trace_key.hpp"

namespace minera::polars_attributes_original_call_detail {
namespace shared=polars_ltl_original_call_detail;
[[noreturn]] inline void invalid(std::string_view message){throw std::invalid_argument("original Polars attributes call_json: "+std::string(message));}
inline void exact_fields(const shared::Node& node,std::initializer_list<std::string_view> names){
  if(node.kind!=shared::Node::kObject||node.fields.size()!=names.size())invalid("invalid typed control fields");
  for(auto name:names)if(!shared::field(node,name))invalid("missing typed control field");
}
// The unchanged Polars oracle constructs empty sequence child keys, decimal
// display values with std::to_string, and default microsecond NativeDate values.
// These are different native carriers from the shared LOG/pandas codecs.
inline EventAttribute attributes_typed(std::string_view raw,const shared::Node& node,std::string key={}){
  const auto* tag=shared::field(node,"type");const auto* value=shared::field(node,"value");
  if(node.kind!=shared::Node::kObject||!tag||tag->kind!=shared::Node::kString)invalid("typed values require a string type");
  if(tag->text=="null"){
    if(node.fields.size()!=(value?2U:1U)||(value&&value->kind!=shared::Node::kNull))invalid("null must omit value or use null");
    EventAttribute result{std::move(key),""};result.is_null=true;return result;
  }
  exact_fields(node,{"type","value"});
  const bool sequence=tag->text=="list"||tag->text=="tuple"||tag->text=="set";
  if(sequence||tag->text=="map"){
    if(value->kind!=(sequence?shared::Node::kArray:shared::Node::kObject))invalid("invalid typed container shape");
    EventAttribute result{std::move(key),""};
    result.container=tag->text=="map"?AttributeContainerKind::kContainer:tag->text=="list"?AttributeContainerKind::kList:tag->text=="tuple"?AttributeContainerKind::kTuple:AttributeContainerKind::kSet;
    if(sequence)for(const auto& child:value->items)result.children.push_back(attributes_typed(raw,child));
    else for(const auto& [name,child]:value->fields)result.children.push_back(attributes_typed(raw,child,name));
    if(tag->text=="set")for(const auto& child:result.children)if(!root_cause_key_is_hashable(child))invalid("set members require immutable typed values");
    return result;
  }
  if(tag->text!="string"&&tag->text!="boolean"&&tag->text!="integer"&&tag->text!="decimal"&&tag->text!="date")invalid("unsupported typed control tag");
  auto result=shared::typed(raw,node);result.key=std::move(key);
  if(tag->text=="decimal")result.value=std::to_string(std::get<double>(result.typed_value));
  if(tag->text=="date")result.typed_value=NativeDate{result.value};
  return result;
}
struct Call {
  PolarsAttributesFilterCall native;
  bool eager=false;
  std::vector<std::string> keyword_order;
};
inline PolarsAttributesFilterOperation operation_for(std::string_view operation){
  if(operation=="traces")return PolarsAttributesFilterOperation::kTraces;
  if(operation=="events")return PolarsAttributesFilterOperation::kEvents;
  if(operation=="numeric_traces")return PolarsAttributesFilterOperation::kNumericTraces;
  if(operation=="numeric_events")return PolarsAttributesFilterOperation::kNumericEvents;
  if(operation=="threshold")return PolarsAttributesFilterOperation::kThreshold;
  if(operation=="top_activities")return PolarsAttributesFilterOperation::kTopActivities;
  if(operation=="on_values")return PolarsAttributesFilterOperation::kOnValues;
  if(operation=="relative")return PolarsAttributesFilterOperation::kRelative;
  if(operation=="counts")return PolarsAttributesFilterOperation::kCounts;
  invalid("unsupported operation");
}
inline std::set<std::string> keywords_for(std::string_view operation){
  if(operation=="traces"||operation=="events")return {"values","parameters"};
  if(operation=="numeric_traces"||operation=="numeric_events")return {"int1","int2","parameters"};
  if(operation=="threshold")return {"thresh","act_count0","activity_key","most_common_variant"};
  if(operation=="top_activities")return {"activity_key","max_no_activities"};
  if(operation=="on_values")return {"values","case_id_glue","attribute_key","positive"};
  if(operation=="relative")return {"min_relative_stake","parameters"};
  if(operation=="counts")return {"attribute_key","parameters"};
  invalid("unsupported operation");
}
inline Call parse(std::string_view raw,std::string_view operation){
  try{
    const auto root=shared::Reader(raw).read();exact_fields(root,{"schema","input_kind","keyword_entries"});
    const auto& schema=*shared::field(root,"schema");const auto& kind=*shared::field(root,"input_kind");const auto& keywords=*shared::field(root,"keyword_entries");
    if(schema.kind!=shared::Node::kString||schema.text!="minera-polars-attributes-original-call-v1")invalid("unsupported schema");
    if(kind.kind!=shared::Node::kString||(kind.text!="DataFrame"&&kind.text!="LazyFrame"))invalid("input_kind must be DataFrame or LazyFrame");
    if(keywords.kind!=shared::Node::kArray)invalid("keyword_entries must be an array");
    Call call;call.eager=kind.text=="DataFrame";call.native.operation=operation_for(operation);
    const auto allowed=keywords_for(operation);std::set<std::string> seen;
    for(const auto& entry:keywords.items){
      exact_fields(entry,{"key","value"});const auto& key=*shared::field(entry,"key");const auto& value=*shared::field(entry,"value");
      if(key.kind!=shared::Node::kString||!allowed.contains(key.text)||!seen.insert(key.text).second)invalid("unknown or duplicate keyword");
      call.keyword_order.push_back(key.text);
      if(key.text=="parameters"){
        const auto* tag=shared::field(value,"type");
        if(tag&&tag->kind==shared::Node::kString&&tag->text=="parameter_entries"){
          exact_fields(value,{"type","entries"});const auto& entries=*shared::field(value,"entries");
          if(entries.kind!=shared::Node::kArray)invalid("parameter_entries entries must be an array");
          call.native.parameters.emplace();std::set<std::pair<std::string,bool>> identities;
          const std::set<std::string> known={"pm4py:param:attribute_key","pm4py:param:activity_key","pm4py:param:case_id_key","decreasingFactor","positive","keep_once_per_case","stream_filter_key1","stream_filter_value1","stream_filter_key2","stream_filter_value2"};
          for(const auto& parameter:entries.items){
            exact_fields(parameter,{"key","enum_key","value"});const auto& name=*shared::field(parameter,"key");const auto& flag=*shared::field(parameter,"enum_key");
            if(name.kind!=shared::Node::kString||flag.kind!=shared::Node::kBoolean||!known.contains(name.text)||!identities.insert({name.text,flag.boolean}).second)invalid("unsupported or duplicate exact parameter identity");
            call.native.parameters->push_back({name.text,attributes_typed(raw,*shared::field(parameter,"value")),flag.boolean});
          }
        }else{
          call.native.parameter_value=attributes_typed(raw,value);
          if(call.native.parameter_value->container==AttributeContainerKind::kContainer)invalid("mapping parameters require parameter_entries");
        }
        continue;
      }
      auto parsed=attributes_typed(raw,value);
      if(key.text=="values")call.native.values=std::move(parsed);
      else if(key.text=="int1")call.native.lower=std::move(parsed);
      else if(key.text=="int2")call.native.upper=std::move(parsed);
      else if(key.text=="thresh")call.native.threshold=std::move(parsed);
      else if(key.text=="max_no_activities")call.native.maximum=std::move(parsed);
      else if(key.text=="min_relative_stake")call.native.stake=std::move(parsed);
      else if(key.text=="attribute_key"){
        if(operation=="counts")call.native.attribute=std::move(parsed);else call.native.options.attribute_key=std::move(parsed);
      }else if(key.text=="activity_key")call.native.options.activity_key=std::move(parsed);
      else if(key.text=="case_id_glue")call.native.options.case_id_key=std::move(parsed);
      else if(key.text=="positive")call.native.options.positive=std::move(parsed);
      else if(key.text=="act_count0")call.native.count_cache=std::move(parsed);
      else if(key.text=="most_common_variant")call.native.most_common_variant=std::move(parsed);
    }
    return call;
  }catch(const std::invalid_argument& error){
    const std::string message=error.what();const std::string_view prefix="original Polars call_json: ";
    if(message.starts_with(prefix))invalid(std::string_view(message).substr(prefix.size()));throw;
  }
}
inline std::string cell_json(const EventAttribute& value){EventTable t;t.columns={"value"};t.dtypes={"object"};t.rows={{value}};return serialize_embedding_workbench_table_json(t);}
inline std::string controls_json(const Call& call){
  const auto q=[](std::string_view value){return serialize_embedding_workbench_string_json(value);};
  const auto cell=[&](const EventAttribute& value){return q(cell_json(value));};
  const auto optional=[&](const std::optional<EventAttribute>& value){return value?cell(*value):std::string("null");};
  const auto& n=call.native;const auto& o=n.options;
  std::string result="{\"input_kind\":"+q(call.eager?"DataFrame":"LazyFrame")+",\"operation\":"+std::to_string(static_cast<int>(n.operation))+",\"keyword_order\":[";
  bool first=true;for(const auto& name:call.keyword_order){if(!first)result+=',';first=false;result+=q(name);}result+="],\"arguments\":{";
  first=true;for(const auto& [name,value]:std::vector<std::pair<std::string,const std::optional<EventAttribute>*>>{{"values",&n.values},{"lower",&n.lower},{"upper",&n.upper},{"threshold",&n.threshold},{"maximum",&n.maximum},{"stake",&n.stake},{"attribute",&n.attribute},{"most_common_variant",&n.most_common_variant}}){if(!first)result+=',';first=false;result+=q(name)+':'+optional(*value);}
  result+="},\"options\":{\"attribute_key\":"+cell(o.attribute_key)+",\"activity_key\":"+cell(o.activity_key)+",\"case_id_key\":"+cell(o.case_id_key)+",\"positive\":"+cell(o.positive)+",\"decreasing_factor\":"+cell(o.decreasing_factor)+",\"keep_once_per_case\":"+optional(o.keep_once_per_case)+",\"stream_filter_key1\":"+optional(o.stream_filter_key1)+",\"stream_filter_value1\":"+optional(o.stream_filter_value1)+",\"stream_filter_key2\":"+optional(o.stream_filter_key2)+",\"stream_filter_value2\":"+optional(o.stream_filter_value2)+'}';
  const auto state=n.parameters?"entries":n.parameter_value?"raw":"omitted";std::string parameters="null";
  if(n.parameters){EventTable t;t.columns={"key","value","enum_key"};t.dtypes={"object","object","object"};for(const auto& p:*n.parameters)t.rows.push_back({EventAttribute{"",p.key},p.value,EventAttribute{"",p.enum_key?"true":"false",AttributeKind::kBoolean}});parameters=q(serialize_embedding_workbench_table_json(t));}
  else if(n.parameter_value)parameters=cell(*n.parameter_value);
  result+=",\"parameter_state\":"+q(state)+",\"parameters_json\":"+parameters+",\"count_cache\":{";
  if(!n.count_cache)result+="\"state\":\"omitted\",\"value_json\":null,\"entries\":[]";
  else if(const auto* raw=std::get_if<EventAttribute>(&*n.count_cache))result+="\"state\":\"value\",\"value_json\":"+cell(*raw)+",\"entries\":[]";
  else{
    result+="\"state\":\"entries\",\"value_json\":null,\"entries\":[";first=true;
    for(const auto& pair:std::get<PolarsAttributeCounts>(*n.count_cache)){if(!first)result+=',';first=false;result+="{\"key_json\":"+cell(pair.value)+",\"value_json\":"+cell(pair.count)+'}';}result+=']';
  }
  return result+"}}";
}
} // namespace minera::polars_attributes_original_call_detail
