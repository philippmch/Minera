#include "minera/monte_carlo.hpp"

#include <iomanip>
#include <sstream>

namespace minera {

std::string monte_carlo_json(const MonteCarloResult& result) {
  const auto& diagnostics = result.diagnostics;
  std::ostringstream out;
  out << std::setprecision(17);
  out << "{\"family\":\"MONTE_CARLO\",\"variant\":\"PETRI_SEMAPH_FIFO\"";
  out << ",\"log\":{\"trace_count\":" << result.log.trace_count()
      << ",\"event_count\":" << result.log.event_count() << '}';
  out << ",\"diagnostics\":{";
  out << "\"simulated_cases\":" << diagnostics.simulated_cases;
  out << ",\"completed_cases\":" << diagnostics.completed_cases;
  out << ",\"deadlocked_cases\":" << diagnostics.deadlocked_cases;
  out << ",\"step_limited_cases\":" << diagnostics.step_limited_cases;
  out << ",\"case_arrival_seconds\":" << diagnostics.case_arrival_seconds;
  out << ",\"total_cases_seconds\":" << diagnostics.total_cases_seconds;
  out << ",\"median_case_execution_seconds\":"
      << diagnostics.median_case_execution_seconds;
  out << ",\"case_execution_seconds\":[";
  for (std::size_t i = 0; i < diagnostics.case_execution_seconds.size(); ++i) {
    if (i) out << ',';
    out << diagnostics.case_execution_seconds[i];
  }
  out << "]}}";
  return out.str();
}

}  // namespace minera
