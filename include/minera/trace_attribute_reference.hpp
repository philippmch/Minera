#pragma once

// The helper modules of algo.clustering.trace_attribute_driven in the PM4Py
// 2.7.22 reference (util.filter_subsets, variants.act_dist_calc,
// variants.suc_dist_calc, variants.sim_calc, variants.logslice_dist,
// leven_dist.leven_dist_calc, dfg.dfg_dist, linkage_method.linkage_avg,
// merge_log.merge_log). The package entry point (util.evaluation through
// discover_trace_attribute_clustering) lives in minera/clustering.hpp.
//
// A log is its traces' activity sequences; the functions that select traces
// by a trace attribute take TraceAttributeReferenceTrace.
//
// Variants. A log's variants are sorted by (count, variant) descending,
// variants compared as tuples of labels. sublog2varlist/sublog2df keep the
// variants seen at least freq_thres times, or the num most frequent when
// fewer remain; sublog2df lists them sorted by variant. sublog_percent keeps
// the most frequent variants while their cumulative share stays within the
// percent. logslice_percent cuts the variants into int(1 / unit) slices:
// slice k takes the next variants while the cumulative share stays within
// unit + (k - 1) * unit, empty slices left out.
//
// Distances. lev is the Levenshtein distance divided by the longer length.
// The cosine distance of two count vectors is max(0, 1 - ab / (|a| |b|)),
// NaN for a zero vector. DMM matches every variant of the longer list (the
// first when equally long) to its nearest variant of the other, and every
// unmatched variant of the other to its nearest; a pair weighs count x count,
// doubled for an exact (<= 1e-8) match; the distance is the weighted mean.
// The *_percent distances compare the sublog_percent variants (activity
// counts; succession counts where one side without successions is at
// distance 1), DMM or weighted average; one side empty gives NaN, both 0.
//
// act_sim / suc_sim reproduce the reference's weighting bug: the weights of
// variant i are the counts of sublog2df (sorted by variant), not of the
// given lists, and an index beyond those counts raises IndexError. act_sim_med
// takes per variant of the longer list the median of its distances found by
// numpy's (unstable) argsort, so the reference value is one of the tie
// choices reported by reference_ta_act_sim_med_outcomes.
//
// Linkage. The linkage_* functions merge, as long as two clusters remain, the
// first closest pair of the active clusters (ordered by position, a merged
// cluster taking the place of its first member). linkage_avg updates by the
// trace-count weighted mean; the *_update variants recompute the distance of
// the merged log (its members' traces concatenated) to each other cluster
// with dfg_dist_calc (alpha act + (1 - alpha) suc), act/suc_sim_percent
// (alpha act + (1 - alpha) suc) or leven_dist. A row is (min id, max id,
// distance, traces or members). The distance matrix is square.
//
// merge_log.clusteredlog cuts the linkage with scipy's fcluster(maxclust)
// and slices the log per flat cluster, in label order; merge_log.merge_log
// always raises TypeError in the pinned release.

#include <array>
#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace minera {

class TraceAttributeReferenceError : public std::runtime_error {
 public:
  TraceAttributeReferenceError(std::string type, const std::string& message)
      : std::runtime_error(message), type(std::move(type)) {}
  std::string type;  // the Python exception class
};

using TraceAttributeVariant = std::vector<std::string>;
using TraceAttributeLog = std::vector<TraceAttributeVariant>;

struct TraceAttributeVariantCount {
  TraceAttributeVariant variant;
  long count = 0;
};

struct TraceAttributeReferenceTrace {
  std::map<std::string, std::string> attributes;
  TraceAttributeVariant events;
};

// ---- util.filter_subsets ----

// apply_trace_attributes: the non-empty traces whose attribute key is (or,
// positive false, is not) among values; a missing attribute is not among them.
[[nodiscard]] std::vector<TraceAttributeReferenceTrace> reference_ta_apply_trace_attributes(
    const std::vector<TraceAttributeReferenceTrace>& log, const std::vector<std::string>& values,
    const std::string& key = "concept:name", bool positive = true);
[[nodiscard]] std::vector<TraceAttributeVariant> reference_ta_sublog2varlist(const TraceAttributeLog& log, long freq_thres,
                                                                             long num);
[[nodiscard]] std::vector<TraceAttributeVariantCount> reference_ta_sublog2df(const TraceAttributeLog& log, long freq_thres,
                                                                             long num);
[[nodiscard]] std::vector<TraceAttributeVariantCount> reference_ta_sublog2df_num(const TraceAttributeLog& log, long num);
// sublog_percent (and its aliases sublog_percent2actlist / 2varlist): the
// kept variants with counts, and the kept variants.
[[nodiscard]] std::pair<std::vector<TraceAttributeVariantCount>, std::vector<TraceAttributeVariant>>
reference_ta_sublog_percent(const TraceAttributeLog& log, double upper_percent);
[[nodiscard]] std::pair<std::vector<std::vector<TraceAttributeVariant>>, std::vector<long>> reference_ta_logslice_percent_act(
    const TraceAttributeLog& log, double unit);
