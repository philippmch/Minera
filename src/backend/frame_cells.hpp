#pragma once

#include "minera/frame_keys.hpp"
#include "python_scalar.hpp"

#include <optional>
#include <string>

namespace minera {

// The cell of `key` for one event of `frame` (dataframe_frame_event_log's
// view) as the Python object pandas holds there: typed numbers and bools,
// text, and dates as UTC Timestamps (CSV timestamp columns and XES dates
// alike). nullopt for a missing cell (NaN, NaT, None) or a key the frame
// does not have for this event.
[[nodiscard]] std::optional<python::Scalar> frame_scalar(const EventLog& frame, const Trace& trace,
                                                         const Event& event, const std::string& key,
                                                         FrameKeyScope scope);

// A FrameValue of class kNumber as its Python number (bool, int or float).
[[nodiscard]] python::Scalar frame_number_scalar(const FrameValue& value);

}  // namespace minera
