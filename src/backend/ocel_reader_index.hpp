// The events frame's "@@index" column of the reference OCEL readers.
#pragma once

#include <algorithm>
#include <string>
#include <string_view>

#include "minera/ocel.hpp"

namespace minera::ocel_frame {

// Round ES: read_ocel_json, read_ocel2_json, read_ocel_xml, read_ocel2_xml
// and read_ocel2_sqlite number the events frame's rows in an "@@index" column
// (overwriting one the file gives) that they drop after sorting, so an event
// attribute of that name is lost while an object's stays (observed
// black-box, probe es x5 p2.py and p3.py). read_ocel_csv raises instead
// (see ocel_csv.cpp). Drops the attribute from the events, the event types'
// declarations, the frame hint and a declared events table.
inline constexpr std::string_view kReaderEventIndexColumn = "@@index";

inline void drop_reader_event_index(Ocel& log) {
  const std::string name(kReaderEventIndexColumn);
  for (auto& event : log.events)
    std::erase_if(event.attributes, [&](const OcelAttribute& attribute) { return attribute.key == name; });
  for (auto& [type, schema] : log.event_type_attributes) schema.erase(name);
  auto& hint = log.event_frame_hint;
  if (const auto found = std::find(hint.columns.begin(), hint.columns.end(), name); found != hint.columns.end()) {
    const auto at = static_cast<std::size_t>(found - hint.columns.begin());
    hint.columns.erase(found);
    for (auto& [column, place] : hint.reserved)
      if (place != kOcelHintEnd && place > at) --place;
  }
  std::erase_if(hint.dropped, [&](const OcelFrameHintColumn& column) { return column.key == name; });
  if (log.table_columns.events) std::erase(*log.table_columns.events, name);
}

}  // namespace minera::ocel_frame
