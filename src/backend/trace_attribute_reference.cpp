#include "minera/trace_attribute_reference.hpp"

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace minera {
namespace {

using Variant = TraceAttributeVariant;
using Log = TraceAttributeLog;
using VariantCount = TraceAttributeVariantCount;
using Trace = TraceAttributeReferenceTrace;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

[[noreturn]] void index_out_of_bounds() {
  throw TraceAttributeReferenceError("IndexError", "single positional indexer is out-of-bounds");
}

// Variants by (count, variant) descending.
std::vector<VariantCount> variants(const Log& log) {
  std::map<Variant, std::int64_t> counts;
  for (const auto& trace : log) ++counts[trace];
  std::vector<VariantCount> out;
  for (const auto& [variant, count] : counts) out.push_back({variant, count});
  std::sort(out.begin(), out.end(), [](const VariantCount& a, const VariantCount& b) {
    return a.count != b.count ? a.count > b.count : b.variant < a.variant;
  });
  return out;
}

std::vector<VariantCount> selected(const Log& log, std::int64_t freq_thres, std::int64_t num) {
  auto all = variants(log);
  std::vector<VariantCount> kept;
  for (const auto& v : all)
    if (v.count >= freq_thres) kept.push_back(v);
  if (static_cast<std::int64_t>(kept.size()) >= num) return kept;
  if (num < static_cast<std::int64_t>(all.size())) all.resize(static_cast<std::size_t>(std::max(std::int64_t{0}, num)));
  return all;
}

std::vector<VariantCount> percent_records(const Log& log, double percent) {
  const auto all = variants(log);
  const auto n = static_cast<double>(log.size());
  std::vector<VariantCount> out;
  std::int64_t total = 0;
  for (const auto& v : all) {
    if (static_cast<double>(total + v.count) / n > percent) break;
    total += v.count;
    out.push_back(v);
  }
  return out;
}

std::vector<std::vector<VariantCount>> slices(const Log& log, double unit) {
  if (unit == 0.0) throw TraceAttributeReferenceError("ZeroDivisionError", "float division by zero");
  const auto all = variants(log);
  const auto n = static_cast<double>(log.size());
  const auto k_max = static_cast<std::int64_t>(1.0 / unit);
  std::vector<std::vector<VariantCount>> out;
  std::size_t i = 0;
  std::int64_t cumulative = 0;
  for (std::int64_t k = 1; k <= k_max; ++k) {
    const double bound = unit + static_cast<double>(k - 1) * unit;
    std::vector<VariantCount> current;
    while (i < all.size() && static_cast<double>(cumulative + all[i].count) / n <= bound) {
      cumulative += all[i].count;
      current.push_back(all[i++]);
    }
    if (!current.empty()) out.push_back(std::move(current));
  }
  return out;
}

using Counts = std::vector<std::pair<std::string, double>>;  // first-appearance order

void add(Counts& counts, const std::string& key, double by) {
  for (auto& [k, v] : counts)
    if (k == key) {
      v += by;
      return;
    }
  counts.emplace_back(key, by);
}

std::string succession_key(const std::string& a, const std::string& b) {
  return a + std::string(1, '\0') + b;
}

Counts activity_counts(const Variant& v) {
  Counts out;
  for (const auto& a : v) add(out, a, 1.0);
  return out;
}

Counts succession_counts(const Variant& v, bool binarize) {
  Counts out;
  for (std::size_t i = 0; i + 1 < v.size(); ++i) {
    const auto key = succession_key(v[i], v[i + 1]);
    if (binarize) {
      if (std::none_of(out.begin(), out.end(), [&](const auto& e) { return e.first == key; })) out.emplace_back(key, 1.0);
    } else {
      add(out, key, 1.0);
    }
  }
  return out;
}

Counts log_counts(const Log& log, bool successions) {
  Counts out;
  for (const auto& t : log) {
    if (successions) {
      for (std::size_t i = 0; i + 1 < t.size(); ++i) add(out, succession_key(t[i], t[i + 1]), 1.0);
    } else {
      for (const auto& a : t) add(out, a, 1.0);
    }
  }
  return out;
}

double lookup(const Counts& counts, const std::string& key) {
  for (const auto& [k, v] : counts)
    if (k == key) return v;
  return 0.0;
}

struct Products {
  double aa = 0, bb = 0, ab = 0;
};

Products products(const Counts& a, const Counts& b) {
  Products p;
  for (const auto& [k, v] : a) {
    p.aa += v * v;
    p.ab += v * lookup(b, k);
  }
  for (const auto& [k, v] : b) p.bb += v * v;
  return p;
}

// scipy's cosine distance: max(0, 1 - ab / (|a| |b|)), NaN for a zero vector.
double cosine(const Counts& a, const Counts& b) {
  const auto p = products(a, b);
  if (p.aa == 0 || p.bb == 0) return kNaN;
  return std::max(0.0, 1.0 - p.ab / (std::sqrt(p.aa) * std::sqrt(p.bb)));
}

// The *_percent cosine: one side without entries is NaN (activities) or at
// distance 1 (successions; both without, 1 as well).
double percent_cosine(const Counts& a, const Counts& b, bool successions) {
  if (!successions && (a.empty() || b.empty())) return kNaN;
  if (a.empty() || b.empty()) return 1.0;
  const auto p = products(a, b);
  return std::min(std::max(1.0 - p.ab / (std::sqrt(p.aa) * std::sqrt(p.bb)), 0.0), 1.0);
}

double levenshtein(const Variant& a, const Variant& b) {
  if (a.empty() && b.empty()) return 0.0;
  std::vector<std::size_t> previous(b.size() + 1), current(b.size() + 1);
  for (std::size_t j = 0; j <= b.size(); ++j) previous[j] = j;
  for (std::size_t i = 1; i <= a.size(); ++i) {
    current[0] = i;
    for (std::size_t j = 1; j <= b.size(); ++j)
      current[j] = std::min({current[j - 1] + 1, previous[j] + 1, previous[j - 1] + (a[i - 1] != b[j - 1] ? 1 : 0)});
    std::swap(previous, current);
  }
  return static_cast<double>(previous[b.size()]) / static_cast<double>(std::max(a.size(), b.size()));
}

// numpy argmin: the first NaN, else the first minimum.
std::size_t argmin(const std::vector<double>& values) {
  std::size_t best = 0;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (std::isnan(values[i])) return i;
    if (values[i] < values[best]) best = i;
  }
  return best;
}

struct Weighted {
  Variant variant;
  double weight = 0;
  Counts vector;  // for the slice distances
};

double dmm(const std::vector<Weighted>& a, const std::vector<Weighted>& b,
           const std::function<double(const Weighted&, const Weighted&)>& distance, bool clamp) {
  const auto& longer = a.size() >= b.size() ? a : b;
  const auto& shorter = a.size() >= b.size() ? b : a;
  std::vector<std::vector<double>> d(longer.size(), std::vector<double>(shorter.size()));
  for (std::size_t i = 0; i < longer.size(); ++i)
    for (std::size_t j = 0; j < shorter.size(); ++j) d[i][j] = distance(longer[i], shorter[j]);
  std::vector<bool> matched(shorter.size(), false);
  double sum = 0, weight = 0;
  for (std::size_t i = 0; i < longer.size(); ++i) {
    const auto j = argmin(d[i]);
    const double pair = longer[i].weight * shorter[j].weight;
    if (d[i][j] <= 1e-8) {
      matched[j] = true;
      weight += pair * 2;
      sum += d[i][j] * pair * 2;
    } else {
      weight += pair;
      sum += d[i][j] * pair;
    }
  }
  for (std::size_t j = 0; j < shorter.size(); ++j) {
    if (matched[j]) continue;
    std::vector<double> column(longer.size());
    for (std::size_t i = 0; i < longer.size(); ++i) column[i] = d[i][j];
    const auto i = argmin(column);
    const double pair = longer[i].weight * shorter[j].weight;
    weight += pair;
    sum += d[i][j] * pair;
  }
  if (!clamp) return sum / weight;
  return weight > 0 ? std::min(std::max(sum / weight, 0.0), 1.0) : 0.0;
}

std::vector<Weighted> weighted(const std::vector<VariantCount>& records) {
  std::vector<Weighted> out;
  for (const auto& r : records) out.push_back({r.variant, static_cast<double>(r.count), {}});
  return out;
}

// The weighted mean over all pairs: per-row sums, then their sum.
double average(const std::vector<VariantCount>& a, const std::vector<VariantCount>& b,
               const std::function<double(const Variant&, const Variant&)>& distance) {
  double sum = 0, weight = 0;
  for (const auto& x : a) {
    double row = 0;
    for (const auto& y : b) {
      row += distance(x.variant, y.variant) * static_cast<double>(x.count * y.count);
      weight += static_cast<double>(x.count * y.count);
    }
    sum += row;
  }
  return weight > 0 ? sum / weight : kNaN;
}

double percent_distance(const Log& log1, const Log& log2, double p1, double p2, bool dmm_mode, bool successions) {
  const auto a = percent_records(log1, p1);
  const auto b = percent_records(log2, p2);
  const auto vec = [successions](const Variant& v) {
    return successions ? succession_counts(v, false) : activity_counts(v);
  };
  const auto distance = [&](const Variant& x, const Variant& y) { return percent_cosine(vec(x), vec(y), successions); };
  if (!dmm_mode) return average(a, b, distance);
  if (a.empty() && b.empty()) return 0.0;
  if (a.empty() || b.empty()) return kNaN;
  if (a.size() == b.size() &&
      std::equal(a.begin(), a.end(), b.begin(), [](const auto& x, const auto& y) { return x.variant == y.variant; }))
    return 0.0;
  for (const auto& x : a)
    for (const auto& y : b)
      if (std::isnan(distance(x.variant, y.variant))) return kNaN;
  return dmm(weighted(a), weighted(b), [&](const Weighted& x, const Weighted& y) { return distance(x.variant, y.variant); },
             true);
}

// The counts act_sim and suc_sim weigh by: sublog2df's, sorted by variant.
std::vector<double> sorted_counts(const Log& log, std::int64_t freq_thres, std::int64_t num) {
  std::vector<double> out;
  for (const auto& r : reference_ta_sublog2df(log, freq_thres, num)) out.push_back(static_cast<double>(r.count));
  return out;
}

double pair_sim(const std::vector<Variant>& v1, const std::vector<Variant>& v2, const Log& log1, const Log& log2,
                std::int64_t freq_thres, std::int64_t num, const std::function<Counts(const Variant&)>& vec) {
  const auto k1 = sorted_counts(log1, freq_thres, num);
  const auto k2 = sorted_counts(log2, freq_thres, num);
  const bool first = v1.size() >= v2.size();
  const auto& longer = first ? v1 : v2;
  const auto& shorter = first ? v2 : v1;
  const auto& kl = first ? k1 : k2;
  const auto& ks = first ? k2 : k1;
  double sl = 0, ss = 0;
  for (const auto c : kl) sl += c;
  for (const auto c : ks) ss += c;
  const double den = sl * ss;
  if (v1 == v2) return den != 0 ? 0.0 : kNaN;
  if (!shorter.empty() && (longer.size() > kl.size() || shorter.size() > ks.size())) index_out_of_bounds();
  double sum = 0;
  for (std::size_t i = 0; i < longer.size(); ++i)
    for (std::size_t j = 0; j < shorter.size(); ++j) sum += cosine(vec(longer[i]), vec(shorter[j])) * kl[i] * ks[j];
  return den != 0 ? sum / den : kNaN;
}

double slice_distance(const Log& log1, const Log& log2, double unit, bool successions) {
  if (reference_ta_sublog2varlist(log1, 0, 0) == reference_ta_sublog2varlist(log2, 0, 0)) return 0.0;
  const auto s1 = reference_ta_logslice_percent(log1, unit);
  const auto s2 = reference_ta_logslice_percent(log2, unit);
  if (s1.first.empty() && s2.first.empty()) return 0.0;
  if (s1.first.empty() || s2.first.empty()) return kNaN;
  const auto build = [successions](const std::pair<std::vector<Log>, std::vector<std::int64_t>>& s) {
    std::vector<Weighted> out;
    for (std::size_t i = 0; i < s.first.size(); ++i)
      out.push_back({{}, static_cast<double>(s.second[i]), log_counts(s.first[i], successions)});
    return out;
  };
  return dmm(build(s1), build(s2), [](const Weighted& x, const Weighted& y) { return cosine(x.vector, y.vector); }, false);
}

bool attribute_inside(const Trace& trace, const std::vector<std::string>& values, const std::string& key) {
  const auto found = trace.attributes.find(key);
  return found != trace.attributes.end() && std::find(values.begin(), values.end(), found->second) != values.end();
}

double minkowski(const Counts& a, const Counts& b) {
  Counts keys = a;
  for (const auto& [k, v] : b) add(keys, k, 0.0);
  if (keys.empty()) throw TraceAttributeReferenceError("ValueError", "Unsupported dtype object");
  double sa = 0, sb = 0;
  for (const auto& [k, v] : a) sa += v;
  for (const auto& [k, v] : b) sb += v;
  if (sa == 0 || sb == 0) throw TraceAttributeReferenceError("ZeroDivisionError", "division by zero");
  double sum = 0;
  for (const auto& [k, v] : keys) {
    const double d = lookup(a, k) / sa - lookup(b, k) / sb;
    sum += d * d;
  }
  return std::sqrt(sum);
}

double dfg_cosine(const Log& log1, const Log& log2, bool successions) {
  const auto a = log_counts(log1, successions);
  const auto b = log_counts(log2, successions);
  const auto p = products(a, b);
  if (p.aa == 0 || p.bb == 0) return kNaN;
  return 1.0 - p.ab / (std::sqrt(p.aa) * std::sqrt(p.bb));
}

TraceAttributeLinkage linkage(const std::vector<Log>& logs, const std::vector<std::vector<double>>& matrix, bool weighted_mean,
                              const std::function<double(const Log&, const Log&)>& recompute) {
  const auto n = logs.size();
  if (n < 2) throw TraceAttributeReferenceError("IndexError", "list index out of range");
  std::map<std::pair<std::size_t, std::size_t>, double> d;
  for (std::size_t a = 0; a < n; ++a)
    for (std::size_t b = 0; b < n; ++b)
      if (a != b) d[{a, b}] = matrix[std::min(a, b)][std::max(a, b)];
  std::vector<std::size_t> active(n);
  std::vector<double> size(2 * n - 1);
  std::vector<Log> content(2 * n - 1);
  for (std::size_t i = 0; i < n; ++i) {
    active[i] = i;
    size[i] = weighted_mean ? static_cast<double>(logs[i].size()) : 1.0;
    content[i] = logs[i];
  }
  TraceAttributeLinkage z;
  for (std::size_t step = 0; step + 1 < n; ++step) {
    std::vector<std::pair<std::size_t, std::size_t>> pairs;
    std::vector<double> values;
    for (std::size_t p = 0; p < active.size(); ++p)
      for (std::size_t q = p + 1; q < active.size(); ++q) {
        pairs.emplace_back(active[p], active[q]);
        values.push_back(d[{active[p], active[q]}]);
      }
    const auto [x, y] = pairs[argmin(values)];
    const double distance = d[{x, y}];
    const auto merged = n + step;
    size[merged] = size[x] + size[y];
    content[merged] = content[x];
    content[merged].insert(content[merged].end(), content[y].begin(), content[y].end());
    z.push_back({static_cast<double>(std::min(x, y)), static_cast<double>(std::max(x, y)), distance, size[merged]});
    *std::find(active.begin(), active.end(), x) = merged;
    active.erase(std::find(active.begin(), active.end(), y));
    for (const auto other : active) {
      if (other == merged) continue;
      const double value = weighted_mean ? (d[{x, other}] * size[x] + d[{y, other}] * size[y]) / (size[x] + size[y])
                                         : recompute(content[merged], content[other]);
      d[{merged, other}] = value;
      d[{other, merged}] = value;
    }
  }
  return z;
}

void add_subtree(const TraceAttributeLinkage& z, std::int64_t id, TraceAttributeTreeNode& parent) {
  const auto n = static_cast<std::int64_t>(z.size()) + 1;
  TraceAttributeTreeNode node;
  node.node_id = id;
  if (id >= n) {
    const auto& row = z[static_cast<std::size_t>(id - n)];
    add_subtree(z, static_cast<std::int64_t>(row[0]), node);
    add_subtree(z, static_cast<std::int64_t>(row[1]), node);
  }
  parent.children.push_back(std::move(node));
}

}  // namespace

