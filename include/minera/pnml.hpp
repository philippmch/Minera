#pragma once

#include <ostream>
#include <string>
#include <string_view>

#include "minera/petri_net.hpp"

namespace minera {

// Serializes a workflow net as PNML (the Petri Net Markup Language, ISO/IEC
// 15909-2) — the standard interchange format read by ProM, PM4Py, and other
// process-mining tools. Places are emitted as p0..pN and transitions as
// t0..tN; a visible transition carries its activity name, a silent (tau)
// transition is left unnamed and flagged invisible with the tool-specific
// annotation PM4Py and ProM recognize. The source place holds the initial
// marking and the sink is recorded under <finalmarkings>.
void write_pnml(std::ostream& out,
                const PetriNet& net,
                std::string_view name = "net");

[[nodiscard]] std::string to_pnml(const PetriNet& net,
                                  std::string_view name = "net");

}  // namespace minera
