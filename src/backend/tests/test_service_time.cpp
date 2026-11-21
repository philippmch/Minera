#include "minera/json_export.hpp"
#include "minera/service_time.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

minera::Event event(std::string activity, std::string start,
                    std::string finish) {
  minera::Event value;
  value.activity = std::move(activity);
  value.timestamp = finish;
  value.attributes.push_back({"started_at", std::move(start),
                              minera::AttributeKind::kDate});
  value.attributes.push_back({"finished_at", std::move(finish),
                              minera::AttributeKind::kDate});
  return value;
}

minera::EventLog fixture() {
  minera::EventLog log;
  minera::Trace first;
  first.events = {event("A", "2024-01-01T00:00:00Z",
                         "2024-01-01T00:10:00Z"),
                  event("B", "2024-01-01T00:20:00Z",
                         "2024-01-01T00:50:00Z")};
  minera::Trace second;
  second.events = {event("A", "2024-01-02T00:00:00Z",
                          "2024-01-02T00:20:00Z"),
                   event("B", "2024-01-02T00:30:00Z",
                          "2024-01-02T01:10:00Z")};
  log.traces = {std::move(first), std::move(second)};
  return log;
}

}  // namespace

int main() {
  const auto log = fixture();
  minera::ServiceTimeOptions options;
  options.start_timestamp_key = "started_at";
  options.timestamp_key = "finished_at";
  const auto result = minera::discover_service_time(log, options);
  assert(result.values.size() == 2);
  assert(std::abs(result.values.at("A") - 900.0) < 1e-9);
  assert(std::abs(result.values.at("B") - 2100.0) < 1e-9);
  assert(std::string(minera::service_time_aggregation_name(
             result.aggregation)) == "MEAN");
  const auto json = minera::service_time_json(result);
  assert(json.find("\"family\":\"SERVICE_TIME\"") != std::string::npos);
  assert(json.find("\"A\":900") != std::string::npos);

  options.aggregation = minera::DfgPerformanceAggregation::kMedian;
  const auto median = minera::discover_service_time(log, options);
  assert(std::abs(median.values.at("B") - 2100.0) < 1e-9);
  // Round FN: pm4py takes the mean for any measure other than median, min,
  // max and sum (its else branch), so raw values, stdev and all give the mean.
  for (const auto other : {minera::DfgPerformanceAggregation::kRawValues,
                           minera::DfgPerformanceAggregation::kStandardDeviation,
                           minera::DfgPerformanceAggregation::kAll}) {
    options.aggregation = other;
    const auto fallback = minera::discover_service_time(log, options);
    assert(fallback.values == result.values);
    assert(std::string(minera::service_time_aggregation_name(fallback.aggregation)) == "MEAN");
  }
  {
    // statistics.mean: 0.1, 0.2 and 0.3 seconds average to exactly 0.2 (a
    // left-to-right sum over three gives 0.20000000000000004).
    minera::EventLog tenths;
    minera::Trace trace;
    trace.events = {event("A", "1970-01-01T00:00:00", "1970-01-01T00:00:00.100000"),
                    event("A", "1970-01-01T00:00:00", "1970-01-01T00:00:00.200000"),
                    event("A", "1970-01-01T00:00:00", "1970-01-01T00:00:00.300000")};
    tenths.traces = {std::move(trace)};
    minera::ServiceTimeOptions mean;
    mean.start_timestamp_key = "started_at";
    assert(minera::discover_service_time(tenths, mean).values.at("A") == 0.2);
    // Timestamp.timestamp() rounds a nanosecond time to the microsecond:
    // .1234565 and .0000005 become .123456 and 0.0 (round half even of the
    // quotient), so the duration is 0.123456 less a rounding of the epoch.
    minera::EventLog nanos;
    minera::Trace one;
    one.events = {event("A", "2024-01-01T00:00:00.000000500", "2024-01-01T00:00:00.123456500")};
    nanos.traces = {std::move(one)};
    const double start = 1704067200.0, finish = 1704067200.123456;
    assert(minera::discover_service_time(nanos, mean).values.at("A") == finish - start);
  }
  // Review V2-1: one CSV row starting 06:30+02:00 and ending 08:00+02:00 on a
  // Monday. pm4py's DataFrame service time with business hours counts the
  // UTC clock (04:30 -> 06:00): 0; the wall clock would count 3600.
  minera::EventLog offset;
  minera::Trace row;
  row.events = {event("A", "2024-01-01T06:30:00+02:00", "2024-01-01T08:00:00+02:00")};
  offset.traces = {std::move(row)};
  minera::ServiceTimeOptions business;
  business.start_timestamp_key = "started_at";
  business.use_business_hours = true;
  assert(minera::discover_service_time(offset, business).values.at("A") == 3600.0);
  business.dataframe = true;
  assert(minera::discover_service_time(offset, business).values.at("A") == 0.0);
  std::cout << "Service-time tests passed\n";
}
