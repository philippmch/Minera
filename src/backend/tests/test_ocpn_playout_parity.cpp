// algo.simulation.playout.ocpn (EXTENSIVE) against the public seeded bank
// (docs/ocpn-playout-bank.txt.gz, scripts/capture-ocpn-playout-bank.py).
// Each case mines its log with discover_ocpn and plays the net out under one
// and two bindings per activity: the traces (as label/object sequences, a
// silent step as null), EXISTS_TRACE, and the trace-order independent content
// of the default and the unique-object OCEL results must equal the
// reference's. Calls that timed out or varied with the hash seed are skipped.
#include "minera/ocpn_reference_playout.hpp"

#include "../gzip_codec.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
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
  Ocel log;
  Json record;
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
    std::istringstream words(line.substr(2));
    if (line[0] == 'C') {
      cases.emplace_back();
      words >> cases.back().name;
    } else if (line[0] == 'O') {
      OcelObject object;
      words >> object.id >> object.type;
      cases.back().log.objects.push_back(object);
    } else if (line[0] == 'E') {
      OcelEvent event;
      int minute = 0;
      words >> event.id >> event.activity >> minute;
      char stamp[64];
      std::snprintf(stamp, sizeof stamp, "2024-01-01T%02d:%02d:00", minute / 60, minute % 60);
      event.timestamp = stamp;
      for (std::string object; words >> object;)
        if (object != "-") cases.back().log.relations.push_back({event.id, object, ""});
      cases.back().log.events.push_back(event);
    } else if (line[0] == 'R') {
      cases.back().record = JsonReader(line.c_str() + 2).value();
    }
  }
  return cases;
}

std::string trace_json(const OcpnReferenceTrace& trace) {
  std::string out = "[";
  for (std::size_t i = 0; i < trace.size(); ++i)
    out += (i ? ",[" : "[") + (trace[i].activity ? quoted(*trace[i].activity) : "null") + "," +
           objects_json(trace[i].objects) + "]";
  return out + "]";
}

std::size_t counts[3] = {};  // compared, skipped, discovery errors

// RETURN_TRACES and EXISTS_TRACE.
std::string check_traces(const OcpnGraph& graph, const Json& record, std::size_t bindings) {
  std::string failures;
  OcpnReferencePlayoutOptions options;
  options.max_bindings_per_activity = bindings;
  const auto& traces = *record.get("traces" + std::to_string(bindings));
  if (traces.get("traces")) {
    ++counts[0];
    std::vector<std::string> native;
    for (const auto& trace : reference_ocpn_extensive_traces(graph, options)) native.push_back(trace_json(trace));
    const auto expected = sorted_items(*traces.get("traces"));
    const auto actual = sorted_list(native);
    if (expected != actual)
      failures += "\n  traces" + std::to_string(bindings) + ": expected " + std::to_string(traces.get("traces")->items.size()) +
                  " traces " + expected.substr(0, 300) + "\n    actual " + std::to_string(native.size()) + " " +
                  actual.substr(0, 300);
  } else {
    ++counts[1];
  }
  const auto& exists = *record.get("exists" + std::to_string(bindings));
  if (const auto* value = exists.get("exists")) {
    ++counts[0];
    if (value->boolean != reference_ocpn_trace_exists(graph, options))
      failures += "\n  exists" + std::to_string(bindings) + " differs";
  } else {
    ++counts[1];
  }
  return failures;
}

