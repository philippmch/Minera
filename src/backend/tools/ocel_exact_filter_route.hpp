#pragma once
#include "minera/ocel_exact_filter_controls.hpp"
#include "minera/ocel_activity_type_matching.hpp"
struct OcelExactFilterDispatch { minera::Ocel log;std::string variant; };
std::optional<OcelExactFilterDispatch> dispatch_ocel_exact_filter(
    const minera::Ocel& source,const httplib::Request& request,
    std::string_view variant,bool positive) {
  const auto required=[&](const std::string& name){
    if(!request.has_param(name))throw std::invalid_argument(std::string(variant)+" requires "+name);
    return request.get_param_value(name);
  };
  if(variant=="event_attribute_exact" || variant=="object_attribute_exact") {
    const auto key=required("attribute_key");
    const auto values=minera::parse_ocel_event_filter_values_json(required("attribute_values_json"));
    const auto kind=minera::parse_ocel_event_attribute_column_kind(
        request.has_param("column_kind")?request.get_param_value("column_kind"):"auto");
    if(variant=="object_attribute_exact")
      return OcelExactFilterDispatch{minera::filter_ocel_object_attribute_exact(source,key,values,positive,kind),"OBJECT_ATTRIBUTE_EXACT"};
    return OcelExactFilterDispatch{minera::filter_ocel_event_attribute_exact(source,key,values,positive,kind),"EVENT_ATTRIBUTE_EXACT"};
  }
  if(variant=="timestamp_exact") {
    const auto from=minera::parse_ocel_event_timestamp_bound_json(required("from_json"));
    const auto to=minera::parse_ocel_event_timestamp_bound_json(required("to_json"));
    const auto key=request.has_param("timestamp_key")?request.get_param_value("timestamp_key"):"ocel:timestamp";
    return OcelExactFilterDispatch{minera::filter_ocel_events_timestamp_exact(source,from,to,key),"TIMESTAMP_EXACT"};
  }
  if(variant=="activity_type_matching_exact") {
    const auto allowed=minera::parse_ocel_exact_activity_type_correspondence_json(required("allowed_json"));
    minera::OcelActivityTypeMatchingOptions options;
    if(request.has_param("activity_key"))options.activity_key=request.get_param_value("activity_key");
    if(request.has_param("object_type_key"))options.object_type_key=request.get_param_value("object_type_key");
    if(request.has_param("temporary_column"))options.temporary_column=request.get_param_value("temporary_column");
    if(request.has_param("temporary_separator"))options.temporary_separator=request.get_param_value("temporary_separator");
    return OcelExactFilterDispatch{minera::filter_ocel_activity_type_matching_exact(source,allowed,options),"ACTIVITY_TYPE_MATCHING_EXACT"};
  }
  if(variant=="object_count_exact")
    return OcelExactFilterDispatch{minera::filter_ocel_events_object_type_counts_exact(source,
        minera::parse_ocel_exact_minimum_counts_json(required("minimum_counts_json"))),"OBJECT_COUNT_EXACT"};
  return std::nullopt;
}
