// Regressions from the 61ebeee0 parity recheck. Expected values were produced
// by the pinned reference release on the same inputs.
#include "minera/attribute_distribution.hpp"
#include "minera/batches.hpp"
#include "minera/dataframe_format.hpp"
#include "minera/dfg_dataframe.hpp"
#include "minera/dfg_dataframe_json.hpp"
#include "minera/footprints.hpp"
#include "minera/ocel.hpp"
#include "minera/ocel_features.hpp"
#include "minera/ocel_filter.hpp"
#include "minera/ocel_sqlite.hpp"
#include "minera/ocel_xml.hpp"
#include "minera/pandas_case_filter.hpp"
#include "minera/pandas_sequence_filters.hpp"
#include "minera/streaming_dfg.hpp"
#include "minera/streaming_footprints.hpp"
#include "minera/timestamp.hpp"
#include "minera/timing_workbench_json.hpp"
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <set>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
using namespace minera;
namespace {
int failures=0;
void check(bool ok,const std::string& name){if(!ok){std::cerr<<"FAIL "<<name<<"\n";++failures;}}

EventAttribute text(std::string key,std::string value){return EventAttribute{std::move(key),std::move(value)};}
EventAttribute when(std::string key,std::string value){
 EventAttribute a{std::move(key),value,AttributeKind::kDate};a.typed_value=NativeDate{value,NativeDateResolution::kNanosecond};return a;}
EventAttribute real(double value){EventAttribute a{"",std::to_string(value),AttributeKind::kFloat};a.typed_value=value;return a;}

// J7: case durations sum the Timedelta components, 86400*days + seconds
// exactly, then 1e-6*microseconds and 1e-9*nanoseconds as floats.
struct DurationVector{const char* unit;const char* start;const char* end;double bound;std::size_t with_minimum,with_maximum;};
const DurationVector kDurationVectors[]={
  {"ns","2020-01-01T00:00:00","2020-01-01T00:00:00.000000003",0x1.9c511dc3a41dfp-29,2,0}, // below
  {"ns","2020-01-01T00:00:00","2020-01-01T00:00:00.000000003",0x1.9c511dc3a41e0p-29,2,2}, // exact
  {"ns","2020-01-01T00:00:00","2020-01-01T00:00:00.000000003",0x1.9c511dc3a41e1p-29,0,2}, // above
  {"ns","2020-01-01T00:00:00","2020-01-01T00:00:00.000000007",0x1.e1094d643f784p-28,2,0}, // below
  {"ns","2020-01-01T00:00:00","2020-01-01T00:00:00.000000007",0x1.e1094d643f785p-28,2,2}, // exact
  {"ns","2020-01-01T00:00:00","2020-01-01T00:00:00.000000007",0x1.e1094d643f786p-28,0,2}, // above
  {"ns","2020-01-01T00:00:00.999999999","2020-01-01T00:00:01.000000002",0x1.9c511dc3a41dfp-29,2,0}, // below
  {"ns","2020-01-01T00:00:00.999999999","2020-01-01T00:00:01.000000002",0x1.9c511dc3a41e0p-29,2,2}, // exact
  {"ns","2020-01-01T00:00:00.999999999","2020-01-01T00:00:01.000000002",0x1.9c511dc3a41e1p-29,0,2}, // above
  {"ns","2020-01-01T00:00:00","2020-01-01T00:00:00.000999",0x1.05e1c15097c7fp-10,2,0}, // below
  {"ns","2020-01-01T00:00:00","2020-01-01T00:00:00.000999",0x1.05e1c15097c80p-10,2,2}, // exact
  {"ns","2020-01-01T00:00:00","2020-01-01T00:00:00.000999",0x1.05e1c15097c81p-10,0,2}, // above
  {"ns","2020-01-01T00:00:00","2020-01-01T00:00:00.123456789",0x1.f9add3739635ep-4,2,0}, // below
  {"ns","2020-01-01T00:00:00","2020-01-01T00:00:00.123456789",0x1.f9add3739635fp-4,2,2}, // exact
  {"ns","2020-01-01T00:00:00","2020-01-01T00:00:00.123456789",0x1.f9add37396360p-4,0,2}, // above
  {"ns","2020-01-01T00:00:00","2020-01-01T00:00:05.000000003",0x1.4000000338a23p+2,2,0}, // below
  {"ns","2020-01-01T00:00:00","2020-01-01T00:00:05.000000003",0x1.4000000338a24p+2,2,2}, // exact
  {"ns","2020-01-01T00:00:00","2020-01-01T00:00:05.000000003",0x1.4000000338a25p+2,0,2}, // above
  {"us","2020-01-01T00:00:00","2020-01-01T00:00:00.000999",0x1.05e1c15097c7fp-10,2,0}, // below
  {"us","2020-01-01T00:00:00","2020-01-01T00:00:00.000999",0x1.05e1c15097c80p-10,2,2}, // exact
  {"us","2020-01-01T00:00:00","2020-01-01T00:00:00.000999",0x1.05e1c15097c81p-10,0,2}, // above
  {"us","2020-01-01T00:00:00.999999","2020-01-01T00:00:01.000002",0x1.92a737110e453p-19,2,0}, // below
  {"us","2020-01-01T00:00:00.999999","2020-01-01T00:00:01.000002",0x1.92a737110e454p-19,2,2}, // exact
  {"us","2020-01-01T00:00:00.999999","2020-01-01T00:00:01.000002",0x1.92a737110e455p-19,0,2}, // above
  {"us","2020-01-01T00:00:00","2020-01-01T00:00:00.001500",0x1.89374bc6a7ef9p-10,2,0}, // below
  {"us","2020-01-01T00:00:00","2020-01-01T00:00:00.001500",0x1.89374bc6a7efap-10,2,2}, // exact
  {"us","2020-01-01T00:00:00","2020-01-01T00:00:00.001500",0x1.89374bc6a7efbp-10,0,2}, // above
  {"ms","2020-01-01T00:00:00","2020-01-01T00:00:00.999000",0x1.ff7ced916872ap-1,2,0}, // below
  {"ms","2020-01-01T00:00:00","2020-01-01T00:00:00.999000",0x1.ff7ced916872bp-1,2,2}, // exact
  {"ms","2020-01-01T00:00:00","2020-01-01T00:00:00.999000",0x1.ff7ced916872cp-1,0,2}, // above
  {"ms","2020-01-01T00:00:00.999000","2020-01-01T00:00:02",0x1.004189374bc69p+0,2,0}, // below
  {"ms","2020-01-01T00:00:00.999000","2020-01-01T00:00:02",0x1.004189374bc6ap+0,2,2}, // exact
  {"ms","2020-01-01T00:00:00.999000","2020-01-01T00:00:02",0x1.004189374bc6bp+0,0,2}, // above
  {"s","2020-01-01T00:00:00","2020-01-01T00:00:05",0x1.3ffffffffffffp+2,2,0}, // below
  {"s","2020-01-01T00:00:00","2020-01-01T00:00:05",0x1.4000000000000p+2,2,2}, // exact
  {"s","2020-01-01T00:00:00","2020-01-01T00:00:05",0x1.4000000000001p+2,0,2}, // above
  {"s","2020-01-01T23:59:59","2020-01-03T00:00:00",0x1.5180fffffffffp+16,2,0}, // below
  {"s","2020-01-01T23:59:59","2020-01-03T00:00:00",0x1.5181000000000p+16,2,2}, // exact
  {"s","2020-01-01T23:59:59","2020-01-03T00:00:00",0x1.5181000000001p+16,0,2}, // above
};
void duration_thresholds(){
 for(const auto& v:kDurationVectors){
  EventTable table;table.columns={"case:concept:name","concept:name","time:timestamp"};
  table.rows.push_back({text("case:concept:name","c"),text("concept:name","A"),when("time:timestamp",v.start)});
  table.rows.push_back({text("case:concept:name","c"),text("concept:name","B"),when("time:timestamp",v.end)});
  table.dtypes={"object","object",std::string("datetime64[")+v.unit+"]"};
  const std::string name=std::string("J7 ")+v.unit+" "+v.start+" "+v.end;
  check(filter_pandas_case_performance(table,real(v.bound),real(1e12)).size()==v.with_minimum,name+" minimum");
  check(filter_pandas_case_performance(table,real(0.0),real(v.bound)).size()==v.with_maximum,name+" maximum");
 }
}

// J8/J9/J11: pandas sequence-filter case identity, timestamp grouping by
// instant, and the dtype inferred for grouped case keys.
EventAttribute signed_id(std::int64_t v){EventAttribute a{"case:concept:name",std::to_string(v),AttributeKind::kInt};a.typed_value=v;return a;}
EventAttribute unsigned_id(std::uint64_t v){EventAttribute a{"case:concept:name",std::to_string(v),AttributeKind::kInt};a.typed_value=v;return a;}
EventAttribute real_id(double v,std::string text){EventAttribute a{"case:concept:name",std::move(text),AttributeKind::kFloat};a.typed_value=v;return a;}
EventAttribute lexical_real_id(std::string text){return EventAttribute{"case:concept:name",std::move(text),AttributeKind::kFloat};}
EventAttribute bool_id(bool v){EventAttribute a{"case:concept:name",v?"True":"False",AttributeKind::kBoolean};a.typed_value=v;return a;}
EventAttribute text_id(std::string v){return text("case:concept:name",std::move(v));}
EventAttribute no_time(){EventAttribute a{"time:timestamp","NaT",AttributeKind::kDate};a.is_null=true;return a;}

EventTable sequence_table(const std::vector<EventAttribute>& ids,std::vector<std::optional<std::string>> times={},std::string time_dtype="datetime64[ns]"){
 EventTable table;table.columns={"case:concept:name","concept:name","time:timestamp"};table.dtypes={"object","object",time_dtype};
 for(std::size_t i=0;i<ids.size();++i){
  const auto t=i<times.size()?times[i]:std::optional<std::string>("2020-01-01T00:00:0"+std::to_string(i));
  table.rows.push_back({ids[i],text("concept:name",std::string(1,static_cast<char>('A'+i))),t?when("time:timestamp",*t):no_time()});
 }
 return table;
}
std::string activities(const EventTable& table){
 const auto column=table.column_index("concept:name");std::string out;
 for(const auto& row:table.rows)out+=row[column]?row[column]->value:"?";
 return out;
}

void sequence_case_identity(){
 const auto umax=std::numeric_limits<std::uint64_t>::max();
 const std::uint64_t two63=std::uint64_t{1}<<63;
 struct Case{const char* name;std::vector<EventAttribute> ids;const char* prefix;const char* suffix;const char* dtype;};
 const std::vector<Case> cases{
  {"uint64 max and -1 stay distinct",{unsigned_id(umax),signed_id(-1),unsigned_id(umax),signed_id(-1)},"A","D","object"},
  {"0.125 and .125 are one case",{real_id(0.125,"0.125"),lexical_real_id(".125"),real_id(0.125,"0.125"),lexical_real_id(".125")},"AB","CD","float64"},
  {"uint64 and float 2**63 are one case",{unsigned_id(two63),real_id(9223372036854775808.0,"9.223372036854776e+18"),unsigned_id(two63),real_id(9223372036854775808.0,"9.223372036854776e+18")},"AB","CD","uint64"},
 };
 for(const auto& c:cases){
  const auto table=sequence_table(c.ids);
  check(activities(pandas_prefix_filter_apply(table,PandasFilterValue::of("C")))==c.prefix,std::string("J8 prefix ")+c.name);
  check(activities(pandas_suffix_filter_apply(table,PandasFilterValue::of("B")))==c.suffix,std::string("J8 suffix ")+c.name);
  const auto grouped=pandas_timestamp_case_grouping_apply(table,std::nullopt);
  check(grouped.size()==4&&grouped.dtypes[0]==c.dtype,std::string("J8 grouping ")+c.name);
 }
}

void timestamp_grouping_instants(){
 struct Case{const char* name;std::vector<std::optional<std::string>> times;const char* kept;};
 const std::vector<Case> cases{
  {"Z and +00:00",{"2020-01-01T00:00:00Z","2020-01-01T00:00:00+00:00","2020-01-01T00:00:00.000000001Z"},"AC"},
  {"fraction, separator and offset spellings",{"2020-01-01T00:00:00.5Z","2020-01-01 00:00:00.500+00:00","2020-01-01T02:00:00.5+02:00"},"A"},
 };
 for(const auto& c:cases){
  const auto table=sequence_table({text_id("c"),text_id("c"),text_id("c")},c.times,"datetime64[ns, UTC]");
  check(activities(pandas_timestamp_case_grouping_apply(table,std::nullopt))==c.kept,std::string("J9 ")+c.name);
 }
}

void grouped_key_dtypes(){
 const auto umax=std::numeric_limits<std::uint64_t>::max();
 const std::uint64_t two63=std::uint64_t{1}<<63;
 struct Case{const char* name;std::vector<EventAttribute> ids;std::vector<std::optional<std::string>> times;const char* dtype;std::vector<std::string> values;};
 const std::optional<std::string> t0="2020-01-01T00:00:00",t1="2020-01-01T00:00:01",nat=std::nullopt;
 const std::vector<Case> cases{
  {"bools",{bool_id(true),bool_id(false)},{},"bool",{"True","False"}},
  {"floats",{real_id(1.5,"1.5"),real_id(2.5,"2.5")},{},"float64",{"1.5","2.5"}},
  {"uint64",{unsigned_id(umax),signed_id(1)},{},"uint64",{"18446744073709551615","1"}},
  {"ints",{signed_id(1),signed_id(2)},{},"int64",{"1","2"}},
  {"1 then True",{signed_id(1),bool_id(true)},{},"int64",{"1","1"}},
  {"True then 1",{bool_id(true),signed_id(1)},{},"bool",{"True","True"}},
  {"int and float",{signed_id(1),real_id(1.5,"1.5")},{},"float64",{"1.0","1.5"}},
  {"negative and above int64",{signed_id(-1),unsigned_id(umax)},{},"object",{"-1","18446744073709551615"}},
  {"dropped str group",{signed_id(1),text_id("x")},{t0,nat},"object",{"1"}},
  {"dropped int group",{signed_id(1),signed_id(2)},{nat,t1},"int64",{"2"}},
  {"dropped str with bool",{bool_id(true),text_id("x")},{t0,nat},"object",{"True"}},
  {"float and bool",{real_id(1.5,"1.5"),bool_id(true)},{},"object",{"1.5","True"}},
  {"bool and int",{bool_id(true),signed_id(2)},{},"object",{"True","2"}},
  {"1.0 then True",{real_id(1.0,"1.0"),bool_id(true)},{},"float64",{"1.0","1.0"}},
  {"2**63 and float",{unsigned_id(two63),real_id(1.5,"1.5")},{},"float64",{"9.223372036854776e+18","1.5"}},
  {"strings",{text_id("a"),text_id("b")},{},"str",{"a","b"}},
 };
 for(const auto& c:cases){
  const auto result=pandas_timestamp_case_grouping_apply(sequence_table(c.ids,c.times),std::nullopt);
  bool ok=result.dtypes[0]==c.dtype&&result.size()==c.values.size();
  for(std::size_t i=0;ok&&i<c.values.size();++i){
   const auto& cell=*result.rows[i][0];
   std::string shown=cell.value;
   if(const auto* b=std::get_if<bool>(&cell.typed_value))shown=*b?"True":"False";
   ok=shown==c.values[i];
   if(!ok)std::cerr<<"  value "<<i<<": "<<shown<<"\n";
  }
  if(!ok&&!result.dtypes.empty())std::cerr<<"  dtype "<<result.dtypes[0]<<" rows "<<result.size()<<"\n";
  check(ok,std::string("J11 ")+c.name);
 }
}

// J29: pandas batch event tuples and group keys keep scalar equality (1, 1.0
// and True are equal) and comparison failures (a number and a text raise).
EventAttribute cell_of(std::string key,const EventAttribute& value){auto a=value;a.key=std::move(key);return a;}
EventTable batch_table(const std::vector<EventAttribute>& cases,const std::vector<EventAttribute>& activities,const std::vector<EventAttribute>& resources,const std::vector<int>& seconds){
 EventTable table;table.columns={"case:concept:name","concept:name","org:resource","time:timestamp"};table.dtypes={"object","object","object","datetime64[ns]"};
 for(std::size_t i=0;i<cases.size();++i)
  table.rows.push_back({cell_of("case:concept:name",cases[i]),cell_of("concept:name",activities[i]),cell_of("org:resource",resources[i]),
                        when("time:timestamp","2020-01-01T00:0"+std::to_string(seconds[i]/60)+":00")});
 return table;
}
std::vector<EventAttribute> repeat(const EventAttribute& v,std::size_t n){return std::vector<EventAttribute>(n,v);}
void batch_scalar_identity(){
 const auto A=text_id("A"),R=text_id("R");
 const auto run=[](const EventTable& t){return discover_batches(t,BatchDiscoveryOptions{.variant=BatchDiscoveryVariant::kPandas,.merge_distance_seconds=900,.min_batch_size=2});};
 const auto raises=[&](const EventTable& t,const std::string& name){
  // The reference's operand order ('int' and 'str' or 'str' and 'int') varies
  // across hash seeds (checked with PYTHONHASHSEED 0-7); either is accepted.
  bool threw=false;try{(void)run(t);}catch(const std::invalid_argument& e){const std::string what=e.what();threw=what=="batches: '<' not supported between instances of 'int' and 'str'"||what=="batches: '<' not supported between instances of 'str' and 'int'";if(!threw)std::cerr<<"  "<<e.what()<<"\n";}
  check(threw,name);};
 // [1, 1.0] is one event: no batch of two.
 check(run(batch_table({signed_id(1),real_id(1.0,"1.0")},repeat(A,2),repeat(R,2),{0,0})).groups.empty(),"J29 1 and 1.0 deduplicate");
 // [True, 1, 2] is two events, True kept.
 {const auto r=run(batch_table({bool_id(true),signed_id(1),signed_id(2)},repeat(A,3),repeat(R,3),{0,0,0}));
  check(r.groups.size()==1&&r.groups[0].event_count==2&&r.groups[0].batches[0].events.size()==2,"J29 True, 1, 2 give two events");}
 // 1 then an equal 1.0 at the same instant: the merge keeps the first.
 {const auto r=run(batch_table({signed_id(1),signed_id(2),real_id(1.0,"1.0")},repeat(A,3),repeat(R,3),{0,60,0}));
  check(r.groups.size()==1&&r.groups[0].event_count==2&&r.groups[0].batches[0].events[0].case_id=="1.0","J29 1, 2, 1.0 give two events, 1.0 kept");}
 raises(batch_table({text_id("a"),signed_id(1)},repeat(A,2),repeat(R,2),{0,0}),"J29 str and int cases at one instant raise");
 raises(batch_table({signed_id(1),text_id("x")},repeat(A,2),repeat(R,2),{0,0}),"J29 int and str cases at one instant raise");
 check(run(batch_table({signed_id(1),text_id("x")},repeat(A,2),repeat(R,2),{0,60})).groups.size()==1,"J29 int and str cases at distinct instants");
 raises(batch_table({text_id("a"),text_id("b"),text_id("c"),text_id("d")},repeat(A,4),{R,R,signed_id(5),signed_id(5)},{0,0,0,0}),"J29 str and int resources raise");
 raises(batch_table({text_id("a"),text_id("b"),text_id("c"),text_id("d")},{signed_id(1),signed_id(1),text_id("B"),text_id("B")},{R,R,text_id("S"),text_id("S")},{0,0,0,0}),"J29 int and str activities raise");
 {const auto r=run(batch_table({text_id("a"),text_id("b"),text_id("c"),text_id("d")},repeat(A,4),{bool_id(true),bool_id(true),signed_id(1),signed_id(1)},{0,0,0,0}));
  check(r.groups.size()==1&&r.groups[0].event_count==4,"J29 True and 1 resources are one group");}
 {const auto r=run(batch_table({text_id("a"),text_id("b"),text_id("c"),text_id("d")},repeat(A,4),{signed_id(5),signed_id(5),real_id(1.0,"1.0"),real_id(1.0,"1.0")},{0,0,0,0}));
  // A resource level holding ints and a float is inferred as float64 by the
  // reference's groupby, so the int label 5 is reported as 5.0.
  check(r.groups.size()==2&&r.groups[0].resource=="5.0"&&r.groups[1].resource=="1.0","J29 numeric resources sort by value, descending");}
}

// J30: the Polars adapter's epoch-nanoseconds-times-1e-9 seconds decide
// batch boundaries (1970 seconds 13/15 are 13.0 and 15.000000000000002).
void polars_batch_boundaries(){
 const auto run=[](int first,int second,double distance){
  EventTable table;table.columns={"case:concept:name","concept:name","org:resource","time:timestamp"};
  table.dtypes={"String","String","String","Datetime(time_unit='us', time_zone=None)"};
  for(const int s:{first,second})
   table.rows.push_back({text("case:concept:name","c"+std::to_string(s)),text("concept:name","A"),text("org:resource","R"),
                         when("time:timestamp","1970-01-01T00:00:"+std::string(s<10?"0":"")+std::to_string(s))});
  return BatchPolarsDiscovery::apply(LazyEventTable::from_table(table),BatchDiscoveryOptions{.merge_distance_seconds=distance,.min_batch_size=2}).groups.size();
 };
 check(run(13,15,2.0)==0,"J30 13/15 distance 2");
 check(run(14,15,1.0)==0,"J30 14/15 distance 1");
 check(run(12,14,2.0)==1,"J30 12/14 distance 2");
 check(run(13,15,2.0000001)==1,"J30 13/15 above distance");
}

// J31: classic OCEL JSON keeps document order before the stable timestamp
// sort. J32/J33: relations follow the event order even when the events are
// already sorted, and SQLite compares instants at nanoseconds.
std::string event_ids(const Ocel& log){std::string out;for(const auto& e:log.events)out+=e.id+",";return out;}
std::string relation_ids(const Ocel& log){std::string out;for(const auto& r:log.relations)out+=r.event_id+",";return out;}
void ocel_import_order(){
 const std::string classic=R"({"ocel:global-event":{},"ocel:global-object":{},"ocel:global-log":{},
  "ocel:objects":{"o1":{"ocel:type":"T","ocel:ovmap":{}}},
  "ocel:events":{"zeta":{"ocel:activity":"A","ocel:timestamp":"2020-01-01T00:00:00","ocel:omap":["o1"],"ocel:vmap":{}},
                 "alpha":{"ocel:activity":"A","ocel:timestamp":"2020-01-01T00:00:00","ocel:omap":["o1"],"ocel:vmap":{}},
                 "middle":{"ocel:activity":"A","ocel:timestamp":"2020-01-01T00:00:00","ocel:omap":["o1"],"ocel:vmap":{}}}})";
 const auto parsed=parse_ocel_json(classic);
 check(event_ids(parsed)=="zeta,alpha,middle,"&&relation_ids(parsed)=="zeta,alpha,middle,","J31 classic JSON keeps tied input order");
 Ocel sorted;
 for(const auto& [id,t]:std::vector<std::pair<std::string,std::string>>{{"zeta","2020-01-01T00:00:02"},{"alpha","2020-01-01T00:00:05"},{"middle","2020-01-01T00:00:09"}})
  sorted.events.push_back({id,"A",t,{}});
 for(const auto* id:{"middle","alpha","zeta"})sorted.relations.push_back({id,"o1",""});
 sort_ocel_events_by_timestamp(sorted,true);
 check(event_ids(sorted)=="zeta,alpha,middle,"&&relation_ids(sorted)=="zeta,alpha,middle,","J32 relations sorted when events already are");
 Ocel submicro;
 for(const auto& [id,t]:std::vector<std::pair<std::string,std::string>>{{"zeta","2020-01-01T00:00:00.000000900"},{"alpha","2020-01-01T00:00:00.000000100"},{"middle","2020-01-01T00:00:00.000000500"}}){
  submicro.events.push_back({id,"A",t,{}});submicro.relations.push_back({id,"o1",""});}
 auto microseconds=submicro;
 sort_ocel_events_by_timestamp(submicro,true);
 check(event_ids(submicro)=="alpha,middle,zeta,"&&relation_ids(submicro)=="alpha,middle,zeta,","J33 SQLite orders nanoseconds");
 sort_ocel_events_by_timestamp(microseconds);
 check(event_ids(microseconds)=="zeta,alpha,middle,","J33 JSON/XML precision keeps tied input order");
}