// ---- util.filter_subsets ----

std::vector<Trace> reference_ta_apply_trace_attributes(const std::vector<Trace>& log, const std::vector<std::string>& values,
                                                       const std::string& key, bool positive) {
  std::vector<Trace> out;
  for (const auto& trace : log)
    if (attribute_inside(trace, values, key) == positive && !trace.events.empty()) out.push_back(trace);
  return out;
}

std::vector<Variant> reference_ta_sublog2varlist(const Log& log, std::int64_t freq_thres, std::int64_t num) {
  std::vector<Variant> out;
  for (const auto& r : selected(log, freq_thres, num)) out.push_back(r.variant);
  return out;
}

std::vector<VariantCount> reference_ta_sublog2df(const Log& log, std::int64_t freq_thres, std::int64_t num) {
  auto out = selected(log, freq_thres, num);
  std::sort(out.begin(), out.end(), [](const VariantCount& a, const VariantCount& b) { return a.variant < b.variant; });
  return out;
}

std::vector<VariantCount> reference_ta_sublog2df_num(const Log& log, std::int64_t num) {
  auto out = variants(log);
  if (num < static_cast<std::int64_t>(out.size())) out.resize(static_cast<std::size_t>(std::max(std::int64_t{0}, num)));
  return out;
}

std::pair<std::vector<VariantCount>, std::vector<Variant>> reference_ta_sublog_percent(const Log& log, double upper_percent) {
  auto records = percent_records(log, upper_percent);
  std::vector<Variant> list;
  for (const auto& r : records) list.push_back(r.variant);
  return {std::move(records), std::move(list)};
}

