#include "minera/bpmn.hpp"
#include "minera/pnml.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <regex>
#include <string>
#include <vector>

namespace {

using namespace minera;

struct ImportCase {
  const char* name;
  const char* bpmn;
  std::vector<std::string> places;
  std::vector<std::string> transitions;
  std::vector<std::string> initial;
  std::vector<std::string> final_marking;
};

struct ExportCase {
  const char* name;
  const char* pnml;
  std::vector<std::string> nodes;
  std::vector<std::string> flows;
};

const std::vector<ImportCase> kImportCases = {
#include "bpmn_reference_cases.inc"
};

const std::vector<ExportCase> kExportCases = {
#include "bpmn_export_reference_cases.inc"
};

bool uuid_shaped(const std::string& name) {
  static const std::regex pattern(
      "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}");
  return std::regex_match(name, pattern);
}

std::string joined(std::vector<std::string> names) {
  std::sort(names.begin(), names.end());
  std::string text;
  for (std::size_t i = 0; i < names.size(); ++i) text += (i ? "," : "") + names[i];
  return text;
}

std::vector<std::string> sorted(std::vector<std::string> values) {
  std::sort(values.begin(), values.end());
  return values;
}

void report(const char* name, const std::vector<std::string>& actual,
            const std::vector<std::string>& expected) {
  if (actual == expected) return;
  std::cerr << name << ": got";
  for (const auto& value : actual) std::cerr << "\n  " << value;
  std::cerr << "\nexpected";
  for (const auto& value : expected) std::cerr << "\n  " << value;
  std::cerr << '\n';
}

// Reference BPMN-to-Petri construction: place names, transition names
// (random UUIDs in the reference, deterministic UUID-shaped names here),
// labels, arcs and markings, captured from pm4py 2.7.22.
void bpmn_import_matches_reference_construction() {
  for (const auto& test : kImportCases) {
    const auto net = parse_bpmn(test.bpmn);
    assert(net.place_names());
    assert(net.transition_names());
    const auto& places = *net.place_names();
    const auto& names = *net.transition_names();
    std::vector<std::string> transitions;
    for (std::size_t t = 0; t < net.transitions().size(); ++t) {
      const auto& transition = net.transitions()[t];
      std::vector<std::string> inputs;
      std::vector<std::string> outputs;
      for (const auto place : transition.inputs) inputs.push_back(places[place]);
      for (const auto place : transition.outputs) outputs.push_back(places[place]);
      assert(names[t]);
      const auto name = uuid_shaped(*names[t]) ? std::string("U") : *names[t];
      const auto label = transition.silent()
                             ? std::string("~")
                             : net.activities()[*transition.activity];
      transitions.push_back(name + "|" + label + "|" + joined(inputs) + "|" +
                            joined(outputs));
    }
    assert(net.has_source() && net.has_sink());
    const std::vector<std::string> initial{places[net.source()]};
    const std::vector<std::string> final_marking{places[net.sink()]};
    report(test.name, sorted(transitions), test.transitions);
    assert(sorted(std::vector<std::string>(places.begin(), places.end())) == test.places);
    assert(sorted(transitions) == test.transitions);
    assert(sorted(initial) == test.initial);
    assert(sorted(final_marking) == test.final_marking);
  }
}

struct ParsedBpmn {
  std::vector<std::string> nodes;
  std::vector<std::string> flows;
  std::size_t shapes = 0;
  std::size_t edges = 0;
};

ParsedBpmn parse_exported(const std::string& xml) {
  static const std::regex node(
      R"re(<(startEvent|endEvent|task|exclusiveGateway|parallelGateway|inclusiveGateway) id="([^"]+)"([^>]*)>)re");
  static const std::regex name_attribute(R"re( name="([^"]*)")re");
  static const std::regex direction_attribute(R"re( gatewayDirection="([^"]*)")re");
  static const std::regex flow(
      R"re(<sequenceFlow id="[^"]+" sourceRef="([^"]+)" targetRef="([^"]+)"/>)re");
  ParsedBpmn parsed;
  std::map<std::string, std::string> labels;
  for (auto it = std::sregex_iterator(xml.begin(), xml.end(), node);
       it != std::sregex_iterator(); ++it) {
    const std::string element = (*it)[1];
    const std::string attributes = (*it)[3];
    std::smatch match;
    std::string name;
    if (element == "task" && std::regex_search(attributes, match, name_attribute))
      name = match[1];
    std::string direction = "Unspecified";
    if (std::regex_search(attributes, match, direction_attribute)) direction = match[1];
    const auto label = element + "|" + name + "|" + direction;
    labels[(*it)[2]] = label;
    parsed.nodes.push_back(label);
  }
  for (auto it = std::sregex_iterator(xml.begin(), xml.end(), flow);
       it != std::sregex_iterator(); ++it)
    parsed.flows.push_back(labels.at((*it)[1]) + ">" + labels.at((*it)[2]));
  const std::string shape = "<bpmndi:BPMNShape ";
  const std::string edge = "<bpmndi:BPMNEdge ";
  for (auto at = xml.find(shape); at != std::string::npos; at = xml.find(shape, at + 1))
    ++parsed.shapes;
  for (auto at = xml.find(edge); at != std::string::npos; at = xml.find(edge, at + 1))
    ++parsed.edges;
  std::sort(parsed.nodes.begin(), parsed.nodes.end());
  std::sort(parsed.flows.begin(), parsed.flows.end());
  return parsed;
}

// Reference Petri-to-BPMN structure (node kinds, task names, gateway
// directions and the labelled flow multiset), plus the diagram interchange
// every node and flow gets.
void petri_export_matches_reference_structure() {
  for (const auto& test : kExportCases) {
    const auto xml = to_bpmn(parse_pnml(test.pnml));
    const auto parsed = parse_exported(xml);
    report(test.name, parsed.nodes, test.nodes);
    report(test.name, parsed.flows, test.flows);
    assert(parsed.nodes == test.nodes);
    assert(parsed.flows == test.flows);
    assert(parsed.shapes == parsed.nodes.size());
    assert(parsed.edges == parsed.flows.size());
    // Every node lists its flows, and the importer reads the export back.
    assert(xml.find("<incoming>") != std::string::npos);
    assert(xml.find("<outgoing>") != std::string::npos);
    (void)parse_bpmn(xml);
  }
}

}  // namespace

int main() {
  bpmn_import_matches_reference_construction();
  petri_export_matches_reference_structure();
  return 0;
}
