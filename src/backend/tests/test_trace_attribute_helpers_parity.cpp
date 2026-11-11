// The helper modules of algo.clustering.trace_attribute_driven against the
// public seeded bank (docs/trace-attribute-helpers-bank.txt.gz,
// scripts/capture-trace-attribute-helpers-bank.py): every selection, slice,
// filter, count table, tree and error (class and message) must equal the
// reference's, every distance agree within 1e-12. act_sim_med must give one
// of its tie choices; occu_suc must pick the same frequencies (and the same
// successions wherever a frequency is taken whole).
#include "minera/trace_attribute_reference.hpp"

#include "../gzip_codec.hpp"

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace minera;

std::vector<std::string> split(const std::string& text, char separator) {
  std::vector<std::string> out;
  std::string piece;
  std::istringstream stream(text);
  while (std::getline(stream, piece, separator))
    if (!piece.empty()) out.push_back(piece);
  return out;
}

std::vector<std::string> tokens(const std::string& text) {
  return text == "." ? std::vector<std::string>{} : split(text, ' ');
}

TraceAttributeVariant variant(const std::string& word) {
  TraceAttributeVariant out;
  if (word == "-") return out;
  for (const char c : word) out.emplace_back(1, c);
  return out;
}

std::string word(const TraceAttributeVariant& v) {
  std::string out;
  for (const auto& a : v) out += a;
  return out.empty() ? "-" : out;
}

TraceAttributeLog traces(const std::string& text) {
  TraceAttributeLog out;
  if (text == ".") return out;
  for (const auto& w : split(text, ',')) out.push_back(variant(w));
  return out;
}

std::string traces_text(const TraceAttributeLog& log) {
  std::string out;
  for (const auto& t : log) out += (out.empty() ? "" : ",") + word(t);
  return out.empty() ? "." : out;
}

std::vector<TraceAttributeReferenceTrace> attributed(const std::string& text, const std::string& key = "grp") {
  std::vector<TraceAttributeReferenceTrace> out;
  if (text == ".") return out;
  for (const auto& item : split(text, ',')) {
    const auto at = item.find('@');
    TraceAttributeReferenceTrace trace;
    trace.events = variant(item.substr(0, at));
    trace.attributes["concept:name"] = "c" + std::to_string(out.size());
    if (item.substr(at + 1) != "~") trace.attributes[key] = item.substr(at + 1);
    out.push_back(std::move(trace));
  }
  return out;
}

std::string names_text(const std::vector<TraceAttributeReferenceTrace>& log) {
  std::string out;
  for (const auto& t : log) out += (out.empty() ? "" : " ") + t.attributes.at("concept:name");
  return out.empty() ? "." : out;
}

std::string records_text(const std::vector<TraceAttributeVariantCount>& records) {
  std::string out;
  for (const auto& r : records) out += (out.empty() ? "" : " ") + word(r.variant) + "=" + std::to_string(r.count);
  return out.empty() ? "." : out;
}

std::string pairs_text(const std::vector<std::pair<std::string, std::int64_t>>& pairs) {
  std::string out;
  for (const auto& [k, v] : pairs) out += (out.empty() ? "" : " ") + (k.empty() ? "-" : k) + "=" + std::to_string(v);
  return out.empty() ? "." : out;
}

std::string longs_text(const std::vector<std::int64_t>& values) {
  std::string out;
  for (const auto v : values) out += (out.empty() ? "" : " ") + std::to_string(v);
  return out.empty() ? "." : out;
}

double number(const std::string& text) { return text == "nan" ? std::nan("") : std::strtod(text.c_str(), nullptr); }

bool close(double expected, double actual) {
  return (std::isnan(expected) && std::isnan(actual)) || std::abs(expected - actual) <= 1e-12;
}

TraceAttributeLinkage linkage_rows(const std::string& text) {
  TraceAttributeLinkage out;
  if (text == ".") return out;
  for (const auto& row : split(text, ';')) {
    const auto values = tokens(row);
    out.push_back({number(values.at(0)), number(values.at(1)), number(values.at(2)), number(values.at(3))});
  }
  return out;
}

