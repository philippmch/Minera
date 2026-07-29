#include "minera/pnml.hpp"

#include <cstddef>
#include <fstream>
#include <iterator>
#include <optional>
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

void write_pnml(std::ostream& out,
                const PetriNet& net,
                std::string_view name) {
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
    if (place == net.source()) {
      out << "        <initialMarking><text>1</text></initialMarking>\n";
    }
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
    out << "      </transition>\n";
  }

  // Arcs: every input place -> transition, then transition -> every output
  // place. Arc ids are unique across the whole net.
  std::size_t arc_id = 0;
  for (std::size_t t = 0; t < transitions.size(); ++t) {
    const auto& transition = transitions[t];
    for (const auto place : transition.inputs) {
      out << "      <arc id=\"a" << arc_id++ << "\" source=\"p" << place
          << "\" target=\"t" << t << "\"/>\n";
    }
    for (const auto place : transition.outputs) {
      out << "      <arc id=\"a" << arc_id++ << "\" source=\"t" << t
          << "\" target=\"p" << place << "\"/>\n";
    }
  }

  out << "    </page>\n";

  // The final marking (one token on the sink) under the convention used by
  // ProM for accepting workflow nets.
  out << "    <finalmarkings>\n";
  out << "      <marking>\n";
  out << "        <place idref=\"p" << net.sink()
      << "\"><text>1</text></place>\n";
  out << "      </marking>\n";
  out << "    </finalmarkings>\n";

  out << "  </net>\n";
  out << "</pnml>\n";
}

std::string to_pnml(const PetriNet& net, std::string_view name) {
  std::ostringstream out;
  write_pnml(out, net, name);
  return out.str();
}

namespace {

// What the first pass collects from the document before the net is assembled.
struct ParsedPlace {
  std::string id;
  std::uint64_t initial_tokens = 0;
};

struct ParsedTransition {
  std::string id;
  std::string label;
  bool invisible = false;
};

struct ParsedArc {
  std::string source;
  std::string target;
};

std::uint64_t parse_token_count(const std::string& text) {
  try {
    return std::stoull(text);
  } catch (const std::exception&) {
    return 0;
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

PetriNet parse_pnml(std::string_view xml) {
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
      arcs.push_back({*source, *target});
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

  const auto text = [&](const std::string& value) {
    if (!in_net) return;
    if (stack_ends_with(stack, {"place", "initialMarking", "text"}) &&
        !places.empty()) {
      places.back().initial_tokens = parse_token_count(value);
    } else if (stack_ends_with(stack, {"transition", "name", "text"}) &&
               !transitions.empty()) {
      transitions.back().label = value;
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
    if (token.name == "net") in_net = false;
  }

  if (places.empty() || transitions.empty()) {
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
  std::vector<bool> place_has_in(places.size(), false);
  std::vector<bool> place_has_out(places.size(), false);

  for (const auto& arc : arcs) {
    const auto from_place = place_index.find(arc.source);
    const auto to_place = place_index.find(arc.target);
    const auto from_transition = transition_index.find(arc.source);
    const auto to_transition = transition_index.find(arc.target);
    if (from_place != place_index.end() &&
        to_transition != transition_index.end()) {
      inputs[to_transition->second].push_back(from_place->second);
      place_has_out[from_place->second] = true;
    } else if (from_transition != transition_index.end() &&
               to_place != place_index.end()) {
      outputs[from_transition->second].push_back(to_place->second);
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
  }
  net.set_activities(std::move(activities));

  // Source: the single-token initial marking, or the only place no arc feeds.
  std::vector<std::size_t> marked;
  for (std::size_t p = 0; p < places.size(); ++p) {
    if (places[p].initial_tokens > 0) marked.push_back(p);
  }
  if (marked.size() == 1 && places[marked.front()].initial_tokens == 1) {
    net.set_source(marked.front());
  } else if (!marked.empty()) {
    throw std::runtime_error(
        "PNML initial marking is not a single token on one place; only "
        "workflow nets can be checked");
  } else {
    std::vector<std::size_t> sources;
    for (std::size_t p = 0; p < places.size(); ++p) {
      if (!place_has_in[p]) sources.push_back(p);
    }
    if (sources.size() != 1) {
      throw std::runtime_error(
          "PNML has no initial marking and no unique source place");
    }
    net.set_source(sources.front());
  }

  // Sink: the recorded final marking, or the only place with no outgoing arc.
  if (final_marking.size() == 1 && final_marking.front().second == 1) {
    const auto it = place_index.find(final_marking.front().first);
    if (it == place_index.end()) {
      throw std::runtime_error("PNML final marking references unknown place '" +
                               final_marking.front().first + "'");
    }
    net.set_sink(it->second);
  } else if (!final_marking.empty()) {
    throw std::runtime_error(
        "PNML final marking is not a single token on one place; only "
        "workflow nets can be checked");
  } else {
    std::vector<std::size_t> sinks;
    for (std::size_t p = 0; p < places.size(); ++p) {
      if (!place_has_out[p]) sinks.push_back(p);
    }
    if (sinks.size() != 1) {
      throw std::runtime_error(
          "PNML has no final marking and no unique sink place");
    }
    net.set_sink(sinks.front());
  }

  return net;
}

PetriNet import_pnml(const std::filesystem::path& path) {
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
  return parse_pnml(xml);
}

}  // namespace minera