// Trace-order independent content of an OCEL result, in the bank's shape.
std::string content(const Ocel& log, bool unique) {
  std::map<std::string, std::vector<std::string>> related;
  for (const auto& relation : log.relations) related[relation.event_id].push_back(relation.object_id);
  std::vector<std::string> ids, timestamps, events, objects;
  std::map<std::string, std::vector<std::string>> traces;
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
  const auto plain = [](const std::vector<std::string>& items) {
    std::string out = "[";
    for (std::size_t i = 0; i < items.size(); ++i) out += (i ? "," : "") + items[i];
    return out + "]";
  };
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

// variants.utils.feasible_traces_to_ocel on the extensive traces, plain and
// with OBJECTS_UNIQUE_PER_TRACE.
std::string check_playout(const OcpnGraph& graph, const Json& record, std::size_t bindings) {
  std::string failures;
  OcpnReferencePlayoutOptions options;
  options.max_bindings_per_activity = bindings;
  for (const bool unique : {false, true}) {
    const auto config = (unique ? "unique" : "ocel") + std::to_string(bindings);
    const auto& expected_json = *record.get(config);
    if (expected_json.get("timeout") || expected_json.get("varies")) {
      ++counts[1];
      continue;
    }
    ++counts[0];
    options.objects_unique_per_trace = unique;
    // The dispatcher's result is the utils conversion of the extensive traces.
    std::map<std::string, std::string> object_types;
    for (const auto& [type, net] : graph.petri_nets) object_types[type + "_0"] = type;
    const auto log = reference_ocpn_traces_to_ocel(reference_ocpn_extensive_traces(graph, options), object_types,
                                                   unique);

    std::string expected;
    if (!expected_json.get("ids")) {
      // No trace: the reference returns an OCEL without event columns.
      expected = "empty";
    } else if (unique) {
      expected = "{\"ids\":" + render(*expected_json.get("ids")) +
                 ",\"objects\":" + render(*expected_json.get("objects")) +
                 ",\"traces\":" + sorted_items(*expected_json.get("traces")) + "}";
    } else {
      expected = "{\"events\":" + sorted_items(*expected_json.get("events")) +
                 ",\"ids\":" + render(*expected_json.get("ids")) +
                 ",\"objects\":" + sorted_items(*expected_json.get("objects")) +
                 ",\"timestamps\":" + render(*expected_json.get("timestamps")) + "}";
    }
    const auto actual = log.events.empty() ? std::string("empty") : content(log, unique);
    if (expected != actual)
      failures += "\n  " + config + ": expected " + expected.substr(0, 300) + "\n    actual " + actual.substr(0, 300);
  }
  return failures;
}

// algorithm.apply (EXTENSIVE) with the default OCEL result.
std::string check_dispatcher(const OcpnGraph& graph, const Json& record, std::size_t bindings) {
  const auto config = "ocel" + std::to_string(bindings);
  const auto& expected = *record.get(config);
  if (!expected.get("ids")) return {};
  OcpnReferencePlayoutOptions options;
  options.max_bindings_per_activity = bindings;
  const auto actual = content(reference_ocpn_extensive_playout(graph, options), false);
  const auto wanted = "{\"events\":" + sorted_items(*expected.get("events")) + ",\"ids\":" + render(*expected.get("ids")) +
                      ",\"objects\":" + sorted_items(*expected.get("objects")) +
                      ",\"timestamps\":" + render(*expected.get("timestamps")) + "}";
  return actual == wanted ? std::string() : "\n  " + config + ": reference_ocpn_extensive_playout differs";
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
    if (const auto* error = item.record.get("error")) {
      ++counts[2];
      try {
        (void)discover_ocpn(item.log);
        failure = "\n  discovery succeeded where the reference raised " + render(*error);
      } catch (const std::exception&) {
      }
    } else {
      try {
        const auto graph = discover_ocpn(item.log);
        for (const std::size_t bindings : {1, 2})
          failure += check_traces(graph, item.record, bindings) + check_playout(graph, item.record, bindings) +
                     check_dispatcher(graph, item.record, bindings);
      } catch (const std::exception& error) {
        failure = std::string("\n  raised ") + error.what();
      }
    }
    if (failure.empty()) continue;
    if (++failures <= 20) std::cerr << item.name << ":" << failure << '\n';
  }
  std::cout << counts[0] << " playout calls compared (" << counts[1] << " timed out or hash-seed dependent, "
            << counts[2] << " cases where discovery raised), " << failures << " failing cases\n";
  return failures || counts[0] == 0 ? 1 : 0;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << "\n";
  return 1;
}
