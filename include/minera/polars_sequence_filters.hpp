#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "minera/event_data.hpp"
#include "minera/pandas_sequence_filters.hpp"

namespace minera {

// The Polars sequence filters take the Python argument values and parameter
// dictionaries of the pandas sequence filters (a Python set travels as a
// list in its iteration order).
using PolarsFilterValue = PandasFilterValue;
using PolarsFilterParameter = PandasFilterParameter;
using PolarsFilterParameters = PandasFilterParameters;

// A public reference failure: the exception class (a Python builtin or a
// polars.exceptions class) and its str(), including the Polars plan text.
class PolarsSequenceFilterError : public std::invalid_argument {
 public:
  PolarsSequenceFilterError(std::string type, std::string message)
      : std::invalid_argument(message), type(std::move(type)), message(std::move(message)) {}
  const std::string type;
  const std::string message;
};

// algo.filtering.polars.polars_filtering_constants and the module constants.
inline constexpr std::int64_t kPolarsMinNoOfActivitiesToRetainForDiagram = 10;
inline constexpr std::int64_t kPolarsMaxNoOfActivitiesToRetainForDiagram = 25;
inline constexpr std::string_view kPolarsFilterCaseConceptName = "case:concept:name";
inline constexpr std::string_view kPolarsFilterDefaultNameKey = "concept:name";
inline constexpr std::string_view kPolarsFilterDefaultTimestampKey = "time:timestamp";
inline constexpr std::string_view kPolarsFilterCaseIdKeyParameter = "pm4py:param:case_id_key";
inline constexpr std::string_view kPolarsFilterActivityKeyParameter = "pm4py:param:activity_key";
inline constexpr std::string_view kPolarsFilterTimestampKeyParameter = "pm4py:param:timestamp_key";
inline constexpr std::string_view kPolarsFilterGroupedDataframe = "grouped_dataframe";
inline constexpr std::string_view kPolarsFilterReturnEaCountDictAutofilter = "return_ea_count_dict_autofilter";

// Tables are Polars frames in the native carrier (no index). Case ids
// compare by value; a null case id is a group of its own for grouping and
// statistics but never joins, so positive filters drop its rows and negative
// (anti-join) filters keep them. Filters keep the source row order.

// start_activities_filter / end_activities_filter: a case's start (end)
// activity is the activity of its first (last) row, null included; a case is
// kept when that activity is a non-null member of `values`.
[[nodiscard]] EventTable polars_start_activities_apply(const EventTable& df, const PolarsFilterValue& values,
                                                       const PolarsFilterParameters& parameters = std::nullopt);
[[nodiscard]] EventTable polars_end_activities_apply(const EventTable& df, const PolarsFilterValue& values,
                                                     const PolarsFilterParameters& parameters = std::nullopt);
[[nodiscard]] EventTable polars_filter_df_on_end_activities(
    const EventTable& df, const PolarsFilterValue& values, const std::string& case_id_glue = "case:concept:name",
    const std::string& activity_key = "concept:name", const PolarsFilterValue& positive = PolarsFilterValue::of(true));
// End activity -> number of cases (the null case group and a null activity
// included), in an unspecified order.
[[nodiscard]] PolarsFilterValue polars_get_end_activities(const EventTable& df,
                                                          const std::string& case_id_key = "case:concept:name",
                                                          const std::string& activity_key = "concept:name");
// Keeps the cases whose end activity is counted at least `nocc` times (or is
// the last activity of a non-empty most_common_variant); a call that keeps
// every counted activity returns the table unchanged. With return_dict the
// kept counts come too.
struct PolarsEndActivitiesNocc {
  EventTable table;
  std::optional<PolarsFilterValue> counts;
};
[[nodiscard]] PolarsEndActivitiesNocc polars_filter_df_on_end_activities_nocc(
    const EventTable& df, const PolarsFilterValue& nocc,
    const std::optional<PolarsFilterValue>& ea_count0 = std::nullopt,
    const std::string& case_id_glue = "case:concept:name", const std::string& activity_key = "concept:name",
    bool return_dict = false, const std::optional<PolarsFilterValue>& most_common_variant = std::nullopt);

// Variants follow the rows sorted by case and "time:timestamp" (stable);
// the variant string joins the non-null activities with ",". The variants
// frames are ordered by case id, nulls first.
[[nodiscard]] EventTable polars_trace_filter_get_variants_df(const EventTable& df,
                                                             const std::string& case_id_key = "case:concept:name",
                                                             const std::string& activity_key = "concept:name");
[[nodiscard]] EventTable polars_ends_with_filter_get_variants_df(const EventTable& df,
                                                                 const std::string& case_id_key = "case:concept:name",
                                                                 const std::string& activity_key = "concept:name");
// trace_filter: the variant equals a ",".join of an admitted trace, where a
// "..." activity stands for any run of activities; an empty admitted list
// admits every case. The admitted case ids are joined back as a Python list.
[[nodiscard]] EventTable polars_trace_filter_apply(const EventTable& df, const PolarsFilterValue& admitted_traces,
                                                   const PolarsFilterParameters& parameters = std::nullopt);
// starts_with_filter / ends_with_filter: str.starts_with / str.ends_with of
// the variant string and a ",".join of each admitted item.
[[nodiscard]] EventTable polars_starts_with_filter_apply(const EventTable& df, const PolarsFilterValue& prefixes,
                                                         const PolarsFilterParameters& parameters = std::nullopt);
[[nodiscard]] EventTable polars_ends_with_filter_apply(const EventTable& df, const PolarsFilterValue& admitted_suffixes,
                                                       const PolarsFilterParameters& parameters = std::nullopt);
// prefix_filter / suffix_filter: the variant's activity list begins (ends)
// with one of the admitted activity lists.
[[nodiscard]] EventTable polars_prefix_filter_apply(const EventTable& df, const PolarsFilterValue& prefixes,
                                                    const PolarsFilterParameters& parameters = std::nullopt);
[[nodiscard]] EventTable polars_suffix_filter_apply(const EventTable& df, const PolarsFilterValue& suffixes,
                                                    const PolarsFilterParameters& parameters = std::nullopt);

// rework_filter: keeps the cases where some activity (null included) occurs
// at least MIN_OCCURRENCES (default 2) times; apply_activity_set first keeps
// only the rows whose activity is in `activities`.
[[nodiscard]] EventTable polars_rework_filter_apply(const EventTable& df,
                                                    const PolarsFilterParameters& parameters = std::nullopt);
[[nodiscard]] EventTable polars_rework_filter_apply_activity_set(
    const EventTable& df, const PolarsFilterValue& activities, const PolarsFilterParameters& parameters = std::nullopt);

// consecutive_act_case_grouping_filter: runs of adjacent rows with equal
// non-null case and activity become their first (last) row; rows come in an
// unspecified order. timestamp_case_grouping_filter: the rows of a case with
// one timestamp become their first (last) row, case and timestamp columns
// first, groups by first appearance. Any other FILTER_TYPE keeps every row.
[[nodiscard]] EventTable polars_consecutive_act_case_grouping_apply(
    const EventTable& df, const PolarsFilterParameters& parameters = std::nullopt);
[[nodiscard]] EventTable polars_timestamp_case_grouping_apply(const EventTable& df,
                                                              const PolarsFilterParameters& parameters = std::nullopt);

// activity_split_filter: each case id becomes case + SUBCASE_CONCAT_STR + the
// number of rows of the case so far whose activity is in `activity`
// (CUT_MODE "this" counts the current row, any other mode the rows before
// it); a null activity makes its own (this) or the next row's (other) id null.
[[nodiscard]] EventTable polars_activity_split_apply(const EventTable& df, const PolarsFilterValue& activity,
                                                     const PolarsFilterParameters& parameters = std::nullopt);

// LazyFrame entry points: the same filters over a materialized LazyEventTable.
[[nodiscard]] LazyEventTable polars_start_activities_apply(const LazyEventTable& df, const PolarsFilterValue& values,
                                                           const PolarsFilterParameters& parameters = std::nullopt);
[[nodiscard]] LazyEventTable polars_end_activities_apply(const LazyEventTable& df, const PolarsFilterValue& values,
                                                         const PolarsFilterParameters& parameters = std::nullopt);
[[nodiscard]] LazyEventTable polars_trace_filter_apply(const LazyEventTable& df, const PolarsFilterValue& admitted_traces,
                                                       const PolarsFilterParameters& parameters = std::nullopt);
[[nodiscard]] LazyEventTable polars_starts_with_filter_apply(const LazyEventTable& df, const PolarsFilterValue& prefixes,
                                                             const PolarsFilterParameters& parameters = std::nullopt);
[[nodiscard]] LazyEventTable polars_ends_with_filter_apply(const LazyEventTable& df,
                                                           const PolarsFilterValue& admitted_suffixes,
                                                           const PolarsFilterParameters& parameters = std::nullopt);
[[nodiscard]] LazyEventTable polars_prefix_filter_apply(const LazyEventTable& df, const PolarsFilterValue& prefixes,
                                                        const PolarsFilterParameters& parameters = std::nullopt);
[[nodiscard]] LazyEventTable polars_suffix_filter_apply(const LazyEventTable& df, const PolarsFilterValue& suffixes,
                                                        const PolarsFilterParameters& parameters = std::nullopt);
[[nodiscard]] LazyEventTable polars_rework_filter_apply(const LazyEventTable& df,
                                                        const PolarsFilterParameters& parameters = std::nullopt);
[[nodiscard]] LazyEventTable polars_consecutive_act_case_grouping_apply(
    const LazyEventTable& df, const PolarsFilterParameters& parameters = std::nullopt);
[[nodiscard]] LazyEventTable polars_timestamp_case_grouping_apply(
    const LazyEventTable& df, const PolarsFilterParameters& parameters = std::nullopt);
[[nodiscard]] LazyEventTable polars_activity_split_apply(const LazyEventTable& df, const PolarsFilterValue& activity,
                                                         const PolarsFilterParameters& parameters = std::nullopt);

}  // namespace minera
