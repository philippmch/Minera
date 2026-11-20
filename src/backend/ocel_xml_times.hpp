#pragma once

#include <vector>

#include "minera/ocel.hpp"

namespace minera {

// pm4py.read_ocel_xml's event order over the times of a file whose events
// carry their time as a keyed child (round ED, observed black-box): the
// element's tag decides the value (one naming "float" in any case a Python
// float, else one naming "date" an ISO date, else the text). A column of
// dates sorts by the ISO wall clock as kIsoFormat reads it, one of floats by
// value; texts sort as Python sorts them (code points), after the dates or
// floats of a mixed column; an event without a time, and a NaN float, go
// last; each stably. Dates next to (non-NaN) floats raise pandas' "'values'
// is not ordered" error. A date's text becomes the stored UTC text as
// kIsoFormat stores it, a float's its Python repr, a missing time or a NaN
// an empty text; a text stays as written (declared: the reference keeps an
// object or str column, so native routes read such a text as a time where
// they can, and an empty text as a missing time).
struct OcelXmlTime {
  enum class Kind { kDate, kFloat, kText, kMissing };
  Kind kind = Kind::kDate;
  double real = 0;
};
void sort_ocel_xml_events(Ocel& log, const std::vector<OcelXmlTime>& times);

// Round EU: read_ocel2_xml's events frame refuses a time column that mixes
// an offset of a day or more (read by dateutil) with any other zone or with
// naive times; one where every time shares one such offset reads. Throws the
// datetime module's error (defined in ocel.cpp).
void check_ocel2_xml_event_time_offsets(const Ocel& log);

}  // namespace minera