bool same_linkage(const TraceAttributeLinkage& expected, const TraceAttributeLinkage& actual) {
  if (expected.size() != actual.size()) return false;
  for (std::size_t i = 0; i < expected.size(); ++i)
    if (expected[i][0] != actual[i][0] || expected[i][1] != actual[i][1] || !close(expected[i][2], actual[i][2]) ||
        expected[i][3] != actual[i][3])
      return false;
  return true;
}

std::string tree_text(const TraceAttributeTreeNode& node) {
  std::string out = "(" + (node.node_id >= 0 ? std::to_string(node.node_id) : node.name);
  for (const auto& child : node.children) out += " " + tree_text(child);
  return out + ")";
}

struct Line {
  std::string tag;
  std::vector<std::string> fields;
  std::string text;
};

Line parse(const std::string& text) {
  Line out;
  out.text = text;
  const auto space = text.find(' ');
  out.tag = text.substr(0, space);
  std::size_t start = space + 1;
  for (;;) {
    const auto at = text.find(" | ", start);
    out.fields.push_back(text.substr(start, at == std::string::npos ? std::string::npos : at - start));
    if (at == std::string::npos) return out;
    start = at + 3;
  }
}

std::size_t failures = 0;

void report(const Line& line, const std::string& expected, const std::string& actual) {
  if (expected != actual && ++failures <= 25)
    std::cerr << line.text.substr(0, 300) << "\n  expected " << expected.substr(0, 300) << "\n  got      "
              << actual.substr(0, 300) << '\n';
}

// The outcome of call as text: its result, or "! Type message".
std::string outcome(const std::function<std::string()>& call) {
  try {
    return call();
  } catch (const TraceAttributeReferenceError& error) {
    return std::string("! ") + error.type + ' ' + error.what();
  }
}

// A distance field: equal errors, or numbers within 1e-12 (the actual text
// then replaced by the expected one).
void report_number(const Line& line, const std::string& expected, const std::function<double()>& call) {
  std::string actual;
  try {
    const double value = call();
    actual = expected.rfind("! ", 0) != 0 && close(number(expected), value) ? expected : std::to_string(value);
  } catch (const TraceAttributeReferenceError& error) {
    actual = std::string("! ") + error.type + ' ' + error.what();
  }
  report(line, expected, actual);
}

// After the inputs, the expected output: the remaining fields joined, or the
// error field.
std::string expected_from(const Line& line, std::size_t first) {
  std::string out;
  for (std::size_t i = first; i < line.fields.size(); ++i) out += (i > first ? " | " : "") + line.fields[i];
  return out;
}

void filter_subsets_case(const Line& line) {
  const auto& f = line.fields;
  if (line.tag == "ata") {
    report(line, f.at(4), outcome([&] {
             return names_text(reference_ta_apply_trace_attributes(attributed(f[0]), tokens(f[1]), f[2] == "missing" ? "missing" : f[2],
                                                                   f[3] == "1"));
           }));
  } else if (line.tag == "s2v" || line.tag == "s2d" || line.tag == "s2n") {
    const auto log = traces(f[0]);
    const auto args = tokens(f[1]);
    report(line, f.at(2), outcome([&] {
             if (line.tag == "s2n") return records_text(reference_ta_sublog2df_num(log, std::stoll(args[0])));
             if (line.tag == "s2d") return records_text(reference_ta_sublog2df(log, std::stoll(args[0]), std::stoll(args[1])));
             return traces_text(reference_ta_sublog2varlist(log, std::stoll(args[0]), std::stoll(args[1])));
           }));
  } else if (line.tag == "sp0" || line.tag == "sp1" || line.tag == "sp2") {
    // sublog_percent and its two aliases.
    report(line, expected_from(line, 2), outcome([&] {
             const auto [records, list] = reference_ta_sublog_percent(traces(f[0]), number(f[1]));
             return records_text(records) + " | " + traces_text(list);
           }));
  } else if (line.tag == "lpa" || line.tag == "lp") {
    report(line, expected_from(line, 2), outcome([&] {
             std::string slices;
             std::vector<std::int64_t> counts;
             if (line.tag == "lpa") {
               const auto out = reference_ta_logslice_percent_act(traces(f[0]), number(f[1]));
               for (const auto& s : out.first) slices += (slices.empty() ? "" : ";") + traces_text(s);
               counts = out.second;
             } else {
               const auto out = reference_ta_logslice_percent(traces(f[0]), number(f[1]));
               for (const auto& s : out.first) slices += (slices.empty() ? "" : ";") + traces_text(s);
               counts = out.second;
             }
             return (slices.empty() ? "." : slices) + " | " + longs_text(counts);
           }));
  } else if (line.tag == "avf") {
    report(line, f.at(3),
           outcome([&] { return traces_text(reference_ta_apply_variants_filter(traces(f[0]), traces(f[1]), f[2] == "1")); }));
  } else {  // ad
    report(line, f.at(1), outcome([] {
             reference_ta_act_dist();
             return std::string();
           }));
  }
}

