#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "minera/event_data.hpp"

namespace minera {

// A Python argument value of the pandas sequence filters: None, bool, int,
// float, str, list, tuple or dict (entries in insertion order).
struct PandasFilterValue {
  enum class Kind { kNone, kBool, kInt, kFloat, kString, kList, kTuple, kDict };
  Kind kind = Kind::kNone;
  bool boolean = false;
  std::int64_t integer = 0;
  double real = 0.0;
  std::string text;
  std::vector<PandasFilterValue> items;
  std::vector<std::pair<PandasFilterValue, PandasFilterValue>> entries;

  [[nodiscard]] static PandasFilterValue none() { return {}; }
  [[nodiscard]] static PandasFilterValue of(bool value);
  [[nodiscard]] static PandasFilterValue of(std::int64_t value);
  [[nodiscard]] static PandasFilterValue of(int value) { return of(static_cast<std::int64_t>(value)); }
  [[nodiscard]] static PandasFilterValue of(double value);
  [[nodiscard]] static PandasFilterValue of(std::string value);
  [[nodiscard]] static PandasFilterValue of(const char* value) { return of(std::string(value)); }
  [[nodiscard]] static PandasFilterValue list(std::vector<PandasFilterValue> values);
  [[nodiscard]] static PandasFilterValue tuple(std::vector<PandasFilterValue> values);
  [[nodiscard]] static PandasFilterValue dict(std::vector<std::pair<PandasFilterValue, PandasFilterValue>> values);
};

// One parameters-dictionary entry: a Parameters enum member (by member name)
// or a plain key. A member is read before the plain key equal to its value.
struct PandasFilterParameter {
  std::string key;
  bool enum_member = false;
  PandasFilterValue value;
};
// std::nullopt is a None parameters argument.
using PandasFilterParameters = std::optional<std::vector<PandasFilterParameter>>;

// A public reference failure: the Python exception class and its str().
class PandasSequenceFilterError : public std::invalid_argument {
 public:
  PandasSequenceFilterError(std::string type, std::string message)
      : std::invalid_argument(message), type(std::move(type)), message(std::move(message)) {}
  const std::string type;
  const std::string message;
};

// algo.filtering.pandas.pd_filtering_constants and the module constants.
inline constexpr std::int64_t kPandasMinNoOfActivitiesToRetainForDiagram = 10;
inline constexpr std::int64_t kPandasMaxNoOfActivitiesToRetainForDiagram = 25;
inline constexpr std::string_view kPandasFilterCaseConceptName = "case:concept:name";
inline constexpr std::string_view kPandasFilterDefaultNameKey = "concept:name";
inline constexpr std::string_view kPandasFilterCaseIdKeyParameter = "pm4py:param:case_id_key";
inline constexpr std::string_view kPandasFilterActivityKeyParameter = "pm4py:param:activity_key";
inline constexpr std::string_view kPandasFilterGroupedDataframe = "grouped_dataframe";
inline constexpr std::string_view kPandasFilterReturnEaCountDictAutofilter = "return_ea_count_dict_autofilter";
inline constexpr std::string_view kPandasReworkIntCaseActSize = "@@int_case_act_size";

// The tables follow the native event-table carrier: result rows keep their
// source index labels (a RangeIndex stays one only while its labels stay an
// arithmetic progression) unless a function resets the index.

// start_activities_filter / end_activities_filter. A case's start (end)
// activity is its first (last) non-null activity in row order, null when it
// has none; a case is kept when that activity is in `values` (a null one
// matches a null value). Rows with a null case id are kept only by negative
// filters.
[[nodiscard]] EventTable pandas_start_activities_apply(const EventTable& df, const PandasFilterValue& values,
                                                       const PandasFilterParameters& parameters = std::nullopt);
[[nodiscard]] EventTable pandas_filter_df_on_start_activities(
    const EventTable& df, const PandasFilterValue& values, const std::string& case_id_glue = "case:concept:name",
    const std::string& activity_key = "concept:name", const PandasFilterValue& positive = PandasFilterValue::of(true));
// Keeps the cases whose start activity is counted at least `nocc` times
// (counts by first appearance of the case, or sa_count0); a call that keeps
// every counted activity returns the table unchanged.
[[nodiscard]] EventTable pandas_filter_df_on_start_activities_nocc(
    const EventTable& df, const PandasFilterValue& nocc,
    const std::optional<PandasFilterValue>& sa_count0 = std::nullopt,
    const std::string& case_id_glue = "case:concept:name", const std::string& activity_key = "concept:name");

[[nodiscard]] EventTable pandas_end_activities_apply(const EventTable& df, const PandasFilterValue& values,
                                                     const PandasFilterParameters& parameters = std::nullopt);
[[nodiscard]] EventTable pandas_filter_df_on_end_activities(
    const EventTable& df, const PandasFilterValue& values, const std::string& case_id_glue = "case:concept:name",
    const std::string& activity_key = "concept:name", const PandasFilterValue& positive = PandasFilterValue::of(true));
// The last activity of most_common_variant is kept whatever its count. With
// return_dict the result also carries the kept counts (a dict whose null key
// is NaN); an empty table is returned alone.
struct PandasEndActivitiesNocc {
  EventTable table;
  std::optional<PandasFilterValue> counts;
};
[[nodiscard]] PandasEndActivitiesNocc pandas_filter_df_on_end_activities_nocc(
    const EventTable& df, const PandasFilterValue& nocc,
    const std::optional<PandasFilterValue>& ea_count0 = std::nullopt,
    const std::string& case_id_glue = "case:concept:name", const std::string& activity_key = "concept:name",
    bool return_dict = false, const std::optional<PandasFilterValue>& most_common_variant = std::nullopt);

// trace_filter / starts_with_filter / ends_with_filter over each case's
// variant string (its activities joined by ","; a null activity raises the
// str.join TypeError). Each admitted item is joined by "," too. The trace
// filter matches the variant exactly, where a "..." activity stands for any
// run of activities; an empty admitted list keeps every case. The prefix and
// suffix filters compare the strings with startswith/endswith.
[[nodiscard]] EventTable pandas_trace_filter_apply(const EventTable& df, const PandasFilterValue& admitted_traces,
                                                   const PandasFilterParameters& parameters = std::nullopt);
[[nodiscard]] EventTable pandas_starts_with_filter_apply(const EventTable& df,
                                                         const PandasFilterValue& admitted_prefixes,
                                                         const PandasFilterParameters& parameters = std::nullopt);
[[nodiscard]] EventTable pandas_ends_with_filter_apply(const EventTable& df, const PandasFilterValue& admitted_suffixes,
                                                       const PandasFilterParameters& parameters = std::nullopt);

// rework_filter: keeps the cases where `activity` occurs more than
// MIN_OCCURRENCES - 1 earlier times, i.e. some occurrence index (from 0)
// reaches MIN_OCCURRENCES - 1.
[[nodiscard]] EventTable pandas_rework_filter_apply(const EventTable& df, const PandasFilterValue& activity,
                                                    const PandasFilterParameters& parameters = std::nullopt);

// prefix_filter / suffix_filter: adds @@index_in_trace (position in the case)
// and @@temp_column (position of the first or last occurrence of `activity`)
// and keeps the events strictly (or not) before / after it.
[[nodiscard]] EventTable pandas_prefix_filter_apply(const EventTable& df, const PandasFilterValue& activity,
                                                    const PandasFilterParameters& parameters = std::nullopt);
[[nodiscard]] EventTable pandas_suffix_filter_apply(const EventTable& df, const PandasFilterValue& activity,
                                                    const PandasFilterParameters& parameters = std::nullopt);

// consecutive_act_case_grouping_filter: merges runs of adjacent rows with the
// same case and activity; timestamp_case_grouping_filter merges all rows of a
// case with the same timestamp. Each group keeps the first (or last) non-null
// value per column; the index is reset.
[[nodiscard]] EventTable pandas_consecutive_act_case_grouping_apply(
    const EventTable& df, const PandasFilterParameters& parameters = std::nullopt);
[[nodiscard]] EventTable pandas_timestamp_case_grouping_apply(const EventTable& df,
                                                              const PandasFilterParameters& parameters = std::nullopt);

// activity_split_filter on a table ordered by case: each case id becomes
// case + SUBCASE_CONCAT_STR + the number of occurrences of the activities so
// far (CUT_MODE "this" counts the current row, any other mode does not).
[[nodiscard]] EventTable pandas_activity_split_apply(const EventTable& df, const PandasFilterValue& activity,
                                                     const PandasFilterParameters& parameters = std::nullopt);

}  // namespace minera
