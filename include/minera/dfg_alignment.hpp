#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

#include "minera/dfg_variants.hpp"

namespace minera {

enum class DfgAlignmentVariant {
  kClassic,
  CLASSIC = kClassic,
};

struct DfgAlignmentMove {
  std::string log;    // ">>" denotes a move on model
  std::string model;  // ">>" denotes a move on log
};

struct DfgAlignmentOptions {
  DfgAlignmentVariant variant = DfgAlignmentVariant::kClassic;
  std::map<std::string, double> sync_cost;
  std::map<std::string, double> model_move_cost;
  std::map<std::string, double> log_move_cost;
  std::map<std::string, double> internal_log_move_cost;
  std::size_t max_visited_states = 0;  // zero means unlimited
  std::uint64_t max_time_ms = 0;       // zero means unlimited
};

struct DfgAlignmentResult {
  std::vector<DfgAlignmentMove> alignment;
  double cost = 0.0;
  double internal_cost = 0.0;
  double fitness = 1.0;
  double best_worst_cost = 0.0;
  std::size_t visited_states = 0;
  std::size_t closed = 0;
  std::size_t closed_states = 0;
};

struct DfgAlignmentLogResult {
  std::vector<DfgAlignmentResult> alignments;
  // Number of unique variants searched (useful to observe caching).
  std::size_t unique_variants = 0;
};

[[nodiscard]] DfgAlignmentResult align_trace_on_dfg(
    std::initializer_list<std::string> trace, const DfgFrequency& dfg,
    const std::map<std::string, std::uint64_t>& start_activities,
    const std::map<std::string, std::uint64_t>& end_activities,
    const DfgAlignmentOptions& options = {});

[[nodiscard]] DfgAlignmentResult align_trace_on_dfg(
    const std::vector<std::string>& trace, const DfgFrequency& dfg,
    const std::map<std::string, std::uint64_t>& start_activities,
    const std::map<std::string, std::uint64_t>& end_activities,
    const DfgAlignmentOptions& options = {});

[[nodiscard]] DfgAlignmentResult align_trace_on_dfg(
    const Trace& trace, const DfgFrequency& dfg,
    const std::map<std::string, std::uint64_t>& start_activities,
    const std::map<std::string, std::uint64_t>& end_activities,
    const DfgAlignmentOptions& options = {});

[[nodiscard]] DfgAlignmentLogResult align_log_on_dfg(
    const EventLog& log, const DfgFrequency& dfg,
    const std::map<std::string, std::uint64_t>& start_activities,
    const std::map<std::string, std::uint64_t>& end_activities,
    const DfgAlignmentOptions& options = {});

struct DfgProjectedEdgeCounts { std::uint64_t sync = 0, mm = 0; };
struct DfgProjectedActivityCounts {
  std::uint64_t sync = 0, mm = 0, lm = 0;
};
struct DfgAlignmentProjection {
  std::map<DfgActivityPair, DfgProjectedEdgeCounts> dfg;
  std::map<std::string, DfgProjectedActivityCounts> activities;
};

[[nodiscard]] EventLog project_log_on_dfg(
    const EventLog& log, const DfgFrequency& dfg,
    const std::map<std::string, std::uint64_t>& start_activities,
    const std::map<std::string, std::uint64_t>& end_activities,
    const DfgAlignmentOptions& options = {});

[[nodiscard]] DfgAlignmentProjection project_alignments_on_dfg(
    const EventLog& log, const DfgFrequency& dfg,
    const std::map<std::string, std::uint64_t>& start_activities,
    const std::map<std::string, std::uint64_t>& end_activities,
    const DfgAlignmentOptions& options = {});

// PM4Py-shaped aliases.
inline DfgAlignmentResult apply_trace(const std::vector<std::string>& t,
    const DfgFrequency& d, const std::map<std::string,std::uint64_t>& s,
    const std::map<std::string,std::uint64_t>& e, const DfgAlignmentOptions& o={})
{ return align_trace_on_dfg(t,d,s,e,o); }
inline DfgAlignmentLogResult apply_log(const EventLog& l, const DfgFrequency& d,
    const std::map<std::string,std::uint64_t>& s,
    const std::map<std::string,std::uint64_t>& e, const DfgAlignmentOptions& o={})
{ return align_log_on_dfg(l,d,s,e,o); }

}  // namespace minera
