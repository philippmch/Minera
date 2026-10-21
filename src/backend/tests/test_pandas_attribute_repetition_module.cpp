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

EventTable snapshot_0(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6332")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d756e75736564")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("5a")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_1(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_2(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_3(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_4(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_5(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d30")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d30")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d31")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d31")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d32")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d32")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d33")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},boolean_value(bytes("73656c6563746564"),false),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d33")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},boolean_value(bytes("73656c6563746564"),false),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d34")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{8LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d34")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{9LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d35")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{10LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d35")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{11LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d36")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{12LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d36")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{13LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d37")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),0x1.8000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{14LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d37")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),0x1.8000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{15LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d38")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{16LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d38")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{17LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d39")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},EventAttribute{bytes("73656c6563746564"),bytes("6e616e")},integer_value(bytes("6f7264696e616c"),std::int64_t{18LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d39")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},EventAttribute{bytes("73656c6563746564"),bytes("6e616e")},integer_value(bytes("6f7264696e616c"),std::int64_t{19LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3130")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},integer_value(bytes("73656c6563746564"),std::int64_t{9007199254740992LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{20LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3130")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},integer_value(bytes("73656c6563746564"),std::int64_t{9007199254740992LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{21LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3131")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},integer_value(bytes("73656c6563746564"),std::int64_t{9007199254740993LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{22LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3131")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},integer_value(bytes("73656c6563746564"),std::int64_t{9007199254740993LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{23LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3132")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+53),integer_value(bytes("6f7264696e616c"),std::int64_t{24LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3132")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+53),integer_value(bytes("6f7264696e616c"),std::int64_t{25LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3133")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::infinity()),integer_value(bytes("6f7264696e616c"),std::int64_t{26LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3133")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::infinity()),integer_value(bytes("6f7264696e616c"),std::int64_t{27LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3134")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),-std::numeric_limits<double>::infinity()),integer_value(bytes("6f7264696e616c"),std::int64_t{28LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3134")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),-std::numeric_limits<double>::infinity()),integer_value(bytes("6f7264696e616c"),std::int64_t{29LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3135")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},date_value(bytes("73656c6563746564"),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030"),std::optional<std::string>{bytes("555443")}),integer_value(bytes("6f7264696e616c"),std::int64_t{30LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3135")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},date_value(bytes("73656c6563746564"),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030"),std::optional<std::string>{bytes("555443")}),integer_value(bytes("6f7264696e616c"),std::int64_t{31LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3136")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},EventAttribute{bytes("73656c6563746564"),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030")},integer_value(bytes("6f7264696e616c"),std::int64_t{32LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3136")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},EventAttribute{bytes("73656c6563746564"),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030")},integer_value(bytes("6f7264696e616c"),std::int64_t{33LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3137")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},nat_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{34LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3137")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},nat_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{35LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3138")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},make_native_na_attribute(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{36LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3138")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},make_native_na_attribute(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{37LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{8LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{9LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{10LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{11LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{12LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{13LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{14LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{15LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{16LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{17LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{18LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{19LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{20LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{21LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{22LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{23LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{24LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{25LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{26LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{27LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{28LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{29LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{30LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{31LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{32LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{33LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{34LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{35LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{36LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{37LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_6(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d32")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d32")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d35")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{10LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d35")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{11LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d36")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{12LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d36")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{13LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{10LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{11LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{12LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{13LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_7(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d33")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},boolean_value(bytes("73656c6563746564"),false),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d33")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},boolean_value(bytes("73656c6563746564"),false),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d34")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{8LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d34")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{9LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{8LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{9LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_8(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d37")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),0x1.8000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{14LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d37")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),0x1.8000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{15LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{14LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{15LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_9(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d38")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{16LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d38")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{17LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{16LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{17LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_10(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d39")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},EventAttribute{bytes("73656c6563746564"),bytes("6e616e")},integer_value(bytes("6f7264696e616c"),std::int64_t{18LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d39")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},EventAttribute{bytes("73656c6563746564"),bytes("6e616e")},integer_value(bytes("6f7264696e616c"),std::int64_t{19LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{18LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{19LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_11(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3130")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},integer_value(bytes("73656c6563746564"),std::int64_t{9007199254740992LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{20LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3130")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},integer_value(bytes("73656c6563746564"),std::int64_t{9007199254740992LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{21LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3132")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+53),integer_value(bytes("6f7264696e616c"),std::int64_t{24LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3132")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+53),integer_value(bytes("6f7264696e616c"),std::int64_t{25LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{20LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{21LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{24LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{25LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_12(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3131")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},integer_value(bytes("73656c6563746564"),std::int64_t{9007199254740993LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{22LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3131")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},integer_value(bytes("73656c6563746564"),std::int64_t{9007199254740993LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{23LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{22LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{23LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_13(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3133")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::infinity()),integer_value(bytes("6f7264696e616c"),std::int64_t{26LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3133")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::infinity()),integer_value(bytes("6f7264696e616c"),std::int64_t{27LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{26LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{27LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_14(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3134")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),-std::numeric_limits<double>::infinity()),integer_value(bytes("6f7264696e616c"),std::int64_t{28LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3134")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},float_value(bytes("73656c6563746564"),-std::numeric_limits<double>::infinity()),integer_value(bytes("6f7264696e616c"),std::int64_t{29LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{28LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{29LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_15(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3135")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},date_value(bytes("73656c6563746564"),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030"),std::optional<std::string>{bytes("555443")}),integer_value(bytes("6f7264696e616c"),std::int64_t{30LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3135")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},date_value(bytes("73656c6563746564"),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030"),std::optional<std::string>{bytes("555443")}),integer_value(bytes("6f7264696e616c"),std::int64_t{31LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{30LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{31LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_16(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3136")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},EventAttribute{bytes("73656c6563746564"),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030")},integer_value(bytes("6f7264696e616c"),std::int64_t{32LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d74797065642d3136")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("7479706564206163746976697479")},EventAttribute{bytes("73656c6563746564"),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030")},integer_value(bytes("6f7264696e616c"),std::int64_t{33LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{32LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{33LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_17(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6332")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+1),integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d756e75736564")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("5a")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("666c6f61743634"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_18(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("666c6f61743634"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_19(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("666c6f61743634"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_20(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},make_native_na_attribute(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6332")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},integer_value(bytes("73656c6563746564"),std::int64_t{2LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d756e75736564")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("5a")},make_native_na_attribute(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("496e743634"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_21(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("496e743634"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_22(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},make_native_na_attribute(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("496e743634"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_23(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},make_native_na_attribute(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6332")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},boolean_value(bytes("73656c6563746564"),false),integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),false),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),false),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d756e75736564")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("5a")},make_native_na_attribute(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("626f6f6c65616e"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_24(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("626f6f6c65616e"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_25(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},make_native_na_attribute(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),false),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("626f6f6c65616e"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_26(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("41")},integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("41")},integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},make_native_na_attribute(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6332")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("42")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("41")},integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("")},integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("")},integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d756e75736564")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("5a")},make_native_na_attribute(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("737472696e67"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_27(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("737472696e67"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_28(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("41")},integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("41")},integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},make_native_na_attribute(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("41")},integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("")},integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("737472696e67"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_29(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("41")},integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("41")},integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6332")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("42")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("41")},integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("")},integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("")},integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d756e75736564")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("5a")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("63617465676f7279"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(EventTableCategoricalDtype{bytes("737472"),{EventAttribute{bytes(""),bytes("")},EventAttribute{bytes(""),bytes("41")},EventAttribute{bytes(""),bytes("42")},EventAttribute{bytes(""),bytes("756e75736564")}},true});
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_30(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("63617465676f7279"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(EventTableCategoricalDtype{bytes("737472"),{EventAttribute{bytes(""),bytes("")},EventAttribute{bytes(""),bytes("41")},EventAttribute{bytes(""),bytes("42")},EventAttribute{bytes(""),bytes("756e75736564")}},true});
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_31(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("41")},integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("41")},integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("41")},integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("")},integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("63617465676f7279"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(EventTableCategoricalDtype{bytes("737472"),{EventAttribute{bytes(""),bytes("")},EventAttribute{bytes(""),bytes("41")},EventAttribute{bytes(""),bytes("42")},EventAttribute{bytes(""),bytes("756e75736564")}},true});
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_32(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes(""),bytes("6177617265"),bytes("6e61697665"),bytes("6c69746572616c2d64617465")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")},nat_value(bytes("6177617265")),nat_value(bytes("6e61697665")),EventAttribute{bytes("6c69746572616c2d64617465"),bytes("4e6154")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")},date_value(bytes("6177617265"),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030"),std::optional<std::string>{bytes("555443")}),date_value(bytes("6e61697665"),bytes("323032362d30342d30345430303a30303a30302e303030303030303031"),std::nullopt),EventAttribute{bytes("6c69746572616c2d64617465"),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")},date_value(bytes("6177617265"),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030"),std::optional<std::string>{bytes("555443")}),date_value(bytes("6e61697665"),bytes("323032362d30342d30345430303a30303a30302e303030303030303031"),std::nullopt),EventAttribute{bytes("6c69746572616c2d64617465"),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6332")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")},date_value(bytes("6177617265"),bytes("323032362d30342d30345430303a30303a30302e3030303030303030322b30303a3030"),std::optional<std::string>{bytes("555443")}),date_value(bytes("6e61697665"),bytes("323032362d30342d30345430303a30303a30302e303030303030303032"),std::nullopt),EventAttribute{bytes("6c69746572616c2d64617465"),bytes("323032362d30342d30345430303a30303a30302e3030303030303030322b30303a3030")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")},nat_value(bytes("6177617265")),nat_value(bytes("6e61697665")),EventAttribute{bytes("6c69746572616c2d64617465"),bytes("4e6154")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")},date_value(bytes("6177617265"),bytes("323032362d30342d30345430303a30303a30302b30303a3030"),std::optional<std::string>{bytes("555443")}),date_value(bytes("6e61697665"),bytes("323032362d30342d30345430303a30303a3030"),std::nullopt),EventAttribute{bytes("6c69746572616c2d64617465"),bytes("323032362d30342d30345430303a30303a30302b30303a3030")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")},nat_value(bytes("6177617265")),nat_value(bytes("6e61697665")),EventAttribute{bytes("6c69746572616c2d64617465"),bytes("4e6154")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d756e75736564")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("5a")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")},nat_value(bytes("6177617265")),nat_value(bytes("6e61697665")),EventAttribute{bytes("6c69746572616c2d64617465"),bytes("4e6154")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374"),bytes("6461746574696d6536345b6e732c205554435d"),bytes("6461746574696d6536345b6e735d"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_33(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes(""),bytes("6177617265"),bytes("6e61697665"),bytes("6c69746572616c2d64617465")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")},nat_value(bytes("6177617265")),nat_value(bytes("6e61697665")),EventAttribute{bytes("6c69746572616c2d64617465"),bytes("4e6154")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")},date_value(bytes("6177617265"),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030"),std::optional<std::string>{bytes("555443")}),date_value(bytes("6e61697665"),bytes("323032362d30342d30345430303a30303a30302e303030303030303031"),std::nullopt),EventAttribute{bytes("6c69746572616c2d64617465"),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")},date_value(bytes("6177617265"),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030"),std::optional<std::string>{bytes("555443")}),date_value(bytes("6e61697665"),bytes("323032362d30342d30345430303a30303a30302e303030303030303031"),std::nullopt),EventAttribute{bytes("6c69746572616c2d64617465"),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")},nat_value(bytes("6177617265")),nat_value(bytes("6e61697665")),EventAttribute{bytes("6c69746572616c2d64617465"),bytes("4e6154")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")},nat_value(bytes("6177617265")),nat_value(bytes("6e61697665")),EventAttribute{bytes("6c69746572616c2d64617465"),bytes("4e6154")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374"),bytes("6461746574696d6536345b6e732c205554435d"),bytes("6461746574696d6536345b6e735d"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_34(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes(""),bytes("6177617265"),bytes("6e61697665"),bytes("6c69746572616c2d64617465")};
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374"),bytes("6461746574696d6536345b6e732c205554435d"),bytes("6461746574696d6536345b6e735d"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_35(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({null_value(bytes("636173653a636f6e636570743a6e616d65")),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({float_value(bytes("636173653a636f6e636570743a6e616d65"),std::numeric_limits<double>::quiet_NaN()),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({make_native_na_attribute(bytes("636173653a636f6e636570743a6e616d65")),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("78")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({null_value(bytes("636173653a636f6e636570743a6e616d65")),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("78")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({float_value(bytes("636173653a636f6e636570743a6e616d65"),std::numeric_limits<double>::quiet_NaN()),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("756e75736564")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("5a")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_36(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("78")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("78")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_37(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({boolean_value(bytes("636173653a636f6e636570743a6e616d65"),true),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({integer_value(bytes("636173653a636f6e636570743a6e616d65"),std::int64_t{1LL}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({float_value(bytes("636173653a636f6e636570743a6e616d65"),0x1.0000000000000p+0),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("31")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({integer_value(bytes("636173653a636f6e636570743a6e616d65"),std::int64_t{0LL}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({boolean_value(bytes("636173653a636f6e636570743a6e616d65"),false),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({float_value(bytes("636173653a636f6e636570743a6e616d65"),-0x0.0p+0),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("756e75736564")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("5a")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_38(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({boolean_value(bytes("636173653a636f6e636570743a6e616d65"),true),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({integer_value(bytes("636173653a636f6e636570743a6e616d65"),std::int64_t{1LL}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({float_value(bytes("636173653a636f6e636570743a6e616d65"),0x1.0000000000000p+0),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({integer_value(bytes("636173653a636f6e636570743a6e616d65"),std::int64_t{0LL}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({boolean_value(bytes("636173653a636f6e636570743a6e616d65"),false),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({float_value(bytes("636173653a636f6e636570743a6e616d65"),-0x0.0p+0),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_39(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({date_value(bytes("636173653a636f6e636570743a6e616d65"),bytes("323032362d30342d30345430303a30303a30302b30303a3030"),std::optional<std::string>{bytes("555443")}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({date_value(bytes("636173653a636f6e636570743a6e616d65"),bytes("323032362d30342d30345430303a30303a30302b30303a3030"),std::optional<std::string>{bytes("555443")}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({date_value(bytes("636173653a636f6e636570743a6e616d65"),bytes("323032362d30342d30345430303a30303a30302b30303a3030"),std::optional<std::string>{bytes("555443")}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({date_value(bytes("636173653a636f6e636570743a6e616d65"),bytes("323032362d30342d30345430303a30303a30302b30303a3030"),std::optional<std::string>{bytes("555443")}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({date_value(bytes("636173653a636f6e636570743a6e616d65"),bytes("323032362d30342d30345430303a30303a30302b30303a3030"),std::optional<std::string>{bytes("555443")}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({date_value(bytes("636173653a636f6e636570743a6e616d65"),bytes("323032362d30342d30345430303a30303a30302b30303a3030"),std::optional<std::string>{bytes("555443")}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({date_value(bytes("636173653a636f6e636570743a6e616d65"),bytes("323032362d30342d30345430303a30303a30302b30303a3030"),std::optional<std::string>{bytes("555443")}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({nat_value(bytes("636173653a636f6e636570743a6e616d65")),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("5a")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6461746574696d6536345b6e732c205554435d"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_40(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({date_value(bytes("636173653a636f6e636570743a6e616d65"),bytes("323032362d30342d30345430303a30303a30302b30303a3030"),std::optional<std::string>{bytes("555443")}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({date_value(bytes("636173653a636f6e636570743a6e616d65"),bytes("323032362d30342d30345430303a30303a30302b30303a3030"),std::optional<std::string>{bytes("555443")}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({date_value(bytes("636173653a636f6e636570743a6e616d65"),bytes("323032362d30342d30345430303a30303a30302b30303a3030"),std::optional<std::string>{bytes("555443")}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({date_value(bytes("636173653a636f6e636570743a6e616d65"),bytes("323032362d30342d30345430303a30303a30302b30303a3030"),std::optional<std::string>{bytes("555443")}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({date_value(bytes("636173653a636f6e636570743a6e616d65"),bytes("323032362d30342d30345430303a30303a30302b30303a3030"),std::optional<std::string>{bytes("555443")}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({date_value(bytes("636173653a636f6e636570743a6e616d65"),bytes("323032362d30342d30345430303a30303a30302b30303a3030"),std::optional<std::string>{bytes("555443")}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({date_value(bytes("636173653a636f6e636570743a6e616d65"),bytes("323032362d30342d30345430303a30303a30302b30303a3030"),std::optional<std::string>{bytes("555443")}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6461746574696d6536345b6e732c205554435d"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_41(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6332")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d756e75736564")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("5a")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("63617465676f7279"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(EventTableCategoricalDtype{bytes("737472"),{EventAttribute{bytes(""),bytes("5044522d42312d6330")},EventAttribute{bytes(""),bytes("5044522d42312d6331")},EventAttribute{bytes(""),bytes("5044522d42312d6332")},EventAttribute{bytes(""),bytes("5044522d42312d6333")},EventAttribute{bytes(""),bytes("5044522d42312d756e75736564")},EventAttribute{bytes(""),bytes("756e6f62736572766564")}},true});
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_42(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("63617465676f7279"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})}}};
table.categorical_dtypes.push_back(EventTableCategoricalDtype{bytes("737472"),{EventAttribute{bytes(""),bytes("5044522d42312d6330")},EventAttribute{bytes(""),bytes("5044522d42312d6331")},EventAttribute{bytes(""),bytes("5044522d42312d6332")},EventAttribute{bytes(""),bytes("5044522d42312d6333")},EventAttribute{bytes(""),bytes("5044522d42312d756e75736564")},EventAttribute{bytes(""),bytes("756e6f62736572766564")}},true});
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_43(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({container_value(bytes("636173653a636f6e636570743a6e616d65"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{0LL})}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({container_value(bytes("636173653a636f6e636570743a6e616d65"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL})}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({container_value(bytes("636173653a636f6e636570743a6e616d65"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{0LL})}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({container_value(bytes("636173653a636f6e636570743a6e616d65"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL})}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({container_value(bytes("636173653a636f6e636570743a6e616d65"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{0LL})}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({container_value(bytes("636173653a636f6e636570743a6e616d65"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL})}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({container_value(bytes("636173653a636f6e636570743a6e616d65"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{0LL})}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({container_value(bytes("636173653a636f6e636570743a6e616d65"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL})}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("5a")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_44(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({container_value(bytes("636173653a636f6e636570743a6e616d65"),AttributeContainerKind::kContainer,{integer_value(bytes("78"),std::int64_t{0LL})}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({container_value(bytes("636173653a636f6e636570743a6e616d65"),AttributeContainerKind::kContainer,{integer_value(bytes("78"),std::int64_t{1LL})}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({container_value(bytes("636173653a636f6e636570743a6e616d65"),AttributeContainerKind::kContainer,{integer_value(bytes("78"),std::int64_t{0LL})}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({container_value(bytes("636173653a636f6e636570743a6e616d65"),AttributeContainerKind::kContainer,{integer_value(bytes("78"),std::int64_t{1LL})}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({container_value(bytes("636173653a636f6e636570743a6e616d65"),AttributeContainerKind::kContainer,{integer_value(bytes("78"),std::int64_t{0LL})}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({container_value(bytes("636173653a636f6e636570743a6e616d65"),AttributeContainerKind::kContainer,{integer_value(bytes("78"),std::int64_t{1LL})}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({container_value(bytes("636173653a636f6e636570743a6e616d65"),AttributeContainerKind::kContainer,{integer_value(bytes("78"),std::int64_t{0LL})}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({container_value(bytes("636173653a636f6e636570743a6e616d65"),AttributeContainerKind::kContainer,{integer_value(bytes("78"),std::int64_t{1LL})}),EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("5a")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_45(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("616273656e74")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("616273656e74")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("616273656e74")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6332")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("616273656e74")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("616273656e74")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("616273656e74")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("616273656e74")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d756e75736564")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("616273656e74")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("737472"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_46(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("737472"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_47(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6332")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d756e75736564")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("737472"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_48(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d756e75736564")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("5a")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6332")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_49(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_50(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6332")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d756e75736564")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("5a")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("496e646578"),bytes("696e743634"),{std::optional<std::string>{bytes("726f77206c6162656c")}},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{9LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_51(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("496e646578"),bytes("696e743634"),{std::optional<std::string>{bytes("726f77206c6162656c")}},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_52(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6332")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d756e75736564")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("5a")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("4d756c7469496e646578"),bytes("6f626a656374"),{std::optional<std::string>{bytes("706172746974696f6e")},std::optional<std::string>{bytes("706f736974696f6e")}},{EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{bytes(""),bytes("67726f7570")}},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})}}},EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{bytes(""),bytes("67726f7570")}},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})}}},EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{bytes(""),bytes("67726f7570")}},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})}}},EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{bytes(""),bytes("67726f7570")}},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})}}},EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{bytes(""),bytes("67726f7570")}},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})}}},EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{bytes(""),bytes("67726f7570")}},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})}}},EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{bytes(""),bytes("67726f7570")}},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})}}},EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{bytes(""),bytes("67726f7570")}},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})}}}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_53(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("4d756c7469496e646578"),bytes("6f626a656374"),{std::optional<std::string>{bytes("706172746974696f6e")},std::optional<std::string>{bytes("706f736974696f6e")}},{EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{bytes(""),bytes("67726f7570")}},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})}}},EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{bytes(""),bytes("67726f7570")}},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})}}},EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{bytes(""),bytes("67726f7570")}},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})}}},EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{bytes(""),bytes("67726f7570")}},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})}}},EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{bytes(""),bytes("67726f7570")}},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})}}},EventTableIndexLabel{std::vector<EventTableIndexLabel>{EventTableIndexLabel{EventAttribute{bytes(""),bytes("67726f7570")}},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})}}}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_54(){EventTable table;table.columns={bytes("206c69746572616c206361736520"),bytes("206c69746572616c20616374697669747920"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("206c69746572616c206361736520"),bytes("5044522d42312d6330")},EventAttribute{bytes("206c69746572616c20616374697669747920"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("206c69746572616c206361736520"),bytes("5044522d42312d6331")},EventAttribute{bytes("206c69746572616c20616374697669747920"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("206c69746572616c206361736520"),bytes("5044522d42312d6330")},EventAttribute{bytes("206c69746572616c20616374697669747920"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("206c69746572616c206361736520"),bytes("5044522d42312d6332")},EventAttribute{bytes("206c69746572616c20616374697669747920"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("206c69746572616c206361736520"),bytes("5044522d42312d6331")},EventAttribute{bytes("206c69746572616c20616374697669747920"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("206c69746572616c206361736520"),bytes("5044522d42312d6333")},EventAttribute{bytes("206c69746572616c20616374697669747920"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("206c69746572616c206361736520"),bytes("5044522d42312d6330")},EventAttribute{bytes("206c69746572616c20616374697669747920"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("206c69746572616c206361736520"),bytes("5044522d42312d756e75736564")},EventAttribute{bytes("206c69746572616c20616374697669747920"),bytes("5a")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_55(){EventTable table;table.columns={bytes("206c69746572616c206361736520"),bytes("206c69746572616c20616374697669747920"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("206c69746572616c206361736520"),bytes("5044522d42312d6330")},EventAttribute{bytes("206c69746572616c20616374697669747920"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("206c69746572616c206361736520"),bytes("5044522d42312d6331")},EventAttribute{bytes("206c69746572616c20616374697669747920"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("206c69746572616c206361736520"),bytes("5044522d42312d6330")},EventAttribute{bytes("206c69746572616c20616374697669747920"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("206c69746572616c206361736520"),bytes("5044522d42312d6331")},EventAttribute{bytes("206c69746572616c20616374697669747920"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("206c69746572616c206361736520"),bytes("5044522d42312d6333")},EventAttribute{bytes("206c69746572616c20616374697669747920"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("206c69746572616c206361736520"),bytes("5044522d42312d6330")},EventAttribute{bytes("206c69746572616c20616374697669747920"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_56(){EventTable table;table.columns={bytes(""),bytes("206c69746572616c2076616c756520"),bytes("73656c6563746564"),bytes("6f7264696e616c")};
table.rows.push_back({EventAttribute{bytes(""),bytes("5044522d42312d6330")},EventAttribute{bytes("206c69746572616c2076616c756520"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL})});
table.rows.push_back({EventAttribute{bytes(""),bytes("5044522d42312d6331")},EventAttribute{bytes("206c69746572616c2076616c756520"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL})});
table.rows.push_back({EventAttribute{bytes(""),bytes("5044522d42312d6330")},EventAttribute{bytes("206c69746572616c2076616c756520"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL})});
table.rows.push_back({EventAttribute{bytes(""),bytes("5044522d42312d6332")},EventAttribute{bytes("206c69746572616c2076616c756520"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL})});
table.rows.push_back({EventAttribute{bytes(""),bytes("5044522d42312d6331")},EventAttribute{bytes("206c69746572616c2076616c756520"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL})});
table.rows.push_back({EventAttribute{bytes(""),bytes("5044522d42312d6333")},EventAttribute{bytes("206c69746572616c2076616c756520"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL})});
table.rows.push_back({EventAttribute{bytes(""),bytes("5044522d42312d6330")},EventAttribute{bytes("206c69746572616c2076616c756520"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL})});
table.rows.push_back({EventAttribute{bytes(""),bytes("5044522d42312d756e75736564")},EventAttribute{bytes("206c69746572616c2076616c756520"),bytes("5a")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL})});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_57(){EventTable table;table.columns={bytes(""),bytes("206c69746572616c2076616c756520"),bytes("73656c6563746564"),bytes("6f7264696e616c")};
table.rows.push_back({EventAttribute{bytes(""),bytes("5044522d42312d6330")},EventAttribute{bytes("206c69746572616c2076616c756520"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL})});
table.rows.push_back({EventAttribute{bytes(""),bytes("5044522d42312d6331")},EventAttribute{bytes("206c69746572616c2076616c756520"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL})});
table.rows.push_back({EventAttribute{bytes(""),bytes("5044522d42312d6330")},EventAttribute{bytes("206c69746572616c2076616c756520"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL})});
table.rows.push_back({EventAttribute{bytes(""),bytes("5044522d42312d6331")},EventAttribute{bytes("206c69746572616c2076616c756520"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL})});
table.rows.push_back({EventAttribute{bytes(""),bytes("5044522d42312d6333")},EventAttribute{bytes("206c69746572616c2076616c756520"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL})});
table.rows.push_back({EventAttribute{bytes(""),bytes("5044522d42312d6330")},EventAttribute{bytes("206c69746572616c2076616c756520"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL})});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634")};
table.index=EventTableIndex{bytes("496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_58(){EventTable table;table.columns={bytes("20636173650acea90020"),bytes("2076616c75650d0acea90020"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("20636173650acea90020"),bytes("5044522d42312d6330")},EventAttribute{bytes("2076616c75650d0acea90020"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("20636173650acea90020"),bytes("5044522d42312d6331")},EventAttribute{bytes("2076616c75650d0acea90020"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("20636173650acea90020"),bytes("5044522d42312d6330")},EventAttribute{bytes("2076616c75650d0acea90020"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("20636173650acea90020"),bytes("5044522d42312d6332")},EventAttribute{bytes("2076616c75650d0acea90020"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("20636173650acea90020"),bytes("5044522d42312d6331")},EventAttribute{bytes("2076616c75650d0acea90020"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("20636173650acea90020"),bytes("5044522d42312d6333")},EventAttribute{bytes("2076616c75650d0acea90020"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("20636173650acea90020"),bytes("5044522d42312d6330")},EventAttribute{bytes("2076616c75650d0acea90020"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("20636173650acea90020"),bytes("5044522d42312d756e75736564")},EventAttribute{bytes("2076616c75650d0acea90020"),bytes("5a")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_59(){EventTable table;table.columns={bytes("20636173650acea90020"),bytes("2076616c75650d0acea90020"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("20636173650acea90020"),bytes("5044522d42312d6330")},EventAttribute{bytes("2076616c75650d0acea90020"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("20636173650acea90020"),bytes("5044522d42312d6331")},EventAttribute{bytes("2076616c75650d0acea90020"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("20636173650acea90020"),bytes("5044522d42312d6330")},EventAttribute{bytes("2076616c75650d0acea90020"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("20636173650acea90020"),bytes("5044522d42312d6331")},EventAttribute{bytes("2076616c75650d0acea90020"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("20636173650acea90020"),bytes("5044522d42312d6333")},EventAttribute{bytes("2076616c75650d0acea90020"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("20636173650acea90020"),bytes("5044522d42312d6330")},EventAttribute{bytes("2076616c75650d0acea90020"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_60(){EventTable table;table.columns={bytes("636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("42")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("41")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636f6e636570743a6e616d65"),bytes("5a")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_61(){EventTable table;table.columns={bytes("636173653a636f6e636570743a6e616d65"),bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6332")},EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6331")},null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6333")},float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d6330")},integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("636173653a636f6e636570743a6e616d65"),bytes("5044522d42312d756e75736564")},float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
EventTable snapshot_62(){EventTable table;table.columns={bytes("73656c6563746564"),bytes("6f7264696e616c"),bytes("")};
table.rows.push_back({boolean_value(bytes("73656c6563746564"),true),integer_value(bytes("6f7264696e616c"),std::int64_t{0LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({integer_value(bytes("73656c6563746564"),std::int64_t{1LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{1LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({float_value(bytes("73656c6563746564"),0x1.0000000000000p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{2LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({EventAttribute{bytes("73656c6563746564"),bytes("31")},integer_value(bytes("6f7264696e616c"),std::int64_t{3LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({null_value(bytes("73656c6563746564")),integer_value(bytes("6f7264696e616c"),std::int64_t{4LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({float_value(bytes("73656c6563746564"),std::numeric_limits<double>::quiet_NaN()),integer_value(bytes("6f7264696e616c"),std::int64_t{5LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({integer_value(bytes("73656c6563746564"),std::int64_t{0LL}),integer_value(bytes("6f7264696e616c"),std::int64_t{6LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.rows.push_back({float_value(bytes("73656c6563746564"),-0x0.0p+0),integer_value(bytes("6f7264696e616c"),std::int64_t{7LL}),EventAttribute{bytes(""),bytes("6c69746572616c20656d707479")}});
table.attributes={EventAttribute{bytes("6c69746572616c"),bytes("206f726967696e616c20cea90a0020")},container_value(bytes("6e6573746564"),AttributeContainerKind::kContainer,{container_value(bytes("61"),AttributeContainerKind::kList,{integer_value(bytes("30"),std::int64_t{1LL}),null_value(bytes("31")),boolean_value(bytes("32"),false)})}),integer_value(bytes("6c6172676520696e7465676572"),std::int64_t{9007199254740993LL}),float_value(bytes("7a65726f"),-0x0.0p+0)};
table.dtypes={bytes("6f626a656374"),bytes("696e743634"),bytes("6f626a656374")};
table.index=EventTableIndex{bytes("52616e6765496e646578"),bytes("696e743634"),{std::nullopt},{EventTableIndexLabel{integer_value(bytes(""),std::int64_t{0LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{1LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{2LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{3LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{4LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{5LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{6LL})},EventTableIndexLabel{integer_value(bytes(""),std::int64_t{7LL})}}};
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
table.categorical_dtypes.push_back(std::nullopt);
return table;}
void retained_pandas_repetition_cases(){std::size_t returned=0,errors=0;
{ // default-omitted
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_1();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064656661756c742d6f6d6974746564"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // default-null-parameters
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_1();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064656661756c742d6e756c6c2d706172616d6574657273"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // default-empty-parameters
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_1();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064656661756c742d656d7074792d706172616d6574657273"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // enum-bounds
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("31"));
options.maximum=TokenInteger(bytes("32"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_2();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a20656e756d2d626f756e6473"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // enum-precedence
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("31"));
options.maximum=TokenInteger(bytes("32"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_2();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a20656e756d2d707265636564656e6365"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // irrelevant
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_1();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a20697272656c6576616e74"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // lower--3
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("2d33"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_2();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206c6f7765722d2d33"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // upper--3
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
options.maximum=TokenInteger(bytes("2d33"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2075707065722d2d33"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // lower--1
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("2d31"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_2();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206c6f7765722d2d31"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // upper--1
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
options.maximum=TokenInteger(bytes("2d31"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2075707065722d2d31"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // lower-0
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_2();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206c6f7765722d30"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // upper-0
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
options.maximum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2075707065722d30"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // lower-1
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("31"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_2();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206c6f7765722d31"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // upper-1
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
options.maximum=TokenInteger(bytes("31"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_4();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2075707065722d31"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // lower-2
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("32"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_1();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206c6f7765722d32"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // upper-2
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
options.maximum=TokenInteger(bytes("32"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_2();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2075707065722d32"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // lower-3
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("33"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206c6f7765722d33"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // upper-3
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
options.maximum=TokenInteger(bytes("33"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_2();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2075707065722d33"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // lower-8
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("38"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206c6f7765722d38"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // upper-8
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
options.maximum=TokenInteger(bytes("38"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_2();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2075707065722d38"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // lower-1.5
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=0x1.8000000000000p+0;
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_1();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206c6f7765722d312e35"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // upper-1.5
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
options.maximum=0x1.8000000000000p+0;
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_4();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2075707065722d312e35"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // lower-2.5
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=0x1.4000000000000p+1;
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206c6f7765722d322e35"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // upper-2.5
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
options.maximum=0x1.4000000000000p+1;
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_2();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2075707065722d322e35"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // lower-true
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=true;
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_2();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206c6f7765722d74727565"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // upper-true
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
options.maximum=true;
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_4();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2075707065722d74727565"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // lower-false
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=false;
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_2();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206c6f7765722d66616c7365"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // upper-false
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
options.maximum=false;
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2075707065722d66616c7365"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // lower-{"$float": "NaN"}
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=std::numeric_limits<double>::quiet_NaN();
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206c6f7765722d7b2224666c6f6174223a20224e614e227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // upper-{"$float": "NaN"}
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
options.maximum=std::numeric_limits<double>::quiet_NaN();
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2075707065722d7b2224666c6f6174223a20224e614e227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // lower-{"$float": "INF"}
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=std::numeric_limits<double>::infinity();
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206c6f7765722d7b2224666c6f6174223a2022494e46227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // upper-{"$float": "INF"}
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
options.maximum=std::numeric_limits<double>::infinity();
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_2();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2075707065722d7b2224666c6f6174223a2022494e46227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // lower-{"$float": "-INF"}
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=-std::numeric_limits<double>::infinity();
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_2();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206c6f7765722d7b2224666c6f6174223a20222d494e46227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // upper-{"$float": "-INF"}
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
options.maximum=-std::numeric_limits<double>::infinity();
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2075707065722d7b2224666c6f6174223a20222d494e46227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // lower-9007199254740993
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("39303037313939323534373430393933"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206c6f7765722d39303037313939323534373430393933"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // upper-9007199254740993
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
options.maximum=TokenInteger(bytes("39303037313939323534373430393933"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_2();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2075707065722d39303037313939323534373430393933"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // lower-null
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=nullptr;
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206c6f7765722d6e756c6c"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // upper-null
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
options.maximum=nullptr;
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2075707065722d6e756c6c"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // lower-"2"
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=bytes("32");
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("547970654572726f72") && std::string(e.what())==bytes("496e76616c696420636f6d70617269736f6e206265747765656e2064747970653d696e74363420616e6420737472");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a206c6f7765722d223222"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // upper-"2"
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
options.maximum=bytes("32");
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("547970654572726f72") && std::string(e.what())==bytes("496e76616c696420636f6d70617269736f6e206265747765656e2064747970653d696e74363420616e6420737472");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a2075707065722d223222"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // closed-range--5-0
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("2d35"));
options.maximum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a20636c6f7365642d72616e67652d2d352d30"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // closed-range-2-2
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("32"));
options.maximum=TokenInteger(bytes("32"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_1();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a20636c6f7365642d72616e67652d322d32"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // closed-range-3-1
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("33"));
options.maximum=TokenInteger(bytes("31"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a20636c6f7365642d72616e67652d332d31"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // closed-range-0.5-1.5
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=0x1.0000000000000p-1;
options.maximum=0x1.8000000000000p+0;
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_4();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a20636c6f7365642d72616e67652d302e352d312e35"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // closed-range--1--1
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("2d31"));
options.maximum=TokenInteger(bytes("2d31"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a20636c6f7365642d72616e67652d2d312d2d31"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-0
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=null_value(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d30"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-1
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=float_value(bytes(""),std::numeric_limits<double>::quiet_NaN());PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d31"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-2
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=boolean_value(bytes(""),true);PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_6();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d32"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-3
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=boolean_value(bytes(""),false);PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_7();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d33"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-4
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=integer_value(bytes(""),std::int64_t{0LL});PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_7();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d34"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-5
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=integer_value(bytes(""),std::int64_t{1LL});PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_6();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d35"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-6
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=float_value(bytes(""),0x1.0000000000000p+0);PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_6();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d36"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-7
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=float_value(bytes(""),0x1.8000000000000p+0);PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_8();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d37"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-8
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("31")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_9();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d38"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-9
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("6e616e")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_10();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d39"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-10
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=integer_value(bytes(""),std::int64_t{9007199254740992LL});PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_11();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d3130"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-11
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=integer_value(bytes(""),std::int64_t{9007199254740993LL});PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_12();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d3131"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-12
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=float_value(bytes(""),0x1.0000000000000p+53);PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_11();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d3132"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-13
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=float_value(bytes(""),std::numeric_limits<double>::infinity());PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_13();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d3133"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-14
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=float_value(bytes(""),-std::numeric_limits<double>::infinity());PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_14();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d3134"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-15
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=date_value(bytes(""),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030"),std::optional<std::string>{bytes("555443")});PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_15();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d3135"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-16
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_16();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d3136"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-17
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=nat_value(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d3137"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // typed-target-18
const auto input=snapshot_5();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=make_native_na_attribute(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2074797065642d7461726765742d3138"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-float-null
const auto input=snapshot_17();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=null_value(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_18();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d666c6f61742d6e756c6c"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-float-{"$float": "NaN"}
const auto input=snapshot_17();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=float_value(bytes(""),std::numeric_limits<double>::quiet_NaN());PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_18();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d666c6f61742d7b2224666c6f6174223a20224e614e227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-float-{"$na": true}
const auto input=snapshot_17();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=make_native_na_attribute(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_18();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d666c6f61742d7b22246e61223a20747275657d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-float-1
const auto input=snapshot_17();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=integer_value(bytes(""),std::int64_t{1LL});PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_19();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d666c6f61742d31"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-float-"A"
const auto input=snapshot_17();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_18();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d666c6f61742d224122"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-nullable-integer-null
const auto input=snapshot_20();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=null_value(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_21();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d6e756c6c61626c652d696e74656765722d6e756c6c"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-nullable-integer-{"$float": "NaN"}
const auto input=snapshot_20();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=float_value(bytes(""),std::numeric_limits<double>::quiet_NaN());PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_21();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d6e756c6c61626c652d696e74656765722d7b2224666c6f6174223a20224e614e227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-nullable-integer-{"$na": true}
const auto input=snapshot_20();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=make_native_na_attribute(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_21();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d6e756c6c61626c652d696e74656765722d7b22246e61223a20747275657d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-nullable-integer-1
const auto input=snapshot_20();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=integer_value(bytes(""),std::int64_t{1LL});PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_22();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d6e756c6c61626c652d696e74656765722d31"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-nullable-integer-"A"
const auto input=snapshot_20();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_21();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d6e756c6c61626c652d696e74656765722d224122"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-nullable-boolean-null
const auto input=snapshot_23();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=null_value(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_24();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d6e756c6c61626c652d626f6f6c65616e2d6e756c6c"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-nullable-boolean-{"$float": "NaN"}
const auto input=snapshot_23();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=float_value(bytes(""),std::numeric_limits<double>::quiet_NaN());PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_24();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d6e756c6c61626c652d626f6f6c65616e2d7b2224666c6f6174223a20224e614e227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-nullable-boolean-{"$na": true}
const auto input=snapshot_23();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=make_native_na_attribute(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_24();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d6e756c6c61626c652d626f6f6c65616e2d7b22246e61223a20747275657d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-nullable-boolean-1
const auto input=snapshot_23();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=integer_value(bytes(""),std::int64_t{1LL});PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_25();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d6e756c6c61626c652d626f6f6c65616e2d31"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-nullable-boolean-"A"
const auto input=snapshot_23();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_24();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d6e756c6c61626c652d626f6f6c65616e2d224122"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-nullable-string-null
const auto input=snapshot_26();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=null_value(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_27();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d6e756c6c61626c652d737472696e672d6e756c6c"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-nullable-string-{"$float": "NaN"}
const auto input=snapshot_26();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=float_value(bytes(""),std::numeric_limits<double>::quiet_NaN());PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_27();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d6e756c6c61626c652d737472696e672d7b2224666c6f6174223a20224e614e227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-nullable-string-{"$na": true}
const auto input=snapshot_26();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=make_native_na_attribute(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_27();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d6e756c6c61626c652d737472696e672d7b22246e61223a20747275657d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-nullable-string-1
const auto input=snapshot_26();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=integer_value(bytes(""),std::int64_t{1LL});PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_27();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d6e756c6c61626c652d737472696e672d31"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-nullable-string-"A"
const auto input=snapshot_26();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_28();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d6e756c6c61626c652d737472696e672d224122"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-category-null
const auto input=snapshot_29();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=null_value(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_30();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d63617465676f72792d6e756c6c"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-category-{"$float": "NaN"}
const auto input=snapshot_29();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=float_value(bytes(""),std::numeric_limits<double>::quiet_NaN());PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_30();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d63617465676f72792d7b2224666c6f6174223a20224e614e227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-category-{"$na": true}
const auto input=snapshot_29();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=make_native_na_attribute(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_30();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d63617465676f72792d7b22246e61223a20747275657d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-category-1
const auto input=snapshot_29();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=integer_value(bytes(""),std::int64_t{1LL});PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_30();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d63617465676f72792d31"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // dtype-category-"A"
const auto input=snapshot_29();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("73656c6563746564");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_31();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2064747970652d63617465676f72792d224122"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // datetime-aware-{"$timestamp": "2026-04-04T00:00:00.000000001Z"}
const auto input=snapshot_32();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=date_value(bytes(""),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030"),std::optional<std::string>{bytes("555443")});PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6177617265");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_33();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206461746574696d652d61776172652d7b222474696d657374616d70223a2022323032362d30342d30345430303a30303a30302e3030303030303030315a227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // datetime-aware-{"$timestamp": "2026-04-04T00:00:00.000000001"}
const auto input=snapshot_32();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=date_value(bytes(""),bytes("323032362d30342d30345430303a30303a30302e303030303030303031"),std::nullopt);PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6177617265");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_34();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206461746574696d652d61776172652d7b222474696d657374616d70223a2022323032362d30342d30345430303a30303a30302e303030303030303031227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // datetime-aware-{"$nat": true}
const auto input=snapshot_32();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=nat_value(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6177617265");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_34();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206461746574696d652d61776172652d7b22246e6174223a20747275657d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // datetime-aware-null
const auto input=snapshot_32();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=null_value(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6177617265");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_34();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206461746574696d652d61776172652d6e756c6c"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // datetime-aware-{"$float": "NaN"}
const auto input=snapshot_32();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=float_value(bytes(""),std::numeric_limits<double>::quiet_NaN());PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6177617265");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_34();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206461746574696d652d61776172652d7b2224666c6f6174223a20224e614e227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // datetime-aware-"2026-04-04T00:00:00.000000001+00:00"
const auto input=snapshot_32();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6177617265");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_33();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206461746574696d652d61776172652d22323032362d30342d30345430303a30303a30302e3030303030303030312b30303a303022"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // datetime-naive-{"$timestamp": "2026-04-04T00:00:00.000000001Z"}
const auto input=snapshot_32();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=date_value(bytes(""),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030"),std::optional<std::string>{bytes("555443")});PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6e61697665");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_34();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206461746574696d652d6e616976652d7b222474696d657374616d70223a2022323032362d30342d30345430303a30303a30302e3030303030303030315a227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // datetime-naive-{"$timestamp": "2026-04-04T00:00:00.000000001"}
const auto input=snapshot_32();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=date_value(bytes(""),bytes("323032362d30342d30345430303a30303a30302e303030303030303031"),std::nullopt);PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6e61697665");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_33();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206461746574696d652d6e616976652d7b222474696d657374616d70223a2022323032362d30342d30345430303a30303a30302e303030303030303031227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // datetime-naive-{"$nat": true}
const auto input=snapshot_32();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=nat_value(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6e61697665");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_34();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206461746574696d652d6e616976652d7b22246e6174223a20747275657d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // datetime-naive-null
const auto input=snapshot_32();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=null_value(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6e61697665");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_34();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206461746574696d652d6e616976652d6e756c6c"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // datetime-naive-{"$float": "NaN"}
const auto input=snapshot_32();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=float_value(bytes(""),std::numeric_limits<double>::quiet_NaN());PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6e61697665");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_34();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206461746574696d652d6e616976652d7b2224666c6f6174223a20224e614e227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // datetime-naive-"2026-04-04T00:00:00.000000001+00:00"
const auto input=snapshot_32();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6e61697665");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_34();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206461746574696d652d6e616976652d22323032362d30342d30345430303a30303a30302e3030303030303030312b30303a303022"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // datetime-literal-date-{"$timestamp": "2026-04-04T00:00:00.000000001Z"}
const auto input=snapshot_32();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=date_value(bytes(""),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030"),std::optional<std::string>{bytes("555443")});PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6c69746572616c2d64617465");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_34();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206461746574696d652d6c69746572616c2d646174652d7b222474696d657374616d70223a2022323032362d30342d30345430303a30303a30302e3030303030303030315a227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // datetime-literal-date-{"$timestamp": "2026-04-04T00:00:00.000000001"}
const auto input=snapshot_32();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=date_value(bytes(""),bytes("323032362d30342d30345430303a30303a30302e303030303030303031"),std::nullopt);PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6c69746572616c2d64617465");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_34();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206461746574696d652d6c69746572616c2d646174652d7b222474696d657374616d70223a2022323032362d30342d30345430303a30303a30302e303030303030303031227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // datetime-literal-date-{"$nat": true}
const auto input=snapshot_32();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=nat_value(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6c69746572616c2d64617465");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_34();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206461746574696d652d6c69746572616c2d646174652d7b22246e6174223a20747275657d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // datetime-literal-date-null
const auto input=snapshot_32();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=null_value(bytes(""));PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6c69746572616c2d64617465");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_34();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206461746574696d652d6c69746572616c2d646174652d6e756c6c"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // datetime-literal-date-{"$float": "NaN"}
const auto input=snapshot_32();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=float_value(bytes(""),std::numeric_limits<double>::quiet_NaN());PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6c69746572616c2d64617465");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_34();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206461746574696d652d6c69746572616c2d646174652d7b2224666c6f6174223a20224e614e227d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // datetime-literal-date-"2026-04-04T00:00:00.000000001+00:00"
const auto input=snapshot_32();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("323032362d30342d30345430303a30303a30302e3030303030303030312b30303a3030")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6c69746572616c2d64617465");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_33();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a206461746574696d652d6c69746572616c2d646174652d22323032362d30342d30345430303a30303a30302e3030303030303030312b30303a303022"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // group-key-case-null
const auto input=snapshot_35();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_36();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2067726f75702d6b65792d636173652d6e756c6c"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // group-key-case-numeric
const auto input=snapshot_37();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_38();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2067726f75702d6b65792d636173652d6e756d65726963"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // group-key-case-datetime
const auto input=snapshot_39();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_40();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2067726f75702d6b65792d636173652d6461746574696d65"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // group-key-case-category
const auto input=snapshot_41();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_42();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2067726f75702d6b65792d636173652d63617465676f7279"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // group-key-case-list
const auto input=snapshot_43();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("547970654572726f72") && std::string(e.what())==bytes("756e6861736861626c6520747970653a20276c69737427");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a2067726f75702d6b65792d636173652d6c697374"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // group-key-case-dict
const auto input=snapshot_44();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("547970654572726f72") && std::string(e.what())==bytes("756e6861736861626c6520747970653a20276469637427");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a2067726f75702d6b65792d636173652d64696374"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // shape-empty
const auto input=snapshot_3();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2073686170652d656d707479"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // shape-no-match
const auto input=snapshot_45();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_46();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2073686170652d6e6f2d6d61746368"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // shape-all-match
const auto input=snapshot_47();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_47();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2073686170652d616c6c2d6d61746368"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // shape-reversed
const auto input=snapshot_48();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_49();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2073686170652d7265766572736564"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // shape-duplicate-index
const auto input=snapshot_50();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_51();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2073686170652d6475706c69636174652d696e646578"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // shape-multi-index
const auto input=snapshot_52();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_53();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2073686170652d6d756c74692d696e646578"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // shape-custom-columns
const auto input=snapshot_54();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.case_id_key=bytes("206c69746572616c206361736520");
options.attribute_key=bytes("206c69746572616c20616374697669747920");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_55();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2073686170652d637573746f6d2d636f6c756d6e73"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // shape-empty-columns
const auto input=snapshot_56();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.case_id_key=bytes("");
options.attribute_key=bytes("206c69746572616c2076616c756520");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_57();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2073686170652d656d7074792d636f6c756d6e73"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // shape-literal-columns
const auto input=snapshot_58();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.case_id_key=bytes("20636173650acea90020");
options.attribute_key=bytes("2076616c75650d0acea90020");
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_59();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2073686170652d6c69746572616c2d636f6c756d6e73"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // selector-pm4py:param:case_id_key-'missing'
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.case_id_key=bytes("6d697373696e67");
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && std::string(e.what())==bytes("276d697373696e6727");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a2073656c6563746f722d706d3470793a706172616d3a636173655f69645f6b65792d276d697373696e6727"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // selector-pm4py:param:case_id_key-''
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.case_id_key=bytes("");
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_0();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2073656c6563746f722d706d3470793a706172616d3a636173655f69645f6b65792d2727"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // selector-pm4py:param:case_id_key-None
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.case_id_key=nullptr;
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("547970654572726f72") && std::string(e.what())==bytes("596f75206861766520746f20737570706c79206f6e65206f66202762792720616e6420276c6576656c27");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a2073656c6563746f722d706d3470793a706172616d3a636173655f69645f6b65792d4e6f6e65"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // selector-pm4py:param:case_id_key-7
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.case_id_key=TokenInteger(bytes("37"));
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && std::string(e.what())==bytes("37");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a2073656c6563746f722d706d3470793a706172616d3a636173655f69645f6b65792d37"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // empty-missing-selector-pm4py:param:case_id_key
const auto input=snapshot_3();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.case_id_key=bytes("6d697373696e67");
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && std::string(e.what())==bytes("276d697373696e6727");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a20656d7074792d6d697373696e672d73656c6563746f722d706d3470793a706172616d3a636173655f69645f6b6579"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // selector-pm4py:param:attribute_key-'missing'
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6d697373696e67");
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && std::string(e.what())==bytes("276d697373696e6727");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a2073656c6563746f722d706d3470793a706172616d3a6174747269627574655f6b65792d276d697373696e6727"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // selector-pm4py:param:attribute_key-''
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("");
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a2073656c6563746f722d706d3470793a706172616d3a6174747269627574655f6b65792d2727"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // selector-pm4py:param:attribute_key-None
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.attribute_key=nullptr;
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && std::string(e.what())==bytes("4e6f6e65");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a2073656c6563746f722d706d3470793a706172616d3a6174747269627574655f6b65792d4e6f6e65"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // selector-pm4py:param:attribute_key-7
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.attribute_key=TokenInteger(bytes("37"));
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && std::string(e.what())==bytes("37");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a2073656c6563746f722d706d3470793a706172616d3a6174747269627574655f6b65792d37"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // empty-missing-selector-pm4py:param:attribute_key
const auto input=snapshot_3();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
options.attribute_key=bytes("6d697373696e67");
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && std::string(e.what())==bytes("276d697373696e6727");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a20656d7074792d6d697373696e672d73656c6563746f722d706d3470793a706172616d3a6174747269627574655f6b6579"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // missing-columns-missing-case
const auto input=snapshot_60();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && std::string(e.what())==bytes("27636173653a636f6e636570743a6e616d6527");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a206d697373696e672d636f6c756d6e732d6d697373696e672d63617365"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // missing-columns-missing-attribute
const auto input=snapshot_61();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && std::string(e.what())==bytes("27636f6e636570743a6e616d6527");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a206d697373696e672d636f6c756d6e732d6d697373696e672d617474726962757465"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // missing-columns-missing-both
const auto input=snapshot_62();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=EventAttribute{bytes(""),bytes("41")};PandasAttributeRepetitionOptions options;
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("4b65794572726f72") && std::string(e.what())==bytes("27636f6e636570743a6e616d6527");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a206d697373696e672d636f6c756d6e732d6d697373696e672d626f7468"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // container-target-{"$list": ["A"]}
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=container_value(bytes(""),AttributeContainerKind::kList,{EventAttribute{bytes("30"),bytes("41")}});PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
bool threw=false;try{(void)filter_pandas_attribute_value_repetition(input,target,options);}catch(const PandasAttributeRepetitionError& e){threw=e.type==bytes("56616c75654572726f72") && std::string(e.what())==bytes("28274c656e67746873206d757374206d6174636820746f20636f6d70617265272c2028382c292c2028312c2929");if(!threw)std::cerr<<e.type<<": "<<e.what()<<"\n";}
if(!threw)throw std::runtime_error(bytes("50616e646173206572726f72206d69736d617463683a20636f6e7461696e65722d7461726765742d7b22246c697374223a205b2241225d7d"));++errors;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // container-target-{"$list": ["A", "A", "A", "A", "A", "A", "A", "A"]}
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=container_value(bytes(""),AttributeContainerKind::kList,{EventAttribute{bytes("30"),bytes("41")},EventAttribute{bytes("31"),bytes("41")},EventAttribute{bytes("32"),bytes("41")},EventAttribute{bytes("33"),bytes("41")},EventAttribute{bytes("34"),bytes("41")},EventAttribute{bytes("35"),bytes("41")},EventAttribute{bytes("36"),bytes("41")},EventAttribute{bytes("37"),bytes("41")}});PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_2();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a20636f6e7461696e65722d7461726765742d7b22246c697374223a205b2241222c202241222c202241222c202241222c202241222c202241222c202241222c202241225d7d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // container-target-{"$tuple": ["A"]}
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=container_value(bytes(""),AttributeContainerKind::kTuple,{EventAttribute{bytes("30"),bytes("41")}});PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a20636f6e7461696e65722d7461726765742d7b22247475706c65223a205b2241225d7d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
{ // container-target-{"$dict": [["x", 1]]}
const auto input=snapshot_0();const auto before=serialize_embedding_workbench_table_json(input);
const auto target=container_value(bytes(""),AttributeContainerKind::kContainer,{integer_value(bytes("78"),std::int64_t{1LL})});PandasAttributeRepetitionOptions options;
options.minimum=TokenInteger(bytes("30"));
const auto result=filter_pandas_attribute_value_repetition(input,target,options);const auto expected=snapshot_3();
if(serialize_embedding_workbench_table_json(result)!=serialize_embedding_workbench_table_json(expected))throw std::runtime_error(bytes("50616e6461732066756c6c207461626c65206d69736d617463683a20636f6e7461696e65722d7461726765742d7b222464696374223a205b5b2278222c20315d5d7d"));++returned;
assert(serialize_embedding_workbench_table_json(input)==before);}
assert(returned==123&&errors==16);std::cout<<returned<<"/123 full pandas repetition results and "<<errors<<"/16 native diagnostics passed\n";}
void pandas_repetition_native_storage(){
 auto input=snapshot_0();input.metadata.compatibility_snapshot_valid=true;
 input.metadata.compatibility_globals_snapshot={{"literal",{{"raw",std::string("\xff\0",2)}}}};
 input.metadata.properties={{"raw",std::string("\xff\0",2)}};
 const auto before=serialize_embedding_workbench_table_json(input);
 const auto result=filter_pandas_attribute_value_repetition(input,EventAttribute{"","A"});
 assert(result.metadata.compatibility_snapshot_valid);
 assert(result.metadata.compatibility_globals_snapshot==input.metadata.compatibility_globals_snapshot);
 auto changed=result;changed.rows[0].clear();changed.attributes.clear();
 assert(serialize_embedding_workbench_table_json(input)==before);
 PandasAttributeRepetitionOptions zero;zero.minimum=TokenInteger(0);zero.maximum=TokenInteger(0);
 assert(filter_pandas_attribute_value_repetition(input,EventAttribute{"","absent"},zero).rows.empty());
 auto dates=snapshot_0();dates.columns={"case:concept:name","concept:name"};dates.dtypes={"object","datetime64[ns, UTC]"};dates.rows.clear();dates.index=std::nullopt;dates.categorical_dtypes.clear();
 const auto a=date_value("concept:name","2026-04-04T00:00:00.000000001Z",std::string("UTC"));
 const auto b=date_value("concept:name","2026-04-04T00:00:00.000000002Z",std::string("UTC"));
 dates.rows={{{EventAttribute{"case:concept:name","same"}},a},{{EventAttribute{"case:concept:name","same"}},b}};
 PandasAttributeRepetitionOptions one;one.minimum=TokenInteger(1);one.maximum=TokenInteger(1);
 assert(filter_pandas_attribute_value_repetition(dates,a,one).rows.size()==2);
}
} // namespace
int main(){retained_pandas_repetition_cases();pandas_repetition_native_storage();}
