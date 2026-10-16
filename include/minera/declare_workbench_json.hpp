#pragma once

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "minera/declare.hpp"
#include "minera/declare_playout.hpp"

namespace minera {

// Exact native map membership: rules is required; every record requires all
// five fields first/second/binary/support/confidence. Unknown and empty groups,
// ordered duplicates, signed metadata and inactive second labels are retained.
// Parsing never adds the constructor's eighteen default groups or validates
// known-family arity; the requested native operation owns that validation.
[[nodiscard]] DeclareModel parse_declare_workbench_model(std::string_view input);
[[nodiscard]] std::string serialize_declare_workbench_model_json(const DeclareModel& model);

// Exact native fields/defaults. supplied changes transactionally on success.
// All names are literal strings, integer domains are exact JSON integer tokens,
// and omitted/null optional seed differs from engaged seed zero.
[[nodiscard]] DeclareConformanceOptions parse_declare_conformance_workbench_options(
    std::string_view input, std::set<std::string>* supplied = nullptr);
[[nodiscard]] DeclareDiagnosticsOptions parse_declare_diagnostics_workbench_options(
    std::string_view input, std::set<std::string>* supplied = nullptr);
[[nodiscard]] DeclarePlayoutOptions parse_declare_playout_workbench_options(
    std::string_view input, std::set<std::string>* supplied = nullptr);
[[nodiscard]] std::string serialize_declare_workbench_options_json(const DeclareConformanceOptions& options);
[[nodiscard]] std::string serialize_declare_workbench_options_json(const DeclareDiagnosticsOptions& options);
[[nodiscard]] std::string serialize_declare_workbench_options_json(const DeclarePlayoutOptions& options);

// All five stored conformance fields and four deviation fields, including
// inactive labels and signed counts. IEEE doubles use NaN/INF/-INF tags,
// max_digits10 finite values and a distinct -0.0 representation.
[[nodiscard]] std::string serialize_declare_workbench_conformance_json(const DeclareConformance& result);
[[nodiscard]] std::string serialize_declare_workbench_conformance_json(const std::vector<DeclareConformance>& results);

// Constructor Trace/Event fields retain native names; enum states use signed
// *_index fields. attributes/properties/children are ordered raw attribute
// arrays; optional first-class attributes are raw object or null. Raw attribute
// fields: key,value,kind_index,container_index,null,scalar_wrapper,
// scalar_children_list,first_class_role_index,first_class_state_index,
// first_class_source_key,typed_value,children. typed_value={index,value} uses
// NativeAttributeValue indexes: 0=null, 1=int64, 2=IEEE double, 3=bool,
// 4={value,resolution_index,timezone:null|string}, 5=uint64, 6="NA".
// Omitted constructor fields keep native defaults. Storage is retained even
// when kind/role/state/lexical projections disagree; no table adapter grammar
// normalizes it. The private scalar alias identity is renewed on construction,
// as with existing native input carriers; it is not a writable stored field.
[[nodiscard]] Trace parse_declare_workbench_trace_json(std::string_view input);
[[nodiscard]] std::string serialize_declare_workbench_trace_json(const Trace& trace);

// All inputs are bounded to32MiB/one million values/64 levels. Literal controls,
// Unicode and escaped lone surrogates survive; invalid raw UTF-8, duplicate
// decoded object keys, unknown fields and type/storage errors reject. Output
// serializers have no input transport caps and never truncate vectors.
}  // namespace minera
