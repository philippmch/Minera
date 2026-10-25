// algo.simulation.playout.oc_causal_net (EXTENSIVE) against the public seeded
// bank (docs/oc-causal-net-playout-bank.txt.gz,
// scripts/capture-oc-causal-net-playout-bank.py). Each case builds the
// native causal net from the bank's marker groups and plays it out under one
// and two bindings per activity: the binding sequences (sorted), and the
// trace-order independent content of the default and the unique-object OCEL
// results must equal the reference's. Calls that timed out or varied with the
// hash seed are skipped.
#include "minera/oc_causal_net_reference_playout.hpp"

#include "../gzip_codec.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace minera;

struct Json {
  enum Kind { kNull, kBool, kNumber, kString, kArray, kObject } kind = kNull;
  double number = 0;
  bool boolean = false;
  std::string text;
  std::vector<Json> items;
  std::map<std::string, Json> fields;
  [[nodiscard]] const Json* get(const std::string& key) const {
    const auto found = fields.find(key);
    return found == fields.end() ? nullptr : &found->second;
  }
};

class JsonReader {
 public:
  explicit JsonReader(const char* text) : at_(text) {}
  Json value() {
    space();
    Json out;
    if (*at_ == '{') {
      out.kind = Json::kObject;
      ++at_;
      for (space(); *at_ != '}'; space()) {
        auto key = value().text;
        space(); ++at_;  // ':'
        out.fields.emplace(std::move(key), value());
        space();
        if (*at_ == ',') ++at_;
      }
      ++at_;
    } else if (*at_ == '[') {
      out.kind = Json::kArray;
      ++at_;
      for (space(); *at_ != ']'; space()) {
        out.items.push_back(value());
        space();
        if (*at_ == ',') ++at_;
      }
      ++at_;
    } else if (*at_ == '"') {
      out.kind = Json::kString;
      for (++at_; *at_ != '"'; ++at_) {
        if (*at_ == '\\') {
          ++at_;
          out.text += *at_ == 'n' ? '\n' : *at_ == 't' ? '\t' : *at_;
          continue;
        }
        out.text += *at_;
      }
      ++at_;
    } else if (*at_ == 't' || *at_ == 'f') {
      out.kind = Json::kBool;
      out.boolean = *at_ == 't';
      at_ += out.boolean ? 4 : 5;
    } else if (*at_ == 'n') {
      at_ += 4;
    } else {
      out.kind = Json::kNumber;
      char* end = nullptr;
      out.number = std::strtod(at_, &end);
      at_ = end;
    }
    return out;
  }

 private:
  void space() { while (*at_ == ' ' || *at_ == '\n') ++at_; }
  const char* at_;
};

// Canonical text (key-sorted objects).
std::string render(const Json& value) {
  switch (value.kind) {
    case Json::kNull: return "null";
    case Json::kBool: return value.boolean ? "true" : "false";
    case Json::kNumber: {
      char buffer[64];
      std::snprintf(buffer, sizeof buffer, "%.12g", value.number);
      return buffer;
    }
    case Json::kString: return '"' + value.text + '"';
    case Json::kArray: {
      std::string out = "[";
      for (std::size_t i = 0; i < value.items.size(); ++i) out += (i ? "," : "") + render(value.items[i]);
      return out + "]";
    }
    case Json::kObject: {
      std::string out = "{";
      bool first = true;
      for (const auto& [key, field] : value.fields) {
        out += (first ? "\"" : ",\"") + key + "\":" + render(field);
        first = false;
      }
      return out + "}";
    }
  }
  return {};
}

std::string quoted(const std::string& value) { return '"' + value + '"'; }

std::string objects_json(const std::vector<std::string>& objects) {
  std::string out = "[";
  for (std::size_t i = 0; i < objects.size(); ++i) out += (i ? "," : "") + quoted(objects[i]);
  return out + "]";
}

// A list rendered from already rendered items, sorted (multiset order).
std::string sorted_list(std::vector<std::string> items) {
  std::sort(items.begin(), items.end());
  std::string out = "[";
  for (std::size_t i = 0; i < items.size(); ++i) out += (i ? "," : "") + items[i];
  return out + "]";
}

std::string sorted_items(const Json& list) {
  std::vector<std::string> items;
  for (const auto& item : list.items) items.push_back(render(item));
  return sorted_list(std::move(items));
}

struct BankCase {
  std::string name;
  Json groups, objects, record;
};

