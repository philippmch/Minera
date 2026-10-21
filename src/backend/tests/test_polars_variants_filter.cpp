#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "minera/polars_variants_filter.hpp"

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
    throw std::runtime_error("Full Polars variants frame mismatch: " + context);
  for (std::size_t row = 0; row < actual.rows.size(); ++row)
    for (std::size_t column = 0; column < actual.columns.size(); ++column) {
      const auto& a = actual.rows[row][column]; const auto& b = expected.rows[row][column];
      if (!a || !b) continue;
      const auto* left = std::get_if<double>(&a->typed_value);
      const auto* right = std::get_if<double>(&b->typed_value);
      if (left && right && std::bit_cast<std::uint64_t>(*left) != std::bit_cast<std::uint64_t>(*right))
        throw std::runtime_error("Polars variants floating bits changed: " + context);
    }
}

template <class Action>
void error_is(Action action, const std::string& type, const std::string& phase,
              const std::string& message, const std::string& context) {
  bool thrown = false;
  try { action(); }
  catch (const PolarsVariantsFilterError& error) {
    thrown = true;
    if (error.type != type || error.phase != phase || error.message != message) {
      const auto mismatch = std::mismatch(message.begin(), message.end(), error.message.begin(), error.message.end());
      std::cerr << "Wrong Polars variants diagnostic for " << context << ": " << error.type << '/' << error.phase
                << "; first differing byte " << std::distance(message.begin(), mismatch.first)
                << "; expected bytes " << message.size() << "; actual bytes " << error.message.size() << '\n';
      std::cerr.write(error.message.data(), static_cast<std::streamsize>(error.message.size()));
      throw std::runtime_error("Captured Polars variants type/phase/full-message mismatch: " + context);
    }
  }
  if (!thrown) throw std::runtime_error("Expected retained Polars variants diagnostic: " + context);
}
LazyEventTable counted_source(const EventTable& table, std::size_t& scans) {
  return LazyEventTable(table.columns, [table, &scans](const auto& consume) {
    ++scans; for (const auto& row : table.rows) consume(row);
  }, table.attributes, table.metadata, table.rows.size(), table.dtypes, table.index, table.categorical_dtypes);
}

#include "polars_variants_filter_public_vectors.inc"

void native_complete_context_and_apply_scan() {
  EventTable source;source.columns={"case:concept:name","concept:name","time:timestamp","payload"};source.dtypes={"String","String","Datetime(time_unit='us', time_zone='UTC')","Float64"};
  source.rows={{EventAttribute{source.columns[0],"c0"},EventAttribute{source.columns[1],"A"},date_value(source.columns[2],"2043-01-01T00:00:02+00:00"),float_value("payload",-0.0)},
               {EventAttribute{source.columns[0],"c1"},EventAttribute{source.columns[1],"A"},date_value(source.columns[2],"2043-01-01T00:00:00+00:00"),float_value("payload",0.0)},
               {EventAttribute{source.columns[0],"c0"},EventAttribute{source.columns[1],"B"},date_value(source.columns[2],"2043-01-01T00:00:01+00:00"),float_value("payload",0.0)},
               {EventAttribute{source.columns[0],"c1"},EventAttribute{source.columns[1],"B"},date_value(source.columns[2],"2043-01-01T00:00:03+00:00"),float_value("payload",-0.0)},
               {null_value(source.columns[0]),EventAttribute{source.columns[1],"A"},date_value(source.columns[2],"2043-01-01T00:00:00+00:00"),float_value("payload",-0.0)}};
  source.attributes={{"context","whole source"}};source.metadata.properties={{"property","whole metadata"}};
  source.rows[0][0]->first_class_role=FirstClassAttributeRole::kCaseId;source.rows[0][0]->first_class_state=FirstClassAttributeState::kPresent;source.rows[0][0]->first_class_source_key="literal case provenance";
  const auto before=source;const auto admitted=collection_value("",AttributeContainerKind::kList,{collection_value("",AttributeContainerKind::kTuple,{{"","A"},{"","B"}})});
  auto member=source;member.rows={source.rows[1],source.rows[3]};std::size_t scans=0;
  const auto input=counted_source(source,scans);const auto selected=filter_polars_variants(input,admitted);assert(scans==0);same_frame(selected.materialize(),member,"canonical timestamp ordering; source row output order");assert(scans==1);
  auto top=source;top.rows={source.rows[0],source.rows[2]};const auto ranked=filter_polars_variants_top_k(input,integer_value("",1));assert(scans==2);same_frame(ranked.materialize(),top,"top-k count then lexical descending tie");assert(scans==3);
  auto independent=ranked.materialize();assert(scans==4);independent.rows[0][0]->value="changed";same_frame(source,before,"restartable complete context and independent result");
  PolarsVariantsFilterOptions options;options.positive=boolean_value("",false);auto excluded=source;excluded.rows={source.rows[1],source.rows[3],source.rows[4]};same_frame(filter_polars_variants_top_k(input,integer_value("",1),options).materialize(),excluded,"negative top-k preserves null case row");assert(scans==6);
  const auto unavailable_scans=scans;
  error_is([&]{(void)auto_filter_polars_variants(input);},"AttributeError","apply","module 'pm4py.util.constants' has no attribute 'DEFAULT_NAME_KEY'","auto unavailable before source scan");assert(scans==unavailable_scans);
  error_is([&]{(void)filter_polars_variants_from_list(input,EventAttribute{"","AB"});},"AttributeError","apply","module 'pm4py.util.constants' has no attribute 'DEFAULT_NAME_KEY'","from-list unavailable before source scan");assert(scans==unavailable_scans);
  error_is([&]{(void)filter_polars_variants(input,std::nullopt);},"TypeError","apply","apply() missing 1 required positional argument: 'admitted_variants'","missing argument differs from explicit None");assert(scans==unavailable_scans);
  error_is([&]{(void)filter_polars_variants(input,null_value(""));},"TypeError","apply","'NoneType' object is not iterable","explicit None supplied");assert(scans==unavailable_scans);
  same_frame(source,before,"all native operations preserve source metadata/provenance");
}

void native_enum_validation_before_sort_comparison() {
  EventTable source;source.columns={"case:concept:name","concept:name","time:timestamp"};source.dtypes={"Enum(categories=['c'])","String","Datetime(time_unit='us', time_zone='UTC')"};source.rows={{EventAttribute{source.columns[0],"c"},EventAttribute{source.columns[1],"A"},date_value(source.columns[2],"2043-01-01T00:00:00+00:00")}};
  const auto query=collection_value("",AttributeContainerKind::kList,{collection_value("",AttributeContainerKind::kList,{{"","A"}})});
  bool rejected=false;try{(void)filter_polars_variants(source,query);}catch(const std::invalid_argument& error){rejected=std::string(error.what()).find("require an Enum dictionary")!=std::string::npos;}assert(rejected);
  source.categorical_dtypes.resize(3);EventTableCategoricalDtype dictionary;dictionary.ordered=true;dictionary.categories_dtype="String";dictionary.categories={{"","other"}};source.categorical_dtypes[0]=dictionary;
  rejected=false;try{(void)filter_polars_variants(source,query);}catch(const std::invalid_argument& error){rejected=std::string(error.what()).find("Enum cell absent")!=std::string::npos;}assert(rejected);
}
}  // namespace

int main(){try{retained_polars_variants_filter_cases();native_complete_context_and_apply_scan();native_enum_validation_before_sort_comparison();std::cout<<"Polars variants: 411 retained calls; 142 exact complete frames, 269 full phase/message diagnostics; native context/top-k apply-scan controls passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
