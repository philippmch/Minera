#pragma once

#include <set>
#include <string>

#include "minera/dfg_variants.hpp"

namespace minera {

// The classic relation abstraction retains edge keys, including zero-valued
// entries. Positive frequencies determine causality; reciprocal edge keys
// determine parallel relations independently of their frequencies.
struct AlphaClassicAbstraction {
  DfgFrequency directly_follows;
  std::set<DfgActivityPair> causal;
  std::set<DfgActivityPair> parallel;
};

[[nodiscard]] AlphaClassicAbstraction make_alpha_classic_abstraction(
    const DfgFrequency& graph);

struct AlphaEndpoints {
  std::set<std::string> starts;
  std::set<std::string> ends;
};

[[nodiscard]] AlphaEndpoints infer_alpha_endpoints(const DfgFrequency& graph);
// Reads the first and last activity of each nonempty trace. Callers resolve
// custom selectors before passing the log to this helper.
[[nodiscard]] AlphaEndpoints alpha_log_endpoints(const EventLog& log);

}  // namespace minera