std::pair<std::vector<std::vector<Variant>>, std::vector<std::int64_t>> reference_ta_logslice_percent_act(const Log& log, double unit) {
  std::pair<std::vector<std::vector<Variant>>, std::vector<std::int64_t>> out;
  for (const auto& slice : slices(log, unit)) {
    std::vector<Variant> list;
    std::int64_t count = 0;
    for (const auto& r : slice) {
      list.push_back(r.variant);
      count += r.count;
    }
    out.first.push_back(std::move(list));
    out.second.push_back(count);
  }
  return out;
}

std::pair<std::vector<Log>, std::vector<std::int64_t>> reference_ta_logslice_percent(const Log& log, double unit) {
  const auto act = reference_ta_logslice_percent_act(log, unit);
  std::pair<std::vector<Log>, std::vector<std::int64_t>> out;
  for (const auto& list : act.first) out.first.push_back(reference_ta_apply_variants_filter(log, list, true));
  out.second = act.second;
  return out;
}

Log reference_ta_apply_variants_filter(const Log& log, const std::vector<Variant>& admitted, bool positive) {
  const std::set<Variant> allowed(admitted.begin(), admitted.end());
  std::vector<Variant> order;
  std::map<Variant, std::int64_t> counts;
  for (const auto& t : log) {
    if (allowed.contains(t) != positive) continue;
    if (counts[t]++ == 0) order.push_back(t);
  }
  Log out;
  for (const auto& v : order) out.insert(out.end(), static_cast<std::size_t>(counts[v]), v);
  return out;
}

