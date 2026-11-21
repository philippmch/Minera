#pragma once

// A CSV upload's dates are pandas Timestamps in a datetime64 column, and
// str(Timestamp) keeps nanoseconds ("...00.000000517+00:00"), while an XES
// upload's dates passed through Python datetimes and keep microseconds. The
// route that fits root cause models on a CSV frame holds this scope so the
// categorical date features keep up to nine fraction digits.

namespace minera::detail {

class RootCauseNanosecondDates {
 public:
  explicit RootCauseNanosecondDates(bool enabled);
  ~RootCauseNanosecondDates();
  RootCauseNanosecondDates(const RootCauseNanosecondDates&) = delete;
  RootCauseNanosecondDates& operator=(const RootCauseNanosecondDates&) = delete;

 private:
  bool saved_;
};

}  // namespace minera::detail