struct PairInputs {
  TraceAttributeLog log1, log2;
  std::vector<TraceAttributeVariant> v1, v2;
  std::int64_t freq_thres = 0, num = 0;
  double alpha = 0;
};

PairInputs pair_inputs(const Line& line) {
  const auto& f = line.fields;
  PairInputs in{traces(f.at(0)), traces(f.at(1)), traces(f.at(3)), traces(f.at(4)), 0, 0, 0};
  const auto args = tokens(f.at(2));
  in.freq_thres = std::stoll(args.at(0));
  in.num = std::stoll(args.at(1));
  in.alpha = number(args.at(2));
  return in;
}

void act_dist_calc_case(const Line& line) {
  const auto& f = line.fields;
  if (line.tag == "ova") {
    report(line, f.at(1), pairs_text(reference_ta_occu_var_act(variant(f[0]))));
  } else if (line.tag == "asim" || line.tag == "adual") {
    const auto in = pair_inputs(line);
    report_number(line, f.at(5), [&] {
      return line.tag == "asim" ? reference_ta_act_sim(in.v1, in.v2, in.log1, in.log2, in.freq_thres, in.num)
                                : reference_ta_act_sim_dual(in.v1, in.v2, in.log1, in.log2, in.freq_thres, in.num);
    });
  } else if (line.tag == "amed") {
    const auto in = pair_inputs(line);
    // Every value the reference gave (one per hash seed when they differ).
    std::vector<std::string> seen = {f.at(5)};
    if (f[5].rfind("! ", 0) != 0) seen = tokens(f[5]);
    std::string actual;
    bool ok = true;
    try {
      const auto outcomes = reference_ta_act_sim_med_outcomes(in.v1, in.v2, in.log1, in.log2, in.freq_thres, in.num);
      for (const auto& s : seen) {
        if (s.rfind("!", 0) == 0) {
          ok = ok && outcomes.may_raise;
        } else {
          ok = ok && std::any_of(outcomes.values.begin(), outcomes.values.end(), [&](double v) { return close(number(s), v); });
        }
      }
      for (const auto v : outcomes.values) actual += std::to_string(v) + ' ';
      if (outcomes.may_raise) actual += "(may raise)";
    } catch (const TraceAttributeReferenceError& error) {
      actual = std::string("! ") + error.type + ' ' + error.what();
      ok = seen.size() == 1 && seen[0] == actual;
    }
    report(line, f[5], ok ? f[5] : actual);
  } else {  // asp, aspa, aspas (act_sim_percent_avg_actset with a 'var' frame)
    const auto args = tokens(f.at(2));
    const auto log1 = traces(f[0]), log2 = traces(f[1]);
    report_number(line, f.at(3), [&] {
      return line.tag == "asp" ? reference_ta_act_sim_percent(log1, log2, number(args[0]), number(args[1]))
                               : reference_ta_act_sim_percent_avg(log1, log2, number(args[0]), number(args[1]));
    });
  }
}

