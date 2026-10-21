#pragma once

#include "minera/ocdfg_performance_controls.hpp"

template <class Request>
std::optional<minera::OcdfgEdgePerformanceOptions>
ocdfg_performance_options_from_request(const Request& request) {
  return minera::parse_ocdfg_performance_controls(
      [&](std::string_view name) { return request.get_param_value(std::string(name)); },
      [&](std::string_view name) { return request.has_param(std::string(name)); });
}
