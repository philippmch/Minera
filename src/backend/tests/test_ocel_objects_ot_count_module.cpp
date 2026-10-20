#include "minera/ocel_filter.hpp"
#include "minera/object_centric_playout_workbench_json.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>
using namespace minera;
namespace {
std::string bytes(std::string_view hex) {
  const auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
  assert(hex.size() % 2 == 0);
  std::string value;
  for (std::size_t i = 0; i < hex.size(); i += 2)
    value.push_back(static_cast<char>(16 * digit(hex[i]) + digit(hex[i + 1])));
  return value;
}
struct Fixture {
  const char* name;
  Ocel input;
  std::map<std::string, OcelObjectTypeCountThreshold> minimum;
  Ocel expected;
};
const std::vector<Fixture>& cases() {
  // Full public snapshots. No expected record is calculated by native filtering.
  static const std::vector<Fixture> values = {
{"incidence-0-requirements-0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-0-requirements-1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-0-requirements-2",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-0-requirements-3",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-0-requirements-4",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-0-requirements-5",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-0-requirements-6",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-0-requirements-7",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-1-requirements-0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-1-requirements-1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-1-requirements-2",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-1-requirements-3",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-1-requirements-4",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-1-requirements-5",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-1-requirements-6",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-1-requirements-7",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-2-requirements-0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-2-requirements-1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-2-requirements-2",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-2-requirements-3",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-2-requirements-4",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-2-requirements-5",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-2-requirements-6",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-2-requirements-7",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-3-requirements-0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-3-requirements-1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-3-requirements-2",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-3-requirements-3",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-3-requirements-4",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-3-requirements-5",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-3-requirements-6",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-3-requirements-7",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-4-requirements-0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-4-requirements-1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-4-requirements-2",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-4-requirements-3",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-4-requirements-4",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-4-requirements-5",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-4-requirements-6",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-4-requirements-7",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-5-requirements-0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-5-requirements-1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-5-requirements-2",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-5-requirements-3",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-5-requirements-4",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-5-requirements-5",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-5-requirements-6",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-5-requirements-7",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-6-requirements-0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-6-requirements-1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-6-requirements-2",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-6-requirements-3",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-6-requirements-4",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-6-requirements-5",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-6-requirements-6",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-6-requirements-7",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-7-requirements-0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-7-requirements-1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-7-requirements-2",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-7-requirements-3",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-7-requirements-4",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-7-requirements-5",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-7-requirements-6",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-7-requirements-7",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-8-requirements-0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-8-requirements-1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-8-requirements-2",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-8-requirements-3",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-8-requirements-4",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-8-requirements-5",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-8-requirements-6",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-8-requirements-7",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-9-requirements-0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-9-requirements-1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-9-requirements-2",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-9-requirements-3",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-9-requirements-4",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-9-requirements-5",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-9-requirements-6",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-9-requirements-7",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-10-requirements-0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-10-requirements-1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-10-requirements-2",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-10-requirements-3",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-10-requirements-4",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-10-requirements-5",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-10-requirements-6",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-10-requirements-7",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-11-requirements-0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-11-requirements-1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-11-requirements-2",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-11-requirements-3",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-11-requirements-4",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-11-requirements-5",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-11-requirements-6",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-11-requirements-7",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-12-requirements-0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-12-requirements-1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-12-requirements-2",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-12-requirements-3",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-12-requirements-4",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-12-requirements-5",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-12-requirements-6",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-12-requirements-7",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-13-requirements-0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-13-requirements-1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-13-requirements-2",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-13-requirements-3",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-13-requirements-4",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-13-requirements-5",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-13-requirements-6",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-13-requirements-7",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-14-requirements-0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-14-requirements-1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-14-requirements-2",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-14-requirements-3",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-14-requirements-4",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-14-requirements-5",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-14-requirements-6",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-14-requirements-7",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-15-requirements-0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-15-requirements-1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-15-requirements-2",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-15-requirements-3",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-15-requirements-4",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-15-requirements-5",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-15-requirements-6",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"incidence-15-requirements-7",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("-1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-2",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{0x1.0000000000000p-1}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-3",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-4",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{0x1.8000000000000p+0}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-5",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-6",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{0x1.4000000000000p+1}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-7",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("3")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-8",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("4")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-9",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-10",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-11",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-12",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-13",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("-1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-14",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("-1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-15",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{0x1.0000000000000p-1}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-16",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-17",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("9007199254740991")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-18",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("9007199254740992")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-19",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("9007199254740993")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"distinct-vs-qualified-duplicate-20",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("18446744073709551617")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"order-events-reversed",
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"order-objects-reversed",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f32"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f32"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"order-relation-reversed",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"order-events-and-relations-reversed",
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"order-timestamps-reversed",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"order-tied-timestamps",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("2")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("616c7465726e617465")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"literal-empty-type",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes(""), {}});
          log.objects.push_back({bytes("6f31"), bytes(""), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes(""), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes(""), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes(""),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes(""), {}});
          log.objects.push_back({bytes("6f31"), bytes(""), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes(""), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes(""), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"literal-spaced-type",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("205420"), {}});
          log.objects.push_back({bytes("6f31"), bytes("205420"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("205420"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("205420"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("205420"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("205420"), {}});
          log.objects.push_back({bytes("6f31"), bytes("205420"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("205420"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("205420"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"literal-unicode-type",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("cea9"), {}});
          log.objects.push_back({bytes("6f31"), bytes("cea9"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("cea9"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("cea9"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("cea9"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("cea9"), {}});
          log.objects.push_back({bytes("6f31"), bytes("cea9"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("cea9"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("cea9"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"literal-unicode-ids",
          [] { Ocel log;
          log.events.push_back({bytes("206576656e742c3020"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6576656e742d3e31"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("20ceb120"), bytes("54"), {}});
          log.objects.push_back({bytes("f09f9880"), bytes("55"), {}});
          log.relations.push_back({bytes("206576656e742c3020"), bytes("20ceb120"), bytes("")});
          log.relations.push_back({bytes("206576656e742c3020"), bytes("f09f9880"), bytes("")});
          log.relations.push_back({bytes("6576656e742d3e31"), bytes("20ceb120"), bytes("")});
          log.relations.push_back({bytes("6576656e742d3e31"), bytes("f09f9880"), bytes("")});
          log.o2o.push_back({bytes("20ceb120"), bytes("f09f9880"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("f09f9880"), bytes("20ceb120"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("20ceb120"), bytes("f09f9880"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("20ceb120"), bytes("20ceb120"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("206576656e742c3020"), bytes("6576656e742d3e31"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6576656e742d3e31"), bytes("206576656e742c3020"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("20ceb120"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e6765642020ceb120")});
          log.object_changes.push_back({bytes("f09f9880"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e67656420f09f9880")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("206576656e742c3020"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6576656e742d3e31"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("20ceb120"), bytes("54"), {}});
          log.objects.push_back({bytes("f09f9880"), bytes("55"), {}});
          log.relations.push_back({bytes("206576656e742c3020"), bytes("20ceb120"), bytes("")});
          log.relations.push_back({bytes("206576656e742c3020"), bytes("f09f9880"), bytes("")});
          log.relations.push_back({bytes("6576656e742d3e31"), bytes("20ceb120"), bytes("")});
          log.relations.push_back({bytes("6576656e742d3e31"), bytes("f09f9880"), bytes("")});
          log.o2o.push_back({bytes("20ceb120"), bytes("f09f9880"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("f09f9880"), bytes("20ceb120"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("20ceb120"), bytes("f09f9880"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("20ceb120"), bytes("20ceb120"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("206576656e742c3020"), bytes("6576656e742d3e31"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6576656e742d3e31"), bytes("206576656e742c3020"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("20ceb120"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e6765642020ceb120")});
          log.object_changes.push_back({bytes("f09f9880"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e67656420f09f9880")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"literal-empty-object-id",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes(""), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes(""), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes(""), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes(""), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes(""), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes(""), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes(""), bytes(""), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes(""), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e67656420")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes(""), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes(""), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes(""), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes(""), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes(""), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes(""), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes(""), bytes(""), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes(""), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e67656420")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"literal-empty-event-id",
          [] { Ocel log;
          log.events.push_back({bytes(""), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes(""), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes(""), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes(""), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes(""), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes(""), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes(""), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes(""), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes(""), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes(""), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"empty-shape-empty-0",
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"empty-shape-empty-1",
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"empty-shape-empty-2",
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"empty-shape-empty-3",
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"empty-shape-objects-only-0",
          [] { Ocel log;
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"empty-shape-objects-only-1",
          [] { Ocel log;
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"empty-shape-objects-only-2",
          [] { Ocel log;
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"empty-shape-objects-only-3",
          [] { Ocel log;
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"empty-shape-events-only-0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"empty-shape-events-only-1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"empty-shape-events-only-2",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"empty-shape-events-only-3",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("616273656e74"),OcelObjectTypeCountThreshold{TokenInteger("0")}}},
          [] { Ocel log;
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"parameters-none",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"parameters-empty",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"parameters-irrelevant",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"parameters-custom-defaults",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("206c69746572616c206576656e7420"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("206c69746572616c206f626a65637420"),bytes("206c69746572616c207479706520")};
          log.table_columns.relations = std::vector<std::string>{bytes("206c69746572616c206576656e7420"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("206c69746572616c206f626a65637420"),bytes("206c69746572616c207479706520"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("206c69746572616c206576656e7420"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("206c69746572616c206f626a65637420"),bytes("206c69746572616c207479706520")};
          log.table_columns.relations = std::vector<std::string>{bytes("206c69746572616c206576656e7420"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("206c69746572616c206f626a65637420"),bytes("206c69746572616c207479706520"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"parameters-custom-columns",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("206c69746572616c206576656e7420"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("206c69746572616c206f626a65637420"),bytes("206c69746572616c207479706520")};
          log.table_columns.relations = std::vector<std::string>{bytes("206c69746572616c206576656e7420"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("206c69746572616c206f626a65637420"),bytes("206c69746572616c207479706520"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("206c69746572616c206576656e7420"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("206c69746572616c206f626a65637420"),bytes("206c69746572616c207479706520")};
          log.table_columns.relations = std::vector<std::string>{bytes("206c69746572616c206576656e7420"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("206c69746572616c206f626a65637420"),bytes("206c69746572616c207479706520"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"parameters-custom-enum",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("206c69746572616c206576656e7420"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("206c69746572616c206f626a65637420"),bytes("206c69746572616c207479706520")};
          log.table_columns.relations = std::vector<std::string>{bytes("206c69746572616c206576656e7420"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("206c69746572616c206f626a65637420"),bytes("206c69746572616c207479706520"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("206c69746572616c206576656e7420"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("206c69746572616c206f626a65637420"),bytes("206c69746572616c207479706520")};
          log.table_columns.relations = std::vector<std::string>{bytes("206c69746572616c206576656e7420"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("206c69746572616c206f626a65637420"),bytes("206c69746572616c207479706520"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"parameters-enum-string-precedence",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"rich-metadata",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c69746572616c206576656e7420617474726962757465"),bytes("20ceb120")}}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {{bytes("6c69746572616c206576656e7420617474726962757465"),bytes("00207461696c")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {{bytes("6c69746572616c206f626a65637420617474726962757465"),0x1.0000000000000p+0}}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {{bytes("6c69746572616c206f626a65637420617474726962757465"),-0x0.0p+0}}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {{bytes("6c69746572616c206f626a65637420617474726962757465"),std::numeric_limits<double>::quiet_NaN()}}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c69746572616c206576656e7420617474726962757465")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6c69746572616c206f626a65637420617474726962757465")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572"),bytes("6c69746572616c2072656c6174696f6e20617474726962757465")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c69746572616c206576656e7420617474726962757465"),bytes("20ceb120")}}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {{bytes("6c69746572616c206576656e7420617474726962757465"),bytes("00207461696c")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {{bytes("6c69746572616c206f626a65637420617474726962757465"),0x1.0000000000000p+0}}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {{bytes("6c69746572616c206f626a65637420617474726962757465"),-0x0.0p+0}}});
          log.objects.push_back({bytes("6f32"), bytes("55"), {{bytes("6c69746572616c206f626a65637420617474726962757465"),std::numeric_limits<double>::quiet_NaN()}}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f32"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f32"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.object_changes.push_back({bytes("6f32"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f32")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6c69746572616c206576656e7420617474726962757465")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6c69746572616c206f626a65637420617474726962757465")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572"),bytes("6c69746572616c2072656c6174696f6e20617474726962757465")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"requirement-insertion-order-TU",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
{"requirement-insertion-order-UT",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }(),
          {{bytes("55"),OcelObjectTypeCountThreshold{TokenInteger("1")}},{bytes("54"),OcelObjectTypeCountThreshold{TokenInteger("1")}}},
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("55"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f31"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c206f626a656374206c696e6b")});
          log.e2e.push_back({bytes("6530"), bytes("6531"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("6f726967696e616c206576656e74206c696e6b")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f30")});
          log.object_changes.push_back({bytes("6f31"), bytes("55"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c6162656c"), bytes("6368616e676564206f31")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"),bytes("6f63656c3a6163746976697479"),bytes("6f63656c3a74696d657374616d70"),bytes("6f63656c3a6f6964"),bytes("6f63656c3a74797065"),bytes("6f63656c3a7175616c6966696572")};
          log.globals.emplace(bytes("6c69746572616c"),bytes("20cea920"));
          log.globals.emplace(bytes("7a65726f"),-0x0.0p+0);
          log.globals.emplace(bytes("616273656e74"),nullptr);
          return log; }()},
  };
  return values;
}
void public_fixtures() {
  std::size_t matched = 0;
  std::size_t legacy_matched = 0;
  for (const auto& fixture : cases()) {
    const auto before = serialize_oc_playout_workbench_ocel_json(fixture.input);
    const auto result = filter_ocel_events_object_type_counts_exact(fixture.input, fixture.minimum);
    const bool equal = serialize_oc_playout_workbench_ocel_json(result) ==
        serialize_oc_playout_workbench_ocel_json(fixture.expected) &&
        serialize_oc_playout_workbench_ocel_json(fixture.input) == before;
    if (!equal) std::cerr << "object count mismatch: " << fixture.name << '\n';
    matched += equal;
    std::map<std::string, std::size_t> legacy;
    bool compatible = true;
    for (const auto& [type, minimum] : fixture.minimum) {
      const auto* integer = std::get_if<TokenInteger>(&minimum);
      if (!integer || *integer < TokenInteger{0} ||
          *integer > TokenInteger{std::numeric_limits<std::size_t>::max()}) {
        compatible = false;
        break;
      }
      legacy.emplace(type, static_cast<std::size_t>(*integer));
    }
    if (compatible) {
      assert(serialize_oc_playout_workbench_ocel_json(
          filter_ocel_events_object_type_counts(fixture.input, legacy)) ==
          serialize_oc_playout_workbench_ocel_json(fixture.expected));
      ++legacy_matched;
    }
  }
  std::cout << matched << "/" << cases().size() << " full public outcomes\n";
  std::cout << legacy_matched << " compatible legacy outcomes\n";
  assert(matched == cases().size());
}
void full_native_state() {
  Ocel source;
  const auto raw = bytes("00ffeda080");
  source.events = {{"e1", raw, "later", {{raw, -0.0}}},
                   {"e0", "A", "earlier", {{"dup", nullptr}, {"dup", raw}}}};
  source.objects = {{raw, raw, {{raw, true}}}, {"other", "U", {}}};
  source.relations = {{"e0", raw, "q"}, {"e1", raw, raw},
                      {"e0", raw, "qualified duplicate"}, {"e1", "other", "q"}};
  source.o2o = {{raw, "other", "link"}, {raw, raw, raw}, {raw, raw, raw}};
  source.e2e = {{"e1", "e0", "link"}, {"e0", "e0", raw}};
  source.object_changes = {{raw, raw, "later", raw, raw},
                           {"other", "U", "later", "value", -0.0}};
  source.globals = {{raw, raw}, {"bool", true}, {"integer", std::int64_t{-7}}};
  source.event_type_attributes = {{"unused", {{raw, "string"}}}};
  source.object_type_attributes = {{raw, {{"ignored", "integer"}}}};
  source.table_columns.events = std::vector<std::string>{raw, "dup", "dup"};
  source.table_columns.objects = std::vector<std::string>{};
  source.table_columns.relations = std::nullopt;
  const auto before = serialize_oc_playout_workbench_ocel_json(source);
  auto expected = source;
  expected.events.erase(expected.events.begin());
  expected.objects.pop_back();
  expected.relations.erase(expected.relations.begin() + 3);
  expected.relations.erase(expected.relations.begin() + 1);
  expected.o2o.erase(expected.o2o.begin());
  expected.e2e.erase(expected.e2e.begin());
  expected.object_changes.pop_back();
  auto result = filter_ocel_events_object_type_counts_exact(source, {{raw, 1.5}});
  assert(serialize_oc_playout_workbench_ocel_json(result) ==
         serialize_oc_playout_workbench_ocel_json(expected));
  result.globals[raw] = std::string("independent");
  result.events[0].attributes.clear();
  assert(serialize_oc_playout_workbench_ocel_json(source) == before);
  // An absent type remains a failed requirement even when its bound is negative.
  assert(filter_ocel_events_object_type_counts_exact(source,
      {{"absent", TokenInteger{-999}}}).events.empty());
  assert(filter_ocel_events_object_type_counts_exact(source,
      {{raw, TokenInteger("999999999999999999999999999999999999")}}).events.empty());
  assert(filter_ocel_events_object_type_counts_exact(source,
      {{raw, TokenInteger("-99999999999999999999999999999999999")}}).events.size() == 2);
  assert(filter_ocel_events_object_type_counts_exact(source,
      {{raw, std::nextafter(1.0, 2.0)}}).events.size() == 1);
  assert(filter_ocel_events_object_type_counts_exact(source,
      {{raw, std::nextafter(2.0, 3.0)}}).events.empty());
}
}  // namespace
int main() { public_fixtures(); full_native_state(); }