void reference_ta_act_dist() {
  throw TraceAttributeReferenceError("TypeError", "sublog2df() missing 1 required positional argument: 'num'");
}

// ---- variants.act_dist_calc ----

std::vector<std::pair<std::string, std::int64_t>> reference_ta_occu_var_act(const Variant& variant) {
  std::vector<std::pair<std::string, std::int64_t>> out;
  for (const auto& [k, v] : activity_counts(variant)) out.emplace_back(k, static_cast<std::int64_t>(v));
  return out;
}

double reference_ta_act_sim(const std::vector<Variant>& v1, const std::vector<Variant>& v2, const Log& log1, const Log& log2,
                            std::int64_t freq_thres, std::int64_t num) {
  return pair_sim(v1, v2, log1, log2, freq_thres, num, activity_counts);
}

TraceAttributeMedianOutcomes reference_ta_act_sim_med_outcomes(const std::vector<Variant>& v1, const std::vector<Variant>& v2,
                                                               const Log& log1, const Log& log2, std::int64_t freq_thres, std::int64_t num) {
  TraceAttributeMedianOutcomes out;
  if (v1 == v2) {
    out.values.push_back(kNaN);
    return out;
  }
  const auto k1 = sorted_counts(log1, freq_thres, num);
  const auto k2 = sorted_counts(log2, freq_thres, num);
  const bool first = v1.size() >= v2.size();
  const auto& longer = first ? v1 : v2;
  const auto& shorter = first ? v2 : v1;
  const auto& kl = first ? k1 : k2;
  const auto& ks = first ? k2 : k1;
  if (shorter.empty()) {
    out.values.push_back(kNaN);
    return out;
  }
  // Per row the (value, weight) choices among the tied medians.
  std::vector<std::vector<std::pair<double, double>>> rows;
  for (std::size_t i = 0; i < longer.size(); ++i) {
    std::vector<double> row;
    for (const auto& y : shorter) row.push_back(cosine(activity_counts(longer[i]), activity_counts(y)));
    auto sorted = row;
    std::sort(sorted.begin(), sorted.end());
    const double median = sorted[row.size() / 2];
    if (i >= kl.size()) index_out_of_bounds();
    std::set<std::pair<double, double>> choices;
    for (std::size_t j = 0; j < row.size(); ++j) {
      if (row[j] != median) continue;
      if (j < ks.size()) {
        choices.insert({median * kl[i] * ks[j], kl[i] * ks[j]});
      } else {
        out.may_raise = true;
      }
    }
    if (choices.empty()) index_out_of_bounds();
    rows.emplace_back(choices.begin(), choices.end());
  }
  std::set<double> results;
  std::vector<std::size_t> pick(rows.size(), 0);
  for (;;) {
    double sum = 0, weight = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
      sum += rows[i][pick[i]].first;
      weight += rows[i][pick[i]].second;
    }
    results.insert(sum / weight);
    std::size_t i = 0;
    while (i < rows.size() && ++pick[i] == rows[i].size()) pick[i++] = 0;
    if (i == rows.size()) break;
  }
  out.values.assign(results.begin(), results.end());
  return out;
}

