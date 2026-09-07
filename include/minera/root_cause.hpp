#pragma once

#include "minera/event_log.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace minera {

struct RootCauseOptions {
  std::vector<std::string> string_attributes;
  std::vector<std::string> numeric_attributes;
  bool enable_multiplier = false;
};

struct RootCauseObservation {
  std::size_t trace_index = 0;
  Event context;
};

struct RootCauseRepresentation {
  std::vector<std::string> feature_names;
  std::vector<std::vector<double>> data;
  std::vector<int> target;
  std::vector<std::size_t> source_trace_indices;
};

// Copy each supplied context into its own otherwise empty trace. Fit contexts
// precede problem contexts. Optional balancing repeats the complete smaller
// cohort floor(larger/smaller) times; balancing an empty cohort throws
// std::domain_error. Without balancing, empty/one-sided cohorts are valid.
[[nodiscard]] EventLog build_root_cause_log(
    const std::vector<RootCauseObservation>& fit,
    const std::vector<RootCauseObservation>& problem,
    bool enable_multiplier = false);

// Encode the same cohort order without training. No selectors produce empty
// rows. Empty cohorts retain requested numeric column names. Infinity and
// values beyond float32 range are preserved; missing/null selected numeric
// attributes still throw std::invalid_argument. Balancing follows the log
// builder's empty-cohort rule. Targets are 0 for fit and 1 for problem.
[[nodiscard]] RootCauseRepresentation build_root_cause_representation(
    const std::vector<RootCauseObservation>& fit,
    const std::vector<RootCauseObservation>& problem,
    const RootCauseOptions& options);

struct RootCauseTreeNode {
  int feature_index = -1;
  double threshold = -2.0;
  int left = -1;
  int right = -1;
  std::array<std::size_t, 2> class_counts{};
  bool missing_go_to_left = false;
  // Separates all finite values (left) from NaN values (right); threshold
  // is zero and unused so the serialized tree stays finite.
  bool missing_only = false;
};

struct RootCauseModel {
  std::vector<std::string> feature_names;
  std::vector<std::string> classes;
  std::vector<std::vector<double>> data;
  std::vector<int> target;
  std::vector<std::size_t> source_trace_indices;
  std::vector<RootCauseTreeNode> tree;
  std::size_t fit_count = 0;
  std::size_t problem_count = 0;
};

// Explicitly selected event attributes form a numeric/one-hot matrix. The
// exposed matrix preserves double precision; CART uses float32 values, Gini
// impurity, a float32 separation tolerance of 1e-7, and a maximum depth of
// seven. A locally entropy-seeded engine shuffles feature visits at each
// node, so equally optimal feature choices can vary between fits. Within a
// feature, missing-right routes precede missing-left routes, and ascending
// thresholds break equal-score ties within each route.
// One-sided cohorts return no model. Invalid selected values throw
// std::invalid_argument. Row order is fit, then problem; balancing repeats
// the entire smaller cohort floor(larger/smaller) times.
[[nodiscard]] std::optional<RootCauseModel> fit_root_cause_model(
    const std::vector<RootCauseObservation>& fit,
    const std::vector<RootCauseObservation>& problem,
    const RootCauseOptions& options,
    const std::string& problem_class = "underfed");

}  // namespace minera