std::vector<BankCase> read_bank(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error(std::string("cannot read ") + path);
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  std::istringstream text(detail::gzip_decompress(bytes, std::size_t{1} << 31));
  std::vector<BankCase> cases;
  std::string line;
  while (std::getline(text, line)) {
    if (line.size() < 2) continue;
    if (line[0] == 'C') {
      cases.emplace_back();
      cases.back().name = line.substr(2);
    } else if (line[0] == 'N') {
      cases.back().groups = JsonReader(line.c_str() + 2).value();
    } else if (line[0] == 'B') {
      cases.back().objects = JsonReader(line.c_str() + 2).value();
    } else if (line[0] == 'R') {
      cases.back().record = JsonReader(line.c_str() + 2).value();
    }
  }
  return cases;
}

OcCausalNet causal_net(const Json& groups) {
  OcCausalNet net;
  std::set<std::string> types;
  for (const auto& [name, sides] : groups.fields) {
    OcCausalActivity activity;
    activity.activity = name;
    for (const auto* side : {"img", "omg"}) {
      auto& target = std::string(side) == "img" ? activity.input_marker_groups : activity.output_marker_groups;
      for (const auto& group : sides.get(side)->items) {
        OcCausalMarkerGroup native;
        for (const auto& marker : group.items) {
          OcCausalMarker m;
          m.related_activity = marker.items[0].text;
          m.object_type = marker.items[1].text;
          m.min_cardinality = static_cast<std::size_t>(marker.items[2].items[0].number);
          const auto high = marker.items[2].items[1].number;
          m.max_cardinality = high < 0 ? kOcCausalInfiniteCardinality : static_cast<std::size_t>(high);
          m.marker_key = static_cast<std::int64_t>(marker.items[3].number);
          types.insert(m.object_type);
          native.markers.push_back(m);
        }
        target.push_back(std::move(native));
      }
    }
    net.activities.push_back(std::move(activity));
  }
  net.object_types.assign(types.begin(), types.end());
  return net;
}

std::map<std::string, std::vector<std::string>> objects_of(const Json& objects) {
  std::map<std::string, std::vector<std::string>> out;
  for (const auto& [type, ids] : objects.fields)
    for (const auto& id : ids.items) out[type].push_back(id.text);
  return out;
}

std::string side_json(const std::optional<std::vector<OcCausalReferenceObligations>>& side) {
  if (!side) return "null";
  std::string out = "[";
  for (std::size_t i = 0; i < side->size(); ++i)
    out += (i ? ",[" : "[") + quoted((*side)[i].activity) + "," + quoted((*side)[i].object_type) + "," +
           objects_json((*side)[i].objects) + "]";
  return out + "]";
}

std::size_t counts[2] = {};  // compared, skipped

std::string sequences_text(const std::vector<OcCausalReferenceSequence>& sequences) {
  std::vector<std::string> rendered;
  for (const auto& sequence : sequences) {
    std::string out = "[";
    for (std::size_t i = 0; i < sequence.size(); ++i)
      out += (i ? ",[" : "[") + quoted(sequence[i].activity) + "," + side_json(sequence[i].consumed) + "," +
             side_json(sequence[i].produced) + "]";
    rendered.push_back(out + "]");
  }
  return sorted_list(std::move(rendered));
}

// RETURN_SEQUENCES.
std::string check_sequences(const OcCausalNet& net, const BankCase& item, std::size_t bindings) {
  const auto config = "sequences" + std::to_string(bindings);
  const auto& expected = *item.record.get(config);
  if (!expected.get("sequences") && !expected.get("error")) {
    ++counts[1];
    return {};
  }
  ++counts[0];
  OcCausalReferencePlayoutOptions options;
  options.max_bindings_per_activity = bindings;
  std::string actual, wanted;
  try {
    actual = sequences_text(reference_oc_causal_net_sequences(net, objects_of(item.objects), options));
  } catch (const OcCausalReferenceError& error) {
    actual = "[" + quoted(error.type) + "," + quoted(error.what()) + "]";
  }
  wanted = expected.get("sequences") ? sorted_items(*expected.get("sequences")) : render(*expected.get("error"));
  return actual == wanted ? std::string()
                          : "\n  " + config + ": expected " + wanted.substr(0, 400) + "\n    actual " + actual.substr(0, 400);
}