double reference_ta_act_sim_dual(const std::vector<Variant>& v1, const std::vector<Variant>& v2, const Log& log1,
                                 const Log& log2, std::int64_t freq_thres, std::int64_t num) {
  const auto k1 = sorted_counts(log1, freq_thres, num);
  const auto k2 = sorted_counts(log2, freq_thres, num);
  const bool first = v1.size() >= v2.size();
  const auto& longer = first ? v1 : v2;
  const auto& shorter = first ? v2 : v1;
  const auto& kl = first ? k1 : k2;
  const auto& ks = first ? k2 : k1;
  double sl = 0, ss = 0;
  for (const auto c : kl) sl += c;
  for (const auto c : ks) ss += c;
  if (v1 != v2 && !shorter.empty() && (longer.size() > kl.size() || shorter.size() > ks.size())) index_out_of_bounds();
  return sl * ss != 0 ? 0.0 : kNaN;
}

double reference_ta_act_sim_percent(const Log& log1, const Log& log2, double percent_1, double percent_2) {
  return percent_distance(log1, log2, percent_1, percent_2, true, false);
}

double reference_ta_act_sim_percent_avg(const Log& log1, const Log& log2, double percent_1, double percent_2) {
  return percent_distance(log1, log2, percent_1, percent_2, false, false);
}

