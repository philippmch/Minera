#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "minera/dfg_variants.hpp"

namespace minera {

enum class DfgAlignmentVariant {
  kClassic,
  CLASSIC = kClassic,
};

struct DfgAlignmentMove {
  std::string log;    // ">>" denotes a move on model
  // ">>" denotes a move on log. CLASSIC can expose an early sink transition;
  // its generated identity is canonicalized to "@@dfg_sink", appending '_'
  // until the name differs from every model and trace activity.
  std::string model;
};

struct DfgAlignmentOptions {
  DfgAlignmentVariant variant = DfgAlignmentVariant::kClassic;
  std::map<std::string, double> sync_cost;
  std::map<std::string, double> model_move_cost;
  std::map<std::string, double> log_move_cost;
  std::map<std::string, double> internal_log_move_cost;
  std::size_t max_visited_states = 0;  // zero means unlimited
  std::uint64_t max_time_ms = 0;       // zero means unlimited
  // Event fields used when converting traces to DFG alignment words and when
  // materializing projected logs.  The conventional aliases resolve to the
  // first-class Event projections; other names resolve to generic attributes.
  std::string activity_attribute = "concept:name";
  std::string timestamp_attribute = "time:timestamp";
  // Nonempty maps are always supplied. These flags distinguish an explicitly
  // empty cost function from an omitted function that receives default costs.
  bool sync_cost_supplied = false;
  bool model_move_cost_supplied = false;
  bool log_move_cost_supplied = false;
  bool internal_log_move_cost_supplied = false;
  // Source and sink routing names. Each public search draws two fresh UUIDs,
  // whose lexical order against the activity names breaks queue ties, so the
  // returned alignment and state counters can vary between calls. Supplying
  // the names fixes that choice (every search then uses this pair); empty
  // keeps the random draw. Names must differ from each other, from every
  // model activity and from the trace's activities.
  std::optional<std::pair<std::string, std::string>> routing_names;
};

struct DfgAlignmentResult {
  std::vector<DfgAlignmentMove> alignment;
  double cost = 0.0;
  double internal_cost = 0.0;
  // Only log alignment computes fitness and best/worst cost. Raw trace
  // alignment leaves both as NaN, matching the reference's absent fields.
  double fitness = 1.0;
  double best_worst_cost = 0.0;
  std::size_t visited_states = 0;
  std::size_t closed = 0;
  std::size_t closed_states = 0;
  // False when the supplied graph has no accepting path. Scores are undefined.
  bool computed = true;
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

// Project a previously computed log alignment.  Supplying the result keeps
// projection tied to the exact routes selected by the alignment search.  The
// options argument remains explicit so legacy fifth-argument `{}` calls stay
// unambiguous.
[[nodiscard]] EventLog project_log_on_dfg(
    const EventLog& log, const DfgFrequency& dfg,
    const std::map<std::string, std::uint64_t>& start_activities,
    const std::map<std::string, std::uint64_t>& end_activities,
    const DfgAlignmentLogResult& result,
    const DfgAlignmentOptions& options);

[[nodiscard]] DfgAlignmentProjection project_alignments_on_dfg(
    const EventLog& log, const DfgFrequency& dfg,
    const std::map<std::string, std::uint64_t>& start_activities,
    const std::map<std::string, std::uint64_t>& end_activities,
    const DfgAlignmentOptions& options = {});

// Aggregate metrics from a previously computed log alignment.  The supplied
// result must contain one computed alignment for every source trace.  The
// options argument is explicit for the same overload compatibility reason.
[[nodiscard]] DfgAlignmentProjection project_alignments_on_dfg(
    const EventLog& log, const DfgFrequency& dfg,
    const std::map<std::string, std::uint64_t>& start_activities,
    const std::map<std::string, std::uint64_t>& end_activities,
    const DfgAlignmentLogResult& result,
    const DfgAlignmentOptions& options);

// Convenience aliases.
inline DfgAlignmentResult apply_trace(const std::vector<std::string>& t,
    const DfgFrequency& d, const std::map<std::string,std::uint64_t>& s,
    const std::map<std::string,std::uint64_t>& e, const DfgAlignmentOptions& o={})
{ return align_trace_on_dfg(t,d,s,e,o); }
inline DfgAlignmentLogResult apply_log(const EventLog& l, const DfgFrequency& d,
    const std::map<std::string,std::uint64_t>& s,
    const std::map<std::string,std::uint64_t>& e, const DfgAlignmentOptions& o={})
{ return align_log_on_dfg(l,d,s,e,o); }

}  // namespace minera
