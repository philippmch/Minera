#pragma once

// The activity key's Python objects as pm4py's classic correlation variants
// read them (round HI, observed black-box): an XES upload's log shape keyed
// by an attribute holding other values than text (read_xes' legacy objects,
// xes_legacy_cell.hpp), and a CSV upload's formatted frame keyed by a column
// other than concept:name (the frame's cells, NaN or NaT for a gap).

#include <cstddef>
#include <string>
#include <vector>

#include "python_scalar.hpp"

namespace minera {

struct CorrelationKeyObject {
  enum class State { kValue, kAbsent, kDict };
  State state = State::kValue;
  python::Scalar value;  // kValue: the object; a NaN is a float NaN
  bool nat = false;      // pandas' NaT (value unused)
  // A date that is a datetime.datetime (read_xes' legacy objects, a frame's
  // object column) rather than a pandas Timestamp: TypeErrors name it so.
  bool py_datetime = false;
  // A NaN or NaT: which object it is (equal ids are one object, so a set
  // keeps one of them; read_xes and a float64 column make one per cell).
  std::size_t object = 0;
};

struct CorrelationKeyObjects {
  std::string key;  // the activity key, as KeyError names it
  // An EventLog's walk (read_xes' legacy objects, or convert_to_event_log of
  // a frame): classic_split reads every event's key in log order and keeps
  // its gaps; else a DataFrame's rows, whose gaps classic_split drops.
  bool legacy = false;
  // A frame without the key's column: pandas' column selection fails
  // (KeyError "['key'] not in index") before any row is read.
  bool missing_column = false;
  // One per input event in the input's order: an EventLog's traces and
  // events in log order, a table's rows.
  std::vector<CorrelationKeyObject> objects;
};

}  // namespace minera
