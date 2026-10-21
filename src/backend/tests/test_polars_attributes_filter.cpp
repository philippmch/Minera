#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <cmath>
#include <stdexcept>
#include <string>

#include "minera/polars_attributes_filter.hpp"
#include "minera/token_integer.hpp"

namespace {
using namespace minera;

EventAttribute null_value(std::string key) {
  EventAttribute value{std::move(key), ""}; value.is_null = true; return value;
}
template <class Integer>
EventAttribute integer_value(std::string key, Integer number) {
  EventAttribute value{std::move(key), std::to_string(number), AttributeKind::kInt};
  value.typed_value = number; return value;
}
EventAttribute float_value(std::string key, double number) {
  EventAttribute value{std::move(key), std::to_string(number), AttributeKind::kFloat};
  value.typed_value = number; return value;
}
EventAttribute boolean_value(std::string key, bool boolean) {
  EventAttribute value{std::move(key), boolean ? "true" : "false", AttributeKind::kBoolean};
  value.typed_value = boolean; return value;
}
EventAttribute date_value(std::string key, std::string instant) {
  EventAttribute value{std::move(key), instant, AttributeKind::kDate};
  value.typed_value = NativeDate{instant}; return value;
}
EventAttribute collection_value(std::string key, AttributeContainerKind kind,
                                std::vector<EventAttribute> children) {
  EventAttribute value{std::move(key), ""}; value.container = kind;
  value.children = std::move(children); return value;
}

void same_frame(const EventTable& actual, const EventTable& expected, const std::string& context) {
  if (actual.columns != expected.columns || actual.dtypes != expected.dtypes || actual.rows != expected.rows ||
      actual.attributes != expected.attributes || actual.metadata != expected.metadata ||
      actual.index != expected.index || actual.categorical_dtypes != expected.categorical_dtypes)
    throw std::runtime_error("Full Polars attributes frame mismatch: " + context);
  for (std::size_t row = 0; row < actual.rows.size(); ++row)
    for (std::size_t column = 0; column < actual.columns.size(); ++column) {
      const auto& a = actual.rows[row][column]; const auto& b = expected.rows[row][column];
      if (!a || !b) continue;
      const auto* left = std::get_if<double>(&a->typed_value);
      const auto* right = std::get_if<double>(&b->typed_value);
      if (left && right && std::bit_cast<std::uint64_t>(*left) != std::bit_cast<std::uint64_t>(*right))
        throw std::runtime_error("Polars attributes floating bits changed: " + context);
    }
}

template <class Action>
void error_is(Action action, const std::string& type, const std::string& phase,
              const std::string& message, const std::string& context) {
  bool thrown = false;
  try { action(); }
  catch (const PolarsAttributesFilterError& error) {
    thrown = true;
    if (error.type != type || error.phase != phase || error.message != message) {
      const auto mismatch = std::mismatch(message.begin(), message.end(), error.message.begin(), error.message.end());
      std::cerr << "Wrong Polars attributes diagnostic for " << context << ": " << error.type << '/' << error.phase
                << "; first differing byte " << std::distance(message.begin(), mismatch.first)
                << "; expected bytes " << message.size() << "; actual bytes " << error.message.size() << '\n';
      std::cerr.write(error.message.data(), static_cast<std::streamsize>(error.message.size()));
      throw std::runtime_error("Captured Polars attributes type/phase/full-message mismatch: " + context);
    }
  }
  if (!thrown) throw std::runtime_error("Expected retained Polars attributes diagnostic: " + context);
}
LazyEventTable counted_source(const EventTable& table, std::size_t& scans) {
  return LazyEventTable(table.columns, [table, &scans](const auto& consume) {
    ++scans; for (const auto& row : table.rows) consume(row);
  }, table.attributes, table.metadata, table.rows.size(), table.dtypes, table.index, table.categorical_dtypes);
}

bool same_count_key(const EventAttribute& a,const EventAttribute& b) {
  if(a.is_null||b.is_null)return a.is_null&&b.is_null;
  if(a.kind!=b.kind)return false;
  if(a.kind==AttributeKind::kInt)return TokenInteger(a.value)==TokenInteger(b.value);
  if(a.kind==AttributeKind::kFloat) {
    const auto x=std::get<double>(a.typed_value),y=std::get<double>(b.typed_value);
    return std::bit_cast<std::uint64_t>(x)==std::bit_cast<std::uint64_t>(y)||(std::isnan(x)&&std::isnan(y));
  }
  return a.value==b.value;
}
void same_counts(const PolarsAttributeCounts& actual,const PolarsAttributeCounts& expected,const std::string& context) {
  if(actual.size()!=expected.size())throw std::runtime_error("Typed counts size mismatch: "+context);
  std::vector<bool> found(actual.size(),false);
  for(const auto& e:expected) {
    bool matched=false;
    for(std::size_t i=0;i<actual.size();++i)if(!found[i]&&same_count_key(actual[i].value,e.value)&&TokenInteger(actual[i].count.value)==TokenInteger(e.count.value)){found[i]=true;matched=true;break;}
    if(!matched)throw std::runtime_error("Complete typed counts key/count mismatch: "+context);
  }
}
void same_top_cutoff_family(const EventTable& actual,const EventTable& source,const EventTable& retained,
                            const std::string& key,std::size_t k,const std::string& context) {
  const auto column=source.column_index(key);
  struct Group {EventAttribute value;std::size_t count;};std::vector<Group> groups;
  for(const auto& row:source.rows) {
    assert(row[column]);const auto& value=*row[column];
    auto found=std::find_if(groups.begin(),groups.end(),[&](const auto& g){return same_count_key(g.value,value);});
    if(found==groups.end())groups.push_back({value,1});else ++found->count;
  }
  std::vector<std::size_t> ranking;for(const auto& g:groups)ranking.push_back(g.count);
  std::sort(ranking.begin(),ranking.end(),std::greater<>());assert(k>0&&k<ranking.size()&&ranking[k-1]==ranking[k]);
  auto validate=[&](const EventTable& output) {
    std::vector<bool> selected(groups.size(),false);
    for(const auto& row:output.rows) {
      if(!row[column])throw std::runtime_error("Cutoff output missing selected cell: "+context);
      bool known=false;for(std::size_t i=0;i<groups.size();++i)if(same_count_key(groups[i].value,*row[column])){selected[i]=true;known=true;break;}
      if(!known)throw std::runtime_error("Cutoff output unknown group: "+context);
    }
    if(std::count(selected.begin(),selected.end(),true)!=k)throw std::runtime_error("Cutoff output group cardinality: "+context);
    for(std::size_t i=0;i<groups.size();++i)
      if((groups[i].count>ranking[k-1]&&!selected[i])||(groups[i].count<ranking[k-1]&&selected[i]))throw std::runtime_error("Cutoff frequency rank violated: "+context);
    auto full=source;full.rows.clear();
    for(const auto& row:source.rows)for(std::size_t i=0;i<groups.size();++i)if(selected[i]&&same_count_key(groups[i].value,*row[column])){full.rows.push_back(row);break;}
    same_frame(output,full,context+" complete selected rows and context");
  };
  validate(retained);validate(actual);
}
#include "polars_attributes_filter_public_vectors.inc"

void native_controls() {
  EventTable source;source.columns={"case:concept:name","concept:name","amount"};source.dtypes={"String","String","Int64"};
  source.attributes={{"context","retained"}};source.metadata.properties={{"property","complete"}};
  source.rows={{EventAttribute{source.columns[0],"b"},EventAttribute{source.columns[1],"A"},integer_value(source.columns[2],std::int64_t{9007199254740993LL})},
               {EventAttribute{source.columns[0],"a"},EventAttribute{source.columns[1],"B"},integer_value(source.columns[2],std::int64_t{9007199254740995LL})},
               {EventAttribute{source.columns[0],"b"},EventAttribute{source.columns[1],"X"},integer_value(source.columns[2],std::int64_t{9007199254740997LL})}};
  const auto before=source;const auto query=collection_value("",AttributeContainerKind::kList,{EventAttribute{"","A"}});
  auto expected=source;expected.rows={source.rows[0],source.rows[2]};
  same_frame(filter_polars_attributes(source,query),expected,"direct whole-case noncontiguous source ordering/context");
  expected.rows={source.rows[0]};same_frame(filter_polars_attributes_events(source,query),expected,"direct event membership");
  PolarsAttributesFilterOptions options;options.attribute_key={"","amount"};
  same_frame(filter_polars_attributes_numeric_events(source,integer_value("",std::int64_t{9007199254740993LL}),integer_value("",std::int64_t{9007199254740993LL}),options),expected,"exact large int bounds");
  std::size_t scans=0;const auto lazy=counted_source(source,scans);const auto filtered=filter_polars_attributes_events(lazy,query);assert(scans==0);
  same_frame(filtered.materialize(),expected,"lazy first scan");same_frame(filtered.materialize(),expected,"lazy restartability");assert(scans==2);
  auto counts=get_polars_attribute_values(lazy,{"","concept:name"});assert(scans==3);assert(counts.size()==3);
  PolarsAttributeCounts cache={{integer_value("",std::int64_t{1}),integer_value("",std::int64_t{5})},{boolean_value("",true),integer_value("",std::int64_t{0})},{float_value("",1.0),integer_value("",std::int64_t{2})}};
  EventTable numeric=source;numeric.dtypes[1]="Int64";for(auto& row:numeric.rows)row[1]=integer_value(numeric.columns[1],std::int64_t{1});
  same_frame(filter_polars_attributes_threshold(numeric,integer_value("",std::int64_t{2}),{},PolarsAttributeCountCache{cache}),numeric,"typed Python dict duplicate numeric keys retain first key and last count");
  auto none=numeric;none.rows.clear();same_frame(filter_polars_attributes_threshold(numeric,integer_value("",std::int64_t{3}),{},PolarsAttributeCountCache{cache}),none,"numeric-equal cache last count replaces prior value");
  same_frame(source,before,"native source preservation");
}
}  // namespace
int main() {try {retained_polars_attributes_filter_cases();native_controls();std::cout<<"726 retained Polars attributes outcomes: complete frames, typed maps, and full phase/message diagnostics passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}catch(...){std::cerr<<"Unknown native Polars attributes exception\n";return 1;}}
