#include "minera/ocdfg_performance_controls.hpp"

#include <charconv>
#include <cmath>
#include <stdexcept>

#include "minera/literal_controls_json.hpp"

namespace minera {
namespace {

class SlotReader {
 public:
  explicit SlotReader(std::string_view input) : input_(input) {}

  OcdfgBusinessHourSlots read() {
    space();
    if (input_.substr(at_, 4) == "null") {
      at_ += 4;
      finish();
      return nullptr;
    }
    require('[');
    std::vector<BusinessHourSlot> slots;
    if (!take(']')) {
      do {
        require('[');
        const auto start = number();
        require(',');
        const auto end = number();
        require(']');
        slots.push_back({start, end});
        if (take(']')) break;
        require(',');
      } while (true);
    }
    finish();
    return slots;
  }

 private:
  std::string_view input_;
  std::size_t at_ = 0;
  [[noreturn]] void fail() const {
    throw std::invalid_argument(
        "ocdfg_business_hour_slots_json expects null or an array of finite numeric [start,end] pairs");
  }
  void space() {
    while (at_ < input_.size() &&
           (input_[at_] == ' ' || input_[at_] == '\t' ||
            input_[at_] == '\r' || input_[at_] == '\n')) ++at_;
  }
  bool take(char expected) {
    space();
    if (at_ < input_.size() && input_[at_] == expected) {
      ++at_;
      return true;
    }
    return false;
  }
  void require(char expected) { if (!take(expected)) fail(); }
  void finish() { space(); if (at_ != input_.size()) fail(); }
  bool digit() const {
    return at_ < input_.size() && input_[at_] >= '0' && input_[at_] <= '9';
  }
  double number() {
    space();
    const auto start = at_;
    if (at_ < input_.size() && input_[at_] == '-') ++at_;
    if (!digit()) fail();
    if (input_[at_] == '0') ++at_;
    else while (digit()) ++at_;
    if (at_ < input_.size() && input_[at_] == '.') {
      ++at_;
      if (!digit()) fail();
      while (digit()) ++at_;
    }
    if (at_ < input_.size() && (input_[at_] == 'e' || input_[at_] == 'E')) {
      ++at_;
      if (at_ < input_.size() && (input_[at_] == '+' || input_[at_] == '-')) ++at_;
      if (!digit()) fail();
      while (digit()) ++at_;
    }
    double result = 0;
    const auto parsed = std::from_chars(input_.data() + start, input_.data() + at_, result);
    if (parsed.ec != std::errc{} || parsed.ptr != input_.data() + at_ || !std::isfinite(result)) fail();
    return result;
  }
};

void validate_calendar_json(std::string_view input) {
  // Reuse the established strict JSON/UTF-8 validator. Reject a top-level
  // comma or closing container so wrapping cannot accept extra object fields.
  int depth = 0;
  bool quoted = false, escaped = false;
  for (const char c : input) {
    if (quoted) {
      if (escaped) escaped = false;
      else if (c == '\\') escaped = true;
      else if (c == '"') quoted = false;
    } else if (c == '"') quoted = true;
    else if (c == '[' || c == '{') ++depth;
    else if (c == ']' || c == '}') {
      if (--depth < 0) throw std::invalid_argument("ocdfg_workcalendar_json requires one JSON value");
    } else if (c == ',' && depth == 0)
      throw std::invalid_argument("ocdfg_workcalendar_json requires one JSON value");
  }
  if (literal_controls_transport_error("{\"calendar\":" + std::string(input) + "}"))
    throw std::invalid_argument("ocdfg_workcalendar_json requires one JSON value");
}

}  // namespace

OcdfgBusinessHourSlots parse_ocdfg_business_hour_slots_json(std::string_view input) {
  return SlotReader(input).read();
}

std::optional<OcdfgEdgePerformanceOptions> parse_ocdfg_performance_controls(
    const FieldLookup& lookup, const FieldPresence& present) {
  bool supplied = false;
  for (const auto key : {"ocdfg_business_hours", "ocdfg_business_hour_slots_json",
                         "ocdfg_event_id_attribute", "ocdfg_timestamp_attribute",
                         "ocdfg_workcalendar_json"})
    supplied = supplied || present(key);
  if (!supplied) return std::nullopt;
  OcdfgEdgePerformanceOptions options;
  if (present("ocdfg_business_hours")) {
    const auto value = lookup("ocdfg_business_hours");
    if (value == "true" || value == "1") options.business_hours = true;
    else if (value == "false" || value == "0") options.business_hours = false;
    else throw std::invalid_argument("ocdfg_business_hours expects true, false, 1 or 0");
  }
  if (present("ocdfg_business_hour_slots_json"))
    options.business_hour_slots = parse_ocdfg_business_hour_slots_json(
        lookup("ocdfg_business_hour_slots_json"));
  if (present("ocdfg_event_id_attribute"))
    options.event_id_attribute = lookup("ocdfg_event_id_attribute");
  if (present("ocdfg_timestamp_attribute"))
    options.timestamp_attribute = lookup("ocdfg_timestamp_attribute");
  if (present("ocdfg_workcalendar_json")) {
    options.workcalendar_json = lookup("ocdfg_workcalendar_json");
    validate_calendar_json(options.workcalendar_json);
  }
  return options;
}

}  // namespace minera
