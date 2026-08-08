#pragma once

#include <filesystem>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>

#include "minera/dfg_variants.hpp"

namespace minera {

class DirectlyFollowsGraph;

// The classic DFG text interchange format consists of an activity-name
// registry, start and end boundary rows, followed by directly-follows rows.
// Rows use the compact `idxcount` and `source>targetxcount` spellings
// without whitespace (for example, `0x3` and `0>1x2`). DfgCleanAnalysis is the
// compact native payload for those three maps.
void write_dfg(std::ostream& out, const DfgCleanAnalysis& dfg);

[[nodiscard]] std::string to_dfg(const DfgCleanAnalysis& dfg);

[[nodiscard]] DfgCleanAnalysis parse_dfg(std::string_view text);

// Projects a discovered graph onto the classic interchange payload. Registry
// entries with no edge or boundary frequency are intentionally omitted.
[[nodiscard]] DfgCleanAnalysis to_dfg_analysis(
    const DirectlyFollowsGraph& graph);

[[nodiscard]] DfgCleanAnalysis import_dfg(std::istream& input);

[[nodiscard]] DfgCleanAnalysis import_dfg(const std::filesystem::path& path);

inline DfgCleanAnalysis read_dfg(std::istream& input) {
  return import_dfg(input);
}

inline DfgCleanAnalysis read_dfg(const std::filesystem::path& path) {
  return import_dfg(path);
}

// Explicit naming aliases for callers that use serialize/export terminology.
[[nodiscard]] inline std::string serialize_dfg(const DfgCleanAnalysis& dfg) {
  return to_dfg(dfg);
}

inline void serialize_dfg(std::ostream& out, const DfgCleanAnalysis& dfg) {
  write_dfg(out, dfg);
}

[[nodiscard]] inline DfgCleanAnalysis deserialize_dfg(std::string_view text) {
  return parse_dfg(text);
}

void export_dfg(const std::filesystem::path& path,
                const DfgCleanAnalysis& dfg);

inline void export_dfg(const DfgCleanAnalysis& dfg,
                       const std::filesystem::path& path) {
  export_dfg(path, dfg);
}

}  // namespace minera