// ---- variants.suc_dist_calc ----

std::vector<std::pair<TraceAttributeSuccession, std::int64_t>> reference_ta_occu_suc(
    const std::vector<std::pair<TraceAttributeSuccession, std::int64_t>>& dfg, double filter_percent) {
  auto out = dfg;
  std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
  // Python's round: half to even.
  const double keep = std::nearbyint(filter_percent * static_cast<double>(dfg.size()));
  if (keep < static_cast<double>(out.size())) out.resize(static_cast<std::size_t>(std::max(0.0, keep)));
  return out;
}

std::vector<std::pair<std::string, std::int64_t>> reference_ta_occu_var_suc(const Variant& variant, bool binarize) {
  std::vector<std::pair<std::string, std::int64_t>> out;
  for (const auto& [k, v] : succession_counts(variant, binarize)) {
    const auto at = k.find('\0');
    out.emplace_back(k.substr(0, at) + "," + k.substr(at + 1), static_cast<std::int64_t>(v));
  }
  return out;
}

double reference_ta_suc_sim(const std::vector<Variant>& v1, const std::vector<Variant>& v2, const Log& log1, const Log& log2,
                            std::int64_t freq_thres, std::int64_t num) {
  return pair_sim(v1, v2, log1, log2, freq_thres, num, [](const Variant& v) { return succession_counts(v, false); });
}

double reference_ta_suc_sim_percent(const Log& log1, const Log& log2, double percent_1, double percent_2) {
  return percent_distance(log1, log2, percent_1, percent_2, true, true);
}

double reference_ta_suc_sim_percent_avg(const Log& log1, const Log& log2, double percent_1, double percent_2) {
  return percent_distance(log1, log2, percent_1, percent_2, false, true);
}

// ---- variants.sim_calc ----

double reference_ta_dist_calc(const std::vector<Variant>& v1, const std::vector<Variant>& v2, const Log& log1, const Log& log2,
                              std::int64_t freq_thres, std::int64_t num, double alpha) {
  const double act = reference_ta_act_sim(v1, v2, log1, log2, freq_thres, num);
  const double suc = reference_ta_suc_sim(v1, v2, log1, log2, freq_thres, num);
  return alpha * act + (1 - alpha) * suc;
}

std::array<double, 3> reference_ta_inner_prod_calc(const std::vector<double>& freq_x, const std::vector<double>& freq_y) {
  double xy = 0, xx = 0, yy = 0;
  for (std::size_t i = 0; i < freq_x.size(); ++i) {
    xy += freq_x[i] * freq_y[i];
    xx += freq_x[i] * freq_x[i];
    yy += freq_y[i] * freq_y[i];
  }
  return {xy, std::sqrt(xx), std::sqrt(yy)};
}

// ---- variants.logslice_dist ----

std::vector<Trace> reference_ta_logslice_log2sublog(const std::vector<Trace>& log, const std::string& value) {
  return reference_ta_apply_trace_attributes(log, {value}, "AMOUNT_REQ", true);
}