// OCEL JSON and XML order events by their wall-clock fields: the reference
// keeps the clock and attaches UTC, dropping the offset. It reads JSON (and
// classic XML) as ISO 8601 the way Python 3.11 does, raising on anything else.
// Expected orders and messages from PM4Py 2.7.22 read_ocel2_json/xml.
std::string ocel2_json(const std::vector<std::string>& times){
 std::string out=R"({"objectTypes":[{"name":"item","attributes":[]}],"eventTypes":[{"name":"A","attributes":[]}],"objects":[{"id":"o1","type":"item"}],"events":[)";
 for(std::size_t i=0;i<times.size();++i)out+=std::string(i?",":"")+R"({"id":"e)"+std::to_string(i)+R"(","type":"A","time":")"+times[i]+R"(","relationships":[{"objectId":"o1","qualifier":""}]})";
 return out+"]}";}
std::string ocel2_xml(const std::vector<std::string>& times){
 std::string out=R"(<?xml version="1.0" encoding="UTF-8"?><log><object-types><object-type name="item"><attributes/></object-type></object-types><event-types><event-type name="A"><attributes/></event-type></event-types><objects><object id="o1" type="item"><attributes/></object></objects><events>)";
 for(std::size_t i=0;i<times.size();++i)out+=R"(<event id="e)"+std::to_string(i)+R"(" type="A" time=")"+times[i]+R"("><objects><relationship object-id="o1" qualifier=""/></objects></event>)";
 return out+"</events></log>";}