void suc_dist_calc_case(const Line& line) {
  const auto& f = line.fields;
  if (line.tag == "ovs") {
    report(line, f.at(2), pairs_text(reference_ta_occu_var_suc(variant(f[0]), f[1] == "1")));
  } else if (line.tag == "osuc") {
    std::vector<std::pair<TraceAttributeSuccession, std::int64_t>> dfg;
    for (const auto& item : tokens(f.at(0))) {
      const auto at = item.find('=');
      dfg.push_back({{item.substr(0, 1), item.substr(1, at - 1)}, std::stoll(item.substr(at + 1))});
    }
    const auto chosen = reference_ta_occu_suc(dfg, number(f.at(1)));
    std::vector<std::pair<std::string, std::int64_t>> actual;
    for (const auto& [s, v] : chosen) actual.emplace_back(s.first + s.second, v);
    // The reference's unstable sort may pick other successions of a
    // frequency it takes only in part.
    std::vector<std::pair<std::string, std::int64_t>> expected;
    for (const auto& item : tokens(f.at(2))) {
      const auto at = item.find('=');
      expected.emplace_back(item.substr(0, at), std::stoll(item.substr(at + 1)));
    }
    bool ok = expected.size() == actual.size();
    for (std::size_t i = 0; ok && i < expected.size(); ++i) ok = expected[i].second == actual[i].second;
    std::map<std::int64_t, std::set<std::string>> expected_by, actual_by;
    std::map<std::int64_t, std::int64_t> available;
    for (const auto& [k, v] : expected) expected_by[v].insert(k);
    for (const auto& [k, v] : actual) actual_by[v].insert(k);
    for (const auto& [s, v] : dfg) ++available[v];
    for (const auto& [v, keys] : expected_by)
      if (ok && static_cast<std::int64_t>(keys.size()) == available[v]) ok = keys == actual_by[v];
    report(line, f[2], ok ? f[2] : pairs_text(actual));
  } else if (line.tag == "ssim" || line.tag == "sdual") {
    const auto in = pair_inputs(line);
    report_number(line, f.at(5), [&] {
      return line.tag == "ssim" ? reference_ta_suc_sim(in.v1, in.v2, in.log1, in.log2, in.freq_thres, in.num)
                                : reference_ta_act_sim_dual(in.v1, in.v2, in.log1, in.log2, in.freq_thres, in.num);
    });
  } else {  // ssp, sspa
    const auto args = tokens(f.at(2));
    const auto log1 = traces(f[0]), log2 = traces(f[1]);
    report_number(line, f.at(3), [&] {
      return line.tag == "ssp" ? reference_ta_suc_sim_percent(log1, log2, number(args[0]), number(args[1]))
                               : reference_ta_suc_sim_percent_avg(log1, log2, number(args[0]), number(args[1]));
    });
  }
}

void sim_calc_case(const Line& line) {
  const auto& f = line.fields;
  if (line.tag == "dist") {
    const auto in = pair_inputs(line);
    report_number(line, f.at(5),
                  [&] { return reference_ta_dist_calc(in.v1, in.v2, in.log1, in.log2, in.freq_thres, in.num, in.alpha); });
  } else {  // ipc
    std::vector<double> x, y;
    for (const auto& t : tokens(f.at(0))) x.push_back(number(t));
    for (const auto& t : tokens(f.at(1))) y.push_back(number(t));
    const auto expected = tokens(f.at(2));
    const auto actual = reference_ta_inner_prod_calc(x, y);
    std::string text;
    for (std::size_t i = 0; i < 3; ++i)
      text += (i ? " " : "") + (i < expected.size() && close(number(expected[i]), actual[i]) ? expected[i] : std::to_string(actual[i]));
    report(line, f[2], text);
  }
}

void logslice_dist_case(const Line& line) {
  const auto& f = line.fields;
  if (line.tag == "l2s") {
    report(line, f.at(2), names_text(reference_ta_logslice_log2sublog(attributed(f[0], "AMOUNT_REQ"), f[1])));
  } else {  // sda, sds
    const auto log1 = traces(f.at(0)), log2 = traces(f.at(1));
    report_number(line, f.at(3), [&] {
      return line.tag == "sda" ? reference_ta_slice_dist_act(log1, log2, number(f[2]))
                               : reference_ta_slice_dist_suc(log1, log2, number(f[2]));
    });
  }
}

