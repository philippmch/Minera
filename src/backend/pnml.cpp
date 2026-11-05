#include "minera/pnml.hpp"
#include "minera/process_tree_petri.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <locale>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "xml_pull_parser.hpp"
#include "pnml_arc_weight.hpp"
#include "root_cause_unicode.hpp"
#include "python_float_text.hpp"

namespace minera {
namespace {

// Escapes the five XML metacharacters so activity names containing '&', '<',
// quotes, etc. produce well-formed PNML.
void write_xml_escaped(std::ostream& out, std::string_view value,
                       bool attribute = false) {
  for (const char c : value) {
    switch (c) {
      case '\t': if (attribute) out << "&#9;"; else out << c; break;
      case '\n': if (attribute) out << "&#10;"; else out << c; break;
      case '\r': out << "&#13;"; break;
      case '&': out << "&amp;"; break;
      case '<': out << "&lt;"; break;
      case '>': out << "&gt;"; break;
      case '"': out << "&quot;"; break;
      case '\'': out << "&apos;"; break;
      default: out << c;
    }
  }
}

}  // namespace

namespace {

std::vector<std::pair<std::size_t, std::size_t>> weighted_endpoints(
    const std::vector<std::size_t>& endpoints) {
  std::vector<std::pair<std::size_t, std::size_t>> weights;
  std::unordered_map<std::size_t, std::size_t> positions;
  for (const auto place : endpoints) {
    const auto [it, inserted] = positions.emplace(place, weights.size());
    if (inserted) weights.emplace_back(place, 1);
    else ++weights[it->second].second;
  }
  return weights;
}

template <class Callback>
void for_each_ordinary_arc(const PetriTransition& transition, bool input,
                           Callback callback) {
  const auto& endpoints = input ? transition.inputs : transition.outputs;
  const auto& weights = input ? transition.input_weights : transition.output_weights;
  if (weights.empty()) {
    for (const auto& [place,weight] : weighted_endpoints(endpoints))
      callback(place,PetriArcWeight{weight,false});
  } else {
    for (std::size_t index = 0; index < endpoints.size(); ++index)
      callback(endpoints[index],input ? transition.input_arc_weight(index) : transition.output_arc_weight(index));
  }
}

void validate_arc_occurrences(const PnmlDocument& document) {
  if (!document.arc_occurrences) return;
  // Compare directly against executable vectors without expanding untrusted
  // weights into allocations. Each transition has an independent cursor per
  // arc side/type, so XML may interleave transitions while retaining order.
  struct Cursors { std::size_t inputs = 0, outputs = 0, resets = 0, inhibitors = 0; };
  std::vector<Cursors> cursors(document.net.transitions().size());
  for (const auto& arc : *document.arc_occurrences) {
    if (arc.place >= document.net.place_count() ||
        arc.transition >= document.net.transitions().size() || arc.weight < 0)
      throw std::invalid_argument("PNML arc occurrence has an invalid endpoint or weight");
    const auto& transition = document.net.transitions()[arc.transition];
    auto& cursor = cursors[arc.transition];
    const std::vector<std::size_t>* endpoints = nullptr;
    std::size_t* position = nullptr;
    TokenInteger multiplicity = arc.weight;
    if (arc.type == PetriArcType::kNormal) {
      endpoints = arc.input ? &transition.inputs : &transition.outputs;
      position = arc.input ? &cursor.inputs : &cursor.outputs;
      const auto& weights = arc.input ? transition.input_weights : transition.output_weights;
      if (!weights.empty()) {
        multiplicity = 1;
        if (*position >= endpoints->size())
          throw std::invalid_argument("PNML arc occurrences do not match executable arcs");
        const auto weight = arc.input ? transition.input_arc_weight(*position) : transition.output_arc_weight(*position);
        if (weight.magnitude != arc.weight || (weight.negative && weight.magnitude) != (arc.negative_weight && arc.weight))
          throw std::invalid_argument("PNML ordinary weights do not match executable arc metadata");
      } else if (arc.negative_weight && arc.weight)
        throw std::invalid_argument("PNML signed arc has no matching executable weight metadata");
    } else if (arc.input && arc.type == PetriArcType::kReset) {
      endpoints = &transition.reset_inputs;
      position = &cursor.resets;
      multiplicity = 1;
    } else if (arc.input && arc.type == PetriArcType::kInhibitor) {
      endpoints = &transition.inhibitor_inputs;
      position = &cursor.inhibitors;
      multiplicity = 1;
    } else {
      throw std::invalid_argument("PNML arc occurrence has an invalid type or direction");
    }
    if (multiplicity > endpoints->size() - *position)
      throw std::invalid_argument("PNML arc occurrences do not match executable arcs");
    if (arc.type != PetriArcType::kNormal) {
      const auto weight = arc.type == PetriArcType::kReset
          ? transition.reset_arc_weight(*position) : transition.inhibitor_arc_weight(*position);
      if (weight.magnitude != arc.weight ||
          (weight.negative && weight.magnitude) != (arc.negative_weight && arc.weight))
        throw std::invalid_argument("PNML extended weights do not match executable arc metadata");
    }
    for (std::size_t n = 0; n < multiplicity; ++n)
      if ((*endpoints)[(*position)++] != arc.place)
        throw std::invalid_argument("PNML arc occurrences do not match executable arc order");
  }
  for (std::size_t t = 0; t < cursors.size(); ++t) {
    const auto& transition = document.net.transitions()[t];
    const auto& cursor = cursors[t];
    if (cursor.inputs != transition.inputs.size() ||
        cursor.outputs != transition.outputs.size() ||
        cursor.resets != transition.reset_inputs.size() ||
        cursor.inhibitors != transition.inhibitor_inputs.size())
      throw std::invalid_argument("PNML arc occurrences omit executable arcs");
  }
}

template <class Callback>
void for_each_document_arc(const PnmlDocument& document, Callback callback) {
  validate_arc_occurrences(document);
  if (document.arc_occurrences) {
    for (const auto& arc : *document.arc_occurrences) callback(arc);
    return;
  }
  for (std::size_t t = 0; t < document.net.transitions().size(); ++t) {
    const auto& transition = document.net.transitions()[t];
    for_each_ordinary_arc(transition,true,[&](std::size_t place,PetriArcWeight weight) {
      callback(PnmlArcOccurrence{place,t,true,PetriArcType::kNormal,weight.magnitude,{},weight.negative});
    });
    for (std::size_t i = 0; i < transition.reset_inputs.size(); ++i) {
      const auto weight = transition.reset_arc_weight(i);
      callback(PnmlArcOccurrence{transition.reset_inputs[i], t, true, PetriArcType::kReset,
                                 weight.magnitude, {}, weight.negative});
    }
    for (std::size_t i = 0; i < transition.inhibitor_inputs.size(); ++i) {
      const auto weight = transition.inhibitor_arc_weight(i);
      callback(PnmlArcOccurrence{transition.inhibitor_inputs[i], t, true, PetriArcType::kInhibitor,
                                 weight.magnitude, {}, weight.negative});
    }
    for_each_ordinary_arc(transition,false,[&](std::size_t place,PetriArcWeight weight) {
      callback(PnmlArcOccurrence{place,t,false,PetriArcType::kNormal,weight.magnitude,{},weight.negative});
    });
  }
}

}  // namespace

void write_pnml_impl(std::ostream& out, const PetriNet& net,
                     const TokenMarking& initial_marking,
                     const TokenMarking& final_marking,
                     const StochasticMap* stochastic_map,
                     std::string_view name,
                     const PnmlDocument* document = nullptr) {
  if (initial_marking.size() != net.place_count() ||
      final_marking.size() != net.place_count())
    throw std::invalid_argument("PNML marking size must match the place count");
  if (std::any_of(initial_marking.begin(), initial_marking.end(),
                  [](const TokenInteger& value) { return value < 0; }) ||
      std::any_of(final_marking.begin(), final_marking.end(),
                  [](const TokenInteger& value) { return value < 0; }))
    throw std::invalid_argument("PNML markings must be non-negative");
  if (stochastic_map) {
    validate_stochastic_map_structure(*stochastic_map, net.transitions().size());
    if (stochastic_map->transitions.size() != net.transitions().size())
      throw std::invalid_argument(
          "stochastic map: transition record count does not match the net");
  }
  std::unordered_set<std::string> used_ids;
  const auto resolve_ids = [&](const std::vector<std::string>& configured,
                                std::size_t count, const char* prefix) {
    if (!configured.empty() && configured.size() != count)
      throw std::invalid_argument("PNML ID count must match the node count");
    std::vector<std::string> result;
    for (std::size_t i = 0; i < count; ++i) {
      auto id = configured.empty() ? std::string(prefix) + std::to_string(i)
                                   : configured[i];
      if (!used_ids.insert(id).second)
        throw std::invalid_argument("PNML node IDs must be unique");
      result.push_back(std::move(id));
    }
    return result;
  };
  const std::vector<std::string> no_ids;
  // Without document IDs, a net that carries reference place names (process
  // tree, BPMN or PNML origin) exports them, unless they are not unique or
  // would collide with the generated transition IDs.
  const auto net_place_ids = [&]() -> const std::vector<std::string>& {
    if (document && !document->place_ids.empty()) return document->place_ids;
    if (!net.place_names() || net.place_names()->size() != net.place_count()) return no_ids;
    const auto& names = *net.place_names();
    std::unordered_set<std::string> seen;
    const bool generated_transition_ids = !document || document->transition_ids.empty();
    for (const auto& name : names) {
      if (!seen.insert(name).second) return no_ids;
      if (generated_transition_ids && name.size() > 1 && name[0] == 't' &&
          std::all_of(name.begin() + 1, name.end(), [](unsigned char c) { return c >= '0' && c <= '9'; }))
        return no_ids;
    }
    if (!generated_transition_ids)
      for (const auto& id : document->transition_ids)
        if (seen.contains(id)) return no_ids;
    return names;
  };
  const auto place_ids = resolve_ids(net_place_ids(), net.place_count(), "p");
  const auto transition_ids = resolve_ids(document ? document->transition_ids : no_ids,
                                          net.transitions().size(), "t");
  const auto validate_keys = [&](const std::vector<bool>& keys,
                                  const TokenMarking& marking) {
    if (keys.empty()) return;
    if (keys.size() != marking.size())
      throw std::invalid_argument("PNML marking key count must match the place count");
    for (std::size_t i = 0; i < keys.size(); ++i)
      if (marking[i] > 0 && !keys[i])
        throw std::invalid_argument("PNML marking keys must include positive tokens");
  };
  if (document) {
    validate_keys(document->initial_marking_keys, initial_marking);
    validate_keys(document->final_marking_keys, final_marking);
    validate_arc_occurrences(*document);
    if (document->arc_occurrences)
      for (const auto& arc : *document->arc_occurrences)
        if (!arc.id.empty() && !used_ids.insert(arc.id).second)
          throw std::invalid_argument("PNML arc IDs must be unique across nodes and arcs");
  }
  for (const auto& transition : net.transitions()) {
    if (transition.activity && *transition.activity >= net.activities().size())
      throw std::invalid_argument("PNML transition activity index is invalid");
    for (const auto* places : {&transition.inputs, &transition.outputs,
                              &transition.reset_inputs, &transition.inhibitor_inputs})
      for (const auto place : *places)
        if (place >= net.place_count())
          throw std::invalid_argument("PNML arc place index is invalid");
  }
  const auto unique_id = [&](std::string base) {
    std::string id = base;
    std::size_t suffix = 0;
    while (!used_ids.insert(id).second) id = base + "_" + std::to_string(++suffix);
    return id;
  };
  const auto net_id = unique_id("net1");
  const auto page_id = unique_id("page1");
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<pnml xmlns=\"http://www.pnml.org/version-2009/grammar/pnml\">\n";
  out << "  <net id=\"" << net_id << "\" "
         "type=\"http://www.pnml.org/version-2009/grammar/pnmlcoremodel\">\n";
  out << "    <name><text>";
  write_xml_escaped(out, name);
  out << "</text></name>\n";
  out << "    <page id=\"" << page_id << "\">\n";

  // Places. The source carries the single initial token.
  for (std::size_t place = 0; place < net.place_count(); ++place) {
    out << "      <place id=\"";
    write_xml_escaped(out, place_ids[place], true);
    out << "\">\n";
    out << "        <name><text>";
    write_xml_escaped(out, place_ids[place]);
    out << "</text></name>\n";
    if (initial_marking[place] != 0 || (document &&
        !document->initial_marking_keys.empty() && document->initial_marking_keys[place]))
      out << "        <initialMarking><text>" << initial_marking[place]
          << "</text></initialMarking>\n";
    out << "      </place>\n";
  }

  // Transitions. Visible transitions are labeled with their activity name;
  // silent ones get an invisible toolspecific flag (the convention used by ProM
  // read) and a tau label so the file is still human-readable.
  const auto& transitions = net.transitions();
  const auto& activities = net.activities();
  for (std::size_t t = 0; t < transitions.size(); ++t) {
    const auto& transition = transitions[t];
    out << "      <transition id=\"";
    write_xml_escaped(out, transition_ids[t], true);
    out << "\">\n";
    out << "        <name><text>";
    if (transition.silent()) {
      out << "tau";
    } else {
      write_xml_escaped(out, activities[*transition.activity]);
    }
    out << "</text></name>\n";
    if (transition.silent()) {
      out << "        <toolspecific tool=\"ProM\" version=\"6.4\" "
             "activity=\"$invisible$\" localNodeID=\"";
      write_xml_escaped(out, transition_ids[t], true);
      out << "\"/>\n";
    }
    if (stochastic_map) {
      const auto& record = stochastic_map->transitions[t];
      std::string type;
      switch (record.distribution) {
        case StochasticMapDistribution::kDeterministic: type = "DETERMINISTIC"; break;
        case StochasticMapDistribution::kExponential: type = "EXPONENTIAL"; break;
        case StochasticMapDistribution::kConstant0: type = "IMMEDIATE"; break;
        case StochasticMapDistribution::kImmediate: type = "IMMEDIATE"; break;
        case StochasticMapDistribution::kNormal: type = "NORMAL"; break;
        case StochasticMapDistribution::kUniform: type = "UNIFORM"; break;
        case StochasticMapDistribution::kLogNormal: type = "LOGNORMAL"; break;
        case StochasticMapDistribution::kGamma: type = "GAMMA"; break;
      }
      out << "        <toolspecific tool=\"StochasticPetriNet\" version=\"0.2\">\n"
          << "          <property key=\"distributionType\">" << type
          << "</property>\n";
      // PM4Py reads two parameters for NORMAL and UNIFORM and three for
      // LOGNORMAL and GAMMA, so those are always written in full.
      if (type != "IMMEDIATE") {
        out << "          <property key=\"distributionParameters\">"
            << record.parameter;
        const bool pair = type == "NORMAL" || type == "UNIFORM";
        const bool triple = type == "LOGNORMAL" || type == "GAMMA";
        if (pair || triple) out << ';' << record.secondary_parameter;
        if (triple) out << ';' << record.tertiary_parameter;
        out << "</property>\n";
      }
      out << "          <property key=\"priority\">" << record.priority
          << "</property>\n"
          << "          <property key=\"invisible\">"
          << (transition.silent() ? "true" : "false") << "</property>\n"
          << "          <property key=\"weight\">" << record.activation_weight
          << "</property>\n        </toolspecific>\n";
    }
    out << "      </transition>\n";
  }

  // Arcs: every input place -> transition, then transition -> every output
  // place. Extended input arcs carry a compact type annotation. Arc ids are
  // unique across the whole net.
  // Document export retains first-occurrence endpoint order: equal-length
  // silent routes can depend on it. Legacy net overloads retain sorted arcs.
  const auto export_endpoints = [&](const std::vector<std::size_t>& endpoints) {
    auto weights = weighted_endpoints(endpoints);
    if (!document) std::sort(weights.begin(), weights.end());
    return weights;
  };
  std::size_t arc_id = 0;
  const auto write_arc = [&](std::size_t place, std::size_t t, bool input,
                             PetriArcType type, const TokenInteger& weight,
                             const std::string& original_id = {},
                             bool negative = false) {
    out << "      <arc id=\"";
    write_xml_escaped(out, original_id.empty()
        ? unique_id("a" + std::to_string(arc_id++)) : original_id, true);
    out << "\" source=\"";
    write_xml_escaped(out, input ? place_ids[place] : transition_ids[t], true);
    out << "\" target=\"";
    write_xml_escaped(out, input ? transition_ids[t] : place_ids[place], true);
    out << "\"";
    if (type == PetriArcType::kNormal && weight == 1 && !negative) {
      out << "/>\n";
    } else {
      out << ">";
      if (weight != 1 || negative)
        out << "<inscription><text>" << (negative && weight ? "-" : "")
            << weight << "</text></inscription>";
      if (type != PetriArcType::kNormal)
        out << "<arctype><text>"
            << (type == PetriArcType::kReset ? "reset" : "inhibitor")
            << "</text></arctype>";
      out << "</arc>\n";
    }
  };
  if (document && document->arc_occurrences) {
    for (const auto& arc : *document->arc_occurrences)
      write_arc(arc.place, arc.transition, arc.input, arc.type, arc.weight, arc.id, arc.negative_weight);
  } else for (std::size_t t = 0; t < transitions.size(); ++t) {
    const auto& transition = transitions[t];
    if (transition.input_weights.empty()) for (const auto& [place,weight] : export_endpoints(transition.inputs))
      write_arc(place,t,true,PetriArcType::kNormal,weight);
    else for_each_ordinary_arc(transition,true,[&](std::size_t place,PetriArcWeight weight) {
      write_arc(place,t,true,PetriArcType::kNormal,weight.magnitude,{},weight.negative);
    });
    for (std::size_t i = 0; i < transition.reset_inputs.size(); ++i) {
      const auto weight = transition.reset_arc_weight(i);
      write_arc(transition.reset_inputs[i], t, true, PetriArcType::kReset,
                weight.magnitude, {}, weight.negative);
    }
    for (std::size_t i = 0; i < transition.inhibitor_inputs.size(); ++i) {
      const auto weight = transition.inhibitor_arc_weight(i);
      write_arc(transition.inhibitor_inputs[i], t, true, PetriArcType::kInhibitor,
                weight.magnitude, {}, weight.negative);
    }
    if (transition.output_weights.empty()) for (const auto& [place,weight] : export_endpoints(transition.outputs))
      write_arc(place,t,false,PetriArcType::kNormal,weight);
    else for_each_ordinary_arc(transition,false,[&](std::size_t place,PetriArcWeight weight) {
      write_arc(place,t,false,PetriArcType::kNormal,weight.magnitude,{},weight.negative);
    });
  }

  out << "    </page>\n";

  out << "    <finalmarkings>\n";
  out << "      <marking>\n";
  for (std::size_t place = 0; place < final_marking.size(); ++place)
    if (final_marking[place] != 0 || (document &&
        !document->final_marking_keys.empty() && document->final_marking_keys[place])) {
      out << "        <place idref=\"";
      write_xml_escaped(out, place_ids[place], true);
      out << "\"><text>" << final_marking[place] << "</text></place>\n";
    }
  out << "      </marking>\n";
  out << "    </finalmarkings>\n";

  out << "  </net>\n";
  out << "</pnml>\n";
}

void write_pnml(std::ostream& out, const PetriNet& net,
                std::string_view name) {
  std::vector<std::int64_t> initial(net.place_count(), 0);
  std::vector<std::int64_t> final(net.place_count(), 0);
  if (net.place_count() != 0) {
    if (net.source() >= net.place_count() || net.sink() >= net.place_count())
      throw std::invalid_argument("PNML workflow boundary index is invalid");
    initial[net.source()] = 1;
    final[net.sink()] = 1;
  }
  write_pnml_impl(out, net, initial, final, nullptr, name);
}

void write_pnml(std::ostream& out, const PetriNet& net,
                const std::vector<std::int64_t>& initial_marking,
                const std::vector<std::int64_t>& final_marking,
                std::string_view name) {
  write_pnml_impl(out, net, initial_marking, final_marking, nullptr, name);
}

void write_pnml(std::ostream& out, const PetriNet& net,
                const std::vector<std::int64_t>& initial_marking,
                const std::vector<std::int64_t>& final_marking,
                const StochasticMap& stochastic_map, std::string_view name) {
  write_pnml_impl(out, net, initial_marking, final_marking, &stochastic_map,
                  name);
}

void write_pnml(std::ostream& out, const PnmlDocument& document,
                std::string_view name) {
  std::ostringstream serialized;
  serialized.imbue(std::locale::classic());
  serialized.precision(std::numeric_limits<double>::max_digits10);
  write_pnml_impl(serialized, document.net, document.initial_marking,
                  document.final_marking,
                  document.stochastic_map ? &*document.stochastic_map : nullptr,
                  name, &document);
  out << serialized.str();
}

void write_pnml(std::ostream& out, const PetriNet& net,
                const TokenMarking& initial_marking,
                const TokenMarking& final_marking, std::string_view name) {
  write_pnml_impl(out, net, initial_marking, final_marking, nullptr, name);
}

void write_pnml(std::ostream& out, const PetriNet& net,
                const TokenMarking& initial_marking,
                const TokenMarking& final_marking,
                const StochasticMap& stochastic_map, std::string_view name) {
  write_pnml_impl(out, net, initial_marking, final_marking, &stochastic_map, name);
}

std::string to_pnml(const PnmlDocument& document, std::string_view name) {
  std::ostringstream out;
  write_pnml(out, document, name);
  return out.str();
}

PetriNetPresentation to_petri_net_presentation(const PnmlDocument& document) {
  PetriNetPresentation result;
  if ((!document.place_ids.empty() && document.place_ids.size() != document.net.place_count()) ||
      (!document.transition_ids.empty() && document.transition_ids.size() != document.net.transitions().size()))
    throw std::invalid_argument("PNML ID count must match the node count");
  for (std::size_t p = 0; p < document.net.place_count(); ++p)
    result.places.push_back({p, document.place_ids.empty()
        ? "p" + std::to_string(p) : document.place_ids[p], {}});
  for (std::size_t t = 0; t < document.net.transitions().size(); ++t) {
    const auto& transition = document.net.transitions()[t];
    std::optional<std::string> label;
    if (transition.activity) {
      if (*transition.activity >= document.net.activities().size())
        throw std::invalid_argument("PNML transition activity index is invalid");
      label = document.net.activities()[*transition.activity];
    }
    result.transitions.push_back({t, document.transition_ids.empty()
        ? "t" + std::to_string(t) : document.transition_ids[t], std::move(label), {}});
  }
  for_each_document_arc(document, [&](const PnmlArcOccurrence& arc) {
    if (arc.place >= result.places.size() || arc.transition >= result.transitions.size())
      throw std::invalid_argument("PNML arc place index is invalid");
    const PetriNodeRef place{PetriNodeKind::kPlace, arc.place};
    const PetriNodeRef transition{PetriNodeKind::kTransition, arc.transition};
    result.arcs.push_back({arc.input ? place : transition,
                           arc.input ? transition : place, arc.weight, {}, arc.negative_weight});
  });
  return result;
}

std::string to_pnml(const PetriNet& net, std::string_view name) {
  std::ostringstream out;
  write_pnml(out, net, name);
  return out.str();
}

std::string to_pnml(const PetriNet& net,
                    const std::vector<std::int64_t>& initial_marking,
                    const std::vector<std::int64_t>& final_marking,
                    std::string_view name) {
  std::ostringstream out;
  write_pnml(out, net, initial_marking, final_marking, name);
  return out.str();
}

std::string to_pnml(const PetriNet& net,
                    const std::vector<std::int64_t>& initial_marking,
                    const std::vector<std::int64_t>& final_marking,
                    const StochasticMap& stochastic_map,
                    std::string_view name) {
  std::ostringstream out;
  write_pnml(out, net, initial_marking, final_marking, stochastic_map, name);
  return out.str();
}

std::string to_pnml(const PetriNet& net,
                    const TokenMarking& initial_marking,
                    const TokenMarking& final_marking, std::string_view name) {
  std::ostringstream out;
  write_pnml(out, net, initial_marking, final_marking, name);
  return out.str();
}

std::string to_pnml(const PetriNet& net,
                    const TokenMarking& initial_marking,
                    const TokenMarking& final_marking,
                    const StochasticMap& stochastic_map, std::string_view name) {
  std::ostringstream out;
  write_pnml(out, net, initial_marking, final_marking, stochastic_map, name);
  return out.str();
}

namespace {

// What the first pass collects from the document before the net is assembled.
struct ParsedPlace {
  std::string id;
  TokenInteger initial_tokens = 0;
  bool has_initial_marking = false;
};

// The properties of one StochasticPetriNet toolspecific as PM4Py reads them:
// an absent or empty property is None.
struct ParsedStochasticProperties {
  std::optional<std::string> type;
  std::optional<std::string> parameters;
  std::optional<double> weight;
  std::optional<TokenInteger> priority;
};

struct ParsedTransition {
  std::string id;
  std::string label;
  bool invisible = false;
  // Each StochasticPetriNet toolspecific replaces the transition's record.
  // A record with an unrecognized or absent type is PM4Py's undefined random
  // variable (no native record); `policy` holds the native policy error of a
  // record that a native stochastic map cannot represent.
  std::optional<ParsedStochasticProperties> stochastic;
  std::optional<StochasticMapTransition> record;
  std::string policy;
  bool has_named_label = false;
};

struct ParsedArc {
  std::string source;
  std::string target;
  PetriArcType type = PetriArcType::kNormal;
  TokenInteger weight = 1;
  std::string id;
  bool negative_weight = false;
};

// UTF-8 scalar decoding and literal rendering use the owned Unicode 14 table.
// The integer error literal is truncated after rendering, by Unicode scalars.
char32_t pnml_codepoint(const std::string& value, std::size_t& position) {
  const auto first = static_cast<unsigned char>(value[position++]);
  char32_t code = first;
  unsigned count = 0;
  if (first >= 0xc2 && first <= 0xdf) { code = first & 0x1f; count = 1; }
  else if (first >= 0xe0 && first <= 0xef) { code = first & 0x0f; count = 2; }
  else if (first >= 0xf0 && first <= 0xf4) { code = first & 0x07; count = 3; }
  else if (first >= 0x80) throw std::runtime_error("PNML marking contains invalid UTF-8");
  for (unsigned i = 0; i < count; ++i) {
    if (position == value.size()) throw std::runtime_error("PNML marking contains invalid UTF-8");
    const auto next = static_cast<unsigned char>(value[position++]);
    if ((next & 0xc0) != 0x80) throw std::runtime_error("PNML marking contains invalid UTF-8");
    code = (code << 6) | (next & 0x3f);
  }
  if ((count == 1 && code < 0x80) || (count == 2 && code < 0x800) ||
      (count == 3 && code < 0x10000) || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
    throw std::runtime_error("PNML marking contains invalid UTF-8");
  return code;
}

// Python repr() of a str; int() messages cut it to 200 code points (%.200R),
// float() messages keep it whole.
std::string pnml_integer_literal(const std::string& value, std::size_t limit = 200) {
  const char quote = value.find('\'') != std::string::npos && value.find('"') == std::string::npos ? '"' : '\'';
  std::string result(1, quote);
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t position = 0; position < value.size();) {
    const auto start = position;
    const auto code = pnml_codepoint(value, position);
    if (code == '\\' || code == static_cast<unsigned char>(quote)) {
      result += '\\'; result += static_cast<char>(code);
    } else if (code == '\n') result += "\\n";
    else if (code == '\r') result += "\\r";
    else if (code == '\t') result += "\\t";
    else if (!root_cause_unicode_printable(code)) {
      const int digits = code <= 0xff ? 2 : code <= 0xffff ? 4 : 8;
      result += digits == 2 ? "\\x" : digits == 4 ? "\\u" : "\\U";
      for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4)
        result += hex[(code >> shift) & 15];
    } else result.append(value, start, position - start);
  }
  result += quote;
  std::size_t end = 0;
  for (std::size_t count = 0; end < result.size() && count < limit; ++count)
    (void)pnml_codepoint(result, end);
  result.resize(end);
  return result;
}

TokenInteger parse_token_count(const std::string& text, bool public_markings) {
  PetriArcWeight value;
  try {
    value = detail::parse_pnml_arc_weight(text);
    if (value.negative && !public_markings)
      throw std::runtime_error("negative PNML token count");
  } catch (const std::exception&) {
    if (public_markings)
      throw std::runtime_error("invalid literal for int() with base 10: " + pnml_integer_literal(text));
    throw std::runtime_error("PNML marking contains an invalid token count");
  }
  if (public_markings) {
    std::size_t digits = 0;
    for (std::size_t position = 0; position < text.size();)
      if (detail::pnml_decimal_digit(pnml_codepoint(text, position)) >= 0) ++digits;
    if (digits > 4300)
      throw std::runtime_error("Exceeds the limit (4300 digits) for integer string conversion: value has " +
          std::to_string(digits) + " digits; use sys.set_int_max_str_digits() to increase the limit");
  }
  return value.negative ? -value.magnitude : value.magnitude;
}

// A stochastic priority property read as int(text): the int() grammar and
// messages of public markings. The native int range is checked only when a
// stochastic map is built.
TokenInteger parse_stochastic_priority(const std::string& text) {
  if (text.empty())
    throw std::runtime_error("int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
  return parse_token_count(text, true);
}

// A stochastic activation weight read as float(text). Non-finite and negative
// weights still reach the stochastic-map validation.
double parse_stochastic_weight(const std::string& text) {
  if (text.empty())
    throw std::runtime_error("float() argument must be a string or a real number, not 'NoneType'");
  const auto value = detail::parse_python_float_text(text);
  if (!value)
    throw std::runtime_error("could not convert string to float: " +
                             pnml_integer_literal(text, std::numeric_limits<std::size_t>::max()));
  return *value;
}

// float(text) over a present string: an empty one raises too.
double python_float(const std::string& text) {
  const auto value = detail::parse_python_float_text(text);
  if (!value)
    throw std::runtime_error("could not convert string to float: " +
                             pnml_integer_literal(text, std::numeric_limits<std::size_t>::max()));
  return *value;
}

std::vector<std::string> split_semicolons(const std::string& text) {
  std::vector<std::string> parts(1);
  for (const char ch : text) {
    if (ch == ';') parts.emplace_back();
    else parts.back() += ch;
  }
  return parts;
}

constexpr const char* kStochasticParameterPolicy =
    "PNML stochastic parameters must be finite numbers for a native stochastic map";

// The record of one StochasticPetriNet toolspecific, finished at its end tag
// as PM4Py finishes it: only the exact type names IMMEDIATE, DETERMINISTIC,
// NORMAL, UNIFORM, LOGNORMAL, GAMMA and EXPONENTIAL define a random variable,
// and each reads distributionParameters its own way. IMMEDIATE ignores them;
// DETERMINISTIC keeps the text; NORMAL and UNIFORM keep the first two of its
// ';'-separated parts (None has no split(), fewer than two parts raise
// IndexError); LOGNORMAL and GAMMA convert the first three parts with
// float() in order; EXPONENTIAL converts the whole text, divides 1 by it
// (zero raises ZeroDivisionError) and keeps 1 / rate for a positive rate,
// else 'UNDEFINED'. Text that PM4Py keeps but a native map cannot hold as
// finite numbers (and an undefined rate) is a native policy error, raised
// only when a stochastic map is imported.
void finish_stochastic_record(ParsedTransition& transition) {
  if (!transition.stochastic) return;
  const auto& properties = *transition.stochastic;
  transition.record.reset();
  transition.policy.clear();
  if (!properties.type) return;
  const auto& type = *properties.type;
  StochasticMapTransition record;
  const auto& parameters = properties.parameters;
  const auto numeric = [&](const std::string& text, double& target) {
    const auto value = detail::parse_python_float_text(text);
    if (!value || !std::isfinite(*value)) transition.policy = kStochasticParameterPolicy;
    else target = *value;
  };
  const auto parts = [&](std::size_t count) {
    if (!parameters) throw std::runtime_error("'NoneType' object has no attribute 'split'");
    auto result = split_semicolons(*parameters);
    if (result.size() < count) throw std::runtime_error("list index out of range");
    return result;
  };
  if (type == "IMMEDIATE") {
    record.timing = StochasticMapTiming::kImmediate;
    record.distribution = StochasticMapDistribution::kImmediate;
    record.parameter = 0.0;
  } else if (type == "DETERMINISTIC") {
    record.distribution = StochasticMapDistribution::kDeterministic;
    if (!parameters) transition.policy = kStochasticParameterPolicy;
    else numeric(*parameters, record.parameter);
  } else if (type == "NORMAL" || type == "UNIFORM") {
    record.distribution = type == "NORMAL" ? StochasticMapDistribution::kNormal
                                           : StochasticMapDistribution::kUniform;
    const auto values = parts(2);
    numeric(values[0], record.parameter);
    numeric(values[1], record.secondary_parameter);
  } else if (type == "LOGNORMAL" || type == "GAMMA") {
    record.distribution = type == "GAMMA" ? StochasticMapDistribution::kGamma
                                          : StochasticMapDistribution::kLogNormal;
    if (!parameters) throw std::runtime_error("'NoneType' object has no attribute 'split'");
    const auto values = split_semicolons(*parameters);
    double* targets[] = {&record.parameter, &record.secondary_parameter, &record.tertiary_parameter};
    for (std::size_t index = 0; index < 3; ++index) {
      if (index >= values.size()) throw std::runtime_error("list index out of range");
      *targets[index] = python_float(values[index]);
    }
    if (!std::isfinite(record.parameter) || !std::isfinite(record.secondary_parameter) ||
        !std::isfinite(record.tertiary_parameter))
      transition.policy = kStochasticParameterPolicy;
  } else if (type == "EXPONENTIAL") {
    record.distribution = StochasticMapDistribution::kExponential;
    if (!parameters)
      throw std::runtime_error("float() argument must be a string or a real number, not 'NoneType'");
    const double value = python_float(*parameters);
    if (value == 0.0) throw std::runtime_error("division by zero");
    const double rate = 1.0 / value;
    if (rate > 0.0 && std::isfinite(1.0 / rate)) record.parameter = 1.0 / rate;
    else transition.policy = kStochasticParameterPolicy;
  } else {
    return;
  }
  if (properties.weight) record.activation_weight = *properties.weight;
  if (properties.priority) {
    if (*properties.priority < TokenInteger(std::numeric_limits<int>::min()) ||
        *properties.priority > TokenInteger(std::numeric_limits<int>::max())) {
      if (transition.policy.empty())
        transition.policy = "PNML stochastic priority is outside the native integer range";
    } else {
      record.priority = static_cast<int>(*properties.priority);
    }
  }
  transition.record = record;
}

bool name_ends_with(const std::string& name, std::string_view suffix) {
  return name.size() >= suffix.size() &&
         name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string ascii_lower(std::string text) {
  for (auto& ch : text)
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  return text;
}

// True when `stack` (element names from the document root down) ends with the
// given path.
bool stack_ends_with(const std::vector<std::string>& stack,
                     std::initializer_list<const char*> path) {
  if (stack.size() < path.size()) return false;
  auto it = stack.end() - static_cast<std::ptrdiff_t>(path.size());
  for (const char* name : path) {
    if (*it++ != name) return false;
  }
  return true;
}

}  // namespace

std::optional<std::vector<std::vector<std::size_t>>>
pnml_place_outgoing_transition_order(const PnmlDocument& document) {
  if (!document.arc_occurrences) return std::nullopt;
  const auto& net = document.net;
  std::vector<std::vector<std::size_t>> order(net.place_count());
  for (const auto& arc : *document.arc_occurrences) {
    if (!arc.input) continue;
    if (arc.place >= order.size() || arc.transition >= net.transitions().size())
      return std::nullopt;
    order[arc.place].push_back(arc.transition);
  }
  // Every adjacent transition must be listed, and nothing else.
  for (std::size_t place = 0; place < order.size(); ++place) {
    std::set<std::size_t> listed(order[place].begin(), order[place].end());
    std::set<std::size_t> adjacent;
    for (std::size_t t = 0; t < net.transitions().size(); ++t) {
      const auto& transition = net.transitions()[t];
      for (const auto* inputs : {&transition.inputs, &transition.reset_inputs,
                                 &transition.inhibitor_inputs})
        if (std::find(inputs->begin(), inputs->end(), place) != inputs->end())
          adjacent.insert(t);
    }
    if (listed != adjacent) return std::nullopt;
  }
  return order;
}

PnmlDocument parse_pnml_document(std::string_view xml,
                                 const PnmlImportOptions& options) {
  // Normalize physical XML line endings before entity decoding. Character
  // references to CR retain their value and their distinct error rendering.
  std::string normalized_xml;
  if (options.public_marking_semantics) {
    normalized_xml.reserve(xml.size());
    for (std::size_t i = 0; i < xml.size(); ++i) {
      if (xml[i] == '\r') {
        normalized_xml += '\n';
        if (i + 1 < xml.size() && xml[i + 1] == '\n') ++i;
      } else normalized_xml += xml[i];
    }
    xml = normalized_xml;
  }
  detail::XmlPullParser parser(xml, /*emit_character_data=*/true,
                              /*coalesce_character_data=*/true);

  std::vector<ParsedPlace> places;
  std::vector<ParsedTransition> transitions;
  std::vector<ParsedArc> arcs;
  std::unordered_map<std::string, std::size_t> place_index;
  std::unordered_map<std::string, std::size_t> transition_index;

  // Both modes visit every <marking> under <finalmarkings> and merge them, as
  // the reference importer does: a later marking replaces an earlier count
  // for the same place. Native document mode still adds repeated entries of
  // one place within a single marking; public marking mode keeps only each
  // place's last positive count.
  std::vector<std::pair<std::string, TokenInteger>> final_marking;
  std::vector<std::size_t> final_marking_source;  // marking index per entry
  std::vector<bool> final_marking_text;  // whether the entry carried a count
  std::size_t markings_seen = 0;
  bool final_markings_present = false;
  const bool public_markings = options.public_marking_semantics;
  const bool require_boundaries = options.require_workflow_boundaries && !public_markings;

  std::vector<std::string> stack;
  // The stack depth of an open StochasticPetriNet toolspecific.
  std::optional<std::size_t> stochastic_depth;
  bool in_net = false;
  bool nets_seen = false;
  std::string final_place_ref;  // idref of the <place> open under <marking>

  const auto open = [&](const detail::XmlPullParser::Token& token) {
    if (token.name == "net") {
      if (nets_seen) return false;  // only the first net is read
      nets_seen = true;
      in_net = true;
      return true;
    }
    if (!in_net) return true;

    if (token.name == "place" && !stack_ends_with(stack, {"marking"})) {
      const auto* id = detail::attribute(token.attributes, "id");
      if (!id) throw std::runtime_error("PNML place is missing its id");
      if (!place_index.emplace(*id, places.size()).second) {
        throw std::runtime_error("PNML has a duplicate place id '" + *id + "'");
      }
      places.push_back({*id, 0});
    } else if (token.name == "place") {
      // A <place idref=…> inside <finalmarkings><marking>.
      if (markings_seen >= 1) {
        const auto* idref = detail::attribute(token.attributes, "idref");
        if (idref) {
          final_place_ref = *idref;
          // A marking entry with no nested <text> carries no count; the
          // reference importer ignores it.
          final_marking.emplace_back(*idref, 0);
          final_marking_source.push_back(markings_seen);
          final_marking_text.push_back(false);
        }
      }
    } else if (token.name == "initialMarking" &&
               stack_ends_with(stack, {"place"}) && !places.empty()) {
      places.back().has_initial_marking = true;
    } else if (token.name == "transition") {
      const auto* id = detail::attribute(token.attributes, "id");
      if (!id) throw std::runtime_error("PNML transition is missing its id");
      if (!transition_index.emplace(*id, transitions.size()).second) {
        throw std::runtime_error("PNML has a duplicate transition id '" + *id +
                                 "'");
      }
      transitions.push_back({*id, *id, false});
    } else if (token.name == "arc") {
      const auto* source = detail::attribute(token.attributes, "source");
      const auto* target = detail::attribute(token.attributes, "target");
      if (!source || !target) {
        throw std::runtime_error("PNML arc is missing its source or target");
      }
      arcs.push_back({*source, *target, PetriArcType::kNormal});
      if (const auto* id = detail::attribute(token.attributes, "id")) arcs.back().id = *id;
    } else if (name_ends_with(token.name, "toolspecific") &&
               stack_ends_with(stack, {"transition"})) {
      // As PM4Py reads a transition's toolspecific children (any tag ending
      // in "toolspecific"): a tool containing "ProM" may mark the transition
      // invisible; otherwise one containing "StochasticPetriNet" starts a new
      // stochastic record; other tools are ignored with their properties.
      const auto* tool = detail::attribute(token.attributes, "tool");
      if (!tool)
        throw std::runtime_error("argument of type 'NoneType' is not a container or iterable");
      const auto* activity = detail::attribute(token.attributes, "activity");
      if (tool->find("ProM") != std::string::npos) {
        if (!activity)
          throw std::runtime_error("argument of type 'NoneType' is not a container or iterable");
        if (activity->find("$invisible$") != std::string::npos && !transitions.empty())
          transitions.back().invisible = true;
      } else if (tool->find("StochasticPetriNet") != std::string::npos && !transitions.empty()) {
        transitions.back().stochastic = ParsedStochasticProperties{};
        if (token.self_closing) finish_stochastic_record(transitions.back());
        else stochastic_depth = stack.size() + 1;
      }
    } else if (token.name == "finalmarkings") {
      final_markings_present = true;
    } else if (token.name == "marking" &&
               stack_ends_with(stack, {"finalmarkings"})) {
      ++markings_seen;
    }
    return true;
  };

  // A stochastic property is any child element of a StochasticPetriNet
  // toolspecific with a key. It reads its element text (the text before any
  // child), as the public importer does: an empty element is a missing
  // value, and weight/priority/invisible convert like float(), int() and
  // .lower() == "true" in document order; a later property of the same key
  // replaces an earlier one.
  struct PropertyText { std::string key; std::size_t depth; std::string value; bool first_segment = true; };
  std::optional<PropertyText> property_text;
  struct ArcText { bool inscription; std::size_t depth; std::string value; bool first_segment = true; };
  std::optional<ArcText> arc_text;
  struct LabelText { std::size_t depth; std::string value; bool first_segment = true; };
  std::optional<LabelText> label_text;
  struct MarkingText { bool initial; std::size_t depth; std::string value; bool first_segment = true; };
  std::optional<MarkingText> marking_text;
  const auto finish_marking_text = [&] {
    if (marking_text->value.empty()) {
      if (public_markings)
        throw std::runtime_error("int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
      // Native mode reads an empty final-marking <text/> as one token (the
      // reference cannot read it at all).
      if (!marking_text->initial) {
        final_marking.back().second = 1;
        final_marking_text.back() = true;
      }
      marking_text.reset();
      return;
    }
    const auto count = parse_token_count(marking_text->value, public_markings);
    if (marking_text->initial) places.back().initial_tokens = count;
    else {
      final_marking.back().second = count;
      final_marking_text.back() = true;
    }
    marking_text.reset();
  };
  const auto finish_property_text = [&] {
    auto& transition = transitions.back();
    if (!transition.stochastic) {
      property_text.reset();
      return;
    }
    auto& properties = *transition.stochastic;
    const auto& key = property_text->key;
    const auto& value = property_text->value;
    const auto text = value.empty() ? std::nullopt : std::optional<std::string>(value);
    if (key == "distributionType") properties.type = text;
    else if (key == "distributionParameters") properties.parameters = text;
    else if (key == "priority") properties.priority = parse_stochastic_priority(value);
    else if (key == "weight") properties.weight = parse_stochastic_weight(value);
    else if (key == "invisible") {
      if (value.empty()) throw std::runtime_error("'NoneType' object has no attribute 'lower'");
      if (ascii_lower(value) == "true") transition.invisible = true;
    }
    property_text.reset();
  };
  const auto finish_label_text = [&] {
    if (!label_text->value.empty() && !transitions.back().has_named_label) {
      transitions.back().label = std::move(label_text->value);
      transitions.back().has_named_label = true;
    }
    label_text.reset();
  };
  const auto finish_arc_text = [&] {
    if (arc_text->inscription) {
      const auto weight = detail::parse_pnml_arc_weight(arc_text->value);
      arcs.back().weight = weight.magnitude;
      arcs.back().negative_weight = weight.negative;
    } else {
      arcs.back().type = arc_text->value == "reset" ? PetriArcType::kReset
          : arc_text->value == "inhibitor" ? PetriArcType::kInhibitor : PetriArcType::kNormal;
    }
    arc_text.reset();
  };
  const auto text = [&](const std::string& value) {
    if (!in_net) return;
    if (stack_ends_with(stack, {"place", "initialMarking", "text"}) &&
        !places.empty()) {
      places.back().initial_tokens = parse_token_count(value, public_markings);
    } else if (stack_ends_with(stack, {"marking", "place", "text"}) &&
               markings_seen >= 1 && !final_marking.empty() &&
               final_marking.back().first == final_place_ref) {
      final_marking.back().second = parse_token_count(value, public_markings);
      final_marking_text.back() = true;
    }
  };

  while (true) {
    auto token = parser.next();
    if (const auto colon = token.name.find(':'); colon != std::string::npos)
      token.name.erase(0, colon + 1);
    if (token.kind == detail::XmlPullParser::Kind::EndOfInput) break;
    if (token.kind == detail::XmlPullParser::Kind::CharacterData) {
      if (marking_text) {
        if (marking_text->first_segment && stack.size() == marking_text->depth)
          marking_text->value += token.text;
        continue;
      }
      if (label_text) {
        if (label_text->first_segment && stack.size() == label_text->depth)
          label_text->value += token.text;
        continue;
      }
      if (property_text) {
        if (property_text->first_segment && stack.size() == property_text->depth)
          property_text->value += token.text;
        continue;
      }
      if (arc_text) {
        if (arc_text->first_segment && stack.size() == arc_text->depth)
          arc_text->value += token.text;
        continue;
      }
      text(token.text);
      continue;
    }
    if (token.kind == detail::XmlPullParser::Kind::ProcessingInstruction) {
      if (marking_text) marking_text->first_segment = false;
      if (arc_text) arc_text->first_segment = false;
      if (label_text) label_text->first_segment = false;
      if (property_text) property_text->first_segment = false;
      continue;
    }
    if (token.kind == detail::XmlPullParser::Kind::StartElement) {
      if (marking_text) marking_text->first_segment = false;
      if (in_net && token.name == "text" &&
          ((stack_ends_with(stack, {"place", "initialMarking"}) && !places.empty()) ||
           (stack_ends_with(stack, {"marking", "place"}) &&
            markings_seen >= 1 && !final_marking.empty() &&
            final_marking.back().first == final_place_ref))) {
        marking_text = MarkingText{stack.back() == "initialMarking", stack.size() + 1, {}};
        if (token.self_closing) finish_marking_text();
      }
      if (arc_text) arc_text->first_segment = false;
      if (label_text) label_text->first_segment = false;
      if (in_net && !transitions.empty() && !transitions.back().has_named_label &&
          token.name == "text" && stack_ends_with(stack, {"transition", "name"})) {
        label_text = LabelText{stack.size() + 1, {}};
        if (token.self_closing) finish_label_text();
      }
      if (in_net && !arcs.empty() && token.name == "text" &&
          (stack_ends_with(stack, {"arc", "arctype"}) || stack_ends_with(stack, {"arc", "inscription"}))) {
        arc_text = ArcText{stack.back() == "inscription", stack.size() + 1, {}};
        if (token.self_closing) finish_arc_text();
      }
      if (property_text) property_text->first_segment = false;
      if (in_net && !transitions.empty() && stochastic_depth &&
          stack.size() == *stochastic_depth) {
        const auto* key = detail::attribute(token.attributes, "key");
        if (key && !key->empty()) {
          property_text = PropertyText{*key, stack.size() + 1, {}};
          if (token.self_closing) finish_property_text();
        }
      }
      const bool entered = open(token);
      if (!token.self_closing) stack.push_back(token.name);
      if (!entered) {
        // A second <net>: skip it wholesale by unwinding to its end tag.
        std::size_t depth = token.self_closing ? 0 : 1;
        while (depth > 0) {
          const auto skipped = parser.next();
          if (skipped.kind == detail::XmlPullParser::Kind::EndOfInput) break;
          if (skipped.kind == detail::XmlPullParser::Kind::StartElement &&
              !skipped.self_closing) {
            ++depth;
          } else if (skipped.kind == detail::XmlPullParser::Kind::EndElement) {
            --depth;
          }
        }
        if (!stack.empty()) stack.pop_back();
      }
      continue;
    }
    // EndElement: unwind to the matching open element.
    if (marking_text && token.name == "text" && stack.size() == marking_text->depth)
      finish_marking_text();
    if (arc_text && token.name == "text" && stack.size() == arc_text->depth)
      finish_arc_text();
    if (label_text && token.name == "text" && stack.size() == label_text->depth)
      finish_label_text();
    if (property_text && stack.size() == property_text->depth && stack.back() == token.name)
      finish_property_text();
    if (stochastic_depth && stack.size() == *stochastic_depth && stack.back() == token.name) {
      finish_stochastic_record(transitions.back());
      stochastic_depth.reset();
    }
    while (!stack.empty() && stack.back() != token.name) stack.pop_back();
    if (!stack.empty()) stack.pop_back();
    if (token.name == "net") in_net = false;
  }

  // A net with places but no transitions is a net: pm4py's read_pnml reads
  // the source/sink net that convert_to_petri_net gives for a DFG without
  // edges (activity-defines-place), and write_pnml writes it.
  if (!nets_seen || (require_boundaries && places.empty())) {
    throw std::runtime_error("PNML input contains no net (or an empty one)");
  }

  // Assemble the net: visible transitions with the same label share an
  // activity, in first-seen order.
  PetriNetLiteralStorage storage;
  storage.place_count = places.size();
  storage.transitions.reserve(transitions.size());

  std::vector<std::string> activities;
  std::unordered_map<std::string, std::size_t> activity_ids;
  std::vector<std::vector<std::size_t>> inputs(transitions.size());
  std::vector<std::vector<std::size_t>> outputs(transitions.size());
  std::vector<std::vector<std::size_t>> reset_inputs(transitions.size());
  std::vector<std::vector<std::size_t>> inhibitor_inputs(transitions.size());
  std::vector<std::vector<PetriArcWeight>> input_weights(transitions.size());
  std::vector<std::vector<PetriArcWeight>> output_weights(transitions.size());
  std::vector<std::vector<PetriArcWeight>> reset_weights(transitions.size());
  std::vector<std::vector<PetriArcWeight>> inhibitor_weights(transitions.size());
  std::vector<bool> place_has_in(places.size(), false);
  std::vector<bool> place_has_out(places.size(), false);

  // Preserve the historical small positive multiplicities. Any zero, signed or
  // large ordinary arc switches its whole transition to occurrence weights.
  constexpr std::size_t materialized_weight_limit = 1024;
  std::vector<bool> compact(transitions.size(),false);
  for (const auto& arc : arcs) if (arc.type == PetriArcType::kNormal &&
      (!arc.weight || arc.negative_weight || arc.weight > materialized_weight_limit)) {
    const auto source = transition_index.find(arc.source);
    const auto target = transition_index.find(arc.target);
    if (source != transition_index.end()) compact[source->second] = true;
    if (target != transition_index.end()) compact[target->second] = true;
  }
  // Parallel arcs stay distinct arcs; repeated units would merge them into one.
  std::set<std::pair<std::string, std::string>> arc_endpoints;
  for (const auto& arc : arcs) if (arc.type == PetriArcType::kNormal &&
      !arc_endpoints.emplace(arc.source, arc.target).second) {
    const auto source = transition_index.find(arc.source);
    const auto target = transition_index.find(arc.target);
    if (source != transition_index.end()) compact[source->second] = true;
    if (target != transition_index.end()) compact[target->second] = true;
  }

  std::vector<PnmlArcOccurrence> arc_occurrences;
  for (const auto& arc : arcs) {
    const auto from_place = place_index.find(arc.source);
    const auto to_place = place_index.find(arc.target);
    const auto from_transition = transition_index.find(arc.source);
    const auto to_transition = transition_index.find(arc.target);
    if (from_place != place_index.end() &&
        to_transition != transition_index.end()) {
      if (arc.type == PetriArcType::kNormal) {
        if (compact[to_transition->second]) {
          inputs[to_transition->second].push_back(from_place->second);
          input_weights[to_transition->second].push_back({arc.weight,arc.negative_weight});
        } else inputs[to_transition->second].insert(
            inputs[to_transition->second].end(), static_cast<std::size_t>(arc.weight),
            from_place->second);
      }
      else if (arc.type == PetriArcType::kReset) {
        reset_inputs[to_transition->second].push_back(from_place->second);
        reset_weights[to_transition->second].push_back({arc.weight, arc.negative_weight});
      } else {
        inhibitor_inputs[to_transition->second].push_back(from_place->second);
        inhibitor_weights[to_transition->second].push_back({arc.weight, arc.negative_weight});
      }
      place_has_out[from_place->second] = true;
      arc_occurrences.push_back({from_place->second, to_transition->second, true,
                                 arc.type, arc.weight, arc.id, arc.negative_weight});
    } else if (from_transition != transition_index.end() &&
               to_place != place_index.end()) {
      if (arc.type != PetriArcType::kNormal)
        throw std::runtime_error("PNML extended arc must enter a transition");
      if (compact[from_transition->second]) {
        outputs[from_transition->second].push_back(to_place->second);
        output_weights[from_transition->second].push_back({arc.weight,arc.negative_weight});
      } else outputs[from_transition->second].insert(
          outputs[from_transition->second].end(), static_cast<std::size_t>(arc.weight),
          to_place->second);
      place_has_in[to_place->second] = true;
      arc_occurrences.push_back({to_place->second, from_transition->second, false,
                                 arc.type, arc.weight, arc.id,arc.negative_weight});
    } else {
      throw std::runtime_error("PNML arc '" + arc.source + "' -> '" +
                               arc.target +
                               "' does not connect a place and a transition");
    }
  }

  for (std::size_t t = 0; t < transitions.size(); ++t) {
    const auto& parsed = transitions[t];
    std::optional<std::size_t> activity;
    if (!parsed.invisible) {
      const auto [it, inserted] =
          activity_ids.emplace(parsed.label, activities.size());
      if (inserted) activities.push_back(parsed.label);
      activity = it->second;
    }
    // The parsed document owns distinct typed arc objects. Incremental net
    // mutators intentionally reject mixed/parallel extended arcs, but those
    // editing restrictions must not discard a valid imported graph.
    storage.transitions.push_back(PetriTransition{activity, std::move(inputs[t]),
        std::move(outputs[t]), std::move(reset_inputs[t]), std::move(inhibitor_inputs[t]),
        std::move(reset_weights[t]), std::move(inhibitor_weights[t]),
        std::move(input_weights[t]), std::move(output_weights[t])});
  }
  storage.activities = std::move(activities);
  // The reference names each imported place by its PNML ID; carrying them on
  // the net keeps replay tie-breaks after copies and legacy re-export.
  storage.place_names.emplace();
  for (const auto& place : places) storage.place_names->push_back(place.id);
  PetriNet net = PetriNet::from_literal_storage(std::move(storage));

  PnmlDocument document;
  document.arc_occurrences = std::move(arc_occurrences);
  for (const auto& place : places) document.place_ids.push_back(place.id);
  for (const auto& transition : transitions) document.transition_ids.push_back(transition.id);
  document.initial_marking.assign(places.size(), 0);
  document.final_marking.assign(places.size(), 0);
  document.initial_marking_keys.assign(places.size(), false);
  document.final_marking_keys.assign(places.size(), false);
  std::vector<std::size_t> marked;
  for (std::size_t p = 0; p < places.size(); ++p) {
    document.initial_marking[p] = public_markings && places[p].initial_tokens <= 0
        ? TokenInteger{0} : places[p].initial_tokens;
    document.initial_marking_keys[p] = public_markings
        ? places[p].initial_tokens > 0 : places[p].has_initial_marking;
    if (places[p].initial_tokens > 0) marked.push_back(p);
  }
  if (!marked.empty()) {
    net.set_source(marked.front());
  } else if (options.infer_initial_marking && !public_markings) {
    std::vector<std::size_t> sources;
    for (std::size_t p = 0; p < places.size(); ++p) {
      if (!place_has_in[p]) sources.push_back(p);
    }
    if (sources.size() != 1) {
      throw std::runtime_error(
          "PNML has no initial marking and no unique source place");
    }
    net.set_source(sources.front());
    document.initial_marking[sources.front()] = 1;
    document.initial_marking_keys[sources.front()] = true;
  } else {
    std::vector<std::size_t> sources;
    for (std::size_t p = 0; p < places.size(); ++p)
      if (!place_has_in[p]) sources.push_back(p);
    if (sources.size() != 1 && require_boundaries)
      throw std::runtime_error("PNML has no unique structural source place");
    if (!sources.empty()) net.set_source(sources.front());
  }

  if (!final_marking.empty()) {
    std::vector<std::size_t> place_source(places.size(), 0);
    for (std::size_t entry = 0; entry < final_marking.size(); ++entry) {
      const auto& [id, tokens] = final_marking[entry];
      const auto it = place_index.find(id);
      if (it == place_index.end()) {
        throw std::runtime_error("PNML final marking references unknown place '" +
                                 id + "'");
      }
      if (public_markings && tokens <= 0) continue;
      // Like the reference, an entry without a count adds nothing, and a
      // non-positive count from a later alternative never replaces an
      // earlier positive one (the reference stores only counts above zero).
      if (!final_marking_text[entry]) continue;
      const auto source = final_marking_source[entry];
      if (!public_markings && tokens <= 0 && place_source[it->second] != 0 &&
          place_source[it->second] != source &&
          document.final_marking[it->second] > 0)
        continue;
      if (public_markings || place_source[it->second] != source)
        document.final_marking[it->second] = tokens;
      else
        document.final_marking[it->second] += tokens;
      place_source[it->second] = source;
      document.final_marking_keys[it->second] = true;
    }
    const auto first = std::find_if(
        document.final_marking.begin(), document.final_marking.end(),
        [](const TokenInteger& tokens) { return tokens > 0; });
    if (first == document.final_marking.end() && require_boundaries) {
      throw std::runtime_error("PNML final marking contains no tokens");
    }
    if (first != document.final_marking.end())
      net.set_sink(static_cast<std::size_t>(first - document.final_marking.begin()));
  } else if (public_markings) {
    // A missing finalmarkings element infers all sinks, including isolated
    // places. An explicitly empty element preserves an empty final marking.
    for (std::size_t p = 0; p < places.size(); ++p) if (!place_has_out[p]) {
      if (!final_markings_present) {
        document.final_marking[p] = 1;
        document.final_marking_keys[p] = true;
      }
      net.set_sink(p);
    }
  } else if (options.infer_final_marking) {
    std::vector<std::size_t> sinks;
    for (std::size_t p = 0; p < places.size(); ++p) {
      if (!place_has_out[p]) sinks.push_back(p);
    }
    if (sinks.size() != 1) {
      throw std::runtime_error(
          "PNML has no final marking and no unique sink place");
    }
    net.set_sink(sinks.front());
    document.final_marking[sinks.front()] = 1;
    document.final_marking_keys[sinks.front()] = true;
  } else {
    std::vector<std::size_t> sinks;
    for (std::size_t p = 0; p < places.size(); ++p)
      if (!place_has_out[p]) sinks.push_back(p);
    if (sinks.size() != 1 && require_boundaries)
      throw std::runtime_error("PNML has no unique structural sink place");
    if (!sinks.empty()) net.set_sink(sinks.front());
  }

  if (options.import_stochastic_map) {
    StochasticMap map;
    map.transitions.reserve(transitions.size());
    bool any = false;
    // Transitions without a defined record keep the native defaults.
    for (std::size_t index = 0; index < transitions.size(); ++index) {
      const auto& parsed = transitions[index];
      if (!parsed.policy.empty()) throw std::runtime_error(parsed.policy);
      StochasticMapTransition record;
      if (parsed.record) {
        any = true;
        record = *parsed.record;
      }
      record.transition = index;
      map.transitions.push_back(record);
    }
    if (any) {
      validate_stochastic_map_structure(map, transitions.size());
      document.stochastic_map = std::move(map);
    }
  }

  document.net = std::move(net);
  return document;
}

PetriNet parse_pnml(std::string_view xml) {
  auto document = parse_pnml_document(xml);
  const auto initial_tokens = std::accumulate(document.initial_marking.begin(),
                                              document.initial_marking.end(),
                                              TokenInteger{0});
  const auto final_tokens = std::accumulate(document.final_marking.begin(),
                                            document.final_marking.end(),
                                            TokenInteger{0});
  if (initial_tokens != 1 || document.initial_marking[document.net.source()] != 1) {
    throw std::runtime_error(
        "PNML initial marking is not a single token on one place; only "
        "workflow nets can be checked");
  }
  if (final_tokens != 1 || document.final_marking[document.net.sink()] != 1) {
    throw std::runtime_error(
        "PNML final marking is not a single token on one place; only "
        "workflow nets can be checked");
  }
  return std::move(document.net);
}

PnmlDocument import_pnml_document(const std::filesystem::path& path,
                                  const PnmlImportOptions& options) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("Unable to open PNML file: " + path.string());
  }
  std::string xml((std::istreambuf_iterator<char>(input)),
                  std::istreambuf_iterator<char>());
  if (xml.size() >= 3 && static_cast<unsigned char>(xml[0]) == 0xEF &&
      static_cast<unsigned char>(xml[1]) == 0xBB &&
      static_cast<unsigned char>(xml[2]) == 0xBF) {
    xml.erase(0, 3);
  }
  return parse_pnml_document(xml, options);
}

PetriNet import_pnml(const std::filesystem::path& path) {
  auto document = import_pnml_document(path);
  const auto initial_tokens = std::accumulate(document.initial_marking.begin(),
                                              document.initial_marking.end(),
                                              TokenInteger{0});
  const auto final_tokens = std::accumulate(document.final_marking.begin(),
                                            document.final_marking.end(),
                                            TokenInteger{0});
  if (initial_tokens != 1 || document.initial_marking[document.net.source()] != 1 ||
      final_tokens != 1 || document.final_marking[document.net.sink()] != 1) {
    throw std::runtime_error(
        "PNML markings do not describe a single-token workflow net");
  }
  return std::move(document.net);
}

PnmlDocument process_tree_pnml_document(const ProcessTree& tree,
                                        ProcessTreePetriNetVariant variant) {
  PnmlDocument document;
  if (variant == ProcessTreePetriNetVariant::kStandard) {
    auto conversion = convert_process_tree_to_petri_net(tree);
    document.net = std::move(conversion.net);
    document.place_ids = std::move(conversion.place_names);
    document.transition_ids = std::move(conversion.transition_names);
  } else {
    document.net = to_petri_net(tree, variant);
  }
  const auto places = document.net.place_count();
  document.initial_marking.assign(places, TokenInteger(0));
  document.final_marking.assign(places, TokenInteger(0));
  if (places != 0) {
    document.initial_marking[document.net.source()] = TokenInteger(1);
    document.final_marking[document.net.sink()] = TokenInteger(1);
  }
  return document;
}

}  // namespace minera
