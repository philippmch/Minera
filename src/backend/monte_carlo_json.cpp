#include "minera/monte_carlo.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace minera {

namespace {

void write_intervals(std::ostream& out,
                     const std::vector<MonteCarloInterval>& intervals) {
  out << '[';
  for (std::size_t index = 0; index < intervals.size(); ++index) {
    if (index) out << ',';
    const auto& interval = intervals[index];
    if (!std::isfinite(interval.start) || !std::isfinite(interval.end) ||
        interval.end < interval.start) {
      throw std::invalid_argument(
          "Monte Carlo interval has invalid finite bounds");
    }
    out << "{\"start\":" << interval.start << ",\"end\":"
        << interval.end << '}';
  }
  out << ']';
}

void write_place_trees(std::ostream& out,
                       const std::vector<MonteCarloPlaceIntervalTree>& trees) {
  std::size_t expected = 0;
  out << '[';
  for (std::size_t index = 0; index < trees.size(); ++index) {
    if (index) out << ',';
    const auto& tree = trees[index];
    if (tree.place_index != expected++)
      throw std::invalid_argument("Monte Carlo place trees are not contiguous");
    out << "{\"place_index\":" << tree.place_index << ",\"intervals\":";
    write_intervals(out, tree.intervals);
    out << '}';
  }
  out << ']';
}

void write_transition_trees(
    std::ostream& out,
    const std::vector<MonteCarloTransitionIntervalTree>& trees) {
  std::size_t expected = 0;
  out << '[';
  for (std::size_t index = 0; index < trees.size(); ++index) {
    if (index) out << ',';
    const auto& tree = trees[index];
    if (tree.transition_index != expected++)
      throw std::invalid_argument("Monte Carlo transition trees are not contiguous");
    out << "{\"transition_index\":" << tree.transition_index
        << ",\"intervals\":";
    write_intervals(out, tree.intervals);
    out << '}';
  }
  out << ']';
}

}  // namespace

std::string monte_carlo_json(const MonteCarloResult& result) {
  const auto& diagnostics = result.diagnostics;
  const auto finite_non_negative = [](double value) {
    return std::isfinite(value) && value >= 0.0;
  };
  if (diagnostics.completed_cases + diagnostics.deadlocked_cases +
          diagnostics.step_limited_cases !=
      diagnostics.simulated_cases ||
      result.log.trace_count() != diagnostics.simulated_cases ||
      !finite_non_negative(diagnostics.case_arrival_seconds) ||
      !finite_non_negative(diagnostics.total_cases_seconds) ||
      !finite_non_negative(diagnostics.median_case_execution_seconds) ||
      !finite_non_negative(diagnostics.case_arrival_ratio) ||
      !finite_non_negative(diagnostics.total_cases_time) ||
      !finite_non_negative(diagnostics.median_cases_ex_time))
    throw std::invalid_argument("Monte Carlo diagnostics are inconsistent");
  for (const auto value : diagnostics.case_execution_seconds)
    if (!finite_non_negative(value))
      throw std::invalid_argument("Monte Carlo case duration is invalid");
  for (const auto value : diagnostics.cases_ex_time)
    if (!finite_non_negative(value))
      throw std::invalid_argument("Monte Carlo execution time is invalid");
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
  out << ']';
  out << ",\"case_arrival_ratio\":" << diagnostics.case_arrival_ratio;
  out << ",\"total_cases_time\":" << diagnostics.total_cases_time;
  out << ",\"median_cases_ex_time\":" << diagnostics.median_cases_ex_time;
  out << ",\"cases_ex_time\":[";
  for (std::size_t i = 0; i < diagnostics.cases_ex_time.size(); ++i) {
    if (i) out << ',';
    out << diagnostics.cases_ex_time[i];
  }
  out << "]}";
  out << ",\"output_places_interval_trees\":";
  write_place_trees(out, result.output_places_interval_trees);
  out << ",\"output_transitions_interval_trees\":";
  write_transition_trees(out, result.output_transitions_interval_trees);
  out << '}';
  return out.str();
}

}  // namespace minera