// Trace-order independent content of an OCEL result, in the bank's shape.
std::string content(const Ocel& log, bool unique) {
  if (log.events.empty()) return "{\"events\":[]}";
  std::map<std::string, std::vector<std::string>> related;
  for (const auto& relation : log.relations) related[relation.event_id].push_back(relation.object_id);
  std::vector<std::string> ids, timestamps, events, objects;
  std::map<std::string, std::vector<std::string>> traces;
  const auto plain = [](const std::vector<std::string>& items) {
    std::string out = "[";
    for (std::size_t i = 0; i < items.size(); ++i) out += (i ? "," : "") + items[i];
    return out + "]";
  };
  for (const auto& event : log.events) {
    ids.push_back(quoted(event.id));
    timestamps.push_back(quoted(event.timestamp));
    auto bound = related[event.id];
    if (unique) {
      const auto suffix = bound.empty() ? std::string() : bound.front().substr(bound.front().rfind('_') + 1);
      for (auto& object : bound) object = object.substr(0, object.rfind('_'));
      std::sort(bound.begin(), bound.end());
      traces[suffix].push_back("[" + quoted(event.activity) + "," + objects_json(bound) + "]");
    } else {
      std::sort(bound.begin(), bound.end());
      events.push_back("[" + quoted(event.activity) + "," + objects_json(bound) + "]");
    }
  }
  if (unique) {
    std::vector<std::string> lists;
    for (const auto& [suffix, list] : traces) lists.push_back(plain(list));
    return "{\"ids\":" + plain(ids) + ",\"objects\":" + std::to_string(log.objects.size()) +
           ",\"traces\":" + sorted_list(lists) + "}";
  }
  for (const auto& object : log.objects) objects.push_back("[" + quoted(object.id) + "," + quoted(object.type) + "]");
  return "{\"events\":" + sorted_list(events) + ",\"ids\":" + plain(ids) + ",\"objects\":" + sorted_list(objects) +
         ",\"timestamps\":" + plain(timestamps) + "}";
}

// The default OCEL result and OBJECTS_UNIQUE_PER_SEQUENCE.
std::string check_playout(const OcCausalNet& net, const BankCase& item, std::size_t bindings) {
  std::string failures;
  for (const bool unique : {false, true}) {
    const auto config = (unique ? "unique" : "ocel") + std::to_string(bindings);
    const auto& expected = *item.record.get(config);
    if (expected.get("timeout") || expected.get("varies") || expected.get("error")) {
      ++counts[1];
      continue;
    }
    ++counts[0];
    OcCausalReferencePlayoutOptions options;
    options.max_bindings_per_activity = bindings;
    options.objects_unique_per_sequence = unique;
    const auto actual = content(reference_oc_causal_net_playout(net, objects_of(item.objects), options), unique);
    std::string wanted;
    if (!expected.get("ids")) {
      wanted = "{\"events\":[]}";
    } else if (unique) {
      wanted = "{\"ids\":" + render(*expected.get("ids")) + ",\"objects\":" + render(*expected.get("objects")) +
               ",\"traces\":" + sorted_items(*expected.get("traces")) + "}";
    } else {
      wanted = "{\"events\":" + sorted_items(*expected.get("events")) + ",\"ids\":" + render(*expected.get("ids")) +
               ",\"objects\":" + sorted_items(*expected.get("objects")) +
               ",\"timestamps\":" + render(*expected.get("timestamps")) + "}";
    }
    if (actual != wanted)
      failures += "\n  " + config + ": expected " + wanted.substr(0, 300) + "\n    actual " + actual.substr(0, 300);
  }
  return failures;
}

}  // namespace

int main(int argc, char** argv) try {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " BANK.txt.gz\n";
    return 2;
  }
  std::size_t failures = 0;
  for (const auto& item : read_bank(argv[1])) {
    std::string failure;
    try {
      const auto net = causal_net(item.groups);
      for (const std::size_t bindings : {1, 2})
        failure += check_sequences(net, item, bindings) + check_playout(net, item, bindings);
    } catch (const std::exception& error) {
      failure = std::string("\n  raised ") + error.what();
    }
    if (failure.empty()) continue;
    if (++failures <= 20) std::cerr << item.name << ":" << failure << '\n';
  }
  std::cout << counts[0] << " playout calls compared (" << counts[1] << " timed out or hash-seed dependent), "
            << failures << " failing cases\n";
  return failures || counts[0] == 0 ? 1 : 0;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << "\n";
  return 1;
}
