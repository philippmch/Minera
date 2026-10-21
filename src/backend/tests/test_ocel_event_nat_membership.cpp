#include "minera/ocel_filter.hpp"
#include "minera/object_centric_playout_workbench_json.hpp"
#include <cassert>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>
using namespace minera;
namespace {
std::string bytes(std::string_view hex) {
  const auto digit=[](char c) { return c<='9' ? c-'0' : c-'a'+10; };
  assert(hex.size()%2==0);
  std::string result;
  for (std::size_t i=0;i<hex.size();i+=2)
    result.push_back(static_cast<char>(16*digit(hex[i])+digit(hex[i+1])));
  return result;
}
struct Fixture {
  const char* name;
  Ocel input;
  std::string key;
  std::vector<OcelEventAttributeFilterValue> values;
  bool positive;
  Ocel expected;
};
const std::vector<Fixture>& cases() {
  // All twelve full native projections come directly from retained records.
  static const std::vector<Fixture> fixtures = {
{"event-nat-ocel:timestamp-nat-True",
[] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c6162656c"),bytes("78")},{bytes("6e756d626572"),std::int64_t{0}},{bytes("6d69786564"),true},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),nullptr},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6531"), bytes("42"), bytes("4e6154"), {{bytes("6c6162656c"),bytes("207920")},{bytes("6e756d626572"),std::int64_t{1}},{bytes("6d69786564"),std::int64_t{1}},{bytes("666c6167"),false},{bytes("6e756c6c61626c65"),std::numeric_limits<double>::quiet_NaN()},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")}}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030"), {{bytes("6c6162656c"),bytes("")},{bytes("6e756d626572"),0x1.8000000000000p+0},{bytes("6d69786564"),0x1.0000000000000p+0},{bytes("666c6167"),nullptr},{bytes("6e756c6c61626c65"),std::int64_t{0}},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6533"), bytes(""), bytes("4e6154"), {{bytes("6c6162656c"),nullptr},{bytes("6e756d626572"),nullptr},{bytes("6d69786564"),bytes("31")},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),bytes("6e616e")},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("6475706c6963617465")});
          log.relations.push_back({bytes("6532"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6532"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6532"), bytes("6533"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6533"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
bytes("6f63656c3a74696d657374616d70"),{OcelAttributeNaT{}},true,
[] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("42"), bytes("4e6154"), {{bytes("6c6162656c"),bytes("207920")},{bytes("6e756d626572"),std::int64_t{1}},{bytes("6d69786564"),std::int64_t{1}},{bytes("666c6167"),false},{bytes("6e756c6c61626c65"),std::numeric_limits<double>::quiet_NaN()},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")}}});
          log.events.push_back({bytes("6533"), bytes(""), bytes("4e6154"), {{bytes("6c6162656c"),nullptr},{bytes("6e756d626572"),nullptr},{bytes("6d69786564"),bytes("31")},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),bytes("6e616e")},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("6475706c6963617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"event-nat-ocel:timestamp-nat-False",
[] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c6162656c"),bytes("78")},{bytes("6e756d626572"),std::int64_t{0}},{bytes("6d69786564"),true},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),nullptr},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6531"), bytes("42"), bytes("4e6154"), {{bytes("6c6162656c"),bytes("207920")},{bytes("6e756d626572"),std::int64_t{1}},{bytes("6d69786564"),std::int64_t{1}},{bytes("666c6167"),false},{bytes("6e756c6c61626c65"),std::numeric_limits<double>::quiet_NaN()},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")}}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030"), {{bytes("6c6162656c"),bytes("")},{bytes("6e756d626572"),0x1.8000000000000p+0},{bytes("6d69786564"),0x1.0000000000000p+0},{bytes("666c6167"),nullptr},{bytes("6e756c6c61626c65"),std::int64_t{0}},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6533"), bytes(""), bytes("4e6154"), {{bytes("6c6162656c"),nullptr},{bytes("6e756d626572"),nullptr},{bytes("6d69786564"),bytes("31")},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),bytes("6e616e")},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("6475706c6963617465")});
          log.relations.push_back({bytes("6532"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6532"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6532"), bytes("6533"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6533"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
bytes("6f63656c3a74696d657374616d70"),{OcelAttributeNaT{}},false,
[] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c6162656c"),bytes("78")},{bytes("6e756d626572"),std::int64_t{0}},{bytes("6d69786564"),true},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),nullptr},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030"), {{bytes("6c6162656c"),bytes("")},{bytes("6e756d626572"),0x1.8000000000000p+0},{bytes("6d69786564"),0x1.0000000000000p+0},{bytes("666c6167"),nullptr},{bytes("6e756c6c61626c65"),std::int64_t{0}},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("71")});
          log.relations.push_back({bytes("6532"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"event-nat-ocel:timestamp-none-True",
[] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c6162656c"),bytes("78")},{bytes("6e756d626572"),std::int64_t{0}},{bytes("6d69786564"),true},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),nullptr},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6531"), bytes("42"), bytes("4e6154"), {{bytes("6c6162656c"),bytes("207920")},{bytes("6e756d626572"),std::int64_t{1}},{bytes("6d69786564"),std::int64_t{1}},{bytes("666c6167"),false},{bytes("6e756c6c61626c65"),std::numeric_limits<double>::quiet_NaN()},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")}}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030"), {{bytes("6c6162656c"),bytes("")},{bytes("6e756d626572"),0x1.8000000000000p+0},{bytes("6d69786564"),0x1.0000000000000p+0},{bytes("666c6167"),nullptr},{bytes("6e756c6c61626c65"),std::int64_t{0}},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6533"), bytes(""), bytes("4e6154"), {{bytes("6c6162656c"),nullptr},{bytes("6e756d626572"),nullptr},{bytes("6d69786564"),bytes("31")},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),bytes("6e616e")},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("6475706c6963617465")});
          log.relations.push_back({bytes("6532"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6532"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6532"), bytes("6533"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6533"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
bytes("6f63656c3a74696d657374616d70"),{nullptr},true,
[] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"event-nat-ocel:timestamp-none-False",
[] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c6162656c"),bytes("78")},{bytes("6e756d626572"),std::int64_t{0}},{bytes("6d69786564"),true},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),nullptr},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6531"), bytes("42"), bytes("4e6154"), {{bytes("6c6162656c"),bytes("207920")},{bytes("6e756d626572"),std::int64_t{1}},{bytes("6d69786564"),std::int64_t{1}},{bytes("666c6167"),false},{bytes("6e756c6c61626c65"),std::numeric_limits<double>::quiet_NaN()},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")}}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030"), {{bytes("6c6162656c"),bytes("")},{bytes("6e756d626572"),0x1.8000000000000p+0},{bytes("6d69786564"),0x1.0000000000000p+0},{bytes("666c6167"),nullptr},{bytes("6e756c6c61626c65"),std::int64_t{0}},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6533"), bytes(""), bytes("4e6154"), {{bytes("6c6162656c"),nullptr},{bytes("6e756d626572"),nullptr},{bytes("6d69786564"),bytes("31")},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),bytes("6e616e")},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("6475706c6963617465")});
          log.relations.push_back({bytes("6532"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6532"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6532"), bytes("6533"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6533"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
bytes("6f63656c3a74696d657374616d70"),{nullptr},false,
[] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c6162656c"),bytes("78")},{bytes("6e756d626572"),std::int64_t{0}},{bytes("6d69786564"),true},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),nullptr},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6531"), bytes("42"), bytes("4e6154"), {{bytes("6c6162656c"),bytes("207920")},{bytes("6e756d626572"),std::int64_t{1}},{bytes("6d69786564"),std::int64_t{1}},{bytes("666c6167"),false},{bytes("6e756c6c61626c65"),std::numeric_limits<double>::quiet_NaN()},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")}}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030"), {{bytes("6c6162656c"),bytes("")},{bytes("6e756d626572"),0x1.8000000000000p+0},{bytes("6d69786564"),0x1.0000000000000p+0},{bytes("666c6167"),nullptr},{bytes("6e756c6c61626c65"),std::int64_t{0}},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6533"), bytes(""), bytes("4e6154"), {{bytes("6c6162656c"),nullptr},{bytes("6e756d626572"),nullptr},{bytes("6d69786564"),bytes("31")},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),bytes("6e616e")},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("6475706c6963617465")});
          log.relations.push_back({bytes("6532"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6532"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6532"), bytes("6533"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6533"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"event-nat-ocel:timestamp-nan-True",
[] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c6162656c"),bytes("78")},{bytes("6e756d626572"),std::int64_t{0}},{bytes("6d69786564"),true},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),nullptr},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6531"), bytes("42"), bytes("4e6154"), {{bytes("6c6162656c"),bytes("207920")},{bytes("6e756d626572"),std::int64_t{1}},{bytes("6d69786564"),std::int64_t{1}},{bytes("666c6167"),false},{bytes("6e756c6c61626c65"),std::numeric_limits<double>::quiet_NaN()},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")}}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030"), {{bytes("6c6162656c"),bytes("")},{bytes("6e756d626572"),0x1.8000000000000p+0},{bytes("6d69786564"),0x1.0000000000000p+0},{bytes("666c6167"),nullptr},{bytes("6e756c6c61626c65"),std::int64_t{0}},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6533"), bytes(""), bytes("4e6154"), {{bytes("6c6162656c"),nullptr},{bytes("6e756d626572"),nullptr},{bytes("6d69786564"),bytes("31")},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),bytes("6e616e")},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("6475706c6963617465")});
          log.relations.push_back({bytes("6532"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6532"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6532"), bytes("6533"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6533"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
bytes("6f63656c3a74696d657374616d70"),{std::numeric_limits<double>::quiet_NaN()},true,
[] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"event-nat-ocel:timestamp-nan-False",
[] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c6162656c"),bytes("78")},{bytes("6e756d626572"),std::int64_t{0}},{bytes("6d69786564"),true},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),nullptr},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6531"), bytes("42"), bytes("4e6154"), {{bytes("6c6162656c"),bytes("207920")},{bytes("6e756d626572"),std::int64_t{1}},{bytes("6d69786564"),std::int64_t{1}},{bytes("666c6167"),false},{bytes("6e756c6c61626c65"),std::numeric_limits<double>::quiet_NaN()},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")}}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030"), {{bytes("6c6162656c"),bytes("")},{bytes("6e756d626572"),0x1.8000000000000p+0},{bytes("6d69786564"),0x1.0000000000000p+0},{bytes("666c6167"),nullptr},{bytes("6e756c6c61626c65"),std::int64_t{0}},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6533"), bytes(""), bytes("4e6154"), {{bytes("6c6162656c"),nullptr},{bytes("6e756d626572"),nullptr},{bytes("6d69786564"),bytes("31")},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),bytes("6e616e")},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("6475706c6963617465")});
          log.relations.push_back({bytes("6532"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6532"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6532"), bytes("6533"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6533"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
bytes("6f63656c3a74696d657374616d70"),{std::numeric_limits<double>::quiet_NaN()},false,
[] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c6162656c"),bytes("78")},{bytes("6e756d626572"),std::int64_t{0}},{bytes("6d69786564"),true},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),nullptr},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6531"), bytes("42"), bytes("4e6154"), {{bytes("6c6162656c"),bytes("207920")},{bytes("6e756d626572"),std::int64_t{1}},{bytes("6d69786564"),std::int64_t{1}},{bytes("666c6167"),false},{bytes("6e756c6c61626c65"),std::numeric_limits<double>::quiet_NaN()},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")}}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030"), {{bytes("6c6162656c"),bytes("")},{bytes("6e756d626572"),0x1.8000000000000p+0},{bytes("6d69786564"),0x1.0000000000000p+0},{bytes("666c6167"),nullptr},{bytes("6e756c6c61626c65"),std::int64_t{0}},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6533"), bytes(""), bytes("4e6154"), {{bytes("6c6162656c"),nullptr},{bytes("6e756d626572"),nullptr},{bytes("6d69786564"),bytes("31")},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),bytes("6e616e")},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("6475706c6963617465")});
          log.relations.push_back({bytes("6532"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6532"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6532"), bytes("6533"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6533"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"event-nat-nullable time-nat-True",
[] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c6162656c"),bytes("78")},{bytes("6e756d626572"),std::int64_t{0}},{bytes("6d69786564"),true},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),nullptr},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6531"), bytes("42"), bytes("4e6154"), {{bytes("6c6162656c"),bytes("207920")},{bytes("6e756d626572"),std::int64_t{1}},{bytes("6d69786564"),std::int64_t{1}},{bytes("666c6167"),false},{bytes("6e756c6c61626c65"),std::numeric_limits<double>::quiet_NaN()},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")}}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030"), {{bytes("6c6162656c"),bytes("")},{bytes("6e756d626572"),0x1.8000000000000p+0},{bytes("6d69786564"),0x1.0000000000000p+0},{bytes("666c6167"),nullptr},{bytes("6e756c6c61626c65"),std::int64_t{0}},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6533"), bytes(""), bytes("4e6154"), {{bytes("6c6162656c"),nullptr},{bytes("6e756d626572"),nullptr},{bytes("6d69786564"),bytes("31")},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),bytes("6e616e")},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("6475706c6963617465")});
          log.relations.push_back({bytes("6532"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6532"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6532"), bytes("6533"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6533"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
bytes("6e756c6c61626c652074696d65"),{OcelAttributeNaT{}},true,
[] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c6162656c"),bytes("78")},{bytes("6e756d626572"),std::int64_t{0}},{bytes("6d69786564"),true},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),nullptr},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030"), {{bytes("6c6162656c"),bytes("")},{bytes("6e756d626572"),0x1.8000000000000p+0},{bytes("6d69786564"),0x1.0000000000000p+0},{bytes("666c6167"),nullptr},{bytes("6e756c6c61626c65"),std::int64_t{0}},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("71")});
          log.relations.push_back({bytes("6532"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"event-nat-nullable time-nat-False",
[] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c6162656c"),bytes("78")},{bytes("6e756d626572"),std::int64_t{0}},{bytes("6d69786564"),true},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),nullptr},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6531"), bytes("42"), bytes("4e6154"), {{bytes("6c6162656c"),bytes("207920")},{bytes("6e756d626572"),std::int64_t{1}},{bytes("6d69786564"),std::int64_t{1}},{bytes("666c6167"),false},{bytes("6e756c6c61626c65"),std::numeric_limits<double>::quiet_NaN()},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")}}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030"), {{bytes("6c6162656c"),bytes("")},{bytes("6e756d626572"),0x1.8000000000000p+0},{bytes("6d69786564"),0x1.0000000000000p+0},{bytes("666c6167"),nullptr},{bytes("6e756c6c61626c65"),std::int64_t{0}},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6533"), bytes(""), bytes("4e6154"), {{bytes("6c6162656c"),nullptr},{bytes("6e756d626572"),nullptr},{bytes("6d69786564"),bytes("31")},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),bytes("6e616e")},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("6475706c6963617465")});
          log.relations.push_back({bytes("6532"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6532"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6532"), bytes("6533"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6533"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
bytes("6e756c6c61626c652074696d65"),{OcelAttributeNaT{}},false,
[] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("42"), bytes("4e6154"), {{bytes("6c6162656c"),bytes("207920")},{bytes("6e756d626572"),std::int64_t{1}},{bytes("6d69786564"),std::int64_t{1}},{bytes("666c6167"),false},{bytes("6e756c6c61626c65"),std::numeric_limits<double>::quiet_NaN()},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")}}});
          log.events.push_back({bytes("6533"), bytes(""), bytes("4e6154"), {{bytes("6c6162656c"),nullptr},{bytes("6e756d626572"),nullptr},{bytes("6d69786564"),bytes("31")},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),bytes("6e616e")},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("6475706c6963617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"event-nat-nullable time-none-True",
[] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c6162656c"),bytes("78")},{bytes("6e756d626572"),std::int64_t{0}},{bytes("6d69786564"),true},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),nullptr},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6531"), bytes("42"), bytes("4e6154"), {{bytes("6c6162656c"),bytes("207920")},{bytes("6e756d626572"),std::int64_t{1}},{bytes("6d69786564"),std::int64_t{1}},{bytes("666c6167"),false},{bytes("6e756c6c61626c65"),std::numeric_limits<double>::quiet_NaN()},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")}}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030"), {{bytes("6c6162656c"),bytes("")},{bytes("6e756d626572"),0x1.8000000000000p+0},{bytes("6d69786564"),0x1.0000000000000p+0},{bytes("666c6167"),nullptr},{bytes("6e756c6c61626c65"),std::int64_t{0}},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6533"), bytes(""), bytes("4e6154"), {{bytes("6c6162656c"),nullptr},{bytes("6e756d626572"),nullptr},{bytes("6d69786564"),bytes("31")},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),bytes("6e616e")},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("6475706c6963617465")});
          log.relations.push_back({bytes("6532"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6532"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6532"), bytes("6533"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6533"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
bytes("6e756c6c61626c652074696d65"),{nullptr},true,
[] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"event-nat-nullable time-none-False",
[] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c6162656c"),bytes("78")},{bytes("6e756d626572"),std::int64_t{0}},{bytes("6d69786564"),true},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),nullptr},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6531"), bytes("42"), bytes("4e6154"), {{bytes("6c6162656c"),bytes("207920")},{bytes("6e756d626572"),std::int64_t{1}},{bytes("6d69786564"),std::int64_t{1}},{bytes("666c6167"),false},{bytes("6e756c6c61626c65"),std::numeric_limits<double>::quiet_NaN()},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")}}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030"), {{bytes("6c6162656c"),bytes("")},{bytes("6e756d626572"),0x1.8000000000000p+0},{bytes("6d69786564"),0x1.0000000000000p+0},{bytes("666c6167"),nullptr},{bytes("6e756c6c61626c65"),std::int64_t{0}},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6533"), bytes(""), bytes("4e6154"), {{bytes("6c6162656c"),nullptr},{bytes("6e756d626572"),nullptr},{bytes("6d69786564"),bytes("31")},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),bytes("6e616e")},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("6475706c6963617465")});
          log.relations.push_back({bytes("6532"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6532"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6532"), bytes("6533"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6533"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
bytes("6e756c6c61626c652074696d65"),{nullptr},false,
[] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c6162656c"),bytes("78")},{bytes("6e756d626572"),std::int64_t{0}},{bytes("6d69786564"),true},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),nullptr},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6531"), bytes("42"), bytes("4e6154"), {{bytes("6c6162656c"),bytes("207920")},{bytes("6e756d626572"),std::int64_t{1}},{bytes("6d69786564"),std::int64_t{1}},{bytes("666c6167"),false},{bytes("6e756c6c61626c65"),std::numeric_limits<double>::quiet_NaN()},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")}}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030"), {{bytes("6c6162656c"),bytes("")},{bytes("6e756d626572"),0x1.8000000000000p+0},{bytes("6d69786564"),0x1.0000000000000p+0},{bytes("666c6167"),nullptr},{bytes("6e756c6c61626c65"),std::int64_t{0}},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6533"), bytes(""), bytes("4e6154"), {{bytes("6c6162656c"),nullptr},{bytes("6e756d626572"),nullptr},{bytes("6d69786564"),bytes("31")},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),bytes("6e616e")},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("6475706c6963617465")});
          log.relations.push_back({bytes("6532"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6532"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6532"), bytes("6533"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6533"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"event-nat-nullable time-nan-True",
[] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c6162656c"),bytes("78")},{bytes("6e756d626572"),std::int64_t{0}},{bytes("6d69786564"),true},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),nullptr},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6531"), bytes("42"), bytes("4e6154"), {{bytes("6c6162656c"),bytes("207920")},{bytes("6e756d626572"),std::int64_t{1}},{bytes("6d69786564"),std::int64_t{1}},{bytes("666c6167"),false},{bytes("6e756c6c61626c65"),std::numeric_limits<double>::quiet_NaN()},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")}}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030"), {{bytes("6c6162656c"),bytes("")},{bytes("6e756d626572"),0x1.8000000000000p+0},{bytes("6d69786564"),0x1.0000000000000p+0},{bytes("666c6167"),nullptr},{bytes("6e756c6c61626c65"),std::int64_t{0}},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6533"), bytes(""), bytes("4e6154"), {{bytes("6c6162656c"),nullptr},{bytes("6e756d626572"),nullptr},{bytes("6d69786564"),bytes("31")},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),bytes("6e616e")},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("6475706c6963617465")});
          log.relations.push_back({bytes("6532"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6532"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6532"), bytes("6533"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6533"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
bytes("6e756c6c61626c652074696d65"),{std::numeric_limits<double>::quiet_NaN()},true,
[] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"event-nat-nullable time-nan-False",
[] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c6162656c"),bytes("78")},{bytes("6e756d626572"),std::int64_t{0}},{bytes("6d69786564"),true},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),nullptr},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6531"), bytes("42"), bytes("4e6154"), {{bytes("6c6162656c"),bytes("207920")},{bytes("6e756d626572"),std::int64_t{1}},{bytes("6d69786564"),std::int64_t{1}},{bytes("666c6167"),false},{bytes("6e756c6c61626c65"),std::numeric_limits<double>::quiet_NaN()},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")}}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030"), {{bytes("6c6162656c"),bytes("")},{bytes("6e756d626572"),0x1.8000000000000p+0},{bytes("6d69786564"),0x1.0000000000000p+0},{bytes("666c6167"),nullptr},{bytes("6e756c6c61626c65"),std::int64_t{0}},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6533"), bytes(""), bytes("4e6154"), {{bytes("6c6162656c"),nullptr},{bytes("6e756d626572"),nullptr},{bytes("6d69786564"),bytes("31")},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),bytes("6e616e")},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("6475706c6963617465")});
          log.relations.push_back({bytes("6532"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6532"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6532"), bytes("6533"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6533"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
bytes("6e756c6c61626c652074696d65"),{std::numeric_limits<double>::quiet_NaN()},false,
[] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c6162656c"),bytes("78")},{bytes("6e756d626572"),std::int64_t{0}},{bytes("6d69786564"),true},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),nullptr},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6531"), bytes("42"), bytes("4e6154"), {{bytes("6c6162656c"),bytes("207920")},{bytes("6e756d626572"),std::int64_t{1}},{bytes("6d69786564"),std::int64_t{1}},{bytes("666c6167"),false},{bytes("6e756c6c61626c65"),std::numeric_limits<double>::quiet_NaN()},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")}}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302e3030303030303030322b30303a3030"), {{bytes("6c6162656c"),bytes("")},{bytes("6e756d626572"),0x1.8000000000000p+0},{bytes("6d69786564"),0x1.0000000000000p+0},{bytes("666c6167"),nullptr},{bytes("6e756c6c61626c65"),std::int64_t{0}},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302e3030303030303030312b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("4e6154")}}});
          log.events.push_back({bytes("6533"), bytes(""), bytes("4e6154"), {{bytes("6c6162656c"),nullptr},{bytes("6e756d626572"),nullptr},{bytes("6d69786564"),bytes("31")},{bytes("666c6167"),true},{bytes("6e756c6c61626c65"),bytes("6e616e")},{bytes("6f627365727665642074696d65"),bytes("323032362d30312d30315430303a30303a30302b30303a3030")},{bytes("6e756c6c61626c652074696d65"),bytes("323032362d30312d30315430303a30303a30302e3130303030302b30303a3030")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("71")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("6475706c6963617465")});
          log.relations.push_back({bytes("6532"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6532"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6532"), bytes("6533"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6533"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c6162656c"),bytes("6e756d626572"),bytes("6d69786564"),bytes("666c6167"),bytes("6e756c6c61626c65"),bytes("6f627365727665642074696d65"),bytes("6e756c6c61626c652074696d65")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
  };
  return fixtures;
}
void retained_public_event_nat_membership() {
  std::size_t matched=0;
  for (const auto& fixture:cases()) {
    const auto before=serialize_oc_playout_workbench_ocel_json(fixture.input);
    const auto result=filter_ocel_event_attribute_exact(fixture.input,fixture.key,
        fixture.values,fixture.positive,OcelEventAttributeColumnKind::kTimestamp);
    const bool equal=serialize_oc_playout_workbench_ocel_json(result)==
        serialize_oc_playout_workbench_ocel_json(fixture.expected) &&
        serialize_oc_playout_workbench_ocel_json(fixture.input)==before;
    if (!equal) std::cerr << "event NaT mismatch: " << fixture.name << '\n';
    matched+=equal;
  }
  std::cout << matched << "/" << cases().size() << " full event NaT public outcomes" << std::endl;
  assert(matched==12);
}
}  // namespace
int main() {
  retained_public_event_nat_membership();
}
