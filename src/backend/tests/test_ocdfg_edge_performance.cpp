#include "minera/ocdfg_edge_performance.hpp"
#include <bit>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <locale>
#include <cmath>
using namespace minera;
namespace {
ParsedExactTimestamp date(const std::string& text){const auto value=parse_timestamp_exact(text);assert(value);return *value;}
void same_values(const std::vector<double>& actual,const std::vector<double>& expected){assert(actual.size()==expected.size());for(std::size_t i=0;i<actual.size();++i)assert(std::bit_cast<std::uint64_t>(actual[i])==std::bit_cast<std::uint64_t>(expected[i]));}
template<class F>void error_is(F action,const std::string& type,const std::string& message){bool thrown=false;try{action();}catch(const OcdfgPerformanceError& error){thrown=true;assert(error.type==type&&error.what()==message);}assert(thrown);}
#include "ocdfg_edge_performance_public_vectors.inc"
#include "ocdfg_working_time_discovery_vectors.inc"
void practical_timing_contract(){
 std::vector<OcdfgPerformanceEvent> events={{"Friday",date("2027-02-05T16:30:00Z")},{"Monday",date("2027-02-08T08:30:00Z")}};
 const std::vector<std::pair<std::string,std::string>> pairs={{"Friday","Monday"}};
 OcdfgEdgePerformanceOptions options;options.business_hours=true;
 same_values(calculate_ocdfg_edge_performance(events,pairs,options),{7200.0});
 options.business_hours=false;same_values(calculate_ocdfg_edge_performance(events,pairs,options),{230400.0});
 options.business_hours=true;options.business_hour_slots=std::vector<BusinessHourSlot>{};same_values(calculate_ocdfg_edge_performance(events,pairs,options),{0.0});
 options.business_hour_slots=nullptr;same_values(calculate_ocdfg_edge_performance(events,{},options),{});
 options.business_hours=false;same_values(calculate_ocdfg_edge_performance(events,pairs,options),{230400.0});
 Ocel log;
 log.events={{"Friday","depart","2027-02-05T16:30:00Z",{}},{"Monday","arrive","2027-02-08T08:30:00Z",{}}};
 log.objects={{"order","Order",{}}};
 log.relations={{"Friday","order",""},{"Monday","order",""}};
 const auto before=serialize_ocel_json(log);
 OcdfgDiscoveryOptions discovery;
 const auto legacy=serialize_ocdfg_json(discover_ocdfg_classic(log));
 assert(serialize_ocdfg_json(discover_ocdfg_classic_with_options(log,discovery))==legacy);
 options.business_hours=true;options.business_hour_slots=std::monostate{};
 discovery.edge_performance_options=options;
 const auto working=discover_ocdfg_classic_with_options(log,discovery);
 same_values(working.edges.at("Order").at({"depart","arrive"}).performance,{7200.0});
 discovery.compute_edges_performance=false;
 discovery.edge_performance_options->business_hour_slots=nullptr;
 assert(discover_ocdfg_classic_with_options(log,discovery).edges.at("Order").at({"depart","arrive"}).performance.empty());
 assert(serialize_ocel_json(log)==before);
}
} // namespace
namespace {
struct CommaPunctuation : std::numpunct<char> {
  char do_decimal_point() const override { return ','; }
  char do_thousands_sep() const override { return '.'; }
  std::string do_grouping() const override { return "\3"; }
};
struct LocaleGuard {
  std::locale saved;
  LocaleGuard() : saved(std::locale::global(std::locale(std::locale::classic(), new CommaPunctuation))) {}
  ~LocaleGuard() { std::locale::global(saved); }
};
void timing_json_roundtrip() {
  const std::vector<double> values = {230400.123456, 0.000001, -0.0,
      std::nextafter(1.0, 2.0), std::numeric_limits<double>::denorm_min(),
      std::numeric_limits<double>::max()};
  OcdfgGraph graph;
  auto& edge = graph.edges["Order"][{"depart", "arrive"}];
  edge.performance = values;
  edge.performance_event_couples = values;
  edge.performance_total_objects = values;
  LocaleGuard hostile_locale;
  const auto json = serialize_ocdfg_json(graph);
  const auto parsed = parse_ocdfg_json(json);
  const auto& restored = parsed.edges.at("Order").at({"depart", "arrive"});
  same_values(restored.performance, values);
  same_values(restored.performance_event_couples, values);
  same_values(restored.performance_total_objects, values);
  assert(serialize_ocdfg_json(parsed) == json);
}
} // namespace
int main(){public_timing_vectors();public_discovery_vectors();practical_timing_contract();timing_json_roundtrip();std::cout<<"OCDFG edge performance: 60 retained public timing cells, eight discovery outcomes, practical working-time controls and bitwise JSON roundtrip passed\n";}
