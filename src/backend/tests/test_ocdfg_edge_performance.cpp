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

void missing_datetime_adapter_contract() {
  // These adapter projections use the retained direct NaT outcomes: zero
  // elapsed time and the captured ValueError under working-time arithmetic.
  for (const auto& key : {std::string("ocel:timestamp"), std::string("observed time"), std::string()}) {
    Ocel log;
    log.events = {{"e1", "A", "NaT", {}},
                  {"e2", "B", "2027-02-05T16:30:00Z", {}}};
    if (key != "ocel:timestamp") {
      log.events[0].attributes.push_back({key, std::string("NaT")});
      log.events[1].attributes.push_back({key, log.events[1].timestamp});
      log.event_type_attributes["A"][key] = "datetime";
      log.event_type_attributes["B"][key] = "datetime";
    }
    log.objects = {{"o1", "Order", {}}, {"o2", "Order", {}}};
    log.relations = {{"e1", "o1", ""}, {"e2", "o1", ""},
                     {"e1", "o2", ""}, {"e2", "o2", ""}};
    const auto before = serialize_ocel_json(log);
    const std::vector<std::pair<std::string, std::string>> pairs = {{"e1", "e2"}};
    const std::vector<OcdfgPerformanceEvent> typed = {
        {"e1", std::monostate{}}, {"e2", date(log.events[1].timestamp)}};
    OcdfgEdgePerformanceOptions options;
    options.timestamp_attribute = key;
    same_values(calculate_ocdfg_edge_performance(typed, pairs, options), {0.0});
    same_values(calculate_ocdfg_edge_performance(log, pairs, options), {0.0});
    auto reopened = key.empty() ? parse_ocel_json_literal_names(before) : parse_ocel_json(before);
    same_values(calculate_ocdfg_edge_performance(reopened, pairs, options), {0.0});
    OcdfgDiscoveryOptions discovery;
    discovery.edge_performance_options = options;
    const auto graph = discover_ocdfg_classic_with_options(log, discovery);
    const auto& edge = graph.edges.at("Order").at({"A", "B"});
    same_values(edge.performance_event_couples, {0.0});
    same_values(edge.performance_total_objects, {0.0, 0.0});
    same_values(edge.performance, {0.0, 0.0});
    options.business_hours = true;
    const auto check_missing = [&](auto action) {
      error_is(action, "ValueError", "cannot convert float NaN to integer");
    };
    check_missing([&] { (void)calculate_ocdfg_edge_performance(typed, pairs, options); });
    check_missing([&] { (void)calculate_ocdfg_edge_performance(log, pairs, options); });
    check_missing([&] { (void)calculate_ocdfg_edge_performance(reopened, pairs, options); });
    discovery.edge_performance_options = options;
    check_missing([&] { (void)discover_ocdfg_classic_with_options(log, discovery); });
    assert(serialize_ocel_json(log) == before);
  }
}

void canonical_and_scalar_adapter_contract() {
  Ocel log;
  log.events = {{"e1", "A", "2027-02-05T16:30:00Z", {}},
                {"e2", "B", "2027-02-05T16:31:00Z", {}}};
  const auto before = serialize_ocel_json(log);
  // Public column selectors must read native canonical roles, including when
  // they are used as grouping IDs rather than their conventional role.
  for (const auto& id_key : {std::string("ocel:eid"), std::string("ocel:activity"), std::string("ocel:timestamp")}) {
    OcdfgEdgePerformanceOptions options;
    options.event_id_attribute = id_key;
    const auto left = id_key == "ocel:eid" ? "e1" : id_key == "ocel:activity" ? "A" : log.events[0].timestamp;
    const auto right = id_key == "ocel:eid" ? "e2" : id_key == "ocel:activity" ? "B" : log.events[1].timestamp;
    same_values(calculate_ocdfg_edge_performance(log, {{left, right}}, options), {60.0});
  }
  assert(serialize_ocel_json(log) == before);
  for (const auto& key : {std::string("ocel:eid"), std::string("ocel:activity"),
                          std::string("scalar"), std::string("undeclared scalar")}) {
    for (const auto& lexical : {std::string("NaT"), std::string("2027-02-05T16:30:00Z"), std::string()}) {
      auto literal = log;
      if (key == "ocel:eid") {
        literal.events[0].id = lexical;
        literal.events[1].id = lexical + " second";
      } else if (key == "ocel:activity") {
        literal.events[0].activity = lexical;
        literal.events[1].activity = lexical;
      } else {
        for (auto& event : literal.events) {
          event.attributes.push_back({key, lexical});
          if (key == "scalar") literal.event_type_attributes[event.activity][key] = "string";
        }
      }
      const auto unchanged = serialize_ocel_json(literal);
      const std::vector<std::pair<std::string, std::string>> pairs = {
          {literal.events[0].id, literal.events[1].id}};
      OcdfgEdgePerformanceOptions options;
      options.timestamp_attribute = key;
      error_is([&] { (void)calculate_ocdfg_edge_performance(literal, pairs, options); },
               "TypeError", "unsupported operand type(s) for -: 'str' and 'str'");
      options.business_hours = true;
      error_is([&] { (void)calculate_ocdfg_edge_performance(literal, pairs, options); },
               "TypeError", "str.replace() takes no keyword arguments");
      assert(serialize_ocel_json(literal) == unchanged);
    }
  }
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
int main(){public_timing_vectors();public_discovery_vectors();practical_timing_contract();missing_datetime_adapter_contract();canonical_and_scalar_adapter_contract();timing_json_roundtrip();std::cout<<"OCDFG edge performance: 60 retained public timing cells, eight discovery outcomes, native missing/scalar/canonical-role adapters, practical working-time controls and bitwise JSON roundtrip passed\n";}
