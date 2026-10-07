#pragma once
#include <algorithm>
#include "query_runtime.hpp"
#include "minera/log_query.hpp"
#include "minera/log_conversion.hpp"

namespace minera::log_cluster_runtime {
inline std::string selection_body(const EventLog& log,std::string_view pattern,std::string_view activity_attribute={}){
 const auto quote=embedding_runtime::detail::escape_json;
 std::string body="{\"regex\":"+quote(pattern)+",\"cases\":[";std::size_t global=0;
 for(std::size_t t=0;t<log.traces.size();++t){if(t)body+=',';const auto& trace=log.traces[t];std::string text,indices;
  for(std::size_t i=0;i<trace.events.size();++i){const auto& event=trace.events[i];std::string activity=event.activity;
   if(!activity_attribute.empty()){const auto field=std::find_if(event.attributes.begin(),event.attributes.end(),[&](const auto& value){return value.key==activity_attribute;});if(field==event.attributes.end()||field->is_null||field->container!=AttributeContainerKind::kScalar)throw std::invalid_argument("cluster activity attribute is absent or not scalar text");activity=field->value;}
   if(i){text+=" -> ";indices+=',';}text+=activity;indices+=std::to_string(global++);
  }
  body+="{\"text\":"+quote(text)+",\"indices\":["+indices+"]}";
 }
 return body+"]}";
}
inline std::vector<std::size_t> select(std::string_view runtime,const EventLog& log,std::string_view pattern,std::string_view activity_attribute={}){
 const auto body=selection_body(log,pattern,activity_attribute);const auto result=query_runtime::request(runtime,"/cluster-select",&body);
 if(result->status!=200)throw std::runtime_error("cluster expression could not be evaluated (HTTP "+std::to_string(result->status)+"): "+result->body);
 return query_runtime::parse_indices(result->body,log.event_count());
}
inline std::string table_json(const EventLog& log,const std::vector<std::size_t>& indices){
 LogConversionOptions options;options.variant=LogConversionVariant::kToDataFrame;auto result=convert_event_log(log,options);auto source=std::move(result.table.rows);result.table.rows.clear();
 const auto original_index=result.table.index;if(result.table.index)result.table.index->labels.clear();
 for(const auto index:indices){if(index>=source.size())throw std::invalid_argument("cluster event identity is outside the source table");result.table.rows.push_back(source[index]);if(result.table.index)result.table.index->labels.push_back(original_index->labels[index]);}
 return log_conversion_json(result);
}
inline std::string summary(const EventLog& filtered,std::string_view name,std::string_view pattern,const std::vector<std::size_t>& indices,std::string_view source={}){
 const auto quote=embedding_runtime::detail::escape_json;
 std::string body="{\"family\":\"LOG_CLUSTER\",\"source_id\":"+quote(source)+",\"name\":"+quote(name)+",\"regex\":"+quote(pattern)+",\"case_count\":"+std::to_string(filtered.trace_count())+",\"event_count\":"+std::to_string(filtered.event_count())+",\"selected_event_indices\":[";
 for(std::size_t i=0;i<indices.size();++i){if(i)body+=',';body+=std::to_string(indices[i]);}return body+"]}";
}
} // namespace minera::log_cluster_runtime
