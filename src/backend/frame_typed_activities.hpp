#pragma once

// An activity column of an XES upload's frame as pm4py reads its cells
// (heuristic_miner.cpp; rounds GM, GQ).

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "minera/event_data.hpp"

namespace minera {

// An activity column of an XES upload's frame as pm4py's classic heuristics
// miner reads it (round GM, observed black-box): each cell the Python
// object read_xes's frame holds (classic_frame_cells: text str, a gap NaN, a
// valueless value None in an object column, ints int (float in a float64
// column), floats, booleans, dates Timestamp in a datetime64 column (a gap
// NaT) and datetime.datetime in an object column, <list>s with children
// dicts), or the Polars copy's (an object column str() of each cell, typed
// columns Int64, Float64, Boolean and Datetime as such, a gap or None None).
struct FrameActivityCell {
  std::string type;      // Python type name
  std::string label;     // Python str of the value
  std::string identity;  // equal in Python <=> equal identity
  long double number = 0.0L;
  bool nan = false;
  std::int64_t micros = 0;
  bool missing = false;  // pandas' NaN gap, Polars' None
};

struct FrameTypedActivities {
  std::vector<FrameActivityCell> cells;  // one per row
  std::vector<std::string> names;        // the node of each row: the first label seen of its value
  std::map<std::string, const FrameActivityCell*> value_of;  // node name -> its first value
  bool dict = false;
  std::optional<std::string> missing_name;
};

// nullopt when every cell is text or missing (the frame's own text path) or
// a kind is not modelled.
// `force`: a column of None alone, which the frame's own path leaves out.
std::optional<FrameTypedActivities> frame_typed_activities(const EventTable& table, std::size_t column, bool polars,
                                                           bool force = false);

}  // namespace minera
