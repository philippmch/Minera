#pragma once

#include <filesystem>
#include <ostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "minera/petri_net.hpp"
#include "minera/stochastic_map.hpp"

namespace minera {

struct PnmlDocument {
  PetriNet net;
  std::vector<std::int64_t> initial_marking;
  std::vector<std::int64_t> final_marking;
  std::optional<StochasticMap> stochastic_map;
};

struct PnmlImportOptions {
  bool infer_initial_marking = true;
  bool infer_final_marking = true;
  bool import_stochastic_map = false;
};

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
void write_pnml(std::ostream& out, const PetriNet& net,
                const std::vector<std::int64_t>& initial_marking,
                const std::vector<std::int64_t>& final_marking,
                const StochasticMap& stochastic_map,
                std::string_view name = "net");

[[nodiscard]] std::string to_pnml(const PetriNet& net,
                                  std::string_view name = "net");
[[nodiscard]] std::string to_pnml(
    const PetriNet& net, const std::vector<std::int64_t>& initial_marking,
    const std::vector<std::int64_t>& final_marking,
    std::string_view name = "net");
[[nodiscard]] std::string to_pnml(
    const PetriNet& net, const std::vector<std::int64_t>& initial_marking,
    const std::vector<std::int64_t>& final_marking,
    const StochasticMap& stochastic_map,
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

// Parses the first PNML net together with its complete nonnegative initial and
// final markings. When a marking is omitted, the unique structural source or
// sink receives one token. Unlike parse_pnml, this accepts markings spanning
// multiple places and token counts greater than one.
[[nodiscard]] PnmlDocument parse_pnml_document(
    std::string_view xml, const PnmlImportOptions& options = {});

// Reads and parses a PNML file.
[[nodiscard]] PetriNet import_pnml(const std::filesystem::path& path);
[[nodiscard]] PnmlDocument import_pnml_document(
    const std::filesystem::path& path,
    const PnmlImportOptions& options = {});

}  // namespace minera
