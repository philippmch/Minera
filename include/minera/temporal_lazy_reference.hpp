#pragma once

// algo.transformation.log_to_features.variants.temporal_lazy of the PM4Py
// 2.7.22 reference, observed black-box on Polars LazyFrames (polars 1.44.1).
//
// _freq_to_polars_duration(freq) rejects None and "" (ValueError "grouper
// frequency must be a non-empty string"), strips Python whitespace, keeps
// the part before the first '-', and splits it into its digits (str.isdigit,
// kept as written) and its letters (str.isalpha). No letters raise
// ValueError "cannot infer unit from grouper frequency '<stripped>'". The
// upper-cased letters name the unit: S, SEC, SECOND -> s; T, MIN -> m; M ->
// mo; D, H, W, Q -> d, h, w, q; Y, A -> y; failing that once more after
// dropping trailing S's, then trailing E's (so MS, ME, QS, YE, AS, MINS, SECS
// match), else ValueError "grouper frequency '<stripped>' is not supported
// when using the temporal_lazy variant.". The result is the digits (or "1")
// followed by the Polars unit.
//
// apply groups the events by their start timestamp (the timestamp unless
// start_timestamp_key is given) truncated the Polars way: weeks from Monday
// 1970-01-05, days, hours, minutes and seconds from the epoch, months,
// quarters and years from January 1970, all in local wall time; a multiple
// of 0 keeps the timestamp itself. A duration whose digits are not ASCII
// raises InvalidOperationError; a missing column ColumnNotFoundError. Only
// buckets with events appear, in label order.
//
// Per case: start = min start timestamp, finish = max timestamp, service =
// sum of the events' (timestamp - start) seconds, sojourn = finish - start,
// waiting = sojourn - service; arrival (finish) rate = seconds since the
// previous case in (start, case id) ((finish, case id)) order, 0 for the
// first. Seconds are microseconds * 1e-6, except sojourn when the frame has
// one case and service when it has one row (microseconds / 1e6). Per bucket:
// distinct non-null resources and activities, events, the five case metrics
// averaged over the bucket's cases (a Kahan sum over the cases, divided by
// their number), reworked activities (events minus distinct non-null
// activities, summed over cases), mean distinct cases per non-null resource
// (0 without one), events per case, cases, and mean distinct non-null
// resources per case. Polars sums the case means in a thread-dependent
// order; this port sums them in first-appearance order and reports the
// summands for each mean in case_values.

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace minera {

class TemporalLazyError : public std::runtime_error {
 public:
  TemporalLazyError(std::string type, const std::string& message)
      : std::runtime_error(type + ": " + message), type_(std::move(type)) {}
  [[nodiscard]] const std::string& type() const noexcept { return type_; }

 private:
  std::string type_;
};

// A null, an Int64, a Utf8 value or (in a time column) epoch microseconds.
using TemporalLazyCell = std::variant<std::monostate, std::int64_t, std::string>;

struct TemporalLazyColumn {
  enum Kind { kText, kInt, kTime };
  std::string name;
  Kind kind = kText;
  std::vector<TemporalLazyCell> cells;
};

struct TemporalLazyFrame {
  std::vector<TemporalLazyColumn> columns;
  bool utc = false;  // the time columns' zone: UTC or naive
};

struct TemporalLazyParameters {
  std::string case_id_key = "case:concept:name";
  std::string activity_key = "concept:name";
  std::string timestamp_key = "time:timestamp";
  std::optional<std::string> start_timestamp_key;
  std::string resource_key = "org:resource";
  std::optional<std::string> grouper_freq = "W";  // nullopt stands for None
};

struct TemporalLazyRow {
  std::int64_t timestamp = 0;  // bucket label, epoch microseconds
  std::uint32_t unique_resources = 0, unique_activities = 0, num_events = 0;
  double average_arrival_rate = 0, average_finish_rate = 0, average_waiting_time = 0;
  double average_sojourn_time = 0, average_service_time = 0;
  std::uint32_t total_number_of_reworked_activities = 0;
  double avg_cases_per_resource = 0, avg_events_per_case = 0;
  std::uint32_t number_of_cases = 0;
  double avg_resources_per_case = 0;
  // The bucket's case values behind the arrival, finish, waiting, sojourn and
  // service means, in the order summed.
  std::array<std::vector<double>, 5> case_values;
};

[[nodiscard]] std::string reference_freq_to_polars_duration(const std::optional<std::string>& freq);

[[nodiscard]] std::vector<TemporalLazyRow> reference_temporal_lazy(const TemporalLazyFrame& frame,
                                                                   const TemporalLazyParameters& parameters = {});

// The Kahan sum Polars' group mean divides by the count.
[[nodiscard]] double temporal_lazy_kahan_sum(const std::vector<double>& values);

}  // namespace minera