double reference_ta_slice_dist_act(const Log& log1, const Log& log2, double unit) {
  return slice_distance(log1, log2, unit, false);
}

double reference_ta_slice_dist_suc(const Log& log1, const Log& log2, double unit) {
  return slice_distance(log1, log2, unit, true);
}

// ---- leven_dist.leven_dist_calc ----

double reference_ta_leven_dist(const Log& log1, const Log& log2, double percent_1, double percent_2) {
  const auto a = percent_records(log1, percent_1);
  const auto b = percent_records(log2, percent_2);
  if (a.empty() && b.empty()) return 0.0;
  if (a.empty() || b.empty()) return kNaN;
  return dmm(weighted(a), weighted(b), [](const Weighted& x, const Weighted& y) { return levenshtein(x.variant, y.variant); },
             true);
}

double reference_ta_leven_dist_avg(const Log& log1, const Log& log2, double percent_1, double percent_2) {
  return average(percent_records(log1, percent_1), percent_records(log2, percent_2), levenshtein);
}

std::pair<std::string, std::string> reference_ta_leven_preprocess(const std::vector<Variant>& list1,
                                                                  const std::vector<Variant>& list2) {
  static const std::string letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  std::set<Variant> distinct(list1.begin(), list1.end());
  distinct.insert(list2.begin(), list2.end());
  if (distinct.size() > letters.size()) throw TraceAttributeReferenceError("IndexError", "list index out of range");
  std::map<Variant, char> letter;
  for (const auto& v : distinct) letter[v] = letters[letter.size()];
  std::pair<std::string, std::string> out;
  for (const auto& v : list1) out.first += letter[v];
  for (const auto& v : list2) out.second += letter[v];
  return out;
}

// ---- dfg.dfg_dist ----

double reference_ta_dfg_dist_calc_act(const Log& log1, const Log& log2) { return dfg_cosine(log1, log2, false); }

double reference_ta_dfg_dist_calc_suc(const Log& log1, const Log& log2) { return dfg_cosine(log1, log2, true); }

std::pair<double, double> reference_ta_dfg_dist_calc(const Log& log1, const Log& log2) {
  const double act = reference_ta_dfg_dist_calc_act(log1, log2);
  const double suc = reference_ta_dfg_dist_calc_suc(log1, log2);
  return {act, std::isnan(suc) ? 1.0 : suc};
}

double reference_ta_dfg_dist_calc_minkowski(const Log& log1, const Log& log2, double alpha) {
  const double act = minkowski(log_counts(log1, false), log_counts(log2, false));
  const double suc = minkowski(log_counts(log1, true), log_counts(log2, true));
  return alpha * act + (1 - alpha) * suc;
}

// ---- linkage_method.linkage_avg ----

TraceAttributeLinkage reference_ta_linkage_avg(const std::vector<Log>& loglist, const std::vector<std::vector<double>>& dist_mat,
                                               double, double) {
  return linkage(loglist, dist_mat, true, nullptr);
}

TraceAttributeLinkage reference_ta_linkage_dfg_update(const std::vector<Log>& loglist,
                                                      const std::vector<std::vector<double>>& dist_mat, double alpha, double) {
  return linkage(loglist, dist_mat, false, [alpha](const Log& a, const Log& b) {
    const auto [act, suc] = reference_ta_dfg_dist_calc(a, b);
    return alpha * act + (1 - alpha) * suc;
  });
}

TraceAttributeLinkage reference_ta_linkage_dmm_update(const std::vector<Log>& loglist,
                                                      const std::vector<std::vector<double>>& dist_mat, double alpha,
                                                      double percent) {
  return linkage(loglist, dist_mat, false, [alpha, percent](const Log& a, const Log& b) {
    return alpha * reference_ta_act_sim_percent(a, b, percent, percent) +
           (1 - alpha) * reference_ta_suc_sim_percent(a, b, percent, percent);
  });
}

TraceAttributeLinkage reference_ta_linkage_dmm_update_leven(const std::vector<Log>& loglist,
                                                            const std::vector<std::vector<double>>& dist_mat, double,
                                                            double percent) {
  return linkage(loglist, dist_mat, false,
                 [percent](const Log& a, const Log& b) { return reference_ta_leven_dist(a, b, percent, percent); });
}

// ---- merge_log.merge_log ----

std::vector<Trace> reference_ta_update_merge(const std::vector<std::vector<Trace>>& loglist) {
  std::vector<Trace> out;
  for (const auto& log : loglist) out.insert(out.end(), log.begin(), log.end());
  return out;
}

