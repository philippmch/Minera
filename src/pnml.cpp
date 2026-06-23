#include "minera/pnml.hpp"

#include <sstream>

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
  // silent ones get an invisible toolspecific flag (the convention PM4Py/ProM
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
  // ProM/PM4Py for accepting workflow nets.
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

}  // namespace minera