void leven_dist_calc_case(const Line& line) {
  const auto& f = line.fields;
  if (line.tag == "lpp") {
    report(line, f.at(2), outcome([&] {
             const auto [a, b] = reference_ta_leven_preprocess(traces(f[0]), traces(f[1]));
             return (a.empty() ? "-" : a) + " " + (b.empty() ? "-" : b);
           }));
  } else {  // lev, leva
    const auto args = tokens(f.at(2));
    const auto log1 = traces(f[0]), log2 = traces(f[1]);
    report_number(line, f.at(3), [&] {
      return line.tag == "lev" ? reference_ta_leven_dist(log1, log2, number(args[0]), number(args[1]))
                               : reference_ta_leven_dist_avg(log1, log2, number(args[0]), number(args[1]));
    });
  }
}

void dfg_dist_case(const Line& line) {
  const auto& f = line.fields;
  const auto log1 = traces(f.at(0)), log2 = traces(f.at(1));
  if (line.tag == "dfg") {
    const auto expected = tokens(f.at(3));
    const auto [act, suc] = reference_ta_dfg_dist_calc(log1, log2);
    const bool ok = expected.size() == 2 && close(number(expected[0]), act) && close(number(expected[1]), suc);
    report(line, f[3], ok ? f[3] : std::to_string(act) + " " + std::to_string(suc));
  } else {
    report_number(line, f.at(3), [&] {
      if (line.tag == "dfa") return reference_ta_dfg_dist_calc_act(log1, log2);
      if (line.tag == "dfs") return reference_ta_dfg_dist_calc_suc(log1, log2);
      return reference_ta_dfg_dist_calc_minkowski(log1, log2, number(f[2]));
    });
  }
}

void linkage_avg_case(const Line& line) {
  const auto& f = line.fields;
  std::vector<TraceAttributeLog> logs;
  for (const auto& part : split(f.at(0), ';')) logs.push_back(traces(part));
  const auto n = logs.size();
  std::vector<std::vector<double>> matrix(n, std::vector<double>(n, 0.0));
  const auto values = tokens(f.at(1));
  std::size_t k = 0;
  for (std::size_t a = 0; a < n; ++a)
    for (std::size_t b = a + 1; b < n; ++b) matrix[a][b] = matrix[b][a] = number(values.at(k++));
  const auto args = tokens(f.at(2));
  const double alpha = number(args.at(0)), percent = number(args.at(1));
  std::string actual;
  try {
    TraceAttributeLinkage z;
    if (line.tag == "lka") z = reference_ta_linkage_avg(logs, matrix, alpha, percent);
    if (line.tag == "lkd") z = reference_ta_linkage_dfg_update(logs, matrix, alpha, percent);
    if (line.tag == "lkm") z = reference_ta_linkage_dmm_update(logs, matrix, alpha, percent);
    if (line.tag == "lkl") z = reference_ta_linkage_dmm_update_leven(logs, matrix, alpha, percent);
    if (f.at(3).rfind("! ", 0) != 0 && same_linkage(linkage_rows(f[3]), z)) {
      actual = f[3];
    } else {
      for (const auto& row : z)
        actual += (actual.empty() ? "" : ";") + std::to_string(row[0]) + " " + std::to_string(row[1]) + " " +
                  std::to_string(row[2]) + " " + std::to_string(row[3]);
    }
  } catch (const TraceAttributeReferenceError& error) {
    actual = std::string("! ") + error.type + ' ' + error.what();
  }
  report(line, f[3], actual);
}