std::string import_error(const std::function<void()>& run){try{run();}catch(const std::invalid_argument& e){return e.what();}return "";}
void ocel_wall_clock_order(){
 const auto offsets=parse_ocel_json(ocel2_json({"2024-01-01T05:00:00+08:00","2024-01-01T01:00:00Z","2024-01-01T03:00:00-05:00","2024-01-01T04",
  "20240101T020000","2024-01-01x03:30:00","2024-W01-1T00:30:00,5","2024-01-01T02 +01:00"}));
 check(event_ids(offsets)=="e6,e1,e4,e7,e2,e5,e3,e0,"&&relation_ids(offsets)=="e6,e1,e4,e7,e2,e5,e3,e0,","OCEL JSON wall-clock order and ISO shapes");
 const auto equivalent=parse_ocel_json(ocel2_json({"2020-01-01T01:00:00+01:00","2020-01-01T00:00:00Z","2020-01-01T00:30:00+00:30"}));
 check(event_ids(equivalent)=="e1,e2,e0,","OCEL JSON offset-equivalent instants order by wall clock");
 for(const auto& [bad,message]:std::vector<std::pair<std::string,std::string>>{
   {"garbage","Invalid isoformat string: 'garbage'"},{"","Invalid isoformat string: ''"},
   {"2024-01-01T05:00:00z","Invalid isoformat string: '2024-01-01T05:00:00z'"},{"2024-W53-1","Invalid isoformat string: '2024-W53-1'"},
   {"2024-01-01T24:00:00","hour must be in 0..23"},{"2024-01-01T05:00:00 ","Invalid isoformat string: '2024-01-01T05:00:00 '"},
   {"2024-01-01T05:00:00+08:00x","Invalid isoformat string: '2024-01-01T05:00:00+08:00x'"},{"2024-02-30","day is out of range for month"}})
  check(import_error([&]{(void)parse_ocel_json(ocel2_json({"2024-01-01T00:00:00",bad}));})==message,"OCEL JSON rejects timestamp '"+bad+"'");
 // Classic JSON is native transport: an unreadable timestamp keeps file order.
 const std::string classic_nat=R"({"ocel:objects":{"o1":{"ocel:type":"T","ocel:ovmap":{}}},"ocel:events":{
  "late":{"ocel:activity":"A","ocel:timestamp":"2020-01-02T00:00:00","ocel:omap":["o1"],"ocel:vmap":{}},
  "nat":{"ocel:activity":"A","ocel:timestamp":"NaT","ocel:omap":["o1"],"ocel:vmap":{}},
  "early":{"ocel:activity":"A","ocel:timestamp":"2020-01-01T00:00:00","ocel:omap":["o1"],"ocel:vmap":{}}}})";
 check(event_ids(parse_ocel_json(classic_nat))=="late,nat,early,","classic OCEL JSON transport keeps NaT logs in file order");
 const auto xml=parse_ocel_xml(ocel2_xml({"2024-01-01T05:00:00+08:00","2024-01-01T01:00:00Z","2024-01-01 03:00:00"}));
 check(event_ids(xml)=="e1,e2,e0,","OCEL XML wall-clock order");
 check(import_error([&]{(void)parse_ocel_xml(ocel2_xml({"2024-01-01T00:00:00","garbage"}));})=="String does not contain a date: garbage","OCEL XML rejects an unreadable timestamp");
 // OCEL 2.0 XML falls back to dateutil, which keeps the offset: an aware
 // fallback value orders by its instant next to ISO wall clocks.
 const auto xml_order=[](const std::vector<std::string>& times){return event_ids(parse_ocel_xml(ocel2_xml(times)));};
 check(xml_order({"2020-01-01T07:00:00","Jan 1 2020 10:00 +0500"})=="e1,e0,","OCEL XML dateutil +0500 orders by instant");
 check(xml_order({"2020-01-01T07:00:00","2020-01-01T10:00:00+05:00 "})=="e1,e0,","OCEL XML dateutil trailing space keeps the offset");
 check(xml_order({"2020/01/01 10:00:00","2020/01/01 07:00:00"})=="e1,e0,","OCEL XML dateutil slashes");
 check(xml_order({"Jan 2 2020 10:00","2 Jan 2020 09:00","January 1, 2020 11:00:00"})=="e2,e1,e0,","OCEL XML dateutil month names");
 check(xml_order({"01/02/2020 10:00","01/01/2020 11:00"})=="e1,e0,","OCEL XML dateutil month first");
 check(xml_order({"2020-01-01T07:00:00","13/01/2020 00:00 Z","2020.01.05 12:00 GMT"})=="e0,e2,e1,","OCEL XML dateutil day first above 12, Z and GMT");
 check(import_error([&]{(void)parse_ocel_xml(ocel2_xml({"2020/01/01 10:00:00","2020-01-01T07:00:00"}));})==
   "'values' is not ordered, please explicitly specify the categories order by passing in a categories argument.","OCEL XML naive dateutil next to aware raises");
 // Stored timestamps are what the reference stores: the wall clock stamped
 // UTC for ISO readings, the instant for an aware dateutil value. Expected
 // values from PM4Py 2.7.22 (read_ocel2_json, filter_ocel_events_timestamp,
 // the OCEL events/objects feature extractors).
 const auto mixed=parse_ocel_json(ocel2_json({"2020-01-01T05:00:00+08:00","2020-01-01T10:00:00Z"}));
 check(mixed.events[0].timestamp=="2020-01-01T05:00:00+00:00"&&mixed.events[1].timestamp=="2020-01-01T10:00:00Z","OCEL JSON stores the wall clock as UTC");
 check(event_ids(filter_ocel_events_timestamp(mixed,"2020-01-01 04:00:00","2020-01-01 06:00:00"))=="e0,","OCEL timestamp filter sees the stored wall clock");
 const auto feature=[](const OcelFeatureTable& table,const std::string& name){
  for(std::size_t i=0;i<table.feature_names.size();++i)if(table.feature_names[i]==name)return table.values.at(0).at(i);
  return -1.0;};
 const auto objects=extract_ocel_features(mixed,OcelFeatureVariant::kObjects);
 check(feature(objects,"@@object_lifecycle_duration")==18000.0&&feature(objects,"@@object_lifecycle_start_timestamp")==1577854800.0,"OCEL object lifecycle features use the stored wall clock");
 check(feature(extract_ocel_features(mixed,OcelFeatureVariant::kEvents),"@@event_timestamp")==1577854800.0,"OCEL event timestamp feature uses the stored wall clock");
 check(serialize_ocel_json(mixed,OcelJsonFormat::kStandard).find(R"("time":"2020-01-01T05:00:00+00:00")")!=std::string::npos,"OCEL JSON export writes the stored UTC value");
 const auto week=parse_ocel_json(ocel2_json({"2024-W01-1T00:30:00,5"}));
 // A naive shape parse_timestamp_exact does not read is stamped "Z" like
 // any naive event time (the reference JSON writers' spelling).
 check(week.events[0].timestamp=="2024-01-01T00:30:00.500000Z","OCEL JSON stores an ISO week date natively readable");
 const auto aware=parse_ocel_xml(ocel2_xml({"2020-01-01T07:00:00","Jan 1 2020 10:00 +0500"}));
 check(aware.events[0].timestamp=="2020-01-01T05:00:00+00:00"&&aware.events[1].timestamp=="2020-01-01T07:00:00Z","OCEL XML stores an aware dateutil value as its UTC instant and stamps a naive ISO value UTC");
 const auto database=std::filesystem::temp_directory_path()/"minera_recheck_mixed_offsets.sqlite";
 std::filesystem::remove(database);
 export_ocel_sqlite(mixed,database);
 const auto reread=import_ocel_sqlite(database);
 std::filesystem::remove(database);
 check(event_ids(reread)=="e0,e1,"&&parse_timestamp_microseconds(reread.events[0].timestamp)==parse_timestamp_microseconds("2020-01-01T05:00:00Z"),"OCEL SQLite round trip keeps the stored UTC value");
 // Object changes are read like events: PM4Py 2.7.22 read_ocel2_json/xml
 // store "09:00+05:00" as 09:00 UTC, read_ocel2_sqlite as its instant 04:00.
 const std::string changes_json=R"({"objectTypes":[{"name":"item","attributes":[{"name":"p","type":"string"}]}],"eventTypes":[{"name":"A","attributes":[]}],
  "objects":[{"id":"o1","type":"item","attributes":[{"name":"p","time":"1970-01-01T00:00:00Z","value":"x"},{"name":"p","time":"2020-01-01T09:00:00+05:00","value":"y"},{"name":"p","time":"2020-01-01T06:00:00Z","value":"z"}]}],
  "events":[{"id":"e1","type":"A","time":"2020-01-01T05:00:00Z","relationships":[{"objectId":"o1","qualifier":"q"}]}]})";
 const auto changed=parse_ocel_json(changes_json);
 check(changed.object_changes.size()==2&&changed.object_changes[0].timestamp=="2020-01-01T09:00:00+00:00"&&changed.object_changes[1].timestamp=="2020-01-01T06:00:00Z","OCEL JSON stores object-change wall clocks as UTC");
 const auto changes_db=std::filesystem::temp_directory_path()/"minera_recheck_change_offsets.sqlite";
 std::filesystem::remove(changes_db);
 export_ocel_sqlite(changed,changes_db);
 const auto changes_reread=import_ocel_sqlite(changes_db);
 std::filesystem::remove(changes_db);
 bool kept_nine=false;
 for(const auto& change:changes_reread.object_changes)
  kept_nine|=change.field=="p"&&change.value==OcelValue(std::string("y"))&&parse_timestamp_microseconds(change.timestamp)==parse_timestamp_microseconds("2020-01-01T09:00:00Z");
 check(changes_reread.object_changes.size()==2&&kept_nine,"OCEL JSON to SQLite keeps an object change at the reference's 09:00");
 auto offset_change=changed;
 offset_change.object_changes[0].timestamp="2020-01-01T09:00:00+05:00";
 std::filesystem::remove(changes_db);
 export_ocel_sqlite(offset_change,changes_db);
 const auto instant_reread=import_ocel_sqlite(changes_db);
 std::filesystem::remove(changes_db);
 bool instant_four=false;
 for(const auto& change:instant_reread.object_changes)
  instant_four|=change.value==OcelValue(std::string("y"))&&change.timestamp=="2020-01-01T04:00:00+00:00";
 check(instant_four,"OCEL SQLite stores an object change with an offset as its UTC instant");
 const auto changes_xml=parse_ocel_xml(R"(<?xml version="1.0"?><log><object-types><object-type name="item"><attributes><attribute name="p" type="string"/></attributes></object-type></object-types><event-types><event-type name="A"><attributes/></event-type></event-types><objects><object id="o1" type="item"><attributes><attribute name="p" time="1970-01-01T00:00:00Z">x</attribute><attribute name="p" time="2020-01-01T09:00:00+05:00">y</attribute><attribute name="p" time="Jan 1 2020 10:00 +0500">w</attribute></attributes></object></objects><events><event id="e1" type="A" time="2020-01-01T05:00:00Z"><objects><relationship object-id="o1" qualifier="q"/></objects></event></events></log>)");
 check(changes_xml.object_changes.size()==2&&changes_xml.object_changes[0].timestamp=="2020-01-01T09:00:00+00:00"&&changes_xml.object_changes[1].timestamp=="2020-01-01T05:00:00+00:00","OCEL XML stores object changes like events");
 // OCEL 2.0 XML carries attribute values as element text: PM4Py 2.7.22
 // read_ocel2_xml reads cost 5, p x/y/z and status "a<b&c" back.
 auto valued=changed;
 valued.events[0].attributes.push_back({"cost",std::int64_t{5}});
 valued.objects[0].attributes.push_back({"status",std::string("a<b&c")});
 OcelXmlExportOptions standard_xml; standard_xml.format=OcelXmlFormat::kStandard;
 const auto written=serialize_ocel_xml(valued,standard_xml);
 check(written.find(R"(<attribute name="cost" type="int">5</attribute>)")!=std::string::npos&&
       written.find(R"(<attribute name="p" time="2020-01-01T09:00:00+00:00" type="string">y</attribute>)")!=std::string::npos&&
       written.find(R"(>a&lt;b&amp;c</attribute>)")!=std::string::npos&&written.find(" value=\"")==std::string::npos,"OCEL 2.0 XML writes values as element text");
 const auto xml_back=parse_ocel_xml(written);
 bool cost=false,status=false;
 for(const auto& a:xml_back.events.at(0).attributes)cost|=a.key=="cost"&&a.value==OcelValue(std::int64_t{5});
 for(const auto& a:xml_back.objects.at(0).attributes)status|=a.key=="status"&&a.value==OcelValue(std::string("a<b&c"));
 check(cost&&status&&xml_back.object_changes.size()==2,"OCEL 2.0 XML element-text values read back");
 // Whitespace-only and CDATA text are values too: PM4Py 2.7.22 read_ocel2_xml
 // keeps "  " and "\n", and so must our reader of our own export.
 auto spaced=changed;
 spaced.events[0].attributes.push_back({"blank",std::string("  ")});
 spaced.events[0].attributes.push_back({"line",std::string("\n")});
 const auto spaced_back=parse_ocel_xml(serialize_ocel_xml(spaced,standard_xml));
 bool blank=false,line=false;
 for(const auto& a:spaced_back.events.at(0).attributes){blank|=a.key=="blank"&&a.value==OcelValue(std::string("  "));line|=a.key=="line"&&a.value==OcelValue(std::string("\n"));}
 check(blank&&line,"OCEL 2.0 XML keeps whitespace-only values");
 const auto cdata=parse_ocel_xml(R"(<?xml version="1.0"?><log><object-types/><event-types><event-type name="A"><attributes><attribute name="s" type="string"/></attributes></event-type></event-types><objects><object id="o1" type="item"><attributes/></object></objects><events><event id="e1" type="A" time="2020-01-01T00:00:00Z"><attributes><attribute name="s"><![CDATA[a<b]]> c</attribute></attributes><objects><relationship object-id="o1" qualifier=""/></objects></event></events></log>)");
 check(cdata.events.at(0).attributes.size()==1&&cdata.events[0].attributes[0].value==OcelValue(std::string("a<b c")),"OCEL 2.0 XML reads CDATA text");
 // Nulls are left out and booleans written as Python text, as PM4Py's writer
 // does: read_ocel2_xml of this export gives f [1.5, nan] and flag ['True',
 // 'False'], the same as for PM4Py's own export of the log (it raised
 // float(None) on the empty null element before).
 Ocel nulls=changed;
 nulls.events.push_back(nulls.events[0]);nulls.events[1].id="e2";
 nulls.relations.push_back({"e2","o1","q"}); // readers drop events without an e2o relation
 nulls.events[0].attributes={{"f",1.5},{"flag",true}};
 nulls.events[1].attributes={{"f",nullptr},{"flag",false}};
 nulls.objects[0].attributes.push_back({"gone",nullptr});
 const auto null_xml=serialize_ocel_xml(nulls,standard_xml);
 check(null_xml.find(R"(type="null")")==std::string::npos&&null_xml.find(R"(name="gone" type="string" time)")==std::string::npos&&
       null_xml.find(R"(<attribute name="f" type="float"/>)")!=std::string::npos&&
       null_xml.find(R"(<attribute name="flag" type="boolean">True</attribute>)")!=std::string::npos&&
       null_xml.find(R"(<attribute name="flag" type="boolean">False</attribute>)")!=std::string::npos,"OCEL 2.0 XML omits nulls and writes Python booleans");
 const auto null_back=parse_ocel_xml(null_xml);
 bool flags=false;
 for(const auto& e:null_back.events)for(const auto& a:e.attributes)if(a.key=="flag")flags|=e.id=="e2"&&a.value==OcelValue(false);
 check(flags&&null_back.events.size()==2,"OCEL 2.0 XML Python booleans read back");
 // A field with changes but no base value: no null base entry, so the first
 // change reads back as the base (PM4Py reads its own export the same way).
 Ocel unseeded=changed;
 unseeded.objects[0].attributes.clear();
 const auto unseeded_back=parse_ocel_xml(serialize_ocel_xml(unseeded,standard_xml));
 check(unseeded_back.objects.at(0).attributes.size()==1&&unseeded_back.object_changes.size()==1,"OCEL 2.0 XML seeds no null base entry");
}

