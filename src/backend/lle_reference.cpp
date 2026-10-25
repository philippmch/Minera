#include "minera/lle_reference.hpp"

#include "minera/log_to_features.hpp"
#include "minera/timestamp.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace minera {
namespace {

constexpr std::size_t kNeighbors = 5;
constexpr double kReg = 1e-3;

[[noreturn]] void raise(const std::string& type, const std::string& message) { throw LleReferenceError(type, message); }

std::int64_t micros_of(const Event& event) {
  const auto parsed = parse_timestamp_microseconds(event.timestamp);
  if (!parsed) throw std::invalid_argument("lle: event without a timestamp");
  return *parsed;
}

// Cyclic Jacobi rotations; returns eigenvalues ascending with their
// eigenvectors as columns of vectors.
void symmetric_eigen(LleMatrix a, std::vector<double>& values, LleMatrix& vectors) {
  const auto n = a.size();
  vectors.assign(n, std::vector<double>(n, 0.0));
  for (std::size_t i = 0; i < n; ++i) vectors[i][i] = 1.0;
  for (int sweep = 0; sweep < 100; ++sweep) {
    double off = 0.0, total = 0.0;
    for (std::size_t i = 0; i < n; ++i)
      for (std::size_t j = 0; j < n; ++j) {
        total += a[i][j] * a[i][j];
        if (i != j) off += a[i][j] * a[i][j];
      }
    if (off <= 1e-30 * total || off == 0.0) break;
    for (std::size_t p = 0; p + 1 < n; ++p)
      for (std::size_t q = p + 1; q < n; ++q) {
        if (a[p][q] == 0.0) continue;
        const double theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
        const double t = (theta >= 0 ? 1.0 : -1.0) / (std::abs(theta) + std::sqrt(theta * theta + 1.0));
        const double c = 1.0 / std::sqrt(t * t + 1.0), s = t * c;
        for (std::size_t k = 0; k < n; ++k) {
          const double akp = a[k][p], akq = a[k][q];
          a[k][p] = c * akp - s * akq;
          a[k][q] = s * akp + c * akq;
        }
        for (std::size_t k = 0; k < n; ++k) {
          const double apk = a[p][k], aqk = a[q][k];
          a[p][k] = c * apk - s * aqk;
          a[q][k] = s * apk + c * aqk;
        }
        for (std::size_t k = 0; k < n; ++k) {
          const double vkp = vectors[k][p], vkq = vectors[k][q];
          vectors[k][p] = c * vkp - s * vkq;
          vectors[k][q] = s * vkp + c * vkq;
        }
      }
  }
  std::vector<std::size_t> order(n);
  std::iota(order.begin(), order.end(), 0);
  std::stable_sort(order.begin(), order.end(), [&](auto x, auto y) { return a[x][x] < a[y][y]; });
  values.clear();
  LleMatrix sorted(n, std::vector<double>(n));
  for (std::size_t c = 0; c < n; ++c) {
    values.push_back(a[order[c]][order[c]]);
    for (std::size_t r = 0; r < n; ++r) sorted[r][c] = vectors[r][order[c]];
  }
  vectors = std::move(sorted);
}

// Solves the symmetric positive definite system g w = 1 by Cholesky.
std::vector<double> solve_ones(LleMatrix g) {
  const auto n = g.size();
  for (std::size_t j = 0; j < n; ++j) {
    double diagonal = g[j][j];
    for (std::size_t k = 0; k < j; ++k) diagonal -= g[j][k] * g[j][k];
    if (!(diagonal > 0.0)) throw std::runtime_error("lle: local Gram matrix is not positive definite");
    g[j][j] = std::sqrt(diagonal);
    for (std::size_t i = j + 1; i < n; ++i) {
      double value = g[i][j];
      for (std::size_t k = 0; k < j; ++k) value -= g[i][k] * g[j][k];
      g[i][j] = value / g[j][j];
    }
  }
  std::vector<double> w(n, 1.0);
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t k = 0; k < i; ++k) w[i] -= g[i][k] * w[k];
    w[i] /= g[i][i];
  }
  for (std::size_t i = n; i-- > 0;) {
    for (std::size_t k = i + 1; k < n; ++k) w[i] -= g[k][i] * w[k];
    w[i] /= g[i][i];
  }
  return w;
}

}  // namespace

std::vector<double> reference_lle_smooth(const std::vector<double>& y, std::int64_t box) {
  if (box < 0) raise("ValueError", "negative dimensions are not allowed");
  const std::vector<double> kernel(static_cast<std::size_t>(box), 1.0 / static_cast<double>(box));
  if (y.empty()) raise("ValueError", "a cannot be empty");
  if (kernel.empty()) raise("ValueError", "v cannot be empty");
  const auto* a = &y;
  const auto* v = &kernel;
  if (v->size() > a->size()) std::swap(a, v);
  const auto n = a->size(), m = v->size();
  const auto start = (m - 1) / 2;
  std::vector<double> out;
  out.reserve(n);
  for (std::size_t position = start; position < start + n; ++position) {
    // Full-convolution index position: a[i] * v[position - i] over the overlap, a ascending.
    const std::size_t first = position + 1 >= m ? position + 1 - m : 0;
    const std::size_t last = std::min(position, n - 1);
    const bool plain = last - first + 1 == m && m <= 11;
    double sum = 0.0;
    for (std::size_t i = first; i <= last; ++i)
      sum = plain ? sum + (*a)[i] * (*v)[position - i] : std::fma((*a)[i], (*v)[position - i], sum);
    out.push_back(sum);
  }
  return out;
}

