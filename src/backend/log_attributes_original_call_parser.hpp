#pragma once

#include "minera/log_attributes_filter.hpp"
#include "minera/embedding_workbench_json.hpp"
#include "log_ltl_original_call_parser.hpp"
#include <charconv>

namespace minera::log_attributes_original_call_detail {
namespace shared = polars_ltl_original_call_detail;

[[noreturn]] inline void invalid(std::string_view message) {
  throw std::invalid_argument("original LOG attributes call_json: " + std::string(message));
}
inline EventAttribute attribute_typed(std::string_view raw,const shared::Node& node) {
  return log_ltl_original_call_detail::log_typed(raw,node);
}
struct TraceHandle { bool auxiliary=false;std::size_t index=0; };
struct VariantEntry { EventAttribute key;std::vector<TraceHandle> traces; };
struct Call {
  std::vector<EventAttribute> args;
  std::optional<LogAttributesFilterParameters> parameters;
  std::optional<EventAttribute> raw_parameters;
  LogAttributesCountInput counts=LogAttributesCounts{};
  std::optional<EventAttribute> raw_variants;
  std::vector<VariantEntry> entries;
  bool needs_auxiliary=false;
  std::string counts_echo,variants_echo,vc_echo;
};
inline std::string attribute_json(const EventAttribute& value) {
  EventTable table;table.columns={"value"};table.dtypes={"object"};table.rows={{value}};
  return serialize_embedding_workbench_table_json(table);
}
inline std::string cache_json(const LogAttributesCountInput& counts,const LogAttributesVariantInput& variants,
    const EventAttribute& vc,const EventAttribute& attribute_key) {
  const auto q=[](std::string_view v){return serialize_embedding_workbench_string_json(v);};
  const auto cell=[&](const EventAttribute& v){return q(attribute_json(v));};
  std::string result="{\"counts\":";
  if(const auto* entries=std::get_if<LogAttributesCounts>(&counts)) {
    result+="{\"kind\":\"dictionary\",\"entries\":[";bool first=true;
    for(const auto& [key,value]:*entries){if(!first)result+=',';first=false;result+="{\"key_json\":"+cell(key)+",\"value_json\":"+cell(value)+'}';}
    result+="]}";
  }else result+="{\"kind\":\"raw\",\"value_json\":"+cell(std::get<EventAttribute>(counts))+'}';
  result+=",\"variants\":";
  if(const auto* entries=std::get_if<LogVariantsDictionary>(&variants)) {
    result+="{\"kind\":\"dictionary\",\"entries\":[";bool first=true;
    for(const auto& entry:*entries) {
      if(!first)result+=',';first=false;EventLog log;log.traces=entry.traces;
      result+="{\"key_json\":"+cell(entry.variant)+",\"traces_json\":"+q(serialize_embedding_workbench_raw_log_json(log))+
          ",\"ordinary_value_json\":"+(entry.ordinary_value?cell(*entry.ordinary_value):"null")+'}';
    }
    result+="]}";
  }else result+="{\"kind\":\"raw\",\"value_json\":"+cell(std::get<EventAttribute>(variants))+'}';
  return result+",\"vc_json\":"+cell(vc)+",\"attribute_key_json\":"+cell(attribute_key)+'}';
}
inline bool tag(const shared::Node& node,std::string_view name) {
  const auto* value=shared::field(node,"type");
  return node.kind==shared::Node::kObject&&value&&value->kind==shared::Node::kString&&value->text==name;
}
inline void exact_fields(const shared::Node& node,std::initializer_list<std::string_view> fields) {
  if(node.kind!=shared::Node::kObject||node.fields.size()!=fields.size())invalid("invalid dictionary or trace-handle fields");
  for(auto name:fields)if(!shared::field(node,name))invalid("missing dictionary or trace-handle field");
}
inline std::size_t index(std::string_view raw,const shared::Node& node) {
  const auto token=raw.substr(node.begin,node.end-node.begin);
  if(node.kind!=shared::Node::kNumber||token.empty()||token.find_first_not_of("0123456789")!=std::string_view::npos)
    invalid("trace indices require nonnegative integer JSON tokens");
  std::size_t result=0;const auto parsed=std::from_chars(token.data(),token.data()+token.size(),result);
  if(parsed.ec!=std::errc{}||parsed.ptr!=token.data()+token.size())invalid("trace index exceeds native range");
  return result;
}
inline Call parse(std::string_view raw,std::string_view operation) {
  try {
    const auto root=shared::Reader(raw).read();
    if(root.kind!=shared::Node::kObject)invalid("call must be an object");
    for(const auto& [name,value]:root.fields){(void)value;if(name!="schema"&&name!="args"&&name!="parameter_value"&&name!="parameter_entries")invalid("unknown call field");}
    const auto* schema=shared::field(root,"schema");
    if(!schema||schema->kind!=shared::Node::kString||schema->text!="minera-log-attributes-original-call-v1")invalid("unsupported or missing schema");
    const auto* args=shared::field(root,"args");const bool threshold=operation=="threshold",maximum=operation=="top_activities";
    const auto arity=operation=="numeric_traces"||operation=="numeric_events"?2U:1U;
    if(!args||args->kind!=shared::Node::kArray||
        (threshold?(args->items.size()!=4&&args->items.size()!=5):maximum?args->items.size()>1:args->items.size()!=arity))
      invalid("args has the wrong positional operand count for this operation");
    Call call;const auto* raw_parameters=shared::field(root,"parameter_value");const auto* entries=shared::field(root,"parameter_entries");
    if(raw_parameters&&entries)invalid("parameter_value and parameter_entries are mutually exclusive by presence");
    if(threshold&&(raw_parameters||entries))invalid("threshold forbids parameters by presence");
    if(raw_parameters){call.raw_parameters=attribute_typed(raw,*raw_parameters);if(call.raw_parameters->container==AttributeContainerKind::kContainer)invalid("mapping parameters require ordered parameter_entries");}
    if(entries) {
      if(entries->kind!=shared::Node::kArray)invalid("parameter_entries must be an array");
      call.parameters.emplace();std::set<std::string> names;
      const std::set<std::string> known={"pm4py:param:attribute_key","pm4py:param:activity_key","pm4py:param:case_id_key","case_id_glue","decreasingFactor","positive","stream_filter_key1","stream_filter_value1","stream_filter_key2","stream_filter_value2","keep_once_per_case"};
      for(const auto& entry:entries->items) {
        exact_fields(entry,{"key","enum_key","value"});
        const auto& key=*shared::field(entry,"key");const auto& is_enum=*shared::field(entry,"enum_key");
        if(key.kind!=shared::Node::kString||is_enum.kind!=shared::Node::kBoolean)invalid("parameter key and enum_key require string and Boolean");
        if(!known.contains(key.text))invalid("unsupported parameter identity");
        if(!names.insert(key.text).second)invalid("repeated underlying parameter names are unsupported");
        call.parameters->push_back({key.text,attribute_typed(raw,*shared::field(entry,"value")),is_enum.boolean});
      }
    }
    if(!threshold){for(const auto& value:args->items)call.args.push_back(attribute_typed(raw,value));return call;}
    const auto& count=args->items[0];const auto& variants=args->items[1];
    call.counts_echo=raw.substr(count.begin,count.end-count.begin);call.variants_echo=raw.substr(variants.begin,variants.end-variants.begin);
    call.vc_echo=raw.substr(args->items[2].begin,args->items[2].end-args->items[2].begin);
    if(tag(count,"attribute_counts")) {
      exact_fields(count,{"type","entries"});const auto& pairs=*shared::field(count,"entries");
      if(pairs.kind!=shared::Node::kArray)invalid("attribute_counts entries must be an array");
      LogAttributesCounts values;
      for(const auto& pair:pairs.items){if(pair.kind!=shared::Node::kArray||pair.items.size()!=2)invalid("attribute_counts entries require key/value pairs");values.push_back({attribute_typed(raw,pair.items[0]),attribute_typed(raw,pair.items[1])});}
      call.counts=std::move(values);
    }else{auto value=attribute_typed(raw,count);if(value.container==AttributeContainerKind::kContainer)invalid("dictionary counts require attribute_counts entries");call.counts=std::move(value);}
    if(tag(variants,"variant_cache")) {
      exact_fields(variants,{"type","entries"});const auto& values=*shared::field(variants,"entries");
      if(values.kind!=shared::Node::kArray)invalid("variant_cache entries must be an array");
      for(const auto& entry:values.items) {
        exact_fields(entry,{"key","traces"});VariantEntry value;value.key=attribute_typed(raw,*shared::field(entry,"key"));
        const auto& traces=*shared::field(entry,"traces");if(traces.kind!=shared::Node::kArray)invalid("variant cache values require arrays of complete trace handles");
        for(const auto& handle:traces.items) {
          if(handle.kind!=shared::Node::kObject||handle.fields.size()!=1)invalid("trace handles require exactly one source or auxiliary index");
          const auto* source=shared::field(handle,"source_trace_index");const auto* auxiliary=shared::field(handle,"auxiliary_trace_index");
          if(!source&&!auxiliary)invalid("unknown trace handle");
          value.traces.push_back({auxiliary!=nullptr,index(raw,auxiliary?*auxiliary:*source)});call.needs_auxiliary=call.needs_auxiliary||auxiliary!=nullptr;
        }
        call.entries.push_back(std::move(value));
      }
    }else{call.raw_variants=attribute_typed(raw,variants);if(call.raw_variants->container==AttributeContainerKind::kContainer)invalid("dictionary variants require variant_cache entries");}
    for(std::size_t i=2;i<args->items.size();++i)call.args.push_back(attribute_typed(raw,args->items[i]));
    return call;
  } catch(const std::invalid_argument& error) {
    const std::string text=error.what();
    for(std::string_view prefix:{"original Polars call_json: ","original LOG call_json: "})if(text.starts_with(prefix))invalid(std::string_view(text).substr(prefix.size()));
    throw;
  }
}
inline LogAttributesVariantInput materialize(const Call& call,const EventLog& source,const std::optional<EventLog>& auxiliary) {
  if(call.raw_variants)return *call.raw_variants;
  LogVariantsDictionary result;
  for(const auto& spec:call.entries) {
    LogVariantsDictionaryEntry entry;entry.variant=spec.key;
    for(const auto& handle:spec.traces) {
      if(handle.auxiliary&&!auxiliary)invalid("auxiliary trace requires an uploaded native log");
      const auto& log=handle.auxiliary?*auxiliary:source;
      if(handle.index>=log.traces.size())invalid("trace index is outside its actual native log");
      entry.traces.push_back(log.traces[handle.index]);
    }
    result.push_back(std::move(entry));
  }
  return result;
}
} // namespace minera::log_attributes_original_call_detail
