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
  // A NaN or NaT: which object it is (equal ids are one object, so a set
  // keeps one of them; read_xes and a float64 column make one per cell).
  std::size_t object = 0;
};

struct CorrelationKeyObjects {
  std::string key;  // the activity key, as KeyError names it
  bool legacy = false;  // read_xes' legacy objects (dates datetime.datetime)
  // One per input event in the input's order: an EventLog's traces and
  // events in log order, a table's rows.
  std::vector<CorrelationKeyObject> objects;
};

}  // namespace minera