TraceAttributeTreeNode reference_ta_add_node(const TraceAttributeLinkage& z) {
  TraceAttributeTreeNode parent;
  add_subtree(z, static_cast<std::int64_t>(2 * z.size()), parent);
  return std::move(parent.children.front());
}

std::vector<std::string> reference_ta_label_tree(TraceAttributeTreeNode& node, const std::vector<std::string>& id2name) {
  std::vector<std::string> leaves;
  if (node.children.empty()) {
    leaves.push_back(id2name.at(static_cast<std::size_t>(node.node_id)));
  } else {
    for (auto& child : node.children) {
      const auto names = reference_ta_label_tree(child, id2name);
      leaves.insert(leaves.end(), names.begin(), names.end());
    }
  }
  auto sorted = leaves;
  std::sort(sorted.begin(), sorted.end());
  node.name.clear();
  for (const auto& name : sorted) node.name += (node.name.empty() ? "" : "-") + name;
  node.node_id = -1;
  return leaves;
}

std::vector<std::int64_t> reference_ta_fcluster_maxclust(const TraceAttributeLinkage& z, std::int64_t maxclust) {
  const auto n = z.size() + 1;
  std::vector<std::int64_t> labels(n, 0);
  if (maxclust >= static_cast<std::int64_t>(n)) {
    for (std::size_t i = 0; i < n; ++i) labels[i] = static_cast<std::int64_t>(i) + 1;
    return labels;
  }
  // The smallest merge height leaving at most maxclust clusters (0 if none).
  std::vector<double> heights;
  for (const auto& row : z) heights.push_back(row[2]);
  std::sort(heights.begin(), heights.end());
  double threshold = 0.0;
  for (const auto h : heights) {
    const auto merged = std::count_if(z.begin(), z.end(), [h](const auto& row) { return row[2] <= h; });
    if (static_cast<std::int64_t>(n) - merged <= maxclust) {
      threshold = h;
      break;
    }
  }
  std::int64_t next = 0;
  std::function<void(std::size_t, std::int64_t)> paint = [&](std::size_t id, std::int64_t label) {
    if (id < n) {
      labels[id] = label;
      return;
    }
    paint(static_cast<std::size_t>(z[id - n][0]), label);
    paint(static_cast<std::size_t>(z[id - n][1]), label);
  };
  // Below the cut: inner children first (left, right), then leaf children.
  std::function<void(std::size_t)> visit = [&](std::size_t id) {
    const auto& row = z[id - n];
    if (row[2] <= threshold) {
      paint(id, ++next);
      return;
    }
    for (int side = 0; side < 2; ++side)
      if (static_cast<std::size_t>(row[side]) >= n) visit(static_cast<std::size_t>(row[side]));
    for (int side = 0; side < 2; ++side)
      if (static_cast<std::size_t>(row[side]) < n) labels[static_cast<std::size_t>(row[side])] = ++next;
  };
  visit(2 * n - 2);
  return labels;
}

std::pair<std::vector<std::vector<Trace>>, std::vector<std::vector<std::string>>> reference_ta_clusteredlog(
    const TraceAttributeLinkage& z, std::int64_t maxclust, const std::vector<std::string>& list_of_vals, const std::vector<Trace>& log,
    const std::string& attr_name) {
  const auto labels = reference_ta_fcluster_maxclust(z, maxclust);
  // One group per label 1..maxclust, empty when fcluster used fewer.
  std::pair<std::vector<std::vector<Trace>>, std::vector<std::vector<std::string>>> out;
  for (std::int64_t label = 1; label <= maxclust; ++label) {
    std::vector<std::string> values;
    for (std::size_t i = 0; i < labels.size() && i < list_of_vals.size(); ++i)
      if (labels[i] == label) values.push_back(list_of_vals[i]);
    out.first.push_back(reference_ta_logslice(log, values, attr_name));
    out.second.push_back(std::move(values));
  }
  return out;
}

std::vector<Trace> reference_ta_merge_log2sublog(const std::vector<Trace>& log, const std::string& value,
                                                 const std::string& key) {
  return reference_ta_apply_trace_attributes(log, {value}, key, true);
}

std::vector<Trace> reference_ta_logslice(const std::vector<Trace>& log, const std::vector<std::string>& values,
                                         const std::string& key) {
  return reference_ta_apply_trace_attributes(log, values, key, true);
}

void reference_ta_merge_log(const std::string&, const std::string&, std::int64_t) {
  throw TraceAttributeReferenceError("TypeError", "can only concatenate str (not \"int\") to str");
}

}  // namespace minera
