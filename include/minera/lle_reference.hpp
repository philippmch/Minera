#pragma once

// algo.transformation.log_to_features.util.locally_linear_embedding of the
// PM4Py 2.7.22 reference, observed black-box (numpy 2.4.6 with
// scipy-openblas 0.3.31, scikit-learn 1.9.0).
//
// smooth(y, box) is numpy's convolve(y, ones(box) / box, "same"): the longer
// of the two arrays slides, and each output is a dot product over the
// overlap. A full overlap of at most 11 terms is summed left to right with
// rounded products; partial overlaps and full overlaps of 12 to 15 terms
// use fused multiply-adds left to right (the BLAS dot below 16 terms;
// longer dots use a SIMD kernel and are outside the bank). A negative box
// raises ValueError "negative dimensions are not allowed", an empty y "a
// cannot be empty" and a zero box "v cannot be empty".
//
// apply sorts the events of every trace by timestamp, then the traces by
// their first timestamp (both stable), and returns the first timestamps and
// the smoothed one-dimensional locally linear embedding of the TRACE_BASED
// features with str_ev_attr and str_evsucc_attr set to the activity. The
// embedding is scikit-learn's standard LLE with 5 neighbours, reg 1e-3 and
// the dense solver: the 6 nearest rows by Euclidean distance (the first
// column, usually the row itself, dropped), barycentric weights from the
// regularised local Gram matrix (reg times its trace, or reg when the trace
// is 0), and the eigenvector of the second smallest eigenvalue of
// (I - W)^T (I - W). Fewer than 6 traces raise ValueError "Expected
// n_neighbors < n_samples, but n_samples = <n>, n_neighbors = 5". The box is
// floor(sqrt(traces)) + 1.
//
// Ties between equal distances are broken by row index here; scikit-learn
// breaks them inside its tree search, so lle_reference_embedding also takes
// the neighbour indices to use. The eigenvector's sign and, for repeated
// eigenvalues, its direction within the eigenspace are LAPACK's choice.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include "minera/event_log.hpp"

namespace minera {

class LleReferenceError : public std::runtime_error {
 public:
  LleReferenceError(std::string type, const std::string& message)
      : std::runtime_error(type + ": " + message), type_(std::move(type)) {}
  [[nodiscard]] const std::string& type() const noexcept { return type_; }

 private:
  std::string type_;
};

using LleMatrix = std::vector<std::vector<double>>;
// Neighbour indices per row as NearestNeighbors(6).kneighbors returns them.
using LleNeighbors = std::vector<std::vector<std::size_t>>;

struct LleReferenceFeatures {
  std::vector<std::int64_t> dates;  // first timestamps of the sorted traces, epoch microseconds
  std::vector<std::string> names;
  LleMatrix rows;
};

struct LleReferenceEmbedding {
  std::vector<double> values;       // unit-norm eigenvector
  std::vector<double> eigenvalues;  // all eigenvalues of M, ascending
  LleMatrix m;                      // (I - W)^T (I - W)
};

struct LleReferenceResult {
  std::vector<std::int64_t> dates;
  std::vector<double> values;
};

[[nodiscard]] std::vector<double> reference_lle_smooth(const std::vector<double>& y, std::int64_t box);
[[nodiscard]] LleReferenceFeatures reference_lle_features(const EventLog& log,
                                                          const std::string& activity_key = "concept:name");
[[nodiscard]] LleNeighbors reference_lle_neighbors(const LleMatrix& rows);
[[nodiscard]] LleReferenceEmbedding reference_lle_embedding(const LleMatrix& rows, const LleNeighbors& neighbors);
[[nodiscard]] std::int64_t reference_lle_box(std::size_t traces);

// The whole of apply; neighbors replaces reference_lle_neighbors when given.
[[nodiscard]] LleReferenceResult reference_lle_apply(
    const EventLog& log, const std::string& activity_key = "concept:name",
    const std::function<LleNeighbors(const LleMatrix&)>& neighbors = {});

}  // namespace minera