[[nodiscard]] std::pair<std::vector<TraceAttributeLog>, std::vector<long>> reference_ta_logslice_percent(
    const TraceAttributeLog& log, double unit);
// apply_variants_filter: the traces of the admitted (positive) or the other
// variants, grouped per variant in first-appearance order.
[[nodiscard]] TraceAttributeLog reference_ta_apply_variants_filter(const TraceAttributeLog& log,
                                                                   const std::vector<TraceAttributeVariant>& admitted,
                                                                   bool positive = true);
// act_dist: always raises TypeError (it calls sublog2df without num).
[[noreturn]] void reference_ta_act_dist();

// ---- variants.act_dist_calc ----

[[nodiscard]] std::vector<std::pair<std::string, long>> reference_ta_occu_var_act(const TraceAttributeVariant& variant);
[[nodiscard]] double reference_ta_act_sim(const std::vector<TraceAttributeVariant>& var_list_1,
                                          const std::vector<TraceAttributeVariant>& var_list_2, const TraceAttributeLog& log1,
                                          const TraceAttributeLog& log2, long freq_thres, long num);
struct TraceAttributeMedianOutcomes {
  std::vector<double> values;  // every tie choice's result (NaN included)
  bool may_raise = false;      // some tie choice raises IndexError
};
// act_sim_med; raises when every tie choice raises.
[[nodiscard]] TraceAttributeMedianOutcomes reference_ta_act_sim_med_outcomes(
    const std::vector<TraceAttributeVariant>& var_list_1, const std::vector<TraceAttributeVariant>& var_list_2,
    const TraceAttributeLog& log1, const TraceAttributeLog& log2, long freq_thres, long num);
// act_sim_dual (and suc_sim_dual): 0, NaN without weights, IndexError as act_sim.
[[nodiscard]] double reference_ta_act_sim_dual(const std::vector<TraceAttributeVariant>& var_list_1,
                                               const std::vector<TraceAttributeVariant>& var_list_2,
                                               const TraceAttributeLog& log1, const TraceAttributeLog& log2, long freq_thres,
                                               long num);
[[nodiscard]] double reference_ta_act_sim_percent(const TraceAttributeLog& log1, const TraceAttributeLog& log2,
                                                  double percent_1, double percent_2);
// act_sim_percent_avg (act_sim_percent_avg_actset with a 'var' frame).
[[nodiscard]] double reference_ta_act_sim_percent_avg(const TraceAttributeLog& log1, const TraceAttributeLog& log2,
                                                      double percent_1, double percent_2);

// ---- variants.suc_dist_calc ----

using TraceAttributeSuccession = std::pair<std::string, std::string>;
// occu_suc: the first round(filter_percent * size) successions by frequency
// (ties among equal frequencies kept in input order here; the reference's
// sort is unstable).
[[nodiscard]] std::vector<std::pair<TraceAttributeSuccession, long>> reference_ta_occu_suc(
    const std::vector<std::pair<TraceAttributeSuccession, long>>& dfg, double filter_percent);
// occu_var_suc: "x,y" successions in first-appearance order, frequency 1 when
// binarised.
[[nodiscard]] std::vector<std::pair<std::string, long>> reference_ta_occu_var_suc(const TraceAttributeVariant& variant,
                                                                                  bool binarize = true);
[[nodiscard]] double reference_ta_suc_sim(const std::vector<TraceAttributeVariant>& var_list_1,
                                          const std::vector<TraceAttributeVariant>& var_list_2, const TraceAttributeLog& log1,
                                          const TraceAttributeLog& log2, long freq_thres, long num);
[[nodiscard]] double reference_ta_suc_sim_percent(const TraceAttributeLog& log1, const TraceAttributeLog& log2,
                                                  double percent_1, double percent_2);
[[nodiscard]] double reference_ta_suc_sim_percent_avg(const TraceAttributeLog& log1, const TraceAttributeLog& log2,
                                                      double percent_1, double percent_2);

// ---- variants.sim_calc ----

// dist_calc: alpha act_sim + (1 - alpha) suc_sim.
[[nodiscard]] double reference_ta_dist_calc(const std::vector<TraceAttributeVariant>& var_list_1,
                                            const std::vector<TraceAttributeVariant>& var_list_2,
                                            const TraceAttributeLog& log1, const TraceAttributeLog& log2, long freq_thres,
                                            long num, double alpha);
// inner_prod_calc: (x . y, |x|, |y|) of the freq_x and freq_y columns.
[[nodiscard]] std::array<double, 3> reference_ta_inner_prod_calc(const std::vector<double>& freq_x,
                                                                 const std::vector<double>& freq_y);

// ---- variants.logslice_dist ----

// log2sublog: the traces whose AMOUNT_REQ is value.
[[nodiscard]] std::vector<TraceAttributeReferenceTrace> reference_ta_logslice_log2sublog(
    const std::vector<TraceAttributeReferenceTrace>& log, const std::string& value);
[[nodiscard]] double reference_ta_slice_dist_act(const TraceAttributeLog& log1, const TraceAttributeLog& log2, double unit);
[[nodiscard]] double reference_ta_slice_dist_suc(const TraceAttributeLog& log1, const TraceAttributeLog& log2, double unit);