LleReferenceFeatures reference_lle_features(const EventLog& log, const std::string& activity_key) {
  EventLog sorted;
  std::vector<std::pair<std::int64_t, std::size_t>> firsts;
  for (const auto& trace : log.traces) {
    if (trace.events.empty()) throw std::invalid_argument("lle: empty traces are outside the observed domain");
    Trace copy = trace;
    std::stable_sort(copy.events.begin(), copy.events.end(),
                     [](const Event& x, const Event& y) { return micros_of(x) < micros_of(y); });
    firsts.emplace_back(micros_of(copy.events.front()), sorted.traces.size());
    sorted.traces.push_back(std::move(copy));
  }
  std::stable_sort(firsts.begin(), firsts.end(),
                   [](const auto& x, const auto& y) { return x.first < y.first; });
  EventLog ordered;
  LleReferenceFeatures out;
  for (const auto& [date, index] : firsts) {
    out.dates.push_back(date);
    ordered.traces.push_back(sorted.traces[index]);
  }
  LogToFeaturesOptions options;
  options.variant = LogToFeaturesVariant::TRACE_BASED;
  options.activity_key = activity_key;
  options.event_string_attributes = {activity_key};
  options.event_successor_attributes = {activity_key};
  options.event_string_attributes_supplied = options.event_numeric_attributes_supplied = true;
  options.case_string_attributes_supplied = options.case_numeric_attributes_supplied = true;
  options.event_successor_attributes_supplied = true;
  const auto features = log_to_features(ordered, options);
  out.names = features.feature_names;
  for (const auto& row : features.trace_rows) {
    std::vector<double> values;
    for (const auto& cell : row) {
      const auto* number = std::get_if<double>(&cell);
      if (!number) throw std::invalid_argument("lle: non-numeric feature cell");
      values.push_back(*number);
    }
    out.rows.push_back(std::move(values));
  }
  return out;
}

LleNeighbors reference_lle_neighbors(const LleMatrix& rows) {
  LleNeighbors out;
  for (const auto& row : rows) {
    std::vector<std::pair<double, std::size_t>> distances;
    for (std::size_t j = 0; j < rows.size(); ++j) {
      double squared = 0.0;
      for (std::size_t k = 0; k < row.size(); ++k) squared += (row[k] - rows[j][k]) * (row[k] - rows[j][k]);
      distances.emplace_back(squared, j);
    }
    std::sort(distances.begin(), distances.end());
    std::vector<std::size_t> nearest;
    for (std::size_t k = 0; k < kNeighbors + 1 && k < distances.size(); ++k) nearest.push_back(distances[k].second);
    out.push_back(std::move(nearest));
  }
  return out;
}

LleReferenceEmbedding reference_lle_embedding(const LleMatrix& rows, const LleNeighbors& neighbors) {
  const auto n = rows.size();
  if (n <= kNeighbors)
    raise("ValueError", "Expected n_neighbors < n_samples, but n_samples = " + std::to_string(n) +
                            ", n_neighbors = " + std::to_string(kNeighbors));
  LleMatrix w(n, std::vector<double>(n, 0.0));
  for (std::size_t i = 0; i < n; ++i) {
    const std::vector<std::size_t> chosen(neighbors.at(i).begin() + 1, neighbors.at(i).end());
    LleMatrix g(chosen.size(), std::vector<double>(chosen.size(), 0.0));
    double trace = 0.0;
    for (std::size_t p = 0; p < chosen.size(); ++p)
      for (std::size_t q = 0; q < chosen.size(); ++q) {
        double dot = 0.0;
        for (std::size_t k = 0; k < rows[i].size(); ++k)
          dot += (rows[chosen[p]][k] - rows[i][k]) * (rows[chosen[q]][k] - rows[i][k]);
        g[p][q] = dot;
        if (p == q) trace += dot;
      }
    const double r = trace > 0 ? kReg * trace : kReg;
    for (std::size_t p = 0; p < chosen.size(); ++p) g[p][p] += r;
    auto weights = solve_ones(g);
    const double total = std::accumulate(weights.begin(), weights.end(), 0.0);
    for (std::size_t p = 0; p < chosen.size(); ++p) w[i][chosen[p]] += weights[p] / total;
  }
  LleReferenceEmbedding out;
  out.m.assign(n, std::vector<double>(n, 0.0));
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < n; ++j) {
      double value = (i == j ? 1.0 : 0.0) - w[i][j] - w[j][i];
      for (std::size_t k = 0; k < n; ++k) value += w[k][i] * w[k][j];
      out.m[i][j] = value;
    }
  LleMatrix vectors;
  symmetric_eigen(out.m, out.eigenvalues, vectors);
  for (std::size_t r = 0; r < n; ++r) out.values.push_back(vectors[r][1]);
  return out;
}

std::int64_t reference_lle_box(std::size_t traces) {
  auto root = static_cast<std::int64_t>(std::sqrt(static_cast<double>(traces)));
  while (root * root > static_cast<std::int64_t>(traces)) --root;
  while ((root + 1) * (root + 1) <= static_cast<std::int64_t>(traces)) ++root;
  return root + 1;
}

LleReferenceResult reference_lle_apply(const EventLog& log, const std::string& activity_key,
                                       const std::function<LleNeighbors(const LleMatrix&)>& neighbors) {
  const auto features = reference_lle_features(log, activity_key);
  if (features.rows.size() <= kNeighbors)
    raise("ValueError", "Expected n_neighbors < n_samples, but n_samples = " + std::to_string(features.rows.size()) +
                            ", n_neighbors = " + std::to_string(kNeighbors));
  const auto chosen = neighbors ? neighbors(features.rows) : reference_lle_neighbors(features.rows);
  const auto embedding = reference_lle_embedding(features.rows, chosen);
  return {features.dates, reference_lle_smooth(embedding.values, reference_lle_box(features.rows.size()))};
}

}  // namespace minera
