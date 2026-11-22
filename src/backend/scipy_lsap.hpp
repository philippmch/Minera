#pragma once

// The assignment SciPy 1.17's `scipy.optimize.linear_sum_assignment` returns
// for a dense float64 cost matrix: the shortest augmenting path algorithm of
// Crouse (2016), one row at a time, columns kept in a "remaining" list filled
// in reverse order, a tie in the path cost resolved toward a column that is
// still free, and a tall matrix transposed first (its result sorted by row).
// Ties and the absorption of small costs next to huge ones (pm4py's
// sys.maxsize weights) follow the same floating-point steps, so the chosen
// assignment is SciPy's, not just an optimal one. Round FT: this model gave
// SciPy's rows and columns on 3,000 random matrices (integer ties, reals,
// sys.maxsize entries, wide and tall), observed black-box.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace minera::detail::scipy_lsap {

// `cost` is row-major with `rows` x `columns` entries. Returns the (row,
// column) pairs in SciPy's output order. Throws std::invalid_argument for an
// infeasible matrix (SciPy's "cost matrix is infeasible").
inline std::vector<std::pair<std::size_t, std::size_t>> linear_sum_assignment(
    std::vector<double> cost, std::size_t rows, std::size_t columns) {
  std::vector<std::pair<std::size_t, std::size_t>> result;
  if (rows == 0 || columns == 0) return result;
  const bool transpose = columns < rows;
  if (transpose) {
    std::vector<double> transposed(rows * columns);
    for (std::size_t i = 0; i < rows; ++i)
      for (std::size_t j = 0; j < columns; ++j)
        transposed[j * rows + i] = cost[i * columns + j];
    cost = std::move(transposed);
    std::swap(rows, columns);
  }
  const std::size_t nr = rows;
  const std::size_t nc = columns;
  constexpr double infinity = std::numeric_limits<double>::infinity();
  constexpr std::ptrdiff_t none = -1;
  std::vector<double> u(nr, 0.0), v(nc, 0.0), shortest(nc, infinity);
  std::vector<std::ptrdiff_t> path(nc, none), col4row(nr, none), row4col(nc, none);
  std::vector<char> sr(nr, 0), sc(nc, 0);
  std::vector<std::size_t> remaining(nc);
  for (std::size_t current = 0; current < nr; ++current) {
    double min_value = 0.0;
    std::size_t left = nc;
    for (std::size_t it = 0; it < nc; ++it) remaining[it] = nc - it - 1;
    std::fill(sr.begin(), sr.end(), 0);
    std::fill(sc.begin(), sc.end(), 0);
    std::fill(shortest.begin(), shortest.end(), infinity);
    std::size_t i = current;
    std::ptrdiff_t sink = none;
    while (sink == none) {
      std::size_t index = 0;
      bool found = false;
      double lowest = infinity;
      sr[i] = 1;
      for (std::size_t it = 0; it < left; ++it) {
        const std::size_t j = remaining[it];
        const double reduced = min_value + cost[i * nc + j] - u[i] - v[j];
        if (reduced < shortest[j]) {
          path[j] = static_cast<std::ptrdiff_t>(i);
          shortest[j] = reduced;
        }
        if (shortest[j] < lowest || (shortest[j] == lowest && row4col[j] == none)) {
          lowest = shortest[j];
          index = it;
          found = true;
        }
      }
      min_value = lowest;
      if (!found || min_value == infinity)
        throw std::invalid_argument("cost matrix is infeasible");
      const std::size_t j = remaining[index];
      if (row4col[j] == none) sink = static_cast<std::ptrdiff_t>(j);
      else i = static_cast<std::size_t>(row4col[j]);
      sc[j] = 1;
      remaining[index] = remaining[--left];
    }
    u[current] += min_value;
    for (std::size_t r = 0; r < nr; ++r)
      if (sr[r] && r != current)
        u[r] += min_value - shortest[static_cast<std::size_t>(col4row[r])];
    for (std::size_t c = 0; c < nc; ++c)
      if (sc[c]) v[c] -= min_value - shortest[c];
    std::ptrdiff_t j = sink;
    while (true) {
      const auto r = path[static_cast<std::size_t>(j)];
      row4col[static_cast<std::size_t>(j)] = r;
      std::swap(col4row[static_cast<std::size_t>(r)], j);
      if (static_cast<std::size_t>(r) == current) break;
    }
  }
  result.reserve(nr);
  if (transpose) {
    // SciPy sorts a transposed result by the original row (col4row here).
    std::vector<std::size_t> order(nr);
    for (std::size_t k = 0; k < nr; ++k) order[k] = k;
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
      return col4row[a] < col4row[b];
    });
    for (const auto k : order)
      result.emplace_back(static_cast<std::size_t>(col4row[k]), k);
  } else {
    for (std::size_t r = 0; r < nr; ++r)
      result.emplace_back(r, static_cast<std::size_t>(col4row[r]));
  }
  return result;
}

// The iteration order of a CPython 3.11 set built one element at a time from
// the consecutive ints first..first+count-1 (no collisions: each lands at its
// value masked by the table size, which grows to the next power of two above
// four times the fill once the fill reaches 3/5 of the mask). Round FT:
// checked against CPython for every first < 70 and count < 70 and larger
// sizes, observed black-box.
inline std::vector<std::size_t> cpython_consecutive_int_set_order(std::size_t first,
                                                                   std::size_t count) {
  std::size_t size = 8;
  for (std::size_t fill = 1; fill <= count; ++fill) {
    if (fill * 5 >= (size - 1) * 3) {
      std::size_t next = 8;
      const std::size_t minimum = fill > 50000 ? fill * 2 : fill * 4;
      while (next <= minimum) next <<= 1;
      size = next;
    }
  }
  std::vector<std::size_t> values(count);
  for (std::size_t k = 0; k < count; ++k) values[k] = first + k;
  std::stable_sort(values.begin(), values.end(), [&](std::size_t a, std::size_t b) {
    return (a & (size - 1)) < (b & (size - 1));
  });
  return values;
}

}  // namespace minera::detail::scipy_lsap
