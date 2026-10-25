// algo.organizational_mining.util against the public seeded bank
// (docs/organizational-util-bank.txt.gz,
// scripts/capture-organizational-util-bank.py): every result compares as
// ordered JSON (dict order included), errors by class and message.
#include "minera/organizational_util.hpp"

#include "../gzip_codec.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

struct BankCase {
  std::string name;
  std::string kind;
  bool custom = false;
  std::vector<std::pair<std::size_t, std::vector<std::string>>> events;  // log events
  std::vector<std::vector<std::string>> rows;                            // table rows
  std::string groups, res_act, resources;
};

std::vector<BankCase> read_bank(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error(std::string("cannot read ") + path);
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  std::istringstream text(detail::gzip_decompress(bytes, std::size_t{1} << 31));
  std::vector<BankCase> cases;
  std::string line;
  while (std::getline(text, line)) {
    if (line.empty()) continue;
    std::istringstream words(line);
    std::string tag;
    words >> tag;
    if (tag == "C") {
      cases.emplace_back();
      std::string custom;
      words >> cases.back().name >> cases.back().kind >> custom;
      cases.back().custom = custom == "1";
      continue;
    }
    auto& item = cases.back();
    const auto rest = line.substr(tag.size() + 1);
    if (tag == "V") {
      std::size_t trace = 0;
      std::vector<std::string> values(3);
      words >> trace >> values[0] >> values[1] >> values[2];
      item.events.push_back({trace, values});
    } else if (tag == "R") {
      std::vector<std::string> values(3);
      words >> values[0] >> values[1] >> values[2];
      item.rows.push_back(values);
    } else if (tag[0] == 'G') {
      item.groups = tag.size() > 1 ? "! " + rest : rest;
    } else if (tag[0] == 'A') {
      item.res_act = tag.size() > 1 ? "! " + rest : rest;
    } else if (tag[0] == 'S') {
      item.resources = tag.size() > 1 ? "! " + rest : rest;
    }
  }
  return cases;
}

std::string json_value(const OrganizationalValue& value) { return value ? "\"" + *value + "\"" : "null"; }

std::string json(const OrganizationalCounts& counts) {
  std::string out = "[";
  for (std::size_t i = 0; i < counts.size(); ++i)
    out += (i ? ",[" : "[") + json_value(counts[i].first) + "," + std::to_string(counts[i].second) + "]";
  return out + "]";
}

std::string json(const OrganizationalNested& nested) {
  std::string out = "[";
  for (std::size_t i = 0; i < nested.size(); ++i)
    out += (i ? ",[" : "[") + json_value(nested[i].first) + "," + json(nested[i].second) + "]";
  return out + "]";
}

template <typename Call>
std::string outcome(Call call) {
  try {
    return call();
  } catch (const OrganizationalUtilError& error) {
    return "! " + error.type + " " + error.what();
  }
}

std::size_t counts[2] = {};

OrganizationalUtilKeys keys_of(const BankCase& item) {
  OrganizationalUtilKeys keys;
  if (item.custom) keys = {"activity", "resource", "group"};
  return keys;
}

EventLog log_of(const BankCase& item) {
  const auto keys = keys_of(item);
  EventLog log;
  for (const auto& [trace, values] : item.events) {
    while (log.traces.size() <= trace) log.traces.emplace_back();
    Event event;
    const std::string* names[] = {&keys.activity_key, &keys.resource_key, &keys.group_key};
    for (std::size_t k = 0; k < 3; ++k) {
      if (values[k] == "-") continue;
      const bool null = values[k] == "!";
      if (*names[k] == "concept:name") {
        event.activity = null ? "" : values[k];
        event.activity_state = null ? FirstClassAttributeState::kNull : FirstClassAttributeState::kPresent;
      } else if (*names[k] == "org:resource") {
        event.resource = null ? "" : values[k];
        event.resource_state = null ? FirstClassAttributeState::kNull : FirstClassAttributeState::kPresent;
      } else {
        EventAttribute attribute;
        attribute.key = *names[k];
        attribute.value = null ? "" : values[k];
        attribute.is_null = null;
        event.attributes.push_back(attribute);
      }
    }
    log.traces[trace].events.push_back(std::move(event));
  }
  return log;
}

EventTable table_of(const BankCase& item) {
  const auto keys = keys_of(item);
  EventTable table;
  table.columns = {keys.activity_key, keys.resource_key, keys.group_key};
  for (const auto& values : item.rows) {
    EventTable::Row row;
    for (std::size_t k = 0; k < 3; ++k) {
      EventAttribute cell;
      cell.key = table.columns[k];
      if (values[k] == "!") cell.is_null = true;
      else cell.value = values[k];
      row.push_back(cell);
    }
    table.rows.push_back(std::move(row));
  }
  return table;
}

template <typename Source>
std::string compare(const BankCase& item, const Source& source) {
  const auto keys = keys_of(item);
  const auto groups = outcome([&] { return json(organizational_groups_from_log(source, keys)); });
  const auto res_act = outcome([&] {
    const auto [by_resource, by_activity] = organizational_res_act_from_log(source, keys);
    return "{\"t\":[" + json(by_resource) + "," + json(by_activity) + "]}";
  });
  const auto resources = outcome([&] { return json(organizational_resources_from_log(source, keys)); });
  if (groups != item.groups) return "groups " + groups + " expected " + item.groups;
  if (res_act != item.res_act) return "res_act " + res_act + " expected " + item.res_act;
  if (resources != item.resources) return "resources " + resources + " expected " + item.resources;
  return {};
}

// The three functions on an EventLog.
std::string check_log(const BankCase& item) {
  ++counts[0];
  return compare(item, log_of(item));
}

// The three functions on a DataFrame.
std::string check_table(const BankCase& item) {
  ++counts[1];
  return compare(item, table_of(item));
}

}  // namespace

int main(int argc, char** argv) try {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " BANK.txt.gz\n";
    return 2;
  }
  std::size_t failures = 0;
  for (const auto& item : read_bank(argv[1])) {
    const auto failure = item.kind == "log" ? check_log(item) : check_table(item);
    if (failure.empty()) continue;
    if (++failures <= 20) std::cerr << item.name << " (" << item.kind << "): " << failure << '\n';
  }
  std::cout << counts[0] + counts[1] << " cases compared (" << counts[0] << " event logs, " << counts[1]
            << " data frames, three calls each), " << failures << " failures\n";
  return failures || counts[0] + counts[1] < 4000 ? 1 : 0;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << "\n";
  return 1;
}