// K1: a nanosecond duration beyond int64 overflows as in the reference; a
// microsecond column holds it. K2: ints mixed with floats key a float64
// index, so an int case id no double holds drops out.
void case_performance_contracts(){
 const auto table_of=[](const char* unit){
  EventTable table;table.columns={"case:concept:name","concept:name","time:timestamp"};table.dtypes={"object","object",std::string("datetime64[")+unit+"]"};
  table.rows.push_back({text("case:concept:name","c"),text("concept:name","A"),when("time:timestamp","1800-01-01T00:00:00")});
  table.rows.push_back({text("case:concept:name","c"),text("concept:name","B"),when("time:timestamp","2201-01-01T00:00:00")});
  return table;};
 bool overflow=false;
 try{(void)filter_pandas_case_performance(table_of("ns"),real(0),real(1e20));}catch(const PandasCaseFilterError& e){overflow=e.type=="OverflowError"&&e.message=="Overflow in int64 addition";}
 check(overflow,"K1 nanosecond overflow raises");
 check(filter_pandas_case_performance(table_of("us"),real(0),real(1e20)).size()==2,"K1 microsecond column keeps the case");
 const auto ids_kept=[](std::vector<EventAttribute> ids){
  EventTable table;table.columns={"case:concept:name","concept:name","time:timestamp"};table.dtypes={"object","object","datetime64[ns]"};
  for(auto& id:ids)table.rows.push_back({id,text("concept:name","A"),when("time:timestamp","2020-01-01T00:00:00")});
  std::string out;for(const auto& row:filter_pandas_case_performance(table,real(0),real(10)).rows)out+=row[0]->value+",";
  return out;};
 check(ids_kept({signed_id(9007199254740993),real_id(9007199254740994.0,"9007199254740994.0")})=="9007199254740994.0,","K2 2**53+1 with a float drops");
 check(ids_kept({signed_id(9007199254740993),signed_id(5)})=="9007199254740993,5,","K2 ints only keep");
 check(ids_kept({signed_id(9007199254740992),real_id(9007199254740994.0,"9007199254740994.0")})=="9007199254740992,9007199254740994.0,","K2 representable int keeps");
 check(ids_kept({signed_id(9007199254740993),text_id("x")})=="9007199254740993,x,","K2 object index keeps");
 check(ids_kept({bool_id(true),real_id(2.5,"2.5")})=="True,2.5,","K2 bool with float keeps both");
 check(ids_kept({unsigned_id(std::numeric_limits<std::uint64_t>::max()),signed_id(-1)})=="18446744073709551615,-1,","K2 uint64 max with -1 keeps");
 check(ids_kept({unsigned_id(std::numeric_limits<std::uint64_t>::max()),real_id(1.5,"1.5")})=="1.5,","K2 uint64 max with a float drops");
}

