#include "minera/ocel.hpp"

#include <bit>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <locale>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct HostilePunctuation : std::numpunct<char> {
  char do_decimal_point() const override { return ','; }
  char do_thousands_sep() const override { return '.'; }
  std::string do_grouping() const override { return "\3"; }
};

struct LocaleGuard {
  std::locale saved;
  explicit LocaleGuard(const std::locale& value) : saved(std::locale::global(value)) {}
  ~LocaleGuard() { std::locale::global(saved); }
};

std::vector<std::string_view> numeric_tokens(const std::string& json,
                                            const std::string& key) {
  std::vector<std::string_view> values;
  const std::string marker='"'+key+'"'+':';
  std::size_t position=0;
  while ((position=json.find(marker,position))!=std::string::npos) {
    const auto begin=position+marker.size();
    const auto end=json.find_first_of(",}",begin);
    assert(end!=std::string::npos);
    values.push_back(std::string_view(json).substr(begin,end-begin));
    position=end;
  }
  return values;
}

void same_real(const std::string& json, const std::string& key,
                double expected, std::size_t occurrences) {
  const auto tokens=numeric_tokens(json,key);
  assert(tokens.size()==occurrences);
  for (const auto token:tokens) {
    if (!std::isfinite(expected)) { assert(token=="null"); continue; }
    double restored=0.0;
    const auto parsed=std::from_chars(token.data(),token.data()+token.size(),restored);
    assert(parsed.ec==std::errc{} && parsed.ptr==token.data()+token.size());
    assert(std::bit_cast<std::uint64_t>(restored)==std::bit_cast<std::uint64_t>(expected));
    if (expected==0.0 && std::signbit(expected)) assert(token=="-0.0");
  }
}

void same_integer(const std::string& json, const std::string& key,
                   std::int64_t expected) {
  const auto tokens=numeric_tokens(json,key);
  assert(tokens.size()==1);
  std::int64_t restored=0;
  const auto token=tokens.front();
  const auto parsed=std::from_chars(token.data(),token.data()+token.size(),restored);
  assert(parsed.ec==std::errc{} && parsed.ptr==token.data()+token.size());
  assert(restored==expected);
}

void conformance_scalar_json_roundtrip() {
  const std::vector<double> values={0.0,-0.0,1.0/3.0,std::nextafter(1.0,2.0),
      1234567.125,-1234567.125,1e-200,-1e200,
      std::numeric_limits<double>::denorm_min(),std::numeric_limits<double>::min(),
      std::numeric_limits<double>::max(),std::numeric_limits<double>::infinity(),
      -std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()};
  for (const double value:values) {
    minera::EtotConformanceResult etot;
    etot.fitness=value;
    etot.relative_frequency_delta={{{"A","T"},value},{{"B","U"},value}};
    const auto etot_json=minera::etot_conformance_json(etot);
    same_real(etot_json,"fitness",value,1);
    same_real(etot_json,"delta",value,2);
    minera::OtgConformanceResult otg;
    otg.fitness=value;
    otg.non_conforming_edges={{{"T","one","U"},value},{{"U","two","T"},value}};
    const auto otg_json=minera::otg_conformance_json(otg);
    same_real(otg_json,"fitness",value,1);
    same_real(otg_json,"delta",value,2);
    minera::OcdfgConformanceResult ocdfg;
    ocdfg.fitness=value;
    same_real(minera::ocdfg_conformance_json(ocdfg),"fitness",value,1);
  }
}

void conformance_locale_and_exact_integer_json() {
  minera::OcdfgConformanceResult ocdfg;
  ocdfg.fitness=std::nextafter(0.5,1.0);
  ocdfg.activity_measure_differences={{"minimum",std::numeric_limits<std::int64_t>::min()},
                                    {"maximum",std::numeric_limits<std::int64_t>::max()},
                                    {"negative",-1234567890123456789}};
  ocdfg.flow_measure_differences={{{"A","B"},std::numeric_limits<std::int64_t>::max()}};
  minera::EtotConformanceResult etot;
  etot.fitness=-0.0;
  etot.relative_frequency_delta[{"A","T"}]=1234567.125;
  minera::OtgConformanceResult otg;
  otg.fitness=1.0/3.0;
  otg.non_conforming_edges[{"T","relation","U"}]=-0.0;
  std::string plain_ocdfg,plain_etot,plain_otg;
  {
    LocaleGuard classic(std::locale::classic());
    plain_ocdfg=minera::ocdfg_conformance_json(ocdfg);
    plain_etot=minera::etot_conformance_json(etot);
    plain_otg=minera::otg_conformance_json(otg);
    conformance_scalar_json_roundtrip();
  }
  {
    LocaleGuard hostile(std::locale(std::locale::classic(),new HostilePunctuation));
    const auto json=minera::ocdfg_conformance_json(ocdfg);
    assert(json==plain_ocdfg);
    assert(minera::etot_conformance_json(etot)==plain_etot);
    assert(minera::otg_conformance_json(otg)==plain_otg);
    same_integer(json,"minimum",std::numeric_limits<std::int64_t>::min());
    same_integer(json,"maximum",std::numeric_limits<std::int64_t>::max());
    same_integer(json,"negative",-1234567890123456789);
    same_integer(json,"difference",std::numeric_limits<std::int64_t>::max());
    conformance_scalar_json_roundtrip();
  }
}