void merge_log_case(const Line& line) {
  const auto& f = line.fields;
  if (line.tag == "um") {
    std::vector<std::vector<TraceAttributeReferenceTrace>> logs;
    if (f.at(0) != ".")
      for (const auto& part : split(f[0], ';')) logs.push_back(attributed(part));
    std::string actual;
    for (const auto& t : reference_ta_update_merge(logs)) {
      const auto found = t.attributes.find("grp");
      actual += (actual.empty() ? "" : ",") + word(t.events) + "@" + (found == t.attributes.end() ? "~" : found->second) + "@" +
                t.attributes.at("concept:name");
    }
    report(line, f.at(1), actual.empty() ? "." : actual);
  } else if (line.tag == "an" || line.tag == "lt") {
    const auto z = linkage_rows(f.at(1));
    auto tree = reference_ta_add_node(z);
    if (line.tag == "an") {
      report(line, f.at(2), tree_text(tree));
    } else {
      const auto names = tokens(f.at(0));
      const auto leaves = reference_ta_label_tree(tree, names);
      std::string text;
      for (const auto& leaf : leaves) text += (text.empty() ? "" : " ") + leaf;
      report(line, f.at(2) + " | " + f.at(3), text + " | " + tree_text(tree));
    }
  } else if (line.tag == "cl") {
    const auto z = linkage_rows(f.at(2));
    std::vector<std::string> values;
    for (std::size_t i = 0; i <= z.size(); ++i) values.push_back("x" + std::to_string(i));
    const auto [sublogs, groups] = reference_ta_clusteredlog(z, std::stoll(f.at(0)), values, attributed(f.at(1)), "grp");
    std::string logs_text, groups_text;
    for (const auto& s : sublogs) logs_text += (logs_text.empty() ? "" : ";") + names_text(s);
    for (const auto& g : groups) {
      std::string joined;
      for (const auto& v : g) joined += (joined.empty() ? "" : " ") + v;
      groups_text += (groups_text.empty() ? "" : ";") + (joined.empty() ? "." : joined);
    }
    report(line, f.at(3) + " | " + f.at(4), (logs_text.empty() ? "." : logs_text) + " | " + (groups_text.empty() ? "." : groups_text));
  } else if (line.tag == "l2sm") {
    report(line, f.at(3), names_text(reference_ta_merge_log2sublog(attributed(f[0]), f[1], f[2])));
  } else if (line.tag == "ls") {
    report(line, f.at(3), names_text(reference_ta_logslice(attributed(f[0]), tokens(f[1]), f[2])));
  } else {  // ml
    const auto args = tokens(f.at(0));
    report(line, f.at(1), outcome([&] {
             reference_ta_merge_log(args.at(0) == "-" ? "" : args[0], args.at(1) == "-" ? "" : args[1], std::stoll(args.at(2)));
             return std::string();
           }));
  }
}

}  // namespace

int main(int argc, char** argv) try {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " BANK.txt.gz\n";
    return 2;
  }
  std::ifstream input(argv[1], std::ios::binary);
  if (!input) throw std::runtime_error(std::string("cannot read ") + argv[1]);
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  std::istringstream text(detail::gzip_decompress(bytes, std::size_t{1} << 31));
  const std::set<std::string> filter_subsets = {"ata", "s2v", "s2d", "s2n", "sp0", "sp1", "sp2", "lpa", "lp", "avf", "ad"};
  const std::set<std::string> act = {"ova", "asim", "amed", "adual", "asp", "aspa", "aspas"};
  const std::set<std::string> suc = {"ovs", "osuc", "ssim", "sdual", "ssp", "sspa"};
  const std::set<std::string> sim = {"dist", "ipc"};
  const std::set<std::string> slices = {"l2s", "sda", "sds"};
  const std::set<std::string> leven = {"lpp", "lev", "leva"};
  const std::set<std::string> dfg = {"dfa", "dfs", "dfg", "dfm"};
  const std::set<std::string> linkage = {"lka", "lkd", "lkm", "lkl"};
  std::map<std::string, std::size_t> calls;
  std::string raw;
  while (std::getline(text, raw)) {
    if (raw.size() < 3) continue;
    const auto line = parse(raw);
    ++calls[line.tag];
    if (filter_subsets.contains(line.tag)) {
      filter_subsets_case(line);
    } else if (act.contains(line.tag)) {
      act_dist_calc_case(line);
    } else if (suc.contains(line.tag)) {
      suc_dist_calc_case(line);
    } else if (sim.contains(line.tag)) {
      sim_calc_case(line);
    } else if (slices.contains(line.tag)) {
      logslice_dist_case(line);
    } else if (leven.contains(line.tag)) {
      leven_dist_calc_case(line);
    } else if (dfg.contains(line.tag)) {
      dfg_dist_case(line);
    } else if (linkage.contains(line.tag)) {
      linkage_avg_case(line);
    } else {
      merge_log_case(line);
    }
  }
  std::size_t total = 0;
  for (const auto& [tag, count] : calls) total += count;
  std::cout << total << " trace-attribute helper calls compared over " << calls.size() << " functions, " << failures
            << " failures\n";
  return failures || total == 0 ? 1 : 0;
} catch (const std::exception& error) {
  std::cerr << "error: " << error.what() << "\n";
  return 1;
}
