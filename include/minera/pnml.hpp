#pragma once

#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "minera/petri_net.hpp"

namespace minera {

// Serializes a workflow net as PNML (the Petri Net Markup Language, ISO/IEC
// 15909-2) — the standard interchange format read by ProM and other
// process-mining tools. Places are emitted as p0..pN and transitions as
// t0..tN; a visible transition carries its activity name, a silent (tau)
// transition is left unnamed and flagged invisible with the tool-specific
// annotation ProM recognizes. The source place holds the initial
// marking and the sink is recorded under <finalmarkings>.
void write_pnml(std::ostream& out,
                const PetriNet& net,
                std::string_view name = "net");
void write_pnml(std::ostream& out, const PetriNet& net,
                const std::vector<std::int64_t>& initial_marking,
                const std::vector<std::int64_t>& final_marking,
                std::string_view name = "net");

[[nodiscard]] std::string to_pnml(const PetriNet& net,
                                  std::string_view name = "net");
[[nodiscard]] std::string to_pnml(
    const PetriNet& net, const std::vector<std::int64_t>& initial_marking,
    const std::vector<std::int64_t>& final_marking,
    std::string_view name = "net");

// Parses a PNML workflow net, so a model discovered or drawn elsewhere can be
// checked against a log. Reads the first <net>: places, transitions, and arcs
// (nested pages included). A transition is silent when it is flagged with the
// toolspecific activity="$invisible$" annotation or carries no name; visible
// transitions with the same label share one activity. The initial marking must
// put a single token on one place (inferred as the only place with no incoming
// arc when the file records no marking); the final marking — the first
// <finalmarkings> entry, or the only place with no outgoing arc — must be a
// single token likewise, since conformance replays source-to-sink workflow
// nets. Malformed input, dangling arc references, and markings outside that
// shape throw std::runtime_error.
[[nodiscard]] PetriNet parse_pnml(std::string_view xml);

// Reads and parses a PNML file.
[[nodiscard]] PetriNet import_pnml(const std::filesystem::path& path);

}  // namespace minera