void same_samples(const std::vector<double>& actual,
                    const std::vector<double>& expected) {
  assert(actual.size()==expected.size());
  for (std::size_t i=0;i<actual.size();++i)
    assert(std::bit_cast<std::uint64_t>(actual[i])==
           std::bit_cast<std::uint64_t>(expected[i]));
}

minera::OcdfgGraph embedded_annotation_graph(const std::string& ocpn_json) {
  const std::string marker="\"annotations\":";
  const auto found=ocpn_json.rfind(marker);
  assert(found!=std::string::npos);
  const auto begin=found+marker.size();
  assert(ocpn_json[begin]=='{' && ocpn_json.back()=='}');
  const auto body=ocpn_json.substr(begin,ocpn_json.size()-begin-1);
  assert(body.front()=='{' && body.back()=='}');
  // Annotation objects omit only the standalone graph's two label catalogues.
  // Supply them, then independently parse the actual embedded numeric arrays.
  return minera::parse_ocdfg_json(
      "{\"activities\":[\"A\",\"B\"],\"object_types\":[\"T\"],"+body.substr(1));
}

void ocpn_embedded_ocdfg_json_roundtrip() {
  minera::OcpnGraph graph;
  graph.activities={"A","B"};
  graph.object_types={"T"};
  graph.annotations.activities=graph.activities;
  graph.annotations.object_types=graph.object_types;
  auto& edge=graph.annotations.edges["T"][{"A","B"}];
  edge.performance={0.0,-0.0,230400.123456,std::nextafter(1.0,2.0),
      std::numeric_limits<double>::denorm_min(),std::numeric_limits<double>::min(),
      std::numeric_limits<double>::max(),-1e200};
  edge.performance_event_couples=std::vector<double>(edge.performance.rbegin(),edge.performance.rend());
  edge.performance_total_objects={-0.0,0.0,1e-200,std::nextafter(0.5,1.0),
      1234567.125,-1234567.125};
  auto& diagnostics=graph.tbr_results["T"];
  diagnostics.enabled=true;
  diagnostics.trace_count=1234567;
  diagnostics.fitting_traces=1234566;
  diagnostics.transition_activations={123456789,987654321};
  std::string plain;
  const auto check=[&] {
    const auto json=minera::serialize_ocpn_json(graph);
    const auto restored=embedded_annotation_graph(json);
    const auto& restored_edge=restored.edges.at("T").at({"A","B"});
    same_samples(restored_edge.performance,edge.performance);
    same_samples(restored_edge.performance_event_couples,edge.performance_event_couples);
    same_samples(restored_edge.performance_total_objects,edge.performance_total_objects);
    same_integer(json,"trace_count",1234567);
    same_integer(json,"fitting_traces",1234566);
    const auto standalone=minera::parse_ocdfg_json(minera::serialize_ocdfg_json(graph.annotations));
    const auto& standalone_edge=standalone.edges.at("T").at({"A","B"});
    same_samples(restored_edge.performance,standalone_edge.performance);
    same_samples(restored_edge.performance_event_couples,standalone_edge.performance_event_couples);
    same_samples(restored_edge.performance_total_objects,standalone_edge.performance_total_objects);
    return json;
  };
  {
    LocaleGuard classic(std::locale::classic());
    plain=check();
  }
  {
    LocaleGuard hostile(std::locale(std::locale::classic(),new HostilePunctuation));
    assert(check()==plain);
    graph.annotated=false;
    assert(minera::serialize_ocpn_json(graph).ends_with("\"annotations\":null}"));
  }
}

}  // namespace

int main() {
  conformance_locale_and_exact_integer_json();
  ocpn_embedded_ocdfg_json_roundtrip();
  std::cout << "ETOT/OTG/OCDFG conformance JSON: 14 scalar states across seven fields "
               "and two locales, signed zero, nonfinite null and exact int64 transport; "
               "OCPN embedded/standalone OCDFG bitwise performance arrays passed\n";
}
