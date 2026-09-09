#include "minera/pnml.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <numeric>
#include <optional>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "xml_pull_parser.hpp"

namespace minera {
namespace {

// Escapes the five XML metacharacters so activity names containing '&', '<',
// quotes, etc. produce well-formed PNML.
void write_xml_escaped(std::ostream& out, std::string_view value) {
  for (const char c : value) {
    switch (c) {
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

void write_pnml_impl(std::ostream& out, const PetriNet& net,
                     const std::vector<std::int64_t>& initial_marking,
                     const std::vector<std::int64_t>& final_marking,
                     const StochasticMap* stochastic_map,
                     std::string_view name) {
  if (initial_marking.size() != net.place_count() ||
      final_marking.size() != net.place_count())
    throw std::invalid_argument("PNML marking size must match the place count");
  if (std::any_of(initial_marking.begin(), initial_marking.end(),
                  [](std::int64_t value) { return value < 0; }) ||
      std::any_of(final_marking.begin(), final_marking.end(),
                  [](std::int64_t value) { return value < 0; }))
    throw std::invalid_argument("PNML markings must be non-negative");
  if (stochastic_map)
    validate_stochastic_map(*stochastic_map, net.transitions().size());
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<pnml xmlns=\"http://www.pnml.org/version-2009/grammar/pnml\">\n";
  out << "  <net id=\"net1\" "
         "type=\"http://www.pnml.org/version-2009/grammar/pnmlcoremodel\">\n";
  out << "    <name><text>";
  write_xml_escaped(out, name);
  out << "</text></name>\n";
  out << "    <page id=\"page1\">\n";

  // Places. The source carries the single initial token.
  for (std::size_t place = 0; place < net.place_count(); ++place) {
    out << "      <place id=\"p" << place << "\">\n";
    out << "        <name><text>p" << place << "</text></name>\n";
    if (initial_marking[place] != 0)
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
    out << "      <transition id=\"t" << t << "\">\n";
    out << "        <name><text>";
    if (transition.silent()) {
      out << "tau";
    } else {
      write_xml_escaped(out, activities[*transition.activity]);
    }
    out << "</text></name>\n";
    if (transition.silent()) {
      out << "        <toolspecific tool=\"ProM\" version=\"6.4\" "
             "activity=\"$invisible$\" localNodeID=\"t"
          << t << "\"/>\n";
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
      if (type != "IMMEDIATE") {
        out << "          <property key=\"distributionParameters\">"
            << record.parameter;
        if (record.secondary_parameter != 0.0 || record.tertiary_parameter != 0.0)
          out << ';' << record.secondary_parameter;
        if (record.tertiary_parameter != 0.0)
          out << ';' << record.tertiary_parameter;
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
  std::size_t arc_id = 0;
  const auto write_input_arc = [&](std::size_t place, std::size_t t,
                                   PetriArcType type, std::size_t weight = 1) {
    out << "      <arc id=\"a" << arc_id++ << "\" source=\"p" << place
        << "\" target=\"t" << t << "\"";
    if (type == PetriArcType::kNormal && weight == 1) {
      out << "/>\n";
    } else {
      out << ">";
      if (weight != 1)
        out << "<inscription><text>" << weight << "</text></inscription>";
      if (type != PetriArcType::kNormal)
        out << "<type><text>"
            << (type == PetriArcType::kReset ? "reset" : "inhibitor")
            << "</text></type>";
      out << "</arc>\n";
    }
  };
  for (std::size_t t = 0; t < transitions.size(); ++t) {
    const auto& transition = transitions[t];
    std::map<std::size_t, std::size_t> input_weights;
    for (const auto place : transition.inputs) ++input_weights[place];
    for (const auto& [place, weight] : input_weights)
      write_input_arc(place, t, PetriArcType::kNormal, weight);
    for (const auto place : transition.reset_inputs)
      write_input_arc(place, t, PetriArcType::kReset);
    for (const auto place : transition.inhibitor_inputs)
      write_input_arc(place, t, PetriArcType::kInhibitor);
    std::map<std::size_t, std::size_t> output_weights;
    for (const auto place : transition.outputs) ++output_weights[place];
    for (const auto& [place, weight] : output_weights) {
      out << "      <arc id=\"a" << arc_id++ << "\" source=\"t" << t
          << "\" target=\"p" << place << "\"";
      if (weight == 1) out << "/>\n";
      else out << "><inscription><text>" << weight
               << "</text></inscription></arc>\n";
    }
  }

  out << "    </page>\n";

  out << "    <finalmarkings>\n";
  out << "      <marking>\n";
  for (std::size_t place = 0; place < final_marking.size(); ++place)
    if (final_marking[place] != 0)
      out << "        <place idref=\"p" << place << "\"><text>"
          << final_marking[place] << "</text></place>\n";
  out << "      </marking>\n";
  out << "    </finalmarkings>\n";

  out << "  </net>\n";
  out << "</pnml>\n";
}

void write_pnml(std::ostream& out, const PetriNet& net,
                std::string_view name) {
  std::vector<std::int64_t> initial(net.place_count(), 0);
  std::vector<std::int64_t> final(net.place_count(), 0);
  initial[net.source()] = 1;
  final[net.sink()] = 1;
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

namespace {

// What the first pass collects from the document before the net is assembled.
struct ParsedPlace {
  std::string id;
  std::uint64_t initial_tokens = 0;
  bool has_initial_marking = false;
};

struct ParsedTransition {
  std::string id;
  std::string label;
  bool invisible = false;
  std::string distribution_type;
  std::string distribution_parameters;
  int priority = 0;
  double weight = 1.0;
  bool has_stochastic_data = false;
};

struct ParsedArc {
  std::string source;
  std::string target;
  PetriArcType type = PetriArcType::kNormal;
  std::size_t weight = 1;
};

std::uint64_t parse_token_count(const std::string& text) {
  try {
    const auto first = text.find_first_not_of(" \t\r\n");
    const auto last = text.find_last_not_of(" \t\r\n");
    if (first == std::string::npos)
      throw std::runtime_error("empty PNML token count");
    const auto trimmed = text.substr(first, last - first + 1);
    if (!trimmed.empty() && trimmed.front() == '-')
      throw std::runtime_error("negative PNML token count");
    std::size_t consumed = 0;
    const auto value = std::stoull(trimmed, &consumed);
    if (consumed != trimmed.size())
      throw std::runtime_error("PNML marking contains a non-integer token count");
    return value;
  } catch (const std::exception&) {
    throw std::runtime_error("PNML marking contains an invalid token count");
  }
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

PnmlDocument parse_pnml_document(std::string_view xml,
                                 const PnmlImportOptions& options) {
  detail::XmlPullParser parser(xml, /*emit_character_data=*/true);

  std::vector<ParsedPlace> places;
  std::vector<ParsedTransition> transitions;
  std::vector<ParsedArc> arcs;
  std::unordered_map<std::string, std::size_t> place_index;
  std::unordered_map<std::string, std::size_t> transition_index;

  // The final marking recorded under <finalmarkings>; only the first <marking>
  // is taken (alternative final markings don't fit a single-sink workflow net).
  std::vector<std::pair<std::string, std::uint64_t>> final_marking;
  std::size_t markings_seen = 0;

  std::vector<std::string> stack;
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
      if (markings_seen == 1) {
        const auto* idref = detail::attribute(token.attributes, "idref");
        if (idref) {
          final_place_ref = *idref;
          // A marking entry with no nested <text> means one token.
          final_marking.emplace_back(*idref, 1);
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
      transitions.push_back({*id, "", false});
    } else if (token.name == "arc") {
      const auto* source = detail::attribute(token.attributes, "source");
      const auto* target = detail::attribute(token.attributes, "target");
      if (!source || !target) {
        throw std::runtime_error("PNML arc is missing its source or target");
      }
      arcs.push_back({*source, *target, PetriArcType::kNormal});
    } else if (token.name == "toolspecific" &&
               stack_ends_with(stack, {"transition"})) {
      const auto* activity = detail::attribute(token.attributes, "activity");
      if (activity && *activity == "$invisible$" && !transitions.empty()) {
        transitions.back().invisible = true;
      }
    } else if (token.name == "marking" &&
               stack_ends_with(stack, {"finalmarkings"})) {
      ++markings_seen;
    }
    return true;
  };

  std::string stochastic_property_key;
  const auto text = [&](const std::string& value) {
    if (!in_net) return;
    if (stack_ends_with(stack, {"place", "initialMarking", "text"}) &&
        !places.empty()) {
      places.back().initial_tokens = parse_token_count(value);
    } else if (stack_ends_with(stack, {"transition", "name", "text"}) &&
               !transitions.empty()) {
      transitions.back().label = value;
    } else if (stack_ends_with(stack, {"arc", "type", "text"}) &&
               !arcs.empty()) {
      if (value == "reset") arcs.back().type = PetriArcType::kReset;
      else if (value == "inhibitor") arcs.back().type = PetriArcType::kInhibitor;
      else if (value == "normal" || value == "") arcs.back().type = PetriArcType::kNormal;
      else throw std::runtime_error("PNML arc has an unknown type '" + value + "'");
    } else if (stack_ends_with(stack, {"arc", "inscription", "text"}) &&
               !arcs.empty()) {
      const auto weight = parse_token_count(value);
      if (weight == 0 || weight > std::numeric_limits<std::size_t>::max())
        throw std::runtime_error("PNML arc weight must be a positive size value");
      arcs.back().weight = static_cast<std::size_t>(weight);
    } else if (stack_ends_with(stack, {"transition", "toolspecific", "property"}) &&
               !transitions.empty() && !stochastic_property_key.empty()) {
      auto& transition = transitions.back();
      transition.has_stochastic_data = true;
      if (stochastic_property_key == "distributionType")
        transition.distribution_type = value;
      else if (stochastic_property_key == "distributionParameters")
        transition.distribution_parameters = value;
      else if (stochastic_property_key == "priority")
        transition.priority = std::stoi(value);
      else if (stochastic_property_key == "weight")
        transition.weight = std::stod(value);
      else if (stochastic_property_key == "invisible" && value == "true")
        transition.invisible = true;
    } else if (stack_ends_with(stack, {"marking", "place", "text"}) &&
               markings_seen == 1 && !final_marking.empty() &&
               final_marking.back().first == final_place_ref) {
      final_marking.back().second = parse_token_count(value);
    }
  };

  while (true) {
    const auto token = parser.next();
    if (token.kind == detail::XmlPullParser::Kind::EndOfInput) break;
    if (token.kind == detail::XmlPullParser::Kind::CharacterData) {
      text(token.text);
      continue;
    }
    if (token.kind == detail::XmlPullParser::Kind::StartElement) {
      if (token.name == "property" &&
          stack_ends_with(stack, {"transition", "toolspecific"})) {
        const auto* key = detail::attribute(token.attributes, "key");
        stochastic_property_key = key ? *key : std::string{};
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
    while (!stack.empty() && stack.back() != token.name) stack.pop_back();
    if (!stack.empty()) stack.pop_back();
    if (token.name == "property") stochastic_property_key.clear();
    if (token.name == "net") in_net = false;
  }

  if (!nets_seen || (options.require_workflow_boundaries &&
                     (places.empty() || transitions.empty()))) {
    throw std::runtime_error("PNML input contains no net (or an empty one)");
  }

  // Assemble the net: visible transitions with the same label share an
  // activity, in first-seen order.
  PetriNet net;
  for (std::size_t i = 0; i < places.size(); ++i) net.add_place();

  std::vector<std::string> activities;
  std::unordered_map<std::string, std::size_t> activity_ids;
  std::vector<std::vector<std::size_t>> inputs(transitions.size());
  std::vector<std::vector<std::size_t>> outputs(transitions.size());
  std::vector<std::vector<std::size_t>> reset_inputs(transitions.size());
  std::vector<std::vector<std::size_t>> inhibitor_inputs(transitions.size());
  std::vector<bool> place_has_in(places.size(), false);
  std::vector<bool> place_has_out(places.size(), false);

  for (const auto& arc : arcs) {
    const auto from_place = place_index.find(arc.source);
    const auto to_place = place_index.find(arc.target);
    const auto from_transition = transition_index.find(arc.source);
    const auto to_transition = transition_index.find(arc.target);
    if (from_place != place_index.end() &&
        to_transition != transition_index.end()) {
      if (arc.type == PetriArcType::kNormal) {
        inputs[to_transition->second].insert(
            inputs[to_transition->second].end(), arc.weight,
            from_place->second);
      }
      else if (arc.type == PetriArcType::kReset)
        reset_inputs[to_transition->second].push_back(from_place->second);
      else
        inhibitor_inputs[to_transition->second].push_back(from_place->second);
      if (arc.type != PetriArcType::kNormal && arc.weight != 1)
        throw std::runtime_error("PNML extended arcs cannot have multiplicity");
      place_has_out[from_place->second] = true;
    } else if (from_transition != transition_index.end() &&
               to_place != place_index.end()) {
      if (arc.type != PetriArcType::kNormal)
        throw std::runtime_error("PNML extended arc must enter a transition");
      outputs[from_transition->second].insert(
          outputs[from_transition->second].end(), arc.weight,
          to_place->second);
      place_has_in[to_place->second] = true;
    } else {
      throw std::runtime_error("PNML arc '" + arc.source + "' -> '" +
                               arc.target +
                               "' does not connect a place and a transition");
    }
  }

  for (std::size_t t = 0; t < transitions.size(); ++t) {
    const auto& parsed = transitions[t];
    std::optional<std::size_t> activity;
    if (!parsed.invisible && !parsed.label.empty()) {
      const auto [it, inserted] =
          activity_ids.emplace(parsed.label, activities.size());
      if (inserted) activities.push_back(parsed.label);
      activity = it->second;
    }
    net.add_transition(activity, std::move(inputs[t]), std::move(outputs[t]));
    for (const auto place : reset_inputs[t]) net.add_reset_arc(t, place);
    for (const auto place : inhibitor_inputs[t]) net.add_inhibitor_arc(t, place);
  }
  net.set_activities(std::move(activities));

  PnmlDocument document;
  for (const auto& place : places) document.place_ids.push_back(place.id);
  for (const auto& transition : transitions) document.transition_ids.push_back(transition.id);
  document.initial_marking.assign(places.size(), 0);
  document.final_marking.assign(places.size(), 0);
  document.initial_marking_keys.assign(places.size(), false);
  document.final_marking_keys.assign(places.size(), false);
  std::vector<std::size_t> marked;
  for (std::size_t p = 0; p < places.size(); ++p) {
    if (places[p].initial_tokens >
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
      throw std::runtime_error("PNML initial marking exceeds signed range");
    }
    document.initial_marking[p] =
        static_cast<std::int64_t>(places[p].initial_tokens);
    document.initial_marking_keys[p] = places[p].has_initial_marking;
    if (places[p].initial_tokens > 0) marked.push_back(p);
  }
  if (!marked.empty()) {
    net.set_source(marked.front());
  } else if (options.infer_initial_marking) {
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
    if (sources.size() != 1 && options.require_workflow_boundaries)
      throw std::runtime_error("PNML has no unique structural source place");
    if (!sources.empty()) net.set_source(sources.front());
  }

  if (!final_marking.empty()) {
    for (const auto& [id, tokens] : final_marking) {
      const auto it = place_index.find(id);
      if (it == place_index.end()) {
        throw std::runtime_error("PNML final marking references unknown place '" +
                                 id + "'");
      }
      if (tokens >
          static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        throw std::runtime_error("PNML final marking exceeds signed range");
      }
      const auto signed_tokens = static_cast<std::int64_t>(tokens);
      if (document.final_marking[it->second] >
          std::numeric_limits<std::int64_t>::max() - signed_tokens) {
        throw std::runtime_error("PNML final marking exceeds signed range");
      }
      document.final_marking[it->second] += signed_tokens;
      document.final_marking_keys[it->second] = true;
    }
    const auto first = std::find_if(
        document.final_marking.begin(), document.final_marking.end(),
        [](std::int64_t tokens) { return tokens > 0; });
    if (first == document.final_marking.end() && options.require_workflow_boundaries) {
      throw std::runtime_error("PNML final marking contains no tokens");
    }
    if (first != document.final_marking.end())
      net.set_sink(static_cast<std::size_t>(first - document.final_marking.begin()));
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
    if (sinks.size() != 1 && options.require_workflow_boundaries)
      throw std::runtime_error("PNML has no unique structural sink place");
    if (!sinks.empty()) net.set_sink(sinks.front());
  }

  if (options.import_stochastic_map) {
    StochasticMap map;
    map.transitions.reserve(transitions.size());
    bool any = false;
    for (std::size_t index = 0; index < transitions.size(); ++index) {
      const auto& parsed = transitions[index];
      StochasticMapTransition record;
      record.transition = index;
      record.activation_weight = parsed.weight;
      record.priority = parsed.priority;
      if (parsed.has_stochastic_data) any = true;
      const auto type = parsed.distribution_type;
      if (type == "IMMEDIATE") {
        record.timing = StochasticMapTiming::kImmediate;
        record.distribution = StochasticMapDistribution::kImmediate;
        record.parameter = 0.0;
      } else if (type == "DETERMINISTIC") {
        record.distribution = StochasticMapDistribution::kDeterministic;
      } else if (type == "NORMAL") {
        record.distribution = StochasticMapDistribution::kNormal;
      } else if (type == "UNIFORM") {
        record.distribution = StochasticMapDistribution::kUniform;
      } else if (type == "LOGNORMAL") {
        record.distribution = StochasticMapDistribution::kLogNormal;
      } else if (type == "GAMMA") {
        record.distribution = StochasticMapDistribution::kGamma;
      } else {
        record.distribution = StochasticMapDistribution::kExponential;
      }
      if (!parsed.distribution_parameters.empty()) {
        std::string normalized = parsed.distribution_parameters;
        std::replace(normalized.begin(), normalized.end(), ';', ' ');
        std::replace(normalized.begin(), normalized.end(), ',', ' ');
        std::istringstream parameters(normalized);
        if (!(parameters >> record.parameter) || !std::isfinite(record.parameter))
          throw std::runtime_error("PNML stochastic parameters are invalid");
        if (parameters >> record.secondary_parameter) {
          if (!std::isfinite(record.secondary_parameter))
            throw std::runtime_error("PNML stochastic parameters are invalid");
          if (parameters >> record.tertiary_parameter) {
            if (!std::isfinite(record.tertiary_parameter))
              throw std::runtime_error("PNML stochastic parameters are invalid");
          }
        }
      }
      map.transitions.push_back(record);
    }
    if (any) {
      validate_stochastic_map(map, transitions.size());
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
                                              std::int64_t{0});
  const auto final_tokens = std::accumulate(document.final_marking.begin(),
                                            document.final_marking.end(),
                                            std::int64_t{0});
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
                                              std::int64_t{0});
  const auto final_tokens = std::accumulate(document.final_marking.begin(),
                                            document.final_marking.end(),
                                            std::int64_t{0});
  if (initial_tokens != 1 || document.initial_marking[document.net.source()] != 1 ||
      final_tokens != 1 || document.final_marking[document.net.sink()] != 1) {
    throw std::runtime_error(
        "PNML markings do not describe a single-token workflow net");
  }
  return std::move(document.net);
}

}  // namespace minera
