#include "minera/ocel_transform.hpp"
#include "minera/object_centric_playout_workbench_json.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
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
using Triple = std::tuple<std::string, std::string, std::string>;
using Selection = std::optional<std::vector<std::string>>;
struct CapturedCase {
  const char* name;
  Ocel input;
  Selection selection;
  bool default_call;
  std::vector<Triple> expected;
  std::optional<std::string> missing;
};
const std::vector<CapturedCase>& cases() {
  // Literal full O2O outcomes from134 captured public apply calls; duplicates
  // are retained. Public set iteration order is not a relational invariant.
  // records.jsonl SHA256: dde3d2462a66af3a48be6e71b782426e7c56289cfa23a0d4046715f72def9694
  // Dataframe-only auxiliary columns/configuration are observed separately;
  // native constructor state is preserved without a lossy dataframe adapter.
  static const std::vector<CapturedCase> values = {
      {"incidence-0#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, bytes("6530")},
      {"incidence-1#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, bytes("6531")},
      {"incidence-2#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, bytes("6531")},
      {"incidence-3#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, bytes("6531")},
      {"incidence-4#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, bytes("6531")},
      {"incidence-5#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, bytes("6531")},
      {"incidence-6#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, bytes("6531")},
      {"incidence-7#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, bytes("6531")},
      {"incidence-8#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, bytes("6530")},
      {"incidence-9#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, std::nullopt},
      {"incidence-10#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, std::nullopt},
      {"incidence-11#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"incidence-12#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, std::nullopt},
      {"incidence-13#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"incidence-14#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"incidence-15#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"incidence-16#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, bytes("6530")},
      {"incidence-17#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, std::nullopt},
      {"incidence-18#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, std::nullopt},
      {"incidence-19#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"incidence-20#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, std::nullopt},
      {"incidence-21#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"incidence-22#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"incidence-23#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"incidence-24#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, bytes("6530")},
      {"incidence-25#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f64656174685f6772617068")}}, std::nullopt},
      {"incidence-26#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f64656174685f6772617068")}}, std::nullopt},
      {"incidence-27#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f64656174685f6772617068")}}, std::nullopt},
      {"incidence-28#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f64656174685f6772617068")}}, std::nullopt},
      {"incidence-29#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"incidence-30#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"incidence-31#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"incidence-32#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, bytes("6530")},
      {"incidence-33#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, std::nullopt},
      {"incidence-34#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, std::nullopt},
      {"incidence-35#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f64656174685f6772617068")}}, std::nullopt},
      {"incidence-36#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, std::nullopt},
      {"incidence-37#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"incidence-38#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"incidence-39#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"incidence-40#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, bytes("6530")},
      {"incidence-41#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"incidence-42#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"incidence-43#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"incidence-44#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"incidence-45#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"incidence-46#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"incidence-47#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"incidence-48#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, bytes("6530")},
      {"incidence-49#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"incidence-50#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"incidence-51#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"incidence-52#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"incidence-53#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"incidence-54#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"incidence-55#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"incidence-56#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {}, bytes("6530")},
      {"incidence-57#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"incidence-58#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"incidence-59#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"incidence-60#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"incidence-61#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"incidence-62#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"incidence-63#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"graph-subset-0#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{}, false,
          {}, std::nullopt},
      {"graph-subset-1#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, false,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("64"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"graph-subset-2#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, false,
          {{bytes("63"), bytes("64"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}}, std::nullopt},
      {"graph-subset-3#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, false,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("64"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}}, std::nullopt},
      {"graph-subset-4#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, false,
          {{bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"graph-subset-5#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, false,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("64"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"graph-subset-6#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f64657363656e64616e74735f6772617068"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, false,
          {{bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}}, std::nullopt},
      {"graph-subset-7#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068"), bytes("6f626a6563745f64657363656e64616e74735f6772617068"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, false,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("64"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"graph-subset-8#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f636f62697274685f6772617068")}, false,
          {{bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"graph-subset-9#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068"), bytes("6f626a6563745f636f62697274685f6772617068")}, false,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("64"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"graph-subset-10#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f64657363656e64616e74735f6772617068"), bytes("6f626a6563745f636f62697274685f6772617068")}, false,
          {{bytes("63"), bytes("64"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}}, std::nullopt},
      {"graph-subset-11#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068"), bytes("6f626a6563745f64657363656e64616e74735f6772617068"), bytes("6f626a6563745f636f62697274685f6772617068")}, false,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("64"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"graph-subset-12#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e6865726974616e63655f6772617068"), bytes("6f626a6563745f636f62697274685f6772617068")}, false,
          {{bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"graph-subset-13#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068"), bytes("6f626a6563745f696e6865726974616e63655f6772617068"), bytes("6f626a6563745f636f62697274685f6772617068")}, false,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("64"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"graph-subset-14#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f64657363656e64616e74735f6772617068"), bytes("6f626a6563745f696e6865726974616e63655f6772617068"), bytes("6f626a6563745f636f62697274685f6772617068")}, false,
          {{bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"graph-subset-15#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068"), bytes("6f626a6563745f64657363656e64616e74735f6772617068"), bytes("6f626a6563745f696e6865726974616e63655f6772617068"), bytes("6f626a6563745f636f62697274685f6772617068")}, false,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("64"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"graph-subset-16#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {}, std::nullopt},
      {"graph-subset-17#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068"), bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("64"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"graph-subset-18#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f64657363656e64616e74735f6772617068"), bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("63"), bytes("64"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}}, std::nullopt},
      {"graph-subset-19#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068"), bytes("6f626a6563745f64657363656e64616e74735f6772617068"), bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("64"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}}, std::nullopt},
      {"graph-subset-20#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e6865726974616e63655f6772617068"), bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"graph-subset-21#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068"), bytes("6f626a6563745f696e6865726974616e63655f6772617068"), bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("64"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"graph-subset-22#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f64657363656e64616e74735f6772617068"), bytes("6f626a6563745f696e6865726974616e63655f6772617068"), bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}}, std::nullopt},
      {"graph-subset-23#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068"), bytes("6f626a6563745f64657363656e64616e74735f6772617068"), bytes("6f626a6563745f696e6865726974616e63655f6772617068"), bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("64"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"graph-subset-24#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f636f62697274685f6772617068"), bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"graph-subset-25#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068"), bytes("6f626a6563745f636f62697274685f6772617068"), bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("64"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"graph-subset-26#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f64657363656e64616e74735f6772617068"), bytes("6f626a6563745f636f62697274685f6772617068"), bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("63"), bytes("64"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}}, std::nullopt},
      {"graph-subset-27#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068"), bytes("6f626a6563745f64657363656e64616e74735f6772617068"), bytes("6f626a6563745f636f62697274685f6772617068"), bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("64"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}}, std::nullopt},
      {"graph-subset-28#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e6865726974616e63655f6772617068"), bytes("6f626a6563745f636f62697274685f6772617068"), bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"graph-subset-29#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068"), bytes("6f626a6563745f696e6865726974616e63655f6772617068"), bytes("6f626a6563745f636f62697274685f6772617068"), bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("64"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"graph-subset-30#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f64657363656e64616e74735f6772617068"), bytes("6f626a6563745f696e6865726974616e63655f6772617068"), bytes("6f626a6563745f636f62697274685f6772617068"), bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"graph-subset-31#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("4130"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("4131"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("4132"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("4133"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.relations.push_back({bytes("6533"), bytes("64"), bytes("6f726967696e616c20726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068"), bytes("6f626a6563745f64657363656e64616e74735f6772617068"), bytes("6f626a6563745f696e6865726974616e63655f6772617068"), bytes("6f626a6563745f636f62697274685f6772617068"), bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("64"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"lifetime-lifetime-chain#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("41"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("61"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("")});
          log.relations.push_back({bytes("6532"), bytes("62"), bytes("")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("")});
          log.relations.push_back({bytes("6533"), bytes("63"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"lifetime-lifetime-split-merge#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.objects.push_back({bytes("64"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("61"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("63"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("64"), bytes("")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("")});
          log.relations.push_back({bytes("6532"), bytes("64"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("64"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("64"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("64"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("61"), bytes("64"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("64"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("64"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("61"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("64"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("63"), bytes("64"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("61"), bytes("64"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"lifetime-lifetime-ephemeral#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("63"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("62"), bytes("63"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("63"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("61"), bytes("63"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f64656174685f6772617068")}}, std::nullopt},
      {"lifetime-lifetime-samebirth-differentdeath#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("62"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("61"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"lifetime-lifetime-differentbirth-samedeath#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("61"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f636f64656174685f6772617068")}}, std::nullopt},
      {"lifetime-lifetime-separatebirth-jointdeath#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("")});
          log.relations.push_back({bytes("6532"), bytes("61"), bytes("")});
          log.relations.push_back({bytes("6532"), bytes("62"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("61"), bytes("62"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"lifetime-lifetime-chain-reverse-events#0",
          [] { Ocel log;
          log.events.push_back({bytes("6533"), bytes("41"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("61"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("")});
          log.relations.push_back({bytes("6532"), bytes("62"), bytes("")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("")});
          log.relations.push_back({bytes("6533"), bytes("63"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("63"), bytes("62"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("63"), bytes("62"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}}, std::nullopt},
      {"lifetime-lifetime-chain-reverse-relations#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("41"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.relations.push_back({bytes("6533"), bytes("63"), bytes("")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("")});
          log.relations.push_back({bytes("6532"), bytes("62"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("61"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"lifetime-lifetime-chain-reverse-timestamps#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("61"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("")});
          log.relations.push_back({bytes("6532"), bytes("62"), bytes("")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("")});
          log.relations.push_back({bytes("6533"), bytes("63"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"lifetime-lifetime-chain-interleave-relations#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6532"), bytes("41"), bytes("323032362d30312d30315430303a30303a30322b30303a3030"), {}});
          log.events.push_back({bytes("6533"), bytes("41"), bytes("323032362d30312d30315430303a30303a30332b30303a3030"), {}});
          log.objects.push_back({bytes("61"), bytes("54"), {}});
          log.objects.push_back({bytes("62"), bytes("54"), {}});
          log.objects.push_back({bytes("63"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("61"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("62"), bytes("")});
          log.relations.push_back({bytes("6532"), bytes("63"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("61"), bytes("")});
          log.relations.push_back({bytes("6532"), bytes("62"), bytes("")});
          log.relations.push_back({bytes("6533"), bytes("63"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("63"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("63"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("62"), bytes("61"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("61"), bytes("62"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"null#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, false,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"empty-parameters#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, false,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"irrelevant#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, false,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"enum-selection#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f64657363656e64616e74735f6772617068"), bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}}, std::nullopt},
      {"reverse-graph-order#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f636f64656174685f6772617068"), bytes("6f626a6563745f636f62697274685f6772617068"), bytes("6f626a6563745f696e6865726974616e63655f6772617068"), bytes("6f626a6563745f64657363656e64616e74735f6772617068"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, false,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"repeated-graph#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, false,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"unknown-graph#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("756e6b6e6f776e206772617068")}, false,
          {}, std::nullopt},
      {"known-then-unknown#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068"), bytes("756e6b6e6f776e206772617068")}, false,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"unknown-then-known#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("756e6b6e6f776e206772617068"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, false,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"null-graph-list#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, false,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"string-graph-list#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, false,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"both-parameter-key-kinds#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}}, std::nullopt},
      {"custom-columns#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("206c69746572616c206576656e7420"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("206c69746572616c206f626a65637420"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("206c69746572616c206576656e7420"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("206c69746572616c206f626a65637420"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, false,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"unicode-ids#0",
          [] { Ocel log;
          log.events.push_back({bytes("206576656e742c3020"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6576656e742d3e31"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("20ceb120"), bytes("54"), {}});
          log.objects.push_back({bytes("f09f9880"), bytes("54"), {}});
          log.objects.push_back({bytes("71756f746564220a"), bytes("54"), {}});
          log.relations.push_back({bytes("206576656e742c3020"), bytes("20ceb120"), bytes("")});
          log.relations.push_back({bytes("206576656e742c3020"), bytes("f09f9880"), bytes("")});
          log.relations.push_back({bytes("6576656e742d3e31"), bytes("20ceb120"), bytes("")});
          log.relations.push_back({bytes("6576656e742d3e31"), bytes("71756f746564220a"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("20ceb120"), bytes("f09f9880"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("20ceb120"), bytes("71756f746564220a"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("20ceb120"), bytes("71756f746564220a"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("20ceb120"), bytes("71756f746564220a"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("20ceb120"), bytes("f09f9880"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("f09f9880"), bytes("20ceb120"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("20ceb120"), bytes("71756f746564220a"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("71756f746564220a"), bytes("20ceb120"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"empty-object-id#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes(""), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes(""), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes(""), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes(""), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes(""), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes(""), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes(""), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes(""), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f31"), bytes(""), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes(""), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes(""), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}}, std::nullopt},
      {"empty-event-id#0",
          [] { Ocel log;
          log.events.push_back({bytes(""), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes(""), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes(""), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"duplicate-qualified#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("6f7468657220726f6c65")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"events-reversed#0",
          [] { Ocel log;
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"relation-reversed#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"rich-metadata#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {{bytes("6c69746572616c206576656e7420617474726962757465"), bytes("20ceb120")}}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {{bytes("6c69746572616c206576656e7420617474726962757465"), bytes("00207461696c")}}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {{bytes("6c69746572616c206f626a65637420617474726962757465"), 0x1.0000000000000p+0}}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {{bytes("6c69746572616c206f626a65637420617474726962757465"), -0x0.0p+0}}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {{bytes("6c69746572616c206f626a65637420617474726962757465"), std::numeric_limits<double>::quiet_NaN()}}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.e2e.push_back({bytes("6531"), bytes("6530"), bytes("7072696f72")});
          log.object_changes.push_back({bytes("6f30"), bytes("54"), bytes("323032362d30312d30315430303a30303a30392b30303a3030"), bytes("6c69746572616c206f626a65637420617474726962757465"), bytes("6368616e676564")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6c69746572616c206576656e7420617474726962757465")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6c69746572616c206f626a65637420617474726962757465")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572"), bytes("6c69746572616c2072656c6174696f6e20617474726962757465")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"existing-o2o#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c")}, {bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"existing-duplicates#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c")}, {bytes("6f30"), bytes("6f31"), bytes("6f726967696e616c")}, {bytes("6f30"), bytes("6f31"), bytes("")}, {bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"existing-self#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f30"), bytes("6f30"), bytes("6f726967696e616c")}, {bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"existing-selected-qualifier#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, false,
          {{bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"repeat-default#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"repeat-default#1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::nullopt, true,
          {{bytes("6f31"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e6865726974616e63655f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f31"), bytes("6f626a6563745f636f62697274685f6772617068")}, {bytes("6f32"), bytes("6f30"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f696e746572616374696f6e5f6772617068")}}, std::nullopt},
      {"repeat-selection#0",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f64657363656e64616e74735f6772617068"), bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}}, std::nullopt},
      {"repeat-selection#1",
          [] { Ocel log;
          log.events.push_back({bytes("6530"), bytes("41"), bytes("323032362d30312d30315430303a30303a30302b30303a3030"), {}});
          log.events.push_back({bytes("6531"), bytes("41"), bytes("323032362d30312d30315430303a30303a30312b30303a3030"), {}});
          log.objects.push_back({bytes("6f30"), bytes("54"), {}});
          log.objects.push_back({bytes("6f31"), bytes("54"), {}});
          log.objects.push_back({bytes("6f32"), bytes("54"), {}});
          log.relations.push_back({bytes("6530"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6530"), bytes("6f31"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f30"), bytes("")});
          log.relations.push_back({bytes("6531"), bytes("6f32"), bytes("")});
          log.o2o.push_back({bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")});
          log.o2o.push_back({bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")});
          log.table_columns.events = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70")};
          log.table_columns.objects = std::vector<std::string>{bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065")};
          log.table_columns.relations = std::vector<std::string>{bytes("6f63656c3a656964"), bytes("6f63656c3a6163746976697479"), bytes("6f63656c3a74696d657374616d70"), bytes("6f63656c3a6f6964"), bytes("6f63656c3a74797065"), bytes("6f63656c3a7175616c6966696572")};
          return log; }(),
          std::vector<std::string>{bytes("6f626a6563745f64657363656e64616e74735f6772617068"), bytes("6f626a6563745f636f64656174685f6772617068")}, false,
          {{bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f636f64656174685f6772617068")}, {bytes("6f30"), bytes("6f32"), bytes("6f626a6563745f64657363656e64616e74735f6772617068")}}, std::nullopt},
  };
  return values;
}
std::vector<Triple> triples(const Ocel& log) {
  std::vector<Triple> result;
  for (const auto& row : log.o2o)
    result.emplace_back(row.source_id, row.target_id, row.qualifier);
  return result;
}
void retained_public_ocel20_cases() {
  std::size_t matched = 0, returned = 0, errors = 0;
  for (const auto& fixture : cases()) {
    const auto before = serialize_oc_playout_workbench_ocel_json(fixture.input);
    const auto selected_before = fixture.selection;
    bool okay = false;
    try {
      auto result = fixture.default_call ? compute_ocel20(fixture.input) :
          compute_ocel20(fixture.input, fixture.selection);
      ++returned;
      auto actual = triples(result);
      auto expected = fixture.expected;
      const auto original = triples(fixture.input);
      const bool prefix = actual.size() >= original.size() &&
          std::equal(original.begin(), original.end(), actual.begin());
      std::sort(actual.begin(), actual.end());
      std::sort(expected.begin(), expected.end());
      // Every non-O2O native field must be byte-identical to the source.
      result.o2o = fixture.input.o2o;
      okay = !fixture.missing && actual == expected && prefix &&
          serialize_oc_playout_workbench_ocel_json(result) == before;
    } catch (const std::out_of_range& error) {
      ++errors;
      okay = fixture.missing && std::string(error.what()) ==
          "OCEL descendants event has no object relations: " + *fixture.missing;
    }
    okay = okay && selected_before == fixture.selection &&
        serialize_oc_playout_workbench_ocel_json(fixture.input) == before;
    if (!okay) std::cerr << "ocel20 mismatch: " << fixture.name << '\n';
    matched += okay;
  }
  std::cout << matched << '/' << cases().size() << " captured cases; "
            << returned << " returns, " << errors << " errors\n";
  assert(cases().size() == 134);
  assert(matched == 134 && returned == 119 && errors == 15);
}

Ocel rich_native() {
  Ocel log;
  log.events = {{"e0", "A", "t0", {{"duplicate", std::int64_t{1}},
      {"duplicate", bytes("00ffeda080")}, {"inactive", nullptr}}},
      {"e1", "B", "t1", {{"zero", -0.0}, {"bool", true}}}};
  log.objects = {{"a", "T", {{"raw", bytes("ff")}}}, {"b", "T", {}}};
  log.relations = {{"e0", "a", "original"}, {"e0", "b", "role"},
                  {"e1", "a", "last"}};
  log.o2o = {{"a", "b", "object_cobirth_graph"},
             {"a", "b", "object_cobirth_graph"}, {"a", "a", "self"}};
  log.e2e = {{"e1", "e0", bytes("ff00")}};
  log.object_changes = {{"a", "T", "t2", "raw", bytes("ff")},
                        {"b", "T", "t3", "zero", -0.0}};
  log.globals = {{"literal", bytes("ff00eda080")}, {"zero", -0.0},
                 {"absent", nullptr}, {"bool", true}};
  log.event_type_attributes = {{"unused event type", {{"x", "string"}}}};
  log.object_type_attributes = {{"unused object type", {{"x", "float"}}}};
  log.table_columns.events = std::vector<std::string>{"literal id", "tail", "tail"};
  log.table_columns.objects = std::vector<std::string>{};
  log.table_columns.relations = std::nullopt;
  return log;
}

void metadata_multiplicity_and_independent_result() {
  const auto input = rich_native();
  const auto before = serialize_oc_playout_workbench_ocel_json(input);
  auto result = compute_ocel20(input);
  const auto prefix = triples(input);
  assert(std::equal(prefix.begin(), prefix.end(), triples(result).begin()));
  const auto expected_count = prefix.size() + 3; // interaction2 + birth1; composite inheritance is descendants
  assert(result.o2o.size() == expected_count);
  auto repeated = compute_ocel20(result);
  assert(repeated.o2o.size() == expected_count + 3);
  repeated.o2o = input.o2o;
  assert(serialize_oc_playout_workbench_ocel_json(repeated) == before);
  result.events[0].activity = "changed result only";
  result.globals["literal"] = std::string("changed");
  assert(serialize_oc_playout_workbench_ocel_json(input) == before);
}

void selected_graph_diagnostics_and_legacy_separation() {
  const Ocel log{{{"first", "A", "t0", {}}, {"last", "B", "t1", {}}},
                 {{"a", "T", {}}}, {}};
  for (const auto& name : {"object_interaction_graph", "unknown"})
    assert(compute_ocel20(log, std::vector<std::string>{name}).o2o.empty());
  assert(compute_ocel20(log, std::vector<std::string>{}).o2o.empty());
  for (const auto& [name, word, event] : std::vector<Triple>{
      {"object_descendants_graph", "descendants", "first"},
      {"object_cobirth_graph", "cobirth", "first"},
      {"object_inheritance_graph", "descendants", "first"},
      {"object_codeath_graph", "codeath", "last"}}) {
    bool rejected = false;
    try { (void)compute_ocel20(log, std::vector<std::string>{name}); }
    catch (const std::out_of_range& error) {
      rejected = std::string(error.what()) ==
          "OCEL " + word + " event has no object relations: " + event;
    }
    assert(rejected);
  }
  const auto original = rich_native();
  const auto exact = compute_ocel20(original);
  const auto legacy = enrich_ocel(original);
  assert(exact.relations[0].qualifier == "original");
  assert(legacy.relations[0].qualifier == "creation");
  // enrich_ocel now writes the same reference O2O rows; only its lifecycle
  // E2O qualifiers differ from the composite.
  assert(exact.o2o.size() == legacy.o2o.size());
  for (std::size_t row = 0; row < exact.o2o.size(); ++row)
    assert(exact.o2o[row].source_id == legacy.o2o[row].source_id &&
           exact.o2o[row].target_id == legacy.o2o[row].target_id &&
           exact.o2o[row].qualifier == legacy.o2o[row].qualifier);
  // The public composite labels descendants as inheritance. The independent
  // direct relation is different on this same-birth/different-death model.
  const auto direct = discover_ocel_object_graph(
      original, OcelEnrichmentGraph::kInheritance);
  assert((direct.edges == std::vector<std::pair<std::string, std::string>>{
      {"b", "a"}}));
  const auto composite = compute_ocel20(original,
      std::vector<std::string>{"object_inheritance_graph"});
  assert(triples(composite) == triples(original));
  auto duplicate = original;
  duplicate.objects.push_back(duplicate.objects[0]);
  bool rejected = false;
  try { (void)compute_ocel20(duplicate); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected);
}
} // namespace

int main() {
  retained_public_ocel20_cases();
  metadata_multiplicity_and_independent_result();
  selected_graph_diagnostics_and_legacy_separation();
  std::cout << "OCEL 2.0 computation module contract passed\n";
}