// J15-J22, K4: Polars dataframe DFG, triples, footprints, formatting and
// distributions; expected values are the reference results of the
// reviewer's minimized cases.
EventAttribute null_cell(std::string key){EventAttribute a{std::move(key),""};a.is_null=true;return a;}
struct Row{const char* c;const char* a;const char* t;const char* start;std::optional<double> cost;};
LazyEventTable polars_table(const std::vector<Row>& rows,const char* unit="us"){
 EventTable table;table.columns={"case:concept:name","concept:name","time:timestamp","begin","cost"};
 const std::string date=std::string("Datetime(time_unit='")+unit+"', time_zone='UTC')";
 table.dtypes={"String","String",date,date,"Float64"};
 for(const auto& r:rows){
  EventAttribute cost=null_cell("cost");if(r.cost)cost=cell_of("cost",real(*r.cost));
  table.rows.push_back({r.c?text("case:concept:name",r.c):null_cell("case:concept:name"),r.a?text("concept:name",r.a):null_cell("concept:name"),
                        when("time:timestamp",r.t),r.start?when("begin",r.start):null_cell("begin"),cost});
 }
 return LazyEventTable::from_table(table);
}
double scalar(const DfgPerformanceStatistics& s){return std::get<double>(s.value);}
void polars_dataframe_contracts(){
 DfgTableOptions both;both.measure=DfgTableMeasure::kBoth;
 // J17: an explicit null start sorts first and gives a zero duration.
 {auto o=both;o.start_timestamp_attribute="begin";
  const auto r=discover_polars_dfg(polars_table({{"u","P","2029-02-06T07:08:01Z","2029-02-06T07:08:00Z",{}},{"u","Q","2029-02-06T07:08:09Z",nullptr,{}},{"u","R","2029-02-06T07:08:05Z","2029-02-06T07:08:04Z",{}}}),o);
  check(r.frequency.size()==2&&r.frequency.count({"P","R"})&&r.frequency.count({"Q","P"})&&scalar(r.performance.at({"P","R"}))==3.0&&scalar(r.performance.at({"Q","P"}))==0.0,"J17 explicit null start");
  const auto all=discover_polars_dfg(polars_table({{"u","P","2029-02-06T07:08:01Z",nullptr,{}},{"u","Q","2029-02-06T07:08:09Z",nullptr,{}},{"u","R","2029-02-06T07:08:05Z",nullptr,{}}}),o);
  check(all.frequency.count({"P","R"})&&all.frequency.count({"R","Q"})&&scalar(all.performance.at({"P","R"}))==0.0&&scalar(all.performance.at({"R","Q"}))==0.0,"J17 all starts null");}
 // J18: nanosecond order.
 {const auto t=polars_table({{"u","Q","2029-02-06T07:08:01.000000090Z",nullptr,{}},{"u","P","2029-02-06T07:08:01.000000010Z",nullptr,{}},{"u","R","2029-02-06T07:08:01.000000180Z",nullptr,{}}},"ns");
  const auto r=discover_polars_dfg(t,both);
  check(r.frequency.size()==2&&r.frequency.count({"P","Q"})&&r.frequency.count({"Q","R"})&&scalar(r.performance.at({"P","Q"}))==0.0,"J18 DFG nanosecond order");
  const auto triples=discover_polars_frequency_triple_table(t,both);
  check(triples.triples.size()==1&&triples.triples.count({"P","Q","R"}),"J18 triples nanosecond order");
  const auto f=discover_footprints(t);
  check(f.start_activities==std::set<std::string>{"P"}&&f.end_activities==std::set<std::string>{"R"},"J18 footprints nanosecond order");}
 // J19: triples keep the null case group.
 {const auto r=discover_polars_frequency_triple_table(polars_table({{nullptr,"P","2029-02-06T07:08:01Z",nullptr,{}},{nullptr,"Q","2029-02-06T07:08:03Z",nullptr,{}},{nullptr,"P","2029-02-06T07:08:05Z",nullptr,{}},{"u","R","2029-02-06T07:08:03Z",nullptr,{}}}),both);
  check(r.triples.size()==1&&r.triples.count({"P","Q","P"})&&r.triples.at({"P","Q","P"})==1,"J19 null case triple");}
 // J20: cost mode keeps null sources and rejects stdev and raw_values.
 {auto o=both;o.measure=DfgTableMeasure::kCost;o.cost_attribute="cost";
  const auto t=polars_table({{"u",nullptr,"2029-02-06T07:08:01Z",nullptr,2.5},{"u","P","2029-02-06T07:08:03Z",nullptr,3.5},{"u","Q","2029-02-06T07:08:05Z",nullptr,{}}});
  const auto r=discover_polars_dfg(t,o);
  check(r.performance.size()==1&&scalar(r.performance.at({"P","Q"}))==0.0&&r.null_source_performance.size()==1&&scalar(r.null_source_performance.at("P"))==3.5,"J20 cost null source");
  check(serialize_dfg_dataframe_json(r).find("\"source\":null")!=std::string::npos,"J16 null source serialized");
  for(const auto aggregation:{DfgPerformanceAggregation::kStandardDeviation,DfgPerformanceAggregation::kRawValues}){
   o.performance_aggregation=aggregation;bool rejected=false;
   try{(void)discover_polars_dfg(t,o);}catch(const std::invalid_argument&){rejected=true;}
   check(rejected,"J20 cost rejects stdev/raw_values");}}
 // J15: a singleton performance edge serializes with a null deviation.
 {auto o=both;o.measure=DfgTableMeasure::kPerformance;
  const auto r=discover_polars_dfg(polars_table({{"u","P","2029-02-06T07:08:01Z",nullptr,{}},{"u","Q","2029-02-06T07:08:03Z",nullptr,{}}}),o);
  bool ok=false;try{ok=serialize_dfg_dataframe_json(r).find("\"stdev\":null")!=std::string::npos;}catch(const std::exception&){}
  check(ok,"J15 singleton performance serializes");}
 // K4: Polars durations are whole seconds.
 {const auto r=discover_polars_dfg(polars_table({{"u","P","2029-02-06T07:08:01.100001Z",nullptr,{}},{"u","Q","2029-02-06T07:08:01.300002Z",nullptr,{}},{"u","P","2029-02-06T07:08:01.600004Z",nullptr,{}}}),both);
  check(scalar(r.performance.at({"P","Q"}))==0.0&&scalar(r.performance.at({"Q","P"}))==0.0,"K4 whole-second durations");}
 // J21: a known category column without its dictionary is not converted.
 {EventTable t;t.columns={"case","act","time","begin","target"};t.dtypes={"object","object","category","object","object"};
  t.categorical_dtypes={std::nullopt,std::nullopt,std::nullopt,std::nullopt,std::nullopt};
  t.rows.push_back({text("case","u"),text("act","P"),text("time","2029-03-01 10:01:00"),text("begin","2029-03-01 09:01:00"),null_cell("target")});
  t.rows.push_back({text("case","u"),text("act","Q"),text("time","2029-03-01 09:01:00"),text("begin","2029-03-01 08:01:00"),null_cell("target")});
  DataFrameFormatOptions o;o.case_id="case";o.activity_key="act";o.timestamp_key="time";o.start_timestamp_key="begin";
  const auto f=format_dataframe(t,o);
  const auto time=f.column_index("time"),canonical=f.column_index("time:timestamp");
  check(f.dtypes[time]=="category"&&f.dtypes[canonical]=="category"&&f.rows[0][canonical]->value=="2029-03-01 09:01:00","J21 category without dictionary stays category");}
 // J22: calendar bins use the datetime's own offset.
 {EventLog log;Trace trace;Event e;e.case_id="u";e.activity="P";e.timestamp="2029-01-01T00:30:00+03:00";trace.events.push_back(e);log.traces.push_back(trace);
  AttributeDistributionOptions o;o.timestamp_key="time:timestamp";
  o.distribution=EventDistributionType::kYears;auto r=discover_event_distribution(log,o);
  check(r.labels==std::vector<std::string>{"2029"}&&r.counts==std::vector<std::uint64_t>{1},"J22 local year");
  o.distribution=EventDistributionType::kHours;r=discover_event_distribution(log,o);
  check(r.counts.size()==24&&r.counts[0]==1,"J22 local hour");
  log.traces[0].events[0].timestamp="2028-12-31T23:30:00-03:00";o.distribution=EventDistributionType::kWeeks;r=discover_event_distribution(log,o);
  check(r.counts.size()==53&&r.counts[52]==1,"J22 local week");}
}
void streaming_and_storage_contracts(){
 // J12: a selected Boolean streams as Python's str(), True/False, whatever
 // its carrier; a genuine string "true" stays lexical.
 {Event e;EventAttribute typed;typed.key="flag";typed.value="true";typed.kind=AttributeKind::kBoolean;typed.typed_value=true;
  EventAttribute lexical{"lexical","false",AttributeKind::kBoolean};EventAttribute word{"word","true",AttributeKind::kString};
  e.attributes={typed,lexical,word};
  check(streaming_event_activity(e,"flag")==std::optional<std::string>("True"),"J12 typed Boolean label");
  check(streaming_event_activity(e,"lexical")==std::optional<std::string>("False"),"J12 lexical Boolean label");
  check(streaming_event_activity(e,"word")==std::optional<std::string>("true"),"J12 string control");
  Event projected;projected.activity="true";projected.activity_state=FirstClassAttributeState::kPresent;
  projected.activity_attribute=EventAttribute{"concept:name","true",AttributeKind::kBoolean};
  check(streaming_event_activity(projected,"concept:name")==std::optional<std::string>("True"),"J12 projected Boolean label");
  // J1: a requested literal "activity" never aliases the canonical projection.
  check(!streaming_event_activity(projected,"activity"),"J1 streaming missing literal activity skipped");}
 // J14: a projected null activity is skipped, so Finalize is an invalid
 // start; a present empty activity is a label and the case fits.
 {Footprint fp;fp.activities={"","Finalize"};fp.start_activities={""};fp.end_activities={"Finalize"};fp.sequence.insert({"","Finalize"});
  const auto run=[&](bool null_first){
   StreamingFootprintsConformance monitor(fp,{"concept:name","case:concept:name"});
   Event first;first.case_id="projection-case";first.activity="";
   first.activity_state=null_first?FirstClassAttributeState::kNull:FirstClassAttributeState::kPresent;first.activity_source_key="concept:name";
   EventAttribute cell{"concept:name","",AttributeKind::kString};cell.is_null=null_first;first.activity_attribute=cell;
   Event second;second.case_id="projection-case";second.activity="Finalize";second.activity_state=FirstClassAttributeState::kPresent;
   (void)monitor.receive(first,"projection-case");(void)monitor.receive(second,"projection-case");
   return monitor.snapshot();};
  const auto null_case=run(true),empty_case=run(false);
  check(null_case.total_events_processed==1&&!null_case.cases.at("projection-case").is_fit,"J14 projected null skipped");
  check(empty_case.total_events_processed==2&&empty_case.cases.at("projection-case").is_fit,"J14 present empty fits");}
 // J34: stored batch results keep a null resource apart from an empty one.
 {BatchDiscovery d;d.variant=BatchDiscoveryVariant::kPolars;d.options.variant=BatchDiscoveryVariant::kPolars;
  BatchGroup null_group;null_group.activity="Pack";null_group.resource_is_null=true;
  BatchGroup empty_group;empty_group.activity="Ship";
  d.groups={null_group,empty_group};
  const auto parsed=parse_timing_workbench_batches_json(serialize_timing_workbench_result_json(d));
  check(parsed.groups.size()==2&&parsed.groups[0].resource_is_null&&!parsed.groups[1].resource_is_null&&parsed.groups[1].resource.empty(),"J34 null resource round trip");}
}
} // namespace
int main(){
 duration_thresholds();
 sequence_case_identity();
 timestamp_grouping_instants();
 grouped_key_dtypes();
 batch_scalar_identity();
 polars_batch_boundaries();
 ocel_import_order();
 ocel_wall_clock_order();
 case_performance_contracts();
 polars_dataframe_contracts();
 streaming_and_storage_contracts();
 if(failures){std::cerr<<failures<<" recheck regressions failed\n";return 1;}
 std::cout<<"recheck regressions passed\n";
}