// ---- leven_dist.leven_dist_calc ----

[[nodiscard]] double reference_ta_leven_dist(const TraceAttributeLog& log1, const TraceAttributeLog& log2, double percent_1,
                                             double percent_2);
[[nodiscard]] double reference_ta_leven_dist_avg(const TraceAttributeLog& log1, const TraceAttributeLog& log2,
                                                 double percent_1, double percent_2);
// leven_preprocess: each element (a variant) becomes the letter of its rank
// among the distinct elements of both lists (a-z, A-Z; more raise IndexError).
[[nodiscard]] std::pair<std::string, std::string> reference_ta_leven_preprocess(
    const std::vector<TraceAttributeVariant>& list1, const std::vector<TraceAttributeVariant>& list2);

// ---- dfg.dfg_dist ----

[[nodiscard]] double reference_ta_dfg_dist_calc_act(const TraceAttributeLog& log1, const TraceAttributeLog& log2);
[[nodiscard]] double reference_ta_dfg_dist_calc_suc(const TraceAttributeLog& log1, const TraceAttributeLog& log2);
// dfg_dist_calc: (act, suc with NaN read as 1).
[[nodiscard]] std::pair<double, double> reference_ta_dfg_dist_calc(const TraceAttributeLog& log1,
                                                                   const TraceAttributeLog& log2);
[[nodiscard]] double reference_ta_dfg_dist_calc_minkowski(const TraceAttributeLog& log1, const TraceAttributeLog& log2,
                                                          double alpha);

// ---- linkage_method.linkage_avg ----

using TraceAttributeLinkage = std::vector<std::array<double, 4>>;
[[nodiscard]] TraceAttributeLinkage reference_ta_linkage_avg(const std::vector<TraceAttributeLog>& loglist,
                                                             const std::vector<std::vector<double>>& dist_mat, double alpha,
                                                             double percent);
[[nodiscard]] TraceAttributeLinkage reference_ta_linkage_dfg_update(const std::vector<TraceAttributeLog>& loglist,
                                                                    const std::vector<std::vector<double>>& dist_mat,
                                                                    double alpha, double percent);
[[nodiscard]] TraceAttributeLinkage reference_ta_linkage_dmm_update(const std::vector<TraceAttributeLog>& loglist,
                                                                    const std::vector<std::vector<double>>& dist_mat,
                                                                    double alpha, double percent);
[[nodiscard]] TraceAttributeLinkage reference_ta_linkage_dmm_update_leven(const std::vector<TraceAttributeLog>& loglist,
                                                                          const std::vector<std::vector<double>>& dist_mat,
                                                                          double alpha, double percent);

// ---- merge_log.merge_log ----

[[nodiscard]] std::vector<TraceAttributeReferenceTrace> reference_ta_update_merge(
    const std::vector<std::vector<TraceAttributeReferenceTrace>>& loglist);

struct TraceAttributeTreeNode {
  long node_id = -1;  // removed (-1) once labelled
  std::string name;
  std::vector<TraceAttributeTreeNode> children;
};
// add_node(to_tree(Z), parent): the dendrogram as nested node ids, left
// child first.
[[nodiscard]] TraceAttributeTreeNode reference_ta_add_node(const TraceAttributeLinkage& z);
// label_tree: leaves named id2name[node_id], inner nodes their sorted leaf
// names joined by '-'; returns the leaf names left to right.
std::vector<std::string> reference_ta_label_tree(TraceAttributeTreeNode& node, const std::vector<std::string>& id2name);
// scipy.cluster.hierarchy.fcluster(Z, maxclust, 'maxclust') for monotonic Z.
[[nodiscard]] std::vector<long> reference_ta_fcluster_maxclust(const TraceAttributeLinkage& z, long maxclust);
// clusteredlog: per label 1..maxclust the log sliced on attr_name by the
// values of that flat cluster (none when fcluster made fewer), and the values.
[[nodiscard]] std::pair<std::vector<std::vector<TraceAttributeReferenceTrace>>, std::vector<std::vector<std::string>>>
reference_ta_clusteredlog(const TraceAttributeLinkage& z, long maxclust, const std::vector<std::string>& list_of_vals,
                          const std::vector<TraceAttributeReferenceTrace>& log, const std::string& attr_name);
// log2sublog / logslice: the traces whose key is value / among values.
[[nodiscard]] std::vector<TraceAttributeReferenceTrace> reference_ta_merge_log2sublog(
    const std::vector<TraceAttributeReferenceTrace>& log, const std::string& value, const std::string& key);
[[nodiscard]] std::vector<TraceAttributeReferenceTrace> reference_ta_logslice(
    const std::vector<TraceAttributeReferenceTrace>& log, const std::vector<std::string>& values, const std::string& key);
// merge_log(path, cate, iter) with a string cate: always raises TypeError.
[[noreturn]] void reference_ta_merge_log(const std::string& path, const std::string& cate, long iterations);

}  // namespace minera
