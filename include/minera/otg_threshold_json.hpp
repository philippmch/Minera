#pragma once
#include <map>
#include <string>
#include <string_view>
#include "minera/ocel.hpp"
namespace minera {
// Exact native byte keys, finite binary64 tokens and explicit IEEE tags.
// Duplicate decoded keys are rejected; an empty array is an empty map.
[[nodiscard]] std::map<std::string,double> parse_otg_theta_by_relation_json(std::string_view);
// Object/edge vectors retain native order and duplicates. Graph strings also
// accept the historical raw byte spelling; NativeString is the portable form.
[[nodiscard]] OtgGraph parse_exact_otg_graph_json(std::string_view);
[[nodiscard]] std::string serialize_exact_otg_graph_json(const OtgGraph&);
[[nodiscard]] std::string serialize_exact_otg_options_json(const OtgConformanceOptions&);
[[nodiscard]] std::string serialize_exact_otg_result_json(const OtgConformanceResult&);
// Safe full result, canonical native inputs/options and original serializer bytes.
// Optional legacy serialization/transport errors never erase the native result.
[[nodiscard]] std::string serialize_exact_otg_conformance_json(
 const OtgConformanceResult&,const OtgGraph&,const OtgGraph&,
 const OtgConformanceOptions&,std::string_view original_real,
 std::string_view original_normative,std::string_view original_threshold_json);
}  // namespace minera
