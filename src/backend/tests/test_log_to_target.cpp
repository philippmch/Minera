#include "minera/log_to_target.hpp"
#include "minera/csv_event_log_importer.hpp"
#include "minera/json_export.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <sstream>
#include <string>
#include <stdexcept>
#include <vector>

namespace {

minera::Event event(const char* activity, const char* timestamp = "") {
  minera::Event result;
  result.activity = activity;
  result.timestamp = timestamp;
  return result;
}

minera::EventLog sample() {
  minera::Trace first;
  first.events = {event("B", "2024-01-01T00:00:00.000Z"),
                  event("A", "2024-01-01T00:00:01.500Z"),
                  event("C", "2024-01-01T00:00:04+00:00")};
  minera::Trace second;
  second.events = {event("A", "2024-01-01T01:00:00+01:00")};
  return minera::EventLog{{first, second}};
}

void next_activity_contract() {
  minera::LogToTargetOptions options;
  options.variant = minera::LogToTargetVariant::NEXT_ACTIVITY;
  const auto result = minera::log_to_target(sample(), options);
  assert(result.variant == minera::LogToTargetVariant::NEXT_ACTIVITY);
  assert((result.classes == std::vector<std::string>{"A", "B", "C"}));
  assert(result.rows.size() == 2);
  assert(result.trace_count == 2 && result.target_width == 3);
  assert(!result.padded);
  assert(result.rows[0].size() == 3);
  assert(result.rows[0][0][0] == 1.0 && result.rows[0][0][1] == 0.0);
  assert(result.rows[0][1][2] == 1.0);
  assert(std::all_of(result.rows[0][2].begin(), result.rows[0][2].end(),
                     [](double value) { return value == 0.0; }));
}

void next_time_contract() {
  minera::LogToTargetOptions options;
  options.variant = minera::LogToTargetVariant::NEXT_TIME;
  const auto result = minera::log_to_target(sample(), options);
  assert(result.variant == minera::LogToTargetVariant::NEXT_TIME);
  assert((result.classes == std::vector<std::string>{"@@next_time"}));
  assert(result.target_width == 1 && result.rows.size() == 2);
  assert(std::abs(result.rows[0][0][0] - 1.5) < 1e-9);
  assert(std::abs(result.rows[0][1][0] - 2.5) < 1e-9);
  assert(result.rows[0][2][0] == 0.0);
  assert(result.rows[1].size() == 1 && result.rows[1][0][0] == 0.0);
}

void remaining_time_contract() {
  minera::LogToTargetOptions options;
  options.variant = minera::LogToTargetVariant::REMAINING_TIME;
  const auto result = minera::log_to_target(sample(), options);
  assert(result.variant == minera::LogToTargetVariant::REMAINING_TIME);
  assert((result.classes ==
          std::vector<std::string>{"@@remaining_time"}));
  assert(result.target_width == 1 && result.rows.size() == 2);
  assert(std::abs(result.rows[0][0][0] - 4.0) < 1e-9);
  assert(std::abs(result.rows[0][1][0] - 2.5) < 1e-9);
  assert(result.rows[0][2][0] == 0.0);
  assert(result.rows[1].size() == 1 && result.rows[1][0][0] == 0.0);
}

void options_and_errors() {
  minera::LogToTargetOptions options;
  options.activities = {"C", "A"};
  options.enable_padding = true;
  options.pad_size = 4;
  const auto result = minera::log_to_target(sample(), options);
  assert((result.classes == std::vector<std::string>{"C", "A"}));
  assert(result.rows[0].size() == 4 && result.rows[1].size() == 4);
  assert(result.padded && result.trace_count == 2 && result.target_width == 2);

  // Round CU, as the reference: without padding the pad size is ignored; a
  // pad size below the longest trace pads only the shorter traces; padding
  // without a pad size pads to the longest trace; an explicit 0 pads nothing.
  options.enable_padding = false;
  auto plain = minera::log_to_target(sample(), options);
  assert(plain.rows[0].size() == 3 && plain.rows[1].size() == 1 && !plain.padded);
  options.enable_padding = true;
  options.pad_size = 2;
  plain = minera::log_to_target(sample(), options);
  assert(plain.rows[0].size() == 3 && plain.rows[1].size() == 2 && plain.padded);
  options.pad_size = 0;
  plain = minera::log_to_target(sample(), options);
  assert(plain.rows[0].size() == 3 && plain.rows[1].size() == 3);
  options.explicit_pad_size = true;
  plain = minera::log_to_target(sample(), options);
  assert(plain.rows[0].size() == 3 && plain.rows[1].size() == 1);

  // Repeated classes: a next activity takes the first equal column; an
  // explicit empty list gives rows without columns.
  minera::LogToTargetOptions repeated;
  repeated.activities = {"A", "B", "A"};
  const auto repeats = minera::log_to_target(sample(), repeated);
  assert(repeats.target_width == 3 && repeats.rows[0][0][0] == 1.0 && repeats.rows[0][0][2] == 0.0);
  minera::LogToTargetOptions none;
  none.explicit_activities = true;
  const auto empty_classes = minera::log_to_target(sample(), none);
  assert(empty_classes.classes.empty() && empty_classes.target_width == 0);
  assert(empty_classes.rows[0].size() == 3 && empty_classes.rows[0][0].empty());

  minera::Trace custom;
  custom.events = {event("ignored"), event("ignored")};
  custom.events[0].attributes.push_back(
      {"my.activity", "A", minera::AttributeKind::kString});
  custom.events[1].attributes.push_back(
      {"my.activity", "B", minera::AttributeKind::kString});
  minera::LogToTargetOptions custom_options;
  custom_options.activity_key = "my.activity";
  const auto custom_result =
      minera::log_to_target(minera::EventLog{{custom}}, custom_options);
  assert((custom_result.classes == std::vector<std::string>{"A", "B"}));
  assert(custom_result.rows[0][0][1] == 1.0);
  // A key an event lacks raises KeyError, as the reference's event lookup.
  custom_options.activity_key = "missing";
  bool threw = false;
  try {
    (void)minera::log_to_target(minera::EventLog{{custom}}, custom_options);
  } catch (const std::invalid_argument& error) {
    threw = std::string(error.what()) == "KeyError: 'missing'";
  }
  assert(threw);
}

void empty_and_invalid_time() {
  // The reference takes max() of the trace lengths whatever the options, so
  // an empty log raises (round CU).
  for (const bool explicit_classes : {false, true}) {
    minera::LogToTargetOptions options;
    if (explicit_classes) options.activities = {"A"};
    bool threw = false;
    try {
      (void)minera::log_to_target(minera::EventLog{}, options);
    } catch (const std::invalid_argument& error) {
      threw = std::string(error.what()) == "ValueError: max() arg is an empty sequence";
    }
    assert(threw);
  }
  minera::LogToTargetOptions next_only;
  next_only.timestamp_key.clear();
  assert(minera::log_to_target(sample(), next_only).rows.size() == 2);
  minera::LogToTargetOptions temporal_only;
  temporal_only.activity_key.clear();
  temporal_only.variant = minera::LogToTargetVariant::NEXT_TIME;
  const auto temporal_result = minera::log_to_target(sample(), temporal_only);
  assert(temporal_result.rows.size() == 2);
  assert((temporal_result.classes ==
          std::vector<std::string>{"@@next_time"}));
  minera::Trace trace;
  trace.events = {event("A", "bad")};
  bool threw = false;
  try {
    (void)minera::log_to_target(minera::EventLog{{trace}},
                                {.variant = minera::LogToTargetVariant::NEXT_TIME});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  // Out-of-order events give negative targets, as the plain subtraction.
  minera::Trace reversed;
  reversed.events = {event("A", "2024-01-01T00:00:10Z"), event("B", "2024-01-01T00:00:04Z")};
  const auto negative = minera::log_to_target(
      minera::EventLog{{reversed}}, {.variant = minera::LogToTargetVariant::REMAINING_TIME});
  assert(negative.rows[0][0][0] == -6.0 && negative.rows[0][1][0] == 0.0);
}

void json_shape() {
  const auto result = minera::log_to_target(sample());
  const auto json = minera::log_to_target_json(result);
  assert(json.find("\"family\":\"LOG_TO_TARGET\"") != std::string::npos);
  assert(json.find("\"variant\":\"next_activity\"") != std::string::npos);
  assert(json.find("\"trace_count\":2") != std::string::npos);
  assert(json.find("\"target_width\":3") != std::string::npos);
  assert(json.find("\"padded\":false") != std::string::npos);
  assert(json.find("\"classes\":[\"A\",\"B\",\"C\"]") !=
         std::string::npos);
  assert(json.find("\"rows\":[") != std::string::npos);

  // Full precision: 1,234,567.25 seconds is not rounded to six digits.
  minera::Trace far;
  far.events = {event("A", "2024-01-01T00:00:00Z"), event("B", "2024-01-15T06:56:07.250Z")};
  const auto far_json = minera::log_to_target_json(minera::log_to_target(
      minera::EventLog{{far}}, {.variant = minera::LogToTargetVariant::NEXT_TIME}));
  assert(far_json.find("[[1234567.25],[0]]") != std::string::npos);

  auto invalid = result;
  invalid.variant = static_cast<minera::LogToTargetVariant>(99);
  bool threw = false;
  try {
    (void)minera::log_to_target_json(invalid);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

// Round CU: an upload is read through its frame's typed cells, as pm4py's
// log_to_target reads format_dataframe(read_csv(...)) (values checked
// against the pinned reference).
void upload_frames() {
  std::istringstream csv(
      "case,act,ts,num,mix\n"
      "c1,a,2024-01-01T00:00:00+00:00,2,x\n"
      "c1,b,2024-01-01T00:00:05+00:00,,1\n"
      "c2,a,2024-01-01T01:00:00+00:00,1,y\n"
      "c2,c,2024-01-01T01:00:02+00:00,2,z\n"
      "c2,b,2024-01-01T01:00:03+00:00,1,x\n");
  minera::CsvImportOptions import;
  import.case_id_column = "case";
  import.activity_column = "act";
  import.timestamp_column = "ts";
  const auto log = minera::import_csv_event_log(csv, import);
  assert(log.dataframe_columns);
  minera::LogToTargetOptions options;
  options.activity_key = "num";
  // A float column: the missing cell is NaN, a class of its own, sorted last.
  auto result = minera::log_to_target(log, options);
  assert((result.classes == std::vector<std::string>{"1.0", "2.0", "nan"}));
  assert((result.rows[0] == std::vector<std::vector<double>>{{0, 0, 1}, {0, 0, 0}}));
  assert((result.rows[1] == std::vector<std::vector<double>>{{0, 1, 0}, {1, 0, 0}, {0, 0, 0}}));
  // Padding fills the shorter case up to the longest one.
  options.enable_padding = true;
  result = minera::log_to_target(log, options);
  assert(result.rows[0].size() == 3 && result.rows[1].size() == 3);
  // A text column of mixed cells sorts as str.
  options = {};
  options.activity_key = "mix";
  result = minera::log_to_target(log, options);
  assert((result.classes == std::vector<std::string>{"1", "x", "y", "z"}));
  // An explicit list still reads every event's activity: a missing key raises.
  options.activity_key = "missing";
  options.activities = {"x"};
  options.explicit_activities = true;
  bool threw = false;
  try {
    (void)minera::log_to_target(log, options);
  } catch (const std::invalid_argument& error) {
    threw = std::string(error.what()) == "KeyError: 'missing'";
  }
  assert(threw);
  // A float timestamp cell has no timestamp().
  options = {};
  options.variant = minera::LogToTargetVariant::REMAINING_TIME;
  options.timestamp_key = "num";
  threw = false;
  try {
    (void)minera::log_to_target(log, options);
  } catch (const std::invalid_argument& error) {
    threw = std::string(error.what()) == "AttributeError: 'float' object has no attribute 'timestamp'";
  }
  assert(threw);
  // An explicit pad size below the case lengths pads nothing.
  options = {};
  options.variant = minera::LogToTargetVariant::NEXT_TIME;
  options.enable_padding = true;
  options.pad_size = 1;
  options.explicit_pad_size = true;
  result = minera::log_to_target(log, options);
  assert((result.rows[0] == std::vector<std::vector<double>>{{5.0}, {0.0}}));
  assert((result.rows[1] == std::vector<std::vector<double>>{{2.0}, {1.0}, {0.0}}));
}

}  // namespace

int main() {
  next_activity_contract();
  next_time_contract();
  remaining_time_contract();
  options_and_errors();
  empty_and_invalid_time();
  json_shape();
  upload_frames();
}
