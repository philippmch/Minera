#pragma once

#include <cstddef>
#include <string>

#include "minera/event_log.hpp"
#include "minera/ocel.hpp"

namespace minera {

// Reference-exact textual abstractions of an event log for LLM prompts, as
// produced by the public algo.querying.llm.abstractions modules of the pinned
// reference. Budgets count Unicode code points, as Python's len does.
struct LlmAbstractionOptions {
  std::size_t max_len = 10000;
  // "If I have ..." lead-in of the DFG, variants and stream descriptions.
  bool response_header = true;
  // Explanatory header of the features and log-skeleton descriptions.
  bool include_header = true;
  bool include_performance = true;
  bool relative_frequency = false;
  // mean, median, max, min, sum or stdev. An unknown primary aggregation
  // throws std::invalid_argument; an unknown secondary one is left out.
  std::string primary_performance_aggregation = "mean";
  std::string secondary_performance_aggregation;
  // OCDFG discovery setting; without edge performance the reference
  // description raises once the graph has an edge.
  bool compute_edges_performance = true;
  // A column of an upload's frame (dataframe_columns) that names the
  // activities, as pm4py.llm's activity_key on the DataFrame (round BT); empty
  // or concept:name keeps the events' activity. The DFG, variants and
  // features descriptions read it; the stream and column descriptions list
  // the frame whatever the key.
  std::string activity_key;
  // A column of an upload's frame the event stream is sorted by, as
  // pm4py.llm's timestamp_key on the DataFrame (round BV): the rows sort by
  // its cells whatever their type, and a row without one raises KeyError.
  // Empty sorts by the events' timestamp.
  std::string timestamp_key;
  // The object features' lifecycle path lines (enable_object_lifecycle_paths):
  // off at the description algorithm's default (ocel_fea_descr, the bank),
  // on at pm4py.llm.abstract_ocel_features' default (round BU).
  bool ocel_lifecycle_paths = false;
};

// Float and datetime columns of the event table (event attributes plus case:
// prefixed trace attributes, one row per event) with their missing counts and
// quantiles, and boolean or mixed-type columns with their most frequent values
// as far as max_len allows. Integer columns without gaps and string columns are
// left out. A tie in frequency between a string and a non-string value of one
// column throws std::invalid_argument, as the reference raises TypeError.
[[nodiscard]] std::string llm_log_columns_description(const EventLog& log, std::size_t max_len = 10000);

// Directly-follows arcs over each case ordered by timestamp, by descending
// frequency (cases containing the arc under relative_frequency), with the
// arc durations in seconds.
[[nodiscard]] std::string llm_log_dfg_description(const EventLog& log, const LlmAbstractionOptions& options = {});

// Activity variants in log order by descending frequency and primary
// performance (case duration in seconds, last minus first timestamp).
[[nodiscard]] std::string llm_log_variants_description(const EventLog& log,
                                                       const LlmAbstractionOptions& options = {});

// Default trace-based log features, each with its count of non-zero traces and
// the quantiles of the non-zero values, by descending count, coefficient of
// variation and description.
[[nodiscard]] std::string llm_log_features_description(const EventLog& log,
                                                       const LlmAbstractionOptions& options = {});

// Events as Python dictionaries in timestamp order (ties in reverse log order),
// keeping the latest events that fit max_len.
[[nodiscard]] std::string llm_event_stream_description(const EventLog& log,
                                                       const LlmAbstractionOptions& options = {});

// The default log skeleton of the log. Relation pairs follow the native sorted
// order (the reference prints Python set order); activity occurrences follow
// first appearance in the log.
[[nodiscard]] std::string llm_log_skeleton_description(const EventLog& log,
                                                       const LlmAbstractionOptions& options = {});

// Object-centric directly-follows edges of every object type, by descending
// event-couple count, mean duration, object type and edge, as many as max_len
// allows, grouped by object type in order of their first listed edge.
// Without include_header the reference prints no graph at all.
[[nodiscard]] std::string llm_ocel_ocdfg_description(const Ocel& log, const LlmAbstractionOptions& options = {});

// Default object features of the objects of one type, described like the
// log features (count of positive values, quantiles, descending count,
// coefficient of variation and description).
[[nodiscard]] std::string llm_ocel_features_description(const Ocel& log, const std::string& object_type,
                                                        const LlmAbstractionOptions& options = {});

}  // namespace minera
