#include "minera/pandas_attribute_repetition_filter.hpp"
#include "minera/embedding_workbench_json.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
using namespace minera;
namespace {
std::string bytes(std::string_view hex){const auto digit=[](char c){return c<='9'?c-'0':c-'a'+10;};assert(hex.size()%2==0);std::string result;for(std::size_t i=0;i<hex.size();i+=2)result.push_back(static_cast<char>(16*digit(hex[i])+digit(hex[i+1])));return result;}
EventAttribute null_value(std::string key){EventAttribute a{std::move(key),""};a.is_null=true;return a;}
EventAttribute nat_value(std::string key){EventAttribute a{std::move(key),"NaT",AttributeKind::kDate,true};a.typed_value=NativeDate{"NaT",NativeDateResolution::kNanosecond};return a;}
EventAttribute integer_value(std::string key,std::int64_t value){EventAttribute a{std::move(key),std::to_string(value),AttributeKind::kInt};a.typed_value=value;return a;}
EventAttribute float_value(std::string key,double value){EventAttribute a{std::move(key),std::to_string(value),AttributeKind::kFloat};a.typed_value=value;return a;}
EventAttribute boolean_value(std::string key,bool value){EventAttribute a{std::move(key),value?"true":"false",AttributeKind::kBoolean};a.typed_value=value;return a;}
EventAttribute date_value(std::string key,std::string value,std::optional<std::string> zone){EventAttribute a{std::move(key),value,AttributeKind::kDate};a.typed_value=NativeDate{value,NativeDateResolution::kNanosecond,std::move(zone)};return a;}
EventAttribute container_value(std::string key,AttributeContainerKind kind,std::vector<EventAttribute> children){EventAttribute a{std::move(key),""};a.container=kind;a.children=std::move(children);return a;}

EventTable snapshot_0(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL})});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL})});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL})});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6332")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL})});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL})});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL})});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL})});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d756e75736564")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("5a")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL})});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0),EventAttribute{bytes("62616e6b"),bytes("70616e6461732d6c69746572616c2d6d697373696e672d636f6c756d6e2d4232")}};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
void retained_pandas_repetition_cases(){std::size_t returned=0,errors=0;
{ // PDR-B2-literal-attribute-0
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6d697373696e67006e616d65");
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && e.message==bytes("276d697373696e675c7830306e616d6527");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a205044522d42322d6c69746572616c2d6174747269627574652d30"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // PDR-B2-literal-attribute-1
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6d697373696e670a6e616d65");
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && e.message==bytes("276d697373696e675c6e6e616d6527");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a205044522d42322d6c69746572616c2d6174747269627574652d31"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // PDR-B2-literal-attribute-2
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6d697373696e672771756f7465");
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && e.message==bytes("226d697373696e672771756f746522");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a205044522d42322d6c69746572616c2d6174747269627574652d32"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // PDR-B2-literal-attribute-3
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6d697373696e672271756f7465");
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && e.message==bytes("276d697373696e672271756f746527");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a205044522d42322d6c69746572616c2d6174747269627574652d33"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // PDR-B2-literal-attribute-4
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6d697373696e675c736c617368");
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && e.message==bytes("276d697373696e675c5c736c61736827");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a205044522d42322d6c69746572616c2d6174747269627574652d34"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // PDR-B2-literal-attribute-5
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("");
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && e.message==bytes("2727");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a205044522d42322d6c69746572616c2d6174747269627574652d35"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // PDR-B2-literal-attribute-6
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6d697373696e6727616e6422626f7468");
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && e.message==bytes("276d697373696e675c27616e6422626f746827");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a205044522d42322d6c69746572616c2d6174747269627574652d36"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // PDR-B2-literal-case-0
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.case_id_key=bytes("6d697373696e670063617365");
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && e.message==bytes("276d697373696e675c7830306361736527");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a205044522d42322d6c69746572616c2d636173652d30"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // PDR-B2-literal-case-1
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.case_id_key=bytes("6d697373696e670a63617365");
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && e.message==bytes("276d697373696e675c6e6361736527");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a205044522d42322d6c69746572616c2d636173652d31"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // PDR-B2-literal-case-2
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.case_id_key=bytes("6d697373696e675c63617365");
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && e.message==bytes("276d697373696e675c5c6361736527");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a205044522d42322d6c69746572616c2d636173652d32"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
assert(returned==0&&errors==10);std::cout<<returned<<"/0 full pandas repetition results and "<<errors<<"/10 literal missing-column diagnostics passed\n";}
} // namespace
int main(){retained_pandas_repetition_cases();}