#pragma once

#include "minera/ocdfg_edge_metrics_json.hpp"

template <class Request>
minera::OcdfgAssociationOptions ocdfg_edge_metrics_options_from_request(const Request& request) {
  minera::OcdfgAssociationOptions options;
  const auto assign = [&](const char* key, std::optional<std::string>& target) {
    if (request.has_param(key)) target = request.get_param_value(key);
  };
  assign("ocdfg_edge_metrics_event_id_key", options.event_id_attribute);
  assign("ocdfg_edge_metrics_object_id_key", options.object_id_attribute);
  assign("ocdfg_edge_metrics_object_type_key", options.object_type_attribute);
  assign("ocdfg_edge_metrics_activity_key", options.activity_attribute);
  assign("ocdfg_edge_metrics_timestamp_key", options.timestamp_attribute);
  return options;
}

template <class Request>
std::string ocdfg_edge_metrics_report_from_request(const minera::Ocel& log, const Request& request) {
  return minera::serialize_ocdfg_edge_metrics_json(minera::find_ocdfg_edge_associations(
      log, ocdfg_edge_metrics_options_from_request(request)));
}
