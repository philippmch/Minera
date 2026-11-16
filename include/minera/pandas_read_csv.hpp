#pragma once

#include "minera/event_data.hpp"

#include <istream>

namespace minera {

// The frame pandas.read_csv reads from `input` with its defaults (sep
// `delimiter`) (observed black-box on pandas 3.0.5): the header named as
// reference_csv_header_names does, every data row in file order (blank lines
// skipped, a short row padded with NaN), and each column typed as read_csv
// types it (reference_csv_column): int64, uint64 for integers past the
// signed range, float64 (an int column with a NaN cell, an all-NaN column),
// bool, object for a bool column with a NaN cell or Python ints and literal
// text, str for text. Fields are not trimmed and dates stay text. NaN cells
// are null; a file without data rows reads object columns. The index is the
// implicit RangeIndex: a first data row longer than the header (read_csv's
// implicit index columns) is refused, as is a later row with more fields.
[[nodiscard]] EventTable read_csv_reference_table(std::istream& input, char delimiter = ',');

}  // namespace minera
