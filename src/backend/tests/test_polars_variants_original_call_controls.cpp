#include "../polars_variants_original_call_parser.hpp"
#include <bit>
#include <iostream>
namespace {
using namespace minera;
namespace o=polars_variants_original_call_detail;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
EventAttribute value(const std::string& text){return o::packed(text,o::shared::Reader(text).read());}
std::string call(const std::string& entries="[]",const std::string& kind="LazyFrame"){return "{\"schema\":\"minera-polars-variants-original-call-v1\",\"input_kind\":\""+kind+"\",\"keyword_entries\":"+entries+'}';}
void check(){
  require(std::get<std::int64_t>(value(R"({"type":"int","value":-9223372036854775808})").typed_value)==INT64_MIN,"int64 minimum lost");
  require(std::get<std::uint64_t>(value(R"({"type":"int","value":18446744073709551615})").typed_value)==UINT64_MAX,"uint64 maximum lost");
  const auto nested=value(R"({"type":"tuple","values":[{"type":"float","hex":"-0x0.0p+0"},{"type":"list","values":[{"type":"int","value":9007199254740993}]}]})");
  require(nested.children[0].key.empty()&&nested.children[1].children[0].key.empty(),"Polars sequence keys are not empty");
  require(nested.children[0].value=="-0.000000"&&std::bit_cast<std::uint64_t>(std::get<double>(nested.children[0].typed_value))==std::bit_cast<std::uint64_t>(-0.0),"GEN double lexical or bits lost");
  const auto date=value(R"({"type":"datetime","iso":"2026-06-01T00:00:00+02:00","fold":1,"offset_seconds":7200.0})");const auto& native=std::get<NativeDate>(date.typed_value);
  require(native.value=="2026-06-01T00:00:00+02:00"&&native.resolution==NativeDateResolution::kMicrosecond&&!native.timezone,"GEN NativeDate was converted to pandas carrier");
  require(std::get<NativeDate>(value(R"({"type":"date","iso":"2026-06-01"})").typed_value).resolution==NativeDateResolution::kMicrosecond,"Date carrier lost");
  const auto dictionary=value(R"({"type":"dict","items":[[{"type":"str","value":"z"},{"type":"int","value":1}],[{"type":"str","value":"a"},{"type":"str","value":""}]]})");require(dictionary.children[0].key=="z"&&dictionary.children[1].key=="a","ordinary dictionary order sorted");
  const auto missing=o::parse(call(R"([{"key":"parameters","value":{"type":"str","value":"unused"}}])"),"membership");
  require(o::missing_required(missing)&&missing.raw_parameters.value=="unused"&&o::invoked_arm(missing)=="missing_required_default_options","missing argument passed constructed parameters");
  const auto none=o::parse(call(R"([{"key":"admitted_variants","value":{"type":"NoneType","value":null}}])"),"membership");
  require(none.argument&&none.argument->is_null&&!o::missing_required(none)&&none.parameter_state=="omitted"&&none.raw_parameters.is_null,"missing versus present None collapsed");
  const auto explicit_none=o::parse(call(R"([{"key":"parameters","value":{"type":"NoneType","value":null}}])"),"auto");require(explicit_none.parameter_state=="null"&&o::invoked_arm(explicit_none)=="auto_raw_parameters","None parameters presence collapsed");
  const auto entries=o::parse(call(R"([{"key":"parameters","value":{"type":"dict","items":[[{"type":"enum","module":"pm4py.algo.filtering.polars.variants.variants_filter","name":"POSITIVE","value":{"type":"str","value":"positive"}},{"type":"bool","value":false}],[{"type":"str","value":"positive"},{"type":"bool","value":true}]]}},{"key":"k","value":{"type":"int","value":1}}])","DataFrame"),"top_k");
  require(entries.parameters&&entries.parameters->size()==2&&entries.parameters->at(0).enum_key&&!entries.parameters->at(1).enum_key&&entries.parameters->at(0).value.key.empty()&&entries.keyword_order==std::vector<std::string>{"parameters","k"},"actual parameter order/enum/value keys lost");
  const auto snapshot=o::shared::Reader(o::controls_json(entries)).read();require(snapshot.fields.size()==8&&o::at(snapshot,"parameters_passed").boolean&&!o::at(snapshot,"native_options_present").boolean,"actual controls not exact8");
  const auto missing_snapshot=o::shared::Reader(o::controls_json(missing)).read();require(!o::at(missing_snapshot,"parameters_passed").boolean&&o::at(o::at(missing_snapshot,"parameters"),"raw_json").text==o::cell_json(missing.raw_parameters),"unused genuine raw object absent from snapshot");
  auto changed=entries;changed.parameters->at(0).value.typed_value=true;require(!o::same_call(entries,changed)&&o::same_call(entries,entries),"actual caller object comparison ineffective");
  const auto exact=o::parse(std::string("\xef\xbb\xbf",3)+"\r\n"+call(R"([{"key":"k","value":{"type":"int","value":9007199254740993}}])")+"\r\n","top_k");require(exact.argument&&exact.argument->value=="9007199254740993","BOM number slice changed");
  EventTable source;source.columns={"x"};source.dtypes={"String"};source.rows={{EventAttribute{"x","v"}}};source.attributes={{"byte",std::string("\xff",1)}};EventTableIndex index;index.labels={{EventAttribute{"","label"}}};source.index=index;
  std::size_t scans=0;const LazyEventTable lazy(source.columns,[&](const LazyEventTable::RowConsumer& consume){++scans;for(const auto& row:source.rows)consume(row);},source.attributes,source.metadata,std::nullopt,source.dtypes,source.index,source.categorical_dtypes);
  const auto metadata=o::shared::Reader(o::returned_metadata(lazy)).read();auto header=source;header.rows.clear();
  require(scans==0&&metadata.fields.size()==4&&!o::at(metadata,"row_count_known").boolean&&o::at(metadata,"row_count").kind==o::Node::kNull,"header snapshot scanned or invented row count");
  const auto context=o::at(metadata,"table_context_json").text;require(context==serialize_embedding_workbench_table_json(header)&&context.find("event-table-raw-v1")!=std::string::npos,"whole raw header codec marker/fields lost");
  auto restored=parse_embedding_workbench_table_json(context);restored.rows=source.rows;require(serialize_embedding_workbench_table_json(restored)==serialize_embedding_workbench_table_json(source),"full indexed raw header context cannot restore source");
  const auto eager_metadata=o::shared::Reader(o::returned_metadata(source)).read();require(o::at(eager_metadata,"row_count_known").boolean&&o::at(eager_metadata,"row_count").kind==o::Node::kNumber,"eager known count missing");
  for(const auto& bad:std::vector<std::string>{R"({"type":"int","value":18446744073709551616})",R"({"type":"int","value":-9223372036854775809})",R"({"type":"int","value":1.0})",R"({"type":"float","hex":"0.1"})",R"({"type":"datetime","iso":"2026-06-01","fold":0,"offset_seconds":1e999})",R"({"type":"datetime","iso":"2026-06-01","fold":2,"offset_seconds":null})",R"({"type":"str","value":"a","\u0076alue":"b"})",R"({"type":"pandas.Timestamp","iso":"2026-06-01"})",R"({"type":"enum","module":"pm4py.algo.filtering.polars.variants.variants_filter","name":"POSITIVE","value":{"type":"str","value":"positive"}})"}){
    bool rejected=false;try{(void)value(bad);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"out of contract packed value accepted");
  }
}
}
int main(){try{check();std::cout<<"private Polars variants actual controls and complete headers passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}return 0;}
