#pragma once
#include <charconv>
#include <functional>
#include <set>
#include "minera/llm_context.hpp"
namespace minera::context_tools {
inline const std::set<std::string> fields={"variant","max_len","case_index","include_header","include_timestamps","include_case_attributes","include_event_attributes","include_performance","relative_frequency","include_lifecycle_paths","primary_aggregation","secondary_aggregation","activity_attribute","timestamp_attribute","object_type","miner","source_id"};
inline LlmContextOptions options(const std::function<std::string(std::string_view)>& get,const std::function<bool(std::string_view)>& has){
 LlmContextOptions result;
 const auto text=[&](std::string_view key,std::string& value){if(has(key))value=get(key);};
 const auto boolean=[&](std::string_view key,bool& value){if(!has(key))return;const auto raw=get(key);if(raw=="true"||raw=="1")value=true;else if(raw=="false"||raw=="0")value=false;else throw std::invalid_argument(std::string(key)+" expects true or false");};
 const auto size=[&](std::string_view key,std::size_t& value){if(!has(key))return;const auto raw=get(key);auto parsed=std::from_chars(raw.data(),raw.data()+raw.size(),value);if(parsed.ec!=std::errc{}||parsed.ptr!=raw.data()+raw.size())throw std::invalid_argument(std::string(key)+" expects an integer");};
 text("variant",result.variant);text("primary_aggregation",result.primary_aggregation);text("secondary_aggregation",result.secondary_aggregation);text("activity_attribute",result.activity_attribute);text("timestamp_attribute",result.timestamp_attribute);text("object_type",result.object_type);
 size("max_len",result.max_len);size("case_index",result.case_index);boolean("include_header",result.include_header);boolean("include_timestamps",result.include_timestamps);boolean("include_case_attributes",result.include_case_attributes);boolean("include_event_attributes",result.include_event_attributes);boolean("include_performance",result.include_performance);boolean("relative_frequency",result.relative_frequency);boolean("include_lifecycle_paths",result.include_lifecycle_paths);return result;
}
} // namespace minera::context_tools
